/*=============================================================================
	InterpEditor.h: Interpolation editing
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	Editor-specific hit proxies.
-----------------------------------------------------------------------------*/

struct HInterpEdTrackBkg : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackBkg,HHitProxy);
	HInterpEdTrackBkg() {}
};

struct HInterpEdGroupTitle : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdGroupTitle,HHitProxy);

	class UInterpGroup* Group;

	HInterpEdGroupTitle(class UInterpGroup* InGroup) :
		Group(InGroup)
	{}
};

struct HInterpEdGroupCollapseBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdGroupCollapseBtn,HHitProxy);

	class UInterpGroup* Group;

	HInterpEdGroupCollapseBtn(class UInterpGroup* InGroup) :
		Group(InGroup)
	{}
};

struct HInterpEdGroupLockCamBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdGroupLockCamBtn,HHitProxy);

	class UInterpGroup* Group;

	HInterpEdGroupLockCamBtn(class UInterpGroup* InGroup) :
		Group(InGroup)
	{}
};

struct HInterpEdTrackTitle : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackTitle,HHitProxy);

	class UInterpGroup* Group;
	INT TrackIndex;

	HInterpEdTrackTitle(class UInterpGroup* InGroup, INT InTrackIndex) :
		Group(InGroup),
		TrackIndex(InTrackIndex)
	{}
};

struct HInterpEdTrackGraphPropBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTrackGraphPropBtn,HHitProxy);

	class UInterpGroup* Group;
	INT TrackIndex;

	HInterpEdTrackGraphPropBtn(class UInterpGroup* InGroup, INT InTrackIndex) :
		Group(InGroup),
		TrackIndex(InTrackIndex)
	{}
};


enum EInterpEdEventDirection
{
	IED_Forward,
	IED_Backward
};

struct HInterpEdEventDirBtn : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdEventDirBtn,HHitProxy);

	class UInterpGroup* Group;
	INT TrackIndex;
	EInterpEdEventDirection Dir;

	HInterpEdEventDirBtn(class UInterpGroup* InGroup, INT InTrackIndex, EInterpEdEventDirection InDir) :
		Group(InGroup),
		TrackIndex(InTrackIndex),
		Dir(InDir)
	{}
};

struct HInterpEdTimelineBkg : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdTimelineBkg,HHitProxy);
	HInterpEdTimelineBkg() {}
};

struct HInterpEdNavigator : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdNavigator,HHitProxy);
	HInterpEdNavigator() {}
};

enum EInterpEdMarkerType
{
	ISM_SeqStart,
	ISM_SeqEnd,
	ISM_LoopStart,
	ISM_LoopEnd
};

struct HInterpEdMarker : public HHitProxy
{
	DECLARE_HIT_PROXY(HInterpEdMarker,HHitProxy);

	EInterpEdMarkerType Type;

	HInterpEdMarker(EInterpEdMarkerType InType) :
		Type(InType)
	{}
};


/*-----------------------------------------------------------------------------
	UInterpEdTransBuffer / FInterpEdTransaction
-----------------------------------------------------------------------------*/

class UInterpEdTransBuffer : public UTransBuffer
{
	DECLARE_CLASS(UInterpEdTransBuffer,UTransBuffer,CLASS_Transient,UnrealEd)
	NO_DEFAULT_CONSTRUCTOR(UInterpEdTransBuffer)
public:

	UInterpEdTransBuffer( SIZE_T InMaxMemory )
	:	UTransBuffer( InMaxMemory )
	{}

	virtual void Begin( const TCHAR* SessionName ) {}
	virtual void End() {}

	virtual void BeginSpecial( const TCHAR* SessionName );
	virtual void EndSpecial();
};

class FInterpEdTransaction : public FTransaction
{
public:
	FInterpEdTransaction( const TCHAR* InTitle=NULL, UBOOL InFlip=0 )
	:	FTransaction(InTitle, InFlip)
	{}

