/*=============================================================================
	UnEdModes : Classes for handling the various editor modes
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

enum EEditorMode
{
	/** Gameplay, editor disabled. */
	EM_None = 0,

	/** Camera movement, actor placement. */
	EM_Default,

	/** Terrain editing. */
	EM_TerrainEdit,

	/** Geometry editing mode. */
	EM_Geometry,

	/** Static Mesh mode. */
	EM_StaticMesh,

	/** Interpolation editing. */
	EM_InterpEdit,

	/** Texture alignment via the widget. */
	EM_Texture,
	
	/** Cover positioning/editing */
	EM_CoverEdit,			
	
	/** Mesh paint tool */
	EM_MeshPaint,

	/** Spline actor editing */
	EM_Spline,

	/** Landscape editing */
	EM_Landscape,

	/** Foliage painting */
	EM_Foliage,

	/** AmbientSoundSpline actor editing - the source of sound is given as a spline */
	EM_AmbientSoundSpline,
};

class FScopedTransaction;

/**
 * Base class for all editor modes.
 */
class FEdMode : public FSerializableObject
{
public:
	FEdMode();
	virtual ~FEdMode();

	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y);

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
	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY );

	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL InputAxis(FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime);
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual UBOOL StartTracking();
	virtual UBOOL EndTracking();
	// Added for handling EDIT Command...
	virtual UBOOL ProcessEditCut() { return FALSE; }
	virtual UBOOL ProcessEditCopy() { return FALSE; }
	virtual UBOOL ProcessEditPaste() { return FALSE; }

	virtual void Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime);

	virtual void ActorMoveNotify() {}
	virtual void ActorsDuplicatedNotify(TArray<AActor*>& PreDuplicateSelection, TArray<AActor*>& PostDuplicateSelection, UBOOL bOffsetLocations) {}
	virtual void ActorSelectionChangeNotify() {}
	virtual void ActorPropChangeNotify() {}
	virtual void MapChangeNotify() {}
	virtual UBOOL ShowModeWidgets() const { return 1; }

	/** If the EdMode is handling InputDelta (ie returning true from it), this allows a mode to indicated whether or not the Widget should also move. */
	virtual UBOOL AllowWidgetMove() { return true; }

	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor ) const { return TRUE; }
	virtual UBOOL GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData ) { return 0; }
	virtual UBOOL GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData ) { return 0; }

	/**
	 * Allows each mode to customize the axis pieces of the widget they want drawn.
	 *
	 * @param	InwidgetMode	The current widget mode
	 *
	 * @return					A bitfield comprised of AXIS_* values
	 */
	virtual INT GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;

	/**
	 * Allows each mode/tool to determine a good location for the widget to be drawn at.
	 */
	virtual FVector GetWidgetLocation() const;

	/**
	 * Lets the mode determine if it wants to draw the widget or not.
	 */
	virtual UBOOL ShouldDrawWidget() const;
	virtual void UpdateInternalData() {}

	virtual FVector GetWidgetNormalFromCurrentAxis( void* InData );

	void ClearComponent();
	void UpdateComponent();
	UEditorComponent* GetComponent() const { return Component; }

	virtual void Enter();
	virtual void Exit();
	virtual UTexture2D* GetVertexTexture() { return GWorld->GetWorldInfo()->BSPVertex; }
	
	/**
	 * Lets each tool determine if it wants to use the editor widget or not.  If the tool doesn't want to use it,
	 * it will be fed raw mouse delta information (not snapped or altered in any way).
	 */
	virtual UBOOL UsesTransformWidget() const;

	/**
	 * Lets each mode selectively exclude certain widget types.
	 */
	virtual UBOOL UsesTransformWidget(FWidget::EWidgetMode CheckMode) const { return TRUE; }

	virtual void PostUndo() {}

	/**
	 * Lets each mode/tool handle box selection in its own way.
	 *
	 * @param	InBox	The selection box to use, in worldspace coordinates.
	 * @return		TRUE if something was selected/deselected, FALSE otherwise.
	 */
	UBOOL BoxSelect( FBox& InBox, UBOOL InSelect = TRUE );

	/**
	 * Lets each mode/tool handle frustum selection in its own way.
	 *
	 * @param	InFrustum	The selection box to use, in worldspace coordinates.
	 * @return	TRUE if something was selected/deselected, FALSE otherwise.
	 */
	UBOOL FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect = TRUE );

	void SelectNone();

	/**
	 * Used to serialize any UObjects references.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar)
	{
		Ar << Component;
	}

	virtual UBOOL HandleClick(HHitProxy *HitProxy, const FViewportClick &Click) { return 0; }
	virtual UBOOL Select( AActor* InActor, UBOOL bInSelected ) { return 0; }

	/** Returns the editor mode. */
	EEditorMode GetID() const { return ID; }

	friend class FEditorModeTools;

	// Tools

	void SetCurrentTool( EModeTools InID );
	void SetCurrentTool( FModeTool* InModeTool );
	FModeTool* FindTool( EModeTools InID );

	const TArray<FModeTool*>& GetTools() const		{ return Tools; }

	virtual void CurrentToolChanged() {}

	/** Returns the current tool. */
	//@{
	FModeTool* GetCurrentTool()				{ return CurrentTool; }
	const FModeTool* GetCurrentTool() const	{ return CurrentTool; }
	//@}

	/** @name Settings */
	//@{
	FToolSettings* Settings;
	virtual const FToolSettings* GetSettings() const;
	//@}

	/** @name Current widget axis. */
	//@{
	void SetCurrentWidgetAxis(EAxis InAxis)		{ CurrentWidgetAxis = InAxis; }
	EAxis GetCurrentWidgetAxis() const			{ return CurrentWidgetAxis; }
	//@}

	/** @name Rendering */
	//@{
	/** Draws translucent polygons on brushes and volumes. */
	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	//void DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,FSceneView* View,FPrimitiveDrawInterface* PDI);

	/** Overlays the editor hud (brushes, drag tools, static mesh vertices, etc*. */
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);
	//@}

	/**
	 * Called when attempting to duplicate the selected actors using a normal key bind,
	 * return TRUE to prevent normal duplication.
	 */
	virtual UBOOL HandleDuplicate() { return FALSE; }

	/**
	 * Called when attempting to duplicate the selected actors by alt+dragging,
	 * return TRUE to prevent normal duplication.
	 */
	virtual UBOOL HandleDragDuplicate() { return FALSE; }

	/**
	 * Called when the mode wants to draw brackets around selected objects
	 */
	virtual void DrawBrackets( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas );

