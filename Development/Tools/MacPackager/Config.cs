/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;

namespace MacPackager
{
	internal class Config
	{
		/// <summary>
		/// The display name in title bars, popups, etc...
		/// </summary>
		public static string AppDisplayName = "Unreal Mac Configuration";

		/// <summary>
		/// Optional Prefix to append to .xcent and .mobileprovision files, for handling multiple certificates on same source game
		/// </summary>
		public static string SigningPrefix = "";

		public static string PCStagingRootDir = "";

		/// <summary>
		/// Whether or not to output extra information (like every file copy and date/time stamp)
		/// </summary>
		public static bool bVerbose = true;

		/// <summary>
		/// Whether or not to output extra information in code signing
		/// </summary>
		public static bool bCodeSignVerbose = false;

		/// <summary>
		/// If the requirements blob is present in the existing executable when code signing, should it be carried forward
		/// (true) or should a dummy requirements blob be created with no actual requirements (false)
		/// </summary>
		public static bool bMaintainExistingRequirementsWhenCodeSigning = false;

		public static string MinMacOSXVersion = "10.6";

		public static string BundleID;
		public static string BundleName;
		public static string BundleIconFile;

		public static string DeployPath;

		public static void Initialize()
		{
			// Root directory on PC for staging files to copy to Mac
			Config.PCStagingRootDir = String.Format(@"..\..\{0}\Build\Mac\Mac-Deploy\{1}",
				Program.GameName,
				Program.GameConfiguration);

			// Make sure the directory exists
			if( !Directory.Exists( Config.PCStagingRootDir ) )
			{
				Directory.CreateDirectory( Config.PCStagingRootDir );
			}

			string Filename = Config.GetPlistOverrideFilename();
			string SourcePList = ReadOrCreatePlist(Filename);
			Utilities.PListHelper Helper = new Utilities.PListHelper(SourcePList);

			if (!Helper.GetString("CFBundleIdentifier", out BundleID) ||
				BundleID == "")
			{
				BundleID = "com.YourCompany.GameNameNoSpaces";
				Helper.SetString("CFBundleIdentifier", BundleID);
			}

			if (!Helper.GetString("CFBundleName", out BundleName) ||
				BundleName == "")
			{
				BundleName = Program.GameName;
				Helper.SetString("CFBundleName", BundleName);
			}

			if (!Helper.GetString("MacPackagerDeployPath", out DeployPath) ||
				DeployPath == "")
			{
				DeployPath = "../../Binaries/Mac";
				Helper.SetString("MacPackagerDeployPath", DeployPath);
			}

			if (!Helper.GetString("CFBundleIconFile", out BundleIconFile) ||
				BundleIconFile == "" )
			{
				BundleIconFile = "";
				Helper.SetString("CFBundleIconFile", BundleIconFile);
			}

			SavePList(Helper, Filename);
		}

		/// <summary>
		/// The local build directory (on PC)
		/// </summary>
		public static string BuildDirectory
		{
			get { return Path.GetFullPath(@"..\..\" + Program.GameName + @"\Build\Mac"); }
		}

		/// <summary>
		/// Returns the name of the file containing user overrides that will be applied when packaging on PC
		/// </summary>
		public static string GetPlistOverrideFilename()
		{
			string Folder = String.Format(@"..\..\{0}\Build\Mac", Program.GameName);

			string Filename = Path.Combine(Folder, Program.GameName + "_MacPackagerConfig.plist");

			return Filename;
		}

		public static string ReadOrCreatePlist(string Filename)
		{
			string Data = "";
			try
			{
				Data = File.ReadAllText(Filename, Encoding.UTF8);
			}
			catch
			{
				Data = "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n" +
					"<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n" +
					"<plist version=\"1.0\">\n<dict>\n</dict>\n</plist>\n";
			}

			return Data;
		}

		public static void SavePList(Utilities.PListHelper Helper, string Filename)
		{
			FileInfo DestInfo = new FileInfo(Filename);
			if (DestInfo.Exists && DestInfo.IsReadOnly)
			{
				DestInfo.IsReadOnly = false;
			}
			Helper.SaveToFile(Filename);
		}
	}
}
