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
    public class PackageAndroid : Pipeline.CommandletStep
    {
        public override bool SupportsClean { get { return false; } }

        public override string StepName { get { return "Package"; } }

        public override String StepNameToolTip { get { return "Package Android app."; } }

        public override bool SupportsReboot { get { return false; } }

        public override String ExecuteDesc { get { return "Package Android App"; } }

        public override bool Execute(IProcessManager ProcessManager, Profile InProfile)
        {
            return PackageAndroid_Internal(ProcessManager, InProfile);
        }

        public override bool CleanAndExecute(IProcessManager ProcessManager, Profile InProfile)
        {
            Session.Current.SessionLog.AddLine(Color.DarkMagenta, "PackageAndroid does not support Clean and Execute.");
            return true;
        }

        bool PackageAndroid_Internal(IProcessManager ProcessManager, Profile InProfile)
        {
            bool bSuccess = true;
            string CommandArgs = "";

            bSuccess = bSuccess && ProcessManager.WaitForActiveProcessesToComplete();

            // Update commandline for packaging
            bSuccess = bSuccess && Pipeline.Sync.UpdateMobileCommandlineFile(InProfile);

            // Collect cooked files into a package if set to Android distribution
            if (InProfile.IsAndroidDistribution)
            {
                // Generate TOC file for all data
                ConsoleInterface.TOCSettings BuildSettings = Pipeline.Sync.CreateTOCSettings(InProfile, false, Profile.Languages.All);
                if (bSuccess && BuildSettings != null)
                {
                    BuildSettings.NoSync = true; // We don't want to actually cause a sync

                    string TagSet = InProfile.SupportsPDBCopy && InProfile.Sync_CopyDebugInfo ? "ConsoleSync" : "ConsoleSyncProgrammer";
                    string CookerSyncCommandLine = Sync.GenerateCookerSyncCommandLine(InProfile, BuildSettings, TagSet, false, false);
                    bSuccess = ProcessManager.StartProcess("CookerSync.exe", CookerSyncCommandLine, "", InProfile.TargetPlatform);

                    // Finish generation
                    bSuccess = bSuccess && ProcessManager.WaitForActiveProcessesToComplete();
                }

                CommandArgs = InProfile.SelectedGameName;

                // Determine any filter extension to add
                switch (InProfile.Android_TextureFilter)
                {
                    case EAndroidTextureFilter.DXT:
                        CommandArgs = CommandArgs + " -appendfilter _DXT";
                        break;
                    case EAndroidTextureFilter.ATITC:
                        CommandArgs = CommandArgs + " -appendfilter _ATITC";
                        break;
                    case EAndroidTextureFilter.PVRTC:
                        CommandArgs = CommandArgs + " -appendfilter _PVRTC";
                        break;
                    case EAndroidTextureFilter.ETC:
                        CommandArgs = CommandArgs + " -appendfilter _ETC";
                        break;
                    case EAndroidTextureFilter.NONE:
                    default:
                        break;
                }

                // Run packager tool
                bSuccess = bSuccess && ProcessManager.StartProcess("Android/AndroidExpansionPackager.exe", CommandArgs, "", InProfile.TargetPlatform);
            }

            // Wait for expansion to package if needed
            bSuccess = bSuccess && ProcessManager.WaitForActiveProcessesToComplete();

            string ConfigurationName = "";
            switch (InProfile.LaunchConfiguration)
            {
                case Profile.Configuration.Debug_32:
                case Profile.Configuration.Debug_64:
                    {
                        ConfigurationName = "Debug";
                        break;
                    }
                case Profile.Configuration.Release_32:
                case Profile.Configuration.Release_64:
                    {
                        ConfigurationName = "Release";
                        break;
                    }
                case Profile.Configuration.Shipping_32:
                case Profile.Configuration.Shipping_64:
                    {
                        ConfigurationName = "Shipping";
                        break;
                    }
                case Profile.Configuration.Test_32:
                case Profile.Configuration.Test_64:
                    {
                        ConfigurationName = "Test";
                        break;
                    }
            }

            string PlatformName = "";
            switch (InProfile.Android_Architecture)
            {
                case EAndroidArchitecture.ARM:
                    PlatformName = "AndroidARM";
                    break;
                case EAndroidArchitecture.x86:
                    PlatformName = "Androidx86";
                    break;
                case EAndroidArchitecture.ALL:
                default:
                    PlatformName = "Android";
                    break;
            }

            // Call packager to create APK
            CommandArgs = InProfile.SelectedGameName + " " + PlatformName + " " + ConfigurationName;
            
            // Determine any filter extension to add
            switch (InProfile.Android_TextureFilter)
            {
                case EAndroidTextureFilter.DXT:
                    CommandArgs = CommandArgs + " -appendfilter _DXT";
                    break;
                case EAndroidTextureFilter.ATITC:
                    CommandArgs = CommandArgs + " -appendfilter _ATITC";
                    break;
                case EAndroidTextureFilter.PVRTC:
                    CommandArgs = CommandArgs + " -appendfilter _PVRTC";
                    break;
                case EAndroidTextureFilter.ETC:
                    CommandArgs = CommandArgs + " -appendfilter _ETC";
                    break;
                case EAndroidTextureFilter.NONE:
                default:
                    break;
            }

            // Add Amazon flag if needed
            if (InProfile.Android_PackagingMode == EAndroidPackageMode.AmazonDistribution)
            {
                CommandArgs = CommandArgs + " -packageForAmazon";
            }

            // Add Google flag if needed
            if (InProfile.Android_PackagingMode == EAndroidPackageMode.GoogleDistribution)
            {
                CommandArgs = CommandArgs + " -packageForGoogle";
            }

            // Run packaging tool
            bSuccess = bSuccess && ProcessManager.StartProcess("Android/AndroidPackager.exe", CommandArgs, "", InProfile.TargetPlatform);

            return true;
        }
    }
}
