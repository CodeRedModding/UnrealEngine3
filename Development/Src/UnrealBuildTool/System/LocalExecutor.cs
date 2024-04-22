/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections;
using System.Collections.Generic;
using System.Text;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using System.Threading;
using System.Reflection;

namespace UnrealBuildTool
{
	class ActionThread
	{
		/** Cache the exit code from the command so that the executor can report errors */
		public int ExitCode = 0;

		/** Set to true only when the local or RPC action is complete */
		public bool bComplete = false;

		/** Tracks how long the action took to complete */
		public TimeSpan TotalProcessorTime;

		/** Cache the action that this thread is managing */
		Action Action;




		/** Regex that matches environment variables in $(Variable) format. */
		static Regex EnvironmentVariableRegex = new Regex("\\$\\(([\\d\\w]+)\\)");

		/** Replaces the environment variables references in a string with their values. */
		public static string ExpandEnvironmentVariables(string Text)
		{
			foreach (Match EnvironmentVariableMatch in EnvironmentVariableRegex.Matches(Text))
			{
				string VariableValue = Environment.GetEnvironmentVariable(EnvironmentVariableMatch.Groups[1].Value);
				Text = Text.Replace(EnvironmentVariableMatch.Value, VariableValue);
			}
			return Text;
		}


		/**
		 * Constructor, takes the action to process
		 */
		public ActionThread(Action InAction)
		{
			Action = InAction;
		}

		/**
		 * Sends a string to an action's OutputEventHandler
		 */
		private static void SendOutputToEventHandler(Action Action, string Output)
		{
			// do nothing with empty strings
			if (string.IsNullOrEmpty(Output))
			{
				return;
			}

			// pass the output to any handler requested
			if (Action.OutputEventHandler != null)
			{
				// NOTE: This is a pretty nasty hack with C# reflection, however it saves us from having to replace all the
				// handlers in various Toolchains with a wrapper handler that takes a string - it is certainly doable, but 
				// touches code outside of this class that I don't want to touch right now

				// DataReceivedEventArgs is not normally constructable, so work around it with creating a scratch Args object
				DataReceivedEventArgs EventArgs = (DataReceivedEventArgs)System.Runtime.Serialization.FormatterServices.GetUninitializedObject(typeof(DataReceivedEventArgs));

				// now we need to set the Data field using reflection, since it is read only
				FieldInfo[] ArgFields = typeof(DataReceivedEventArgs).GetFields(BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.DeclaredOnly);

				// call the handler once per line
				string[] Lines = Output.Split("\r\n".ToCharArray());
				foreach (string Line in Lines)
				{
					// set the Data field
					ArgFields[0].SetValue(EventArgs, Line);

					// finally call the handler with this faked object
					Action.OutputEventHandler(Action, EventArgs);
				}
			}
			else
			{
				// if no handler, print it out!
				Console.WriteLine(Output);
			}
			
		}
		/**
		 * The actual function to run in a thread. This is potentially long and blocking
		 */
		private void ThreadFunc()
		{
			// thread start time
			DateTime StartTime = DateTime.UtcNow;
			Action.StartTime = DateTimeOffset.Now;

			// Log command-line used to execute task if debug info printing is enabled.
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Console.WriteLine("Executing: {0} {1}", Action.CommandPath, Action.CommandArguments);
			}
			// Log summary if wanted.
			else if (Action.bShouldLogIfExecutedLocally)
			{
				Console.WriteLine(Action.StatusDescription);
			}

