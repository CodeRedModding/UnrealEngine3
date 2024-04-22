/*=============================================================================
	TTFontImport.cpp: True-type Font Importing
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "UnrealEd.h"
#include "Factories.h"



IMPLEMENT_CLASS(UTrueTypeFontFactory);
IMPLEMENT_CLASS(UTrueTypeMultiFontFactory);


INT FromHex( TCHAR Ch )
{
	if( Ch>='0' && Ch<='9' )
		return Ch-'0';
	else if( Ch>='a' && Ch<='f' )
		return 10+Ch-'a';
	else if( Ch>='A' && Ch<='F' )
		return 10+Ch-'A';
	appErrorf(LocalizeSecure(LocalizeUnrealEd("Error_ExpectingDigitGotCharacter"),Ch));
	return 0;
}




void UTrueTypeFontFactory::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);

	// Font import options property (UFontImportOptions)
	new( GetClass(), TEXT( "ImportOptions" ), RF_Public ) UObjectProperty( CPP_PROPERTY( ImportOptions ), TEXT( "" ), CPF_Edit | CPF_EditInline | CPF_NoClear , UFontImportOptions::StaticClass() );
	{
		// Tell the garbage collector about our UObject reference so that it won't be GC'd!
		UClass* TheClass = GetClass();
		TheClass->EmitObjectReference( STRUCT_OFFSET( UTrueTypeFontFactory, ImportOptions ) );
	}
}


#if !__WIN32__  // !!! FIXME
#define FW_NORMAL 400
#endif



/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTrueTypeFontFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UFont::StaticClass();
	bCreateNew = TRUE;
	bEditAfterNew = TRUE;
	AutoPriority = -1;
	Description = SupportedClass->GetName();
}



UTrueTypeFontFactory::UTrueTypeFontFactory()
{
}



void UTrueTypeFontFactory::SetupFontImportOptions()
{
	// Allocate our import options object if it hasn't been created already!
	ImportOptions = ConstructObject< UFontImportOptions >( UFontImportOptions::StaticClass(), this, NAME_None );
}


UObject* UTrueTypeFontFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext*	Warn )
{
#if !__WIN32__
	STUBBED("Win32 TTF code");
	return NULL;
#else
	check(Class==UFont::StaticClass());

	// Create font and its texture.
	UFont* Font = new( InParent, Name, Flags )UFont;
	
	if (ImportOptions->Data.bUseDistanceFieldAlpha)
	{
		// high res font bitmap should only contain a mask
		ImportOptions->Data.bEnableAntialiasing = FALSE;
		// drop shadows can be generated dynamically during rendering of distance fields
		ImportOptions->Data.bEnableDropShadow = FALSE;
		// scale factor should always be a power of two
		ImportOptions->Data.DistanceFieldScaleFactor = appRoundUpToPowerOfTwo(Max<INT>(ImportOptions->Data.DistanceFieldScaleFactor,2));
		ImportOptions->Data.DistanceFieldScanRadiusScale = Clamp<FLOAT>(ImportOptions->Data.DistanceFieldScanRadiusScale,0.f,8.f);
		// need a minimum padding of 4,4 to prevent bleeding of distance values across characters
 		ImportOptions->Data.XPadding = Max<INT>(4,ImportOptions->Data.XPadding);
 		ImportOptions->Data.YPadding = Max<INT>(4,ImportOptions->Data.YPadding);
	}

	// Copy the import settings into the font for later reference
	Font->ImportOptions = ImportOptions->Data;
	

	// For a single-resolution font, we'll create a one-element array and pass that along to our import function
	TArray< FLOAT > ResHeights;
	ResHeights.AddItem( ImportOptions->Data.Height );

	GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT( "FontFactory_ImportingTrueTypeFont" ) ), TRUE );

	// Import the font
	const UBOOL bSuccess = ImportTrueTypeFont( Font, Warn, ResHeights.Num(), ResHeights );
	if (!bSuccess)
	{
		Font = NULL;
	}

	GWarn->EndSlowTask();

	return bSuccess ? Font : NULL;
#endif
}

/**
 * Attempt to reimport the specified object from its source
 *
 * @param	Obj	Object to attempt to reimport
 *
 * @return	TRUE if this handler was able to handle reimporting the provided object
 */
UBOOL UTrueTypeFontFactory::Reimport( UObject* InObject )
{
	UFont* FontToReimport = Cast<UFont>(InObject);
	UBOOL bSuccess = FALSE;
	if ( FontToReimport != NULL)
	{
		SetupFontImportOptions();
		this->ImportOptions->Data = FontToReimport->ImportOptions;

		if (NULL != UFactory::StaticImportObject(InObject->GetClass(), InObject->GetOuter(), *InObject->GetName(), RF_Public|RF_Standalone, TEXT(""), NULL, this))
		{
			bSuccess = TRUE;
		}
	}

	return bSuccess;
}


/**
* Utility to convert texture alpha mask to a signed distance field
*
* Based on the following paper:
* http://www.valvesoftware.com/publications/2007/SIGGRAPH2007_AlphaTestedMagnification.pdf
*/
class FTextureAlphaToDistanceField
{
	/** Container for the input image from which we build the distance field */
	struct FTaskSrcData
	{
		/**
		 * Creates a source data with the following parameters:
		 *
		 * @param SrcTexture  A pointer to the raw data
		 * @param InSrcSizeX  The Width of the data in pixels
		 * @param InSrcSizeY  The Height of the data in pixels
		 * @param InSrcFormat The format of each pixel
		 */
		FTaskSrcData(const BYTE* SrcTexture, INT InSrcSizeX, INT InSrcSizeY, BYTE InSrcFormat)
			: SrcSizeX(InSrcSizeX)
			, SrcSizeY(InSrcSizeY)
			, SrcTexture(SrcTexture)
			, SrcFormat(InSrcFormat)
		{ 
			check(SrcFormat == PF_A8R8G8B8);
		}

	/**
	 * Get the color for the source texture at the specified coordinates
	 *
	 * @param PointX - x coordinate
	 * @param PointY - y coordinate
	 * @return texel color
	 */
	FORCEINLINE FColor GetSourceColor(INT PointX, INT PointY) const
	{
		checkSlow(PointX < SrcSizeX && PointY < SrcSizeY);
		return FColor(
			SrcTexture[4 * (PointX + PointY * SrcSizeX) + 2],
			SrcTexture[4 * (PointX + PointY * SrcSizeX) + 1],
			SrcTexture[4 * (PointX + PointY * SrcSizeX) + 0],
			SrcTexture[4 * (PointX + PointY * SrcSizeX) + 3]
		);
	}
		
	/**
	 * Get just the alpha for the source texture at the specified coordinates
	 *
	 * @param PointX - x coordinate
	 * @param PointY - y coordinate
	 * @return texel alpha
	 */
	FORCEINLINE BYTE GetSourceAlpha(INT PointX, INT PointY) const
	{
		checkSlow(PointX < SrcSizeX && PointY < SrcSizeY);
		return SrcTexture[4 * (PointX + PointY * SrcSizeX) + 3];
	}

	/** Width of the source texture */
	const INT SrcSizeX;
	/** Height of the source texture */
	const INT SrcSizeY;

	private:
		/** Source texture used for silhouette determination. Alpha channel contains mask */
		const BYTE* SrcTexture;
		/** Format of the source texture. Assumed to be PF_A8R8G8B8 */
		const BYTE SrcFormat;
	};
	
	/** The source data */
	const FTaskSrcData TaskData;

	/** Downsampled destination texture. Populated by Generate().  Alpha channel contains distance field */
	TArray<BYTE> DstTexture;
	/** Width of the destination texture */
	INT DstSizeX;
	/** Height of the destination texture */
	INT DstSizeY;
	/** Format of the destination texture */
	BYTE DstFormat;

