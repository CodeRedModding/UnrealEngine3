/*=============================================================================
	UnEdModeTools.h: Tools that the editor modes rely on
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * Base class for utility classes that storing settings specific to each mode/tool.
 */
class FToolSettings
{
public:
	virtual ~FToolSettings() {}
};

// -----------------------------------------------------------------------------

class FTerrainToolSettings : public FToolSettings
{
public:
	FTerrainToolSettings():
		  RadiusMin(128)
		, RadiusMax(512)
		, Strength(100.0f)
		, DecoLayer(0)
		, LayerIndex(INDEX_NONE)
		, MaterialIndex(-1)
		, FlattenAngle(FALSE)
		, FlattenHeight(0.f)
		, UseFlattenHeight(FALSE)
		, ScaleFactor(1.0f)
		, MirrorSetting(TTMirror_NONE)
		, bSoftSelectionEnabled(FALSE)
		, CurrentTerrain(NULL)
	{}

	INT RadiusMin, RadiusMax;
	FLOAT Strength;

	UBOOL	DecoLayer;
	INT		LayerIndex;		// INDEX_NONE = Heightmap
	INT		MaterialIndex;	// -1 = No specific material

	UBOOL	FlattenAngle;
	FLOAT	FlattenHeight;
	UBOOL	UseFlattenHeight;

	FLOAT	ScaleFactor;

	FString	HeightMapExporterClass;

	enum MirrorFlags
	{
		TTMirror_NONE = 0,
		TTMirror_X,
		TTMirror_Y,
		TTMirror_XY
	};

	MirrorFlags MirrorSetting;

	UBOOL bSoftSelectionEnabled;

	class ATerrain* CurrentTerrain;
};

// -----------------------------------------------------------------------------

class FTextureToolSettings : public FToolSettings
{
public:
	FTextureToolSettings() {}
};

// -----------------------------------------------------------------------------

class FStaticMeshToolSettings : public FToolSettings
{
public:
	FStaticMeshToolSettings() {}
};

// -----------------------------------------------------------------------------

enum EGeomSelectionType
{
	GS_Object,
	GS_Poly,
	GS_Edge,
	GS_Vertex,
};

enum EGeomSelectionStatus
{
	GSS_None = 0,
	GSS_Polygon = 1,
	GSS_Edge = 2,
	GSS_Vertex = 4,
};

// -----------------------------------------------------------------------------

class FGeometryToolSettings : public FToolSettings
{
public:
	FGeometryToolSettings() :
			bShowModifierWindow(0)
	{}

	/** If TRUE, show the modifier window. */
	UBOOL bShowModifierWindow;
};

/*-----------------------------------------------------------------------------
	Misc
-----------------------------------------------------------------------------*/

enum EModeTools
{
	MT_None,
	MT_TerrainPaint,			// Painting on heightmaps/layers
	MT_TerrainSmooth,			// Smoothing height/alpha maps
	MT_TerrainAverage,			// Averaging height/alpha maps
	MT_TerrainFlatten,			// Flattening height/alpha maps
	MT_TerrainFlattenSpecific,	// Flattening height/alpha maps to a specific value
	MT_TerrainNoise,			// Adds random noise into the height/alpha maps
	MT_TerrainVisibility,		// Add/remove holes in terrain
	MT_TerrainTexturePan,		// Pan the terrain texture
	MT_TerrainTextureRotate,	// Rotate the terrain texture
	MT_TerrainTextureScale,		// Scale the terrain texture
	MT_InterpEdit,
	MT_GeometryModify,			// Modification of geometry through modifiers
	MT_StaticMesh,				// Placing static meshes in the level.
	MT_Texture,					// Modifying texture alignment via the widget
	MT_TerrainSplitX,			// Split the terrain along the X-axis
	MT_TerrainSplitY,			// Split the terrain along the Y-axis
	MT_TerrainMerge,			// Merge two terrains into a single one
	MT_TerrainAddRemoveSectors,	// Add/remove sectors to existing terrain
	MT_TerrainOrientationFlip,	// Flip the orientation of a terrain quad
	MT_TerrainVertexEdit		// Edit vertices of terrain directly
};

/*-----------------------------------------------------------------------------
	FModeTool.
-----------------------------------------------------------------------------*/

/**
 * Base class for all editor mode tools.
 */
class FModeTool
{
public:
	FModeTool();
	virtual ~FModeTool();

