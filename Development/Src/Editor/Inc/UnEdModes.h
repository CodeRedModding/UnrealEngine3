/*=============================================================================
	UnEdModes : Classes for handling the various editor modes

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

enum EEditorMode
{
	EM_None = 0,			// Gameplay, editor disabled.
	EM_Default,				// Camera movement, actor placement
	EM_TerrainEdit,			// Terrain editing.
	EM_Geometry,			// Geometry editing mode
	EM_InterpEdit,			// Interpolation editing
	EM_Texture,				// Texture aligment via the widget.
};

/*------------------------------------------------------------------------------
    Base class.
------------------------------------------------------------------------------*/

class FEdMode
{
public:
	FEdMode();
	~FEdMode();

	FString Desc;					// Description for the editor to display
	void *BitmapOn, *BitmapOff;		// EMaskedBitmap pointers
	UEdModeComponent* Component;	// The scene component specific to this mode.
	class WxModeBar* ModeBar;		// The bar to display at the top of the editor when this mode is current.
	EAxis CurrentWidgetAxis;		// The current axis that is being dragged on the widget

	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y);
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event);
	UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual UBOOL StartTracking();
	virtual UBOOL EndTracking();

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	virtual void ActorMoveNotify() {}
	virtual void CamMoveNotify(FEditorLevelViewportClient* ViewportClient) {}
	virtual void ActorSelectionChangeNotify() {}
	virtual void ActorPropChangeNotify() {}
	virtual void MapChangeNotify() {}
	virtual UBOOL ShowModeWidgets() { return 1; }

	/** If the EdMode is handling InputDelta (ie returning true from it), this allows a mode to indicated whether or not the Widget should also move. */
	virtual UBOOL AllowWidgetMove() { return true; }

	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor ) { return 1; }
	virtual UBOOL GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData ) { return 0; }
	virtual UBOOL GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData ) { return 0; }
	virtual INT GetWidgetAxisToDraw( EWidgetMode InWidgetMode );
	virtual FVector GetWidgetLocation();
	virtual UBOOL ShouldDrawWidget();
	virtual void UpdateInternalData() {}

	virtual FVector GetWidgetNormalFromCurrentAxis( void* InData );

	void ClearComponent();
	void UpdateComponent();

	virtual void Enter();
	virtual void Exit();
	virtual UTexture2D* GetVertexTexture() { return GEngine->GetLevel()->GetLevelInfo()->BSPVertex; }
	virtual UBOOL IsVertexSelected( ABrush* InBrush, FVector* InVertex ) { return 1; }
	virtual UBOOL UsesWidget();

	virtual void SoftSelect() {}
	virtual void SoftSelectClear() {}

	virtual void PostUndo() {}

	virtual UBOOL ExecDelete();

	void BoxSelect( FBox& InBox, UBOOL InSelect = 1 );

	virtual void Serialize( FArchive &Ar ) {}

	/**
	 * Allows an editor mode to reject or allow selections on specific types of actors.
	 *
	 * @param	InActor		The actor being considered for selection
	 */
	virtual UBOOL CanSelect( AActor* InActor ) { return 1; }

	inline EEditorMode GetID() { return ID; }

	friend class FEditorModeTools;

	// Tools

	void SetCurrentTool( EModeTools InID );
	void SetCurrentTool( FModeTool* InModeTool );
	FModeTool* FindTool( EModeTools InID );
	virtual void CurrentToolChanged() {}

	inline FModeTool* GetCurrentTool()	{ return CurrentTool; }

	TArray<FModeTool*> Tools;		// Optional array of tools for this mode
	FModeTool* CurrentTool;			// The tool that is currently active within this mode

	// Settings
	
	FToolSettings* Settings;
	virtual FToolSettings* GetSettings();

	// Rendering

	virtual DWORD GetLayerMask(const FSceneContext& Context) const
	{
		return PLM_Background | PLM_Translucent | PLM_Opaque | PLM_Foreground;
	}
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void RenderBackground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	void DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,FSceneView* View,FPrimitiveRenderInterface* PRI);

	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,const FSceneContext& Context,FRenderInterface* RI);

protected:
	EEditorMode ID;					// The enumerated ID
};

/*------------------------------------------------------------------------------
    Default.

	The default mode.  User can work with BSP and the builder brush.
------------------------------------------------------------------------------*/

class FEdModeDefault : public FEdMode
{
public:
	FEdModeDefault();
	~FEdModeDefault();
};

/*------------------------------------------------------------------------------
    Vertex Editing.

	Allows the editing of vertices on BSP brushes.
------------------------------------------------------------------------------*/

class FSelectedVertex
{
public:
	FSelectedVertex()
	{ check(0); }
	FSelectedVertex( ABrush* InBrush, FVector* InVertex )
	{
		Brush = InBrush;
		Vertex = InVertex;
	}
	~FSelectedVertex()
	{}

	UBOOL operator==(const FSelectedVertex& In)
	{
		return ( Brush == In.Brush && Vertex == In.Vertex );
	}
	FSelectedVertex& operator=( const FSelectedVertex& In)
	{
		Brush = In.Brush;
		Vertex = In.Vertex;
		return *this;
	}

	ABrush* Brush;		// The brush containing the vertex
	FVector* Vertex;	// The vertex itself
};

/*------------------------------------------------------------------------------
	Geometry Editing.

	Allows MAX "edit mesh"-like editing of BSP geometry.
------------------------------------------------------------------------------*/

class FEdModeGeometry : public FEdMode
{
public:
	FEdModeGeometry();
	~FEdModeGeometry();