	/** A task that builds the distance field for a strip of the image. */
	class FBuildDistanceFieldTask : public FNonAbandonableTask
	{
	public:
		/**
		 * Initialize the task as follows:
		 *
		 * @param InThreadScaleCounter  Used by the invoker to track when tasks are completed.
		 * @param InTaskData            Source data from which to build the distance field
		 * @param InStartRow            Row on which this task should start.
		 * @param InNumRowsToProcess    How many rows this task should process
		 * @param InScanRadius          Radius of area to scan for nearest border
		 * @param InScaleFactor         The input image and resulting distance field
		 * @param WorkRemainingCounter  Counter used for providing feedback (i.e. updating the progress bar)
		 */
		FBuildDistanceFieldTask(
			FThreadSafeCounter* InThreadScaleCounter,
			TArray<FLOAT>* OutSignedDistances,
			const FTaskSrcData* InTaskData,
			INT InStartRow,
			INT InDstRowWidth,
			INT InNumRowsToProcess,
			INT InScanRadius,
			INT InScaleFactor,
			FThreadSafeCounter* InWorkRemainingCounter)
			: ThreadScaleCounter(InThreadScaleCounter)
			, SignedDistances(*OutSignedDistances)
			, TaskData(InTaskData)
			, StartRow(InStartRow)
			, DstRowWidth(InDstRowWidth)
			, NumRowsToProcess(InNumRowsToProcess)
			, ScanRadius(InScanRadius)
			, ScaleFactor(InScaleFactor)
			, WorkRemainingCounter(InWorkRemainingCounter)
		{
		}

		/**
		 * Calculate the signed distance at the given coordinate to the 
		 * closest silhouette edge of the source texture.  
		 * 
		 * If the current point is solid then the closest non-solid 
		 * pixel is the edge, and if the current point is non-solid
		 * then the closest solid pixel is the edge.
		 *
		 * @param PointX - x coordinate
		 * @param PointY - y coordinate
		 * @param ScanRadius - radius (in texels) to scan the source texture for a silhouette edge
		 * @return distance to silhouette edge
		 */
		FLOAT CalcSignedDistanceToSrc(INT PointX, INT PointY, INT ScanRadius) const;

		/** Called by the thread pool to do the work in this task */
		void DoWork(void);

		/** Give the name for external event viewers
		* @return	the name to display in external event viewers
		*/
		static const TCHAR *Name()
		{
			return TEXT("FBuildDistanceFieldTask");
		}

	private:
		FThreadSafeCounter* ThreadScaleCounter;
		TArray<FLOAT>& SignedDistances;
		const FTaskSrcData *TaskData;
		const INT StartRow;
		const INT DstRowWidth;
		const INT NumRowsToProcess;
		const INT ScanRadius;
		const INT ScaleFactor;
		FThreadSafeCounter* WorkRemainingCounter;
	};
	
public:

	/**
	* Constructor
	*
	* @param InSrcTexture - source texture data
	* @param InSrcSizeX - source texture width
	* @param InSrcSizeY - source texture height
	* @param InSrcFormat - source texture format
	*/	
	FTextureAlphaToDistanceField(const BYTE* InSrcTexture, INT InSrcSizeX, INT InSrcSizeY, BYTE InSrcFormat)
	:	TaskData(InSrcTexture, InSrcSizeX, InSrcSizeY, InSrcFormat)
	,	DstSizeX(0)
	,	DstSizeY(0)
	,	DstFormat(PF_A8R8G8B8)
	{
	}
	
	// Accessors
	
	const BYTE* GetResultTexture() const
	{
		return DstTexture.GetData();
	}
	const INT GetResultTextureSize() const
	{
		return DstTexture.Num();
	}	
	const INT GetResultSizeX() const
	{
		return DstSizeX;
	}	
	const INT GetResultSizeY() const
	{
		return DstSizeY;
	}
	
	/**
	* Generates the destination texture from the source texture.
	* The alpha channel of the destination texture contains the
	* signed distance field.
	*
	* The destination texture size is scaled based on the ScaleFactor.
	* Eg. a scale factor of 4 creates a destination texture 4x smaller.
	*
	* @param ScaleFactor - downsample scale from source to destination texture
	* @param ScanRadius - distance in texels to scan high res font for the silhouette
	*/
	void Generate(INT ScaleFactor, INT ScanRadius);

	/**
	 * Calculate the distance between 2 coordinates
	 *
	 * @param X1 - 1st x coordinate
	 * @param Y1 - 1st y coordinate
	 * @param X2 - 2nd x coordinate
	 * @param Y2 - 2nd y coordinate
	 * @return 2d distance between the coordinates 
	 */
	static FORCEINLINE FLOAT CalcDistance(INT X1, INT Y1, INT X2, INT Y2)
	{
		INT DX = X1 - X2;
		INT DY = Y1 - Y2;
		return appSqrt(DX*DX + DY*DY);
	}
	
};




/**
 * Generates the destination texture from the source texture.
 * The alpha channel of the destination texture contains the
 * signed distance field.
 *
 * The destination texture size is scaled based on the ScaleFactor.
 * Eg. a scale factor of 4 creates a destination texture 4x smaller.
 *
 * @param ScaleFactor - downsample scale from source to destination texture
 * @param ScanRadius - distance in texels to scan high res font for the silhouette
 */
void FTextureAlphaToDistanceField::Generate(INT ScaleFactor, INT ScanRadius)
{
	// restart progress bar for distance field generation as this can be slow
	GWarn->StatusUpdatef(0,0,TEXT("Generating Distance Field"));

	// need to maintain pow2 sizing for textures
	ScaleFactor = appRoundUpToPowerOfTwo(ScaleFactor);
	DstSizeX = TaskData.SrcSizeX / ScaleFactor;
	DstSizeY = TaskData.SrcSizeY / ScaleFactor;

	// destination texture
	// note that destination format can be different from source format	
	SIZE_T NumBytes = CalculateImageBytes(DstSizeX,DstSizeY,0,DstFormat);	
	DstTexture.Empty(NumBytes);	
	DstTexture.AddZeroed(NumBytes);
	
	// array of signed distance values for the downsampled texture
	TArray<FLOAT> SignedDistance;
	SignedDistance.Empty(DstSizeX*DstSizeY);
	SignedDistance.Add(DstSizeX*DstSizeY);
	
	// We want to run the distance field computation as 16 tasks for a speed boost on multi-core machines.
	const INT NumTasks = 16;
	FThreadSafeCounter BuildTasksCounter;
	const INT DstStripHeight = DstSizeY / NumTasks;
	
	// We need to report the progress, and all the threads must be able to update it.
	const INT TotalDistFieldWork = DstStripHeight * NumTasks;
	FThreadSafeCounter WorkProgressCounter;
	

	INT TotalProgress = DstSizeY-1;	
	// calculate distances
	for (INT y=0; y < DstSizeY; y+=DstStripHeight)
	{
		// The tasks will clean themselves up when they are completed; no need to call delete elsewhere.
		BuildTasksCounter.Increment();
		(new FAutoDeleteAsyncTask<FBuildDistanceFieldTask>(
			&BuildTasksCounter,
			&SignedDistance,
			&TaskData,
			y,
			DstSizeX,
			DstStripHeight,
			ScanRadius,
			ScaleFactor,
			&WorkProgressCounter ))->StartBackgroundTask();
	}

	while( BuildTasksCounter.GetValue() > 0 )
	{
		// Waiting for Distance Field to finish generating.
		GWarn->UpdateProgress(WorkProgressCounter.GetValue(),TotalDistFieldWork);	
		appSleep(.1f);
	}
	
	// find min,max range of distances
	const FLOAT BadMax = CalcDistance(0,0,TaskData.SrcSizeX,TaskData.SrcSizeY);;
	const FLOAT BadMin = -BadMax;
	FLOAT MaxDistance = BadMin;
	FLOAT MinDistance = BadMax;
	for (INT i=0; i < SignedDistance.Num(); i++)
	{
		if (SignedDistance(i) > BadMin &&
			SignedDistance(i) < BadMax)
		{
			MinDistance = Min<FLOAT>(MinDistance,SignedDistance(i));
			MaxDistance = Max<FLOAT>(MaxDistance,SignedDistance(i));
		}
	}
	
	// normalize distances
	FLOAT RangeDistance = MaxDistance - MinDistance;
	for (INT i=0; i < SignedDistance.Num(); i++)
	{
		// clamp edge cases that were never found due to limited scan radius
		if (SignedDistance(i) <= MinDistance)
		{			
			SignedDistance(i) = 0.0f;
		}
		else if (SignedDistance(i) >= MaxDistance)
		{
			SignedDistance(i) = 1.0f;		
		}
		else
		{
			// normalize and remap from [-1,+1] to [0,+1]
			SignedDistance(i) = SignedDistance(i) / RangeDistance * 0.5f + 0.5f;		
		}
	}
	
	// copy results to the destination texture
	if (DstFormat == PF_G8)
	{
		for( INT x=0; x < DstSizeX; x++ )
		{
			for( INT y=0; y < DstSizeY; y++ )
			{
				DstTexture(x + y * DstSizeX) = SignedDistance(x + y * DstSizeX) * 255;
			}
		}
	}
	else if (DstFormat == PF_A8R8G8B8)
	{
		for( INT x=0; x < DstSizeX; x++ )
		{
			for( INT y=0; y < DstSizeY; y++ )
			{
				const FColor SrcColor(TaskData.GetSourceColor((x*ScaleFactor) + (ScaleFactor / 2), (y*ScaleFactor) + (ScaleFactor / 2)));
				DstTexture(4 * (x + y * DstSizeX) + 0) = SrcColor.B;
				DstTexture(4 * (x + y * DstSizeX) + 1) = SrcColor.G;
				DstTexture(4 * (x + y * DstSizeX) + 2) = SrcColor.R;
				DstTexture(4 * (x + y * DstSizeX) + 3) = SignedDistance(x + y * DstSizeX) * 255;
			}
		}	
	}
	else
	{
		checkf(0,TEXT("unsupported format specified"));
	}
}

