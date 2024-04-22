/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

// ShaderCompileWorker.cpp : Defines the entry point for the console application.
//

#include "ShaderCompileWorker.h"

const INT ShaderCompileWorkerInputVersion = 0;
const INT ShaderCompileWorkerOutputVersion = 0;

FILE* LogFile = NULL;

/** Logs a string to the log file and debug output */
void Log(const TCHAR* LogString)
{
	if (LogFile)
	{
		wstring LogStringWithNewline = LogString;
		LogStringWithNewline += TEXT("\n");
		fputws(LogStringWithNewline.c_str(), LogFile);
		fflush(LogFile);
		//Build machine picks this up, so only output when being debugged
		if (IsDebuggerPresent())
		{
			OutputDebugString(LogStringWithNewline.c_str());
		}
	}
}

void ExitCleanup(UINT ExitCode)
{
	Log(TEXT("Exiting"));
	if (LogFile)
	{
		fclose(LogFile);
	}
	ExitProcess(ExitCode);
}

/** Writes the given data to a file, Unicode version */
bool WriteFileW(const wstring& FilePath, const void* Contents, size_t Size, errno_t* CRTErrorCode)
{
	if (CRTErrorCode)
	{
		*CRTErrorCode = 0;
	}
	FILE* OutputFile = NULL;
	const errno_t ErrorCode = _wfopen_s(&OutputFile, FilePath.c_str(), TEXT("wb"));
	if (ErrorCode != 0)
	{
		if (CRTErrorCode)
		{
			*CRTErrorCode = ErrorCode;
		}
		return false;
	}
	const size_t NumWrote = fwrite(Contents, Size, 1, OutputFile);
	fclose(OutputFile);
	if (NumWrote == 1)
	{
		return true;
	}
	return false;
}

/** Writes the given data to a file, Ansi version */
bool WriteFileA(const string& FilePath, const string& Contents, errno_t* CRTErrorCode)
{
	if (CRTErrorCode)
	{
		*CRTErrorCode = 0;
	}
	FILE* OutputFile = NULL;
	const errno_t ErrorCode = fopen_s(&OutputFile, FilePath.c_str(), "wb");
	if (ErrorCode != 0)
	{
		if (CRTErrorCode)
		{
			*CRTErrorCode = ErrorCode;
		}
		return false;
	}
	const size_t NumWrote = fwrite(Contents.c_str(), Contents.size(), 1, OutputFile);
	fclose(OutputFile);
	if (NumWrote == 1)
	{
		return true;
	}
	return false;
}

/** Returns true if the file is present on disk */
bool IsFilePresentW(const wstring& FilePath)
{
	FILE* TestFile = NULL;
	errno_t InputOpenReturn = _wfopen_s(&TestFile, FilePath.c_str(), TEXT("rb"));
	if (InputOpenReturn == 0)
	{
		fclose(TestFile);
	}
	return InputOpenReturn == 0;
}

/** Directory where UE3 expects the output file, used by ERRORF and CHECKF to report errors. */
wstring WorkingDirectory;
wstring OutputFile;