	virtual void SaveObject( UObject* Object );
	virtual void SaveArray( UObject* Object, FArray* Array, INT Index, INT Count, INT Oper, INT ElementSize, STRUCT_AR Serializer, STRUCT_DTOR Destructor );
};

/*-----------------------------------------------------------------------------
	FInterpEdViewportClient
-----------------------------------------------------------------------------*/

class FInterpEdViewportClient : public FEditorLevelViewportClient
{
public:
	FInterpEdViewportClient( class WxInterpEd* InInterpEd );
	~FInterpEdViewportClient();

	void DrawTimeline(FChildViewport* Viewport,FRenderInterface* RI);
	void DrawMarkers(FChildViewport* Viewport,FRenderInterface* RI);
	void DrawGrid(FChildViewport* Viewport,FRenderInterface* RI);

	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void MouseMove(FChildViewport* Viewport, INT X, INT Y);
	virtual void InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime);
	virtual EMouseCursor GetCursor(FChildViewport* Viewport,INT X,INT Y);

	virtual void Tick(FLOAT DeltaSeconds);

	virtual ULevel* GetLevel() { return NULL; }

	virtual void Serialize(FArchive& Ar) { Ar << Input; }

	INT OldMouseX, OldMouseY;
	INT BoxStartX, BoxStartY;
	INT BoxEndX, BoxEndY;
	INT DistanceDragged;

	class WxInterpEd* InterpEd;

	EInterpEdMarkerType	GrabbedMarkerType;

	FLOAT	ViewStartTime;
	FLOAT	ViewEndTime;

	// Used to convert between seconds and size on the timeline
	INT		TrackViewSizeX;
	FLOAT	PixelsPerSec;
	FLOAT	NavPixelsPerSecond;

	FLOAT	UnsnappedMarkerPos;

	UBOOL	bPanning;
	UBOOL   bMouseDown;
	UBOOL	bGrabbingHandle;
	UBOOL	bNavigating;
	UBOOL	bBoxSelecting;
	UBOOL	bTransactionBegun;
	UBOOL	bGrabbingMarker;
};

/*-----------------------------------------------------------------------------
	WxInterpEdVCHolder
-----------------------------------------------------------------------------*/

class WxInterpEdVCHolder : public wxWindow
{
public:
	WxInterpEdVCHolder( wxWindow* InParent, wxWindowID InID, class WxInterpEd* InInterpEd );
	~WxInterpEdVCHolder();

	void OnSize( wxSizeEvent& In );

	FInterpEdViewportClient* InterpEdVC;

	DECLARE_EVENT_TABLE()
};


/*-----------------------------------------------------------------------------
	WxInterpEdToolBar
-----------------------------------------------------------------------------*/

class WxInterpEdToolBar : public wxToolBar
{
public:
	WxMaskedBitmap AddB, PlayB, LoopSectionB, StopB, UndoB, RedoB, CurveEdB, SnapB, FitSequenceB, FitLoopB;
	WxMaskedBitmap Speed1B, Speed10B, Speed25B, Speed50B, Speed100B;
	wxComboBox* SnapCombo;

	WxInterpEdToolBar( wxWindow* InParent, wxWindowID InID );
	~WxInterpEdToolBar();
};

/*-----------------------------------------------------------------------------
	WxInterpEdMenuBar
-----------------------------------------------------------------------------*/

class WxInterpEdMenuBar : public wxMenuBar
{
public:
	wxMenu	*EditMenu;

	WxInterpEdMenuBar();
	~WxInterpEdMenuBar();
};

/*-----------------------------------------------------------------------------
	WxInterpEd
-----------------------------------------------------------------------------*/

static const FLOAT InterpEdSnapSizes[4] = {0.01f, 0.05f, 0.1f, 0.5f};

