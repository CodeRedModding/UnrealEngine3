/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Drawing;
using System.IO;
using System.Text;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;

namespace UnrealDVDLayout
{
	public class InstallDesignerDataSetEULA
	{
		public int LCID;
		public string Path;
	}

	public class InstallDesignerDataSetFile
	{
		public string Path;

		public InstallDesignerDataSetFile()
		{
		}

		public InstallDesignerDataSetFile( string SourcePath )
		{
			Path = SourcePath;
		}
	}

	public class InstallDesignerDataSetFirewall
	{
		public string Path;
	}

	public class InstallDesignerDataSetLanguage
	{
		public int LCID;
		public string BannerPath;
		public string ProductName;
		public string Publisher;
	}

	public class InstallDesignerDataSetPackage
	{
		public string Type;
		public string SourceFile;
		public string ShowUI;
		public string InstallArguments;
	}

	public class InstallDesignerDataSetProperty
	{
		public string Name;
		public string Value;
	}

	public class InstallDesignerDataSetRegistry
	{
		public string Root;
		public string Key;
		public string Name;
		public string Type;
		public string Value;
	}

	public class InstallDesignerDataSetTask
	{
		public int LCID;
		public int Index;
		public string Type;
		public string SubType;
		public string Name;
		public string Path;
		public string Arguments;
		public string Link;
	}

	[XmlRoot( Namespace = "urn:schemas-microsoft-com:InstallDesignerProject.v1" )]
	public class InstallDesignerDataSet
	{
		[XmlElementAttribute( "Property" )]
		public List<InstallDesignerDataSetProperty> Properties = new List<InstallDesignerDataSetProperty>();

		[XmlElementAttribute( "Registry" )]
		public List<InstallDesignerDataSetRegistry> Registries = new List<InstallDesignerDataSetRegistry>();

		[XmlElementAttribute( "File" )]
		public List<InstallDesignerDataSetFile> Files = new List<InstallDesignerDataSetFile>();

		[XmlElementAttribute( "EULA" )]
		public List<InstallDesignerDataSetEULA> EULAs = new List<InstallDesignerDataSetEULA>();

		[XmlElementAttribute( "Language" )]
		public List<InstallDesignerDataSetLanguage> Languages = new List<InstallDesignerDataSetLanguage>();

		[XmlElementAttribute( "Package" )]
		public List<InstallDesignerDataSetPackage> Packages = new List<InstallDesignerDataSetPackage>();

		[XmlElementAttribute( "Firewall" )]
		public List<InstallDesignerDataSetFirewall> Firewalls = new List<InstallDesignerDataSetFirewall>();
	
		[XmlElementAttribute( "Task" )]
		public List<InstallDesignerDataSetTask> Tasks = new List<InstallDesignerDataSetTask>();

		public string GetSupportDir()
		{
			string SupportDir = Path.Combine( Environment.CurrentDirectory, ".." );

			foreach( InstallDesignerDataSetProperty Property in Properties )
			{
				if( Property.Name == "[SupportDir]" )
				{
					SupportDir = Property.Value;
				}
			}

			SupportDir = Path.GetFullPath( Path.Combine( "..", SupportDir ) );
			return ( SupportDir );
		}

		// Add an object based on a TOC entry
		public bool AddObject( TOCInfo TOCEntry, int Layer )
		{
			// do not append files that are not included
			if( Layer == -1 || TOCEntry.Type != UnrealDVDLayout.ObjectType.File )
			{
				return false;
			}

			string FullName = "[APPLICATIONROOTDIRECTORY]" + Path.Combine( TOCEntry.SubDirectory, TOCEntry.Name );
			bool bExists = false;
			foreach( InstallDesignerDataSetFile File in Files )
			{
				if( File.Path == FullName )
				{
					bExists = true;
					break;
				}
			}

			if( !bExists )
			{
				InstallDesignerDataSetFile File = new InstallDesignerDataSetFile( FullName );
				Files.Add( File );
			}

			return ( true );
		}

		public bool AddSupportObject( string FileName )
		{
			string FullName = "[SupportDir]" + FileName;
			bool bExists = false;
			foreach( InstallDesignerDataSetFile File in Files )
			{
				if( File.Path == FullName )
				{
					bExists = true;
					break;
				}
			}

			if( !bExists )
			{
				InstallDesignerDataSetFile File = new InstallDesignerDataSetFile( FullName );
				Files.Add( File );
			}

			return ( true );
		}
	}

	partial class UnrealDVDLayout
	{
		private void AddGroupToIDP( string GroupName )
		{
			// Get the group in order and sort the component files
			TOCGroup Group = TableOfContents.GetGroup( GroupName );
			List<TOCInfo> Entries = TableOfContents.GetEntriesInGroup( Group );
			TableOfContents.ApplySort( Entries );

			// Add to the DVD
			foreach( TOCInfo Entry in Entries )
			{
				PCDiscLayout.AddObject( Entry, Group.Layer );
			}
		}

		private void RecursiveAddSupportObjects( string RootDir, string SupportDir )
		{
			DirectoryInfo DirInfo = new DirectoryInfo( SupportDir );
			if( DirInfo.Exists )
			{
				foreach( FileInfo Info in DirInfo.GetFiles() )
				{
					PCDiscLayout.AddSupportObject( Info.FullName.Substring( RootDir.Length ) );
				}

				foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
				{
					RecursiveAddSupportObjects( RootDir, SubDirInfo.FullName );
				}
			}
		}

