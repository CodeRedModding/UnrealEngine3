/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Security.Cryptography.X509Certificates;
using System.Text;
using System.Text.RegularExpressions;
using System.Windows.Forms;
using System.Xml;
using System.Xml.Serialization;
using Microsoft.Win32;
using Ionic.Zip;

namespace UnSetup
{
	public partial class Utils
	{
		[DllImport( "user32.dll" )]
		static extern IntPtr SendMessage( IntPtr hWnd, UInt32 Msg, IntPtr wParam, IntPtr lParam );

		public const Int32 WM_CLOSE = 0x0010;

		public static string GameDefaultName = "MyGame";
		public static string GameDefaultLongName = "My Game Long Name";

		public enum BUILDCOMMANDS
		{
			None,
			CreateManifest,
			GameCreateManifest,
			BuildInstaller,
			BuildGameInstaller,
			Package,
			PackageRedist,
#if DEBUG
			UnPackage
#endif
		}

		public enum COMMANDS
		{
			Help,
			EULA,
			Manifest,
			Game,
			SetupInstall,
			Install,
			SetupUninstall,
			Uninstall,
			Redist,
			HandleRedist,
			MakeShortcuts,
#if DEBUG
			Extract,
			Subscribe,
			CheckSignature,
			Shortcuts
#endif
		}

		/*
		 * The Guid to get the uninstall location in the registry
		 */
		public class InstallInfo
		{
			[XmlElement]
			public string InstallGuidString = "";
		}

		/*
		 * Game specific install info
		 */
		public class GameManifestOptions
		{
			// App to launch at the end of the install process to create ini files
			[XmlElementAttribute]
			public string AppToCreateInis { get; set; }

			// App to optionally launch after the install process
			[XmlElementAttribute]
			public string AppToElevate { get; set; }

			// Command line of the above app
			[XmlElementAttribute]
			public string AppCommandLine { get; set; }

			// Full descriptive name of the application
			[XmlElementAttribute]
			public string Name { get; set; }

			public GameManifestOptions()
			{
				AppToCreateInis = "Win32\\UDK.exe";
				AppToElevate = "UDKLift.exe";
				AppCommandLine = "editor";
				Name = "Unreal Development Kit";
			}
		}

		/*
		 * Details about the links to install
		 */
		public class LinkShortcutOptions
		{
			[XmlElementAttribute]
			public string DisplayPath { get; set; }

			[XmlElementAttribute]
			public string UrlFilePath { get; set; }

			[XmlElementAttribute]
			public string Name { get; set; }

			public LinkShortcutOptions()
			{
				DisplayPath = "";
				UrlFilePath = "";
				Name = "";
			}
		}

		/*
		 * Which files to include/exclude from the UDK/Game
		 */
		public class ManifestOptions
		{
			[CategoryAttribute( "InstallInfo" )]
			[DescriptionAttribute( "The root name of the installer to be created. This will be the default install folder and the prefix to the package name." )]
			[XmlElementAttribute]
			public string RootName { get; set; }

			[CategoryAttribute( "InstallInfo" )]
			[DescriptionAttribute( "The descriptive name to be used for the shortcuts folder, ARP display name and in more descriptive dialogs." )]
			[XmlElementAttribute]
			public string FullName { get; set; }

			[CategoryAttribute( "InstallInfo" )]
			[DescriptionAttribute( "The application to launch on game install completion." )]
			[XmlElementAttribute]
			public string AppToLaunch { get; set; }

			[CategoryAttribute( "InstallInfo" )]
			[DescriptionAttribute( "Whether to show the email subscription option on the install options page." )]
			[XmlElementAttribute]
			public bool ShowEmailSubscription { get; set; }

			[CategoryAttribute( "InstallInfo" )]
			[XmlElement]
			public List<LinkShortcutOptions> LinkShortcuts { get; set; }

			[CategoryAttribute( "GameInstallInfo" )]
			[XmlElement]
			public List<GameManifestOptions> GameInfo { get; set; }

			[CategoryAttribute( "FileManifests" )]
			[DescriptionAttribute( "The files to exclude from original the build." )]
			[XmlArrayAttribute]
			public string[] MainFilesToExclude { get; set; }

			[CategoryAttribute( "FileManifests" )]
			[DescriptionAttribute( "The files to EXCLUDE from the game build that were in the original build." )]
			[XmlArrayAttribute]
			public string[] GameFilesToExclude { get; set; }

			[CategoryAttribute( "FileManifests" )]
			[DescriptionAttribute( "The folders that are required to exist for the game to package properly." )]
			[XmlArrayAttribute]
			public string[] RequiredFolders { get; set; }

			[CategoryAttribute( "FileManifests" )]
			[DescriptionAttribute( "The files to INCLUDE for the game build." )]
			[XmlArrayAttribute]
			public string[] GameFilesToInclude { get; set; }

			public ManifestOptions()
			{
				RootName = "UDK";
				FullName = "Unreal Development Kit";
				AppToLaunch = "Win32\\UDK.exe";
				ShowEmailSubscription = true;

				LinkShortcuts = new List<LinkShortcutOptions>();
				GameInfo = new List<GameManifestOptions>() { };

				MainFilesToExclude = new string[] { };
				GameFilesToExclude = new string[] { };
				RequiredFolders = new string[] { };
				GameFilesToInclude = new string[] { };
			}
		}

		/*
		 *	The information required to describe a packaged game
		 */
		public class GameOptions
		{
			[CategoryAttribute( "GameSettings" )]
			[DescriptionAttribute( "The abbreviated name or acronym of the game. The installer will make a file UDKInstall-Game.exe." )]
			[XmlAttribute]
			public string GameName { get; set; }

			[CategoryAttribute( "GameSettings" )]
			[DescriptionAttribute( "The long name used in the start menu shortcut entry. The abbreviated name is used by default." )]
			[XmlAttribute]
			public string GameLongName { get; set; }

			[XmlElement]
			public Guid GameUniqueID = Guid.Empty;

			[XmlElement]
			public Guid MachineUniqueID = Guid.Empty;

			public GameOptions()
			{
				GameName = GameDefaultName;
				GameLongName = GameDefaultLongName;
			}
		}

		public string ManifestFileName = "Binaries\\InstallData\\Manifest.xml";
		public string GameManifestFileName = "Binaries\\InstallData\\GameManifest.xml";

		public string BackupDataDirectory = "UDKGame\\ProjectTemplates\\BaseTemplate\\";

		public X509Certificate EpicCertificate = null;
		public BUILDCOMMANDS BuildCommand = BUILDCOMMANDS.None;
		public COMMANDS Command = COMMANDS.Install;
		public string PackageInstallLocation = Application.StartupPath;
		public string MainModuleName = "";
		public string SubscribeEmail = "";
		public string UnSetupTimeStamp = "";
		public string UnSetupVersionString = "";
		public string GameName = "UDK";
		public string PlatformName = "PC";
		public bool bInstalledBuild = false;
		public bool bStandAloneRedist = false;
		public bool bDependenciesSuccessful = false;
		public bool bSkipDependencies = false;
		public bool bProgressOnly = false;
		public bool bIsCustomProject = false;
		private string SplashHandle = "";

		public string InstallFolder = "";
		public string InstallerRoot = "Game";
		public ManifestOptions Manifest = null;
		public GameOptions Game = null;
		public string CustomProjectShortName = "Custom";

		public InstallInfo InstallInfoData = null;
		public Localise Phrases = null;
		public ZipFile MainZipFile = null;
		private FileStream ZipStream = null;

		private ProgressBar Progress = null;
		private long TotalUncompressedSize = 0;
		private long CurrentDecompressedSize = 0;
		private List<byte[]> ValidSerialNumbers = new List<byte[]>();


		public Utils( string Language, string ParentLanguage )
		{
			// Init the localisation system
			Phrases = new Localise();
			Phrases.Init( Language, ParentLanguage );

			// The the digital signature of this file
			GetCertificate();

			// Serial numbers of X509 digital signatures that we allow to be packaged
			ValidSerialNumbers.Add( new byte[] { 0xdf, 0xad, 0xec, 0x28, 0x52, 0x05, 0x10, 0x04, 0x58, 0x99, 0x76, 0x90, 0xa6, 0x20, 0xf8, 0x3a } );	// Nvidia1
			ValidSerialNumbers.Add( new byte[] { 0x31, 0xb0, 0xf8, 0x84, 0xdb, 0x2d, 0xd1, 0x0d, 0x84, 0xd9, 0x56, 0xbe, 0xd0, 0xbe, 0x4a, 0x53 } );	// Nvidia2
			ValidSerialNumbers.Add( new byte[] { 0xf5, 0x09, 0x03, 0xd0, 0xe1, 0x39, 0xd8, 0x6d, 0x28, 0x66, 0x98, 0x60, 0x7d, 0x43, 0xbb, 0x43 } );	// Nvidia3
			ValidSerialNumbers.Add( new byte[] { 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x4d, 0x78, 0x0f, 0x61 } );										// Microsoft1
			ValidSerialNumbers.Add( new byte[] { 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x81, 0x27, 0x06, 0x61 } );										// Microsoft2
			ValidSerialNumbers.Add( new byte[] { 0x0f, 0x00, 0x00, 0x00, 0x00, 0x00, 0x3e, 0xcf, 0x01, 0x61 } );										// Microsoft3
			ValidSerialNumbers.Add( new byte[] { 0x15, 0x00, 0x00, 0x00, 0x00, 0x00, 0x9b, 0xb2, 0x01, 0x61 } );										// Microsoft4
			ValidSerialNumbers.Add( new byte[] { 0x27, 0x6a, 0x14, 0x28, 0x35, 0x15, 0xc1, 0x91, 0x1f, 0xfe, 0x81, 0xe3, 0x35, 0x75, 0x69, 0x68 } );	// Valve
			ValidSerialNumbers.Add( new byte[] { 0x1e, 0x5b, 0xe6, 0x8f, 0x86, 0xd1, 0xf2, 0x0b, 0x34, 0xec, 0xcb, 0x70, 0x6f, 0x32, 0xf6, 0x7b } );	// Valve
			ValidSerialNumbers.Add( new byte[] { 0xd6, 0x04, 0x39, 0x2f, 0x6f, 0xc9, 0x23, 0xf0, 0x20, 0x20, 0x63, 0x96, 0xe1, 0x9b, 0x59, 0x19 } );	// AMD
			ValidSerialNumbers.Add( new byte[] { 0xc8, 0x01, 0x00, 0x00, 0x00, 0x00, 0xab, 0x25, 0x95, 0x47 } );										// Intel
			ValidSerialNumbers.Add( new byte[] { 0x1c, 0x24, 0x63, 0xc1, 0xfe, 0x2e, 0x15, 0x78, 0x33, 0xc4, 0xe1, 0xae, 0xda, 0xc6, 0xc4, 0x64 } );	// Imagination Technologies 
			ValidSerialNumbers.Add( new byte[] { 0x50, 0xda, 0xaf, 0x3d, 0x3d, 0xa5, 0xcf, 0x55, 0x25, 0x2d, 0x91, 0xdc, 0x64, 0x30, 0x20, 0x17 });	    // Imagination Technologies 2
			ValidSerialNumbers.Add( new byte[] { 0x94, 0x42, 0x40, 0xdc, 0xa5, 0x42, 0x98, 0xfa, 0x2f, 0xc3, 0x50, 0x39, 0x78, 0x43, 0xb7, 0x00 });		// Perforce
			ValidSerialNumbers.Add( new byte[] { 0x42, 0x58, 0xa1, 0xd9, 0x82, 0x4b, 0x70, 0xe5, 0x07, 0x19, 0x96, 0xd8, 0xda, 0xcd, 0x16, 0x1c } );	// Epic Games Inc. (old)

			// Comment this line in if you sign UnSetup with your own digital signature
			// ValidSerialNumbers.Add( new byte[] { 0x4f, 0xc2, 0xc5, 0x64, 0xae, 0x71, 0xdf, 0xe2, 0x34, 0x3d, 0x2c, 0x5d, 0x50, 0x38, 0x13, 0x3e };	// Epic Games Inc. (new)
		}