	/** Returns the name that gets reported to the editor. */
	virtual FString GetName() const		{ return TEXT("Default"); }

	// User input

	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y) { return 0; }

	/**
	 * Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewportClient	Level editor viewport client that captured the mouse input
	 * @param	InViewport			Viewport that captured the mouse input
	 * @param	InMouseX			New mouse cursor X coordinate
	 * @param	InMouseY			New mouse cursor Y coordinate
	 *
	 * @return	TRUE if input was handled
	 */
	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY ) { return FALSE; }


	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputAxis(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime)
	{
		return FALSE;
	}

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale)
	{
		return FALSE;
	}

	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event)
	{
		return FALSE;
	}

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);

	//@{
	virtual UBOOL StartModify()	{ return 0; }
	virtual UBOOL EndModify()	{ return 0; }
	//@}

	//@{
	virtual void StartTrans()	{}
	virtual void EndTrans()		{}
	//@}

	// Tick
	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime) {}

	/** @name Selections */
	//@{
	virtual void SelectNone() {}
	/** @return		TRUE if something was selected/deselected, FALSE otherwise. */
	virtual UBOOL BoxSelect( FBox& InBox, UBOOL InSelect = TRUE )
	{
		return FALSE;
	}
	//@}

	virtual UBOOL FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect = TRUE )
	{
		return FALSE;
	}

	/** Returns the tool type. */
	EModeTools GetID() const			{ return ID; }

	/** Returns true if this tool wants to have input filtered through the editor widget. */
	UBOOL UseWidget() const				{ return bUseWidget; }

	/** Returns the active tool settings. */
	FToolSettings* GetSettings()	{ return Settings; }

protected:
	/** Which tool this is. */
	EModeTools ID;

	/** If true, this tool wants to have input filtered through the editor widget. */
	UBOOL bUseWidget;

	/** Tool settings. */
	FToolSettings* Settings;
};

/*-----------------------------------------------------------------------------
	FModeTool_GeometryModify.	
-----------------------------------------------------------------------------*/

/**
 * Widget manipulation of geometry.
 */
class FModeTool_GeometryModify : public FModeTool
{
public:
	FModeTool_GeometryModify();

	virtual FString GetName() const		{ return TEXT("Modifier"); }

	void SetCurrentModifier( UGeomModifier* InModifier )
	{
		if( CurrentModifier )
		{
			CurrentModifier->WasDeactivated();
		}
		CurrentModifier = InModifier;
		CurrentModifier->WasActivated();
	}
	UGeomModifier* GetCurrentModifier()						{ return CurrentModifier; }

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	virtual UBOOL StartModify();
	virtual UBOOL EndModify();

	virtual void StartTrans();
	virtual void EndTrans();

	virtual void SelectNone();
	virtual UBOOL BoxSelect( FBox& InBox, UBOOL InSelect = TRUE );
	virtual UBOOL FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect = TRUE );

	/** @name Modifier iterators */
	//@{
	typedef TArray<UGeomModifier*>::TIterator TModifierIterator;
	typedef TArray<UGeomModifier*>::TConstIterator TModifierConstIterator;

	TModifierIterator		ModifierIterator()				{ return TModifierIterator( Modifiers ); }
	TModifierConstIterator	ModifierConstIterator() const	{ return TModifierConstIterator( Modifiers ); }

	// @todo DB: Get rid of these; requires changes to UnGeom.cpp
	UGeomModifier* GetModifier(INT Index)					{ return Modifiers( Index ); }
	const UGeomModifier* GetModifier(INT Index) const		{ return Modifiers( Index ); }
	//@}

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);

	/** Used to track when actualy modification takes place */
	UBOOL bGeomModified;

protected:
	/** All available modifiers. */
	TArray<UGeomModifier*> Modifiers;

	/** The current modifier. */
	UGeomModifier* CurrentModifier;	
};

/*-----------------------------------------------------------------------------
	FModeTool_Terrain.
-----------------------------------------------------------------------------*/

/**
 * Base class for all terrain tools.
 */
class FModeTool_Terrain : public FModeTool
{
	TArray<class ATerrain*> ModifiedTerrains;
	UBOOL					bIsTransacting;

	typedef TArray<FLOAT>	TerrainPartialValues;
	struct FPartialData
	{
		INT								LayerCount;
		INT								DecoLayerCount;
		TArray<TerrainPartialValues>	EditValues;
	};
	static TMap<ATerrain*, FPartialData*>	PartialValueData;

public:
	FViewportClient*	PaintingViewportClient;

