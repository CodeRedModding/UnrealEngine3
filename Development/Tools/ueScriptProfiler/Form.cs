/*=============================================================================
	Form.cs: Auto generated code and helper classes for UI.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
		
	Remember your first C program? Now imagine one that grew from hello world
	to doing something more complex and you'll understand the below.
=============================================================================*/

using System;
using System.IO;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Data;
using ueScriptProfilerControls;

namespace ueScriptProfiler
{
	public class ueScriptProfiler : System.Windows.Forms.Form
	{
		private System.Windows.Forms.TreeView		CallGraph;
		private System.Windows.Forms.TabControl		tabControl1;
		private System.Windows.Forms.TabPage		tabCallGraph;
		private System.Windows.Forms.TabPage		tabSortedView;
		private System.Windows.Forms.ListView		SortedView;
		private System.Windows.Forms.ColumnHeader	ColumnName;
		private System.Windows.Forms.ColumnHeader	ColumnIncl;
		private System.Windows.Forms.ColumnHeader	ColumnExcl;
		private System.Windows.Forms.ColumnHeader	ColumnCalls;
		private System.ComponentModel.IContainer components;
		private System.Windows.Forms.TabPage		tabExpensiveFunctions;
		private System.Windows.Forms.TreeView		ExpensiveFunctions;
		private System.Windows.Forms.ColumnHeader	ColumnInclPerCall;
		private System.Windows.Forms.ColumnHeader	ColumnExclPerCall;
		private System.Windows.Forms.OpenFileDialog openFileDialog;
		private System.Windows.Forms.MenuItem menuItem1;
		private System.Windows.Forms.MenuItem menuFileOpen;
		private System.Windows.Forms.MenuItem menuItem3;
		private System.Windows.Forms.MenuItem menuFileExit;
		private System.Windows.Forms.ToolBar toolBar1;
		private System.Windows.Forms.ToolBarButton toolBarFileOpen;
		private System.Windows.Forms.ImageList imageListToolBar;
		private System.Windows.Forms.StatusBarPanel statusBarPanel1;
		private System.Windows.Forms.StatusBarPanel statusBarPanel2;
		private ueScriptProfilerControls.UEStatusBar MainStatusBar;
		private System.Windows.Forms.MenuItem menuItem2;
		private System.Windows.Forms.MenuItem menuHelpUDN;
		private System.Windows.Forms.ImageList imageTabs;
		private System.Windows.Forms.MainMenu mainMenu;
	
		/** Column SortedView list view is currently being sorted by */
		private int SortColumn = 1;

		public ueScriptProfiler()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();

			// Associate custom sorter with list view.
			SortedView.ListViewItemSorter	 = new ListViewItemComparer( 1, SortOrder.Descending );

