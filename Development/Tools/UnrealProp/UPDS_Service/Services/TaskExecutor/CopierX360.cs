/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.IO;
using System.Collections;
using XDevkit;
using RemotableType;

namespace UnrealProp
{
    class CopierX360 : CopierTask
    {
        private X360FileManager XFileManager = new X360FileManager();

        public CopierX360( Tasks.TasksRow Task )
        {
            TaskData = Task;
            Worker = new Thread( new ThreadStart( WorkerProc ) );
            Worker.Start();
        }

		private void Cleanup( IXboxConsole Target )
		{
			if( Target != null )
			{
                XFileManager.DisconnectFromTarget( Target );
			}
		}

		private string HackDeployPath( string FileName )
		{
			int FolderKey = FileName.ToLower().IndexOf( "\\binaries\\xbox360" );
			if( FolderKey >= 0 )
			{
				string Extension = Path.GetExtension( FileName ).ToLower();
				if( Extension == ".xex" || Extension == ".pdb" )
				{
					FileName = FileName.Substring( 0, FolderKey ) + FileName.Substring( FolderKey + "\\binaries\\xbox360".Length );
				}
			}
			return ( FileName );
		}

        override public void WorkerProc()
        {
            IXboxConsole Target = null;

#if !DEBUG
			try
            {
#endif
                const ulong SpaceNeeded = 10L * 1024L * 1024L * 1024L;

                string HashedFileName;
                string FileName;
                string DestPath;
                string DestFolder;
                short LastProgress = -1;
                int MaxErrors = 10;
                bool TaskCancelled = false;
                Hashtable CreatedDirectories = new Hashtable();

                string TargetName = TaskData.Path;
                string DestinationPath = UPDS_Service.IUPMS.PlatformBuild_GetTitle( TaskData.PlatformBuildID ).Replace( '=', '-' );
                string RepositoryPath = UPDS_Service.IUPMS.PlatformBuild_GetRepositoryPath( TaskData.PlatformBuildID );
                FilesToCopy = UPDS_Service.IUPMS.PlatformBuild_GetFiles( TaskData.PlatformBuildID );
                long BuildSize = UPDS_Service.IUPMS.PlatformBuild_GetBuildSize( TaskData.PlatformBuildID );

                // Precache all files in this build (before the reboot)
                CacheSystem.PrecacheBuild( FilesToCopy, RepositoryPath, false );

                // Connect to Xenon
                Target = XFileManager.ConnectToTarget( TargetName );
                if( Target == null )
                {
					SendUserFailureEmail( "Failed to connect to console: " + TargetName + " - is it turned on?" );
					Cleanup( Target );
					TaskExecutor.TaskError( this, "Connection failure" );
					return;
                }

                // Optionally reboot console
                if( TaskData.Reboot )
                {
                    if( !XFileManager.RebootTarget( Target ) )
                    {
						SendUserFailureEmail( "Failed to reboot console: " + TargetName );
						Cleanup( Target );
						TaskExecutor.TaskError( this, "Reboot failure" );
						return;
                    }

                    Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Debug, "After reboot console..." );
                    // Wait for console reboot.
                    Thread.Sleep( 30000 );
                    Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Debug, "After sleep..." );
                }

                // Reconnect to target
                Target = XFileManager.ConnectToTarget( TargetName );
                if( Target == null )
                {
					SendUserFailureEmail( "Failed to connect to console after reboot: " + TargetName + " - is it turned on?" );
					Cleanup( Target );
					TaskExecutor.TaskError( this, "Reconnection failure" );
					return;
                }

				Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Debug, "After Connect to console..." );

