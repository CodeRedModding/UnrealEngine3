/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using System.IO;
using System.Collections;
using RemotableType;

namespace UnrealProp
{
    public class CopierPC : CopierTask
    {
        private PCFileManager PCFileManager = new PCFileManager();

        public CopierPC( Tasks.TasksRow Task )
        {
            TaskData = Task;
            Worker = new Thread( new ThreadStart( WorkerProc ) );
            Worker.Start();
        }

        override public void WorkerProc()
        {
            IPCTarget Target = null;
#if !DEBUG
            try
            {
#endif
                short LastProgress = -1;
                long BuildSize = 0;
                int MaxErrors = 5;
                bool TaskCancelled = false;

                Hashtable CreatedDirectories = new Hashtable();

                string DestinationName = TaskData.Path;
                string TargetName = DestinationName.Substring( 2, DestinationName.IndexOf( '\\', 2 ) - 2 );
                string DestinationPath = DestinationName + "\\" + UPDS_Service.IUPMS.PlatformBuild_GetTitle( TaskData.PlatformBuildID ) + "\\";
                string RepositoryPath = UPDS_Service.IUPMS.PlatformBuild_GetRepositoryPath( TaskData.PlatformBuildID );
                FilesToCopy = UPDS_Service.IUPMS.PlatformBuild_GetFiles( TaskData.PlatformBuildID );
                BuildSize = UPDS_Service.IUPMS.PlatformBuild_GetBuildSize( TaskData.PlatformBuildID );

                // cache files
                CacheSystem.PrecacheBuild( FilesToCopy, RepositoryPath, false );

                Target = PCFileManager.ConnectToTarget( TargetName );
                if( Target == null )
                {
					SendUserFailureEmail( "Failed to connect to PC: " + TargetName + " - is it turned on?" );
					TaskExecutor.TaskError( this, "Connection failure" );
					return;
                }

                Log.WriteLine( "UPDS CopierPC", Log.LogType.Debug, "After Connect to PC..." );

                // clean the target folder
                PCFileManager.DeleteDirectory( Target, DestinationPath );

                Log.WriteLine( "UPDS CopierPC", Log.LogType.Debug, "After delete directory..." );

                // copy file
                int NumberFilesToCopy = FilesToCopy.Tables[0].Rows.Count;
                long NumberBytesCopied = 0;
                do
                {
                    foreach( PlatformBuildFiles.PlatformBuildFilesRow File in FilesToCopy.Tables[0].Rows )
                    {
                        // has not yet copied
                        if( File.RowState != System.Data.DataRowState.Modified )
                        {
                            if( CacheSystem.IsFileAvailable( File, RepositoryPath ) )
                            {
                                NumberBytesCopied += File.Size;
                                TaskData.Progress = ( short )( ( double )( NumberBytesCopied * 100.0 ) / ( double )BuildSize );

                                string HashedFileName = CacheSystem.GetCachedFilePath( File.Hash.Trim() );
                                string DestPath = DestinationPath + File.Path.Trim();
                                string DestDir = Path.GetDirectoryName( DestPath );

                                if( !CreatedDirectories.ContainsKey( DestDir ) )
                                {
                                    PCFileManager.CreateDirectory( Target, DestDir );
                                    CreatedDirectories.Add( DestDir, true );
                                }

                                if( PCFileManager.SendFile( Target, HashedFileName, DestPath ) )
                                {
                                    File.SetModified();
                                    NumberFilesToCopy--;

                                    Log.WriteLine( "UPDS CopierPC", Log.LogType.Debug, TaskData.Progress + "%: " + File.Path );

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
                                    Log.WriteLine( "UPDS CopierPC", Log.LogType.Error, "Failed to send file: " + HashedFileName + " to: " + DestPath + " with exception " + PCFileManager.ErrorString );
                                    
                                    // Exit if we hit too many errors
                                    MaxErrors--;
                                    if( MaxErrors <= 0 )
                                    {
                                        break;
                                    }
                                }
                            }

                            Thread.Sleep( 10 );
                        }
                    }
                }
                while( NumberFilesToCopy > 0 && MaxErrors > 0 && !TaskCancelled );

                TaskData.Progress = 100;
                Log.WriteLine( "UPDS CopierPC", Log.LogType.Info, "Task complete!" );
                TaskExecutor.ConfirmTask( this );

                if( TaskCancelled )
                {
                    SendCancelledEmail( DestinationPath, TargetName, TaskData.AssignedUPDS );
                    return;
                }

                if( MaxErrors <= 0 )
                {
                    throw new System.Exception( "Exceeded max number of errors" );
                }

                // Send success email
                string LongTargetName = TargetName + " (" + TaskData.FriendlyName + ")";
                SendSuccessEmail( DestinationPath, LongTargetName, TaskData.AssignedUPDS, BuildSize );

                // Run the propped task if requested (does not work - included here for completeness)
                if( TaskData.RunAfterProp )
                {
                    string Game = UPDS_Service.IUPMS.PlatformBuild_GetProject( TaskData.PlatformBuildID );

                    if( !PCFileManager.RebootAndLaunchTarget( Target, DestinationPath, Game, TaskData.BuildConfig, TaskData.CommandLine ) )
                    {
                        SendFailedToLaunchEmail( DestinationPath, TaskData.BuildConfig, TaskData.CommandLine );
                    }
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

				SendFailureEmail( Error );
			}
#endif
        }
    }
}