			// Associate "on click" handlers with list view and open button.
			SortedView.ColumnClick			+= new System.Windows.Forms.ColumnClickEventHandler(SortedView_ColumnClick);
		}

		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if (components != null) 
				{
					components.Dispose();
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
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(ueScriptProfiler));
			this.CallGraph = new System.Windows.Forms.TreeView();
			this.tabControl1 = new System.Windows.Forms.TabControl();
			this.tabCallGraph = new System.Windows.Forms.TabPage();
			this.tabExpensiveFunctions = new System.Windows.Forms.TabPage();
			this.ExpensiveFunctions = new System.Windows.Forms.TreeView();
			this.tabSortedView = new System.Windows.Forms.TabPage();
			this.SortedView = new System.Windows.Forms.ListView();
			this.ColumnName = new System.Windows.Forms.ColumnHeader();
			this.ColumnIncl = new System.Windows.Forms.ColumnHeader();
			this.ColumnExcl = new System.Windows.Forms.ColumnHeader();
			this.ColumnCalls = new System.Windows.Forms.ColumnHeader();
			this.ColumnInclPerCall = new System.Windows.Forms.ColumnHeader();
			this.ColumnExclPerCall = new System.Windows.Forms.ColumnHeader();
			this.imageTabs = new System.Windows.Forms.ImageList(this.components);
			this.openFileDialog = new System.Windows.Forms.OpenFileDialog();
			this.mainMenu = new System.Windows.Forms.MainMenu();
			this.menuItem1 = new System.Windows.Forms.MenuItem();
			this.menuFileOpen = new System.Windows.Forms.MenuItem();
			this.menuItem3 = new System.Windows.Forms.MenuItem();
			this.menuFileExit = new System.Windows.Forms.MenuItem();
			this.menuItem2 = new System.Windows.Forms.MenuItem();
			this.menuHelpUDN = new System.Windows.Forms.MenuItem();
			this.toolBar1 = new System.Windows.Forms.ToolBar();
			this.toolBarFileOpen = new System.Windows.Forms.ToolBarButton();
			this.imageListToolBar = new System.Windows.Forms.ImageList(this.components);
			this.MainStatusBar = new ueScriptProfilerControls.UEStatusBar();
			this.statusBarPanel1 = new System.Windows.Forms.StatusBarPanel();
			this.statusBarPanel2 = new System.Windows.Forms.StatusBarPanel();
			this.tabControl1.SuspendLayout();
			this.tabCallGraph.SuspendLayout();
			this.tabExpensiveFunctions.SuspendLayout();
			this.tabSortedView.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel1)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel2)).BeginInit();
			this.SuspendLayout();
			// 
			// CallGraph
			// 
			this.CallGraph.Dock = System.Windows.Forms.DockStyle.Fill;
			this.CallGraph.Font = new System.Drawing.Font("Courier New", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.CallGraph.ImageIndex = -1;
			this.CallGraph.Indent = 19;
			this.CallGraph.ItemHeight = 20;
			this.CallGraph.Location = new System.Drawing.Point(0, 0);
			this.CallGraph.Name = "CallGraph";
			this.CallGraph.SelectedImageIndex = -1;
			this.CallGraph.Size = new System.Drawing.Size(984, 581);
			this.CallGraph.TabIndex = 0;
			// 
			// tabControl1
			// 
			this.tabControl1.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
				| System.Windows.Forms.AnchorStyles.Left) 
				| System.Windows.Forms.AnchorStyles.Right)));
			this.tabControl1.Controls.Add(this.tabCallGraph);
			this.tabControl1.Controls.Add(this.tabExpensiveFunctions);
			this.tabControl1.Controls.Add(this.tabSortedView);
			this.tabControl1.Font = new System.Drawing.Font("Courier New", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.tabControl1.ImageList = this.imageTabs;
			this.tabControl1.Location = new System.Drawing.Point(0, 32);
			this.tabControl1.Name = "tabControl1";
			this.tabControl1.SelectedIndex = 0;
			this.tabControl1.ShowToolTips = true;
			this.tabControl1.Size = new System.Drawing.Size(992, 616);
			this.tabControl1.TabIndex = 1;
			this.tabControl1.Visible = false;
			// 
			// tabCallGraph
			// 
			this.tabCallGraph.Controls.Add(this.CallGraph);
			this.tabCallGraph.ImageIndex = 0;
			this.tabCallGraph.Location = new System.Drawing.Point(4, 31);
			this.tabCallGraph.Name = "tabCallGraph";
			this.tabCallGraph.Size = new System.Drawing.Size(984, 581);
			this.tabCallGraph.TabIndex = 0;
			this.tabCallGraph.Text = "Call Graph";
			// 
			// tabExpensiveFunctions
			// 
			this.tabExpensiveFunctions.Controls.Add(this.ExpensiveFunctions);
			this.tabExpensiveFunctions.ImageIndex = 1;
			this.tabExpensiveFunctions.Location = new System.Drawing.Point(4, 31);
			this.tabExpensiveFunctions.Name = "tabExpensiveFunctions";
			this.tabExpensiveFunctions.Size = new System.Drawing.Size(984, 581);
			this.tabExpensiveFunctions.TabIndex = 2;
			this.tabExpensiveFunctions.Text = "Expensive Functions";
			// 
			// ExpensiveFunctions
			// 
			this.ExpensiveFunctions.Dock = System.Windows.Forms.DockStyle.Fill;
			this.ExpensiveFunctions.ImageIndex = -1;
			this.ExpensiveFunctions.Indent = 19;
			this.ExpensiveFunctions.ItemHeight = 20;
			this.ExpensiveFunctions.Location = new System.Drawing.Point(0, 0);
			this.ExpensiveFunctions.Name = "ExpensiveFunctions";
			this.ExpensiveFunctions.SelectedImageIndex = -1;
			this.ExpensiveFunctions.Size = new System.Drawing.Size(984, 581);
			this.ExpensiveFunctions.TabIndex = 0;
			// 
			// tabSortedView
			// 
			this.tabSortedView.Controls.Add(this.SortedView);
			this.tabSortedView.ImageIndex = 2;
			this.tabSortedView.Location = new System.Drawing.Point(4, 31);
			this.tabSortedView.Name = "tabSortedView";
			this.tabSortedView.Size = new System.Drawing.Size(984, 581);
			this.tabSortedView.TabIndex = 1;
			this.tabSortedView.Text = "Incl/Excl Time";
			// 
			// SortedView
			// 
			this.SortedView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
				| System.Windows.Forms.AnchorStyles.Left) 
				| System.Windows.Forms.AnchorStyles.Right)));
			this.SortedView.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
																						 this.ColumnName,
																						 this.ColumnIncl,
																						 this.ColumnExcl,
																						 this.ColumnCalls,
																						 this.ColumnInclPerCall,
																						 this.ColumnExclPerCall});
			this.SortedView.Font = new System.Drawing.Font("Courier New", 9.75F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ((System.Byte)(0)));
			this.SortedView.FullRowSelect = true;
			this.SortedView.GridLines = true;
			this.SortedView.Location = new System.Drawing.Point(0, 0);
			this.SortedView.Name = "SortedView";
			this.SortedView.Size = new System.Drawing.Size(984, 581);
			this.SortedView.TabIndex = 0;
			this.SortedView.View = System.Windows.Forms.View.Details;
			// 
			// ColumnName
			// 
			this.ColumnName.Text = "Function Name";
			this.ColumnName.Width = 500;
			// 
			// ColumnIncl
			// 
			this.ColumnIncl.Text = "incl. %";
			this.ColumnIncl.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ColumnIncl.Width = 85;
			// 
			// ColumnExcl
			// 
			this.ColumnExcl.Text = "excl. %";
			this.ColumnExcl.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ColumnExcl.Width = 85;
			// 
			// ColumnCalls
			// 
			this.ColumnCalls.Text = "calls";
			this.ColumnCalls.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ColumnCalls.Width = 85;
			// 
			// ColumnInclPerCall
			// 
			this.ColumnInclPerCall.Text = "incl. per call";
			this.ColumnInclPerCall.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ColumnInclPerCall.Width = 130;
			// 
			// ColumnExclPerCall
			// 
			this.ColumnExclPerCall.Text = "excl. per call";
			this.ColumnExclPerCall.TextAlign = System.Windows.Forms.HorizontalAlignment.Center;
			this.ColumnExclPerCall.Width = 130;
			// 
			// imageTabs
			// 
			this.imageTabs.ColorDepth = System.Windows.Forms.ColorDepth.Depth32Bit;
			this.imageTabs.ImageSize = new System.Drawing.Size(24, 24);
			this.imageTabs.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageTabs.ImageStream")));
			this.imageTabs.TransparentColor = System.Drawing.Color.Transparent;
			// 
			// openFileDialog
			// 
			this.openFileDialog.DefaultExt = "uprof";
			this.openFileDialog.Filter = "UnrealScript Profile Files (*.uprof)|*.uprof|All Files (*.*)|*.*";
			this.openFileDialog.RestoreDirectory = true;
			// 
			// mainMenu
			// 
			this.mainMenu.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					 this.menuItem1,
																					 this.menuItem2});
			// 
			// menuItem1
			// 
			this.menuItem1.Index = 0;
			this.menuItem1.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					  this.menuFileOpen,
																					  this.menuItem3,
																					  this.menuFileExit});
			this.menuItem1.Text = "&File";
			// 
			// menuFileOpen
			// 
			this.menuFileOpen.Index = 0;
			this.menuFileOpen.Shortcut = System.Windows.Forms.Shortcut.CtrlO;
			this.menuFileOpen.Text = "&Open...";
			this.menuFileOpen.Click += new System.EventHandler(this.menuFileOpen_Click);
			// 
			// menuItem3
			// 
			this.menuItem3.Index = 1;
			this.menuItem3.Text = "-";
			// 
			// menuFileExit
			// 
			this.menuFileExit.Index = 2;
			this.menuFileExit.Shortcut = System.Windows.Forms.Shortcut.AltF4;
			this.menuFileExit.Text = "E&xit";
			this.menuFileExit.Click += new System.EventHandler(this.menuFileExit_Click);
			// 
			// menuItem2
			// 
			this.menuItem2.Index = 1;
			this.menuItem2.MenuItems.AddRange(new System.Windows.Forms.MenuItem[] {
																					  this.menuHelpUDN});
			this.menuItem2.Text = "&Help";
			// 
			// menuHelpUDN
			// 
			this.menuHelpUDN.Index = 0;
			this.menuHelpUDN.Shortcut = System.Windows.Forms.Shortcut.F1;
			this.menuHelpUDN.Text = "&UDN...";
			this.menuHelpUDN.Click += new System.EventHandler(this.menuHelpUDN_Click);
			// 
			// toolBar1
			// 
			this.toolBar1.Buttons.AddRange(new System.Windows.Forms.ToolBarButton[] {
																						this.toolBarFileOpen});
			this.toolBar1.DropDownArrows = true;
			this.toolBar1.ImageList = this.imageListToolBar;
			this.toolBar1.Location = new System.Drawing.Point(0, 0);
			this.toolBar1.Name = "toolBar1";
			this.toolBar1.ShowToolTips = true;
			this.toolBar1.Size = new System.Drawing.Size(992, 27);
			this.toolBar1.TabIndex = 2;
			this.toolBar1.ButtonClick += new System.Windows.Forms.ToolBarButtonClickEventHandler(this.toolBar1_ButtonClick);
			// 
			// toolBarFileOpen
			// 
			this.toolBarFileOpen.ImageIndex = 0;
			this.toolBarFileOpen.ToolTipText = "Open Script Profile...";
			// 
			// imageListToolBar
			// 
			this.imageListToolBar.ColorDepth = System.Windows.Forms.ColorDepth.Depth32Bit;
			this.imageListToolBar.ImageSize = new System.Drawing.Size(16, 15);
			this.imageListToolBar.ImageStream = ((System.Windows.Forms.ImageListStreamer)(resources.GetObject("imageListToolBar.ImageStream")));
			this.imageListToolBar.TransparentColor = System.Drawing.Color.Silver;
			// 
			// MainStatusBar
			// 
			this.MainStatusBar.Location = new System.Drawing.Point(0, 647);
			this.MainStatusBar.Name = "MainStatusBar";
			this.MainStatusBar.Panels.AddRange(new System.Windows.Forms.StatusBarPanel[] {
																							 this.statusBarPanel1,
																							 this.statusBarPanel2});
			this.MainStatusBar.Size = new System.Drawing.Size(992, 22);
			this.MainStatusBar.TabIndex = 3;
			this.MainStatusBar.Text = "Ready.";
			// 
			// statusBarPanel1
			// 
			this.statusBarPanel1.AutoSize = System.Windows.Forms.StatusBarPanelAutoSize.Contents;
			this.statusBarPanel1.BorderStyle = System.Windows.Forms.StatusBarPanelBorderStyle.None;
			this.statusBarPanel1.Text = "Loading:";
			this.statusBarPanel1.Width = 58;
			// 
			// statusBarPanel2
			// 
			this.statusBarPanel2.AutoSize = System.Windows.Forms.StatusBarPanelAutoSize.Spring;
			this.statusBarPanel2.BorderStyle = System.Windows.Forms.StatusBarPanelBorderStyle.None;
			this.statusBarPanel2.Width = 918;
			// 
			// ueScriptProfiler
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(992, 669);
			this.Controls.Add(this.MainStatusBar);
			this.Controls.Add(this.toolBar1);
			this.Controls.Add(this.tabControl1);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Menu = this.mainMenu;
			this.Name = "ueScriptProfiler";
			this.Text = "UnrealScript Profiler";
			this.tabControl1.ResumeLayout(false);
			this.tabCallGraph.ResumeLayout(false);
			this.tabExpensiveFunctions.ResumeLayout(false);
			this.tabSortedView.ResumeLayout(false);
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel1)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.statusBarPanel2)).EndInit();
			this.ResumeLayout(false);

		}
		#endregion

		[STAThread]
		static void Main() 
		{
			Application.Run(new ueScriptProfiler());
		}

		/**
		 * Handler for clicking on the columns of the SortedView list view.
		 * 
		 * @param	Sender	unused
		 * @param	Event	Event arguments.	
		 */
		private void SortedView_ColumnClick( object Sender, System.Windows.Forms.ColumnClickEventArgs Event )
		{
			bool RequiresResort = false;

			if( Event.Column == 0 )
			{
				// Function name column allows sorting both descending and ascending. Clicking on the already
				// sorted column swaps the sort order.

				RequiresResort					= true;
				SortColumn						= 0;

				// We abuse the SortedView's Sorting member variable to keep track of the function name column's current sorting.
				if( SortedView.Sorting != SortOrder.Ascending )
				{
					SortedView.Sorting = SortOrder.Ascending;
				}
				else
				{
					SortedView.Sorting = SortOrder.Descending;
				}
				SortedView.ListViewItemSorter	= new ListViewItemComparer( SortColumn, SortedView.Sorting );
			}
			else if( Event.Column != SortColumn )
			{
				// Non- function name columns are always sorted in descending order.

				RequiresResort					= true;
				SortColumn						= Event.Column;
				SortedView.ListViewItemSorter	= new ListViewItemComparer( SortColumn, SortOrder.Descending );
			}

			if( RequiresResort )
			{
				SortedView.Sort();
			}
		}

		private void menuFileExit_Click(object sender, System.EventArgs e)
		{
			Close();
		}

		private void menuFileOpen_Click(object sender, System.EventArgs e)
		{
			OpenFile();
		}

		/**
		 * Asks for a filename and opens it in the parser.
		 */

		public void OpenFile()
		{
			if( openFileDialog.ShowDialog() == DialogResult.OK ) 
			{ 
				MainStatusBar.ShowProgress( true );

				CallGraphParser Parser = new CallGraphParser();
				Parser.ParseStream(  openFileDialog.FileName, CallGraph, ExpensiveFunctions, SortedView, MainStatusBar.LoadingProgressBar );

				MainStatusBar.ShowProgress( false );
				tabControl1.Visible = true;
			}
		}

		private void toolBar1_ButtonClick(object sender, System.Windows.Forms.ToolBarButtonClickEventArgs e)
		{
			OpenFile();
		}

		private void menuHelpUDN_Click(object sender, System.EventArgs e)
		{
			System.Diagnostics.Process.Start("https://udn.epicgames.com/bin/view/Three/ScriptProfiler");
		}

		/**
		 * Comparator used to sort the columns of a list view in a specified order. Internally
		 * Falls back to using the string comparator for its sorting.	
		 */
		public class ListViewItemComparer : IComparer 
		{
			/** Column to sort by		*/
			private int			Column;
			/** Sort order for column	*/
			private SortOrder	Order;
		
			/**
			 * Constructor
			 * 
			 * @param	InColumn	Column to sort by
			 * @param	InOrder		Sort order to use (either ascending or descending)	
			 */
			public ListViewItemComparer(int InColumn, SortOrder InOrder) 
			{
				Column	= InColumn;
				Order	= InOrder;
			}
			
			/**
			 * Compare function
			 * 
			 * @documentation	
			 */
			public int Compare( object A, object B ) 
			{
				int SortValue = String.Compare( ((ListViewItem) A).SubItems[Column].Text, ((ListViewItem) B).SubItems[Column].Text);
			
				if( Order == SortOrder.Descending )
				{
					return -SortValue;
				}
				else
				{
					return SortValue;
				}
			}
		}
	}
}