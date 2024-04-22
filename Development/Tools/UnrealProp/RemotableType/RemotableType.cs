/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Data;
using System.Diagnostics;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;

namespace RemotableType
{
    [Serializable]
    public enum BuildStatus
    {
        None = 0,
        Discovered = 1,
        Analyzing = 2,
        Ready = 3,
        Archived = 4,
        Hidden = 5,
        Deleted = 6
    }

    [Serializable]
    public enum TaskStatus
    {
        None = 0,
        Scheduled = 1,
        InProgress = 2,
        Failed = 3,
        Finished = 4,
        Canceled = 5
    }

    public interface IUPMS_Interface
    {
        // Get a list of available platforms
        Platforms Platform_GetList();

        // Get a list of available projects
        Projects Project_GetList();

        // Get a list of platforms for a given project
        Platforms Platform_GetListForProject( short ProjectID );

        // Get a list of builds for a given project
        PlatformBuilds PlatformBuild_GetListForProject( short ProjectID );

        // Delete all references of a build from the database
        void PlatformBuild_Delete( long PlatformBuildID );

        // Get a list of builds available for the desired platform
        PlatformBuilds PlatformBuild_GetListForBuild( long PlatformBuildID );

        // Get a list of builds based on platform, project and status
        PlatformBuilds PlatformBuild_GetListForProjectPlatformAndStatus( short ProjectID, short PlatformID, short StatusID );

        // Get a list of builds based on platform, project, user and status
        PlatformBuilds PlatformBuild_GetListForProjectPlatformUserAndStatus( short ProjectID, short PlatformID, int UserNameID, short StatusID );

        //
        DescriptionWithID[] PlatformBuild_GetSimpleListForProjectAndPlatform( short ProjectID, short PlatformID );

        // Get a list of files associated with a build
        PlatformBuildFiles PlatformBuild_GetFiles( long PlatformBuildID );

        // Get the number of builds in the database
        int PlatformBuild_GetCount( string Platform, string Project );

        // Get the total number of bytes stored on the master server
		long PlatformBuildFiles_GetTotalSize( string Platform, string Project );

        // Updates the database with a set of files
        void PlatformBuildFiles_Update( PlatformBuildFiles Files );

        // Get the total size of the build in bytes
        long PlatformBuild_GetBuildSize( long PlatformBuildID );

        // Gets the friendly name of a build
        string PlatformBuild_GetTitle( long PlatformBuildID );

        // Get the name of the platform from a build ID
        string PlatformBuild_GetPlatformName( long PlatformBuildID );

        // Get where the repository is for the build
        string PlatformBuild_GetRepositoryPath( long PlatformBuildID );

        // Updates the status of a build e.g. sets state to "analysing" or "deleted" and optionally the name of the build
        void PlatformBuild_ChangeStatus( long PlatformBuildID, BuildStatus StatusID, string BuildName );

        // Updates the Discovery Time of the build - which bumps it's priority in the UPDS caches
        void PlatformBuild_ChangeTime( long PlatformBuildID, DateTime TimeStamp );

        // Returns the percentage complete of the analysing stage
        int PlatformBuild_GetAnalyzingProgress( long PlatformBuildID );

        // Returns whether the file with this hash is still referenced by a build
        bool CachedFileInfo_FileExists( string Hash );

        // Returns the project associated with this build
        string PlatformBuild_GetProject( long PlatformBuildID );

        // Gets the list of statuses for a build
        PlatformBuildStatuses PlatformBuildStatus_GetList();

        // Returns a list of target machines associated with the target
        ClientMachines ClientMachine_GetListForPlatform( short PlatformID );

        // Returns a list of target machines associated with the target and user
        ClientMachines ClientMachine_GetListForPlatformAndUser( short PlatformID, int UserNameID );

        // Returns a list of target machines associated with the target belonging to the defined group
        ClientMachines ClientMachine_GetListForPlatformGroupUser( string Platform, string Group, string Email );

        // Creates or updates a target machine to the current parameters
        long ClientMachine_Update( int ClientMachineID, string Platform, string Name, string Path, string ClientGroup, string Email, bool Reboot );

        // Delete a target machine from the list that can be propped to
        void ClientMachine_Delete( int ClientMachineID );

        // Get a list of client groups that have at least one member for the specified platform
        ClientGroups ClientGroups_GetByPlatform( string Platform );

        //
        string[] DistributionServer_GetConnectedList();

        //
        string[] DistributionServer_GetListFromTasks();

        // Let the master service know a distribution server is available
        void DistributionServer_Register( string MachineName );

        // Let the master service know a distribution service is no longer available
        void DistributionServer_Unregister( string MachineName );

        // Add a new scheduled task
        long Task_AddNew( long PlatformBuildID, DateTime ScheduleTime, int ClientMachineID, string Email, bool RunAfterProp, string BuildConfig, string CommandLine, bool Recurring );

        // Remove a task
        void Task_Delete( long ID );

        // Get a task that is assigned to a UPDS
        long Task_GetAssignedTask( string UPDSName );

        // Updates the status of a task for display in the frontend (returns false if the task has been deleted)
        bool Task_UpdateStatus( long TaskID, TaskStatus StatusID, int Progress, string Error );

        //
        Tasks Task_GetList();

        // Return the details of a single task
        Tasks Task_GetByID( long ID );

        // Gets a list of valid task statuses
        TaskStatuses TaskStatus_GetList();

        // Gets an array of available configs for the given platform
        string[] BuildConfigs_GetForPlatform( string Platform );

        // Gets the ID of a user
        int User_GetID( string Email );

        // Gets a list of users that have triggered tasks
        DescriptionWithID[] User_GetListFromTasks();

        // Gets a list of users that have client machines
        DescriptionWithID[] User_GetListFromTargets();

        // Gets a list of users that have uploaded builds
        DescriptionWithID[] User_GetListFromBuilds();

        // Send an email (normally reporting an error)
        void Utils_SendEmail( int TaskerUserNameID, int TaskeeUserNameID, string Subject, string Message, int Importance );

        // Updates the stats table
        void Utils_UpdateStats( string Project, string Platform, long Bytes, DateTime Scheduled );

        // Gets the stat data
        long Utils_GetStats( string Project, string Platform, DateTime Since, ref long BytesPropped, ref float DataRate );

        // Save the news file on the server
        void News_Add( string News );

        // Get the news string from the server
        string News_Get();
    }

    // universal data format for populate simple web controls (listbox, dropdownlist etc.)
    [Serializable]
    public class DescriptionWithID
    {
        string description;
        long id;

        public string Description
        {
            get { return description; }
            set { description = value; }
        }

        public long ID
        {
            get { return id; }
            set { id = value; }
        }
    }
}

