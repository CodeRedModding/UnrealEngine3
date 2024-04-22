/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text;
using System.IO;
using System.Diagnostics;
using System.Threading;
using System.Reflection;
// using Microsoft.VisualStudio.VCProjectEngine;
// using EnvDTE100;

namespace UnrealBuildTool
{
    partial class UnrealBuildTool
	{
		/** Time at which control fist passes to UBT. */
		static public DateTime StartTime;

		/** Total time spent linking. */
		static public double TotalLinkTime = 0;

		static int Main(string[] Arguments)
		{
			//Output command line options for debugging purposes
			Console.Out.WriteLine("UBT Arguments: {0}", string.Join(" ", Arguments));

			// Change the working directory to be the Development/Src folder.
			Directory.SetCurrentDirectory(Path.Combine(Path.GetDirectoryName(Assembly.GetExecutingAssembly().Location), "..\\..\\..\\Src"));

			// Helpers used for stats tracking.
			StartTime = DateTime.UtcNow;
			int NumExecutedActions = 0;
			TimeSpan MutexWaitTime = TimeSpan.Zero;
			string ExecutorName = "Unknown";
			var Targets = new List<UE3BuildTarget>();

			// Don't allow simultaneous execution of Unreal Built Tool. Multi-selection in the UI e.g. causes this and you want serial
			// execution in this case to avoid contention issues with shared produced items.
			bool bSuccess = true;
			bool bCreatedMutex = false;
			using (Mutex SingleInstanceMutex = new Mutex(true, "Global\\UnrealBuildTool_Mutex", out bCreatedMutex))
			{
				if (!bCreatedMutex)
				{
					// If this instance didn't create the mutex, wait for the existing mutex to be released by the mutex's creator.
					DateTime MutexWaitStartTime = DateTime.UtcNow;
					SingleInstanceMutex.WaitOne();
					MutexWaitTime = DateTime.UtcNow - MutexWaitStartTime;
				}

				try
                {
					List<string[]> TargetSettings = ParseCommandLineFlags(Arguments);

					// Build action lists for all passed in targets.
					var TargetOutputItems = new List<FileItem>();
					foreach( string[] TargetSetting in TargetSettings )
					{										
						var Target = new UE3BuildTarget(TargetSetting);
						Targets.Add(Target);
						TargetOutputItems.AddRange(Target.Build());
					}

					// Plan the actions to execute for the build.
					List<Action> ActionsToExecute = GetActionsToExecute(TargetOutputItems);
					NumExecutedActions = ActionsToExecute.Count;

					// Display some stats to the user.
					if (BuildConfiguration.bPrintDebugInfo)
					{
						Console.WriteLine(
							"{0} actions, {1} outdated and requested actions",
							AllActions.Count,
							ActionsToExecute.Count
							);
					}

					// Any additional pre-build actions, but only if we're about to do any work
					if (NumExecutedActions > 0 || BuildConfiguration.bUseXcodeToolchain)
					{
						foreach( var SyncTarget in Targets )
						{
							// If the platform requires it, perform any pre-build syncing to remote machines, etc.
							if (SyncTarget.GetPlatform() == UnrealTargetPlatform.IPhone)
							{
								IPhoneToolChain.SyncHelper(	
									SyncTarget.GetGameName(), 
									SyncTarget.GetPlatform(), 
									SyncTarget.GetConfiguration(), 
									SyncTarget.GetOutputDirectory(), 
									true);

								// Don't distribute via XGE if we're compiling for iPhone and this is the only target. This allows us to properly
								// adjust the number of concurrent tasks based on the Mac we are using to compile on.
								if( Targets.Count == 1 )
								{
									BuildConfiguration.bAllowXGE = false;
								}
							}
							else if (SyncTarget.GetPlatform() == UnrealTargetPlatform.Mac)
							{
								MacToolChain.SyncHelper(
									SyncTarget.GetGameName(),
									SyncTarget.GetPlatform(),
									SyncTarget.GetConfiguration(),
									SyncTarget.GetOutputDirectory(),
									true);

								// Don't distribute via XGE if we're compiling for Mac and this is the only target. This allows us to properly
								// adjust the number of concurrent tasks based on the Mac we are using to compile on.
								if (Targets.Count == 1)
								{
									BuildConfiguration.bAllowXGE = false;
								}
							}
						}
					}

					// Execute the actions.
					bSuccess = ExecuteActions(ActionsToExecute, out ExecutorName, Targets.Count > 0 ? Targets[0].GetPlatform() : UnrealTargetPlatform.Unknown);

					// Any additional post-build actions, but only if the build was successful and it actually did any work
					if (NumExecutedActions > 0 && bSuccess)
					{
						foreach( var SyncTarget in Targets )
						{
							// If the platform requires it, perform any post-build installation, signing, copy back to PC, etc.
							if (SyncTarget.GetPlatform() == UnrealTargetPlatform.IPhone)
							{
								IPhoneToolChain.SyncHelper(
									SyncTarget.GetGameName(), 
									SyncTarget.GetPlatform(), 
									SyncTarget.GetConfiguration(), 
									SyncTarget.GetOutputDirectory(), 
									false);
							}
							else if (SyncTarget.GetPlatform() == UnrealTargetPlatform.Mac)
							{
								MacToolChain.SyncHelper(
									SyncTarget.GetGameName(),
									SyncTarget.GetPlatform(),
									SyncTarget.GetConfiguration(),
									SyncTarget.GetOutputDirectory(),
									false);
							}
						}
					}
                }
				catch (BuildException Exception)
				{
					// treat our (expected) BuildExceptions as nice messages, not the full callstack dump
					Console.WriteLine("ERROR: " + Exception.Message);
					bSuccess = false;
				}
				catch (Exception Exception)
				{
					Console.WriteLine("{0}", Exception);
					bSuccess = false;
				}

				// Release the mutex.
				SingleInstanceMutex.ReleaseMutex();
			}

			// Figure out how long we took to execute and update stats DB if there is a valid target.
			double BuildDuration = (DateTime.UtcNow - StartTime - MutexWaitTime).TotalSeconds;
			if( Targets.Count > 0 )
			{
				PerfDataBase.SendBuildSummary( BuildDuration, TotalLinkTime, Targets.Count != 1 ? null : Targets[0], bSuccess, AllActions.Count, NumOutdatedActions, NumExecutedActions, ExecutorName );
			}

			// Update duration to include time taken to talk to the database and log it to the console
			double BuildAndSqlDuration = (DateTime.UtcNow - StartTime - MutexWaitTime).TotalSeconds;
			if( ExecutorName == "Local" )
			{
				Console.WriteLine("[{0}] UBT execution time: {1:0.00} seconds, {2:0.00} seconds linking", DateTime.Now.ToString("t"), BuildAndSqlDuration, TotalLinkTime);
			}
			else if( ExecutorName == "XGE" )
			{
				Console.WriteLine("[{0}] XGE execution time: {1:0.00} seconds", DateTime.Now.ToString("t"), BuildAndSqlDuration);
			}
			
			// Warn is connecting to the DB took too long.
			if( BuildAndSqlDuration - BuildDuration > 1 )
			{
				Console.WriteLine("Warning: Communicating with the Database took {0} seconds.",BuildAndSqlDuration - BuildDuration);
			}

			return bSuccess ? 0 : 1;
		}

