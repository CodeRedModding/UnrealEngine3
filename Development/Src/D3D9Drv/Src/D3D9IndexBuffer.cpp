/*=============================================================================
	D3D9IndexBuffer.cpp: D3D Index buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

FIndexBufferRHIRef FD3D9DynamicRHI::CreateIndexBuffer(UINT Stride,UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	// Explicitly check that the size is nonzero before allowing CreateIndexBuffer to opaquely fail.
	check(Size > 0);

	// Determine the appropriate usage flags, pool and format for the resource.
	const DWORD Usage = (InUsage & RUF_AnyDynamic) ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : 0;
	const D3DPOOL Pool = (InUsage & RUF_AnyDynamic) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;
	const D3DFORMAT Format = (Stride == sizeof(WORD) ? D3DFMT_INDEX16 : D3DFMT_INDEX32);

	// Create the index buffer.
	TRefCountPtr<FD3D9IndexBuffer> IndexBuffer;
	VERIFYD3D9RESULT(Direct3DDevice->CreateIndexBuffer(Size,Usage,Format,Pool,(IDirect3DIndexBuffer9**)IndexBuffer.GetInitReference(),NULL));

	// If a resource array was provided for the resource, copy the contents into the buffer.
	if(ResourceArray)
	{
		// Initialize the buffer.
		void* Buffer = RHILockIndexBuffer(IndexBuffer,0,Size);
		check(Buffer);
		check(Size == ResourceArray->GetResourceDataSize());
		appMemcpy(Buffer,ResourceArray->GetResourceData(),Size);
		RHIUnlockIndexBuffer(IndexBuffer);

		// Discard the resource array's contents.
		ResourceArray->Discard();
	}

	return IndexBuffer.GetReference();
}

FIndexBufferRHIRef FD3D9DynamicRHI::CreateInstancedIndexBuffer(UINT Stride,UINT Size,DWORD InUsage,UINT PreallocateInstanceCount,UINT& OutNumInstances)
{
	// PC never needs extra instances in the index buffer
	OutNumInstances = 1;
	return CreateIndexBuffer(Stride,Size,0,InUsage);
}

void* FD3D9DynamicRHI::LockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI,UINT Offset,UINT Size)
{
	DYNAMIC_CAST_D3D9RESOURCE(IndexBuffer,IndexBuffer);

	// Determine whether this is a static or dynamic IB.
	D3DINDEXBUFFER_DESC IndexBufferDesc;
	VERIFYD3D9RESULT(IndexBuffer->GetDesc(&IndexBufferDesc));
	const UBOOL bIsDynamic = (IndexBufferDesc.Usage & D3DUSAGE_DYNAMIC) != 0;

	// For dynamic IBs, discard the previous contents before locking.
	const DWORD LockFlags = bIsDynamic ? D3DLOCK_DISCARD : 0;

	// Lock the index buffer.
	void* Data = NULL;
	VERIFYD3D9RESULT(IndexBuffer->Lock(Offset,Size,&Data,LockFlags));
	return Data;
}

void FD3D9DynamicRHI::UnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(IndexBuffer,IndexBuffer);
	VERIFYD3D9RESULT(IndexBuffer->Unlock());
}

FIndexBufferRHIRef FD3D9DynamicRHI::CreateAliasedIndexBuffer(FVertexBufferRHIParamRef InBuffer, UINT InStride)
{
    check(0);
    return NULL;
}
