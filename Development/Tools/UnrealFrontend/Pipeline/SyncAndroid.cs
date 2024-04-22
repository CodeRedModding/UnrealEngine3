/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using System.IO;
using System.Xml;

namespace UnrealFrontend.Pipeline
{
    public enum EAndroidPackageMode
    {
        Development,
        GoogleDistribution,
        AmazonDistribution,
    }

    public enum EAndroidArchitecture
    {
        ARM,
        x86,
		ALL,
    }

    public enum EAndroidTextureFilter
    {
        NONE,
        DXT,
        ATITC,
        PVRTC,
        ETC,
    }

	public class SyncAndroid : Pipeline.Sync
	{
        public static String EAndroidPackageMode_ToFriendlyName(EAndroidPackageMode InPackageMode)
        {
            switch (InPackageMode)
            {
                default: return InPackageMode.ToString();
                case EAndroidPackageMode.Development: return "Development";
                case EAndroidPackageMode.GoogleDistribution: return "Google Distribution";
                case EAndroidPackageMode.AmazonDistribution: return "Amazon Distribution";
            }
        }

        public static List<EAndroidPackageMode> GetValidPackageModes()
        {
            List<EAndroidPackageMode> AvailablePackageModes = new List<EAndroidPackageMode>
			{
				EAndroidPackageMode.Development,
				EAndroidPackageMode.GoogleDistribution,
                EAndroidPackageMode.AmazonDistribution,
			};

            return AvailablePackageModes;
        }

        public static String EAndroidArchitecture_ToFriendlyName(EAndroidArchitecture InArchitecture)
        {
            switch (InArchitecture)
            {
                default: return InArchitecture.ToString();
                case EAndroidArchitecture.ALL: return "Combined";
                case EAndroidArchitecture.ARM: return "ARM";
                case EAndroidArchitecture.x86: return "x86";
            }
        }
        
        public static List<EAndroidArchitecture> GetValidArchitectures()
        {
            List<EAndroidArchitecture> AvailableArchitectures = new List<EAndroidArchitecture>
			{
                EAndroidArchitecture.ALL,
				EAndroidArchitecture.ARM,
				EAndroidArchitecture.x86,
			};

            return AvailableArchitectures;
        }

        public static String EAndroidTextureFilter_ToFriendlyName(EAndroidTextureFilter InTextureFilter)
        {
            switch (InTextureFilter)
            {
                default: return InTextureFilter.ToString();
                case EAndroidTextureFilter.NONE: return "None";
                case EAndroidTextureFilter.DXT: return "DXT";
                case EAndroidTextureFilter.ATITC: return "ATITC";
                case EAndroidTextureFilter.PVRTC: return "PVRTC";
                case EAndroidTextureFilter.ETC: return "ETC";
            }
        }

        public static List<EAndroidTextureFilter> GetValidTextureFilters()
        {
            List<EAndroidTextureFilter> AvailableTextureFilters = new List<EAndroidTextureFilter>
			{
				EAndroidTextureFilter.NONE,
                EAndroidTextureFilter.DXT,
                EAndroidTextureFilter.ATITC,
                EAndroidTextureFilter.PVRTC,
                EAndroidTextureFilter.ETC,
			};

            return AvailableTextureFilters;
        }

		#region Pipeline.Step
		
		public override bool SupportsClean { get { return false; } }

		public override string StepName { get { return "Sync Android"; } }

		public override String StepNameToolTip { get { return "Sync to Android Device"; } }

		public override bool SupportsReboot { get { return false; } }

		public override String ExecuteDesc { get { return "Sync to Android Device"; } }

		public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
		{
			ConsoleInterface.TOCSettings BuildSettings = Pipeline.Sync.CreateTOCSettings(InProfile, false, Profile.Languages.All);
			if( BuildSettings == null )
			{
				return false;
			}

			// Android doesn't support syncing or launching (yet). But it does need the UE3CommandLine.txt file to be generated.
			if (!Pipeline.Sync.UpdateMobileCommandlineFile(InProfile))
			{
				return false;
			}

            // Only sync OBB files if in distribution mode
            if (InProfile.IsAndroidDistribution)
            {
                BuildSettings.GenerateTOC = true;
				string TagSet = InProfile.SupportsPDBCopy && InProfile.Sync_CopyDebugInfo ? "ConsoleSync" : "ConsoleSyncProgrammer";
				string DistributionCommandLine = GenerateCookerSyncCommandLine( InProfile, BuildSettings, TagSet, false, false );

                // Determine any filter extension to add
                string Filter = "";
                switch (InProfile.Android_TextureFilter)
                {
                    case EAndroidTextureFilter.DXT:
                        Filter = "_DXT";
                        break;
                    case EAndroidTextureFilter.ATITC:
                        Filter = "_ATITC";
                        break;
                    case EAndroidTextureFilter.PVRTC:
                        Filter = "_PVRTC";
                        break;
                    case EAndroidTextureFilter.ETC:
                        Filter = "_ETC";
                        break;
                    case EAndroidTextureFilter.NONE:
                    default:
                        break;
                }

                // Find the manifest file
                string ManifestPath = "..\\" + InProfile.SelectedGameName + "\\Build\\Android\\java\\AndroidManifest" + Filter + ".xml";
                if (!File.Exists(Path.GetFullPath(ManifestPath)))
                {
                    ManifestPath = "..\\Development\\Src\\Android\\java\\AndroidManifest" + Filter + ".xml";
                }

                if (!File.Exists(ManifestPath))
                {
                    return false;
                }

                // Parse manifest to get package name and 
                XmlDocument XmlDoc = new XmlDocument();
                XmlDoc.Load(ManifestPath);

                // Get Element
                XmlNode ManifestNode = XmlDoc["manifest"];
                // Grab version number and package name
                string VersionNumber = ManifestNode.Attributes["android:versionCode"].Value;
                string PackageName = ManifestNode.Attributes["package"].Value;

                // Build OBB name
                string OBBName = "main." + VersionNumber + "." + PackageName + Filter + ".obb";
                string OBBNameNoFilter = "main." + VersionNumber + "." + PackageName + ".obb";

                // override OBB name to AMAZON if Amazon distribution
                if (InProfile.Android_PackagingMode == EAndroidPackageMode.AmazonDistribution)
                {
                    OBBName = "AMAZON";
                }

                DistributionCommandLine += " -var AndroidExpansion=" + OBBName;

                // Include the destination directory as well
                DistributionCommandLine += " -var AndroidExpansionDirectory=" + "\\obb\\" + PackageName + "\\";

                // Include any filter
                DistributionCommandLine += " -var AndroidExpansionDestination=" + OBBNameNoFilter;

				return ProcessManager.StartProcess("CookerSync.exe", DistributionCommandLine, "", InProfile.TargetPlatform);
            }

			return base.Execute(ProcessManager, InProfile);
		}

		public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
		{
			throw new NotImplementedException();
		}

		#endregion
	}
}
