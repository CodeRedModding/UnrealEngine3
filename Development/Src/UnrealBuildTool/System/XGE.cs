/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Xml;

namespace UnrealBuildTool
{
	class XGE
	{
		public static ExecutionResult ExecuteActions(List<Action> Actions)
		{
			ExecutionResult XGEResult = ExecutionResult.TasksSucceeded;
			if (Actions.Count > 0)
			{
				// Write the actions to execute to a XGE task file.
				string XGETaskFilePath = Path.Combine(BuildConfiguration.BaseIntermediatePath, "XGETasks.xml");
				WriteTaskFile(Actions, XGETaskFilePath);

				// Try to execute the XGE tasks, and if XGE is available, skip the local execution fallback.
				XGEResult = XGE.ExecuteTaskFile(XGETaskFilePath, Actions[0].OutputEventHandler);
			}
			return XGEResult;
		}

		/** Writes a XGE task file containing the specified actions to the specified file path. */
		public static void WriteTaskFile(List<Action> Actions, string TaskFilePath)
		{
			XmlDocument XGETaskDocument = new XmlDocument();

			// <BuildSet FormatVersion="1">...</BuildSet>
			XmlElement BuildSetElement = XGETaskDocument.CreateElement("BuildSet");
			XGETaskDocument.AppendChild(BuildSetElement);
			BuildSetElement.SetAttribute("FormatVersion", "1");

			// <Environments>...</Environments>
			XmlElement EnvironmentsElement = XGETaskDocument.CreateElement("Environments");
			BuildSetElement.AppendChild(EnvironmentsElement);

			// <Environment Name="Default">...</CompileEnvironment>
			XmlElement EnvironmentElement = XGETaskDocument.CreateElement("Environment");
			EnvironmentsElement.AppendChild(EnvironmentElement);
			EnvironmentElement.SetAttribute("Name", "Default");
			
			// <Tools>...</Tools>
			XmlElement ToolsElement = XGETaskDocument.CreateElement("Tools");
			EnvironmentElement.AppendChild(ToolsElement);

			for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
			{
				Action Action = Actions[ActionIndex];

				// <Tool ... />
				XmlElement ToolElement = XGETaskDocument.CreateElement("Tool");
				ToolsElement.AppendChild(ToolElement);
				ToolElement.SetAttribute("Name", string.Format("Tool{0}", ActionIndex));
				ToolElement.SetAttribute("AllowRemote", Action.bCanExecuteRemotely.ToString());
				ToolElement.SetAttribute("GroupPrefix", Action.StatusDescription);
				ToolElement.SetAttribute("Params", Action.CommandArguments);
				ToolElement.SetAttribute("Path", Action.CommandPath);
				ToolElement.SetAttribute("VCCompiler", Action.bIsVCCompiler.ToString());
				ToolElement.SetAttribute("SkipIfProjectFailed", "true");
				if (Action.bIsGCCCompiler)
				{
					ToolElement.SetAttribute( "AutoReserveMemory", "*.gch" );
				}
				ToolElement.SetAttribute(
					"OutputFileMasks", 
					string.Join(
						",",
						Action.ProducedItems.ConvertAll<string>(
							delegate(FileItem ProducedItem) { return ProducedItem.ToString(); }
							).ToArray()
						)
					);
				if ( Action.bShouldLogIfExecutedByXGE )
				{
					System.Console.WriteLine("{0}", Action.StatusDescription);
				}
			}

			// <Project Name="Default" Env="Default">...</Project>
			XmlElement ProjectElement = XGETaskDocument.CreateElement("Project");
			BuildSetElement.AppendChild(ProjectElement);
			ProjectElement.SetAttribute("Name", "Default");
			ProjectElement.SetAttribute("Env", "Default");

			for (int ActionIndex = 0; ActionIndex < Actions.Count; ActionIndex++)
			{
				Action Action = Actions[ActionIndex];

				// <Task ... />
				XmlElement TaskElement = XGETaskDocument.CreateElement("Task");
				ProjectElement.AppendChild(TaskElement);
				TaskElement.SetAttribute("SourceFile", "");
				TaskElement.SetAttribute("Name", string.Format("Action{0}",ActionIndex));
				TaskElement.SetAttribute("Tool", string.Format("Tool{0}", ActionIndex));
				TaskElement.SetAttribute("WorkingDir", Action.WorkingDirectory);
				TaskElement.SetAttribute("SkipIfProjectFailed", "true");

				// Create a semi-colon separated list of the other tasks this task depends on the results of.
				List<string> DependencyNames = new List<string>();
				foreach(FileItem Item in Action.PrerequisiteItems)
				{
					if(Item.ProducingAction != null && Actions.Contains(Item.ProducingAction))
					{
						DependencyNames.Add(string.Format("Action{0}", Actions.IndexOf(Item.ProducingAction)));
					}
				}

				if (DependencyNames.Count > 0)
				{
					TaskElement.SetAttribute("DependsOn", string.Join(";", DependencyNames.ToArray()));
                }
			}

			// Write the XGE task XML to a temporary file.
			using (FileStream OutputFileStream = new FileStream(TaskFilePath, FileMode.Create, FileAccess.Write))
			{
				XGETaskDocument.Save(OutputFileStream);
			}
		}

		/** The possible result of executing tasks with XGE. */
		public enum ExecutionResult
		{
			Unavailable,
			TasksFailed,
			TasksSucceeded,
		}

		/**
		 * Executes the tasks in the specified file.
		 * @param TaskFilePath - The path to the file containing the tasks to execute in XGE XML format.
		 * @return Indicates whether the tasks were successfully executed.
		 */
		public static ExecutionResult ExecuteTaskFile(string TaskFilePath, DataReceivedEventHandler OutputEventHandler )
		{
            string SilentOption = BuildConfiguration.bSilentCompileOutput ? "/Silent" : "";

            ProcessStartInfo XGEStartInfo = new ProcessStartInfo(
				"xgConsole",
				string.Format("{0} /Rebuild /NoWait /StopOnErrors /NoLogo {1}", TaskFilePath, SilentOption)
				);
			XGEStartInfo.UseShellExecute = false;

			// Use the IDE-integrated Incredibuild monitor to display progress.
			XGEStartInfo.Arguments += " /UseIdeMonitor";

			// Optionally display the external XGE monitor.
			if (BuildConfiguration.bShowXGEMonitor)
			{
				XGEStartInfo.Arguments += " /OpenMonitor";
			}

			try
			{
				// Start the process, redirecting stdout/stderr if requested.
				Process XGEProcess = new Process();
				XGEProcess.StartInfo = XGEStartInfo;
				bool bShouldRedirectOuput = OutputEventHandler != null;
				if (bShouldRedirectOuput)
				{
					XGEStartInfo.RedirectStandardError = true;
					XGEStartInfo.RedirectStandardOutput = true;
					XGEProcess.EnableRaisingEvents = true;
					XGEProcess.OutputDataReceived += OutputEventHandler;
					XGEProcess.ErrorDataReceived += OutputEventHandler;
				}
				XGEProcess.Start();
				if (bShouldRedirectOuput)
				{
					XGEProcess.BeginOutputReadLine();
					XGEProcess.BeginErrorReadLine();
				}

				// Wait until the process is finished and return whether it all the tasks successfully executed.
				XGEProcess.WaitForExit();
				return XGEProcess.ExitCode == 0 ? 
					ExecutionResult.TasksSucceeded :
					ExecutionResult.TasksFailed;
			}
			catch (Exception)
			{
				// If an exception is thrown while starting the process, return Unavailable.
				return ExecutionResult.Unavailable;
			}
		}
	}
}
