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
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.Windows.Forms.DataVisualization.Charting;
using System.Xml;
using System.Xml.Serialization;
using EpicCommonUtilities;

namespace MemLeakDiffer
{
    public partial class MemDiffer : Form
    {
        SlowProgressDialog ProgressDialog;
		public SettableOptions Options = null;

        // True if the user has picked a custom filename, false if they've used the suggestion each time
        bool bPickedCustomFilename = false;

        // The value if the user has overridden the grouping filename
        string UserOverridenGroupingFilename = "";
        string DefaultGroupingFilenameForGame = "";

		/// <summary>
		/// Contains a data series for each class group (or metadata group, etc...)
		/// </summary>
		private static GroupedClassInfoTracker ClassGroupingData;

        public MemDiffer()
        {
			Options = UnrealControls.XmlHandler.ReadXml<SettableOptions>( Path.Combine( Application.StartupPath, "MemLeakCheckDiffer.Settings.xml" ) );
			
			InitializeComponent();
            ProgressDialog = new SlowProgressDialog();
			InitializeGameNameList();
			ClassGroupingData = new GroupedClassInfoTracker(0);
			ClassGroupingData.ParseGroupFile(GetGroupFilePath());
        }

		public static GroupedClassInfoTracker GetClassGroupingData()
		{
			return ClassGroupingData;
		}

        /// <summary>
        /// Reads through directories that are peer to Binaries for any that match the *Game pattern
        /// and adds the games to a drop-down list that can be picked.
        /// </summary>
        void InitializeGameNameList()
        {
            string ParentPath =
                Path.GetDirectoryName(Application.ExecutablePath) +
                Path.DirectorySeparatorChar +
                ".." +
                Path.DirectorySeparatorChar;
            string [] GameDirectories = Directory.GetDirectories(ParentPath, "*Game");

            // Fetch the currently active game name
            string ActiveGameName = Options.GameName;

            // Add the game names, highlighting the active one
            GameSelectionBox.Items.Clear();
            foreach (string GameDir in GameDirectories)
            {
                int CutIndex = GameDir.LastIndexOf(Path.DirectorySeparatorChar);
                string GameName = GameDir.Substring(CutIndex + 1);

                int GameIndexInComboBox = GameSelectionBox.Items.Add(GameName);
                if (GameName.ToUpperInvariant() == ActiveGameName.ToUpperInvariant())
                {
                    GameSelectionBox.SelectedIndex = GameIndexInComboBox;
                }
            }

            DefaultGroupingFilenameForGame = GetDefaultGroupFilePath();
            CustomGroupingDisplayLabel.Text = "(default for game)";
        }

        string GetDefaultGroupFilePath()
        {
            string ActiveGameDir = "";
            if (GameSelectionBox.SelectedItem == null)
            {
                if (GameSelectionBox.Items.Count > 0)
                {
					int CheckIdx = 0;
					foreach (string Item in GameSelectionBox.Items)
					{
						if (Item == "SwordGame")
						{
							GameSelectionBox.SelectedItem = GameSelectionBox.Items[CheckIdx];
							break;
						}
						CheckIdx++;
					}

					if (GameSelectionBox.SelectedItem == null)
					{
						GameSelectionBox.SelectedItem = GameSelectionBox.Items[0];
					}
				}
            }

            if (GameSelectionBox.SelectedItem != null)
            {
                ActiveGameDir = (GameSelectionBox.SelectedItem as string) + System.IO.Path.DirectorySeparatorChar;
            }

            string GroupFilePath =
                System.IO.Path.GetDirectoryName(Application.ExecutablePath) +
                System.IO.Path.DirectorySeparatorChar +
                ".." +
                System.IO.Path.DirectorySeparatorChar +
                ActiveGameDir +
                "Config" +
                System.IO.Path.DirectorySeparatorChar +
                "MemLeakCheckGroups.txt";
            
            GroupFilePath = Path.GetFullPath(GroupFilePath);
            
            return GroupFilePath;
        }

        /// <summary>
        /// Returns the constructed path to the active group file
        /// </summary>
        /// <returns></returns>
        string GetGroupFilePath()
        {
            if (cbOverrideGroupings.Checked)
            {
                return UserOverridenGroupingFilename;
            }
            else
            {
                return DefaultGroupingFilenameForGame;
            }
        }

        /// <summary>
        /// Searches through all the files in SearchDir ensuring that any memlk files found there have a
        /// corresponding node under ParentNode.  If not, they are loaded and parsed and returned in a list.
        /// </summary>
        private List<MemLeakFile> RefreshDirectory(string SearchDir, TreeNode ParentNode, BackgroundWorker BGWorker)
        {
            List<MemLeakFile> ResultList = new List<MemLeakFile>();

            // Add all of the memlk files that were found to the parent node
            string[] Files = Directory.GetFiles(SearchDir, "*.memlk");

			int FileIndex = 0;
            foreach (string Filename in Files)
            {
                if (ParentNode.Nodes.Find(Filename, false).Length == 0)
                {
                    string ShortName = Path.GetFileNameWithoutExtension(Filename);
                
                    // Report that we found a new file to parse
                    BGWorker.ReportProgress((FileIndex*100) / Files.Length, "Parsing '" + ShortName + "'");
                    FileIndex++;

                    // Parse it
                    ResultList.Add(new MemLeakFile(Filename));
                }
            }

            return ResultList;
        }

        /// <summary>
        /// Bring up a file dialog to let a user pick a file, and then open all of the memlk files in that directory
        /// </summary>
        private void PickFileButton_Click(object sender, EventArgs e)
        {
            if (openFileDialog1.ShowDialog() == DialogResult.OK)
            {
                string SearchDir = Path.GetDirectoryName(openFileDialog1.FileName);
                OpenAllFilesInDirectory(SearchDir);

                FileListTree.ExpandAll();
            }
        }

        /// <summary>
        /// Opens all of the MemLeak files in a directory
        /// </summary>
        /// <param name="SearchDir"></param>
        private void OpenAllFilesInDirectory(string SearchDir)
        {
            // Get just the name of the directory containing the selected file
            int CutIndex = SearchDir.LastIndexOf(Path.DirectorySeparatorChar);
            string ParentDir = SearchDir.Substring(CutIndex + 1);

            // Find or create the parent node representing the directory
            TreeNode[] ParentDirNodes = FileListTree.Nodes.Find(ParentDir, false);
            TreeNode ParentNode = null;
            if (ParentDirNodes.Length == 0)
            {
                ParentNode = FileListTree.Nodes.Add(ParentDir);
                ParentNode.Checked = true;
                ParentNode.Tag = SearchDir;
            }
            else
            {
                Debug.Assert(ParentDirNodes.Length == 1);
                ParentNode = ParentDirNodes[0];
            }

            // List of files that were found during the async scan
            List<MemLeakFile> WorkingSetDuringScanning = null;

            // The work to do asynchronously (scan for files and parse them)
            ProgressDialog.OnBeginBackgroundWork = delegate(BackgroundWorker BGWorker)
            {
                // Add all of the memlk files that were found to the parent node
                WorkingSetDuringScanning = RefreshDirectory(SearchDir, ParentNode, BGWorker);
            };

            if (ProgressDialog.ShowDialog() == DialogResult.OK)
            {
				if (WorkingSetDuringScanning != null)
				{
					// Sort the files by their capture timestamps
					WorkingSetDuringScanning.Sort();
					// Store the parsed files into the tree
					foreach (MemLeakFile File in WorkingSetDuringScanning)
					{
						string ShortName = Path.GetFileNameWithoutExtension(File.Filename);
						TreeNode NewFileNode = ParentNode.Nodes.Add(ShortName);
						NewFileNode.Checked = true;
						NewFileNode.Tag = File;
					}

					FillMemoryCharts();
				}
            }
        }


