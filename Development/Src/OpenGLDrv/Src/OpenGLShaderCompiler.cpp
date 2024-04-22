/*=============================================================================
	OpenGLShaderCompiler.cpp: OpenGL shader compiler implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

#if !UE3_LEAN_AND_MEAN

#if _WINDOWS

extern UBOOL D3D9CompileShaderThroughDll(
	const TCHAR* SourceFilename,
	const TCHAR* FunctionName,
	const TCHAR* ShaderProfile,
	DWORD CompileFlags,
	const FShaderCompilerEnvironment& Environment,
	FShaderCompilerOutput& Output,
	TArray<D3DXMACRO>& Macros,
	INT AttributeMacroIndex,
	TArray<FD3D9ConstantDesc>& Constants,
	FString& DisassemblyString,
	TArray<FString>& FilteredErrors
	);
extern void D3D9BeginCompileShaderThroughWorker(
	INT JobId,
	UINT ThreadId,
	const TCHAR* SourceFilename,
	LPCSTR FunctionName,
	LPCSTR ShaderProfile,
	LPCSTR IncludePath,
	DWORD CompileFlags,
	const FShaderCompilerEnvironment& Environment,
	TArray<D3DXMACRO>& Macros
	);
extern void D3D9FilterShaderCompileWarnings(const FString& CompileWarnings, TArray<FString>& FilteredWarnings);

extern UBOOL BytecodeVSToGLSLText(const DWORD *Bytecode, FString &OutputShader, UBOOL bHasGlobalConsts, UBOOL bHasBonesConsts);
extern UBOOL BytecodePSToGLSLText(const DWORD *Bytecode, FString &OutputShader, UBOOL bHasGlobalConsts);

/**
 * TranslateCompilerFlag - translates the platform-independent compiler flags into D3DX defines
 * @param CompilerFlag - the platform-independent compiler flag to translate
 * @return DWORD - the value of the appropriate D3DX enum
 */
static DWORD TranslateCompilerFlag(ECompilerFlags CompilerFlag)
{
	switch(CompilerFlag)
	{
	case CFLAG_PreferFlowControl: return D3DXSHADER_PREFER_FLOW_CONTROL;
	case CFLAG_Debug: return D3DXSHADER_DEBUG | D3DXSHADER_SKIPOPTIMIZATION;
	case CFLAG_AvoidFlowControl: return D3DXSHADER_AVOID_FLOW_CONTROL;
	case CFLAG_SkipValidation: return D3DXSHADER_SKIPVALIDATION;
	default: return 0;
	};
}

static DWORD GetBufferIndex(FString Name, UBOOL bIsPixelShader)
{
	if (bIsPixelShader)
	{
		if (Name == TEXT("ScreenPositionScaleBias")
			|| Name == TEXT("MinZ_MaxZRatio")
			|| Name == TEXT("NvStereoEnabled")
			|| Name == TEXT("DiffuseOverrideParameter")
			|| Name == TEXT("SpecularOverrideParameter")
			|| Name == TEXT("CameraPositionPS")
			)
		{
			return GLOBAL_CONSTANT_BUFFER;
		}
	}
	else
	{
		if (Name == TEXT("ViewProjectionMatrix")
			|| Name == TEXT("CameraPositionVS")
			|| Name == TEXT("PreViewTranslation")
			)
		{
			return GLOBAL_CONSTANT_BUFFER;
		}
		else if (Name == TEXT("BoneQuats")
			|| Name == TEXT("BoneScales")
			|| Name == TEXT("BoneMatrices"))
		{
			return VS_BONE_CONSTANT_BUFFER;
		}
	}

	return LOCAL_CONSTANT_BUFFER;
}

