/*=============================================================================
	UnResource.cpp: Resource manager.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "EnginePrivate.h"

//
//	FResourceManager::FResourceManager
//

FResourceManager::FResourceManager()
{
	NextResourceIndex = 0;
}

//
//	FResourceManager::RegisterClient
//

void FResourceManager::RegisterClient(FResourceClient* Client)
{
	Clients.AddItem(Client);
}

//
//	FResourceManager::DeregisterClient
//

void FResourceManager::DeregisterClient(FResourceClient* Client)
{
	Clients.RemoveItem(Client);
}

//
//	FResourceManager::AllocateResource
//

void FResourceManager::AllocateResource(FResource* Resource)
{
	check(Resource->ResourceIndex == INDEX_NONE);
	Resource->ResourceIndex = NextResourceIndex++;
}

//
//	FResourceManager::FreeResource
//

void FResourceManager::FreeResource(FResource* Resource)
{
	check( Resource->ResourceIndex != INDEX_NONE );
	check( !GStreamingManager->IsResourceConsidered( Resource ) );

	// Deregister the resource with all the resource clients.
	for(UINT ClientIndex = 0;ClientIndex < (UINT)Clients.Num();ClientIndex++)
		Clients(ClientIndex)->FreeResource(Resource);
	
	Resource->ResourceIndex = INDEX_NONE;
}

//
//	FResourceManager::UpdateResource
//

void FResourceManager::UpdateResource(FResource* Resource)
{
	check(Resource->ResourceIndex != INDEX_NONE);

	// Update the resource with all the resource clients.

	for(UINT ClientIndex = 0;ClientIndex < (UINT)Clients.Num();ClientIndex++)
		Clients(ClientIndex)->UpdateResource(Resource);
}

/**
 * Request an increase or decrease in the amount of miplevels a texture uses.
 *
 * @param	Texture				texture to adjust
 * @param	RequestedMips		miplevels the texture should have
 * @return	NULL if a decrease is requested, pointer to information about request otherwise (NOTE: pointer gets freed in FinalizeMipRequest)
 */
FTextureMipRequest* FResourceManager::RequestMips( FTextureBase* Texture, UINT RequestedMips )
{
	FTextureMipRequest* TextureMipRequest = NULL;

	if( RequestedMips > Texture->CurrentMips )
	{
		TextureMipRequest = new FTextureMipRequest;
		check( RequestedMips < ARRAY_COUNT( TextureMipRequest->Mips ) );
		appMemzero( TextureMipRequest, sizeof(FTextureMipRequest) );
		TextureMipRequest->Texture		 = Texture;
		TextureMipRequest->RequestedMips = RequestedMips;
	}

	if( Clients.Num() )
	{
		//@todo streaming: mip change requests only get routed to the first resource client
		Clients(0)->RequestMips( Texture, RequestedMips, TextureMipRequest );
		
		if( TextureMipRequest == NULL )
		{
			// Requested fewer miplevels in which case we're already done.
			Texture->CurrentMips = RequestedMips;
		}
	}

	return TextureMipRequest;		
}

/**
 * Finalizes a mip request. This gets called after all requested data has finished loading.	
 *
 * @param	TextureMipRequest	Mip request that is being finalized.
 */
void FResourceManager::FinalizeMipRequest( FTextureMipRequest* TextureMipRequest )
{
	if( Clients.Num() )
	{
		//@todo streaming: mip change requests only get routed to the first resource client
		Clients(0)->FinalizeMipRequest( TextureMipRequest );
		// We've just performed the switcheroo so update CurrentMips.
		TextureMipRequest->Texture->CurrentMips = TextureMipRequest->RequestedMips;
	}
	
	delete TextureMipRequest;
}


//
//	Globals.
//

FResourceManager*	GResourceManager;
FResourceType		FResource::StaticType(TEXT("FResource"),NULL);