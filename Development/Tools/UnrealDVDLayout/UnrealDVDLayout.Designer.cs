namespace UnrealDVDLayout
{
    partial class UnrealDVDLayout
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose( bool disposing )
        {
            if( disposing && ( components != null ) )
            {
                components.Dispose();
            }
            base.Dispose( disposing );
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager( typeof( UnrealDVDLayout ) );
			this.MainMenu = new System.Windows.Forms.MenuStrip();
			this.FileMainMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ImportTOCsFileMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.setIDPTemplateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.setGP3TemplateToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.SaveXGDMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.QuitFileMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolsMainMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OptionsToolsMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.MainToolStrip = new System.Windows.Forms.ToolStrip();
			this.ConfigurationCombo = new System.Windows.Forms.ToolStripComboBox();
			this.GameToolStripComboBox = new System.Windows.Forms.ToolStripComboBox();
			this.PlatformToolStripComboBox = new System.Windows.Forms.ToolStripComboBox();
			this.ImportTOCsToolStripButton = new System.Windows.Forms.ToolStripButton();
			this.SaveLayoutToolStripButton = new System.Windows.Forms.ToolStripButton();
			this.SaveISOToolStripButton = new System.Windows.Forms.ToolStripButton();
			this.toolStripButton1 = new System.Windows.Forms.ToolStripButton();
			this.GenericOpenFileDialog = new System.Windows.Forms.OpenFileDialog();
			this.MainLogWindow = new System.Windows.Forms.RichTextBox();
			this.GenericSaveFileDialog = new System.Windows.Forms.SaveFileDialog();
			this.FileListView = new System.Windows.Forms.ListView();
			this.FileListViewPathHeader = new System.Windows.Forms.ColumnHeader();
			this.FileListViewMenuStrip = new System.Windows.Forms.ContextMenuStrip( this.components );
			this.AddToGroupToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.MakeFromRegExpMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.GroupListView = new System.Windows.Forms.ListView();
			this.ListViewNameHeader = new System.Windows.Forms.ColumnHeader();
			this.ListViewSizeHeader = new System.Windows.Forms.ColumnHeader();
			this.GroupListViewMenuStrip = new System.Windows.Forms.ContextMenuStrip( this.components );
			this.AddToLayer0MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.AddToLayer1MenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.removeFromDiscToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.Layer0ListView = new System.Windows.Forms.ListView();
			this.Layer0GroupHeader = new System.Windows.Forms.ColumnHeader();
			this.Layer0SizeHeader = new System.Windows.Forms.ColumnHeader();
			this.Layer0Label = new System.Windows.Forms.Label();
			this.GroupMoveUpButton = new System.Windows.Forms.Button();
			this.GroupMoveDownButton = new System.Windows.Forms.Button();
			this.DiscLayoutBox = new System.Windows.Forms.GroupBox();
			this.SlowLabel = new System.Windows.Forms.Label();
			this.FastLabel = new System.Windows.Forms.Label();
			this.Layer0MoveDownButton = new System.Windows.Forms.Button();
			this.Layer0MoveUpButton = new System.Windows.Forms.Button();
			this.Layer1Label = new System.Windows.Forms.Label();
			this.Layer1MoveUpButton = new System.Windows.Forms.Button();
			this.Layer1MoveDownButton = new System.Windows.Forms.Button();
			this.Layer1ListView = new System.Windows.Forms.ListView();
			this.Layer1GroupHeader = new System.Windows.Forms.ColumnHeader();
			this.Layer1SizeHeader = new System.Windows.Forms.ColumnHeader();
			this.FilesLabel = new System.Windows.Forms.Label();
			this.GroupsLabel = new System.Windows.Forms.Label();
			this.GenericFolderBrowserDialog = new System.Windows.Forms.FolderBrowserDialog();
			this.MainMenu.SuspendLayout();
			this.MainToolStrip.SuspendLayout();
			this.FileListViewMenuStrip.SuspendLayout();
			this.GroupListViewMenuStrip.SuspendLayout();
			this.DiscLayoutBox.SuspendLayout();
			this.SuspendLayout();
			// 
			// MainMenu
			// 
			this.MainMenu.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.FileMainMenuItem,
            this.ToolsMainMenuItem} );
			this.MainMenu.Location = new System.Drawing.Point( 0, 0 );
			this.MainMenu.Name = "MainMenu";
			this.MainMenu.Size = new System.Drawing.Size( 1366, 24 );
			this.MainMenu.TabIndex = 0;
			this.MainMenu.Text = "menuStrip1";
			// 
			// FileMainMenuItem
			// 
			this.FileMainMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.ImportTOCsFileMenuItem,
            this.setIDPTemplateToolStripMenuItem,
            this.setGP3TemplateToolStripMenuItem,
            this.SaveXGDMenuItem,
            this.toolStripSeparator1,
            this.QuitFileMenuItem} );
			this.FileMainMenuItem.Name = "FileMainMenuItem";
			this.FileMainMenuItem.Size = new System.Drawing.Size( 35, 20 );
			this.FileMainMenuItem.Text = "File";
			// 
			// ImportTOCsFileMenuItem
			// 
			this.ImportTOCsFileMenuItem.Name = "ImportTOCsFileMenuItem";
			this.ImportTOCsFileMenuItem.Size = new System.Drawing.Size( 159, 22 );
			this.ImportTOCsFileMenuItem.Text = "Import TOCs";
			this.ImportTOCsFileMenuItem.Click += new System.EventHandler( this.ImportTOCMenu_Click );
			// 
			// setIDPTemplateToolStripMenuItem
			// 
			this.setIDPTemplateToolStripMenuItem.Name = "setIDPTemplateToolStripMenuItem";
			this.setIDPTemplateToolStripMenuItem.Size = new System.Drawing.Size( 159, 22 );
			this.setIDPTemplateToolStripMenuItem.Text = "Set IDP Template";
			this.setIDPTemplateToolStripMenuItem.Click += new System.EventHandler( this.SetIDPTemplateMenu_Click );
			// 
			// setGP3TemplateToolStripMenuItem
			// 
			this.setGP3TemplateToolStripMenuItem.Name = "setGP3TemplateToolStripMenuItem";
			this.setGP3TemplateToolStripMenuItem.Size = new System.Drawing.Size( 159, 22 );
			this.setGP3TemplateToolStripMenuItem.Text = "Set GP3 Template";
			this.setGP3TemplateToolStripMenuItem.Click += new System.EventHandler( this.SetGP3TemplateMenu_Click );
			// 
			// SaveXGDMenuItem
			// 
			this.SaveXGDMenuItem.Name = "SaveXGDMenuItem";
			this.SaveXGDMenuItem.Size = new System.Drawing.Size( 159, 22 );
			this.SaveXGDMenuItem.Text = "Save Layout";
			this.SaveXGDMenuItem.Click += new System.EventHandler( this.SaveLayoutMenu_Click );
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size( 156, 6 );
			// 
			// QuitFileMenuItem
			// 
			this.QuitFileMenuItem.Name = "QuitFileMenuItem";
			this.QuitFileMenuItem.Size = new System.Drawing.Size( 159, 22 );
			this.QuitFileMenuItem.Text = "Quit";
			this.QuitFileMenuItem.Click += new System.EventHandler( this.QuitMenuItem_Click );
			// 
			// ToolsMainMenuItem
			// 
			this.ToolsMainMenuItem.DropDownItems.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.OptionsToolsMenuItem} );
			this.ToolsMainMenuItem.Name = "ToolsMainMenuItem";
			this.ToolsMainMenuItem.Size = new System.Drawing.Size( 44, 20 );
			this.ToolsMainMenuItem.Text = "Tools";
			// 
			// OptionsToolsMenuItem
			// 
			this.OptionsToolsMenuItem.Name = "OptionsToolsMenuItem";
			this.OptionsToolsMenuItem.Size = new System.Drawing.Size( 111, 22 );
			this.OptionsToolsMenuItem.Text = "Options";
			this.OptionsToolsMenuItem.Click += new System.EventHandler( this.OptionsMenuItem_Click );
			// 
			// MainToolStrip
			// 
			this.MainToolStrip.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.ConfigurationCombo,
            this.GameToolStripComboBox,
            this.PlatformToolStripComboBox,
            this.ImportTOCsToolStripButton,
            this.SaveLayoutToolStripButton,
            this.SaveISOToolStripButton,
            this.toolStripButton1} );
			this.MainToolStrip.Location = new System.Drawing.Point( 0, 24 );
			this.MainToolStrip.Name = "MainToolStrip";
			this.MainToolStrip.Size = new System.Drawing.Size( 1366, 25 );
			this.MainToolStrip.TabIndex = 1;
			// 
			// ConfigurationCombo
			// 
			this.ConfigurationCombo.Items.AddRange( new object[] {
            "Shipping",
            "Debug",
            "Release",
            "Test"} );
			this.ConfigurationCombo.Name = "ConfigurationCombo";
			this.ConfigurationCombo.Size = new System.Drawing.Size( 121, 25 );
			this.ConfigurationCombo.SelectedIndexChanged += new System.EventHandler( this.ConfigurationCombo_SelectedIndexChanged );
			// 
			// GameToolStripComboBox
			// 
			this.GameToolStripComboBox.Items.AddRange( new object[] {
            "Example",
            "Storm"} );
			this.GameToolStripComboBox.Name = "GameToolStripComboBox";
			this.GameToolStripComboBox.Size = new System.Drawing.Size( 121, 25 );
			this.GameToolStripComboBox.ToolTipText = "Game to make layout for";
			this.GameToolStripComboBox.SelectedIndexChanged += new System.EventHandler( this.GameToolStripComboBox_Changed );
			// 
			// PlatformToolStripComboBox
			// 
			this.PlatformToolStripComboBox.Items.AddRange( new object[] {
            "Xbox360",
            "PS3",
            "PCConsole"} );
			this.PlatformToolStripComboBox.Name = "PlatformToolStripComboBox";
			this.PlatformToolStripComboBox.Size = new System.Drawing.Size( 121, 25 );
			this.PlatformToolStripComboBox.ToolTipText = "Platform to make layout for";
			this.PlatformToolStripComboBox.SelectedIndexChanged += new System.EventHandler( this.PlatformToolStripComboBox_Changed );
			// 
			// ImportTOCsToolStripButton
			// 
			this.ImportTOCsToolStripButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.ImportTOCsToolStripButton.Image = global::UnrealDVDLayout.Properties.Resources.folder;
			this.ImportTOCsToolStripButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.ImportTOCsToolStripButton.Name = "ImportTOCsToolStripButton";
			this.ImportTOCsToolStripButton.Size = new System.Drawing.Size( 23, 22 );
			this.ImportTOCsToolStripButton.ToolTipText = "Import TOC Files";
			this.ImportTOCsToolStripButton.Click += new System.EventHandler( this.ImportTOCMenu_Click );
			// 
			// SaveLayoutToolStripButton
			// 
			this.SaveLayoutToolStripButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.SaveLayoutToolStripButton.Image = ( ( System.Drawing.Image )( resources.GetObject( "SaveLayoutToolStripButton.Image" ) ) );
			this.SaveLayoutToolStripButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.SaveLayoutToolStripButton.Name = "SaveLayoutToolStripButton";
			this.SaveLayoutToolStripButton.Size = new System.Drawing.Size( 23, 22 );
			this.SaveLayoutToolStripButton.ToolTipText = "Save Layout File (XGD/GP3)";
			this.SaveLayoutToolStripButton.Click += new System.EventHandler( this.SaveLayoutMenu_Click );
			// 
			// SaveISOToolStripButton
			// 
			this.SaveISOToolStripButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.SaveISOToolStripButton.Image = global::UnrealDVDLayout.Properties.Resources.writeiso;
			this.SaveISOToolStripButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.SaveISOToolStripButton.Name = "SaveISOToolStripButton";
			this.SaveISOToolStripButton.Size = new System.Drawing.Size( 23, 22 );
			this.SaveISOToolStripButton.ToolTipText = "Save Disc Image (ISO/XSF)";
			this.SaveISOToolStripButton.Click += new System.EventHandler( this.SaveISOClick );
			// 
			// toolStripButton1
			// 
			this.toolStripButton1.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.toolStripButton1.Image = global::UnrealDVDLayout.Properties.Resources.Emulate;
			this.toolStripButton1.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.toolStripButton1.Name = "toolStripButton1";
			this.toolStripButton1.Size = new System.Drawing.Size( 23, 22 );
			this.toolStripButton1.Text = "Launch Xbox360 DVD emulation";
			this.toolStripButton1.Click += new System.EventHandler( this.EmulateClick );
			// 
			// GenericOpenFileDialog
			// 
			this.GenericOpenFileDialog.DefaultExt = "*.txt";
			this.GenericOpenFileDialog.FileName = "Xbox360TOC.txt";
			this.GenericOpenFileDialog.Filter = "TOC Files (*.txt)|*.txt|XGD Files (*.xgd)|*.xgd";
			this.GenericOpenFileDialog.Multiselect = true;
			this.GenericOpenFileDialog.ReadOnlyChecked = true;
			this.GenericOpenFileDialog.RestoreDirectory = true;
			this.GenericOpenFileDialog.ShowHelp = true;
			this.GenericOpenFileDialog.SupportMultiDottedExtensions = true;
			// 
			// MainLogWindow
			// 
			this.MainLogWindow.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.MainLogWindow.Font = new System.Drawing.Font( "Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.MainLogWindow.Location = new System.Drawing.Point( 12, 891 );
			this.MainLogWindow.Name = "MainLogWindow";
			this.MainLogWindow.ReadOnly = true;
			this.MainLogWindow.Size = new System.Drawing.Size( 1342, 271 );
			this.MainLogWindow.TabIndex = 2;
			this.MainLogWindow.Text = "";
			// 
			// GenericSaveFileDialog
			// 
			this.GenericSaveFileDialog.DefaultExt = "*.XGD";
			this.GenericSaveFileDialog.Filter = "XGD files (*.XGD)|*.XGD";
			this.GenericSaveFileDialog.RestoreDirectory = true;
			this.GenericSaveFileDialog.ShowHelp = true;
			this.GenericSaveFileDialog.SupportMultiDottedExtensions = true;
			// 
			// FileListView
			// 
			this.FileListView.Columns.AddRange( new System.Windows.Forms.ColumnHeader[] {
            this.FileListViewPathHeader} );
			this.FileListView.ContextMenuStrip = this.FileListViewMenuStrip;
			this.FileListView.HideSelection = false;
			this.FileListView.Location = new System.Drawing.Point( 12, 88 );
			this.FileListView.Name = "FileListView";
			this.FileListView.Size = new System.Drawing.Size( 324, 797 );
			this.FileListView.TabIndex = 3;
			this.FileListView.UseCompatibleStateImageBehavior = false;
			this.FileListView.View = System.Windows.Forms.View.Details;
			// 
			// FileListViewPathHeader
			// 
			this.FileListViewPathHeader.Text = "File path";
			this.FileListViewPathHeader.Width = 344;
			// 
			// FileListViewMenuStrip
			// 
			this.FileListViewMenuStrip.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.AddToGroupToolStripMenuItem,
            this.MakeFromRegExpMenuItem} );
			this.FileListViewMenuStrip.Name = "FileListBoxMenuStrip";
			this.FileListViewMenuStrip.Size = new System.Drawing.Size( 165, 48 );
			this.FileListViewMenuStrip.Opening += new System.ComponentModel.CancelEventHandler( this.FileListBoxMenuStrip_Opening );
			// 
			// AddToGroupToolStripMenuItem
			// 
			this.AddToGroupToolStripMenuItem.DoubleClickEnabled = true;
			this.AddToGroupToolStripMenuItem.Name = "AddToGroupToolStripMenuItem";
			this.AddToGroupToolStripMenuItem.Size = new System.Drawing.Size( 164, 22 );
			this.AddToGroupToolStripMenuItem.Text = "Add to Group";
			// 
			// MakeFromRegExpMenuItem
			// 
			this.MakeFromRegExpMenuItem.Name = "MakeFromRegExpMenuItem";
			this.MakeFromRegExpMenuItem.Size = new System.Drawing.Size( 164, 22 );
			this.MakeFromRegExpMenuItem.Text = "Make from RegExp";
			this.MakeFromRegExpMenuItem.Click += new System.EventHandler( this.MakeFromRegExpMenuItem_Click );
			// 
			// GroupListView
			// 
			this.GroupListView.Columns.AddRange( new System.Windows.Forms.ColumnHeader[] {
            this.ListViewNameHeader,
            this.ListViewSizeHeader} );
			this.GroupListView.ContextMenuStrip = this.GroupListViewMenuStrip;
			this.GroupListView.FullRowSelect = true;
			this.GroupListView.HideSelection = false;
			this.GroupListView.Location = new System.Drawing.Point( 351, 88 );
			this.GroupListView.Name = "GroupListView";
			this.GroupListView.Size = new System.Drawing.Size( 250, 797 );
			this.GroupListView.TabIndex = 4;
			this.GroupListView.UseCompatibleStateImageBehavior = false;
			this.GroupListView.View = System.Windows.Forms.View.Details;
			this.GroupListView.DoubleClick += new System.EventHandler( this.GroupListView_DoubleClick );
			this.GroupListView.KeyDown += new System.Windows.Forms.KeyEventHandler( this.GroupListView_KeyPress );
			// 
			// ListViewNameHeader
			// 
			this.ListViewNameHeader.Text = "Group Name";
			this.ListViewNameHeader.Width = 176;
			// 
			// ListViewSizeHeader
			// 
			this.ListViewSizeHeader.Text = "Size (MB)";
			this.ListViewSizeHeader.Width = 70;
			// 
			// GroupListViewMenuStrip
			// 
			this.GroupListViewMenuStrip.Items.AddRange( new System.Windows.Forms.ToolStripItem[] {
            this.AddToLayer0MenuItem,
            this.AddToLayer1MenuItem,
            this.removeFromDiscToolStripMenuItem} );
			this.GroupListViewMenuStrip.Name = "GroupListViewMenuStrip";
			this.GroupListViewMenuStrip.Size = new System.Drawing.Size( 161, 70 );
			// 
			// AddToLayer0MenuItem
			// 
			this.AddToLayer0MenuItem.Name = "AddToLayer0MenuItem";
			this.AddToLayer0MenuItem.Size = new System.Drawing.Size( 160, 22 );
			this.AddToLayer0MenuItem.Text = "Add to Layer 0";
			this.AddToLayer0MenuItem.Click += new System.EventHandler( this.AddToLayer0MenuItem_Click );
			// 
			// AddToLayer1MenuItem
			// 
			this.AddToLayer1MenuItem.Name = "AddToLayer1MenuItem";
			this.AddToLayer1MenuItem.Size = new System.Drawing.Size( 160, 22 );
			this.AddToLayer1MenuItem.Text = "Add to Layer 1";
			this.AddToLayer1MenuItem.Click += new System.EventHandler( this.AddToLayer1MenuItem_Click );
			// 
			// removeFromDiscToolStripMenuItem
			// 
			this.removeFromDiscToolStripMenuItem.Name = "removeFromDiscToolStripMenuItem";
			this.removeFromDiscToolStripMenuItem.Size = new System.Drawing.Size( 160, 22 );
			this.removeFromDiscToolStripMenuItem.Text = "Remove from Disc";
			this.removeFromDiscToolStripMenuItem.Click += new System.EventHandler( this.RemoveFromDiscMenuItem_Click );
			// 
			// Layer0ListView
			// 
			this.Layer0ListView.Columns.AddRange( new System.Windows.Forms.ColumnHeader[] {
            this.Layer0GroupHeader,
            this.Layer0SizeHeader} );
			this.Layer0ListView.HideSelection = false;
			this.Layer0ListView.Location = new System.Drawing.Point( 71, 36 );
			this.Layer0ListView.Name = "Layer0ListView";
			this.Layer0ListView.Size = new System.Drawing.Size( 250, 670 );
			this.Layer0ListView.TabIndex = 5;
			this.Layer0ListView.UseCompatibleStateImageBehavior = false;
			this.Layer0ListView.View = System.Windows.Forms.View.Details;
			this.Layer0ListView.KeyDown += new System.Windows.Forms.KeyEventHandler( this.Layer0ListView_KeyPress );
			// 
			// Layer0GroupHeader
			// 
			this.Layer0GroupHeader.Text = "Group Name";
			this.Layer0GroupHeader.Width = 186;
			// 
			// Layer0SizeHeader
			// 
			this.Layer0SizeHeader.Text = "Size (MB)";
			// 
			// Layer0Label
			// 
			this.Layer0Label.AutoSize = true;
			this.Layer0Label.Font = new System.Drawing.Font( "Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.Layer0Label.ForeColor = System.Drawing.Color.RoyalBlue;
			this.Layer0Label.Location = new System.Drawing.Point( 71, 14 );
			this.Layer0Label.Name = "Layer0Label";
			this.Layer0Label.Size = new System.Drawing.Size( 99, 16 );
			this.Layer0Label.TabIndex = 12;
			this.Layer0Label.Text = "Layer 0 : 0% full";
			// 
			// GroupMoveUpButton
			// 
			this.GroupMoveUpButton.Location = new System.Drawing.Point( 607, 430 );
			this.GroupMoveUpButton.Name = "GroupMoveUpButton";
			this.GroupMoveUpButton.Size = new System.Drawing.Size( 50, 40 );
			this.GroupMoveUpButton.TabIndex = 14;
			this.GroupMoveUpButton.Text = "Up";
			this.GroupMoveUpButton.UseVisualStyleBackColor = true;
			this.GroupMoveUpButton.Click += new System.EventHandler( this.GroupMoveUpButton_Click );
			// 
			// GroupMoveDownButton
			// 
			this.GroupMoveDownButton.Location = new System.Drawing.Point( 607, 476 );
			this.GroupMoveDownButton.Name = "GroupMoveDownButton";
			this.GroupMoveDownButton.Size = new System.Drawing.Size( 50, 40 );
			this.GroupMoveDownButton.TabIndex = 15;
			this.GroupMoveDownButton.Text = "Down";
			this.GroupMoveDownButton.UseVisualStyleBackColor = true;
			this.GroupMoveDownButton.Click += new System.EventHandler( this.GroupMoveDownButton_Click );
			// 
			// DiscLayoutBox
			// 
			this.DiscLayoutBox.Controls.Add( this.SlowLabel );
			this.DiscLayoutBox.Controls.Add( this.FastLabel );
			this.DiscLayoutBox.Controls.Add( this.Layer0Label );
			this.DiscLayoutBox.Controls.Add( this.Layer0MoveDownButton );
			this.DiscLayoutBox.Controls.Add( this.Layer0MoveUpButton );
			this.DiscLayoutBox.Controls.Add( this.Layer0ListView );
			this.DiscLayoutBox.Controls.Add( this.Layer1Label );
			this.DiscLayoutBox.Controls.Add( this.Layer1MoveUpButton );
			this.DiscLayoutBox.Controls.Add( this.Layer1MoveDownButton );
			this.DiscLayoutBox.Controls.Add( this.Layer1ListView );
			this.DiscLayoutBox.Location = new System.Drawing.Point( 663, 52 );
			this.DiscLayoutBox.Name = "DiscLayoutBox";
			this.DiscLayoutBox.Size = new System.Drawing.Size( 703, 807 );
			this.DiscLayoutBox.TabIndex = 16;
			this.DiscLayoutBox.TabStop = false;
			this.DiscLayoutBox.Text = "Disc Layout";
			// 
			// SlowLabel
			// 
			this.SlowLabel.AutoSize = true;
			this.SlowLabel.ForeColor = System.Drawing.Color.Red;
			this.SlowLabel.Location = new System.Drawing.Point( 28, 655 );
			this.SlowLabel.Name = "SlowLabel";
			this.SlowLabel.Size = new System.Drawing.Size( 30, 13 );
			this.SlowLabel.TabIndex = 20;
			this.SlowLabel.Text = "Slow";
			// 
			// FastLabel
			// 
			this.FastLabel.AutoSize = true;
			this.FastLabel.ForeColor = System.Drawing.Color.Green;
			this.FastLabel.Location = new System.Drawing.Point( 28, 65 );
			this.FastLabel.Name = "FastLabel";
			this.FastLabel.Size = new System.Drawing.Size( 27, 13 );
			this.FastLabel.TabIndex = 19;
			this.FastLabel.Text = "Fast";
			// 
			// Layer0MoveDownButton
			// 
			this.Layer0MoveDownButton.Location = new System.Drawing.Point( 332, 422 );
			this.Layer0MoveDownButton.Name = "Layer0MoveDownButton";
			this.Layer0MoveDownButton.Size = new System.Drawing.Size( 50, 40 );
			this.Layer0MoveDownButton.TabIndex = 15;
			this.Layer0MoveDownButton.Text = "Down";
			this.Layer0MoveDownButton.UseVisualStyleBackColor = true;
			this.Layer0MoveDownButton.Click += new System.EventHandler( this.Layer0MoveDownButton_Click );
			// 
			// Layer0MoveUpButton
			// 
			this.Layer0MoveUpButton.Location = new System.Drawing.Point( 332, 376 );
			this.Layer0MoveUpButton.Name = "Layer0MoveUpButton";
			this.Layer0MoveUpButton.Size = new System.Drawing.Size( 50, 40 );
			this.Layer0MoveUpButton.TabIndex = 16;
			this.Layer0MoveUpButton.Text = "Up";
			this.Layer0MoveUpButton.UseVisualStyleBackColor = true;
			this.Layer0MoveUpButton.Click += new System.EventHandler( this.Layer0MoveUpButton_Click );
			// 
			// Layer1Label
			// 
			this.Layer1Label.AutoSize = true;
			this.Layer1Label.Font = new System.Drawing.Font( "Microsoft Sans Serif", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.Layer1Label.ForeColor = System.Drawing.Color.ForestGreen;
			this.Layer1Label.Location = new System.Drawing.Point( 385, 14 );
			this.Layer1Label.Name = "Layer1Label";
			this.Layer1Label.Size = new System.Drawing.Size( 99, 16 );
			this.Layer1Label.TabIndex = 14;
			this.Layer1Label.Text = "Layer 1 : 0% full";
			// 
			// Layer1MoveUpButton
			// 
			this.Layer1MoveUpButton.Location = new System.Drawing.Point( 644, 376 );
			this.Layer1MoveUpButton.Name = "Layer1MoveUpButton";
			this.Layer1MoveUpButton.Size = new System.Drawing.Size( 50, 40 );
			this.Layer1MoveUpButton.TabIndex = 17;
			this.Layer1MoveUpButton.Text = "Up";
			this.Layer1MoveUpButton.UseVisualStyleBackColor = true;
			this.Layer1MoveUpButton.Click += new System.EventHandler( this.Layer1MoveUpButton_Click );
			// 
			// Layer1MoveDownButton
			// 
			this.Layer1MoveDownButton.Location = new System.Drawing.Point( 643, 422 );
			this.Layer1MoveDownButton.Name = "Layer1MoveDownButton";
			this.Layer1MoveDownButton.Size = new System.Drawing.Size( 50, 40 );
			this.Layer1MoveDownButton.TabIndex = 18;
			this.Layer1MoveDownButton.Text = "Down";
			this.Layer1MoveDownButton.UseVisualStyleBackColor = true;
			this.Layer1MoveDownButton.Click += new System.EventHandler( this.Layer1MoveDownButton_Click );
			// 
			// Layer1ListView
			// 
			this.Layer1ListView.Columns.AddRange( new System.Windows.Forms.ColumnHeader[] {
            this.Layer1GroupHeader,
            this.Layer1SizeHeader} );
			this.Layer1ListView.HideSelection = false;
			this.Layer1ListView.Location = new System.Drawing.Point( 388, 36 );
			this.Layer1ListView.Name = "Layer1ListView";
			this.Layer1ListView.Size = new System.Drawing.Size( 250, 670 );
			this.Layer1ListView.TabIndex = 7;
			this.Layer1ListView.UseCompatibleStateImageBehavior = false;
			this.Layer1ListView.View = System.Windows.Forms.View.Details;
			// 
			// Layer1GroupHeader
			// 
			this.Layer1GroupHeader.Text = "Group Name";
			this.Layer1GroupHeader.Width = 186;
			// 
			// Layer1SizeHeader
			// 
			this.Layer1SizeHeader.Text = "Size (MB)";
			// 
			// FilesLabel
			// 
			this.FilesLabel.AutoSize = true;
			this.FilesLabel.Location = new System.Drawing.Point( 13, 68 );
			this.FilesLabel.Name = "FilesLabel";
			this.FilesLabel.Size = new System.Drawing.Size( 130, 13 );
			this.FilesLabel.TabIndex = 17;
			this.FilesLabel.Text = "Files not included on DVD";
			// 
			// GroupsLabel
			// 
			this.GroupsLabel.AutoSize = true;
			this.GroupsLabel.Location = new System.Drawing.Point( 351, 67 );
			this.GroupsLabel.Name = "GroupsLabel";
			this.GroupsLabel.Size = new System.Drawing.Size( 175, 13 );
			this.GroupsLabel.TabIndex = 18;
			this.GroupsLabel.Text = "Groups that could be added to Disc";
			// 
			// GenericFolderBrowserDialog
			// 
			this.GenericFolderBrowserDialog.RootFolder = System.Environment.SpecialFolder.MyComputer;
			// 
			// UnrealDVDLayout
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF( 6F, 13F );
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size( 1366, 1174 );
			this.Controls.Add( this.GroupsLabel );
			this.Controls.Add( this.FilesLabel );
			this.Controls.Add( this.FileListView );
			this.Controls.Add( this.GroupMoveDownButton );
			this.Controls.Add( this.MainToolStrip );
			this.Controls.Add( this.GroupMoveUpButton );
			this.Controls.Add( this.MainMenu );
			this.Controls.Add( this.MainLogWindow );
			this.Controls.Add( this.GroupListView );
			this.Controls.Add( this.DiscLayoutBox );
			this.MainMenuStrip = this.MainMenu;
			this.Name = "UnrealDVDLayout";
			this.Text = "Unreal DVD Layout Tool";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler( this.UnrealDVDLayout_FormClosed );
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler( this.UnrealDVDLayout_FormClosing );
			this.MainMenu.ResumeLayout( false );
			this.MainMenu.PerformLayout();
			this.MainToolStrip.ResumeLayout( false );
			this.MainToolStrip.PerformLayout();
			this.FileListViewMenuStrip.ResumeLayout( false );
			this.GroupListViewMenuStrip.ResumeLayout( false );
			this.DiscLayoutBox.ResumeLayout( false );
			this.DiscLayoutBox.PerformLayout();
			this.ResumeLayout( false );
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.MenuStrip MainMenu;
        private System.Windows.Forms.ToolStrip MainToolStrip;
        private System.Windows.Forms.OpenFileDialog GenericOpenFileDialog;
        private System.Windows.Forms.ToolStripMenuItem FileMainMenuItem;
        private System.Windows.Forms.ToolStripMenuItem ImportTOCsFileMenuItem;
        private System.Windows.Forms.ToolStripMenuItem QuitFileMenuItem;
        private System.Windows.Forms.ToolStripButton ImportTOCsToolStripButton;
        private System.Windows.Forms.ToolStripMenuItem ToolsMainMenuItem;
        private System.Windows.Forms.ToolStripMenuItem OptionsToolsMenuItem;
        private System.Windows.Forms.RichTextBox MainLogWindow;
        private System.Windows.Forms.ToolStripButton SaveLayoutToolStripButton;
        private System.Windows.Forms.ToolStripMenuItem SaveXGDMenuItem;
        private System.Windows.Forms.SaveFileDialog GenericSaveFileDialog;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ListView FileListView;
        private System.Windows.Forms.ContextMenuStrip FileListViewMenuStrip;
        private System.Windows.Forms.ToolStripMenuItem AddToGroupToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem MakeFromRegExpMenuItem;
        private System.Windows.Forms.ToolStripComboBox GameToolStripComboBox;
        private System.Windows.Forms.ListView GroupListView;
        private System.Windows.Forms.ColumnHeader ListViewNameHeader;
        private System.Windows.Forms.ColumnHeader ListViewSizeHeader;
        private System.Windows.Forms.ColumnHeader FileListViewPathHeader;
		private System.Windows.Forms.ListView Layer0ListView;
        private System.Windows.Forms.ColumnHeader Layer0GroupHeader;
		private System.Windows.Forms.ColumnHeader Layer0SizeHeader;
        private System.Windows.Forms.ContextMenuStrip GroupListViewMenuStrip;
        private System.Windows.Forms.ToolStripMenuItem AddToLayer0MenuItem;
        private System.Windows.Forms.ToolStripMenuItem AddToLayer1MenuItem;
		private System.Windows.Forms.Label Layer0Label;
        private System.Windows.Forms.ToolStripMenuItem removeFromDiscToolStripMenuItem;
        private System.Windows.Forms.Button GroupMoveUpButton;
		private System.Windows.Forms.Button GroupMoveDownButton;
		private System.Windows.Forms.GroupBox DiscLayoutBox;
		private System.Windows.Forms.Button Layer1MoveUpButton;
		private System.Windows.Forms.Button Layer0MoveDownButton;
		private System.Windows.Forms.Button Layer0MoveUpButton;
		private System.Windows.Forms.Label Layer1Label;
		private System.Windows.Forms.ListView Layer1ListView;
		private System.Windows.Forms.ColumnHeader Layer1GroupHeader;
		private System.Windows.Forms.ColumnHeader Layer1SizeHeader;
		private System.Windows.Forms.Button Layer1MoveDownButton;
		private System.Windows.Forms.Label SlowLabel;
		private System.Windows.Forms.Label FastLabel;
		private System.Windows.Forms.Label FilesLabel;
		private System.Windows.Forms.Label GroupsLabel;
		private System.Windows.Forms.ToolStripComboBox ConfigurationCombo;
		private System.Windows.Forms.ToolStripButton SaveISOToolStripButton;
		private System.Windows.Forms.ToolStripMenuItem setGP3TemplateToolStripMenuItem;
		private System.Windows.Forms.ToolStripComboBox PlatformToolStripComboBox;
		private System.Windows.Forms.ToolStripMenuItem setIDPTemplateToolStripMenuItem;
		private System.Windows.Forms.FolderBrowserDialog GenericFolderBrowserDialog;
		private System.Windows.Forms.ToolStripButton toolStripButton1;
    }
}

