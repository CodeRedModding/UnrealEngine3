/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

using System;
using System.IO;
using System.Collections.Generic;

namespace UnrealBuildTool
{
	class BuildConfiguration
	{
		/** Whether to unify C++ code into larger files for faster compilation. */
		public static bool bUseUnityBuild = Utils.GetEnvironmentVariable("ue3.bUseUnityBuild", true);

		/** An approximate number of bytes of C++ code to target for inclusion in a single unified C++ file. */
		public static int NumIncludedBytesPerUnityCPP = 256 * 1024;

		/** Whether to stress test the C++ unity build robustness by including all C++ files files in a project from a single unified file. */
        public static bool bStressTestUnity = Utils.GetEnvironmentVariable("ue3.bStressTestUnity", false);

		/** Whether headers in system paths should be checked for modification when determining outdated actions. */
		public static bool bCheckSystemHeadersForModification = Utils.GetEnvironmentVariable("ue3.bCheckSystemHeadersForModification", false);

		/** Whether headers in the Development\Extrenal folder should be checked for modification when determining outdated actions. */
		public static bool bCheckExternalHeadersForModification = Utils.GetEnvironmentVariable("ue3.bCheckExternalHeadersForModification", false);

        /** Whether to globally disable debug info generation; see DebugInfoHeuristics.cs for per-config and per-platform options. */
        public static bool bDisableDebugInfo = Utils.GetEnvironmentVariable("ue3.bDisableDebugInfo", false);

		/** Whether to disable debug info on PC in release builds (for faster developer iteration, as link times are extremely fast with debug info disabled.) */
		public static bool bOmitPCDebugInfoInRelease = Utils.GetEnvironmentVariable( "ue3.bOmitPCDebugInfoInRelease", true );

		/** Whether PDB files should be used for Visual C++ builds. */
		public static bool bUsePDBFiles = Utils.GetEnvironmentVariable("ue3.bUsePDBFiles", false);
 
		/** Whether PCH files should be used. */
		public static bool bUsePCHFiles = Utils.GetEnvironmentVariable("ue3.bUsePCHFiles", true);

		/** Whether to generate command line dependencies for compile actions when requested */
		public static bool bUseCommandDependencies = true;

		/** The minimum number of files that must use a precompiled header before it will be created and used. */
		public static int MinFilesUsingPrecompiledHeader = 5;

		/** Whether debug info should be written to the console. */
		public static bool bPrintHeaderResolveInfo = Utils.GetEnvironmentVariable( "ue3.bPrintHeaderResolveInfo", false );

		/** Whether debug info should be written to the console. */
		public static bool bPrintDebugInfo = Utils.GetEnvironmentVariable("ue3.bPrintDebugInfo", false);

		/** Whether to log detailed action stats. This forces local execution. */
		public static bool bLogDetailedActionStats = Utils.GetEnvironmentVariable("ue3.bLogDetailedActionStats", false);

        /** Whether to deploy the executable after compilation on platforms that require deployment. */
        public static bool bDeployAfterCompile = false;

		/** Whether XGE may be used. */
		public static bool bAllowXGE = Utils.GetEnvironmentVariable("ue3.bAllowXGE", true);

		/** Whether to display the XGE build monitor. */
		public static bool bShowXGEMonitor = Utils.GetEnvironmentVariable("ue3.bShowXGEMonitor", false);

		/** Whether or not to delete outdated produced items. */
		public static bool bShouldDeleteAllOutdatedProducedItems = Utils.GetEnvironmentVariable("ue3.bShouldDeleteAllOutdatedProducedItems", false);

		/** Whether to use incremental linking or not. */
		public static bool bUseIncrementalLinking = Utils.GetEnvironmentVariable("ue3.bUseIncrementalLinking", false);

		/** Whether to allow the use of LTCG (link time code generation) .*/
		public static bool bAllowLTCG = Utils.GetEnvironmentVariable("ue3.bAllowLTCG", true);

		/** Whether to support edit and continue. */
		public static bool bSupportEditAndContinue = Utils.GetEnvironmentVariable("ue3.bSupportEditAndContinue", false);

        /** Whether to omit frame pointers or not. Disabling is useful for e.g. memory profiling on the PC */
        public static bool bOmitFramePointers = Utils.GetEnvironmentVariable("ue3.bOmitFramePointers", true);
        
		/** Processor count multiplier for local execution. Can be below 1 to reserve CPU for other tasks. */
		public static double ProcessorCountMultiplier = Utils.GetEnvironmentVariable("ue3.ProcessorCountMultiplier", 1.5);

		/** The path to the intermediate folder, relative to Development/Src. */
		public static string BaseIntermediatePath = "../Intermediate";

		/** The path to the targets folder, relative to Development/Src. */
		public static string BaseTargetsPath = "Targets";

		/** Name of performance database to talk to, clear out if you don't have one running */
		public static string PerfDatabaseName = "production-db";

		/** Whether to use the Intel compiler for the Win32 build */
		public static bool bUseIntelCompiler = Utils.GetEnvironmentVariable("ue3.bUseIntelCompiler", false);

		/** Whether to output an iPhone Xcode project and use that to compile */
		public static bool bUseXcodeToolchain = Utils.GetEnvironmentVariable("ue3.bUseXcodeToolchain", false);

		/** Whether to output a Mac Xcode project and use that to compile */
		public static bool bUseXcodeMacToolchain = Utils.GetEnvironmentVariable("ue3.bUseXcodeMacToolchain", false);

		/** Whether to compile as an exe or a dll */
		public static bool bCompileAsdll = false;

