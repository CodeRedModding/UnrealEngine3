/*=============================================================================
	OpenGLVertexBuffer.cpp: OpenGL vertex buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

FVertexBufferRHIRef FOpenGLDynamicRHI::CreateVertexBuffer(UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	const void *Data = NULL;

	// If a resource array was provided for the resource, create the resource pre-populated
	if(ResourceArray)
	{
		check(Size == ResourceArray->GetResourceDataSize());
		Data = ResourceArray->GetResourceData();
	}

	TRefCountPtr<FOpenGLVertexBuffer> VertexBuffer = new FOpenGLVertexBuffer(Size, (InUsage & RUF_AnyDynamic) != 0, Data);
	return VertexBuffer.GetReference();
}

void* FOpenGLDynamicRHI::LockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI,UINT Offset,UINT Size,UBOOL bReadOnlyInsteadOfWriteOnly)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexBuffer,VertexBuffer);
	return VertexBuffer->Lock(Offset, Size, bReadOnlyInsteadOfWriteOnly, VertexBuffer->IsDynamic() && !bReadOnlyInsteadOfWriteOnly);
}

void FOpenGLDynamicRHI::UnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(VertexBuffer,VertexBuffer);
	VertexBuffer->Unlock();
}

/**
 * Checks if a vertex buffer is still in use by the GPU.
 * @param VertexBuffer - the RHI texture resource to check
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL FOpenGLDynamicRHI::IsBusyVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
	//@todo opengl: Implement somehow! (could perhaps use fences)
	return FALSE;
}