/** Processes the results of a D3D9 shader compilation for OpenGL. */
static UBOOL FinishCompilingOpenGLShader(
									   UBOOL bShaderCompileSucceeded,
									   FShaderTarget Target,
									   const TArray<FD3D9ConstantDesc>& Constants,
									   const FString& DisassemblyString,
									   const TArray<FString>& FilteredErrors,
									   FShaderCompilerOutput& Output)
{
	for (INT ErrorIndex = 0; ErrorIndex < FilteredErrors.Num(); ErrorIndex++)
	{
		const FString& CurrentError = FilteredErrors(ErrorIndex);
		FShaderCompilerError NewError;
		// Extract the filename and line number from the shader compiler error message for PC whose format is:
		// "d:\UnrealEngine3\Binaries\memory(210,5): error X3004: undeclared identifier 'LightTransfer'"
		INT FirstParenIndex = CurrentError.InStr(TEXT("("));
		INT LastParenIndex = CurrentError.InStr(TEXT("):"));
		if (FirstParenIndex != INDEX_NONE 
			&& LastParenIndex != INDEX_NONE
			&& LastParenIndex > FirstParenIndex)
		{
			NewError.ErrorFile = CurrentError.Left(FirstParenIndex);
			NewError.ErrorLineString = CurrentError.Mid(FirstParenIndex + 1, LastParenIndex - FirstParenIndex - appStrlen(TEXT("(")));
			NewError.StrippedErrorMessage = CurrentError.Right(CurrentError.Len() - LastParenIndex - appStrlen(TEXT("):")));
		}
		else
		{
			NewError.StrippedErrorMessage = CurrentError;
		}
		Output.Errors.AddItem(NewError);
	}

	UINT NumInstructions = 0;
	if (bShaderCompileSucceeded)
	{
		// Extract the instruction count from the disassembly text.
		if (Parse(
			*DisassemblyString,
			TEXT("// approximately "),
			(DWORD&)NumInstructions
			))
		{
			// Fail compilation if the shader used more than the minimum number of instructions available in Shader Model 3.0 for both pixel and vertex shaders.
			// Instruction count for SM 3.0 is only validated at runtime (CreateVertex/PixelShader), this check catches the incompatibility early,
			// Since some older SM3.0 cards only support 544 instructions (Geforce 7900 GT, others) and we only compile one version for all SM3 targets.
			if (NumInstructions > 512 && Target.Platform == SP_PCD3D_SM3)
			{
				bShaderCompileSucceeded = FALSE;
				FShaderCompilerError NewError;
				NewError.StrippedErrorMessage = FString::Printf(TEXT("Shader uses too many instructions for Shader Model 3.0! (Used %u, Minimum guaranteed in SM3.0 is 512)"), NumInstructions);
				Output.Errors.AddItem(NewError);
			}
		}

		const TCHAR* ConstantIdentifier = TEXT(" c");
		INT ConstantRegisterIndex = DisassemblyString.InStr(ConstantIdentifier);
		INT LargestConstantRegister = 0;
		// Search through the asm text for the largest referenced constant register
		while (ConstantRegisterIndex != INDEX_NONE)
		{
			INT ConstantRegisterEndIndex = ConstantRegisterIndex + appStrlen(ConstantIdentifier);
			// Find the index just after the last digit
			while (ConstantRegisterEndIndex <= DisassemblyString.Len() && appIsDigit(DisassemblyString[ConstantRegisterEndIndex]))
			{
				ConstantRegisterEndIndex++;
			}
			if (ConstantRegisterEndIndex != ConstantRegisterIndex + appStrlen(ConstantIdentifier))
			{
				// Extract the constant number string
				const FString RegisterNumString = DisassemblyString.Mid(ConstantRegisterIndex + appStrlen(ConstantIdentifier), ConstantRegisterEndIndex - (ConstantRegisterIndex + appStrlen(ConstantIdentifier)));
				const INT RegisterIndex = appAtoi(*RegisterNumString);
				// Keep track of the largest register index
				LargestConstantRegister = Max(LargestConstantRegister, RegisterIndex);
			}
			// Find the next register reference
			ConstantRegisterIndex = DisassemblyString.InStr(ConstantIdentifier, FALSE, FALSE, ConstantRegisterIndex + appStrlen(ConstantIdentifier));
		}
		if (Target.Frequency == SF_Vertex && Target.Platform == SP_PCOGL && LargestConstantRegister > 255)
		{
			// Fail compilation if the vertex shader used more than the minimum number of float constant registers available in Shader Model 3.0.
			// Vertex shader constant count for SM 3.0 is only validated at runtime (CreateVertex/PixelShader) based on the value of D3DCAPS9.MaxVertexShaderConst.
			// This check catches the incompatibility early.
			bShaderCompileSucceeded = FALSE;
			FShaderCompilerError NewError;
			NewError.StrippedErrorMessage = FString::Printf(TEXT("Vertex shader uses too many constant registers for Shader Model 3.0! (Used %u, Minimum guaranteed in VS3.0 is 256)"), LargestConstantRegister + 1);
			Output.Errors.AddItem(NewError);
		}
	}

	if (bShaderCompileSucceeded)
	{
		UBOOL bHasGlobalConsts = FALSE;
		UBOOL bHasBonesConsts = FALSE;

		// Map each constant in the table.
		for(INT ConstantIndex = 0;ConstantIndex < Constants.Num();ConstantIndex++)
		{
			// Read the constant description.
			const FD3D9ConstantDesc& Constant = Constants(ConstantIndex);

			if(Constant.bIsSampler)
			{
				Output.ParameterMap.AddParameterAllocation(
					ANSI_TO_TCHAR(Constant.Name),
					0,
					Constant.RegisterIndex,
					Constant.RegisterCount,
					Constant.RegisterIndex
					);
			}
			else
			{
				DWORD BufferIndex = GetBufferIndex(ANSI_TO_TCHAR(Constant.Name), (Target.Frequency == SF_Pixel));
				Output.ParameterMap.AddParameterAllocation(
					ANSI_TO_TCHAR(Constant.Name),
					BufferIndex,
					Constant.RegisterIndex * sizeof(FLOAT) * 4,
					Constant.RegisterCount * sizeof(FLOAT) * 4,
					0
					);

				if (!bHasGlobalConsts || !bHasBonesConsts)
				{
					bHasGlobalConsts = bHasGlobalConsts || (BufferIndex == GLOBAL_CONSTANT_BUFFER);
					bHasBonesConsts = bHasBonesConsts || (BufferIndex == VS_BONE_CONSTANT_BUFFER);
				}
			}
		}

		// If the shader compile succeeded then this should contain the bytecode
		check(Output.Code.Num() > 0);

		FString GLSLShader;

		if (Target.Frequency == SF_Vertex)
		{
			bShaderCompileSucceeded = BytecodeVSToGLSLText((const DWORD *)&Output.Code(0), GLSLShader, bHasGlobalConsts, bHasBonesConsts);
		}
		else
		{
			bShaderCompileSucceeded = BytecodePSToGLSLText((const DWORD *)&Output.Code(0), GLSLShader, bHasGlobalConsts);
		}

		Output.Code.Empty(GLSLShader.Len() + 1);
		Output.Code.Add(GLSLShader.Len() + 1);
		appMemcpy(&Output.Code(0),TCHAR_TO_ANSI(*GLSLShader),GLSLShader.Len() + 1);

		// Pass the target through to the output.
		Output.Target = Target;
		Output.NumInstructions = NumInstructions;
	}

	return bShaderCompileSucceeded;
}