        /// <summary>
        /// Handler invoked when tree selection changes.  If a single memlk file is selected in the tree, it will be
        /// displayed in the right-hand panel.
        /// </summary>
        private void FileListTree_AfterSelect(object sender, TreeViewEventArgs e)
        {
            InfoView.Clear();

            TreeNode Node = FileListTree.SelectedNode;
            if (Node == null)
            {
                return;
            }

            MemLeakFile report1 = Node.Tag as MemLeakFile;
            if (report1 == null)
            {
                return;
            }

            InfoView.BeginUpdate();
            ColumnHeader Header = InfoView.Columns.Add("Info");
            Header.Width = -2;
            InfoView.Groups.Clear();

            int GroupKey = 0;
            foreach (ReportSection Section in report1.Sections)
            {
                ++GroupKey;
                ListViewGroup Group = InfoView.Groups.Add(GroupKey.ToString(), Section.MyHeading);
                foreach (string line in Section.MyLines)
                {
                    ListViewItem Item = InfoView.Items.Add(line);
                    Item.Group = Group;
                    //Item.Text = line;
                }
            }

            InfoView.EndUpdate();
        }

        /// <summary>
        /// Handle initial application initialization
        /// </summary>
        private void MemDiffer_Load(object sender, EventArgs e)
        {
            Application.EnableVisualStyles();
            ObjectDumpLine.Init();
        }

        /// <summary>
        /// Picks a suitable default filename to save a diff file, given a tree containing file nodes
        /// </summary>
        string GetSuggestedFilename(TreeNodeCollection Tree)
        {
            string Result = null;
            string FirstDir = "";
            foreach (TreeNode Node in Tree)
            {
                if (Result == null)
                {
                    Result = Node.Text;
                    FirstDir = Node.Tag as string;
                }
                else
                {
                    Result = Result + "_vs_" + Node.Text;
                }
            }

            if (Result == null)
            {
                Result = "DiffReport";
            }

            return FirstDir + Path.DirectorySeparatorChar + Result + ".csv";
        }

        /// <summary>
        /// Builds a list of all of the memlk files in the passed in tree.
        /// </summary>
        /// <param name="Tree">The tree to search for files (a file will be in the Tag of sub nodes).</param>
        /// <param name="Files">List to place found files in.  List must be already allocated.</param>
        void BuildRecursiveListFromTree(TreeNodeCollection Tree, List<MemLeakFile> Files)
        {
            foreach (TreeNode Node in Tree)
            {
                if (Node.Checked)
                {
                    MemLeakFile MLF = Node.Tag as MemLeakFile;

                    if (MLF != null)
                    {
                        Files.Add(MLF);
                    }

                    BuildRecursiveListFromTree(Node.Nodes, Files);
                }
            }

            Files.Sort();
        }

        /// <summary>
        /// Parses the Min. Size KB filter box and returns the value if valid, or a passed-in default value otherwise
        /// </summary>
        /// <returns></returns>
        int GetCutoffSizeKB(int DefaultValue)
        {
            int MinSignificantSizeKB;
            if (!int.TryParse(sigSizeKB_TextBox.Text, out MinSignificantSizeKB))
            {
                MinSignificantSizeKB = DefaultValue;
            }
            else
            {
                MinSignificantSizeKB = Math.Max(MinSignificantSizeKB, 0);
            }

            return MinSignificantSizeKB;
        }

        /// <summary>
        /// Parses the adjustment size box for PS3 (to allow running on a devkit but treating it like a testkit/retail console)
        /// </summary>
        int GetMemoryAdjustPS3SizeKB()
        {
            int AdjustmentSizeBytes = 0;
            if (!int.TryParse(PS3MemoryAdjuster.Text, out AdjustmentSizeBytes))
            {
                AdjustmentSizeBytes = 0;
            }

            return AdjustmentSizeBytes * 1024;
        }

        /// <summary>
        /// Exports a difference report for all of the files whose nodes are checked in the tree view.
        /// Currently the report contains:
        ///    Title Free Memory (in KB)
        ///    Memory Waste within the allocator (in KB)
        ///    Per-class information from an 'OBJ LIST' command
        ///    A sum of all sounds in the 'Listing all sounds' section
        ///    A mapping from entry back to original .memlk file
        /// </summary>
        private void CreateDiffReportButton_Click(object sender, EventArgs e)
        {
            ObjectDumpLine.bUseTrueResKB = cbUseGoodSizes.Checked;

            string SuggestedFilename = GetSuggestedFilename(FileListTree.Nodes);
            saveFileDialog1.Filter = "CSV Spreadsheet (*.csv)|*.csv|All Files (*.*)|*.*";
            if (!bPickedCustomFilename)
            {
                saveFileDialog1.InitialDirectory = Path.GetDirectoryName(SuggestedFilename);
                saveFileDialog1.FileName = Path.GetFileName(SuggestedFilename);
            }

            if (saveFileDialog1.ShowDialog() == DialogResult.OK)
            {
                if (saveFileDialog1.FileName != SuggestedFilename)
                {
                    bPickedCustomFilename = true;
                }

                int MinSignificantSizeKB = GetCutoffSizeKB(256);
                string OutputFilename = saveFileDialog1.FileName;

                // Build a list of loaded memory leak files
                List<MemLeakFile> Files = new List<MemLeakFile>();
                BuildRecursiveListFromTree(FileListTree.Nodes, Files);

                // Generate the report
                try
                {
                    SequentialDiffReporter ReportMaker = new SequentialDiffReporter(Files);
                    ReportMaker.bUseMB = cbInMB.Checked;
                    ReportMaker.ProcessFiles(GetGroupFilePath());

                    ReportMaker.GenerateStandardReport(OutputFilename, MinSignificantSizeKB);

                    string TextureOutputFilename = Path.ChangeExtension(OutputFilename, null) + "_texpool.csv";
                    ReportMaker.GeneratePriorityGroupReport(TextureOutputFilename, "TexturePool");

                    string MemoryOutputFilename = Path.ChangeExtension(OutputFilename, null) + "_mempool.csv";
                    ReportMaker.GeneratePriorityGroupReport(MemoryOutputFilename, 200, 203);
                }
                catch (System.Exception)
                {
                	MessageBox.Show("Failed to save diff to file '" + OutputFilename + "', make sure it is not open in Excel right now");
                }
            }
        }

        
        /// <summary>
        /// Runs a series of experiments on information from the pooled allocator gathered for the currently checked files
        /// The actual experiments have no GUI; edit the code in PooledAllocatorExperiments.cs to change tests.
        /// Output is to the console.
        /// </summary>
        private void RunMemExperimentButton_Click(object sender, EventArgs e)
        {
            // Build a list of loaded memory leak files
            List<MemLeakFile> Files = new List<MemLeakFile>();
            BuildRecursiveListFromTree(FileListTree.Nodes, Files);

            // Run the experiments
            PooledAllocatorExperiments.Run(Files);
        }

