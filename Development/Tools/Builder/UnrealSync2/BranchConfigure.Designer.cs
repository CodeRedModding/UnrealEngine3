namespace Builder.UnrealSync
{
	partial class BranchConfigure
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
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(BranchConfigure));
			this.BranchDataGridView = new System.Windows.Forms.DataGridView();
			this.ButtonAccept = new System.Windows.Forms.Button();
			this.DisplayInMenu = new System.Windows.Forms.DataGridViewCheckBoxColumn();
			this.PerforceClient = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.BranchName = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.SyncTime = new System.Windows.Forms.DataGridViewComboBoxColumn();
			this.SyncType = new System.Windows.Forms.DataGridViewComboBoxColumn();
			this.HeadChangelist = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.LastGoodCIS = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.LatestBuild = new System.Windows.Forms.DataGridViewTextBoxColumn();
			this.QABuild = new System.Windows.Forms.DataGridViewTextBoxColumn();
			((System.ComponentModel.ISupportInitialize)(this.BranchDataGridView)).BeginInit();
			this.SuspendLayout();
			// 
			// BranchDataGridView
			// 
			this.BranchDataGridView.AllowUserToAddRows = false;
			this.BranchDataGridView.AllowUserToDeleteRows = false;
			this.BranchDataGridView.AllowUserToOrderColumns = true;
			this.BranchDataGridView.AllowUserToResizeRows = false;
			this.BranchDataGridView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.BranchDataGridView.AutoSizeColumnsMode = System.Windows.Forms.DataGridViewAutoSizeColumnsMode.Fill;
			this.BranchDataGridView.BackgroundColor = System.Drawing.SystemColors.Control;
			this.BranchDataGridView.ColumnHeadersHeightSizeMode = System.Windows.Forms.DataGridViewColumnHeadersHeightSizeMode.AutoSize;
			this.BranchDataGridView.Columns.AddRange(new System.Windows.Forms.DataGridViewColumn[] {
            this.DisplayInMenu,
            this.PerforceClient,
            this.BranchName,
            this.SyncTime,
            this.SyncType,
            this.HeadChangelist,
            this.LastGoodCIS,
            this.LatestBuild,
            this.QABuild});
			this.BranchDataGridView.GridColor = System.Drawing.SystemColors.Control;
			this.BranchDataGridView.Location = new System.Drawing.Point(1, 1);
			this.BranchDataGridView.Name = "BranchDataGridView";
			this.BranchDataGridView.Size = new System.Drawing.Size(1323, 241);
			this.BranchDataGridView.TabIndex = 1;
			// 
			// ButtonAccept
			// 
			this.ButtonAccept.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ButtonAccept.Location = new System.Drawing.Point(1237, 257);
			this.ButtonAccept.Name = "ButtonAccept";
			this.ButtonAccept.Size = new System.Drawing.Size(75, 23);
			this.ButtonAccept.TabIndex = 6;
			this.ButtonAccept.Text = "OK";
			this.ButtonAccept.UseVisualStyleBackColor = true;
			this.ButtonAccept.Click += new System.EventHandler(this.BranchConfigureOKButtonClicked);
			// 
			// DisplayInMenu
			// 
			this.DisplayInMenu.FillWeight = 25F;
			this.DisplayInMenu.HeaderText = "Display in Menus";
			this.DisplayInMenu.Name = "DisplayInMenu";
			this.DisplayInMenu.ToolTipText = "Display this branch when right clicking UnrealSync2 in the notify tray.";
			// 
			// PerforceClient
			// 
			this.PerforceClient.FillWeight = 75F;
			this.PerforceClient.HeaderText = "Perforce Client";
			this.PerforceClient.Name = "PerforceClient";
			this.PerforceClient.ReadOnly = true;
			this.PerforceClient.ToolTipText = "The name of the clientspec in Perforce";
			// 
			// BranchName
			// 
			this.BranchName.FillWeight = 75F;
			this.BranchName.HeaderText = "Branch Name";
			this.BranchName.Name = "BranchName";
			this.BranchName.ReadOnly = true;
			this.BranchName.ToolTipText = "The name of the branch (deduced by the existence of build.properties and BaseEngi" +
    "ne.ini)";
			// 
			// SyncTime
			// 
			this.SyncTime.FillWeight = 50F;
			this.SyncTime.HeaderText = "Sync Time";
			this.SyncTime.Items.AddRange(new object[] {
            "Never",
            "3:00 AM",
            "3:30 AM",
            "4:00 AM",
            "4:30 AM",
            "5:00 AM",
            "5:30 AM",
            "6:00 AM",
            "6:30 AM",
            "7:00 AM",
            "7:30 AM",
            "8:00 AM",
            "8:30 AM",
            "9:00 AM",
            "9:30 AM",
            "10:00 AM",
            "10:30 AM",
            "11:00 AM",
            "11:30 AM",
            "12:00 PM"});
			this.SyncTime.Name = "SyncTime";
			this.SyncTime.SortMode = System.Windows.Forms.DataGridViewColumnSortMode.Automatic;
			this.SyncTime.ToolTipText = "The time to start the sync for this branch.";
			// 
			// SyncType
			// 
			this.SyncType.FillWeight = 50F;
			this.SyncType.HeaderText = "Sync Type";
			this.SyncType.Name = "SyncType";
			// 
			// HeadChangelist
			// 
			this.HeadChangelist.FillWeight = 40F;
			this.HeadChangelist.HeaderText = "#head";
			this.HeadChangelist.Name = "HeadChangelist";
			this.HeadChangelist.ReadOnly = true;
			// 
			// LastGoodCIS
			// 
			this.LastGoodCIS.FillWeight = 40F;
			this.LastGoodCIS.HeaderText = "Last Good CIS";
			this.LastGoodCIS.Name = "LastGoodCIS";
			this.LastGoodCIS.ReadOnly = true;
			// 
			// LatestBuild
			// 
			this.LatestBuild.HeaderText = "Latest Build";
			this.LatestBuild.Name = "LatestBuild";
			this.LatestBuild.ReadOnly = true;
			this.LatestBuild.ToolTipText = "The latest unpromoted build built in this branch by the build system.";
			// 
			// QABuild
			// 
			this.QABuild.HeaderText = "QA Build";
			this.QABuild.Name = "QABuild";
			this.QABuild.ReadOnly = true;
			// 
			// BranchConfigure
			// 
			this.AcceptButton = this.ButtonAccept;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(1324, 292);
			this.Controls.Add(this.ButtonAccept);
			this.Controls.Add(this.BranchDataGridView);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.Name = "BranchConfigure";
			this.Text = "Configure Branches";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.BranchConfigureClosed);
			((System.ComponentModel.ISupportInitialize)(this.BranchDataGridView)).EndInit();
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.DataGridView BranchDataGridView;
		private System.Windows.Forms.Button ButtonAccept;
		private System.Windows.Forms.DataGridViewCheckBoxColumn DisplayInMenu;
		private System.Windows.Forms.DataGridViewTextBoxColumn PerforceClient;
		private System.Windows.Forms.DataGridViewTextBoxColumn BranchName;
		private System.Windows.Forms.DataGridViewComboBoxColumn SyncTime;
		private System.Windows.Forms.DataGridViewComboBoxColumn SyncType;
		private System.Windows.Forms.DataGridViewTextBoxColumn HeadChangelist;
		private System.Windows.Forms.DataGridViewTextBoxColumn LastGoodCIS;
		private System.Windows.Forms.DataGridViewTextBoxColumn LatestBuild;
		private System.Windows.Forms.DataGridViewTextBoxColumn QABuild;
	}
}