/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections;
using System.Collections.Generic;
using System.IO;
using System.Diagnostics;
using System.Security.AccessControl;

namespace UnrealBuildTool
{
	class MacToolChain
	{
		/** Keep a list of remote files that are potentially copied from local to remote */
		private static Dictionary<FileItem, FileItem> CachedRemoteFileItems = new Dictionary<FileItem, FileItem>();

		/***********************************************************************
		 * NOTE:
		 *  Do NOT change the defaults to set your values, instead you should set the environment variables
		 *  properly in your system, as other tools make use of them to work properly!
		 *  The defaults are there simply for examples so you know what to put in your env vars...
		 ***********************************************************************

        /** The name of the MacOS machine to talk to compile the game */
		private static string MacName = Utils.GetStringEnvironmentVariable("ue3.Mac_CompileServerName", "a1487");
		private static string[] PotentialMacNames = {
			"a1487",
			"a1488",
		};

		/** The path (on the Mac) to the your particular development directory, where files will be copied to from the PC */
		private static string UserDevRootMac = "/UnrealEngine3/Builds/";

		/** The directory that this local branch is in, without drive information (strip off X:\ from X:\dev\UnrealEngine3) */
		private static string BranchDirectory = Environment.MachineName + "\\" + Path.GetFullPath("..\\..\\").Substring(3);
		private static string BranchDirectoryMac = BranchDirectory.Replace("\\", "/");

		/** The path (on the Mac) to the your particular development directory, where CreateAppBundle.sh is and Mac app bundle will be moved, if requested */
		private static string UserDevBinariesMac = UserDevRootMac + BranchDirectoryMac + "Binaries/Mac/";

		/** Which version of the Mac OS SDK to target at build time */
		public static string MacOSSDKVersion = "10.7";

		/** Which version of the Mac OS X to allow at run time */
		public static string MacOSVersion = "10.6";

		/** Which developer directory to root from */
//10.6 		private static string DeveloperDir = "/Developer/";
//10.6 		private static string ToolchainDir = DeveloperDir;
//10.6 		private static string SDKDir = DeveloperDir + "SDKs/MacOSX" + MacOSSDKVersion + ".sdk";
		private static string DeveloperDir = "/Applications/Xcode.app/Contents/Developer/";
 		private static string ToolchainDir = DeveloperDir + "Toolchains/XcodeDefault.xctoolchain/";
		private static string SDKDir = DeveloperDir + "Platforms/MacOSX.platform/Developer/SDKs/MacOSX" + MacOSSDKVersion + ".sdk";

		/** Which compiler frontend to use */
        private static string MacCompiler;

		/** Which linker frontend to use */
        private static string MacLinker;

        /** Substrings that indicate a line contains an error */
        protected static List<string> ErrorMessageTokens;

		/** An array of files to RPCUtility for the RPCBatchUpload command */
		private static List<string> BatchUploadCommands = new List<string>();

        static MacToolChain()
        {
 			MacCompiler = "clang++";
 			MacLinker = "clang++";

            ErrorMessageTokens = new List<string>();
            ErrorMessageTokens.Add("ERROR ");
            ErrorMessageTokens.Add("** BUILD FAILED **");
            ErrorMessageTokens.Add("[BEROR]");
            ErrorMessageTokens.Add("IPP ERROR");
            ErrorMessageTokens.Add("System.Net.Sockets.SocketException");
        }

        // Do any one-time, global initialization for the tool chain
		public static void SetUpGlobalEnvironment()
		{
			// If we don't care which machine we're going to build on, query and
			// pick the one with the most free command slots available
			if (MacName == "best_available")
			{
				string MostAvailableName = "";
				Int32 MostAvailableCount = Int32.MinValue;
				foreach (string NextMacName in PotentialMacNames)
				{
					Int32 NextAvailableCount = GetAvailableCommandSlotCount(NextMacName);
					if (NextAvailableCount > MostAvailableCount)
					{
						MostAvailableName = NextMacName;
						MostAvailableCount = NextAvailableCount;
					}
				}
				if (BuildConfiguration.bPrintDebugInfo)
				{
					Console.WriteLine("Picking the compile server with the most available command slots: " + MostAvailableName);
				}
				// Finally, assign the name of the Mac we're going to use
				MacName = MostAvailableName;
			}

			// crank up RPC communications
			RPCUtilHelper.Initialize(MacName);
		}