/** Handles a critical error.  In release, the output file that UE3 is expecting is written with the error message. */
void Error(const TCHAR* ErrorString, const TCHAR* Filename, UINT LineNum, const TCHAR* FunctionName)
{
	if (IsDebuggerPresent())
	{
		DebugBreak();
	}
	else
	{
#if _DEBUG
		assert(0);
#else

		TCHAR* FilenameCopy = _wcsdup(Filename);
		const TCHAR* LastPathSeparator = wcsrchr(FilenameCopy, L'\\');
		if (LastPathSeparator == NULL)
		{
			LastPathSeparator = FilenameCopy;
		}
		TCHAR LineNumString[25];
		_itow_s(LineNum, LineNumString, 25, 10);
		wstring ErrorMessage = wstring(TEXT("CriticalError: ")) + wstring(LastPathSeparator + 1) + TEXT(",") + FunctionName + TEXT(":") + LineNumString + TEXT("  ") + ErrorString;
		delete [] FilenameCopy;
		OutputDebugString(ErrorMessage.c_str());
		Log(ErrorMessage.c_str());

		{
			// Write the error message to the output file so that UE3 can assert with the error
			wstring OutputFilePath = WorkingDirectory + OutputFile;
			vector<BYTE> OutputData;
			const BYTE ErrorOutputVersion = 0;
			WriteValue(OutputData, ErrorOutputVersion);
			const EWorkerJobType JobType = WJT_WorkerError;
			WriteValue(OutputData, JobType);
			WriteValue(OutputData, ErrorMessage.size() * sizeof(TCHAR));
			if (ErrorMessage.size() > 0)
			{
				WriteArray(OutputData, ErrorMessage.c_str(), (UINT)(ErrorMessage.size() * sizeof(TCHAR)));
			}
			bool bWroteOutputFile = WriteFileW(OutputFilePath, &OutputData.front(), OutputData.size());
			assert(bWroteOutputFile);
		}
		// Sleep a bit before exiting to make sure that UE3 checks for the output file happen before checks that the worker app is still running
		Sleep(100);
#endif
	}
	ExitCleanup(1);
}

/** Throws a critical error if Expression is false. */
void Check(bool Expression, const TCHAR* ExpressionString, const TCHAR* ErrorString, const TCHAR* Filename, UINT LineNum, const TCHAR* FunctionName)
{
	if (!Expression)
	{
		wstring AssertString = wstring(TEXT("Assertion failed: ")) + ExpressionString + TEXT(" '") + ErrorString + TEXT("'");
		Error(AssertString.c_str(), Filename, LineNum, FunctionName);
	}
}

INT CreateMiniDump( LPEXCEPTION_POINTERS ExceptionInfo )
{
	//@todo - implement
	return EXCEPTION_EXECUTE_HANDLER;
}

void CreateLogFile(const TCHAR* WorkingDirectory, const TCHAR* InputFilename)
{
	// Local job inputs contain the word 'Only', distributed jobs do not.
	// Don't log for distributed jobs since multiple threads operate out of the same directory.
	if (_tcsstr(InputFilename, TEXT("Only")) != NULL)
	{
		const TCHAR* LogFileName = TEXT("\\WorkerLog.txt");
		wstring LogFilePath = wstring(WorkingDirectory) + LogFileName;
		errno_t Return;
		int RetryCount = 0;
		do 
		{
			Return = _wfopen_s(&LogFile, LogFilePath.c_str(), TEXT("w"));
			RetryCount++;
		} 
		while (Return != 0 && RetryCount < 20);

		if (Return == 0)
		{
			Log(TEXT("LogFile opened"));
		}
		else
		{
			LogFile = NULL;
		}
	}
}

/**
 * Reads in an array of bytes from a file.
 */
void ReadArray(FILE* File, vector<BYTE>& Buffer, UINT Length)
{
	Buffer.resize(Length);
	size_t NumRead = fread(&Buffer.at(0),Length,1,File);
	CHECKF(NumRead == 1, TEXT("Read wrong amount"));
}

/** 
 * Reads an Ansi string from the file, and NULL terminates the string.
 * StringLen is the string length WITH the terminator.
 */
void ReadAnsiString(const vector<BYTE>& Buffer, UINT& Offset, LPSTR String, UINT StringLen)
{
	assert((Offset + StringLen - 1) <= Buffer.size());
	memcpy(String, &Buffer.at(Offset), StringLen - 1);
	String[StringLen - 1] = 0;
	Offset += StringLen - 1;
}

/** 
 * Reads a std::string from a file, where the string is stored in the file with length first, then string without NULL terminator.
 */
void ParseAnsiString(const vector<BYTE>& Buffer, UINT& Offset, string& String)
{
	UINT StringLenWithoutNULL;
	ReadValue(Buffer, Offset, StringLenWithoutNULL);
	// Allocate space for the string and the terminator
	LPSTR TempString = new CHAR[StringLenWithoutNULL + 1];
	ReadAnsiString(Buffer, Offset, TempString, StringLenWithoutNULL + 1);
	String = TempString;
	delete [] TempString;
}