        /// <summary>
        /// Clear the tree list of files
        /// </summary>
        private void ClearListButton_Click(object sender, EventArgs e)
        {
            FileListTree.Nodes.Clear();
        }

        /// <summary>
        /// Generates a full csv for every sub group of files, as well as an overarching one
        /// covering general trends for all
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void GenerateSummaryReportButton_Click(object sender, EventArgs e)
        {
            ProgressDialog.OnBeginBackgroundWork = DoSummaryReport;
            ProgressDialog.ShowDialog();
        }

        /// <summary>
        /// Constructs a CSV fragment containing info on a statistical variable, in the format used for the summary report
        /// </summary>
        string StartSummaryReportStatLine(string StatName, StatisticalFloat Stat, float SizeScaler)
        {
            return String.Format("{0},{1:0.0},{2:0.0},{3:0.0},{4:0.0},{5:0.0}",
                StatName,
                Stat.Min * SizeScaler,
                Stat.Max * SizeScaler,
                Stat.Average * SizeScaler,
                Stat.Median * SizeScaler,
                Stat.StandardDeviation * SizeScaler);
        }

        /// <summary>
        /// Converts Mode to a human readable representation
        /// </summary>
        public static string StatTypeToString(EStatType Mode)
        {
            switch (Mode)
            {
                case EStatType.Minimum:
                    return "Min";
                case EStatType.Maximum:
                    return "Max";
                case EStatType.Average:
                    return "Mean";
                case EStatType.StDev:
                    return "St.Dev";
                case EStatType.Median:
                    return "Median";
                case EStatType.Count:
                    return "Count";
                default:
                    return "(unknown)";
            }
        }

        private void DoSummaryReport(BackgroundWorker BGWorker)
        {
            int NodeIndex = 0;
            string FirstDirectory = null;

            ObjectDumpLine.bUseTrueResKB = cbUseGoodSizes.Checked;

            bool bUseMB = cbInMB.Checked;
            string SizeSuffix = bUseMB ? "MB" : "KB";
            float SizeScaler = bUseMB ? 1.0f / 1024.0f : 1.0f;

            // Summary info and per-directory info
            SeriesReportData OverallInfo = new SeriesReportData("GlobalSummary");
            List<SeriesReportData> PerRunInfo = new List<SeriesReportData>();

            string GroupFilePath = GetGroupFilePath();

            foreach (TreeNode RunDirectoryNode in FileListTree.Nodes)
            {
                // Report on the overall progress
                int ProgressPercentage = (NodeIndex * 100) / FileListTree.Nodes.Count;
                string CurState = "Generating a report for '" + RunDirectoryNode.Text + "'";
                BGWorker.ReportProgress(ProgressPercentage, CurState);
                NodeIndex++;

                // Construct a filename (in the parent directory of the set of runs, with suffix _full to indicate this was an unfiltered run)
                string RunDir = (RunDirectoryNode.Tag as string) + Path.DirectorySeparatorChar + ".." + Path.DirectorySeparatorChar;
                string OutputFilename = RunDir + RunDirectoryNode.Text + "_full.csv";
                if (FirstDirectory == null)
                {
                    FirstDirectory = RunDir;
                }

                // Build a list of loaded memory leak files for this directory node
                List<MemLeakFile> Files = new List<MemLeakFile>();
                BuildRecursiveListFromTree(RunDirectoryNode.Nodes, Files);

                // Generate the report
                if (Files.Count > 0)
                {
                    try
                    {
                        // Generate one report, with no filtering
                        SequentialDiffReporter ReportMaker = new SequentialDiffReporter(Files);
                        ReportMaker.bUseMB = bUseMB;
                        ReportMaker.ProcessFiles(GroupFilePath);

                        SeriesReportData RunData = ReportMaker.GenerateStandardReport(OutputFilename, 0);
                        PerRunInfo.Add(RunData);

                        string TextureOutputFilename = String.Format("{0}TextureStats{1}{2}_texpool.csv",
                            RunDir, Path.DirectorySeparatorChar, RunDirectoryNode.Text);
                        ReportMaker.GeneratePriorityGroupReport(TextureOutputFilename, "TexturePool");

                        string MemoryOutputFilename = String.Format("{0}MemoryPoolStats{1}{2}_mempool.csv",
                            RunDir, Path.DirectorySeparatorChar, RunDirectoryNode.Text);
                        ReportMaker.GeneratePriorityGroupReport(MemoryOutputFilename, 200, 203);
                    }
                    catch (System.Exception)
                    {
                        MessageBox.Show("Failed to save diff to file '" + OutputFilename + "', make sure it is not open in Excel right now");
                    }
                }
            }

            // Done creating the per-directory ones, now create the global summary CSV
            string SummaryOutputFilename = ((FirstDirectory != null) ? FirstDirectory : "") + "GlobalSummary.csv";
            try
            {
                // Aggregate the info per run directory into the summary info
                string Headers = "GroupName,Min,Max,Avg,Median,StDev,Mode";
                foreach (SeriesReportData Run in PerRunInfo)
                {
                    Headers = Headers + "," + Run.Name.Replace("_full", "");

                    // Aggregate the group info
                    foreach (string GroupName in Run.Summaries.Keys)
                    {
                        if (OverallInfo.Summaries.ContainsKey(GroupName))
                        {
                            OverallInfo.Summaries[GroupName].MergeSamples(Run.Summaries[GroupName]);
                        }
                        else
                        {
                            OverallInfo.Summaries.Add(GroupName, new GroupSummary(Run.Summaries[GroupName]));
                        }
                    }
                }

                // Generate one report, with no filtering
                StreamWriter SW = new StreamWriter(SummaryOutputFilename);

                // Write out the header row
                SW.WriteLine(Headers);

                // Write out a line per group, with aggregate stats and per-directory stats
                int MinSignificantSizeKB = GetCutoffSizeKB(256);
                foreach (KeyValuePair<string, GroupSummary> KVP in OverallInfo.Summaries)
                {
                    string GroupName = KVP.Key;
                    GroupSummary Group = KVP.Value;

                    // Skip ones that were tiny in *every* run
                    if (!Group.PassesInterestingForSummaryFilter(MinSignificantSizeKB, true))
                    {
                        continue;
                    }

                    float GroupSizeScaler = (Group.SampleUnitType == ESampleUnit.Kilobytes) ? SizeScaler : 1.0f;

                    // Write out the aggregate statistics for the group
                    string LineOut = StartSummaryReportStatLine(GroupName, Group.Distribution, GroupSizeScaler);

                    // Write out the averaging mode
                    LineOut += "," +  StatTypeToString(Group.OverviewStatType) + ">";

                    // Write out the important statistic of this group for each directory of files
                    foreach (SeriesReportData Run in PerRunInfo)
                    {
                        double InstanceStatSize = 0.0;

                        GroupSummary InstanceGroup;
                        if (Run.Summaries.TryGetValue(GroupName, out InstanceGroup))
                        {
                            InstanceStatSize = InstanceGroup.Distribution.GetByType(Group.OverviewStatType) * GroupSizeScaler;
                        }

                        LineOut = LineOut + "," + InstanceStatSize.ToString("0.0");
                    }

                    SW.WriteLine(LineOut);
                }

                SW.Close();
            }
            catch (System.Exception)
            {
                MessageBox.Show("Failed to save diff to file '" + SummaryOutputFilename + "', make sure it is not open in Excel right now");
            }
        }

