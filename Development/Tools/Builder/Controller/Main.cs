// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Collections.Specialized;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Management;
using System.ServiceProcess;
using System.Text;
using System.Threading;
using System.Windows;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;

using Controller.Models;

namespace Controller
{
	public enum MODES
	{
		Init,
		Monitor,
		Wait,
		WaitForFTP,
		WaitForJobs,
		WaitTargetState,
		Finalise,
		Exit,
	}

	public enum COMMANDS
	{
		None,
		Error,
		SilentlyFail,
		NoScript,
		IllegalCommand,
		TimedOut,
		WaitTimedOut,
		WaitJobsTimedOut,
		FailedJobs,
		Crashed,
		CriticalError,
		NetworkVanished,
		Process,
		Config,
		SendEmail,
		CookingSuccess,
		CookerSyncSuccess,
		SCC_FileLocked,
		SCC_CheckConsistency,
		SCC_GetLatestChangelist,
		SCC_Sync,
		SCC_SyncToHead,
		SCC_Resolve,
		SCC_ArtistSync,
		SCC_GetChanges,
		SCC_SyncSingleChangeList,
		SCC_Checkout,
		SCC_OpenForDelete,
		SCC_GetClientRoot,
		SCC_CheckoutManifest,
		SCC_CheckoutGame,
		SCC_CheckoutGFWLGame,
		SCC_CheckoutShader,
		SCC_CheckoutDialog,
		SCC_CheckoutFonts,
		SCC_CheckoutLocPackage,
		SCC_CheckoutGDF,
		SCC_CheckoutCat,
		SCC_CheckoutGADCheckpoint,
		SCC_CheckoutLayout,
		SCC_CheckoutHashes,
		SCC_CheckoutDLC,
		SCC_CheckoutAFTScreenshots,
		SCC_CheckoutConnCache,
		SCC_SignUnsubmitted,
		SCC_Submit,
		SCC_CreateNewLabel,
		SCC_DeleteLabel,
		SCC_LabelCreateNew,
		SCC_LabelUpdateDescription,
		SCC_GetLabelInfo,
		SCC_LabelDelete,
		SCC_OpenedFiles,
		SCC_Revert,
		SCC_RevertFileSpec,
		SCC_Tag,
		SCC_TagMessage,
		SCC_CreatePendingChangelist,
		SCC_GetDepotFileSpec,
		SCC_GetClientInfo,
		SCC_GetUserInfo,
		SCC_GetChangelists,
		SCC_GetHaveRevision,
		SCC_GetIncorrectCheckedOutFiles,
		SCC_GetNextChangelist,
		GetChangelist,
		WaitForJobs,
		GetAllTargets,
		AddUnrealGameJob,
		AddUnrealAgnosticJob,
		AddUnrealAgnosticJobNoEditor,
		AddUnrealFullGameJob,
		AddUnrealGFWLGameJob,
		AddUnrealGFWLFullGameJob,
		AddConformJob,
		SetDependency,
		Clean,
		CleanMac,
		CleanMacs,
		BuildUBT9,
		BuildUBT10,
		RemoteBuildUBT10,
		BuildUAT10,
		MSBuild,
		MS9Build,
		MS10Build,
		MSVCClean,
		MSVCBuild,
		MSVCDeploy,
		MSVC9Clean,
		MSVC9Build,
		MSVC9Deploy,
		MSVC10Clean,
		MSVC10Build,
		MSVC10Deploy,
		UnrealBuild,
		RemoteUnrealBuild,
		UnrealBuildTool,
		UnrealAutomationTool,
		GenerateManifest,
		ShaderClean,
		ShaderBuild,
		CookShaderBuild,
		ConnectionBuild,
		BuildScript,
		BuildScriptNoClean,
        Commandlet,
		iPhonePackage,
		AndroidPackage,
		iPhoneSetDeveloperSigningIdentity,
		iPhoneSetDistributionSigningIdentity,
		iPhoneCompileServer,
		iPhoneSigningServer,
		PreHeatMapOven,
		PreHeatDLC,
		PackageTU,
		PackageDLC,
		CookMaps,
		CookIniMaps,
		CookSounds,
		CreateHashes,
		UpdateDDC,
		Wrangle,
		Publish,
		PublishTagset,
		PublishLanguage,
		PublishLayout,
		PublishLayoutLanguage,
		PublishDLC,
		PublishTU,
		PublishFiles,
		PublishRawFiles,
        PublishFolder,
		CookerSyncReplacement,
        RunBatchFile,
		GenerateTOC,
		Deploy,
		MakeISO,
		MakeMD5,
		SetSteamID,
		CopyFolder,
		MoveFolder,
		GetPublishedData,
		TestMap,
		LaunchGame,
		LaunchConsole,
		WaitTargetState,
		RetrieveScreenShots,
		ProcessScreenShots,
		SteamPipe,
		SteamMakeVersion,
		UpdateSteamServer,
		StartSteamServer,
		StopSteamServer,
		RestartSteamServer,
		UnSetup,
		CreateDVDLayout,
		Conform,
		PatchScript,
		CheckpointGameAssetDatabase,
		UpdateGameAssetDatabase,
		TagReferencedAssets,
		TagDVDAssets,
		AuditContent,
		FindStaticMeshCanBecomeDynamic,
		FixupRedirects,
		ResaveDeprecatedPackages,
		AnalyzeReferencedContent,
		MineCookedPackages,
		ContentComparison,
		DumpMapSummary,
		Finished,
		Trigger,
		CleanTrigger,
		AutoTrigger,
		SQLExecInt,
		SQLExecDouble,
		ValidateInstall,
		ExtractSHAs,
		CheckSHAs,
		BumpAgentVersion,
		BumpEngineVersion,
		GetEngineVersion,
		UpdateGDFVersion,
		MakeGFWLCat,
		ZDPP,
		SteamDRM,
		FixupSteamDRM,
		SaveDefines,
		UpdateSourceServer,
		UpdateSourceServerSetP4PORT,
		UpdateSourceServerSetP4CLIENT,
		UpdateSourceServerSetP4USER,
		UpdateSourceServerSetP4PASSWD,
		UpdateSourceServerP4Set,
		UpdateSourceServerP4Info,
		UpdateSourceServerResults,
		PS3MakePatchBinary,
		PS3MakePatch,
		PS3MakeDLC,
		PS3MakeTU,
		PCMakeTU,
		PCPackageTU,
		CheckSigned,
		Sign,
		SignCat,
		SignBinary,
		SignFile,
		TrackFileSize,
		TrackFolderSize,
		SimpleCopy,
		CopyScriptPatchFiles,
		SimpleDelete,
		SimpleRename,
		RenamedCopy,
		Wait,
		UpdateLabel,
		UpdateFolder,
		CISProcessP4Changes,
		CISUpdateMonitorValues,
		CheckForUCInVCProjFiles,
		SmokeTest,
		LoadPackages,
		CookPackages,
		CreateMGSTrigger,
		CreateFakeTOC,
		FTPSendFile,
		FTPSendFolder,
		FTPSendImage,
		ZipAddImage,
		ZipAddFile,
		ZipSave,
		PhysXGeneratePhysX,
		PhysXGenerateAPEX,

		VCFull,         // Composite command - clean then build
		MSVC9Full,      // Composite command - clean then build
		MSVC10Full,     // Composite command - clean then build
		GCCFull,        // Composite command - clean then build
		ShaderFull,     // Composite command - clean then build
	}

	public enum RevisionType
	{
		Invalid,
		Head,
		ChangeList,
		SimpleLabel,
		BuilderLabel,
		Label,
	}

	public enum JobState
	{
		Unneeded = 0,
		Pending,
		Optional,
		InProgress,
		InProgressOptional,
		Succeeded,
		Failed
	}

	public partial class Main : Form
	{
		// For consistent formatting to the US standard (05/29/2008 11:33:52)
		public const string US_TIME_DATE_STRING_FORMAT = "MM/dd/yyyy HH:mm:ss";

		private UnrealControls.OutputWindowDocument MainLogDoc = new UnrealControls.OutputWindowDocument();

		public string LabelRoot = "Unknown";

		public Emailer Mailer = null;
		public P4 SCC = null;
		public Watcher Watcher = null;

		public string DefaultIPhoneCompileServerName = "a1488";
		public string DefaultIPhoneSigningServerName = "a1488";
		public string DefaultMacCompileServerName = "a1487";

		private DateTime LastPrimaryBuildPollTime = DateTime.UtcNow;
		private DateTime LastNonPrimaryBuildPollTime = DateTime.UtcNow;
		private DateTime LastPrimaryJobPollTime = DateTime.UtcNow;
		private DateTime LastNonPrimaryJobPollTime = DateTime.UtcNow;
		private DateTime LastBuildKillTime = DateTime.UtcNow;
		private DateTime LastJobKillTime = DateTime.UtcNow;
		private DateTime LastCheckForJobTime = DateTime.UtcNow;
		private DateTime LastCheckForTargetStateTime = DateTime.UtcNow;

		// The next scheduled time we'll update the database
		DateTime NextMaintenanceTime = DateTime.UtcNow;

		// Time periods, in seconds, between various checks
		uint MaintenancePeriod = 10;

		public class ClientInfo
		{
			public string ClientName = "";
			public string ClientRoot = "";
			public bool bIsRemoteClient = false;

			public ClientInfo()
			{
			}

			public ClientInfo( string InName, string InRoot )
			{
				ClientName = InName;
				ClientRoot = InRoot;

				bIsRemoteClient = InRoot.StartsWith( "\\\\" );
			}
		}

		public class BranchDefinition
		{
			public int ID;
			public int Version;
			public string Branch;
			public string Server;
			public string User;
			public string Password;
			public string SyncUser;
			public string SyncPassword;
			public string BranchAdmin;
			public string DirectXVersion;
			public string AndroidSDKVersion;
			public string iPhoneSDKVersion;
			public string MacSDKVersion;
			public string NGPSDKVersion;
			public string PS3SDKVersion;
			public string WiiUSDKVersion;
			public string Xbox360SDKVersion;
			public string FlashSDKVersion;
			public string PFXSubjectName;
			public string IPhone_DeveloperSigningIdentity;
			public string IPhone_DistributionSigningIdentity;

			public ClientInfo CurrentClient;
			public List<ClientInfo> Clients;

			public BranchDefinition( int InID, 
									int InVersion,
									string InBranch, 
									string InServer, 
									string InUser, 
									string InPassword,
									string InSyncUser,
									string InSyncPassword,
									string InClientSpec, 
									string InBranchAdmin, 
									string InDXVersion,
									string InAndroidSDKVersion,
									string IniPhoneSDKVersion,
									string InMacSDKVersion,
									string InNGPSDKVersion,
									string InPS3SDKVersion,
									string InWiiUSDKVersion,
									string InXbox360SDKVersion,
									string InFlashSDKVersion,
									string InPFXSubjectName,
									string InIPhoneDevId,
									string InIPhoneDistId )
			{
				ID = InID;
				Version = InVersion;
				Branch = InBranch;
				Server = InServer;
				User = InUser;
				Password = InPassword;
				SyncUser = InSyncUser;
				SyncPassword = InSyncPassword;
				BranchAdmin = InBranchAdmin;
				DirectXVersion = InDXVersion;
				AndroidSDKVersion = InAndroidSDKVersion;
				iPhoneSDKVersion = IniPhoneSDKVersion;
				MacSDKVersion = InMacSDKVersion;
				NGPSDKVersion = InNGPSDKVersion;
				PS3SDKVersion = InPS3SDKVersion;
				WiiUSDKVersion = InWiiUSDKVersion;
				Xbox360SDKVersion = InXbox360SDKVersion;
				FlashSDKVersion = InFlashSDKVersion;
				PFXSubjectName = InPFXSubjectName;
				IPhone_DeveloperSigningIdentity = InIPhoneDevId;
				IPhone_DistributionSigningIdentity = InIPhoneDistId;

				CurrentClient = null;
				Clients = new List<ClientInfo>();
			}
		}

		public class SuppressedJobsInfo
		{
			public string Platform = "";
			public string Game = "";
			public string Config = "";

			public SuppressedJobsInfo( string InPlatform, string InGame, string InConfig )
			{
				Platform = InPlatform;
				Game = InGame;
				Config = InConfig;
			}
		}

		public class CISJobsInfo
		{
			public int ID;
			public int CISJobStateID;
			public int Changelist;
			public bool Active;
			public bool Complete;
			public bool Success;

			public CISJobsInfo( int InJobID, int? InCISJobStateID, string InChangelist, bool InActive, bool InComplete, bool InSuccess )
			{
				ID = InJobID;
				CISJobStateID = 0;
				if( InCISJobStateID.HasValue )
				{
					CISJobStateID= InCISJobStateID.Value;
				}

				Changelist = 0;
				Int32.TryParse( InChangelist, out Changelist );
				Active = InActive;
				Complete = InComplete;
				Success = InSuccess;
			}
		}

		public class CISJobStateInfo
		{
			public JobState CISJobState;
			public int Mask;

			public CISJobStateInfo( byte InJobState, int InMask )
			{
				CISJobState = ( JobState )InJobState;
				Mask = InMask;
			}
		}

		public class CommandInfo
		{
			public int CommandID = 0;
			public int JobID = 0;
			public string Script = "";
			public string Description = "";
			public int BranchConfigID = 0;
			public int CISTaskID = 0;
			public int CISJobStateID = 0;
			public string Operator = "";
			public string Game = "";
			public string Platform = "";
			public string Config = "";
			public string ScriptConfig = "";
			public string Language = "";
			public string Define = "";
			public string Parameter = "";
			public bool bRemote = false;
			public string Label = "";
			public string Promoted = "";
			public int LastAttemptedBuild = 0;
			public int LastGoodBuild = 0;
			public int LastFailedBuild = 0;
			public bool bIsPrimaryBuild = false;
			public DateTime BuildStarted = DateTime.UtcNow;

			public CommandInfo( int InID, string InScript, string InDescription, int InBranchConfigID, string InGame, string InPlatform, string InConfig, string InLanguage, string InParameter, bool InRemote,
							string InOperator, string InPromoted, int InLastAttempt, int InLastGood, int InLastFailed, bool InPrimary )
			{
				CommandID = InID;
				Script = InScript;
				Description = InDescription;
				BranchConfigID = InBranchConfigID;
				Game = InGame;
				Platform = InPlatform;
				Config = InConfig;
				Language = InLanguage;
				Parameter = InParameter;
				bRemote = InRemote;
				Operator = InOperator;
				Promoted = InPromoted;
				LastAttemptedBuild = InLastAttempt;
				LastGoodBuild = InLastGood;
				LastFailedBuild = InLastFailed;
				bIsPrimaryBuild = InPrimary;
			}

