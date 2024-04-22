/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.Net;

namespace UnrealDatabaseProxy
{
	/// <summary>
	/// This class represents the server and handles incoming connections.
	/// </summary>
	public class Server
	{
		const int PORT = 10500;

		Socket mListenSocket;
		SocketAsyncEventArgs mAcceptArgs = new SocketAsyncEventArgs();

		/// <summary>
		/// Constructor.
		/// </summary>
		public Server()
		{
			mAcceptArgs.DisconnectReuseSocket = false;
			mAcceptArgs.Completed += new EventHandler<SocketAsyncEventArgs>( OnAccept );
		}

		/// <summary>
		/// Callback for accepting incoming connections.
		/// </summary>
		/// <param name="sender">The object that initiated the event.</param>
		/// <param name="e">Information about the event.</param>
		void OnAccept( object sender, SocketAsyncEventArgs e )
		{
			ClientConnection ProxyClient = null;
			try
			{
				if( mListenSocket != null )
				{
					if( e.AcceptSocket == null )
					{
						throw new ArgumentNullException( "AcceptSocket" );
					}

					Socket AcceptSock = e.AcceptSocket;
					e.AcceptSocket = null;

					ProxyClient = new ClientConnection( AcceptSock );

					mListenSocket.AcceptAsync( e );

					ProxyClient.BeginRecv();

					System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", "New connection accepted from " + AcceptSock.LocalEndPoint.ToString(), System.Diagnostics.EventLogEntryType.Information );
				}
			}
			catch( Exception ex )
			{
				if( ProxyClient != null )
				{
					ProxyClient.Dispose();
				}

				System.Diagnostics.Debug.WriteLine( ex.ToString() );
				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", ex.ToString(), System.Diagnostics.EventLogEntryType.Error );
			}
		}

		/// <summary>
		/// Starts the server and begins listening for incoming connections.
		/// </summary>
		public void Start()
		{
			try
			{
				if( mListenSocket != null && mListenSocket.Connected )
				{
					mListenSocket.Close();
					mListenSocket = null;
				}

				mListenSocket = new Socket( AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp );

				mListenSocket.Bind( new IPEndPoint( IPAddress.Any, PORT ) );
				mListenSocket.Listen( 100 );
				mListenSocket.AcceptAsync( mAcceptArgs );

				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", "Started", System.Diagnostics.EventLogEntryType.Information );
			}
			catch( Exception ex )
			{
				System.Diagnostics.Debug.WriteLine( ex.ToString() );
				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", ex.ToString(), System.Diagnostics.EventLogEntryType.Error );
			}
		}

		/// <summary>
		/// Stops the server from accepting incoming connections.
		/// </summary>
		public void Stop()
		{
			try
			{
				mListenSocket.Close();
				mListenSocket = null;

				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", "Stopped", System.Diagnostics.EventLogEntryType.Information );
			}
			catch( Exception ex )
			{
				System.Diagnostics.Debug.WriteLine( ex.ToString() );
				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", ex.ToString(), System.Diagnostics.EventLogEntryType.Error );
			}
		}
	}
}
