/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Text;

namespace UnrealLoc
{
    public class FileEntry
    {
        private UnrealLoc Main = null;
        private LanguageInfo Lang = null;

		public string RelativeName { get; set; }
		public DateTime LastWriteTime { get; set; }
		public bool bValidEncoding { get; set; }
		public bool HasNewLocEntries { get; set; }

        private ObjectEntryHandler FileObjectEntryHandler;

        public List<ObjectEntry> GetObjectEntries()
        {
            return ( FileObjectEntryHandler.GetObjectEntries() );
        }

        public int GetObjectCount()
        {
            return ( FileObjectEntryHandler.GetObjectCount() );
        }

        public void GenerateLocObjects( FileEntry DefaultFE )
        {
            FileObjectEntryHandler.GenerateLocObjects( DefaultFE );
        }

        public void RemoveOrphans()
        {
            FileObjectEntryHandler.RemoveOrphans();
        }

        public bool CreateDirectory( string FolderName )
        {
            DirectoryInfo DirInfo = new DirectoryInfo( FolderName );
            DirInfo.Create();

            return( true );
        }

        public bool WriteLocFiles()
        {
			if( !bValidEncoding )
			{
				Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... not creating loc file as the source format was invalid: " + RelativeName, Color.Black );
				return ( false );
			}

			Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... creating loc file: " + RelativeName, Color.Black );

            FileInfo LocFileInfo = new FileInfo( RelativeName );
            CreateDirectory( LocFileInfo.DirectoryName );

            if( !Main.AddToSourceControl( Lang, RelativeName ) )
            {
				return( false );
			}
               
			Lang.FilesCreated++;
			LocFileInfo = new FileInfo( RelativeName );

            if( !LocFileInfo.IsReadOnly || !LocFileInfo.Exists )
            {
                StreamWriter File = new StreamWriter( RelativeName, false, System.Text.Encoding.Unicode );
                FileObjectEntryHandler.WriteLocFiles( File );
                File.Close();
                return ( true );
            }
            else
            {
                Main.Log(UnrealLoc.VerbosityLevel.Informative, " ... unable to write to read only file: " + RelativeName, Color.Red);          
            }

            return ( false );
        }

        public bool WriteDiffLocFiles( string Folder )
        {
            string LocFileName = Folder + "\\" + Lang.LangID + "\\" + RelativeName;
            Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... creating loc diff file: " + LocFileName, Color.Black );
            FileInfo LocFileInfo = new FileInfo( LocFileName );
            CreateDirectory( LocFileInfo.DirectoryName );

            if( LocFileInfo.Exists )
            {
                LocFileInfo.IsReadOnly = false;
                LocFileInfo.Delete();
            }

            StreamWriter File = new StreamWriter( LocFileName, false, System.Text.Encoding.Unicode );
            FileObjectEntryHandler.WriteDiffLocFiles( File );
            File.Close();

            return ( false );
        }

        public bool ImportText( string FileName )
        {
            return( FileObjectEntryHandler.ImportText( FileName ) );
        }

        public FileEntry( UnrealLoc InMain, LanguageInfo InLang, string InRelativeName, DateTime InLastWriteTime )
        {
            Main = InMain;
            Lang = InLang;
            RelativeName = InRelativeName;
			LastWriteTime = InLastWriteTime;
            FileObjectEntryHandler = new ObjectEntryHandler( Main, Lang, this );
            bValidEncoding = FileObjectEntryHandler.FindObjects();
        }

        public FileEntry( UnrealLoc InMain, LanguageInfo InLang, FileEntry DefaultFE )
        {
            Main = InMain;
            Lang = InLang;
			bValidEncoding = true;

            RelativeName = DefaultFE.RelativeName;
            RelativeName = RelativeName.Replace( ".INT","." + Lang.LangID );
            RelativeName = RelativeName.Replace( "\\INT\\", "\\" + Lang.LangID + "\\" );

            FileObjectEntryHandler = new ObjectEntryHandler( Main, Lang, this );
        }
    }