protected:
	/** Description for the editor to display. */
	FString Desc;

	/** EMaskedBitmap pointers. */
	void *BitmapOn, *BitmapOff;

	/** The scene component specific to this mode. */
	UEdModeComponent* Component;

	/** The current axis that is being dragged on the widget. */
	EAxis CurrentWidgetAxis;

	/** Optional array of tools for this mode. */
	TArray<FModeTool*> Tools;

	/** The tool that is currently active within this mode. */
	FModeTool* CurrentTool;

	/** The enumerated ID. */
	EEditorMode ID;
};

/*------------------------------------------------------------------------------
    Default.
------------------------------------------------------------------------------*/

/**
 * The default editing mode.  User can work with BSP and the builder brush.
 */
class FEdModeDefault : public FEdMode
{
public:
	FEdModeDefault();
};

/*------------------------------------------------------------------------------
    Vertex Editing.
------------------------------------------------------------------------------*/

/**
 * Allows the editing of vertices on BSP brushes.
 */
class FSelectedVertex
{
public:
	FSelectedVertex( ABrush* InBrush, FVector* InVertex ):
	  Brush( InBrush ),
	  Vertex( InVertex )
	{}

	/** The brush containing the vertex. */
	ABrush* Brush;

	/** The vertex itself. */
	FVector* Vertex;

private:
	FSelectedVertex();
};

/*------------------------------------------------------------------------------
	Geometry Editing.	
------------------------------------------------------------------------------*/

/**
 * Allows MAX "edit mesh"-like editing of BSP geometry.
 */
class FEdModeGeometry : public FEdMode
{
public:
	/**
	 * Struct for cacheing of selected objects components midpoints for reselection when rebuilding the BSP
	 */
	struct HGeomMidPoints
	{
		/** The actor that the verts/edges/polys belong to */
		ABrush* ActualBrush;

