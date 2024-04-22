/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Net;
using System.Net.Sockets;
using System.Text;

namespace StatsViewer
{
	/// <summary>
	/// Maintains a connection to a server. Holds both the send and receive 
	/// connections to the server
	/// </summary>
	public class ServerConnection
	{
		/// <summary>
		/// The IP address of the server we are communicating with
		/// </summary>
		private IPAddress ServerAddress;
		/// <summary>
		/// The port to send server requests to
		/// </summary>
		private int RequestPortNo;
		/// <summary>
		/// The port to listen for server responses on
		/// </summary>
		private int ListenPortNo;
		/// <summary>
		/// Used to receive data from the remote server
		/// </summary>
		private AsyncUdpClient ListenClient;
		/// <summary>
		/// Used to send commands to the remote server
		/// </summary>
		private UdpClient RequestClient;

		/// <summary>
		/// Copies the specified values into the appropriate members. Does
		/// not create the underlying socket connections. That is done in
		/// Connect(). Use Disconnect() to stop all socket processing.
		/// </summary>
		/// <param name="InAddress">The address we'll be communicating with</param>
		/// <param name="InRequestPortNo">The port to send server requests to</param>
		/// <param name="InListenPortNo">The port to listen for server responses on</param>
		public ServerConnection(IPAddress InAddress,int InRequestPortNo,
			int InListenPortNo)
		{
			ServerAddress = InAddress;
			RequestPortNo = InRequestPortNo;
			ListenPortNo = InListenPortNo;
		}

		/// <summary>
		/// Connects the sockets to their corresponing ports
		/// </summary>
		public void Connect()
		{
			RequestClient = new UdpClient();

			// Connect to the server's listener
			RequestClient.Connect(ServerAddress,RequestPortNo);

			// Create our connection that we'll read from
			ListenClient = new AsyncUdpClient(ListenPortNo);
			ListenClient.StartReceiving();
		}

		/// <summary>
		/// Closes both of the connections
		/// </summary>
		public void Disconnect()
		{
			if( RequestClient != null )
			{
				RequestClient.Close();
			}

			if( ListenClient != null )
			{
				ListenClient.StopReceiving();
			}
		}

		/// <summary>
		/// Sends a client connect request to the server
		/// </summary>
		public void SendConnectRequest()
		{
			// Send the 'CC' client connect request
			Byte[] BytesToSend = Encoding.ASCII.GetBytes("CC");
			RequestClient.Send(BytesToSend,BytesToSend.Length);
		}

		/// <summary>
		/// Sends a client disconnect request to the server
		/// </summary>
		public void SendDisconnectRequest()
		{
			if( RequestClient != null )
			{
				// Send the 'CD' client disconnect request
				Byte[] BytesToSend = Encoding.ASCII.GetBytes( "CD" );
				RequestClient.Send( BytesToSend, BytesToSend.Length );
			}
		}

		/// <summary>
		/// Gets the next packet waiting in the async queue
		/// </summary>
		/// <returns>The next available packet or null if none are waiting</returns>
		public Packet GetNextPacket()
		{
			if( ListenClient != null )
			{
				return ListenClient.GetNextPacket();
			}

			return null;
		}

		/// <summary>
		/// Returns the address of the server that we are connected to
		/// </summary>
		public IPAddress Address
		{
			get
			{
				return ServerAddress;
			}
		}

		/// <summary>
		/// Returns the port of the server that we are connected to
		/// </summary>
		public int Port
		{
			get
			{
				return ListenPortNo;
			}
		}
	}
}
