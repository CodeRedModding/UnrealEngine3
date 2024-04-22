/*=============================================================================
	UnCodecs.cpp: Movie codec implementations
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnCodecs.h"

IMPLEMENT_CLASS(UCodecMovie);
IMPLEMENT_CLASS(UCodecMovieFallback);
IMPLEMENT_CLASS(UCodecMovieBink);

#if USE_BINK_CODEC
	#include "../Bink/Src/UnCodecBink.inl"
#endif

/*-----------------------------------------------------------------------------
	UCodecMovie
-----------------------------------------------------------------------------*/

/** 
* Returns the movie format.
*
* @return format of movie.
*/
EPixelFormat UCodecMovie::GetFormat()
{
	return PF_Unknown;
}

/*-----------------------------------------------------------------------------
UCodecMovieFallback
-----------------------------------------------------------------------------*/

/**
* Not all codec implementations are available
* @return TRUE if the current codec is supported
*/
UBOOL UCodecMovieFallback::IsSupported()
{
	return TRUE;
}

/**
 * Returns the movie width.
 *
 * @return width of movie.
 */
UINT UCodecMovieFallback::GetSizeX()
{
	return 1;
}

/**
 * Returns the movie height.
 *
 * @return height of movie.
 */
UINT UCodecMovieFallback::GetSizeY()
{
	return 1;
}

/** 
 * Returns the movie format, in this case PF_A8R8G8B8
 *
 * @return format of movie (always PF_A8R8G8B8)
 */
EPixelFormat UCodecMovieFallback::GetFormat()
{
	return PF_A8R8G8B8;
}

/**
 * Initializes the decoder to stream from disk.
 *
 * @param	Filename	unused
 * @param	Offset		unused
 * @param	Size		unused.
 *
 * @return	TRUE if initialization was successful, FALSE otherwise.
 */
UBOOL UCodecMovieFallback::Open( const FString& /*Filename*/, DWORD /*Offset*/, DWORD /*Size*/ )
{
	PlaybackDuration	= 1.f;
	CurrentTime			= 0;
	return TRUE;
}

/**
 * Initializes the decoder to stream from memory.
 *
 * @param	Source		unused
 * @param	Size		unused
 *
 * @return	TRUE if initialization was successful, FALSE otherwise.
 */
UBOOL UCodecMovieFallback::Open( void* /*Source*/, DWORD /*Size*/ )
{
	PlaybackDuration	= 1.f;
	CurrentTime			= 0;
	return TRUE;
}

/**
 * Resets the stream to its initial state so it can be played again from the beginning.
 */
void UCodecMovieFallback::ResetStream()
{
	CurrentTime = 0.f;
}

/**
 * Returns the framerate the movie was encoded at.
 *
 * @return framerate the movie was encoded at.
 */
FLOAT UCodecMovieFallback::GetFrameRate()
{
	return 30.f;
}

/**
* Queues the request to retrieve the next frame.
*
* @param InTextureMovieResource - output from movie decoding is written to this resource
*/
void UCodecMovieFallback::GetFrame( class FTextureMovieResource* InTextureMovieResource )
{
	CurrentTime += 1.f / GetFrameRate();
	if( CurrentTime > PlaybackDuration )
	{
		CurrentTime = 0.f;
	}	
	if( InTextureMovieResource &&
		InTextureMovieResource->IsInitialized() )
	{
		FLinearColor ClearColor(1.f,CurrentTime/PlaybackDuration,0.f,1.f);
		RHISetRenderTarget(InTextureMovieResource->GetRenderTargetSurface(),FSurfaceRHIRef());
		RHIClear(TRUE,ClearColor,FALSE,0.f,FALSE,0);
		RHICopyToResolveTarget(InTextureMovieResource->GetRenderTargetSurface(),FALSE,FResolveParams());
	}
}


