/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Text;
using System.Text.RegularExpressions;
using System.IO;

namespace UnrealBuildTool
{
	/** The platforms that may be compilation targets for C++ files. */
	enum CPPTargetPlatform
	{
		Win32,
		Win64,
		IPhone,
		AndroidARM,
        Androidx86,
		Mac,
		Flash,
	}

	/** The optimization level that may be compilation targets for C++ files. */
	enum CPPTargetConfiguration
	{
		Debug,
		Release,
		Shipping,
        Test,
	}

	/** The optimization level that may be compilation targets for C# files. */
	enum CSharpTargetConfiguration
	{
		Debug,
		Release,
	}

	/** The possible interactions between a precompiled header and a C++ file being compiled. */
	enum PrecompiledHeaderAction
	{
		None,
		Include,
		Create
	}

	/** Whether the Common Language Runtime is enabled when compiling C++ targets (enables C++/CLI) */
	enum CPPCLRMode
	{
		CLRDisabled,
		CLREnabled,
	}

	/** Encapsulates the compilation output of compiling a set of C++ files. */
	class CPPOutput
	{
		public List<FileItem> ObjectFiles = new List<FileItem>();
		public List<FileItem> DebugDataFiles = new List<FileItem>();
		public FileItem PrecompiledHeaderFile = null;
	}


	/** Describes a private assembly dependency */
	class PrivateAssemblyInfo
	{
		/** The file item for the reference assembly on disk */
		public FileItem FileItem = null;

		/** True if the assembly should be copied to an output folder.  Often, referenced assemblies must
		    be loaded from the directory that the main executable resides in (or a sub-folder).  Note that
		    PDB files will be copied as well. */
		public bool bShouldCopyToOutputDirectory = false;

		/** Optional sub-directory to copy the assembly to */
		public string DestinationSubDirectory = "";
	}

	/** Encapsulates the environment that a C# file is compiled in. */
	class CSharpEnvironment
	{
		/** The configuration to be compiled for. */
		public CSharpTargetConfiguration TargetConfiguration;
		/** The target platform used to set the environment. Doesn't affect output. */
		public CPPTargetPlatform EnvironmentTargetPlatform;
	}

	/** Encapsulates the environment that a C++ file is compiled in. */
	partial class CPPEnvironment
	{
		/** The directory to put the output object/debug files in. */
		public string OutputDirectory = null;

		/** The directory to shadow source files in for syncing to remote compile servers */
		public string LocalShadowDirectory = null;

		/** The file containing the precompiled header data. */
		public FileItem PrecompiledHeaderFile = null;

		/** The name of the header file which is precompiled. */
		public string PrecompiledHeaderIncludeFilename = null;

		/** Whether the compilation should create, use, or do nothing with the precompiled header. */
		public PrecompiledHeaderAction PrecompiledHeaderAction = PrecompiledHeaderAction.None;

		/** The platform to be compiled for. */
		public CPPTargetPlatform TargetPlatform;

		/** The configuration to be compiled for. */
		public CPPTargetConfiguration TargetConfiguration;

		/** True if debug info should be created. */
		public bool bCreateDebugInfo = true;

		/** Whether the CLR (Common Language Runtime) support should be enabled for C++ targets (C++/CLI). */
		public CPPCLRMode CLRMode = CPPCLRMode.CLRDisabled;

		/** The include paths to look for included files in. */
		public List<string> IncludePaths = new List<string>();

		/**
		 * The include paths where changes to contained files won't cause dependent C++ source files to
		 * be recompiled, unless BuildConfiguration.bCheckSystemHeadersForModification==TRUE.
		 */
		public List<string> SystemIncludePaths = new List<string>();

		/** Paths where .NET framework assembly references are found, when compiling CLR applications. */
		public List<string> SystemDotNetAssemblyPaths = new List<string>();

		/** Full path and file name of .NET framework assemblies we're referencing */
		public List<string> FrameworkAssemblyDependencies = new List<string>();

		/** List of private CLR assemblies that, when modified, will force recompilation of any CLR source files */
		public List<PrivateAssemblyInfo> PrivateAssemblyDependencies = new List<PrivateAssemblyInfo>();

		/** The C++ preprocessor definitions to use. */
		public List<string> Definitions = new List<string>();

		/** Additional arguments to pass to the compiler. */
		public string AdditionalArguments = "";

		/** Default constructor. */
		public CPPEnvironment()
		{}

