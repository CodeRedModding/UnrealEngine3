/*=============================================================================
	OpenGLVertexDeclaration.cpp: OpenGL vertex declaration RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

IMPLEMENT_COMPARE_CONSTREF(
						   OpenGLVertexElement,
						   OpenGLVertexDeclaration,
{ return ((INT)A.Offset + A.StreamIndex * MAXWORD) - ((INT)B.Offset + B.StreamIndex * MAXWORD); }
)

FOpenGLVertexDeclaration::FOpenGLVertexDeclaration(const FVertexDeclarationElementList& InElements)
{
	for(UINT ElementIndex = 0;ElementIndex < InElements.Num();ElementIndex++)
	{
		const FVertexElement& Element = InElements(ElementIndex);
		OpenGLVertexElement GLElement;
		GLElement.StreamIndex = Element.StreamIndex;
		GLElement.Offset = Element.Offset;
		switch(Element.Type)
		{
		case VET_Float1:		GLElement.Type = GL_FLOAT; GLElement.Size = 1; GLElement.bNormalized = FALSE; break;
		case VET_Float2:		GLElement.Type = GL_FLOAT; GLElement.Size = 2; GLElement.bNormalized = FALSE; break;
		case VET_Float3:		GLElement.Type = GL_FLOAT; GLElement.Size = 3; GLElement.bNormalized = FALSE; break;
		case VET_Float4:		GLElement.Type = GL_FLOAT; GLElement.Size = 4; GLElement.bNormalized = FALSE; break;
		case VET_PackedNormal:	GLElement.Type = GL_UNSIGNED_BYTE; GLElement.Size = 4; GLElement.bNormalized = FALSE; break;
		case VET_UByte4:		GLElement.Type = GL_UNSIGNED_BYTE; GLElement.Size = 4; GLElement.bNormalized = FALSE; break;
		case VET_UByte4N:		GLElement.Type = GL_UNSIGNED_BYTE; GLElement.Size = 4; GLElement.bNormalized = TRUE; break;
		case VET_Color:			GLElement.Type = GL_UNSIGNED_BYTE; GLElement.Size = GL_BGRA; GLElement.bNormalized = TRUE; break;
		case VET_Short2:		GLElement.Type = GL_SHORT; GLElement.Size = 2; GLElement.bNormalized = FALSE; break;
		case VET_Short2N:		GLElement.Type = GL_SHORT; GLElement.Size = 2; GLElement.bNormalized = TRUE; break;
		case VET_Half2:			GLElement.Type = GL_HALF_FLOAT_ARB; GLElement.Size = 2; GLElement.bNormalized = FALSE; break;
		default: appErrorf(TEXT("Unknown RHI vertex element type %u"),InElements(ElementIndex).Type);
		};
		switch(Element.Usage)
		{
		case VEU_Position:			GLElement.Usage = GLAttr_Position; break;
		case VEU_TextureCoordinate:	check(Element.UsageIndex < 8); GLElement.Usage = GLAttr_TexCoord0 + Element.UsageIndex; break;
		case VEU_BlendWeight:		GLElement.Usage = GLAttr_Weights; break;
		case VEU_BlendIndices:		GLElement.Usage = GLAttr_Bones; break;
		case VEU_Normal:			GLElement.Usage = GLAttr_Normal; break;
		case VEU_Tangent:			GLElement.Usage = GLAttr_Tangent; break;
		case VEU_Binormal:			GLElement.Usage = GLAttr_Binormal; break;
		case VEU_Color:				check(Element.UsageIndex < 2); GLElement.Usage = GLAttr_Color0 + Element.UsageIndex; break;
		default: appErrorf(TEXT("Unknown RHI vertex element usage %u"),InElements(ElementIndex).Usage);
		};
		VertexElements.AddItem(GLElement);
	}

	// Sort the OpenGLVertexElements by stream then offset.
	Sort<USE_COMPARE_CONSTREF(OpenGLVertexElement,OpenGLVertexDeclaration)>(&VertexElements(0),InElements.Num());
}

FVertexDeclarationRHIRef FOpenGLDynamicRHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements)
{
	return new FOpenGLVertexDeclaration(Elements);
}

FVertexDeclarationRHIRef FOpenGLDynamicRHI::CreateVertexDeclaration(const FVertexDeclarationElementList& Elements, FName DeclName)
{
	return CreateVertexDeclaration(Elements);
}