		/** If true then the compile output/errors are suppressed */
        public static bool bSilentCompileOutput = false;

        /** If true, then a stub IPA will be generated when compiling is done (minimal files needed for a valid IPA) */
        public static bool bCreateStubIPA = Utils.GetEnvironmentVariable("ue3.iPhone_CreateStubIPA", true);

        /** If true, then enable memory profiling in the build (defines USE_MALLOC_PROFILER=1 and forces bOmitFramePointers=false) */
        public static bool bUseMallocProfiler = false;

		/** Whether to generate a dSYM file or not. */
		public static bool bGeneratedSYMFile = Utils.GetEnvironmentVariable("ue3.bGeneratedSYMFile", false) || bUseMallocProfiler;


        /**
         * Validates the configuration. E.g. some options are mutually exclusive whereof some imply others. Also
         * some functionality is not available on all platforms.
         * 
         * @param	Configuration	Current configuration (e.g. release, debug, ...)
         * @param	Platform		Current platform (e.g. Win32, ...)
         * 
         * @warning: the order of validation is important
         */
		public static void ValidateConfiguration( CPPTargetConfiguration Configuration, CPPTargetPlatform Platform )
		{
			// E&C support.
			if( bSupportEditAndContinue )
			{
				// Only supported on PC in debug
				if( ( Platform == CPPTargetPlatform.Win32 || Platform == CPPTargetPlatform.Win64 ) &&
					Configuration == CPPTargetConfiguration.Debug )
				{
					// Relies on incremental linking.
					bUseIncrementalLinking = true;
				}
				// Disable.
				else
				{
					bUseIncrementalLinking = false;
				}
			}
			// Incremental linking.
			if( bUseIncrementalLinking )
			{
				// Only supported on PC.
				if( Platform == CPPTargetPlatform.Win32 || Platform == CPPTargetPlatform.Win64 )
				{
					bUsePDBFiles = true;
				}
				// Disable.
				else
				{
					bUseIncrementalLinking = false;
				}
			}
			// Detailed stats
			if( bLogDetailedActionStats )
			{
				// Force local execution as we only have stats for local actions.
				bAllowXGE = false;
			}
			// PDB
			if( bUsePDBFiles )
			{
				// Force local execution as we have one PDB for all files using the same PCH. This currently doesn't
				// scale well with XGE due to required networking bandwidth. Xoreax mentioned that this was going to
				// be fixed in a future version of the software.
				bAllowXGE = false;
			}
            // Intel compiler
            if(bUseIntelCompiler)
            {				
                // Make sure ICPP_COMPILER12 environment variable exists.
                string Value = Environment.GetEnvironmentVariable("ICPP_COMPILER12");
                if (Value == null)
                {
                    Console.WriteLine("ICPP_COMPILER12 environment variable doesnt exist. Reverting to VC compiler.");
                    bUseIntelCompiler = false;           
                }
				// Intel compiler is only supported on Win 32.
				else if( Platform == CPPTargetPlatform.Win32 || Platform == CPPTargetPlatform.Win64 )
				{
					// Force local execution as we don't support distributing Intel compiler yet.
					bAllowXGE = false;
					// Disable use of precompiled headers as ICL massages output filenames.
					bUsePCHFiles = false;
				}
				// Non- Win32 platform, disable Intel compiler.
				else
				{
					bUseIntelCompiler = false;
				}
            }
			// iPhone
			if( Platform == CPPTargetPlatform.IPhone )
			{
				// needed for iPhone so that it can cache External headers to remote
				bCheckExternalHeadersForModification = true;

				// Check the number of CPUs and memory available in the compile server
				// and adjust the multiplier accordingly.
				ProcessorCountMultiplier = IPhoneToolChain.GetAdjustedProcessorCountMultiplier();

				// disable unity and PCH files for now with Xcode tool chain
				if (bUseXcodeToolchain)
				{
					bUseUnityBuild = false;
					bUsePCHFiles = false;
				}
			}
			// Mac
			if (Platform == CPPTargetPlatform.Mac)
			{
				// needed for Mac so that it can cache External headers to remote
				bCheckExternalHeadersForModification = true;

				// Check the number of CPUs and memory available in the compile server
				// and adjust the multiplier accordingly.
				ProcessorCountMultiplier = MacToolChain.GetAdjustedProcessorCountMultiplier();
			}
			// Flash
			if( Platform == CPPTargetPlatform.Flash )
			{
				ProcessorCountMultiplier /= 2.0;
			}

			// Android
            if (Platform == CPPTargetPlatform.AndroidARM || Platform == CPPTargetPlatform.Androidx86)
			{
				if( Configuration == CPPTargetConfiguration.Debug )
				{
					// In debug, disable Unity builds so that IDE debugging works as expected
					bUseUnityBuild = false;
				}
			}
			// Flash
			if (Platform == CPPTargetPlatform.Flash)
			{
				// executing remotely can cause g++ to crash
				bAllowXGE = false;
			}
		}

        /**
         * Returns extra arguments that will be parsed as as if they were passed in on the command line.  Can be used to
         * simulate additional defines, etc... that are temporary but global
         * 
         * @return Extra arguments that will be parsed as as if they were passed in on the command line
         */
        public static List<string> GetExtraCommandArguments()
        {
            List<string> ExtraArguments = new List<string>();
            if (bUseMallocProfiler)
            {
                ExtraArguments.Add("-DEFINE");
                ExtraArguments.Add("USE_MALLOC_PROFILER=1");
            }

            return ExtraArguments;
        }
	}
}