/** Reads the worker outputs and forwards them to FinishCompilingD3D9Shader. */
UBOOL OpenGLFinishCompilingShaderThroughWorker(
	FShaderTarget Target,
	INT& CurrentPosition,
	const TArray<BYTE>& WorkerOutput,
	FShaderCompilerOutput& Output)
{
	const BYTE D3D9ShaderCompileWorkerOutputVersion = 1;
	// Read the worker output in the same format that it was written
	BYTE ReadVersion;
	WorkerOutputReadValue(ReadVersion, CurrentPosition, WorkerOutput);
	check(ReadVersion == D3D9ShaderCompileWorkerOutputVersion);
	EWorkerJobType OutJobType;
	WorkerOutputReadValue(OutJobType, CurrentPosition, WorkerOutput);
	check(OutJobType == WJT_D3D9Shader);
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

	UINT ConstantArrayLength;
	WorkerOutputReadValue(ConstantArrayLength,CurrentPosition,WorkerOutput);

	TArray<FD3D9ConstantDesc> Constants;
	if(ConstantArrayLength > 0)
	{
		const INT NumConstants = ConstantArrayLength / sizeof(FD3D9ConstantDesc);
		Constants.Empty(NumConstants);
		Constants.Add(NumConstants);
		WorkerOutputReadMemory(&Constants(0),ConstantArrayLength,CurrentPosition,WorkerOutput);
	}

	UINT DisassemblyStringLength;
	WorkerOutputReadValue(DisassemblyStringLength, CurrentPosition, WorkerOutput);

	FString DisassemblyString;
	if (DisassemblyStringLength > 0)
	{
		ANSICHAR* DisassemblyStringAnsi = new ANSICHAR[DisassemblyStringLength + 1];
		WorkerOutputReadMemory(DisassemblyStringAnsi, DisassemblyStringLength, CurrentPosition, WorkerOutput);
		DisassemblyStringAnsi[DisassemblyStringLength] = 0;
		DisassemblyString = FString(ANSI_TO_TCHAR(DisassemblyStringAnsi));
		delete [] DisassemblyStringAnsi;
	}	

	UBOOL bShaderCompileSucceeded = TRUE;
	TArray<FString> FilteredErrors;
	if (FAILED(CompileResult))
	{
		// Copy the error text to the output.
		if (ErrorString.Len() > 0)
		{
			D3D9FilterShaderCompileWarnings(ErrorString, FilteredErrors);
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

	return FinishCompilingOpenGLShader(bShaderCompileSucceeded, Target, Constants, DisassemblyString, FilteredErrors, Output);
}

UBOOL OpenGLBeginCompileShader(
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

	TArray<FD3D9ConstantDesc> Constants;
	FString DisassemblyString;

	// Translate the input environment's definitions to D3DXMACROs.
	TArray<D3DXMACRO> Macros;
	for(TMap<FName,FString>::TConstIterator DefinitionIt(Environment.Definitions);DefinitionIt;++DefinitionIt)
	{
		FString Name = DefinitionIt.Key().ToString();
		FString Definition = DefinitionIt.Value();

		D3DXMACRO* Macro = new(Macros) D3DXMACRO;
		Macro->Name = new ANSICHAR[Name.Len() + 1];
		appStrncpyANSI( (ANSICHAR*)Macro->Name, TCHAR_TO_ANSI(*Name), Name.Len()+1 );
		Macro->Definition = new ANSICHAR[Definition.Len() + 1];
		appStrncpyANSI( (ANSICHAR*)Macro->Definition, TCHAR_TO_ANSI(*Definition), Definition.Len()+1 );
	}

	// set the COMPILER type
	D3DXMACRO* Macro = new(Macros) D3DXMACRO;
#define COMPILER_NAME "COMPILER_HLSL"
	Macro->Name = appStrcpyANSI(
		new ANSICHAR[strlen(COMPILER_NAME) + 1], 
		strlen(COMPILER_NAME) + 1,
		COMPILER_NAME
		);
	Macro->Definition = appStrcpyANSI(new ANSICHAR[2], 2, "1");

	DWORD CompileFlags = D3DXSHADER_ENABLE_BACKWARDS_COMPATIBILITY;

	for(INT FlagIndex = 0;FlagIndex < Environment.CompilerFlags.Num();FlagIndex++)
	{
		//accumulate flags set by the shader
		CompileFlags |= TranslateCompilerFlag(Environment.CompilerFlags(FlagIndex));
	}

	UBOOL bShaderCompileSucceeded = FALSE;
	const FString ShaderPath = appShaderDir();
	const FString PreprocessorOutputDir = ShaderPath * ShaderPlatformToText((EShaderPlatform)Target.Platform) * FString(ShaderSubDir);

	TCHAR ShaderProfile[32];
	TArray<FString> FilteredErrors;

	//set defines and profiles for the appropriate shader paths
	check(Target.Platform == SP_PCOGL);
	if (Target.Frequency == SF_Pixel)
	{
		appStrcpy(ShaderProfile, TEXT("ps_3_0"));
	}
	else
	{
		appStrcpy(ShaderProfile, TEXT("vs_3_0"));
	}

	// Set SM3_PROFILE, which indicates that we are compiling for sm3.  
	D3DXMACRO* MacroSM3Profile = new(Macros) D3DXMACRO;
	MacroSM3Profile->Name = appStrcpyANSI(
		new ANSICHAR[strlen("SM3_PROFILE") + 1], 
		strlen("SM3_PROFILE") + 1,
		"SM3_PROFILE"
		);
	MacroSM3Profile->Definition = appStrcpyANSI(new ANSICHAR[2], 2, "1");

	// Set OGL_PROFILE, which indicates that we want all the changes OpenGL requires.
	D3DXMACRO* MacroOGLProfile = new(Macros) D3DXMACRO;
	MacroOGLProfile->Name = appStrcpyANSI(
		new ANSICHAR[strlen("OPENGL") + 1], 
		strlen("OPENGL") + 1,
		"OPENGL"
		);
	MacroOGLProfile->Definition = appStrcpyANSI(new ANSICHAR[2], 2, "1");

	// Add a define that indicates whether the compiler supports attributes such as [unroll]
	INT AttributeMacroIndex = Macros.Num();
	D3DXMACRO* MacroCompilerSupportsAttributes = new(Macros) D3DXMACRO;
	MacroCompilerSupportsAttributes->Name = appStrcpyANSI(
		new ANSICHAR[strlen("COMPILER_SUPPORTS_ATTRIBUTES") + 1], 
		strlen("COMPILER_SUPPORTS_ATTRIBUTES") + 1,
		"COMPILER_SUPPORTS_ATTRIBUTES"
		);
	MacroCompilerSupportsAttributes->Definition = appStrcpyANSI(new ANSICHAR[2], 2, "1");

	// Terminate the Macros list.
	D3DXMACRO* TerminatorMacro = new(Macros) D3DXMACRO;
	TerminatorMacro->Name = NULL;
	TerminatorMacro->Definition = NULL;

	// If this is a multi threaded compile, we must use the worker, otherwise compile the shader directly.
	if (GShaderCompilingThreadManager->IsMultiThreadedCompile())
	{
		D3D9BeginCompileShaderThroughWorker(
			JobId,
			ThreadId,
			SourceFilename,
			TCHAR_TO_ANSI(FunctionName),
			TCHAR_TO_ANSI(ShaderProfile),
			TCHAR_TO_ANSI(*ShaderPath),
			CompileFlags,
			Environment,
			Macros
			);

		// The job has been enqueued, compilation results are not known yet
		bShaderCompileSucceeded = TRUE;
	}
	else
	{
		bShaderCompileSucceeded = D3D9CompileShaderThroughDll(
			SourceFilename,
			FunctionName,
			ShaderProfile,
			CompileFlags,
			Environment,
			Output,
			Macros,
			AttributeMacroIndex,
			Constants,
			DisassemblyString,
			FilteredErrors
			);

		// Process compilation results
		bShaderCompileSucceeded = FinishCompilingOpenGLShader(bShaderCompileSucceeded, Target, Constants, DisassemblyString, FilteredErrors, Output);
	}

	// Free temporary strings allocated for the macros.
	for(INT MacroIndex = 0;MacroIndex < Macros.Num();MacroIndex++)
	{
		delete [] Macros(MacroIndex).Name;
		delete [] Macros(MacroIndex).Definition;
	}

	return bShaderCompileSucceeded;
}

#else // _WINDOWS

UBOOL OpenGLBeginCompileShader(
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
	appErrorf(TEXT("Only Windows can compile shaders for OpenGL."));
	return FALSE;
}

#endif // _WINDOWS

#endif // !UE3_LEAN_AND_MEAN
