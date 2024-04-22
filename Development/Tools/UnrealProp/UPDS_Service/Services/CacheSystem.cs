/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Collections;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Security.AccessControl;
using System.Security.Permissions;
using System.Security.Principal;
using System.Runtime.InteropServices;
using RemotableType;

namespace UnrealProp
{
    public class CachedFile
    {
        public long FileSize = 0;
    }

    public class FileToCache
    {
        public string RepositoryPath = "";
        public long FileSize = 0;
        public bool IdleMode = false;
		public bool Remove = false;
		public bool Force = false;

        public FileToCache( string InRepositoryPath, long InFileSize, bool InIdleMode, bool InRemove, bool InForce )
        {
            RepositoryPath = InRepositoryPath;
            FileSize = InFileSize;
            IdleMode = InIdleMode;
			Remove = InRemove;
			Force = InForce;
        }
    }

    static class CacheSystem
    {
        static Object SyncObject = new Object();

        static string CachePath = Properties.Settings.Default.CacheLocation + "\\";
        static Hashtable CachedFiles = Hashtable.Synchronized( new Hashtable() );
        static Hashtable FilesToCache = Hashtable.Synchronized( new Hashtable() );
        static Thread Worker;

        static public int GetNumberFilesToCache()
        {
            return ( FilesToCache.Count );
        }

		static public int GetNumberFilesToForceCache()
		{
			int Count = 0;
			lock( FilesToCache.SyncRoot )
			{
				foreach( FileToCache File in FilesToCache.Values )
				{
					if( !File.IdleMode )
					{
						Count++;
					}
				}
			}

			return ( Count );
		}

        static public long GetCacheSize()
        {
            long CacheSize = 0;

            lock( CachedFiles.SyncRoot )
            {
				foreach( CachedFile FileData in CachedFiles.Values )
				{
					CacheSize += FileData.FileSize;
				}
            }

            return ( CacheSize );
        }

		static public string GetCachedFilePath( string Hash )
		{
			return CachePath + Hash.Substring( 0, 2 ) + "\\" + Hash;
		}

		static public string GetCleanHash( string InHash )
		{
			string Hash = InHash.Trim().ToUpper();
			Debug.Assert( Hash.Length == 40 );
			return ( Hash );
		}

