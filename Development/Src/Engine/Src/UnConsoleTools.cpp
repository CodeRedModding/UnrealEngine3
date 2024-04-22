/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "UnConsoleTools.h"

// we don't need any of this on console
#if !CONSOLE && !PLATFORM_MACOSX

#if _WINDOWS 
#include "PreWindowsApi.h"
#include <d3dx9.h>
#include "PostWindowsApi.h"
#endif

/**
 * TranslateCompilerFlag - translates the platform-independent compiler flags into D3DX defines
 * @param CompilerFlag - the platform-independent compiler flag to translate
 * @return DWORD - the value of the appropriate D3DX enum
 */
static DWORD TranslateCompilerFlag(ECompilerFlags CompilerFlag)
{
#if _WINDOWS
	//@todo: Make this platform independent.
	switch(CompilerFlag)
	{

	case CFLAG_PreferFlowControl: return D3DXSHADER_PREFER_FLOW_CONTROL;
	case CFLAG_Debug: return D3DXSHADER_DEBUG | D3DXSHADER_SKIPOPTIMIZATION;
	case CFLAG_AvoidFlowControl: return D3DXSHADER_AVOID_FLOW_CONTROL;
	case CFLAG_SkipValidation: return D3DXSHADER_SKIPVALIDATION;
	default: return 0;
	};
#else
	return 0;
#endif // _WINDOWS
}

/** Compiles a console shader directly through the console tools dll. */
static bool PrecompileConsoleShaderThroughDll(
	const TCHAR* SourceFilename, 
	const TCHAR* FunctionName, 
	const FShaderTarget& Target, 
	const FShaderCompilerEnvironment& Environment, 
	const TCHAR* Definitions,
	DWORD CompileFlags, 
	FString& ConstantTable,
	FShaderCompilerOutput& Output
	)
{
	// build up path to pass to precompiler
	FString ShaderDir = FString(appBaseDir()) * FString(appShaderDir());
	FString ShaderPath;
	if ( Target.Platform == SP_NGP )
	{
		ShaderDir *= TEXT("NGP");
		ShaderPath = FString(SourceFilename);	// For NGP, SourceFilename is an absolute path to a C-preprocessed file.
	}
	else
	{
		ShaderPath = ShaderDir * FString(SourceFilename) + TEXT(".usf");
	}

	// change the location to the user directory if needed (compiling a PS3 shader from a end-user's PC)
	// @todo ship: This requires all shaders to exist in the My Games dir, which is very non-ideal
	if (Target.Platform == SP_PS3)
	{
		ShaderPath = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*ShaderPath));
	}

	char** IncludeFileNames = NULL;
	char** IncludeFileContents = NULL;
	const INT NumIncludes = Environment.AddIncludesForDll(IncludeFileNames, IncludeFileContents);

	// write out the files that may need to be included
	for(INT i = 0; i < NumIncludes; i++)
	{
		//@todo - implement the include handler for PS3
		if (Target.Platform == SP_PS3)
		{
			// the name already has the .usf extension
			FString IncludePath = ShaderDir * ANSI_TO_TCHAR(IncludeFileNames[i]);
			if (appSaveStringToFile(ANSI_TO_TCHAR(IncludeFileContents[i]), *IncludePath, TRUE) == FALSE)
			{
				warnf(TEXT("ShaderPrecompiler: Failed to save to file %s - Is it READONLY?"), *IncludePath);
				return FALSE;
			}
		}
	}

	// allocate a huge buffer for constants and bytecode
	// @GEMINI_TODO: Validate this in the dll or something by passing in the size
	const UINT BytecodeBufferAllocatedSize = 1024 * 1024; // 1M
	BYTE* BytecodeBuffer = (BYTE*)appMalloc(BytecodeBufferAllocatedSize); 
	// Zero the bytecode in case the dll doesn't write to all of it,
	// since the engine caches shaders with the same bytecode.
	appMemzero(BytecodeBuffer, BytecodeBufferAllocatedSize);
	char* ConstantBuffer = (char*)appMalloc(256 * 1024); // 256k
	char* ErrorBuffer = (char*)appMalloc(256 * 1024); // 256k
	// to avoid crashing if the DLL doesn't set these
	ConstantBuffer[0] = 0;
	ErrorBuffer[0] = 0;
	INT BytecodeSize = 0;

	// call the DLL precompiler
	bool bSucceeded = GConsoleShaderPrecompilers[Target.Platform]->PrecompileShader(
		TCHAR_TO_ANSI(*ShaderPath), 
		TCHAR_TO_ANSI(FunctionName), 
		Target.Frequency == SF_Vertex, 
		CompileFlags, 
		TCHAR_TO_ANSI(Definitions),
		TCHAR_TO_ANSI(*(ShaderDir + PATH_SEPARATOR)),
		IncludeFileNames,
		IncludeFileContents,
		NumIncludes,
		GShaderCompilingThreadManager->IsDumpingShaderPDBs() == TRUE,
		TCHAR_TO_ANSI(*FShaderCompilingThreadManager::GetShaderPDBPath()),
		BytecodeBuffer, 
		BytecodeSize, 
		ConstantBuffer, 
		ErrorBuffer
		);

	// output any errors
	if (ErrorBuffer[0])
	{
		FShaderCompilerError NewError;
		NewError.StrippedErrorMessage = FString(ANSI_TO_TCHAR(ErrorBuffer));
		Output.Errors.AddItem(NewError);
	}
	else if (!bSucceeded)
	{
		FShaderCompilerError NewError;
		NewError.StrippedErrorMessage = FString::Printf(TEXT("Failed to compile shader file %s for platform %s without compile error message!"), SourceFilename, ShaderPlatformToText((EShaderPlatform)Target.Platform));
		Output.Errors.AddItem(NewError);
	}

	if (bSucceeded)
	{
		check(BytecodeSize > 0);
		// copy the bytecode into the output structure
		Output.Code.Empty(BytecodeSize);
		Output.Code.Add(BytecodeSize);
		appMemcpy(Output.Code.GetData(), BytecodeBuffer, BytecodeSize);
		ConstantTable = FString(ANSI_TO_TCHAR(ConstantBuffer));
	}

	// free buffers
	appFree(BytecodeBuffer);
	appFree(ConstantBuffer);
	appFree(ErrorBuffer);

	for (INT i = 0; i < NumIncludes; i++)
	{
		// These were allocated with _strdup, which uses malloc
		free(IncludeFileNames[i]);
		free(IncludeFileContents[i]);
	}
	delete [] IncludeFileNames;
	delete [] IncludeFileContents;
	
	return bSucceeded;
}

