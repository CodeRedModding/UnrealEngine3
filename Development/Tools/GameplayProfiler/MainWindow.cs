using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;
using System.Windows.Forms.DataVisualization.Charting;

namespace GameplayProfiler
{
    public partial class MainWindow : Form
    {
        public MainWindow( string FileName )
        {
            InitializeComponent();

			FrameFunctionListView.ListViewItemSorter = new ListViewItemComparer( 1 );
			AggregateFunctionListView.ListViewItemSorter = new ListViewItemComparer( 1 );

			// Allow passing in file via command line.
			if( FileName != null )
			{
				ChangeFileInternal( FileName );
			}
        }

		private ProfilerStream CurrentProfilerStream = null;

		/**
		 * Generic file open dialog to pick file to use.
		 */
		private void FileOpen_Click(object sender, EventArgs e)
		{
			if( OpenFileDialog.ShowDialog() == DialogResult.OK )
			{
				ChangeFileInternal( OpenFileDialog.FileName );
			}
		}

		/**
		 * Change currently displayed/ used file.
		 */
		private void ChangeFileInternal( string FileName )
		{
			CurrentProfilerStream = StreamParser.Parse( File.OpenRead( FileName ) );
			ChartParser.Parse( FPSChart, CurrentProfilerStream );

			UpdateAggregateData();
            
			// Set the filename currently being viewed so the user knows which file it is
            Text = "Gameplay Profiler - " + FileName;
            
			// This will force an update
            FrameNumTextBox.Text = "0";

            OpenFileDialog.FileName = FileName;
        }


		/**
		 * Recomputes aggregate data
		 */
		private void UpdateAggregateData()
		{
			float Threshold = (float)TimeThresholdBox.Value;
			bool bShowCycleStats = ShowCycleStatsCheckBox.Checked;
			if( CurrentProfilerStream != null )
			{
				FunctionInfo.DumpFunctionInfoMapToList( CurrentProfilerStream, CurrentProfilerStream.AggregateNameToFunctionInfoMap, AggregateFunctionListView, 1, true, bShowCycleStats );
				FunctionTreeViewParser.Parse( AggregateFunctionCallGraph, CurrentProfilerStream, 0, false, Threshold, bShowCycleStats );
			}
		}

		/**
		 * Display/ update views for passed in frame.
		 * 
		 * @param	SelectedFrame	Frame to display
		 */
		private void DisplayFrame( int SelectedFrame )
		{
			float Threshold = (float) TimeThresholdBox.Value;
			bool bShowCycleStats = ShowCycleStatsCheckBox.Checked;
			ActorTreeViewParser.Parse( PerActorTreeView, PerClassTreeView, CurrentProfilerStream, SelectedFrame, Threshold, bShowCycleStats );
			FunctionInfo.DumpFunctionInfoMapToList(CurrentProfilerStream,CurrentProfilerStream.Frames[SelectedFrame].NameToFunctionInfoMap, FrameFunctionListView, Threshold, false, bShowCycleStats);
			ClassHierarchyTreeViewParser.Parse(ClassHierarchyTreeView, CurrentProfilerStream, SelectedFrame, Threshold);
			FunctionTreeViewParser.Parse(FrameFunctionCallGraph, CurrentProfilerStream, SelectedFrame, true, Threshold, bShowCycleStats);
		}

		/**
		 * Handler for selecting a frame in the chart
		 * 
		 * @param	Sender	unused
		 * @param	Event	Event arguments.	
		 */		
		private void FPSChart_SelectionRangeChanged(object Sender, CursorEventArgs Event)
		{
			if(Event.Axis.AxisName == AxisName.X)
			{
				// Disregard event when zooming or invalid (NaN) selection.
				if( Event.NewSelectionStart != Event.NewSelectionEnd )
				{
				}
				// Single frame is selected, parse it!
				else
				{
					int SelectedFrame = (int) Event.NewSelectionStart;
                    string SelectedFrameText = SelectedFrame.ToString();
                    // Don't update if the values are the same
                    if (SelectedFrameText != FrameNumTextBox.Text)
                    {
                        FrameNumTextBox.Text = SelectedFrameText;
                    }
				}
			}
		}


		/**
		 * Comparator used to sort the columns of a list view.
		 */
		public class ListViewItemComparer : System.Collections.IComparer 
		{
			/** Column to sort by. */
			private int	Column;
		
			/**
			 * Constructor
			 * 
			 * @param	InColumn	Column to sort by
			 */
			public ListViewItemComparer(int InColumn) 
			{
				Column	= InColumn;
			}
			
			/**
			 * Compare function
			 */
			public int Compare( object A, object B ) 
			{			
				var TextA = ((ListViewItem) A).SubItems[Column].Text;
				var TextB = ((ListViewItem) B).SubItems[Column].Text;

				// Numbers.
				if( Column > 0 )
				{
					var ValueA = float.Parse(TextA);
					var ValueB = float.Parse(TextB);
					return Math.Sign( ValueB - ValueA );
				}
				// Text.
				else
				{
					return String.Compare( TextA, TextB );
				}
			}
		}

