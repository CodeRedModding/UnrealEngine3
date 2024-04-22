/*=============================================================================
	TextureFlipBook.cpp: UTextureFlipBook implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UTextureFlipBook);

/** Plays the movie and also unpauses.		*/
void UTextureFlipBook::Play()
{
	bPaused		= 0;
	bStopped	= 0;
}

/** Pauses the movie.						*/	
void UTextureFlipBook::Pause()
{
	bPaused		= 1;
}

/** Stops movie playback.					*/
void UTextureFlipBook::Stop()
{
	bStopped			= 1;
	TimeIntoMovie		= 0.f;
	TimeSinceLastFrame	= 0.f;

	SetStartFrame();
}

/** Sets the current frame to display.		*/	
void UTextureFlipBook::SetCurrentFrame(INT InRow,INT InCol)
{
	if ((InRow < HorizontalImages) && (InCol < VerticalImages))
	{
		CurrentRow		= InRow;
		CurrentColumn	= InCol;
	}
}
	
// FTickableObject interface
/**
 * Updates the movie texture if necessary by requesting a new frame from the decoder taking into account both
 * game and movie framerate.
 */
void UTextureFlipBook::Tick(FLOAT DeltaTime)
{
	if (!bPaused && !bStopped)
	{
		TimeIntoMovie		+= DeltaTime;
		TimeSinceLastFrame	+= DeltaTime;

		// Respect the movie streams frame rate for playback/ frame update.
		if (TimeSinceLastFrame >= FrameTime)
		{
			switch (FBMethod)
			{
			case TFBM_UL_ROW:
				if ((CurrentColumn + 1) >= HorizontalImages)
				{
					if ((CurrentRow + 1) >= VerticalImages)
					{
						if (bLooping)
						{
							CurrentRow		= 0;
							CurrentColumn	= 0;
						}
					}
					else
					{
						CurrentRow++;
						CurrentColumn	= 0;
					}
				}
				else
				{
					CurrentColumn++;
				}
				break;
			case TFBM_UL_COL:
				if ((CurrentRow + 1) >= VerticalImages)
				{
					if ((CurrentColumn + 1) >= HorizontalImages)
					{
						if (bLooping)
						{
							CurrentRow		= 0;
							CurrentColumn	= 0;
						}
					}
					else
					{
						CurrentRow	= 0;
						CurrentColumn++;
					}
				}
				else
				{
					CurrentRow++;
				}
				break;
			case TFBM_UR_ROW:
				if ((CurrentColumn - 1) < 0)
				{
					if ((CurrentRow + 1) >= VerticalImages)
					{
						if (bLooping)
						{
							CurrentRow		= 0;
							CurrentColumn	= HorizontalImages - 1;
						}
					}
					else
					{
						CurrentRow++;
						CurrentColumn	= HorizontalImages - 1;
					}
				}
				else
				{
					CurrentColumn--;
				}
				break;
			case TFBM_UR_COL:
				if ((CurrentRow + 1) >= VerticalImages)
				{
					if ((CurrentColumn - 1) < 0)
					{
						if (bLooping)
						{
							CurrentRow		= 0;
							CurrentColumn	= HorizontalImages - 1;
						}
					}
					else
					{
						CurrentRow	= 0;
						CurrentColumn--;
					}
				}
				else
				{
					CurrentRow++;
				}
				break;
			case TFBM_LL_ROW:
				if ((CurrentColumn + 1) >= HorizontalImages)
				{
					if ((CurrentRow - 1) < 0)
					{
						if (bLooping)
						{
							CurrentRow		= VerticalImages - 1;
							CurrentColumn	= 0;
						}
					}
					else
					{
						CurrentRow--;
						CurrentColumn	= 0;
					}
				}
				else
				{
					CurrentColumn++;
				}
				break;
			case TFBM_LL_COL:
				if ((CurrentRow - 1) < 0)
				{
					if ((CurrentColumn + 1) >= HorizontalImages)
					{
						if (bLooping)
						{
							CurrentRow		= VerticalImages - 1;
							CurrentColumn	= 0;
						}
					}
					else
					{
						CurrentRow		= VerticalImages - 1;
						CurrentColumn++;
					}
				}
				else
				{
					CurrentRow--;
				}
				break;
			case TFBM_LR_ROW:
				if ((CurrentColumn - 1) < 0)
				{
					if ((CurrentRow - 1) < 0)
					{
						if (bLooping)
						{
							CurrentRow		= VerticalImages - 1;
							CurrentColumn	= HorizontalImages - 1;
						}
					}
					else
					{
						CurrentRow--;
						CurrentColumn	= HorizontalImages - 1;
					}
				}
				else
				{
					CurrentColumn--;
				}
				break;
			case TFBM_LR_COL:
				if ((CurrentRow - 1) < 0)
				{
					if ((CurrentColumn - 1) < 0)
					{
						if (bLooping)
						{
							CurrentRow		= VerticalImages - 1;
							CurrentColumn	= HorizontalImages - 1;
						}
					}
					else
					{
						CurrentRow		= VerticalImages - 1;
						CurrentColumn--;
					}
				}
				else
				{
					CurrentRow--;
				}
				break;
			case TFBM_RANDOM:
				{
					CurrentColumn	= appTrunc(appFrand() * HorizontalImages);
					CurrentRow		= appTrunc(appFrand() * VerticalImages);
				}
				break;
			}

			// Avoid getting out of sync by adjusting TimeSinceLastFrame to take into account difference
			// between where we think the movie should be at and what the decoder thinks.
			TimeSinceLastFrame = 0;
		}
	}

	SetTextureOffset();
}