/**
 * Calculate the signed distance at the given coordinate to the 
 * closest silhouette edge of the source texture.  
 * 
 * If the current point is solid then the closest non-solid 
 *  pixel is the edge, and if the current point is non-solid
 * then the closest solid pixel is the edge.
 *
 * @param PointX - x coordinate
 * @param PointY - y coordinate
 * @param ScanRadius - radius (in texels) to scan the source texture for a silhouette edge
 *
 * @return distance to silhouette edge
 */
FLOAT FTextureAlphaToDistanceField::FBuildDistanceFieldTask::CalcSignedDistanceToSrc(INT PointX, INT PointY,INT ScanRadius) const
{
	// determine whether or not the source point is solid
	const UBOOL BaseIsSolid = TaskData->GetSourceAlpha(PointX,PointY) > 0;

	FLOAT ClosestDistance = FTextureAlphaToDistanceField::CalcDistance(0,0,TaskData->SrcSizeX,TaskData->SrcSizeY);
	
	// If the current point is solid then the closest non-solid 
	// pixel is the edge, and if the current point is non-solid
	// then the closest solid pixel is the edge.

	// Search pattern:
	//   Use increasing ring sizes allows us to early out.
	//   In the picture below 1s indicate the first ring
	//   while 2s indicate the 2nd ring.
	//
	//        2 2 2 2 2
	//        2 1 1 1 2
	//        2 1 * 1 2
	//        2 1 1 1 2
	//        2 2 2 2 2
    //  
	// Note that the "rings" are not actually circular, so
	// we may find a sample that is up to Sqrt(2*(RingSize^2)) away.
	// Once we have found such a sample, we must search a few more
	// rings in case a nearer sample can be found.

	UBOOL bFoundClosest = FALSE;
	INT RequiredRadius = ScanRadius;
	for (int RingSize=1; RingSize <= RequiredRadius; ++RingSize)
	{
		const INT StartX = Clamp<INT>(PointX - RingSize,0,TaskData->SrcSizeX);
		const INT EndX  = Clamp<INT>(PointX + RingSize,0,TaskData->SrcSizeX-1);
		const INT StartY = Clamp<INT>(PointY - RingSize,0,TaskData->SrcSizeY);
		const INT EndY = Clamp<INT>(PointY + RingSize,0,TaskData->SrcSizeY-1);
		
		//    - - -    <-- Search this top line
		//    . * .
		//    . . . 
		for (int x=StartX; x<=EndX; ++x)
		{
			const INT y = StartY;
			const BYTE SrcAlpha(TaskData->GetSourceAlpha(x, y));
			
			if ((BaseIsSolid && SrcAlpha == 0) || (!BaseIsSolid && SrcAlpha > 0))
			{
				const FLOAT Dist = CalcDistance(PointX, PointY, x, y);
				ClosestDistance = Min<FLOAT>(Dist, ClosestDistance);
				bFoundClosest = TRUE;
			}
		}

		//    . . .    
		//    . * .
		//    - - -    <-- Search the bottom line
		for (int x=StartX; x<=EndX; ++x)
		{
			const INT y = EndY;
			const BYTE SrcAlpha(TaskData->GetSourceAlpha(x, y));

			if ((BaseIsSolid && SrcAlpha == 0) || (!BaseIsSolid && SrcAlpha > 0))
			{
				const FLOAT Dist = CalcDistance(PointX, PointY, x, y);
				ClosestDistance = Min<FLOAT>(Dist, ClosestDistance);
				bFoundClosest = TRUE;
			}
		}

		//    . . .    
		//    - * -   <-- Search the left and right two vertical lines
		//    . . .    
		for (int y=StartY+1; y<=EndY-1; ++y)
		{
			{
				const INT x = StartX;
				const BYTE SrcAlpha(TaskData->GetSourceAlpha(x, y));

				if ((BaseIsSolid && SrcAlpha == 0) || (!BaseIsSolid && SrcAlpha > 0))
				{
					const FLOAT Dist = CalcDistance(PointX, PointY, x, y);
					ClosestDistance = Min<FLOAT>(Dist, ClosestDistance);
					bFoundClosest = TRUE;
				}
			}

			{
				const INT x = EndX;
				const BYTE SrcAlpha(TaskData->GetSourceAlpha(x, y));

				if ((BaseIsSolid && SrcAlpha == 0) || (!BaseIsSolid && SrcAlpha > 0))
				{
					const FLOAT Dist = CalcDistance(PointX, PointY, x, y);
					ClosestDistance = Min<FLOAT>(Dist, ClosestDistance);
					bFoundClosest = TRUE;
				}
			}
		}

		// We have found a sample on the edge, but we might have to search 
		// a few more rings to guarantee that we found the closest sample
		// on the edge.
		if ( bFoundClosest && RequiredRadius >= ScanRadius )
		{
			RequiredRadius = appCeil( appSqrt(RingSize*RingSize*2) );
			RequiredRadius = Min(RequiredRadius, ScanRadius);
		}
	}

	// positive distance if inside and negative if outside
	return BaseIsSolid ? ClosestDistance : -ClosestDistance;
}



/** Called by the thread pool to do the work in this task */
void FTextureAlphaToDistanceField::FBuildDistanceFieldTask::DoWork(void)
{
	// Build the distance field for the strip specified for this task.
	for (INT y=StartRow; y < (StartRow+NumRowsToProcess); y++)
	{
		if ( y%16 == 0 )
		{
			// Update the user about our progress
			WorkRemainingCounter->Add(16);
		}

		// Build distance field for a single line
		for (INT x=0; x < DstRowWidth; x++)
		{
			SignedDistances(x + y * DstRowWidth) = CalcSignedDistanceToSrc( 
				(x * ScaleFactor) + (ScaleFactor / 2),
				(y * ScaleFactor) + (ScaleFactor / 2),
				ScanRadius );
		}
	}
	ThreadScaleCounter->Decrement();
}



#if __WIN32__
/**
 * Win32 Platform Only: Creates a single font texture using the Windows GDI
 *
 * @param Font (In/Out) The font we're operating with
 * @param dc The device context configured to paint this font
 * @param RowHeight Height of a font row in pixels
 * @param TextureNum The current texture index
 *
 * @return Returns the newly created texture, if successful, otherwise NULL
 */