        static public void Init()
        {
            CachedFiles.Clear();

            Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Important, "Creating cache folder: " + CachePath );
            Directory.CreateDirectory( CachePath );
            char[] HexChars = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
            foreach( char HiNybble in HexChars )
            {
                foreach( char LoNybble in HexChars )
                {
                    string Path = CachePath + HiNybble + LoNybble;
                    Directory.CreateDirectory( Path );

                    // enumerate cached files on disk
                    DirectoryInfo DirInfo = new DirectoryInfo( Path );
                    FileInfo[] Files = DirInfo.GetFiles();
                    foreach( FileInfo File in Files )
                    {
                        CachedFile NewCachedFile = new CachedFile();
                        NewCachedFile.FileSize = File.Length;
						CachedFiles.Add( GetCleanHash( File.Name ), NewCachedFile );
                    }
                }
            }

            Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Important, "Initialised!" );
			long CacheSizeGB = GetCacheSize() / ( 1024 * 1024 * 1024 );
            Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Info, "Found " + CachedFiles.Count.ToString() + " files in local cache (" + CacheSizeGB.ToString() + " GB)" );

            Worker = new Thread( new ThreadStart( FileCacherProc ) );
            Worker.Start();
        }

        static public bool PrecacheBuild( PlatformBuildFiles Files, string RepositoryPath, bool IdleMode )
        {
            // end of idling
            if( IdleMode == false )
            {
                Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Removing files marked to cache idly from caching list" );

                List<string> KeysToRemove = new List<string>();

                // remove all idle files from cache
                lock( FilesToCache.SyncRoot )
                {
					foreach( string Hash in FilesToCache.Keys )
					{
						FileToCache CachedFile = ( FileToCache )FilesToCache[Hash];
						if( CachedFile.IdleMode )
						{
							KeysToRemove.Add( Hash );
						}
					}

					Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, " ... removing " + KeysToRemove.Count.ToString() + " files" );
					foreach( string Hash in KeysToRemove )
					{
						FilesToCache.Remove( Hash );
					}
				}
			}

            Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Adding " + Files.Tables[0].Rows.Count.ToString() + " required build files" );
            foreach( PlatformBuildFiles.PlatformBuildFilesRow File in Files.Tables[0].Rows )
            {
				FileToCache CacheData = new FileToCache( RepositoryPath + "\\" + File.Path.Trim(), File.Size, IdleMode, false, false );
				AddCacheOperation( GetCleanHash( File.Hash ), CacheData );
            }

            return( true );
        }

		/** 
		 * Delete all files that exist on the client, but are no longer referenced by any build
		 */
        static public void DeleteOrphans()
        {
			List<string> Keys = new List<string>();
			lock( CachedFiles.SyncRoot )
			{
				foreach( string Hash in CachedFiles.Keys )
				{
					Keys.Add( Hash );
				}
			}

			Random NumberGenerator = new Random();
			for( int FileToCheck = 0; FileToCheck < 500; FileToCheck++ )
			{
				string Hash = Keys[NumberGenerator.Next( Keys.Count )];

				if( !UPDS_Service.IUPMS.CachedFileInfo_FileExists( Hash ) )
				{
					FileToCache CacheData = new FileToCache( Hash, 0, false, true, true );
					AddCacheOperation( Hash, CacheData );

					Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Purging orphaned cache file: (" + Hash + ")" );
				}
			}
        }

		/** 
		 * Container for a cached file
		 */
		public class CachedFileData
		{
			public string Hash;
			public FileInfo Info;

			public CachedFileData( string InHash )
			{
				Hash = GetCleanHash( InHash );
				Info = new FileInfo( GetCachedFilePath( Hash ) );
			}
		}

		public class FileInfoComparer : IComparer<CachedFileData>
		{
			public int Compare( CachedFileData Left, CachedFileData Right )
			{
				TimeSpan Age = Left.Info.LastAccessTimeUtc - Right.Info.LastAccessTimeUtc;
				return ( Age.Milliseconds );
			}
		}

		/** 
		 * Delete the oldest files if we are above the cache limit
		 */
        static public void DeleteOldFiles()
        {
            // Check to see the size of the cache
            long CacheSize = GetCacheSize();
            long LocalCacheLimit = Int64.Parse( Properties.Settings.Default.CacheSizeGB ) * 1024 * 1024 * 1024;
            if( CacheSize > LocalCacheLimit )
            {
				// Make a copy of the cached files as they could be accessed on a different thread
				Hashtable CurrentCachedFiles = null;
				lock( CachedFiles.SyncRoot )
				{
					CurrentCachedFiles = new Hashtable( CachedFiles );
				}

				// Get the last accessed time for all the files
				List<CachedFileData> CachedFileDatas = new List<CachedFileData>();
				foreach( string Hash in CurrentCachedFiles.Keys )
				{
					CachedFileDatas.Add( new CachedFileData( Hash ) );
				}

				// Sort by last accessed time
				FileInfoComparer Comparer = new FileInfoComparer();
				CachedFileDatas.Sort( Comparer );

                // Delete files from end (oldest) until cache is back under control
				foreach( CachedFileData Oldest in CachedFileDatas )
                {
					FileToCache CacheData = new FileToCache( Oldest.Hash, 0, false, true, true );
					AddCacheOperation( Oldest.Hash, CacheData );

					CacheSize -= Oldest.Info.Length;

                    Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Purging old cache file: (" + Oldest.Hash + "-" + Oldest.Info.LastAccessTimeUtc.ToString() + ")" );
                    if( CacheSize < LocalCacheLimit )
                    {
						double CacheSizeGB = CacheSize / ( 1024.0 * 1024.0 * 1024.0 );
						Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Info, "Cache is now (GB): " + CacheSizeGB.ToString( "f2" ) );
						break;
                    }
                }
            }
        }

		/**
		 * Remove file from cache and disk
		 */
		static private bool UncacheFile( string Hash, FileToCache CacheData )
		{
			try
			{
				lock( CachedFiles.SyncRoot )
				{
					if( !CachedFiles.ContainsKey( Hash ) )
					{
						Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "File to uncache not in cache: (" + Hash + ") " + CacheData.RepositoryPath );
						return ( true );
					}
				}

				string DestFileName = GetCachedFilePath( Hash );
				if( !File.Exists( DestFileName ) )
				{
					Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "File to uncache does not exist on disk: (" + Hash + ") " + CacheData.RepositoryPath );
					return ( true );
				}

				File.Delete( DestFileName );

				if( File.Exists( DestFileName ) )
				{
					Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Failed to delete file on disk: (" + Hash + ") " + CacheData.RepositoryPath );
					return ( false );
				}

				lock( CachedFiles.SyncRoot )
				{
					CachedFiles.Remove( Hash );
				}

				Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Deleted local file: " + CacheData.RepositoryPath + " (" + FilesToCache.Count.ToString() + " cache operations pending)" );
			}
			catch
			{
				Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Error, "Exception in UncacheFile" );
			}

			return true;
		}

		/** 
		 * Copy file from build repository and add to the cache
		 */
		static private bool CacheFile( string Hash, FileToCache CacheData )
		{
			try
			{
				lock( CachedFiles.SyncRoot )
				{
					if( CachedFiles.ContainsKey( Hash ) )
					{
						if( !CacheData.Force )
						{
							Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Already in cached files hash: (" + Hash + ") " + CacheData.RepositoryPath );
							return ( true );
						}
						else
						{
							Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "Force recache of: (" + Hash + ") " + CacheData.RepositoryPath );
							CachedFiles.Remove( Hash );
						}
					}
				}

				CachedFile NewCachedFile = new CachedFile();
				string DestFileName = GetCachedFilePath( Hash );

				if( !File.Exists( DestFileName ) )
				{
					string ErrorString = "";

					NewCachedFile.FileSize = Utils.CopyFile( CacheData.RepositoryPath, DestFileName, ref ErrorString );
					if( NewCachedFile.FileSize >= 0 )
					{
						Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "File copied from server: (" + Hash + ") " + CacheData.RepositoryPath + " (" + FilesToCache.Count.ToString() + " cache operations pending)" );
					}
					else
					{
						Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Error, "Utils.CopyFile: (" + Hash + ") " + CacheData.RepositoryPath + " : " + ErrorString );
					}
				}
				else
				{
					Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Debug, "File found in disk cache: (" + Hash + ") " + CacheData.RepositoryPath );
				}

				lock( CachedFiles.SyncRoot )
				{
					CachedFiles.Add( Hash, NewCachedFile );
				}
			}
			catch
			{
				Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Error, "Exception in CacheFile" );
			}

			return true;
		}

		static public bool IsFileAvailable( PlatformBuildFiles.PlatformBuildFilesRow File, string RepositoryPath )
		{
			string Hash = GetCleanHash( File.Hash );

			bool Available = false;
			lock( CachedFiles.SyncRoot )
			{
				Available = CachedFiles.ContainsKey( Hash );
			}

			if( !Available )
			{
				FileToCache CacheData = new FileToCache( RepositoryPath + "\\" + File.Path.Trim(), File.Size, false, false, false );
				AddCacheOperation( Hash, CacheData );
			}

			return ( Available );
		}

        static public void AddCacheOperation( string Hash, FileToCache CacheData )
        {
			lock( FilesToCache.SyncRoot )
			{
				if( !FilesToCache.ContainsKey( Hash ) )
				{
					FilesToCache.Add( Hash, CacheData );
				}
            }
        }

        static void FileCacherProc()
        {
            while( true )
            {
#if !DEBUG
                try
                {
#endif
                    while( FilesToCache.Count > 0 )
                    {
                        string Hash;
                        FileToCache CacheData;

                        lock( FilesToCache.SyncRoot )
                        {
                            IDictionaryEnumerator Enumerator = FilesToCache.GetEnumerator();
                            Enumerator.MoveNext();
                            Hash = Enumerator.Key.ToString().ToUpper();
                            CacheData = FilesToCache[Hash] as FileToCache;

                            FilesToCache.Remove( Hash );
                        }

						bool bSuccess = false;
						if( CacheData.Remove )
						{
							bSuccess = UncacheFile( Hash, CacheData );
						}
						else
						{
							bSuccess = CacheFile( Hash, CacheData );
						}

						if( bSuccess )
						{
							if( CacheData.IdleMode )
							{
								// sleep 0.5 on idle mode
								Thread.Sleep( 500 );
							}
						}
						else
						{
							// If the cache failed, readd the file to be cached
							AddCacheOperation( Hash, CacheData );
						}

                        Thread.Sleep( 10 );

						// Send a periodic status email
						UPDS_Service.SendStatusEmail();
					}

                    Thread.Sleep( 100 );
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    Log.WriteLine( "UPDS CACHE SYSTEM", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                }
#endif
            }
        }
    }
}