			if (Action.ActionHandler != null)
			{
				// call the function and get the ExitCode and an output string
				string Output;
				Action.ActionHandler(Action, out ExitCode, out Output);
				SendOutputToEventHandler(Action, Output);
			}
			else
			{
				// Create the action's process.
				ProcessStartInfo ActionStartInfo = new ProcessStartInfo();
				ActionStartInfo.WorkingDirectory = ExpandEnvironmentVariables(Action.WorkingDirectory);
				ActionStartInfo.FileName = ExpandEnvironmentVariables(Action.CommandPath);
				ActionStartInfo.Arguments = ExpandEnvironmentVariables(Action.CommandArguments);
				ActionStartInfo.UseShellExecute = false;
				ActionStartInfo.RedirectStandardInput = Action.bShouldBlockStandardInput;
				ActionStartInfo.RedirectStandardOutput = Action.bShouldBlockStandardOutput;
				ActionStartInfo.RedirectStandardError = Action.bShouldBlockStandardOutput;

				// Try to launch the action's process, and produce a friendly error message if it fails.
				Process ActionProcess = null;
				try
				{
					ActionProcess = new Process();
					ActionProcess.StartInfo = ActionStartInfo;
					bool bShouldRedirectOuput = Action.OutputEventHandler != null;
					if (bShouldRedirectOuput)
					{
						ActionStartInfo.RedirectStandardOutput = true;
						ActionStartInfo.RedirectStandardError = true;
						ActionProcess.EnableRaisingEvents = true;
						ActionProcess.OutputDataReceived += Action.OutputEventHandler;
						ActionProcess.ErrorDataReceived += Action.OutputEventHandler;
					}
					ActionProcess.Start();
					ActionProcess.PriorityClass = ProcessPriorityClass.BelowNormal;
					if (bShouldRedirectOuput)
					{
						ActionProcess.BeginOutputReadLine();
						ActionProcess.BeginErrorReadLine();
					}
				}
				catch (Exception)
				{
					throw new BuildException("Failed to start local process for action: {0} {1}", Action.CommandPath, Action.CommandArguments);
				}

				// block until it's complete
				while (!ActionProcess.HasExited)
				{
					Thread.Sleep(100);
				}

				// capture exit code
				ExitCode = ActionProcess.ExitCode;
			}

			// track how long it took
			TotalProcessorTime = DateTime.UtcNow.Subtract(StartTime);

			// let RPCUtilHelper clean up anything thread related
			RPCUtilHelper.OnThreadComplete();

			// we are done!!
			bComplete = true;
		}