		/** Arrays of the midpoints of all the selected verts/edges/polys */
		TArray<FVector> VertexPool;
		TArray<FVector> EdgePool;
		TArray<FVector> PolyPool;	
	};

	FEdModeGeometry();
	virtual ~FEdModeGeometry();

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual UBOOL ShowModeWidgets() const;

	/** If the actor isn't selected, we don't want to interfere with it's rendering. */
	virtual UBOOL ShouldDrawBrushWireframe( AActor* InActor ) const;
	virtual UBOOL GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual UBOOL GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual void Enter();
	virtual void Exit();
	virtual void ActorSelectionChangeNotify();
	virtual void MapChangeNotify();
	void SelectNone();
	void UpdateModifierWindow();
	virtual void Serialize( FArchive &Ar );

	virtual FVector GetWidgetLocation() const;

	/**
	* Returns the number of objects that are selected.
	*/
	INT CountObjectsSelected();

	/**
	* Returns the number of polygons that are selected.
	*/
	INT CountSelectedPolygons();

	/**
	* Returns the polygons that are selected.
	*
	* @param	InPolygons	An array to fill with the selected polygons.
	*/
	void GetSelectedPolygons( TArray<FGeomPoly*>& InPolygons );

	/**
	* Returns TRUE if the user has polygons selected.
	*/
	UBOOL HavePolygonsSelected();

	/**
	* Returns the number of edges that are selected.
	*/
	INT CountSelectedEdges();

	/**
	* Returns the edges that are selected.
	*
	* @param	InEdges	An array to fill with the selected edges.
	*/
	void GetSelectedEdges( TArray<FGeomEdge*>& InEdges );

	/**
	* Returns TRUE if the user has edges selected.
	*/
	UBOOL HaveEdgesSelected();

	/**
	* Returns the number of vertices that are selected.
	*/
	INT CountSelectedVertices();
	
	/**
	* Returns TRUE if the user has vertices selected.
	*/
	UBOOL HaveVerticesSelected();

	/**
	 * Fills an array with all selected vertices.
	 */
	void GetSelectedVertices( TArray<FGeomVertex*>& InVerts );

	/**
	 * Utility function that allow you to poll and see if certain sub elements are currently selected.
	 *
	 * Returns a combination of the flags in EGeomSelectionStatus.
	 */
	INT GetSelectionState();

	/**
	 * Cache all the selected geometry on the object, and add to the array if any is found
	 *
	 * Return TRUE if new object has been added to the array.
	 */
	UBOOL CacheSelectedData( TArray<HGeomMidPoints>& raGeomData, const FGeomObject& rGeomObject ) const;

	/**
	 * Attempt to find all the new geometry using the cached data, and cache those new ones out
	 *
	 * Return TRUE everything was found (or there was nothing to find)
	 */
	UBOOL FindFromCache( TArray<HGeomMidPoints>& raGeomData, FGeomObject& rGeomObject, TArray<FGeomBase*>& raSelectedGeom ) const;

	/**
	 * Select all the verts/edges/polys that were found
	 *
	 * Return TRUE if successful
	 */
	UBOOL SelectCachedData( TArray<FGeomBase*>& raSelectedGeom ) const;

	/**
	 * Compiles geometry mode information from the selected brushes.
	 */
	void GetFromSource();
	
	/**
	 * Changes the source brushes to match the current geometry data.
	 */
	void SendToSource();
	UBOOL FinalizeSourceData();
	virtual void PostUndo();
	UBOOL ExecDelete();
	virtual void UpdateInternalData();

	void RenderPoly( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI );
	void RenderEdge( const FSceneView* View, FPrimitiveDrawInterface* PDI );
	void RenderVertex( const FSceneView* View, FPrimitiveDrawInterface* PDI );

	void ShowModifierWindow(UBOOL bShouldShow);

	/** @name GeomObject iterators */
	//@{
	typedef TArray<FGeomObject*>::TIterator TGeomObjectIterator;
	typedef TArray<FGeomObject*>::TConstIterator TGeomObjectConstIterator;

