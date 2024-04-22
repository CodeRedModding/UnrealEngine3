/*=============================================================================
	ShaderCompiler.h: Platform independent shader compilation definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SHADERCOMPILER_H__
#define __SHADERCOMPILER_H__

enum EShaderFrequency
{
	SF_Vertex			= 0,
	SF_Hull				= 1,
	SF_Domain			= 2,
	SF_Pixel			= 3,
	SF_Geometry			= 4,
	SF_Compute			= 5,

	SF_NumFrequencies	= 6,
	SF_NumBits			= 3,
};

inline EMemoryStats GetMemoryStatType(EShaderFrequency ShaderFrequency)
{
	checkAtCompileTime(6 == SF_NumFrequencies, Bad_EShaderFrequency);
	switch(ShaderFrequency)
	{
	case SF_Vertex:		return STAT_VertexShaderMemory;
	case SF_Hull:		return STAT_VertexShaderMemory;
	case SF_Domain:		return STAT_VertexShaderMemory;
	case SF_Geometry:	return STAT_VertexShaderMemory;
	case SF_Pixel:		return STAT_PixelShaderMemory;
	case SF_Compute:	return STAT_PixelShaderMemory;
	default: return EMemoryStats(0);
	}
}

extern const TCHAR* GetShaderFrequencyName(EShaderFrequency ShaderFrequency);

/** @warning: update ShaderPlatformToText and GetMaterialPlatform when the below changes */
enum EShaderPlatform
{
	SP_PCD3D_SM3		= 0,
	SP_PS3				= 1,
	SP_XBOXD3D			= 2,
	SP_PCD3D_SM4		= 3,
	SP_PCD3D_SM5		= 4,
	SP_NGP				= 5,
	SP_PCOGL			= 6,
	SP_WIIU				= 7,

	SP_NumPlatforms		= 8,
	SP_NumBits			= 4,
};



namespace ShaderUtils
{
	/**
	 * Checks whether shaders other than SM3 should be allowed on PC platforms.  This can be used to disable shader models
	 * that we don't want to support in redistributed PC builds, such as UDK.  Note that this affects both runtime
	 * caching and cooking, as well as commandlets!
	 *
	 * @return	True if only D3D9 shaders should be allowed
	 */
	inline UBOOL ShouldForceSM3ShadersOnPC()
	{
		return FALSE;
	}
}


inline UBOOL IsPCPlatform( const EShaderPlatform Platform )
{
	return Platform == SP_PCD3D_SM3	|| Platform == SP_PCD3D_SM5 || Platform == SP_PCOGL;
}

/** Current shader platform. */
extern EShaderPlatform GRHIShaderPlatform;

/**
 * Converts shader platform to human readable string. 
 *
 * @param ShaderPlatform	Shader platform enum
 * @param bUseAbbreviation	Specifies whether, or not, to use the abbreviated name
 * @param bIncludeGLES2		True if we should even include GLES2, even though it doesn't have a shader platform
 * @return text representation of enum
 */
extern const TCHAR* ShaderPlatformToText( EShaderPlatform ShaderPlatform, UBOOL bUseAbbreviation = FALSE, UBOOL bIncludeGLES2 = FALSE );

/** Parses an EShaderPlatform from the given text, returns SP_NumPlatforms if not possible. */
extern EShaderPlatform ShaderPlatformFromText( const TCHAR* PlatformName );

/** Returns TRUE if debug viewmodes are allowed for the given platform. */
extern UBOOL AllowDebugViewmodes(EShaderPlatform Platform = GRHIShaderPlatform);

/** Converts UE3::EPlatformType console platforms to the appropriate EShaderPlatform, otherwise returns SP_PCD3D_SM3. */
extern EShaderPlatform ShaderPlatformFromUE3Platform(UE3::EPlatformType UE3Platform);

/** Converts EShaderPlatform to the appropriate UE3::EPlatformType console platforms, otherwise returns Windows. */
extern UE3::EPlatformType UE3PlatformFromShaderPlatform(EShaderPlatform ShaderPlatform);

enum EMaterialShaderQuality
{
	MSQ_HIGH				=0,
	MSQ_LOW                 =1,
	MSQ_MAX                 =2,