UTexture2D* UTrueTypeFontFactory::CreateTextureFromDC( UFont* Font, HDC dc, INT Height, INT TextureNum )
{
	FString TextureString = FString::Printf( TEXT("%s_Page"), *Font->GetName() );
	if( TextureNum < 26 )
	{
		TextureString = TextureString + FString::Printf(TEXT("%c"), 'A'+TextureNum);
	}
	else
	{
		TextureString = TextureString + FString::Printf(TEXT("%c%c"), 'A'+TextureNum/26, 'A'+TextureNum%26 );
	}

 	if( StaticFindObject( NULL, Font, *TextureString ) )
	{
		warnf( TEXT("A texture named %s already exists!"), *TextureString );
	}
	
	INT BitmapWidth = ImportOptions->Data.TexturePageWidth;
	INT BitmapHeight = appRoundUpToPowerOfTwo(Height);
	if( ImportOptions->Data.bUseDistanceFieldAlpha )
	{
		// scale original bitmap width by scale factor to generate high res font
		// note that height is already scaled during font bitmap generation
		BitmapWidth *= ImportOptions->Data.DistanceFieldScaleFactor;
	}

	// Create texture for page.
	UTexture2D* Texture = new(Font, *TextureString)UTexture2D;

	// note RF_Public because font textures can be referenced directly by material expressions
	Texture->SetFlags(RF_Public);	
	Texture->Init( BitmapWidth, BitmapHeight, PF_A8R8G8B8 );

	// Copy the LODGroup from the font factory to the new texture
	// By default, this should be TEXTUREGROUP_UI for fonts!
	Texture->LODGroup = LODGroup;

	// Also, we never want to stream in font textures since that always looks awful
	Texture->NeverStream = TRUE;

	// Copy bitmap data to texture page.
	FColor FontColor8Bit( ImportOptions->Data.ForegroundColor );

	// restart progress bar for font bitmap generation since this takes a while
	INT TotalProgress = BitmapWidth-1;
	GWarn->StatusUpdatef(0,0,TEXT("Generating font page %d"),TextureNum);
	
	
	

	TArray<INT> SourceData;	
	// Copy the data from the Device Context to our SourceData array.
	// This approach is much faster than using GetPixel()!
	{
		// We must make a new bitmap to populate with data from the DC
		BITMAPINFO BitmapInfo;
		BitmapInfo.bmiHeader.biBitCount = 32;
		BitmapInfo.bmiHeader.biCompression = BI_RGB;
		BitmapInfo.bmiHeader.biPlanes = 1;
		BitmapInfo.bmiHeader.biSize = sizeof(BitmapInfo.bmiHeader);
		BitmapInfo.bmiHeader.biWidth = BitmapWidth;
		BitmapInfo.bmiHeader.biHeight = -BitmapHeight; // Having a positive height results in an upside-down bitmap

		// Initialize the Bitmap and the Device Context in a way that they are automatically cleaned up.
		struct CleanupResourcesScopeGuard
		{
			HDC BitmapDC;
			HBITMAP BitmapHandle;
			~CleanupResourcesScopeGuard()
			{
				DeleteDC( BitmapDC );
				DeleteObject( BitmapHandle );
			}
		} Resources;
		Resources.BitmapDC = CreateCompatibleDC(dc);
		Resources.BitmapHandle = CreateDIBSection(Resources.BitmapDC, &BitmapInfo, DIB_RGB_COLORS, 0, 0, 0);

		// Bind the bitmap to the Device Context
		SelectObject(Resources.BitmapDC, Resources.BitmapHandle);

		// Copy from the Device Context to the Bitmap we created
		BitBlt(Resources.BitmapDC, 0, 0, BitmapWidth, BitmapHeight, dc, 0, 0, SRCCOPY);

		// Finally copy the data from the Bitmap into a UE3 data array.
		SourceData.Add(BitmapWidth * BitmapHeight);
		GetDIBits( Resources.BitmapDC, Resources.BitmapHandle, 0, BitmapHeight, SourceData.GetData(), &BitmapInfo, DIB_RGB_COLORS );
	}

	BYTE* MipData = (BYTE*) Texture->Mips(0).Data.Lock(LOCK_READ_WRITE);
	if( !ImportOptions->Data.bEnableAntialiasing )
	{
		for( INT i=0; i<(INT)Texture->SizeX; i++ )
		{
			// update progress bar
			GWarn->UpdateProgress(i,TotalProgress);
			
			for( INT j=0; j<(INT)Texture->SizeY; j++ )
			{
				INT CharAlpha = SourceData(i+j*BitmapWidth);
				INT DropShadowAlpha;

				if( ImportOptions->Data.bEnableDropShadow && i > 0 && j >> 0 )
				{
					DropShadowAlpha = ( (i-1)+(j-1)*BitmapWidth);
				}
				else
				{
					DropShadowAlpha = 0;
				}

				if( CharAlpha )
				{
					MipData[4 * (i + j * Texture->SizeX) + 0] = FontColor8Bit.B;
					MipData[4 * (i + j * Texture->SizeX) + 1] = FontColor8Bit.G;
					MipData[4 * (i + j * Texture->SizeX) + 2] = FontColor8Bit.R;
					MipData[4 * (i + j * Texture->SizeX) + 3] = 0xFF;
				}
				else if( DropShadowAlpha )
				{
					MipData[4 * (i + j * Texture->SizeX) + 0] = 0x00;
					MipData[4 * (i + j * Texture->SizeX) + 1] = 0x00;
					MipData[4 * (i + j * Texture->SizeX) + 2] = 0x00;
					MipData[4 * (i + j * Texture->SizeX) + 3] = 0xFF;
				}
				else
				{
					MipData[4 * (i + j * Texture->SizeX) + 0] = FontColor8Bit.B;
					MipData[4 * (i + j * Texture->SizeX) + 1] = FontColor8Bit.G;
					MipData[4 * (i + j * Texture->SizeX) + 2] = FontColor8Bit.R;
					MipData[4 * (i + j * Texture->SizeX) + 3] = 0x00;
				}
			}
		}
	}
	else
	{
		for( INT i=0; i<(INT)Texture->SizeX; i++ )
		{
			for( INT j=0; j<(INT)Texture->SizeY; j++ )
			{
				INT CharAlpha = SourceData(i+j*BitmapWidth);
				FLOAT fCharAlpha = FLOAT( CharAlpha ) / 255.0f;

				INT DropShadowAlpha = 0;
				if( ImportOptions->Data.bEnableDropShadow && i > 0 && j > 0 )
				{
					// Character opacity takes precedence over drop shadow opacity
					DropShadowAlpha =
						( BYTE )( ( 1.0f - fCharAlpha ) * ( FLOAT )GetRValue( SourceData((i - 1)+(j - 1)*BitmapWidth) ) );
				}
				FLOAT fDropShadowAlpha = FLOAT( DropShadowAlpha ) / 255.0f;

				// Color channel = Font color, except for drop shadow pixels
				MipData[4 * (i + j * Texture->SizeX) + 0] = ( BYTE )( FontColor8Bit.B * ( 1.0f - fDropShadowAlpha ) );
				MipData[4 * (i + j * Texture->SizeX) + 1] = ( BYTE )( FontColor8Bit.G * ( 1.0f - fDropShadowAlpha ) );
				MipData[4 * (i + j * Texture->SizeX) + 2] = ( BYTE )( FontColor8Bit.R * ( 1.0f - fDropShadowAlpha ) );
				MipData[4 * (i + j * Texture->SizeX) + 3] = CharAlpha + DropShadowAlpha;
			}
		}
	}
	Texture->Mips(0).Data.Unlock();
	MipData = NULL;
	
	// convert bitmap font alpha channel to distance field
	if (ImportOptions->Data.bUseDistanceFieldAlpha)
	{
		// Initialize distance field generator with high res source bitmap texels
		FTextureAlphaToDistanceField DistanceFieldTex(
			(BYTE*)Texture->Mips(0).Data.Lock(LOCK_READ_ONLY), 
			Texture->SizeX, 
			Texture->SizeY,
			Texture->Format);
		// estimate scan radius based on half font height scaled by bitmap scale factor
		const INT ScanRadius = ImportOptions->Data.Height/2 * ImportOptions->Data.DistanceFieldScaleFactor * ImportOptions->Data.DistanceFieldScanRadiusScale;
		// generate downsampled distance field using high res source bitmap
		DistanceFieldTex.Generate(ImportOptions->Data.DistanceFieldScaleFactor,ScanRadius);
		check(DistanceFieldTex.GetResultTextureSize() > 0);
		Texture->Mips(0).Data.Unlock();	
		// resize/update texture using distance field values
		Texture->Init(DistanceFieldTex.GetResultSizeX(),DistanceFieldTex.GetResultSizeY(),(EPixelFormat)Texture->Format);
		appMemcpy(Texture->Mips(0).Data.Lock(LOCK_READ_WRITE),DistanceFieldTex.GetResultTexture(),DistanceFieldTex.GetResultTextureSize());		
		Texture->Mips(0).Data.Unlock();		
		// use PF_G8 for all distance field textures for better precision than DXT5
		Texture->CompressionSettings = TC_Displacementmap;
		// disable gamma correction since storing alpha in linear color for PF_G8
		Texture->SRGB = FALSE;
	}
	else
	{
		// if we dont care about colors then store texture as PF_G8
		if (ImportOptions->Data.bAlphaOnly &&
			!ImportOptions->Data.bEnableDropShadow)
		{
			// use PF_G8 for all distance field textures for better precision than DXT5
			Texture->CompressionSettings = TC_Displacementmap;
			// disable gamma correction since storing alpha in linear color for PF_G8
			Texture->SRGB = FALSE;
		}
	}	
	
	// Set the uncompressed source art
	Texture->SetUncompressedSourceArt(Texture->Mips(0).Data.Lock(LOCK_READ_ONLY), Texture->Mips(0).Data.GetBulkDataSize());
	Texture->Mips(0).Data.Unlock();

	// Compress source art.
	Texture->CompressSourceArt();

	Texture->MipGenSettings = TMGS_NoMipmaps;
	Texture->Compress();

	return Texture;
}
#endif



