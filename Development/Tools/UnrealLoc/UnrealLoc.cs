/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Reflection;
using System.Text;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;

// TODO:
// Handle comments
// Need better validation
// Need to get a valid list of chars per language
// Need to handle when an English string changes

namespace UnrealLoc
{
    public partial class UnrealLoc : Form
    {
        public enum VerbosityLevel
        {
            Silent,
            Critical,
            Simple,
            Informative,
            Complex,
            Verbose,
            ExtraVerbose
        };

		public enum EFileType
		{
			UTF16Mergable,
			BinaryExclusive
		};

        private UnrealControls.OutputWindowDocument MainLogDoc = new UnrealControls.OutputWindowDocument();
        public bool Ticking = false;
        public SettableOptions Options = null;
        public string GameName = "UDK";
		public bool bDLCProfile = false;
		public string BranchRootFolder = "";
        private List<LanguageInfo> LanguageInfos = new List<LanguageInfo>();
        private P4 SourceControl = null;
        private Validator TheCorrector = null;
        public LanguageInfo DefaultLanguageInfo = null;

        delegate void DelegateAddLine( VerbosityLevel Verbosity, string Line, Color TextColor );

        public UnrealLoc()
        {
            InitializeComponent();
            MainLogWindow.Document = MainLogDoc;
        }

        public void Init()
        {
            Ticking = true;

			string OptionsName = Path.Combine( Path.GetDirectoryName( Assembly.GetEntryAssembly().Location ), "UnrealLoc.Settings.xml" );
			Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( OptionsName );

            LanguageInfos.Add( new LanguageInfo( this, "INT", CheckBox_INT ) );
			LanguageInfos.Add( new LanguageInfo( this, "FRA", CheckBox_FRA ) );
#if !DEBUG
			LanguageInfos.Add( new LanguageInfo( this, "ITA", CheckBox_ITA ) );
			LanguageInfos.Add( new LanguageInfo( this, "DEU", CheckBox_DEU ) );
			LanguageInfos.Add( new LanguageInfo( this, "ESN", CheckBox_ESN ) );
			LanguageInfos.Add( new LanguageInfo( this, "ESM", CheckBox_ESM ) );
			LanguageInfos.Add( new LanguageInfo( this, "PTB", CheckBox_PTB ) );
			LanguageInfos.Add( new LanguageInfo( this, "RUS", CheckBox_RUS ) );
			LanguageInfos.Add( new LanguageInfo( this, "POL", CheckBox_POL ) );
			LanguageInfos.Add( new LanguageInfo( this, "HUN", CheckBox_HUN ) );
			LanguageInfos.Add( new LanguageInfo( this, "CZE", CheckBox_CZE ) );
			LanguageInfos.Add( new LanguageInfo( this, "SLO", CheckBox_SLO ) );
			LanguageInfos.Add( new LanguageInfo( this, "JPN", CheckBox_JPN ) );
			LanguageInfos.Add( new LanguageInfo( this, "KOR", CheckBox_KOR ) );
            LanguageInfos.Add( new LanguageInfo( this, "CHN", CheckBox_CHN ) );
#endif
			LanguageInfos.Add( new LanguageInfo( this, "XXX", CheckBox_XXX ) );

            Log( UnrealLoc.VerbosityLevel.Critical, "Welcome to UnrealLoc - (c) 2011", Color.Black );

			// Find the root of the branch
			Assembly EntryAssembly = Assembly.GetEntryAssembly();
			BranchRootFolder = Path.GetFullPath( Path.Combine( Path.GetDirectoryName( EntryAssembly.Location ), ".." ) );
			
			// Create an object to validate the text
            TheCorrector = new Validator( this );

			// Pick the game we wish to work on
            PickGame Picker = new PickGame( this );
            Picker.ShowDialog();

            if( GameName.Length > 0 )
            {
                Log( UnrealLoc.VerbosityLevel.Critical, "Scanning loc data for game " + GameName, Color.Green );

                Show();

                ScanLocFolders();

                Log( UnrealLoc.VerbosityLevel.Critical, " ... finished scanning loc data", Color.Green );

				AppDomain.CurrentDomain.AssemblyResolve += new ResolveEventHandler( CurrentDomain_AssemblyResolve );
            }
            else
            {
                Ticking = false;
            }
        }

