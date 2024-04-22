/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Windows.Forms;

namespace UDKLift
{
	static class Program
	{
		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static int Main( string[] Arguments )
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault( false );

			string CommandLine = "";
			bool bIsEditorTokenPresent = false;
			foreach( string Argument in Arguments )
			{
				CommandLine += Argument + " ";
				// Parse the arguements, looking for 'editor'
				if( ( Argument.Equals( "editor", StringComparison.OrdinalIgnoreCase ) == true ) ||
					( Argument.Equals( "cookpackages", StringComparison.OrdinalIgnoreCase ) == true ) ||
					( Argument.Equals( "make", StringComparison.OrdinalIgnoreCase ) == true ) )
				{
					// This is the editor, so we need to launch elevated
					bIsEditorTokenPresent = true;
				}
			}

			// Run the application, elevating if required...
			// Parse the executable name to remove the 'Lift' component...
			String ExecutableName = Application.ExecutablePath;
			Int32 LastSlashIndex = ExecutableName.LastIndexOf( "\\" );
			if( LastSlashIndex != -1 )
			{
				LastSlashIndex += 1;
				ExecutableName = ExecutableName.Substring( LastSlashIndex, ExecutableName.Length - LastSlashIndex );
			}
			// Parse out the 'Lift'
			ExecutableName = ExecutableName.Replace( "Lift.", "." );

			String BinariesFolder = Application.StartupPath;
			if( BinariesFolder.LastIndexOf( "\\" ) != ( BinariesFolder.Length - 1 ) )
			{
				BinariesFolder += "\\";
			}

			try
			{
				Process LaunchProcess = new Process();

				// Find whether we're running in 64 bit mode
				string BitFolder = "Win32\\";
				if( IntPtr.Size == 8 )
				{
					BitFolder = "Win64\\";
				}

				LaunchProcess.StartInfo.FileName = BinariesFolder + BitFolder + ExecutableName;
				LaunchProcess.StartInfo.Arguments = CommandLine;
				LaunchProcess.StartInfo.WorkingDirectory = BinariesFolder + BitFolder;
				if( bIsEditorTokenPresent == true )
				{
					// Can we write a file?
					String TestFilename = BinariesFolder + "UDK_" + Guid.NewGuid().ToString() + ".tmp";

					try
					{
						StreamWriter Writer = new StreamWriter( TestFilename );
						Writer.Write( "TESTING TESTING" );
						Writer.Close();
						File.Delete( TestFilename );
					}
					catch
					{
						// Assuming that we have failed to write a file - need elevation!
						LaunchProcess.StartInfo.Verb = "runas";
					}
				}

				LaunchProcess.Start();
			}
			catch
			{
				// We need the try-catch to prevent UDKLift.exe from showing the crash dialog
				// on UAC-elevation cancellation...
			}

			return 0;
		}
	}
}
