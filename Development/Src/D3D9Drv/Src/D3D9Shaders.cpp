/*=============================================================================
	D3D9Shaders.cpp: D3D shader RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

FVertexShaderRHIRef FD3D9DynamicRHI::CreateVertexShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	TRefCountPtr<FD3D9VertexShader> VertexShader;
	VERIFYD3D9RESULT(Direct3DDevice->CreateVertexShader((DWORD*)&Code(0),(IDirect3DVertexShader9**)VertexShader.GetInitReference()));
	return VertexShader.GetReference();
}

FPixelShaderRHIRef FD3D9DynamicRHI::CreatePixelShader(const TArray<BYTE>& Code)
{
	check(Code.Num());
	TRefCountPtr<FD3D9PixelShader> PixelShader = NULL;
	VERIFYD3D9RESULT(Direct3DDevice->CreatePixelShader((DWORD*)&Code(0),(IDirect3DPixelShader9**)PixelShader.GetInitReference()));
	return PixelShader.GetReference();
}

FD3D9BoundShaderState::FD3D9BoundShaderState(
	FVertexDeclarationRHIParamRef InVertexDeclarationRHI,
	DWORD* InStreamStrides,
	FVertexShaderRHIParamRef InVertexShaderRHI,
	FPixelShaderRHIParamRef InPixelShaderRHI
	):
	CacheLink(InVertexDeclarationRHI,InStreamStrides,InVertexShaderRHI,InPixelShaderRHI,this)
{
	DYNAMIC_CAST_D3D9RESOURCE(VertexDeclaration,InVertexDeclaration);
	DYNAMIC_CAST_D3D9RESOURCE(VertexShader,InVertexShader);
	DYNAMIC_CAST_D3D9RESOURCE(PixelShader,InPixelShader);

	VertexDeclaration = InVertexDeclaration;
	check(IsValidRef(VertexDeclaration));
	VertexShader = InVertexShader;
	PixelShader = InPixelShader;
}

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, and pixel shader
 * @param VertexDeclaration - existing vertex decl
 * @param StreamStrides - optional stream strides
 * @param VertexShader - existing vertex shader
 * @param PixelShader - existing pixel shader
 * @param MobileGlobalShaderType - global shader type to use for mobile
 */
FBoundShaderStateRHIRef FD3D9DynamicRHI::CreateBoundShaderState(
	FVertexDeclarationRHIParamRef VertexDeclarationRHI, 
	DWORD* StreamStrides, 
	FVertexShaderRHIParamRef VertexShaderRHI, 
	FPixelShaderRHIParamRef PixelShaderRHI,
	EMobileGlobalShaderType MobileGlobalShaderType
	)
{
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
		return new FD3D9BoundShaderState(VertexDeclarationRHI,StreamStrides,VertexShaderRHI,PixelShaderRHI);
	}
}
