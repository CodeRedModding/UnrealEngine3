// 	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Text.RegularExpressions;
using System.Configuration;


namespace ProfNodeVisualizer
{
    public partial class Form1 : Form
    {
        public Form1()
        {
            InitializeComponent();

            SettingRootLevelDisplayThreshold = Properties.Settings.Default.RootLevelDisplayThreshold;
            UpdateRootThresholdTime();
        }

        private List<ProfData> CurrentProfData;
        static public float SettingRootLevelDisplayThreshold = 0;

        #region Log File Parsing
        static List<ProfData> ParseLogFile(string FileName)
        {
            var FileData = (from L in File.ReadLines(FileName)
                            let X = L.Split(",".ToCharArray())
                            where ((L.Length > 0) && (Regex.IsMatch(L, "^.*ProfNode.*$")) && (X.Length == 4) && (X[3].Trim().Length > 0))
                            select new ProfData()
                            {
                                Thread  = Convert.ToUInt32(X[0].Split(':')[2], 16),
                                Depth   = Convert.ToUInt32(X[1].Trim(), 10),
                                Name    = X[2].Trim(),
                                Time    = Convert.ToSingle(X[3].Trim())
                            }).ToList();

            return FileData;
        }
        #endregion

        #region Converting File Data To Tree Data
        static private void AddListToProfData(ProfData Node, List<ProfData> Data, UInt32 Depth)
        {
            int Index = 0;
            AddListToProfDataHelper(Node, Data, Depth, ref Index);
            ReverseChildren(Node);
        }

        static private void ReverseChildren(ProfData Node)
        {
            foreach (ProfData n in Node.Children)
            {
                ReverseChildren(n);
            }

            if (Node.Children.Count > 0)
            {
                Node.Children.Reverse();
            }
        }

        static private void AddListToProfDataHelper(ProfData Node, List<ProfData> Data, UInt32 Depth, ref int Index)
        {
            for (; Index < Data.Count; ++Index)
            {
                if (Data[Index].Depth == Depth)
                {
                    Node.Children.Add(new ProfData(Data[Index]));
                    Node.Children[Node.Children.Count - 1].Parent = Node;
                }
                else if (Data[Index].Depth > Depth)
                {
                    if (Node.Children.Count == 0)
                    {
                        Node.Children.Add(new ProfData());
                    }
                    AddListToProfDataHelper(Node.Children[Node.Children.Count - 1], Data, Depth + 1, ref Index);
                }
                else if (Data[Index].Depth < Depth)
                {
                    --Index;
                    break;
                }
            }
        }

        static List<ProfData> ConvertFileDataToTree(List<ProfData> FileData)
        {
            Dictionary<UInt32, List<ProfData>> ThreadGroups = new Dictionary<UInt32, List<ProfData>>();

            // split profiling data into thread groups
            foreach (ProfData Data in FileData)
            {
                if (!ThreadGroups.ContainsKey(Data.Thread))
                {
                    ThreadGroups.Add(Data.Thread, new List<ProfData>());
                }

                ThreadGroups[Data.Thread].Add(Data);
            }

            // trim out unparented profiling data
            foreach (var Data in ThreadGroups)
            {
                for (int Index = Data.Value.Count - 1; Index >= 0; --Index)
                {
                    if (Data.Value[Index].Depth == 0)
                    {
                        break;
                    }
                    else
                    {
                        Data.Value.RemoveAt(Index);
                    }
                }
            }

            // Put the data into a tree
            List<ProfData> ThreadProfDataList = new List<ProfData>();
            foreach (var Data in ThreadGroups)
            {
                Data.Value.Reverse();

                ThreadProfDataList.Add(new ProfData { Thread = Data.Value[0].Thread, Depth = 0, Name = "Thread: 0x" + Data.Value[0].Thread.ToString("x"), Children = new List<ProfData>() });

                AddListToProfData(ThreadProfDataList[ThreadProfDataList.Count - 1], Data.Value, 0);

                for (int Index = 0; Index < Data.Value.Count; ++Index)
                {
                    if (Data.Value[Index].Depth == 0)
                    {
                        break;
                    }
                    else
                    {
                        Data.Value.RemoveAt(Index);
                    }
                }
            }

            // fill out top level thread times
            foreach (var Data in ThreadProfDataList)
            {
                float TotalTime = 0.0f;
                foreach (var ChildData in Data.Children)
                {
                    TotalTime += ChildData.Time;
                }

                Data.Time = TotalTime;
            }

            return ThreadProfDataList;
        }
        #endregion

