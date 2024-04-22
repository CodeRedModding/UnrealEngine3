// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using Microsoft.Win32;

namespace Controller
{
    public partial class SandboxedAction
	{
		// To do:
		// Retrieve versions from database

		// To verify:
		// SQL server version
		// - MSVC settings
		// - branch names start with UnrealEngine3
		// - check OS version (Windows 7 SP1 required)

		private bool ValidateEnvVarFolder( string EnvVar )
		{
			Builder.Write( "Checking: " + EnvVar );
			string Folder = Environment.GetEnvironmentVariable( EnvVar );
			if( Folder != null )
			{
				DirectoryInfo DirInfo = new DirectoryInfo( Folder );
				if( !DirInfo.Exists )
				{
					Builder.Write( "VALIDATION ERROR: folder does not exist: " + Folder );
					return ( false );
				}

				return ( true );
			}

			Builder.Write( "VALIDATION ERROR: Failed to find environment variable: " + EnvVar );
			return ( false );
		}

		private bool ValidateOSVersion()
		{
			OperatingSystem OSVersion = Environment.OSVersion;

			if( OSVersion.Version.Major < 6 )
			{
				Builder.Write( "VALIDATION ERROR: Operating systems before Windows 7 are not supported" );
				return ( false );
			}
			else if( OSVersion.Version.Minor < 1 )
			{
				// Required for the database connectivity on Live (MS changed the API)
				Builder.Write( "VALIDATION ERROR: Windows 7 must be at least Service Pack 1" );
				return ( false );
			}

			return ( true );
		}

		private bool ValidateTimeZone()
		{
			TimeZone LocalZone = TimeZone.CurrentTimeZone;

			// This can be any time zone you like, but must be the same across all build machines
			// Everything is internally UTC, except for label name generation (and hence folder name generation)
			if( LocalZone.StandardName != "Eastern Standard Time" )
			{
				Builder.Write( "VALIDATION ERROR: TimeZone is not EST" );
				return ( false );
			}

			return ( true );
		}

		// Check p4win
		private bool ValidateP4()
		{
			if( !SCC.ValidateP4Settings( Builder ) )
			{
				return ( false );
			}

			Builder.Write( "Perforce settings validated" );
			return ( true );
		}

		// DX version matches SQL entry
		private bool ValidateDX()
		{
			string DXFolder = "C:\\Program Files (x86)\\Microsoft DirectX SDK (June 2010)";
			DirectoryInfo DirInfo = new DirectoryInfo( DXFolder );
			if( !DirInfo.Exists )
			{
				Builder.Write( "DX not installed: " + DXFolder );
				return ( false );
			}

			Builder.Write( "DirectX version validated as June 2010" );
			return ( true );
		}

		// XDK matches SQL entry
		private bool ValidateXDK()
		{
			if( !ValidateEnvVarFolder( "XEDK" ) )
			{
				return ( false );
			}

			if( Parent.Xbox360SDKVersion != "21173" && Parent.Xbox360SDKVersion != "21076" )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find correct version of the XDK." );
				return ( false );
			}

			Builder.Write( "XDK validated as 21173, or 21076" );
			return ( true );
		}

		// Flash SDK matches SQL entry
		private bool ValidateFlash()
		{
			if( !ValidateEnvVarFolder( "ALCHEMY_ROOT" ) )
			{
				return ( false );
			}

			if( Parent.FlashSDKVersion != "1101086" )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find correct version of the Flash SDK." );
				return ( false );
			}