        public void Destroy()
        {
			string OptionsName = Path.Combine( Path.GetDirectoryName( Assembly.GetEntryAssembly().Location ), "UnrealLoc.Settings.xml" );
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, OptionsName, "" );
        }

		private Assembly CurrentDomain_AssemblyResolve( Object sender, ResolveEventArgs args )
		{
			// Name is fully qualified assembly definition - e.g. "p4dn, Version=1.0.0.0, Culture=neutral, PublicKeyToken=ff968dc1933aba6f"
			string[] AssemblyInfo = args.Name.Split( ",".ToCharArray() );
			string AssemblyName = AssemblyInfo[0];

			if( AssemblyName.ToLower() == "p4dn" || AssemblyName.ToLower() == "p4api" )
			{
				if( IntPtr.Size == 8 )
				{
					AssemblyName = Application.StartupPath + "\\Win64\\" + AssemblyName + ".dll";
				}
				else
				{
					AssemblyName = Application.StartupPath + "\\Win32\\" + AssemblyName + ".dll";
				}

				Debug.WriteLineIf( Debugger.IsAttached, "Loading assembly: " + AssemblyName );

				Assembly P4NET = Assembly.LoadFile( AssemblyName );
				return ( P4NET );
			}

			return ( null );
		}

        private void UnrealLoc_FormClosed( object sender, FormClosedEventArgs e )
        {
            Ticking = false;
        }

        public void Log( VerbosityLevel Verbosity, string Line, Color TextColour )
        {
            if( Verbosity > Options.Verbosity )
            {
                return;
            }

            if( Line == null || !Ticking )
            {
                return;
            }

            // if we need to, invoke the delegate
            if( InvokeRequired )
            {
                Invoke( new DelegateAddLine( Log ), new object[] { Verbosity, Line, TextColour } );
                return;
            }

            DateTime Now = DateTime.Now;
			string FullLine = DateTime.Now.ToString( "HH:mm:ss" ) + ": " + Line;

            MainLogDoc.AppendText( TextColour, FullLine + "\r\n" );
        }

        public void Log( VerbosityLevel Verbosity, Array Lines, Color TextColour )
        {
            foreach( string Line in Lines )
            {
                Log( Verbosity, Line, TextColour );
            }
        }

        public void Warning( LanguageInfo Lang, string Line )
        {
            Lang.AddWarning( Line );
        }

        public void Error( LanguageInfo Lang, string Line )
        {
            Lang.AddError( Line );
        }

        public bool AddToSourceControl( LanguageInfo Lang, string Name )
        {
            if( !Options.bUseSourceControl )
            {
                // We're not using source control, so we want to write the files if possible
                return ( true );
            } 
            
            if( SourceControl == null )
            {
                SourceControl = new P4( this );
            }

            bool Success = SourceControl.AddToSourceControl( Lang, Name );
            return ( Success );
        }

        public bool RevertUnchanged()
        {
			if( !Options.bUseSourceControl )
			{
				return ( false );
			}

            if( SourceControl == null )
            {
                SourceControl = new P4( this );
            }

            SourceControl.RevertUnchanged();
            return ( true );
        }

        private void RecursiveGetLangFiles( string LangID, string Folder, List<string> Files )
        {
            DirectoryInfo DirInfo = new DirectoryInfo( Folder );

            foreach( FileInfo Info in DirInfo.GetFiles() )
            {
                if( Info.Extension.ToUpper() == LangID )
                {
                    Files.Add( Info.FullName );
                }
            }

            foreach( DirectoryInfo Dirs in DirInfo.GetDirectories() )
            {
                RecursiveGetLangFiles( LangID, Dirs.FullName, Files );
            }
        }