        private void openAllMemLeakFilesInAFolderAndSubfoldersToolStripMenuItem_Click(object sender, EventArgs e)
        {
            if (folderBrowserDialog1.ShowDialog() == DialogResult.OK)
            {
                string RootPath = folderBrowserDialog1.SelectedPath;

                // Open all of the files in the current directory if there are no sub directories, and all of the files in each subdirectory otherwise
                string[] SubDirectories = Directory.GetDirectories(RootPath);
                if (SubDirectories.Length == 0)
                {
                    OpenAllFilesInDirectory(RootPath);
                }
                else
                {
                    foreach (string CurrentDir in SubDirectories)
                    {
                        OpenAllFilesInDirectory(CurrentDir);
                    }
                }
            }
        }

        private void cbOverrideGroupings_Click(object sender, EventArgs e)
        {
            if (cbOverrideGroupings.Checked)
            {
                // Pick a good default
                if (!File.Exists(UserOverridenGroupingFilename))
                {
                    UserOverridenGroupingFilename = GetDefaultGroupFilePath();
                }
                openFileDialog1.FileName = Path.GetFileName(UserOverridenGroupingFilename);
                openFileDialog1.InitialDirectory = Path.GetDirectoryName(UserOverridenGroupingFilename);

                // Display the file dialog to pick a new grouping
                bool bSuccessfullyPickedFile = false;

                string SavedFilter = openFileDialog1.Filter;
                openFileDialog1.Filter = "Text Files (*.txt)|*.txt|All Files (*.*)|*.*";
                if (openFileDialog1.ShowDialog() == DialogResult.OK)
                {
                    if (File.Exists(openFileDialog1.FileName))
                    {
                        bSuccessfullyPickedFile = true;
                        UserOverridenGroupingFilename = openFileDialog1.FileName;
                    }
                }
                openFileDialog1.Filter = SavedFilter;

                // Now display the name of the custom chosen grouping file
                if (bSuccessfullyPickedFile)
                {
                    CustomGroupingDisplayLabel.Text = Path.GetFileName(UserOverridenGroupingFilename);
                }
                else
                {
                    cbOverrideGroupings.Checked = false;
                }
            }
            else
            {
                // Clear the label
                CustomGroupingDisplayLabel.Text = "(default for game)";
            }
        }

        private void GameSelectionBox_SelectedIndexChanged(object sender, EventArgs e)
        {
            DefaultGroupingFilenameForGame = GetDefaultGroupFilePath();

            // Push the active game back to settings
			if( GameSelectionBox.SelectedItem != null && GameSelectionBox.SelectedItem.ToString().Length > 0 )
			{
				Options.GameName = GameSelectionBox.SelectedItem.ToString();
			}
			ClassGroupingData = new GroupedClassInfoTracker(0);
			ClassGroupingData.ParseGroupFile(GetGroupFilePath());
		}

		private void ExitToolStripMenuItem_Click(object sender, EventArgs e)
		{
			// Exit the application
			this.Close();
		}

		private void TabView_KeyDown(object sender, KeyEventArgs e)
		{
			if (TabView.SelectedIndex >= 0)
			{
				if (TabView.TabPages[TabView.SelectedIndex].Name == "AnalyzerPage")
				{
					int Scale = 1;
					if (Control.ModifierKeys == Keys.Shift)
					{
						Scale = 10;
					}
					if (e.KeyCode == Keys.Right)
					{
						MoveChartCursor(Scale);
						e.Handled = true;
					}
					else if (e.KeyCode == Keys.Left)
					{
						MoveChartCursor(-Scale);
						e.Handled = true;
					}
				}
			}
		}

		private TreeNode GetChartColorNode(string Legend, ref TreeNode OutParentNode)
		{
			foreach (TreeNode ParentNode in DisplayedCurvesTreeView.Nodes)
			{
				foreach (TreeNode ColorNode in ParentNode.Nodes)
				{
					if (ColorNode.Name == Legend)
					{
						OutParentNode = ParentNode;
						return ColorNode;
					}
				}
			}

			return null;
		}

		private bool IsChartEntryEnabled(string Legend, bool bNotFoundResult)
		{
			TreeNode ParentNode = null;
			TreeNode ColorNode = GetChartColorNode(Legend, ref ParentNode);
			if (ColorNode != null)
			{
				return ColorNode.Checked;
			}

			return bNotFoundResult;
		}

		private bool IsDisplayEnabled(string Legend, bool bNotFoundResult)
		{
			foreach (TreeNode ParentNode in DisplayedCurvesTreeView.Nodes)
			{
				if (ParentNode.Name == Legend)
				{
					return ParentNode.Checked;
				}
			}
			return bNotFoundResult;
		}

		private MemLeakFile GetSelectedMemLeakFile()
		{
			TreeNode NodeToFillFrom = GetNodeToFillChartsFrom();
			if (NodeToFillFrom != null)
			{
				int FrameIndex = (int)SystemMemoryChart.ChartAreas[0].CursorX.Position;
				// Find the memleak file corresponding to this position
				MemLeakFile ReturnFile = null;
				int CheckIndex = 0;
				foreach (TreeNode FileNode in NodeToFillFrom.Nodes)
				{
					// Assuming a one-deep heirarchy here...
					MemLeakFile LeakFile = FileNode.Tag as MemLeakFile;
					if (LeakFile != null)
					{
						if (CheckIndex == FrameIndex)
						{
							ReturnFile = LeakFile;
							break;
						}
						CheckIndex++;
					}
				}

				return ReturnFile;
			}
			return null;
		}

		private Series AddChartSeries(string Legend, string Name)
		{
			// Find the legend in the Displayed Curve
			Color LineColor = Color.FromArgb(255,0,0,0);
			bool bEnabled = false;
			bool bWideLine = false;
			Chart OwnerChart = null;

			TreeNode ParentNode = null;
			TreeNode ColorNode = GetChartColorNode(Legend, ref ParentNode);
			if (ColorNode != null)
			{
				LineColor = ColorNode.BackColor;
				bEnabled = ColorNode.Checked;
				if (ParentNode.Name == "_TextureMemory")
				{
					OwnerChart = TextureMemoryChart;
				}
				else if (ParentNode.Name == "_SystemMemory")
				{
					OwnerChart = SystemMemoryChart;
				}
			}
			else
			{
				if (Legend == "MemoryBarKB")
				{
					LineColor = ClassGroupingData.SystemMemBarColor;
					bEnabled = true;
					bWideLine = true;
					OwnerChart = SystemMemoryChart;
				}
			}

			if (OwnerChart == null)
			{
				return null;
			}

			Series ExistingSeries = null;
			// Does the series already exist? 
			// If so, return it if it is enabled (after setting the color)
			//        or remove it if it is disabled
			for (int SeriesIdx = 0; (SeriesIdx < OwnerChart.Series.Count) && (ExistingSeries == null); SeriesIdx++)
			{
				if (OwnerChart.Series[SeriesIdx] != null)
				{
					if ((OwnerChart.Series[SeriesIdx].Name == Name) &&
						(OwnerChart.Series[SeriesIdx].Legend == Legend))
					{
						ExistingSeries = OwnerChart.Series[SeriesIdx];
					}
				}
			}

			if (ExistingSeries != null)
			{
				if (bEnabled == true)
				{
					ExistingSeries.Color = LineColor;
					return ExistingSeries;
				}
				else
				{
					// remove it!
					OwnerChart.Series.Remove(ExistingSeries);
					return null;
				}
			}

			if (bEnabled == true)
			{
				Series NewSeries = new Series();
				NewSeries.ChartArea = "DefaultChartArea";
				NewSeries.ChartType = SeriesChartType.Line;
				NewSeries.Legend = Legend;
				NewSeries.Name = Name;
				NewSeries.Color = LineColor;
				if (bWideLine == true)
				{
					NewSeries.BorderWidth = NewSeries.BorderWidth * 3;
					NewSeries.MarkerSize = NewSeries.MarkerSize * 3;
				}

				int NewIndex = OwnerChart.Series.Count();
				OwnerChart.Series.Add(NewSeries);

				return OwnerChart.Series[NewIndex];
			}

			return null;
		}