		/**
		 * Starts a thread and runs the action in that thread
		 */
		public void Run()
		{
			Thread T = new Thread(ThreadFunc);
			T.Start();
		}
	};

	class LocalExecutor
	{

		/**
		 * Executes the specified actions locally.
		 * @return True if all the tasks succesfully executed, or false if any of them failed.
		 */
		public static bool ExecuteActions(List<Action> Actions)
		{
			// Time to sleep after each iteration of the loop in order to not busy wait.
			const float LoopSleepTime = 0.1f;

			Dictionary<Action, ActionThread> ActionThreadDictionary = new Dictionary<Action, ActionThread>();
			while (true)
			{
				// Count the number of unexecuted and still executing actions.
				int NumUnexecutedActions = 0;
				int NumExecutingActions = 0;
				foreach (Action Action in Actions)
				{
					ActionThread ActionThread = null;
					bool bFoundActionThread = ActionThreadDictionary.TryGetValue(Action, out ActionThread);
					if (bFoundActionThread == false)
					{
						NumUnexecutedActions++;
					}
					else if (ActionThread != null)
					{
						if (ActionThread.bComplete == false)
						{
							NumUnexecutedActions++;
							NumExecutingActions++;
						}
						// Set action end time. Accuracy is dependent on loop execution and wait time.
						else if( Action.EndTime == DateTimeOffset.MinValue )
						{
							Action.EndTime = DateTimeOffset.Now;
							// Separately keep track of total time spent linking.
							if( Action.bIsLinker )
							{
								double LinkTime = (Action.EndTime - Action.StartTime).TotalSeconds;
								UnrealBuildTool.TotalLinkTime += LinkTime;
							}
						}
					}
				}

				// If there aren't any unexecuted actions left, we're done executing.
				if (NumUnexecutedActions == 0)
				{
					break;
				}

				// If there are fewer actions executing than the maximum, look for unexecuted actions that don't have any outdated
				// prerequisites.
				foreach (Action Action in Actions)
				{
					ActionThread ActionProcess = null;
					bool bFoundActionProcess = ActionThreadDictionary.TryGetValue(Action, out ActionProcess);
					if (bFoundActionProcess == false)
					{
						if (NumExecutingActions < Math.Max(1,System.Environment.ProcessorCount * BuildConfiguration.ProcessorCountMultiplier) )
						{
							// Determine whether there are any prerequisites of the action that are outdated.
							bool bHasOutdatedPrerequisites = false;
							bool bHasFailedPrerequisites = false;
							foreach (FileItem PrerequisiteItem in Action.PrerequisiteItems)
							{
								if (PrerequisiteItem.ProducingAction != null && Actions.Contains(PrerequisiteItem.ProducingAction))
								{
									ActionThread PrerequisiteProcess = null;
									bool bFoundPrerequisiteProcess = ActionThreadDictionary.TryGetValue(PrerequisiteItem.ProducingAction, out PrerequisiteProcess);
									if (bFoundPrerequisiteProcess == true)
									{
										if (PrerequisiteProcess == null)
										{
											bHasFailedPrerequisites = true;
										}
										else if (PrerequisiteProcess.bComplete == false)
										{
											bHasOutdatedPrerequisites = true;
										}
										else if (PrerequisiteProcess.ExitCode != 0)
										{
											bHasFailedPrerequisites = true;
										}
									}
									else
									{
										bHasOutdatedPrerequisites = true;
									}
								}
							}

							// If there are any failed prerequisites of this action, don't execute it.
							if (bHasFailedPrerequisites)
							{
								// Add a null entry in the dictionary for this action.
								ActionThreadDictionary.Add( Action, null );
							}
							// If there aren't any outdated prerequisites of this action, execute it.
							else if (!bHasOutdatedPrerequisites)
							{
								ActionThread ActionThread = new ActionThread(Action);
								ActionThread.Run();

								// Add the action's process to the dictionary.
								ActionThreadDictionary.Add(Action, ActionThread);

								NumExecutingActions++;
							}
						}
					}
				}

				System.Threading.Thread.Sleep(TimeSpan.FromSeconds(LoopSleepTime));
			}

            if( BuildConfiguration.bLogDetailedActionStats )
            {
                Console.WriteLine("^Thread seconds (total)^Thread seconds (self)^Tool^Task^Description^Using PCH");
            }
			double TotalThreadSeconds = 0;
			double TotalThreadSelfSeconds = 0;

			// Check whether any of the tasks failed and log action stats if wanted.
			bool bSuccess = true;
			foreach (KeyValuePair<Action, ActionThread> ActionProcess in ActionThreadDictionary)
			{
				Action Action = ActionProcess.Key;
				ActionThread ActionThread = ActionProcess.Value;

				// Check for unexecuted actions, preemptive failure
				if (ActionThread == null)
				{
					bSuccess = false;
					continue;
				}
				// Check for executed action but general failure
				if (ActionThread.ExitCode != 0)
				{
					bSuccess = false;
				}
                // Log CPU time, tool and task.
				double ThreadSeconds = (Action.EndTime - Action.StartTime).TotalSeconds - LoopSleepTime;

				if (BuildConfiguration.bLogDetailedActionStats)
				{
                    Console.WriteLine( "^{0}^{1}^{2}^{3}^{4}^{5}", 
						ThreadSeconds,
                        ActionThread.TotalProcessorTime.TotalSeconds, 
						Path.GetFileName(Action.CommandPath), 
                        Action.StatusDescription,
                        Action.StatusDetailedDescription,
						Action.bIsUsingPCH);
                }
				// Keep track of total thread seconds spent on tasks.
				TotalThreadSeconds += ThreadSeconds;
				TotalThreadSelfSeconds += ActionThread.TotalProcessorTime.TotalSeconds;
			}

			// Log total CPU seconds and numbers of processors involved in tasks.
			if( BuildConfiguration.bLogDetailedActionStats || BuildConfiguration.bPrintDebugInfo )
			{
				Console.WriteLine("Thread seconds: {0} Thread seconds (self) {1}  Processors: {2}", TotalThreadSeconds, TotalThreadSelfSeconds, System.Environment.ProcessorCount);
			}

			return bSuccess;
		}
	};
}