/** Compiles a shader through the worker application. */
static void PrecompileConsoleShaderThroughWorker(
	INT JobId,
	UINT ThreadId, 
	const TCHAR* SourceFilename, 
	const TCHAR* FunctionName, 
	LPCSTR IncludePath,
	const FShaderTarget& Target, 
	const FShaderCompilerEnvironment& Environment, 
	const TCHAR* Definitions,
	DWORD CompileFlags
	)
{
	EWorkerJobType JobType;
	if (Target.Platform == SP_XBOXD3D)
	{
		JobType = WJT_XenonShader;
	}
	else if (Target.Platform == SP_PS3)
	{
		JobType = WJT_PS3Shader;
	}
	else if (Target.Platform == SP_WIIU)
	{
		JobType = WJT_WiiUShader;
	}
	else
	{
		JobType = WJT_JobTypeMax;
		appErrorf(TEXT("Platform %s not supported through this shader compiling path!"), ShaderPlatformToText((EShaderPlatform)Target.Platform));
	}
	TRefCountPtr<FBatchedShaderCompileJob> BatchedJob = new FBatchedShaderCompileJob(JobId, ThreadId, JobType);
	TArray<BYTE>& WorkerInput = BatchedJob->WorkerInput;
	// Presize to avoid lots of allocations
	WorkerInput.Empty(1000);
	// Setup the input for the worker app, everything that is needed to compile the shader.
	// Note that any format changes here also need to be done in the worker
	// Write a job type so the worker app can know which compiler to invoke
	WorkerInputAppendValue(JobType, WorkerInput);
	// Version number so we can detect stale data
	const BYTE ConsoleShaderCompileWorkerInputVersion = 2;
	WorkerInputAppendValue(ConsoleShaderCompileWorkerInputVersion, WorkerInput);
	WorkerInputAppendMemory(appGetGameName(), appStrlen(appGetGameName()) * sizeof(TCHAR), WorkerInput);
	WorkerInputAppendMemory(TCHAR_TO_ANSI(SourceFilename), appStrlen(SourceFilename), WorkerInput);
	WorkerInputAppendMemory(TCHAR_TO_ANSI(FunctionName), appStrlen(FunctionName), WorkerInput);
	BYTE bIsVertexShader = Target.Frequency == SF_Vertex;
	WorkerInputAppendValue(bIsVertexShader, WorkerInput);
	WorkerInputAppendValue(CompileFlags, WorkerInput);
	WorkerInputAppendMemory(IncludePath, appStrlen(IncludePath) * sizeof(CHAR), WorkerInput);

	Environment.AddIncludesForWorker(WorkerInput);

	WorkerInputAppendMemory(TCHAR_TO_ANSI(Definitions), appStrlen(Definitions), WorkerInput);

	BYTE bIsDumpingShaderPDBs = GShaderCompilingThreadManager->IsDumpingShaderPDBs();
	WorkerInputAppendValue(bIsDumpingShaderPDBs, WorkerInput);
	const FString PDBPath = FShaderCompilingThreadManager::GetShaderPDBPath();
	WorkerInputAppendMemory(TCHAR_TO_ANSI(*PDBPath), PDBPath.Len(), WorkerInput);

	TArray<BYTE> WorkerOutput;
	// Invoke the worker
	GShaderCompilingThreadManager->BeginWorkerCompile(BatchedJob);
}

