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
		bool MacSupportEnabled()
		{
			return true;
		}

		void SetUpMacEnvironment()
		{
			GlobalCPPEnvironment.Definitions.Add("MAC=1");
			GlobalCPPEnvironment.Definitions.Add("__MACOSX__");
			GlobalCPPEnvironment.Definitions.Add("PLATFORM_MACOSX=1");
			GlobalCPPEnvironment.Definitions.Add("EXCEPTIONS_DISABLED=1");

            GlobalCPPEnvironment.Definitions.Add("WITH_TTS=0");
            GlobalCPPEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");

			// static libs
			FinalLinkEnvironment.AdditionalLibraries.Add("z"); // ZLib

			// PhysX
			string PhysXLibPath = "../External/PhysX/SDKs/lib/osxstatic/";
			FinalLinkEnvironment.LibraryPaths.Add(PhysXLibPath);
			FinalLinkEnvironment.AdditionalLibraries.Add("PhysXCooking");
			FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysXCooking.a");
			FinalLinkEnvironment.AdditionalLibraries.Add("PhysXCore");
			FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysXCore.a");
			FinalLinkEnvironment.AdditionalLibraries.Add("PhysXExtensions");
			FinalLinkEnvironment.AdditionalShadowFiles.Add(PhysXLibPath + "libPhysXExtensions.a");
			GlobalCPPEnvironment.Definitions.Add("NX64=1");

			// GFx
			if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx=0"))
			{
				string GFxLibDir;
				switch (Configuration)
				{
					case UnrealTargetConfiguration.Debug:
						GFxLibDir = "/Lib/MacOS-x86_64/Debug_NoRTTI";
						break;
					case UnrealTargetConfiguration.Shipping:
						GFxLibDir = "/Lib/MacOS-x86_64/Shipping_NoRTTI";
						break;
					default:
						GFxLibDir = "/Lib/MacOS-x86_64/Release_NoRTTI";
						break;
				}
				FinalLinkEnvironment.LibraryPaths.Add(GFxDir + GFxLibDir);
				FinalLinkEnvironment.AdditionalLibraries.Add("gfx");
				FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as2");
				FinalLinkEnvironment.AdditionalLibraries.Add("gfx_as3");

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

			if (UE3BuildConfiguration.bCompileSpeedTree && Directory.Exists("../External/SpeedTree/Lib"))
			{
				// SpeedTree
				GlobalCPPEnvironment.Definitions.Add("WITH_SPEEDTREE=1");
				GlobalCPPEnvironment.IncludePaths.Add("../External/SpeedTree/Include");
				string SpeedTreeLibPath = "../External/SpeedTree/Lib/MacOSX/";
				FinalLinkEnvironment.LibraryPaths.Add(SpeedTreeLibPath);
				string SpeedTreeLibName = "SpeedTreeCore_v5.0_Static";
				if (Configuration == UnrealTargetConfiguration.Debug)
				{
					SpeedTreeLibName += "_d";
				}
				FinalLinkEnvironment.AdditionalLibraries.Add(SpeedTreeLibName);
				FinalLinkEnvironment.AdditionalShadowFiles.Add(SpeedTreeLibPath + "lib" + SpeedTreeLibName + ".a");
			}

			// FaceFX
			string FaceFXLibPath = "../External/FaceFX/FxSDK/lib/Mac/";
			FinalLinkEnvironment.LibraryPaths.Add(FaceFXLibPath);
			FinalLinkEnvironment.AdditionalLibraries.Add("FxSDK_Unreal");
			FinalLinkEnvironment.AdditionalShadowFiles.Add(FaceFXLibPath + "libFxSDK_Unreal.a");

			GlobalCPPEnvironment.IncludePaths.Add("CoreAudio/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("CoreAudio/CoreAudio.vcxproj"));

			GlobalCPPEnvironment.IncludePaths.Add("OpenGLDrv/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("OpenGLDrv/OpenGLDrv.vcxproj"));

			GlobalCPPEnvironment.IncludePaths.Add("Mac");
			GlobalCPPEnvironment.IncludePaths.Add("Mac/Inc");
			NonGameProjects.Add(new UE3ProjectDesc("Mac/Mac.vcxproj"));

			FinalLinkEnvironment.AdditionalShadowFiles.Add("../../Binaries/Mac/CreateAppBundle.sh");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/" + Game.GetGameName() + ".icns");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/" + Game.GetGameName() + "-Info.plist");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/English.lproj/InfoPlist.strings");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/English.lproj/MainMenu.xib");
            FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/RadioEffectUnit.component/Contents/MacOS/RadioEffectUnit");
            FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/RadioEffectUnit.component/Contents/Resources/English.lproj/Localizable.strings");
            FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/RadioEffectUnit.component/Contents/Resources/RadioEffectUnit.rsrc");
            FinalLinkEnvironment.AdditionalShadowFiles.Add("Mac/Resources/RadioEffectUnit.component/Contents/Info.plist");

			// Compile and link with Ogg/Vorbis.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libogg-1.2.2/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libvorbis-1.3.2/include");
			FinalLinkEnvironment.LibraryPaths.Add("../External/libogg-1.2.2/macosx/");
			FinalLinkEnvironment.LibraryPaths.Add("../External/libvorbis-1.3.2/macosx/");
			FinalLinkEnvironment.AdditionalLibraries.Add("ogg");
			FinalLinkEnvironment.AdditionalLibraries.Add("vorbis");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("../External/libogg-1.2.2/macosx/libogg.dylib");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("../External/libvorbis-1.3.2/macosx/libvorbis.dylib");

			// Compile and link with lzo.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/lzopro/include");
			FinalLinkEnvironment.LibraryPaths.Add("../External/lzopro/lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("lzopro");
			FinalLinkEnvironment.AdditionalShadowFiles.Add("../External/lzopro/lib/liblzopro.a");

			// Additional include dirs to make sure all files are copied over to the Mac
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libogg-1.2.2/include/ogg");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libvorbis-1.3.2/include/vorbis");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/lzopro/include/lzo");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/lzopro/include/lzo/lzopro");
			GlobalCPPEnvironment.IncludePaths.Add("GFxUI/Src");
			GlobalCPPEnvironment.IncludePaths.Add("Launch/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("OpenGLDrv/Src");

			// frameworks
            FinalLinkEnvironment.Frameworks.Add("AudioToolbox");
            FinalLinkEnvironment.Frameworks.Add("AudioUnit");
			FinalLinkEnvironment.Frameworks.Add("Cocoa");
			FinalLinkEnvironment.Frameworks.Add("CoreAudio");
			FinalLinkEnvironment.Frameworks.Add("OpenGL");
			FinalLinkEnvironment.Frameworks.Add("IOKit");
			FinalLinkEnvironment.Frameworks.Add("Security");

			// compile an OnlineSubsystem
			string DesiredOSS = Game.GetDesiredOnlineSubsystem(GlobalCPPEnvironment, Platform);

			if (DesiredOSS == "PC")
			{
				SetUpPCEnvironment();
			}
			else if (DesiredOSS == "GameSpy")
			{
				SetUpGameSpyEnvironment();
			}
			else if (DesiredOSS == "Steamworks")
			{
				SetUpSteamworksEnvironment();
			}
			else
			{
				throw new BuildException("Requested OnlineSubsystem{0}, but that is not supported on Mac (only OSSPC)", DesiredOSS);
			}
		}

		List<FileItem> GetMacOutputItems()
		{
			if (OutputPath == null)
			{
				OutputPath = string.Format("../../Binaries/Mac/{0}-Mac-{1}",
					Game.GetGameName(), Configuration.ToString());
			}

			XcodeMacToolChain.GameName = Game.GetGameName();

			FinalLinkEnvironment.OutputFilePath = OutputPath;
			FileItem MainOutputItem = FinalLinkEnvironment.LinkExecutable();

			if (BuildConfiguration.bGeneratedSYMFile == true)
			{
				// If requested, generate the dSYM file
				Console.WriteLine( "Generating the dSYM file - this will add some time to your build..." );
				MainOutputItem = MacToolChain.GenerateDebugInfo(MainOutputItem);
			}

			if( !BuildConfiguration.bUseXcodeMacToolchain )
			{
				// For the non-Xcode path, manually build the application bundle
				MainOutputItem = MacToolChain.CreateAppBundle(MainOutputItem, Game.GetGameName());
			}

			// Return a list of the output files that require work on the Mac
			List<FileItem> OutputFiles = new List<FileItem>()
			{
				MainOutputItem
			};

			return OutputFiles;
		}
	}
}
