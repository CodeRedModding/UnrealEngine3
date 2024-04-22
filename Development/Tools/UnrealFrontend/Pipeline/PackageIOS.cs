/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using Color = System.Drawing.Color;
using System.IO;

namespace UnrealFrontend.Pipeline
{
	public enum EPackageMode
	{
		FastIterationAndSign,
		DistributionAndSign,
		XCodeDebugging,
		Distribution,
		DistributionPathway,		
		FastIteration,
		IterationWithSmallSize,
	}

	public class PackageIOS : Pipeline.CommandletStep
	{
		public static String EPackageMode_ToFriendlyName(EPackageMode InPackageMode)
		{
			switch (InPackageMode)
			{
				default: return InPackageMode.ToString();
				case EPackageMode.FastIterationAndSign: return "Default";
				case EPackageMode.DistributionAndSign: return "Distribution";
				case EPackageMode.XCodeDebugging: return "XCode Debugging";
				case EPackageMode.Distribution: return "Distribution Using Mac";
				case EPackageMode.DistributionPathway: return "Development Using Mac";
				case EPackageMode.FastIteration: return "Development (no resigning)";
				case EPackageMode.IterationWithSmallSize: return "Development Compressed (no resigning)";
			}
		}

		public static List<EPackageMode> GetValidPackageModes()
		{
			List<EPackageMode> AvailablePackageModes = new List<EPackageMode>
			{
				EPackageMode.FastIterationAndSign,
				EPackageMode.DistributionAndSign,
			};

			if (Session.Current.IsRPCUtilityFound)
			{
				AvailablePackageModes.Add(EPackageMode.XCodeDebugging);
				AvailablePackageModes.Add(EPackageMode.Distribution);
				AvailablePackageModes.Add(EPackageMode.DistributionPathway);
			}

			if (Session.Current.UDKMode == EUDKMode.None)
			{
				AvailablePackageModes.Add(EPackageMode.FastIteration);
				AvailablePackageModes.Add(EPackageMode.IterationWithSmallSize);
			}				

			return AvailablePackageModes;
		}


		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Package"; } }

		public override String StepNameToolTip { get { return "Package iOS app."; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Package iOS App"; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			return PackageIOS_Internal( ProcessManager, InProfile );
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			Session.Current.SessionLog.AddLine(Color.DarkMagenta, "PackageIOS does not support Clean and Execute.");
			return true;
		}

		bool PackageIOS_Internal(IProcessManager ProcessManager, Profile InProfile)
		{
			ConsoleInterface.TOCSettings BuildSettings = Pipeline.Sync.CreateTOCSettings(InProfile, false, Profile.Languages.All);
			if( BuildSettings == null )
			{
				return false;
			}

			StringBuilder CommandLine = new StringBuilder();
			String CWD = "";
			String ExecutablePath = "";

			ConsoleInterface.Platform CurPlat = InProfile.TargetPlatform;
			if (!Pipeline.Sync.UpdateMobileCommandlineFile(InProfile))
			{
				return false;
			}

			// use the directory that this local branch is in, without drive information (strip off X:\ from X:\dev\UnrealEngine3)
			string SyncSubDir = Path.GetFullPath("..\\").Substring(3).Replace("\\", "/");

			// if the directory we got has any backslashes, then assume it's a "CookerSync" copy, instead of packaging it up,
			// so let the 
			if (!SyncSubDir.Contains("\\"))
			{
				string PlatformName = CurPlat.Type.ToString();
				// The name of the directory needs to match where DevStudio outputs the executable - and it uses Shipping, not FR
				string ConfigurationName = GetMobileConfigurationString(InProfile.LaunchConfiguration);

				ExecutablePath = Path.GetFullPath(PlatformName + "\\iPhonePackager.exe");

				string ExtraFlags = "-interactive ";

				switch (InProfile.Mobile_PackagingMode)
				{

					default:
					case EPackageMode.FastIteration:
						CommandLine.Append("RepackageIPA");
						ExtraFlags += "-compress=none";
						break;
					case EPackageMode.IterationWithSmallSize:
						CommandLine.Append("RepackageIPA");
						ExtraFlags += "-compress=fast";
						break;
					case EPackageMode.XCodeDebugging:
						CommandLine.Append("PackageApp");
						break;
					case EPackageMode.Distribution:
						CommandLine.Append("PackageIPA");
						ExtraFlags += "-compress=best -distribution";
						break;
					case EPackageMode.DistributionPathway:
						CommandLine.Append("PackageIPA");
						ExtraFlags += "-compress=best -strip";
						break;
					case EPackageMode.FastIterationAndSign:
						CommandLine.Append("RepackageIPA");
						ExtraFlags += "-compress=none -sign";
						break;
					case EPackageMode.DistributionAndSign:
						CommandLine.Append("RepackageIPA");
						ExtraFlags += "-compress=best -sign -distribution";
						break;
				}

                // INT is always cooked.
                String LanguageCookString = "-r INT";

                foreach (String Language in BuildSettings.Languages)
                {
                    if (Language != "INT")
                    {
                        // Add the language if its not INT.  INT is already added to the string
                        LanguageCookString += "+" + Language;
                    }
                }

                CommandLine.AppendFormat(" {0} {1} {2} {3}", BuildSettings.GameName, ConfigurationName, ExtraFlags, LanguageCookString);

				if (InProfile.Mobile_UseNetworkFileLoader)
				{
					CommandLine.Append(" -network");
				}

				CWD = Path.GetFullPath(PlatformName);

				return ProcessManager.StartProcess(ExecutablePath, CommandLine.ToString(), CWD, InProfile.TargetPlatform);
			}

			return false;
		}

		public static string GetMobileConfigurationString( Profile.Configuration Config )
		{
			string RetVal = string.Empty;

			switch (Config)
			{
				case Profile.Configuration.Debug_32:
				case Profile.Configuration.Debug_64:
					{
						RetVal = "Debug";
						break;
					}
				case Profile.Configuration.Release_32:
				case Profile.Configuration.Release_64:
					{
						RetVal = "Release";
						break;
					}
				case Profile.Configuration.Shipping_32:
				case Profile.Configuration.Shipping_64:
					{
						RetVal = "Shipping";
						break;
					}
				case Profile.Configuration.Test_32:
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