        private void DumpErrorsAndWarnings()
        {
            foreach( LanguageInfo Lang in LanguageInfos )
            {
                if( Lang.LangExists && Lang.LangCheckBox.Checked )
                {
                    Lang.ErrorSummary();
                    Lang.WarningSummary();
                }
            }
        }

        private void ScanLocFolders()
        {
            // Scan the existing data for the current state of the loc data
            foreach( LanguageInfo Lang in LanguageInfos )
            {
				Log( UnrealLoc.VerbosityLevel.Simple, " ... scanning loc data for " + Lang.LangID, Color.Green );

				Lang.ResetCounts();
				Lang.LangFileHandler = new FileEntryHandler( this, Lang );
                Lang.LangCheckBox.Checked = false;

                if( !Lang.FindLocFiles() )
                {
                    continue;
                }

                Lang.LangExists = true;
                Lang.LangCheckBox.Checked = Lang.LangExists;

                if( Lang.LangID == "INT" )
                {
                    DefaultLanguageInfo = Lang;
                }

                Application.DoEvents();

				Log( UnrealLoc.VerbosityLevel.Simple, " ... validating loc data for " + Lang.LangID, Color.Green );
				
				Lang.GenerateLocFiles( DefaultLanguageInfo );

				Log( UnrealLoc.VerbosityLevel.Simple, " ... finished processing " + Lang.LangID, Color.Green );

				Application.DoEvents();
			}

            // Dump any warnings or errors that were found
            DumpErrorsAndWarnings();
        }

        private void Button_GenLocFiles_Click( object Sender, EventArgs E )
        {
			MainLogWindow.ScrollToEnd();
			
			Log( VerbosityLevel.Critical, "Generating loc files for " + GameName + " ...", Color.Green );

            foreach( LanguageInfo Lang in LanguageInfos )
            {
                if( Lang.LangCheckBox.Checked )
                {
                    if( !Lang.LocFilesGenerated )
                    {
                        Lang.GenerateLocFiles( DefaultLanguageInfo );
                    }

                    Lang.WriteLocFiles();
                }

                Application.DoEvents();
            }

            RevertUnchanged();

            // Dump any warnings or errors that were found
            DumpErrorsAndWarnings();

            Log( VerbosityLevel.Critical, " ... finished generating missing loc files", Color.Green );
        }

		private void ReloadChangedFilesClick( object sender, EventArgs e )
		{
			MainLogWindow.ScrollToEnd();

			Log( VerbosityLevel.Critical, "Reloading changed files for " + GameName + " ...", Color.Green );

			foreach( LanguageInfo Lang in LanguageInfos )
			{
				if( Lang.LangCheckBox.Checked )
				{
					Lang.ResetCounts();

					Lang.ReloadChangedFiles();

					Application.DoEvents();

					Lang.GenerateLocFiles( DefaultLanguageInfo );

					Application.DoEvents();
				}
			}

			Log( VerbosityLevel.Critical, " ... finished reloading changed files", Color.Green );

			// Dump any warnings or errors that were found
			DumpErrorsAndWarnings();
		}

        private void Button_SaveWarnings_Click( object sender, EventArgs e )
        {
			MainLogWindow.ScrollToEnd();
			
			GenericFolderBrowser.Description = "Select the folder where you wish the warnings and errors to be written...";
            if( GenericFolderBrowser.ShowDialog() == DialogResult.OK )
            {
                foreach( LanguageInfo Lang in LanguageInfos )
                {
                    if( Lang.LangExists && Lang.LangCheckBox.Checked )
                    {
                        string ReportName = GenericFolderBrowser.SelectedPath + "\\Report_" + Lang.LangID + ".txt";
                        Log( VerbosityLevel.Simple, "Writing: " + ReportName, Color.Black );
                        StreamWriter Writer = new StreamWriter( ReportName, false, System.Text.Encoding.Unicode );
                        Lang.ErrorSummary( Writer );
                        Lang.WarningSummary( Writer );
                        Writer.Close();
                    }
                }
            }
        }

