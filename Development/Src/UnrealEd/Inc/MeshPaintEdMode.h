/*================================================================================
	MeshPaintEdMode.h: Mesh paint tool
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/


#ifndef __MeshPaintEdMode_h__
#define __MeshPaintEdMode_h__

#ifdef _MSC_VER
	#pragma once
#endif



/** Mesh paint resource types */
namespace EMeshPaintResource
{
	enum Type
	{
		/** Editing vertex colors */
		VertexColors,

		/** Editing textures */
		Texture
	};
}



/** Mesh paint mode */
namespace EMeshPaintMode
{
	enum Type
	{
		/** Painting colors directly */
		PaintColors,

		/** Painting texture blend weights */
		PaintWeights
	};
}



/** Vertex paint target */
namespace EMeshVertexPaintTarget
{
	enum Type
	{
		/** Paint the static mesh component instance in the level */
		ComponentInstance,

		/** Paint the actual static mesh asset */
		Mesh
	};
}




/** Mesh paint color view modes (somewhat maps to EVertexColorViewMode engine enum.) */
namespace EMeshPaintColorViewMode
{
	enum Type
	{
		/** Normal view mode (vertex color visualization off) */
		Normal,

		/** RGB only */
		RGB,
		
		/** Alpha only */
		Alpha,

		/** Red only */
		Red,

		/** Green only */
		Green,

		/** Blue only */
		Blue,
	};
}


/**
 * Mesh Paint settings
 */
class FMeshPaintSettings
{

public:

	/** Static: Returns global mesh paint settings */
	static FMeshPaintSettings& Get()
	{
		return StaticMeshPaintSettings;
	}


protected:

	/** Static: Global mesh paint settings */
	static FMeshPaintSettings StaticMeshPaintSettings;



public:

	/** Constructor */
	FMeshPaintSettings()
		: BrushRadius( 32.0f ),
		  BrushFalloffAmount( 1.0f ),
		  BrushStrength( 0.2f ),
		  bEnableFlow( TRUE ),
		  FlowAmount( 1.0f ),
		  bOnlyFrontFacingTriangles( TRUE ),
		  ResourceType( EMeshPaintResource::VertexColors ),
		  PaintMode( EMeshPaintMode::PaintColors ),
		  PaintColor( 1.0f, 1.0f, 1.0f, 1.0f ),
		  EraseColor( 0.0f, 0.0f, 0.0f, 0.0f ),
		  bWriteRed( TRUE ),
		  bWriteGreen( TRUE ),
		  bWriteBlue( TRUE ),
		  bWriteAlpha( FALSE ),		// NOTE: Don't write alpha by default
		  TotalWeightCount( 2 ),
		  PaintWeightIndex( 0 ),
		  EraseWeightIndex( 1 ),
		  VertexPaintTarget( EMeshVertexPaintTarget::ComponentInstance ),
		  UVChannel( 0 ),
		  ColorViewMode( EMeshPaintColorViewMode::Normal ),
		  WindowPositionX( -1 ),
		  WindowPositionY( -1 ),
		  bEnableSeamPainting( TRUE )
	{
	}


public:

	/** Radius of the brush (world space units) */
	FLOAT BrushRadius;

	/** Amount of falloff to apply (0.0 - 1.0) */
	FLOAT BrushFalloffAmount;

	/** Strength of the brush (0.0 - 1.0) */
	FLOAT BrushStrength;

	/** Enables "Flow" painting where paint is continually applied from the brush every tick */
	UBOOL bEnableFlow;

	/** When "Flow" is enabled, this scale is applied to the brush strength for paint applied every tick (0.0-1.0) */
	FLOAT FlowAmount;

	/** Whether back-facing triangles should be ignored */
	UBOOL bOnlyFrontFacingTriangles;

	/** Resource type we're editing */
	EMeshPaintResource::Type ResourceType;


	/** Mode we're painting in.  If this is set to PaintColors we're painting colors directly; if it's set
	    to PaintWeights we're painting texture blend weights using a single channel. */
	EMeshPaintMode::Type PaintMode;


	/** Colors Mode: Paint color */
	FLinearColor PaintColor;

	/** Colors Mode: Erase color */
	FLinearColor EraseColor;

	/** Colors Mode: True if red colors values should be written */
	UBOOL bWriteRed;

	/** Colors Mode: True if green colors values should be written */
	UBOOL bWriteGreen;

	/** Colors Mode: True if blue colors values should be written */
	UBOOL bWriteBlue;

