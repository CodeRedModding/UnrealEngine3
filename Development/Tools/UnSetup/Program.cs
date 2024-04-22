/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections;
using System.Collections.Generic;
using System.Deployment.Application;
using System.Diagnostics;
using System.Drawing;
using System.IO;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Ionic.Zip;

namespace UnSetup
{
	static class Program
	{
		[DllImport( "kernel32.dll" )]
		static extern bool AttachConsole( UInt32 dwProcessId );

		private const UInt32 ATTACH_PARENT_PROCESS = 0xffffffff;

		static public Utils Util = null;

		static int ProcessArguments( string[] Arguments )
		{
			foreach( string Argument in Arguments )
			{
				if( Argument.ToLower().StartsWith( "-" ) )
				{
					return ( 1 );
				}
				if( Argument.ToLower().StartsWith( "/" ) )
				{
					return ( 2 );
				}
			}

			return ( 2 );
		}

		static string GetWorkFolder()
		{
			string TempFolder = Path.Combine( Path.GetTempPath(), "Epic-" + Util.InstallInfoData.InstallGuidString );
			Directory.CreateDirectory( TempFolder );

			return ( TempFolder );
		}

		static void CopyFile( string WorkFolder, string FileName )
		{
			FileInfo Source = new FileInfo( FileName );
			string DestFileName = Path.Combine( WorkFolder, Source.Name );

			// Ensure the destination folder exists
			Directory.CreateDirectory( Path.GetDirectoryName( DestFileName ) );

			// Delete the destination file if it already exists
			FileInfo Dest = new FileInfo( DestFileName );
			if( Dest.Exists )
			{
				Dest.IsReadOnly = false;
				Dest.Delete();
			}

			if( Source.Exists )
			{
				Source.CopyTo( DestFileName );
			}
		}

		static string ExtractWorkFiles()
		{
			// Set the CWD to a work folder
			string WorkFolder = GetWorkFolder();
			Environment.CurrentDirectory = WorkFolder;

			// Extract any files required by the setup
			Util.ExtractSingleFile( "Binaries\\UnSetup.exe.config" );
			Util.ExtractSingleFile( "Binaries\\UnSetup.Game.xml" );
			Util.ExtractSingleFile( "Binaries\\UnSetup.Manifests.xml" );

			// Extract the EULA for the local language (may not exist)
			string LocEULA = Util.GetLocFileName( "Binaries\\InstallData\\EULA", "rtf" );
			Util.ExtractSingleFile( LocEULA );

			// Always extract the INT EULA (the fallback in case the local language version does not exist)
			Util.ExtractSingleFile( "Binaries\\InstallData\\EULA.INT.rtf" );

			Util.ExtractSingleFile( "Binaries\\InstallData\\Interop.IWshRuntimeLibrary.dll" );

			Util.ExtractSingleFile( "Binaries\\Redist\\UE3Redist.exe" );

			return ( WorkFolder );
		}

		static string ExtractRedistWorkFiles()
		{
			// Set the CWD to a work folder
			string WorkFolder = GetWorkFolder();
			Environment.CurrentDirectory = WorkFolder;

			// Extract any files required by the setup
			Util.ExtractSingleFile( "Binaries\\UnSetup.exe.config" );

			// Extract any files required by the setup
			Util.ExtractSingleFile( "Binaries\\InstallData\\EULA.rtf" );

			return ( WorkFolder );
		}

		static void DeleteWorkFile( string WorkFolder, string FileName )
		{
			string PathName = Path.Combine( WorkFolder, FileName );
			FileInfo Info = new FileInfo( PathName );
			if( Info.Exists )
			{
				Info.IsReadOnly = false;
				Info.Delete();
			}
		}