	// Terrain only supports high quality
	MSQ_TERRAIN				=MSQ_HIGH,

	// Use an invalid value for default value to functions when not specifying a quality
	MSQ_UNSPECIFIED			=MSQ_MAX,
};


struct FShaderTarget
{
	BITFIELD Frequency : SF_NumBits;
	BITFIELD Platform : SP_NumBits;
};

enum ECompilerFlags
{
	CFLAG_PreferFlowControl = 0,
	CFLAG_Debug,
	CFLAG_AvoidFlowControl,
	/** Disable shader validation */
	CFLAG_SkipValidation
};

/**
 * A map of shader parameter names to registers allocated to that parameter.
 */
class FShaderParameterMap
{
public:

	FShaderParameterMap() : UniformExpressionSet(NULL)
	{}

	UBOOL FindParameterAllocation(const TCHAR* ParameterName,WORD& OutBufferIndex,WORD& OutBaseIndex,WORD& OutSize,WORD& OutSamplerIndex) const;
	void AddParameterAllocation(const TCHAR* ParameterName,WORD BufferIndex,WORD BaseIndex,WORD Size,WORD SamplerIndex);
	/** Checks that all parameters are bound and asserts if any aren't in a debug build
	* @param InVertexFactoryType can be 0
	*/
	void VerifyBindingsAreComplete(const TCHAR* ShaderTypeName, EShaderFrequency Frequency, class FVertexFactoryType* InVertexFactoryType) const;

	DWORD GetCRC() const
	{
		DWORD ParameterCRC = 0;
		for(TMap<FString,FParameterAllocation>::TConstIterator ParameterIt(ParameterMap);ParameterIt;++ParameterIt)
		{
			const FString& ParamName = ParameterIt.Key();
			const FParameterAllocation& ParamValue = ParameterIt.Value();
			ParameterCRC = appMemCrc(*ParamName, ParamName.Len() * sizeof(TCHAR), ParameterCRC);
			ParameterCRC = appMemCrc(&ParamValue, sizeof(ParamValue), ParameterCRC);
		}
		return ParameterCRC;
	}

	/** Output uniform expressions from binding. */
	mutable const class FUniformExpressionSet* UniformExpressionSet;
private:
	struct FParameterAllocation
	{
		WORD BufferIndex;
		WORD BaseIndex;
		WORD Size;
		WORD SamplerIndex;
		mutable UBOOL bBound;

		FParameterAllocation() :
			bBound(FALSE)
		{}
	};

	// Verify that FParameterAllocation does not contain any padding, which would cause FShaderParameterMap::GetCRC to operate on garbage data
	checkAtCompileTime(sizeof(FParameterAllocation) == sizeof(WORD) * 4 + sizeof(UBOOL), FParameterAllocationContainsPadding);

	TMap<FString,FParameterAllocation> ParameterMap;
};

/**
 * The environment used to compile a shader.
 */
struct FShaderCompilerEnvironment
{
	TMap<FString,FString> IncludeFiles;
	const TCHAR* VFFileName;
	const ANSICHAR* MaterialShaderCode;
	TMap<FName,FString> Definitions;
	TArray<ECompilerFlags> CompilerFlags;

	FShaderCompilerEnvironment() :
		VFFileName(NULL),
		MaterialShaderCode(NULL)
	{}

	void AddIncludesForWorker(TArray<BYTE>& WorkerInput) const;

	INT AddIncludesForDll(char**& IncludeFileNames, char**& IncludeFileContents) const;
};

/** A shader compiler error or warning. */
struct FShaderCompilerError
{
	FShaderCompilerError() :
		ErrorFile(TEXT("")),
		ErrorLineString(TEXT("")),
		StrippedErrorMessage(TEXT(""))
	{}

	FString ErrorFile;
	FString ErrorLineString;
	FString StrippedErrorMessage;

	FString GetErrorString() const
	{
		return ErrorFile + TEXT("(") + ErrorLineString + TEXT("): ") + StrippedErrorMessage;
	}
};

/**
 * The output of the shader compiler.
 */