			public CommandInfo( int InID, string InScript, string InDescription, int InBranchConfigID, int InCISTaskID, int InCISJobStateID, string InGame, string InPlatform, string InConfig, string InScriptConfig, 
								string InLanguage, string InDefine, string InParameter, bool InRemote, string InLabel, bool InPrimary )
			{
				JobID = InID;
				Script = InScript;
				Description = InDescription;
				BranchConfigID = InBranchConfigID;
				CISTaskID = InCISTaskID;
				CISJobStateID = InCISJobStateID;
				Game = InGame;
				Platform = InPlatform;
				Config = InConfig;
				ScriptConfig = InScriptConfig;
				Language = InLanguage;
				Define = InDefine;
				Parameter = InParameter;
				bRemote = InRemote;
				Label = InLabel;
				bIsPrimaryBuild = InPrimary;
			}
		}

		public List<BranchDefinition> AvailableBranches = new List<BranchDefinition>();

		// A timestamp of when this builder was compiled
		public DateTime CompileDateTime { get; set; }

		// The system processor string
		public string ProcessorString { get; set; }

		// Number of processors in this machine - used to define number of threads to use when choice is available
		public int NumProcessors { get; set; }

		// Number of GB RAM in this machine
		public int PhysicalMemory { get; set; }

		// Size of the C: drive
		public int DriveCSize { get; set; }

		// Free space on C: drive
		public int DriveCFree { get; set; }

		// Size of the D: drive
		public int DriveDSize { get; set; }

		// Free space on D: drive
		public int DriveDFree { get; set; }

		// The used version of DirectX
		public string DXVersion { get; set; }

		// The local current version of the Android SDK
		public string AndroidSDKVersion { get; set; }

		// The local current version of the iPhone SDK
		public string iPhoneSDKVersion { get; set; }

		// The local current version of the Mac SDK
		public string MacSDKVersion { get; set;	}

		// The local current version of the NGP SDK
		public string NGPSDKVersion { get; set; }

		// The local current version of the PS3 SDK
		public string PS3SDKVersion { get; set; }

		// The local current version of the WiiU SDK
		public string WiiUSDKVersion { get;set; }

		// The local current version of the XDK
		public string Xbox360SDKVersion { get; set; }

		// The local current version of the XDK
		public string FlashSDKVersion { get; set; }

		// Number of parallel jobs to use (based on the number of logical processors)
		public int NumJobs { get; set; }

		// Whether this machine is locked to a specific build
		public int MachineLock { get; set; }

		// A unique id for each job set
		public long JobSpawnTime { get; set; }

		// The number of jobs placed into the job table
		public int NumJobsToWaitFor { get; set; }

		// The number of jobs completed so far
		public int NumCompletedJobs { get; set; }

		// A log of everything the system 
		public TextWriter SystemLog { get; set; }

		public bool Ticking = true;
		public bool Restart = false;
		public string MachineName = "";
		private string LoggedOnUser = "";

		public int CommandID = 0;
		public int JobID = 0;
		private int BuilderID = 0;
		private int BuildLogID = 0;
		private List<SandboxedAction> PendingCommands = new List<SandboxedAction>();
		private int BlockingBuildID = 0;
		private DateTime BlockingBuildStartTime = DateTime.UtcNow;
		private DateTime ConsoleStateStartTime = DateTime.UtcNow;
		private BuildState Builder = null;
		private MODES Mode = 0;
		private int SyncRetryCount = 0;
		private int CompileRetryCount = 0;
		private int LinkRetryCount = 0;
		private string FinalStatus = "";

		delegate void DelegateAddLine( string Line, Color TextColor );
		delegate void DelegateWritePerformanceData( string MachineName, string BranchName, string KeyName, long Value, int Changelist );
		delegate void DelegateSetStatus( string Line );
		delegate void DelegateMailGlitch( string Line );

		public static string GetStringEnvironmentVariable( string VarName, string Default )
		{
			string Value = Environment.GetEnvironmentVariable( VarName );
			if( Value != null )
			{
				return Value;
			}
			return Default;
		}

		public void SendWarningMail( string Title, string Warnings, bool CCIT )
		{
			Mailer.SendWarningMail( Title, Warnings, CCIT );
		}

		public void SendErrorMail( string Title, string Warnings )
		{
			Mailer.SendErrorMail( Title, Warnings, false );
		}

		public void SendAlreadyInProgressMail( string Operator, string BuildDescription )
		{
			Mailer.SendAlreadyInProgressMail( Operator, BuildDescription );
		}

		public void DeleteFiles( string Path, int DaysOld )
		{
			// Delete all the files in the directory tree
			DirectoryInfo DirInfo = new DirectoryInfo( Path );
			if( DirInfo.Exists )
			{
				DirectoryInfo[] Directories = DirInfo.GetDirectories();
				foreach( DirectoryInfo Directory in Directories )
				{
					DeleteFiles( Directory.FullName, DaysOld );
				}

				FileInfo[] Files = DirInfo.GetFiles();
				foreach( FileInfo File in Files )
				{
					bool bShouldDelete = true;
					if( DaysOld > 0 )
					{
						TimeSpan Age = DateTime.UtcNow - File.CreationTimeUtc;
						TimeSpan MaxAge = new TimeSpan( DaysOld, 0, 0, 0 );
						if( Age <= MaxAge )
						{
							bShouldDelete = false;
						}
					}
					if( bShouldDelete )
					{
						try
						{
							File.IsReadOnly = false;
							File.Delete();
						}
						catch
						{
						}
					}
				}
			}
		}

		public void DeleteEmptyDirectories( string Path )
		{
			// Delete all the files in the directory tree
			DirectoryInfo DirInfo = new DirectoryInfo( Path );
			if( DirInfo.Exists )
			{
				DirectoryInfo[] Directories = DirInfo.GetDirectories();
				foreach( DirectoryInfo Directory in Directories )
				{
					DeleteEmptyDirectories( Directory.FullName );

					// Delete this folder if it has no subfolders or files
					FileInfo[] SubFiles = Directory.GetFiles();
					DirectoryInfo[] SubDirectories = Directory.GetDirectories();

					if( SubFiles.Length == 0 && SubDirectories.Length == 0 )
					{
						try
						{
							Directory.Delete( true );
						}
						catch
						{
						}
					}
				}
			}
		}

		// Recursively delete an entire directory tree
		public bool DeleteDirectory( string Path, int DaysOld )
		{
			try
			{
				// Delete all the files in the directory tree
				DeleteFiles( Path, DaysOld );

				// Delete all the empty folders
				DeleteEmptyDirectories( Path );
			}
			catch( Exception Ex )
			{
				Log( "Failed to delete directory: '" + Path + "' with Exception: " + Ex.Message, Color.Red );
				return ( false );
			}

			return ( true );
		}

		// Ensure the base build folder exists to copy builds to
		public void EnsureDirectoryExists( string PathName )
		{
			try
			{
				DirectoryInfo Dir = new DirectoryInfo( PathName );
				if( !Dir.Exists )
				{
					Dir.Create();
				}
			}
			catch
			{
			}
		}

		// Extract the time and date of compilation from the version number
		private void CalculateCompileDateTime()
		{
			System.Version Version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
			CompileDateTime = DateTime.Parse( "01/01/2000" ).AddDays( Version.Build ).AddSeconds( Version.Revision * 2 );

			Log( "Controller compiled on " + CompileDateTime.ToString(), Color.Blue );
		}

		private string GetWMIValue( string Key, ManagementObject ManObject )
		{
			Object Value;

			try
			{
				Value = ManObject.GetPropertyValue( Key );
				if( Value != null )
				{
					return ( Value.ToString() );
				}
			}
			catch
			{
			}

			return ( "" );
		}

		private void GetProcessorInfo()
		{
			// Type and speed of processor
			ManagementObjectSearcher Searcher = new ManagementObjectSearcher( "Select * from CIM_Processor" );
			ManagementObjectCollection Collection = Searcher.Get();

			foreach( ManagementObject Object in Collection )
			{
				ProcessorString = GetWMIValue( "Name", Object );
				break;
			}

			// Number of cores/threads
			Searcher = new ManagementObjectSearcher( "Select * from Win32_Processor" );
			Collection = Searcher.Get();

			foreach( ManagementObject Object in Collection )
			{
				string NumberOfProcs = GetWMIValue( "NumberOfLogicalProcessors", Object );
				if( NumberOfProcs.Length > 0 )
				{
					// Best guess at number of processors
					NumProcessors = Int32.Parse( NumberOfProcs );
					if( NumProcessors < 2 )
					{
						NumProcessors = 2;
					}

					// Best guess at number of jobs to spawn
					NumJobs = ( NumProcessors * 3 ) / 2;
					if( NumJobs < 2 )
					{
						NumJobs = 2;
					}
					else if( NumJobs > 8 )
					{
						NumJobs = 8;
					}
				}
				break;
			}
		}

		private void GetMemoryInfo()
		{
			// Amount and type of memory
            ManagementObjectSearcher Searcher = new ManagementObjectSearcher( "Select * from CIM_PhysicalMemory" );
            ManagementObjectCollection Collection = Searcher.Get();
			long Memory = 0;

            foreach( ManagementObject Object in Collection )
            {
                string Capacity = GetWMIValue( "Capacity", Object );
				Memory += Int64.Parse( Capacity );
            }

			PhysicalMemory = ( int )( Memory / ( 1024 * 1024 * 1024 ) );
		}

		private void GetDiskInfo()
		{
			try
			{
				// Available disks and free space
				ManagementObjectSearcher Searcher = new ManagementObjectSearcher( "Select * from Win32_LogicalDisk" );
				ManagementObjectCollection Collection = Searcher.Get();

				foreach( ManagementObject Object in Collection )
				{
					string DriveType = GetWMIValue( "DriveType", Object );
					if( DriveType == "3" )
					{
						Int64 Size = 0;
						Int64 FreeSpace = 0;

						string Name = GetWMIValue( "Caption", Object );
						string SizeInfo = GetWMIValue( "Size", Object );
						string FreeSpaceInfo = GetWMIValue( "FreeSpace", Object );

						try
						{
							Size = Int64.Parse( SizeInfo ) / ( 1024 * 1024 * 1024 );
							FreeSpace = Int64.Parse( FreeSpaceInfo ) / ( 1024 * 1024 * 1024 );
						}
						catch
						{
						}

						switch( Name.ToUpper() )
						{
						case "C:":
							DriveCSize = ( int )Size;
							DriveCFree = ( int )FreeSpace;
							break;

						case "D:":
							DriveDSize = ( int )Size;
							DriveDFree = ( int )FreeSpace;
							break;

						default:
							break;
						}
					}
				}
			}
			catch
			{
			}
		}

		private void GetInfo()
		{
			MachineName = Environment.MachineName;
			LoggedOnUser = Environment.UserName;

			Log( "Welcome \"" + LoggedOnUser + "\" running on \"" + MachineName + "\"", Color.Blue );
		}

		private void GetPS3SDKVersion()
		{
			Log( " ... searching for PS3 SDK", Color.Blue );
			string Line = "";
			try
			{
				string Dir = Environment.GetEnvironmentVariable( "SCE_PS3_ROOT" );
				string PS3SDK = Path.Combine( Dir, "version-SDK" );
				StreamReader Reader = new StreamReader( PS3SDK );
				if( Reader != null )
				{
					Line = Reader.ReadToEnd();
					Reader.Close();
				}
			}
			catch
			{
			}

			PS3SDKVersion = Line.Trim();

			if( PS3SDKVersion.Length > 0 )
			{
				Log( " ...   using PS3 SDK: " + PS3SDKVersion, Color.Blue );
			}
			else
			{
				PS3SDKVersion = "None";
			}
		}

		private void GetNGPSDKVersion()
		{
			Log( " ... searching for NGP SDK", Color.Blue );
			NGPSDKVersion = "";
			string NGPEnvironmentVariable = Environment.GetEnvironmentVariable( "SCE_PSP2_SDK_DIR" );
			if( NGPEnvironmentVariable != null )
			{
				DirectoryInfo NGPSDKBaseDirectory = new DirectoryInfo( NGPEnvironmentVariable );
				if( NGPSDKBaseDirectory.Exists )
				{
					NGPSDKVersion = NGPSDKBaseDirectory.Name;
				}
			}

			if( NGPSDKVersion.Length > 0 )
			{
				Log( " ...   using NGP SDK: " + NGPSDKVersion, Color.Blue );
			}
			else
			{
				NGPSDKVersion = "None";
			}
		}

		private void GetWiiUSDKVersion()
		{
			Log( " ... searching for WiiU SDK", Color.Blue );
			try
			{
				// Find the latest WiiU SDK
				DirectoryInfo DirInfo = new DirectoryInfo( Environment.GetEnvironmentVariable( "CAFE_ROOT" ) );
				if( DirInfo.Exists )
				{
					WiiUSDKVersion = DirInfo.Name;
				}

				// Append the GHS version
				if( WiiUSDKVersion.Length > 0 )
				{
					DirInfo = new DirectoryInfo( Environment.GetEnvironmentVariable( "GHS_ROOT" ) );
					if( DirInfo.Exists )
					{
						WiiUSDKVersion += "/" + DirInfo.Name;
					}
					else
					{
						WiiUSDKVersion = "";
					}
				}
			}
			catch
			{
			}

			if( WiiUSDKVersion.Length > 0 )
			{
				Log( " ...   using WiiU SDK: " + WiiUSDKVersion, Color.Blue );
			}
			else
			{
				WiiUSDKVersion = "None";
			}
		}

		private void GetFlashSDKVersion()
		{
			Log( " ... searching for Flash SDK", Color.Blue );
			string Line = "";
			try
			{
				string Dir = Environment.GetEnvironmentVariable( "ALCHEMY_ROOT" );
				string FlashSDK = Path.Combine( Dir, "ver.txt" );
				StreamReader Reader = new StreamReader( FlashSDK );
				if( Reader != null )
				{
					Line = Reader.ReadToEnd();
					Reader.Close();
				}
			}
			catch
			{
			}

			FlashSDKVersion = Line.Trim();
			if( FlashSDKVersion.Length > 0 )
			{
				Log( " ...   using Flash SDK: " + FlashSDKVersion, Color.Blue );
			}
			else
			{
				FlashSDKVersion = "None";
			}
		}

