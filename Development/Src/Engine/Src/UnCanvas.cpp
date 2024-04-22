/*=============================================================================
	UnCanvas.cpp: Unreal canvas rendering.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"
#include "ScenePrivate.h"
#include "TileRendering.h"
#include "EngineMaterialClasses.h"
#include "Localization.h"

#if DWTRIOVIZSDK
	#include "DwTrioviz/DwTriovizImpl.h"
#endif

IMPLEMENT_CLASS(UCanvas);

DECLARE_STATS_GROUP(TEXT("Canvas"),STATGROUP_Canvas);
DECLARE_CYCLE_STAT(TEXT("Flush Time"),STAT_Canvas_FlushTime,STATGROUP_Canvas);
DECLARE_CYCLE_STAT(TEXT("Draw Texture Tile Time"),STAT_Canvas_DrawTextureTileTime,STATGROUP_Canvas);
DECLARE_CYCLE_STAT(TEXT("Draw Material Tile Time"),STAT_Canvas_DrawMaterialTileTime,STATGROUP_Canvas);
DECLARE_CYCLE_STAT(TEXT("Draw String Time"),STAT_Canvas_DrawStringTime,STATGROUP_Canvas);
DECLARE_CYCLE_STAT(TEXT("Get Batched Element Time"),STAT_Canvas_GetBatchElementsTime,STATGROUP_Canvas);
DECLARE_CYCLE_STAT(TEXT("Add Material Tile Time"),STAT_Canvas_AddTileRenderTime,STATGROUP_Canvas);
DECLARE_DWORD_COUNTER_STAT(TEXT("Num Batches Created"),STAT_Canvas_NumBatchesCreated,STATGROUP_Canvas);


/**
 * Convert a depth value into a left/right shift, for stereoscopic rendering of 2 canvas's,
 * onhe for left eye, one for right eye
 */
static FLOAT GetStereoscopicDisparity(FLOAT Z)
{
	FLOAT Disparity = 0.0f;
#if DWTRIOVIZSDK
	if (Z > 0.95f)
	{
		if(DwTriovizImpl_IsTriovizActive())
		{
			if(DwTriovizImpl_IsSideBySide())
			{
				// turns out depths .95-1.0 are the interesting ones
				const FLOAT ZMin = 0.95f;
				const FLOAT InvZMin = 1.0f / (1.0f - ZMin);
				FLOAT MaxDisparity = 2.0f * DwTriovizImpl_GetCurrentProfileMultiplier() * Z; // actually depends of screensize but we'll make it depend of multiplier instead
				FLOAT Delta = 2.0f * DwTriovizImpl_GetCurrentProfileMultiplier();			 // depends of Trioviz multiplier 0.1f is a magic number, depends of the values 
																							 // of Z you intend to pass on this function
				Disparity = (((Z - ZMin) * InvZMin) * ((Z - ZMin) * InvZMin) * Delta) * Z;
				Disparity = Clamp<FLOAT>(Disparity, 0.0f, MaxDisparity);
				
				if(!DwTriovizImpl_IsRenderingLeftEye())
				{
					Disparity = -Disparity;
				}
			}
		}
	}
#endif
	return Disparity;
}

FCanvas::FCanvas(FRenderTarget* InRenderTarget,FHitProxyConsumer* InHitProxyConsumer)
:	RenderTarget(InRenderTarget)
,	bEnableDepthTest(FALSE)
,	bRenderTargetDirty(FALSE)
,	HitProxyConsumer(InHitProxyConsumer)
,	AllowedModes(0xFFFFFFFF)
{
	check(RenderTarget);
	// Push the viewport transform onto the stack.  Default to using a 2D projection. 
	new(TransformStack) FTransformEntry( 
		FMatrix( CalcBaseTransform2D(RenderTarget->GetSizeX(),RenderTarget->GetSizeY()) ) 
		);
	// init alpha to 1
	AlphaModulate=1.0;

	// init the cached time to 0
	CachedTime=0.f;

	// make sure the LastElementIndex is invalid
	LastElementIndex = INDEX_NONE;

	// init sort key to 0
	PushDepthSortKey(0);
}

/**
* Replace the base (ie. TransformStack(0)) transform for the canvas with the given matrix
*
* @param Transform - The transform to use for the base
*/
void FCanvas::SetBaseTransform(const FMatrix& Transform)
{
	// set the base transform
	if( TransformStack.Num() > 0 )
	{
		TransformStack(0).SetMatrix(Transform);
	}
	else
	{
		new(TransformStack) FTransformEntry(Transform);
	}
}

/**
* Generate a 2D projection for the canvas. Use this if you only want to transform in 2D on the XY plane
*
* @param ViewSizeX - Viewport width
* @param ViewSizeY - Viewport height
* @return Matrix for canvas projection
*/
FMatrix FCanvas::CalcBaseTransform2D(UINT ViewSizeX, UINT ViewSizeY)
{
	return 
		FTranslationMatrix(FVector(-GPixelCenterOffset,-GPixelCenterOffset,0)) *
		FMatrix(
			FPlane(	1.0f / (ViewSizeX / 2.0f),	0.0,										0.0f,	0.0f	),
			FPlane(	0.0f,						-1.0f / (ViewSizeY / 2.0f),					0.0f,	0.0f	),
			FPlane(	0.0f,						0.0f,										1.0f,	0.0f	),
			FPlane(	-1.0f,						1.0f,										0.0f,	1.0f	)
			);
}

/**
* Generate a 3D projection for the canvas. Use this if you want to transform in 3D 
*
* @param ViewSizeX - Viewport width
* @param ViewSizeY - Viewport height
* @param fFOV - Field of view for the projection
* @param NearPlane - Distance to the near clip plane
* @return Matrix for canvas projection
*/
FMatrix FCanvas::CalcBaseTransform3D(UINT ViewSizeX, UINT ViewSizeY, FLOAT fFOV, FLOAT NearPlane)
{
	FMatrix ViewMat(CalcViewMatrix(ViewSizeX,ViewSizeY,fFOV));
	FMatrix ProjMat(CalcProjectionMatrix(ViewSizeX,ViewSizeY,fFOV,NearPlane));
	return ViewMat * ProjMat;
}

/**
* Generate a view matrix for the canvas. Used for CalcBaseTransform3D
*
* @param ViewSizeX - Viewport width
* @param ViewSizeY - Viewport height
* @param fFOV - Field of view for the projection
* @return Matrix for canvas view orientation
*/
FMatrix FCanvas::CalcViewMatrix(UINT ViewSizeX, UINT ViewSizeY, FLOAT fFOV)
{
	// convert FOV to randians
	FLOAT FOVRad = fFOV * (FLOAT)PI / 360.0f;
	// move camera back enough so that the canvas items being rendered are at the same screen extents as regular canvas 2d rendering	
	FTranslationMatrix CamOffsetMat(-FVector(0,0,-appTan(FOVRad)*ViewSizeX/2));
	// adjust so that canvas items render as if they start at [0,0] upper left corner of screen 
	// and extend to the lower right corner [ViewSizeX,ViewSizeY]. 
	FMatrix OrientCanvasMat(
		FPlane(	1.0f,				0.0f,				0.0f,	0.0f	),
		FPlane(	0.0f,				-1.0f,				0.0f,	0.0f	),
		FPlane(	0.0f,				0.0f,				1.0f,	0.0f	),
		FPlane(	ViewSizeX * -0.5f,	ViewSizeY * 0.5f,	0.0f, 1.0f		)
		);
	return 
		// also apply screen offset to align to pixel centers
		FTranslationMatrix(FVector(-GPixelCenterOffset,-GPixelCenterOffset,0)) * 
		OrientCanvasMat * 
		CamOffsetMat;
}

/**
* Generate a projection matrix for the canvas. Used for CalcBaseTransform3D
*
* @param ViewSizeX - Viewport width
* @param ViewSizeY - Viewport height
* @param fFOV - Field of view for the projection
* @param NearPlane - Distance to the near clip plane
* @return Matrix for canvas projection
*/
FMatrix FCanvas::CalcProjectionMatrix(UINT ViewSizeX, UINT ViewSizeY, FLOAT fFOV, FLOAT NearPlane)
{
	// convert FOV to randians
	FLOAT FOVRad = fFOV * (FLOAT)PI / 360.0f;
	// project based on the FOV and near plane given
	return FPerspectiveMatrix(
		FOVRad,
		ViewSizeX,
		ViewSizeY,
		NearPlane
		);
}

/**
* Base interface for canvas items which can be batched for rendering
*/
class FCanvasBaseRenderItem
{
public:
	virtual ~FCanvasBaseRenderItem()
	{}

	/**
	* Renders the canvas item
	*
	* @param Canvas - canvas currently being rendered
	* @return TRUE if anything rendered
	*/
	virtual UBOOL Render( const FCanvas* Canvas ) =0;
	/**
	* FCanvasBatchedElementRenderItem instance accessor
	*
	* @return FCanvasBatchedElementRenderItem instance
	*/
	virtual class FCanvasBatchedElementRenderItem* GetCanvasBatchedElementRenderItem() { return NULL; }
	/**
	* FCanvasTileRendererItem instance accessor
	*
	* @return FCanvasTileRendererItem instance
	*/
	virtual class FCanvasTileRendererItem* GetCanvasTileRendererItem() { return NULL; }
};

/**
* Info needed to render a batched element set
*/
class FCanvasBatchedElementRenderItem : public FCanvasBaseRenderItem
{
public:
	/** 
	* Init constructor 
	*/
	FCanvasBatchedElementRenderItem(
		FBatchedElementParameters* InBatchedElementParameters=NULL,
		const FTexture* InTexture=NULL,
		ESimpleElementBlendMode InBlendMode=SE_BLEND_MAX,
		FCanvas::EElementType InElementType=FCanvas::ET_MAX,
		const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity),
		const FDepthFieldGlowInfo& InGlowInfo=FDepthFieldGlowInfo(EC_EventParm) )
		// this data is deleted after rendering has completed
		: Data(new FRenderData(InBatchedElementParameters, InTexture, InBlendMode, InElementType, InTransform, InGlowInfo))
	{}

	/**
	* Destructor to delete data in case nothing rendered
	*/
	virtual ~FCanvasBatchedElementRenderItem()
	{
		delete Data;
	}

	/**
	* FCanvasBatchedElementRenderItem instance accessor
	*
	* @return this instance
	*/
	virtual class FCanvasBatchedElementRenderItem* GetCanvasBatchedElementRenderItem() 
	{ 
		return this; 
	}

	/**
	* Renders the canvas item. 
	* Iterates over all batched elements and draws them with their own transforms
	*
	* @param Canvas - canvas currently being rendered
	* @return TRUE if anything rendered
	*/
	virtual UBOOL Render( const FCanvas* Canvas );

	/**
	* Determine if this is a matching set by comparing texture,blendmode,elementype,transform. All must match
	*
	* @param BatchedElementParameters - parameters for this batched element
	* @param InTexture - texture resource for the item being rendered
	* @param InBlendMode - current alpha blend mode 
	* @param InElementType - type of item being rendered: triangle,line,etc
	* @param InTransform - the transform for the item being rendered
	* @param InGlowInfo - the depth field glow of the item being rendered
	* @return TRUE if the parameters match this render item
	*/
	UBOOL IsMatch(FBatchedElementParameters* BatchedElementParameters, const FTexture* InTexture, ESimpleElementBlendMode InBlendMode, FCanvas::EElementType InElementType, const FCanvas::FTransformEntry& InTransform, const FDepthFieldGlowInfo& InGlowInfo)
	{
		return(	Data->BatchedElementParameters.GetReference() == BatchedElementParameters &&
				Data->Texture == InTexture &&
				Data->BlendMode == InBlendMode &&
				Data->ElementType == InElementType &&
				Data->Transform.GetMatrixCRC() == InTransform.GetMatrixCRC() &&
				Data->GlowInfo == InGlowInfo );
	}

	/**
	* Accessor for the batched elements. This can be used for adding triangles and primitives to the batched elements
	*
	* @return pointer to batched elements struct
	*/
	FORCEINLINE FBatchedElements* GetBatchedElements()
	{
		return &Data->BatchedElements;
	}

private:
	class FRenderData
	{
	public:
		/**
		* Init constructor
		*/
		FRenderData(
			FBatchedElementParameters* InBatchedElementParameters=NULL,
			const FTexture* InTexture=NULL,
			ESimpleElementBlendMode InBlendMode=SE_BLEND_MAX,
			FCanvas::EElementType InElementType=FCanvas::ET_MAX,
			const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity),
			const FDepthFieldGlowInfo& InGlowInfo=FDepthFieldGlowInfo(EC_EventParm) )
			:	BatchedElementParameters(InBatchedElementParameters)
			,	Texture(InTexture)
			,	BlendMode(InBlendMode)
			,	ElementType(InElementType)
			,	Transform(InTransform)
			,	GlowInfo(InGlowInfo)
		{}
		/** Current batched elements, destroyed once rendering completes. */
		FBatchedElements BatchedElements;
		/** Batched element parameters */
		TRefCountPtr<FBatchedElementParameters> BatchedElementParameters;
		/** Current texture being used for batching, set to NULL if it hasn't been used yet. */
		const FTexture* Texture;
		/** Current blend mode being used for batching, set to BLEND_MAX if it hasn't been used yet. */
		ESimpleElementBlendMode BlendMode;
		/** Current element type being used for batching, set to ET_MAX if it hasn't been used yet. */
		FCanvas::EElementType ElementType;
		/** Transform used to render including projection */
		FCanvas::FTransformEntry Transform;
		/** info for optional glow effect when using depth field rendering */
		FDepthFieldGlowInfo GlowInfo;
	};
	/**
	* Render data which is allocated when a new FCanvasBatchedElementRenderItem is added for rendering.
	* This data is only freed on the rendering thread once the item has finished rendering
	*/
	FRenderData* Data;		
};

/**
* Renders the canvas item. 
* Iterates over all batched elements and draws them with their own transforms
*
* @param Canvas - canvas currently being rendered
* @return TRUE if anything rendered
*/
UBOOL FCanvasBatchedElementRenderItem::Render( const FCanvas* Canvas )
{	
	checkSlow(Data);
	UBOOL bDirty=FALSE;		
	if( Data->BatchedElements.HasPrimsToDraw() )
	{
		bDirty = TRUE;

		// current render target set for the canvas
		const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
		FLOAT Gamma = 1.0f / CanvasRenderTarget->GetDisplayGamma();
		if ( Data->Texture && Data->Texture->bIgnoreGammaConversions )
		{
			Gamma = 1.0f;
		}

		// this allows us to use FCanvas operations from the rendering thread (ie, render subtitles
		// on top of a movie that is rendered completely in rendering thread)
		if (IsInRenderingThread())
		{
			SCOPED_DRAW_EVENT(EventUIBatchFromRT)(DEC_SCENE_ITEMS,TEXT("UI Texture Draw [RT]"));
			// draw batched items
			Data->BatchedElements.Draw(
				Data->Transform.GetMatrix(),
				CanvasRenderTarget->GetSizeX(),
				CanvasRenderTarget->GetSizeY(),
				Canvas->IsHitTesting(),
				Gamma
				);

			if( Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender )
			{
				// delete data since we're done rendering it
				delete Data;
			}
		}
		else
		{
			// Render the batched elements.
			struct FBatchedDrawParameters
			{
				FRenderData* RenderData;
				BITFIELD bHitTesting : 1;
				UINT ViewportSizeX;
				UINT ViewportSizeY;
				FLOAT DisplayGamma;
				DWORD AllowedCanvasModes;
			};
			// all the parameters needed for rendering
			FBatchedDrawParameters DrawParameters =
			{
				Data,
				Canvas->IsHitTesting(),
				CanvasRenderTarget->GetSizeX(),
				CanvasRenderTarget->GetSizeY(),
				Gamma,
				Canvas->GetAllowedModes()
			};
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				BatchedDrawCommand,
				FBatchedDrawParameters,Parameters,DrawParameters,
			{
				SCOPED_DRAW_EVENT(EventUIBatchFromGT)(DEC_SCENE_ITEMS,TEXT("UI Texture Draw [GT]"));

				// draw batched items
				Parameters.RenderData->BatchedElements.Draw(
					Parameters.RenderData->Transform.GetMatrix(),
					Parameters.ViewportSizeX,
					Parameters.ViewportSizeY,
					Parameters.bHitTesting,
					Parameters.DisplayGamma
					);
				if( Parameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender )
				{
					delete Parameters.RenderData;
				}
			});
		}
	}
	if( Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender )
	{
		Data = NULL;
	}
	return bDirty;
}

/**
* Info needed to render a single FTileRenderer
*/
class FCanvasTileRendererItem : public FCanvasBaseRenderItem
{
public:
	/** 
	* Init constructor 
	*/
	FCanvasTileRendererItem( 
		const FMaterialRenderProxy* InMaterialRenderProxy=NULL,
		const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity),
		UBOOL bInFreezeTime=FALSE,
		FLOAT InCachedTime = 0.0f)
		// this data is deleted after rendering has completed
		:	Data(new FRenderData(InMaterialRenderProxy,InTransform))
		,	bFreezeTime(bInFreezeTime)
		,	CachedTime(InCachedTime)
	{}

	/**
	* Destructor to delete data in case nothing rendered
	*/
	virtual ~FCanvasTileRendererItem()
	{
		delete Data;
	}

	/**
	* FCanvasTileRendererItem instance accessor
	*
	* @return this instance
	*/
	virtual class FCanvasTileRendererItem* GetCanvasTileRendererItem() 
	{ 
		return this; 
	}

	/**
	* Renders the canvas item. 
	* Iterates over each tile to be rendered and draws it with its own transforms
	*
	* @param Canvas - canvas currently being rendered
	* @return TRUE if anything rendered
	*/
	virtual UBOOL Render( const FCanvas* Canvas );

	/**
	* Determine if this is a matching set by comparing material,transform. All must match
	*
	* @param IInMaterialRenderProxy - material proxy resource for the item being rendered
	* @param InTransform - the transform for the item being rendered
	* @return TRUE if the parameters match this render item
	*/
	UBOOL IsMatch( const FMaterialRenderProxy* InMaterialRenderProxy, const FCanvas::FTransformEntry& InTransform )
	{
		return( Data->MaterialRenderProxy == InMaterialRenderProxy && 
				Data->Transform.GetMatrixCRC() == InTransform.GetMatrixCRC() );
	};

	/**
	* Add a new tile to the render data. These tiles all use the same transform and material proxy
	*
	* @param X - tile X offset
	* @param Y - tile Y offset
	* @param SizeX - tile X size
	* @param SizeY - tile Y size
	* @param U - tile U offset
	* @param V - tile V offset
	* @param SizeU - tile U size
	* @param SizeV - tile V size
	* @param return number of tiles added
	*/
	FORCEINLINE INT AddTile(FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FHitProxyId HitProxyId)
	{
		return Data->AddTile(X,Y,SizeX,SizeY,U,V,SizeU,SizeV,HitProxyId);
	};

