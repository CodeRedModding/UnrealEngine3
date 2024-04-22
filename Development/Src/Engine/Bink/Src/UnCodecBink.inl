/*=============================================================================
	UnCodecBink.inl: Bink movie codec implementation (inline)
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#if USE_BINK_CODEC

// link with Bink libs
//#pragma message("Linking Bink Libs...")
#if XBOX
	#if 0	// FINAL_RELEASE
		#pragma comment(lib, "binkxenon.LTCG.lib")
	#else
		#pragma comment(lib, "binkxenon.lib")
	#endif
#elif defined(_WINDOWS_)
	#if _WIN64
		#if BINKUDK
			#pragma comment(lib, "binkudkw64.lib")
		#else
			#pragma comment(lib, "binkw64.lib")
		#endif
	#else
		#if BINKUDK
			#pragma comment(lib, "binkudk.lib")
		#else
			#pragma comment(lib, "binkw32.lib")
		#endif
	#endif
#endif

// include internal frame buffer texture support and YUV->RGB shaders
#if PS3
#include "BinkTexturesPS3.inl"
#else
#include "BinkTexturesRHI.inl"
#endif

// bink shaders used by the UCodecMovieBink instances
UCodecMovieBink::FInternalBinkShaders UCodecMovieBink::BinkShaders;

/*-----------------------------------------------------------------------------
UCodecMovieBink
-----------------------------------------------------------------------------*/

/**
* Constructor
*/
UCodecMovieBink::UCodecMovieBink()
:	Bink(NULL)
,	FileHandle(NULL)
,	MemorySource(NULL)
,	bPauseNextFrame(FALSE)
,	bIsLooping(FALSE)
,	DestroyFence(NULL)
{	
}

/** 
* Creates the internal bink frame buffer textures
*/
void UCodecMovieBink::FInternalBinkTextures::Init(HBINK Bink)
{
	if( !bInitialized )
	{
		check(Bink);

		// allocate the texture set info and zero it
		TextureSet = new FBinkTextureSet;
		appMemzero(&TextureSet->bink_buffers,sizeof(TextureSet->bink_buffers));
		appMemzero(TextureSet->textures,BINKMAXFRAMEBUFFERS*sizeof(BINKFRAMETEXTURES));
		// get information about the YUV frame buffers required by Bink
		BinkGetFrameBuffersInfo( Bink, &TextureSet->bink_buffers );
		// create the YUV frame buffers required for decoding by Bink
		CreateBinkTextures( TextureSet );
		bInitialized=TRUE;
	}
	
}

/**
* Clears the internal bink frame buffer textures
*/
void UCodecMovieBink::FInternalBinkTextures::Clear()
{
	if( bInitialized )
	{
#if USE_ASYNC_BINK
		// make sure bink is completed
		BinkDoFrameAsyncWait(Bink, -1);
#endif

		FreeBinkTextures( TextureSet );
		delete TextureSet;
		TextureSet = NULL;
		bInitialized=FALSE;
	}
}

/** 
* Creates the internal bink shaders
*/
void UCodecMovieBink::FInternalBinkShaders::Init()
{
	if( !bInitialized )
	{
		// shaders for converting from YUV(+Alpha) to RGB(+Alpha)
		CreateBinkShaders();
		bInitialized=TRUE;
	}
}

/**
* Frees the internal bink shaders
*/
void UCodecMovieBink::FInternalBinkShaders::Clear()
{
	if( bInitialized )
	{
		FreeBinkShaders();
		bInitialized=FALSE;
	}
}

/**
* Do the decoding for the next movie frame
*/
void UCodecMovieBink::DecodeFrame()
{
	if( Bink && 
		!BinkWait(Bink))
	{
		if( bIsLooping || GetBinkInfo()->FrameNum < GetBinkInfo()->Frames )
		{
#if USE_ASYNC_BINK
			// make sure bink is completed
			BinkDoFrameAsyncWait(Bink, -1);
#endif
			// initialize internal bink shaders for rendering
			BinkShaders.Init();	
			// initialize internal bink textures
			BinkTextures.Init(Bink);
			// lock bink buffer textures to decode into
			LockBinkTextures( BinkTextures.TextureSet );
			// register locked buffer texture ptrs 
			BinkRegisterFrameBuffers( Bink, &BinkTextures.TextureSet->bink_buffers );
			// wait for the GPU to finish with the previous frame textures
			WaitForBinkTextures( BinkTextures.TextureSet );
			// decode the current movie frame to the buffer textures and swap to the next ones
#if USE_ASYNC_BINK
			// we always use just one thread for movie textures, as the game is running now and we don't want to hurt other threads
			BinkDoFrameAsync(Bink, GAsyncBinkThread1, GAsyncBinkThread1);
#else
			BinkDoFrame( Bink );
#endif
			// skip frames if necessary to catch up to audio (not needed really)
			if( BinkShouldSkip( Bink ) )
			{
				BinkNextFrame( Bink );
#if USE_ASYNC_BINK
				BinkDoFrameAsync(Bink, GAsyncBinkThread1, GAsyncBinkThread1);
				BinkDoFrameAsyncWait(Bink, -1);
#else
				BinkDoFrame( Bink );
#endif
			}
			// unlock bink buffer textures so that we can render with them
			UnlockBinkTextures( BinkTextures.TextureSet, Bink );

			if( bPauseNextFrame )
			{
				BinkPause(Bink,TRUE);
				bPauseNextFrame = FALSE;
			}
			else
			{
				// increment to the next frame in the movie stream
				BinkNextFrame( Bink );
			}
		}
		else if (bResetOnLastFrame)
		{
			// back to first frame and pause
			BinkGoto(Bink,1,0);
			BinkPause(Bink,TRUE);
		}
	}
}

