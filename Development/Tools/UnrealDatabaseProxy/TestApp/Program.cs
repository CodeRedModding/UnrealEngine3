/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Net.Sockets;
using System.Net;

namespace TestApp
{
	class Program
	{
		static void Main( string[] args )
		{
			Console.WriteLine( "Creating socket..." );
			Socket sock = new Socket( AddressFamily.InterNetwork, SocketType.Stream, ProtocolType.Tcp );

			Console.WriteLine( "Connecting to 127.0.0.1:10500..." );
			sock.Connect( new IPEndPoint( IPAddress.Parse( "127.0.0.1" ), 10500 ) );

			Console.WriteLine( "Sending data..." );

			Console.WriteLine( "{0} bytes sent!", sock.Send( Encoding.BigEndianUnicode.GetBytes( "<command results=\"true\">EXEC BeginRun @PlatformName='Xbox360', @MachineName='NewConsole', @UserName='User', @Changelist='576897', @GameName='UDK', @ResolutionName='1280x720', @ConfigName='DEBUG', @CmdLine='dm-deck?Name=Player?team=255?gAPT=1?AutoTests=1?quickstart=1?bTourist=1?gDASR=1?gSTD=BotMatch', @GameType='AutoTestManager_0', @LevelName='DM-Deck', @TaskDescription='BotMatch', @TaskParameter='', @Tag=''</command>" ) ) );

			Console.WriteLine( "Closing socket..." );
			sock.Close();
		}
	}
}
