/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Text;
using Microsoft.Win32;

namespace UnSetup
{
	public partial class Utils
	{
		public void AddUDKToARP( string InstallLocation, string DisplayName, bool bEULAAccepted )
		{
			try
			{
				RegistryKey Key = Registry.LocalMachine.CreateSubKey( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + Manifest.RootName + "-" + InstallInfoData.InstallGuidString );
				if( Key != null )
				{
					Key.SetValue( "DisplayName", DisplayName, RegistryValueKind.String );

					Key.SetValue( "InstallLocation", InstallLocation, RegistryValueKind.String );
					Key.SetValue( "Publisher", "Epic Games, Inc.", RegistryValueKind.String );
					Key.SetValue( "DisplayIcon", InstallLocation + "\\Binaries\\InstallData\\Uninstall.ico", RegistryValueKind.String );
					Key.SetValue( "UninstallString", InstallLocation + "\\Binaries\\UnSetup.exe /uninstall", RegistryValueKind.String );

					Key.SetValue( "NoModify", 1, RegistryValueKind.DWord );
					Key.SetValue( "NoRepair", 1, RegistryValueKind.DWord );

					if( bEULAAccepted )
					{
						Key.SetValue( "EULAAccepted", 1, RegistryValueKind.DWord );
					}
					else
					{
						Key.SetValue( "EULAAccepted", 0, RegistryValueKind.DWord );
					}

					Key.Close();
				}
			}
			catch
			{
			}
		}

		public void RemoveUDKFromARP()
		{
			try
			{
				Registry.LocalMachine.DeleteSubKey( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + Manifest.RootName + "-" + InstallInfoData.InstallGuidString );
			}
			catch
			{
			}
		}

		public Guid GetMachineID()
		{
			Guid MachineID = Guid.Empty;

			RegistryKey Key = Registry.CurrentUser.OpenSubKey( "SOFTWARE\\Epic Games\\UDK" );
			if( Key != null )
			{
				string MachineIDString = ( string )Key.GetValue( "ID" );
				MachineID = new Guid( MachineIDString );

				Key.Close();
			}

			return ( MachineID );
		}

		public bool GetEULAAccepted()
		{
			int IsEULAAccepted = 0;
			RegistryKey Key = Registry.LocalMachine.OpenSubKey( "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + Manifest.RootName + "-" + InstallInfoData.InstallGuidString );
			if( Key != null )
			{
				object Value = Key.GetValue( "EULAAccepted" );
				if( Value != null )
				{
					IsEULAAccepted = ( int )Value;
				}
				Key.Close();
			}

			return ( IsEULAAccepted == 1 );
		}

		public string GetUninstallInfo()
		{
			string UninstallInfo = String.Empty;
			RegistryKey Key = Registry.LocalMachine.OpenSubKey("SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\" + Manifest.RootName + "-" + InstallInfoData.InstallGuidString);
			if (Key != null)
			{
				object Value = Key.GetValue("UninstallString");
				if (Value != null)
				{
					UninstallInfo = (string)Value;
				}
				Key.Close();
			}

			return UninstallInfo;
		}
	}
}
