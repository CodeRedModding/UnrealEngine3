using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Color = System.Drawing.Color;
using System.IO;

namespace UnrealFrontend.Pipeline
{
	public enum EMacPackageMode
	{
		Normal,
		MacAppStore,
	}

	public class PackageMac : Pipeline.CommandletStep
	{
		public static String EMacPackageMode_ToFriendlyName(EMacPackageMode InPackageMode)
		{
			switch (InPackageMode)
			{
				default: return InPackageMode.ToString();
				case EMacPackageMode.Normal: return "Normal";
				case EMacPackageMode.MacAppStore: return "Mac App Store";
			}
		}

		public static List<EMacPackageMode> GetValidPackageModes()
		{
			List<EMacPackageMode> AvailablePackageModes = new List<EMacPackageMode>
			{
				EMacPackageMode.Normal,
				EMacPackageMode.MacAppStore,
			};

			return AvailablePackageModes;
		}

		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Package"; } }

		public override String StepNameToolTip { get { return "Package Mac app."; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Package Mac App"; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			return PackageMac_Internal(ProcessManager, InProfile);
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			Session.Current.SessionLog.AddLine(Color.DarkMagenta, "PackageMac does not support Clean and Execute.");
			return true;
		}

		bool PackageMac_Internal(IProcessManager ProcessManager, Profile InProfile)
		{
			ConsoleInterface.TOCSettings BuildSettings = Pipeline.Sync.CreateTOCSettings(InProfile, false, Profile.Languages.All);
			if (BuildSettings == null)
			{
				return false;
			}

			StringBuilder CommandLine = new StringBuilder();
			String CWD = "";
			String ExecutablePath = "";

			// use the directory that this local branch is in, without drive information (strip off X:\ from X:\dev\UnrealEngine3)
			string SyncSubDir = Path.GetFullPath("..\\").Substring(3).Replace("\\", "/");

			// if the directory we got has any backslashes, then assume it's a "CookerSync" copy, instead of packaging it up,
			// so let the 
			if (!SyncSubDir.Contains("\\"))
			{
				string PlatformName = "Mac";
				// The name of the directory needs to match where DevStudio outputs the executable - and it uses Shipping, not FR
				string ConfigurationName = GetMacConfigurationString(InProfile.LaunchConfiguration);

				ExecutablePath = Path.GetFullPath(PlatformName + "\\MacPackager.exe");

				// Get all languages that need to be cooked
				String[] Languages = GetLanguagesToCookAndSync(InProfile, Profile.Languages.All);

				// INT is always cooked.
				String LanguageCookString = "INT";

				foreach (String Language in Languages)
				{
					if (Language != "INT")
					{
						// Add the language if its not INT.  INT is already added to the string
						LanguageCookString += "+" + Language;
					}
				}

				string PackageMode = (InProfile.Mac_PackagingMode == EMacPackageMode.Normal) ? "PackageApp" : "PackageMAS";
				CommandLine.AppendFormat(" {0} {1} {2} {3}", PackageMode, BuildSettings.GameName, ConfigurationName, LanguageCookString);

				CWD = Path.GetFullPath(PlatformName);

				return ProcessManager.StartProcess(ExecutablePath, CommandLine.ToString(), CWD, InProfile.TargetPlatform);
			}

			return false;
		}

		public static string GetMacConfigurationString(Profile.Configuration Config)
		{
			string RetVal = string.Empty;

			switch (Config)
			{
				case Profile.Configuration.Debug_64:
					{
						RetVal = "Debug";
						break;
					}
				case Profile.Configuration.Release_64:
					{
						RetVal = "Release";
						break;
					}
				case Profile.Configuration.Shipping_64:
					{
						RetVal = "Shipping";
						break;
					}
				case Profile.Configuration.Test_64:
					{
						RetVal = "Test";
						break;
					}
			}

			return RetVal;
		}
	}
}
