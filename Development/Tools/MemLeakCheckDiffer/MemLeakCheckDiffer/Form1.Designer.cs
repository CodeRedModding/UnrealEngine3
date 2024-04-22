namespace MemLeakDiffer
{
    partial class MemDiffer
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            System.Windows.Forms.DataVisualization.Charting.ChartArea chartArea1 = new System.Windows.Forms.DataVisualization.Charting.ChartArea();
            System.Windows.Forms.DataVisualization.Charting.Title title1 = new System.Windows.Forms.DataVisualization.Charting.Title();
            System.Windows.Forms.TreeNode treeNode1 = new System.Windows.Forms.TreeNode("TitleFreeKB");
            System.Windows.Forms.TreeNode treeNode2 = new System.Windows.Forms.TreeNode("LowestRecentTitleFreeKB");
            System.Windows.Forms.TreeNode treeNode3 = new System.Windows.Forms.TreeNode("AllocUnusedKB");
            System.Windows.Forms.TreeNode treeNode4 = new System.Windows.Forms.TreeNode("AllocPureOverheadKB");
            System.Windows.Forms.TreeNode treeNode5 = new System.Windows.Forms.TreeNode("AllocUsedKB");
            System.Windows.Forms.TreeNode treeNode6 = new System.Windows.Forms.TreeNode("TaskResidentKB (iDevice)");
            System.Windows.Forms.TreeNode treeNode7 = new System.Windows.Forms.TreeNode("TaskVirtualKB (iDevice)");
            System.Windows.Forms.TreeNode treeNode8 = new System.Windows.Forms.TreeNode("PhysicalFreeKB (iDevice)");
            System.Windows.Forms.TreeNode treeNode9 = new System.Windows.Forms.TreeNode("PhysicalUsedMemKB (iDevice)");
            System.Windows.Forms.TreeNode treeNode10 = new System.Windows.Forms.TreeNode("HighestMemRecentlyKB (iDevice)");
            System.Windows.Forms.TreeNode treeNode11 = new System.Windows.Forms.TreeNode("HighestMemEverKB (iDevice)");
            System.Windows.Forms.TreeNode treeNode12 = new System.Windows.Forms.TreeNode("System Memory", new System.Windows.Forms.TreeNode[] {
            treeNode1,
            treeNode2,
            treeNode3,
            treeNode4,
            treeNode5,
            treeNode6,
            treeNode7,
            treeNode8,
            treeNode9,
            treeNode10,
            treeNode11});
            System.Windows.Forms.TreeNode treeNode13 = new System.Windows.Forms.TreeNode("Current Pool");
            System.Windows.Forms.TreeNode treeNode14 = new System.Windows.Forms.TreeNode("Target Pool");
            System.Windows.Forms.TreeNode treeNode15 = new System.Windows.Forms.TreeNode("Over Budget");
            System.Windows.Forms.TreeNode treeNode16 = new System.Windows.Forms.TreeNode("Used");
            System.Windows.Forms.TreeNode treeNode17 = new System.Windows.Forms.TreeNode("Largest Hole");
            System.Windows.Forms.TreeNode treeNode18 = new System.Windows.Forms.TreeNode("Texture Memory", new System.Windows.Forms.TreeNode[] {
            treeNode13,
            treeNode14,
            treeNode15,
            treeNode16,
            treeNode17});
            System.Windows.Forms.DataVisualization.Charting.ChartArea chartArea2 = new System.Windows.Forms.DataVisualization.Charting.ChartArea();
            System.Windows.Forms.DataVisualization.Charting.Title title2 = new System.Windows.Forms.DataVisualization.Charting.Title();
            this.openFileDialog1 = new System.Windows.Forms.OpenFileDialog();
            this.saveFileDialog1 = new System.Windows.Forms.SaveFileDialog();
            this.menuStrip1 = new System.Windows.Forms.MenuStrip();
            this.fileToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.openMemLeakFilesInAFolderToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
            this.ExitToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.reportToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.generateOverallReportToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.toolStripMenuItem1 = new System.Windows.Forms.ToolStripSeparator();
            this.generateDiffReportForAllSelectedItemsToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.runMemExperimentToolStripMenuItem = new System.Windows.Forms.ToolStripMenuItem();
            this.folderBrowserDialog1 = new System.Windows.Forms.FolderBrowserDialog();
            this.splitContainer1 = new System.Windows.Forms.SplitContainer();
            this.TabView = new System.Windows.Forms.TabControl();
            this.InfoPage = new System.Windows.Forms.TabPage();
            this.AnalyzerPage = new System.Windows.Forms.TabPage();
            this.ObjectListView = new System.Windows.Forms.ListView();
            this.columnHeader8 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader9 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader10 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader11 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.GroupListView = new System.Windows.Forms.ListView();
            this.columnHeader4 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader6 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader7 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.StreamingLevelsListView = new System.Windows.Forms.ListView();
            this.columnHeader2 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.columnHeader3 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.TextureMemoryChart = new System.Windows.Forms.DataVisualization.Charting.Chart();
            this.DisplayedCurvesTreeView = new System.Windows.Forms.TreeView();
            this.SystemMemoryChart = new System.Windows.Forms.DataVisualization.Charting.Chart();
            this.panel1 = new System.Windows.Forms.Panel();
            this.label3 = new System.Windows.Forms.Label();
            this.PS3MemoryAdjuster = new System.Windows.Forms.TextBox();
            this.CustomGroupingDisplayLabel = new System.Windows.Forms.Label();
            this.cbOverrideGroupings = new System.Windows.Forms.CheckBox();
            this.label2 = new System.Windows.Forms.Label();
            this.GameSelectionBox = new System.Windows.Forms.ComboBox();
            this.cbInMB = new System.Windows.Forms.CheckBox();
            this.GenerateSummaryReportButton = new System.Windows.Forms.Button();
            this.ClearListButton = new System.Windows.Forms.Button();
            this.cbUseGoodSizes = new System.Windows.Forms.CheckBox();
            this.label1 = new System.Windows.Forms.Label();
            this.sigSizeKB_TextBox = new System.Windows.Forms.TextBox();
            this.PickFileButton = new System.Windows.Forms.Button();
            this.panel2 = new System.Windows.Forms.Panel();
            this.FileListTree = new System.Windows.Forms.TreeView();
            this.splitContainer2 = new System.Windows.Forms.SplitContainer();
            this.splitContainer3 = new System.Windows.Forms.SplitContainer();
            this.splitContainer4 = new System.Windows.Forms.SplitContainer();
            this.InfoView = new MemLeakDiffer.BufferedListView();
            this.columnHeader1 = ((System.Windows.Forms.ColumnHeader)(new System.Windows.Forms.ColumnHeader()));
            this.menuStrip1.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).BeginInit();
            this.splitContainer1.Panel1.SuspendLayout();
            this.splitContainer1.Panel2.SuspendLayout();
            this.splitContainer1.SuspendLayout();
            this.TabView.SuspendLayout();
            this.InfoPage.SuspendLayout();
            this.AnalyzerPage.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.TextureMemoryChart)).BeginInit();
            ((System.ComponentModel.ISupportInitialize)(this.SystemMemoryChart)).BeginInit();
            this.panel1.SuspendLayout();
            this.panel2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).BeginInit();
            this.splitContainer2.Panel1.SuspendLayout();
            this.splitContainer2.Panel2.SuspendLayout();
            this.splitContainer2.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer3)).BeginInit();
            this.splitContainer3.Panel1.SuspendLayout();
            this.splitContainer3.Panel2.SuspendLayout();
            this.splitContainer3.SuspendLayout();
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer4)).BeginInit();
            this.splitContainer4.Panel1.SuspendLayout();
            this.splitContainer4.Panel2.SuspendLayout();
            this.splitContainer4.SuspendLayout();
            this.SuspendLayout();
            // 
            // openFileDialog1
            // 
            this.openFileDialog1.FileName = "openFileDialog1";
            this.openFileDialog1.Filter = "Memory Leak Reports (*.memlk)|*.memlk|All Files (*.*)|*.*";
            // 
            // menuStrip1
            // 
            this.menuStrip1.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.fileToolStripMenuItem,
            this.reportToolStripMenuItem});
            this.menuStrip1.Location = new System.Drawing.Point(0, 0);
            this.menuStrip1.Name = "menuStrip1";
            this.menuStrip1.Size = new System.Drawing.Size(1409, 24);
            this.menuStrip1.TabIndex = 10;
            this.menuStrip1.Text = "menuStrip1";
            // 
            // fileToolStripMenuItem
            // 
            this.fileToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem,
            this.openMemLeakFilesInAFolderToolStripMenuItem,
            this.toolStripSeparator1,
            this.ExitToolStripMenuItem});
            this.fileToolStripMenuItem.Name = "fileToolStripMenuItem";
            this.fileToolStripMenuItem.Size = new System.Drawing.Size(37, 20);
            this.fileToolStripMenuItem.Text = "File";
            // 
            // openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem
            // 
            this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem.Name = "openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem";
            this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem.Size = new System.Drawing.Size(227, 22);
            this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem.Text = "Open folder and subfolders...";
            this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem.Click += new System.EventHandler(this.openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem_Click);
            // 
            // openMemLeakFilesInAFolderToolStripMenuItem
            // 
            this.openMemLeakFilesInAFolderToolStripMenuItem.Name = "openMemLeakFilesInAFolderToolStripMenuItem";
            this.openMemLeakFilesInAFolderToolStripMenuItem.Size = new System.Drawing.Size(227, 22);
            this.openMemLeakFilesInAFolderToolStripMenuItem.Text = "Open folder...";
            this.openMemLeakFilesInAFolderToolStripMenuItem.Click += new System.EventHandler(this.PickFileButton_Click);
            // 
            // toolStripSeparator1
            // 
            this.toolStripSeparator1.Name = "toolStripSeparator1";
            this.toolStripSeparator1.Size = new System.Drawing.Size(224, 6);
            // 
            // ExitToolStripMenuItem
            // 
            this.ExitToolStripMenuItem.Name = "ExitToolStripMenuItem";
            this.ExitToolStripMenuItem.Size = new System.Drawing.Size(227, 22);
            this.ExitToolStripMenuItem.Text = "E&xit";
            this.ExitToolStripMenuItem.Click += new System.EventHandler(this.ExitToolStripMenuItem_Click);
            // 
            // reportToolStripMenuItem
            // 
            this.reportToolStripMenuItem.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.generateOverallReportToolStripMenuItem,
            this.toolStripMenuItem1,
            this.generateDiffReportForAllSelectedItemsToolStripMenuItem,
            this.runMemExperimentToolStripMenuItem});
            this.reportToolStripMenuItem.Name = "reportToolStripMenuItem";
            this.reportToolStripMenuItem.Size = new System.Drawing.Size(54, 20);
            this.reportToolStripMenuItem.Text = "Report";
            // 
            // generateOverallReportToolStripMenuItem
            // 
            this.generateOverallReportToolStripMenuItem.Name = "generateOverallReportToolStripMenuItem";
            this.generateOverallReportToolStripMenuItem.Size = new System.Drawing.Size(303, 22);
            this.generateOverallReportToolStripMenuItem.Text = "Generate Overall Report";
            this.generateOverallReportToolStripMenuItem.Click += new System.EventHandler(this.GenerateSummaryReportButton_Click);
            // 
            // toolStripMenuItem1
            // 
            this.toolStripMenuItem1.Name = "toolStripMenuItem1";
            this.toolStripMenuItem1.Size = new System.Drawing.Size(300, 6);
            // 
            // generateDiffReportForAllSelectedItemsToolStripMenuItem
            // 
            this.generateDiffReportForAllSelectedItemsToolStripMenuItem.Name = "generateDiffReportForAllSelectedItemsToolStripMenuItem";
            this.generateDiffReportForAllSelectedItemsToolStripMenuItem.Size = new System.Drawing.Size(303, 22);
            this.generateDiffReportForAllSelectedItemsToolStripMenuItem.Text = "Generate single report (All Selected Items)...";
            this.generateDiffReportForAllSelectedItemsToolStripMenuItem.Click += new System.EventHandler(this.CreateDiffReportButton_Click);
            // 
            // runMemExperimentToolStripMenuItem
            // 
            this.runMemExperimentToolStripMenuItem.Name = "runMemExperimentToolStripMenuItem";
            this.runMemExperimentToolStripMenuItem.Size = new System.Drawing.Size(303, 22);
            this.runMemExperimentToolStripMenuItem.Text = "Run Mem Experiment";
            this.runMemExperimentToolStripMenuItem.Click += new System.EventHandler(this.RunMemExperimentButton_Click);
            // 
            // splitContainer1
            // 
            this.splitContainer1.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer1.FixedPanel = System.Windows.Forms.FixedPanel.Panel1;
            this.splitContainer1.Location = new System.Drawing.Point(0, 24);
            this.splitContainer1.MinimumSize = new System.Drawing.Size(600, 400);
            this.splitContainer1.Name = "splitContainer1";
            // 
            // splitContainer1.Panel1
            // 
            this.splitContainer1.Panel1.Controls.Add(this.panel2);
            this.splitContainer1.Panel1.Controls.Add(this.panel1);
            this.splitContainer1.Panel1MinSize = 329;
            // 
            // splitContainer1.Panel2
            // 
            this.splitContainer1.Panel2.Controls.Add(this.TabView);
            this.splitContainer1.Panel2MinSize = 100;
            this.splitContainer1.Size = new System.Drawing.Size(1409, 729);
            this.splitContainer1.SplitterDistance = 329;
            this.splitContainer1.TabIndex = 20;
            // 
            // TabView
            // 
            this.TabView.Controls.Add(this.InfoPage);
            this.TabView.Controls.Add(this.AnalyzerPage);
            this.TabView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.TabView.HotTrack = true;
            this.TabView.Location = new System.Drawing.Point(0, 0);
            this.TabView.Name = "TabView";
            this.TabView.SelectedIndex = 0;
            this.TabView.Size = new System.Drawing.Size(1076, 729);
            this.TabView.TabIndex = 18;
            this.TabView.KeyDown += new System.Windows.Forms.KeyEventHandler(this.TabView_KeyDown);
            // 
            // InfoPage
            // 
            this.InfoPage.Controls.Add(this.InfoView);
            this.InfoPage.Location = new System.Drawing.Point(4, 22);
            this.InfoPage.Name = "InfoPage";
            this.InfoPage.Padding = new System.Windows.Forms.Padding(3);
            this.InfoPage.Size = new System.Drawing.Size(1068, 703);
            this.InfoPage.TabIndex = 0;
            this.InfoPage.Text = "Info";
            this.InfoPage.UseVisualStyleBackColor = true;
            // 
            // AnalyzerPage
            // 
            this.AnalyzerPage.Controls.Add(this.splitContainer2);
            this.AnalyzerPage.Controls.Add(this.splitContainer4);
            this.AnalyzerPage.Location = new System.Drawing.Point(4, 22);
            this.AnalyzerPage.Name = "AnalyzerPage";
            this.AnalyzerPage.Padding = new System.Windows.Forms.Padding(3);
            this.AnalyzerPage.Size = new System.Drawing.Size(1068, 703);
            this.AnalyzerPage.TabIndex = 1;
            this.AnalyzerPage.Text = "Analyzer";
            this.AnalyzerPage.UseVisualStyleBackColor = true;
            // 
            // ObjectListView
            // 
            this.ObjectListView.BackColor = System.Drawing.SystemColors.ControlLight;
            this.ObjectListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader8,
            this.columnHeader9,
            this.columnHeader10,
            this.columnHeader11});
            this.ObjectListView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.ObjectListView.FullRowSelect = true;
            this.ObjectListView.GridLines = true;
            this.ObjectListView.Location = new System.Drawing.Point(0, 0);
            this.ObjectListView.MultiSelect = false;
            this.ObjectListView.Name = "ObjectListView";
            this.ObjectListView.ShowGroups = false;
            this.ObjectListView.Size = new System.Drawing.Size(348, 205);
            this.ObjectListView.TabIndex = 7;
            this.ObjectListView.UseCompatibleStateImageBehavior = false;
            this.ObjectListView.View = System.Windows.Forms.View.Details;
            // 
            // columnHeader8
            // 
            this.columnHeader8.Text = "Object";
            this.columnHeader8.Width = 156;
            // 
            // columnHeader9
            // 
            this.columnHeader9.Text = "Count";
            this.columnHeader9.Width = 54;
            // 
            // columnHeader10
            // 
            this.columnHeader10.Text = "Size (kB)";
            this.columnHeader10.Width = 80;
            // 
            // columnHeader11
            // 
            this.columnHeader11.Text = "ResSize (kB)";
            this.columnHeader11.Width = 80;
            // 
            // GroupListView
            // 
            this.GroupListView.BackColor = System.Drawing.SystemColors.ControlLight;
            this.GroupListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader4,
            this.columnHeader6,
            this.columnHeader7});
            this.GroupListView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.GroupListView.FullRowSelect = true;
            this.GroupListView.GridLines = true;
            this.GroupListView.HideSelection = false;
            this.GroupListView.Location = new System.Drawing.Point(0, 0);
            this.GroupListView.Name = "GroupListView";
            this.GroupListView.ShowGroups = false;
            this.GroupListView.Size = new System.Drawing.Size(352, 205);
            this.GroupListView.TabIndex = 6;
            this.GroupListView.UseCompatibleStateImageBehavior = false;
            this.GroupListView.View = System.Windows.Forms.View.Details;
            this.GroupListView.ItemSelectionChanged += new System.Windows.Forms.ListViewItemSelectionChangedEventHandler(this.GroupListView_ItemSelectionChanged);
            // 
            // columnHeader4
            // 
            this.columnHeader4.Text = "Group";
            this.columnHeader4.Width = 156;
            // 
            // columnHeader6
            // 
            this.columnHeader6.Text = "Size (kB)";
            this.columnHeader6.Width = 80;
            // 
            // columnHeader7
            // 
            this.columnHeader7.Text = "ResSize (kB)";
            this.columnHeader7.Width = 80;
            // 
            // StreamingLevelsListView
            // 
            this.StreamingLevelsListView.BackColor = System.Drawing.SystemColors.ControlLight;
            this.StreamingLevelsListView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader2,
            this.columnHeader3});
            this.StreamingLevelsListView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.StreamingLevelsListView.FullRowSelect = true;
            this.StreamingLevelsListView.GridLines = true;
            this.StreamingLevelsListView.Location = new System.Drawing.Point(0, 0);
            this.StreamingLevelsListView.MultiSelect = false;
            this.StreamingLevelsListView.Name = "StreamingLevelsListView";
            this.StreamingLevelsListView.ShowGroups = false;
            this.StreamingLevelsListView.Size = new System.Drawing.Size(354, 205);
            this.StreamingLevelsListView.TabIndex = 5;
            this.StreamingLevelsListView.UseCompatibleStateImageBehavior = false;
            this.StreamingLevelsListView.View = System.Windows.Forms.View.Details;
            // 
            // columnHeader2
            // 
            this.columnHeader2.Text = "Level";
            this.columnHeader2.Width = 156;
            // 
            // columnHeader3
            // 
            this.columnHeader3.Text = "Status";
            this.columnHeader3.Width = 156;
            // 
            // TextureMemoryChart
            // 
            this.TextureMemoryChart.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.TextureMemoryChart.BackImageTransparentColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.TextureMemoryChart.BackSecondaryColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.TextureMemoryChart.BorderlineColor = System.Drawing.Color.LightGray;
            this.TextureMemoryChart.BorderSkin.PageColor = System.Drawing.Color.LightGray;
            chartArea1.AlignmentOrientation = ((System.Windows.Forms.DataVisualization.Charting.AreaAlignmentOrientations)((System.Windows.Forms.DataVisualization.Charting.AreaAlignmentOrientations.Vertical | System.Windows.Forms.DataVisualization.Charting.AreaAlignmentOrientations.Horizontal)));
            chartArea1.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            chartArea1.CursorX.IsUserEnabled = true;
            chartArea1.CursorX.IsUserSelectionEnabled = true;
            chartArea1.CursorY.IsUserEnabled = true;
            chartArea1.CursorY.IsUserSelectionEnabled = true;
            chartArea1.Name = "DefaultChartArea";
            this.TextureMemoryChart.ChartAreas.Add(chartArea1);
            this.TextureMemoryChart.Dock = System.Windows.Forms.DockStyle.Top;
            this.TextureMemoryChart.Location = new System.Drawing.Point(0, 245);
            this.TextureMemoryChart.Name = "TextureMemoryChart";
            this.TextureMemoryChart.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Bright;
            this.TextureMemoryChart.Size = new System.Drawing.Size(849, 245);
            this.TextureMemoryChart.TabIndex = 3;
            this.TextureMemoryChart.Text = "Texture Memory";
            title1.Alignment = System.Drawing.ContentAlignment.BottomCenter;
            title1.DockedToChartArea = "DefaultChartArea";
            title1.Docking = System.Windows.Forms.DataVisualization.Charting.Docking.Bottom;
            title1.IsDockedInsideChartArea = false;
            title1.Name = "Texture Memory";
            title1.Text = "Texture Memory";
            title1.TextOrientation = System.Windows.Forms.DataVisualization.Charting.TextOrientation.Horizontal;
            this.TextureMemoryChart.Titles.Add(title1);
            this.TextureMemoryChart.GetToolTipText += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.ToolTipEventArgs>(this.TextureMemoryChart_GetToolTipText);
            // 
            // DisplayedCurvesTreeView
            // 
            this.DisplayedCurvesTreeView.CheckBoxes = true;
            this.DisplayedCurvesTreeView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.DisplayedCurvesTreeView.Location = new System.Drawing.Point(0, 0);
            this.DisplayedCurvesTreeView.Name = "DisplayedCurvesTreeView";
            treeNode1.BackColor = System.Drawing.Color.Red;
            treeNode1.Checked = true;
            treeNode1.Name = "TitleFreeKB";
            treeNode1.Text = "TitleFreeKB";
            treeNode2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(128)))), ((int)(((byte)(255)))), ((int)(((byte)(128)))));
            treeNode2.Checked = true;
            treeNode2.Name = "LowestRecentTitleFreeKB";
            treeNode2.Text = "LowestRecentTitleFreeKB";
            treeNode3.BackColor = System.Drawing.Color.LightSteelBlue;
            treeNode3.Checked = true;
            treeNode3.Name = "AllocUnusedKB";
            treeNode3.Text = "AllocUnusedKB";
            treeNode4.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(128)))));
            treeNode4.Checked = true;
            treeNode4.Name = "AllocPureOverheadKB";
            treeNode4.Text = "AllocPureOverheadKB";
            treeNode5.BackColor = System.Drawing.Color.DodgerBlue;
            treeNode5.Name = "AllocUsedKB";
            treeNode5.Text = "AllocUsedKB";
            treeNode6.BackColor = System.Drawing.Color.YellowGreen;
            treeNode6.Name = "TaskResidentKB";
            treeNode6.Text = "TaskResidentKB (iDevice)";
            treeNode7.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(64)))));
            treeNode7.Name = "TaskVirtualKB";
            treeNode7.Text = "TaskVirtualKB (iDevice)";
            treeNode8.BackColor = System.Drawing.Color.YellowGreen;
            treeNode8.Name = "PhysicalFreeKB";
            treeNode8.Text = "PhysicalFreeKB (iDevice)";
            treeNode9.BackColor = System.Drawing.Color.BlueViolet;
            treeNode9.Name = "PhysicalUsedMemKB";
            treeNode9.Text = "PhysicalUsedMemKB (iDevice)";
            treeNode10.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(128)))), ((int)(((byte)(255)))), ((int)(((byte)(128)))));
            treeNode10.Name = "HighestMemRecentlyKB";
            treeNode10.Text = "HighestMemRecentlyKB (iDevice)";
            treeNode11.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(64)))), ((int)(((byte)(64)))));
            treeNode11.Name = "HighestMemEverKB";
            treeNode11.Text = "HighestMemEverKB (iDevice)";
            treeNode12.Checked = true;
            treeNode12.Name = "_SystemMemory";
            treeNode12.Text = "System Memory";
            treeNode13.BackColor = System.Drawing.Color.Red;
            treeNode13.Checked = true;
            treeNode13.Name = "TexturesInMemoryCurrent";
            treeNode13.Text = "Current Pool";
            treeNode14.BackColor = System.Drawing.Color.Green;
            treeNode14.Checked = true;
            treeNode14.Name = "TexturesInMemoryTarget";
            treeNode14.Text = "Target Pool";
            treeNode15.BackColor = System.Drawing.Color.DodgerBlue;
            treeNode15.Checked = true;
            treeNode15.Name = "OverBudget";
            treeNode15.Text = "Over Budget";
            treeNode16.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(255)))), ((int)(((byte)(255)))), ((int)(((byte)(128)))));
            treeNode16.Checked = true;
            treeNode16.Name = "PoolMemoryUsed";
            treeNode16.Text = "Used";
            treeNode17.BackColor = System.Drawing.Color.Cyan;
            treeNode17.Checked = true;
            treeNode17.Name = "LargestFreeMemoryHole";
            treeNode17.Text = "Largest Hole";
            treeNode18.Checked = true;
            treeNode18.Name = "_TextureMemory";
            treeNode18.Text = "Texture Memory";
            this.DisplayedCurvesTreeView.Nodes.AddRange(new System.Windows.Forms.TreeNode[] {
            treeNode12,
            treeNode18});
            this.DisplayedCurvesTreeView.Size = new System.Drawing.Size(209, 492);
            this.DisplayedCurvesTreeView.TabIndex = 2;
            this.DisplayedCurvesTreeView.AfterCheck += new System.Windows.Forms.TreeViewEventHandler(this.DisplayedCurvesTreeView_AfterCheck);
            this.DisplayedCurvesTreeView.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.DisplayedCurvesTreeView_AfterCheck);
            this.DisplayedCurvesTreeView.NodeMouseDoubleClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.DisplayedCurvesTreeView_NodeMouseDoubleClick);
            // 
            // SystemMemoryChart
            // 
            this.SystemMemoryChart.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.SystemMemoryChart.BackImageTransparentColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.SystemMemoryChart.BackSecondaryColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            this.SystemMemoryChart.BorderlineColor = System.Drawing.Color.LightGray;
            this.SystemMemoryChart.BorderSkin.PageColor = System.Drawing.Color.LightGray;
            chartArea2.AlignmentOrientation = ((System.Windows.Forms.DataVisualization.Charting.AreaAlignmentOrientations)((System.Windows.Forms.DataVisualization.Charting.AreaAlignmentOrientations.Vertical | System.Windows.Forms.DataVisualization.Charting.AreaAlignmentOrientations.Horizontal)));
            chartArea2.AxisX.MinorGrid.Enabled = true;
            chartArea2.BackColor = System.Drawing.Color.FromArgb(((int)(((byte)(224)))), ((int)(((byte)(224)))), ((int)(((byte)(224)))));
            chartArea2.CursorX.IsUserEnabled = true;
            chartArea2.CursorX.IsUserSelectionEnabled = true;
            chartArea2.CursorY.IsUserEnabled = true;
            chartArea2.CursorY.IsUserSelectionEnabled = true;
            chartArea2.Name = "DefaultChartArea";
            this.SystemMemoryChart.ChartAreas.Add(chartArea2);
            this.SystemMemoryChart.Dock = System.Windows.Forms.DockStyle.Top;
            this.SystemMemoryChart.Location = new System.Drawing.Point(0, 0);
            this.SystemMemoryChart.Name = "SystemMemoryChart";
            this.SystemMemoryChart.Palette = System.Windows.Forms.DataVisualization.Charting.ChartColorPalette.Bright;
            this.SystemMemoryChart.RightToLeft = System.Windows.Forms.RightToLeft.No;
            this.SystemMemoryChart.Size = new System.Drawing.Size(849, 245);
            this.SystemMemoryChart.TabIndex = 0;
            this.SystemMemoryChart.Text = "System Memory";
            title2.Alignment = System.Drawing.ContentAlignment.BottomCenter;
            title2.DockedToChartArea = "DefaultChartArea";
            title2.Docking = System.Windows.Forms.DataVisualization.Charting.Docking.Bottom;
            title2.IsDockedInsideChartArea = false;
            title2.Name = "System Memory";
            title2.Text = "System Memory";
            title2.TextOrientation = System.Windows.Forms.DataVisualization.Charting.TextOrientation.Horizontal;
            this.SystemMemoryChart.Titles.Add(title2);
            this.SystemMemoryChart.GetToolTipText += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.ToolTipEventArgs>(this.SystemMemoryChart_GetToolTipText);
            this.SystemMemoryChart.SelectionRangeChanged += new System.EventHandler<System.Windows.Forms.DataVisualization.Charting.CursorEventArgs>(this.MemoryChart_SelectionRangeChanged);
            this.SystemMemoryChart.MouseClick += new System.Windows.Forms.MouseEventHandler(this.MemoryChart_MouseClick);
            // 
            // panel1
            // 
            this.panel1.Controls.Add(this.label3);
            this.panel1.Controls.Add(this.PS3MemoryAdjuster);
            this.panel1.Controls.Add(this.CustomGroupingDisplayLabel);
            this.panel1.Controls.Add(this.cbOverrideGroupings);
            this.panel1.Controls.Add(this.label2);
            this.panel1.Controls.Add(this.GameSelectionBox);
            this.panel1.Controls.Add(this.cbInMB);
            this.panel1.Controls.Add(this.GenerateSummaryReportButton);
            this.panel1.Controls.Add(this.ClearListButton);
            this.panel1.Controls.Add(this.cbUseGoodSizes);
            this.panel1.Controls.Add(this.label1);
            this.panel1.Controls.Add(this.sigSizeKB_TextBox);
            this.panel1.Controls.Add(this.PickFileButton);
            this.panel1.Dock = System.Windows.Forms.DockStyle.Bottom;
            this.panel1.Location = new System.Drawing.Point(0, 500);
            this.panel1.MinimumSize = new System.Drawing.Size(329, 0);
            this.panel1.Name = "panel1";
            this.panel1.Size = new System.Drawing.Size(329, 229);
            this.panel1.TabIndex = 35;
            // 
            // label3
            // 
            this.label3.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.label3.AutoSize = true;
            this.label3.Location = new System.Drawing.Point(187, 153);
            this.label3.Name = "label3";
            this.label3.Size = new System.Drawing.Size(85, 13);
            this.label3.TabIndex = 45;
            this.label3.Text = "PS3 Mem Adjust";
            // 
            // PS3MemoryAdjuster
            // 
            this.PS3MemoryAdjuster.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.PS3MemoryAdjuster.Location = new System.Drawing.Point(204, 169);
            this.PS3MemoryAdjuster.Name = "PS3MemoryAdjuster";
            this.PS3MemoryAdjuster.Size = new System.Drawing.Size(100, 20);
            this.PS3MemoryAdjuster.TabIndex = 44;
            this.PS3MemoryAdjuster.Text = "0";
            this.PS3MemoryAdjuster.TextChanged += new System.EventHandler(this.PS3MemoryAdjuster_TextChanged);
            // 
            // CustomGroupingDisplayLabel
            // 
            this.CustomGroupingDisplayLabel.AutoSize = true;
            this.CustomGroupingDisplayLabel.Location = new System.Drawing.Point(35, 84);
            this.CustomGroupingDisplayLabel.Name = "CustomGroupingDisplayLabel";
            this.CustomGroupingDisplayLabel.Size = new System.Drawing.Size(133, 13);
            this.CustomGroupingDisplayLabel.TabIndex = 43;
            this.CustomGroupingDisplayLabel.Text = "Custom Grouping Filename";
            // 
            // cbOverrideGroupings
            // 
            this.cbOverrideGroupings.AutoSize = true;
            this.cbOverrideGroupings.Location = new System.Drawing.Point(12, 64);
            this.cbOverrideGroupings.Name = "cbOverrideGroupings";
            this.cbOverrideGroupings.Size = new System.Drawing.Size(126, 17);
            this.cbOverrideGroupings.TabIndex = 42;
            this.cbOverrideGroupings.Text = "Override Groupings...";
            this.cbOverrideGroupings.UseVisualStyleBackColor = true;
            this.cbOverrideGroupings.Click += new System.EventHandler(this.cbOverrideGroupings_Click);
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.label2.AutoSize = true;
            this.label2.Location = new System.Drawing.Point(5, 11);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(72, 13);
            this.label2.TabIndex = 41;
            this.label2.Text = "Current Game";
            // 
            // GameSelectionBox
            // 
            this.GameSelectionBox.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
            this.GameSelectionBox.FormattingEnabled = true;
            this.GameSelectionBox.Location = new System.Drawing.Point(8, 27);
            this.GameSelectionBox.Name = "GameSelectionBox";
            this.GameSelectionBox.Size = new System.Drawing.Size(312, 21);
            this.GameSelectionBox.TabIndex = 40;
            this.GameSelectionBox.SelectedIndexChanged += new System.EventHandler(this.GameSelectionBox_SelectedIndexChanged);
            // 
            // cbInMB
            // 
            this.cbInMB.AutoSize = true;
            this.cbInMB.Checked = true;
            this.cbInMB.CheckState = System.Windows.Forms.CheckState.Checked;
            this.cbInMB.Location = new System.Drawing.Point(12, 138);
            this.cbInMB.Name = "cbInMB";
            this.cbInMB.Size = new System.Drawing.Size(72, 17);
            this.cbInMB.TabIndex = 39;
            this.cbInMB.Text = "Units: MB";
            this.cbInMB.UseVisualStyleBackColor = true;
            // 
            // GenerateSummaryReportButton
            // 
            this.GenerateSummaryReportButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.GenerateSummaryReportButton.Location = new System.Drawing.Point(174, 194);
            this.GenerateSummaryReportButton.Name = "GenerateSummaryReportButton";
            this.GenerateSummaryReportButton.Size = new System.Drawing.Size(146, 23);
            this.GenerateSummaryReportButton.TabIndex = 38;
            this.GenerateSummaryReportButton.Text = "Generate Overall Report";
            this.GenerateSummaryReportButton.UseVisualStyleBackColor = true;
            this.GenerateSummaryReportButton.Click += new System.EventHandler(this.GenerateSummaryReportButton_Click);
            // 
            // ClearListButton
            // 
            this.ClearListButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.ClearListButton.Location = new System.Drawing.Point(8, 165);
            this.ClearListButton.Name = "ClearListButton";
            this.ClearListButton.Size = new System.Drawing.Size(94, 23);
            this.ClearListButton.TabIndex = 37;
            this.ClearListButton.Text = "Clear List";
            this.ClearListButton.UseVisualStyleBackColor = true;
            this.ClearListButton.Click += new System.EventHandler(this.ClearListButton_Click);
            // 
            // cbUseGoodSizes
            // 
            this.cbUseGoodSizes.AutoSize = true;
            this.cbUseGoodSizes.Location = new System.Drawing.Point(12, 115);
            this.cbUseGoodSizes.Name = "cbUseGoodSizes";
            this.cbUseGoodSizes.Size = new System.Drawing.Size(157, 17);
            this.cbUseGoodSizes.TabIndex = 36;
            this.cbUseGoodSizes.Text = "Use Good Sizes If Available";
            this.cbUseGoodSizes.UseVisualStyleBackColor = true;
            // 
            // label1
            // 
            this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.label1.AutoSize = true;
            this.label1.Location = new System.Drawing.Point(187, 115);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(125, 13);
            this.label1.TabIndex = 35;
            this.label1.Text = "Filter Groups Under X KB";
            // 
            // sigSizeKB_TextBox
            // 
            this.sigSizeKB_TextBox.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.sigSizeKB_TextBox.Location = new System.Drawing.Point(204, 131);
            this.sigSizeKB_TextBox.Name = "sigSizeKB_TextBox";
            this.sigSizeKB_TextBox.Size = new System.Drawing.Size(100, 20);
            this.sigSizeKB_TextBox.TabIndex = 34;
            this.sigSizeKB_TextBox.Text = "128";
            // 
            // PickFileButton
            // 
            this.PickFileButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.PickFileButton.Location = new System.Drawing.Point(8, 194);
            this.PickFileButton.Name = "PickFileButton";
            this.PickFileButton.Size = new System.Drawing.Size(160, 23);
            this.PickFileButton.TabIndex = 33;
            this.PickFileButton.Text = "Load all diffs in subfolders...";
            this.PickFileButton.UseVisualStyleBackColor = true;
            this.PickFileButton.Click += new System.EventHandler(this.PickFileButton_Click);
            // 
            // panel2
            // 
            this.panel2.Controls.Add(this.FileListTree);
            this.panel2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.panel2.Location = new System.Drawing.Point(0, 0);
            this.panel2.Name = "panel2";
            this.panel2.Size = new System.Drawing.Size(329, 500);
            this.panel2.TabIndex = 36;
            // 
            // FileListTree
            // 
            this.FileListTree.CheckBoxes = true;
            this.FileListTree.Dock = System.Windows.Forms.DockStyle.Fill;
            this.FileListTree.FullRowSelect = true;
            this.FileListTree.HideSelection = false;
            this.FileListTree.Location = new System.Drawing.Point(0, 0);
            this.FileListTree.Name = "FileListTree";
            this.FileListTree.Size = new System.Drawing.Size(329, 500);
            this.FileListTree.TabIndex = 35;
            this.FileListTree.AfterCheck += new System.Windows.Forms.TreeViewEventHandler(this.FileListTree_AfterSelect);
            this.FileListTree.AfterSelect += new System.Windows.Forms.TreeViewEventHandler(this.FileListTree_AfterSelect);
            // 
            // splitContainer2
            // 
            this.splitContainer2.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer2.Location = new System.Drawing.Point(3, 495);
            this.splitContainer2.Name = "splitContainer2";
            // 
            // splitContainer2.Panel1
            // 
            this.splitContainer2.Panel1.Controls.Add(this.StreamingLevelsListView);
            // 
            // splitContainer2.Panel2
            // 
            this.splitContainer2.Panel2.Controls.Add(this.splitContainer3);
            this.splitContainer2.Size = new System.Drawing.Size(1062, 205);
            this.splitContainer2.SplitterDistance = 354;
            this.splitContainer2.TabIndex = 8;
            // 
            // splitContainer3
            // 
            this.splitContainer3.Dock = System.Windows.Forms.DockStyle.Fill;
            this.splitContainer3.Location = new System.Drawing.Point(0, 0);
            this.splitContainer3.Name = "splitContainer3";
            // 
            // splitContainer3.Panel1
            // 
            this.splitContainer3.Panel1.Controls.Add(this.GroupListView);
            // 
            // splitContainer3.Panel2
            // 
            this.splitContainer3.Panel2.Controls.Add(this.ObjectListView);
            this.splitContainer3.Size = new System.Drawing.Size(704, 205);
            this.splitContainer3.SplitterDistance = 352;
            this.splitContainer3.TabIndex = 0;
            // 
            // splitContainer4
            // 
            this.splitContainer4.Dock = System.Windows.Forms.DockStyle.Top;
            this.splitContainer4.Location = new System.Drawing.Point(3, 3);
            this.splitContainer4.Name = "splitContainer4";
            // 
            // splitContainer4.Panel1
            // 
            this.splitContainer4.Panel1.Controls.Add(this.TextureMemoryChart);
            this.splitContainer4.Panel1.Controls.Add(this.SystemMemoryChart);
            // 
            // splitContainer4.Panel2
            // 
            this.splitContainer4.Panel2.Controls.Add(this.DisplayedCurvesTreeView);
            this.splitContainer4.Size = new System.Drawing.Size(1062, 492);
            this.splitContainer4.SplitterDistance = 849;
            this.splitContainer4.TabIndex = 9;
            // 
            // InfoView
            // 
            this.InfoView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.columnHeader1});
            this.InfoView.Cursor = System.Windows.Forms.Cursors.Default;
            this.InfoView.Dock = System.Windows.Forms.DockStyle.Fill;
            this.InfoView.Font = new System.Drawing.Font("Courier New", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
            this.InfoView.FullRowSelect = true;
            this.InfoView.Location = new System.Drawing.Point(3, 3);
            this.InfoView.Name = "InfoView";
            this.InfoView.Size = new System.Drawing.Size(1062, 697);
            this.InfoView.TabIndex = 1;
            this.InfoView.UseCompatibleStateImageBehavior = false;
            this.InfoView.View = System.Windows.Forms.View.Details;
            // 
            // columnHeader1
            // 
            this.columnHeader1.Text = "Report1";
            this.columnHeader1.Width = 500;
            // 
            // MemDiffer
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(1409, 753);
            this.Controls.Add(this.splitContainer1);
            this.Controls.Add(this.menuStrip1);
            this.MainMenuStrip = this.menuStrip1;
            this.Name = "MemDiffer";
            this.Text = "MemLeakCheck Differ";
            this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.MemDiffer_Closed);
            this.Load += new System.EventHandler(this.MemDiffer_Load);
            this.menuStrip1.ResumeLayout(false);
            this.menuStrip1.PerformLayout();
            this.splitContainer1.Panel1.ResumeLayout(false);
            this.splitContainer1.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer1)).EndInit();
            this.splitContainer1.ResumeLayout(false);
            this.TabView.ResumeLayout(false);
            this.InfoPage.ResumeLayout(false);
            this.AnalyzerPage.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.TextureMemoryChart)).EndInit();
            ((System.ComponentModel.ISupportInitialize)(this.SystemMemoryChart)).EndInit();
            this.panel1.ResumeLayout(false);
            this.panel1.PerformLayout();
            this.panel2.ResumeLayout(false);
            this.splitContainer2.Panel1.ResumeLayout(false);
            this.splitContainer2.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer2)).EndInit();
            this.splitContainer2.ResumeLayout(false);
            this.splitContainer3.Panel1.ResumeLayout(false);
            this.splitContainer3.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer3)).EndInit();
            this.splitContainer3.ResumeLayout(false);
            this.splitContainer4.Panel1.ResumeLayout(false);
            this.splitContainer4.Panel2.ResumeLayout(false);
            ((System.ComponentModel.ISupportInitialize)(this.splitContainer4)).EndInit();
            this.splitContainer4.ResumeLayout(false);
            this.ResumeLayout(false);
            this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.OpenFileDialog openFileDialog1;
        private System.Windows.Forms.SaveFileDialog saveFileDialog1;
        private System.Windows.Forms.MenuStrip menuStrip1;
        private System.Windows.Forms.ToolStripMenuItem fileToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem openMemLeakFilesInAFolderToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem reportToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem generateDiffReportForAllSelectedItemsToolStripMenuItem;
        private System.Windows.Forms.ToolStripMenuItem generateOverallReportToolStripMenuItem;
        private System.Windows.Forms.FolderBrowserDialog folderBrowserDialog1;
        private System.Windows.Forms.ToolStripSeparator toolStripMenuItem1;
        private System.Windows.Forms.ToolStripMenuItem runMemExperimentToolStripMenuItem;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
        private System.Windows.Forms.ToolStripMenuItem ExitToolStripMenuItem;
        private System.Windows.Forms.SplitContainer splitContainer1;
        private System.Windows.Forms.Panel panel2;
        private System.Windows.Forms.TreeView FileListTree;
        private System.Windows.Forms.Panel panel1;
        private System.Windows.Forms.Label label3;
        private System.Windows.Forms.TextBox PS3MemoryAdjuster;
        private System.Windows.Forms.Label CustomGroupingDisplayLabel;
        private System.Windows.Forms.CheckBox cbOverrideGroupings;
        private System.Windows.Forms.Label label2;
        private System.Windows.Forms.ComboBox GameSelectionBox;
        private System.Windows.Forms.CheckBox cbInMB;
        private System.Windows.Forms.Button GenerateSummaryReportButton;
        private System.Windows.Forms.Button ClearListButton;
        private System.Windows.Forms.CheckBox cbUseGoodSizes;
        private System.Windows.Forms.Label label1;
        private System.Windows.Forms.TextBox sigSizeKB_TextBox;
        private System.Windows.Forms.Button PickFileButton;
        private System.Windows.Forms.TabControl TabView;
        private System.Windows.Forms.TabPage InfoPage;
        private BufferedListView InfoView;
        private System.Windows.Forms.ColumnHeader columnHeader1;
        private System.Windows.Forms.TabPage AnalyzerPage;
        private System.Windows.Forms.ListView ObjectListView;
        private System.Windows.Forms.ColumnHeader columnHeader8;
        private System.Windows.Forms.ColumnHeader columnHeader9;
        private System.Windows.Forms.ColumnHeader columnHeader10;
        private System.Windows.Forms.ColumnHeader columnHeader11;
        private System.Windows.Forms.ListView GroupListView;
        private System.Windows.Forms.ColumnHeader columnHeader4;
        private System.Windows.Forms.ColumnHeader columnHeader6;
        private System.Windows.Forms.ColumnHeader columnHeader7;
        private System.Windows.Forms.ListView StreamingLevelsListView;
        private System.Windows.Forms.ColumnHeader columnHeader2;
        private System.Windows.Forms.ColumnHeader columnHeader3;
        private System.Windows.Forms.DataVisualization.Charting.Chart TextureMemoryChart;
        private System.Windows.Forms.TreeView DisplayedCurvesTreeView;
        private System.Windows.Forms.DataVisualization.Charting.Chart SystemMemoryChart;
        private System.Windows.Forms.SplitContainer splitContainer2;
        private System.Windows.Forms.SplitContainer splitContainer3;
        private System.Windows.Forms.SplitContainer splitContainer4;
    }
}

