using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace GameplayProfiler
{
	/** Enum values need to be in sync with UE3 */
	public enum ETokenTypes
	{        
		Function = 0,               // Function call
		Actor,                      // Actor tick
		Component,                  // Component tick
		EndOfScope,                 // End of current scope
		Frame,                      // Frame marker
		EndOfStream,                // End of stream marker
		CycleStat,					// Cycle stat token
		MaxAndInvalid,				// Invalid token, also used as the max token index
	}

	/**
	 * Base class of network token/ events
	 */
	class TokenBase
	{
		/** Type of token. */
		public ETokenTypes TokenType = ETokenTypes.MaxAndInvalid;

		/** Profiler data stream this token belongs to. */
		public ProfilerStream ProfilerStream = null;

		/** Stats about token types being serialized. */
		public static int[] TokenTypeStats = Enumerable.Repeat(0, (int)ETokenTypes.MaxAndInvalid).ToArray();

		/**
		 * Reads the next token from the stream and returns it.
		 * 
		 * @param	BinaryStream	    Stream used to serialize from
		 * @param	InProfilerStream	Profiler stream this token belongs to
		 * @return	Token serialized
		 */
		public static TokenBase ReadNextToken(BinaryReader BinaryStream, ProfilerStream InProfilerStream, StreamHeader InStreamHeader)
		{
			TokenBase SerializedToken = null;

			ETokenTypes TokenType = (ETokenTypes)BinaryStream.ReadByte();
			// Handle token specific serialization.
			switch (TokenType)
			{
				case ETokenTypes.Function:
					SerializedToken = new TokenFunction(BinaryStream, InStreamHeader);
					break;
				case ETokenTypes.Actor:
					SerializedToken = new TokenActor(BinaryStream, InStreamHeader);
					break;
				case ETokenTypes.Component:
					SerializedToken = new TokenComponent(BinaryStream, InStreamHeader);
					break;
				case ETokenTypes.CycleStat:
					SerializedToken = new TokenCycleStat(BinaryStream, InStreamHeader);
					break;
				case ETokenTypes.EndOfScope:
					SerializedToken = new TokenEndOfScope(BinaryStream, InStreamHeader);
					break;
				case ETokenTypes.Frame:
					SerializedToken = new TokenFrameMarker(BinaryStream, InStreamHeader);
					break;
				case ETokenTypes.EndOfStream:
					SerializedToken = new TokenEndOfStreamMarker();
					break;
				default:
					throw new InvalidDataException();
			}

			TokenTypeStats[(int)TokenType]++;
			SerializedToken.ProfilerStream = InProfilerStream;
			SerializedToken.TokenType = TokenType;
			return SerializedToken;
		}
	}

	/**
	 * End of stream token.
	 */
	class TokenEndOfStreamMarker : TokenBase
	{
	}

	/**
	 * Frame marker token.
	 */
	class TokenFrameMarker : TokenBase
	{
		/** Time this (preceding/ finished) frame took to tick. */
		public float ElapsedTime;

		/** Constructor, serializing members from passed in stream. */
		public TokenFrameMarker(BinaryReader BinaryStream, StreamHeader InStreamHeader)
		{
			ElapsedTime = BinaryStream.ReadSingle() * 1000;
		}
	}

	/**
	 * Token class for any object that takes time
	 */
	class TokenCounterBase : TokenBase
	{
		/** Time elapsed while tracking this object. For actors also includes component time. */
		public float InclusiveTime = 0;
		/** Time elapsed tracking children. Exclusive Time == Inclusive Time - Children Time. */
		public float ChildrenTime = 0;

		/** List of child tokens based on scoping or parent/ child relationship. */
		public List<TokenCounterBase> Children = new List<TokenCounterBase>();

		/** Whether we should skip this token in detailed view. */
		public bool bShouldSkipInDetailedView = false;
	}


	/**
	 * Object token base class.
	 */
	class TokenObject : TokenCounterBase
	{
		public Int32 ObjectNameIndex;
		public Int32 ClassNameIndex;
		public Int32 OutermostNameIndex;
		public Int32 AssetNameIndex;

		public virtual string GetObjectName(bool bWithAsset)
		{
			return "";
		}

		public string GetClassName()
		{
			return ProfilerStream.GetName(ClassNameIndex);
		}

		public Class GetClass()
		{
			return ProfilerStream.IndexToClassMap[ClassNameIndex];
		}
	}

	/**
	 * Function token.
	 */
	class TokenFunction : TokenObject
	{
		public Int32 OuterNameIndex;

		/** Constructor, serializing members from passed in stream. */
		public TokenFunction(BinaryReader BinaryStream, StreamHeader InStreamHeader)
		{
			ObjectNameIndex = BinaryStream.ReadInt32();
			ClassNameIndex = BinaryStream.ReadInt32();
			OuterNameIndex = BinaryStream.ReadInt32();
			OutermostNameIndex = BinaryStream.ReadInt32();
			if (InStreamHeader.VersionNumber > 0)
			{
				AssetNameIndex = BinaryStream.ReadInt32();
			}
			else
			{
				AssetNameIndex = 0;
			}
		}

		public string GetFunctionName()
		{
			return ProfilerStream.GetObjectName(-1, ObjectNameIndex, -1, OuterNameIndex, OutermostNameIndex, -1);
		}

		public override string GetObjectName(bool bWithAsset)
		{
			return ProfilerStream.GetObjectName(-1, ObjectNameIndex, ClassNameIndex, OuterNameIndex, OutermostNameIndex, -1);
		}
	}

	/**
	 * Actor component token
	 */
	class TokenComponent : TokenObject
	{
		public Int32 ObjectInstanceIndex;
		public Int32 OuterNameIndex;
		public Int32 OuterNameInstance;

		/** Constructor, serializing members from passed in stream. */
		public TokenComponent(BinaryReader BinaryStream, StreamHeader InStreamHeader)
		{
			ObjectInstanceIndex = BinaryStream.ReadInt32();
			ObjectNameIndex = BinaryStream.ReadInt32();
			ClassNameIndex = BinaryStream.ReadInt32();
			OuterNameIndex = BinaryStream.ReadInt32();
			OuterNameInstance = BinaryStream.ReadInt32();
			OutermostNameIndex = BinaryStream.ReadInt32();
			if (InStreamHeader.VersionNumber > 0)
			{
				AssetNameIndex = BinaryStream.ReadInt32();
			}
			else
			{
				AssetNameIndex = 0;
			}
		}

		public override string GetObjectName(bool bWithAsset)
		{
			return ProfilerStream.GetObjectName(ObjectInstanceIndex, ObjectNameIndex, -1, -1, -1, bWithAsset ? AssetNameIndex : -1);
		}

		public string GetActorPathName(bool bWithAsset)
		{
			return ProfilerStream.GetObjectName(OuterNameInstance, OuterNameIndex, -1, -1, OutermostNameIndex, bWithAsset ? AssetNameIndex : -1);
		}
	}

	/**
	 * Actor token
	 */
	class TokenActor : TokenObject
	{
		public Int32 ObjectInstanceIndex;

		/** Constructor, serializing members from passed in stream. */
		public TokenActor(BinaryReader BinaryStream, StreamHeader InStreamHeader)
		{
			ObjectInstanceIndex = BinaryStream.ReadInt32();
			ObjectNameIndex = BinaryStream.ReadInt32();
			ClassNameIndex = BinaryStream.ReadInt32();
			OutermostNameIndex = BinaryStream.ReadInt32();
			if (InStreamHeader.VersionNumber > 0)
			{
				AssetNameIndex = BinaryStream.ReadInt32();
			}
			else
			{
				AssetNameIndex = 0;
			}
		}

		/** Special constructor for ungrouped token. */
		public TokenActor(ProfilerStream InProfilerStream)
		{
			TokenType = ETokenTypes.Actor;
			ProfilerStream = InProfilerStream;
			ObjectNameIndex = -1;
		}

		/** @return TRUE if this actor is the ungrouped container actor, FALSE, otherwise. */
		public bool IsUngroupedActor()
		{
			return ObjectNameIndex == -1;
		}

		public override string GetObjectName(bool bWithAsset)
		{
			return ProfilerStream.GetObjectName(ObjectInstanceIndex, ObjectNameIndex, -1, -1, -1, bWithAsset ? AssetNameIndex : -1);
		}

		public string GetActorPathName(bool bWithAsset)
		{
			return ProfilerStream.GetObjectName(ObjectInstanceIndex, ObjectNameIndex, -1, -1, OutermostNameIndex, bWithAsset ? AssetNameIndex : -1);
		}
	}

	/**
	 * Cycle stat token
	 */
	class TokenCycleStat : TokenCounterBase
	{
		public Int32 CycleStatNameIndex;

		/** Constructor, serializing members from passed in stream. */
		public TokenCycleStat(BinaryReader BinaryStream, StreamHeader InStreamHeader)
		{
			CycleStatNameIndex = BinaryStream.ReadInt32();
		}

		public string GetCycleStatName()
		{
			return ProfilerStream.GetName( CycleStatNameIndex );
		}
	}

	/**
	 * End of scope token.
	 */
	class TokenEndOfScope : TokenBase
	{
		/** Whether we should skip this token in detailed view. */
		public bool bShouldSkipInDetailedView = false;
		/** Cycles elapsed while scope was active. */
		public UInt32 DeltaCycles;

		/** Constructor, serializing members from passed in stream. */
		public TokenEndOfScope(BinaryReader BinaryStream, StreamHeader InStreamHeader)
		{
			bShouldSkipInDetailedView = BinaryStream.ReadByte() > 0 ? true : false;
			DeltaCycles = BinaryStream.ReadUInt32();
		}
	}
}
