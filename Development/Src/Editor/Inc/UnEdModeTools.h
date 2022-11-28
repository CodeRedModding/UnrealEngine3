/*=============================================================================
	UnEdModeTools.h: Tools that the editor modes rely on

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	Utility classes for storing settings specific to each mode/tool.
-----------------------------------------------------------------------------*/

class FToolSettings
{
public:
	FToolSettings()
	{}
	virtual ~FToolSettings()
	{}
};

// -----------------------------------------------------------------------------

class FTerrainToolSettings : public FToolSettings
{
public:
	FTerrainToolSettings():
		RadiusMin(128.0f),
		RadiusMax(512.0f),
		Strength(100.0f),
		DecoLayer(0),
		LayerIndex(INDEX_NONE)
	{}
	virtual ~FTerrainToolSettings()
	{}

	INT RadiusMin, RadiusMax;
	FLOAT Strength;

	UBOOL	DecoLayer;
	INT		LayerIndex; // INDEX_NONE = Heightmap
};

// -----------------------------------------------------------------------------

class FTextureToolSettings : public FToolSettings
{
public:
	FTextureToolSettings()
	{}
	virtual ~FTextureToolSettings()
	{}
};

// -----------------------------------------------------------------------------

// Keep this mirrored to EGeometrySelectionType in script code!
enum EGeometrySelectionType
{
	GST_Object,
	GST_Poly,
	GST_Edge,
	GST_Vertex,
};

class FGeometryToolSettings : public FToolSettings
{
public:
	FGeometryToolSettings() :
		SelectionType( GST_Vertex )
		,	bShowNormals(0)
		,	bAffectWidgetOnly(0)
		,	bUseSoftSelection(0)
		,	SoftSelectionRadius(500)
		,	bShowModifierWindow(0)
	{}
	virtual ~FGeometryToolSettings()
	{}

	/** The type of geometry that is being selected and worked with. */
	EGeometrySelectionType SelectionType;

	/** Should normals be drawn in the editor viewports? */
	UBOOL bShowNormals;

	/**
	 * If TRUE, selected objects won't be dragged around allowing the widget
	 * to be placed manually for ease of rotations and other things.
	 */
	UBOOL bAffectWidgetOnly;

	/** If TRUE, we are using soft selection. */
	UBOOL bUseSoftSelection;

	/** The falloff radius used when soft selection is in use. */
	INT SoftSelectionRadius;

	/** If TRUE, show the modifier window. */
	UBOOL bShowModifierWindow;
};

/*-----------------------------------------------------------------------------
	Misc
-----------------------------------------------------------------------------*/

enum EModeTools
{
	MT_None,
	MT_TerrainPaint,		// Painting on heightmaps/layers
	MT_TerrainSmooth,		// Smoothing height/alpha maps
	MT_TerrainAverage,		// Averaging height/alpha maps
	MT_TerrainFlatten,		// Flattening height/alpha maps
	MT_TerrainNoise,		// Adds random noise into the height/alpha maps
	MT_InterpEdit,
	MT_GeometryModify,		// Modification of geometry through modifiers
	MT_Texture,				// Modifying texture alignment via the widget
};

/*-----------------------------------------------------------------------------
	FModeTool.

	Base class for all mode tools.
-----------------------------------------------------------------------------*/

class FModeTool
{
public:
	FModeTool();
	~FModeTool();

	EModeTools ID;				// Which tool this is
	UBOOL bUseWidget;			// If 1, this tool wants to get input filtered through the editor widget.

	// The name that gets reported to the editor

	virtual FString GetName()	{ return TEXT("Default"); }

	// User input

	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y) { return 0; }
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale) { return 0; }
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event) { return 0; }
	virtual UBOOL StartModify() { return 0; }
	virtual UBOOL EndModify() { return 0; }
	virtual void StartTrans() {}
	virtual void EndTrans() {}

	// Tick

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime) {}

	// Selections

	virtual void SelectNone() {}
	virtual void BoxSelect( FBox& InBox, UBOOL InSelect = 1 ) {}

	// Utility functions

	inline EModeTools GetID()			{ return ID; }

	// Settings
	
	FToolSettings* Settings;
	virtual FToolSettings* GetSettings();
};

