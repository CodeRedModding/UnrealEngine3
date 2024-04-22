namespace AIProfiler
{
	partial class MainWindow
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
			this.components = new System.ComponentModel.Container();
			this.MainTableLayoutPanel = new System.Windows.Forms.TableLayoutPanel();
			this.FilterGroupBox = new System.Windows.Forms.GroupBox();
			this.ControllerClassListBox = new System.Windows.Forms.CheckedListBox();
			this.ControllerClassLabel = new System.Windows.Forms.Label();
			this.EventCategoryLabel = new System.Windows.Forms.Label();
			this.EventCategoryListBox = new System.Windows.Forms.CheckedListBox();
			this.EndTimeControl = new System.Windows.Forms.NumericUpDown();
			this.TimeEndLabel = new System.Windows.Forms.Label();
			this.StartTimeControl = new System.Windows.Forms.NumericUpDown();
			this.TimeStartLabel = new System.Windows.Forms.Label();
			this.SearchTextBox = new System.Windows.Forms.TextBox();
			this.SearchLabel = new System.Windows.Forms.Label();
			this.TreeViewGroupBox = new System.Windows.Forms.GroupBox();
			this.MainTreeView = new System.Windows.Forms.TreeView();
			this.MainMenu = new System.Windows.Forms.MenuStrip();
			this.FileMenu = new System.Windows.Forms.ToolStripMenuItem();
			this.FileOpenMenuItem = new System.Windows.Forms.ToolStripMenuItem();
			this.OpenFileDialog = new System.Windows.Forms.OpenFileDialog();
			this.DeferredFilterUpdateTimer = new System.Windows.Forms.Timer(this.components);
			this.MainTableLayoutPanel.SuspendLayout();
			this.FilterGroupBox.SuspendLayout();
			((System.ComponentModel.ISupportInitialize)(this.EndTimeControl)).BeginInit();
			((System.ComponentModel.ISupportInitialize)(this.StartTimeControl)).BeginInit();
			this.TreeViewGroupBox.SuspendLayout();
			this.MainMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// MainTableLayoutPanel
			// 
			this.MainTableLayoutPanel.ColumnCount = 1;
			this.MainTableLayoutPanel.ColumnStyles.Add(new System.Windows.Forms.ColumnStyle());
			this.MainTableLayoutPanel.Controls.Add(this.FilterGroupBox, 0, 1);
			this.MainTableLayoutPanel.Controls.Add(this.TreeViewGroupBox, 0, 2);
			this.MainTableLayoutPanel.Controls.Add(this.MainMenu, 0, 0);
			this.MainTableLayoutPanel.Dock = System.Windows.Forms.DockStyle.Fill;
			this.MainTableLayoutPanel.Location = new System.Drawing.Point(0, 0);
			this.MainTableLayoutPanel.Name = "MainTableLayoutPanel";
			this.MainTableLayoutPanel.RowCount = 3;
			this.MainTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Absolute, 20F));
			this.MainTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 30F));
			this.MainTableLayoutPanel.RowStyles.Add(new System.Windows.Forms.RowStyle(System.Windows.Forms.SizeType.Percent, 70F));
			this.MainTableLayoutPanel.Size = new System.Drawing.Size(764, 562);
			this.MainTableLayoutPanel.TabIndex = 0;
			// 
			// FilterGroupBox
			// 
			this.FilterGroupBox.AutoSize = true;
			this.FilterGroupBox.Controls.Add(this.ControllerClassListBox);
			this.FilterGroupBox.Controls.Add(this.ControllerClassLabel);
			this.FilterGroupBox.Controls.Add(this.EventCategoryLabel);
			this.FilterGroupBox.Controls.Add(this.EventCategoryListBox);
			this.FilterGroupBox.Controls.Add(this.EndTimeControl);
			this.FilterGroupBox.Controls.Add(this.TimeEndLabel);
			this.FilterGroupBox.Controls.Add(this.StartTimeControl);
			this.FilterGroupBox.Controls.Add(this.TimeStartLabel);
			this.FilterGroupBox.Controls.Add(this.SearchTextBox);
			this.FilterGroupBox.Controls.Add(this.SearchLabel);
			this.FilterGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			this.FilterGroupBox.Font = new System.Drawing.Font("Calibri", 10F);
			this.FilterGroupBox.Location = new System.Drawing.Point(3, 23);
			this.FilterGroupBox.Name = "FilterGroupBox";
			this.FilterGroupBox.Size = new System.Drawing.Size(758, 156);
			this.FilterGroupBox.TabIndex = 0;
			this.FilterGroupBox.TabStop = false;
			this.FilterGroupBox.Text = "Filters";
			// 
			// ControllerClassListBox
			// 
			this.ControllerClassListBox.CheckOnClick = true;
			this.ControllerClassListBox.Font = new System.Drawing.Font("Calibri", 9F);
			this.ControllerClassListBox.Location = new System.Drawing.Point(424, 44);
			this.ControllerClassListBox.Name = "ControllerClassListBox";
			this.ControllerClassListBox.Size = new System.Drawing.Size(270, 106);
			this.ControllerClassListBox.Sorted = true;
			this.ControllerClassListBox.TabIndex = 9;
			this.ControllerClassListBox.ItemCheck += new System.Windows.Forms.ItemCheckEventHandler(this.OnControllerClassCheck);
			// 
			// ControllerClassLabel
			// 
			this.ControllerClassLabel.Font = new System.Drawing.Font("Calibri", 9F);
			this.ControllerClassLabel.Location = new System.Drawing.Point(355, 44);
			this.ControllerClassLabel.Name = "ControllerClassLabel";
			this.ControllerClassLabel.Size = new System.Drawing.Size(68, 30);
			this.ControllerClassLabel.TabIndex = 8;
			this.ControllerClassLabel.Text = "Controller Class";
			// 
			// EventCategoryLabel
			// 
			this.EventCategoryLabel.Font = new System.Drawing.Font("Calibri", 9F);
			this.EventCategoryLabel.Location = new System.Drawing.Point(9, 49);
			this.EventCategoryLabel.Name = "EventCategoryLabel";
			this.EventCategoryLabel.Size = new System.Drawing.Size(56, 30);
			this.EventCategoryLabel.TabIndex = 7;
			this.EventCategoryLabel.Text = "Event Category";
			// 
			// EventCategoryListBox
			// 
			this.EventCategoryListBox.CheckOnClick = true;
			this.EventCategoryListBox.Font = new System.Drawing.Font("Calibri", 9F);
			this.EventCategoryListBox.Location = new System.Drawing.Point(71, 44);
			this.EventCategoryListBox.Name = "EventCategoryListBox";
			this.EventCategoryListBox.Size = new System.Drawing.Size(270, 106);
			this.EventCategoryListBox.Sorted = true;
			this.EventCategoryListBox.TabIndex = 6;
			this.EventCategoryListBox.ItemCheck += new System.Windows.Forms.ItemCheckEventHandler(this.OnEventCategoryCheck);
			// 
			// EndTimeControl
			// 
			this.EndTimeControl.DecimalPlaces = 2;
			this.EndTimeControl.Font = new System.Drawing.Font("Calibri", 9F);
			this.EndTimeControl.Location = new System.Drawing.Point(600, 18);
			this.EndTimeControl.Maximum = new decimal(new int[] {
            1410065407,
            2,
            0,
            131072});
			this.EndTimeControl.Name = "EndTimeControl";
			this.EndTimeControl.Size = new System.Drawing.Size(94, 22);
			this.EndTimeControl.TabIndex = 5;
			this.EndTimeControl.Value = new decimal(new int[] {
            1410065407,
            2,
            0,
            131072});
			this.EndTimeControl.ValueChanged += new System.EventHandler(this.OnEndTimeValueChanged);
			// 
			// TimeEndLabel
			// 
			this.TimeEndLabel.AutoSize = true;
			this.TimeEndLabel.Font = new System.Drawing.Font("Calibri", 9F);
			this.TimeEndLabel.Location = new System.Drawing.Point(537, 20);
			this.TimeEndLabel.Name = "TimeEndLabel";
			this.TimeEndLabel.Size = new System.Drawing.Size(57, 14);
			this.TimeEndLabel.TabIndex = 4;
			this.TimeEndLabel.Text = "End Time";
			// 
			// StartTimeControl
			// 
			this.StartTimeControl.DecimalPlaces = 2;
			this.StartTimeControl.Font = new System.Drawing.Font("Calibri", 9F);
			this.StartTimeControl.Location = new System.Drawing.Point(425, 17);
			this.StartTimeControl.Maximum = new decimal(new int[] {
            1874919423,
            2328306,
            0,
            131072});
			this.StartTimeControl.Name = "StartTimeControl";
			this.StartTimeControl.Size = new System.Drawing.Size(94, 22);
			this.StartTimeControl.TabIndex = 3;
			this.StartTimeControl.ValueChanged += new System.EventHandler(this.OnStartTimeValueChanged);
			// 
			// TimeStartLabel
			// 
			this.TimeStartLabel.AutoSize = true;
			this.TimeStartLabel.Font = new System.Drawing.Font("Calibri", 9F);
			this.TimeStartLabel.Location = new System.Drawing.Point(355, 20);
			this.TimeStartLabel.Name = "TimeStartLabel";
			this.TimeStartLabel.Size = new System.Drawing.Size(62, 14);
			this.TimeStartLabel.TabIndex = 2;
			this.TimeStartLabel.Text = "Start Time";
			// 
			// SearchTextBox
			// 
			this.SearchTextBox.Font = new System.Drawing.Font("Calibri", 9F);
			this.SearchTextBox.Location = new System.Drawing.Point(71, 17);
			this.SearchTextBox.Name = "SearchTextBox";
			this.SearchTextBox.Size = new System.Drawing.Size(270, 22);
			this.SearchTextBox.TabIndex = 1;
			this.SearchTextBox.TextChanged += new System.EventHandler(this.OnSearchTextChanged);
			// 
			// SearchLabel
			// 
			this.SearchLabel.AutoSize = true;
			this.SearchLabel.Font = new System.Drawing.Font("Calibri", 9F);
			this.SearchLabel.Location = new System.Drawing.Point(9, 20);
			this.SearchLabel.Name = "SearchLabel";
			this.SearchLabel.Size = new System.Drawing.Size(43, 14);
			this.SearchLabel.TabIndex = 0;
			this.SearchLabel.Text = "Search";
			// 
			// TreeViewGroupBox
			// 
			this.TreeViewGroupBox.AutoSize = true;
			this.TreeViewGroupBox.Controls.Add(this.MainTreeView);
			this.TreeViewGroupBox.Dock = System.Windows.Forms.DockStyle.Fill;
			this.TreeViewGroupBox.Location = new System.Drawing.Point(3, 185);
			this.TreeViewGroupBox.Name = "TreeViewGroupBox";
			this.TreeViewGroupBox.Size = new System.Drawing.Size(758, 374);
			this.TreeViewGroupBox.TabIndex = 1;
			this.TreeViewGroupBox.TabStop = false;
			this.TreeViewGroupBox.Text = "Results";
			// 
			// MainTreeView
			// 
			this.MainTreeView.Dock = System.Windows.Forms.DockStyle.Fill;
			this.MainTreeView.Location = new System.Drawing.Point(3, 20);
			this.MainTreeView.Name = "MainTreeView";
			this.MainTreeView.Size = new System.Drawing.Size(752, 351);
			this.MainTreeView.TabIndex = 0;
			this.MainTreeView.AfterCollapse += new System.Windows.Forms.TreeViewEventHandler(this.OnAfterNodeCollapse);
			this.MainTreeView.AfterExpand += new System.Windows.Forms.TreeViewEventHandler(this.OnAfterNodeExpand);
			// 
			// MainMenu
			// 
			this.MainMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FileMenu});
			this.MainMenu.Location = new System.Drawing.Point(0, 0);
			this.MainMenu.Name = "MainMenu";
			this.MainMenu.Size = new System.Drawing.Size(764, 20);
			this.MainMenu.TabIndex = 2;
			this.MainMenu.Text = "menuStrip1";
			// 
			// FileMenu
			// 
			this.FileMenu.DropDownItems.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.FileOpenMenuItem});
			this.FileMenu.Name = "FileMenu";
			this.FileMenu.Size = new System.Drawing.Size(37, 16);
			this.FileMenu.Text = "File";
			// 
			// FileOpenMenuItem
			// 
			this.FileOpenMenuItem.Name = "FileOpenMenuItem";
			this.FileOpenMenuItem.Size = new System.Drawing.Size(103, 22);
			this.FileOpenMenuItem.Text = "Open";
			this.FileOpenMenuItem.Click += new System.EventHandler(this.OnFileOpen);
			// 
			// OpenFileDialog
			// 
			this.OpenFileDialog.DefaultExt = "aiprof";
			this.OpenFileDialog.Filter = "AI Profiling Files|*.aiprof";
			this.OpenFileDialog.FileOk += new System.ComponentModel.CancelEventHandler(this.OnFileDialogOk);
			// 
			// DeferredFilterUpdateTimer
			// 
			this.DeferredFilterUpdateTimer.Enabled = true;
			this.DeferredFilterUpdateTimer.Interval = 250;
			this.DeferredFilterUpdateTimer.Tick += new System.EventHandler(this.OnDeferredFilterUpdateTick);
			// 
			// MainWindow
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(7F, 15F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(764, 562);
			this.Controls.Add(this.MainTableLayoutPanel);
			this.Font = new System.Drawing.Font("Calibri", 10F);
			this.MinimumSize = new System.Drawing.Size(730, 600);
			this.Name = "MainWindow";
			this.Text = "AI Profiler";
			this.MainTableLayoutPanel.ResumeLayout(false);
			this.MainTableLayoutPanel.PerformLayout();
			this.FilterGroupBox.ResumeLayout(false);
			this.FilterGroupBox.PerformLayout();
			((System.ComponentModel.ISupportInitialize)(this.EndTimeControl)).EndInit();
			((System.ComponentModel.ISupportInitialize)(this.StartTimeControl)).EndInit();
			this.TreeViewGroupBox.ResumeLayout(false);
			this.MainMenu.ResumeLayout(false);
			this.MainMenu.PerformLayout();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.TableLayoutPanel MainTableLayoutPanel;
		private System.Windows.Forms.OpenFileDialog OpenFileDialog;
		private System.Windows.Forms.GroupBox FilterGroupBox;
		private System.Windows.Forms.GroupBox TreeViewGroupBox;
		private System.Windows.Forms.MenuStrip MainMenu;
		private System.Windows.Forms.ToolStripMenuItem FileMenu;
		private System.Windows.Forms.ToolStripMenuItem FileOpenMenuItem;
		private System.Windows.Forms.TreeView MainTreeView;
		private System.Windows.Forms.Label SearchLabel;
		private System.Windows.Forms.TextBox SearchTextBox;
		private System.Windows.Forms.NumericUpDown StartTimeControl;
		private System.Windows.Forms.Label TimeStartLabel;
		private System.Windows.Forms.NumericUpDown EndTimeControl;
		private System.Windows.Forms.Label TimeEndLabel;
		private System.Windows.Forms.Timer DeferredFilterUpdateTimer;
		private System.Windows.Forms.CheckedListBox EventCategoryListBox;
		private System.Windows.Forms.Label EventCategoryLabel;
		private System.Windows.Forms.CheckedListBox ControllerClassListBox;
		private System.Windows.Forms.Label ControllerClassLabel;
	}
}

