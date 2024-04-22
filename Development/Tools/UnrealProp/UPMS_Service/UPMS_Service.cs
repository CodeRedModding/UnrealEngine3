/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Configuration;
using System.Data;
using System.Diagnostics;
using System.IO;
using System.Runtime.Remoting;
using System.Runtime.Remoting.Channels;
using System.Runtime.Remoting.Channels.Tcp;
using System.Runtime.InteropServices;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using RemotableType;

namespace UnrealProp
{
    public partial class UPMS_Service : ServiceBase
    {
        // For consistent formatting to the US standard (05/29/2008 11:33:52)
        public const string US_TIME_DATE_STRING_FORMAT = "MM/dd/yyyy HH:mm:ss";
		public const string Spaces = "                                                                ";

        public static IUPMS_Interface IUPMS = null;
		public static List<string> UPDSList = new List<string>();

        private DateTime StartTime = DateTime.Now;
        private DateTime LastStatusTime = DateTime.MinValue;

		public List<string> BuildRepositoryPaths = new List<string>();
		public int NewBuildsTotal;

		public int TotalBuilds;
		public int LocalTotalBuilds;
		public long TotalBuildSize;
		public long LocalTotalBuildSize;

        public UPMS_Service()
        {
            InitializeComponent();
        }

        public void DebugRun( string[] Args )
        {
            OnStart( Args );
            Console.WriteLine( "Press enter to exit" );
            Console.Read();
            OnStop();
        }

		private void RecursiveGetFolderSize( DirectoryInfo Path, bool Root )
		{
			foreach( FileInfo File in Path.GetFiles() )
			{
				LocalTotalBuildSize += File.Length;
			}

			foreach( DirectoryInfo Dir in Path.GetDirectories() )
			{
				RecursiveGetFolderSize( Dir, false );
			}

			if( Root )
			{
				LocalTotalBuilds += Path.GetDirectories().Length;
			}
		}

		private string RecursiveGetFolderInfo( string Path, int Depth, string FolderInfo )
		{
			// Indent the folder name depending on path depth
			string Line = Spaces.Substring( Spaces.Length - ( Depth * 4 ) ) + Path;

			DirectoryInfo DirInfo = new DirectoryInfo( Path );
			if( Depth == 2 )
			{
				LocalTotalBuilds = 0;
				LocalTotalBuildSize = 0;
				
				RecursiveGetFolderSize( DirInfo, true );

				TotalBuilds += LocalTotalBuilds;
				TotalBuildSize += LocalTotalBuildSize;

				double LocalTotalSize = 0.0;
				if( LocalTotalBuildSize > 0 )
				{
					LocalTotalSize = LocalTotalBuildSize / ( 1024.0 * 1024.0 * 1024.0 );
				}

				Line = string.Format( "{0,-48}{1} GB ({2})", Line, LocalTotalSize.ToString( "0.00" ), LocalTotalBuilds.ToString() );
				FolderInfo += Line + Environment.NewLine;
			}
			else
			{
				FolderInfo += Line + Environment.NewLine;
				foreach( DirectoryInfo Dir in DirInfo.GetDirectories() )
				{
					FolderInfo = RecursiveGetFolderInfo( Dir.FullName, Depth + 1, FolderInfo );
				}
			}

			return ( FolderInfo );
		}

		private string GetFolderInfo( List<string> BuildRepositoryPaths )
		{
			string Folderinfo = "";
			TotalBuilds = 0;
			TotalBuildSize = 0;

			foreach( string BuildRespository in BuildRepositoryPaths )
			{
				Folderinfo = RecursiveGetFolderInfo( BuildRespository, 0, Folderinfo );
			}

			double TotalSize = 0.0;
			if( TotalBuildSize > 0 )
			{
				TotalSize = TotalBuildSize / ( 1024.0 * 1024.0 * 1024.0 );
			}
			Folderinfo += Environment.NewLine + "Total size: " + TotalSize.ToString( "0.00" ) + " GB in " + TotalBuilds.ToString() + " builds." + Environment.NewLine;

			return ( Folderinfo );
		}

