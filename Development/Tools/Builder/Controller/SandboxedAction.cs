// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.Globalization;
using System.IO;
using System.Net.Sockets;
using System.Reflection;
using System.Text;
using System.Xml;
using System.Xml.XPath;
using System.Xml.Serialization;

using Controller.Models;

namespace Controller
{
	public class JobInfo
	{
		[XmlAttribute]
		public string Name = "";

		[XmlAttribute]
		public string Command = "";

		[XmlAttribute]
		public string Parameter = "";
	}

	public class JobDescriptions
	{
		[XmlArray( "Jobs" )]
		public JobInfo[] Jobs = new JobInfo[0];

		public JobDescriptions()
		{
		}
	}

	/// <summary>
	/// A class containing the rules to complete an artist sync
	/// </summary>
	public class ArtistSyncRules
	{
		[XmlElement]
		public string PromotionLabel;
		[XmlArray]
		public List<string> FilesToClean;
		[XmlArray]
		public List<string> Rules;

		public ArtistSyncRules()
		{
			PromotionLabel = "";
			FilesToClean = new List<string>();
			Rules = new List<string>();
		}
	}

	/// <summary>
	/// A class containing all the special functions that are triggered by build scripts
	/// </summary>
	public partial class SandboxedAction
	{
		public delegate MODES CommandDelegateType();
		public CommandDelegateType CommandDelegate = null;
		public COMMANDS State = COMMANDS.None;
		public string CommandLine = "";
		public bool bIsPublishing = false;
		public bool bIsCooking = false;

		private Main Parent = null;
		private P4 SCC = null;
		private BuildState Builder = null;

		private BuildProcess CurrentBuild = null;
		private DateTime StartTime = DateTime.UtcNow;
		private DateTime LastRespondingTime = DateTime.UtcNow;

		private string CommonCommandLine = "-unattended -nopause -buildmachine -forcelogflush";

		public SandboxedAction( Main InParent, P4 InSCC, BuildState InBuilder )
		{
			Parent = InParent;
			SCC = InSCC;
			Builder = InBuilder;

			// Add source control info to the common command line params.
			CommonCommandLine += " -GADBranchName=" + Builder.BranchDef.Branch;
			CommonCommandLine += " -P4Port=" + Builder.BranchDef.Server;
			CommonCommandLine += " -P4User=" + Builder.BranchDef.User;
			CommonCommandLine += " -P4Passwd=" + Builder.BranchDef.Password;
            CommonCommandLine += " -P4Client=" + Parent.MachineName;
		}

		public COMMANDS GetErrorLevel()
		{
			return ( State );
		}

		public BuildProcess GetCurrentBuild()
		{
			return ( CurrentBuild );
		}

		public int GetExitCode()
		{
			if( CurrentBuild != null )
			{
				return CurrentBuild.ExitCode;
			}

			return 0;
		}

		private void CleanIniFiles( GameConfig Config )
		{
			string ConfigFolder = Config.GetConfigFolderName();
			DirectoryInfo Dir = new DirectoryInfo( ConfigFolder );

			if( Dir.Exists )
			{
				foreach( FileInfo File in Dir.GetFiles() )
				{
					Builder.Write( " ... checking: " + File.Name );
					if( File.IsReadOnly == false )
					{
						File.Delete();
						Builder.Write( " ...... deleted: " + File.Name );
					}
				}
			}
		}

		private void CleanArtistSyncFiles( List<string> FilePatterns )
		{
			foreach( string FilePattern in FilePatterns )
			{
				try
				{
					string FullPath = Path.GetFullPath( FilePattern );
					string Folder = Path.GetDirectoryName( FullPath );
					string FileSpec = Path.GetFileName( FullPath );

					DirectoryInfo DirInfo = new DirectoryInfo( Folder );
					if( DirInfo.Exists )
					{
						FileInfo[] FileInfos = DirInfo.GetFiles( FileSpec );
						foreach( FileInfo Info in FileInfos )
						{
							Info.IsReadOnly = false;
							Info.Delete();
						}
					}
				}
				catch
				{
				}
			}
		}

		private void AddSimpleJob( string Command, string Parameter )
		{
			JobInfo Job = new JobInfo();

			Job.Name = "Job";
			if( Builder.LabelInfo.Game.Length > 0 )
			{
				Job.Name += "_" + Builder.LabelInfo.Game;
			}
			if( Builder.LabelInfo.Platform.Length > 0 )
			{
				Job.Name += "_" + Builder.LabelInfo.Platform;
			}
			Job.Name += "_" + Builder.BuildConfig;

			Job.Command = Command;
			Job.Parameter = Parameter;

			Parent.AddJob( Job );

			Builder.Write( "Added Job: " + Job.Name );
		}

