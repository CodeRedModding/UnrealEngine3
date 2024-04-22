using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

using System.IO;
using System.Xml;
using System.Diagnostics;

namespace AndroidPackager
{
    class Program
    {
        static string GameName = "";
        static string TargetPlatform = "";
        static string Configuration = "";
        static string OutputPath = "";
        static string IntermediatePath = "";

        static bool bPackageForAmazon = false;
        static bool bPackageForGoogle = false;
        static bool bReleaseSignAPK = false;
        static string Filter = "";

        static void WriteLine(string Msg, params object[] Parms)
        {
            string Final = string.Format(Msg, Parms);

            System.Diagnostics.Debug.WriteLine(Final);
            Console.WriteLine(Final);
        }

        static void ShowUsage()
        {
            WriteLine("Packages Android .apk");
            WriteLine("");
            WriteLine("AndroidPackager <GameName> <TargetPlatform> <Configuration> -params");
            WriteLine("");
            WriteLine("  Name           : Name of the game to be packaged");
            WriteLine("");
            WriteLine("  TargetPlatform : Platform to target (Android, AndroidARM, Androidx86)");
            WriteLine("");
            WriteLine("  Configuration  : Compiled configuration to use");
            WriteLine("");
            WriteLine("  -appendFilter  : Uses Manifest file with _FILTER suffix and adds _FILTER suffix to output");
            WriteLine("");
            WriteLine("  -packageForAmazon  : Packages obb file into APK, adds _Amazon suffix to result");
            WriteLine("");
            WriteLine("  -packageForGoogle  : Informs packager build is for Google Distribution");
            WriteLine("");
        }

        /// <summary>
        /// Main entry point
        /// </summary>
        /// <param name="Args">Command line arguments</param>
        /// <returns></returns>
        static int Main(string[] Args)
        {
            bool bSuccess = true;

            if (Args.Length < 3)
            {
                ShowUsage();
                return 1;
            }

            if (Args[0].StartsWith("-")
                    || Args[1].StartsWith("-")
                    || Args[2].StartsWith("-"))
            {
                ShowUsage();
                return 1;
            }

            InitializeEnvironmentVariables();

            GameName = Args[0];
            TargetPlatform = Args[1];
            Configuration = Args[2];

            bPackageForAmazon = false;
            bPackageForGoogle = false;
            Filter = "";

            // if any additional params were specified, parse them
            for (int ArgIndex = 3; ArgIndex < Args.Length; ArgIndex++)
            {
                if ((String.Compare(Args[ArgIndex], "-appendfilter", true) == 0))
                {
                    // make sure there is another param after this one
                    if (Args.Length > ArgIndex + 1)
                    {
                        // the next param is the base directory
                        Filter = Args[ArgIndex + 1];

                        // skip over it
                        ArgIndex++;
                    }
                    else
                    {
                        WriteLine("Error: No filter specified (use -h to see usage).");
                        return 1;
                    }
                }
                else if ((String.Compare(Args[ArgIndex], "-packageForAmazon", true) == 0))
                {
                    bPackageForAmazon = true;
                }
                else if ((String.Compare(Args[ArgIndex], "-packageForGoogle", true) == 0))
                {
                    bPackageForGoogle = true;
                }
                else if (String.Compare(Args[ArgIndex], "-h", true) == 0)
                {
                    ShowUsage();
                }
            }

            // Set path to UE3 Root (packager location and then back up)
            Directory.SetCurrentDirectory(Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location));
            Directory.SetCurrentDirectory("../..");

            // Remove previous APK
            OutputPath = "Binaries/Android/" + GameName + "-" + TargetPlatform + "-" + Configuration + Filter;
            OutputPath += bPackageForAmazon ? "_Amazon.apk" : ".apk";
            if (File.Exists(OutputPath))
            {
                File.SetAttributes(OutputPath, FileAttributes.Normal);
                File.Delete(OutputPath);
            }

            WriteLine("Preparing Intermediate directory...");
            bSuccess = bSuccess && PrepareIntermediateDirectory();

