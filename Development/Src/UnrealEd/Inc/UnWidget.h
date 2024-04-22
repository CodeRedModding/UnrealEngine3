 /*=============================================================================
	UnWidget: Editor widgets for control like 3DS Max
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

class FWidget : public FSerializableObject
{
public:
	enum EWidgetMode
	{
		WM_None			= -1,
		WM_Translate,
		WM_Rotate,
		WM_Scale,
		WM_ScaleNonUniform,
		WM_TranslateRotateZ,
		WM_Max,
	};

	FWidget();

	/**
	 * Renders any widget specific HUD text
	 * @param Canvas - Canvas to use for 2d rendering
	 */
	void DrawHUD (FCanvas* Canvas);

	void Render( const FSceneView* View,FPrimitiveDrawInterface* PDI );

	/**
	 * Draws an arrow head line for a specific axis.
	 * @param	bCubeHead		[opt] If TRUE, render a cube at the axis tips.  If FALSE (the default), render a cone.
	 */
	void Render_Axis( const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxis InAxis, FMatrix& InMatrix, UMaterialInterface* InMaterial, FColor* InColor, FVector2D& OutAxisEnd, FLOAT InScale, UBOOL bDrawWidget, UBOOL bCubeHead=FALSE );

	/**
	 * Draws the translation widget.
	 */
	void Render_Translate( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget );

	/**
	 * Draws the rotation widget.
	 */
	void Render_Rotate( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget );

	/**
	 * Draws the scaling widget.
	 */
	void Render_Scale( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget );

	/**
	 * Draws the non-uniform scaling widget.
	 */
	void Render_ScaleNonUniform( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget );

	/**
	* Draws the Translate & Rotate Z widget.
	*/
	void Render_TranslateRotateZ( const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, UBOOL bDrawWidget );

	/**
	 * Converts mouse movement on the screen to widget axis movement/rotation.
	 */
	void ConvertMouseMovementToAxisMovement( FEditorLevelViewportClient* InViewportClient, const FVector& InLocation, const FVector& InDiff, FVector& InDrag, FRotator& InRotation, FVector& InScale );

	/**
	 * Absolute Translation conversion from mouse movement on the screen to widget axis movement/rotation.
	 */
	void AbsoluteTranslationConvertMouseMovementToAxisMovement(FSceneView* InView, FEditorLevelViewportClient* InViewportClient, const FVector& InLocation, const FVector2D& InMousePosition, FVector& OutDrag, FRotator& OutRotation, FVector& OutScale );

	/** 
	 * Grab the initial offset again first time input is captured
	 */
	void ResetInitialTranslationOffset (void)
	{
		bAbsoluteTranslationInitialOffsetCached = FALSE;
	}

	/** Only some modes support Absolute Translation Movement.  Check current mode */
	static UBOOL AllowsAbsoluteTranslationMovement(void);

	/**
	 * Sets the axis currently being moused over.  Typically called by FMouseDeltaTracker or FEditorLevelViewportClient.
	 *
	 * @param	InCurrentAxis	The new axis value.
	 */
	void SetCurrentAxis(EAxis InCurrentAxis)
	{
		CurrentAxis = InCurrentAxis;
	}

	/**
	 * @return	The axis currently being moused over.
	 */
	EAxis GetCurrentAxis() const
	{
		return CurrentAxis;
	}

	/**
	 * Returns whether we are actively dragging
	 */
	UBOOL IsDragging(void) const
	{
		return bDragging;
	}

	/**
	 * Sets if we are currently engaging the widget in dragging
	 */
	void SetDragging (const UBOOL InDragging) 
	{ 
		bDragging = InDragging;
	}

	/**
	 * Sets if we are currently engaging the widget in dragging
	 */
	void SetSnapEnabled (const UBOOL InSnapEnabled) 
	{ 
		bSnapEnabled = InSnapEnabled;
	}

	// FSerializableObject interface

	/** 
	 * Serializes the widget reference so they dont get garbage collected.
	 *
	 * @param Ar	FArchive to serialize with
	 */
	void Serialize(FArchive& Ar);

