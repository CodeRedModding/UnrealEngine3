/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Text;

namespace UnrealBuildTool
{
	class IPhoneToolChain
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
		public static string MacName = Utils.GetStringEnvironmentVariable( "ue3.iPhone_CompileServerName", "a1487" );
		public static string[] PotentialMacNames = {
			"a1487",
			"a1488",
		};

		/** The path (on the Mac) to the your particular development directory, where files will be copied to from the PC */
		public static string UserDevRootMac = "/UnrealEngine3/Builds/";

		/** The directory that this local branch is in, without drive information (strip off X:\ from X:\dev\UnrealEngine3) */
		public static string BranchDirectory = Environment.MachineName + "\\" + Path.GetFullPath("..\\..\\").Substring(3);
		public static string BranchDirectoryMac = BranchDirectory.Replace("\\", "/");

		/** Which version of the iOS SDK to target at build time */
		public static string IPhoneSDKVersion = Utils.GetStringEnvironmentVariable("ue3.iPhone_SdkVersion", "6.0" );

		/** Which version of the iOS to allow at run time (these two following MUST MATCH) */
		public static string IPhoneOSVersion = "4.3";
		public static string IPhoneOSVersionDefine = "40300";

		/** What devices to support (only needed for XcodeToolchain) - 1 = iPhone/iPoad, 2 = iPad */
		public static string DeviceFamilies = "1,2";

		/** Which developer directory to root from */
		private static string DeveloperDir;

		/** Where the compiler and linker toolchain lives */
		private static string ToolchainDir;

		/** Which compiler frontend to use */
		private static string IPhoneCompiler;

		/** Which linker frontend to use */
		private static string IPhoneLinker;

        /** Substrings that indicate a line contains an error */
        protected static List<string> ErrorMessageTokens;

		/** An array of files to RPCUtility for the RPCBatchUpload command */
		private static List<string> BatchUploadCommands = new List<string>();