	TGeomObjectIterator			GeomObjectItor()			{ return TGeomObjectIterator( GeomObjects ); }
	TGeomObjectConstIterator	GeomObjectConstItor() const	{ return TGeomObjectConstIterator( GeomObjects ); }

	// @todo DB: Get rid of these; requires changes to FGeomBase::ParentObjectIndex
	FGeomObject* GetGeomObject(INT Index)					{ return GeomObjects( Index ); }
	const FGeomObject* GetGeomObject(INT Index) const		{ return GeomObjects( Index ); }
	//@}

protected:
	/** 
	 * Custom data compiled when this mode is entered, based on currently
	 * selected brushes.  This data is what is drawn and what the LD
	 * interacts with while in this mode.  Changes done here are
	 * reflected back to the real data in the level at specific times.
	 */
	TArray<FGeomObject*> GeomObjects;
};

/*------------------------------------------------------------------------------
	Static Mesh.	
------------------------------------------------------------------------------*/

/**
* Allows LDs to place static meshes with more options.
*/
class FEdModeStaticMesh : public FEdMode
{
public:
	FEdModeStaticMesh();
	virtual ~FEdModeStaticMesh();
};

/*------------------------------------------------------------------------------
	Terrain Editing.
------------------------------------------------------------------------------*/

/**
 * Allows editing of terrain heightmaps and their layers.
 */
class FEdModeTerrainEditing : public FEdMode
{
public:
	FEdModeTerrainEditing();

	virtual const FToolSettings* GetSettings() const;
	
	virtual UBOOL AllowWidgetMove()	{ return FALSE;	}

	void DrawTool( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
		class ATerrain* Terrain, FVector& Location, FLOAT InnerRadius, FLOAT OuterRadius, 
		TArray<ATerrain*>& Terrains );
	void DrawToolCircle( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
		class ATerrain* Terrain, FVector& Location, FLOAT Radius, TArray<ATerrain*>& Terrains, 
		UBOOL bMirror = FALSE );
	void DrawToolCircleBallAndSticks( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
		class ATerrain* Terrain, FVector& Location, FLOAT InnerRadius, FLOAT OuterRadius, 
		TArray<ATerrain*>& Terrains, UBOOL bMirror = FALSE );
	void DrawBallAndStick( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI, 
		class ATerrain* Terrain, FVector& Location, FLOAT Strength = 1.0f );
	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);

	void DetermineMirrorLocation( FPrimitiveDrawInterface* PDI, class ATerrain* Terrain, FVector& Location );
	INT GetMirroredValue_Y(ATerrain* Terrain, INT InY, UBOOL bPatchOperation = FALSE);
	INT GetMirroredValue_X(ATerrain* Terrain, INT InX, UBOOL bPatchOperation = FALSE);

	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL HandleClick(HHitProxy *HitProxy, const FViewportClick &Click);

	UBOOL			bPerToolSettings;
	FColor			ModeColor;
	UBOOL			bConstrained;
	UBOOL			bShowBallAndSticks;

	FLinearColor	ToolColor;
	FLinearColor	MirrorColor;

	UTexture2D*		BallTexture;

	// To avoid re-calculating multiple times a frame?
	FVector			MirrorLocation;

	ATerrain*		CurrentTerrain;

	// location of the terrain under mouse cursor
	FVector			ToolHitLocation;
	// terrains currently contained in the tool circle
	TArray<class ATerrain*> ToolTerrains;
};

/*------------------------------------------------------------------------------
	Texture.
------------------------------------------------------------------------------*/

/**
 * Allows texture alignment on BSP surfaces via the widget.
 */
class FEdModeTexture : public FEdMode
{
public:
	FEdModeTexture();
	virtual ~FEdModeTexture();

	/** Stores the coordinate system that was active when the mode was entered so it can restore it later. */
	ECoordSystem SaveCoordSystem;

