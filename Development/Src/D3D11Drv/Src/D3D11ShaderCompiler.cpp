/*=============================================================================
	D3D11ShaderCompiler.cpp: D3D shader compiler implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

#if !UE3_LEAN_AND_MEAN

#if _WIN64
#pragma pack(push,16)
#else
#pragma pack(push,8)
#endif
#define D3D_OVERLOADS 1
#include "D3Dcompiler.h"
#include <d3d11Shader.h>
#pragma pack(pop)

/**
 * An implementation of the D3DX include interface to access a FShaderCompilerEnvironment.
 */
class FD3D11IncludeEnvironment : public ID3DInclude
{
public:

	STDMETHOD(Open)(D3D_INCLUDE_TYPE Type,LPCSTR Name,LPCVOID ParentData,LPCVOID* Data,UINT* Bytes)
	{
		FString Filename(ANSI_TO_TCHAR(Name));

		if (appStrcmp(*Filename, TEXT("Material.usf")) == 0)
		{
			check(Environment.MaterialShaderCode);
			const INT Length = strlen(Environment.MaterialShaderCode) + 1;
			ANSICHAR* AnsiFileContents = new ANSICHAR[Length];
			appStrncpyANSI(AnsiFileContents, Environment.MaterialShaderCode, Length);
			*Data = (LPCVOID)AnsiFileContents;
			check(Length > 1);
			*Bytes = Length - 1;
		}
		else
		{
			FString FileContents;

			FString* OverrideContents = Environment.IncludeFiles.Find(*Filename);
			if(OverrideContents)
			{
				FileContents = *OverrideContents;
			}
			else if (appStrcmp(*Filename, TEXT("VertexFactory.usf")) == 0)
			{
				check(Environment.VFFileName);
				FileContents = LoadShaderSourceFile(Environment.VFFileName);
			}
			else
			{
				FileContents = LoadShaderSourceFile(*Filename);
			}

			// Convert the file contents to ANSI.
			FTCHARToANSI ConvertToAnsiFileContents(*FileContents);
			ANSICHAR* AnsiFileContents = new ANSICHAR[ConvertToAnsiFileContents.Length() + 1];
			appStrncpyANSI( AnsiFileContents, (ANSICHAR*)ConvertToAnsiFileContents, ConvertToAnsiFileContents.Length() + 1 );

			// Write the result to the output parameters.
			*Data = (LPCVOID)AnsiFileContents;
			*Bytes = ConvertToAnsiFileContents.Length();
		}

		return S_OK;
	}

	STDMETHOD(Close)(LPCVOID Data)
	{
		delete [] Data;
		return S_OK;
	}

	FD3D11IncludeEnvironment(const FShaderCompilerEnvironment& InEnvironment):
		Environment(InEnvironment)
	{}

private:

	FShaderCompilerEnvironment Environment;
};

/**
 * TranslateCompilerFlag - translates the platform-independent compiler flags into D3DX defines
 * @param CompilerFlag - the platform-independent compiler flag to translate
 * @return DWORD - the value of the appropriate D3DX enum
 */
static DWORD TranslateCompilerFlagD3D11(ECompilerFlags CompilerFlag)
{
	// @TODO - currently d3d11 uses d3d10 shader compiler flags... update when this changes in DXSDK
	switch(CompilerFlag)
	{
	case CFLAG_PreferFlowControl: return D3D10_SHADER_PREFER_FLOW_CONTROL;
	case CFLAG_Debug: return D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;
	case CFLAG_AvoidFlowControl: return D3D10_SHADER_AVOID_FLOW_CONTROL;
	default: return 0;
	};
}

/**
 * D3D11CreateShaderCompileCommandLine - takes shader parameters used to compile with the DX10
 * compiler and returns an fxc command to compile from the command line
 */