	FModeTool_Terrain();

	virtual FString GetName() const				{ return TEXT("N/A");	}
	virtual UBOOL SupportsMirroring() const		{ return TRUE;			}

	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y);

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	/** Casts a ray from the viewpoint through a given pixel and returns the hit terrain/location. */
	class ATerrain* TerrainTrace(FViewport* Viewport,const FSceneView* View,FVector& Location,FVector& Normal,FTerrainToolSettings* Settings, UBOOL bGetFirstHit = FALSE);

	/**
	 * Determines how strongly the editing circles affect a vertex.
	 *
	 * @param	ToolCenter		The location being pointed at on the terrain
	 * @param	Vertex			The vertex being affected
	 * @param	MinRadius		The outer edge of the inner circle
	 * @param	MaxRadius		The outer edge of the outer circle
	 *
	 * @return	A vaue between 0-1, representing how strongly the tool should affect the vertex.
	 */
	inline FLOAT RadiusStrength(const FVector& ToolCenter,const FVector& Vertex,FLOAT MinRadius,FLOAT MaxRadius)
	{
		FLOAT	Distance = (Vertex - ToolCenter).Size2D();
		if (Distance <= MinRadius)
		{
			return 1.0f;
		}
		else if (Distance < MaxRadius)
		{
			return (1.0f - (Distance - MinRadius) / (MaxRadius - MinRadius));
		}
		else
		{
			return 0.0f;
		}
	}

	// BeginApplyTool
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

	// ApplyTool
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE) = 0;

	// EndApplyTool
	virtual void EndApplyTool();

	/**
	 *	Retreive the mirrored versions of the Min/Max<X/Y> values
	 *
	 *	@param	Terrain		Pointer to the terrain of interest
	 *	@param	InMinX		The 'source' MinX value
	 *	@param	InMinY		The 'source' MinY value
	 *	@param	InMaxX		The 'source' MaxX value
	 *	@param	InMaxY		The 'source' MaxY value
	 *	@param	bMirrorX	Whether to mirror about the X axis
	 *	@param	bMirrorY	Whether to mirror about the Y axis
	 *	@param	OutMinX		The output of the mirrored MinX value
	 *	@param	OutMinY		The output of the mirrored MinY value
	 *	@param	OutMaxX		The output of the mirrored MaxX value
	 *	@param	OutMaxY		The output of the mirrored MaxY value
	 */
	virtual void GetMirroredExtents(ATerrain* Terrain, INT InMinX, INT InMinY, INT InMaxX, INT InMaxY, 
		UBOOL bMirrorX, UBOOL bMirrorY, INT& OutMinX, INT& OutMinY, INT& OutMaxX, INT& OutMaxY);

	// TerrainIsValid
	UBOOL TerrainIsValid(ATerrain* TestTerrain, FTerrainToolSettings* Settings);

	// Render
	virtual UBOOL RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI ) { return TRUE; }

	/**
	 *	UpdateIntermediateValues
	 *	Update the intermediate values of the edited quads.
	 *
	 *	@param	Terrain			Pointer to the terrain of interest
	 *	@param	DecoLayer		Boolean indicating the layer is a deco layer
	 *	@param	LayerIndex		The index of the layer being edited
	 *	@param	MaterialIndex	The index of the material being edited
	 *	@param	InMinX			The 'source' MinX value
	 *	@param	InMinY			The 'source' MinY value
	 *	@param	InMaxX			The 'source' MaxX value
	 *	@param	InMaxY			The 'source' MaxY value
	 *	@param	bMirrorX		Whether to mirror about the X axis
	 *	@param	bMirrorY		Whether to mirror about the Y axis
	 */
	virtual void UpdateIntermediateValues(ATerrain* Terrain, 
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		INT InMinX, INT InMinY, INT InMaxX, INT InMaxY, 
		UBOOL bMirrorX, UBOOL bMirrorY);

	/** Partial data values held during editing */
	/**
	 *	Clear the partial data array.
	 *	Called when closing the terrain editor dialog.
	 */
	static void ClearPartialData();
	/**
	 *	Add the given terrain to the partial data array.
	 *
	 *	@param	InTerrain		The terrain of interest
	 */
	static void AddTerrainToPartialData(ATerrain* InTerrain);
	/**
	 *	Remove the given terrain from the partial data array.
	 *
	 *	@param	InTerrain		The terrain of interest
	 */
	static void RemoveTerrainToPartialData(ATerrain* InTerrain);
	/**
	 *	Clear the partial data array to zeroes.
	 *	Called when switching tools.
	 */
	static void ZeroAllPartialData();
	/**
	 *	Check that the stored count of data is the same as required for the given terrain.
	 *	Called when post-edit change on terrain occurs.
	 *
	 *	@param	InTerrain		The terrain of that had a property change
	 */
	static void CheckPartialData(ATerrain* InTerrain);
	/**
	 *	Get the partial data value for the given terrain at the given coordinates.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	DataIndex		The index of the data required. -1 = Heightmap, otherwise AlphaMapIndex
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *
	 *	@return FLOAT			The partial alpha value
	 */
	static FLOAT GetPartialData(ATerrain* InTerrain, INT DataIndex, INT X, INT Y);
	/**
	 *	Get the alpha value for the given terrain at the given coordinates, including the partial.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	DataIndex		The index of the data required. -1 = Heightmap, otherwise AlphaMapIndex
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *
	 *	@return FLOAT			The current + partial alpha value
	 */
	static FLOAT GetData(ATerrain* InTerrain, INT DataIndex, INT X, INT Y);
	/**
	 *	Set the partial alpha value for the given terrain at the given coordinates to the given value.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	DataIndex		The index of the data required. -1 = Heightmap, otherwise AlphaMapIndex
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *	@param	NewPartial		The new value to set it to.
	 */
	static void SetPartialData(ATerrain* InTerrain, INT DataIndex, INT X, INT Y, FLOAT NewPartial);
	/**
	 *	Get the partial height value for the given terrain at the given coordinates.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *
	 *	@return FLOAT			The partial height value
	 */
	static FLOAT GetPartialHeight(ATerrain* InTerrain, INT X, INT Y)
	{
		return GetPartialData(InTerrain, -1, X, Y);
	}
	/**
	 *	Get the height value for the given terrain at the given coordinates, including the partial.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *
	 *	@return FLOAT			The current + partial height value
	 */
	static FLOAT GetHeight(ATerrain* InTerrain, INT X, INT Y)
	{
		return GetData(InTerrain, -1, X, Y);
	}
	/**
	 *	Set the partial height value for the given terrain at the given coordinates to the given value.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *	@param	NewPartial		The new value to set it to.
	 */
	static void SetPartialHeight(ATerrain* InTerrain, INT X, INT Y, FLOAT NewPartial)
	{
		SetPartialData(InTerrain, -1, X, Y, NewPartial);
	}
	/**
	 *	Get the partial alpha value for the given terrain at the given coordinates.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *
	 *	@return FLOAT			The partial alpha value
	 */
	static FLOAT GetPartialAlpha(ATerrain* InTerrain, INT AlphaMapIndex, INT X, INT Y)
	{
		return GetPartialData(InTerrain, AlphaMapIndex, X, Y);
	}
	/**
	 *	Get the alpha value for the given terrain at the given coordinates, including the partial.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *
	 *	@return FLOAT			The current + partial alpha value
	 */
	static FLOAT GetAlpha(ATerrain* InTerrain, INT AlphaMapIndex, INT X, INT Y)
	{
		return GetData(InTerrain, AlphaMapIndex, X, Y);
	}
	/**
	 *	Set the partial alpha value for the given terrain at the given coordinates to the given value.
	 *
	 *	@param	InTerrain		The terrain of interest
	 *	@param	X				The vertex X
	 *	@param	Y				The vertex Y
	 *	@param	NewPartial		The new value to set it to.
	 */
	static void SetPartialAlpha(ATerrain* InTerrain, INT AlphaMapIndex, INT X, INT Y, FLOAT NewPartial)
	{
		SetPartialData(InTerrain, AlphaMapIndex, X, Y, NewPartial);
	}