	/** 
	 * Custom data compiled when this mode is entered, based on currently
	 * selected brushes.  This data is what is drawn and what the LD
	 * interacts with while in this mode.  Changes done here are
	 * reflected back to the real data in the level at specific times.
	 */
	TArray<FGeomObject> GeomObjects;

	/** The modifier window that pops up when the mode is active. */
	WxGeomModifiers* ModifierWindow;

	/**
	 * Used to render geometry elements.
	 */
	FMaterialInstance ColorInstance;

	virtual UBOOL CanSelect( AActor* InActor );
	virtual void CurrentToolChanged();

	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual UBOOL ShowModeWidgets();
	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor );
	virtual UBOOL GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual UBOOL GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual void Enter();
	virtual void Exit();
	virtual void ActorSelectionChangeNotify();
	virtual void MapChangeNotify();
	void SelectNone();
	void UpdateModifierWindow();
	virtual void Serialize( FArchive &Ar );
	void GetFromSource();
	void SendToSource();
	UBOOL FinalizeSourceData();
	virtual void PostUndo();
	virtual UBOOL ExecDelete();
	virtual void UpdateInternalData();

	void ApplyToVertex( FGeomVertex* InVtx, FVector& InDrag, FRotator& InRot, FVector& InScale );

	void RenderSinglePoly( FGeomPoly* InPoly, const FSceneContext &InContext, FPrimitiveRenderInterface* InPRI, FTriangleRenderInterface* InTRI );
	void RenderSingleEdge( FGeomEdge* InEdge, FColor InColor, const FSceneContext &InContext, FPrimitiveRenderInterface* InPRI, UBOOL InShowEdgeMarkers );
	void RenderSinglePolyWireframe( FGeomPoly* InPoly, FColor InColor, const FSceneContext &InContext, FPrimitiveRenderInterface* InPRI );
	void RenderObject( const FSceneContext& Context, FPrimitiveRenderInterface* PRI );
	void RenderPoly( const FSceneContext& Context, FPrimitiveRenderInterface* PRI );
	void RenderEdge( const FSceneContext& Context, FPrimitiveRenderInterface* PRI );
	void RenderVertex( const FSceneContext& Context, FPrimitiveRenderInterface* PRI );

	virtual void SoftSelect();
	virtual void SoftSelectClear();
};

/*------------------------------------------------------------------------------
	Terrain Editing.

	Allows editing of terrain heightmaps and their layers.
------------------------------------------------------------------------------*/

class FEdModeTerrainEditing : public FEdMode
{
public:
	FEdModeTerrainEditing();
	~FEdModeTerrainEditing();

	virtual FToolSettings* GetSettings();

	UBOOL bPerToolSettings;

	ATerrain* CurrentTerrain;

	void DrawToolCircle( FPrimitiveRenderInterface* PRI, class ATerrain* Terrain, FVector Location, FLOAT Radius );
	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
};

/*------------------------------------------------------------------------------
	Texture.

	Allows texture alignment on BSP surfaces via the widget.
------------------------------------------------------------------------------*/

class FEdModeTexture : public FEdMode
{
public:
	FEdModeTexture();
	~FEdModeTexture();

	/** Stores the coordinate system that was active when the mode was entered so it can restore it later. */
	ECoordSystem SaveCoordSystem;

	virtual void Enter();
	virtual void Exit();
	virtual FVector GetWidgetLocation();
	virtual UBOOL ShouldDrawWidget();
	virtual UBOOL GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual UBOOL GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual INT GetWidgetAxisToDraw( EWidgetMode InWidgetMode );
	virtual UBOOL StartTracking();
	virtual UBOOL EndTracking();
};

/*------------------------------------------------------------------------------
	FEditorModeTools

	A helper class to store the state of the various editor modes.
------------------------------------------------------------------------------*/

class FEditorModeTools
{
public:
	FEditorModeTools();
	virtual ~FEditorModeTools();

	void Init();
	void SetCurrentMode( EEditorMode InID );
	FEdMode* FindMode( EEditorMode InID );

	FMatrix GetCustomDrawingCoordinateSystem();
	FMatrix GetCustomInputCoordinateSystem();
	
	inline FEdMode* GetCurrentMode() { return CurrentMode; }
	inline FModeTool* GetCurrentTool() { return GetCurrentMode()->GetCurrentTool(); }
	inline EEditorMode GetCurrentModeID() { return CurrentMode->GetID(); }

	inline void SetShowWidget( UBOOL InShowWidget ) { bShowWidget = InShowWidget; }
	inline UBOOL GetShowWidget() { return bShowWidget; }

	FVector GetWidgetLocation();
	void SetWidgetMode( EWidgetMode InWidgetMode );
	void SetWidgetModeOverride( EWidgetMode InWidgetMode );
	EWidgetMode GetWidgetMode();

	void SetBookMark( INT InIndex, FEditorLevelViewportClient* InViewportClient );
	void JumpToBookMark( INT InIndex );

	void Serialize( FArchive &Ar );

	TArray<FEdMode*> Modes;			// A list of all available editor modes

	UBOOL PivotShown, Snapping;
	FVector PivotLocation, SnappedLocation, GridBase;
	ECoordSystem CoordSystem;		// The coordinate system the widget is operating within
	FString InfoString;				// Draws in the top level corner of all FEditorLevelViewportClient windows (can be used to relay info to the user)

protected:
	FEdMode* CurrentMode;			// The editor mode currently in use
	EWidgetMode WidgetMode;			// The mode that the editor viewport widget is in
	EWidgetMode OverrideWidgetMode;	// If the widget mode is being overridden, this will be != WM_None
	UBOOL bShowWidget;				// If 1, draw the widget and let the user interact with it
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