		static string CopyUnSetup()
		{
			string WorkFolder = GetWorkFolder();
#if DEBUG
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Path.Combine( Environment.CurrentDirectory, "UnSetup.exe" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Path.Combine( Environment.CurrentDirectory, "UnSetup.exe.config" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Path.Combine( Environment.CurrentDirectory, "UnSetup.Game.xml" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Path.Combine( Environment.CurrentDirectory, "UnSetup.Manifests.xml" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\InstallData\\" ), Path.Combine( Environment.CurrentDirectory, "InstallData\\InstallInfo.xml" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\InstallData\\" ), Path.Combine( Environment.CurrentDirectory, "InstallData\\Interop.IWshRuntimeLibrary.dll" ) );
#else
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Application.ExecutablePath );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Application.ExecutablePath + ".config" );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Path.Combine( Application.StartupPath, "UnSetup.Game.xml" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\" ), Path.Combine( Application.StartupPath, "UnSetup.Manifests.xml" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\InstallData\\" ), Path.Combine( Application.StartupPath, "InstallData\\InstallInfo.xml" ) );
			CopyFile( Path.Combine( WorkFolder, "Binaries\\InstallData\\" ), Path.Combine( Application.StartupPath, "InstallData\\Interop.IWshRuntimeLibrary.dll" ) );
#endif
			return ( WorkFolder );
		}

		static bool Launch( string Executable, string WorkFolder, string CommandLine, bool bWait )
		{
			string FileName = Path.Combine( WorkFolder, "Binaries\\" + Executable );
			FileInfo ExecutableInfo = new FileInfo( FileName );

			if( ExecutableInfo.Exists )
			{
				ProcessStartInfo StartInfo = new ProcessStartInfo();

				StartInfo.FileName = FileName;
				StartInfo.Arguments = CommandLine;
				StartInfo.WorkingDirectory = Path.Combine( WorkFolder, "Binaries\\" );

				Process LaunchedProcess = Process.Start( StartInfo );

				if( bWait == true )
				{
					return Util.WaitForProcess( LaunchedProcess, 12000 );
				}
			}

			return ( true );
		}

		static void DisplayErrorMessage( string ErrorMessage )
		{
			string DisplayMessage = Util.GetPhrase( "ErrorMessage" ) + " " + ErrorMessage;
			DisplayMessage += Environment.NewLine + Util.GetPhrase( "UDKTrouble" ) + Environment.NewLine + " http://udk.com/troubleshooting";

			GenericQuery Query = new GenericQuery( "GQCaptionInstallFail", DisplayMessage, false, "GQCancel", true, "GQOK" );
			Query.ShowDialog();
		}

		static void UpdatePath()
		{
			// There has to be a better way of doing this
			IDictionary MachineEnvVars = Environment.GetEnvironmentVariables( EnvironmentVariableTarget.Machine );
			IDictionary UserEnvVars = Environment.GetEnvironmentVariables( EnvironmentVariableTarget.User );

			string SystemPath = "";
			string UserPath = "";

			foreach( DictionaryEntry Entry in MachineEnvVars )
			{
				string Key = ( string )Entry.Key;
				if( Key.ToLower() == "path" )
				{
					SystemPath = ( string )Entry.Value;
					break;
				}
			}

			foreach( DictionaryEntry Entry in UserEnvVars )
			{
				string Key = ( string )Entry.Key;
				if( Key.ToLower() == "path" )
				{
					UserPath = ( string )Entry.Value;
					break;
				}
			}

			Environment.SetEnvironmentVariable( "Path", SystemPath + ";" + UserPath );
		}

		static bool HandleInstallRedist( bool Standalone, bool IsGameInstall )
		{
			// Work out the name of the zip with the redist in it
			string ModuleName = Util.MainModuleName;
			if( Standalone )
			{
				// ModuleName is the UDK install name, so need to remap to the redist package
				ModuleName = "Redist/UE3Redist.exe";
			}

			// Extract all the redist files
			if( Util.OpenPackagedZipFile( ModuleName, Util.UDKStartRedistSignature ) )
			{
				Util.CreateProgressBar( Util.GetPhrase( "PBDecompPrereq" ) );

				// Extract all the redist elements
				string DestFolder = Path.Combine( GetWorkFolder(), "Redist\\" );
				Util.UnzipAllFiles( Util.MainZipFile, DestFolder );

				Util.DestroyProgressBar();

				RedistProgress Progress = new RedistProgress();
				Progress.Show();

				Util.bDependenciesSuccessful = true;
				try
				{
					string Status = "OK";

					Status = Util.InstallVCRedist( Progress, DestFolder );
					if( Status != "OK" )
					{
						DisplayErrorMessage( Status );
						Util.bDependenciesSuccessful = false;
					}

					// Install a minimal set up DirectX - this is a different set for UE3Redist vs. UE3RedistGame
					Status = Util.InstallDXCutdown( Progress, DestFolder );
					if( Status != "OK" )
					{
						DisplayErrorMessage( Status );
						Util.bDependenciesSuccessful = false;
					}

					Status = Util.InstallAMDCPUDrivers( Progress, DestFolder );
					if( Status != "OK" )
					{
						DisplayErrorMessage( Status );
						Util.bDependenciesSuccessful = false;
					}
				}
				catch( Exception Ex )
				{
					DisplayErrorMessage( Ex.Message );
					Util.bDependenciesSuccessful = false;
				}

				// Apply any path changes that may have occurred in the redist, and set for the current process
				UpdatePath();

				// Cleanup
				Util.ClosePackagedZipFile();

				// Close the progress bar
				Progress.Close();

				return ( Util.bDependenciesSuccessful );
			}

			return ( false );
		}

