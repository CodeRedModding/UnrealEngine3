/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Text.RegularExpressions;
using System.Diagnostics;
using System.IO;
using Microsoft.Win32;

namespace UnrealBuildTool
{
	class AndroidToolChain
	{
		/**
		 * Log that was suppressed during the build. Can be written out if we encounter a build error.
		 */
		static private List<string> SuppressedLog = new List<string>();

        static private bool bHasNDKExtensionsCompiled = false;

		/**
		 * Clears the suppressed log.
		 */
		static public void ClearSuppressedLog()
		{
			SuppressedLog.Clear();
		}

		/**
		 * Outputs the suppressed log.
		 */
		static public void OutputSuppressedLog()
		{
			System.Console.WriteLine("APK build log:");
			foreach (string Line in SuppressedLog)
			{
				System.Console.WriteLine(Line);
			}
		}

		static string GetCLArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

            if (CompileEnvironment.TargetPlatform == CPPTargetPlatform.AndroidARM)
            {
                Result += " -mthumb-interwork";         // Generates code which supports calling between ARM and Thumb instructions, w/o it you can't reliability use both together 
                Result += " -ffunction-sections";       // Places each function in its own section of the output file, linker may be able to perform opts to improve locality of reference
                Result += " -funwind-tables";           // Just generates any needed static data, affects no code 
                Result += " -fstack-protector";         // Emits extra code to check for buffer overflows
                Result += " -mlong-calls";              // Perform function calls by first loading the address of the function into a reg and then performing the subroutine call
                Result += " -fno-strict-aliasing";      // Prevents unwanted or invalid optimizations that could produce incorrect code
                Result += " -fpic";                     // Generates position-independent code (PIC) suitable for use in a shared library
                Result += " -fno-exceptions";           // Do not enable exception handling, generates extra code needed to propagate exceptions
                Result += " -fno-short-enums";          // Do not allocate to an enum type only as many bytes as it needs for the declared range of possible values
                Result += " -finline-limit=64";         // GCC limits the size of functions that can be inlined, this flag allows coarse control of this limit
                Result += " -Werror";                   // Make all warnings into hard errors
                Result += " -Wno-psabi";                // Warn when G++ generates code that is probably not compatible with the vendor-neutral C++ ABI

                Result += " -march=armv7-a";
                Result += " -mfloat-abi=softfp";
                Result += " -mfpu=vfp";
            }
            else if (CompileEnvironment.TargetPlatform == CPPTargetPlatform.Androidx86)
            {
                Result += " -fstrict-aliasing";
                Result += " -funswitch-loops";
                Result += " -finline-limit=128";
                Result += " -fno-omit-frame-pointer";
                Result += " -fno-strict-aliasing";
                Result += " -fno-short-enums";
                Result += " -fno-exceptions";
                Result += " -march=atom";
            }

            if (CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Debug)
            {
                // for now, we keep the -Os in the hopes of having smaller .so's even when debugging
                Result += " -g";
            }
            else
            {
                Result += " -fomit-frame-pointer";
                Result += " -Os";
                Result += " -O2";
            }

			return Result;
		}

		static string GetCLArguments_CPP( CPPEnvironment CompileEnvironment )
		{
			string Result = "";
            Result += " -fno-rtti";                 // Disable generation of information about every class with virtual functions for use by the C++ runtime type identification features
			return Result;
		}
		
		static string GetCLArguments_C()
		{
			string Result = "";
            Result += " -x c";
			return Result;
		}

		static string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";
			Result += " -nostdlib";
            Result += " -Wl,-shared,-Bsymbolic";
	        Result += " -Wl,--no-undefined";

            if (LinkEnvironment.TargetPlatform == CPPTargetPlatform.AndroidARM)
            {
                Result += " -Wl,--fix-cortex-a8";   // Contains a workaround for an erratum in ARM Cortex-A8
            }

