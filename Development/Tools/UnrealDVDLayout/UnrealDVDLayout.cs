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
using System.Text;
using System.Xml;
using System.Xml.Serialization;
using System.Windows.Forms;

using UnrealControls;
using UnrealDVDLayout.Properties;

namespace UnrealDVDLayout
{
    public partial class UnrealDVDLayout : Form
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

        public enum ObjectType
        {
            Reserved,
            Volume,
            Directory,
            File,
        };

		public const long XboxSectorsPerLayer0 = 2097152;
		public const long XboxSectorsPerLayer1 = 1939866;
		public const long PS3SectorsPerLayer = 12219392;
		public const long BytesPerSector = 2048;

        public bool Ticking = false;
        public bool Interactive = true;
        public bool UpdateFiles = false;
        public bool UpdateGroups = false;
        public bool UpdateLayers = false;
        public DateTime LastUpdate = DateTime.UtcNow;
        public TOC TableOfContents = null;
		public string GP3TemplateName = "";
		public string IDPTemplateName = "";
		public psproject PS3DiscLayout = null;
		public InstallDesignerDataSet PCDiscLayout = null;
		public Xbox360GameDiscLayout XboxDiscLayout = null;
        public SettableOptions Options = null;

        delegate void DelegateAddLine( VerbosityLevel Verbosity, string Line, Color TextColor );

        public UnrealDVDLayout()
        {
            InitializeComponent();

			List<string> Games = UnrealControls.GameLocator.LocateGames();
			GameToolStripComboBox.Items.AddRange( Games.ToArray() );
        }

        public bool Init()
        {
            Ticking = true;

			string SettingsFile = Path.Combine( Application.StartupPath, "UnrealDVDLayout.Settings.xml" );
			Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( SettingsFile );

            GameToolStripComboBox.SelectedItem = Options.GameName;
			PlatformToolStripComboBox.SelectedItem = Options.Platform;
			ConfigurationCombo.SelectedIndex = Options.BuildConfiguration;
			Location = Options.LastLocation;
			Size = Options.LastSize;

			switch( Options.Platform.ToLower() )
			{
			case "xbox360":
				Layer0ListView.Enabled = true;
				Layer1ListView.Enabled = true;
				break;

			case "ps3":
				Layer0ListView.Enabled = true;
				Layer1ListView.Enabled = false;
				break;
			}

            return ( true );
        }

        public bool Destroy()
        {
			// Save out the updated rules file
			if( TableOfContents != null )
			{
				string ProjectFileName = Options.GameName + "Game\\Build\\" + Options.Platform + "\\UnrealDVDLayout.Project.xml";
				UnrealControls.XmlHandler.WriteXml<TOCGroups>( TableOfContents.Groups, ProjectFileName, "" );
			}

			string SettingsFile = Path.Combine( Application.StartupPath, "UnrealDVDLayout.Settings.xml" );
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, SettingsFile, "" );