		/***
			* Setup our application information
			* ***/
		private void InitApplicationInfo()
		{
#if DEBUG
			Environment.CurrentDirectory = "D:\\depot\\UnrealEngine3";
			InstallFolder = Environment.CurrentDirectory + "\\Binaries\\";
			MainModuleName = Environment.CurrentDirectory + "\\Binaries\\UnSetup.exe";
			ReadGame(Environment.CurrentDirectory + "\\Binaries\\");
			ReadInstallInfo(Environment.CurrentDirectory + "\\Binaries\\");
#else
			InstallFolder = Application.StartupPath;
			MainModuleName = Application.ExecutablePath;
			ReadGame( Application.StartupPath );
			ReadInstallInfo( Application.StartupPath );
#endif
		}

		private void InitVersionString()
		{
			if( bStandAloneRedist )
			{
				UnSetupVersionString = " UE3Redist";
			}
			else
			{
				// Create a string based on the year and month extracted from the version
				System.Version Version = System.Reflection.Assembly.GetExecutingAssembly().GetName().Version;
				DateTime CompileTime = DateTime.Parse( "01/01/2000" ).AddDays( Version.Build ).AddSeconds( Version.Revision * 2 );
				UnSetupTimeStamp = CompileTime.Year.ToString() + "-" + CompileTime.Month.ToString( "00" );
				UnSetupVersionString = " " + InstallerRoot + "-" + UnSetupTimeStamp;
			}
		}

		public void Destroy()
		{
			Phrases.Destroy();
		}

		public class FileProperties
		{
			[XmlAttribute]
			public string FileName;
			[XmlAttribute]
			public long Size;

			public FileProperties()
			{
				FileName = "";
				Size = 0;
			}

			public FileProperties( string InName, long InSize )
			{
				FileName = InName;
				Size = InSize;
			}
		}

		public class FolderProperties
		{
			[XmlAttribute]
			public string FolderName;
			[XmlArray]
			public List<FolderProperties> Folders;
			[XmlArray]
			public List<FileProperties> Files;
			[XmlIgnore]
			public long Size;

			public FolderProperties()
			{
				FolderName = ".";
				Folders = new List<FolderProperties>();
				Files = new List<FileProperties>();
				Size = 0;
			}

			public FolderProperties( string InFolder )
			{
				FolderName = InFolder.TrimStart( "\\".ToCharArray() );
				Folders = new List<FolderProperties>();
				Files = new List<FileProperties>();
				Size = 0;
			}

			public void AddFolder( FolderProperties Folder )
			{
				Folders.Add( Folder );
			}

			public void AddFile( FileProperties File )
			{
				Files.Add( File );
			}

			// Find a folder in this folder's subfolders
			public FolderProperties FindFolder( string FolderName )
			{
				FolderProperties Folder = null;
				foreach( FolderProperties FolderPropterty in Folders )
				{
					if( string.Compare( FolderPropterty.FolderName, FolderName, true ) == 0 )
					{
						Folder = FolderPropterty;
					}
				}

				return ( Folder );
			}

			// Find a file in this folder's files
			public FileProperties FindFile( string FileName )
			{
				FileProperties File = null;
				foreach( FileProperties FilePropterty in Files )
				{
					if( string.Compare( FilePropterty.FileName, FileName, true ) == 0 )
					{
						File = FilePropterty;
					}
				}

				return ( File );
			}

			// Recursive function to return all files in FolderName and below
			public void FindAllFiles( string FolderName, Regex RX, List<string> AllowedBaseFolders )
			{
				DirectoryInfo DirInfo = new DirectoryInfo( FolderName );

				// The root of a drive contains a "." folder like all others, but unlike all the others, it's hidden
				bool bIsDirHidden = ( ( DirInfo.Attributes & FileAttributes.Hidden ) == FileAttributes.Hidden ) && ( FolderName != "." );
				if( DirInfo.Exists && !bIsDirHidden )
				{
					foreach( DirectoryInfo Dir in DirInfo.GetDirectories() )
					{
						if( AllowedBaseFolders == null || AllowedBaseFolders.Contains( Dir.Name.ToLower() ) )
						{
							FolderProperties Folder = new FolderProperties( Dir.Name );
							AddFolder( Folder );

							Folder.FindAllFiles( FolderName + "\\" + Dir.Name, RX, null );
						}
					}

					if( AllowedBaseFolders == null || AllowedBaseFolders.Contains( DirInfo.Name.ToLower() ) )
					{
						foreach( FileInfo File in DirInfo.GetFiles() )
						{
							bool bIsFileInvalid = ( File.Attributes & FileAttributes.Hidden ) == FileAttributes.Hidden;

							// Only add the file if it matches the regexp
							if( RX != null )
							{
								Match RegExpMatch = RX.Match( File.Name );
								// If it doesn't match, mark the file as invalid
								bIsFileInvalid |= !RegExpMatch.Success;
							}

							if( !bIsFileInvalid )
							{
								AddFile( new FileProperties( File.Name, File.Length ) );
							}
						}
					}
				}
			}

			// Recursive function to add all files to a zip
			public void ZipFiles( string FolderName, ZipFile Zip )
			{
				foreach( FileProperties File in Files )
				{
					string RelativeName = FolderName + "\\" + File.FileName;
					FileInfo Info = new FileInfo( RelativeName );
					if( Info.Exists )
					{
						Info.IsReadOnly = false;
						Zip.UpdateFile( RelativeName );
						Program.Util.TotalUncompressedSize += Info.Length;
					}
				}

				foreach( FolderProperties Folder in Folders )
				{
					Folder.ZipFiles( FolderName + "\\" + Folder.FolderName, Zip );
				}
			}

			// Recursive function to delete all files in a tree
			public void DeleteFiles( string FolderName )
			{
				foreach( FolderProperties Folder in Folders )
				{
					Folder.DeleteFiles( FolderName + "\\" + Folder.FolderName );
					Application.DoEvents();
				}

				bool Retry;
				do
				{
					Retry = false;
					foreach( FileProperties File in Files )
					{
						FileInfo Info = null;
						try
						{
							if( File.Size > -1 )
							{
								Info = new FileInfo( FolderName + "\\" + File.FileName );
								if( Info.Exists )
								{
									Info.IsReadOnly = false;
									Info.Delete();
								}
							}
						}
						catch( Exception )
						{
							string Description = Info.FullName + Environment.NewLine + Environment.NewLine;
							Description += Program.Util.GetPhrase( "UnableDeleteFile" ) + Environment.NewLine;
							Description += Program.Util.GetPhrase( "DeleteRetry" );
							GenericQuery Query = new GenericQuery( "GQCaptionDelFileFail", Description, true, "GQSkip", true, "GQRetry" );
							Query.ShowDialog();
							Retry = ( Query.DialogResult == DialogResult.OK );
						}
					}
				}
				while( Retry );

				Application.DoEvents();

				do
				{
					Retry = false;

					DirectoryInfo DirInfo = new DirectoryInfo( FolderName );
					if( DirInfo.Exists && DirInfo.GetDirectories().Length == 0 && DirInfo.GetFiles().Length == 0 )
					{
						try
						{
							DirInfo.Delete();
						}
						catch( Exception )
						{
							string Description = DirInfo.FullName + Environment.NewLine + Environment.NewLine;
							Description += Program.Util.GetPhrase( "UnableDeleteFolder" ) + Environment.NewLine;
							Description += Program.Util.GetPhrase( "DeleteRetry" );
							GenericQuery Query = new GenericQuery( "GQCaptionDelFolderFail", Description, true, "GQSkip", true, "GQRetry" );
							Query.ShowDialog();
							Retry = ( Query.DialogResult == DialogResult.OK );
						}
					}
				}
				while( Retry );
			}

			// Recursive function to filter out all unsigned binaries
			public void FilterUnsignedBinaries( Utils Util, string FolderName )
			{
				foreach( FolderProperties Folder in Folders )
				{
					if( Folder.Size >= 0 )
					{
						Folder.FilterUnsignedBinaries( Util, FolderName + "\\" + Folder.FolderName );
					}
				}

				foreach( FileProperties File in Files )
				{
					if( File.Size >= 0 )
					{
						FileInfo Info = new FileInfo( FolderName + "\\" + File.FileName );
						if( Info.Extension.ToLower() == ".exe" || Info.Extension.ToLower() == ".dll" )
						{
							if( !Util.ValidateCertificate( Info.FullName ) )
							{
#if DEBUG
								Console.WriteLine( "WARNING: Would filter unsigned: " + Info.FullName );
#else
								File.Size = -1;
#endif
							}
						}
					}
				}
			}

			// Recursive function to delete all shortcuts that point to our install location
			public void FilterUDKShortcuts( Utils Util, string FolderName )
			{
				string ShortcutFolder = "";

				foreach( FolderProperties Folder in Folders )
				{
					string ChildFolderName = FolderName + "\\" + Folder.FolderName;
					DirectoryInfo DirInfo = new DirectoryInfo( ChildFolderName );
					if( DirInfo.Exists )
					{
						Folder.FilterUDKShortcuts( Util, ChildFolderName );
					}
				}

				Application.DoEvents();

				foreach( FileProperties File in Files )
				{
					FileInfo Info = new FileInfo( FolderName + "\\" + File.FileName );
					if( Info.Exists && Info.Extension.ToLower() == ".lnk" )
					{
						if( Util.AnalyseShortcut( Info.FullName ) )
						{
							ShortcutFolder = Info.DirectoryName;
							File.Size = -1;

							try
							{
								Info.IsReadOnly = false;
								Info.Delete();
							}
							catch( Exception )
							{
							}
						}
					}
				}

				if( ShortcutFolder.Length > 0 )
				{
					DirectoryInfo DirInfo = new DirectoryInfo( ShortcutFolder );
					if( DirInfo.Exists && DirInfo.GetDirectories().Length == 0 && DirInfo.GetFiles().Length == 0 )
					{
						try
						{
							DirInfo.Delete();
						}
						catch( Exception )
						{
						}
					}
				}
			}