		/**
		 * Handler for clicking on the columns of the per frame function list view.
		 * 
		 * @param	Sender	unused
		 * @param	Event	Event arguments.	
		 */		
		private void FrameFunctionListView_ColumnClick( object Sender, System.Windows.Forms.ColumnClickEventArgs Event )
		{
			// Sort function name A-Z, others in descending order.
			FrameFunctionListView.ListViewItemSorter = new ListViewItemComparer( Event.Column );
			FrameFunctionListView.Sort();
		}

		/**
		 * Handler for clicking on the columns of the aggregate function list view.
		 * 
		 * @param	Sender	unused
		 * @param	Event	Event arguments.	
		 */		
		private void AggregateFunctionListView_ColumnClick( object Sender, ColumnClickEventArgs Event )
		{
			// Sort function name A-Z, others in descending order.
			AggregateFunctionListView.ListViewItemSorter = new ListViewItemComparer( Event.Column );
			AggregateFunctionListView.Sort();
		}

        /// <summary>
        /// Sets the frame number to the one the user entered
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void FrameNumTextBox_TextChanged(object sender, EventArgs e)
        {
            int FrameNum = 0;

			if( CurrentProfilerStream != null )
			{
				if( FrameNumTextBox.Text.Length > 0 )
				{
					// Convert the entry into a frame that can be displayed
					FrameNum = Convert.ToInt32( FrameNumTextBox.Text );
					// If this is a valid frame
					if( FrameNum >= 0 && FrameNum < CurrentProfilerStream.Frames.Count )
					{
						DisplayFrame( FrameNum );
					}
				}
			}
        }

        private void AggregateFunctionListView_ItemSelectionChanged_1(object sender, ListViewItemSelectionChangedEventArgs e)
        {
            // Update the selected graph lines to show when the selected function(s) were used and for how much time.
            HashSet<string> FunctionNames = new HashSet<string>();
            foreach (ListViewItem Item in AggregateFunctionListView.SelectedItems)
            {
                FunctionNames.Add(Item.Text);
            }

            ChartParser.ShowFunctionGraph(FPSChart, CurrentProfilerStream, FunctionNames);
        }

        private void AggregateFunctionListView_SelectedIndexChanged(object sender, EventArgs e)
        {

        }

        private void FPSChart_GetToolTipText(object sender, ToolTipEventArgs e)
        {
            if (CurrentProfilerStream == null)
            {
                return;
            }

            int FrameIndex = (int)FPSChart.ChartAreas[0].AxisX.PixelPositionToValue(e.X);

            e.Text = "";

            if ((FrameIndex >= 0) && (FrameIndex < CurrentProfilerStream.Frames.Count))
            {
                Frame FrameInfo = CurrentProfilerStream.Frames[FrameIndex];

                e.Text += "Frame #" + FrameIndex.ToString() + " (" + FrameInfo.FrameTime.ToString() + " ms)\n";
            }

            //FPSChart.Series.
            if (e.HitTestResult != null)
            {
                if (e.HitTestResult.ChartArea != null)
                {
                    //double X = e.HitTestResult.ChartArea.AxisX.PixelPositionToValue(e.X);
                }

                Series s = e.HitTestResult.Series;
                if (s != null)
                {
                    e.Text += s.ToolTip + "\n";
                }
            }
        }

        private void AggregateFunctionCallGraph_AfterSelect(object sender, TreeViewEventArgs e)
        {
            // Gather the selected functions
            HashSet<string> FuncNames = new HashSet<string>();
            TreeNode N = AggregateFunctionCallGraph.SelectedNode;
            if (N != null)
            {
	            FunctionTreeViewParser.NodePayload payload = N.Tag as FunctionTreeViewParser.NodePayload;
				FuncNames.Add( payload.Name );
            }

            // Update the selected graph lines to show when the selected function(s) were used and for how much time.
            ChartParser.ShowFunctionGraph(FPSChart, CurrentProfilerStream, FuncNames);
        }

        private void TimeThresholdBox_ValueChanged(object sender, EventArgs e)
        {
			// Update aggregate data
			UpdateAggregateData();

			// Update per frame data
			FrameNumTextBox_TextChanged( sender, e );
        }

		private void ShowCycleStatsCheckBox_CheckChanged( object sender, EventArgs e )
		{
			// Update aggregate data
			UpdateAggregateData();

			// Update per frame data
			FrameNumTextBox_TextChanged( sender, e );
		}
	}

	/// <summary>
	/// Colors for various table data entries
	/// </summary>
	public static class TableEntryColors
	{
		public static Color ClassColor = Color.DarkOrange;
		public static Color FunctionColor = Color.DarkBlue;
		public static Color ActorColor = Color.DarkGreen;
		public static Color ComponentColor = Color.DarkOliveGreen;
		public static Color CycleStatColor = Color.DarkRed;
		public static Color LevelColor = Color.DarkViolet;
	}
}
