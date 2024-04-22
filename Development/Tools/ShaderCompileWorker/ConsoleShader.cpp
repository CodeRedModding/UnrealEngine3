/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "ShaderCompileWorker.h"
#include "..\..\Src\Engine\Inc\UnConsoleTools.h"

FConsoleSupport* ConsoleSupport[WJT_JobTypeMax] = {NULL};
FConsoleShaderPrecompiler* ShaderPrecompiler[WJT_JobTypeMax] = {NULL};

const BYTE ConsoleShaderCompileWorkerInputVersion = 2;
const BYTE ConsoleShaderCompileWorkerOutputVersion = 0;

// Number of elements in an array.
#define ARRAY_COUNT( array ) \
	( sizeof(array) / sizeof((array)[0]) )

/** Loads console support DLL's */
bool LoadConsoleSupport(EWorkerJobType JobType, const wstring& GameName)
{
	if (!ConsoleSupport[JobType])
	{
		HANDLE ConsoleDLL = NULL;

		// Setup path to the console support DLL file.  Currently we expect the DLL to be in a direct
		// subdirectory of the application executable (ShaderCompilerWorker.exe)
		TCHAR PlatformDllPathAndFile[ 2048 ];
		{
			GetModuleFileName( NULL, PlatformDllPathAndFile, ARRAY_COUNT( PlatformDllPathAndFile ) );
			INT StringLength = (INT)_tcslen( PlatformDllPathAndFile );
			if(StringLength > 0)
			{
				--StringLength;
				for(; StringLength > 0; StringLength-- )
				{
					if( PlatformDllPathAndFile[StringLength - 1] == '\\' || PlatformDllPathAndFile[StringLength - 1] == '/' )
					{
						break;
					}
				}
			}
			PlatformDllPathAndFile[StringLength] = 0;
		}

		if (JobType == WJT_XenonShader)
		{
			//@todo - don't hardcode dll name
#if _WIN64
			_tcscat_s( PlatformDllPathAndFile, TEXT("..\\Xbox360\\Xbox360Tools_x64.dll") );
#else
			_tcscat_s( PlatformDllPathAndFile, TEXT("..\\Xbox360\\Xbox360Tools.dll") );
#endif
		}
		else if (JobType == WJT_PS3Shader)
		{
			//@todo - don't hardcode dll name
#if _WIN64
			_tcscat_s( PlatformDllPathAndFile, TEXT("..\\PS3\\PS3Tools_x64.dll") );
#else
			_tcscat_s( PlatformDllPathAndFile, TEXT("..\\PS3\\PS3Tools.dll") );
#endif
		}
		else if (JobType == WJT_WiiUShader)
		{
			//@todo - don't hardcode dll name
#if _WIN64
			_tcscat_s( PlatformDllPathAndFile, TEXT("..\\WiiU\\WiiUTools_x64.dll") );
#else
			_tcscat_s( PlatformDllPathAndFile, TEXT("..\\WiiU\\WiiUTools.dll") );
#endif
		}

		// Some DLLs (such as XeTools) may have load-time references to other DLLs that reside in
		// same directory, so we need to make sure Windows knows to look in that folder for dependencies.
		// By specifying LOAD_WITH_ALTERED_SEARCH_PATH, LoadLibrary will look in the target DLL's
		// directory for the necessary dependent DLL files.
		ConsoleDLL = LoadLibraryEx( PlatformDllPathAndFile, NULL, LOAD_WITH_ALTERED_SEARCH_PATH );

		if (ConsoleDLL)
		{
			// look for the main entry point function that returns a pointer to the ConsoleSupport subclass
			FuncGetConsoleSupport SupportProc = (FuncGetConsoleSupport)GetProcAddress((HMODULE)ConsoleDLL, "GetConsoleSupport");
			ConsoleSupport[JobType] = SupportProc ? SupportProc(ConsoleDLL) : NULL;
			if (ConsoleSupport[JobType])
			{
#ifdef _DEBUG
				ConsoleSupport[JobType]->Initialize(GameName.c_str(), TEXT("Debug"));
#else
				ConsoleSupport[JobType]->Initialize(GameName.c_str(), TEXT("Release"));
#endif

				ShaderPrecompiler[JobType] = ConsoleSupport[JobType]->GetGlobalShaderPrecompiler();
				CHECKF(ShaderPrecompiler[JobType] != NULL, TEXT("Failed to create shader precompiler"));
				return true;
			}
			else
			{
				FreeLibrary((HMODULE)ConsoleDLL);
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	return true;
}

bool ConsoleCompileShader(EWorkerJobType JobType, BYTE InputVersion, const vector<BYTE>& WorkerInput, UINT& Offset, const TCHAR* WorkingDirectory, vector<BYTE>& OutputData)
{
	CHECKF(JobType == WJT_XenonShader || JobType == WJT_PS3Shader || JobType == WJT_WiiUShader, TEXT("Unsupported job type"));
	CHECKF(InputVersion == ConsoleShaderCompileWorkerInputVersion, TEXT("Wrong version for a console shader"));

	wstring GameName;
	ParseUnicodeString(WorkerInput, Offset, GameName);

	if (!LoadConsoleSupport(JobType, GameName))
	{
		ERRORF((wstring(TEXT("Failed to load console support dll for ")) + GetJobTypeName(JobType)).c_str());
	}
	CHECKF(ShaderPrecompiler[JobType] != NULL, TEXT("Console precompiler was not initialized"));

	string SourceFileName;
	ParseAnsiString(WorkerInput, Offset, SourceFileName);

	string FunctionName;
	ParseAnsiString(WorkerInput, Offset, FunctionName);

	BYTE bIsVertexShader;
	ReadValue(WorkerInput, Offset, bIsVertexShader);

	DWORD CompileFlags;
	ReadValue(WorkerInput, Offset, CompileFlags);

	string IncludePath;
	ParseAnsiString(WorkerInput, Offset, IncludePath);

	string SourceFilePath = IncludePath + "\\" + SourceFileName + ".usf";

	UINT NumIncludes;
	ReadValue(WorkerInput, Offset, NumIncludes);

	LPSTR WorkingDirectoryAnsi = NULL;
	if (JobType == WJT_PS3Shader)
	{
		size_t NumCharsConverted;
		const size_t WorkingDirectoryLen = wcslen(WorkingDirectory); 
		WorkingDirectoryAnsi = new CHAR[WorkingDirectoryLen + 1];
		// Convert working directory from Unicode to Ansi so it can be used by the precompiler
		errno_t ErrorCode = wcstombs_s(&NumCharsConverted, WorkingDirectoryAnsi, WorkingDirectoryLen + 1, WorkingDirectory, WorkingDirectoryLen + 1);
		CHECKF(ErrorCode == 0, TEXT("Failed to convert working directory to ansi"));
		SourceFilePath = string(WorkingDirectoryAnsi) + "\\" + SourceFileName + ".usf";
	}
	
	char** IncludeFileNames = new char*[NumIncludes];
	char** IncludeFileContents = new char*[NumIncludes];

	for (UINT IncludeIndex = 0; IncludeIndex < NumIncludes; IncludeIndex++)
	{
		FInclude NewInclude;
		ParseAnsiString(WorkerInput, Offset, NewInclude.IncludeName);
		ParseAnsiString(WorkerInput, Offset, NewInclude.IncludeFile);

		IncludeFileNames[IncludeIndex] = _strdup(NewInclude.IncludeName.c_str());
		IncludeFileContents[IncludeIndex] = _strdup(NewInclude.IncludeFile.c_str());

		if (JobType == WJT_PS3Shader)
		{
			errno_t CRTWriteError = 0;
			string IncludeFilePath = string(WorkingDirectoryAnsi) + "\\" + NewInclude.IncludeName;
			bool bWroteFile = WriteFileA(IncludeFilePath, NewInclude.IncludeFile, &CRTWriteError);
			// Wait and retry a number of times as there is some interference causing these writes to fail on build machines occasionally
			for (INT RetryCount = 0; !bWroteFile && RetryCount < 20; RetryCount++)
			{
				Sleep(100);
				bWroteFile = WriteFileA(IncludeFilePath, NewInclude.IncludeFile, &CRTWriteError);
			}
			if (!bWroteFile)
			{
				TCHAR CRTErrorMessage[80];
				_wcserror_s(CRTErrorMessage, 80, CRTWriteError);
				TCHAR CRTErrorCodeString[25];
				_itow_s(CRTWriteError, CRTErrorCodeString, 25, 10);
				const DWORD LastError = GetLastError();
				TCHAR LastErrorString[25];
				_itow_s(LastError, LastErrorString, 25, 10);
				wstring ErrorString = wstring(TEXT("Failed to write out include file for console shader, GetLastError=")) + LastErrorString + TEXT(" CRTError=") + CRTErrorMessage + TEXT(" CRTErrorCode=") + CRTErrorCodeString;
				ERRORF(ErrorString.c_str());
			}
		}
	}

	string Definitions;
	ParseAnsiString(WorkerInput, Offset, Definitions);

	BYTE bIsDumpingShaderPDBs;
	ReadValue(WorkerInput, Offset, bIsDumpingShaderPDBs);

	string ShaderPDBPath;
	ParseAnsiString(WorkerInput, Offset, ShaderPDBPath);

	// allocate a huge buffer for constants and bytecode
	// @GEMINI_TODO: Validate this in the dll or something by passing in the size
	const UINT BytecodeBufferAllocatedSize = 1024 * 1024; // 1M
	BYTE* BytecodeBuffer = new BYTE[BytecodeBufferAllocatedSize]; 
	// Zero the bytecode in case the dll doesn't write to all of it,
	// since the engine caches shaders with the same bytecode.
	ZeroMemory(BytecodeBuffer, BytecodeBufferAllocatedSize);
	char* ErrorBuffer = new char[256 * 1024]; // 256k
	char* ConstantBuffer = new char[256 * 1024]; // 256k
	// to avoid crashing if the DLL doesn't set these
	ConstantBuffer[0] = 0;
	ErrorBuffer[0] = 0;
	INT BytecodeSize = 0;

	// call the DLL precompiler
	bool bSucceeded = ShaderPrecompiler[JobType]->PrecompileShader(
		SourceFilePath.c_str(), 
		FunctionName.c_str(),
		bIsVertexShader == 0 ? false : true, 
		CompileFlags, 
		Definitions.c_str(), 
		(IncludePath + "\\").c_str(),
		IncludeFileNames,
		IncludeFileContents,
		NumIncludes,
		bIsDumpingShaderPDBs == 0 ? false : true,
		ShaderPDBPath.c_str(),
		BytecodeBuffer, 
		BytecodeSize, 
		ConstantBuffer, 
		ErrorBuffer);

	WriteValue(OutputData, ConsoleShaderCompileWorkerOutputVersion);
	WriteValue(OutputData, JobType);
	BYTE bSuccededByte = bSucceeded;
	WriteValue(OutputData, bSuccededByte);
	WriteValue(OutputData, BytecodeSize);
	if (BytecodeSize > 0)
	{
		WriteArray(OutputData, BytecodeBuffer, BytecodeSize);
	}
	UINT ConstantBufferLength = (UINT)strlen(ConstantBuffer);
	WriteValue(OutputData, ConstantBufferLength);
	if (ConstantBufferLength > 0)
	{
		WriteArray(OutputData, ConstantBuffer, ConstantBufferLength);
	}
	UINT ErrorBufferLength = (UINT)strlen(ErrorBuffer);
	WriteValue(OutputData, ErrorBufferLength);
	if (ErrorBufferLength > 0)
	{
		WriteArray(OutputData, ErrorBuffer, ErrorBufferLength);
	}

	delete [] WorkingDirectoryAnsi;
	delete [] BytecodeBuffer;
	delete [] ErrorBuffer;
	delete [] ConstantBuffer;

	for (UINT i = 0; i < NumIncludes; i++)
	{
		// These were allocated with _strdup, which uses malloc
		free(IncludeFileNames[i]);
		free(IncludeFileContents[i]);
	}
	delete [] IncludeFileNames;
	delete [] IncludeFileContents;

	return true;
}
