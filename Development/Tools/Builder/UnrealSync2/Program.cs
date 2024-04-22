// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

using System;
using System.Collections.Generic;
using System.Deployment.Application;
using System.Windows.Forms;

namespace Builder.UnrealSync
{
	static class Program
	{
		private static DateTime LastUpdateCheck = DateTime.MinValue;

		static bool CheckForUpdates()
		{
			try
			{
				if( LastUpdateCheck < DateTime.UtcNow )
				{
					if( ApplicationDeployment.IsNetworkDeployed )
					{
						ApplicationDeployment Current = ApplicationDeployment.CurrentDeployment;

						// If there are any updates available, install them now
						if( Current.CheckForUpdate() )
						{
							return Current.Update();
						}
					}

					LastUpdateCheck = DateTime.UtcNow.AddHours( 1 );
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
		[STAThread]
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
                UnrealSync2 MainWindow = new UnrealSync2();
                MainWindow.Init( Args );

                while( MainWindow.bTicking )
                {
                    Application.DoEvents();
                    MainWindow.Run();
					if( CheckForUpdates() )
					{
						MainWindow.bTicking = false;
						MainWindow.bRestart = true;
					}

                    System.Threading.Thread.Sleep( 50 );
                }

                MainWindow.Destroy();

				// Restart if requested
				if( MainWindow.bRestart )
				{
					Application.Restart();
				}
            }
#if !DEBUG
            catch( Exception Ex )
            {
                System.Windows.Forms.MessageBox.Show( "Unknown failure during initialisation\n" + Ex.Message, "UnrealSync2 Error", MessageBoxButtons.OK, MessageBoxIcon.Error );
            }
#endif
		}
	}
}