private:
	class FRenderData
	{
	public:
		FRenderData(
			const FMaterialRenderProxy* InMaterialRenderProxy=NULL,
			const FCanvas::FTransformEntry& InTransform=FCanvas::FTransformEntry(FMatrix::Identity) )
			:	MaterialRenderProxy(InMaterialRenderProxy)
			,	Transform(InTransform)
		{}
		const FMaterialRenderProxy* MaterialRenderProxy;
		FCanvas::FTransformEntry Transform;

		struct FTileInst
		{
			FLOAT X,Y;
			FLOAT SizeX,SizeY;
			FLOAT U,V;
			FLOAT SizeU,SizeV;
			FHitProxyId HitProxyId;
		};
		TArray<FTileInst> Tiles;

		FORCEINLINE INT AddTile(FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,FHitProxyId HitProxyId)
		{
			FTileInst NewTile = {X,Y,SizeX,SizeY,U,V,SizeU,SizeV,HitProxyId};
			return Tiles.AddItem(NewTile);
		};
	};
	/**
	* Render data which is allocated when a new FCanvasTileRendererItem is added for rendering.
	* This data is only freed on the rendering thread once the item has finished rendering
	*/
	FRenderData* Data;	

	const UBOOL bFreezeTime;

	float CachedTime;
};

/**
* Renders the canvas item. 
* Iterates over each tile to be rendered and draws it with its own transforms
*
* @param Canvas - canvas currently being rendered
* @return TRUE if anything rendered
*/
UBOOL FCanvasTileRendererItem::Render( const FCanvas* Canvas )
{
	FLOAT CurrentRealTime = 0.f;
	FLOAT CurrentWorldTime = 0.f;
	FLOAT DeltaWorldTime = 0.f;

	if (!bFreezeTime)
	{
		// update time using latest game time
		if (IsInGameThread())
		{
			CurrentRealTime = GWorld->GetRealTimeSeconds();
			CurrentWorldTime = GWorld->GetTimeSeconds();
			DeltaWorldTime = GWorld->GetDeltaSeconds();
		}
		else
		{
			static DOUBLE LastSysTime = appSeconds();
			static FLOAT FloatFakeTime = 0;
			const DOUBLE SysTime = appSeconds();
			DeltaWorldTime = Min<FLOAT>(SysTime - LastSysTime, 0.1f);
			FloatFakeTime += DeltaWorldTime;
			LastSysTime = SysTime;
			CurrentRealTime = FloatFakeTime;
			CurrentWorldTime = FloatFakeTime;
		}
	}
	else
	{
		// Update time using last cached time,
		// effectively pauses display by passing
		// in the time it was stopped at. 
		CurrentWorldTime = CachedTime;
		CurrentRealTime = CachedTime;
		DeltaWorldTime = 0.0f;
	}

	checkSlow(Data);
	// current render target set for the canvas
	const FRenderTarget* CanvasRenderTarget = Canvas->GetRenderTarget();
	FSceneViewFamily* ViewFamily = new FSceneViewFamily(
		CanvasRenderTarget,
		NULL,
		SHOW_DefaultGame,
		CurrentWorldTime,
		DeltaWorldTime,
		CurrentRealTime,
		FALSE,FALSE,FALSE,TRUE,TRUE,
		CanvasRenderTarget->GetDisplayGamma(),
		FALSE, FALSE
		);

	// make a temporary view
	FViewInfo* View = new FViewInfo(ViewFamily, 
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
		Data->Transform.GetMatrix(), 
		FLinearColor::Black, 
		FLinearColor(0.f, 0.f, 0.f, 0.f), 
		FLinearColor::White, 
		TSet<UPrimitiveComponent*>()
		);
	// Render the batched elements.
	if( IsInRenderingThread() )
	{
		SCOPED_DRAW_EVENT(EventUIDrawMatFromRT)(DEC_SCENE_ITEMS,TEXT("UI Material Draw [RT]"));

		FTileRenderer TileRenderer;

		for( INT TileIdx=0; TileIdx < Data->Tiles.Num(); TileIdx++ )
		{
			const FRenderData::FTileInst& Tile = Data->Tiles(TileIdx);
			TileRenderer.DrawTile(
				*View, 
				Data->MaterialRenderProxy, 
				Tile.X, Tile.Y, Tile.SizeX, Tile.SizeY, 
				Tile.U, Tile.V, Tile.SizeU, Tile.SizeV,
				Canvas->IsHitTesting(), Tile.HitProxyId
				);
		}

		delete View->Family;
		delete View;
		if( Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender )
		{
			delete Data;
		}
	}
	else
	{
		struct FDrawTileParameters
		{
			FViewInfo* View;
			FRenderData* RenderData;
			BITFIELD bIsHitTesting : 1;
			DWORD AllowedCanvasModes;
		};
		FDrawTileParameters DrawTileParameters =
		{
			View,
			Data,
			Canvas->IsHitTesting(),
			Canvas->GetAllowedModes()
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			DrawTileCommand,
			FDrawTileParameters,Parameters,DrawTileParameters,
		{
			SCOPED_DRAW_EVENT(EventUIDrawMatFromGT)(DEC_SCENE_ITEMS,TEXT("UI Material Draw [GT]"));

			FTileRenderer TileRenderer;

			for( INT TileIdx=0; TileIdx < Parameters.RenderData->Tiles.Num(); TileIdx++ )
			{
				const FRenderData::FTileInst& Tile = Parameters.RenderData->Tiles(TileIdx);
				TileRenderer.DrawTile(
					*Parameters.View, 
					Parameters.RenderData->MaterialRenderProxy, 
					Tile.X, Tile.Y, Tile.SizeX, Tile.SizeY, 
					Tile.U, Tile.V, Tile.SizeU, Tile.SizeV,
					Parameters.bIsHitTesting, Tile.HitProxyId
					);
			}

			delete Parameters.View->Family;
			delete Parameters.View;
			if( Parameters.AllowedCanvasModes & FCanvas::Allow_DeleteOnRender )
			{
				delete Parameters.RenderData;
			}
		});
	}
	if( Canvas->GetAllowedModes() & FCanvas::Allow_DeleteOnRender )
	{
		Data = NULL;
	}
	return TRUE;
}

/**
* Get the sort element for the given sort key. Allocates a new entry if one does not exist
*
* @param DepthSortKey - the key used to find the sort element entry
* @return sort element entry
*/
FCanvas::FCanvasSortElement& FCanvas::GetSortElement(INT DepthSortKey)
{
	// Optimization to resuse last index so that the more expensive Find() does not
	// need to be called as much.
	if (SortedElements.IsValidIndex(LastElementIndex))
	{
		FCanvasSortElement & LastElement = SortedElements(LastElementIndex);
		if (LastElement.DepthSortKey == DepthSortKey)
		{
			return LastElement;
		}
	}

	// find the FCanvasSortElement array entry based on the sortkey
	INT ElementIdx = INDEX_NONE;
	INT* ElementIdxFromMap = SortedElementLookupMap.Find(DepthSortKey);
	if( ElementIdxFromMap )
	{
		ElementIdx = *ElementIdxFromMap;
		checkSlow( SortedElements.IsValidIndex(ElementIdx) );
	}	
	// if it doesn't exist then add a new entry (no duplicates allowed)
	else
	{
		new(SortedElements) FCanvasSortElement(DepthSortKey);
		ElementIdx = SortedElements.Num()-1;
		// keep track of newly added array index for later lookup
		SortedElementLookupMap.Set( DepthSortKey, ElementIdx );
	}
	LastElementIndex = ElementIdx;
	return SortedElements(ElementIdx);
}

/**
* Returns a FBatchedElements pointer to be used for adding vertices and primitives for rendering.
* Adds a new render item to the sort element entry based on the current sort key.
*
* @param InElementType - Type of element we are going to draw.
* @param InBatchedElementParameters - Parameters for this element
* @param InTexture - New texture that will be set.
* @param InBlendMode - New blendmode that will be set.
* @param GlowInfo - info for optional glow effect when using depth field rendering
* @return Returns a pointer to a FBatchedElements object.
*/
FBatchedElements* FCanvas::GetBatchedElements(EElementType InElementType, FBatchedElementParameters* InBatchedElementParameters, const FTexture* InTexture, ESimpleElementBlendMode InBlendMode, const FDepthFieldGlowInfo& GlowInfo)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_GetBatchElementsTime);

	// get sort element based on the current sort key from top of sort key stack
	FCanvasSortElement& SortElement = FCanvas::GetSortElement(TopDepthSortKey());
	// find a batch to use 
	FCanvasBatchedElementRenderItem* RenderBatch = NULL;
	// get the current transform entry from top of transform stack
	const FTransformEntry& TopTransformEntry = TransformStack.Top();

	// try to use the current top entry in the render batch array
	if( SortElement.RenderBatchArray.Num() > 0 )
	{
		checkSlow( SortElement.RenderBatchArray.Last() );
		RenderBatch = SortElement.RenderBatchArray.Last()->GetCanvasBatchedElementRenderItem();
	}	
	// if a matching entry for this batch doesn't exist then allocate a new entry
	if( RenderBatch == NULL ||		
		!RenderBatch->IsMatch(InBatchedElementParameters, InTexture, InBlendMode, InElementType, TopTransformEntry, GlowInfo) )
	{
		INC_DWORD_STAT(STAT_Canvas_NumBatchesCreated);

		RenderBatch = new FCanvasBatchedElementRenderItem( InBatchedElementParameters, InTexture, InBlendMode, InElementType, TopTransformEntry, GlowInfo);
		SortElement.RenderBatchArray.AddItem(RenderBatch);
	}
	return RenderBatch->GetBatchedElements();
}

/**
* Generates a new FCanvasTileRendererItem for the current sortkey and adds it to the sortelement list of itmes to render
*/
void FCanvas::AddTileRenderItem(FLOAT X,FLOAT Y,FLOAT SizeX,FLOAT SizeY,FLOAT U,FLOAT V,FLOAT SizeU,FLOAT SizeV,const FMaterialRenderProxy* MaterialRenderProxy,FHitProxyId HitProxyId,UBOOL bFreezeTime)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_AddTileRenderTime);

	// get sort element based on the current sort key from top of sort key stack
	FCanvasSortElement& SortElement = FCanvas::GetSortElement(TopDepthSortKey());
	// find a batch to use 
	FCanvasTileRendererItem* RenderBatch = NULL;
	// get the current transform entry from top of transform stack
	const FTransformEntry& TopTransformEntry = TransformStack.Top();	

	// try to use the current top entry in the render batch array
	if( SortElement.RenderBatchArray.Num() > 0 )
	{
		checkSlow( SortElement.RenderBatchArray.Last() );
		RenderBatch = SortElement.RenderBatchArray.Last()->GetCanvasTileRendererItem();
	}	
	// if a matching entry for this batch doesn't exist then allocate a new entry
	if( RenderBatch == NULL ||		
		!RenderBatch->IsMatch(MaterialRenderProxy,TopTransformEntry) )
	{
		INC_DWORD_STAT(STAT_Canvas_NumBatchesCreated);
		RenderBatch = new FCanvasTileRendererItem( MaterialRenderProxy,TopTransformEntry,bFreezeTime,CachedTime);
		SortElement.RenderBatchArray.AddItem(RenderBatch);
	}
	// add the quad to the tile render batch
	RenderBatch->AddTile( X,Y,SizeX,SizeY,U,V,SizeU,SizeV,HitProxyId);
}

/**
 * Setup the current masked region during flush
 */
void FCanvas::FlushSetMaskRegion()
{
	if( !RegionMaskingAllowed() || !(AllowedModes&Allow_MaskedRegions) ) 
	{ 
		return; 
	}

	FMaskRegion MaskRegion = GetCurrentMaskRegion();
	checkSlow(MaskRegion.IsValid());

	// create a batch for rendering the masked region 
	FBatchedElements* BatchedElements = new FBatchedElements();
	const FVector2D ZeroUV(0,0);
	const FLinearColor& Color = FLinearColor::White;
	INT V00 = BatchedElements->AddVertex(FVector4(MaskRegion.X,MaskRegion.Y,0,1), ZeroUV, Color,FHitProxyId());
	INT V10 = BatchedElements->AddVertex(FVector4(MaskRegion.X + MaskRegion.SizeX,MaskRegion.Y,0,1),ZeroUV,	Color,FHitProxyId());
	INT V01 = BatchedElements->AddVertex(FVector4(MaskRegion.X,MaskRegion.Y + MaskRegion.SizeY,0,1),ZeroUV,	Color,FHitProxyId());
	INT V11 = BatchedElements->AddVertex(FVector4(MaskRegion.X + MaskRegion.SizeX,MaskRegion.Y + MaskRegion.SizeY,0,1), ZeroUV, Color,FHitProxyId());
	BatchedElements->AddTriangle(V00,V10,V11,GWhiteTexture,BLEND_Opaque);
	BatchedElements->AddTriangle(V00,V11,V01,GWhiteTexture,BLEND_Opaque);

	if( IsInRenderingThread() )
	{
		// Set the RHI render target to the scene color surface, which is necessary so the render target's dimensions and anti-aliasing
		// parameters match the scene depth surface
		RHISetRenderTarget(GSceneRenderTargets.GetSceneColorSurface(), FSceneDepthTargetProxy().GetDepthTargetSurface());
		// set viewport to RT size
		RHISetViewport(0,0,0.0f,RenderTarget->GetSizeX(),RenderTarget->GetSizeY(),1.0f);	
		// disable color writes
		RHISetColorWriteEnable(FALSE);
		// set stencil write enable to one
		RHISetStencilState(TStaticStencilState<TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,0xff,0xff,1>::GetRHI());
		// render the masked region
		BatchedElements->Draw(
			MaskRegion.Transform,
			RenderTarget->GetSizeX(),
			RenderTarget->GetSizeY(),
			IsHitTesting(),
			1.0f
			);
		// Restore the canvas's render target
		RHISetRenderTarget(RenderTarget->GetRenderTargetSurface(), FSceneDepthTargetProxy().GetDepthTargetSurface());
		// Restore the viewport; RHISetRenderTarget resets to the full render target.
		RHISetViewport(0,0,0.0f,RenderTarget->GetSizeX(),RenderTarget->GetSizeY(),1.0f);	
		// reenable color writes
		RHISetColorWriteEnable(TRUE);
		// set stencil state to only render to the masked region
		RHISetStencilState(TStaticStencilState<TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,0xff,0xff,0>::GetRHI());
		// done rendering batch so delete it
		delete BatchedElements;
	}
	else
	{
		struct FCanvasFlushParameters
		{
			UINT ViewSizeX;
			UINT ViewSizeY;
			const FRenderTarget* RenderTarget;
			BITFIELD bIsHitTesting : 1;
			FMatrix Transform;
			FBatchedElements* BatchedElements;
		};
		FCanvasFlushParameters FlushParameters =
		{			
			RenderTarget->GetSizeX(),
			RenderTarget->GetSizeY(),
			RenderTarget,
			IsHitTesting(),
			MaskRegion.Transform,
			BatchedElements
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			CanvasFlushSetMaskRegion,
			FCanvasFlushParameters,Parameters,FlushParameters,
		{
			// Set the RHI render target to the scene color surface, which is necessary so the render target's dimensions and anti-aliasing
			// parameters match the the scene depth surface
			RHISetRenderTarget(GSceneRenderTargets.GetSceneColorSurface(), FSceneDepthTargetProxy().GetDepthTargetSurface());
			// set viewport to RT size
			RHISetViewport(0,0,0.0f,Parameters.ViewSizeX,Parameters.ViewSizeY,1.0f);	
			// disable color writes
			RHISetColorWriteEnable(FALSE);
			// set stencil write enable to one
			RHISetStencilState(TStaticStencilState<TRUE,CF_Always,SO_Keep,SO_Keep,SO_Replace,FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,0xff,0xff,1>::GetRHI());
			// render the masked region
			Parameters.BatchedElements->Draw(
				Parameters.Transform,
				Parameters.ViewSizeX,
				Parameters.ViewSizeY,
				Parameters.bIsHitTesting,
				1.0f
				);
			// Restore the canvas's render target
			RHISetRenderTarget(Parameters.RenderTarget->GetRenderTargetSurface(), FSceneDepthTargetProxy().GetDepthTargetSurface());
			// Restore the viewport; RHISetRenderTarget resets to the full render target.
			RHISetViewport(0,0,0.0f,Parameters.ViewSizeX,Parameters.ViewSizeY,1.0f);	
			// reenable color writes
			RHISetColorWriteEnable(TRUE);
			// set stencil state to only render to the masked region
			RHISetStencilState(TStaticStencilState<TRUE,CF_NotEqual,SO_Keep,SO_Keep,SO_Keep,FALSE,CF_Always,SO_Keep,SO_Keep,SO_Keep,0xff,0xff,0>::GetRHI());
			// done rendering batch so delete it
			delete Parameters.BatchedElements;
		});
	}
}

/**
* Clear masked region during flush
*/
void FCanvas::FlushResetMaskRegion()
{
	if( !RegionMaskingAllowed() || !(AllowedModes&Allow_MaskedRegions) ) 
	{ 
		return; 
	}

	checkSlow(GetCurrentMaskRegion().IsValid());

	if( IsInRenderingThread() )
	{
		// clear stencil to 0
		RHIClear(FALSE,FLinearColor::Black,FALSE,0,TRUE,0);
		// reset stencil state
		RHISetStencilState(TStaticStencilState<>::GetRHI());
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			CanvasFlushResetMaskRegionCommand,
		{
			// clear stencil to 0
			RHIClear(FALSE,FLinearColor::Black,FALSE,0,TRUE,0);
			// reset stencil state
			RHISetStencilState(TStaticStencilState<>::GetRHI());
		});
	}
}

