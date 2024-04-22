namespace UnrealscriptDevSuite
{
	partial class ClassBrowser
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
			if ( disposing && ( components != null ) )
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Component Designer generated code

		/// <summary> 
		/// Required method for Designer support - do not modify 
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(ClassBrowser));
			this.HierarchyToolStrip = new System.Windows.Forms.ToolStrip();
			this.RefreshButton = new System.Windows.Forms.ToolStripButton();
			this.ActorsOnly = new System.Windows.Forms.ToolStripButton();
			this.tbFindClass = new System.Windows.Forms.ToolStripButton();
			this.tbTextFilter = new System.Windows.Forms.ToolStripTextBox();
			this.QuickSearch = new System.Windows.Forms.ToolStripButton();
			this.ExpandTree = new System.Windows.Forms.ToolStripButton();
			this.CollapseTree = new System.Windows.Forms.ToolStripButton();
			this.btnSearchLineage = new System.Windows.Forms.ToolStripButton();
			this.tcPages = new System.Windows.Forms.TabControl();
			this.tpClassView = new System.Windows.Forms.TabPage();
			this.BrowserContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.miAddClass = new System.Windows.Forms.ToolStripMenuItem();
			this.toolStripSeparator1 = new System.Windows.Forms.ToolStripSeparator();
			this.miShowDescendantsOnly = new System.Windows.Forms.ToolStripMenuItem();
			this.tpInterfaces = new System.Windows.Forms.TabPage();
			this.tpIncludes = new System.Windows.Forms.TabPage();
			this.lbIncludes = new System.Windows.Forms.ListBox();
			this.ClassImageList = new System.Windows.Forms.ImageList(this.components);
			this.RefreshTimer = new System.Windows.Forms.Timer(this.components);
			this.Fuck = new System.Windows.Forms.ToolStripButton();
			this.tvClassTree = new UnrealscriptDevSuite.ClassTree();
			this.tvInterfaceTree = new UnrealscriptDevSuite.ClassTree();
			this.HierarchyToolStrip.SuspendLayout();
			this.tcPages.SuspendLayout();
			this.tpClassView.SuspendLayout();
			this.BrowserContextMenu.SuspendLayout();
			this.tpInterfaces.SuspendLayout();
			this.tpIncludes.SuspendLayout();
			this.SuspendLayout();
			// 
			// HierarchyToolStrip
			// 
			this.HierarchyToolStrip.BackColor = System.Drawing.SystemColors.ButtonFace;
			this.HierarchyToolStrip.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
			this.RefreshButton,
			this.ActorsOnly,
			this.tbFindClass,
			this.tbTextFilter,
			this.QuickSearch,
			this.ExpandTree,
			this.CollapseTree,
			this.btnSearchLineage});
			this.HierarchyToolStrip.Location = new System.Drawing.Point(0, 0);
			this.HierarchyToolStrip.Name = "HierarchyToolStrip";
			this.HierarchyToolStrip.Size = new System.Drawing.Size(339, 25);
			this.HierarchyToolStrip.TabIndex = 2;
			this.HierarchyToolStrip.Text = "toolStrip1";
			// 
			// RefreshButton
			// 
			this.RefreshButton.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.RefreshButton.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.RefreshButton.Name = "RefreshButton";
			this.RefreshButton.Size = new System.Drawing.Size(23, 22);
			this.RefreshButton.Text = "toolStripButton1";
			this.RefreshButton.ToolTipText = "Refresh the hierarchy.";
			this.RefreshButton.Click += new System.EventHandler(this.RefreshButton_Click);
			// 
			// ActorsOnly
			// 
			this.ActorsOnly.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.ActorsOnly.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.ActorsOnly.Name = "ActorsOnly";
			this.ActorsOnly.Size = new System.Drawing.Size(23, 22);
			this.ActorsOnly.Text = "toolStripButton1";
			this.ActorsOnly.ToolTipText = "Only show children of the selected class.";
			this.ActorsOnly.Click += new System.EventHandler(this.ActorsOnly_Click);
			// 
			// tbFindClass
			// 
			this.tbFindClass.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.tbFindClass.Image = ( (System.Drawing.Image)( resources.GetObject("tbFindClass.Image") ) );
			this.tbFindClass.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.tbFindClass.Name = "tbFindClass";
			this.tbFindClass.Size = new System.Drawing.Size(23, 22);
			this.tbFindClass.Text = "toolStripButton1";
			this.tbFindClass.ToolTipText = "Find the current open class in the hierarchy.";
			this.tbFindClass.Click += new System.EventHandler(this.tbFindClass_Click);
			// 
			// tbTextFilter
			// 
			this.tbTextFilter.BorderStyle = System.Windows.Forms.BorderStyle.FixedSingle;
			this.tbTextFilter.Name = "tbTextFilter";
			this.tbTextFilter.Size = new System.Drawing.Size(100, 23);
			this.tbTextFilter.ToolTipText = "Any text in this field will be used to filter the hierarchy.";
			this.tbTextFilter.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.tbTextFilter_KeyPress);
			// 
			// QuickSearch
			// 
			this.QuickSearch.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.QuickSearch.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.QuickSearch.Name = "QuickSearch";
			this.QuickSearch.Size = new System.Drawing.Size(23, 22);
			this.QuickSearch.Text = "toolStripButton1";
			this.QuickSearch.ToolTipText = "Filter the hierarchy.";
			this.QuickSearch.Click += new System.EventHandler(this.QuickSearch_Click);
			// 
			// ExpandTree
			// 
			this.ExpandTree.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.ExpandTree.Image = ( (System.Drawing.Image)( resources.GetObject("ExpandTree.Image") ) );
			this.ExpandTree.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.ExpandTree.Name = "ExpandTree";
			this.ExpandTree.Size = new System.Drawing.Size(23, 22);
			this.ExpandTree.Text = "toolStripButton1";
			this.ExpandTree.ToolTipText = "Expand All.";
			this.ExpandTree.Click += new System.EventHandler(this.ExpandTree_Click);
			// 
			// CollapseTree
			// 
			this.CollapseTree.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.CollapseTree.Image = ( (System.Drawing.Image)( resources.GetObject("CollapseTree.Image") ) );
			this.CollapseTree.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.CollapseTree.Name = "CollapseTree";
			this.CollapseTree.Size = new System.Drawing.Size(23, 22);
			this.CollapseTree.Text = "toolStripButton2";
			this.CollapseTree.ToolTipText = "Collapse All";
			this.CollapseTree.Click += new System.EventHandler(this.CollapseTree_Click);
			// 
			// btnSearchLineage
			// 
			this.btnSearchLineage.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.btnSearchLineage.Image = ( (System.Drawing.Image)( resources.GetObject("btnSearchLineage.Image") ) );
			this.btnSearchLineage.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.btnSearchLineage.Name = "btnSearchLineage";
			this.btnSearchLineage.Size = new System.Drawing.Size(23, 22);
			this.btnSearchLineage.Text = "toolStripButton1";
			this.btnSearchLineage.ToolTipText = "Search Lineage";
			this.btnSearchLineage.Click += new System.EventHandler(this.btnSearchLineage_Click);
			// 
			// tcPages
			// 
			this.tcPages.Alignment = System.Windows.Forms.TabAlignment.Bottom;
			this.tcPages.Controls.Add(this.tpClassView);
			this.tcPages.Controls.Add(this.tpInterfaces);
			this.tcPages.Controls.Add(this.tpIncludes);
			this.tcPages.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tcPages.Location = new System.Drawing.Point(0, 25);
			this.tcPages.Margin = new System.Windows.Forms.Padding(0);
			this.tcPages.Name = "tcPages";
			this.tcPages.Padding = new System.Drawing.Point(0, 0);
			this.tcPages.SelectedIndex = 0;
			this.tcPages.Size = new System.Drawing.Size(339, 411);
			this.tcPages.TabIndex = 3;
			// 
			// tpClassView
			// 
			this.tpClassView.Controls.Add(this.tvClassTree);
			this.tpClassView.Location = new System.Drawing.Point(4, 4);
			this.tpClassView.Name = "tpClassView";
			this.tpClassView.Padding = new System.Windows.Forms.Padding(3);
			this.tpClassView.Size = new System.Drawing.Size(331, 385);
			this.tpClassView.TabIndex = 0;
			this.tpClassView.Text = "Classes";
			this.tpClassView.UseVisualStyleBackColor = true;
			// 
			// BrowserContextMenu
			// 
			this.BrowserContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.miAddClass,
            this.toolStripSeparator1,
            this.miShowDescendantsOnly});
			this.BrowserContextMenu.Name = "BrowserContextMenu";
			this.BrowserContextMenu.Size = new System.Drawing.Size(202, 54);
			// 
			// miAddClass
			// 
			this.miAddClass.Name = "miAddClass";
			this.miAddClass.Size = new System.Drawing.Size(201, 22);
			this.miAddClass.Text = "Add Child Class";
			this.miAddClass.Click += new System.EventHandler(this.miAddClass_Click);
			// 
			// toolStripSeparator1
			// 
			this.toolStripSeparator1.Name = "toolStripSeparator1";
			this.toolStripSeparator1.Size = new System.Drawing.Size(198, 6);
			// 
			// miShowDescendantsOnly
			// 
			this.miShowDescendantsOnly.Name = "miShowDescendantsOnly";
			this.miShowDescendantsOnly.Size = new System.Drawing.Size(201, 22);
			this.miShowDescendantsOnly.Text = "Show Descendants Only";
			this.miShowDescendantsOnly.Click += new System.EventHandler(this.ActorsOnly_Click);
			// 
			// tpInterfaces
			// 
			this.tpInterfaces.Controls.Add(this.tvInterfaceTree);
			this.tpInterfaces.Location = new System.Drawing.Point(4, 4);
			this.tpInterfaces.Name = "tpInterfaces";
			this.tpInterfaces.Padding = new System.Windows.Forms.Padding(3);
			this.tpInterfaces.Size = new System.Drawing.Size(331, 385);
			this.tpInterfaces.TabIndex = 1;
			this.tpInterfaces.Text = "Interfaces";
			this.tpInterfaces.UseVisualStyleBackColor = true;
			// 
			// tpIncludes
			// 
			this.tpIncludes.Controls.Add(this.lbIncludes);
			this.tpIncludes.Location = new System.Drawing.Point(4, 4);
			this.tpIncludes.Name = "tpIncludes";
			this.tpIncludes.Padding = new System.Windows.Forms.Padding(3);
			this.tpIncludes.Size = new System.Drawing.Size(331, 385);
			this.tpIncludes.TabIndex = 2;
			this.tpIncludes.Text = "Includes";
			this.tpIncludes.UseVisualStyleBackColor = true;
			// 
			// lbIncludes
			// 
			this.lbIncludes.DisplayMember = "IncludeName";
			this.lbIncludes.Dock = System.Windows.Forms.DockStyle.Fill;
			this.lbIncludes.FormattingEnabled = true;
			this.lbIncludes.Location = new System.Drawing.Point(3, 3);
			this.lbIncludes.Name = "lbIncludes";
			this.lbIncludes.Size = new System.Drawing.Size(325, 368);
			this.lbIncludes.TabIndex = 0;
			this.lbIncludes.ValueMember = "ProjItem";
			this.lbIncludes.MouseDoubleClick += new System.Windows.Forms.MouseEventHandler(this.lbIncludes_MouseDoubleClick);
			// 
			// ClassImageList
			// 
			this.ClassImageList.ImageStream = ( (System.Windows.Forms.ImageListStreamer)( resources.GetObject("ClassImageList.ImageStream") ) );
			this.ClassImageList.TransparentColor = System.Drawing.Color.Transparent;
			this.ClassImageList.Images.SetKeyName(0, "Refresh.png");
			this.ClassImageList.Images.SetKeyName(1, "Filter.png");
			this.ClassImageList.Images.SetKeyName(2, "Go.png");
			this.ClassImageList.Images.SetKeyName(3, "Find.png");
			this.ClassImageList.Images.SetKeyName(4, "Plus.png");
			this.ClassImageList.Images.SetKeyName(5, "Minus.png");
			this.ClassImageList.Images.SetKeyName(6, "Lineage.png");
			this.ClassImageList.Images.SetKeyName(7, "Cancel.png");
			// 
			// RefreshTimer
			// 
			this.RefreshTimer.Tick += new System.EventHandler(this.RefreshTimer_Tick);
			// 
			// Fuck
			// 
			this.Fuck.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Image;
			this.Fuck.Image = ( (System.Drawing.Image)( resources.GetObject("Fuck.Image") ) );
			this.Fuck.ImageTransparentColor = System.Drawing.Color.Magenta;
			this.Fuck.Name = "Fuck";
			this.Fuck.Size = new System.Drawing.Size(23, 22);
			this.Fuck.Text = "toolStripButton1";
			// 
			// tvClassTree
			// 
			this.tvClassTree.ContextMenuStrip = this.BrowserContextMenu;
			this.tvClassTree.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tvClassTree.Font = new System.Drawing.Font("Arial", 8.25F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( (byte)( 0 ) ));
			this.tvClassTree.Location = new System.Drawing.Point(3, 3);
			this.tvClassTree.Name = "tvClassTree";
			this.tvClassTree.Size = new System.Drawing.Size(325, 379);
			this.tvClassTree.TabIndex = 0;
			this.tvClassTree.NodeMouseDoubleClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.ClassTree_NodeMouseDoubleClick);
			this.tvClassTree.NodeMouseHover += new System.Windows.Forms.TreeNodeMouseHoverEventHandler(this.ClassTree_NodeMouseHover);
			this.tvClassTree.BeforeSelect += new System.Windows.Forms.TreeViewCancelEventHandler(this.tvBeforeSelect);
			// 
			// tvInterfaceTree
			// 
			this.tvInterfaceTree.Dock = System.Windows.Forms.DockStyle.Fill;
			this.tvInterfaceTree.Location = new System.Drawing.Point(3, 3);
			this.tvInterfaceTree.Name = "tvInterfaceTree";
			this.tvInterfaceTree.Size = new System.Drawing.Size(325, 379);
			this.tvInterfaceTree.TabIndex = 0;
			this.tvInterfaceTree.NodeMouseDoubleClick += new System.Windows.Forms.TreeNodeMouseClickEventHandler(this.tvInterfaceTree_NodeMouseDoubleClick);
			// 
			// ClassBrowser
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.Controls.Add(this.tcPages);
			this.Controls.Add(this.HierarchyToolStrip);
			this.Name = "ClassBrowser";
			this.Size = new System.Drawing.Size(339, 436);
			this.HierarchyToolStrip.ResumeLayout(false);
			this.HierarchyToolStrip.PerformLayout();
			this.tcPages.ResumeLayout(false);
			this.tpClassView.ResumeLayout(false);
			this.BrowserContextMenu.ResumeLayout(false);
			this.tpInterfaces.ResumeLayout(false);
			this.tpIncludes.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private UnrealscriptDevSuite.ClassTree tvClassTree;
		private System.Windows.Forms.ToolStrip HierarchyToolStrip;
		private System.Windows.Forms.ToolStripButton RefreshButton;
		private System.Windows.Forms.TabControl tcPages;
		private System.Windows.Forms.TabPage tpClassView;
		private System.Windows.Forms.TabPage tpInterfaces;
		private System.Windows.Forms.TabPage tpIncludes;
		private System.Windows.Forms.ListBox lbIncludes;
		private UnrealscriptDevSuite.ClassTree tvInterfaceTree;
		private System.Windows.Forms.ToolStripButton ActorsOnly;
		private System.Windows.Forms.ToolStripTextBox tbTextFilter;
		private System.Windows.Forms.ToolStripButton QuickSearch;
		private System.Windows.Forms.ImageList ClassImageList;
		private System.Windows.Forms.ToolStripButton ExpandTree;
		private System.Windows.Forms.ToolStripButton CollapseTree;
		private System.Windows.Forms.ToolStripButton tbFindClass;
		private System.Windows.Forms.ToolStripButton btnSearchLineage;
		private System.Windows.Forms.ContextMenuStrip BrowserContextMenu;
		private System.Windows.Forms.ToolStripMenuItem miAddClass;
		private System.Windows.Forms.ToolStripSeparator toolStripSeparator1;
		private System.Windows.Forms.ToolStripMenuItem miShowDescendantsOnly;
		private System.Windows.Forms.Timer RefreshTimer;
		private System.Windows.Forms.ToolStripButton Fuck;



	}
}