/** Processes the results of a console shader compilation. */
static UBOOL FinishCompilingConsoleShader(
	UBOOL bShaderCompileSucceeded,
	FShaderTarget Target,
	const FString& ConstantTable,
	FShaderCompilerOutput& Output)
{
	if (bShaderCompileSucceeded)
	{
		// make a string of the constant returns
		TArray<FString> ConstantArray;
		// break "WorldToLocal,100,4 Scale,101,1" into "WorldToLocal,100,4" and "Scale,101,1"
		ConstantTable.ParseIntoArray(&ConstantArray, TEXT(" "), TRUE);
		for (INT ConstantIndex = 0; ConstantIndex < ConstantArray.Num(); ConstantIndex++)
		{
			TArray<FString> ConstantValues;
			// break "WorldToLocal,100,4" into "WorldToLocal","100","4"
			ConstantArray(ConstantIndex).ParseIntoArray(&ConstantValues, TEXT(","), TRUE);

			// make sure we only have 3 values
			if (ConstantValues.Num() != 3)
			{
				warnf(NAME_Warning, TEXT("Shader precompiler returned a bad constant string [%s]"), *ConstantArray(ConstantIndex));
			}

			FString ConstantName = *ConstantValues(0);
			INT RegisterIndex = appAtoi(*ConstantValues(1));
			INT RegisterCount = appAtoi(*ConstantValues(2));
			//warnf(TEXT("Constant Table [%s : %d : %d]"), *ConstantName, RegisterIndex, RegisterCount);
			// collapse arrays down to the base parameter
			INT Bracket = ConstantName.InStr("[");
			if (Bracket != -1)
			{
				// convert the number after the [ to an integer
				INT ArraySize = appAtoi(*ConstantName.Right((ConstantName.Len() - Bracket) - 1)) + 1;
				// Struct array needs to accumulate the size, not multiplying registercount to ArraySize
				UBOOL StructArray = (ConstantName.InStr(".")!=-1);

				// cut down the name to before the [
				ConstantName = ConstantName.Left(Bracket);

				WORD ExistingRegisterIndex = RegisterIndex;
				WORD ExistingRegisterCount = 0;
				WORD ExistingConstantBufferIndex = 0;
				WORD ExistingSamplerIndex = 0;
				// find any existing thing with the base name
				Output.ParameterMap.FindParameterAllocation(*ConstantName, ExistingConstantBufferIndex, ExistingRegisterIndex, ExistingRegisterCount, ExistingSamplerIndex);

				// accumulate RegisterCount to ExistingRegisterCount if StructArray
				if (StructArray)
				{
					ArraySize = ExistingRegisterCount+RegisterCount;
				}
				else
				{
					// this is the old way we count the size, but new way will be safer, and will need to check making sure
					// multiple array index by element size (registercount per element) for total 
					//  number of registers needed by the array
					ArraySize *= RegisterCount;
				}

				// update the parameter if we see a bigger index
				// if current area (from index to count) does not cover new area
				//if (ExistingRegisterIndex+ExistingRegisterCount < RegisterIndex+RegisterCount)
				if (ArraySize >= ExistingRegisterCount)
				{
					// making sure old value is same as new value
					//check (ArraySize==RegisterIndex+RegisterCount-ExistingRegisterIndex);
					Output.ParameterMap.AddParameterAllocation(*ConstantName, ExistingConstantBufferIndex, ExistingRegisterIndex, ArraySize, ExistingRegisterIndex);
				}
			}
			else
			{
				// output the constants to the parameter map
				Output.ParameterMap.AddParameterAllocation(*ConstantName, 0, RegisterIndex, RegisterCount, RegisterIndex);
			}
		}
	}
	else
	{
		//@todo - Parse errors into file and line as a FShaderCompilerError for Output.Errors
	}

	// Pass the target through to the output.
	Output.Target = Target;
	return bShaderCompileSucceeded;
}