			// Remove all the folders and files that have been marked for delete (size set to -1)
			public bool Clean()
			{
				List<FolderProperties> CleanFolders = new List<FolderProperties>();

				foreach( FolderProperties FP in Folders )
				{
					if( FP.Size >= 0 )
					{
						if( FP.Clean() )
						{
							CleanFolders.Add( FP );
						}
					}
				}

				List<FileProperties> CleanFiles = new List<FileProperties>();

				foreach( FileProperties FP in Files )
				{
					if( FP.Size >= 0 )
					{
						CleanFiles.Add( FP );
					}
				}

				Folders = CleanFolders;
				Files = CleanFiles;

				if( Folders.Count == 0 && Files.Count == 0 )
				{
					return ( false );
				}

				return ( true );
			}

			// Returns a count of the number of folders in the tree
			public int FolderCount()
			{
				int FolderCount = 1;

				foreach( FolderProperties Folder in Folders )
				{
					FolderCount += Folder.FolderCount();
				}

				return ( FolderCount );
			}

			// Returns a count of the number of files in the tree
			public int FileCount()
			{
				int FileCount = Files.Count;

				foreach( FolderProperties Folder in Folders )
				{
					FileCount += Folder.FileCount();
				}

				return ( FileCount );
			}

			/// <summary>
			/// Accumulates and returns the size of all files in this folder and sub folders that are not marked for removal(size -1).
			/// </summary>
			/// <returns>
			///   <c>true</c> if there were no errors encountered extracting files; otherwise, <c>false</c>.
			/// </returns>
			public UInt64 GetFolderSize()
			{
				return GetFolderSizeRecursive( this );
			}

			/// <summary>
			/// Recursively iterates through the file list and all sub folders to calculate the size of all files that are not marked for removal(size -1).
			/// </summary>
			/// <param name="InFolderProperties">The folder to calculate size for.</param>
			/// <returns>
			///   <c>true</c> if there were no errors encountered extracting files; otherwise, <c>false</c>.
			/// </returns>
			private UInt64 GetFolderSizeRecursive( FolderProperties InFolderProperties )
			{
				UInt64 ReturnResult = 0;
				foreach( FileProperties File in InFolderProperties.Files )
				{
					if( File.Size != -1 )
					{
						ReturnResult += ( UInt64 )File.Size;
					}
				}

				foreach( FolderProperties Folder in InFolderProperties.Folders )
				{
					if( Folder.Size != -1 )
					{
						ReturnResult += GetFolderSizeRecursive( Folder );
					}
				}
				return ReturnResult;
			}
		}

		protected void XmlSerializer_UnknownAttribute( object sender, XmlAttributeEventArgs e )
		{
		}

		protected void XmlSerializer_UnknownNode( object sender, XmlNodeEventArgs e )
		{
		}

		private T ReadXml<T>( string FileName ) where T : new()
		{
			T Instance = new T();
			try
			{
				using( Stream XmlStream = new FileStream( FileName, FileMode.Open, FileAccess.Read, FileShare.None ) )
				{
					// Creates an instance of the XmlSerializer class so we can read the settings object
					XmlSerializer ObjSer = new XmlSerializer( typeof( T ) );
					// Add our callbacks for a busted XML file
					ObjSer.UnknownNode += new XmlNodeEventHandler( XmlSerializer_UnknownNode );
					ObjSer.UnknownAttribute += new XmlAttributeEventHandler( XmlSerializer_UnknownAttribute );

					// Create an object graph from the XML data
					Instance = ( T )ObjSer.Deserialize( XmlStream );
				}
			}
			catch( Exception E )
			{
				Console.WriteLine( E.Message );
			}

			return ( Instance );
		}

		private bool WriteXml<T>( string FileName, T Instance )
		{
			// Make sure the file we're writing is actually writable
			FileInfo Info = new FileInfo( FileName );
			if( Info.Exists )
			{
				Info.IsReadOnly = false;
			}

			// Write out the xml stream
			Stream XmlStream = null;
			try
			{
				using( XmlStream = new FileStream( FileName, FileMode.Create, FileAccess.Write, FileShare.None ) )
				{
					XmlSerializer ObjSer = new XmlSerializer( typeof( T ) );

					// Add our callbacks for a busted XML file
					ObjSer.UnknownNode += new XmlNodeEventHandler( XmlSerializer_UnknownNode );
					ObjSer.UnknownAttribute += new XmlAttributeEventHandler( XmlSerializer_UnknownAttribute );

					ObjSer.Serialize( XmlStream, Instance );
				}
			}
			catch( Exception E )
			{
				Console.WriteLine( E.ToString() );
				return ( false );
			}

			return ( true );
		}

		public bool ReadManifestOptions( bool bCreatingManifest )
		{
			string FinalManifestsName = Path.Combine( InstallFolder, "UnSetup.Manifests.xml" );
			
			if( !bInstalledBuild )
			{
				// Recopy the EULAs as they may not exist
				GetEULAs();

				// If we're not an installed build, write out a new manifest descriptor with expanded macros
				string ManifestsName = Path.Combine( InstallFolder, "UnSetup.Manifests." + InstallerRoot + ".xml" );
				FileInfo ManifestsInfo = new FileInfo( ManifestsName );
				if( ManifestsInfo.Exists )
				{
					Manifest = ReadXml<ManifestOptions>( ManifestsName );
					FixupPlatformAndGameNameWildcards();
					WriteXml<ManifestOptions>( FinalManifestsName, Manifest );
				}
			}

			// Now we're either either an installed build, or we just created the manifests file; either way, we should be good
			FileInfo FinalManifestsInfo = new FileInfo( FinalManifestsName );
			if( FinalManifestsInfo.Exists )
			{
				Manifest = ReadXml<ManifestOptions>( FinalManifestsName );
				return true;
			}

			Console.WriteLine( " ... no UnSetup.Manifests.xml found" );
			return false;
		}

		/// <summary>
		/// Helper function which takes all the entries we obtained from our Manifest xml and fixes up and references to the wildcards for platform and game name.
		/// </summary>
		public void FixupPlatformAndGameNameWildcards()
		{
			Manifest.AppToLaunch = Manifest.AppToLaunch.Replace( "%GAMENAME%", GameName );
			Manifest.AppToLaunch = Manifest.AppToLaunch.Replace( "%PLATFORM%", PlatformName );

			Manifest.RootName = Manifest.RootName.Replace( "%GAMENAME%", GameName );
			Manifest.RootName = Manifest.RootName.Replace( "%PLATFORM%", PlatformName );

			Manifest.FullName = Manifest.FullName.Replace( "%GAMENAME%", GameName );
			Manifest.FullName = Manifest.FullName.Replace( "%PLATFORM%", PlatformName );

			foreach( GameManifestOptions GMO in Manifest.GameInfo )
			{
				GMO.Name = GMO.Name.Replace( "%GAMENAME%", GameName );
				GMO.Name = GMO.Name.Replace( "%PLATFORM%", PlatformName );

				GMO.AppCommandLine = GMO.AppCommandLine.Replace( "%GAMENAME%", GameName );
				GMO.AppCommandLine = GMO.AppCommandLine.Replace( "%PLATFORM%", PlatformName );

				GMO.AppToCreateInis = GMO.AppToCreateInis.Replace( "%GAMENAME%", GameName );
				GMO.AppToCreateInis = GMO.AppToCreateInis.Replace( "%PLATFORM%", PlatformName );

				GMO.AppToElevate = GMO.AppToElevate.Replace( "%GAMENAME%", GameName );
				GMO.AppToElevate = GMO.AppToElevate.Replace( "%PLATFORM%", PlatformName );
			}

			foreach( LinkShortcutOptions LSO in Manifest.LinkShortcuts )
			{
				LSO.Name = LSO.Name.Replace( "%GAMENAME%", GameName );
				LSO.Name = LSO.Name.Replace( "%PLATFORM%", PlatformName );

				LSO.UrlFilePath = LSO.UrlFilePath.Replace( "%GAMENAME%", GameName );
				LSO.UrlFilePath = LSO.UrlFilePath.Replace( "%PLATFORM%", PlatformName );

				LSO.DisplayPath = LSO.DisplayPath.Replace( "%GAMENAME%", GameName );
				LSO.DisplayPath = LSO.DisplayPath.Replace( "%PLATFORM%", PlatformName );
			}

			for( int Idx = 0; Idx < Manifest.RequiredFolders.Length; Idx++ )// String FolderName in Manifest.RequiredFolders )
			{
				Manifest.RequiredFolders[Idx] = Manifest.RequiredFolders[Idx].Replace( "%GAMENAME%", GameName );
				Manifest.RequiredFolders[Idx] = Manifest.RequiredFolders[Idx].Replace( "%PLATFORM%", PlatformName );
			}

			for( int Idx = 0; Idx < Manifest.GameFilesToInclude.Length; Idx++ )// String FolderName in Manifest.RequiredFolders )
			{
				Manifest.GameFilesToInclude[Idx] = Manifest.GameFilesToInclude[Idx].Replace( "%GAMENAME%", GameName );
				Manifest.GameFilesToInclude[Idx] = Manifest.GameFilesToInclude[Idx].Replace( "%PLATFORM%", PlatformName );
			}

			for( int Idx = 0; Idx < Manifest.GameFilesToExclude.Length; Idx++ )// String FolderName in Manifest.RequiredFolders )
			{
				Manifest.GameFilesToExclude[Idx] = Manifest.GameFilesToExclude[Idx].Replace( "%GAMENAME%", GameName );
				Manifest.GameFilesToExclude[Idx] = Manifest.GameFilesToExclude[Idx].Replace( "%PLATFORM%", PlatformName );
			}

			for( int Idx = 0; Idx < Manifest.MainFilesToExclude.Length; Idx++ )// String FolderName in Manifest.RequiredFolders )
			{
				Manifest.MainFilesToExclude[Idx] = Manifest.MainFilesToExclude[Idx].Replace( "%GAMENAME%", GameName );
				Manifest.MainFilesToExclude[Idx] = Manifest.MainFilesToExclude[Idx].Replace( "%PLATFORM%", PlatformName );
			}
		}

		public void SaveManifestOptions()
		{
			string ManifestsName = "UnSetup.Manifests." + InstallerRoot + ".xml";
			WriteXml<ManifestOptions>( Path.Combine( InstallFolder, ManifestsName ), Manifest );
		}

		public void ReadGame( string InstallFolder )
		{
			string GameOptionsName = Path.Combine( InstallFolder, "UnSetup.Game.xml" );

			FileInfo GameOptionsInfo = new FileInfo( GameOptionsName );
			if( GameOptionsInfo.Exists )
			{
				Game = ReadXml<GameOptions>( GameOptionsName );

				Console.WriteLine( "Loaded: " + GameOptionsName + " (" + Game.GameLongName + ")" );
			}
			else
			{
				Console.WriteLine( " ... no UnSetup.Game.xml found; creating default." );
				Game = new GameOptions();
			}
		}

		public void SaveGame()
		{
			WriteXml<GameOptions>( Path.Combine( Application.StartupPath, "UnSetup.Game.xml" ), Game );
		}

