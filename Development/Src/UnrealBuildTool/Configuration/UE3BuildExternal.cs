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
	partial class UE3BuildTarget
	{
		public static string ForceOnlineSubsystem( UnrealTargetPlatform Platform )
		{
			switch( Platform )
			{
			case UnrealTargetPlatform.Win32:
				if( UE3BuildConfiguration.bForceSteamworks )
				{
					return ( "Steamworks" );
				}
				else if( UE3BuildConfiguration.bForceGameSpy )
				{
					return ( "GameSpy" );
				}
				else if( UE3BuildConfiguration.bForceLive )
				{
					return ( "Live" );
				}
				break;

			case UnrealTargetPlatform.Win64:
                if (UE3BuildConfiguration.bForceSteamworks)
                {
                    return ("Steamworks");
                }
                break;

			case UnrealTargetPlatform.IPhone:
				// Only GameCenter (and PC) are supported on the IPhone
				if (UE3BuildConfiguration.bForceGameCenter)
				{
					return ("GameCenter");
				}
				break;
			}

			return ( null );
		}

		public static bool SupportsOSSPC()
		{
			// No external dependencies required for this
			return ( true );
		}

		public static bool SupportsOSSSteamworks()
		{
			// Check for command line/env var suppression
			if( !UE3BuildConfiguration.bAllowSteamworks )
			{
				return ( false );
			}

			if( !Directory.Exists( "../External/Steamworks" ) )
			{
				return ( false );
			}

			if( !File.Exists( "OnlineSubsystemSteamworks/OnlineSubsystemSteamworks.vcxproj" ) )
			{
				return ( false );
			}

			return ( true );
		}

		public static bool SupportsOSSGameSpy()
		{
			// Check for command line/env var suppression
			if( !UE3BuildConfiguration.bAllowGameSpy )
			{
				return ( false );
			}

			if( !Directory.Exists( "../External/GameSpy" ) )
			{
				return ( false );
			}

			if (!File.Exists("OnlineSubsystemGamespy/OnlineSubsystemGamespy.vcxproj"))
			{
				return ( false );
			}

			return ( true );
		}

		public static bool SupportsOSSGameCenter()
		{
			// Check for command line/env var suppression
			if (!UE3BuildConfiguration.bAllowGameCenter)
			{
				return false;
			}

			if (!File.Exists("OnlineSubsystemGamecenter/OnlineSubsystemGamecenter.vcxproj"))
			{
				return false;
			}

			return true;
		}

		public static bool SupportsOSSLive()
		{
			// Check for command line/env var suppression
			if( !UE3BuildConfiguration.bAllowLive )
			{
				return ( false );
			}

			if (!File.Exists("OnlineSubsystemLive/OnlineSubsystemLive.vcxproj"))
			{
				return ( false );
			}

			string GFWLDir = Environment.GetEnvironmentVariable( "GFWLSDK_DIR" );
			if( ( GFWLDir == null ) || !Directory.Exists( GFWLDir.TrimEnd( "\\".ToCharArray() ) ) )
			{
				return ( false );
			}

			// Do the special env var checking for GFWL
			string WithPanoramaEnvVar = Environment.GetEnvironmentVariable( "WITH_PANORAMA" );
			if( WithPanoramaEnvVar != null )
			{
				bool Setting = false;
				try
				{
					Setting = Convert.ToBoolean( WithPanoramaEnvVar );
				}
				catch
				{
				}

				if( !Setting )
				{
					return ( false );
				}
			}

			return ( true );
		}

		/// <summary>
		/// Adds the paths and projects needed for Windows Live
		/// </summary>
		void SetUpPCEnvironment()
		{
			GlobalCPPEnvironment.IncludePaths.Add( "OnlineSubsystemPC/Inc" );
			NonGameProjects.Add(new UE3ProjectDesc("OnlineSubsystemPC/OnlineSubsystemPC.vcxproj"));
		}

		/// <summary>
		/// Adds the paths and projects needed for Steamworks
		/// </summary>
		void SetUpSteamworksEnvironment()
		{
			// Compile and link with Steamworks.
			GlobalCPPEnvironment.IncludePaths.Add("OnlineSubsystemSteamworks/Inc");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/Steamworks/sdk/public");
			if ( Platform == UnrealTargetPlatform.Win64 )
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/Steamworks/sdk/redistributable_bin/win64");
				FinalLinkEnvironment.AdditionalLibraries.Add("steam_api64.lib");
			}
			else if ( Platform == UnrealTargetPlatform.Win32 )
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/Steamworks/sdk/redistributable_bin");
				FinalLinkEnvironment.AdditionalLibraries.Add("steam_api.lib");
			}
			NonGameProjects.Add(new UE3ProjectDesc("OnlineSubsystemSteamworks/OnlineSubsystemSteamworks.vcxproj"));

			GlobalCPPEnvironment.Definitions.Add( "WITH_STEAMWORKS=1" );
		}

		/// <summary>
		/// Adds the paths and projects needed for GameSpy
		/// </summary>
		void SetUpGameSpyEnvironment()
		{
			GlobalCPPEnvironment.IncludePaths.Add("OnlineSubsystemGameSpy/Inc");

			// Compile and link with GameSpy.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/GameSpy");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/GameSpy/voice2/speex-1.0.5/include");
			GlobalCPPEnvironment.Definitions.Add( "GSI_UNICODE=1" );
			GlobalCPPEnvironment.Definitions.Add("SB_ICMP_SUPPORT=1");
			GlobalCPPEnvironment.Definitions.Add("_CRT_SECURE_NO_WARNINGS=1");
			GlobalCPPEnvironment.Definitions.Add("UNIQUEID=1");
			GlobalCPPEnvironment.Definitions.Add("DIRECTSOUND_VERSION=0x0900");
			NonGameProjects.Add(new UE3ProjectDesc("OnlineSubsystemGamespy/OnlineSubsystemGamespy.vcxproj"));

			GlobalCPPEnvironment.Definitions.Add( "WITH_GAMESPY=1" );
		}

		/// <summary>
		/// Adds the paths and projects needed for Windows Live
		/// </summary>
		void SetUpWindowsLiveEnvironment()
		{
			GlobalCPPEnvironment.IncludePaths.Add( "OnlineSubsystemLive/Inc" );

			GlobalCPPEnvironment.IncludePaths.Add( "$(GFWLSDK_DIR)include" );
			FinalLinkEnvironment.LibraryPaths.Add( "$(GFWLSDK_DIR)lib/x86" );
			NonGameProjects.Add(new UE3ProjectDesc("OnlineSubsystemLive/OnlineSubsystemLive.vcxproj"));

			GlobalCPPEnvironment.Definitions.Add( "WITH_PANORAMA=1" );
		}

		/// <summary>
		/// Adds the paths and projects needed for Game Center
		/// </summary>
		void SetUpGameCenterEnvironment()
		{
			GlobalCPPEnvironment.Definitions.Add("WITH_GAMECENTER=1");
			GlobalCPPEnvironment.IncludePaths.Add("OnlineSubsystemGameCenter/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("OnlineSubsystemGameCenter/OnlineSubsystemGameCenter.vcxproj"));
		}

        /// <summary>
        /// Adds the libraries needed for SpeedTree 5.0
        /// </summary>
        void SetUpSpeedTreeEnvironment()
        {
            if (UE3BuildConfiguration.bCompileSpeedTree && Directory.Exists("../External/SpeedTree/Lib"))
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_SPEEDTREE=1");
                GlobalCPPEnvironment.IncludePaths.Add("../External/SpeedTree/Include");

                string SpeedTreeLibrary = "SpeedTreeCore_";

				if (File.Exists("../External/SpeedTree/Lib/Windows/VC10/SpeedTreeCore_v5.2_VC100MTDLL_Static_d.lib"))
                {
                    SpeedTreeLibrary += "v5.2_";
                }
				else if (File.Exists("../External/SpeedTree/Lib/Windows/VC10/SpeedTreeCore_v5.1_VC100MTDLL_Static_d.lib"))
                {
                    SpeedTreeLibrary += "v5.1_";
                }
                else
                {
                    SpeedTreeLibrary += "v5.0_";
                }

                if (Platform == UnrealTargetPlatform.Win64)
                {
					FinalLinkEnvironment.LibraryPaths.Add("../External/SpeedTree/Lib/Windows/VC10.x64/");
					SpeedTreeLibrary += "VC100MTDLL64_Static";
                }
                else if (Platform == UnrealTargetPlatform.Win32)
                {
					FinalLinkEnvironment.LibraryPaths.Add("../External/SpeedTree/Lib/Windows/VC10/");
					SpeedTreeLibrary += "VC100MTDLL_Static";
                }

                if (Configuration == UnrealTargetConfiguration.Debug)
                {
                    SpeedTreeLibrary += "_d";
                }

                SpeedTreeLibrary += ".lib";

                FinalLinkEnvironment.AdditionalLibraries.Add(SpeedTreeLibrary);
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_SPEEDTREE=0");
            }
        }

		/// <summary>
        /// Adds the libraries needed for Trioviz
        /// </summary>
        void SetUpTriovizEnvironment()
        {
			// since UDK is released to the world, compiled by Epic, we need to disable it, just for UDK

			// @todo : Re-enable trioviz!
			UE3BuildConfiguration.bCompileTrioviz = false;

			// no need for trioviz when Null RHI is forced (either from dedicated server or from verification builds, etc)
			if (GlobalCPPEnvironment.Definitions.Contains("USE_NULL_RHI=1"))
			{
				UE3BuildConfiguration.bCompileTrioviz = false;
			}

			if (UE3BuildConfiguration.bCompileTrioviz && Directory.Exists("../External/DwTriovizSDK/Lib"))
            {
				// enable the code switch
				GlobalCPPEnvironment.Definitions.Add("DWTRIOVIZSDK=1");

				// point to the headers
				GlobalCPPEnvironment.IncludePaths.Add("../External/DwTriovizSDK/Include");
				

				// point to the appropriate lib
                string TriovizLibrary = "DwTriovizSDK";

				if (Configuration == UnrealTargetConfiguration.Debug)
				{
					TriovizLibrary += "_D";
				}
				else if (Configuration == UnrealTargetConfiguration.Release)
				{
					TriovizLibrary += "_P";
				}

				if (Platform == UnrealTargetPlatform.Win32)
				{
					TriovizLibrary += "_MT_2008.lib";
				}
				else if (Platform == UnrealTargetPlatform.Win64)
				{
					TriovizLibrary += "_MT_pack4_2008.lib";
				}

				// finally, add the library name to the list of libs
				FinalLinkEnvironment.AdditionalLibraries.Add(TriovizLibrary);


				// use the appropriate library path
                if (Platform == UnrealTargetPlatform.Win64)
                {
                    FinalLinkEnvironment.LibraryPaths.Add("../External/DwTriovizSDK/Lib/Win64");
                }
                else if (Platform == UnrealTargetPlatform.Win32)
                {
                    FinalLinkEnvironment.LibraryPaths.Add("../External/DwTriovizSDK/Lib/Win32");
                }
            }
            else
            {
				// if it's not supported, disable it in code
				GlobalCPPEnvironment.Definitions.Add("DWTRIOVIZSDK=0");
            }
        }

        /// <summary>
        /// Modifies the build environment to compile in support for Simplygon.
        /// </summary>
        void SetUpSimplygonEnvironment()
        {
            // Enable this to force the editor to link against the Simplygon DLL.
            const bool bForceSimplygonDLL = false;
 
            // Disable Simplygon support if compiling against the NULL RHI.
            if (GlobalCPPEnvironment.Definitions.Contains("USE_NULL_RHI=1"))
            {
                UE3BuildConfiguration.bCompileSimplygon = false;
            }

            if (UE3BuildConfiguration.bCompileSimplygon && Directory.Exists("../External/Simplygon"))
            {
                if (!bForceSimplygonDLL && Directory.Exists("../External/Simplygon/Lib"))
                {
                    string SimplygonLibrary = "SimplygonSDKEpicUE3StaticLibrary";

                    if (Configuration == UnrealTargetConfiguration.Debug)
                    {
                        SimplygonLibrary += "Debug";
                    }
                    else
                    {
                        SimplygonLibrary += "Release";
                    }

                    if (Platform == UnrealTargetPlatform.Win32)
                    {
                        SimplygonLibrary += "Win32.lib";
                    }
                    else if (Platform == UnrealTargetPlatform.Win64)
                    {
                        SimplygonLibrary += "x64.lib";
                    }
                    else
                    {
                        // Simplygon is a Win32/Win64 editor only feature.
                        return;
                    }

                    FinalLinkEnvironment.LibraryPaths.Add("../External/Simplygon/Lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add(SimplygonLibrary);
                }
                else
                {
                    GlobalCPPEnvironment.Definitions.Add("WITH_SIMPLYGON_DLL=1");
                }

                GlobalCPPEnvironment.Definitions.Add("WITH_SIMPLYGON=1");
                GlobalCPPEnvironment.IncludePaths.Add("../External/Simplygon/Inc");
            }
        }

        // This function is used for Windows
        void SetUpPhysXEnvironment()
		{
			// PhysX headers
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/Foundation/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/Physics/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/Physics/include/fluids");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/Physics/include/softbody");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/Physics/include/cloth");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/Cooking/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/PhysXLoader/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/PhysXExtensions/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/SDKs/TetraMaker/NxTetra");

			// Nxd headers
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/Nxd/include");

			// APEX and PhysX 3 shared headers
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX-3/Include");

			// APEX headers
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/shared/general/shared");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/public");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/framework/public");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/framework/public/PhysX2");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/NxParameterized/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/EditorWidgets");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/clothing/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/destructible/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/emitter/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/explosion/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/forcefield/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/iofx/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/basicios/public");
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/PhysX/APEX/module/particles/public");

            // If USE_DEBUG_NOVODEX is defined in UnNovodexSupport.h, then the PhysXLoaderDEBUG.dll should be added to the delay load list!
            bool UseDebugNovodex = false;

            if (Platform == UnrealTargetPlatform.Win64)
            {
                if (UseDebugNovodex && (Configuration == UnrealTargetConfiguration.Debug))
                {
                    FinalLinkEnvironment.DelayLoadDLLs.Add("PhysXLoader64DEBUG.dll");
                }
                else
                {
                    FinalLinkEnvironment.DelayLoadDLLs.Add("PhysXLoader64.dll");
                }
            }
 			else
 			{
                if (UseDebugNovodex && (Configuration == UnrealTargetConfiguration.Debug))
                {
                    FinalLinkEnvironment.DelayLoadDLLs.Add("PhysXLoaderDEBUG.dll");
                }
                else
                {
                    FinalLinkEnvironment.DelayLoadDLLs.Add("PhysXLoader.dll");
                }
			}
		}

		void SetUpFaceFXEnvironment()
		{
			// If FaceFX isn't available locally, disable it.
            if ( !UE3BuildConfiguration.bCompileFaceFX || !Directory.Exists( "../External/FaceFX" ) )
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_FACEFX=0");
			}
			else
			{
				// Compile and link with the FaceFX SDK on all platforms.
				GlobalCPPEnvironment.SystemIncludePaths.Add("../External/FaceFX/FxSDK/Inc");
				GlobalCPPEnvironment.SystemIncludePaths.Add("../External/FaceFX/FxCG/Inc");
				GlobalCPPEnvironment.SystemIncludePaths.Add("../External/FaceFX/FxAnalysis/Inc");

				// Only compile FaceFX studio on Windows and only if we want it to be compiled.
				if( UE3BuildConfiguration.bCompileFaceFXStudio 
					&& (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64 )
					&& UE3BuildConfiguration.bBuildEditor)
				{
					// Compile and link with FaceFX studio on Win32.
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Main/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Widgets/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Audio/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Commands/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Console/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/GUI/Inc");
                    GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Misc/Extensions/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Misc/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Proxies/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/Framework/Gestures/Inc");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/External/OpenAL/include");
					GlobalCPPEnvironment.IncludePaths.Add("../External/FaceFX/Studio/External/libresample-0.1.3/include");
                    GlobalCPPEnvironment.IncludePaths.Add("../External/tinyXML");
					NonGameProjects.Add(new UE3ProjectDesc("../External/FaceFX/Studio/Studio_vs10.vcxproj"));
				}
				else
				{
					GlobalCPPEnvironment.Definitions.Add("WITH_FACEFX_STUDIO=0");
				}
			}
		}

		bool SetUpBink1Environment()
		{
			bool bPlatformAllowed = 
					Platform != UnrealTargetPlatform.IPhone
				&&	Platform != UnrealTargetPlatform.AndroidARM
                &&  Platform != UnrealTargetPlatform.Androidx86
				&&	Platform != UnrealTargetPlatform.Flash
				&&	Platform != UnrealTargetPlatform.Mac;

			if( Directory.Exists( "../External/Bink" ) && bPlatformAllowed )
			{
				// Bink is allowed, add the proper include paths, but let UnBuild.h to specify USE_BINK_CODEC
				GlobalCPPEnvironment.SystemIncludePaths.Add( "../External/Bink" );
				UE3BuildConfiguration.BinkVersion = 1;
				return true;
			}

			return false;
		}

		bool SetUpBink2Environment()
		{
			bool bPlatformAllowed = 
					Platform != UnrealTargetPlatform.IPhone
				&&	Platform != UnrealTargetPlatform.AndroidARM
                &&  Platform != UnrealTargetPlatform.Androidx86
				&&	Platform != UnrealTargetPlatform.Flash;

			if( Directory.Exists( "../External/Bink2" ) && bPlatformAllowed )
			{
				// Bink is allowed, add the proper include paths, but let UnBuild.h to specify USE_BINK_CODEC
				GlobalCPPEnvironment.SystemIncludePaths.Add( "../External/Bink2" );
				GlobalCPPEnvironment.Definitions.Add( "IS_USING_BINK2=1" );
				UE3BuildConfiguration.BinkVersion = 2;
				return true;
			}

			return false;
		}

		void SetUpBinkEnvironment()
		{
			if( UE3BuildConfiguration.bCompileBink )
			{
				// @todo: could also be SetUpBink1Environment to use Bink2
				UE3BuildConfiguration.bCompileBink = SetUpBink1Environment();
			}

            if (!UE3BuildConfiguration.bCompileBink)
            {
                // Not using Bink
                GlobalCPPEnvironment.Definitions.Add("USE_BINK_CODEC=0");
                UE3BuildConfiguration.BinkVersion = 0;
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("USE_BINK_CODEC=1");
            }
		}

		void SetUpSourceControlEnvironment()
		{
			// Perforce can only be used under managed code
			if (UE3BuildConfiguration.bCompilePerforce && UE3BuildConfiguration.bBuildEditor && UE3BuildConfiguration.bAllowManagedCode)
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_PERFORCE=1");
				GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "../../Binaries/p4api.dll" );
			}
		}

		void SetUpRealDEnvironment()
		{
			if ((UE3BuildConfiguration.bCompileRealD) &&
				Directory.Exists("Engine/Src/RealD") &&
				File.Exists("Engine/Src/RealD/RealD.cpp") &&
				Directory.Exists("Engine/Inc/RealD") &&
				File.Exists("Engine/Inc/RealD/RealD.h") &&
				Directory.Exists("../../Engine/Shaders/RealD") &&
				File.Exists("../../Engine/Shaders/RealD/CommonDepth.usf")
				)

			{
				// NOTE: if you add a new platform you will also need to add it to the RealD change in the
				//       following files:
				//          - ShaderCompiler.cpp: BeginCompileShader() function
				//          - UnBuild.h: WITH_REALD macro definition
				// It is being disabled by default.
				if (Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64)
				{
					Console.WriteLine("Building with RealD...");
					GlobalCPPEnvironment.Definitions.Add("WITH_REALD=1");
				}
			}
		}

		string GFxDir;
		string GFxLocalDir;

		void SetUpGFxEnvironment()
		{
			bool bPlatformAllowed =
					Platform != UnrealTargetPlatform.Flash
				&&	Platform != UnrealTargetPlatform.AndroidARM
                &&  Platform != UnrealTargetPlatform.Androidx86;

			if (bPlatformAllowed && UE3BuildConfiguration.bCompileScaleform)
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx=1");
				// Audio/video support is disabled by default
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx_AUDIO=0");
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx_VIDEO=0");

				bool bIMEPlatformAllowed =
						Platform == UnrealTargetPlatform.Win32
					|| Platform == UnrealTargetPlatform.Win64;

				if (bIMEPlatformAllowed)
				{
					GlobalCPPEnvironment.Definitions.Add("WITH_GFx_IME=1");
				}
				else
				{
					GlobalCPPEnvironment.Definitions.Add("WITH_GFx_IME=0");
				}
			}
			else
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx=0");
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx_IME=0");
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx_AUDIO=0");
				GlobalCPPEnvironment.Definitions.Add("WITH_GFx_VIDEO=0");
			}

			// We always need to include Scaleform GFx headers!
			// When needed, GFx will be compiled out in the code via WITH_GFx=0.
			{
				GFxDir = "../External/GFx";
				GFxLocalDir = GFxDir;

				// Scaleform headers
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Include");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/AMP");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/AS2");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/AS3");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/Audio");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/IME");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/Text");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/GFx/XML");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/Kernel");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/Render");
				GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/Src/Render/ImageFiles");
			}

			if (Configuration == UnrealTargetConfiguration.Debug)
			{
				GlobalCPPEnvironment.Definitions.Add("SF_BUILD_DEBUG=1");
			}
			else if (Configuration == UnrealTargetConfiguration.Shipping)
			{
				GlobalCPPEnvironment.Definitions.Add("SF_BUILD_SHIPPING=1");
			}
		}

		void SetUpRecastEnvironment()
		{
			if (UE3BuildConfiguration.bCompileRecast &&
				(Platform == UnrealTargetPlatform.Win32 || Platform == UnrealTargetPlatform.Win64))
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_RECAST=1");
				GlobalCPPEnvironment.SystemIncludePaths.Add("../External/Recast/UE3/include");

				FinalLinkEnvironment.LibraryPaths.Add("../External/Recast/UE3/lib");

				string RecastLibName = "recast";
				if (Platform == UnrealTargetPlatform.Win64)
				{
					RecastLibName += "_x64";
				}

				if (Configuration == UnrealTargetConfiguration.Debug)
				{
					RecastLibName += "d";
				}

				FinalLinkEnvironment.AdditionalLibraries.Add(RecastLibName + ".lib");
			}
		}

		// Allegorithmic Substance Air Integration
        void SetUpSubstanceAir()
        {
            if (!UE3BuildConfiguration.bCompileSubstanceAir || 
                !Directory.Exists("../External/Substance/Framework") || 
                (Platform != UnrealTargetPlatform.Win32 && Platform != UnrealTargetPlatform.Win64) )
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_SUBSTANCE_AIR=0");
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_SUBSTANCE_AIR=1");

                // compile and link with substance
                string SubstanceAirPlatform = null;
                string SubstanceAirPath = "../External/Substance/Framework/include";

                // default to sse2 engine
                if (GlobalCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win32 ||
                    GlobalCPPEnvironment.TargetPlatform == CPPTargetPlatform.Win64)
                {
                    SubstanceAirPlatform = "SUBSTANCE_PLATFORM_BLEND";
                }       
                else
                {
                    throw new BuildException(
                        "The Substance integration only support Windows for the moment.\n");
                }

                GlobalCPPEnvironment.Definitions.Add(SubstanceAirPlatform);
                GlobalCPPEnvironment.IncludePaths.Add(SubstanceAirPath);

                GlobalCPPEnvironment.IncludePaths.Add(
                    "../External/Substance/AirTextureCache/include");
            }
        }
        // end Allegorithmic Substance Integration
	}
}
