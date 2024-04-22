/**********************************************************************

Filename    :   ScaleformFullscreenMovie.cpp
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

#include "GFxUI.h"


#if WITH_GFx

#include "ScaleformEngine.h"
#include "ScaleformStats.h"
#include "ScaleformAllocator.h"
#include "ScaleformFullscreenMovie.h"
#include "../../Engine/Src/SceneRenderTargets.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "Kernel/SF_SysFile.h"
#include "GFx/GFx_ImageResource.h"
#include "GFx/GFx_Log.h"
#include "Render/RHI_HAL.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

class FGFxFSMFSCommandHandler : public GFx::FSCommandHandler
{
	public:
		// Callback through which the movie sends messages to the game
		virtual void Callback ( GFx::Movie* pmovie, const char* pcommand, const char* parg )
		{
#if WITH_GFx_FULLSCREEN_MOVIE
			check ( pmovie );
			if ( pmovie->GetUserData() )
			{
				FFullScreenMovieGFx* Player = ( FFullScreenMovieGFx* ) pmovie->GetUserData();
				if ( !strcmp ( pcommand, "stopMovie" ) )
				{
					Player->DoStop = 1;
				}
			}
#endif
		}
};

static const int RTTempTextureCounts[] = {0, 24, 8, 8};

#if WITH_GFx_FULLSCREEN_MOVIE

FFullScreenMovieGFx::FFullScreenMovieGFx ( UBOOL bUseSound /*=TRUE*/ ) : Viewport ( NULL )
{
	EngineInitialized = FALSE;
	FontlibInitialized = FALSE;
	FsCommandHandler = *SF_NEW FGFxFSMFSCommandHandler();

	DoStop = 2;
	GGFxEngine->FullscreenMovie = this;
}

static FFullScreenMovieGFx* StaticInstance = NULL;

FFullScreenMovieSupport* FFullScreenMovieGFx::StaticInitialize ( UBOOL bUseSound )
{
	if ( !StaticInstance )
	{
		FGFxEngine::GetEngineNoRender();
		StaticInstance = new FFullScreenMovieGFx ( bUseSound );
	}
	return StaticInstance;
}

FFullScreenMovieGFx::~FFullScreenMovieGFx()
{
	Renderer2D = 0;
	RenderHal = 0;
}

void FFullScreenMovieGFx::Shutdown()
{
	if ( StaticInstance )
	{
		if ( IsInGameThread() )
		{
			StaticInstance->GameThreadStopMovie ( 0, true, true );
			StaticInstance->MovieView = NULL;
			StaticInstance->MovieDef = NULL;
		}
		else if ( IsInRenderingThread() )
		{
			delete StaticInstance;
			StaticInstance = NULL;
		}
	}
}

UBOOL FFullScreenMovieGFx::IsTickable() const
{
	return ( IsStopped == 0 );
}

void FFullScreenMovieGFx::Tick ( FLOAT DeltaTime )
{
	UBOOL bisDrawing = RHIIsDrawingViewport();

	if ( DoStop >= ( GGFxEngine->GameHasRendered > 1 ? 1 : 2 ) )
	{
		PlayerMutex.DoLock();
		IsStopped = TRUE;
		MovieFinished.NotifyAll();
		PlayerMutex.Unlock();
		return;
	}

	if ( Viewport == NULL || ( ( GFx::Movie* ) MovieView == NULL ) || RenderHal->GetDefaultRenderTarget() == NULL )
	{
		return;
	}

	if ( !bisDrawing )
	{
		Viewport->BeginRenderFrame();
		Renderer2D->BeginFrame();
	}

	float CurTime = appSeconds();
	if ( NextAdvanceTime > CurTime )
	{
		appSleep ( NextAdvanceTime - CurTime );
		CurTime = appSeconds();
	}
	NextAdvanceTime = CurTime + MovieView->Advance ( CurTime - LastAdvanceTime, 0 );
	if ( !MovieHidden )
	{
		if ( MovieDispHandle.NextCapture ( Renderer2D->GetContextNotify() ) )
		{
			bool hasViewport = MovieDispHandle.GetRenderEntry()->HasViewport();
			if ( hasViewport )
			{
				const Render::Viewport &vp3d = MovieDispHandle.GetRenderEntry()->GetDisplayData()->VP;
				Render::Rect<int> viewRect ( vp3d.Left, vp3d.Top, vp3d.Left + vp3d.Width, vp3d.Top + vp3d.Height );
				RenderHal->SetFullViewRect ( viewRect );
			}
			Renderer2D->Display ( MovieDispHandle );
		}
	}

	if ( !bisDrawing )
	{
		Renderer2D->EndFrame();
		Viewport->EndRenderFrame ( TRUE, TRUE );
	}
}

