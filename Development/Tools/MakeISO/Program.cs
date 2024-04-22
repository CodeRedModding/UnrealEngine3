/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Runtime.InteropServices;
using System.Threading;
using IMAPI2FS;

namespace MakeISO
{
	partial class Program
	{
		[Flags]
		public enum StgmConstants
		{
			STGM_READ = 0x0,
			STGM_WRITE = 0x1,
			STGM_READWRITE = 0x2,
			STGM_SHARE_DENY_NONE = 0x40,
			STGM_SHARE_DENY_READ = 0x30,
			STGM_SHARE_DENY_WRITE = 0x20,
			STGM_SHARE_EXCLUSIVE = 0x10,
			STGM_PRIORITY = 0x40000,
			STGM_CREATE = 0x1000,
			STGM_CONVERT = 0x20000,
			STGM_FAILIFTHERE = 0x0,
			STGM_DIRECT = 0x0,
			STGM_TRANSACTED = 0x10000,
			STGM_NOSCRATCH = 0x100000,
			STGM_NOSNAPSHOT = 0x200000,
			STGM_SIMPLE = 0x8000000,
			STGM_DIRECT_SWMR = 0x400000,
			STGM_DELETEONRELEASE = 0x4000000
		}

		public const int FILE_ATTRIBUTE_NORMAL = 0x00000080;

		[DllImport( "shlwapi.dll", CharSet = CharSet.Unicode, PreserveSig = false )]
		static extern IStream SHCreateStreamOnFileEx( string pszFile, uint grfMode, uint dwAttributes, bool fCreate, IStream pstmTemplate );

		static int ReturnCode = 0;
		static public string SourceFolder = "";
		static public string DestFile = "";
		static public string VolumeName = "VOLUME";

		// Get a sorted list of all the path names in the source folder
		static private void GetPathNames( DirectoryInfo SourceFolderInfo, ref List<string> PathNames )
		{
			foreach( FileInfo Info in SourceFolderInfo.GetFiles() )
			{
				PathNames.Add( Info.FullName.Substring( SourceFolder.Length + 1 ) );
			}

			foreach( DirectoryInfo SubDirInfo in SourceFolderInfo.GetDirectories() )
			{
				GetPathNames( SubDirInfo, ref PathNames );
			}
		}

		// Extract all the required directory names
		static private List<string> GetDirectoryNames( List<string> PathNames )
		{
			List<string> DirectoryNames = new List<string>();

			foreach( string PathName in PathNames )
			{
				string DirName = Path.GetDirectoryName( PathName ).Replace( '/', '\\' );
				string[] DirNameComponents = DirName.Split( "\\".ToCharArray() );

				string FullDirName = "";
				foreach( string DirNameComp in DirNameComponents )
				{
					if( DirNameComp.Length > 0 )
					{
						FullDirName += DirNameComp;
						if( !DirectoryNames.Contains( FullDirName ) )
						{
							DirectoryNames.Add( FullDirName );
						}
						FullDirName += "\\";
					}
				}
			}

			return ( DirectoryNames );
		}