		private void InitializeChartForDataUpdate(ref Chart ChartToBeFilled)
		{
			ChartToBeFilled.BeginInit();
			foreach (var Series in ChartToBeFilled.Series)
			{
				Series.Points.Clear();
			}
		}

		private void PrepChartForDataFill(ref Chart ChartToBeFilled)
		{
			ChartToBeFilled.ResetAutoValues();
			ChartToBeFilled.Invalidate();
		}

		private void FinalizeChartDataFile(ref Chart ChartThatWasFilled)
		{
			ChartThatWasFilled.Invalidate();
			// Let the system deal with automatically scaling the axis.
			ChartThatWasFilled.ChartAreas["DefaultChartArea"].RecalculateAxesScale();
			ChartThatWasFilled.EndInit();
		}

		private TreeNode GetNodeToFillChartsFrom()
		{
			TreeNode NodeToFillFrom = FileListTree.SelectedNode;
			if (NodeToFillFrom == null)
			{
				NodeToFillFrom = FileListTree.TopNode;
			}

			if (NodeToFillFrom != null)
			{
				if (NodeToFillFrom.GetNodeCount(true) == 0)
				{
					TreeNode Parent = NodeToFillFrom.Parent;
					while ((NodeToFillFrom != null) && (NodeToFillFrom.GetNodeCount(true) == 0))
					{
						NodeToFillFrom = Parent;
					}
				}
			}

			if (NodeToFillFrom == null)
			{
				// nothing to fill in...
				return null;
			}

			int MemLeakFileCount = 0;
			foreach (TreeNode FileNode in NodeToFillFrom.Nodes)
			{
				MemLeakFile LeakFile = FileNode.Tag as MemLeakFile;
				if (LeakFile != null)
				{
					MemLeakFileCount++;
				}
			}

			if (MemLeakFileCount == 0)
			{
				// no files to fill in
				return null;
			}

			return NodeToFillFrom;
		}

		private void FillMemoryCharts()
		{
			// Start Init and reset existing data.
			InitializeChartForDataUpdate(ref SystemMemoryChart);
			InitializeChartForDataUpdate(ref TextureMemoryChart);

			TreeNode NodeToFillFrom = GetNodeToFillChartsFrom();
			if (NodeToFillFrom == null)
			{
				return;
			}

			// The system memory seriese...
			Series SysMem_MemoryBarKB = AddChartSeries("MemoryBarKB", "SM_MemoryBarKB");
			Series SysMem_TitleFreeKB = AddChartSeries("TitleFreeKB", "SM_TitleFreeKB");
			Series SysMem_LowestRecentTitleFreeKB = AddChartSeries("LowestRecentTitleFreeKB", "SM_LowestRecentTitleFreeKB");
			Series SysMem_AllocUnusedKB = AddChartSeries("AllocUnusedKB", "SM_AllocUnusedKB");
            Series SysMem_AllocUsedKB = AddChartSeries("AllocUsedKB", "SM_AllocUsedKB");
//			Series SysMem_AllocPureOverheadKB = AddChartSeries("AllocPureOverheadKB", "SM_AllocPureOverheadKB");
			Series SysMem_PhysicalFreeKB = AddChartSeries("PhysicalFreeKB", "SM_PhysicalFreeKB");
			Series SysMem_PhysicalUsedKB = AddChartSeries("PhysicalUsedKB", "SM_PhysicalUsedKB");
			Series SysMem_TaskResidentKB = AddChartSeries("TaskResidentKB", "SM_TaskResidentKB");
			Series SysMem_TaskVirtualKB = AddChartSeries("TaskVirtualKB", "SM_TaskVirtualKB");
			Series SysMem_HighestMemRecentKB = AddChartSeries("HighestMemRecentlyKB", "SM_HighestMemRecentlyKB");
			Series SysMem_HighestMemEverKB = AddChartSeries("HighestMemEverKB", "SM_HighestMemEverKB");

			// The texture series...
			Series TxtrInMemCurr_Series = AddChartSeries("TexturesInMemoryCurrent", "TM_TexturesInMemoryCurrent");
			Series TxtrInMemTarget_Series = AddChartSeries("TexturesInMemoryTarget", "TM_TexturesInMemoryTarget");
			Series TxtrOverBudget_Series = AddChartSeries("OverBudget", "TM_OverBudget");
			Series TxtrPoolUsed_Series = AddChartSeries("PoolMemoryUsed", "TM_PoolMemoryUsed");
			Series TxtrPoolLargestHole = AddChartSeries("LargestFreeMemoryHole", "TM_LargestFreeMemoryHole");

			PrepChartForDataFill(ref SystemMemoryChart);
			PrepChartForDataFill(ref TextureMemoryChart);

			Console.WriteLine("Filling in the Analysis Chart!");

			int XAxis = 0;
			foreach (TreeNode FileNode in NodeToFillFrom.Nodes)
			{
				// Assuming a one-deep heirarchy here...
				MemLeakFile LeakFile = FileNode.Tag as MemLeakFile;
				if (LeakFile != null)
				{
					TextureStreamingSection TextureStreamingSectionInst;
					if (LeakFile.FindFirstSection<TextureStreamingSection>(out TextureStreamingSectionInst))
					{
						if (TxtrInMemCurr_Series != null)
						{
							TxtrInMemCurr_Series.Points.AddXY(XAxis, TextureStreamingSectionInst.TexturesInMemoryCurrent);
						}
						if (TxtrInMemTarget_Series != null)
						{
							TxtrInMemTarget_Series.Points.AddXY(XAxis, TextureStreamingSectionInst.TexturesInMemoryTarget);
						}
						if (TxtrOverBudget_Series != null)
						{
							TxtrOverBudget_Series.Points.AddXY(XAxis, TextureStreamingSectionInst.OverBudget);
						}
						if (TxtrPoolUsed_Series != null)
						{
							TxtrPoolUsed_Series.Points.AddXY(XAxis, TextureStreamingSectionInst.PoolMemoryUsed);
						}
						if (TxtrPoolLargestHole != null)
						{
							TxtrPoolLargestHole.Points.AddXY(XAxis, TextureStreamingSectionInst.LargestFreeMemoryHole);
						}
					}

					MemStatsReportSection MemStartsReportSectionInst;
					if (LeakFile.FindFirstSection<MemStatsReportSection>(out MemStartsReportSectionInst))
					{
						SysMem_MemoryBarKB.Points.AddXY(XAxis, ClassGroupingData.SystemMemBar);
						if (SysMem_TitleFreeKB != null)
						{
							SysMem_TitleFreeKB.Points.AddXY(XAxis, MemStartsReportSectionInst.TitleFreeKB);
						}
						if (SysMem_LowestRecentTitleFreeKB != null)
						{
							SysMem_LowestRecentTitleFreeKB.Points.AddXY(XAxis, MemStartsReportSectionInst.LowestRecentTitleFreeKB);
						}
						if (SysMem_AllocUnusedKB != null)
						{
							SysMem_AllocUnusedKB.Points.AddXY(XAxis, MemStartsReportSectionInst.AllocUnusedKB);
						}
                        if (SysMem_AllocUsedKB != null)
                        {
                            SysMem_AllocUsedKB.Points.AddXY(XAxis, MemStartsReportSectionInst.AllocUsedKB);
                        }
// 							if (SysMem_AllocPureOverheadKB != null)
// 							{
// 								SysMem_AllocPureOverheadKB.Points.AddXY(XAxis, MemStartsReportSectionInst.AllocPureOverheadKB);
// 							}

						if (SysMem_PhysicalFreeKB != null)
						{
                            SysMem_PhysicalFreeKB.Points.AddXY(XAxis, MemStartsReportSectionInst.GetValue("iOS_PhysicalFreeMem"));
						}
						if (SysMem_PhysicalUsedKB != null)
						{
							SysMem_PhysicalUsedKB.Points.AddXY(XAxis, MemStartsReportSectionInst.GetValue("iOS_PhysicalUsedMem"));
						}
						if (SysMem_TaskResidentKB != null)
						{
							SysMem_TaskResidentKB.Points.AddXY(XAxis, MemStartsReportSectionInst.GetValue("iOS_TaskResident"));
						}
						if (SysMem_TaskVirtualKB != null)
						{
							SysMem_TaskVirtualKB.Points.AddXY(XAxis, MemStartsReportSectionInst.GetValue("iOS_TaskVirtual"));
						}
						if (SysMem_HighestMemRecentKB != null)
						{
							SysMem_HighestMemRecentKB.Points.AddXY(XAxis, MemStartsReportSectionInst.HighestRecentMemoryAllocatedKB);
						}
						if (SysMem_HighestMemEverKB != null)
						{
							SysMem_HighestMemEverKB.Points.AddXY(XAxis, MemStartsReportSectionInst.HighestMemoryAllocatedKB);
						}
					}
				}

				XAxis++;
			}

			FinalizeChartDataFile(ref SystemMemoryChart);
			FinalizeChartDataFile(ref TextureMemoryChart);
		}

