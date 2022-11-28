/*=============================================================================
	D3DShader.cpp: Unreal Direct3D shader implementation.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "D3DDrv.h"

//
//	FD3DShaderParameter::FD3DShaderParameter
//

FD3DShaderParameter::FD3DShaderParameter(FD3DShader* Shader,const TCHAR* InParameterName):
	VertexRegisterIndex(INDEX_NONE),
	PixelRegisterIndex(INDEX_NONE),
	SamplerIndex(INDEX_NONE),
	NumVertexRegisters(0),
	NumPixelRegisters(0),
	NumSamplers(0),
	ParameterName(InParameterName)
{
	// Add this register to the shader's linked list of registers.

	NextParameter = Shader->FirstParameter;
	Shader->FirstParameter = this;
}

//
//	FD3DShaderParameter::SetVectors
//

void FD3DShaderParameter::SetVectors(const FLOAT* Vectors,UINT Index,UINT NumVectors) const
{
	if(VertexRegisterIndex != INDEX_NONE)
	{
		check(Index + NumVectors <= NumVertexRegisters);
		GDirect3DDevice->SetVertexShaderConstantF(VertexRegisterIndex + Index * NumVectors,Vectors,NumVectors);
	}
	if(PixelRegisterIndex != INDEX_NONE)
	{
		check(Index + NumVectors <= NumPixelRegisters);
		GDirect3DDevice->SetPixelShaderConstantF(PixelRegisterIndex + Index * NumVectors,Vectors,NumVectors);
	}
	check(SamplerIndex == INDEX_NONE);
}

//
//	FD3DShaderParameter::SetTexture
//

void FD3DShaderParameter::SetTexture(IDirect3DBaseTexture9* Texture,DWORD Filter,UBOOL Clamp,INT ElementIndex,UBOOL SRGB,UINT Index) const
{
	if(SamplerIndex != INDEX_NONE)
	{
		check(Index < NumSamplers);
		GDirect3DDevice->SetTexture(SamplerIndex + Index,Texture);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MAGFILTER,Filter);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MINFILTER,Filter == D3DTEXF_LINEAR && GD3DRenderDevice->LevelOfAnisotropy > 1 ? D3DTEXF_ANISOTROPIC : Filter);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MAXANISOTROPY,GD3DRenderDevice->LevelOfAnisotropy);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MIPFILTER,Filter == D3DTEXF_LINEAR && GD3DRenderDevice->UseTrilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ADDRESSU,Clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ADDRESSV,Clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ADDRESSW,Clamp ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
#ifndef XBOX
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ELEMENTINDEX,ElementIndex);
#endif
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_SRGBTEXTURE,SRGB);
	}
	check(VertexRegisterIndex == INDEX_NONE);
	check(PixelRegisterIndex == INDEX_NONE);
}

//
//	FD3DShaderParameter::SetUserTexture
//

void FD3DShaderParameter::SetUserTexture(FD3DTexture* Texture,UINT Index) const
{
	BEGINCYCLECOUNTER(GD3DStats.SetUserTextureTime);
	GD3DStats.SetUserTextures.Value++;

	if(SamplerIndex != INDEX_NONE)
	{
		check(Index < NumSamplers);
		GDirect3DDevice->SetTexture(SamplerIndex + Index,Texture->Texture);
		if( Texture->RequiresPointFiltering )
		{
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MAGFILTER,D3DTEXF_POINT);
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MINFILTER,D3DTEXF_POINT);
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MIPFILTER,D3DTEXF_POINT);
		}
		else
		{
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MAGFILTER,D3DTEXF_LINEAR);
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MINFILTER,GD3DRenderDevice->LevelOfAnisotropy > 1 ? D3DTEXF_ANISOTROPIC : D3DTEXF_LINEAR);
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MAXANISOTROPY,GD3DRenderDevice->LevelOfAnisotropy);
			GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_MIPFILTER,GD3DRenderDevice->UseTrilinear ? D3DTEXF_LINEAR : D3DTEXF_POINT);
		}
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ADDRESSU,Texture->ClampU ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ADDRESSV,Texture->ClampV ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_ADDRESSW,Texture->ClampW ? D3DTADDRESS_CLAMP : D3DTADDRESS_WRAP);
		GDirect3DDevice->SetSamplerState(SamplerIndex + Index,D3DSAMP_SRGBTEXTURE,Texture->GammaCorrected);
	}
	check(VertexRegisterIndex == INDEX_NONE);
	check(PixelRegisterIndex == INDEX_NONE);

	ENDCYCLECOUNTER;
}

//
//	FD3DShader::FD3DShader
//

FD3DShader::FD3DShader(FD3DResourceSet* InSet,ED3DShaderFunction InFunction):
	FD3DResource(InSet),
	Function(InFunction),
	FirstParameter(NULL),
	PreviousLightingTextureParameter(this,TEXT("PreviousLightingTexture")),
	ScreenPositionScaleBiasParameter(this,TEXT("ScreenPositionScaleBias")),
	LightTextureParameter(this,TEXT("LightTexture")),
	TwoSidedSignParameter(this,TEXT("TwoSidedSign"))
{}

//
//	FD3DShader::MapParameter
//

UBOOL FD3DShader::MapParameter(ED3DShaderParameterType ParameterType,const TCHAR* ParameterName,UINT RegisterIndex,UINT NumRegisters)
{
	for(FD3DShaderParameter* Parameter = FirstParameter;Parameter;Parameter = Parameter->NextParameter)
	{
		if(!appStricmp(Parameter->ParameterName,ParameterName))
		{
			switch(ParameterType)
			{
			case SPT_VertexShaderConstant:
				Parameter->VertexRegisterIndex = RegisterIndex;
				Parameter->NumVertexRegisters = NumRegisters;
				return 1;
			case SPT_PixelShaderConstant:
				Parameter->PixelRegisterIndex = RegisterIndex;
				Parameter->NumPixelRegisters = NumRegisters;
				return 1;
			case SPT_PixelShaderSampler:
				Parameter->SamplerIndex = RegisterIndex;
				Parameter->NumSamplers = NumRegisters;
				return 1;
			};
		}
	}

	return 0;
}

//
//	FD3DShader::Cache
//

TArray<FString> FD3DShader::Cache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions)
{
	HRESULT	Result;

	AddDefinition(Definitions,TEXT("SUPPORTS_FP_FILTERING"),TEXT("%u"),GD3DRenderDevice->UseFPFiltering);
	AddDefinition(Definitions,TEXT("SUPPORTS_HW_SHADOWMAP"),TEXT("%u"),GD3DRenderDevice->UseHWShadowMaps);

#ifdef XBOX
	AddDefinition( Definitions,TEXT("XBOX"), TEXT("1") );
#endif

	// Compile the shaders.

	INT									VertexShaderCodeIndex = INDEX_NONE,
										PixelShaderCodeIndex = INDEX_NONE;
	TArray<FD3DUnmappedShaderParameter>	UnmappedParameters;
	TArray<FString>						Errors = CompileShaders(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions,VertexShaderCodeIndex,PixelShaderCodeIndex,UnmappedParameters);

	USE_OWN_ALLOCATOR;
	if(!Errors.Num())
	{
		if(VertexShaderFunction)
		{
			check(VertexShaderCodeIndex != INDEX_NONE);

			NumVertexShaderInstructions = GD3DRenderDevice->CompiledShaders(VertexShaderCodeIndex).NumInstructions;

			if(!GD3DRenderDevice->VertexShaders(VertexShaderCodeIndex))
			{
				// Create the device vertex shader.

				Result = GDirect3DDevice->CreateVertexShader((DWORD*)&GD3DRenderDevice->CompiledShaders(VertexShaderCodeIndex).Code(0),&GD3DRenderDevice->VertexShaders(VertexShaderCodeIndex).Handle);
				if(FAILED(Result))
					appErrorf(TEXT("Failed to create vertex shader: %s"),*D3DError(Result));
			}

			VertexShader = GD3DRenderDevice->VertexShaders(VertexShaderCodeIndex);
		}

		if(PixelShaderFunction)
		{
			check(PixelShaderCodeIndex != INDEX_NONE);

			NumPixelShaderInstructions = GD3DRenderDevice->CompiledShaders(PixelShaderCodeIndex).NumInstructions;

			if(!GD3DRenderDevice->PixelShaders(PixelShaderCodeIndex))
			{
				// Create the device pixel shader.

				Result = GDirect3DDevice->CreatePixelShader((DWORD*)&GD3DRenderDevice->CompiledShaders(PixelShaderCodeIndex).Code(0),&GD3DRenderDevice->PixelShaders(PixelShaderCodeIndex).Handle);
				if(FAILED(Result))
					appErrorf(TEXT("Failed to create pixel shader: %s"),*D3DError(Result));
			}

			PixelShader = GD3DRenderDevice->PixelShaders(PixelShaderCodeIndex);
		}

		// Map parameters to constant and sampler registers.

		for(UINT ParameterIndex = 0;ParameterIndex < (UINT)UnmappedParameters.Num();ParameterIndex++)
			MapParameter(
				(ED3DShaderParameterType)UnmappedParameters(ParameterIndex).Type,
				*UnmappedParameters(ParameterIndex).Name,
				UnmappedParameters(ParameterIndex).RegisterIndex,
				UnmappedParameters(ParameterIndex).NumRegisters
				);
	}
	USE_DEFAULT_ALLOCATOR;

	return Errors;
}

//
//	FD3DShader::SafeCache
//

void FD3DShader::SafeCache(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions)
{
	TArray<FString>	Errors = Cache(ShaderFilename,VertexShaderFunction,PixelShaderFunction,Definitions);

	if(Errors.Num())
	{
		FString	ErrorString;
		for(UINT ErrorIndex = 0;ErrorIndex < (UINT)Errors.Num();ErrorIndex++)
			ErrorString = FString::Printf(TEXT("%s%s\n"),*ErrorString,*Errors(ErrorIndex));
		appErrorf(TEXT("Unexpected shader compilation failure:\n%s"),*ErrorString);
	}
}

//
//	FD3DShaderInclude
//

class FD3DShaderInclude: public ID3DXInclude
{
private:

	FD3DShader*			Shader;
	TArray<ANSICHAR*>	Files;

public:

	// Constructor.

	FD3DShaderInclude(FD3DShader* InShader): Shader(InShader) {}
	~FD3DShaderInclude() { check(!Files.Num());	}

	// ID3DXInclude interface.

	STDMETHOD(Open)(D3DXINCLUDE_TYPE Type,LPCSTR Name,LPCVOID ParentData,LPCVOID* Data,UINT* Bytes)
	{
		FString		FileContents = Shader->GetShaderSource(ANSI_TO_TCHAR(Name));
		ANSICHAR*	AnsiFileContents = new ANSICHAR[FileContents.Len() + 1];
		strncpy( AnsiFileContents, TCHAR_TO_ANSI(*FileContents), FileContents.Len() + 1 );
		Files.AddItem(AnsiFileContents);
		*Data = (LPCVOID)AnsiFileContents;
		*Bytes = FileContents.Len();
		return D3D_OK;
	}

	STDMETHOD(Close)(LPCVOID Data)
	{
		Files.RemoveItem((ANSICHAR*)Data);
		delete Data;
		return D3D_OK;
	}
};

//
//	ProcessConstantTable
//

void ProcessConstantTable(ED3DShaderType ShaderType,ID3DXConstantTable* ConstantTable,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters)
{
	// Read the constant table description.

	D3DXCONSTANTTABLE_DESC	Desc;

	HRESULT	Result = ConstantTable->GetDesc(&Desc);
	if(FAILED(Result))
		appErrorf(TEXT("GetDesc failed: %s"),*D3DError(Result));

	// Map each constant in the table.

	for(UINT ConstantIndex = 0;ConstantIndex < Desc.Constants;ConstantIndex++)
	{
		// Read the constant description.

		D3DXCONSTANT_DESC	ConstantDesc;
		UINT				NumConstants = 1;

		Result = ConstantTable->GetConstantDesc(ConstantTable->GetConstant(NULL,ConstantIndex),&ConstantDesc,&NumConstants);
		if(FAILED(Result))
			appErrorf(TEXT("GetConstantDesc failed: %s"),*D3DError(Result));

		// Map the constant register to a parameter.

		ED3DShaderParameterType	ParameterType;
		switch(ConstantDesc.Type)
		{
		case D3DXPT_FLOAT:
			ParameterType = ShaderType == ST_VertexShader ? SPT_VertexShaderConstant : SPT_PixelShaderConstant;
			break;
		case D3DXPT_SAMPLER:
		case D3DXPT_SAMPLER2D:
		case D3DXPT_SAMPLER3D:
		case D3DXPT_SAMPLERCUBE:
			check(ShaderType == ST_PixelShader);
			ParameterType = SPT_PixelShaderSampler;
			break;
		default:
			continue; // Skip parameters of unknown type.
		};

		new(UnmappedParameters) FD3DUnmappedShaderParameter(ParameterType,ANSI_TO_TCHAR(ConstantDesc.Name),ConstantDesc.RegisterIndex,ConstantDesc.RegisterCount);
	}
}

//
//	FD3DShader::CompileShaders
//

TArray<FString> FD3DShader::CompileShaders(const TCHAR* ShaderFilename,const TCHAR* VertexShaderFunction,const TCHAR* PixelShaderFunction,TArray<FD3DDefinition>& Definitions,INT& VertexShaderCode,INT& PixelShaderCode,TArray<FD3DUnmappedShaderParameter>& UnmappedParameters)
{
	TArray<FString>		Errors;
	FD3DShaderInclude	Include(this);
	HRESULT				Result;

	// Build an array of D3DXMACROs from the definitions list.

	TArray<D3DXMACRO>	Macros;
	for(UINT DefinitionIndex = 0;DefinitionIndex < (UINT)Definitions.Num();DefinitionIndex++)
	{
		FD3DDefinition&	Definition = Definitions(DefinitionIndex);
		D3DXMACRO		Macro;

		ANSICHAR* TempName = new ANSICHAR[Definition.Name.Len() + 1];
		strncpy( TempName, TCHAR_TO_ANSI(*Definition.Name), Definition.Name.Len() + 1 );
		ANSICHAR* TempValue = new ANSICHAR[Definition.Value.Len() + 1];
		strncpy( TempValue, TCHAR_TO_ANSI(*Definition.Value), Definition.Value.Len() + 1 );

		Macro.Name = TempName;
		Macro.Definition = TempValue;		
		Macros.AddItem(Macro);
	}

	D3DXMACRO	MacrosTerminator = { NULL, NULL };
	Macros.AddItem(MacrosTerminator);

	// Load the shader's function file.

	FString	FunctionFile = GetShaderSource(ShaderFilename);

	// Compile the vertex shader.

	check(VertexShaderFunction);

	{
		TD3DRef<ID3DXBuffer>		VertexShaderBuffer = NULL;
		TD3DRef<ID3DXBuffer>		VertexShaderErrors = NULL;
		TD3DRef<ID3DXConstantTable>	VertexShaderConstantTable = NULL;

		Result = D3DXCompileShader(
			TCHAR_TO_ANSI(*FunctionFile),
			FunctionFile.Len(),
			&Macros(0),
			&Include,
			TCHAR_TO_ANSI(VertexShaderFunction),
			GetVertexShaderTarget(),
			D3DXSHADER_PARTIALPRECISION,
			&VertexShaderBuffer.Handle,
			&VertexShaderErrors.Handle,
			&VertexShaderConstantTable.Handle
			);

		// Try again without optimizations. This might help depending on the platform/ version of D3DX.
		if( FAILED( Result ) )
		{
			Result = D3DXCompileShader(
				TCHAR_TO_ANSI(*FunctionFile),
				FunctionFile.Len(),
				&Macros(0),
				&Include,
				TCHAR_TO_ANSI(VertexShaderFunction),
				GetVertexShaderTarget(),
				D3DXSHADER_SKIPOPTIMIZATION,
				&VertexShaderBuffer.Handle,
				&VertexShaderErrors.Handle,
				&VertexShaderConstantTable.Handle
				);
		}

		if(FAILED(Result))
		{
			if(VertexShaderErrors && VertexShaderErrors->GetBufferPointer())
			{
				new(Errors) FString(ANSI_TO_TCHAR((ANSICHAR*)VertexShaderErrors->GetBufferPointer()));
				return Errors;
			}
			else
				appErrorf(TEXT("Failed to compile vertex shader %s: %s"),ShaderFilename,*D3DError(Result));
		}

		VertexShaderCode = GD3DRenderDevice->GetShaderCodeIndex(VertexShaderBuffer->GetBufferPointer(),VertexShaderBuffer->GetBufferSize());

		// Map parameters to the vertex shader's registers.

		ProcessConstantTable(ST_VertexShader,VertexShaderConstantTable,UnmappedParameters);
	}

	check(PixelShaderFunction);

	{
		// Compile the pixel shader.

		TD3DRef<ID3DXBuffer>		PixelShaderBuffer = NULL;
		TD3DRef<ID3DXBuffer>		PixelShaderErrors = NULL;
		TD3DRef<ID3DXConstantTable>	PixelShaderConstantTable = NULL;

		Result = D3DXCompileShader(
			TCHAR_TO_ANSI(*FunctionFile),
			FunctionFile.Len(),
			&Macros(0),
			&Include,
			TCHAR_TO_ANSI(PixelShaderFunction),
			GetPixelShaderTarget(),
			0,
			&PixelShaderBuffer.Handle,
			&PixelShaderErrors.Handle,
			&PixelShaderConstantTable.Handle
			);

		// Try again without optimizations. This might help depending on the platform/ version of D3DX.
		if( FAILED( Result ) )
		{
			Result = D3DXCompileShader(
				TCHAR_TO_ANSI(*FunctionFile),
				FunctionFile.Len(),
				&Macros(0),
				&Include,
				TCHAR_TO_ANSI(PixelShaderFunction),
				GetPixelShaderTarget(),
				D3DXSHADER_SKIPOPTIMIZATION,
				&PixelShaderBuffer.Handle,
				&PixelShaderErrors.Handle,
				&PixelShaderConstantTable.Handle
				);
		}

		if(FAILED(Result))
		{
			if(PixelShaderErrors && PixelShaderErrors->GetBufferPointer())
			{
				new(Errors) FString(ANSI_TO_TCHAR((ANSICHAR*)PixelShaderErrors->GetBufferPointer()));
				return Errors;
			}
			else
				appErrorf(TEXT("Failed to compile pixel shader %s: %s"),ShaderFilename,*D3DError(Result));
		}

		PixelShaderCode = GD3DRenderDevice->GetShaderCodeIndex(PixelShaderBuffer->GetBufferPointer(),PixelShaderBuffer->GetBufferSize());

		// Map parameters to the pixel shader's registers.

		ProcessConstantTable(ST_PixelShader,PixelShaderConstantTable,UnmappedParameters);
	}

	// Clean up strings in macros array.

	for(UINT MacroIndex = 0;MacroIndex < (UINT)Macros.Num();MacroIndex++)
	{
		delete Macros(MacroIndex).Name;
		delete Macros(MacroIndex).Definition;
	}

	return Errors;
}

//
//	FD3DShader::GetShaderSource
//

FString FD3DShader::GetShaderSource(const TCHAR* Filename)
{
	return GD3DRenderDevice->GetShaderSource(Filename);
}

//
//	FD3DShader::Set
//

void FD3DShader::Set()
{
	GD3DRenderDevice->SetVertexShader(VertexShader);
	GD3DRenderDevice->SetPixelShader(PixelShader);

	if(GD3DSceneRenderer)
	{
		PreviousLightingTextureParameter.SetTexture(GD3DSceneRenderer->GetLastLightingBuffer(),D3DTEXF_POINT,1);
		ScreenPositionScaleBiasParameter.SetVector(
			(FLOAT)GD3DSceneRenderer->Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX / 2.0f,
			(FLOAT)GD3DSceneRenderer->Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY / -2.0f,
			-1.0f * (FLOAT)GD3DSceneRenderer->Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY / -2.0f + 0.5f / GD3DRenderDevice->BufferSizeY,
			1.0f * (FLOAT)GD3DSceneRenderer->Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX / 2.0f + 0.5f / GD3DRenderDevice->BufferSizeX
			);
	}

	LightTextureParameter.SetTexture( GD3DRenderDevice->LightRenderTarget ? GD3DRenderDevice->LightRenderTarget->GetTexture() : NULL, D3DTEXF_POINT, 1 );

	TwoSidedSignParameter.SetScalar(GD3DSceneRenderer ? GD3DSceneRenderer->TwoSidedSign : 1.0f);
}

//
//	FD3DShader::Dump
//

void FD3DShader::Dump(FOutputDevice& Ar)
{
	Ar.Logf(TEXT("%s: Function=%u, Cached=%u, NumRefs=%u"),GetTypeDescription(),(UINT)Function,Cached,NumRefs);
}

//
//	FD3DShader::GetDumpName
//

FString FD3DShader::GetDumpName() const
{
	const TCHAR*	BlendingString = (Function & SF_UseFPBlending) ? TEXT(".SF_UseFPBlending") : TEXT("");
	const TCHAR*	OpaqueString = (Function & SF_OpaqueLayer) ? TEXT(".SF_OpaqueLayer") : TEXT("");
#define	SHADERFUNCTIONNAME(x) case x: return FString::Printf(TEXT("%s%s%s"),TEXT(#x),BlendingString,OpaqueString);
	switch(Function & SF_FunctionMask)
	{
	SHADERFUNCTIONNAME(SF_PointLight)
	SHADERFUNCTIONNAME(SF_DirectionalLight)
	SHADERFUNCTIONNAME(SF_SpotLight)
	SHADERFUNCTIONNAME(SF_Emissive)
	SHADERFUNCTIONNAME(SF_UnlitTranslucency)
	SHADERFUNCTIONNAME(SF_Unlit)
	SHADERFUNCTIONNAME(SF_ShadowDepth)
	SHADERFUNCTIONNAME(SF_Wireframe)
	SHADERFUNCTIONNAME(SF_WireframeHitProxy)
	SHADERFUNCTIONNAME(SF_VertexLighting)
	SHADERFUNCTIONNAME(SF_HitProxy)
	SHADERFUNCTIONNAME(SF_ShadowVolume)
	SHADERFUNCTIONNAME(SF_ShadowDepthTest)
	SHADERFUNCTIONNAME(SF_NVShadowDepthTest)
	SHADERFUNCTIONNAME(SF_LightFunction)
	SHADERFUNCTIONNAME(SF_Blur)
	SHADERFUNCTIONNAME(SF_AccumulateImage)
	SHADERFUNCTIONNAME(SF_BlendImage)
	SHADERFUNCTIONNAME(SF_Exposure)
	SHADERFUNCTIONNAME(SF_Fill)
	SHADERFUNCTIONNAME(SF_FinishImage)
	SHADERFUNCTIONNAME(SF_Copy)
	SHADERFUNCTIONNAME(SF_CopyFill)
	SHADERFUNCTIONNAME(SF_Fog)
	SHADERFUNCTIONNAME(SF_EmissiveTile)
	SHADERFUNCTIONNAME(SF_EmissiveTileHitProxy)
	SHADERFUNCTIONNAME(SF_Sprite)
	SHADERFUNCTIONNAME(SF_SpriteHitProxy)
	SHADERFUNCTIONNAME(SF_OcclusionQuery)
	SHADERFUNCTIONNAME(SF_TranslucentLayer)
	default: return TEXT("SF_Unknown");
	}
}

//
//	FD3DShader::DumpShader
//

void FD3DShader::DumpShader()
{
	// Disassemble the vertex and pixel shader and dump them to the hard drive.

	//@warning: code relies on D3DX summer update 2004.
#if ALLOW_DUMP_SHADERS
	if(VertexShader)
	{
		INT	VertexShaderCodeIndex = INDEX_NONE;
		for(INT ShaderIndex = 0;ShaderIndex < GD3DRenderDevice->VertexShaders.Num();ShaderIndex++)
		{
			if(GD3DRenderDevice->VertexShaders(ShaderIndex) == VertexShader)
			{
				VertexShaderCodeIndex = ShaderIndex;
				break;
			}
		}
		if(VertexShaderCodeIndex != INDEX_NONE)
		{
			TD3DRef<ID3DXBuffer>	VertexShaderAssembly;
			HRESULT	Result = D3DXDisassembleShader(
				(DWORD*)&GD3DRenderDevice->CompiledShaders(VertexShaderCodeIndex).Code(0),
				0,
				NULL,
				&VertexShaderAssembly.Handle
				);
			if(FAILED(Result))
				appErrorf(TEXT("Failed to disassemble vertex shader: %s"),*D3DError(Result));
			appSaveStringToFile(FString(ANSI_TO_TCHAR((ANSICHAR*)VertexShaderAssembly->GetBufferPointer())),*FString::Printf(TEXT("%s\\Shaders\\%s.VertexShader.txt"),appBaseDir(),*GetDumpName()));
		}
	}

	if(PixelShader)
	{
		INT	PixelShaderCodeIndex = INDEX_NONE;
		for(INT ShaderIndex = 0;ShaderIndex < GD3DRenderDevice->PixelShaders.Num();ShaderIndex++)
		{
			if(GD3DRenderDevice->PixelShaders(ShaderIndex) == PixelShader)
			{
				PixelShaderCodeIndex = ShaderIndex;
				break;
			}
		}
		if(PixelShaderCodeIndex != INDEX_NONE)
		{
			TD3DRef<ID3DXBuffer>	PixelShaderAssembly;
			HRESULT	Result = D3DXDisassembleShader(
				(DWORD*)&GD3DRenderDevice->CompiledShaders(PixelShaderCodeIndex).Code(0),
				0,
				NULL,
				&PixelShaderAssembly.Handle
				);
			if(FAILED(Result))
				appErrorf(TEXT("Failed to disassemble pixel shader: %s"),*D3DError(Result));
			appSaveStringToFile(FString(ANSI_TO_TCHAR((ANSICHAR*)PixelShaderAssembly->GetBufferPointer())),*FString::Printf(TEXT("%s\\Shaders\\%s.PixelShader.txt"),appBaseDir(),*GetDumpName()));
		}
	}
#endif
}

//
//	FD3DSpriteShader::Set
//

void FD3DSpriteShader::Set(FTexture2D* Texture,FColor Color)
{
	FD3DShader::Set();

	FD3DTexture2D*	CachedTexture = (FD3DTexture2D*)GD3DRenderDevice->Resources.GetCachedResource(Texture);

	SpriteTextureParameter.SetTexture(CachedTexture->Texture,D3DTEXF_LINEAR,1,0,CachedTexture->GammaCorrected);
	SpriteColorParameter.SetColor(Color);
}

//
//	FD3DSpriteHitProxyShader::Set
//

void FD3DSpriteHitProxyShader::Set(FTexture2D* Texture,FColor Color)
{
	FD3DShader::Set();

	FD3DTexture2D*	CachedTexture = (FD3DTexture2D*)GD3DRenderDevice->Resources.GetCachedResource(Texture);

	SpriteTextureParameter.SetTexture(CachedTexture->Texture,D3DTEXF_LINEAR,1,0,CachedTexture->GammaCorrected);
	SpriteColorParameter.SetColor(Color);

	FPlane IdColor = FColor(GD3DRenderInterface.CurrentHitProxyIndex).Plane();
	HitProxyIdParameter.SetVector(IdColor.X,IdColor.Y,IdColor.Z,IdColor.W);
}

//
//	FD3DBlurShader::Cache
//

void FD3DBlurShader::Cache()
{
	TArray<FD3DDefinition>	Definitions;
	AddDefinition(Definitions,TEXT("NUM_SAMPLES"),TEXT("%u"),NumSamples);
	FD3DShader::SafeCache(TEXT("PostProcess.hlsl"),TEXT("blurVertexShader"),TEXT("blur"),Definitions);
}

//
//	FD3DBlurShader::Set
//

void FD3DBlurShader::Set(const TD3DRef<IDirect3DTexture9>& BlurTexture,const FVector2D* Offsets,const FPlane* Weights,const FPlane& FinalMultiple,UBOOL Filtered)
{
	FD3DShader::Set();

	BlurTextureParameter.SetTexture(BlurTexture,Filtered ? D3DTEXF_LINEAR : D3DTEXF_POINT,1);

	D3DSURFACE_DESC	SurfaceDesc;
	BlurTexture->GetLevelDesc(0,&SurfaceDesc);

	for(INT SampleIndex = 0;SampleIndex < NumSamples;SampleIndex += 2)
	{
		FLOAT	OffsetVector[4] =
		{
			Offsets[SampleIndex + 0].X / (FLOAT)SurfaceDesc.Width,
			Offsets[SampleIndex + 0].Y / (FLOAT)SurfaceDesc.Height,
			Offsets[SampleIndex + 1].Y / (FLOAT)SurfaceDesc.Height,
			Offsets[SampleIndex + 1].X / (FLOAT)SurfaceDesc.Width
		};
		SampleOffsetsParameter.SetVectors(OffsetVector,SampleIndex / 2,1);
	}

	FPlane	Sum(0,0,0,0);
	for(INT SampleIndex = 0;SampleIndex < NumSamples;SampleIndex++)
		Sum += Weights[SampleIndex];

	for(INT SampleIndex = 0;SampleIndex < NumSamples;SampleIndex++)
	{
		FLOAT	WeightVector[4] =
		{
			Weights[SampleIndex].X * FinalMultiple.X / Sum.X,
			Weights[SampleIndex].Y * FinalMultiple.Y / Sum.Y,
			Weights[SampleIndex].Z * FinalMultiple.Z / Sum.Z,
			Weights[SampleIndex].W * FinalMultiple.W / Sum.W
		};
		SampleWeightsParameter.SetVectors(WeightVector,SampleIndex,1);
	}
}

//
//	FD3DBlurShaderCache::GetCachedShader
//

FD3DBlurShader* FD3DBlurShaderCache::GetCachedShader(INT NumSamples)
{
	FD3DBlurShader*	CachedShader = GetCachedResource(NumSamples);
	if(!CachedShader)
	{
		CachedShader = new FD3DBlurShader(this,NumSamples);
		AddCachedResource(NumSamples,CachedShader);
	}
	return CachedShader;
}

//
//	FD3DAccumulateImageShader::Set
//

void FD3DAccumulateImageShader::Set(const TD3DRef<IDirect3DTexture9>& LinearImage,const FLinearColor& ScaleFactor)
{
	FD3DShader::Set();

	if (GD3DRenderDevice->UseFPFiltering)
		LinearImageParameter.SetTexture(LinearImage,D3DTEXF_LINEAR,1);
	else
		LinearImageParameter.SetTexture(LinearImage,D3DTEXF_POINT,1);

	UINT	BlurredSceneSizeX = GD3DSceneRenderer->Context.SizeX / GD3DRenderDevice->CachedBlurBufferDivisor,
			BlurredSceneSizeY = GD3DSceneRenderer->Context.SizeY / GD3DRenderDevice->CachedBlurBufferDivisor;
	FLOAT	BufferUL = (FLOAT)GD3DSceneRenderer->Context.SizeX / (FLOAT)GD3DRenderDevice->BufferSizeX,
			BufferVL = (FLOAT)GD3DSceneRenderer->Context.SizeY / (FLOAT)GD3DRenderDevice->BufferSizeY,
			BlurBufferUL = (FLOAT)BlurredSceneSizeX / (FLOAT)GD3DRenderDevice->BlurBufferSizeX,
			BlurBufferVL = (FLOAT)BlurredSceneSizeY / (FLOAT)GD3DRenderDevice->BlurBufferSizeY;

	ScaleFactorParameter.SetColor(ScaleFactor);

	LinearImageScaleBiasParameter.SetVector(
		BlurBufferUL / BufferUL,
		BlurBufferVL / BufferVL,
		1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeY - 0.5f / (FLOAT)GD3DRenderDevice->BufferSizeY * BlurBufferVL / BufferVL,
		1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeX - 0.5f / (FLOAT)GD3DRenderDevice->BufferSizeX * BlurBufferUL / BufferUL
		);
	LinearImageResolutionParameter.SetVector(
		GD3DRenderDevice->BlurBufferSizeX,
		GD3DRenderDevice->BlurBufferSizeY,
		1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeY,
		1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeX
		);
}

//
//	FD3DBlendImageShader::Set
//

void FD3DBlendImageShader::Set(const TD3DRef<IDirect3DTexture9>& LinearImage)
{
	FD3DShader::Set();
	LinearImageParameter.SetTexture(LinearImage,D3DTEXF_POINT,1);
}

//
//	FD3DExposureShader::Set
//

void FD3DExposureShader::Set(const TD3DRef<IDirect3DTexture9>& LinearImage,const TD3DRef<IDirect3DTexture9>& ExposureTexture,FLOAT MaxDeltaExposure,FLOAT ManualExposure)
{
	FD3DShader::Set();

	LinearImageParameter.SetTexture(LinearImage,D3DTEXF_POINT,1);
	LinearImageResolutionParameter.SetVector(
		GD3DRenderDevice->BlurBufferSizeX,
		GD3DRenderDevice->BlurBufferSizeY,
		1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeY,
		1.0f / (FLOAT)GD3DRenderDevice->BlurBufferSizeX
		);
	ExposureTextureParameter.SetTexture(ExposureTexture,D3DTEXF_POINT,1);
	MaxDeltaExposureParameter.SetScalar(MaxDeltaExposure);
	ManualExposureParameter.SetScalar(ManualExposure);
}

//
//	FD3DFillShader::Set
//

void FD3DFillShader::Set(const FLinearColor& FillValue)
{
	FD3DShader::Set();
	FillValueParameter.SetColor(FillValue);
}

//
//	FD3DFinishImageShader::Set
//

void FD3DFinishImageShader::Set(const TD3DRef<IDirect3DTexture9>& LinearImage,const TD3DRef<IDirect3DTexture9>& ExposureTexture,const FLinearColor& ScaleFactor,const FLinearColor& OverlayColor)
{
	FD3DShader::Set();

	LinearImageParameter.SetTexture(LinearImage,D3DTEXF_POINT,1);
	ScaleFactorParameter.SetColor(ScaleFactor);
	ExposureTextureParameter.SetTexture(ExposureTexture,D3DTEXF_POINT,1);
	OverlayColorParameter.SetColor(OverlayColor);
}

//
//	FD3DCopyShader::Set
//

void FD3DCopyShader::Set(const TD3DRef<IDirect3DTexture9>& CopyTexture)
{
	FD3DShader::Set();
	CopyTextureParameter.SetTexture(CopyTexture,D3DTEXF_POINT,1);
}

//
//	FD3DCopyFillShader::Set
//

void FD3DCopyFillShader::Set(const TD3DRef<IDirect3DTexture9>& CopyTexture,const FLinearColor& FillColor,const FPlane& FillAlpha)
{
	FD3DShader::Set();
	CopyTextureParameter.SetTexture(CopyTexture,D3DTEXF_POINT,1);
	FillValueParameter.SetColor(FillColor);
	FillAlphaParameter.SetVector(FillAlpha.X,FillAlpha.Y,FillAlpha.Z,FillAlpha.W);
}

//
//	FD3DFogShader::Set
//

void FD3DFogShader::Set(const TD3DRef<IDirect3DTexture9>& SceneTexture)
{
	check(GD3DSceneRenderer);

	FD3DShader::Set();
	SceneTextureParameter.SetTexture(SceneTexture,D3DTEXF_POINT,1);
	ScreenToWorldParameter.SetMatrix4(
		FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-NEAR_CLIPPING_PLANE * (1.0f - Z_PRECISION),0)
			) *
			GD3DSceneRenderer->InvViewProjectionMatrix *
			FTranslationMatrix(-(FVector)GD3DSceneRenderer->CameraPosition)
		);
	CameraPositionParameter.SetVector(GD3DSceneRenderer->CameraPosition,GD3DSceneRenderer->CameraPosition.W);

	for(INT LayerIndex = 0;LayerIndex < 4 && LayerIndex < (INT)FogInScatteringParameter.NumPixelRegisters;LayerIndex++)
	{
		FogInScatteringParameter.SetColor(GD3DSceneRenderer->FogInScattering[LayerIndex],LayerIndex);
	}
	FogDistanceScaleParameter.SetVectors(GD3DSceneRenderer->FogDistanceScale,0,1);
	FogMinHeightParameter.SetVectors(GD3DSceneRenderer->FogMinHeight,0,1);
	FogMaxHeightParameter.SetVectors(GD3DSceneRenderer->FogMaxHeight,0,1);
}

//
//	FD3DShadowVolumeShader::Set
//

void FD3DShadowVolumeShader::Set(FVertexFactory* VertexFactory,ULightComponent* Light)
{
	FD3DShader::Set();

	check(VertexFactory->Type()->IsA(FLocalShadowVertexFactory::StaticType));
	LocalToWorldParameter.SetMatrix4(((FLocalShadowVertexFactory*)VertexFactory)->LocalToWorld);

	ViewProjectionMatrixParameter.SetMatrix4(GD3DSceneRenderer->ViewProjectionMatrix);

	if(Cast<UPointLightComponent>(Light))
		LightPositionParameter.SetVector(Light->GetOrigin(),1);
	else if(Cast<UDirectionalLightComponent>(Light))
		LightPositionParameter.SetVector(-Light->GetDirection(),0);
}

//
//	FD3DOcclusionQueryShader::Set
//

void FD3DOcclusionQueryShader::Set()
{
	FD3DShader::Set();
	ViewProjectionMatrixParameter.SetMatrix4(GD3DSceneRenderer->ViewProjectionMatrix);
}

void FD3DTileShader::Set(FD3DTexture* TileTexture)
{
	FD3DShader::Set();
	if(Function == SF_Tile || Function == SF_TileHitProxy)
	{
		check(TileTexture);
		TileTextureParameter.SetTexture(TileTexture->Texture,D3DTEXF_LINEAR);
	}
	FPlane IdColor = FColor(GD3DRenderInterface.CurrentHitProxyIndex).Plane();
	HitProxyIdParameter.SetVector(IdColor.X,IdColor.Y,IdColor.Z,IdColor.W);
}