/**
* Render the current movie frame to the target movie texture 
*
* @param InTextureMovieResource - output from movie decoding is written to this resource
*/
void UCodecMovieBink::RenderFrame( class FTextureMovieResource* InTextureMovieResource )
{
	if( InTextureMovieResource &&
		InTextureMovieResource->IsInitialized() )
	{
		if( !Bink ||
			!BinkTextures.bInitialized )
		{
			// render the last decoded frame to the texture target
			RHISetRenderTarget( InTextureMovieResource->GetRenderTargetSurface(), FSurfaceRHIRef() );
			RHIClear( TRUE, FLinearColor::Black, FALSE, 0, FALSE, 0 );
			// copy to texture
			RHICopyToResolveTarget( InTextureMovieResource->GetRenderTargetSurface(), FALSE, FResolveParams() );
		}
		else
		{
			// render the last decoded frame to the texture target
			RHISetRenderTarget( InTextureMovieResource->GetRenderTargetSurface(), FSurfaceRHIRef() );
			// use the YUV textures from the last decoded frame
			DrawBinkTextures(
				BinkTextures.TextureSet,
				Bink->Width, Bink->Height,
				-1.0f,
				+1.0f,
				+2.0f / (FLOAT)InTextureMovieResource->GetSizeX(),
				-2.0f / (FLOAT)InTextureMovieResource->GetSizeY(),
				1.0f,
				TRUE,
				FALSE
				);
			// copy to texture
			RHICopyToResolveTarget( InTextureMovieResource->GetRenderTargetSurface(), FALSE, FResolveParams() );
		}		
	}
}

/**
* Not all codec implementations are available
* (This is only called by the game thread)
*
* @return TRUE if the current codec is supported
*/
UBOOL UCodecMovieBink::IsSupported()
{
	return TRUE;
}

/**
* Returns the movie width.
* (This is only called by the game thread)
*
* @return width of movie.
*/
UINT UCodecMovieBink::GetSizeX()
{
	UINT Result=1;
	if( Bink )
	{
		Result = GetBinkInfo()->Width;
	}
	return Result;
}

/**
* Returns the movie height.
* (This is only called by the game thread)
*
* @return height of movie.
*/
UINT UCodecMovieBink::GetSizeY()
{
	UINT Result=1;
	if( Bink )
	{
		Result = GetBinkInfo()->Height;
	}
	return Result;
}

/** 
* Returns the movie format, in this case PF_A8R8G8B8
* (This is only called by the game thread)
*
* @return format of movie (always PF_A8R8G8B8)
*/
EPixelFormat UCodecMovieBink::GetFormat()
{
	return PF_A8R8G8B8;
}

/**
* Initializes the decoder to stream from disk.
* (This is only called by the game thread)
*
* @param	Filename	Filename of compressed media.
* @param	Offset		Offset into file to look for the beginning of the compressed data.
* @param	Size		Size of compressed data.
*
* @return	TRUE if initialization was successful, FALSE otherwise.
*/
UBOOL UCodecMovieBink::Open( const FString& Filename, DWORD Offset, DWORD Size )
{
	UBOOL Result=FALSE;
	if( Bink )
	{
		// make sure we close an existing stream
		Close();
	}

	//@todo bink - not implemented
	PlaybackDuration	= 1.f;

	return Result;
}

/**
* Initializes the decoder to stream from memory.
* (This is only called by the game thread)
*
* @param	Source		Beginning of memory block holding compressed data.
* @param	Size		Size of memory block.
*
* @return	TRUE if initialization was successful, FALSE otherwise.
*/
UBOOL UCodecMovieBink::Open( void* Source, DWORD Size )
{
	UBOOL Result=FALSE;

	if( Bink )
	{
		// make sure we close an existing stream
		Close();
	}

	if( Source )
	{
		// keep track of the externally allocated memory
		MemorySource = (BYTE*)Source;
		// turn off sound playback (has to preceed BinkOpen)
		BinkSetSoundTrack( 0, NULL );
		// open the movie stream from memory for decoding
		Bink = BinkOpen( (const char*)MemorySource, BINKFROMMEMORY|BINKNOFRAMEBUFFERS|BINKALPHA );
		// we have a valid movie stream 
		if( Bink )
		{
			Result = TRUE;

			// keep track of total playback time in seconds
			PlaybackDuration = GetBinkInfo()->Frames / GetFrameRate();
		}
	}	
	return Result;
}