static FString D3D11CreateShaderCompileCommandLine(
	const FString& ShaderPath, 
	const FString& IncludePath, 
	const TCHAR* EntryFunction, 
	const TCHAR* ShaderProfile, 
	D3D_SHADER_MACRO *Macros,
	DWORD CompileFlags,
	UBOOL bPreprocessedCommandLine
	)
{
	// fxc is our command line compiler
	FString FXCCommandline = FString(TEXT("\"%DXSDK_DIR%\\Utilities\\bin\\x86\\fxc\" ")) + ShaderPath;

	if (!bPreprocessedCommandLine)
	{
		// add definitions
		if(Macros != NULL)
		{
			for (INT i = 0; Macros[i].Name != NULL; i++)
			{
				FXCCommandline += FString(TEXT(" /D ")) + ANSI_TO_TCHAR(Macros[i].Name) + TEXT("=") + ANSI_TO_TCHAR(Macros[i].Definition);
			}
		}
	}

	// add the entry point reference
	FXCCommandline += FString(TEXT(" /E ")) + EntryFunction;

	if (!bPreprocessedCommandLine)
	{
		// add the include path
		FXCCommandline += FString(TEXT(" /I ")) + IncludePath;
	}

	// @TODO - currently d3d11 uses d3d10 shader compiler flags... update when this changes in DXSDK
	// go through and add other switches
	if(CompileFlags & D3D10_SHADER_PREFER_FLOW_CONTROL)
	{
		CompileFlags &= ~D3D10_SHADER_PREFER_FLOW_CONTROL;
		FXCCommandline += FString(TEXT(" /Gfp"));
	}
	if(CompileFlags & D3D10_SHADER_DEBUG)
	{
		CompileFlags &= ~D3D10_SHADER_DEBUG;
		FXCCommandline += FString(TEXT(" /Zi"));
	}
	if(CompileFlags & D3D10_SHADER_SKIP_OPTIMIZATION)
	{
		CompileFlags &= ~D3D10_SHADER_SKIP_OPTIMIZATION;
		FXCCommandline += FString(TEXT(" /Od"));
	}
	if(CompileFlags & D3D10_SHADER_AVOID_FLOW_CONTROL)
	{
		CompileFlags &= ~D3D10_SHADER_AVOID_FLOW_CONTROL;
		FXCCommandline += FString(TEXT(" /Gfa"));
	}
	if(CompileFlags & D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY)
	{
		CompileFlags &= ~D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;
		FXCCommandline += FString(TEXT(" /Gec"));
	}
	if(CompileFlags & D3D10_SHADER_OPTIMIZATION_LEVEL3)
	{
		CompileFlags &= ~D3D10_SHADER_OPTIMIZATION_LEVEL3;
		FXCCommandline += FString(TEXT(" /O3"));
	}
	checkf(CompileFlags == 0, TEXT("Unhandled d3d10 shader compiler flag!"));

	// add the target instruction set
	FXCCommandline += FString(TEXT(" /T ")) + ShaderProfile;

	// add a pause on a newline
	FXCCommandline += FString(TEXT(" \r\n pause"));
	return FXCCommandline;
}

/**
 * Uses D3D11 to PreProcess a shader (resolve all #includes and #defines) and dumps it out for debugging
 */
static void D3D11PreProcessShader(
	const TCHAR* SourceFilename,
	const FString& SourceFile,
	const TArray<D3D_SHADER_MACRO>& Macros,
	FD3D11IncludeEnvironment& IncludeEnvironment,
	const TCHAR* ShaderPath
	)
{
	TRefCountPtr<ID3DBlob> ShaderText;
	TRefCountPtr<ID3DBlob> PreProcessErrors;

	FTCHARToANSI AnsiSourceFile(*SourceFile);
	HRESULT PreProcessHR = D3DPreprocess(
		(ANSICHAR*)AnsiSourceFile,
		AnsiSourceFile.Length(),
		NULL,
		&Macros(0),
		&IncludeEnvironment,
		ShaderText.GetInitReference(),
		PreProcessErrors.GetInitReference());

	if(FAILED(PreProcessHR))
	{
		warnf( NAME_Warning, TEXT("Preprocess failed for shader %s: %s"), SourceFilename, ANSI_TO_TCHAR(PreProcessErrors->GetBufferPointer()) );
	}
	else
	{
		appSaveStringToFile(
			FString(ANSI_TO_TCHAR(ShaderText->GetBufferPointer())), 
			*(FString(ShaderPath) * FString(SourceFilename) + TEXT(".pre")));
	}
}

/**
 * Filters out unwanted shader compile warnings
 */
static void D3D11FilterShaderCompileWarnings(const FString& CompileWarnings, TArray<FString>& FilteredWarnings)
{
	TArray<FString> WarningArray;
	FString OutWarningString = TEXT("");
	CompileWarnings.ParseIntoArray(&WarningArray, TEXT("\n"), TRUE);
	
	//go through each warning line
	for (INT WarningIndex = 0; WarningIndex < WarningArray.Num(); WarningIndex++)
	{
		//suppress "warning X3557: Loop only executes for 1 iteration(s), forcing loop to unroll"
		if (WarningArray(WarningIndex).InStr(TEXT("X3557")) == INDEX_NONE
			// "warning X3205: conversion from larger type to smaller, possible loss of data"
			// Gets spammed when converting from float to half
			&& WarningArray(WarningIndex).InStr(TEXT("X3205")) == INDEX_NONE)
		{
			FilteredWarnings.AddUniqueItem(WarningArray(WarningIndex));
		}
	}
}