		public void ReadInstallInfo( string InstallFolder )
		{
			string InstallInfoPath = Path.Combine( InstallFolder, "InstallInfo.xml" );
			FileInfo InstallInfoInfo = new FileInfo( InstallInfoPath );
			if( InstallInfoInfo.Exists )
			{
				InstallInfoData = ReadXml<InstallInfo>( InstallInfoPath );
				bInstalledBuild = true;
				Console.WriteLine( " ... loaded official UDK install Guid" );
			}
			else
			{
				InstallInfoData = new InstallInfo();
				bInstalledBuild = false;
				Console.WriteLine( " ... no InstallInfo.xml found; not an installed build" );
			}
		}

		public void SaveInstallInfo( string InstallFolder )
		{
			WriteXml<InstallInfo>( Path.Combine( InstallFolder, "InstallInfo.xml" ), InstallInfoData );
		}

		public string GetGameLongName()
		{
			string LongName = Game.GameLongName;
			if( LongName.Length == 0 )
			{
				LongName = Game.GameName;
			}

			return ( LongName );
		}

		public string GetPhrase( string Phrase )
		{
			return ( Phrases.GetPhrase( Phrase ) );
		}

		public string GetLocFileName( string Root, string Extension )
		{
			string Path = Root + "." + Phrases.GetUE3Language() + "." + Extension;
			return ( Path );
		}

		public string GetSafeLocFileName( string Root, string Extension )
		{
			string Path = GetLocFileName( Root, Extension );
			FileInfo Info = new FileInfo( Path );
			if( !Info.Exists )
			{
				Path = Root + ".INT." + Extension;
				Info = new FileInfo( Path );
				if( !Info.Exists )
				{
					return ( Root + "." + Extension );
				}
			}

			return ( Path );
		}

		private void GetCertificate()
		{
			try
			{
				EpicCertificate = X509Certificate.CreateFromSignedFile( Application.ExecutablePath );
			}
			catch( Exception )
			{
				// Any exception means that either the file isn't signed or has an invalid certificate
			}

			if( EpicCertificate != null )
			{
				byte[] EpicCertificateSerialNumber = { 0x4f, 0xc2, 0xc5, 0x64, 0xae, 0x71, 0xdf, 0xe2, 0x34, 0x3d, 0x2c, 0x5d, 0x50, 0x38, 0x13, 0x3e };
				byte[] OtherCertificateSerialNumber = EpicCertificate.GetSerialNumber();

				bool IsEpicCertificate = true;
				if( EpicCertificateSerialNumber.Length != OtherCertificateSerialNumber.Length )
				{
					IsEpicCertificate = false;
				}

				for( Int32 Index = 0; Index < EpicCertificateSerialNumber.Length && IsEpicCertificate; Index++ )
				{
					IsEpicCertificate &= ( EpicCertificateSerialNumber[Index] == OtherCertificateSerialNumber[Index] );
				}

				if( !IsEpicCertificate )
				{
					EpicCertificate = null;
				}
			}
		}

		private bool CompareCertificateSerials( byte[] KnownCertificateSerialNumber, byte[] OtherCertificateSerialNumber )
		{
			bool IsKnownCertificate = false;

			if( KnownCertificateSerialNumber.Length == OtherCertificateSerialNumber.Length )
			{
				IsKnownCertificate = true;
				for( Int32 Index = 0; Index < KnownCertificateSerialNumber.Length; Index++ )
				{
					IsKnownCertificate &= ( KnownCertificateSerialNumber[Index] == OtherCertificateSerialNumber[Index] );
				}
			}

			return ( IsKnownCertificate );
		}

		public bool ValidateCertificate( string FileName )
		{
			bool CertificateValid = false;
#if !DEBUG
			if( EpicCertificate != null )
#endif
			{
				X509Certificate Certificate = null;
				try
				{
					Certificate = X509Certificate.CreateFromSignedFile( FileName );
				}
				catch( Exception )
				{
					// Any exception means that either the file isn't signed or has an invalid certificate
				}

				if( Certificate != null )
				{
#if !DEBUG
					// Is it an Epic certificate?
					try
					{
						CertificateValid = Certificate.Equals( EpicCertificate );
					}
					catch( Exception )
					{
					}
#endif
					if( !CertificateValid )
					{
						CertificateValid = FileName.ToLower().Contains( "binaries\\win32\\usercode" );
					}

					if( !CertificateValid )
					{
						// .. if it's not an Epic certificate, also allow Microsoft, AMD, Valve and Nvidia ones through
						byte[] OtherCertificateSerialNumber = Certificate.GetSerialNumber();

						foreach( byte[] SerialNumber in ValidSerialNumbers )
						{
							CertificateValid = CompareCertificateSerials( SerialNumber, OtherCertificateSerialNumber );
							if( CertificateValid )
							{
								break;
							}
						}
					}
				}
				else
				{
					CertificateValid = FileName.ToLower().Contains( "binaries\\win32\\usercode\\unsetupnative" );
				}
			}
#if DEBUG
			return( true );
#else
			return ( CertificateValid );
#endif
		}

		private long GetFileSize( FolderProperties FolderProperty, string FileName )
		{
			if( FolderProperty != null )
			{
				FileName = FileName.Replace( '\\', '/' );
				string[] Folders = FileName.Split( '/' );
				if( Folders.Length > 1 )
				{
					FolderProperties Folder = FolderProperty.FindFolder( Folders[0] );
					return ( GetFileSize( Folder, FileName.Substring( Folders[0].Length + 1 ) ) );
				}

				FileProperties File = FolderProperty.FindFile( Folders[0] );
				if( File != null )
				{
					return ( File.Size );
				}
			}

			return ( 0 );
		}

		private FolderProperties FindParentFolder( string FolderName, FolderProperties ParentFolder )
		{
			FolderName = FolderName.Replace( '\\', '/' );
			string[] Folders = FolderName.Split( '/' );
			FolderProperties Folder = ParentFolder.FindFolder( Folders[0] );

			if( Folder == null )
			{
				Folder = new FolderProperties( Folders[0] );
				ParentFolder.AddFolder( Folder );
			}

			if( Folders.Length > 1 )
			{
				return ( FindParentFolder( FolderName.Substring( Folders[0].Length + 1 ), Folder ) );
			}

			return ( Folder );
		}

		private int FilterOutFileSpec( FolderProperties FolderProperty, string FileName )
		{
			int FilterCount = 0;
			FileName = FileName.Replace( '\\', '/' );

			string[] Folders = FileName.Split( '/' );
			if( Folders.Length > 1 )
			{
				foreach( FolderProperties FP in FolderProperty.Folders )
				{
					if( string.Compare( FP.FolderName, Folders[0], true ) == 0 )
					{
						FilterCount = FilterOutFileSpec( FP, FileName.Substring( Folders[0].Length + 1 ) );
						break;
					}
				}
			}
			else
			{
				Regex RX = new Regex( Folders[0], RegexOptions.IgnoreCase | RegexOptions.Compiled );

				foreach( FolderProperties FP in FolderProperty.Folders )
				{
					Match RegExpMatch = RX.Match( FP.FolderName );
					if( RegExpMatch.Success )
					{
						FP.Size = -1;
						FilterCount++;
						// Console.WriteLine( " ****** Filtering folder: " + FP.FolderName );
					}
				}

				foreach( FileProperties FP in FolderProperty.Files )
				{
					Match RegExpMatch = RX.Match( FP.FileName );
					if( RegExpMatch.Success )
					{
						FP.Size = -1;
						FilterCount++;
						// Console.WriteLine( " ****** Filtering: " + FP.FileName );
					}
				}
			}

			return ( FilterCount );
		}

		private void AddFileSpec( FolderProperties RootFolderProperty, string Spec )
		{
			// Ensure everything is a forward slash
			Spec = Spec.Replace( '\\', '/' );

			// Split up into folder and file spec
			int LastSlash = Spec.LastIndexOf( '/' );
			string FolderSpec = Spec.Substring( 0, LastSlash );
			string FileSpec = Spec.Substring( LastSlash + 1, Spec.Length - LastSlash - 1 );
			Regex RX = new Regex( FileSpec, RegexOptions.IgnoreCase | RegexOptions.Compiled );

			FolderProperties Folder = FindParentFolder( FolderSpec, RootFolderProperty );
			if( Folder != null )
			{
				Folder.FindAllFiles( FolderSpec, RX, null );
			}
		}

		// Extract the EULAs from the loc folder and place them where the installer can find them
		private void GetEULAs()
		{
			DirectoryInfo DirInfo = new DirectoryInfo( "Engine\\Localization" );
			if( DirInfo.Exists )
			{
				foreach( FileInfo EULAFile in DirInfo.GetFiles( "EULA." + InstallerRoot + ".???.rtf" ) )
				{
					string DestName = Path.Combine( "Binaries\\InstallData\\", EULAFile.Name.Replace( "." + InstallerRoot, "" ) );
					FileInfo DestInfo = new FileInfo( DestName );

					if( DestInfo.Exists )
					{
						DestInfo.IsReadOnly = false;
						DestInfo.Delete();
					}

					EULAFile.CopyTo( DestName, true );
				}
			}
		}

		// Create an xml file of the files to add to the installer
		public void CreateManifest()
		{
			Console.WriteLine( "Creating manifest ..." );

			// Read in the manifest options for this type of installer (e.g. UDK)
			if( !ReadManifestOptions( false ) )
			{
				return;
			}

			// Delete any old manifest files
			FileInfo ManifestFile = new FileInfo( ManifestFileName );
			if( ManifestFile.Exists )
			{
				ManifestFile.IsReadOnly = false;
				ManifestFile.Delete();
			}

			// Find and extract the relevant EULA's to Binaries\InstallData
			GetEULAs();

			// Find all files in the folder structure
			Console.WriteLine( " ... current directory: " + Environment.CurrentDirectory );
			FolderProperties RootFolderProperty = new FolderProperties( "." );
			RootFolderProperty.FindAllFiles( ".", null, null );

			// Filter out special files
			int FilterCount = 0;
			foreach( string FileSpec in Manifest.MainFilesToExclude )
			{
				FilterCount += FilterOutFileSpec( RootFolderProperty, FileSpec );
			}

			// Filter out the binaries not signed by Epic
			RootFolderProperty.FilterUnsignedBinaries( this, "." );

			// Always add the manifest options file, this has details about how to run the installer
			FileInfo Info = new FileInfo( "Binaries\\UnSetup.Manifests.xml" );
			RootFolderProperty.AddFile( new FileProperties( "Binaries\\UnSetup.Manifests.xml", Info.Length ) );

			// Remove any files marked for exclusion
			RootFolderProperty.Clean();

			int NumFiles = RootFolderProperty.FileCount();
			int NumFolders = RootFolderProperty.FolderCount();
			Console.WriteLine( " ... manifest created with " + NumFolders.ToString() + " folders and " + NumFiles.ToString() + " files (" + FilterCount.ToString() + " were filtered out)." );

			// Output an XML file with the potential files
			WriteXml<FolderProperties>( ManifestFileName, RootFolderProperty );

			Console.WriteLine( " ... manifest saved." );
		}

