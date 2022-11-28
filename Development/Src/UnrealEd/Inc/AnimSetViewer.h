/*============================================================================
	AnimSetViewer.h: AnimSet viewer
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/


struct FASVViewportClient: public FEditorLevelViewportClient
{
	class WxAnimSetViewer*		AnimSetViewer;

	ULevel*						Level;
	UDirectionalLightComponent*	DirectionalLightComponent;
	USkyLightComponent*			SkyLightComponent;
	UEditorComponent*			EditorComponent;

	UBOOL						bShowBoneNames;
	UBOOL						bShowFloor;

	FASVViewportClient(WxAnimSetViewer* InASV);
	~FASVViewportClient();

	// FEditorLevelViewportClient interface

	virtual FScene* GetScene() { return Level; }
	virtual ULevel* GetLevel() { return Level; }
	virtual FColor GetBackgroundColor();
	virtual void Draw(FChildViewport* Viewport, FRenderInterface* RI);
	virtual void Tick(FLOAT DeltaSeconds);

	virtual void Serialize(FArchive& Ar);
};

/*-----------------------------------------------------------------------------
	WxASVPreview
-----------------------------------------------------------------------------*/

// wxWindows Holder for FCascadePreviewViewportClient
class WxASVPreview : public wxWindow
{
public:
	FASVViewportClient* ASVPreviewVC;

	WxASVPreview( wxWindow* InParent, wxWindowID InID, class WxAnimSetViewer* InASV );
	~WxASVPreview();

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WWxASVMenuBar
-----------------------------------------------------------------------------*/

class WxASVMenuBar : public wxMenuBar
{
public:
	WxASVMenuBar();
	~WxASVMenuBar();

	wxMenu*	FileMenu;
	wxMenu*	ViewMenu;
	wxMenu* MeshMenu;
	wxMenu* AnimSetMenu;
	wxMenu* AnimSeqMenu;
	wxMenu* NotifyMenu;
};

/*-----------------------------------------------------------------------------
	WWxASVToolBar
-----------------------------------------------------------------------------*/

class WxASVToolBar : public wxToolBar
{
public:
	WxASVToolBar( wxWindow* InParent, wxWindowID InID );
	~WxASVToolBar();

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxASVStatusBar
-----------------------------------------------------------------------------*/

class WxASVStatusBar : public wxStatusBar
{
public:
	WxASVStatusBar( wxWindow* InParent, wxWindowID InID);
	~WxASVStatusBar();

	void UpdateStatusBar( WxAnimSetViewer* AnimSetViewer );
};

/*-----------------------------------------------------------------------------
	WxAnimSetViewer
-----------------------------------------------------------------------------*/

#define ASV_SCRUBRANGE (10000)

class WxAnimSetViewer : public wxFrame, public FNotifyHook, public WxSerializableWindow
{
public:
	WxASVMenuBar*			MenuBar;
	WxASVToolBar*			ToolBar;
	WxASVStatusBar*			StatusBar;

	USkeletalMesh*			SelectedSkelMesh;
	class UAnimSet*			SelectedAnimSet;
	class UAnimSequence*	SelectedAnimSeq;


	FASVViewportClient*		ASVPreviewVC;

	AASVActor*					PreviewActor;
	USkeletalMeshComponent*		PreviewSkelComp;
	class UAnimNodeSequence*	PreviewAnimNode;

	ULevel*						PreviewLevel;
	APhATFloor*					PreviewFloor;


	// Various wxWindows controls
	WxBitmap				StopB, PlayB, LoopB, NoLoopB;

	wxComboBox*				SkelMeshCombo;
	wxComboBox*				AnimSetCombo;
	wxListBox*				AnimSeqList;
	wxNotebook*				PropNotebook;
	wxSlider*				TimeSlider;
	wxBitmapButton*			PlayButton;
	wxBitmapButton*			LoopButton;

	class WxPropertyWindow*	MeshProps;
	class WxPropertyWindow*	AnimSetProps;
	class WxPropertyWindow*	AnimSeqProps;


	WxAnimSetViewer(wxWindow* InParent, wxWindowID InID, USkeletalMesh* InSkelMesh, class UAnimSet* InSelectAnimSet = NULL);
	~WxAnimSetViewer();

	// WxSerializableWindow interface
	void Serialize(FArchive& Ar);

	// Menu handlers
	void OnSkelMeshComboChanged( wxCommandEvent& In );
	void OnAnimSetComboChanged( wxCommandEvent& In );
	void OnAnimSeqListChanged( wxCommandEvent& In );
	void OnImportPSA( wxCommandEvent& In );
	void OnNewAnimSet( wxCommandEvent& In );
	void OnTimeScrub( wxScrollEvent& In );
	void OnViewBones( wxCommandEvent& In );
	void OnViewBoneNames( wxCommandEvent& In );
	void OnViewFloor( wxCommandEvent& In );
	void OnViewWireframe( wxCommandEvent& In );
	void OnViewGrid( wxCommandEvent& In );
	void OnViewRefPose( wxCommandEvent& In );
	void OnLoopAnim( wxCommandEvent& In );
	void OnPlayAnim( wxCommandEvent& In );
	void OnEmptySet( wxCommandEvent& In );
	void OnRenameSequence( wxCommandEvent& In );
	void OnDeleteSequence( wxCommandEvent& In );
	void OnSequenceApplyRotation( wxCommandEvent& In );
	void OnSequenceReZero( wxCommandEvent& In );
	void OnSequenceCrop( wxCommandEvent& In );
	void OnNotifySort( wxCommandEvent& In );
	void OnCopySequenceName( wxCommandEvent& In );
	void OnCopyMeshBoneNames( wxCommandEvent& In );

	// Tools
	void SetSelectedSkelMesh(USkeletalMesh* InSkelMesh);
	void SetSelectedAnimSet(class UAnimSet* InAnimSet);
	void SetSelectedAnimSequence(class UAnimSequence* InAnimSeq);
	void ImportPSA();
	void CreateNewAnimSet();
	void UpdateAnimSeqList();
	void UpdateAnimSetCombo();
	void RefreshPlaybackUI();
	FLOAT GetCurrentSequenceLength();
	void TickViewer(FLOAT DeltaSeconds);
	void EmptySelectedSet();
	void RenameSelectedSeq();
	void DeleteSelectedSequence();
	void SequenceApplyRotation();
	void SequenceReZeroToCurrent();
	void SequenceCrop(UBOOL bFromStart=true);

	// Notification hook.
	virtual void NotifyDestroy( void* Src );
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	virtual void NotifyExec( void* Src, const TCHAR* Cmd );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgRotateAnimSeq
-----------------------------------------------------------------------------*/

class WxDlgRotateAnimSeq : public wxDialog
{
public:
	WxDlgRotateAnimSeq();
	~WxDlgRotateAnimSeq();

	FLOAT			Degrees;
	EAxis			Axis;

	wxTextCtrl		*DegreesEntry;
	wxComboBox		*AxisCombo;

	int ShowModal();
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};