/** Compiles a D3D11 shader through the D3DX Dll */
static UBOOL D3D11CompileShaderThroughDll(
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	const TCHAR* ShaderProfile,
	DWORD CompileFlags,
	const FShaderCompilerEnvironment& Environment,
	FShaderCompilerOutput& Output,
	TArray<D3D_SHADER_MACRO>& Macros,
	TArray<FString>& FilteredErrors
	)
{
	TRefCountPtr<ID3DBlob> Shader;
	TRefCountPtr<ID3DBlob> Errors;

	const FString SourceFile = LoadShaderSourceFile(SourceFilename);
	FD3D11IncludeEnvironment IncludeEnvironment(Environment);

	FTCHARToANSI AnsiSourceFile(*SourceFile);
	HRESULT Result = D3DCompile(
		(ANSICHAR*)AnsiSourceFile,
		AnsiSourceFile.Length(),
		TCHAR_TO_ANSI(SourceFilename),
		&Macros(0),
		&IncludeEnvironment,
		TCHAR_TO_ANSI(FunctionName),
		TCHAR_TO_ANSI(ShaderProfile),
		CompileFlags,
		0,
		Shader.GetInitReference(),
		Errors.GetInitReference()
		);

	if (FAILED(Result))
	{
		// Copy the error text to the output.
		void* ErrorBuffer = Errors ? Errors->GetBufferPointer() : NULL;
		if (ErrorBuffer)
		{
			D3D11FilterShaderCompileWarnings(ANSI_TO_TCHAR(ErrorBuffer), FilteredErrors);
		}
		else
		{
			FilteredErrors.AddItem(TEXT("Compile Failed without warnings!"));
		}
		return FALSE;
	}
	else
	{
		UINT NumShaderBytes = Shader->GetBufferSize();
		Output.Code.Empty(NumShaderBytes);
		Output.Code.Add(NumShaderBytes);
		appMemcpy(&Output.Code(0),Shader->GetBufferPointer(),NumShaderBytes);

		return TRUE;
	}
}

/** Enqueues compilation of a D3D11 shader through a worker process */
static void D3D11BeginCompilingShaderThroughWorker(
	INT JobId,
	UINT ThreadId,
	const TCHAR* SourceFilename,
	LPCSTR FunctionName,
	LPCSTR ShaderProfile,
	LPCSTR IncludePath,
	DWORD CompileFlags,
	const FShaderCompilerEnvironment& Environment,
	TArray<D3D_SHADER_MACRO>& Macros
	)
{
	EWorkerJobType JobType = WJT_D3D11Shader;
	TRefCountPtr<FBatchedShaderCompileJob> BatchedJob = new FBatchedShaderCompileJob(JobId, ThreadId, JobType);
	TArray<BYTE>& WorkerInput = BatchedJob->WorkerInput;
	// Presize to avoid lots of allocations
	WorkerInput.Empty(1000);
	// Setup the input for the worker app, everything that is needed to compile the shader.
	// Note that any format changes here also need to be done in the worker
	// Write a job type so the worker app can know which compiler to invoke
	WorkerInputAppendValue(JobType, WorkerInput);
	// Version number so we can detect stale data
	const BYTE D3D11ShaderCompileWorkerInputVersion = 0;
	WorkerInputAppendValue(D3D11ShaderCompileWorkerInputVersion, WorkerInput);
	WorkerInputAppendMemory(TCHAR_TO_ANSI(SourceFilename), appStrlen(SourceFilename), WorkerInput);
	const FString SourceFile = LoadShaderSourceFile(SourceFilename);
	FTCHARToANSI AnsiSourceFile(*SourceFile);
	WorkerInputAppendMemory((ANSICHAR*)AnsiSourceFile, AnsiSourceFile.Length(), WorkerInput);
	WorkerInputAppendMemory(FunctionName, appStrlen(FunctionName) * sizeof(CHAR), WorkerInput);
	WorkerInputAppendMemory(ShaderProfile, appStrlen(ShaderProfile) * sizeof(CHAR), WorkerInput);
	WorkerInputAppendValue(CompileFlags, WorkerInput);
	WorkerInputAppendMemory(IncludePath, appStrlen(IncludePath) * sizeof(CHAR), WorkerInput);

	Environment.AddIncludesForWorker(WorkerInput);

	INT NumMacros = Macros.Num() - 1;
	WorkerInputAppendValue(NumMacros, WorkerInput);

	for (INT MacroIndex = 0; MacroIndex < Macros.Num() - 1; MacroIndex++)
	{
		D3D_SHADER_MACRO CurrentMacro = Macros(MacroIndex);
		check( CurrentMacro.Name);
		WorkerInputAppendMemory(CurrentMacro.Name, appStrlen(CurrentMacro.Name), WorkerInput);
		WorkerInputAppendMemory(CurrentMacro.Definition, appStrlen(CurrentMacro.Definition), WorkerInput);
	}

	TArray<BYTE> WorkerOutput;
	// Invoke the worker
	GShaderCompilingThreadManager->BeginWorkerCompile(BatchedJob);
}

