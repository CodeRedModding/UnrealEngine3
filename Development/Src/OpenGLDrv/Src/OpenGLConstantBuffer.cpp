/*=============================================================================
	OpenGLConstantBuffer.cpp: OpenGL Constant buffer RHI implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "OpenGLDrvPrivate.h"

FOpenGLConstantBuffer::FOpenGLConstantBuffer(UINT InSize,UINT InBaseOffset,UINT SubBuffers)
:	MaxSize(InSize)
,	BaseOffset(InBaseOffset)
,	bIsDirty(FALSE)
,	ShadowData(NULL)
,	CurrentSubBuffer(0)
,	NumSubBuffers(SubBuffers)
,	CurrentUpdateSize(0)
,	TotalUpdateSize(0)
{
	InitResource();
}

FOpenGLConstantBuffer::~FOpenGLConstantBuffer()
{
	ReleaseResource();
}

/**
* Creates a constant buffer on the device
*/
void FOpenGLConstantBuffer::InitDynamicRHI()
{
	ShadowData = new BYTE[MaxSize];
	appMemzero(ShadowData,MaxSize);

#if OPENGL_USE_BINDABLE_UNIFORMS
	Buffers = new TRefCountPtr<FOpenGLUniformBuffer>[NumSubBuffers];
	for(UINT Index = 0; Index < NumSubBuffers; Index++)
	{
		Buffers[Index] = new FOpenGLUniformBuffer(MaxSize, TRUE);
		Buffers[Index]->Update(ShadowData, 0, MaxSize, TRUE);
	}
#endif
}

void FOpenGLConstantBuffer::ReleaseDynamicRHI()
{
	if(ShadowData)
	{
		delete [] ShadowData;
	}

#if OPENGL_USE_BINDABLE_UNIFORMS
	if(Buffers)
	{
		for(UINT Index = 0; Index < NumSubBuffers; Index++)
		{
			delete Buffers[Index];
		}
	}
#endif
}

/**
* Updates a variable in the constant buffer.
* @param Data - The data to copy into the constant buffer
* @param Offset - The offset in the constant buffer to place the data at
* @param Size - The size of the data being copied
*/
void FOpenGLConstantBuffer::UpdateConstant(const BYTE* InData, WORD Offset, WORD InSize)
{
	bIsDirty = TRUE;
	check(BaseOffset <= Offset);
	Offset -= BaseOffset;
	appMemcpy(ShadowData+Offset, InData, InSize);
	CurrentUpdateSize = Max( (UINT)(Offset + InSize), CurrentUpdateSize );
}

UBOOL FOpenGLConstantBuffer::CommitConstantsToDevice(UBOOL bDiscardSharedConstants)
{
	if(bIsDirty)
	{
		SCOPE_CYCLE_COUNTER(STAT_OpenGLConstantBufferUpdateTime);

		if ( bDiscardSharedConstants )
		{
			// If we're discarding shared constants, just use constants that have been updated since the last Commit.
			TotalUpdateSize = CurrentUpdateSize;
		}
		else
		{
			// If we're re-using shared constants, use all constants.
			TotalUpdateSize = Max( CurrentUpdateSize, TotalUpdateSize );
		}

#if OPENGL_USE_BINDABLE_UNIFORMS
		CurrentSubBuffer++;
		if (CurrentSubBuffer == NumSubBuffers)
		{
			CurrentSubBuffer = 0;
		}

		Buffers[CurrentSubBuffer]->Update(ShadowData, 0, TotalUpdateSize, bDiscardSharedConstants);

		bIsDirty = FALSE;
		CurrentUpdateSize = 0;
#endif

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

void FOpenGLDynamicRHI::InitConstantBuffers()
{
	VSConstantBuffers.Empty(MAX_VS_CONSTANT_BUFFER_SLOTS);

	for(INT BufferIndex = 0;BufferIndex < MAX_VS_CONSTANT_BUFFER_SLOTS;BufferIndex++)
	{
		UINT Size=256 * sizeof(FVector4);
		UINT SubBuffers=1;
		UINT BaseOffset=0;
		if(BufferIndex == GLOBAL_CONSTANT_BUFFER)
		{
			BaseOffset = VS_GLOBAL_CONSTANT_BASE_INDEX * sizeof(FVector4);
		}
		else if(BufferIndex == VS_BONE_CONSTANT_BUFFER)
		{
			BaseOffset = BONE_CONSTANT_BASE_INDEX * sizeof(FVector4);
		}

		VSConstantBuffers.AddItem(new FOpenGLConstantBuffer(Size,BaseOffset,SubBuffers));
	}

	PSConstantBuffers.Empty(MAX_PS_CONSTANT_BUFFER_SLOTS);

	for(INT BufferIndex = 0;BufferIndex < MAX_PS_CONSTANT_BUFFER_SLOTS;BufferIndex++)
	{
		UINT Size=256 * sizeof(FVector4);
		UINT SubBuffers=1;
		UINT BaseOffset=0;
		if(BufferIndex == GLOBAL_CONSTANT_BUFFER)
		{
			BaseOffset = PS_GLOBAL_CONSTANT_BASE_INDEX * sizeof(FVector4);
		}

		PSConstantBuffers.AddItem(new FOpenGLConstantBuffer(Size,BaseOffset,SubBuffers));
	}
}