                // Delete the destination folder to ensure a clean start
                XFileManager.DeleteDirectory( Target, DestinationPath );

				Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Debug, "After delete directory..." );

                if( XFileManager.AvailableSpace( Target ) < SpaceNeeded )
                {
					SendUserFailureEmail( "Not enough space available for build on: " + TargetName );
					Cleanup( Target );
					TaskExecutor.TaskError( this, "Not enough free space" );
					return;
                }
                
                int NumberFilesToCopy = FilesToCopy.Tables[0].Rows.Count;
                long NumberBytesCopied = 0;
                do
                {
                    foreach( PlatformBuildFiles.PlatformBuildFilesRow Row in FilesToCopy.Tables[0].Rows )
                    {
                        if( Row.RowState != System.Data.DataRowState.Modified )
                        {
                            if( CacheSystem.IsFileAvailable( Row, RepositoryPath ) )
                            {
                                NumberBytesCopied += Row.Size;
                                TaskData.Progress = ( short )( ( NumberBytesCopied * 100.0 ) / ( ( double )BuildSize ) );

                                // Get the cached file name
                                HashedFileName = CacheSystem.GetCachedFilePath( Row.Hash.Trim() );
                                FileName = Row.Path.Trim();

								FileName = HackDeployPath( FileName );

								// Create any new required folders
                                DestPath = DestinationPath + FileName;
                                DestFolder = Path.GetDirectoryName( DestPath );
                                if( !CreatedDirectories.ContainsKey( DestFolder ) )
                                {
                                    XFileManager.CreateDirectory( Target, DestFolder );
                                    CreatedDirectories.Add( DestFolder, true );
                                }

                                FileInfo HashedFileInfo = new FileInfo( HashedFileName );
                                if( HashedFileInfo.Exists )
                                {
                                    // Send the file to the Xenon
                                    if( XFileManager.SendFile( Target, HashedFileName, DestPath ) )
                                    {
                                        Row.SetModified();
                                        NumberFilesToCopy--;

										Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Debug, TaskData.Progress + "%: " + Row.Path );
                                        if( TaskData.Progress != LastProgress )
                                        {
                                            LastProgress = TaskData.Progress;
                                            if( !UPDS_Service.IUPMS.Task_UpdateStatus( TaskData.ID, TaskStatus.InProgress, TaskData.Progress, "In progress" ) )
                                            {
                                                TaskCancelled = true;
                                                break;
                                            }
                                        }
                                    }
                                    else
                                    {
										Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Error, "Failed to send file: " + HashedFileName + " to: " + DestPath + " with exception " + XFileManager.ErrorString );
                                        MaxErrors--;
                                        if( MaxErrors <= 0 )
                                        {
                                            break;
                                        }
                                    }
                                }
                                else
                                {
                                    // Recache the file if it doesn't exist
									FileToCache CacheData = new FileToCache( RepositoryPath + "\\" + Row.Path.Trim(), Row.Size, false, false, true );
									CacheSystem.AddCacheOperation( CacheSystem.GetCleanHash( Row.Hash ), CacheData );
									Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Debug, "Recaching: " + Row.Path );
                                }
                            }
                        }

                        Thread.Sleep( 10 );
                    }
                }
                while( NumberFilesToCopy > 0 && MaxErrors > 0 && !TaskCancelled );

                // Mark as 100% complete
                TaskData.Progress = 100;
				Log.WriteLine( "UPDS CopierXbox360", Log.LogType.Info, "Task complete!" );
                TaskExecutor.ConfirmTask( this );

                if( TaskCancelled )
                {
                    SendCancelledEmail( DestinationPath, TargetName, TaskData.AssignedUPDS );
                    XFileManager.DisconnectFromTarget( Target );
                }
                else
                {
                    if( MaxErrors <= 0 )
                    {
                        throw new Exception( "Exceeded max number of SendFile errors" );
                    }
                    string LongTargetName = TargetName + " (" + TaskData.FriendlyName + ")";
                    SendSuccessEmail( DestinationPath, LongTargetName, TaskData.AssignedUPDS, BuildSize );
                    if( TaskData.RunAfterProp )
                    {
                        string Game = UPDS_Service.IUPMS.PlatformBuild_GetProject( TaskData.PlatformBuildID );
                        if( !XFileManager.RebootAndLaunchTarget( Target, DestinationPath, Game, TaskData.BuildConfig, TaskData.CommandLine ) )
                        {
                            SendFailedToLaunchEmail( DestinationPath, TaskData.BuildConfig, TaskData.CommandLine );
                        }
                    }

                    XFileManager.DisconnectFromTarget( Target );
                }
#if !DEBUG
			}
            catch( Exception Ex )
            {
				string Error = Ex.ToString();
				if( !TaskExecutor.TaskError( this, "Unhandled exception" ) )
                {
					Error = "Additional exception in TaskExecutor.TaskError()" + Environment.NewLine + Environment.NewLine + Error;
                }

                XFileManager.DisconnectFromTarget( Target );
                SendFailureEmail( Error );
			}
#endif
		}
    }
}