			return Result;
		}

        static void ConditionallyAddNDKSourceFiles(List<FileItem> SourceFiles)
        {
            if (!bHasNDKExtensionsCompiled)
            {
                SourceFiles.Add(FileItem.GetItemByPath(Environment.GetEnvironmentVariable("NDKROOT") + "/sources/android/cpufeatures/cpu-features.c"));
            }
            bHasNDKExtensionsCompiled = true;
        }

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
		{
			string Arguments = GetCLArguments_Global(CompileEnvironment);

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.IncludePaths)
			{
				Arguments += string.Format(" -I \"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				Arguments += string.Format(" -I \"{0}\"", IncludePath);
			}

            // Directly added NDK files for NDK extensions
            ConditionallyAddNDKSourceFiles(SourceFiles);

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D{0}", Definition);
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action();
				string FileArguments = "";
				bool bIsPlainCFile = Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C";

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);
				foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
				{
					CompileAction.PrerequisiteItems.Add(IncludedFile);
				}

                // Add C or C++ specific compiler arguments.
                if (bIsPlainCFile)
                {
                    FileArguments += GetCLArguments_C();
                }
                else
                {
                    FileArguments += GetCLArguments_CPP(CompileEnvironment);
                }

				// Add the source file path to the command-line.
				FileArguments += string.Format(" -c \"{0}\"", SourceFile.AbsolutePath);

				// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByPath(
					Path.Combine(
						CompileEnvironment.OutputDirectory,
						Path.GetFileName(SourceFile.AbsolutePath) + ".o"
						)
					);
				CompileAction.ProducedItems.Add(ObjectFile);
				Result.ObjectFiles.Add(ObjectFile);
				FileArguments += string.Format(" -o\"{0}\"", ObjectFile.AbsolutePath);
				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = GetAndroidToolPath(CompileEnvironment.TargetPlatform, "g++");
				CompileAction.bIsVCCompiler = false;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.bShouldLogIfExecutedLocally = false;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(NativeErrorHandler);

				// Don't farm out creation of precomputed headers as it is the critical path task.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.PrecompiledHeaderAction != PrecompiledHeaderAction.Create;
			}
			return Result;
		}

		public static FileItem LinkFiles(LinkEnvironment LinkEnvironment)
		{
			// Create an action that invokes the linker.
			Action LinkAction = new Action();
			LinkAction.bIsLinker = true;
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = GetAndroidToolPath(LinkEnvironment.TargetPlatform, "g++");

			// Get link arguments.
			LinkAction.CommandArguments = GetLinkArguments(LinkEnvironment);

			// Add the library paths to the argument list.
			foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
			{
				LinkAction.CommandArguments += string.Format(" -L\"{0}\" -Wl,-rpath-link -Wl,\"{0}\"", LibraryPath);
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));
			LinkAction.OutputEventHandler = new DataReceivedEventHandler(NativeErrorHandler);


			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath.Replace("\\", "\\\\")));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file.
			string ResponseFileName = LinkEnvironment.OutputFilePath + ".response";
			LinkAction.CommandArguments += string.Format(" @\"{0}\"", ResponseFile.Create(ResponseFileName, InputFileNames));
			
            // This is supposed to go after the source files, but before the stdlibs
            if (LinkEnvironment.TargetPlatform == CPPTargetPlatform.AndroidARM)
            {
                LinkAction.CommandArguments += " $(NDKROOT)/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/lib/gcc/arm-linux-androideabi/4.4.3/libgcc.a";
            }
            else
            {
                LinkAction.CommandArguments += " $(NDKROOT)/toolchains/x86-4.4.3/prebuilt/windows/lib/gcc/i686-linux-android/4.4.3/libgcc.a";
                LinkAction.CommandArguments += " $(NDKROOT)/toolchains/x86-4.4.3/prebuilt/windows/lib/gcc/i686-linux-android/4.4.3/crtbegin.o";
            }
            
            foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
                LinkAction.CommandArguments += string.Format(" -l\"{0}\"", AdditionalLibrary);
            }

			// Add the output file to the command-line.
            // Adding it twice:  name only for libname, full path for output...
            LinkAction.CommandArguments += string.Format("  -Wl,-soname,\"{0}\"", Path.GetFileName(OutputFile.ToString()));
            LinkAction.CommandArguments += string.Format("  -o \"{0}\"", OutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}

		/** Accesses the bin directory for the VC toolchain for the specified platform. */
		static string GetAndroidToolPath(CPPTargetPlatform Platform,string ToolName)
		{	
			// Initialize environment variables required for spawned tools.
			InitializeEnvironmentVariables();

			// Out variable that is going to contain fully qualified path to executable upon return.
			string AndroidToolPath = "";

            if (Platform == CPPTargetPlatform.Androidx86)
            {
                AndroidToolPath = Path.Combine(Environment.GetEnvironmentVariable("NDKROOT") + "/toolchains/x86-4.4.3/prebuilt/windows/bin", "i686-linux-android-" + ToolName + ".exe");
            }
            else
            {
                AndroidToolPath = Path.Combine(Environment.GetEnvironmentVariable("NDKROOT") + "/toolchains/arm-linux-androideabi-4.4.3/prebuilt/windows/bin/", "arm-linux-androideabi-" + ToolName + ".exe");
            }

			return AndroidToolPath;
		}

		/** Helper to only initialize environment variables once. */
		static bool bAreEnvironmentVariablesAlreadyInitialized = false;
		
		/** The strings used to update build error paths for double clicking to work */
		static string JavaSourcePathReplacementFrom = "";
		static string JavaSourcePathReplacementTo = "";

		/**
		 * Initializes environment variables required by toolchain. Different for 32 and 64 bit.
		 */
		public static void InitializeEnvironmentVariables( )
		{
			if( !bAreEnvironmentVariablesAlreadyInitialized )
			{
				// Set up Android SDK root folder.
				string AndroidRoot = Environment.GetEnvironmentVariable( "ANDROID_ROOT" );	// Epic variable, allows user override.
				if ( AndroidRoot == null )
				{
					AndroidRoot = "C:\\Android";
					if ( !Directory.Exists("C:\\Android") && Directory.Exists("D:\\Android") )
					{
						AndroidRoot = "D:\\Android";
					}
					Environment.SetEnvironmentVariable( "ANDROID_ROOT", AndroidRoot );
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

				// Manually include the Windows and Android SDK paths in the LIB/ INCLUDE/ PATH variables.
				string AddPath = ";" + Path.Combine(Environment.GetEnvironmentVariable("ANDROID_ROOT"), "android-sdk-windows\\tools");
				AddPath += ";" + Path.Combine(Environment.GetEnvironmentVariable("ANT_ROOT"), "bin");
				AddPath += ";" + Path.Combine(Environment.GetEnvironmentVariable("CYGWIN_HOME"), "bin");
				AddPath += ";" + Path.Combine(Environment.GetEnvironmentVariable("JAVA_HOME"), "bin");
				string AddClassPath = Path.Combine(Environment.GetEnvironmentVariable("JAVA_HOME"), "lib");

				Environment.SetEnvironmentVariable( "PATH", Utils.ResolveEnvironmentVariable(AddPath + ";%PATH%") );
				Environment.SetEnvironmentVariable( "CLASSPATH", Utils.ResolveEnvironmentVariable(AddClassPath + ";%CLASSPATH%") );

				bAreEnvironmentVariablesAlreadyInitialized = true;
			}			
		}

		public static void NativeErrorHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null))
			{
				string OutString = Line.Data;
				
				// try to convert <blah>:line:<blah> into something Visual Studio can double-click on
				Int32 ColonIndex = -1;
				if ((ColonIndex = OutString.IndexOf(':')) >= 0 && ColonIndex < OutString.Length - 1)
				{
					bool bFoundNumber = false;
					while (!bFoundNumber)
					{
						int NextColonIndex = OutString.IndexOf(':', ColonIndex + 1);
						// if there are no more colons, we're done
						if (NextColonIndex < 0)
						{
							break;
						}

						// get the bit between the colons
						string Substring = OutString.Substring(ColonIndex + 1, (NextColonIndex - ColonIndex) - 1);
						try
						{
							int Number = int.Parse(Substring);
							bFoundNumber = true;

							// modify the string
							OutString = OutString.Replace(":" + Substring + ":", "(" + Substring + "):");
						}
						catch (FormatException)
						{
							// move to the next colon
							ColonIndex = NextColonIndex;
						}
					}
				}

				// output the final string
				Console.WriteLine(OutString);
			}
		}

		public static void OutputReceivedDataEventHandler( Object Sender, DataReceivedEventArgs Line )
		{
			Int32 JavaCIndex = -1;
			Int32 JavaIndex = -1;
			Int32 XmlIndex = -1;
			bool bHasLoggedOut = false;
			if ( ( Line != null ) && ( Line.Data != null ) )
			{
				if ( Line.Data.StartsWith( "Syncing from" ) )
				{
					System.Console.WriteLine( "[Deploy] " + Line.Data );
					SuppressedLog.Add("[Deploy] " + Line.Data);
					bHasLoggedOut = true;
				}
				else if ((JavaCIndex = Line.Data.IndexOf("[setup]")) >= 0)
				{
					if ((JavaIndex = Line.Data.IndexOf("WARNING:")) >= 0 )
					{
						System.Console.WriteLine(Line.Data);
						SuppressedLog.Add(Line.Data);
						bHasLoggedOut = true;
					}
				}
				else if ((JavaCIndex = Line.Data.IndexOf("[javac]")) >= 0)
				{
					if ((JavaIndex = Line.Data.IndexOf(".java:")) >= 0 ||
						(XmlIndex = Line.Data.IndexOf(".xml:")) >= 0)
					{
						int FilePathStart = JavaCIndex + 8;
						int FilePathEnd = (JavaIndex >= 0) ? (JavaIndex + 5) : (XmlIndex + 4);
						string FilePath = Line.Data.Substring(FilePathStart, FilePathEnd - FilePathStart);

						// fix FilePath to point to original location, not the one in Intermediate
						FilePath = FilePath.Replace(JavaSourcePathReplacementFrom, JavaSourcePathReplacementTo);

						string LineNrString = Line.Data.Substring(FilePathEnd + 1);
						int MsgIndex = LineNrString.IndexOf(':');
						if (MsgIndex > 0 && LineNrString.Length > MsgIndex + 2)
						{
							string Message = LineNrString.Substring(MsgIndex + 2);
							LineNrString = LineNrString.Remove(MsgIndex);
							try
							{
								int LineNr = int.Parse(LineNrString);
								System.Console.WriteLine(String.Format("{0}({1}): {2}", FilePath, LineNr, Message));
								SuppressedLog.Add(String.Format("{0}({1}): {2}", FilePath, LineNr, Message));
								bHasLoggedOut = true;
							}
							catch (Exception)
							{
							}
						}
					}
				}

				if (!bHasLoggedOut)
				{
					if ( BuildConfiguration.bPrintDebugInfo )
					{
						System.Console.WriteLine(Line.Data);
					}

					// Add it to the suppressed log, so we can output everything if we encounter a build error later.
					SuppressedLog.Add(Line.Data);
				}
			}
		}

		/**
		 * Called if the APK wasn't created properly.
		 */
		public static void APKErrorHandler( Action A )
		{
			OutputSuppressedLog();
			ClearSuppressedLog();
		}

		public static FileItem BuildAPK( FileItem SOFile, LinkEnvironment LinkEnvironment, UnrealTargetConfiguration Configuration,
			string GameName, string OutputPath)
		{
			// Create an action that invokes ...\Development\Src\Android\Java\build.bat
			string JavaSourceRoot = Path.GetFullPath("..\\..\\Development\\Src\\Android\\java");

			Action APKAction = new Action();
			APKAction.WorkingDirectory = Path.GetDirectoryName(SOFile.AbsolutePath);
			APKAction.CommandPath = "..\\..\\Binaries\\Android\\AndroidPackager.exe";
            APKAction.CommandArguments = " " + GameName + " " + LinkEnvironment.TargetPlatform.ToString() + " " + Configuration.ToString();
			APKAction.bCanExecuteRemotely = false;
			APKAction.bShouldLogIfExecutedByXGE = true;

			// Make UBT check that the produced file was actually produced.
			APKAction.bIsLinker = true;

			// Specify the dependencies (most of them).
			APKAction.PrerequisiteItems.Add(SOFile);
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaApp.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaFileDownloader.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaDownloaderAlarmReceiver.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaPreferences.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaApsalar.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaPhoneHome.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "src\\com\\epicgames\\EpicCitadel\\UE3JavaApplicationInfo.java")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "default.properties")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "AndroidManifest.xml")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "build.xml")));
			APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "res\\values\\strings.xml")));
            APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "res\\xml\\preferences.xml")));
            APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "res\\layout\\main.xml")));
            APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine(JavaSourceRoot, "res\\values\\arrays.xml")));
            APKAction.PrerequisiteItems.Add(FileItem.GetItemByPath(Path.Combine("..\\..\\Binaries\\Android", "AndroidPackager.exe")));

			// Specify the resulting file.
			FileItem OutputFile = FileItem.GetItemByPath(OutputPath);
			APKAction.ProducedItems.Add(OutputFile);
			APKAction.StatusDescription = Path.GetFileName(OutputPath);

			// Capture standard output.
			APKAction.bShouldBlockStandardOutput = true;
			APKAction.OutputEventHandler = new DataReceivedEventHandler(OutputReceivedDataEventHandler);
			APKAction.LinkErrorHandler = new Action.EventHandler(APKErrorHandler);

			// rememeber how to convert paths so that double-clicking on build errors goes to the source (replace
			// the Intermediate directory with the original source directory)
			JavaSourcePathReplacementFrom = APKAction.WorkingDirectory;
			JavaSourcePathReplacementTo = JavaSourceRoot;

			return OutputFile;
		}
	};
}
