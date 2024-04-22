/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

namespace UnrealDVDLayout
{
    static class Program
    {
		[DllImport( "kernel32.dll" )]
		static extern bool AttachConsole( UInt32 dwProcessId );

		private const UInt32 ATTACH_PARENT_PROCESS = 0xffffffff;

		[STAThread]
        static void Main( string[] Arguments )
        {
			AttachConsole( ATTACH_PARENT_PROCESS );
			
			Application.EnableVisualStyles();
            Application.SetCompatibleTextRenderingDefault( false );
			Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;

            // Work from the branch root
#if !DEBUG
			Environment.CurrentDirectory = Path.Combine( Path.GetDirectoryName( Application.ExecutablePath ), ".." );
#else
			Environment.CurrentDirectory = Path.Combine( Environment.CurrentDirectory, ".." );
#endif

            // Create the window
            UnrealDVDLayout MainWindow = new UnrealDVDLayout();
            MainWindow.Init();

            if( Arguments.Length > 0 )
            {
                // UnrealDVDLayout Game Platform lang lang lang [-iso]
                MainWindow.HandleCommandLine( Arguments );
            }
            else
            {
                MainWindow.Show();

                while( MainWindow.Ticking )
                {
                    Application.DoEvents();

                    MainWindow.DoPopulateListBoxes();

                    // Yield a little time to the system
                    System.Threading.Thread.Sleep( 50 );
                }
            }

            MainWindow.Destroy();
        }
    }
}