/** 
 * Reads an Ansi string from the file, and returns the string and the length including the terminator.
 * Note: Caller is responsible for freeing String 
 */
void ParseAnsiString(const vector<BYTE>& Buffer, UINT& Offset, LPCSTR& String, UINT& StringLength)
{
	UINT StringLenWithoutNULL;
	ReadValue(Buffer, Offset, StringLenWithoutNULL);
	LPSTR NewString = new CHAR[StringLenWithoutNULL + 1];
	ReadAnsiString(Buffer, Offset, NewString, StringLenWithoutNULL + 1);
	String = NewString;
	StringLength = StringLenWithoutNULL + 1;
}

/** 
 * Reads a Unicode string from the file, and NULL terminates the string.
 * StringLen is the string length WITH the terminator.
 */
void ReadUnicodeString(const vector<BYTE>& Buffer, UINT& Offset, TCHAR* String, UINT StringLen)
{
	assert((Offset + (StringLen - 1) * sizeof(TCHAR)) <= Buffer.size());
	memcpy(String, &Buffer.at(Offset), (StringLen - 1) * sizeof(TCHAR));
	String[StringLen - 1] = 0;
	Offset += (StringLen - 1) * sizeof(TCHAR);
}

/** 
 * Reads a std::wstring from a file, where the string is stored in the file with length first, then string without NULL terminator.
 */
void ParseUnicodeString(const vector<BYTE>& Buffer, UINT& Offset, wstring& String)
{
	UINT StringSizeWithoutNULL;
	ReadValue(Buffer, Offset, StringSizeWithoutNULL);
	UINT StringLengthWithoutNULL = StringSizeWithoutNULL / sizeof(TCHAR);
	TCHAR* TempString = new TCHAR[StringLengthWithoutNULL + 1];
	ReadUnicodeString(Buffer, Offset, TempString, StringLengthWithoutNULL + 1);
	String = TempString;
	delete [] TempString;
}

/** 
 * Writes memory into a buffer.
 */
void WriteArray(vector<BYTE>& Buffer, const void* Value, UINT Length)
{
	const BYTE* ValueAsByteStart = reinterpret_cast<const BYTE*>(Value);
	const BYTE* ValueAsByteEnd = ValueAsByteStart + Length;
	Buffer.insert(Buffer.end(), ValueAsByteStart, ValueAsByteEnd);
}

wstring GetJobTypeName(EWorkerJobType JobType)
{
	if (JobType == WJT_D3D9Shader)
	{
		return TEXT("WJT_D3D9Shader");
	}
	else if (JobType == WJT_D3D11Shader)
	{
		return TEXT("WJT_D3D11Shader");
	}
	else if (JobType == WJT_XenonShader)
	{
		return TEXT("WJT_XenonShader");
	}
	else if (JobType == WJT_PS3Shader)
	{
		return TEXT("WJT_PS3Shader");
	}
	else if (JobType == WJT_WiiUShader)
	{
		return TEXT("WJT_WiiUShader");
	}
	else if (JobType == WJT_WorkerError)
	{
		return TEXT("WJT_WorkerError");
	}
	ERRORF(TEXT("Unknown job type"));
	return TEXT("");
}

DWORD LastCompileTime = 0;

