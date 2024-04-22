/*=============================================================================
	ShaderCompiler.cpp: Platform independent shader compilation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnConsoleTools.h"

// Set to 1 to debug ShaderCompilerWorker.exe. Set a breakpoint in LaunchWorker() to get the cmd-line.
#define DEBUG_SHADERCOMPILEWORKER 0

// Set this to true in the debugger to enable retries in debug
UBOOL GRetryShaderCompilation = FALSE;

const TCHAR* GetShaderFrequencyName(EShaderFrequency ShaderFrequency)
{
	checkAtCompileTime(6 == SF_NumFrequencies, Bad_EShaderFrequency);
	switch(ShaderFrequency)
	{
		case SF_Vertex:		return TEXT("Vertex");
		case SF_Hull:		return TEXT("Hull");
		case SF_Domain:		return TEXT("Domain");
		case SF_Geometry:	return TEXT("Geometry");
		case SF_Pixel:		return TEXT("Pixel");
		case SF_Compute:	return TEXT("Compute");
	}
	return TEXT("");
}

/**
* Converts shader platform to human readable string. 
*
* @param ShaderPlatform	Shader platform enum
* @param bUseAbbreviation	Specifies whether, or not, to use the abbreviated name
* @param bIncludeGLES2		True if we should even include GLES2, even though it doesn't have a shader platform
* @return text representation of enum
*/
const TCHAR* ShaderPlatformToText( EShaderPlatform ShaderPlatform, UBOOL bUseAbbreviation, UBOOL bIncludeGLES2 )
{
	if( bIncludeGLES2 && GUsingES2RHI )
	{
		return bUseAbbreviation ? TEXT("ES2") : TEXT("OpenGLES2");
	}

	switch( ShaderPlatform )
	{
	case SP_PCD3D_SM3:
		return bUseAbbreviation ? TEXT("DX9") : TEXT("PC-D3D-SM3");
	case SP_XBOXD3D:
		return TEXT("Xbox360");
	case SP_PS3:
		return TEXT("PS3");
	case SP_PCD3D_SM5:
		return bUseAbbreviation ? TEXT("DX11") : TEXT("PC-D3D-SM5");
	case SP_PCOGL:
		return bUseAbbreviation ? TEXT("OpenGL") : TEXT("PC-OpenGL");
	case SP_NGP:
		return TEXT("NGP");
	case SP_WIIU:
		return TEXT("WiiU");
	}
	return TEXT("Unknown");
}

/** Parses an EShaderPlatform from the given text, returns SP_NumPlatforms if not possible. */
EShaderPlatform ShaderPlatformFromText( const TCHAR* PlatformName )
{
	if (appStrcmp(PlatformName, TEXT("PC-D3D-SM3")) == 0)
	{
		return SP_PCD3D_SM3;
	}
	else if (appStrcmp(PlatformName, TEXT("PC-D3D-SM5")) == 0)
	{
		return SP_PCD3D_SM5;
	}
	else if (appStrcmp(PlatformName, TEXT("PC-OpenGL")) == 0)
	{
		return SP_PCOGL;
	}
	else if (appStrcmp(PlatformName, TEXT("Xbox360")) == 0)
	{
		return SP_XBOXD3D;
	}
	else if (appStrcmp(PlatformName, TEXT("PS3")) == 0)
	{
		return SP_PS3;
	}
	else if (appStrcmp(PlatformName, TEXT("WiiU")) == 0)
	{
		return SP_WIIU;
	}
	else if (appStrcmp(PlatformName, TEXT("NGP")) == 0)
	{
		return SP_NGP;
	}
	return SP_NumPlatforms;
}

/** Returns TRUE if debug viewmodes are allowed for the given platform. */
UBOOL AllowDebugViewmodes(EShaderPlatform Platform)
{
#if WITH_MOBILE_RHI
	if( GUsingMobileRHI )
	{
		// platforms with flattened materials can't show any debug view modes
		return FALSE;
	}
#endif

	// if cooking for such a device, never allow debug viewmodes either
	if (GCookingTarget & (UE3::PLATFORM_Mobile | UE3::PLATFORM_WindowsServer))
	{
		return FALSE;
	}

	static UBOOL bAllowDebugViewmodesOnConsoles = FALSE;
	static UBOOL bReadFromIni = FALSE;
	if (!bReadFromIni)
	{
		bReadFromIni = TRUE;
		GConfig->GetBool( TEXT("Engine.Engine"), TEXT("bAllowDebugViewmodesOnConsoles"), bAllowDebugViewmodesOnConsoles, GEngineIni );
	}

	// To use debug viewmodes on consoles, bAllowDebugViewmodesOnConsoles in the engine ini must be set to TRUE, 
	// And EngineDebugMaterials must be in the StartupPackages for the target platform.
	return bAllowDebugViewmodesOnConsoles || !(Platform == SP_XBOXD3D || Platform == SP_PS3 || Platform == SP_WIIU || appGetPlatformType() == UE3::PLATFORM_WindowsServer);
}

/** Converts UE3::EPlatformType console platforms to the appropriate EShaderPlatform, otherwise returns SP_PCD3D_SM3. */
EShaderPlatform ShaderPlatformFromUE3Platform(UE3::EPlatformType UE3Platform)
{
	// Note: There is no complete mapping from UE3 platform to shader platform, so just handle the ones that users of this function need
	switch (UE3Platform)
	{
	case UE3::PLATFORM_PS3:
		return SP_PS3;
	case UE3::PLATFORM_Xbox360:
		return SP_XBOXD3D;
	case UE3::PLATFORM_NGP:
		return SP_PCD3D_SM3;
	case UE3::PLATFORM_WiiU:
		return SP_WIIU;
	}
	return SP_PCD3D_SM3;
}

/** Converts EShaderPlatform to the appropriate UE3::EPlatformType console platforms, otherwise returns Windows. */
UE3::EPlatformType UE3PlatformFromShaderPlatform(EShaderPlatform ShaderPlatform)
{
	// Note: There is no complete mapping from UE3 platform to shader platform, so just handle the ones that users of this function need
	switch (ShaderPlatform)
	{
	case SP_PS3:
		return UE3::PLATFORM_PS3;
	case SP_XBOXD3D:
		return UE3::PLATFORM_Xbox360;
	case SP_NGP:
		return UE3::PLATFORM_NGP;
	case SP_WIIU:
		return UE3::PLATFORM_WiiU;
	case SP_PCOGL:
		return UE3::PLATFORM_MacOSX;
	}
	return UE3::PLATFORM_Windows;
}

FConsoleShaderPrecompiler* GConsoleShaderPrecompilers[SP_NumPlatforms] = { NULL, NULL, NULL, NULL};

DECLARE_STATS_GROUP(TEXT("ShaderCompiling"),STATGROUP_ShaderCompiling);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Total Material Shader Compiling Time"),STAT_ShaderCompiling_MaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Total Global Shader Compiling Time"),STAT_ShaderCompiling_GlobalShaders,STATGROUP_ShaderCompiling);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("RHI Compile Time"),STAT_ShaderCompiling_RHI,STATGROUP_ShaderCompiling);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("CRCing Shader Files"),STAT_ShaderCompiling_HashingShaderFiles,STATGROUP_ShaderCompiling);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Loading Shader Files"),STAT_ShaderCompiling_LoadingShaderFiles,STATGROUP_ShaderCompiling);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("HLSL Translation"),STAT_ShaderCompiling_HLSLTranslation,STATGROUP_ShaderCompiling);

DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Total Material Shaders"),STAT_ShaderCompiling_NumTotalMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Special Material Shaders"),STAT_ShaderCompiling_NumSpecialMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Terrain Material Shaders"),STAT_ShaderCompiling_NumTerrainMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Decal Material Shaders"),STAT_ShaderCompiling_NumDecalMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Particle Material Shaders"),STAT_ShaderCompiling_NumParticleMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Skinned Material Shaders"),STAT_ShaderCompiling_NumSkinnedMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Lit Material Shaders"),STAT_ShaderCompiling_NumLitMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Unlit Material Shaders"),STAT_ShaderCompiling_NumUnlitMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Transparent Material Shaders"),STAT_ShaderCompiling_NumTransparentMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Opaque Material Shaders"),STAT_ShaderCompiling_NumOpaqueMaterialShaders,STATGROUP_ShaderCompiling);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Masked Material Shaders"),STAT_ShaderCompiling_NumMaskedMaterialShaders,STATGROUP_ShaderCompiling);

DECLARE_STATS_GROUP(TEXT("ShaderCompression"),STATGROUP_ShaderCompression);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Shaders Loaded"),STAT_ShaderCompression_NumShadersLoaded,STATGROUP_ShaderCompression);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("RT Shader Load Time"),STAT_ShaderCompression_RTShaderLoadTime,STATGROUP_ShaderCompression);
DECLARE_DWORD_ACCUMULATOR_STAT(TEXT("Num Shaders Used"),STAT_ShaderCompression_NumShadersUsedForRendering,STATGROUP_ShaderCompression);
DECLARE_FLOAT_ACCUMULATOR_STAT(TEXT("Total RT Shader Init Time"),STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,STATGROUP_ShaderCompression);
DECLARE_CYCLE_STAT(TEXT("Frame RT Shader Init Time"),STAT_ShaderCompression_FrameRTShaderInitForRenderingTime,STATGROUP_ShaderCompression);
DECLARE_MEMORY_STAT2_FAST(TEXT("Compressed Shader Memory"),STAT_ShaderCompression_CompressedShaderMemory,STATGROUP_ShaderCompression, MCR_Physical, FALSE);
DECLARE_MEMORY_STAT2_FAST(TEXT("Uncompressed Shader Memory"),STAT_ShaderCompression_UncompressedShaderMemory,STATGROUP_ShaderCompression, MCR_Physical, FALSE);

#if !UE3_LEAN_AND_MEAN