void FFullScreenMovieGFx::GameThreadPlayMovie ( EMovieMode InMovieMode, const TCHAR* InMovieFilename, INT StartFrame,
        INT InStartOfRenderingMovieFrame, INT InEndOfRenderingMovieFrame )
{
#if !CONSOLE
	if ( GIsGame )
	{
		extern void appHideSplash();
		extern void appShowGameWindow();
		appHideSplash();
		appShowGameWindow();
	}
#endif

	if ( !EngineInitialized )
	{
		FGFxEngine::GetEngine();
		Renderer2D = GGFxEngine->GetRenderer2D();
		RenderHal = GGFxEngine->GetRenderHAL();
		EngineInitialized = TRUE;
	}

	GameThreadStopMovie ( 0, TRUE, TRUE );

	if ( GEngine == NULL || GEngine->GameViewport == NULL )
	{
		return;
	}
	FViewport* Viewport = GEngine->GameViewport->Viewport;

	unsigned   loadConstants = GFx::Loader::LoadAll | GFx::Loader::LoadWaitCompletion;
	FString    PkgPath = FString ( TEXT ( "/ package/" ) ) + InMovieFilename;
	USwfMovie* pmovieInfo = LoadObject<USwfMovie> ( NULL, InMovieFilename, NULL, LOAD_None, NULL );

	FGFxEngine::ReplaceCharsInFString ( PkgPath, TEXT ( "." ), TEXT ( '/' ) );

#ifndef GFX_NO_LOCALIZATION
	if ( !FontlibInitialized )
	{
		// Fontlib must be configured before attempting to load movies that use it.
		if ( pmovieInfo && pmovieInfo->bUsesFontlib )
		{
			GGFxEngine->InitFontlib();
			FontlibInitialized = TRUE;
		}
	}
#endif

	Ptr<GFx::MovieDef> NewMovieDef = *GGFxEngine->GetLoader()->CreateMovie ( FTCHARToUTF8 ( *PkgPath ), loadConstants );
	if ( !NewMovieDef )
	{
		return;
	}

	Ptr<GFx::Movie> NewMovieView = *NewMovieDef->CreateInstance ( 1 );
	NewMovieView->SetViewport ( Viewport->GetSizeX(), Viewport->GetSizeY(),
	                            0, 0, Viewport->GetSizeX(), Viewport->GetSizeY(), Render::RHI::Viewport_NoGamma );
	NewMovieView->SetUserData ( this );
	NewMovieView->SetFSCommandHandler ( FsCommandHandler );

	if ( StartFrame )
	{
		NewMovieView->Advance ( StartFrame / NewMovieDef->GetFrameRate() );
	}

	DoStop = 0;
	IsStopped = FALSE;
	MovieHidden = 0;
	MovieFilename = InMovieFilename;

	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER ( FFullScreenMovieGFxPlayer,
	        FFullScreenMovieGFx*, Player, this, Ptr<GFx::Movie>, MovieView, NewMovieView, Ptr<GFx::MovieDef>, NewMovieDef, NewMovieDef,
	{
		// ensure depth buffer is large enough
		GSceneRenderTargets.Allocate ( GEngine->GameViewport->Viewport->GetSizeX(), GEngine->GameViewport->Viewport->GetSizeY() );

		// MovieView is being passed to the render thread, which will call capture.
		// Notify it here, so that it won't think the threading access in invalid.
		MovieView->SetCaptureThread ( Scaleform::GetCurrentThreadId() );

		Player->MovieDef = NewMovieDef;
		Player->MovieView = MovieView;
		Player->LastAdvanceTime = appSeconds();
		Player->NextAdvanceTime = 0;
		Player->Viewport = GEngine->GameViewport->Viewport;
		Player->MovieDispHandle = MovieView->GetDisplayHandle();
	} );
}

