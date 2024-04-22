/*=============================================================================
	TextureMovie.cpp: UTextureMovie implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/*-----------------------------------------------------------------------------
UTextureMovie
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTextureMovie);

/**
* Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
* asynchronous cleanup process.
*
* We need to ensure that the decoder doesn't have any references to the movie texture resource before destructing it.
*/
void UTextureMovie::BeginDestroy()
{
	// this will also release the FTextureMovieResource
	Super::BeginDestroy();

	if( Decoder )
	{
		// close the movie
		Decoder->Close();
		// FTextureMovieResource no longer exists and is not trying to use the decoder 
		// to update itself on the render thread so it should be safe to set this
		Decoder = NULL;
	}

	// synchronize with the rendering thread by inserting a fence
 	if( !ReleaseCodecFence )
 	{
 		ReleaseCodecFence = new FRenderCommandFence();
 	}
 	ReleaseCodecFence->BeginFence();
}
/**
* Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
* potentially asynchronous object cleanup.
*
* @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
*/
UBOOL UTextureMovie::IsReadyForFinishDestroy()
{
	// ready to call FinishDestroy if the codec flushing fence has been hit
	return( 
		Super::IsReadyForFinishDestroy() &&
		ReleaseCodecFence && 
		ReleaseCodecFence->GetNumPendingFences() == 0 
		);
}

/**
* Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
*
* note: because ExitProperties() is called here, Super::FinishDestroy() should always be called at the end of your child class's
* FinishDestroy() method, rather than at the beginning.
*/
void UTextureMovie::FinishDestroy()
{
	delete ReleaseCodecFence;
	ReleaseCodecFence = NULL;
	
	Super::FinishDestroy();
}

/**
* Called when a property on this object has been modified externally
*
* @param PropertyAboutToChange the property that will be modified
*/
void UTextureMovie::PreEditChange(UProperty* PropertyAboutToChange)
{
	// this will release the FTextureMovieResource
	Super::PreEditChange(PropertyAboutToChange);

	// synchronize with the rendering thread by flushing all render commands
	FlushRenderingCommands();

	if( Decoder )
	{
		// close the movie
		Decoder->Close();
		// FTextureMovieResource no longer exists and is not trying to use the decoder 
		// to update itself on the render thread so it should be safe to set this
		Decoder = NULL;
	}
}

/**
* Called when a property on this object has been modified externally
*
* @param PropertyThatChanged the property that was modified
*/
void UTextureMovie::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	// reinit the decoder
	InitDecoder();	

	// Update the internal size and format with what the decoder provides. 
	// This is not necessarily a power of two.	
	SizeX	= Decoder->GetSizeX();
	SizeY	= Decoder->GetSizeY();
	Format	= Decoder->GetFormat();

	// Non power of two textures need to use clamping.
	if( SizeX & (SizeX - 1) ||
		SizeY & (SizeY - 1) )
	{
		AddressX = TA_Clamp;
		AddressY = TA_Clamp;
	}

	// this will recreate the FTextureMovieResource
	Super::PostEditChangeProperty(PropertyChangedEvent);
    
	if( AutoPlay )
	{
		// begin movie playback if AutoPlay is enabled
		Play();
	}
	else
	{
		// begin movie playback so that we have at least one frame from the movie
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			PauseCommand,
			UCodecMovie*,Decoder,Decoder,
		{
			Decoder->Play(FALSE,TRUE,TRUE);
		});
		Paused = TRUE;
	}
}

/**
* Postload initialization of movie texture. Creates decoder object and retriever first frame.
*/
void UTextureMovie::PostLoad()
{
	Super::PostLoad();

	if ( !HasAnyFlags(RF_ClassDefaultObject) && !GIsBuildMachine)  // we won't initialize this on build machines
	{
		// create the decoder and verify the movie stream
		InitDecoder();

		// Update the internal size and format with what the decoder provides. 
		// This is not necessarily a power of two.		
		SizeX	= Decoder->GetSizeX();
		SizeY	= Decoder->GetSizeY();
		Format	= Decoder->GetFormat();

		// Non power of two textures need to use clamping.
		if( SizeX & (SizeX - 1) ||
			SizeY & (SizeY - 1) )
		{
			AddressX = TA_Clamp;
			AddressY = TA_Clamp;
		}

		// recreate the FTextureMovieResource
		UpdateResource();
		
		if( AutoPlay )
		{
			// begin movie playback if AutoPlay is enabled
			Play();
		}
		else
		{			
			// begin movie playback so that we have at least one frame from the movie
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				PauseCommand,
				UCodecMovie*,Decoder,Decoder,
			{
				Decoder->Play(FALSE,TRUE,TRUE);
			});
			Paused = TRUE;
		}
	}
}