		public MODES AddUnrealGameJob()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddUnrealGameJob ), false );

				AddSimpleJob( "Jobs/UnrealGameJob", Builder.GetCurrentCommandLine() );

				GameConfig Config = Builder.CreateGameConfig( false );
				Builder.LabelInfo.Games.Add( Config );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddUnrealGameJob;
				Builder.Write( "Error: exception while adding a game job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES AddUnrealAgnosticJob()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddUnrealAgnosticJob ), false );

				AddSimpleJob( "Jobs/UnrealAgnosticJob", Builder.GetCurrentCommandLine() );

				GameConfig Config = Builder.CreateGameConfig( false );
				Builder.LabelInfo.Games.Add( Config );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddUnrealAgnosticJob;
				Builder.Write( "Error: exception while adding a game agnostic job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES AddUnrealAgnosticJobNoEditor()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddUnrealAgnosticJobNoEditor ), false );

				AddSimpleJob( "Jobs/UnrealAgnosticJobNoEditor", Builder.GetCurrentCommandLine() );

				GameConfig Config = Builder.CreateGameConfig( false );
				Builder.LabelInfo.Games.Add( Config );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddUnrealAgnosticJobNoEditor;
				Builder.Write( "Error: exception while adding a game agnostic non editor job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES AddUnrealFullGameJob()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddUnrealFullGameJob ), false );

				AddSimpleJob( "Jobs/UnrealFullGameJob", Builder.GetCurrentCommandLine() );

				GameConfig Config = Builder.CreateGameConfig( false );
				Builder.LabelInfo.Games.Add( Config );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddUnrealFullGameJob;
				Builder.Write( "Error: exception while adding a full game job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES AddUnrealGFWLGameJob()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddUnrealGFWLGameJob ), false );

				AddSimpleJob( "Jobs/UnrealGFWLGameJob", Builder.GetCurrentCommandLine() );

				GameConfig Config = Builder.CreateGameConfig( false );
				Builder.LabelInfo.Games.Add( Config );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddUnrealGFWLGameJob;
				Builder.Write( "Error: exception while adding a GFWL game job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES AddUnrealGFWLFullGameJob()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddUnrealGFWLFullGameJob ), false );

				AddSimpleJob( "Jobs/UnrealGFWLFullGameJob", Builder.GetCurrentCommandLine() );

				GameConfig Config = Builder.CreateGameConfig( false );
				Builder.LabelInfo.Games.Add( Config );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddUnrealGFWLFullGameJob;
				Builder.Write( "Error: exception while adding a GFWL full game job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES AddConformJob()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.AddConformJob ), false );

				AddSimpleJob( "Jobs/ConformJob", Builder.GetCurrentCommandLine() );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.AddConformJob;
				Builder.Write( "Error: exception while adding a conform job." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}


		private string GetWorkingDirectory( string Path, ref string Solution )
		{
			string Directory = "";
			Solution = "";

			int Index = Path.LastIndexOf( '/' );
			if( Index >= 0 )
			{
				Directory = Path.Substring( 0, Index );
				Solution = Path.Substring( Index + 1, Path.Length - Index - 1 );
			}

			return ( Directory );
		}

		private string GetCWD( int BranchVersion )
		{
			if( BranchVersion >= 10 )
			{
				return "Engine/Source";
			}

			return "Development/Src";
		}

		private MODES BuildUBT( string DotNetVer )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.BuildUBT9 ), false );

				string MSBuildAppName = Environment.GetEnvironmentVariable( "FrameworkDir" ) + "/" + DotNetVer + "/MSBuild.exe";

				string CommandLine;
				if( Builder.BranchDef.Version >= 10 )
				{
					CommandLine = "../../Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.csproj /verbosity:normal /target:Rebuild /property:Configuration=\"Development\" /property:Platform=\"AnyCPU\"";
				}
				else
				{
					CommandLine = "UnrealBuildTool/UnrealBuildTool.csproj /verbosity:normal /target:Rebuild /property:Configuration=\"Release\"";
				}
				CurrentBuild = new BuildProcess( Parent, Builder, MSBuildAppName, CommandLine, GetCWD( Builder.BranchDef.Version ), false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.BuildUBT9;
				Builder.Write( "Error: exception while starting to build UnrealBuildTool (" + DotNetVer + ")" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES BuildUBT9()
		{
			return BuildUBT( "v3.5" );
		}

		public MODES BuildUBT10()
		{
			return BuildUBT( "v4.0.30319" );
		}

        private void RemoteProcedureCall( string RemoteHost, string WorkingDirectory, string ExecutableName, string Arguments )
        {
            FileInfo RPCUtilityInfo = new FileInfo("Engine/Binaries/DotNET/RPCUtility.exe");
            FileInfo TempRPCLocation = new FileInfo(Path.Combine(Path.GetTempPath(), "RPCUtility.exe"));
            if (TempRPCLocation.Exists)
            {
                TempRPCLocation.IsReadOnly = false;
            }

            RPCUtilityInfo.CopyTo(TempRPCLocation.FullName, true);

            string RPCExecutable = TempRPCLocation.FullName;
            string RPCArguments = RemoteHost + " " + WorkingDirectory;
            string AllArguments = RPCArguments + " " + ExecutableName + " " + Arguments;

            CurrentBuild = new BuildProcess(Parent, Builder, RPCExecutable, AllArguments, "", false);
            State = CurrentBuild.GetErrorLevel();
        }

		private MODES RemoteBuildUBT( string DotNetVer )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.RemoteBuildUBT10 ), false );

                string Host = Builder.iPhoneCompileServerOverride != "" ? Builder.iPhoneCompileServerOverride : Parent.DefaultIPhoneCompileServerName;
                string WorkingDirectory = "/UnrealEngine3/Builds/" + Parent.MachineName + "-MAC";
                string ExecutableName = "xbuild";
				string[] Arguments = new string[]
				{
                    "/verbosity:normal",
                    "/target:Rebuild",
                    "/property:Configuration=\"Development\"",
                    "UE4/Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool_Mono.csproj"
				};

                RemoteProcedureCall(Host, WorkingDirectory, ExecutableName, String.Join(" ", Arguments));

                StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.RemoteBuildUBT10;
				Builder.Write( "Error: exception while starting to build UnrealBuildTool (" + DotNetVer + ")" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES RemoteBuildUBT10()
		{
			return RemoteBuildUBT( "v4.0.30319" );
		}

		private MODES BuildUAT( string DotNetVer )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.BuildUAT10 ), false );

				string MSBuildAppName = Environment.GetEnvironmentVariable( "FrameworkDir" ) + "/" + DotNetVer + "/MSBuild.exe";
				string CommandLine = "../../Engine/Source/Programs/UnrealAutomationTool/UnrealAutomationTool.csproj /verbosity:normal /target:Rebuild /property:Configuration=\"Development\" /property:Platform=\"AnyCPU\"";

				CurrentBuild = new BuildProcess( Parent, Builder, MSBuildAppName, CommandLine, GetCWD( Builder.BranchDef.Version ), false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.BuildUAT10;
				Builder.Write( "Error: exception while starting to build UnrealAutomationTool (" + DotNetVer + ")" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES BuildUAT10()
		{
			return BuildUAT( "v4.0.30319" );
		}

		private MODES MS_Build( COMMANDS Command, string MSBuildAppName )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( Command ), false );

				Builder.HandleMSVCDefines();

				string CommandLine = "";
				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 1 )
				{
					Builder.Write( "Error: incorrect number of parameters. Usage: MSBuild <Project>" );
					State = COMMANDS.MSBuild;
				}
				else
				{
					CommandLine = "\"" + Parms[0] + "\" /verbosity:normal /target:Rebuild /property:Configuration=\"" + Builder.BuildConfig + "\"";

					CurrentBuild = new BuildProcess( Parent, Builder, MSBuildAppName, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.MSBuild;
				Builder.Write( "Error: exception while starting to build using MSBuild" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MS9Build()
		{
			return MS_Build( COMMANDS.MS9Build, Builder.ToolConfig.MSBuild9Application );
		}

		public MODES MS10Build()
		{
			return MS_Build( COMMANDS.MS10Build, Builder.ToolConfig.MSBuild10Application );
		}

		private MODES MSVCClean( COMMANDS Command, string AppName )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( Command ), false );

				Builder.HandleMSVCDefines();

				string CommandLine = "";
				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 || Parms.Length > 2 )
				{
					Builder.Write( "Error: incorrect number of parameters. Usage: MSVCClean <Solution> [Project]" );
					State = COMMANDS.MSVCClean;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					if( Parms.Length == 1 )
					{
						CommandLine = "\"" + Parms[0] + ".sln\" /clean \"" + Config.Configuration + "\"";
					}
					else if( Parms.Length == 2 )
					{
						CommandLine = "\"" + Parms[0] + ".sln\" /project \"" + Parms[1] + "\" /clean \"" + Config.Configuration + "\"";
					}

					CurrentBuild = new BuildProcess( Parent, Builder, AppName, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.MSVCClean;
				Builder.Write( "Error: exception while starting to clean using devenv" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MSVC9Clean()
		{
			return MSVCClean( COMMANDS.MSVC9Clean, Builder.ToolConfig.MSVC9Application );
		}												   

		public MODES MSVC10Clean()
		{
			return MSVCClean( COMMANDS.MSVC10Clean, Builder.ToolConfig.MSVC10Application );
		}

		private MODES MSVCBuild( COMMANDS Command, string AppName )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( Command ), false );

				Builder.HandleMSVCDefines();

				string CommandLine = "";
				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 || Parms.Length > 2 )
				{
					Builder.Write( "Error: incorrect number of parameters. Usage: MSVCBuild <Solution> [Project]" );
					State = COMMANDS.MSVCBuild;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					if( Parms.Length == 1 )
					{
						CommandLine = "\"" + Parms[0] + ".sln\" /build \"" + Config.Configuration + "\"";
					}
					else if( Parms.Length == 2 )
					{
						CommandLine = "\"" + Parms[0] + ".sln\" /project \"" + Parms[1] + "\" /build \"" + Config.Configuration + "\"";
					}

					CurrentBuild = new BuildProcess( Parent, Builder, AppName, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.MSVCBuild;
				Builder.Write( "Error: exception while starting to build using devenv" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MSVC9Build()
		{
			return MSVCBuild( COMMANDS.MSVC9Build, Builder.ToolConfig.MSVC9Application );
		}

		public MODES MSVC10Build()
		{
			return MSVCBuild( COMMANDS.MSVC10Build, Builder.ToolConfig.MSVC10Application );
		}

		private MODES MS_Deploy( COMMANDS Command )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( Command ), false );

				// Use the version of MSBuild that matches the Visual Studio version we're building with
				string MSBuildAppName;
				if( Command == COMMANDS.MSVC9Deploy )
				{
					MSBuildAppName = Environment.GetEnvironmentVariable( "FrameworkDir" ) + "/v3.5/MSBuild.exe";
				}
				else if( Command == COMMANDS.MSVC10Deploy )
				{
					MSBuildAppName = Environment.GetEnvironmentVariable( "FrameworkDir" ) + "/v4.0.30319/MSBuild.exe";
				}
				else
				{
					throw new Exception( "Unsupported version of Visual Studio" );
				}

				Builder.HandleMSVCDefines();

				string CommandLine = "";
				string[] Params = Builder.GetCurrentCommandLine().Split( ",".ToCharArray() );
				if( Params.Length != 3 )
				{
					Builder.Write( "Error: incorrect number of parameters. Usage: MSVCDeploy SolutionName,ProjectName,PublishLocation" );
					State = COMMANDS.MSVCDeploy;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					CommandLine = String.Format( "/t:{1}:publish /v:normal /property:Configuration=\"{3}\";PublishDir=\"{2}\" {0}.sln",
						Params[0],
						Params[1],
						Params[2],
						Config.Configuration );

					CurrentBuild = new BuildProcess( Parent, Builder, MSBuildAppName, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.MSVCDeploy;
				Builder.Write( "Error: exception while starting to publish using msbuild" );
				Builder.Write( "Error: " + Ex.ToString() );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MSVC9Deploy()
		{
			return MS_Deploy( COMMANDS.MSVC9Deploy );
		}

		public MODES MSVC10Deploy()
		{
			return MS_Deploy( COMMANDS.MSVC10Deploy );
		}

		public MODES UnrealBuild()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.UnrealBuild ), false );

				bool bAllowXGE = !Builder.bXGEHasFailed && Builder.AllowXGE;

				string UBTConfigName = "Release";
				if( Builder.BranchDef.Version >= 10 )
				{
					UBTConfigName = "Development";
				}

				string Executable = "";
				if( Builder.BranchDef.Version >= 10 )
				{
					Executable = "Engine/Intermediate/BuildData/UnrealBuildTool/" + UBTConfigName + "/UnrealBuildTool.exe";
				}
				else
				{
					Executable = "Development/Intermediate/UnrealBuildTool/" + UBTConfigName + "/UnrealBuildTool.exe";
				}
				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = Config.GetUBTCommandLine( Builder.BranchDef.Version, Builder.CommandDetails.bIsPrimaryBuild, bAllowXGE, Builder.AllowPCH, Builder.LabelInfo.bIsTool );
				if( Builder.UnityStressTest )
				{
					CommandLine += " -StressTestUnity";
				}
				if( Builder.UnityDisable )
				{
					CommandLine += " -DisableUnity";
				}

				CommandLine += Builder.HandleUBTDefines( Builder.BranchDef.Version );

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, GetCWD( Builder.BranchDef.Version ), false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.UnrealBuild;
				Builder.Write( "Error: exception while starting to build using UnrealBuildTool" );
				Builder.Write( "Error: " + Ex.ToString() );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		/// <summary>
		/// Spawn UnrealBuildTool on the Mac under mono
		/// </summary>
		/// <returns>Monitor - watch the process for completion</returns>
		public MODES RemoteUnrealBuild()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.RemoteUnrealBuild ), false );

                string Host = Builder.iPhoneCompileServerOverride != "" ? Builder.iPhoneCompileServerOverride : Parent.DefaultIPhoneCompileServerName;
                string WorkingDirectory = "/UnrealEngine3/Builds/" + Parent.MachineName + "-MAC";
                string ExecutableName = "mono";

                string UBTExecutableFilePath = "UE4/Engine/Intermediate/BuildData/UnrealBuildTool/Development/UnrealBuildTool.exe";
				bool bAllowXGE = !Builder.bXGEHasFailed && Builder.AllowXGE;
				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = UBTExecutableFilePath;
                CommandLine += " " + Config.GetUBTCommandLine( Builder.BranchDef.Version, Builder.CommandDetails.bIsPrimaryBuild, bAllowXGE, Builder.AllowPCH, Builder.LabelInfo.bIsTool );

				if( Builder.UnityStressTest )
				{
					CommandLine += " -StressTestUnity";
				}

				if( Builder.UnityDisable )
				{
					CommandLine += " -DisableUnity";
				}

				CommandLine += Builder.HandleUBTDefines( Builder.BranchDef.Version );

                RemoteProcedureCall(Host, WorkingDirectory, ExecutableName, CommandLine);

                StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.UnrealBuild;
				Builder.Write( "Error: exception while starting to build using RemoteUnrealBuildTool" );
				Builder.Write( "Error: " + Ex.ToString() );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES UnrealAutomationTool()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.UnrealAutomationTool ), false );

				string Executable = "Engine/Intermediate/BuildData/UnrealAutomationTool/Development/UnrealAutomationTool.exe";
				string CommandLine = Builder.GetCurrentCommandLine();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.UnrealAutomationTool;
				Builder.Write( "Error: exception while starting to use UnrealAutomationTool" );
				Builder.Write( "Error: " + Ex.ToString() );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES ShaderClean()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ShaderClean );
				Builder.OpenLog( LogFileName, false );

				string ShaderName;
				FileInfo Info;

				GameConfig Config = Builder.CreateGameConfig();

				// Delete ref shader cache
				ShaderName = Config.GetRefShaderName();
				Builder.Write( " ... checking for: " + ShaderName );
				Info = new FileInfo( ShaderName );
				if( Info.Exists )
				{
					Info.IsReadOnly = false;
					Info.Delete();
					Builder.Write( " ...... deleted: " + ShaderName );
				}

				// Delete local shader cache
				ShaderName = Config.GetLocalShaderName();
				Builder.Write( " ... checking for: " + ShaderName );
				Info = new FileInfo( ShaderName );
				if( Info.Exists )
				{
					Info.IsReadOnly = false;
					Info.Delete();
					Builder.Write( " ...... deleted: " + ShaderName );
				}

				CleanIniFiles( Config );

				State = COMMANDS.None;
				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.ShaderClean;
				Builder.Write( "Error: exception while starting to clean precompiled shaders" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES ShaderBuild()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ShaderBuild );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				if( Builder.GetCurrentCommandLine().Length > 0 )
				{
					string EncodedFoldername = Builder.GetFolderName();
					Executable = Config.GetUDKComName( Executable, Builder.BranchDef.Branch, EncodedFoldername, Builder.GetCurrentCommandLine() );
				}

				string ShaderPlatform = Config.GetShaderPlatform( Builder.BranchDef.Version );
				CommandLine += "PrecompileShaders Platform=" + ShaderPlatform + " -RefCache -ALLOW_PARALLEL_PRECOMPILESHADERS " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.ShaderBuild;
				Builder.Write( "Error: exception while starting to build precompiled shaders" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CookShaderBuild()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CookShaderBuild );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				if( Builder.GetCurrentCommandLine().Length > 0 )
				{
					string EncodedFoldername = Builder.GetFolderName();
					Executable = Config.GetUDKComName( Executable, Builder.BranchDef.Branch, EncodedFoldername, Builder.GetCurrentCommandLine() );
				}

				string ShaderPlatform = Config.GetShaderPlatform( Builder.BranchDef.Version );
				CommandLine += "PrecompileShaders Platform=" + ShaderPlatform + " -SkipMaps  -precook " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CookShaderBuild;
				Builder.Write( "Error: exception while starting to build local precompiled shaders for cooking" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES ConnectionBuild()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ConnectionBuild );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();

				// Delete the local connection caches
				DeletePatternFromFolder( Builder.LabelInfo.Game + "Game\\Content", "LocalConnCache.upk", false );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "BuildRefConnCache " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.ConnectionBuild;
				Builder.Write( "Error: exception while starting to build connection cache." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES PS3MakePatchBinary()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PS3MakePatchBinary );
				string[] Parameters = Builder.SplitCommandline();
				if( Parameters.Length != 2 )
				{
					State = COMMANDS.PS3MakePatchBinary;
				}
				else
				{
					string CommandLine = "/c make_fself_npdrm " + Parameters[0] + " " + Parameters[1] + "  > " + LogFileName + " 2>&1";

					CurrentBuild = new BuildProcess( Parent, Builder, CommandLine, "" );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PS3MakePatchBinary;
			}

			return MODES.Monitor;
		}

		public MODES PS3MakePatch()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PS3MakePatch ), false );

				string[] Parameters = Builder.SplitCommandline();

				if( Parameters.Length != 2 )
				{
					State = COMMANDS.PS3MakePatch;
					Builder.Write( "Error: incorrect number of parameters" );
				}
				else
				{
					string Executable = "make_package_npdrm.exe";
					string CommandLine = "--patch " + Parameters[0] + " " + Parameters[1];

					GameConfig Config = Builder.CreateGameConfig();
					string WorkingDirectory = Config.GetPatchFolderName();

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, WorkingDirectory, true );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				Builder.Write( "Error: exception while starting to make PS3 patch" );
				State = COMMANDS.PS3MakePatch;
			}

			return MODES.Monitor;
		}

		public MODES PS3MakeDLC()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PS3MakeDLC ), false );

				string[] Parameters = Builder.SplitCommandline();

				if( Parameters.Length != 2 )
				{
					State = COMMANDS.PS3MakeDLC;
					Builder.Write( "Error: incorrect number of parameters (needs 2: the DLC directory and the region" );
				}
				else
				{
					// setup PS3Packager run
					string Executable = "Binaries/PS3/PS3Packager.exe";
					string DLCName = Parameters[0];
					string Region = Parameters[1];

					GameConfig Config = Builder.CreateGameConfig();
					string CommandLine = "DLC " + Config.GameName + " " + DLCName + " " + Region;

					// we can run from anywhere
					string WorkingDirectory = Environment.CurrentDirectory;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, WorkingDirectory, false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				Builder.Write( "Error: exception while starting to make PS3 DLC" );
				State = COMMANDS.PS3MakeDLC;
			}

			return MODES.Monitor;
		}

		public MODES PS3MakeTU()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PS3MakeTU ), false );

				string[] Parameters = Builder.SplitCommandline();

				if( Parameters.Length != 2 )
				{
					State = COMMANDS.PS3MakeTU;
					Builder.Write( "Error: incorrect number of parameters (needs 2: the TU directory name and the region" );
				}
				else
				{
					// setup PS3Packager run
					string Executable = "Binaries/PS3/PS3Packager.exe";
					string TUName = Parameters[0];
					string Region = Parameters[1];

					GameConfig Config = Builder.CreateGameConfig();
					string CommandLine = "PATCH " + Config.GameName + " " + TUName + " " + Region;

					// we can run from anywhere
					string WorkingDirectory = Environment.CurrentDirectory;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, WorkingDirectory, false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				Builder.Write( "Error: exception while starting to make PS3 TU" );
				State = COMMANDS.PS3MakeTU;
			}

			return MODES.Monitor;
		}

		public MODES PCMakeTU()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PCMakeTU ), false );

				string[] Parameters = Builder.SplitCommandline();

				if( Parameters.Length != 1 )
				{
					State = COMMANDS.PCMakeTU;
					Builder.Write( "Error: incorrect number of parameters (needs 1: the TU directory name" );
				}
				else
				{
					// setup PCPackager run
					string Executable = "Binaries/Win32/PCPackager.exe";
					string TUName = Parameters[0];

					GameConfig Config = Builder.CreateGameConfig();
					string CommandLine = Config.GameName + " " + TUName;

					// we can run from anywhere
					string WorkingDirectory = Environment.CurrentDirectory;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, WorkingDirectory, false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				Builder.Write( "Error: exception while starting to make PC TU" );
				State = COMMANDS.PCMakeTU;
			}

			return MODES.Monitor;
		}

		public MODES PCPackageTU()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PCPackageTU ), false );

				string[] Parameters = Builder.SplitCommandline();

				if( Parameters.Length != 1 )
				{
					State = COMMANDS.PCPackageTU;
					Builder.Write( "Error: incorrect number of parameters (needs 1: the TU directory name" );
				}
				else
				{
					string Executable = "cmd.exe";
					string TUName = Parameters[0];

					GameConfig Config = Builder.CreateGameConfig();
					string WorkingDirectory = Environment.CurrentDirectory + "/" + Config.GameName + "/Build/PCConsole";
					string CommandLine = "/c PackageTitleUpdate.bat " + TUName;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, WorkingDirectory, false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				Builder.Write( "Error: exception while starting to package PC TU" );
				State = COMMANDS.PCPackageTU;
			}

			return MODES.Monitor;
		}

		public MODES PackageTU()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PackageTU );
				Builder.OpenLog( LogFileName, false );

				if( Builder.LabelInfo.BuildVersion.Build == 0 )
				{
					Builder.Write( "Error: while package title update." );
					Builder.Write( "Error: no engine version found; has BumpEngineVersion/GetEngineVersion been called?" );
					State = COMMANDS.PackageTU;
				}
				else
				{
					string[] Parameters = Builder.SplitCommandline();

					string Executable = "";
					switch( Builder.LabelInfo.Platform.ToLower() )
					{
					case "xbox360":
						Executable = "Binaries\\Xbox360\\XboxPackager.exe";
						break;
					default:
						break;
					}

					if( Executable.Length == 0 )
					{
						State = COMMANDS.PackageTU;
					}
					else
					{
						GameConfig Config = Builder.CreateGameConfig();

						string EngineVersion = Builder.LabelInfo.BuildVersion.Build.ToString();
						string CommandLine = Builder.LabelInfo.Game + " TU " + Parameters[0] + " /verbose /EngineVersion=" + EngineVersion + " /BuildConfig=" + Config.Configuration + Builder.GetPackageTUConfig();
						CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", false );
						State = CurrentBuild.GetErrorLevel();
					}
				}

				StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.PackageTU;
				Builder.Write( "Error: exception while starting to PackageTU: " + Ex.Message );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES PackageDLC()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PackageDLC );
				Builder.OpenLog( LogFileName, false );

				string[] Parameters = Builder.SplitCommandline();

				string Executable = "";
				switch( Builder.LabelInfo.Platform.ToLower() )
				{
				case "xbox360":
					Executable = "Binaries\\Xbox360\\XboxPackager.exe";
					break;
				default:
					break;
				}

				if( Executable.Length == 0 )
				{
					State = COMMANDS.PackageDLC;
				}
				else
				{
					string CommandLine = Builder.LabelInfo.Game + " DLC " + Parameters[0] + " /verbose";
					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.PackageDLC;
				Builder.Write( "Error: exception while starting to PackageDLC: " + Ex.Message );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		private MODES BuildScript( COMMANDS CommandID )
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( CommandID );
				Builder.OpenLog( LogFileName, false );

				if( Builder.GetCurrentCommandLine().Length == 0 )
				{
					Builder.Write( "Error: missing required parameter. Usage: BuildScript <Game>." );
					State = CommandID;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig( Builder.GetCurrentCommandLine() );
					CleanIniFiles( Config );

					string CommandLine = "";
					string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
					CommandLine += "make";
					if( CommandID == COMMANDS.BuildScript )
					{
						CommandLine += " -full";
					}
					CommandLine += " -silentbuild -stripsource " + CommonCommandLine + " " + Builder.GetScriptConfiguration();
					if( !Builder.AllowSloppyScript )
					{
						CommandLine += " -warningsaserrors";
					}

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = CommandID;
				Builder.Write( "Error: exception while starting to build script" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES BuildScript()
		{
			return BuildScript( COMMANDS.BuildScript );
		}

		public MODES BuildScriptNoClean()
		{
			return BuildScript( COMMANDS.BuildScriptNoClean );
		}

		public MODES Commandlet()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.Commandlet );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );

				CommandLine += String.Join( " ", Parms ) + " " + CommonCommandLine + Builder.GetOptionalCommandletConfig();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch( Exception Ex )
			{
				State = COMMANDS.Commandlet;

				Builder.Write( "Error: Exception when executing commandlet" + Ex.ToString() );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES iPhonePackage()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.iPhonePackage );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				string PackageCommandLine = "PackageIPA " + Config.GameName + " " + Config.Configuration;

				string ExtraFlags = " -compress=best -strip";
				if( Builder.GetCurrentCommandLine().Length != 0 )
				{
					ExtraFlags += " " + Builder.GetCurrentCommandLine();
				}

				string CWD = "Binaries\\" + Config.Platform;
				string Executable = CWD + "\\iPhonePackager.exe";
				string CommandLine = PackageCommandLine + ExtraFlags;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, CWD, false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.iPhonePackage;
				Builder.Write( "Error: exception while starting to package IPA" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES AndroidPackage()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.AndroidPackage );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				string CommandLine = Config.GameName + " " + Config.Platform + " " + Config.Configuration;

				if( Builder.GetCurrentCommandLine().Length != 0 )
				{
					CommandLine += " " + Builder.GetCurrentCommandLine();
				}

				string CWD = "Binaries\\" + Config.Platform;
				string Executable = CWD + "\\AndroidPackager.exe";

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, CWD, false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.iPhonePackage;
				Builder.Write( "Error: exception while starting to package IPA" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		private void DeleteLocalShaderCache( GameConfig Game )
		{
			string[] LocalShaderCaches = Game.GetLocalShaderNames();
			foreach( string LocalShaderCache in LocalShaderCaches )
			{
				Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + LocalShaderCache + "'" );
				FileInfo LCSInfo = new FileInfo( LocalShaderCache );
				if( LCSInfo.Exists )
				{
					LCSInfo.IsReadOnly = false;
					LCSInfo.Delete();
					Builder.Write( " ... done" );
				}
			}
		}

		private void DeleteGlobalShaderCache( GameConfig Game )
		{
			string[] GlobalShaderCaches = Game.GetGlobalShaderNames();
			foreach( string GlobalShaderCache in GlobalShaderCaches )
			{
				Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + GlobalShaderCache + "'" );
				FileInfo GCSInfo = new FileInfo( GlobalShaderCache );
				if( GCSInfo.Exists )
				{
					GCSInfo.IsReadOnly = false;
					GCSInfo.Delete();
					Builder.Write( " ... done" );
				}
			}
		}

		private bool DeletePatternFromFolder( string Folder, string Pattern, bool Recurse )
		{
			Builder.Write( "Attempting delete of '" + Pattern + "' from '" + Folder + "'" );
			try
			{
				DirectoryInfo DirInfo = new DirectoryInfo( Folder );
				if( DirInfo.Exists )
				{
					Builder.Write( "Deleting '" + Pattern + "' from: '" + Builder.BranchDef.Branch + "/" + Folder + "'" );
					FileInfo[] Files = DirInfo.GetFiles( Pattern );
					foreach( FileInfo File in Files )
					{
						if( File.Exists )
						{
							File.IsReadOnly = false;
							File.Delete();
						}
					}

					if( Recurse )
					{
						DirectoryInfo[] Dirs = DirInfo.GetDirectories();
						foreach( DirectoryInfo Dir in Dirs )
						{
							DeletePatternFromFolder( Dir.FullName, Pattern, Recurse );
						}
					}
				}
			}
			catch( Exception Ex )
			{
				Builder.Write( "Error: Exception when deleting pattern from folder: " + Ex.ToString() );
				return false;
			}

			return true;
		}

		public MODES PreHeatMapOven()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PreHeatMapOven ), false );

				if( Builder.GetCurrentCommandLine().Length > 0 )
				{
					Builder.Write( "Error: too many parameters. Usage: PreheatMapOven." );
					State = COMMANDS.PreHeatMapOven;
				}
				else if( Builder.LabelInfo.Game.Length == 0 )
				{
					Builder.Write( "Error: no game defined for PreheatMapOven." );
					State = COMMANDS.PreHeatMapOven;
				}
				else
				{
					// Create a game config to interrogate
					GameConfig Config = Builder.CreateGameConfig();

					// Delete the cooked folder to start from scratch
					string CookedFolder = Config.GetCookedFolderName( Builder.BranchDef.Version );
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + CookedFolder + "'" );
					if( Directory.Exists( CookedFolder ) )
					{
						Parent.DeleteDirectory( CookedFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete any old wrangled content
					string CutdownPackagesFolder = Config.GetWrangledFolderName();
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + CutdownPackagesFolder + "'" );
					if( Directory.Exists( CutdownPackagesFolder ) )
					{
						Parent.DeleteDirectory( CutdownPackagesFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete the config folder to start from scratch
					string ConfigFolder = Config.GetCookedConfigFolderName( Builder.BranchDef.Version );
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ConfigFolder + "'" );
					if( Directory.Exists( ConfigFolder ) )
					{
						Parent.DeleteDirectory( ConfigFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete any screenshots
					DeletePatternFromFolder( Builder.LabelInfo.Game + "Game\\Screenshots", "*.bmp", false );
					DeletePatternFromFolder( Builder.LabelInfo.Game + "Game\\Screenshots", "*.png", false );

					// Delete the encrypted shaders
					string EncryptedShaderFolder = "Engine\\Shaders\\Binaries";
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + EncryptedShaderFolder + "'" );
					if( Directory.Exists( EncryptedShaderFolder ) )
					{
						Parent.DeleteDirectory( EncryptedShaderFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete any map summary data
					string MapSummaryFolder = Config.GetDMSFolderName();
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + MapSummaryFolder + "'" );
					if( Directory.Exists( MapSummaryFolder ) )
					{
						Parent.DeleteDirectory( MapSummaryFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete any map patch work files
					string PatchWorkFolder = Config.GetPatchesFolderName();
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + PatchWorkFolder + "'" );
					if( Directory.Exists( PatchWorkFolder ) )
					{
						Parent.DeleteDirectory( PatchWorkFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete any generated mobile shaders/keys
					string MobileShadersFolder = Config.GetMobileShadersFolderName();
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + MobileShadersFolder + "'" );
					if( Directory.Exists( MobileShadersFolder ) )
					{
						Parent.DeleteDirectory( MobileShadersFolder, 0 );
						Builder.Write( " ... done" );
					}

					// Delete any ZDPP files
					string ZDP32Redist = "Binaries\\Win32\\Zdp";
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ZDP32Redist + "'" );
					if( Directory.Exists( ZDP32Redist ) )
					{
						Parent.DeleteDirectory( ZDP32Redist, 0 );
						Builder.Write( " ... done" );
					}

					string ZDP64Redist = "Binaries\\Win64\\Zdp";
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ZDP64Redist + "'" );
					if( Directory.Exists( ZDP64Redist ) )
					{
						Parent.DeleteDirectory( ZDP64Redist, 0 );
						Builder.Write( " ... done" );
					}

					string ZDP32SdkOutput = "Binaries\\Win32\\ZdpSdkOutput";
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ZDP32SdkOutput + "'" );
					if( Directory.Exists( ZDP32SdkOutput ) )
					{
						Parent.DeleteDirectory( ZDP32SdkOutput, 0 );
						Builder.Write( " ... done" );
					}

					string ZDP64SdkOutput = "Binaries\\Win64\\ZdpSdkOutput";
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ZDP64SdkOutput + "'" );
					if( Directory.Exists( ZDP64SdkOutput ) )
					{
						Parent.DeleteDirectory( ZDP64SdkOutput, 0 );
						Builder.Write( " ... done" );
					}

					DeletePatternFromFolder( "Binaries\\Win32", "*.zdp", false );
					DeletePatternFromFolder( "Binaries\\Win64", "*.zdp", false );

					// Delete any stale TOC files
					DeletePatternFromFolder( Builder.LabelInfo.Game + "Game", "*.txt", false );

					// Delete the config folder to start from scratch
					DeletePatternFromFolder( Builder.LabelInfo.Game + "Game/Localization", "Coalesced*", false );

					// Delete any leftover files from layout creation
					DeletePatternFromFolder( ".", "*.", false );
					DeletePatternFromFolder( ".", "default.xex", false );
					DeletePatternFromFolder( ".", "*.xdb", false );

					// Delete the local connection caches
					DeletePatternFromFolder( Builder.LabelInfo.Game + "Game\\Content", "LocalConnCache.upk", false );

					// Delete the local shader caches
					DeleteLocalShaderCache( Config );

					// Delete the local global caches
					DeleteGlobalShaderCache( Config );

					Builder.Write( "Deleting ini files" );
					CleanIniFiles( Config );

					Builder.ClearPublishDestinations();
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.PreHeatMapOven;
				Builder.Write( "Error: exception while cleaning cooked data." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES PreHeatDLC()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.PreHeatDLC ), false );

				Parent.Log( "PreHeating DLC directories for: " + Builder.DLCName, Color.Magenta );
				if( Builder.GetCurrentCommandLine().Length > 0 )
				{
					Builder.Write( "Error: too many parameters. Usage: PreheatDLC." );
					State = COMMANDS.PreHeatDLC;
				}
				else if( Builder.LabelInfo.Game.Length == 0 )
				{
					Builder.Write( "Error: no game defined for PreheatDLC." );
					State = COMMANDS.PreHeatDLC;
				}
				else if( Builder.DLCName.Length == 0 )
				{
					Builder.Write( "Error: no modname defined for PreheatDLC." );
					State = COMMANDS.PreHeatDLC;
				}
				else
				{
					// Create a game config to abstract the folder names
					GameConfig Config = Builder.CreateGameConfig();

					// Delete the DLC cooked folder to start from scratch
					string[] CookedFolders = Config.GetDLCCookedFolderNames( Builder.DLCName, Builder.BranchDef.Version );

					foreach( string CookedFolder in CookedFolders )
					{
						Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + CookedFolder + "'" );
						if( Directory.Exists( CookedFolder ) )
						{
							Parent.DeleteDirectory( CookedFolder, 0 );
							Builder.Write( " ... done" );
						}
					}

					// Delete the created DLC file(s)
					string DLCFolder = Config.GetDLCFolderName( Builder.DLCName );
					DirectoryInfo DirInfo = new DirectoryInfo( DLCFolder );
					if( DirInfo.Exists )
					{
						foreach( FileInfo Info in DirInfo.GetFiles( "*." ) )
						{
							if( Info.Exists )
							{
								Info.IsReadOnly = false;
								Info.Delete();
							}
						}
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.PreHeatDLC;
				Builder.Write( "Error: exception while cleaning cooked DLC data." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		// Recursively delete an entire directory tree
		private void PrintDirectory( string Path, int Level )
		{
			DirectoryInfo DirInfo = new DirectoryInfo( Path );
			if( DirInfo.Exists )
			{
				string Indent = "";
				for( int IndentLevel = 0; IndentLevel < Level; IndentLevel++ )
				{
					Indent += "    ";
				}
				Builder.Write( Indent + Path );

				DirectoryInfo[] Directories = DirInfo.GetDirectories();
				foreach( DirectoryInfo Directory in Directories )
				{
					PrintDirectory( Directory.FullName, Level + 1 );
				}

				Indent += "    ";
				FileInfo[] Files = DirInfo.GetFiles();
				foreach( FileInfo File in Files )
				{
					Builder.Write( Indent + File.Name );
				}
			}
		}

		private void PrintCurrentDriveStats( string LogicalDrive )
		{
			try
			{
				DriveInfo LogicalDriveInfo = new DriveInfo( LogicalDrive );
				Builder.Write( "    Drive " + LogicalDrive );
				Builder.Write( "        Total disk size : " + LogicalDriveInfo.TotalSize.ToString() );
				Builder.Write( "        Total free size : " + LogicalDriveInfo.AvailableFreeSpace.ToString() );
			}
			catch( System.Exception Ex )
			{
				Builder.Write( "        Error getting drive stats: " + Ex.Message );
			}
		}

		public MODES Clean()
		{
			Builder.OpenLog( Builder.GetLogFileName( COMMANDS.Clean ), false );

			List<string> IntermediateFolders = new List<string>();
			if( Builder.BranchDef.Version >= 10 )
			{
				IntermediateFolders.Add( "Engine/Intermediate/BuildData" );
				DirectoryInfo DirInfo = new DirectoryInfo( "." );
				foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories( "*Game" ) )
				{
					IntermediateFolders.Add( Path.Combine( SubDirInfo.Name, "Intermediate", "BuildData" ) );
				}
			}
			else
			{
				IntermediateFolders.Add( "Development/Intermediate" );
			}

			try
			{
				// Log out some drive stats before and after the clean
				Builder.Write( "Drive stats before clean:" );
				PrintCurrentDriveStats( Directory.GetDirectoryRoot( "." ) );

				foreach( string IntermediateFolder in IntermediateFolders )
				{
					// Delete object and other compilation work files
					Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + IntermediateFolder + "'" );

					if( Directory.Exists( IntermediateFolder ) )
					{
						if( Parent.DeleteDirectory( IntermediateFolder, 0 ) )
						{
							Builder.Write( " ... done" );
						}
						else
						{
							State = COMMANDS.Clean;

							// Enumerate the contents of the directory for debugging
							Builder.Write( "Folder structure -" );
							PrintDirectory( IntermediateFolder, 0 );
						}
					}
				}

				Builder.Write( "Drive stats after clean:" );
				PrintCurrentDriveStats( Directory.GetDirectoryRoot( "." ) );
			}
			catch( System.Exception Ex )
			{
				State = COMMANDS.Clean;
				Parent.Log( Ex.Message, Color.Red );
				Builder.Write( "Error: Exception while cleaning: '" + Ex.Message + "'" );
			}

			Builder.CloseLog();
			return MODES.Finalise;
		}

		public MODES CleanMac()
		{
			Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CleanMac ), false );

			try
			{
				Builder.Write( "Cleaning remote iPhone/Mac build directories for Mac: " + Builder.GetCurrentCommandLine() );

				FileInfo RPCUtilityInfo = new FileInfo( "Binaries/RPCUtility.exe" );
				if( !RPCUtilityInfo.Exists )
				{
					RPCUtilityInfo = new FileInfo( "Binaries/iPhone/RPCUtility.exe" );
				}

				if( RPCUtilityInfo.Exists )
				{
					string CommandLine = Builder.GetCurrentCommandLine() + " / rm -rf /UnrealEngine3/Builds/" + Parent.MachineName;
					CurrentBuild = new BuildProcess( Parent, Builder, RPCUtilityInfo.FullName, CommandLine, "", true );
				}
				else
				{
					Builder.Write( "Error: Failed to find RPCUtility.exe" );
				}

				StartTime = DateTime.UtcNow;
			}
			catch( System.Exception Ex )
			{
				State = COMMANDS.CleanMac;
				Parent.Log( Ex.Message, Color.Red );
				Builder.Write( "Error: Exception while cleaning Mac up: '" + Ex.Message + "'" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		private string GetMTCommand()
		{
			int NumProcesses = Parent.PhysicalMemory / 5;
			if( NumProcesses > 28 )
			{
				NumProcesses = 28;
			}
			return ( " -Processes=" + NumProcesses.ToString() );
		}

		public MODES CookMaps()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CookMaps );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "CookPackages -Platform=" + Config.GetCookedFolderPlatform();
				CommandLine += String.Join( " ", Parms ) + " " + CommonCommandLine;

				string Language = Builder.LabelInfo.Language;
				if( Language.Length == 0 )
				{
					Language = "INT";
				}

				CommandLine += " -LanguageForCooking=" + Language + Builder.GetScriptConfiguration() + Builder.GetCookConfiguration();
				CommandLine += GetMTCommand();
				CommandLine += Builder.GetContentPath() + Builder.GetDLCName();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CookMaps;
				Builder.Write( "Error: exception while starting to cook" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CookIniMaps()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CookIniMaps );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "CookPackages -Platform=" + Config.GetCookedFolderPlatform();
				CommandLine += " -MapIniSection=" + Builder.GetCurrentCommandLine();
				CommandLine += " " + CommonCommandLine;

				Queue<string> Languages = Builder.GetLanguages();
				Queue<string> TextLanguages = Builder.GetTextLanguages();

				if( Languages.Count > 0 || TextLanguages.Count > 0 )
				{
					CommandLine += " -multilanguagecook=INT";
					foreach( string Language in Languages )
					{
						if( Language != "INT" )
						{
							CommandLine += "+" + Language;
						}
					}

					foreach( string Language in TextLanguages )
					{
						if( Language != "INT" )
						{
							CommandLine += "-" + Language;
						}
					}

					CommandLine += " -sha";
				}
				else
				{
					string Language = Builder.LabelInfo.Language;
					if( Language.Length == 0 )
					{
						Language = "INT";
					}

					CommandLine += " -LanguageForCooking=" + Language;
				}

				CommandLine += Builder.GetScriptConfiguration() + Builder.GetCookConfiguration();
				CommandLine += GetMTCommand();
				CommandLine += Builder.GetContentPath() + Builder.GetDLCName();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CookIniMaps;
				Builder.Write( "Error: exception while starting to cook from maps from ini" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CookSounds()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CookSounds );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 1 )
				{
					Builder.Write( "Error: Incorrect number of parameters. Usage: " + COMMANDS.CookSounds.ToString() + " <Package>" );
					State = COMMANDS.CookSounds;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					string CommandLine = "";
					string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
					CommandLine += "ResavePackages -ResaveClass=SoundNodeWave -ForceSoundRecook";

					Queue<string> Languages = Builder.GetLanguages();
					foreach( string Language in Languages )
					{
						CommandLine += " -Package=" + Parms[0];
						if( Language.Length > 0 && Language != "INT" )
						{
							CommandLine += "_" + Language;
						}
					}

					CommandLine += " " + CommonCommandLine;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CookSounds;
				Builder.Write( "Error: exception while starting to cook sounds" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CreateHashes()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CreateHashes );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "CookPackages -Platform=" + Config.GetCookedFolderPlatform();

				CommandLine += " -sha -inisonly " + CommonCommandLine;

				string Language = Builder.LabelInfo.Language;
				if( Language.Length == 0 )
				{
					Language = "INT";
				}
				CommandLine += " -LanguageForCooking=" + Language + Builder.GetScriptConfiguration();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CreateHashes;
				Builder.Write( "Error: exception while starting to create hashes" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES UpdateDDC()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.UpdateDDC );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "DerivedDataCache -fill " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.UpdateDDC;
				Builder.Write( "Error: exception while starting to update the DDC" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES Wrangle()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.Wrangle );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: too few parameters. Usage: Wrangle <section>." );
					State = COMMANDS.Wrangle;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();
					CleanIniFiles( Config );

					Builder.Write( "Deleting cutdown packages folder ... " );
					Config.DeleteCutdownPackages( Parent );
					Builder.Write( " ... cutdown packages folder deleted " );

					string CommandLine = "";
					string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
					CommandLine += "WrangleContent ";

					CommandLine += "SECTION=" + Parms[0] + " ";

					CommandLine += "-nosaveunreferenced ";

					if( Builder.KeepAssets.Length > 0 )
					{
						CommandLine += "-PlatformsToKeep=" + Builder.KeepAssets + " ";
					}

					if( Builder.StripSourceContent )
					{
						CommandLine += "-StripLargeEditorData ";
					}

					CommandLine += CommonCommandLine;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.Wrangle;
				Builder.Write( "Error: exception while starting to wrangle" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		private void Publish( COMMANDS Command, string Tagset, bool AddToReport, string AdditionalOptions )
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( Command ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: too few parameters. Usage: " + Command.ToString() + " <Dest1> [Dest2...]" );
					State = Command;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					string Executable = "Binaries/CookerSync.exe";
					string CookerSyncCommandLine = Builder.LabelInfo.Game + " -p " + Config.GetCookedFolderPlatform();

					string Language = Builder.LabelInfo.Language;
					if( Language.Length == 0 )
					{
						Language = "INT";
					}

					CookerSyncCommandLine += " -r " + Language;
					CookerSyncCommandLine += " -b " + Builder.BranchDef.Branch + Builder.GetSubBranch();
					CookerSyncCommandLine += " -x " + Tagset;
					CookerSyncCommandLine += " -crc -l";

					switch( Builder.PublishVerification )
					{
					case BuildState.PublishVerificationType.MD5:
					default:
						CookerSyncCommandLine += " -v";
						break;

					case BuildState.PublishVerificationType.None:
						break;
					}

					if( Builder.ForceCopy )
					{
						CookerSyncCommandLine += " -f";
					}
					CookerSyncCommandLine += AdditionalOptions;

					// Get the timestamped folder name with game and platform
					string DestFolder = "";
					if( Builder.IncludeTimestampFolder )
					{
						DestFolder = "\\" + Builder.GetFolderName();
					}

					// Set to publish to a zip file if requested
					switch( Builder.PublishMode )
					{
					case BuildState.PublishModeType.Zip:
						DestFolder += ".zip";
						break;

					case BuildState.PublishModeType.Iso:
						DestFolder += ".iso";
						break;

					default:
						// no extension by default - free files
						break;
					}

					// Add in all the destination paths
					for( int i = 0; i < Parms.Length; i++ )
					{
						string PublishFolder = Parms[i].Replace( '/', '\\' ) + DestFolder;
						CookerSyncCommandLine += " " + PublishFolder;
						if( AddToReport )
						{
							Builder.AddPublishDestination( PublishFolder );
						}
					}

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CookerSyncCommandLine, Environment.CurrentDirectory + "/Binaries", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = Command;
				Builder.Write( "Error: exception while starting to publish" );
				Builder.CloseLog();
			}
		}

		public MODES PublishTagset()
		{
			// Extract the tagset name
			string[] Parms = Builder.SplitCommandline();
			if( Parms.Length < 2 )
			{
				Builder.Write( "Error: too few parameters. Usage: " + COMMANDS.PublishTagset.ToString() + " <Tagset> <Dest1> [<Dest2>...]" );
				State = COMMANDS.PublishTagset;
			}
			else
			{
				string TagsetName = Parms[0];
				Builder.SetCurrentCommandLine( Builder.GetCurrentCommandLine().Substring( TagsetName.Length ) );

				Publish( COMMANDS.PublishTagset, TagsetName, true, "" );
			}

			return MODES.Monitor;
		}

		public MODES Publish()
		{
			Publish( COMMANDS.Publish, "CompleteBuild", true, "" );
			return MODES.Monitor;
		}

		public MODES PublishLanguage()
		{
			Publish( COMMANDS.PublishLanguage, "Loc", false, "" );
			return MODES.Monitor;
		}

		public MODES PublishLayout()
		{
			Publish( COMMANDS.PublishLayout, "CompleteBuild", true, " -notoc" );
			return MODES.Monitor;
		}

		public MODES PublishLayoutLanguage()
		{
			Publish( COMMANDS.PublishLayout, "Loc", false, " -notoc" );
			return MODES.Monitor;
		}

		public MODES PublishDLC()
		{
			Publish( COMMANDS.PublishDLC, "DLC", true, "" );
			return MODES.Monitor;
		}

		public MODES PublishTU()
		{
			Publish( COMMANDS.PublishTU, "TU", true, "" );
			return MODES.Monitor;
		}

		public MODES PublishFiles()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PublishFiles );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: PublishFiles <Dest> [Src] <FilePatten>." );
					State = COMMANDS.PublishFiles;
				}
				else
				{
					string ActiveFolder = Builder.GetFolderName();
					string SourceFolder = ".\\";
					string PublishFolder = ".\\";
					string FilePattern = "";

					if( Parms.Length < 3 )
					{
						switch( Builder.PublishMode )
						{
						case BuildState.PublishModeType.Files:
							// Publish from local source control branch
							PublishFolder = Path.Combine( Parms[0], Path.Combine( ActiveFolder + "_Loose", Builder.BranchDef.Branch ) );
							FilePattern = Parms[1];
							break;

						case BuildState.PublishModeType.Zip:
						case BuildState.PublishModeType.Iso:
						case BuildState.PublishModeType.Xsf:
							// Publish from local source control branch
							PublishFolder = Parms[0];
							SourceFolder = Parms[1];
							FilePattern = ActiveFolder + "." + Builder.PublishMode.ToString();
							break;
						}
					}
					else
					{
						// Publish from alternate location if it is specified
						SourceFolder = Path.Combine( Parms[1], Path.Combine( ActiveFolder, Builder.BranchDef.Branch ) );
						PublishFolder = Path.Combine( Parms[0], Path.Combine( ActiveFolder + "_Install", Builder.BranchDef.Branch ) );
						FilePattern = Parms[2];
					}

					string CommandLine = SourceFolder;
					CommandLine += " ";
					CommandLine += PublishFolder;
					CommandLine += " ";
					CommandLine += FilePattern;
					CommandLine += " /S /COPY:DAT";

					CurrentBuild = new BuildProcess( Parent, Builder, "RoboCopy.exe", CommandLine, "", true );

					Builder.AddPublishDestination( PublishFolder );

					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PublishFiles;
				Builder.Write( "Error: exception while starting to publish files" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES PublishRawFiles()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PublishRawFiles );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 3 )
				{
					Builder.Write( "Error: incorrect number of parameters. Usage: PublishRawFiles <Dest> <Src> <FilePatten>." );
					State = COMMANDS.PublishRawFiles;
				}
				else
				{
					string ActiveFolder = Builder.GetFolderName();

					// Construct the folder or file name based on the image mode
					string SourceFolder = Parms[1];
					if( Builder.ImageMode == "" )
					{
						SourceFolder = Path.Combine( Parms[1], ActiveFolder );
					}

					string SubPath = Parms[2].Replace( '/', '\\' );
					int PathSeparatorIndex = SubPath.LastIndexOf( '\\' );
					string FilePattern = SubPath;
					if( PathSeparatorIndex > 0 )
					{
						SourceFolder = Path.Combine( SourceFolder, SubPath.Substring( 0, PathSeparatorIndex ) );
						FilePattern = SubPath.Substring( PathSeparatorIndex + 1 );
					}

					string PublishFolder = Path.Combine( Parms[0], ActiveFolder + "_Install" );

					string CommandLine = SourceFolder;
					CommandLine += " ";
					CommandLine += PublishFolder;
					CommandLine += " ";
					CommandLine += FilePattern;
					CommandLine += " /S /COPY:DAT";

					CurrentBuild = new BuildProcess( Parent, Builder, "RoboCopy.exe", CommandLine, "", true );

					Builder.AddPublishDestination( PublishFolder );

					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PublishRawFiles;
				Builder.Write( "Error: exception while starting to publish raw files" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}


        public MODES PublishFolder()
        {
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PublishFolder );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: " + COMMANDS.PublishFolder.ToString() + " <Src> <Dest> [filespec]." );
					State = COMMANDS.PublishFolder;
				}
				else
				{
					string SourceFolder = Path.Combine( Builder.BranchDef.CurrentClient.ClientRoot, Builder.BranchDef.Branch, Parms[0] );
					string DestFolder = Path.Combine( Parms[1], Builder.GetFolderName(), Builder.BranchDef.Branch );
					
					string CommandLine = SourceFolder;
					CommandLine += " ";
					CommandLine += DestFolder;

					if( Parms.Length > 3 )
					{
						for( int Index = 3; Index < Parms.Length; Index++ )
						{
							CommandLine += " " + Parms[Index];
						}
					}
					else
					{
						CommandLine += " *.*";
					}

					CommandLine += " /V /S /MOVE /COPY:DAT";

					CurrentBuild = new BuildProcess( Parent, Builder, "RoboCopy.exe", CommandLine, "", true );

					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PublishFolder;

				Builder.Write( "Error: exception while starting to " + COMMANDS.PublishFolder.ToString() + " operation." );
				Builder.CloseLog();
			}

            return MODES.Monitor;
        }

		public MODES CookerSyncReplacement()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CookerSyncReplacement );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: " + COMMANDS.CookerSyncReplacement.ToString() + " <Src> <Dest>." );
					State = COMMANDS.CookerSyncReplacement;
				}
				else
				{
					string SourceFolder = Path.Combine( Parms[0], Builder.GetFolderName() );
					string DestFolder = Parms[1];

					string CommandLine = SourceFolder;
					CommandLine += " ";
					CommandLine += DestFolder;

					if( Parms.Length > 3 )
					{
						for( int Index = 3; Index < Parms.Length; Index++ )
						{
							CommandLine += " " + Parms[Index];
						}
					}
					else
					{
						CommandLine += " *.*";
					}

					CommandLine += " /V /S /COPY:DAT";

					CurrentBuild = new BuildProcess( Parent, Builder, "RoboCopy.exe", CommandLine, "", true );

					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CookerSyncReplacement;

				Builder.Write( "Error: exception while starting to " + COMMANDS.CookerSyncReplacement.ToString() + " operation." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

        public MODES RunBatchFile()
        {
            try
            {
                string LogFileName = Builder.GetLogFileName(COMMANDS.RunBatchFile);
                string[] Parms = Builder.SplitCommandline();

                if (Parms.Length >= 1)
                {
                    string CommandLine = "/c " + Builder.GetCurrentCommandLine() + "  > " + LogFileName + " 2>&1";
                    CurrentBuild = new BuildProcess(Parent, Builder, CommandLine, "");

                    State = CurrentBuild.GetErrorLevel();
                    StartTime = DateTime.UtcNow;
                }
                else
                {
                    State = COMMANDS.RunBatchFile;
                }

            }
            catch
            {
                State = COMMANDS.RunBatchFile;
            }

            return MODES.Monitor;
        }

		public MODES GenerateTOC()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.GenerateTOC );
				Builder.OpenLog( LogFileName, false );

				string Tagset = "CompleteBuild";
				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length == 1 )
				{
					Tagset = Parms[0];
				}

				GameConfig Config = Builder.CreateGameConfig();

				string Executable = "Binaries/CookerSync.exe";
				string CookerSyncCommandLine = Builder.LabelInfo.Game + " -p " + Config.GetCookedFolderPlatform();

				string Language = Builder.LabelInfo.Language;
				if( Language.Length == 0 )
				{
					Language = "INT";
				}

				CookerSyncCommandLine += " -r " + Language;
				CookerSyncCommandLine += " -b " + Builder.BranchDef.Branch + Builder.GetSubBranch();
				CookerSyncCommandLine += " -x " + Tagset;
				CookerSyncCommandLine += " -crc -l -nd";

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CookerSyncCommandLine, Environment.CurrentDirectory + "/Binaries", false );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.GenerateTOC;
				Builder.Write( "Error: exception while starting to make generate TOC file." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MakeISO()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.MakeISO );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 3 )
				{
					Builder.Write( "Error: incorrect number of parameters. Usage: MakeISO <Dest> <Src> <Subfolder> [<VolumeName>]." );
					State = COMMANDS.MakeISO;
				}
				else
				{
					string ActiveFolder = Builder.GetFolderName();

					// Publish from alternate location if it is specified
					string SourceFolder = Path.Combine( Parms[1], ActiveFolder );
					SourceFolder = Path.Combine( SourceFolder, Parms[2] );
					string PublishFile = Path.Combine( Parms[0], ActiveFolder + "." + Builder.ImageMode );

					string CommandLine = "-Source " + SourceFolder;
					CommandLine += " -Dest " + PublishFile;
					if( Parms.Length == 4 )
					{
						CommandLine += " -Volume " + Parms[3];
					}

					CurrentBuild = new BuildProcess( Parent, Builder, "Binaries\\MakeISO.exe", CommandLine, "", true );

					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.MakeISO;
				Builder.Write( "Error: exception while starting to make an ISO." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MakeMD5()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.MakeMD5 );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: too few parameters. Usage: MakeMD5 [Src] <FilePatten>." );
					State = COMMANDS.MakeMD5;
				}
				else
				{
					string ActiveFolder = Builder.GetFolderName();
					string SourceFolder = ".\\";
					string FilePattern = "";

					if( Parms.Length < 2 )
					{
						// Checksum from local source control branch
						FilePattern = Parms[0];
					}
					else
					{
						// Checksum from alternate location if it is specified
						SourceFolder = Path.Combine( Parms[0], Path.Combine( ActiveFolder, Builder.BranchDef.Branch ) );
						FilePattern = Parms[1];
					}

					MD5Creation Checksummer = new MD5Creation( Builder, SourceFolder );

					State = Checksummer.CalculateChecksums( FilePattern );
					if( State == COMMANDS.None )
					{
						State = Checksummer.WriteChecksumFile( "Checksums" );
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.MakeMD5;
				Builder.Write( "Error: exception while making MD5 checksum" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		private void FolderOperation( COMMANDS Command, string Options )
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( Command );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 3 )
				{
					Builder.Write( "Error: too few parameters. Usage: " + Command.ToString() + " <RootFolder> <Src> <Dest> [filespec]." );
					State = Command;
				}
				else
				{
					// If RootFolder is ".", it's a simple local copy
					string SourceFolder = Parms[1];
					string DestFolder = Parms[2];

					// If it isn't, it's an out of branch copy
					if( Parms[0] != "." )
					{
						// UT_PC_[2000-00-00_00.00]
						string ActiveFolder = Builder.GetFolderName();

						// C:\Builds\UT_PC_[2000-00-00_00.00]\UnrealEngine3
						string RootFolder = Path.Combine( Parms[0], Path.Combine( ActiveFolder, Builder.BranchDef.Branch ) );

						// C:\Builds\UT_PC_[2000-00-00_00.00]\UnrealEngine3\UTGame\CutdownPackages\UTGame
						SourceFolder = Path.Combine( RootFolder, Parms[1] );

						// C:\Builds\UT_PC_[2000-00-00_00.00]\UnrealEngine3\UTGame
                        DestFolder = Path.Combine(RootFolder, Parms[2]);
					}

					string CommandLine = SourceFolder;
					CommandLine += " ";
					CommandLine += DestFolder;
					if( Parms.Length > 3 )
					{
						for( int Index = 3; Index < Parms.Length; Index++ )
						{
							CommandLine += " " + Parms[Index];
						}
					}
					else
					{
						CommandLine += " *.*";
					}

					CommandLine += " " + Options;

					CurrentBuild = new BuildProcess( Parent, Builder, "RoboCopy.exe", CommandLine, "", true );

					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = Command;
				Builder.Write( "Error: exception while starting to " + Command.ToString() + " operation." );
				Builder.CloseLog();
			}
		}

		public MODES CopyFolder()
		{
			FolderOperation( COMMANDS.CopyFolder, "/V /S /COPY:DAT" );
			return MODES.Monitor;
		}

		public MODES MoveFolder()
		{
			FolderOperation( COMMANDS.MoveFolder, "/V /S /MOVE /COPY:DAT" );
			return MODES.Monitor;
		}

		public MODES GetPublishedData()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.GetPublishedData ), false );

				// Extract the tagset name
				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: GetPublishedData <Tagset> <Source>" );
					State = COMMANDS.GetPublishedData;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					string Executable = "Binaries/CookerSync.exe";
					string CookerSyncCommandLine = Builder.LabelInfo.Game + " -p " + Config.GetCookedFolderPlatform();

					string Language = Builder.LabelInfo.Language;
					if( Language.Length == 0 )
					{
						Language = "INT";
					}

					CookerSyncCommandLine += " -r " + Language;
					CookerSyncCommandLine += " -b " + Builder.BranchDef.Branch + Builder.GetSubBranch();
					CookerSyncCommandLine += " -x " + Parms[0];
					CookerSyncCommandLine += " -crc -l";
					CookerSyncCommandLine += " -i -notoc";

					switch( Builder.PublishVerification )
					{
					case BuildState.PublishVerificationType.MD5:
					default:
						CookerSyncCommandLine += " -v";
						break;

					case BuildState.PublishVerificationType.None:
						break;
					}

					string DestFolder = Parms[1];
					if( Builder.IncludeTimestampFolder )
					{
						DestFolder = Path.Combine( Parms[1], Builder.GetFolderName() );
					}

					CookerSyncCommandLine += " " + DestFolder;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CookerSyncCommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				Builder.Write( "Error: exception while starting to get published data" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES SteamPipe()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SteamPipe ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: too few parameters. Usage: SteamPipe <ScriptFile>" );
					State = COMMANDS.SteamPipe;
				}
				else
				{
					string PublishedFolder = Builder.GetFolderName();

					string Executable = Builder.ToolConfig.SteamContentToolLocation;
					string ScriptPath = Path.Combine( Environment.CurrentDirectory, "FortniteGame\\Build\\Steam", Parms[0] + ".vdf" );

					string CommandLine = "+login " + Builder.FTPUserName + " " + Builder.FTPPassword;
					CommandLine += " +run_app_build " + ScriptPath;
					CommandLine += " +quit";

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.SteamPipe;
				Builder.Write( "Error: exception while starting to make a steam pipe version" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES SteamMakeVersion()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SteamMakeVersion ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: too few parameters. Usage: SteamMakeVersion <ScriptFile>" );
					State = COMMANDS.SteamMakeVersion;
				}
				else
				{
					string PublishedFolder = Builder.GetFolderName();

					string Executable = Builder.ToolConfig.SteamContentToolLocation;
					string CommandLine = "/console /verbose /filename " + Parms[0] + ".smd";

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.SteamMakeVersion;
				Builder.Write( "Error: exception while starting to make a steam version" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES UpdateSteamServer()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.UpdateSteamServer ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: UpdateSteamServer <appid> <Depot>" );
					State = COMMANDS.UpdateSteamServer;
				}
				else
				{
					string CommandLine = Parms[1] + " " + Builder.CopyDestination + " " + Parms[0] + "_*.* /XO";

					CurrentBuild = new BuildProcess( Parent, Builder, "RoboCopy.exe", CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.UpdateSteamServer;
				Builder.Write( "Error: exception while trying to publish files to a local test server" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES StartSteamServer()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.StartSteamServer ), false );

				string Executable = Builder.ToolConfig.SteamContentServerLocation;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, "/start", "", false );
				State = CurrentBuild.GetErrorLevel();

				System.Threading.Thread.Sleep( 1000 );

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.StartSteamServer;
				Builder.Write( "Error: exception while starting Steam content server" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES StopSteamServer()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.StopSteamServer ), false );

				string Executable = Builder.ToolConfig.SteamContentServerLocation;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, "/stop", "", false );
				State = CurrentBuild.GetErrorLevel();

				System.Threading.Thread.Sleep( 1000 );

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.StopSteamServer;
				Builder.Write( "Error: exception while stopping Steam content server" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES UnSetup()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.UnSetup ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: too few parameters. Usage: UnSetup <Command>" );
					State = COMMANDS.UnSetup;
				}
				else
				{
					string CWD = Path.Combine( "D:\\Builds", Builder.GetFolderName() );
					CWD = Path.Combine( CWD, Builder.BranchDef.Branch );
					string Executable = Path.Combine( CWD, "Binaries/UnSetup.exe" );
					string CommandLine = "-" + Parms[0];
					if( Builder.UnSetupType.Length > 0 )
					{
						CommandLine += " -installer=" + Builder.UnSetupType;
					}

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, CWD, false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.UnSetup;
				Builder.Write( "Error: exception while starting UnSetup" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CreateDVDLayout()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CreateDVDLayout );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length > 1 )
				{
					Builder.Write( "Error: Spurious parameter(s)." );
					State = COMMANDS.CreateDVDLayout;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();

					string Executable = "Binaries/UnrealDVDLayout.exe";
					string CommandLine = Builder.LabelInfo.Game + " " + Config.GetCookedFolderPlatform();

					string LayoutFileName = Config.GetLayoutFileName( Builder.SkuName, Builder.GetLanguages().ToArray(), Builder.GetTextLanguages().ToArray() );
					CommandLine += " " + LayoutFileName;

					Queue<string> Langs = Builder.GetLanguages();
					foreach( string Lang in Langs )
					{
						CommandLine += " " + Lang.ToUpper();
					}

					Queue<string> TextLangs = Builder.GetTextLanguages();
					foreach( string TextLang in TextLangs )
					{
						CommandLine += " " + TextLang.ToUpper();
					}

					CommandLine += " -MaxFull " + Langs.Count.ToString();
					CommandLine += " -Configuration " + Config.Configuration;

					if( Parms.Length > 0 )
					{
						CommandLine += " -image " + Path.Combine( Parms[0], Builder.GetFolderName() );
						if( Builder.ImageMode.Length > 0 )
						{
							CommandLine += "." + Builder.ImageMode;
						}

						if( Builder.KeyLocation.Length > 0 && Builder.KeyPassword.Length > 0 )
						{
							CommandLine += " -keylocation " + Builder.KeyLocation;
							CommandLine += " -keypassword " + Builder.KeyPassword;
						}
					}

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", false );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CreateDVDLayout;
				Builder.Write( "Error: exception while starting to CreateDVDLayout" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES Conform()
		{
			try
			{
				if( Builder.GetValidLanguages() == null || Builder.GetValidLanguages().Count < 2 )
				{
					Parent.Log( "Error: not enough languages to conform.", Color.Red );
					State = COMMANDS.Conform;
				}
				else
				{
					string LogFileName = Builder.GetLogFileName( COMMANDS.Conform );
					Builder.OpenLog( LogFileName, true );

					string[] Parms = Builder.SplitCommandline();
					if( Parms.Length != 1 )
					{
						Builder.Write( "Error: missing package name. Usage: Conform <Package>." );
						State = COMMANDS.Conform;
					}
					else
					{
						GameConfig Config = Builder.CreateGameConfig();
						CleanIniFiles( Config );

						string CommandLine = "";
						string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );

						// Remove the INT entry
						Builder.GetValidLanguages().Dequeue();

						// Add the commandlet name and INT package
						CommandLine += "Conform " + Parms[0];

						// Add in all the loc packages
						while( Builder.GetValidLanguages().Count > 0 )
						{
							CommandLine += " " + Parms[0] + "_" + Builder.GetValidLanguages().Dequeue();
						}

						CommandLine += " " + CommonCommandLine + Builder.GetCookConfiguration();

						CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
						State = CurrentBuild.GetErrorLevel();
					}

					StartTime = DateTime.UtcNow;
				}
			}
			catch
			{
				State = COMMANDS.Conform;
				Builder.Write( "Error: exception while starting to conform dialog" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES PatchScript()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PatchScript );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 1 )
				{
					Builder.Write( "Error: missing package name. Usage: PatchScript <Package>." );
					State = COMMANDS.PatchScript;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();
					CleanIniFiles( Config );

					string CommandLine = "";
					string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
					CommandLine += "patchscript " + Parms[0] + " -Platform=" + Config.GetCookedFolderPlatform() + " " + Builder.GetScriptConfiguration() + " ..\\..\\" + Config.GetCookedFolderName( Builder.BranchDef.Version ) + " ..\\..\\" + Config.GetOriginalCookedFolderName( Builder.BranchDef.Version ) + " " + CommonCommandLine;

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PatchScript;
				Builder.Write( "Error: exception while starting to patch script" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		private MODES CheckpointGameAssetDatabase( string AdditionalOptions )
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CheckpointGameAssetDatabase );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				if( AdditionalOptions.Length > 0 )
				{
					string EncodedFoldername = Builder.GetFolderName();
					Executable = Config.GetUDKComName( Executable, Builder.BranchDef.Branch, EncodedFoldername, Builder.GetCurrentCommandLine() );
				}
				CommandLine += "CheckpointGameAssetDatabase " + AdditionalOptions + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CheckpointGameAssetDatabase;
				Builder.Write( "Error: exception while starting to checkpoint the game asset database." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CheckpointGameAssetDatabase()
		{
			return CheckpointGameAssetDatabase( "" );
		}

		public MODES UpdateGameAssetDatabase()
		{
			return CheckpointGameAssetDatabase( "-NoDeletes -Repair -PurgeGhosts -DeletePrivateCollections -DeleteNonUDKCollections " );
		}

		private MODES TagReferencedAssets( bool bUseCookedData )
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.TagReferencedAssets );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "TagReferencedAssets ";
				if( bUseCookedData )
				{
					CommandLine = "TagCookedReferencedAssets -Platform=" + Config.GetCookedFolderPlatform() + " ";
				}
				CommandLine += Builder.GetDLCName() + " ";
				CommandLine += CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.TagReferencedAssets;
				Builder.Write( "Error: exception while starting to tag referenced assets." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES TagReferencedAssets()
		{
			return TagReferencedAssets( false );
		}

		public MODES TagDVDAssets()
		{
			return TagReferencedAssets( true );
		}

		public MODES AuditContent()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.AuditContent );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "ContentAuditCommandlet " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.AuditContent;
				Builder.Write( "Error: exception while starting to audit content." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES FindStaticMeshCanBecomeDynamic()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.FindStaticMeshCanBecomeDynamic );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "FindStaticMeshCanBecomeDynamic " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.FindStaticMeshCanBecomeDynamic;
				Builder.Write( "Error: exception while starting to find static meshes that can become dynamic." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES FixupRedirects()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.FixupRedirects );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "FixupRedirects " + CommonCommandLine + Builder.GetOptionalCommandletConfig();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.FixupRedirects;
				Builder.Write( "Error: exception while starting to Fix Up Redirects." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES ResaveDeprecatedPackages()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ResaveDeprecatedPackages );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "ResavePackages -ResaveDeprecated -MaxPackagesToResave=50 -AutoCheckoutPackages " + CommonCommandLine + Builder.GetOptionalCommandletConfig();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.ResaveDeprecatedPackages;
				Builder.Write( "Error: exception while starting to resave deprecated packages." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES AnalyzeReferencedContent()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.AnalyzeReferencedContent );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "AnalyzeReferencedContent " + CommonCommandLine + Builder.GetAnalyzeReferencedContentConfiguration();

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.AnalyzeReferencedContent;
				Builder.Write( "Error: exception while starting to Analyze Referenced Content." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES MineCookedPackages()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.MineCookedPackages );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "MineCookedPackages " + CommonCommandLine + Builder.GetOptionalCommandletConfig() + Builder.GetDataBaseConnectionInfo() + " ../../" + Config.GetCookedFolderName( Builder.BranchDef.Version ) + "/*.xxx";

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.MineCookedPackages;
				Builder.Write( "Error: exception while starting to Mine Cooked Packages." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES ContentComparison()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ContentComparison );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "ContentComparison " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.ContentComparison;
				Builder.Write( "Error: exception while starting to Compare Content." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES DumpMapSummary()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.DumpMapSummary );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: missing map name. Usage: DumpMapSummary <map>." );
					State = COMMANDS.DumpMapSummary;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();
					CleanIniFiles( Config );

					string CommandLine = "";
					string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
					CommandLine += "DumpMapSummary ";

					foreach( string Parm in Parms )
					{
						CommandLine += Parm + " ";
					}

					CommandLine += CommonCommandLine;
					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();

					StartTime = DateTime.UtcNow;
				}
			}
			catch
			{
				State = COMMANDS.DumpMapSummary;
				Builder.Write( "Error: exception while starting to dump map summary." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES ExtractSHAs()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.ExtractSHAs );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: missing section name. Usage: ValidateSHA <section>." );
					State = COMMANDS.ExtractSHAs;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();
					List<string> Binaries = Config.GetExecutableNames( 1 );

					string Executable = "imagexex.exe";
					string CommandLine = "/dump /sectionname:" + Parms[0] + " /sectionfile:" + Parms[0] + ".bin " + Binaries[0];

					CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();
				}
			}
			catch
			{
				State = COMMANDS.ExtractSHAs;
				Builder.Write( "Error: exception while validating SHA section." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CheckSHAs()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CheckSHAs );
				Builder.OpenLog( LogFileName, false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length < 1 )
				{
					Builder.Write( "Error: missing section name. Usage: ValidateSHA <section>." );
					State = COMMANDS.CheckSHAs;
				}
				else
				{
					FileInfo Info = new FileInfo( Parms[0] + ".bin" );
					if( !Info.Exists )
					{
						Builder.Write( "Error: Extracted SHA file does not exist!" );
						State = COMMANDS.CheckSHAs;
					}
					else if( Info.Length < 2 )
					{
						Builder.Write( "Error: Extracted SHA file is too small! (" + Info.Length.ToString() + " bytes)" );
						State = COMMANDS.CheckSHAs;
					}
					else
					{
						Builder.Write( "[REPORT] SHA section is " + Info.Length.ToString() + " bytes for " + Builder.BuildConfig + " config." );
						Info.Delete();
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.CheckSHAs;
				Builder.Write( "Error: exception while checking SHA section." );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		private void BumpEngineCpp( BuildState Builder, List<string> Lines )
		{
			// Bump ENGINE_VERSION and BUILT_FROM_CHANGELIST
			int BumpIncrement = 1;
			if( Builder.GetCurrentCommandLine().Length > 0 )
			{
				BumpIncrement = Builder.SafeStringToInt( Builder.GetCurrentCommandLine() );
			}

			for( int i = 0; i < Lines.Count; i++ )
			{
				string[] Parms = Builder.SafeSplit( Lines[i], false );

				// Looks for the template of 'define', 'constant', 'value'
				if( Parms.Length == 3 && Parms[0].ToUpper() == "#DEFINE" )
				{
					if( Parms[1].ToUpper() == "MAJOR_VERSION" )
					{
						Builder.LabelInfo.BuildVersion = new Version( Builder.LabelInfo.SafeStringToInt( Parms[2] ),
																		Builder.LabelInfo.BuildVersion.Minor,
																		Builder.LabelInfo.BuildVersion.Build,
																		Builder.LabelInfo.BuildVersion.Revision );
					}

					if( Parms[1].ToUpper() == "MINOR_VERSION" )
					{
						Builder.LabelInfo.BuildVersion = new Version( Builder.LabelInfo.BuildVersion.Major,
																		Builder.LabelInfo.SafeStringToInt( Parms[2] ),
																		Builder.LabelInfo.BuildVersion.Build,
																		Builder.LabelInfo.BuildVersion.Revision );
					}

					if( Parms[1].ToUpper() == "ENGINE_VERSION" )
					{
						if( Builder.BranchDef.Version < 10 )
						{
							// UE3 branches use an auto-incremented engine version value
							Builder.LabelInfo.BuildVersion = new Version( Builder.LabelInfo.BuildVersion.Major,
																			Builder.LabelInfo.BuildVersion.Minor,
																			Builder.LabelInfo.SafeStringToInt( Parms[ 2 ] ) + BumpIncrement,
																			Builder.LabelInfo.BuildVersion.Revision );
						}
						else
						{
							// UE4 branches use the Perforce changelist number as the ENGINE_VERSION
							Builder.LabelInfo.BuildVersion = new Version( Builder.LabelInfo.BuildVersion.Major,
																			Builder.LabelInfo.BuildVersion.Minor,
																			Builder.LabelInfo.Changelist,
																			Builder.LabelInfo.BuildVersion.Revision );
						}
						Lines[i] = "#define\tENGINE_VERSION\t" + Builder.LabelInfo.BuildVersion.Build.ToString();
					}

					if( Parms[1].ToUpper() == "PRIVATE_VERSION" )
					{
						Builder.LabelInfo.BuildVersion = new Version( Builder.LabelInfo.BuildVersion.Major,
																		Builder.LabelInfo.BuildVersion.Minor,
																		Builder.LabelInfo.BuildVersion.Build,
																		Builder.LabelInfo.SafeStringToInt( Parms[2] ) );
					}

					// UE4 branches don't use BUILT_FROM_CHANGELIST.  Instead, ENGINE_VERSION *is* the changelist number.
					if( Builder.BranchDef.Version < 10 )
					{
						if( Parms[1].ToUpper() == "BUILT_FROM_CHANGELIST" )
						{
							Lines[i] = "#define\tBUILT_FROM_CHANGELIST\t" + Builder.LabelInfo.Changelist.ToString();
						}
					}
				}
			}
		}

		private string GetHexVersion( int EngineVersion )
		{
			string HexVersion = "";
			char[] HexDigits = "0123456789abcdef".ToCharArray();

			int MajorVer = Builder.LabelInfo.BuildVersion.Major;
			int MinorVer = Builder.LabelInfo.BuildVersion.Minor;

			// First 4 bits is major version
			HexVersion += HexDigits[MajorVer & 0xf];
			// Next 4 bits is minor version
			HexVersion += HexDigits[MinorVer & 0xf];
			// Next 16 bits is build number
			HexVersion += HexDigits[( EngineVersion >> 12 ) & 0xf];
			HexVersion += HexDigits[( EngineVersion >> 8 ) & 0xf];
			HexVersion += HexDigits[( EngineVersion >> 4 ) & 0xf];
			HexVersion += HexDigits[( EngineVersion >> 0 ) & 0xf];
			// Client code is required to have the first 4 bits be 0x8, where server code is required to have the first 4 bits be 0xC.
			HexVersion += HexDigits[0x8];
			// DiscID varies for different languages
			HexVersion += HexDigits[0x1];

			return ( HexVersion );
		}

		public MODES BumpAgentVersion()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.BumpAgentVersion ), false );

				// We have two files to update the version in
				string AgentFile = "Binaries/SwarmAgent.exe";
				string ProjectFile = "Development/Tools/UnrealSwarm/Agent/Agent.csproj";
				string AssemblyInfoFile = "Development/Tools/UnrealSwarm/Agent/Properties/AssemblyInfo.cs";
				string SwarmDefinesFile = "Development/Src/UnrealSwarm/Inc/SwarmDefines.h";
				string AgentInterfaceFile = "Development/Tools/UnrealSwarm/AgentInterface/AgentInterface.h";

				// Make sure they all exist
				if( !File.Exists( AgentFile ) )
				{
					State = COMMANDS.BumpAgentVersion;
					Builder.Write( "Error: File does not exist: '" + AgentFile + "'" );
					return MODES.Finalise;
				}
				if( !File.Exists( ProjectFile ) )
				{
					State = COMMANDS.BumpAgentVersion;
					Builder.Write( "Error: File does not exist: '" + ProjectFile + "'" );
					return MODES.Finalise;
				}
				if( !File.Exists( AssemblyInfoFile ) )
				{
					State = COMMANDS.BumpAgentVersion;
					Builder.Write( "Error: File does not exist: '" + AssemblyInfoFile + "'" );
					return MODES.Finalise;
				}
				if( !File.Exists( SwarmDefinesFile ) )
				{
					State = COMMANDS.BumpAgentVersion;
					Builder.Write( "Error: File does not exist: '" + SwarmDefinesFile + "'" );
					return MODES.Finalise;
				}
				if( !File.Exists( AgentInterfaceFile ) )
				{
					State = COMMANDS.BumpAgentVersion;
					Builder.Write( "Error: File does not exist: '" + AgentInterfaceFile + "'" );
					return MODES.Finalise;
				}

				// 				// Use the SCC revision numbers to create the minor version number
				// 				int SwarmDefinesFileVersion = SCC.GetHaveRevision( Builder, SwarmDefinesFile );
				// 				if( SwarmDefinesFileVersion == 0 )
				// 				{
				// 					State = COMMANDS.BumpAgentVersion;
				// 					Builder.Write( "Error: revision of file is 0: '" + SwarmDefinesFile + "'" );
				// 					return;
				// 				}
				// 
				// 				int AgentInterfaceFileVersion = SCC.GetHaveRevision( Builder, AgentInterfaceFile );
				// 				if( AgentInterfaceFileVersion == 0 )
				// 				{
				// 					State = COMMANDS.BumpAgentVersion;
				// 					Builder.Write( "Error: revision of file is 0: '" + AgentInterfaceFile + "'" );
				// 					return;
				// 				}

				// Get the current version of the agent and increment it
				Builder.Write( "Getting version info for " + AgentFile );
				FileVersionInfo AgentFileVersionInfo = FileVersionInfo.GetVersionInfo( Path.Combine( Environment.CurrentDirectory, AgentFile ) );
				Version OldAgentVersion = new Version( AgentFileVersionInfo.FileVersion );

				// Keep the old major version number
				int NewVersionMajor = OldAgentVersion.Major;

				// 				// The minor version is a simple sum of have revisions of the files that matter
				// 				int NewVersionMinor = SwarmDefinesFileVersion + AgentInterfaceFileVersion;
				// 
				// 				// Determine the build and revision values
				// 				int NewVersionBuild;
				// 				int NewVersionRevision;
				// 
				// 				if( NewVersionMinor == OldAgentVersion.Minor )
				// 				{
				// 					// Only increment the build version
				// 					NewVersionBuild = OldAgentVersion.Build + 1;
				// 					// And clear out the revision
				// 					NewVersionRevision = 0;
				// 				}
				// 				else
				// 				{
				// 					// If the minor version changed, reset the build and revision
				// 					NewVersionBuild = 0;
				// 					NewVersionRevision = 0;
				// 				}

				// The minor version is a simple sum of have revisions of the files that matter
				int NewVersionMinor = OldAgentVersion.Minor;

				// Only increment the build version
				int NewVersionBuild = OldAgentVersion.Build + 1;

				// And clear out the revision
				int NewVersionRevision = 0;

				Version NewAgentVersion = new Version( NewVersionMajor, NewVersionMinor, NewVersionBuild, NewVersionRevision );
				Builder.Write( "New version is " + NewAgentVersion.ToString() );

				StreamReader Reader;
				TextWriter Writer;
				List<string> Lines;
				string Line;

				// Bump the project version
				{
					Lines = new List<string>();
					Reader = new StreamReader( ProjectFile );
					if( Reader == null )
					{
						State = COMMANDS.BumpAgentVersion;
						Builder.Write( "Error: failed to open for reading '" + ProjectFile + "'" );
						return MODES.Finalise;
					}

					Line = Reader.ReadLine();
					while( Line != null )
					{
						Lines.Add( Line );
						Line = Reader.ReadLine();
					}
					Reader.Close();

					// Look for the line to edit
					for( int i = 0; i < Lines.Count; i++ )
					{
						// Looking for something like "<ApplicationVersion>1.0.0.0</ApplicationVersion>"
						if( Lines[i].Trim().StartsWith( "<ApplicationVersion>" ) )
						{
							Lines[i] = "    <ApplicationVersion>" + NewAgentVersion.ToString() + "</ApplicationVersion>";
						}
					}

					Writer = TextWriter.Synchronized( new StreamWriter( ProjectFile, false, Encoding.ASCII ) );
					if( Writer == null )
					{
						State = COMMANDS.BumpAgentVersion;
						Builder.Write( "Error: failed to open for writing '" + ProjectFile + "'" );
						return MODES.Finalise;
					}
					foreach( string SingleLine in Lines )
					{
						Writer.Write( SingleLine + Environment.NewLine );
					}
					Writer.Close();
				}

				// Bump the assembly info version
				{
					Lines = new List<string>();
					Reader = new StreamReader( AssemblyInfoFile );
					if( Reader == null )
					{
						State = COMMANDS.BumpAgentVersion;
						Builder.Write( "Error: failed to open for reading '" + AssemblyInfoFile + "'" );
						return MODES.Finalise;
					}

					Line = Reader.ReadLine();
					while( Line != null )
					{
						Lines.Add( Line );
						Line = Reader.ReadLine();
					}
					Reader.Close();

					// Look for the line to edit
					for( int i = 0; i < Lines.Count; i++ )
					{
						// Looking for something like "[assembly: AssemblyVersion( "1.0.0.171" )]"
						if( Lines[i].Trim().StartsWith( "[assembly: AssemblyVersion(" ) )
						{
							Lines[i] = "[assembly: AssemblyVersion( \"" + NewAgentVersion.ToString() + "\" )]";
						}
					}

					Writer = TextWriter.Synchronized( new StreamWriter( AssemblyInfoFile, false, Encoding.ASCII ) );
					if( Writer == null )
					{
						State = COMMANDS.BumpAgentVersion;
						Builder.Write( "Error: failed to open for writing '" + AssemblyInfoFile + "'" );
						return MODES.Finalise;
					}
					foreach( string SingleLine in Lines )
					{
						Writer.Write( SingleLine + Environment.NewLine );
					}
					Writer.Close();
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.BumpAgentVersion;
				Builder.Write( "Error: while bumping agent version" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		private void BumpEngineXml( BuildState Builder, List<string> Lines )
		{
			// Bump build version in Live! stuff
			for( int i = 0; i < Lines.Count; i++ )
			{
				if( Lines[i].Trim().StartsWith( "build=" ) )
				{
					Lines[i] = "     build=\"" + Builder.LabelInfo.BuildVersion.Build.ToString() + "\"";
				}
				else if( Lines[i].Trim().StartsWith( "<titleversion>" ) )
				{
					Lines[i] = "  <titleversion>" + GetHexVersion( Builder.LabelInfo.BuildVersion.Build ) + "</titleversion>";
				}
				else if( Lines[i].Trim().StartsWith( "<VersionNumber versionNumber=" ) )
				{
					Lines[i] = "      <VersionNumber versionNumber=\"" + Builder.LabelInfo.BuildVersion.ToString() + "\" />";
				}
			}
		}

		private void BumpEngineHeader( BuildState Builder, List<string> Lines )
		{
			// Bump the defines in the header files
			for( int i = 0; i < Lines.Count; i++ )
			{
				if( Lines[i].Trim().StartsWith( "#define ENGINE_VERSION" ) )
				{
					Lines[i] = "#define ENGINE_VERSION " + Builder.LabelInfo.BuildVersion.Build.ToString();
				}

				// NOTE: BUILT_FROM_CHANGELIST is only used in branches earlier than version 10.  It later versions,
				//       the ENGINE_VERSION is the changelist.
				else if( Lines[i].Trim().StartsWith( "#define BUILT_FROM_CHANGELIST" ) )
				{
					Lines[i] = "#define BUILT_FROM_CHANGELIST " + Builder.LabelInfo.Changelist.ToString();
				}
			}
		}

		private void BumpEngineResource( BuildState Builder, List<string> Lines )
		{
			// Bump the versions in the rc file
			for( int i = 0; i < Lines.Count; i++ )
			{
				// FILEVERSION 1,0,0,0
				if( Lines[i].Trim().StartsWith( "FILEVERSION" ) )
				{
					Lines[i] = " FILEVERSION 1,0," + Builder.LabelInfo.BuildVersion.Build.ToString() + ",0";
				}
				// PRODUCTVERSION 1,0,0,0
				else if( Lines[i].Trim().StartsWith( "PRODUCTVERSION" ) )
				{
					Lines[i] = " PRODUCTVERSION 1,0," + Builder.LabelInfo.BuildVersion.Build.ToString() + ",0";
				}
				// VALUE "FileVersion", "1, 0, 0, 0"
				else if( Lines[i].Trim().StartsWith( "VALUE \"FileVersion\"" ) )
				{
					Lines[i] = "\t\t\tVALUE \"FileVersion\", \"1, 0, " + Builder.LabelInfo.BuildVersion.Build.ToString() + ", 0\"";
				}
				// VALUE "ProductVersion", "1, 0, 0, 0"
				else if( Lines[i].Trim().StartsWith( "VALUE \"ProductVersion\"" ) )
				{
					Lines[i] = "\t\t\tVALUE \"ProductVersion\", \"1, 0, " + Builder.LabelInfo.BuildVersion.Build.ToString() + ", 0\"";
				}
			}
		}

		private void BumpEngineProperties( BuildState Builder, List<string> Lines, string TimeStamp, int ChangeList, int EngineVersion )
		{
			// Bump build version in properties file
			for( int i = 0; i < Lines.Count; i++ )
			{
				if( Lines[i].Trim().StartsWith( "timestampForBVT=" ) )
				{
					Lines[i] = "timestampForBVT=" + TimeStamp;
				}
				else if( Lines[i].Trim().StartsWith( "changelistBuiltFrom=" ) )
				{
					Lines[i] = "changelistBuiltFrom=" + ChangeList.ToString();
				}
				else if( Lines[i].Trim().StartsWith( "engineVersion=" ) )
				{
					Lines[i] = "engineVersion=" + EngineVersion.ToString();
				}
			}
		}

		private void BumpEngineRDF( BuildState Builder, List<string> Lines )
		{
			string Spaces = "                                          ";

			// Bump build version in rdf file
			for( int i = 0; i < Lines.Count; i++ )
			{
				// [spaces]em:version="1.0.0.1"
				if( Lines[i].Trim().StartsWith( "em:version=" ) )
				{
					int NumSpaces = Lines[i].IndexOf( "em:version=" );
					Lines[i] = "";
					if( NumSpaces > -1 )
					{
						Lines[i] = Spaces.Substring( Spaces.Length - NumSpaces );
					}

					Lines[i] += "em:version=\"1.0." + Builder.LabelInfo.BuildVersion.Build.ToString() + ".0\"";
				}
			}
		}

		private void BumpEngineINF( BuildState Builder, List<string> Lines )
		{
			// Bump build version in inf file
			for( int i = 0; i < Lines.Count; i++ )
			{
				// FileVersion=1,0,6713,0
				if( Lines[i].Trim().StartsWith( "FileVersion=" ) )
				{
					Lines[i] = "FileVersion=1,0," + Builder.LabelInfo.BuildVersion.Build.ToString() + ",0";
				}
			}
		}

		private void BumpEngineJSON( BuildState Builder, List<string> Lines )
		{
			// Bump build version in json file
			for( int i = 0; i < Lines.Count; i++ )
			{
				// "version": "1.0.6777.0",
				if( Lines[i].Trim().StartsWith( "\"version\":" ) )
				{
					Lines[i] = "  \"version\": \"1.0." + Builder.LabelInfo.BuildVersion.Build.ToString() + ".0\",";
				}
			}
		}

		private void BumpEnginePList( BuildState Builder, List<string> Lines )
		{
			// Bump build version in a plist file
			for( int i = 0; i < Lines.Count; i++ )
			{
				// <key>CFBundleVersion</key>
				if( Lines[i].Trim().StartsWith( "<key>CFBundleVersion</key>" ) && Lines.Count > i + 1 )
				{
					Lines[i + 1] = "\t<string>" + Builder.LabelInfo.BuildVersion.Build.ToString() + ".0</string>";
					break;
				}
			}
		}

		private List<string> ReadTextFileToArray( FileInfo Info )
		{
			List<string> Lines = new List<string>();

			// Check to see if the version file is writable (otherwise the StreamWriter creation will exception)
			if( Info.IsReadOnly )
			{
				State = COMMANDS.BumpEngineVersion;
				Builder.Write( "Error: version file is read only '" + Info.FullName + "'" );
				return ( Lines );
			}

			// Read in existing file
			Lines = new List<string>( File.ReadAllLines( Info.FullName ) );

			return Lines;
		}

		private void SaveLinesToTextFile( string FileName, List<string> Lines, bool bUseASCII )
		{
			TextWriter Writer;

			if( bUseASCII )
			{
				Writer = TextWriter.Synchronized( new StreamWriter( FileName, false, Encoding.ASCII ) );
			}
			else
			{
				Writer = TextWriter.Synchronized( new StreamWriter( FileName, false, Encoding.Unicode ) );
			}

			if( Writer == null )
			{
				State = COMMANDS.BumpEngineVersion;
				Builder.Write( "Error: failed to open for writing '" + FileName + "'" );
				return;
			}

			foreach( string SingleLine in Lines )
			{
				Writer.Write( SingleLine + Environment.NewLine );
			}

			Writer.Close();
		}

		private void BumpVersionFile( BuildState Builder, string File, bool GetVersion )
		{
			bool bUseASCII = true;
			FileInfo Info = new FileInfo( File );
			List<string> Lines = ReadTextFileToArray( Info );

			Builder.Write( "Bumping version for '" + Info.FullName + "'" );

			// Bump the version dependent on the file extension
			if( GetVersion && Info.Extension.ToLower() == ".cpp" )
			{
				BumpEngineCpp( Builder, Lines );
			}
			else
			{
				if( Info.Extension.ToLower() == ".xml" )
				{
					BumpEngineXml( Builder, Lines );
					bUseASCII = ( File.ToLower().IndexOf( ".gdf.xml" ) < 0 );
				}
				else if( Info.Extension.ToLower() == ".properties" )
				{
					BumpEngineProperties( Builder, Lines, Builder.GetTimeStamp(), Builder.LabelInfo.Changelist, Builder.LabelInfo.BuildVersion.Build );
				}
				else if( Info.Extension.ToLower() == ".h" )
				{
					BumpEngineHeader( Builder, Lines );
				}
				else if( Info.Extension.ToLower() == ".rc" )
				{
					BumpEngineResource( Builder, Lines );
				}
				else if( Info.Extension.ToLower() == ".rdf" )
				{
					BumpEngineRDF( Builder, Lines );
					bUseASCII = false;
				}
				else if( Info.Extension.ToLower() == ".inf" )
				{
					BumpEngineINF( Builder, Lines );
				}
				else if( Info.Extension.ToLower() == ".json" )
				{
					BumpEngineJSON( Builder, Lines );
				}
				else if( Info.Extension.ToLower() == ".plist" )
				{
					BumpEnginePList( Builder, Lines );
				}
				else
				{
					State = COMMANDS.BumpEngineVersion;
					Builder.Write( "Error: invalid extension for '" + File + "'" );
					return;
				}
			}

			// Write out version
			SaveLinesToTextFile( File, Lines, bUseASCII );

			Builder.Write( " ... bumped!" );
		}

		public MODES BumpEngineVersion()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.BumpEngineVersion ), false );

				string File = Builder.VersionConfig.EngineVersionFile;
				BumpVersionFile( Builder, File, true );

				List<string> VersionFiles = new List<string>( Builder.VersionConfig.MiscVersionFiles );
				VersionFiles.AddRange( Builder.VersionConfig.ConsoleVersionFiles );
				VersionFiles.AddRange( Builder.VersionConfig.MobileVersionFiles );
				foreach( string XmlFile in VersionFiles )
				{
					if( XmlFile.Trim().Length > 0 )
					{
						BumpVersionFile( Builder, XmlFile.Trim(), false );
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.BumpEngineVersion;
				Builder.Write( "Error: while bumping engine version in '" + Builder.VersionConfig.EngineVersionFile + "'" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		private int GetVersionFile( BuildState Builder, string File )
		{
			if( Builder.BranchDef.Version >= 10 )
			{
				throw new Exception( "GetVersionFile() is not supported in UE4.  The engine version is the changelist number." );
			}

			List<string> Lines = new List<string>();
			string Line;
			int NewEngineVersion = 0;

			// Check to see if the version file is writable (otherwise the StreamWriter creation will exception)
			FileInfo Info = new FileInfo( File );

			// Read in existing file
			StreamReader Reader = new StreamReader( File );
			if( Reader == null )
			{
				State = COMMANDS.GetEngineVersion;
				Builder.Write( "Error: failed to open for reading '" + File + "'" );
				return ( 0 );
			}

			Line = Reader.ReadLine();
			while( Line != null )
			{
				Lines.Add( Line );

				Line = Reader.ReadLine();
			}

			Reader.Close();

			for( int i = 0; i < Lines.Count; i++ )
			{
				string[] Parms = Builder.SafeSplit( Lines[i], false );

				if( Parms.Length == 3 && Parms[0].ToUpper() == "#DEFINE" )
				{
					if( Parms[1].ToUpper() == "ENGINE_VERSION" )
					{
						NewEngineVersion = Builder.SafeStringToInt( Parms[2] );
					}
				}
			}

			return ( NewEngineVersion );
		}

		public MODES GetEngineVersion()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.GetEngineVersion ), false );

				int EngineVersion;
				if( Builder.BranchDef.Version < 10 )
				{
					EngineVersion = GetVersionFile( Builder, Builder.VersionConfig.EngineVersionFile );
				}
				else
				{
					// UE4 uses the changelist as the engine version
					EngineVersion = Builder.LabelInfo.Changelist;
				}
				Builder.LabelInfo.BuildVersion = new Version( Builder.LabelInfo.BuildVersion.Major,
																Builder.LabelInfo.BuildVersion.Minor,
																EngineVersion,
																Builder.LabelInfo.BuildVersion.Revision );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.BumpEngineVersion;
				Builder.Write( "Error: while getting engine version in '" + Builder.VersionConfig.EngineVersionFile + "'" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES UpdateGDFVersion()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.UpdateGDFVersion ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: UpdateGDFVersion <Game> <ResourcePath>." );
					State = COMMANDS.UpdateGDFVersion;
				}
				else
				{
					int EngineVersion = Builder.LabelInfo.BuildVersion.Build;
					Queue<string> Languages = Builder.GetLanguages();

					foreach( string Lang in Languages )
					{
						string GDFFileName = Parms[1] + "/" + Lang.ToUpper() + "/" + Parms[0] + "Game.gdf.xml";
						BumpVersionFile( Builder, GDFFileName, false );
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.UpdateGDFVersion;
				Builder.Write( "Error: while bumping engine version in '" + Builder.VersionConfig.EngineVersionFile + "'" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES MakeGFWLCat()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.MakeGFWLCat ), false );

				GameConfig Config = Builder.CreateGameConfig();
				List<string> CatNames = Config.GetCatNames( Builder.BranchDef.Version );

				string Folder = Path.GetDirectoryName( CatNames[1] );
				string CdfFile = Path.GetFileName( CatNames[1] );

				Parent.Log( "[STATUS] Making cat for '" + CdfFile + "' in folder '" + Folder + "'", Color.Magenta );

				string MakeCatName = Builder.ToolConfig.CatToolName;
				string CommandLine = "-v " + CdfFile;
				string WorkingFolder = Folder;

				CurrentBuild = new BuildProcess( Parent, Builder, MakeCatName, CommandLine, WorkingFolder, false );
				State = CurrentBuild.GetErrorLevel();
				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.MakeGFWLCat;
				Builder.Write( "Error: while making cat file" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES ZDPP()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.ZDPP ), false );

				// ZDP tool can't operate if the redist portion has already been copied - so delete it
				string ZDPFolder = "Binaries\\Win32\\Zdp";
				Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ZDPFolder + "'" );
				if( Directory.Exists( ZDPFolder ) )
				{
					Parent.DeleteDirectory( ZDPFolder, 0 );
					Builder.Write( " ... done" );
				}

				string ZDPOutputFolder = "Binaries\\Win32\\ZdpSdkOutput";
				Builder.Write( "Deleting: '" + Builder.BranchDef.Branch + "/" + ZDPOutputFolder + "'" );
				if( Directory.Exists( ZDPOutputFolder ) )
				{
					Parent.DeleteDirectory( ZDPOutputFolder, 0 );
					Builder.Write( " ... done" );
				}

				// Running the ZDP tool copies the redist files back in
				GameConfig Config = Builder.CreateGameConfig();
				List<string> ExeNames = Config.GetExecutableNames( Builder.BranchDef.Version );

				string Exectable = Builder.ToolConfig.ZDPPToolName;
				string WorkingFolder = "Binaries\\Win32";
				string CommandLine = "-s ..\\..\\StormGame\\Build\\ZDPP\\ZdpSettings.xml -x " + Path.GetFileName( ExeNames[0] );

				CurrentBuild = new BuildProcess( Parent, Builder, Exectable, CommandLine, WorkingFolder, false );
				State = CurrentBuild.GetErrorLevel();
				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.ZDPP;
				Builder.Write( "Error: while applying ZDPP" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES SteamDRM()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SteamDRM ), false );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: too few parameters. Usage: SteamDRM <AppId> <flags>." );
					State = COMMANDS.SteamDRM;
				}
				else
				{
					GameConfig Config = Builder.CreateGameConfig();
					List<string> ExeNames = Config.GetExecutableNames( Builder.BranchDef.Version );
					string Exectable = "Binaries\\Win32\\drmtool.exe";
					string WorkingFolder = "Binaries\\Win32";

					string CommandLine = "-drm " + Path.GetFileName( ExeNames[0] ) + " " + Parms[0] + " " + Parms[1];

					CurrentBuild = new BuildProcess( Parent, Builder, Exectable, CommandLine, WorkingFolder, false );
					State = CurrentBuild.GetErrorLevel();
					StartTime = DateTime.UtcNow;
				}
			}
			catch
			{
				State = COMMANDS.SteamDRM;
				Builder.Write( "Error: while applying Steam DRM" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES FixupSteamDRM()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.FixupSteamDRM ), false );

				GameConfig Config = Builder.CreateGameConfig();
				List<string> ExeNames = Config.GetExecutableNames( Builder.BranchDef.Version );

				Builder.Write( "Fixing up: " + ExeNames[0] );
				string ExeFolder = Path.GetDirectoryName( ExeNames[0] );
				string ExeName = Path.GetFileNameWithoutExtension( ExeNames[0] );

				DirectoryInfo FolderInfo = new DirectoryInfo( ExeFolder );
				Builder.Write( " ... finding: " + ExeName + "_*.exe in " + FolderInfo.FullName );
				FileInfo[] Executables = FolderInfo.GetFiles( ExeName + "_*.exe" );
				Builder.Write( " ... found: " + Executables.Length.ToString() + " files" );

				if( Executables.Length == 1 )
				{
					FileInfo ExeInfo = new FileInfo( ExeNames[0] );
					ExeInfo.Delete();

					Executables[0].MoveTo( ExeNames[0] );
				}
				else
				{
					State = COMMANDS.FixupSteamDRM;
					Builder.Write( "Error: while fixing up Steam DRMed filenames (no or multiple executables found)" );
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.FixupSteamDRM;
				Builder.Write( "Error: while fixing up Steam DRMed filenames" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SaveDefines()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SaveDefines ), false );

				// Read in the properties file
				Builder.Write( "Reading: Binaries/build.properties" );
				FileInfo Info = new FileInfo( "Binaries/build.properties" );
				List<string> Lines = ReadTextFileToArray( Info );

				// Remove any stale defines
				if( Lines.Count > 3 )
				{
					Lines.RemoveRange( 3, Lines.Count - 3 );
				}

				// Set the new defines
				foreach( string Define in Builder.LabelInfo.Defines )
				{
					Builder.Write( "Adding: " + Define );
					Lines.Add( Define );
				}

				// Write out the updated file
				Builder.Write( "Writing: Binaries/build.properties" );
				SaveLinesToTextFile( "Binaries/build.properties", Lines, true );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SaveDefines;
				Builder.Write( "Error: while saving defines to build.properties" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		// Force the source server ini file to do the right thing
		private void UpdateSourceServerIni()
		{
			string IniFileName = Path.Combine( Path.GetDirectoryName( Builder.ToolConfig.SourceServerCmd ), "srcsrv.ini" );
			FileInfo IniFile = new FileInfo( IniFileName );
			if( IniFile.Exists )
			{
				IniFile.IsReadOnly = false;
				IniFile.Delete();
			}

			StreamWriter Writer = IniFile.AppendText();

			Writer.WriteLine("[variables]");
			Writer.WriteLine("MYSERVER=" + Properties.Settings.Default.SourceServerName);

			Writer.WriteLine("[trusted commands]");
			Writer.WriteLine("p4.exe");

			Writer.WriteLine("[server errors]");
			Writer.WriteLine("perforce=var2,Connect to server failed;");

			Writer.Close();
		}

		private string CreateBatFileHack()
		{
			string BatFileName = Path.Combine( Path.GetDirectoryName( Builder.ToolConfig.SourceServerCmd ), "uesrcsrv.bat" );
			FileInfo BatFileInfo = new FileInfo( BatFileName );
			if( BatFileInfo.Exists )
			{
				BatFileInfo.IsReadOnly = false;
				BatFileInfo.Delete();
			}

			StreamWriter Writer = BatFileInfo.AppendText();

			string OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerSetP4PORT);
			Writer.WriteLine("\"C:\\Program Files\\Perforce\\p4.exe\" set P4PORT=" + Properties.Settings.Default.SourceServerName + " > " + OutputFile);
			OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerSetP4CLIENT);
			Writer.WriteLine( "\"C:\\Program Files\\Perforce\\p4.exe\" set P4CLIENT=" + Builder.BranchDef.CurrentClient.ClientName + " > " + OutputFile );
			OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerSetP4USER);
			Writer.WriteLine("\"C:\\Program Files\\Perforce\\p4.exe\" set P4USER=" + Builder.BranchDef.User + " > " + OutputFile);
			OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerSetP4PASSWD);
			Writer.WriteLine("\"C:\\Program Files\\Perforce\\p4.exe\" set P4PASSWD=" + Builder.BranchDef.Password + " > " + OutputFile);

			OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerP4Set);
			Writer.WriteLine("\"C:\\Program Files\\Perforce\\p4.exe\" set > " + OutputFile);

			OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerP4Info);
			Writer.WriteLine("\"C:\\Program Files\\Perforce\\p4.exe\" info > " + OutputFile);

			string SourceFolder = Path.Combine( Environment.CurrentDirectory );
			string SymbolsFolder = Path.Combine( Environment.CurrentDirectory, "Engine", "Binaries");
			OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerResults);
			Writer.WriteLine( "call \"" + Builder.ToolConfig.SourceServerCmd + "\" -Source=\"" + SourceFolder + "\" -Symbols=\"" + SymbolsFolder + "\" -DEBUG > " + OutputFile );

			// We need to add all the games here as well
			// This assumes that the current directory is depot/UE4
			DirectoryInfo DirInfo = new DirectoryInfo(Environment.CurrentDirectory);
			foreach (DirectoryInfo SubDirInfo in DirInfo.GetDirectories("*Game", SearchOption.TopDirectoryOnly))
			{
				string GameFolder = SubDirInfo.FullName.Replace(Environment.CurrentDirectory, "");
				if (Environment.CurrentDirectory.EndsWith("\\") == false)
				{
					GameFolder = SubDirInfo.FullName.Replace(Environment.CurrentDirectory + "\\", "");
				}
				SymbolsFolder = Path.Combine(SubDirInfo.FullName, "Binaries");
				DirectoryInfo CheckDir = new DirectoryInfo(SymbolsFolder);
				if (CheckDir.Exists)
				{
					string OptionalTag = "_" + GameFolder;
					OutputFile = Builder.GetLogFileName(COMMANDS.UpdateSourceServerResults, OptionalTag);
					Writer.WriteLine("call \"" + Builder.ToolConfig.SourceServerCmd + "\" -Source=\"" + SourceFolder + "\" -Symbols=\"" + SymbolsFolder + "\" -DEBUG > " + OutputFile);
				}
			}

			Writer.Close();

			return BatFileName;
		}

		public MODES UpdateSourceServer()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.UpdateSourceServer ), false );

				// Set the perforce environment
				string BatFileName = CreateBatFileHack();

				// Create a source server ini file that will work
				UpdateSourceServerIni();

				string CommandLine = "/c \"" + BatFileName + "\"";
				CurrentBuild = new BuildProcess(Parent, Builder, CommandLine, "", true, false);
				CurrentBuild.WaitForExit();
				State = CurrentBuild.GetErrorLevel();
				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.UpdateSourceServer;
				Builder.Write( "Error: while updating source server." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CheckSigned()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CheckSigned ), true );

				Parent.Log( "[STATUS] Checking '" + Builder.GetCurrentCommandLine() + "' for signature", Color.Magenta );

				string SignToolName = Builder.ToolConfig.SignToolName;
				string CommandLine = "verify /pa /v /tw " + Builder.GetCurrentCommandLine();

				CurrentBuild = new BuildProcess( Parent, Builder, SignToolName, CommandLine, "", false );
				State = COMMANDS.CheckSigned;
				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CheckSigned;
				Builder.Write( "Error: while checking for signed binaries." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		private void SignHelper( COMMANDS Command, string FileToSign )
		{
			Builder.OpenLog( Builder.GetLogFileName( Command ), true );
			Parent.Log( "[STATUS] Signing '" + FileToSign + "'", Color.Magenta );

			string SignToolName = Builder.ToolConfig.SignToolName;
			string CommandLine = "sign /a";
			if( Builder.BranchDef.PFXSubjectName.Length > 0 )
			{
				CommandLine += " /n \"" + Builder.BranchDef.PFXSubjectName + "\"";
			}

			CommandLine += " /t http://timestamp.verisign.com/scripts/timestamp.dll /v " + FileToSign;

			CurrentBuild = new BuildProcess( Parent, Builder, SignToolName, CommandLine, "", false );
			State = CurrentBuild.GetErrorLevel();
			StartTime = DateTime.UtcNow;
		}

		public MODES Sign()
		{
			try
			{
				SignHelper( COMMANDS.Sign, Builder.GetCurrentCommandLine() );
			}
			catch
			{
				State = COMMANDS.Sign;
				Builder.Write( "Error: while signing binaries." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES SignCat()
		{
			try
			{
				GameConfig Config = Builder.CreateGameConfig();
				List<string> CatFiles = Config.GetCatNames( Builder.BranchDef.Version );

				SignHelper( COMMANDS.SignCat, CatFiles[0] );
			}
			catch
			{
				State = COMMANDS.SignCat;
				Builder.Write( "Error: while signing cat file." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES SignBinary()
		{
			try
			{
				GameConfig Config = Builder.CreateGameConfig();
				List<string> ExeFiles = Config.GetExecutableNames( Builder.BranchDef.Version );

				SignHelper( COMMANDS.SignBinary, ExeFiles[0] );
			}
			catch
			{
				State = COMMANDS.SignBinary;
				Builder.Write( "Error: while signing binary." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES SignFile()
		{
			try
			{
				string[] Parms = Builder.SplitCommandline();
				string ActiveFolder = Builder.GetFolderName();
				string SourceFile = Path.Combine( Parms[0], Path.Combine( ActiveFolder, Builder.BranchDef.Branch ) ) + "\\" + Parms[1];

				SignHelper( COMMANDS.SignFile, SourceFile );
			}
			catch
			{
				State = COMMANDS.SignFile;
				Builder.Write( "Error: while signing file." );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CopyScriptPatchFiles()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CopyScriptPatchFiles ), true );

				GameConfig Config = Builder.CreateGameConfig();
				string ScriptPatchFolder = Path.Combine( Config.GetPatchesFolderName(), Config.GetCookedFolderPlatform() );
				string TUScriptPatchFolder = Path.Combine( Config.GetTUFolderName( Builder.GetCurrentCommandLine() ), Config.GetPatchesFolderName() );

				Builder.Write( "Copying: " + ScriptPatchFolder );
				Builder.Write( " ... to: " + TUScriptPatchFolder );

				// Copy over all the script patch files
				DirectoryInfo DirInfo = new DirectoryInfo( ScriptPatchFolder );
				if( DirInfo.Exists )
				{
					DirectoryInfo DestDirInfo = new DirectoryInfo( TUScriptPatchFolder );

					// Ensure the parent exists
					Parent.EnsureDirectoryExists( DestDirInfo.Parent.FullName );
					// Ensure the actual folder does *not* exist
					if( DestDirInfo.Exists )
					{
						DestDirInfo.Delete( true );
					}

					DirInfo.MoveTo( TUScriptPatchFolder );
				}
				else
				{
					Builder.Write( "Error: Source folder does not exist!" );
				}

				Builder.CloseLog();
			}
			catch( Exception Ex )
			{
				State = COMMANDS.CopyScriptPatchFiles;
				Builder.Write( "Error: exception copying script patch files." );
				Builder.Write( "Error: " + Ex.Message );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		private void DoCopy( string SourcePath, string DestFolder )
		{
			Builder.Write( "Copying: " + SourcePath );
			Builder.Write( " ... to: " + DestFolder );

			FileInfo Source = new FileInfo( SourcePath );
			if( Source.Exists )
			{
				// Get the filename
				int FileNameOffset = SourcePath.LastIndexOf( '/' );
				string FileName = SourcePath.Substring( FileNameOffset + 1 );
				string DestPathName = DestFolder + "/" + FileName;

				// Create the dest folder if it doesn't exist
				DirectoryInfo DestDir = new DirectoryInfo( DestFolder );
				if( !DestDir.Exists )
				{
					Builder.Write( " ... creating: " + DestDir.FullName );
					DestDir.Create();
				}

				// Copy the file
				FileInfo Dest = new FileInfo( DestPathName );
				if( Dest.Exists )
				{
					Builder.Write( " ... deleting: " + Dest.FullName );
					Dest.IsReadOnly = false;
					Dest.Delete();
				}

				Source.CopyTo( DestPathName, true );
			}
			else
			{
				Builder.Write( "Error: source file does not exist for copying" );
				State = COMMANDS.SimpleCopy;
			}
		}

		public MODES SimpleCopy()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SimpleCopy ), true );

				string PathName = Builder.GetCurrentCommandLine();
				DoCopy( PathName, Builder.CopyDestination );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SimpleCopy;
				Builder.Write( "Error: exception copying file" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SetSteamID()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SetSteamID ), false );

				GameConfig Config = Builder.CreateGameConfig( Builder.LabelInfo.Game, "Win32" );
				string Source = Config.GetSteamIDSource();
				string Destination = Config.GetSteamIDestinationFolder();
				DoCopy( Source, Destination );

				Config = Builder.CreateGameConfig( Builder.LabelInfo.Game, "Win64" );
				Destination = Config.GetSteamIDestinationFolder();
				DoCopy( Source, Destination );

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SimpleCopy;
				Builder.Write( "Error: exception copying file" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SimpleDelete()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SimpleDelete ), true );

				string PathName = Builder.GetCurrentCommandLine();

				Builder.Write( "Deleting: " + PathName );

				FileInfo Source = new FileInfo( PathName );
				if( Source.Exists )
				{
					Source.Delete();
				}
				else
				{
					// Silent "error" that will log the event, but not actually kill the build
					Builder.Write( "Warning: source file does not exist for deletion" );
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SimpleDelete;
				Builder.Write( "Error: exception deleting file" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES SimpleRename()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.SimpleRename ), true );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: while renaming file (wrong number of parameters)" );
					State = COMMANDS.SimpleRename;
				}
				else
				{
					string BaseFolder = "";
					if( Builder.CopyDestination.Length > 0 )
					{
						BaseFolder = Builder.CopyDestination + "/" + Builder.BranchDef.Branch + "/";
					}

					Builder.Write( "Renaming: " );
					Builder.Write( " ... from: " + BaseFolder + Parms[0] );
					Builder.Write( " ... to: " + BaseFolder + Parms[1] );

					FileInfo Source = new FileInfo( BaseFolder + Parms[0] );
					if( Source.Exists )
					{
						FileInfo Dest = new FileInfo( BaseFolder + Parms[1] );
						if( Dest.Exists )
						{
							Dest.IsReadOnly = false;
							Dest.Delete();
						}
						Source.IsReadOnly = false;

						Source.CopyTo( Dest.FullName );
						Source.Delete();
					}
					else
					{
						Builder.Write( "Error: source file does not exist for renaming" );
						State = COMMANDS.SimpleRename;
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.SimpleRename;
				Builder.Write( "Error: exception renaming file" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		public MODES RenamedCopy()
		{
			try
			{
				Builder.OpenLog( Builder.GetLogFileName( COMMANDS.RenamedCopy ), true );

				string[] Parms = Builder.SplitCommandline();
				if( Parms.Length != 2 )
				{
					Builder.Write( "Error: while renaming and copying file (wrong number of parameters)" );
					State = COMMANDS.RenamedCopy;
				}
				else
				{
					Builder.Write( "Renaming and copying: " );
					Builder.Write( " ... from: " + Parms[0] );
					Builder.Write( " ... to: " + Parms[1] );

					FileInfo Source = new FileInfo( Parms[0] );
					if( Source.Exists )
					{
						FileInfo Dest = new FileInfo( Parms[1] );
						if( Dest.Exists )
						{
							Builder.Write( " ... deleting: " + Dest.FullName );
							Dest.IsReadOnly = false;
							Dest.Delete();
						}

						Builder.Write( " ... creating: " + Dest.DirectoryName );
						Parent.EnsureDirectoryExists( Dest.DirectoryName );

						Source.CopyTo( Dest.FullName );
					}
					else
					{
						Builder.Write( "Error: source file does not exist for renaming and copying" );
						State = COMMANDS.RenamedCopy;
					}
				}

				Builder.CloseLog();
			}
			catch
			{
				State = COMMANDS.RenamedCopy;
				Builder.Write( "Error: exception renaming and copying file" );
				Builder.CloseLog();
			}

			return MODES.Finalise;
		}

		// Reads the list of files in a project from the specified project file
		private List<string> CheckForUCInVCProjFiles_GetProjectFiles( string ProjectPath, string FileExtension )
		{
			List<string> RelativeFilePaths = new List<string>();
			using( FileStream ProjectStream = new FileStream( ProjectPath, FileMode.Open, FileAccess.Read ) )
			{
				// Parse the project's root node.
				XPathDocument Doc = new XPathDocument( ProjectStream );
				XPathNavigator Nav = Doc.CreateNavigator();

				var MyNameTable = new NameTable();
				var NSManager = new XmlNamespaceManager( MyNameTable );
				NSManager.AddNamespace( "ns", "http://schemas.microsoft.com/developer/msbuild/2003" );

				XPathNavigator MSBuildProjVersion = Nav.SelectSingleNode( "/ns:Project/@ToolsVersion", NSManager );
				if( MSBuildProjVersion != null && ( MSBuildProjVersion.Value == "4.0" || MSBuildProjVersion.Value == "4,0" ) )
				{
					XPathNodeIterator UCIter = Nav.Select( "/ns:Project/ns:ItemGroup/ns:None/@Include", NSManager );
					foreach( XPathNavigator It in UCIter )
					{
						// Add all matching files to the tracking array
						if( It.Value.EndsWith( FileExtension, true, CultureInfo.CurrentCulture ) )
						{
							RelativeFilePaths.Add( Path.GetFileName( It.Value ).ToLowerInvariant() );
						}
					}
				}
			}
			return RelativeFilePaths;
		}

		// Gets the list of files that matches a specified pattern
		private void CheckForUCInVCProjFiles_DirectorySearch( string RootDirectory, string FilePattern, ref List<string> ListToAddTo )
		{
			foreach( string FileName in Directory.GetFiles( RootDirectory, FilePattern, SearchOption.AllDirectories ) )
			{
				ListToAddTo.Add( FileName.ToLowerInvariant() );
			}
		}

		public MODES CheckForUCInVCProjFiles()
		{
			List<string> ListOfUCFiles = new List<string>();
			List<string> ListOfProjects = new List<string>();
			Dictionary<string, List<string>> ListOfFilesInProjects = new Dictionary<string, List<string>>();

			Builder.OpenLog( Builder.GetLogFileName( COMMANDS.CheckForUCInVCProjFiles ), true );

			// We run from <branch>\Development\Builder and we need to get to <branch>\Development\Src
			string RootDirectory = Path.Combine( Environment.CurrentDirectory, "Development\\Src" );
			CheckForUCInVCProjFiles_DirectorySearch( RootDirectory, "*.uc", ref ListOfUCFiles );
			CheckForUCInVCProjFiles_DirectorySearch( RootDirectory, "*.vcxproj", ref ListOfProjects );
			foreach( string ProjectName in ListOfProjects )
			{
				ListOfFilesInProjects.Add( ProjectName, CheckForUCInVCProjFiles_GetProjectFiles( ProjectName, ".uc" ) );
			}

			// Check for any files on disk that are missing in the project
			int NumberOfMissingFiles = 0;
			foreach( string UCFile in ListOfUCFiles )
			{
				// For each script file...
				string UCFileDirName = Path.GetDirectoryName( UCFile ) + "\\";
				foreach( string ProjectName in ListOfProjects )
				{
					// Look in each project...
					List<string> ListOfFilesInProject;
					if( ListOfFilesInProjects.TryGetValue( ProjectName, out ListOfFilesInProject ) )
					{
						// Find the appropriate project...
						string ProjectDirectoryName = Path.GetDirectoryName( ProjectName ) + "\\";
						if( UCFileDirName.Contains( ProjectDirectoryName ) )
						{
							// If it's not there, log out an error...
							if( !ListOfFilesInProject.Contains( Path.GetFileName( UCFile ).ToLowerInvariant() ) )
							{
								Builder.Write( "Error: " + ProjectName + " is missing " + UCFile );
								NumberOfMissingFiles++;
							}
						}
					}
				}
			}
			if( NumberOfMissingFiles > 0 )
			{
				Builder.Write( "Error: missing " + NumberOfMissingFiles.ToString() + " file(s) from the project file" );
				State = COMMANDS.CheckForUCInVCProjFiles;
			}
			else
			{
				Builder.Write( "Not missing any files in the project file" );
			}

			// Check for any files in the project, missing from disk (forgot to check in a new file?)
			NumberOfMissingFiles = 0;
			foreach( string ProjectName in ListOfProjects )
			{
				// For each project...
				List<string> ListOfFilesInProject;
				if( ListOfFilesInProjects.TryGetValue( ProjectName, out ListOfFilesInProject ) )
				{
					// For each script file in the project...
					string ProjectClassesDirectoryName = Path.GetDirectoryName( ProjectName ) + "\\classes\\";
					foreach( string FileInProject in ListOfFilesInProject )
					{
						// Check for the script file on disk...
						string UCFileInProjectClassesDirectory = Path.Combine( ProjectClassesDirectoryName, FileInProject );
						if( !ListOfUCFiles.Contains( UCFileInProjectClassesDirectory ) )
						{
							Builder.Write( "Error: " + ProjectName + " contains a reference to " + UCFileInProjectClassesDirectory + ", which is missing on disk" );
							NumberOfMissingFiles++;
						}
					}
				}
			}
			if( NumberOfMissingFiles > 0 )
			{
				Builder.Write( "Error: missing " + NumberOfMissingFiles.ToString() + " script file(s) on disk that are referenced in projects" );
				State = COMMANDS.CheckForUCInVCProjFiles;
			}
			else
			{
				Builder.Write( "Not missing any files on disk" );
			}

			Builder.CloseLog();

			return MODES.Finalise;
		}

		public MODES SmokeTest()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.SmokeTest );
				Builder.OpenLog( LogFileName, false );

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				List<string> Executables = Config.GetExecutableNames( Builder.BranchDef.Version );
				if( Executables.Count > 0 )
				{
					string CommandLine = "smoketest " + CommonCommandLine;

					// Special smoketest for dedicated servers
					if( Config.Platform.ToLower() == "win32server" )
					{
						CommandLine = "-smoketest -login=epicsmoketest@xboxtest.com -password=supersecret " + CommonCommandLine;
					}

					CurrentBuild = new BuildProcess( Parent, Builder, Executables[0], CommandLine, "", true );
					State = CurrentBuild.GetErrorLevel();

					StartTime = DateTime.UtcNow;
				}
				else
				{
					State = COMMANDS.SmokeTest;
					Builder.Write( "Error: could not evaluate executable name" );
					Builder.CloseLog();
				}
			}
			catch
			{
				State = COMMANDS.SmokeTest;
				Builder.Write( "Error: exception while starting smoke test" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES LoadPackages()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.LoadPackages );
				Builder.OpenLog( LogFileName, false );

				// Optional command line parameter to set the packages to load
				string PackageNames = "-all";
				if( Builder.GetCurrentCommandLine().Length > 0 )
				{
					PackageNames = Builder.GetCurrentCommandLine();
				}

				GameConfig Config = Builder.CreateGameConfig();
				CleanIniFiles( Config );

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "loadpackage " + PackageNames + " " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.LoadPackages;
				Builder.Write( "Error: exception while starting loading all packages" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES CookPackages()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.CookPackages );
				Builder.OpenLog( LogFileName, false );

				string[] Params = Builder.SplitCommandline();
				if( Params.Length < 1 )
				{
					Builder.Write( "Error: wrong number of parameters. Usage: " + COMMANDS.CookPackages.ToString() + " <Platform>" );
					State = COMMANDS.CookPackages;
					return MODES.Monitor;
				}

				// Optional command line parameter to set the packages to load
				string PackageNames = "-full";
				if( Params.Length > 1 )
				{
					PackageNames = Builder.GetCurrentCommandLine().Substring( Params[0].Length );
				}

				GameConfig Config = Builder.CreateGameConfig();

				string CommandLine = "";
				string Executable = Config.GetComName( Builder.BranchDef.Version, ref CommandLine );
				CommandLine += "cookpackages -platform=" + Params[0] + " " + PackageNames + " " + CommonCommandLine;

				CurrentBuild = new BuildProcess( Parent, Builder, Executable, CommandLine, "", true );
				State = CurrentBuild.GetErrorLevel();

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.CookPackages;
				Builder.Write( "Error: exception while starting cooking all packages" );
				Builder.CloseLog();
			}

			return MODES.Monitor;
		}

		public MODES PhysXGeneratePhysX()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PhysXGeneratePhysX );

				string[] Parameters = Builder.SplitCommandline();
				if( Parameters.Length > 0 )
				{
					State = COMMANDS.PhysXGeneratePhysX;
				}
				else
				{
					string CommandLine = "/c create_projects_public.cmd > " + "../../../../" + LogFileName + " 2>&1";

					CurrentBuild = new BuildProcess(Parent, Builder, CommandLine, "PhysX_32_Epic/Source/compiler/xpj");
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PhysXGeneratePhysX;
			}

			return MODES.Monitor;
		}

		public MODES PhysXGenerateAPEX()
		{
			try
			{
				string LogFileName = Builder.GetLogFileName( COMMANDS.PhysXGenerateAPEX );

				string[] Parameters = Builder.SplitCommandline();
				if( Parameters.Length > 0 )
				{
					State = COMMANDS.PhysXGenerateAPEX;
				}
				else
				{
					string CommandLine = "/c create_projects.cmd > " + "../../../" + LogFileName + " 2>&1";

					CurrentBuild = new BuildProcess(Parent, Builder, CommandLine, "APEX_1.2_vs_PhysX_3.2/compiler/xpj");
					State = CurrentBuild.GetErrorLevel();
				}

				StartTime = DateTime.UtcNow;
			}
			catch
			{
				State = COMMANDS.PhysXGenerateAPEX;
			}

			return MODES.Monitor;
		}

		public bool IsTimedOut()
		{
			return DateTime.UtcNow - StartTime > Builder.GetTimeout();
		}

		public MODES IsFinished()
		{
			// Also check for timeout
			if( CurrentBuild != null )
			{
				if( CurrentBuild.IsFinished )
				{
					CurrentBuild.Cleanup();
					return MODES.Finalise;
				}

				if( IsTimedOut() )
				{
					CurrentBuild.Kill();
					State = COMMANDS.TimedOut;
					return MODES.Finalise;
				}

				if( !CurrentBuild.IsResponding() )
				{
					if( DateTime.UtcNow - LastRespondingTime > Builder.GetRespondingTimeout() )
					{
						CurrentBuild.Kill();
						State = COMMANDS.Crashed;
						return MODES.Finalise;
					}
				}
				else
				{
					LastRespondingTime = DateTime.UtcNow;
				}

				return MODES.Monitor;
			}

			// No running build? Something went wrong
			return MODES.Finalise;
		}

		public void Kill()
		{
			if( CurrentBuild != null )
			{
				CurrentBuild.Kill();
			}
		}
	}
}