#if __WIN32__
/**
 * Win32 Platform Only: Imports a TrueType font
 *
 * @param Font (In/Out) The font object that we're importing into
 * @param Warn Feedback context for displaying warnings and errors
 * @param NumResolutions Number of resolution pages we should generate 
 * @param ResHeights Font height for each resolution (one array entry per resolution)
 *
 * @return TRUE if successful
 */
UBOOL UTrueTypeFontFactory::ImportTrueTypeFont(
	UFont* Font,
	FFeedbackContext* Warn,
	const INT NumResolutions,
	const TArray< FLOAT >& ResHeights )
{
	DOUBLE StartTime = appSeconds();

	Font->Kerning = ImportOptions->Data.Kerning;
	Font->IsRemapped = 0;

	// Zero out the Texture Index
	INT CurrentTexture = 0;

	TMap<TCHAR,TCHAR> InverseMap;

	const UBOOL UseFiles = ImportOptions->Data.CharsFileWildcard != TEXT("") && ImportOptions->Data.CharsFilePath != TEXT("");
	const UBOOL UseRange = ImportOptions->Data.UnicodeRange != TEXT("");
	const UBOOL UseSpecificText = (ImportOptions->Data.Chars.Len()>0);

	INT CharsPerPage = 0;
	if( UseFiles || UseRange || UseSpecificText)
	{
		Font->IsRemapped = 1;

		// Only include ASCII characters if we were asked to
		INT MinRangeCharacter = 0;
		if( ImportOptions->Data.bIncludeASCIIRange )
		{
			// Map (ASCII)
			for( TCHAR c=0;c<256;c++ )
			{
				Font->CharRemap.Set( c, c );
				InverseMap.Set( c, c );
			}

			MinRangeCharacter = 256;
		}

		TArray<BYTE> Chars;
		Chars.AddZeroed(65536);

		if( UseFiles  || UseSpecificText)
		{
			FString S;
			if (UseFiles)
			{
				// find all characters in specified path/wildcard
				TArray<FString> Files;
				GFileManager->FindFiles( Files, *(ImportOptions->Data.CharsFilePath*ImportOptions->Data.CharsFileWildcard),1,0 );
				for( TArray<FString>::TIterator it(Files); it; ++it )
				{
					FString FileText;
					verify(appLoadFileToString(FileText,*(ImportOptions->Data.CharsFilePath * *it)));
					S+=FileText;
				}
				warnf(TEXT("Checked %d files"), Files.Num() );
			}
			else
			{
				S = ImportOptions->Data.Chars;
			}
			for( INT i=0; i<S.Len(); i++ )
			{
				Chars((*S)[i]) = 1;
			}
		}

		if( UseRange )
		{
			Warn->Logf(TEXT("UnicodeRange <%s>:"), *ImportOptions->Data.UnicodeRange);
			INT From = 0;
			INT To = 0;
			UBOOL HadDash = 0;
			for( const TCHAR* C=*ImportOptions->Data.UnicodeRange; *C; C++ )
			{
				if( (*C>='A' && *C<='F') || (*C>='a' && *C<='f') || (*C>='0' && *C<='9') )
				{
					if( HadDash )
					{
						To = 16*To + FromHex(*C);
					}
					else
					{
						From = 16*From + FromHex(*C);
					}
				}
				else if( *C=='-' )
				{
					HadDash = 1;
				}
				else if( *C==',' )
				{
					warnf(TEXT("Adding unicode character range %x-%x (%d-%d)"),From,To,From,To);
					for( INT i=From;i<=To&&i>=0&&i<65536;i++ )
					{
						Chars(i) = 1;
					}
					HadDash=0;
					From=0;
					To=0;
				}
			}
			warnf(TEXT("Adding unicode character range %x-%x (%d-%d)"),From,To,From,To);
			for( INT i=From; i<=To && i>=0 && i<65536; i++ )
			{
				Chars(i) = 1;
			}

		}

		INT j=MinRangeCharacter;
		INT Min=65536, Max=0;
		for( INT i=MinRangeCharacter; i<65536; i++ )
		{
			if( Chars(i) )
			{
				if( i < Min )
				{
					Min = i;
				}
				if( i > Max )
				{
					Max = i;
				}

				Font->CharRemap.Set( i, j );
				InverseMap.Set( j++, i );
			}
		}

		warnf(TEXT("Importing %d characters (unicode range %04x-%04x)"), j, Min, Max);

		CharsPerPage = j;
	}
	else
	{
		// No range specified, so default to the ASCII range
		CharsPerPage = 256;
	}

	// Add space for characters.
	Font->Characters.AddZeroed(CharsPerPage * NumResolutions );
    
	// If all upper case chars have lower case char counterparts no mapping is required.   
	if( !Font->IsRemapped )
	{
		bool NeedToRemap = false;
        
		for( const TCHAR* p = *ImportOptions->Data.Chars; *p; p++ )
		{
			TCHAR c;
            
			if( !appIsAlpha( *p ) )
			{
				continue;
			}
            
			if( appIsUpper( *p ) )
			{
				c = appToLower( *p );
			}
			else
			{
				c = appToUpper( *p );
			}

			if( appStrchr(*ImportOptions->Data.Chars, c) )
			{
				continue;
			}
            
			NeedToRemap = true;
			break;
		}
        
		if( NeedToRemap )
		{
			Font->IsRemapped = 1;

			for( const TCHAR* p = *ImportOptions->Data.Chars; *p; p++ )
			{
				TCHAR c;

				if( !appIsAlpha( *p ) )
				{
					Font->CharRemap.Set( *p, *p );
					InverseMap.Set( *p, *p );
					continue;
				}
                
				if( appIsUpper( *p ) )
				{
					c = appToLower( *p );
				}
				else
				{
					c = appToUpper( *p );
				}

				Font->CharRemap.Set( *p, *p );
				InverseMap.Set( *p, *p );

				if( !appStrchr(*ImportOptions->Data.Chars, c) )
				{
					Font->CharRemap.Set( c, *p );
				}
			}
		}
	}


	// Get the Logical Pixels Per Inch to be used when calculating the height later
	HDC tempDC = CreateCompatibleDC( NULL );
	FLOAT LogicalPPIY = (FLOAT)GetDeviceCaps(tempDC, LOGPIXELSY) / 72.0;
	
	DeleteDC( tempDC );

	const INT TotalProgress = NumResolutions * CharsPerPage;

	DWORD ImportCharSet = DEFAULT_CHARSET;
	switch ( ImportOptions->Data.CharacterSet )
	{
		case FontICS_Ansi:
			ImportCharSet = ANSI_CHARSET;
			break;

		case FontICS_Default:
			ImportCharSet = DEFAULT_CHARSET;
			break;

		case FontICS_Symbol:
			ImportCharSet = SYMBOL_CHARSET;
			break;
	}

	for( INT Page = 0; Page < NumResolutions; ++Page )
	{
		INT nHeight = -appRound( ResHeights(Page) * LogicalPPIY );
		
		// scale font height to generate high res bitmap based on scale factor
		// this high res bitmap is later used to generate the downsampled distance field
		if (ImportOptions->Data.bUseDistanceFieldAlpha)
		{
			nHeight *= ImportOptions->Data.DistanceFieldScaleFactor;
		}

		// Create the Windows font
		HFONT FontHandle =
			CreateFont(
				nHeight,
				0,
				0,
				0,
				ImportOptions->Data.bEnableBold ? FW_BOLD : FW_NORMAL,
				ImportOptions->Data.bEnableItalic,
				ImportOptions->Data.bEnableUnderline,
				0,
				ImportCharSet,
				OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				ImportOptions->Data.bEnableAntialiasing ? ANTIALIASED_QUALITY : NONANTIALIASED_QUALITY,
				VARIABLE_PITCH,
				*ImportOptions->Data.FontName );

		if( FontHandle == NULL ) 
		{
			TCHAR ErrorBuffer[1024];
			Warn->Logf( NAME_Error, TEXT("CreateFont failed: %s"), appGetSystemErrorMessage(ErrorBuffer,1024) );
			return FALSE;
		}

		// Create DC
		HDC DeviceDCHandle = GetDC( NULL );
		if( DeviceDCHandle == NULL )
		{
			TCHAR ErrorBuffer[1024];
			Warn->Logf( NAME_Error, TEXT("GetDC failed: %s"), appGetSystemErrorMessage(ErrorBuffer,1024) );
			return FALSE;
		}

		HDC DCHandle = CreateCompatibleDC( DeviceDCHandle );
		if( !DCHandle )
		{
			TCHAR ErrorBuffer[1024];
			Warn->Logf( NAME_Error, TEXT("CreateDC failed: %s"), appGetSystemErrorMessage(ErrorBuffer,1024) );
			return FALSE;
		}

		// Create bitmap
		BITMAPINFO WinBitmapInfo;
		appMemzero( &WinBitmapInfo, sizeof( WinBitmapInfo ) );
		HBITMAP BitmapHandle;
		void* BitmapDataPtr = NULL;
		
		INT BitmapWidth = ImportOptions->Data.TexturePageWidth;
		INT BitmapHeight = ImportOptions->Data.TexturePageMaxHeight;
		INT BitmapPaddingX = ImportOptions->Data.XPadding;
		INT BitmapPaddingY = ImportOptions->Data.YPadding;

		// scale up bitmap dimensions by for distance field generation
		if( ImportOptions->Data.bUseDistanceFieldAlpha )
		{
			BitmapWidth *= ImportOptions->Data.DistanceFieldScaleFactor;
			BitmapHeight *= ImportOptions->Data.DistanceFieldScaleFactor;
			BitmapPaddingX *= ImportOptions->Data.DistanceFieldScaleFactor;
			BitmapPaddingY *= ImportOptions->Data.DistanceFieldScaleFactor;
		}

		if( ImportOptions->Data.bEnableAntialiasing )
		{
			WinBitmapInfo.bmiHeader.biSize          = sizeof(BITMAPINFOHEADER);			
			WinBitmapInfo.bmiHeader.biWidth         = ImportOptions->Data.TexturePageWidth;
			WinBitmapInfo.bmiHeader.biHeight        = ImportOptions->Data.TexturePageMaxHeight;
			WinBitmapInfo.bmiHeader.biPlanes        = 1;      //  Must be 1
			WinBitmapInfo.bmiHeader.biBitCount      = 32;
			WinBitmapInfo.bmiHeader.biCompression   = BI_RGB; 
			WinBitmapInfo.bmiHeader.biSizeImage     = 0;      
			WinBitmapInfo.bmiHeader.biXPelsPerMeter = 0;      
			WinBitmapInfo.bmiHeader.biYPelsPerMeter = 0;      
			WinBitmapInfo.bmiHeader.biClrUsed       = 0;      
			WinBitmapInfo.bmiHeader.biClrImportant  = 0;      

			BitmapHandle = CreateDIBSection(
				(HDC)NULL, 
				&WinBitmapInfo,
				DIB_RGB_COLORS,
				&BitmapDataPtr,
				NULL,
				0);  
		}
		else
		{
			BitmapHandle = CreateBitmap( BitmapWidth, BitmapHeight, 1, 1, NULL);
		}

		if( BitmapHandle == NULL )
		{
			TCHAR ErrorBuffer[1024];
			Warn->Logf( NAME_Error, TEXT("CreateBitmap failed: %s"), appGetSystemErrorMessage(ErrorBuffer,1024) );
			return FALSE;
		}

		SelectObject( DCHandle, FontHandle );

		// Grab size information for this font
		TEXTMETRIC WinTextMetrics;
		GetTextMetrics( DCHandle, &WinTextMetrics );

        FLOAT EmScale = 1024.f/-nHeight;
        Font->EmScale = EmScale;
        if (ImportOptions->Data.bUseDistanceFieldAlpha)
            Font->EmScale *= ImportOptions->Data.DistanceFieldScaleFactor;
        Font->Ascent = WinTextMetrics.tmAscent * EmScale;
        Font->Descent = WinTextMetrics.tmDescent * EmScale;
        Font->Leading = WinTextMetrics.tmExternalLeading * EmScale;

		HBITMAP LastBitmapHandle = ( HBITMAP )SelectObject( DCHandle, BitmapHandle );
		SetTextColor( DCHandle, 0x00ffffff );
		SetBkColor( DCHandle, 0x00000000 );

		// clear the bitmap
		HBRUSH Black = CreateSolidBrush(0x00000000);
		RECT r = {0, 0, BitmapWidth, BitmapHeight};
		FillRect( DCHandle, &r, Black );

		INT X=BitmapPaddingX, Y=BitmapPaddingY, RowHeight=0;

		for( INT CurCharIndex = 0; CurCharIndex < CharsPerPage; ++CurCharIndex )
		{
  			GWarn->UpdateProgress( Page * CharsPerPage + CurCharIndex, TotalProgress );

			// Remap the character if we need to
			TCHAR Char = ( TCHAR )CurCharIndex;
			if( Font->IsRemapped )
			{
				TCHAR* FoundRemappedChar = InverseMap.Find( Char );
				if( FoundRemappedChar != NULL )
				{
					Char = *FoundRemappedChar;
				}
				else
				{
					// Skip missing remapped character
					continue;
				}
			}


			// Skip ASCII character if it isn't in the list of characters to import.
			if( Char < 256 && ImportOptions->Data.Chars != TEXT("") && (!Char || !appStrchr(*ImportOptions->Data.Chars, Char)) )
			{
				continue;
			}

			// Skip if the user has requested that only printable characters be
			// imported and the character isn't printable
			if( ImportOptions->Data.bCreatePrintableOnly == TRUE && iswprint(Char) == FALSE )
			{
				continue;
			}

			
			// Compute the size of the character
			INT CharWidth = 0;
			INT CharHeight = 0;
			{
				TCHAR Tmp[2];
				Tmp[0] = Char;
				Tmp[1] = 0;

				SIZE Size;
				GetTextExtentPoint32( DCHandle, Tmp, 1, &Size );

				CharWidth = Size.cx;
				CharHeight = Size.cy;
			}
			
			
			// OK, now try to grab glyph data using the GetGlyphOutline API.  This is only supported for vector-based fonts
			// like TrueType and OpenType; it won't work for raster fonts!
			UBOOL bUsingGlyphOutlines = FALSE;
			GLYPHMETRICS WinGlyphMetrics;
			const MAT2 WinIdentityMatrix2x2 = { 0,1, 0,0, 0,0, 0,1 };
			INT VerticalOffset = 0;
			UINT GGODataSize = 0;
			if( !ImportOptions->Data.bEnableLegacyMode && ImportOptions->Data.bEnableAntialiasing )    // We only bother using GetGlyphOutline for AntiAliased fonts!
			{
				GGODataSize =
					GetGlyphOutlineW(
						DCHandle,                         // Device context
						Char,	                            // Character
						GGO_GRAY8_BITMAP,                 // Format
						&WinGlyphMetrics,                 // Out: Metrics
						0,																// Output buffer size
						NULL,															// Glyph data buffer or NULL
						&WinIdentityMatrix2x2 );          // Transform

				if( GGODataSize != GDI_ERROR && GGODataSize != 0)
				{
					CharWidth = WinGlyphMetrics.gmBlackBoxX;
					CharHeight = WinGlyphMetrics.gmBlackBoxY;

					VerticalOffset = WinTextMetrics.tmAscent - WinGlyphMetrics.gmptGlyphOrigin.y;

					// Extend the width of the character by 1 (empty) pixel for spacing purposes.  Note that with the legacy
					// font import, we got this "for free" from TextOut
					// @todo frick: Properly support glyph origins and cell advancement!  The only reason we even want to
					//    to continue to do this is to prevent texture bleeding across glyph cell UV rects
					++CharWidth;

					bUsingGlyphOutlines = TRUE;
				}
				else
				{
					// GetGlyphOutline failed; it's probably a raster font.  Oh well, no big deal.
				}
			}

			// Adjust character dimensions to accommodate a drop shadow
			if( ImportOptions->Data.bEnableDropShadow )
			{
				CharWidth += 1;
				CharHeight += 1;
			}
			if (ImportOptions->Data.bUseDistanceFieldAlpha)
			{
				
				// Make X and Y positions a multiple of the scale factor.
				CharWidth = appRound(CharWidth / static_cast<FLOAT>(ImportOptions->Data.DistanceFieldScaleFactor)) * ImportOptions->Data.DistanceFieldScaleFactor;
				CharHeight = appRound(CharHeight / static_cast<FLOAT>(ImportOptions->Data.DistanceFieldScaleFactor)) * ImportOptions->Data.DistanceFieldScaleFactor;
			}

			// If the character is bigger than our texture size, then this isn't going to work!  The user
			// will need to specify a larger texture resolution
			if( CharWidth > BitmapWidth ||
				CharHeight > BitmapHeight )
			{
				warnf( TEXT( "At the specified font size, at least one font glyph would be larger than the maximum texture size you specified.") );
				DeleteDC( DCHandle );
				DeleteObject( BitmapHandle );
				return FALSE;
			}

			// If it doesn't fit right here, advance to next line.
			if( CharWidth + X + 2 > BitmapWidth)
			{
				X=BitmapPaddingX;
				Y = Y + RowHeight + BitmapPaddingY;
				RowHeight = 0;
			}
			INT OldRowHeight = RowHeight;
			if( CharHeight > RowHeight )
			{
				RowHeight = CharHeight;
			}

			// new page
			if( Y+RowHeight > BitmapHeight )
			{
				Font->Textures.AddItem( CreateTextureFromDC( Font, DCHandle, Y+OldRowHeight, CurrentTexture ) );
				CurrentTexture++;

				// blank out DC
				HBRUSH Black = CreateSolidBrush(0x00000000);
				RECT r = {0, 0, BitmapWidth, BitmapHeight};
				FillRect( DCHandle, &r, Black );

				X=BitmapPaddingX;
				Y=BitmapPaddingY;
				
				RowHeight = 0;
			}

			// NOTE: This extra offset is for backwards compatibility with legacy TT/raster fonts.  With the new method
			//   of importing fonts, this offset is not needed since the glyphs already have the correct vertical size
			const INT ExtraVertOffset = bUsingGlyphOutlines ? 0 : 1;

			// Set font character information.
			FFontCharacter& NewCharacterRef = Font->Characters( CurCharIndex + (CharsPerPage * Page));
			INT FontX = X;
			INT FontY = Y;
			INT FontWidth = CharWidth;
			INT FontHeight = CharHeight;
			// scale character offset UVs back down based on scale factor
			if (ImportOptions->Data.bUseDistanceFieldAlpha)
			{
				FontX = appRound(FontX / (FLOAT)ImportOptions->Data.DistanceFieldScaleFactor);
				FontY = appRound(FontY / (FLOAT)ImportOptions->Data.DistanceFieldScaleFactor);
				FontWidth = appRound(FontWidth / (FLOAT)ImportOptions->Data.DistanceFieldScaleFactor);
				FontHeight = appRound(FontHeight / (FLOAT)ImportOptions->Data.DistanceFieldScaleFactor);
			}
			NewCharacterRef.StartU =
				Clamp<INT>( FontX - ImportOptions->Data.ExtendBoxLeft,
							0, ImportOptions->Data.TexturePageWidth - 1 );
			NewCharacterRef.StartV =
				Clamp<INT>( FontY + ExtraVertOffset-ImportOptions->Data.ExtendBoxTop,
							0, ImportOptions->Data.TexturePageMaxHeight - 1 );
			NewCharacterRef.USize =
				Clamp<INT>( FontWidth + ImportOptions->Data.ExtendBoxLeft + ImportOptions->Data.ExtendBoxRight,
							0, ImportOptions->Data.TexturePageWidth - NewCharacterRef.StartU );
			NewCharacterRef.VSize =
				Clamp<INT>( FontHeight + ImportOptions->Data.ExtendBoxTop + ImportOptions->Data.ExtendBoxBottom,
							0, ImportOptions->Data.TexturePageMaxHeight - NewCharacterRef.StartV );
			NewCharacterRef.TextureIndex = CurrentTexture;
			NewCharacterRef.VerticalOffset = VerticalOffset;

			// Draw character into font and advance.
			if( bUsingGlyphOutlines )
			{
				// GetGlyphOutline requires at least a DWORD aligned address
				BYTE* AlignedGlyphData = ( BYTE* )appMalloc( GGODataSize, DEFAULT_ALIGNMENT );
				check( AlignedGlyphData != NULL );

				// Grab the actual glyph bitmap data
				GetGlyphOutlineW(
					DCHandle,                         // Device context
					Char,	                            // Character
					GGO_GRAY8_BITMAP,                 // Format
					&WinGlyphMetrics,                 // Out: Metrics
					GGODataSize,                      // Data size
					AlignedGlyphData,                 // Out: Glyph data (aligned)
					&WinIdentityMatrix2x2 );          // Transform

				// Make sure source pitch is DWORD aligned
				INT SourceDataPitch = WinGlyphMetrics.gmBlackBoxX;
				if( SourceDataPitch % 4 != 0 )
				{
					SourceDataPitch += 4 - SourceDataPitch % 4;
				}
				BYTE* SourceDataPtr = AlignedGlyphData;

				const INT DestDataPitch = WinBitmapInfo.bmiHeader.biWidth * WinBitmapInfo.bmiHeader.biBitCount / 8;
				BYTE* DestDataPtr = ( BYTE* )BitmapDataPtr;
				check( DestDataPtr != NULL );

				// We're going to write directly to the bitmap, so we'll unbind it from the GDI first
				SelectObject( DCHandle, LastBitmapHandle );

				// Copy the glyph data to our bitmap!
				for( UINT SourceY = 0; SourceY < WinGlyphMetrics.gmBlackBoxY; ++SourceY )
				{
					for( UINT SourceX = 0; SourceX < WinGlyphMetrics.gmBlackBoxX; ++SourceX )
					{
						// Values are between 0 and 64 inclusive (64 possible shades, including black)
						BYTE Opacity = ( BYTE )( ( ( INT )SourceDataPtr[ SourceY * SourceDataPitch + SourceX ] * 255 ) / 64 );

						// Copy the opacity value into the RGB components of the bitmap, since that's where we'll be looking for them
						// NOTE: Alpha channel is set to zero
						const UINT DestX = X + SourceX;
						const UINT DestY = WinBitmapInfo.bmiHeader.biHeight - ( Y + SourceY ) - 1;  // Image is upside down!
						*( UINT* )&DestDataPtr[ DestY * DestDataPitch + DestX * sizeof( UINT ) ] =
							( Opacity ) |            // B
							( Opacity << 8 ) |       // G
							( Opacity << 16 );       // R
					}
				}

				// OK, we can rebind it now!
				SelectObject( DCHandle, BitmapHandle );

				appFree( AlignedGlyphData );
			}
			else
			{
				TCHAR Tmp[2];
				Tmp[0] = Char;
				Tmp[1] = 0;

				TextOut( DCHandle, X, Y, Tmp, 1 );
				
				GWarn->Logf(TEXT("OutPutGlyph X=%d Y=%d FontHeight=%d FontWidth=%d Char=%04x U=%d V=%d =Usize=%d VSIze=%d"), 
					X,Y,FontHeight,FontWidth,Char,
					NewCharacterRef.StartU ,
					NewCharacterRef.StartV ,
					NewCharacterRef.USize,
					NewCharacterRef.VSize);
			}
			X = X + CharWidth + BitmapPaddingX;
		}
		// save final page
		Font->Textures.AddItem( CreateTextureFromDC( Font, DCHandle, Y+RowHeight, CurrentTexture ) );
		CurrentTexture++;

		DeleteDC( DCHandle );
		DeleteObject( BitmapHandle );
	}

	// Store character count
	Font->CacheCharacterCountAndMaxCharHeight();

	GWarn->UpdateProgress( TotalProgress, TotalProgress );
	
	DOUBLE EndTime = appSeconds();
	GWarn->Logf(TEXT("ImportTrueTypeFont: Total Time %0.2f"), EndTime - StartTime);

	return TRUE;	
}
#endif



