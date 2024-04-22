/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Collections;
using System.Collections.Generic;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;
using System.Xml;
using System.Xml.Serialization;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Text;
using Stats;

using StatsViewer.Properties;

namespace StatsViewer
{
	/// <summary>
	/// The game types for browsing
	/// </summary>
	public enum GameType
	{
		Unknown,
		Editor,
		Server,
		ListenServer,
		Client
	};

	/// <summary>
	/// The platform the game is running on
	/// </summary>
	public enum PlatformType
	{
		Unknown = 0x00,
		Windows = 0x01,
		WindowsConsole = 0x02,
		WindowsServer = 0x04,
		Xenon = 0x08,
		PS3 = 0x10,
		Linux = 0x20,
		Mac = 0x40,
		IPhone = 0x80,
		Android = 0x200
	};

	/// <summary>
	/// Indicates the type of per frame search we are trying to do for a stat
	/// </summary>
	public enum SearchByType
	{
		GreaterThan,
		LessThan,
		EqualTo
	};




	/// <summary>
	/// Stats viewer application, aka UnrealPerfMon. Displays stats from the
	/// engine in a UI. Stats are either from a file or from the network when
	/// connected to a remote PC/Xe/PS3
	/// </summary>
	public class StatsViewerFrame : System.Windows.Forms.Form
	{
		/// <summary>
		/// This is the currently opened stats file
		/// </summary>
		private StatFile CurrentFile;

		/// <summary>
		/// Holds the string that is initially assigned to the main frame
		/// </summary>
		private string InitialWindowName;

		/// <summary>
		/// The number of frames to have in the viewable window 1 per 2 pixels
		/// </summary>
		private int NumViewableFrames = 0;

		/// <summary>
		/// The starting frame to draw at
		/// </summary>
		private int DrawRangeStart = 0;

		/// <summary>
		/// The ending frame to draw to
		/// </summary>
		private int DrawRangeEnd = 0;

		/// <summary>
		/// Scale to apply to the number of frames to have in a given
		/// viewable window
		/// </summary>
		private float ScaleFactor = 1.0F;

		/// <summary>
		/// Last scale set
		/// </summary>
		private float LastScaleFactor = 1.0F;

		/// <summary>
		/// The value used to figure out where to plot points. Represents
		/// the max value that can be displayed. Defaults to 100 ms.
		/// </summary>
		private double DrawMaxValue = 100.0;

        /// <summary>
        /// The max value for the right-hand-scale
        /// </summary>
        private double DrawMaxValueMB = 256.0f;

        /// <summary>
        /// The values for the y-intercepts of the current frame selection
        /// </summary>
        private Dictionary<int, double> YIntercepts = new Dictionary<int,double>();

		/// <summary>
		/// Holds the list of stats being tracked
		/// </summary>
		private ArrayList TrackedStats = new ArrayList();

		/// <summary>
		/// Holds our set of colors to use when drawing the graph data
		/// </summary>
		private GraphColors Colors = new GraphColors();

		/// <summary>
		/// Holds the last used color id for a newly added stat
		/// </summary>
		private int LastColorId = 0;

		/// <summary>
		/// Enum of text modes for drawing on the graph area
		/// </summary>
		public enum GraphTextOptions { GRAPH_ShowFrameNums, GRAPH_ShowTimes };

		/// <summary>
		/// Determines which text to place on the graph
		/// </summary>
		private GraphTextOptions ShowTextOptions = GraphTextOptions.GRAPH_ShowFrameNums;

		/// <summary>
		/// This is the font used in drawing text on the graph display
		/// </summary>
		private Font GraphFont = new Font( FontFamily.GenericSansSerif, 8 );

		/// <summary>
		/// This is a fast look up for the ranged aggregate data
		/// </summary>
		private SortedList StatIdToRangeAggData = new SortedList();

		/// <summary>
		/// Enum of modes for aggregate data
		/// </summary>
		public enum AggregateDataOptions { AGGDATA_Ranged, AGGDATA_Overall };

		/// <summary>
		/// The setting for which type of aggregate data to show
		/// </summary>
		private AggregateDataOptions AggDataOptions = AggregateDataOptions.AGGDATA_Overall;

		/// <summary>
		/// Holds the async stats UDP connection for receiving stats data and
		/// sending commands to the server
		/// </summary>
		private ServerConnection Connection;

		/// <summary>
		/// Periodic callback to process any queued packets
		/// </summary>
		private System.Windows.Forms.Timer ProcessPacketsTimer = new System.Windows.Forms.Timer();

		/// <summary>
		/// Whether to scroll the display as stats arrive via network
		/// </summary>
		private bool bShouldAutoScroll = true;

		/// <summary>
		/// Tracks the location of the mouse within the stats area
		/// </summary>
		private Point StatsGraphMouseLocation = new Point();

		/// <summary>
		/// Holds the list of currently open frame views
		/// </summary>
		private ArrayList PerFrameViews = new ArrayList();

		/// <summary>
		/// Holds the settings for the last use of the tool
		/// </summary>
		private SavedSettings LastUsedSettings;

		/// <summary>
		/// The currently selected graph frame index, or -1 if none is selected
		/// </summary>
		private int SelectedGraphFrame = -1;

		/// <summary>
		/// True if the user is currently dragging the selected graph frame with the mouse
		/// </summary>
		private bool bIsDraggingGraphSelection = false;

        /// <summary>
        /// The path to the preset views file.
        /// </summary>
        private string PresetViewsFilename = "";

		/// <summary>
		/// Handles column-based sorting of the stats data list
		/// </summary>
		private ListViewColumnSorter StatDataListColumnSorter;

		#region Windows Form Designer generated code
		private System.Windows.Forms.MenuItem divider;
		private System.Windows.Forms.MenuItem ZoomIn;
		private System.Windows.Forms.MenuItem ZoomOut;
		private System.Windows.Forms.MenuItem ShowTimes;
		private System.Windows.Forms.MenuItem divider2;
		private System.Windows.Forms.MenuItem ShowFrameNums;
		private System.Windows.Forms.MainMenu MainMenu;
		private System.Windows.Forms.MenuItem divider3;
		private System.Windows.Forms.Splitter splitter1;
		private System.Windows.Forms.Splitter splitter2;
		private System.Windows.Forms.TreeView StatGroupsTree;
		private System.Windows.Forms.Panel StatsArea;
		private System.Windows.Forms.ListView StatDataList;
		private System.Windows.Forms.MenuItem OpenStatFile;
		private System.Windows.Forms.MenuItem Exit;
		private System.Windows.Forms.MenuItem FileMenu;
		private System.Windows.Forms.MenuItem ViewMenu;
		private System.Windows.Forms.HScrollBar StatsGraphScrollbar;
		private System.Windows.Forms.MenuItem divider1;
		private System.Windows.Forms.MenuItem ViewRangedAggData;
		private System.Windows.Forms.MenuItem ViewOverallAggData;
		private System.Windows.Forms.MenuItem menuItem5;
		private System.Windows.Forms.MenuItem ConnectToTarget;
		private System.Windows.Forms.MenuItem CloseConnection;
		private System.Windows.Forms.MenuItem AutoScrollDisplay;
		private System.Windows.Forms.ContextMenu StatsGraphContextMenu;
		private System.Windows.Forms.MenuItem ViewFrameNumMenuItem;
		private System.Windows.Forms.MenuItem SelectBackgroundColorMenuItem;
		private System.Windows.Forms.ContextMenu StatsTreeMenu;
		private System.Windows.Forms.MenuItem ViewFramesByCriteriaMenuItem;
		private System.Windows.Forms.MenuItem ConnectToIP;
		#endregion
		private DoubleBufferedPanel StatsGraphPanel;
		private MenuItem SaveStatFile;
		private TextBox FilterBox;
		private Panel FilterTreePanel;
		private Panel FilterPanel;
		private Label FilterLabel;
		private IContainer components;

		private string[] FilterStrings = new string[0];
		private Button ClearFilterButton;
        private ToolTip toolTip1;
        private MenuItem CopyLocationToClipboardMI;

		Dictionary<string, bool> ExpansionStates = new Dictionary<string, bool>();

		/// <summary>
		/// Sets up all of the base settings (selection, columns, etc.)
		/// </summary>
		private void InitListView()
		{
			// Initialize and assign the column sorter for the list view
			StatDataListColumnSorter = new ListViewColumnSorter();
			StatDataList.ListViewItemSorter = StatDataListColumnSorter;

			// Set up the list view prefs
			StatDataList.View = View.Details;
			StatDataList.LabelEdit = false;
			StatDataList.AllowColumnReorder = true;
			StatDataList.CheckBoxes = true;
			StatDataList.FullRowSelect = true;
			StatDataList.GridLines = true;
			StatDataList.Sorting = SortOrder.Ascending;

			// Create columns for the items and subitems.
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			StatDataList.Columns.Add( "", -2, HorizontalAlignment.Left );
			UpdateStatsDataListColumnNames();

			// Init the menu check state
			switch( ShowTextOptions )
			{
				case GraphTextOptions.GRAPH_ShowFrameNums:
					{
						ShowTimes.Checked = false;
						ShowFrameNums.Checked = true;
						break;
					}
				case GraphTextOptions.GRAPH_ShowTimes:
					{
						ShowTimes.Checked = true;
						ShowFrameNums.Checked = false;
						break;
					}
			}
			// Init aggregate data menus check states
			switch( AggDataOptions )
			{
				case AggregateDataOptions.AGGDATA_Ranged:
					{
						ViewRangedAggData.Checked = true;
						ViewOverallAggData.Checked = false;
						break;
					}
				case AggregateDataOptions.AGGDATA_Overall:
					{
						ViewRangedAggData.Checked = false;
						ViewOverallAggData.Checked = true;
						break;
					}
			}
		}


		/// <summary>
		/// Sets the column names for the stats list based on current settings
		/// </summary>
		private void UpdateStatsDataListColumnNames()
		{
			string RangeName = ( AggDataOptions == AggregateDataOptions.AGGDATA_Ranged ) ? "Rng" : "Ovrl";

			// Create columns for the items and subitems.
			int CurColumnIndex = 0;
            StatDataList.Columns[CurColumnIndex++].Text = "Stat";
			StatDataList.Columns[ CurColumnIndex++ ].Text = "Color";
			StatDataList.Columns[ CurColumnIndex++ ].Text = "Sel Total";
			StatDataList.Columns[ CurColumnIndex++ ].Text = "Sel Calls";
			StatDataList.Columns[ CurColumnIndex++ ].Text = RangeName + " Avg";
			StatDataList.Columns[ CurColumnIndex++ ].Text = RangeName + " Avg/Call";
			StatDataList.Columns[ CurColumnIndex++ ].Text = RangeName + " Min";
			StatDataList.Columns[ CurColumnIndex++ ].Text = RangeName + " Max";

		}

		/// <summary>
		/// Frame window intialization code
		/// </summary>
		public StatsViewerFrame()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			// Save off the initial window name so we can append the file name later
			InitialWindowName = Text;

			// Set our list view defaults
			InitListView();

			// Add the callback for our timer
			ProcessPacketsTimer.Tick += new EventHandler( OnTimer );

			// And now kick it off
			ProcessPacketsTimer.Interval = 250;
			ProcessPacketsTimer.Start();

			// Load the last used settings
			if( !LoadUserSettings() )
			{
				// Couldn't load settings, so at least make sure we have defaults
				LastUsedSettings = new SavedSettings();
			}
        }

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if( components != null )
				{
					components.Dispose();
				}

				// Save the last used settings
				//SaveUserSettings();

