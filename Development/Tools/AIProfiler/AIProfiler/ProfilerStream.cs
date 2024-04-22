using System;
using System.Collections.Generic;
using System.IO;

namespace AIProfiler
{
	/** Class containing parsed information from profiling file stream */
	class ProfilerStream
	{
		/** Name table of strings of all names utilized by the profiling stream */
		private List<String> NameTable = new List<String>();

		/** Table of AI controllers; Emitted tokens identify their owners by indices into the table */
		public List<AIController> ControllerTable = new List<AIController>();

		/** Constant representing an invalid index */
		public const Int32 InvalidIndex = -1;
	
		/**
		 * Helper method to retrieve a name from the name table based on index
		 * 
		 * @param	Index	Index to lookup in the name table
		 * 
		 * @return	The name associated with the provided index, or the empty string if the index is invalid
		 */
		public String GetName(Int32 Index)
		{
			if (Index >= 0 && Index < NameTable.Count)
			{
				return NameTable[Index];
			}
			return String.Empty;
		}

		/**
		 * Helper method to retrieve a controller from the controller table based on index
		 * 
		 * @param	Index	Index to lookup in the controller table
		 * 
		 * @return	The AIController associated with the provided index, or null if the index is invalid
		 */
		public AIController GetController(Int32 Index)
		{
			if (Index >= 0 && Index < ControllerTable.Count)
			{
				return ControllerTable[Index];
			}
			return null;
		}

		/**
		 * Returns a list composed of all of the event categories contained in all emitted tokens
		 * across all AI controllers
		 * 
		 * @return	List composed of all event categories contained in all emitted tokens across all AI controllers
		 */
		public List<String> GetAllEventCategories()
		{
			List<String> EventCategories = new List<String>();
			foreach (AIController CurController in ControllerTable)
			{
				foreach (EmittedTokenBase CurToken in CurController.EmittedTokens)
				{
					String CurCategory = CurToken.GetEventCategory();
					if (!EventCategories.Contains(CurCategory))
					{
						EventCategories.Add(CurCategory);
					}
				}
			}
			return EventCategories;
		}

		/**
		 * Returns a list composed of all AI controller class names
		 * 
		 * @return	List of all AI controller class names
		 */
		public List<String> GetAllControllerClassNames()
		{
			List<String> ControllerClassNames = new List<String>();
			foreach (AIController CurController in ControllerTable)
			{
				String CurClassName = CurController.GetClassName();
				if (!ControllerClassNames.Contains(CurClassName))
				{
					ControllerClassNames.Add(CurClassName);
				}
			}
			return ControllerClassNames;
		}

		/**
		 * Static method to parse a stream into a profiler stream
		 *
		 * @param	StreamToParse	Base stream to parse into a profiler stream
		 * 
		 * @return	Parsed profiler stream
		 */
		public static ProfilerStream Parse(Stream StreamToParse)
		{
			// Attempt to read the header from the stream
			BinaryReader BinaryStream = null;
			StreamHeader Header = StreamHeader.ReadHeader(StreamToParse, out BinaryStream);

			ProfilerStream ProfileStream = new ProfilerStream();

			// Cache the current position of the stream, as it represents the begginings of the tokens
			// which cannot be properly parsed yet
			Int64 TokenStreamOffset = StreamToParse.Position;

			// Seek to the location of the name table in the stream and populate the ProfileStream's
			// name table with the data
			StreamToParse.Seek(Header.NameTableOffset, SeekOrigin.Begin);
			for (UInt32 NameIndex = 0; NameIndex < Header.NameTableEntries; ++NameIndex)
			{
				UInt32 CurStringLen = BinaryStream.ReadUInt32();
				ProfileStream.NameTable.Add(new String(BinaryStream.ReadChars((Int32)CurStringLen)));
			}

			// Seek to the location of the controller info table in the stream and populate the ProfileStream's
			// AI controller table with the data
			StreamToParse.Seek(Header.ControllerInfoTableOffset, SeekOrigin.Begin);
			for (UInt32 ControllerIndex = 0; ControllerIndex < Header.ControllerInfoEntries; ++ControllerIndex)
			{
				ProfileStream.ControllerTable.Add(new AIController(ProfileStream, BinaryStream));
			}

			// Seek back to the tokens, which can now be properly parsed with the name/controller tables in place
			StreamToParse.Seek(TokenStreamOffset, SeekOrigin.Begin);
			bool bEndOfStream = false;
			
			// Parse tokens until encountering an end-of-stream token
			while (!bEndOfStream)
			{
				TokenBase Token = TokenBase.ReadNextToken(BinaryStream, ProfileStream);
				if (Token.TokenType == ETokenType.EndOfStream)
				{
					bEndOfStream = true;
				}
			}

			return ProfileStream;
		}
	}
}