void FFullScreenMovieGFx::GameThreadStopMovie ( FLOAT DelayInSeconds, UBOOL bWaitForMovie, UBOOL bForceStop )
{
	DoStop = 2;
	if ( bWaitForMovie )
	{
		GameThreadWaitForMovie();
	}
}

void FFullScreenMovieGFx::GameThreadRequestDelayedStopMovie()
{
	GGFxEngine->GameHasRendered = 0;
	DoStop = 1;
}

void FFullScreenMovieGFx::GameThreadWaitForMovie()
{
	PlayerMutex.DoLock();
	if ( MovieView )
	{
		MovieFinished.Wait ( &PlayerMutex );
		MovieView->SetCaptureThread ( Scaleform::GetCurrentThreadId() );
	}
	PlayerMutex.Unlock();
	MovieDef = 0;
	MovieView = 0;
	MovieDispHandle = 0;
}

UBOOL FFullScreenMovieGFx::GameThreadIsMovieFinished ( const TCHAR* InMovieFilename )
{
	return !GameThreadIsMoviePlaying ( InMovieFilename );
}

UBOOL FFullScreenMovieGFx::GameThreadIsMoviePlaying ( const TCHAR* InMovieFilename )
{
	// Empty string will match any movie
	UBOOL FilenameMatch = appStricmp(InMovieFilename, TEXT("")) == 0 || MovieFilename == InMovieFilename;

	UBOOL Result = MovieView.GetPtr() != 0 && FilenameMatch && !IsStopped;
	if ( IsStopped )
	{
		if ( MovieView )
		{
			MovieView->SetCaptureThread ( Scaleform::GetCurrentThreadId() );
		}
		MovieDef = 0;
		MovieView = 0;
		MovieDispHandle = 0;
	}
	return Result;
}

FString FFullScreenMovieGFx::GameThreadGetLastMovieName()
{
	return MovieFilename;
}

void FFullScreenMovieGFx::GameThreadInitiateStartupSequence()
{
	FConfigSection* MovieIni = GConfig->GetSectionPrivate( TEXT("FullScreenMovie"), FALSE, TRUE, GEngineIni );
	if (MovieIni)
	{
		TArray<FString> StartupMovies;
		for (FConfigSectionMap::TIterator It(*MovieIni); It; ++It)
		{
			if (It.Key() == TEXT("StartupMovies"))
			{
				StartupMovies.AddItem(It.Value());
			}
		}
		// Play a random movie from the list, if any
		if (StartupMovies.Num() != 0)
		{
			GameThreadPlayMovie( MM_PlayOnceFromStream, *StartupMovies(appRand() % StartupMovies.Num()) );
		}
	}
}

INT FFullScreenMovieGFx::GameThreadGetCurrentFrame()
{
	Scaleform::Mutex::Locker locker ( &PlayerMutex );
	if ( MovieView )
	{
		return MovieView->GetCurrentFrame();
	}
	else
	{
		return 0;
	}
}

void FFullScreenMovieGFx::ReleaseDynamicResources()
{
}

void FFullScreenMovieGFx::GameThreadSetMovieHidden ( UBOOL bInHidden )
{
	MovieHidden = bInHidden;
}

UBOOL FFullScreenMovieGFx::InputKey ( FViewport* Viewport, INT ControllerId, FName Key, EInputEvent EventType, FLOAT AmountDepressed, UBOOL bGamepad )
{
	return FALSE;
}

#endif // WITH_GFx_FULLSCREEN_MOVIE

#endif // WITH_GFx