/** Reads the worker outputs and forwards them to FinishCompilingConsoleShader. */
UBOOL ConsoleFinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output)
{
	const BYTE ConsoleShaderCompileWorkerOutputVersion = 0;
	// Read the worker output in the same format that it was written
	BYTE ReadVersion;
	WorkerOutputReadValue(ReadVersion, CurrentPosition, WorkerOutput);
	check(ReadVersion == ConsoleShaderCompileWorkerOutputVersion);
	EWorkerJobType OutJobType;
	WorkerOutputReadValue(OutJobType, CurrentPosition, WorkerOutput);
	check(OutJobType == WJT_PS3Shader || OutJobType == WJT_XenonShader || OutJobType == WJT_WiiUShader);
	BYTE bCompileSucceeded;
	WorkerOutputReadValue(bCompileSucceeded, CurrentPosition, WorkerOutput);
	UINT ByteCodeSize;
	WorkerOutputReadValue(ByteCodeSize, CurrentPosition, WorkerOutput);
	check(!bCompileSucceeded || ByteCodeSize > 0);
	Output.Code.Empty(ByteCodeSize);
	Output.Code.Add(ByteCodeSize);
	if (ByteCodeSize > 0)
	{
		WorkerOutputReadMemory(&Output.Code(0), Output.Code.Num(), CurrentPosition, WorkerOutput);
	}

	UINT ConstantBufferSize;
	WorkerOutputReadValue(ConstantBufferSize, CurrentPosition, WorkerOutput);
	char* ConstantBuffer = new char[ConstantBufferSize + 1];
	FString ConstantTable;
	if (ConstantBufferSize > 0)
	{
		WorkerOutputReadMemory(ConstantBuffer, ConstantBufferSize, CurrentPosition, WorkerOutput);
		ConstantBuffer[ConstantBufferSize] = 0;
		ConstantTable = FString(ANSI_TO_TCHAR(ConstantBuffer));
	}
	delete [] ConstantBuffer;

	UINT ErrorBufferSize;
	WorkerOutputReadValue(ErrorBufferSize, CurrentPosition, WorkerOutput);
	char* ErrorBuffer = new char[ErrorBufferSize + 1];
	if (ErrorBufferSize > 0)
	{
		WorkerOutputReadMemory(ErrorBuffer, ErrorBufferSize, CurrentPosition, WorkerOutput);
		ErrorBuffer[ErrorBufferSize] = 0;
		FShaderCompilerError NewError;
		NewError.StrippedErrorMessage = FString(ANSI_TO_TCHAR(ErrorBuffer));
		Output.Errors.AddItem(NewError);
	}
	delete [] ErrorBuffer;

	return FinishCompilingConsoleShader(bCompileSucceeded != 0, Target, ConstantTable, Output);
}

/** 
 * Compiles a shader for the console platforms. 
 * If this is a multi threaded compile, this function merely enqueues a compilation job.
 */