	virtual void Enter();
	virtual void Exit();
	virtual FVector GetWidgetLocation() const;
	virtual UBOOL ShouldDrawWidget() const;
	virtual UBOOL GetCustomDrawingCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual UBOOL GetCustomInputCoordinateSystem( FMatrix& InMatrix, void* InData );
	virtual INT GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;
	virtual UBOOL StartTracking();
	virtual UBOOL EndTracking();
	virtual UBOOL AllowWidgetMove() { return FALSE; }

protected:
	/** The current transaction. */
	FScopedTransaction*		ScopedTransaction;
};


/*------------------------------------------------------------------------------
	Cover
------------------------------------------------------------------------------*/

class FEdModeCoverEdit : public FEdMode
{
private:
	UBOOL bCanAltDrag;
	UBOOL bTabDown;
	ACoverLink* LastSelectedCoverLink;
	FCoverSlot* LastSelectedCoverSlot;

public:
	FEdModeCoverEdit();
	virtual ~FEdModeCoverEdit();

	virtual void Enter();
	virtual void Exit();

	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual void ActorSelectionChangeNotify();
	virtual UBOOL UsesTransformWidget() const { return TRUE; }
	virtual UBOOL UsesTransformWidget(FWidget::EWidgetMode CheckMode) const;
	virtual FVector GetWidgetLocation() const;
	virtual UBOOL HandleDragDuplicate();
	virtual UBOOL HandleDuplicate();
	virtual UBOOL HandleClick(HHitProxy *HitProxy, const FViewportClick &Click);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);
};



/*------------------------------------------------------------------------------
	FEditorModeTools
------------------------------------------------------------------------------*/

/**
 * A helper class to store the state of the various editor modes.
 */
