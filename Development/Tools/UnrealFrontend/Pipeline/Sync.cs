/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Input;

using Color = System.Drawing.Color;

namespace UnrealFrontend.Pipeline
{
	public class RebootAndSync : Pipeline.Sync
	{
		public override bool SupportsReboot { get { return true; } }
	}

	public class Sync : Pipeline.CommandletStep
	{
		#region Pipeline.Step

		public override bool SupportsClean { get { return true; } }

		public override String StepName { get { return "Sync"; } }

		public override String StepNameToolTip { get { return "Sync cooked data. (F6)"; } }

		public override String ExecuteDesc { get { return "Sync Cooked Data"; } }

		public override bool SupportsReboot { get { return false; } }

		public override Key KeyBinding { get { return Key.F6; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			return RunCookerSync(ProcessManager, InProfile, this.SupportsReboot && this.RebootBeforeStep, false);
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			return RunCookerSync(ProcessManager, InProfile, this.SupportsReboot && this.RebootBeforeStep, true);
		}

		#endregion

		public static bool RunCookerSync( IProcessManager ProcessManager, Profile InProfile, bool bShouldReboot, bool bForceSync )
		{
			ConsoleInterface.Platform CurPlat = InProfile.TargetPlatform;

			bool bSuccess = true;
			bool bAlreadyRebooted = false;

			// Copy all the localised data (if any) (needs to be done first as certain tools (e.g. UnrealProp) key off the INT TOC file)
			ConsoleInterface.TOCSettings LocBuildSettings = CreateTOCSettings( InProfile, bForceSync, Profile.Languages.OnlyNonDefault );
			if( LocBuildSettings != null && LocBuildSettings.Languages.Length > 0 )
			{
				string LocCommandLine = GenerateCookerSyncCommandLine( InProfile, LocBuildSettings, "Loc", false, bShouldReboot );
				bSuccess = ProcessManager.StartProcess("CookerSync.exe", LocCommandLine, "", InProfile.TargetPlatform);
				bSuccess = bSuccess && ProcessManager.WaitForActiveProcessesToComplete();
				bAlreadyRebooted = true;				
			}

			// Copy all the common data
			ConsoleInterface.TOCSettings BuildSettings = CreateTOCSettings( InProfile, bForceSync, Profile.Languages.OnlyDefault );
			if ( bSuccess && BuildSettings != null )
			{
				string TagSet = InProfile.SupportsPDBCopy && InProfile.Sync_CopyDebugInfo ? "ConsoleSync" : "ConsoleSyncProgrammer";
				string CommonCommandLine = GenerateCookerSyncCommandLine( InProfile, BuildSettings, TagSet, false, bShouldReboot && !bAlreadyRebooted );
				bSuccess = ProcessManager.StartProcess("CookerSync.exe", CommonCommandLine, "", InProfile.TargetPlatform);
			}

			return bSuccess;
		}


		/// <summary>
		/// Generates the TOC for the current platform.
		/// </summary>
		/// <returns>The TOC for the current platform.</returns>
		internal static ConsoleInterface.TOCSettings CreateTOCSettings(Profile InProfile, bool bForceSync, Profile.Languages LanguageSelection)
		{
			ConsoleInterface.TOCSettings BuildSettings = new ConsoleInterface.TOCSettings(Session.Current.SessionLog.ConsoleOutputHandler);

			foreach (Target CurTarget in InProfile.TargetsList.Targets)
			{
				if (CurTarget.ShouldUseTarget)
				{
					BuildSettings.TargetsToSync.Add(CurTarget.Name);
				}
			}

			BuildSettings.GameName = InProfile.SelectedGameName;
			BuildSettings.TargetBaseDirectory = InProfile.Targets_ConsoleBaseDir;
			BuildSettings.Languages = GetLanguagesToCookAndSync(InProfile, LanguageSelection);
			BuildSettings.TextureExtensions = GetTextureFormatsToCookAndSync(InProfile);
			BuildSettings.Force = BuildSettings.Force || bForceSync;
			BuildSettings.NoDest = BuildSettings.TargetsToSync.Count == 0;

			if( BuildSettings.TargetBaseDirectory.IndexOf( ' ' ) >= 0 )
			{
				Session.Current.SessionLog.AddLine( Color.Red, "ERROR: Target base directory has spaces. Spaces are not allowed!" );
				return null;
			}

			return BuildSettings;
		}


		/// <summary>
		/// Writes out the commandline to a text file usable on mobile devices
		/// </summary>
		internal static bool UpdateMobileCommandlineFile(Profile InProfile)
		{
			SessionLog Log = Session.Current.SessionLog;

			// get some options from UFE
			ConsoleInterface.Platform CurPlat = InProfile.TargetPlatform;
			string PlatformName = CurPlat.Type.ToString();
			string GameOptions, EngineOptions;
			GetSPURLOptions(InProfile, out GameOptions, out EngineOptions);

			// compute where to write it
			string DestinationDirectory = "..\\" + InProfile.SelectedGameName + "\\Cooked" + PlatformName;

			// end with a slash
			DestinationDirectory += "\\";

			if (!System.IO.Directory.Exists(DestinationDirectory))
			{
				Log.AddLine(Color.Red, "DIRECTORY DOES NOT EXIST: " + DestinationDirectory);
				Log.AddLine(Color.Red, " ... aborting");
				return (false);
			}

			// generate the commandline
			string GameCommandLine = GetFinalURL(InProfile, GameOptions, EngineOptions, false);

			// write it out
			string Filename = DestinationDirectory + "UE3CommandLine.txt";
			System.IO.File.WriteAllText(Filename, GameCommandLine);

			// if we want to use network file loading, then write out this computer's IP address to a special
			// file that is synced (different from the CommandLine.txt so that we can load the commandline
			// over the network)
			if (InProfile.Mobile_UseNetworkFileLoader)
			{
				// get my IP address
				System.Net.IPAddress[] LocalAddresses = System.Net.Dns.GetHostAddresses("");
				if (LocalAddresses == null)
				{
					// make sure we have a local IP address
					System.Windows.MessageBox.Show("Can't use network file hosting, since no local IP address could be determined. Disabling Networked File Loader.");
					InProfile.Mobile_UseNetworkFileLoader = false;
				}
				else
				{
					string HostFilename = DestinationDirectory + "UE3NetworkFileHost.txt";
					// write out our first IPv4 address
					foreach (System.Net.IPAddress Addr in LocalAddresses)
					{
						if (Addr.AddressFamily == System.Net.Sockets.AddressFamily.InterNetwork)
						{
							System.IO.File.WriteAllText(HostFilename, Addr.ToString());
							break;
						}
					}
				}
			}

			Log.WriteCommandletEvent(Color.Blue, string.Format("Writing game commandline [{0}] to '{1}'", GameCommandLine, Filename));

			return (true);
		}



		/// <summary>
		/// Generates the command line used for starting a cooker sync commandlet.
		/// </summary>
		/// <param name="CommandLine">The StringBuilder being used to generate the commandline.</param>
		/// <param name="PlatSettings">The settings being used to sync.</param>
		/// <param name="TagSet">The tag set to sync.</param>
		public static string GenerateCookerSyncCommandLine(Profile InProfile, ConsoleInterface.TOCSettings BuildSettings, string TagSet, bool EnforceFolderName, bool bRebootBeforeSync)
		{
			ConsoleInterface.Platform CurPlat = InProfile.TargetPlatform;
			StringBuilder CommandLine = new StringBuilder();
			StringBuilder Languages = new StringBuilder();

			//NOTE: Due to a workaround for an issue with cooking multiple languages
			// it's possible for INT to show up in the languages list twice.
			// Use this dictionary to prevent that.
			Dictionary<string, string> FinalLanguagesToSync = new Dictionary<string, string>();

			foreach (string CurLang in BuildSettings.Languages)
			{
				if (!FinalLanguagesToSync.ContainsKey(CurLang))
				{
					FinalLanguagesToSync[CurLang] = CurLang;

					Languages.Append(" -r ");
					Languages.Append(CurLang);
				}
			}

			// force the command line for Wii U (this could be in a Sync subclass? - or could be put 
			// into grun, but CookerSync doesn't work in Cygwin right now)
			if (CurPlat.Type == ConsoleInterface.PlatformType.WiiU)
			{
				CommandLine.AppendFormat("{0} -p {1} -x ConsoleSync{2}{3} -b . ..\\{0}\\Build\\WiiU\\Host\\content",
					BuildSettings.GameName, CurPlat.Type.ToString(), Languages.ToString(), BuildSettings.Force ? " -f" : "");
				return CommandLine.ToString();
			}
			// force the command line for Flash (this could be in a Sync subclass?
			else if (CurPlat.Type == ConsoleInterface.PlatformType.Flash)
			{
				CommandLine.AppendFormat("{0} -p {1} -x CookedData{2}{3} -b . ..\\{0}\\Build\\{1}\\Website\\FlashBuild",
					BuildSettings.GameName, CurPlat.Type.ToString(), Languages.ToString(), BuildSettings.Force ? " -f" : "");
				return CommandLine.ToString();
			}

			if (EnforceFolderName)
			{
				if( !BuildSettings.TargetBaseDirectory.StartsWith( "UnrealEngine3" ) && !BuildSettings.TargetBaseDirectory.StartsWith( "UE3" ) )
				{
					System.Windows.MessageBox.Show("Console base directory must start with 'UnrealEngine3' or 'UE3'.", "UnrealProp Error", System.Windows.MessageBoxButton.OK, System.Windows.MessageBoxImage.Error);
					return "";
				}
			}

			CommandLine.AppendFormat("{0} -p {1} -x {2}{3} -b \"{4}\"", BuildSettings.GameName, CurPlat.Type.ToString(), TagSet, Languages.ToString(), BuildSettings.TargetBaseDirectory);

			if (bRebootBeforeSync)
			{
				CommandLine.Append(" -o");
			}			

			if (BuildSettings.Force)
			{
				CommandLine.Append(" -f");
			}

			if (!BuildSettings.GenerateTOC)
			{
				CommandLine.Append(" -notoc");
			}

			if (BuildSettings.VerifyCopy)
			{
				CommandLine.Append(" -v");
			}

			if (BuildSettings.NoSync)
			{
				CommandLine.Append(" -n");
			}

			if (BuildSettings.NoDest)
			{
				CommandLine.Append(" -nd");
			}

			if (BuildSettings.MergeExistingCRC)
			{
				CommandLine.Append(" -m");
			}

			if (BuildSettings.ComputeCRC)
			{
				CommandLine.Append(" -c");
			}

			if (BuildSettings.TextureExtensions != null)
			{
                foreach (string TexExtension in BuildSettings.TextureExtensions)
                {
                    CommandLine.Append(" -texformat " + TexExtension);
                }
			}

			foreach (string Target in BuildSettings.TargetsToSync)
			{
				CommandLine.Append(' ');
				CommandLine.Append(Target);
			}

			foreach (string Target in BuildSettings.DestinationPaths)
			{
				CommandLine.Append(' ');
				CommandLine.Append('\"');
				CommandLine.Append(Target);
				CommandLine.Append('\"');
			}

			return CommandLine.ToString();
		}

		/// <summary>
		/// Gets the options for a single player game on the current platform.
		/// </summary>
		/// <param name="GameOptions">Receives the game options URL.</param>
		/// <param name="EngineOptions">Receives the engine options URL.</param>
		public static void GetSPURLOptions(Profile InProfile, out string GameOptions, out string EngineOptions)
		{
			if (InProfile.Launch_UseUrl == 1)
			{
				GameOptions = InProfile.Launch_Url.Trim();
			}
			else 
			{
				// put together with URL options
				GameOptions = (InProfile.LaunchDefaultMap ? "" : InProfile.MapToPlay.Name);

			}

			// just use the extra options to start with
			EngineOptions = InProfile.Launch_ExtraOptions;

			EngineOptions = EngineOptions.Trim();

			if( InProfile.TargetPlatformType == ConsoleInterface.PlatformType.PCServer )
			{
				EngineOptions += " -seekfreeloadingserver";
			}
			else if( InProfile.TargetPlatformType == ConsoleInterface.PlatformType.PCConsole )
			{
				EngineOptions += " -seekfreeloadingpcconsole";
			}
			else if( InProfile.TargetPlatformType == ConsoleInterface.PlatformType.PC )
			{
				EngineOptions += " -seekfreeloading";
			}

            // Launch in install mode if in Android distribution
            if (InProfile.IsAndroidDistribution)
            {
                EngineOptions += " -installed";
            }

            if (InProfile.IsAndroidProfile && InProfile.Android_SkipDownloader)
            {
                EngineOptions += " -skipdownloader";
            }

			//Resolution Res;
			//if (ComboBox_Platform.Text == "PC" && Resolution.TryParse(ComboBox_Game_Resolution.Text, out Res))
			//{
			//    EngineOptions += string.Format(" -resx={0} -resy={1}", Res.Width, Res.Height);
			//}
		}

	}


}
