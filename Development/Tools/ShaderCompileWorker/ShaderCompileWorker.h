/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#pragma once

#ifndef _WIN32_WINNT		// Allow use of features specific to Windows XP or later.                   
#define _WIN32_WINNT 0x0501	// Change this to the appropriate value to target other versions of Windows.
#endif						

#include <stdio.h>
#include <tchar.h>
#include <io.h>
#include <assert.h>
#include <string>
#include <vector>
using namespace std;
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <mmsystem.h>

typedef unsigned __int8		BYTE;		// 8-bit  unsigned.
typedef signed __int32 		INT;		// 32-bit signed.

/** Job type - used to know which compiler to use.  Mirrored from ShaderCompiler.cpp */
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

struct FInclude
{
	string IncludeName;
	string IncludeFile;
};

/** File writing helper functions */
extern bool WriteFileW(const wstring& FilePath, const void* Contents, size_t Size, errno_t* CRTErrorCode = NULL);

extern bool WriteFileA(const string& FilePath, const string& Contents, errno_t* CRTErrorCode = NULL);

/** Custom asserts */
extern void Error(const TCHAR* ErrorString, const TCHAR* Filename, UINT LineNum, const TCHAR* FunctionName);
#define ERRORF(x) Error(x, __FILEW__, __LINE__, __FUNCTIONW__)

extern void Check(bool Expression, const TCHAR* ExpressionString, const TCHAR* ErrorString, const TCHAR* Filename, UINT LineNum, const TCHAR* FunctionName);
#define CHECKF(exp,msg) Check(exp,L#exp, msg, __FILEW__, __LINE__, __FUNCTIONW__)
#define VERIFYF(exp,msg) Check(exp,L#exp, msg, __FILEW__, __LINE__, __FUNCTIONW__)

extern void Log(const TCHAR* LogString);

extern void ExitCleanup();

/** Helpers to read from a file */
template<class T>
void ReadValue(FILE* File, T& Value)
{
	size_t NumRead = fread(&Value,sizeof(Value),1,File);
	assert(NumRead == 1);
}
extern void ReadArray(FILE* File, vector<BYTE>& Buffer, UINT Length);

/** Helpers to read from a buffer */
template<class T>
void ReadValue(const vector<BYTE>& Buffer, UINT& Offset, T& Value)
{
	assert((Offset + sizeof(Value)) <= Buffer.size());
	memcpy(&Value, &Buffer.at(Offset), sizeof(Value));
	Offset += sizeof(Value);
}
extern void ParseAnsiString(const vector<BYTE>& Buffer, UINT& Offset, string& String);
/** Note: Caller is responsible for freeing String */
extern void ParseAnsiString(const vector<BYTE>& Buffer, UINT& Offset, LPCSTR& String, UINT& StringLength);
extern void ParseUnicodeString(const vector<BYTE>& Buffer, UINT& Offset, wstring& String);

/** Helpers to write to a buffer */
template<class T>
void WriteValue(std::vector<BYTE>& Buffer, const T& Value)
{
	const BYTE* ValueAsByteStart = reinterpret_cast<const BYTE*>(&Value);
	const BYTE* ValueAsByteEnd = ValueAsByteStart + sizeof(Value);
	Buffer.insert(Buffer.end(), ValueAsByteStart, ValueAsByteEnd);
}
extern void WriteArray(vector<BYTE>& Buffer, const void* Value, UINT Length);

extern wstring GetJobTypeName(EWorkerJobType JobType);

extern bool LoadShaderSourceFile( const string& IncludePath, LPCSTR Name, BYTE** OutAllocatedResult, UINT* OutNumBytes );