extern UBOOL D3D9BeginCompileShader(
	INT JobId, 
	UINT ThreadId,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	const FShaderCompilerEnvironment& Environment,
	FShaderCompilerOutput& Output,
	UBOOL bDebugDump = FALSE,
	const TCHAR* ShaderSubDir = NULL
	);

extern UBOOL D3D9FinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output);

extern UBOOL D3D11BeginCompileShader(
	 INT JobId, 
	 UINT ThreadId,
	 const TCHAR* SourceFilename,
	 const TCHAR* FunctionName,
	 FShaderTarget Target,
	 const FShaderCompilerEnvironment& Environment,
	 FShaderCompilerOutput& Output,
	 UBOOL bDebugDump = FALSE,
	 const TCHAR* ShaderSubDir = NULL
	 );

extern UBOOL D3D11FinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output);

extern UBOOL OpenGLBeginCompileShader(
	INT JobId, 
	UINT ThreadId,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	const FShaderCompilerEnvironment& Environment,
	FShaderCompilerOutput& Output,
	UBOOL bDebugDump = FALSE,
	const TCHAR* ShaderSubDir = NULL
	);

extern UBOOL OpenGLFinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output);

#endif

extern UBOOL PrecompileConsoleShader(
	INT JobId, 
	UINT ThreadId, 
	const TCHAR* SourceFilename, 
	const TCHAR* FunctionName, 
	const FShaderTarget& Target, 
	const FShaderCompilerEnvironment& Environment, 
	FShaderCompilerOutput& Output);

extern UBOOL PreprocessConsoleShader(
	const TCHAR* SourceFilename, 
	const TCHAR* FunctionName, 
	const FShaderTarget& Target, 
	const FShaderCompilerEnvironment& Environment, 
	const TCHAR* ShaderSubDir);

extern UBOOL ConsoleFinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output);

// A distinct timer for each platform to tease out time spent
// compiling for multiple platforms with a single PCS command
DOUBLE GRHIShaderCompileTime_Total     = 0.0;
DOUBLE GRHIShaderCompileTime_PS3       = 0.0;
DOUBLE GRHIShaderCompileTime_NGP       = 0.0;
DOUBLE GRHIShaderCompileTime_XBOXD3D   = 0.0;
DOUBLE GRHIShaderCompileTime_PCD3D_SM3 = 0.0;
DOUBLE GRHIShaderCompileTime_PCD3D_SM5 = 0.0;
DOUBLE GRHIShaderCompileTime_PCOGL     = 0.0;
DOUBLE GRHIShaderCompileTime_WIIU      = 0.0;

FShaderCompileThreadRunnable::FShaderCompileThreadRunnable(FShaderCompilingThreadManager* InManager) :
	Manager(InManager),
	Thread(NULL),
	WorkerAppId(0),
	ThreadId(InManager->NextThreadId++),
	bTerminatedByError(FALSE),
	bCopiedShadersToWorkingDirectory(FALSE)
{
}

/** Entry point for shader compiling threads, all but the main thread start here. */
DWORD FShaderCompileThreadRunnable::Run()
{
	while (TRUE)
	{
		// Break out of the loop if the main thread is signaling us to.
		if (Manager->KillThreadsCounter.GetValue() != 0)
		{
			break;
		}
		// Only enter BeginCompilingThreadLoop if the main thread has incremented BeginCompilingActiveCounter
		else if (BeginCompilingActiveCounter.GetValue() > 0)
		{
#if _MSC_VER && !XBOX
			extern INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo );
			if(!appIsDebuggerPresent())
			{
				__try
				{
					// Do the work
					Manager->BeginCompilingThreadLoop(ThreadId);
				}
				__except( CreateMiniDump( GetExceptionInformation() ) )
				{
#if !CONSOLE
					ErrorMessage = GErrorHist;
#endif
					// Use a memory barrier to ensure that the main thread sees the write to ErrorMessage before
					// the write to bTerminatedByError.
					appMemoryBarrier();

					bTerminatedByError = TRUE;
					BeginCompilingActiveCounter.Decrement();
					break;
				}
			}
			else
#endif
			{
				Manager->BeginCompilingThreadLoop(ThreadId);
			}
			BeginCompilingActiveCounter.Decrement();
			// Only enter BeginCompilingThreadLoop if the main thread has incremented BeginCompilingActiveCounter
		}
		else if (FinishCompilingActiveCounter.GetValue() > 0)
		{
#if _MSC_VER && !XBOX
			extern INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo );
			if(!appIsDebuggerPresent())
			{
				__try
				{
					// Do the work
					Manager->FinishCompilingThreadLoop(ThreadId);
				}
				__except( CreateMiniDump( GetExceptionInformation() ) )
				{
#if !CONSOLE
					ErrorMessage = GErrorHist;
#endif
					// Use a memory barrier to ensure that the main thread sees the write to ErrorMessage before
					// the write to bTerminatedByError.
					appMemoryBarrier();

					bTerminatedByError = TRUE;
					FinishCompilingActiveCounter.Decrement();
					break;
				}
			}
			else
#endif
			{
				Manager->FinishCompilingThreadLoop(ThreadId);
			}
			FinishCompilingActiveCounter.Decrement();
		}
		else
		{
			// Yield CPU time while waiting for work, sleep for 10ms
			//@todo - shut this thread down if not used for some amount of time
			appSleep(0.01f);
		}
	}

	return 0;
}

/** Called by the main thread only, reports exceptions in the worker threads */
void FShaderCompileThreadRunnable::CheckHealth() const
{
	if (bTerminatedByError)
	{
#if !CONSOLE
		GErrorHist[0] = 0;
#endif
		GIsCriticalError = FALSE;
		GError->Logf(TEXT("Shader Compiling thread %u exception:\r\n%s"), ThreadId, *ErrorMessage);
	}
}

/** Helper function that appends memory to a buffer */
void WorkerInputAppendMemory(const void* Ptr, INT Size, TArray<BYTE>& Buffer)
{
	INT OldNum = Buffer.Add(sizeof(Size) + Size);
	appMemcpy(&Buffer(OldNum), &Size, sizeof(Size));
	appMemcpy(&Buffer(OldNum) + sizeof(Size), Ptr, Size);
}

/** Helper function that reads memory from a buffer */
void WorkerOutputReadMemory(void* Dest, UINT Size, INT& CurrentPosition, const TArray<BYTE>& Buffer)
{
	check(CurrentPosition >= 0 && CurrentPosition + (INT)Size <= Buffer.Num());
	appMemcpy(Dest, &Buffer(CurrentPosition), Size);
	CurrentPosition += Size;
}

FShaderCompilingThreadManager* GShaderCompilingThreadManager = NULL;

FShaderCompilingThreadManager::FShaderCompilingThreadManager() :
	NumShadersCompiledDistributed(0),
	NumShadersCompiledLocal(0),
	NumDistributedCompiles(0),
	NumLocalCompiles(0),
	NumShaderMapsCompiled(0),
	BeginCompilingTime(0),
	DistributedCompilingTime(0),
	FinishCompilingTime(0),
	TotalDistributedCompilingTime(0),
	TotalLocalCompilingTime(0),
	NextThreadId(0),
	NumUnusedShaderCompilingThreads(0),
	ThreadedShaderCompileThreshold(4),
	MaxShaderJobBatchSize(30),
	EffectiveShaderJobBatchSize(0),
	PrecompileShadersJobThreshold(0),
	bAllowMultiThreadedShaderCompile(FALSE),
	bAllowDistributedShaderCompile(FALSE),
	bAllowDistributedShaderCompileForBuildPCS(FALSE),
	bMultithreadedCompile(FALSE),
	bDistributedCompile(FALSE),
	bHasPS3Jobs(FALSE),
	bHasXboxJobs(FALSE),
	bHasNGPJobs(FALSE),
#if _WIN64
	ShaderCompileWorkerName(TEXT("..\\Win64\\UE3ShaderCompileWorker.exe")),
#else
	ShaderCompileWorkerName(TEXT("..\\Win32\\UE3ShaderCompileWorker.exe")),