		public int GameCreateManifest()
		{
			Console.WriteLine( "Creating game manifest ..." );

			FileInfo Info = new FileInfo( "Binaries/UnSetup.Game.xml" );
			if( !Info.Exists )
			{
				Console.WriteLine( " ... failed. Game needs the configuration file UnSetup.Game.xml." );
				return ( 1 );
			}

			if( Game.GameUniqueID == Guid.Empty )
			{
				Console.WriteLine( " ... failed. Game needs a name and unique identifier." );
				return ( 2 );
			}

			// Read in the manifest options for this type of installer (e.g. UDK)
			if( !ReadManifestOptions( true ) )
			{
				return ( 3 );
			}

			foreach( string RequiredFolder in Manifest.RequiredFolders )
			{
				DirectoryInfo DirInfo = new DirectoryInfo( RequiredFolder );
				if( !DirInfo.Exists )
				{
					Console.WriteLine( " ... failed. Missing required folder '" + RequiredFolder + "'. Has data been cooked?" );
					return ( 4 );
				}
			}

			FileInfo ManifestFile = new FileInfo( GameManifestFileName );
			if( ManifestFile.Exists )
			{
				ManifestFile.IsReadOnly = false;
				ManifestFile.Delete();
			}

			FolderProperties RootFolderProperty = ReadXml<FolderProperties>( ManifestFileName );

			// Only check the EULAs in an installed build as there is nothing to check against otherwise
			if( bInstalledBuild )
			{
				List<string> EULAFiles = new List<string>()
				{
					"Binaries/InstallData/EULA.INT.rtf",
					"Binaries/InstallData/EULA.FRA.rtf",
					"Binaries/InstallData/EULA.ITA.rtf",
					"Binaries/InstallData/EULA.DEU.rtf",
					"Binaries/InstallData/EULA.ESN.rtf",
					"Binaries/InstallData/EULA.RUS.rtf",
					"Binaries/InstallData/EULA.POL.rtf",
					"Binaries/InstallData/EULA.HUN.rtf",
					"Binaries/InstallData/EULA.JPN.rtf",
					"Binaries/InstallData/EULA.KOR.rtf",
					"Binaries/InstallData/EULA.CHN.rtf"
				};

				foreach( string EULAFile in EULAFiles )
				{
					long FileSize = GetFileSize( RootFolderProperty, EULAFile );
					FileInfo EULAInfo = new FileInfo( EULAFile );
					if( !EULAInfo.Exists || FileSize != EULAInfo.Length )
					{
						Console.WriteLine( " ... failed. EULA file \"{0}\" is corrupt or missing.", EULAFile );
						return ( 3 );
					}
				}
			}

			Console.WriteLine( " ... current directory: " + Environment.CurrentDirectory );

			int FilterCount = 0;
			foreach( string FolderSpec in Manifest.GameFilesToExclude )
			{
				FilterCount += FilterOutFileSpec( RootFolderProperty, FolderSpec );
			}

			// Remove any files or folders marked for exclusion
			RootFolderProperty.Clean();

			Console.WriteLine( " ... filtered out " + FilterCount.ToString() + " filespecs." );

			// Parse the spec into a folder spec and file spec
			foreach( string Spec in Manifest.GameFilesToInclude )
			{
				AddFileSpec( RootFolderProperty, Spec );
			}

			// Filter out the binaries not signed by Epic
			RootFolderProperty.FilterUnsignedBinaries( this, "." );

			// Always add the game properties file, this is what states the install package is a game
			RootFolderProperty.AddFile( new FileProperties( "Binaries/UnSetup.Game.xml", Info.Length ) );

			// Remove any files or folders marked for exclusion
			RootFolderProperty.Clean();

			int NumFiles = RootFolderProperty.FileCount();
			int NumFolders = RootFolderProperty.FolderCount();
			Console.WriteLine( " ... game manifest created with " + NumFolders.ToString() + " folders and " + NumFiles.ToString() + " files." );

			WriteXml<FolderProperties>( GameManifestFileName, RootFolderProperty );

			Console.WriteLine( " ... game manifest saved." );
			return ( 0 );
		}

		delegate void DelegateUpdateSaveProgress( object Sender, SaveProgressEventArgs Event );

		public void UpdateSaveProgress( object Sender, SaveProgressEventArgs Event )
		{
			const int Divisor = 64;

			if( Progress.GenericProgressBar.InvokeRequired )
			{
				Progress.GenericProgressBar.Invoke( new DelegateUpdateSaveProgress( UpdateSaveProgress ), new object[] { Sender, Event } );
				return;
			}

			bool DoEvents = false;

			if( Event.EventType == ZipProgressEventType.Saving_BeforeWriteEntry )
			{
				Progress.GenericProgressBar.Minimum = 0;
				Progress.GenericProgressBar.Maximum = Math.Min( ( int )( TotalUncompressedSize / Divisor ), Int32.MaxValue );
				Progress.GenericProgressBar.Value = Math.Min( ( int )( CurrentDecompressedSize / Divisor ), Int32.MaxValue );

				Progress.ProgressLabel.Text = Event.CurrentEntry.FileName;

				DoEvents = true;
			}
			else if( Event.EventType == ZipProgressEventType.Saving_EntryBytesRead )
			{
				if( Event.TotalBytesToTransfer < 0x40000 )
				{
					Progress.SubProgressBar.Minimum = 0;
					Progress.SubProgressBar.Maximum = 1;
					Progress.SubProgressBar.Value = 1;
				}
				else
				{
					Progress.GenericProgressBar.Minimum = 0;
					Progress.GenericProgressBar.Maximum = Math.Min( ( int )( TotalUncompressedSize / Divisor ), Int32.MaxValue );
					if( CurrentDecompressedSize + Event.BytesTransferred <= TotalUncompressedSize )
					{
						Progress.GenericProgressBar.Value = Math.Min( ( int )( ( CurrentDecompressedSize + Event.BytesTransferred ) / Divisor ), Int32.MaxValue );
					}

					Progress.SubProgressBar.Minimum = 0;
					Progress.SubProgressBar.Maximum = Math.Min( ( int )( Event.TotalBytesToTransfer / Divisor ), Int32.MaxValue );
					Progress.SubProgressBar.Value = ( int )( Event.BytesTransferred / Divisor );
					DoEvents = true;
				}
			}
			else if( Event.EventType == ZipProgressEventType.Saving_AfterWriteEntry )
			{
				CurrentDecompressedSize += Event.CurrentEntry.UncompressedSize;
				if( CurrentDecompressedSize > TotalUncompressedSize )
				{
					CurrentDecompressedSize = TotalUncompressedSize;
				}
			}

			if( DoEvents )
			{
				Application.DoEvents();
			}
		}

		delegate void DelegateUpdateExtractProgress( object Sender, ExtractProgressEventArgs Event );

		public void UpdateExtractProgress( object Sender, ExtractProgressEventArgs Event )
		{
			if( Progress == null )
			{
				return;
			}

			if( Progress.GenericProgressBar.InvokeRequired )
			{
				Progress.GenericProgressBar.Invoke( new DelegateUpdateExtractProgress( UpdateExtractProgress ), new object[] { Sender, Event } );
				return;
			}

			bool DoEvents = false;

			if( Event.EventType == ZipProgressEventType.Extracting_BeforeExtractEntry )
			{
				Progress.GenericProgressBar.Minimum = 0;
				Progress.GenericProgressBar.Maximum = ( int )( TotalUncompressedSize / 16 );
				Progress.GenericProgressBar.Value = ( int )( CurrentDecompressedSize / 16 );

				Progress.ProgressLabel.Text = Event.CurrentEntry.FileName;

				DoEvents = true;
			}
			else if( Event.EventType == ZipProgressEventType.Extracting_EntryBytesWritten )
			{
				if( Event.TotalBytesToTransfer < 0x40000 )
				{
					Progress.SubProgressBar.Minimum = 0;
					Progress.SubProgressBar.Maximum = 1;
					Progress.SubProgressBar.Value = 1;
				}
				else
				{
					Progress.GenericProgressBar.Minimum = 0;
					Progress.GenericProgressBar.Maximum = ( int )( TotalUncompressedSize / 16 );
					if( CurrentDecompressedSize + Event.BytesTransferred <= TotalUncompressedSize )
					{
						Progress.GenericProgressBar.Value = ( int )( ( CurrentDecompressedSize + Event.BytesTransferred ) / 16 );
					}

					Progress.SubProgressBar.Minimum = 0;
					Progress.SubProgressBar.Maximum = ( int )( Event.TotalBytesToTransfer / 16 );
					Progress.SubProgressBar.Value = ( int )( Event.BytesTransferred / 16 );

					DoEvents = true;
				}
			}
			else if( Event.EventType == ZipProgressEventType.Extracting_AfterExtractEntry )
			{
				FileInfo Info = new FileInfo( Path.Combine( Event.ExtractLocation, Event.CurrentEntry.FileName ) );
				if( Info.Exists && ( Info.Extension.ToLower() == ".exe" || Info.Extension.ToLower() == ".dll" ) )
				{
					if( !ValidateCertificate( Info.FullName ) )
					{
						Info.IsReadOnly = false;
#if DEBUG
						Console.WriteLine( "WARNING: Would delete unsigned: " + Info.FullName );
#else
						Info.Delete();
#endif
					}
				}

				// Update the progress bar
				CurrentDecompressedSize += Event.CurrentEntry.UncompressedSize;
				if( CurrentDecompressedSize > TotalUncompressedSize )
				{
					CurrentDecompressedSize = TotalUncompressedSize;
				}
			}

			if( DoEvents )
			{
				Application.DoEvents();
			}
		}

		public void UpdateProgressBar( string Label, int Current, int Max )
		{
			if( Progress != null )
			{
				Progress.ProgressLabel.Text = Label;
				Progress.GenericProgressBar.Minimum = 0;
				Progress.GenericProgressBar.Maximum = Max;
				Progress.GenericProgressBar.Value = Current;

				Application.DoEvents();
			}
		}

		public void UpdateSubProgressBar( int Current, int Max )
		{
			if( Progress == null )
			{
				return;
			}

			Progress.SubProgressBar.Minimum = 0;
			Progress.SubProgressBar.Maximum = Max;
			Progress.SubProgressBar.Value = Current;

			Application.DoEvents();
		}

		public void CreateProgressBar( string Title )
		{
			Progress = new ProgressBar();
			Progress.Text = Title;
			Progress.ProgressTitleLabel.Text = Title;

			if( bStandAloneRedist )
			{
				Progress.Icon = global::UnSetup.Properties.Resources.UE3Redist;
				Progress.ProgressTitleLabel.Image = UnSetup.Properties.Resources.UE3RedistBannerImage;
			}
			else
			{
				Progress.Icon = global::UnSetup.Properties.Resources.UDKIcon;
				Progress.ProgressTitleLabel.Image = UnSetup.Properties.Resources.BannerImage;
			}

			Progress.Show();
			Application.DoEvents();
		}

		public void UpdateProgressBar( string Title )
		{
			Progress.Text = Title;
			Progress.ProgressTitleLabel.Text = Title;
			Application.DoEvents();
		}

		public void DestroyProgressBar()
		{
			Progress.Close();
		}