		static void InstallRedist()
		{
			DialogResult EULAResult = DialogResult.OK;
			if( !Util.bProgressOnly )
			{
				EULA EULAScreen = new EULA();
				EULAResult = EULAScreen.ShowDialog();
			}

			if( EULAResult == DialogResult.OK )
			{
				bool IsGameInstall = Util.MainModuleName.ToLower().Contains( "game" );
				if( HandleInstallRedist( false, IsGameInstall ) )
				{
					if( !Util.bProgressOnly )
					{
						GenericQuery Query = new GenericQuery( "GQCaptionRedistInstallComplete", "GQDescRedistInstallComplete", false, "GQCancel", true, "GQOK" );
						Query.ShowDialog();
					}
				}
			}
		}

		static void DoUninstall( bool bAll, bool bIsGame )
		{
			UninstallProgress Progress = new UninstallProgress( Util );
			Progress.Show();
			Application.DoEvents();

			foreach( Utils.GameManifestOptions GameInstallInfo in Util.Manifest.GameInfo )
			{
				Launch( GameInstallInfo.AppToCreateInis, Util.PackageInstallLocation, "-uninstallfw", true );
			}

			Util.DeleteFiles( bAll, bIsGame );
			Progress.SetDeletingShortcuts();
			Util.DeleteShortcuts();

			// Remove from ARP
			Util.RemoveUDKFromARP();
			Progress.Close();

			GenericQuery Query = new GenericQuery( "GQCaptionUninstallComplete", "GQDescUninstallComplete", false, "GQCancel", true, "GQOK" );
			Query.ShowDialog();
		}

		static void HandleUninstall()
		{
			Util.PackageInstallLocation = Util.MainModuleName.Substring( 0, Util.MainModuleName.Length - "Binaries\\UnSetup.exe".Length ).TrimEnd( "\\/".ToCharArray() );
			// Fix path if user installed to drive root
			if( Util.PackageInstallLocation.EndsWith( ":" ) )
			{
				Util.PackageInstallLocation += "\\";
			}

			// Are we a game or the UDK
			FileInfo UDKManifest = new FileInfo( Path.Combine( Util.PackageInstallLocation, Util.ManifestFileName ) );
			bool bIsGameInstall = !UDKManifest.Exists;

			UninstallOptions UninstallOptionsScreen = new UninstallOptions( bIsGameInstall );
			DialogResult UninstallOptionResult = UninstallOptionsScreen.ShowDialog();
			if( UninstallOptionResult == DialogResult.OK )
			{
				if( UninstallOptionsScreen.GetDeleteAll() )
				{
					GenericQuery AreYouSure = new GenericQuery( "GQCaptionUninstallAllSure", "GQDescUninstallAllSure", true, "GQCancel", true, "GQYes" );
					DialogResult AreYouSureResult = AreYouSure.ShowDialog();
					if( AreYouSureResult == DialogResult.OK )
					{
						DoUninstall( true, bIsGameInstall );
					}
				}
				else
				{
					DoUninstall( false, bIsGameInstall );
				}
			}
		}

