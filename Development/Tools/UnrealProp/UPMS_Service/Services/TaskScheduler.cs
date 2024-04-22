/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Collections;
using System.Text;
using System.Data;
using System.Threading;
using RemotableType;
using System.Configuration;

namespace UnrealProp
{
    // working thread for executing scheduled tasks and monitoring tesk's execution on UPDSs
    public static class TaskScheduler
    {
        static object SyncWorkingTasks = new object();
        static Tasks.TasksDataTable WorkingTasks = new Tasks.TasksDataTable();
        static Random Rnd = new Random( 3400 );
        static Thread Thread, ThreadGuard;

        static public void Init()
        {
            Thread = new Thread( new ThreadStart( SchedulerProc ) );
            Thread.Start();

            ThreadGuard = new Thread( new ThreadStart( TaskGuardProc ) );
            ThreadGuard.Start();

            Log.WriteLine( "UPMS TASK SCHEDULER", Log.LogType.Important, "Initialised!" );
        }

        static public void Release()
        {
            Thread.Abort();
            Thread = null;

            ThreadGuard.Abort();
            ThreadGuard = null;
        }

        static private int GetActiveTasks( string UPDSName )
        {
            int ActiveTasks = 0;

            foreach( Tasks.TasksRow TaskInProgress in WorkingTasks.Rows )
            {
                if( TaskInProgress.AssignedUPDS == UPDSName )
                {
                    ActiveTasks++;
                }
            }

            Log.WriteLine( "UPMS TASK SCHEDULER", Log.LogType.Info, UPDSName + " has " + ActiveTasks.ToString() + " active tasks" );

            return ( ActiveTasks );
        }