/**
* Serializes the compressed movie data.
*
* @param Ar	FArchive to serialize Data with.
*/
void UTextureMovie::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	Data.Serialize( Ar, this );
}

/** 
* Creates a new codec and checks to see if it has a valid stream.
*/
void UTextureMovie::InitDecoder()
{
	check(Decoder == NULL);

	if( DecoderClass )
	{
		// Constructs a new decoder of the appropriate class.
		// The decoder class is set by the UTextureMovieFactory during import based on the movie file type
		Decoder = ConstructObject<UCodecMovie>( DecoderClass );
	}

	UBOOL bIsStreamValid=FALSE;
	if( Decoder )
	{				
#if 0	//@todo bink - implement stream from file
		// Newly imported objects won't have a linker till they are saved so we have to play from memory. We also play
		// from memory in the Editor to avoid having an open file handle to an existing package.
		if(!GIsEditor && 
			GetLinker() && 
			MovieStreamSource == MovieStream_File )		
		{
			// Have the decoder open the file itself and stream from disk.
			bIsStreamValid = Decoder->Open( GetLinker()->Filename, Data.GetBulkDataOffsetInFile(), Data.GetBulkDataSize() );
		}
		else
#endif
		{
			// allocate memory and copy movie stream into it.
			// decoder is responsible for free'ing this memory on Close.
			void* CopyOfData = NULL;
			Data.GetCopy( &CopyOfData, TRUE );
			// open the movie stream for decoding
			if( Decoder->Open( CopyOfData, Data.GetBulkDataSize() ) )
			{
				bIsStreamValid = TRUE;
			}
			else
			{
				bIsStreamValid = FALSE;
				// free copy of data since we own it now
				appFree( CopyOfData );
			}
		}
	}

	// The engine always expects a valid decoder so we're going to revert to a fallback decoder
	// if we couldn't associate a decoder with this stream.
	if( !bIsStreamValid )
	{
		debugf( NAME_Warning, TEXT("Invalid movie stream data for %s"), *GetFullName() );
		Decoder = ConstructObject<UCodecMovieFallback>( UCodecMovieFallback::StaticClass() );
		verify( Decoder->Open( NULL, 0 ) );
	}
}

/**
 * Thumbnail description
 *
 * @return info string about the movie
 */
FString UTextureMovie::GetDesc()
{
	if (Decoder)
	{
		return FString::Printf( 
			TEXT("%dx%d [%s], %.1f FPS, %.1f sec"), 
			SizeX, 
			SizeY, 
			GPixelFormats[Format].Name, 
			Decoder->GetFrameRate(), 
			Decoder->GetDuration() 
			);
	}
	else
	{
		return FString();
	}
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UTextureMovie::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeY );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		break;
	case 2:
		Description = FString::Printf( TEXT( "%.1f fps" ), Decoder->GetFrameRate() );
		break;
	case 3:
		Description = FString::Printf( TEXT( "%.1f seconds" ), Decoder->GetDuration() );
		break;
	}
	return( Description );
}

/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UTextureMovie::GetResourceSize()
{
	INT ResourceSize = 0;
	if (!GExclusiveResourceSizeMode)
	{	
		// Object overhead.
		FArchiveCountMem CountBytesSize( this );
		ResourceSize += CountBytesSize.GetNum();
	}
	// Movie bulk data size.
	ResourceSize += Data.GetBulkDataSize();
	// Cost of render targets.
	ResourceSize += SizeX * SizeY * 4;
	return ResourceSize;
}

/**
* Plays the movie and also unpauses.
*/
void UTextureMovie::Play()
{
	check(Decoder);

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		PauseCommand,
		UCodecMovie*,Decoder,Decoder,
		UBOOL,Looping,Looping,
		UBOOL,ResetOnLastFrame,ResetOnLastFrame,
	{
		Decoder->Play(Looping,FALSE,ResetOnLastFrame);
	});

	Paused = FALSE;
	Stopped = FALSE;
}

/**
* Pauses the movie.
*/
void UTextureMovie::Pause()
{
	if (Decoder)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			PauseCommand,
			UCodecMovie*,Decoder,Decoder,
		{
			Decoder->Pause(TRUE);
		});
	}
	Paused = TRUE;
}