// UObject interface.
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTextureFlipBook::InitializeIntrinsicPropertyValues()
{
	SRGB = TRUE;
	UnpackMin[0] = UnpackMin[1] = UnpackMin[2] = UnpackMin[3] = 0.0f;
	UnpackMax[0] = UnpackMax[1] = UnpackMax[2] = UnpackMax[3] = 1.0f;
}
/**
 * Serializes the data.
 */
void UTextureFlipBook::Serialize(FArchive& Ar)
{
	//@todo. Remove this if it doesn't extend the base functionality
	Super::Serialize(Ar);
//	FStaticTexture2D::Serialize(Ar);
}

/**
 * Postload initialization of movie texture. Creates decoder object and retriever first frame.
 */
void UTextureFlipBook::PostLoad()
{
	Super::PostLoad();

	// Determine the horizontal and vertical scale.
	HorizontalScale	= 1.0f / (FLOAT)HorizontalImages;// / (FLOAT)SizeX;
	VerticalScale	= 1.0f / (FLOAT)VerticalImages;// / (FLOAT)SizeY;
	if (FrameRate > 0)
	{
		FrameTime = 1.0f / FrameRate;
	}
	else
	{
		FrameTime = 1.0f;
	}

	SetStartFrame();

	if(!HasAnyFlags(RF_ClassDefaultObject))
	{
#if GEMINI_TODO
		GResourceManager->UpdateResource(this);
#endif
	}

	if (!bAutoPlay)
	{
		bPaused		= 1;
		bStopped	= 0;
	}
}

/**
 * Called after the garbage collection mark phase on unreachable objects.
 */
void UTextureFlipBook::BeginDestroy()
{
	if (!ReleaseResourcesFence)
	{
		ReleaseResourcesFence = new FRenderCommandFence;
	}
	ReleaseResourcesFence->BeginFence();
	Super::BeginDestroy();
}

/**
 * Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
 * potentially asynchronous object cleanup.
 * @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
 */
UBOOL UTextureFlipBook::IsReadyForFinishDestroy()
{
	check(ReleaseResourcesFence);
	const UBOOL bIsReadyForFinishDestroy = (ReleaseResourcesFence->GetNumPendingFences() == 0);
	const UBOOL bSuperIsReadyForFinishDestroy = Super::IsReadyForFinishDestroy();
	return (bIsReadyForFinishDestroy && bSuperIsReadyForFinishDestroy);
}

/**
 * We need to ensure that the decoder doesn't have any references to RawData before destructing it.
 */
