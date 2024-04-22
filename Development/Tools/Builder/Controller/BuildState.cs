// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;
using System.Threading;
using P4API;
using Ionic.Zip;
using UnrealControls;

namespace Controller
{
    public partial class BuildState
    {
        public enum COLLATION
        {
			Engine,
			Rendering,
			Audio,
			Editor,
			GFx,
			Game	= 10,

            Windows = 100,
			Xbox360,
            PS3,
			WiiU,
			NGP,
			iPhone,
			Android,
			Mac,
			Flash,
			Dingo,
			Mocha,

			OnlinePC,
			GameCenter,
			GameSpy,
			G4WLive,
			Steam,

			Infrastructure,
			Swarm,
			Lightmass,
			Tools,
			Install,
			Loc,

			Count,
        }

        public class ChangeList
        {
            public int Number;
            public string User;
            public int Time;
            public string Description;
            public COLLATION Collate;
            public List<string> Files = new List<string>();

            public ChangeList()
            {
                Number = 0;
                Time = 0;
                Collate = COLLATION.Count;
            }

            public void CleanDescription()
            {
				string WorkDescription = Description.Replace( "\r", "" ).Trim();

				// Replace any characters that could mess up a SQL query
				WorkDescription = WorkDescription.Replace( "\'", "" );
				WorkDescription = WorkDescription.Replace( "\"", "" );

				Description = "";

				// Process lines in their entirety
				string[] Lines = WorkDescription.Split( "\n".ToCharArray() );
				foreach( string Line in Lines )
				{
					// Skip any code review lines
					if( Line.ToLower().StartsWith( "#codereview" ) )
					{
						continue;
					}

					// Parse each word on the line
	                string[] Parms = Line.Trim().Split( " \t".ToCharArray() );
					string NewLine = "";
					for( int i = 0; i < Parms.Length; i++ )
					{
						string Token = Parms[i];

						if( Token.Length < 1 )
						{
							continue;
						}

						// Skip any tags
						if( Token.StartsWith( "#" ) )
						{
							if( Token.Length > 1 )
							{
								if( Token[1] < '0' || Token[1] > '9' )
								{
									continue;
								}
							}
						}

						NewLine += Token + " ";
					}

					if( NewLine.Length > 0 )
					{
						Description += "\t" + NewLine + Environment.NewLine;
					}
                }
            }
        }

        public class CollationType
        {
            public COLLATION Collation;
			public string Title;
            public bool Active;
			public List<String> FolderMatches = new List<String>();

            public CollationType( COLLATION InCollation, string InTitle )
            {
				Collation = InCollation;
				Title = InTitle;
                Active = false;
            }

			public void SetFolderMatches( List<string> Matches )
			{
				FolderMatches = Matches;
			}

            static public COLLATION GetReportType( string CollType, List<string> ValidGames )
            {
				// Is it a member of the predefined enum list?
                if( Enum.IsDefined( typeof( COLLATION ), CollType ) )
                {
                    COLLATION Report = ( COLLATION )Enum.Parse( typeof( COLLATION ), CollType, true );
                    return ( Report );
                }

				// Is it a dynamically generated enum?
				int GameCollation = ( int )COLLATION.Game;
				foreach( string ValidGame in ValidGames )
				{
					string GameName = ValidGame.ToLower();
					string GameNameMaps = GameName + "maps";
					string GameNameContent = GameName + "content";

					if( CollType.ToLower() == GameName )
					{
						return ( ( COLLATION )GameCollation );
					}
					else if( CollType.ToLower() == GameNameMaps )
					{
						return ( ( COLLATION )( GameCollation + 1 ) );
					}
					else if( CollType.ToLower() == GameNameContent )
					{
						return ( ( COLLATION )( GameCollation + 2 ) );
					}

					GameCollation += 3;
				}

                return ( COLLATION.Count );
            }
        }

		// Tool configuration state - typically not adjusted
		public ToolConfiguration ToolConfig = null;

		// Version configuration state - typically set once per build
		public VersionConfiguration VersionConfig = null;

		// The location of the pfx file used to sign installers
		public string KeyLocation { get; set; }

		// The password for the above pfx
		public string KeyPassword { get; set; }

		// Root name of the current log file
		private string LogFileRootName = "";

		public enum ErrorModeType
		{
			CheckErrors,
			IgnoreErrors,
			SuppressErrors,
		}

		// Whether to check for errors in the build log
		public ErrorModeType ErrorMode { get; set; }

		// Whether a suppressed error has been seen in this task
		public bool HasSuppressedErrors { get; set; }

		// Whether to allow warnings when compiling script code
		public bool AllowSloppyScript { get; set; }

		// Whether to cc EngineQA
		public bool OfficialBuild { get; set; }

		// Whether to include the disambiguating timestamp folder when acquiring an installable build
		public bool IncludeTimestampFolder { get; set; }

		// Do we allow XGE for non primary builds
		public bool AllowXGE { get; set; }

		// Do we allow PCH for compilation?
		public bool AllowPCH { get; set; }

		// The branch that we are compiling from e.g. UnrealEngine3-Delta
		public Main.BranchDefinition BranchDef { get; set; }

		// Details from the database on the command we're running
		public Main.CommandInfo CommandDetails { get; set; }

		// Cached string containing the name of the user who killed the build
		public string Killer { get; set; }

		// The most recent label that was synced to
		public string SyncedLabel { get; set; }

		// The head changelist when syncing starts
		public int HeadChangelist { get; set; }

		// Time the local build started waiting for network bandwidth
		public DateTime StartWaitForConch { get; set; }

		// The last time the local build sent a status update
		public DateTime LastWaitForConchUpdate { get; set; }

		// The last time the local build sent a status update
		public DateTime LastConchCheck { get; set; }

		// Whether a label has already been created to host the files
		public bool NewLabelCreated { get; set; }

		// Use this name instead of the game name in the publish folder
		public string GameNameOverride { get; set; }

		// The sub branch to use in addition to the branch the game runs out of
		public string SubBranchName { get; set; }

		// Whether to include the platform in the publish folder
		public bool IncludePlatform { get; set; }

		// Whether to block other publish operations to throttle the network
		public bool BlockOnPublish { get; set; }

		// Whether to ignore timestamps when publishing
		public bool ForceCopy { get; set; }

		// Whether to use the 64 bit or 32 bit binaries
		public bool Use64BitBinaries { get; set; }

		// Whether to strip source content when wrangling
		public bool StripSourceContent { get; set; }

		public enum PublishModeType
		{
			Files,
			Zip,
			Iso,
			Xsf
		}

		// Whether to publish raw files, add them to a zip, or add to an iso
		public PublishModeType PublishMode { get; set; }

		public enum PublishVerificationType
		{
			None,
			MD5
		}

		// The type of verification used when publishing builds
		public PublishVerificationType PublishVerification { get; set; }

		// Which audio assets to keep when wrangling
		public string KeepAssets { get; set; }

		// The type of installer UnSetup will build
		public string UnSetupType { get; set; }

		// The database connection info
		public string DataBaseName { get; set; }
		public string DataBaseCatalog { get; set; }

		// Whether to enable unity stress testing
		public bool UnityStressTest { get; set; }

		// Whether to enable unity source file concatenation
		public bool UnityDisable { get; set; }

		// The name of the sku (currently used for disambiguating layout filenames)
		public string SkuName { get; set; }

		// The root location where the cooked files are copied to
		public string PublishFolder { get; set; }

		// The email addresses to send to when a build is triggered
		public string TriggerAddress { get; set; }

		// The email addresses to send to when a build fails
		public string FailAddress { get; set; }

		// The email addresses to send to when a build is successful
		public string SuccessAddress { get; set; }

		// A mailing list to cc all the emails to
		public string CarbonCopyAddress { get; set; }

		// A list of lines that make up the BUNs email
		public List<string> BUNs { get; set; }

		// The label or build that this build syncs to
		public string Dependency { get; set; }

		// A C++ define to be passed to the compiler
		public string BuildDefine { get; set; }

		// Destination folder of simple copy operations
		public string CopyDestination { get; set; }

		// The mode for saving a DVD image (e.g. ISO or XSF)
		public string ImageMode { get; set; }

		// The current mod name we are cooking for
		public string DLCName { get; set; }

		// The current command line for the builder command
		public string LogFileName { get; set; }

		// If there is a 'submit' at some point in the build script
		public bool IsBuilding { get; set; }

		// If we want to tag files to the label when submitting
		public bool IsTagging { get; set; }

		// If we want to preserve the head revision after submitting the builder's
		// changes, if we're not at the head revision already
		public bool IsRestoringNewerChanges { get; set; }

		// If there is a 'tag' at some point in the build script
		public bool IsPromoting { get; set; }

		// If there is a 'publish' at some point in the build script
		public bool bIsPublishing { get; set; }

		// If there is a 'UserChanges' at some point in the build script
		public bool IsSendingQAChanges { get; set; }

		// FTP settings
		public string FTPServer { get; set; }
		public string FTPUserName { get; set; }
		public string FTPPassword { get; set; }

		// Signing identities
		public string iPhoneCompileServerOverride { get;	set; }
		public string iPhoneSigningServerOverride { get; set; }
		public string SigningPrefixOverride { get; set; }
		public string DeveloperSigningIdentityOverride { get; set; }
		public string DistributionSigningIdentityOverride { get; set; }

		// AFT settings
		public string AFTTestMap { get; set; }
		public string TargetType = "DevKit";
		public List<string> AFTScreenShots = new List<string>();

		// The thread running the FTP upload
		public Thread ManageFTPThread = null;

		// Current zip class we are adding files to
		public ZipFile CurrentZip = null;

		// Creates a unique log file name for language independent operations
		public string GetLogFileName(COMMANDS Command, string OptionalTag = "")
		{
			LogFileName = LogFileRootName + "_" + LineCount.ToString() + "_" + Command.ToString() + OptionalTag + ".txt";
			return ( LogFileName );
		}

		public List<ChangeList> ChangeLists = new List<ChangeList>();
		private CollationType[] CollationTypes = new CollationType[( int )COLLATION.Count];
		private List<COLLATION> AllReports = new List<COLLATION>();
		private Queue<COLLATION> Reports = new Queue<COLLATION>();
		private List<string> ValidGames = new List<string>();

		private Main Parent = null;
		private P4 SCC = null;
		private SandboxedAction CurrentCommand = null;
		private StreamReader Script = null;
		public int LineCount = 0;
		public LabelInfo LabelInfo;

		// Symbol store
		private Queue<string> SymStoreCommands = new Queue<string>();

		// Normally set once at the beginning of the file

		// List of language variants that have full audio loc
		private Queue<string> Languages = new Queue<string>();
		// Subset of languages that have a valid loc'd variant
		private Queue<string> ValidLanguages;
		// List of language variants that have text only loc
		private Queue<string> TextLanguages = new Queue<string>();
		private TimeSpan OperationTimeout = new TimeSpan( 0, 10, 0 );
		private TimeSpan RespondingTimeout = new TimeSpan( 0, 10, 0 );

