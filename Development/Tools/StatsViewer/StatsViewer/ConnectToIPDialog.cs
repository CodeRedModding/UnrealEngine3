/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Drawing;
using System.Collections;
using System.ComponentModel;
using System.Windows.Forms;
using System.Net;
using System.Net.Sockets;

namespace StatsViewer
{
	/// <summary>
	/// Displays a simple entry for entering an IP address to connect to
	/// </summary>
	public class ConnectToIPDialog : System.Windows.Forms.Form
	{
		private System.Windows.Forms.Button ConnectBtn;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.TextBox IpAddrEdit;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.TextBox PortEdit;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;

		public ConnectToIPDialog()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();
		}

		/// <summary>
		/// Sets the edit controls to specific strings
		/// </summary>
		/// <param name="Ip">The ip address to use</param>
		/// <param name="Port">The port to use</param>
		public ConnectToIPDialog(string Ip,string Port)
		{
			// Required for Windows Form Designer support
			InitializeComponent();
			// Assign the port and ip
			PortEdit.Text = Port;
			IpAddrEdit.Text = Ip;
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
		/// Returns the server connection object for the entered IP address
		/// </summary>
		/// <returns>A new sever connection object</returns>
		public ServerConnection GetServerConnection()
		{
			return new ServerConnection(IPAddress.Parse(IpAddrEdit.Text),13000,
				Convert.ToInt32(PortEdit.Text));
		}

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
			System.Resources.ResourceManager resources = new System.Resources.ResourceManager(typeof(ConnectToIPDialog));
			this.ConnectBtn = new System.Windows.Forms.Button();
			this.CancelBtn = new System.Windows.Forms.Button();
			this.IpAddrEdit = new System.Windows.Forms.TextBox();
			this.label1 = new System.Windows.Forms.Label();
			this.label2 = new System.Windows.Forms.Label();
			this.PortEdit = new System.Windows.Forms.TextBox();
			this.SuspendLayout();
			// 
			// ConnectBtn
			// 
			this.ConnectBtn.DialogResult = System.Windows.Forms.DialogResult.OK;
			this.ConnectBtn.Location = new System.Drawing.Point(120, 88);
			this.ConnectBtn.Name = "ConnectBtn";
			this.ConnectBtn.TabIndex = 0;
			this.ConnectBtn.Text = "&Connect";
			// 
			// CancelBtn
			// 
			this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.CancelBtn.Location = new System.Drawing.Point(200, 88);
			this.CancelBtn.Name = "CancelBtn";
			this.CancelBtn.TabIndex = 1;
			this.CancelBtn.Text = "C&ancel";
			// 
			// IpAddrEdit
			// 
			this.IpAddrEdit.Location = new System.Drawing.Point(48, 16);
			this.IpAddrEdit.Name = "IpAddrEdit";
			this.IpAddrEdit.Size = new System.Drawing.Size(224, 20);
			this.IpAddrEdit.TabIndex = 2;
			this.IpAddrEdit.Text = "127.0.0.1";
			// 
			// label1
			// 
			this.label1.Location = new System.Drawing.Point(16, 16);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(24, 16);
			this.label1.TabIndex = 3;
			this.label1.Text = "&IP:";
			// 
			// label2
			// 
			this.label2.Location = new System.Drawing.Point(16, 48);
			this.label2.Name = "label2";
			this.label2.Size = new System.Drawing.Size(32, 16);
			this.label2.TabIndex = 4;
			this.label2.Text = "&Port:";
			// 
			// PortEdit
			// 
			this.PortEdit.Location = new System.Drawing.Point(48, 48);
			this.PortEdit.Name = "PortEdit";
			this.PortEdit.Size = new System.Drawing.Size(224, 20);
			this.PortEdit.TabIndex = 5;
			this.PortEdit.Text = "13002";   // Must match default "StatsTrafficPort" in UDP stat provider
			// 
			// ConnectToIPDialog
			// 
			this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
			this.ClientSize = new System.Drawing.Size(292, 126);
			this.Controls.Add(this.PortEdit);
			this.Controls.Add(this.IpAddrEdit);
			this.Controls.Add(this.label2);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.CancelBtn);
			this.Controls.Add(this.ConnectBtn);
			this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "ConnectToIPDialog";
			this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
			this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
			this.Text = "Connect to IP...";
			this.Closing += new System.ComponentModel.CancelEventHandler(this.ConnectToIPDialog_Closing);
			this.ResumeLayout(false);

		}
		#endregion


		/// <summary>
		/// Validates whether the IP address can be used
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ConnectToIPDialog_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			Form SenderForm = sender as Form;
			if( SenderForm.DialogResult == DialogResult.OK )
			{
				try
				{
					IPAddress.Parse( IpAddrEdit.Text );
				}
				catch( Exception Except )
				{
					MessageBox.Show( "Invalid IP address entered" );
					Console.WriteLine( Except.ToString() );
					e.Cancel = true;
				}
			}
		}
	}
}