struct FShaderCompilerOutput
{
	FShaderCompilerOutput() :
		NumInstructions(0)
	{}

	FShaderParameterMap ParameterMap;
	TArray<FShaderCompilerError> Errors;
	FShaderTarget Target;
	TArray<BYTE> Code;
	UINT NumInstructions;
};

/**
 * Generates binary version of the shader files in Engine\Shaders\Binary\*.bin,
 * if they're out of date.
 */
void GenerateBinaryShaderFiles( );

/**
 * Loads the shader file with the given name.
 * @return The contents of the shader file.
 */
extern FString LoadShaderSourceFile(const TCHAR* Filename);

/** Enqueues a shader compile job with GShaderCompilingThreadManager. */
extern void BeginCompileShader(
	UINT Id, 
	class FVertexFactoryType* VFType,
	class FShaderType* ShaderType,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	const FShaderCompilerEnvironment& Environment
	);

/** Stores all of the input and output information used to compile a single shader. */
class FShaderCompileJob : public FRefCountedObject
{
public:
	/** Id of the shader map this shader belongs to. */
	UINT Id;
	/** Vertex factory type that this shader belongs to, may be NULL */
	FVertexFactoryType* VFType;
	/** Shader type that this shader belongs to, must be valid */
	FShaderType* ShaderType;
	/** Input for the shader compile */
	FString SourceFilename;
	FString FunctionName;
	FShaderTarget Target;
	FShaderCompilerEnvironment Environment;
	/** TRUE if the results of the shader compile have been processed. */
	UBOOL bFinalized;
	/** Output of the shader compile */
	UBOOL bSucceeded;
	FShaderCompilerOutput Output;

	FShaderCompileJob(
		const UINT& InId,
		FVertexFactoryType* InVFType,
		FShaderType* InShaderType,
		const TCHAR* InSourceFilename,
		const TCHAR* InFunctionName,
		FShaderTarget InTarget,
		const FShaderCompilerEnvironment& InEnvironment) 
		:
		Id(InId),
		VFType(InVFType),
		ShaderType(InShaderType),
		SourceFilename(InSourceFilename),
		FunctionName(InFunctionName),
		Target(InTarget),
		Environment(InEnvironment),
		bFinalized(FALSE),
		bSucceeded(FALSE)
	{
	}

	~FShaderCompileJob()
	{
	}
};

enum EWorkerJobType
{
	WJT_D3D9Shader = 0,
	WJT_D3D11Shader,
	WJT_XenonShader,
	WJT_PS3Shader,
	WJT_WiiUShader,
	WJT_WorkerError,
	WJT_JobTypeMax
};

/** Data from a single shader compile job that will be batched with others. */
class FBatchedShaderCompileJob : public FRefCountedObject
{
public:
	/** Index into FShaderCompilingThreadManager::CompileQueue */
	INT JobId;
	UINT ThreadId;
	EWorkerJobType JobType;
	TArray<BYTE> WorkerInput;

	FBatchedShaderCompileJob(
		const INT& InJobId,
		const UINT& InThreadId,
		EWorkerJobType InJobType) 
		:
		JobId(InJobId),
		ThreadId(InThreadId),
		JobType(InJobType)
	{
	}
};

/** Shader compiling thread, managed by FShaderCompilingThreadManager */
class FShaderCompileThreadRunnable : public FRunnable
{
	friend class FShaderCompilingThreadManager;
private:
	/** The manager for this thread */
	class FShaderCompilingThreadManager* Manager;
	/** The runnable thread */
	FRunnableThread* Thread;
	/** 
	 * Counter used to suspend and activate compilation.  
	 * The main thread increments this when Manager->CompileQueue can be processed by the thread, 
	 * and the thread decrements it when it is finished. 
	 */
	FThreadSafeCounter BeginCompilingActiveCounter;
	/** 
	 * Counter used to suspend and activate finalization.  
	 * The main thread increments this when distributed compilation outputs can be processed by the thread, 
	 * and the thread decrements it when it is finished. 
	 */
	FThreadSafeCounter FinishCompilingActiveCounter;
	/** Process Id of the worker application associated with this thread */
	DWORD WorkerAppId;
	/** Unique Id assigned by Manager and the index of this thread into Manager->Threads */
	UINT ThreadId;
	/** If the thread has been terminated by an unhandled exception, this contains the error message. */
	FString ErrorMessage;
	/** TRUE if the thread has been terminated by an unhandled exception. */
	UBOOL bTerminatedByError;
	/** Indicates whether shaders have been copied to the working directory for this run */
	UBOOL bCopiedShadersToWorkingDirectory;