void UTextureFlipBook::FinishDestroy()
{
	delete ReleaseResourcesFence;
	ReleaseResourcesFence = NULL;
	//@todo. Remove this if it doesn't extend the base functionality
	Super::FinishDestroy();
}

/**
 * PostEditChange - gets called whenever a property is either edited via the Editor or the "set" console command.
 */
void UTextureFlipBook::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	//@todo. Remove this if it doesn't extend the base functionality
	Super::PostEditChangeProperty(PropertyChangedEvent);

	// Determine the horizontal and vertical scale.
	HorizontalScale	= 1.0f / (FLOAT)HorizontalImages;// / (FLOAT)SizeX;
	VerticalScale	= 1.0f / (FLOAT)VerticalImages;// / (FLOAT)SizeY;
	if (FrameRate > 0)
	{
		FrameTime = 1.0f / FrameRate;
	}
	else
	{
		FrameTime = 1.0f;
	}

	// Reset to the starting frame
	SetStartFrame();
}

/**
 * SetStartFrame
 *
 * Determines the starting row/col from the currently set flip-book method.
 */
void UTextureFlipBook::SetStartFrame()
{
	switch (FBMethod)
	{
	case TFBM_UL_ROW:
	case TFBM_UL_COL:
		CurrentRow		= 0;
		CurrentColumn	= 0;
		break;
	case TFBM_UR_ROW:
	case TFBM_UR_COL:
		CurrentRow		= 0;
		CurrentColumn	= HorizontalImages - 1;
		break;
	case TFBM_LL_ROW:
	case TFBM_LL_COL:
		CurrentRow		= VerticalImages - 1;
		CurrentColumn	= 0;
		break;
	case TFBM_LR_ROW:
	case TFBM_LR_COL:
		CurrentRow		= VerticalImages - 1;
		CurrentColumn	= HorizontalImages - 1;
		break;
	}
}

// Thumbnail interface.

/** 
 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
 */
FString UTextureFlipBook::GetDesc()
{
	return FString::Printf( TEXT("%dx%d[%s%s] %dx%d"), SizeX, SizeY, GPixelFormats[Format].Name,
		DeferCompression ? TEXT("*") : TEXT(""), HorizontalImages, VerticalImages);
}

/** 
 * Returns detailed info to populate listview columns
 */
FString UTextureFlipBook::GetDetailedDescription( INT InIndex )
{
	FString Description = TEXT( "" );
	switch( InIndex )
	{
	case 0:
		Description = FString::Printf( TEXT( "%dx%d" ), SizeX, SizeY );
		break;
	case 1:
		Description = GPixelFormats[Format].Name;
		if( DeferCompression )
		{
			Description += TEXT( "*" );
		}
		break;
	}
	return( Description );
}

// FlipBook texture interface...
void UTextureFlipBook::GetTextureOffset(FVector2D& UVOffset)
{
	UVOffset.X = HorizontalScale * CurrentColumn;
	UVOffset.Y = VerticalScale	 * CurrentRow;
}

void UTextureFlipBook::GetTextureOffset_RenderThread(FLinearColor& UVOffset) const
{
    UVOffset.R = RenderOffsetU;
	UVOffset.G = RenderOffsetV;
	UVOffset.B = 0.0f;
	UVOffset.A = 0.0f;
}
	
void UTextureFlipBook::SetTextureOffset()
{
	FLOAT UOffset = HorizontalScale * CurrentColumn;
	FLOAT VOffset = VerticalScale * CurrentRow;
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		FlipBookSetTextureOffsetCommand,
		UTextureFlipBook*, FlipBook, this,
		FLOAT, NewUOffset, UOffset,
		FLOAT, NewVOffset, VOffset,
		{
			FlipBook->SetTextureOffset_RenderThread(NewUOffset, NewVOffset);
		}
	);
}

void UTextureFlipBook::SetTextureOffset_RenderThread(FLOAT UOffset, FLOAT VOffset)
{
    RenderOffsetU = UOffset;
    RenderOffsetV = VOffset;
}
