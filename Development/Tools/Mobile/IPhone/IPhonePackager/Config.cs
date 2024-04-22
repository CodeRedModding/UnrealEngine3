/*
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace iPhonePackager
{
    internal class Config
    {
        /// <summary>
        /// The display name in title bars, popups, etc...
        /// </summary>
        public static string AppDisplayName = "Unreal iOS Configuration";

        /// <summary>
        /// Optional Prefix to append to .xcent and .mobileprovision files, for handling multiple certificates on same source game
        /// </summary>
        public static string SigningPrefix = "";

        public static string PCStagingRootDir = "";

        /// <summary>
        /// The local staging directory for files needed by Xcode on the Mac (on PC)
        /// </summary>
        public static string PCXcodeStagingDir = "";
        
        /// <summary>
        /// The local build directory (on PC)
        /// </summary>
        public static string BuildDirectory
        {
            get { return Path.GetFullPath(@"..\..\" + Program.GameName + @"\Build\IPhone"); }
        }

		/// <summary>
		/// The local directory where the game lives (on PC)
		/// </summary>
		public static string GameDirectory
		{
			get { return Path.GetFullPath(@"..\..\" + Program.GameName); }
		}

		/// <summary>
        /// The local directory cooked files are placed (on PC)
        /// </summary>
        public static string CookedDirectory
        {
            get { return Path.GetFullPath(@"..\..\" + Program.GameName + @"\CookedIPhone"); }
        }

        /// <summary>
        /// The local directory that a payload (GameName.app) is assembled into before being copied to the Mac (on PC)
        /// </summary>
        public static string PayloadDirectory
        {
			get { return Path.GetFullPath(PCStagingRootDir + @"\Payload\" + Program.GameName + ".app"); }
        }

		/// <summary>
		/// The local directory that a payload (GameName.app) is assembled into before being copied to the Mac (on PC)
		/// </summary>
		public static string PayloadRootDirectory
		{
			get { return Path.GetFullPath(PCStagingRootDir + @"\Payload"); }
		}

		/// <summary>
        /// The local binaries directory (device and configuration specific) (on PC) 
        /// </summary>
        public static string DeviceConfigSpecificBinariesDirectory
        {
            get { return Path.GetFullPath("."); }
        }

        /// <summary>
        /// Returns the filename for the IPA (no path, just filename)
        /// </summary>
        public static string IPAFilenameOnMac
        {
			get { return SigningPrefix + Program.GameName + "-IPhone-" + Program.GameConfiguration + ".ipa"; }
        }

        /// <summary>
        /// Returns the name of the file containing user overrides that will be applied when packaging on PC
        /// </summary>
        public static string GetPlistOverrideFilename()
        {
            return GetPlistOverrideFilename(false);
        }

        public static string GetPlistOverrideFilename(bool bWantDistributionOverlay)
        {
            string Folder = String.Format(@"..\..\{0}\Build\IPhone", Program.GameName);

            string Prefix = "";
            if (bWantDistributionOverlay)
            {
                Prefix = "Distro_";
            }

            string Filename = Path.Combine(Folder, Prefix + Program.GameName + "Overrides.plist");

            return Filename;
        }

        /// <summary>
        /// Returns the full path for either the stub or final IPA on the PC
        /// </summary>
        public static string GetIPAPath(string FileSuffix)
        {
            // Quash the default Epic_ so that stubs for UDK installers get named correctly and can be used
            string FilePrefix = (SigningPrefix == "Epic_") ? "" : SigningPrefix;

			string Filename = Path.Combine( Config.DeviceConfigSpecificBinariesDirectory, FilePrefix + Program.GameName + "-IPhone-" + Program.GameConfiguration + FileSuffix );
            return Filename;
        }

        /// <summary>
        /// Returns the full path for the stub IPA, following the signing prefix resolution rules
        /// </summary>
        public static string GetIPAPathForReading(string FileSuffix)
        {
			return FileOperations.FindPrefixedFile( Config.DeviceConfigSpecificBinariesDirectory, Program.GameName + "-IPhone-" + Program.GameConfiguration + FileSuffix );
        }

        /// <summary>
        /// Whether or not to allow interactive dialogs to pop up when a traditionally non-interactive command was specified
        /// (e.g., can we pop up a configuration dialog during PackageIPA?)
        /// </summary>
        public static bool bAllowInteractiveDialogsDuringNonInteractiveCommands = false;

		/// <summary>
        /// Whether or not to output extra information (like every file copy and date/time stamp)
        /// </summary>
        public static bool bVerbose = true;

        /// <summary>
        /// Whether or not to output extra information in code signing
        /// </summary>
        public static bool bCodeSignVerbose = false;

        /// <summary>
        /// Whether or not the wifi network will be used to provide files to the resulting run (if true, cooked content isn't deployed)
        /// </summary>
        public static bool bForNetwork = false;

        /// <summary>
        /// Whether or not non-critical files will be packaged (critical == required for signing or .app validation, the app may still fail to
        /// run with only 'critical' files present).  Also affects the name and location of the IPA back on the PC
        /// </summary>
        public static bool bUseStubFileSet = false;

        /// <summary>
        /// Is this a distribution packaging build?  Controls a number of aspects of packing (which signing prefix and provisioning profile to use, etc...)
        /// </summary>
        public static bool bForDistribution = false;

        /// <summary>
        /// Whether or not to strip symbols (they will always be stripped when packaging for distribution)
        /// </summary>
        public static bool bForceStripSymbols = false;

        /// <summary>
        /// Do a code signing update when repackaging?
        /// </summary>
        public static bool bPerformResignWhenRepackaging = false;

        /// <summary>
        /// Whether to use None or Best for the compression setting when repackaging IPAs
        /// </summary>
        public static Ionic.Zlib.CompressionLevel RecompressionSetting = Ionic.Zlib.CompressionLevel.None;

        /// <summary>
        /// Returns the application directory inside the zip
        /// </summary>
        public static string AppDirectoryInZIP
        {
			get { return String.Format("Payload/{0}.app", Program.GameName); }
        }

        /// <summary>
        /// If the requirements blob is present in the existing executable when code signing, should it be carried forward
        /// (true) or should a dummy requirements blob be created with no actual requirements (false)
        /// </summary>
        public static bool bMaintainExistingRequirementsWhenCodeSigning = false;

        /// <summary>
        /// The code signing identity to use when signing via RPC
        /// </summary>
        public static string CodeSigningIdentity;

        /// <summary>
        /// Returns a path to the place to back up documents from a device
        /// </summary>
        /// <returns></returns>
        public static string GetRootBackedUpDocumentsDirectory()
        {
			return Path.GetFullPath( @"..\..\" + Program.GameName + "-IPhone-" + Program.GameConfiguration + @"\iOS_Backups" );
        }

        public static void Initialize()
        {
            bool bIsEpicInternal = File.Exists("..\\EpicInternal.txt");

            // Root directory on PC for staging files to copy to Mac
            Config.PCStagingRootDir = String.Format(@"..\..\Development\Intermediate\IPhone-Deploy\{0}-IPhone\{1}",
                Program.GameName,
				Program.GameConfiguration);

            // make a directory for the shared UE3-Xcode directory
            Config.PCXcodeStagingDir = Config.PCStagingRootDir + "UE3-Xcode";

            // Code signing identity
            // Rules:
            //   An environment variable wins if set
            //   Otherwise for internal development builds, an internal identity is used
            //   Otherwise, developer or distribution are used
            // Distro builds won't succeed on a machine with multiple distro certs installed unless the environment variable is set.
            Config.CodeSigningIdentity = Config.bForDistribution ? "iPhone Distribution" : "iPhone Developer";
            if (bIsEpicInternal && !Config.bForDistribution)
            {
                Config.CodeSigningIdentity = "iPhone Developer: Mike Capps (E6";
            }

            if (Config.bForDistribution)
            {
                Config.CodeSigningIdentity = Utilities.GetEnvironmentVariable("ue3.iPhone_DistributionSigningIdentity", Config.CodeSigningIdentity);
            }
            else
            {
                Config.CodeSigningIdentity = Utilities.GetEnvironmentVariable("ue3.iPhone_DeveloperSigningIdentity", Config.CodeSigningIdentity);
            }

			// look for the signing prefix environment variable
            string DefaultPrefix = "";
            if (bIsEpicInternal)
            {
                // default Epic to "Epic_" prefix
                DefaultPrefix = "Epic_";
            }
            
            if (Config.bForDistribution)
			{
                DefaultPrefix = "Distro_";
            }
            Config.SigningPrefix = Utilities.GetEnvironmentVariable("ue3.iPhone_SigningPrefix", DefaultPrefix);

			// Windows doesn't allow environment vars to be set to blank so detect "none" and treat it as such
			if (Config.SigningPrefix == "none")
			{
				Config.SigningPrefix = Config.bForDistribution ? "Distro_" : "";
			}
        }
    }
}