UBOOL PrecompileConsoleShader(
	INT JobId, 
	UINT ThreadId, 
	const TCHAR* SourceFilename, 
	const TCHAR* FunctionName, 
	const FShaderTarget& Target, 
	const FShaderCompilerEnvironment& Environment, 
	FShaderCompilerOutput& Output)
{
	// build up a string of definitions
	FString Definitions;
	for(TMap<FName,FString>::TConstIterator DefinitionIt(Environment.Definitions);DefinitionIt;++DefinitionIt)
	{
		if (Definitions.Len())
		{
			Definitions += TEXT(" ");
		}

		FString Name = DefinitionIt.Key().ToString();
		FString Definition = DefinitionIt.Value();
		Definitions += FString("-D") + Name + TEXT("=") + Definition;
	}

	// Translate compiler flags
	DWORD CompileFlags = 0;
	for (INT i = 0; i < Environment.CompilerFlags.Num(); i++)
	{
		CompileFlags |= TranslateCompilerFlag(Environment.CompilerFlags(i));
	}

	UBOOL bSucceeded = false;
	FString ConstantTable;
	// If this is a multi threaded compile, we must use the worker, otherwise compile the shader directly.
	if (GShaderCompilingThreadManager->IsMultiThreadedCompile())
	{
		PrecompileConsoleShaderThroughWorker(
			JobId,
			ThreadId, 
			SourceFilename, 
			FunctionName, 
			TCHAR_TO_ANSI(*(FString(appBaseDir()) * appShaderDir())),
			Target, 
			Environment, 
			*Definitions,
			CompileFlags
			);

		// The job has been enqueued, compilation results are not known yet
		bSucceeded = TRUE;
	}
	else
	{
		bSucceeded = PrecompileConsoleShaderThroughDll(
			SourceFilename, 
			FunctionName, 
			Target, 
			Environment, 
			*Definitions,
			CompileFlags, 
			ConstantTable,
			Output
			);

		bSucceeded = FinishCompilingConsoleShader(bSucceeded, Target, ConstantTable, Output);
	}

	return bSucceeded;
}

/**
 * CallPreprocessor - preprocesses, compiles, disassembles shader, outputting files
 *		also creates a command line compilation string and outputs to a file.
 * @param SourceFilename - name of shader source file
 * @param FunctionName - name of entry point function
 * @param Target - shader instruction set target
 * @param Environment - environment/includes for compilation
 * @param Output - output buffer for compiled shader
 * @param ShaderSubDir - directory name to output preprocessed shader files
 * @return bool - success
 */
