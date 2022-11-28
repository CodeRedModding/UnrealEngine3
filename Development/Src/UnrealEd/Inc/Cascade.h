/*=============================================================================
	Cascade.h: 'Cascade' particle editor
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"


struct HCascadeEmitterProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCascadeEmitterProxy,HHitProxy);

	class UParticleEmitter* Emitter;

	HCascadeEmitterProxy(class UParticleEmitter* InEmitter) :
		Emitter(InEmitter)
	{}
};

struct HCascadeModuleProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCascadeModuleProxy,HHitProxy);

	class UParticleEmitter* Emitter;
	class UParticleModule* Module;

	HCascadeModuleProxy(class UParticleEmitter* InEmitter, class UParticleModule* InModule) :
		Emitter(InEmitter),
		Module(InModule)
	{}
};

struct HCascadeDrawModeButtonProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCascadeDrawModeButtonProxy,HHitProxy);

	class UParticleEmitter* Emitter;
	INT DrawMode;

	HCascadeDrawModeButtonProxy(class UParticleEmitter* InEmitter, INT InDrawMode) :
		Emitter(InEmitter),
		DrawMode(InDrawMode)
	{}
};

struct HCascadeGraphButton : public HHitProxy
{
	DECLARE_HIT_PROXY(HCascadeGraphButton,HHitProxy);

	class UParticleEmitter* Emitter;
	class UParticleModule* Module;

	HCascadeGraphButton(class UParticleEmitter* InEmitter, class UParticleModule* InModule) :
		Emitter(InEmitter),
		Module(InModule)
	{}
};

struct HCascade3DDrawModeButtonProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCascade3DDrawModeButtonProxy,HHitProxy);

	class UParticleEmitter* Emitter;
	class UParticleModule* Module;

	HCascade3DDrawModeButtonProxy(class UParticleEmitter* InEmitter, class UParticleModule* InModule) :
		Emitter(InEmitter),
		Module(InModule)
	{}
};

struct HCascade3DDrawModeOptionsButtonProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HCascade3DDrawModeOptionsButtonProxy,HHitProxy);

	class UParticleEmitter* Emitter;
	class UParticleModule* Module;

	HCascade3DDrawModeOptionsButtonProxy(class UParticleEmitter* InEmitter, class UParticleModule* InModule) :
		Emitter(InEmitter),
		Module(InModule)
	{}
};

/*-----------------------------------------------------------------------------
	FCascadePreviewViewportClient
-----------------------------------------------------------------------------*/

class FCascadePreviewViewportClient : public FEditorLevelViewportClient
{
public:
	class WxCascade*			Cascade;

	FLOAT						TimeScale;

	ULevel*						Level;
	UDirectionalLightComponent*	DirectionalLightComponent;
	USkyLightComponent*			SkyLightComponent;

	FLOAT						SkyBrightness, Brightness;
	FColor						SkyColor, Color;
	FLOAT						LightDirection;
	FLOAT						LightElevation;

	FRotator					PreviewAngle;
	FLOAT						PreviewDistance;

	UBOOL						bDrawOriginAxes;
	UBOOL						bDrawParticleCounts;

	FCascadePreviewViewportClient( class WxCascade* InCascade );
	~FCascadePreviewViewportClient();

	// FEditorLevelViewportClient interface
	virtual FScene* GetScene() { return Level; }
	virtual ULevel* GetLevel() { return Level; }
	virtual FColor GetBackgroundColor();
	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void MouseMove(FChildViewport* Viewport, INT X, INT Y);
	virtual void InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime);

	virtual void Tick(FLOAT DeltaSeconds);

	virtual void Serialize(FArchive& Ar) { Ar << DirectionalLightComponent << SkyLightComponent << Input; }

	// FCascadePreviewViewportClient interface

	void SetPreviewCamera(const FRotator& PreviewAngle, FLOAT PreviewDistance);
	void UpdateLighting();
};

/*-----------------------------------------------------------------------------
	WxCascadePreview
-----------------------------------------------------------------------------*/