/** Called in the idle loop, checks for conditions under which the helper should exit */
void CheckExitConditions(const TCHAR* ParentId, const TCHAR* WorkingDirectory, const TCHAR* InputFilename)
{
	if (_tcsstr(InputFilename, TEXT("Only")) == NULL)
	{
		Log(TEXT("InputFilename did not contain 'Only', exiting after one job."));
		ExitCleanup(0);
	}

	// Don't do these if the debugger is present
	//@todo - don't do these if UE3 is being debugged either
	if (!IsDebuggerPresent())
	{
		// The Win32 parent process Id was passed on the commandline
		int ParentProcessId = _wtoi(ParentId);
		
		if (ParentProcessId > 0)
		{
			wstring InputFilePath = wstring(WorkingDirectory) + InputFilename;

			bool bParentStillRunning = true;
			HANDLE ParentProcessHandle = OpenProcess(SYNCHRONIZE, false, ParentProcessId);
			// If we couldn't open the process then it is no longer running, exit
			if (ParentProcessHandle == NULL)
			{
				CHECKF(!IsFilePresentW(InputFilePath), TEXT("Exiting due to OpenProcess(ParentProcessId) failing and the input file is present!"));
				Log(TEXT("Couldn't OpenProcess, Parent process no longer running, exiting"));
				ExitCleanup(0);
			}
			else
			{
				// If we did open the process, that doesn't mean it is still running
				// The process object stays alive as long as there are handles to it
				// We need to check if the process has signaled, which indicates that it has exited
				DWORD WaitResult = WaitForSingleObject(ParentProcessHandle, 0);
				if (WaitResult != WAIT_TIMEOUT)
				{
					CHECKF(!IsFilePresentW(InputFilePath), TEXT("Exiting due to WaitForSingleObject(ParentProcessHandle) signaling and the input file is present!"));
					Log(TEXT("WaitForSingleObject signaled, Parent process no longer running, exiting"));
					ExitCleanup(0);
				}
				CloseHandle(ParentProcessHandle);
			}
		}

		DWORD CurrentTime = timeGetTime();
		// If we have been idle for 20 seconds then exit
		if (CurrentTime - LastCompileTime > 20000)
		{
			Log(TEXT("No jobs found for 20 seconds, exiting"));
			ExitCleanup(0);
		}
	}
}

// IMPORTANT: Must match ShaderManager.cpp
#define BINARY_SHADER_FILE_VERSION		1
#define BINARY_SHADER_FILE_HEADER_SIZE	24