private:


	struct FAbsoluteMovementParams
	{
		/** The normal of the plane to project onto */
		FVector PlaneNormal;
		/** A vector that represent any displacement we want to mute (remove an axis if we're doing axis movement)*/
		FVector NormalToRemove;
		/** The current position of the widget */
		FVector Position;

		//Coordinate System Axes
		FVector XAxis;
		FVector YAxis;
		FVector ZAxis;

		//TRUE if camera movement is locked to the object
		FLOAT bMovementLockedToCamera;

		//Direction in world space to the current mouse location
		FVector PixelDir;
		//Direction in world space of the middle of the camera
		FVector CameraDir;
		FVector EyePos;

		//whether to snap the requested positionto the grid
		UBOOL bPositionSnapping;
	};

	struct FThickArcParams
	{
		FThickArcParams(FPrimitiveDrawInterface* InPDI, const FVector& InPosition, UMaterial* InMaterial, const FLOAT InInnerRadius, const FLOAT InOuterRadius)
			: PDI(InPDI)
			, Position(InPosition)
			, Material(InMaterial)
			, InnerRadius(InInnerRadius)
			, OuterRadius(InOuterRadius)
		{
		}

		/** The current position of the widget */
		FVector Position;

		//interface for Drawing
		FPrimitiveDrawInterface* PDI;

		//Material to use to render
		UMaterial* Material;

		//Radii
		FLOAT InnerRadius;
		FLOAT OuterRadius;
	};

	/**
	 * Returns the Delta from the current position that the absolute movement system wants the object to be at
	 * @param InParams - Structure containing all the information needed for absolute movement
	 * @return - The requested delta from the current position
	 */
	FVector GetAbsoluteTranslationDelta (const FAbsoluteMovementParams& InParams);
	/**
	 * Returns the offset from the initial selection point
	 */
	FVector GetAbsoluteTranslationInitialOffset(const FVector& InNewPosition, const FVector& InCurrentPosition);

	/**
	 * Returns TRUE if we're in Local Space editing mode or editing BSP (which uses the World axes anyway
	 */
	UBOOL IsRotationLocalSpace (void) const;

	/**
	 * Returns the "word" representation of how far we have just rotated
	 */
	INT GetDeltaRotation (void) const;

	/**
	 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
	 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
	 * @param View - Information about the scene/camera/etc
	 * @param PDI - Drawing interface
	 * @param InAxis - Enumeration of axis to rotate about
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InDirectionToWidget - Direction from camera to the widget
	 * @param InColor - The color associated with the axis of rotation
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void DrawRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxis InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FVector& InDirectionToWidget, const FColor& InColor, const FLOAT InScale);

	/**
	 * If actively dragging, draws a ring representing the potential rotation of the selected objects, snap ticks, and "delta" markers
	 * If not actively dragging, draws a quarter ring representing the closest quadrant to the camera
	 * @param View - Information about the scene/camera/etc
	 * @param PDI - Drawing interface
	 * @param InAxis - Enumeration of axis to rotate about
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InStartAngle - The starting angle about (Axis0^Axis1) to render the arc
	 * @param InEndAngle - The ending angle about (Axis0^Axis1) to render the arc
	 * @param InColor - The color associated with the axis of rotation
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void DrawPartialRotationArc(const FSceneView* View, FPrimitiveDrawInterface* PDI, EAxis InAxis, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FLOAT InStartAngle, const FLOAT InEndAngle, const FColor& InColor, const FLOAT InScale);

	/**
	 * Renders a portion of an arc for the rotation widget
	 * @param InParams - Material, Radii, etc
	 * @param InStartAxis - Start of the arc
	 * @param InEndAxis - End of the arc
	 * @param InColor - Color to use for the arc
	 */
	void DrawThickArc (const FThickArcParams& InParams, const FVector& Axis0, const FVector& Axis1, const FLOAT InStartAngle, const FLOAT InEndAngle, const FColor& InColor);

	/**
	 * Draws protractor like ticks where the rotation widget would snap too.
	 * Also, used to draw the wider axis tick marks
	 * @param PDI - Drawing interface
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
	 * @param InColor - The color to use for line/poly drawing
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 * @param InWidthPercent - The percent of the distance between the outer ring and inner ring to use for tangential thickness
	 * @param InPercentSize - The percent of the distance between the outer ring and inner ring to use for radial distance
	 */
	void DrawSnapMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FColor& InColor, const FLOAT InScale, const FLOAT InWidthPercent=0.0f, const FLOAT InPercentSize = 1.0f);

	/**
	 * Draw Start/Stop Marker to show delta rotations along the arc of rotation
	 * @param PDI - Drawing interface
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param InAngle - The Angle to rotate about the axis of rotation, the vector (Axis0 ^ Axis1)
	 * @param InColor - The color to use for line/poly drawing
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void 	DrawStartStopMarker(FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const INT InAngle, const FColor& InColor, const FLOAT InScale);

	/**
	 * Caches off HUD text to display after 3d rendering is complete
	 * @param View - Information about the scene/camera/etc
	 * @param PDI - Drawing interface
	 * @param InLocation - The Origin of the widget
	 * @param Axis0 - The Axis that describes a 0 degree rotation
	 * @param Axis1 - The Axis that describes a 90 degree rotation
	 * @param AngleOfAngle - angle we've rotated so far (in degrees)
	 * @param InScale - Multiplier to maintain a constant screen size for rendering the widget
	 */
	void CacheRotationHUDText(const FSceneView* View, FPrimitiveDrawInterface* PDI, const FVector& InLocation, const FVector& Axis0, const FVector& Axis1, const FLOAT AngleOfChange, const FLOAT InScale);

	/** The axis currently being moused over */
	EAxis CurrentAxis;

	/** Locations of the various points on the widget */
	FVector2D Origin, XAxisEnd, YAxisEnd, ZAxisEnd;

	enum
	{
		AXIS_ARROW_SEGMENTS = 6
	};

	/** Materials and colors to be used when drawing the items for each axis */
	UMaterial *AxisMaterialX, *AxisMaterialY, *AxisMaterialZ, *PlaneMaterialXY, *GridMaterial, *CurrentMaterial;
	FColor AxisColorX, AxisColorY, AxisColorZ, PlaneColorXY, CurrentColor;

	/**
	 * An extra matrix to apply to the widget before drawing it (allows for local/custom coordinate systems).
	 */
	FMatrix CustomCoordSystem;

	//location in the viewport to render the hud string
	FVector2D HUDInfoPos;
	//string to be displayed on top of the viewport
	FString HUDString;

	/** Whether Absolute Translation cache position has been captured */
	UBOOL bAbsoluteTranslationInitialOffsetCached;
	/** The initial offset where the widget was first clicked */
	FVector InitialTranslationOffset;
	/** The initial position of the widget before it was clicked */
	FVector InitialTranslationPosition;
	/** Whether or not the widget is actively dragging */
	UBOOL bDragging;
	/** Whether or not snapping is enabled for all actors */
	UBOOL bSnapEnabled;
};

/**
 * Widget hit proxy.
 */
struct HWidgetAxis : public HHitProxy
{
	DECLARE_HIT_PROXY(HWidgetAxis,HHitProxy);

	EAxis Axis;

	HWidgetAxis(EAxis InAxis):
		HHitProxy(HPP_UI),
		Axis(InAxis) {}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_SizeAll;
	}

	/**
	 * Method that specifies whether the hit proxy *always* allows translucent primitives to be associated with it or not,
	 * regardless of any other engine/editor setting. For example, if translucent selection was disabled, any hit proxies
	 *  returning TRUE would still allow translucent selection. In this specific case, TRUE is always returned because geometry
	 * mode hit proxies always need to be selectable or geometry mode will not function correctly.
	 *
	 * @return	TRUE if translucent primitives are always allowed with this hit proxy; FALSE otherwise
	 */
	virtual UBOOL AlwaysAllowsTranslucentPrimitives() const
	{
		return TRUE;
	}
};
