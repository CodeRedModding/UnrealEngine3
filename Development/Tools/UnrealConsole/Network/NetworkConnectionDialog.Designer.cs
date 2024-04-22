/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Windows.Forms;

namespace UnrealConsole
{
	/// <summary>
	/// This dialog shows the user a list of available game instances that
	/// can be connected to
	/// </summary>
	public partial class NetworkConnectionDialog : System.Windows.Forms.Form
	{
		#region Windows Form Designer generated code

		private ListView ConnectionList;
		private ColumnHeader TargetNameHeader;
		private ColumnHeader PlatformTypeHeader;
		private Button ConnectButton;
		private Button CancelBtn;
		private Button RefreshButton;
		private ColumnHeader TitleIPAddressHeader;
		private ColumnHeader DebugIPAddressHeader;
		private ColumnHeader TypeHeader;
		public CheckBox mCheckBox_ShowAllTargetInfo;

		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		/// <summary>
		/// Clean up any resources being used.
		/// </summary>
		protected override void Dispose( bool disposing )
		{
			if( disposing )
			{
				if( components != null )
				{
					components.Dispose();
				}
			}
			base.Dispose( disposing );
		}

		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			this.ConnectionList = new System.Windows.Forms.ListView();
			this.TargetNameHeader = new System.Windows.Forms.ColumnHeader();
			this.PlatformTypeHeader = new System.Windows.Forms.ColumnHeader();
			this.DebugIPAddressHeader = new System.Windows.Forms.ColumnHeader();
			this.TitleIPAddressHeader = new System.Windows.Forms.ColumnHeader();
			this.TypeHeader = new System.Windows.Forms.ColumnHeader();
			this.ConnectButton = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.RefreshButton = new System.Windows.Forms.Button();
			this.mCheckBox_ShowAllTargetInfo = new System.Windows.Forms.CheckBox();
			this.SuspendLayout();
			// 
			// ConnectionList
			// 
			this.ConnectionList.AllowColumnReorder = true;
			this.ConnectionList.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( ( ( System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom )
						| System.Windows.Forms.AnchorStyles.Left )
						| System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ConnectionList.Columns.AddRange( new System.Windows.Forms.ColumnHeader[] {
            this.TargetNameHeader,
            this.PlatformTypeHeader,
            this.DebugIPAddressHeader,
            this.TitleIPAddressHeader,
            this.TypeHeader} );
			this.ConnectionList.FullRowSelect = true;
			this.ConnectionList.HideSelection = false;
			this.ConnectionList.Location = new System.Drawing.Point( 12, 35 );
			this.ConnectionList.Name = "ConnectionList";
			this.ConnectionList.Size = new System.Drawing.Size( 576, 383 );
			this.ConnectionList.TabIndex = 0;
			this.ConnectionList.UseCompatibleStateImageBehavior = false;
			this.ConnectionList.View = System.Windows.Forms.View.Details;
			this.ConnectionList.ItemActivate += new System.EventHandler( this.ConnectionList_ItemActivate );
			this.ConnectionList.SelectedIndexChanged += new System.EventHandler( this.ConnectionList_SelectedIndexChanged );
			this.ConnectionList.ColumnClick += new System.Windows.Forms.ColumnClickEventHandler( this.ConnectionList_ColumnClick );
			// 
			// TargetNameHeader
			// 
			this.TargetNameHeader.Text = "Target";
			this.TargetNameHeader.Width = 170;
			// 
			// PlatformTypeHeader
			// 
			this.PlatformTypeHeader.Text = "Platform";
			this.PlatformTypeHeader.Width = 80;
			// 
			// DebugIPAddressHeader
			// 
			this.DebugIPAddressHeader.Text = "Debug IP Address";
			this.DebugIPAddressHeader.Width = 118;
			// 
			// TitleIPAddressHeader
			// 
			this.TitleIPAddressHeader.Text = "TitleIP Address";
			this.TitleIPAddressHeader.Width = 114;
			// 
			// TypeHeader
			// 
			this.TypeHeader.Text = "Type";
			this.TypeHeader.Width = 72;
			// 
			// ConnectButton
			// 
			this.ConnectButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.ConnectButton.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.ConnectButton.Enabled = false;
			this.ConnectButton.Location = new System.Drawing.Point( 432, 424 );
			this.ConnectButton.Name = "ConnectButton";
			this.ConnectButton.Size = new System.Drawing.Size( 75, 23 );
			this.ConnectButton.TabIndex = 4;
			this.ConnectButton.Text = "&Connect";
			// 
			// CancelBtn
			// 
			this.CancelBtn.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right ) ) );
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point( 512, 424 );
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.Size = new System.Drawing.Size( 75, 23 );
			this.CancelBtn.TabIndex = 5;
			this.CancelBtn.Text = "C&ancel";
			// 
			// RefreshButton
			// 
			this.RefreshButton.Anchor = ( ( System.Windows.Forms.AnchorStyles )( ( System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left ) ) );
			this.RefreshButton.Location = new System.Drawing.Point( 325, 424 );
			this.RefreshButton.Name = "RefreshButton";
			this.RefreshButton.Size = new System.Drawing.Size( 75, 23 );
			this.RefreshButton.TabIndex = 6;
			this.RefreshButton.Text = "&Refresh";
			this.RefreshButton.Click += new System.EventHandler( this.RefreshButton_Click );
			// 
			// mCheckBox_ShowAllTargetInfo
			// 
			this.mCheckBox_ShowAllTargetInfo.AutoSize = true;
			this.mCheckBox_ShowAllTargetInfo.Location = new System.Drawing.Point( 12, 12 );
			this.mCheckBox_ShowAllTargetInfo.Name = "mCheckBox_ShowAllTargetInfo";
			this.mCheckBox_ShowAllTargetInfo.Size = new System.Drawing.Size( 175, 19 );
			this.mCheckBox_ShowAllTargetInfo.TabIndex = 7;
			this.mCheckBox_ShowAllTargetInfo.Text = "Show All Target Information";
			this.mCheckBox_ShowAllTargetInfo.UseVisualStyleBackColor = true;
			this.mCheckBox_ShowAllTargetInfo.CheckedChanged += new System.EventHandler( this.AllTargetInfo_CheckChanged );
			// 
			// NetworkConnectionDialog
			// 
			this.AcceptButton = this.ConnectButton;
			this.AutoScaleBaseSize = new System.Drawing.Size( 6, 16 );
			this.CancelButton = this.CancelBtn;
			this.ClientSize = new System.Drawing.Size( 600, 454 );
			this.ControlBox = false;
			this.Controls.Add( this.mCheckBox_ShowAllTargetInfo );
			this.Controls.Add( this.RefreshButton );
			this.Controls.Add( this.CancelBtn );
			this.Controls.Add( this.ConnectButton );
			this.Controls.Add( this.ConnectionList );
			this.Font = new System.Drawing.Font( "Segoe UI", 9F, System.Drawing.FontStyle.Regular, System.Drawing.GraphicsUnit.Point, ( ( byte )( 0 ) ) );
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "NetworkConnectionDialog";
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Available Targets";
			this.Shown += new System.EventHandler( this.NetworkConnectionShown );
			this.Closing += new System.ComponentModel.CancelEventHandler( this.NetworkConnectionDialog_Closing );
			this.ResumeLayout( false );
			this.PerformLayout();

		}
		#endregion

	}
}

