/**
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
using Ionic.Zip;
using Ionic.Zlib;

namespace iPhonePackager
{
	/**
	 * Operations performed done at cook time - there should be no calls to the Mac here
	 */
	public class CookTime
	{
        /// <summary>
        /// List of files being inserted or updated in the Zip
        /// </summary>
        static private HashSet<string> FilesBeingModifiedToPrintOut = new HashSet<string>();
        
        /**
         * Create and open a work IPA file
         */
        static private ZipFile SetupWorkIPA()
        {
            string ReferenceZipPath = Config.GetIPAPathForReading(".stub");
            string WorkIPA = Config.GetIPAPath(".ipa");
            return CreateWorkingIPA(ReferenceZipPath, WorkIPA);
        }
        
        /// <summary>
        /// Creates a copy of a source IPA to a working path and opens it up as a Zip for further modifications
        /// </summary>
        static private ZipFile CreateWorkingIPA(string SourceIPAPath, string WorkIPAPath)
        {
            FileInfo ReferenceInfo = new FileInfo(SourceIPAPath);
            if (!ReferenceInfo.Exists)
            {
                Program.Error(String.Format("Failed to find stub IPA '{0}'", SourceIPAPath));
                return null;
            }
            else
            {
                Program.Log(String.Format("Loaded stub IPA from '{0}' ...", SourceIPAPath));
            }

			// Make sure there are no stale working copies around
            FileOperations.DeleteFile(WorkIPAPath);

			// Create a working copy of the IPA
            File.Copy(SourceIPAPath, WorkIPAPath);

			// Open up the zip file
            ZipFile Stub = ZipFile.Read(WorkIPAPath);

            // Do a few quick spot checks to catch problems that may have occurred earlier
            bool bHasCodeSignature = Stub[Config.AppDirectoryInZIP + "/_CodeSignature/CodeResources"] != null;
            bool bHasMobileProvision = Stub[Config.AppDirectoryInZIP + "/embedded.mobileprovision"] != null;
            if (!bHasCodeSignature || !bHasMobileProvision)
            {
                Program.Error("Stub IPA does not appear to be signed correctly (missing mobileprovision or CodeResources)");
                Program.ReturnCode = 129;
            }

            return Stub;
		}

        /// <summary>
        /// Extracts Info.plist from a Zip
        /// </summary>
        static private string ExtractEmbeddedPList(ZipFile Zip)
        {
            // Extract the existing Info.plist
            string PListPathInZIP = String.Format("{0}/Info.plist", Config.AppDirectoryInZIP);
            if (Zip[PListPathInZIP] == null)
            {
                Program.Error("Failed to find Info.plist in IPA (cannot update plist version)");
                Program.ReturnCode = 142;
                return "";
            }
            else
            {
                // Extract the original into a temporary directory
                string TemporaryName = Path.GetTempFileName();
                FileStream OldFile = File.OpenWrite(TemporaryName);
                Zip[PListPathInZIP].Extract(OldFile);
                OldFile.Close();

                // Read the text and delete the temporary copy
                string PListSource = File.ReadAllText(TemporaryName, Encoding.UTF8);
                File.Delete(TemporaryName);

                return PListSource;
            }
        }

        static public void CopySignedFiles()
        {
            // Copy and un-decorate the binary name
			FileOperations.CopyFiles(Config.DeviceConfigSpecificBinariesDirectory, Config.PayloadDirectory, "<PAYLOADDIR>", Program.GameName + "-IPhone-" + Program.GameConfiguration, null);
			FileOperations.RenameFile(Config.PayloadDirectory, Program.GameName + "-IPhone-" + Program.GameConfiguration, Program.GameName);

            //@TODO: Remove icons (and therefore validation check) from stub creation, so we don't have to include them in the stub
            FileOperations.CopyFiles(Config.BuildDirectory + @"\Resources\Graphics", Config.PayloadDirectory, "<PAYLOADDIR>", "*.png", null);

			FileOperations.CopyNonEssentialFile(
				Path.Combine(Config.DeviceConfigSpecificBinariesDirectory, Program.GameName + "-IPhone-" + Program.GameConfiguration + ".app.dSYM.zip"),
				Path.Combine(Config.PCXcodeStagingDir, Program.GameName + "-IPhone-" + Program.GameConfiguration + ".app.dSYM.zip.datecheck")
			);
        }

        /// <summary>
        /// Copy files always needed that are not in the stub IPA
        /// </summary>
        static public void CopyCommonFiles()
        {
            FileOperations.CopyFiles(Config.BuildDirectory + @"\Resources\Graphics", Config.PayloadDirectory, "<PAYLOADDIR>", "*.png", null);

            FileOperations.CopyFiles(Config.BuildDirectory, Config.PCStagingRootDir, null, "iTunesArtwork", null);
            FileOperations.CopyFiles(Config.BuildDirectory + @"\Resources\Music", Config.PayloadDirectory, "<PAYLOADDIR>", "*.mp3", null);
            FileOperations.CopyFiles(Config.BuildDirectory + @"\Resources\Movies", Config.PayloadDirectory, "<PAYLOADDIR>", "*.*", null);
            //PUT BACK FOR IB2
            ////CHAIR CHANGE TO SUPPORT CHECKED IN SAVES
            //FileOperations.CopyFolder(Config.BuildDirectory + @"\Resources\Saves", Config.PayloadDirectory + @"\FixedSaves", @"<PAYLOADDIR>\FixedSaves", false);
			FileOperations.CopyFolder(@"..\..\Engine\Stats", Config.PayloadDirectory + @"\Engine\Stats", @"<PAYLOADDIR>\Engine\Stats", false);

			string Results;
            Utilities.RunExecutableAndWait(@"..\CookerSync.exe", Program.GameName + " -p iphone -nd" + Program.Languages.ToString(), out Results);

			FileOperations.CopyFiles(Config.GameDirectory, Config.PayloadDirectory, "<PAYLOADDIR>", "IPhoneTOC.txt", null);

			// Copy the appropriate Settings bundle (only ever use Settings.bundle or Distro_Settings.bundle, there is
			// no fallback, because we can't have it at all for distro builds if we want no settings, so we just have no
			// file at all, which means we can't allow a fallback)
			string SourceSettings = Path.Combine(Config.BuildDirectory, Path.Combine(@"Resources\Settings",
				(Config.bForDistribution || Program.GameConfiguration == "Shipping") ? "Distro_Settings.bundle" : "Settings.bundle"));
			if (Directory.Exists(SourceSettings) && Directory.GetFiles(SourceSettings, "*.plist", SearchOption.AllDirectories).Length > 0)
			{
				FileOperations.CopyFolder(SourceSettings, Config.PayloadDirectory + @"\Settings.bundle", @"<PAYLOADDIR>\Settings.bundle", false);
			}
        }

        /// <summary>
        /// Copy the cooked files to be stored in the IPA
        /// </summary>
		static public void CopyPackageFiles()
		{
            // Copy everything in the cooked directory but text files
            FileOperations.CopyFiles(Config.CookedDirectory, Path.Combine(Config.PayloadDirectory, "CookedIPhone"), "<PAYLOADDIR>\\CookedIPhone", "*.*", ".txt");

            // Copy the command line options text file explicitly
            FileOperations.CopyNonEssentialFile(
                Path.Combine(Config.CookedDirectory, "UE3CommandLine.txt"),
                Path.Combine(Config.PayloadDirectory, @"CookedIPhone\UE3CommandLine.txt"));

			// Copy CachedProgramKeys.txt explicitly
			FileOperations.CopyFiles(Config.CookedDirectory, Path.Combine(Config.PayloadDirectory, "CookedIPhone"), "<PAYLOADDIR>\\CookedIPhone", "CachedProgramKeys.txt", null);
            FileOperations.CopyFiles(Config.CookedDirectory, Path.Combine(Config.PayloadDirectory, "CookedIPhone"), "<PAYLOADDIR>\\CookedIPhone", "ShaderGroup_*.txt", null);
		}

		static public void CopyNetworkFiles()
		{
            FileOperations.CopyRequiredFile(
                Path.Combine(Config.CookedDirectory, "UE3NetworkFileHost.txt"),
                Path.Combine(Config.PayloadDirectory, @"CookedIPhone\UE3NetworkFileHost.txt"));
		}


		/**
		 * Callback for setting progress when saving zip file
		 */
		static private void UpdateSaveProgress( object Sender, SaveProgressEventArgs Event )
		{
			if (Event.EventType == ZipProgressEventType.Saving_BeforeWriteEntry)
			{
                if (FilesBeingModifiedToPrintOut.Contains(Event.CurrentEntry.FileName))
                {
                    Program.Log(" ... Packaging '{0}'", Event.CurrentEntry.FileName);
                }
			}
		}

        /// <summary>
        /// Updates the version string and then applies the settings in the user overrides plist
        /// </summary>
        /// <param name="Info"></param>
        public static void UpdateVersionAndApplyOverrides(Utilities.PListHelper Info)
        {
            // Update the minor version number if the current one is older than the version tracker file
            // Assuming that the version will be set explicitly in the overrides file for distribution
            VersionUtilities.UpdateMinorVersion(Info);

            // Mark the type of build (development or distribution)
            Info.SetString("EpicPackagingMode", Config.bForDistribution ? "Distribution" : "Development");

            // Merge in any user overrides that exist
            string UserOverridesPListFilename = Config.GetPlistOverrideFilename(false);
            if (File.Exists(UserOverridesPListFilename))
            {
                // Read the user overrides plist
                string Overrides = File.ReadAllText(UserOverridesPListFilename, Encoding.UTF8);
                Info.MergePlistIn(Overrides);
            }
            else
            {
                Program.Log("Failed to find user overrides plist. Signing application with default bundle identifier, etc...");
            }

            // Check for distribution overrides and merge those in as well
            if (Config.bForDistribution)
            {
                string DistroOverrridesFilename = Config.GetPlistOverrideFilename(true);
                if (File.Exists(DistroOverrridesFilename))
                {
                    string DistroOverrides = File.ReadAllText(DistroOverrridesFilename, Encoding.UTF8);
                    Info.MergePlistIn(DistroOverrides);
                }
            }
        }

        /** 
         * Using the stub IPA previously compiled on the Mac, create a new IPA with assets
         */
        static public void RepackageIPAFromStub()
		{
			DateTime StartTime = DateTime.Now;
			CodeSignatureBuilder CodeSigner = null;

			// Clean the staging directory
			Program.ExecuteCommand( "Clean", null );

			// Create a copy of the IPA so as to not trash the original
			ZipFile Zip = SetupWorkIPA();
			if( Zip == null )
			{
				return;
			}

			string ZipWorkingDir = String.Format( "Payload/{0}.app/", Program.GameName );

			FileOperations.ZipFileSystem FileSystem = new FileOperations.ZipFileSystem( Zip, ZipWorkingDir );

			// Get the name of the exectable file
			string CFBundleExecutable;
			{
				byte[] RawInfoPList = FileSystem.ReadAllBytes( "Info.plist" );
				Utilities.PListHelper Info = new Utilities.PListHelper( Encoding.UTF8.GetString( RawInfoPList ) );

				// Get the name of the executable file
				if( !Info.GetString( "CFBundleExecutable", out CFBundleExecutable ) )
				{
					throw new InvalidDataException( "Info.plist must contain the key CFBundleExecutable" );
				}
			}

			// Tell the file system about the exectuable file name so that we can set correct attributes on
			// the file when zipping it up
			FileSystem.ExecutableFileName = CFBundleExecutable;


			// Prepare for signing if requested
			if( Config.bPerformResignWhenRepackaging )
			{
				// Start the resign process (load the mobileprovision and info.plist, find the cert, etc...)
				CodeSigner = new CodeSignatureBuilder();
				CodeSigner.FileSystem = FileSystem;

				CodeSigner.PrepareForSigning();

				// Merge in any user overrides that exist
				UpdateVersionAndApplyOverrides( CodeSigner.Info );
			}

			// Empty the current staging directory
			FileOperations.DeleteDirectory( new DirectoryInfo( Config.PCStagingRootDir ) );

			// Copy in the resources
			CopyCommonFiles();
			if( Config.bForNetwork )
			{
				CopyNetworkFiles();
			}
			else
			{
				CopyPackageFiles();
			}

			// Save the zip
			Program.Log( "Saving IPA ..." );

			FilesBeingModifiedToPrintOut.Clear();
			Zip.SaveProgress += UpdateSaveProgress;

			Zip.CompressionLevel = Config.RecompressionSetting;

			// Add all of the payload files, replacing existing files in the stub IPA if necessary (should only occur for icons)
			{
				string SourceDir = Path.GetFullPath( Config.PayloadDirectory );
				string[] PayloadFiles = Directory.GetFiles( SourceDir, "*.*", SearchOption.AllDirectories );
				foreach( string Filename in PayloadFiles )
				{
					// Get the relative path to the file (this implementation only works because we know the files are all
					// deeper than the base dir, since they were generated from a search)
					string AbsoluteFilename = Path.GetFullPath( Filename );
					string RelativeFilename = AbsoluteFilename.Substring( SourceDir.Length + 1 ).Replace( '\\', '/' );

					string ZipAbsolutePath = String.Format( "Payload/{0}.app/{1}",
						Program.GameName,
						RelativeFilename );

					byte[] FileContents = File.ReadAllBytes( AbsoluteFilename );
					if( FileContents.Length == 0 )
					{
						// Zero-length files added by Ionic cause installation/upgrade to fail on device with error 0xE8000050
						// We store a single byte in the files as a workaround for now
						FileContents = new byte[ 1 ];
						FileContents[ 0 ] = 0;
					}

					FileSystem.WriteAllBytes( RelativeFilename, FileContents );

					if( ( FileContents.Length >= 1024 * 1024 ) || ( Config.bVerbose ) )
					{
						FilesBeingModifiedToPrintOut.Add( ZipAbsolutePath );
					}
				}
			}

			// Re-sign the executable if there is a signing context
			if( CodeSigner != null )
			{
				CodeSigner.PerformSigning();
			}

			// Stick in the iTunesArtwork PNG if available
			string iTunesArtworkPath = Path.Combine( Config.BuildDirectory, "iTunesArtwork" );
			if( File.Exists( iTunesArtworkPath ) )
			{
				Zip.UpdateFile( iTunesArtworkPath, "" );
			}

			// Save the Zip
			Program.Log( "Compressing files into IPA.{0}", Config.bVerbose ? "" : "  Only large files will be listed next, but other files are also being packaged." );
			FileSystem.Close();

			TimeSpan ZipLength = DateTime.Now - StartTime;

			Program.Log( String.Format( "Finished repackaging into IPA, written to '{0}' (took {1:0.00} s)",
				Zip.Name,
				ZipLength.TotalSeconds ) );
		}

        static public bool ExecuteCookCommand(string Command, string RPCCommand)
        {
            switch (Command.ToLowerInvariant())
            {
                case "installstub":
                    Program.Log("Installing resigned stub IPA to binaries directory...");
                    if (RPCCommand == null)
                    {
                        Program.Error("Expected path to stub but no path was specified!");
                    }
                    else
                    {
                        // Copy the stub back in place with a forced signing prefix (UDK or whatever)
                        FileOperations.CopyRequiredFile(RPCCommand, Config.GetIPAPath(".stub"));
                    }
                    break;
                default:
                    return false;
            }

            return true;
        }
    }
}