		static string GetCompileArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			Result += " -fmessage-length=0";
			Result += " -pipe";
			Result += " -Wno-trigraphs";
			Result += " -fpascal-strings";
			Result += " -mdynamic-no-pic";
			Result += " -Wreturn-type";
			Result += " -Wno-format";
			Result += " -Wno-return-type-c-linkage";
			Result += " -fexceptions";

			Result += " -Wno-unused-value";
			Result += " -Wno-switch";
			Result += " -Wno-switch-enum";
			Result += " -Wno-switch";
			Result += " -Wno-logical-op-parentheses";	// needed for external headers we shan't change
			Result += " -Wno-null-arithmetic";			// needed for external headers we shan't change
			Result += " -Wno-constant-logical-operand"; // needed for "&& GIsEditor" failing due to it being a constant
// @HACK
Result += " -Wno-deprecated-declarations"; // TEMPORARY: Disables warnings for deprecated functions

			Result += " -Werror";
			Result += " -fasm-blocks";
			Result += " -ffast-math";
			Result += " -c";

			Result += " -arch x86_64";
			Result += " -isysroot " + SDKDir;
			Result += " -mmacosx-version-min=" + MacOSVersion;

			// Optimize non- debug builds.
			if (CompileEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug)
			{
				Result += " -O3";
			}
			else
			{
				Result += " -O0";
			}

			// Create DWARF format debug info if wanted,
			if (CompileEnvironment.bCreateDebugInfo)
			{
				Result += " -gdwarf-2";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x c++";
			Result += " -fno-rtti";
			Result += " -std=c++0x";
			return Result;
		}

		static string GetCompileArguments_MM()
		{
			string Result = "";
			Result += " -x objective-c++";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -fno-rtti";
			Result += " -std=c++0x";
			return Result;
		}

		static string GetCompileArguments_M()
		{
			string Result = "";
			Result += " -x objective-c";
			Result += " -fobjc-abi-version=2";
			Result += " -fobjc-legacy-dispatch";
			Result += " -std=gnu99";
			return Result;
		}

		static string GetCompileArguments_C()
		{
			string Result = "";
			Result += " -x c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x c++-header";
            Result += " -fno-rtti";
			Result += " -std=c++0x";
            return Result;
		}

		static string GetLinkArguments_Global(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			Result += " -arch x86_64";
			Result += " -isysroot " + SDKDir;
			Result += " -mmacosx-version-min=" + MacOSVersion;
			Result += " -dead_strip";

			// link in the frameworks
			foreach (string Framework in LinkEnvironment.Frameworks)
			{
				Result += " -framework " + Framework;
			}
			foreach (string Framework in LinkEnvironment.WeakFrameworks)
			{
				Result += " -weak_framework " + Framework;
			}

			return Result;
		}

		/**
		 * @return The Mac version of the path to get to the Development/Src directory from which UBT expects to run
		 */
		static string GetMacDevSrcRoot()
		{
			return UserDevRootMac + BranchDirectoryMac + "Development/Src/";
		}

		/**
		 * @return The index of root directory of the path, assumed to be rooted with either Development or Binaries
		 */
		static int RootDirectoryLocation(string LocalPath)
		{
			int RootDirLocation = LocalPath.LastIndexOf("Development", StringComparison.InvariantCultureIgnoreCase);
			if (RootDirLocation < 0)
			{
				RootDirLocation = LocalPath.LastIndexOf("Binaries", StringComparison.InvariantCultureIgnoreCase);
			}
			return RootDirLocation;
		}

		/**
		 * Converts a filename from local PC path to what the path as usable on the Mac
		 *
		 * @param LocalPath Absolute path of the PC file
		 * @param bIsHomeRelative If TRUE, the returned path will be usable from where the SSH logs into (home), if FALSE, it will be relative to the BranchRoot
		 */
		public static string LocalToMacPath(string LocalPath, bool bIsHomeRelative)
		{
			string MacPath = "";
			// Move from home to branch root if home relative
			if (bIsHomeRelative)
			{
				MacPath += GetMacDevSrcRoot();
			}

			// In the case of paths from the PC to the Mac over a UNC path, peel off the possible roots
			string StrippedPath = LocalPath.Replace( BranchDirectory, "" );

			// Now, reduce the path down to just relative to Development\Src
			MacPath += "../../" + StrippedPath.Substring( RootDirectoryLocation( StrippedPath ) );

			// Replace back slashes with forward for the Mac
			return MacPath.Replace("\\", "/");
		}

