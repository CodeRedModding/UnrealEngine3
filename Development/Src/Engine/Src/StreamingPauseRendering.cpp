/*=============================================================================
	StreamingPauseRendering.cpp: Streaming pause implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "StreamingPauseRendering.h"

#if XBOX
#include "XeD3DDrvPrivate.h"
#endif

#if PS3
#include <sysutil/sysutil_common.h>
#endif

#define STREAMING_PAUSE_DELAY (1.0f / 30.0f)

extern FLOAT GetDisplayGamma(void);
extern void SetDisplayGamma(FLOAT Gamma);

struct FStreamingPauseFlipPumper;
class FFrontBufferTexture;

/** Wheter to use streaming pause instead of default suspend/resume RHI interface. */
UBOOL						GUseStreamingPause = FALSE;

/** Temporary view used for drawing the streaming pause icon. */
FViewInfo*					GStreamingPauseView = NULL;

/** Material used to draw the streaming pause icon. */
FMaterialRenderProxy*		GStreamingPauseMaterialRenderProxy = NULL;

/** Viewport that we are rendering into. */
FViewport*					GStreamingPauseViewport = NULL;

/** Texture used to store the front buffer. */
FFrontBufferTexture*		GStreamingPauseBackground = NULL;

/** An instance of the streaming pause object. */
FStreamingPauseFlipPumper*	GStreamingPause = NULL;

/**
 * A texture used to store the front buffer. 
 */
class FFrontBufferTexture : public FTextureResource
{
public:
	/** Constructor. */
	FFrontBufferTexture( INT InSizeX, INT InSizeY )
		: SizeX( InSizeX )
		, SizeY( InSizeY )
	{
	}

	// FResource interface.
	virtual void InitRHI()
	{
		// Create the texture RHI.  	
		DWORD TexCreateFlags = TexCreate_Dynamic;
#if PS3
		// To force correct pixel mapping
		TexCreateFlags |= TexCreate_ResolveTargetable;
#endif

		Texture2DRHI = RHICreateTexture2D( SizeX, SizeY, PF_A8R8G8B8, 1, TexCreateFlags, NULL );
		TextureRHI = Texture2DRHI;

		// Create the sampler state RHI resource.
		FSamplerStateInitializerRHI SamplerStateInitializer(SF_Point,AM_Wrap,AM_Wrap,AM_Wrap);
		SamplerStateRHI = RHICreateSamplerState(SamplerStateInitializer);
	}

	/** Returns the width of the texture in pixels. */
	virtual UINT GetSizeX() const
	{
		return SizeX;
	}

	/** Returns the height of the texture in pixels. */
	virtual UINT GetSizeY() const
	{
		return SizeY;
	}

	/** Returns the texture2D's RHI resource. */
	FTexture2DRHIRef GetTexture2DRHI()
	{
		return Texture2DRHI;
	}

private:
	/** The texture2D's RHI resource. */
	FTexture2DRHIRef Texture2DRHI;

	/** The width of the front buffer texture in pixels. */
	INT SizeX;

	/** The height of the front buffer texture in pixels. */
	INT SizeY;
};

/** Returns TRUE if we can render the streaming pause. */
UBOOL CanRenderStreamingPause()
{
	return GStreamingPauseView && GStreamingPauseMaterialRenderProxy && GStreamingPauseViewport && GStreamingPauseBackground;
}

/** Renders the streaming pause icon. */
void FStreamingPause::Render()
{
	if ( CanRenderStreamingPause() )
	{		
		GStreamingPauseViewport->BeginRenderFrame();

		FCanvas Canvas(GStreamingPauseViewport, NULL);

		const FLOAT LoadingSize = 0.1f * GStreamingPauseViewport->GetSizeY();
		const FLOAT LoadingOffsetX =  0.8f * GStreamingPauseViewport->GetSizeX() - LoadingSize;
		const FLOAT LoadingOffsetY =  0.8f * GStreamingPauseViewport->GetSizeY() - LoadingSize;

		DrawTile(&Canvas, 0, 0, GStreamingPauseViewport->GetSizeX(), GStreamingPauseViewport->GetSizeY(), 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor(1.0f, 1.0f, 1.0f, 1), GStreamingPauseBackground, FALSE);
		DrawTile(&Canvas, LoadingOffsetX, LoadingOffsetY, LoadingSize, LoadingSize, 0.0f, 0.0f, 1.0f, 1.0f, GStreamingPauseMaterialRenderProxy);

		const FLOAT OldGamma = GetDisplayGamma();
		SetDisplayGamma( 1.0f );

		Canvas.Flush();

		SetDisplayGamma( OldGamma );

		GStreamingPauseViewport->EndRenderFrame( TRUE, TRUE );
	}
}