void UTrueTypeMultiFontFactory::StaticConstructor()
{
	new(GetClass()->HideCategories) FName(NAME_Object);

	UArrayProperty*	RTProp = new(GetClass(),TEXT("ResTests"),	  RF_Public)UArrayProperty(CPP_PROPERTY(ResTests  ), TEXT(""), CPF_Edit );
	check(RTProp);
	RTProp->Inner = new(RTProp,TEXT("FloatProperty0"),RF_Public) UFloatProperty(EC_CppProperty,0,TEXT(""),CPF_Edit);

	UArrayProperty* RHProp = new(GetClass(),TEXT("ResHeights"), RF_Public)UArrayProperty(CPP_PROPERTY(ResHeights), TEXT(""), CPF_Edit );
	check(RHProp);
	RHProp->Inner = new(RHProp, TEXT("FloatProperty0"), RF_Public) UFloatProperty(EC_CppProperty,0,TEXT(""),CPF_Edit);

	UArrayProperty* RFProp = new(GetClass(),TEXT("ResFonts"), RF_Public)UArrayProperty(CPP_PROPERTY(ResFonts), TEXT(""), CPF_Edit );
	check(RFProp);
	RFProp->Inner = new(RFProp, TEXT("ObjectProperty0"), RF_Public) UObjectProperty( EC_CppProperty,0,TEXT(""),CPF_Edit, UFont::StaticClass() );
}