/** Returns whether region masking is allowed. */
UBOOL FCanvas::RegionMaskingAllowed() const
{
	// Don't allow region masking while hit testing in D3D11, since region masking will pair a smaller color buffer with a larger depth buffer whose MSAA types may not match.
	return !(GRHIShaderPlatform == SP_PCD3D_SM5 && IsHitTesting());
}

/**
* Destructor for canvas
*/
FCanvas::~FCanvas()
{
	// delete batches from elements entries
	for( INT Idx=0; Idx < SortedElements.Num(); Idx++ )
	{
		FCanvasSortElement& SortElement = SortedElements(Idx);
		for( INT BatchIdx=0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++ )
		{
			FCanvasBaseRenderItem* RenderItem = SortElement.RenderBatchArray(BatchIdx);
			delete RenderItem;
		}
	}
}

/** 
* Sends a message to the rendering thread to draw the batched elements. 
*/
void FCanvas::Flush(UBOOL bForce)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_FlushTime);

	if( !(AllowedModes&Allow_Flush) && !bForce ) 
	{ 
		return; 
	}

#if !CONSOLE
	if (IsInGameThread())
	{
		// Make sure deferred shader compiling is completed before exposing shader maps to the rendering thread
		GShaderCompilingThreadManager->FinishDeferredCompilation();
	}
#endif

	// current render target set for the canvas
	check(RenderTarget);	 	

	// sort the array of FCanvasSortElement entries so that higher sort keys render first (back-to-front)
	Sort<USE_COMPARE_CONSTREF(FCanvasSortElement,UnCanvas)>( &SortedElements(0), SortedElements.Num() );

	// Don't allow depth testing while hit testing in D3D11, since depth testing will pair a smaller color buffer with a larger depth buffer whose MSAA types may not match.
	const UBOOL bAllowDepthTesting = !(GRHIShaderPlatform == SP_PCD3D_SM5 && IsHitTesting());

	if( IsInRenderingThread() )
	{
		if( bAllowDepthTesting &&
			bEnableDepthTest &&
			(AllowedModes&Allow_DepthTest) )
		{
			// Set the RHI render target. and the scene depth surface
			RHISetRenderTarget(RenderTarget->GetRenderTargetSurface(), FSceneDepthTargetProxy().GetDepthTargetSurface());
			// enable depth test & disable writes
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
		}
		else
		{
			// Set the RHI render target.
			RHISetRenderTarget(RenderTarget->GetRenderTargetSurface(), StereoizedDrawNullTarget(FSceneDepthTargetProxy().GetDepthTargetSurface()));
			// disable depth test & writes
			RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
		}
		// set viewport to RT size
		RHISetViewport(0,0,0.0f,RenderTarget->GetSizeX(),RenderTarget->GetSizeY(),1.0f);	
	}
	else 
	{
		struct FCanvasFlushParameters
		{
			BITFIELD bDepthTestEnabled : 1;
			UINT ViewSizeX;
			UINT ViewSizeY;
			const FRenderTarget* CanvasRenderTarget;
		};
		FCanvasFlushParameters FlushParameters =
		{
			bAllowDepthTesting && bEnableDepthTest && (AllowedModes&Allow_DepthTest),
			RenderTarget->GetSizeX(),
			RenderTarget->GetSizeY(),
			RenderTarget
		};
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			CanvasFlushSetupCommand,
			FCanvasFlushParameters,Parameters,FlushParameters,
		{
			if( Parameters.bDepthTestEnabled )
			{
				// Set the RHI render target. and the scene depth surface
				RHISetRenderTarget(Parameters.CanvasRenderTarget->GetRenderTargetSurface(), FSceneDepthTargetProxy().GetDepthTargetSurface());
				// enable depth test & disable writes
				RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
			}
			else
			{
				// Set the RHI render target.
				RHISetRenderTarget(Parameters.CanvasRenderTarget->GetRenderTargetSurface(), StereoizedDrawNullTarget(FSceneDepthTargetProxy().GetDepthTargetSurface()));
				// disable depth test & writes
				RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
			}
			// set viewport to RT size
			RHISetViewport(0,0,0.0f,Parameters.ViewSizeX,Parameters.ViewSizeY,1.0f);	
		});
	}

	if( GetCurrentMaskRegion().IsValid() )
	{
		// setup the masked region if it was valid
		FlushSetMaskRegion();
	}

	// iterate over the FCanvasSortElements in sorted order and render all the batched items for each entry
	for( INT Idx=0; Idx < SortedElements.Num(); Idx++ )
	{
		FCanvasSortElement& SortElement = SortedElements(Idx);
		for( INT BatchIdx=0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++ )
		{
			FCanvasBaseRenderItem* RenderItem = SortElement.RenderBatchArray(BatchIdx);
			if( RenderItem )
			{
				// mark current render target as dirty since we are drawing to it
				bRenderTargetDirty |= RenderItem->Render(this);
				if( AllowedModes & Allow_DeleteOnRender )
				{
					delete RenderItem;
				}
			}			
		}
		if( AllowedModes & Allow_DeleteOnRender )
		{
			SortElement.RenderBatchArray.Empty();
		}
	}
	if( AllowedModes & Allow_DeleteOnRender )
	{
		// empty the array of FCanvasSortElement entries after finished with rendering	
		SortedElements.Empty();
		SortedElementLookupMap.Empty();
		LastElementIndex = INDEX_NONE;
	}

	if( GetCurrentMaskRegion().IsValid() )
	{
		// reset the masked region if it was valid
		FlushResetMaskRegion();
	}
}

void FCanvas::PushRelativeTransform(const FMatrix& Transform)
{
	INT PreviousTopIndex = TransformStack.Num() - 1;
#if 0
	static UBOOL DEBUG_NoRotation=1;
	if( DEBUG_NoRotation )
	{
		FMatrix TransformNoRotation(FMatrix::Identity);
		TransformNoRotation.SetOrigin(Transform.GetOrigin());
		TransformStack.AddItem( FTransformEntry(TransformNoRotation * TransformStack(PreviousTopIndex).GetMatrix()) );
	}
	else
#endif
	{
		TransformStack.AddItem( FTransformEntry(Transform * TransformStack(PreviousTopIndex).GetMatrix()) );
	}
}

void FCanvas::PushAbsoluteTransform(const FMatrix& Transform) 
{
	TransformStack.AddItem( FTransformEntry(Transform * TransformStack(0).GetMatrix()) );
}

void FCanvas::PopTransform()
{
	TransformStack.Pop();
}

void FCanvas::SetHitProxy(HHitProxy* HitProxy)
{
	// Change the current hit proxy.
	CurrentHitProxy = HitProxy;

	if(HitProxyConsumer && HitProxy)
	{
		// Notify the hit proxy consumer of the new hit proxy.
		HitProxyConsumer->AddHitProxy(HitProxy);
	}
}

/**
* Determine if the canvas has dirty batches that need to be rendered
*
* @return TRUE if the canvas has any element to render
**/
UBOOL FCanvas::HasBatchesToRender() const
{
	for( INT Idx=0; Idx < SortedElements.Num(); Idx++ )
	{
		const FCanvasSortElement& SortElement = SortedElements(Idx);
		for( INT BatchIdx=0; BatchIdx < SortElement.RenderBatchArray.Num(); BatchIdx++ )
		{
			if( SortElement.RenderBatchArray(BatchIdx) )
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

/**
* Copy the conents of the TransformStack from an existing canvas
*
* @param Copy	canvas to copy from
**/
void FCanvas::CopyTransformStack(const FCanvas& Copy)
{ 
	TransformStack = Copy.TransformStack;
}

/**
* Toggles current depth testing state for the canvas. All batches
* will render with depth testing against the depth buffer if enabled.
*
* @param bEnabled - if TRUE then depth testing is enabled
*/
void FCanvas::SetDepthTestingEnabled(UBOOL bEnabled)
{
	if( bEnableDepthTest != bEnabled )
	{
		Flush();
		bEnableDepthTest = bEnabled;
	}
}

/** 
 * Set the current masked region on the canvas
 * All rendering from this point on will be masked to this region.
 * The region being masked uses the current canvas transform
 *
 * @param X - x offset in canvas coords
 * @param Y - y offset in canvas coords
 * @param SizeX - x size in canvas coords
 * @param SizeY - y size in canvas coords
 */
void FCanvas::PushMaskRegion( FLOAT X, FLOAT Y, FLOAT SizeX, FLOAT SizeY )
{
	FMaskRegion NewMask(X, Y, SizeX, SizeY, TransformStack.Top().GetMatrix());
	if ( !NewMask.IsEqual(GetCurrentMaskRegion()) )
	{
		Flush();
	}

	MaskRegionStack.Push(NewMask);
}

/**
 * Replace the top element of the masking region stack with a new region
 */
void FCanvas::ReplaceMaskRegion( FLOAT X, FLOAT Y, FLOAT SizeX, FLOAT SizeY )
{
	if ( MaskRegionStack.Num() > 0 )
	{
		const INT CurrentMaskIdx = MaskRegionStack.Num() - 1;

		FMaskRegion NewMask(X, Y, SizeX, SizeY, TransformStack.Top().GetMatrix());
		if ( !NewMask.IsEqual(MaskRegionStack(CurrentMaskIdx)) )
		{
			Flush();
			MaskRegionStack(CurrentMaskIdx) = NewMask;
		}
	}
	else
	{
		PushMaskRegion(X, Y, SizeX, SizeY);
	}
}

/**
 * Remove the current masking region; if other masking regions were previously pushed onto the stack,
 * the next one down will be activated.
 */
void FCanvas::PopMaskRegion()
{
	FMaskRegion NextMaskRegion = MaskRegionStack.Num() > 1 
		? MaskRegionStack(MaskRegionStack.Num() - 2)
		: FMaskRegion();

	if ( !NextMaskRegion.IsEqual(GetCurrentMaskRegion()) )
	{
		Flush();
	}

	if ( MaskRegionStack.Num() > 0 )
	{
		MaskRegionStack.Pop();
	}
}

/**
 * Get the top-most canvas masking region from the stack.
 */
FCanvas::FMaskRegion FCanvas::GetCurrentMaskRegion() const
{
	if ( MaskRegionStack.Num() > 0 )
	{
		return MaskRegionStack(MaskRegionStack.Num() - 1);
	}

	return FMaskRegion();
}

void FCanvas::SetRenderTarget(FRenderTarget* NewRenderTarget)
{
	if( RenderTarget != NewRenderTarget )
	{
		// flush whenever we swap render targets
		if( RenderTarget )
		{
			Flush();			

			// resolve the current render target if it is dirty
			if( bRenderTargetDirty )
			{
				if( IsInRenderingThread() )
				{
					RHICopyToResolveTarget(RenderTarget->GetRenderTargetSurface(),TRUE,FResolveParams());					
				}
				else
				{
					ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
						ResolveCanvasRTCommand,
						FRenderTarget*,CanvasRenderTarget,RenderTarget,
					{
						RHICopyToResolveTarget(CanvasRenderTarget->GetRenderTargetSurface(),TRUE,FResolveParams());
					});
				}
				SetRenderTargetDirty(FALSE);
			}
		}
		// Change the current render target.
		RenderTarget = NewRenderTarget;
	}
}

void Clear(FCanvas* Canvas,const FLinearColor& Color)
{
	// desired display gamma space
	const FLOAT DisplayGamma = (GEngine && GEngine->Client) ? GEngine->Client->DisplayGamma : 2.2f;
	// render target gamma space expected
	FLOAT RenderTargetGamma = DisplayGamma;
	if( Canvas->GetRenderTarget() )
	{
		RenderTargetGamma = Canvas->GetRenderTarget()->GetDisplayGamma();
	}
	// assume that the clear color specified is in 2.2 gamma space
	// so convert to the render target's color space 
	FLinearColor GammaCorrectedColor(Color);
	GammaCorrectedColor.R = appPow(Clamp<FLOAT>(GammaCorrectedColor.R,0.0f,1.0f), DisplayGamma / RenderTargetGamma);
	GammaCorrectedColor.G = appPow(Clamp<FLOAT>(GammaCorrectedColor.G,0.0f,1.0f), DisplayGamma / RenderTargetGamma);
	GammaCorrectedColor.B = appPow(Clamp<FLOAT>(GammaCorrectedColor.B,0.0f,1.0f), DisplayGamma / RenderTargetGamma);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		ClearCommand,
		FColor,Color,GammaCorrectedColor,
		FRenderTarget*,CanvasRenderTarget,Canvas->GetRenderTarget(),
		{
			if( CanvasRenderTarget )
			{
				RHISetRenderTarget(CanvasRenderTarget->GetRenderTargetSurface(),FSurfaceRHIRef());
				RHISetViewport(0,0,0.0f,CanvasRenderTarget->GetSizeX(),CanvasRenderTarget->GetSizeY(),1.0f);
			}
			RHIClear(TRUE,Color,FALSE,0.0f,FALSE,0);
		});
}

void ClearAll(FCanvas* Canvas,const FLinearColor& Color)
{
	// desired display gamma space
	const FLOAT DisplayGamma = (GEngine && GEngine->Client) ? GEngine->Client->DisplayGamma : 2.2f;
	// render target gamma space expected
	FLOAT RenderTargetGamma = DisplayGamma;
	if( Canvas->GetRenderTarget() )
	{
		RenderTargetGamma = Canvas->GetRenderTarget()->GetDisplayGamma();
	}
	// assume that the clear color specified is in 2.2 gamma space
	// so convert to the render target's color space 
	FLinearColor GammaCorrectedColor(Color);
	GammaCorrectedColor.R = appPow(Clamp<FLOAT>(GammaCorrectedColor.R,0.0f,1.0f), DisplayGamma / RenderTargetGamma);
	GammaCorrectedColor.G = appPow(Clamp<FLOAT>(GammaCorrectedColor.G,0.0f,1.0f), DisplayGamma / RenderTargetGamma);
	GammaCorrectedColor.B = appPow(Clamp<FLOAT>(GammaCorrectedColor.B,0.0f,1.0f), DisplayGamma / RenderTargetGamma);

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		ClearAllCommand,
		FColor,Color,GammaCorrectedColor,
		FRenderTarget*,CanvasRenderTarget,Canvas->GetRenderTarget(),
	{
		if( CanvasRenderTarget )
		{
			RHISetRenderTarget(CanvasRenderTarget->GetRenderTargetSurface(),FSurfaceRHIRef());
			RHISetViewport(0,0,0.0f,CanvasRenderTarget->GetSizeX(),CanvasRenderTarget->GetSizeY(),1.0f);
		}
		RHIClear(TRUE,Color,TRUE,1.0f,TRUE,0);
	});
}

/**
 *	Draws a line.
 *
 * @param	Canvas		Drawing canvas.
 * @param	StartPos	Starting position for the line.
 * @param	EndPos		Ending position for the line.
 * @param	Color		Color for the line.
 */
void DrawLine(FCanvas* Canvas,const FVector& StartPos,const FVector& EndPos,const FLinearColor& Color)
{
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Line);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	BatchedElements->AddLine(StartPos,EndPos,Color,HitProxyId);
}

/**
 *	Draws a 2D line.
 *
 * @param	Canvas		Drawing canvas.
 * @param	StartPos	Starting position for the line.
 * @param	EndPos		Ending position for the line.
 * @param	Color		Color for the line.
 */
void DrawLine2D(FCanvas* Canvas,const FVector2D& StartPos,const FVector2D& EndPos,const FLinearColor& Color, FLOAT LineThickness)
{
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Line);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	BatchedElements->AddLine(FVector(StartPos.X,StartPos.Y,0), FVector(EndPos.X,EndPos.Y,0), Color, HitProxyId, LineThickness, FALSE);
}

void DrawBox2D(FCanvas* Canvas,const FVector2D& StartPos,const FVector2D& EndPos,const FLinearColor& Color)
{
	DrawLine2D(Canvas,FVector2D(StartPos.X,StartPos.Y),FVector2D(StartPos.X,EndPos.Y),Color);
	DrawLine2D(Canvas,FVector2D(StartPos.X,EndPos.Y),FVector2D(EndPos.X,EndPos.Y),Color);
	DrawLine2D(Canvas,FVector2D(EndPos.X,EndPos.Y),FVector2D(EndPos.X,StartPos.Y),Color);
	DrawLine2D(Canvas,FVector2D(EndPos.X,StartPos.Y),FVector2D(StartPos.X,StartPos.Y),Color);
}

void DrawTile(
	FCanvas* Canvas,
	FLOAT X,
	FLOAT Y,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	const FLinearColor& Color,
	const FTexture* Texture,
	UBOOL AlphaBlend
	)
{
    DrawTileZ(Canvas, X, Y, 1.0f, SizeX, SizeY, U, V, SizeU, SizeV, Color, Texture, AlphaBlend);
}

void DrawTile(
	FCanvas* Canvas,
	FLOAT X,
	FLOAT Y,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	const FLinearColor& Color,
	const FTexture* Texture,
	ESimpleElementBlendMode BlendMode
	)
{
    DrawTileZ(Canvas, X, Y, 1.0f, SizeX, SizeY, U, V, SizeU, SizeV, Color, Texture, BlendMode);
}

void DrawTile(
	FCanvas* Canvas,
	FLOAT X,
	FLOAT Y,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	const FMaterialRenderProxy* MaterialRenderProxy,
	UBOOL bFreezeTime
	)
{
    DrawTileZ(Canvas, X, Y, 1.0f, SizeX, SizeY, U, V, SizeU, SizeV, MaterialRenderProxy, bFreezeTime);
}