class FEditorModeTools :
	// Interface for receiving notifications of events
	public FCallbackEventDevice
{
public:
	FEditorModeTools();
	virtual ~FEditorModeTools();

	void Init();

	void Shutdown();

	/**
	 * Activates an editor mode. Shuts down all other active modes which cannot run with the passed in mode.
	 * 
	 * @param InID		The ID of the editor mode to activate.
	 * @param bToggle	TRUE if the passed in editor mode should be toggled off if it is already active.
	 */
	void ActivateMode( EEditorMode InID, UBOOL bToggle = FALSE );

	/**
	 * Deactivates an editor mode. 
	 * 
	 * @param InID		The ID of the editor mode to deactivate.
	 */
	void DeactivateMode( EEditorMode InID );

	/** 
	 * Returns the editor mode specified by the passed in ID
	 */
	FEdMode* FindMode( EEditorMode InID );

	/**
	 * Returns TRUE if the current mode is not the specified ModeID.  Also optionally warns the user.
	 *
	 * @param	ModeID			The editor mode to query.
	 * @param	bNotifyUser		If TRUE, inform the user that the requested operation cannot be performed in the specified mode.
	 * @return					TRUE if the current mode is not the specified mode.
	 */
	UBOOL EnsureNotInMode(EEditorMode ModeID, UBOOL bNotifyUser) const;

	FMatrix GetCustomDrawingCoordinateSystem();
	FMatrix GetCustomInputCoordinateSystem();
	
	/** 
	 * Returns true if the passed in editor mode is active 
	 */
	UBOOL IsModeActive( EEditorMode InID ) const;

	/**
	 * Returns a pointer to an active mode specified by the passed in ID
	 * If the editor mode is not active, NULL is returned
	 */
	FEdMode* GetActiveMode( EEditorMode InID );
	const FEdMode* GetActiveMode( EEditorMode InID ) const;

	/**
	 * Returns the active tool of the passed in editor mode.
	 * If the passed in editor mode is not active or the mode has no active tool, NULL is returned
	 */
	const FModeTool* GetActiveTool( EEditorMode InID ) const;

	/** 
	 * Returns an array of all active modes
	 */
	void GetActiveModes( TArray<FEdMode*>& OutActiveModes );

	void SetShowWidget( UBOOL InShowWidget )	{ bShowWidget = InShowWidget; bFlashWidget = FALSE; }
	UBOOL GetShowWidget()
	{
		UBOOL bShow = bShowWidget;
		if( !bShow )
		{
			if( bFlashWidget )
			{
				DOUBLE TimePassed = appSeconds() - FlashWidgetStartTime;
				if( TimePassed > FlashWidgetTime )
				{
					bFlashWidget = FALSE;
				}
				bShow = TRUE;
			}
		}
		else
		{
			bFlashWidget = FALSE; // force this to FALSE in case the user switched show mode during a flash
		}
		return bShow;
	}
	void FlashWidget() { FlashWidgetStartTime = appSeconds(); FlashWidgetTime = 0.25; bFlashWidget = TRUE; };


	void CycleWidgetMode (void);

	/** Accessors for Absolute Translation Movement*/
	UBOOL IsUsingAbsoluteTranslation (void)                 { return bUseAbsoluteTranslation; }
	void SetUsingAbsoluteTranslation (const UBOOL bInOnOff) { bUseAbsoluteTranslation = bInOnOff; }

	/** Allow translate/rotate widget */
	UBOOL GetAllowTranslateRotateZWidget (void)                 { return bAllowTranslateRotateZWidget; }
	void SetAllowTranslateRotateZWidget (const UBOOL bInOnOff) { bAllowTranslateRotateZWidget = bInOnOff; }

	/**Save Widget Settings to Ini file*/
	void SaveWidgetSettings(void);
	/**Load Widget Settings from Ini file*/
	void LoadWidgetSettings(void);

	void SetMouseLock( UBOOL InMouseLock )		{ bMouseLock = InMouseLock; }
	UBOOL GetMouseLock() const					{ return bMouseLock; }

	/** Gets the widget axis to be drawn */
	INT GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const;

	/** Mouse tracking interface.  Passes tracking messages to all active modes */
	UBOOL StartTracking();
	UBOOL EndTracking();

	/** Notifies all active modes that a map change has occured */
	void MapChangeNotify();

	/** Notifies all active modes to empty their selections */
	void SelectNone();

	/** Notifies all active modes of box selection attempts */
	UBOOL BoxSelect( FBox& InBox, UBOOL InSelect );

	/** Notifies all active modes of frustum selection attempts */
	UBOOL FrustumSelect( const FConvexVolume& InFrustum, UBOOL InSelect );

	/** TRUE if any active mode uses a transform widget */
	UBOOL UsesTransformWidget() const;

	/** TRUE if any active mode uses the passed in transform widget */
	UBOOL UsesTransformWidget( FWidget::EWidgetMode CheckMode ) const;

	/** Sets the current widget axis */
	void SetCurrentWidgetAxis( EAxis NewAxis );

	/** Notifies all active modes of mouse click messages. */
	UBOOL HandleClick( HHitProxy *HitProxy, const FViewportClick &Click );

	/** TRUE if the passed in brush actor should be drawn in wireframe */	
	UBOOL ShouldDrawBrushWireframe( AActor* InActor ) const;

	/** TRUE if brush vertices should be drawn */
	UBOOL ShouldDrawBrushVertices() const;

	/** Ticks all active modes */
	void Tick( FEditorLevelViewportClient* ViewportClient, FLOAT DeltaTime );

	/** Notifies all active modes of any change in mouse movement */
	UBOOL InputDelta( FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale );

	/** Notifies all active modes of captured mouse movement */	
	UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY );

	/** Notifies all active modes of keyboard input */
	UBOOL InputKey( FEditorLevelViewportClient* InViewportClient, FViewport* Viewport, FName Key, EInputEvent Event);

	/** Notifies all active modes of axis movement */
	UBOOL InputAxis( FEditorLevelViewportClient* InViewportClient, FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime);

	/** Notifies all active modes that the mouse has moved */
	UBOOL MouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* Viewport, INT X, INT Y );

	/** Draws all active mode components */	
	void DrawComponents( const FSceneView* InView, FPrimitiveDrawInterface* PDI );

	/** Renders all active modes */
	void Render( const FSceneView* InView, FViewport* Viewport, FPrimitiveDrawInterface* PDI );

	/** Draws the HUD for all active modes */
	void DrawHUD( FEditorLevelViewportClient* InViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas );

	/** Updates all active mode components */
	void UpdateComponents();

	/** Clears all active mode components */
	void ClearComponents();

	/** Calls PostUndo on all active components */
	void PostUndo();

	/** Called when an object is duplicated.  Sends the message to all active components */
	UBOOL HandleDuplicate();

	/** True if we should allow widget move */
	UBOOL AllowWidgetMove() const;

	/**
	 * Returns a good location to draw the widget at.
	 */
	FVector GetWidgetLocation() const;

	/**
	 * Changes the current widget mode.
	 */
	void SetWidgetMode( FWidget::EWidgetMode InWidgetMode );

	/**
	 * Allows you to temporarily override the widget mode.  Call this function again
	 * with WM_None to turn off the override.
	 */
	void SetWidgetModeOverride( FWidget::EWidgetMode InWidgetMode );

	/**
	 * Retrieves the current widget mode, taking overrides into account.
	 */
	FWidget::EWidgetMode GetWidgetMode() const;

	/**
	 * Sets editor wide usage of center zoom around cursor
	 */
	void SetCenterZoomAroundCursor(UBOOL bInCenterZoomAroundCursor) { bCenterZoomAroundCursor = bInCenterZoomAroundCursor; }
	/**
	 * Gets the current state of zooming around cursor
	 * @ return - If TRUE, the camera is zooming around the cursor position
	 */
	UBOOL GetCenterZoomAroundCursor (void) const { return bCenterZoomAroundCursor; }
	/**
	 * Sets editor wide usage of pan moving the canvas instead of the camera
	 */
	void SetPanMovesCanvas(UBOOL bInPanMovesCamera) { bPanMovesCanvas = bInPanMovesCamera; }
	/**
	 * Gets the current state of editor wide usage of pan moving the canvas instead of the camera.
	 * @ return - If TRUE, the camera is moving the canvas instead of the camera
	 */
	UBOOL GetPanMovesCanvas (void) const { return bPanMovesCanvas; }
	/**
	 * Sets editor wide usage of replace respecting scale
	 */
	void SetReplaceRespectsScale(UBOOL bInReplaceRespectsScale) { bReplaceRespectsScale = bInReplaceRespectsScale; }
	/**
	 * Gets the current state of editor wide usage of replace respecting scale
	 * @ return - If TRUE, replace respects scale
	 */
	UBOOL GetReplaceRespectsScale(void) const { return bReplaceRespectsScale; }

	/**
	 * Sets Interp Editor inversion of panning on drag
	 */
	void SetInterpPanInvert(UBOOL bInInterpPanInverted) { bInterpPanInverted = bInInterpPanInverted; }
	/**
	 * Gets the current state of the interp editor panning
	 * @ return - If TRUE, the interp editor left-right panning is inverted
	 */
	UBOOL GetInterpPanInvert(void) const { return bInterpPanInverted; }
	/**
	 * Sets highlighting selected objects with brackets
	 */
	void SetHighlightWithBrackets(UBOOL bInHighlightWithBrackets) { bHighlightWithBrackets = bInHighlightWithBrackets; }
	/**
	 * Gets the current state of clicking BSP surface selecting brush
	 * @ return - If TRUE, the clicking BSP surface selects brush
	 */
	UBOOL GetHighlightWithBrackets(void) const { return bHighlightWithBrackets; }
	/**
	 * Sets clicking BSP surface selecting brush
	 */
	void SetClickBSPSelectsBrush(UBOOL bInClickBSPSelectsBrush) { bClickBSPSelectsBrush = bInClickBSPSelectsBrush; }
	/**
	 * Gets the current state of clicking BSP surface selecting brush
	 * @ return - If TRUE, the clicking BSP surface selects brush
	 */
	UBOOL GetClickBSPSelectsBrush(void) const { return bClickBSPSelectsBrush; }
	/**
	 * Sets BSP auto updating
	 */
	void SetBSPAutoUpdate(UBOOL bInBSPAutoUpdate) { bBSPAutoUpdate = bInBSPAutoUpdate; }
	/**
	 * Gets the current state of BSP auto-updating
	 * @ return - If TRUE, BSP auto-updating is enabled
	 */
	UBOOL GetBSPAutoUpdate(void) const { return bBSPAutoUpdate; }

	/**
	 * Sets a bookmark in the levelinfo file, allocating it if necessary.
	 */
	void SetBookmark( UINT InIndex, FEditorLevelViewportClient* InViewportClient );

	/**
	 * Checks to see if a bookmark exists at a given index
	 */
	UBOOL CheckBookmark( UINT InIndex );

	/**
	 * Retrieves a bookmark from the list.
	 */
	void JumpToBookmark( UINT InIndex, UBOOL bShouldRestoreLevelVisibility );

	/**
	 * Serializes the components for all modes.
	 */
	void Serialize( FArchive &Ar );

	/**
	 * Loads the state that was saved in the INI file
	 */
	void LoadConfig(void);

	/**
	 * Saves the current state to the INI file
	 */
	void SaveConfig(void);

