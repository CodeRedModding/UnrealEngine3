/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Data;
using System.IO;
using System.Security;
using System.Text;
using System.Threading;
using RemotableType;
using RemotableType.PlatformBuildFilesTableAdapters;

namespace UnrealProp
{
    // Working thread for analysing discovered builds
    public static class BuildAnalyzer
    {
        static Random Rnd = new Random( 5000 );
        static Thread Thread;
        static object SyncObject = new Object();
        static long CurID = -1;
        static int CurProgress = 0;

        static public long CurrentPlatformBuildID
        {
            get
            {
                long Id = -1;
                lock( SyncObject )
                {
                    Id = CurID;
                }
                return Id;
            }
            set
            {
                lock( SyncObject )
                {
                    CurID = value;
                }
            }
        }

        static public int CurrentProgress
        {
            get
            {
                int Progress = 0;
                lock( SyncObject )
                {
                    Progress = CurProgress;
                }
                return Progress;
            }
            set
            {
                lock( SyncObject )
                {
                    CurProgress = value;
                }
            }
        }

        static public void Init()
        {
            Thread = new Thread( new ThreadStart( AnalysingProc ) );
            Thread.Start();
            Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Important, "Initialised!" );
        }

        static public void Release()
        {
			if( Thread != null )
			{
				Thread.Abort();
				Thread = null;
			}
        }

        static string GetSHA1Hash( string PathName )
        {
            string HashString = "";

            byte[] HashValue;
            FileStream Stream = null;
            System.Security.Cryptography.SHA1CryptoServiceProvider SHA1Hasher = new System.Security.Cryptography.SHA1CryptoServiceProvider();

#if !DEBUG
            try
            {
#endif
                Stream = new FileStream( PathName, FileMode.Open, FileAccess.Read, FileShare.ReadWrite );
                HashValue = SHA1Hasher.ComputeHash( Stream );
                Stream.Close();

                HashString = BitConverter.ToString( HashValue );
                HashString = HashString.Replace( "-", "" );
#if !DEBUG
            }
            catch( Exception Ex )
            {
                Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Error, Ex.ToString() );
            }
#endif

            return ( HashString );
        }

        static void RecursiveAnalyze( DirectoryInfo DirInfo, ref PlatformBuildFiles FilesTable, long PlatformBuildID, int RelativePathStart )
        {
            FileInfo[] Files = DirInfo.GetFiles( "*.*" );
            foreach( FileInfo File in Files )
            {
                PlatformBuildFiles.PlatformBuildFilesRow Row = ( PlatformBuildFiles.PlatformBuildFilesRow )FilesTable.Tables[0].NewRow();
                Row.PlatformBuildID = PlatformBuildID;
                Row.Path = File.FullName.Substring( RelativePathStart );
                Row.Size = File.Length;
                Row.DateAndTime = File.LastWriteTime;
                Row.Hash = "";
                FilesTable.Tables[0].Rows.Add( Row );
            }

            DirectoryInfo[] SubDirs = DirInfo.GetDirectories();
            foreach( DirectoryInfo SubDir in SubDirs )
            {
                RecursiveAnalyze( SubDir, ref FilesTable, PlatformBuildID, RelativePathStart );
            }
        }

        static void AnalyzeBuild( DataRow Build )
        {
#if !DEBUG
            try
            {
#endif
                Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Info, "started analysing new build :" + Build["Path"].ToString().Trim() );

                long PlatformBuildID = Convert.ToInt64( Build["ID"] );
                CurrentPlatformBuildID = PlatformBuildID;
                CurrentProgress = 0;

                DataHelper.PlatformBuild_ChangeStatus( PlatformBuildID, BuildStatus.Analyzing, null );

                // delete all analysed files (analysing was broken)
                DataHelper.ExecuteNonQuery( "DELETE FROM [PlatformBuildFiles] WHERE ( PlatformBuildID = " + PlatformBuildID + " )" );

                // analyzing files
                string Path = Build["Path"].ToString().Trim();
                PlatformBuildFiles FilesTable = new PlatformBuildFiles();
                DirectoryInfo DirInfo = new DirectoryInfo( Path );

				if( !DirInfo.Exists )
				{
					DataHelper.PlatformBuild_ChangeStatus( PlatformBuildID, BuildStatus.Deleted, null );
					Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Important, "build does not exist; aborting" );
					return;
				}

                // Get all the files in the directory tree
                RecursiveAnalyze( DirInfo, ref FilesTable, PlatformBuildID, Path.Length );
                int CurFile = 0;
                int Files = FilesTable.Tables[0].Rows.Count;

                // Calculate the hash values for all the found files
                foreach( PlatformBuildFiles.PlatformBuildFilesRow File in FilesTable.Tables[0].Rows )
                {
                    File.Hash = GetSHA1Hash( Path + File.Path );
                    CurrentProgress = ( int )( ( ( float )CurFile / ( float )Files ) * 100.0f );
                    Log.WriteLine( "UPMS BUILD ANALYZER", Log.LogType.Debug, CurrentProgress + "% " + File.Hash + " : " + File.Path );
                    CurFile++;
                }

                // Save to the database
                DataHelper.PlatformBuildFiles_Update( FilesTable );

                // Work out the total size and update the database
                DataHelper.PlatformBuild_ChangeSize( PlatformBuildID );

                // Inform the user that analysing has finished
                DataHelper.PlatformBuild_ChangeStatus( PlatformBuildID, BuildStatus.Ready, null );
                Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Info, "finished analysing new build :" + Build["Path"].ToString().Trim() );
#if !DEBUG
            }
            catch( Exception Ex )
            {
                Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
            }
#endif
        }

        // Main thread to analyse a build
        static void AnalysingProc()
        {
            Thread.Sleep( 10 * 1000 );

            while( true )
            {
#if !DEBUG
                try
                {
#endif
                    // all discovered builds or broken during analyzing
                    DataSet Builds = DataHelper.GetDataSet( "SELECT * FROM [PlatformBuilds] WHERE ( StatusID IN ( 1, 2 ) ) ORDER BY StatusID DESC" );
                    foreach( DataRow Build in Builds.Tables[0].Rows )
                    {
                        AnalyzeBuild( Build );
                    }

                    CurrentPlatformBuildID = -1;

                    // 55-65 sec interval
                    Thread.Sleep( Rnd.Next( 55000, 65000 ) ); 
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                    {
                        Log.WriteLine( "UPMS BUILD ANALYSER", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                    }
                }
#endif
            }
        }

        static public int PlatformBuild_GetAnalyzingProgress( long PlatformBuildID )
        {
            if( PlatformBuildID == CurrentPlatformBuildID )
            {
                return( CurrentProgress ); 
            }

            return( 0 );
        }
    }
}