void DrawTileZ(
	FCanvas* Canvas,
	FLOAT X,
	FLOAT Y,
    FLOAT Z,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	const FLinearColor& Color,
	const FTexture* Texture,
	UBOOL AlphaBlend
	)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_DrawTextureTileTime);

	FLinearColor ActualColor = Color;
	ActualColor.A *= Canvas->AlphaModulate;

	const FTexture* FinalTexture = Texture ? Texture : GWhiteTexture;
	const ESimpleElementBlendMode BlendMode = AlphaBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();


    // Correct for Depth. This only works because we won't be applying a transform later--otherwise we'd have to adjust the transform instead.
    FLOAT Left, Top, Right, Bottom;
    Left = X * Z;
    Top = Y * Z;
    Right = (X + SizeX) * Z;
    Bottom = (Y + SizeY) * Z;

	FLOAT S3DDisparity = GetStereoscopicDisparity(Z);
	Left += S3DDisparity;
	Right += S3DDisparity;

	INT V00 = BatchedElements->AddVertex(FVector4(Left,	    Top,	0,Z),FVector2D(U,			V),			ActualColor,HitProxyId);
	INT V10 = BatchedElements->AddVertex(FVector4(Right,    Top,	0,Z),FVector2D(U + SizeU,	V),			ActualColor,HitProxyId);
	INT V01 = BatchedElements->AddVertex(FVector4(Left,	    Bottom,	0,Z),FVector2D(U,			V + SizeV),	ActualColor,HitProxyId);
	INT V11 = BatchedElements->AddVertex(FVector4(Right,    Bottom,	0,Z),FVector2D(U + SizeU,	V + SizeV),	ActualColor,HitProxyId);

	BatchedElements->AddTriangle(V00,V10,V11,FinalTexture,BlendMode);
	BatchedElements->AddTriangle(V00,V11,V01,FinalTexture,BlendMode);
}

void DrawTileZ(
	FCanvas* Canvas,
	FLOAT X,
	FLOAT Y,
    FLOAT Z,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	const FLinearColor& Color,
	const FTexture* Texture,
	ESimpleElementBlendMode BlendMode
	)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_DrawTextureTileTime);

	FLinearColor ActualColor = Color;
	ActualColor.A *= Canvas->AlphaModulate;
	const FTexture* FinalTexture = Texture ? Texture : GWhiteTexture;	
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

    // Correct for Depth. This only works because we won't be applying a transform later--otherwise we'd have to adjust the transform instead.
    FLOAT Left, Top, Right, Bottom;
    Left = X * Z;
    Top = Y * Z;
    Right = (X + SizeX) * Z;
    Bottom = (Y + SizeY) * Z;

	// offset for 3D
	FLOAT S3DDisparity = GetStereoscopicDisparity(Z);
	Left += S3DDisparity;
	Right += S3DDisparity;

	INT V00 = BatchedElements->AddVertex(FVector4(Left,		Top,	0,Z),FVector2D(U,			V),			ActualColor,HitProxyId);
	INT V10 = BatchedElements->AddVertex(FVector4(Right,    Top,	0,Z),FVector2D(U + SizeU,	V),			ActualColor,HitProxyId);
	INT V01 = BatchedElements->AddVertex(FVector4(Left,		Bottom,	0,Z),FVector2D(U,			V + SizeV),	ActualColor,HitProxyId);
	INT V11 = BatchedElements->AddVertex(FVector4(Right,    Bottom,	0,Z),FVector2D(U + SizeU,	V + SizeV),	ActualColor,HitProxyId);

	BatchedElements->AddTriangle(V00,V10,V11,FinalTexture,BlendMode);
	BatchedElements->AddTriangle(V00,V11,V01,FinalTexture,BlendMode);
}

void DrawTileZ(
	FCanvas* Canvas,
	FLOAT X,
	FLOAT Y,
    FLOAT Z,
	FLOAT SizeX,
	FLOAT SizeY,
	FLOAT U,
	FLOAT V,
	FLOAT SizeU,
	FLOAT SizeV,
	const FMaterialRenderProxy* MaterialRenderProxy,
	UBOOL bFreezeTime
	)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_DrawMaterialTileTime);

	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	// offset for 3D
	FLOAT Disparity = GetStereoscopicDisparity(Z);
	X += Disparity;

	// add a new FTileRenderItem entry. These get rendered and freed when the canvas is flushed
	Canvas->AddTileRenderItem(X,Y,SizeX,SizeY,U,V,SizeU,SizeV,MaterialRenderProxy,HitProxyId,bFreezeTime);
}

struct CanvasDrawTimerRect
{
	FLOAT Left, Right, Top, Bottom, MidX, MidY;
};


// See where two lines intersect defined by two line segments.  Note that it is not a line segment test.
UBOOL FindLineIntersection(FVector2D& Result, const FVector2D& P1, const FVector2D& P2, const FVector2D& P3, const FVector2D& P4) 
{  
	// Intersection of two segments using determinants (matrix)...in code common calculations are extracted.
	//Result.X = (((P1.X * P2.Y) - (P1.Y * P2.X)) * (P3.X - P4.X)) - ((P1.X - P2.X) * ((P3.X * P4.Y) - (P3.Y * P4.X)));
	//Result.X /= ((P1.X - P2.X) * (P3.Y - P4.Y)) - ((P1.Y - P2.Y) * (P3.X - P4.X));
	//Result.Y = (((P1.X * P2.Y) - (P1.Y * P2.X)) * (P3.Y - P4.Y)) - ((P1.Y - P2.Y) * ((P3.X * P4.Y) - (P3.Y * P4.X)));
	//Result.Y /= ((P1.X - P2.X) * (P3.Y - P4.Y)) - ((P1.Y - P2.Y) * (P3.X - P4.X));

	FLOAT L1DX = P1.X - P2.X;
	FLOAT L1DY = P1.Y - P2.Y;
	FLOAT L2DX = P3.X - P4.X;
	FLOAT L2DY = P3.Y - P4.Y;
	FLOAT Dot = (L1DX * L2DY) - (L1DY * L2DX);
	if (Dot == 0)
		return FALSE;

	FLOAT ProdX1Y2 = P1.X * P2.Y;
	FLOAT ProdY1X2 = P1.Y * P2.X;
	FLOAT Prod1Diff = ProdX1Y2 - ProdY1X2;

	FLOAT ProdX2Y4 = P3.X * P4.Y;
	FLOAT ProdY3X4 = P3.Y * P4.X;
	FLOAT Prod2Diff = ProdX2Y4 - ProdY3X4;

	Result.X = (Prod1Diff * L2DX) - (L1DX * Prod2Diff);
	Result.X /= Dot;

	Result.Y = (Prod1Diff * L2DY) - (L1DY * Prod2Diff);
	Result.Y /= Dot;

	return TRUE;
}  

// Very specific for DrawTimer().  Given a Time of 0-1, determine
// where the vertex should be on the polygon edge.  If TRUE is returned,
// then Slice is on of 8 vertexes already created, otherwise a new
// vertex needs to be created using V.
UBOOL DrawTimer_VertexForTime(FLOAT Time, const CanvasDrawTimerRect& Rect, INT& Slice, FVector2D& V)
{
	Slice = INT(Time / 0.125f);

	FLOAT InSlice = (Time - (Slice * 0.125f)) / 0.125f;
	if (InSlice < 0.00001)
		return TRUE;

	// Convert time percent to angle.
	FLOAT Angle = Time - 0.25f;
	if (Angle < 0) 
		Angle += 1.0f;
	Angle = 1.0f - Angle;
	Angle *= 2 * PI;

	// NOTE: Scale not required, but easier to debug.
	FLOAT Scale = Rect.MidX - Rect.Left;
	FVector2D Center(Rect.MidX, Rect.MidY);
	FVector2D EndPoint(Center.X + Scale * appCos(Angle), Center.Y + Scale * -appSin(Angle));
	FVector2D Intersect, P1, P2;

	// Slice 1 starts at 12:00, 2 at 3:00...and going around clockwise..
	// For each case, create a line on correct of edge of timer poly.
	// Find intersection of that edge and the line from center to edge.
	// Convert the point back a percentage on the perimeter of the poly.
	switch (Slice)
	{
	case 0:	
	case 7: 
		P1.Set(Rect.Left, Rect.Top);
		P2.Set(Rect.Right, Rect.Top);
		FindLineIntersection(V, P1, P2, Center, EndPoint);
		V.Set((V.X - P1.X) / (P2.X - P1.X), 0);
		break;
	case 1: 
	case 2: 
		P1.Set(Rect.Right, Rect.Top);
		P2.Set(Rect.Right, Rect.Bottom);
		FindLineIntersection(V, P1, P2, Center, EndPoint);
		V.Set(1.0f, (V.Y - P1.Y) / (P2.Y - P1.Y));
		break;
	case 3: 
	case 4: 
		P1.Set(Rect.Left, Rect.Bottom);
		P2.Set(Rect.Right, Rect.Bottom);
		FindLineIntersection(V, P1, P2, Center, EndPoint);
		V.Set((V.X - P1.X) / (P2.X - P1.X), 1.0f);
		break;
	case 5: 
	case 6: 
		P1.Set(Rect.Left, Rect.Top);
		P2.Set(Rect.Left, Rect.Bottom);
		FindLineIntersection(V, P1, P2, Center, EndPoint);
		V.Set(0, (V.Y - P1.Y) / (P2.Y - P1.Y));
		break;
	}

	return FALSE;
}


void DrawTimerZ(
				FCanvas* Canvas,
				FLOAT StartTime, 
				FLOAT TotalTime, 
				FLOAT X,
				FLOAT Y,
				FLOAT Z,
				FLOAT SizeX,
				FLOAT SizeY,
				FLOAT U,
				FLOAT V,
				FLOAT SizeU,
				FLOAT SizeV,
				const FLinearColor& Color,
				const FTexture* Texture,
				UBOOL AlphaBlend
				)
{
	if ( !Canvas ) 
		return;

	SCOPE_CYCLE_COUNTER(STAT_Canvas_DrawTextureTileTime);

	FLinearColor ActualColor = Color;
	ActualColor.A *= Canvas->AlphaModulate;

	const FTexture* FinalTexture = Texture ? Texture : GWhiteTexture;
	const ESimpleElementBlendMode BlendMode = AlphaBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	// Correct for Depth. This only works because we won't be applying a transform later--otherwise we'd have to adjust the transform instead.
	CanvasDrawTimerRect Rect;

	Rect.Left = X * Z;
	Rect.Top = Y * Z;
	Rect.Right = (X + SizeX) * Z;
	Rect.Bottom = (Y + SizeY) * Z;

	FLOAT S3DDisparity = GetStereoscopicDisparity(Z);
	Rect.Left += S3DDisparity;
	Rect.Right += S3DDisparity;

	Rect.MidX = Rect.Left + (Rect.Right - Rect.Left) * 0.5f;
	Rect.MidY = Rect.Top + (Rect.Bottom - Rect.Top) * 0.5f;

	// Convert start and percent to be 
	if (TotalTime < 0)
	{
		TotalTime = -TotalTime;
		StartTime -= TotalTime;
	}

	StartTime -= int(StartTime);
	if (StartTime < 0)
		StartTime = 1.0f - StartTime;

	FLOAT EndU = U + SizeU;
	FLOAT MidU = U + (SizeU * 0.5f);
	FLOAT EndV = V + SizeV;
	FLOAT MidV = V + (SizeV * 0.5f);

	// 0 is at 12:00 position, 0.25 is 3:00 position, etc.
	INT Center   = BatchedElements->AddVertex(FVector4(Rect.MidX, Rect.MidY, 0,Z),FVector2D(MidU, MidV), ActualColor,HitProxyId);
	INT Vert[9];
	Vert[0] = BatchedElements->AddVertex(FVector4(Rect.MidX, Rect.Top, 0,Z),FVector2D(MidU, V), ActualColor,HitProxyId);				//V = 0 & 1	(12:00)
	Vert[1]  = BatchedElements->AddVertex(FVector4(Rect.Right, Rect.Top, 0,Z),FVector2D(EndU, V), ActualColor,HitProxyId);			//V = .125	
	Vert[2]  = BatchedElements->AddVertex(FVector4(Rect.Right, Rect.MidY, 0,Z),FVector2D(EndU, MidV), ActualColor,HitProxyId);		//V = .25	(3:00
	Vert[3]  = BatchedElements->AddVertex(FVector4(Rect.Right, Rect.Bottom, 0,Z),FVector2D(EndU, EndV),	ActualColor,HitProxyId);	//V = .375
	Vert[4]  = BatchedElements->AddVertex(FVector4(Rect.MidX, Rect.Bottom, 0,Z),FVector2D(MidU, EndV), ActualColor,HitProxyId);		//V = .5	(6:00)
	Vert[5]  = BatchedElements->AddVertex(FVector4(Rect.Left, Rect.Bottom, 0,Z),FVector2D(U, EndV),	ActualColor,HitProxyId);		//V = .625
	Vert[6]  = BatchedElements->AddVertex(FVector4(Rect.Left, Rect.MidY, 0,Z),FVector2D(U, MidV), ActualColor,HitProxyId);			//V = .75	(9:00)
	Vert[7]  = BatchedElements->AddVertex(FVector4(Rect.Left, Rect.Top, 0,Z),FVector2D(U, V),	ActualColor,HitProxyId);			//V = .875
	Vert[8] = Vert[0];

	FLOAT T0 = StartTime;
	FLOAT T1 = T0 + TotalTime;
	FLOAT T2 = -1.0f;
	FLOAT T3 = -1.0f;
	if (T1 > 1.0f)
	{
		T3 = T1 - 1.0f;
		T1 = 1.0f;
		T2 = 0;
	}

	for (INT Slice = 0; Slice < 8; Slice++)
	{
		FLOAT Start = 0.125f * Slice;
		FLOAT End = Start + 0.125f;

		if ((T0 <= Start && T1 >= End) || (T2 <= Start && T3 >= End))
			BatchedElements->AddTriangle(Center, Vert[Slice], Vert[Slice+1],FinalTexture,BlendMode);
	}

	FLOAT Ta = T0;
	FLOAT Tb = T1;
	// Test Tb for early exit on second loop - it will only loop twice if 0 (12:00) is inside Start and End time.
	for (int Lp = 0; Lp < 2 && (Tb > 0); Lp++)
	{
		INT SliceA, SliceB;
		FVector2D EdgeA, EdgeB;
		UBOOL bExistsA, bExistsB; // Is it a predefined vertex?
		INT VertA, VertB;

		bExistsA = DrawTimer_VertexForTime(Ta, Rect, SliceA, EdgeA);
		if (bExistsA)
			VertA = Vert[SliceA];
		else
			VertA = BatchedElements->AddVertex(FVector4(Lerp(Rect.Left, Rect.Right, EdgeA.X), Lerp(Rect.Top, Rect.Bottom, EdgeA.Y), 0,Z), FVector2D(Lerp(U, EndU, EdgeA.X), Lerp(V, EndV, EdgeA.Y)),ActualColor,HitProxyId);

		bExistsB = DrawTimer_VertexForTime(Tb, Rect, SliceB, EdgeB);
		if (bExistsB)
			VertB = Vert[SliceB];
		else
			VertB = BatchedElements->AddVertex(FVector4(Lerp(Rect.Left, Rect.Right, EdgeB.X), Lerp(Rect.Top, Rect.Bottom, EdgeB.Y), 0,Z), FVector2D(Lerp(U, EndU, EdgeB.X), Lerp(V, EndV, EdgeB.Y)),ActualColor,HitProxyId);

		// Start and stop in same slice.
		if (SliceA == SliceB)
		{
			BatchedElements->AddTriangle(Center, VertA, VertB,FinalTexture,BlendMode);
		}
		else 
		{
			if (!bExistsA)
			{
				// Draw first part of timer.
				BatchedElements->AddTriangle(Center, VertA, Vert[SliceA + 1], FinalTexture,BlendMode);
			}
			if (!bExistsB)
			{
				// Last part of timer.
				BatchedElements->AddTriangle(Center, Vert[SliceB], VertB, FinalTexture,BlendMode);
			}
		}
		Ta = T2;
		Tb = T3;
	}
}


void DrawTriangle2D(
	FCanvas* Canvas,
	const FVector2D& Position0,
	const FVector2D& TexCoord0,
	const FVector2D& Position1,
	const FVector2D& TexCoord1,
	const FVector2D& Position2,
	const FVector2D& TexCoord2,
	const FLinearColor& Color,
	const FTexture* Texture,
	UBOOL AlphaBlend
	)
{
	const ESimpleElementBlendMode BlendMode = AlphaBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
	const FTexture* FinalTexture = Texture ? Texture : GWhiteTexture;
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	INT V0 = BatchedElements->AddVertex(FVector4(Position0.X,Position0.Y,0,1),TexCoord0,Color,HitProxyId);
	INT V1 = BatchedElements->AddVertex(FVector4(Position1.X,Position1.Y,0,1),TexCoord1,Color,HitProxyId);
	INT V2 = BatchedElements->AddVertex(FVector4(Position2.X,Position2.Y,0,1),TexCoord2,Color,HitProxyId);

	BatchedElements->AddTriangle(V0,V1,V2,FinalTexture, BlendMode);
}

void DrawTriangle2D(
	FCanvas* Canvas,
	const FVector2D& Position0,
	const FVector2D& TexCoord0,
	const FLinearColor& Color0,
	const FVector2D& Position1,
	const FVector2D& TexCoord1,
	const FLinearColor& Color1,
	const FVector2D& Position2,
	const FVector2D& TexCoord2,
	const FLinearColor& Color2,
	const FTexture* Texture,
	UBOOL AlphaBlend
	)
{
	const ESimpleElementBlendMode BlendMode = AlphaBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
	const FTexture* FinalTexture = Texture ? Texture : GWhiteTexture;
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	INT V0 = BatchedElements->AddVertex(FVector4(Position0.X,Position0.Y,0,1),TexCoord0,Color0,HitProxyId);
	INT V1 = BatchedElements->AddVertex(FVector4(Position1.X,Position1.Y,0,1),TexCoord1,Color1,HitProxyId);
	INT V2 = BatchedElements->AddVertex(FVector4(Position2.X,Position2.Y,0,1),TexCoord2,Color2,HitProxyId);

	BatchedElements->AddTriangle(V0,V1,V2,FinalTexture, BlendMode);
}


