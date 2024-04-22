/*=============================================================================
	UnrealSourceStats.cs: Source code base comparison and statistics tool
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


/// Uncomment the following code line to store specific data about every occurrence of a line across code bases.
/// NOTE: This is really just for future use.
// #define STORE_DETAILED_OCCURRENCE_DATA


using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Text;
using System.Xml.Serialization;


#region Comments: TODO list

// @todo: Better error messages for missing folder names
// @todo: Color-coded output
// @todo: Averages for file sizes and line counts
// @todo: Longest line stats, largest code file, etc.
// @todo: Print all config options?
// @todo: Track number of source code TOKENS?
// @todo: Show average line count per file, average character count per line
// @todo: Show average line comment size, average contiguous comment size
// @todo: Show most commonly occurring strings!  (easy!  sort by most occurrences!)
// @todo: Support case insensitivity for diffs?  Do we ever want this?

#endregion


namespace UnrealSourceStats
{

	#region App configuration data structures

	///
	/// UnrealSourceStatsConfig - Contains program configuration data. (Serialized to/from disk)
	///

	public class UnrealSourceStatsConfig
	{

		///
		/// CodeBaseConfig - Settings for an individual code base
		///

		public class CodeBaseConfig
		{
			/// Short name for this code base
			public string ShortName = "UE";

			/// Base folder name.  By default, we'll just use the current directory
			public string BaseFolderName = "./";

			/// File name patterns to look for.  The default file patterns are usually what we want.
			public string[] FileNamePatterns = new string[] {
				"*.c",			// C++
				"*.cpp",		// C++
				"*.h",			// C++
				"*.inl",		// C++
				"*.asm",    // ...
				"*.cs",			// C#
				"*.uc",			// UnrealScript
				"*.frm",    // Visual Basic stuff
			  "*.bas",    // Visual Basic stuff
			  "*.cls" };  // Visual Basic stuff

			/// List of sub-folder substrings to exclude from our analysis
			public string[] SubFoldersToExclude;
		}


		/// Configuration data for each code base
		public CodeBaseConfig[] CodeBases = new CodeBaseConfig[] { new CodeBaseConfig() };


		/// True if we should trim white space from beginning and end of code lines
		public bool TrimWhiteSpace = true;

		/// True if we should completely ignore empty lines from comparisons
		public bool IgnoreEmptyLines = false;

		/// True if we should completely ignore all comment lines from comparisons
		public bool IgnoreCommentLines = false;

		/// True if we should completely ignore lines with code that only contains brace characters
		public bool IgnoreBraceLines = false;

		/// True if ALL white space in should be excluded!
		public bool IgnoreAllWhiteSpace = false;

	}

	#endregion


	#region Code database structures

	///
	/// BlobOccurrence - A single occurrence of a blob in a code base
	///

	class BlobOccurrence
	{
#if STORE_DETAILED_OCCURRENCE_DATA
		/// Name of file where this blob is located
		// FIXME: Should be file index! (for better performance)
		public string FullFileName;

		/// Line number that this blob can be located in the above file
		public Int32 LineNumber;
#endif
	}



	/// 
	/// BlobData - A unique piece of text in a code base.  Each occurrence of this text gets it's own BlobOccurrence.
	///

	class BlobData
	{
		/// A new occurrence for each instance of this blob text in the code base
		public List< BlobOccurrence > Occurrences;

		/// Number of current matches (when performing comparisons.)  This is reset to zero between comparisons.
		public Int32 MatchCount;
	}



	///
	/// CodeBaseInfo - Cached information about a code base
	///

	class CodeBaseInfo
	{
		/// Initializing constructor
		public CodeBaseInfo( string InitShortName, DirectoryInfo InitBaseFolderInfo )
		{
			ShortName = InitShortName;
			BaseFolderInfo = InitBaseFolderInfo;

			BlobDatabase = new Dictionary< string, BlobData >();
		}


		/// Short name for this code base
		public string m_ShortName;
		public string ShortName
		{
			get
			{
				return m_ShortName;
			}

			private set
			{
				m_ShortName = value;
			}
		}

		/// Base folder name for this code base
		public DirectoryInfo m_BaseFolderInfo;
		public DirectoryInfo BaseFolderInfo
		{
			get
			{
				return m_BaseFolderInfo;
			}

			private set
			{
				m_BaseFolderInfo = value;
			}
		}


		/// Total number of sub-folders
		public Int32 TotalSubFolderCount;

		/// Total number of files
		public Int32 TotalFileCount;

		/// Number of files that match our pattern filter
		public Int32 NumFilesInsidePattern;

		/// Number of files that did not match out pattern
		public Int32 NumFilesOutsidePattern;

		/// Size (in bytes) of all files in our filter
		public Int64 TotalSizeOfFilesInsidePattern;

		/// Total number of text lines
		public Int32 TotalLineCount;

		/// Number lines that will be used for comparisons between code bases
		public Int32 TotalComparableLines;



		/// Number of empty lines (or entirely whitespace)
		public Int32 EmptyLineCount;

		/// Number of comment lines (no code at all, only comments)
		public Int32 CommentLineCount;

		/// Number of comments (including comments that start at the end of lines with actual code)
		public Int32 CommentCount;

		/// Number of unique comments made up of contiguous lines (either multiple single-line comments, or a
		/// multi-line comment block)
		public Int32 ContiguousCommentCount;



		/// A huge hash table of all blobs in the code base!
		public Dictionary< string, BlobData > BlobDatabase;
	}

	#endregion


	///
	/// UnrealSourceStatsApp - This is the meat right here.
	///

	class UnrealSourceStatsApp
	{
		#region App globals

		/// Name/version info string, shown when the application starts
		string m_AppNameAndVersionString = "UnrealSourceStats Version 1.0\n(C) Copyright 1998-2007, Epic Games, inc.  All Rights Reserved.";

		/// Name of config file
		string m_ConfigFileName = "UnrealSourceStatsConfig.xml";

		#endregion


		///
		/// ParseFile - Parses (loads) a single file; updates the code base with the loaded text
		///

		public void ParseFile( string FileFullName, CodeBaseInfo CodeBase, UnrealSourceStatsConfig Config, UnrealSourceStatsConfig.CodeBaseConfig CodeBaseConfig )
		{
			StreamReader MyStreamReader = File.OpenText( FileFullName );
			if( MyStreamReader != null )
			{
				// File name pattern matching
				FileInfo MyFileInfo = new FileInfo( FileFullName );


				// Check to see what type of file this is
				string LowerCaseFileExtension = MyFileInfo.Extension.ToLower();
				bool IsCPlusPlusFile =
					LowerCaseFileExtension.Equals( ".cpp" ) ||
					LowerCaseFileExtension.Equals( ".h" ) ||
					LowerCaseFileExtension.Equals( ".inl" ) ||
					LowerCaseFileExtension.Equals( ".cs" ) ||   // OK, C# is close enough to C++ for comment and string parsing
					LowerCaseFileExtension.Equals( ".c" ) ||    // OK, not really C++ but that's OK for testing comments
					LowerCaseFileExtension.Equals( ".uc" );			// OK, definitely not C++ but close enough for us!


				Int32 TotalLinesInFile = 0;
				bool IsParsingMultiLineCommentBlock = false;
				bool LastLineWasAComment = false;


				// Read text lines
				for( string LineText = MyStreamReader.ReadLine(); LineText != null; LineText = MyStreamReader.ReadLine() )
				{
					// OK, lets start the cleanup process.  We'll begin by removing all whitespace.
					string CleanedText = Config.TrimWhiteSpace ? LineText.Trim() : LineText;


					// Kill all white space if we were asked to do that
					if( Config.IgnoreAllWhiteSpace )
					{
						StringBuilder NewStringBuilder = new StringBuilder();
						for( Int32 CurCharIndex = 0; CurCharIndex < CleanedText.Length; ++CurCharIndex )
						{
							if( !char.IsWhiteSpace( CleanedText[ CurCharIndex ] ) )
							{
								NewStringBuilder.Append( CleanedText[ CurCharIndex ] );
							}
						}
						CleanedText = NewStringBuilder.ToString();
					}

					bool UseLineInComparisons = true;

					// Keep track of empty lines
					if( CleanedText.Length == 0 )
					{
						++CodeBase.EmptyLineCount;

						if( Config.IgnoreEmptyLines )
						{
							UseLineInComparisons = false;
						}
					}


					// We'll count the number of non-comment, non-whitespace characters on this line.  We also keep track
					// of the number of brace characters, so we can optionally ignore lines with *only* braces
					int NumEffectiveCodeChars = 0;
					int NumBraceCodeChars = 0;
					bool LineContainsAComment = IsParsingMultiLineCommentBlock;

					if( IsCPlusPlusFile )
					{
						bool IsParsingSingleQuoteChar = false;
						bool IsParsingDoubleQuoteString = false;

						
						// OK, now actually start parsing some basic C++
						for( Int32 CurCharIndex = 0; CurCharIndex < CleanedText.Length; ++CurCharIndex )
						{
							// Gather characters
							char CurChar = CleanedText[ CurCharIndex ];
							char PrevChar = ( char )0;
							char NextChar = ( char )0;
							string CurAndNextChars = String.Empty;
							if( CurCharIndex > 0 )
							{
								PrevChar = CleanedText[ CurCharIndex - 1 ];
							}
							if( CurCharIndex + 1 < CleanedText.Length )
							{
								NextChar = CleanedText[ CurCharIndex + 1 ];
								CurAndNextChars = CleanedText.Substring( CurCharIndex, 2 );
							}


							if( IsParsingMultiLineCommentBlock )
							{
								if( CurAndNextChars.Equals( "*/" ) )
								{
									// End of multi-line comment
									LineContainsAComment = true;
									IsParsingMultiLineCommentBlock = false;
									++CurCharIndex;
								}
								else
								{
									// Character is part of a multi-line comment; we'll ignore it
								}
							}
							else if( IsParsingDoubleQuoteString )
							{
								++NumEffectiveCodeChars;
								if( CurChar == '"' && PrevChar != '\\' )
								{
									// End of double-quote string
									IsParsingDoubleQuoteString = false;
								}
								else
								{
									// Character is part of a double quote string; we'll ignore it
								}
							}
							else if( IsParsingSingleQuoteChar )
							{
								++NumEffectiveCodeChars;
								if( CurChar == '\'' && PrevChar != '\\' )
								{
									// End of single-quote char
									IsParsingSingleQuoteChar = false;
								}
								else
								{
									// Character is part of a single quote character; we'll ignore it
								}
							}
							else
							{
								if( char.IsWhiteSpace( CurChar ) )
								{
									// Character is white space; we'll ignore it
								}
								else if( CurAndNextChars.Equals( "//" ) )
								{
									// Single line comment found
									LineContainsAComment = true;
									++CurCharIndex;

									++CodeBase.CommentCount;

									// Just ignore the rest of the text line
									break;
								}
								else if( CurAndNextChars.Equals( "/*" ) )
								{
									// Start of multi-line comment found
									LineContainsAComment = true;
									IsParsingMultiLineCommentBlock = true;
									++CurCharIndex;

									++CodeBase.CommentCount;
									++CodeBase.ContiguousCommentCount;
								}
								else
								{
									// Wow, this appears to be actual source code
									++NumEffectiveCodeChars;

									if( CurChar == '{' || CurChar == '}' )
									{
										// Found a brace (either open or close)
										++NumBraceCodeChars;
									}
									else if( CurChar == '"' && PrevChar != '\\' )
									{
										// Start of double-quote string
										IsParsingDoubleQuoteString = true;
									}
									else if( CurChar == '\'' && PrevChar != '\\' )
									{
										// Start of single-quote char
										IsParsingSingleQuoteChar = true;
									}
									else
									{
										// Character is source code!
									}
								}
							}
						}
					}


					// Check to see if the entire line can be considered a comment
					bool CurLineIsAComment = ( LineContainsAComment && NumEffectiveCodeChars == 0 );
					if( CurLineIsAComment )
					{
						++CodeBase.CommentLineCount;

						if( !LastLineWasAComment )
						{
							// This is the start of a new comment
							++CodeBase.ContiguousCommentCount;
						}

						if( Config.IgnoreCommentLines )
						{
							UseLineInComparisons = false;
						}
					}
					else
					{
						// Figure out if the only thing this line had to offer was a Brace.  If so, then we may want to ignore it
						if( NumBraceCodeChars > 0 && NumEffectiveCodeChars == NumBraceCodeChars )
						{
							if( Config.IgnoreBraceLines )
							{
								UseLineInComparisons = false;
							}
						}
					}
					LastLineWasAComment = CurLineIsAComment;


					if( UseLineInComparisons )
					{
						// Look for the text in our hash table
						BlobData MyBlobData = null;
						if( CodeBase.BlobDatabase.ContainsKey( CleanedText ) )
						{
							// We've already seen this string before
							MyBlobData = CodeBase.BlobDatabase[ CleanedText ];
						}
						else
						{
							// First time we've seen this string in the code base
							MyBlobData = new BlobData();
							MyBlobData.MatchCount = 0;
							CodeBase.BlobDatabase.Add( CleanedText, MyBlobData );
						}

						// Add new occurrence of this blob
						BlobOccurrence NewOccurrence = new BlobOccurrence();

#if STORE_DETAILED_OCCURRENCE_DATA
						NewOccurrence.FullFileName = MyFileInfo.FullName;
						NewOccurrence.LineNumber = TotalLinesInFile;
#endif

						// Allocate list on demand
						if( MyBlobData.Occurrences == null )
						{
							MyBlobData.Occurrences = new List<BlobOccurrence>();
						}
						MyBlobData.Occurrences.Add( NewOccurrence );

						++CodeBase.TotalComparableLines;
					}

					// Count this line
					++TotalLinesInFile;
				}

				CodeBase.TotalLineCount += TotalLinesInFile;

				// We're done with this file now!
				MyStreamReader.Close();
			}
		}



		///
		/// LoadFileDataFromFoldersRecursively - Recurses through folders, parsing files and gathering data
		///

		public void LoadFileDataFromFoldersRecursively( string FolderName, CodeBaseInfo CodeBase, UnrealSourceStatsConfig Config, UnrealSourceStatsConfig.CodeBaseConfig CodeBaseConfig )
		{
			DirectoryInfo CurFolder = new DirectoryInfo( FolderName );

			// Process child folders
			DirectoryInfo[] SubFolders = CurFolder.GetDirectories();
			foreach( DirectoryInfo CurSubFolder in SubFolders )
			{
				// Count this sub-folder
				++CodeBase.TotalSubFolderCount;

				// Filter this folder name against our list of evil sub-folders
				bool ShouldExcludeThisFolder = false;
				{
					String SubFolderString = CurSubFolder.FullName.Substring( CodeBase.BaseFolderInfo.FullName.Length );
					String AdjustedSubFolderString = SubFolderString.ToLower().Replace( "\\", "/" );

					if( CodeBaseConfig.SubFoldersToExclude != null )
					{
						foreach( string CurFolderExclusion in CodeBaseConfig.SubFoldersToExclude )
						{
							String AdjustedFolderExclusion = CurFolderExclusion.ToLower().Replace( "\\", "/" );
							if( AdjustedSubFolderString.StartsWith( AdjustedFolderExclusion ) )
							{
								ShouldExcludeThisFolder = true;
								break;
							}
						}
					}
				}

				if( !ShouldExcludeThisFolder )
				{
					// Recurse!
					LoadFileDataFromFoldersRecursively( CurSubFolder.FullName, CodeBase, Config, CodeBaseConfig );
				}
			}


			// Count all files
			FileInfo[] AllFileInfosInFolder = CurFolder.GetFiles( "*.*" );

			// Find files that match our supplied wildcards
			List< String > FilteredFileNames = new List< String >();
			foreach( string CurFileNamePattern in CodeBaseConfig.FileNamePatterns )
			{
				FileInfo[] MatchingFileInfos = CurFolder.GetFiles( CurFileNamePattern );
				foreach( FileInfo CurFileInfo in MatchingFileInfos )
				{
					// Make sure this file isn't already in our list
					// FIXME: Use a dictionary here for constant time?
					if( !FilteredFileNames.Contains( CurFileInfo.Name ) )
					{
						FilteredFileNames.Add( CurFileInfo.Name );
						CodeBase.TotalSizeOfFilesInsidePattern += CurFileInfo.Length;
					}
					else
					{
						// Element was already in the list!
					}
				}
			}


			// Update counts
			CodeBase.TotalFileCount += AllFileInfosInFolder.Length;
			CodeBase.NumFilesInsidePattern += FilteredFileNames.Count;
			CodeBase.NumFilesOutsidePattern += ( AllFileInfosInFolder.Length - FilteredFileNames.Count	 );


			// Process files in this folder, for each wildcard string we were given
			foreach( String CurFileName in FilteredFileNames )
			{
				String FullName = CurFolder.FullName + "\\" + CurFileName;

				ParseFile( FullName, CodeBase, Config, CodeBaseConfig );
			}
		}



		///
		/// CheckForABlobsInB - Determines exactly which blobs of text in CodeBaseA are also in CodeBaseB
		///

		public void CheckForABlobsInB( CodeBaseInfo CodeBaseA, CodeBaseInfo CodeBaseB, ref Int32 TotalMatchingBlobs, ref Int32 NumABlobsNotInB )
		{
			// Count every blob from CodeBaseA that exists in CodeBaseB
			foreach( KeyValuePair<string, BlobData> CodeBaseABlobIter in CodeBaseA.BlobDatabase )
			{
				// Try to find this blob in the other code base
				BlobData CodeBaseBBlobData;
				if( CodeBaseB.BlobDatabase.TryGetValue( CodeBaseABlobIter.Key, out CodeBaseBBlobData ) )
				{
					// OK, the other code base has at least ONE match.

					// NOTE: We skip over occurrences that have already been matched on the source code base
					Int32 CurOccurrence = CodeBaseABlobIter.Value.MatchCount;
					for( ; CurOccurrence < CodeBaseABlobIter.Value.Occurrences.Count; ++CurOccurrence )
					{
						// Are all of the other code base's blob occurrences already matched?
						if( CodeBaseBBlobData.MatchCount >= CodeBaseBBlobData.Occurrences.Count )
						{
							// No more room for this occurrence!
							++NumABlobsNotInB;
						}
						else
						{
							// OK, we found a match
							++CodeBaseBBlobData.MatchCount;
							++TotalMatchingBlobs;

							if( CodeBaseABlobIter.Value.MatchCount >= CodeBaseABlobIter.Value.Occurrences.Count )
							{
								throw new IndexOutOfRangeException();
							}
							++CodeBaseABlobIter.Value.MatchCount;
						}
					}
				}
				else
				{
					// No occurrences of this blob exist in the other code base!
					NumABlobsNotInB += CodeBaseABlobIter.Value.Occurrences.Count;
				}
			}
		}



		///
		/// CompareCodeBases - Compares the specified code bases and spews some statistics
		///

		public void CompareCodeBases( CodeBaseInfo CodeBaseA, CodeBaseInfo CodeBaseB )
		{
			// Reset match counts in both code bases
			foreach( KeyValuePair< string, BlobData > CodeBaseABlobIter in CodeBaseA.BlobDatabase )
			{
				CodeBaseABlobIter.Value.MatchCount = 0;
			}
			foreach( KeyValuePair<string, BlobData> CodeBaseBBlobIter in CodeBaseB.BlobDatabase )
			{
				CodeBaseBBlobIter.Value.MatchCount = 0;
			}


			// OK, for this comparison we'll be counting to the total number of blobs that match.
			Int32 TotalMatchingBlobs = 0;


			// We start off by looking for the first code base's blobs in the second code base.  While we're doing this
			// we'll be counting both code base's matched blobs for every single occurrence.  This data will be stored
			// right inside the code bases' respective blob data structures.
			Int32 NumABlobsNotInB = 0;
			CheckForABlobsInB( CodeBaseA, CodeBaseB, ref TotalMatchingBlobs, ref NumABlobsNotInB );

			// Now we flip things around by finding the remaining blobs in the second code base that don't exist in
			// the first code base.  This relies on 'occurrence match count' data that was setup in the previous call.
			Int32 NumBBlobsNotInA = 0;
			CheckForABlobsInB( CodeBaseB, CodeBaseA, ref TotalMatchingBlobs, ref NumBBlobsNotInA );


			Console.WriteLine();
			Console.WriteLine( "Comparison of {0} to {1}:", CodeBaseA.ShortName, CodeBaseB.ShortName );

			Console.WriteLine();

			Int32 CodeBaseBExtraFiles = CodeBaseB.TotalFileCount - CodeBaseA.TotalFileCount;
			Console.WriteLine( "  File count: {0} has {1:#,0} {2} files than {3}  ({4:#,0.0%} {5})",
				CodeBaseB.ShortName,
				Math.Abs( CodeBaseBExtraFiles ),
				CodeBaseBExtraFiles >= 0 ? "more" : "fewer",
				CodeBaseA.ShortName,
				( float )Math.Abs( CodeBaseBExtraFiles ) / CodeBaseA.TotalFileCount,
				CodeBaseBExtraFiles >= 0 ? "increase" : "decrease" );
			Int64 CodeBaseBExtraBytes = CodeBaseB.TotalSizeOfFilesInsidePattern - CodeBaseA.TotalSizeOfFilesInsidePattern;
			Console.WriteLine( "  File size: {0} is {1:#,0.0} MB {2} than {3}  ({4:#,0.0%} {5})",
				CodeBaseB.ShortName,
				Math.Abs( CodeBaseBExtraBytes / ( 1024 * 1024 ) ),
				CodeBaseBExtraBytes >= 0 ? "larger" : "smaller",
				CodeBaseA.ShortName,
				( float )Math.Abs( CodeBaseBExtraBytes ) / CodeBaseA.TotalSizeOfFilesInsidePattern,
				CodeBaseBExtraBytes >= 0 ? "increase" : "decrease" );
			Int32 CodeBaseBExtraLines = CodeBaseB.TotalLineCount - CodeBaseA.TotalLineCount;
			Console.WriteLine( "  Line count: {0} has {1:#,0} {2} lines than {3}  ({4:#,0.0%} {5})",
				CodeBaseB.ShortName,
				Math.Abs( CodeBaseBExtraLines ),
				CodeBaseBExtraLines >= 0 ? "more" : "fewer",
				CodeBaseA.ShortName,
				( float )Math.Abs( CodeBaseBExtraLines ) / CodeBaseA.TotalLineCount,
				CodeBaseBExtraLines >= 0 ? "increase" : "decrease" );
			float CodeBaseACommentsPerLine = ( float )CodeBaseA.CommentLineCount / CodeBaseA.TotalLineCount;
			float CodeBaseBCommentsPerLine = ( float )CodeBaseB.CommentLineCount / CodeBaseB.TotalLineCount;
			float CodeBaseBExtraCommentsPerLine = CodeBaseBCommentsPerLine - CodeBaseACommentsPerLine;
			Console.WriteLine( "  Comments: {0} has {1:#,0.0%} {2} comments per line than {3}  ({4:#,0.0%} {5})",
				CodeBaseB.ShortName,
				Math.Abs( CodeBaseBExtraCommentsPerLine ),
				CodeBaseBExtraCommentsPerLine >= 0 ? "more" : "fewer",
				CodeBaseA.ShortName,
				( float )Math.Abs( CodeBaseBExtraCommentsPerLine ) / CodeBaseACommentsPerLine,
				CodeBaseBExtraCommentsPerLine >= 0 ? "increase" : "decrease" );
			
			Console.WriteLine();

			Console.WriteLine( "  Unchanged lines: {0:#,0}  ({1:#,0.0%} of {2}, {3:#,0.0%} of {4})",
				TotalMatchingBlobs,
				( float )TotalMatchingBlobs / CodeBaseA.TotalComparableLines,
				CodeBaseA.ShortName,
				( float )TotalMatchingBlobs / CodeBaseB.TotalComparableLines,
				CodeBaseB.ShortName );
			Console.WriteLine( "  Removed or replaced lines since {0}: {1:#,0}  ({2:#,0.0%} of {3} was removed)",
				CodeBaseA.ShortName,
				NumABlobsNotInB,
				( float )NumABlobsNotInB / CodeBaseA.TotalComparableLines,
				CodeBaseA.ShortName );
			Console.WriteLine( "  New or changed lines for {0}: {1:#,0}  ({2:#,0.0%} of {3} is new)",
				CodeBaseB.ShortName,
				NumBBlobsNotInA,
				( float )NumBBlobsNotInA / CodeBaseB.TotalComparableLines,
				CodeBaseB.ShortName );

			Int32 EffectiveLinesAddedForCodeBaseB = NumBBlobsNotInA - ( CodeBaseA.TotalComparableLines - NumABlobsNotInB );
			if( EffectiveLinesAddedForCodeBaseB > 0 )
			{
				Console.WriteLine( "  Effective line count growth for {0}: {1:#,0}  ({2:#,0.0%} growth since {3})",
					CodeBaseB.ShortName,
					EffectiveLinesAddedForCodeBaseB,
					( float )EffectiveLinesAddedForCodeBaseB / CodeBaseA.TotalComparableLines,
					CodeBaseA.ShortName );
			}
			else
			{
				Console.WriteLine( "  Effective line count reduction for {0}: {1:#,0}  ({2:#,0.0%} smaller since {3})",
					CodeBaseB.ShortName,
					-EffectiveLinesAddedForCodeBaseB,
					( float )-EffectiveLinesAddedForCodeBaseB / CodeBaseA.TotalComparableLines,
					CodeBaseA.ShortName );
			}

			Console.WriteLine();
		}



		///
		/// Run - Off we go!
		///

		public void Run()
		{
			// Spew app name and version
			Console.WriteLine();
			Console.WriteLine( m_AppNameAndVersionString );
			Console.WriteLine( "==============================================================================" );
			Console.WriteLine();


			UnrealSourceStatsConfig MyConfig = new UnrealSourceStatsConfig();


			// Try to load the configuration from disk
			if( File.Exists( m_ConfigFileName ) )
			{
				try
				{
					FileStream MyXMLFile = new FileStream( m_ConfigFileName, FileMode.Open );
					if( MyXMLFile != null )
					{
						XmlSerializer MyXMLSerializer = new XmlSerializer( typeof( UnrealSourceStatsConfig ) );
						MyConfig = ( UnrealSourceStatsConfig )MyXMLSerializer.Deserialize( MyXMLFile );
						MyXMLFile.Close();
					}
				}
				catch( Exception )
				{
					// Ignore exceptions while loading the file
				}
			}
			else
			{
				// No config file, so at least save out a default one for the user to edit
				try
				{
					FileStream MyXMLFile = new FileStream( m_ConfigFileName, FileMode.Create );
					if( MyXMLFile != null )
					{
						XmlSerializer MyXMLSerializer = new XmlSerializer( typeof( UnrealSourceStatsConfig ) );
						MyXMLSerializer.Serialize( MyXMLFile, MyConfig );
						MyXMLFile.Close();
					}
				}
				catch( Exception )
				{
					// Ignore exceptions while loading the file
				}
			}


			// We'll build a list of 'code bases', one for each folder we were given
			List< CodeBaseInfo > CodeBases = new List<CodeBaseInfo>();


			// Start loading folder data
			Int32 NumBaseFolders = MyConfig.CodeBases.Length;
			for( Int32 CurBaseFolderIndex = 0; CurBaseFolderIndex < NumBaseFolders; ++CurBaseFolderIndex )
			{
				UnrealSourceStatsConfig.CodeBaseConfig CodeBaseConfig = MyConfig.CodeBases[ CurBaseFolderIndex ];

				DirectoryInfo BaseFolderInfo = new DirectoryInfo( CodeBaseConfig.BaseFolderName );
				if( BaseFolderInfo == null )
				{
					Console.WriteLine( "Unable to locate the specified folder {0}.", CodeBaseConfig.BaseFolderName );
					return;
				}

				Console.Write( "Loading folder contents for {0}: {1} ...", CodeBaseConfig.ShortName, BaseFolderInfo.FullName );

				// Create a code base object for this base folder
				CodeBaseInfo NewCodeBase = new CodeBaseInfo( CodeBaseConfig.ShortName, BaseFolderInfo );

				// Recursively process folders and files
				LoadFileDataFromFoldersRecursively( CodeBaseConfig.BaseFolderName, NewCodeBase, MyConfig, CodeBaseConfig );

				// Add to list
				CodeBases.Add( NewCodeBase );

				Console.WriteLine();
			}


			Console.WriteLine();
			Console.WriteLine( "Performing diff analysis..." );
			Console.WriteLine();


			// Sequentially compare code bases to one-another
			for( Int32 CurCodeBaseIndex = 0; CurCodeBaseIndex < CodeBases.Count; ++CurCodeBaseIndex )
			{
				CodeBaseInfo CurCodeBase = CodeBases[ CurCodeBaseIndex ];

				Int32 NonCommentNonEmptyLineCount =
					CurCodeBase.TotalLineCount - ( CurCodeBase.CommentLineCount + CurCodeBase.EmptyLineCount );

				// Spew stats!
				Console.WriteLine();
				Console.WriteLine( "Code base statistics for {0}:", CurCodeBase.ShortName );
				Console.WriteLine();
				Console.WriteLine( "  Files processed (matching pattern): {0:#,0}", CurCodeBase.NumFilesInsidePattern );
				Console.WriteLine( "  Size of processed files: {0:#,0} bytes  ({1:#,0.0} MB)", CurCodeBase.TotalSizeOfFilesInsidePattern, CurCodeBase.TotalSizeOfFilesInsidePattern / ( 1024 * 1024 ) );
				Console.WriteLine( "  Total lines: {0:#,0}", CurCodeBase.TotalLineCount );
				Console.WriteLine();
				Console.WriteLine( "  Comparable lines: {0:#,0}  ({1:#,0.0%} of total, {2:#,0} lines filtered out)", CurCodeBase.TotalComparableLines, ( float )CurCodeBase.TotalComparableLines / CurCodeBase.TotalLineCount, CurCodeBase.TotalLineCount - CurCodeBase.TotalComparableLines );
				Console.WriteLine( "  Lines with code (not empty/comment): {0:#,0}  ({1:#,0.0%} of total)", NonCommentNonEmptyLineCount, ( float )NonCommentNonEmptyLineCount / CurCodeBase.TotalLineCount );
				Console.WriteLine( "  Empty lines: {0:#,0}  ({1:#,0.0%} of total)", CurCodeBase.EmptyLineCount, ( float )CurCodeBase.EmptyLineCount / CurCodeBase.TotalLineCount );
				Console.WriteLine( "  Comment lines: {0:#,0}  ({1:#,0.0%} of total)", CurCodeBase.CommentLineCount, ( float )CurCodeBase.CommentLineCount / CurCodeBase.TotalLineCount );
				Console.WriteLine( "  Unique lines: {0:#,0}  ({1:#,0.0%} of comparable)", CurCodeBase.BlobDatabase.Count, ( float )CurCodeBase.BlobDatabase.Count / CurCodeBase.TotalComparableLines );
				Console.WriteLine( "  Number of comments: {0:#,0}", CurCodeBase.CommentCount );
				Console.WriteLine( "  Number of contiguous sets of single-line comments: {0:#,0}", CurCodeBase.ContiguousCommentCount );
				Console.WriteLine();
				Console.WriteLine( "  Total number of files (including filtered files): {0:#,0}", CurCodeBase.TotalFileCount );
				Console.WriteLine( "  Total number of sub-folders: {0:#,0}", CurCodeBase.TotalSubFolderCount );
				Console.WriteLine( "  Number of skipped files (due to file pattern): {0:#,0}", CurCodeBase.NumFilesOutsidePattern );
				Console.WriteLine();

				// Do we have a previous code base to compare against?
				if( CurCodeBaseIndex > 0 )
				{
					for( Int32 PrevCodeBaseIndex = 0; PrevCodeBaseIndex < CurCodeBaseIndex; ++PrevCodeBaseIndex )
					{
						CodeBaseInfo PrevCodeBase = CodeBases[ PrevCodeBaseIndex ];

						// Do the comparison!
						CompareCodeBases( PrevCodeBase, CurCodeBase );
					}
				}
			}

			Console.WriteLine();
		}



		///
		/// Main - Static application entry point
		///

		static void Main( string[] args )
		{
			try
			{
				// Allocate and run UnrealSourceStatsApp!
				UnrealSourceStatsApp USS = new UnrealSourceStatsApp();
				USS.Run();
			}

			catch( Exception ThrownException )
			{
				Console.WriteLine();
				Console.WriteLine( "Error: The following exception was thrown: {0}", ThrownException );
				Console.WriteLine();
			}
		}
	}
}