        private void SendStartEmail()
        {
            string Message = "Started at: " + StartTime.ToString( US_TIME_DATE_STRING_FORMAT ) + Environment.NewLine;
            double MasterCacheSize = IUPMS.PlatformBuildFiles_GetTotalSize( "Any", "Any" ) / ( 1024.0 * 1024.0 * 1024.0 );
			Message += "Master cache size is: " + MasterCacheSize.ToString( "0.00" ) + " GB" + Environment.NewLine;
            Message += Environment.NewLine + "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;
            IUPMS.Utils_SendEmail( -1, -1, "UPMS Started", Message, 1 );
        }

		public void SendStatusEmail()
        {
            string Message;

            if( DateTime.Now - LastStatusTime > new TimeSpan( 3, 0, 0 ) )
            {
                TimeSpan Span = DateTime.Now - StartTime;
                Message = "Uptime:               " + Span.Days.ToString() + " days and " + Span.Hours + " hours." + Environment.NewLine;
				Message += "New builds:           " + NewBuildsTotal.ToString() + Environment.NewLine + Environment.NewLine;

				// Interrogate the database for the status of builds
				Message += "Builds in the database:" + Environment.NewLine;

				Projects ProjectNames = IUPMS.Project_GetList();
				Platforms PlatformNames = IUPMS.Platform_GetList();

				foreach( DataRow ProjectRow in ProjectNames.Tables[0].Rows )
				{
					string Project = ( string )ProjectRow.ItemArray[1];
					bool ProjectAdded = false;
					foreach( DataRow PlatformRow in PlatformNames.Tables[0].Rows )
					{
						string Platform = ( string )PlatformRow.ItemArray[1];
						int BuildCount = IUPMS.PlatformBuild_GetCount( Platform, Project );
						if( BuildCount > 0 )
						{
							if( !ProjectAdded )
							{
								Message += Environment.NewLine + Project + Environment.NewLine;
								ProjectAdded = true;
							}

							long BuildSizeBytes = IUPMS.PlatformBuildFiles_GetTotalSize( Platform, Project );
							double BuildSize = 0.0;
							if( BuildSizeBytes > 0 )
							{
								BuildSize = BuildSizeBytes / ( 1024.0 * 1024.0 * 1024.0 );
							}
							string LineStart = "    " + Platform + ":";
							Message += LineStart + Spaces.Substring( Spaces.Length - ( 24 - LineStart.Length ) );
							Message += BuildSize.ToString( "0.00" ) + " GB (" + BuildCount.ToString() + ")" + Environment.NewLine;
						}
					}
				}

				long MasterCacheSizeBytes = IUPMS.PlatformBuildFiles_GetTotalSize( "Any", "Any" );
				double MasterCacheSize = 0.0;
				if( MasterCacheSizeBytes > 0 )
				{
					MasterCacheSize = MasterCacheSizeBytes / ( 1024.0 * 1024.0 * 1024.0 );
				}
				int MasterBuildCount = IUPMS.PlatformBuild_GetCount( "Any", "Any" );
				Message += Environment.NewLine + "Master cache size is: " + MasterCacheSize.ToString( "0.00" ) + " GB in " + MasterBuildCount.ToString() + " builds." + Environment.NewLine + Environment.NewLine;

				// Interrogate the filesytem for the status of the builds
				Message += "Builds in the filesystem:" + Environment.NewLine;

				string FolderInfo = GetFolderInfo( BuildRepositoryPaths );
				Message += Environment.NewLine + FolderInfo;

                Message += Environment.NewLine + "Cheers" + Environment.NewLine + "UnrealProp" + Environment.NewLine;
                IUPMS.Utils_SendEmail( -1, -1, "UPMS Status", Message, 1 );

                NewBuildsTotal = 0;
                LastStatusTime = DateTime.Now;
            }
        }