void DrawTriangle2DWithParameters(
	FCanvas* Canvas,
	const FVector2D& Position0,
	const FVector2D& TexCoord0,
	const FLinearColor& Color0,
	const FVector2D& Position1,
	const FVector2D& TexCoord1,
	const FLinearColor& Color1,
	const FVector2D& Position2,
	const FVector2D& TexCoord2,
	const FLinearColor& Color2,
	FBatchedElementParameters* BatchedElementParameters,
	UBOOL AlphaBlend
	)
{
	check( BatchedElementParameters != NULL );
	const ESimpleElementBlendMode BlendMode = AlphaBlend ? SE_BLEND_Translucent : SE_BLEND_Opaque;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, NULL, BlendMode);
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();

	INT V0 = BatchedElements->AddVertex(FVector4(Position0.X,Position0.Y,0,1),TexCoord0,Color0,HitProxyId);
	INT V1 = BatchedElements->AddVertex(FVector4(Position1.X,Position1.Y,0,1),TexCoord1,Color1,HitProxyId);
	INT V2 = BatchedElements->AddVertex(FVector4(Position2.X,Position2.Y,0,1),TexCoord2,Color2,HitProxyId);

	BatchedElements->AddTriangle(V0,V1,V2, BatchedElementParameters, BlendMode);
}

INT DrawStringCenteredZ(
	FCanvas* Canvas,
	FLOAT StartX,
	FLOAT StartY,
	FLOAT Z,
	const TCHAR* Text,
	class UFont* Font,
	const FLinearColor& Color
	)
{
	INT XL, YL;
	StringSize( Font, XL, YL, Text );

	return DrawStringZ( Canvas, StartX - ( XL / 2 ), StartY, Z, Text, Font, Color );
}

/** The color of the optional box that is drawn 'under' text */
const FLinearColor UE3_DrawStringOutlineBoxColor(0.0f, 0.0f, 0.0f, 0.75f);
/** The default offset of the outline box */
FIntRect UE3_DrawStringOutlineBoxOffset(2, 2, 4, 4);

INT DrawStringOutlinedCenteredZ(
	FCanvas* Canvas,
	FLOAT StartX,
	FLOAT StartY,
	FLOAT Z,
	const TCHAR* Text,
	class UFont* Font,
	const FLinearColor& Color,
	UBOOL bDrawBackgroundBox, 
	FIntRect& BackgroundBoxOffset
	)
{
	INT XL, YL;
	StringSize( Font, XL, YL, Text );

	FLOAT Left = StartX - ( XL / 2 );

	if (bDrawBackgroundBox == TRUE)
	{
		// Draw to the depth to fix up 3D issues
		DrawTileZ(Canvas, 
			Left - BackgroundBoxOffset.Min.X, StartY - BackgroundBoxOffset.Min.Y, Z,
			XL + BackgroundBoxOffset.Max.X, YL + BackgroundBoxOffset.Max.Y,  
			0.0f, 0.0f, 1.0f, 1.0f,
			GEngine->RemoveSurfaceMaterial->GetRenderProxy( FALSE ),
			TRUE
			);
	}

	DrawStringZ(Canvas, Left - 1.0f, StartY - 1.0f, Z, Text, Font, FLinearColor::Black );
	DrawStringZ(Canvas, Left - 1.0f, StartY + 1.0f, Z, Text, Font, FLinearColor::Black );
	DrawStringZ(Canvas, Left + 1.0f, StartY + 1.0f, Z, Text, Font, FLinearColor::Black );
	DrawStringZ(Canvas, Left + 1.0f, StartY - 1.0f, Z, Text, Font, FLinearColor::Black );

	return DrawStringZ( Canvas, Left, StartY, Z, Text, Font, Color );
}

INT DrawStringOutlinedZ(
	FCanvas* Canvas,
	FLOAT StartX,
	FLOAT StartY,
	FLOAT Depth,
	const TCHAR* Text,
	class UFont* Font,
	const FLinearColor& Color, 
	UBOOL bDrawBackgroundBox, 
	FIntRect& BackgroundBoxOffset,
	FLOAT XScale, 
	FLOAT YScale, 
	FLOAT HorizSpacingAdjust, 
	const FLOAT* ForcedViewportHeight,
	ESimpleElementBlendMode BlendMode, 
	UBOOL DoDraw,
	FLOAT XOffset,
	FLOAT YOffset,
	const FFontRenderInfo& RenderInfo
	)
{
	if (bDrawBackgroundBox == TRUE)
	{
		INT XL, YL;
		StringSize(Font, XL, YL, Text);

		// Draw to the depth to fix up 3D issues
		DrawTileZ(Canvas, 
			StartX - BackgroundBoxOffset.Min.X, StartY - BackgroundBoxOffset.Min.Y, Depth,
			XL + BackgroundBoxOffset.Max.X, YL + BackgroundBoxOffset.Max.Y, 
			0.0f, 0.0f, 1.0f, 1.0f,
			GEngine->RemoveSurfaceMaterial->GetRenderProxy( FALSE ),
			TRUE
			);
	}

#if !MOBILE
	// this many draw calls is actually too CPU intensive for mobile devices
	DrawStringZ( Canvas, StartX - 1.0f, StartY - 1.0f, 1.0f, Text, Font, FLinearColor::Black, XScale, YScale, HorizSpacingAdjust, ForcedViewportHeight, BlendMode, DoDraw, XOffset, YOffset, RenderInfo );
	DrawStringZ( Canvas, StartX - 1.0f, StartY + 1.0f, 1.0f, Text, Font, FLinearColor::Black, XScale, YScale, HorizSpacingAdjust, ForcedViewportHeight, BlendMode, DoDraw, XOffset, YOffset, RenderInfo );
	DrawStringZ( Canvas, StartX + 1.0f, StartY + 1.0f, 1.0f, Text, Font, FLinearColor::Black, XScale, YScale, HorizSpacingAdjust, ForcedViewportHeight, BlendMode, DoDraw, XOffset, YOffset, RenderInfo );
	DrawStringZ( Canvas, StartX + 1.0f, StartY - 1.0f, 1.0f, Text, Font, FLinearColor::Black, XScale, YScale, HorizSpacingAdjust, ForcedViewportHeight, BlendMode, DoDraw, XOffset, YOffset, RenderInfo );
#endif
	return DrawStringZ( Canvas, StartX, StartY, 1.0f, Text, Font, Color, XScale, YScale, HorizSpacingAdjust, ForcedViewportHeight, BlendMode, DoDraw, XOffset, YOffset, RenderInfo );
}

INT DrawShadowedString(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,const TCHAR* Text,class UFont* Font,const FLinearColor& Color)
{
	return DrawShadowedStringZ(Canvas, StartX, StartY, 1.0f, Text, Font, Color);
}

INT DrawShadowedStringZ(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,FLOAT Z,const TCHAR* Text,class UFont* Font,const FLinearColor& Color)
{
	if (Font->ImportOptions.bUseDistanceFieldAlpha)
	{	
		// just render text in single pass for distance field drop shadow
		return DrawStringZ(Canvas,StartX,StartY,Z, Text,Font,Color,1.0f,1.0f,0.0f,NULL,SE_BLEND_MaskedDistanceFieldShadowed);
	}

	// Draw a shadow of the text offset by 1 pixel in X and Y.
	DrawStringZ(Canvas,StartX + 1,StartY + 1,Z,Text,Font,FLinearColor::Black);

	// Draw the text.
	return DrawStringZ(Canvas,StartX,StartY,Z,Text,Font,Color);
}

INT DrawString(
	FCanvas* Canvas,
	FLOAT StartX,
	FLOAT StartY,
	const TCHAR* Text,
	class UFont* Font,
	const FLinearColor& Color,
	FLOAT XScale, 
	FLOAT YScale, 
	FLOAT HorizSpacingAdjust, 
	const FLOAT* ForcedViewportHeight/*=NULL*/,
	ESimpleElementBlendMode BlendMode/*=SE_BLEND_Translucent*/,
	UBOOL DoDraw/*=TRUE*/,
	FLOAT XShadowOffset/*=0.f*/,
	FLOAT YShadowOffset/*=1.f*/,
	const FFontRenderInfo& RenderInfo/*=FFontRenderInfo(EC_EventParm)*/)
{
    return DrawStringZ(Canvas, StartX, StartY, 1.0f, Text, Font, Color, XScale, YScale, HorizSpacingAdjust, ForcedViewportHeight, BlendMode, DoDraw, XShadowOffset, YShadowOffset, RenderInfo);
}

INT DrawStringZ(
	FCanvas* Canvas,
	FLOAT StartX,
	FLOAT StartY,
    FLOAT Depth,
	const TCHAR* Text,
	class UFont* Font,
	const FLinearColor& Color,
	FLOAT XScale, 
	FLOAT YScale, 
	FLOAT HorizSpacingAdjust, 
	const FLOAT* ForcedViewportHeight/*=NULL*/,
	ESimpleElementBlendMode BlendMode/*=SE_BLEND_Translucent*/,
	UBOOL DoDraw/*=TRUE*/,
	FLOAT XShadowOffset/*=0.f*/,
	FLOAT YShadowOffset/*=1.f*/,
	const FFontRenderInfo& RenderInfo/*=FFontRenderInfo(EC_EventParm)*/)
{
	SCOPE_CYCLE_COUNTER(STAT_Canvas_DrawStringTime);

	if(Font == NULL || Text == NULL)
	{
		return FALSE;
	}

	// Get the scaling and resolution information from the font.
	const FLOAT FontResolutionTest = (ForcedViewportHeight && *ForcedViewportHeight != 0) ? *ForcedViewportHeight : Canvas->GetRenderTarget()->GetSizeY();
	const INT PageIndex = Font->GetResolutionPageIndex(FontResolutionTest);
	const FLOAT FontScale = Font->GetScalingFactor(FontResolutionTest);

	// apply the font's internal scale to the desired scaling
	XScale *= FontScale;
	YScale *= FontScale;

	FLinearColor ActualColor = Color;
	ActualColor.A *= Canvas->AlphaModulate;

	FBatchedElements* BatchedElements = NULL;
	if (Font->ImportOptions.bUseDistanceFieldAlpha)
	{
		// convert blend mode to distance field type
		switch(BlendMode)
		{
		case SE_BLEND_Translucent:
			BlendMode = (RenderInfo.bEnableShadow) ? SE_BLEND_TranslucentDistanceFieldShadowed : SE_BLEND_TranslucentDistanceField;
			break;
		case SE_BLEND_Masked:
			BlendMode = (RenderInfo.bEnableShadow) ? SE_BLEND_MaskedDistanceFieldShadowed : SE_BLEND_MaskedDistanceField;
			break;
		};
	}	
	FHitProxyId HitProxyId = Canvas->GetHitProxyId();
	FTexture* LastTexture = NULL;
	UTexture2D* Tex = NULL;

	const FLOAT CharIncrement = ( (FLOAT)Font->Kerning + HorizSpacingAdjust ) * XScale;

	// if not rendering a shadow then eliminate the offset
	if (BlendMode != SE_BLEND_TranslucentDistanceFieldShadowed && BlendMode != SE_BLEND_MaskedDistanceFieldShadowed)
	{
		XShadowOffset = 0.f;
		YShadowOffset = 0.f;
	}

	// constant for all characters
	FLOAT S3DDisparity = GetStereoscopicDisparity(Depth);

	FLOAT LineX = 0;
	INT TextLen = appStrlen(Text);
	if ( DoDraw )
	{
		// Draw all characters in string.
		for( INT i=0; i < TextLen; i++ )
		{
			INT Ch = (INT)Font->RemapChar(Text[i]);

			// Process character if it's valid.
			if( Font->Characters.IsValidIndex(Ch + PageIndex) )
			{
				FFontCharacter& Char = Font->Characters(Ch + PageIndex);
				if( Font->Textures.IsValidIndex(Char.TextureIndex) && 
					(Tex=Font->Textures(Char.TextureIndex))!=NULL && 
					Tex->Resource != NULL )
				{
					if( LastTexture != Tex->Resource || BatchedElements == NULL )
					{
						FBatchedElementParameters* BatchedElementParameters = NULL;
						BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, Tex->Resource, BlendMode, RenderInfo.GlowInfo);

						// trade-off between memory and performance by pre-allocating more reserved space 
						// for the triangles/vertices of the batched elements used to render the text tiles
						//BatchedElements->AddReserveTriangles(TextLen*2,Tex->Resource,BlendMode);
						//BatchedElements->AddReserveVertices(TextLen*4);
					}
					LastTexture = Tex->Resource;

					const FLOAT X      = LineX + StartX;
					const FLOAT Y      = StartY + Char.VerticalOffset * YScale;
					FLOAT SizeX = (Char.USize + XShadowOffset) * XScale;
					const FLOAT SizeY = (Char.VSize + YShadowOffset) * YScale;
					const FLOAT U     = Char.StartU / (FLOAT)Tex->OriginalSizeX;
					const FLOAT V     = Char.StartV / (FLOAT)Tex->OriginalSizeY;
					const FLOAT SizeU = (Char.USize + XShadowOffset) / (FLOAT)Tex->OriginalSizeX;
					const FLOAT SizeV = (Char.VSize + YShadowOffset) / (FLOAT)Tex->OriginalSizeY;				

                    FLOAT Left, Top, Right, Bottom;
                    Left = X * Depth;
                    Top = Y * Depth;
                    Right = (X + SizeX) * Depth;
                    Bottom = (Y + SizeY) * Depth;

					// adjust for 3D
					Left += S3DDisparity;
					Right += S3DDisparity;

					INT V00 = BatchedElements->AddVertex(FVector4(Left,		Top,			0,Depth),FVector2D(U,			V),			ActualColor,HitProxyId);
					INT V10 = BatchedElements->AddVertex(FVector4(Right,    Top,			0,Depth),FVector2D(U + SizeU,	V),			ActualColor,HitProxyId);
					INT V01 = BatchedElements->AddVertex(FVector4(Left,		Bottom,	0,Depth),FVector2D(U,			V + SizeV),	ActualColor,HitProxyId);
					INT V11 = BatchedElements->AddVertex(FVector4(Right,    Bottom,	0,Depth),FVector2D(U + SizeU,	V + SizeV),	ActualColor,HitProxyId);

					BatchedElements->AddTriangle(V00, V10, V11, Tex->Resource, BlendMode, RenderInfo.GlowInfo);
					BatchedElements->AddTriangle(V00, V11, V01, Tex->Resource, BlendMode, RenderInfo.GlowInfo);

					// if we have another non-whitespace character to render, add the font's kerning.
					if ( Text[i+1] && !appIsWhitespace(Text[i+1]) )
					{
						SizeX += CharIncrement;
					}

					// Update the current rendering position
					LineX += SizeX;
				}
			}
		}
	}
	else
	{
		// Don't actually draw, just compute the size.
		// This is a copy of the draw code stripped down to only compute size.
		for( INT i=0; i < TextLen; i++ )
		{
			INT Ch = (INT)Font->RemapChar(Text[i]);

			// Process character if it's valid.
			if( Font->Characters.IsValidIndex(Ch + PageIndex) )
			{
				FFontCharacter& Char = Font->Characters(Ch + PageIndex);
				if( Font->Textures.IsValidIndex(Char.TextureIndex) && 
					(Tex=Font->Textures(Char.TextureIndex))!=NULL && 
					Tex->Resource != NULL )
				{
					FLOAT SizeX = (Char.USize + XShadowOffset) * XScale;

					// if we have another non-whitespace character to render, add the font's kerning.
					if ( Text[i+1] && !appIsWhitespace(Text[i+1]) )
					{
						SizeX += CharIncrement;
					}

					// Update the current rendering position
					LineX += SizeX;
				}
			}
		}
	}

	return appTrunc(LineX);
}

/**
* Render string using both a font and a material. The material should have a font exposed as a 
* parameter so that the correct font page can be set based on the character being drawn.
*
* @param Canvas - valid canvas for rendering tiles
* @param StartX - starting X screen position
* @param StartY - starting Y screen position
* @param XScale - scale of text rendering in X direction
* @param YScale - scale of text rendering in Y direction
* @param Text - string of text to be rendered
* @param Font - font containing texture pages of character glyphs
* @param MatInst - material with a font parameter
* @param FontParam - name of the font parameter in the material
* @return total size in pixels of text drawn
*/
INT DrawStringMat(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,FLOAT XScale,FLOAT YScale,FLOAT HorizSpacingAdjust,const TCHAR* Text,class UFont* Font,UMaterialInterface* MatInst,const TCHAR* FontParam)
{
    return DrawStringMatZ(Canvas, StartX, StartY, 1.0f, XScale, YScale, HorizSpacingAdjust, Text, Font, MatInst, FontParam);
}

