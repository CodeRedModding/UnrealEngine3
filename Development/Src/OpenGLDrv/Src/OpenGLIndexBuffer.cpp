/*=============================================================================
	OpenGLIndexBuffer.cpp: OpenGL Index buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

FIndexBufferRHIRef FOpenGLDynamicRHI::CreateIndexBuffer(UINT Stride,UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	const void *Data = NULL;

	// If a resource array was provided for the resource, create the resource pre-populated
	if(ResourceArray)
	{
		check(Size == ResourceArray->GetResourceDataSize());
		Data = ResourceArray->GetResourceData();
	}

	TRefCountPtr<FOpenGLIndexBuffer> IndexBuffer = new FOpenGLIndexBuffer(Size, InUsage & RUF_AnyDynamic, Data, (Stride == sizeof(DWORD)));
	return IndexBuffer.GetReference();
}

FIndexBufferRHIRef FOpenGLDynamicRHI::CreateInstancedIndexBuffer(UINT Stride,UINT Size,DWORD InUsage,UINT PreallocateInstanceCount,UINT& OutNumInstances)
{
	// PC never needs extra instances in the index buffer
	OutNumInstances = 1;
	return CreateIndexBuffer(Stride,Size,0,InUsage);
}

void* FOpenGLDynamicRHI::LockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI,UINT Offset,UINT Size)
{
	DYNAMIC_CAST_OPENGLRESOURCE(IndexBuffer,IndexBuffer);
	return IndexBuffer->Lock(Offset, Size, FALSE, IndexBuffer->IsDynamic());
}

void FOpenGLDynamicRHI::UnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	DYNAMIC_CAST_OPENGLRESOURCE(IndexBuffer,IndexBuffer);
	IndexBuffer->Unlock();
}

// This could be supported, but using it may cause performance problems.
FIndexBufferRHIRef FOpenGLDynamicRHI::CreateAliasedIndexBuffer(FVertexBufferRHIParamRef InBuffer, UINT InStride)
{
    check(0);
    return NULL;
}