/*-----------------------------------------------------------------------------
	FModeTool_GeometryModify.

	Widget manipulation of geometry.
-----------------------------------------------------------------------------*/

class FModeTool_GeometryModify : public FModeTool
{
public:
	FModeTool_GeometryModify();
	~FModeTool_GeometryModify();

	/** All available modifiers. */
	TArray<UGeomModifier*> Modifiers;
	UGeomModifier* CurrentModifier;

	virtual FString GetName()	{ return TEXT("Modifier"); }

	void SetCurrentModifier( UGeomModifier* InModifier );
	virtual void SelectNone();
	virtual void BoxSelect( FBox& InBox, UBOOL InSelect = 1 );
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual UBOOL StartModify();
	virtual UBOOL EndModify();
	virtual void StartTrans();
	virtual void EndTrans();
};

/*-----------------------------------------------------------------------------
	FModeTool_Terrain.

	Base class for all terrain tools.
-----------------------------------------------------------------------------*/

class FModeTool_Terrain : public FModeTool
{
public:

	FViewportClient*	PaintingViewportClient;

	FModeTool_Terrain();
	~FModeTool_Terrain();

	virtual FString GetName()	{ return TEXT("N/A"); }

	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y);
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event);

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	// TerrainTrace - Casts a ray from the viewpoint through a given pixel and returns the hit terrain/location.

	class ATerrain* TerrainTrace(FChildViewport* Viewport,FSceneView* View,FVector& Location);

	// RadiusStrength

	FLOAT RadiusStrength(const FVector& ToolCenter,const FVector& Vertex,FLOAT MinRadius,FLOAT MaxRadius);

	// BeginApplyTool

	virtual void BeginApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition) {}

	// ApplyTool

	virtual void ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY) = 0;
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainPaint.

	For painting terrain heightmaps/layers.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainPaint : public FModeTool_Terrain
{
public:
	FModeTool_TerrainPaint();
	~FModeTool_TerrainPaint();

	virtual FString GetName()	{ return TEXT("Paint"); }

	// FModeTool_Terrain interface.

	virtual void ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainSmooth.

	Smooths heightmap vertices/layer alpha maps.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainSmooth : public FModeTool_Terrain
{
public:
	FModeTool_TerrainSmooth();
	~FModeTool_TerrainSmooth();

	virtual FString GetName()	{ return TEXT("Smooth"); }

	// FModeTool_Terrain interface.

	virtual void ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainAverage.

	Averages heightmap vertices/layer alpha maps.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainAverage : public FModeTool_Terrain
{
public:
	FModeTool_TerrainAverage();
	~FModeTool_TerrainAverage();

	virtual FString GetName()	{ return TEXT("Average"); }

	// FModeTool_Terrain interface.

	virtual void ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainFlatten.

	Flattens heightmap vertices/layer alpha maps.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainFlatten : public FModeTool_Terrain
{
public:

	FLOAT	FlatValue;

	FModeTool_TerrainFlatten();
	~FModeTool_TerrainFlatten();

	virtual FString GetName()	{ return TEXT("Flatten"); }

	// FModeTool_Terrain interface.

	virtual void BeginApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition);
	virtual void ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY);
};

/*-----------------------------------------------------------------------------
	FModeTool_TerrainNoise.

	For adding random noise heightmaps/layers.
-----------------------------------------------------------------------------*/

class FModeTool_TerrainNoise : public FModeTool_Terrain
{
public:
	FModeTool_TerrainNoise();
	~FModeTool_TerrainNoise();

	virtual FString GetName()	{ return TEXT("Noise"); }

	// FModeTool_Terrain interface.

	virtual void ApplyTool(ATerrain* Terrain,UBOOL DecoLayer,INT LayerIndex,const FVector& LocalPosition,FLOAT LocalMinRadius,FLOAT LocalMaxRadius,FLOAT InDirection,FLOAT LocalStrength,INT MinX,INT MinY,INT MaxX,INT MaxY);
};

/*-----------------------------------------------------------------------------
	FModeTool_Texture.
-----------------------------------------------------------------------------*/

class FModeTool_Texture : public FModeTool
{
public:

	FModeTool_Texture();
	~FModeTool_Texture();

	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