        #region Filling Out TreeView
        static private void ProfDataToTreeView(TreeNode ParentNode, List<ProfData> ProfDataTree)
        {
            foreach (var Data in ProfDataTree)
            {
                // If the time is above the threshold level and we don't have a parent (so we know we are  a top level node)
                if (Data.Time > SettingRootLevelDisplayThreshold && Data.Parent != null)
                {
                    ParentNode.Nodes.Add(Data.ToString());
                    float ColorPercent = (1.0f - (Data.Time / Data.Parent.Time));
                    // Make sure that any value greather than 0.5 seconds has a minimum highlighting color
                    if (Data.Time > 0.5f)
                    {
                        ColorPercent = Math.Min(0.75f, ColorPercent);
                    }
                    ParentNode.LastNode.BackColor = Color.FromArgb(255, (Int32)(Color.White.G * ColorPercent), (Int32)(Color.White.B * ColorPercent));

                    if (Data.Children.Count > 0)
                    {
                        ProfDataToTreeView(ParentNode.LastNode, Data.Children);
                    }
                }
            }
        }

        static private void ProfDataToTreeView(TreeNodeCollection Parent, List<ProfData> ProfDataTree)
        {
            if (ProfDataTree == null)
            {
                return;
            }

            foreach (var Data in ProfDataTree)
            {
                TreeNode Node = Parent.Add(Data.ToString());

                if (Data.Children.Count > 0)
                {
                    ProfDataToTreeView(Node, Data.Children);
                }
            }
        }

        private void UpdateTreeView()
        {
            ProfNodeView.BeginUpdate();
            ProfNodeView.Nodes.Clear();

            ProfDataToTreeView(ProfNodeView.Nodes, CurrentProfData);
                
            ProfNodeView.EndUpdate();
        }

        private void UpdateRootThresholdTime()
        {
            ToolStripStatusLabel1.Text = string.Format("Root Threshold Time: {0}", SettingRootLevelDisplayThreshold);
        }

        private void LoadFileData(string FileName)
        {
            var FileData = ParseLogFile(FileName);
            CurrentProfData = ConvertFileDataToTree(FileData);
        }
        #endregion

        #region Events
        private void ProfNodeViewDragDrop(object sender, DragEventArgs e)
        {
            if (e.Data.GetDataPresent("FileDrop"))
            {
                string FileName = ((String[])e.Data.GetData("FileDrop"))[0];
                LoadFileData(FileName);
                UpdateTreeView();
            }
        }

        private void ProfNodeViewDragOver(object sender, DragEventArgs e)
        {
            e.Effect = DragDropEffects.Copy;
        }
        #endregion

        #region Menu Items
        private void LoadFileToolStripMenuItemClick(object sender, EventArgs e)
        {
            OpenFileDialog dialog = new OpenFileDialog();
            if (dialog.ShowDialog() == DialogResult.OK)
            {
                LoadFileData(dialog.FileName);
            }
        }

        private void ExitToolStripMenuItemClick(object sender, EventArgs e)
        {
            Close();
        }
        #endregion

        #region Searching...
        static private void SearchTreeNodes(string SearchTerm, TreeView Tree, TreeNodeCollection Nodes)
        {
            foreach (TreeNode Tn in Nodes)
            {
                if (Tn.Text.ToLower().Contains(SearchTerm))
                {
                    Tree.SelectedNode = Tn;
                }

                if (Tn.Nodes.Count > 0)
                {
                    SearchTreeNodes(SearchTerm, Tree, Tn.Nodes);
                }
            }
        }

        private void ToolStripSearchBoxKeyDown(object sender, KeyEventArgs e)
        {
            if (ToolStripTextBox1.Text.Length >= 3)
            {
                string SearchTerm = ToolStripTextBox1.Text.ToLower();
                ProfNodeView.CollapseAll();
                SearchTreeNodes(SearchTerm, ProfNodeView, ProfNodeView.Nodes);
            }
        }
        #endregion

        #region Node Exporting
        static private void WriteString(FileStream Out, string OutString)
        {
            byte[] Bytes = System.Text.Encoding.UTF8.GetBytes(OutString);
            Out.Write(Bytes, 0, Bytes.Length);
        }

        static private void ExportNode(FileStream Out, TreeNode Node, Int32 Depth)
        {
            for (Int32 i = 0; i < Depth; ++i)
            {
                WriteString(Out, "\t");
            }

            WriteString(Out, Node.Text);
            WriteString(Out, "\n");
            
            foreach (TreeNode ChildNode in Node.Nodes)
            {
                ExportNode(Out, ChildNode, Depth + 1);
            }
        }

