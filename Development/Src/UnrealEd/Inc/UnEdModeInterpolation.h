/*=============================================================================
	UnEdModeInterpolation : Editor mode for setting up interpolation sequences.

	Revision history:
		* Created by James Golding
=============================================================================*/

//////////////////////////////////////////////////////////////////////////
// FEdModeInterpEdit
//////////////////////////////////////////////////////////////////////////

class FEdModeInterpEdit : public FEdMode
{
public:
	FEdModeInterpEdit();
	~FEdModeInterpEdit();

	virtual void Enter();
	virtual void Exit();
	virtual void ActorMoveNotify();
	virtual void CamMoveNotify(FEditorLevelViewportClient* ViewportClient);
	virtual void ActorSelectionChangeNotify();
	virtual void ActorPropChangeNotify();
	virtual UBOOL AllowWidgetMove();

	virtual void RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,const FSceneContext& Context,FRenderInterface* RI);

	void InitInterpMode(class USeqAct_Interp* EditInterp);

	class USeqAct_Interp*	Interp;
	class WxInterpEd*		InterpEd;

	UBOOL					bLeavingMode;
};

//////////////////////////////////////////////////////////////////////////
// FModeTool_InterpEdit
//////////////////////////////////////////////////////////////////////////

class FModeTool_InterpEdit : public FModeTool
{
public:
	FModeTool_InterpEdit();
	~FModeTool_InterpEdit();

	virtual FString GetName()	{ return TEXT("Interp Edit"); }

	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,INT x, INT y);
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FChildViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual void SelectNone();


	UBOOL bMovingHandle;
	UInterpGroup* DragGroup;
	INT DragTrackIndex;
	INT DragKeyIndex;
	UBOOL bDragArriving;
};

//////////////////////////////////////////////////////////////////////////
// WxModeBarInterpEdit
//////////////////////////////////////////////////////////////////////////

class WxModeBarInterpEdit : public WxModeBar
{
public:
	WxModeBarInterpEdit( wxWindow* InParent, wxWindowID InID );
	~WxModeBarInterpEdit();

	DECLARE_EVENT_TABLE()
};