INT DrawStringMatZ(FCanvas* Canvas,FLOAT StartX,FLOAT StartY,FLOAT Z,FLOAT XScale,FLOAT YScale,FLOAT HorizSpacingAdjust,const TCHAR* Text,class UFont* Font,UMaterialInterface* MatInst,const TCHAR* FontParam)
{
	checkSlow(Canvas);

	INT Result = 0;	
	if( Font && Text )
	{
		if( MatInst )
		{
			// check for valid font parameter name
			UFont* TempFont;
			INT TempFontPage;
			if( !FontParam || 
				!MatInst->GetFontParameterValue(FName(FontParam),TempFont,TempFontPage) )
			{
				//debugf(NAME_Warning,TEXT("Invalid font parameter name [%s]"),FontParam ? FontParam : TEXT("NULL"));
				Result = DrawStringZ(Canvas,StartX,StartY,Z,Text,Font,FLinearColor(0,1,0,1),XScale,YScale);
			}
			else
			{
				// Get the scaling and resolution information from the font.
				const FLOAT FontResolutionTest = Canvas->GetRenderTarget()->GetSizeY();
				const INT PageIndex = Font->GetResolutionPageIndex(FontResolutionTest);
				const FLOAT FontScale = Font->GetScalingFactor(FontResolutionTest);

				// apply the font's internal scale to the desired scaling
				XScale *= FontScale;
				YScale *= FontScale;

				// create a FFontMaterialRenderProxy for each font page
				TArray<FFontMaterialRenderProxy> FontMats;
				for( INT FontPage=0; FontPage < Font->Textures.Num(); FontPage++ )
				{
					new(FontMats) FFontMaterialRenderProxy(MatInst->GetRenderProxy(FALSE),Font,FontPage,FName(FontParam));
				}
				// Draw all characters in string.
				FLOAT LineX = 0;
				for( INT i=0; Text[i]; i++ )
				{
					// unicode mapping of text
					INT Ch = (INT)Font->RemapChar(Text[i]);
					// Process character if it's valid.
					if( Ch < Font->Characters.Num() )
					{
						UTexture2D* Tex = NULL;
						FFontCharacter& Char = Font->Characters(Ch);
						// only render fonts with a valid texture page
						if( Font->Textures.IsValidIndex(Char.TextureIndex) && 
							(Tex=Font->Textures(Char.TextureIndex)) != NULL )
						{
							const FLOAT X			= LineX + StartX;
							const FLOAT Y			= StartY + Char.VerticalOffset * YScale;
							const FLOAT CU			= Char.StartU;
							const FLOAT CV			= Char.StartV;
							const FLOAT CUSize		= Char.USize;
							const FLOAT CVSize		= Char.VSize;
							const FLOAT ScaledSizeU	= CUSize * XScale;
							const FLOAT ScaledSizeV	= CVSize * YScale;

							// Draw using the font material instance
							DrawTileZ(
								Canvas,
								X,
								Y,
                                Z,
								ScaledSizeU,
								ScaledSizeU,
								CU		/ (FLOAT)Tex->SizeX,
								CV		/ (FLOAT)Tex->SizeY,
								CUSize	/ (FLOAT)Tex->SizeX,
								CVSize	/ (FLOAT)Tex->SizeY,
								&FontMats(Char.TextureIndex)
								);

							// Update the current rendering position
							LineX += ScaledSizeU;

							// if we have another non-whitespace character to render, add the font's kerning.
							if ( Text[i+1] && !appIsWhitespace(Text[i+1]) )
							{
								LineX += XScale * ( ( FLOAT )Font->Kerning + HorizSpacingAdjust );
							}
						}
					}
				}
				// @todo - temp fix to allow for material tiles rendered with FFontMaterialRenderProxy
				// need to allow for material tile batches to take ownership of FMaterialRenderProxy
				Canvas->Flush(TRUE);
				// return the resulting line position
				Result = appTrunc(LineX);
			}
		}
		else
		{
			// fallback to just using the font texture without a material
			Result = DrawString(Canvas,StartX,StartY,Text,Font,FLinearColor(0,1,0,1),XScale,YScale);
		}
	}

	return Result;
}

void StringSize(UFont* Font,INT& XL,INT& YL,const TCHAR* Text)
{
	// this functionality has been moved to a static function in UIString
	FTextSizingParameters Parameters(Font,1.f,1.f);
	UCanvas::CanvasStringSize(Parameters, Text);
	XL = appTrunc(Parameters.DrawXL);
	YL = appTrunc(Parameters.DrawYL);
}

/**
 * Calculates the width and height of a typical character in the specified font.
 *
 * @param	DrawFont			the font to use for calculating the character size
 * @param	DefaultCharWidth	[out] will be set to the width of the typical character
 * @param	DefaultCharHeight	[out] will be set to the height of the typical character
 * @param	pDefaultChar		if specified, pointer to a single character to use for calculating the default character size
 */
static void GetDefaultCharSize( UFont* DrawFont, FLOAT& DefaultCharWidth, FLOAT& DefaultCharHeight, FLOAT ViewportHeight, const TCHAR* pDefaultChar=NULL )
{
	// Get the scaling and resolution information from the font.
	const INT PageIndex = DrawFont->GetResolutionPageIndex(ViewportHeight);

	TCHAR DefaultChar = pDefaultChar != NULL ? *pDefaultChar : TEXT('0');
	DrawFont->GetCharSize(DefaultChar, DefaultCharWidth, DefaultCharHeight, PageIndex);
	if ( DefaultCharWidth == 0 )
	{
		// this font doesn't contain '0', try 'A'
		DrawFont->GetCharSize(TEXT('A'), DefaultCharWidth, DefaultCharHeight, PageIndex);
	}
}

/**
 * Calculates the size of the specified string.
 *
 * @param	Parameters	Used for various purposes
 *							DrawXL:		[out] will be set to the width of the string
 *							DrawYL:		[out] will be set to the height of the string
 *							DrawFont:	specifies the font to use for retrieving the size of the characters in the string
 *							Scale:		specifies the amount of scaling to apply to the string
 * @param	pText		the string to calculate the size for
 * @param	EOL			a pointer to a single character that is used as the end-of-line marker in this string
 * @param	bStripTrailingCharSpace
 *						whether the inter-character spacing following the last character should be included in the calculated width of the result string
 */
void UCanvas::CanvasStringSize( FTextSizingParameters& Parameters, const TCHAR* pText, const TCHAR* EOL/*=NULL*/, UBOOL bStripTrailingCharSpace/*=TRUE*/ )
{
	Parameters.DrawXL = 0.f;
	Parameters.DrawYL = 0.f;

	if( Parameters.DrawFont )
	{

		const TCHAR* pCurrentPos = pText;

		// Get the scaling and resolution information from the font.
		const FLOAT FontResolutionTest = GEngine != NULL && GEngine->GameViewport && GEngine->GameViewport->Viewport != NULL 
			? GEngine->GameViewport->Viewport->GetSizeY() : 768.f;
		const INT PageIndex = Parameters.DrawFont->GetResolutionPageIndex(FontResolutionTest);
		const FLOAT FontScale = Parameters.DrawFont->GetScalingFactor(FontResolutionTest);

		// get a default character width and height to be used for non-renderable characters
		FLOAT DefaultCharWidth, DefaultCharHeight;
		GetDefaultCharSize( Parameters.DrawFont, DefaultCharWidth, DefaultCharHeight, FontResolutionTest );

		// we'll need to use scaling in multiple places, so create a variable to hold it so that if we modify
		// how the scale is calculated we only have to update these two lines
		const FLOAT ScaleX = Parameters.Scaling.X * FontScale;
		const FLOAT ScaleY = Parameters.Scaling.Y * FontScale;


		const FLOAT CharIncrement = ( (FLOAT)Parameters.DrawFont->Kerning + Parameters.SpacingAdjust.X ) * ScaleX;
		const FLOAT DefaultScaledHeight = DefaultCharHeight * ScaleY + Parameters.SpacingAdjust.Y * ScaleY;
		while ( *pCurrentPos )
		{
			FLOAT CharWidth, CharHeight;

			// if an EOL character was specified, skip over that character in the stream
			if ( EOL != NULL )
			{
				while ( *pCurrentPos == *EOL )
				{
					Parameters.DrawYL = Max(Parameters.DrawYL, DefaultScaledHeight);
					pCurrentPos++;
				}

				if ( *pCurrentPos == 0 )
				{
					break;
				}
			}

			TCHAR Ch = *pCurrentPos++;
			Parameters.DrawFont->GetCharSize(Ch, CharWidth, CharHeight,PageIndex);
			if ( CharHeight == 0 && Ch == TEXT('\n') )
			{
				CharHeight = DefaultCharHeight;
			}
			CharWidth *= ScaleX;
			CharHeight *= ScaleY;

			// never add character spacing if the next character is whitespace
			if ( !appIsWhitespace(*pCurrentPos) )
			{
				// if we have another character or desire trailing char spacing to be included, append the character spacing
				if ( *pCurrentPos || !bStripTrailingCharSpace )
				{
					CharWidth += CharIncrement;
				}
			}

			const FLOAT ScaledVertSpacing = Parameters.SpacingAdjust.Y * ScaleY;

			Parameters.DrawXL += CharWidth;
			Parameters.DrawYL = Max<FLOAT>(Parameters.DrawYL, CharHeight + ScaledVertSpacing );
		}
	}
}


/**
 * Parses a single string into an array of strings that will fit inside the specified bounding region.
 *
 * @param	Parameters		Used for various purposes:
 *							DrawX:		[in] specifies the pixel location of the start of the horizontal bounding region that should be used for wrapping.
 *							DrawY:		[in] specifies the Y origin of the bounding region.  This should normally be set to 0, as this will be
 *										     used as the base value for DrawYL.
 *										[out] Will be set to the Y position (+YL) of the last line, i.e. the total height of all wrapped lines relative to the start of the bounding region
 *							DrawXL:		[in] specifies the pixel location of the end of the horizontal bounding region that should be used for wrapping
 *							DrawYL:		[in] specifies the height of the bounding region, in pixels.  A input value of 0 indicates that
 *										     the bounding region height should not be considered.  Once the total height of lines reaches this
 *										     value, the function returns and no further processing occurs.
 *							DrawFont:	[in] specifies the font to use for retrieving the size of the characters in the string
 *							Scaling:	[in] specifies the amount of scaling to apply to the string
 * @param	CurX			specifies the pixel location to begin the wrapping; usually equal to the X pos of the bounding region, unless wrapping is initiated
 * @param	pText			the text that should be wrapped
 * @param	out_Lines		[out] will contain an array of strings which fit inside the bounding region specified.  Does
 *							not clear the array first.
 * @param	EOL				a pointer to a single character that is used as the end-of-line marker in this string, for manual line breaks
 * @param	MaxLines		the maximum number of lines that can be created.
 */
void UCanvas::WrapString( FTextSizingParameters& Parameters, FLOAT CurX, const TCHAR* pText, TArray<FWrappedStringElement>& out_Lines, const TCHAR* EOL/*=NULL*/, INT MaxLines/*=MAXINT*/ )
{
	check(pText != NULL);
	check(Parameters.DrawFont != NULL);

	if ( *pText == 0 )
	{
		return;
	}

	const FLOAT OrigX = Parameters.DrawX;
	const FLOAT OrigY = Parameters.DrawY;
	const FLOAT ClipX = Parameters.DrawXL;
	const FLOAT ClipY = Parameters.DrawYL == 0 ? MAX_FLT : (OrigY + Parameters.DrawYL);

	// Get the scaling and resolution information from the font.
	const FLOAT ViewportHeight = Parameters.ViewportHeight > 0.f 
		? Parameters.ViewportHeight
		: (GEngine != NULL && GEngine->GameViewport && GEngine->GameViewport->Viewport != NULL
			? GEngine->GameViewport->Viewport->GetSizeY() : 768.f);

	const INT PageIndex = Parameters.DrawFont->GetResolutionPageIndex(ViewportHeight);
	const FLOAT FontScale = Parameters.DrawFont->GetScalingFactor(ViewportHeight);

	// we'll need to use scaling in multiple places, so create a variable to hold it so that if we modify
	// how the scale is calculated we only have to update these two lines
	const FLOAT ScaleX = Parameters.Scaling.X * FontScale;
	const FLOAT ScaleY = Parameters.Scaling.Y * FontScale;

	// calculate the width of the default character for this font; this will be used as the size for non-printable characters
	FLOAT DefaultCharWidth, DefaultCharHeight;
	GetDefaultCharSize(Parameters.DrawFont, DefaultCharWidth, DefaultCharHeight, ViewportHeight, EOL);
	DefaultCharWidth *= ScaleX;
	DefaultCharHeight *= ScaleY;

	// used to determine what to do if we run out of space before the first word has been processed...
	// if the starting position indicates that this line is indented, we'll assume this means that destination region had
	// existing text on the starting line - in this case, we'll just wrap the entire word to the next line
	UBOOL bIndented = CurX != OrigX;

	// this represents the total height of the wrapped lines, relative to the Y position of the bounding region
	FLOAT& LineExtentY = Parameters.DrawY;

	// how much spacing to insert between each character
	//@note: add any additional spacing between characters here
	FLOAT CharIncrement = ( (FLOAT)Parameters.DrawFont->Kerning + Parameters.SpacingAdjust.X ) * ScaleX;

	const FLOAT ScaledVertSpacing = Parameters.SpacingAdjust.Y * ScaleY;

	INT LineStartPosition, WordStartPosition;
	LineStartPosition = WordStartPosition = 0;//FLocalizedWordWrapHelper::GetStartingPosition(pText);
	INT WordEndPosition = FLocalizedWordWrapHelper::GetNextBreakPosition(pText, WordStartPosition);

	const TCHAR* pCurrent = pText;
	while ( *pCurrent != 0 && out_Lines.Num() < MaxLines )
	{
 		// for each leading line break, add an empty line to the output array
		while ( FLocalizedWordWrapHelper::IsLineBreak(pText, pCurrent - pText, EOL) && out_Lines.Num() < MaxLines )
		{
			// if the next line would fall outside the clipping region, stop here
			if ( LineExtentY + DefaultCharHeight + ScaledVertSpacing > ClipY + 0.25f )
				return;

			new(out_Lines) FWrappedStringElement(TEXT(""),DefaultCharWidth,DefaultCharHeight + ScaledVertSpacing);
			LineExtentY += DefaultCharHeight + ScaledVertSpacing;

			// no longer indented
			bIndented = FALSE;

			// reset the starting position to the left edge of the bounding region for calculating the next line
			CurX  = OrigX;

			// since we've processed this character, advance all tracking variables forward by one
			pCurrent++;
			WordStartPosition++;
			LineStartPosition++;

		}

		// the running width of the characters evaluated in this line so far
		FLOAT TestXL = CurX;
		// the maximum height of the characters evaluated in this line so far
		FLOAT TestYL = 0;
		// the combined width of the characters from the beginning of the line to the wrap position
		FLOAT LineXL = 0;
		// the maximum height of the characters from the beginning of the line to the wrap position
		FLOAT LineYL = 0;
		// the maximum height of the characters from the beginning of the line to the last character processed before the line overflowed
		FLOAT LastYL = 0;
		// the width and height of the last character that was processed
		FLOAT ChW=0, ChH=0;

		UBOOL bProcessedWord = FALSE;
		INT ProcessedCharacters = 0;

		// Process each word until the current line overflows.
		for ( ; WordEndPosition != INDEX_NONE;
			WordStartPosition = WordEndPosition, WordEndPosition = FLocalizedWordWrapHelper::GetNextBreakPosition(pText, WordStartPosition) )
		{
			UBOOL bWordOverflowed = FALSE;
			UBOOL bManualLineBreak = FALSE;

			// calculate the widths of the characters in this word - if this word fits within the bounding region,
			// append the word width to the total line width; otherwise, create an entry in the output array containing the text
			// to the left of the current word, then reset all line tracking variables and start over from the beginning of the current word.
			for ( INT CharIndex = WordStartPosition; CharIndex < WordEndPosition; CharIndex++ )
			{
				if ( FLocalizedWordWrapHelper::IsLineBreak(pText, CharIndex, EOL) )
				{
					WordStartPosition = CharIndex;
					bManualLineBreak = TRUE;
					break;
				}

				Parameters.DrawFont->GetCharSize(pText[CharIndex], ChW, ChH, PageIndex);
				ChW *= ScaleX;
				ChH *= ScaleY;

				// if we have at least one more character in the string, add the inter-character spacing here
				if ( pText[CharIndex+1] && !appIsWhitespace(pText[CharIndex+1]) )
				{
					ChW += CharIncrement;
				}

				TestXL += ChW;
				TestYL = Max(TestYL, ChH + ScaledVertSpacing);

				if ( TestXL > ClipX && !appIsWhitespace(pText[CharIndex]) )
				{
					bWordOverflowed = TRUE;
					break;
				}

				LastYL = TestYL;
				ProcessedCharacters++;
			}

			if ( bWordOverflowed == TRUE )
			{
				break;
			}

			bProcessedWord = TRUE;
			LineXL = TestXL - CurX;
			LineYL = TestYL;

			if ( bManualLineBreak == TRUE )
			{
				if ( LineYL == 0 )
				{
					LineYL = DefaultCharHeight + ScaledVertSpacing;
				}
				break;
			}
		}

		// if the next line would fall outside the clipping region, stop here
		if ( LineExtentY + LineYL + ScaledVertSpacing > ClipY + 0.25f )
		{
			break;
		}

		// CreateNewLine
		
		// we've reached the end of the current line - add a new element to the output array which contains the character we've processed so far
		// WordStartPosition is now the position of the first character that should be on the next line, so the current line is everything from
		// the start of the current line up to WordStartPosition
		INT LineLength = WordStartPosition - LineStartPosition;

		FWrappedStringElement* CurrentLine = NULL;
		if ( bProcessedWord == FALSE && bIndented == TRUE )
		{
			// we didn't have enough space for the first word.  If indented, we shouldn't break the line in
			// the middle of the word; rather, create a blank line and wrap the entire word to the next line
			CurrentLine = new(out_Lines) FWrappedStringElement(TEXT(""),0,DefaultCharHeight + ScaledVertSpacing);
			LineExtentY += DefaultCharHeight + ScaledVertSpacing;

			// we're going to advance the pCurrent pointer by LineLength below, but since all the characters we processed are going on the next line,
			// we don't want to advance the pCurrent pointer at all
			LineLength = 0;
		}
		else
		{
			if ( bProcessedWord == FALSE && ProcessedCharacters == 0 )
			{
				// there wasn't enough room in the bounding region for the first character
				if ( *pCurrent != 0 )
				{
					CurrentLine = new(out_Lines) FWrappedStringElement(*FString::Printf(TEXT("%c"), *pCurrent++), TestXL, TestYL);
					LineExtentY += TestYL;
					WordStartPosition++;
				}
				else
				{
					// we've reached the end of the string
					break;
				}
			}
			else
			{
				if ( LineLength == 0 )
				{
					// not enough room to process the first word - we'll have to break up the word

					// set the line length to the number of characters that will fit into the bounding region
					LineLength = ProcessedCharacters;

					// advance the position to start the next line by the number of characters processed
					WordStartPosition += ProcessedCharacters;

					// now calculate the length and height of the processed characters - TestXL and TestYL include the dimensions of the character that
					// overflowed the bounds (which means it won't be part of this line), so subtract the last character's width from the running width
					// and use the height calculated after processing the previous character
					LineXL = TestXL - ChW;
					LineYL = LastYL;
				}

				// we've run out of space - copy the characters up to the beginning of the current word into the line text
				FString LineString;
				TArray<TCHAR>& LineStringChars = LineString.GetCharArray();
				if ( pCurrent )
				{
					LineStringChars.Add(LineLength + 1);
					appStrncpy(&LineStringChars(0), pCurrent, LineLength+1);

					CurrentLine = new(out_Lines) FWrappedStringElement(*LineString, LineXL, LineYL);
				}
				LineExtentY += TestYL;
			}
		}

		// reset the starting position to the left edge of the bounding region for calculating the next line
		CurX  = OrigX;

		// advance the character pointer beyond the characters that we just added to the wrapped line
		pCurrent += LineLength;

		// no longer indented
		bIndented = FALSE;

		if ( *pCurrent && FLocalizedWordWrapHelper::IsLineBreak(pText, pCurrent - pText, EOL) )
		{
			// if this line was forced by a manual line break, advance past the line break
			// so that we don't create a blank line at the top of this loop
			pCurrent++;
			WordStartPosition++;
		}

		// reset the line start marker to point to the beginning of the current word
		LineStartPosition = WordStartPosition;
	}
}