        private void Button_GenDiffFiles_Click( object sender, EventArgs e )
        {
			MainLogWindow.ScrollToEnd();
			
			Log( VerbosityLevel.Critical, "Generating the missing loc entries for " + GameName + " ...", Color.Green );

            GenericFolderBrowser.Description = "Select the folder where you wish the files with loc changes to go...";
            if( GenericFolderBrowser.ShowDialog() == DialogResult.OK )
            {
                foreach( LanguageInfo Lang in LanguageInfos )
                {
                    if( Lang.LangCheckBox.Checked )
                    {
                        if( !Lang.LocFilesGenerated )
                        {
                            Lang.GenerateLocFiles( DefaultLanguageInfo );
                        }

                        Lang.WriteDiffLocFiles( GenericFolderBrowser.SelectedPath );
                    }
                }
            }

            Log( VerbosityLevel.Critical, " ... finished generating the missing loc entries", Color.Green );
        }

        private void Button_ImportText_Click( object Sender, EventArgs E )
        {
			MainLogWindow.ScrollToEnd();
			
			Log( VerbosityLevel.Critical, "Importing localised text into " + GameName + " ...", Color.Green );

            GenericFolderBrowser.Description = "Select the folder where the loc data to import resides...";
			GenericFolderBrowser.SelectedPath = Options.ImportTextFolder;
            if( GenericFolderBrowser.ShowDialog() == DialogResult.OK )
            {
				Options.ImportTextFolder = GenericFolderBrowser.SelectedPath;

                // Update with the data from the new files
                foreach( LanguageInfo Lang in LanguageInfos )
                {
					List<string> LangFiles = new List<string>();

                    if( Lang.LangCheckBox.Checked )
                    {
						Lang.ResetCounts();
						
						if( !Lang.LocFilesGenerated )
                        {
                            Lang.GenerateLocFiles( DefaultLanguageInfo );
                        }

                        RecursiveGetLangFiles( "." + Lang.LangID, GenericFolderBrowser.SelectedPath, LangFiles );
                        Lang.ImportText( LangFiles );
                    }
                }

                // Dump any warnings or errors that were found
                DumpErrorsAndWarnings();

                Application.DoEvents();
            }
        }

        private void QuitToolStripMenuItem_Click( object sender, EventArgs e )
        {
            Ticking = false;
        }

        private void OptionsToolStripMenuItem_Click( object sender, EventArgs e )
        {
            OptionsDialog DisplayOptions = new OptionsDialog( this, Options );
            DisplayOptions.ShowDialog();
        }