		/**
		 * Track this file for batch uploading
		 */
		static public void QueueFileForBatchUpload(FileItem LocalFileItem)
		{
			// If not, create it now
			string RemoteFilePath = LocalToMacPath(LocalFileItem.AbsolutePath, true);

			// add this file to the list of files we will RPCBatchUpload later
			string Entry = LocalFileItem.AbsolutePath + " " + RemoteFilePath;
			if (!BatchUploadCommands.Contains(Entry))
			{
				BatchUploadCommands.Add(Entry);
			}
		}

		/**
		 * Creates a FileItem descriptor for a remote file, and can optionally make an action to copy if needed
		 * 
		 * @param LocalFileItem Local file descriptor
		 * @param bShouldUpload If TRUE, this 
		 * 
		 * @return File descriptor for the remote file on the Mac
		 */
		static public FileItem LocalToRemoteFileItem(FileItem LocalFileItem, bool bShouldUpload)
		{
			FileItem RemoteFileItem;

			// Look to see if we've already made a remote FileItem for this local FileItem
			if( !CachedRemoteFileItems.TryGetValue( LocalFileItem, out RemoteFileItem ) )
			{
				// If not, create it now
				string RemoteFilePath = LocalToMacPath(LocalFileItem.AbsolutePath, true);
				RemoteFileItem = FileItem.GetRemoteItemByPath(RemoteFilePath, UnrealTargetPlatform.Mac);

				// Is shadowing requested?
				if(bShouldUpload)
				{
					QueueFileForBatchUpload(LocalFileItem);
				}

				CachedRemoteFileItems.Add( LocalFileItem, RemoteFileItem );
			}

			return RemoteFileItem;
		}