		static IPhoneToolChain()
		{
			DeveloperDir = Utils.GetEnvironmentVariable("ue3.XcodeDeveloperDir", "/Applications/Xcode.app/Contents/Developer/");

			if (IPhoneSDKVersion == "6.0")
			{
				ToolchainDir = DeveloperDir + "Toolchains/XcodeDefault.xctoolchain/";
			}
			else
			{
				ToolchainDir = DeveloperDir + "Platforms/iPhoneOS.platform/Developer/";
			}

			IPhoneCompiler = "clang++";
			IPhoneLinker = "clang++";

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
			if( MacName == "best_available" )
			{
				string MostAvailableName = "";
				Int32 MostAvailableCount = Int32.MinValue;
				foreach( string NextMacName in PotentialMacNames )
				{
					Int32 NextAvailableCount = GetAvailableCommandSlotCount( NextMacName );
					if( NextAvailableCount > MostAvailableCount )
					{
						MostAvailableName = NextMacName;
						MostAvailableCount = NextAvailableCount;
					}
				}
				if( BuildConfiguration.bPrintDebugInfo )
				{
					Console.WriteLine( "Picking the compile server with the most available command slots: " + MostAvailableName );
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
			Result += " -fno-exceptions";
			if (IPhoneCompiler == "clang" || IPhoneCompiler == "clang++")
			{
				Result += " -Wno-return-type-c-linkage"; //clang - needed for NxCooking.h
				Result += " -Wno-unused-value"; // clang
				Result += " -Wno-switch-enum"; // clang
                Result += " -Wno-switch";
                Result += " -Wno-objc-redundant-literal-use";
				Result += " -Wno-logical-op-parentheses"; // clang - needed for external headers we shan't change
				Result += " -Wno-constant-logical-operand"; // clang - needed for "&& GIsEditor" failing due to it being a constant
				Result += " -DCOMPILED_WITH_CLANG=1";
			}
			Result += " -Werror";
			Result += " -c";
			Result += " -D__IPHONE_OS_VERSION_MIN_REQUIRED=" + IPhoneOSVersionDefine;

			Result += " -arch armv7";
			Result += " -isysroot " + DeveloperDir + "Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS" + IPhoneSDKVersion + ".sdk";
			Result += " -miphoneos-version-min=" + IPhoneOSVersion;
			Result += " -mno-thumb";

			Result += BuildConfiguration.bUseMallocProfiler || !BuildConfiguration.bOmitFramePointers ? " -fno-fomit-frame-pointer" : " -omit-frame-pointer";

			// Optimize non- debug builds.
			if( CompileEnvironment.TargetConfiguration != CPPTargetConfiguration.Debug )
			{
				Result += " -O3";
			}
			else
			{
				Result += " -O0";
			}

			// Create DWARF format debug info if wanted,
			if( CompileEnvironment.bCreateDebugInfo )
			{
				Result += " -gdwarf-2";
			}

			return Result;
		}

		static string GetCompileArguments_CPP()
		{
			string Result = "";
			Result += " -x c++";
//			Result += " -x objective-c++";
			Result += " -fno-rtti";
//			Result += " -stdlib=libc++";
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
//			Result += " -stdlib=libc++";
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
//			Result += " -x objective-c";
			return Result;
		}

		static string GetCompileArguments_PCH()
		{
			string Result = "";
			Result += " -x c++-header";
//			Result += " -x objective-c++-header";
			Result += " -fno-rtti";
//			Result += " -stdlib=libc++";
			Result += " -std=c++0x";
			return Result;
		}

		static string GetLinkArguments_Global( LinkEnvironment LinkEnvironment )
		{
			string Result = "";
			Result += " -arch armv7";
			Result += " -isysroot " + DeveloperDir + "Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS" + IPhoneSDKVersion + ".sdk";
 			Result += " -dead_strip";
//			Result += " -v";
//			Result += " -stdlib=libc++";
			Result += " -miphoneos-version-min=" + IPhoneOSVersion;

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
		public static string GetMacDevSrcRoot()
		{
			return UserDevRootMac + BranchDirectoryMac + "Development/Src/";
		}

		/**
		 * @return The index of root directory of the path, assumed to be rooted with either Development or Binaries
		 */
		static int RootDirectoryLocation( string LocalPath )
		{
			// By default, assume the filename is already stripped down and the root is at zero
			int RootDirLocation = 0;

			string UBTRootPath = Path.GetFullPath( AppDomain.CurrentDomain.BaseDirectory + "..\\..\\..\\..\\" );
			if( LocalPath.ToUpperInvariant().Contains( UBTRootPath.ToUpperInvariant() ) )
			{
				// If the file is a full path name and rooted at the same location as UBT,
				// use that location as the root and simply return the length
				RootDirLocation = UBTRootPath.Length;
			}

			return RootDirLocation;
		}


		/**
		 * Converts a filename from local PC path to what the path as usable on the Mac
		 *
		 * @param LocalPath Absolute path of the PC file
		 * @param bIsHomeRelative If TRUE, the returned path will be usable from where the SSH logs into (home), if FALSE, it will be relative to the BranchRoot
		 */
		public static string LocalToMacPath( string LocalPath, bool bIsHomeRelative )
		{
			string MacPath = "";
			// Move from home to branch root if home relative
			if( bIsHomeRelative )
			{
				MacPath += GetMacDevSrcRoot();
			}

			// In the case of paths from the PC to the Mac over a UNC path, peel off the possible roots
			string StrippedPath = LocalPath.Replace( BranchDirectory, "" );

			// Now, reduce the path down to just relative to Development\Src
			MacPath += "../../" + StrippedPath.Substring( RootDirectoryLocation( StrippedPath ) );

			// Replace back slashes with forward for the Mac
			return MacPath.Replace( "\\", "/" );
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
				RemoteFileItem = FileItem.GetRemoteItemByPath(RemoteFilePath, UnrealTargetPlatform.IPhone);

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
			string Arguments = GetCompileArguments_Global( CompileEnvironment );
			string PCHArguments = "";

			if( CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include )
			{
				// Add the precompiled header file's path to the include path so GCC can find it.
				// This needs to be before the other include paths to ensure GCC uses it instead of the source header file.
				PCHArguments += string.Format( " -include \"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath.Replace( ".gch", "" ) );
			}

			// Add include paths to the argument list.
			foreach( string IncludePath in CompileEnvironment.IncludePaths )
			{
				Arguments += string.Format( " -I\"{0}\"", IncludePath );
			}
			foreach( string IncludePath in CompileEnvironment.SystemIncludePaths )
			{
				Arguments += string.Format( " -I\"{0}\"", IncludePath );
			}

			foreach( string Definition in CompileEnvironment.Definitions )
			{
				Arguments += string.Format( " -D\"{0}\"", Definition );
			}

			CPPOutput Result = new CPPOutput();
			// Create a compile action for each source file.
			foreach( FileItem SourceFile in SourceFiles )
			{
				Action CompileAction = new Action();
				string FileArguments = "";
				string Extension = Path.GetExtension( SourceFile.AbsolutePathUpperInvariant );

				if( CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create )
				{
					// Compile the file as a C++ PCH.
					FileArguments += GetCompileArguments_PCH();
				}
				else if( Extension == ".C" )
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
				CompileAction.PrerequisiteItems.Add( SourceFile );
				foreach( FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies( SourceFile ) )
				{
					QueueFileForBatchUpload(IncludedFile);
					CompileAction.PrerequisiteItems.Add( IncludedFile );
				}

				if( CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create )
				{
					// Add the precompiled header file to the produced item list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName( SourceFile.AbsolutePath ) + ".gch"
							)
						);

					FileItem RemotePrecompiledHeaderFile = LocalToRemoteFileItem( PrecompiledHeaderFile, false );
					CompileAction.ProducedItems.Add( RemotePrecompiledHeaderFile );
					Result.PrecompiledHeaderFile = RemotePrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" -o \"{0}\"", RemotePrecompiledHeaderFile.AbsolutePath);
				}
				else
				{
					if( CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include )
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add( CompileEnvironment.PrecompiledHeaderFile );
					}

					// Add the object file to the produced item list.
					FileItem ObjectFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName( SourceFile.AbsolutePath ) + ".o"
							)
						);

					FileItem RemoteObjectFile = LocalToRemoteFileItem( ObjectFile, false );
					CompileAction.ProducedItems.Add( RemoteObjectFile );
					Result.ObjectFiles.Add( RemoteObjectFile );
					FileArguments += string.Format(" -o \"{0}\"", RemoteObjectFile.AbsolutePath);
				}

