/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;

namespace UnrealBuildTool
{
	class IntelToolChain
	{
		/** Accesses the bin directory for the Intel toolchain. */
		public static string GetIntelBinDirectory( CPPTargetPlatform Platform )
		{
			if( Platform == CPPTargetPlatform.Win32 )
			{
				return Path.Combine(
				Environment.GetEnvironmentVariable( "ICPP_COMPILER12" ),
				"bin\\ia32"
				);
			}
			else
			{
				return Path.Combine(
				Environment.GetEnvironmentVariable( "ICPP_COMPILER12" ),
				"bin\\intel64"
				);
			}
		}

		/** Accesses the lib directory for the Intel toolchain. */
		public static string GetIntelLibDirectory( CPPTargetPlatform Platform )
		{
			if( Platform == CPPTargetPlatform.Win32 )
			{
				return Path.Combine(
				Environment.GetEnvironmentVariable( "ICPP_COMPILER12" ),
				"compiler\\lib\\ia32"
				);
			}
			else
			{
				return Path.Combine(
				Environment.GetEnvironmentVariable( "ICPP_COMPILER12" ),
				"compiler\\lib\\intel64"
				);
			}
		}

		/** Accesses the inc directory for the Intel toolchain. */
		public static string GetIntelIncDirectory()
		{
			return Path.Combine(
			Environment.GetEnvironmentVariable( "ICPP_COMPILER12" ),
			"compiler\\include"
			);
		}

		static string GetCLArguments_Global(CPPEnvironment CompileEnvironment)
		{
			string Result = "";

			// Prevents the compiler from displaying its logo for each invocation.
			Result += " /nologo";

			// Favor code speed.
			Result += " /Ot";

			// Enable intrinsic functions.
			Result += " /Oi";

			// Pack struct members on 4-byte boundaries.
			Result += " /Zp4";

			// Separate functions for linker.
			Result += " /Gy";

			// Relaxes floating point precision semantics to allow more optimization.
			Result += " /fp:fast";

			// Compile into an .obj file, and skip linking.
			Result += " /c";

			// Enable C++ exception handling, but not C exceptions.
			Result += " /EHsc";

			// Enable support for C++0x standard, including variadic macros,
			Result += " /Qstd=c++0x";

			// Specify compatibility with Microsoft* Visual Studio 2008.
			Result += " /Qvc9";

			// Disable debug spew by vectorizer.
			Result += " /Qvec-report0";

			//
			//	Debug
			//
			if (CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Debug)
			{
				// Disable compiler optimization.
				Result += " /Od";

				if( CompileEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || CompileEnvironment.TargetPlatform == CPPTargetPlatform.Win64 )
				{
					// Enable runtime stack checking for Win32 debug builds.
					Result += " /RTCs";
				}

				// Multi-threaded dynamic debug runtime
				Result += " /MDd";
			}
			//
			//	Release and LTCG
			//
			else
			{
				// Maximum optimizations.
				Result += " /Ox";

				// Allow inline method expansion.			
				Result += " /Ob2";

				// Multi-threaded dynamic release runtime
				Result += " /MD";

				//
				// LTCG
				//
                if (CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Shipping || CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Test)
				{
                    if (BuildConfiguration.bAllowLTCG && !(CompileEnvironment.TargetPlatform == CPPTargetPlatform.Win32 || CompileEnvironment.TargetPlatform == CPPTargetPlatform.Win64))
					{
						// Link-time code generation.
						Result += " /GL";
					}
				}
			}

			// If enabled, create debug information.
			if (CompileEnvironment.bCreateDebugInfo)
			{
				// Store debug info in .pdb files.
				if (BuildConfiguration.bUsePDBFiles)
				{
					// Create debug info suitable for E&C if wanted.
					if (BuildConfiguration.bSupportEditAndContinue
						// We only need to do this in debug as that's the only configuration that supports E&C.
					&& CompileEnvironment.TargetConfiguration == CPPTargetConfiguration.Debug)
					{
						Result += " /ZI";
					}
					// Regular PDB debug information.
					else
					{
						Result += " /Zi";
					}
				}
				// Store C7-format debug info in the .obj files, which is faster.
				else
				{
					Result += " /Z7";
				}
			}

			return Result;
		}

		static string GetCLArguments_CPP()
		{
			string Result = "";

			// Explicitly compile the file as C++.
			Result += " /TP";

			// Disable C++ RTTI.
			Result += " /GR-";

			// Level 0 warnings for now.
			Result += " /W0";

			return Result;
		}