		private void GetXDKVersion()
		{
			Log( " ... searching for Xbox360 SDK (XDK)", Color.Blue );
			try
			{
				string Line;

				string XEDK = Environment.GetEnvironmentVariable( "XEDK" );
				if( XEDK != null )
				{
					string XDK = XEDK + "/include/win32/xdk.h";
					FileInfo Info = new FileInfo( XDK );

					if( Info.Exists )
					{
						StreamReader Reader = new StreamReader( XDK );
						if( Reader != null )
						{
							Line = Reader.ReadLine();
							while( Line != null )
							{
								if( Line.StartsWith( "#define" ) )
								{
									int Offset = Line.IndexOf( "_XDK_VER" );
									if( Offset >= 0 )
									{
										Xbox360SDKVersion = Line.Substring( Offset + "_XDK_VER".Length ).Trim();
										break;
									}
								}

								Line = Reader.ReadLine();
							}
							Reader.Close();
						}
					}
				}
				else
				{
					Log( "Could not find XEDK environment variable", Color.Blue );
				}
			}
			catch
			{
			}

			if( Xbox360SDKVersion.Length > 0 )
			{
				Log( " ...   using Xbox360 SDK (XDK): " + Xbox360SDKVersion, Color.Blue );
			}
			else
			{
				Xbox360SDKVersion = "None";
			}
		}

		private void GetAvailableBranches()
		{
			// Clear out any existing connections
			SCC.DeleteTickets();

			// Find out which branches we have locally
			List<BranchDefinition> PotentialBranches = BuilderLinq.GetBranches();
			foreach( BranchDefinition Branch in PotentialBranches )
			{
				if( SCC.BranchExists( Branch ) )
				{
					AvailableBranches.Add( Branch );
					Log( " ... added branch: '" + Branch.Branch + "' on server '" + Branch.Server + "' for " + Branch.Clients.Count + " clients", Color.DarkGreen );
					foreach( ClientInfo Client in Branch.Clients )
					{
						Log( " ...... client: " + Client.ClientName + " (" + Client.ClientRoot + ")", Color.DarkGreen );
					}
				}
			}
		}

		public BranchDefinition FindBranchDefinition( int BranchConfigID )
		{
			BranchDefinition FoundBranchDef = null;

			foreach( BranchDefinition BranchDef in AvailableBranches )
			{
				if( BranchDef.ID == BranchConfigID )
				{
					FoundBranchDef = BranchDef;
					break;
				}
			}

			return ( FoundBranchDef );
		}

		private void CheckMachineLock()
		{
			// If this machine locked to a build then don't allow it to grab normal ones
			int OldMachineLock = MachineLock;

			MachineLock = BuilderLinq.GetMachineLock( MachineName );
			// Check for first instance
			if( OldMachineLock < 0 )
			{
				if( MachineLock == 0 )
				{
					Log( "Starting up with machine not locked to any specific command", Color.Blue );
				}
				else
				{
					Log( "Starting up with machine locked to command ID " + MachineLock.ToString(), Color.Blue );
				}
			}
			// Check for a change of machine locked state
			else if( MachineLock != OldMachineLock )
			{
				if( MachineLock == 0 )
				{
					Log( "Machine is now unlocked from any command", Color.Blue );
				}
				else
				{
					Log( "Machine is now locked to command ID " + MachineLock.ToString(), Color.Blue );
				}
			}
		}

		private void BroadcastMachine()
		{
			// Clearing out orphaned connections if the process stopped unexpectedly
			BuilderLinq.CleanupOrphanedBuilders( MachineName );

			// Clear out orphaned commands
			BuilderLinq.CleanupCommand( 0, MachineName );

			// Clear out any orphaned jobs
			BuilderLinq.CleanupJob( MachineName );

			// Insert machine as new row
			Log( "Registering '" + MachineName + "' with database", Color.Blue );
			BuilderID = BuilderLinq.BroadcastMachine( this );
			if( BuilderID != 0 )
			{
				GetAvailableBranches();
			}

			// Check to see if this builder is locked to any commands
			CheckMachineLock();
		}

		private void MaintainMachine()
		{
			// Check to see if it's time for maintenance
			if( DateTime.UtcNow >= NextMaintenanceTime )
			{
				BuilderLinq.PingDatabase( BuilderID );

				// Schedule the next update
				NextMaintenanceTime = DateTime.UtcNow.AddSeconds( MaintenancePeriod );

				// Check to see if this builder is locked to any commands
				CheckMachineLock();
			}
		}

		private int PollForBuild( bool PrimaryBuildsOnly )
		{
			int ID = 0;

			// Check every 5 seconds
			TimeSpan PingTime = new TimeSpan( 0, 0, 5 );

			// Be sure to check against the correct timer
			if( ( PrimaryBuildsOnly && ( DateTime.UtcNow - LastPrimaryBuildPollTime > PingTime ) ) ||
									  ( DateTime.UtcNow - LastNonPrimaryBuildPollTime > PingTime ) )
			{
				ID = BuilderLinq.PollForBuild( this, PrimaryBuildsOnly );
				if( PrimaryBuildsOnly )
				{
					LastPrimaryBuildPollTime = DateTime.UtcNow;
				}
				else
				{
					LastNonPrimaryBuildPollTime = DateTime.UtcNow;
				}

				if( ID >= 0 )
				{
					if( ID > 0 )
					{
						// Check for build already running
						string Machine = BuilderLinq.GetCommandString( ID, "Machine" );
						if( Machine.Length > 0 )
						{
							string BuildType = BuilderLinq.GetCommandString( ID, "Description" );
							string Operator = BuilderLinq.GetCommandString( ID, "Operator" );
							Log( "[STATUS] Suppressing retrigger of '" + BuildType + "'", Color.Magenta );
							Mailer.SendAlreadyInProgressMail( Operator, BuildType );

							ID = 0;
						}
					}
				}
				else
				{
					Mailer.SendWarningMail( "PollForBuild", "PollForBuild returned an error! Check logs for more details.", false );
					ID = 0;
				}
			}

			return ( ID );
		}

		private int PollForJob( bool PrimaryBuildsOnly )
		{
			int ID = 0;

			if( MachineLock == 0 )
			{
				// Check every 5 seconds
				TimeSpan PingTime = new TimeSpan( 0, 0, 5 );

				if( ( PrimaryBuildsOnly && ( DateTime.UtcNow - LastPrimaryJobPollTime > PingTime ) ) ||
										  ( DateTime.UtcNow - LastNonPrimaryJobPollTime > PingTime ) )
				{
					ID = BuilderLinq.CheckForJob( this, PrimaryBuildsOnly );
					if( PrimaryBuildsOnly )
					{
						LastPrimaryJobPollTime = DateTime.UtcNow;
					}
					else
					{
						LastNonPrimaryJobPollTime = DateTime.UtcNow;
					}
				}
			}

			return ( ID );
		}

		private int PollForKillBuild()
		{
			int ID = 0;

			// Check every 2 seconds
			TimeSpan PingTime = new TimeSpan( 0, 0, 2 );
			if( DateTime.UtcNow - LastBuildKillTime > PingTime )
			{
				ID = BuilderLinq.PollForKillBuild( MachineName );
				LastBuildKillTime = DateTime.UtcNow;
			}

			return ( ID );
		}

		private int PollForKillJob()
		{
			int ID = 0;

			// Check every 2 seconds
			TimeSpan PingTime = new TimeSpan( 0, 0, 2 );
			if( DateTime.UtcNow - LastJobKillTime > PingTime )
			{
				ID = BuilderLinq.PollForKillJob( MachineName );
				LastJobKillTime = DateTime.UtcNow;
			}

			return ( ID );
		}

		private void AppExceptionHandler( object sender, UnhandledExceptionEventArgs args )
		{
			Exception E = ( Exception )args.ExceptionObject;
			Log( "Application exception: " + E.ToString(), Color.Red );

			// Send a warning email with this error, if possible
			if( Mailer != null )
			{
				Mailer.SendErrorMail( "Application Exception in Controller!", E.ToString(), false );
			}
		}

		public void Init()
		{
			// Register application exception handler
			AppDomain.CurrentDomain.UnhandledException += new UnhandledExceptionEventHandler( AppExceptionHandler );

			// Show log window
			Show();

			SCC = new P4( this );
			Mailer = new Emailer( this, SCC );
			Watcher = new Watcher( this );

			Application.DoEvents();

			GetInfo();
			CalculateCompileDateTime();
			GetProcessorInfo();
			GetMemoryInfo();
			GetDiskInfo();
			GetPS3SDKVersion();
			GetNGPSDKVersion();
			GetWiiUSDKVersion();
			GetFlashSDKVersion();
			GetXDKVersion();

			Application.DoEvents();

			// Something went wrong during setup - sleep and retry
			if( !Ticking )
			{
				System.Threading.Thread.Sleep( 30000 );
				Restart = true;
				return;
			}

			// Register with DB
			BroadcastMachine();

			Log( "Running!", Color.DarkGreen );	
		}

		public void Destroy()
		{
			Log( "Unregistering '" + MachineName + "' from database", Color.Blue );
			BuilderLinq.UnbroadcastMachine( this );
		}

		public Main()
		{
			InitializeComponent();
			MainLogWindow.Document = MainLogDoc;

			CompileDateTime = DateTime.UtcNow;
			DXVersion = "None";
			AndroidSDKVersion = "None";
			iPhoneSDKVersion = "None";
			MacSDKVersion = "None";
			NGPSDKVersion = "None";
			PS3SDKVersion = "None";
			WiiUSDKVersion = "None";
			Xbox360SDKVersion = "None";
			FlashSDKVersion = "None";
			NumProcessors = 2;
			NumJobs = 4;
			MachineLock = -1;
			JobSpawnTime = 0;
			NumJobsToWaitFor = 0;
			NumCompletedJobs = 0;
			SystemLog = null;

			BuilderLinq.Parent = this;
		}

		public void WritePerformanceData( string MachineName, string BranchName, string KeyName, long Value, int Changelist )
		{
			if( KeyName.Length == 0 || Value <= 0 )
			{
				return;
			}

			// if we need to, invoke the delegate
			if( InvokeRequired )
			{
				Invoke( new DelegateWritePerformanceData( WritePerformanceData ), new object[] { MachineName, BranchName, KeyName, Value, Changelist } );
				return;
			}

			BuilderLinq.WritePerformanceData( MachineName, BranchName, KeyName, Value, Changelist );
		}

		public void WritePerformanceEvent( string Group, string Event )
		{
			WritePerformanceData( MachineName, Group, Event, 1, 0 );
		}

		public void Log( string Line, Color TextColour )
		{
			if( Line == null || !Ticking )
			{
				return;
			}

			// if we need to, invoke the delegate
			if( InvokeRequired )
			{
				Invoke( new DelegateAddLine( Log ), new object[] { Line, TextColour } );
				return;
			}

			// Not UTC as this is visible to users
			string FullLine = DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Line;

			MainLogDoc.AppendText( TextColour, FullLine + Environment.NewLine );

			// Log it out to the system log, if there is one
			if( SystemLog != null )
			{
				SystemLog.WriteLine( FullLine );
			}

			CheckStatusUpdate( Line );
		}

		private void HandleWatchStatus( string Line )
		{
			string KeyName = "";
			long Value = -1;

			if( Line.StartsWith( "[WATCHSTART " ) )
			{
				KeyName = Line.Substring( "[WATCHSTART ".Length ).TrimEnd( "]".ToCharArray() );
				Watcher.WatchStart( KeyName );
				Value = -1;
			}
			else if( Line.StartsWith( "[WATCHSTOP]" ) )
			{
				Value = Watcher.WatchStop( ref KeyName );
			}
			else if( Line.StartsWith( "[WATCHTIME " ) )
			{
				KeyName = "";
				string KeyValue = Line.Substring( "[WATCHTIME ".Length ).TrimEnd( "]".ToCharArray() );

				string[] Split = Builder.SafeSplit( KeyValue, false );
				if( Split.Length == 2 )
				{
					KeyName = Split[0];
					Value = Builder.SafeStringToInt( Split[1] );
				}
			}

			// Write the perf data to the database
			WritePerformanceData( MachineName, Builder.BranchDef.Branch, KeyName, Value, Builder.LabelInfo.Changelist );
		}

		public void AddPerfData( string Line )
		{
			string Key = "";
			string Value = "";

			string[] KeyValue = Builder.SafeSplit( Line, false );

			foreach( string Name in KeyValue )
			{
				if( Name.Length > 0 )
				{
					if( Key.Length == 0 )
					{
						Key = Name;
					}
					else if( Value.Length == 0 )
					{
						Value = Name;
					}
					else
					{
						break;
					}
				}
			}

			// Write the perf data to the database
			long LongValue = Builder.SafeStringToLong( Value );
			WritePerformanceData( MachineName, Builder.BranchDef.Branch, Key, LongValue, Builder.LabelInfo.Changelist );
		}

		public void CheckStatusUpdate( string Line )
		{
			if( Line == null || !Ticking || BuildLogID == 0 )
			{
				return;
			}

			// Handle any special controls
			if( Line.StartsWith( "[STATUS] " ) )
			{
				Line = Line.Substring( "[STATUS] ".Length );
				SetStatus( Line.Trim() );
			}
			else if( Line.StartsWith( "[WATCH" ) )
			{
				HandleWatchStatus( Line );
			}

			if( Line.IndexOf( "=> NETWORK " ) >= 0 )
			{
				MailGlitch( Line );
			}
		}

		public long GetDirectorySize( string Directory )
		{
			long CurrentSize = 0;

			DirectoryInfo DirInfo = new DirectoryInfo( Directory );
			if( DirInfo.Exists )
			{
				FileInfo[] Infos = DirInfo.GetFiles();
				foreach( FileInfo Info in Infos )
				{
					if( Info.Exists )
					{
						CurrentSize += Info.Length;
					}
				}

				DirectoryInfo[] SubDirInfo = DirInfo.GetDirectories();
				foreach( DirectoryInfo Info in SubDirInfo )
				{
					if( Info.Exists )
					{
						CurrentSize += GetDirectorySize( Info.FullName );
					}
				}
			}

			return CurrentSize;
		}

		public void MailGlitch( string Line )
		{
			if( InvokeRequired )
			{
				Invoke( new DelegateMailGlitch( MailGlitch ), new object[] { Line } );
				return;
			}

			Mailer.SendGlitchMail( Line );
		}

		public bool KillProcess( Process ChildProcess )
		{
			try
			{
				Log( " ... killing: '" + ChildProcess.ProcessName + "'", Color.Red );
				ChildProcess.Kill();
				return true;
			}
			catch
			{
				Log( " ... killing failed", Color.Red );
			}
			return false;
		}

		public bool KillProcess( string ProcessName )
		{
			bool bKilledAnyProcess = false;
			Process[] ChildProcesses = Process.GetProcessesByName( ProcessName );
			foreach( Process ChildProcess in ChildProcesses )
			{
				bKilledAnyProcess |= KillProcess( ChildProcess );
			}
			return bKilledAnyProcess;
		}