		// Changed multiple times during script execution
		private List<string> PublishDestinations = new List<string>();

		// Additional messages to report with the final success email
		private List<string> StatusReport = new List<string>();

		public string BuildConfig = "UnknownBuildConfiguration";
		public string ScriptConfiguration = "";
		public string ScriptConfigurationDLC = "";
		private string CookConfiguration = "";
		private string CommandletConfiguration = "";
		private string PackageTUConfiguration = "";
		private string AnalyzeReferencedContentConfiguration = "";
        private string ContentPath = "";

        // Working variables
		private COMMANDS ErrorLevel = COMMANDS.None;
		public bool ChangelistsCollated = false;

		// Compile-specific values
		public bool bXGEHasFailed = false;

        // The log file that gets referenced
        private TextWriter Log = null;

		public BuildState( Main InParent, P4 InSCC, Main.CommandInfo InCommandDetails, Main.BranchDefinition InBranchDef )
        {
            Parent = InParent;
			SCC = InSCC;
			BranchDef = InBranchDef;

			Environment.CurrentDirectory = BranchDef.CurrentClient.ClientRoot + "\\" + BranchDef.Branch;

			Parent.LabelRoot = "UnrealEngine3";
			string ScriptRoot = "Development/Builder/Scripts";
			string LogRoot = "Development/Builder/Logs";

			if( BranchDef.Version >= 10 )
			{
				Parent.LabelRoot = "UE4";
				ScriptRoot = "Engine/Build/Scripts";
				LogRoot = "Engine/Saved/BuildLogs";
			}

			Parent.EnsureDirectoryExists( LogRoot );
			Parent.DeleteDirectory( LogRoot, 5 );

			Parent.DXVersion = BranchDef.DirectXVersion;

            try
            {
				// Static initialisers
				KeyLocation = "";
				KeyPassword = "";

				ErrorMode = ErrorModeType.CheckErrors;
				HasSuppressedErrors = false;
				AllowSloppyScript = false;
				OfficialBuild = true;
				IncludeTimestampFolder = true;
				AllowXGE = true;
				AllowPCH = true;

				Killer = "";
				SyncedLabel = "";
				HeadChangelist = -1;

				LastWaitForConchUpdate = DateTime.UtcNow;
				NewLabelCreated = false;
				GameNameOverride = "";
				SubBranchName = "";
				IncludePlatform = true;
				BlockOnPublish = false;
				ForceCopy = false;
				Use64BitBinaries = true;
				StripSourceContent = false;
				PublishMode = PublishModeType.Files;
				PublishVerification = PublishVerificationType.MD5;
				
				KeepAssets = "";
				UnSetupType = "";
				
				UnSetupType = "UDK";
				UnityStressTest = false;
				UnityDisable = false;

				SkuName = "";
				PublishFolder = "";
				TriggerAddress = "";
				SuccessAddress = "";
				FailAddress = "";
				CarbonCopyAddress = "";
				BUNs = new List<string>();
				Dependency = "";
				BuildDefine = "";
				CopyDestination = "";
				ImageMode = "ISO";
				DLCName = "";
				LogFileName = "";
				
				IsBuilding = false;
				IsTagging = false;
				IsRestoringNewerChanges = false;
				IsPromoting = false;
				bIsPublishing = false;
				IsSendingQAChanges = false;

				// FTP settings
				FTPServer = "";
				FTPUserName = "";
				FTPPassword = "";

				SigningPrefixOverride = "";
				DeveloperSigningIdentityOverride = "";
				DistributionSigningIdentityOverride = "";

				// iPhone settings
				iPhoneCompileServerOverride = "";
				iPhoneSigningServerOverride = "";
				SigningPrefixOverride = "";
				DeveloperSigningIdentityOverride = "";
				DistributionSigningIdentityOverride = "";

				// AFT settings
				AFTTestMap = "";

				// Dynamic initialization
				CommandDetails = InCommandDetails;
				StartWaitForConch = CommandDetails.BuildStarted;
				LastConchCheck = CommandDetails.BuildStarted;

				// Default version file names
				VersionConfig = new VersionConfiguration( this, BranchDef );

				// Init the tools configuration
				ToolConfig = new ToolConfiguration( this, BranchDef );

                // Always need some label
				LabelInfo = new LabelInfo( Parent, this );
				LabelInfo.Init( this, RevisionType.Head );

				string ScriptFile = Path.Combine( ScriptRoot, CommandDetails.Script + ".build" );
                string NewScriptFile = Path.Combine( ScriptRoot, "Current.build" );

				CreateCurrentScriptFile( ScriptRoot, ScriptFile, NewScriptFile );

                Script = new StreamReader( NewScriptFile );
                Parent.Log( "Using build script '" + BranchDef.Branch + "/" + ScriptFile + "'", Color.Magenta );

                LogFileRootName = Path.Combine( LogRoot, "Builder_[" + GetTimeStamp() + "]" );

                // Make sure there are no defines hanging around
                Environment.SetEnvironmentVariable( "CL", "" );

				InitCollationTypes();
            }
            catch
            {
                Parent.Log( "Error, problem loading build script", Color.Red );
            }
        }

        public void Destroy()
        {
            Parent.Log( LineCount.ToString() + " lines of build script parsed", Color.Magenta );
            if( Script != null )
            {
                Script.Close();
            }
        }

		public void OpenLog( string LogFileName, bool Append )
        {
			Log = TextWriter.Synchronized( new StreamWriter( LogFileName, Append, Encoding.Unicode ) );
		}

        public void CloseLog()
        {
            if( Log != null )
            {
				try
				{
					Log.Close();
					Log = null;
				}
				catch( Exception Ex )
				{
					Parent.Log( "Log close exception: exception '" + Ex.Message + "'", Color.Red );
					ErrorLevel = COMMANDS.NetworkVanished;
				}
            }
        }

		public TextWriter OpenSystemLog()
		{
			return TextWriter.Synchronized( new StreamWriter( LogFileRootName + "_SystemLog.txt", false, Encoding.Unicode ) );
		}
		
		public void Write( string Output )
        {
            if( Log != null && Output != null )
            {
                try
                {
					Log.Write( DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Output + Environment.NewLine );
                }
                catch( Exception Ex )
                {
                    Parent.Log( "Log write exception: exception '" + Ex.Message + "'", Color.Red );
                    Parent.Log( "Log write exception: intended to write '" + Output + "'", Color.Red );
                }
            }
        }

		public SandboxedAction GetCurrentCommand()
		{
			return CurrentCommand;
		}

		public void SetCurrentCommand( SandboxedAction NewCommand )
		{
			CurrentCommand = NewCommand;
		}

		public bool HasSandboxedAction()
		{
			return CurrentCommand != null;
		}

		public BuildProcess GetCurrentBuild()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.GetCurrentBuild();
			}

