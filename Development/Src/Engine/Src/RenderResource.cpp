/*=============================================================================
	RenderResource.cpp: Render resource implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

TLinkedList<FRenderResource*>*& FRenderResource::GetResourceList()
{
	static TLinkedList<FRenderResource*>* FirstResourceLink = NULL;
	return FirstResourceLink;
}

void FRenderResource::InitResource()
{
	check(IsInRenderingThread());
	if(!bInitialized)
	{
		ResourceLink = TLinkedList<FRenderResource*>(this);
		ResourceLink.Link(GetResourceList());
		if(GIsRHIInitialized)
		{
			InitDynamicRHI();
			InitRHI();
		}
		bInitialized = TRUE;
	}
}

void FRenderResource::ReleaseResource()
{
	if ( !GIsCriticalError )
	{
		check(IsInRenderingThread());
		if(bInitialized)
		{
			if(GIsRHIInitialized)
			{
				ReleaseRHI();
				ReleaseDynamicRHI();
			}
			ResourceLink.Unlink();
			bInitialized = FALSE;
		}
	}
}

void FRenderResource::UpdateRHI()
{
	check(IsInRenderingThread());
	if(bInitialized && GIsRHIInitialized)
	{
		ReleaseRHI();
		ReleaseDynamicRHI();
		InitDynamicRHI();
		InitRHI();
	}
}

FRenderResource::~FRenderResource()
{
	if (bInitialized && !GIsCriticalError)
	{
		// Deleting an initialized FRenderResource will result in a crash later since it is still linked
		appErrorf(TEXT("An FRenderResource was deleted without being released first!"));
	}
}

void BeginInitResource(FRenderResource* Resource)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		InitCommand,
		FRenderResource*,Resource,Resource,
		{
			Resource->InitResource();
		});
}

void BeginUpdateResourceRHI(FRenderResource* Resource)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		UpdateCommand,
		FRenderResource*,Resource,Resource,
		{
			Resource->UpdateRHI();
		});
}

void BeginReleaseResource(FRenderResource* Resource)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseCommand,
		FRenderResource*,Resource,Resource,
		{
			Resource->ReleaseResource();
		});
}

void ReleaseResourceAndFlush(FRenderResource* Resource)
{
	// Send the release message.
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		ReleaseCommand,
		FRenderResource*,Resource,Resource,
		{
			Resource->ReleaseResource();
		});

	FlushRenderingCommands();
}

/** The global null color vertex buffer, which is set with a stride of 0 on meshes without a color component. */
TGlobalResource<FNullColorVertexBuffer> GNullColorVertexBuffer;

/** The global null shadowmap vertex buffer, which is set with a stride of 0 when needed. */
TGlobalResource<FNullShadowmapVertexBuffer> GNullShadowmapVertexBuffer;