		static string GetCLArguments_C()
		{
			string Result = "";

			// Explicitly compile the file as C.
			Result += " /TC";

			// Level 0 warnings.  Needed for external C projects that produce warnings at higher warning levels.
			Result += " /W0";

			return Result;
		}
        
		static string GetLinkArguments(LinkEnvironment LinkEnvironment)
		{
			string Result = "";

			// Don't create a side-by-side manifest file for the executable.
			Result += " /MANIFEST:NO";

			// Prevents the linker from displaying its logo for each invocation.
			Result += " /NOLOGO";

			// Output debug info for the linked executable.
			Result += " /DEBUG";

			// Link for win32.
			Result += " /SUBSYSTEM:WINDOWS";

			// Allow the OS to load the EXE at different base addresses than its preferred base address.
			Result += " /FIXED:No";

			// Disables the 2GB address space limit on 64-bit Windows and 32-bit Windows with /3GB specified in boot.ini
			Result += " /LARGEADDRESSAWARE";

			// Explicitly declare that the executable is compatible with Data Execution Prevention.
			Result += " /NXCOMPAT";

			// Set the default stack size.
			Result += " /STACK:5000000,5000000";

			// E&C can't use /SAFESEH.
			if (!BuildConfiguration.bSupportEditAndContinue)
			{
				// Generates a table of Safe Exception Handlers.  Documentation isn't clear whether they actually mean
				// Structured Exception Handlers.
				Result += " /SAFESEH";
			}

			// Include definition file required for PixelMine's UnrealScript debugger.
			Result += " /DEF:UnrealEngine3.def";

			// Allow delay-loaded DLLs to be explicitly unloaded.
			Result += " /DELAY:UNLOAD";

			//
			//	Shipping binary
			//
			if (LinkEnvironment.bIsShippingBinary)
			{
				// Generate an EXE checksum.
				Result += " /RELEASE";

				// Eliminate unreferenced symbols.
				Result += " /OPT:REF";

				// Remove redundant COMDATs.
				Result += " /OPT:ICF";
			}
			else
			{
				// Keep symbols that are unreferenced.
				Result += " /OPT:NOREF";

				// Disable identical COMDAT folding.
				Result += " /OPT:NOICF";
			}

			// Enable incremental linking if wanted.
			if (BuildConfiguration.bUseIncrementalLinking)
			{
				Result += " /INCREMENTAL";
			}
			// Disabled by default as it can cause issues and forces local execution.
			else
			{
				Result += " /INCREMENTAL:NO";
			}

			return Result;
		}