protected:
	INT FindMatchingTerrainLayer( ATerrain* TestTerrain, FTerrainToolSettings* Settings );
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainPaint.
-----------------------------------------------------------------------------*/

/**
 * For painting terrain heightmaps/layers.
 */
class FModeTool_TerrainPaint : public FModeTool_Terrain
{
public:
	FModeTool_TerrainPaint();

	virtual FString GetName() const		{ return TEXT("Paint"); }

	// FModeTool_Terrain interface.

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSmooth.
-----------------------------------------------------------------------------*/

/**
 * Smooths heightmap vertices/layer alpha maps.
 */
class FModeTool_TerrainSmooth : public FModeTool_Terrain
{
public:
	FModeTool_TerrainSmooth();

	virtual FString GetName() const		{ return TEXT("Smooth"); }

	// FModeTool_Terrain interface.

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainAverage.
-----------------------------------------------------------------------------*/

/**
 * Averages heightmap vertices/layer alpha maps.
 */
class FModeTool_TerrainAverage : public FModeTool_Terrain
{
public:
	FModeTool_TerrainAverage();

	virtual FString GetName() const		{ return TEXT("Average"); }

	// FModeTool_Terrain interface.

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainFlatten.
-----------------------------------------------------------------------------*/

/**
 * Flattens heightmap vertices/layer alpha maps.
 */
class FModeTool_TerrainFlatten : public FModeTool_Terrain
{
public:
	FLOAT	FlatValue;
	FVector	FlatWorldPosition;
	FVector	FlatWorldNormal;