/** Processes the results of a D3D11 shader compilation. */
static UBOOL FinishCompilingD3D11Shader(
	UBOOL bShaderCompileSucceeded,
	FShaderTarget Target,
	const TArray<FString>& FilteredErrors,
	FShaderCompilerOutput& Output)
{
	for (INT ErrorIndex = 0; ErrorIndex < FilteredErrors.Num(); ErrorIndex++)
	{
		const FString& CurrentError = FilteredErrors(ErrorIndex);
		FShaderCompilerError NewError;
		// Extract the filename and line number from the shader compiler error message for PC whose format is:
		// "d:\UnrealEngine3\Binaries\BasePassPixelShader(30,7): error X3000: invalid target or usage string"
		INT FirstParenIndex = CurrentError.InStr(TEXT("("));
		INT LastParenIndex = CurrentError.InStr(TEXT("):"));
		if (FirstParenIndex != INDEX_NONE 
			&& LastParenIndex != INDEX_NONE
			&& LastParenIndex > FirstParenIndex)
		{
			FFilename ErrorFileAndPath = CurrentError.Left(FirstParenIndex);
			if (ErrorFileAndPath.GetExtension().ToUpper() == TEXT("USF"))
			{
				NewError.ErrorFile = ErrorFileAndPath.GetCleanFilename();
			}
			else
			{
				NewError.ErrorFile = ErrorFileAndPath.GetCleanFilename() + TEXT(".usf");
			}

			NewError.ErrorLineString = CurrentError.Mid(FirstParenIndex + 1, LastParenIndex - FirstParenIndex - appStrlen(TEXT("(")));
			NewError.StrippedErrorMessage = CurrentError.Right(CurrentError.Len() - LastParenIndex - appStrlen(TEXT("):")));
		}
		else
		{
			NewError.StrippedErrorMessage = CurrentError;
		}
		Output.Errors.AddItem(NewError);
	}

	if (bShaderCompileSucceeded)
	{
		ID3D11ShaderReflection* Reflector = NULL;
		VERIFYD3D11RESULT(D3DReflect(Output.Code.GetData(),Output.Code.Num(),IID_ID3D11ShaderReflection, (void**) &Reflector) );

		// Read the constant table description.
		D3D11_SHADER_DESC ShaderDesc;
		Reflector->GetDesc(&ShaderDesc);

		// Add parameters for shader resources (constant buffers, textures, samplers, etc. */
		for (UINT ResourceIndex = 0; ResourceIndex < ShaderDesc.BoundResources; ResourceIndex++)
		{
			D3D11_SHADER_INPUT_BIND_DESC BindDesc;
			Reflector->GetResourceBindingDesc(ResourceIndex, &BindDesc);
			if (BindDesc.Type == D3D10_SIT_CBUFFER || BindDesc.Type == D3D10_SIT_TBUFFER)
			{
				const UINT CBIndex = BindDesc.BindPoint;
				ID3D11ShaderReflectionConstantBuffer* ConstantBuffer = Reflector->GetConstantBufferByName(BindDesc.Name);
				D3D11_SHADER_BUFFER_DESC CBDesc;
				ConstantBuffer->GetDesc(&CBDesc);

				if (CBDesc.Size > GConstantBufferSizes[CBIndex])
				{
					appErrorf(TEXT("Set GConstantBufferSizes[%d] to >= %d"), CBIndex, CBDesc.Size);
				}

				// Track all of the variables in this constant buffer.
				for (UINT ConstantIndex = 0; ConstantIndex < CBDesc.Variables; ConstantIndex++)
				{
					ID3D11ShaderReflectionVariable* Variable = ConstantBuffer->GetVariableByIndex(ConstantIndex);
					D3D11_SHADER_VARIABLE_DESC VariableDesc;
					Variable->GetDesc(&VariableDesc);
					if (VariableDesc.uFlags & D3D10_SVF_USED)
					{
						Output.ParameterMap.AddParameterAllocation(
							ANSI_TO_TCHAR(VariableDesc.Name),
							CBIndex,
							VariableDesc.StartOffset,
							VariableDesc.Size,
							0
							);
					}
				}
			}
			else if (BindDesc.Type == D3D10_SIT_TEXTURE)
			{
				UBOOL bHasMatchingSampler = FALSE;
				// Find the sampler that goes with this texture
				for (UINT R2 = 0; R2 < ShaderDesc.BoundResources; R2++)
				{
					D3D11_SHADER_INPUT_BIND_DESC TextureDesc;
					Reflector->GetResourceBindingDesc(R2, &TextureDesc);
					if (TextureDesc.Type == D3D10_SIT_SAMPLER)
					{
						if (strcmp(TextureDesc.Name, BindDesc.Name) == 0)
						{
							bHasMatchingSampler = TRUE;
							break;
						}
					}
				}

				// Add a parameter for a texture without a matching sampler (SamplerState in HLSL)
				// If the texture has a matching sampler (sampler2D in HLSL), it will be handled in the D3D10_SIT_SAMPLER branch
				if (!bHasMatchingSampler)
				{
					TCHAR OfficialName[1024];
					UINT BindCount = 1;
					appStrcpy(OfficialName, ANSI_TO_TCHAR(BindDesc.Name));

					// Assign the name and optionally strip any "[#]" suffixes
					TCHAR *BracketLocation = appStrchr(OfficialName, TEXT('['));
					if (BracketLocation)
					{
						*BracketLocation = 0;	

						const INT NumCharactersBeforeArray = BracketLocation - OfficialName;

						// In SM5, for some reason, array suffixes are included in Name, i.e. "LightMapTextures[0]", rather than "LightMapTextures"
						// Additionally elements in an array are listed as SEPERATE bound resources.
						// However, they are always contiguous in resource index, so iterate over the samplers and textures of the initial association
						// and count them, identifying the bindpoint and bindcounts

						while (ResourceIndex + 1 < ShaderDesc.BoundResources)
						{
							D3D11_SHADER_INPUT_BIND_DESC BindDesc2;
							Reflector->GetResourceBindingDesc(ResourceIndex + 1, &BindDesc2);

							if (BindDesc2.Type == D3D10_SIT_TEXTURE && strncmp(BindDesc2.Name, BindDesc.Name, NumCharactersBeforeArray) == 0)
							{
								BindCount++;
								// Skip over this resource since it is part of an array
								ResourceIndex++;
							}
							else
							{
								break;
							}
						}
					}

					// Add a parameter for the texture only, the sampler index will be invalid
					Output.ParameterMap.AddParameterAllocation(
						OfficialName,
						0,
						BindDesc.BindPoint,
						BindCount,
						USHRT_MAX
						);
				}
			}
			else if (BindDesc.Type == D3D11_SIT_UAV_RWTYPED)
			{
				TCHAR OfficialName[1024];
				UINT BindCount = 1;
				appStrcpy(OfficialName, ANSI_TO_TCHAR(BindDesc.Name));
	
				// todo: arrays are not yet supported

				Output.ParameterMap.AddParameterAllocation(
					OfficialName,
					0,
					BindDesc.BindPoint,
					BindCount,
					USHRT_MAX
					);
			}
			else if (BindDesc.Type == D3D10_SIT_SAMPLER)
			{
				UBOOL bHasMatchingTexture = FALSE;
				// Find the texture that goes with this sampler
				for (UINT R2 = 0; R2 < ShaderDesc.BoundResources; R2++)
				{
					D3D11_SHADER_INPUT_BIND_DESC TextureDesc;
					Reflector->GetResourceBindingDesc(R2, &TextureDesc);
					if (TextureDesc.Type == D3D10_SIT_TEXTURE)
					{
						if (strcmp(TextureDesc.Name, BindDesc.Name) == 0)
						{
							bHasMatchingTexture = TRUE;

							TCHAR OfficialName[1024];
							UINT SamplerBindPoint = BindDesc.BindPoint;
							UINT TextureBindPoint = TextureDesc.BindPoint;
							UINT OfficialBindCount = 0;

							// In SM5, for some reason, array suffixes are included in Name, i.e. "LightMapTextures[0]", rather than "LightMapTextures"
							// Additionally elements in an array are listed as SEPERATE bound resources.
							// However, they are always contiguous in resource index, so iterate over the samplers and textures of the initial association
							// and count them, identifying the bindpoint and bindcounts

							appStrcpy(OfficialName, ANSI_TO_TCHAR(BindDesc.Name));
							// Assign the name and optionally strip any "[#]" suffixes
							
							TCHAR *BracketLocation = appStrchr(OfficialName, TEXT('['));
							if (BracketLocation)
							{
								*BracketLocation = 0;	
							}

							// Start with the current location.  All sampler/texture pairs should get one loop through this iteration(even non arrays)
							UINT ResourceOffset = 0;
							while (R2 + ResourceOffset < ShaderDesc.BoundResources && ResourceIndex + ResourceOffset < ShaderDesc.BoundResources)
							{
								D3D11_SHADER_INPUT_BIND_DESC TextureTestDesc;
								D3D11_SHADER_INPUT_BIND_DESC SamplerTestDesc;
								Reflector->GetResourceBindingDesc(R2 + ResourceOffset, &TextureTestDesc);
								Reflector->GetResourceBindingDesc(ResourceIndex + ResourceOffset, &SamplerTestDesc);

								TCHAR TestTexName[1024];
								TCHAR TestSamName[1024];

								appStrcpy(TestTexName, ANSI_TO_TCHAR(TextureTestDesc.Name));
								appStrcpy(TestSamName, ANSI_TO_TCHAR(SamplerTestDesc.Name));

								BracketLocation = appStrchr(TestSamName,TEXT('['));
								if (BracketLocation) 
								{
									*BracketLocation = 0;	
								}

								BracketLocation = appStrchr(TestTexName,TEXT('['));
								if (BracketLocation) 
								{
									*BracketLocation = 0;
								}

								// First verify that we're still mapping proper samplers and textures for continuation of an array
								if (SamplerTestDesc.Type != D3D10_SIT_SAMPLER ||			// still sample and texture?
									TextureTestDesc.Type != D3D10_SIT_TEXTURE ||
									appStrcmp(TestSamName, TestTexName) != 0 ||	// still match?
									appStrcmp(TestSamName, OfficialName) != 0)	// still the SAME sampler?
								{
									break;
								}

								// Establish the lowest bind point (should always be the first resource, right?!?)
								// Establish the BindCount
								TextureBindPoint = Min<UINT>(TextureBindPoint, TextureTestDesc.BindPoint);
								SamplerBindPoint = Min<UINT>(SamplerBindPoint, SamplerTestDesc.BindPoint);
								OfficialBindCount++;

								// try next 
								ResourceOffset++;
							}

							if (OfficialBindCount <= 0)
							{
								appErrorf(TEXT("Unable to establish Binding for D3D11 Sampler/Texture - %s / $s "), ANSI_TO_TCHAR(BindDesc.Name), ANSI_TO_TCHAR(TextureDesc.Name));
							}

							// manually skip past the other samplers in this array as they are covered with BindCount
							ResourceIndex += OfficialBindCount - 1;	

							Output.ParameterMap.AddParameterAllocation(
								OfficialName,
								0,
								TextureBindPoint,
								OfficialBindCount,
								SamplerBindPoint
								);
							break;
						}
					}
				}

				if (!bHasMatchingTexture)
				{
					// Add a parameter for the sampler only, the texture index will be invalid
					Output.ParameterMap.AddParameterAllocation(
						ANSI_TO_TCHAR(BindDesc.Name),
						0,
						USHRT_MAX,
						BindDesc.BindCount,
						BindDesc.BindPoint
						);
				}
			}
		}

		// Set the number of instructions.
		Output.NumInstructions = ShaderDesc.InstructionCount;

		// Reflector is a com interface, so it needs to be released.
		Reflector->Release();

		// Pass the target through to the output.
		Output.Target = Target;
	}
	return bShaderCompileSucceeded;
}