		public static CPPOutput CompileCPPFiles(CPPEnvironment CompileEnvironment, List<FileItem> SourceFiles)
		{
			string Arguments = GetCLArguments_Global( CompileEnvironment );

			CompileEnvironment.SystemIncludePaths.Add( GetIntelIncDirectory() );

			// Add include paths to the argument list.
			foreach (string IncludePath in CompileEnvironment.IncludePaths)
			{
				Arguments += string.Format(" /I \"{0}\"", IncludePath);
			}
			foreach (string IncludePath in CompileEnvironment.SystemIncludePaths)
			{
				Arguments += string.Format(" /I \"{0}\"", IncludePath);
			}

			// Add preprocessor definitions to the argument list.
			foreach (string Definition in CompileEnvironment.Definitions)
			{
				Arguments += string.Format(" /D \"{0}\"", Definition);
			}

			// Create a compile action for each source file.
			CPPOutput Result = new CPPOutput();
			foreach (FileItem SourceFile in SourceFiles)
			{
				Action CompileAction = new Action();
				string FileArguments = "";

				// Add the C++ source file and its included files to the prerequisite item list.
				CompileAction.PrerequisiteItems.Add(SourceFile);
				foreach (FileItem IncludedFile in CompileEnvironment.GetIncludeDependencies(SourceFile))
				{
					CompileAction.PrerequisiteItems.Add(IncludedFile);
				}

				if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Create)
				{
					// Generate a CPP File that just includes the precompiled header.
					string PCHCPPFilename = Path.GetFileName(CompileEnvironment.PrecompiledHeaderIncludeFilename) + ".PCH.cpp";
					string PCHCPPPath = Path.Combine(CompileEnvironment.OutputDirectory, PCHCPPFilename);
					FileItem PCHCPPFile = FileItem.CreateIntermediateTextFile(
						PCHCPPPath,
						string.Format("#include \"{0}\"\r\n", CompileEnvironment.PrecompiledHeaderIncludeFilename)
						);

					// Add the precompiled header file to the produced items list.
					FileItem PrecompiledHeaderFile = FileItem.GetItemByPath(
						Path.Combine(
							CompileEnvironment.OutputDirectory,
							Path.GetFileName(SourceFile.AbsolutePath) + ".pch"
							)
						);
					CompileAction.ProducedItems.Add(PrecompiledHeaderFile);
					Result.PrecompiledHeaderFile = PrecompiledHeaderFile;

					// Add the parameters needed to compile the precompiled header file to the command-line.
					FileArguments += string.Format(" /Yc\"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename);
					FileArguments += string.Format(" /Fp\"{0}\"", PrecompiledHeaderFile.AbsolutePath);
					FileArguments += string.Format(" {0}", PCHCPPFile.AbsolutePath);
				}
				else
				{
					if (CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.Include)
					{
						CompileAction.bIsUsingPCH = true;
						CompileAction.PrerequisiteItems.Add(CompileEnvironment.PrecompiledHeaderFile);
						FileArguments += string.Format(" /Yu\"{0}\"", CompileEnvironment.PrecompiledHeaderIncludeFilename);
						FileArguments += string.Format(" /Fp\"{0}\"", CompileEnvironment.PrecompiledHeaderFile.AbsolutePath);

					}

					// Add the source file path to the command-line.
					FileArguments += string.Format(" \"{0}\"", SourceFile.AbsolutePath);
				}

				// Add the object file to the produced item list.
				FileItem ObjectFile = FileItem.GetItemByPath(
					Path.Combine(
						CompileEnvironment.OutputDirectory,
						Path.GetFileName(SourceFile.AbsolutePath) + ".obj"
						)
					);
				CompileAction.ProducedItems.Add(ObjectFile);
				Result.ObjectFiles.Add(ObjectFile);
				FileArguments += string.Format(" /Fo\"{0}\"", ObjectFile.AbsolutePath);

                if (CompileEnvironment.bCreateDebugInfo && BuildConfiguration.bUsePDBFiles)
				{
					// Specify the PDB file that the compiler should write to.
					FileItem PDBFile = FileItem.GetItemByPath(
							Path.Combine(
								CompileEnvironment.OutputDirectory,
								Path.GetFileName(SourceFile.AbsolutePath) + ".pdb"
								)
							);
					FileArguments += string.Format(" /Fd\"{0}\"", PDBFile.AbsolutePath);
					CompileAction.ProducedItems.Add(PDBFile);
					Result.DebugDataFiles.Add(PDBFile);
				}

				// Add C or C++ specific compiler arguments.
				if (Path.GetExtension(SourceFile.AbsolutePath).ToUpperInvariant() == ".C")
				{
					FileArguments += GetCLArguments_C();
				}
				else
				{
					FileArguments += GetCLArguments_CPP();
				}

				CompileAction.WorkingDirectory = Path.GetFullPath(".");
				CompileAction.CommandPath = Path.Combine(GetIntelBinDirectory( CompileEnvironment.TargetPlatform ), "icl.exe");
				CompileAction.bIsVCCompiler = false;
				CompileAction.CommandArguments = Arguments + FileArguments + CompileEnvironment.AdditionalArguments;
				CompileAction.StatusDescription = string.Format("{0}", Path.GetFileName(SourceFile.AbsolutePath));
				CompileAction.StatusDetailedDescription = SourceFile.Description;
				CompileAction.bShouldLogIfExecutedLocally = false;

				// Only tasks that don't use precompiled headers can be distributed with the current version of XGE.
				CompileAction.bCanExecuteRemotely = CompileEnvironment.PrecompiledHeaderAction == PrecompiledHeaderAction.None;
			}
			return Result;
		}

