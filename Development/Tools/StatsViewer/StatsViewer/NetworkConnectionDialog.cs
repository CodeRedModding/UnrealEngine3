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
using System.Threading;
using System.Text;
using Stats;

namespace StatsViewer
{
	/// <summary>
	/// This dialog shows the user a list of available game instances that
	/// can be connected to
	/// </summary>
	public class NetworkConnectionDialog : System.Windows.Forms.Form
	{
		/// <summary>
		/// Holds the address and server information for a potential server
		/// </summary>
		private class GameServer
		{
			/// <summary>
			/// The ip address of the server
			/// </summary>
			public IPEndPoint Address;
			/// <summary>
			/// The name of the computer running the game
			/// </summary>
			public string ComputerName;
			/// <summary>
			/// The name of the game that is running (UT, War, etc.)
			/// </summary>
			public string GameName;
			/// <summary>
			/// The type of game that is running
			/// </summary>
			public GameType ServerGameType;
			/// <summary>
			/// The platform it is running on
			/// </summary>
			public PlatformType ServerPlatformType;
			/// <summary>
			/// The port that the server is sending stats information on
			/// </summary>
			public int StatsPortNo;

			/// <summary>
			/// Constructor that copies the address and initializes the vars
			/// from the byte stream
			/// </summary>
			/// <param name="InAddress">The ip address of the server</param>
			/// <param name="Bytes">The byte stream from this server</param>
			public GameServer(IPEndPoint InAddress,Byte[] Bytes)
			{
				Address = InAddress;
				// Parse the byte stream into our members
				int CurrentOffset = 2;
				// Decode the computer name from the packet
				ComputerName = ByteStreamConverter.ToString(Bytes,ref CurrentOffset);
				// Now get the game name string
				GameName = ByteStreamConverter.ToString(Bytes,ref CurrentOffset);
				// Now decode the game type & platform type
				ServerGameType = (GameType)Bytes[CurrentOffset++];
				ServerPlatformType = (PlatformType)Bytes[CurrentOffset++];
				// Get the port to listen on from the packet
				StatsPortNo = ByteStreamConverter.ToInt(Bytes,ref CurrentOffset);
			}
		}
		/// <summary>
		/// Class that handles receiving async UDP packets
		/// </summary>
		private AsyncUdpClient ResponseClient;
		/// <summary>
		/// Holds the UdpClient used to broadcast server announcement requests
		/// </summary>
		private UdpClient BroadcastClient;
		/// <summary>
		/// Our timer for polling our server response socket
		/// </summary>
		private System.Windows.Forms.Timer ResponseTimer = new System.Windows.Forms.Timer();
		/// <summary>
		/// The list of servers that have responded
		/// </summary>
		private ArrayList Servers = new ArrayList();
		/// <summary>
		/// Holds the selected server item
		/// </summary>
		private GameServer SelectedServer;
		/// <summary>
		/// The port we are sending query requests on
		/// </summary>
		private int Port;

		#region Windows Form Designer generated code
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.ListView ConnectionList;
		private System.Windows.Forms.ColumnHeader ComputerName;
		private System.Windows.Forms.ColumnHeader GameType;
		private System.Windows.Forms.ColumnHeader PlatformType;
		private System.Windows.Forms.ColumnHeader GameName;
		private System.Windows.Forms.Label label2;
		private System.Windows.Forms.Button ConnectButton;
		private System.Windows.Forms.Button CancelBtn;
		private System.Windows.Forms.Button RefreshButton;
		private System.Windows.Forms.TextBox PortNo;
        private ColumnHeader IP;
		/// <summary>
		/// Required designer variable.
		/// </summary>
		private System.ComponentModel.Container components = null;
		#endregion