    public class FileEntryHandler
    {
        private UnrealLoc Main = null;
        private LanguageInfo Lang = null;
        public List<FileEntry> FileEntries;

        public FileEntryHandler( UnrealLoc InMain, LanguageInfo InLang )
        {
            Main = InMain;
            Lang = InLang;
            FileEntries = new List<FileEntry>();
        }

		public string CanoniseName( string FileName )
		{
			FileName = Path.GetFullPath( FileName );
			string Folder = Path.GetDirectoryName( FileName );
			string Name = Path.GetFileNameWithoutExtension( FileName ).ToLower();
			string Extension = Path.GetExtension( FileName ).ToUpper();
			return ( Folder + "\\" + Name + Extension );
		}

        public FileEntry CreateFile( FileEntry DefaultFileEntry )
        {
            FileEntry FileElement = new FileEntry( Main, Lang, DefaultFileEntry );
            FileEntries.Add( FileElement );

            Main.Log( UnrealLoc.VerbosityLevel.Informative, "Created file '" + FileElement.RelativeName + "'", Color.Blue );
            return ( FileElement );
        }

		public bool AddFile( FileEntry OldFileEntry, string LangFolder, FileInfo Info )
        {
			// Remove old file entry
			if( OldFileEntry != null )
			{
				FileEntries.Remove( OldFileEntry );
			}

			// Remember uppercase extension, lowercase root name and relative name
			string FileName = CanoniseName( Info.FullName );

			string RootPath = Path.GetFullPath( LangFolder );
			string RelativeName = Path.Combine( LangFolder, FileName.Substring( RootPath.Length ) );

			// Check for duplicates
			if( OldFileEntry == null )
			{
				// FIXME: Linear search
				foreach( FileEntry FE in FileEntries )
				{
					if( FE.RelativeName == RelativeName )
					{
						return ( false );
					}
				}
			}

            FileEntry FileElement = new FileEntry( Main, Lang, RelativeName, Info.LastWriteTimeUtc );
            FileEntries.Add( FileElement );
            return ( true );
        }

		private FileEntry FindLocFileEntry( FileEntry DefaultFE )
        {
            foreach( FileEntry ExistingFE in FileEntries )
            {
				// Check for loc in the same folder (e.g. DLC)
				string RootName = Path.GetDirectoryName( ExistingFE.RelativeName ) + "\\" + Path.GetFileNameWithoutExtension( ExistingFE.RelativeName );
				string LocRootName = Path.GetDirectoryName( DefaultFE.RelativeName ) + "\\" + Path.GetFileNameWithoutExtension( DefaultFE.RelativeName );
                if( RootName == LocRootName )
                {
                    return ( ExistingFE );
                }

				// Check for loc in a parallel folder (e.g. full loc)
				LocRootName = LocRootName.Replace( "\\INT\\", "\\" + Lang.LangID + "\\" );
				if( RootName == LocRootName )
				{
					return ( ExistingFE );
				}
            }

            return ( null );
        }

		private FileEntry FileExists( string FileName )
		{
			foreach( FileEntry ExistingFE in FileEntries )
			{
				// Check for loc in the same folder (e.g. DLC)
				string RootName = Path.GetDirectoryName( ExistingFE.RelativeName ) + "\\" + Path.GetFileNameWithoutExtension( ExistingFE.RelativeName );
				string LocRootName = Path.GetDirectoryName( FileName ) + "\\" + Path.GetFileNameWithoutExtension( FileName );

				if( LocRootName.EndsWith( RootName ) )
				{
					return ( ExistingFE );
				}

				// Check for loc in a parallel folder (e.g. full loc)
				LocRootName = LocRootName.Replace( "\\INT\\", "\\" + Lang.LangID + "\\" );
				if( RootName == LocRootName )
				{
					return ( ExistingFE );
				}
			}

			return ( null );
		}

		private FileEntry FindFileEntry( string RelativeName )
		{
			foreach( FileEntry ExistingFE in FileEntries )
			{
				if( RelativeName == ExistingFE.RelativeName )
				{
					return ( ExistingFE );
				}
			}

			return ( null );
		}

