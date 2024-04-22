/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using IWshRuntimeLibrary;

namespace UnSetup
{
	public partial class Utils
	{
		public string GetAllUsersStartMenu()
		{
			string StartMenu = "";
			try
			{
				WshShellClass Shell = new WshShellClass();
				IWshCollection Coll = Shell.SpecialFolders;
				object LocationToLookup = "AllUsersPrograms";
				StartMenu = ( string )Coll.Item( ref LocationToLookup );
			}
			catch
			{
			}

			return ( StartMenu );
		}

		public void CreateShortcuts( string StartMenuLocation, string InstallLocation, bool bIncludeSpeedTree )
		{
			try
			{
				WshShellClass Shell = new WshShellClass();

				foreach( GameManifestOptions GameInstallInfo in Manifest.GameInfo )
				{
					IWshShortcut EditorShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\" + GameInstallInfo.Name + " Editor.lnk" );
					EditorShortcut.TargetPath = InstallLocation + "\\Binaries\\" + GameInstallInfo.AppToElevate;
					EditorShortcut.IconLocation = InstallLocation + "\\Binaries\\" + GameInstallInfo.AppToCreateInis + ", 1";
					EditorShortcut.Arguments = "editor";
					EditorShortcut.WorkingDirectory = InstallLocation;
					EditorShortcut.Save();

					IWshShortcut GameShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\" + GameInstallInfo.Name + " Game.lnk" );
					GameShortcut.TargetPath = InstallLocation + "\\Binaries\\" + GameInstallInfo.AppToCreateInis;
					GameShortcut.IconLocation = InstallLocation + "\\Binaries\\" + GameInstallInfo.AppToCreateInis + ", 0";
					GameShortcut.WorkingDirectory = InstallLocation;
					GameShortcut.Save();
				}

                // Add SpeedTree tools shortcuts
				if( bIncludeSpeedTree )
				{
					IWshShortcut SpeedTreeModShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\Tools\\SpeedTree 5.0 Modeler.lnk" );
					SpeedTreeModShortcut.TargetPath = InstallLocation + "\\Binaries\\SpeedTreeModeler\\SpeedTree Modeler UDK.exe";
					SpeedTreeModShortcut.WorkingDirectory = InstallLocation + "\\Binaries\\SpeedTreeModeler";
					SpeedTreeModShortcut.Save();

					IWshShortcut SpeedTreeComShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\Tools\\SpeedTree 5.0 Compiler.lnk" );
					SpeedTreeComShortcut.TargetPath = InstallLocation + "\\Binaries\\SpeedTreeModeler\\SpeedTree Compiler UDK.exe";
					SpeedTreeComShortcut.WorkingDirectory = InstallLocation + "\\Binaries\\SpeedTreeModeler";
					SpeedTreeComShortcut.Save();
				}

                // Add UFE tool shortcut
				IWshShortcut UFEShortcut = (IWshShortcut)Shell.CreateShortcut(StartMenuLocation + "\\Tools\\Unreal Frontend.lnk");
                UFEShortcut.TargetPath = InstallLocation + "\\Binaries\\UnrealFrontend.exe";
                UFEShortcut.WorkingDirectory = InstallLocation + "\\Binaries";
                UFEShortcut.Save();

				// Add the iOS configurator
				IWshShortcut IPPShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\Tools\\Unreal iOS Configuration.lnk" );
				IPPShortcut.TargetPath = InstallLocation + "\\Binaries\\IPhone\\iPhonePackager.exe";
				IPPShortcut.Arguments = "gui MobileGame ConfigNA";
				IPPShortcut.WorkingDirectory = InstallLocation + "\\Binaries\\IPhone";
				IPPShortcut.Save();

                // Add the documentation shortcuts
				IWshShortcut ReadMeShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\Documentation\\ReadMe.lnk" );
				ReadMeShortcut.TargetPath = GetSafeLocFileName( InstallLocation + "\\Engine\\Localization\\Readme." + Manifest.RootName, "rtf" );
				ReadMeShortcut.WorkingDirectory = InstallLocation;
				ReadMeShortcut.Save();

				foreach( LinkShortcutOptions ShortcutInfo in Manifest.LinkShortcuts )
				{
					IWshShortcut Shortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\" + ShortcutInfo.DisplayPath );
					Shortcut.TargetPath = InstallLocation + "\\" + ShortcutInfo.UrlFilePath;
					Shortcut.IconLocation = InstallLocation + "\\Binaries\\InstallData\\Link.ico";
					Shortcut.WorkingDirectory = InstallLocation;
					Shortcut.Save();
				}
			}
			catch
			{
			}
		}

		public void CreateGameShortcuts( string StartMenuLocation, string InstallLocation )
		{
			try
			{
				WshShellClass Shell = new WshShellClass();

				IWshShortcut GameShortcut = ( IWshShortcut )Shell.CreateShortcut( StartMenuLocation + "\\" + GetGameLongName() + ".lnk" );
				GameShortcut.TargetPath = InstallLocation + "\\Binaries\\" + Manifest.AppToLaunch;
				GameShortcut.Arguments = "-seekfreeloading";
				GameShortcut.IconLocation = InstallLocation + "\\Binaries\\InstallData\\GameIcon.ico";
				GameShortcut.WorkingDirectory = InstallLocation;
				GameShortcut.Save();
			}
			catch
			{
			}
		}

		public bool AnalyseShortcut( string FullName )
		{
			try
			{
				WshShellClass Shell = new WshShellClass();

				IWshShortcut Shortcut = ( IWshShortcut )Shell.CreateShortcut( FullName );
				bool IsUDK = Shortcut.WorkingDirectory.StartsWith( PackageInstallLocation );
				return ( IsUDK );
			}
			catch
			{
			}

			return ( false );
		}
	}
}