		/// <summary>
		/// Default constructor
		/// </summary>
		public NetworkConnectionDialog()
		{
			//
			// Required for Windows Form Designer support
			//
			InitializeComponent();
			// Add the callback for our timer
			ResponseTimer.Tick += new EventHandler(OnTimer);
			// And now kick it off
			ResponseTimer.Interval = 500;
			ResponseTimer.Start();
			// Init connection list view
			ConnectionList.View = View.Details;
			ConnectionList.LabelEdit = false;
			ConnectionList.AllowColumnReorder = true;
			ConnectionList.CheckBoxes = false;
			ConnectionList.FullRowSelect = true;
			ConnectionList.GridLines = false;
			ConnectionList.Sorting = SortOrder.Ascending;
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

		#region Windows Form Designer generated code
		/// <summary>
		/// Required method for Designer support - do not modify
		/// the contents of this method with the code editor.
		/// </summary>
		private void InitializeComponent()
		{
            System.ComponentModel.ComponentResourceManager resources = new System.ComponentModel.ComponentResourceManager(typeof(NetworkConnectionDialog));
            this.ConnectionList = new System.Windows.Forms.ListView();
            this.ComputerName = new System.Windows.Forms.ColumnHeader();
            this.GameName = new System.Windows.Forms.ColumnHeader();
            this.GameType = new System.Windows.Forms.ColumnHeader();
            this.PlatformType = new System.Windows.Forms.ColumnHeader();
            this.label1 = new System.Windows.Forms.Label();
            this.label2 = new System.Windows.Forms.Label();
            this.PortNo = new System.Windows.Forms.TextBox();
            this.ConnectButton = new System.Windows.Forms.Button();
            this.CancelBtn = new System.Windows.Forms.Button();
            this.RefreshButton = new System.Windows.Forms.Button();
            this.IP = new System.Windows.Forms.ColumnHeader();
            this.SuspendLayout();
            // 
            // ConnectionList
            // 
            this.ConnectionList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom)
                        | System.Windows.Forms.AnchorStyles.Left)
                        | System.Windows.Forms.AnchorStyles.Right)));
            this.ConnectionList.Columns.AddRange(new System.Windows.Forms.ColumnHeader[] {
            this.ComputerName,
            this.IP,
            this.GameName,
            this.GameType,
            this.PlatformType});
            this.ConnectionList.Location = new System.Drawing.Point(16, 32);
            this.ConnectionList.Name = "ConnectionList";
            this.ConnectionList.Size = new System.Drawing.Size(545, 405);
            this.ConnectionList.TabIndex = 0;
            this.ConnectionList.UseCompatibleStateImageBehavior = false;
            this.ConnectionList.ItemActivate += new System.EventHandler(this.ConnectionList_ItemActivate);
            // 
            // ComputerName
            // 
            this.ComputerName.Text = "Computer Name";
            this.ComputerName.Width = 120;
            // 
            // GameName
            // 
            this.GameName.Text = "Game";
            this.GameName.Width = -2;
            // 
            // GameType
            // 
            this.GameType.Text = "Game Type";
            this.GameType.Width = -2;
            // 
            // PlatformType
            // 
            this.PlatformType.Text = "Platform";
            this.PlatformType.Width = -2;
            // 
            // label1
            // 
            this.label1.Location = new System.Drawing.Point(16, 14);
            this.label1.Name = "label1";
            this.label1.Size = new System.Drawing.Size(100, 23);
            this.label1.TabIndex = 1;
            this.label1.Text = "Available Games:";
            // 
            // label2
            // 
            this.label2.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.label2.Location = new System.Drawing.Point(16, 445);
            this.label2.Name = "label2";
            this.label2.Size = new System.Drawing.Size(64, 23);
            this.label2.TabIndex = 2;
            this.label2.Text = "&Query Port:";
            // 
            // PortNo
            // 
            this.PortNo.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.PortNo.Location = new System.Drawing.Point(88, 445);
            this.PortNo.Name = "PortNo";
            this.PortNo.Size = new System.Drawing.Size(40, 20);
            this.PortNo.TabIndex = 3;
            this.PortNo.Text = "13000";
            this.PortNo.TextChanged += new System.EventHandler(this.PortNo_TextChanged);
            // 
            // ConnectButton
            // 
            this.ConnectButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.ConnectButton.DialogResult = System.Windows.Forms.DialogResult.OK;
            this.ConnectButton.Location = new System.Drawing.Point(409, 485);
            this.ConnectButton.Name = "ConnectButton";
            this.ConnectButton.Size = new System.Drawing.Size(75, 23);
            this.ConnectButton.TabIndex = 4;
            this.ConnectButton.Text = "&Connect";
            // 
            // CancelBtn
            // 
            this.CancelBtn.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
            this.CancelBtn.DialogResult = System.Windows.Forms.DialogResult.Cancel;
            this.CancelBtn.Location = new System.Drawing.Point(489, 485);
            this.CancelBtn.Name = "CancelBtn";
            this.CancelBtn.Size = new System.Drawing.Size(75, 23);
            this.CancelBtn.TabIndex = 5;
            this.CancelBtn.Text = "C&ancel";
            // 
            // RefreshButton
            // 
            this.RefreshButton.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
            this.RefreshButton.Location = new System.Drawing.Point(144, 445);
            this.RefreshButton.Name = "RefreshButton";
            this.RefreshButton.Size = new System.Drawing.Size(75, 23);
            this.RefreshButton.TabIndex = 6;
            this.RefreshButton.Text = "&Refresh";
            this.RefreshButton.Click += new System.EventHandler(this.RefreshButton_Click);
            // 
            // IP
            // 
            this.IP.Text = "IP";
            this.IP.Width = 120;
            // 
            // NetworkConnectionDialog
            // 
            this.AcceptButton = this.ConnectButton;
            this.AutoScaleBaseSize = new System.Drawing.Size(5, 13);
            this.CancelButton = this.CancelBtn;
            this.ClientSize = new System.Drawing.Size(577, 515);
            this.ControlBox = false;
            this.Controls.Add(this.RefreshButton);
            this.Controls.Add(this.CancelBtn);
            this.Controls.Add(this.ConnectButton);
            this.Controls.Add(this.PortNo);
            this.Controls.Add(this.label2);
            this.Controls.Add(this.ConnectionList);
            this.Controls.Add(this.label1);
            this.Icon = ((System.Drawing.Icon)(resources.GetObject("$this.Icon")));
            this.MaximizeBox = false;
            this.MinimizeBox = false;
            this.Name = "NetworkConnectionDialog";
            this.SizeGripStyle = System.Windows.Forms.SizeGripStyle.Hide;
            this.StartPosition = System.Windows.Forms.FormStartPosition.CenterParent;
            this.Text = "Connect To";
            this.Closing += new System.ComponentModel.CancelEventHandler(this.NetworkConnectionDialog_Closing);
            this.Load += new System.EventHandler(this.NetworkConnectionDialog_Load);
            this.ResumeLayout(false);
            this.PerformLayout();

		}
		#endregion


		/// <summary>
		/// Creates a UdpClient object to send our announcement requests on
		/// </summary>
		private void CreateUdpClients()
		{
			ResponseTimer.Stop();
			// Close any previous connection
			if (BroadcastClient != null)
			{
				BroadcastClient.Close();
			}
			if (ResponseClient != null)
			{
				ResponseClient.StopReceiving();
			}
			Port = Convert.ToInt32(PortNo.Text);
			// Connect to the broadcast address
			BroadcastClient = new UdpClient();
			BroadcastClient.Connect(IPAddress.Broadcast,Port);
			// Create our connection that we'll read from
			ResponseClient = new AsyncUdpClient(Port + 1);
			ResponseClient.StartReceiving();

			ResponseTimer.Start();
		}

		/// <summary>
		/// Checks the socket for server announcement response packets
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void OnTimer(Object sender,EventArgs e)
		{
			ResponseTimer.Stop();
			try
			{
				// While there are packets to be processed
				for (Packet packet = ResponseClient.GetNextPacket();
					packet != null;
					packet = ResponseClient.GetNextPacket())
				{
					GameServer Server = new GameServer(packet.Address,
						packet.Data);
					// Add the server info with IP addr
					ListViewItem lvi = new ListViewItem(Server.ComputerName);
                    lvi.SubItems.Add(Server.Address.ToString());
                    lvi.SubItems.Add(Server.GameName);
					lvi.SubItems.Add(Server.ServerGameType.ToString());
					lvi.SubItems.Add(Server.ServerPlatformType.ToString());
					lvi.Tag = Server;
					// Add the data to the ui
					ConnectionList.Items.Add(lvi);
				}
			}
			catch (Exception E)
			{
				Console.WriteLine("Exception during listening:\r\n" + E.ToString());
			}
			finally
			{
				ResponseTimer.Start();
			}
		}

		/// <summary>
		/// Sends a "server announcement" request on the broadcast address
		/// </summary>
		private void BroadcastServerAnnounceRequest()
		{
			Byte[] BytesToSend = Encoding.ASCII.GetBytes("SA");
			BroadcastClient.Send(BytesToSend,BytesToSend.Length);
		}

		/// <summary>
		/// Creates a UDP beacon to broadcast server announcement requestes.
		/// It then populates the list based upon the results that come back
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void NetworkConnectionDialog_Load(object sender, System.EventArgs e)
		{
			CreateUdpClients();
			BroadcastServerAnnounceRequest();
		}

		/// <summary>
		/// Rebuilds the connection and sends the server announce request
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void RefreshButton_Click(object sender, System.EventArgs e)
		{
			CreateUdpClients();
			// Clear out our cached lists of servers
			Servers.Clear();
			ConnectionList.Items.Clear();
			// Now ask for responses
			BroadcastServerAnnounceRequest();
		}

		/// <summary>
		/// Disable the refresh button if there is no text in the port no control
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void PortNo_TextChanged(object sender, System.EventArgs e)
		{
			try
			{
				// Disable the refresh button if the text is invalid
				RefreshButton.Enabled = PortNo.Text.Length > 0 &&
					Convert.ToInt32(PortNo.Text) > 0;
			}
			catch (Exception)
			{
				// This means there were non-numerics in there
				RefreshButton.Enabled = false;
			}
		}

		/// <summary>
		/// Shuts down any sockets and sets our out variables
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void NetworkConnectionDialog_Closing(object sender, System.ComponentModel.CancelEventArgs e)
		{
			ResponseTimer.Stop();
			if (BroadcastClient != null)
			{
				BroadcastClient.Close();
			}
			// Shutdown our async upd receiver so the socket isn't left bound
			if (ResponseClient != null)
			{
				ResponseClient.StopReceiving();
			}
			// Store the selected item as the server to connect to
			if (ConnectionList.SelectedItems.Count > 0)
			{
				SelectedServer = (GameServer)ConnectionList.SelectedItems[0].Tag;
			}
		}

		/// <summary>
		/// Returns the server connection object for the selected server
		/// </summary>
		/// <returns>A new sever connection object</returns>
		public ServerConnection GetServerConnection()
		{
			if (SelectedServer != null)
			{
				return new ServerConnection(SelectedServer.Address.Address, Port,
					SelectedServer.StatsPortNo);
			}
			return null;
		}

		/// <summary>
		/// Handles double clicking on a specific server. Same as clicking once
		/// and closing the dialog via Connect
		/// </summary>
		/// <param name="sender"></param>
		/// <param name="e"></param>
		private void ConnectionList_ItemActivate(object sender, System.EventArgs e)
		{
			DialogResult = DialogResult.OK;
			Close();
		}
	}
}
