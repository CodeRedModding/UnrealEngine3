namespace Builder.UnrealSync
{
	public partial class UnrealSync2
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
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(UnrealSync2));
			this.UnrealSyncConfigurationGrid = new System.Windows.Forms.PropertyGrid();
			this.ButtonOK = new System.Windows.Forms.Button();
			this.VersionText = new System.Windows.Forms.Label();
			this.UnrealSyncContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.ToolStripTopSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.ToolStripFaveSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.ToolStripMenuItemShowLogs = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemConfigureBranches = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemPreferences = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemLaunchP4Win = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripBottomSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.ToolStripMenuItemExit = new System.Windows.Forms.ToolStripMenuItem();
			this.UnrealSync2NotifyIcon = new System.Windows.Forms.NotifyIcon(this.components);
			this.UnrealSyncContextMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// UnrealSyncConfigurationGrid
			// 
			this.UnrealSyncConfigurationGrid.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.UnrealSyncConfigurationGrid.Location = new System.Drawing.Point(0, 2);
			this.UnrealSyncConfigurationGrid.Name = "UnrealSyncConfigurationGrid";
			this.UnrealSyncConfigurationGrid.Size = new System.Drawing.Size(703, 441);
			this.UnrealSyncConfigurationGrid.TabIndex = 1;
			this.UnrealSyncConfigurationGrid.Tag = "Poo";
			// 
			// ButtonOK
			// 
			this.ButtonOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ButtonOK.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.ButtonOK.Location = new System.Drawing.Point(617, 458);
			this.ButtonOK.Name = "ButtonOK";
			this.ButtonOK.Size = new System.Drawing.Size(75, 23);
			this.ButtonOK.TabIndex = 2;
			this.ButtonOK.Text = "OK";
			this.ButtonOK.UseVisualStyleBackColor = true;
			this.ButtonOK.Click += new System.EventHandler(this.ButtonOKClick);
			// 
			// VersionText
			// 
			this.VersionText.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.VersionText.AutoSize = true;
			this.VersionText.Location = new System.Drawing.Point(12, 458);
			this.VersionText.Name = "VersionText";
			this.VersionText.Size = new System.Drawing.Size(68, 13);
			this.VersionText.TabIndex = 3;
			this.VersionText.Text = "1st Jan 2001";
			// 
			// UnrealSyncContextMenu
			// 
			this.UnrealSyncContextMenu.AllowMerge = false;
			this.UnrealSyncContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ToolStripTopSeparator,
            this.ToolStripFaveSeparator,
            this.ToolStripMenuItemShowLogs,
            this.ToolStripMenuItemConfigureBranches,
            this.ToolStripMenuItemPreferences,
            this.ToolStripMenuItemLaunchP4Win,
            this.ToolStripBottomSeparator,
            this.ToolStripMenuItemExit});
			this.UnrealSyncContextMenu.Name = "UnrealSyncContextMenu";
			this.UnrealSyncContextMenu.Size = new System.Drawing.Size(162, 132);
			// 
			// ToolStripTopSeparator
			// 
			this.ToolStripTopSeparator.Name = "ToolStripTopSeparator";
			this.ToolStripTopSeparator.Size = new System.Drawing.Size(158, 6);
			// 
			// ToolStripFaveSeparator
			// 
			this.ToolStripFaveSeparator.Name = "ToolStripFaveSeparator";
			this.ToolStripFaveSeparator.Size = new System.Drawing.Size(158, 6);
			// 
			// ToolStripMenuItemShowLogs
			// 
			this.ToolStripMenuItemShowLogs.Name = "ToolStripMenuItemShowLogs";
			this.ToolStripMenuItemShowLogs.Size = new System.Drawing.Size(161, 22);
			this.ToolStripMenuItemShowLogs.Text = "Show Logs";
			this.ToolStripMenuItemShowLogs.Click += new System.EventHandler(this.ContextMenuShowLogsClicked);
			// 
			// ToolStripMenuItemConfigureBranches
			// 
			this.ToolStripMenuItemConfigureBranches.Name = "ToolStripMenuItemConfigureBranches";
			this.ToolStripMenuItemConfigureBranches.Size = new System.Drawing.Size(161, 22);
			this.ToolStripMenuItemConfigureBranches.Text = "Configure Syncing";
			this.ToolStripMenuItemConfigureBranches.Click += new System.EventHandler(this.ContextMenuConfigureBranchesClicked);
			// 
			// ToolStripMenuItemPreferences
			// 
			this.ToolStripMenuItemPreferences.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ToolStripMenuItemPreferences.Name = "ToolStripMenuItemPreferences";
			this.ToolStripMenuItemPreferences.Size = new System.Drawing.Size(161, 22);
			this.ToolStripMenuItemPreferences.Text = "Preferences";
			this.ToolStripMenuItemPreferences.Click += new System.EventHandler(this.ContextMenuPreferencesClicked);
			// 
			// ToolStripMenuItemLaunchP4Win
			// 
			this.ToolStripMenuItemLaunchP4Win.Name = "ToolStripMenuItemLaunchP4Win";
			this.ToolStripMenuItemLaunchP4Win.Size = new System.Drawing.Size(161, 22);
			this.ToolStripMenuItemLaunchP4Win.Text = "Perforce";
			this.ToolStripMenuItemLaunchP4Win.Click += new System.EventHandler(this.ContextMenuP4WinClicked);
			// 
			// ToolStripBottomSeparator
			// 
			this.ToolStripBottomSeparator.Name = "ToolStripBottomSeparator";
			this.ToolStripBottomSeparator.Size = new System.Drawing.Size(158, 6);
			// 
			// ToolStripMenuItemExit
			// 
			this.ToolStripMenuItemExit.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ToolStripMenuItemExit.Name = "ToolStripMenuItemExit";
			this.ToolStripMenuItemExit.Size = new System.Drawing.Size(161, 22);
			this.ToolStripMenuItemExit.Text = "Exit";
			this.ToolStripMenuItemExit.Click += new System.EventHandler(this.ExitMenuItemClick);
			// 
			// UnrealSync2NotifyIcon
			// 
			this.UnrealSync2NotifyIcon.ContextMenuStrip = this.UnrealSyncContextMenu;
			this.UnrealSync2NotifyIcon.Visible = true;
			this.UnrealSync2NotifyIcon.BalloonTipClicked += new System.EventHandler(this.BalloonTipClicked);
			this.UnrealSync2NotifyIcon.Click += new System.EventHandler(this.TrayIconClicked);
			// 
			// UnrealSync2
			// 
			this.AcceptButton = this.ButtonOK;
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(704, 493);
			this.ControlBox = false;
			this.Controls.Add(this.VersionText);
			this.Controls.Add(this.ButtonOK);
			this.Controls.Add(this.UnrealSyncConfigurationGrid);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.MinimumSize = new System.Drawing.Size(300, 300);
			this.Name = "UnrealSync2";
			this.Text = "UnrealSync2 Configuration";
			this.UnrealSyncContextMenu.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.PropertyGrid UnrealSyncConfigurationGrid;
		private System.Windows.Forms.Button ButtonOK;
		private System.Windows.Forms.Label VersionText;
		private System.Windows.Forms.ContextMenuStrip UnrealSyncContextMenu;
		private System.Windows.Forms.ToolStripSeparator ToolStripTopSeparator;
		private System.Windows.Forms.ToolStripMenuItem ToolStripMenuItemPreferences;
		private System.Windows.Forms.ToolStripSeparator ToolStripBottomSeparator;
		private System.Windows.Forms.ToolStripMenuItem ToolStripMenuItemExit;
		private System.Windows.Forms.NotifyIcon UnrealSync2NotifyIcon;
		private System.Windows.Forms.ToolStripMenuItem ToolStripMenuItemConfigureBranches;
		private System.Windows.Forms.ToolStripMenuItem ToolStripMenuItemShowLogs;
		private System.Windows.Forms.ToolStripMenuItem ToolStripMenuItemLaunchP4Win;
		private System.Windows.Forms.ToolStripSeparator ToolStripFaveSeparator;
	}
}

