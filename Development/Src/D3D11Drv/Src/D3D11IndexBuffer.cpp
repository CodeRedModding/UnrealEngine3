/*=============================================================================
	D3D11IndexBuffer.cpp: D3D Index buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "D3D11DrvPrivate.h"

FIndexBufferRHIRef FD3D11DynamicRHI::CreateIndexBuffer(UINT Stride,UINT Size,FResourceArrayInterface* ResourceArray,DWORD InUsage)
{
	// Explicitly check that the size is nonzero before allowing CreateIndexBuffer to opaquely fail.
	check(Size > 0);

	// Describe the index buffer.
	D3D11_BUFFER_DESC Desc;
	Desc.ByteWidth = Size;
	Desc.Usage = (InUsage & (RUF_AnyDynamic|RUF_SmallUpdate)) ? D3D11_USAGE_DYNAMIC : D3D11_USAGE_DEFAULT;
	Desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
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

	TRefCountPtr<ID3D11Buffer> IndexBufferResource;
	VERIFYD3D11RESULT(Direct3DDevice->CreateBuffer(&Desc,pInitData,IndexBufferResource.GetInitReference()));

	if(ResourceArray)
	{
		// Discard the resource array's contents.
		ResourceArray->Discard();
	}

	return new FD3D11IndexBuffer(IndexBufferResource, Stride, InUsage);
}

FIndexBufferRHIRef FD3D11DynamicRHI::CreateInstancedIndexBuffer(UINT Stride,UINT Size,DWORD InUsage,UINT PreallocateInstanceCount,UINT& OutNumInstances)
{
	// PC never needs extra instances in the index buffer
	OutNumInstances = 1;
	return CreateIndexBuffer(Stride,Size,0,InUsage);
}

void* FD3D11DynamicRHI::LockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI,UINT Offset,UINT Size)
{
	DYNAMIC_CAST_D3D11RESOURCE(IndexBuffer,IndexBuffer);

	D3D11_BUFFER_DESC Desc;
	IndexBuffer->Resource->GetDesc(&Desc);
	void* Data = NULL;

	if(Desc.Usage == D3D11_USAGE_DEFAULT)
	{
		FD3D11LockedKey LockedKey(IndexBuffer->Resource);
		FD3D11LockedData LockedData;
		LockedData.Data = (BYTE*)appMalloc(Desc.ByteWidth);
		LockedData.Pitch = Desc.ByteWidth;
		Data = LockedData.Data;

		OutstandingLocks.Set(LockedKey,LockedData);
	}
	else
	{
		// Small update usage indicates that the buffer should not be discarded.
		D3D11_MAP WriteMode = D3D11_MAP_WRITE_DISCARD;
		if ( IndexBuffer->Usage == RUF_SmallUpdate )
		{
			WriteMode = D3D11_MAP_WRITE_NO_OVERWRITE;
		}

		D3D11_MAPPED_SUBRESOURCE mappedSubresource;
		VERIFYD3D11RESULT(Direct3DDeviceIMContext->Map(IndexBuffer->Resource,0,WriteMode,0,&mappedSubresource));
		Data = mappedSubresource.pData;
	}

	// Return the offset pointer
	return (void*)((BYTE*)Data + Offset);
}

void FD3D11DynamicRHI::UnlockIndexBuffer(FIndexBufferRHIParamRef IndexBufferRHI)
{
	DYNAMIC_CAST_D3D11RESOURCE(IndexBuffer,IndexBuffer);

	D3D11_BUFFER_DESC Desc;
	IndexBuffer->Resource->GetDesc(&Desc);

	if(Desc.Usage == D3D11_USAGE_DEFAULT)
	{
		// Find the lock
		FD3D11LockedKey LockedKey(IndexBuffer->Resource);
		FD3D11LockedData* LockedData = OutstandingLocks.Find(LockedKey);

		// Update the buffer
		Direct3DDeviceIMContext->UpdateSubresource(IndexBuffer->Resource,LockedKey.Subresource,NULL,LockedData->Data,LockedData->Pitch,0);
		appFree(LockedData->Data);

		// remove the FD3D11LockedData from the lock list
		OutstandingLocks.Remove(LockedKey);
	}
	else
	{
		Direct3DDeviceIMContext->Unmap(IndexBuffer->Resource,0);
	}
}

FIndexBufferRHIRef FD3D11DynamicRHI::CreateAliasedIndexBuffer(FVertexBufferRHIParamRef InBuffer, UINT InStride)
{
	check(0);
	return NULL;
}
