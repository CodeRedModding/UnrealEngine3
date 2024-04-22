// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Deployment.Application;
using System.ServiceProcess;
using System.Text;
using System.Windows.Forms;

namespace CISMonitor
{
    public class Program
    {
		private static DateTime LastUpdateCheck = DateTime.MinValue;

		static bool CheckForUpdates()
		{
			try
			{
				if( DateTime.UtcNow - LastUpdateCheck > new TimeSpan( 1, 0, 0 ) )
				{
					LastUpdateCheck = DateTime.UtcNow;

					if( ApplicationDeployment.IsNetworkDeployed )
					{
						ApplicationDeployment Current = ApplicationDeployment.CurrentDeployment;

						// If there are any updates available, install them now
						if( Current.CheckForUpdate() )
						{
							return ( Current.Update() );
						}
					}
				}
			}
			catch( Exception )
			{
			}

			return( false );
		}

        /// <summary>
        /// The main entry point for the application.
        /// </summary>
        static void Main( string[] Args )
        {
#if !DEBUG
            try
#endif
            {
				Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;

				// Check for there already being an updated version
				if( CheckForUpdates() )
				{
					Application.Restart();
					return;
				}

				// Create the window
                CISMonitor MainWindow = new CISMonitor();
                MainWindow.Init( Args );

                while( MainWindow.Ticking )
                {
                    Application.DoEvents();
                    MainWindow.Run();
					if( CheckForUpdates() )
					{
						MainWindow.Ticking = false;
						MainWindow.Restart = true;
					}

                    System.Threading.Thread.Sleep( 50 );
                }

                MainWindow.Destroy();

				if( MainWindow.Restart )
				{
					Application.Restart();
				}
            }
#if !DEBUG
            catch
            {
                System.Windows.Forms.MessageBox.Show( "Unknown failure during initialisation", "CIS Monitor Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
            }
#endif
		}
    }
}