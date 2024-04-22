/*
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using System.Windows.Forms;
using Ionic.Zlib;

namespace iPhonePackager
{
    public class Settings
    {
    }

	/**
	 * Operations done at compile time - these may involve the Mac
	 */
	public class CompileTime
	{
        /**
         * Location of the Xcode installation on the Mac.  For example: 
         * "/Applications/Xcode.app/Contents/Developer"
         */
        private static string XcodeDeveloperDir = "";

		private static string iPhone_SigningDevRootMac = "";
		private static string iPhone_SigningDevRootWin = "";

        /// <summary>
        /// The file name (no path) of the temporary mobile provision that will be placed on the remote mac for use in makeapp
        /// </summary>
        private static string MacMobileProvisionFilename;

        private static string MacName = "";
        private static string MacStagingRootDir = "";
		private static string MacXcodeStagingDir = "";

        /** /MacStagingRootDir/Payload/GameName.app */
        protected static string RemoteAppDirectory
        {
			get { return MacStagingRootDir + "/Payload/" + Program.GameName + ".app"; }
        }

		/** /MacStagingRootDir/Payload */
		public static string RemoteAppPayloadDirectory
		{
			get { return MacStagingRootDir + "/Payload"; }
		}

        /** /MacStagingRootDir/Payload/GameName.app/GameName */
        protected static string RemoteExecutablePath
        {
			get { return RemoteAppDirectory + "/" + Program.GameName; }
        }

		private static string CurrentBaseXCodeCommandLine;

		/**
		 * @return the commandline used to run Xcode (can add commands like "clean" as needed to the result)
		 */
        static private string GetBaseXcodeCommandline()
        {
			string CmdLine = XcodeDeveloperDir + "usr/bin/xcodebuild" +
					" -project UE3.xcodeproj" + 
					" -configuration " + Program.GameConfiguration +
					" -target " + Program.GameName +
					" -sdk iphoneos";

            // sign with the Distribution identity when packaging for distribution
            if (!Config.bForDistribution)
            {
                CmdLine += String.Format(" CODE_SIGN_RESOURCE_RULES_PATH={0}/CustomResourceRules.plist", MacStagingRootDir);
            }
            CmdLine += String.Format(" CODE_SIGN_IDENTITY=\\\"{0}\\\"", Config.CodeSigningIdentity);

            return CmdLine;
        }

		/** 
		 * Handle the plethora of environment variables required to remote to the Mac
		 */
		static public void ConfigurePaths()
		{
            Config.Initialize();
            
            string MachineName = Environment.MachineName;
			string CurrentFolder = Environment.CurrentDirectory;

            XcodeDeveloperDir = Utilities.GetEnvironmentVariable("ue3.XcodeDeveloperDir", "/Applications/Xcode.app/Contents/Developer/");

			// MacName=%ue3.iPhone_SigningServerName%
			MacName = Utilities.GetEnvironmentVariable( "ue3.iPhone_SigningServerName", "a1487" );
			iPhone_SigningDevRootWin = "\\\\" + MacName + "\\UnrealEngine3\\Builds";
			iPhone_SigningDevRootMac = "/UnrealEngine3/Builds";

			// get the path to mirror into on the Mac
			string Root = Path.GetPathRoot( CurrentFolder );
			string BranchPath = MachineName + "/" + CurrentFolder.Substring( Root.Length );
			BranchPath = BranchPath.Replace('\\', '/');

			// generate the directories to recursively copy into later on
			MacStagingRootDir = string.Format("{0}/{1}/{2}-IPhone/{3}", iPhone_SigningDevRootMac, BranchPath, Program.GameName, Program.GameConfiguration);
			MacXcodeStagingDir = string.Format("{0}/{1}/UE3-Xcode", iPhone_SigningDevRootMac, BranchPath);

            MacMobileProvisionFilename = MachineName + "_UE3Temp.mobileprovision";

			CurrentBaseXCodeCommandLine = GetBaseXcodeCommandline();
		}

        /// <summary>
        /// Logs out a line of stdout or stderr from RPCUtility.exe to the program log
        /// </summary>
        /// <param name="Sender"></param>
        /// <param name="Line"></param>
        static public void OutputReceivedRemoteProcessCall(Object Sender, DataReceivedEventArgs Line)
        {
            if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
            {
                Program.Log("[RPC] " + Line.Data);

                if (Line.Data.Contains("** BUILD FAILED **"))
                {
                    Program.Error("Xcode build failed!");
                }
            }
        }

        /// <summary>
        /// Copy the files always needed (even in a stub IPA)
        /// </summary>
        static public void CopyFilesNeededForMakeApp()
        {
            // Copy Info.plist over (modifiying it as needed)
            string SourcePListFilename = Utilities.GetPrecompileSourcePListFilename();
            Utilities.PListHelper Info = Utilities.PListHelper.CreateFromFile(SourcePListFilename);
            
            // Edit the plist
            CookTime.UpdateVersionAndApplyOverrides(Info);

            // Write out the <GameName>-Info.plist file to the xcode staging directory
            string TargetPListFilename = Path.Combine(Config.PCXcodeStagingDir, Program.GameName + "-Info.plist");
			Directory.CreateDirectory(Path.GetDirectoryName(TargetPListFilename));
            byte[] RawInfoPList = Encoding.UTF8.GetBytes(Info.SaveToString());
            File.WriteAllBytes(TargetPListFilename, RawInfoPList);

			// look for an entitlements file (optional)
			string SourceEntitlements = FileOperations.FindPrefixedFile(Config.BuildDirectory, Program.GameName + ".entitlements");
			
			// set where to make the entitlements file (
			string TargetEntitlements = Path.Combine(Config.PCXcodeStagingDir, Program.GameName + ".entitlements");
			if (File.Exists(SourceEntitlements))
			{
				FileOperations.CopyRequiredFile(SourceEntitlements, TargetEntitlements);
			}
			else
			{
				// we need to have something so Xcode will compile, so we just set the get-task-allow, since we know the value, 
				// which is based on distribution or not (true means debuggable)
				File.WriteAllText(TargetEntitlements, string.Format("<plist><dict><key>get-task-allow</key><{0}/></dict></plist>",
					Config.bForDistribution ? "false" : "true"));
			}
			
			
			// Copy the no sign resource rules file over
            if (!Config.bForDistribution)
            {
                FileOperations.CopyRequiredFile(@"..\..\Development\Src\IPhone\Resources\CustomResourceRules.plist", Path.Combine(Config.PCStagingRootDir, "CustomResourceRules.plist"));
            }

            // Copy the mobile provision file over
            string ProvisionWithPrefix = FileOperations.FindPrefixedFile(Config.BuildDirectory, Program.GameName + ".mobileprovision");
			string FinalMobileProvisionFilename = Path.Combine(Config.PCXcodeStagingDir, MacMobileProvisionFilename);
			FileOperations.CopyRequiredFile(ProvisionWithPrefix, FinalMobileProvisionFilename);
			// make sure this .mobileprovision file is newer than any other .mobileprovision file on the Mac (this file gets multiple games named the same file, 
			// so the time stamp checking can fail when moving between games, a la the buildmachines!)
			File.SetLastWriteTime(FinalMobileProvisionFilename, DateTime.UtcNow);

			FileOperations.CopyRequiredFile(@"UE3.xcodeproj\project.pbxproj", Path.Combine(Config.PCXcodeStagingDir, @"UE3.xcodeproj\project.pbxproj.datecheck"));
			
			// needs Mac line endings so it can be executed
			string SrcPath = @"UE3-Xcode\prepackage.sh";
			string DestPath = Path.Combine(Config.PCXcodeStagingDir, @"prepackage.sh");
			Program.Log(" ... '" + SrcPath + "' -> '" + DestPath + "'");
			string SHContents = File.ReadAllText(SrcPath);
			SHContents = SHContents.Replace("\r\n", "\n");
			File.WriteAllText(DestPath, SHContents);

			CookTime.CopySignedFiles();
        }
        
        /**
         * Handle spawning of the RPCUtility with parameters
         */
		public static bool RunRPCUtilty( string RPCCommand )
		{
			string CommandLine = "";
			string WorkingFolder = "";
			string DisplayCommandLine = "";
			string DisplayMacStagingRootDir = MacStagingRootDir;

			Program.Log( "Running RPC on " + MacName + " ... " );

			if( DisplayMacStagingRootDir.StartsWith( iPhone_SigningDevRootMac ) )
			{
				DisplayMacStagingRootDir = MacStagingRootDir.Substring( iPhone_SigningDevRootMac.Length );
			}

			switch (RPCCommand.ToLowerInvariant())
			{
            case "clean":
				Program.Log(" ... cleaning Xcode");
				DisplayCommandLine = CurrentBaseXCodeCommandLine + " clean";
				CommandLine = MacStagingRootDir + "/../.. " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir + "/../..";
				break;

            case "deletemacstagingfiles":
				Program.Log( " ... deleting staging files on the Mac" );
				DisplayCommandLine = "rm -rf Payload";
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir;
				break;

            case "ensureprovisiondirexists":
                Program.Log(" ... creating provisioning profiles directory");

                DisplayCommandLine = String.Format("mkdir -p ~/Library/MobileDevice/Provisioning\\ Profiles");

                CommandLine = MacStagingRootDir + "/../../UE3-Xcode " + DisplayCommandLine;
                WorkingFolder = DisplayMacStagingRootDir + "/../../UE3-Xcode";
                break;

            case "installprovision":
                // Note: The provision must have already been copied over to the Mac
				Program.Log(" ... installing .mobileprovision");

                DisplayCommandLine = String.Format("cp -f {0} ~/Library/MobileDevice/Provisioning\\ Profiles",  MacMobileProvisionFilename);


				CommandLine = MacStagingRootDir + "/../../UE3-Xcode " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir + "/../../UE3-Xcode";
				break;

            case "removeprovision":
                Program.Log(" ... removing .mobileprovision");
                DisplayCommandLine = String.Format("rm -f ~/Library/MobileDevice/Provisioning\\ Profiles/{0}", MacMobileProvisionFilename);
                CommandLine = MacStagingRootDir + "/../../UE3-Xcode " + DisplayCommandLine;
                WorkingFolder = DisplayMacStagingRootDir + "/../../UE3-Xcode";
                break;

			case "setexec": 
                // Note: The executable must have already been copied over
                Program.Log(" ... setting executable bit");
                DisplayCommandLine = "chmod a+x " + RemoteExecutablePath;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir;
				break;

			case "prepackage":
				Program.Log(" ... running prepackage script remotely ");
				DisplayCommandLine = String.Format("sh prepackage.sh {0} IPhone {1}", Program.GameName, Program.GameConfiguration);
				CommandLine = MacStagingRootDir + "/../../UE3-Xcode " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir + "/../../UE3-Xcode";
				break;

			case "makeapp":
				Program.Log(" ... making application (codesign, etc...)");
                Program.Log("  Using signing identity '{0}'", Config.CodeSigningIdentity);
				DisplayCommandLine = CurrentBaseXCodeCommandLine;
				CommandLine = MacStagingRootDir + "/../.. " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir + "/../..";
				break;

			case "validation":
				Program.Log( " ... validating distribution package" );
				DisplayCommandLine = XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/usr/bin/Validation " + RemoteAppDirectory;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir;
				break;

            case "deleteipa":
                Program.Log(" ... deleting IPA on Mac");
                DisplayCommandLine = "rm -f " + Config.IPAFilenameOnMac;
                CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
                WorkingFolder = DisplayMacStagingRootDir;
                break;

			case "kill":
				Program.Log( " ... killing" );
				DisplayCommandLine = "killall " + Program.GameName;
				CommandLine = ". " + DisplayCommandLine;
				WorkingFolder = ".";
				break;

			case "strip":
				Program.Log( " ... stripping" );
                DisplayCommandLine = XcodeDeveloperDir + "Platforms/iPhoneOS.platform/Developer/usr/bin/strip " + RemoteExecutablePath;
				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
                WorkingFolder = DisplayMacStagingRootDir;
				break;

            case "resign":
                Program.Log("... resigning");
                DisplayCommandLine = "bash -c '" + "chmod a+x ResignScript" + ";" + "./ResignScript" + "'";
                CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
                WorkingFolder = DisplayMacStagingRootDir;
                break;

			case "zip":
				Program.Log( " ... zipping" );

				// NOTE: -y preserves symbolic links which is needed for iOS distro builds
                // -x excludes a file (excluding the dSYM keeps sizes smaller, and it shouldn't be in the IPA anyways)
				string dSYMName = "Payload/" + Program.GameName + ".app.dSYM";
				DisplayCommandLine = String.Format("zip -r -y -{0} -T {1} Payload iTunesArtwork -x {2}/ -x {2}/* " +
					"-x {2}/Contents/ -x {2}/Contents/* -x {2}/Contents/Resources/ -x {2}/Contents/Resources/* " +
					" -x {2}/Contents/Resources/DWARF/ -x {2}/Contents/Resources/DWARF/*",
					(int)Config.RecompressionSetting,
					Config.IPAFilenameOnMac,
					dSYMName);

				CommandLine = MacStagingRootDir + " " + DisplayCommandLine;
				WorkingFolder = DisplayMacStagingRootDir;
				break;

			default:
				Program.Error( "Unrecognized RPC command" );
				return ( false );
			}

			Program.Log( " ... working folder: " + WorkingFolder );
			Program.Log( " ... " + DisplayCommandLine );

			Process RPCUtil = new Process();
			RPCUtil.StartInfo.FileName = "..\\RPCUtility.exe";
			RPCUtil.StartInfo.UseShellExecute = false;
			RPCUtil.StartInfo.Arguments = MacName + " " + CommandLine;
            RPCUtil.StartInfo.RedirectStandardOutput = true;
            RPCUtil.StartInfo.RedirectStandardError = true;
            RPCUtil.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedRemoteProcessCall);
            RPCUtil.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedRemoteProcessCall);

            RPCUtil.Start();
            
            RPCUtil.BeginOutputReadLine();
            RPCUtil.BeginErrorReadLine();

            RPCUtil.WaitForExit();

            if (RPCUtil.ExitCode != 0)
            {
                Program.Error("RPCCommand {0} failed with return code {1}", RPCCommand, RPCUtil.ExitCode);
                switch (RPCCommand.ToLowerInvariant())
                {
                    case "installprovision":
                        Program.Error("Ensure your access permissions for '~/Library/MobileDevice/Provisioning Profiles' are set correctly.");
                        break;
                    default:
                        break;
                }
            }

            return (RPCUtil.ExitCode == 0);
		}


        /** 
         * Creates the application directory on the Mac
         */
        static public void CreateApplicationDirOnMac()
        {
            DateTime StartTime = DateTime.Now;

            // Cleans out the intermediate folders on both ends
            CompileTime.ExecuteRemoteCommand("DeleteIPA");
            CompileTime.ExecuteRemoteCommand("DeleteMacStagingFiles");
            Program.ExecuteCommand("Clean", null);
            //@TODO: mnoland 10/5/2010
            // Need to come up with a way to prevent this from causing an error on the remote machine
            // CompileTime.ExecuteRemoteCommand("Clean");

            // Stage files
            Program.Log("Staging files before copying to Mac ...");
            if (!Config.bUseStubFileSet)
            {
                CookTime.CopyCommonFiles();
                if (Config.bForNetwork)
                {
                    CookTime.CopyNetworkFiles();
                }
                else
                {
                    CookTime.CopyPackageFiles();
                }
            }
            CopyFilesNeededForMakeApp();

            // Copy staged files from PC to Mac
            Program.ExecuteCommand("StageMacFiles", null);

            // Set the executable bit on the EXE
            CompileTime.ExecuteRemoteCommand("SetExec");

            // Install the provision (necessary for MakeApp to succeed)
            CompileTime.ExecuteRemoteCommand("EnsureProvisionDirExists");
            CompileTime.ExecuteRemoteCommand("InstallProvision");

            // strip the symbols if desired or required
            if (Config.bForceStripSymbols || Config.bForDistribution)
            {
                CompileTime.ExecuteRemoteCommand("Strip");
            }

            // sign the exe, etc...
			CompileTime.ExecuteRemoteCommand("PrePackage");
			CompileTime.ExecuteRemoteCommand("MakeApp");

            Program.Log(String.Format("Finished creating .app directory on Mac (took {0:0.00} s)",
                (DateTime.Now - StartTime).TotalSeconds));
        }
        
        /** 
         * Packages an IPA on the Mac
         */
        static public void PackageIPAOnMac()
        {
            // Create the .app structure on the Mac (and codesign, etc...)
            CreateApplicationDirOnMac();

            DateTime StartTime = DateTime.Now;

            // zip up
            CompileTime.ExecuteRemoteCommand("Zip");

            // fetch the IPA
            if (Config.bUseStubFileSet)
            {
                Program.ExecuteCommand("GetStubIPA", null);
            }
            else
            {
                Program.ExecuteCommand("GetIPA", null);
            }

            Program.Log(String.Format("Finished packaging into IPA (took {0:0.00} s)",
                (DateTime.Now - StartTime).TotalSeconds));
        }

        static public void ExecuteRemoteCommand(string RemoteCommand)
        {
            Program.Log("Running RPC on " + MacName + " ... ");
            RunRPCUtilty(RemoteCommand);
        }

        static public bool ExecuteCompileCommand(string Command, string RPCCommand)
        {
            switch (Command.ToLowerInvariant())
            {
                case "clean":
                    Program.Log("Cleaning temporary files from PC ... ");
                    Program.Log(" ... cleaning: " + Config.PCStagingRootDir);
                    FileOperations.DeleteDirectory(new DirectoryInfo(Config.PCStagingRootDir));
                    break;
 
                case "rpc":
                    ExecuteRemoteCommand(RPCCommand);
                    break;

                case "getipa":
                    {
                        Program.Log("Fetching IPA from Mac...");

                        string IpaDestFilename = Config.GetIPAPath(".ipa");
						FileOperations.DownloadFile(MacName, MacStagingRootDir + "/" + Config.IPAFilenameOnMac, IpaDestFilename);

                        Program.Log("... Saved IPA to '{0}'", Path.GetFullPath(IpaDestFilename));
                    }
                    break;

                case "getstubipa":
                    {
                        Program.Log("Fetching stub IPA from Mac...");

                        string IpaDestFilename = Config.GetIPAPath(".stub");
                        FileOperations.DownloadFile(MacName, MacStagingRootDir + "/" + Config.IPAFilenameOnMac, IpaDestFilename);

                        Program.Log("... Saved IPA to '{0}'", Path.GetFullPath(IpaDestFilename));
                    }
                    break;

                case "stagemacfiles":
                    Program.Log("Copying all staged files to Mac " + MacName + " ...");
					FileOperations.BatchUploadFolder(MacName, Config.PCStagingRootDir, MacStagingRootDir, false);
					FileOperations.BatchUploadFolder(MacName, Config.PCXcodeStagingDir, MacXcodeStagingDir, false);
					break;

                default:
                    return false;
            }

            return true;
        }
	}
}
