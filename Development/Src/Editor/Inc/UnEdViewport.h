/*=============================================================================
	UnEdViewport.h: Unreal editor viewport definition.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

struct FEditorLevelViewportClient;

struct FViewportClick
{
	FVector		Origin,
				Direction;
	FName		Key;
	EInputEvent	Event;
	UBOOL		ControlDown,
				ShiftDown,
				AltDown;

	// Constructor.
	FViewportClick(struct FSceneView* View,FEditorLevelViewportClient* ViewportClient,FName InKey,EInputEvent InEvent,INT X,INT Y);
};

//
//	FEditorLevelViewportClient
//

struct FEditorLevelViewportClient: FViewportClient
{
	FChildViewport*			Viewport;
	FVector					ViewLocation;
	FRotator				ViewRotation;
	FLOAT					ViewFOV;
	INT						ViewportType;
	FLOAT					OrthoZoom;
	UEditorViewportInput*	Input;

	ESceneViewMode			ViewMode;
	DWORD					ShowFlags;
	UBOOL					Realtime,
							Redraw;

	UBOOL					bDrawAxes;

	UBOOL					bConstrainAspectRatio;
	FLOAT					AspectRatio;

	/** Whether this viewport is a realtime audio viewport */
	UBOOL					RealtimeAudio;

	UBOOL					bEnableFading;
	FLOAT					FadeAmount;
	FColor					FadeColor;

	class FWidget*			Widget;
	FMouseDeltaTracker		MouseDeltaTracker;

	// Constructor/ Destructor.

	FEditorLevelViewportClient();
	~FEditorLevelViewportClient();

	// CalcSceneView - Sets up the view and projection matrices for the viewport.

	virtual void CalcSceneView(FSceneView* View);

	// Editor utility functions

	inline UBOOL IsOrtho() { return (ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoXZ || ViewportType == LVT_OrthoYZ ); }
	inline UBOOL IsPerspective() { return (ViewportType == LVT_Perspective); }
	UBOOL ComponentIsTouchingSelectionBox( AActor* InActor, UPrimitiveComponent* InComponent, FBox InSelBBox );

	// FEditorLevelViewportClient interface.

	virtual ULevel* GetLevel();
	virtual FScene* GetScene();
	virtual void ProcessClick(FSceneView* View,HHitProxy* HitProxy,FName Key,EInputEvent Event,UINT HitX,UINT HitY);
	virtual void Tick(FLOAT DeltaSeconds);
	void UpdateMouseDelta();
	EAxis GetHorizAxis();
	EAxis GetVertAxis();

	// FViewportClient interface.

	virtual void RedrawRequested(FChildViewport* Viewport) { Redraw = 1; }

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime);
	virtual FVector TranslateDelta( FName InKey, FLOAT InDelta, UBOOL InNudge );
	void ConvertMovementToDragRot( const FVector& InDelta, FVector& InDragDelta, FRotator& InRotDelta );
	void MoveViewportCamera( const FVector& InDrag, const FRotator& InRot );
	void ApplyDeltaToActors( FVector& InDrag, FRotator& InRot, FVector& InScale );
	void ApplyDeltaToActor( AActor* InActor, FVector& InDeltaDrag, FRotator& InDeltaRot, FVector& InDeltaScale );
	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);
	virtual FColor GetBackgroundColor();
	virtual void MouseMove(FChildViewport* Viewport,INT x, INT y);

	virtual EMouseCursor GetCursor(FChildViewport* Viewport,INT X,INT Y);

	// FEditorLevelViewportClient interface
	void DrawKismetRefs(USequence* Seq, FChildViewport* Viewport, FSceneContext* Context, FRenderInterface* RI);
	void DrawAxes(FChildViewport* Viewport,FRenderInterface* RI, const FRotator* InRotation = NULL);

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FEditorLevelViewportClient& Viewport)
	{
		return Ar << Viewport.Input;
	}
};