/** Streaming pause object. */
struct FStreamingPauseFlipPumper : public FTickableObjectRenderThread
{
	FStreamingPauseFlipPumper()
		: DelayCountdown(STREAMING_PAUSE_DELAY)
	{
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It is
	 * used to determine whether an object is ready to be ticked. This is 
	 * required for example for all UObject derived classes as they might be
	 * loaded async and therefore won't be ready immediately.
	 *
	 * @return	TRUE if class is ready to be ticked, FALSE otherwise.
	 */
	virtual UBOOL IsTickable() const
	{
		return TRUE;
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. It will
	 * be called from within UnLevTick.cpp after ticking all actors.
	 *
	 * @param DeltaTime	Game time passed since the last call.
	 */
	void Tick(FLOAT DeltaTime)
	{
		// countdown to a flip
		DelayCountdown -= DeltaTime;
		
		// if we reached the flip time, flip
		if (DelayCountdown <= 0.0f)
		{
			FStreamingPause::Render();

			// reset timer by adding 1 full delay, but don't allow it to be negative
			DelayCountdown = Max(0.0f, DelayCountdown + STREAMING_PAUSE_DELAY);
		}
	}

	/** Delay until next flip */
	FLOAT DelayCountdown;
};

/** Initializes the streaming pause object. */
void FStreamingPause::Init()
{
	if( GStreamingPauseBackground == NULL && GUseStreamingPause )
	{
		GStreamingPauseBackground = new FFrontBufferTexture( GSceneRenderTargets.GetBufferSizeX(), GSceneRenderTargets.GetBufferSizeY() );
		GStreamingPauseBackground->InitRHI();
	}
}

/** Suspends the title rendering and enables the streaming pause rendering. */
void FStreamingPause::SuspendRendering()
{
	if (GStreamingPause == NULL)
	{
		if ( CanRenderStreamingPause() )
		{
			check( GStreamingPauseBackground );
			
			FResolveParams ResolveParams( GStreamingPauseBackground->GetTexture2DRHI() );
			RHICopyFrontBufferToTexture( ResolveParams );
		}
		
		// start the flip pumper
		GStreamingPause = new FStreamingPauseFlipPumper;
	}	
}

/** Resumes the title rendering and deletes the streaming pause rendering. */
void FStreamingPause::ResumeRendering()
{
	if ( GStreamingPauseView )
	{
		delete GStreamingPauseView->Family;
		delete GStreamingPauseView;
		GStreamingPauseView = NULL;
	}
	GStreamingPauseMaterialRenderProxy = NULL;

	delete GStreamingPause;
	GStreamingPause = NULL;

	GStreamingPauseViewport = NULL;
}

/** Returns TRUE if the title rendering is suspended. Otherwise, returns FALSE. */
UBOOL FStreamingPause::IsRenderingSuspended()
{
	return GStreamingPause ? TRUE : FALSE;
}

/** Updates the streaming pause rendering. */
void FStreamingPause::Tick(FLOAT DeltaTime)
{
	if (GStreamingPause)
	{
		GStreamingPause->Tick(DeltaTime);
	}
}

/** Enqueue the streaming pause to suspend rendering during blocking load. */
void FStreamingPause::GameThreadWantsToSuspendRendering( FViewport* GameViewport )
{
#if WITH_STREAMING_PAUSE
	if( GUseStreamingPause )
	{
		FSceneViewFamily* StreamingPauseViewFamily = NULL;
		FViewInfo* StreamingPauseView = NULL;
		FMaterialRenderProxy* StreamingPauseMaterialProxy = NULL;
		UCanvas* CanvasObject = FindObject<UCanvas>(UObject::GetTransientPackage(),TEXT("CanvasObject"));

		if ( GameViewport != NULL )
		{
			FCanvas Canvas(GameViewport,NULL);

			const FRenderTarget* CanvasRenderTarget = Canvas.GetRenderTarget();

			StreamingPauseViewFamily = new FSceneViewFamily(
				CanvasRenderTarget,
				NULL,
				SHOW_DefaultGame,
				GWorld->GetTimeSeconds(),
				GWorld->GetDeltaSeconds(),
				GWorld->GetRealTimeSeconds(),
				FALSE,FALSE,FALSE,TRUE,TRUE,
				CanvasRenderTarget->GetDisplayGamma(),
				FALSE, FALSE
				);

			// make a temporary view
			StreamingPauseView = new FViewInfo(StreamingPauseViewFamily, 
				NULL, 
				-1,
				NULL, 
				NULL, 
				NULL, 
				NULL, 
				NULL,
				NULL, 
				0, 
				0, 
				0,
				0,
				CanvasRenderTarget->GetSizeX(), 
				CanvasRenderTarget->GetSizeY(), 
				FMatrix::Identity, 
				FMatrix::Identity, 
				FLinearColor::Black, 
				FLinearColor::White, 
				FLinearColor::White, 
				TSet<UPrimitiveComponent*>()
				);

			UMaterial* StreamingPauseMaterial = NULL;
			const AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
			if (WorldInfo)
			{
				if (WorldInfo->Game)
				{
					StreamingPauseMaterial = GWorld->GetGameInfo()->StreamingPauseIcon;
				}
				else if (WorldInfo->GRI && WorldInfo->GRI->GameClass)
				{
					const AGameInfo* GameDefaults = (AGameInfo*)WorldInfo->GRI->GameClass->GetDefaultObject();
					StreamingPauseMaterial = GameDefaults->StreamingPauseIcon;
				}
			}
			if ( StreamingPauseMaterial )
			{
				StreamingPauseMaterialProxy = StreamingPauseMaterial->GetRenderProxy( FALSE );
			}
		}

		struct FSuspendRenderingParameters
		{
			FViewInfo*  StreamingPauseView;
			FMaterialRenderProxy* StreamingPauseMaterialRenderProxy;
			FViewport* Viewport;
		};

		FSuspendRenderingParameters SuspendRenderingParameters =
		{
			StreamingPauseView,
			StreamingPauseMaterialProxy,
			GameViewport,
		};

		// Enqueue command to suspend rendering during blocking load.
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			SuspendRendering,
			FSuspendRenderingParameters, Parameters, SuspendRenderingParameters,
		{ 
			extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = TRUE; 
			extern FViewInfo* GStreamingPauseView; GStreamingPauseView = Parameters.StreamingPauseView; 
			extern FMaterialRenderProxy* GStreamingPauseMaterialRenderProxy; GStreamingPauseMaterialRenderProxy = Parameters.StreamingPauseMaterialRenderProxy; 
			extern FViewport* GStreamingPauseViewport; GStreamingPauseViewport = Parameters.Viewport;
		} );
	}
	else
#endif // WITH_STREAMING_PAUSE
	{
		// Enqueue command to suspend rendering during blocking load.
		ENQUEUE_UNIQUE_RENDER_COMMAND( SuspendRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = TRUE; } );
	}

	// Flush rendering commands to ensure we don't have any outstanding work on rendering thread
	// and rendering is indeed suspended.
	FlushRenderingCommands();
}

/** Enqueue the streaming pause to resume rendering after blocking load is completed. */
void FStreamingPause::GameThreadWantsToResumeRendering()
{
	// Resume rendering again now that we're done loading.
	ENQUEUE_UNIQUE_RENDER_COMMAND( ResumeRendering, { extern UBOOL GGameThreadWantsToSuspendRendering; GGameThreadWantsToSuspendRendering = FALSE; RHIResumeRendering(); } );

	FlushRenderingCommands();
}