		private void DisplayedCurvesTreeView_AfterCheck(object sender, TreeViewEventArgs e)
		{
			if (FileListTree.GetNodeCount(true) > 1)
			{
				FillMemoryCharts();
			}
		}

		private void DisplayedCurvesTreeView_NodeMouseDoubleClick(object sender, TreeNodeMouseClickEventArgs e)
		{
			//@todo. Pop up a color selection dialog for this curve... assuming it is *not* a top level node
		}

		bool bInChartSync = false;

		private void MemoryChart_SelectionRangeChanged(object sender, CursorEventArgs e)
		{
			if (bInChartSync == false)
			{
				Chart SourceChart = sender as Chart;
				if (SourceChart != null)
				{
					TabView.Focus();
					bInChartSync = true;
					SyncChart(ref SourceChart);
					bInChartSync = false;
					SourceChart.Invalidate();
				}
			}
		}

		private void MemoryChart_MouseClick(object sender, MouseEventArgs e)
		{
			if (bInChartSync == false)
			{
				if (e.Button == MouseButtons.Left)
				{
					Chart SourceChart = sender as Chart;
					if (SourceChart != null)
					{
						TabView.Focus();
						bInChartSync = true;
						SyncChart(ref SourceChart);
						bInChartSync = false;
						SourceChart.Invalidate();
					}
				}
				else if (e.Button == MouseButtons.Right)
				{
					// Copy the BugItGo to the clipboard
					MemLeakFile LeakFile = GetSelectedMemLeakFile();
					if (LeakFile != null)
					{
						BugItSection BugItSectionInst;
						if (LeakFile.FindFirstSection<BugItSection>(out BugItSectionInst))
						{
							Clipboard.SetText(BugItSectionInst.CoordinateString);
						}
					}
				}
			}
		}

		private string SystemMemoryChart_GetText(MemLeakFile SourceFile, string Delimiter)
		{
			string SystemMemString = "";
			if (SourceFile != null)
			{
				// Grab the system memory info
				MemStatsReportSection MemStartsReportSectionInst;
				if (SourceFile.FindFirstSection<MemStatsReportSection>(out MemStartsReportSectionInst))
				{
					if (IsChartEntryEnabled("TitleFreeKB", true) == true)
					{
						SystemMemString += "TitleFreeKB=" + MemStartsReportSectionInst.TitleFreeKB + Delimiter;
					}
					if (IsChartEntryEnabled("LowestRecentTitleFreeKB", true) == true)
					{
						SystemMemString += "LowestRecent=" + MemStartsReportSectionInst.LowestRecentTitleFreeKB + Delimiter;
					}
					if (IsChartEntryEnabled("AllocUnusedKB", true) == true)
					{
						SystemMemString += "AllocUnused=" + MemStartsReportSectionInst.AllocUnusedKB + Delimiter;
					}
                    if (IsChartEntryEnabled("AllocUsedKB", true) == true)
                    {
                        SystemMemString += "AllocUsed=" + MemStartsReportSectionInst.AllocUsedKB + Delimiter;
                    }
					//MemStartsReportSectionInst.AllocPureOverheadKB
					if (IsChartEntryEnabled("TitleFreeKB", true) == true)
					{
						SystemMemString += "TitleFreeKB=" + MemStartsReportSectionInst.TitleFreeKB + Delimiter;
					}
					if (IsChartEntryEnabled("PhysicalFreeKB", true) == true)
					{
						SystemMemString += "PhysicalFreeKB=" + MemStartsReportSectionInst.GetValue("iOS_PhysicalFreeMem") + Delimiter;
					}
					if (IsChartEntryEnabled("PhysicalUsedKB", true) == true)
					{
						SystemMemString += "PhysicalUsedKB=" + MemStartsReportSectionInst.GetValue("iOS_PhysicalUsedMem") + Delimiter;
					}
					if (IsChartEntryEnabled("TaskResidentKB", true) == true)
					{
                        SystemMemString += "TaskResidentKB=" + MemStartsReportSectionInst.GetValue("iOS_TaskResident") + Delimiter;
					}
					if (IsChartEntryEnabled("TaskVirtualKB", true) == true)
					{
                        SystemMemString += "TaskVirtualKB=" + MemStartsReportSectionInst.GetValue("iOS_TaskVirtual") + Delimiter;
					}
					if (IsChartEntryEnabled("HighestMemEverKB", true) == true)
					{
						SystemMemString += "HighestMemEverKB=" + MemStartsReportSectionInst.HighestMemoryAllocatedKB + Delimiter;
					}
					if (IsChartEntryEnabled("HighestMemRecentlyKB", true) == true)
					{
						SystemMemString += "HighestMemRecentlyKB=" + MemStartsReportSectionInst.HighestRecentMemoryAllocatedKB + Delimiter;
						SystemMemString += "HighestMemOccurredAgo=" + MemStartsReportSectionInst.iOS_TaskResidentRecentPeakAgo + Delimiter;
					}
				}
			}
			return SystemMemString;
		}