UBOOL PreprocessConsoleShader(
	const TCHAR* SourceFilename, 
	const TCHAR* FunctionName, 
	const FShaderTarget& Target, 
	const FShaderCompilerEnvironment& Environment, 
	const TCHAR* ShaderSubDir)
{
	// build up path to pass to preprocessors
	const FString EngineShaderDir = FString(appBaseDir()) * FString(appShaderDir());
	FString EngineShaderPath = EngineShaderDir * FString(SourceFilename) + TEXT(".usf");
	const FString ShaderDir = EngineShaderDir * ShaderPlatformToText((EShaderPlatform)Target.Platform) * FString(ShaderSubDir);
	FString ShaderPath = ShaderDir * FString(SourceFilename) + TEXT(".usf");

	// change the location to the user directory if needed (compiling a PS3 shader from a end-user's PC)
	// @todo ship: This requires all shaders to exist in the My Games dir, which is very non-ideal
	if (Target.Platform == SP_PS3)
	{
		ShaderPath = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*ShaderPath));
		EngineShaderPath = GFileManager->ConvertAbsolutePathToUserPath(*GFileManager->ConvertToAbsolutePath(*EngineShaderPath));
	}

	// just in case the shader dir has not been created yet
	GFileManager->MakeDirectory( *ShaderDir, true );

	// copy the source shader over to our subdir
	GFileManager->Copy(*ShaderPath, *EngineShaderPath);

	// build up a string of definitions
	FString Definitions;
	for(TMap<FName,FString>::TConstIterator DefinitionIt(Environment.Definitions);DefinitionIt;++DefinitionIt)
	{
		if (Definitions.Len())
		{
			Definitions += TEXT(" ");
		}

		FString Name = DefinitionIt.Key().ToString();
		FString Definition = DefinitionIt.Value();
		Definitions += FString("-D") + Name + TEXT("=") + Definition;
	}

	char** IncludeFileNames = NULL;
	char** IncludeFileContents = NULL;
	const INT NumIncludes = Environment.AddIncludesForDll(IncludeFileNames, IncludeFileContents);

	// write out the files that may need to be included
	for(INT i = 0; i < NumIncludes; i++)
	{
		// the name already has the .usf extension
		FString IncludePath = EngineShaderDir * ANSI_TO_TCHAR(IncludeFileNames[i]);
		if (appSaveStringToFile(ANSI_TO_TCHAR(IncludeFileContents[i]), *IncludePath, TRUE) == FALSE)
		{
			warnf(TEXT("ShaderPrecompiler: Failed to save to file %s - Is it READONLY?"), *IncludePath);
			return FALSE;
		}
		// copy them to our subdir with shader-specific prefix
		FString CopyToPath = ShaderDir * ANSI_TO_TCHAR(IncludeFileNames[i]);
		GFileManager->Copy(*CopyToPath, *IncludePath);
	}

	DWORD CompileFlags = 0;

	for (INT i = 0; i < Environment.CompilerFlags.Num(); i++)
	{
		CompileFlags |= TranslateCompilerFlag(Environment.CompilerFlags(i));
	}

	const UINT BytecodeBufferAllocatedSize = 256 * 1024; // 256K
	BYTE* BytecodeBuffer = (BYTE*)appMalloc(BytecodeBufferAllocatedSize); 
	appMemzero(BytecodeBuffer, BytecodeBufferAllocatedSize);

	const UINT PreprocessedBufferAllocatedSize = 512 * 1024; // 512K
	unsigned char* PreprocessedBuffer = (unsigned char*)appMalloc(PreprocessedBufferAllocatedSize);
	appMemzero(PreprocessedBuffer, PreprocessedBufferAllocatedSize);

	const UINT AssemblyBufferAllocatedSize = 256 * 1024; // 256K
	unsigned char* AssemblyBuffer = (unsigned char*)appMalloc(AssemblyBufferAllocatedSize);
	appMemzero(AssemblyBuffer, AssemblyBufferAllocatedSize);

	char* ConstantBuffer = (char*)appMalloc(4 * 1024); // 4k
	char* ErrorBuffer = (char*)appMalloc(4 * 1024); // 4k
	char* CmdLineBuffer = (char*)appMalloc(4 * 1024); // 4k

	// to avoid crashing if the DLL doesn't set these
	ConstantBuffer[0] = 0;
	ErrorBuffer[0] = 0;
	CmdLineBuffer[0] = 0;

	INT BytecodeSize = 0;
	INT PreprocessedSize = 0;
	INT AssemblySize = 0;

	UBOOL bSucceeded = FALSE;

	// compiler command line .bat file
	{
		// create a string for command line compilation
		bSucceeded = GConsoleShaderPrecompilers[Target.Platform]->CreateShaderCompileCommandLine(
			TCHAR_TO_ANSI(*ShaderPath), 
			TCHAR_TO_ANSI(*EngineShaderDir), 
			TCHAR_TO_ANSI(FunctionName), 
			Target.Frequency == SF_Vertex, 
			CompileFlags,
			TCHAR_TO_ANSI(*Definitions), 
			CmdLineBuffer,
			FALSE);

		if(bSucceeded)
		{
			// write out the command line string to a file
			const FString ShaderCmdLinePath = ShaderDir * FString(SourceFilename) + TEXT(".bat");
			appSaveStringToFile(ANSI_TO_TCHAR(CmdLineBuffer), *ShaderCmdLinePath );
		}
		else
		{
			warnf(TEXT("CreateShaderCompileCommandLine on %s for platform %s failed!"), SourceFilename, ShaderPlatformToText((EShaderPlatform)Target.Platform));
		}
	}

	{
		// create a string for command line compilation of the preprocessed file
		bSucceeded = GConsoleShaderPrecompilers[Target.Platform]->CreateShaderCompileCommandLine(
			TCHAR_TO_ANSI(*(FString(SourceFilename) + TEXT(".pre"))), 
			TCHAR_TO_ANSI(*EngineShaderDir), 
			TCHAR_TO_ANSI(FunctionName), 
			Target.Frequency == SF_Vertex, 
			CompileFlags,
			TCHAR_TO_ANSI(*Definitions), 
			CmdLineBuffer,
			TRUE);

		if(bSucceeded)
		{
			// write out the command line string to a file
			const FString ShaderCmdLinePath = ShaderDir * FString(SourceFilename) + TEXT("PRE.bat");
			appSaveStringToFile(ANSI_TO_TCHAR(CmdLineBuffer), *ShaderCmdLinePath );
		}
		else
		{
			warnf(TEXT("Preprocessed CreateShaderCompileCommandLine on %s for platform %s failed!"), SourceFilename, ShaderPlatformToText((EShaderPlatform)Target.Platform));
		}
	}

	// preprocessed .pre file
	{
		// preprocess shader
		bSucceeded = GConsoleShaderPrecompilers[Target.Platform]->PreprocessShader(
			TCHAR_TO_ANSI(*EngineShaderPath), 
			TCHAR_TO_ANSI(*Definitions), 
			TCHAR_TO_ANSI(*(EngineShaderDir + PATH_SEPARATOR)),
			IncludeFileNames,
			IncludeFileContents,
			NumIncludes,
			PreprocessedBuffer, 
			PreprocessedSize, 
			ErrorBuffer);

		if (ErrorBuffer[0])
		{
			warnf(ANSI_TO_TCHAR(ErrorBuffer));
			ErrorBuffer[0] = 0;
		}

		if(bSucceeded)
		{
			// write out preprocessed shader text to a file
			const FString PreprocessedShaderPath = ShaderDir * FString(SourceFilename) + TEXT(".pre");
			appSaveStringToFile(ANSI_TO_TCHAR(PreprocessedBuffer), *PreprocessedShaderPath );
		}
		else
		{
			warnf(TEXT("PreprocessShader on %s for platform %s failed!"), SourceFilename, ShaderPlatformToText((EShaderPlatform)Target.Platform));
		}
	}

	// call the DLL precompiler
	bSucceeded = GConsoleShaderPrecompilers[Target.Platform]->PrecompileShader(
		TCHAR_TO_ANSI(*EngineShaderPath), 
		TCHAR_TO_ANSI(FunctionName), 
		Target.Frequency == SF_Vertex, 
		CompileFlags, 
		TCHAR_TO_ANSI(*Definitions), 
		TCHAR_TO_ANSI(*(EngineShaderDir + PATH_SEPARATOR)),
		IncludeFileNames,
		IncludeFileContents,
		NumIncludes,
		GShaderCompilingThreadManager->IsDumpingShaderPDBs() == TRUE,
		TCHAR_TO_ANSI(*FShaderCompilingThreadManager::GetShaderPDBPath()),
		BytecodeBuffer, 
		BytecodeSize, 
		ConstantBuffer, 
		ErrorBuffer
		);

	// output any erorrs
	if (ErrorBuffer[0])
	{
		warnf(ANSI_TO_TCHAR(ErrorBuffer));
	}

	if (bSucceeded)
	{
		// disassembled .asm file
		// disassemble the compiled shader's byte code buffer
		bSucceeded = GConsoleShaderPrecompilers[Target.Platform]->DisassembleShader(
			(DWORD *)BytecodeBuffer, 
			AssemblyBuffer, 
			AssemblySize);

		if(bSucceeded)
		{
			// write out the assembly text to a file
			FString AssemblyShaderPath = ShaderDir * FString(SourceFilename) + TEXT(".asm");
			appSaveStringToFile(ANSI_TO_TCHAR(AssemblyBuffer), *AssemblyShaderPath );
		}
		else
		{
			warnf(TEXT("DisassembleShader on %s for platform %s failed!"), SourceFilename, ShaderPlatformToText((EShaderPlatform)Target.Platform));
		}
	}

	// free buffers
	appFree(ConstantBuffer);
	appFree(BytecodeBuffer);
	appFree(ErrorBuffer);
	appFree(CmdLineBuffer);

	appFree(PreprocessedBuffer);
	appFree(AssemblyBuffer);

	for (INT i = 0; i < NumIncludes; i++)
	{
		// These were allocated with _strdup, which uses malloc
		free(IncludeFileNames[i]);
		free(IncludeFileContents[i]);
	}
	delete [] IncludeFileNames;
	delete [] IncludeFileContents;

	return bSucceeded;
}

#endif // !CONSOLE
