/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Data;
using System.Data.SqlClient;
using System.IO;
using System.Net;
using System.Net.Sockets;
using System.Reflection;
using System.Reflection.Emit;
using System.Text;
using System.Xml;
using System.Xml.Linq;

namespace UnrealDatabaseProxy
{
	/// <summary>
	/// This class wraps connection to a client.
	/// </summary>
	public class ClientConnection : IDisposable
	{
		/// <summary>
		/// Delegate for command handlers.
		/// </summary>
		/// <param name="Connection">The connection that is handling the command.</param>
		/// <param name="Cmd">The command to be processed.</param>
		delegate void CommandHandlerDelegate( ClientConnection Connection, XElement Cmd );

		static Dictionary<string, CommandHandlerDelegate> mCommandHandlers = new Dictionary<string, CommandHandlerDelegate>();
		List<ResultSet> mResults = new List<ResultSet>();
		// Mirrors the Socket->SetSendBufferSize in UE3
		byte[] mRecvBuf = new byte[0x20000];
		byte[] mOutBuf = new byte[0x20000];
		BinaryWriter mWriter;
		Socket mSock;
		SocketAsyncEventArgs mRecvArgs = new SocketAsyncEventArgs();
		StringBuilder mCurrentCmd = new StringBuilder();
		volatile string mConnectionString;

		/// <summary>
		/// Constructor.
		/// </summary>
		/// <param name="Sock">The physical client connection.</param>
		public ClientConnection( Socket Sock )
		{
			mWriter = new BinaryWriter( new MemoryStream( mOutBuf ), Encoding.Unicode );
			mSock = Sock;

			mRecvArgs.DisconnectReuseSocket = false;
			mRecvArgs.SetBuffer( mRecvBuf, 0, mRecvBuf.Length );
			mRecvArgs.Completed += new EventHandler<SocketAsyncEventArgs>( OnRecv );
		}

		/// <summary>
		/// Static constructor loads all of the command handlers.
		/// </summary>
		static ClientConnection()
		{
			LoadCommandHandlers();
		}

		/// <summary>
		/// Uses reflection to load all command handlers.
		/// </summary>
		static void LoadCommandHandlers()
		{
			MethodInfo[] Methods = typeof( ClientConnection ).GetMethods( BindingFlags.Public | BindingFlags.Static | BindingFlags.NonPublic | BindingFlags.InvokeMethod );

			foreach( MethodInfo CurMethod in Methods )
			{
				CommandHandlerAttribute[] Attribs = CurMethod.GetCustomAttributes( typeof( CommandHandlerAttribute ), false ) as CommandHandlerAttribute[];

				if( Attribs != null && Attribs.Length > 0 )
				{
					CommandHandlerDelegate CmdHandler;

					if( mCommandHandlers.TryGetValue( Attribs[0].CommandName, out CmdHandler ) )
					{
						System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", string.Format( "Command \'{0}\' already contains a handler!", Attribs[0].CommandName ), System.Diagnostics.EventLogEntryType.Warning );
						System.Diagnostics.Debug.WriteLine( string.Format( "Command \'{0}\' already contains a handler!", Attribs[0].CommandName ) );
						continue;
					}

					// void OnSomeCommand(ClientConnection Connetion, XElement Cmd) { SomeCommand(Cmd); }
					DynamicMethod MethodBldr = new DynamicMethod( string.Format( "On{0}", Attribs[0].CommandName ), typeof( void ), new Type[] { typeof( ClientConnection ), typeof( XElement ) }, typeof( ClientConnection ), true );
					MethodBldr.DefineParameter( 1, ParameterAttributes.None, "Connection" );
					MethodBldr.DefineParameter( 2, ParameterAttributes.None, "Cmd" );

					ILGenerator ILGen = MethodBldr.GetILGenerator();

					ILGen.Emit( OpCodes.Ldarg_0 );
					ILGen.Emit( OpCodes.Ldarg_1 );
					ILGen.Emit( OpCodes.Tailcall );
					ILGen.Emit( OpCodes.Call, CurMethod );
					ILGen.Emit( OpCodes.Ret );

					mCommandHandlers[Attribs[0].CommandName] = ( CommandHandlerDelegate )MethodBldr.CreateDelegate( typeof( CommandHandlerDelegate ) );
				}
			}
		}