		protected void ExtractBuildRepositories( string BuildRepositoryPath )
		{
			string[] Repositories = BuildRepositoryPath.Split( ";".ToCharArray() );
			foreach( string Repository in Repositories )
			{
				string CleanRepository = Repository.Replace( '/', '\\' ).TrimEnd( "/".ToCharArray() );

				if( Directory.Exists( CleanRepository ) )
				{
					BuildRepositoryPaths.Add( CleanRepository );
					Log.WriteLine( "UPMS", Log.LogType.Info, " ... added path: " + CleanRepository );
				}
			}

			if( BuildRepositoryPaths.Count == 0 )
			{
				Log.WriteLine( "UPMS", Log.LogType.Error, "No valid repositories found!" );
			}
		}

        protected override void OnStart( string[] args )
        {
            // Configure the remoting service
            TcpChannel ServerChannel = new TcpChannel( 9090 );
            ChannelServices.RegisterChannel( ServerChannel, false );

            RemotingConfiguration.RegisterWellKnownServiceType( typeof( UPMS_Implementation ), "UPMS", WellKnownObjectMode.Singleton );

#if DEBUG
            IUPMS = ( IUPMS_Interface )Activator.GetObject( typeof( IUPMS_Interface ), "tcp://localhost:9090/UPMS" );
#else
            IUPMS = ( IUPMS_Interface )Activator.GetObject( typeof( IUPMS_Interface ), Properties.Settings.Default.RemotableHost );
#endif
            
            // Grab the location of the builds from the config
			ExtractBuildRepositories( Properties.Settings.Default.BuildRepositoryPath );
			BuildDiscoverer.Init( this );

            // Initialise and start the worker threads
#if !DEBUG
            BuildAnalyzer.Init();
			BuildValidator.Init();
#endif
			TaskScheduler.Init();

            StartTime = DateTime.Now;
            SendStartEmail();

            Log.WriteLine( "UPMS", Log.LogType.Important, "Service has been successfully started!" );
        }

        protected override void OnStop()
        {
            BuildDiscoverer.Stop();
#if !DEBUG
			BuildValidator.Release();
            BuildAnalyzer.Release();
#endif
			TaskScheduler.Release();

            Log.WriteLine( "UPMS", Log.LogType.Important, "Service has been stopped!" );
        }
    }

    public class UPMS_Implementation : MarshalByRefObject, IUPMS_Interface
    {
        // platforms
        public Platforms Platform_GetList()
        {
            return DataHelper.Platform_GetList();
        }

        public Platforms Platform_GetListForProject( short ProjectID )
        {
            return DataHelper.Platform_GetListForProject( ProjectID );
        }

        // projects
        public Projects Project_GetList()
        {
            return DataHelper.Project_GetList();
        }

        // platform build
        public PlatformBuilds PlatformBuild_GetListForProject( short ProjectID )
        {
            return DataHelper.PlatformBuild_GetListForProject( ProjectID );
        }

        public void PlatformBuild_Delete( long PlatformBuildID )
        {
            DataHelper.PlatformBuild_Delete( PlatformBuildID );
        }

        public PlatformBuilds PlatformBuild_GetListForBuild( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetListForBuild( PlatformBuildID );
        }

        public PlatformBuilds PlatformBuild_GetListForProjectPlatformAndStatus( short ProjectID, short PlatformID, short StatusID )
        {
            return DataHelper.PlatformBuild_GetListForProjectPlatformAndStatus( ProjectID, PlatformID, StatusID );
        }

        public PlatformBuilds PlatformBuild_GetListForProjectPlatformUserAndStatus( short ProjectID, short PlatformID, int UserNameID, short StatusID )
        {
            return DataHelper.PlatformBuild_GetListForProjectPlatformUserAndStatus( ProjectID, PlatformID, UserNameID, StatusID );
        }

        public PlatformBuildFiles PlatformBuild_GetFiles( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetFiles( PlatformBuildID );
        }

        public int PlatformBuild_GetCount( string Platform, string Project )
        {
            return DataHelper.PlatformBuild_GetCount( Platform, Project );
        }

		public long PlatformBuildFiles_GetTotalSize( string Platform, string Project )
        {
            return DataHelper.PlatformBuildFiles_GetTotalSize( Platform, Project );
        }