	/** Colors Mode: True if alpha colors values should be written */
	UBOOL bWriteAlpha;


	/** Weights Mode: Total weight count */
	INT TotalWeightCount;

	/** Weights Mode: Weight index that we're currently painting */
	INT PaintWeightIndex;

	/** Weights Mode: Weight index that we're currently using to erase with */
	INT EraseWeightIndex;


	/**
	 * Vertex paint settings
	 */
	
	/** Vertex paint target */
	EMeshVertexPaintTarget::Type VertexPaintTarget;


	/**
	 * Texture paint settings
	 */
	
	/** UV channel to paint textures using */
	INT UVChannel;


	/**
	 * View settings
	 */

	/** Color visualization mode */
	EMeshPaintColorViewMode::Type ColorViewMode;


	/**
	 * Window settings
	 */

	/** Horizontal window position */
	INT WindowPositionX;

	/** Vertical window position */
	INT WindowPositionY;

	/** Seam painting flag, True if we should enable dilation to allow the painting of texture seams */
	UBOOL bEnableSeamPainting; 

};



/** Mesh painting action (paint, erase) */
namespace EMeshPaintAction
{
	enum Type
	{
		/** Paint (add color or increase blending weight) */
		Paint,

		/** Erase (remove color or decrease blending weight) */
		Erase,

		/** Fill with the active paint color */
		Fill,

		/** Push instance colors to mesh color */
		PushInstanceColorsToMesh
	};
}



namespace MeshPaintDefs
{
	// Design constraints

	// Currently we never support more than five channels (R, G, B, A, OneMinusTotal)
	static const INT MaxSupportedPhysicalWeights = 4;
	static const INT MaxSupportedWeights = MaxSupportedPhysicalWeights + 1;
}


/**
 *  Wrapper to expose texture targets to WPF code.
 */
struct FTextureTargetListInfo
{
	UTexture2D* TextureData;
	UBOOL bIsSelected;
	UINT UndoCount;
	FTextureTargetListInfo(UTexture2D* InTextureData, UBOOL InbIsSelected = FALSE)
		:	TextureData(InTextureData)
		,	bIsSelected(InbIsSelected)
		,	UndoCount(0)
	{}
};


/**
 * Mesh Paint editor mode
 */
