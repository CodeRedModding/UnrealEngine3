/*=============================================================================
	UnContentStreaming.cpp: Implementation of content streaming classes.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Daniel Vogel
=============================================================================*/

#include "EnginePrivate.h"
#include "UnLinker.h"

/**
 * FStreamTextureInstance serialize operator.
 *
 * @param	Ar					Archive to to serialize object to/ from
 * @param	TextureInstance		Object to serialize
 * @return	Returns the archive passed in
 */
FArchive& operator<<( FArchive& Ar, FStreamableTextureInstance& TextureInstance )
{
	return Ar << TextureInstance.BoundingSphere << TextureInstance.TexelFactor;
}

/**
 * FStreamableTextureInfo serialize operator.
 *
 * @param	Ar					Archive to to serialize object to/ from
 * @param	TextureInfo			Object to serialize
 * @return	Returns the archive passed in
 */
FArchive& operator<<( FArchive& Ar, FStreamableTextureInfo& TextureInfo )
{
	return Ar << TextureInfo.Texture << TextureInfo.TextureInstances;
}

/**
 * Adds a streamer to the list of content streamers to route requests to.	
 *
 * @param Streamer	Streamer to add.
 */
void FStreamingManager::AddStreamer( FContentStreamer* Streamer )
{
	ContentStreamers.AddUniqueItem( Streamer );
}

/**
 * Removes a streamer from the list of content streamers to route requests to.	
 *
 * @param Streamer	Streamer to remove.
 */
void FStreamingManager::RemoveStreamer( FContentStreamer* Streamer )
{
	ContentStreamers.RemoveItem( Streamer );
}

/**
 * Returns whether any content streamer currently considers a passed in resource
 * for streaming.
 *
 * @param	Resource	resource to check for
 * @return	TRUE if a streamer is aware of resource, FALSE otherwise
 */
UBOOL FStreamingManager::IsResourceConsidered( FResource* Resource )
{
	UBOOL IsConsidered = 0;

	for( INT i=0; i<ContentStreamers.Num(); i++ )
	{
		IsConsidered |= ContentStreamers(i)->IsResourceConsidered( Resource );
		if( IsConsidered )
			break;
	}

	return IsConsidered;
}

/**
 * Take the current player/ context combination into account for streaming.
 *
 * @param Player	Player to get level from
 * @param Context	Scene context used to retrieve location
 */
void FStreamingManager::ProcessViewer(  ULocalPlayer* Player, FSceneContext* Context )
{
	FCycleCounterSection CycleCounter(GStreamingStats.ProcessViewerTime);
	if( GEngine->UseStreaming )
	{
		for( INT i=0; i<ContentStreamers.Num(); i++ )
		{
			ContentStreamers(i)->ProcessViewer( Player, Context );
		}
	}
}

/**
 * Tick function, called before Viewport->Draw.
 */
void FStreamingManager::Tick()
{
	FCycleCounterSection CycleCounter(GStreamingStats.TickTime);
	if( GEngine->UseStreaming )
	{
		for( INT i=0; i<ContentStreamers.Num(); i++ )
		{
			ContentStreamers(i)->Tick();
		}
	}
}

/**
 * Flushs resources from all streamers. Waits for all outstanding requests to be fulfilled and 
 * optionally also removes any pending requests.
 *
 * @param OnlyOutstanding	Whether to flush all resources or simply wait till outstanding requests are fulfilled
 */
void FStreamingManager::Flush( UBOOL OnlyOutstanding )
{
	GAsyncIOManager->CancelAllRequests();

	for( INT i=0; i<ContentStreamers.Num(); i++ )
	{
		ContentStreamers(i)->Flush( OnlyOutstanding );
	}
}

/**
 * Returns whether this content streamer currently considers a passed in resource
 * for streaming.
 *
 * @param	Resource	resource to check for
 * @return	TRUE if streamer is aware of resource, FALSE otherwise
 */