	/** Jobs that are being combined in one batch to be compiled together. */
	TArray<TRefCountPtr<FBatchedShaderCompileJob> > BatchedJobs;

public:
	/** Initialization constructor. */
	FShaderCompileThreadRunnable(class FShaderCompilingThreadManager* InManager);

	// FRunnable interface.
	virtual UBOOL Init(void) { return TRUE; }
	virtual void Exit(void) {}
	virtual void Stop(void) {}
	virtual DWORD Run(void);

	/** Checks the thread's health, and passes on any errors that have occured.  Called by the main thread. */
	void CheckHealth() const;
};

/** Helper function that writes an arbitrary typed value to a buffer */
template<class T>
void WorkerInputAppendValue(T& Value, TArray<BYTE>& Buffer)
{
	INT OldNum = Buffer.Add(sizeof(Value));
	appMemcpy(&Buffer(OldNum), &Value, sizeof(Value));
}

/** Helper function that appends memory to a buffer */
void WorkerInputAppendMemory(const void* Ptr, INT Size, TArray<BYTE>& Buffer);

/** Helper function that reads an arbitrary typed value from a buffer */
template<class T>
void WorkerOutputReadValue(T& Value, INT& CurrentPosition, const TArray<BYTE>& Buffer)
{
	check(CurrentPosition >= 0 && CurrentPosition + (INT)sizeof(Value) <= Buffer.Num());
	appMemcpy(&Value, &Buffer(CurrentPosition), sizeof(Value));
	CurrentPosition += sizeof(Value);
}

/** Helper function that reads memory from a buffer */
void WorkerOutputReadMemory(void* Dest, UINT Size, INT& CurrentPosition, const TArray<BYTE>& Buffer);

/** Manages parallel shader compilation */
class FShaderCompilingThreadManager
{
	friend class FShaderCompileThreadRunnable;
private:

	/** Shader compiling stats. */
	INT NumShadersCompiledDistributed;
	INT NumShadersCompiledLocal;
	INT NumDistributedCompiles;
	INT NumLocalCompiles;
	INT NumShaderMapsCompiled;
	FLOAT BeginCompilingTime;
	FLOAT DistributedCompilingTime;
	FLOAT FinishCompilingTime;
	FLOAT TotalDistributedCompilingTime;
	FLOAT TotalLocalCompilingTime;

