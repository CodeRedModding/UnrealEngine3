/*=============================================================================
	D3D11VertexBuffer.cpp: D3D vertex buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

FVertexBufferRHIRef FD3D11DynamicRHI::CreateVertexBuffer(UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	// Explicitly check that the size is nonzero before allowing CreateVertexBuffer to opaquely fail.
	check(Size > 0);

	D3D11_BUFFER_DESC Desc;
	Desc.ByteWidth = Size;
	Desc.Usage = (InUsage & (RUF_AnyDynamic|RUF_SmallUpdate)) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
	Desc.CPUAccessFlags = (InUsage & (RUF_AnyDynamic|RUF_SmallUpdate)) ? D3D11_CPU_ACCESS_WRITE : 0;
	Desc.MiscFlags = 0;

	// If a resource array was provided for the resource, create the resource pre-populated
	D3D11_SUBRESOURCE_DATA InitData;
	D3D11_SUBRESOURCE_DATA* pInitData = NULL;
	if(ResourceArray)
	{
		check(Size == ResourceArray->GetResourceDataSize());
		InitData.pSysMem = ResourceArray->GetResourceData();
		InitData.SysMemPitch = Size;
		InitData.SysMemSlicePitch = 0;
		pInitData = &InitData;
	}

	TRefCountPtr<ID3D11Buffer> VertexBufferResource;
	VERIFYD3D11RESULT(Direct3DDevice->CreateBuffer(&Desc,pInitData,VertexBufferResource.GetInitReference()));

	if(ResourceArray)
	{
		// Discard the resource array's contents.
		ResourceArray->Discard();
	}

	return new FD3D11VertexBuffer(VertexBufferResource, InUsage);
}

void* FD3D11DynamicRHI::LockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI,UINT Offset,UINT Size,UBOOL bReadOnlyInsteadOfWriteOnly)
{
	DYNAMIC_CAST_D3D11RESOURCE(VertexBuffer,VertexBuffer);

	// Determine whether the vertex buffer is dynamic or not.
	D3D11_BUFFER_DESC Desc;
	VertexBuffer->Resource->GetDesc(&Desc);
	const UBOOL bIsDynamic = (Desc.Usage == D3D11_USAGE_DYNAMIC);

	FD3D11LockedKey LockedKey(VertexBuffer->Resource);
	FD3D11LockedData LockedData;

	if(bIsDynamic)
	{
		check(!bReadOnlyInsteadOfWriteOnly);

		// Small update usage indicates that the buffer should not be discarded.
		D3D11_MAP WriteMode = D3D11_MAP_WRITE_DISCARD;
		if ( VertexBuffer->Usage == RUF_SmallUpdate )
		{
			WriteMode = D3D11_MAP_WRITE_NO_OVERWRITE;
		}

		// If the buffer is dynamic, map its memory for writing.
		D3D11_MAPPED_SUBRESOURCE MappedSubresource;
		VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(VertexBuffer->Resource,0,WriteMode,0,&MappedSubresource));
		LockedData.Data = (BYTE*)MappedSubresource.pData;
		LockedData.Pitch = MappedSubresource.RowPitch;
	}
	else
	{
		if(bReadOnlyInsteadOfWriteOnly)
		{
			// If the static buffer is being locked for reading, create a staging buffer.
			D3D11_BUFFER_DESC StagingBufferDesc;
			StagingBufferDesc.ByteWidth = Size;
			StagingBufferDesc.Usage = D3D11_USAGE_STAGING;
			StagingBufferDesc.BindFlags = 0;
			StagingBufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			StagingBufferDesc.MiscFlags = 0;
			TRefCountPtr<ID3D11Buffer> StagingVertexBuffer;
			VERIFYD3D11RESULT(Direct3DDevice->CreateBuffer(&StagingBufferDesc,NULL,StagingVertexBuffer.GetInitReference()));
			LockedData.StagingResource = StagingVertexBuffer;

			// Copy the contents of the vertex buffer to the staging buffer.
			Direct3DDeviceIMContext->CopyResource(StagingVertexBuffer,VertexBuffer->Resource);

			// Map the staging buffer's memory for reading.
			D3D11_MAPPED_SUBRESOURCE MappedSubresource;
			VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(StagingVertexBuffer,0,D3D11_MAP_READ,0,&MappedSubresource));
			LockedData.Data = (BYTE*)MappedSubresource.pData;
			LockedData.Pitch = MappedSubresource.RowPitch;
		}
		else
		{
			// If the static buffer is being locked for writing, allocate memory for the contents to be written to.
			LockedData.Data = (BYTE*)appMalloc(Desc.ByteWidth);
			LockedData.Pitch = Desc.ByteWidth;
		}
	}

	// Add the lock to the lock map.
	OutstandingLocks.Set(LockedKey,LockedData);

	// Return the offset pointer
	return (void*)((BYTE*)LockedData.Data + Offset);
}

void FD3D11DynamicRHI::UnlockVertexBuffer(FVertexBufferRHIParamRef VertexBufferRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(VertexBuffer,VertexBuffer);

	// Determine whether the vertex buffer is dynamic or not.
	D3D11_BUFFER_DESC Desc;
	VertexBuffer->Resource->GetDesc(&Desc);
	const UBOOL bIsDynamic = (Desc.Usage == D3D11_USAGE_DYNAMIC);

	// Find the outstanding lock for this VB.
	FD3D11LockedKey LockedKey(VertexBuffer->Resource);
	FD3D11LockedData* LockedData = OutstandingLocks.Find(LockedKey);
	check(LockedData);

	if(bIsDynamic)
	{
		// If the VB is dynamic, its memory was mapped directly; unmap it.
		Direct3DDeviceIMContext->Unmap(VertexBuffer->Resource,0);
	}
	else
	{
		// If the static VB lock involved a staging resource, it was locked for writing.
		if(LockedData->StagingResource)
		{
			// Unmap the staging buffer's memory.
			ID3D11Buffer* StagingBuffer = (ID3D11Buffer*)LockedData->StagingResource.GetReference();
			Direct3DDeviceIMContext->Unmap(StagingBuffer,0);
		}
		else 
		{
			// Copy the contents of the temporary memory buffer allocated for writing into the VB.
			Direct3DDeviceIMContext->UpdateSubresource(VertexBuffer->Resource,LockedKey.Subresource,NULL,LockedData->Data,LockedData->Pitch,0);

			// Free the temporary memory buffer.
			appFree(LockedData->Data);
		}
	}

	// Remove the FD3D11LockedData from the lock map.
	// If the lock involved a staging resource, this releases it.
	OutstandingLocks.Remove(LockedKey);
}

/**
	* Checks if a vertex buffer is still in use by the GPU.
	* @param VertexBuffer - the RHI texture resource to check
	* @return TRUE if the texture is still in use by the GPU, otherwise FALSE
	*/
UBOOL FD3D11DynamicRHI::IsBusyVertexBuffer(FVertexBufferRHIParamRef VertexBuffer)
{
	//@TODO: Implement somehow! (could perhaps use D3D11_MAP_FLAG_DO_NOT_WAIT)
	return FALSE;
}