            if (bSuccess)
            {
                if (File.Exists(IntermediatePath + "/libs/armeabi-v7a/libUnrealEngine3.so"))
                {
                    WriteLine("Stripping /armeabi-v7a/libUnrealEngine3.so");
                    string Arguments = IntermediatePath + "/libs/armeabi-v7a/libUnrealEngine3.so";

                    // Set arguments based on configuration
                    if ((bPackageForAmazon || bPackageForGoogle) && Configuration == "Shipping")
                    {
                        Arguments = "--strip-all " + Arguments;
                        WriteLine("\tUsing --strip-all for Distribution Shipping Configuration");
                    }

                    if (!File.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/bin/arm-linux-androideabi-strip.exe")))
                    {
                        WriteLine("Error: Could not find: " + Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/bin/arm-linux-androideabi-strip.exe"));
                        return 1;
                    }

                    ProcessStartInfo StripProcessStartInfo = new ProcessStartInfo();
                    StripProcessStartInfo.WorkingDirectory = ".";
                    StripProcessStartInfo.FileName = Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/bin/arm-linux-androideabi-strip.exe");
                    StripProcessStartInfo.Arguments = Arguments;
                    StripProcessStartInfo.UseShellExecute = false;
                    Process StripProcess = new Process();
                    StripProcess.StartInfo = StripProcessStartInfo;
                    StripProcess.Start();
                    StripProcess.WaitForExit();
                }
                if (File.Exists(IntermediatePath + "/libs/x86/libUnrealEngine3.so"))
                {
                    WriteLine("Stripping /x86/libUnrealEngine3.so");
                    string Arguments = IntermediatePath + "/libs/x86/libUnrealEngine3.so";

                    // Set arguments based on configuration
                    if ((bPackageForAmazon || bPackageForGoogle) && Configuration == "Shipping")
                    {
                        Arguments = "--strip-all " + Arguments;
                        WriteLine("\tUsing --strip-all for Distribution Shipping Configuration");
                    }

                    if (!File.Exists(Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/x86-4.4.3/prebuilt/windows/i686-linux-android/bin/strip.exe")))
                    {
                        WriteLine("Error: Could not find: " + Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/x86-4.4.3/prebuilt/windows/i686-linux-android/bin/strip.exe"));
                        return 1;
                    }

                    ProcessStartInfo StripProcessStartInfo = new ProcessStartInfo();
                    StripProcessStartInfo.WorkingDirectory = ".";
                    StripProcessStartInfo.FileName = Environment.ExpandEnvironmentVariables("%NDKROOT%/toolchains/x86-4.4.3/prebuilt/windows/i686-linux-android/bin/strip.exe");
                    StripProcessStartInfo.Arguments = Arguments;
                    StripProcessStartInfo.UseShellExecute = false;
                    Process StripProcess = new Process();
                    StripProcess.StartInfo = StripProcessStartInfo;
                    StripProcess.Start();
                    StripProcess.WaitForExit();
                }
            }
            
            // Bail now if failed on the binary libs
            if (!bSuccess)
            {
                return 1;
            }

            // Remove any existing data that may be problematic if left stale
            if (File.Exists(IntermediatePath + "/ant.properties")) { File.Delete(IntermediatePath + "/ant.properties"); }

            WriteLine("Copying from default location (Development/Src/Android/java)");
            string SrcDir = "Development/Src/Android/java/";
            File.Copy(SrcDir + "build.xml", IntermediatePath + "/build.xml", true);
            if (File.Exists(SrcDir + "AndroidManifest" + Filter + ".xml")) { File.Copy(SrcDir + "AndroidManifest" + Filter + ".xml", IntermediatePath + "/AndroidManifest.xml", true); }
            File.Copy(SrcDir + "project.properties", IntermediatePath + "/project.properties", true);
            if (File.Exists(SrcDir + "ant.properties")) { File.Copy(SrcDir + "ant.properties", IntermediatePath + "/ant.properties", true); }
            DirectoryDelete(IntermediatePath + "/res");
            DirectoryDelete(IntermediatePath + "/src");
            DirectoryCopy(SrcDir + "res", IntermediatePath + "/res", true);
            DirectoryCopy(SrcDir + "src", IntermediatePath + "/src", true);

            // Make sure all new contents are writable
            DirectoryMakeWriteable(IntermediatePath);

            WriteLine("Overwriting with any game specific files..");
            SrcDir = GameName + "/Build/Android/java/";
            if (File.Exists(SrcDir + "build.xml")) { File.Copy(SrcDir + "build.xml", IntermediatePath + "/build.xml", true); }
            if (File.Exists(SrcDir + "AndroidManifest" + Filter + ".xml")) { File.Copy(SrcDir + "AndroidManifest" + Filter + ".xml", IntermediatePath + "/AndroidManifest.xml", true); }
            if (File.Exists(SrcDir + "project.properties")) { File.Copy(SrcDir + "project.properties", IntermediatePath + "/project.properties", true); }
            if (File.Exists(SrcDir + "ant.properties")) { File.Copy(SrcDir + "ant.properties", IntermediatePath + "/ant.properties", true); }
            if (Directory.Exists(SrcDir + "res")) { DirectoryCopy(SrcDir + "res", IntermediatePath + "/res", true); }

            // Make sure an appropriate Manifest was found
            string ManifestPath = IntermediatePath + "/AndroidManifest.xml";
            if (!File.Exists(ManifestPath))
            {
                WriteLine("Error: No AndroidManifest" + Filter + ".xml file was found");
                return 1;
            }

            // Read manifest information
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

            // remove potentially stale directories
            DirectoryDelete(IntermediatePath + "/bin");
            DirectoryDelete(IntermediatePath + "/assets");
            Directory.CreateDirectory(IntermediatePath + "/assets");

            // Copy commandline into apk
            if (bSuccess && File.Exists(GameName + "/CookedAndroid/UE3CommandLine.txt"))
            {
                WriteLine("Copied UE3CommandLine.txt into /assets");
                File.Copy(GameName + "/CookedAndroid/UE3CommandLine.txt", IntermediatePath + "/assets/UE3CommandLine.txt");
            }

            if (bSuccess && bPackageForAmazon)
            {
                if (File.Exists("Binaries/Android/assets/" + OBBName))
                {
					// @hack .png is put at the end to prevent compression of the OBB file now that AAPT is no longer called directly
                    File.Copy("Binaries/Android/assets/" + OBBName, IntermediatePath + "/assets/" + OBBNameNoFilter + ".png");
                }
                else
                {
                    WriteLine("Error: Could not find " + "Binaries/Android/assets/" + OBBName + " to package into Amazon APK!");
                    bSuccess = false;
                }
            }  

            // Check for keystore if there is an ant.properties
            bSuccess = bSuccess && FindKeystore();

            // Run android.bat update
            if (!File.Exists(Environment.ExpandEnvironmentVariables("%ANDROID_ROOT%/android-sdk-windows/tools/android.bat")))
            {
                WriteLine("Error: Could not find: " + Environment.ExpandEnvironmentVariables("%ANDROID_ROOT%/android-sdk-windows/tools/android.bat"));
                bSuccess = false;
            }

            // First update libraries
            if (bSuccess)
            {
                ProcessStartInfo AndroidBatStartInfo = new ProcessStartInfo();
                AndroidBatStartInfo.WorkingDirectory = IntermediatePath;
                AndroidBatStartInfo.FileName = "cmd.exe";
                AndroidBatStartInfo.Arguments = "/c " + Environment.ExpandEnvironmentVariables("%ANDROID_ROOT%/android-sdk-windows/tools/android.bat")
                    + " update lib-project --path ..\\..\\..\\..\\External\\google\\play_apk_expansion\\downloader_library";
                AndroidBatStartInfo.UseShellExecute = false;
                Process AndroidBat = new Process();
                AndroidBat.StartInfo = AndroidBatStartInfo;
                AndroidBat.Start();
                AndroidBat.WaitForExit();
            }
            if (bSuccess)
            {
                ProcessStartInfo AndroidBatStartInfo = new ProcessStartInfo();
                AndroidBatStartInfo.WorkingDirectory = IntermediatePath;
                AndroidBatStartInfo.FileName = "cmd.exe";
                AndroidBatStartInfo.Arguments = "/c " + Environment.ExpandEnvironmentVariables("%ANDROID_ROOT%/android-sdk-windows/tools/android.bat")
                    + " update lib-project --path ..\\..\\..\\..\\External\\google\\play_licensing\\library";
                AndroidBatStartInfo.UseShellExecute = false;
                Process AndroidBat = new Process();
                AndroidBat.StartInfo = AndroidBatStartInfo;
                AndroidBat.Start();
                AndroidBat.WaitForExit();
            }

            // Then the main project
            if (bSuccess)
            {
                ProcessStartInfo AndroidBatStartInfo = new ProcessStartInfo();
                AndroidBatStartInfo.WorkingDirectory = IntermediatePath;
                AndroidBatStartInfo.FileName = "cmd.exe";
                AndroidBatStartInfo.Arguments = "/c " + Environment.ExpandEnvironmentVariables("%ANDROID_ROOT%/android-sdk-windows/tools/android.bat") + " update project --path .";
                AndroidBatStartInfo.UseShellExecute = false;
                Process AndroidBat = new Process();
                AndroidBat.StartInfo = AndroidBatStartInfo;
                AndroidBat.Start();
                AndroidBat.WaitForExit();
            }


            // Run ant
            if (!File.Exists(Environment.ExpandEnvironmentVariables("%ANT_ROOT%/bin/ant")))
            {
                WriteLine("Error: Could not find: " + Environment.ExpandEnvironmentVariables("%ANT_ROOT%/bin/ant"));
                bSuccess = false;
            }

            if (bSuccess)
            {
                ProcessStartInfo CallAntStartInfo = new ProcessStartInfo();
                CallAntStartInfo.WorkingDirectory = IntermediatePath;
                CallAntStartInfo.FileName = "cmd.exe";
                if (bReleaseSignAPK)
                {
                    CallAntStartInfo.Arguments = "/c " + Environment.ExpandEnvironmentVariables("%ANT_ROOT%/bin/ant") + " release";
                }
                else
                {
                    CallAntStartInfo.Arguments = "/c " + Environment.ExpandEnvironmentVariables("%ANT_ROOT%/bin/ant") + " debug";
                }
                CallAntStartInfo.UseShellExecute = false;
                Process CallAnt = new Process();
                CallAnt.StartInfo = CallAntStartInfo;
                CallAnt.Start();
                CallAnt.WaitForExit();
            }

            string ResultPath = IntermediatePath + "/bin/UnrealEngine3-";
            ResultPath += bReleaseSignAPK ? "release.apk" : "debug.apk";
            if (bSuccess && File.Exists(ResultPath))
            {
                File.Copy(ResultPath, OutputPath, true);
            }
            else
            {
                bSuccess = false;
            }

            if (bSuccess)
            {
                WriteLine("Packaged APK written to " + Path.GetFileName(OutputPath));

                if (!bReleaseSignAPK)
                {
                    WriteLine("Warning: Packaged APK was signed with a Debug key, and cannot be released publicly");
                    WriteLine("Please review the information on APK Release signing at https://udn.epicgames.com/Three/DistributionAndroidOS");
                }
            }

            // Make sure all new contents are writable
            DirectoryMakeWriteable(IntermediatePath);

            return bSuccess ? 0 : 1;
        }

        /*
         * Searches for private key and then runs jarsigner
         */
        static bool FindKeystore()
        {
            // Look for private key or use the debug key if needed
            string AntPropertiesPath = IntermediatePath + "/ant.properties";
            
            if (!File.Exists(AntPropertiesPath))
            {
                WriteLine("Warning: No ant.properties file, debug signing");
                return true;
            }

            // Extract name of keystore from ant.properties
            string KeystorePath = null;

            // load ant.properties
            TextReader AntReader = new StreamReader(AntPropertiesPath);

            // Scan for keystore name
            string CurrentLine = AntReader.ReadLine();
            while (CurrentLine != null)
            {
                if (CurrentLine.StartsWith("key.store="))
                {
                    CurrentLine = CurrentLine.Substring(("key.store=").Length);
                    KeystorePath = Path.GetDirectoryName(AntPropertiesPath) + "/" + CurrentLine;

                    // Search for keystore and copy it if found
                    if (File.Exists(GameName + "/Build/Android/java/" + CurrentLine))
                    {
                        // Game specific
                        File.Copy(GameName + "/Build/Android/java/" + CurrentLine, KeystorePath, true);
                        bReleaseSignAPK = true;
                    }
                    else if (File.Exists("Development/Src/Android/java/" + CurrentLine))
                    {
                        // Default Android directory
                        File.Copy("Development/Src/Android/java/" + CurrentLine, KeystorePath, true);
                        bReleaseSignAPK = true;
                    }
                    else
                    {
                        // Missing named Keystore!
                        WriteLine("Error: Could not locate the keystore " + CurrentLine + " specified by ant.properties!");
                        WriteLine("Error: Please review the information on APK Release signing at https://udn.epicgames.com/Three/DistributionAndroidOS");
                        return false;
                    }
                }

                CurrentLine = AntReader.ReadLine();
            }

            return true;
        }

        /*
         * Wipe Intermediate directory and then copy needed .so files
         */
        static bool PrepareIntermediateDirectory()
        {
            IntermediatePath = "Development/Intermediate/" + GameName + "/" + TargetPlatform + "/" + Configuration;

            // Delete and remake any existing build folder
            if (!Directory.Exists(IntermediatePath))
            {
                Directory.CreateDirectory(IntermediatePath);
            }

            System.Threading.Thread.Sleep(1);

            // Make all existing contents writeable
            DirectoryMakeWriteable(IntermediatePath);

            // Copy NDK libs
            DirectoryDelete(IntermediatePath + "/libs");
            Directory.CreateDirectory(IntermediatePath + "/libs");

            // ARM
            if (String.Equals(TargetPlatform.ToUpper(), "ANDROIDARM") || String.Equals(TargetPlatform.ToUpper(), "ANDROID"))
            {
                Directory.CreateDirectory(IntermediatePath + "/libs/armeabi-v7a");

                if (!File.Exists("Binaries/Android/" + GameName + "-AndroidARM-" + Configuration + ".so"))
                {
                    WriteLine("Error: Could not find " + "Binaries/Android/" + GameName + "-AndroidARM-" + Configuration + ".so");
                    return false;
                }

                File.Copy("Binaries/Android/" + GameName + "-AndroidARM-" + Configuration + ".so", IntermediatePath + "/libs/armeabi-v7a/libUnrealEngine3.so");
            }

            // x86
            if (String.Equals(TargetPlatform.ToUpper(), "ANDROIDX86") || String.Equals(TargetPlatform.ToUpper(), "ANDROID"))
            {
                Directory.CreateDirectory(IntermediatePath + "/libs/x86");

                if (!File.Exists("Binaries/Android/" + GameName + "-Androidx86-" + Configuration + ".so"))
                {
                    WriteLine("Error: Could not find " + "Binaries/Android/" + GameName + "-Androidx86-" + Configuration + ".so");
                    return false;
                }

                File.Copy("Binaries/Android/" + GameName + "-Androidx86-" + Configuration + ".so", IntermediatePath + "/libs/x86/libUnrealEngine3.so");
            }

            // Copy Apsalar libs if found
            if (Directory.Exists("Development/External/NoRedist/Apsalar/Android"))
            {
                DirectoryCopy("Development/External/NoRedist/Apsalar/Android", IntermediatePath + "/libs", false);
            }

            return true;
        }

        /**
         * Initializes environment variables required by toolchain.
        */
        public static void InitializeEnvironmentVariables()
        {
            // Set up Android SDK root folder.
            string AndroidRoot = Environment.GetEnvironmentVariable("ANDROID_ROOT");	// Epic variable, allows user override.
            if (AndroidRoot == null)
            {
                AndroidRoot = "C:/Android";
                if (!Directory.Exists("C:/Android") && Directory.Exists("D:/Android"))
                {
                    AndroidRoot = "D:/Android";
                }
                Environment.SetEnvironmentVariable("ANDROID_ROOT", AndroidRoot);
            }

            // Set up Android variables, assuming all components are installed under the root folder.
            if (Environment.GetEnvironmentVariable("ANT_ROOT") == null)
            {
                Environment.SetEnvironmentVariable("ANT_ROOT", Path.Combine(AndroidRoot, "apache-ant-1.8.1"));
            }
            if (Environment.GetEnvironmentVariable("CYGWIN") == null)
            {
                Environment.SetEnvironmentVariable("CYGWIN", "nodosfilewarning");
            }
            if (Environment.GetEnvironmentVariable("CYGWIN_HOME") == null)
            {
                Environment.SetEnvironmentVariable("CYGWIN_HOME", Path.Combine(AndroidRoot, "cygwin"));
            }
            if (Environment.GetEnvironmentVariable("JAVA_HOME") == null)
            {
                Environment.SetEnvironmentVariable("JAVA_HOME", Path.Combine(AndroidRoot, "jdk1.6.0_21"));
            }
            if (Environment.GetEnvironmentVariable("NDKROOT") == null)
            {
                Environment.SetEnvironmentVariable("NDKROOT", Path.Combine(AndroidRoot, "android-ndk-r8b"));
            }
        }

        // From MSDN: http://msdn.microsoft.com/en-us/library/bb762914.aspx
        static void DirectoryCopy(string sourceDirName, string destDirName, bool copySubDirs)
        {
            DirectoryInfo dir = new DirectoryInfo(sourceDirName);
            DirectoryInfo[] dirs = dir.GetDirectories();

            if (!dir.Exists)
            {
                throw new DirectoryNotFoundException(
                    "Source directory does not exist or could not be found: "
                    + sourceDirName);
            }

            if (!Directory.Exists(destDirName))
            {
                Directory.CreateDirectory(destDirName);
            }

            FileInfo[] files = dir.GetFiles();
            foreach (FileInfo file in files)
            {
                string temppath = Path.Combine(destDirName, file.Name);

                if (File.Exists(temppath))
                {
                    File.SetAttributes(temppath, FileAttributes.Normal);
                }

                file.CopyTo(temppath, true);
            }

            if (copySubDirs)
            {
                foreach (DirectoryInfo subdir in dirs)
                {
                    string temppath = Path.Combine(destDirName, subdir.Name);
                    DirectoryCopy(subdir.FullName, temppath, copySubDirs);
                }
            }
        }

        /*
         * Recursively remove an entire directory tree
         */
        static void DirectoryDelete(String TargetPath)
        {
            if (!Directory.Exists(TargetPath))
            {
                return;
            }

            string[] Files = Directory.GetFiles(TargetPath);
            string[] Directories = Directory.GetDirectories(TargetPath);

            foreach (string FileToDelete in Files)
            {
                File.SetAttributes(FileToDelete, FileAttributes.Normal);
                File.Delete(FileToDelete);
            }

            foreach (string DirToDelete in Directories)
            {
                DirectoryDelete(DirToDelete);
            }

            Directory.Delete(TargetPath, false);
        }

        /*
         * Recursively make all data writable
         */
        static void DirectoryMakeWriteable (String TargetPath)
        {
            if (!Directory.Exists(TargetPath))
            {
                return;
            }

            string[] Files = Directory.GetFiles(TargetPath);
            string[] Directories = Directory.GetDirectories(TargetPath);

            foreach (string FileToModify in Files)
            {
                File.SetAttributes(FileToModify, FileAttributes.Normal);
            }

            foreach (string DirToModify in Directories)
            {
                DirectoryMakeWriteable(DirToModify);
            }

            DirectoryInfo TargetInfo = new DirectoryInfo(TargetPath);
            TargetInfo.Attributes &= ~FileAttributes.ReadOnly;
        }
    }
}