	/** Job queue, flushed by FinishCompiling.  Threads can only read from this when their BeginCompilingActiveCounter is incremented by the main thread. */
	TArray<TRefCountPtr<FShaderCompileJob> > CompileQueue;
	/** The next index into CompileQueue which processing hasn't started for yet. */
	FThreadSafeCounter NextShaderToBeginCompiling;
	/** The index of the next batch of jobs to finish processing. */
	FThreadSafeCounter NextShaderToFinishCompiling;
	/** The number of job batches that were created. */
	FThreadSafeCounter NumBatchedShaderJobs;
	/** Incremented by the main thread, Indicates to all threads that they must exit. */
	FThreadSafeCounter KillThreadsCounter;
	/** Incremented for each shader that failed to compile. */
	FThreadSafeCounter ShaderCompileErrorCounter;
	/** The threads spawned for shader compiling. */
	TIndirectArray<FShaderCompileThreadRunnable> Threads;
	/** Counter used to give each thread a unique Id. */
	UINT NextThreadId;
	/** Number of hardware threads that should not be used by shader compiling. */
	UINT NumUnusedShaderCompilingThreads;
	/** If there are less jobs than this, shader compiling will not use multiple threads. */
	UINT ThreadedShaderCompileThreshold;
	/** Largest number of jobs that can be put in the same batch. */
	INT MaxShaderJobBatchSize;
	/** Number of jobs in the same batch for the current compile, only valid in FinishCompiling. */
	INT EffectiveShaderJobBatchSize;
	/** 
	 * How many jobs PCS should accumulate before kicking off a distributed compile. 
	 * Larger values allow for better parallelization but use more memory, and sometimes end up in slower builds due to XGE scheduling overhead. 
	 */
	INT PrecompileShadersJobThreshold;
	DWORD ProcessId;
	/** Whether to allow multi threaded shader compiling. */
	UBOOL bAllowMultiThreadedShaderCompile;
	/** Whether to allow distributed shader compiling. */
	UBOOL bAllowDistributedShaderCompile;
	/** Whether to allow distributed shader compiling on the build machine while doing a precompile shaders commandlet. */
	UBOOL bAllowDistributedShaderCompileForBuildPCS;
	/** TRUE if the current compile is multi threaded, only valid in FinishCompiling. */
	UBOOL bMultithreadedCompile;
	/** TRUE if the current compile is distributed, only valid in FinishCompiling. */
	UBOOL bDistributedCompile;
	/** TRUE if the current compile is dumping debug shader data.  This forces the single-threaded compile path. */
	UBOOL bDebugDump;
	/** TRUE if the current compile should dump UPDB's.  This is compatible with the multi-threaded compile path. */
	UBOOL bDumpShaderPDBs;
	UBOOL bPromptToRetryFailedShaderCompiles;
	UBOOL bHasPS3Jobs;
	UBOOL bHasXboxJobs;
	UBOOL bHasNGPJobs;
	FString ShaderBaseWorkingDirectory;
	/** Name of the shader worker application. */
	const FString ShaderCompileWorkerName;
	const TCHAR* CurrentMaterialName;

	/** Can only be called from within FinishCompiling. */
	UBOOL IsDistributedShaderCompile() const { return bDistributedCompile; }

	/** Launches the worker, returns the launched Process Id. */
	DWORD LaunchWorker(const FString& WorkingDirectory, DWORD ProcessId, UINT ThreadId, const FString& WorkerInputFile, const FString& WorkerOutputFile);

	/** 
	 * Processes shader compilation jobs, shared by all threads.  The Main thread has an Id of 0. 
	 * If this is a distributed compile, this will just setup for the distribution step.
	 */
	void BeginCompilingThreadLoop(UINT CurrentThreadId);

	/** Processes the shader compilation results of a distributed compile, shared by all threads.  The Main thread has an Id of 0. */
	void FinishCompilingThreadLoop(UINT CurrentThreadId);

	/** Compiles shader jobs using XGE. */
	void DistributedCompile();

	/** Writes out a worker input file for the thread's BatchedJobs. */
	void FlushBatchedJobs(UINT CurrentThreadId);

	/** Launches the worker for non-distributed compiles, reads in the worker output and passes it to the platform specific finish compiling handler. */
	void FinishWorkerCompile(INT BatchedJobId, UINT ThreadId);

	/** Flushes all pending jobs for the given material, called from the main thread. */
	void FinishCompiling(TArray<TRefCountPtr<FShaderCompileJob> >& Results, const TCHAR* MaterialName, UBOOL bForceLogErrors, UBOOL bDebugDump);

public:
	
	/** Returns the absolute path to the shader PDB directory */
	static FString GetShaderPDBPath();

	FShaderCompilingThreadManager();

	/** Returns TRUE if the current job is multi threaded, can only be called from within FinishCompiling. */
	UBOOL IsMultiThreadedCompile() const { return bMultithreadedCompile; }

	/** 
	 * Returns TRUE if shaders from different shader maps should be compiled together.  
	 * Enabled if compiles can be distributed so that many shaders will be compiled at the same time.
	 */
	UBOOL IsDeferringCompilation() const;

	/** Returns TRUE if the current job should dump shader PDBs. */
	UBOOL IsDumpingShaderPDBs() const { return bDumpShaderPDBs; }

	UINT GetNumDeferredJobs() const { return CompileQueue.Num(); }

	/** Adds a job to the compile queue, called from the main thread. */
	void AddJob(TRefCountPtr<FShaderCompileJob> NewJob);

