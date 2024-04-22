/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace UnrealBuildTool
{
	partial class CPPEnvironment
	{
		/** Contains a cache of include dependencies (direct and indirect). */
		public static DependencyCache IncludeCache = null;

		/** Contains a cache of include dependencies (direct only). */
		public static DependencyCache DirectIncludeCache = null;
		
		/** Contains a mapping from filename to the full path of the header in this environment. */
		Dictionary<string, FileItem> IncludeFileSearchDictionary = new Dictionary<string, FileItem>();

		/** Absolute path to external folder, upper invariant. */
		string AbsoluteExternalPathUpperInvariant = Path.GetFullPath("..\\External\\").ToUpperInvariant();

		/** 
         * Finds the header file that is referred to by a partial include filename. 
         * 
         * @param RelativeIncludePath path relative to the project
         * @param bSkipExternalHeader true to skip processing of headers in external path
         */
		public FileItem FindIncludedFile(string RelativeIncludePath, bool bSkipExternalHeader)
		{
			// Only search for the include file if the result hasn't been cached.
			FileItem Result = null;
			if( !IncludeFileSearchDictionary.TryGetValue(RelativeIncludePath, out Result) )
			{
				// Build a single list of include paths to search.
				List<string> IncludePathsToSearch = new List<string>();
				IncludePathsToSearch.AddRange(IncludePaths);

				// Optionally search for the file in the system include paths.
				foreach( string CurSystemInclude in SystemIncludePaths)
				{
					FileItem CurIncludeResult = FileItem.GetExistingItemByPath( Path.Combine( CurSystemInclude, RelativeIncludePath ) );
					if( CurIncludeResult != null )
					{
					    if( BuildConfiguration.bCheckSystemHeadersForModification ||
						    ( !bSkipExternalHeader && CurIncludeResult.AbsolutePathUpperInvariant.Contains( AbsoluteExternalPathUpperInvariant ) ) )
					    {
						    IncludePathsToSearch.Add( CurSystemInclude );
					    }
					}
				}

				// Find the first include path that the included file exists in.
				foreach (string IncludePath in IncludePathsToSearch)
				{
					if (Result == null)
					{
						Result = FileItem.GetExistingItemByPath(
							Path.Combine(
								IncludePath,
								RelativeIncludePath
								)
							);
					}
					else
					{
						break;
					}
				}

				// Cache the result of the include path search.
				IncludeFileSearchDictionary.Add(RelativeIncludePath, Result);
			}
			
			// Check whether the header should be skipped. We need to do this after resolving as we need 
			// the absolute path to compare against the External folder.
			bool bWasHeaderSkipped = false;
            if (Result != null && bSkipExternalHeader)
			{
				// Check whether header path is under External root.
				if( Result.AbsolutePathUpperInvariant.Contains( AbsoluteExternalPathUpperInvariant ) )
				{
					// It is, skip and reset result.
					Result = null;
					bWasHeaderSkipped = true;
				}
			}

			// Log status for header.
			if( BuildConfiguration.bPrintHeaderResolveInfo )
			{
				if (Result != null)
				{
					Console.WriteLine("Resolved included file \"{0}\" to: {1}", RelativeIncludePath, Result.AbsolutePath);
				}
				else if( bWasHeaderSkipped )
				{
					Console.WriteLine("Skipped included file \"{0}\"", RelativeIncludePath);
				}
				else
				{
					Console.WriteLine("Couldn't resolve included file \"{0}\"", RelativeIncludePath);
				}
			}

			return Result;
		}
		
		/** A cache of the list of other files that are directly or indirectly included by a C++ file. */
		static Dictionary<FileItem, List<FileItem>> IncludedFilesMap = new Dictionary<FileItem, List<FileItem>>();

		/** Finds the files directly or indirectly included by the given C++ file. */
		void GetIncludeDependencies(FileItem CPPFile, ref Dictionary<FileItem,bool> Result)
		{
			if (!IncludedFilesMap.ContainsKey(CPPFile))
			{
				// Add a dummy entry for the include file to avoid infinitely recursing on include file loops.
				IncludedFilesMap.Add(CPPFile, new List<FileItem>());

				// Gather a list of names of files directly included by this C++ file.
				List<string> DirectlyIncludedFileNames = GetDirectIncludeDependencies(CPPFile);

				// Build a list of the unique set of files that are included by this file.
				Dictionary<FileItem, bool> IncludedFileDictionary = new Dictionary<FileItem, bool>();
				foreach (string DirectlyIncludedFileName in DirectlyIncludedFileNames)
				{
					// Resolve the included file name to an actual file.
                    FileItem IncludedFile = FindIncludedFile(DirectlyIncludedFileName, !BuildConfiguration.bCheckExternalHeadersForModification);
					if (IncludedFile != null)
					{
						if (!IncludedFileDictionary.ContainsKey(IncludedFile))
						{
							IncludedFileDictionary.Add(IncludedFile,true);
						}
					}
				}

				// Convert the dictionary of files included by this file into a list.
				List<FileItem> IncludedFileList = new List<FileItem>();
				foreach (KeyValuePair<FileItem, bool> IncludedFile in IncludedFileDictionary)
				{
					IncludedFileList.Add(IncludedFile.Key);
				}

				// Add the set of files included by this file to the cache.
				IncludedFilesMap.Remove(CPPFile);
				IncludedFilesMap.Add(CPPFile, IncludedFileList);
			}

			// Copy the list of files included by this file into the result list.
			foreach (FileItem IncludedFile in IncludedFilesMap[CPPFile])
			{
				if (!Result.ContainsKey(IncludedFile))
				{
					// If the result list doesn't contain this file yet, add the file and the files it includes.
					Result.Add(IncludedFile,true);
					GetIncludeDependencies(IncludedFile, ref Result);
				}
			}
		}

		/** @return The list of files which are directly or indirectly included by a C++ file. */
		public List<FileItem> GetIncludeDependencies(FileItem CPPFile)
		{
			// Try to fulfill request from cache first.
			List<FileItem> CachedDependencies = null;
			if (IncludeCache != null && IncludeCache.TryFetchDependencies(CPPFile, out CachedDependencies))
			{
				return CachedDependencies;
			}

			// Find the dependencies of the file.
			Dictionary<FileItem, bool> IncludedFileDictionary = new Dictionary<FileItem, bool>();
			GetIncludeDependencies(CPPFile, ref IncludedFileDictionary);

			// Convert the dependency dictionary into a list.
			List<FileItem> Result = new List<FileItem>();
			foreach (KeyValuePair<FileItem, bool> IncludedFile in IncludedFileDictionary)
			{
				Result.Add(IncludedFile.Key);
			}

			// Populate cache with results.
			if (IncludeCache != null)
			{
				IncludeCache.Update(CPPFile, Result);
			}

			return Result;
		}

		/** Regex that matches #include statements. */
		static Regex CPPHeaderRegex = new Regex(	"(([ \t]*#[ \t]*include[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n)|([^\n]*\n))*", 
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture );

		static Regex MMHeaderRegex = new Regex(		"(([ \t]*#[ \t]*import[ \t]*[<\"](?<HeaderFile>[^\">]*)[\">][^\n]*\n)|([^\n]*\n))*",
													RegexOptions.Compiled | RegexOptions.Singleline | RegexOptions.ExplicitCapture);

		/** Finds the names of files directly included by the given C++ file. */
		public List<string> GetDirectIncludeDependencies( FileItem CPPFile )
		{
			// Try to fulfill request from cache first.
			List<string> Result = null;
			if (DirectIncludeCache.TryFetchDirectDependencies(CPPFile, out Result))
			{
				return Result;
			}

			string FileToRead = CPPFile.AbsolutePath;
			// iPhone and Mac need special handling - it needs the real Unity file to have Mac paths, but we need
			// an extra file for tracking includes
			if ((TargetPlatform == CPPTargetPlatform.IPhone || TargetPlatform == CPPTargetPlatform.Mac) &&
				Path.GetFileName(FileToRead).StartsWith("Unity_"))
			{
				FileToRead += ".ex";
			}

			// Read lines from the C++ file.
			string FileContents = Utils.ReadAllText(FileToRead);
			Match M = CPPHeaderRegex.Match( FileContents );
			CaptureCollection CC = M.Groups["HeaderFile"].Captures;
			Result = new List<string>( CC.Count );
			foreach( Capture C in CC )
			{
				Result.Add( C.Value );
			}

			// also look for #import in objective C files
			string Ext = Path.GetExtension(CPPFile.AbsolutePathUpperInvariant);
			if (Ext == ".MM" || Ext == ".M")
			{
				M = MMHeaderRegex.Match(FileContents);
				CC = M.Groups["HeaderFile"].Captures;
				Result.Capacity += CC.Count;
				foreach (Capture C in CC)
				{
					Result.Add(C.Value);
				}
			}

			// Populate cache with results.
			DirectIncludeCache.Update(CPPFile, Result);

			return Result;
		}
	}
}