/** Reads the worker outputs and forwards them to FinishCompilingD3D11Shader. */
UBOOL D3D11FinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output)
{
	const BYTE D3D11ShaderCompileWorkerOutputVersion = 0;
	// Read the worker output in the same format that it was written
	BYTE ReadVersion;
	WorkerOutputReadValue(ReadVersion, CurrentPosition, WorkerOutput);
	check(ReadVersion == D3D11ShaderCompileWorkerOutputVersion);
	EWorkerJobType OutJobType;
	WorkerOutputReadValue(OutJobType, CurrentPosition, WorkerOutput);
	check(OutJobType == WJT_D3D11Shader);
	HRESULT CompileResult;
	WorkerOutputReadValue(CompileResult, CurrentPosition, WorkerOutput);
	UINT ByteCodeLength;
	WorkerOutputReadValue(ByteCodeLength, CurrentPosition, WorkerOutput);
	Output.Code.Empty(ByteCodeLength);
	Output.Code.Add(ByteCodeLength);
	if (ByteCodeLength > 0)
	{
		WorkerOutputReadMemory(&Output.Code(0), Output.Code.Num(), CurrentPosition, WorkerOutput);
	}
	UINT ErrorStringLength;
	WorkerOutputReadValue(ErrorStringLength, CurrentPosition, WorkerOutput);
	
	FString ErrorString;
	if (ErrorStringLength > 0)
	{
		ANSICHAR* ErrorBuffer = new ANSICHAR[ErrorStringLength + 1];
		WorkerOutputReadMemory(ErrorBuffer, ErrorStringLength, CurrentPosition, WorkerOutput);
		ErrorBuffer[ErrorStringLength] = 0;
		ErrorString = FString(ANSI_TO_TCHAR(ErrorBuffer));
		delete [] ErrorBuffer;
	}	

	UBOOL bShaderCompileSucceeded = TRUE;
	TArray<FString> FilteredErrors;
	if (FAILED(CompileResult))
	{
		// Copy the error text to the output.
		if (ErrorString.Len() > 0)
		{
			D3D11FilterShaderCompileWarnings(ErrorString, FilteredErrors);
		}
		else
		{
			FilteredErrors.AddItem(TEXT("Compile Failed without warnings!"));
		}

		bShaderCompileSucceeded = FALSE;
	}
	else
	{
		bShaderCompileSucceeded = TRUE;
	}
	return FinishCompilingD3D11Shader(bShaderCompileSucceeded, Target, FilteredErrors, Output);
}