		public void SetStatus( string Line )
		{
			// if we need to, invoke the delegate
			if( InvokeRequired )
			{
				Invoke( new DelegateSetStatus( SetStatus ), new object[] { Line } );
				return;
			}

			if( Line.Length > 127 )
			{
				Line = Line.Substring( 0, 127 );
			}

			BuilderLinq.SetCurrentStatus( BuilderID, Line );
		}

		public void Log( Array Lines, Color TextColour )
		{
			foreach( string Line in Lines )
			{
				Log( Line, TextColour );
			}
		}

		public string ExpandParameter( string Input, string Parameter, string Replacement )
		{
			while( Input.Contains( Parameter ) )
			{
				Input = Input.Replace( Parameter, Replacement );
			}

			return ( Input );
		}

		public string ExpandString( string Input )
		{
			// Expand predefined constants
			Input = ExpandParameter( Input, "%Game%", Builder.CommandDetails.Game );
			Input = ExpandParameter( Input, "%Platform%", Builder.CommandDetails.Platform );
			Input = ExpandParameter( Input, "%Config%", Builder.CommandDetails.Config );
			Input = ExpandParameter( Input, "%Language%", Builder.CommandDetails.Language );
			Input = ExpandParameter( Input, "%DatabaseParameter%", Builder.CommandDetails.Parameter );
			Input = ExpandParameter( Input, "%Promoted%", Builder.CommandDetails.Promoted );
			Input = Input.Replace( "%Branch%", Builder.BranchDef.Branch );

			Input = ExpandParameter( Input, "%JobGame%", Builder.CommandDetails.Game );
			Input = ExpandParameter( Input, "%JobPlatform%", Builder.CommandDetails.Platform );
			Input = ExpandParameter( Input, "%JobConfig%", Builder.CommandDetails.Config );
			Input = ExpandParameter( Input, "%JobScriptConfig%", Builder.CommandDetails.ScriptConfig );
			Input = ExpandParameter( Input, "%JobLanguage%", Builder.CommandDetails.Language );
			Input = ExpandParameter( Input, "%JobDefine%", Builder.CommandDetails.Define );
			Input = ExpandParameter( Input, "%JobParameter%", Builder.CommandDetails.Parameter );
			Input = ExpandParameter( Input, "%JobLabel%", Builder.CommandDetails.Label );
			Input = Input.Replace( "%JobBranch%", Builder.BranchDef.Branch );

			// Expand variables from variables table
			string[] Parms = Builder.SafeSplit( Input, true );
			for( int i = 0; i < Parms.Length; i++ )
			{
				if( Parms[i].StartsWith( "#" ) && Parms[i] != "#head" )
				{
					// Special handling for the verification label
					if( Parms[i].ToLower() == "#verificationlabel" )
					{
						Parms[i] = BuilderLinq.GetVariable( 1, Parms[i].Substring( 1 ) );
					}
					else
					{
						// .. otherwise just look up the variable in the database
						Parms[i] = BuilderLinq.GetVariable( Builder.BranchDef.ID, Parms[i].Substring( 1 ) );
					}
				}
			}

			// Reconstruct Command line
			string Line = "";
			foreach( string Parm in Parms )
			{
				Line += Parm + " ";
			}

			return ( Line.Trim() );
		}

		private string CommonPath
		{
			get
			{
				string ProgramFilesFolder = "C:\\Program Files (x86)";
				string PS3Root = Environment.GetEnvironmentVariable( "SCE_PS3_ROOT" );
				string SNCommonPath = Environment.GetEnvironmentVariable( "SN_COMMON_PATH" );
				string SNPS3Path = Environment.GetEnvironmentVariable( "SN_PS3_PATH" );
				string XEDKPath = Environment.GetEnvironmentVariable( "XEDK" );
				string SCERootPath = Environment.GetEnvironmentVariable( "SCE_ROOT_DIR" );

				string SearchPath = "";
				if( SNCommonPath != null )
				{
					SearchPath += Path.Combine( SNCommonPath, "VSI\\bin" ) + ";";
					SearchPath += Path.Combine( SNCommonPath, "bin" ) + ";";
				}

				if( SNPS3Path != null )
				{
					SearchPath += Path.Combine( SNPS3Path, "bin" ) + ";";
				}

				if( SCERootPath != null )
				{
					SearchPath += Path.Combine( SCERootPath, "PSP2\\Tools\\Target Manager Server\\bin" ) + ";";
				}

				if( XEDKPath != null )
				{
					SearchPath += Path.Combine( XEDKPath, "bin\\win32" ) + ";";
				}

				if( PS3Root != null )
				{
					SearchPath += Path.Combine( PS3Root, "\\host-win32\\bin" ) + ";";
					SearchPath += Path.Combine( PS3Root, "\\host-win32\\ppu\\bin" ) + ";";
					SearchPath += Path.Combine( PS3Root, "\\host-win32\\spu\\bin" ) + ";";
					SearchPath += Path.Combine( PS3Root, "\\host-win32\\Cg\\bin" ) + ";";
				}

				SearchPath += ProgramFilesFolder + "\\NVIDIA Corporation\\PhysX\\Common;";
				SearchPath += ProgramFilesFolder + "\\NVIDIA Corporation\\PhysX\\Common64;";
				SearchPath += ProgramFilesFolder + "\\Microsoft DirectX SDK (" + DXVersion + ")\\Utilities\\Bin\\x86;";
				SearchPath += ProgramFilesFolder + "\\Microsoft SQL Server\\90\\Tools\\binn\\;";
				SearchPath += ProgramFilesFolder + "\\Perforce;";
				SearchPath += "C:\\Program Files\\Perforce;";
				SearchPath += "C:\\Perl\\bin\\;";
				SearchPath += "C:\\Perl64\\bin\\;";
				SearchPath += ProgramFilesFolder + "\\Windows Imaging\\;";
				SearchPath += ProgramFilesFolder + "\\Xoreax\\IncrediBuild;";
				SearchPath += "C:\\Program Files\\System Center Operations Manager 2007\\;";
				SearchPath += "C:\\Windows\\system32;";
				SearchPath += "C:\\Windows;";
				SearchPath += "C:\\Windows\\System32\\Wbem;";

				return ( SearchPath );
			}
		}

		private string VC9Path
		{
			get
			{
				string ProgramFilesFolder = "C:\\Program Files (x86)";

				string SearchPath = ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\Common7\\IDE;";
				SearchPath += ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\BIN;";
				SearchPath += ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\Common7\\Tools;";
				SearchPath += "C:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\bin;";
				SearchPath += "C:\\Windows\\Microsoft.NET\\Framework\\v3.5;";
				SearchPath += "C:\\Windows\\Microsoft.NET\\Framework\\v2.0.50727;";
				SearchPath += ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\VCPackages;";

				return ( SearchPath );
			}
		}

		private string VC10Path
		{
			get
			{
				string ProgramFilesFolder = "C:\\Program Files (x86)";

				string SearchPath = ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\Common7\\IDE;";
				SearchPath += ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\BIN;";
				SearchPath += ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\Common7\\Tools;";
				SearchPath += "C:\\Program Files\\Microsoft SDKs\\Windows\\v7.0A\\bin;";
				SearchPath += "C:\\Windows\\Microsoft.NET\\Framework\\v4.0.30319;";
				SearchPath += ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\VCPackages;";

				return ( SearchPath );
			}
		}

