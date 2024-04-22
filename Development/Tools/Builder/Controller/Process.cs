// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections;
using System.Collections.Specialized;
using System.Drawing;
using System.Diagnostics;
using System.IO;

namespace Controller
{
    public class BuildProcess
    {
        public bool IsFinished = false;
		public int ExitCode;

        private Main Parent;
        private BuildState Builder;
        private Process RunningProcess;
		private COMMANDS ErrorLevel = COMMANDS.None;
        private bool CaptureOutputDebugString = false;
        private string Executable;
        private string CommandLine;

		public COMMANDS GetErrorLevel()
        {
            return ErrorLevel;
        }

        public BuildProcess( Main InParent, BuildState InBuilder, string InExecutable, string InCommandLine, string WorkingDirectory, bool InCaptureOutputDebugString )
        {
            Parent = InParent;
            Builder = InBuilder;
            RunningProcess = new Process();
            Executable = InExecutable;
            CommandLine = InCommandLine;
            CaptureOutputDebugString = InCaptureOutputDebugString;
            ExitCode = 0;

            // Prepare a ProcessStart structure 
            RunningProcess.StartInfo.FileName = Executable;
            RunningProcess.StartInfo.Arguments = CommandLine;
            if( WorkingDirectory.Length > 0 )
            {
                RunningProcess.StartInfo.WorkingDirectory = WorkingDirectory;
            }
            else
            {
                RunningProcess.StartInfo.WorkingDirectory = Environment.CurrentDirectory;
            }

			// Redirect the output.
            RunningProcess.StartInfo.UseShellExecute = false;
            RunningProcess.StartInfo.RedirectStandardOutput = !CaptureOutputDebugString;
            RunningProcess.StartInfo.RedirectStandardError = !CaptureOutputDebugString;
            RunningProcess.StartInfo.CreateNoWindow = true;

            Parent.Log( "Spawning: " + Executable + " " + CommandLine + " (CWD: " + WorkingDirectory + ")", Color.DarkGreen );
            Builder.Write( "Spawning: " + Executable + " " + CommandLine + " (CWD: " + WorkingDirectory + ")" );

            // Spawn the process - try to start the process, handling thrown exceptions as a failure.
            try
            {
                if( !CaptureOutputDebugString )
                {
                    RunningProcess.OutputDataReceived += new DataReceivedEventHandler( PrintLog );
                    RunningProcess.ErrorDataReceived += new DataReceivedEventHandler( PrintLog );
                }
                else
                {
#if !DEBUG
                    DebugMonitor.OnOutputDebugString += new OnOutputDebugStringHandler( CaptureDebugString );
                    DebugMonitor.Start();
#endif
                }
                RunningProcess.Exited += new EventHandler( ProcessExit );

                RunningProcess.Start();

                if( !CaptureOutputDebugString )
                {
                    RunningProcess.BeginOutputReadLine();
                    RunningProcess.BeginErrorReadLine();
                }
                RunningProcess.EnableRaisingEvents = true;
            }
            catch( Exception Ex )
            {
                RunningProcess = null;
                IsFinished = true;
                Parent.Log( "PROCESS ERROR: Failed to start: " + Executable + " with exception: " + Ex.ToString(), Color.Red );

				Builder.Write( "PROCESS ERROR: Failed to start: " + Executable + " with exception: " + Ex.ToString() );
                Builder.CloseLog();
#if !DEBUG
                if( CaptureOutputDebugString )
                {
                    CaptureOutputDebugString = false;
                    DebugMonitor.Stop();
                }
#endif

				ErrorLevel = COMMANDS.Process;
            }
        }

        public BuildProcess( Main InParent, BuildState InBuilder, string InCommandLine, string WorkingDirectory, 
			bool bInUseShellExecute = true, bool bInShowNoWindow = true)
        {
            Parent = InParent;
            Builder = InBuilder;
            RunningProcess = new Process();
            Executable = "Cmd.exe";
            CommandLine = InCommandLine;
            ExitCode = 0;

            // Prepare a ProcessStart structure 
            RunningProcess.StartInfo.FileName = Executable;
            RunningProcess.StartInfo.Arguments = CommandLine;
			if( WorkingDirectory.Length > 0 )
            {
                RunningProcess.StartInfo.WorkingDirectory = WorkingDirectory;
            }
            else
            {
                RunningProcess.StartInfo.WorkingDirectory = Environment.CurrentDirectory;
            }

			// Redirect the output.
			RunningProcess.StartInfo.UseShellExecute = bInUseShellExecute;
			RunningProcess.StartInfo.CreateNoWindow = bInShowNoWindow;

            Parent.Log( "Spawning: " + Executable + " " + CommandLine, Color.DarkGreen );

            // Spawn the process - try to start the process, handling thrown exceptions as a failure.
            try
            {
                RunningProcess.Exited += new EventHandler( ProcessExit );

                RunningProcess.Start();

                RunningProcess.EnableRaisingEvents = true;
            }
            catch
            {
                RunningProcess = null;
                IsFinished = true;
                Parent.Log( "PROCESS ERROR: Failed to start: " + Executable, Color.Red );

                Builder.Write( "PROCESS ERROR: Failed to start: " + Executable );
                Builder.CloseLog();
				ErrorLevel = COMMANDS.Process;
            }
        }

		public void WaitForExit()
		{
			while( !RunningProcess.HasExited )
			{
				RunningProcess.WaitForExit( 50 );
			}
		}

        public void Cleanup()
        {
            Builder.CloseLog();

#if !DEBUG
            if( CaptureOutputDebugString )
            {
                CaptureOutputDebugString = false;
                DebugMonitor.Stop();
            }
#endif
        }

        public void Kill()
        {
            Parent.Log( "Killing active processes ...", Color.Red );
            string Name = RunningProcess.ProcessName.ToLower();

            // Kill the currently spawned process
            if( RunningProcess != null )
            {
				try
				{
					Parent.Log( " ... killing: '" + Executable + "'", Color.Red );

					RunningProcess.Kill();
					// Wait for 30 seconds for the process to exit
					RunningProcess.WaitForExit( 30 * 1000 );
				}
				catch
				{
				}
            }

            // Kill any compilation processes
            Parent.KillProcess( "UnrealBuildTool" );
            Parent.KillProcess( "cl" );
            Parent.KillProcess( "cl_Incredibuild_Interop_0" );

            // If we're running the com version, kill the exe too
            if( Name.IndexOf( ".com" ) >= 0 )
            {
                Name = Name.Replace( ".com", "" );
                Parent.KillProcess( Name );
            }

            Cleanup();
            IsFinished = true;
        }

        public bool IsResponding()
        {
            try
            {
                if( RunningProcess != null )
                {
                    return ( RunningProcess.Responding );
                }
            }
            catch
            {
            }

            return ( false );
        }

        public void ProcessExit( object Sender, EventArgs e )
        {
            ExitCode = RunningProcess.ExitCode;
			if( !CaptureOutputDebugString )
			{
				// Flush any pending delegates
				// RunningProcess.CancelErrorRead();
				// RunningProcess.CancelOutputRead();
			}
			RunningProcess.EnableRaisingEvents = false;
            IsFinished = true;
        }

        public void CaptureDebugString( int PID, string Text )
        {
            if( Text != null )
            {
                Builder.Write( Text.Trim() );
            }
        }

        public void PrintLog( object Sender, DataReceivedEventArgs e )
        {
            string Line = e.Data;
            Builder.Write( Line );
            Parent.CheckStatusUpdate( Line );
        }
    }
}