/*-----------------------------------------------------------------------------
	UCanvas object functions.
-----------------------------------------------------------------------------*/

void UCanvas::Init()
{
}

void UCanvas::Update()
{
	// Call UnrealScript to reset.
	eventReset();

	// Copy size parameters from viewport.
	ClipX = SizeX;
	ClipY = SizeY;

}

/*-----------------------------------------------------------------------------
	UCanvas scaled sprites.
-----------------------------------------------------------------------------*/

void UCanvas::SetPos(FLOAT X, FLOAT Y, FLOAT Z)
{
	CurX = X;
	CurY = Y;
    CurZ = Z > 0.0f ? Z : 0.1f; // Ensure that Z is positive and non-zero.
}

void UCanvas::SetDrawColor(BYTE R, BYTE G, BYTE B, BYTE A)
{
	DrawColor.R = R;
	DrawColor.G = G;
	DrawColor.B = B;
	DrawColor.A = A;
}

/**
 * Translate EBlendMode into ESimpleElementBlendMode used by tiles
 * 
 * @param BlendMode Normal UE3 blend mode enum
 *
 * @return simple element rendering blend mode enum
 */
static ESimpleElementBlendMode BlendToSimpleElementBlend(EBlendMode BlendMode)
{
	switch (BlendMode)
	{
		case BLEND_Opaque:
			return SE_BLEND_Opaque;
		case BLEND_Masked:
			return SE_BLEND_Masked;
		case BLEND_Additive:
			return SE_BLEND_Additive;
		case BLEND_Modulate:
			return SE_BLEND_Modulate;
		case BLEND_ModulateAndAdd:
			return SE_BLEND_ModulateAndAdd;
		case BLEND_Translucent:
		case BLEND_DitheredTranslucent:
		default:
			return SE_BLEND_Translucent;
	};
}

//
// Draw arbitrary aligned rectangle.
//
void UCanvas::DrawTile
(
	UTexture*			Tex,
	FLOAT				X,
	FLOAT				Y,
    FLOAT				Z,
	FLOAT				XL,
	FLOAT				YL,
	FLOAT				U,
	FLOAT				V,
	FLOAT				UL,
	FLOAT				VL,
	const FLinearColor&	Color,
	EBlendMode BlendMode
)
{
    if ( !Canvas || !Tex ) 
        return;
	FLOAT MyClipX = OrgX + ClipX;
	FLOAT MyClipY = OrgY + ClipY;
	FLOAT w = X + XL > MyClipX ? MyClipX - X : XL;
	FLOAT h = Y + YL > MyClipY ? MyClipY - Y : YL;
	if (XL > 0.f &&
		YL > 0.f)
	{
		// here we use the original size of the texture, not the current size (for instance, 
		// PVRTC textures, on some platforms anyway, need to be square), but the script code
		// was written using 0..TexSize coordinates, not 0..1, so to make the 0..1 coords
		// we divide by what the texture was when the script code was written
		FLOAT SizeX = Tex->GetOriginalSurfaceWidth();
		FLOAT SizeY = Tex->GetOriginalSurfaceHeight();

		::DrawTileZ(
			Canvas,
			(X),
			(Y),
            (Z),
			(w),
			(h),
			U/SizeX,
			V/SizeY,
			UL/SizeX * w/XL,
			VL/SizeY * h/YL,
			Color,
			Tex->Resource,
			BlendToSimpleElementBlend((EBlendMode)BlendMode));
	}
}

/**
 * Translate ECanvasBlendMode into ESimpleElementBlendMode used by tiles
 * 
 * @param BlendMode Canvas blend mode.
 *
 * @return simple element rendering blend mode enum
 */
static ESimpleElementBlendMode CanvasBlendToSimpleElementBlend(ECanvasBlendMode BlendMode)
{
	switch (BlendMode)
	{
		case BLEND_CANVAS_Opaque:
			return SE_BLEND_Opaque;
		case BLEND_CANVAS_Masked:
			return SE_BLEND_Masked;
		case BLEND_CANVAS_Additive:
			return SE_BLEND_Additive;
		case BLEND_CANVAS_Modulate:
			return SE_BLEND_Modulate;
		case BLEND_CANVAS_ModulateAndAdd:
			return SE_BLEND_ModulateAndAdd;
		case BLEND_CANVAS_AlphaOnly:
			return SE_BLEND_AlphaOnly;
		case BLEND_CANVAS_Translucent:
		case BLEND_CANVAS_DitheredTranslucent:
		default:
			return SE_BLEND_Translucent;
	};
}

/**
 * Draw a texture to the canvas using one of the special Canvas blend modes
 *
 * @param	Tex		The texture to draw onto the canvas
 * @param	XL/YL	Size on canvas (starting pos is CurX/CurY)
 * @param	U/V/UL/VL	Texture coordinates
 * @param	Blend	The ECanvasBlendMode to use for drawing the Texture
 *
 **/
void UCanvas::DrawBlendedTile(UTexture* Tex, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, BYTE Blend)
{
	if (Tex != NULL)
	{
		DrawTile(Tex, OrgX + CurX, OrgY + CurY, CurZ, XL, YL, U, V, UL, VL, DrawColor, (ECanvasBlendMode)Blend);
	}
}

//
// Draw arbitrary aligned rectangle using a specified canvas blend mode.
//
void UCanvas::DrawTile
(
	UTexture*			Tex,
	FLOAT				X,
	FLOAT				Y,
	FLOAT				Z,
	FLOAT				XL,
	FLOAT				YL,
	FLOAT				U,
	FLOAT				V,
	FLOAT				UL,
	FLOAT				VL,
	const FLinearColor&	Color,
	ECanvasBlendMode BlendMode
)
{
	if ( !Canvas || !Tex ) 
		return;
	FLOAT MyClipX = OrgX + ClipX;
	FLOAT MyClipY = OrgY + ClipY;
	FLOAT w = X + XL > MyClipX ? MyClipX - X : XL;
	FLOAT h = Y + YL > MyClipY ? MyClipY - Y : YL;
	if (XL > 0.f &&
		YL > 0.f)
	{
		// here we use the original size of the texture, not the current size (for instance, 
		// PVRTC textures, on some platforms anyway, need to be square), but the script code
		// was written using 0..TexSize coordinates, not 0..1, so to make the 0..1 coords
		// we divide by what the texture was when the script code was written
		FLOAT SizeX = Tex->GetOriginalSurfaceWidth();
		FLOAT SizeY = Tex->GetOriginalSurfaceHeight();

		::DrawTileZ(
			Canvas,
			(X),
			(Y),
			(Z),
			(w),
			(h),
			U/SizeX,
			V/SizeY,
			UL/SizeX * w/XL,
			VL/SizeY * h/YL,
			Color,
			Tex->Resource,
			CanvasBlendToSimpleElementBlend((ECanvasBlendMode)BlendMode));
	}
}

void UCanvas::DrawMaterialTile(
	UMaterialInterface*	Material,
	FLOAT				X,
	FLOAT				Y,
    FLOAT               Z,
	FLOAT				XL,
	FLOAT				YL,
	FLOAT				U,
	FLOAT				V,
	FLOAT				UL,
	FLOAT				VL
	)
{
    if ( !Canvas || !Material ) 
        return;
	::DrawTileZ(
		Canvas,
		(X),
		(Y),
        (Z),
		(XL),
		(YL),
		U,
		V,
		UL,
		VL,
		Material->GetRenderProxy(0)
		);
}

void UCanvas::ClippedStrLen( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Text )
{
	XL = 0;
	YL = 0;
	if (Font != NULL)
	{
		FTextSizingParameters Parameters(Font,ScaleX,ScaleY);
		CanvasStringSize(Parameters, Text);

		XL = appTrunc(Parameters.DrawXL);
		YL = appTrunc(Parameters.DrawYL);
	}
}

//
// Calculate the size of a string built from a font, word wrapped
// to a specified region.
//
void VARARGS UCanvas::WrappedStrLenf( UFont* Font, FLOAT ScaleX, FLOAT ScaleY, INT& XL, INT& YL, const TCHAR* Fmt, ... ) 
{
	TCHAR Text[4096];
	GET_VARARGS( Text, ARRAY_COUNT(Text), ARRAY_COUNT(Text)-1, Fmt, Fmt );

	WrappedPrint( FALSE, XL, YL, Font, ScaleX, ScaleY, FALSE, Text ); 
}

//
// Compute size and optionally print text with word wrap.
//!!For the next generation, redesign to ignore CurX,CurY.
//
INT UCanvas::WrappedPrint(UBOOL Draw, INT& out_XL, INT& out_YL, UFont* Font, FLOAT ScaleX, FLOAT ScaleY, UBOOL Center, const TCHAR* Text, const FFontRenderInfo& RenderInfo) 
{
	if (ClipX < 0 || ClipY < 0)
	{
		return 0;
	}
	if (Font == NULL)
	{
		debugf(NAME_Warning, TEXT("UCanvas::WrappedPrint() called with a NULL Font!"));
		return 0;
	}

	FTextSizingParameters RenderParms(0.f,0.f,ClipX - (OrgX + CurX),0.f,Font,0.f);
	RenderParms.Scaling.X = ScaleX;
	RenderParms.Scaling.Y = ScaleY;

	TArray<FWrappedStringElement> WrappedStrings;
	WrapString(RenderParms,0,Text,WrappedStrings);

	FLOAT DrawX = OrgX + CurX;
	FLOAT DrawY = OrgY + CurY;
	FLOAT XL = 0.f;
	FLOAT YL = 0.f;
	for (INT Idx = 0; Idx < WrappedStrings.Num(); Idx++)
	{
		FLOAT LineXL;
		if (Center)
		{
			INT LineWidth, UnusedY;
			StringSize( Font, LineWidth, UnusedY, *WrappedStrings(Idx).Value );
			LineWidth *= ScaleX;

			FLOAT CenteredX = DrawX + ((RenderParms.DrawXL - LineWidth) * 0.5f);
			LineXL = DrawStringZ(Canvas, CenteredX, DrawY, CurZ, *WrappedStrings(Idx).Value, Font, DrawColor, ScaleX, ScaleY, 0.0f, NULL, SE_BLEND_Translucent, Draw, 0.f, 1.f, RenderInfo);
		}
		else
		{
			LineXL = DrawStringZ(Canvas, DrawX, DrawY, CurZ, *WrappedStrings(Idx).Value, Font, DrawColor, ScaleX, ScaleY, 0.0f, NULL, SE_BLEND_Translucent, Draw, 0.f, 1.f, RenderInfo);
		}
		XL = Max<FLOAT>(XL,LineXL);
		DrawY += Font->GetMaxCharHeight() * ScaleY;
		YL += Font->GetMaxCharHeight() * ScaleY;
	}

	out_XL = appTrunc(XL);
	out_YL = appTrunc(YL);
	return WrappedStrings.Num();
}


/*-----------------------------------------------------------------------------
	UCanvas natives.
-----------------------------------------------------------------------------*/


void UCanvas::execDrawTile( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture,Tex);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_GET_STRUCT_OPTX(FLinearColor, TileDrawColor, DrawColor);
	P_GET_UBOOL_OPTX(bClipTile, FALSE);
	P_GET_BYTE_OPTX(BlendMode,BLEND_Translucent)
	P_FINISH;

	if( !Tex )
	{
		return;
	}

	if ( bClipTile )
	{
		if( XL > 0 && YL > 0 )
		{		
			if( CurX<0 )
			{FLOAT C=CurX*UL/XL; U-=C; UL+=C; XL+=CurX; CurX=0;}
			if( CurY<0 )
			{FLOAT C=CurY*VL/YL; V-=C; VL+=C; YL+=CurY; CurY=0;}
			if( XL>ClipX-CurX )
			{UL+=(ClipX-CurX-XL)*UL/XL; XL=ClipX-CurX;}
			if( YL>ClipY-CurY )
			{VL+=(ClipY-CurY-YL)*VL/YL; YL=ClipY-CurY;}
		}
	}

	DrawTile
	(
		Tex,
		OrgX+CurX,
		OrgY+CurY,
        CurZ,
		XL,
		YL,
		U,
		V,
		UL,
		VL,
		TileDrawColor,
		(EBlendMode)BlendMode
	);
	CurX += XL;
	CurYL = Max(CurYL,YL);
}

/**
 * Optimization call to pre-allocate vertices and triangles for future DrawTile() calls.
 *		NOTE: Num is number of subsequent DrawTile() calls that will be made in a row with the 
 *			same Texture and Blend settings. If other draws (Text, different textures, etc) are
 *			done before the Num DrawTile calls, the optimization will not work and will only waste memory.
 *
 * @param	Num - The number of DrawTile calls that will follow this function call
 * @param	Tex - The texture that will be used to render tiles.
 * @param	Blend - The blend mode that will be used for tiles.
 */
void UCanvas::execPreOptimizeDrawTiles( FFrame& Stack, RESULT_DECL )
{
	P_GET_INT(Num);
	P_GET_OBJECT(UTexture,Tex);
	P_GET_BYTE_OPTX(BlendMode,BLEND_Translucent)
	P_FINISH;

	// get the texture resource that will be renderd
	const FTexture* FinalTexture = ((Tex != NULL) && Tex->Resource) ? Tex->Resource: GWhiteTexture;
	// set up other params for GetBatchedElements
	FBatchedElementParameters* BatchedElementParameters = NULL;
	ESimpleElementBlendMode SimpleBlendMode = BlendToSimpleElementBlend((EBlendMode)BlendMode);

	// get the batched elements object that matches the parameters
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, SimpleBlendMode);

	// reserve space for the next set of DrawTiles that will be called with this texture and blend mode
	BatchedElements->AddReserveVertices(Num * 4);
	BatchedElements->AddReserveTriangles(Num * 2, FinalTexture, SimpleBlendMode);
}

void UCanvas::execDrawMaterialTile( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UMaterialInterface,Material);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT_OPTX(U,0.f);
	P_GET_FLOAT_OPTX(V,0.f);
	P_GET_FLOAT_OPTX(UL,1.f);
	P_GET_FLOAT_OPTX(VL,1.f);
	P_GET_UBOOL_OPTX(bClipTile, FALSE);
	P_FINISH;

	if(!Material)
	{
		return;
	}

	if ( bClipTile )
	{
		if( CurX<0 )
		{FLOAT C=CurX*UL/XL; U-=C; UL+=C; XL+=CurX; CurX=0;}
		if( CurY<0 )
		{FLOAT C=CurY*VL/YL; V-=C; VL+=C; YL+=CurY; CurY=0;}
		if( XL>ClipX-CurX )
		{UL+=(ClipX-CurX-XL)*UL/XL; XL=ClipX-CurX;}
		if( YL>ClipY-CurY )
		{VL+=(ClipY-CurY-YL)*VL/YL; YL=ClipY-CurY;}
	}

	DrawMaterialTile
	(
		Material,
		OrgX+CurX,
		OrgY+CurY,
        CurZ,
		XL,
		YL,
		U,
		V,
		UL,
		VL
	);
	CurX += XL;
	CurYL = Max(CurYL,YL);
}

void UCanvas::DrawText(const FString &Text)
{
	INT XL = 0;
	INT YL = 0;
	WrappedPrint(1, XL, YL, Font, 1.f, 1.f, bCenter, *Text); 
}

void UCanvas::execDrawText( FFrame& Stack, RESULT_DECL )
{
	P_GET_STR(InText);
	P_GET_UBOOL_OPTX(CR,1);
	P_GET_FLOAT_OPTX(XScale,1.0f);
	P_GET_FLOAT_OPTX(YScale,1.0f);
	P_GET_STRUCT_OPTX_REF(FFontRenderInfo,RenderInfo,FFontRenderInfo(EC_EventParm));
	P_FINISH;

	if( !Font )
	{
		Stack.Logf( NAME_Warning, TEXT("DrawText: No font") ); 
		return;
	}
	INT		XL		= 0;
	INT		YL		= 0; 
	FLOAT	OldCurX	= CurX;
	FLOAT	OldCurY	= CurY;
	
	// handle scaled down screen percentage
	if ( !GSystemSettings.bUpscaleScreenPercentage && GSystemSettings.ScreenPercentage < 100.0f )
	{
		XScale *= 0.01f*GSystemSettings.ScreenPercentage;
		YScale *= 0.01f*GSystemSettings.ScreenPercentage;
	}

	if (RenderInfo.bClipText)
	{
		DrawStringZ(Canvas, appTrunc(OrgX + CurX), appTrunc(OrgY + CurY), CurZ, *InText, Font, DrawColor, XScale, YScale, 0.0f, NULL, SE_BLEND_Translucent, TRUE, 0.f, 1.f, RenderInfo);
	}
	else
	{
		WrappedPrint(1, XL, YL, Font, XScale, YScale, bCenter, *InText, RenderInfo); 
	}

	// if expecting a carriage return then reset x, and move y down the area draw
	if (CR)
	{
		CurX	= OldCurX;
		CurY	= OldCurY + YL;
	}
	else
	{
		// otherwise just move x to the end of the draw text
		CurX += XL;
	}
}

