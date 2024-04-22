/**
 * TextureFlipBook
 * FlipBook texture support base class.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class TextureFlipBook extends Texture2D
	native(Texture)
	hidecategories(Object)
	inherits(FTickableObject)
	forcescriptorder( TRUE );

// TextureFlipBook

/** Time into the movie in seconds.																*/
var				const	transient	float				TimeIntoMovie;
/** Time that has passed since the last frame. Will be adjusted by decoder to combat drift.		*/
var				const	transient	float				TimeSinceLastFrame;

/** The horizontal scale factor																	*/
var				const	transient	float				HorizontalScale;
/** The vertical scale factor																	*/
var				const	transient	float				VerticalScale;

/** Whether the movie is currently paused.														*/
var				const				bool				bPaused;
/** Whether the movie is currently stopped.														*/
var				const				bool				bStopped;
/** Whether the movie should loop when it reaches the end.										*/
var(FlipBook)						bool				bLooping;
/** Whether the movie should automatically start playing when it is loaded.						*/
var(FlipBook)						bool				bAutoPlay;

/** The horizontal and vertical sub-image count													*/
var(FlipBook)						int					HorizontalImages;
var(FlipBook)						int					VerticalImages;

/** FlipBookMethod
 *
 * This defines the order by which the images should be 'flipped through'
 *	TFBM_UL_ROW		Start upper-left, go across to the the right, go to
 *				    the next row down left-most and repeat.
 *	TFBM_UL_COL		Start upper-left, go down to the bottom, pop to the
 *					top of the next column to the right and repeat.
 *	TFBM_UR_ROW		Start upper-right, go across to the the left, go to
 *				    the next row down right-most and repeat.
 *	TFBM_UR_COL		Start upper-right, go down to the bottom, pop to the
 *					top of the next column to the left and repeat.
 *	TFBM_LL_ROW		Start lower-left, go across to the the right, go to
 *				    the next row up left-most and repeat.
 *	TFBM_LL_COL		Start lower-left, go up to the top, pop to the
 *					bottom of the next column to the right and repeat.
 *	TFBM_LR_ROW		Start lower-right, go across to the the left, go to
 *				    the next row up left-most and repeat.
 *	TFBM_LR_COL		Start lower-right, go up to the top, pop to the
 *					bottom of the next column to the left and repeat.
 *	TFBM_RANDOM		Randomly select the next image
 *
 */
enum TextureFlipBookMethod
{
 	TFBM_UL_ROW,
 	TFBM_UL_COL,
 	TFBM_UR_ROW,
 	TFBM_UR_COL,
 	TFBM_LL_ROW,
 	TFBM_LL_COL,
 	TFBM_LR_ROW,
 	TFBM_LR_COL,
	TFBM_RANDOM
};
var(FlipBook)						TextureFlipBookMethod	FBMethod;

/** The time to display a single frame															*/
var(FlipBook)						float					FrameRate;
var				private				float					FrameTime;

/** The current sub-image row																	*/
var				const	transient	int						CurrentRow;
/** The current sub-image column																*/
var				const	transient	int						CurrentColumn;

/** The current sub-image row for the render-thread												*/
var				const	transient	float					RenderOffsetU;
/** The current sub-image column for the render-thread											*/
var				const	transient	float					RenderOffsetV;
/** Command fence used to shut down properly													*/
var		native	const	pointer								ReleaseResourcesFence{FRenderCommandFence};

/** Plays the movie and also unpauses.															*/
native function Play();
/** Pauses the movie.																			*/
native function Pause();
/** Stops movie playback.																		*/
native function Stop();
/** Sets the current frame of the 'movie'.														*/
native function SetCurrentFrame(int Row, int Col);

cpptext
{
	// FTickableObject interface

	/**
	 * Updates the movie texture if necessary by requesting a new frame from the decoder taking into account both
	 * game and movie framerate.
	 *
	 * @param DeltaTime		Time (in seconds) that has passed since the last time this function has been called.
	 */
	virtual void Tick( FLOAT DeltaTime );

	/**
	 * Returns whether it is okay to tick this object. E.g. objects being loaded in the background shouldn't be ticked
	 * till they are finalized and unreachable objects cannot be ticked either.
	 *
	 * @return	TRUE if tickable, FALSE otherwise
	 */
	virtual UBOOL IsTickable() const
	{
		// We cannot tick objects that are unreachable or are in the process of being loaded in the background.
		return !HasAnyFlags( RF_Unreachable | RF_AsyncLoading );
	}
	
	// UObject interface.
	/**
	 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
	 * is initialized against its archetype, but before any objects of this class are created.
	 */
	void InitializeIntrinsicPropertyValues();
	/**
	 * Serializes the compressed movie data.
	 *
	 * @param Ar	FArchive to serialize RawData with.
	 */
	virtual void Serialize(FArchive& Ar);
	
	/**
	 * Postload initialization of movie texture. Creates decoder object and retriever first frame.
	 */
	virtual void PostLoad();
	/**
	 * PostEditChange - gets called whenever a property is either edited via the Editor or the "set" console command.
	 *
	 * @param PropertyThatChanged	Property that changed
	 */
	virtual void PostEditChangeProperty(FPropertyChangedEvent& /*PropertyChangedEvent*/);

	/**
	 * Called after the garbage collection mark phase on unreachable objects.
	 */
	virtual void BeginDestroy();
	/**
	 * Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
	 * potentially asynchronous object cleanup.
	 * @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
	 */
	virtual UBOOL IsReadyForFinishDestroy();
	/**
	 * We need to ensure that the decoder doesn't have any references to RawData before destructing it.
	 */
	virtual void FinishDestroy();

	// Thumbnail interface.

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	// FlipBook texture interface...
	void			SetStartFrame();
	virtual UBOOL	IsAFlipBook()	{ return true;	}
	virtual void	GetFlipBookOffset(FVector& Offset)
	{
		Offset.X = CurrentColumn	* HorizontalScale;
		Offset.Y = CurrentRow		* VerticalScale;
	}
	virtual void	GetFlipBookScale(FVector& Scale)
	{
		Scale.X	= HorizontalScale;
		Scale.Y = VerticalScale;
	}

	/**
	 *	Retrieve the UV offset
	 *
	 *	@param	UVOffset	FVector2D to fill in with the offset
	 */
	void GetTextureOffset(FVector2D& UVOffset);
	/**
	 *	Retrieve the UV offset
	 *
	 *	@param	UVOffset	FLinearColor to fill in with the offset
	 */
	virtual void GetTextureOffset_RenderThread(FLinearColor& UVOffset) const;

	/**	
	 *	Set the texture offset (pass it to the render thread)
	 */
	void SetTextureOffset();
	/**
	 *	Set the texture offset in the render thread
	 *
	 *	@param	UOffset		The value to set for the U offset
	 *	@param	VOffset		The value to set for the V offset
	 */
	void SetTextureOffset_RenderThread(FLOAT UOffset, FLOAT VOffset);
}

defaultproperties
{
	bStopped=false
	bLooping=true
	bAutoPlay=true
	FrameRate=4
	FrameTime=0.25
	CurrentRow=0
	CurrentColumn=0
	HorizontalImages=1
	VerticalImages=1
	FBMethod=TFBM_UL_ROW
	AddressX=TA_Clamp
	AddressY=TA_Clamp
}
