using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace UnrealFrontend.Pipeline
{
	public class UnProp : Pipeline.Step
	{
		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Prop"; } }

		public override String StepNameToolTip { get { return "Distribute a build to UnrealProp."; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Send to UnProp"; } }

		public UnProp()
		{
			this.ShouldSkipThisStep = true;
		}

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			string PlatformName = InProfile.TargetPlatformType.ToString();
			string TimeStampString = DateTime.Now.ToString("yyyy-MM-dd_HH.mm");

			DirectoryInfo Branch = Directory.GetParent(Directory.GetCurrentDirectory());
			
			string DestPath = string.Format("{0}\\{1}User\\{2}\\{1}_{2}_[{3}]_[{4}]", Session.Current.StudioSettings.UnPropHostDirectory, InProfile.SelectedGameName.Replace("Game", ""), PlatformName, TimeStampString, Environment.UserName.ToUpper());

			List<string> DestPathList = new List<string>();
			DestPathList.Add(DestPath);

			// Copy up and loc data (needs to be done first as UnrealProp keys off the INT TOC file)
			ConsoleInterface.TOCSettings LocBuildSettings = Pipeline.Sync.CreateTOCSettings(InProfile, false, Profile.Languages.OnlyNonDefault);
			if( LocBuildSettings != null && LocBuildSettings.Languages.Length > 0 )
			{
				LocBuildSettings.TargetsToSync.Clear();
				LocBuildSettings.DestinationPaths = DestPathList;
				
				string LocCommandLine = Pipeline.Sync.GenerateCookerSyncCommandLine( InProfile, LocBuildSettings, "Loc", false, false );
				ProcessManager.StartProcess( "CookerSync.exe", LocCommandLine, "", InProfile.TargetPlatform );
				ProcessManager.WaitForActiveProcessesToComplete();
			}

			// we don't want to sync to a console so clear targets
			ConsoleInterface.TOCSettings BuildSettings = Pipeline.Sync.CreateTOCSettings( InProfile, false, Profile.Languages.OnlyDefault );
			if( BuildSettings != null )
			{
				BuildSettings.TargetsToSync.Clear();
				BuildSettings.DestinationPaths = DestPathList;

				string CommandLine = Pipeline.Sync.GenerateCookerSyncCommandLine( InProfile, BuildSettings, "CompleteBuild", true, false );

				return ProcessManager.StartProcess( "CookerSync.exe", CommandLine, "", InProfile.TargetPlatform );
			}

			return false;
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			return true;
		}
	}
}