class FEdModeMeshPaint
	: public FEdMode
{
private:
	/** struct used to store the color data copied from mesh instance to mesh instance */
	struct FPerLODVertexColorData
	{
		TArray< FColor > ColorsByIndex;
		TMap<FVector, FColor> ColorsByPosition;
	};

private:

#if _WINDOWS
	/** Used to store Window Messages we get from WPF */
	struct FDeferredWindowMessage
	{
		FDeferredWindowMessage( UINT Msg, WPARAM w, LPARAM l )
			: Message( Msg ), wParam( w ), lParam( l )
		{
		}
		UINT Message;
		WPARAM wParam;
		LPARAM lParam;
	};
	
	/** List of pending window messages we have to process, used to store messages we get from the WPF window */
	TArray<FDeferredWindowMessage> WindowMessages;
	
#endif // _WINDOWS

public:

	struct PaintTexture2DData
	{
		/** The original texture that we're painting */
		UTexture2D* PaintingTexture2D;
		UBOOL bIsPaintingTexture2DModified;

		/** A copy of the original texture we're painting, used for restoration. */
		UTexture2D* PaintingTexture2DDuplicate;

		/** Render target texture for painting */
		UTextureRenderTarget2D* PaintRenderTargetTexture;

		/** List of materials we are painting on */
		TArray< UMaterialInterface* > PaintingMaterials;

		/** Default ctor */
		PaintTexture2DData() :
			PaintingTexture2D( NULL ),
			PaintingTexture2DDuplicate ( NULL ),
			bIsPaintingTexture2DModified( FALSE ),
			PaintRenderTargetTexture( NULL )
		{}

		PaintTexture2DData( UTexture2D* InPaintingTexture2D, UBOOL InbIsPaintingTexture2DModified = FALSE ) :
			PaintingTexture2D( InPaintingTexture2D ),
			bIsPaintingTexture2DModified( InbIsPaintingTexture2DModified ),
			PaintRenderTargetTexture( NULL )
		{}

		/** Serializer */
		friend FArchive& operator<<( FArchive& Ar, PaintTexture2DData& Asset )
		{
			// @todo MeshPaint: We're relying on GC to clean up render targets, can we free up remote memory more quickly?
			return Ar << Asset.PaintingTexture2D << Asset.PaintRenderTargetTexture << Asset.PaintingMaterials;
		}
	};

	struct FTexturePaintTriangleInfo
	{
		FVector TriVertices[ 3 ];
		FVector2D TrianglePoints[ 3 ];
		FVector2D TriUVs[ 3 ];
	};

	/** Constructor */
	FEdModeMeshPaint();

	/** Destructor */
	virtual ~FEdModeMeshPaint();

#if _WINDOWS
	/** Adds window messages that we will process once per frame. */
	void AddWindowMessage( UINT Msg, WPARAM wParam, LPARAM lParam )
	{
		WindowMessages.AddItem( FDeferredWindowMessage( Msg, wParam, lParam ) );
	}
#endif // _WINDOWS

	/** FSerializableObject: Serializer */
	virtual void Serialize( FArchive &Ar );

	/** FEdMode: Called when the mode is entered */
	virtual void Enter();

	/** FEdMode: Called when the mode is exited */
	virtual void Exit();

	/** FEdMode: Called when the mouse is moved over the viewport */
	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y );

	/**
	 * FEdMode: Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	TRUE if input was handled
	 */
	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY );

	/** FEdMode: Called when a mouse button is pressed */
	virtual UBOOL StartTracking();

	/** FEdMode: Called when a mouse button is released */
	virtual UBOOL EndTracking();

	/** FEdMode: Called when a key is pressed */
	virtual UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FName InKey, EInputEvent InEvent );

	/** FEdMode: Called when mouse drag input it applied */
	virtual UBOOL InputDelta( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale );

	/** FEdMode: Called after an Undo operation */
	virtual void PostUndo();

	/** Returns TRUE if we need to force a render/update through based fill/copy */
	UBOOL IsForceRendered (void) const;

	/** FEdMode: Render the mesh paint tool */
	virtual void Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

	/** FEdMode: Render HUD elements for this tool */
	virtual void DrawHUD( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas );

	/** FEdMode: Handling SelectActor */
	virtual UBOOL Select( AActor* InActor, UBOOL bInSelected );

	/** FEdMode: Called when the currently selected actor has changed */
	virtual void ActorSelectionChangeNotify();

	/** FEdMode: If the EdMode is handling InputDelta (ie returning true from it), this allows a mode to indicated whether or not the Widget should also move. */
	virtual UBOOL AllowWidgetMove() { return FALSE; }

	/** FEdMode: Draw the transform widget while in this mode? */
	virtual UBOOL ShouldDrawWidget() const { return FALSE; }

	/** FEdMode: Returns true if this mode uses the transform widget */
	virtual UBOOL UsesTransformWidget() const { return FALSE; }

	/** Saves out cached mesh settings for the given actor */
	void SaveSettingsForActor( AActor* InActor );
	void SaveSettingsForStaticMeshComponent( UStaticMeshComponent* InStaticMeshComponent );
	void UpdateSettingsForStaticMeshComponent( UStaticMeshComponent* InStaticMeshComponent, UTexture2D* InOldTexture, UTexture2D* InNewTexture );

	/** Helper function to get the current paint action for use in DoPaint */
	EMeshPaintAction::Type GetPaintAction(FViewport* InViewport);

	/** Removes vertex colors associated with the object (if it's a StaticMeshActor or DynamicSMActor) */
	void RemoveInstanceVertexColors(UObject* Obj) const;

	/** Removes vertex colors associated with the currently selected mesh */
	void RemoveInstanceVertexColors() const;

	/** Copies vertex colors associated with the currently selected mesh */
	void CopyInstanceVertexColors();

	/** Pastes vertex colors to the currently selected mesh */
	void PasteInstanceVertexColors();

	/** Returns whether the instance vertex colors associated with the currently selected mesh need to be fixed up or not */
	UBOOL RequiresInstanceVertexColorsFixup() const;

	/** Attempts to fix up the instance vertex colors associated with the currently selected mesh, if necessary */
	void FixupInstanceVertexColors() const;

	/** Fills the vertex colors associated with the currently selected mesh*/
	void FillInstanceVertexColors();

	/** Pushes instance vertex colors to the  mesh*/
	void PushInstanceVertexColorsToMesh();

	/** Creates a paintable material/texture for the selected mesh */
	void CreateInstanceMaterialAndTexture() const;

	/** Removes instance of paintable material/texture for the selected mesh */
	void RemoveInstanceMaterialAndTexture() const;

	/** Returns information about the currently selected mesh */
	UBOOL GetSelectedMeshInfo( INT& OutTotalBaseVertexColorBytes, INT& OutTotalInstanceVertexColorBytes, UBOOL& bOutHasInstanceMaterialAndTexture ) const;

	/** Returns whether there are colors in the copy buffer */
	UBOOL CanPasteVertexColors() const
	{ 
		for (INT i=0; i<CopiedColorsByLOD.Num(); i++)
		{
			if (0 < CopiedColorsByLOD(i).ColorsByIndex.Num())
			{
				return TRUE;
			}
		}
		return FALSE; 
	}

	/** 
	 * Will update the list of available texture paint targets based on selection 
	 */
	void UpdateTexturePaintTargetList();

	/** Will update the list of available texture paint targets based on selection */
	INT GetMaxNumUVSets() const;

	/** Will return the list of available texture paint targets */
	TArray<FTextureTargetListInfo>* GetTexturePaintTargetList();

	/** Will return the selected target paint texture if there is one. */
	UTexture2D* GetSelectedTexture();

	/** will find the currently selected paint target texture in the content browser */
	void FindSelectedTextureInContentBrowser();

	/** 
	 * Used to commit all paint changes to corresponding target textures. 
	 * @param	bShouldTriggerUIRefresh		Flag to trigger a UI Refresh if any textures have been modified (defaults to TRUE)
	 */	
	void CommitAllPaintedTextures(UBOOL bShouldTriggerUIRefresh=TRUE);

	/** Clears all texture overrides for this static mesh. */
	void ClearStaticMeshTextureOverrides(UStaticMeshComponent* InStaticMeshComponent);

	/** Clears all texture overrides, removing any pending texture paint changes */
	void ClearAllTextureOverrides();

	/** Sets all texture overrides available for the mesh. */
	void SetAllTextureOverrides(UStaticMeshComponent* InStaticMeshComponent);

	/** Sets the override for a specific texture for any materials using it in the mesh, clears the override if it has no overrides. */
	void SetSpecificTextureOverrideForMesh(UStaticMeshComponent* InStaticMeshComponent, UTexture* Texture);

	/** Used to tell the texture paint system that we will need to restore the rendertargets */
	void RestoreRenderTargets();

	/** Returns the number of texture that require a commit. */
	INT GetNumberOfPendingPaintChanges();

	typedef UStaticMesh::kDOPTreeType kDOPTreeType;

	/** Returns index of the currently selected Texture Target */
	INT FEdModeMeshPaint::GetCurrentTextureTargetIndex() const;

	/** Duplicates the currently selected texture and material, relinking them together and assigning them to the selected meshes. */
	void DuplicateTextureMaterialCombo();

	/** Will create a new texture. */
	void CreateNewTexture();