// IMPORTANT: Must match UnMisc.cpp
#define SBO_KEY_LENGTH (761)
static BYTE SecurityByObscurityKey[SBO_KEY_LENGTH] = 
{
 	0xAD,0x50,0x3B,0x59,0x5A,0x5D,0x33,0x5C,0xEA,0xA6,0x58,0xA7,0xED,0x28,0xF4,0x90,0x9B,0x6F,0x60,0x70,0x50,0x30,0x3E,0x5E,0x0F,0x3A,0xB6,0xC4,0xEE,0x91,0xF7,0x2F,0x6D,0xEE,0x6F,0xAE,0x2D,0x9E,0x6F,0x20,0x94,0x6D,0x7A,0x6B,0x21,0x88,0xD4,0xF8,0xAE,0x74,
 	0xDF,0x08,0x31,0x2A,0xEE,0x9D,0xA3,0x33,0xB7,0x57,0x60,0xB0,0x60,0xD5,0x88,0x4B,0x07,0xC4,0xFB,0x02,0x42,0x84,0x02,0x74,0xA6,0x6A,0x45,0x70,0x65,0x7D,0xA5,0xDB,0x4F,0x8B,0xF3,0xF4,0x95,0x8B,0xFF,0xF8,0x75,0x86,0xB0,0x8F,0xFF,0x4D,0xDC,0xE7,0xC2,0xF1,
 	0xF2,0x53,0xCA,0xDF,0x0B,0x6B,0x80,0x5C,0xF8,0x97,0x6F,0x67,0x33,0x98,0xD5,0xE3,0x06,0x50,0x95,0x53,0xF1,0x16,0x4A,0xAA,0xC7,0x3D,0x94,0x48,0x7F,0xAE,0xAE,0x40,0x98,0x5D,0x22,0x57,0xB7,0xB7,0xE6,0x83,0x21,0x3F,0xD5,0xF3,0xDD,0x8C,0x28,0x01,0xBC,0xFC,
 	0x62,0xDF,0x93,0x14,0x86,0x6C,0xF2,0xC4,0x29,0xEC,0x4C,0x9A,0x24,0x7B,0x2F,0x19,0xAD,0x0C,0x62,0x9B,0x0F,0x36,0xC5,0x0E,0x4C,0xDF,0xA1,0xA8,0x55,0xAB,0x81,0x06,0x07,0x43,0x69,0xD7,0x2D,0x9F,0x7C,0x83,0xD0,0x7B,0xB5,0x92,0xE9,0xB2,0x3E,0x40,0xED,0xB5,
 	0x72,0xB4,0x16,0x4E,0x7C,0x6F,0xD7,0x3F,0x00,0xD1,0x4F,0x65,0x63,0x4A,0x9F,0xB9,0x2B,0x50,0x99,0x5F,0x65,0x66,0xFF,0x1B,0x8B,0x07,0x4A,0x9E,0xEC,0x66,0x4B,0xEB,0xCB,0x25,0x14,0xDD,0xC7,0x79,0xAE,0x5B,0xC8,0xA0,0x85,0x8E,0x1E,0x62,0x17,0xD6,0x19,0x5C,
 	0xC0,0x1B,0x0E,0xD8,0x3A,0xDC,0x57,0xB8,0xC0,0x8E,0xB8,0x97,0x9E,0x21,0x14,0x19,0x9C,0x27,0xC5,0x30,0x54,0x87,0x5B,0xB3,0x3C,0x86,0xB8,0x4F,0x94,0xD1,0xAE,0x16,0x03,0x7B,0xC1,0x34,0x9F,0x91,0xC6,0x3B,0xA4,0x1B,0x1C,0x17,0x3F,0x98,0xBD,0xC8,0x17,0xE9,
 	0x2A,0x3D,0x96,0x54,0x53,0x40,0xBE,0x26,0x2D,0xE2,0x59,0x2A,0x1A,0x80,0xD0,0x5C,0xDE,0xBC,0xBC,0x0C,0x62,0x08,0x5A,0xB5,0xE8,0xC3,0x11,0xF5,0xA3,0xFA,0xE3,0x7E,0xB9,0x80,0x00,0xA1,0xC7,0x26,0x0C,0x1B,0xF7,0xB5,0x13,0xE4,0xDD,0x2A,0x43,0x81,0x23,0xE8,
 	0x85,0x5D,0x53,0xE1,0x30,0xEB,0x61,0x86,0x4F,0xB7,0x72,0x46,0xA6,0x84,0x57,0x63,0x8E,0xAE,0x07,0xD2,0xE6,0x3E,0xA9,0x83,0x3C,0x9E,0x81,0x3B,0x52,0x45,0x6A,0xC4,0x63,0x0A,0x04,0xAB,0xEB,0xD5,0x50,0x13,0x85,0x95,0x64,0x3E,0xFA,0x2D,0x52,0x5F,0x8B,0xB4,
 	0x3B,0x88,0xD3,0xD4,0x5B,0x7B,0x52,0x77,0xD1,0x04,0x7B,0xBD,0x41,0x1B,0xE9,0x8D,0x70,0x22,0x60,0x80,0x47,0xD3,0x92,0x8C,0x55,0xBB,0xFD,0x7C,0x1B,0x29,0x1D,0x6D,0x15,0x43,0x3B,0xE5,0x69,0x7E,0x1E,0x3B,0x80,0x7D,0xEC,0x33,0x28,0x3D,0x5B,0x6F,0xD6,0xF2,
 	0x06,0xC2,0xF3,0x52,0xC2,0x88,0x90,0x25,0x86,0x84,0xCF,0x72,0xCF,0xB7,0xAB,0xEC,0x9E,0xE4,0xD9,0x21,0x70,0x6D,0x53,0xCF,0xDC,0xB9,0xA1,0x38,0x92,0x75,0x94,0xFA,0x08,0x70,0xA6,0xC4,0x85,0x6C,0xCE,0x3D,0xFD,0x7F,0x4C,0x5B,0xD6,0x47,0x53,0xA5,0xF8,0x49,
 	0xE8,0x79,0xF3,0xA8,0xD2,0x45,0xEF,0x1C,0x78,0xA7,0xE2,0x6E,0x26,0x48,0xCF,0x2A,0xBD,0xBE,0x2F,0xDA,0xDE,0xE4,0x5E,0x4F,0x5E,0x8F,0xB2,0x02,0xD7,0xEB,0xF0,0x28,0xDB,0xB9,0x69,0xA2,0xD2,0xA1,0x7A,0x08,0x08,0x48,0xB5,0x36,0x02,0x67,0x31,0x9F,0xCD,0xCB,
 	0xF3,0xE5,0x0B,0xFF,0xAE,0xFB,0xB5,0xC4,0x69,0xB1,0xC8,0xFE,0xE3,0xE6,0x42,0x9F,0xF4,0x86,0xDD,0xDF,0x43,0xC2,0xF7,0x67,0xCB,0x08,0xB1,0xD6,0x59,0x08,0xE1,0xB2,0x28,0xC3,0x93,0xE7,0x5E,0x06,0x5F,0xF7,0x2F,0x29,0xBA,0x3A,0xAD,0x54,0x15,0x03,0x86,0x9F,
 	0xC8,0xC7,0xE3,0x82,0xA7,0xFB,0xFD,0x5D,0xA7,0xE6,0xA1,0xBC,0x3A,0x79,0xFD,0x8B,0x29,0x2D,0x1C,0x46,0x2F,0x22,0x07,0xC3,0xEA,0x39,0xAC,0x5C,0x3D,0xDA,0xE5,0x10,0x6A,0xF0,0x22,0x76,0xC5,0x04,0x0F,0x4A,0x68,0xE9,0x95,0x37,0xE6,0x17,0x0D,0x53,0x25,0x46,
 	0x50,0x4C,0xD9,0x98,0x16,0xC6,0x8A,0xE1,0x34,0x69,0xD8,0xD7,0x73,0xAA,0x9C,0x53,0x64,0xE8,0x77,0x2B,0xD5,0xF8,0x3E,0xAB,0x1C,0x97,0x49,0x97,0x39,0x9A,0xE3,0xF3,0x3E,0x8C,0xEB,0xF2,0x05,0x2C,0x8F,0x7B,0x3E,0xCF,0xD1,0xA1,0xDE,0xA9,0x8C,0x2F,0x6E,0x2F,
 	0x4A,0x26,0xDF,0x31,0xD2,0xAD,0xBB,0xA4,0x25,0x90,0x3D,0xC6,0x16,0x90,0x2C,0xA8,0x67,0xC6,0xC5,0x10,0x87,0x1B,0x20,0xCB,0x04,0x4A,0x6B,0x07,0x75,0xC0,0x68,0x5D,0xE6,0x5B,0x4D,0x98,0xF4,0x43,0x37,0x00,0xC2,0x10,0x0C,0xAD,0x7D,0xEF,0xEF,0xA2,0xA0,0x25,
 	0xD8,0xFB,0xDC,0xA9,0x8D,0x01,0x83,0x52,0x2C,0x5B,0xAC
};