        public void PlatformBuildFiles_Update( PlatformBuildFiles Files )
        {
            DataHelper.PlatformBuildFiles_Update( Files );
        }

        public long PlatformBuild_GetBuildSize( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetBuildSize( PlatformBuildID );
        }

        public string PlatformBuild_GetTitle( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetTitle( PlatformBuildID );
        }

        public string PlatformBuild_GetPlatformName( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetPlatformName( PlatformBuildID );
        }

        public string PlatformBuild_GetRepositoryPath( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetRepositoryPath( PlatformBuildID );
        }

        public void PlatformBuild_ChangeStatus( long PlatformBuildID, BuildStatus StatusID, string BuildName )
        {
            DataHelper.PlatformBuild_ChangeStatus( PlatformBuildID, StatusID, BuildName );
        }

        public void PlatformBuild_ChangeTime( long PlatformBuildID, DateTime TimeStamp )
        {
            DataHelper.PlatformBuild_ChangeTime( PlatformBuildID, TimeStamp );
        }

        public int PlatformBuild_GetAnalyzingProgress( long PlatformBuildID )
        {
            return BuildAnalyzer.PlatformBuild_GetAnalyzingProgress( PlatformBuildID );
        }

        public bool CachedFileInfo_FileExists( string Hash )
        {
            return DataHelper.CachedFileInfo_FileExists( Hash );
        }

        public string PlatformBuild_GetProject( long PlatformBuildID )
        {
            return DataHelper.PlatformBuild_GetProject( PlatformBuildID );
        }

        public DescriptionWithID[] PlatformBuild_GetSimpleListForProjectAndPlatform( short ProjectID, short PlatformID )
        {
            return DataHelper.PlatformBuild_GetSimpleListForProjectAndPlatform( ProjectID, PlatformID );
        }

        public PlatformBuildStatuses PlatformBuildStatus_GetList()
        {
            return DataHelper.PlatformBuildStatus_GetList();
        }

        // clients
        public ClientMachines ClientMachine_GetListForPlatform( short PlatformID )
        {
            return( DataHelper.ClientMachine_GetListForPlatform( PlatformID ) );
        }

        public ClientMachines ClientMachine_GetListForPlatformAndUser( short PlatformID, int UserNameID )
        {
            return ( DataHelper.ClientMachine_GetListForPlatformAndUser( PlatformID, UserNameID ) );
        }

        public ClientMachines ClientMachine_GetListForPlatformGroupUser( string Platform, string Group, string Email )
        {
            return( DataHelper.ClientMachine_GetListForPlatformGroupUser( Platform, Group, Email ) );
        }

        public long ClientMachine_Update( int ClientMachineID, string Platform, string Name, string Path, string ClientGroup, string Email, bool Reboot )
        {
            return DataHelper.ClientMachine_Update( ClientMachineID, Platform, Name, Path, ClientGroup, Email, Reboot );
        }

        public void ClientMachine_Delete( int ClientMachineID )
        {
            DataHelper.ClientMachine_Delete( ClientMachineID );
        }

        public ClientGroups ClientGroups_GetByPlatform( string Platform )
        {
            return( DataHelper.ClientGroups_GetByPlatform( Platform ) );
        }
        
        // distribution servers
        public string[] DistributionServer_GetConnectedList()
        {
            return ( UPMS_Service.UPDSList.ToArray() );
        }

        public string[] DistributionServer_GetListFromTasks()
        {
            return DataHelper.DistributionServer_GetListFromTasks();
        }

        public void DistributionServer_Register( string MachineName )
        {
            UPMS_Service.UPDSList.Add( MachineName );
        }

        public void DistributionServer_Unregister( string MachineName )
        {
            UPMS_Service.UPDSList.Remove( MachineName );
        }

        // tasks
        public long Task_AddNew( long PlatformBuildID, DateTime ScheduleTime, int ClientMachineID, string Email, bool RunAfterProp, string BuildConfig, string CommandLine, bool Recurring )
        {
            return DataHelper.Task_AddNew( PlatformBuildID, ScheduleTime, ClientMachineID, Email, RunAfterProp, BuildConfig, CommandLine, Recurring );
        }

