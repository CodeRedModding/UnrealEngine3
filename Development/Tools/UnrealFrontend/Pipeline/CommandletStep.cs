/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnrealFrontend.Pipeline
{
	public abstract class CommandletStep : Pipeline.Step
	{

		/// <summary>
		/// Calculate the name of the PC executable for commandlets and PC game
		/// </summary>
		/// <param name="bCommandlet">True if this is a path for a commandlet or tool.</param>
		/// <param name="bPromoteTox64">Force 64bit exe even if the profile is configured for 32bit</param>
		/// <returns></returns>
		public static string GetExecutablePath(Profile InProfile, bool bCommandletOrTool, bool bPromoteTox64)
		{
			// Figure out executable path.
			Profile.Configuration SelectedConfig = bCommandletOrTool ? InProfile.CommandletConfiguration : InProfile.LaunchConfiguration;

			string Executable = InProfile.SelectedGameName;

			if( InProfile.TargetPlatformType == ConsoleInterface.PlatformType.PCServer && !bCommandletOrTool)
			{
				Executable += "Server";
			}

			if( InProfile.UseMProfExe && !bCommandletOrTool )
			{
				Executable += ".MProf";
			}

			switch (SelectedConfig)
			{
				case Profile.Configuration.Debug_32:
					Executable = "Win32\\" + Executable + "-Win32-Debug.exe";
					break;

				case Profile.Configuration.Release_32:
					Executable = "Win32\\" + Executable + ".exe";
					break;

				case Profile.Configuration.Test_32:
				case Profile.Configuration.Shipping_32:
					// Special case certain exes
					if( Executable.ToLower() == "udkgame" )
					{
						Executable = "Win32\\UDK.exe";
					}
					else if( Executable.ToLower() == "mobilegame" )
					{
						Executable = "Win32\\UDKMobile.exe";
					}
					else
					{
                        if (SelectedConfig == Profile.Configuration.Test_32)
                        {
                            Executable = "Win32\\" + Executable + "-Win32-Test.exe";
                        }
                        else
                        {
                            Executable = "Win32\\" + Executable + "-Win32-Shipping.exe";
                        }
					}
					break;

				case Profile.Configuration.Debug_64:
					Executable = "Win64\\" + Executable + "-Win64-Debug.exe";
					break;

				case Profile.Configuration.Release_64:
					Executable = "Win64\\" + Executable + ".exe";
					break;

				case Profile.Configuration.Test_64:
				case Profile.Configuration.Shipping_64:
					// Special case certain exes
					if( Executable.ToLower() == "udkgame" )
					{
						Executable = "Win64\\UDK.exe";
					}
					else if( Executable.ToLower() == "mobilegame" )
					{
						Executable = "Win64\\UDKMobile.exe";
					}
					else
					{
                        if (SelectedConfig == Profile.Configuration.Test_64)
                        {
                            Executable = "Win64\\" + Executable + "-Win64-Test.exe";
                        }
                        else
                        {
                            Executable = "Win64\\" + Executable + "-Win64-Shipping.exe";
                        }
					}
					break;
			}

			return (Executable);
		}




		/// <summary>
		/// Retrieves the list of languages to use for cooking and syncing.
		/// </summary>
		/// <returns>An array of languages to use to for cooking and syncing.</returns>
		public static string[] GetLanguagesToCookAndSync( Profile InProfile, Profile.Languages LanguageSelection )
		{
			List<string> Languages = new List<string>();

			if( LanguageSelection == Profile.Languages.All || LanguageSelection == Profile.Languages.OnlyDefault )
			{
				Languages.Add( "INT" );
			}

			if( LanguageSelection == Profile.Languages.All || LanguageSelection == Profile.Languages.OnlyNonDefault )
			{
				foreach( LangOption LanguageOption in InProfile.Cooking_LanguagesToCookAndSync.Content )
				{
					if( LanguageOption.IsEnabled )
					{
						if( LanguageOption.Name != "INT" )
						{
							Languages.Add( LanguageOption.Name );
						}
					}
				}
			}

			return Languages.ToArray();
		}

		/// <summary>
		/// Retrieves the list of texture formats to use for cooking.
		/// </summary>
		/// <returns>An array of texture foramts to use to for cooking .</returns>
		public static string[] GetTextureFormatsToCookAndSync(Profile InProfile)
		{
			if (InProfile.TargetPlatform.Type == ConsoleInterface.PlatformType.Android)
			{
				List<string> Formats = new List<string>();

				foreach (LangOption LanguageOption in InProfile.Cooking_TextureFormat.Content)
				{
					if (LanguageOption.IsEnabled)
					{
						Formats.Add(LanguageOption.Name);
					}
				}

				// Always cook DXT if nothing was checked
				if (Formats.Count == 0)
				{
					Formats.Add("DXT");
				}

                // If there is a texture filter set throw out the formats and use the filter instead
                switch (InProfile.Android_TextureFilter)
                {
                    case EAndroidTextureFilter.DXT:
                        Formats.Clear();
                        Formats.Add("DXT");
                        break;
                    case EAndroidTextureFilter.ATITC:
                        Formats.Clear();
                        Formats.Add("ATITC");
                        break;
                    case EAndroidTextureFilter.PVRTC:
                        Formats.Clear();
                        Formats.Add("PVRTC");
                        break;
                    case EAndroidTextureFilter.ETC:
                        Formats.Clear();
                        Formats.Add("ETC");
                        break;
                    case EAndroidTextureFilter.NONE:
                    default:
                        break;
                }

				return Formats.ToArray();
			}

			// return nothing for non-Android
			return null;
		}


		/// <summary>
		/// Creates the exec temp file.
		/// </summary>
		public static void CreateTempExec(Profile InProfile)
		{
			string TmpExecLocation;

			TmpExecLocation = "UnrealFrontend_TmpExec.txt";

			if (InProfile.UseExecCommands)
			{
				System.IO.File.WriteAllLines(TmpExecLocation, InProfile.Launch_ExecCommands.Split(Environment.NewLine[0]));
			}
			
		}

		/// <summary>
		/// Generate a final URL to pass to the game
		/// </summary>
		/// <param name="GameOptions">Game type options (? options)</param>
		/// <param name="PostCmdLine">Engine type options (- options)</param>
		/// <param name="bCreateExecFile"></param>
		/// <returns></returns>
		protected static string GetFinalURL(Profile InProfile, string GameOptions, string EngineOptions, bool bCreateExecFile)
		{
			// build the commandline
			StringBuilder CmdLine = new StringBuilder();

			if (GameOptions != null && GameOptions.Length > 0)
			{
				CmdLine.Append(GameOptions);
			}

			GameOptions = BuildGameCommandLine(InProfile);

			if (GameOptions != null && GameOptions.Length > 0)
			{
				CmdLine.Append(' ');
				CmdLine.Append(GameOptions);
			}

			if (EngineOptions != null && EngineOptions.Length > 0)
			{
				CmdLine.Append(' ');
				CmdLine.Append(EngineOptions);
			}

			CmdLine.Append(" -Exec=UnrealFrontend_TmpExec.txt");

			// final pass for execs
			if (bCreateExecFile)
			{
				CreateTempExec(InProfile);
			}

			// if the DefaultMapString is in the URL, replace it with nothing (to make the game use the default map)
			CmdLine = CmdLine.Replace(DefaultMapString, "");

			return CmdLine.ToString();
		}


		/// <summary>
		/// Builds the game execution command line.
		/// </summary>
		/// <returns>The command line that will be used to execute the game with the current options and configuration.</returns>
		public static string BuildGameCommandLine(Profile InProfile)
		{
			StringBuilder Bldr = new StringBuilder();

			if (InProfile.Launch_NoVSync)
			{
				Bldr.Append("-novsync ");
			}

			if (InProfile.Launch_CaptureFPSChartInfo)
			{
				Bldr.Append("-gCFPSCI ");
			}

			return Bldr.ToString().Trim();
		}

		// the string to show in the Map to Play box when no map is manually entered - this implies using the default map in the game's .ini file
		private static readonly string DefaultMapString = "<Default>";



	}
}
