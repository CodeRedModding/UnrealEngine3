/*=============================================================================
	UnEdModeInterpolation : Editor mode for setting up interpolation sequences.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

//////////////////////////////////////////////////////////////////////////
// FEdModeInterpEdit
//////////////////////////////////////////////////////////////////////////

class FEdModeInterpEdit : public FEdMode
{
public:
	FEdModeInterpEdit();
	~FEdModeInterpEdit();

	virtual UBOOL InputKey( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, FName Key, EInputEvent Event );
	virtual void Enter();
	virtual void Exit();
	virtual void ActorMoveNotify();
	virtual void ActorSelectionChangeNotify();
	virtual void ActorPropChangeNotify();
	virtual UBOOL AllowWidgetMove();

	virtual void Render(const FSceneView* View,FViewport* Viewport,FPrimitiveDrawInterface* PDI);
	virtual void DrawHUD(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,const FSceneView* View,FCanvas* Canvas);

	void CamMoveNotify(FEditorLevelViewportClient* ViewportClient);
	void InitInterpMode(class USeqAct_Interp* EditInterp);
	class WxCameraAnimEd* InitCameraAnimMode(class USeqAct_Interp* EditInterp);

	class USeqAct_Interp*	Interp;
	class WxInterpEd*		InterpEd;

	UBOOL					bLeavingMode;
private:
	// Grouping is always disabled while in InterpEdit Mode, re-enable the saved value on exit
	UBOOL					bGroupingActiveSaved;

};

//////////////////////////////////////////////////////////////////////////
// FModeTool_InterpEdit
//////////////////////////////////////////////////////////////////////////

class FModeTool_InterpEdit : public FModeTool
{
public:
	FModeTool_InterpEdit();
	~FModeTool_InterpEdit();

	virtual FString GetName() const		{ return TEXT("Interp Edit"); }

	/**
	 * @return		TRUE if the key was handled by this editor mode tool.
	 */
	virtual UBOOL InputKey(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,FName Key,EInputEvent Event);
	virtual UBOOL MouseMove(FEditorLevelViewportClient* ViewportClient,FViewport* Viewport,INT x, INT y);

	/**
	 * @return		TRUE if the delta was handled by this editor mode tool.
	 */
	virtual UBOOL InputDelta(FEditorLevelViewportClient* InViewportClient,FViewport* InViewport,FVector& InDrag,FRotator& InRot,FVector& InScale);
	virtual void SelectNone();


	UBOOL bMovingHandle;
	UInterpGroup* DragGroup;
	INT DragTrackIndex;
	INT DragKeyIndex;
	UBOOL bDragArriving;
};
