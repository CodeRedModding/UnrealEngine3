/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Windows.Forms;

namespace UnrealLoc
{
    static class Program
    {
        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        [STAThread]
        static void Main()
        {
            Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault( false );
			Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;

            // Work from the branch root
            string Path = Application.StartupPath.Replace( "\\Binaries", "" );
            Environment.CurrentDirectory = Path;

            // Create the window
            UnrealLoc MainWindow = new UnrealLoc();
            MainWindow.Init();

            while( MainWindow.Ticking )
            {
                Application.DoEvents();

                // Yield a little time to the system
                System.Threading.Thread.Sleep( 50 );
            }

            MainWindow.Destroy();
        }
    }
}