				// Add the source file path to the command-line.
				FileArguments += string.Format( " {0}", LocalToMacPath( SourceFile.AbsolutePath, false ) );

				//string CompilerPath = DeveloperDir + "Platforms/iPhoneOS.platform/Developer/usr/bin/" + IPhoneCompiler;
                string CompilerPath = ToolchainDir + "usr/bin/" + IPhoneCompiler;

				CompileAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
				// RPC utility parameters are in terms of the Mac side
				CompileAction.WorkingDirectory = GetMacDevSrcRoot();
				CompileAction.CommandPath = CompilerPath;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.StatusDescription = string.Format( "{0}", Path.GetFileName( SourceFile.AbsolutePath ) );
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.OutputEventHandler = new DataReceivedEventHandler(CompileOutputReceivedDataEventHandler);
				CompileAction.bIsGCCCompiler = true;
				// We're already distributing the command by execution on Mac.
				CompileAction.bCanExecuteRemotely = false;
			}
			return Result;
		}

		public static FileItem LinkFiles( LinkEnvironment LinkEnvironment )
		{
			//string LinkerPath = DeveloperDir + "Platforms/iPhoneOS.platform/Developer/usr/bin/" + IPhoneLinker;
            string LinkerPath = ToolchainDir + "usr/bin/" + IPhoneLinker;


			// Create an action that invokes the linker.
			Action LinkAction = new Action();
			LinkAction.bIsLinker = true;
			LinkAction.ActionHandler = new Action.BlockingActionHandler(RPCUtilHelper.RPCActionHandler);
			// RPC utility parameters are in terms of the Mac side
			LinkAction.WorkingDirectory = GetMacDevSrcRoot();
			LinkAction.CommandPath = LinkerPath;

			// build this up over the rest of the function
			LinkAction.CommandArguments = GetLinkArguments_Global( LinkEnvironment );

			// Add the library paths to the argument list.
			foreach( string LibraryPath in LinkEnvironment.LibraryPaths )
			{
				LinkAction.CommandArguments += string.Format( " -L\"{0}\"", LibraryPath );
			}

			// Add the additional libraries to the argument list.
			foreach( string AdditionalLibrary in LinkEnvironment.AdditionalLibraries )
			{
				LinkAction.CommandArguments += string.Format( " -l{0}", AdditionalLibrary );
			}

			// Add any additional files that we'll need in order to link the app
			foreach( string AdditionalShadowFile in LinkEnvironment.AdditionalShadowFiles )
			{
				FileItem ShadowFile = FileItem.GetExistingItemByPath( AdditionalShadowFile );
				if( ShadowFile != null )
				{
					QueueFileForBatchUpload(ShadowFile);
					LinkAction.PrerequisiteItems.Add( ShadowFile );
				}
				else
				{
					throw new BuildException( "Couldn't find required additional file to shadow: {0}", AdditionalShadowFile );
				}
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath( Path.GetFullPath( LinkEnvironment.OutputFilePath ) );
			FileItem RemoteOutputFile = LocalToRemoteFileItem( OutputFile, false );
			LinkAction.ProducedItems.Add( RemoteOutputFile );
			LinkAction.StatusDescription = string.Format( "{0}", OutputFile.AbsolutePath);

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach( FileItem InputFile in LinkEnvironment.InputFiles )
			{
				InputFileNames.Add( string.Format( "\"{0}\"", InputFile.AbsolutePath) );
				LinkAction.PrerequisiteItems.Add( InputFile );
			}

			// Write the list of input files to a response file, with a tempfilename, on remote machine
			string ResponsePath = Path.GetFullPath( "..\\Intermediate\\IPhone\\LinkFileList.tmp" );
			ResponseFile.Create(ResponsePath, InputFileNames);
			RPCUtilHelper.CopyFile(ResponsePath, LocalToMacPath(ResponsePath, true), true);


			LinkAction.CommandArguments += string.Format( " @\"{0}\"", LocalToMacPath( ResponsePath, true ) );

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format( " -o \"{0}\"", RemoteOutputFile.AbsolutePath );

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
			FileItem DestFile = FileItem.GetRemoteItemByPath(FullDestPath, UnrealTargetPlatform.IPhone);

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
			GenDebugAction.StatusDescription = GenDebugAction.CommandArguments;// string.Format("Generating debug info for {0}", Path.GetFileName(Executable.AbsolutePath));
			GenDebugAction.bCanExecuteRemotely = false;

			return DestFile;
		}

		/**
		 * Queries the remote compile server for CPU information
		 * and computes the proper ProcessorCountMultiplier.
		 */
		static private Int32 QueryResult = 0;
		static public void OutputReceivedForQuery(Object Sender, DataReceivedEventArgs Line)
		{
			if( ( Line != null ) && ( Line.Data != null ) && ( Line.Data != "" ) )
			{
				Int32 TestValue = 0;
				if( Int32.TryParse( Line.Data, out TestValue ) )
				{
					QueryResult = TestValue;
				}
				else
				{
					if( BuildConfiguration.bPrintDebugInfo )
					{
						Console.WriteLine( "Warning: Unexpected output from remote Mac system info query:" );
						Console.WriteLine( Line.Data );
					}
				}
			}
		}
		static public Int32 QueryRemoteMachine(string MachineName, string Command)
		{
			// we must run the commandline RPCUtility, because we could run this before we have opened up the RemoteRPCUtlity
			Process QueryProcess = new Process();
			QueryProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\..\\Binaries");
			QueryProcess.StartInfo.FileName = QueryProcess.StartInfo.WorkingDirectory + "\\RPCUtility.exe";
			QueryProcess.StartInfo.Arguments = string.Format( "{0} {1} sysctl -n hw.ncpu",
				MachineName,
				UserDevRootMac );
			QueryProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedForQuery);
			QueryProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedForQuery);

			// Try to launch the query's process, and produce a friendly error message if it fails.
            Utils.RunLocalProcess(QueryProcess);

			return QueryResult;
		}

		/**
		 * Queries the remote compile server for CPU information
		 * and computes the proper ProcessorCountMultiplier.
		 */
		static public Double GetAdjustedProcessorCountMultiplier()
			{
			Int32 RemoteCPUCount = QueryRemoteMachine(MacName, "sysctl -n hw.ncpu");
			if (RemoteCPUCount == 0)
				{
				RemoteCPUCount = Environment.ProcessorCount;
				}
			

			Double AdjustedMultiplier = (Double)RemoteCPUCount / (Double)Environment.ProcessorCount;
					if( BuildConfiguration.bPrintDebugInfo )
					{
				Console.WriteLine( "Adjusting the remote Mac compile process multiplier to " + AdjustedMultiplier.ToString() );
			}
			return AdjustedMultiplier;
		}

		static public Int32 GetAvailableCommandSlotCount( string TargetMacName )
		{
			// ask how many slots are available, and increase by 1 (not sure why)
			Int32 RemoteAvailableCommandSlotCount = 1 + QueryRemoteMachine(TargetMacName, "rpc:command_slots_available");

			if( BuildConfiguration.bPrintDebugInfo )
			{
				Console.WriteLine( "Available command slot count for " + TargetMacName + " is " + RemoteAvailableCommandSlotCount.ToString() );
			}
			return RemoteAvailableCommandSlotCount;
		}

		/**
		 * Helper function to sync source files to and from the local system and a remote Mac
		 */
		public static bool OutputReceivedDataEventHandlerEncounteredError = false;
		public static string OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
		public static void OutputReceivedDataEventHandler( Object Sender, DataReceivedEventArgs Line )
		{
			if( ( Line != null ) && ( Line.Data != null ) )
			{
				Console.WriteLine( Line.Data );

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
		
		public static void SyncHelper( string GameName, UnrealTargetPlatform Platform, UnrealTargetConfiguration Configuration, string LocalShadowDirectoryRoot, bool bPreBuild )
		{
			string LocalShadowDirectory = Path.GetFullPath( LocalShadowDirectoryRoot );
			string RemoteShadowDirectoryMac = UserDevRootMac + BranchDirectoryMac;

			if( bPreBuild )
			{
				// batch upload
				RPCUtilHelper.BatchUpload(BatchUploadCommands.ToArray());

				// we can now clear out the set of files
				BatchUploadCommands.Clear();
			}
			else
			{
				string BinariesPathSuffix = "Binaries\\IPhone\\";
				string DecoratedGameName = GameName + "-IPhone-" + Configuration;
				RPCUtilHelper.CopyFile(
					RemoteShadowDirectoryMac + "/Binaries/IPhone/" + DecoratedGameName,
					Path.Combine(Path.Combine(Path.GetFullPath("..\\.."), BinariesPathSuffix), DecoratedGameName),
					false);

 				if( BuildConfiguration.bGeneratedSYMFile == true )
 				{
					string DSYMName = DecoratedGameName + ".app.dSYM.zip";
					RPCUtilHelper.CopyFile(
						RemoteShadowDirectoryMac + "/Binaries/IPhone/" + DSYMName,
						Path.Combine(Path.Combine(Path.GetFullPath("..\\.."), BinariesPathSuffix), DSYMName),
						false);
 				}
			}

            // Generate the stub
            if (!bPreBuild && BuildConfiguration.bCreateStubIPA)
            {
                Process StubGenerateProcess = new Process();
                StubGenerateProcess.StartInfo.WorkingDirectory = Path.GetFullPath("..\\..\\Binaries\\IPhone");
                StubGenerateProcess.StartInfo.FileName = Path.Combine(StubGenerateProcess.StartInfo.WorkingDirectory, "iPhonePackager.exe");
				StubGenerateProcess.StartInfo.Arguments = "PackageIPA " + GameName + " " + Configuration + " -createstub -strip";
				// programmers that use Xcode packaging mode should use the following commandline instead, as it will package for Xcode on each compile
//				StubGenerateProcess.StartInfo.Arguments = "PackageApp " + GameName + " " + Configuration;

                StubGenerateProcess.OutputDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);
                StubGenerateProcess.ErrorDataReceived += new DataReceivedEventHandler(OutputReceivedDataEventHandler);

				OutputReceivedDataEventHandlerEncounteredError = false;
				OutputReceivedDataEventHandlerEncounteredErrorMessage = "";
                Utils.RunLocalProcess(StubGenerateProcess);
				if( OutputReceivedDataEventHandlerEncounteredError )
				{
					throw new Exception( OutputReceivedDataEventHandlerEncounteredErrorMessage );
				}
			}
		}

		/** 
		 * Add an action to create a stub IPA for subsequent content iteration
		 */
		public static FileItem AddCreateIPAAction( FileItem MainOutputItem, string GameName, string Configuration )
		{
			Action ITPAction = new Action();

			ITPAction.WorkingDirectory = Path.GetFullPath( "..\\..\\Binaries\\IPhone" );
			ITPAction.CommandPath = ITPAction.WorkingDirectory + "\\iPhonePackager.exe";
			ITPAction.CommandArguments = "PackageIPA " + GameName + " " + Configuration;
            ITPAction.CommandArguments += " -createstub -strip";

			ITPAction.PrerequisiteItems.Add( MainOutputItem );

            // Construct the name of the generated stub IPA
            FileItem RemoteDestFile = FileItem.GetItemByPath( "Stub" + MainOutputItem.AbsolutePath + ".ipa" );
			ITPAction.ProducedItems.Add( RemoteDestFile );

			ITPAction.StatusDescription = "Creating stub IPA for " + GameName + "-" + Configuration;
			ITPAction.bCanExecuteRemotely = false;

			return ( ITPAction.ProducedItems[0] );
		}
	};
}