UBOOL FStaticTextureStreamer::IsResourceConsidered( FResource* Resource )
{
	FTextureBase* Texture = CastResource<FTextureBase>(Resource);

	if( Texture && PendingRequests.Find( Texture ) )
	{
		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Take the current player/ context combination into account for streaming.
 *
 * @param Player	Player to get level from
 * @param Context	Scene context used to retrieve location
 */
void FStaticTextureStreamer::ProcessViewer( ULocalPlayer* Player, FSceneContext* Context )
{
	check(Player->Actor->Level);
	ULevel* Level = Player->Actor->GetLevel();

	// The level most likely needs to be resaved.
	if( !Level->StaticStreamableTextureInfos.Num() )
		return;

	// Get location directly from scene context instead of eventPlayerViewpoint.
	FVector ViewLocation		= Context->View->ViewMatrix.Inverse().GetOrigin();
	UINT	MaxTextureSize		= GEngine->Client->GetRenderDevice()->MaxTextureSize;

	// The initial call to ProcessViewer needs to take all textures into account in order for IsResourceConsidered to work reliably.
	UINT	TexturesToConsider	= InitialProcessViewerCall ? Level->StaticStreamableTextureInfos.Num() : Max( 1, Level->StaticStreamableTextureInfos.Num() / 3 );
	InitialProcessViewerCall	= 0;

	// Iterate over a part of the StaticStreamableTextureInfos area. The data is structured so all objects using a certain texture are always being processed
	// in the same frame.
	for( UINT TextureInfoIndex=0; TextureInfoIndex<TexturesToConsider; TextureInfoIndex++ )
	{
		FStreamableTextureInfo& TextureInfo = Level->StaticStreamableTextureInfos((TextureInfoIndex + CurrentIndexOffset) % Level->StaticStreamableTextureInfos.Num());
		FTextureBase*			Texture		= TextureInfo.Texture->GetTexture();

		// Nothing to if there aren't enough miplevels as UEngine::PostEditChange flushes so there is never a case where CurrentMips < NumMips if NumMips <= MinStreamedInMips.
		if( Texture && (Texture->NumMips > GEngine->MinStreamedInMips) )
		{
			for( INT TextureInstanceIndex=0; TextureInstanceIndex<TextureInfo.TextureInstances.Num(); TextureInstanceIndex++ )
			{
				FStreamableTextureInstance& TextureInstance = TextureInfo.TextureInstances(TextureInstanceIndex);
			
				// Default to loading all miplevels.
				UINT	WantedMipCount			= Texture->NumMips;
				FLOAT	Distance				= FDist( ViewLocation, TextureInstance.BoundingSphere );

				// Calculated miplevel based on screen space size of bounding sphere unless we're actually inside the bounding sphere.			
				if( Distance > ( TextureInstance.BoundingSphere.W + 1.f) )
				{
					FLOAT	SilhouetteRadius	= TextureInstance.BoundingSphere.W * appInvSqrt( Square(Distance) - Square(TextureInstance.BoundingSphere.W) );				
					FLOAT	ScreenSpaceSizeX	= Context->View->ProjectionMatrix.M[0][0] * 2.f * SilhouetteRadius * Context->SizeX;	// Take FOV into account.
					FLOAT	ScreenSpaceSizeY	= Context->View->ProjectionMatrix.M[1][1] * 2.f * SilhouetteRadius * Context->SizeY;	// Take FOV into account.
					FLOAT	ScreenDimension		= TextureInstance.TexelFactor / (2.f * TextureInstance.BoundingSphere.W) * Max( ScreenSpaceSizeX, ScreenSpaceSizeY );

					// WantedMipCount is the number of mips so we need to adjust with "+ 1".
					WantedMipCount				= 1 + appCeilLogTwo( Min<UINT>( ScreenDimension, MaxTextureSize ) );
				}

				// We can't pull the Min into the Clamp as Texture->NumMips could be less than MinStreamedInMips!
				WantedMipCount = Min( Texture->NumMips, Clamp( WantedMipCount, GEngine->MinStreamedInMips, GEngine->MaxStreamedInMips ) );

				UINT* CurrentMipRequest = PendingRequests.Find( Texture );
				if( CurrentMipRequest )
				{
					// Only request mip increases as Tick automatically resets the WantedMipCont to MinStreamedInMips each frame by removing the texture from the map.
					*CurrentMipRequest = Max( *CurrentMipRequest, WantedMipCount );
				}
				else
				{
					PendingRequests.Set( Texture, WantedMipCount );
				}
			}
		}
	}

	CurrentIndexOffset = (CurrentIndexOffset + TexturesToConsider) % Level->StaticStreamableTextureInfos.Num();
}

/**
 * Flushs resources. Waits for all outstanding requests to be fulfilled and optionally also
 * removes any pending requests.
 *
 * @param OnlyOutstanding	Whether to flush all resources or simply wait till outstanding requests are fulfilled
 */
void FStaticTextureStreamer::Flush( UBOOL OnlyOutstanding )
{
	// Finalize all outstanding requests to give the D3D layer a chance to unlock the resource.
	for( TDynamicMap<FTextureBase*,FTextureMipRequest*>::TIterator It(OutstandingRequests); It; ++It )
	{
		FTextureMipRequest* TextureMipRequest = It.Value();
		GResourceManager->FinalizeMipRequest( TextureMipRequest );
	}

	if( !OnlyOutstanding )
	{
		// Also clean up pending requests, aka all textures being considered. Not needed when e.g. flushing D3D device
		// like it occurs on resolution change or initial creation.
		PendingRequests.Empty();
		InitialProcessViewerCall = 1;
	}
	OutstandingRequests.Empty();
}

/**
 * Tick function, called before Viewport->Draw.
 */
void FStaticTextureStreamer::Tick()
{
	BEGINCYCLECOUNTER(GStreamingStats.DataShuffleTime);
	// Iterate over all considered texture requests.
	for( TDynamicMap<FTextureBase*,UINT>::TIterator It(PendingRequests); It; ++It )
	{	
		FTextureBase*	Texture				= It.Key();
		UINT&			RequestedMips		= It.Value();
	
		// Find out whether we have a pending outstanding mip request for this texture...
		FTextureMipRequest* OutstandingMipRequest = OutstandingRequests.FindRef( Texture );
		if( OutstandingMipRequest )
		{
			// ... and if yes, whether it makes sense (and is possible) to cancel it.
			UBOOL ShouldCancelRequest	= (OutstandingMipRequest->RequestedMips > RequestedMips);
			// In which case we should see whether it is possible. @warning: the CurrentMips >= RequestedMips check is important!
			UBOOL CanceledRequest		= ShouldCancelRequest && (Texture->CurrentMips >= RequestedMips) && OutstandingMipRequest->Loader->CancelMipRequest( OutstandingMipRequest );

			if( CanceledRequest )
			{
				// We need to unlock the resource and clean up all assorted magic. The easiest way to do this is to just finalize 
				// the texture even though the larger miplevels now might contain garbage. This doesn't really matter as we only 
				// cancel requests where we can be sure that the existing non- garbage miplevels are sufficient for dropping 
				// miplevels and we drop miplevels in a blocking fashion right below.
				// @warning: this approach needs to be changed if we are going to use a non blocking way to lower mips!
				GResourceManager->FinalizeMipRequest( OutstandingMipRequest );
				OutstandingRequests.Remove( Texture );

				GStreamingStats.CanceledTextureRequests.Value++;
			}
			else
			{
				// Remove the texture from the map so it correctly sets a new minimum the next time it gets processed.
				It.RemoveCurrent();

				// Either request didn't need to be or couldn't be canceled.
				continue;
			}
		}

		if( RequestedMips != Texture->CurrentMips )
		{
			check( (RequestedMips >= GEngine->MinStreamedInMips) || (Texture->NumMips < GEngine->MinStreamedInMips) );
			check( RequestedMips <= Texture->NumMips );

			// Request a change in miplevels. A return value of NULL indicates that the request is already fulfilled
			// as no data needed to be loaded.
			FTextureMipRequest* TextureMipRequest = GResourceManager->RequestMips( Texture, RequestedMips );	

			if( TextureMipRequest )
			{
				// Request is still outstanding so we need to pass it to the resource loader.
				if( !GResourceLoader->LoadTextureMips( TextureMipRequest ) )
				{
					appErrorf(TEXT("Failed loading mips for texture %s"), *Texture->DescribeResource() );
				}
				OutstandingRequests.Set( Texture, TextureMipRequest );
			}
		}
	
		// Remove the texture from the map so it correctly sets a new minimum the next time it gets processed.
		It.RemoveCurrent();

		GStreamingStats.StreamedInTextures.Value++;
	}
	ENDCYCLECOUNTER;

	BEGINCYCLECOUNTER(GStreamingStats.FinalizeTime);
	// Iterate over all outstanding requests and finalize them if they are completed.
	for( TDynamicMap<FTextureBase*,FTextureMipRequest*>::TIterator It(OutstandingRequests); It; ++It )
	{
		FTextureMipRequest* TextureMipRequest = It.Value();
		if( TextureMipRequest->OutstandingMipRequests.GetValue() == 0 )
		{	
			GResourceManager->FinalizeMipRequest( TextureMipRequest );
			It.RemoveCurrent();
		}
		else
		{		
			GStreamingStats.OutstandingTextureRequests.Value++;
		}
	}
	ENDCYCLECOUNTER;
}

/**
 * Requests (potentially async) loading of miplevels.
 *
 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
 * @return	TRUE if this loader can handle the request, FALSE otherwise
 */
UBOOL FBlockingLoaderUnreal::LoadTextureMips( FTextureMipRequest* TextureMipRequest )
{
	check(TextureMipRequest);
	check(TextureMipRequest->Texture);

	UTexture2D*		Texture2D	= Cast<UTexture2D>(TextureMipRequest->Texture->GetUTexture());
	FTextureBase*	TextureBase = TextureMipRequest->Texture;

	if( Texture2D )
	{
		for( UINT MipIndex=0; MipIndex<ARRAY_COUNT(TextureMipRequest->Mips); MipIndex++ )
		{
			if( TextureMipRequest->Mips[MipIndex].Data )
			{
				// Load the miplevel directly into the destination pointer.
				TextureMipRequest->OutstandingMipRequests.Increment();
				TextureMipRequest->Loader = this;
				check( MipIndex < (UINT)Texture2D->Mips.Num() );
				Texture2D->Mips(MipIndex).Data.Load();
				check( (UINT)Texture2D->Mips(MipIndex).Data.Num() <= TextureMipRequest->Mips[MipIndex].Size );
				appMemcpy( TextureMipRequest->Mips[MipIndex].Data, Texture2D->Mips(MipIndex).Data.GetData(), Texture2D->Mips(MipIndex).Data.Num() );
				Texture2D->Mips(MipIndex).Data.Unload();
				TextureMipRequest->OutstandingMipRequests.Decrement();
			}
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Requests (potentially async) loading of miplevels.
 *
 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
 * @return	TRUE if this loader can handle the request, FALSE otherwise
 */
UBOOL FBackgroundLoaderUnreal::LoadTextureMips( FTextureMipRequest* TextureMipRequest )
{
	check(TextureMipRequest);
	check(TextureMipRequest->Texture);

	UTexture2D*		Texture2D	= Cast<UTexture2D>(TextureMipRequest->Texture->GetUTexture());
	FTextureBase*	TextureBase = TextureMipRequest->Texture;

	if( Texture2D )
	{
		for( UINT MipIndex=0; MipIndex<ARRAY_COUNT(TextureMipRequest->Mips); MipIndex++ )
		{
			if( TextureMipRequest->Mips[MipIndex].Data )
			{
				check( MipIndex < (UINT)Texture2D->Mips.Num() );

				// Keep track of loader as it's required for canceling a request.
				TextureMipRequest->Loader = this;

				// Pass the request on to the async io manager after increasing the request count.
				TextureMipRequest->OutstandingMipRequests.Increment();
				TextureMipRequest->Mips[MipIndex].IORequestIndex = GAsyncIOManager->LoadData( 
																		Texture2D->GetLinker()->Filename, 
																		Texture2D->Mips(MipIndex).Data.GetOffset(),
																		TextureMipRequest->Mips[MipIndex].Size,
																		TextureMipRequest->Mips[MipIndex].Data,
																		&TextureMipRequest->OutstandingMipRequests
																		);
			}
		}

		return 1;
	}
	else
	{
		return 0;
	}
}

/**
 * Requests the cancelation of a mip request if it isn't already in the process of being fulfilled.
 *
 * @param	TextureMipRequest	Mip request to cancel.
 * @return	TRUE if mip request was successfully canceled, FALSE if it is currently in the process of being 
 *			fulfilled and hence wasn't canceled.
 */
UBOOL FBackgroundLoaderUnreal::CancelMipRequest( FTextureMipRequest* TextureMipRequest )
{
	QWORD	IORequestIndices[ARRAY_COUNT(TextureMipRequest->Mips)];
	INT		NumIndices = 0;

	for( INT MipIndex=0; MipIndex<ARRAY_COUNT(TextureMipRequest->Mips); MipIndex++ )
	{
		if( TextureMipRequest->Mips[MipIndex].IORequestIndex )
		{
			IORequestIndices[NumIndices] = TextureMipRequest->Mips[MipIndex].IORequestIndex;
			NumIndices++;
		}
	}

	return GAsyncIOManager->CancelRequests( IORequestIndices, NumIndices );
}


/**
 * Requests (potentially async) loading of miplevels.
 *
 * @param	TextureMipRequest	Structure containing information about requested mips (e.g. where to load them to)
 * @return	TRUE if any loader can handle the request, FALSE otherwise
 */
UBOOL FResourceLoader::LoadTextureMips( FTextureMipRequest* TextureMipRequest )
{
	UBOOL Succeeded = 0;
	for( INT LoaderIndex=0; LoaderIndex<BackgroundLoaders.Num(); LoaderIndex++ )
	{
		FBackgroundLoader* BackgroundLoader = BackgroundLoaders(LoaderIndex);
		if( BackgroundLoader->LoadTextureMips( TextureMipRequest ) )
		{
			Succeeded = 1;
			break;
		}
	}
	return Succeeded;
}

/** 
 * Adds a loader to the array of loaders taken into account.
 *
 * @param	BackgroundLoader	Loader to add to list.
 */
void FResourceLoader::AddLoader( FBackgroundLoader* BackgroundLoader )
{
	BackgroundLoaders.AddUniqueItem( BackgroundLoader );
}

/** 
 * Removes a loader from the array of loaders taken into account.
 *
 * @param	BackgroundLoader	Loader to remove from list.
 */
void FResourceLoader::RemoveLoader( FBackgroundLoader* BackgroundLoader )
{
	BackgroundLoaders.RemoveItem( BackgroundLoader );
}

/**
 * Initializes critical section, event and other used variables. 
 *
 * This is called in the context of the thread object that aggregates 
 * this, not the thread that passes this runnable to a new thread.
 *
 * @return True if initialization was successful, false otherwise
 */
UBOOL FAsyncIOManagerWindows::Init()
{
	CriticalSection				= GSynchronizeFactory->CreateCriticalSection();
	OutstandingRequestsEvent	= GSynchronizeFactory->CreateSynchEvent();
	RequestIndex				= 1;
	IsRunning.Increment();
	return TRUE;
}

/**
 * Called in the context of the aggregating thread to perform cleanup.
 */
void FAsyncIOManagerWindows::Exit()
{
	for( TMap<FString,HANDLE>::TIterator It(NameToHandleMap); It; ++It )
	{
		CloseHandle( It.Value() );
	}
	NameToHandleMap.Empty();
	GSynchronizeFactory->Destroy( CriticalSection );
	GSynchronizeFactory->Destroy( OutstandingRequestsEvent );
}

/**
 * This is called if a thread is requested to terminate early
 */
void FAsyncIOManagerWindows::Stop()
{
	OutstandingRequestsEvent->Trigger();
	IsRunning.Decrement();
}

/**
 * This is where all the actual loading is done. This is only called
 * if the initialization was successful.
 *
 * @return always 0
 */
DWORD FAsyncIOManagerWindows::Run()
{
	// IsRunning gets decremented by Stop.
	while( IsRunning.GetValue() > 0 )
	{
		FAsyncIORequest IORequest = {0};
		{
			FScopeLock ScopeLock( CriticalSection );
			if( OutstandingRequests.Num() )
			{
				IORequest = OutstandingRequests(0);
				OutstandingRequests.Remove( 0 );
				BusyReading.Increment();	// We're busy reading. Updated inside scoped lock to ensure CancelAllRequests works correctly.
			}
			else
			{
				IORequest.FileHandle = NULL;
			}
		}

		if( IORequest.FileHandle )
		{
			//@warning: this code doesn't handle failure as it doesn't have a way to pass back the information.
			DWORD BytesRead;
			SetFilePointer( IORequest.FileHandle, IORequest.Offset, 0, FILE_BEGIN );
			ReadFile( IORequest.FileHandle, IORequest.Dest, IORequest.Size, &BytesRead, NULL );
			IORequest.Counter->Decrement(); // Request fulfilled.
			BusyReading.Decrement();		// We're done reading for now.
		}
		else
		{
			// Wait till the calling thread signals further work.
			OutstandingRequestsEvent->Wait();
		}
	}

	return 0;
}

/**
 * Requests data to be loaded async. Returns immediately.
 *
 * @param	Filename	Filename to load
 * @param	Offset		Offset into file
 * @param	Size		Size of load request
 * @param	Dest		Pointer to load data into
 * @param	Counter		Thread safe counter to decrement when loading has finished
 *
 * @return Returns an index to the request that can be used for canceling or 0 if the request failed.
 */
QWORD FAsyncIOManagerWindows::LoadData( FString Filename, UINT Offset, UINT Size, void* Dest, FThreadSafeCounter* Counter )
{
	HANDLE FileHandle = NameToHandleMap.FindRef( Filename );

	if( !FileHandle )
	{
#ifndef XBOX
		FileHandle = TCHAR_CALL_OS(
						CreateFileW( *Filename,						GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) ,
						CreateFileA( TCHAR_TO_ANSI(*Filename),		GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL ) );
#else
		FFilename CookedFilename = Filename;
		CookedFilename = CookedFilename.GetPath() + TEXT("\\") + CookedFilename.GetBaseFilename() + TEXT(".xxx");

		FileHandle =	CreateFileA( GetXenonFilename(*CookedFilename),	GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL );
#endif
	}

	if( FileHandle != INVALID_HANDLE_VALUE )
	{
		FScopeLock ScopeLock( CriticalSection );

		NameToHandleMap.Set( *Filename, FileHandle );

		FAsyncIORequest IORequest;

		IORequest.RequestIndex	= RequestIndex++;
		IORequest.FileHandle	= FileHandle;
		IORequest.Offset		= Offset;
		IORequest.Size			= Size;
		IORequest.Dest			= Dest;
		IORequest.Counter		= Counter;
	
		OutstandingRequests.AddItem( IORequest );

		// Trigger event telling IO thread to wake up to perform work.
		OutstandingRequestsEvent->Trigger();

		return IORequest.RequestIndex;
	}
	else
	{
		return 0;
	}
}

/**
 * Removes N outstanding requests from the queue in an atomic all or nothing operation.
 * NOTE: Requests are only canceled if ALL requests are still pending and neither one
 * is currently in flight.
 *
 * @param	RequestIndices	Indices of requests to cancel.
 * @return	TRUE if all requests were still outstanding, FALSE otherwise
 */
UBOOL FAsyncIOManagerWindows::CancelRequests( QWORD* RequestIndices, UINT NumIndices )
{
	FScopeLock ScopeLock( CriticalSection );
	
	UINT	NumFound			= 0;
	UINT*	OutstandingIndices	= (UINT*) appAlloca( sizeof(UINT) * NumIndices );

	for( UINT OutstandingIndex=0; OutstandingIndex<(UINT)OutstandingRequests.Num() && NumFound<NumIndices; OutstandingIndex++ )
	{
		for( UINT RequestIndex=0; RequestIndex<NumIndices; RequestIndex++ )
		{
			if( OutstandingRequests(OutstandingIndex).RequestIndex == RequestIndices[RequestIndex] )
			{
				OutstandingIndices[NumFound] = OutstandingIndex;
				NumFound++;
			}
		}
	}

	if( NumFound == NumIndices )
	{
		for( UINT RequestIndex=0; RequestIndex<NumFound; RequestIndex++ )
		{
			OutstandingRequests.Remove( OutstandingIndices[RequestIndex] - RequestIndex );
		}

		return TRUE;
	}
	else
	{
		return FALSE;
	}
}

/**
 * Blocks till all currently outstanding requests are canceled.
 */
void FAsyncIOManagerWindows::CancelAllRequests()
{
	// We need the scope lock here to ensure that BusyReading isn't being updated while we try to read from it.
	FScopeLock ScopeLock( CriticalSection );

	OutstandingRequests.Empty();

	while( BusyReading.GetValue() )
	{
		// We could probably use an event instead.
		Sleep(1);
	}
}

/** Global resource loader */
FResourceLoader*	GResourceLoader;
/** Global streaming manager */
FStreamingManager*	GStreamingManager;
/** Global async IO manager */
FAsyncIOManager*	GAsyncIOManager;

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
