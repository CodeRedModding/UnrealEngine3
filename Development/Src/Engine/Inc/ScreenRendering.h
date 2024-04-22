/*=============================================================================
	ScreenRendering.h: D3D render target implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCREENRENDERING_H__
#define __SCREENRENDERING_H__

struct FScreenVertex
{
	FVector2D Position;
	FVector2D UV;
};

/** The filter vertex declaration resource type. */
class FScreenVertexDeclaration : public FRenderResource
{
public:
	FVertexDeclarationRHIRef VertexDeclarationRHI;

	// Destructor.
	virtual ~FScreenVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FScreenVertex,Position),VET_Float2,VEU_Position,0));
		Elements.AddItem(FVertexElement(0,STRUCT_OFFSET(FScreenVertex,UV),VET_Float2,VEU_TextureCoordinate,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

extern TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

/**
 * A pixel shader for rendering a textured screen element.
 */
class FScreenPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FScreenPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FScreenPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		TextureParameter.Bind(Initializer.ParameterMap,TEXT("InTexture"));
	}
	FScreenPixelShader() {}

	void SetParameters(const FTexture* Texture)
	{
		SetTextureParameter(GetPixelShader(),TextureParameter,Texture);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << TextureParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter TextureParameter;
};

/**
 * A vertex shader for rendering a textured screen element.
 */
class FScreenVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FScreenVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FScreenVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
	}
	FScreenVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FShader::Serialize(Ar);
	}
};

/**
 * Draws a texture rectangle on the screen using normalized (-1 to 1) device coordinates.
 */
extern void DrawScreenQuad(  FLOAT X0, FLOAT Y0, FLOAT U0, FLOAT V0, FLOAT X1, FLOAT Y1, FLOAT U1, FLOAT V1, const FTexture* Texture );

#endif