// wxWindows Holder for FCascadePreviewViewportClient
class WxCascadePreview : public wxWindow
{
public:
	FCascadePreviewViewportClient* CascadePreviewVC;


	WxCascadePreview( wxWindow* InParent, wxWindowID InID, class WxCascade* InCascade );
	~WxCascadePreview();

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};



/*-----------------------------------------------------------------------------
	FCascadeEmitterEdViewportClient
-----------------------------------------------------------------------------*/

enum ECascadeModMoveMode
{
	CMMM_None,
	CMMM_Move,
	CMMM_Instance
};

class FCascadeEmitterEdViewportClient : public FEditorLevelViewportClient
{
public:
	class WxCascade*	Cascade;

	ECascadeModMoveMode	CurrentMoveMode;
	FIntPoint			MouseHoldOffset; // Top-left corner of dragged module relative to mouse cursor.
	FIntPoint			MousePressPosition; // Location of cursor when mouse was pressed.
	UBOOL				bMouseDragging;
	UBOOL				bMouseDown;
	UBOOL				bPanning;
    UBOOL               bDrawModuleDump;

	FIntPoint			Origin2D;
	INT					OldMouseX, OldMouseY;

	// Currently dragged module.
	class UParticleModule*	DraggedModule;

	// If we abort a drag - here is where put the module back to (in the selected Emitter)
	INT						ResetDragModIndex;

	FCascadeEmitterEdViewportClient( class WxCascade* InCascade );
	~FCascadeEmitterEdViewportClient();

	virtual void Serialize(FArchive& Ar) { Ar << Input; }

	// FEditorLevelViewportClient interface
	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void MouseMove(FChildViewport* Viewport, INT X, INT Y);
	virtual void InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime);

	virtual ULevel* GetLevel() { return NULL; }

	// FCascadeEmitterEdViewportClient interface

	void FindDesiredModulePosition(const FIntPoint& Pos, class UParticleEmitter* &OutEmitter, INT &OutIndex);
	FIntPoint FindModuleTopLeft(class UParticleEmitter* Emitter, class UParticleModule* Module, FChildViewport* Viewport);

    void DrawEmitter(INT Index, INT XPos, UParticleEmitter* Emitter, FChildViewport* Viewport, FRenderInterface* RI);
	void DrawHeaderBlock(INT Index, INT XPos, UParticleEmitter* Emitter, FChildViewport* Viewport, FRenderInterface* RI);
	void DrawTypeDataBlock(INT XPos, UParticleEmitter* Emitter, FChildViewport* Viewport, FRenderInterface* RI);
    void DrawModule(INT XPos, INT YPos, UParticleEmitter* Emitter, UParticleModule* Module, FChildViewport* Viewport, FRenderInterface* RI);
	void DrawModule(FRenderInterface* RI, UParticleModule* Module, FColor ModuleBkgColor);
    void DrawDraggedModule(UParticleModule* Module, FChildViewport* Viewport, FRenderInterface* RI);
    void DrawCurveButton(UParticleEmitter* Emitter, UParticleModule* Module, UBOOL bHitTesting, FRenderInterface* RI);
    void Draw3DDrawButton(UParticleEmitter* Emitter, UParticleModule* Module, UBOOL bHitTesting, FRenderInterface* RI);

    void DrawModuleDump(FChildViewport* Viewport, FRenderInterface* RI);
};

/*-----------------------------------------------------------------------------
	WxCascadeEmitterEd
-----------------------------------------------------------------------------*/

// wxWindows Holder for FCascadePreviewViewportClient
class WxCascadeEmitterEd : public wxWindow
{
public:
	FCascadeEmitterEdViewportClient* EmitterEdVC;


	WxCascadeEmitterEd( wxWindow* InParent, wxWindowID InID, class WxCascade* InCascade );
	~WxCascadeEmitterEd();

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxCascadeMenuBar
-----------------------------------------------------------------------------*/

class WxCascadeMenuBar : public wxMenuBar
{
public:
	wxMenu	*EditMenu, *ViewMenu;