			return null;
		}

		public bool CommandIsMSVC9Specific()
		{
			if( CurrentCommand != null )
			{
				if( CurrentCommand.CommandDelegate == CurrentCommand.MSVC9Build
					|| CurrentCommand.CommandDelegate == CurrentCommand.MSVC9Clean
					|| CurrentCommand.CommandDelegate == CurrentCommand.MSVC9Deploy
					|| CurrentCommand.CommandDelegate == CurrentCommand.MS9Build
					|| CurrentCommand.CommandDelegate == CurrentCommand.BuildUBT9 )
				{
					return true;
				}
			}

			return false;
		}

		public bool CommandIsMSVC10Specific()
		{
			if( CurrentCommand != null )
			{
				if( CurrentCommand.CommandDelegate == CurrentCommand.MSVC10Build
					|| CurrentCommand.CommandDelegate == CurrentCommand.MSVC10Clean
					|| CurrentCommand.CommandDelegate == CurrentCommand.MSVC10Deploy
					|| CurrentCommand.CommandDelegate == CurrentCommand.MS10Build
					|| CurrentCommand.CommandDelegate == CurrentCommand.BuildUBT10 )
				{
					return true;
				}
			}

			return false;
		}

		public string GetCurrentCommandLine()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.CommandLine.Trim();
			}

			return "";
		}

		public void SetCurrentCommandLine( string NewCommandLine )
		{
			if( CurrentCommand != null )
			{
				CurrentCommand.CommandLine = NewCommandLine;
			}
		}

		public MODES ExecuteCommand()
		{
			// Fire off a new process safely
			if( CurrentCommand != null )
			{
				if( CurrentCommand.CommandDelegate != null )
				{
					return CurrentCommand.CommandDelegate();
				}
				else
				{
					Parent.SendWarningMail( "[DEBUG] Delegate is NULL!", "Error: " + CurrentCommand.State.ToString(), false );
				}
			}

			return MODES.Finalise;
		}

		public void ExpandCommandLine()
		{
			if( CurrentCommand != null )
			{
				CurrentCommand.CommandLine = Parent.ExpandString( CurrentCommand.CommandLine );
			}
		}

		public COMMANDS GetCurrentState()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.State;
			}

			return COMMANDS.None;
		}

		public void SetCurrentState( COMMANDS NewState )
		{
			if( CurrentCommand != null )
			{
				CurrentCommand.State = NewState;
			}
		}

		public int GetCurrentExitCode()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.GetExitCode();
			}

			return 0;
		}

		public MODES IsCurrentlyFinished()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.IsFinished();
			}

			return MODES.Finalise;
		}

		public bool IsCurrentlyTimedOut()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.IsTimedOut();
			}

			return true;
		}

		public bool IsCurrentlyCooking()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.bIsCooking;
			}

			return false;
		}

		public bool IsCurrentlyPublishing()
		{
			if( CurrentCommand != null )
			{
				return CurrentCommand.bIsPublishing;
			}

			return false;
		}

		public void RevertAllFiles()
		{
			CurrentCommand = new SandboxedAction( Parent, SCC, this );
			CurrentCommand.SCC_Revert();
		}

		public void KillCurrentCommand()
		{
			if( CurrentCommand != null )
			{
				CurrentCommand.Kill();
			}
		}

		public string GetBranchRelativePath( string FullPath )
		{
			if( Path.IsPathRooted( FullPath ) )
			{
				string BranchRoot = Path.Combine( BranchDef.CurrentClient.ClientRoot, BranchDef.Branch );
				return FullPath.Substring( BranchRoot.Length + 1 );
			}

			return FullPath;
		}

		private void CreateCurrentScriptFile( string ScriptRoot, string ScriptFile, string NewScriptFile )
		{
			FileInfo ScriptInfo = new FileInfo( NewScriptFile );
			if( ScriptInfo.Exists )
			{
				ScriptInfo.IsReadOnly = false;
				ScriptInfo.Delete();
			}

			StreamReader Source = new StreamReader( ScriptFile );
			StreamWriter Destination = new StreamWriter( NewScriptFile );

			while( !Source.EndOfStream )
			{
				string Line = Source.ReadLine();

				if( Line.ToLower().StartsWith( "include" ) )
				{
					Destination.WriteLine();

					string BrickName = Path.Combine( ScriptRoot, "Include", Line.Substring( "include".Length ).Trim() + ".brick" );
					FileInfo BrickInfo = new FileInfo( BrickName );

					if( BrickInfo.Exists )
					{
						string[] AllLines = File.ReadAllLines( BrickName );
						foreach( string IncludeLine in AllLines )
						{
							Destination.WriteLine( IncludeLine );
						}
					}
					else
					{
						Destination.WriteLine( "ERROR: Failed to include: " + BrickName );
					}

					Destination.WriteLine();
				}
				else
				{
					Destination.WriteLine( Line );
				}
			}

			Destination.Close();
			Source.Close();
		}

		public string[] SafeSplit( string Line, bool bPreserveQuotes )
		{
			List<string> SafeParms = new List<string>();

			string Parm = "";
			bool bInQuotes = false;
			foreach( char Letter in Line )
			{
				if( Letter == ' ' || Letter == '\t' )
				{
					if( !bInQuotes )
					{
						if( Parm.Length > 0 )
						{
							SafeParms.Add( Parm );
							Parm = "";
						}
						continue;
					}
					else
					{
						Parm += Letter;
					}
				}
				else if( Letter == '\"' )
				{
					bInQuotes = !bInQuotes;
					if( bPreserveQuotes )
					{
						Parm += Letter;
					}
				}
				else
				{
					Parm += Letter;
				}
			}

			if( Parm.Length > 0 )
			{
				SafeParms.Add( Parm );
			}

			return ( SafeParms.ToArray() );
		}

		public string[] SplitCommandline()
		{
			return ( SafeSplit( CurrentCommand.CommandLine, false ) );
		}

        public Queue<string> GetLanguages()
        {
            return ( Languages );
        }

		public Queue<string> GetTextLanguages()
		{
			return ( TextLanguages );
		}

        public void SetValidLanguages( Queue<string> ValidLangs )
        {
            ValidLanguages = ValidLangs;
        }

        public Queue<string> GetValidLanguages()
        {
            return ( ValidLanguages );
        }

		public string GetFolderName()
		{
			return ( LabelInfo.GetFolderName( GameNameOverride, IncludePlatform, BranchDef.Version ) );
		}

		public string GetSubBranch()
		{
			string SubBranch = "";
			if( SubBranchName.Length > 0 )
			{
				SubBranch = "\\" + SubBranchName;
			}

			return ( SubBranch );
		}

		public string GetImageName()
		{
			string FolderName = LabelInfo.GetFolderName( GameNameOverride, IncludePlatform, BranchDef.Version );
			if( SkuName.Length > 0 )
			{
				FolderName += "_" + SkuName;
			}
			return ( FolderName );
		}

        public string GetScriptConfiguration()
        {
			string ScriptConfig = "";
			if( ScriptConfiguration.Length > 0 )
            {
                string[] Configs = SafeSplit( ScriptConfiguration, false );
                foreach( string Config in Configs )
                {
                    if( Config.ToLower() == "fr" )
                    {
                        ScriptConfig += " -final_release";
                    }
                    else
                    {
                        ScriptConfig += " -" + Config;
                    }
                }
            }
			if( ScriptConfigurationDLC.Length > 0 )
			{
				ScriptConfig += " -dlc " + ScriptConfigurationDLC;
			}
			return ScriptConfig;
        }

        public string GetCookConfiguration()
        {
            if( CookConfiguration.Length > 0 )
            {
                return ( " -" + CookConfiguration );
            }
            return ( "" );
        }

		public string GetOptionalCommandletConfig()
		{
			if( CommandletConfiguration.Length > 0 )
			{
				return ( " -" + CommandletConfiguration );
			}

			return ( "" );
		}

		public string GetPackageTUConfig()
		{
			if( PackageTUConfiguration.Length > 0 )
			{
				return ( " " + PackageTUConfiguration );
			}

			return ( "" );
		}

		public string GetDataBaseConnectionInfo()
		{
			string ConnectionInfo = "";
			if( DataBaseName.Length > 0 && DataBaseCatalog.Length > 0 )
			{
				ConnectionInfo = " -DATABASE=" + DataBaseName + " -CATALOG=" + DataBaseCatalog;
			}

			return ( ConnectionInfo );
		}

		public string GetAnalyzeReferencedContentConfiguration()
		{
			if( AnalyzeReferencedContentConfiguration.Length > 0 )
			{
				return ( " " + AnalyzeReferencedContentConfiguration );
			}
			return ( "" );
		}

        public string GetContentPath()
        {
            if( ContentPath.Length > 0 )
            {
                return ( " -paths=" + ContentPath );
            }
            return ( "" );
        }

        public string GetDLCName()
        {
			if( DLCName.Length > 0 )
            {
				return ( " -DLCName=" + DLCName );
            }
            return ( "" );
        }

        public void ClearPublishDestinations()
        {
            PublishDestinations.Clear();
        }

        public void AddPublishDestination( string Dest )
        {
            PublishDestinations.Add( Dest );
        }

        public List<string> GetPublishDestinations()
        {
            return ( PublishDestinations );
        }

		public void AddToSuccessStatus( string AdditionalStatus )
		{
			StatusReport.Add( AdditionalStatus );
		}

		public void AddUniqueToSuccessStatus( string AdditionalStatus )
		{
			if( !StatusReport.Contains( AdditionalStatus ) )
			{
				StatusReport.Add( AdditionalStatus );
			}
		}

		public List<string> GetStatusReport()
		{
			return( StatusReport );
		}

        public void AddUpdateSymStore( string CommandName )
        {
            SymStoreCommands.Enqueue( CommandName );
        }

        public string PopUpdateSymStore()
        {
            if( !UpdateSymStoreEmpty() )
            {
                return ( SymStoreCommands.Dequeue() );
            }

            return ( "" );
        }

        public bool UpdateSymStoreEmpty()
        {
            return ( SymStoreCommands.Count == 0 );
        }

		public void SetErrorLevel( COMMANDS Error )
        {
            ErrorLevel = Error;
        }

		public COMMANDS GetErrorLevel()
        {
            return ErrorLevel;
        }

        public string GetTimeStamp()
        {
			DateTime LocalTime = CommandDetails.BuildStarted.ToLocalTime();

			string TimeStamp = LocalTime.Year + "-"
						+ LocalTime.Month.ToString( "00" ) + "-"
						+ LocalTime.Day.ToString( "00" ) + "_"
						+ LocalTime.Hour.ToString( "00" ) + "."
						+ LocalTime.Minute.ToString( "00" ) + "."
						+ LocalTime.Second.ToString( "00" );
			return ( TimeStamp );
        }

        public int SafeStringToInt( string Number )
        {
            int Result = 0;

            try
            {
                Number = Number.Trim();
                Result = Int32.Parse( Number );
            }
            catch
            {
            }

            return ( Result );
        }

		public long SafeStringToLong( string Number )
		{
			long Result = 0;

			try
			{
				Number = Number.Trim();
				Result = long.Parse( Number );
			}
			catch
			{
			}

			return ( Result );
		}

		public void AddScreenShot( string ScreenShot )
		{
			AFTScreenShots.Add( ScreenShot );
		}

        public GameConfig AddCheckedOutGame()
        {
			GameConfig Config = new GameConfig( CurrentCommand.CommandLine, LabelInfo.Platform, BuildConfig, LabelInfo.Defines, true, Use64BitBinaries );
            LabelInfo.Games.Add( Config );

            return ( Config );
        }

        public GameConfig CreateGameConfig( string Game, string InPlatform )
        {
			GameConfig Config = new GameConfig( Game, InPlatform, BuildConfig, LabelInfo.Defines, true, Use64BitBinaries );

            return ( Config );
        }

        public GameConfig CreateGameConfig( string Game )
        {
			GameConfig Config = new GameConfig( Game, LabelInfo.Platform, BuildConfig, LabelInfo.Defines, true, Use64BitBinaries );

            return ( Config );
        }

        public GameConfig CreateGameConfig()
        {
			GameConfig Config = new GameConfig( LabelInfo.Game, LabelInfo.Platform, BuildConfig, LabelInfo.Defines, true, Use64BitBinaries );

            return ( Config );
        }

        public GameConfig CreateGameConfig( bool Local )
        {
			GameConfig Config = new GameConfig( LabelInfo.Game, LabelInfo.Platform, BuildConfig, LabelInfo.Defines, Local, Use64BitBinaries );

            return ( Config );
        }

        public TimeSpan GetTimeout()
        {
            return ( OperationTimeout );
        }

        public TimeSpan GetRespondingTimeout()
        {
            return ( RespondingTimeout );
        }

        // Set the CL env var based on required defines
        public void HandleMSVCDefines()
        {
			// Clear out env var
            Environment.SetEnvironmentVariable( "CL", "" );

			// Set up new one if necessary
			string EnvVar = "";
			if( LabelInfo.Defines.Count > 0 )
			{
				foreach( string Define in LabelInfo.Defines )
				{
					EnvVar += "/D " + Define + " ";
				}

				Environment.SetEnvironmentVariable( "CL", EnvVar.Trim() );
			}

            Write( "Setting CL=" + EnvVar );
        }

        // Passed in on the command line, nothing special to do here
        public string HandleUBTDefines( int BranchVersion )
        {
			// Clear out env var
			Environment.SetEnvironmentVariable( "CL", "" );

			// Set up new parameter
			string CommandLine = "";
			if( LabelInfo.Defines.Count > 0 )
			{
				foreach( string Define in LabelInfo.Defines )
				{
					if( BranchVersion >= 11 )
					{
						CommandLine += Define + " ";
					}
					else
					{
						CommandLine += "-define " + Define + " ";
					}
				}

				CommandLine = " " + CommandLine.Trim();
			}

			return CommandLine;
        }

        public void ParseNextLine()
        {
            string Line;

			CurrentCommand = new SandboxedAction( Parent, SCC, this );
			CurrentCommand.State = COMMANDS.Config;
			CurrentCommand.CommandLine = "";
			CurrentCommand.CommandDelegate = null;

            if( Script == null )
            {
				CurrentCommand.State = COMMANDS.Error;
				ErrorLevel = COMMANDS.NoScript;
                return;
            }

            Line = Script.ReadLine();
            if( Line == null )
            {
                Parent.Log( "[STATUS] Script parsing completed", Color.Magenta );
				CurrentCommand.State = COMMANDS.Finished;
                return;
            }

            LineCount++;
            string[] Parms = SafeSplit( Line, false );
            if( Parms.Length > 0 )
            {
                string CommandName = Parms[0].ToLower();
				CurrentCommand.CommandLine = Line.Substring( Parms[0].Length ).Trim();
				CurrentCommand.CommandLine = CurrentCommand.CommandLine.Replace( '\\', '/' );
                // Need to keep the double backslashes for network locations
				CurrentCommand.CommandLine = CurrentCommand.CommandLine.Replace( "//", "\\\\" );

                if( CommandName.Length == 0 || CommandName.StartsWith( "//" ) )
                {
                    // Comment - ignore
					CurrentCommand.State = COMMANDS.Config;
                }
                else if( CommandName == "status" )
                {
                    // Status string - just echo
					Parent.Log( "[STATUS] " + Parent.ExpandString( CurrentCommand.CommandLine ), Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
                }
                else if( CommandName == "watchstart" )
                {
                    // Start a timer
					string PerfKeyName = Parent.ExpandString( CurrentCommand.CommandLine );
                    Parent.Log( "[WATCHSTART " + PerfKeyName + "]", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
                }
                else if( CommandName == "watchstop" )
                {
                    // Stop a timer
                    Parent.Log( "[WATCHSTOP]", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
                }
				else if( CommandName == "errormode" )
				{
					string ErrorModeString = Parent.ExpandString( CurrentCommand.CommandLine );
					try
					{
						ErrorMode = ( ErrorModeType )Enum.Parse( typeof( ErrorModeType ), ErrorModeString, true );
					}
					catch
					{
						ErrorMode = ErrorModeType.CheckErrors;
					}

					Parent.Log( "ErrorMode set to '" + ErrorMode.ToString() + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "allowsloppyscript" )
				{
					AllowSloppyScript = true;
					Parent.Log( "Script warnings will not fail the build", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "unofficial" )
				{
					OfficialBuild = false;
					Parent.Log( "EngineQA will not be cc'd on build failure", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ambiguousfoldername" )
				{
					// This is a requirement for the Steam content server
					IncludeTimestampFolder = false;
					Parent.Log( "The timestamped folder will not be used when getting an installable build", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ubtenablexge" )
				{
					AllowXGE = true;
					Parent.Log( "XGE is now ENABLED in UBT", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ubtdisablexge" )
				{
					AllowXGE = false;
					Parent.Log( "XGE is now DISABLED in UBT", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ubtenablepch" )
				{
					AllowPCH = true;
					Parent.Log( "PCHs are now ENABLED in UBT", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ubtdisablepch" )
				{
					AllowPCH = false;
					Parent.Log( "PCHs are now DISABLED in UBT", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "platformspecific" )
				{
					IncludePlatform = true;
					Parent.Log( "Publish folder includes the platform", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "platformagnostic" )
				{
					IncludePlatform = false;
					Parent.Log( "Publish folder does NOT include the platform", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "gamenameoverride" )
				{
					GameNameOverride = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Publish folder uses '" + GameNameOverride + "' rather than the game name", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "subbranch" )
				{
					SubBranchName = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Publish folder adds in '" + SubBranchName + "' to the branch name", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "languagespecific" )
                {
					Parent.SendErrorMail( "Deprecated command 'LanguageSpecific'", "This command is no longer used. CommandID = " + CommandDetails.CommandID.ToString() );
					Parent.Log( "[DEPRECATED] LanguageSpecific", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
                }
                else if( CommandName == "languageagnostic" )
                {
					Parent.SendErrorMail( "Deprecated command 'LanguageAgnostic'", "This command is no longer used. CommandID = " + CommandDetails.CommandID.ToString() );
					Parent.Log( "[DEPRECATED] LanguageAgnostic", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
                }
                else if( CommandName == "touchlabel" )
                {
					// This is used to update the timestamp of the folder to publish to
                    LabelInfo.Touch();
                    Parent.Log( "The current label has been touched", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
                }
                else if( CommandName == "updatelabel" )
                {
					CurrentCommand.State = COMMANDS.UpdateLabel;
                }
                else if( CommandName == "updatefolder" )
                {
					CurrentCommand.State = COMMANDS.UpdateFolder;
                }
				else if( CommandName == "clearpublishdestinations" )
				{
					Parent.Log( "Clearing publish destinations", Color.Magenta );
					ClearPublishDestinations();
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "saveerror" )
				{
					Parent.SendErrorMail( "Deprecated command 'SaveError'", "This command is no longer used. CommandID = " + CommandDetails.CommandID.ToString() );
					Parent.Log( "[DEPRECATED] SaveError", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "keylocation" )
				{
					KeyLocation = Parent.ExpandString( CurrentCommand.CommandLine );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "keypassword" )
				{
					KeyPassword = Parent.ExpandString( CurrentCommand.CommandLine );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "tag" )
				{
					Parent.Log( "The latest build label will be copied to '" + CurrentCommand.CommandLine + "'", Color.Magenta );
					IsPromoting = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_Tag;
				}
				else if( CommandName == "report" )
				{
					Reports.Clear();

					string ExpandedCommandLine = Parent.ExpandString( CurrentCommand.CommandLine );
					string[] NewParms = SafeSplit( ExpandedCommandLine, false );

					for( int i = 0; i < NewParms.Length; i++ )
					{
						if( NewParms[i].Length > 0 )
						{
							COLLATION Collation = CollationType.GetReportType( NewParms[i], ValidGames );
							if( Collation != COLLATION.Count )
							{
								Reports.Enqueue( Collation );
							}
							else
							{
								Parent.Log( "Missing COLLATION type: " + NewParms[i], Color.Red );
							}
						}
					}
				}
				else if( CommandName == "triggeraddress" )
				{
					Parent.SendErrorMail( "Deprecated command 'TriggerAddress'", "CommandID = " + CommandDetails.CommandID.ToString() );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "failaddress" )
				{
					// Email addresses to use if the build fails
					FailAddress = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "FailAddress set to '" + FailAddress + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "successaddress" )
				{
					// Email addresses to use if the build succeeds
					SuccessAddress = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "SuccessAddress set to '" + SuccessAddress + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "carboncopyaddress" )
				{
					// Email addresses to use when the build is triggered
					CarbonCopyAddress = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "CarbonCopyAddress set to '" + CarbonCopyAddress + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "sendemail" )
				{
					// Send an email based on the current state of the build
					Parent.Log( "Sending update email", Color.Magenta );
					CurrentCommand.State = COMMANDS.SendEmail;
				}
				else if( CommandName == "addbunline" )
				{
					BUNs.Add( CurrentCommand.CommandLine.Trim( '\"' ) );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "cisprocessp4changes" )
				{
					// Instruct the controller to process all unprocessed p4 changes for CIS tasks
					Parent.Log( "Processing P4 changes...", Color.Magenta );
					CurrentCommand.State = COMMANDS.CISProcessP4Changes;
				}
				else if( CommandName == "cisupdatemonitorvalues" )
				{
					// Instruct the controller to evaluate all completed CIS jobs and post results
					// the CIS Monitor can use
					Parent.Log( "Processing completed CIS jobs...", Color.Magenta );
					CurrentCommand.State = COMMANDS.CISUpdateMonitorValues;
				}
				else if( CommandName == "sku" )
				{
					SkuName = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Sku set to: " + SkuName, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "createmgstrigger" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CreateMGSTrigger;
				}
				else if( CommandName == "createfaketoc" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CreateFakeTOC;
				}
				else if( CommandName == "ftpserver" )
				{
					FTPServer = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "FTP server set to: " + FTPServer, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ftpusername" )
				{
					FTPUserName = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "FTP username set to: " + FTPUserName, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ftppassword" )
				{
					FTPPassword = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "FTP password set to: <NOT TELLING!>", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "ftpsendfile" )
				{
					CurrentCommand.State = COMMANDS.FTPSendFile;
					CurrentCommand.CommandDelegate = CurrentCommand.FTPSendFile;
				}
				else if( CommandName == "ftpsendimage" )
				{
					CurrentCommand.State = COMMANDS.FTPSendImage;
					CurrentCommand.CommandDelegate = CurrentCommand.FTPSendImage;
				}
				else if( CommandName == "ftpsendfolder" )
				{
					CurrentCommand.State = COMMANDS.FTPSendFolder;
					CurrentCommand.CommandDelegate = CurrentCommand.FTPSendFolder;
				}
				else if( CommandName == "zipaddimage" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.ZipAddImage;
				}
				else if( CommandName == "zipaddfile" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.ZipAddFile;
				}
				else if( CommandName == "zipsave" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.ZipSave;
				}
				else if( CommandName == "define" )
				{
					// Define for the compiler to use
					LabelInfo.ClearDefines();

					BuildDefine = Parent.ExpandString( CurrentCommand.CommandLine );
					if( BuildDefine.Length > 0 )
					{
						string[] DefineParms = SafeSplit( BuildDefine, false );
						foreach( string Define in DefineParms )
						{
							LabelInfo.AddDefine( Define );
						}
					}
					Parent.Log( "Define set to '" + BuildDefine + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "language" )
				{
					// Language for the cooker to use
					LabelInfo.Language = Parent.ExpandString( CurrentCommand.CommandLine ).ToUpper();
					Parent.Log( "Language set to '" + LabelInfo.Language + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "languages" )
				{
					// List of languages used to conform or ML cook
					Languages.Clear();

					string LangLine = Parent.ExpandString( CurrentCommand.CommandLine ).ToUpper();
					string[] LangParms = SafeSplit( LangLine, false );
					foreach( string Lang in LangParms )
					{
						if( Lang.Length > 0 )
						{
							Languages.Enqueue( Lang );
						}
					}

					Parent.Log( "Number of languages to process: " + Languages.Count, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "textlanguages" )
				{
					// List of languages that only have text loc
					TextLanguages.Clear();

					string LangLine = Parent.ExpandString( CurrentCommand.CommandLine ).ToUpper();
					string[] LangParms = SafeSplit( LangLine, false );
					foreach( string Lang in LangParms )
					{
						if( Lang.Length > 0 )
						{
							TextLanguages.Enqueue( Lang );
						}
					}

					Parent.Log( "Number of text languages to process: " + TextLanguages.Count, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "clientspec" )
				{
					Parent.SendErrorMail( "Deprecated command 'ClientSpec'", "CommandID = " + CommandDetails.CommandID.ToString() );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "timeout" )
				{
					// Number of minutes to failure
					int OpTimeout = SafeStringToInt( Parent.ExpandString( CurrentCommand.CommandLine ) );
					OperationTimeout = new TimeSpan( 0, OpTimeout, 0 );
					Parent.Log( "Timeout set to " + OpTimeout.ToString() + " minutes", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "msvc9application" )
				{
					// MSVC command line app
					ToolConfig.MSVC9Application = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "MSVC9 executable set to '" + ToolConfig.MSVC9Application + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "msvc10application" )
				{
					// MSVC command line app
					ToolConfig.MSVC10Application = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "MSVC10 executable set to '" + ToolConfig.MSVC10Application + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "steamcontenttool" )
				{
					// Steam Content Tool executable location
					ToolConfig.SteamContentToolLocation = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Steam Content Tool executable set to '" + ToolConfig.SteamContentToolLocation + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "steamcontentserver" )
				{
					// Steam Content Server executable location
					ToolConfig.SteamContentServerLocation = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Steam Content Server executable set to '" + ToolConfig.SteamContentServerLocation + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "signtoolapplication" )
				{
					// Sign tool location
					ToolConfig.SignToolName = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Sign tool executable set to '" + ToolConfig.SignToolName + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "sourceservercommand" )
				{
					// bat/perl script to index pdbs
					ToolConfig.SourceServerCmd = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Source server script set to '" + ToolConfig.SourceServerCmd + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "versionfile" )
				{
					// Main version file
					VersionConfig.EngineVersionFile = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Engine version file set to '" + VersionConfig.EngineVersionFile + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "miscversionfiles" )
				{
					// Version files that have a version propagated to
					string[] VersionFiles = Parent.ExpandString( CurrentCommand.CommandLine ).Split( "; \t".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					VersionConfig.MiscVersionFiles = new List<string>( VersionFiles );
					Parent.Log( VersionConfig.MiscVersionFiles.Count.ToString() + " misc version files set", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "consoleversionfiles" )
				{
					// Version files that have a version propagated to
					string[] VersionFiles = Parent.ExpandString( CurrentCommand.CommandLine ).Split( "; \t".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					VersionConfig.ConsoleVersionFiles = new List<string>( VersionFiles );
					Parent.Log( VersionConfig.ConsoleVersionFiles.Count.ToString() + " console version files set", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "mobileversionfiles" )
				{
					// Version files that have a version propagated to
					string[] VersionFiles = Parent.ExpandString( CurrentCommand.CommandLine ).Split( "; \t".ToCharArray(), StringSplitOptions.RemoveEmptyEntries );
					VersionConfig.MobileVersionFiles = new List<string>( VersionFiles );
					Parent.Log( VersionConfig.MobileVersionFiles.Count.ToString() + " mobile version files set", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "game" )
				{
					// Game we are interested in
					LabelInfo.Game = Parent.ExpandString( CurrentCommand.CommandLine );
					LabelInfo.bIsTool = false;
					Parent.Log( "Game set to '" + LabelInfo.Game + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "tool" )
				{
					// Game we are interested in
					LabelInfo.Game = Parent.ExpandString( CurrentCommand.CommandLine );
					LabelInfo.bIsTool = true;
					Parent.Log( "Tool set to '" + LabelInfo.Game + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "platform" )
				{
					// Platform we are interested in
					LabelInfo.Platform = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Platform set to '" + LabelInfo.Platform + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "targettype" )
				{
					// Platform we are interested in
					TargetType = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "TargetType set to '" + TargetType + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "buildconfig" )
				{
					// Build we are interested in
					BuildConfig = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Build configuration set to '" + BuildConfig + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "scriptconfig" )
				{
					// Script release or final_release we are interested in
					ScriptConfiguration = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Script configuration set to '" + ScriptConfiguration + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "scriptconfigdlc" )
				{
					// Script release or final_release we are interested in
					ScriptConfigurationDLC = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "DLC script configuration set to '" + ScriptConfigurationDLC + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "cookconfig" )
				{
					// Cook config we are interested in (eg. CookForServer - dedicated server trimming)
					CookConfiguration = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Cook configuration set to '" + CookConfiguration + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "dlcname" )
				{
					// The mod name we are cooking for
					DLCName = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "DLCName set to '" + DLCName + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "contentpath" )
				{
					// Used after a wrangle
					ContentPath = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "ContentPath set to '" + ContentPath + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "commandlet" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.Commandlet;
				}
				else if( CommandName == "commandletconfig" )
				{
					// Set to size or speed
					CommandletConfiguration = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "CommandletConfiguration set to '" + CommandletConfiguration + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "packagetuconfig" )
				{
					// Set to size or speed
					PackageTUConfiguration = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "PackageTUConfiguration set to '" + PackageTUConfiguration + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "database" )
				{
					string[] DataBaseConnection = SplitCommandline();
					if( DataBaseConnection.Length == 2 )
					{
						DataBaseName = DataBaseConnection[0];
						DataBaseCatalog = DataBaseConnection[1];
						Parent.Log( "Database connection set to '" + DataBaseName + "' and '" + DataBaseCatalog + "'", Color.Magenta );
						CurrentCommand.State = COMMANDS.Config;
					}
					else
					{
						Parent.Log( "Error, need both database and catalog in database definition", Color.Magenta );

						ErrorLevel = COMMANDS.IllegalCommand;
						CurrentCommand.State = COMMANDS.Error;
					}
				}
				else if( CommandName == "dependency" )
				{
					Dependency = Parent.ExpandString( CurrentCommand.CommandLine );
					Parent.Log( "Dependency set to '" + Dependency + "'", Color.DarkGreen );
					CurrentCommand.State = COMMANDS.SetDependency;
				}
				else if( CommandName == "copydest" )
				{
					CopyDestination = Parent.ExpandString( CurrentCommand.CommandLine );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "blockonpublish" )
				{
					BlockOnPublish = true;
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "forcecopy" )
				{
					ForceCopy = true;
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "use64bit" )
				{
					Use64BitBinaries = true;
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "use32bit" )
				{
					Use64BitBinaries = false;
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "stripsourcecontent" )
				{
					StripSourceContent = true;
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "publishmode" )
				{
					string PublishModeString = Parent.ExpandString( CurrentCommand.CommandLine );
					try
					{
						PublishMode = ( PublishModeType )Enum.Parse( typeof( PublishModeType ), PublishModeString, true );
					}
					catch
					{
						PublishMode = PublishModeType.Files;
					}

					Parent.Log( "Publishing mode set to '" + PublishMode.ToString() + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "publishverification" )
				{
					string PublishVerificationString = Parent.ExpandString( CurrentCommand.CommandLine );
					try
					{
						PublishVerification = ( PublishVerificationType )Enum.Parse( typeof( PublishVerificationType ), PublishVerificationString, true );
					}
					catch
					{
						PublishVerification = PublishVerificationType.MD5;
					}

					Parent.Log( "Publishing verification set to '" + PublishVerification.ToString() + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "keepassets" )
				{
					KeepAssets = Parent.ExpandString( CurrentCommand.CommandLine );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "imagemode" )
				{
					ImageMode = Parent.ExpandString( CurrentCommand.CommandLine );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "unity" )
				{
					string UnityParameter = Parent.ExpandString( CurrentCommand.CommandLine );
					if( UnityParameter == "disable" )
					{
						UnityDisable = true;
					}
					else if( UnityParameter == "stresstest" )
					{
						UnityStressTest = true;
					}
					Parent.Log( "Unity set to '" + UnityParameter + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "copyscriptpatchfiles" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CopyScriptPatchFiles;
				}
				else if( CommandName == "copy" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SimpleCopy;
				}
				else if( CommandName == "delete" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SimpleDelete;
				}
				else if( CommandName == "rename" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SimpleRename;
				}
				else if( CommandName == "renamecopy" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.RenamedCopy;
				}
				else if( CommandName == "setsteamid" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SetSteamID;
				}
				else if( CommandName == "checkconsistency" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckConsistency;
				}
				else if( CommandName == "getchangelist" )
				{
					CurrentCommand.State = COMMANDS.GetChangelist;
				}
				else if( CommandName == "getpublisheddata" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.GetPublishedData;
				}
				else if( CommandName == "testmap" )
				{
					AFTTestMap = Parent.ExpandString( CurrentCommand.CommandLine );
					AFTScreenShots.Clear();

					Parent.Log( "AFT: Test map set to '" + AFTTestMap + "'", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "waittargetstate" )
				{
					CurrentCommand.State = COMMANDS.WaitTargetState;
				}
				else if( CommandName == "steampipe" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SteamPipe;
				}
				else if( CommandName == "steammakeversion" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SteamMakeVersion;
				}
				else if( CommandName == "updatesteamserver" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.UpdateSteamServer;
				}
				else if( CommandName == "restartsteamserver" )
				{
					CurrentCommand.State = COMMANDS.RestartSteamServer;
				}
				else if( CommandName == "startsteamserver" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.StartSteamServer;
				}
				else if( CommandName == "stopsteamserver" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.StopSteamServer;
				}
				else if( CommandName == "unsetuptype" )
				{
					UnSetupType = Parent.ExpandString( CurrentCommand.CommandLine );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "unsetup" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.UnSetup;
				}
				else if( CommandName == "createdvdlayout" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CreateDVDLayout;
				}
				else if( CommandName == "sync" )
				{
					CurrentCommand.State = COMMANDS.SCC_Sync;
				}
				else if( CommandName == "synctohead" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_SyncToHead;
				}
				else if( CommandName == "artistsync" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_ArtistSync;
				}
				else if( CommandName == "getchanges" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_GetChanges;
				}
				else if( CommandName == "sendqachanges" )
				{
					CurrentCommand.State = COMMANDS.Config;
					IsSendingQAChanges = true;
				}
				else if( CommandName == "syncsinglechangelist" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_SyncSingleChangeList;
				}
				else if( CommandName == "checkout" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_Checkout;
				}
				else if( CommandName == "openfordelete" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_OpenForDelete;
				}
				else if( CommandName == "checkoutgame" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutGame;
				}
				else if( CommandName == "checkoutmanifest" )
				{
					CurrentCommand.State = COMMANDS.SCC_CheckoutManifest;
				}
				else if( CommandName == "checkoutgfwlgame" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutGFWLGame;
				}
				else if( CommandName == "checkoutgadcheckpoint" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutGADCheckpoint;
				}
				else if( CommandName == "checkoutshader" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutShader;
				}
				else if( CommandName == "checkoutdialog" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutDialog;
				}
				else if( CommandName == "checkoutfonts" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutFonts;
				}
				else if( CommandName == "checkoutlocpackage" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutLocPackage;
				}
				else if( CommandName == "checkoutgdf" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutGDF;
				}
				else if( CommandName == "checkoutcat" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutCat;
				}
				else if( CommandName == "checkoutlayout" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutLayout;
				}
				else if( CommandName == "checkouthashes" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutHashes;
				}
				else if( CommandName == "checkoutdlc" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutDLC;
				}
				else if( CommandName == "checkoutaftscreenshots" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutAFTScreenshots;
				}
				else if( CommandName == "checkoutconncache" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CheckoutConnCache;
				}
				else if( CommandName == "getnextchangelist" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_GetNextChangelist;
				}
				else if( CommandName == "submit" )
				{
					IsBuilding = true;
					IsTagging = false;
					CurrentCommand.State = COMMANDS.SCC_SignUnsubmitted;
				}
				else if( CommandName == "submitandtag" )
				{
					IsBuilding = true;
					IsTagging = true;
					CurrentCommand.State = COMMANDS.SCC_SignUnsubmitted;
				}
				else if( CommandName == "enablerestorenewerchanges" )
				{
					IsRestoringNewerChanges = true;
					Parent.Log( "The submission restore/no overwrite mode is now TRUE", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "disablerestorenewerchanges" )
				{
					IsRestoringNewerChanges = false;
					Parent.Log( "The submission restore/no overwrite mode is now FALSE", Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "createnewlabel" || CommandName == "createlabel" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_CreateNewLabel;
				}
				else if( CommandName == "updatelabeldescription" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_LabelUpdateDescription;
				}
				else if( CommandName == "revert" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_Revert;
				}
				else if( CommandName == "revertfilespec" || CommandName == "revertfile" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.SCC_RevertFileSpec;
				}
				else if( CommandName == "waitforjobs" )
				{
					CurrentCommand.State = COMMANDS.WaitForJobs;
				}
				else if( CommandName == "getalltargets" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.GetAllTargets;
				}
				else if( CommandName == "addunrealgamejob" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddUnrealGameJob;
				}
				else if( CommandName == "addunrealagnosticjob" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddUnrealAgnosticJob;
				}
				else if( CommandName == "addunrealagnosticjobnoeditor" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddUnrealAgnosticJobNoEditor;
				}
				else if( CommandName == "addunrealfullgamejob" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddUnrealFullGameJob;
				}
				else if( CommandName == "addunrealgfwlgamejob" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddUnrealGFWLGameJob;
				}
				else if( CommandName == "addunrealgfwlfullgamejob" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddUnrealGFWLFullGameJob;
				}
				else if( CommandName == "addconformjob" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AddConformJob;
				}
				else if( CommandName == "clean" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.Clean;
				}
				else if( CommandName == "cleanmacs" )
				{
					CurrentCommand.State = COMMANDS.CleanMacs;
				}
				else if( CommandName == "buildubt9" || CommandName == "buildubt" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.BuildUBT9;
					if( CommandName == "buildubt" )
					{
						Parent.SendWarningMail( "Deprecated command 'BuildUBT'", "Use BuildUBT9 instead. CommandID = " + CommandDetails.CommandID.ToString(), false );
						Parent.Log( "[DEPRECATED] BuildUBT", Color.Magenta );
					}
				}
				else if( CommandName == "buildubt10" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.BuildUBT10;
				}
				else if( CommandName == "remotebuildubt10" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.RemoteBuildUBT10;
				}
				else if( CommandName == "builduat10" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.BuildUAT10;
				}
				else if( CommandName == "ms9build" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.MS9Build;
				}
				else if( CommandName == "ms10build" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.MS10Build;
				}
				else if( CommandName == "unrealbuild" || CommandName == "unrealbuildtool" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.UnrealBuild;
					if( CommandName == "unrealbuildtool" )
					{
						Parent.SendWarningMail( "Deprecated command 'UnrealBuildTool'", "Use 'Tool' to define what to compile instead (as opposed to 'Game'). CommandID = " + CommandDetails.CommandID.ToString(), false );
						Parent.Log( "[DEPRECATED] UnrealBuildTool", Color.Magenta );
					}
				}
				else if( CommandName == "remoteunrealbuild" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.RemoteUnrealBuild;
				}
				else if( CommandName == "unrealautomationtool" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.UnrealAutomationTool;
				}
				else if( CommandName == "msvc9clean" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.MSVC9Clean;
				}
				else if( CommandName == "msvc9build" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.MSVC9Build;
				}
				else if( CommandName == "msvc9deploy" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.MSVC9Deploy;
				}
				else if( CommandName == "msvc9full" )
				{
					CurrentCommand.State = COMMANDS.MSVC9Full;
				}
				else if( CommandName == "msvc10clean" )
				{
					CurrentCommand.State = COMMANDS.MSVC10Clean;
					CurrentCommand.CommandDelegate = CurrentCommand.MSVC10Clean;
				}
				else if( CommandName == "msvc10build" )
				{
					CurrentCommand.State = COMMANDS.MSVC10Clean;
					CurrentCommand.CommandDelegate = CurrentCommand.MSVC10Build;
				}
				else if( CommandName == "msvc10deploy" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.MSVC10Deploy;
				}
				else if( CommandName == "msvc10full" )
				{
					CurrentCommand.State = COMMANDS.MSVC10Full;
				}
				else if( CommandName == "shaderclean" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.ShaderClean;
				}
				else if( CommandName == "shaderbuild" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.ShaderBuild;
				}
				else if( CommandName == "connectionbuild" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.ConnectionBuild;
				}
				else if( CommandName == "cookshaderbuild" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CookShaderBuild;
				}
				else if( CommandName == "shaderfull" )
				{
					CurrentCommand.State = COMMANDS.ShaderFull;
				}
				else if( CommandName == "ps3makepatchbinary" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PS3MakePatchBinary;
				}
				else if( CommandName == "ps3makepatch" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PS3MakePatch;
				}
				else if( CommandName == "ps3makedlc" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PS3MakeDLC;
				}
				else if( CommandName == "ps3maketu" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PS3MakeTU;
				}
				else if( CommandName == "pcmaketu" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PCMakeTU;
				}
				else if( CommandName == "pcpackagetu" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PCPackageTU;
				}
				else if( CommandName == "buildscript" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.BuildScript;
				}
				else if( CommandName == "buildscriptnoclean" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.BuildScriptNoClean;
				}
				else if( CommandName == "iphonepackage" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.iPhonePackage;
				}
				else if( CommandName == "androidpackage" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.AndroidPackage;
				}
				else if( CommandName == "iphonesetsigningprefix" )
				{
					SigningPrefixOverride = CurrentCommand.CommandLine;
					Parent.Log( "SigningPrefixOverride is now set to " + CurrentCommand.CommandLine, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "iphonesetdevelopersigningidentity" )
				{
					DeveloperSigningIdentityOverride = CurrentCommand.CommandLine;
					Parent.Log( "DeveloperSigningIdentityOverride is now set to " + CurrentCommand.CommandLine, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "iphonesetdistributionsigningidentity" )
				{
					DistributionSigningIdentityOverride = CurrentCommand.CommandLine;
					Parent.Log( "DistributionSigningIdentityOverride is now set to " + CurrentCommand.CommandLine, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "iphonecompileserver" )
				{
					iPhoneCompileServerOverride = CurrentCommand.CommandLine;
					Parent.Log( "iPhoneCompileServerOverride is now set to " + CurrentCommand.CommandLine, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "iphonesigningserver" )
				{
					iPhoneSigningServerOverride = CurrentCommand.CommandLine;
					Parent.Log( "iPhoneSigningServerOverride is now set to " + CurrentCommand.CommandLine, Color.Magenta );
					CurrentCommand.State = COMMANDS.Config;
				}
				else if( CommandName == "preheatmapoven" || CommandName == "cleanup" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PreHeatMapOven;
					if( CommandName == "cleanup" )
					{
						Parent.SendWarningMail( "Deprecated command 'CleanUp'", "Use PreHeatMapOven instead. CommandID = " + CommandDetails.CommandID.ToString(), false );
						Parent.Log( "[DEPRECATED] CleanUp", Color.Magenta );
					}
				}
				else if( CommandName == "preheatdlc" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PreHeatDLC;
				}
				else if( CommandName == "packagetu" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PackageTU;
				}
				else if( CommandName == "packagedlc" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PackageDLC;
				}
				else if( CommandName == "cookmaps" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.bIsCooking = true;
					CurrentCommand.CommandDelegate = CurrentCommand.CookMaps;
				}
				else if( CommandName == "cookinimaps" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.bIsCooking = true;
					CurrentCommand.CommandDelegate = CurrentCommand.CookIniMaps;
				}
				else if( CommandName == "cooksounds" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CookSounds;
				}
				else if( CommandName == "createhashes" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CreateHashes;
				}
				else if( CommandName == "updateddc" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.UpdateDDC;
				}
				else if( CommandName == "wrangle" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.Wrangle;
				}
				else if( CommandName == "publish" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.Publish;
					CurrentCommand.CommandDelegate = CurrentCommand.Publish;
				}
				else if( CommandName == "publishtagset" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishTagset;
				}
				else if( CommandName == "publishlanguage" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishLanguage;
				}
				else if( CommandName == "publishlayout" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishLayout;
				}
				else if( CommandName == "publishlayoutlanguage" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishLayoutLanguage;
				}
				else if( CommandName == "publishdlc" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishDLC;
				}
				else if( CommandName == "publishtu" )
				{
					bIsPublishing = true;
					CurrentCommand.bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishTU;
				}
				else if( CommandName == "publishfiles" )
				{
					bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishFiles;
				}
				else if( CommandName == "publishrawfiles" )
				{
					bIsPublishing = true;
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.PublishRawFiles;
				}
                else if (CommandName == "publishfolder")
                {
                    bIsPublishing = true;
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.PublishFolder;
                }
				else if( CommandName == "cookersyncreplacement" )
				{
					CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.CookerSyncReplacement;
				}
                else if (CommandName == "runbatchfile")
                {
                    bIsPublishing = true;
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.RunBatchFile;
                }
                else if (CommandName == "generatetoc")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.GenerateTOC;
                }
                else if (CommandName == "makeiso")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.MakeISO;
                }
                else if (CommandName == "makemd5")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.MakeMD5;
                }
                else if (CommandName == "copyfolder")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.CopyFolder;
                }
                else if (CommandName == "movefolder")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.MoveFolder;
                }
                else if (CommandName == "conform")
                {
                    CurrentCommand.State = COMMANDS.None;
					CurrentCommand.CommandDelegate = CurrentCommand.Conform;
				}
                else if (CommandName == "patchscript")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.PatchScript;
                }
                else if (CommandName == "checkpointgameassetdatabase")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.CheckpointGameAssetDatabase;
                }
                else if (CommandName == "updategameassetdatabase")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.UpdateGameAssetDatabase;
                }
                else if (CommandName == "tagreferencedassets")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.TagReferencedAssets;
                }
                else if (CommandName == "tagdvdassets")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.TagDVDAssets;
                }
                else if (CommandName == "auditcontent")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.AuditContent;
                }
                else if (CommandName == "findstaticmeshcanbecomedynamic")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.FindStaticMeshCanBecomeDynamic;
                }
                else if (CommandName == "fixupredirects")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.FixupRedirects;
                }
                else if (CommandName == "resavedeprecatedpackages")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.ResaveDeprecatedPackages;
                }
                else if (CommandName == "analyzereferencedcontent")
                {
                    AnalyzeReferencedContentConfiguration = Parent.ExpandString(CurrentCommand.CommandLine);
                    Parent.Log("AnalyzeReferencedContentConfiguration set to '" + AnalyzeReferencedContentConfiguration + "'", Color.Magenta);
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.AnalyzeReferencedContent;
                }
                else if (CommandName == "minecookedpackages")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.MineCookedPackages;
                }
                else if (CommandName == "contentcomparison")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.ContentComparison;
                }
                else if (CommandName == "dumpmapsummary")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.DumpMapSummary;
                }
                else if (CommandName == "trigger")
                {
                    CurrentCommand.State = COMMANDS.Trigger;
                }
                else if (CommandName == "cleantrigger")
                {
                    CurrentCommand.State = COMMANDS.CleanTrigger;
                }
                else if (CommandName == "autotrigger")
                {
                    CurrentCommand.State = COMMANDS.AutoTrigger;
                }
                else if (CommandName == "sqlexecint")
                {
                    CurrentCommand.State = COMMANDS.SQLExecInt;
                }
                else if (CommandName == "sqlexecdouble")
                {
                    CurrentCommand.State = COMMANDS.SQLExecDouble;
                }
                else if (CommandName == "validateinstall")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.ValidateInstall;
                }
                else if (CommandName == "validateshas")
                {
                    CurrentCommand.State = COMMANDS.ExtractSHAs;
                }
                else if (CommandName == "bumpagentversion")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.BumpAgentVersion;
                }
                else if (CommandName == "bumpengineversion")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.BumpEngineVersion;
                }
                else if (CommandName == "getengineversion")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.GetEngineVersion;
                }
                else if (CommandName == "updategdfversion")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.UpdateGDFVersion;
                }
                else if (CommandName == "makegfwlcat")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.MakeGFWLCat;
                }
                else if (CommandName == "zdpp")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.ZDPP;
                }
                else if (CommandName == "steamdrm")
                {
                    CurrentCommand.State = COMMANDS.SteamDRM;
                }
                else if (CommandName == "savedefines")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.SaveDefines;
                }
                else if (CommandName == "updatesymbolserver")
                {
                    CurrentCommand.State = COMMANDS.UpdateSourceServer;
                }
                else if (CommandName == "sign")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.CheckSigned;
                }
                else if (CommandName == "signcat")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.SignCat;
                }
                else if (CommandName == "signbinary")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.SignBinary;
                }
                else if (CommandName == "signfile")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.SignFile;
                }
                else if (CommandName == "trackfilesize")
                {
                    CurrentCommand.State = COMMANDS.TrackFileSize;
                }
                else if (CommandName == "trackfoldersize")
                {
                    CurrentCommand.State = COMMANDS.TrackFolderSize;
                }
                else if (CommandName == "wait")
                {
                    CurrentCommand.State = COMMANDS.Wait;
                }
                else if (CommandName == "checkforucinvcprojfiles")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.CheckForUCInVCProjFiles;
                }
                else if (CommandName == "smoketest")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.SmokeTest;
                }
                else if (CommandName == "loadpackages")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.LoadPackages;
                }
                else if (CommandName == "cookpackages")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.CookPackages;
                }
                else if (CommandName == "physxgeneratephysx")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.PhysXGeneratePhysX;
                }
                else if (CommandName == "physxgenerateapex")
                {
                    CurrentCommand.State = COMMANDS.None;
                    CurrentCommand.CommandDelegate = CurrentCommand.PhysXGenerateAPEX;
                }
                else
                {
                    ErrorLevel = COMMANDS.IllegalCommand;
                    CurrentCommand.State = COMMANDS.Error;
                    CurrentCommand.CommandLine = Line;
                }
            }
        }

		/**
		 * Init the collation types for processing changelists
		 */
		public void InitCollationTypes()
		{
			ValidGames = UnrealControls.GameLocator.LocateGames( Environment.CurrentDirectory );

			CollationTypes[( int )COLLATION.Engine] = new CollationType( COLLATION.Engine, "Engine Code Changes" );
			CollationTypes[( int )COLLATION.Rendering] = new CollationType( COLLATION.Rendering, "Rendering Code Changes" );
			CollationTypes[( int )COLLATION.Audio] = new CollationType( COLLATION.Audio, "Audio Code Changes" );
			CollationTypes[( int )COLLATION.Editor]	= new CollationType( COLLATION.Editor, "Editor Code Changes" );
			CollationTypes[( int )COLLATION.GFx] = new CollationType( COLLATION.GFx, "GFx Code Changes" );

			int GameCollation = ( int )COLLATION.Game;
			foreach( string ValidGame in ValidGames )
			{
				CollationTypes[GameCollation] = new CollationType( ( COLLATION )GameCollation, ValidGame + " Code Changes" );
				GameCollation++;
				CollationTypes[GameCollation] = new CollationType( ( COLLATION )GameCollation, ValidGame + " Map Changes" );
				GameCollation++;
				CollationTypes[GameCollation] = new CollationType( ( COLLATION )GameCollation, ValidGame + " Content Changes" );
				GameCollation++;
			}

			CollationTypes[( int )COLLATION.Windows] = new CollationType( COLLATION.Windows, "Windows Specific Changes" );
			CollationTypes[( int )COLLATION.Xbox360] = new CollationType( COLLATION.Xbox360, "Xbox360 Changes" );
			CollationTypes[( int )COLLATION.PS3] = new CollationType( COLLATION.PS3, "PS3 Changes" );
			CollationTypes[( int )COLLATION.WiiU] = new CollationType( COLLATION.WiiU, "WiiU Changes" );
			CollationTypes[( int )COLLATION.NGP] = new CollationType( COLLATION.iPhone, "NGP Changes" );
			CollationTypes[( int )COLLATION.iPhone] = new CollationType( COLLATION.iPhone, "iPhone Changes" );
			CollationTypes[( int )COLLATION.Android] = new CollationType( COLLATION.Android, "Android Changes" );
			CollationTypes[( int )COLLATION.Mac] = new CollationType( COLLATION.Mac, "Mac Changes" );
			CollationTypes[( int )COLLATION.Flash] = new CollationType( COLLATION.Mac, "Flash Changes" );
			CollationTypes[( int )COLLATION.Dingo] = new CollationType( COLLATION.Dingo, "Dingo Code Changes" );
			CollationTypes[( int )COLLATION.Mocha] = new CollationType( COLLATION.Mocha, "Mocha Code Changes" );

			CollationTypes[( int )COLLATION.OnlinePC] = new CollationType( COLLATION.OnlinePC, "Online PC Changes" );
			CollationTypes[( int )COLLATION.GameCenter] = new CollationType( COLLATION.GameSpy, "GameCenter Changes" );
			CollationTypes[( int )COLLATION.GameSpy] = new CollationType( COLLATION.GameSpy, "GameSpy Changes" );
			CollationTypes[( int )COLLATION.G4WLive] = new CollationType( COLLATION.G4WLive, "G4W Live Changes" );
			CollationTypes[( int )COLLATION.Steam] = new CollationType( COLLATION.Steam, "Steam Changes" );

			CollationTypes[( int )COLLATION.Infrastructure] = new CollationType( COLLATION.Infrastructure, "Infrastructure Changes" );
			CollationTypes[( int )COLLATION.Install] = new CollationType( COLLATION.Install, "Installer Changes" );
			CollationTypes[( int )COLLATION.Swarm] = new CollationType( COLLATION.Swarm, "Swarm Changes" );
			CollationTypes[( int )COLLATION.Lightmass] = new CollationType( COLLATION.Lightmass, "Lightmass Changes" );
			CollationTypes[( int )COLLATION.Tools] = new CollationType( COLLATION.Tools, "Tools Changes" );
			CollationTypes[( int )COLLATION.Loc] = new CollationType( COLLATION.Loc, "Localisation Changes" );

			// Set the folders to look for 
			CollationTypes[( int )COLLATION.Engine].SetFolderMatches( 
				new List<string>()
				{
					// UE3
					"/development/src/core/",
					"/development/src/engine/",
					"/development/src/gameframework/",
					"/development/src/ipdrv/",
					"/development/src/launch/",
					"/engine/config/", 
					"/engine/content/",
					"/engine/shaders/",
					// UE4
					"/engine/source/runtime/",
					"/engine/source/thirdparty/"

				} );

			CollationTypes[( int )COLLATION.Rendering].SetFolderMatches(
				new List<string>()
				{
					"/development/src/es2drv/",
					"/development/src/opengldrv/",
					"/development/src/d3d9drv/",
					"/development/src/d3d11drv/"
				} );

			CollationTypes[( int )COLLATION.Audio].SetFolderMatches(
				new List<string>()
				{
					"/development/src/alaudio/",
					"/development/src/coreaudio/",
					"/development/src/xaudio2/"
				} );

			CollationTypes[( int )COLLATION.Editor].SetFolderMatches(
				new List<string>()
				{
					// UE3
					"/development/src/unrealed/",
					"/engine/editorresources/facefx/",
					"/engine/editorresources/wpf/controls/",
					"/engine/editorresources/wx/",
					// UE4
					"/engine/source/editor/"
				} );

			CollationTypes[( int )COLLATION.GFx].SetFolderMatches(
				new List<string>()
				{
					"/development/src/gfxui/",
					"/development/src/gfxuieditor/",
					"/development/external/gfx/"
				} );

			CollationTypes[( int )COLLATION.Windows].SetFolderMatches(
				new List<string>()
				{
					"/development/src/windrv/",
					"/development/src/windows/"
				} );

			CollationTypes[( int )COLLATION.Xbox360].SetFolderMatches(
				new List<string>()
				{
					"/development/src/xenon/",
					"/development/tools/xenon/",
					"/development/src/xbox360/"
				} );

			CollationTypes[( int )COLLATION.PS3].SetFolderMatches(
				new List<string>()
				{
					"/development/src/ps3/"
				} );

			CollationTypes[( int )COLLATION.WiiU].SetFolderMatches(
				new List<string>()
				{
					"/development/src/wiiu/"
				} );

			CollationTypes[( int )COLLATION.NGP].SetFolderMatches(
				new List<string>()
				{
					"/development/src/ngp/"
				} );

			CollationTypes[( int )COLLATION.iPhone].SetFolderMatches(
				new List<string>()
				{
					"/development/tools/mobile/iphone/",
					"/development/src/iphone/",
				} );

			CollationTypes[( int )COLLATION.Android].SetFolderMatches(
				new List<string>()
				{
					"/development/src/android/"
				} );

			CollationTypes[( int )COLLATION.Mac].SetFolderMatches(
				new List<string>()
				{
					"/development/src/mac/",
					"/development/tools/macpackager/"
				} );

			CollationTypes[( int )COLLATION.Flash].SetFolderMatches(
				new List<string>()
				{
					"/development/src/flash/"
				} );

			CollationTypes[( int )COLLATION.Dingo].SetFolderMatches(
				new List<string>()
				{
					"/engine/source/runtime/dingo"
				} );

			CollationTypes[( int )COLLATION.Mocha].SetFolderMatches(
				new List<string>()
				{
					"/engine/source/runtime/mocha"
				} );

			CollationTypes[( int )COLLATION.OnlinePC].SetFolderMatches(
				new List<string>()
				{
					"/development/src/onlinesubsystempc/",
				} );

			CollationTypes[( int )COLLATION.GameCenter].SetFolderMatches(
				new List<string>()
				{
					"/development/src/onlinesubsystemgamecenter/"
				} );

			CollationTypes[( int )COLLATION.GameSpy].SetFolderMatches(
				new List<string>()
				{
					"/development/src/onlinesubsystemgamespy/"
				} );

			CollationTypes[( int )COLLATION.G4WLive].SetFolderMatches(
				new List<string>()
				{
					"/development/src/onlinesubsystemlive/"
				} );

			CollationTypes[( int )COLLATION.Steam].SetFolderMatches(
				new List<string>()
				{
					"/development/src/onlinesubsystemsteamworks/"
				} );

			CollationTypes[( int )COLLATION.Infrastructure].SetFolderMatches(
				new List<string>()
				{
					"/development/builder/",
					"/development/tools/builder/",
					"/development/tools/unrealprop/",
					"/development/src/targets/",
					"/development/src/unrealbuildtool/",
					"/development/src/xenon/unrealbuildtool/",
					"/development/src/ps3/unrealbuildtool/",
					"/development/src/wiiu/unrealbuildtool/",
					"/binaries/cookersync.xml",

					// UE4
					"/Engine/Source/Programs/UnrealAutomationTool/",
					"/Engine/Source/Programs/UnrealBuildTool/",
				} );

			CollationTypes[( int )COLLATION.Install].SetFolderMatches(
				new List<string>()
				{
					"/development/tools/unsetup/"
				} );

			CollationTypes[( int )COLLATION.Swarm].SetFolderMatches(
				new List<string>()
				{
					"/development/src/unrealswarm/",
					"/development/tools/unrealswarm/"
				} );

			CollationTypes[( int )COLLATION.Lightmass].SetFolderMatches(
				new List<string>()
				{
					"/development/tools/unreallightmass/"
				} );

			CollationTypes[( int )COLLATION.Tools].SetFolderMatches(
				new List<string>()
				{
					// UE3
					"/development/tools/",
					// UE4
					"/engine/source/developer/",
					"/engine/source/programs/"
				} );

			CollationTypes[( int )COLLATION.Loc].SetFolderMatches(
				new List<string>()
				{
					"/engine/localization/",
					"/engine/editorresources/wpf/localized/"
				} );

			// Set the order of the reports 
			Reports.Enqueue( COLLATION.Engine );
			Reports.Enqueue( COLLATION.Rendering );
			Reports.Enqueue( COLLATION.Audio );
			Reports.Enqueue( COLLATION.Editor );
			Reports.Enqueue( COLLATION.GFx );

			int GameStep = ValidGames.Count;

			// Add in source changes per game
			GameCollation = ( int )COLLATION.Game;
			foreach( string ValidGame in ValidGames )
			{
				Reports.Enqueue( ( COLLATION )GameCollation );
				GameCollation += 3;
			}

			// Add in map changes per game
			GameCollation = ( ( int )COLLATION.Game ) + 1;
			foreach( string ValidGame in ValidGames )
			{
				Reports.Enqueue( ( COLLATION )GameCollation );
				GameCollation += 3;
			}

			// Add in content changes per game
			GameCollation = ( ( int )COLLATION.Game ) + 2;
			foreach( string ValidGame in ValidGames )
			{
				Reports.Enqueue( ( COLLATION )GameCollation );
				GameCollation += 3;
			}

			Reports.Enqueue( COLLATION.Windows );
			Reports.Enqueue( COLLATION.Xbox360 );
			Reports.Enqueue( COLLATION.PS3 );
			Reports.Enqueue( COLLATION.WiiU );
			Reports.Enqueue( COLLATION.NGP );
			Reports.Enqueue( COLLATION.iPhone );
			Reports.Enqueue( COLLATION.Android );
			Reports.Enqueue( COLLATION.Mac );
			Reports.Enqueue( COLLATION.Flash );
			Reports.Enqueue( COLLATION.Dingo );
			Reports.Enqueue( COLLATION.Mocha );

			Reports.Enqueue( COLLATION.OnlinePC );
			Reports.Enqueue( COLLATION.GameCenter );
			Reports.Enqueue( COLLATION.GameSpy );
			Reports.Enqueue( COLLATION.G4WLive );
			Reports.Enqueue( COLLATION.Steam );

			Reports.Enqueue( COLLATION.Infrastructure );
			Reports.Enqueue( COLLATION.Install );
			Reports.Enqueue( COLLATION.Swarm );
			Reports.Enqueue( COLLATION.Lightmass );
			Reports.Enqueue( COLLATION.Tools );
			Reports.Enqueue( COLLATION.Loc );

			// Remember the prioritised order of the reporting tags
			AllReports = new List<COLLATION>( Reports );
			AllReports.Add( COLLATION.Count );
		}

		// Iterate over all submitted changelists, and extract a list of users
		public string[] GetPerforceUsers()
		{
			List<string> Users = new List<string>();

			// Extract unique P4 users
			foreach( ChangeList CL in ChangeLists )
			{
				if( !Users.Contains( CL.User.ToLower() ) )
				{
					Users.Add( CL.User.ToLower() );
				}
			}

			Users.Sort();
			return ( Users.ToArray() );
		}

        // Goes over a changelist description and extracts the relevant information
        public void ProcessChangeList( P4Record Record )
        {
			// Add any changelist that isn't from builder to a list for later processing
			if( Record.Fields["user"].ToLower() != "buildmachine" )
			{
				ChangeList CL = new ChangeList();

				CL.Description = Record.Fields["desc"];
				CL.CleanDescription();

				// Get the latest changelist number in this build - even if it is from the builder
				CL.Number = SafeStringToInt( Record.Fields["change"] );
				if( CL.Number > LabelInfo.Changelist )
				{
					LabelInfo.Changelist = CL.Number;
				}

				CL.User = Record.Fields["user"];
				CL.Time = SafeStringToInt( Record.Fields["time"] );

				if( Record.ArrayFields["depotFile"] != null )
				{
					foreach( string DepotFile in Record.ArrayFields["depotFile"] )
					{
						CL.Files.Add( "    ... " + DepotFile );
					}
				}

				ChangeLists.Add( CL );
			}
        }

        private COLLATION CollateGameChanges( COLLATION NewCollation, ChangeList CL, string CleanFile, string Game, int GameCollation )
        {
			// Cannot override a previously found collation of higher priority
			if( GameCollation >= ( int )NewCollation )
			{
				return NewCollation;
			}

			// Check for an extension the same as the game name - this is a map
            if( CleanFile.IndexOf( "." + Game ) >= 0 )
            {
				return ( ( COLLATION )( GameCollation + 1 ) );
            }
			// Check for game specific UE3 source files
            else if( CleanFile.IndexOf( "development/src/" + Game ) >= 0 )
            {
				return ( ( COLLATION )GameCollation );
            }
			// Check for game specific UE4 source files
			else if( CleanFile.IndexOf( Game + "/source" ) >= 0 )
			{
				return ( ( COLLATION )GameCollation );
			}
			// Check for content changes
			else if( CleanFile.IndexOf( Game + "game/" ) >= 0 )
            {
				return ( ( COLLATION )( GameCollation + 2 ) );
            }

			return ( NewCollation );
        }

		private COLLATION CheckCollation( COLLATION NewCollation, string CleanFile, COLLATION Check )
		{
			// Cannot override a previously found collation of higher priority
			if( Check >= NewCollation )
			{
				return NewCollation;
			}

			// See if the file name contains any of the folders for this type
			foreach( string FolderMatch in CollationTypes[( int )Check].FolderMatches )
			{
				if( CleanFile.IndexOf( FolderMatch ) >= 0 )
				{
					return Check;
				}
			}

			return NewCollation;
		}

        private void CollateChanges()
        {
			if( ChangelistsCollated == true )
			{
				return;
			}

            foreach( ChangeList CL in ChangeLists )
            {
				COLLATION NewCollation = COLLATION.Count;

				// Check files that were checked in
				foreach( string File in CL.Files )
				{
					string CleanFile = File.ToLower().Replace( '\\', '/' );

					// Skip all development folders for BUNs
					if( CleanFile.Contains( "game/content/developers/" ) )
					{
						continue;
					}

					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Engine );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Rendering );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Audio );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Editor );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.GFx );

					int GameCollation = ( int )COLLATION.Game;
					foreach( string ValidGame in ValidGames )
					{
						NewCollation = CollateGameChanges( NewCollation, CL, CleanFile, ValidGame.ToLower(), GameCollation );
						GameCollation += 3;
					}

					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Windows );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Xbox360 );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.PS3 );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.WiiU );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.NGP );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.iPhone );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Android );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Mac );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Flash );

					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.OnlinePC );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.GameCenter );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.GameSpy );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.G4WLive );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Steam );

					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Infrastructure );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Install );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Swarm );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Lightmass );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Tools );
					NewCollation = CheckCollation( NewCollation, CleanFile, COLLATION.Loc );

					if( AllReports.IndexOf( NewCollation ) < AllReports.IndexOf( CL.Collate ) )
					{
						CL.Collate = NewCollation;
					}
				}

				if( NewCollation == COLLATION.Count )
				{
					NewCollation = COLLATION.Engine;
				}

				if( CL.Collate < COLLATION.Count )
				{
					CollationTypes[( int )CL.Collate].Active = true;
				}

				Parent.Log( " ... changelist " + CL.Number.ToString() + " placed in collation " + CL.Collate.ToString(), Color.DarkGreen );
            }

			ChangelistsCollated = true;
        }

        private bool GetCollatedChanges( StringBuilder Changes, COLLATION Collate, string User )
        {
            bool CollTypeHasEntries = false;
            CollationType CollType = CollationTypes[( int )Collate];
            if( CollType.Active )
            {
                foreach( ChangeList CL in ChangeLists )
                {
                    if( CL.Collate == Collate && ( User.Length == 0 || User.ToLower() == CL.User.ToLower() ) )
                    {
                        CollTypeHasEntries = true;
                        break;
                    }
                }

                if( CollTypeHasEntries )
                {
                    Changes.Append( "------------------------------------------------------------" + Environment.NewLine );
                    Changes.Append( "\t" + CollType.Title + Environment.NewLine );
                    Changes.Append( "------------------------------------------------------------" + Environment.NewLine + Environment.NewLine );

                    foreach( ChangeList CL in ChangeLists )
                    {
                        if( CL.Collate == Collate && ( User.Length == 0 || User.ToLower() == CL.User.ToLower() ) )
                        {
                            DateTime Time = new DateTime( 1970, 1, 1 );
                            Time += new TimeSpan( ( long )CL.Time * 10000 * 1000 );
                            Changes.Append( "Change: " + CL.Number.ToString() + " by " + CL.User + " on " + Time.ToLocalTime().ToString() + Environment.NewLine );
                            Changes.Append( CL.Description + Environment.NewLine );
                        }
                    }
                }
            }

            return ( CollTypeHasEntries );
        }

        public string GetChanges( string User )
        {
            bool HasChanges = false;
            StringBuilder Changes = new StringBuilder();

            Changes.Append( LabelInfo.Description );

            Changes.Append( Environment.NewLine );

            CollateChanges();

            foreach( COLLATION Collation in Reports )
            {
                HasChanges |= GetCollatedChanges( Changes, Collation, User );
            }

            if( HasChanges )
            {
                return ( Changes.ToString() );
            }

            return ( "" );
        }
	}
}
