/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using System.Threading;
using RemotableType;

namespace UnrealProp
{
    public static class TaskPoller
    {
        static Thread Thread;
        static object SyncObj = new Object();

        static public void Init()
        {
            Thread = new Thread( new ThreadStart( PollerProc ) );
            Thread.Start();
        }

        static public void Release()
        {
            if( Thread != null )
            {
                Thread.Abort();
                Thread = null;
            }
        }

        static void PollerProc()
        {
            Thread.Sleep( 10 * 1000 );

            while( true )
            {
#if !DEBUG
                try
                {
#endif
                    long TaskID = UPDS_Service.IUPMS.Task_GetAssignedTask( UPDS_Service.MachineName );
                    if( TaskID != 0 )
                    {
                        Tasks TasksToExec = UPDS_Service.IUPMS.Task_GetByID( TaskID );
						if( TasksToExec != null )
						{
							Tasks.TasksRow Task = ( Tasks.TasksRow )TasksToExec.Tables[0].Rows[0];
							if( Task.PlatformBuildID > 0 )
							{
								// PC
								if( Task.TargetPlatform == "PC" )
								{
									TaskExecutor.AddTask( new CopierPC( Task ) );
								}

								// X360
								if( Task.TargetPlatform == "Xbox360" )
								{
									TaskExecutor.AddTask( new CopierX360( Task ) );
								}

								UPDS_Service.IUPMS.Task_UpdateStatus( TaskID, TaskStatus.InProgress, 0, "In progress" );
							}
							else
							{
								Log.WriteLine( "UPDS TASK POLLER", Log.LogType.Important, "Build does not exist!" );
								UPDS_Service.IUPMS.Task_UpdateStatus( TaskID, TaskStatus.Canceled, 0, "Canceled" );
							}
						}
					}
                    // 5 seconds between polls
                    Thread.Sleep( 5 * 1000 );
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                    {
                        Log.WriteLine( "UPDS TASK POLLER", Log.LogType.Error, "Unhandled exception: " + Ex.GetType().ToString() + Ex.ToString() );
                    }
                }
#endif
            }
        }
    }
}



