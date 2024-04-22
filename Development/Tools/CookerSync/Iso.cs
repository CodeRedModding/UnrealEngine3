// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Runtime.InteropServices;
using IMAPI2FS;

namespace CookerSync
{
	partial class CookerSyncApp
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

		// Get a sorted list of all the path names in the TOC
		static private List<string> GetPathNames( List<List<ConsoleInterface.TOCInfo>> TOCList )
		{
			List<string> PathNames = new List<string>();

			foreach( List<ConsoleInterface.TOCInfo> TOC in TOCList )
			{
				foreach( ConsoleInterface.TOCInfo Entry in TOC )
				{
					string FullFileName = Entry.FileName;
					if( FullFileName.StartsWith( "..\\" ) )
					{
						FullFileName = FullFileName.Substring( 3 );
					}

					PathNames.Add( FullFileName );
				}
			}

			PathNames.Sort();
			return ( PathNames );
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

		static public bool SaveIso( List<List<ConsoleInterface.TOCInfo>> TOCList, ConsoleInterface.TOCSettings BuildSettings )
		{
			string OldCWD = Environment.CurrentDirectory;
			Environment.CurrentDirectory = Path.Combine( Environment.CurrentDirectory, ".." );

			Log( Color.Green, "[SAVING TO ISO STARTED]" );
			DateTime StartTime = DateTime.UtcNow;

			// Get a sorted list of all the source path names
			List<string> PathNames = GetPathNames( TOCList );
			// Get a sorted list of all the unique directories
			List<string> DirectoryNames = GetDirectoryNames( PathNames );

			foreach( string IsoFileName in BuildSettings.IsoFiles )
			{
				IFileSystemImage DiscImage = new MsftFileSystemImage();

				DiscImage.ChooseImageDefaultsForMediaType( IMAPI_MEDIA_PHYSICAL_TYPE.IMAPI_MEDIA_TYPE_DISK );
				DiscImage.FileSystemsToCreate = FsiFileSystems.FsiFileSystemISO9660 | FsiFileSystems.FsiFileSystemJoliet;
				DiscImage.VolumeName = BuildSettings.GameName;

				// Add in all the directories
				foreach( string DirName in DirectoryNames )
				{
					Log( Color.Black, "Adding directory: " + DirName );

					DiscImage.Root.AddDirectory( DirName );
				}

				// Add in all the files
				foreach( string PathName in PathNames )
				{
					Log( Color.Black, "Adding file: " + PathName );

					FileInfo Info = new FileInfo( PathName );
					if( !Info.Exists )
					{
						Log( Color.Red, "SYNC ERROR: File in TOC does not exist on disk: " + PathName );
					}

					IMAPI2FS.IStream FileStream = ( IMAPI2FS.IStream )SHCreateStreamOnFileEx( PathName, ( uint )( StgmConstants.STGM_READ ), FILE_ATTRIBUTE_NORMAL, false, null );
					DiscImage.Root.AddFile( PathName, ( FsiStream )FileStream );
				}

				// Get stream that represents the ISO image
				IFileSystemImageResult PseudoImage = DiscImage.CreateResultImage();
				System.Runtime.InteropServices.ComTypes.IStream SourceStream = ( System.Runtime.InteropServices.ComTypes.IStream )PseudoImage.ImageStream;

				// Get a file stream to write to
				System.Runtime.InteropServices.ComTypes.IStream DestStream = ( System.Runtime.InteropServices.ComTypes.IStream )SHCreateStreamOnFileEx( IsoFileName, ( uint )( StgmConstants.STGM_CREATE | StgmConstants.STGM_WRITE ), FILE_ATTRIBUTE_NORMAL, false, null ); 

				System.Runtime.InteropServices.ComTypes.STATSTG Stats;
				SourceStream.Stat( out Stats, 0 );

				SourceStream.CopyTo( DestStream, Stats.cbSize, IntPtr.Zero, IntPtr.Zero );
			}

			TimeSpan Duration = DateTime.UtcNow.Subtract( StartTime );
			Log( Color.Green, "Operation took " + Duration.Minutes.ToString() + ":" + Duration.Seconds.ToString( "D2" ) );
			Log( Color.Green, "[SAVING TO ISO FINISHED]" );

			Environment.CurrentDirectory = OldCWD;
			return( true );
		}
	}
}