		public static FileItem LinkFiles(LinkEnvironment LinkEnvironment)
		{
			// Create an action that invokes the linker.
			Action LinkAction = new Action();
			LinkAction.bIsLinker = true;
			LinkAction.WorkingDirectory = Path.GetFullPath(".");
			LinkAction.CommandPath = Path.Combine(GetIntelBinDirectory( LinkEnvironment.TargetPlatform ), "xilink.exe");

			// Get link arguments.
			LinkAction.CommandArguments = GetLinkArguments(LinkEnvironment);

			// Delay-load these DLLs.
			foreach (string DelayLoadDLL in LinkEnvironment.DelayLoadDLLs)
			{
				LinkAction.CommandArguments += string.Format(" /DELAYLOAD:\"{0}\"", DelayLoadDLL);
			}

			// Add the Intel lib directory to the library path.
			LinkEnvironment.LibraryPaths.Add(GetIntelLibDirectory( LinkEnvironment.TargetPlatform ));

			// Add the library paths to a response file, and pass the response file on the command-line
			List<string> LibraryPaths = new List<string>();
			foreach (string LibraryPath in LinkEnvironment.LibraryPaths)
			{
				LibraryPaths.Add( string.Format(" /LIBPATH:\"{0}\"", Utils.ResolveEnvironmentVariable(LibraryPath)) );
			}
			string ResponseFileNameLibrary = Path.Combine( LinkEnvironment.OutputDirectory, Path.GetFileName( LinkEnvironment.OutputFilePath ) + ".response_library" );
			LinkAction.CommandArguments += string.Format( " @\"{0}\"", ResponseFile.Create( ResponseFileNameLibrary, LibraryPaths ) );

			// Add the excluded default libraries to the argument list.
			foreach (string ExcludedLibrary in LinkEnvironment.ExcludedLibraries)
			{
				LinkAction.CommandArguments += string.Format(" /NODEFAULTLIB:\"{0}\"", ExcludedLibrary);
			}

			// Add the output file as a production of the link action.
			FileItem OutputFile = FileItem.GetItemByPath(LinkEnvironment.OutputFilePath);
			LinkAction.ProducedItems.Add(OutputFile);
			LinkAction.StatusDescription = string.Format("{0}", Path.GetFileName(OutputFile.AbsolutePath));

			// Add the input files to a response file, and pass the response file on the command-line.
			List<string> InputFileNames = new List<string>();
			foreach (FileItem InputFile in LinkEnvironment.InputFiles)
			{
				InputFileNames.Add(string.Format("\"{0}\"", InputFile.AbsolutePath));
				LinkAction.PrerequisiteItems.Add(InputFile);
			}
			foreach (string AdditionalLibrary in LinkEnvironment.AdditionalLibraries)
			{
				InputFileNames.Add(string.Format("\"{0}\"", AdditionalLibrary));
			}
			string ResponseFileNameInput = Path.Combine( LinkEnvironment.OutputDirectory, Path.GetFileName( LinkEnvironment.OutputFilePath ) + ".response_input" );
			LinkAction.CommandArguments += string.Format( " @\"{0}\"", ResponseFile.Create( ResponseFileNameInput, InputFileNames ) );

			// Add the output file to the command-line.
			LinkAction.CommandArguments += string.Format(" /OUT:\"{0}\"", OutputFile.AbsolutePath);

			// Write the import library to the output directory.
			string ImportLibraryFilePath = Path.Combine(
				LinkEnvironment.OutputDirectory,
				Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".lib"
				);
			FileItem ImportLibraryFile = FileItem.GetItemByPath(ImportLibraryFilePath);
			LinkAction.CommandArguments += string.Format(" /IMPLIB:\"{0}\"", ImportLibraryFilePath);
			LinkAction.ProducedItems.Add(ImportLibraryFile);

			// An export file is written to the output directory implicitly; add it to the produced items list.
			string ExportFilePath = Path.ChangeExtension(ImportLibraryFilePath, ".exp");
			FileItem ExportFile = FileItem.GetItemByPath(ExportFilePath);
			LinkAction.ProducedItems.Add(ExportFile);

			// Write the PDB file to the output directory.
			string PDBFilePath = Path.Combine(LinkEnvironment.OutputDirectory, Path.GetFileNameWithoutExtension(OutputFile.AbsolutePath) + ".pdb");
			FileItem PDBFile = FileItem.GetItemByPath(PDBFilePath);
			LinkAction.CommandArguments += string.Format(" /PDB:\"{0}\"", PDBFilePath);
			LinkAction.ProducedItems.Add(PDBFile);

			// Add the additional arguments specified by the environment.
			LinkAction.CommandArguments += LinkEnvironment.AdditionalArguments;

			// Only execute linking on the local PC.
			LinkAction.bCanExecuteRemotely = false;

			return OutputFile;
		}
	};
}