// FCallbackEventDevice interface

	void Send(ECallbackEventType InType);

	/**
	 * Handles notification of an object selection change. Updates the
	 * Pivot and Snapped location values based upon the selected actor
	 *
	 * @param InType the event that was fired
	 * @param InObject the object associated with this event
	 */
	void Send(ECallbackEventType InType,UObject* InObject);

	/** 
	 * Returns a list of active modes that are incompatible with the passed in mode.
	 * 
	 * @param InID 				The mode to check for incompatibilites.
	 * @param IncompatibleModes	The list of incompatible modes.
	 */
	void GetIncompatibleActiveModes( EEditorMode InID, TArray<FEdMode*>& IncompatibleModes );

	UBOOL PivotShown, Snapping;
	FVector PivotLocation, SnappedLocation, GridBase;

	//The angle for the translate rotate widget
	INT TranslateRotateXAxisAngle;

	//Delta Angle from start of rotation
	INT TotalDeltaRotation;

	//A place to store the rotation delta (magnitude)
	INT CurrentDeltaRotation;

	/** The coordinate system the widget is operating within. */
	ECoordSystem CoordSystem;		// The coordinate system the widget is operating within

	/** Draws in the top level corner of all FEditorLevelViewportClient windows (can be used to relay info to the user). */
	FString InfoString;