/**
 * Function to encrypt and decrypt an array of bytes using obscurity
 * 
 * @Param InAndOutData data to encrypt or decrypt, and also the result
 */
void SecurityByObscurityEncryptAndDecrypt(BYTE* InAndOutData, UINT Size)
{
	if (Size)
	{
		UINT KeyOffset = 240169 + INT(244109 * Size);
		for (UINT Index = 0; Index < Size; Index++, KeyOffset++)
		{
			KeyOffset = KeyOffset % SBO_KEY_LENGTH;
			InAndOutData[Index] = InAndOutData[Index] ^ SecurityByObscurityKey[KeyOffset]; 
		}
	}
}

void SetFileExtension( string& FilePath, const string& NewExtension )
{
	size_t Pos = FilePath.rfind( '.' );
	if ( Pos >= 0 )
	{
		FilePath = FilePath.replace( Pos, FilePath.size(), ".bin" );
	}
	else
	{
		FilePath += ".bin";
	}
}

bool LoadShaderSourceFile( const string& IncludePath, LPCSTR Name, BYTE** OutAllocatedResult, UINT* OutNumBytes )
{
	string FilePath = IncludePath + "\\Binaries\\" + Name;
	SetFileExtension( FilePath, string(".bin") );

#if _DEBUG
	wstring wFilePath(FilePath.length(), L' ');
	copy( FilePath.begin(), FilePath.end(), wFilePath.begin() );
	Log( wFilePath.c_str() );
#endif

	FILE* IncludeFile = NULL;
	errno_t ErrorCode = fopen_s(&IncludeFile, FilePath.c_str(), "rb");
	if (ErrorCode == 0)
	{
		CHECKF(IncludeFile != NULL, TEXT("Couldn't open include file"));
		int FileId = _fileno(IncludeFile);
		CHECKF(FileId >= 0, TEXT("Couldn't get file id for the include file"));
		long FileLength = _filelength(FileId);
		CHECKF(FileLength >= BINARY_SHADER_FILE_HEADER_SIZE, TEXT("Missing file header"));
		// Skip the header
		FileLength -=  BINARY_SHADER_FILE_HEADER_SIZE;
		fseek(IncludeFile, BINARY_SHADER_FILE_HEADER_SIZE, SEEK_SET);

		// Read the contents.
		BYTE* FileContents = new BYTE[FileLength + 1];
		size_t NumRead = fread(FileContents,sizeof(BYTE),FileLength,IncludeFile);
		fclose(IncludeFile);
		CHECKF(NumRead == FileLength, TEXT("Failed to read the whole include file"));

		// Decrypt
		SecurityByObscurityEncryptAndDecrypt( FileContents, FileLength );
		FileContents[FileLength] = 0;

		*OutAllocatedResult = FileContents;
		*OutNumBytes = (UINT)NumRead;
		return true;
	}
	return false;
}

