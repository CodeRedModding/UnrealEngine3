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
		void SetUpAndroidEnvironment()
        {
			AndroidToolChain.ClearSuppressedLog();

			AndroidToolChain.InitializeEnvironmentVariables();

            // Disable a variety of subsystems, common to simulator and device
            GlobalCPPEnvironment.Definitions.Add("WITH_FACEFX=0");
            GlobalCPPEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
            GlobalCPPEnvironment.Definitions.Add("USE_NETWORK_PROFILER=0");
            GlobalCPPEnvironment.Definitions.Add("WITH_TTS=0");
            GlobalCPPEnvironment.Definitions.Add("WITH_SPEEDTREE=0");
            GlobalCPPEnvironment.Definitions.Add("WITH_LZO=0");
            GlobalCPPEnvironment.Definitions.Add("WITH_OGGVORBIS=0");
            GlobalCPPEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=1");
            GlobalCPPEnvironment.Definitions.Add("WITH_EDITOR=0");

            GlobalCPPEnvironment.Definitions.Add("ANDROID=1");
            GlobalCPPEnvironment.Definitions.Add("__ARM_ARCH_7__=1");
            GlobalCPPEnvironment.Definitions.Add("__ARM_ARCH_7A__=1");
            GlobalCPPEnvironment.Definitions.Add("__ARM_ARCH_7R__=1");
            GlobalCPPEnvironment.Definitions.Add("__ARM_ARCH_7M__=1");
            GlobalCPPEnvironment.Definitions.Add("NDEBUG=1");
			GlobalCPPEnvironment.Definitions.Add("USE_STATIC_ES2_RHI=1");


            // Standard UE3 include directories for Android
            GlobalCPPEnvironment.IncludePaths.Add("Android");
            GlobalCPPEnvironment.IncludePaths.Add("Android/Inc");
            NonGameProjects.Add(new UE3ProjectDesc("Android/Android.vcxproj"));

            // Include ES2 support for rendering
            GlobalCPPEnvironment.SystemIncludePaths.Add("ES2Drv/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("ES2Drv/ES2Drv.vcxproj"));

            // compile an OnlineSubsystem
            string DesiredOSS = Game.GetDesiredOnlineSubsystem(GlobalCPPEnvironment, Platform);
            if (DesiredOSS == "PC")
            {
                SetUpPCEnvironment();
            }
            else
            {
                throw new BuildException("Requested OnlineSubsystem{0}, but that is not supported on iPhone (only OSSPC)", DesiredOSS);
            }

            if (Platform == UnrealTargetPlatform.Androidx86)
            {
                GlobalCPPEnvironment.Definitions.Add("ANDROID_X86=1");
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("ARM=1");
                GlobalCPPEnvironment.Definitions.Add("_ARM_=1");
            }

			// Android platform includes and libraries
			GlobalCPPEnvironment.IncludePaths.Add("$(NDKROOT)/sources/cxx-stl/system/include");
            GlobalCPPEnvironment.IncludePaths.Add("$(NDKROOT)/sources/cxx-stl/stlport/stlport");
            GlobalCPPEnvironment.IncludePaths.Add("$(NDKROOT)/sources/android/cpufeatures");
            if (Platform == UnrealTargetPlatform.Androidx86)
            {
                GlobalCPPEnvironment.IncludePaths.Add("$(NDKROOT)/platforms/android-9/arch-x86/usr/include");
                FinalLinkEnvironment.LibraryPaths.Add("$(NDKROOT)/platforms/android-9/arch-x86/usr/lib");
            }
            else
            {
                GlobalCPPEnvironment.IncludePaths.Add("$(NDKROOT)/platforms/android-9/arch-arm/usr/include");
                FinalLinkEnvironment.LibraryPaths.Add("$(NDKROOT)/platforms/android-9/arch-arm/usr/lib");
            }

            // This is a fix for building using NDK versions newer than 6b, which will otherwise break from undefined __dso_handle symbols
            if (Platform != UnrealTargetPlatform.Androidx86)
            {
                FinalLinkEnvironment.InputFiles.Add(FileItem.GetItemByPath(Environment.GetEnvironmentVariable("NDKROOT") + "/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/lib/gcc/arm-linux-androideabi/4.4.3/armv7-a/crtbeginS.o"));
                FinalLinkEnvironment.InputFiles.Add(FileItem.GetItemByPath(Environment.GetEnvironmentVariable("NDKROOT") + "/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/lib/gcc/arm-linux-androideabi/4.4.3/armv7-a/crtendS.o"));
            }

            // Apsalar Analytics
            string ApsalarLibPath = "../External/NoRedist/Apsalar/Android/";
            if (!Directory.Exists(ApsalarLibPath))
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_APSALAR=0");
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_APSALAR=1");
            }

			// PhysX
			bool bCompileWithPhysX = UE3BuildConfiguration.bCompilePhysXWithMobile;
            if (bCompileWithPhysX)
			{
				string PhysXLibPath = "../External/PhysX/SDKs/lib/android/";

                // Correct path if x86
                if (Platform == UnrealTargetPlatform.Androidx86)
                {
                    PhysXLibPath = "../External/PhysX/SDKs/lib/androidx86/";
                }

				FinalLinkEnvironment.LibraryPaths.Add(PhysXLibPath);

				// @todo: Add debug PhysX libraries
				FinalLinkEnvironment.AdditionalLibraries.Add("PhysXCore");
				FinalLinkEnvironment.AdditionalLibraries.Add("PhysXCooking");
	
				// Android always uses 32-bit PhysX
				GlobalCPPEnvironment.Definitions.Add( "NX32=1" );
			}
			else
			{
				GlobalCPPEnvironment.Definitions.Add( "WITH_NOVODEX=0" );
			}

            // Link with GFx
            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx=0"))
            {
                string GFxLibDir;

                switch (Configuration)
                {
                    case UnrealTargetConfiguration.Debug:
                        GFxLibDir = "/Lib/Android/Debug_NoRTTI";
                        break;
                    case UnrealTargetConfiguration.Shipping:
                        GFxLibDir = "/Lib/Android/Shipping_NoRTTI";
                        break;
                    default:
                        GFxLibDir = "/Lib/Android/Release_NoRTTI";
                        break;
                }
                FinalLinkEnvironment.LibraryPaths.Add(GFxDir + GFxLibDir);

                // Do not remove the duplicates, they are needed
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as2");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as3");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as2");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as3");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as2");
                FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as3");

                if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx_AUDIO=0"))
                {
                    GlobalCPPEnvironment.Definitions.Add("GFX_SOUND_FMOD=1");
                    GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/3rdParty/fmod/Android/inc");
                    FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/fmod/Android/lib");

                    FinalLinkEnvironment.AdditionalLibraries.Add("gfxsound_fmod");
                    FinalLinkEnvironment.AdditionalLibraries.Add("fmodex");
                }
            }

            // Compile and link with libPNG
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libPNG");
            GlobalCPPEnvironment.Definitions.Add("PNG_NO_FLOATING_POINT_SUPPORTED=1");

            // Compile and link with zlib
            FinalLinkEnvironment.AdditionalLibraries.Add("z");
            FinalLinkEnvironment.AdditionalLibraries.Add("stdc++");
            FinalLinkEnvironment.AdditionalLibraries.Add("c");
            FinalLinkEnvironment.AdditionalLibraries.Add("m");
            FinalLinkEnvironment.AdditionalLibraries.Add("log");
            FinalLinkEnvironment.AdditionalLibraries.Add("dl");
            FinalLinkEnvironment.AdditionalLibraries.Add("GLESv2");
            FinalLinkEnvironment.AdditionalLibraries.Add("EGL");

            FinalLinkEnvironment.AdditionalLibraries.Add("android");

			// Add the game build directory to pick up hashes
			GlobalCPPEnvironment.IncludePaths.Add( "../../" + Game.GetGameName() + "/Build" );
		}

		
		List<FileItem> GetAndroidOutputItems()
		{
			// Verify that the user has specified the expected output extension.
			if( Path.GetExtension( OutputPath ).ToUpperInvariant() != ".SO")
			{
				throw new BuildException("Unexpected output extension: {0} instead of .SO", Path.GetExtension(OutputPath));
			}

			// Put the non-executable output files (PDB, import library, etc) in the same directory as the executables
			FinalLinkEnvironment.OutputDirectory = Path.GetDirectoryName(OutputPath);
			FinalLinkEnvironment.OutputFilePath = OutputPath;

			// Link the SO file.
			FileItem SOFile = FinalLinkEnvironment.LinkExecutable();

			// Build the APK file
			string ApkOutputFilePath = Path.Combine( Path.GetDirectoryName( OutputPath ), Path.GetFileNameWithoutExtension( OutputPath ) + ".apk" );
			FileItem APKFile = AndroidToolChain.BuildAPK( SOFile, FinalLinkEnvironment, Configuration, Game.GetGameName(), ApkOutputFilePath );

            // Return a list of the output files.
			List<FileItem> OutputFiles = new List<FileItem>();
			OutputFiles.Add( APKFile /*SOFile*/ );
			
			return OutputFiles;
		}
	}
}
