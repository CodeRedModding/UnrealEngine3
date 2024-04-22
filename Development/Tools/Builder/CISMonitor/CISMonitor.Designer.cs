// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System.Windows.Forms;

namespace CISMonitor
{
    partial class CISMonitor : Form
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

        #region Component Designer generated code

        /// <summary> 
        /// Required method for Designer support - do not modify 
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
			this.components = new System.ComponentModel.Container();
			System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(CISMonitor));
			this.BuildState = new System.Windows.Forms.NotifyIcon(this.components);
			this.CISMonitorContextMenu = new System.Windows.Forms.ContextMenuStrip(this.components);
			this.ToolStripMenuItemBuildStatus5 = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemBuildStatus4 = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemBuildStatus3 = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemBuildStatus2 = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemBuildStatus1 = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripTopSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.ToolStripMenuItemStatus = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripMenuItemConfigure = new System.Windows.Forms.ToolStripMenuItem();
			this.ToolStripBottomSeparator = new System.Windows.Forms.ToolStripSeparator();
			this.ToolStripMenuItemExit = new System.Windows.Forms.ToolStripMenuItem();
			this.ConfigurationGrid = new System.Windows.Forms.PropertyGrid();
			this.ButtonOK = new System.Windows.Forms.Button();
			this.VersionText = new System.Windows.Forms.Label();
			this.CISMonitorContextMenu.SuspendLayout();
			this.SuspendLayout();
			// 
			// BuildState
			// 
			this.BuildState.ContextMenuStrip = this.CISMonitorContextMenu;
			this.BuildState.Visible = true;
			this.BuildState.BalloonTipClicked += new System.EventHandler(this.BalloonTipClicked);
			this.BuildState.BalloonTipClosed += new System.EventHandler(this.BalloonTipClosed);
			this.BuildState.BalloonTipShown += new System.EventHandler(this.BalloonTipShown);
			this.BuildState.MouseClick += new System.Windows.Forms.MouseEventHandler(this.ShowChangelists);
			this.BuildState.MouseMove += new System.Windows.Forms.MouseEventHandler(this.BuildStateMouseOver);
			// 
			// CISMonitorContextMenu
			// 
			this.CISMonitorContextMenu.AllowMerge = false;
			this.CISMonitorContextMenu.Items.AddRange(new System.Windows.Forms.ToolStripItem[] {
            this.ToolStripMenuItemBuildStatus5,
            this.ToolStripMenuItemBuildStatus4,
            this.ToolStripMenuItemBuildStatus3,
            this.ToolStripMenuItemBuildStatus2,
            this.ToolStripMenuItemBuildStatus1,
            this.ToolStripTopSeparator,
            this.ToolStripMenuItemStatus,
            this.ToolStripMenuItemConfigure,
            this.ToolStripBottomSeparator,
            this.ToolStripMenuItemExit});
			this.CISMonitorContextMenu.Name = "CISMonitorContextMenu";
			this.CISMonitorContextMenu.Size = new System.Drawing.Size(131, 192);
			// 
			// ToolStripMenuItemBuildStatus5
			// 
			this.ToolStripMenuItemBuildStatus5.Name = "ToolStripMenuItemBuildStatus5";
			this.ToolStripMenuItemBuildStatus5.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemBuildStatus5.Text = "Build Status";
			this.ToolStripMenuItemBuildStatus5.Click += new System.EventHandler(this.MenuItemBuildStatusClick);
			// 
			// ToolStripMenuItemBuildStatus4
			// 
			this.ToolStripMenuItemBuildStatus4.Name = "ToolStripMenuItemBuildStatus4";
			this.ToolStripMenuItemBuildStatus4.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemBuildStatus4.Text = "Build Status";
			this.ToolStripMenuItemBuildStatus4.Click += new System.EventHandler(this.MenuItemBuildStatusClick);
			// 
			// ToolStripMenuItemBuildStatus3
			// 
			this.ToolStripMenuItemBuildStatus3.Name = "ToolStripMenuItemBuildStatus3";
			this.ToolStripMenuItemBuildStatus3.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemBuildStatus3.Text = "Build Status";
			this.ToolStripMenuItemBuildStatus3.Click += new System.EventHandler(this.MenuItemBuildStatusClick);
			// 
			// ToolStripMenuItemBuildStatus2
			// 
			this.ToolStripMenuItemBuildStatus2.Name = "ToolStripMenuItemBuildStatus2";
			this.ToolStripMenuItemBuildStatus2.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemBuildStatus2.Text = "Build Status";
			this.ToolStripMenuItemBuildStatus2.Click += new System.EventHandler(this.MenuItemBuildStatusClick);
			// 
			// ToolStripMenuItemBuildStatus1
			// 
			this.ToolStripMenuItemBuildStatus1.Name = "ToolStripMenuItemBuildStatus1";
			this.ToolStripMenuItemBuildStatus1.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemBuildStatus1.Text = "Build Status";
			this.ToolStripMenuItemBuildStatus1.Click += new System.EventHandler(this.MenuItemBuildStatusClick);
			// 
			// ToolStripTopSeparator
			// 
			this.ToolStripTopSeparator.Name = "ToolStripTopSeparator";
			this.ToolStripTopSeparator.Size = new System.Drawing.Size(127, 6);
			// 
			// ToolStripMenuItemStatus
			// 
			this.ToolStripMenuItemStatus.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ToolStripMenuItemStatus.Font = new System.Drawing.Font("Segoe UI", 9F, System.Drawing.FontStyle.Bold, System.Drawing.GraphicsUnit.Point, ((byte)(0)));
			this.ToolStripMenuItemStatus.Name = "ToolStripMenuItemStatus";
			this.ToolStripMenuItemStatus.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemStatus.Text = "Status";
			this.ToolStripMenuItemStatus.Click += new System.EventHandler(this.MenuItemStatusClick);
			// 
			// ToolStripMenuItemConfigure
			// 
			this.ToolStripMenuItemConfigure.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ToolStripMenuItemConfigure.Name = "ToolStripMenuItemConfigure";
			this.ToolStripMenuItemConfigure.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemConfigure.Text = "Configure";
			this.ToolStripMenuItemConfigure.Click += new System.EventHandler(this.MenuItemConfigureClick);
			// 
			// ToolStripBottomSeparator
			// 
			this.ToolStripBottomSeparator.Name = "ToolStripBottomSeparator";
			this.ToolStripBottomSeparator.Size = new System.Drawing.Size(127, 6);
			// 
			// ToolStripMenuItemExit
			// 
			this.ToolStripMenuItemExit.DisplayStyle = System.Windows.Forms.ToolStripItemDisplayStyle.Text;
			this.ToolStripMenuItemExit.Name = "ToolStripMenuItemExit";
			this.ToolStripMenuItemExit.Size = new System.Drawing.Size(130, 22);
			this.ToolStripMenuItemExit.Text = "Exit";
			this.ToolStripMenuItemExit.Click += new System.EventHandler(this.ExitMenuItemClicked);
			// 
			// ConfigurationGrid
			// 
			this.ConfigurationGrid.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.ConfigurationGrid.Location = new System.Drawing.Point(1, 1);
			this.ConfigurationGrid.Name = "ConfigurationGrid";
			this.ConfigurationGrid.Size = new System.Drawing.Size(359, 259);
			this.ConfigurationGrid.TabIndex = 0;
			this.ConfigurationGrid.Tag = "Poo";
			// 
			// ButtonOK
			// 
			this.ButtonOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.ButtonOK.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.ButtonOK.Location = new System.Drawing.Point(274, 293);
			this.ButtonOK.Name = "ButtonOK";
			this.ButtonOK.Size = new System.Drawing.Size(75, 23);
			this.ButtonOK.TabIndex = 1;
			this.ButtonOK.Text = "OK";
			this.ButtonOK.UseVisualStyleBackColor = true;
			this.ButtonOK.Click += new System.EventHandler(this.ButtonOKClick);
			// 
			// VersionText
			// 
			this.VersionText.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.VersionText.AutoSize = true;
			this.VersionText.Location = new System.Drawing.Point(12, 297);
			this.VersionText.Name = "VersionText";
			this.VersionText.Size = new System.Drawing.Size(68, 13);
			this.VersionText.TabIndex = 2;
			this.VersionText.Text = "1st Jan 2001";
			// 
			// CISMonitor
			// 
			this.AcceptButton = this.ButtonOK;
			this.CancelButton = this.ButtonOK;
			this.ClientSize = new System.Drawing.Size(369, 360);
			this.ControlBox = false;
			this.Controls.Add(this.VersionText);
			this.Controls.Add(this.ButtonOK);
			this.Controls.Add(this.ConfigurationGrid);
			this.FormBorderStyle = System.Windows.Forms.FormBorderStyle.SizableToolWindow;
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MinimumSize = new System.Drawing.Size(320, 320);
			this.Name = "CISMonitor";
			this.ShowInTaskbar = false;
			this.Text = "CIS Monitor Configuration";
			this.CISMonitorContextMenu.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

        }

        #endregion

        private System.Windows.Forms.NotifyIcon BuildState;
        private PropertyGrid ConfigurationGrid;
        private Button ButtonOK;
        private ContextMenuStrip CISMonitorContextMenu;
        private ToolStripMenuItem ToolStripMenuItemConfigure;
        private ToolStripSeparator ToolStripBottomSeparator;
        private ToolStripMenuItem ToolStripMenuItemExit;
        private Label VersionText;
        private ToolStripMenuItem ToolStripMenuItemStatus;
		private ToolStripMenuItem ToolStripMenuItemBuildStatus1;
		private ToolStripSeparator ToolStripTopSeparator;
		private ToolStripMenuItem ToolStripMenuItemBuildStatus5;
		private ToolStripMenuItem ToolStripMenuItemBuildStatus4;
		private ToolStripMenuItem ToolStripMenuItemBuildStatus3;
		private ToolStripMenuItem ToolStripMenuItemBuildStatus2;
    }
}