		private string TextureMemoryChart_GetText(MemLeakFile SourceFile, string Delimiter)
		{
			string TextureMemString = "";
			if (SourceFile != null)
			{
				// Grab the texture info
				TextureStreamingSection TextureStreamingSectionInst;
				if (SourceFile.FindFirstSection<TextureStreamingSection>(out TextureStreamingSectionInst))
				{
					if (IsChartEntryEnabled("TexturesInMemoryCurrent", true) == true)
					{
						TextureMemString += "Current=" + TextureStreamingSectionInst.TexturesInMemoryCurrent + Delimiter;
					}
					if (IsChartEntryEnabled("TexturesInMemoryTarget", true) == true)
					{
						TextureMemString += "Target=" + TextureStreamingSectionInst.TexturesInMemoryTarget + Delimiter;
					}
					if (IsChartEntryEnabled("OverBudget", true) == true)
					{
						TextureMemString += "OverBudget=" + TextureStreamingSectionInst.OverBudget + Delimiter;
					}
					if (IsChartEntryEnabled("PoolMemoryUsed", true) == true)
					{
						TextureMemString += "Used=" + TextureStreamingSectionInst.PoolMemoryUsed + Delimiter;
					}
					if (IsChartEntryEnabled("LargestFreeMemoryHole", true) == true)
					{
						TextureMemString += "BiggestHole=" + TextureStreamingSectionInst.LargestFreeMemoryHole + Delimiter;
					}
				}
			}
			return TextureMemString;
		}

		private void SystemMemoryChart_GetToolTipText(object sender, ToolTipEventArgs e)
		{
			MemLeakFile SourceFile = GetToolTipText(sender, e, SystemMemoryChart);
			if (SourceFile != null)
			{
				string SysMemString = SystemMemoryChart_GetText(SourceFile, "\n");
				e.Text += SysMemString;
			}
		}

		private void TextureMemoryChart_GetToolTipText(object sender, ToolTipEventArgs e)
		{
			MemLeakFile SourceFile = GetToolTipText(sender, e, TextureMemoryChart);
			if (SourceFile != null)
			{
				string TextureMemString = TextureMemoryChart_GetText(SourceFile, "\n");
				e.Text += TextureMemString;
			}
		}

		private MemLeakFile GetToolTipText(object sender, ToolTipEventArgs e, Chart SourceChart)
		{
			TreeNode NodeToFillFrom = GetNodeToFillChartsFrom();
			if (NodeToFillFrom == null)
			{
				return null;
			}

			int FrameIndex = (int)SourceChart.ChartAreas[0].AxisX.PixelPositionToValue(e.X);
			e.Text = "";

			// Find the memleak file corresponding to this position
			MemLeakFile ReturnFile = null;
			int CheckIndex = 0;
			foreach (TreeNode FileNode in NodeToFillFrom.Nodes)
			{
				// Assuming a one-deep heirarchy here...
				MemLeakFile LeakFile = FileNode.Tag as MemLeakFile;
				if (LeakFile != null)
				{
					if (CheckIndex == FrameIndex)
					{
						ReturnFile = LeakFile;
						break;
					}
					CheckIndex++;
				}
			}

			if (ReturnFile != null)
			{
				string Filename = ReturnFile.Filename;
				int LastSlashIdx = Filename.LastIndexOf('\\');
				int MemlkIdx = Filename.LastIndexOf(".memlk");
				if (LastSlashIdx != -1)
				{
					if (MemlkIdx != -1)
					{
						Filename = Filename.Substring(LastSlashIdx + 1, MemlkIdx - LastSlashIdx - 1);
					}
					else
					{
						Filename = Filename.Substring(LastSlashIdx + 1);
					}
				}

				e.Text = Filename + "\n";
			}
			else
			{
				e.Text = "File not found...";
			}

			return ReturnFile;
		}

		private void SyncChart(ref Chart SourceChart)
		{
			if (SourceChart == SystemMemoryChart)
			{
				SyncCharts(ref SourceChart, ref TextureMemoryChart);
			}
			else if (SourceChart == TextureMemoryChart)
			{
				SyncCharts(ref SourceChart, ref SystemMemoryChart);
			}
		}

		private void SyncCharts(ref Chart SourceChart, ref Chart ChartToSync)
		{
			// Determine the selection region of the source and set it on the other
			// Also need to set the selected point as well
			ChartToSync.ChartAreas[0].AxisX.ScaleView = SourceChart.ChartAreas[0].AxisX.ScaleView;
			ChartToSync.ChartAreas[0].CursorX.Position = SourceChart.ChartAreas[0].CursorX.Position;
			ChartToSync.Invalidate();

			UpdateStreamingLevelList();
			UpdateGroupList();
			UpdateChartTitles();
		}

		private void MoveChartCursor(int Amount)
		{
			if (SystemMemoryChart.Series.Count > 0)
			{
				// We know that the x-axis is simply the number of series in the chart...
				int MaxCount = SystemMemoryChart.Series[0].Points.Count - 1;

				SystemMemoryChart.ChartAreas[0].CursorX.Position += Amount;
				if (SystemMemoryChart.ChartAreas[0].CursorX.Position < 0)
				{
					SystemMemoryChart.ChartAreas[0].CursorX.Position = 0;
				}
				else if (SystemMemoryChart.ChartAreas[0].CursorX.Position > MaxCount)
				{
					SystemMemoryChart.ChartAreas[0].CursorX.Position = MaxCount;
				}
				SyncCharts(ref SystemMemoryChart, ref TextureMemoryChart);
				SystemMemoryChart.Invalidate();
			}
		}

		private void UpdateChartTitles()
		{
			MemLeakFile LeakFile = GetSelectedMemLeakFile();
			if (LeakFile != null)
			{
				UpdateSystemMemoryChartTitle(LeakFile);
				UpdateTextureMemoryChartTitle(LeakFile);
			}
		}

		private void UpdateSystemMemoryChartTitle(MemLeakFile LeakFile)
		{
			if (LeakFile != null)
			{
				// Grab the system memory info
				string SysMemString = SystemMemoryChart_GetText(LeakFile, "; ");
				SystemMemoryChart.Titles[0].Text = "System Memory: " + SysMemString;

				BugItSection BugItSectionInst;
				if (LeakFile.FindFirstSection<BugItSection>(out BugItSectionInst))
				{
					if (SystemMemoryChart.Titles[0].Text.Contains("HighestMemOccurredAgo") == false)
					{
						SystemMemoryChart.Titles[0].Text += "\n";
					}
					SystemMemoryChart.Titles[0].Text += BugItSectionInst.CoordinateString;
				}
			}
		}

		private void UpdateTextureMemoryChartTitle(MemLeakFile LeakFile)
		{
			if (LeakFile != null)
			{
				// Grab the texture memory info
				string TextureMemString = TextureMemoryChart_GetText(LeakFile, "; ");
				TextureMemoryChart.Titles[0].Text = "Texture Memory: " + TextureMemString;
			}
		}