#endif
	CurrentMaterialName(NULL)
{
	// Read values from the engine ini
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowMultiThreadedShaderCompile"), bAllowMultiThreadedShaderCompile, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowDistributedShaderCompile"), bAllowDistributedShaderCompile, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bAllowDistributedShaderCompileForBuildPCS"), bAllowDistributedShaderCompileForBuildPCS, GEngineIni ));
	
	if (GIsBuildMachine 
		// Don't want to assume that others have XGE setup
		&& GIsEpicInternal
		&& bAllowDistributedShaderCompileForBuildPCS 
		&& appStristr(appCmdLine(), TEXT("PrecompileShaders")) != NULL)
	{
		bAllowDistributedShaderCompile = TRUE;
	}

	INT TempValue;
	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("NumUnusedShaderCompilingThreads"), TempValue, GEngineIni ));
	NumUnusedShaderCompilingThreads = TempValue;
	// Use all the cores on the build machines
	if (GIsBuildMachine || ParseParam(appCmdLine(), TEXT("USEALLAVAILABLECORES")))
	{
		NumUnusedShaderCompilingThreads = 0;
	}
	if (ParseParam(appCmdLine(), TEXT("MTCHILD")))
	{
		// during parallel cooking, we don't have extra cores so max of four threads prevents the machine from getting gummed up
		NumUnusedShaderCompilingThreads = GNumHardwareThreads - 4;
		// XGE only allows one task to be instigated per machine at any time
		bAllowDistributedShaderCompile = FALSE;
	}
	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("ThreadedShaderCompileThreshold"), TempValue, GEngineIni ));
	ThreadedShaderCompileThreshold = TempValue;
	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("MaxShaderJobBatchSize"), MaxShaderJobBatchSize, GEngineIni ));
	verify(GConfig->GetInt( TEXT("DevOptions.Shaders"), TEXT("PrecompileShadersJobThreshold"), PrecompileShadersJobThreshold, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bDumpShaderPDBs"), bDumpShaderPDBs, GEngineIni ));
	verify(GConfig->GetBool( TEXT("DevOptions.Shaders"), TEXT("bPromptToRetryFailedShaderCompiles"), bPromptToRetryFailedShaderCompiles, GEngineIni ));

	GRetryShaderCompilation = bPromptToRetryFailedShaderCompiles;

	ProcessId = 0;
#if _WINDOWS
	// Get the current process Id, this will be used by the worker app to shut down when it's parent is no longer running.
	ProcessId = GetCurrentProcessId();
#endif

	// Use a working directory unique to this game, process and thread so that it will not conflict 
	// With processes from other games, processes from the same game or threads in this same process.
	ShaderBaseWorkingDirectory = FString( appShaderDir() ) * TEXT("WorkingDirectory") PATH_SEPARATOR + FString(appGetGameName())
		+ PATH_SEPARATOR + appItoa(ProcessId) + PATH_SEPARATOR;
}

/** Returns the absolute path to the shader PDB directory */
FString FShaderCompilingThreadManager::GetShaderPDBPath()
{
	return FString(appBaseDir()) * FString(appShaderDir()) * TEXT("PDBDump") PATH_SEPARATOR;
}

/** 
 * Returns TRUE if shaders from different shader maps should be compiled together.  
 * Enabled if compiles can be distributed so that many shaders will be compiled at the same time.
 */
UBOOL FShaderCompilingThreadManager::IsDeferringCompilation() const 
{ 
	static UBOOL CommandLineOverrideChecked = FALSE;
	static UBOOL CommandLineForce;
	if (!CommandLineOverrideChecked)
	{
		CommandLineOverrideChecked = TRUE;
		CommandLineForce = ParseParam(appCmdLine(), TEXT("AllowDeferredShaderCompilation"));
		if (CommandLineForce)
		{
			debugf(TEXT("Allowing deferred shader compilation"));
		}
	}
	if (CommandLineForce)
	{
		return bAllowMultiThreadedShaderCompile; 
	}
	return bAllowDistributedShaderCompile && bAllowMultiThreadedShaderCompile; 
}

/** Adds a job to the compile queue, called by the main thread only. */
void FShaderCompilingThreadManager::AddJob(TRefCountPtr<FShaderCompileJob> NewJob)
{
	CompileQueue.AddItem(NewJob);
	if (NewJob->Target.Platform == SP_PS3)
	{
		bHasPS3Jobs = TRUE;
	}
	if (NewJob->Target.Platform == SP_XBOXD3D)
	{
		bHasXboxJobs = TRUE;
	}
	else if ( NewJob->Target.Platform == SP_NGP)
	{
		bHasNGPJobs = TRUE;
	}
}

/** Launches the worker, returns the launched Process Id. */
DWORD FShaderCompilingThreadManager::LaunchWorker(const FString& WorkingDirectory, DWORD ProcessId, UINT ThreadId, const FString& WorkerInputFile, const FString& WorkerOutputFile)
{
	// Setup the parameters that the worker application needs
	// Surround the working directory with double quotes because it may contain a space 
	// WorkingDirectory ends with a '\', so we have to insert another to meet the Windows commandline parsing rules 
	// http://msdn.microsoft.com/en-us/library/17w5ykft.aspx 
	const FString WorkerParameters = FString(TEXT("\"")) + FString(appBaseDir()) * WorkingDirectory + TEXT("\\\" ") + appItoa(ProcessId) + TEXT(" ") + appItoa(ThreadId) + TEXT(" ") + WorkerInputFile + TEXT(" ") + WorkerOutputFile;

	// Launch the worker process
	INT PriorityModifier = -1; // below normal
	if (ParseParam(appCmdLine(), TEXT("MTCHILD")))
	{
		PriorityModifier = -2; // idle
	}
#if DEBUG_SHADERCOMPILEWORKER
	// Note: Set breakpoint here and launch the shadercompileworker with WorkerParameters a cmd-line
	return 17;
#else
	void* WorkerHandle = appCreateProc(*ShaderCompileWorkerName, *WorkerParameters, TRUE, FALSE, FALSE, NULL, PriorityModifier);
	if( !WorkerHandle )
	{
		// If this doesn't error, the app will hang waiting for jobs that can never be completed
		appErrorf( TEXT( "Couldn't launch %s! Make sure the exe is in your binaries folder." ), *ShaderCompileWorkerName );
	}
	DWORD WorkerId = 0;
#if _WINDOWS
	// Get the worker's process Id from the returned handle
	WorkerId = GetProcessId(WorkerHandle);
	check(WorkerId > 0);
	CloseHandle(WorkerHandle);
#endif
	return WorkerId;
#endif
}

/** 
 * Processes shader compilation jobs, shared by all threads.  The Main thread has an Id of 0. 
 * If this is a distributed compile, this will just setup for the distribution step.
 */
void FShaderCompilingThreadManager::BeginCompilingThreadLoop(UINT CurrentThreadId)
{
	UBOOL bIsDone = FALSE;
	while (!bIsDone)
	{
		// Atomically read and increment the next job index to process.
		INT JobIndex = NextShaderToBeginCompiling.Increment() - 1;

		// Only continue if JobIndex is valid
		if (JobIndex < CompileQueue.Num())
		{
			FShaderCompileJob& CurrentJob = *CompileQueue(JobIndex);
			
			// Main thread only
			if (CurrentThreadId == 0 && bMultithreadedCompile)
			{
				for(INT ThreadIndex = 0;ThreadIndex < Threads.Num();ThreadIndex++)
				{
					Threads(ThreadIndex).CheckHealth();
				}
			}

#if PLATFORM_DESKTOP && !USE_NULL_RHI && !UE3_LEAN_AND_MEAN && !CONSOLE
			FString DebugDumpDir;
			FString DirectoryFriendlyShaderTypeName;
			if ( CurrentJob.ShaderType )
			{
				DirectoryFriendlyShaderTypeName = CurrentJob.ShaderType->GetName();
			}
			// Replace template directives with valid symbols so it can be used as a windows directory name
			DirectoryFriendlyShaderTypeName.ReplaceInline(TEXT("<"),TEXT("("));
			DirectoryFriendlyShaderTypeName.ReplaceInline(TEXT(">"),TEXT(")"));
			FString ShaderSubDir = CurrentMaterialName;
			if (CurrentJob.VFType)
			{
				ShaderSubDir = ShaderSubDir * CurrentJob.VFType->GetName();
			}
			ShaderSubDir = ShaderSubDir * DirectoryFriendlyShaderTypeName;

			UBOOL BeginCompileResult = FALSE;
#if !PLATFORM_MACOSX
			if (CurrentJob.Target.Platform == SP_PCD3D_SM5)
			{
				BeginCompileResult = D3D11BeginCompileShader(
					JobIndex,
					CurrentThreadId,
					*CurrentJob.SourceFilename,
					*CurrentJob.FunctionName,
					CurrentJob.Target,
					CurrentJob.Environment,
					CurrentJob.Output,
					bDebugDump,
					*ShaderSubDir
					);
			}
			else if (CurrentJob.Target.Platform == SP_PCD3D_SM3)
			{
				BeginCompileResult = D3D9BeginCompileShader(
					JobIndex,
					CurrentThreadId,
					*CurrentJob.SourceFilename,
					*CurrentJob.FunctionName,
					CurrentJob.Target,
					CurrentJob.Environment,
					CurrentJob.Output,
					bDebugDump,
					*ShaderSubDir);
			}
			else
#endif
			if (CurrentJob.Target.Platform == SP_PCOGL)
			{
				BeginCompileResult = OpenGLBeginCompileShader(
					JobIndex,
					CurrentThreadId,
					*CurrentJob.SourceFilename,
					*CurrentJob.FunctionName,
					CurrentJob.Target,
					CurrentJob.Environment,
					CurrentJob.Output,
					bDebugDump,
					*ShaderSubDir);
			}
#if !PLATFORM_MACOSX
			else if (CurrentJob.Target.Platform == SP_PS3 || CurrentJob.Target.Platform == SP_XBOXD3D ||
				CurrentJob.Target.Platform == SP_NGP || CurrentJob.Target.Platform == SP_WIIU)
			{
				// Console shaders use the global shader precompiler for that platform
				check(GConsoleShaderPrecompilers[CurrentJob.Target.Platform]);
				if (bDebugDump)
				{
					check(CurrentThreadId == 0);
					// Preprocess instead of compile the shader
					BeginCompileResult = PreprocessConsoleShader(
						*CurrentJob.SourceFilename, 
						*CurrentJob.FunctionName, 
						CurrentJob.Target, 
						CurrentJob.Environment, 
						*ShaderSubDir);

					if (BeginCompileResult)
					{
						// Make sure Output gets initialized with correct bytecode etc
						BeginCompileResult = PrecompileConsoleShader(
							JobIndex,
							CurrentThreadId,
							*CurrentJob.SourceFilename, 
							*CurrentJob.FunctionName, 
							CurrentJob.Target, 
							CurrentJob.Environment, 
							CurrentJob.Output);
					}
				}
				else
				{
					BeginCompileResult = PrecompileConsoleShader(
						JobIndex,
						CurrentThreadId,
						*CurrentJob.SourceFilename, 
						*CurrentJob.FunctionName, 
						CurrentJob.Target, 
						CurrentJob.Environment, 
						CurrentJob.Output);
				}
			}
#endif
			else
			{
				appErrorf(TEXT("Unhandled shader platform"));
			}

			if (!BeginCompileResult)
			{
				ShaderCompileErrorCounter.Increment();
			}

			if (!IsMultiThreadedCompile())
			{
				CurrentJob.bSucceeded = BeginCompileResult;
			}
#else
			{
				appErrorf(TEXT("Attempted to compile \'%s\' shader for platform %d on console."),*CurrentJob.SourceFilename,CurrentJob.Target.Platform);
			}
#endif
		}
		else
		{
			// Processing has begun for all jobs
			bIsDone = TRUE;
		}
	}
	// Flush the current batch
	FlushBatchedJobs(CurrentThreadId);
}

/** Processes the shader compilation results of a distributed compile, shared by all threads.  The Main thread has an Id of 0. */
void FShaderCompilingThreadManager::FinishCompilingThreadLoop(UINT CurrentThreadId)
{
	UBOOL bIsDone = FALSE;
	while (!bIsDone)
	{
		// Atomically read and increment the next job index to process.
		INT BatchedJobId = NextShaderToFinishCompiling.Increment() - 1;

		// Only continue if JobIndex is valid and no other threads have encountered a shader compile error
		if (BatchedJobId < NumBatchedShaderJobs.GetValue())
		{
			// Main thread only
			if (CurrentThreadId == 0 && bMultithreadedCompile)
			{
				for(INT ThreadIndex = 0;ThreadIndex < Threads.Num();ThreadIndex++)
				{
					Threads(ThreadIndex).CheckHealth();
				}
			}

			FinishWorkerCompile(BatchedJobId, CurrentThreadId);
		}
		else
		{
			// Processing has begun for all jobs
			bIsDone = TRUE;
		}
	}
}

/** Compiles shader jobs using XGE. */
void FShaderCompilingThreadManager::DistributedCompile()
{
	// All threads of a distributed build on the same machine work out of the same directory, so use a thread id of 0
	const FString WorkingDirectory = FString(appBaseDir()) * ShaderBaseWorkingDirectory + TEXT("0") + PATH_SEPARATOR;
	{
		const FString SearchString = WorkingDirectory + TEXT("WorkerOutput*.out");
		TArray<FString> StaleOutputFiles;
		GFileManager->FindFiles(StaleOutputFiles, *SearchString, TRUE, FALSE);
		// Delete any stale output files
		// These should have been removed after being processed, but can remain in rare cases
		for (INT FileIndex = 0; FileIndex < StaleOutputFiles.Num(); FileIndex++)
		{
			verify(GFileManager->Delete(*(WorkingDirectory + StaleOutputFiles(FileIndex))));
		}
	}
	
	FString XGETaskDescription;
	// Load the task template
	verify(appLoadFileToString(XGETaskDescription, *(FString(appShaderDir()) * TEXT("XGETaskDefinitionTemplate.xml"))));
	
	FString FileList;
	// Presize to minimize string allocations
	FileList.Empty(NumBatchedShaderJobs.GetValue() * 80);
	for (INT BatchedJobIndex = 0; BatchedJobIndex < NumBatchedShaderJobs.GetValue(); BatchedJobIndex++)
	{
		const FString IndexString = appItoa(BatchedJobIndex);
		// Add a task line for every batch of shader jobs
		FileList += FString(TEXT("        <Task SourceFile=\"WorkerInput")) + IndexString + TEXT(".in\" OutputFiles=\"WorkerOutput") + IndexString + (TEXT(".out\" />") LINE_TERMINATOR);
	}

	// XGE does not support distributing 64 bit apps
	const FString WorkerPath = FString(appBaseDir()) + TEXT("..\\Win32\\UE3ShaderCompileWorker.exe");

	// Surround the working directory with XML double quotes because it may contain a space 
	// WorkingDirectory ends with a '\', so we have to insert another to meet the Windows commandline parsing rules 
	// http://msdn.microsoft.com/en-us/library/17w5ykft.aspx 
	const FString FormattedWorkingDirectory = FString(TEXT("&quot;")) + WorkingDirectory + TEXT("\\&quot;");
	// Fill in the template with information for this compile
	XGETaskDescription = FString::Printf(*XGETaskDescription, *FormattedWorkingDirectory, *WorkerPath, appBaseDir(), *FileList);
	const FString TaskDefinitionPath = WorkingDirectory + TEXT("XGETaskDefinition.xml");
	// Save the completed task description to disk
	verify(appSaveStringToFile(XGETaskDescription, *TaskDefinitionPath));

	// Set bShowMonitor to TRUE to visualize the build progress
	const UBOOL bShowMonitor = FALSE;
	// Launch xgConsole, suppressing the DETACHED_PROCESS flag which causes xgConsole to crash
	// Remove /Silent to see xgConsole output in a console window, remove the bLaunchHidden parameter to launch the xgConsole window visible
	void* XGConsoleHandle = appCreateProc(TEXT("xgConsole.exe"), *(FString(TEXT("\"")) + TaskDefinitionPath + TEXT("\"") + TEXT(" /Silent") + (bShowMonitor ? TEXT(" /OpenMonitor") : TEXT(""))), FALSE, TRUE);
	checkf(XGConsoleHandle, TEXT("Failed to launch xgConsole.exe!  Make sure XGE is installed and xgConsole is in your path to use distributed shader compiling."));
	UBOOL bDistributionComplete = FALSE;
	INT ReturnCode = 1;
	while (!bDistributionComplete)
	{
		// Block until xgConsole returns, at which point all of the output files will have been written out
		bDistributionComplete = appGetProcReturnCode(XGConsoleHandle, &ReturnCode);
		if (!bDistributionComplete)
		{
			appSleep(0.1f);
		}
	}
	checkf(ReturnCode == 0, TEXT("XGE distribution failed, there's currently no fallback."));
}

/** Writes out a worker input file for the thread's BatchedJobs. */
void FShaderCompilingThreadManager::FlushBatchedJobs(UINT CurrentThreadId)
{
	FShaderCompileThreadRunnable& CurrentThread = Threads(CurrentThreadId);
	if (CurrentThread.BatchedJobs.Num() > 0)
	{
		const INT CurrentBatchId = NumBatchedShaderJobs.Increment() - 1;
		// Distributed compiles always use the same directory
		const FString WorkingDirectory = ShaderBaseWorkingDirectory + (IsDistributedShaderCompile() ? TEXT("0") : appItoa(CurrentThreadId));
		// Write out the file that the worker app is waiting for, which has all the information needed to compile the shader.
		// 'Only' indicates to the worker that it should log and continue checking for the input file after the first one is processed
		const FString TransferFileName = WorkingDirectory * TEXT("WorkerInput") + (IsDistributedShaderCompile() ? appItoa(CurrentBatchId) : TEXT("Only")) + TEXT(".in");
		FArchive* TransferFile = NULL;
		INT RetryCount = 0;
		// Retry over the next two seconds if we can't write out the input file
		// Anti-virus and indexing applications can interfere and cause this write to fail
		//@todo - switch to shared memory or some other method without these unpredictable hazards
		while (TransferFile == NULL && RetryCount < 20)
		{
			if (RetryCount > 0)
			{
				appSleep(0.1f);
			}
			TransferFile = GFileManager->CreateFileWriter(*TransferFileName, FILEWRITE_EvenIfReadOnly);
			RetryCount++;
		}
		if (TransferFile == NULL)
		{
			TransferFile = GFileManager->CreateFileWriter(*TransferFileName, FILEWRITE_EvenIfReadOnly | FILEWRITE_NoFail);
		}
		check(TransferFile);

		INT ShaderCompileWorkerInputVersion = 0;
		TransferFile->Serialize(&ShaderCompileWorkerInputVersion, sizeof(ShaderCompileWorkerInputVersion));
		INT NumBatches = CurrentThread.BatchedJobs.Num();
		TransferFile->Serialize(&NumBatches, sizeof(NumBatches));

		// Serialize all the batched jobs
		for (INT BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
		{
			TRefCountPtr<FBatchedShaderCompileJob> BatchedJob = CurrentThread.BatchedJobs(BatchIndex);
			TransferFile->Serialize(&BatchedJob->JobId, sizeof(BatchedJob->JobId));
			INT InputLength = BatchedJob->WorkerInput.Num();
			TransferFile->Serialize(&InputLength, sizeof(InputLength));
			INT bIsEncrypted = 0;
			bIsEncrypted = 1;
			SecurityByObscurityEncryptAndDecrypt(BatchedJob->WorkerInput);
			TransferFile->Serialize(&bIsEncrypted, sizeof(bIsEncrypted));
			TransferFile->Serialize(&BatchedJob->WorkerInput(0), InputLength);
		}

		TransferFile->Close();
		delete TransferFile;
		CurrentThread.BatchedJobs.Empty(CurrentThread.BatchedJobs.Num());

		// If this is a local multi-threaded compile, launch the worker and process the results now.
		// If this is a distributed compile, setup is complete for this batch of jobs, and the input files will be consumed in the distribution step.
		if (!IsDistributedShaderCompile())
		{
			FinishWorkerCompile(CurrentBatchId, CurrentThreadId);
		}
	}
}

/** Enqueues compilation of a single job through the worker application, called from all the threads. */
void FShaderCompilingThreadManager::BeginWorkerCompile(TRefCountPtr<FBatchedShaderCompileJob> BatchedJob)
{
	FShaderCompileThreadRunnable& CurrentThread = Threads(BatchedJob->ThreadId);
	// Distributed compiles always use the same directory
	const FString WorkingDirectory = ShaderBaseWorkingDirectory + (IsDistributedShaderCompile() ? TEXT("0") : appItoa(BatchedJob->ThreadId));

	// For PS3 shaders, the precompiler expects a shader's includes to be on disk in the same directory as the main shader,
	// But multiple threads are writing out Material.usf and VertexFactory.usf at the same time,
	// So we must duplicate all the shader files into each working directory.
	// Only do the copy once, the first time it is needed.
	//@todo - implement the include handler for PS3
	if ( BatchedJob->JobType == WJT_PS3Shader
		&& !CurrentThread.bCopiedShadersToWorkingDirectory)
	{
		checkSlow(!IsDistributedShaderCompile());
		const FString SearchString = FString( appShaderDir() ) * TEXT("*.usf");
		TArray<FString> ShaderFiles;
		GFileManager->FindFiles(ShaderFiles, *SearchString, TRUE, FALSE);
		for (INT ShaderIndex = 0; ShaderIndex < ShaderFiles.Num(); ShaderIndex++)
		{
			const FString SourcePath = FString( appShaderDir() ) * ShaderFiles(ShaderIndex);
			const FString DestPath = WorkingDirectory * ShaderFiles(ShaderIndex);
			const DWORD CopyResult = GFileManager->Copy(*DestPath, *SourcePath, TRUE, TRUE);
			checkf(CopyResult == COPY_OK, TEXT("Failed to copy shader file %s to %s with copy result %u!"), *SourcePath, *DestPath, CopyResult);
		}
		CurrentThread.bCopiedShadersToWorkingDirectory = TRUE;
	}

	if (CurrentThread.BatchedJobs.Num() + 1 < EffectiveShaderJobBatchSize)
	{
		// Add the job to the current batch if we haven't reached the batch size yet
		CurrentThread.BatchedJobs.AddItem(BatchedJob);
	}
	else
	{
		CurrentThread.BatchedJobs.AddItem(BatchedJob);
		FlushBatchedJobs(BatchedJob->ThreadId);
	}
}

/** Launches the worker for non-distributed compiles, reads in the worker output and passes it to the platform specific finish compiling handler. */
void FShaderCompilingThreadManager::FinishWorkerCompile(INT BatchedJobId, UINT ThreadId)
{
	FShaderCompileThreadRunnable& CurrentThread = Threads(ThreadId);
	TArray<BYTE> WorkerOutput;

	// Distributed compiles always use the same directory
	const FString WorkingDirectory = ShaderBaseWorkingDirectory + (IsDistributedShaderCompile() ? TEXT("0") : appItoa(ThreadId)) + PATH_SEPARATOR;
	// 'Only' indicates to the worker that it should log and continue checking for the input file after the first one is processed
	const FString InputFileName = FString(TEXT("WorkerInput")) + (IsDistributedShaderCompile() ? appItoa(BatchedJobId) : TEXT("Only")) + TEXT(".in");
	const FString OutputFileName = FString(TEXT("WorkerOutput")) + (IsDistributedShaderCompile() ? appItoa(BatchedJobId) : TEXT("Only")) + TEXT(".out");
	const FString OutputFileNameAndPath = WorkingDirectory + OutputFileName;

	UBOOL bLoadedFile = FALSE;
	UBOOL bLaunchedOnce = FALSE;
	// Block until the worker writes out the output file
	while (!bLoadedFile)
	{
		bLoadedFile = appLoadFileToArray(WorkerOutput, *OutputFileNameAndPath, GFileManager, FILEREAD_Silent);
		// Launch the worker if this is not a distributed compile
		if (!IsDistributedShaderCompile() && !bLoadedFile)
		{
			if (CurrentThread.WorkerAppId == 0 || !appIsApplicationRunning(CurrentThread.WorkerAppId))
			{
				// Try to load the file again since it may have been written out since the last load attempt
				bLoadedFile = appLoadFileToArray(WorkerOutput, *OutputFileNameAndPath, GFileManager, FILEREAD_Silent);
				if (!bLoadedFile)
				{
#if !DEBUG_SHADERCOMPILEWORKER
					if (bLaunchedOnce)
					{
						// Check that the worker is still running, if not we are in a deadlock
						// Normal errors in the worker will still write out the output file, 
						// This should only happen if a breakpoint in the debugger caused the worker to exit due to inactivity.
						appErrorf(TEXT("%s terminated unexpectedly! ThreadId=%u"), 
							*ShaderCompileWorkerName,
							ThreadId
							);
					}
#endif

					// Store the Id with this thread so that we will know not to launch it again
					CurrentThread.WorkerAppId = LaunchWorker(WorkingDirectory, ProcessId, ThreadId, InputFileName, OutputFileName);
					bLaunchedOnce = TRUE;
				}
			}
			// Yield CPU time while we are waiting
			appSleep(0.01f);
		}
	}

	// Delete the output file now that we have consumed it, to avoid reading stale data on the next compile loop.
	UBOOL bDeletedOutput = GFileManager->Delete(*OutputFileNameAndPath, TRUE, TRUE);
	INT RetryCount = 0;
	// Retry over the next two seconds if we couldn't delete it
	while (!bDeletedOutput && RetryCount < 20)
	{
		appSleep(0.1f);
		bDeletedOutput = GFileManager->Delete(*OutputFileNameAndPath, TRUE, TRUE);
		RetryCount++;
	}
	checkf(bDeletedOutput, TEXT("Failed to delete %s!"), *OutputFileNameAndPath);

	INT CurrentPosition = 0;

	INT ShaderCompileWorkerOutputVersion;
	WorkerOutputReadValue(ShaderCompileWorkerOutputVersion, CurrentPosition, WorkerOutput);
	check(ShaderCompileWorkerOutputVersion == 0);

	INT NumBatches;
	WorkerOutputReadValue(NumBatches, CurrentPosition, WorkerOutput);

	for (INT BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
	{
		INT JobId;
		WorkerOutputReadValue(JobId, CurrentPosition, WorkerOutput);

		INT BatchBeginPosition = CurrentPosition;
		const BYTE WorkerErrorOutputVersion = 0;
		BYTE ReadVersion;
		WorkerOutputReadValue(ReadVersion, BatchBeginPosition, WorkerOutput);
		EWorkerJobType OutJobType;
		WorkerOutputReadValue(OutJobType, BatchBeginPosition, WorkerOutput);
		if (OutJobType == WJT_WorkerError)
		{
			// The worker terminated with an error, read it out of the file and assert on it.
			check(ReadVersion == WorkerErrorOutputVersion);
			UINT ErrorStringSize;
			WorkerOutputReadValue(ErrorStringSize, BatchBeginPosition, WorkerOutput);
			TCHAR* ErrorBuffer = new TCHAR[ErrorStringSize / sizeof(TCHAR) + 1];
			WorkerOutputReadMemory(ErrorBuffer, ErrorStringSize, BatchBeginPosition, WorkerOutput);
			ErrorBuffer[ErrorStringSize / sizeof(TCHAR)] = 0;
			appErrorf(TEXT("%s for thread %u terminated with message: \n %s"), *ShaderCompileWorkerName, ThreadId, ErrorBuffer);
			delete [] ErrorBuffer;
		}

		// This check can fail under very rare conditions. We haven't tracked that down yet.
		// Initiating a shader recompile should help
		check(CompileQueue.IsValidIndex(JobId));
		TRefCountPtr<FShaderCompileJob> CurrentJob = CompileQueue(JobId);
		check(!CurrentJob->bFinalized);
		CurrentJob->bFinalized = TRUE;

#if _WINDOWS && !USE_NULL_RHI && !UE3_LEAN_AND_MEAN && !CONSOLE

		// Do the platform specific processing of compilation results
		if (CurrentJob->Target.Platform == SP_PCD3D_SM3)
		{
			CurrentJob->bSucceeded = D3D9FinishCompilingShaderThroughWorker(
				CurrentJob->Target,
				CurrentPosition,
				WorkerOutput,
				CurrentJob->Output);
		}
		else if (CurrentJob->Target.Platform == SP_PCD3D_SM5)
		{
			CurrentJob->bSucceeded = D3D11FinishCompilingShaderThroughWorker(
				CurrentJob->Target,
				CurrentPosition,
				WorkerOutput,
				CurrentJob->Output);
		}
		else if (CurrentJob->Target.Platform == SP_PCOGL)
		{
			CurrentJob->bSucceeded = OpenGLFinishCompilingShaderThroughWorker(
				CurrentJob->Target,
				CurrentPosition,
				WorkerOutput,
				CurrentJob->Output);
		}
		else if (CurrentJob->Target.Platform == SP_PS3 || CurrentJob->Target.Platform == SP_XBOXD3D || CurrentJob->Target.Platform == SP_WIIU)
		{
			CurrentJob->bSucceeded = ConsoleFinishCompilingShaderThroughWorker(
				CurrentJob->Target,
				CurrentPosition,
				WorkerOutput,
				CurrentJob->Output);
		}
		else
#endif
		{
			checkf(0, TEXT("Unsupported shader platform"));
		}

		if (!CurrentJob->bSucceeded)
		{
			ShaderCompileErrorCounter.Increment();
		}
	}
}

/** Flushes all pending jobs for the given material. */
void FShaderCompilingThreadManager::FinishCompiling(TArray<TRefCountPtr<FShaderCompileJob> >& Results, const TCHAR* InMaterialName, UBOOL bForceLogErrors, UBOOL bInDebugDump)
{
	if (CompileQueue.Num() > 0)
	{
#if UDK
		// Always disable dumping shader source in UDK.
		bDebugDump = FALSE;
#else
		bDebugDump = bInDebugDump;
#endif
		// If no material name has been passed in, it's because we're doing a deferred compile which involves many materials
		CurrentMaterialName = InMaterialName ? InMaterialName : TEXT("Deferred");
		
		const DOUBLE StartTime = appSeconds();
		UBOOL bRetryCompile = FALSE;
		do
		{
			// Calculate how many Threads we should use for compiling shaders
			INT NumShaderCompilingThreads = (!bHasNGPJobs && bAllowMultiThreadedShaderCompile) ? Max<INT>(1,GNumHardwareThreads - NumUnusedShaderCompilingThreads) : 1;
			// Don't use multiple threads if we are dumping debug data or there are less jobs in the queue than the threshold.
			if (bDebugDump || (UINT)CompileQueue.Num() < ThreadedShaderCompileThreshold)
			{
				NumShaderCompilingThreads = 1;
			}

			NextShaderToBeginCompiling.Reset();
			NextShaderToFinishCompiling.Reset();
			NumBatchedShaderJobs.Reset();
			ShaderCompileErrorCounter.Reset();

			bMultithreadedCompile = NumShaderCompilingThreads > 1;

#if DEBUG_SHADERCOMPILEWORKER
			NumShaderCompilingThreads = 1;
			bMultithreadedCompile = TRUE;
#endif
			bDistributedCompile = 
				// Can't distribute PS3 shader jobs because they write out include files to the same folder
				//@todo - implement the include handler for PS3
				!bHasPS3Jobs
				&& !bHasNGPJobs
				// PDB dumping output files are currently not transferred back
				&& !(bDumpShaderPDBs && (bHasPS3Jobs || bHasXboxJobs))
				&& bMultithreadedCompile 
				&& bAllowDistributedShaderCompile 
				// Only distribute large amounts of compile jobs because small amounts are faster to compile locally due to distribution overhead
				&& CompileQueue.Num() > NumShaderCompilingThreads * MaxShaderJobBatchSize * 2;

			// Only batch shader jobs together if this is a multithreaded compile and there's enough for each thread to have a batch
			if (!bMultithreadedCompile || CompileQueue.Num() < NumShaderCompilingThreads * MaxShaderJobBatchSize)
			{
				EffectiveShaderJobBatchSize = 1;
			}
			// Small batches are more efficient if there won't be many batches per thread
			else if (CompileQueue.Num() < NumShaderCompilingThreads * MaxShaderJobBatchSize * 10)
			{
				EffectiveShaderJobBatchSize = MaxShaderJobBatchSize / 2;
			}
			else
			{
				// Use the maximum batch size for large numbers of jobs to minimize per-job distribution overhead
				EffectiveShaderJobBatchSize = MaxShaderJobBatchSize;
			}

			const DOUBLE StartBeginCompileTime = appSeconds();

			// Recreate threads if necessary
			if (Threads.Num() != NumShaderCompilingThreads)
			{
				// Stop the shader compiling threads
				// Signal the threads to break out of their loop
				KillThreadsCounter.Increment();
				for(INT ThreadIndex = 1;ThreadIndex < Threads.Num();ThreadIndex++)
				{
					// Wait for the thread to exit
					Threads(ThreadIndex).Thread->WaitForCompletion();
					// Report any unhandled exceptions by the thread
					Threads(ThreadIndex).CheckHealth();
					// Destroy the thread
					GThreadFactory->Destroy(Threads(ThreadIndex).Thread);
				}
				// All threads have terminated
				Threads.Empty();
				// Reset the kill counter
				KillThreadsCounter.Reset();

				// Create the shader compiling threads
				for(INT ThreadIndex = 0;ThreadIndex < NumShaderCompilingThreads;ThreadIndex++)
				{
					const FString ThreadName = FString::Printf(TEXT("ShaderCompilingThread%i"), NextThreadId);
					FShaderCompileThreadRunnable* ThreadRunnable = new(Threads) FShaderCompileThreadRunnable(this);
					// Index 0 is the main thread, it does not need a runnable thread
					if (ThreadIndex > 0)
					{
						ThreadRunnable->Thread = GThreadFactory->CreateThread(ThreadRunnable, *ThreadName, 0, 0, 0, TPri_Normal);
					}
				}
			}

			// Signal to the Threads to start processing CompileQueue
			for(INT ThreadIndex = 1;ThreadIndex < NumShaderCompilingThreads;ThreadIndex++)
			{
				Threads(ThreadIndex).BeginCompilingActiveCounter.Increment();
			}

			STAT(DOUBLE RHICompileTime = 0);
			{
				SCOPE_SECONDS_COUNTER(RHICompileTime);
				// Enter the job loop with the main thread
				// When this returns, all jobs will have begun
				// If this is a distributed compile, this will just setup the compile jobs.
				// If this is a local compile, this will also compile the shader jobs.
				BeginCompilingThreadLoop(0);

				// Wait for the shader compiling threads to finish their jobs
				for(INT ThreadIndex = 1;ThreadIndex < Threads.Num();ThreadIndex++)
				{
					while (Threads(ThreadIndex).BeginCompilingActiveCounter.GetValue() > 0)
					{
						// Yield CPU time while waiting
						appSleep(0);
						Threads(ThreadIndex).CheckHealth();
					}
				}

				if (IsDistributedShaderCompile())
				{
					const DOUBLE EndBeginCompilingTime = appSeconds();
					BeginCompilingTime += EndBeginCompilingTime - StartBeginCompileTime;

					// Compile the deferred jobs with distribution
					DistributedCompile();

					const DOUBLE EndDistributedCompilingTime = appSeconds();
					DistributedCompilingTime += EndDistributedCompilingTime - EndBeginCompilingTime;

					// Signal to the Threads to start processing deferred jobs
					for(INT ThreadIndex = 1;ThreadIndex < NumShaderCompilingThreads;ThreadIndex++)
					{
						Threads(ThreadIndex).FinishCompilingActiveCounter.Increment();
					}

					// Enter the job loop with the main thread
					// When this returns, all jobs will have begun
					FinishCompilingThreadLoop(0);

					// Wait for the shader compiling threads to finish their jobs
					for(INT ThreadIndex = 1;ThreadIndex < Threads.Num();ThreadIndex++)
					{
						while (Threads(ThreadIndex).FinishCompilingActiveCounter.GetValue() > 0)
						{
							// Yield CPU time while waiting
							appSleep(0);
							Threads(ThreadIndex).CheckHealth();
						}
					}
					FinishCompilingTime += appSeconds() - EndDistributedCompilingTime;
				}

				// Verify that all deferred jobs have been processed
				if (IsDeferringCompilation() && !bDebugDump)
				{
					for (INT JobIndex = 0; JobIndex < CompileQueue.Num(); JobIndex++)
					{
						check(CompileQueue(JobIndex)->bFinalized);
					}
				}
			}

			INC_FLOAT_STAT_BY(STAT_ShaderCompiling_RHI,(FLOAT)RHICompileTime);

			if (CompileQueue.Num() > 0)
			{
				switch (CompileQueue(0)->Target.Platform)
				{
					case SP_PS3:
						STAT(GRHIShaderCompileTime_PS3 += RHICompileTime);
						break;
					case SP_XBOXD3D:
						STAT(GRHIShaderCompileTime_XBOXD3D += RHICompileTime);
						break;
					case SP_PCD3D_SM3:
						STAT(GRHIShaderCompileTime_PCD3D_SM3 += RHICompileTime);
						break;
					case SP_PCD3D_SM5:
						STAT(GRHIShaderCompileTime_PCD3D_SM5 += RHICompileTime);
						break;
					case SP_PCOGL:
						STAT(GRHIShaderCompileTime_PCOGL += RHICompileTime);
						break;
					case SP_NGP:
						STAT(GRHIShaderCompileTime_NGP += RHICompileTime);
						break;
					case SP_WIIU:
						STAT(GRHIShaderCompileTime_WIIU += RHICompileTime);
						break;
					default:
						break;
				}
				STAT(GRHIShaderCompileTime_Total += RHICompileTime);
			}

#if PLATFORM_DESKTOP
			bRetryCompile = FALSE;
			if (CompileQueue.Num() > 0 
				&& ShaderCompileErrorCounter.GetValue() > 0
				// Shader error debugging is enabled by removing Suppress=DevShaders from BaseEngine.ini
				&& (!FName::SafeSuppressed(NAME_DevShaders) || bForceLogErrors))
			{
				TArray<const FShaderCompileJob*> ErrorJobs;
				// Gather unique errors
				TArray<FString> UniqueErrors;
				for (INT JobIndex = 0; JobIndex < CompileQueue.Num(); JobIndex++)
				{
					const FShaderCompileJob& CurrentJob = *CompileQueue(JobIndex);
					if (!CurrentJob.bSucceeded)
					{
						for (INT ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
						{
							const FShaderCompilerError& CurrentError = CurrentJob.Output.Errors(ErrorIndex);

							// Include warnings if DevShaders is unsuppressed, otherwise only include errors
							if (!FName::SafeSuppressed(NAME_DevShaders) || CurrentError.StrippedErrorMessage.InStr(TEXT("error"), FALSE, TRUE) != INDEX_NONE)
							{
							UniqueErrors.AddUniqueItem(CurrentJob.Output.Errors(ErrorIndex).GetErrorString());
								ErrorJobs.AddUniqueItem(&CurrentJob);
							}
						}
					}
				}

				// Assuming all the jobs are for the same platform
				const EShaderPlatform TargetShaderPlatform = (EShaderPlatform)CompileQueue(0)->Target.Platform;
				FString ErrorString = FString::Printf(TEXT("%i Shader compiler errors compiling %s for platform %s:"), UniqueErrors.Num(), CurrentMaterialName, ShaderPlatformToText(TargetShaderPlatform));
				warnf(NAME_Warning, *ErrorString);
				ErrorString += TEXT("\n");
							
				for (INT JobIndex = 0; JobIndex < CompileQueue.Num(); JobIndex++)
				{
					const FShaderCompileJob& CurrentJob = *CompileQueue(JobIndex);
					if (!CurrentJob.bSucceeded)
					{
						for (INT ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
						{
							FShaderCompilerError CurrentError = CurrentJob.Output.Errors(ErrorIndex);
							INT UniqueError = INDEX_NONE;
							if (UniqueErrors.FindItem(CurrentError.GetErrorString(), UniqueError))
							{
								// This unique error is being processed, remove it from the array
								UniqueErrors.Remove(UniqueError);
								
								// Remap filenames
								if (CurrentError.ErrorFile == TEXT("Material.usf"))
								{
									// MaterialTemplate.usf is dynamically included as Material.usf
									// Currently the material translator does not add new lines when filling out MaterialTemplate.usf,
									// So we don't need the actual filled out version to find the line of a code bug.
									CurrentError.ErrorFile = TEXT("MaterialTemplate.usf");
								}
								else if (CurrentError.ErrorFile.InStr(TEXT("memory")) != INDEX_NONE)
								{
									// Files passed to the shader compiler through memory will be named memory
									// Only the shader's main file is passed through memory without a filename
									CurrentError.ErrorFile = FString(CurrentJob.ShaderType->GetShaderFilename()) + TEXT(".usf");
								}
								else if (CurrentError.ErrorFile == TEXT("VertexFactory.usf"))
								{
									// VertexFactory.usf is dynamically included from whichever vertex factory the shader was compiled with.
									check(CurrentJob.VFType);
									CurrentError.ErrorFile = FString(CurrentJob.VFType->GetShaderFilename()) + TEXT(".usf");
								}
								else if (CurrentError.ErrorFile == TEXT("") && CurrentJob.ShaderType)
								{
									// Some shader compiler errors won't have a file and line number, so we just assume the error happened in file containing the entrypoint function.
									CurrentError.ErrorFile = FString(CurrentJob.ShaderType->GetShaderFilename()) + TEXT(".usf");
								}

								FString UniqueErrorString;
								if ( CurrentJob.ShaderType )
								{
									// Construct a path that will enable VS.NET to find the shader file, relative to the solution
									CurrentError.ErrorFile = FString( appShaderDir() ) * CurrentError.ErrorFile;
									UniqueErrorString = FString::Printf(TEXT("%s(%s): Shader %s, VF %s: %s\n"), 
										*CurrentError.ErrorFile, 
										*CurrentError.ErrorLineString, 
										CurrentJob.ShaderType->GetName(), 
										CurrentJob.VFType ? CurrentJob.VFType->GetName() : TEXT("None"), 
										*CurrentError.StrippedErrorMessage);
								}
								else
								{
									UniqueErrorString = FString::Printf(TEXT("%s(0): %s\n"), 
										*CurrentJob.SourceFilename, 
										*CurrentError.StrippedErrorMessage);
								}

#if !PLATFORM_MACOSX
								if (appIsDebuggerPresent() && !GIsBuildMachine)
								{
									// Using OutputDebugString to avoid any text getting added before the filename,
									// Which will throw off VS.NET's ability to take you directly to the file and line of the error when double clicking it in the output window.
									OutputDebugString(*UniqueErrorString);
								}
								else
#endif
								{
									warnf(NAME_Warning, *UniqueErrorString);
								}
								
								ErrorString += UniqueErrorString;
							}
						}
					}
				}
				
				if (!FName::SafeSuppressed(NAME_DevShaders) && bPromptToRetryFailedShaderCompiles)
				{
	#if _DEBUG
					// Use debug break in debug with the debugger attached, otherwise message box
					if (appIsDebuggerPresent())
					{
						// A shader compile error has occurred, see the debug output for information.
						// Double click the errors in the VS.NET output window and the IDE will take you directly to the file and line of the error.
						// Check ErrorJobs for more state on the failed shaders, for example in-memory includes like Material.usf
						appDebugBreak();
						// Set GRetryShaderCompilation to true in the debugger to enable retries in debug
						bRetryCompile = GRetryShaderCompilation;
					}
					else 
	#endif
					if (GWarn->YesNof( TEXT("%s\r\n\r\nRetry compilation?"), *ErrorString))
					{
						bRetryCompile = TRUE;
					}
				}

				if (bRetryCompile)
				{
					// Flush the shader file cache so that any changes will be propagated.
					FlushShaderFileCache();
					// Reset outputs
					for (INT JobIndex = 0; JobIndex < CompileQueue.Num(); JobIndex++)
					{
						FShaderCompileJob& CurrentJob = *CompileQueue(JobIndex);
							
						if (CurrentJob.VFType)
						{
							// Reload the vertex factory file from disk
							const FString VertexFactoryFile = LoadShaderSourceFile(CurrentJob.VFType->GetShaderFilename());
							CurrentJob.Environment.IncludeFiles.Set(TEXT("VertexFactory.usf"), *VertexFactoryFile);
						}

						// NOTE: Changes to MaterialTemplate.usf before retrying won't work, because the entry for Material.usf in CurrentJob.Environment.IncludeFiles isn't reset
						CurrentJob.Output = FShaderCompilerOutput();
						CurrentJob.bFinalized = FALSE;
					}
				}
			}
	#endif

		} while (bRetryCompile);

		NextThreadId = 0;
		Results = CompileQueue;
		CompileQueue.Empty();
		bHasPS3Jobs = FALSE;
		bHasXboxJobs = FALSE;
		bHasNGPJobs = FALSE;

		const FLOAT CompilingTime = appSeconds() - StartTime;
		if (IsDistributedShaderCompile())
		{
			NumShadersCompiledDistributed += Results.Num();
			NumDistributedCompiles++;
			TotalDistributedCompilingTime += CompilingTime;
		}
		else
		{
			NumShadersCompiledLocal += Results.Num();
			NumLocalCompiles++;
			TotalLocalCompilingTime += CompilingTime;
		}
	}
}
			
void FShaderCompilingThreadManager::DumpStats()
{
	warnf(NAME_DevShaders, TEXT("Total shader compiling time %.2fs (%.1f%% Distributed), Begin %.2fs, Distributed %.2fs, Finish %.2fs, %u shaders (%.1f%% Distributed), %u shader maps"), 
		TotalDistributedCompilingTime + TotalLocalCompilingTime,
		100.0f * TotalDistributedCompilingTime / (TotalDistributedCompilingTime + TotalLocalCompilingTime),
		BeginCompilingTime,
		DistributedCompilingTime,
		FinishCompilingTime,
		NumShadersCompiledDistributed + NumShadersCompiledLocal,
		100.0f * NumShadersCompiledDistributed / (FLOAT)(NumShadersCompiledDistributed + NumShadersCompiledLocal),
		NumShaderMapsCompiled);

	warnf(NAME_DevShaders, TEXT("%u Distributed compiles with avg %.1f shaders, %u Local compiles with avg %.1f shaders"),
		NumDistributedCompiles,
		NumShadersCompiledDistributed / (FLOAT)NumDistributedCompiles,
		NumLocalCompiles,
		NumShadersCompiledLocal / (FLOAT)NumLocalCompiles);
}

/** Completes all pending shader compilation tasks. */
void FShaderCompilingThreadManager::FinishDeferredCompilation(const TCHAR* MaterialName, UBOOL bForceLogErrors, UBOOL bInDebugDump)
{
	if (CompileQueue.Num() > 0)
	{
		if (IsDistributedShaderCompile())
		{
			warnf(NAME_DevShaders, TEXT("Beginning distributed compile of %u shaders..."), CompileQueue.Num());
		}
		const DOUBLE StartTime = appSeconds();
		if( GIsEditor && !GIsUCC )
		{
			GWarn->PushStatus();
			const FString Message = FString(TEXT("Compiling shaders")) + (MaterialName ? (FString(TEXT(" for material ")) + MaterialName) : TEXT(""));
			GWarn->StatusUpdatef(-1, -1, *Message);
		}

		TArray<TRefCountPtr<FShaderCompileJob> > Results;
		FinishCompiling(Results, MaterialName, bForceLogErrors, bInDebugDump);

		TMap<UINT, TArray<TRefCountPtr<FShaderCompileJob> > > SortedResults;
		// Sort compiled results by shader map Id
		for (INT ResultIndex = 0; ResultIndex < Results.Num(); ResultIndex++)
		{
			TRefCountPtr<FShaderCompileJob> CurrentJob = Results(ResultIndex);
			TArray<TRefCountPtr<FShaderCompileJob> >* ResultArray = SortedResults.Find(CurrentJob->Id);
			if (!ResultArray)
			{
				ResultArray = &SortedResults.Set(CurrentJob->Id, TArray<TRefCountPtr<FShaderCompileJob> >());
			}
			ResultArray->AddItem(CurrentJob);
		}

		// Process the shader maps that were deferring their compilation
		for (TMap<FMaterialShaderMap*, TArray<FMaterial*> >::TIterator It(FMaterialShaderMap::ShaderMapsBeingCompiled); It; ++It)
		{
			TArray<FString> Errors;
			FMaterialShaderMap* ShaderMap = It.Key();
			TArray<TRefCountPtr<FShaderCompileJob> >* ResultArray = SortedResults.Find(ShaderMap->CompilingId);
			check(ResultArray);
			UBOOL bSuccess = TRUE;
			for (INT JobIndex = 0; JobIndex < ResultArray->Num(); JobIndex++)
			{
				FShaderCompileJob& CurrentJob = *(*ResultArray)(JobIndex);
				bSuccess = bSuccess && CurrentJob.bSucceeded;
				if (CurrentJob.bSucceeded)
				{
					check(CurrentJob.Output.Code.Num() > 0);
				}
				else 
				{
					for (INT ErrorIndex = 0; ErrorIndex < CurrentJob.Output.Errors.Num(); ErrorIndex++)
					{
						Errors.AddUniqueItem(CurrentJob.Output.Errors(ErrorIndex).StrippedErrorMessage);
					}
				}
			}
			ShaderMap->ProcessCompilationResults(*ResultArray, bSuccess);
			ShaderMap->bCompiledSuccessfully = bSuccess;
			if (bSuccess)
			{
				if (!MaterialName)
				{
					// Only initialize the shader map if this was a deferred compile
					ShaderMap->BeginInit();
				}
			}
			else
			{
				TArray<FMaterial*>& Materials = It.Value();
				for (INT MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
				{
					FMaterial& CurrentMaterial = *Materials(MaterialIndex);
					// Propagate error messages
					CurrentMaterial.CompileErrors = Errors;

					// If compilation is being deferred, handle failed compilation here
					if (!MaterialName)
					{
						// Set the shader map reference to NULL so the default material will be used
						CurrentMaterial.ShaderMap = NULL;
						if (CurrentMaterial.IsSpecialEngineMaterial())
						{
							// Assert if the default material could not be compiled, since there will be nothing for other failed materials to fall back on.
							appErrorf(TEXT("Failed to compile default material %s!"), *CurrentMaterial.GetBaseMaterialPathName());
						}

						warnf(NAME_Warning, TEXT("Failed to compile Material %s for platform %s, Default Material will be used in game."), 
							*CurrentMaterial.GetBaseMaterialPathName(), 
							ShaderPlatformToText(ShaderMap->GetShaderPlatform()));

						for (INT ErrorIndex = 0; ErrorIndex < Errors.Num(); ErrorIndex++)
						{
							warnf(NAME_DevShaders, TEXT("	%s"), *Errors(ErrorIndex));
						}
					}
				}
			}
		}

		FMaterialShaderMap::ShaderMapsBeingCompiled.Empty();

		for (TMap<UINT, const ANSICHAR*>::TIterator It(FMaterialShaderMap::MaterialCodeBeingCompiled); It; ++It)
		{
			delete [] It.Value();
		}
		FMaterialShaderMap::MaterialCodeBeingCompiled.Empty();

		if( GIsEditor && !GIsUCC )
		{
			// We called StatusUpdatef earlier, so we want to restore whatever the previous status was now
			GWarn->PopStatus();
		}

		const DOUBLE EndTime = appSeconds();
		NumShaderMapsCompiled += SortedResults.Num();

		if (IsDistributedShaderCompile())
		{
			warnf(NAME_DevShaders, TEXT("	FinishDeferredCompilation %u shaders, %u shader maps, %s with %u threads, %.1fs"), 
				Results.Num(), 
				SortedResults.Num(), 
				(IsDistributedShaderCompile() ? TEXT("Distributed") : TEXT("Local")), 
				Threads.Num(),
				EndTime - StartTime);
			DumpStats();
		}
	}
}

/** Compiles deferred shaders for the precompile shaders commandlet if the job limit has been reached. */
void FShaderCompilingThreadManager::ConditionallyCompileForPCS()
{
	// Flush deferred shader compiling once a bunch of shaders are queued
	if (GetNumDeferredJobs() > (UINT)PrecompileShadersJobThreshold)
	{
		FinishDeferredCompilation(NULL, TRUE);
	}
}

void FShaderCompilerEnvironment::AddIncludesForWorker(TArray<BYTE>& WorkerInput) const
{
	INT NumIncludes = IncludeFiles.Num();
	if (MaterialShaderCode)
	{
		NumIncludes++;
	}
	if (VFFileName)
	{
		NumIncludes++;
	}
	WorkerInputAppendValue(NumIncludes, WorkerInput);

	for(TMap<FString,FString>::TConstIterator IncludeIt(IncludeFiles); IncludeIt; ++IncludeIt)
	{
		const FString& IncludeName = IncludeIt.Key();
		WorkerInputAppendMemory(TCHAR_TO_ANSI(*IncludeName), IncludeName.Len(), WorkerInput);

		const FString& IncludeFile = IncludeIt.Value();
		FTCHARToANSI AnsiFile(*IncludeFile);
		WorkerInputAppendMemory((ANSICHAR*)AnsiFile, AnsiFile.Length(), WorkerInput);
	}
	if (MaterialShaderCode)
	{
		const TCHAR* MaterialTemplate = TEXT("Material.usf");
		WorkerInputAppendMemory(TCHAR_TO_ANSI(MaterialTemplate), appStrlen(MaterialTemplate), WorkerInput);
		WorkerInputAppendMemory(MaterialShaderCode, strlen(MaterialShaderCode), WorkerInput);
	}
	if (VFFileName)
	{
		const TCHAR* VertexFactory = TEXT("VertexFactory.usf");
		WorkerInputAppendMemory(TCHAR_TO_ANSI(VertexFactory), appStrlen(VertexFactory), WorkerInput);
		const FString VertexFactoryFile = LoadShaderSourceFile(VFFileName);
		WorkerInputAppendMemory(TCHAR_TO_ANSI(*VertexFactoryFile), VertexFactoryFile.Len(), WorkerInput);
	}
}

INT FShaderCompilerEnvironment::AddIncludesForDll(char**& IncludeFileNames, char**& IncludeFileContents) const
{
	INT NumIncludes = IncludeFiles.Num();
	if (MaterialShaderCode)
	{
		NumIncludes++;
	}
	if (VFFileName)
	{
		NumIncludes++;
	}

	IncludeFileNames = new char*[NumIncludes];
	IncludeFileContents = new char*[NumIncludes];

#if !CONSOLE
	INT IncludeIndex = 0;
	for(TMap<FString,FString>::TConstIterator IncludeIt(IncludeFiles); IncludeIt; ++IncludeIt)
	{
		IncludeFileNames[IncludeIndex] = _strdup(TCHAR_TO_ANSI(*IncludeIt.Key()));
		IncludeFileContents[IncludeIndex] = _strdup(TCHAR_TO_ANSI(*IncludeIt.Value()));
		IncludeIndex++;
	}

	if (MaterialShaderCode)
	{
		const TCHAR* MaterialTemplate = TEXT("Material.usf");
		IncludeFileNames[IncludeIndex] = _strdup(TCHAR_TO_ANSI(MaterialTemplate));
		IncludeFileContents[IncludeIndex] = _strdup(MaterialShaderCode);
		IncludeIndex++;
	}

	if (VFFileName)
	{
		const TCHAR* VertexFactory = TEXT("VertexFactory.usf");
		IncludeFileNames[IncludeIndex] = _strdup(TCHAR_TO_ANSI(VertexFactory));
		const FString VertexFactoryFile = LoadShaderSourceFile(VFFileName);
		IncludeFileContents[IncludeIndex] = _strdup(TCHAR_TO_ANSI(*VertexFactoryFile));
		IncludeIndex++;
	}
#endif

	return NumIncludes;
}

/** Enqueues a shader compile job with GShaderCompilingThreadManager. */
void BeginCompileShader(
	UINT Id, 
	FVertexFactoryType* VFType,
	FShaderType* ShaderType,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	const FShaderCompilerEnvironment& InEnvironment
	)
{
	FShaderCompilerEnvironment Environment( InEnvironment );

	// #define PIXELSHADER and VERTEXSHADER accordingly
	{
		Environment.Definitions.Set( TEXT("PIXELSHADER"),  (Target.Frequency == SF_Pixel) ? TEXT("1") : TEXT("0") );
		Environment.Definitions.Set( TEXT("DOMAINSHADER"), (Target.Frequency == SF_Domain) ? TEXT("1") : TEXT("0") );
		Environment.Definitions.Set( TEXT("HULLSHADER"), (Target.Frequency == SF_Hull) ? TEXT("1") : TEXT("0") );
		Environment.Definitions.Set( TEXT("VERTEXSHADER"), (Target.Frequency == SF_Vertex) ? TEXT("1") : TEXT("0") );
		Environment.Definitions.Set( TEXT("GEOMETRYSHADER"), (Target.Frequency == SF_Geometry) ? TEXT("1") : TEXT("0") );
		Environment.Definitions.Set( TEXT("COMPUTESHADER"), (Target.Frequency == SF_Compute) ? TEXT("1") : TEXT("0") );
	}

	UBOOL bAllowedPlatform = (Target.Platform == SP_PCD3D_SM3 || Target.Platform == SP_PCD3D_SM5);
	Environment.Definitions.Set(TEXT("ALLOW_NVIDIA_STEREO_3D"), (GAllowNvidiaStereo3d && bAllowedPlatform) ? TEXT("1") : TEXT("0"));

#if WITH_REALD
	// NOTE: if you add a new platform you will also need to add it to the RealD change in the
	//       following files:
	//          - UnBuild.h: WITH_REALD macro definition
	//          - UE3BuildExternal.cs: SetUpRealDEnvironment() function
	Environment.Definitions.Set(TEXT("WITH_REALD"), (1 && (Target.Platform == SP_PCD3D_SM3 || Target.Platform == SP_PCD3D_SM4 /*|| Target.Platform == SP_XBOXD3D*/)) ? TEXT("1") : TEXT("0"));
#endif	//WITH_REALD

	// Create a new job and enqueue it with the shader compiling manager
	TRefCountPtr<FShaderCompileJob> NewJob = new FShaderCompileJob(Id, VFType, ShaderType, SourceFilename, FunctionName, Target, Environment);
	GShaderCompilingThreadManager->AddJob(NewJob);
}

FSimpleScopedTimer::FSimpleScopedTimer(const TCHAR* InInfoStr, FName InSuppressName, FLOAT InUnsuppressThreshold) :
	InfoStr(InInfoStr),
	SuppressName(InSuppressName),
	bAlreadyStopped(FALSE),
	UnsuppressThreshold(InUnsuppressThreshold)
{
	StartTime = appSeconds();
}

void FSimpleScopedTimer::Stop(UBOOL DisplayLog)
{
	if (!bAlreadyStopped)
	{
		bAlreadyStopped = TRUE;
		const DOUBLE TimeElapsed = appSeconds() - StartTime;
		if (DisplayLog)
		{
			if (TimeElapsed > UnsuppressThreshold)
			{
				warnf(NAME_Warning, TEXT("		[%s] took [%.4f] s"),*InfoStr,TimeElapsed);
			}
			else
			{
				warnf((EName)SuppressName.GetIndex(), TEXT("		[%s] took [%.4f] s"),*InfoStr,TimeElapsed);
			}
		}
	}
}

FSimpleScopedTimer::~FSimpleScopedTimer()
{
	Stop(TRUE);
}