	WxCascadeMenuBar(WxCascade* Cascade);
	~WxCascadeMenuBar();
};

/*-----------------------------------------------------------------------------
	WxCascadeToolBar
-----------------------------------------------------------------------------*/

class WxCascadeToolBar : public wxToolBar
{
public:
	WxCascadeToolBar( wxWindow* InParent, wxWindowID InID );
	~WxCascadeToolBar();

	WxMaskedBitmap SaveCamB, ResetSystemB, OrbitModeB;
	WxMaskedBitmap PlayB, PauseB;
	WxMaskedBitmap Speed1B, Speed10B, Speed25B, Speed50B, Speed100B;
	WxMaskedBitmap LoopSystemB;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxCascadeModuleTreeLeafData
-----------------------------------------------------------------------------*/

class WxCascadeModuleTreeLeafData : public wxTreeItemData
{
public:
	WxCascadeModuleTreeLeafData()
	{
		check(0);
	}	// Wrong ctor

	WxCascadeModuleTreeLeafData(INT ModuleIndex)
	{
		Data = ModuleIndex;
	}
	
	~WxCascadeModuleTreeLeafData()
	{
	}

	INT Data;
};

/*-----------------------------------------------------------------------------
	WxCascade
-----------------------------------------------------------------------------*/

class WxCascade : public wxFrame, public FNotifyHook, public WxSerializableWindow
{
public:
	WxCascadeMenuBar* MenuBar;
	WxCascadeToolBar* ToolBar;

	wxSplitterWindow* TopSplitterWnd; // Top level splitter that splits window into left and right sections
	wxSplitterWindow* PreviewSplitterWnd; // Left-hand splitter that separates property editor from preview 
	wxSplitterWindow* EditorSplitterWnd; // Right-hand splitter that separates emitter editor from graph editor

	WxPropertyWindow*					PropertyWindow;
	FCascadePreviewViewportClient*		PreviewVC;
	FCascadeEmitterEdViewportClient*	EmitterEdVC;
	WxCurveEditor*						CurveEd;

	wxSplitterWindow*					TreeSplitterWnd;
	wxTreeCtrl*							ModuleTree;
	wxImageList*						ModuleTreeImages;
	INT									TreeSelectedModuleIndex;

	FLOAT								SimSpeed;

	class UParticleSystem*	PartSys;

	// Resources for previewing system
	class ULevel*				PreviewLevel;
	class ACascadePreviewActor*	PreviewEmitter;

	class UParticleModule*		SelectedModule;
	class UParticleEmitter*		SelectedEmitter;

	class UParticleModule*		CopyModule;
	class UParticleEmitter*		CopyEmitter;

	TArray<class UParticleModule*>	ModuleDumpList;

	// Whether to reset the simulation once it has completed a run and all particles have died.
	UBOOL					bResetOnFinish;
	UBOOL					bPendingReset;
	DOUBLE					ResetTime;
	UBOOL					bOrbitMode;

	UObject*				CurveToReplace;

	UCascadeOptions*		EditorOptions;

	// Static list of all ParticleModule subclasses.
	static TArray<UClass*>	ParticleModuleBaseClasses;

	static TArray<UClass*>	ParticleModuleClasses;
	static UBOOL			bParticleModuleClassesInitialized;

	// Static list of all ParticleEmitter subclasses.
	static UBOOL			bParticleEmitterClassesInitialized;
	static TArray<UClass*>	ParticleEmitterClasses;

	WxCascade( wxWindow* InParent, wxWindowID InID, class UParticleSystem* InPartSys  );
	~WxCascade();

	// wxFrame interface
	virtual wxToolBar* OnCreateToolBar(long style, wxWindowID id, const wxString& name);

	// WxSerializableWindow interface
	void Serialize(FArchive& Ar);

	// Menu callbacks
	void OnSize( wxSizeEvent& In );

	void OnRenameEmitter(wxCommandEvent& In);

	void OnNewEmitter( wxCommandEvent& In );
	void OnNewModule( wxCommandEvent& In );