				// Discard any connection
				if( IsConnected() )
				{
					PerformDisconnect( false );
					PerformSave( true );
				}
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(StatsViewerFrame));
			this.MainMenu = new System.Windows.Forms.MainMenu(this.components);
			this.FileMenu = new System.Windows.Forms.MenuItem();
			this.OpenStatFile = new System.Windows.Forms.MenuItem();
			this.SaveStatFile = new System.Windows.Forms.MenuItem();
			this.divider3 = new System.Windows.Forms.MenuItem();
			this.Exit = new System.Windows.Forms.MenuItem();
			this.ViewMenu = new System.Windows.Forms.MenuItem();
			this.ViewOverallAggData = new System.Windows.Forms.MenuItem();
			this.ViewRangedAggData = new System.Windows.Forms.MenuItem();
			this.divider1 = new System.Windows.Forms.MenuItem();
			this.ShowTimes = new System.Windows.Forms.MenuItem();
			this.ShowFrameNums = new System.Windows.Forms.MenuItem();
			this.divider2 = new System.Windows.Forms.MenuItem();
			this.ZoomIn = new System.Windows.Forms.MenuItem();
			this.ZoomOut = new System.Windows.Forms.MenuItem();
			this.divider = new System.Windows.Forms.MenuItem();
			this.AutoScrollDisplay = new System.Windows.Forms.MenuItem();
			this.menuItem5 = new System.Windows.Forms.MenuItem();
			this.ConnectToTarget = new System.Windows.Forms.MenuItem();
			this.ConnectToIP = new System.Windows.Forms.MenuItem();
			this.CloseConnection = new System.Windows.Forms.MenuItem();
			this.StatGroupsTree = new System.Windows.Forms.TreeView();
			this.StatsTreeMenu = new System.Windows.Forms.ContextMenu();
			this.ViewFramesByCriteriaMenuItem = new System.Windows.Forms.MenuItem();
			this.splitter1 = new System.Windows.Forms.Splitter();
			this.StatsArea = new System.Windows.Forms.Panel();
			this.StatsGraphPanel = new StatsViewer.DoubleBufferedPanel();
			this.StatsGraphContextMenu = new System.Windows.Forms.ContextMenu();
			this.ViewFrameNumMenuItem = new System.Windows.Forms.MenuItem();
			this.SelectBackgroundColorMenuItem = new System.Windows.Forms.MenuItem();
			this.CopyLocationToClipboardMI = new System.Windows.Forms.MenuItem();
			this.StatsGraphScrollbar = new System.Windows.Forms.HScrollBar();
			this.splitter2 = new System.Windows.Forms.Splitter();
			this.StatDataList = new System.Windows.Forms.ListView();
			this.FilterBox = new System.Windows.Forms.TextBox();
			this.FilterTreePanel = new System.Windows.Forms.Panel();
			this.FilterPanel = new System.Windows.Forms.Panel();
			this.FilterLabel = new System.Windows.Forms.Label();
			this.ClearFilterButton = new System.Windows.Forms.Button();
			this.toolTip1 = new System.Windows.Forms.ToolTip(this.components);
			this.StatsArea.SuspendLayout();
			this.StatsGraphPanel.SuspendLayout();
			this.FilterTreePanel.SuspendLayout();
			this.FilterPanel.SuspendLayout();
			this.SuspendLayout();
			// 
			// MainMenu
			// 
			this.MainMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
            this.FileMenu,
            this.ViewMenu,
            this.menuItem5});
			// 
			// FileMenu
			// 
			this.FileMenu.Index = 0;
			this.FileMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
            this.OpenStatFile,
            this.SaveStatFile,
            this.divider3,
            this.Exit});
			this.FileMenu.Text = "&File";
			// 
			// OpenStatFile
			// 
			this.OpenStatFile.Index = 0;
			this.OpenStatFile.Text = "&Open Stats Data (.UStats)...";
			this.OpenStatFile.Click += new System.EventHandler(this.OpenStatFile_Click);
			// 
			// SaveStatFile
			// 
			this.SaveStatFile.Index = 1;
			this.SaveStatFile.Text = "&Save As...";
			this.SaveStatFile.Click += new System.EventHandler(this.SaveStatFile_Click);
			// 
			// divider3
			// 
			this.divider3.Index = 2;
			this.divider3.Text = "-";
			// 
			// Exit
			// 
			this.Exit.Index = 3;
			this.Exit.Text = "E&xit";
			this.Exit.Click += new System.EventHandler(this.Exit_Click);
			// 
			// ViewMenu
			// 
			this.ViewMenu.Index = 1;
			this.ViewMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
            this.ViewOverallAggData,
            this.ViewRangedAggData,
            this.divider1,
            this.ShowTimes,
            this.ShowFrameNums,
            this.divider2,
            this.ZoomIn,
            this.ZoomOut,
            this.divider,
            this.AutoScrollDisplay});
			this.ViewMenu.Text = "&View";
			// 
			// ViewOverallAggData
			// 
			this.ViewOverallAggData.Checked = true;
			this.ViewOverallAggData.Index = 0;
			this.ViewOverallAggData.RadioCheck = true;
			this.ViewOverallAggData.Text = "O&verall Aggregate Data";
			this.ViewOverallAggData.Click += new System.EventHandler(this.ViewOverallAggData_Click);
			// 
			// ViewRangedAggData
			// 
			this.ViewRangedAggData.Index = 1;
			this.ViewRangedAggData.RadioCheck = true;
			this.ViewRangedAggData.Text = "&Ranged Aggregate Data";
			this.ViewRangedAggData.Click += new System.EventHandler(this.ViewRangedAggData_Click);
			// 
			// divider1
			// 
			this.divider1.Index = 2;
			this.divider1.Text = "-";
			// 
			// ShowTimes
			// 
			this.ShowTimes.Index = 3;
			this.ShowTimes.RadioCheck = true;
			this.ShowTimes.Text = "Show &Times";
			this.ShowTimes.Click += new System.EventHandler(this.ShowTimes_Click);
			// 
			// ShowFrameNums
			// 
			this.ShowFrameNums.Index = 4;
			this.ShowFrameNums.RadioCheck = true;
			this.ShowFrameNums.Text = "Show &Frame #";
			this.ShowFrameNums.Click += new System.EventHandler(this.ShowFrameNums_Click);
			// 
			// divider2
			// 
			this.divider2.Index = 5;
			this.divider2.Text = "-";
			// 
			// ZoomIn
			// 
			this.ZoomIn.Index = 6;
			this.ZoomIn.Shortcut = System.Windows.Forms.Shortcut.F1;
			this.ZoomIn.Text = "Zoom &In";
			this.ZoomIn.Click += new System.EventHandler(this.ZoomIn_Click);
			// 
			// ZoomOut
			// 
			this.ZoomOut.Index = 7;
			this.ZoomOut.Shortcut = System.Windows.Forms.Shortcut.F2;
			this.ZoomOut.Text = "Zoom O&ut";
			this.ZoomOut.Click += new System.EventHandler(this.ZoomOut_Click);
			// 
			// divider
			// 
			this.divider.Index = 8;
			this.divider.Text = "-";
			// 
			// AutoScrollDisplay
			// 
			this.AutoScrollDisplay.Checked = true;
			this.AutoScrollDisplay.Index = 9;
			this.AutoScrollDisplay.Shortcut = System.Windows.Forms.Shortcut.F3;
			this.AutoScrollDisplay.Text = "&Auto-scroll Display";
			this.AutoScrollDisplay.Click += new System.EventHandler(this.AutoScrollDisplay_Click);
			// 
			// menuItem5
			// 
			this.menuItem5.Index = 2;
			this.menuItem5.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
            this.ConnectToTarget,
            this.ConnectToIP,
            this.CloseConnection});
			this.menuItem5.Text = "&Connection";
			// 
			// ConnectToTarget
			// 
			this.ConnectToTarget.Index = 0;
			this.ConnectToTarget.Text = "&Connect to Target...";
			this.ConnectToTarget.Click += new System.EventHandler(this.ConnectTo_Click);
			// 
			// ConnectToIP
			// 
			this.ConnectToIP.Index = 1;
			this.ConnectToIP.Text = "Connect to &IP...";
			this.ConnectToIP.Click += new System.EventHandler(this.ConnectToIP_Click);
			// 
			// CloseConnection
			// 
			this.CloseConnection.Index = 2;
			this.CloseConnection.Text = "C&lose Connection";
			this.CloseConnection.Click += new System.EventHandler(this.CloseConnection_Click);
			// 
			// StatGroupsTree
			// 
			this.StatGroupsTree.AllowDrop = true;
			this.StatGroupsTree.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
						| System.Windows.Forms.AnchorStyles.Left)
						| System.Windows.Forms.AnchorStyles.Right)));
			this.StatGroupsTree.ContextMenu = this.StatsTreeMenu;
			this.StatGroupsTree.Location = new System.Drawing.Point(0, 22);
			this.StatGroupsTree.Name = "StatGroupsTree";
			this.StatGroupsTree.Size = new System.Drawing.Size(200, 267);
			this.StatGroupsTree.TabIndex = 0;
			this.StatGroupsTree.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.StatsGroupTree_DoubleClick);
			this.StatGroupsTree.DragDrop += new System.Windows.Forms.DragEventHandler(this.StatsGroupTree_DragDrop);
			this.StatGroupsTree.DragEnter += new System.Windows.Forms.DragEventHandler(this.StatsGroupTree_DragEnter);
			this.StatGroupsTree.ItemDrag += new System.Windows.Forms.ItemDragEventHandler(this.StatsGroupTree_ItemDrag);
			// 
			// StatsTreeMenu
			// 
			this.StatsTreeMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
            this.ViewFramesByCriteriaMenuItem});
			this.StatsTreeMenu.Popup += new System.EventHandler(this.StatsTreeMenu_Popup);
			// 
			// ViewFramesByCriteriaMenuItem
			// 
			this.ViewFramesByCriteriaMenuItem.Index = 0;
			this.ViewFramesByCriteriaMenuItem.Text = "&Open Call Graph by Criteria...";
			this.ViewFramesByCriteriaMenuItem.Click += new System.EventHandler(this.ViewFramesByCriteriaMenuItem_Click);
			// 
			// splitter1
			// 
			this.splitter1.Location = new System.Drawing.Point(200, 0);
			this.splitter1.Name = "splitter1";
			this.splitter1.Size = new System.Drawing.Size(3, 289);
			this.splitter1.TabIndex = 1;
			this.splitter1.TabStop = false;
			// 
			// StatsArea
			// 
			this.StatsArea.Controls.Add(this.StatsGraphPanel);
			this.StatsArea.Controls.Add(this.splitter2);
			this.StatsArea.Controls.Add(this.StatDataList);
			this.StatsArea.Dock = System.Windows.Forms.DockStyle.Fill;
			this.StatsArea.Location = new System.Drawing.Point(203, 0);
			this.StatsArea.Name = "StatsArea";
			this.StatsArea.Size = new System.Drawing.Size(810, 289);
			this.StatsArea.TabIndex = 2;
			// 
			// StatsGraphPanel
			// 
			this.StatsGraphPanel.AllowDrop = true;
			this.StatsGraphPanel.ContextMenu = this.StatsGraphContextMenu;
			this.StatsGraphPanel.Controls.Add(this.StatsGraphScrollbar);
			this.StatsGraphPanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.StatsGraphPanel.Location = new System.Drawing.Point(0, 0);
			this.StatsGraphPanel.Name = "StatsGraphPanel";
			this.StatsGraphPanel.Size = new System.Drawing.Size(810, 142);
			this.StatsGraphPanel.TabIndex = 3;
			this.StatsGraphPanel.TabStop = true;
			this.StatsGraphPanel.DoubleClick += new System.EventHandler(this.StatsGraphPanel_DoubleClick);
			this.StatsGraphPanel.MouseWheel += new System.Windows.Forms.MouseEventHandler(this.StatsGraphPanel_MouseWheel);
			this.StatsGraphPanel.Paint += new System.Windows.Forms.PaintEventHandler(this.StatsGraphPanel_Paint);
			this.StatsGraphPanel.MouseMove += new System.Windows.Forms.MouseEventHandler(this.StatsGraphPanel_MouseMove);
			this.StatsGraphPanel.Click += new System.EventHandler(this.StatsGraphPanel_Click);
			this.StatsGraphPanel.DragDrop += new System.Windows.Forms.DragEventHandler(this.GraphPanelOrStatsList_DragDrop);
			this.StatsGraphPanel.MouseDown += new System.Windows.Forms.MouseEventHandler(this.StatsGraphPanel_MouseDown);
			this.StatsGraphPanel.Resize += new System.EventHandler(this.StatsGraphPanel_Resize);
			this.StatsGraphPanel.MouseHover += new System.EventHandler(this.StatsGraphPanel_MouseHover);
			this.StatsGraphPanel.MouseUp += new System.Windows.Forms.MouseEventHandler(this.StatsGraphPanel_MouseUp);
			this.StatsGraphPanel.DragEnter += new System.Windows.Forms.DragEventHandler(this.GraphPanelOrStatsList_DragEnter);
			// 
			// StatsGraphContextMenu
			// 
			this.StatsGraphContextMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
            this.ViewFrameNumMenuItem,
            this.SelectBackgroundColorMenuItem,
            this.CopyLocationToClipboardMI});
			this.StatsGraphContextMenu.Popup += new System.EventHandler(this.StatsGraphContextMenu_Popup);
			// 
			// ViewFrameNumMenuItem
			// 
			this.ViewFrameNumMenuItem.Index = 0;
			this.ViewFrameNumMenuItem.Text = "View Frame #";
			this.ViewFrameNumMenuItem.Click += new System.EventHandler(this.ViewFrameNumMenuItem_Click);
			// 
			// SelectBackgroundColorMenuItem
			// 
			this.SelectBackgroundColorMenuItem.Index = 1;
			this.SelectBackgroundColorMenuItem.Text = "Select Background Color...";
			this.SelectBackgroundColorMenuItem.Click += new System.EventHandler(this.SelectBackgroundColorMenuItem_Click);
			// 
			// CopyLocationToClipboardMI
			// 
			this.CopyLocationToClipboardMI.Index = 2;
			this.CopyLocationToClipboardMI.Text = "Copy Location to Clipboard";
			this.CopyLocationToClipboardMI.Click += new System.EventHandler(this.CopyLocationToClipboardMI_Click);
			// 
			// StatsGraphScrollbar
			// 
			this.StatsGraphScrollbar.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.StatsGraphScrollbar.Location = new System.Drawing.Point(0, 125);
			this.StatsGraphScrollbar.Name = "StatsGraphScrollbar";
			this.StatsGraphScrollbar.Size = new System.Drawing.Size(810, 17);
			this.StatsGraphScrollbar.TabIndex = 2;
			this.StatsGraphScrollbar.Scroll += new System.Windows.Forms.ScrollEventHandler(this.StatsGraphScrollbar_Scroll);
			// 
			// splitter2
			// 
			this.splitter2.BackColor = System.Drawing.SystemColors.Control;
			this.splitter2.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.splitter2.Location = new System.Drawing.Point(0, 142);
			this.splitter2.Name = "splitter2";
			this.splitter2.Size = new System.Drawing.Size(810, 3);
			this.splitter2.TabIndex = 1;
			this.splitter2.TabStop = false;
			// 
			// StatDataList
			// 
			this.StatDataList.AllowDrop = true;
			this.StatDataList.Dock = System.Windows.Forms.DockStyle.Bottom;
			this.StatDataList.Location = new System.Drawing.Point(0, 145);
			this.StatDataList.Name = "StatDataList";
			this.StatDataList.OwnerDraw = true;
			this.StatDataList.Size = new System.Drawing.Size(810, 144);
			this.StatDataList.TabIndex = 0;
			this.StatDataList.UseCompatibleStateImageBehavior = false;
			this.StatDataList.DrawColumnHeader += new System.Windows.Forms.DrawListViewColumnHeaderEventHandler(this.StatDataList_DrawColumnHeader);
			this.StatDataList.ItemActivate += new System.EventHandler(this.StatDataList_ItemActivate);
			this.StatDataList.ItemChecked += new System.Windows.Forms.ItemCheckedEventHandler(this.StatDataList_ItemChecked);
			this.StatDataList.SelectedIndexChanged += new System.EventHandler(this.StatDataList_SelectedIndexChanged);
			this.StatDataList.DragDrop += new System.Windows.Forms.DragEventHandler(this.GraphPanelOrStatsList_DragDrop);
			this.StatDataList.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler(this.StatDataList_ColumnClick);
			this.StatDataList.DragEnter += new System.Windows.Forms.DragEventHandler(this.GraphPanelOrStatsList_DragEnter);
			this.StatDataList.KeyDown += new System.Windows.Forms.KeyEventHandler(this.StatDataList_KeyDown);
			this.StatDataList.ItemDrag += new System.Windows.Forms.ItemDragEventHandler(this.StatDataList_ItemDrag);
			this.StatDataList.DrawSubItem += new System.Windows.Forms.DrawListViewSubItemEventHandler(this.StatDataList_DrawSubItem);
			// 
			// FilterBox
			// 
			this.FilterBox.Dock = System.Windows.Forms.DockStyle.Right;
			this.FilterBox.Location = new System.Drawing.Point(35, 0);
			this.FilterBox.Name = "FilterBox";
			this.FilterBox.Size = new System.Drawing.Size(140, 20);
			this.FilterBox.TabIndex = 3;
			this.FilterBox.TextChanged += new System.EventHandler(this.FilterBox_TextChanged);
			// 
			// FilterTreePanel
			// 
			this.FilterTreePanel.AutoSize = true;
			this.FilterTreePanel.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.FilterTreePanel.Controls.Add(this.FilterPanel);
			this.FilterTreePanel.Controls.Add(this.StatGroupsTree);
			this.FilterTreePanel.Dock = System.Windows.Forms.DockStyle.Left;
			this.FilterTreePanel.Location = new System.Drawing.Point(0, 0);
			this.FilterTreePanel.Name = "FilterTreePanel";
			this.FilterTreePanel.Size = new System.Drawing.Size(200, 289);
			this.FilterTreePanel.TabIndex = 4;
			// 
			// FilterPanel
			// 
			this.FilterPanel.AutoSize = true;
			this.FilterPanel.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.FilterPanel.Controls.Add(this.FilterLabel);
			this.FilterPanel.Controls.Add(this.FilterBox);
			this.FilterPanel.Controls.Add(this.ClearFilterButton);
			this.FilterPanel.Dock = System.Windows.Forms.DockStyle.Top;
			this.FilterPanel.Location = new System.Drawing.Point(0, 0);
			this.FilterPanel.MinimumSize = new System.Drawing.Size(0, 20);
			this.FilterPanel.Name = "FilterPanel";
			this.FilterPanel.Size = new System.Drawing.Size(200, 20);
			this.FilterPanel.TabIndex = 4;
			// 
			// FilterLabel
			// 
			this.FilterLabel.AutoSize = true;
			this.FilterLabel.Dock = System.Windows.Forms.DockStyle.Right;
			this.FilterLabel.Location = new System.Drawing.Point(6, 0);
			this.FilterLabel.Margin = new System.Windows.Forms.Padding(3);
			this.FilterLabel.Name = "FilterLabel";
			this.FilterLabel.Size = new System.Drawing.Size(29, 13);
			this.FilterLabel.TabIndex = 4;
			this.FilterLabel.Text = "Filter";
			this.FilterLabel.TextAlign = System.Drawing.ContentAlignment.MiddleCenter;
			// 
			// ClearFilterButton
			// 
			this.ClearFilterButton.AutoSizeMode = System.Windows.Forms.AutoSizeMode.GrowAndShrink;
			this.ClearFilterButton.BackgroundImage = global::StatsViewer.Properties.Resources.Cancel;
			this.ClearFilterButton.BackgroundImageLayout = System.Windows.Forms.ImageLayout.Center;
			this.ClearFilterButton.Dock = System.Windows.Forms.DockStyle.Right;
			this.ClearFilterButton.Location = new System.Drawing.Point(175, 0);
			this.ClearFilterButton.Name = "ClearFilterButton";
			this.ClearFilterButton.Size = new System.Drawing.Size(25, 20);
			this.ClearFilterButton.TabIndex = 5;
			this.ClearFilterButton.UseVisualStyleBackColor = true;
			this.ClearFilterButton.Click += new System.EventHandler(this.ClearFilterButton_Click);
			// 
			// StatsViewerFrame
			// 
			this.AllowDrop = true;
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(1013, 289);
			this.Controls.Add(this.StatsArea);
			this.Controls.Add(this.splitter1);
			this.Controls.Add(this.FilterTreePanel);
			this.DataBindings.Add(new System.Windows.Forms.Binding("Location", global::StatsViewer.Properties.Settings.Default, "LastLocation", true, System.Windows.Forms.DataSourceUpdateMode.OnPropertyChanged));
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Location = global::StatsViewer.Properties.Settings.Default.LastLocation;
			this.Menu = this.MainMenu;
			this.Name = "StatsViewerFrame";
			this.Text = "Unreal Engine 3 Stats Viewer";
			this.DragDrop += new System.Windows.Forms.DragEventHandler(this.StatsViewerFrame_DragDrop);
			this.DragEnter += new System.Windows.Forms.DragEventHandler(this.StatsViewerFrame_DragEnter);
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.StatsViewerFrame_FormClosing);
			this.StatsArea.ResumeLayout(false);
			this.StatsGraphPanel.ResumeLayout(false);
			this.FilterTreePanel.ResumeLayout(false);
			this.FilterTreePanel.PerformLayout();
			this.FilterPanel.ResumeLayout(false);
			this.FilterPanel.PerformLayout();
			this.ResumeLayout(false);
			this.PerformLayout();

		}
		#endregion

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static void Main( string[] Args )
		{
			try
			{
				// Create the frame window
				StatsViewerFrame Frame = new StatsViewerFrame();

				// If the user passed the filename open it
				if( Args.Length > 0 )
				{
					Frame.OpenFile( Args[ 0 ] );
				}
				Application.Run( Frame );
			}
			catch( Exception E )
			{
				MessageBox.Show( "Exception starting app:\r\n" + E.ToString() );
			}
		}

		/// <summary>
		/// Close the app
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void Exit_Click( object sender, System.EventArgs e )
		{
			Application.Exit();
		}

		/// <summary>
		/// Empties the contents of the tree & list views
		/// </summary>
		private void EmptyUI()
		{
			// Close the dependent views first
			ClosePerFrameViews();

			// Zero out our per file instance data
			TrackedStats.Clear();

			// Empty any range aggregate data
			StatIdToRangeAggData.Clear();

			// Clear the stats form
			StatDataList.Items.Clear();
			StatGroupsTree.Nodes.Clear();

			// Empty the scroll bar
			StatsGraphScrollbar.Minimum = 0;
			StatsGraphScrollbar.Maximum = 0;
			LastColorId = 0;
		}


		/// <summary>
		/// Updates the number of viewable frames as well as the scrollbar thumb size
		/// </summary>
		private void UpdateViewableFrames()
		{
			// Set up scroll bars based on the number of frames
			if( CurrentFile != null )
			{
				StatsGraphScrollbar.Minimum = 0;
				StatsGraphScrollbar.Maximum = CurrentFile.Frames.Length - 1;
			}

			// Update the number of frames in the view area
			NumViewableFrames = StatsGraphPanel.Width / 2;
			if( CurrentFile != null && CurrentFile.Frames.Length < NumViewableFrames )
			{
				NumViewableFrames = CurrentFile.Frames.Length;
			}

			// Set the 'large change' value for the scrollbar.  This is how much the scrollbar jumps when
			// clicking on an area outside of the thumb widget.  Also, it defines the visual size of the thumb.
			StatsGraphScrollbar.LargeChange = NumViewableFrames;
		}


		/// <summary>
		/// Initializes the UI from the newly opened file data
		/// </summary>
		private void BuildUI()
		{
			DrawRangeStart = 0;
			ScaleFactor = 1.0f;

			// Update the number of viewable frames as well as the scrollbar thumb size
			UpdateViewableFrames();


			// Set our ending range to the number of viewable frames or the
			// length of the array
			DrawRangeEnd = NumViewableFrames - 1;

			// Do we have valid data?
			if( CurrentFile != null && CurrentFile.Frames.Length > 0 )
			{
				// Select the last frame
				SelectedGraphFrame = CurrentFile.Frames.Length - 1;

				// Check to see if we have a 'frame time' stat.  If so, we'll always display that by default.
				foreach( Frame CurFrame in CurrentFile.Frames )
				{
					Stat FrameTimeStat = CurFrame.GetFrameTimeStat();
					if( FrameTimeStat != null )
					{
						if( TrackStat( FrameTimeStat ) )
						{
							AddStatToList( FrameTimeStat );
						}

						// Resize the stat name column
						StatDataList.Columns[ 0 ].Width = -1;

						break;
					}
				}
			}

			// Update the master list of stats
			BuildTreeView();

			// Force a redraw
			StatsGraphPanel.Invalidate();
		}

		/// <summary>
		/// Common method for opening a file and populating the view
		/// </summary>
		/// <param name="FileName">The file to open and process</param>
		public void OpenFile( string FileName )
		{
			// Show that we are going to be busy
			Cursor = Cursors.WaitCursor;

			bool bRefreshGUI = false;
			bool bLoadedStatsFile = false;

            if (FileName.EndsWith(".ustats", true, null))
			{
				// Load the binary file
				string ErrorMessage;
				if( BinaryStatsDataLoader.LoadStatsFile( FileName, out CurrentFile, out ErrorMessage ) )
				{
					// Update the title bar if we loaded ok
					Text = InitialWindowName + " - " + FileName;

					bLoadedStatsFile = true;

					// Loaded OK!
					bRefreshGUI = true;
				}

				
				// NOTE: We display the error if we have one, even if the file was loaded OK
				if( ErrorMessage != null )
				{
					MessageBox.Show( this, ErrorMessage, "File Error" );
				}
			}
            else if (FileName.EndsWith(".xml", true, null))
			{
				// Load the XML file
				XmlStatsDataLoader StatsLoader = new XmlStatsDataLoader();
				if( StatsLoader.LoadStatsFile( FileName, out CurrentFile ) )
				{
					// Update the title bar if we loaded ok
					Text = InitialWindowName + " - " + FileName;

					CurrentFile.FixupData();

					bLoadedStatsFile = true;

					// Loaded OK!
					bRefreshGUI = true;
				}
				else
				{
					MessageBox.Show( this, "Unable to read/parse file", "File Error" );
				}
			}
			else
			{
				MessageBox.Show( this, "Unrecognized file type", "File Error" );
			}


			if( bLoadedStatsFile )
			{
				if( CurrentFile.RepairWarningMessages.Count > 0 )
				{
					StringBuilder WarningText = new StringBuilder();
					foreach( string CurWarning in CurrentFile.RepairWarningMessages )
					{
						WarningText.AppendLine( CurWarning );
					}
					MessageBox.Show( this, WarningText.ToString(), "Stat Data Warnings" );
				}
			}

			// Update the title bar if we loaded ok
			if( bRefreshGUI )
			{
				// Clear the current selection
				SelectedGraphFrame = -1;
				bIsDraggingGraphSelection = false;

				// Clear out any windows
				EmptyUI();

				// Update the UI views
				BuildUI();
			}

			// Restore the cursor when done
			Cursor = Cursors.Default;
		}

		/// <summary>
		/// Opens an XML file containing all of the stats data
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void OpenStatFile_Click( object sender, System.EventArgs e )
		{
			OpenFileDialog OpenFileDlg = new OpenFileDialog();

			OpenFileDlg.Filter = "Stats Files (*.ustats;*.xml)|*.ustats;*.xml|All Files (*.*)|*.*";
			OpenFileDlg.FilterIndex = 1;	// Default to Stats Files
			OpenFileDlg.RestoreDirectory = true;
			OpenFileDlg.ValidateNames = true;

			// Show the dialog and only load the file if the user said ok
			if( OpenFileDlg.ShowDialog() == DialogResult.OK )
			{
				// Use the common file opening routine
				OpenFile( OpenFileDlg.FileName );
			}
		}


		/// <summary>
		/// Writes the current stats data out to an XML file
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void SaveStatFile_Click( object sender, System.EventArgs e )
		{
			try
			{
				// Use the common save routine and don't prompt
				PerformSave( false );
			}
			catch( Exception E )
			{
				Console.WriteLine( "Exception saving XML: " + E.ToString() );
				MessageBox.Show( this, "Unable to save the file data", "File Error" );
			}
		}


		/// <summary>
		/// Adds all of the known groups to the tree
		/// </summary>
		/// <param name="Parent">The parent node list to attach the groups to</param>
		private void AddGroupsToTree( TreeNodeCollection ParentNodeList )
		{
			if( CurrentFile != null )
			{
				// Add each group to the tree view
				foreach( Group group in CurrentFile.Descriptions.Groups )
				{
					bool bGroupAddedYet = false;
					TreeNode GroupNode = null;
					// Now add all the stats in this group
					foreach( Stat stat in group.OwnedStats )
					{
						string LowerCaseStatName = stat.Name.ToLower();
						bool bFoundFilter = true;
						for (int i = 0; i < FilterStrings.Length; ++i)
						{
							if (!LowerCaseStatName.Contains(FilterStrings[i]))
							{
								bFoundFilter = false;
								break;
							}
						}

						if (bFoundFilter)
						{
							if (!bGroupAddedYet)
							{
								// Create the new tree node for this group
								GroupNode = new TreeNode(group.Name);

								// Set the object that it is representing
								GroupNode.Tag = group;

								// Add the group to the tree
								ParentNodeList.Add(GroupNode);

								//only call this once per group
								bGroupAddedYet = true;
							}

							// Create the new tree node for this group
							TreeNode NewNode = new TreeNode(stat.Name);

							// Set the object that it is representing
							NewNode.Tag = stat;

							// Add the stat to the group's node
							GroupNode.Nodes.Add(NewNode);

							//get the statistics data for color coding
							AggregateStatData StatData = CurrentFile.GetAggregateData(stat.StatId);
							if (StatData.NumStats == 0)
							{
								NewNode.ForeColor = Color.Gray;
							}

							//bold if being displayed or not
							for (int DisplayedStatIndex = 0; DisplayedStatIndex < StatDataList.Items.Count; ++DisplayedStatIndex)
							{
								if (StatDataList.Items[DisplayedStatIndex].Text == stat.Name)
								{
									NewNode.NodeFont = new Font(StatGroupsTree.Font, FontStyle.Bold);
									break;
								}
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Adds the data in the CurrentFile to the treeview portion of the UI
		/// </summary>
		private void BuildTreeView()
		{
			// Avoid multiple repaint flashes
			StatGroupsTree.BeginUpdate();
			if( StatGroupsTree.Nodes.Count > 0 )
			{
				//save expansion state for any nodes that exist
				for (int i = 0; i < StatGroupsTree.Nodes.Count; ++i )
				{
					string GroupName = StatGroupsTree.Nodes[i].Text;
					bool bExpanded = StatGroupsTree.Nodes[i].IsExpanded;
					if (ExpansionStates.ContainsKey(GroupName))
					{
						ExpansionStates.Remove(GroupName);
					}
					ExpansionStates.Add(GroupName, bExpanded);
				}

				StatGroupsTree.Nodes.Clear();
			}

			// Add all of the groups to the tree
			AddGroupsToTree( StatGroupsTree.Nodes );

			//restore expansion stsate
			for (int i = 0; i < StatGroupsTree.Nodes.Count; ++i)
			{
				string GroupName = StatGroupsTree.Nodes[i].Text;
				bool bExpanded = false;
				if (ExpansionStates.ContainsKey(GroupName))
				{
					bExpanded = ExpansionStates[GroupName];
				}
				if (bExpanded)
				{
					StatGroupsTree.Nodes[i].Expand();
				}
				else
				{
					StatGroupsTree.Nodes[i].Collapse();
				}
			}


			// Sort everything alphabetically
			StatGroupsTree.Sort();

			// Repaint the control now
			StatGroupsTree.EndUpdate();
		}

		/// <summary>
		/// Handles drag and drop being initiated from the stats tree view
		/// </summary>
		/// <param name="sender">The tree view</param>
		/// <param name="e">The node being dragged</param>
		private void StatsGroupTree_ItemDrag( object sender, System.Windows.Forms.ItemDragEventArgs e )
		{
			int DragDropValue = 0;

			// Get the node so we can get the stat/group data out of it
			TreeNode Item = (TreeNode)e.Item;

			// Root node doesn't have an object associated with it
			if( Item.Tag != null )
			{
				UIBaseElement Base = (UIBaseElement)Item.Tag;

				// Handle building the value based upon the type
				if( Base.UIType == UIBaseElement.ElementType.StatsObject )
				{
					Stat stat = (Stat)Base;
					DragDropValue = ( ( (byte)UIBaseElement.ElementType.StatsObject ) << 24 ) | stat.StatId;
				}
				else if( Base.UIType == UIBaseElement.ElementType.GroupObject )
				{
					Group group = (Group)Base;
					DragDropValue = ( ( (byte)UIBaseElement.ElementType.GroupObject ) << 24 ) | group.GroupId;
				}

				// Kick off drag and drop
				StatGroupsTree.DoDragDrop( DragDropValue, DragDropEffects.Link );
			}
		}

		/// <summary>
		/// Called when a drag and drop operation first enters the window. The
		/// data is checked to see if we can parse it. If so, we accept the
		/// drag operation
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGroupTree_DragEnter( object sender, System.Windows.Forms.DragEventArgs e )
		{
			// If this is one of our types, allow it to be dropped
			if( e.Data.GetDataPresent( typeof( ListView.SelectedListViewItemCollection ) ) )
			{
				e.Effect = DragDropEffects.Link;
			}
			else
			{
				// Unknown type so ignore
				e.Effect = DragDropEffects.None;
			}
		}

		/// <summary>
		/// Called when a drag and drop operation first enters the window. The
		/// data is checked to see if we can parse it. If so, we accept the
		/// drag operation
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void GraphPanelOrStatsList_DragEnter( object sender, System.Windows.Forms.DragEventArgs e )
		{
			// If this is one of our types, allow it to be dropped
			if( e.Data.GetDataPresent( typeof( int ) ) )
			{
				e.Effect = DragDropEffects.Link;
			}
			else
			{
				// Unknown type so ignore
				e.Effect = DragDropEffects.None;
			}
		}

		/// <summary>
		/// Adds the stat to the list view
		/// </summary>
		/// <param name="StatToAdd">The stat to add the data for</param>
		private void AddStatToList( Stat StatToAdd )
		{
			AggregateStatData AggData = null;

            string Suffix = "";
            double MetadataScale = 1.0;
            if (StatToAdd.Metadata != null)
            {
                if (StatToAdd.Metadata.Suffix != "")
                {
                    Suffix = StatToAdd.Metadata.Suffix;
                }
                MetadataScale = StatToAdd.Metadata.Scale;
            }
            
			// Figure out which aggregate data object to add to the display
			switch( AggDataOptions )
			{
				case AggregateDataOptions.AGGDATA_Ranged:
					{
						AggData = (AggregateStatData)StatIdToRangeAggData[ StatToAdd.StatId ];
						break;
					}
				case AggregateDataOptions.AGGDATA_Overall:
					{
						AggData = CurrentFile.GetAggregateData( StatToAdd.StatId );
						break;
					}
			}
			//don't bother showing empty stats
			if( AggData != null && (AggData.NumStats > 0))
			{
				// Create the list view item to add. Note the name is added as an
				// string of spaces so that the background color is the color of
				// the stat
                ListViewItem lvi = new ListViewItem( StatToAdd.Name );
				lvi.Tag = StatToAdd;
				lvi.UseItemStyleForSubItems = false;

                // Color column
                lvi.SubItems.Add("       ");


				// Add data for the selected frame
				double CurFrameTotal = 0.0;
				int CurFrameCalls = 0;
				if( SelectedGraphFrame != -1 )
				{
					if( CurrentFile != null && CurrentFile.Frames.Length > 0 )
					{
						if( SelectedGraphFrame >= 0 && SelectedGraphFrame < CurrentFile.Frames.Length )
						{
							PerFrameStatData FrameData = CurrentFile.Frames[ SelectedGraphFrame ].GetPerFrameStat( StatToAdd.StatId );
							if( FrameData != null )
							{
								if( StatToAdd.Type == Stat.StatType.STATTYPE_CycleCounter )
								{
									CurFrameTotal = FrameData.TotalTime;
								}
								else
								{
									CurFrameTotal = FrameData.Total;
								}

								CurFrameCalls = FrameData.TotalCalls;
							}
						}
					}
				}

                CurFrameTotal *= MetadataScale;
				lvi.SubItems.Add( CurFrameTotal.ToString( "F2" ) + Suffix);
				lvi.SubItems.Add( CurFrameCalls.ToString() );

				// Now add the aggregate data
                lvi.SubItems.Add((AggData.Average * MetadataScale).ToString("F2"));
                lvi.SubItems.Add((AggData.AveragePerCall * MetadataScale).ToString("F2"));

				// If the data hasn't been initialized show zero rather than Min/Max double
				if( AggData.NumStats > 0 )
				{
                    lvi.SubItems.Add((AggData.Min * MetadataScale).ToString("F2"));
                    lvi.SubItems.Add((AggData.Max * MetadataScale).ToString("F2"));
				}
				else
				{
					lvi.SubItems.Add( "0" );
					lvi.SubItems.Add( "0" );
				}

                // set checked to be true
                lvi.Checked = StatToAdd.bShowGraph;

				// Add to the list view
				StatDataList.Items.Add( lvi );
			}
		}

		/// <summary>
		/// Adds the stat to our tracked list. Also, chooses a new color for the stat
		/// </summary>
		/// <param name="StatToTrack">The stat to add to our tracked list</param>
		private bool TrackStat( Stat StatToTrack )
		{
			bool bAlreadyExists = false;
			foreach( Stat CurStat in TrackedStats )
			{
				if( CurStat.Name == StatToTrack.Name )
				{
					bAlreadyExists = true;
					break;
				}
			}

			// Don't add twice
			if( TrackedStats.Contains( StatToTrack ) == false && !bAlreadyExists )
			{
				// Add this to our list that we iterate during rendering
				TrackedStats.Add( StatToTrack );
				StatToTrack.ColorId = Colors.GetNextColorId( LastColorId );
				LastColorId = StatToTrack.ColorId;

				return true;
			}

			return false;
		}

		/// <summary>
		/// Iterates through the group's stats adding them to the list
        /// If you change this function, please refactor ResetAndAddGroupToList
		/// </summary>
		/// <param name="GroupToAdd">The group to add all the stats for</param>
		private void AddGroupToList( Group GroupToAdd )
		{
			// Add each stat in this group
			foreach( Stat stat in GroupToAdd.OwnedStats )
			{
				if( TrackStat( stat ) )
				{
					AddStatToList( stat );
				}
			}
		}

        /// <summary>
        /// Iterates through the group's stats reset their showflag, and add them
        /// </summary>
        /// <param name="GroupToAdd">The group to add all the stats for</param>
        private void ResetAndAddGroupToList(Group GroupToAdd)
        {
            // Add each stat in this group
            foreach (Stat stat in GroupToAdd.OwnedStats)
            {
                if (TrackStat(stat))
                {
                    stat.bShowGraph = true;
                    AddStatToList(stat);
                }
            }
        }
		/// <summary>
		/// Handles the drop event. If the item dropped is a stat, that stat is
		/// added. If the item is a group, all of it's items are added.
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void GraphPanelOrStatsList_DragDrop( object sender, System.Windows.Forms.DragEventArgs e )
		{
			if (e.Data.GetDataPresent(typeof(int)))
			{
				// Decode the int into two fields
				int WholeValue = (int)e.Data.GetData( typeof( int ) );
				int Type = ( WholeValue >> 24 ) & 0xFF;
				int ID = WholeValue & ~( 0xFF << 24 );

				AddStatToGraphPanelOrStatsList(Type, ID);
			}
		}

		/// <summary>
		/// Helper function to add stats/group to the graphpanel or stats list
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void AddStatToGraphPanelOrStatsList( int InType, int InID )
		{
			// Don't flicker while inserting multiple items
			StatDataList.BeginUpdate();
			// Figure out which type it is
			switch ((UIBaseElement.ElementType)InType)
			{
				// Figure out if we are adding a stat
				case UIBaseElement.ElementType.StatsObject:
					{
						// Get the stat that was referenced. Handle null
						// in case the format is busted
						Stat StatToAdd = CurrentFile.GetStat(InID);
						if (StatToAdd != null)
						{
							StatToAdd.bShowGraph = true;
							if (TrackStat(StatToAdd))
							{
								AddStatToList(StatToAdd);
							}
						}
						break;
					}
				// Or an entire group of stats
				case UIBaseElement.ElementType.GroupObject:
					{
						// Get the group that was referenced. Handle null
						// in case the format is busted
						Group GroupToAdd = CurrentFile.GetGroup(InID);
						if (GroupToAdd != null)
						{
							ResetAndAddGroupToList(GroupToAdd);
						}
						break;
					}
			}
			// Resize the stat name column
			StatDataList.Columns[0].Width = -1;

			// Force an update of the draw range
			UpdateDrawRange();

			StatsGraphPanel.Invalidate();

			// Repaint the list view
			StatDataList.EndUpdate();

			BuildTreeView();
		}

		/// <summary>
		/// Handles the drop event. If the item dropped is a stat, that stat is
		/// added. If the item is a group, all of it's items are added.
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGroupTree_DragDrop( object sender, System.Windows.Forms.DragEventArgs e )
		{
			// Don't flicker while inserting multiple items
			StatDataList.BeginUpdate();

			if( e.Data.GetDataPresent( typeof( ListView.SelectedListViewItemCollection ) ) )
			{
				// Iterate through the list of items and remove them
				foreach( ListViewItem item in StatDataList.SelectedItems )
				{
					// Get the stat that this item corresponds to
					Stat stat = (Stat)item.Tag;

					// Remove the item from the list view
					StatDataList.Items.Remove( item );

					// Remove the stat from our tracked stats
					TrackedStats.Remove( stat );
				}
			}

			// Resize the stat name column
			StatDataList.Columns[ 0 ].Width = -1;

			// Force an update of the draw range
			UpdateDrawRange();

			StatsGraphPanel.Invalidate();

			// Repaint the list view
			StatDataList.EndUpdate();

			//rebuild tree to fix bolding
			BuildTreeView();
		}



		/// <summary>
		/// Determines if the specified stat is selected in the list view
		/// </summary>
		/// <param name="stat">The stat to check</param>
		/// <returns>True if the stat is selected, false otherwise</returns>
		private bool IsStatSelected( Stat stat )
		{
			// Iterate through the list of items and remove them
			foreach( ListViewItem item in StatDataList.SelectedItems )
			{
				if( stat == item.Tag )
				{
					return true;
				}
			}
			return false;
		}

		/// <summary>
		/// Draws the bottom lines, frame #s, and times
		/// </summary>
		/// <param name="Gfx">The graphics object to draw with</param>
		private void DrawBottomMarkers( Graphics Gfx )
		{
			// Minimum distance in pixels a marker must be from the selected frame marker for the text
			// to be visible.  This enables us to hide marker text so it's not overlapping the selected
			// frame's marker text.
			int MinDistanceToSelectedFrame = 36;

			// Draw the bottom bar and tick marks
			int Y = StatsGraphScrollbar.Top - 4;
			Gfx.DrawLine( Pens.Black, 0, Y, StatsGraphPanel.Width, Y );

			// Left tick
			Gfx.DrawLine( Pens.Black, 0, Y, 0, Y - 6 );

			// Center tick
			int Center = StatsGraphPanel.Width / 2;
			Gfx.DrawLine( Pens.Black, Center, Y, Center, Y - 6 );

			// Right tick
			int Right = StatsGraphPanel.Width - 2;
			Gfx.DrawLine( Pens.Black, Right, Y, Right, Y - 6 );


			// Draw a stat every 2 (or more) pixels
			float XStepSize = Math.Max( 2.0F, (float)StatsGraphPanel.Width / (float)NumViewableFrames );

			// Selected frame tick
			int SelectedFrameCenterPos = 0;
			if( SelectedGraphFrame != -1 )
			{
				int StartX = (int)( XStepSize * (float)( SelectedGraphFrame - DrawRangeStart ) );
				int EndX = StartX + (int)XStepSize;

				SelectedFrameCenterPos = StartX + ( EndX - StartX ) / 2;

				if( SelectedFrameCenterPos > -XStepSize && SelectedFrameCenterPos <= StatsGraphPanel.Width + XStepSize )
				{
					Gfx.DrawLine( Pens.Black, SelectedFrameCenterPos, Y, SelectedFrameCenterPos, Y - 6 );
				}
			}

			// Calculate where to draw the text
			int TextY = Y - 6 - GraphFont.Height;

			// Determine the type of text to show
			switch( ShowTextOptions )
			{
				case GraphTextOptions.GRAPH_ShowFrameNums:
					{
						string Text = DrawRangeStart.ToString();

						if( Math.Abs( SelectedFrameCenterPos - 0 ) > MinDistanceToSelectedFrame )
						{
							// Draw the starting frame #
							Gfx.DrawString( Text, GraphFont, Brushes.Black, 0, TextY );
						}

						if( Math.Abs( SelectedFrameCenterPos - Center ) * 2 > MinDistanceToSelectedFrame )
						{
							// Draw the middle frame #
							Text = ( DrawRangeStart + ( ( DrawRangeEnd - DrawRangeStart ) / 2 ) ).ToString();
							SizeF StrSize = Gfx.MeasureString( Text, GraphFont );
							Gfx.DrawString( Text, GraphFont, Brushes.Black,
								Center - ( StrSize.Width / 2 ), TextY );
						}

						if( Math.Abs( SelectedFrameCenterPos - Right ) > MinDistanceToSelectedFrame )
						{
							// Draw the ending frame #
							Text = DrawRangeEnd.ToString();
							SizeF StrSize = Gfx.MeasureString( Text, GraphFont );
							Gfx.DrawString( Text, GraphFont, Brushes.Black,
								Right - StrSize.Width, TextY );
						}

						// Draw the selected frame #
						if( SelectedGraphFrame != -1 )
						{
							if( SelectedFrameCenterPos > -XStepSize && SelectedFrameCenterPos <= StatsGraphPanel.Width + XStepSize )
							{
								Text = SelectedGraphFrame.ToString();
								SizeF StrSize = Gfx.MeasureString( Text, GraphFont );
								Gfx.DrawString( Text, GraphFont, Brushes.Black,
									SelectedFrameCenterPos - StrSize.Width / 2, TextY );
							}
						}
						break;
					}
				case GraphTextOptions.GRAPH_ShowTimes:
					{
						// Draw the starting frame time
						double ElapsedTime = CurrentFile.Frames[ DrawRangeStart ].ElapsedTime / 1000.0;
						string Text = ElapsedTime.ToString( "F2" );
						Gfx.DrawString( Text, GraphFont, Brushes.Black, 0, TextY );

						// Draw the middle frame time
						ElapsedTime = CurrentFile.Frames[ ( DrawRangeStart + ( ( DrawRangeEnd - DrawRangeStart ) / 2 ) ) ].ElapsedTime / 1000.0;
						Text = ElapsedTime.ToString( "F2" );
						SizeF StrSize = Gfx.MeasureString( Text, GraphFont );
						Gfx.DrawString( Text, GraphFont, Brushes.Black, Center - ( StrSize.Width / 2 ), TextY );

						// Draw the ending frame time
						ElapsedTime = CurrentFile.Frames[ DrawRangeEnd ].ElapsedTime / 1000.0;
						Text = ElapsedTime.ToString( "F2" );
						StrSize = Gfx.MeasureString( Text, GraphFont );
						Gfx.DrawString( Text, GraphFont, Brushes.Black, Right - StrSize.Width, TextY );

						// Draw the selected frame #
						if( SelectedGraphFrame != -1 )
						{
							if( SelectedFrameCenterPos > -XStepSize && SelectedFrameCenterPos <= StatsGraphPanel.Width + XStepSize )
							{
								ElapsedTime = CurrentFile.Frames[ SelectedGraphFrame ].ElapsedTime / 1000.0;
								Text = SelectedGraphFrame.ToString( "F2" );
								StrSize = Gfx.MeasureString( Text, GraphFont );
								Gfx.DrawString( Text, GraphFont, Brushes.Black,
									SelectedFrameCenterPos - StrSize.Width / 2, TextY );
							}
						}

						break;
					}
			}
		}

        /// <summary>
        ///  Draw the scale info on the side
        /// </summary>
        void DrawMarkerSet(Graphics Gfx, int ViewportHeight, float X0, double MaxValue, bool bDrawBolded)
        {
            // Draw the lines
            Pen CurrentPen = new Pen(Color.Black, bDrawBolded ? 3.0f : 1.0f);

            Gfx.DrawLine(CurrentPen, X0, 0, X0+0, ViewportHeight);
            Gfx.DrawLine(CurrentPen, X0, 0, X0+2, 0);
            Gfx.DrawLine(CurrentPen, X0, ViewportHeight / 4, X0+6, ViewportHeight / 4);
            Gfx.DrawLine(CurrentPen, X0, ViewportHeight / 2, X0+6, ViewportHeight / 2);
            Gfx.DrawLine(CurrentPen, X0, ViewportHeight * 3 / 4, X0+6, ViewportHeight * 3 / 4);
            Gfx.DrawLine(CurrentPen, X0, ViewportHeight, X0+2, ViewportHeight);

            int MaxScaledValue = (int)(MaxValue * ScaleFactor);
            int ThreeQuarterScaledValue = MaxScaledValue * 3 / 4;
            int HalfMaxScaledValue = MaxScaledValue / 2;
            int QuarterScaledValue = MaxScaledValue / 4;

            // Draw the scale values
            Gfx.DrawString("0", GraphFont, Brushes.Black, 2, ViewportHeight - 12);
            Gfx.DrawString(QuarterScaledValue.ToString(), GraphFont, Brushes.Black, X0+2, ViewportHeight * 3 / 4 + 2);
            Gfx.DrawString(HalfMaxScaledValue.ToString(), GraphFont, Brushes.Black, X0+2, ViewportHeight / 2 + 2);
            Gfx.DrawString(ThreeQuarterScaledValue.ToString(), GraphFont, Brushes.Black, X0+2, ViewportHeight / 4 + 2);
            Gfx.DrawString(MaxScaledValue.ToString(), GraphFont, Brushes.Black, X0+2, 2);

        }

		/// <summary>
		///  Draw the scale info on the side
		/// </summary>
		/// <param name="Gfx">The graphics object to draw with</param>
		/// <param name="ViewportHeight">The height of the stats area</param>
		void DrawSideMarkers( Graphics Gfx, int ViewportHeight )
        {
            // Determine which scales should be bolded
            bool bBoldLeft = false;
            bool bBoldRight = false;
            foreach (Stat stat in TrackedStats)
            {
                if (stat.bShowGraph)
                {
                    if (IsStatSelected(stat))
                    {
                        if (stat.Metadata != null)
                        {
                            if (stat.Metadata.Units == StatMetadata.Unit.Bytes)
                            {
                                bBoldRight = true;
                            }
                            else
                            {
                                bBoldLeft = true;
                            }
                        }
                        else
                        {
                            bBoldLeft = true;
                        }
                    }                    
                }
            }

            // Draw the left (time in ms) scale
            float X0 = 0.0f;
            DrawMarkerSet(Gfx, ViewportHeight, X0, DrawMaxValue, bBoldLeft);

            // Draw the right (size in MB) scale
            float X1 = StatsGraphPanel.Width - 24.0f;
            DrawMarkerSet(Gfx, ViewportHeight, X1, DrawMaxValueMB, bBoldRight);

            // Draw a line for 33.3 ms if it is less than max scaled and max scaled isn't too big.
            int MaxScaledValue = (int)(DrawMaxValue * ScaleFactor);
            if (MaxScaledValue > 33.3 && MaxScaledValue <= 1000)
            {
                int VSyncY = (int)(ViewportHeight - (float)ViewportHeight / MaxScaledValue * 33.3);
                Gfx.DrawString("33.3", GraphFont, Brushes.Black, 2, VSyncY + 2);
                Gfx.DrawLine(Pens.Black, X0, VSyncY, X0 + 6, VSyncY);
            }
        }

		/// <summary>
		/// Draws the graph of the current set of stats
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGraphPanel_Paint( object sender, System.Windows.Forms.PaintEventArgs e )
		{
			try
			{
                YIntercepts.Clear();

				// Don't try to draw if we don't have any data
				if( CurrentFile != null && CurrentFile.Frames.Length > 0 )
				{
					// Draw a stat every 2 (or more) pixels
					float XStepSize = Math.Max( 2.0F, (float)StatsGraphPanel.Width / (float)NumViewableFrames );
					int ViewportHeight = StatsGraphScrollbar.Top - 12 - GraphFont.Height;

					// Figure out how many pixels per stat value to use
					double YScaleBase = ( (double)ViewportHeight / ( DrawMaxValue * ScaleFactor ) );
                    double YScaleBaseMB = ((double)ViewportHeight / (DrawMaxValueMB * ScaleFactor));

					if( true )
					{
						List<Point> LinePoints = new List<Point>();
						LinePoints.Capacity = ( DrawRangeEnd - DrawRangeStart ) + 1;

						// Loop through the tracked stats and get their values
						foreach( Stat stat in TrackedStats )
						{
                            if (stat.bShowGraph)
                            {
                                int CurrentX = 0;
                                float FloatCurrentX = 0.0F;

                                float YScale;
                                double MetadataScale = 1.0;
                                if (stat.Metadata != null)
                                {
                                    MetadataScale = stat.Metadata.Scale;
                                    if (stat.Metadata.Units == StatMetadata.Unit.Bytes)
                                    {
                                        YScale = (float)(YScaleBaseMB);
                                    }
                                    else
                                    {
                                        YScale = (float)(YScaleBase);
                                    }
                                }
                                else
                                {
                                    YScale = (float)(YScaleBase);
                                }
                                

                                // Get the pen to draw with for this stat. Selected stats use the bold pen
                                Pen pen = Colors.GetPen(stat.ColorId, IsStatSelected(stat));

                                // For each frame in the range
                                for (int FrameNo = DrawRangeStart; FrameNo < DrawRangeEnd; FrameNo++)
                                {
                                    // Set our X positions
                                    int StartX = (int)CurrentX;
                                    int EndX = (int)(CurrentX + XStepSize);

                                    // Get the frame that is being processing
                                    Frame CurrentFrame = CurrentFile.Frames[FrameNo];
                                    Frame NextFrame = CurrentFile.Frames[FrameNo + 1];
                                    if (CurrentFrame != null && NextFrame != null)
                                    {
                                        // Get the first & next stat's instance
                                        PerFrameStatData FirstStat = CurrentFrame.GetPerFrameStat(stat.StatId);
                                        PerFrameStatData NextStat = NextFrame.GetPerFrameStat(stat.StatId);

                                        // It's possible that we dropped a stat or it hasn't arrived yet
                                        if ((FirstStat != null) || (NextStat != null))
                                        {
                                            int StartY, EndY;

                                            // Determine the Y values for the points
                                            double FirstValue;
                                            double NextValue;
                                            if (stat.Type == Stat.StatType.STATTYPE_CycleCounter)
                                            {
                                                FirstValue = (FirstStat != null) ? FirstStat.TotalTime : 0.0;
                                                NextValue = (NextStat != null) ? NextStat.TotalTime : 0.0;
                                            }
                                            else
                                            {
                                                FirstValue = (FirstStat != null) ? FirstStat.Total : 0.0;
                                                NextValue = (NextStat != null) ? NextStat.Total : 0.0;
                                            }
                                            FirstValue *= MetadataScale;
                                            NextValue *= MetadataScale;

                                            StartY = ViewportHeight - (int)(YScale * FirstValue);
                                            EndY = ViewportHeight - (int)(YScale * NextValue);

                                            // Add the point to the y intercept list
                                            if (FrameNo == SelectedGraphFrame)
                                            {
                                                int yi = (StartY + EndY) / 2;
                                                if (!YIntercepts.ContainsKey(yi))
                                                {
                                                    YIntercepts.Add(yi, (FirstValue + NextValue) / 2.0);
                                                }
                                            }
                                            

                                            // Add the first point on the line segment if we don't have one yet for this batch
                                            if (LinePoints.Count == 0)
                                            {
                                                LinePoints.Add(new Point(StartX, StartY));
                                            }

                                            // Add the next point on the line segment
                                            LinePoints.Add(new Point(EndX, EndY));
                                        }
                                        else
                                        {
                                            // Draw last line batch
                                            if (LinePoints.Count > 0)
                                            {
                                                e.Graphics.DrawLines(pen, LinePoints.ToArray());
                                                LinePoints.Clear();
                                                LinePoints.Capacity = (DrawRangeEnd - DrawRangeStart) + 1;
                                            }
                                        }
                                    }

                                    // Move to the next X location to draw from
                                    FloatCurrentX += XStepSize;
                                    CurrentX = (int)FloatCurrentX;
                                }

                                // Draw last line batch
                                if (LinePoints.Count > 0)
                                {
                                    e.Graphics.DrawLines(pen, LinePoints.ToArray());
                                    LinePoints.Clear();
                                    LinePoints.Capacity = (DrawRangeEnd - DrawRangeStart) + 1;
                                }
                            }
						}
					}

					// Draw cursor for the currently selected frame
					{
						// Selected frame tick
						int SelectedFrameCenterPos = 0;
						if( SelectedGraphFrame != -1 )
						{
							int StartX = (int)( XStepSize * (float)( SelectedGraphFrame - DrawRangeStart ) );
							int EndX = StartX + (int)XStepSize;

							SelectedFrameCenterPos = StartX + ( EndX - StartX ) / 2;

							if( SelectedFrameCenterPos > -XStepSize && SelectedFrameCenterPos <= StatsGraphPanel.Width + XStepSize )
							{
								Brush brush = new SolidBrush( Color.Gray );
								e.Graphics.FillRectangle( brush, StartX, 0, ( EndX - StartX ) + 1, ViewportHeight );

								Pen pen = new Pen( Color.Black, 1.0f );
								e.Graphics.DrawRectangle( pen, StartX, 0, ( EndX - StartX ) + 1, ViewportHeight );
							}
						}
					}


					// Draw the frame # / time markers
					DrawBottomMarkers( e.Graphics );

					// Draw the scale info on the side
					DrawSideMarkers( e.Graphics, ViewportHeight );

				}
			}
			catch( Exception E )
			{
				Console.WriteLine( "Exception in StatsGraphPanel_Paint:\r\n" + E.ToString() );
			}
		}

		/// <summary>
		/// Handles keypresses in the list view. Used to delete stats from the list
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatDataList_KeyDown( object sender, System.Windows.Forms.KeyEventArgs e )
		{
			// If the user is trying to remove the selected stat from the list
			if( e.KeyCode == Keys.Delete || e.KeyCode == Keys.Back )
			{
				e.Handled = true;

				StatDataList.BeginUpdate();

				// Iterate through the list of items and remove them
				foreach( ListViewItem item in StatDataList.SelectedItems )
				{
					// Get the stat that this item corresponds to
					Stat stat = (Stat)item.Tag;

					// Remove the item from the list view
					StatDataList.Items.Remove( item );

					// Remove the stat from our tracked stats
					TrackedStats.Remove( stat );
				}
				StatDataList.EndUpdate();

				// Redraw the view
				StatsGraphPanel.Invalidate();
			}


			// Move the selected graph frame to the left
			if( e.KeyCode == Keys.Left )
			{
				e.Handled = true;

				if( SelectedGraphFrame != -1 )
				{
					--SelectedGraphFrame;
					if( SelectedGraphFrame < 0 )
					{
						SelectedGraphFrame = 0;
					}


					// Update the stats graph
					StatsGraphPanel.Invalidate();

					// Update the stats list
					RebuildStatsList( true );
				}
			}


			// Move the selected graph frame to the right
			if( e.KeyCode == Keys.Right )
			{
				e.Handled = true;

				if( SelectedGraphFrame != -1 )
				{
					if( CurrentFile != null )
					{
						++SelectedGraphFrame;
						if( SelectedGraphFrame >= CurrentFile.Frames.Length )
						{
							SelectedGraphFrame = CurrentFile.Frames.Length - 1;
						}

						// Update the stats graph
						StatsGraphPanel.Invalidate();

						// Update the stats list
						RebuildStatsList( true );
					}
				}
			}
		}

		/// <summary>
		/// Recalculates the draw range (which frames) for the graph display
		/// </summary>
		/// <param name="Delta">The change to apply to the range</param>
		private void UpdateDrawRange( int Delta )
		{
			if( Delta != 0 )
			{
				// Change the range of what we are drawing based upon the
				// change in scroll location. Make sure the window stays within
				// array boundaries
				int NewStart = Math.Max( DrawRangeStart + Delta, 0 );

				// Don't adjust the start until we have more data than the
				// number of frames we intend to show
				if( CurrentFile != null )
				{
					if( NewStart + NumViewableFrames < CurrentFile.Frames.Length )
					{
						DrawRangeStart = NewStart;
					}

					// Don't draw beyond the end
					DrawRangeEnd = Math.Min( DrawRangeStart + NumViewableFrames - 1,
						CurrentFile.Frames.Length - 1 );

					// Update the aggregate information for the range
					RebuildStatsList( AggDataOptions == AggregateDataOptions.AGGDATA_Ranged );
				}
			}
		}

		/// <summary>
		/// Recalculates the draw range (which frames) for the graph display
		/// </summary>
		private void UpdateDrawRange()
		{
			if( CurrentFile != null && CurrentFile.Frames.Length > 0 )
			{
				// Don't draw beyond the end
				DrawRangeEnd = Math.Min( DrawRangeStart + NumViewableFrames - 1,
					CurrentFile.Frames.Length - 1 );

				// Update the aggregate information for the range
				RebuildStatsList( AggDataOptions == AggregateDataOptions.AGGDATA_Ranged );
			}
			else
			{
				DrawRangeStart = DrawRangeEnd = 0;
			}
		}

		/// <summary>
		/// Handles scrolling through the frame data
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGraphScrollbar_Scroll( object sender, System.Windows.Forms.ScrollEventArgs e )
		{
			// Determine the amount of change that happened
			int Delta = e.NewValue - StatsGraphScrollbar.Value;
			if( Delta != 0 )
			{
				UpdateDrawRange( Delta );

				// Force an update
				StatsGraphPanel.Invalidate();
			}
		}

		/// <summary>
		/// Refreshes the display area when the user selects different stats
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatDataList_SelectedIndexChanged( object sender, System.EventArgs e )
		{
			StatsGraphPanel.Invalidate();
		}

		/// <summary>
		/// Handles the zoom in request
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ZoomIn_Click( object sender, System.EventArgs e )
		{
			// Don't scale too small
			if( ScaleFactor > 0.125 )
			{
				LastScaleFactor = ScaleFactor;
				ScaleFactor /= 2;
				StatsGraphPanel.Invalidate();
			}
		}

		/// <summary>
		/// Handles the zoom out request
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ZoomOut_Click( object sender, System.EventArgs e )
		{
			if( ScaleFactor < 32 )
			{
				LastScaleFactor = ScaleFactor;
				ScaleFactor *= 2;
				StatsGraphPanel.Invalidate();
			}
		}

		/// <summary>
		/// Brings up the color chooser so that the user can change the
		/// color of the stat that was selected
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatDataList_ItemActivate( object sender, System.EventArgs e )
		{
			// This crashed if nothing is selected
            if (StatDataList.SelectedItems.Count > 0)
            {
                ListViewItem Item = StatDataList.SelectedItems[0];
                // Get the stat from the list view
                Stat stat = (Stat)Item.Tag;
                // Get the color and set it as the color in the color dialog
                Color color = Colors.GetColor(stat.ColorId);
                ColorDialog ColorDlg = new ColorDialog();
                ColorDlg.Color = color;
                // Show the color chooser box
                if (ColorDlg.ShowDialog(this) == DialogResult.OK)
                {
                    // Replace the color with the one the user chose
                    Colors.SetColorForId(stat.ColorId, ColorDlg.Color);
                    // Force redraws
                    StatDataList.Invalidate();
                    StatsGraphPanel.Invalidate();
                }
                ColorDlg.Dispose();
            }
		}

		/// <summary>
		/// Toggles the show times flag
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ShowTimes_Click( object sender, System.EventArgs e )
		{
			ShowTextOptions = GraphTextOptions.GRAPH_ShowTimes;
			ShowTimes.Checked = true;
			ShowFrameNums.Checked = false;
			StatsGraphPanel.Invalidate();
		}

		/// <summary>
		/// Toggles the show frame numbers flag
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ShowFrameNums_Click( object sender, System.EventArgs e )
		{
			ShowTextOptions = GraphTextOptions.GRAPH_ShowFrameNums;
			ShowTimes.Checked = false;
			ShowFrameNums.Checked = true;
			StatsGraphPanel.Invalidate();
		}

		/// <summary>
		/// Called when the user double-clicks in the stats graph
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGraphPanel_DoubleClick( object sender, System.EventArgs e )
		{
			// Open the frame under the cursor in the call graph
			OpenCallGraphForFrameUnderCursor();
		}

		/// <summary>
		/// Allows the user to change the background color of the stats area
		/// </summary>
		private void OpenModalBackgroundColorSelectDialog()
		{
			ColorDialog ColorDlg = new ColorDialog();
			// Get the background color so we can change it
			ColorDlg.Color = StatsGraphPanel.BackColor;
			// Show the color chooser box
			if( ColorDlg.ShowDialog( this ) == DialogResult.OK )
			{
				// Replace the color with the one the user chose
				StatsGraphPanel.BackColor = ColorDlg.Color;
				StatsGraphPanel.Invalidate();
			}
			ColorDlg.Dispose();
		}


		/// <summary>
		/// Repopulates the list view when data and/or view preference changes
		/// </summary>
		/// <param name="bRefreshList">Whether to empty and re-add stats or not</param>
		private void RebuildStatsList( bool bRefreshList )
		{
			// Update the ranged data
			RebuildRangedAggregateData();

			if( bRefreshList )
			{
				// Stop redraws until we are done adding
				StatDataList.BeginUpdate();

				// Grab the current selection so we can try to restore it later
				IList<int> SelectedIndices = new List<int>();
				foreach( int CurIndex in StatDataList.SelectedIndices )
				{
					SelectedIndices.Add( CurIndex );
				}

				// Empty the list view
				StatDataList.Items.Clear();

				// Add each tracked stat to the view again
				foreach( Stat stat in TrackedStats )
				{
					AddStatToList( stat );
				}

				// Try to retain the current selection
				foreach( int CurIndex in SelectedIndices )
				{
					if( StatDataList.Items.Count > CurIndex )
					{
						StatDataList.Items[ CurIndex ].Selected = true;
					}
				}

				StatDataList.Invalidate();

				// Allow redraws
				StatDataList.EndUpdate();
			}
		}

		/// <summary>
		/// Changes the view mode for the aggregate data
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ViewOverallAggData_Click( object sender, System.EventArgs e )
		{
			AggregateDataOptions LastOpts = AggDataOptions;
			// Change to viewing all aggregate data
			ViewRangedAggData.Checked = false;
			ViewOverallAggData.Checked = true;
			AggDataOptions = AggregateDataOptions.AGGDATA_Overall;
			// Don't cause an update if it's the same
			if( LastOpts != AggDataOptions )
			{
				// Update the column names
				UpdateStatsDataListColumnNames();

				// Repopulate the list with the new data
				RebuildStatsList( true );
			}
		}

		/// <summary>
		/// Changes the view mode for the aggregate data
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ViewRangedAggData_Click( object sender, System.EventArgs e )
		{
			AggregateDataOptions LastOpts = AggDataOptions;
			// Change to viewing ranged aggregate data
			ViewRangedAggData.Checked = true;
			ViewOverallAggData.Checked = false;
			AggDataOptions = AggregateDataOptions.AGGDATA_Ranged;
			// Don't cause an update if it's the same
			if( LastOpts != AggDataOptions )
			{
				// Update the column names
				UpdateStatsDataListColumnNames();

				// Repopulate the list with the new data
				RebuildStatsList( true );
			}
		}

		/// <summary>
		/// Iterates through the frames within the range, updating the aggregate
		/// data for each stat that we are tracking
		/// </summary>
		private void RebuildRangedAggregateData()
		{
			// Get rid of any previous data
			StatIdToRangeAggData.Clear();
			// Don't calculate if we aren't updating ranged data
			if( AggDataOptions == AggregateDataOptions.AGGDATA_Ranged )
			{
				// For each tracked stat, go through and build a ranged set of data
				foreach( Stat stat in TrackedStats )
				{
					// Create a new aggregate object to hold the aggregate data
					AggregateStatData AggData = new AggregateStatData();
					// Insert it into our hash
					StatIdToRangeAggData.Add( stat.StatId, AggData );
					// Go through each frame in this range updating the aggregate data
					for( int Index = DrawRangeStart; Index < DrawRangeEnd; Index++ )
					{
						// Get the next frame
						Frame CurrentFrame = CurrentFile.Frames[ Index ];
						// Find the frame's set of data for this stat
						PerFrameStatData PerFrameData = CurrentFrame.GetPerFrameStat( stat.StatId );
						if( PerFrameData != null )
						{
							// Add each occurance to the aggregate
							foreach( Stat StatInstance in PerFrameData.Stats )
							{
								// Add it to our aggregate data
								AggData += StatInstance;
							}
						}
					}
				}
			}
		}

		/// <summary>
		/// Determines if the viewer is currently connected
		/// </summary>
		/// <returns>True if connected, false otherwise</returns>
		private bool IsConnected()
		{
			return Connection != null;
		}

		/// <summary>
		/// Handles disconnecting from a remote server. Prompts the user
		/// </summary>
		/// <returns>True if the user chose to disconnect, false otherwise</returns>
		private bool PerformDisconnect( bool bShouldPrompt )
		{
			bool bDisconnected = false;

			// Ask if they really want to disconnect
			if( !bShouldPrompt || 
				MessageBox.Show( this, "Are you sure you want to disconnect from your current session?",
					"Disconnect from server?", MessageBoxButtons.YesNo, MessageBoxIcon.Question ) ==
					DialogResult.Yes )
			{
				// Update the title bar
				Text = InitialWindowName;

				// Tells the server we are no longer interested in receiving packets
				Connection.SendDisconnectRequest();
				bDisconnected = true;

				// Disconnect from the server
				Connection.Disconnect();

				// Process any outstanding packets so that all the data is accounted for
				ProcessPackets();

				// Mark the connection as empty
				Connection = null;
			}
			return bDisconnected;
		}

		/// <summary>
		/// Saves the data to the specified file. Prompts the user about saving
		/// the bShouldPrompt flag is true. 
		/// </summary>
		/// <param name="bShouldPrompt"></param>
		/// <returns></returns>
		private bool PerformSave( bool bShouldPrompt )
		{
			bool bDidSave = false;
			// Default to true if we aren't prompting, otherwise default to false
			bool bShouldSave = bShouldPrompt == false;
			// Ask the user if they want to save
			if( bShouldPrompt )
			{
				bShouldSave = MessageBox.Show( this,
					"You have unsaved data. Do you want to save before continuing?",
					"Save current data?", MessageBoxButtons.YesNo,
					MessageBoxIcon.Question ) == DialogResult.Yes;
			}
			// If we should save, bring up the dialog
			if( bShouldSave )
			{
				SaveFileDialog SaveFileDlg = new SaveFileDialog();
				// Filter on our XML file extension
				SaveFileDlg.Filter = "XML Files (*.xml)|*.xml|All Files (*.*)|*.*";
				SaveFileDlg.FilterIndex = 1;
				SaveFileDlg.RestoreDirectory = true;
				// Show the dialog and only load the file if the user said ok
				if( SaveFileDlg.ShowDialog() == DialogResult.OK )
				{
					// Get the XML data stream to read from
					Stream XmlStream = SaveFileDlg.OpenFile();
					// Creates an instance of the XmlSerializer class so we can
					// write the object graph
					XmlSerializer ObjSer = new XmlSerializer( typeof( StatFile ) );
					// Create an object graph from the XML data
					ObjSer.Serialize( XmlStream, CurrentFile );
					// Don't keep the document locked
					XmlStream.Close();
					// Update the title bar
					Text = InitialWindowName + " - " + SaveFileDlg.FileName;
					SaveFileDlg.Dispose();
					bDidSave = true;
				}
			}
			return bDidSave;
		}

		/// <summary>
		/// Creates the connection dialog showing the list of available games
		/// that can be connected to. If the user chooses a connection, it
		/// closes the previous one.
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ConnectTo_Click( object sender, System.EventArgs e )
		{
			// If we are currently connected, prompt to close/save the data
			if( IsConnected() )
			{
				if( PerformDisconnect( true ) )
				{
					// Now prompt to save the data
					PerformSave( true );
				}
				else
				{
					// The user didn't want to disconnect
					return;
				}
			}

			// Now show the connection dialog
			NetworkConnectionDialog NetDlg = new NetworkConnectionDialog();
			if( NetDlg.ShowDialog( this ) == DialogResult.OK )
			{
				// Get the connection info from the dialog
				Connection = NetDlg.GetServerConnection();

				// Connect to the specified server
				ConnectToServer();
			}
			NetDlg.Dispose();
		}

		/// <summary>
		/// Closes the remote connection by telling the client to remove it
		/// from the list of interested clients
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void CloseConnection_Click( object sender, System.EventArgs e )
		{
			// If we are currently connected, prompt to close/save the data
			if( IsConnected() )
			{
				// Make sure they really meant this
				if( PerformDisconnect( true ) == true )
				{
					// OK, we're disconnected!
				}
			}
		}

		/// <summary>
		/// Creates our connection to the remote server. Creates a new file object
		/// to receive the remote data
		/// </summary>
		private void ConnectToServer()
		{
			if (Connection == null)
			{
				return;
			}
			try
			{
				// Cache the settings
				LastUsedSettings.LastIpAddress = Connection.Address.ToString();
				LastUsedSettings.LastPort = Connection.Port;

				// Clear out the UI
				EmptyUI();

				// Create the new place we'll store data
				CurrentFile = new StatFile();

				// Set up our in/out connections
				Connection.Connect();

				// Tell the server we're ready to start receiving data
				Connection.SendConnectRequest();

				// Update the title bar
				Text = InitialWindowName + " - Connecting to: " + Connection.Address.ToString() + " ...";
			}
			catch( Exception e )
			{
				string AddressString = Connection.Address.ToString();

				// Connection failed, so clear out everything so we don't crash later on
				PerformDisconnect( false );

				MessageBox.Show( "Failed to connect to server " + AddressString + "\r\nDue to: " + e.ToString() );
			}
		}

		/// <summary>
		/// Processes any pending packets by converting them into object form
		/// </summary>
		private void ProcessPackets()
		{
			if( IsConnected() && CurrentFile != null )
			{
				// Vars used for figuring out the type of updates needed
				bool bRequiresTreeRebuild = false;
				int NumPacketsProcessed = 0;
				int NumFramesAdded = 0;

				bool bHaveFirstFrame = false;

				// While there are packets to be processed
				for( Packet packet = Connection.GetNextPacket();
					packet != null;
					packet = Connection.GetNextPacket() )
				{
					NumPacketsProcessed++;

					// Get the 2 character packet code
					string PacketType = Encoding.ASCII.GetString( packet.Data, 0, 2 );

					// Route to the correct class/handler based upon packet type
					switch( PacketType )
					{
						// SD == Stat description
						case "SD":
							{
								bRequiresTreeRebuild = true;
								// Add the new stat description
								CurrentFile.AppendStatDescription( new Stat( PacketType, packet.Data ) );
								break;
							}
						// Ux == Update an existing stat
						case "UC":
						case "UD":
						case "UF":
							{
								// Create a new stat object from the update data
								CurrentFile.AppendStat( new Stat( PacketType, packet.Data ) );
								break;
							}
						// GD == Group description
						case "GD":
							{
								bRequiresTreeRebuild = true;
								// Create a new group description
								CurrentFile.AppendGroupDescription( new Group( packet.Data ) );
								break;
							}
						// PC == Sends the value of Seconds Per Cycle so that we
						// can properly convert from cycles to ms
						case "PC":
							{
								CurrentFile.UpdateConversionFactor( packet.Data );
								break;
							}
						// NF == New frame
						case "NF":
							{
								NumFramesAdded++;

								// Are we receiving the first frame?
								bHaveFirstFrame = ( CurrentFile.Frames.Length == 0 );

								// Create a new frame object that we'll add updates to
								CurrentFile.AppendFrame( new Frame( packet.Data ) );

								break;
							}
						default:
							{
								Console.WriteLine( "Unknown packet type " + PacketType );
								break;
							}
					}
				}

				// Don't do the updating of data unless needed
				if( NumPacketsProcessed > 0 )
				{
					// Fix up an items that were added
					{
						CurrentFile.RepairWarningMessages.Clear();

						CurrentFile.FixupRecentItems();

						if( CurrentFile.RepairWarningMessages.Count > 0 )
						{
							StringBuilder WarningText = new StringBuilder();
							foreach( string CurWarning in CurrentFile.RepairWarningMessages )
							{
								WarningText.AppendLine( CurWarning );
							}
							MessageBox.Show( this, WarningText.ToString(), "Stat Data Warnings" );
						}
					}


					if( bHaveFirstFrame )
					{
						// Update the title bar
						Text = InitialWindowName + " - Connected to: " + Connection.Address.ToString();

						BuildUI();
					}

					// Rebuild the tree data if new stats/groups were added
					if( bRequiresTreeRebuild )
					{
						BuildTreeView();
					}

					// Update the scrollbars and stats area if needed
					if( NumFramesAdded > 0 )
					{
						UpdateViewableFrames();

						ScrollStats();

						// If the selected frame was the last frame, then automatically scroll that too!
						int OldFrameCount = CurrentFile.Frames.Length - NumFramesAdded;
						if( SelectedGraphFrame >= OldFrameCount - 1 )
						{
							SelectedGraphFrame = CurrentFile.Frames.Length - 1;

							// Update the stats graph
							StatsGraphPanel.Invalidate();

							// Update the stats list
							RebuildStatsList( true );
						}
					}
				}
			}
		}

		/// <summary>
		/// Scrolls the scrollbar and updates the view
		/// </summary>
		private void ScrollStats()
		{
			// Set the new max
			StatsGraphScrollbar.Maximum = CurrentFile.Frames.Length - 1;
			if( bShouldAutoScroll )
			{
				int Delta = StatsGraphScrollbar.Maximum - StatsGraphScrollbar.Value - 1;

				// It was all the way to the right so auto scroll
				StatsGraphScrollbar.Value = StatsGraphScrollbar.Maximum;
				UpdateDrawRange( Delta );

				// Force an update
				StatsGraphPanel.Invalidate();
			}
		}

		/// <summary>
		/// Processes any packets that have been queued up since the last timer
		/// event
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void OnTimer( Object sender, EventArgs e )
		{
			// Stop it in case it takes a long time to process
			ProcessPacketsTimer.Stop();
			// Process any pending packets
			ProcessPackets();
			// Start the timer again
			ProcessPacketsTimer.Start();
		}

		/// <summary>
		/// Toggles the auto scrolling state
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void AutoScrollDisplay_Click( object sender, System.EventArgs e )
		{
			bShouldAutoScroll ^= true;
			AutoScrollDisplay.Checked = bShouldAutoScroll;
		}

		/// <summary>
		/// Updates the stats area when resized
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGraphPanel_Resize( object sender, System.EventArgs e )
		{
			// Update the number of viewable frames as well as the scrollbar thumb size
			UpdateViewableFrames();

			// Force update that
			UpdateDrawRange();

			// Force a redraw
			StatsGraphPanel.Invalidate();
		}

		/// <summary>
		/// Updates the view frame popup menu based upon the selected frame
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGraphContextMenu_Popup( object sender, System.EventArgs e )
		{
			// Get the menu item that needs adjusting
			MenuItem Item = StatsGraphContextMenu.MenuItems[ 0 ];
			// Make sure this was done from the stats area
			if( StatsGraphContextMenu.SourceControl == StatsGraphPanel && IsInStatsArea() )
			{
				// Get the frame number so it can be appended
				int FrameNum = GetFrameNumUnderMouse();
				// Update the item depending upon whether we have valid data
				if( CurrentFile != null &&
					CurrentFile.Frames.Length > 0 &&
					FrameNum >= 0 &&
					FrameNum < CurrentFile.Frames.Length )
				{
					Item.Enabled = true;
					Item.Text = "Open Call Graph for Frame " + FrameNum.ToString();
				}
				else
				{
					// Disable it
					Item.Enabled = false;
					Item.Text = "Open Call Graph for Frame";
				}
			}
			else
			{
				// Disable it
				Item.Enabled = false;
				Item.Text = "Open Call Graph for Frame";
			}
		}

		/// <summary>
		/// Determines if the mouse is currently within the stats graph area
		/// </summary>
		/// <returns>true if it is, false otherwise</returns>
		private bool IsInStatsArea()
		{
			// Transform the mouse position relative to the stats area
			Point LocalPos = StatsGraphPanel.PointToClient( Cursor.Position );
			int ViewportHeight = StatsGraphScrollbar.Top;
			int ViewportWidth = StatsGraphPanel.Width;
			// If it's within the width & height, then it's ok
			return LocalPos.X < ViewportWidth && LocalPos.Y < ViewportHeight;
		}

		/// <summary>
		/// Tracks the mouse position for determining which frame # the mouse
		/// is currently positioned at
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsGraphPanel_MouseMove( object sender, System.Windows.Forms.MouseEventArgs e )
		{
			// Set the stats graph panel as the frame's active control so that it can catch mouse wheel events
			this.ActiveControl = StatsGraphPanel;
			
			// Track the location of the mouse so we can show frame data
			StatsGraphMouseLocation.X = e.X;
			StatsGraphMouseLocation.Y = e.Y;

			if( bIsDraggingGraphSelection )
			{
				int NewGraphFrame = GetFrameNumUnderMouse();
				if( CurrentFile != null &&
					NewGraphFrame != SelectedGraphFrame &&
					NewGraphFrame >= 0 &&
					NewGraphFrame < CurrentFile.Frames.Length )
				{
					SelectedGraphFrame = NewGraphFrame;

					// Update the stats graph
					StatsGraphPanel.Invalidate();

					// Update the stats list
					RebuildStatsList( true );
				}
			}




            if (CurrentFile == null)
            {
                return;
            }

            // Determine the number of pixels per frame
            float XStepSize = Math.Max(2.0F, (float)StatsGraphPanel.Width /
                Math.Min((float)CurrentFile.Frames.Length, (float)NumViewableFrames));

            // Divide the X location by the step size to figure out the
            // frame number from the start
            int FrameOffset = (int)((float)(e.X) / XStepSize);

            // Clamp that frame offset
            FrameOffset = Math.Min(DrawRangeStart + FrameOffset, CurrentFile.Frames.Length);

            if (Math.Abs(FrameOffset - SelectedGraphFrame) < 2)
            {
                string Values = "";

                // Check the y value
                foreach (var Pair in YIntercepts)
                {
                    if (Math.Abs(Pair.Key - e.Y) < 5.0f)
                    {
                        if (Values != "")
                        {
                            Values += "\n";
                        }
                        Values += Pair.Value.ToString("F2");
                    }
                }

                if (Values != "")
                {
                    toolTip1.Show(Values, StatsGraphPanel, e.X, e.Y, 1000);
                }
            }

		}

		/// <summary>
		/// Determines the frame object that is beneath the mouse cursor and
		/// returns that object to the caller
		/// </summary>
		/// <returns>The frame beneath the current mouse location</returns>
		private int GetFrameNumUnderMouse()
		{
			int FrameOffset = -1;
			if( CurrentFile != null && CurrentFile.Frames.Length > 0 )
			{
				// Determine the number of pixels per frame
				float XStepSize = Math.Max( 2.0F, (float)StatsGraphPanel.Width /
					Math.Min( (float)CurrentFile.Frames.Length, (float)NumViewableFrames ) );

				// Divide the X location by the step size to figure out the
				// frame number from the start
				FrameOffset = (int)( (float)StatsGraphMouseLocation.X / XStepSize );

				// Clamp that frame offset
				FrameOffset = Math.Min( DrawRangeStart + FrameOffset, CurrentFile.Frames.Length );
			}
			return FrameOffset;
		}

		/// <summary>
		/// Opens a call graph view for the frames near the mouse
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ViewFrameNumMenuItem_Click( object sender, System.EventArgs e )
		{
			OpenCallGraphForFrameUnderCursor();
		}


		/// <summary>
		/// Opens a call graph view for the frame number under the mouse cursor
		/// </summary>
		private void OpenCallGraphForFrameUnderCursor()
		{
			// Figure out which frame we are over
			int FrameNum = GetFrameNumUnderMouse();
			if( CurrentFile != null &&
				FrameNum >= 0 &&
				FrameNum < CurrentFile.Frames.Length )
			{
				// Add the currently specified frame
				ArrayList Frames = new ArrayList();
				Frames.Add( CurrentFile.Frames[ FrameNum ] );

				// Grab the game frame number for the first frame in our data set
				int FirstFrameNumber = 0;
				if( CurrentFile.Frames.Length > 0 )
				{
					FirstFrameNumber = CurrentFile.Frames[ 0 ].FrameNumber;
				}

				// Crete the view with the set of frames
				CreateFramesView( FirstFrameNumber, Frames );
			}
		}


		/// <summary>
		/// Allows the user to choose a new background color for the stats graph
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void SelectBackgroundColorMenuItem_Click( object sender, System.EventArgs e )
		{
			OpenModalBackgroundColorSelectDialog();
		}


		/// <summary>
		/// Common function for creating a frames view from an array of frames
		/// </summary>
		/// <param name="Frames">The data to populate the view with</param>
		private void CreateFramesView( int FirstFrameNumber, ArrayList Frames )
		{
			// Create the new view with the set of frames
			PerFrameView NewView = new PerFrameView( this, FirstFrameNumber, Frames );
			NewView.Show();

			// Add to our internal list so it can be closed when the file/connection is closed
			PerFrameViews.Add( NewView );
		}

		/// <summary>
		/// Removes the specified view from the child view list
		/// </summary>
		/// <param name="View">the view to remove</param>
		public void RemovePerFrameView( PerFrameView View )
		{
			PerFrameViews.Remove( View );
		}

		/// <summary>
		/// Closes all of the per frame views and empties the list
		/// </summary>
		public void ClosePerFrameViews()
		{
			// Make a new per frame list
			ArrayList LocalList = PerFrameViews;
			PerFrameViews = new ArrayList();
			// Tell each to close down
			foreach( PerFrameView View in LocalList )
			{
				View.Close();
			}
		}

		/// <summary>
		/// Processes the context menu popup event. Enables/disables the menu as
		/// based upon whether the user selected a stat
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void StatsTreeMenu_Popup( object sender, System.EventArgs e )
		{
			bool bEnabled = false;
			// Transform the mouse position relative to the tree control
			Point LocalPos = StatGroupsTree.PointToClient( Cursor.Position );
			// Find the item that was selected
			TreeNode Node = StatGroupsTree.GetNodeAt( LocalPos );
			if( Node != null )
			{
				UIBaseElement Base = (UIBaseElement)Node.Tag;
				// Skip if not a stats object
				if( Base.UIType == UIBaseElement.ElementType.StatsObject )
				{
					bEnabled = true;
					StatGroupsTree.SelectedNode = Node;
				}
			}
			// Enable or disable the menu accordingly
			StatsTreeMenu.MenuItems[ 0 ].Enabled = bEnabled;
		}

		/// <summary>
		/// Displays the search dialog when chosen. Kicks off the search with
		/// the user specifications
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ViewFramesByCriteriaMenuItem_Click( object sender, System.EventArgs e )
		{
			// Find the item that was selected
			TreeNode Node = StatGroupsTree.SelectedNode;
			if( Node != null )
			{
				UIBaseElement Base = (UIBaseElement)Node.Tag;
				// Skip if not a stats object
				if( Base.UIType == UIBaseElement.ElementType.StatsObject )
				{
					Stat stat = (Stat)Base;
					// Create the dialog and tell it this stat
					ViewFramesByCriteriaDialog Dlg = new ViewFramesByCriteriaDialog( stat.Name );
					if( Dlg.ShowDialog( this ) == DialogResult.OK )
					{
						// Open a set of frames that match the search criteria
						DisplaySearchResults( stat, Dlg.GetSearchType(),
							Dlg.GetSearchValue() );
					}
				}
			}
		}

		/// <summary>
		/// Opens a tree view with a set of frames from a search
		/// </summary>
		/// <param name="stat">The stat being searched for</param>
		/// <param name="SearchType">The criteria to search with</param>
		/// <param name="Value">The value to compare against</param>
		private void DisplaySearchResults( Stat stat, SearchByType SearchType,double Value )
		{
			ArrayList Frames = new ArrayList();

			// Grab the game frame number for the first frame in our data set
			int FirstFrameNumber = 0;
			if( CurrentFile.Frames.Length > 0 )
			{
				FirstFrameNumber = CurrentFile.Frames[ 0 ].FrameNumber;
			}
			
			// Iterate over the set of frames capturing ones that match
			foreach( Frame frame in CurrentFile.Frames )
			{
				if( DoesFrameMatchSearchCriteria( frame, stat, SearchType, Value ) )
				{
					Frames.Add( frame );
				}

			}

			// Show the built data
			CreateFramesView( FirstFrameNumber, Frames );
		}

		/// <summary>
		/// Builds an aggregate of the frame's data for a specific stat and then
		/// compares the results against the search criteria
		/// </summary>
		/// <param name="frame">The frame to process</param>
		/// <param name="stat">The stat to search for</param>
		/// <param name="SearchType">The search matching option</param>
		/// <param name="Value">The value to match</param>
		/// <returns>true if this frame is a match, false otherwise</returns>
		bool DoesFrameMatchSearchCriteria( Frame frame, Stat stat,SearchByType SearchType,double Value )
		{
			bool bMatches = false;
			// Check the per frame data for the stat
			PerFrameStatData PerFrameData = frame.GetPerFrameStat( stat.StatId );
			if( PerFrameData != null )
			{
				double StatValue = 0.0;
				// Figure out if it is a counter or not
				if( stat.Type == Stat.StatType.STATTYPE_CycleCounter )
				{
					StatValue = PerFrameData.TotalTime;
				}
				else
				{
					StatValue = PerFrameData.Total;
				}
				// Now do the compare based upon the search type
				if( SearchType == SearchByType.GreaterThan )
				{
					bMatches = ( StatValue > Value );
				}
				else if( SearchType == SearchByType.GreaterThan )
				{
					bMatches = ( StatValue < Value );
				}
				else
				{
					bMatches = ( StatValue == Value );
				}
			}
			return bMatches;
		}

		/// <summary>
		/// Displays a dialog for entering an IP address and connects to it
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ConnectToIP_Click( object sender, System.EventArgs e )
		{
			// If we are currently connected, prompt to close/save the data
			if( IsConnected() )
			{
                if( PerformDisconnect( true ) )
				{
					// Now prompt to save the data
					PerformSave( true );
				}
				else
				{
					// The user didn't want to disconnect
					return;
				}
			}
			// Now show the connection dialog
			ConnectToIPDialog ConnectToDlg =
				new ConnectToIPDialog(
					LastUsedSettings.LastIpAddress,
					LastUsedSettings.LastPort.ToString() );
			if( ConnectToDlg.ShowDialog( this ) == DialogResult.OK )
			{
				// Get the connection info from the dialog
				Connection = ConnectToDlg.GetServerConnection();

				// Connect to the specified server
				ConnectToServer();
			}
			ConnectToDlg.Dispose();
		}

		/// <summary>
		/// Logs the node information for debugging purposes
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e">The info about the bad node type</param>
		protected void XmlSerializer_UnknownNode( object sender, XmlNodeEventArgs e )
		{
			Console.WriteLine( "Unknown Node:" + e.Name + "\t" + e.Text );
		}

		/// <summary>
		/// Logs the bad attribute information for debugging purposes
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e">The attribute info</param>
		protected void XmlSerializer_UnknownAttribute( object sender, XmlAttributeEventArgs e )
		{
			System.Xml.XmlAttribute attr = e.Attr;
			Console.WriteLine( "Unknown attribute " + attr.Name + "='" + attr.Value + "'" );
		}

		/// <summary>
		/// Reads the last used application settings from the XML file
		/// </summary>
		/// <returns>true if there were valid settings, false otherwise</returns>
		bool LoadUserSettings()
		{
            // Load the metadata and presets
            string BaseDirectory = Path.GetDirectoryName(Application.ExecutablePath);
            string MetadataFilename = Path.Combine(BaseDirectory, "StatsViewerMetadata.xml");
            PresetViewsFilename = Path.Combine(BaseDirectory, "StatsViewerPresets.xml");

            StatMetadataManager.Initialize();
            StatMetadataManager.LoadSettings(MetadataFilename);
            StatMetadataManager.LoadSettings(PresetViewsFilename);

            Stream XmlStream = null;
			try
			{
				string Name = Application.ExecutablePath.Substring( 0, Application.ExecutablePath.Length - 4 );
				Name += "UserSettings.xml";
				
				// Get the XML data stream to read from
				XmlStream = new FileStream( Name, FileMode.Open,
					FileAccess.Read, FileShare.None, 256 * 1024, false );
				
				// Creates an instance of the XmlSerializer class so we can
				// read the settings object
				XmlSerializer ObjSer = new XmlSerializer( typeof( SavedSettings ) );
				
				// Add our callbacks for a busted XML file
				ObjSer.UnknownNode += new XmlNodeEventHandler( XmlSerializer_UnknownNode );
				ObjSer.UnknownAttribute += new XmlAttributeEventHandler( XmlSerializer_UnknownAttribute );
				
				// Create an object graph from the XML data
				LastUsedSettings = (SavedSettings)ObjSer.Deserialize( XmlStream );
				if( LastUsedSettings == null )
				{
					LastUsedSettings = new SavedSettings();
				}

				if (Settings.Default.LastLocation != null)
				{
					Location = Settings.Default.LastLocation;
				}

				if (Settings.Default.LastSize != null)
				{
					Size = Settings.Default.LastSize;
				}
			}
			catch( Exception e )
			{
				Console.WriteLine( e.ToString() );
				return false;
			}
			finally
			{
				if( XmlStream != null )
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}

            return true;
		}

		/// <summary>
		/// Saves the current application settings to the XML file
		/// </summary>
		private void SaveUserSettings()
		{
			Stream XmlStream = null;
			try
			{
				//make sure to save any values that might be invalid (due to full screen, etc)
				if (WindowState == FormWindowState.Normal)
				{
					Settings.Default.LastLocation = Location;
					Settings.Default.LastSize = Size;
				}
				else
				{
					Settings.Default.LastLocation = RestoreBounds.Location;
					Settings.Default.LastSize = RestoreBounds.Size;
				}


				Settings.Default.Save();

				string Name = Application.ExecutablePath.Substring( 0, Application.ExecutablePath.Length - 4 );
				Name += "UserSettings.xml";
				
				// Get the XML data stream to read from
				XmlStream = new FileStream( Name, FileMode.Create,
					FileAccess.Write, FileShare.None, 256 * 1024, false );
				
				// Creates an instance of the XmlSerializer class so we can
				// save the settings object
				XmlSerializer ObjSer = new XmlSerializer( typeof( SavedSettings ) );
				
				// Write the object graph as XML data
				ObjSer.Serialize( XmlStream, LastUsedSettings );
			}
			catch( Exception e )
			{
				Console.WriteLine( e.ToString() );
			}
			finally
			{
				if( XmlStream != null )
				{
					// Done with the file so close it
					XmlStream.Close();
				}
			}
		}

		private void StatDataList_ColumnClick( object sender, ColumnClickEventArgs e )
		{
			// Determine if clicked column is already the column that is being sorted.
			if( e.Column == StatDataListColumnSorter.SortColumn )
			{
				// Reverse the current sort direction for this column.
				if( StatDataListColumnSorter.Order == SortOrder.Ascending )
				{
					StatDataListColumnSorter.Order = SortOrder.Descending;
				}
				else
				{
					StatDataListColumnSorter.Order = SortOrder.Ascending;
				}
			}
			else
			{
				// Set the column number that is to be sorted; default to ascending.
				StatDataListColumnSorter.SortColumn = e.Column;
				StatDataListColumnSorter.Order = SortOrder.Ascending;
			}

			// Perform the sort with these new sort options.
			this.StatDataList.Sort();
		}


		private void StatsGraphPanel_Click( object sender, EventArgs e )
		{
			// We're done dragging if we were doing that
			if( bIsDraggingGraphSelection )
			{
				bIsDraggingGraphSelection = false;
			}

			// User clicked, so update the selected frame immediately
			int NewGraphFrame = GetFrameNumUnderMouse();
			if( CurrentFile != null &&
				NewGraphFrame != SelectedGraphFrame &&
				NewGraphFrame >= 0 &&
				NewGraphFrame < CurrentFile.Frames.Length )
			{
				SelectedGraphFrame = NewGraphFrame;

				// Update the stats graph
				StatsGraphPanel.Invalidate();

				// Update the stats list
				RebuildStatsList( true );
			}
		}

		private void StatsGraphPanel_MouseDown( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Left )
			{
				// Start changing the currently selected graph frame by dragging the mouse
				bIsDraggingGraphSelection = true;

				// Update the selected frame immediately
				int NewGraphFrame = GetFrameNumUnderMouse();
				if( CurrentFile != null &&
					NewGraphFrame != SelectedGraphFrame &&
					NewGraphFrame >= 0 &&
					NewGraphFrame < CurrentFile.Frames.Length )
				{
					SelectedGraphFrame = NewGraphFrame;

					// Update the stats graph
					StatsGraphPanel.Invalidate();

					// Update the stats list
					RebuildStatsList( true );
				}
			}
		}

		private void StatsGraphPanel_MouseUp( object sender, MouseEventArgs e )
		{
			if( e.Button == MouseButtons.Left )
			{
				// We're done dragging if we were doing that
				if( bIsDraggingGraphSelection )
				{
					bIsDraggingGraphSelection = false;
				}

				// User clicked, so update the selected frame immediately
				int NewGraphFrame = GetFrameNumUnderMouse();
				if( CurrentFile != null &&
					NewGraphFrame != SelectedGraphFrame &&
					NewGraphFrame >= 0 &&
					NewGraphFrame < CurrentFile.Frames.Length )
				{
					SelectedGraphFrame = NewGraphFrame;

					// Update the stats graph
					StatsGraphPanel.Invalidate();

					// Update the stats list
					RebuildStatsList( true );
				}
			}
		}


		private void StatsGraphPanel_MouseWheel( object sender, MouseEventArgs e )
		{
			bool bRefreshView = false;

			if( e.Delta < 0 )
			{
				// Zoom out vertically
				if( ScaleFactor < 32 )
				{
					LastScaleFactor = ScaleFactor;
					ScaleFactor *= 2;
					bRefreshView = true;
				}
			}
			else if( e.Delta > 0 )
			{
				// Zoom in vertically
				if( ScaleFactor > 0.125 )
				{
					LastScaleFactor = ScaleFactor;
					ScaleFactor /= 2;
					bRefreshView = true;
				}
			}

			if( bRefreshView )
			{
				// Update the stats graph
				StatsGraphPanel.Invalidate();
			}
		}

		
		private void StatDataList_ItemDrag( object sender, ItemDragEventArgs e )
		{
			if( StatDataList.SelectedItems.Count > 0 )
			{
				// Kick off drag and drop
				StatDataList.DoDragDrop( StatDataList.SelectedItems, DragDropEffects.Link );
			}
		}

        private void StatDataList_ItemChecked( object sender,	ItemCheckedEventArgs e )
        {
            // Get the stat from the list view
            Stat stat = (Stat)e.Item.Tag;
            stat.bShowGraph = e.Item.Checked;

            // Update the stats graph
            StatsGraphPanel.Invalidate();
        }

        private void StatDataList_DrawSubItem ( object sender, DrawListViewSubItemEventArgs e )
        {
            // Unless the item is selected, draw the standard 
            // background to make it stand out from the gradient.
            if ( e.ColumnIndex == 1 )
            {
                ListViewItem Item = StatDataList.Items[e.ItemIndex];
                Stat stat = (Stat)Item.Tag;
                e.SubItem.BackColor = Colors.GetColor(stat.ColorId);
                e.DrawBackground();
            }
            else
            {
                e.DrawDefault = true;
            }
        }

        private void StatDataList_DrawColumnHeader(object sender, DrawListViewColumnHeaderEventArgs e)
        {
            e.DrawDefault = true;
        }

		private void StatsGroupTree_DoubleClick(object sender, MouseEventArgs e)
		{
			// Get the node so we can get the stat/group data out of it
			TreeView EventTreeView = (TreeView)sender;
			TreeNode Item = EventTreeView.SelectedNode;

			// Root node doesn't have an object associated with it
			if ((Item != null) && (Item.Tag != null))
			{
				UIBaseElement Base = (UIBaseElement)Item.Tag;

				// Handle building the value based upon the type
				if (Base.UIType == UIBaseElement.ElementType.StatsObject)
				{
					Stat stat = (Stat)Base;
					int Type = (int)UIBaseElement.ElementType.StatsObject;
					int ID = stat.StatId;
					AddStatToGraphPanelOrStatsList(Type, ID);
				}
				else if (Base.UIType == UIBaseElement.ElementType.GroupObject)
				{
					//counter-act automatic expansion on double click
					if (Item.IsExpanded)
					{
						Item.Collapse();
					}
					else
					{
						Item.Expand();
					}

					Group group = (Group)Base;
					int Type = (int)UIBaseElement.ElementType.GroupObject;
					int ID = group.GroupId;
					AddStatToGraphPanelOrStatsList(Type, ID);
				}
			}
		}

		private void FilterBox_TextChanged(object sender, EventArgs e)
		{
			TextBox SenderAsTextBox = (TextBox)sender;

			if (SenderAsTextBox.Text.Length > 0)
			{
				string LowerCaseString = SenderAsTextBox.Text.ToLower();
				FilterStrings = LowerCaseString.Split(' ');
			}
			else
			{
				FilterStrings = new string[0];
			}

			BuildTreeView();
		}

		private void ClearFilterButton_Click(object sender, EventArgs e)
		{
			FilterBox.Text = "";
		}

		private void StatsViewerFrame_FormClosing(object sender, FormClosingEventArgs e)
		{
			SaveUserSettings();
		}

        private void StatsGraphPanel_MouseHover(object sender, EventArgs e)
        {

        }

        private void CopyLocationToClipboardMI_Click(object sender, EventArgs e)
        {
            try
            {
                Frame CurrentFrame = CurrentFile.Frames[SelectedGraphFrame];

                string BugItGo = String.Format("BugItGo {0} {1} {2} {3} {4} {5}",
                    CurrentFrame.ViewLocationX, CurrentFrame.ViewLocationY, CurrentFrame.ViewLocationZ,
                    CurrentFrame.ViewRotationPitch, CurrentFrame.ViewRotationYaw, CurrentFrame.ViewRotationRoll);

                Clipboard.SetText(BugItGo);
            }
            catch (Exception)
            {
                Console.Beep();
            }
        }

		private void StatsViewerFrame_DragDrop(object sender, DragEventArgs e)
		{
			string[] Files = (string[]) e.Data.GetData(DataFormats.FileDrop, false);
			if (Files.Length > 0)
			{
				OpenFile(Files[0]);
			}
		}

		//Allow drag drop of files
		private void StatsViewerFrame_DragEnter(object sender, DragEventArgs e)
		{
			if (e.Data.GetDataPresent(DataFormats.FileDrop))
				e.Effect = DragDropEffects.All;
			else
				e.Effect = DragDropEffects.None;
		}
	}
}