	FModeTool_TerrainFlatten();

	virtual FString GetName() const		{ return TEXT("Flatten"); }

	// FModeTool_Terrain interface.

	// BeginApplyTool
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

	// ApplyTool
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainNoise.
-----------------------------------------------------------------------------*/

/**
 * For adding random noise heightmaps/layers.
 */
class FModeTool_TerrainNoise : public FModeTool_Terrain
{
public:
	FModeTool_TerrainNoise();

	virtual FString GetName() const		{ return TEXT("Noise"); }

	// FModeTool_Terrain interface.

	// ApplyTool
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainVisibility.
-----------------------------------------------------------------------------*/

/**
 *	Used for painting 'holes' in the terrain
 */
class FModeTool_TerrainVisibility : public FModeTool_Terrain
{
public:
	FModeTool_TerrainVisibility();

	virtual FString GetName() const		{ return TEXT("Visibility"); }

	// FModeTool_Terrain interface.

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainTexturePan.
-----------------------------------------------------------------------------*/

/**
 *	
 */
class FModeTool_TerrainTexturePan : public FModeTool_Terrain
{
public:
	FModeTool_TerrainTexturePan();

	virtual FString GetName() const				{ return TEXT("Tex Pan");	}
	virtual UBOOL SupportsMirroring() const		{ return FALSE;				}

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

protected:
	FVector	LastPosition;
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainTextureRotate.
-----------------------------------------------------------------------------*/

/**
 *	
 */
class FModeTool_TerrainTextureRotate : public FModeTool_Terrain
{
public:
	FModeTool_TerrainTextureRotate();

	virtual FString GetName() const				{ return TEXT("Tex Rotate");	}
	virtual UBOOL SupportsMirroring() const		{ return FALSE;					}

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

protected:
	FVector	LastPosition;
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainTextureScale.
-----------------------------------------------------------------------------*/

/**
 *	
 */
class FModeTool_TerrainTextureScale : public FModeTool_Terrain
{
public:
	FModeTool_TerrainTextureScale();

	virtual FString GetName() const				{ return TEXT("Tex Scale"); }
	virtual UBOOL SupportsMirroring() const		{ return FALSE;				}

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,
		INT MinX,INT MinY,INT MaxX,INT MaxY, 
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);

protected:
	FVector	LastPosition;
};

/*-----------------------------------------------------------------------------
	FModeTool_Texture.
-----------------------------------------------------------------------------*/

class FModeTool_Texture : public FModeTool
{
public:
	FModeTool_Texture();

	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	// override these to allow this tool to keep track of the users dragging during a single drag event
	virtual UBOOL StartModify()	{ PreviousInputDrag = FVector(0.0f); return TRUE; }
	virtual UBOOL EndModify()	{ return TRUE; }

private:
	FVector PreviousInputDrag;
};

/*-----------------------------------------------------------------------------
	FModeTool_StaticMesh.
-----------------------------------------------------------------------------*/

class FModeTool_StaticMesh : public FModeTool, public FSerializableObject
{
public:
	FModeTool_StaticMesh();

	UStaticMeshMode_Options* StaticMeshModeOptions;

	virtual void Serialize(FArchive& Ar);