/** 
 * Main entrypoint, guarded by a try ... except.
 * This expects 4 parameters:
 *		The image path and name
 *		The working directory path, which has to be unique to the instigating process and thread.
 *		The parent process Id
 *		The thread Id corresponding to this worker
 */
int GuardedMain(int argc, _TCHAR* argv[])
{
	assert(argc == 6);

	// After WorkingDirectory is initialized it is safe to use CHECKF and ERRORF
	WorkingDirectory = argv[1];
	OutputFile = argv[5];

	// After CreateLogFile it is safe to use Log
	CreateLogFile(argv[1], argv[4]);

	//@todo - would be nice to change application name or description to have the ThreadId in it for debugging purposes
	SetConsoleTitle(argv[3]);
	// Filename that UE3 will write when it wants a job processed.
	//@todo - use memory mapped files for faster transfer
	// http://msdn.microsoft.com/en-us/library/aa366551(VS.85).aspx
	wstring InputFilePath = wstring(argv[1]) + argv[4];

	LastCompileTime = timeGetTime();

	Log(TEXT("Entering job loop"));
	
	while (true)
	{
		//while (true) {}
		FILE* InputFile = NULL;
		errno_t InputOpenReturn = 1;
		bool FirstOpenTry = true;
		while (InputOpenReturn != 0)
		{
			// Try to open the input file that we are going to process
			InputOpenReturn = _wfopen_s(&InputFile, InputFilePath.c_str(), TEXT("rb"));
			if (InputOpenReturn != 0 && !FirstOpenTry)
			{
				CheckExitConditions(argv[2], argv[1], argv[4]);
				// Give up CPU time while we are waiting
				Sleep(10);
			}
			FirstOpenTry = false;
		}
		CHECKF(InputFile != NULL, TEXT("Input file was not opened successfully"));
		Log(TEXT(""));
		Log(TEXT("Processing shader"));
		LastCompileTime = timeGetTime();

		vector<BYTE> OutputData;

		INT BatchedJobInputVersion;
		ReadValue(InputFile, BatchedJobInputVersion);
		CHECKF(BatchedJobInputVersion == ShaderCompileWorkerInputVersion, TEXT("Mismatching BatchedJobInputVersion"));

		WriteValue(OutputData, ShaderCompileWorkerOutputVersion);

		INT NumBatches;
		ReadValue(InputFile, NumBatches);
		WriteValue(OutputData, NumBatches);

		for (INT BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
		{
			INT JobId;
			ReadValue(InputFile, JobId);
			WriteValue(OutputData, JobId);

			// Read the worker input for this job and decrypt it.
			INT InputLength = 0;
			INT bIsEncrypted = 0;
			vector<BYTE> WorkerInput;
			ReadValue(InputFile, InputLength);
			ReadValue(InputFile, bIsEncrypted);
			ReadArray(InputFile, WorkerInput, InputLength);
			if ( bIsEncrypted )
			{
				SecurityByObscurityEncryptAndDecrypt( &WorkerInput.at(0), (UINT) WorkerInput.size() );
			}

			// Read the job type out of the file
			UINT Offset = 0;
			EWorkerJobType JobType;
			ReadValue(WorkerInput, Offset, JobType);

			// Read the version number
			BYTE InputVersion;
			ReadValue(WorkerInput, Offset, InputVersion);

			Log((wstring(TEXT("Job is ")) + GetJobTypeName(JobType)).c_str());

			// Call the appropriate shader compiler for each job type
			if (JobType == WJT_D3D9Shader)
			{
				extern bool D3D9CompileShader(BYTE InputVersion, const vector<BYTE>& WorkerInput, UINT& Offset, vector<BYTE>& OutputData);
				D3D9CompileShader(InputVersion, WorkerInput, Offset, OutputData);
			}
			else if (JobType == WJT_XenonShader || JobType == WJT_PS3Shader || JobType == WJT_WiiUShader)
			{
				extern bool ConsoleCompileShader(EWorkerJobType JobType, BYTE InputVersion, const vector<BYTE>& WorkerInput, UINT& Offset, const TCHAR* WorkingDirectory, vector<BYTE>& OutputData);
				ConsoleCompileShader(JobType, InputVersion, WorkerInput, Offset, argv[1], OutputData);
			}
			else if (JobType == WJT_D3D11Shader)
			{
				extern bool D3D11CompileShader(BYTE InputVersion, const vector<BYTE>& WorkerInput, UINT& Offset, vector<BYTE>& OutputData);
				D3D11CompileShader(InputVersion, WorkerInput, Offset, OutputData);
			}
			else
			{
				ERRORF(TEXT("Unsupported job type"));
			}
			CHECKF(OutputData.size() > 0, TEXT("OutputData size was invalid"));
		}

		fclose(InputFile);

		// Remove the input file so that it won't get processed more than once
		int RemoveResult = _wremove(InputFilePath.c_str());;
		int DeleteRetryCount = 0;

		while (RemoveResult < 0 && DeleteRetryCount < 20)
		{
			++DeleteRetryCount;
			Sleep(10);
			RemoveResult = _wremove(InputFilePath.c_str());
		}

		if (RemoveResult < 0)
		{
			int ErrorCode = errno;
			if (ErrorCode == 13)
			{
				ERRORF(TEXT("Couldn't delete input file, opened by another process"));
			}
			ERRORF(TEXT("Couldn't delete input file, is it readonly?"));
		}

		Log(TEXT("Writing output"));

		wstring OutputFilePath = wstring(argv[1]) + argv[5];
		// Write the output file that will indicate to UE3 that compilation has finished
		bool bWroteOutputFile = WriteFileW(OutputFilePath, &OutputData.front(), OutputData.size());
		CHECKF(bWroteOutputFile, (wstring(TEXT("Failed to write output file to ") + OutputFilePath).c_str()));
	}
	Log(TEXT("Exiting main loop"));
	return 0;
}

int _tmain(int argc, _TCHAR* argv[])
{
	int ReturnCode = 0;
	if (IsDebuggerPresent())
	{
		ReturnCode = GuardedMain(argc, argv);
	}
	else
	{
		__try
		{
			ReturnCode = GuardedMain(argc, argv);
		}
		__except( CreateMiniDump( GetExceptionInformation() ) )
		{
			//@todo - need to get the callstack and transfer to UE3
			ERRORF(TEXT("Unhandled exception!  UE3ShaderCompileWorker.exe does not yet transfer callstacks back to UE3, debug this error by setting bAllowMultiThreadedShaderCompile=False in BaseEngine.ini.  Exiting"));
		}
	}

	ExitCleanup(ReturnCode);
	return ReturnCode;
}

