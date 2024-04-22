/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Diagnostics;
using System.IO;
using System.Threading;

namespace DualMode2
{
	class Program
	{
		static private Process ProcessToLaunch = null;
		static private bool bIsServer = false;

		static private bool GetExecutable( ref string Executable )
		{
			string Location = Path.GetDirectoryName( Executable ).ToLower();
			string BaseName = Path.GetFileNameWithoutExtension( Executable );

			if( Location.EndsWith( "win32" ) || Location.EndsWith( "win64" ) )
			{
				// Must me a com file parallel to the exe
				Executable = Path.ChangeExtension( Executable, ".exe" );

				// Make sure we can launch the x64 version if requested
				if( Location.EndsWith( "win64" ) && IntPtr.Size < 8 )
				{
					Console.WriteLine( "Unable to launch x64 binaries on non x64 system!" );
					return ( false );
				}
			}
			else
			{
				// Must be in the binaries folder
				bool Needx86Version = true;
				if( IntPtr.Size == 8 && BaseName.ToLower() != "udk" )
				{
					// Construct 64 bit path
					Executable = Path.Combine( Location, "Win64\\" );
					Executable = Path.Combine( Executable, BaseName );
					Executable = Path.ChangeExtension( Executable, ".exe" );

					FileInfo TestInfo = new FileInfo( Executable );
					Needx86Version = !TestInfo.Exists;
				}

				// Fall back to x86 version if the x64 version can't be run or can't be found
				if( Needx86Version )
				{
					// Construct 32 bit path
					Executable = Path.Combine( Location, "Win32\\" );
					Executable = Path.Combine( Executable, BaseName );
					Executable = Path.ChangeExtension( Executable, ".exe" );
				}
			}

			// Make sure the requested file exists
			FileInfo Binary = new FileInfo( Executable );
			if( !Binary.Exists )
			{
				Console.WriteLine( "Unable to to find binary to run!" );
				return ( false );
			}

			Executable = Binary.FullName;
			return ( true );
		}

		protected static void ControlCHandler( object sender, ConsoleCancelEventArgs args )
		{
			if( ProcessToLaunch != null )
			{
				if( bIsServer )
				{
					// Signal the server commandlet to shutdown
					EventWaitHandle Event = new EventWaitHandle( false, EventResetMode.ManualReset, "ComWrapperShutdown" );
					Event.Set();
				}
				else
				{
					// Don't care - just kill the process
					ProcessToLaunch.Kill();
				}
			}
		}

		static public int Main( string[] args )
		{
#if DEBUG
			string Executable = Path.GetFullPath( "GearGame.com" );
#else
			string[] Executables = Environment.GetCommandLineArgs();
			string Executable = Path.GetFullPath( Executables[0] );
#endif
			if( !GetExecutable( ref Executable ) )
			{
				return( -1 );
			}

            // Reconstruct cmdLine into one big string. If any string in args[] has a whitespace, wrap it in quotes
            string CommandLine = "";

            for (int i = 0; i < args.Length; i++)
            {
                if (args[i].Contains(" "))
                {
                    CommandLine += "\"" + args[i] + "\"";
                }
                else
                {
                    CommandLine += args[i];
                }
                if ( i < args.Length-1 )
                {
                    CommandLine += " ";
                }
            }

            // Check to see if we are running the server commandlet
			bIsServer = CommandLine.ToLower().Contains( "server" );

			// Override ctrl-c handler
			Console.CancelKeyPress += new ConsoleCancelEventHandler( ControlCHandler );

			// Spawn the process
			// Console.WriteLine( "Launching: " + Executable );

			ProcessToLaunch = new Process();
			ProcessToLaunch.StartInfo.FileName = Executable;
			ProcessToLaunch.StartInfo.Arguments = CommandLine;
			if( !ProcessToLaunch.Start() )
			{
				Console.WriteLine( "Failed to start process" );
				return( -1 );
			}

			// Create the events to wait for
			string EventName = "dualmode_die_event" + ProcessToLaunch.Id.ToString();
			EventWaitHandle Event = new EventWaitHandle( false, EventResetMode.ManualReset, EventName );

			// Connect up the pipe
			NamedPipe RelayPipe = new NamedPipe( ProcessToLaunch.Id );

			// Poll for output while the process is running and it hasn't told us to go away
			while( !ProcessToLaunch.HasExited && !Event.WaitOne( 1 ) )
			{
				RelayPipe.PollOutput();
			}

			// Cleanup
			RelayPipe.Destroy();
			if( ProcessToLaunch.HasExited )
			{
				return ( ProcessToLaunch.ExitCode );
			}

			return ( 0 );
		}
	}
}