	/**
	 * Applies the stored static mesh options to the given static mesh actor.
	 *
	 * @param	MeshActor	The static mesh actor to apply settings to, this now applies to SpeedTrees also.
	 */
	void ApplySettings( AActor* MeshActor );
};

/*-----------------------------------------------------------------------------
	FModeTool_VertexLock.
-----------------------------------------------------------------------------*/

class FModeTool_VertexLock : public FModeTool
{
public:
	FModeTool_VertexLock();

	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSplitX.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainSplitX : public FModeTool_Terrain
{
public:
	FModeTool_TerrainSplitX() { ID = MT_TerrainSplitX; }
	virtual FString GetName() const				{ return TEXT("SplitX");	}
	virtual UBOOL SupportsMirroring() const		{ return FALSE;				}

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE) 
	{
		return TRUE;
	}
	virtual UBOOL RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );
};
/*-----------------------------------------------------------------------------
	FModeTool_TerrainSplitY.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainSplitY : public FModeTool_Terrain
{
public:
	FModeTool_TerrainSplitY() { ID = MT_TerrainSplitY; }
	virtual FString GetName() const				{ return TEXT("SplitY");	}
	virtual UBOOL SupportsMirroring() const		{ return FALSE;				}

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE)
	{
		return TRUE;
	}
	virtual UBOOL RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );
};


/*-----------------------------------------------------------------------------
	FModeTool_Merge.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainMerge : public FModeTool_Terrain
{
public:
	FModeTool_TerrainMerge() { ID = MT_TerrainMerge; }
	virtual FString GetName() const				{ return TEXT("Merge");		}
	virtual UBOOL SupportsMirroring() const		{ return FALSE;				}

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE)
	{
		return TRUE;
	}
	virtual UBOOL RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainAddRemoveSectors.
-----------------------------------------------------------------------------*/

/**
 * For adding/removing Sectors to existing terrain
 */
class FModeTool_TerrainAddRemoveSectors : public FModeTool_Terrain
{
public:
	FModeTool_TerrainAddRemoveSectors()	{ ID = MT_TerrainAddRemoveSectors; }

	virtual FString GetName() const		{ return TEXT("AddRemoveSectors"); }

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual void EndApplyTool();
	virtual UBOOL RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

protected:
	ATerrain* CurrentTerrain;
	FVector	StartPosition;
	FLOAT Direction;
	FVector	CurrPosition;
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainOrientationFlip.
-----------------------------------------------------------------------------*/

/**
 *  For flipping the orientation of a quad (edge flipping)
 */
class FModeTool_TerrainOrientationFlip : public FModeTool_Terrain
{
public:
	FModeTool_TerrainOrientationFlip()	{ ID = MT_TerrainOrientationFlip; }
	virtual FString GetName() const		{ return TEXT("OrientationFlip"); }

	// FModeTool_Terrain interface.
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainVertexEdit.
-----------------------------------------------------------------------------*/

/**
* For editing terrain vertices directly.
*/
class FModeTool_TerrainVertexEdit : public FModeTool_Terrain
{
public:
	FModeTool_TerrainVertexEdit()
	{
		ID = MT_TerrainVertexEdit;
		bCtrlIsPressed = FALSE;
		bAltIsPressed = FALSE;
		bSelectVertices = FALSE;
		bDeselectVertices = FALSE;
		bMouseLeftPressed = FALSE;
		bMouseRightPressed = FALSE;
		MouseYDelta = 0;
	}

	virtual FString GetName() const		{ return TEXT("VertexEdit"); }

	// FModeTool interface.
	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y);

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputAxis(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime);

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);

	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);

	// FModeTool_Terrain interface.
	virtual UBOOL BeginApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,const FVector& WorldPosition, const FVector& WorldNormal,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual UBOOL ApplyTool(ATerrain* Terrain,
		UBOOL DecoLayer,INT LayerIndex,INT MaterialIndex,
		const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,
		FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY,
		UBOOL bMirrorX = FALSE, UBOOL bMirrorY = FALSE);
	virtual void EndApplyTool();
	virtual UBOOL RenderTerrain(ATerrain* Terrain, const FVector HitLocation, const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

protected:
	UBOOL bCtrlIsPressed;
	UBOOL bAltIsPressed;
	UBOOL bSelectVertices;
	UBOOL bDeselectVertices;
	UBOOL bMouseLeftPressed;
	UBOOL bMouseRightPressed;
	INT MouseYDelta;
};

