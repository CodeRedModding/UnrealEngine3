/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Net;
using System.Net.Sockets;

namespace StatsViewer
{
	/// <summary>
	/// Holds the data for a single packet (sender/destination and data)
	/// </summary>
	public class Packet
	{
		/// <summary>
		/// The IP address of the sender of this packet
		/// </summary>
		public IPEndPoint Address;
		/// <summary>
		/// The data sent as the packet
		/// </summary>
		public Byte[] Data;

		/// <summary>
		/// Sets the fields for this packet
		/// </summary>
		/// <param name="InAddress">The address of where the data is going/came from</param>
		/// <param name="InData">The payload of the packet</param>
		public Packet(IPEndPoint InAddress,Byte[] InData)
		{
			Address = InAddress;
			Data = InData;
		}

		/// <summary>
		/// Constructs a packet without an IP address associated with it
		/// </summary>
		/// <param name="InData">The data in the payload of this packet</param>
		public Packet(Byte[] InData)
		{
			Data = InData;
		}
	};
}
