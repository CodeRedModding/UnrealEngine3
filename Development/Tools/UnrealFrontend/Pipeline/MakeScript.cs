/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.Windows.Input;

namespace UnrealFrontend.Pipeline
{
	public class MakeScript : Pipeline.CommandletStep
	{
		#region Pipeline.Step

		public override bool SupportsClean { get { return true; } }

		public override String StepName { get { return "Script"; } }

		public override String StepNameToolTip { get { return "Compile scripts. (F4)"; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Compile scripts"; } }

		public override String CleanAndExecuteDesc { get { return "Full recompile"; } }

		public override Key KeyBinding { get { return Key.F4; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			return ProcessManager.StartProcess(
				CommandletStep.GetExecutablePath(InProfile, true, false),
				GetCompileScriptCommandLine( InProfile, false ),
				"",
				InProfile.TargetPlatform
			);
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			return ProcessManager.StartProcess(
				CommandletStep.GetExecutablePath(InProfile, true, false),
				GetCompileScriptCommandLine(InProfile, true),
				"",
				InProfile.TargetPlatform
			);
		}
		
		#endregion


		/// <summary>
		/// Generates the commandline for compiling scripts.
		/// </summary>
		/// <returns>The commandline for compiling scripts.</returns>
		public static string GetCompileScriptCommandLine(Profile InProfile, bool Full)
		{
			// plain make commandline
			string CommandLine = "make";

			switch (InProfile.ScriptConfiguration)
			{
				case Profile.Configuration.DebugScript:
					CommandLine += " -debug";
					break;
				case Profile.Configuration.FinalReleaseScript:
					CommandLine += " -final_release";
					break;
			}

			if (Full)
			{
				CommandLine += " -full";
			}

			return CommandLine;
		}
	}
}
