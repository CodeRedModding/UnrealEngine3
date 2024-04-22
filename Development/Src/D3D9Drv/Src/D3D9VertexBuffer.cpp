/*=============================================================================
	D3D9VertexBuffer.cpp: D3D vertex buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D9DrvPrivate.h"

FVertexBufferRHIRef FD3D9DynamicRHI::CreateVertexBuffer(UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	// Explicitly check that the size is nonzero before allowing CreateVertexBuffer to opaquely fail.
	check(Size > 0);

	// Determine the appropriate usage flags and pool for the resource.
	const DWORD Usage = (InUsage & RUF_AnyDynamic) ? (D3DUSAGE_DYNAMIC | D3DUSAGE_WRITEONLY) : 0;
	const D3DPOOL Pool = (InUsage & RUF_AnyDynamic) ? D3DPOOL_DEFAULT : D3DPOOL_MANAGED;

	// Create the vertex buffer.
	TRefCountPtr<FD3D9VertexBuffer> VertexBuffer;
	VERIFYD3D9RESULT(Direct3DDevice->CreateVertexBuffer(Size,Usage,0,Pool,(IDirect3DVertexBuffer9**)VertexBuffer.GetInitReference(),NULL));

	// If a resource array was provided for the resource, copy the contents into the buffer.
	if(ResourceArray)
	{
		// Initialize the buffer.
		void* Buffer = RHILockVertexBuffer(VertexBuffer,0,Size,FALSE);
		check(Buffer);
		check(Size == ResourceArray->GetResourceDataSize());
		appMemcpy(Buffer,ResourceArray->GetResourceData(),Size);
		RHIUnlockVertexBuffer(VertexBuffer);

		// Discard the resource array's contents.
		ResourceArray->Discard();
	}

	return VertexBuffer.GetReference();
}

void* FD3D9DynamicRHI::LockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI,UINT Offset,UINT Size,UBOOL bReadOnlyInsteadOfWriteOnly)
{
	DYNAMIC_CAST_D3D9RESOURCE(VertexBuffer,VertexBuffer);

	// Determine whether this is a static or dynamic VB.
	D3DVERTEXBUFFER_DESC VertexBufferDesc;
	VERIFYD3D9RESULT(VertexBuffer->GetDesc(&VertexBufferDesc));
	const UBOOL bIsDynamic = (VertexBufferDesc.Usage & D3DUSAGE_DYNAMIC) != 0;

	// For dynamic VBs, discard the previous contents before locking.
	const DWORD LockFlags = bIsDynamic ? D3DLOCK_DISCARD : (bReadOnlyInsteadOfWriteOnly ? D3DLOCK_READONLY : 0);

	// Lock the vertex buffer.
	void* Data = NULL;
	VERIFYD3D9RESULT(VertexBuffer->Lock(Offset,Size,&Data,LockFlags));
	return Data;
}

void FD3D9DynamicRHI::UnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	DYNAMIC_CAST_D3D9RESOURCE(VertexBuffer,VertexBuffer);
	VERIFYD3D9RESULT(VertexBuffer->Unlock());
}

/**
 * Checks if a vertex buffer is still in use by the GPU.
 * @param VertexBuffer - the RHI texture resource to check
 * @return TRUE if the texture is still in use by the GPU, otherwise FALSE
 */
UBOOL FD3D9DynamicRHI::IsBusyVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
	//@TODO: Implement somehow!
	return FALSE;
}

