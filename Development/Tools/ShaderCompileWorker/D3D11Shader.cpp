/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "ShaderCompileWorker.h"
#include <d3d11.h>
#include <d3dx11.h>
#include <d3d11Shader.h>
#include "D3Dcompiler.h"

const BYTE D3D11ShaderCompileWorkerInputVersion = 0;
const BYTE D3D11ShaderCompileWorkerOutputVersion = 0;
const UINT REQUIRED_D3DX11_SDK_VERSION = 43;

/**
 * An implementation of the D3DX include interface to access a FShaderCompilerEnvironment.
 */
class FD3D11IncludeEnvironment : public ID3DInclude
{
public:

	vector<FInclude> Includes;

	STDMETHOD(Open)(D3D_INCLUDE_TYPE Type,LPCSTR Name,LPCVOID ParentData,LPCVOID* Data,UINT* Bytes)
	{
		bool bFoundInclude = false;
		for (UINT IncludeIndex = 0; IncludeIndex < Includes.size(); IncludeIndex++)
		{
			if (Includes[IncludeIndex].IncludeName == Name)
			{
				bFoundInclude = true;
				const UINT DataLength = (UINT)Includes[IncludeIndex].IncludeFile.size();
				CHAR* OutData = new CHAR[DataLength];
				memcpy(OutData, Includes[IncludeIndex].IncludeFile.c_str(), DataLength);
				*Data = OutData;
				*Bytes = DataLength;
				break;
			}
		}

		if (!bFoundInclude)
		{
			BYTE* FileContents = NULL;
			UINT FileSize = 0;
			bFoundInclude = LoadShaderSourceFile( IncludePath, Name, &FileContents, &FileSize );
			if ( bFoundInclude )
			{
				*Data = FileContents;
				*Bytes = FileSize;
			}
		}

		return bFoundInclude ? S_OK : S_FALSE;
	}

	STDMETHOD(Close)(LPCVOID Data)
	{
		delete [] Data;
		return S_OK;
	}

	FD3D11IncludeEnvironment(const string& InIncludePath) :
		IncludePath(InIncludePath)
	{}

private:

	string IncludePath;
};

/**
 * This wraps D3DX11CompileFromMemory in a __try __except block to catch occasional crashes in the function.
 */
static HRESULT D3D11SafeCompileShader(
	LPCSTR SourceFileName,
	LPCSTR pSrcData,
	UINT srcDataLen,
	const D3D_SHADER_MACRO* Defines,
	ID3DInclude* pInclude,
	LPCSTR pFunctionName,
	LPCSTR pProfile,
	DWORD Flags,
	ID3DBlob** ppShader,
	ID3DBlob** ppErrorMsgs
	)
{
	__try
	{
		return D3DX11CompileFromMemory(
			pSrcData,
			srcDataLen,
			SourceFileName,
			Defines,
			pInclude,
			pFunctionName,
			pProfile,
			Flags,
			0,
			NULL,
			ppShader,
			ppErrorMsgs,
			NULL
			);
	}
	__except(EXCEPTION_EXECUTE_HANDLER)
	{
		ERRORF(TEXT("D3DXCompileShader threw an exception"));
		return E_FAIL;
	}
}

bool D3D11CompileShader(BYTE InputVersion, const vector<BYTE>& WorkerInput, UINT& Offset, vector<BYTE>& OutputData)
{
	CHECKF(InputVersion == D3D11ShaderCompileWorkerInputVersion, TEXT("Wrong job version for D3D11 shader"));
	CHECKF(D3DX11_SDK_VERSION == REQUIRED_D3DX11_SDK_VERSION, TEXT("Compiled with wrong DX SDK, June 2010 DX SDK required"));

	string SourceFileName;
	ParseAnsiString(WorkerInput, Offset, SourceFileName);

	string SourceFile;
	ParseAnsiString(WorkerInput, Offset, SourceFile);

	string FunctionName;
	ParseAnsiString(WorkerInput, Offset, FunctionName);

	string ShaderProfile;
	ParseAnsiString(WorkerInput, Offset, ShaderProfile);

	DWORD CompileFlags;
	ReadValue(WorkerInput, Offset, CompileFlags);

	string IncludePath;
	ParseAnsiString(WorkerInput, Offset, IncludePath);
	FD3D11IncludeEnvironment IncludeEnvironment(IncludePath);

	UINT NumIncludes;
	ReadValue(WorkerInput, Offset, NumIncludes);

	for (UINT IncludeIndex = 0; IncludeIndex < NumIncludes; IncludeIndex++)
	{
		FInclude NewInclude;
		ParseAnsiString(WorkerInput, Offset, NewInclude.IncludeName);
		ParseAnsiString(WorkerInput, Offset, NewInclude.IncludeFile);
		IncludeEnvironment.Includes.push_back(NewInclude);
	}

	UINT NumMacros;
	ReadValue(WorkerInput, Offset, NumMacros);

	vector<D3D_SHADER_MACRO> Macros;
	for (UINT MacroIndex = 0; MacroIndex < NumMacros; MacroIndex++)
	{
		D3D_SHADER_MACRO NewMacro;
		UINT NameLen;
		ParseAnsiString(WorkerInput, Offset, NewMacro.Name, NameLen);
		UINT DefinitionLen;
		ParseAnsiString(WorkerInput, Offset, NewMacro.Definition, DefinitionLen);
		Macros.push_back(NewMacro);
	}

	D3D_SHADER_MACRO TerminatorMacro;
	TerminatorMacro.Name = NULL;
	TerminatorMacro.Definition = NULL;
	Macros.push_back(TerminatorMacro);

	ID3DBlob* ShaderByteCode = NULL;
	ID3DBlob* Errors = NULL;

	HRESULT hr = D3D11SafeCompileShader(
		SourceFileName.c_str(),
		SourceFile.c_str(),
		(UINT)SourceFile.size(),
		&Macros.front(),
		&IncludeEnvironment,
		FunctionName.c_str(),
		ShaderProfile.c_str(),
		CompileFlags,
		&ShaderByteCode,
		&Errors
		);

	for (UINT MacroIndex = 0; MacroIndex < Macros.size(); MacroIndex++)
	{
		delete [] Macros[MacroIndex].Name;
		delete [] Macros[MacroIndex].Definition;
	}

	EWorkerJobType JobType = WJT_D3D11Shader;
	UINT ByteCodeLength = SUCCEEDED(hr) ? (UINT)ShaderByteCode->GetBufferSize() : 0;
	UINT ErrorStringLength = Errors == NULL ? 0 : (UINT)Errors->GetBufferSize();

	WriteValue(OutputData, D3D11ShaderCompileWorkerOutputVersion);
	WriteValue(OutputData, JobType);
	WriteValue(OutputData, hr);
	WriteValue(OutputData, ByteCodeLength);
	if (ByteCodeLength > 0)
	{
		WriteArray(OutputData, ShaderByteCode->GetBufferPointer(), ByteCodeLength);
	}
	WriteValue(OutputData, ErrorStringLength);
	if (ErrorStringLength > 0)
	{
		WriteArray(OutputData, Errors->GetBufferPointer(), ErrorStringLength);
	}

	if (ShaderByteCode != NULL)
	{
		ShaderByteCode->Release();
	}

	if (Errors != NULL)
	{
		Errors->Release();
	}

	return true;
}
