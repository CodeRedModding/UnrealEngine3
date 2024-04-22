/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;
using System.Windows.Forms;
using System.Runtime.InteropServices;
using Microsoft.Win32;

namespace UnSetup
{
	public partial class Utils
	{
		[StructLayout( LayoutKind.Sequential )]
		public struct OSVERSIONINFO
		{
			public int dwOSVersionInfoSize;
			public int dwMajorVersion;
			public int dwMinorVersion;
			public int dwBuildNumber;
			public int dwPlatformId;
			[MarshalAs( UnmanagedType.ByValTStr, SizeConst = 128 )]
			public string szCSDVersion;
		}

		[DllImport( "kernel32.Dll" )]
		public static extern short GetVersionEx( ref OSVERSIONINFO o );

		public bool WaitForProcess( Process RunningProcess, int Timeout )
		{
			int WaitCount = 0;
			while( !RunningProcess.HasExited )
			{
				Application.DoEvents();

				RunningProcess.WaitForExit( 100 );
				WaitCount++;

				if( WaitCount > Timeout * 10 )
				{
					GenericQuery Query = new GenericQuery( "GQCaptionWaiting", "GQDescWaiting", true, "GQCancel", true, "GQRetry" );
					Query.ShowDialog();
					if( Query.DialogResult == DialogResult.Cancel )
					{
						break;
					}
					else
					{
						WaitCount = 0;
					}
				}
			}

			if( !RunningProcess.HasExited )
			{
				RunningProcess.Kill();
				return ( false );
			}

			if( RunningProcess.ExitCode != 0 )
			{
				return ( false );
			}

			return ( true );
		}

		private int GetOSVersionMajor()
		{
			OSVERSIONINFO OS = new OSVERSIONINFO();

			try
			{
				OS.dwOSVersionInfoSize = Marshal.SizeOf( typeof( OSVERSIONINFO ) );
				GetVersionEx( ref OS );
			}
			catch
			{
			}

			return ( OS.dwMajorVersion );
		}

		public string InstallVCRedist( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";
			Process VCRedist = null;

			Progress.LabelDescription.Text = GetPhrase( "RedistVCRedistHeader" );
			if( bStandAloneRedist || Manifest.RootName != "UDK" )
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistVCRedistContentTech" );
			}
			else
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistVCRedistContent" );
			}

			Application.DoEvents();

			// Always install the x86 redists on all OSes
			ProcessStartInfo StartInfox86 = new ProcessStartInfo( "vcredist_x86_vs2010sp1.exe", "/q" );
			StartInfox86.WorkingDirectory = SourceFolder;
			VCRedist = Process.Start( StartInfox86 );
			if( WaitForProcess( VCRedist, 120 ) == false )
			{
				Status = GetPhrase( "RedistVCRedistx86Fail" );
			}

			Application.DoEvents();

			// Install the x64 redist on 64 bit OSes
			if( Environment.Is64BitOperatingSystem )
			{
				ProcessStartInfo StartInfox64 = new ProcessStartInfo( "vcredist_x64_vs2010sp1.exe", "/q" );
				StartInfox64.WorkingDirectory = SourceFolder;
				VCRedist = Process.Start( StartInfox64 );
				if( WaitForProcess( VCRedist, 120 ) == false )
				{
					Status = GetPhrase( "RedistVCRedistx64Fail" );
				}
			}

			return ( Status );
		}

		public string InstallDXCutdown( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";

			Progress.LabelDescription.Text = GetPhrase( "RedistDXRedistHeader" );
			if( bStandAloneRedist || Manifest.RootName != "UDK" )
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistDXRedistContentTech" );
			}
			else
			{
				Progress.LabelDetail.Text = GetPhrase( "RedistDXRedistContent" );
			}

			Application.DoEvents();

			ProcessStartInfo StartInfo = new ProcessStartInfo( "DXRedistCutdown\\DXSetup.exe", "/silent" );
			StartInfo.WorkingDirectory = SourceFolder;
			Process DXRedist = Process.Start( StartInfo );
			if( WaitForProcess( DXRedist, 240 ) == false )
			{
				Status = GetPhrase( "RedistDXRedistFail" );
			}

			return ( Status );
		}

		public string InstallAMDCPUDrivers( RedistProgress Progress, string SourceFolder )
		{
			string Status = "OK";

			// Check for Vista/XP
			if( GetOSVersionMajor() < 6 )
			{
				// Check for AMD processor
				string CPUType = "";
				RegistryKey Key = Registry.LocalMachine.OpenSubKey( "HARDWARE\\Description\\System\\CentralProcessor\\0" );
				if( Key != null )
				{
					CPUType = ( string )Key.GetValue( "ProcessorNameString" );
				}

				if( CPUType.ToUpper().StartsWith( "AMD" ) )
				{
					// Install the CPU drivers
					Progress.LabelDescription.Text = GetPhrase( "RedistAMDCPUHeader" );
					if( bStandAloneRedist || Manifest.RootName != "UDK" )
					{
						Progress.LabelDetail.Text = GetPhrase( "RedistAMDCPUContenttTech" );
					}
					else
					{
						Progress.LabelDetail.Text = GetPhrase( "RedistAMDCPUContent" );
					}

					Application.DoEvents();

					ProcessStartInfo StartInfo = new ProcessStartInfo( "AMD\\amdcpusetup.exe", "/s" );
					StartInfo.WorkingDirectory = SourceFolder;
					Process AMDCPURedist = Process.Start( StartInfo );
					if( WaitForProcess( AMDCPURedist, 120 ) == false )
					{
						Status = GetPhrase( "RedistAMDCPUFail" );
					}
				}
			}

			return ( Status );
		}
	}
}
