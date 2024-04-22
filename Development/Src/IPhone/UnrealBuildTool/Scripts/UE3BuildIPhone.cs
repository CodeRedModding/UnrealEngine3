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
		bool IPhoneSupportEnabled()
		{
			return true;
		}

		void SetUpIPhoneEnvironment()
		{
            if (Game.GetGameName() == "ExoGame")
            {
                GlobalCPPEnvironment.Definitions.Add("EXPERIMENTAL_FAST_BOOT_IPHONE=0");
            }

			GlobalCPPEnvironment.Definitions.Add("IPHONE=1");
			GlobalCPPEnvironment.Definitions.Add("WITH_FACEFX=0");
			GlobalCPPEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
			GlobalCPPEnvironment.Definitions.Add("USE_NETWORK_PROFILER=0");
			GlobalCPPEnvironment.Definitions.Add("WITH_TTS=0");
			GlobalCPPEnvironment.Definitions.Add("WITH_SPEEDTREE=0");
			GlobalCPPEnvironment.Definitions.Add("WITH_LZO=0");
			GlobalCPPEnvironment.Definitions.Add("WITH_OGGVORBIS=0");
			GlobalCPPEnvironment.Definitions.Add("WITH_GAMESPY=0");
			GlobalCPPEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=1");
			GlobalCPPEnvironment.Definitions.Add("USE_STATIC_ES2_RHI=1");

			//Detailed IPhone Memory Tracking
			if (Configuration == UnrealTargetConfiguration.Debug || Configuration == UnrealTargetConfiguration.Release)
			{
				GlobalCPPEnvironment.Definitions.Add("USE_DETAILED_IPHONE_MEM_TRACKING=1");
			}

			// @todo remove this once all C++ code that uses it is fixed
			GlobalCPPEnvironment.Definitions.Add("WITH_IOS_5=1");

			// frameworks
			FinalLinkEnvironment.Frameworks.Add("QuartzCore");
			FinalLinkEnvironment.Frameworks.Add("OpenGLES");
			FinalLinkEnvironment.Frameworks.Add("UIKit");
			FinalLinkEnvironment.Frameworks.Add("Foundation");
			FinalLinkEnvironment.Frameworks.Add("CoreGraphics");
			FinalLinkEnvironment.Frameworks.Add("AVFoundation");
			FinalLinkEnvironment.Frameworks.Add("SystemConfiguration");
			FinalLinkEnvironment.Frameworks.Add("MediaPlayer");
			FinalLinkEnvironment.Frameworks.Add("AudioToolbox");
			FinalLinkEnvironment.Frameworks.Add("StoreKit");
			FinalLinkEnvironment.Frameworks.Add("CoreMedia");
			FinalLinkEnvironment.WeakFrameworks.Add("GameKit");
			FinalLinkEnvironment.WeakFrameworks.Add("CoreMotion");
			FinalLinkEnvironment.WeakFrameworks.Add("iAd");
			FinalLinkEnvironment.WeakFrameworks.Add("Twitter");
			FinalLinkEnvironment.WeakFrameworks.Add("Accounts");
            FinalLinkEnvironment.WeakFrameworks.Add("CoreTelephony");
            FinalLinkEnvironment.WeakFrameworks.Add("MessageUI");

			// static libs
			FinalLinkEnvironment.AdditionalLibraries.Add("z"); // ZLib

            // Apsalar Analytics
            string ApsalarLibPath = "../External/NoRedist/Apsalar/IPhone/";
            if (!Directory.Exists(ApsalarLibPath))
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_APSALAR=0");
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_APSALAR=1");
                FinalLinkEnvironment.AdditionalLibraries.Add("Apsalar");
                FinalLinkEnvironment.LibraryPaths.Add(ApsalarLibPath);
                GlobalCPPEnvironment.IncludePaths.Add(ApsalarLibPath);
                FinalLinkEnvironment.AdditionalShadowFiles.Add(ApsalarLibPath + "libApsalar.a");
                // Apsalar uses sqlite 3.0 for storing stats locally
                FinalLinkEnvironment.AdditionalLibraries.Add("sqlite3");
                FinalLinkEnvironment.Frameworks.Add("Security");
				FinalLinkEnvironment.WeakFrameworks.Add("AdSupport");
			}

			// Swrve Analytics
			string SwrveLibPath = "../External/NoRedist/Swrve/";
			if (!Directory.Exists(SwrveLibPath))
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_SWRVE=0");
			}
			else
			{
				GlobalCPPEnvironment.Definitions.Add("WITH_SWRVE=1");
				GlobalCPPEnvironment.IncludePaths.Add(SwrveLibPath);
				FinalLinkEnvironment.Frameworks.Add("CFNetwork");
			}

			// PhysX
            bool bCompileWithPhysX = Game.ShouldCompilePhysXMobile();
			if (bCompileWithPhysX)
			{
				string PhysXLibPath = "../External/PhysX/SDKs/lib/ios/";
				FinalLinkEnvironment.LibraryPaths.Add(PhysXLibPath);
				if (Configuration == UnrealTargetConfiguration.Debug)
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("PhysX_ios_static_DEBUG");
					FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysX_ios_static_DEBUG.a");

					FinalLinkEnvironment.AdditionalLibraries.Add("PhysX_Cooking_ios_static_DEBUG");
					FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysX_Cooking_ios_static_DEBUG.a");
				}
				else
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("PhysX_ios_static");
					FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysX_ios_static.a");

					FinalLinkEnvironment.AdditionalLibraries.Add("PhysX_Cooking_ios_static");
					FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysX_Cooking_ios_static.a");
				}

                GlobalCPPEnvironment.Definitions.Add("NX32=1");
			}
			else
			{
				GlobalCPPEnvironment.Definitions.Add( "WITH_NOVODEX=0" );
			}

            // Link with GFx
            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx=0"))
            {
                UnrealTargetConfiguration ForcedConfiguration = Configuration;
                string GFxLibDir;
                /*if (UE3BuildConfiguration.bForceScaleformRelease && Configuration == UnrealTargetConfiguration.Debug)
                {
                    ForcedConfiguration = UnrealTargetConfiguration.Release;
                }*/
                switch (ForcedConfiguration)
                {
                    case UnrealTargetConfiguration.Debug:
                        GFxLibDir = "/Lib/iPhone-armv7/Debug_NoRTTI";
                        break;
                    case UnrealTargetConfiguration.Shipping:
                        GFxLibDir = "/Lib/iPhone-armv7/Shipping_NoRTTI";
                        break;
                    default:
                        GFxLibDir = "/Lib/iPhone-armv7/Release_NoRTTI";
                        break;
                }

                // TEMP: force to shipping libs to avoid crash in AMP restore (TTP 236010)
                if (GlobalCPPEnvironment.Definitions.Contains("FORCE_SCALEFORM_SHIPPING=1"))
                {
                    GFxLibDir = "/Lib/iPhone-armv7/Shipping_NoRTTI";
                }

                FinalLinkEnvironment.LibraryPaths.Add(GFxDir + GFxLibDir);
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as2");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as3");

	            GlobalCPPEnvironment.IncludePaths.Add("GFxUI/Src");

                if (GFxDir == GFxLocalDir)
                {
                    FinalLinkEnvironment.AdditionalShadowFiles.Add(GFxLocalDir + GFxLibDir + "/libgfx.a");
                    FinalLinkEnvironment.AdditionalShadowFiles.Add(GFxLocalDir + GFxLibDir + "/libgfx_as2.a");
                    FinalLinkEnvironment.AdditionalShadowFiles.Add(GFxLocalDir + GFxLibDir + "/libgfx_as3.a");

                    // Make sure that all GFx headers are pushed to the remote compiling server
                    string[] AllGFxHeaders = Directory.GetFiles(GFxLocalDir, "*.h", SearchOption.AllDirectories);
                    foreach (string NextGFxHeader in AllGFxHeaders)
                    {
                        FinalLinkEnvironment.AdditionalShadowFiles.Add(NextGFxHeader);
                    }
                }
            }

			// Add the IPhone-specific projects.
			GlobalCPPEnvironment.IncludePaths.Add("ES2Drv/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("ES2Drv/ES2Drv.vcxproj"));

			GlobalCPPEnvironment.IncludePaths.Add("IPhone");
			GlobalCPPEnvironment.IncludePaths.Add("IPhone/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("IPhone/IPhone.vcxproj"));

			GlobalCPPEnvironment.IncludePaths.Add("../External/Facebook/IPhone/src");
			GlobalCPPEnvironment.IncludePaths.Add("../External/Facebook/IPhone/src/JSON");

            // Compile and link with JPEG decoding libs
            GlobalCPPEnvironment.IncludePaths.Add("../External/libJPG");
            GlobalCPPEnvironment.Definitions.Add("WITH_JPEG=1");

			// compile an OnlineSubsystem
			string DesiredOSS = Game.GetDesiredOnlineSubsystem( GlobalCPPEnvironment, Platform );

			if (DesiredOSS == "PC")
			{
				SetUpPCEnvironment();
			}
			else  if (DesiredOSS == "GameCenter")
			{
				SetUpGameCenterEnvironment();
			}
			else
			{
				throw new BuildException("Requested OnlineSubsystem{0}, but that is not supported on iPhone (only OSSPC)", DesiredOSS);
			}

			// Add the game build directory to pick up hashes
			GlobalCPPEnvironment.IncludePaths.Add( "../../" + Game.GetGameName() + "/Build" );

		}

		List<FileItem> GetIPhoneOutputItems()
		{
			if (OutputPath == null)
			{
				OutputPath = string.Format("../../Binaries/IPhone/{0}-IPhone-{1}",
					Game.GetGameName(), Configuration.ToString() );
			}
			FinalLinkEnvironment.OutputFilePath = OutputPath;
			FileItem MainOutputItem = FinalLinkEnvironment.LinkExecutable();

			// For iPhone, generate the dSYM file if the config file is set to do so
			if (BuildConfiguration.bGeneratedSYMFile == true)
			{
				Console.WriteLine("Generating the dSYM file - this will add some time to your build...");
				MainOutputItem = IPhoneToolChain.GenerateDebugInfo(MainOutputItem);
			}

            //@TODO: Move copy-back-from-Mac to be an action, so this can depend on it
            /*
            if (BuildConfiguration.bCreateStubIPA)
            {
                MainOutputItem = IPhoneToolChain.AddCreateIPAAction(MainOutputItem, Game.GetGameName(), Configuration.ToString());
            }
            */

            // Return a list of the output files that require work on the Mac
            List<FileItem> OutputFiles = new List<FileItem>()
			{
				MainOutputItem
			};

            return OutputFiles;
        }
	}
}
