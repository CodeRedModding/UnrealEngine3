/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;

namespace UnrealBuildTool
{
	/**
	 * Represents a file on disk that is used as an input or output of a build action.
	 * FileItems are created by calling FileItem.GetItemByPath, which creates a single FileItem for each unique file path.
	 */
	class FileItem
	{
		/** The action that produces the file. */
		public Action ProducingAction = null;

		/** The absolute path of the file. */
		public readonly string AbsolutePath;

		/** The absolute path of the file, stored as uppercase invariant. */
		public readonly string AbsolutePathUpperInvariant;

        /** The pch file that this file will use */
        public string PrecompiledHeaderIncludeFilename;

        /** Description of file, used for logging. */
        public string Description;

		/** Relative cost of action associated with producing this file. */
		public long RelativeCost = 0;

		/** Whether or not this is a remote file, in which case we can't access it directly */
		public bool bIsRemoteFile;

		/** A dictionary that's used to map each unique file name to a single FileItem object. */
		static Dictionary<string, FileItem> UniqueSourceFileMap = new Dictionary<string, FileItem>();

		/** A list of remote file items that have been created but haven't needed the remote info yet, so we can gang up many into one request */
		static List<FileItem> DelayedRemoteLookupFiles = new List<FileItem>();
		
		/** The last write time of the file. */
		public DateTimeOffset _LastWriteTime;
		public DateTimeOffset LastWriteTime
		{
			get
			{
				LookupOutstandingFiles();
				return _LastWriteTime;
			}
			set { _LastWriteTime = value; }
		}

		/** Whether the file exists. */
		public bool _bExists;
		public bool bExists
		{
			get 
			{
				LookupOutstandingFiles();
				return _bExists; 
			}
			set { _bExists = value; }
		}

		/** Size of the file if it exists, otherwise -1 */
		public long _Length = -1;
		public long Length
		{
			get
			{
				LookupOutstandingFiles();
				return _Length;
			}
			set { _Length = value; }
		}

		/**
		 * Resolve any outstanding remote file info lookups
		 */
		private void LookupOutstandingFiles()
		{
			// for remote files, look up any outstanding files
			if (bIsRemoteFile)
			{
				FileItem[] Files = null;
				lock (DelayedRemoteLookupFiles)
				{
					if (DelayedRemoteLookupFiles.Count > 0)
					{
						// make an array so we can clear the original array, just in case BatchFileInfo does something that uses
						// DelayedRemoteLookupFiles, so we don't deadlock
						Files = DelayedRemoteLookupFiles.ToArray();
						DelayedRemoteLookupFiles.Clear();
					}
				}
				if (Files != null)
				{
					RPCUtilHelper.BatchFileInfo(Files);
				}
			}
		}



		/** @return The FileItem that represents the given file path. */
		public static FileItem GetItemByPath(string Path)
		{
			string FullPath = System.IO.Path.GetFullPath(Path);
			string InvariantPath = FullPath.ToUpperInvariant();
			FileItem Result = null;
			if( UniqueSourceFileMap.TryGetValue(InvariantPath, out Result) )
			{
				return Result;
			}
			else
			{
				return new FileItem(FullPath);
			}
		}

		/** @return The remote FileItem that represents the given file path. */
		public static FileItem GetRemoteItemByPath(string AbsoluteRemotePath, UnrealTargetPlatform Platform)
		{
			if (AbsoluteRemotePath.StartsWith("."))
			{
				throw new BuildException("GetRemoteItemByPath must be passed an absolute path, not a relative path '{0}'", AbsoluteRemotePath);
			}

			string InvariantPath = AbsoluteRemotePath.ToUpperInvariant();
			FileItem Result = null;
			if (UniqueSourceFileMap.TryGetValue(InvariantPath, out Result))
			{
				return Result;
			}
			else
			{
				return new FileItem(AbsoluteRemotePath, true, Platform);
			}
		}

		/** If the given file path identifies a file that already exists, returns the FileItem that represents it. */
		public static FileItem GetExistingItemByPath(string Path)
		{
			FileItem Result = GetItemByPath(Path);
			if (Result.bExists)
			{
				return Result;
			}
			else
			{
				return null;
			}
		}

