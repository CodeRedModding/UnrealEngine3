using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace UnrealFrontend.Pipeline
{
	public class UnSetup : Pipeline.Step
	{
		public UnSetup()
		{
			this.ShouldSkipThisStep = true;
		}

		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Package Game"; } }

		public override String StepNameToolTip { get { return "Package Game"; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Package Game"; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			String CWD = Environment.CurrentDirectory.Substring(0, Environment.CurrentDirectory.Length - "\\Binaries".Length);
			if (CWD.EndsWith(":"))
			{
				CWD += "\\";
			}

			StringBuilder CommandLine = new StringBuilder();

			// Step 1: Configure mod
			CommandLine.Append("/GameSetup");
			bool bSuccess = ProcessManager.StartProcess("UnSetup.exe", CommandLine.ToString(), CWD, InProfile.TargetPlatform);
			if(bSuccess)
			{
				bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
			}

			if(bSuccess)
			{
				// Step 2: Create mod manifest
				CommandLine = new StringBuilder();
				CommandLine.Append("-GameCreateManifest");
				AppendGameNameAndPlatform(InProfile, ref CommandLine);
				bSuccess = ProcessManager.StartProcess("UnSetup.exe", CommandLine.ToString(), CWD, InProfile.TargetPlatform);
				if (bSuccess)
				{
					bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
				}
			}

			if(bSuccess)
			{
				// Step 3: Build mod installer 
				CommandLine = new StringBuilder();
				CommandLine.Append("-BuildGameInstaller");
				AppendGameNameAndPlatform(InProfile, ref CommandLine);
				bSuccess = ProcessManager.StartProcess("UnSetup.exe", CommandLine.ToString(), CWD, InProfile.TargetPlatform);
				if (bSuccess)
				{
					bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
				}
			}

			if(bSuccess)
			{
				// Step 4: Package game 
				CommandLine = new StringBuilder();
				CommandLine.Append("-Package");
				AppendGameNameAndPlatform(InProfile, ref CommandLine);
				bSuccess = ProcessManager.StartProcess("UnSetup.exe", CommandLine.ToString(), CWD, InProfile.TargetPlatform);
				if (bSuccess)
				{
					bSuccess = ProcessManager.WaitForActiveProcessesToComplete();
				}
			}

			return bSuccess;
		}

		private void AppendGameNameAndPlatform(Profile InProfile, ref StringBuilder OutCommandline)
		{
			// append the game name and platform.
			string ExtraArguments = "";
			string GameName = InProfile.SelectedGameName.Substring(0, (InProfile.SelectedGameName.Length - "Game".Length));
			ExtraArguments += (" -gamename=\"" + GameName + "\"");
			ExtraArguments += (" -platform=\"" + InProfile.TargetPlatform + "\"");
			OutCommandline.Append(ExtraArguments);
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			throw new NotImplementedException();
		}


	}
}