        private void ExportFromHereToolStripMenuItemClick(object sender, EventArgs e)
        {
            TreeNode SelectedNode = ProfNodeView.SelectedNode;

            if (SelectedNode == null)
            {
                return;
            }

            // show file selection dialog to find out where to export it to
            SaveFileDialog SaveDialog = new SaveFileDialog();
            SaveDialog.Filter = "Text|*.txt|All|*.*";
            SaveDialog.Title = "Export node data";
            SaveDialog.ShowDialog();

            // export the nodes
            if (SaveDialog.FileName != "")
            {
                FileStream Out = (FileStream)SaveDialog.OpenFile();

                ExportNode(Out, SelectedNode, 0);

                Out.Close();
            }
        }
    #endregion

        private void ToolStripTextBoxClick(object sender, EventArgs e)
        {
            ToolStripTextBox1.Text = string.Empty;
        }

        private void ToolStripStatusLabelClick(object sender, EventArgs e)
        {
            ShowRootDisplayThresholdForm();
        }

        private void FormFormClosed(object sender, FormClosedEventArgs e)
        {
            Properties.Settings.Default.Save();
        }

        private void ProfNodeViewNodeMouseClick(object sender, TreeNodeMouseClickEventArgs e)
        {
            TreeNode Node = e.Node;

            while (Node.Parent != null && Node.Parent.Parent != null)
            {
                Node = Node.Parent;
            }

            if (Node != null && Node.Parent != null)
            {
                UInt32 ThreadId = Convert.ToUInt32(Regex.Matches(Node.Parent.Text, @".*(0x[\dabcdef]*).*")[0].Groups[1].Value,16);
                Single TotalTime = 0;

                String NodeTime = "0.0";
                String NodeName = "Test";

                MatchCollection Matches = Regex.Matches(Node.Text, @"(.*) - ([0-9\.]*) s");

                if (Matches.Count == 1 && Matches[0].Groups.Count == 3)
                {
                    NodeName = Matches[0].Groups[1].Value;
                    NodeTime = Matches[0].Groups[2].Value;
                }

                for (int i = 0; i < CurrentProfData.Count; ++i)
                {
                    if (CurrentProfData[i].Thread == ThreadId)
                    {
                        for (int j = 0; j < CurrentProfData[i].Children.Count; ++j)
                        {
                            TotalTime += CurrentProfData[i].Children[j].Time;
                            String FormattedTime = String.Format("{0:f6}", CurrentProfData[i].Children[j].Time);
                            if ((FormattedTime == NodeTime) && (CurrentProfData[i].Children[j].Name == NodeName))
                            {
                                break;
                            }
                        }
                        break;
                    }
                }

                TimeToCursorLabel.Text = string.Format("Time to Curosr: {0:f6}", TotalTime);
            };
        }

        private void SetRootDisplayThresholdToolStripMenuItemClick(object sender, EventArgs e)
        {
            ShowRootDisplayThresholdForm();
        }

        private void ShowRootDisplayThresholdForm()
        {
            InputForm Dialog = new InputForm();
            Dialog.Text = "Root Threshold Time";
            Dialog.InputFormLabel.Text = "Root Threshold Time:";
            DialogResult Result = Dialog.ShowDialog();

            if (Result == DialogResult.OK)
            {
                Properties.Settings.Default.RootLevelDisplayThreshold = Convert.ToSingle(Dialog.TextBox.Text);
                SettingRootLevelDisplayThreshold = Properties.Settings.Default.RootLevelDisplayThreshold;
                UpdateRootThresholdTime();
                UpdateTreeView();
            }
        }

        protected override bool ProcessCmdKey(ref Message msg, Keys keyData)
        {
            if (keyData.HasFlag(Keys.Control))
            {
                if (keyData.HasFlag(Keys.F))
                {
                    ToolStripTextBox1.Focus();
                    return true;
                }
            }

            return base.ProcessCmdKey(ref msg, keyData);
        }
    }

    class ProfData
    {
        public UInt32         Thread;
        public UInt32         Depth;
        public string         Name;
        public float          Time;
        public List<ProfData> Children;
        public ProfData       Parent;

        public ProfData()
        {
            Children = new List<ProfData>();
        }

        public ProfData(ProfData Data)
        {
            Thread   = Data.Thread;
            Depth    = Data.Depth;
            Name     = Data.Name;
            Time     = Data.Time;
            Children = Data.Children;
        }

        public override string ToString()
        {
            return string.Format("{0} - {1:f6} s", Name, Time);
        }
    }
}
