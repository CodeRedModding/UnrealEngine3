/*=============================================================================
	ScreenRendering.cpp: D3D render target implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "ScreenRendering.h"

/** Vertex declaration for screen-space rendering. */
TGlobalResource<FScreenVertexDeclaration> GScreenVertexDeclaration;

// Shader implementations.
IMPLEMENT_SHADER_TYPE(,FScreenPixelShader,TEXT("ScreenPixelShader"),TEXT("Main"),SF_Pixel,0,0);
IMPLEMENT_SHADER_TYPE(,FScreenVertexShader,TEXT("ScreenVertexShader"),TEXT("Main"),SF_Vertex,0,0);

FGlobalBoundShaderState ScreenBoundShaderState;

/**
 * Draws a texture rectangle on the screen using normalized (-1 to 1) device coordinates.
 */
void DrawScreenQuad( FLOAT X0, FLOAT Y0, FLOAT U0, FLOAT V0, FLOAT X1, FLOAT Y1, FLOAT U1, FLOAT V1, const FTexture* Texture )
{ 
    // Set the screen vertex shader.
    TShaderMapRef<FScreenVertexShader> ScreenVertexShader(GetGlobalShaderMap());

    // Set the screen pixel shader.
    TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
    ScreenPixelShader->SetParameters(Texture);

	SetGlobalBoundShaderState(ScreenBoundShaderState, GScreenVertexDeclaration.VertexDeclarationRHI, *ScreenVertexShader, *ScreenPixelShader, sizeof(FScreenVertex));

    // Generate the vertices used
    FScreenVertex Vertices[4];

    Vertices[0].Position.X = X1;
    Vertices[0].Position.Y = Y0;
    Vertices[0].UV.X       = U1;
    Vertices[0].UV.Y       = V0;

    Vertices[1].Position.X = X1;
    Vertices[1].Position.Y = Y1;
    Vertices[1].UV.X       = U1;
    Vertices[1].UV.Y       = V1;

    Vertices[2].Position.X = X0;
    Vertices[2].Position.Y = Y0;
    Vertices[2].UV.X       = U0;
    Vertices[2].UV.Y       = V0;

    Vertices[3].Position.X = X0;
    Vertices[3].Position.Y = Y1;
    Vertices[3].UV.X       = U0;
    Vertices[3].UV.Y       = V1;

    RHIDrawPrimitiveUP(PT_TriangleStrip,2,Vertices,sizeof(Vertices[0]));
}
