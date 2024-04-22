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
    static class TaskExecutor
    {
        static Object SyncObject = new Object();
        static List<CopierTask> CurrentTasks = new List<CopierTask>();

        static public void AddTask( CopierTask Task )
        {
            lock( SyncObject )
            {
                CurrentTasks.Add( Task );
            }
        }

        static public void StopAllTasks()
        {
            lock( SyncObject )
            {
                foreach( CopierTask Task in CurrentTasks )
                {
                    Task.Abort();
                }

                CurrentTasks.Clear();
            }
        }

        static public bool ConfirmTask( CopierTask Task )
        {
            lock( SyncObject )
            {
                CurrentTasks.Remove( Task );
            }

            try
            {
                UPDS_Service.IUPMS.Task_UpdateStatus( Task.TaskData.ID, TaskStatus.Finished, 100, "OK" );
            }
            catch
            {
                return ( false );
            }

            return ( true );
        }

        static public bool TaskError( CopierTask Task, string Error )
        {
			try
			{
				UPDS_Service.IUPMS.Task_UpdateStatus( Task.TaskData.ID, TaskStatus.Failed, 0, Error );

				lock( SyncObject )
				{
					if( CurrentTasks.Contains( Task ) )
					{
						CurrentTasks.Remove( Task );
					}
				}
            }
            catch
            {
                return ( false );
            }

            return ( true );
        }

        static public int GetTaskProgress( long TaskID )
        {
            lock( SyncObject )
            {
                foreach( CopierTask Task in CurrentTasks )
                {
                    if( Task.TaskData.ID == TaskID )
                    {
                        return( Task.TaskData.Progress );
                    }
                }
            }

            return( 100 );
        }

        static public int TaskCount
        {
            get
            {
                int Count;

                lock( SyncObject )
                {
                    Count = CurrentTasks.Count;
                }

                return( Count );
            }
        }
    }
}