/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTrueTypeMultiFontFactory::InitializeIntrinsicPropertyValues()
{
	Super::InitializeIntrinsicPropertyValues();
	SupportedClass = UMultiFont::StaticClass();
	Description = TEXT("MultiFont Imported From TrueType");
}



UTrueTypeMultiFontFactory::UTrueTypeMultiFontFactory()
{
}



UObject* UTrueTypeMultiFontFactory::FactoryCreateNew(
	UClass* Class,
	UObject* InParent,
	FName Name,
	EObjectFlags Flags,
	UObject* Context,
	FFeedbackContext*	Warn )
{
#if !__WIN32__
	STUBBED("Win32 TTF code");
	return NULL;
#else
	check(Class==UMultiFont::StaticClass());

	// Make sure we are properly setup
	if (ResTests.Num() == 0)
	{
		Warn->Logf(NAME_Error, TEXT("Could not create fonts: At least 1 resolution test is required"));
		return NULL;
	}
	if (ResFonts.Num() > 0)
	{
		if ( ResTests.Num() != ResFonts.Num() )
		{
			Warn->Logf( NAME_Error, TEXT("Could not combine fonts: Resolution Tests must equal Heights & Fonts"));
			return NULL;
		}
	}
	else if ( ResTests.Num() != ResHeights.Num() )
	{
		Warn->Logf( NAME_Error, TEXT("Could not create fonts: Resolution Tests must equal Heights/Fonts"));
		return NULL;
	}

	// Create font and its texture.
	UMultiFont* Font = new( InParent, Name, Flags )UMultiFont;

	// Copy the Resolution Tests
	Font->ResolutionTestTable = ResTests;



	// Check to see if we are converting fonts, or creating a new font
	if ( ResFonts.Num() > 0 )		// <--- Converting
	{
		// Zero out the Texture Index
		INT CurrentTexture = 0;

		// Copy the font information
		Font->Kerning = ResFonts(0)->Kerning;
		Font->IsRemapped = ResFonts(0)->IsRemapped;
		Font->CharRemap = ResFonts(0)->CharRemap;

		INT CharIndex = 0;

		// Process each font
		for ( INT Fnt = 0; Fnt < ResFonts.Num() ; Fnt++ )
		{
			if (!Cast<UMultiFont>( ResFonts(Fnt) ))
			{
				// Make duplicates of the Textures	
				for (INT i=0;i <ResFonts(Fnt)->Textures.Num();i++)
				{
					FString TextureString = FString::Printf( TEXT("%s_Page"), *Font->GetName() );

					if( CurrentTexture < 26 )
					{
						TextureString = TextureString + FString::Printf(TEXT("%c"), 'A'+CurrentTexture);
					}
					else
					{
						TextureString = TextureString + FString::Printf(TEXT("%c%c"), 'A'+CurrentTexture/26, 'A'+CurrentTexture%26 );
					}

					UTexture2D* FontTex = Cast<UTexture2D>( StaticDuplicateObject(ResFonts(Fnt)->Textures(i),ResFonts(Fnt)->Textures(i), Font, *TextureString));
					Font->Textures.AddItem(FontTex);
				}

				// Now duplicate the characters and fix up their references
				Font->Characters.AddZeroed( ResFonts(Fnt)->Characters.Num() );
				for (INT i=0;i<ResFonts(Fnt)->Characters.Num();i++)
				{
					Font->Characters(CharIndex) = ResFonts(Fnt)->Characters(i);
					Font->Characters(CharIndex).TextureIndex += CurrentTexture;
					CharIndex++;
				}

				CurrentTexture += ResFonts(Fnt)->Textures.Num();
			}
			else
			{
				Warn->Logf( NAME_Error, TEXT("Could not process %s because it's already a multifont.. Skipping!"),*ResFonts(Fnt)->GetFullName() );
			}
		}
	}
	else
	{
		// OK, we're creating a new font!

		GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT( "FontFactory_ImportingTrueTypeFont" ) ), TRUE );

		// For multi-resolution fonts, we'll pass in our per-resolution height array
		if( !ImportTrueTypeFont( Font, Warn, ResTests.Num(), ResHeights ) )
		{
			// Error importing font
			Font = NULL;
		}			

		GWarn->EndSlowTask();
	}

	return Font;
#endif
}