        public void Task_Delete( long ID )
        {
            DataHelper.Task_Delete( ID );
        }

        // Get a task that is assigned to a UPDS
        public long Task_GetAssignedTask( string UPDSName )
        {
            return ( DataHelper.Task_GetAssignedTask( UPDSName ) );
        }

        public bool Task_UpdateStatus( long TaskID, TaskStatus StatusID, int Progress, string Error )
        {
            bool ValidTask = DataHelper.Task_UpdateStatus( TaskID, StatusID, Progress, Error );
            TaskScheduler.OnUpdateTaskStatus( TaskID, ( short )StatusID );
            return ( ValidTask );
        }

        public Tasks Task_GetList()
        {
            return DataHelper.Task_GetList();
        }

        public Tasks Task_GetByID( long ID )
        {
            return DataHelper.Task_GetByID( ID );
        }

        public TaskStatuses TaskStatus_GetList()
        {
            return DataHelper.TaskStatus_GetList();
        }

        public string[] BuildConfigs_GetForPlatform( string Platform )
        {
            return ( DataHelper.BuildConfigs_GetForPlatform( Platform ) );
        }

        public int User_GetID( string Email )
        {
            return( DataHelper.User_GetID( Email ) );
        }

        public DescriptionWithID[] User_GetListFromTasks()
        {
            return DataHelper.User_GetListFromTasks();
        }

        public DescriptionWithID[] User_GetListFromTargets()
        {
            return DataHelper.User_GetListFromTargets();
        }

        public DescriptionWithID[] User_GetListFromBuilds()
        {
            return DataHelper.User_GetListFromBuilds();
        }

        public void Utils_SendEmail( int TaskerUserNameID, int TaskeeUserNameID, string Subject, string Message, int Importance )
        {
            Mailer.SendEmail( TaskerUserNameID, TaskeeUserNameID, Subject, Message, Importance );
        }

        public void Utils_UpdateStats( string Project, string Platform, long Bytes, DateTime Scheduled )
        {
            DataHelper.Utils_UpdateStats( Project, Platform, Bytes, Scheduled );
        }

        public long Utils_GetStats( string Project, string Platform, DateTime Since, ref long BytesPropped, ref float DataRate )
        {
            return ( DataHelper.Utils_GetStats( Project, Platform, Since, ref BytesPropped, ref DataRate ) );
        }

        public void News_Add( string News )
        {
            DataHelper.News_Add( News );
        }

        public string News_Get()
        {
            return ( DataHelper.News_Get() );
        }
    }

    public class Log
    {
        public enum LogType
        {
            Debug = 0,
			Info = 1,
			Important = 2,
            Error = 3,
        }

        private static StreamWriter LogFile = null;

        public static void WriteLine( string Label, LogType Type, string Message )
        {
			// Suppress debug information
			if( Type < LogType.Info )
			{
				return;
			}

            string Format = DateTime.Now.ToString( UPMS_Service.US_TIME_DATE_STRING_FORMAT ) + " [" + Label + "] ";
            if( Type == LogType.Error )
            {
                Format += "ERROR!!! ";
            }

			string RawString = Format + Message;

            Trace.WriteLine( RawString );
            Console.WriteLine( RawString );

            if( LogFile == null )
            {
                string LogFilePath = "UPMS_" + DateTime.Now.ToString( "yyyy_MM_dd-HH_mm_ss" ) + ".txt";
                LogFile = new StreamWriter( LogFilePath, false, Encoding.ASCII );
            }

            LogFile.WriteLine( RawString );

            if( Type == LogType.Error )
            {
                UPMS_Service.IUPMS.Utils_SendEmail( -1, -1, "UPMS reported error!", RawString, 2 );
                LogFile.Flush();
                // Sleep 5 minutes after any error
                Thread.Sleep( 5 * 60 * 1000 );
            }
        }
    }
}


