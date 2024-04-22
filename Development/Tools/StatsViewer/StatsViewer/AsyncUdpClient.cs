/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Net;
using System.Net.Sockets;
using System.Threading;

namespace StatsViewer
{
	/// <summary>
	/// This class performs asynchronous reads from a UDP port
	/// </summary>
	public class AsyncUdpClient : Socket
	{
		/// <summary>
		/// Holds the array of received data packets
		/// </summary>
		private ArrayList PacketArray = new ArrayList();
		/// <summary>
		/// This is the buffer used for asynch reads
		/// </summary>
		private Byte[] ReadBuffer = new Byte[128 * 1024];
		/// <summary>
		/// The end point to use for listening for requests. Can be IPAddress.Any
		/// </summary>
		private IPEndPoint DestEndPoint;
		/// <summary>
		/// Used to communicate with the async thread that the socket was closed or not
		/// </summary>
		private bool bIsTimeToClose;

		/// <summary>
		/// Initializes the socket and creates the read buffer
		/// </summary>
		/// <param name="PortNo">The port associated with this socket</param>
		public AsyncUdpClient(int InPortNo) :
			base(AddressFamily.InterNetwork,SocketType.Dgram,ProtocolType.Udp)
		{
			DestEndPoint = new IPEndPoint(IPAddress.Any,InPortNo);
		}

		/// <summary>
		/// Constructor that takes an address and a port (specific server in mind)
		/// </summary>
		/// <param name="InAddress">The address that will be sending data</param>
		/// <param name="PortNo">The port the data will come in on</param>
		public AsyncUdpClient(IPAddress InAddress,int PortNo) :
			base(AddressFamily.InterNetwork,SocketType.Dgram,ProtocolType.Udp)
		{
			DestEndPoint = new IPEndPoint(InAddress,PortNo);
		}

		/// <summary>
		/// Queues a packet into our thread safe array
		/// </summary>
		/// <param name="Packet">The packet to add to the queue</param>
		private void QueuePacket(Packet packet)
		{
			// Lock the array for thread safety
			Monitor.Enter(PacketArray);
			// Queue our packet
			PacketArray.Add(packet);
			// Release the lock
			Monitor.Exit(PacketArray);
		}

		/// <summary>
		/// Processes the callback from BeginReceiveData(). Takes the data
		/// that is read and adds it to the queue
		/// </summary>
		/// <param name="AsyncResult">The callback data object</param>
		private void OnReceiveData(IAsyncResult AsyncResult)
		{
			Monitor.Enter(this);
			try
			{
				// Don't read if the socket has been shutdown
				if (bIsTimeToClose == false)
				{
					EndPoint RefSender = (EndPoint)DestEndPoint;
					// Figure out how much was read
					int Read = EndReceiveFrom(AsyncResult,ref RefSender);
					if (Read > 0)
					{
						// Create a new byte array just containing the read data
						Byte[] BytesRead = new Byte[Read];
						Array.Copy(ReadBuffer,0,BytesRead,0,Read);
						// Get the sender of the packet
						IPEndPoint Sender = (IPEndPoint)RefSender;
						// Add the packet to the queue
						QueuePacket(new Packet(Sender,BytesRead));
					}
					// Listen for the server on the specified port/address
					RefSender = (EndPoint)DestEndPoint;
					// Kick off a new read request
					BeginReceiveFrom(ReadBuffer,0,ReadBuffer.Length,SocketFlags.None,ref RefSender,
						new AsyncCallback(OnReceiveData),this);
				}
			}
			catch (ObjectDisposedException)
			{
				// The socket was shutdown externally
			}
			finally
			{
				Monitor.Exit(this);
			}
		}

		/// <summary>
		/// Starts the asynchronous reading of packets
		/// </summary>
		public void StartReceiving()
		{
			// Listen for the server on the specified port/address
			EndPoint RefSender = (EndPoint)DestEndPoint;
			try
			{
				// Bind to the specified port
				Bind(DestEndPoint);
				// Set the size of the socket's read buffer
				SetSocketOption(SocketOptionLevel.Socket,SocketOptionName.ReceiveBuffer,0x100000);
				// Kick off a new read request
				BeginReceiveFrom(ReadBuffer,0,ReadBuffer.Length,SocketFlags.None,
					ref RefSender,new AsyncCallback(OnReceiveData),this);
			}
			catch (Exception e)
			{
				Console.WriteLine("Exception during StartReceiving(): " + e.ToString());
			}
		}

		/// <summary>
		/// Shuts down the socket
		/// </summary>
		public void StopReceiving()
		{
			// Tell the other thread we are closing
			Monitor.Enter(this);
			bIsTimeToClose = true;
			// Kill the socket
			Shutdown(SocketShutdown.Both);
			Close();
			Monitor.Exit(this);
		}

		/// <summary>
		/// Returns the next availalbe packet from the asynch packet queue (FIFO)
		/// </summary>
		/// <returns>The next available packet or null</returns>
		public Packet GetNextPacket()
		{
			// Lock the array for thread safety
			Monitor.Enter(PacketArray);
			Packet NextPacket = null;
			if (PacketArray.Count > 0)
			{
				// Pull the next packet from the array
				NextPacket = (Packet)PacketArray[0];
				PacketArray.RemoveAt(0);
			}
			// Release our thread lock
			Monitor.Exit(PacketArray);
			return NextPacket;
		}
	}
}