		static void HandleInstall()
		{
			GenericQuery Query = null;

			// Read in the install specific data
			bool IsGameInstall = Util.IsFileInPackage( "Binaries\\UnSetup.Game.xml" );
			Util.CloseSplash();

			DialogResult EULAResult = DialogResult.OK;
			if( !Util.bProgressOnly )
			{
				EULA EULAScreen = new EULA();
				EULAResult = EULAScreen.ShowDialog();
			}

			if( EULAResult == DialogResult.OK )
			{
				bool bShowProjectSelectScreen = !IsGameInstall;
				ProjectSelect ProjectSelectScreen = new ProjectSelect();
				InstallOptions InstallOptionsScreen = new InstallOptions( IsGameInstall );
				DialogResult InstallOptionResult = DialogResult.OK;
				if( !Util.bProgressOnly )
				{
					do
					{
						if( bShowProjectSelectScreen )
						{
							DialogResult ProjectSelectResult = ProjectSelectScreen.ShowDialog();

							// If the user cancels here via the title bar x button, we will make sure not to show subsequent screens
							if( ProjectSelectResult == DialogResult.Cancel )
							{
								// We will break the loop and since the user canceled we will set the install options result to canceled as well
								InstallOptionResult = DialogResult.Cancel;
								break;
							}
						}
						// If we see the retry return result we know that the user hit the back button.
						InstallOptionResult = InstallOptionsScreen.ShowDialog();
					} while( InstallOptionResult == DialogResult.Retry );
				}

				if( InstallOptionResult == DialogResult.OK )
				{
					string Email = InstallOptionsScreen.GetSubscribeAddress();
					if( Email.Length > 0 )
					{
						Query = new GenericQuery( "GQCaptionSubscribe", "GQDescSubscribe", false, "GQCancel", false, "GQOK" );

                        // Capture the current time just before the dialog shows.  We will use this to make sure it is up long enough for the user to read.
                        DateTime ShowDialogTime = DateTime.Now;

						Query.Show();
						Application.DoEvents();

						bool bIsSubmitSuccessful = Util.SubscribeToMailingList( Email );

						// The dialog may have dissappeared before a user could read it, we pad out the operation to 3 seconds.  If the e-mail submit operation took
						//  longer we don't add any additional time here
                        TimeSpan TimeDiff = DateTime.Now - ShowDialogTime;
                        if ( TimeDiff.Milliseconds >= 0 && TimeDiff.Milliseconds < 3000 )
                        {
                            Thread.Sleep( 3000 - TimeDiff.Milliseconds );
                        }

						Query.Close();

                        if ( !bIsSubmitSuccessful )
                        {
                            GenericQuery FailurePrompt = new GenericQuery( "GQCaptioSubmitEmailFailed", "GQDescSubmitEmailFailed", false, "", true, "GQOK" );
                            FailurePrompt.OverrideDescriptionAlignment( HorizontalAlignment.Left );
                            FailurePrompt.ShowDialog();
                        }
					}

					if( !Util.bSkipDependencies )
					{
						HandleInstallRedist( true, IsGameInstall );
					}

					Util.CreateProgressBar( Util.GetPhrase( "PBDecompressing" ) );

					string DestFolder = InstallOptionsScreen.GetInstallLocation();
					Util.CustomProjectShortName = InstallOptionsScreen.GetProjectName();

					bool bUnzipSuccessful = false;
					bool IsCustomProject = Util.bIsCustomProject;
					if( !IsGameInstall && IsCustomProject )
					{
						bUnzipSuccessful = Util.ExtractCustomProjectFiles( DestFolder );
					}
					else
					{
						Util.OpenPackagedZipFile( Util.MainModuleName, Util.UDKStartZipSignature );
						bUnzipSuccessful = Util.UnzipAllFiles( Util.MainZipFile, DestFolder );
						Util.ClosePackagedZipFile();
					}


					if( bUnzipSuccessful )
					{

						// Run each game to generate the proper ini files...
						Util.UpdateProgressBar( Util.GetPhrase( "PBSettingInitial" ) );
						foreach( Utils.GameManifestOptions GameInstallInfo in Util.Manifest.GameInfo )
						{
							string CmdLine = "-firstinstall -LanguageForCooking=" + Util.Phrases.GetUE3Language() + " -installfw -namefw=" + GameInstallInfo.Name;
							Launch( GameInstallInfo.AppToCreateInis, DestFolder, CmdLine, true );
						}

						if( !IsGameInstall && IsCustomProject )
						{
							Util.UpdateProgressBar( Util.GetPhrase( "PBSettingInitial" ) );
							// Run each game to build scripts.
							foreach( Utils.GameManifestOptions GameInstallInfo in Util.Manifest.GameInfo )
							{
								Launch( GameInstallInfo.AppToCreateInis, DestFolder, "make -full -nopauseonsuccess -silent", true );
							}

							Util.UpdateProgressBar( Util.GetPhrase( "PBSettingInitial" ) );
							// Generate GAD checkpoint files.
							foreach( Utils.GameManifestOptions GameInstallInfo in Util.Manifest.GameInfo )
							{
								Launch( GameInstallInfo.AppToCreateInis, DestFolder, "CheckpointGameAssetDatabase -nopauseonsuccess -silent", true );
							}
						}

						// Adding shortcuts to the start menu
						Util.UpdateProgressBar( Util.GetPhrase( "GQAddingShortcuts" ) );

						string StartMenu = Util.GetAllUsersStartMenu();

						Application.DoEvents();

						// If they don't exist (are default) then we are installing the UDK
						if( !IsGameInstall )
						{
							StartMenu = Path.Combine( StartMenu, InstallOptionsScreen.GetStartMenuLocation() );
							Directory.CreateDirectory( StartMenu );
							Directory.CreateDirectory( Path.Combine( StartMenu, "Documentation\\" ) );
							Directory.CreateDirectory( Path.Combine( StartMenu, "Tools\\" ) );

							// Only include SpeedTreeModeler shortcuts if the SpeedTreeModeler folder exists
							DirectoryInfo DirInfo = new DirectoryInfo( Path.Combine( InstallOptionsScreen.GetInstallLocation(), "Binaries\\SpeedTreeModeler\\" ) );

							Util.CreateShortcuts( StartMenu, InstallOptionsScreen.GetInstallLocation(), DirInfo.Exists );

							// Adding to ARP
							Util.UpdateProgressBar( Util.GetPhrase( "RegUDK" ) );
							Util.AddUDKToARP( DestFolder, Util.Manifest.FullName + ": " + Util.UnSetupTimeStamp, !Util.bProgressOnly );
						}
						else
						{
							Util.CreateGameShortcuts( StartMenu, InstallOptionsScreen.GetInstallLocation() );

							// Adding to ARP
							Util.UpdateProgressBar( Util.GetPhrase( "RegGame" ) );
							Util.AddUDKToARP( DestFolder, Util.GetGameLongName(), !Util.bProgressOnly );
						}

						// Delete as many work files as we can
						Util.UpdateProgressBar( Util.GetPhrase( "CleaningUp" ) );
						Util.DeleteTempFiles( GetWorkFolder() );

						// Save install guid
						Util.SaveInstallInfo( Path.Combine( DestFolder, "Binaries" ) );

						// Destroy the progress bar so it does not obscure the finished dialog
						Util.DestroyProgressBar();

						bool bShowInstallExtras = false;

						// Check to see if the Install Extras needs to be shown.
						if( !IsGameInstall )
						{
							// The extras screen only includes Perforce tech at the moment so we first check and
							//  only display this screen if the client/server is not installed or outdated.
							string ClientVersion = Util.GetP4ClientVersion();
							string ServerVersion = Util.GetP4ServerVersion();
							if( ServerVersion == string.Empty ||
								ClientVersion == string.Empty ||
								Util.DoesP4ServerNeedUpgrade( DestFolder ) ||
								Util.DoesP4ClientNeedUpgrade( DestFolder ) )
							{
								bShowInstallExtras = true;
							}
						}

						InstallExtras IE = new InstallExtras( DestFolder );
						InstallFinished IF = new InstallFinished( DestFolder, IsGameInstall, bShowInstallExtras );
						DialogResult InstallFinishedResult = DialogResult.Cancel;
						if( !Util.bProgressOnly )
						{
							do
							{
								if( bShowInstallExtras )
								{
									DialogResult InstallExtrasResult = IE.ShowDialog();

									// If the user cancels here via the title bar x button, we will not show the finish screen
									if( InstallExtrasResult == DialogResult.Cancel )
									{
										break;
									}
								}

								// If the InstallFinished dialog returns with the Retry result, we know that the back button was pressed.
								InstallFinishedResult = IF.ShowDialog();

							} while( InstallFinishedResult == DialogResult.Retry );
						}


						// Launch game or editor if requested
						if( InstallFinishedResult == DialogResult.OK )
						{
							if( IsGameInstall )
							{
								if( IF.GetLaunchChecked() )
								{
									Launch( Util.Manifest.AppToLaunch, DestFolder, "", false );
								}
							}
							else
							{
								Utils.GameManifestOptions AppToLaunch = IF.GetLaunchType();
								if( AppToLaunch != null )
								{
									Launch( AppToLaunch.AppToElevate, DestFolder, AppToLaunch.AppCommandLine, false );
								}
							}
						}
					}
					else
					{
						Util.DestroyProgressBar();
					}
				}
			}
		}