		/// Callback for when a packet has been received.
		/// </summary>
		/// <param name="sender">The object that triggered the event.</param>
		/// <param name="e">Information about the event.</param>
		void OnRecv( object sender, SocketAsyncEventArgs e )
		{
			string[] CurrentCommands = null;
			try
			{
				if( e.BytesTransferred > 0 )
				{
					// Convert a byte array into a big endian unicode string
					Decoder BufferDecoder = Encoding.BigEndianUnicode.GetDecoder();

					int CharCount = BufferDecoder.GetCharCount( mRecvBuf, 0, e.BytesTransferred );
					char[] DecodedString = new char[CharCount];
					BufferDecoder.GetChars( mRecvBuf, 0, e.BytesTransferred, DecodedString, 0 );
					string CommandString = new string( DecodedString );

					CurrentCommands = CommandString.Split( "\0".ToCharArray() );

					foreach( string CurrentCommand in CurrentCommands )
					{
						if( CurrentCommand.Length > 0 )
						{
							OnProcessCommand( CurrentCommand );
						}
					}

					BeginRecv();
				}
			}
			// When a socket has been closed pending asynchronous operations sometimes throw an ObjectDisposedException.
			catch( ObjectDisposedException )
			{
				Dispose();
			}
			catch( Exception ex )
			{
				Dispose();

				System.Diagnostics.Debug.WriteLine( ex.ToString() );
				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", ex.ToString(), System.Diagnostics.EventLogEntryType.Error );
			}
		}

		/// <summary>
		/// Begins an asynchronous recv() operation.
		/// </summary>
		public void BeginRecv()
		{
			try
			{
				mSock.ReceiveAsync( mRecvArgs );
			}
			catch( Exception ex )
			{
				System.Diagnostics.Debug.WriteLine( ex.ToString() );
				System.Diagnostics.EventLog.WriteEntry( "UnrealDatabaseProxy", ex.ToString(), System.Diagnostics.EventLogEntryType.Error );
			}
		}

		/// <summary>
		/// Called to process a command.
		/// </summary>
		/// <param name="State">Information about the state of the command.</param>
		void OnProcessCommand( string Cmd )
		{
			if( Cmd != null )
			{
				XElement Element = XElement.Parse( Cmd, LoadOptions.None );

				CommandHandlerDelegate CmdHandler;

				if( mCommandHandlers.TryGetValue( Element.Name.LocalName, out CmdHandler ) )
				{
					CmdHandler( this, Element );
				}
				else
				{
					System.Diagnostics.Debug.WriteLine( string.Format( "Unknown command: \'{0}\'", Element.Value ) );
				}
			}
		}

		/// <summary>
		/// Creates a new <see cref="ResultSEt"/>.
		/// </summary>
		/// <param name="Table">The table the new <see cref="ResultSet"/> will wrap.</param>
		/// <param name="Index">The index of the new <see cref="ResultSet"/>.</param>
		/// <param name="Results">Receives a new instance of <see cref="ResultSet"/>.</param>
		void CreateNewResultSet( DataTable Table, out int Index, out ResultSet Results )
		{
			if( Table == null )
			{
				throw new ArgumentNullException( "Table" );
			}

			bool bFound = false;
			Results = null;

			for( Index = 0; Index < mResults.Count && !bFound; ++Index )
			{
				if( mResults[Index] == null )
				{
					mResults[Index] = Results = new ResultSet( Table );
					bFound = true;
					break;
				}
			}

			if( !bFound )
			{
				Index = mResults.Count;
				Results = new ResultSet( Table );
				mResults.Add( Results );
			}
		}

		/// <summary>
		/// Releases the resources allocated by a <see cref="ResultSet"/>.
		/// </summary>
		/// <param name="Index"></param>
		void ReleaseResultSet( int Index )
		{
			mResults[Index].Dispose();
			mResults[Index] = null;
		}

		/// <summary>
		/// Retrieves the <see cref="ResultSet"/> with the specified index.
		/// </summary>
		/// <param name="Index">The index of the requested <see cref="ResultSet"/>.</param>
		/// <returns></returns>
		ResultSet GetResultSet( int Index )
		{
			return mResults[Index];
		}