/**
 * The D3D11/HLSL shader compiler.
 * If this is a multi threaded compile, this function merely enqueues a compilation job.
 */ 
UBOOL D3D11BeginCompileShader(
	INT JobId, 
	UINT ThreadId,
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	FShaderTarget Target,
	const FShaderCompilerEnvironment& Environment,
	FShaderCompilerOutput& Output,
	UBOOL bDebugDump = FALSE,
	const TCHAR* ShaderSubDir = NULL
	)
{
	// ShaderSubDir must be valid if we are dumping debug shader data
	checkSlow(!bDebugDump || ShaderSubDir != NULL);
	// Must not be doing a multithreaded compile if we are dumping debug shader data
	checkSlow(!bDebugDump || !GShaderCompilingThreadManager->IsMultiThreadedCompile());

	// Translate the input environment's definitions to D3DXMACROs.
	TArray<D3D_SHADER_MACRO> Macros;
	for(TMap<FName,FString>::TConstIterator DefinitionIt(Environment.Definitions);DefinitionIt;++DefinitionIt)
	{
		FString Name = DefinitionIt.Key().ToString();
		FString Definition = DefinitionIt.Value();

		D3D_SHADER_MACRO* Macro = new(Macros) D3D_SHADER_MACRO;
		ANSICHAR* tName = new ANSICHAR[Name.Len() + 1];
		strncpy_s(tName,Name.Len() + 1,TCHAR_TO_ANSI(*Name),Name.Len() + 1);
		Macro->Name = tName;
		ANSICHAR* tDefinition = new ANSICHAR[Definition.Len() + 1];
		strncpy_s(tDefinition,Definition.Len() + 1,TCHAR_TO_ANSI(*Definition),Definition.Len() + 1);
		Macro->Definition = tDefinition;
	}

	// set the COMPILER type
	D3D_SHADER_MACRO* Macro = new(Macros) D3D_SHADER_MACRO;
#define COMPILER_NAME "COMPILER_HLSL"
	ANSICHAR* tName1 = new ANSICHAR[strlen(COMPILER_NAME) + 1];
	strcpy_s(tName1, strlen(COMPILER_NAME) + 1, COMPILER_NAME);
	Macro->Name = tName1;

	ANSICHAR* tDefinition1 = new ANSICHAR[2];
	strcpy_s(tDefinition1, 2, "1");
	Macro->Definition = tDefinition1;

	// set the SM5_PROFILE definition
	static const char* ProfileName = "SM5_PROFILE";
	D3D_SHADER_MACRO* ProfileMacro = new(Macros) D3D_SHADER_MACRO;
	ProfileMacro->Name = appStrcpyANSI(new ANSICHAR[strlen(ProfileName) + 1],strlen(ProfileName) + 1,ProfileName);
	ProfileMacro->Definition = appStrcpyANSI(new ANSICHAR[2],2,"1");

	// set SUPPORTS_DEPTH_TEXTURES
	D3D_SHADER_MACRO* MacroDepthSupport = new(Macros) D3D_SHADER_MACRO;
	ANSICHAR* tName2 = new ANSICHAR[strlen("SUPPORTS_DEPTH_TEXTURES") + 1];
	strcpy_s(tName2, strlen("SUPPORTS_DEPTH_TEXTURES") + 1, "SUPPORTS_DEPTH_TEXTURES");
	MacroDepthSupport->Name = tName2;

	ANSICHAR* tDefinition2 = new ANSICHAR[2];
	strcpy_s(tDefinition2, 2, "1");
	MacroDepthSupport->Definition = tDefinition2;

	// @TODO - currently d3d11 uses d3d10 shader compiler flags... update when this changes in DXSDK
	DWORD CompileFlags = 0;

	// @TODO - implement different material path to allow us to remove backwards compat flag on sm5 shaders
	CompileFlags = D3D10_SHADER_ENABLE_BACKWARDS_COMPATIBILITY;

	if (DEBUG_SHADERS) 
	{
		//add the debug flags
		CompileFlags |= D3D10_SHADER_DEBUG | D3D10_SHADER_SKIP_OPTIMIZATION;
	}
	else
	{
		CompileFlags |= D3D10_SHADER_OPTIMIZATION_LEVEL3;
	}

	for(INT FlagIndex = 0;FlagIndex < Environment.CompilerFlags.Num();FlagIndex++)
	{
		//accumulate flags set by the shader
		CompileFlags |= TranslateCompilerFlagD3D11(Environment.CompilerFlags(FlagIndex));
	}

	UBOOL bShaderCompileSucceeded = FALSE;
	const TCHAR* ShaderPath = appShaderDir();
	const FString PreprocessorOutputDir = FString(ShaderPath) * ShaderPlatformToText((EShaderPlatform)Target.Platform) * FString(ShaderSubDir);

	TCHAR ShaderProfile[32];
	TArray<FString> FilteredErrors;
	{
		checkSlow(Target.Frequency == SF_Vertex ||
			Target.Frequency == SF_Pixel ||
			Target.Frequency == SF_Hull ||
			Target.Frequency == SF_Domain ||
			Target.Frequency == SF_Compute ||
			Target.Frequency == SF_Geometry);

		//set defines and profiles for the appropriate shader paths
		if (Target.Frequency == SF_Pixel)
		{
			appStrcpy(ShaderProfile, TEXT("ps_5_0"));
		}
		else if(Target.Frequency == SF_Vertex)
		{
			appStrcpy(ShaderProfile, TEXT("vs_5_0"));
		}
		else if(Target.Frequency == SF_Hull)
		{
			appStrcpy(ShaderProfile, TEXT("hs_5_0"));
		}
		else if(Target.Frequency == SF_Domain)
		{
			appStrcpy(ShaderProfile, TEXT("ds_5_0"));
		}
		else if(Target.Frequency == SF_Geometry)
		{
			appStrcpy(ShaderProfile, TEXT("gs_5_0"));
		}
		else if(Target.Frequency == SF_Compute)
		{
			appStrcpy(ShaderProfile, TEXT("cs_5_0"));
		}
		else
		{
			return FALSE;
		}

		// Terminate the Macros list.
		D3D_SHADER_MACRO* TerminatorMacro = new(Macros) D3D_SHADER_MACRO;
		TerminatorMacro->Name = NULL;
		TerminatorMacro->Definition = NULL;

		// If this is a multi threaded compile, we must use the worker, otherwise compile the shader directly.
		if (GShaderCompilingThreadManager->IsMultiThreadedCompile())
		{
			D3D11BeginCompilingShaderThroughWorker(
				JobId,
				ThreadId,
				SourceFilename,
				TCHAR_TO_ANSI(FunctionName),
				TCHAR_TO_ANSI(ShaderProfile),
				TCHAR_TO_ANSI(ShaderPath),
				CompileFlags,
				Environment,
				Macros
				);

			// The job has been enqueued, compilation results are not known yet
			bShaderCompileSucceeded = TRUE;
		}
		else
		{
			bShaderCompileSucceeded = D3D11CompileShaderThroughDll(
				SourceFilename,
				FunctionName,
				ShaderProfile,
				CompileFlags,
				Environment,
				Output,
				Macros,
				FilteredErrors
				);

			bShaderCompileSucceeded = FinishCompilingD3D11Shader(bShaderCompileSucceeded, Target, FilteredErrors, Output);
		}
	}

	// If we are dumping out preprocessor data
	// @todo - also dump out shader data when compilation fails
	if (bDebugDump)
	{
		// just in case the preprocessed shader dir has not been created yet
		GFileManager->MakeDirectory( *PreprocessorOutputDir, true );

		// save out include files from the environment definitions
		// Note: Material.usf and VertexFactory.usf vary between materials/vertex factories
		// this is handled because fxc will search for the includes in the same directory as the main shader before searching the include path 
		// otherwise it would find a stale Material.usf and VertexFactory.usf left behind by other platforms
		for(TMap<FString,FString>::TConstIterator IncludeIt(Environment.IncludeFiles); IncludeIt; ++IncludeIt)
		{
			FString IncludePath = PreprocessorOutputDir * IncludeIt.Key();
			appSaveStringToFile(IncludeIt.Value(), *IncludePath);
		}

		const FString SaveFileName = PreprocessorOutputDir * SourceFilename;
		const FString SourceFile = LoadShaderSourceFile(SourceFilename);
		appSaveStringToFile(SourceFile, *(SaveFileName + TEXT(".usf")));

		//allow dumping the preprocessed shader
		FD3D11IncludeEnvironment IncludeEnvironment(Environment);
		D3D11PreProcessShader(SourceFilename, SourceFile, Macros, IncludeEnvironment, *PreprocessorOutputDir);

		const FString AbsoluteShaderPath = FString(appBaseDir()) * PreprocessorOutputDir;
		const FString AbsoluteIncludePath = FString(appBaseDir()) * ShaderPath;
		// get the fxc command line
		FString FXCCommandline = D3D11CreateShaderCompileCommandLine(
			AbsoluteShaderPath * SourceFilename + TEXT(".usf"), 
			AbsoluteIncludePath, 
			FunctionName, 
			ShaderProfile,
			&Macros(0),
			CompileFlags,
			FALSE);

		appSaveStringToFile(FXCCommandline, *(SaveFileName + TEXT(".bat")));

		FString PreprocessedFXCCommandline = D3D11CreateShaderCompileCommandLine(
			FString(SourceFilename) + TEXT(".pre"), 
			AbsoluteIncludePath, 
			FunctionName, 
			ShaderProfile,
			&Macros(0),
			CompileFlags,
			TRUE);

		appSaveStringToFile(PreprocessedFXCCommandline, *(SaveFileName + TEXT("PRE.bat")));

		if (bDebugDump && !bShaderCompileSucceeded)
		{
			warnf( NAME_DevShaders, TEXT( "ASM not supported for DX10 as of March 2009 DirectX." ) );
		}
	}

	// Free temporary strings allocated for the macros.
	for(INT MacroIndex = 0;MacroIndex < Macros.Num();MacroIndex++)
	{
		delete [] Macros(MacroIndex).Name;
		delete [] Macros(MacroIndex).Definition;
	}

	return bShaderCompileSucceeded;
}

#endif