void UCanvas::execStrLen( FFrame& Stack, RESULT_DECL ) // wrapped 
{
	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_FINISH;

	if (Font == NULL)
	{
		Stack.Logf(NAME_ScriptWarning, TEXT("No Font"));
	}
	else
	{
		INT XLi = 0, YLi = 0;

		FLOAT OldCurX = CurX;
		FLOAT OldCurY = CurY;
		CurX = 0.f;
		CurY = 0.f;
		FLOAT OldOrgX = OrgX;
		FLOAT OldOrgY = OrgY;
		OrgX = 0.f;
		OrgY = 0.f;

		FLOAT ScaleX = 1.f;
		FLOAT ScaleY = 1.f;

		// handle scaled down screen percentage
		if ( !GSystemSettings.bUpscaleScreenPercentage && GSystemSettings.ScreenPercentage < 100.0f )
		{
			ScaleX = 0.01f*GSystemSettings.ScreenPercentage;
			ScaleY = 0.01f*GSystemSettings.ScreenPercentage;
		}

		WrappedStrLenf( Font, ScaleX, ScaleY, XLi, YLi, TEXT("%s"), *InText );

		CurY = OldCurY;
		CurX = OldCurX;
		OrgY = OldOrgY;
		OrgX = OldOrgX;
		XL = XLi;
		YL = YLi;
	}
}

void UCanvas::execTextSize( FFrame& Stack, RESULT_DECL ) // clipped
{
	P_GET_STR(InText);
	P_GET_FLOAT_REF(XL);
	P_GET_FLOAT_REF(YL);
	P_GET_FLOAT_OPTX(XScale,1.0f);
	P_GET_FLOAT_OPTX(YScale,1.0f);
	P_FINISH;

	INT XLi, YLi;

	if( !Font )
	{
		Stack.Logf( TEXT("TextSize: No font") ); 
		return;
	}

	FLOAT ScaleX = XScale;
	FLOAT ScaleY = YScale;

	// handle scaled down screen percentage
	if ( !GSystemSettings.bUpscaleScreenPercentage && GSystemSettings.ScreenPercentage < 100.0f )
	{
		ScaleX = 0.01f*GSystemSettings.ScreenPercentage;
		ScaleY = 0.01f*GSystemSettings.ScreenPercentage;
	}

	ClippedStrLen( Font, ScaleX, ScaleY, XLi, YLi, *InText );

	XL = XLi;
	YL = YLi;

}

void UCanvas::execProject( FFrame& Stack, RESULT_DECL )
{
	P_GET_VECTOR(Location);
	P_FINISH;

	FPlane V(0,0,0,0);

	if (SceneView!=NULL)
		V = SceneView->Project(Location);

	FVector resultVec(V);
	resultVec.X = (ClipX/2.f) + (resultVec.X*(ClipX/2.f));
	resultVec.Y *= -1.f;
	resultVec.Y = (ClipY/2.f) + (resultVec.Y*(ClipY/2.f));

	// if behind the screen, clamp depth to the screen
	if (V.W <= 0.0f)
	{
		resultVec.Z = 0.0f;
	}
	*(FVector*)Result =	resultVec;

}

void UCanvas::execDeProject( FFrame& Stack, RESULT_DECL )
{
	P_GET_STRUCT(FVector2D, ScreenPos);
	P_GET_VECTOR_REF(WorldOrigin);
	P_GET_VECTOR_REF(WorldDirection);
	P_FINISH;

	if (SceneView != NULL)
	{
		SceneView->DeprojectFVector2D(ScreenPos, WorldOrigin, WorldDirection);
	}
}

/** 
 * Pushes a translation matrix onto the canvas. 
 *
 * @param TranslationVector		Translation vector to use to create the translation matrix.
 */
void UCanvas::execPushTranslationMatrix(FFrame& Stack, RESULT_DECL)
{
	P_GET_VECTOR(TranslationVector);
	P_FINISH;

	if(Canvas != NULL)
	{
		Canvas->PushRelativeTransform(FTranslationMatrix(TranslationVector));
	}
}

/** Pops the topmost matrix from the canvas transform stack. */
void UCanvas::execPopTransform(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	if(Canvas != NULL)
	{
		Canvas->PopTransform();
	}
}

void UCanvas::execPushMaskRegion(FFrame& Stack, RESULT_DECL)
{
	P_GET_FLOAT(X);
	P_GET_FLOAT(Y);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_FINISH;

	if(Canvas != NULL)
	{
		Canvas->PushMaskRegion(X,Y,XL,YL);
	}
}

void UCanvas::execPopMaskRegion(FFrame& Stack, RESULT_DECL)
{
	P_FINISH;

	if(Canvas != NULL)
	{
		Canvas->PopMaskRegion();
	}
}

void UCanvas::execDrawTileStretched( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture,Tex);
	P_GET_FLOAT(AWidth);
	P_GET_FLOAT(AHeight);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_GET_STRUCT_OPTX(FLinearColor, TileDrawColor, DrawColor);
	P_GET_UBOOL_OPTX(bStretchHorizontally, TRUE);
	P_GET_UBOOL_OPTX(bStretchVertically, TRUE);
	P_GET_FLOAT_OPTX(ScalingFactor, 1.0);
	P_FINISH;

	DrawTileStretched(Tex, CurX, CurY, CurZ, AWidth, AHeight, U, V, UL, VL, TileDrawColor, bStretchHorizontally, bStretchVertically, ScalingFactor);

}

void UCanvas::DrawTileStretched(UTexture* Tex, FLOAT Left, FLOAT Top, FLOAT Depth, FLOAT AWidth, FLOAT AHeight, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FLinearColor DrawColor, UBOOL bStretchHorizontally,UBOOL bStretchVertically, FLOAT ScalingFactor)
{
	// Offset for the origin.
	Left += OrgX;
	Top += OrgY;

	// Compute the fraction of the tile which the texture edges cover without being stretched.
	const FLOAT EdgeFractionX = (Abs(AWidth) < DELTA || !bStretchHorizontally) ? 1.0f : Min(1.0f,Abs(UL * ScalingFactor / AWidth));
	const FLOAT EdgeFractionY = (Abs(AHeight) < DELTA || !bStretchVertically) ? 1.0f :  Min(1.0f,Abs(VL * ScalingFactor / AHeight));

	// Compute the dimensions of each row and column.
	const FLOAT EdgeSizeX = AWidth * EdgeFractionX * 0.5f;
	const FLOAT EdgeSizeY = AHeight * EdgeFractionY * 0.5f;
	const FLOAT EdgeSizeU = UL * 0.5f;
	const FLOAT EdgeSizeV = VL * 0.5f;

	const FLOAT ColumnSizeX[3] = {	EdgeSizeX,	AWidth - EdgeSizeX * 2,		EdgeSizeX	};
	const FLOAT ColumnSizeU[3] = {	EdgeSizeU,	0.0f,						EdgeSizeU	};

	const FLOAT RowSizeY[3] = {		EdgeSizeY,	AHeight - EdgeSizeY * 2,	EdgeSizeY	};
	const FLOAT RowSizeV[3] = {		EdgeSizeV,	0.0f,						EdgeSizeV	};

	// Draw each row, starting from the top.
	FLOAT RowY = Top;
	FLOAT RowV = V;
	for(INT RowIndex = 0;RowIndex < 3;RowIndex++)
	{
		// Draw each column in the row, starting from the left.
		FLOAT ColumnX = Left;
		FLOAT ColumnU = U;
		for(INT ColumnIndex = 0;ColumnIndex < 3;ColumnIndex++)
		{
			// The tile may not be stretched on all axes, so some rows or columns will have zero size, and don't need to be rendered.
			if(ColumnSizeX[ColumnIndex] > 0.0f && RowSizeY[RowIndex] > 0.0f)
			{
				DrawTile(Tex,ColumnX,RowY,Depth,ColumnSizeX[ColumnIndex],RowSizeY[RowIndex],ColumnU,RowV,ColumnSizeU[ColumnIndex],RowSizeV[RowIndex],DrawColor);

				ColumnX += ColumnSizeX[ColumnIndex];
				ColumnU += ColumnSizeU[ColumnIndex];
			}
		}

		RowY += RowSizeY[RowIndex];
		RowV += RowSizeV[RowIndex];
	}
}

void UCanvas::DrawRotatedTile(UTexture* Tex, FRotator Rotation, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FLOAT AnchorX, FLOAT AnchorY)
{
	if(!Tex)
	{
		return;
	}
	// Figure out where we are drawing
	FVector Position( OrgX + CurX, OrgY + CurY, 0 );

	// Anchor is the center of the tile
	FVector AnchorPos( XL * AnchorX, YL * AnchorY, 0 );

	FRotationMatrix RotMatrix( Rotation );
	FMatrix TransformMatrix = FTranslationMatrix(-AnchorPos) * RotMatrix * FTranslationMatrix(AnchorPos);

	// translate the matrix back to origin, apply the rotation matrix, then transform back to the current position
 	FMatrix FinalTransform = FTranslationMatrix(-Position) * TransformMatrix * FTranslationMatrix(Position);

	Canvas->PushRelativeTransform(FinalTransform);

	DrawTile(Tex,OrgX+CurX,OrgY+CurY,CurZ,XL,YL,U,V,UL,VL,DrawColor);

	Canvas->PopTransform();
}

void UCanvas::DrawRotatedMaterialTile(UMaterialInterface* Tex, FRotator Rotation, FLOAT XL, FLOAT YL, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL, FLOAT AnchorX, FLOAT AnchorY)
{
	if(!Tex)
	{
		return;
	}

	UMaterial* Mat = Tex->GetMaterial();

	if (UL <= 0.0f)
	{
		UL = 1.0f;
	}
	if (VL <= 0.0f) 
	{
		VL = 1.0f;
	}

	FLOAT Z = CurZ;
	// due to rotation transform, the calculated stereoscopic disparity in DrawTileZ will be rotated, hurting the eyeballs,
	// so in that case, we do it out here, before the rotation
	FLOAT S3DDisparity = GetStereoscopicDisparity(Z);
	if (S3DDisparity > 0.0f)
	{
		// this will disable stereoscopic in the GetStereoscopicDisparity inside DrawTileZ 
		Z = 0.1f;
	}

	// Figure out where we are drawing
	FVector Position( OrgX + CurX + S3DDisparity, OrgY + CurY, Z );

	// Anchor is the center of the tile
	FVector AnchorPos( XL * AnchorX, YL * AnchorY, 0 );

	FRotationMatrix RotMatrix( Rotation );
	FMatrix TransformMatrix;
	TransformMatrix = FTranslationMatrix(-AnchorPos) * RotMatrix * FTranslationMatrix(AnchorPos);

	// translate the matrix back to origin, apply the rotation matrix, then transform back to the current position
 	FMatrix FinalTransform = TransformMatrix * FTranslationMatrix(Position);

	Canvas->PushRelativeTransform(FinalTransform);
	// draw at 0, the final transform already will transform it to the right spot
	DrawMaterialTile(Tex, 0, 0, 0, XL,YL,U,V,UL,VL);
	Canvas->PopTransform();
}


//
// Draw a texture as a timer/radar/Pizza.  Typically the texture is a circle.
// You choose starting position, and how much of cirlce to draw.  0 - 12:00, 0.25 = 3:00
//
void UCanvas::DrawTimer
(
 UTexture*			Tex,
 FLOAT				StartTime, 
 FLOAT				TotalTime, 
 FLOAT				X,
 FLOAT				Y,
 FLOAT				Z,
 FLOAT				XL,
 FLOAT				YL,
 FLOAT				U,
 FLOAT				V,
 FLOAT				UL,
 FLOAT				VL,
 const FLinearColor&	Color,
 EBlendMode BlendMode
 )
{
	if ( !Canvas || !Tex ) 
		return;
	FLOAT MyClipX = OrgX + ClipX;
	FLOAT MyClipY = OrgY + ClipY;
	FLOAT w = X + XL > MyClipX ? MyClipX - X : XL;
	FLOAT h = Y + YL > MyClipY ? MyClipY - Y : YL;
	if (XL > 0.f &&
		YL > 0.f)
	{
		// here we use the original size of the texture, not the current size (for instance, 
		// PVRTC textures, on some platforms anyway, need to be square), but the script code
		// was written using 0..TexSize coordinates, not 0..1, so to make the 0..1 coords
		// we divide by what the texture was when the script code was written
		FLOAT SizeX = Tex->GetOriginalSurfaceWidth();
		FLOAT SizeY = Tex->GetOriginalSurfaceHeight();

		::DrawTimerZ(
			Canvas,
			StartTime,
			TotalTime,
			(X),
			(Y),
			(Z),
			(w),
			(h),
			U/SizeX,
			V/SizeY,
			UL/SizeX * w/XL,
			VL/SizeY * h/YL,
			Color,
			Tex->Resource,
			BlendToSimpleElementBlend((EBlendMode)BlendMode));
	}
}

void UCanvas::execDrawTimer( FFrame& Stack, RESULT_DECL )
{
	P_GET_OBJECT(UTexture,Tex);
	P_GET_FLOAT(StartTime);
	P_GET_FLOAT(TotalTime);
	P_GET_FLOAT(XL);
	P_GET_FLOAT(YL);
	P_GET_FLOAT(U);
	P_GET_FLOAT(V);
	P_GET_FLOAT(UL);
	P_GET_FLOAT(VL);
	P_GET_STRUCT_OPTX(FLinearColor, TileDrawColor, DrawColor);
	P_GET_BYTE_OPTX(BlendMode,BLEND_Translucent)
	P_FINISH;

	if( !Tex )
		return;

	DrawTimer
	(
		Tex,
		StartTime,
		TotalTime,
		OrgX+CurX,
		OrgY+CurY,
		CurZ,
		XL,
		YL,
		U,
		V,
		UL,
		VL,
		TileDrawColor,
		(EBlendMode)BlendMode
	);

	CurX += XL;
	CurYL = Max(CurYL,YL);
}

void UCanvas::execDraw2DLine(FFrame& Stack, RESULT_DECL )
{
	P_GET_FLOAT(X1);
	P_GET_FLOAT(Y1);
	P_GET_FLOAT(X2);
	P_GET_FLOAT(Y2);
	P_GET_STRUCT(FColor,LineColor);
	P_FINISH;

	X1+= OrgX;
	X2+= OrgX;
	Y1+= OrgY;
	Y2+= OrgY;

	DrawLine2D(Canvas, FVector2D(X1, Y1), FVector2D(X2, Y2), LineColor);
}

void UCanvas::DrawTextureLine(FVector StartPoint, FVector EndPoint, FLOAT Perc, FLOAT Width, FColor LineColor, UTexture* Tex, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL)
{
	if (!Tex)
	{
		Tex = DefaultTexture;
	}

	FRotator R(0,0,0);
	FLOAT Dist;
	FVector Dir;

	Dir = (EndPoint - StartPoint).SafeNormal();

	DrawColor = LineColor;

	R.Yaw = (StartPoint - EndPoint).SafeNormal().Rotation().Yaw;
	Dist = (StartPoint - EndPoint).Size2D();
	Dir *= Dist * 0.5;

	Dist -= Perc;

	CurX = StartPoint.X + Dir.X - (Dist *0.5);
	CurY = StartPoint.Y + Dir.Y - 1;

	DrawRotatedTile(Tex, R, Dist, Width, U, V, UL, VL, 0.5, 0.5);
}

void UCanvas::DrawTextureDoubleLine(FVector StartPoint, FVector EndPoint, FLOAT Perc, FLOAT Spacing, FLOAT Width, FColor LineColor, FColor AltLineColor, UTexture *Tex, FLOAT U, FLOAT V, FLOAT UL, FLOAT VL)
{
	if (!Tex)
	{
		Tex = DefaultTexture;
	}

	FRotator R(0,0,0);
	FLOAT Dist;
	FVector Dir,Center, Ofst;

	Dir = (EndPoint - StartPoint).SafeNormal();
	R.Yaw = (StartPoint - EndPoint).SafeNormal().Rotation().Yaw;
	Dist = (StartPoint - EndPoint).Size2D();

	Center.X = StartPoint.X + (Dir.X * Dist * 0.5);
	Center.Y = StartPoint.Y + (Dir.Y * Dist * 0.5);

	Dist -= Perc;
	Ofst = Dir * (Spacing + Width);

	CurX = Center.X+(Ofst.Y) - (Dist * 0.5);
	CurY = Center.Y+(Ofst.X * -1) - Width;

	DrawColor = LineColor;
	DrawRotatedTile(Tex, R, Dist, Width, U, V, UL, VL, 0.5, 0.5);

	Ofst = Dir * Spacing;

	CurX = Center.X-(Ofst.Y) - (Dist * 0.5);
	CurY = Center.Y-(Ofst.X * -1) - Width;

	DrawColor = AltLineColor;
	DrawRotatedTile(Tex, R, Dist, Width, U, V, UL, VL, 0.5, 0.5);
}

/** 
 *	Draw a number of triangles on the canvas
 *	@param Tex			Texture to apply to triangles
 *	@param Triangles	Array of triangles to render
 */
void UCanvas::DrawTris(class UTexture* Tex, const TArray<struct FCanvasUVTri>& Triangles, FColor InColor)
{
	const ESimpleElementBlendMode BlendMode = SE_BLEND_Opaque;
	const FTexture* FinalTexture = (Tex && Tex->Resource) ? Tex->Resource : GWhiteTexture;
	FBatchedElementParameters* BatchedElementParameters = NULL;
	FBatchedElements* BatchedElements = Canvas->GetBatchedElements(FCanvas::ET_Triangle, BatchedElementParameters, FinalTexture, BlendMode);
	FColor Color = InColor;

	for(INT i=0; i<Triangles.Num(); i++)
	{
		const FCanvasUVTri& Tri = Triangles(i);

		INT V0 = BatchedElements->AddVertex(FVector4(Tri.V0_Pos.X,Tri.V0_Pos.Y,0,1), Tri.V0_UV, Color, FHitProxyId());
		INT V1 = BatchedElements->AddVertex(FVector4(Tri.V1_Pos.X,Tri.V1_Pos.Y,0,1), Tri.V1_UV, Color, FHitProxyId());
		INT V2 = BatchedElements->AddVertex(FVector4(Tri.V2_Pos.X,Tri.V2_Pos.Y,0,1), Tri.V2_UV, Color, FHitProxyId());

		BatchedElements->AddTriangle(V0, V1, V2, FinalTexture, BlendMode);
	}
}