		/**
		 * Parses the passed in command line for build configuration overrides.
		 * 
		 * @param	Arguments	List of arguments to parse
		 * @return	List of build target settings
		 */
		private static List<string[]> ParseCommandLineFlags(string[] Arguments)
		{
			var TargetSettings = new List<string[]>();
			int ArgumentIndex = 0;

			// Log command-line arguments.
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Console.Write("Command-line arguments: ");
				foreach (string Argument in Arguments)
				{
					Console.Write("{0} ", Argument);
				}
				Console.WriteLine("");
			}

			// Parse optional command-line flags.
			if (Utils.ParseCommandLineFlag(Arguments, "-verbose", out ArgumentIndex))
			{
				BuildConfiguration.bPrintDebugInfo = true;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-noxge", out ArgumentIndex))
			{
				BuildConfiguration.bAllowXGE = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-gendsym", out ArgumentIndex))
			{
				BuildConfiguration.bGeneratedSYMFile = true;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-noxgemonitor", out ArgumentIndex))
			{
				BuildConfiguration.bShowXGEMonitor = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-stresstestunity", out ArgumentIndex))
			{
				BuildConfiguration.bStressTestUnity = true;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-disableunity", out ArgumentIndex))
			{
				BuildConfiguration.bUseUnityBuild = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-nopch", out ArgumentIndex))
			{
				BuildConfiguration.bUsePCHFiles = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-noCommandDependencies", out ArgumentIndex))
			{
				BuildConfiguration.bUseCommandDependencies = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-noLTCG", out ArgumentIndex))
			{
				BuildConfiguration.bAllowLTCG = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-nopdb", out ArgumentIndex))
			{
				BuildConfiguration.bUsePDBFiles = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-deploy", out ArgumentIndex))
			{
				BuildConfiguration.bDeployAfterCompile = true;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-nodebuginfo", out ArgumentIndex))
			{
				BuildConfiguration.bDisableDebugInfo = true;
			}
			else if( Utils.ParseCommandLineFlag( Arguments, "-forcedebuginfo", out ArgumentIndex ) )
			{
				BuildConfiguration.bDisableDebugInfo = false;
				BuildConfiguration.bOmitPCDebugInfoInRelease = false;
			}

			if (Utils.ParseCommandLineFlag(Arguments, "-targets", out ArgumentIndex))
			{
				if (ArgumentIndex + 1 >= Arguments.Length)
				{
					throw new BuildException("Expected filename after -targets argument, but found nothing.");
				}				
				// Parse lines from the referenced file into target settings.
				var Lines = File.ReadAllLines( Arguments[ArgumentIndex+1] );
				foreach( string Line in Lines )
				{
					if( Line != "" && Line[0] != ';' )
					{
						TargetSettings.Add( Line.Split( ' ' ) );
					}
				}
			}
			// Simply use full command line arguments as target setting if not otherwise overriden.
			else
			{
				TargetSettings.Add( Arguments );
			}

			return TargetSettings;
		}
	}
}