		private string GetPackageName()
		{
			string ZipFileName = "";
			if( Game.GameUniqueID == Guid.Empty )
			{
				ZipFileName = Manifest.RootName + "Install-" + UnSetupTimeStamp;
			}
			else
			{
				ZipFileName = Manifest.RootName + "Install-" + Game.GameName;
			}

			return ( ZipFileName );
		}

		public int BuildInstaller( string ManifestFile )
		{
			Console.WriteLine( "Building zip ..." );

			if( !ReadManifestOptions( false ) )
			{
				Console.WriteLine( " ... failed. No default manifests file! Please create a manifest first." );
				return ( 1 );
			}

			CreateProgressBar( GetPhrase( "PBCompressing" ) );

			// Work out the installer name
			Console.WriteLine( " ... getting package name" );
			string ZipFileName = GetPackageName() + ".zip";

			// Delete old version
			FileInfo ZipInfo = new FileInfo( ZipFileName );
			if( ZipInfo.Exists )
			{
				ZipInfo.IsReadOnly = false;
				ZipInfo.Delete();
			}

			// Load in file manifest
			FolderProperties RootFolderProperty = ReadXml<FolderProperties>( ManifestFile );

			// Create new zip
			Console.WriteLine( " ... creating zip" );

			ZipFile Zip = new ZipFile();
			Zip.CompressionLevel = Ionic.Zlib.CompressionLevel.Level9;
			Zip.UseZip64WhenSaving = Zip64Option.Always;
			Zip.BufferSize = 0x10000;
			Zip.SaveProgress += UpdateSaveProgress;

			Console.WriteLine( " ... adding files to zip" );

			// Add the files referenced in the manifest to the zip file
			TotalUncompressedSize = 0;
			CurrentDecompressedSize = 0;
			RootFolderProperty.ZipFiles( ".", Zip );

			// Add the file manifest
			Zip.AddFile( ManifestFile );

			Console.WriteLine( " ... saving zip: " + ZipFileName );

			Zip.Save( ZipFileName );

			DestroyProgressBar();

			Console.WriteLine( " ... building zip successful!" );
			return ( 0 );
		}

		public void DeleteFiles( bool AllFiles, bool bIsGame )
		{
			Environment.CurrentDirectory = PackageInstallLocation;

			FolderProperties RootFolderProperty = null;
			if( !AllFiles )
			{
				// Get files that were installed
				if( !bIsGame )
				{
					RootFolderProperty = ReadXml<FolderProperties>( ManifestFileName );
				}
				else
				{
					RootFolderProperty = ReadXml<FolderProperties>( GameManifestFileName );
				}

				// Add in all files from the following folders
				List<string> AdditionalFolders = new List<string>() 
				{ 
					"binaries/installdata/(.*)", 
					"binaries/logs/(.*)", 
					"engine/shaders/workingdirectory/(.*)",
					"mobilegame/autosaves/(.*)",
					"mobilegame/config/(.*)",
					"mobilegame/content/^localshadercache(.*).upk",
					"mobilegame/content/^globalshadercache(.*).upk",
					"mobilegame/cookediphone/(.*)",
					"mobilegame/cookedpc/(.*)",
					"mobilegame/logs/(.*)",
					"udkgame/autosaves/(.*)",
					"udkgame/config/(.*)",
					"udkgame/content/^localshadercache(.*).upk",
					"udkgame/content/^globalshadercache(.*).upk",
					"udkgame/cookediphone/(.*)",
					"udkgame/cookedpc/(.*)",
					"udkgame/logs/(.*)",
				};

				foreach( string Spec in AdditionalFolders )
				{
					AddFileSpec( RootFolderProperty, Spec );
				}
			}
			else
			{
				RootFolderProperty = new FolderProperties( "." );

				// Only delete the root folders we install
				List<string> AllowedFolders = new List<string>() 
				{ 
					"binaries", 
					"mobilegame", 
					"udkgame", 
					"development", 
					"engine" 
				};

				RootFolderProperty.FindAllFiles( ".", null, AllowedFolders );
			}

			// Make sure the root folder is not in use
			Environment.CurrentDirectory = Application.StartupPath;

			// Delete all files
			RootFolderProperty.DeleteFiles( PackageInstallLocation );
		}

		public void DeleteTempFiles( string WorkFolder )
		{
			Environment.CurrentDirectory = WorkFolder;

			FolderProperties RootFolderProperty = new FolderProperties( "." );
			RootFolderProperty.FindAllFiles( ".", null, null );

			// Filter out the files that will be in use
			FilterOutFileSpec( RootFolderProperty, "Binaries\\UnSetup.exe" );
			FilterOutFileSpec( RootFolderProperty, "Binaries\\UnSetup.exe.config" );
			FilterOutFileSpec( RootFolderProperty, "Binaries\\InstallData\\Interop.IWshRuntimeLibrary.dll" );

			// Delete all files
			RootFolderProperty.DeleteFiles( WorkFolder );
		}

		/** Iterate over the user's start menu and delete any shortcuts pointing to the folder we just uninstalled */
		public void DeleteShortcuts()
		{
			string StartMenu = GetAllUsersStartMenu();
			FolderProperties RootFolderProperty = new FolderProperties( "." );
			RootFolderProperty.FindAllFiles( StartMenu, null, null );

			RootFolderProperty.FilterUDKShortcuts( this, StartMenu );
		}

		private void WriteData( FileStream Source, FileStream Dest, long Length )
		{
			byte[] Buffer = new byte[1024 * 1024];

			UpdateSubProgressBar( 0, ( int )( Length / 16 ) );

			long BytesWritten = 0;
			while( BytesWritten < Length )
			{
				int Count = Source.Read( Buffer, 0, 1024 * 1024 );

				if( BytesWritten + Count > Length )
				{
					Count = ( int )( Length - BytesWritten );
				}

				BytesWritten += Count;
				Dest.Write( Buffer, 0, Count );

				UpdateSubProgressBar( ( int )( BytesWritten / 16 ), ( int )( Length / 16 ) );
			}
		}

		private bool Concatenate( FileStream Dest, string SourceName )
		{
			FileInfo Info = new FileInfo( SourceName );
			if( !Info.Exists )
			{
				return ( false );
			}
			long Length = Info.Length;
			Console.WriteLine( " ...... writing " + Length.ToString() + " bytes" );

			FileStream Source = Info.OpenRead();
			WriteData( Source, Dest, Length );

			Console.WriteLine( " ...... position " + Dest.Position.ToString() );

			Source.Close();
			return ( true );
		}

		private bool SaveStreamToFile( FileStream Source, string DestName, long Length )
		{
			FileInfo Info = new FileInfo( DestName );
			if( Info.Exists )
			{
				Info.IsReadOnly = false;
				Info.Delete();
			}

			FileStream Dest = Info.OpenWrite();
			WriteData( Source, Dest, Length );
			Dest.Close();
			return ( true );
		}

		// UDKNativ
		private List<byte> UDKStartUnSetupSignature = new List<byte> { 0x55, 0x44, 0x4B, 0x4E, 0x61, 0x74, 0x69, 0x76 };
		// UDKNET40
		private List<byte> UDKStartDotNetSignature = new List<byte> { 0x55, 0x44, 0x4B, 0x4E, 0x45, 0x54, 0x34, 0x30 };
		// UDKRedis
		public List<byte> UDKStartRedistSignature = new List<byte> { 0x55, 0x44, 0x4B, 0x52, 0x65, 0x64, 0x69, 0x73 };
		// UDKMagic
		public List<byte> UDKStartZipSignature = new List<byte> { 0x55, 0x44, 0x4B, 0x4D, 0x61, 0x67, 0x69, 0x63 };

		private void PackageFile( FileStream Packaged, List<byte> Signature, string FileName, int ProgressCount )
		{
			UpdateProgressBar( FileName, ProgressCount, 5 );

			if( Signature != null )
			{
				Console.WriteLine( " ... writing signature" );
				Packaged.Write( Signature.ToArray(), 0, Signature.Count );
			}

			if( FileName.Length > 0 )
			{
				Console.WriteLine( " ... adding " + FileName );
				if( !Concatenate( Packaged, FileName ) )
				{
					Console.WriteLine( " ... failed To add: " + FileName );
				}
			}
		}

		private FileStream OpenNewPackage( string PackageName )
		{
			Console.WriteLine( " ... creating package: " + PackageName );
			FileInfo PackageFile = new FileInfo( PackageName );
			if( PackageFile.Exists )
			{
				if( PackageFile.IsReadOnly )
				{
					Console.WriteLine( " ... failed to create package" );
					return ( null );
				}

				PackageFile.Delete();
			}

			// FIXME: Check for exceptions
			FileStream Package = PackageFile.OpenWrite();
			return ( Package );
		}

		public int PackageFiles()
		{
			Console.WriteLine( "Packaging files ..." );

			if( !ReadManifestOptions( false ) )
			{
				Console.WriteLine( " ... no default manifests file! Please create a manifest first." );
				return ( 1 );
			}

			CreateProgressBar( GetPhrase( "PBPackaging" ) );

			Console.WriteLine( " ... getting package name" );

			string ZipFileName = GetPackageName();

			FileStream Packaged = OpenNewPackage( ZipFileName + ".exe" );
			if( Packaged != null )
			{
				// Construct a package from all the required elements
				PackageFile( Packaged, null, "Binaries\\Win32\\UserCode\\UnSetupNativeWrapper.exe", 0 );
				PackageFile( Packaged, UDKStartUnSetupSignature, "Binaries\\UnSetup.exe", 1 );
				PackageFile( Packaged, UDKStartDotNetSignature, "Binaries\\InstallData\\dotNetFx40_Full_setup.exe", 2 );
				PackageFile( Packaged, UDKStartZipSignature, ZipFileName + ".zip", 4 );

				UpdateProgressBar( "", 5, 5 );

				Packaged.Close();

				// Delete the source zip file
				FileInfo ZipFile = new FileInfo( ZipFileName + ".zip" );
				if( ZipFile.Exists )
				{
					ZipFile.IsReadOnly = false;
					ZipFile.Delete();
				}

				FileInfo Installer = new FileInfo( ZipFileName + ".exe" );
				if( Installer.Exists )
				{
					Console.WriteLine( " ... packaging files successful!" );
					Console.WriteLine( "" );
					Console.WriteLine( "Finished creating installer at: file://" + Installer.FullName );
				}
				else
				{
					Console.WriteLine( "ERROR: Failed to find newly created installer." );
				}
			}

			DestroyProgressBar();
			return ( 0 );
		}

