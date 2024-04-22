/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	/** An interface implemented by UE3 games to define their build behavior. */
	interface UE3BuildGame
	{
		/** Returns the singular name of the game being built ("ExampleGame", "UDKGame", etc) */
		string GetGameName();

		/** Returns a subplatform (e.g. dll) to disambiguate object files NOTE: Needs adding to the command line to pass into Clean*.bat */
		string GetSubPlatform();

		string GetDesiredOnlineSubsystem( CPPEnvironment CPPEnv, UnrealTargetPlatform Platform );

		/** Returns true if the game wants to have PC ES2 simulator (ie ES2 Dynamic RHI) enabled */
		bool ShouldCompileES2();

		/** Returns whether PhysX should be compiled on mobile platforms */
		bool ShouldCompilePhysXMobile();

		/** Allows the game to add any global environment settings before building */
		void GetGameSpecificGlobalEnvironment(CPPEnvironment GlobalEnvironment, UnrealTargetPlatform Platform);

		/** Allows the game to add any Platform/Configuration environment settings before building */
		void GetGameSpecificPlatformConfigurationEnvironment(CPPEnvironment GlobalEnvironment, LinkEnvironment FinalLinkEnvironment);

		/** Returns the xex.xml file for the given game */
		FileItem GetXEXConfigFile();

		/** Allows the game to add any additional environment settings before building */
		void SetUpGameEnvironment(CPPEnvironment GameCPPEnvironment, LinkEnvironment FinalLinkEnvironment, List<UE3ProjectDesc> GameProjects);
	}

	enum UnrealTargetPlatform
	{
		Unknown,
		Win32,
		Win64,
		IPhone,
		AndroidARM,
        Androidx86,
		Mac,
		Flash,
	}

	enum UnrealTargetConfiguration
	{
		Unknown,
		Debug,
		Release,
		Shipping,
		Test,
	}

	enum UnrealProjectType
	{		
		CPlusPlus,	// C++ or C++/CLI
		CSharp,		// C#
	}

	class UE3ProjectDesc
	{
		/** The project path and file name */
		public string ProjectPath;

		/** The type of project */
		public UnrealProjectType ProjectType;

		/** For C++ projects, sets whether the CLR (Common Language Runtime) should be enabled for this project */
		public CPPCLRMode CLRMode = CPPCLRMode.CLRDisabled;

		/** For C# projects, the destination assembly file that the project generates */
		public string OutputAssemblyFileName;

		public UE3ProjectDesc( string InProjectPath )
		{
			ProjectPath = InProjectPath;
			CLRMode = CPPCLRMode.CLRDisabled;
			DetermineProjectType();
		}

		public UE3ProjectDesc( string InProjectPath, CPPCLRMode InCLRMode )
		{
			ProjectPath = InProjectPath;
			CLRMode = InCLRMode;
			DetermineProjectType();
		}

		public UE3ProjectDesc(string InProjectPath, string InOutputAssembly)
		{
			ProjectPath = InProjectPath;
			CLRMode = CPPCLRMode.CLRDisabled;
			OutputAssemblyFileName = InOutputAssembly;
			DetermineProjectType();
		}

		/** Sets the type of project based on the project file's extension */
		void DetermineProjectType()
		{
			// Try to determine the project type by peeking at the project file's extension
			string LowerProjectFileExt = Path.GetExtension(ProjectPath).ToLowerInvariant();
			if (LowerProjectFileExt == ".vcxproj")
			{
				ProjectType = UnrealProjectType.CPlusPlus;
			}
			else if (LowerProjectFileExt == ".csproj")
			{
				ProjectType = UnrealProjectType.CSharp;
				if (OutputAssemblyFileName == null || OutputAssemblyFileName.Length == 0)
				{
					throw new BuildException("A valid destination assembly file name is required for C# projects");
				}
			}
			else
			{
				throw new BuildException("Unrecognized project file extension '{0}'", Path.GetExtension(ProjectPath));
			}
		}
	}


	partial class UE3BuildTarget
	{
		/** Game we're building. */
		UE3BuildGame Game = null;
		
		/** Platform as defined by the VCProject and passed via the command line. Not the same as internal config names. */
		UnrealTargetPlatform Platform = UnrealTargetPlatform.Unknown;
		
		/** Target as defined by the VCProject and passed via the command line. Not necessarily the same as internal name. */
		UnrealTargetConfiguration Configuration = UnrealTargetConfiguration.Unknown;
		
		/** Output path of final executable. */
		string OutputPath = null;

		/** Remote path of the binary if it is to be synced with CookerSync */
		string RemoteRoot = null;

		/** Additional definitions passed via the command line. */
		List<string> AdditionalDefinitions = new List<string>();

		/** The C++ environment that all the environments used to compile UE3-based projects are derived from. */
		CPPEnvironment GlobalCPPEnvironment = new CPPEnvironment();

		/** The link environment that produces the output executable. */
		LinkEnvironment FinalLinkEnvironment = new LinkEnvironment();

		/** A list of projects that are compiled with the game-independent compilation environment. */
		List<UE3ProjectDesc> NonGameProjects = new List<UE3ProjectDesc>();

		/** A list of projects that are compiled with the game-dependent compilation environment. */
		List<UE3ProjectDesc> GameProjects = new List<UE3ProjectDesc>();

		/**
		 * Constructor, initializing target information based on passed in arguments.
		 * 
		 * @param	Arguments	Arguments to extract target information from
		 */
		public UE3BuildTarget( string[] Arguments )
		{
			// Parse the passed in arguments to set up target information.
			ParseArguments(Arguments);
		}

		/**
		 * @return List of all projects being built for this Target configuration
		 */
		public List<string> GetProjectPaths() 
		{
			List<string> AllProjects = new List<string>(); 
			foreach (UE3ProjectDesc ProjDesc in NonGameProjects)
			{
				AllProjects.Add(ProjDesc.ProjectPath);
			}
			foreach (UE3ProjectDesc ProjDesc in GameProjects)
			{
				AllProjects.Add(ProjDesc.ProjectPath);
			}
			return AllProjects;
		}

		public List<FileItem> Build()
		{
			// Validate UE3 configuration - needs to happen before setting any environment mojo and after argument parsing.
			UE3BuildConfiguration.ValidateConfiguration();

			// Describe what's being built.
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Console.WriteLine("Building {0} - {1} - {2}", Game.GetGameName(), Platform, Configuration);
			}

			// Set up the global compile and link environment in GlobalCPPEnvironment and FinalLinkEnvironment.
			SetUpGlobalEnvironment();

			// Initialize/ serialize the dependency cache from disc.
			InitializeDependencyCaches();

			// Initialize the CPP and Link environments per game and per config
			SetUpPlatformEnvironment();
			SetUpConfigurationEnvironment();

			// Validates current settings and updates if required.
			BuildConfiguration.ValidateConfiguration( GlobalCPPEnvironment.TargetConfiguration, GlobalCPPEnvironment.TargetPlatform );

			// Compile the game and platform independent projects.
			CompileNonGameProjects();

			// Compile the game-specific projects.
			CompileGameProjects();

			// Save modified dependency cache and discard it.
			SaveDependencyCaches();
			
			// Put the non-executable output files (PDB, import library, etc) in the intermediate directory.
			// Note that this is overridden in GetWindowsOutputItems().
			FinalLinkEnvironment.OutputDirectory = GlobalCPPEnvironment.OutputDirectory;

			// By default, shadow source files for this target in the root OutputDirectory
			FinalLinkEnvironment.LocalShadowDirectory = FinalLinkEnvironment.OutputDirectory;

			// Link the object files into an executable.
			List<FileItem> OutputItems = null;
			if( Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 )
			{
				OutputItems = GetWindowsOutputItems();
			}
			else if (Platform == UnrealTargetPlatform.IPhone)
			{
				OutputItems = GetIPhoneOutputItems();
			}
            else if (Platform == UnrealTargetPlatform.AndroidARM || 
                     Platform == UnrealTargetPlatform.Androidx86)
			{
				OutputItems = GetAndroidOutputItems();
			}
			else if (Platform == UnrealTargetPlatform.Mac)
			{
				OutputItems = GetMacOutputItems();
			}
			else if (Platform == UnrealTargetPlatform.Flash)
			{
				OutputItems = GetFlashOutputItems();
			}
			else
			{
				Debug.Fail("Unrecognized build platform.");
			}

			// Generates a prerequisite for each action based on the command and arguments used
			// to execute it, augmenting the normal dependency tracking. The biggest benefit of
			// this is the ability to avoid a full "make clean" whenever UBT is recompiled,
			// which we used to do because we didn't know what might have changed.
			GenerateActionCommandPrerequisites();

			return OutputItems;
		}

		private string GetOutputExtension( UnrealTargetPlatform Platform )
		{
			string Extension = "";
			switch( Platform )
			{
			case UnrealTargetPlatform.Win32:
			case UnrealTargetPlatform.Win64:
				Extension = ".exe";
				break;

			case UnrealTargetPlatform.IPhone:
			case UnrealTargetPlatform.Mac:
				break;

			case UnrealTargetPlatform.AndroidARM:
            case UnrealTargetPlatform.Androidx86:
				Extension += ".apk";
				break;

			case UnrealTargetPlatform.Flash:
				Extension += ".stub";
				break;

			default:
				throw new BuildException( "Couldn't determine platform name." );
			}

			return Extension;
		}

		/**
		 * @return Name of target, e.g. name of the game being built.
		 */
		public string GetGameName()
		{
			return Game.GetGameName();
		}
		/**
		 * @return Name of configuration, e.g. "Release"
		 */
		public UnrealTargetConfiguration GetConfiguration()
		{
			return Configuration;
		}
		/**
		 * @return Name of platform, e.g. "Win32"
		 */
		public UnrealTargetPlatform GetPlatform()
		{
			return Platform;
		}
		/**
		 * @return TRUE if debug information is created, FALSE otherwise.
		 */
		public bool IsCreatingDebugInfo()
		{
			return GlobalCPPEnvironment.bCreateDebugInfo;
		}
		/**
		 * @return The overall output directory of actions for this target.
		 */
		public string GetOutputDirectory()
		{
			return GlobalCPPEnvironment.OutputDirectory;
		}

		/** 
		 * @return TRUE if we are building for dedicated server, FALSE otherwise.
		 */
		public bool IsBuildingDedicatedServer()
		{
			return UE3BuildConfiguration.bBuildDedicatedServer;
		}

		void ParseArguments(string[] SourceArguments)
		{
			// Get any extra user arguments
			List<string> ExtraArguments = BuildConfiguration.GetExtraCommandArguments();

			// Combine the two arrays of arguments
			List<string> Arguments = new List<string>(SourceArguments.Length + ExtraArguments.Count);
			Arguments.AddRange(SourceArguments);
			Arguments.AddRange(ExtraArguments);
            
			for (int ArgumentIndex = 0; ArgumentIndex < Arguments.Count; ArgumentIndex++)
			{
				switch (Arguments[ArgumentIndex].ToUpperInvariant())
				{
					// Game names:
					case "EXAMPLEGAME":
						Game = new UE3BuildExampleGame();
						break;
                    case "DUKEGAME":
                        Game = new UE3BuildDukeGame();
                        break;
					case "EXOGAME":
						Game = new UE3BuildExoGame();
						break;
					case "UDKGAME":
						Game = new UE3BuildUDKGame();
						UE3BuildConfiguration.bCompileRealD = true;
						UE3BuildConfiguration.bCompileSubstanceAir = true;
						break;
					case "UDKGAMEDLL":
						Game = new UE3BuildUDKGameDLL();
						break;
					case "SWORDGAME":
						Game = new UE3BuildSwordGame();
						break;

					// Platform names:
					case "WIN32":
						Platform = UnrealTargetPlatform.Win32;
						break;
					case "WIN64":
					case "X64":
						Platform = UnrealTargetPlatform.Win64;
						break;
					case "IPHONE":
						Platform = UnrealTargetPlatform.IPhone;
						break;
					case "ANDROIDARM":
						Platform = UnrealTargetPlatform.AndroidARM;
						break;
                    case "ANDROIDX86":
                        Platform = UnrealTargetPlatform.Androidx86;
                        break;
					case "MAC":
						Platform = UnrealTargetPlatform.Mac;
						break;
					case "FLASH":
						Platform = UnrealTargetPlatform.Flash;
						break;

					// Configuration names:
					case "DEBUG":
						Configuration = UnrealTargetConfiguration.Debug;
						break;
					case "RELEASE":
						Configuration = UnrealTargetConfiguration.Release;
						break;
					case "SHIPPING":
						Configuration = UnrealTargetConfiguration.Shipping;
						break;
					case "TEST":
						Configuration = UnrealTargetConfiguration.Test;
						break;

					// -Output specifies the output filename.
					case "-OUTPUT":
						if (ArgumentIndex + 1 >= Arguments.Count)
						{
							throw new BuildException("Expected path after -output argument, but found nothing.");
						}
						ArgumentIndex++;
						OutputPath = Arguments[ArgumentIndex];
						break;

					// -Define <definition> adds a definition to the global C++ compilation environment.
					case "-DEFINE":
						if (ArgumentIndex + 1 >= Arguments.Count)
						{
							throw new BuildException("Expected path after -define argument, but found nothing.");
						}
						ArgumentIndex++;
						AdditionalDefinitions.Add(Arguments[ArgumentIndex]);
						break;

					// -RemoteRoot <RemoteRoot> sets where the generated binaries are CookerSynced.
					case "-REMOTEROOT":
						if( ArgumentIndex + 1 >= Arguments.Count )
						{
							throw new BuildException( "Expected path after -RemoteRoot argument, but found nothing." );
						}
						ArgumentIndex++;
						if (Arguments[ArgumentIndex].StartsWith("xe:\\") == true)
						{
							RemoteRoot = Arguments[ArgumentIndex].Substring( "xe:\\".Length );
						}
						else if (Arguments[ArgumentIndex].StartsWith("devkit:\\") == true)
						{
							RemoteRoot = Arguments[ArgumentIndex].Substring("devkit:\\".Length);
						}
						break;

					// Build in dedicated server mode
					case "-DEDICATEDSERVER":
						UE3BuildConfiguration.bBuildDedicatedServer = true;
						break;

					// Disable editor support
					case "-NOEDITOR":
						UE3BuildConfiguration.bBuildEditor = false;
						break;
				}
			}

			// handle some special case defines (so build system can pass -DEFINE as normal instead of needing
			// to know about special parameters)
			foreach (string Define in AdditionalDefinitions)
			{
				switch (Define)
				{
					case "WITH_EDITOR=0":
						UE3BuildConfiguration.bBuildEditor = false;
						break;

					case "WITH_STEAMWORKS=0":
						UE3BuildConfiguration.bAllowSteamworks = false;
						break;

					case "FORCE_STEAMWORKS=1":
						UE3BuildConfiguration.bForceSteamworks = true;
						break;

					case "WITH_GAMESPY=0":
						UE3BuildConfiguration.bAllowGameSpy = false;
						break;

					case "FORCE_GAMESPY=1":
						UE3BuildConfiguration.bForceGameSpy = true;
						break;

					case "WITH_GAMECENTER=0":
						UE3BuildConfiguration.bAllowGameCenter = false;
						break;

					case "FORCE_GAMECENTER=1":
						UE3BuildConfiguration.bForceGameCenter = true;
						break;

					case "WITH_PANORAMA=0":
						UE3BuildConfiguration.bAllowLive = false;
						break;

					case "FORCE_PANORAMA=1":
						UE3BuildConfiguration.bForceLive = true;
						break;

					case "DEDICATED_SERVER=1":
						UE3BuildConfiguration.bBuildDedicatedServer = true;
						break;

					case "WITH_SPEEDTREE=0":
						UE3BuildConfiguration.bCompileSpeedTree = false;
						break;

                    // Memory profiler doesn't work if frame pointers are omitted
					case "USE_MALLOC_PROFILER=1":
                        BuildConfiguration.bOmitFramePointers = false;
                        break;

					case "WITH_FBX=0":
						UE3BuildConfiguration.bCompileFBX = false;
						break;

					case "WITH_LEAN_AND_MEAN_UE3=1":
						UE3BuildConfiguration.bCompileLeanAndMeanUE3 = true;
						break;

					case "BUILD_SCRIPT_PATCHING_EXECUTABLE=1":
						UE3BuildConfiguration.bBuildScriptPatchingExecutable = true;
						break;
				}
			}

			// Verify that the required parameters have been found.
			if (Game == null)
			{
				throw new BuildException("Couldn't determine game name.");
			}
			if (Platform == UnrealTargetPlatform.Unknown)
			{
				throw new BuildException("Couldn't find platform name.");
			}
			if (Configuration == UnrealTargetConfiguration.Unknown)
			{
				throw new BuildException("Couldn't determine configuration name.");
			}

			// Construct the output path based on configuration, platform, game if not specified.
			if( OutputPath == null )
			{
				// e.g. ..\..\Binaries\Win64 (defined here so iPhone can override it)
				OutputPath = Path.Combine( "..\\..\\Binaries\\", Platform.ToString() );
				// e.g. UDKGameServer-Win32-Shipping.exe
				string ExecutableName = GetGameName() + Game.GetSubPlatform() + "-" + Platform.ToString() + "-" + Configuration.ToString() + GetOutputExtension( Platform );

				// Example: Binaries\\Win32\\ExampleGame.exe
				if( Configuration == UnrealTargetConfiguration.Release )
				{
					ExecutableName = GetGameName() + Game.GetSubPlatform() + GetOutputExtension( Platform );
				}

                if (Platform == UnrealTargetPlatform.Androidx86)
                {
                    ExecutableName = "lib" + GetGameName() + "-Androidx86-" + Configuration.ToString() + ".so";
                }

				OutputPath = Path.Combine( OutputPath, ExecutableName );
			}

			// Convert output path to absolute path so we can mix and match with invocation via target's nmake
			OutputPath = Path.GetFullPath( OutputPath );

			// If the output is set to be a dll, set the required flags to compile as such
			if( Path.GetExtension( OutputPath ).ToUpper() == ".DLL" )
			{
				BuildConfiguration.bCompileAsdll = true;
				UE3BuildConfiguration.bCompileLeanAndMeanUE3 = true;
			}

			// Enable lean and mean for the Flash platform
			if( Platform == UnrealTargetPlatform.Flash )
			{
				UE3BuildConfiguration.bCompileLeanAndMeanUE3 = true;
			}

			// iphone toolchain will figure out the output, if it's not specified, based on gamename, config, platform
			if (OutputPath == null && Platform == UnrealTargetPlatform.IPhone)
			{
				throw new BuildException("Couldn't find output path on command-line.");
			}
		}

		void SetUpGlobalEnvironment()
		{
			// Do any platform specific tool chain initialization
			{
				if( Platform == UnrealTargetPlatform.IPhone )
				{
					IPhoneToolChain.SetUpGlobalEnvironment();
				}
				else if( Platform == UnrealTargetPlatform.Mac )
				{
					MacToolChain.SetUpGlobalEnvironment();
				}
			}

			// Incorporate toolchain in platform name.
			string PlatformString = Platform.ToString();
			if (BuildConfiguration.bUseIntelCompiler && (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64))
			{
				PlatformString += "-ICL";
			}
			// Incorporate sub type like Server, DLL into platform name
			if (Game.GetSubPlatform() != "" )
			{
				PlatformString += "-" + Game.GetSubPlatform();
			}

			// Determine the directory to store intermediate files in for this target.
			GlobalCPPEnvironment.OutputDirectory = Path.Combine( BuildConfiguration.BaseIntermediatePath, GetGameName() );
			GlobalCPPEnvironment.OutputDirectory = Path.Combine( GlobalCPPEnvironment.OutputDirectory, PlatformString );
			GlobalCPPEnvironment.OutputDirectory = Path.Combine( GlobalCPPEnvironment.OutputDirectory, Configuration.ToString() );

			// By default, shadow source files for this target in the root OutputDirectory
			GlobalCPPEnvironment.LocalShadowDirectory = GlobalCPPEnvironment.OutputDirectory;

			GlobalCPPEnvironment.Definitions.Add("UNICODE");
			GlobalCPPEnvironment.Definitions.Add("_UNICODE");
			GlobalCPPEnvironment.Definitions.Add("__UNREAL__");

			// Add the platform-independent public project include paths.
			GlobalCPPEnvironment.IncludePaths.Add("Core/Inc/Epic");
			GlobalCPPEnvironment.IncludePaths.Add("Core/Inc/Licensee");
			GlobalCPPEnvironment.IncludePaths.Add("Core/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Engine/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("GameFramework/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("IpDrv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("UnrealEd/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("UnrealEd/FaceFX");
			if( UE3BuildConfiguration.bAllowManagedCode )
			{
				GlobalCPPEnvironment.IncludePaths.Add("UnrealEdCLR/Inc");
				GlobalCPPEnvironment.IncludePaths.Add("UnrealSwarm/Inc");
			}

			// Compile and link with PhysX.
			SetUpPhysXEnvironment();

			// Get any game specific global environment settings
			Game.GetGameSpecificGlobalEnvironment(GlobalCPPEnvironment, Platform);

			// Compile and link with FaceFX.
			SetUpFaceFXEnvironment();

			// Compile and link with Bink.
			SetUpBinkEnvironment();

			// Compile and link with RealD.
			SetUpRealDEnvironment();

            // Allegorithmic Substance Air Integration
            // Compile and link with Substance Air
            SetUpSubstanceAir();

            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_SUBSTANCE_AIR=0"))
            {
                NonGameProjects.Add(new UE3ProjectDesc("SubstanceAir/SubstanceAir.vcxproj"));
                GlobalCPPEnvironment.IncludePaths.Add("SubstanceAir/Inc");

                if (UE3BuildConfiguration.bBuildEditor)
                {
                    if (UE3BuildConfiguration.bAllowManagedCode)
                    {
                        NonGameProjects.Add(new UE3ProjectDesc("SubstanceAirEdCLR/SubstanceAirEdCLR.vcxproj", CPPCLRMode.CLREnabled));
                        GlobalCPPEnvironment.IncludePaths.Add("SubstanceAirEdCLR/Inc");
                    }

                    NonGameProjects.Add(new UE3ProjectDesc("SubstanceAirEd/SubstanceAirEd.vcxproj"));
                    GlobalCPPEnvironment.IncludePaths.Add("SubstanceAirEd/Inc");
                }
            }
            // end Allegorithmic Substance Integration

			GlobalCPPEnvironment.IncludePaths.Add("../External/SpeedTree/Include");

			// Compile and link with GFx.
			SetUpGFxEnvironment();

			// Compile and link with Recast.
			SetUpRecastEnvironment();

			// Add the definitions specified on the command-line.
			GlobalCPPEnvironment.Definitions.AddRange(AdditionalDefinitions);

			// Add the projects compiled for all games, all platforms.
			NonGameProjects.Add(new UE3ProjectDesc("Core/Core.vcxproj"));
			NonGameProjects.Add(new UE3ProjectDesc("Engine/Engine.vcxproj"));
			NonGameProjects.Add(new UE3ProjectDesc("GameFramework/GameFramework.vcxproj"));
			NonGameProjects.Add(new UE3ProjectDesc("IpDrv/IpDrv.vcxproj"));

			NonGameProjects.Add(new UE3ProjectDesc("GFxUI/GFxUI.vcxproj"));
			GlobalCPPEnvironment.IncludePaths.Add("GFxUI/Inc");

			// Launch is compiled for all games/platforms, but uses the game-dependent defines, and so needs to be in the game project list.
			GameProjects.Add(new UE3ProjectDesc("Launch/Launch.vcxproj"));

			// Disable editor when its not needed
			if ((Platform != UnrealTargetPlatform.Win32 && Platform != UnrealTargetPlatform.Win64) ||
				UE3BuildConfiguration.bBuildDedicatedServer)
			{
				UE3BuildConfiguration.bBuildEditor = false;
			}

			// Skip compiling rarely used code that isn't already controlled via other globals.
			if (UE3BuildConfiguration.bTrimRarelyUsedCode)
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_TESTTRACK=0");
				GlobalCPPEnvironment.Definitions.Add("WITH_TTS=0");
				GlobalCPPEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			}

			// Propagate whether we want a lean and mean build to the C++ code.
			if (UE3BuildConfiguration.bCompileLeanAndMeanUE3)
			{
				GlobalCPPEnvironment.Definitions.Add("UE3_LEAN_AND_MEAN=1");
			}
			else
			{
				GlobalCPPEnvironment.Definitions.Add("UE3_LEAN_AND_MEAN=0");
			}

			// Set the defines required to build a script patching executable if requested
			if( UE3BuildConfiguration.bBuildScriptPatchingExecutable )
			{
				GlobalCPPEnvironment.Definitions.Add( "SUPPORTS_SCRIPTPATCH_CREATION=1" );
				GlobalCPPEnvironment.Definitions.Add( "WITH_EDITORONLY_DATA=0" );
			}

			// bBuildEditor has now been set appropriately for all platforms, so this is here to make sure the #define 
			if (UE3BuildConfiguration.bBuildEditor)
			{
 				NonGameProjects.Add(new UE3ProjectDesc("UnrealEd/UnrealEd.vcxproj"));
 				GlobalCPPEnvironment.Definitions.Add("WITH_EDITOR=1");

				// Compile and link with SourceControl.
				SetUpSourceControlEnvironment();
			}
			else
			{
				if (!GlobalCPPEnvironment.Definitions.Contains("WITH_EDITOR=0"))
				{
					GlobalCPPEnvironment.Definitions.Add("WITH_EDITOR=0");
				}

				// disable CLR if no editor for now
				UE3BuildConfiguration.bAllowManagedCode = false;
			}

			//Check to see if the APEX build flags have been overridden
			if (UE3BuildConfiguration.bOverrideAPEXBuild)
			{
				if (UE3BuildConfiguration.bUseAPEX)
				{
					GlobalCPPEnvironment.Definitions.Add("WITH_APEX=1");
				}
				else
				{
					GlobalCPPEnvironment.Definitions.Add("WITH_APEX=0");
				}
			}
			GlobalCPPEnvironment.Definitions.Add("PX_NO_WARNING_PRAGMAS");


			//Build config dependent defines
			if (Configuration == UnrealTargetConfiguration.Debug || Configuration == UnrealTargetConfiguration.Release)
			{
				GlobalCPPEnvironment.Definitions.Add("USE_SCOPED_MEM_STATS=1");
			}
		}

		void SetUpPlatformEnvironment()
		{
			// Determine the primary C++ platform to compile the engine for.
			CPPTargetPlatform MainCompilePlatform;
			switch (Platform)
			{
				case UnrealTargetPlatform.Win32: MainCompilePlatform = CPPTargetPlatform.Win32; break;
				case UnrealTargetPlatform.Win64: MainCompilePlatform = CPPTargetPlatform.Win64; break;
				case UnrealTargetPlatform.IPhone: MainCompilePlatform = CPPTargetPlatform.IPhone; break;
				case UnrealTargetPlatform.AndroidARM: MainCompilePlatform = CPPTargetPlatform.AndroidARM; break;
                case UnrealTargetPlatform.Androidx86: MainCompilePlatform = CPPTargetPlatform.Androidx86; break;
				case UnrealTargetPlatform.Mac: MainCompilePlatform = CPPTargetPlatform.Mac; break;
				case UnrealTargetPlatform.Flash: MainCompilePlatform = CPPTargetPlatform.Flash; break;
				default: throw new BuildException("Unrecognized target platform");
			}

			FinalLinkEnvironment.TargetPlatform = MainCompilePlatform;
			GlobalCPPEnvironment.TargetPlatform = MainCompilePlatform;

			// Set up the platform-specific environment.
			switch(Platform)
			{
				case UnrealTargetPlatform.Win32:
				case UnrealTargetPlatform.Win64:
					SetUpWindowsEnvironment();
					break;
				case UnrealTargetPlatform.IPhone:
					SetUpIPhoneEnvironment();
					break;
				case UnrealTargetPlatform.AndroidARM:
                case UnrealTargetPlatform.Androidx86:
					SetUpAndroidEnvironment();
					break;
				case UnrealTargetPlatform.Mac:
					SetUpMacEnvironment();
					break;
				case UnrealTargetPlatform.Flash:
					SetUpFlashEnvironment();
					break;
			}
		}

		void SetUpConfigurationEnvironment()
		{
			// Determine the C++ compile/link configuration based on the Unreal configuration.
			CPPTargetConfiguration CompileConfiguration;
			switch (Configuration)
			{
				default:
				case UnrealTargetConfiguration.Debug:
					CompileConfiguration = CPPTargetConfiguration.Debug;
					GlobalCPPEnvironment.Definitions.Add("_DEBUG=1");
// WITH_GFx
					GlobalCPPEnvironment.Definitions.Add("SF_BUILD_DEBUG=1");
// end WITH_GFx
					GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE=0");
					break;
				case UnrealTargetConfiguration.Release:
					CompileConfiguration = CPPTargetConfiguration.Release;
					GlobalCPPEnvironment.Definitions.Add("NDEBUG=1");
					GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE=0");
					break;
				case UnrealTargetConfiguration.Shipping:
					CompileConfiguration = CPPTargetConfiguration.Shipping;
					GlobalCPPEnvironment.Definitions.Add("NDEBUG=1");

					if( Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 )
					{
						GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE=0");
						GlobalCPPEnvironment.Definitions.Add("SHIPPING_PC_GAME=1");
						if (UE3BuildConfiguration.bCompileLeanAndMeanUE3)
						{
							GlobalCPPEnvironment.Definitions.Add("NO_LOGGING=1");
						}
					}
					else
					{
						GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE=1");
						GlobalCPPEnvironment.Definitions.Add("NO_LOGGING=1");
					}
					FinalLinkEnvironment.bIsShippingBinary = true;
					break;
				case UnrealTargetConfiguration.Test:
					CompileConfiguration = CPPTargetConfiguration.Test;
					GlobalCPPEnvironment.Definitions.Add("NDEBUG=1");

					if( Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 )
					{
						GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE=0");
						GlobalCPPEnvironment.Definitions.Add("SHIPPING_PC_GAME=1");
						if (UE3BuildConfiguration.bCompileLeanAndMeanUE3)
						{
							GlobalCPPEnvironment.Definitions.Add("NO_LOGGING=1");
						}
					}
					else
					{
						GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE=1");
						GlobalCPPEnvironment.Definitions.Add("FINAL_RELEASE_DEBUGCONSOLE=1");
						GlobalCPPEnvironment.Definitions.Add("NO_LOGGING=1");
					}
					FinalLinkEnvironment.bIsShippingBinary = true;
					break;
			}

			// Set up the global C++ compilation and link environment.
			GlobalCPPEnvironment.TargetConfiguration = CompileConfiguration;
			FinalLinkEnvironment.TargetConfiguration = CompileConfiguration;

			// Create debug info based on the heuristics specified by the user.
			GlobalCPPEnvironment.bCreateDebugInfo = !BuildConfiguration.bDisableDebugInfo && DebugInfoHeuristic.ShouldCreateDebugInfo(Platform, Configuration);			

			// NOTE: Even when debug info is turned off, we currently force the linker to generate debug info
			//       anyway on Visual C++ platforms.  This will cause a PDB file to be generated with symbols
			//       for most of the classes and function/method names, so that crashes still yield somewhat
			//       useful call stacks, even though compiler-generate debug info may be disabled.  This gives
			//       us much of the build-time savings of fully-disabled debug info, without giving up call
			//       data completely.
			bool bForceLinkerDebugInfo =
				( Platform == UnrealTargetPlatform.Win32 ||
				  Platform == UnrealTargetPlatform.Win64 );
			FinalLinkEnvironment.bCreateDebugInfo = GlobalCPPEnvironment.bCreateDebugInfo || bForceLinkerDebugInfo;

			/** Allows the game to add any Platform/Configuration environment settings before building */
			Game.GetGameSpecificPlatformConfigurationEnvironment(GlobalCPPEnvironment, FinalLinkEnvironment);
    
		}

		void CompileNonGameProjects()
		{
			CPPEnvironment GameIndependentCPPEnvironment = new CPPEnvironment(GlobalCPPEnvironment);
			// Compile the cross-platform engine projects.
			Utils.CompileProjects(GameIndependentCPPEnvironment, FinalLinkEnvironment, NonGameProjects);
		}

		void CompileGameProjects()
		{
			// Set up the game-specific environment.
			CPPEnvironment GameCPPEnvironment = new CPPEnvironment(GlobalCPPEnvironment);
			Game.SetUpGameEnvironment(GameCPPEnvironment, FinalLinkEnvironment, GameProjects);

			// Compile the game-dependent projects.
			Utils.CompileProjects(GameCPPEnvironment, FinalLinkEnvironment, GameProjects);
		}

		/** Initializes/ creates the dependency caches. */
		void InitializeDependencyCaches()
		{			
			CPPEnvironment.IncludeCache = DependencyCache.Create( Path.Combine(GlobalCPPEnvironment.OutputDirectory,"DependencyCache.bin") );
			CPPEnvironment.DirectIncludeCache = DependencyCache.Create( Path.Combine(GlobalCPPEnvironment.OutputDirectory,"DirectDependencyCache.bin") );
		}

		/** Saves dependency caches and discard them. */
		void SaveDependencyCaches()
		{
			if (CPPEnvironment.IncludeCache != null)
			{
				CPPEnvironment.IncludeCache.Save();
				CPPEnvironment.IncludeCache = null;
			}
			if (CPPEnvironment.DirectIncludeCache != null)
			{
				CPPEnvironment.DirectIncludeCache.Save();
				CPPEnvironment.DirectIncludeCache = null;
			}
		}        

		/** Generates prerequisites for each action based on the command used to execute it. */
		void GenerateActionCommandPrerequisites()
		{
			string OutputDirectory = Path.GetFullPath(GetOutputDirectory());
			foreach (Action Action in UnrealBuildTool.AllActions)
			{
				if ((Action.ProducedItems.Count > 0) &&
					(Action.bShouldTrackCommand) &&
					BuildConfiguration.bUseCommandDependencies)
				{
					// If this action produces output, track its command line
					string CommandPath = Path.GetFullPath(Action.ProducedItems[0].AbsolutePath + ".ubt");
					if (!CommandPath.StartsWith(OutputDirectory, StringComparison.InvariantCultureIgnoreCase))
					{
						CommandPath = Path.Combine(OutputDirectory, Path.GetFileName(CommandPath));
					}
					// Keep a 1-deep history of the commands
					if (File.Exists(CommandPath))
					{
						File.Copy(CommandPath, CommandPath + ".last", true);
					}

					// The contents of the file we track will be the exact command and arguments used to execute it
					string Command = Action.CommandPath + Environment.NewLine + Action.CommandArguments;
					// This call to CreateIntermediateTextFile will only update the file if the contents will change
					FileItem CommandDependency = FileItem.CreateIntermediateTextFile(CommandPath, Command);

					// Finally, add the dependency on the command file (which is only touched when the command or arguments change)
					Action.PrerequisiteItems.Add(CommandDependency);
				}
			}
		}
	}
}