/**
* Tears down stream, closing file handle if there was an open one.
* (This is only called by the game thread)
*/
void UCodecMovieBink::Close()
{
	if( Bink )
	{
		// close the decoder stream
		BinkClose( Bink );
		Bink = NULL;
	}

	// delete the YUV decoding textures
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FreeInternal,
		UCodecMovieBink*,CodecMovieBink,this,
	{
		CodecMovieBink->BinkTextures.Clear();
	});
	
	// if the file was opened from memory then free the memory
	if( MemorySource )
	{
		appFree( MemorySource );
		MemorySource = NULL;
	}

	PlaybackDuration = 0.f;
}

/**
* Resets the stream to its initial state so it can be played again from the beginning.
*/
void UCodecMovieBink::ResetStream()
{
	// go back to the first movie frame 
	if( Bink )
	{
		BinkGoto(Bink,1,0);
	}
}

/**
* Returns the framerate the movie was encoded at.
* (This is only called by the game thread)
*
* @return (FPS) frames per second for the movie stream 
*/
FLOAT UCodecMovieBink::GetFrameRate()
{
	FLOAT Result=0.0f;
	if( Bink )
	{
		checkSlow( GetBinkInfo()->FrameRateDiv > 0 );
		Result = GetBinkInfo()->FrameRate / GetBinkInfo()->FrameRateDiv;
	}
	return Result;
}

/**
* Queues the request to retrieve the next frame.
*
* @param InTextureMovieResource - output from movie decoding is written to this resource
*/
void UCodecMovieBink::GetFrame( class FTextureMovieResource* InTextureMovieResource )
{
	DecodeFrame();
	RenderFrame(InTextureMovieResource);
}

/** 
* Begin playback of the movie stream 
* (This is only called by the rendering thread)
*
* @param bLooping - if TRUE then the movie loops back to the start when finished
* @param bOneFrameOnly - if TRUE then the decoding is paused after the first frame is processed 
* @param bResetOnLastFrame - if TRUE then the movie frame is set to 0 when playback finishes
*/
void UCodecMovieBink::Play(UBOOL bLooping, UBOOL bOneFrameOnly, UBOOL bInResetOnLastFrame)
{
	if( Bink )
	{
		// resume playback of a paused stream
		BinkPause(Bink,FALSE);
        // request to only process one frame and pause afterwards
		bPauseNextFrame = bOneFrameOnly;

		bIsLooping = bLooping;
		bResetOnLastFrame = bInResetOnLastFrame;
	}	
}

/** 
* Pause or resume the movie playback.
* (This is only called by the rendering thread)
*
* @param bPause - if TRUE then decoding will be paused otherwise it resumes
*/
void UCodecMovieBink::Pause(UBOOL bPause)
{
	if( Bink )
	{
		BinkPause( Bink, bPause );
	}
}

/**
* Stop playback fromt he movie stream
* (This is only called by the rendering thread)
*/ 
void UCodecMovieBink::Stop()
{
	if( Bink )
	{
		BinkPause( Bink, TRUE );
	}
}

/**
* Release any dynamic rendering resources created by this codec
* (This is only called by the rendering thread)
*/
void UCodecMovieBink::ReleaseDynamicResources()
{
	BinkTextures.Clear();
	ResetStream();
}

/**
* Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
* asynchronous cleanup process.
*
* We need to ensure that the decoder doesn't have any references to the movie texture resource before destructing it.
*/
void UCodecMovieBink::BeginDestroy()
{
	Super::BeginDestroy();

	// delete the YUV decoding textures
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		FreeInternal,
		UCodecMovieBink*,CodecMovieBink,this,
	{
		CodecMovieBink->BinkTextures.Clear();
	});

	// synchronize with the rendering thread by inserting a fence
	if( !DestroyFence )
	{
		DestroyFence = new FRenderCommandFence();
	}
	DestroyFence->BeginFence();
}

/**
* Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
* potentially asynchronous object cleanup.
* @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
*/
UBOOL UCodecMovieBink::IsReadyForFinishDestroy()
{
	// ready to call FinishDestroy if the DestroyFence has been hit
	return( 
		Super::IsReadyForFinishDestroy() &&
		DestroyFence && 
		DestroyFence->GetNumPendingFences() == 0 
		);
}

/**
* Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
*
* note: because ExitProperties() is called here, Super::FinishDestroy() should always be called at the end of your child class's
* FinishDestroy() method, rather than at the beginning.
*/
void UCodecMovieBink::FinishDestroy()
{
	delete DestroyFence;
	DestroyFence = NULL;

	Super::FinishDestroy();
}


#endif //USE_BINK_CODEC