        public List<FileEntry> GetFileEntries()
        {
            return ( FileEntries );
        }

        public int GetCount()
        {
            return( FileEntries.Count );
        }

        public bool GenerateLocFiles( LanguageInfo DefaultLangInfo )
        {
            List<FileEntry> DefaultFileEntries = DefaultLangInfo.GetFileEntries();
            foreach( FileEntry DefaultFE in DefaultFileEntries )
            {
                FileEntry LocFE = FindLocFileEntry( DefaultFE );
                if( LocFE == null )
                {
                    LocFE = CreateFile( DefaultFE );
                }

                LocFE.GenerateLocObjects( DefaultFE );
            }

            return ( true );
        }

        public void RemoveOrphans()
        {
            foreach( FileEntry FE in FileEntries )
            {
                FE.RemoveOrphans();
            }
        }

        public bool WriteLocFiles()
        {
            foreach( FileEntry FE in FileEntries )
            {
                FE.WriteLocFiles();
            }

            return ( true );
        }

        public bool WriteDiffLocFiles( string Folder )
        {
            foreach( FileEntry FE in FileEntries )
            {
                if( FE.HasNewLocEntries )
                {
                    FE.WriteDiffLocFiles( Folder );
                }
            }

            return ( true );
        }

        private string GetRelativePath()
        {
            string LangFolder = "";

            if( Main.GameName == "Engine" )
            {
				LangFolder = Main.GameName + "\\Localization\\" + Lang.LangID + "\\";
            }
            else
            {
				if( Main.bDLCProfile )
				{
					LangFolder = Main.GameName + "Game\\DLC\\";
				}
				else
				{
					LangFolder = Main.GameName + "Game\\Localization\\" + Lang.LangID + "\\";
				}
            }

            return ( LangFolder );
        }

		public void RecursiveFindLocFiles( bool bRecurse, DirectoryInfo DirInfo, string RootFolder, string ExtLangID )
		{
			if( bRecurse )
			{
				foreach( DirectoryInfo SubDirInfo in DirInfo.GetDirectories() )
				{
					RecursiveFindLocFiles( true, SubDirInfo, RootFolder, ExtLangID );
				}
			}

			foreach( FileInfo File in DirInfo.GetFiles() )
			{
				if( File.Extension.ToUpper() == ExtLangID )
				{
					AddFile( null, RootFolder, File );
				}
			}
		}

        public bool FindLocFiles()
        {
            string LangFolder = GetRelativePath();
            string ExtLangID = "." + Lang.LangID;

            DirectoryInfo DirInfo = new DirectoryInfo( LangFolder );
            if( DirInfo.Exists )
            {
				RecursiveFindLocFiles( Main.bDLCProfile, DirInfo, LangFolder, ExtLangID );

                Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... found " + GetCount().ToString() + " files for " + Lang.LangID, Color.Black );
                return ( true );
            }

            return ( false );
        }

		public void ReloadChangedFiles()
		{
			string LangFolder = GetRelativePath();
			string ExtLangID = "." + Lang.LangID;

			DirectoryInfo DirInfo = new DirectoryInfo( LangFolder );
			if( DirInfo.Exists )
			{
				foreach( FileInfo File in DirInfo.GetFiles() )
				{
					if( File.Extension.ToUpper() == ExtLangID )
					{
						// Find FileEntry
						string RelativeName = File.FullName.Substring( Main.BranchRootFolder.Length + 1 );
						FileEntry FE = FindFileEntry( RelativeName );

						if( FE != null )
						{
							// Check timestamps
							if( FE.LastWriteTime < File.LastWriteTimeUtc )
							{
								Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... updating " + RelativeName, Color.Black );
								AddFile( FE, LangFolder, File );
							}
						}
					}
				}
			}
		}

        public bool ImportText( string FileName )
        {
			FileName = CanoniseName( FileName );
			FileEntry FE = FileExists( FileName );
            if( FE == null )
            {
				Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... INT version of '" + FileName + "' not found.", Color.Red );
                return ( false );
            }

            return( FE.ImportText( FileName ) );
        }
    }
}
