/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.IO;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Input;


namespace UnrealFrontend.Pipeline
{
	public class Cook : Pipeline.CommandletStep
	{
		private enum ECookOptions
		{
			None,
			FullRecook,
			INIsOnly,
		}

		#region Pipeline.Step
		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			bool bSuccess = Execute_Internal(ProcessManager, InProfile, ECookOptions.None);

            return bSuccess;
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			return Execute_Internal(ProcessManager, InProfile, ECookOptions.FullRecook);
		}

		public override bool SupportsClean { get { return true; } }

		public override String StepName { get { return "Cook"; } }

		public override String StepNameToolTip { get { return "Cook Packages. (F5)"; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Cook Packages"; } }

		public override String CleanAndExecuteDesc { get { return "Clean and Full Recook"; } }

		public override Key KeyBinding { get { return Key.F5; } }

		#endregion

		public bool ExecuteINIsOnly( IProcessManager ProcessManager, Profile InProfile )
		{
			return Execute_Internal(ProcessManager, InProfile, ECookOptions.INIsOnly);
		}

        private static bool CheckForAudioSolution()
        {
            return File.Exists("NoRedist\\Flash\\Lame.exe");
        }

		private static bool Execute_Internal(IProcessManager ProcessManager, Profile InProfile, ECookOptions CookingOptions)
		{
			ConsoleInterface.Platform CurPlatform = InProfile.TargetPlatform;

			if (CurPlatform == null)
			{
				return false;
			}

            if (CurPlatform.Type == ConsoleInterface.PlatformType.Flash)
            {
                if (!CheckForAudioSolution())
                {
                    var result = System.Windows.Forms.MessageBox.Show("Binaries\\NoRedist\\Flash\\Lame.exe was not found.\n\nWould you like to open a browser to download it from http://lame.sourceforge.net/links.php#Binaries", 
														  "Lame not found", 
														  System.Windows.Forms.MessageBoxButtons.YesNo, 
														  System.Windows.Forms.MessageBoxIcon.Error );

                    if (result == System.Windows.Forms.DialogResult.Yes)
                    {
                        System.Diagnostics.Process.Start("http://lame.sourceforge.net/links.php#Binaries");
                        return false;
                    }

                }
            }

			// handle files (TextureFileCache especially) being open on PC when trying to cook
			if ((CurPlatform.Type == ConsoleInterface.PlatformType.PS3 || CurPlatform.Type == ConsoleInterface.PlatformType.WiiU) && 
				CookingOptions != ECookOptions.INIsOnly)
			{
				// Disconnect any running PS3s to close all file handles.
				foreach (Target SomeTarget in InProfile.TargetsList.Targets)
				{
					if (SomeTarget.ShouldUseTarget)
					{
						SomeTarget.TheTarget.Disconnect();
					}
				}
			}

            if ( CurPlatform.Type == ConsoleInterface.PlatformType.PC && 
                 Session.Current.UDKMode == EUDKMode.UDK )
            {
                // Use LINQ to see if the package step is in the build queue, if it is we will skip the UnSetup /GameSetup prompt here
                bool bShouldSkip = InProfile.Pipeline.Steps.Any(entry => (entry.GetType() == typeof(UnrealFrontend.Pipeline.UnSetup) && entry.ShouldSkipThisStep == false));
                if (!bShouldSkip)
                {
                    string ModInfoPath = "UnSetup.Game.xml";

                    if (!File.Exists(ModInfoPath) || CookingOptions == ECookOptions.FullRecook)
                    {
                        String CWD = Environment.CurrentDirectory.Substring(0, Environment.CurrentDirectory.Length - "\\Binaries".Length);
                        if (CWD.EndsWith(":"))
                        {
                            CWD += "\\";
                        }
                        StringBuilder CommandLine = new StringBuilder();

                        CommandLine.Append("/GameSetup");
                        bool ProcSuccess = ProcessManager.StartProcess("UnSetup.exe", CommandLine.ToString(), CWD, InProfile.TargetPlatform);
                        if (ProcSuccess)
                        {
                            ProcessManager.WaitForActiveProcessesToComplete();
                        }
                    }
                }
            }

			bool bSuccess = false;

			// android may need to loop over cooking for multiple texture formats
			if (InProfile.TargetPlatformType == ConsoleInterface.PlatformType.Android)
			{
				string[] Formats = GetTextureFormatsToCookAndSync(InProfile);

                // Start the cook
                string CommandLine = GetCookingCommandLine(InProfile, CookingOptions);

                if (Formats.Length > 0)
                {
                    CommandLine += " -texformat=";
                    for (int i = 0; i < Formats.Length; ++i)
                    {
                        if (i > 0)
                        {
                            CommandLine += "|";
                        }

                        string Format = Formats[i];
                        CommandLine += Format;
                    }
                }
                
                bSuccess = ProcessManager.StartProcess(
                            CommandletStep.GetExecutablePath(InProfile, true, false),
                            CommandLine,
                            "",
                            InProfile.TargetPlatform
                        );
			}
			else
			{
				// Start the cook
				bSuccess = ProcessManager.StartProcess(
						CommandletStep.GetExecutablePath(InProfile, true, false),
						GetCookingCommandLine(InProfile, CookingOptions),
						"",
						InProfile.TargetPlatform
					);
			}


			if ((InProfile.TargetPlatformType == ConsoleInterface.PlatformType.PS3 ||
					InProfile.TargetPlatformType == ConsoleInterface.PlatformType.NGP)
				 && bSuccess)
			{
				// On PS3 and NGP we want to create the TOC right away because there is no Sync step.
				bSuccess =
					ProcessManager.WaitForActiveProcessesToComplete() &&
					Pipeline.Sync.RunCookerSync(ProcessManager, InProfile, false, false);
			}

			return bSuccess;
		}


		private static string GetCookingCommandLine(Profile InProfile, ECookOptions Options)
		{
			// Base command
			string CommandLine = "CookPackages -platform=" + InProfile.TargetPlatform.Name;

			if (Options == ECookOptions.INIsOnly)
			{
				CommandLine += " -inisOnly";
			}
			else
			{
				if (InProfile.Cooking_MapsToCook.Count > 0)
				{
					// Add in map name
					CommandLine += " " + InProfile.Cooking_MapsToCookAsString.Trim();
				}
			}

			if ( InProfile.IsCookDLCProfile )
			{
				ConsoleInterface.Platform CurPlateform = InProfile.TargetPlatform;

				if (CurPlateform.Type == ConsoleInterface.PlatformType.Xbox360 || CurPlateform.Type == ConsoleInterface.PlatformType.PS3)
				{
					CommandLine += " -dlcname=" + InProfile.DLC_Name;
				}
			}

			// Add in the final release option if necessary
			switch (InProfile.ScriptConfiguration)
			{
				case Profile.Configuration.DebugScript:
					CommandLine += " -debug";
					break;
				case Profile.Configuration.FinalReleaseScript:
					CommandLine += " -final_release";
					break;
			}

			// Add in the final release option if necessary
			if (Options == ECookOptions.FullRecook)
			{
				CommandLine += " -full";
			}

			if (InProfile.Cooking_UseFastCook)
			{
				CommandLine += " -FASTCOOK";
			}

			// Get all languages that need to be cooked
			String[] Languages = GetLanguagesToCookAndSync(InProfile, Profile.Languages.All);

			// INT is always cooked.
			String LanguageCookString = "INT";

			foreach( String Language in Languages )
			{
				if( Language != "INT")
				{
					// Add the language if its not INT.  INT is already added to the string
					LanguageCookString += "+"+Language ;
				}
			}

			//// Always add in the language we cook for
			CommandLine += " -multilanguagecook=" + LanguageCookString;
			
			{
				String TrimmedAdditionalOptions = InProfile.Cooking_AdditionalOptions.Trim();
				if (TrimmedAdditionalOptions.Length > 0)
				{
					CommandLine += " " + TrimmedAdditionalOptions;
				}
			}

			return CommandLine;
		}
	}
}