/**
* Stops movie playback.
*/
void UTextureMovie::Stop()
{
	if (Decoder)	
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			PauseCommand,
			UCodecMovie*,Decoder,Decoder,
		{
			Decoder->Stop();
			Decoder->ResetStream();
		});
	}
	Stopped	= TRUE;
}

/**
* Create a new movie texture resource
*
* @return newly created FTextureMovieResource
*/
FTextureResource* UTextureMovie::CreateResource()
{
	FTextureMovieResource* Result = new FTextureMovieResource(this);
	return Result;
}

/**
* Access the movie target resource for this movie texture object
*
* @return pointer to resource or NULL if not initialized
*/
class FTextureMovieResource* UTextureMovie::GetTextureMovieResource()
{
	FTextureMovieResource* Result = NULL;
	if( Resource &&
		Resource->IsInitialized() )
	{
		Result = (FTextureMovieResource*)Resource;
	}
	return Result;
}

/**
* Materials should treat a movie texture like a regular 2D texture resource.
*
* @return EMaterialValueType for this resource
*/
EMaterialValueType UTextureMovie::GetMaterialType()
{
	return MCT_Texture2D;    
}


/*-----------------------------------------------------------------------------
FTextureMovieResource
-----------------------------------------------------------------------------*/

/**
* Initializes the dynamic RHI resource and/or RHI render target used by this resource.
* Called when the resource is initialized, or when reseting all RHI resources.
* This is only called by the rendering thread.
*/
void FTextureMovieResource::InitDynamicRHI()
{
	if( Owner->SizeX > 0 && Owner->SizeY > 0 )
	{
		// Create the RHI texture. Only one mip is used and the texture is targetablef or resolve.
		DWORD TexCreateFlags = Owner->SRGB ? TexCreate_SRGB : 0;
		Texture2DRHI = RHICreateTexture2D(
			Owner->SizeX, 
			Owner->SizeY, 
			Owner->Format, 
			1,
			TexCreate_ResolveTargetable|TexCreateFlags,
			NULL);
		TextureRHI = (FTextureRHIRef&)Texture2DRHI;

		// Create the RHI target surface used for rendering to
		RenderTargetSurfaceRHI = RHICreateTargetableSurface(
			Owner->SizeX,
			Owner->SizeY,
			Owner->Format,
			Texture2DRHI,
			0,
			TEXT("AuxColor")
			);

		// add to the list of global deferred updates (updated during scene rendering)
		// since the latest decoded movie frame is rendered to this movie texture target
		AddToDeferredUpdateList();
	}

	// Create the sampler state RHI resource.
	FSamplerStateInitializerRHI SamplerStateInitializer
	(
		GSystemSettings.TextureLODSettings.GetSamplerFilter( Owner ),
		Owner->AddressX == TA_Wrap ? AM_Wrap : (Owner->AddressX == TA_Clamp ? AM_Clamp : AM_Mirror),
		Owner->AddressY == TA_Wrap ? AM_Wrap : (Owner->AddressY == TA_Clamp ? AM_Clamp : AM_Mirror),
		AM_Wrap
	);
	SamplerStateRHI = RHICreateSamplerState( SamplerStateInitializer );
}

/**
* Release the dynamic RHI resource and/or RHI render target used by this resource.
* Called when the resource is released, or when reseting all RHI resources.
* This is only called by the rendering thread.
*/
void FTextureMovieResource::ReleaseDynamicRHI()
{
	// release the FTexture RHI resources here as well
	ReleaseRHI();

	Texture2DRHI.SafeRelease();
	RenderTargetSurfaceRHI.SafeRelease();	

	// remove from global list of deferred updates
	RemoveFromDeferredUpdateList();

	if( Owner->Decoder )
	{
		// release any dynamnic resources allocated by the decoder
		Owner->Decoder->ReleaseDynamicResources();
	}
}

/**
* Decodes the next frame of the movie stream and renders the result to this movie texture target
* This is only called by the rendering thread.
*/
void FTextureMovieResource::UpdateResource()
{
	if( Owner->Decoder )
	{
		SCOPED_DRAW_EVENT(EventDecode)(DEC_SCENE_ITEMS,TEXT("Movie[%s]"),*Owner->GetName());
		Owner->Decoder->GetFrame(this);
	}
}

/**
* @return width of the target
*/
UINT FTextureMovieResource::GetSizeX() const
{
	return Owner->SizeX;
}

/**
* @return height of the target
*/
UINT FTextureMovieResource::GetSizeY() const
{
	return Owner->SizeY;
}
