/*=============================================================================
	D3D11Shaders.cpp: D3D shader RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

FVertexShaderRHIRef FD3D11DynamicRHI::CreateVertexShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	TRefCountPtr<ID3D11VertexShader> VertexShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateVertexShader((DWORD*)&Code(0),Code.Num(),NULL,VertexShader.GetInitReference()));
	return new FD3D11VertexShader(VertexShader,Code);
}

FPixelShaderRHIRef FD3D11DynamicRHI::CreatePixelShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	TRefCountPtr<FD3D11PixelShader> PixelShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreatePixelShader((DWORD*)&Code(0),Code.Num(),NULL,(ID3D11PixelShader**)PixelShader.GetInitReference()));
	return PixelShader.GetReference();
}

FHullShaderRHIRef FD3D11DynamicRHI::CreateHullShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	TRefCountPtr<FD3D11HullShader> HullShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateHullShader((DWORD*)&Code(0),Code.Num(),NULL,(ID3D11HullShader**)HullShader.GetInitReference()));
	return HullShader.GetReference();
}

FDomainShaderRHIRef FD3D11DynamicRHI::CreateDomainShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	TRefCountPtr<FD3D11DomainShader> DomainShader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateDomainShader((DWORD*)&Code(0),Code.Num(),NULL,(ID3D11DomainShader**)DomainShader.GetInitReference()));
	return DomainShader.GetReference();
}

FGeometryShaderRHIRef FD3D11DynamicRHI::CreateGeometryShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	TRefCountPtr<FD3D11GeometryShader> Shader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateGeometryShader((DWORD*)&Code(0),Code.Num(),NULL,(ID3D11GeometryShader**)Shader.GetInitReference()));
	return Shader.GetReference();
}

FComputeShaderRHIRef FD3D11DynamicRHI::CreateComputeShader(const TArray<BYTE>& Code) 
{ 
	check(Code.Num());
	TRefCountPtr<FD3D11ComputeShader> Shader;
	VERIFYD3D11RESULT(Direct3DDevice->CreateComputeShader((DWORD*)&Code(0),Code.Num(),NULL,(ID3D11ComputeShader**)Shader.GetInitReference()));
	return Shader.GetReference();
}

void FD3D11DynamicRHI::SetMultipleViewports(UINT Count, FViewPortBounds* Data) 
{ 
	check(Count > 0);
	check(Data);

	// structures are chosen to be directly mappable
	D3D11_VIEWPORT* D3DData = (D3D11_VIEWPORT*)Data;

	Direct3DDeviceIMContext->RSSetViewports(Count, D3DData);
}

FD3D11BoundShaderState::FD3D11BoundShaderState(
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	DWORD* InStreamStrides,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI,
	FHullShaderRHIParamRef InHullShaderRHI,
	FDomainShaderRHIParamRef InDomainShaderRHI,
	FGeometryShaderRHIParamRef InGeometryShaderRHI,
	ID3D11Device* Direct3DDevice
	):
	CacheLink(InVertexDeclarationRHI,InStreamStrides,InVertexShaderRHI,InPixelShaderRHI,InHullShaderRHI,InDomainShaderRHI,InGeometryShaderRHI,this)
{
	DYNAMIC_CAST_D3D11RESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_D3D11RESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_D3D11RESOURCE(PixelShader,InPixelShader);
	DYNAMIC_CAST_D3D11RESOURCE(HullShader,InHullShader);
	DYNAMIC_CAST_D3D11RESOURCE(DomainShader,InDomainShader);
	DYNAMIC_CAST_D3D11RESOURCE(GeometryShader,InGeometryShader);

	// Create an input layout for this combination of vertex declaration and vertex shader.
	VERIFYD3D11RESULT(Direct3DDevice->CreateInputLayout(
		&InVertexDeclaration->VertexElements(0),
		InVertexDeclaration->VertexElements.Num(),
		&InVertexShader->Code(0),
		InVertexShader->Code.Num(),
		InputLayout.GetInitReference()
		));

	VertexShader = InVertexShader->Resource;
	PixelShader = InPixelShader;
	HullShader = InHullShader;
	DomainShader = InDomainShader;
	GeometryShader = InGeometryShader;
}

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param PixelShader - existing pixel shader
 * @param MobileGlobalShaderType - Specifies which global shader to use for mobile platforms
 */
FBoundShaderStateRHIRef FD3D11DynamicRHI::CreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides,
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	EMobileGlobalShaderType /*MobileGlobalShaderType*/
	)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized && Direct3DDeviceIMContext,(TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		StreamStrides,
		VertexShaderRHI,
		PixelShaderRHI
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		return new FD3D11BoundShaderState(VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI,NULL,NULL,NULL,Direct3DDevice);
	}
}

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param HullShader - existing hull shader
 * @param DomainShader - existing domain shader
 * @param PixelShader - existing pixel shader
 * @param GeometryShader - existing geometry shader
 * @param MobileGlobalShaderType - global shader type to use for mobile
 */
FBoundShaderStateRHIRef FD3D11DynamicRHI::CreateBoundShaderStateD3D11(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides,
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FHullShaderRHIParamRef HullShaderRHI, 
	FDomainShaderRHIParamRef DomainShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	FGeometryShaderRHIParamRef GeometryShaderRHI,
	EMobileGlobalShaderType /*MobileGlobalShaderType*/
	)
{
	SCOPE_CYCLE_COUNTER(STAT_D3D11CreateBoundShaderStateTime);

	checkf(GIsRHIInitialized && Direct3DDeviceIMContext,(TEXT("Bound shader state RHI resource was created without initializing Direct3D first")));

	// Check for an existing bound shader state which matches the parameters
	FCachedBoundShaderStateLink* CachedBoundShaderStateLink = GetCachedBoundShaderState(
		VertexDeclarationRHI,
		StreamStrides,
		VertexShaderRHI,
		PixelShaderRHI,
		HullShaderRHI,
		DomainShaderRHI,
		GeometryShaderRHI
		);
	if(CachedBoundShaderStateLink)
	{
		// If we've already created a bound shader state with these parameters, reuse it.
		return CachedBoundShaderStateLink->BoundShaderState;
	}
	else
	{
		return new FD3D11BoundShaderState(VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI,HullShaderRHI,DomainShaderRHI,GeometryShaderRHI,Direct3DDevice);
	}
}