	void OnDuplicateEmitter(wxCommandEvent& In);
	void OnDeleteEmitter( wxCommandEvent& In );
	void OnAddSelectedModule(wxCommandEvent& In);
	void OnCopyModule(wxCommandEvent& In);
	void OnPasteModule(wxCommandEvent& In);
	void OnDeleteModule( wxCommandEvent& In );

	void OnMenuSimSpeed( wxCommandEvent& In );
	void OnSaveCam( wxCommandEvent& In );
	void OnResetSystem( wxCommandEvent& In );
	void OnResetPeakCounts(wxCommandEvent& In);
	void OnOrbitMode(wxCommandEvent& In);
	void OnViewAxes( wxCommandEvent& In );
	void OnViewCounts(wxCommandEvent& In);
	void OnLoopSimulation( wxCommandEvent& In );

	void OnViewModuleDump(wxCommandEvent& In);

	void OnPlay(wxCommandEvent& In);
	void OnPause(wxCommandEvent& In);
	void OnSpeed(wxCommandEvent& In);
	void OnLoopSystem(wxCommandEvent& In);

	// Utils
	void CreateNewModule(INT ModClassIndex);

	void SetSelectedEmitter( UParticleEmitter* NewSelectedEmitter );
	void SetSelectedModule( UParticleEmitter* NewSelectedEmitter, UParticleModule* NewSelectedModule );

	void DeleteSelectedEmitter();
	void MoveSelectedEmitter(INT MoveAmount);

	void DeleteSelectedModule();
	void InsertModule(UParticleModule* Module, UParticleEmitter* TargetEmitter, INT TargetIndex);

	UBOOL ModuleIsShared(UParticleModule* Module);

	void AddSelectedToGraph();

	void SetCopyEmitter(UParticleEmitter* NewEmitter);
	void SetCopyModule(UParticleEmitter* NewEmitter, UParticleModule* NewModule);

	void RemoveModuleFromDump(UParticleModule* Module);

	// Tree control interface
	INT GetTreeSelectedModule();
	void RebuildTreeControl();
	void OnTreeItemSelected(wxTreeEvent& In);
	void OnTreeItemActivated(wxTreeEvent& In);
	void OnTreeExpanding(wxTreeEvent& In);
	void OnTreeCollapsed(wxTreeEvent& In);

	// FNotify interface
	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	static void InitParticleModuleClasses();
	static void InitParticleEmitterClasses();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxMBCascadeModule
-----------------------------------------------------------------------------*/

class WxMBCascadeModule : public wxMenu
{
public:
	WxMBCascadeModule(WxCascade* Cascade);
	~WxMBCascadeModule();
};

/*-----------------------------------------------------------------------------
	WxMBCascadeEmitterBkg
-----------------------------------------------------------------------------*/

class WxMBCascadeEmitterBkg : public wxMenu
{
public:
	enum Mode
	{
		EMITTER_ONLY	= 0x0001,
		TYPEDATAS_ONLY	= 0x0002,
		SPAWNS_ONLY		= 0x0004,
		UPDATES_ONLY	= 0x0008,
		NON_TYPEDATAS	= SPAWNS_ONLY | UPDATES_ONLY,
		EVERYTHING		= EMITTER_ONLY | TYPEDATAS_ONLY | SPAWNS_ONLY | UPDATES_ONLY
	};

	WxMBCascadeEmitterBkg(WxCascade* Cascade, Mode eMode);
	~WxMBCascadeEmitterBkg();

protected:
	static UBOOL			InitializedModuleEntries;
	static TArray<FString>	TypeDataModuleEntries;
	static TArray<INT>		TypeDataModuleIndices;
	static TArray<FString>	ModuleEntries;
	static TArray<INT>		ModuleIndices;
};

/*-----------------------------------------------------------------------------
	WxMBCascadeBkg
-----------------------------------------------------------------------------*/

class WxMBCascadeBkg : public wxMenu
{
public:
	WxMBCascadeBkg(WxCascade* Cascade);
	~WxMBCascadeBkg();
};