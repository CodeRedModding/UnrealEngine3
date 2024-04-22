using System;
using System.IO;
using System.Text;

namespace AIProfiler
{
	/** Enumeration of token types */
	public enum ETokenType
	{
		AILog,					// Token representing an AI log event
		ControllerDestroyed,	// Token representing the destruction of an AI controller
		EndOfStream,			// Token representing the end of the profiling stream
		InvalidToken			// Invalid token
	}

	/** Base class for all stream tokens */
	abstract class TokenBase
	{
		/** Token type of the token */
		public ETokenType		TokenType = ETokenType.InvalidToken;

		/** Profiler stream associated with the token */
		public ProfilerStream	AssociatedProfilerStream;

		/**
		 * Constructor
		 * 
		 * @param	InTokenType			Token type to set for the token
		 * @param	InProfilerStream	Profiler stream associated with the token
		 */
		public TokenBase(ETokenType InTokenType, ProfilerStream InProfilerStream)
		{
			TokenType = InTokenType;
			AssociatedProfilerStream = InProfilerStream;
		}

		/**
		 * Static method to read and parse the next token in the stream
		 * 
		 * @param	BinaryStream		Stream to parse tokens from
		 * @param	InProfilerStream	Profiler stream to associate with the tokens
		 * 
		 * @return	Returns the next token in the stream
		 */
		public static TokenBase ReadNextToken(BinaryReader BinaryStream, ProfilerStream InProfilerStream)
		{
			TokenBase SerializedToken = null;

			// Read the token type from the stream and create the corresponding
			// token as appropriate
			ETokenType CurTokenType = (ETokenType)BinaryStream.ReadByte();
			switch (CurTokenType)
			{
				case ETokenType.AILog:
					SerializedToken = new AILogToken(BinaryStream, InProfilerStream);
					break;
				case ETokenType.ControllerDestroyed:
					SerializedToken = new ControllerDestroyedToken(BinaryStream, InProfilerStream);
					break;
				case ETokenType.EndOfStream:
					SerializedToken = new EndOfStreamToken(InProfilerStream);
					break;
				default:
					throw new InvalidDataException();
			}

			return SerializedToken;
		}
	}

	/** Token representing the end of the profiling stream */
	class EndOfStreamToken : TokenBase
	{
		/**
		 * Constructor
		 * 
		 * @param	InProfilerStream	Profiler stream associated with the token
		 */
		public EndOfStreamToken(ProfilerStream InProfilerStream)
			: base(ETokenType.EndOfStream, InProfilerStream)
		{}
	}

	/** Token base class for tokens emitted by AI controllers */
	abstract class EmittedTokenBase : TokenBase
	{
		/** Index in the controller table of the controller that emitted this token */
		public Int32 OwnerControllerIndex;

		/** Index in the name table of the event category of this token */
		public Int32 EventCategoryNameIndex;

		/** Index in the name table of the pawn name of this token's controller, if any */
		public Int32 PawnNameIndex;

		/** Instance number of the pawn name of this token's controller, if any */
		public Int32 PawnNameInstance;

		/** Index in the name table of the pawn class name of this token's controller, if any */
		public Int32 PawnClassNameIndex;

		/** Index in the name table of the current command class name of this token's controller, if any */
		public Int32 CommandClassNameIndex;

		/** Index in the name table of the state of this token's controller */
		public Int32 StateNameIndex;

		/** Time this token was emitted, in WorldInfo.TimeSeconds */
		public Single WorldTimeSeconds;

		/**
		 * Constructor
		 * 
		 * @param	InTokenType			Token type of the token
		 * @param	InStream			BinaryReader to read token data from
		 * @param	InProfilerStream	Profiler stream associated with the token
		 */
		public EmittedTokenBase(ETokenType InTokenType, BinaryReader InStream, ProfilerStream InProfilerStream)
			: base(InTokenType, InProfilerStream)
		{
			// Read in token data from the binary reader
			OwnerControllerIndex = InStream.ReadInt32();
			EventCategoryNameIndex = InStream.ReadInt32();
			PawnNameIndex = InStream.ReadInt32();
			PawnNameInstance = InStream.ReadInt32();
			PawnClassNameIndex = InStream.ReadInt32();
			CommandClassNameIndex = InStream.ReadInt32();
			StateNameIndex = InStream.ReadInt32();
			WorldTimeSeconds = InStream.ReadSingle();

			// Add the token to its owner controller's emitted tokens array
			GetOwnerController().EmittedTokens.Add(this);
		}