		static private bool CreateISO( List<string> DirectoryNames, List<string> PathNames )
		{
			try
			{
				IFileSystemImage DiscImage = new MsftFileSystemImage();

				DiscImage.ChooseImageDefaultsForMediaType( IMAPI_MEDIA_PHYSICAL_TYPE.IMAPI_MEDIA_TYPE_DISK );
				DiscImage.FileSystemsToCreate = FsiFileSystems.FsiFileSystemISO9660 | FsiFileSystems.FsiFileSystemJoliet;
				DiscImage.VolumeName = VolumeName;

				// Add in all the directories
				foreach( string DirName in DirectoryNames )
				{
					DiscImage.Root.AddDirectory( DirName );
				}

				Log( "Added " + DirectoryNames.Count.ToString() + " folders" );

				// Add in all the files
				foreach( string PathName in PathNames )
				{
					IMAPI2FS.IStream FileStream = ( IMAPI2FS.IStream )SHCreateStreamOnFileEx( Path.Combine( SourceFolder, PathName ), ( uint )( StgmConstants.STGM_READ ), FILE_ATTRIBUTE_NORMAL, false, null );
					DiscImage.Root.AddFile( PathName, ( FsiStream )FileStream );
				}

				Log( "Added " + PathNames.Count.ToString() + " files" );

				// Get stream that represents the ISO image
				IFileSystemImageResult PseudoImage = DiscImage.CreateResultImage();
				System.Runtime.InteropServices.ComTypes.IStream SourceStream = ( System.Runtime.InteropServices.ComTypes.IStream )PseudoImage.ImageStream;

				// Get a file stream to write to
				System.Runtime.InteropServices.ComTypes.IStream DestStream = ( System.Runtime.InteropServices.ComTypes.IStream )SHCreateStreamOnFileEx( DestFile, ( uint )( StgmConstants.STGM_CREATE | StgmConstants.STGM_WRITE ), FILE_ATTRIBUTE_NORMAL, false, null );

				System.Runtime.InteropServices.ComTypes.STATSTG Stats;
				SourceStream.Stat( out Stats, 0 );

				SourceStream.CopyTo( DestStream, Stats.cbSize, IntPtr.Zero, IntPtr.Zero );

				Log( "ISO created successfully!" );
				return ( true );
			}
			catch( Exception Ex )
			{
				Error( "Exception: " + Ex.ToString() );
			}

			return ( false );
		}

		/**
		 * Main control loop
		 */
		static int Main( string[] args )
		{
			if( !ParseCommandLine( args ) )
			{
				Log( "Usage: MakeISO -Source <SourceFolder> -Dest <DestFolder> -VolumeName <Name> [-FolderExt <Extension>]" );
				return ( 1 );
			}

			DirectoryInfo SourceFolderInfo = new DirectoryInfo( SourceFolder );
			if( !SourceFolderInfo.Exists )
			{
				Error( "Source folder does not exist! " + SourceFolder );
				return ( 2 );
			}

			Log( "Creating ISO with volume name: " + VolumeName );

			// Get a sorted list of all the source path names
			List<string> PathNames = new List<string>();
			SourceFolder = SourceFolderInfo.FullName;
			GetPathNames( SourceFolderInfo, ref PathNames );
			PathNames.Sort();

			// Get a sorted list of all the unique directories
			List<string> DirectoryNames = GetDirectoryNames( PathNames );

			// Create the ISO file
			CreateISO( DirectoryNames, PathNames );

			return ( ReturnCode );
		}

		static private void Log(string Line)
		{
			Console.WriteLine(Line);
		}

		static void Error(string Line)
		{
			Console.ForegroundColor = ConsoleColor.Red;
			Log("MAKEISO ERROR: " + Line);
			Console.ResetColor();
		}

		static private void Warning(string Line)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Log( "MAKEISO WARNING: " + Line );
			Console.ResetColor();
		}

		static private bool ParseCommandLine( string[] Args )
		{
			bool bSuccess = true;
			for( int ArgIndex = 0; ArgIndex < Args.Length - 1; ArgIndex += 2 )
			{
				switch( Args[ArgIndex].ToLower() )
				{
				case "-source":
					SourceFolder = Args[ArgIndex + 1];
					break;

				case "-dest":
					DestFile = Args[ArgIndex + 1];
					break;

				case "-volume":
					VolumeName = Args[ArgIndex + 1];
					break;
				}
			}

			if( SourceFolder.Length == 0 )
			{
				Error( "Source folder is required" );
				bSuccess = false;
			}

			if( DestFile.Length == 0 )
			{
				Error( "Destination file is required" );
				bSuccess = false;
			}

			return ( bSuccess );
		}
	}
}