		public void PackageRedist( bool IsRedistGamePackage )
		{
			bStandAloneRedist = true;

			Console.WriteLine( "Packaging redist files ..." );
			string ZipFileName = "UE3Redist.zip";
			string RootFolder = Environment.CurrentDirectory;

			// Get list of files to add to the zip
			string PackageName = "UE3Redist";
			string WorkingFolder = "Development\\Install\\InstallFiles";

			Environment.CurrentDirectory = Path.Combine( Environment.CurrentDirectory, WorkingFolder );
			Console.WriteLine( " ... current directory: " + Environment.CurrentDirectory );

			// Delete old version
			FileInfo ZipInfo = new FileInfo( ZipFileName );
			if( ZipInfo.Exists )
			{
				ZipInfo.IsReadOnly = false;
				ZipInfo.Delete();
			}

			FolderProperties RootFolderProperty = new FolderProperties( "." );
			RootFolderProperty.FindAllFiles( ".", null, null );

			// Filter out the binaries not signed by Epic
			RootFolderProperty.FilterUnsignedBinaries( this, "." );

			// Remove any files marked for exclusion
			RootFolderProperty.Clean();

			int NumFiles = RootFolderProperty.FileCount();
			int NumFolders = RootFolderProperty.FolderCount();
			Console.WriteLine( " ... manifest created with " + NumFolders.ToString() + " folders and " + NumFiles.ToString() + " files." );

			// Zip up those files
			CreateProgressBar( GetPhrase( "PBCompressing" ) );

			Console.WriteLine( " ... creating zip" );

			ZipFile Zip = new ZipFile();
			Zip.CompressionLevel = Ionic.Zlib.CompressionLevel.Level9;
			Zip.BufferSize = 0x10000;
			Zip.SaveProgress += UpdateSaveProgress;

			Console.WriteLine( " ... adding files to zip" );

			// Add the files referenced in the manifest to the zip file
			RootFolderProperty.ZipFiles( ".", Zip );

			Console.WriteLine( " ... saving zip: " + ZipFileName );

			Zip.Save( ZipFileName );

			// Package the files to make a redist
			Environment.CurrentDirectory = RootFolder;

			UpdateProgressBar( GetPhrase( "PBPackagingPrereqs" ) );
			FileStream Packaged = OpenNewPackage( "Binaries\\Redist\\" + PackageName + ".exe" );
			if( Packaged != null )
			{
				// Construct a package from all the required elements
				PackageFile( Packaged, null, "Binaries\\Win32\\UserCode\\UnSetupNativeRedistWrapper.exe", 0 );
				PackageFile( Packaged, UDKStartUnSetupSignature, "Binaries\\UnSetup.exe", 1 );
				PackageFile( Packaged, UDKStartDotNetSignature, "Binaries\\InstallData\\dotNetFx40_Full_setup.exe", 2 );
				PackageFile( Packaged, UDKStartRedistSignature, WorkingFolder + "\\UE3Redist.zip", 3 );

				UpdateProgressBar( "", 5, 5 );

				Packaged.Close();

				Console.WriteLine( " ... packaging redist successful!" );
			}

			DestroyProgressBar();
		}

#if DEBUG
		public void UnPackageFiles()
		{
			Console.WriteLine( "Unpackaging files ..." );

			// Load in the last long in the file as the size of the zip
			long ZipLength = 0;
			if( OpenZipStream( MainModuleName, UDKStartZipSignature, out ZipLength ) )
			{
				// Already seeked to beginning of zip
				Console.WriteLine( " ... saving zip of length " + ZipLength.ToString() );
				SaveStreamToFile( ZipStream, MainModuleName + ".zip", ZipLength );
				ZipStream.Close();
			}

			Console.WriteLine( " ... unpackaging files successful!" );
		}
#endif

		public int ProcessBuildCommandLine( string[] Arguments )
		{
			int ErrorCode = 0;

			// Parse the command line
			foreach( string Argument in Arguments )
			{
				if( Argument.ToLower().StartsWith( "-gamename=" ) )
				{
					GameName = Argument.Substring( "-gamename=".Length );
				}
				else if( Argument.ToLower().StartsWith( "-platform=" ) )
				{
					PlatformName = Argument.Substring( "-platform=".Length );
				}
				else if( Argument.ToLower().StartsWith( "-module=" ) )
				{
					MainModuleName = Argument.Substring( "-module=".Length );
				}
				else if( Argument.ToLower().StartsWith( "-installer=" ) )
				{
					InstallerRoot = Argument.Substring( "-installer=".Length );
				}
				else if( Argument.ToLower() == "-createmanifest" || Argument.ToLower() == "-c" )
				{
					// Creates an xml file with the files needed to make a build
					BuildCommand = BUILDCOMMANDS.CreateManifest;
				}
				else if( Argument.ToLower() == "-gamecreatemanifest" || Argument.ToLower() == "-g" )
				{
					// Creates an xml file with the files needed to make a build
					BuildCommand = BUILDCOMMANDS.GameCreateManifest;
				}
				else if( Argument.ToLower() == "-buildinstaller" || Argument.ToLower() == "-b" )
				{
					// Zips up all the files in the above xml into a zip
					BuildCommand = BUILDCOMMANDS.BuildInstaller;
				}
				else if( Argument.ToLower() == "-buildgameinstaller" || Argument.ToLower() == "-h" )
				{
					// Zips up all the files in the above xml into a zip
					BuildCommand = BUILDCOMMANDS.BuildGameInstaller;
				}
				else if( Argument.ToLower() == "-package" || Argument.ToLower() == "-p" )
				{
					// Combines several files together to make an installer
					BuildCommand = BUILDCOMMANDS.Package;
				}
				else if( Argument.ToLower() == "-redistpackage" || Argument.ToLower() == "-r" )
				{
					// Combines all the files required to make a redist installer
					BuildCommand = BUILDCOMMANDS.PackageRedist;
				}
#if DEBUG
				else if( Argument.ToLower() == "-unpackage" || Argument.ToLower() == "-u" )
				{
					// Splits off the embedded zip
					BuildCommand = BUILDCOMMANDS.UnPackage;
				}
#endif
			}

			InitApplicationInfo();
			InitVersionString();

			return ( ErrorCode );
		}

		public void DisplayHelp()
		{
			MessageBox.Show( "UnSetup - Copyright 2011 Epic Games, Inc. All Rights reserved" + Environment.NewLine +
							"" + Environment.NewLine +
							"UnSetup works in 2 modes - " + Environment.NewLine +
							"  '-' options it makes install files" + Environment.NewLine +
							"  '/' options it installs files" + Environment.NewLine +
							"" + Environment.NewLine +
							"Make Installer Options" + Environment.NewLine +
							" -createmanifest - creates a list of files to add to the installer in Binaries\\InstallData\\Manifest.xml" + Environment.NewLine +
							" -buildinstaller - adds all the files listed in the above file to a zip" + Environment.NewLine +
							" -gamecreatemanifest - creates a list of files to add to the installer in Binaries\\InstallData\\Game.xml" + Environment.NewLine +
							" -buildgameinstaller - adds all the files listed in the above file to a zip" + Environment.NewLine +
							" -package - packages all the required files into a single installer" + Environment.NewLine +
							" -redistpackage - packages all the required files to make a redist installer" + Environment.NewLine +
							" -redistpackagegame - packages all the required files to make a redist installer for a game" + Environment.NewLine +
							" -installer=<RootName> - defines the root name for install operations" + Environment.NewLine +
							" -module=<Module> - defines the zip file to use" + Environment.NewLine +
							"" + Environment.NewLine +
							"Installer Options" + Environment.NewLine +
							" with no options th install process is started (if part of a package)" + Environment.NewLine +
							" /Help - this info" + Environment.NewLine +
							" /EULA - display the EULA if a valid install is not found" + Environment.NewLine +
							" /Manifest - brings up the manifest dialog to adjust which files are included" + Environment.NewLine +
							" /GameSetup - brings up the game dialog to define the properties of a game" + Environment.NewLine +
							" /HandleInstall - handles the install process" + Environment.NewLine +
							" /Uninstall - starts the uninstall process" + Environment.NewLine +
							" /HandleUninstall - handles the uninstall process" + Environment.NewLine +
							" /Redist - starts the redistributable install" + Environment.NewLine +
							" /HandleRedist - handles the redistributable install" + Environment.NewLine +
							" /Installer=<RootName> - defines the root name for install operations" + Environment.NewLine +
							" /Module=<Module> - defines the zip file to use" + Environment.NewLine +
							" /SkipDependencies - skips the prerequisite installs" + Environment.NewLine +
							" /ProgressOnly - display progress indicators only" + Environment.NewLine +
							" /Language=<XXX> - 3 letter language code" + Environment.NewLine +
							" /MakeShortcuts - Generates start menu shortcuts and adds project to Add/Remove programs" + Environment.NewLine,
							"UnSetup Help", MessageBoxButtons.OK, MessageBoxIcon.Information );
		}


		public void ProcessInstallCommandLine( string[] Arguments )
		{
			// The default action is to install everything from this file
			Command = COMMANDS.SetupInstall;
			InitApplicationInfo();

			foreach( string Argument in Arguments )
			{
				if( Argument.ToLower().StartsWith( "/help" ) )
				{
					Command = COMMANDS.Help;
				}
				else if( Argument.ToLower().StartsWith( "/eula" ) )
				{
					Command = COMMANDS.EULA;
				}
				else if( Argument.ToLower().StartsWith( "/manifest" ) )
				{
					Command = COMMANDS.Manifest;
				}
				else if( Argument.ToLower().StartsWith( "/gamesetup" ) )
				{
					Command = COMMANDS.Game;
				}
				else if( Argument.ToLower().StartsWith( "/handleinstall" ) )
				{
					Command = COMMANDS.Install;
				}
				else if( Argument.ToLower().StartsWith( "/uninstall" ) )
				{
					Command = COMMANDS.SetupUninstall;
				}
				else if( Argument.ToLower().StartsWith( "/handleuninstall" ) )
				{
					Command = COMMANDS.Uninstall;
				}
				else if( Argument.ToLower().StartsWith( "/redist" ) )
				{
					bStandAloneRedist = true;
					Command = COMMANDS.Redist;
				}
				else if( Argument.ToLower().StartsWith( "/handleredist" ) )
				{
					bStandAloneRedist = true;
					Command = COMMANDS.HandleRedist;
				}
				else if( Argument.ToLower().StartsWith( "/makeshortcuts" ) )
				{
					Command = COMMANDS.MakeShortcuts;
				}
#if DEBUG
				else if( Argument.ToLower().StartsWith( "/subscribe" ) )
				{
					Command = COMMANDS.Subscribe;
				}
				else if( Argument.ToLower().StartsWith( "/shortcuts" ) )
				{
					Command = COMMANDS.Shortcuts;
				}
				else if( Argument.ToLower().StartsWith( "/extract" ) )
				{
					Command = COMMANDS.Extract;
				}
				else if( Argument.ToLower().StartsWith( "/checksignature" ) )
				{
					Command = COMMANDS.CheckSignature;
				}

				if( Argument.ToLower().StartsWith( "/email=" ) )
				{
					SubscribeEmail = Argument.Substring( "/email=".Length );
				}
#endif
				if( Argument.ToLower().StartsWith( "/gamename=" ) )
				{
					GameName = Argument.Substring( "/gamename=".Length );
				}
				else if( Argument.ToLower().StartsWith( "/platform=" ) )
				{
					PlatformName = Argument.Substring( "/platform=".Length );
				}
				else if( Argument.ToLower().StartsWith( "/module=" ) )
				{
					MainModuleName = Argument.Substring( "/module=".Length );
				}
				else if( Argument.ToLower().StartsWith( "/installer=" ) )
				{
					InstallerRoot = Argument.Substring( "/installer=".Length );
				}
				else if( Argument.ToLower().StartsWith( "/splashhandle=" ) )
				{
					SplashHandle = Argument.Substring( "/splashhandle=".Length );
				}
				else if( Argument.ToLower().StartsWith( "/installguid=" ) )
				{
					InstallInfoData.InstallGuidString = Argument.Substring( "/installguid=".Length );
				}
				else if( Argument.ToLower().StartsWith( "/skipdependencies" ) )
				{
					bSkipDependencies = true;
				}
				else if( Argument.ToLower().StartsWith( "/progressonly" ) )
				{
					bProgressOnly = true;
				}
				else if( Argument.ToLower().StartsWith( "/language=" ) )
				{
					string Language = Argument.Substring( "/language=".Length );
					Phrases.Init( Language, Language );
				}
			}

			InitVersionString();
		}