class WxInterpEd : public wxFrame, public FNotifyHook, public WxSerializableWindow, public FCurveEdNotifyInterface
{
public:
	WxInterpEd( wxWindow* InParent, wxWindowID InID, class USeqAct_Interp* InInterp  );
	~WxInterpEd();

	void OnSize( wxSizeEvent& In );
	void OnClose( wxCloseEvent& In );

	// FNotify interface

	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// FCurveEdNotifyInterface
	virtual void PreEditCurve(TArray<UObject*> CurvesAboutToChange);
	virtual void PostEditCurve();
	virtual void MovedKey();
	virtual void DesireUndo();
	virtual void DesireRedo();

	// WxSerializableWindow
	void Serialize(FArchive& Ar);

	// Menu handlers
	void OnMenuAddKey( wxCommandEvent& In );
	void OnMenuPlay( wxCommandEvent& In );
	void OnMenuStop( wxCommandEvent& In );
	void OnChangePlaySpeed( wxCommandEvent& In );
	void OnMenuInsertSpace( wxCommandEvent& In );
	void OnMenuStretchSection( wxCommandEvent& In );
	void OnMenuDeleteSection( wxCommandEvent& In );
	void OnMenuSelectInSection( wxCommandEvent& In );
	void OnMenuDuplicateSelectedKeys( wxCommandEvent& In );
	void OnSavePathTime( wxCommandEvent& In );
	void OnJumpToPathTime( wxCommandEvent& In );
	void OnToggleCurveEd( wxCommandEvent& In );
	void OnGraphSplitChangePos( wxSplitterEvent& In );
	void OnToggleSnap( wxCommandEvent& In );
	void OnChangeSnapSize( wxCommandEvent& In );
	void OnViewFitSequence( wxCommandEvent& In );
	void OnViewFitLoop( wxCommandEvent& In );

	void OnContextNewTrack( wxCommandEvent& In );
	void OnContextNewGroup( wxCommandEvent& In );
	void OnContextTrackRename( wxCommandEvent& In );
	void OnContextTrackDelete( wxCommandEvent& In );
	void OnContextTrackChangeFrame( wxCommandEvent& In );
	void OnContextGroupRename( wxCommandEvent& In );
	void OnContextGroupDelete( wxCommandEvent& In );
	void OnContextGroupSetParent( wxCommandEvent& In );
	void OnContextKeyInterpMode( wxCommandEvent& In );
	void OnContextRenameEventKey( wxCommandEvent& In );

	void OnMenuUndo( wxCommandEvent& In );
	void OnMenuRedo( wxCommandEvent& In );

	// Selection
	void SetActiveTrack(class UInterpGroup* InGroup, INT InTrackIndex);

	UBOOL KeyIsInSelection(class UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex);
	void AddKeyToSelection(class UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex, UBOOL bAutoWind);
	void RemoveKeyFromSelection(class UInterpGroup* InGroup, INT InTrackIndex, INT InKeyIndex);
	void ClearKeySelection();
	void CalcSelectedKeyRange(FLOAT& OutStartTime, FLOAT& OutEndTime);
	void SelectKeysInLoopSection();

	// Utils
	void DeleteSelectedKeys(UBOOL bDoTransaction=false);
	void DuplicateSelectedKeys();
	void BeginMoveSelectedKeys();
	void EndMoveSelectedKeys();
	void MoveSelectedKeys(FLOAT DeltaTime);
	void AddKey();

	void UpdateMatineeActionConnectors();

	void LockCamToGroup(class UInterpGroup* InGroup);
	class AActor* GetViewedActor();
	void UpdateCameraToGroup();
	void UpdateCamColours();

	void SyncCurveEdView();
	void AddTrackToCurveEd(class UInterpGroup* InGroup, INT InTrackIndex);

	void SetInterpPosition(FLOAT NewPosition);
	void SelectActiveGroupParent();

	void SelectNextKey();
	void SelectPreviousKey();
	void ZoomView(FLOAT ZoomAmount);
	FLOAT SnapTime(FLOAT InTime);