		private void CreateInstallDesignerProject( string LayoutID )
		{
			if( IDPTemplateName.Length > 0 )
			{
				// Create the gp3 file by reading in the template
				PCDiscLayout = UnrealControls.XmlHandler.ReadXml<InstallDesignerDataSet>( IDPTemplateName );
				if( PCDiscLayout.Properties == null || PCDiscLayout.Properties.Count == 0 )
				{
					Error( "Could not load IDP template .... " + IDPTemplateName );
				}
				else
				{
					// Add all the objects
					foreach( string GroupName in TableOfContents.Groups.TOCGroupLayer0 )
					{
						AddGroupToIDP( GroupName );
					}

					// Look for [SupportDir] in the PCDiscLayout
					string SupportDir = PCDiscLayout.GetSupportDir();
					// Add all the support files
					RecursiveAddSupportObjects( SupportDir, SupportDir );

					if( LayoutID != null )
					{
						//	Log( UnrealDVDLayout.VerbosityLevel.Informative, "[REPORT] " + PCDiscLayout.GetSummary(), Color.Blue );
						//	Log( UnrealDVDLayout.VerbosityLevel.Informative, "[PERFCOUNTER] " + Options.GameName + PCDiscLayout.GetPerfSummary(), Color.Blue );
						//	Log( UnrealDVDLayout.VerbosityLevel.Informative, "[PERFCOUNTER] PCDVDCapacity " + PCDiscLayout.GetCapacity(), Color.Blue );
					}
				}
			}
		}

		public void SaveIDP()
		{
			if( IDPTemplateName.Length == 0 )
			{
				Error( "No InstallDesignerProject template set ...." );
			}
			else
			{
				// Create the GP3 file from the template
				CreateInstallDesignerProject( "Layout" );

				// Save it out
				GenericSaveFileDialog.Title = "Select InstallDesignerProject file to export...";
				GenericSaveFileDialog.DefaultExt = "*.InstallDesignerProject";
				GenericSaveFileDialog.Filter = "InstallDesignerProject files (*.InstallDesignerProject)|*.InstallDesignerProject";
				GenericSaveFileDialog.InitialDirectory = Environment.CurrentDirectory;
				if( GenericSaveFileDialog.ShowDialog() == DialogResult.OK )
				{
					Log( VerbosityLevel.Informative, "Saving ... '" + GenericSaveFileDialog.FileName + "'", Color.Blue );
					if( UnrealControls.XmlHandler.WriteXml<InstallDesignerDataSet>( PCDiscLayout, GenericSaveFileDialog.FileName, "urn:schemas-microsoft-com:InstallDesignerProject.v1" ) )
					{
						Log( VerbosityLevel.Informative, " ... successful", Color.Blue );
					}
				}
			}
		}

		public void SaveIDPISO()
		{
			if( IDPTemplateName.Length == 0 )
			{
				Error( "No InstallDesignerProject template set ...." );
			}
			else
			{
				// Create a layout based on the latest data
				CreateInstallDesignerProject( "Layout" );

				// Save it out
				GenericFolderBrowserDialog.Description = "Select folder to export installer to...";
				if( GenericFolderBrowserDialog.ShowDialog() == DialogResult.OK )
				{
					// Save temp
					string TempIDPName = Path.Combine( Environment.CurrentDirectory, Options.GameName + "Game\\Build\\PCConsole\\Temp.InstallDesignerProject" );
					if( UnrealControls.XmlHandler.WriteXml<InstallDesignerDataSet>( PCDiscLayout, TempIDPName, "urn:schemas-microsoft-com:InstallDesignerProject.v1" ) )
					{
						SaveInstallerFiles( TempIDPName, GenericFolderBrowserDialog.SelectedPath );
					}
				}
			}
		}

		public void SaveInstallerFiles( string IDPFileName, string FolderName )
		{
			string GFWLSDKPath = Environment.GetEnvironmentVariable( "GFWLSDK_DIR" );
			if( GFWLSDKPath != null )
			{
				Log( VerbosityLevel.Informative, "Spawning InstallGenerator to create submission in ... '" + FolderName + "'", Color.Blue );
				string CWD = Path.GetFullPath( Path.Combine( Environment.CurrentDirectory, ".." ) );
				string FileName = IDPFileName.Substring( CWD.Length + 1 );
				string RootFolder = "-r " + CWD;
				string OutputFolder = "-o " + FolderName;
				string Key = "-pfx " + Options.KeyLocation;
				string KeyPassword = "-pfxpw " + Options.KeyPassword;
				string WorkingFolder = "-w c:\\temp";
				// Create install files
				SpawnProcess( Path.Combine( GFWLSDKPath, "tools/InstallDesigner/InstallGenerator.exe" ), CWD, RootFolder, OutputFolder, WorkingFolder, Key, KeyPassword, IDPFileName );
				// Create submission packages
				SpawnProcess( Path.Combine( GFWLSDKPath, "tools/InstallDesigner/InstallGenerator.exe" ), CWD, RootFolder, OutputFolder, WorkingFolder, "-submission", IDPFileName );
			}
		}
	}
}
