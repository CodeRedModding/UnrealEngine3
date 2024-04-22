/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using Stats;

namespace StatsViewer
{
	/// <summary>
	/// This view provides a call graph view of the stats data on a per frame
	/// basis. It's used to drill down through a set of stats in order to find
	/// spikes in a given stat's time.
	/// </summary>
	public class PerFrameView : System.Windows.Forms.Form
	{
		/// <summary>
		/// The frame window that created this view. This window must be notified
		/// when this form is closed.
		/// </summary>
		private StatsViewerFrame StatsViewer;
		#region Windows Form Designer generated code
		private FTreeListView CallGraphTree;
        private ToolStrip toolStrip1;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		#endregion

		private ArrayList Frames;
		private CheckBox ShowHierarchy;

        private string SelectedStat = null;

        /// <summary>
        /// Check box preference for whether to hide calls whose inclusive time is less than a
        /// the threshold set by MinTimeForDisplay
        /// </summary>
		private CheckBox HideQuickCalls;

        /// <summary>
        /// Minimum inclusive time in milliseconds for stat to be displayed in graph when
        /// 'Hide Quick Calls' is checked
        /// </summary>
        private float MinTimeForQuickCallDisplay = 0.0009f;

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(PerFrameView));
            this.CallGraphTree = new StatsViewer.FTreeListView();
            this.toolStrip1 = new System.Windows.Forms.ToolStrip();
            this.ShowHierarchy = new System.Windows.Forms.CheckBox();
            this.HideQuickCalls = new System.Windows.Forms.CheckBox();
            this.SuspendLayout();
            // 
            // CallGraphTree
            // 
            this.CallGraphTree.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.CallGraphTree.BackColor = System.Drawing.SystemColors.Window;
            this.CallGraphTree.Font = new System.Drawing.Font("Microsoft Sans Serif", 8.25F);
            this.CallGraphTree.Location = new System.Drawing.Point(0, 25);
            this.CallGraphTree.Name = "CallGraphTree";
            this.CallGraphTree.SelectedNode = null;
            this.CallGraphTree.Size = new System.Drawing.Size(817, 514);
            this.CallGraphTree.TabIndex = 0;
            this.CallGraphTree.Click += new System.EventHandler(this.CallGraphTree_Click);
            // 
            // toolStrip1
            // 
            this.toolStrip1.Location = new System.Drawing.Point(0, 0);
            this.toolStrip1.Name = "toolStrip1";
            this.toolStrip1.RenderMode = System.Windows.Forms.ToolStripRenderMode.System;
            this.toolStrip1.Size = new System.Drawing.Size(817, 25);
            this.toolStrip1.TabIndex = 1;
            this.toolStrip1.Text = "toolStrip1";
            // 
            // ShowHierarchy
            // 
            this.ShowHierarchy.AutoSize = true;
            this.ShowHierarchy.Checked = true;
            this.ShowHierarchy.CheckState = System.Windows.Forms.CheckState.Checked;
            this.ShowHierarchy.Location = new System.Drawing.Point(12, 4);
            this.ShowHierarchy.Name = "ShowHierarchy";
            this.ShowHierarchy.RightToLeft = System.Windows.Forms.RightToLeft.Yes;
            this.ShowHierarchy.Size = new System.Drawing.Size(101, 17);
            this.ShowHierarchy.TabIndex = 2;
            this.ShowHierarchy.Text = "Show Hierarchy";
            this.ShowHierarchy.UseVisualStyleBackColor = true;
            this.ShowHierarchy.CheckedChanged += new System.EventHandler(this.OnShowHierarchy);
            // 
            // HideQuickCalls
            // 
            this.HideQuickCalls.AutoSize = true;
            this.HideQuickCalls.Checked = true;
            this.HideQuickCalls.CheckState = System.Windows.Forms.CheckState.Checked;
            this.HideQuickCalls.Location = new System.Drawing.Point(139, 4);
            this.HideQuickCalls.Name = "HideQuickCalls";
            this.HideQuickCalls.RightToLeft = System.Windows.Forms.RightToLeft.Yes;
            this.HideQuickCalls.Size = new System.Drawing.Size(104, 17);
            this.HideQuickCalls.TabIndex = 3;
            this.HideQuickCalls.Text = "Hide Quick Calls";
            this.HideQuickCalls.UseVisualStyleBackColor = true;
            this.HideQuickCalls.CheckedChanged += new System.EventHandler(this.OnHideQuickCalls);
            // 
            // PerFrameView
            // 
            this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
            this.ClientSize = new System.Drawing.Size(817, 539);
            this.Controls.Add(this.HideQuickCalls);
            this.Controls.Add(this.ShowHierarchy);
            this.Controls.Add(this.toolStrip1);
            this.Controls.Add(this.CallGraphTree);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.Name = "PerFrameView";
            this.Text = "Call Graph";
            this.Closing += new System.ComponentModel.CancelEventHandler(this.PerFrameView_Closing);
            this.ResumeLayout(false);
            this.PerformLayout();

		}
		#endregion

		/// <summary>
		/// Constructs the new view for the specified range of frames
		/// </summary>
		/// <param name="Owner">The window that owns this view</param>
		/// <param name="Frames">The list of frames to add to the view</param>
		public PerFrameView(StatsViewerFrame Owner, int FirstFrameNumber, ArrayList InFrames)
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			// Used to notify the owner when we close
			StatsViewer = Owner;

			// Build the tree(s) from the data
            Frames = InFrames;

			// Update window caption
			if( InFrames.Count > 1 )
			{
				Text = "Call Graph: Multiple Frames";
			}
			else if( InFrames.Count > 0 )
			{
				Frame MyFrame = Frames[ 0 ] as Frame;
				Text = "Call Graph: Frame " + ( Convert.ToInt32( MyFrame.FrameNumber ) - FirstFrameNumber ).ToString();
			}

			
			// Generate tree list view content
			BuildTreeData( Frames );
		}


		private FTreeListViewItem FindNode( FTreeListViewItem.FTreeListViewItemNodeCollection Nodes, string Name )
        {
			foreach( FTreeListViewItem ExistingNode in Nodes )
            {
                if (ExistingNode.Text == Name)
                {
                    return ExistingNode;
                }
            }
            return null;
        }

		private FTreeListViewItem FindNodeByTag( FTreeListViewItem.FTreeListViewItemNodeCollection Nodes, string Tag )
        {
			foreach( FTreeListViewItem ExistingNode in Nodes )
            {
				Stat ExistingStat = ExistingNode.Tag as Stat;
                if ( ExistingStat.Name == Tag)
                {
                    return ExistingNode;
                }
            }
            return null;
        }

		/// <summary>
		/// Builds the call graph tree using the per frame data
		/// </summary>
		/// <param name="Frames">The list of frames to add to the tree</param>
		private void BuildTreeData(ArrayList Frames)
		{
			// Start UI changes
			Cursor = Cursors.WaitCursor;
			CallGraphTree.BeginUpdate();
			try
			{
				CallGraphTree.Items.Clear();
				CallGraphTree.Columns.Clear();

				// Setup tree/list columns
				{
					CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "Stat Tree", 260 ) );

					if( ShowHierarchy.Checked )
					{
						// Call tree arranged by caller to callee

						// Inclusive time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "Time", 80 ) );

						// Inclusive time as percent of frame time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "% of Frame", 80 ) );

						// Inclusive time as percent of caller's inclusive time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "% of Caller", 80 ) );

						// Exclusive time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "Self Time", 80 ) );

						// Exclusive time as percent of this call's inclusive time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "Self % of Call", 80 ) );

                        // Number of calls this frame
                        CallGraphTree.Columns.Add(new FTreeListViewColumnHeader("Calls", 40));

                        // Number of calls this frame
                        CallGraphTree.Columns.Add(new FTreeListViewColumnHeader("Visual", 100));
                    }
					else
					{
						// Somewhat flat view of categorized calls

						// Inclusive time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "Time", 80 ) );

						// Inclusive time as percent of frame time
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "% of Frame", 80 ) );

						// Number of calls this frame
						CallGraphTree.Columns.Add( new FTreeListViewColumnHeader( "Calls", 40 ) );
                    
                        // Number of calls this frame
                        CallGraphTree.Columns.Add(new FTreeListViewColumnHeader("Visual", 100));
                    }
				}

				// For each frame in the list, recursively add their data
				foreach( Frame frame in Frames )
				{
					// Grab the frame time
					Stat FrameTimeStat = frame.GetFrameTimeStat();
					double FrameTimeInMS = 0.0;
					if( FrameTimeStat != null )
					{
						FrameTimeInMS = FrameTimeStat.ValueInMS;
					}


					// Add the frame number here
					FTreeListViewItem FrameNode =
						new FTreeListViewItem(
							"Frame " + frame.FrameNumber.ToString() +
							"   " +
							FrameTimeInMS.ToString( "F3" ) +
							" ms (" +
							( 1000.0 / FrameTimeInMS ).ToString( "F1" ) +
							" fps)" );
					CallGraphTree.Items.Add( FrameNode );



					// Look for all root stats in this frame
					foreach( Stat stat in frame.Stats )
					{
						// if it has no parent, it's a thread root
						if( stat.ParentStat == null && stat.Type == Stat.StatType.STATTYPE_CycleCounter )
						{
							// make a node name for the thread
							string ThreadName = "Thread " + stat.ThreadId.ToString( "X" );

							// look for a node already made for this Thread.
							FTreeListViewItem ThreadNode = FindNode( FrameNode.Nodes, ThreadName );

							// if we didn't find one, make a new one for the thread
							if( ThreadNode == null )
							{
								// Add the thread node here
								ThreadNode = new FTreeListViewItem( ThreadName );
								FrameNode.Nodes.Add( ThreadNode );
							}

							// add this thread 'root' stat to the thread node
							AddStatToNode( FrameTimeInMS, stat, false, ThreadNode );
						}
					}


					// Always expand frames
					FrameNode.Expanded = true;


					// Expand threads
					foreach( FTreeListViewItem CurThreadItem in FrameNode.Nodes )
					{
						// Always expand threads
						CurThreadItem.Expanded = true;

						if( ShowHierarchy.Checked )
						{
							// Locate the slowest branch and expand it by default
							ExpandSlowestCallBranchRecursively( CurThreadItem.Nodes );
						}
					}


				}
			}
			catch (Exception e)
			{
				Console.WriteLine("Exception: \r\n" + e.ToString());
			}
			finally
			{
				// Finalize UI changes
				CallGraphTree.EndUpdate();
				Cursor = Cursors.Default;
			}
		}



		/// <summary>
		/// Expands the slowest branch in the stat tree
		/// </summary>
		/// <param name="Stats">Root list of stats</param>
		/// <returns>The slowest leaf stat found, or null on error</returns>
		private void ExpandSlowestCallBranchRecursively( FTreeListViewItem.FTreeListViewItemNodeCollection TreeNodeItems )
		{
			double SlowestCallTimeInMS = 0.0;
			FTreeListViewItem SlowestItem = null;

			foreach( FTreeListViewItem CurItem in TreeNodeItems )
			{
				Stat CurStat = CurItem.Tag as Stat;
				if( CurStat != null )
				{
					if( CurStat.ValueInMS > SlowestCallTimeInMS )
					{
						// Ignore stats that say 'Wait' or 'Idle' in them, since these are usually
						// not typical performance targets.  Also ignore 'Self' time.
						string LowerCaseStatName = CurStat.Name.ToLower();
						if( !LowerCaseStatName.Contains( "wait" ) &&
							!LowerCaseStatName.Contains( "idle" ) &&
							!LowerCaseStatName.Contains( "perframecapture" ) &&
							LowerCaseStatName != "Self" )
						{
							SlowestCallTimeInMS = CurStat.ValueInMS;
							SlowestItem = CurItem;
						}
					}
				}
			}

			if( SlowestItem != null )
			{
				// Expand this slow item!
				SlowestItem.Expanded = true;

				if( SlowestItem.Nodes.Count > 0 )
				{
					ExpandSlowestCallBranchRecursively( SlowestItem.Nodes );
				}
				else
				{
					// Found the slowest leaf in the tree.  We're done!
				}
			}
		}

        class FInclusiveExclusiveBarGraphCell : FTreeListViewItem.FTreeListViewSubItem
        {
            public double InclusiveTime;
            public double ExclusiveTime;
            public double FrameTime;

            /// <summary>
            /// Controls whether one bar is drawn (inclusive time) or two (exclusive time + inclusive time)
            /// </summary>
            public bool IsSingleTime = false;

            protected SolidBrush ExclusiveBrush;
            protected SolidBrush InclusiveBrush;

            /// <summary>
            /// The color to draw the inclusive time bar in
            /// </summary>
            public Color InclusiveColor
            {
                get { return InclusiveBrush.Color; }
                set { InclusiveBrush.Color = value; }
            }

            /// <summary>
            /// The color to draw the exclusive time bar in
            /// </summary>
            public Color ExclusiveColor
            {
                get { return ExclusiveBrush.Color; }
                set { ExclusiveBrush.Color = value; }
            }

			public FInclusiveExclusiveBarGraphCell(FTreeListViewItem Owner) : base(Owner)
			{
                ExclusiveBrush = new SolidBrush(Color.DarkRed);
                InclusiveBrush = new SolidBrush(Color.LightPink);
			}

            /**
             * Custom drawing method.  Returns true if implemented, otherwise the default string rendering will occur.
             */
            public override bool Draw(Graphics Gfx, FTreeListViewColumnHeader Column, ref RectangleF Rect, Color BackgroundColor, Color ForeGroundColor, bool bSelected)
            {
                int InclusiveWidth = (int)((InclusiveTime / FrameTime) * Rect.Width);
                int ExclusiveWidth = (int)((ExclusiveTime / FrameTime) * Rect.Width);
                if (InclusiveWidth <= 0)
                    InclusiveWidth = 1;
                if (ExclusiveWidth <= 0)
                    ExclusiveWidth = 1;

                Rect.Width = InclusiveWidth;
                Rect.Height = Rect.Height - 2;
                Rect.Y++;
                Gfx.FillRectangle(IsSingleTime ? ExclusiveBrush : InclusiveBrush, Rect);

                if (!IsSingleTime)
                {
                    Rect.Width = ExclusiveWidth;
                    Rect.Height = Rect.Height - 2;
                    Rect.Y++;
                    Gfx.FillRectangle(ExclusiveBrush, Rect);
                }
                return true;
            }
        }

		/// <summary>
		/// Adds a stat to the tree node recursively
		/// </summary>
		/// <param name="stat"></param>
		/// <param name="Parent"></param>
		private void AddStatToNode( double FrameTimeInMS, Stat stat, bool bIsSelfStat, FTreeListViewItem Parent )
		{
			// Compute this call's inclusive time as a percent of total frame time
			double PercentOfFrameTime = 100.0;
			if( FrameTimeInMS > 0.0 )
			{
				PercentOfFrameTime = stat.ValueInMS / FrameTimeInMS * 100.0;
			}

			
			if( ShowHierarchy.Checked )
			{
				if( stat.CallsPerFrame > 0 &&
					( !HideQuickCalls.Checked || stat.ValueInMS > MinTimeForQuickCallDisplay ) )
				{
					// Sum up total time of child calls
					double TotalChildValue = 0.0;
					double TotalChildTimeInMS = 0.0;
					int ChildCount = 0;
					foreach( Stat Child in stat.Children )
					{
						if( Child.Type == Stat.StatType.STATTYPE_CycleCounter )
						{
							TotalChildValue += Child.Value;
							TotalChildTimeInMS += Child.ValueInMS;
						}

						++ChildCount;
					}

					// Compute inclusive time
					double InclusiveTime = stat.ValueInMS;

					// Check for stat reporting errors
					if( InclusiveTime < TotalChildTimeInMS )
					{
						throw new Exception( "A stat's inclusive time was smaller than the sum of all of it's children's inclusive times!" );
					}


					// Compute the percent of time this stat is of the caller's total time
					double PercentOfCallerTime = 100.0;
					Stat ParentStat = Parent.Tag as Stat;
					if( ParentStat != null && ParentStat.ValueInMS > 0.0 )
					{
						PercentOfCallerTime = InclusiveTime / ParentStat.ValueInMS * 100.0;
					}

					// Compute exclusive time for this stat (self time)
					double SelfValue = stat.Value - TotalChildValue;
					double SelfTimeInMS = InclusiveTime - TotalChildTimeInMS;

					// Compute percent of exclusive time for this call
					double SelfPercentOfTotal = 100.0;
					if( InclusiveTime > 0.0 )
					{
						SelfPercentOfTotal = SelfTimeInMS / InclusiveTime * 100.0;
					}


					// @todo: Display Value, Self Value, and Self Value %?

					string CallString = ( stat.CallsPerFrame == 1 ) ? " call" : " calls";
					string SummaryString =
							stat.Name + "   " +
							InclusiveTime.ToString( "F3" ) + " ms (" +
							PercentOfFrameTime.ToString( "F1" ) + "% of frame, " +
							PercentOfCallerTime.ToString( "F1" ) + "% of caller)   " +
							"Self: " +
							SelfTimeInMS.ToString( "F3" ) + " ms (" +
							SelfPercentOfTotal.ToString( "F1" ) + "%)   " +
							stat.CallsPerFrame.ToString() + CallString;

					FTreeListViewItem StatNode = new FTreeListViewItem( stat.Name );
					StatNode.ToolTipText = SummaryString;

					// Inclusive time
					StatNode.SubItems.Add( InclusiveTime.ToString( "F3" ) + " ms" );

					// Inclusive time as percent of frame time
					StatNode.SubItems.Add( PercentOfFrameTime.ToString( "F1" ) + " %" );

					// Inclusive time as percent of caller's inclusive time
					StatNode.SubItems.Add( PercentOfCallerTime.ToString( "F1" ) + " %" );

					// Exclusive time
					StatNode.SubItems.Add( SelfTimeInMS.ToString( "F3" ) + " ms" );

					// Exclusive time as percent of the inclusive time for this call
					StatNode.SubItems.Add( SelfPercentOfTotal.ToString( "F1" ) + " %" );

					// Number of calls this frame
					StatNode.SubItems.Add( stat.CallsPerFrame.ToString() );

                    // Visual representation of inclusive/exclusive time
                    FInclusiveExclusiveBarGraphCell GraphicalCell = new FInclusiveExclusiveBarGraphCell( StatNode );
                    GraphicalCell.InclusiveTime = InclusiveTime;
                    GraphicalCell.ExclusiveTime = SelfTimeInMS;
                    GraphicalCell.FrameTime = FrameTimeInMS;
                    StatNode.SubItems.Add(GraphicalCell);

					// Set text color
					{
						// Compute heat level as a scalar from 0.0 (no heat) to 1.0 (max heat)
						float HeatLevel;
						{
							// Color-code based on percent of frame time
							// 				float MinHotPercent = 2.5f;	// 5% of total frame time considered HOT!
							// 				HeatLevel = Math.Min( 1.0f, (float)PercentOfFrameTime / MinHotPercent );

							// Color-code based on time value
							float MinHotMS = 4.0f;	// 4.0 ms considered HOT!
							HeatLevel = Math.Min( 1.0f, (float)stat.ValueInMS / MinHotMS );
						}

						Color HeatColor;
						HeatColor = Color.FromArgb( (int)( HeatLevel * 200.0f ), 0, 0 );

						StatNode.ForeColor = HeatColor;
					}


					// Store the original stat as the node's tag so we can use it to sort later
					StatNode.Tag = stat;

					// Find a location to insert the node.  We'll sort the calls from longest inclusive time to
					// shortest.  Note this isn't the most efficient way to sort a tree, but it's good enough!
					int InsertBeforeIndex;
					for( InsertBeforeIndex = 0; InsertBeforeIndex < Parent.Nodes.Count; ++InsertBeforeIndex )
					{
						Stat SiblingStat = Parent.Nodes[ InsertBeforeIndex ].Tag as Stat;
						if( InclusiveTime > SiblingStat.ValueInMS )
						{
							// Our stat is larger than this sibling's so we'll insert here!
							break;
						}
					}

					// Add the stat to its parent
					Parent.Nodes.Insert( InsertBeforeIndex, StatNode );

					// Maintain selection
					if( SelectedStat != null && StatNode.Text == SelectedStat )
					{
						CallGraphTree.SelectedNode = StatNode;
					}

					// Create a fake stat that represents this stat's exclusive time
					if( ChildCount > 0 && !bIsSelfStat )
					{
						Stat SelfFakeStat = new Stat();
						SelfFakeStat.Type = stat.Type;
						SelfFakeStat.Name = "Self";
						SelfFakeStat.CallsPerFrame = stat.CallsPerFrame;
						SelfFakeStat.Value = SelfValue;
						SelfFakeStat.ValueInMS = SelfTimeInMS;

						// Add a fake stat that represents this call's "self" time
						AddStatToNode( FrameTimeInMS, SelfFakeStat, true, StatNode );
					}

					// Now add each child to the new stat node (parent)
					foreach( Stat Child in stat.Children )
					{
						AddStatToNode( FrameTimeInMS, Child, false, StatNode );
					}
				}
			}
			else
			{
				FTreeListViewItem GroupNode = FindNode( Parent.Nodes, stat.OwningGroup.Name );

				// Did we encounter a new group?
				if ( GroupNode == null )
				{
					// Reset the values
					foreach (Stat Child in stat.OwningGroup.OwnedStats)
					{
						Child.ValueInMS = 0.0;
						Child.CallsPerFrame = 0;
					}
				}

				// Update the stat value.
				double InclusiveTimeForAllInstances = 0.0;
				int CallCountForAllInstances = 0;
				foreach (Stat Child in stat.OwningGroup.OwnedStats)
				{
					if (Child.Name == stat.Name)
					{
						Child.ValueInMS += stat.ValueInMS;
						Child.CallsPerFrame += stat.CallsPerFrame;

						InclusiveTimeForAllInstances = Child.ValueInMS;
						CallCountForAllInstances = Child.CallsPerFrame;
						break;
					}
				}

				if( CallCountForAllInstances > 0 &&
					( HideQuickCalls.Checked || InclusiveTimeForAllInstances > MinTimeForQuickCallDisplay ) )
				{
					// Add the group if necessary?
					if( GroupNode == null )
					{
						GroupNode = new FTreeListViewItem( stat.OwningGroup.Name );
						Parent.Nodes.Add( GroupNode );
					}

					// Find the stat in the group.
					FTreeListViewItem StatNode = FindNodeByTag( GroupNode.Nodes, stat.Name );

					// Did we encounter a new stat?
					if( StatNode == null )
					{
						// Add the stat
						StatNode = new FTreeListViewItem( stat.Name );

						// Store the original stat as the node's tag so we can access it's name later
						StatNode.Tag = stat;

						// Inclusive time
						StatNode.SubItems.Add( "" );

						// Inclusive time as percent of frame time
						StatNode.SubItems.Add( "" );

						// Number of calls this frame
						StatNode.SubItems.Add( "" );

                        // Visual representation of time
                        StatNode.SubItems.Add(new FInclusiveExclusiveBarGraphCell(StatNode));

						// @todo: Currently we can't sort the nodes easily because of how stat instances are coallated

						GroupNode.Nodes.Add( StatNode );
					}


					StatNode.Text = stat.Name;

					// Update the text.
					string CallString = ( CallCountForAllInstances == 1 ) ? " call" : " calls";
					string SummaryString =
						stat.Name + "   " +
						InclusiveTimeForAllInstances.ToString( "F3" ) + " ms (" +
						PercentOfFrameTime.ToString( "F1" ) + "% of frame)   " +
						CallCountForAllInstances.ToString() + CallString;
					StatNode.ToolTipText = SummaryString;


					// Inclusive time
					StatNode.SubItems[ 0 ].Text = InclusiveTimeForAllInstances.ToString( "F3" ) + " ms";

					// Inclusive time as percent of frame time
					StatNode.SubItems[ 1 ].Text = PercentOfFrameTime.ToString( "F1" ) + " %";

					// Number of calls this frame
					StatNode.SubItems[ 2 ].Text = CallCountForAllInstances.ToString();

                    // Graphical view of time
                    FInclusiveExclusiveBarGraphCell GraphicalCell = StatNode.SubItems[ 3 ] as FInclusiveExclusiveBarGraphCell;
                    GraphicalCell.InclusiveTime = stat.ValueInMS;
                    GraphicalCell.FrameTime = FrameTimeInMS;
                    GraphicalCell.IsSingleTime = true;

					// Set text color
					{
						// Compute heat level as a scalar from 0.0 (no heat) to 1.0 (max heat)
						float HeatLevel;
						{
							// Color-code based on percent of frame time
							// 				float MinHotPercent = 2.5f;	// 5% of total frame time considered HOT!
							// 				HeatLevel = Math.Min( 1.0f, (float)PercentOfFrameTime / MinHotPercent );

							// Color-code based on time value
							float MinHotMS = 4.0f;	// 4.0 ms considered HOT!
							HeatLevel = Math.Min( 1.0f, (float)InclusiveTimeForAllInstances / MinHotMS );
						}

						Color HeatColor;
						HeatColor = Color.FromArgb( (int)( HeatLevel * 200.0f ), 0, 0 );

						StatNode.ForeColor = HeatColor;
					}

					// Maintain selection
					if( SelectedStat != null && StatNode.Text == SelectedStat )
					{
						CallGraphTree.SelectedNode = StatNode;
					}
				}

				// Now add each child to the new stat node (parent)
				foreach (Stat Child in stat.Children)
				{
					AddStatToNode( FrameTimeInMS, Child, false, Parent );
				}
			}
		}

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if(components != null)
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}
		
		/// <summary>
		/// Need to notify the main window that we are closing so it can remove the
		/// this view from the list it maintains
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void PerFrameView_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			StatsViewer.RemovePerFrameView(this);
		}

        private void OnHideQuickCalls( object sender, EventArgs e )
        {
			FTreeListViewItem SelectedNode = CallGraphTree.SelectedNode;
            if ( SelectedNode != null )
            {
                SelectedStat = SelectedNode.Text;
            }
            else
            {
                SelectedStat = null;
            }
            BuildTreeData(Frames);
        }

        private void OnShowHierarchy(object sender, EventArgs e)
        {
			FTreeListViewItem SelectedNode = CallGraphTree.SelectedNode;
            if (SelectedNode != null)
            {
                SelectedStat = SelectedNode.Text;
            }
            else
            {
                SelectedStat = null;
            }
            BuildTreeData(Frames);
        }

        private void CallGraphTree_Click(object sender, EventArgs e)
        {

        }
	}
}