        // thread proc
        static void SchedulerProc()
        {
            Thread.Sleep( 15000 );

            int MaxTasksOnUPDS = Properties.Settings.Default.MaxTasksOnSingleUPDS;

#if !DEBUG
            try
            {
#endif
                // mark all "in progress" tasks as "failed"
                DataHelper.ExecuteNonQuery( "UPDATE [Tasks] SET StatusID = 3, ErrorID = 2 WHERE ( StatusID = 2 )" );
#if !DEBUG
            }
            catch( Exception Ex )
            {
                Log.WriteLine( "UPMS TASK SCHEDULER INIT", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
            }
#endif

            while( true )
            {
#if !DEBUG
                try
                {
#endif
                    // get task list with status=scheduled
                    Tasks Tasks = DataHelper.Task_GetScheduledList();
                    foreach( Tasks.TasksRow Task in Tasks.Tables[0].Rows )
                    {
						BuildStatus Status = DataHelper.PlatformBuild_GetStatus( Task.PlatformBuildID );
						if( Status != BuildStatus.Ready && Status != BuildStatus.Archived )
                        {
                            DataHelper.Task_UpdateStatus( Task.ID, TaskStatus.Failed, 0, "Build is not ready! Maybe it is hidden or deleted?" );
                            continue;
                        }

                        // is it time for this task?
                        if( DateTime.Now > Task.ScheduleTime )
                        {
                            // is there any UPDS to execute this task?
                            if( UPMS_Service.UPDSList.Count > 0 )
                            {
                                string BestUPDSName = "";
                                int MinRunningTasks = 9999;

                                // check that this target machine is not in use
                                lock( SyncWorkingTasks )
                                {
                                    bool InUse = false;
                                    foreach( Tasks.TasksRow TaskInProgress in WorkingTasks.Rows )
                                    {
                                        if( TaskInProgress.ClientMachineID == Task.ClientMachineID )
                                        {
                                            InUse = true;
                                            break;
                                        }
                                    }

                                    if( InUse )
                                    {
                                        Log.WriteLine( "UPMS TASK SCHEDULER", Log.LogType.Info, "Target machine for task: " + Task.ID.ToString() + " is currently in use by another task!" );
                                        continue;
                                    }
                                }

                                // Remember the original UPDS name for recurring tasks
                                string AssignedUPDS = "";

                                // Choose the best (least used) UPDS for this task
                                foreach( string UPDSName in UPMS_Service.UPDSList )
                                {
                                    // Get the number of active tasks on this UPDS
                                    int RunningTasks = GetActiveTasks( UPDSName );

                                    AssignedUPDS = "";
                                    if( Task.AssignedUPDS != "None" )
                                    {
                                        AssignedUPDS = Task.AssignedUPDS.Trim();
                                    }

                                    if( AssignedUPDS.Length > 0 && UPDSName != AssignedUPDS )
                                    {
                                        continue;
                                    }

                                    // Remember this UPDS if it has the fewest active tasks
                                    if( UPDSName == AssignedUPDS || RunningTasks < MinRunningTasks )
                                    {
                                        BestUPDSName = UPDSName;
                                        MinRunningTasks = RunningTasks;

                                        if( UPDSName == AssignedUPDS )
                                        {
                                            break;
                                        }
                                    }
                                }

                                // Send task to chosen UPDS if there is a slot available
                                if( BestUPDSName.Length > 0 && MinRunningTasks < MaxTasksOnUPDS )
                                {
                                    Task.StatusID = ( short )TaskStatus.InProgress;
                                    Task.AssignedUPDS = BestUPDSName;

                                    DataHelper.Task_AssignToUPDS( Task.ID, Task.AssignedUPDS );

                                    // Add to active tasks
                                    lock( SyncWorkingTasks )
                                    {
                                        WorkingTasks.Rows.Add( Task.ItemArray );
                                    }

                                    // Repeat the same task 1 day in the future
                                    if( Task.Recurring )
                                    {
                                        TimeSpan OneDay = new TimeSpan( 1, 0, 0, 0 );
                                        DateTime NewTime = Task.ScheduleTime + OneDay;
                                        DataHelper.Task_AddNew( Task.PlatformBuildID, NewTime, Task.ClientMachineID, Task.Email, Task.RunAfterProp, Task.BuildConfig, Task.CommandLine, Task.Recurring );
                                    }
                                }
                                else
                                {
                                    if( BestUPDSName.Length == 0 )
                                    {
                                        BestUPDSName = AssignedUPDS;
                                    }

                                    Log.WriteLine( "UPMS TASK SCHEDULER", Log.LogType.Info, BestUPDSName + " is too busy to execute task ID: " + Task.ID.ToString() + "!" );
                                }
                            }
                            else
                            {
                                Log.WriteLine( "UPMS TASK SCHEDULER", Log.LogType.Info, "Cannot find any UPDS to execute task ID: " + Task.ID.ToString() + "!" );
                            }
                        }
                    }

                    // 25-35 sec interval
                    Thread.Sleep( Rnd.Next( 25000, 35000 ) ); 
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                    {
                        Log.WriteLine( "UPMS TASK SCHEDULER", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                    }
                }
#endif
            }
        }

        // thread proc for checking active task status
        static void TaskGuardProc()
        {
            while( true )
            {
#if !DEBUG
                try
                {
#endif
                    Tasks.TasksDataTable Tasks = new Tasks.TasksDataTable();

                    lock( SyncWorkingTasks )
                    {
                        // remove finished/failed tasks
                        ArrayList Finished = new ArrayList();
                        foreach( Tasks.TasksRow Task in WorkingTasks.Rows )
                        {
                            if( Task.StatusID != ( short )TaskStatus.InProgress )
                            {
                                Finished.Add( Task );
                            }
                        }

                        foreach( DataRow Task in Finished )
                        {
                            WorkingTasks.Rows.Remove( Task );
                        }

                        // clone syncWorkingTasks to avoid multithreaded locking 
                        Tasks.Merge( WorkingTasks.Copy() );
                    }

                    Thread.Sleep( Rnd.Next( 10000, 15000 ) );
#if !DEBUG
                }
                catch( Exception Ex )
                {
                    if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                    {
                        Log.WriteLine( "UPMS TASK GUARD", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                    }
                }
#endif
            }
        }

        // error handling
        static public void OnUPDSDisconnected( string UPDSName )
        {
#if !DEBUG
            try
            {
#endif
                lock( SyncWorkingTasks )
                {
                    foreach( Tasks.TasksRow WorkingTask in WorkingTasks.Rows )
                    {
                        if( WorkingTask.AssignedUPDS.Trim() == UPDSName )
                        {
                            WorkingTask.StatusID = ( short )TaskStatus.Failed;
                            DataHelper.Task_UpdateStatus( WorkingTask.ID, ( TaskStatus )WorkingTask.StatusID, 0, "UPMS has lost connection to UPDS!" );
                        }
                    }
                }
#if !DEBUG
            }
            catch( Exception Ex )
            {
                if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                {
                    Log.WriteLine( "UPMS UPDS DISCONNECTED", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                }
            }
#endif
        }

        static public void OnUpdateTaskStatus( long TaskID, short Status )
        {
#if !DEBUG
            try
            {
#endif
                lock( SyncWorkingTasks )
                {
                    Tasks.TasksRow TaskRow = ( Tasks.TasksRow )WorkingTasks.Rows.Find( TaskID );
                    if( TaskRow != null )
                    {
                        TaskRow.StatusID = Status;
                    }
                }
#if !DEBUG
            }
            catch( Exception Ex )
            {
                if( Ex.GetType() != typeof( System.Threading.ThreadAbortException ) )
                {
                    Log.WriteLine( "UPMS TASK STATUS UPDATE", Log.LogType.Error, "Unhandled exception: " + Ex.ToString() );
                }
            }
#endif
        }
    }
}