			Builder.Write( "Flash SDK validated as 1101086" );
			return ( true );
		}

		// Make sure GFWL SDK installed and is the correct version
		private bool ValidateGFWL()
		{
			if( !ValidateEnvVarFolder( "GFWLSDK_DIR" ) )
			{
				return ( false );
			}

			FileInfo Info = new FileInfo( Path.Combine( Environment.GetEnvironmentVariable( "GFWLSDK_DIR" ), "bin\\x86\\security-enabled\\xlive.dll" ) );

			Version ReferenceVersion = new Version( 3, 5, 88, 0 );
			FileVersionInfo VersionInfo = FileVersionInfo.GetVersionInfo( Info.FullName );
			Version GFWLVersion = new Version( VersionInfo.FileMajorPart, VersionInfo.FileMinorPart, VersionInfo.FileBuildPart, VersionInfo.FilePrivatePart );

			if( GFWLVersion != ReferenceVersion )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find correct version of xlive.dll; found version : " + VersionInfo.FileVersion );
				return ( false );
			}

			Builder.Write( "GFWL validated" );
			return ( true );
		}

		// Make sure Perl is installed for the source server
		private bool ValidatePerl()
		{
			DirectoryInfo Dir32Info = new DirectoryInfo( "C:\\Perl\\Bin" );
			DirectoryInfo Dir64Info = new DirectoryInfo( "C:\\Perl64\\Bin" );

			FileInfo PerlBinaryFileInfo = null;

			// Check version
			if( Dir32Info.Exists )
			{
				PerlBinaryFileInfo = new FileInfo( "C:\\Perl\\Bin\\Perl.exe" );
				if( !PerlBinaryFileInfo.Exists )
				{
					Builder.Write( "VALIDATION ERROR: Failed to find Perl at C:\\Perl\\Bin\\Perl.exe" );
					return ( false );
				}
			}
			else if( Dir64Info.Exists )
			{
				PerlBinaryFileInfo = new FileInfo( "C:\\Perl64\\Bin\\Perl.exe" );
				if( !PerlBinaryFileInfo.Exists )
				{
					Builder.Write( "VALIDATION ERROR: Failed to find Perl at C:\\Perl64\\Bin\\Perl.exe" );
					return ( false );
				}
			}
			else
			{
				Builder.Write( "VALIDATION ERROR: Failed to find Perl at either C:\\Perl\\Bin or C:\\Perl64\\Bin" );
				return ( false );
			}

			Version ReferenceVersionA = new Version( 5, 8, 8, 819 );
			Version ReferenceVersionB = new Version( 5, 10, 1, 1006 );
			FileVersionInfo VersionInfo = FileVersionInfo.GetVersionInfo( PerlBinaryFileInfo.FullName );
			Version PerlVersion = new Version( VersionInfo.FileMajorPart, VersionInfo.FileMinorPart, VersionInfo.FileBuildPart, VersionInfo.FilePrivatePart );

			if( PerlVersion != ReferenceVersionA && PerlVersion != ReferenceVersionB )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find correct version of Perl.exe; found version : " + VersionInfo.FileVersion );
				return ( false );
			}

			Builder.Write( "Perl validated" );
			return ( true );
		}

		// Make sure Debugging tools for windows is installed for the source and symbol server
		private bool ValidateDebuggingTools()
		{
			// Check for x86 version
			// TODO: check for x64 also
			FileInfo SourceServerCmdInfo = new FileInfo( Builder.ToolConfig.SourceServerCmd );
			if( !SourceServerCmdInfo.Exists )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find source server command: " + Builder.ToolConfig.SourceServerCmd );
				return ( false );
			}

			Builder.Write( "Debugging Tools for Windows validated" );
			return ( true );
		}

		// Make sure we have ILMerge installed for UnSetup
		private bool ValidateILMerge()
		{
			FileInfo ILMerge = new FileInfo( "C:\\Program Files (x86)\\Microsoft\\ILMerge\\Ilmerge.exe" );
			if( !ILMerge.Exists )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find ILMerge: " + ILMerge.FullName );
				return ( false );
			}

			Builder.Write( "ILMerge validated" );
			return ( true );
		}
	
		// Make sure we have the signing tools available and our certificate for signing
		private bool ValidateSigningKey()
		{
			FileInfo SignToolNameInfo = new FileInfo( Builder.ToolConfig.SignToolName );
			if( !SignToolNameInfo.Exists )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find signtool: " + Builder.ToolConfig.SignToolName );
				return ( false );
			}

			FileInfo CatToolNameInfo = new FileInfo( Builder.ToolConfig.CatToolName );
			if( !CatToolNameInfo.Exists )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find cat: " + Builder.ToolConfig.CatToolName );
				return ( false );
			}

			// Check to see if we have the pfx is installed
			X509Certificate2Collection CertificateCollection;
			X509Store CertificateStore = new X509Store( StoreLocation.CurrentUser );
			try
			{
				// create and open store for read-only access
				CertificateStore.Open( OpenFlags.ReadOnly );

				// search store for Epic cert
				CertificateCollection = CertificateStore.Certificates.Find( X509FindType.FindBySerialNumber, "3E1338505D2C3D34E2DF71AE64C5C24F", true );
				if( CertificateCollection.Count == 0 )
				{
					Builder.Write( "VALIDATION ERROR: Failed to find certificate" );
					return ( false );
				}
			}
			// always close the store
			finally
			{
				CertificateStore.Close();
			}

			Builder.Write( "Signing tools validated" );
			return ( true );
		}

		// Make sure we have ILMerge installed for UnSetup
		private bool ValidateAndroidSDKInstall()
		{
			DirectoryInfo AndroidSDKDirectory = new DirectoryInfo( "C:\\Android" );
			if( !AndroidSDKDirectory.Exists )
			{
				Builder.Write( "VALIDATION ERROR: Failed to find Android SDK install: " + AndroidSDKDirectory.FullName );
				return ( false );
			}

			Builder.Write( "Android SDK install validated" );
			return ( true );
		}

		public MODES ValidateInstall()
		{
			string LogFileName = Builder.GetLogFileName( COMMANDS.ValidateInstall );
			Builder.OpenLog( LogFileName, false );
			bool Validated = true;

			try
			{
				Validated &= ValidateOSVersion();
				Validated &= ValidateTimeZone();
				Validated &= ValidateP4();
				Validated &= ValidateDX();
				Validated &= ValidateXDK();
				Validated &= ValidateFlash();
				Validated &= ValidateGFWL();
				Validated &= ValidatePerl();
				Validated &= ValidateDebuggingTools();
				Validated &= ValidateILMerge();
				Validated &= ValidateSigningKey();
				Validated &= ValidateAndroidSDKInstall();
			}
			catch( Exception Ex )
			{
				Builder.Write( "VALIDATION ERROR: unhandled exception" );
				Builder.Write( "Exception: " + Ex.Message );
				Validated = false;
			}

			if( Validated )
			{
				Builder.Write( "Validation completed successfully!" );
			}
			else
			{
				Builder.Write( "VALIDATION ERROR: Install validation failed!" );
			}

			Builder.CloseLog();

			if( !Validated )
			{
				State = COMMANDS.ValidateInstall;
			}

			return MODES.Finalise;
		}
	}
}