		public static void CompileOutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
			{

				bool bWasHandled = false;
				// does it look like an error? something like this:
				//     2>Core/Inc/UnStats.h:478:3: error: no matching constructor for initialization of 'FStatCommonData'

				try
				{
					// if we split on colon, an error will have at least 4 tokens
					string[] Tokens = Line.Data.Split(":".ToCharArray());
					if (Tokens.Length > 3)
					{
						string Filename = Tokens[0];
						// source files have a harccoded Mac path, but contain some crazy relative path like:
						// /UnrealEngine3/Builds/D1150/dev/UnrealEngine3/Development/Src/../../Development/Src/Core/Src/UnStats.cpp
						const string DevPath = "Development/Src/../../Development/Src/";
						int DevLocation = Filename.IndexOf(DevPath);
						if (DevLocation != -1)
						{
							// skip over the extraneous part, to make it look like the header version above (about 10 lines up)
							Filename = Filename.Substring(DevLocation + DevPath.Length);
						}
						Filename = Path.GetFullPath(Filename);

						// build up the final string
						string Output = string.Format("{0}({1}) : ", Filename, Tokens[1]);
						for (int T = 3; T < Tokens.Length; T++)
						{
							Output += Tokens[T];
							if (T < Tokens.Length - 1)
							{
								Output += ":";
							}
						}

						// output the result
						Console.WriteLine(Output);
						bWasHandled = true;
					}
				}
				catch (Exception)
				{
					bWasHandled = false;
				}

				// write if not properly handled
				if (!bWasHandled)
				{
					Console.WriteLine(Line.Data);
				}
			}
		}

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
		{
			string Arguments = GetCompileArguments_Global(CompileEnvironment);
			string PCHArguments = "";

			if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				PCHArguments += string.Format(" -include \"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace(".gch", ""));
			}

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.IncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				Arguments += string.Format(" -I\"{0}\"", IncludePath);
			}

			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" -D\"{0}\"", Definition);
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action();
				string FileArguments = "";
				string Extension = Path.GetExtension(SourceFile.AbsolutePathUpperInvariant);

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Compile the file as a C++ PCH.
					FileArguments += GetCompileArguments_PCH();
				}
				else if (Extension == ".C")
				{
					// Compile the file as C code.
					FileArguments += GetCompileArguments_C();
				}
				else if (Extension == ".MM")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_MM();
				}
				else if (Extension == ".M")
				{
					// Compile the file as Objective-C++ code.
					FileArguments += GetCompileArguments_M();
				}
				else
				{
					// Compile the file as C++ code.
					FileArguments += GetCompileArguments_CPP();

					// only use PCH for .cpp files
					FileArguments += PCHArguments;
				}

				// Add the C++ source file and its included files to the prerequisite item list.
				QueueFileForBatchUpload(SourceFile);
				CompileAction.PrerequisiteItems.Add(SourceFile);
				foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
				{
					QueueFileForBatchUpload(IncludedFile);
					CompileAction.PrerequisiteItems.Add(IncludedFile);
				}

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ".gch"
							)
						);

					FileItem RemotePrecompiledHeaderFile = LocalToRemoteFileItem(PrecompiledHeaderFile, false);
					CompileAction.ProducedItems.Add(RemotePrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = RemotePrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", RemotePrecompiledHeaderFile.AbsolutePath);
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
					}

					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ".o"
							)
						);

					FileItem RemoteObjectFile = LocalToRemoteFileItem(ObjectFile, false);
					CompileAction.ProducedItems.Add(RemoteObjectFile);
					Result.ObjectFiles.Add(RemoteObjectFile);
					FileArguments += string.Format(" -o \"{0}\"", RemoteObjectFile.AbsolutePath);
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format(" {0}", LocalToMacPath(SourceFile.AbsolutePath, false));


				string CompilerPath = ToolchainDir + "usr/bin/" + MacCompiler;

				CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
			}
			return Result;
		}

		public static FileItem LinkFiles(LinkEnvironment LinkEnvironment)
		{
			string LinkerPath = ToolchainDir + "usr/bin/" + MacLinker;

			// Create an action that invokes the linker.
			Action LinkAction = new Action();
			LinkAction.bIsLinker = true;
			LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = LinkerPath;
			LinkAction.CommandArguments = GetLinkArguments_Global(LinkEnvironment);

			// Add the library paths to the argument list.
			foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
			{
				LinkAction.CommandArguments += string.Format(" -L\"{0}\"", LibraryPath);
			}

			// Add the additional libraries to the argument list.
			foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				LinkAction.CommandArguments += string.Format(" -l{0}", AdditionalLibrary);
			}

			// Add any additional files that we'll need in order to link the app
			foreach (string AdditionalShadowFile in LinkEnvironment.AdditionalShadowFiles)
			{
				FileItem ShadowFile = FileItem.GetExistingItemByPath(AdditionalShadowFile);
				if (ShadowFile != null)
				{
					QueueFileForBatchUpload(ShadowFile);
					LinkAction.PrerequisiteItems.Add(ShadowFile);
				}
				else
				{
					throw new BuildException("Couldn't find required additional file to shadow: {0}", AdditionalShadowFile);
				}
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(Path.GetFullPath(LinkEnvironment.OutputFilePath));
			FileItem RemoteOutputFile = LocalToRemoteFileItem(OutputFile, false);
			LinkAction.ProducedItems.Add(RemoteOutputFile);
			LinkAction.StatusDescription = string.Format( "{0}", OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			string ResponsePath = Path.GetFullPath("..\\Intermediate\\Mac\\LinkFileList.tmp");
			ResponseFile.Create(ResponsePath, InputFileNames);
			RPCUtilHelper.CopyFile(ResponsePath, LocalToMacPath(ResponsePath, true), true);
			LinkAction.CommandArguments += string.Format(" @\"{0}\"", LocalToMacPath(ResponsePath, true));

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" -o \"{0}\"", RemoteOutputFile.AbsolutePath);

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return RemoteOutputFile;
		}

		/**
		 * Generates debug info for a given executable
		 * 
		 * @param Executable FileItem describing the executable to generate debug info for
		 */
		static public FileItem GenerateDebugInfo(FileItem Executable)
		{
			// Make a file item for the source and destination files
			string FullDestPathRoot = Executable.AbsolutePath + ".app.dSYM";
			string FullDestPath = FullDestPathRoot;
			FileItem DestFile = FileItem.GetRemoteItemByPath(FullDestPath, UnrealTargetPlatform.Mac);

			// Make the compile action
			Action GenDebugAction = new Action();
			GenDebugAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			GenDebugAction.WorkingDirectory = GetMacDevSrcRoot();
			GenDebugAction.CommandPath = "sh";

			// note that the source and dest are switched from a copy command
			GenDebugAction.CommandArguments = string.Format("-c '{0}/usr/bin/dsymutil {1} -o {2}; cd {2}/..; zip -r -y -1 {3}.app.dSYM.zip {3}.app.dSYM'",
				DeveloperDir,
				Executable.AbsolutePath,
				FullDestPathRoot,
				Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.PrerequisiteItems.Add(Executable);
			GenDebugAction.ProducedItems.Add(DestFile);
			GenDebugAction.StatusDescription = string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		/**
		 * Creates app bundle for a given executable
		 * 
		 * @param Executable FileItem describing the executable to generate app bundle for
		 */
		static public FileItem CreateAppBundle(FileItem Executable, string GameName)
		{
			// Make a file item for the source and destination files
			string FullDestPathRoot = Executable.AbsolutePath + ".app";
			string FullDestPath = FullDestPathRoot;
			FileItem DestFile = FileItem.GetItemByPath(FullDestPath);

			// Make the compile action
			Action CreateAppBundleAction = new Action();
			CreateAppBundleAction.WorkingDirectory = Path.GetFullPath("..\\..\\Binaries");
			CreateAppBundleAction.CommandPath = CreateAppBundleAction.WorkingDirectory + "\\RPCUtility.exe";

			// note that the source and dest are switched from a copy command
			CreateAppBundleAction.CommandArguments = string.Format("{0} {1} sh {2}/CreateAppBundle.sh {2} {3} {4} {5} {6} {7}",
				MacName,
				GetMacDevSrcRoot(),
				LocalToMacPath(Path.GetDirectoryName(Executable.AbsolutePath), false),
				GameName,
				Path.GetFileName(Executable.AbsolutePath),
				MacOSSDKVersion,
				MacOSSDKVersion,
				DeveloperDir);
			CreateAppBundleAction.PrerequisiteItems.Add(Executable);
			CreateAppBundleAction.ProducedItems.Add(DestFile);
			CreateAppBundleAction.StatusDescription = string.Format("Creating app bundle {0}.app", Path.GetFileName(Executable.AbsolutePath));
			CreateAppBundleAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		/**
		 * Queries the remote compile server for CPU information
		 * and computes the proper ProcessorCountMultiplier.
		 */
		static private Int32 RemoteCPUCount = Environment.ProcessorCount;
		static public void OutputReceivedForSystemInfoQuery(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
			{
				Int32 TestValue = 0;
				if (Int32.TryParse(Line.Data, out TestValue))
				{
					RemoteCPUCount = TestValue;
				}
				else
				{
					if (BuildConfiguration.bPrintDebugInfo)
					{
						Console.WriteLine("Warning: Unexpected output from remote Mac system info query:");
						Console.WriteLine(Line.Data);
					}
				}
			}
		}

		static public Double GetAdjustedProcessorCountMultiplier()
		{
			Process QueryProcess = new Process();
			QueryProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\..\\Binaries");
			QueryProcess.StartInfo.FileName = QueryProcess.StartInfo.WorkingDirectory + "\\RPCUtility.exe";
			QueryProcess.StartInfo.Arguments = string.Format("{0} {1} sysctl -n hw.ncpu",
				MacName,
				UserDevRootMac);
			QueryProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedForSystemInfoQuery);
			QueryProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedForSystemInfoQuery);

			// Try to launch the query's process, and produce a friendly error message if it fails.
			Utils.RunLocalProcess(QueryProcess);

			Double AdjustedMultiplier = (Double)RemoteCPUCount / (Double)Environment.ProcessorCount;
			if (BuildConfiguration.bPrintDebugInfo)
			{
				Console.WriteLine("Adjusting the remote Mac compile process multiplier to " + AdjustedMultiplier.ToString());
			}
			return AdjustedMultiplier;
		}

		/**
		 * Simple query for getting the number of available command slots on a named Mac
		 */
		static private Int32 RemoteAvailableCommandSlotCount = Int32.MinValue;
		static public void OutputReceivedForAvailableCommandSlotQuery(Object Sender, DataReceivedEventArgs Line)
		{
			if ((Line != null) && (Line.Data != null) && (Line.Data != ""))
			{
				Int32 TestValue = 0;
				if (Int32.TryParse(Line.Data, out TestValue))
				{
					RemoteAvailableCommandSlotCount = TestValue;
				}
				else
				{
					if (BuildConfiguration.bPrintDebugInfo)
					{
						Console.WriteLine("Warning: Unexpected output from remote Mac available command slot query:");
						Console.WriteLine(Line.Data);
					}
				}
			}
		}
		static public Int32 GetAvailableCommandSlotCount(string TargetMacName)
		{
			Process QueryProcess = new Process();
			QueryProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\..\\Binaries");
			QueryProcess.StartInfo.FileName = QueryProcess.StartInfo.WorkingDirectory + "\\RPCUtility.exe";
			QueryProcess.StartInfo.Arguments = string.Format("{0} / rpc:command_slots_available",
				TargetMacName);
			QueryProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedForAvailableCommandSlotQuery);
			QueryProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedForAvailableCommandSlotQuery);

			// Try to launch the query's process, and produce a friendly error message if it fails.
			RemoteAvailableCommandSlotCount = Int32.MinValue + 1;

			Utils.RunLocalProcess(QueryProcess);

			if (BuildConfiguration.bPrintDebugInfo)
			{
				Console.WriteLine("Available command slot count for " + TargetMacName + " is " + RemoteAvailableCommandSlotCount.ToString());
			}
			return RemoteAvailableCommandSlotCount;
		}

        /**
         * Helper function to sync source files to and from the local system and a remote Mac
         */
        public static bool OutputReceivedDataEventHandlerEncounteredError = false;
        public static string OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
        public static void OutputReceivedDataEventHandler(Object Sender, DataReceivedEventArgs Line)
        {
            if ((Line != null) && (Line.Data != null))
            {
                Console.WriteLine(Line.Data);

                foreach (string ErrorToken in ErrorMessageTokens)
                {
                    if (Line.Data.Contains(ErrorToken))
                    {
                        OutputReceivedDataEventHandlerEncounteredError = true;
                        OutputReceivedDataEventHandlerEncounteredErrorMessage += Line.Data;
                        break;
                    }
                }
            }
        }

		public static void SyncHelper(string GameName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string LocalShadowDirectoryRoot, bool bPreBuild)
		{
			string LocalShadowDirectory = Path.GetFullPath(LocalShadowDirectoryRoot);
			string RemoteShadowDirectory = UserDevRootMac + BranchDirectoryMac;

            if (bPreBuild)
            {
				// batch upload
				RPCUtilHelper.BatchUpload(BatchUploadCommands.ToArray());

				// we can now clear out the set of files
				BatchUploadCommands.Clear();
            }
            else
            {
                string BinariesPathSuffix = "Binaries\\Mac\\";
                string DecoratedGameName = GameName + "-Mac-" + Configuration;
                RPCUtilHelper.CopyFile(
					RemoteShadowDirectory + "/Binaries/Mac/" + DecoratedGameName,
                    Path.Combine(Path.Combine(Path.GetFullPath("..\\.."), BinariesPathSuffix), DecoratedGameName),
                    false);
                string StubName = DecoratedGameName + ".app.stub";
				RPCUtilHelper.CopyFile(
					RemoteShadowDirectory + "/Binaries/Mac/" + StubName,
                    Path.Combine(Path.Combine(Path.GetFullPath("..\\.."), BinariesPathSuffix), StubName),
                    false);

                if (BuildConfiguration.bGeneratedSYMFile == true)
                {
                    string DSYMName = DecoratedGameName + ".app.dSYM.zip";
					RPCUtilHelper.CopyFile(
						RemoteShadowDirectory + "/Binaries/Mac/" + DSYMName,
                        Path.Combine(Path.Combine(Path.GetFullPath("..\\.."), BinariesPathSuffix), DSYMName),
                        false);
                }
            }
		}
	};
}
