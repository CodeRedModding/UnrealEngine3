using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using Ionic.Zip;

namespace MacPackager
{
	public class CompileTime
	{
		/** 
		 * Creates the Mac application bundle
		 */
		static public void PackageApp()
		{
			CodeSignatureBuilder CodeSigner = null;

			if (Directory.Exists(Config.PCStagingRootDir))
			{
				FileOperations.DeleteDirectory(new DirectoryInfo(Config.PCStagingRootDir));
			}
			Directory.CreateDirectory(Config.PCStagingRootDir);

			// Create folders for the bundle
			string BundlePath = Path.GetFullPath(Config.PCStagingRootDir + @"\" + Config.BundleName + ".app");
			Directory.CreateDirectory(BundlePath);
			Directory.CreateDirectory(BundlePath + @"\Contents");
			Directory.CreateDirectory(BundlePath + @"\Contents\MacOS");
			Directory.CreateDirectory(BundlePath + @"\Contents\Frameworks");
			Directory.CreateDirectory(BundlePath + @"\Contents\Resources");
			Directory.CreateDirectory(BundlePath + @"\Contents\Resources\English.lproj");

			string ExeName = Program.GameName + "-Mac-" + Program.GameConfiguration;

			// Unzip stub file
			ZipFile StubFile = ZipFile.Read(ExeName + ".app.stub");
			StubFile.ExtractAll(Config.PCStagingRootDir);

			string StubPath = Path.Combine(Config.PCStagingRootDir, ExeName + ".app");

			// Copy files that require no modification
			if (Config.BundleIconFile.Length != 0)
			{
				FileOperations.CopyRequiredFile(Config.BundleIconFile, Path.Combine(BundlePath, @"Contents\Resources\" + Config.BundleName + "Icon.icns"));
			}
			else
			{
				FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\Resources\" + Program.GameName + ".icns"), Path.Combine(BundlePath, @"Contents\Resources\" + Config.BundleName + "Icon.icns"));
			}

            FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\MacOS\" + ExeName), Path.Combine(BundlePath, @"Contents\MacOS\" + Program.GameName));
			FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\Frameworks\libogg.dylib"), Path.Combine(BundlePath, @"Contents\Frameworks\libogg.dylib"));
			FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\Frameworks\libvorbis.dylib"), Path.Combine(BundlePath, @"Contents\Frameworks\libvorbis.dylib"));
			FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\Resources\English.lproj\MainMenu.nib"), Path.Combine(BundlePath, @"Contents\Resources\English.lproj\MainMenu.nib"));
			FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\Resources\English.lproj\InfoPlist.strings"), Path.Combine(BundlePath, @"Contents\Resources\English.lproj\InfoPlist.strings"));
			FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\PkgInfo"), Path.Combine(BundlePath, @"Contents\PkgInfo"));

			// Prepare Info.plist file
			string InfoPlistContents = File.ReadAllText(Path.Combine(StubPath, @"Contents\Info.plist"));
			InfoPlistContents = InfoPlistContents.Replace(Program.GameName, Config.BundleName + "Icon");
            InfoPlistContents = InfoPlistContents.Replace("${EXECUTABLE_NAME}", Program.GameName);
			InfoPlistContents = InfoPlistContents.Replace("com.epicgames.${PRODUCT_NAME:rfc1034identifier}", Config.BundleID);
			InfoPlistContents = InfoPlistContents.Replace("${PRODUCT_NAME}", Config.BundleName);
			InfoPlistContents = InfoPlistContents.Replace("${MACOSX_DEPLOYMENT_TARGET}", Config.MinMacOSXVersion);
			File.WriteAllText(Path.Combine(BundlePath, @"Contents\Info.plist"), InfoPlistContents);

			// Copy game's data
			FileOperations.CopyFolder(@"..\..\" + Program.GameName + @"\CookedMac", BundlePath + @"\Contents\Resources\" + Program.GameName + @"\CookedMac", null, false);
			FileOperations.CopyFiles(@"..\..\" + Program.GameName + @"\Config", BundlePath + @"\Contents\Resources\" + Program.GameName + @"\Config", null, "Default*.ini", null);
			FileOperations.CopyFiles(@"..\..\" + Program.GameName + @"\Config\Mac", BundlePath + @"\Contents\Resources\" + Program.GameName + @"\Config\Mac", null, "*.ini", null);
			FileOperations.CopyRequiredFile(@"..\..\" + Program.GameName + @"\Splash\Mac\Splash.bmp", BundlePath + @"\Contents\Resources\" + Program.GameName + @"\Splash\Mac\Splash.bmp");

			FileOperations.CopyFiles(@"..\..\Engine\Config", BundlePath + @"\Contents\Resources\Engine\Config", null, "Base*.ini", null);
			FileOperations.CopyFiles(@"..\..\Engine\Config\Mac", BundlePath + @"\Contents\Resources\Engine\Config\Mac", null, "*.ini", null);
			FileOperations.CopyFiles(@"..\..\Engine\Shaders\Binaries", BundlePath + @"\Contents\Resources\Engine\Shaders\Binaries", null, "*.bin", null);

			foreach (string Language in Program.Languages)
			{
				if (Directory.Exists(@"..\..\Engine\Localization\" + Language))
				{
					FileOperations.CopyFolder(@"..\..\Engine\Localization\" + Language, BundlePath + @"\Contents\Resources\Engine\Localization\" + Language, null, false);
				}
				if (Directory.Exists(@"..\..\" + Program.GameName + @"\Localization\" + Language))
				{
					FileOperations.CopyFolder(@"..\..\" + Program.GameName + @"\Localization\" + Language, BundlePath + @"\Contents\Resources\" + Program.GameName + @"\Localization\" + Language, null, false);
					FileOperations.DeleteFile(BundlePath + @"\Contents\Resources\" + Program.GameName + @"\Localization\" + Language + @"\PS3.int");
				}
				FileOperations.CopyFiles(@"..\..\Engine\Localization", BundlePath + @"\Contents\Resources\Engine\Localization", null, "*." + Language + ".*", null);
			}

			if (Program.bPackagingForMAS)
			{
				FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\CodeResources"), Path.Combine(BundlePath, @"Contents\CodeResources"));
				FileOperations.CopyRequiredFile(Path.Combine(StubPath, @"Contents\_CodeSignature\CodeResources"), Path.Combine(BundlePath, @"Contents\_CodeSignature\CodeResources"));
				FileOperations.RegularFileSystem FileSystem = new FileOperations.RegularFileSystem(BundlePath + @"\Contents\");
				CodeSigner = new CodeSignatureBuilder();
				CodeSigner.FileSystem = FileSystem;
				CodeSigner.PrepareForSigning();
				CodeSigner.PerformSigning();
			}

			// Delete unzipped contents of stub file
			FileOperations.DeleteDirectory(new DirectoryInfo(StubPath));

			ZipFile Zip = new ZipFile();
			Zip.AddDirectory(Config.PCStagingRootDir);
			Zip.Save(Path.GetFullPath(Config.PCStagingRootDir + @"\" + Config.BundleName + ".app.zip"));
			FileOperations.DeleteDirectory(new DirectoryInfo(BundlePath));
		}
	}
}