	/** Enqueues compilation of a single job through the worker application, called from all the threads. */
	void BeginWorkerCompile(TRefCountPtr<FBatchedShaderCompileJob> BatchedJob);

	void DumpStats();

	/** Completes all pending shader compilation tasks. */
	void FinishDeferredCompilation(const TCHAR* MaterialName = NULL, UBOOL bForceLogErrors=FALSE, UBOOL bInDebugDump=FALSE);

	/** Compiles deferred shaders for the precompile shaders commandlet if the job limit has been reached. */
	void ConditionallyCompileForPCS();

	friend void VerifyGlobalShaders(EShaderPlatform Platform);
	friend class UDumpShadersCommandlet;
	friend void NGPFinishCompileShaders( const TArray<struct FNGPShaderCompileInfo>& ShaderCompileInfos );
};

/** The global shader compiling thread manager. */
extern FShaderCompilingThreadManager* GShaderCompilingThreadManager;

/** The shader precompilers for each platform.  These are only set during the console shader compilation while cooking or in the PrecompileShaders commandlet. */
extern class FConsoleShaderPrecompiler* GConsoleShaderPrecompilers[SP_NumPlatforms];

enum EShaderCompilingStats
{
	STAT_ShaderCompiling_MaterialShaders = STAT_ShaderCompilingFirstStat,
	STAT_ShaderCompiling_GlobalShaders,
	STAT_ShaderCompiling_RHI,
	STAT_ShaderCompiling_LoadingShaderFiles,
	STAT_ShaderCompiling_HashingShaderFiles,
	STAT_ShaderCompiling_HLSLTranslation,
	STAT_ShaderCompiling_NumTotalMaterialShaders,
	STAT_ShaderCompiling_NumSpecialMaterialShaders,
	STAT_ShaderCompiling_NumTerrainMaterialShaders,
	STAT_ShaderCompiling_NumDecalMaterialShaders,
	STAT_ShaderCompiling_NumParticleMaterialShaders,
	STAT_ShaderCompiling_NumSkinnedMaterialShaders,
	STAT_ShaderCompiling_NumLitMaterialShaders,
	STAT_ShaderCompiling_NumUnlitMaterialShaders,
	STAT_ShaderCompiling_NumTransparentMaterialShaders,
	STAT_ShaderCompiling_NumOpaqueMaterialShaders,
	STAT_ShaderCompiling_NumMaskedMaterialShaders
};

enum EShaderCompressionStats
{
	STAT_ShaderCompression_NumShadersLoaded = STAT_ShaderCompressionFirstStat,
	STAT_ShaderCompression_RTShaderLoadTime,
	STAT_ShaderCompression_NumShadersUsedForRendering,
	STAT_ShaderCompression_TotalRTShaderInitForRenderingTime,
	STAT_ShaderCompression_FrameRTShaderInitForRenderingTime,
	STAT_ShaderCompression_CompressedShaderMemory,
	STAT_ShaderCompression_UncompressedShaderMemory
};

/** A simple timer which prints the time that an instance was in scope. */
class FSimpleScopedTimer
{
public:
	FSimpleScopedTimer(const TCHAR* InInfoStr, FName InSuppressName, FLOAT InUnsuppressThreshold);
	void Stop(UBOOL DisplayLog = TRUE);
	~FSimpleScopedTimer();

private:
	DOUBLE StartTime;
	FString InfoStr;
	FName SuppressName;
	UBOOL bAlreadyStopped;
	FLOAT UnsuppressThreshold;
};

/** A simple thread-safe accumulate counter that times how long an instance was in scope. */
class FSimpleScopedAccumulateCycleTimer
{
public:
	FSimpleScopedAccumulateCycleTimer(DWORD& InCounter) :
	  Counter(InCounter)
	  {
		  StartTime = appCycles();
	  }

	  ~FSimpleScopedAccumulateCycleTimer()
	  {
		  appInterlockedAdd((INT*)&Counter, appCycles() - StartTime);
	  }

private:
	DWORD StartTime;
	DWORD& Counter;
};

#endif // __SHADERCOMPILER_H__
