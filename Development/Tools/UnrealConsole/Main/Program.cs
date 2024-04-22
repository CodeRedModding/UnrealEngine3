/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Reflection;
using System.Windows.Forms;

namespace UnrealConsole
{
	static class Program
	{
        public static int ExitCode = 0;

		static private void ParseCommandline( string[] Args, ref string Platform, ref string TargetName )
		{
			// go over argument list
			foreach( string Arg in Args )
			{
				if( Arg.ToLower().StartsWith( "-platform=" ) )
				{
					Platform = Arg.Substring( "-platform=".Length );
				}
				else if( Arg.ToLower().StartsWith( "-target=" ) )
				{
					TargetName = Arg.Substring( "-target=".Length );
				}
			}
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static private int Main( string[] Args )
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault( false );
			Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;

#if !DEBUG
			try
#endif
			{
                if ((Args.Length >= 2) && (Args[0] == "-wrapexe"))
                {
                    // Wrap execution of another exe and show progress + an optional log
                    ThunkingProgressDialog MainWindow = new ThunkingProgressDialog();
                    MainWindow.StartApp(Args);
                    Application.Run(MainWindow);
                }
                else
                {
                    // Grab the commandline 
                    string Platform = null;
                    string TargetName = null;
                    ParseCommandline(Args, ref Platform, ref TargetName);

                    // Create the window
                    UnrealConsoleWindow MainWindow = new UnrealConsoleWindow();
                    MainWindow.Ticking = MainWindow.Init(Platform, TargetName);

                    while (MainWindow.Ticking)
                    {
                        Application.DoEvents();

						MainWindow.Run();

                        // Yield a little time to the system
                        System.Threading.Thread.Sleep(10);
                    }

                    MainWindow.Destroy();
                }
			}
#if !DEBUG
			catch( Exception Ex )
			{
                ExitCode = 1;
				Debug.WriteLine( Ex.ToString() );
			}
#endif

            return ExitCode;
		}
	}
}