		/**
		 * Return the event category of the token
		 * 
		 * @return Event category of the token
		 */
		public String GetEventCategory()
		{
			return AssociatedProfilerStream.GetName(EventCategoryNameIndex);
		}

		/**
		 * Return the pawn class name of the token, if any
		 * 
		 * @return Pawn class name of the token; Empty string if no pawn is associated with the token
		 */
		public String GetPawnClass()
		{
			return AssociatedProfilerStream.GetName(PawnClassNameIndex);
		}

		/**
		 * Return the pawn name of the token, if any
		 * 
		 * @return Pawn name of the token; Empty string if no pawn is associated with the token
		 */
		public String GetPawnName()
		{
			if (PawnNameIndex != ProfilerStream.InvalidIndex)
			{
				return String.Format("{0}_{1}", AssociatedProfilerStream.GetName(PawnNameIndex), PawnNameInstance);
			}
			return String.Empty;
		}

		/**
		 * Return the command class of the token, if any
		 * 
		 * @return Command class of the token; Empty string if no command class is associated with the token
		 */
		public String GetCommandClass()
		{
			return AssociatedProfilerStream.GetName(CommandClassNameIndex);
		}

		/**
		 * Return the state name of the token
		 * 
		 * @return	State name of the token
		 */
		public String GetStateName()
		{
			return AssociatedProfilerStream.GetName(StateNameIndex);
		}

		/**
		 * Returns the owning AI controller of this token
		 * 
		 * @return AI controller which emitted/owns this token
		 */
		public AIController GetOwnerController()
		{
			AIController ReturnController = AssociatedProfilerStream.GetController(OwnerControllerIndex);
			if (ReturnController == null)
			{
				throw new InvalidDataException("Invalid data: Token associated with invalid controller index!");
			}
			return ReturnController;
		}

		/**
		 * Overridden string representation of the token
		 * 
		 * @return	String representation of the token
		 */
		public override String ToString()
		{
			// Use string builder to construct the string, as it combines numerous elements
			StringBuilder StringBuild = new StringBuilder();

			// If a pawn is associated with the token, add it to the output
			String PawnClass = GetPawnClass();
			String PawnName = GetPawnName();
			if (!String.IsNullOrEmpty(PawnClass) && !String.IsNullOrEmpty(PawnName))
			{
				StringBuild.AppendFormat("{0,-25}{1,-25}", PawnClass, PawnName);
			}

			StringBuild.AppendFormat("{0,-10:0.00}", WorldTimeSeconds);

			// If a command class is associated with the token, add it to the output
			String CommandString = GetCommandClass();
			if (!String.IsNullOrEmpty(CommandString))
			{
				StringBuild.AppendFormat("{0,-25}", CommandString);
			}

			StringBuild.AppendFormat("{0,-25}", GetStateName());
			return StringBuild.ToString();
		}
	}

	/** Token representing an AI log event */
	class AILogToken : EmittedTokenBase
	{
		/** Log text of the AI log */
		public String LogText;

		/**
		 * Constructor
		 * 
		 * @param	InStream			BinaryReader to read token data from
		 * @param	InProfilerStream	Profiler stream associated with the token
		 */
		public AILogToken(BinaryReader InStream, ProfilerStream InProfilerStream)
			: base(ETokenType.AILog, InStream, InProfilerStream)
		{
			UInt32 CurStringLen = InStream.ReadUInt32();
			LogText = new String(InStream.ReadChars((Int32)CurStringLen));
		}

		/**
		 * Overridden string representation of the token
		 * 
		 * @return	String representation of the token
		 */
		public override String ToString()
		{
			return String.Format("{0,-10}{1}{2}", "AILog", base.ToString(), LogText);
		}
	}

	/** Token representing the destruction of an AI controller */
	class ControllerDestroyedToken : EmittedTokenBase
	{
		/**
		 * Constructor
		 * 
		 * @param	InStream			BinaryReader to read token data from
		 * @param	InProfilerStream	Profiler stream associated with the token
		 */
		public ControllerDestroyedToken(BinaryReader InStream, ProfilerStream InProfilerStream)
			: base(ETokenType.ControllerDestroyed, InStream, InProfilerStream)
		{
			// Register this token as its owner controller's destruction token
			GetOwnerController().DestructionToken = this;
		}

		/**
		 * Overridden string representation of the token
		 * 
		 * @return	String representation of the token
		 */
		public override String ToString()
		{
			return String.Format("{0,-30}{1}", "ControllerDestroyed", base.ToString());
		}
	}
}