            return ( true );
        }

		public void UtilityLog( object Sender, DataReceivedEventArgs e )
		{
			Log( VerbosityLevel.Informative, e.Data, Color.Black );
		}

		private void SpawnProcess( string Executable, string CWD, params string[] Parameters )
		{
			FileInfo Info = new FileInfo( Executable );
			if( !Info.Exists )
			{
				Error( "Executable does not exist! (" + Executable + ")" );
				return;
			}

			try
			{
				Process Utility = new Process();
				Utility.StartInfo.FileName = Executable;
				foreach( string Parameter in Parameters )
				{
					Utility.StartInfo.Arguments += Parameter + " ";
				}
				Utility.StartInfo.WorkingDirectory = CWD;

				Utility.StartInfo.CreateNoWindow = true;
				Utility.StartInfo.UseShellExecute = false;

				Utility.StartInfo.RedirectStandardOutput = true;
				Utility.StartInfo.RedirectStandardError = true;
				Utility.OutputDataReceived += new DataReceivedEventHandler( UtilityLog );
				Utility.ErrorDataReceived += new DataReceivedEventHandler( UtilityLog );

				Log( VerbosityLevel.Informative, "Spawning: " + Utility.StartInfo.FileName + " " + Utility.StartInfo.Arguments + "(CWD: " + Utility.StartInfo.WorkingDirectory + ")", Color.Green );

				if( !Utility.Start() )
				{
					Error( "Failed to launch process! (" + Executable + " " + Utility.StartInfo.Arguments + ")" );
					return;
				}

				Utility.BeginOutputReadLine();
				Utility.BeginErrorReadLine();
				Utility.EnableRaisingEvents = true;

				while( !Utility.HasExited )
				{
					Application.DoEvents();
					Utility.WaitForExit( 5 );
				}

				Log( VerbosityLevel.Informative, "Utility finished with exit code: " + Utility.ExitCode.ToString(), Color.Green );
			}
			catch( Exception Ex )
			{
				Error( "Spawning process: " + Ex.Message );
			}
		}

        private void GameToolStripComboBox_Changed( object sender, EventArgs e )
        {
            Options.GameName = ( string )GameToolStripComboBox.SelectedItem;
        }

		private void PlatformToolStripComboBox_Changed( object sender, EventArgs e )
		{
			Options.Platform = ( string )PlatformToolStripComboBox.SelectedItem;
		}

        private void QuitMenuItem_Click( object sender, EventArgs e )
        {
            Ticking = false;
        }

        private void OptionsMenuItem_Click( object sender, EventArgs e )
        {
            OptionsDialog DisplayOptions = new OptionsDialog( this, Options );
            DisplayOptions.ShowDialog();
        }

        private void UnrealDVDLayout_FormClosed( object sender, FormClosedEventArgs e )
        {
            Ticking = false;
        }

        private void GroupListView_KeyPress( object sender, KeyEventArgs e )
        {
            if( e.KeyCode == Keys.Delete )
            {
                foreach( ListViewItem GroupName in GroupListView.SelectedItems )
                {
                    TableOfContents.Groups.TOCGroupLayer0.Remove( GroupName.Text );
                    TableOfContents.Groups.TOCGroupLayer1.Remove( GroupName.Text );

                    TableOfContents.RemoveGroup( GroupName.Text );
                }

                PopulateListBoxes( true, true, true );
            }
        }

        private void Layer0ListView_KeyPress( object sender, KeyEventArgs e )
        {
            if( e.KeyCode == Keys.Delete )
            {
                foreach( ListViewItem GroupName in Layer0ListView.SelectedItems )
                {
                    TOCGroup Group = TableOfContents.GetGroup( GroupName.Text );
                    TableOfContents.AddGroupToLayer( Group, -1 ); 
                    
                    TableOfContents.Groups.TOCGroupLayer0.Remove( GroupName.Text );
                }

                PopulateListBoxes( false, true, true );
            }
        }

        private void Layer1ListView_KeyPress( object sender, KeyEventArgs e )
        {
            if( e.KeyCode == Keys.Delete )
            {
                foreach( ListViewItem GroupName in Layer1ListView.SelectedItems )
                {
                    TOCGroup Group = TableOfContents.GetGroup( GroupName.Text );
                    TableOfContents.AddGroupToLayer( Group, -1 );

                    TableOfContents.Groups.TOCGroupLayer1.Remove( GroupName.Text );
                }

                PopulateListBoxes( false, true, true );
            }
        }

        private void GroupListView_DoubleClick( object sender, EventArgs e )
        {
            GroupProperties GroupProp = new GroupProperties();

            TOCGroup Group = TableOfContents.GetGroup( GroupListView.FocusedItem.Text );
            GroupProp.Init( TableOfContents, Group );

            if( GroupProp.ShowDialog() == DialogResult.OK )
            {
                GroupProp.ApplyChanges();
            }

            PopulateListBoxes( true, true, true );
        }

        private void AddToGroupItem_Click( object sender, EventArgs e )
        {
            ToolStripItem Item = ( ToolStripItem )sender;
            TOCGroup Group = TableOfContents.GetGroup( Item.Text );

            foreach( ListViewItem Selected in FileListView.SelectedItems )
            {
                TableOfContents.AddFileToGroup( Group, Selected.Text );
            }

            PopulateListBoxes( true, true, true );
        }

        private void FileListBoxMenuStrip_Opening( object sender, CancelEventArgs e )
        {
            List<string> GroupNames = TableOfContents.GetGroupNames();

            AddToGroupToolStripMenuItem.DropDownItems.Clear();
            foreach( string GroupName in GroupNames )
            {
                ToolStripItem Item = AddToGroupToolStripMenuItem.DropDownItems.Add( GroupName );
                Item.Click += new System.EventHandler( AddToGroupItem_Click );
            }
        }

        private void GroupMoveUpButton_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in GroupListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.MoveGroupInGroups( Group, 1 );
            }

            PopulateListBoxes( false, true, true );
        }

        private void GroupMoveDownButton_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in GroupListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.MoveGroupInGroups( Group, -1 );
            }

            PopulateListBoxes( false, true, true );
        }

        private void AddToLayer0MenuItem_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in GroupListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.AddGroupToLayer( Group, 0 );
            }

            PopulateListBoxes( false, true, true );
        }

        private void AddToLayer1MenuItem_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in GroupListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.AddGroupToLayer( Group, 1 );
            }

            PopulateListBoxes( false, true, true );
        }

        private void RemoveFromDiscMenuItem_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in GroupListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.AddGroupToLayer( Group, -1 );
            }

            PopulateListBoxes( false, true, true );
        }

        private void Layer0MoveUpButton_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in Layer0ListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.MoveGroupInLayer( Group, 0, 1 );
            }

            PopulateListBoxes( false, false, true );
        }

        private void Layer0MoveDownButton_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in Layer0ListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.MoveGroupInLayer( Group, 0, -1 );
            }

            PopulateListBoxes( false, false, true );
        }

        private void Layer1MoveUpButton_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in Layer1ListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.MoveGroupInLayer( Group, 1, 1 );
            }

            PopulateListBoxes( false, false, true );
        }

        private void Layer1MoveDownButton_Click( object sender, EventArgs e )
        {
            foreach( ListViewItem Selected in Layer1ListView.SelectedItems )
            {
                TOCGroup Group = TableOfContents.GetGroup( Selected.Text );
                TableOfContents.MoveGroupInLayer( Group, 1, -1 );
            }

            PopulateListBoxes( false, false, true );
        }

        private bool ImportTOCFile( string[] FileNames )
        {
			TableOfContents = null;

            if( FileNames.Length == 0 )
            {
                Log( VerbosityLevel.Informative, "No files selected!", Color.Red );
                return( false );
            }

            foreach( string FileName in FileNames )
            {
                if( !FileName.ToLower().Contains( "game" ) )
                {
                    Error( "Need a game name in the path (Game) for '" + FileName + "'" );
                    return( false );
                }
            }

            Log( VerbosityLevel.Informative, "Attempting to read " + FileNames.Length.ToString() + " TOC files.", Color.Blue );

			// Extract the platform name
			string TOCFileName = Path.GetFileNameWithoutExtension( FileNames[0] );
			string Platform = "Unknown";
			if( TOCFileName.ToLower().StartsWith( "xbox360" ) )
			{
				Platform = "Xbox360";
			}
			else if( TOCFileName.ToLower().StartsWith( "ps3" ) )
			{
				Platform = "PS3";
			}
			else if( TOCFileName.ToLower().StartsWith( "pcconsole" ) )
			{
				Platform = "PCConsole";
			}

			Options.Platform = Platform;
			PlatformToolStripComboBox.SelectedItem = Options.Platform;

			// Strip off the TOCName
            string TOCName = Path.GetDirectoryName( FileNames[0] );

            // Strip of the game name
            int LastSlash = TOCName.LastIndexOf( '\\' );
            string GameName = TOCName.Substring( LastSlash + 1 );
            string TOCFolder = "";
            if( LastSlash >= 0 )
            {
                TOCFolder = TOCName.Substring( 0, LastSlash );
            }

            Options.GameName = GameName.Substring( 0, GameName.Length - "Game".Length );
            GameToolStripComboBox.SelectedItem = Options.GameName;

			// Read in all the TOCs
            TableOfContents = new TOC( this, TOCFolder );
			for( int FileIndex = 0; FileIndex < FileNames.Length; FileIndex++ )
			{
				string FileName = FileNames[FileIndex];
				string RelativeName = FileName.Substring( Environment.CurrentDirectory.Length );
				TableOfContents.Read( FileName, RelativeName, Options.FingerprintName, FileIndex < Options.MaxFullyLocalisedLanguages );
			}

            // Get the relevant system files
			switch( Platform.ToLower() )
			{
			case "xbox360":
				TableOfContents.GetXex( GameName );
				TableOfContents.GetNXEArt( GameName, FileNames[0] );
				TableOfContents.GetAvatarAwards( GameName, FileNames[0] );
				break;

			case "ps3":
				TableOfContents.GetEboot( GameName );
				break;

			case "pcconsole":
				TableOfContents.CopySupportFiles( GameName );
				break;
			}

            // Create the folder objects
            TableOfContents.CreateFolders();

            // Check for duplicate files
            TableOfContents.CheckDuplicates();

            // Load in the project file - we have the game name
            string ProjectFileName = GameName + "\\Build\\" + Platform + "\\UnrealDVDLayout.Project.xml";
            TableOfContents.Groups = UnrealControls.XmlHandler.ReadXml<TOCGroups>( ProjectFileName );

            // Apply the correct sorts to all the groups
            TableOfContents.UpdateTOCFromGroups();
            TableOfContents.FinishSetup();

			Text = "Unreal DVD Layout Tool : " + TableOfContents.GetSummary();

            // Populate the UI if we have one
            if( Interactive )
            {
                PopulateListBoxes( true, true, true );
			}

			return ( true );
        }

		private void SetGP3TemplateMenu_Click( object sender, EventArgs e )
		{
			GenericOpenFileDialog.Title = "Select GP3 template file...";
			GenericOpenFileDialog.Filter = "GP3 Templates Files (*.gp3_template)|*.gp3_template|All Files (*.*)|*.*";
			GenericOpenFileDialog.InitialDirectory = Environment.CurrentDirectory;
			if( GenericOpenFileDialog.ShowDialog() == DialogResult.OK )
			{
				GP3TemplateName = GenericOpenFileDialog.FileName;
			}
		}

		private void SetIDPTemplateMenu_Click( object sender, EventArgs e )
		{
			GenericOpenFileDialog.Title = "Select IDP template file...";
			GenericOpenFileDialog.Filter = "InstallDesignerProject Templates Files (*.idp_template)|*.idp_template|All Files (*.*)|*.*";
			GenericOpenFileDialog.InitialDirectory = Environment.CurrentDirectory;
			if( GenericOpenFileDialog.ShowDialog() == DialogResult.OK )
			{
				IDPTemplateName = GenericOpenFileDialog.FileName;
			}
		}

        private void ImportTOCMenu_Click( object sender, EventArgs e )
        {
            GenericOpenFileDialog.Title = "Select TOC file(s) to import...";
            GenericOpenFileDialog.Filter = "ToC Files (*.txt)|*.txt";
            GenericOpenFileDialog.InitialDirectory = Environment.CurrentDirectory;
            if( GenericOpenFileDialog.ShowDialog() == DialogResult.OK )
            {
				if( ImportTOCFile( GenericOpenFileDialog.FileNames ) )
				{
					// Create a sample layout 
					switch( Options.Platform.ToLower() )
					{
					case "xbox360":
						CreateXGDFile( "Layout" );
						break;

					case "ps3":
						CreateGP3File( "Layout" );
						break;

					case "pcconsole":
						CreateInstallDesignerProject( "Layout" );
						break;
					}
				}
            }
        }

        private void SaveLayoutMenu_Click( object sender, EventArgs e )
        {
            if( TableOfContents == null )
            {
                Error( "No TOC loaded to be able to save ...." );
            }
            else
            {
				string Platform = ( string )PlatformToolStripComboBox.SelectedItem;

				// Save out the updated rules file
				if( TableOfContents != null )
				{
					string ProjectFileName = Options.GameName + "Game\\Build\\" + Platform + "\\UnrealDVDLayout.Project.xml";
					UnrealControls.XmlHandler.WriteXml<TOCGroups>( TableOfContents.Groups, ProjectFileName, "" );
				}

				// Save the platform specific layout file
				switch( Platform.ToLower() )
				{
				case "xbox360":
					SaveXGD();
					break;

				case "ps3":
					SaveGP3();
					break;

				case "pcconsole":
					SaveIDP();
					break;
				}
            }
        }

		private void SaveISOClick( object sender, EventArgs e )
		{
			if( TableOfContents == null )
			{
				Error( "No TOC loaded to be able to save ...." );
			}
			else
			{
				string Platform = ( string )PlatformToolStripComboBox.SelectedItem;
				switch( Platform.ToLower() )
				{
				case "xbox360":
					SaveXGDISO();
					break;

				case "ps3":
					SaveGP3ISO();
					break;

				case "pcconsole":
					SaveIDPISO();
					break;
				}
			}
		}

		private void EmulateClick( object sender, EventArgs e )
		{
			if( TableOfContents == null )
			{
				Error( "No TOC loaded to be able to emulate ...." );
			}
			else
			{
				string Platform = ( string )PlatformToolStripComboBox.SelectedItem;
				switch( Platform.ToLower() )
				{
				case "xbox360":
					LaunchEmulation();
					break;

				case "ps3":
				case "pcconsole":
				default:
					break;
				}
			}
		}

        private TOCGroup PopulateLayer( ListView LayerListView, string GroupName )
        {
            TOCGroup Group = TableOfContents.GetGroup( GroupName );

            ListViewItem Item = LayerListView.Items.Add( GroupName );
            Item.Selected = Group.LayerSelected;
            Group.LayerSelected = false;

            float SizeMB = Group.Size / ( 1024.0f * 1024.0f );
            Item.SubItems.Add( SizeMB.ToString( "f2" ) );

            return ( Group );
        }

        public void PopulateListBoxes( bool FilesDirty, bool GroupsDirty, bool LayersDirty )
        {
            UpdateFiles = FilesDirty;
            UpdateGroups = GroupsDirty;
            UpdateLayers = LayersDirty;

            TimeSpan Delta = TimeSpan.FromMilliseconds(300);
            LastUpdate = DateTime.UtcNow + Delta;
        }

        public void DoPopulateListBoxes()
        {
            if( !UpdateFiles && !UpdateGroups && !UpdateLayers )
            {
                return;
            }

			if( DateTime.UtcNow < LastUpdate )
			{
				return;
			}

            FileListView.SuspendLayout();
            GroupListView.SuspendLayout();
            Layer0ListView.SuspendLayout();
            Layer1ListView.SuspendLayout();

            TableOfContents.UpdateTOCFromGroups();

            if( UpdateFiles )
            {
                FileListView.Items.Clear();

                // Add in each entry
                foreach( TOCInfo TOCEntry in TableOfContents.TOCFileEntries )
                {
                    if( TOCEntry.Group == null )
                    {
                        FileListView.Items.Add( TOCEntry.Path );
                    }
                }
            }

            // Add in each group
            if( TableOfContents.Groups != null )
            {
                if( UpdateGroups )
                {
                    GroupListView.Items.Clear();

                    foreach( TOCGroup Group in TableOfContents.Groups.TOCGroupEntries )
                    {
                        // Populate layer list view
                        ListViewItem Item = GroupListView.Items.Add( Group.GroupName );
                        Item.Selected = Group.GroupSelected;
                        Group.GroupSelected = false;

                        switch( Group.Layer )
                        {
                            case -1:
                                Item.ForeColor = Color.Red;
                                break;
                            case 0:
                                Item.ForeColor = Color.Blue;
                                break;
                            case 1:
                                Item.ForeColor = Color.Green;
                                break;
                        }

                        float SizeMB = Group.Size / ( 1024.0f * 1024.0f );
                        Item.SubItems.Add( SizeMB.ToString( "f2" ) );
                    }
                }

                if( UpdateLayers )
                {
					switch( Options.Platform.ToLower() )
					{
					case "xbox360":
						long Layer0Free = XboxSectorsPerLayer0;
						long Layer1Free = XboxSectorsPerLayer1;

						Layer0ListView.Items.Clear();

						foreach( string GroupName in TableOfContents.Groups.TOCGroupLayer0 )
						{
							TOCGroup Group = PopulateLayer( Layer0ListView, GroupName );
							Layer0Free -= Group.SectorSize;
						}

						float Layer0Percent = ( float )( XboxSectorsPerLayer0 - Layer0Free ) * 100.0f / XboxSectorsPerLayer0;
						Layer0Label.Text = "Layer 0 : " + Layer0Percent.ToString( "f1" ) + "% full (" + Layer0Free.ToString() + " sectors free)";

						Layer1ListView.Items.Clear();

						foreach( string GroupName in TableOfContents.Groups.TOCGroupLayer1 )
						{
							TOCGroup Group = PopulateLayer( Layer1ListView, GroupName );
							Layer1Free -= Group.SectorSize;
						}

						float Layer1Percent = ( float )( XboxSectorsPerLayer1 - Layer1Free ) * 100.0f / XboxSectorsPerLayer1;
						Layer1Label.Text = "Layer 1 : " + Layer1Percent.ToString( "f1" ) + "% full (" + Layer1Free.ToString() + " sectors free)";

						Layer1ListView.Enabled = true;
						break;

					case "ps3":
						long LayerFree = PS3SectorsPerLayer;

						Layer0ListView.Items.Clear();

						foreach( string GroupName in TableOfContents.Groups.TOCGroupLayer0 )
						{
							TOCGroup Group = PopulateLayer( Layer0ListView, GroupName );
							LayerFree -= Group.SectorSize;
						}

						float LayerPercent = ( float )( PS3SectorsPerLayer - LayerFree ) * 100.0f / PS3SectorsPerLayer;
						Layer0Label.Text = "Layer 0 : " + LayerPercent.ToString( "f1" ) + "% full (" + LayerFree.ToString() + " sectors free)";

						Layer1ListView.Enabled = false;
						Layer1ListView.Items.Clear();
						break;
					}
                }
            }

            UpdateFiles = false;
            UpdateGroups = false;
            UpdateLayers = false;

            FileListView.ResumeLayout();
            GroupListView.ResumeLayout();
            Layer0ListView.ResumeLayout();
            Layer1ListView.ResumeLayout();
        }

        // Save each original TOC with incorporated sector offsets
        public void SaveTOCs()
        {
            Dictionary<string, StreamWriter> TOCs = new Dictionary<string, StreamWriter>();
            List<TOCInfo> TOCEntries = new List<TOCInfo>();

            // Find all the TOC files and open a stream for them.
            foreach( TOCInfo TOCEntry in TableOfContents.TOCFileEntries )
            {
                if( TOCEntry.Layer != -1 && TOCEntry.OwnerTOC != null )
                {
                    if( !TOCs.ContainsKey( TOCEntry.OwnerTOC ) )
                    {
                        StreamWriter TOC = new StreamWriter( TOCEntry.OwnerTOC );
                        TOCs.Add( TOCEntry.OwnerTOC, TOC );
                    }
                }
            }

            // Iterate over the TOCs and write the entries
            foreach( TOCInfo TOCEntry in TableOfContents.TOCFileEntries )
            {
                if( TOCEntry.Layer != -1 && TOCEntry.OwnerTOC != null )
                {
                    if( TOCEntry.IsTOC )
                    {
                        // Remember these so we can refresh the size
                        TOCEntries.Add( TOCEntry );
                    }
                    else
                    {
                        string Line = TOCEntry.Size.ToString() + " " + TOCEntry.DecompressedSize.ToString() + " ";
                        Line += ".." + TOCEntry.Path + " " + TOCEntry.CRCString;

                        TOCs[TOCEntry.OwnerTOC].WriteLine( Line );
                    }
                }
            }

			// Update to the new sizes
			foreach( TOCInfo TOCEntry in TOCEntries )
			{
				TOCEntry.DeriveData( TOCEntry.OwnerTOC );

				string Line = "0 0 .." + TOCEntry.Path + " 0";
				TOCs[TOCEntry.OwnerTOC].WriteLine( Line );
			}

            // Close all the streams
            foreach( StreamWriter TOC in TOCs.Values )
            {
                TOC.Close();
            }
        }

        public void HandleCommandLine( string[] Arguments )
        {
            // Let the program know we're running from the command line
            Interactive = false;

			// Don't create an ISO/XSF by default
			bool bCreateImage = false;
			string ImageName = "";

            if( Arguments.Length < 4 )
            {
				Error( "Not enough parameters; usage 'UnrealDVDLayout <Game> <Platform> <LayoutName> <Language> [Language] [Language] [-image ImageName] [-KeyLocation PFXPath] [-KeyPassword PFXPassword]'" );
                return;
            }

            // Grab the common data
            string Game = Arguments[0] + "Game";
            string Platform = Arguments[1];
			string LayoutFileName = Arguments[2];
			string LayoutID = Path.GetFileNameWithoutExtension( LayoutFileName );

            Log( VerbosityLevel.Informative, "Creating layout for " + Game + " " + Platform, Color.Blue );

            // Work out the list of TOCs to load
            string TOCFileRoot = Environment.CurrentDirectory + "\\" + Game + "\\" + Platform + "TOC";
            List<string> TOCFileNames = new List<string>();

            for( int Index = 3; Index < Arguments.Length; Index++ )
            {
				if( Arguments[Index].StartsWith( "-" ) )
				{
					if( Index + 1 < Arguments.Length )
					{
						if( Arguments[Index].ToUpper() == "-IMAGE" )
						{
							Index++;
							ImageName = Arguments[Index];
							bCreateImage = true;
						}
						else if( Arguments[Index].ToUpper() == "-KEYLOCATION" )
						{
							Index++;
							Options.KeyLocation = Arguments[Index];
						}
						else if( Arguments[Index].ToUpper() == "-KEYPASSWORD" )
						{
							Index++;
							Options.KeyPassword = Arguments[Index];
						}
						else if( Arguments[Index].ToUpper() == "-MAXFULL" )
						{
							Options.MaxFullyLocalisedLanguages = -1;
							Index++;
							try
							{
								Options.MaxFullyLocalisedLanguages = Int32.Parse( Arguments[Index] );
							}
							catch
							{
							}
						}
					}
				}
				else
				{
					string Language = Arguments[Index].ToUpper();
					string TOCFileName = "";
					if( Language == "INT" )
					{
						TOCFileName = TOCFileRoot + ".txt";
					}
					else
					{
						TOCFileName = TOCFileRoot + "_" + Language + ".txt";
					}

					FileInfo Info = new FileInfo( TOCFileName );
					if( Info.Exists )
					{
						TOCFileNames.Add( TOCFileName );
					}
					else
					{
						Warning( "Could not find TOC for language '" + Language + "'" );
					}
				}
            }

			if( Options.MaxFullyLocalisedLanguages < 0 )
			{
				Error( "Invalid value for -MaxFull (the maximum number of fully localised languages) ...." );
				return;
			}

            // Read in and setup a DVD layout
            if( !ImportTOCFile( TOCFileNames.ToArray() ) )
			{
				Error( "No TableOfContents could be processed ...." );
				return;
			}

			switch( Platform.ToLower() )
			{
			case "xbox360":
				// Create a transient XGD to set all the layers in the TOC entries
				CreateXGDFile( null );

				// Save the updated TOCs (can be done now the layers have been set in the previous call)
				SaveTOCs();
	
				// Create a new XGD with the updated TOC sizes
				CreateXGDFile( LayoutID );

				// Create the XGD name
				string XGDFileName = Path.ChangeExtension( LayoutFileName, ".XGD" );

				// Write out the XGD file
				UnrealControls.XmlHandler.WriteXml<Xbox360GameDiscLayout>( XboxDiscLayout, XGDFileName, "http://www.w3.org/2001/XMLSchema-instance" );

				// Write out the ISO/XSF if requested
				if( bCreateImage )
				{
					SaveXGDImage( XGDFileName, ImageName );
				}
				break;

			case "ps3":
				// Create a transient XGD to set all the layers in the TOC entries
				CreateGP3File( null );

				// Save the updated TOCs (can be done now the layers have been set in the previous call)
				SaveTOCs();

				// Create the GP3_template name
				GP3TemplateName = Path.ChangeExtension( LayoutFileName, ".GP3_template" );

				// Create a new GP3 with the updated TOC sizes
				CreateGP3File( LayoutID );

				// Write out the GP3 file
				string GP3FileName = Path.ChangeExtension( GP3TemplateName, ".gp3" );
				UnrealControls.XmlHandler.WriteXml<psproject>( PS3DiscLayout, GP3FileName, "" );

				// Write out the ISO if requested
				if( bCreateImage )
				{
					SaveGP3Image( GP3FileName, ImageName );
				}
				break;

			case "pcconsole":
				// Create the GP3_template name
				IDPTemplateName = Path.ChangeExtension( LayoutFileName, ".IDP_template" );

				// Create a new GP3 with the updated TOC sizes
				CreateInstallDesignerProject( LayoutID );

				// Write out the GP3 file
				string IDPFileName = Path.GetFullPath( Path.ChangeExtension( IDPTemplateName, ".InstallDesignerProject" ) );
				UnrealControls.XmlHandler.WriteXml<InstallDesignerDataSet>( PCDiscLayout, IDPFileName, "urn:schemas-microsoft-com:InstallDesignerProject.v1" );

				// Write out the ISO if requested
				if( bCreateImage )
				{
					SaveInstallerFiles( IDPFileName, ImageName );
				}
				break;
			}
        }

        private bool HandleRegExpDialog( GroupRegExp Group )
        {
            if( Group.ShowDialog() == DialogResult.OK )
            {
                string Expression = Group.GetExpression();
                string GroupName = Group.GetGroupName();

                if( TableOfContents.CollateFiles( Expression, GroupName ) )
                {
                    PopulateListBoxes( true, true, true );
                    return ( true );
                }
                else
                {
                    return ( false );
                }
            }

            return ( true );
        }

        private void MakeFromRegExpMenuItem_Click( object sender, EventArgs e )
        {
            GroupRegExp NewGroup = new GroupRegExp();

			if( FileListView.FocusedItem != null )
			{
				NewGroup.SetExpression( FileListView.FocusedItem.Text.Replace( "\\", "\\\\" ) );
			}
            NewGroup.SetGroupNames( TableOfContents.GetGroupNames() );

            while( !HandleRegExpDialog( NewGroup ) )
            {
                Log( VerbosityLevel.Informative, "Invalid regular expression; please try again", Color.Blue );
            }
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
            string FullLine = Now.ToLongTimeString() + ": " + Line;

            if( Interactive )
            {
                // Write to the log window
                MainLogWindow.Focus();
                MainLogWindow.SelectionLength = 0;

                // Only set the color if it is different than the foreground colour
                if( MainLogWindow.SelectionColor != TextColour )
                {
                    MainLogWindow.SelectionColor = TextColour;
                }

                MainLogWindow.AppendText( FullLine + "\r\n" );
            }
            else
            {
                // Write to the console
                Console.WriteLine( FullLine );
            }
        }

        public void Log( VerbosityLevel Verbosity, Array Lines, Color TextColour )
        {
            foreach( string Line in Lines )
            {
                Log( Verbosity, Line, TextColour );
            }
        }

        public void Error( string Line )
        {
            Log( VerbosityLevel.Critical, "Error: " + Line, Color.Red );
        }

        public void Warning( string Line )
        {
            Log( VerbosityLevel.Simple, "Warning: " + Line, Color.Orange );
        }

		private void UnrealDVDLayout_FormClosing( object sender, FormClosingEventArgs e )
		{
			// Make sure to save any values that might be invalid (due to full screen, etc)
			if( WindowState == FormWindowState.Normal )
			{
				Options.LastLocation = Location;
				Options.LastSize = Size;
			}
			else
			{
				Options.LastLocation = RestoreBounds.Location;
				Options.LastSize = RestoreBounds.Size;
			}
		}

		private void ConfigurationCombo_SelectedIndexChanged( object sender, EventArgs e )
		{
			Options.BuildConfiguration = ConfigurationCombo.SelectedIndex;
			if( TableOfContents != null )
			{
				TableOfContents.GetXex( Options.GameName + "Game" );
				PopulateListBoxes( false, true, true );
			}
		}
	}

    public class SettableOptions
    {
        [CategoryAttribute( "Settings" )]
        [DescriptionAttribute( "The amount of text spewed to the window." )]
		[XmlElement]
		public UnrealDVDLayout.VerbosityLevel Verbosity { get; set; }

        [CategoryAttribute( "Settings" )]
        [DescriptionAttribute( "The game we are creating a disc layout for." )]
		[XmlElement]
		public string GameName { get; set; }

		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "The platform we are creating a disc layout for." )]
		[XmlElement]
		public string Platform { get; set; }

		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "The maximum number of fully localised languages allowed on the disk." )]
		[XmlElement]
		public int MaxFullyLocalisedLanguages { get; set; }

		[CategoryAttribute( "Settings" )]
		[DescriptionAttribute( "The name of the fingerprint file." )]
		[XmlElement]
		public string FingerprintName { get; set; }

		[CategoryAttribute( "Security" )]
		[DescriptionAttribute( "The name of the pfx file." )]
		[XmlElement]
		public string KeyLocation { get; set; }

		[CategoryAttribute( "Security" )]
		[DescriptionAttribute( "The password for the pfx." )]
		[XmlElement]
		public string KeyPassword { get; set; }

		[XmlElement]
		public int BuildConfiguration;
		[XmlElement]
		public Point LastLocation;
		[XmlElement]
		public Size LastSize;

		public SettableOptions()
		{
			Verbosity = UnrealDVDLayout.VerbosityLevel.Informative;
			GameName = "Example";
			Platform = "Xbox360";
			MaxFullyLocalisedLanguages = 1;
			KeyLocation = "";
			KeyPassword = "";
			FingerprintName = "GlobalContentGuids.bin";
			BuildConfiguration = 0;
			LastLocation = new Point( 0, 0 );
			LastSize = new Size( 1374, 1200 );
		}
	}
}