		private void UpdateStreamingLevelList()
		{
			// fill in the list box
			MemLeakFile LeakFile = GetSelectedMemLeakFile();
			if (LeakFile != null)
			{
				LoadedLevelsSection LevelsSectionInst;
				if (LeakFile.FindFirstSection<LoadedLevelsSection>(out LevelsSectionInst))
				{
					StreamingLevelsListView.BeginUpdate();
					StreamingLevelsListView.Items.Clear();

					foreach (FLoadedLevelInfo Level in LevelsSectionInst.LoadedLevelsData)
					{
						string ListString = "";
						if (Level.bPermanent == true)
						{
							ListString = "* ";
						}
						ListString += Level.LevelName;

						ListViewItem NewItem = new ListViewItem(ListString);
						switch (Level.Status)
						{
							case ELoadedStatus.LOADSTATUS_Visible:
								NewItem.SubItems.Add("Loaded & visible");
								NewItem.ForeColor = Color.Red;
								break;
							case ELoadedStatus.LOADSTATUS_MakingVisible:
								NewItem.SubItems.Add("Being made visible");
								NewItem.ForeColor = Color.Orange;
								break;
							case ELoadedStatus.LOADSTATUS_Loaded:
								NewItem.SubItems.Add("Loaded not visible");
								NewItem.ForeColor = Color.YellowGreen;
								break;
							case ELoadedStatus.LOADSTATUS_UnloadedButStillAround:
								NewItem.SubItems.Add("Waiting for GC");
								NewItem.ForeColor = Color.Blue;
								break;
							case ELoadedStatus.LOADSTATUS_Unloaded:
								NewItem.SubItems.Add("Unloaded");
								NewItem.ForeColor = Color.Green;
								break;
							case ELoadedStatus.LOADSTATUS_Preloading:
								NewItem.SubItems.Add("Preloading");
								NewItem.ForeColor = Color.Purple;
								break;
							case ELoadedStatus.LOADSTATUS_Unknown:
							default:
								NewItem.SubItems.Add("*** UNKNOWN ***");
								NewItem.ForeColor = Color.Brown;
								break;
						}
						StreamingLevelsListView.Items.Add(NewItem);
					}
					StreamingLevelsListView.EndUpdate();
					StreamingLevelsListView.Invalidate();
				}
			}
		}

		private void AddObjectListSeparator(string TypeName)
		{
			ListViewItem EmptyItem = new ListViewItem(TypeName);
			EmptyItem.ForeColor = Color.Red;
			EmptyItem.SubItems.Add("---");
			EmptyItem.SubItems.Add("---");
			EmptyItem.SubItems.Add("---");
			ObjectListView.Items.Add(EmptyItem);
		}

		private void UpdateGroupList()
		{
			string[] SelectedItems = new String[GroupListView.SelectedItems.Count];
			int InsertIdx = 0;
			foreach (ListViewItem SelectedItem in GroupListView.SelectedItems)
			{
				SelectedItems[InsertIdx++] = SelectedItem.Text;
			}

			// fill in the list box
			MemLeakFile LeakFile = GetSelectedMemLeakFile();
			if (LeakFile != null)
			{
				ObjDumpReportSection ObjDumpSectionInst;
				if (LeakFile.FindFirstSection<ObjDumpReportSection>(out ObjDumpSectionInst))
				{
					// Fill in the group view
					GroupListView.BeginUpdate();
					GroupListView.Items.Clear();

					ListViewItem TotalItem = new ListViewItem("TOTAL");
					TotalItem.ForeColor = Color.Red;
					TotalItem.SubItems.Add(ObjDumpSectionInst.ObjectsTotalInfo.MaxkBytes.ToString());
					TotalItem.SubItems.Add(ObjDumpSectionInst.ObjectsTotalInfo.ReskBytes.ToString());
					GroupListView.Items.Add(TotalItem);
					TotalItem.Tag = ObjDumpSectionInst.ObjectsTotalInfo;

					foreach (FClassGroupResourceInfo ClassGroupResInfo in ObjDumpSectionInst.ClassGroupResourceInfoData)
					{
						ListViewItem NewItem = new ListViewItem(ClassGroupResInfo.GroupName);
						NewItem.SubItems.Add(ClassGroupResInfo.MaxkBytes.ToString());
						NewItem.SubItems.Add(ClassGroupResInfo.ReskBytes.ToString());
						for (int ReselectIdx = 0; ReselectIdx < SelectedItems.Length; ReselectIdx++)
						{
							string GroupName = SelectedItems[ReselectIdx];
							if (NewItem.Text == GroupName)
							{
								NewItem.Selected = true;
								break;
							}
						}
						NewItem.Tag = ClassGroupResInfo;
						GroupListView.Items.Add(NewItem);
					}

					GroupListView.EndUpdate();
					GroupListView.Refresh();

					UpdateObjectList();
				}
			}
		}

		private void UpdateObjectList()
		{
			if (GroupListView.SelectedItems.Count > 0)
			{
				ObjectListView.BeginUpdate();
				ObjectListView.Items.Clear();

				foreach (ListViewItem GroupItem in GroupListView.SelectedItems)
				{
					FClassGroupResourceInfo GroupResInfo = GroupItem.Tag as FClassGroupResourceInfo;
					if (GroupResInfo != null)
					{
						// Add the group and group totals as a separator
						ListViewItem NewGroupItem = new ListViewItem(GroupResInfo.GroupName);
						NewGroupItem.ForeColor = Color.Red;
						NewGroupItem.SubItems.Add("---");
						NewGroupItem.SubItems.Add(GroupResInfo.MaxkBytes.ToString());
						NewGroupItem.SubItems.Add(GroupResInfo.ReskBytes.ToString());
						ObjectListView.Items.Add(NewGroupItem);

						// Add each item in the group
						foreach (FClassResourceInfo ClassResInfo in GroupResInfo.ClassInfoData)
						{
							ListViewItem NewItem = new ListViewItem(ClassResInfo.ClassName);
							NewItem.SubItems.Add(ClassResInfo.Count.ToString());
							NewItem.SubItems.Add(ClassResInfo.MaxkBytes.ToString());
							NewItem.SubItems.Add(ClassResInfo.ReskBytes.ToString());
							ObjectListView.Items.Add(NewItem);
						}
					}
				}

				ObjectListView.EndUpdate();
				ObjectListView.Invalidate();
			}
		}

		private void GroupListView_ItemSelectionChanged(object sender, ListViewItemSelectionChangedEventArgs e)
		{
			UpdateObjectList();
		}

        private void PS3MemoryAdjuster_TextChanged(object sender, EventArgs e)
        {
            MemStatsReportSectionPS3.PS3AdjustKB = GetMemoryAdjustPS3SizeKB();
        }

		private void MemDiffer_Closed( object sender, FormClosedEventArgs e )
		{
			UnrealControls.XmlHandler.WriteXml<SettableOptions>( Options, Path.Combine( Application.StartupPath, "MemLeakCheckDiffer.Settings.xml" ), "" );
		}
	}

    /// <summary>
    /// This class just derives from ListView to turn on double buffering, which is protected by default.
    /// </summary>
    public class BufferedListView : ListView
    {
        public BufferedListView()
            : base()
        {
            SetStyle(ControlStyles.OptimizedDoubleBuffer, true);
        }
    }

	public class SettableOptions
	{
		[XmlElement]
		public string GameName
		{
			get;
			set;
		}

		public SettableOptions()
		{
			GameName = "GearGame";
		}
	}
}