		[STAThread]
		static int Main( string[] Arguments )
		{
			AttachConsole( ATTACH_PARENT_PROCESS );

			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault( false );

			string TLLN = System.Threading.Thread.CurrentThread.CurrentUICulture.ThreeLetterWindowsLanguageName;
			string ParentTLLN = System.Threading.Thread.CurrentThread.CurrentUICulture.Parent.ThreeLetterWindowsLanguageName;
			Util = new Utils( TLLN, ParentTLLN );

			Application.CurrentCulture = System.Globalization.CultureInfo.InvariantCulture;
			string WorkFolder = "";
			string CommandLine = "";
			int ErrorCode = 0;

			switch( ProcessArguments( Arguments ) )
			{
			case 0:
				break;

			case 1:
				ErrorCode = Util.ProcessBuildCommandLine( Arguments );

				switch( Util.BuildCommand )
				{
				case Utils.BUILDCOMMANDS.CreateManifest:
					Util.CreateManifest();
					break;

				case Utils.BUILDCOMMANDS.GameCreateManifest:
					ErrorCode = Util.GameCreateManifest();
					break;

				case Utils.BUILDCOMMANDS.BuildInstaller:
					ErrorCode = Util.BuildInstaller( Util.ManifestFileName );
					break;

				case Utils.BUILDCOMMANDS.BuildGameInstaller:
					Util.BuildInstaller( Util.GameManifestFileName );
					break;

				case Utils.BUILDCOMMANDS.Package:
					Util.PackageFiles();
					break;

				case Utils.BUILDCOMMANDS.PackageRedist:
					Util.PackageRedist( false );
					break;
#if DEBUG
				case Utils.BUILDCOMMANDS.UnPackage:
					Util.UnPackageFiles();
					break;
#endif
				}
				break;

			case 2:
				// Process any command line options
				Util.ProcessInstallCommandLine( Arguments );

				switch( Util.Command )
				{
				case Utils.COMMANDS.Help:
					Util.CloseSplash();
					Util.DisplayHelp();
					break;

				case Utils.COMMANDS.EULA:
					Util.CloseSplash();
					ErrorCode = Util.DisplayEULA();
					break;

				case Utils.COMMANDS.Game:
					Util.CloseSplash();
					GameDialog Game = new GameDialog();
					DialogResult Result = Game.ShowDialog();
					if( Result == DialogResult.OK )
					{
						Util.SaveGame();
					}
					else
					{
						ErrorCode = 1;
					}
					break;

				case Utils.COMMANDS.SetupInstall:
					if( Util.MainModuleName.EndsWith( "UnSetup.exe" ) )
					{
						Util.CloseSplash();
						Util.DisplayHelp();
					}
					else if( Util.OpenPackagedZipFile( Util.MainModuleName, Util.UDKStartZipSignature ) )
					{
						// Find the offset into the packaged file that represents the zip
						WorkFolder = ExtractWorkFiles();
						CommandLine = Util.GetCommandLine( "/HandleInstall" );
						Launch( "UnSetup.exe", WorkFolder, CommandLine, false );
					}
					break;

				case Utils.COMMANDS.Install:
					if( Util.ReadManifestOptions( false ) )
					{
						// Find the offset into the packaged file that represents the zip
						if( Util.OpenPackagedZipFile( Util.MainModuleName, Util.UDKStartZipSignature ) )
						{
							HandleInstall();
						}
					}
					break;

				case Utils.COMMANDS.SetupUninstall:
					WorkFolder = CopyUnSetup();
					CommandLine = Util.GetCommandLine( "/HandleUninstall" );
					Launch( "UnSetup.exe", WorkFolder, CommandLine, false );
					break;

				case Utils.COMMANDS.Uninstall:
					if( Util.ReadManifestOptions( false ) )
					{
						HandleUninstall();
					}
					break;

				case Utils.COMMANDS.Redist:
					// Find the offset into the packaged file that represents the zip
					if( Util.OpenPackagedZipFile( Util.MainModuleName, Util.UDKStartRedistSignature ) )
					{
						WorkFolder = ExtractRedistWorkFiles();
						CommandLine = Util.GetCommandLine( "/HandleRedist" );
						Launch( "UnSetup.exe", WorkFolder, CommandLine, true );
					}
					break;

				case Utils.COMMANDS.HandleRedist:
					Util.CloseSplash();
					InstallRedist();
					break;

				case Utils.COMMANDS.MakeShortcuts:
					DirectoryInfo ProjectFolder = Directory.GetParent( Application.StartupPath );
					if( ProjectFolder != null && ProjectFolder.Exists )
					{
						string StartMenuLocation = Path.Combine( Program.Util.Manifest.FullName + "\\", ProjectFolder.Name );

						string StartMenu = Util.GetAllUsersStartMenu();
						StartMenu = Path.Combine( StartMenu, StartMenuLocation );
						Directory.CreateDirectory( StartMenu );
						Directory.CreateDirectory( Path.Combine( StartMenu, "Documentation\\" ) );
						Directory.CreateDirectory( Path.Combine( StartMenu, "Tools\\" ) );

						// Only include SpeedTreeModeler shortcuts if the SpeedTreeModeler folder exists
						DirectoryInfo DirInfo = new DirectoryInfo( Path.Combine( ProjectFolder.FullName, "Binaries\\SpeedTreeModeler\\" ) );

						Util.CreateShortcuts( StartMenu, ProjectFolder.FullName, DirInfo.Exists );

						bool bDoAddToARP = true;

						FileInfo UDKManifest = new FileInfo( Path.Combine( ProjectFolder.FullName, "Binaries\\InstallData\\Manifest.xml" ) );
						if( !UDKManifest.Exists )
						{
							// If manifest is missing, then this is not UDK so we don't want to continue adding to ARP
							bDoAddToARP = false;
						}
						else if( Util.InstallInfoData.InstallGuidString.Length > 0 )
						{
							if( Util.GetUninstallInfo() != string.Empty )
							{
								// Uninstall info is present so we assume ARP entry was added already and we don't want to continue adding to ARP
								bDoAddToARP = false;
							}
						}

						if( bDoAddToARP )
						{
							if( Util.InstallInfoData.InstallGuidString.Length == 0 )
							{
								Util.InstallInfoData.InstallGuidString = Guid.NewGuid().ToString();
							}

							Util.AddUDKToARP( ProjectFolder.FullName, Util.Manifest.FullName + ": " + ProjectFolder.Name, true );

							// Save install guid
							Util.SaveInstallInfo( Path.Combine( ProjectFolder.FullName, "Binaries" ) );
						}
					}

					break;
#if DEBUG
				case Utils.COMMANDS.Extract:
					Util.MainZipFile = ZipFile.Read( Util.MainModuleName );
					ExtractWorkFiles();
					break;

				case Utils.COMMANDS.Subscribe:
					Util.SubscribeToMailingList( Util.SubscribeEmail );
					break;

				case Utils.COMMANDS.CheckSignature:
					Util.ValidateCertificate( Util.MainModuleName );
					break;

				case Utils.COMMANDS.Shortcuts:
					string StartMenuPath = Util.GetAllUsersStartMenu() + "\\UDKTest";
					Directory.CreateDirectory( StartMenuPath );
					Directory.CreateDirectory( Path.Combine( StartMenuPath, "Documentation\\" ) );
					Directory.CreateDirectory( Path.Combine( StartMenuPath, "Tools\\" ) );
					Directory.CreateDirectory( Path.Combine( StartMenuPath, "Uninstall\\" ) );

					Util.CreateShortcuts( StartMenuPath, "C:\\UDK\\UDK-2009-09", false );
					break;
#endif
				}

				break;
			}

			Util.Destroy();
			return ( ErrorCode );
		}
	}
}