		/// <summary>
		/// Command handler for the &lt;command/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "command" )]
		static void SendCommand( ClientConnection Connection, XElement Cmd )
		{
			XAttribute results = Cmd.Attribute( "results" );

			using( SqlConnection Con = new SqlConnection( Connection.mConnectionString ?? Properties.Settings.Default.ConnectionString ) )
			{
				using( SqlCommand SqlCmd = Con.CreateCommand() )
				{
					SqlCmd.CommandText = Cmd.Value;
					SqlCmd.CommandTimeout = 600;
					//Console.WriteLine(Cmd.Value);

					if( results == null || results.Value != "true" )
					{
						Con.Open();
						SqlCmd.ExecuteNonQuery();
						Con.Close();
					}
					else
					{
						using( SqlDataAdapter Adapter = new SqlDataAdapter( SqlCmd ) )
						{
							DataTable Table = new DataTable();
							ResultSet Results;
							int Index;

							Adapter.Fill( Table );

							Connection.CreateNewResultSet( Table, out Index, out Results );

							Connection.mWriter.Seek( 0, SeekOrigin.Begin );
							Connection.mWriter.Write( IPAddress.HostToNetworkOrder( Index ) );
							Connection.mSock.Send( Connection.mOutBuf, sizeof( int ), SocketFlags.None );
						}
					}
				}
			}
		}

		/// <summary>
		/// Command handler for the &lt;getint/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "getint" )]
		static void GetInt( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			ResultSet Results = Connection.GetResultSet( Index );

			Index = Results.GetInt( Cmd.Value );

			Connection.mWriter.Seek( 0, SeekOrigin.Begin );
			Connection.mWriter.Write( IPAddress.HostToNetworkOrder( Index ) );
			Connection.mSock.Send( Connection.mOutBuf, sizeof( int ), SocketFlags.None );
		}

		/// <summary>
		/// Command handler for the &lt;getfloat/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "getfloat" )]
		unsafe static void GetFloat( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			ResultSet Results = Connection.GetResultSet( Index );

			float Number = Results.GetFloat( Cmd.Value );

			Index = *( ( int* )&Number );

			Connection.mWriter.Seek( 0, SeekOrigin.Begin );
			Connection.mWriter.Write( IPAddress.HostToNetworkOrder( Index ) );
			Connection.mSock.Send( Connection.mOutBuf, sizeof( int ), SocketFlags.None );
		}

		/// <summary>
		/// Command handler for the &lt;getstring/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "getstring" )]
		static void GetString( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			ResultSet Results = Connection.GetResultSet( Index );

			string Str = Results.GetString( Cmd.Value );

			Connection.mWriter.Seek( 0, SeekOrigin.Begin );

			Connection.mWriter.Write( IPAddress.HostToNetworkOrder( Str.Length ) );

			for( int i = 0; i < Str.Length; ++i )
			{
				Connection.mWriter.Write( IPAddress.HostToNetworkOrder( ( short )Str[i] ) );
			}

			Connection.mSock.Send( Connection.mOutBuf, sizeof( int ) + Str.Length * sizeof( char ), SocketFlags.None );
		}

		/// <summary>
		/// Command handler for the &lt;movetofirst/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "movetofirst" )]
		static void MoveToFirst( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			ResultSet Results = Connection.GetResultSet( Index );

			Results.MoveToFirst();
		}

		/// <summary>
		/// Command handler for the &lt;isatend/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "isatend" )]
		static void IsAtEnd( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			ResultSet Results = Connection.GetResultSet( Index );

			Connection.mWriter.Seek( 0, SeekOrigin.Begin );
			Connection.mWriter.Write( IPAddress.HostToNetworkOrder( Results.IsAtEnd ? 1 : 0 ) );
			Connection.mSock.Send( Connection.mOutBuf, sizeof( int ), SocketFlags.None );
		}

		/// <summary>
		/// Command handler for the &lt;movetonext/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "movetonext" )]
		static void MoveToNext( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			ResultSet Results = Connection.GetResultSet( Index );

			Results.MoveToNext();
		}

		/// <summary>
		/// Command handler for the &lt;closeresultset/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "closeresultset" )]
		static void CloseResultSet( ClientConnection Connection, XElement Cmd )
		{
			XAttribute resultset = Cmd.Attribute( "resultset" );
			int Index = int.Parse( resultset.Value );
			Connection.ReleaseResultSet( Index );
		}

		/// <summary>
		/// Command handler for the &lt;connectionString/&gt; command.
		/// </summary>
		/// <param name="Connection">The connection that is executing the command.</param>
		/// <param name="Cmd">The command to be handled.</param>
		[CommandHandler( "connectionString" )]
		static void SetConnectionString( ClientConnection Connection, XElement Cmd )
		{
			Connection.mConnectionString = Cmd.Value;
		}
		/// Cleans up resources.
		/// </summary>
		public void Dispose()
		{
			mSock.Close();
			mWriter.Close();
		}
	}
}