        public string Validate( LanguageInfo Lang, ref LocEntry LE, bool bOuterQuotesRequired )
        {
            LE.Validate( Lang, LE.DefaultLE );
            return ( TheCorrector.Validate( Lang, ref LE, bOuterQuotesRequired ) );
        }
    }

    public class SettableOptions
    {
		static private string IntToString( int CharNum )
		{
			char Char = ( char )CharNum;
			return ( Char.ToString() );
		}

        [CategoryAttribute( "Settings" )]
        [DescriptionAttribute( "The amount of text spewed to the window." )]
        public UnrealLoc.VerbosityLevel Verbosity
        {
            get { return ( LocalVerbosity ); }
            set { LocalVerbosity = value; }
        }
        [XmlEnumAttribute]
        private UnrealLoc.VerbosityLevel LocalVerbosity = UnrealLoc.VerbosityLevel.Informative;

		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "Additional non ANSI characters allowed for Western European strings. The characters listed here are in addition to ASCII, high ASCII for codepage 1252 (barring multiply and divide), and shift space." )]
		public string AllowedWesternEuroCharactersAlt
		{
			get { return ( LocalAllowedWesternEuroCharacters ); }
			set { LocalAllowedWesternEuroCharacters = value; }
		}
		[XmlEnumAttribute]
		private string LocalAllowedWesternEuroCharacters = IntToString( 0x00a1 ) + IntToString( 0x00ab ) + IntToString( 0x00bb ) + IntToString( 0x00bf ) + IntToString( 0x00a5 ) + 
															IntToString( 0x00a3 ) + IntToString( 0x00a7 ) + IntToString( 0x00a9 ) + IntToString( 0x00ae ) + IntToString( 0x2014 ) +
															IntToString( 0x2026 ) + IntToString( 0x0152 ) + IntToString( 0x0153 ) + IntToString( 0x0178 ) + IntToString( 0x2122 ) +
															IntToString( 0x0160 ) + IntToString( 0x00ba ) + IntToString( 0x00b0 ) + IntToString( 0x00aa ) + IntToString( 0x010D ) +
															IntToString( 0x010c ) + IntToString( 0x0151 ) + IntToString( 0x0150 );

		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "Allowed strings." )]
		public string[] AllowedStrings
		{
			get { return ( LocalAllowedStrings ); }
			set { LocalAllowedStrings = value; }
		}
		[XmlAttribute]
		private string[] LocalAllowedStrings = new string[] {};

		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "Whether to remove orphans." )]
		public bool bRemoveOrphans
		{
			get { return ( LocalbRemoveOrphans ); }
			set { LocalbRemoveOrphans = value; }
		}
		[XmlEnumAttribute]
		private bool LocalbRemoveOrphans = true;

        [CategoryAttribute( "Source Control" )]
        [DescriptionAttribute( "Automatically checkout/add files that require it." )]
        public bool bUseSourceControl
        {
			get { return ( bLocalUseSourceControl ); }
			set { bLocalUseSourceControl = value; }
        }
        [XmlAttribute]
		private bool bLocalUseSourceControl = false;

		[CategoryAttribute( "Source Control" )]
		[DescriptionAttribute( "ClientSpec to operate on - blank is default." )]
		public string ClientSpec
		{
			get { return ( LocalClientSpec ); }
			set { LocalClientSpec = value; }
		}
		[XmlTextAttribute]
		private string LocalClientSpec = "";

		[CategoryAttribute( "Source Control" )]
		[DescriptionAttribute( "The Perforce type for newly generated localisation text files." )]
		public UnrealLoc.EFileType LocFileType
		{
			get { return ( LocalLocFileType ); }
			set { LocalLocFileType = value; }
		}
		[XmlTextAttribute]
		private UnrealLoc.EFileType LocalLocFileType = UnrealLoc.EFileType.UTF16Mergable;

        [CategoryAttribute( "Ellipses" )]
        [DescriptionAttribute( "Automatically convert ellipses and 4 or 5 periods in sequence to 3 periods." )]
        public bool bRemoveEllipses
        {
            get { return ( bLocalRemoveEllipses ); }
            set { bLocalRemoveEllipses = value; }
        }
        [XmlAttribute]
        private bool bLocalRemoveEllipses = false;

        [CategoryAttribute( "Ellipses" )]
        [DescriptionAttribute( "Automatically convert 3, 4 and 5 periods to an ellipsis." )]
        public bool bAddEllipses
        {
            get { return ( bLocalAddEllipses ); }
            set { bLocalAddEllipses = value; }
        }
        [XmlAttribute]
        private bool bLocalAddEllipses = false;

		[XmlElement]
		public string ImportTextFolder = Environment.CurrentDirectory;
    }

    public class LanguageInfo
    {
        private UnrealLoc Main = null;
        private List<string> Warnings = new List<string>();
        private List<string> Errors = new List<string>();

        public int FilesCreated = 0;
        public int ObjectsCreated = 0;
        public int LocCreated = 0;
        public int NumOrphansRemoved = 0;

        public string LangID { get; set; }
        public CheckBox LangCheckBox { get; set; }
        public bool LangExists { get; set; }
        public FileEntryHandler LangFileHandler { get; set; }
        public bool LocFilesGenerated { get; set; }

        public LanguageInfo( UnrealLoc InMain, string ID, CheckBox CB )
        {
            Main = InMain;

            LangID = ID;
            LangCheckBox = CB;
        }

        public void AddWarning( string Line )
        {
            Warnings.Add( Line );
        }

        public void AddError( string Line )
        {
            Errors.Add( Line );
        }

        public void WarningSummary()
        {
            if( Warnings.Count > 0 && Warnings.Count < 25 )
            {
                Main.Log( UnrealLoc.VerbosityLevel.Informative, "Warning summary for " + LangID + " -", Color.Black );
                foreach( string Warning in Warnings )
                {
                    Main.Log( UnrealLoc.VerbosityLevel.Informative, Warning, Color.Orange );
                }
            }
            else if( Warnings.Count > 0 )
            {
                Main.Log( UnrealLoc.VerbosityLevel.Informative, "Warning summary for " + LangID + " - " + Warnings.Count.ToString() + " warnings", Color.Black );
            }
        }

        public void ErrorSummary()
        {
            if( Errors.Count > 0 )
            {
                Main.Log( UnrealLoc.VerbosityLevel.Critical, "Error summary for " + LangID + " -", Color.Black );
                foreach( string Error in Errors )
                {
                    Main.Log( UnrealLoc.VerbosityLevel.Critical, Error, Color.Red );
                }
            }
        }

        public void WarningSummary( StreamWriter Writer )
        {
            if( Warnings.Count > 0 )
            {
                Writer.WriteLine( "Warning summary for " + LangID + " -" );
                foreach( string Warning in Warnings )
                {
                    Writer.WriteLine( Warning );
                }
            }
        }

        public void ErrorSummary( StreamWriter Writer )
        {
            if( Errors.Count > 0 )
            {
                Writer.WriteLine( "Error summary for " + LangID + " -" );
                foreach( string Error in Errors )
                {
                    Writer.WriteLine( Error );
                }
            }
        }

        public List<FileEntry> GetFileEntries()
        {
            return ( LangFileHandler.GetFileEntries() );
        }

        public bool FindLocFiles()
        {
            return( LangFileHandler.FindLocFiles() );
        }

		public void ReloadChangedFiles()
		{
			LangFileHandler.ReloadChangedFiles();
		}

		public void ResetCounts()
		{
			// Refresh all the error and warning lists
			Errors.Clear();
			Warnings.Clear();

			// Clear out any counts
			FilesCreated = 0;
			ObjectsCreated = 0;
			LocCreated = 0;
			NumOrphansRemoved = 0;
		}

        public void GenerateLocFiles( LanguageInfo DefaultLang )
        {
            // Generate all the files, objects and entries missing from the loc languages
            LangFileHandler.GenerateLocFiles( DefaultLang );

			Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... " + Errors.Count.ToString() + " errors found", Color.Black );
			Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... " + Warnings.Count.ToString() + " warnings found", Color.Black );
			
			// Remove keys that only exist in localised files
			Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... removing orphans for: " + LangID, Color.Green );
			LangFileHandler.RemoveOrphans();

            Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... " + FilesCreated.ToString() + " files created", Color.Black );
            Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... " + ObjectsCreated.ToString() + " objects created", Color.Black );
            Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... " + LocCreated.ToString() + " loc entries created", Color.Black );
            Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... " + NumOrphansRemoved.ToString() + " orphans removed", Color.Black );

            LocFilesGenerated = true;
        }

        public bool WriteLocFiles()
        {
            Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... creating loc data for language: " + LangID, Color.Black );
            return ( LangFileHandler.WriteLocFiles() );
        }

        public bool WriteDiffLocFiles( string Folder )
        {
            Main.Log( UnrealLoc.VerbosityLevel.Simple, " ... creating loc diff data for language: " + LangID, Color.Black );
            return ( LangFileHandler.WriteDiffLocFiles( Folder ) );
        }

        public bool ImportText( List<string> LangFiles )
        {
            foreach( string LangFile in LangFiles )
            {
                Main.Log( UnrealLoc.VerbosityLevel.Informative, " ... importing '" + LangFile + "'", Color.Black );
                LangFileHandler.ImportText( LangFile );
            }

            return ( true );
        }

		public string Validate( ref LocEntry LE, bool bOuterQuotesRequired )
        {
			return ( Main.Validate( this, ref LE, bOuterQuotesRequired ) );
        }
    }
}