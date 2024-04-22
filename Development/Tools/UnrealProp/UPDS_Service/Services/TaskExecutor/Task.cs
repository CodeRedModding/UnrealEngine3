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
    // thread safety
    public abstract class Task
    {
        protected Object SyncObject = new Object();
        protected Thread Worker;
        abstract public void WorkerProc();
    }

    public abstract class CopierTask : Task
    {
        public Tasks.TasksRow TaskData;
        protected PlatformBuildFiles FilesToCopy;

        public void Abort()
        {
            if( Worker != null )
            {
                Worker.Abort();
            }
        }

        public void SendSuccessEmail( string DestinationPath, string TargetName, string UPDS, long BuildSize )
        {
            string Body = "Build:    " + DestinationPath + Environment.NewLine;
            Body += "Sent to:  " + TargetName + Environment.NewLine;
            Body += "Using:    " + UPDS + Environment.NewLine;
            Body += Environment.NewLine;
            Body += "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;

            UPDS_Service.IUPMS.Utils_SendEmail( TaskData.TaskerUserNameID, TaskData.TaskeeUserNameID, "UPDS Successful prop!", Body, 2 );

            // Update stats
            UPDS_Service.IUPMS.Utils_UpdateStats( TaskData.Project, TaskData.TargetPlatform, BuildSize, TaskData.ScheduleTime );
        }

        public void SendCancelledEmail( string DestinationPath, string TargetName, string UPDS )
        {
            string Body = "Build:     " + DestinationPath + Environment.NewLine;
            Body += "Sent to:  " + TargetName + Environment.NewLine;
            Body += "Using:    " + UPDS + Environment.NewLine;
            Body += Environment.NewLine;
            Body += "*** CANCELLED ***" + Environment.NewLine;
            Body += Environment.NewLine;
            Body += "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;

            UPDS_Service.IUPMS.Utils_SendEmail( TaskData.TaskerUserNameID, TaskData.TaskeeUserNameID, "UPDS Cancelled prop!", Body, 2 );
        }

        public void SendFailedToLaunchEmail( string DestinationPath, string Executable, string CommandLine )
        {
            string Body = "Build:                " + DestinationPath + Environment.NewLine;
            Body += Environment.NewLine;
            Body += "*** FAILED TO LAUNCH GAME ***" + Environment.NewLine; 
            Body += Environment.NewLine;
            Body += "Build Configuration:  " + Executable;
            Body += "Command Line:         " + CommandLine;
            Body += Environment.NewLine;
            Body += "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;
        }

		public void SendUserFailureEmail( string Error )
		{
			string Body = "'" + TaskData.AssignedUPDS + "' failed to send build with error:" + Environment.NewLine + Environment.NewLine;
			Body += Error + Environment.NewLine;
			Body += Environment.NewLine;
			Body += "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;

			UPDS_Service.IUPMS.Utils_SendEmail( TaskData.TaskerUserNameID, TaskData.TaskeeUserNameID, "UPDS Target problem!", Body, 2 );
		}

        public void SendFailureEmail( string Error )
        {
            string Body = "'" + TaskData.AssignedUPDS + "' failed to send build with error:" + Environment.NewLine + Environment.NewLine;
            Body += Error + Environment.NewLine;
            Body += Environment.NewLine;
            Body += "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;

            UPDS_Service.IUPMS.Utils_SendEmail( TaskData.TaskerUserNameID, TaskData.TaskeeUserNameID, "UPDS ERROR!", Body, 2 );
        }
    }
}