		/**
		 * Creates a text file with the given contents.  If the contents of the text file aren't changed, it won't write the new contents to
		 * the file to avoid causing an action to be considered outdated.
		 */
		public static FileItem CreateIntermediateTextFile(string AbsolutePath, string Contents)
		{
			// Create the directory if it doesn't exist.
			Directory.CreateDirectory(Path.GetDirectoryName(AbsolutePath));

			// Only write the file if its contents have changed.
			if (!File.Exists(AbsolutePath) || Utils.ReadAllText(AbsolutePath) != Contents)
			{
				File.WriteAllText(AbsolutePath, Contents);
			}

			return GetItemByPath(AbsolutePath);
		}

		/** Deletes the file. */
		public void Delete()
		{
			Debug.Assert(_bExists);
			Debug.Assert(!bIsRemoteFile);

			int MaxRetryCount = 3;
			int DeleteTryCount = 0;
			bool bFileDeletedSuccessfully = false;
			do
			{
				// If this isn't the first time through, sleep a little before trying again
				if( DeleteTryCount > 0 )
				{
					Thread.Sleep( 1000 );
				}
				DeleteTryCount++;
				try
				{
					// Delete the destination file if it exists
					FileInfo DeletedFileInfo = new FileInfo( AbsolutePath );
					if( DeletedFileInfo.Exists )
					{
						DeletedFileInfo.IsReadOnly = false;
						DeletedFileInfo.Delete();
					}
					// Success!
					bFileDeletedSuccessfully = true;
				}
				catch( Exception Ex )
				{
					Console.WriteLine( "Failed to delete file '" + AbsolutePath + "'" );
					Console.WriteLine( "    Exception: " + Ex.Message );
					if( DeleteTryCount < MaxRetryCount )
					{
						Console.WriteLine( "Attempting to retry..." );
					}
					else
					{
						Console.WriteLine( "ERROR: Exhausted all retries!" );
					}
				}
			}
			while( !bFileDeletedSuccessfully && ( DeleteTryCount < MaxRetryCount ) );
		}

		/** Initialization constructor. */
		public FileItem(string InAbsolutePath)
		{
			AbsolutePath = InAbsolutePath;
			// Convert to upper invariant as it's the unique key.
			AbsolutePathUpperInvariant = AbsolutePath.ToUpperInvariant();
			
			FileInfo Info = new FileInfo(AbsolutePath);
			
			_bExists = Info.Exists;
			if (_bExists)
			{
				_LastWriteTime = Info.LastWriteTimeUtc;
				_Length = Info.Length;
			}

			UniqueSourceFileMap[AbsolutePathUpperInvariant] = this;
		}

		/** Initialization constructor for optionally remote files. */
		public FileItem(string InAbsolutePath, bool InIsRemoteFile, UnrealTargetPlatform Platform)
		{
			bIsRemoteFile = InIsRemoteFile;
			AbsolutePath = InAbsolutePath;
			// Convert to upper invariant as it's the unique key.
			AbsolutePathUpperInvariant = AbsolutePath.ToUpperInvariant();

			if (bIsRemoteFile)
			{
				if (Platform == UnrealTargetPlatform.IPhone || Platform == UnrealTargetPlatform.Mac)
				{
					lock (DelayedRemoteLookupFiles)
					{
						DelayedRemoteLookupFiles.Add(this);
					}
				}
				else
				{
					throw new BuildException("Only IPhone and Mac support remote FileItems");
				}
			}
			else
			{
				FileInfo Info = new FileInfo(AbsolutePath);

				_bExists = Info.Exists;
				if (_bExists)
				{
					_LastWriteTime = Info.LastWriteTimeUtc;
					_Length = Info.Length;
				}
			}

			UniqueSourceFileMap[AbsolutePathUpperInvariant] = this;
		}

		public override string ToString()
		{
			return Path.GetFileName(AbsolutePath);
		}
	}

}
