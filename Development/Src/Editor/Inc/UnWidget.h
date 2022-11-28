 /*=============================================================================
	UnWidget: Editor widgets for control like 3DS Max

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

/*------------------------------------------------------------------------------
    FWidget.
------------------------------------------------------------------------------*/

#define AXIS_ARROW_SEGMENTS		6
#define AXIS_ARROW_RADIUS		8
#define AXIS_CIRCLE_RADIUS		48.0f
#define AXIS_CIRCLE_SIDES		24

enum EWidgetMode
{
	WM_None			= -1,
	WM_Translate,
	WM_Rotate,
	WM_Scale,
	WM_ScaleNonUniform,
	WM_Max,
};

class FWidget
{
public:
	FWidget();
	~FWidget();

	void Render_Axis( const FSceneContext& Context, FPrimitiveRenderInterface* PRI, FTriangleRenderInterface* TRI, EAxis InAxis, FMatrix& InMatrix, UMaterialInstance* InMaterial, FColor* InColor, FVector& InAxisEnd, float InScale );
	void RenderForeground( const FSceneContext& Context,FPrimitiveRenderInterface* PRI );
	void Render_Translate( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation );
	void Render_Rotate( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation );
	void Render_Scale( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation );
	void Render_ScaleNonUniform( const FSceneContext& Context,FPrimitiveRenderInterface* PRI, const FVector& InLocation );
	void ConvertMouseMovementToAxisMovement( FEditorLevelViewportClient* InViewportClient, const FVector& InLocation, const FVector& InDiff, FVector& InDrag, FRotator& InRotation, FVector& InScale );

	/** The axis currently being moused over */
	EAxis CurrentAxis;

	/** Locations of the various points on the widget */
	FVector Origin, XAxisEnd, YAxisEnd, ZAxisEnd;

	/** Precomputed verts to draw the axis arrow heads with */
	FRawTriangleVertex rootvtx;
	FRawTriangleVertex verts[AXIS_ARROW_SEGMENTS+1];

	/** Materials and colors to be used when drawing the items for each axis */
	UMaterial *AxisMaterialX, *AxisMaterialY, *AxisMaterialZ, *CurrentMaterial;
	FColor AxisColorX, AxisColorY, AxisColorZ, CurrentColor;

	FMatrix CustomCoordSystem;			// An extra matrix to apply to the widget before drawing it (allows for local/custom coordinate systems)
};

/*------------------------------------------------------------------------------
    HWidgetAxis.
------------------------------------------------------------------------------*/

struct HWidgetAxis : public HHitProxy
{
	DECLARE_HIT_PROXY(HWidgetAxis,HHitProxy);

	EAxis Axis;

	HWidgetAxis(EAxis InAxis):
		Axis(InAxis) {}

	virtual EMouseCursor GetMouseCursor()
	{
		return MC_SizeAll;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