	void BeginMoveMarker();
	void EndMoveMarker();
	void SetInterpEnd(FLOAT NewInterpLength);
	void MoveLoopMarker(FLOAT NewMarkerPos, UBOOL bIsStart);

	void BeginDrag3DHandle(UInterpGroup* Group, INT TrackIndex);
	void Move3DHandle(UInterpGroup* Group, INT TrackIndex, INT KeyIndex, UBOOL bArriving, const FVector& Delta);
	void EndDrag3DHandle();

	void ActorModified();
	void ActorSelectionChange();
	void CamMoved(const FVector& NewCamLocation, const FRotator& NewCamRotation);
	UBOOL ProcessKeyPress(FName Key, UBOOL bCtrlDown);

	void InterpEdUndo();
	void InterpEdRedo();

	void DrawTracks3D(const FSceneContext& Context, FPrimitiveRenderInterface* PRI);
	void DrawModeHUD(FEditorLevelViewportClient* ViewportClient,FChildViewport* Viewport,const FSceneContext& Context,FRenderInterface* RI);

	void TickInterp(FLOAT DeltaSeconds);

	static void InitInterpTrackClasses();

	WxInterpEdToolBar* ToolBar;
	WxInterpEdMenuBar* MenuBar;

	wxSplitterWindow* TopSplitterWnd; // Divides property window from the rest
	wxSplitterWindow* GraphSplitterWnd; // Divides the graph from the track view.
	INT GraphSplitPos;

	WxPropertyWindow* PropertyWindow;
	FInterpEdViewportClient* InterpEdVC;
	WxCurveEditor* CurveEd;
	WxInterpEdVCHolder* TrackWindow;

	UTexture2D*	BarGradText;
	FColor PosMarkerColor;
	FColor RegionFillColor;
	FColor RegionBorderColor;

	class USeqAct_Interp* Interp;
	class UInterpData* IData;

	// Only 1 track can be 'Active' at a time. This will be used for new keys etc.
	// You may have a Group selected but no Tracks (eg. empty group)
	class UInterpGroup* ActiveGroup;
	INT ActiveTrackIndex;

	// If we are connecting the camera to a particular group, this is it. If not, its NULL;
	class UInterpGroup* CamViewGroup;

	// Editor-specific Object, containing preferences and selection set to be serialised/undone.
	UInterpEdOptions* Opt;

	// Are we currently editing the value of a keyframe. This should only be true if there is one keyframe selected and the time is currently set to it.
	UBOOL bAdjustingKeyframe;

	// If we are looping 
	UBOOL bLoopingSection;

	// Currently moving a curve handle in the 3D viewport.
	UBOOL bDragging3DHandle;

	// Multiplier for preview playback of sequence
	FLOAT PlaybackSpeed;

	// Snap settings.
	UBOOL bSnapEnabled;
	FLOAT SnapAmount;

	UTransactor* NormalTransactor;
	UInterpEdTransBuffer* InterpEdTrans;

	// Static list of all InterpTrack subclasses.
	static TArray<UClass*>	InterpTrackClasses;
	static UBOOL			bInterpTrackClassesInitialized;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdGroupMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdGroupMenu : public wxMenu
{
public:
	WxMBInterpEdGroupMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdGroupMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdTrackMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdTrackMenu : public wxMenu
{
public:
	WxMBInterpEdTrackMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdTrackMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdBkgMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdBkgMenu : public wxMenu
{
public:
	WxMBInterpEdBkgMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdBkgMenu();
};

/*-----------------------------------------------------------------------------
	WxMBInterpEdKeyMenu
-----------------------------------------------------------------------------*/

class WxMBInterpEdKeyMenu : public wxMenu
{
public:
	WxMBInterpEdKeyMenu(WxInterpEd* InterpEd);
	~WxMBInterpEdKeyMenu();
};