		public void SetMSVC9EnvVars()
		{
			string ProgramFilesFolder = "C:\\Program Files (x86)";

			Environment.SetEnvironmentVariable( "VSINSTALLDIR", ProgramFilesFolder + "\\Microsoft Visual Studio 9.0" );
			Environment.SetEnvironmentVariable( "VCINSTALLDIR", ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC" );

			Environment.SetEnvironmentVariable( "INCLUDE", ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\ATLMFC\\INCLUDE;" + ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\INCLUDE;C:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\include;" );
			Environment.SetEnvironmentVariable( "LIB", ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\ATLMFC\\LIB;" + ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\LIB;C:\\Program Files\\Microsoft SDKs\\Windows\\v6.0A\\lib;" );
			Environment.SetEnvironmentVariable( "LIBPATH", "C:\\Windows\\Microsoft.NET\\Framework\\v3.5;C:\\Windows\\Microsoft.NET\\Framework\\v2.0.50727;" + ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\ATLMFC\\LIB;" + ProgramFilesFolder + "\\Microsoft Visual Studio 9.0\\VC\\LIB;" );
			Environment.SetEnvironmentVariable( "Path", VC9Path + CommonPath );

			Environment.SetEnvironmentVariable( "FrameworkDir", "C:\\Windows\\Microsoft.NET\\Framework" );
		}

		public void SetMSVC10EnvVars()
		{
			string ProgramFilesFolder = "C:\\Program Files (x86)";

			Environment.SetEnvironmentVariable( "VSINSTALLDIR", ProgramFilesFolder + "\\Microsoft Visual Studio 10.0" );
			Environment.SetEnvironmentVariable( "VCINSTALLDIR", ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC" );

			Environment.SetEnvironmentVariable( "INCLUDE", ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\INCLUDE;" + ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\INCLUDE;" + ProgramFilesFolder + "\\Microsoft SDKs\\Windows\\v7.0A\\include;" );
			Environment.SetEnvironmentVariable( "LIB", ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;" + ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\LIB;" + ProgramFilesFolder + "\\Microsoft SDKs\\Windows\\v7.0A\\lib;" );
			Environment.SetEnvironmentVariable( "LIBPATH", "C:\\Windows\\Microsoft.NET\\Framework64\\v4.0.30319;" + ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\ATLMFC\\LIB;" + ProgramFilesFolder + "\\Microsoft Visual Studio 10.0\\VC\\LIB;" );
			Environment.SetEnvironmentVariable( "Path", VC10Path + CommonPath );

			Environment.SetEnvironmentVariable( "FrameworkDir", "C:\\Windows\\Microsoft.NET\\Framework" );
		}

		private void SetDXSDKEnvVar()
		{
			// The number of ms MSBuild hangs around
			Environment.SetEnvironmentVariable( "MSBUILDNODECONNECTIONTIMEOUT", "5000" );

			string DXSDKLocation = "C:\\Program Files (x86)\\Microsoft DirectX SDK (" + DXVersion + ")\\";
			Environment.SetEnvironmentVariable( "DXSDK_DIR", DXSDKLocation );
			// Log( "Set DXSDK_DIR='" + DXSDKLocation + "'", Color.DarkGreen );
		}

		private void SetIPhoneEnvVar()
		{
			// All of the special incantations necessary for iPhone building and signing remotely on a Mac
			Environment.SetEnvironmentVariable( "ue3.iPhone_CompileServerName", 
				Builder.iPhoneCompileServerOverride != "" ? Builder.iPhoneCompileServerOverride : DefaultIPhoneCompileServerName );
			Environment.SetEnvironmentVariable( "ue3.iPhone_SigningServerName",
				Builder.iPhoneSigningServerOverride != "" ? Builder.iPhoneSigningServerOverride : DefaultIPhoneSigningServerName );
			Environment.SetEnvironmentVariable( "ue3.iPhone_SigningPrefix",
				Builder.SigningPrefixOverride != "" ?
				Builder.SigningPrefixOverride :
				null );

			Environment.SetEnvironmentVariable( "ue3.iPhone_DeveloperSigningIdentity",
				Builder.DeveloperSigningIdentityOverride != "" ?
				Builder.DeveloperSigningIdentityOverride :
				Builder.BranchDef.IPhone_DeveloperSigningIdentity );

			Environment.SetEnvironmentVariable( "ue3.iPhone_DistributionSigningIdentity",
				Builder.DistributionSigningIdentityOverride != "" ?
				Builder.DistributionSigningIdentityOverride :
				Builder.BranchDef.IPhone_DistributionSigningIdentity );

			bool bIPhoneCreateStubIPA = 
				Builder.BranchDef.IPhone_DeveloperSigningIdentity.Length > 0 ||
				Builder.BranchDef.IPhone_DistributionSigningIdentity.Length > 0;

			// If no signing identity has been set for this branch, don't generate any stub (which involves signing)
			Environment.SetEnvironmentVariable( "ue3.iPhone_CreateStubIPA",
				bIPhoneCreateStubIPA ? "true" : "false" );
		}

		private void SetMacEnvVar()
		{
			// All of the special incantations necessary for Mac building remotely on a Mac
			Environment.SetEnvironmentVariable( "ue3.Mac_CompileServerName", DefaultMacCompileServerName );
		}

		private void SetNGPSDKEnvVar()
		{
			string SCERootLocation = "C:\\Program Files (x86)\\SCE\\";
			string NGPSDKLocation = "C:\\Program Files (x86)\\SCE\\PSP2 SDKs\\" + NGPSDKVersion;

			Environment.SetEnvironmentVariable( "SCE_ROOT_DIR", SCERootLocation );
			Environment.SetEnvironmentVariable( "SCE_PSP2_SDK_DIR", NGPSDKLocation );
		}

		private void TrackFileSize()
		{
			try
			{
				FileInfo Info = new FileInfo( Builder.GetCurrentCommandLine() );
				if( Info.Exists )
				{
					string KeyName = Builder.GetCurrentCommandLine().Replace( '/', '_' );
					if( KeyName.Length > 0 )
					{
						WritePerformanceData( MachineName, Builder.BranchDef.Branch, KeyName, Info.Length, Builder.LabelInfo.Changelist );
					}
				}
			}
			catch
			{
			}
		}

		private void RecursiveGetFolderSize( DirectoryInfo Path, ref long TotalBuildSize )
		{
			foreach( FileInfo File in Path.GetFiles() )
			{
				TotalBuildSize += File.Length;
			}

			foreach( DirectoryInfo Dir in Path.GetDirectories() )
			{
				RecursiveGetFolderSize( Dir, ref TotalBuildSize );
			}
		}

		private void TrackFolderSize()
		{
			try
			{
				Log( "Tracking folder size of '" + Builder.GetCurrentCommandLine() + "'", Color.Black );
				DirectoryInfo Info = new DirectoryInfo( Builder.GetCurrentCommandLine() );
				if( Info.Exists )
				{
					string KeyName = Builder.GetCurrentCommandLine().Replace( '/', '_' );
					if( KeyName.Length > 0 )
					{
						long TotalBuildSize = 0;
						RecursiveGetFolderSize( Info, ref TotalBuildSize );
						WritePerformanceData( MachineName, Builder.BranchDef.Branch, KeyName, TotalBuildSize, Builder.LabelInfo.Changelist );

						Log( " ... size = " + TotalBuildSize + " bytes", Color.Black );
					}
				}
				else
				{
					Log( " ... folder does not exist!", Color.Black );
				}
			}
			catch
			{
			}
		}

		// Any cleanup that needs to happen when the build fails
		private void FailCleanup()
		{
			if( Builder.NewLabelCreated )
			{
				string LabelName = Builder.LabelInfo.GetLabelName();
				// Kill any jobs that are dependent on this label
				BuilderLinq.KillAssociatedJobs( LabelName );

				// Delete the partial label so that no one can get a bad sync
				SCC.DeleteLabel( Builder, LabelName );
				Builder.NewLabelCreated = false;
			}
		}

		// Cleanup that needs to happen on success or failure
		private void Cleanup()
		{
			// Revert all open files
			Builder.RevertAllFiles();

			BuilderLinq.SetBuildState( BuilderID, "Connected" );

			if( CommandID != 0 )
			{
				BuilderLinq.CleanupCommand( CommandID, MachineName );
			}
			else if( JobID != 0 )
			{
				BuilderLinq.MarkJobComplete( JobID );
			}

			// Set the DB up with the result of the build
			SetStatus( FinalStatus );
			BuilderLinq.SetBuildLogString( BuildLogID, "CurrentStatus", FinalStatus );

			LabelRoot = "Unknown";

			// Remove any active watchers
			Watcher = new Watcher( this );

			FinalStatus = "";
			Builder.Destroy();
			Builder = null;
			BlockingBuildID = 0;
			JobID = 0;
			NumJobsToWaitFor = 0;
			CommandID = 0;
			BuildLogID = 0;

			if( SystemLog != null )
			{
				SystemLog.Close();
				SystemLog = null;
			}
		}

		/** 
		 * Send any emails based on the current state of the build
		 */
		private void HandleEmails()
		{
			if( Builder.IsPromoting )
			{
				// There was a 'tag' command in the script
				Mailer.SendPromotedMail( Builder, CommandID );
			}
			else if( Builder.bIsPublishing )
			{
				// There was a 'publish' command in the script
				Mailer.SendPublishedMail( Builder, CommandID, BuildLogID );
			}
			else if( Builder.IsBuilding )
			{
				// There was a 'submit' command in the script
				Mailer.SendSucceededMail( Builder, CommandID, BuildLogID, Builder.GetChanges( "" ) );
			}
			else if( Builder.IsSendingQAChanges )
			{
				string[] PerforceUsers = Builder.GetPerforceUsers();
				foreach( string User in PerforceUsers )
				{
					Mailer.SendUserChanges( Builder, User );
				}
			}
			else if( Builder.CommandDetails.bIsPrimaryBuild )
			{
				Mailer.SendSucceededMail( Builder, CommandID, BuildLogID, Builder.GetChanges( "" ) );
			}
			else if( !Builder.CommandDetails.bIsPrimaryBuild && CommandID != 0 )
			{
				// For verification builds only, if the last time we attempted this build it did not succeed but it has succeeded this time,
				// send an email notifying the appropriate parties that the build now passes
				if( Builder.CommandDetails.LastAttemptedBuild != Builder.CommandDetails.LastGoodBuild )
				{
					Mailer.SendNewSuccessMail( Builder, CommandID, BuildLogID, Builder.GetChanges( "" ) );
				}
			}
		}

		private MODES HandleComplete()
		{
			BuilderLinq.LogBuildEnded( BuildLogID );

			if( CommandID != 0 )
			{
				HandleEmails();

				if( Builder.LabelInfo.Changelist >= Builder.CommandDetails.LastGoodBuild )
				{
					BuilderLinq.SetLastGoodBuild( CommandID, Builder.LabelInfo.Changelist );
					BuilderLinq.SetLastAttemptedBuild( CommandID, Builder.LabelInfo.Changelist );

					string Label = Builder.LabelInfo.GetLabelName();
					if( Builder.NewLabelCreated )
					{
						BuilderLinq.SetCommandString( CommandID, "LastGoodLabel", Label );
						BuilderLinq.SetBuildLogString( BuildLogID, "BuildLabel", Label );
					}

					if( Builder.IsPromoting )
					{
						BuilderLinq.SetCommandString( CommandID, "LastGoodLabel", Label );
					}
				}

				// Handle overall time
				string WatchTimer = "OverallBuildTime_" + CommandID.ToString();
				long OverallTime = Watcher.WatchStop( ref WatchTimer );

				// Write the perf data to the database
				WritePerformanceData( MachineName, Builder.BranchDef.Branch, WatchTimer, OverallTime, Builder.LabelInfo.Changelist );
			}
			else if( JobID != 0 )
			{
				BuilderLinq.MarkJobSucceeded( JobID, Builder.HasSuppressedErrors );
			}

			FinalStatus = "Succeeded";
			return ( MODES.Exit );
		}

		public string GetLabelToSync()
		{
			if( Builder.LabelInfo.RevisionType == RevisionType.Head )
			{
				Log( "Label revision: #head", Color.DarkGreen );
				return ( "#head" );
			}
			else if( Builder.LabelInfo.RevisionType == RevisionType.Label )
			{
				// If we have a valid label - use that
				string Label = Builder.LabelInfo.GetLabelName();
				Log( "Label revision: @" + Label, Color.DarkGreen );
				return ( "@" + Label );
			}
			else if( Builder.LabelInfo.RevisionType == RevisionType.SimpleLabel || Builder.LabelInfo.RevisionType == RevisionType.BuilderLabel  )
			{
				// If we have a valid simple label - use that
				string Label = Builder.Dependency;
				Log( "Label revision: @" + Label, Color.DarkGreen );
				return ( "@" + Label );
			}
			else
			{
				// ... or there may just be a changelist
				int Changelist = Builder.LabelInfo.Changelist;
				if( Changelist > 0 )
				{
					Log( "Changelist revision: @" + Changelist.ToString(), Color.DarkGreen );
					return ( "@" + Changelist.ToString() );
				}
			}

			// No label to sync - sync to head
			Builder.LabelInfo.RevisionType = RevisionType.Head;
			Log( "Invalid or nonexistent label, default: #head", Color.DarkGreen );
			return ( "#head" );
		}

		public string GetChangeListToSync()
		{
			if( Builder.LabelInfo.RevisionType == RevisionType.Label || Builder.LabelInfo.RevisionType == RevisionType.BuilderLabel )
			{
				string Changelist = Builder.LabelInfo.Changelist.ToString();
				Log( "Changelist revision: @" + Changelist, Color.DarkGreen );
				return ( "@" + Changelist );
			}

			// No label to sync - grab the changelist when we synced to #head
			if( Builder.HeadChangelist < 0 )
			{
				Builder.HeadChangelist = SCC.GetLatestChangelist( Builder, "//" + Builder.BranchDef.CurrentClient.ClientName + "/" + Builder.BranchDef.Branch + "/..." );
				Builder.LabelInfo.Changelist = Builder.HeadChangelist;
			}

			Log( "Using changelist at time of sync #head command", Color.DarkGreen );
			return ( "@" + Builder.HeadChangelist.ToString() );
		}

		private string GetTaggedMessage()
		{
			// Refresh the label info
			SCC.GetLabelInfo( Builder, Builder.LabelInfo.GetLabelName(), Builder.LabelInfo );

			string FailureMessage = Builder.LabelInfo.Description;
			int Index = Builder.LabelInfo.Description.IndexOf( "Job failed" );
			if( Index > 0 )
			{
				FailureMessage = FailureMessage.Substring( Index );
			}
			return ( FailureMessage );
		}

		public void AddJob( JobInfo Job )
		{
			string Define = Builder.LabelInfo.GetDefinesAsString();

			BuilderLinq.AddJob( Job.Name,						// Name
								Job.Command,					// Command
								Builder.LabelInfo.Platform,		// Platform
								Builder.LabelInfo.Game,			// Game
								Builder.BuildConfig,			// BuildConfiguration
								Builder.ScriptConfiguration,	// ScriptConfiguration
								Builder.LabelInfo.Language,		// Language
								Define,							// Define
								Job.Parameter,					// Parameter
								false,
								Builder.BranchDef.ID,			// Branch
								0,								// CIS task ID
								0,								// Job State ID
								Builder.Dependency,				// Dependency
								true,							// PrimaryBuild
								JobSpawnTime );					// Spawntime

			NumJobsToWaitFor++;

			Log( "Added job: " + Job.Name, Color.DarkGreen );
		}

		// Kill an in progress build
		private void KillBuild( int ID )
		{
			if( CommandID > 0 && CommandID == ID )
			{
				Log( "[STATUS] Killing build ...", Color.Red );

				// Kill the active command
				if( Builder != null )
				{
					Builder.KillCurrentCommand();
				}

				// Kill all associated jobs
				BuilderLinq.KillAssociatedJobs( JobSpawnTime );

				// Clean up
				FailCleanup();

				Mode = MODES.Exit;
				Log( "Process killed", Color.Red );

				string Killer = BuilderLinq.GetCommandString( CommandID, "Killer" );
				Mailer.SendKilledMail( Builder, CommandID, BuildLogID, Killer );

				Cleanup();
			}
		}

		private void KillJob( int ID )
		{
			if( JobID > 0 && JobID == ID )
			{
				Log( "[STATUS] Killing job ...", Color.Red );

				// Kill the active command
				if( Builder != null )
				{
					Builder.KillCurrentCommand();
				}

				Mode = MODES.Exit;
				Log( "Process killed", Color.Red );

				Cleanup();
			}
		}

		private void CleanupRogueProcesses( BranchDefinition BranchDefToCleanup )
		{
			bool bAnyProcessWasKilled = false;
			string WarningEmailText = "List of killed processes:\n";

			// Collect a list of all Unreal binaries that could be running to make sure they're killed
			string[] BinariesToLookFor = { };
			if( BranchDefToCleanup != null )
			{
				string BinariesDirectory = BranchDefToCleanup.CurrentClient.ClientRoot + "\\" + BranchDefToCleanup.Branch + "\\Binaries";
				if( Directory.Exists( BinariesDirectory ) )
				{
					BinariesToLookFor = Directory.GetFiles( BinariesDirectory, "*.exe", SearchOption.AllDirectories );
				}
			}

			// Do this loop as many times as necessary to clean out all rogue processes
			bool bAnyProcessWasKilledThisTime;
			do
			{
				bAnyProcessWasKilledThisTime = false;

				// Kill the crash dialog
				if( KillProcess( "WerFault" ) )
				{
					WarningEmailText += "    Killed 'WerFault' manually\n";
					bAnyProcessWasKilledThisTime = true;
				}
				// Kill the msdev crash dialog if it exists
				if( KillProcess( "DW20" ) )
				{
					WarningEmailText += "    Killed 'DW20' manually\n";
					bAnyProcessWasKilledThisTime = true;
				}
				// Kill the ProDG compiler if necessary
				if( KillProcess( "vsimake" ) )
				{
					WarningEmailText += "    Killed 'vsimake' manually\n";
					bAnyProcessWasKilledThisTime = true;
				}
				// Kill the command prompt
				if( KillProcess( "cmd" ) )
				{
					WarningEmailText += "    Killed 'cmd' manually\n";
					bAnyProcessWasKilledThisTime = true;
				}
				// Kill the linker
				if( KillProcess( "link" ) )
				{
					WarningEmailText += "    Killed 'link' manually\n";
					bAnyProcessWasKilledThisTime = true;
				}
				// Kill the XGE BuildSystem (should auto shutdown after the build is done)
				if( KillProcess( "BuildSystem" ) )
				{
					WarningEmailText += "    Killed 'BuildSystem' manually\n";
					bAnyProcessWasKilledThisTime = true;
				}

				// Iterate over all Unreal processes
				foreach( string NextBinaryToLookFor in BinariesToLookFor )
				{
					if( KillProcess( Path.GetFileNameWithoutExtension( NextBinaryToLookFor ) ) )
					{
						WarningEmailText += "    Killed '" + NextBinaryToLookFor + "' manually\n";
						bAnyProcessWasKilledThisTime = true;
					}
				}

				bAnyProcessWasKilled |= bAnyProcessWasKilledThisTime;
			}
			while( bAnyProcessWasKilledThisTime );
			
			// Finally report all processes killed manually
			if( bAnyProcessWasKilled )
			{
				Mailer.SendWarningMail( "Warning! Processes forcibly killed!", WarningEmailText, false );
			}
		}

		private void CreateSystemFolders()
		{
			// Delete any work folders (even if they have moved drives)
#if !DEBUG
			DeleteDirectory( "C:\\Builds", 0 );
			DeleteDirectory( "D:\\Builds", 0 );
			DeleteDirectory( "C:\\Install", 0 );
			DeleteDirectory( "D:\\Install", 0 );

			string TempFolder = Environment.GetEnvironmentVariable( "TEMP" );
			DeleteDirectory( TempFolder, 1 );

			TempFolder = Environment.GetEnvironmentVariable( "TMP" );
			DeleteDirectory( TempFolder, 1 );
#endif
			// Prefer the D: drive if it exists
			string RootDrive = "D:\\";
			if( !Directory.Exists( RootDrive ) )
			{
				RootDrive = "C:\\";
			}

			// Set the temp folders to the D: drive for speed and space
			Environment.SetEnvironmentVariable( "TEMP", RootDrive + "Temp" );
			Environment.SetEnvironmentVariable( "TMP", RootDrive + "Temp" );

			EnsureDirectoryExists( RootDrive + "Builds" );
			EnsureDirectoryExists( RootDrive + "Install" );

			// Also clean out Flash temporary files
			DeleteDirectory( "C:\\alchemy\\cygwin\\tmp", 1 );

			string NewTempFolder = Environment.GetEnvironmentVariable( "TEMP" );
			DeleteDirectory( NewTempFolder, 0 );

			NewTempFolder = Environment.GetEnvironmentVariable( "TMP" );
			DeleteDirectory( NewTempFolder, 0 );

			EnsureDirectoryExists( NewTempFolder );
		}

		private void CommonInit( BranchDefinition BranchDef )
		{
			string ScriptRoot = "Development/Builder/Scripts";
			if( BranchDef.Version >= 10 )
			{
				ScriptRoot = "Engine/Build/Scripts";
			}

			// Sync up the build scripts for all roots of this branch
			EnsureDirectoryExists( BranchDef.CurrentClient.ClientRoot + "\\" + BranchDef.Branch );
			Environment.CurrentDirectory = BranchDef.CurrentClient.ClientRoot + "\\" + BranchDef.Branch;

			// Before we spawn any work, make sure all rogue processes are killed off
			CleanupRogueProcesses( BranchDef );

			SCC.SyncBuildFiles( BranchDef, "/" + ScriptRoot + "/..." );
		}

		private void SpawnBuild( int ID )
		{
			SetMSVC9EnvVars();

			CreateSystemFolders();

			CommandID = ID;
			CompileRetryCount = 0;
			LinkRetryCount = 0;
			SyncRetryCount = 0;

			CommandInfo CommandDetails = BuilderLinq.GetCommandInfo( CommandID );
			if( CommandDetails == null )
			{
				Log( "ERRROR: Could not find command in available commands", Color.Red );
				Mode = MODES.Exit;
				return;
			}

			BranchDefinition BranchDef = FindBranchDefinition( CommandDetails.BranchConfigID );
			if( BranchDef == null )
			{
				Log( "ERRROR: Could not find branch in available branches", Color.Red );
				Mode = MODES.Exit;
				return;
			}

			CommonInit( BranchDef );

			// Make sure there are no pending kill commands
			BuilderLinq.CleanupCommand( CommandID, MachineName );

			Builder = new BuildState( this, SCC, CommandDetails, BranchDef );

			SystemLog = Builder.OpenSystemLog();

			// Send an email to the people subscribed to receive them
			Mailer.SendTriggeredMail( Builder );

			// Create a build log
			BuildLogID = BuilderLinq.CreateBuildLog( CommandDetails, BranchDef, MachineName );
			if( BuildLogID != 0 )
			{
				BuilderLinq.SetCommandActive( CommandID, BuildLogID, MachineName );
				BuilderLinq.SetBuildState( BuilderID, "Building" );
			}

			JobSpawnTime = DateTime.UtcNow.Ticks;
			Watcher.WatchStart( "OverallBuildTime_" + CommandID.ToString() );

			// Once we initialize everything, make sure we're starting with a clean slate by reverting everything that may be open for edit or delete.
			// Do this is easy way by simply enqueueing a revert as the first command.
			ClearCommands();
			Mode = MODES.Init;
		}

		private void SpawnJob( int ID )
		{
			SetMSVC9EnvVars();

			CreateSystemFolders();

			JobID = ID;

			CompileRetryCount = 0;
			LinkRetryCount = 0;
			SyncRetryCount = 0;

			CommandInfo CommandDetails = BuilderLinq.GetJobInfo( JobID );
			if( CommandDetails == null )
			{
				Log( "ERRROR: Could not find job in available jobs", Color.Red );
				Mode = MODES.Exit;
				return;
			}

			BranchDefinition BranchDef = FindBranchDefinition( CommandDetails.BranchConfigID );
			if( BranchDef == null )
			{
				Log( "ERRROR: Could not find branch in available branches", Color.Red );
				Mode = MODES.Exit;
				return;
			}

			CommonInit( BranchDef );

			Builder = new BuildState( this, SCC, CommandDetails, BranchDef );

			// Add a new entry with the command
			BuildLogID = BuilderLinq.CreateBuildLog( CommandDetails, BranchDef, MachineName );
			if( BuildLogID != 0 )
			{
				BuilderLinq.SetJobActive( JobID, BuildLogID, MachineName );
				BuilderLinq.SetBuildState( BuilderID, "Building" );
			}

			// Grab game and platform
			Builder.LabelInfo.Game = CommandDetails.Game;
			Builder.LabelInfo.Platform = CommandDetails.Platform;
			Builder.Dependency = CommandDetails.Label;

			// Handle the special case of CIS tasks passing in a numerical changelist
			if( CommandDetails.bIsPrimaryBuild )
			{
				// Standard, non-CIS case using the dependency as a standard label or folder
				Builder.LabelInfo.Init( SCC, Builder );
			}
			else
			{
				// Convert the dependency label to an integer changelist
				Builder.CommandDetails.LastAttemptedBuild = Builder.SafeStringToInt( Builder.Dependency );
				Builder.LabelInfo.Init( Builder, RevisionType.ChangeList );
			}

			// Once we initialize everything, make sure we're starting with a clean slate by reverting everything that may be open for edit or delete.
			// Do this is easy way by simply enqueueing a revert as the first command.
			ClearCommands();
			Mode = MODES.Init;
		}

		private void ClearCommands()
		{
			PendingCommands.Clear();

			SandboxedAction RevertCommand = new SandboxedAction( this, SCC, Builder );
			RevertCommand.CommandDelegate = RevertCommand.SCC_Revert;
			RevertCommand.CommandLine = "";
			SetNextCommand( RevertCommand );
		}

		private void RetryLastCommand()
		{
			SandboxedAction Current = Builder.GetCurrentCommand();
			PendingCommands.Insert( 0, Current );
		}

		private void AddCommandToEnd( SandboxedAction NextCommand )
		{
			PendingCommands.Add( NextCommand );
		}

		private void SetNextCommand( SandboxedAction NextCommand )
		{
			PendingCommands.Insert( 0, NextCommand );
		}

		private MODES HandleError()
		{
			int TimeOutMinutes;
			string GeneralStatus = "";
			string Status = "Succeeded";

			// Always free up the conch whenever a command finishes to let other builds finish
			BuilderLinq.SetCommandString( CommandID, "ConchHolder", "null" );
			Builder.StartWaitForConch = Builder.CommandDetails.BuildStarted;

			// Internal error?
			COMMANDS ErrorLevel = Builder.GetErrorLevel();

			if( Builder.HasSandboxedAction() && ErrorLevel == COMMANDS.None )
			{
				// ...or error that requires parsing the log
				ErrorLevel = Builder.GetCurrentState();

				LogParser Parser = new LogParser( this, Builder );
				bool ReportEverything = ( ErrorLevel >= COMMANDS.SCC_ArtistSync && ErrorLevel <= COMMANDS.SCC_TagMessage );
				Status = Parser.Parse( ReportEverything, Builder.IsCurrentlyCooking(), Builder.IsCurrentlyPublishing(), ref ErrorLevel );
			}

#if !DEBUG
			// If we were cooking, and didn't find the cooking success message, set to fail
			if( Builder.IsCurrentlyCooking() )
			{
				if( ErrorLevel == COMMANDS.CookingSuccess )
				{
					ErrorLevel = COMMANDS.None;
				}
				else if( ErrorLevel == COMMANDS.None && Status == "Succeeded" )
				{
					ErrorLevel = COMMANDS.CookMaps;
					Status = "Could not find cooking successful message";
				}
			}
#endif

			// If we were publishing, and didn't find the publish success message, set to fail
			if( Builder.IsCurrentlyPublishing() )
			{
				if( ErrorLevel == COMMANDS.CookerSyncSuccess )
				{
					ErrorLevel = COMMANDS.None;
				}
				else if( ErrorLevel == COMMANDS.None && Builder.PublishMode == BuildState.PublishModeType.Files && Status == "Succeeded" )
				{
					ErrorLevel = COMMANDS.Publish;
					Status = "Could not find publish completion message";
				}
			}

			// Check for total success
			if( ErrorLevel == COMMANDS.None && Status == "Succeeded" )
			{
				return ( MODES.Init );
			}

			// If we were checking to see if a file was signed, conditionally add another command
			if( ErrorLevel == COMMANDS.CheckSigned )
			{
				// Error checking to see if file was signed - so sign it
				if( Builder.GetCurrentExitCode() != 0 )
				{
					SandboxedAction SignCommand = new SandboxedAction( this, SCC, Builder );
					SignCommand.CommandDelegate = SignCommand.Sign;
					SignCommand.CommandLine = Builder.GetCurrentCommandLine();
					SetNextCommand( SignCommand );
				}
				return ( MODES.Init );
			}

			// Auto retry any p4 sync errors
			if( Status.Contains( "P4ERROR: Failed Perforce command 'sync'" ) )
			{
				if( SyncRetryCount < 3 )
				{
					SyncRetryCount++;

					Mailer.SendWarningMail( "Perforce Sync Error", "The Perforce server failed to sync properly; retry #" + SyncRetryCount.ToString() + Environment.NewLine + Environment.NewLine + Status, false );
					Builder.LineCount++;
					RetryLastCommand();
					return ( MODES.Init );
				}

				Status = Status + Environment.NewLine + Environment.NewLine + " ... after " + SyncRetryCount.ToString() + " retries.";
			}

			// Auto retry any proxy sync errors
			if( Status.Contains( "Proxy could not update its cache" ) )
			{
				if( SyncRetryCount < 3 )
				{
					SyncRetryCount++;

					Mailer.SendWarningMail( "Proxy cache failure", "The proxy server failed to update its cache; retry #" + SyncRetryCount.ToString() + Environment.NewLine + Environment.NewLine + Status, false );
					Builder.LineCount++;
					RetryLastCommand();
					return ( MODES.Init );
				}

				Status = Status + Environment.NewLine + Environment.NewLine + " ... after " + SyncRetryCount.ToString() + " retries.";
			}

			// Auto retry the connection to the Verisign timestamp server
			if( Status.Contains( "The specified timestamp server either could not be reached" ) )
			{
				Mailer.SendWarningMail( "Verisign connection failure", "The build machine failed to contact the Verisign timestamp server; retrying." + Environment.NewLine + Environment.NewLine + Status, false );
				Builder.LineCount++;
				RetryLastCommand();
				return ( MODES.Init );
			}

			// Deal with an XGE issue that causes some builds to fail immediately, but succeed on retry
			if( Status.Contains( "Fatal Error: Failed to initiate build" )
				|| Status.Contains( ": fatal error LNK1103:" )
				|| Status.Contains( ": fatal error LNK1106:" )
				|| Status.Contains( ": fatal error LNK1136:" )
				|| Status.Contains( ": fatal error LNK1143:" )
				|| Status.Contains( ": fatal error LNK1179:" )
				|| Status.Contains( ": fatal error LNK1183:" )
				|| Status.Contains( ": fatal error LNK1190:" )
				|| Status.Contains( ": fatal error LNK1215:" )
				|| Status.Contains( ": fatal error LNK1235:" )
				|| Status.Contains( ": fatal error LNK1248:" )
				|| Status.Contains( ": fatal error LNK1257:" )
				|| Status.Contains( ": fatal error LNK2022:" )
				|| Status.Contains( ": error: L0015:" )
				|| Status.Contains( ": error: L0072:" )
				|| Status.Contains( "corrupted section headers in file" )
				|| Status.Contains( ": warning LNK4019: corrupt string table" )
				|| Status.Contains( "Sorry but the link was not completed because memory was exhausted." )
				|| Status.Contains( "Another build is already started on this computer." )
				|| Status.Contains( "Failed to initialize Build System:" )
				|| Status.Contains( "simply rerunning the compiler might fix this problem" )
				|| Status.Contains( "Internal Linker Exception:" )
				// This message keeps appearing in windows ce builds - a rebuild seems to clean it up
				|| Status.Contains( ": unexpected error with pch, try rebuilding the pch" ) )
			{
				if( CompileRetryCount < 2 )
				{
					CompileRetryCount++;

					// Send a warning email
					if( CompileRetryCount == 1 )
					{
						// Restart the service and try again
						Mailer.SendWarningMail( "Compile (XGE) Failure #" + CompileRetryCount.ToString(), "XGE appears to have failed to start up; restarting the XGE service and trying again." + Environment.NewLine + Environment.NewLine + Status, false );

						Process ServiceStop = Process.Start( "net", "stop \"Incredibuild Agent\"" );
						ServiceStop.WaitForExit();

						Process ServiceStart = Process.Start( "net", "start \"Incredibuild Agent\"" );
						ServiceStart.WaitForExit();

						WritePerformanceData( MachineName, "Controller", "XGEServiceRestarted", 1, 0 );
					}
					else
					{
						// Queue up a retry without XGE
						Mailer.SendWarningMail( "Compile (XGE) Failure #" + CompileRetryCount.ToString(), "XGE appears to have failed to start up; retrying without XGE entirely." + Environment.NewLine + Environment.NewLine + Status, false );
						Builder.bXGEHasFailed = true;

						WritePerformanceData( MachineName, "Controller", "XGEServiceFailed", 1, 0 );
					}

					Builder.LineCount++;

					SandboxedAction UnrealBuildCommand = new SandboxedAction( this, SCC, Builder );
					UnrealBuildCommand.CommandDelegate = UnrealBuildCommand.UnrealBuild;
					UnrealBuildCommand.CommandLine = "";
					SetNextCommand( UnrealBuildCommand );

					SandboxedAction BuildUBT10Command = new SandboxedAction( this, SCC, Builder );
					BuildUBT10Command.CommandDelegate = BuildUBT10Command.BuildUBT10;
					BuildUBT10Command.CommandLine = "";
					SetNextCommand( BuildUBT10Command );

					SandboxedAction CleanCommand = new SandboxedAction( this, SCC, Builder );
					CleanCommand.CommandDelegate = CleanCommand.Clean;
					CleanCommand.CommandLine = "";
					SetNextCommand( CleanCommand );

					return ( MODES.Init );
				}

				Status = Status + Environment.NewLine + Environment.NewLine + " ... after " + CompileRetryCount.ToString() + " retries with and without XGE";
			}

			// If we had a failure while linking due to being unable to access the file,
			// retry the build, which should end up being only a relink
			if( Status.Contains( "fatal error LNK1201: error writing to program database" ) ||
				Status.Contains( "fatal error LNK1104: cannot open file" ) ||
				Status.Contains( "error: L0055: could not open output file" ) )
			{
				if( LinkRetryCount < 2 )
				{
					LinkRetryCount++;

					// Send a warning email
					Mailer.SendWarningMail( "Link Failure #" + LinkRetryCount.ToString(), "Couldn't write to the output file, sleeping then retrying." + Environment.NewLine + Environment.NewLine + Status, false );
					// Sleep for 10 seconds
					System.Threading.Thread.Sleep( 10 * 1000 );

					// Queue up a retry
					Builder.LineCount++;

					SandboxedAction UnrealBuildCommand = new SandboxedAction( this, SCC, Builder );
					UnrealBuildCommand.CommandDelegate = UnrealBuildCommand.UnrealBuild;
					UnrealBuildCommand.CommandLine = "";
					SetNextCommand( UnrealBuildCommand );

					return ( MODES.Init );
				}

				Status = Status + Environment.NewLine + Environment.NewLine + " ... after " + LinkRetryCount.ToString() + " link retries";
			}

			if( ErrorLevel == COMMANDS.NetworkVanished )
			{
				Status = "Network connection disappeared (The specified network name is no longer available)";
			}

			// Error found, but no description located. Explain this.
			if( Status == "Succeeded" )
			{
				Status = "Could not find error";
			}

			// Handle suppression and ignoring of errors
			switch( Builder.ErrorMode )
			{
			// Completely ignore the error and continue
			case BuildState.ErrorModeType.IgnoreErrors:
				return ( MODES.Init );

			// Ignore the error, but send an informative email
			case BuildState.ErrorModeType.SuppressErrors:
				Mailer.SendSuppressedMail( Builder, CommandID, BuildLogID, Status );
				Builder.HasSuppressedErrors = true;
				return ( MODES.Init );

			// Continue to handle the failure
			case BuildState.ErrorModeType.CheckErrors:
				break;
			}

			// Copy the failure log to a common network location
			FileInfo LogFile = new FileInfo( "None" );
			if( Builder.LogFileName.Length > 0 )
			{
				LogFile = new FileInfo( Builder.LogFileName );
				try
				{
					if( LogFile.Exists )
					{
						// Make sure destination directory exists
						string LogDirectoryName = Properties.Settings.Default.FailedLogLocation + "/" + Builder.BranchDef.Branch;
						if( !Directory.Exists( LogDirectoryName ) )
						{
							Directory.CreateDirectory( LogDirectoryName );
						}
						LogFile.CopyTo( LogDirectoryName + "/" + LogFile.Name );
					}
				}
				catch
				{
				}
			}

			// If we wish to save the error string in the database, do so now
			if( Builder.CommandDetails.CISJobStateID > 0 )
			{
				// Testing race condition theory
				BuilderLinq.MarkJobComplete( JobID );

				string ErrorMessage = Status.Replace( '\'', '\"' );
				BuilderLinq.UpdateChangelist( Builder.CommandDetails.CISJobStateID, JobState.Failed, ErrorMessage );

				ErrorMessage += Environment.NewLine + "<a href=\"file:///" + Properties.Settings.Default.FailedLogLocation.Replace( '\\', '/' ) + "/" + Builder.BranchDef.Branch + "/" + LogFile.Name + "\"> Click here for the detailed log </a>";
			}

			// Set the end time and detailed log path as we've now finished
			BuilderLinq.LogBuildEnded( BuildLogID );
			BuilderLinq.SetBuildLogString( BuildLogID, "DetailedLogPath", LogFile.Name );

			if( CommandID != 0 )
			{
				// Update the commands table for this failure
				BuilderLinq.SetLastFailedBuild( CommandID, Builder.LabelInfo.Changelist );

				// Special work for non-CIS (Jobs) verification builds
				if( !Builder.CommandDetails.bIsPrimaryBuild )
				{
					// Report only if we actually attempted something new, or if
					// this is a CIS verification job 
					if( Builder.CommandDetails.LastAttemptedBuild == Builder.LabelInfo.Changelist )
					{
						ErrorLevel = COMMANDS.SilentlyFail;
					}
				}
			}

			// Try to explain the error
			string Explanation = LogParser.ExplainError( Status );
			if( Explanation.Length > 0 )
			{
				Status += Environment.NewLine;
				Status += Explanation;
			}

			// Be sure to update the DB with this attempt 
			BuilderLinq.SetLastAttemptedBuild( CommandID, Builder.LabelInfo.Changelist );

			// Handle specific errors
			switch( ErrorLevel )
			{
			case COMMANDS.None:
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );

				Log( "LOG ERROR: " + Builder.GetCurrentState() + " " + Builder.GetCurrentCommandLine() + " failed", Color.Red );
				Log( Status, Color.Red );
				break;

			case COMMANDS.SilentlyFail:
				Log( "LOG ERROR: " + Builder.GetCurrentState() + " " + Builder.GetCurrentCommandLine() + " failed", Color.Red );
				Log( "LOG ERROR: failing silently (without email) because this is a verification build that doesn't require direct reporting", Color.Red );
				Log( Status, Color.Red );
				break;

			case COMMANDS.NoScript:
				Status = "No build script";
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.IllegalCommand:
				Status = "Illegal command: '" + Builder.GetCurrentCommandLine() + "'";
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.SCC_Submit:
			case COMMANDS.SCC_FileLocked:
			case COMMANDS.SCC_Sync:
			case COMMANDS.SCC_Checkout:
			case COMMANDS.SCC_Revert:
			case COMMANDS.SCC_Tag:
			case COMMANDS.SCC_GetClientRoot:
			case COMMANDS.CriticalError:
			case COMMANDS.ValidateInstall:
				GeneralStatus = Builder.GetCurrentState() + " " + Builder.GetCurrentCommandLine() + " failed with error '" + ErrorLevel.ToString() + "'";
				GeneralStatus += Environment.NewLine + Environment.NewLine + Status;
				Log( "ERROR: " + GeneralStatus, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, GeneralStatus, LogFile.Name );
				break;

			case COMMANDS.Process:
			case COMMANDS.MSVC9Clean:
			case COMMANDS.MSVC9Build:
			case COMMANDS.MSVC9Deploy:
			case COMMANDS.MSVC10Clean:
			case COMMANDS.MSVC10Build:
			case COMMANDS.MSVC10Deploy:
			case COMMANDS.ShaderBuild:
			case COMMANDS.CookShaderBuild:
			case COMMANDS.ShaderClean:
			case COMMANDS.BuildScript:
			case COMMANDS.BuildScriptNoClean:
			case COMMANDS.CookMaps:
			case COMMANDS.CheckForUCInVCProjFiles:
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.Publish:
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.CheckSigned:
				return MODES.Init;

			case COMMANDS.TimedOut:
				TimeOutMinutes = ( int )Builder.GetTimeout().TotalMinutes;
				GeneralStatus = "'" + Builder.GetCurrentState() + " " + Builder.GetCurrentCommandLine() + "' TIMED OUT after " + TimeOutMinutes.ToString() + " minutes";
				GeneralStatus += Environment.NewLine + Environment.NewLine + "(This normally means a child process crashed)";
				GeneralStatus += Environment.NewLine + Environment.NewLine + Status;
				Log( "ERROR: " + GeneralStatus, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, GeneralStatus, LogFile.Name );
				break;

			case COMMANDS.WaitTimedOut:
				TimeOutMinutes = ( int )Builder.GetTimeout().TotalMinutes;
				Status = "Waiting for '" + Builder.GetCurrentCommandLine() + "' TIMED OUT after " + TimeOutMinutes.ToString() + " minutes";
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.WaitJobsTimedOut:
				TimeOutMinutes = ( int )Builder.GetTimeout().TotalMinutes;
				int JobCount = BuilderLinq.GetCompletedJobsCount( JobSpawnTime );
				Status = "Waiting for " + ( NumJobsToWaitFor - JobCount ).ToString() + " job(s) out of " + NumJobsToWaitFor.ToString() + " TIMED OUT after " + TimeOutMinutes.ToString() + " minutes";
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.FailedJobs:
				Status = "All jobs completed, but one or more failed." + Environment.NewLine + GetTaggedMessage();
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			case COMMANDS.Crashed:
				int NotRespondingMinutes = ( int )Builder.GetRespondingTimeout().TotalMinutes;
				Status = "'" + Builder.GetCurrentState() + " " + Builder.GetCurrentCommandLine() + "' was not responding for " + NotRespondingMinutes.ToString() + " minutes; presumed crashed.";
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;

			default:
				Status = "'" + Builder.GetCurrentState() + " " + Builder.GetCurrentCommandLine() + "' unhandled error '" + ErrorLevel.ToString() + "'";
				Log( "ERROR: " + Status, Color.Red );
				Mailer.SendFailedMail( Builder, CommandID, BuildLogID, Status, LogFile.Name );
				break;
			}

			// Any cleanup that needs to happen only in a failure case
			FailCleanup();

			FinalStatus = "Failed";
			return ( MODES.Exit );
		}

		private bool RunBuild()
		{
			// Start off optimistic 
			bool StillRunning = true;

			switch( Mode )
			{
			case MODES.Init:
				// Get a new command ...
				if( PendingCommands.Count > 0 )
				{
					// ... either pending 
					Builder.SetCurrentCommand( PendingCommands[0] );
					PendingCommands.RemoveAt( 0 );
				}
				else
				{
					// ... or from the script
					Builder.ParseNextLine();
				}

				// Expand out any variables
				Builder.ExpandCommandLine();

				// Set up the required env
				if( Builder.CommandIsMSVC9Specific() )
				{
					SetMSVC9EnvVars();
				}
				else if( Builder.CommandIsMSVC10Specific() )
				{
					SetMSVC10EnvVars();
				}

				SetDXSDKEnvVar();

				SetIPhoneEnvVar();

				SetMacEnvVar();

				SetNGPSDKEnvVar();

				switch( Builder.GetCurrentState() )
				{
				case COMMANDS.Error:
					Mode = MODES.Finalise;
					break;

				case COMMANDS.Finished:
					Mode = HandleComplete();
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.Config:
					break;

				case COMMANDS.SendEmail:
					HandleEmails();
					Mode = MODES.Finalise;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.Wait:
					BlockingBuildID = BuilderLinq.GetParentBuild( Builder.GetCurrentCommandLine() );
					BlockingBuildStartTime = DateTime.UtcNow;
					Mode = MODES.Wait;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.WaitForJobs:
					Log( "[STATUS] Waiting for " + ( NumJobsToWaitFor - NumCompletedJobs ).ToString() + "/" + NumJobsToWaitFor.ToString() + " jobs.", Color.Magenta );
					BlockingBuildStartTime = DateTime.UtcNow;
					Mode = MODES.WaitForJobs;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.WaitTargetState:
					Log( "[STATUS] Waiting for target state: " + Builder.GetCurrentCommandLine(), Color.Magenta );
					ConsoleStateStartTime = DateTime.UtcNow;
					Mode = MODES.WaitTargetState;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SetDependency:
					int DependentCommandID = BuilderLinq.GetParentBuild( Builder.Dependency );
					if( DependentCommandID == 0 )
					{
						Builder.LabelInfo.Init( SCC, Builder );
					}
					else
					{
						Builder.CommandDetails.LastAttemptedBuild = BuilderLinq.GetLastGoodChangelist( DependentCommandID );
						Builder.LabelInfo.Init( Builder, RevisionType.ChangeList );
					}
					Mode = MODES.Finalise;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.CleanMacs:
					Queue<string> MacsToClean = new Queue<string>();

					if( DefaultIPhoneCompileServerName.Length > 0 )
					{
						MacsToClean.Enqueue( DefaultIPhoneCompileServerName );
					}

					if( DefaultIPhoneSigningServerName.Length > 0 )
					{
						MacsToClean.Enqueue( DefaultIPhoneSigningServerName );
					}

					if( DefaultMacCompileServerName.Length > 0 )
					{
						MacsToClean.Enqueue( DefaultMacCompileServerName );
					}

					if( Builder.iPhoneCompileServerOverride.Length > 0 )
					{
						MacsToClean.Enqueue( Builder.iPhoneCompileServerOverride );
					}

					if( Builder.iPhoneSigningServerOverride.Length > 0 )
					{
						MacsToClean.Enqueue( Builder.iPhoneSigningServerOverride );
					}

					while( MacsToClean.Count > 0 )
					{
						SandboxedAction CleanMacCommand = new SandboxedAction( this, SCC, Builder );
						CleanMacCommand.CommandDelegate = CleanMacCommand.CleanMac;
						CleanMacCommand.CommandLine = MacsToClean.Dequeue();
						SetNextCommand( CleanMacCommand );
					}
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.RestartSteamServer:
					SandboxedAction StopSteamServerCommand = new SandboxedAction( this, SCC, Builder );
					StopSteamServerCommand.CommandDelegate = StopSteamServerCommand.StopSteamServer;
					StopSteamServerCommand.CommandLine = "";
					Builder.SetCurrentCommand( StopSteamServerCommand );
					Mode = Builder.ExecuteCommand();

					SandboxedAction StartSteamServerCommand = new SandboxedAction( this, SCC, Builder );
					StartSteamServerCommand.CommandDelegate = StartSteamServerCommand.StartSteamServer;
					StartSteamServerCommand.CommandLine = "";
					SetNextCommand( StartSteamServerCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SteamDRM:
					SandboxedAction SteamDRMCommand = new SandboxedAction( this, SCC, Builder );
					SteamDRMCommand.CommandDelegate = SteamDRMCommand.SteamDRM;
					SteamDRMCommand.CommandLine = "";
					Builder.SetCurrentCommand( SteamDRMCommand );
					Mode = Builder.ExecuteCommand();

					SandboxedAction FixupSteamDRMCommand = new SandboxedAction( this, SCC, Builder );
					FixupSteamDRMCommand.CommandDelegate = FixupSteamDRMCommand.FixupSteamDRM;
					FixupSteamDRMCommand.CommandLine = "";
					SetNextCommand( FixupSteamDRMCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.MSVC9Full:
					SandboxedAction MSVC9CleanCommand = new SandboxedAction( this, SCC, Builder );
					MSVC9CleanCommand.CommandDelegate = MSVC9CleanCommand.MSVC9Clean;
					MSVC9CleanCommand.CommandLine = Builder.GetCurrentCommandLine();
					Builder.SetCurrentCommand( MSVC9CleanCommand );
					Mode = Builder.ExecuteCommand();

					SandboxedAction MSVC9BuildCommand = new SandboxedAction( this, SCC, Builder );
					MSVC9BuildCommand.CommandDelegate = MSVC9BuildCommand.MSVC9Build;
					MSVC9BuildCommand.CommandLine = Builder.GetCurrentCommandLine();
					SetNextCommand( MSVC9BuildCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.MSVC10Full:
					SandboxedAction MSVC10CleanCommand = new SandboxedAction( this, SCC, Builder );
					MSVC10CleanCommand.CommandDelegate = MSVC10CleanCommand.MSVC10Clean;
					MSVC10CleanCommand.CommandLine = Builder.GetCurrentCommandLine();
					Builder.SetCurrentCommand( MSVC10CleanCommand );
					Mode = Builder.ExecuteCommand();

					SandboxedAction MSVC10BuildCommand = new SandboxedAction( this, SCC, Builder );
					MSVC10BuildCommand.CommandDelegate = MSVC10BuildCommand.MSVC10Build;
					MSVC10BuildCommand.CommandLine = Builder.GetCurrentCommandLine();
					SetNextCommand( MSVC10BuildCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.ShaderFull:
					SandboxedAction ShaderCleanCommand = new SandboxedAction( this, SCC, Builder );
					ShaderCleanCommand.CommandDelegate = ShaderCleanCommand.ShaderClean;
					ShaderCleanCommand.CommandLine = Builder.GetCurrentCommandLine();
					Builder.SetCurrentCommand( ShaderCleanCommand );
					Mode = Builder.ExecuteCommand();

					SandboxedAction ShaderBuildCommand = new SandboxedAction( this, SCC, Builder );
					ShaderBuildCommand.CommandDelegate = ShaderBuildCommand.ShaderBuild;
					ShaderBuildCommand.CommandLine = Builder.GetCurrentCommandLine();
					SetNextCommand( ShaderBuildCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.ExtractSHAs:
					SandboxedAction ExtractSHAsCommand = new SandboxedAction( this, SCC, Builder );
					ExtractSHAsCommand.CommandDelegate = ExtractSHAsCommand.ExtractSHAs;
					ExtractSHAsCommand.CommandLine = Builder.GetCurrentCommandLine();
					Builder.SetCurrentCommand( ExtractSHAsCommand );
					Mode = Builder.ExecuteCommand();

					SandboxedAction CheckSHAsCommand = new SandboxedAction( this, SCC, Builder );
					CheckSHAsCommand.CommandDelegate = CheckSHAsCommand.CheckSHAs;
					CheckSHAsCommand.CommandLine = Builder.GetCurrentCommandLine();
					SetNextCommand( CheckSHAsCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SCC_CheckoutManifest:
					Queue<string> Games = new Queue<string>( Builder.LabelInfo.Game.Split( " ".ToCharArray() ) );

					SandboxedAction GenerateManifestCommand = new SandboxedAction( this, SCC, Builder );
					GenerateManifestCommand.CommandDelegate = GenerateManifestCommand.GenerateManifest;
					GenerateManifestCommand.CommandLine = Games.Dequeue();
					Builder.SetCurrentCommand( GenerateManifestCommand );
					Mode = Builder.ExecuteCommand();

					foreach( string Game in Games )
					{
						SandboxedAction GenerateMergedManifestCommand = new SandboxedAction( this, SCC, Builder );
						GenerateMergedManifestCommand.CommandDelegate = GenerateMergedManifestCommand.GenerateManifest;
						GenerateMergedManifestCommand.CommandLine = Game + " -MergeManifests";
						AddCommandToEnd( GenerateMergedManifestCommand );
					}

					SandboxedAction CheckoutManifestCommand = new SandboxedAction( this, SCC, Builder );
					CheckoutManifestCommand.CommandDelegate = CheckoutManifestCommand.SCC_CheckoutManifest;
					CheckoutManifestCommand.CommandLine = "";
					AddCommandToEnd( CheckoutManifestCommand );

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SCC_SignUnsubmitted:
					if( SCC.Pending != null )
					{
						// Find all the exes, dlls and cats that are currently checked out - and sign them
						List<string> CheckedOutFiles = SCC.GetCheckedOutFiles( Builder );

						foreach( string CheckedOutFile in CheckedOutFiles )
						{
							string Extension = Path.GetExtension( CheckedOutFile ).ToLower();
							if( Extension == ".exe" || Extension == ".dll" || Extension == ".com" )
							{
								SandboxedAction CheckSignedCommand = new SandboxedAction( this, SCC, Builder );
								CheckSignedCommand.CommandDelegate = CheckSignedCommand.CheckSigned;
								CheckSignedCommand.CommandLine = CheckedOutFile;
								AddCommandToEnd( CheckSignedCommand );
							}
						}

						SandboxedAction SubmitCommand = new SandboxedAction( this, SCC, Builder );
						SubmitCommand.CommandDelegate = SubmitCommand.SCC_Submit;
						SubmitCommand.CommandLine = "";
						AddCommandToEnd( SubmitCommand );
					}

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.UpdateSourceServer:
					if( Builder.LabelInfo.Platform.ToLower() == "xbox360" ||
						Builder.LabelInfo.Platform.ToLower() == "win32" ||
						Builder.LabelInfo.Platform.ToLower() == "win64" )
					{
						SandboxedAction UpdateSourceServerCommand = new SandboxedAction( this, SCC, Builder );
						UpdateSourceServerCommand.CommandDelegate = UpdateSourceServerCommand.UpdateSourceServer;
						UpdateSourceServerCommand.CommandLine = "";
						Builder.SetCurrentCommand( UpdateSourceServerCommand );
						Mode = Builder.ExecuteCommand();
					}
					else
					{
						Log( "Suppressing UpdateSourceServer for " + Builder.LabelInfo.Platform, Color.DarkGreen );
						Mode = MODES.Finalise;
					}

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.Trigger:
				case COMMANDS.AutoTrigger:
					string BuildType = Builder.GetCurrentCommandLine();
					Log( "[STATUS] Triggering build '" + BuildType + "'", Color.Magenta );
					BuilderLinq.Trigger( CommandID, BuildType, Builder.GetCurrentState() == COMMANDS.AutoTrigger );
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.CleanTrigger:
					string CleanBuildType = Builder.GetCurrentCommandLine();
					if( !Builder.HasSuppressedErrors )
					{
						Log( "[STATUS] Triggering build '" + CleanBuildType + "'", Color.Magenta );
						BuilderLinq.Trigger( CommandID, CleanBuildType, Builder.GetCurrentState() == COMMANDS.AutoTrigger );
					}
					else
					{
						Log( "[STATUS] Suppressing trigger of build '" + CleanBuildType + "' due to there being suppressed errors in the current build.", Color.Magenta );
					}

					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SQLExecInt:
					string SQLExecIntFunction = Builder.GetCurrentCommandLine();
					int SQLExecIntResult = BuilderLinq.GenericStoredProcedure<int>( SQLExecIntFunction );
					Builder.AddToSuccessStatus( SQLExecIntFunction + " : " + SQLExecIntResult.ToString() );
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SQLExecDouble:
					string SQLExecDoubleFunction = Builder.GetCurrentCommandLine();
					double SQLExecDoubleResult = ( double )BuilderLinq.GenericStoredProcedure<decimal>( SQLExecDoubleFunction );
					Builder.AddToSuccessStatus( SQLExecDoubleFunction + " : " + SQLExecDoubleResult.ToString( "F2" ) );
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.UpdateLabel:
					string LabelCmdLine = Builder.GetCurrentCommandLine();
					BuilderLinq.UpdateVariable( Builder.BranchDef.ID, LabelCmdLine, Builder.LabelInfo.GetLabelName() );
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.UpdateFolder:
					string FolderCmdLine = Builder.GetCurrentCommandLine();
					BuilderLinq.UpdateVariable( Builder.BranchDef.ID, FolderCmdLine, Builder.GetFolderName() );
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.Publish:
				case COMMANDS.FTPSendFile:
				case COMMANDS.FTPSendFolder:
				case COMMANDS.FTPSendImage:
					if( Builder.BlockOnPublish )
					{
						// Check for available bandwidth every 3 seconds
						if( DateTime.UtcNow - Builder.LastConchCheck > new TimeSpan( 0, 0, 3 ) )
						{
							Builder.LastConchCheck = DateTime.UtcNow;

							if( !BuilderLinq.AvailableBandwidth( CommandID ) )
							{
								RetryLastCommand();

								if( Builder.StartWaitForConch == Builder.CommandDetails.BuildStarted )
								{
									Builder.StartWaitForConch = DateTime.UtcNow;
									Builder.LastWaitForConchUpdate = Builder.StartWaitForConch;

									Log( "[STATUS] Waiting for bandwidth ( 00:00:00 )", Color.Yellow );
								}
								else if( DateTime.UtcNow - Builder.LastWaitForConchUpdate > new TimeSpan( 0, 0, 5 ) )
								{
									Builder.LastWaitForConchUpdate = DateTime.UtcNow;
									TimeSpan Taken = DateTime.UtcNow - Builder.StartWaitForConch;
									Log( "[STATUS] Waiting for bandwidth ( " + Taken.Hours.ToString( "00" ) + ":" + Taken.Minutes.ToString( "00" ) + ":" + Taken.Seconds.ToString( "00" ) + " )", Color.Yellow );
								}
							}
							else
							{
								Log( "[STATUS] Bandwidth acquired - publishing/ftping!", Color.Magenta );

								Mode = Builder.ExecuteCommand();
								Builder.SetCurrentState( COMMANDS.None );
							}
						}
						else
						{
							RetryLastCommand();
						}
					}
					else
					{
						Mode = Builder.ExecuteCommand();
						Builder.SetCurrentState( COMMANDS.None );
					}
					break;

				case COMMANDS.GetChangelist:
					GetChangeListToSync();
					Mode = MODES.Finalise;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.CISProcessP4Changes:
					ProcessP4Changes();
					Mode = MODES.Finalise;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.CISUpdateMonitorValues:
					UpdateMonitorValues();
					Mode = MODES.Finalise;
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.TrackFileSize:
					TrackFileSize();
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.TrackFolderSize:
					TrackFolderSize();
					Builder.SetCurrentState( COMMANDS.None );
					break;

				case COMMANDS.SCC_Sync:
					SandboxedAction SyncCommand = new SandboxedAction( this, SCC, Builder );
					SyncCommand.CommandDelegate = SyncCommand.SCC_Sync;
					SyncCommand.CommandLine = "";
					Builder.SetCurrentCommand( SyncCommand );
					Mode = Builder.ExecuteCommand();

					BuilderLinq.SetBuildLogChangelist( BuildLogID, Builder.LabelInfo.Changelist );
					Builder.SetCurrentState( COMMANDS.None );
					break;

				default:
					// Fire off a new process safely
					Mode = Builder.ExecuteCommand();
					break;
				}
				break;

			case MODES.Monitor:
				// Check for completion
				Mode = Builder.IsCurrentlyFinished();
				break;

			case MODES.Wait:
				if( BlockingBuildID != 0 )
				{
					// Has the child build been updated to the same build?
					int LastGoodChangeList = BuilderLinq.GetLastGoodChangelist( BlockingBuildID );
					if( LastGoodChangeList >= Builder.LabelInfo.Changelist )
					{
						Mode = MODES.Finalise;
					}
					else
					{
						DateTime? BlockingBuildStartTime = BuilderLinq.GetBlockingBuildStartTime( BlockingBuildID );

						// Check to see if the build timed out (default time is when wait was started)
						if( BlockingBuildStartTime != null && DateTime.UtcNow - BlockingBuildStartTime > Builder.GetTimeout() )
						{
							Builder.SetErrorLevel( COMMANDS.WaitTimedOut );
							Mode = MODES.Finalise;
						}
					}
				}
				else
				{
					Mode = MODES.Finalise;
				}
				break;

			case MODES.WaitForFTP:
				// Check every 5 seconds
				TimeSpan WaitForFTPTime = new TimeSpan( 0, 0, 5 );
				if( DateTime.UtcNow - LastCheckForJobTime > WaitForFTPTime )
				{
					if( Builder.ManageFTPThread == null )
					{
						Builder.CloseLog();
						Mode = MODES.Finalise;
					}
					else if( !Builder.ManageFTPThread.IsAlive )
					{
						Builder.ManageFTPThread = null;
						Builder.CloseLog();
						Mode = MODES.Finalise;
					}
					// Check to see if waiting for the build timed out
					else if( Builder.IsCurrentlyTimedOut() )
					{
						Builder.SetErrorLevel( COMMANDS.WaitTimedOut );
						Builder.ManageFTPThread.Abort();
						Builder.ManageFTPThread = null;
						Builder.SetCurrentCommandLine( "FTP send from " + Builder.GetCurrentCommandLine() );
						Builder.CloseLog();
						Mode = MODES.Finalise;
					}

					LastCheckForJobTime = DateTime.UtcNow;
				}
				break;

			case MODES.WaitForJobs:
				// Check every 5 seconds
				TimeSpan WaitForJobsTime = new TimeSpan( 0, 0, 5 );
				if( DateTime.UtcNow - LastCheckForJobTime > WaitForJobsTime )
				{
					int Count = BuilderLinq.GetCompletedJobsCount( JobSpawnTime );
					if( NumCompletedJobs != Count )
					{
						int RemainingJobs = NumJobsToWaitFor - Count;
						Log( "[STATUS] Waiting for " + RemainingJobs.ToString() + "/" + NumJobsToWaitFor.ToString() + " jobs.", Color.Magenta );
						NumCompletedJobs = Count;
					}

					// See if we're done waiting yet
					if( Count == NumJobsToWaitFor )
					{
						// Check the status of jobs that have completed
						Count = BuilderLinq.GetSucceededJobsCount( JobSpawnTime );
						if( Count != NumJobsToWaitFor )
						{
							Builder.SetErrorLevel( COMMANDS.FailedJobs );
						}

						// Find suppressed error jobs and add to Builder.LabelInfo.FailedGames
						List<SuppressedJobsInfo> SuppressedJobs = BuilderLinq.FindSuppressedJobs( JobSpawnTime );
						if( SuppressedJobs.Count > 0 )
						{
							foreach( SuppressedJobsInfo SuppressedJob in SuppressedJobs )
							{
								GameConfig GameInfo = new GameConfig( SuppressedJob.Game, SuppressedJob.Platform, SuppressedJob.Config, null, true, true );
								Builder.LabelInfo.FailedGames.Add( GameInfo );
							}

							Builder.HasSuppressedErrors = true;
						}

						NumJobsToWaitFor = 0;
						JobSpawnTime = DateTime.UtcNow.Ticks;
						Mode = MODES.Finalise;
					}
					else if( DateTime.UtcNow - BlockingBuildStartTime > Builder.GetTimeout() )
					{
						// Check to see if the build timed out
						Builder.SetErrorLevel( COMMANDS.WaitJobsTimedOut );
						Mode = MODES.Finalise;
					}

					LastCheckForJobTime = DateTime.UtcNow;
				}
				break;

			case MODES.WaitTargetState:
				break;

			case MODES.Finalise:
				// Analyze logs and restart or exit
				Mode = HandleError();
				break;

			case MODES.Exit:
				Cleanup();
				StillRunning = false;
				break;
			}

			return StillRunning;
		}

		/** 
		 * Main loop tick - returns true if a build is in progress
		 */
		public bool Run()
		{
			// Ping the server to say we're still alive every 30 seconds
			MaintainMachine();

			if( BuildLogID != 0 || JobID != 0 )
			{
				bool StillRunning = RunBuild();
				if( StillRunning )
				{
					int ID = PollForKillBuild();
					if( ID != 0 )
					{
						KillBuild( ID );
					}

					ID = PollForKillJob();
					if( ID != 0 )
					{
						KillJob( ID );
					}
				}

				return StillRunning;
			}
			else
			{
				if( Ticking )
				{
					// Poll the DB for commands

					// Poll for Primary Builds
					int ID = PollForBuild( true );
					if( ID > 0 )
					{
						SpawnBuild( ID );
						return true;
					}

					// Poll for Primary Jobs
					ID = PollForJob( true );
					if( ID > 0 )
					{
						SpawnJob( ID );
						return true;
					}

					// Poll for Non-Primary Builds
					ID = PollForBuild( false );
					if( ID > 0 )
					{
						SpawnBuild( ID );
						return true;
					}

					// Poll for Non-Primary Jobs
					ID = PollForJob( false );
					if( ID > 0 )
					{
						SpawnJob( ID );
						return true;
					}
				}
			}

			return false;
		}

		private void Main_FormClosed( object sender, FormClosedEventArgs e )
		{
			BuilderLinq.SetCommandString( CommandID, "Killer", "LocalUser" );

			KillBuild( CommandID );
			Ticking = false;
		}
	}
}
