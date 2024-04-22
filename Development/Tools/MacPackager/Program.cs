/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Windows.Forms;
using System.ComponentModel;

namespace MacPackager
{
	public partial class Program
	{
		static public int ReturnCode = 0;
		static string MainCommand = "";
		static string MainRPCCommand = "";
		static public string GameName = "";
		static public string GameConfiguration = "";
		static public string[] Languages;
		static public bool bPackagingForMAS = false;

		static public SlowProgressDialog ProgressDialog = null;
		static public BackgroundWorker BGWorker = null;
		static public int ProgressIndex = 0;

		static public void UpdateStatus(string Line)
		{
			if (BGWorker != null)
			{
				int Percent = Math.Min(Math.Max(ProgressIndex, 0), 100);
				BGWorker.ReportProgress(Percent, Line);
			}
		}

		static public void Log(string Line)
		{
			UpdateStatus(Line);
			Console.WriteLine(Line);
		}

		static public void Log(string Line, params object[] Args)
		{
			Log(String.Format(Line, Args));
		}

		static public void Error(string Line)
		{
			if (Program.ReturnCode == 0)
			{
				Program.ReturnCode = 1;
			}

			Console.ForegroundColor = ConsoleColor.Red;
			Log("MacP ERROR: " + Line);

			Console.ResetColor();
		}

		static public void Error(string Line, params object[] Args)
		{
			Error(String.Format(Line, Args));
		}

		static public void Warning(string Line)
		{
			Console.ForegroundColor = ConsoleColor.Yellow;
			Log("MacP WARNING: " + Line);
			Console.ResetColor();
		}

		static public void Warning(string Line, params object[] Args)
		{
			Warning(String.Format(Line, Args));
		}

		static private bool ParseCommandLine(ref string[] Arguments)
		{
			if (Arguments.Length == 0)
			{
				Program.Warning("No arguments specified, assuming gui mode for UDKGame");
				Arguments = new string[] { "gui", "UDKGame", "Shipping", "INT" };
				//return ( false );
			}

			if (Arguments.Length == 1)
			{
				MainCommand = Arguments[0];
			}
			else if (Arguments.Length == 2)
			{
				MainCommand = Arguments[0];
				GameName = Arguments[1];
			}
			else if (Arguments.Length >= 3)
			{
				MainCommand = Arguments[0];
				GameName = Arguments[1];
				GameConfiguration = Arguments[2];
				if (Arguments.Length > 3)
				{
					Languages = Arguments[3].Split('+');
				}
				else
				{
					Languages = new string[] { "INT" };
				}

				for (int ArgIndex = 4; ArgIndex < Arguments.Length; ArgIndex++)
				{
					string Arg = Arguments[ArgIndex].ToLowerInvariant();

					if (Arg.StartsWith("-"))
					{
					}
					else
					{
						// RPC command
						MainRPCCommand = Arguments[ArgIndex];
					}
				}
			}

			return (true);
		}

		static private bool CheckArguments()
		{
			if (GameName.Length == 0 || GameConfiguration.Length == 0 || Languages.Length == 0)
			{
				Error("Invalid number of arguments");
				return false;
			}

			return true;
		}

		static public void ExecuteCommand(string Command, string RPCCommand)
		{
			if (ReturnCode > 0)
			{
				Warning("Error in previous command; suppressing: " + Command + " " + RPCCommand ?? "");
				return;
			}

			if (Config.bVerbose)
			{
				Log("");
				Log("----------");
				Log(String.Format("Executing command '{0}' {1}...", Command, (RPCCommand != null) ? ("'" + RPCCommand + "' ") : ""));
			}

			try
			{
				bool bHandledCommand = false;

				if (!bHandledCommand)
				{
					bHandledCommand = DeployTime.ExecuteDeployCommand(Command, RPCCommand);
				}

				if (!bHandledCommand)
				{
					bHandledCommand = true;
					switch (Command)
					{
						case "configure":
							RunInVisualMode(delegate { return ToolsHub.CreateShowingTools(); });
							break;

						default:
							bHandledCommand = false;
							break;
					}
				}

				if (!bHandledCommand)
				{
					Error("Unknown command");
					ReturnCode = 2;
				}
			}
			catch (Exception Ex)
			{
				Error("Error while executing command: " + Ex.ToString());
				ReturnCode = 200;
			}

			Console.WriteLine();
		}

		delegate Form CreateFormDelegate();

		static void StartVisuals()
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			ProgressDialog = new SlowProgressDialog();
		}

		static void RunInVisualMode(CreateFormDelegate Work)
		{
			StartVisuals();

			Form DisplayForm = Work.Invoke();
			Application.Run(DisplayForm);
		}

		/// <summary>
		/// The main entry point for the application.
		/// </summary>
		[STAThread]
		static int Main(string[] args)
		{
			try
			{
				if (!ParseCommandLine(ref args))
				{
					Log("Usage: MacPackager <Command> <GameName> <Configuration> [RPCCommand &| Switch]");
					Log("");
					Log("Common commands:");
					Log(" ... PackageApp GameName Configuration Language");
                    Log(" ... PackageMAS GameName Configuration Language");
                    Log("");
					Log("Sample commandline:");
					Log(" ... MacPackager PackageApp UDKGame Release INT");
					return 1;
				}

				Log("Executing MacPackager " + String.Join(" ", args));

				Config.Initialize();

				switch (MainCommand.ToLowerInvariant())
				{
				case "packageapp":
					if (CheckArguments())
					{
						// Create the .app
						CompileTime.PackageApp();
						DeployTime.ExecuteDeployCommand("deploy", ""); // @todo: remove this line once Deploy button in UFE is restored
					}
					break;

                case "packagemas":
                    if (CheckArguments())
                    {
						bPackagingForMAS = true;
                        // Create the .app
                        CompileTime.PackageApp();
						DeployTime.ExecuteDeployCommand("deploy", ""); // @todo: remove this line once Deploy button in UFE is restored
                    }
                    break;

                case "gui":
					RunInVisualMode(delegate { return ToolsHub.CreateShowingTools(); });
					break;

				case "guimas":
					bPackagingForMAS = true;
					RunInVisualMode(delegate { return ToolsHub.CreateShowingTools(); });
					break;

				default:
					// Commands by themself default to packaging for the device
					if (CheckArguments())
					{
						ExecuteCommand(MainCommand, MainRPCCommand);
					}
					break;
				}
			}
			catch( Exception Ex )
			{
                Error("Application exception: " + Ex.ToString());
				ReturnCode = 1;
			}

            return ( ReturnCode );
		}
	}
}