		/** Copy constructor. */
		public CPPEnvironment(CPPEnvironment InCopyEnvironment)
		{
			OutputDirectory = InCopyEnvironment.OutputDirectory;
			LocalShadowDirectory = InCopyEnvironment.LocalShadowDirectory;
			PrecompiledHeaderFile = InCopyEnvironment.PrecompiledHeaderFile;
			PrecompiledHeaderIncludeFilename = InCopyEnvironment.PrecompiledHeaderIncludeFilename;
			PrecompiledHeaderAction = InCopyEnvironment.PrecompiledHeaderAction;
			TargetPlatform = InCopyEnvironment.TargetPlatform;
			TargetConfiguration = InCopyEnvironment.TargetConfiguration;
			bCreateDebugInfo = InCopyEnvironment.bCreateDebugInfo;
			CLRMode = InCopyEnvironment.CLRMode;
			IncludePaths.AddRange(InCopyEnvironment.IncludePaths);
			SystemIncludePaths.AddRange(InCopyEnvironment.SystemIncludePaths);
			SystemDotNetAssemblyPaths.AddRange(InCopyEnvironment.SystemDotNetAssemblyPaths);
			FrameworkAssemblyDependencies.AddRange( InCopyEnvironment.FrameworkAssemblyDependencies );
			PrivateAssemblyDependencies.AddRange(InCopyEnvironment.PrivateAssemblyDependencies);
			Definitions.AddRange(InCopyEnvironment.Definitions);
			AdditionalArguments = InCopyEnvironment.AdditionalArguments;
		}

		/**
		 * Creates actions to compile a set of C++ source files.
		 * @param CPPFiles - The C++ source files to compile.
		 * @return The object files produced by the actions.
		 */
		public CPPOutput CompileFiles(List<FileItem> CPPFiles)
		{
			if( TargetPlatform == CPPTargetPlatform.Win32 || TargetPlatform == CPPTargetPlatform.Win64 )
			{
				if( BuildConfiguration.bUseIntelCompiler )
				{
					return IntelToolChain.CompileCPPFiles( this, CPPFiles );
				}
				else
				{
					return VCToolChain.CompileCPPFiles( this, CPPFiles );
				}
			}
			else if (TargetPlatform == CPPTargetPlatform.IPhone)
			{
				if (BuildConfiguration.bUseXcodeToolchain)
				{
					return XcodeToolChain.CompileCPPFiles(this, CPPFiles);
				}
				else
				{
					return IPhoneToolChain.CompileCPPFiles( this, CPPFiles );
				}
			}
            else if (TargetPlatform == CPPTargetPlatform.AndroidARM ||
                TargetPlatform == CPPTargetPlatform.Androidx86)
			{
				return AndroidToolChain.CompileCPPFiles(this, CPPFiles);
			}
			else if (TargetPlatform == CPPTargetPlatform.Mac)
			{
				if (BuildConfiguration.bUseXcodeMacToolchain)
				{
					return XcodeMacToolChain.CompileCPPFiles(this, CPPFiles);
				}
				else
				{
					return MacToolChain.CompileCPPFiles(this, CPPFiles);
				}
			}
			else if (TargetPlatform == CPPTargetPlatform.Flash)
			{
				return FlashToolChain.CompileCPPFiles(this, CPPFiles);
			}
			else
			{
				Debug.Fail("Unrecognized C++ target platform.");
				return new CPPOutput();
			}
		}

		/**
		 * Creates actions to compile a set of Windows resource script files.
		 * @param RCFiles - The resource script files to compile.
		 * @return The compiled resource (.res) files produced by the actions.
		 */
		public CPPOutput CompileRCFiles(List<FileItem> RCFiles)
		{
			if( TargetPlatform == CPPTargetPlatform.Win32 || TargetPlatform == CPPTargetPlatform.Win64 )
			{
				return VCToolChain.CompileRCFiles(this, RCFiles);
			}
			else
			{
				return new CPPOutput();
			}
		}

		/**
		 * Whether to use PCH files with the current target
		 * 
		 * @return	TRUE if PCH files should be used, FALSE otherwise
		 */
		public bool ShouldUsePCHs()
		{
			bool bUsePCHFiles = BuildConfiguration.bUsePCHFiles;
			// Disable PCH on Android till it is set up and also for SPU jobs as they are tiny.
            if (TargetPlatform == CPPTargetPlatform.AndroidARM || TargetPlatform == CPPTargetPlatform.Androidx86 || TargetPlatform == CPPTargetPlatform.Flash)
			{
				bUsePCHFiles = false;
			}
			return bUsePCHFiles;
		}
	};
}