private:

	/** Struct to hold MeshPaint settings on a per mesh basis */
	struct StaticMeshSettings
	{
		UTexture2D* SelectedTexture;
		INT SelectedUVChannel;

		StaticMeshSettings( )
			:	SelectedTexture(NULL)
			,	SelectedUVChannel(0)
		{}
		StaticMeshSettings( UTexture2D* InSelectedTexture, INT InSelectedUVSet )
			:	SelectedTexture(InSelectedTexture)
			,	SelectedUVChannel(InSelectedUVSet)
		{}

		void operator=(const StaticMeshSettings& SrcSettings)
		{
			SelectedTexture = SrcSettings.SelectedTexture;
			SelectedUVChannel = SrcSettings.SelectedUVChannel;
		}
	};

	/** Static: Determines if a world space point is influenced by the brush and reports metrics if so */
	static UBOOL IsPointInfluencedByBrush( const FVector& InPosition,
										   const class FMeshPaintParameters& InParams,
										   FLOAT& OutSquaredDistanceToVertex2D,
										   FLOAT& OutVertexDepthToBrush );

	/** Static: Paints the specified vertex!  Returns TRUE if the vertex was in range. */
	static UBOOL PaintVertex( const FVector& InVertexPosition,
							  const class FMeshPaintParameters& InParams,
							  const UBOOL bIsPainting,
							  FColor& InOutVertexColor );

	/** Paint the mesh that impacts the specified ray */
	void DoPaint( const FVector& InCameraOrigin,
				  const FVector& InRayOrigin,
				  const FVector& InRayDirection,
				  FPrimitiveDrawInterface* PDI,
				  const EMeshPaintAction::Type InPaintAction,
				  const UBOOL bVisualCueOnly,
				  const FLOAT InStrengthScale,
				  OUT UBOOL& bAnyPaintAbleActorsUnderCursor);

	/** Paints mesh vertices */
	void PaintMeshVertices( UStaticMeshComponent* StaticMeshComponent, const FMeshPaintParameters& Params, const UBOOL bShouldApplyPaint, FStaticMeshRenderData& LODModel, const FVector& ActorSpaceCameraPosition, const FMatrix& ActorToWorldMatrix, FPrimitiveDrawInterface* PDI, const FLOAT VisualBiasDistance );

	/** Paints mesh texture */
	void PaintMeshTexture( UStaticMeshComponent* StaticMeshComponent, const FMeshPaintParameters& Params, const UBOOL bShouldApplyPaint, FStaticMeshRenderData& LODModel, const FVector& ActorSpaceCameraPosition, const FMatrix& ActorToWorldMatrix, const FLOAT ActorSpaceSquaredBrushRadius, const FVector& ActorSpaceBrushPosition );
	
	/** Forces real-time perspective viewports */
	void ForceRealTimeViewports( const UBOOL bEnable, const UBOOL bStoreCurrentState );

	/** Sets show flags for perspective viewports */
	void SetViewportShowFlags( const UBOOL bAllowColorViewModes );

	/** Starts painting a texture */
	void StartPaintingTexture( UStaticMeshComponent* InStaticMeshComponent );

	/** Paints on a texture */
	void PaintTexture( const FMeshPaintParameters& InParams,
					   const TArray< INT >& InInfluencedTriangles,
					   const FMatrix& InActorToWorldMatrix );

	/** Finishes painting a texture */
	void FinishPaintingTexture();

	/** Makes sure that the render target is ready to paint on */
	void SetupInitialRenderTargetData( UTexture2D* InTextureSource, UTextureRenderTarget2D* InRenderTarget );

	/** Static: Copies a texture to a render target texture */
	static void CopyTextureToRenderTargetTexture( UTexture* SourceTexture, UTextureRenderTarget2D* RenderTargetTexture );

	/** Will generate a mask texture, used for texture dilation, and store it in the passed in rendertarget */
	UBOOL GenerateSeamMask(UStaticMeshComponent* StaticMeshComponent, INT UVSet, UTextureRenderTarget2D* RenderTargetTexture);

	/** Static: Creates a temporary texture used to transfer data to a render target in memory */
	UTexture2D* CreateTempUncompressedTexture( UTexture2D* SourceTexture );

	/** Used when we want to change selection to the next available paint target texture.  Will stop at the end of the list and will NOT cycle to the beginning of the list. */
	void SelectNextTexture() { ShiftSelectedTexture( TRUE, FALSE ); }

	/** Used to change the selected texture paint target texture. Will stop at the beginning of the list and will NOT cycle to the end of the list. */
	void SelectPrevTexture() { ShiftSelectedTexture( FALSE, FALSE); }

	/**
	 * Used to change the currently selected paint target texture.
	 *
	 * @param	bToTheRight 	True if a shift to next texture is desired, false if a shift to the previous texture is desired.
	 * @param	bCycle		 	If set to False, this function will stop at the first or final element.  If TRUE the selection will cycle to the opposite end of the list.
	 */
	void ShiftSelectedTexture( UBOOL bToTheRight = TRUE, UBOOL bCycle = TRUE );

	
	/**
	 * Used to get a reference to data entry associated with the texture.  Will create a new entry if one is not found.
	 *
	 * @param	inTexture 		The texture we want to retrieve data for.
	 * @return					Returns a reference to the paint data associated with the texture.  This reference
	 *								is only valid until the next change to any key in the map.  Will return NULL only when inTexture is NULL.
	 */
	PaintTexture2DData* GetPaintTargetData(  UTexture2D* inTexture );

	/**
	 * Used to add an entry to to our paint target data.
	 *
	 * @param	inTexture 		The texture we want to create data for.
	 * @return					Returns a reference to the newly created entry.  If an entry for the input texture already exists it will be returned instead.
	 *								Will return NULL only when inTexture is NULL.   This reference is only valid until the next change to any key in the map.
	 *								 
	 */
	PaintTexture2DData* AddPaintTargetData(  UTexture2D* inTexture );

	/**
	 * Used to get the original texture that was overridden with a render target texture.
	 *
	 * @param	inTexture 		The render target that was used to override the original texture.
	 * @return					Returns a reference to texture that was overridden with the input render target texture.  Returns NULL if we don't find anything.
	 *								 
	 */
	UTexture2D* GetOriginalTextureFromRenderTarget( UTextureRenderTarget2D* inTexture );


	/** FEdMode: Called once per frame */
	virtual void Tick( FEditorLevelViewportClient* ViewportClient, FLOAT DeltaTime );

