/**********************************************************************

Filename    :   ScaleformFullscreenMovie.h
Content     :

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#ifndef GFxFullscreenMovie_h
#define GFxFullscreenMovie_h

#if WITH_GFx

#include "FullScreenMovie.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_Types.h"
#include "Kernel/SF_RefCount.h"
#include "GFx/GFx_Player.h"
#include "GFx/GFx_Loader.h"
#include "Kernel/SF_Threads.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

#if WITH_GFx_FULLSCREEN_MOVIE

namespace Scaleform
{
namespace Render
{
namespace RHI
{
class HAL;
}
}
}

using namespace Scaleform;

class FFullScreenMovieGFx : public FFullScreenMovieSupport
{
		Ptr<Render::Renderer2D> Renderer2D;
		Ptr<Render::RHI::HAL>   RenderHal;
		UBOOL                   EngineInitialized;
		UBOOL                   FontlibInitialized;
		FString                 MovieFilename;
		Ptr<GFx::MovieDef>      MovieDef;
		Ptr<GFx::Movie>         MovieView;
		GFx::MovieDisplayHandle MovieDispHandle;
		Mutex                   PlayerMutex;
		AtomicInt<Scaleform::SInt32>       DoStop;
		AtomicInt<Scaleform::SInt32>       IsStopped;
		WaitCondition           MovieFinished;
		float                   LastAdvanceTime, NextAdvanceTime;
		UBOOL                   MovieHidden;
		FViewport*              Viewport;

		Ptr<GFx::FSCommandHandler>  FsCommandHandler;
		Ptr<GFx::ExternalInterface> ExternalInterface;

		friend class FGFxFSMFSCommandHandler;

	public:

		/**
		* Perform one-time initialization and create instance
		*
		* @param bUseSound - TRUE if sound should be enabled for movie playback
		* @return new instance if successful
		*/
		static FFullScreenMovieSupport* StaticInitialize ( UBOOL bUseSound );

		/**
		* Perform one-time shutdown
		*
		*/
		static void Shutdown();

		// FTickableObject interface

		/**
		* Pure virtual that must be overloaded by the inheriting class. It will
		* be called from within UnLevTick.cpp after ticking all actors.
		*
		* @param DeltaTime	Game time passed since the last call.
		*/
		virtual void Tick ( FLOAT DeltaTime );

		/**
		* Pure virtual that must be overloaded by the inheriting class. It is
		* used to determine whether an object is ready to be ticked. This is
		* required for example for all UObject derived classes as they might be
		* loaded async and therefore won't be ready immediately.
		*
		* @return	TRUE if class is ready to be ticked, FALSE otherwise.
		*/
		virtual UBOOL IsTickable() const;

		// FFullScreenMovieSupport interface

		/**
		* Kick off a movie play from the game thread
		*
		* @param InMovieMode How to play the movie (usually MM_PlayOnceFromStream or MM_LoopFromMemory).
		* @param MovieFilename Path of the movie to play in its entirety
		* @param StartFrame Optional frame number to start on
		*/
		virtual void GameThreadPlayMovie ( EMovieMode InMovieMode, const TCHAR* MovieFilename, INT StartFrame = 0, INT InStartOfRenderingMovieFrame = -1, INT InEndOfRenderingMovieFrame = -1 );

		/**
		* Stops the currently playing movie
		*
		* @param DelayInSeconds Will delay the stopping of the movie for this many seconds. If zero, this function will wait until the movie stops before returning.
		* @param bWaitForMovie if TRUE then wait until the movie finish event triggers
		* @param bForceStop if TRUE then non-skippable movies and startup movies are forced to stop
		*/
		virtual void GameThreadStopMovie ( FLOAT DelayInSeconds = 0.0f, UBOOL bWaitForMovie = TRUE, UBOOL bForceStop = FALSE );

		/**
		* Block game thread until movie is complete (must have been started
		* with GameThreadPlayMovie or it may never return)
		*/
		virtual void GameThreadWaitForMovie();

		/**
		* Checks to see if the movie has finished playing. Will return immediately
		*
		* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
		*
		* @return TRUE if the named movie has finished playing
		*/
		virtual UBOOL GameThreadIsMovieFinished ( const TCHAR* MovieFilename );

		/**
		* Checks to see if the movie is playing. Will return immediately
		*
		* @param MovieFilename MovieFilename to check against (should match the name passed to GameThreadPlayMovie). Empty string will match any movie
		*
		* @return TRUE if the named movie is playing
		*/
		virtual UBOOL GameThreadIsMoviePlaying ( const TCHAR* MovieFilename );

		/**
		* Get the name of the most recent movie played
		*
		* @return Name of the movie that was most recently played, or empty string if a movie hadn't been played
		*/
		virtual FString GameThreadGetLastMovieName();

		/**
		* Kicks off a thread to control the startup movie sequence
		*/
		virtual void GameThreadInitiateStartupSequence();

		/**
		* Returns the current frame number of the movie (not thred synchronized in anyway, but it's okay
		* if it's a little off
		*/
		virtual INT GameThreadGetCurrentFrame();

		/**
		* Tells the movie player to allow game rendering for a frame while still keeping the movie
		* playing. This will cover delays from render caching the first frame
		*/
		virtual void GameThreadRequestDelayedStopMovie();

		/**
		* Controls whether the movie player processes input.
		*
		* @param	bShouldMovieProcessInput	whether the movie should process input.
		*/
		virtual void GameThreadToggleInputProcessing ( UBOOL bShouldMovieProcessInput )
		{
			/* this movie player doesn't process input */
		}

		/**
		* Controls whether the movie  is hidden and if input will forcibly stop the movie from playing when hidden/
		*
		* @param	bHidden	whether the movie should be hidden
		*/
		virtual void GameThreadSetMovieHidden ( UBOOL bInHidden );

		/**
		* Releases any dynamic resources. This is needed for flushing resources during device reset on d3d.
		*/
		virtual void ReleaseDynamicResources();

		UBOOL InputKey ( FViewport* Viewport, INT ControllerId, FName Key, EInputEvent EventType, FLOAT AmountDepressed, UBOOL bGamepad );

		~FFullScreenMovieGFx();
	protected:

		/**
		* Constructor
		*/
		FFullScreenMovieGFx ( UBOOL bUseSound = TRUE );

		void Initialize();
};

#endif // WITH_GFx_FULLSCREEN_MOVIE

#endif // WITH_GFx

#endif
