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
		void AddPrivateAssembly(string Dir, string SubDir, string Name)
		{
			PrivateAssemblyInfo NewPrivateAssembly = new PrivateAssemblyInfo();
			NewPrivateAssembly.FileItem = FileItem.GetItemByPath(Dir + Name);
			NewPrivateAssembly.bShouldCopyToOutputDirectory = false;
			NewPrivateAssembly.DestinationSubDirectory = SubDir;
			GlobalCPPEnvironment.PrivateAssemblyDependencies.Add(NewPrivateAssembly);
		}

		void SetUpWindowsEnvironment()
        {            
            GlobalCPPEnvironment.Definitions.Add("_WINDOWS=1");
			GlobalCPPEnvironment.Definitions.Add("WIN32=1");
			GlobalCPPEnvironment.Definitions.Add("_WIN32_WINNT=0x0502");
			GlobalCPPEnvironment.Definitions.Add("WINVER=0x0502");

			// If compiling as a dll, set the relevant defines
			if( BuildConfiguration.bCompileAsdll )
			{
				GlobalCPPEnvironment.Definitions.Add( "_WINDLL" );
			}

			// Add the Windows public module include paths.
			GlobalCPPEnvironment.IncludePaths.Add("OnlineSubsystemPC/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("WinDrv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("D3D9Drv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("D3D11Drv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("OpenGLDrv/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("XAudio2/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("Launch/Inc");
			GlobalCPPEnvironment.IncludePaths.Add("../Tools/UnrealLightmass/Public");
			GlobalCPPEnvironment.IncludePaths.Add("$(CommonProgramFiles)");

			if (Platform == UnrealTargetPlatform.Win64)
			{
				GlobalCPPEnvironment.IncludePaths.Add("../External/IntelTBB-3.0/Include");
				FinalLinkEnvironment.LibraryPaths.Add("../External/IntelTBB-3.0/lib/intel64/vc10");
			}

			string DesiredOSS = Game.GetDesiredOnlineSubsystem( GlobalCPPEnvironment, Platform );

			switch( DesiredOSS )
			{
			case "PC":
				SetUpPCEnvironment();
				break;

			case "Steamworks":
				SetUpSteamworksEnvironment();
				break;

			case "GameSpy":
				SetUpGameSpyEnvironment();
				break;

			case "Live":
				SetUpWindowsLiveEnvironment();
				break;

			default:
				throw new BuildException( "Requested OnlineSubsystem{0}, but that is not supported on PC", DesiredOSS );
			}

            // Compile and link for dedicated server
            SetUpDedicatedServerEnvironment();

			// Compile and link with libPNG
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libPNG");
			GlobalCPPEnvironment.Definitions.Add("PNG_NO_FLOATING_POINT_SUPPORTED=1");
			
            // Compile and link with JPEG decoding libs
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libJPG");
            GlobalCPPEnvironment.Definitions.Add("WITH_JPEG=1");
			
			// Compile and link with DirectShow
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/DirectShow/DirectShow");
            FinalLinkEnvironment.LibraryPaths.Add("../External/DirectShow/Lib");
			if (Platform == UnrealTargetPlatform.Win64)
			{
				if ( Configuration == UnrealTargetConfiguration.Debug )
				{
                    FinalLinkEnvironment.AdditionalLibraries.Add("DirectShowDEBUGx64.lib");
				}
				else
				{
                    FinalLinkEnvironment.AdditionalLibraries.Add("DirectShowx64.lib");
				}
			}
			else
			{
				if ( Configuration == UnrealTargetConfiguration.Debug )
				{
                    FinalLinkEnvironment.AdditionalLibraries.Add("DirectShowDEBUG.lib");
				}
				else
				{
                    FinalLinkEnvironment.AdditionalLibraries.Add("DirectShow.lib");
				}
			}
			
            // Compile and link with NVAPI
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/nvapi");

            // Compile and link with PN-AEN library
            GlobalCPPEnvironment.SystemIncludePaths.Add("../External/nvtesslib/inc");

			// Compile and link with lzo.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/lzopro/include");
			FinalLinkEnvironment.LibraryPaths.Add("../External/lzopro/lib");
			if( Platform == UnrealTargetPlatform.Win64 )
 			{
				// @todo win64: Enable LZOPro once we have 64-bit support
 				FinalLinkEnvironment.AdditionalLibraries.Add( "lzopro_64.lib" );
 			}
			else
 			{
 				FinalLinkEnvironment.AdditionalLibraries.Add( "lzopro.lib" );
 			}

			// Compile and link with zlib.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/zlib/inc");
			FinalLinkEnvironment.LibraryPaths.Add("../External/zlib/Lib");
			if( Platform == UnrealTargetPlatform.Win64 )
			{
				FinalLinkEnvironment.AdditionalLibraries.Add( "zlib_64.lib" );
			}
			else
			{
				FinalLinkEnvironment.AdditionalLibraries.Add( "zlib.lib" );
			}

            // Compile and link with wxWidgets.
            if (UE3BuildConfiguration.bCompileWxWidgets)
            {
                SetupWxWindowsEnvironment();
            }

            // Explicitly exclude the MS C++ runtime libraries we're not using, to ensure other libraries we link with use the same
			// runtime library as the engine.
			if (Configuration == UnrealTargetConfiguration.Debug)
			{
				FinalLinkEnvironment.ExcludedLibraries.Add("MSVCRT");
				FinalLinkEnvironment.ExcludedLibraries.Add("MSVCPRT");
			}
			else
			{
				FinalLinkEnvironment.ExcludedLibraries.Add("MSVCRTD");
				FinalLinkEnvironment.ExcludedLibraries.Add("MSVCPRTD");
			}
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBC");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCMT");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCPMT");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCP");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCD");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCMTD");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCPMTD");
			FinalLinkEnvironment.ExcludedLibraries.Add("LIBCPD");

			// Add the library used for the delayed loading of DLLs.
			FinalLinkEnvironment.AdditionalLibraries.Add( "delayimp.lib" );

            if (UE3BuildConfiguration.bCompileFBX)
            {
                // Compile and link with FBX.
                GlobalCPPEnvironment.Definitions.Add("WITH_FBX=1");
                GlobalCPPEnvironment.Definitions.Add("FBXSDK_NEW_API");

                GlobalCPPEnvironment.SystemIncludePaths.Add("../External/FBX/2013.1/include");
                GlobalCPPEnvironment.SystemIncludePaths.Add("../External/FBX/2013.1/include/fbxsdk");

                if (Platform == UnrealTargetPlatform.Win64)
                {
                    FinalLinkEnvironment.LibraryPaths.Add("../External/FBX/2013.1/lib/vs2010/x64");
                }
                else
                {
                    FinalLinkEnvironment.LibraryPaths.Add("../External/FBX/2013.1/lib/vs2010/x86");    
                }


                if (Configuration == UnrealTargetConfiguration.Debug)
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("fbxsdk-2013.1-mdd.lib");
                }
                else
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("fbxsdk-2013.1-md.lib");
                }
            }
            else
            {
                GlobalCPPEnvironment.Definitions.Add("WITH_FBX=0");
            }

			// EasyHook is used for different methods in 32 or 64 bit Windows, but is needed for both
			{
				GlobalCPPEnvironment.SystemIncludePaths.Add( "../External/EasyHook" );
				FinalLinkEnvironment.LibraryPaths.Add( "../External/EasyHook/" );
			}

			// Compile and link with DirectX.
			GlobalCPPEnvironment.SystemIncludePaths.Add( "$(DXSDK_DIR)/include" );
			if( Platform == UnrealTargetPlatform.Win64 )
			{
				FinalLinkEnvironment.LibraryPaths.Add( "$(DXSDK_DIR)/Lib/x64" );
			}
			else
			{
				FinalLinkEnvironment.LibraryPaths.Add( "$(DXSDK_DIR)/Lib/x86" );
			}

			// Link against DirectX
			FinalLinkEnvironment.AdditionalLibraries.Add("dinput8.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("dxguid.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("XInput.lib");

			// link against wininet (used by FBX and Facebook)
			FinalLinkEnvironment.AdditionalLibraries.Add("wininet.lib");

			if (!IsBuildingDedicatedServer())
            {
				FinalLinkEnvironment.AdditionalLibraries.Add( "d3d9.lib" );
				FinalLinkEnvironment.AdditionalLibraries.Add( "d3d11.lib" );
                FinalLinkEnvironment.DelayLoadDLLs.Add("d3d11.dll");
                FinalLinkEnvironment.AdditionalLibraries.Add("dxgi.lib");
				FinalLinkEnvironment.DelayLoadDLLs.Add( "dxgi.dll" );
			}

			// Link against D3DX
			if( !UE3BuildConfiguration.bCompileLeanAndMeanUE3 )
			{
				FinalLinkEnvironment.AdditionalLibraries.Add("d3dcompiler.lib");
				if (Configuration == UnrealTargetConfiguration.Debug)
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("d3dx9d.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("d3dx11d.lib");
				}
				else
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("d3dx9.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("d3dx11.lib");
				}
			}

			// Compile and link against OpenGL for desktop OpenGL as well as OpenGL ES emulation
			if (!IsBuildingDedicatedServer())
			{
				GlobalCPPEnvironment.SystemIncludePaths.Add( "../External/OpenGL" );
				FinalLinkEnvironment.AdditionalLibraries.Add( "opengl32.lib" );
				FinalLinkEnvironment.DelayLoadDLLs.Add( "opengl32.dll" );

				// OpenGL ES emulation support
				if( Game.ShouldCompileES2() && File.Exists( "ES2Drv/ES2Drv.vcxproj" ) )
				{
					GlobalCPPEnvironment.IncludePaths.Add( "ES2Drv/Inc" );
					GlobalCPPEnvironment.Definitions.Add( "USE_DYNAMIC_ES2_RHI=1" );
				}
				else
				{
					GlobalCPPEnvironment.Definitions.Add( "USE_DYNAMIC_ES2_RHI=0" );
				}
			}

			// Required for 3D spatialisation in XAudio2
            FinalLinkEnvironment.AdditionalLibraries.Add( "X3DAudio.lib" );
            FinalLinkEnvironment.AdditionalLibraries.Add( "xapobase.lib" );
            FinalLinkEnvironment.AdditionalLibraries.Add( "XAPOFX.lib" );

            if (!IsBuildingDedicatedServer() && !UE3BuildConfiguration.bCompileLeanAndMeanUE3)
			{
				// Compile and link with NVIDIA Texture Tools.
				GlobalCPPEnvironment.SystemIncludePaths.Add("../External/nvTextureTools-2.0.6/src/src");
				FinalLinkEnvironment.LibraryPaths.Add("../External/nvTextureTools-2.0.6/lib");
				if( Platform == UnrealTargetPlatform.Win64 )
				{
					FinalLinkEnvironment.AdditionalLibraries.Add( "nvtt_64.lib" );
				}
				else
				{
					FinalLinkEnvironment.AdditionalLibraries.Add( "nvtt.lib" );
				}

				// Compile and link with NVIDIA triangle strip generator.
				FinalLinkEnvironment.LibraryPaths.Add("../External/nvTriStrip/Lib");
				if (Configuration == UnrealTargetConfiguration.Debug)
				{
					if( Platform == UnrealTargetPlatform.Win64 )
					{
						FinalLinkEnvironment.AdditionalLibraries.Add( "nvTriStripD_64.lib" );
					}
					else
					{
						FinalLinkEnvironment.AdditionalLibraries.Add( "nvTriStripD.lib" );
					}
				}
				else
				{
					if( Platform == UnrealTargetPlatform.Win64 )
					{
						FinalLinkEnvironment.AdditionalLibraries.Add( "nvTriStrip_64.lib" );
					}
					else
					{
						FinalLinkEnvironment.AdditionalLibraries.Add( "nvTriStrip.lib" );
					}
				}
			}

            // Allegorithmic Substance Air Integration
            // Link with Substance Air engine and linker
            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_SUBSTANCE_AIR=0"))
            {
                string SubstanceAirPath = "../External/Substance/Framework/";
                string TCAirPath = "../External/Substance/AirTextureCache/lib/";

                string DllPath = null;
                string LibPath = null;
                string PlatformPath = null;
                string SubstancePlatformPath = null;
                string DllDstPath = null;
                bool useDebugBinaries = Utils.GetEnvironmentVariable("ue3.substance.useDebugBinaries", false);

                switch (GlobalCPPEnvironment.TargetPlatform)
                {
                    case CPPTargetPlatform.Win32:
                        PlatformPath = "Win32/";
                        SubstancePlatformPath = "win32-msvc2010/";
                        TCAirPath += "win32-msvc2010/";
                        break;
                    case CPPTargetPlatform.Win64:
                        PlatformPath = "Win64/";
                        SubstancePlatformPath = "win32-msvc2010-64/";
                        TCAirPath += "win32-msvc2010-64/";
                        break;
                }

                DllPath = SubstanceAirPath + "bin/" + SubstancePlatformPath;
                LibPath = SubstanceAirPath + "lib/" + SubstancePlatformPath;

                if (Configuration == UnrealTargetConfiguration.Debug && useDebugBinaries)
                {
                    TCAirPath += "debug_md/";
                    DllPath += "debug_md/";
                    LibPath += "debug_md";
                }
                else
                {
                    TCAirPath += "release_md/";
                    DllPath += "release_md/";
                    LibPath += "release_md";
                }

                FinalLinkEnvironment.LibraryPaths.Add(LibPath);
                FinalLinkEnvironment.LibraryPaths.Add(TCAirPath);

                FinalLinkEnvironment.AdditionalLibraries.Add("substance_sse2_blend.lib");
                FinalLinkEnvironment.AdditionalLibraries.Add("substance_linker.lib");
                FinalLinkEnvironment.AdditionalLibraries.Add("atc_api.lib");

                DllDstPath = "../../Binaries/" + PlatformPath;

                if (useDebugBinaries)
                {
                    System.IO.File.Copy(DllPath + "substance_sse2_blend.dll", DllDstPath + "substance_sse2_blend.dll", true);
                    System.IO.File.Copy(DllPath + "substance_linker.dll", DllDstPath + "substance_linker.dll", true);
                    System.IO.File.Copy(TCAirPath + "atc_api.dll", DllDstPath + "atc_api.dll", true);
                }
            }
            // end Allegorithmic Substance Air Integration

            // Compile and link with libffi only if we are building UDK.
			if (GlobalCPPEnvironment.Definitions.Contains("UDK=1") && !UE3BuildConfiguration.bCompileLeanAndMeanUE3)
            {
                GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libffi");
                FinalLinkEnvironment.AdditionalLibraries.Add("LibFFI.lib");
                if (Platform == UnrealTargetPlatform.Win64)
                {
                    if (Configuration == UnrealTargetConfiguration.Debug)
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/libffi/lib/x64/Debug");
                    }
                    else
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/libffi/lib/x64/Release");
                    }
                }
                else
                {
                    if (Configuration == UnrealTargetConfiguration.Debug)
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/libffi/lib/Win32/Debug");
                    }
                    else
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/libffi/lib/Win32/Release");
                    }
                }
                GlobalCPPEnvironment.Definitions.Add("WITH_LIBFFI=1");
            }

            // Compile and link with kissFFT
            if (!IsBuildingDedicatedServer() && !UE3BuildConfiguration.bCompileLeanAndMeanUE3)
            {
                GlobalCPPEnvironment.SystemIncludePaths.Add("../External/kiss_fft129");
                FinalLinkEnvironment.AdditionalLibraries.Add("KissFFT.lib");
                if (Platform == UnrealTargetPlatform.Win64)
                {
                    if (Configuration == UnrealTargetConfiguration.Debug)
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/kiss_fft129/lib/x64/Debug");
                    }
                    else
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/kiss_fft129/lib/x64/Release");
                    }
                }
                else
                {
                    if (Configuration == UnrealTargetConfiguration.Debug)
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/kiss_fft129/lib/Win32/Debug");
                    }
                    else
                    {
                        FinalLinkEnvironment.LibraryPaths.Add("../External/kiss_fft129/lib/Win32/Release");
                    }
                }
                GlobalCPPEnvironment.Definitions.Add("WITH_KISSFFT=1");
            }

			// Compile and link with Ogg/Vorbis.
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libogg-1.2.2/include");
			GlobalCPPEnvironment.SystemIncludePaths.Add("../External/libvorbis-1.3.2/include");

			if( Platform == UnrealTargetPlatform.Win64 )
			{
				//@todo.VS10: Support debug versions of these libraries?
				FinalLinkEnvironment.LibraryPaths.Add( "../External/libvorbis-1.3.2/win32/VS2010/libvorbis/x64/Release" );
				FinalLinkEnvironment.LibraryPaths.Add( "../External/libvorbis-1.3.2/win32/VS2010/libvorbisfile/x64/Release" );
				FinalLinkEnvironment.LibraryPaths.Add( "../External/libogg-1.2.2/win32/VS2010/x64/Release" );
				FinalLinkEnvironment.AdditionalLibraries.Add( "libvorbis_64.lib" );
				FinalLinkEnvironment.AdditionalLibraries.Add("libvorbisfile_64.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("libogg_64.lib");
			}
			else
			{
				//@todo.VS10: Support debug versions of these libraries?
				FinalLinkEnvironment.LibraryPaths.Add( "../External/libvorbis-1.3.2/win32/VS2010/libvorbis/Win32/Release" );
				FinalLinkEnvironment.LibraryPaths.Add( "../External/libvorbis-1.3.2/win32/VS2010/libvorbisfile/Win32/Release" );
				FinalLinkEnvironment.LibraryPaths.Add( "../External/libogg-1.2.2/win32/VS2010/Win32/Release" );
				FinalLinkEnvironment.AdditionalLibraries.Add("libvorbis.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("libvorbisfile.lib");
				FinalLinkEnvironment.AdditionalLibraries.Add("libogg.lib");
			}

            if (Platform == UnrealTargetPlatform.Win64)
            {
                FinalLinkEnvironment.AdditionalLibraries.Add("nvapi64.lib");
            }
            else
            {
                FinalLinkEnvironment.AdditionalLibraries.Add("nvapi.lib");
            }

            if (Configuration == UnrealTargetConfiguration.Debug)
            {
                FinalLinkEnvironment.AdditionalLibraries.Add("nvtessd.lib");
            }
            else
            {
                FinalLinkEnvironment.AdditionalLibraries.Add("nvtess.lib");
            }

			// Compile and link with Win32 API libraries.
			FinalLinkEnvironment.AdditionalLibraries.Add("rpcrt4.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("wsock32.lib");

			FinalLinkEnvironment.AdditionalLibraries.Add("dbghelp.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("comctl32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("Winmm.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("kernel32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("user32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("gdi32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("winspool.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("comdlg32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("advapi32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("shell32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("ole32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("oleaut32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("uuid.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("odbc32.lib");
			FinalLinkEnvironment.AdditionalLibraries.Add("odbccp32.lib");

			// No more XDK support needed
				GlobalCPPEnvironment.Definitions.Add("XDKINSTALLED=0");

            // Link with GFx
            if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx=0"))
            {
                UnrealTargetConfiguration ForcedConfiguration = Configuration;
/*                if (UE3BuildConfiguration.bForceScaleformRelease && Configuration == UnrealTargetConfiguration.Debug)
                {
                    ForcedConfiguration = UnrealTargetConfiguration.Release;
                }*/
            	switch(ForcedConfiguration)
			    {
				    case UnrealTargetConfiguration.Debug:
                        if (Platform == UnrealTargetPlatform.Win64)
                        {
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/x64/Msvc10/Debug");
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/jpeg-6b/Lib/x64/Msvc10/Debug");
                        }
                        else
                        {
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/Win32/Msvc10/Debug");
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/jpeg-6b/Lib/Win32/Msvc10/Debug");
                        }
                        break;

					case UnrealTargetConfiguration.Shipping:
						if( Platform == UnrealTargetPlatform.Win64 )
						{
							FinalLinkEnvironment.LibraryPaths.Add( GFxDir + "/Lib/x64/Msvc10/Shipping" );
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/jpeg-6b/Lib/x64/Msvc10/Shipping");
                        }
						else
						{
							FinalLinkEnvironment.LibraryPaths.Add( GFxDir + "/Lib/Win32/Msvc10/Shipping" );
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/jpeg-6b/Lib/Win32/Msvc10/Shipping");
                        }
						break;

                    default:
                        if (Platform == UnrealTargetPlatform.Win64)
                        {
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/x64/Msvc10/Release");
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/jpeg-6b/Lib/x64/Msvc10/Release");
                        }
                        else
                        {
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/Lib/Win32/Msvc10/Release");
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/jpeg-6b/Lib/Win32/Msvc10/Release");
                        }
                        break;
                }

				// For UDK 
				{
					FinalLinkEnvironment.AdditionalLibraries.Add("libgfx.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("libgfx_as2.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("libgfx_as3.lib");
                    if (GlobalCPPEnvironment.Definitions.Contains("WITH_GFx_IME=1"))
					{
						FinalLinkEnvironment.AdditionalLibraries.Add("libgfx_ime.lib");
                        FinalLinkEnvironment.AdditionalLibraries.Add("libgfxexpat.lib");
					}
                    //FinalLinkEnvironment.AdditionalLibraries.Add("libjpeg.lib");

                    if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx_AUDIO=0"))
                    {
                        if (Platform == UnrealTargetPlatform.Win64)
                        {
                            GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/3rdParty/fmod/pc/x64/inc");
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/fmod/pc/x64/lib");
                            FinalLinkEnvironment.AdditionalLibraries.Add("fmodex64L_vc.lib");
                        }
                        else
                        {
                            GlobalCPPEnvironment.SystemIncludePaths.Add(GFxDir + "/3rdParty/fmod/pc/Win32/inc");
                            FinalLinkEnvironment.LibraryPaths.Add(GFxDir + "/3rdParty/fmod/pc/Win32/lib");
                            FinalLinkEnvironment.AdditionalLibraries.Add("fmodexL_vc.lib");
                        }
                        FinalLinkEnvironment.AdditionalLibraries.Add("libgfxsound_fmod.lib");
                    }

                    if (!GlobalCPPEnvironment.Definitions.Contains("WITH_GFx_VIDEO=0"))
                    {
                        FinalLinkEnvironment.AdditionalLibraries.Add("libgfxvideo.lib");
                    }
                }
            }

			// Add the Win32-specific projects.
            if (IsBuildingDedicatedServer())
            {
				NonGameProjects.Add(new UE3ProjectDesc("WinDrv/WinDrv.vcxproj"));
				NonGameProjects.Add(new UE3ProjectDesc("XAudio2/XAudio2.vcxproj"));
            }
            else
            {
				NonGameProjects.Add(new UE3ProjectDesc("D3D11Drv/D3D11Drv.vcxproj"));
				NonGameProjects.Add(new UE3ProjectDesc("D3D9Drv/D3D9Drv.vcxproj"));
				NonGameProjects.Add(new UE3ProjectDesc("OpenGLDrv/OpenGLDrv.vcxproj"));
				NonGameProjects.Add(new UE3ProjectDesc("WinDrv/WinDrv.vcxproj"));
				NonGameProjects.Add(new UE3ProjectDesc("XAudio2/XAudio2.vcxproj"));

				// OpenGL ES emulation support
				if( Game.ShouldCompileES2() && File.Exists( "ES2Drv/ES2Drv.vcxproj" ) )
				{
					NonGameProjects.Add( new UE3ProjectDesc( "ES2Drv/ES2Drv.vcxproj" ) );
				}
            }
           
            // Add library paths for libraries included via pragma comment(lib)
			if( Platform == UnrealTargetPlatform.Win64 )
			{
                FinalLinkEnvironment.LibraryPaths.Add("../External/AgPerfMon/lib/win64");

				switch( UE3BuildConfiguration.BinkVersion )
				{
				case 0:
					break;
				case 1:
					FinalLinkEnvironment.LibraryPaths.Add( "../External/Bink/lib/x64/" );
					break;
				case 2:
					FinalLinkEnvironment.LibraryPaths.Add( "../External/Bink2/Win64/" );
					break;
				}
			}
			else
			{
                FinalLinkEnvironment.LibraryPaths.Add("../External/AgPerfMon/lib/win32");

				switch( UE3BuildConfiguration.BinkVersion )
				{
				case 0:
					break;
				case 1:
					FinalLinkEnvironment.LibraryPaths.Add( "../External/Bink/lib/Win32/" );
					break;
				case 2:
					FinalLinkEnvironment.LibraryPaths.Add( "../External/Bink2/Win32/" );
					break;
				}
			}
			FinalLinkEnvironment.LibraryPaths.Add("../External/GamersSDK/4.2.1/lib/Win32/");

            SetUpSpeedTreeEnvironment();
			SetUpTriovizEnvironment();
            SetUpSimplygonEnvironment();

			FinalLinkEnvironment.LibraryPaths.Add("../External/DECtalk464/lib/Win32");
			if( Platform == UnrealTargetPlatform.Win64 )
			{
				FinalLinkEnvironment.LibraryPaths.Add( "../External/FaceFX/FxSDK/lib/x64/vs10/" );
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/FxCG/lib/x64/vs10/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/FxAnalysis/lib/x64/vs10/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/Studio/External/libresample-0.1.3/lib/x64/vs10/");
			}
			else
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/FxSDK/lib/win32/vs10/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/FxCG/lib/win32/vs10/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/FxAnalysis/lib/win32/vs10/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/FaceFX/Studio/External/libresample-0.1.3/lib/win32/vs10/");
			}
			
			if (Platform == UnrealTargetPlatform.Win64)
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/SDKs/lib/win64/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/Nxd/lib/win64/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/SDKs/TetraMaker/lib/win64/");
                FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/APEX/lib/vc10win64-PhysX_2.8.4/");
			}
			else
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/SDKs/lib/win32/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/Nxd/lib/win32/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/SDKs/TetraMaker/lib/win32/");
				FinalLinkEnvironment.LibraryPaths.Add("../External/PhysX/APEX/lib/vc10win32-PhysX_2.8.4/");
			}
			FinalLinkEnvironment.LibraryPaths.Add("../External/libPNG/lib/");
			FinalLinkEnvironment.LibraryPaths.Add("../External/nvDXT/Lib/");
			FinalLinkEnvironment.LibraryPaths.Add("../External/ConvexDecomposition/Lib/");
			if (Configuration == UnrealTargetConfiguration.Debug)
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/TestTrack/Lib/Debug/");
			}
			else
			{
				FinalLinkEnvironment.LibraryPaths.Add("../External/TestTrack/Lib/Release/");
			}

			if ( UE3BuildConfiguration.bBuildEditor )
			{
				NonGameProjects.Add(new UE3ProjectDesc("GFxUIEditor/GFxUIEditor.vcxproj"));
				GlobalCPPEnvironment.IncludePaths.Add("GFxUIEditor/Inc");
			}
			
			// For 64-bit builds, we'll forcibly ignore a linker warning with DirectInput.  This is
			// Microsoft's recommended solution as they don't have a fixed .lib for us.
			if( Platform == UnrealTargetPlatform.Win64 )
			{
				FinalLinkEnvironment.AdditionalArguments += " /ignore:4078";
			}

			FinalLinkEnvironment.LibraryPaths.Add("../External/nvapi/");

            if (Platform == UnrealTargetPlatform.Win64)
            {
                FinalLinkEnvironment.LibraryPaths.Add("../External/nvtesslib/lib/x64/");
            }
            else
            {
                FinalLinkEnvironment.LibraryPaths.Add("../External/nvtesslib/lib/Win32/");
            }

			if( Platform == UnrealTargetPlatform.Win32 )
			{
				GlobalCPPEnvironment.Definitions.Add( "WITH_OPEN_AUTOMATE=1" );
			}

			// Setup CLR environment
			if( UE3BuildConfiguration.bAllowManagedCode )
			{
				// Set a global C++ definition so that any CLR-based features can turn themselves on
				GlobalCPPEnvironment.Definitions.Add( "WITH_MANAGED_CODE=1" );

				// Setup Editor DLL file path
				String EditorDLLFileName = "UnrealEdCSharp.dll";
				String EditorDLLDirectory = Path.GetDirectoryName(OutputPath) + "\\..\\";
				String EditorDLLAssemblyPath = EditorDLLDirectory + "\\" + EditorDLLFileName;

				// Add C# projects
				NonGameProjects.Add( new UE3ProjectDesc( "UnrealEdCSharp/UnrealEdCSharp.csproj", EditorDLLAssemblyPath) );

				// Add C++/CLI projects
				NonGameProjects.Add(new UE3ProjectDesc("UnrealEdCLR/UnrealEdCLR.vcxproj", CPPCLRMode.CLREnabled));
				NonGameProjects.Add(new UE3ProjectDesc("UnrealSwarm/SwarmInterfaceMake.vcxproj", CPPCLRMode.CLREnabled));

				// Add required .NET Framework assemblies
				{
					// Pass the 4.0 reference assembly path as the assembly search path.
					GlobalCPPEnvironment.SystemDotNetAssemblyPaths.Add(
						Environment.GetFolderPath( Environment.SpecialFolder.ProgramFilesX86 ) +
						"/Reference Assemblies/Microsoft/Framework/.NETFramework/v4.0" );

					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "System.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "System.Data.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "System.Drawing.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "System.Xml.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "System.Management.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "System.Windows.Forms.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "PresentationCore.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "PresentationFramework.dll" );
					GlobalCPPEnvironment.FrameworkAssemblyDependencies.Add( "WindowsBase.dll" );
				}

				// Add private assembly dependencies.  If any of these are changed, then CLR projects will
				// be forced to rebuild.
				{
					// The editor needs to be able to link against it's own .NET assembly dlls/exes.  For example,
					// this is needed so that we can reference the "UnrealEdCSharp" assembly.  At runtime, the
					// .NET loader will locate these using the settings specified in the target's app.config file					
					AddPrivateAssembly(EditorDLLDirectory, "", EditorDLLFileName);

					// Add reference to AgentInterface
					string AgentDLLDirectory = Path.GetDirectoryName(OutputPath) + "\\..\\";
					AddPrivateAssembly(AgentDLLDirectory, "", "AgentInterface.dll");
				}


				// Use of WPF in managed projects requires single-threaded apartment model for CLR threads
				FinalLinkEnvironment.AdditionalArguments += " /CLRTHREADATTRIBUTE:STA";
			}
		}

        void SetUpDedicatedServerEnvironment()
        {
            // Check for dedicated server being requested
            if (IsBuildingDedicatedServer())
            {           
                // Add define
                GlobalCPPEnvironment.Definitions.Add("DEDICATED_SERVER=1");
                // Turn on NULL_RHI
                GlobalCPPEnvironment.Definitions.Add("USE_NULL_RHI=1");
                // Turn off TTS
                GlobalCPPEnvironment.Definitions.Add("WITH_TTS=0");
                // Turn off Speech Recognition
                GlobalCPPEnvironment.Definitions.Add("WITH_SPEECH_RECOGNITION=0");
            }
            else
            {
                // Add define
                GlobalCPPEnvironment.Definitions.Add("DEDICATED_SERVER=0");
            }
        }

        void SetupWxWindowsEnvironment()
        {
            // Compile and link with wxWidgets.
            GlobalCPPEnvironment.IncludePaths.Add("../External/wxWidgets/include");
            GlobalCPPEnvironment.IncludePaths.Add("../External/wxWidgets/lib/vc_dll");
            GlobalCPPEnvironment.IncludePaths.Add("../External/wxExtended/wxDockit/include");

            GlobalCPPEnvironment.Definitions.Add("WXUSINGDLL");
            GlobalCPPEnvironment.Definitions.Add("wxUSE_NO_MANIFEST");
            if (Platform == UnrealTargetPlatform.Win64)
            {
                FinalLinkEnvironment.LibraryPaths.Add("../External/wxWidgets/lib/vc_dll/x64");
                if (Configuration == UnrealTargetConfiguration.Debug)
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_core_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_aui_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_xrc_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_richtext_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_qa_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_media_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_html_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_adv_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_net_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_xml_64.lib");
                }
                else
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_core_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_aui_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_xrc_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_richtext_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_qa_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_media_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_html_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_adv_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_net_64.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_xml_64.lib");
                }
            }
            else
            {
                FinalLinkEnvironment.LibraryPaths.Add("../External/wxWidgets/lib/vc_dll/win32");
                if (Configuration == UnrealTargetConfiguration.Debug)
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_core.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_aui.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_xrc.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_richtext.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_qa.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_media.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_html.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_adv.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_net.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28ud_xml.lib");
                }
                else
                {
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_core.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_aui.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_xrc.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_richtext.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_qa.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_media.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_html.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_adv.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_net.lib");
                    FinalLinkEnvironment.AdditionalLibraries.Add("wxmsw28u_xml.lib");
                }
            }
        }

		/** Creates an action to copy the specified file */
		void CreateFileCopyAction( FileItem SourceFile, FileItem DestinationFile )
		{
			Action CopyFileAction = new Action();

			// Specify the source file (prerequisite) for the action
			CopyFileAction.PrerequisiteItems.Add( SourceFile );

			// Setup the "copy file" action
			CopyFileAction.WorkingDirectory = Path.GetFullPath( "." );
			CopyFileAction.StatusDescription = string.Format( "- Copying '{0}'...", Path.GetFileName( SourceFile.AbsolutePath ) );
			CopyFileAction.bCanExecuteRemotely = false;

			// Setup xcopy.exe command line
			CopyFileAction.CommandPath = String.Format( "xcopy.exe" );
			CopyFileAction.CommandArguments = "";
			CopyFileAction.CommandArguments += " /Q";	// Don't display file items while copying
			CopyFileAction.CommandArguments += " /Y";	// Suppress prompts about overwriting files
			CopyFileAction.CommandArguments += " /I";	// Specifies that destination parameter is a directory, not a file
			CopyFileAction.CommandArguments +=
				String.Format( " \"{0}\" \"{1}\"",
					SourceFile.AbsolutePath, Path.GetDirectoryName( DestinationFile.AbsolutePath ) + Path.DirectorySeparatorChar );

			// We don't want to avoid xcopy spewing text to the build window, where possible
			CopyFileAction.bShouldBlockStandardOutput = true;
			CopyFileAction.bShouldBlockStandardInput = true;

			// We don't need to track the command executed for this action and doing so actually
			// causes unnecessary out-of-date prerequisites since the tracking file will almost
			// always be newer than the file being copied.
			CopyFileAction.bShouldTrackCommand = false;

			// Specify the output file
			CopyFileAction.ProducedItems.Add( DestinationFile );
		}


		/** Returns output files for any private assembly dependencies that need to be copied, including PDB files. */
		List<FileItem> GetCopiedPrivateAssemblyItems()
		{
			List<FileItem> CopiedFiles = new List<FileItem>();

			foreach( PrivateAssemblyInfo CurPrivateAssembly in GlobalCPPEnvironment.PrivateAssemblyDependencies )
			{
				if( CurPrivateAssembly.bShouldCopyToOutputDirectory )
				{
					// Setup a destination file item for the file we're copying
					string DestinationFilePath =
						Path.GetDirectoryName( OutputPath ) + "\\" +
						CurPrivateAssembly.DestinationSubDirectory +
						Path.GetFileName( CurPrivateAssembly.FileItem.AbsolutePath );
					FileItem DestinationFileItem = FileItem.GetItemByPath( DestinationFilePath );

					// Create the action to copy the file
					CreateFileCopyAction( CurPrivateAssembly.FileItem, DestinationFileItem );
					CopiedFiles.Add( DestinationFileItem );

					// Also create a file item for the PDB associated with this assembly
					string PDBFileName = Path.GetFileNameWithoutExtension( CurPrivateAssembly.FileItem.AbsolutePath ) + ".pdb";
					string PDBFilePath = Path.GetDirectoryName( CurPrivateAssembly.FileItem.AbsolutePath ) + "\\" + PDBFileName;
					if( File.Exists( PDBFilePath ) )
					{
						FileItem SourcePDBFileItem = FileItem.GetItemByPath( PDBFilePath );
						string DestinationPDBFilePath =
							Path.GetDirectoryName( OutputPath ) + "\\" +
							CurPrivateAssembly.DestinationSubDirectory +
							PDBFileName;
						FileItem DestinationPDBFileItem = FileItem.GetItemByPath( DestinationPDBFilePath );

						// Create the action to copy the PDB file
						CreateFileCopyAction( SourcePDBFileItem, DestinationPDBFileItem );
						CopiedFiles.Add( DestinationPDBFileItem );
					}
				}
			}

			return CopiedFiles;
		}

		
		List<FileItem> GetWindowsOutputItems()
		{
			// Verify that the user has specified the expected output extension.
			if( Path.GetExtension( OutputPath ).ToUpperInvariant() != ".EXE" && Path.GetExtension( OutputPath ).ToUpperInvariant() != ".DLL" )
			{
				throw new BuildException("Unexpected output extension: {0} instead of .EXE", Path.GetExtension(OutputPath));
			}

			// Put the non-executable output files (PDB, import library, etc) in the same directory as the executables
			FinalLinkEnvironment.OutputDirectory = Path.GetDirectoryName(OutputPath);

            if (AdditionalDefinitions.Contains("USE_MALLOC_PROFILER=1"))
            {
                if (!OutputPath.ToLower().Contains(".mprof.exe"))
                {
                    OutputPath = Path.ChangeExtension(OutputPath, ".MProf.exe");
                }
            }

			// Link the EXE file.
			FinalLinkEnvironment.OutputFilePath = OutputPath;
			FileItem EXEFile = FinalLinkEnvironment.LinkExecutable();

            // Do post build step for Steamworks if requested
            if (GlobalCPPEnvironment.Definitions.Contains("WITH_STEAMWORKS=1"))
            {
                string GameName = Game.GetGameName();

                // Copy the steam_appid.txt during development
                string SrcAppIDPath = string.Format("..\\..\\{0}\\Build\\Steam\\steam_appid.txt", GameName);
                string DstAppIDPath = string.Format("{0}\\steam_appid.txt", Path.GetDirectoryName(EXEFile.AbsolutePath));

                // If the file exists then make sure it's already writable so we can copy over it
                if (File.Exists(DstAppIDPath))
                {
                    File.SetAttributes(DstAppIDPath, FileAttributes.Normal);
                }

                File.Copy(SrcAppIDPath, DstAppIDPath, true);
            }

            // Do post build step for Windows Live if requested
            if (GlobalCPPEnvironment.Definitions.Contains("WITH_PANORAMA=1"))
            {
				// Creates a cfg file so Live can load
				string CfgFilePath = string.Format( "{0}.cfg", EXEFile.AbsolutePath );

				// If the file exists then make sure it's already writable so we can copy over it
				if( File.Exists( CfgFilePath ) )
				{
					File.SetAttributes( CfgFilePath, FileAttributes.Normal );
				}

				// Copy the live config file over our game-specific version
				string GameName = Game.GetGameName();
				string SrcFilePath = string.Format( ".\\{0}\\Live\\LiveConfig.xml", GameName );
				if( !File.Exists( SrcFilePath ) )
				{
					string GameNameBase = GameName.Replace( "Game", "Base" );
					SrcFilePath = string.Format( ".\\{0}\\Live\\LiveConfig.xml", GameNameBase );
				}

				File.Copy( SrcFilePath, CfgFilePath, true );
            }

            // Return a list of the output files.
			List<FileItem> OutputFiles = new List<FileItem>();
			OutputFiles.Add( EXEFile );
			
			// Also return any private assemblies that we copied to the output folder
			OutputFiles.AddRange( GetCopiedPrivateAssemblyItems() );

			return OutputFiles;
		}
	}
}