protected:
	/** A mapping of editor modes that can be active at the same time */
	TMultiMap<EEditorMode,EEditorMode> ModeCompatabilityMap;

	/** A list of all available editor modes. */
	TArray<FEdMode*> Modes;

	/** The editor modes currently in use. */
	TArray<FEdMode*> ActiveModes;

	/** The mode that the editor viewport widget is in. */
	FWidget::EWidgetMode WidgetMode;

	/** If the widget mode is being overridden, this will be != WM_None. */
	FWidget::EWidgetMode OverrideWidgetMode;

	/** Whether to use mouse position as direct widget position*/
	UBOOL bUseAbsoluteTranslation;

	/** Allow translate/rotate widget */
	UBOOL bAllowTranslateRotateZWidget;

	/** If 1, draw the widget and let the user interact with it. */
	UBOOL bShowWidget;

	/** If 1, the mouse is locked from moving actors but button presses are still accepted. */
	UBOOL bMouseLock;

	/** If TRUE, moves the canvas and shows the mouse.  If FALSE, uses original camera movement*/
	UBOOL bPanMovesCanvas;
	/** If TRUE, zooms centering on the mouse position.  If FALSE, the zoom is around the center of the viewport*/
	UBOOL bCenterZoomAroundCursor;
	/** If TRUE, panning in the Interp Editor is inverted. If FALSE, panning is normal*/
	UBOOL bInterpPanInverted;
	/** If TRUE, Replaces respects the scale of the original actor. If FALSE, Replace sets the scale to 1.0 */
	UBOOL bReplaceRespectsScale;
	/** If TRUE, Selected objects will be highlighted with brackets in all modes rather than a blue shade. */
	UBOOL bHighlightWithBrackets;
	/** If TRUE, Clicking a BSP selects the brush and ctrl+shift+click selects the surface. If FALSE, vice versa */
	UBOOL bClickBSPSelectsBrush;	
	/** If TRUE, BSP will auto-update */
	UBOOL bBSPAutoUpdate;

	// Widget flashing bits - used for when the widget is hidden but the widget mode is changed
	DOUBLE FlashWidgetStartTime;
	DOUBLE FlashWidgetRate;
	DOUBLE FlashWidgetTime;
	UBOOL bFlashWidget;

};