private:

	/** Whether we're currently painting */
	UBOOL bIsPainting;

	/** Whether we are doing a flood fill the next render */
	UBOOL bIsFloodFill;

	/** Whether we are pushing the instance colors to the mesh next render */
	UBOOL bPushInstanceColorsToMesh;

	/** Will store the state of selection locks on start of paint mode so that it can be restored on close */
	UBOOL bWasSelectionLockedOnStart;

	/** True when the actor selection has changed since we last updated the TexturePaint list, used to avoid reloading the same textures */
	UBOOL bShouldUpdateTextureList;


	/** True if we need to generate a texture seam mask used for texture dilation */
	UBOOL bGenerateSeamMask;

	/** Real time that we started painting */
	DOUBLE PaintingStartTime;

	/** Array of static meshes that we've modified */
	TArray< UStaticMesh* > ModifiedStaticMeshes;

	/** A mapping of static meshes to temp kDOP trees that were built for static meshes without collision */
	TMap<UStaticMesh*, kDOPTreeType> StaticMeshToTempkDOPMap;

#if WITH_MANAGED_CODE
	/** Mesh Paint tool palette window */
	TScopedPointer< class FMeshPaintWindow > MeshPaintWindow;
#endif

	/** Which mesh LOD index we're painting */
	// @todo MeshPaint: Allow painting on other LODs?
	static const UINT PaintingMeshLODIndex = 0;

	/** Texture paint: The static mesh components that we're currently painting */
	UStaticMeshComponent* TexturePaintingStaticMeshComponent;

	/** The original texture that we're painting */
	UTexture2D* PaintingTexture2D;

	/** Stores data associated with our paint target textures */
	TMap< UTexture2D*, PaintTexture2DData > PaintTargetData;

	/** Temporary buffers used when copying/pasting colors */
	TArray< FPerLODVertexColorData > CopiedColorsByLOD;

	/** Texture paint: Will hold a list of texture items that we can paint on */
	TArray<FTextureTargetListInfo> TexturePaintTargetList;

	/** Map of settings for each StaticMeshComponent */
	TMap< UStaticMeshComponent*, StaticMeshSettings > StaticMeshSettingsMap;

	/** Used to store a flag that will tell the tick function to restore data to our rendertargets after they have been invalidated by a viewport resize. */
	UBOOL bDoRestoreRenTargets;

	/** Temporary rendertarget used to draw incremental paint to */
	UTextureRenderTarget2D* BrushRenderTargetTexture;
	
	/** Temporary rendertarget used to store a mask of the affected paint region, updated every time we add incremental texture paint */ 
	UTextureRenderTarget2D* BrushMaskRenderTargetTexture;
	
	/** Temporary rendertarget used to store generated mask for texture seams, we create this by projecting object triangles into texture space using the selected UV channel */
	UTextureRenderTarget2D* SeamMaskRenderTargetTexture;
};



/**
* Imports vertex colors from tga file onto selected meshes
*/
class ImportVertexTextureHelper  
{
public:
	enum ChannelsMask{
		ERed		= 0x1,
		EGreen		= 0x2,
		EBlue		= 0x4,
		EAlpha		= 0x8,
	};

	/**
	* PickVertexColorFromTex() - Color picker function. Retrieves pixel color from coordinates and mask.
	* @param NewVertexColor - returned color
	* @param MipData - Highest mip-map with pixels data
	* @param UV - texture coordinate to read
	* @param Tex - texture info
	* @param ColorMask - mask for filtering which colors to use
	*/
	void PickVertexColorFromTex(FColor & NewVertexColor, BYTE* MipData, FVector2D & UV, UTexture2D* Tex, BYTE & ColorMask);
	/**
	* Imports Vertex Color date from texture scanning thought uv vertex coordinates for selected actors.  
	* @param Path - path for loading TGA file
	* @param UVIndex - Coordinate index
	* @param ImportLOD - LOD level to work with
	* @param Tex - texture info
	* @param ColorMask - mask for filtering which colors to use
	*/
	void Apply(FString & Path, INT UVIndex, INT ImportLOD, BYTE ColorMask);
};
#endif	// __MeshPaintEdMode_h__