		private bool OpenZipStream( string ModuleName, List<byte> Signature, out long ZipLength )
		{
			ZipLength = 0;

			FileInfo Info = new FileInfo( ModuleName );
			if( Info.Exists )
			{
				ZipStream = Info.OpenRead();

				byte[] BytesToCheck = new byte[8];

				while( ZipStream.Position < Info.Length - 8 )
				{
					ZipStream.Read( BytesToCheck, 0, 8 );

					bool SignatureFound = true;
					for( int Index = 0; Index < Signature.Count; Index++ )
					{
						if( BytesToCheck[Index] != Signature[Index] )
						{
							SignatureFound = false;
							break;
						}
					}

					if( SignatureFound )
					{
						// Seek to the beginning of the zip
						ZipLength = Info.Length - ZipStream.Position;
						return ( true );
					}
				}
			}

			return ( false );
		}

		/** 
		 * Analyse the contents of the zip file and get the total uncompressed and compressed sizes
		 */
		private void AnalyseZipFile( ZipFile Zip )
		{
			TotalUncompressedSize = 0;
			CurrentDecompressedSize = 0;
			foreach( ZipEntry Entry in Zip.Entries )
			{
				TotalUncompressedSize += Entry.UncompressedSize;
			}
		}

		/* 
		 * Open the data part of a file as a zip
		 */
		public bool OpenPackagedZipFile( string ModuleName, List<byte> Signature )
		{
			try
			{
				// Try to find the signature in the file
				long ZipLength;
				if( OpenZipStream( ModuleName, Signature, out ZipLength ) )
				{
					// Open the zip file
					MainZipFile = ZipFile.Read( ZipStream );
					MainZipFile.ExtractProgress += UpdateExtractProgress;

					AnalyseZipFile( MainZipFile );

					return ( true );
				}
			}
			catch( Exception )
			{
			}

			string ErrorMessage = "GQDescCorrupt1";
			if( ZipStream != null )
			{
				ErrorMessage = "GQDescCorrupt2";
			}

			CloseSplash();
			GenericQuery Query = new GenericQuery( "GQCaptionCorrupt", ErrorMessage, false, "GQCancel", true, "GQOK" );
			Query.ShowDialog();

			return ( false );
		}

		public void ClosePackagedZipFile()
		{
			ZipStream.Close();
			MainZipFile = null;
		}

		public bool UnzipAllFiles( ZipFile Zip, string Destination )
		{
			try
			{
				Zip.ExtractAll( Destination, ExtractExistingFileAction.OverwriteSilently );
				return ( true );
			}
			catch( Exception Ex )
			{
				MessageBox.Show( GetPhrase( "ExtractFailMessage1" ) + Environment.NewLine + Environment.NewLine + Ex.ToString() + Environment.NewLine + Environment.NewLine + GetPhrase( "ExtractFailMessage2" ), GetPhrase( "ExtractFailCaption" ), MessageBoxButtons.OK, MessageBoxIcon.Error );
			}

			return ( false );
		}

		public bool IsFileInPackage( string FileName )
		{
			if( MainZipFile != null )
			{
				ZipEntry Entry = MainZipFile[FileName];
				return ( Entry != null );
			}

			return ( false );
		}

		public bool ExtractSingleFile( string FileToExtract )
		{
			try
			{
				// Extract the file from the main zip file
				if( MainZipFile != null )
				{
					// Delete the file before we extract it to be safe
					FileInfo Info = new FileInfo( FileToExtract );
					if( Info.Exists )
					{
						Info.IsReadOnly = false;
						Info.Delete();
					}

					ZipEntry Entry = MainZipFile[FileToExtract];
					if( Entry != null )
					{
						Entry.Extract( ExtractExistingFileAction.OverwriteSilently );
						return ( true );
					}
				}
			}
			catch( Exception Ex )
			{
				Debug.WriteLine( Ex.Message );
			}

			return ( false );
		}

		public int DisplayEULA()
		{
			// If we have a valid install guid, check the registry
			if( InstallInfoData.InstallGuidString.Length > 0 )
			{
				if( GetEULAAccepted() )
				{
					// EULA has already been accepted
					return ( 0 );
				}
			}

			// Set up the CWD so the EULAs can be found
			Environment.CurrentDirectory = Application.StartupPath;

			// Display the standard EULA screen
			EULA EULAScreen = new EULA();
			DialogResult EULAResult = EULAScreen.ShowDialog();
			if( EULAResult != DialogResult.OK )
			{
				// EULA rejected, do not continue
				return ( 1 );
			}

			// EULA accepted, so update registry
			InstallInfoData.InstallGuidString = Guid.NewGuid().ToString();

			string InstallFolder = Path.GetFullPath( Path.Combine( Application.StartupPath, ".." ) );
			FileInfo UDKManifest = new FileInfo( Path.Combine( InstallFolder, "Binaries\\InstallData\\Manifest.xml" ) );

			string DisplayName = "Unreal Development Kit: " + UnSetupTimeStamp;
			if( !UDKManifest.Exists )
			{
				DisplayName = GetGameLongName();
			}

			AddUDKToARP( InstallFolder, DisplayName, true );

			// Save install guid
			SaveInstallInfo( Path.Combine( InstallFolder, "Binaries" ) );

			return ( 0 );
		}

		public void CloseSplash()
		{
			if( SplashHandle.Length > 0 )
			{
				IntPtr Hwnd = ( IntPtr )Int32.Parse( SplashHandle );
				SendMessage( Hwnd, WM_CLOSE, IntPtr.Zero, IntPtr.Zero );
			}
		}

		public string GetCommandLine( string Command )
		{
			string CommandLine = Command;
			CommandLine += " /Module=\"" + MainModuleName + "\"";
			if( SplashHandle.Length > 0 )
			{
				CommandLine += " /SplashHandle=" + SplashHandle;
			}

			if( InstallInfoData.InstallGuidString.Length > 0 )
			{
				CommandLine += " /InstallGuid=" + InstallInfoData.InstallGuidString;
			}

			if( bSkipDependencies )
			{
				CommandLine += " /SkipDependencies";
			}

			if( bProgressOnly )
			{
				CommandLine += " /ProgressOnly";
			}

			return ( CommandLine );
		}

		public void Log( string Line, Color TextColour )
		{
			if( Line == null )
			{
				return;
			}

			DateTime Now = DateTime.Now;
			string FullLine = DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Line;

			// Write to the console
			Console.WriteLine( FullLine );
		}

		public bool CopyDirectory( string SourceDirectory, string TargetDirectory, bool Overwrite )
		{

			// Return early if the source directory does not exist
			if( !Directory.Exists( SourceDirectory ) )
			{
				return ( false );
			}

			// Create the target directory if it does not exist
			if( !Directory.Exists( TargetDirectory ) )
			{
				try
				{
					Directory.CreateDirectory( TargetDirectory );
				}
				catch( Exception Ex )
				{
					Console.WriteLine( Ex.Message );
				}
			}

			string[] SourceSubDirectories = Directory.GetDirectories( SourceDirectory, "*", SearchOption.AllDirectories );

			// Make sure all the target sub directories exist
			foreach( string SourceDirectoryPath in Directory.GetDirectories( SourceDirectory, "*", SearchOption.AllDirectories ) )
			{
				string TargetDirectoryPath = SourceDirectoryPath.Replace( SourceDirectory, TargetDirectory );
				try
				{

					Directory.CreateDirectory( TargetDirectoryPath );
				}
				catch( Exception Ex )
				{
					Console.WriteLine( Ex.Message );
				}
			}

			string[] SourceFiles = Directory.GetFiles( SourceDirectory, "*.*", SearchOption.AllDirectories );

			// Copy all the files from the SourceDirectory directory into the targetDirectory
			for( int SourceFileIndex = 0; SourceFileIndex < SourceFiles.Length; SourceFileIndex++ )
			{
				UpdateSubProgressBar( SourceFileIndex, SourceFiles.Length );
				string TargetFilePath = SourceFiles[SourceFileIndex].Replace( SourceDirectory, TargetDirectory );
				try
				{
					File.Copy( SourceFiles[SourceFileIndex], TargetFilePath, Overwrite );
				}
				catch( Exception Ex )
				{
					Console.WriteLine( Ex.Message );
				}
			}

			// Max the progress bar.
			UpdateSubProgressBar( 1, 1 );

			return ( true );
		}

		public void CopyBackupFiles( string SourceDirectory )
		{

			// If the Project Wizard Base Template folder exists, we copy to it
			string TargetBackupDirectory = Path.Combine( SourceDirectory, BackupDataDirectory );
			if( Directory.Exists( TargetBackupDirectory ) )
			{
				UpdateProgressBar( GetPhrase( "PBCreatingBackup" ) );

				List<string> BackupDirectories = new List<string>()
				{
					"Binaries\\",
					"Development\\",
					"Engine\\"
				};

				// Add to the BackupDirectories all the items in UDKGame except for Content and ProjectTemplates
				foreach( string DirectoryPath in Directory.GetDirectories( Path.Combine( SourceDirectory, "UDKGame\\" ), "*", SearchOption.TopDirectoryOnly ) )
				{
					if( !DirectoryPath.EndsWith( "UDKGame\\Content" ) &&
						!DirectoryPath.EndsWith( "UDKGame\\ProjectTemplates" ) )
					{
						BackupDirectories.Add( DirectoryPath + "\\" );
					}
				}

				for( int SourceDirIndex = 0; SourceDirIndex < BackupDirectories.Count; SourceDirIndex++ )
				{
					UpdateProgressBar( ".\\" + BackupDirectories[SourceDirIndex], SourceDirIndex, BackupDirectories.Count );
					CopyDirectory( Path.Combine( SourceDirectory, BackupDirectories[SourceDirIndex] ), Path.Combine( TargetBackupDirectory, BackupDirectories[SourceDirIndex] ), false );
				}

				UpdateProgressBar( "", BackupDirectories.Count, BackupDirectories.Count );

			}
		}


		static public void CenterFormToPrimaryMonitor( Form InForm )
		{
			Screen PrimaryScreen = Screen.PrimaryScreen;

			int NewX = ( PrimaryScreen.Bounds.Left + PrimaryScreen.Bounds.Right - InForm.Width ) / 2;
			int NewY = ( PrimaryScreen.Bounds.Top + PrimaryScreen.Bounds.Bottom - InForm.Height ) / 2;

			InForm.SetDesktopLocation( NewX, NewY );
		}
	}
}
