/*============================================================================
	AnimSetViewer.h: AnimSet viewer
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ANIMSETVIEWER_H__
#define __ANIMSETVIEWER_H__

#include "SocketManager.h"
#include "TrackableWindow.h"
#include "AnimationUtils.h"

#include "NvApexManager.h" 

// Forward declarations.
class UAnimNodeMirror;
class UAnimNodeSequence;
class UAnimSequence;
class UAnimSet;
class UAnimTree;
class UMorphNodeMultiPose;
class UMorphTarget;
class UMorphTargetSet;
class WxAnimSetViewer;
class WxASVPreview;
class WxDlgAnimationCompression;
class WxPropertyWindow;
class WxSocketManager;
class WxSkeletalMeshSimplificationWindow;
enum  EMorphImportError;

/**
 * Viewport client for the AnimSet viewer.
 */
struct FASVViewportClient: public FEditorLevelViewportClient
{
	WxAnimSetViewer*			AnimSetViewer;

	FPreviewScene				PreviewScene;
	FEditorCommonDrawHelper		DrawHelper;

	UBOOL						bShowBoneNames;
	UBOOL						bShowFloor;
	UBOOL						bShowSockets;
	UBOOL						bShowWindDir;
	UBOOL						bShowMorphKeys;
	UBOOL						bTriangleSortMode;

	UBOOL						bManipulating;
	EAxis						SocketManipulateAxis;
	EAxis						BoneManipulateAxis;
	FLOAT						DragDirX;
	FLOAT						DragDirY;
	FVector						WorldManDir;
	FVector						LocalManDir;
	ULineBatchComponent*		LineBatcher; // used by NVIDIA PhysX to draw debug visualization info even when inside the ANIM set viewer

	UBOOL						bDraggingTrackPos;
	INT							DraggingNotifyIndex;
	UBOOL						bDraggingNotifyTail;
	INT							DraggingMetadataIndex;
	INT							DraggingMetadataKeyIndex;

#if WITH_SIMPLYGON
	UTexture2D*					SimplygonLogo;
#endif // #if WITH_SIMPLYGON

	FASVViewportClient(WxAnimSetViewer* InASV);
	virtual ~FASVViewportClient();

	// FEditorLevelViewportClient interface

	virtual FSceneInterface* GetScene() { return PreviewScene.GetScene(); }
	virtual FLinearColor GetBackgroundColor();
	virtual void DrawTools(const FSceneView* View,FPrimitiveDrawInterface* PDI){}
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas);
	virtual void Tick(FLOAT DeltaSeconds);

	virtual UBOOL InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport,INT x, INT y);

	virtual void Serialize(FArchive& Ar);

	// FASVViewportClient interface
	void DrawWindDir(FViewport* Viewport, FCanvas* Canvas);
	/**
	 * Copies selected animseq to the clipboad
	 */
	void CopySelectedAnimSeqToClipboard(void);
};

/*-----------------------------------------------------------------------------
	WxASVMenuBar
-----------------------------------------------------------------------------*/

class WxASVMenuBar : public wxMenuBar
{
public:
	WxASVMenuBar();

	wxMenu*	FileMenu;
	wxMenu* EditMenu;
	wxMenu*	ViewMenu;
	wxMenu* AltBoneWeightingMenu;
	wxMenu* PhysXMenu;
	wxMenu* PhysX_Body;
	wxMenu* PhysX_Joint;
	wxMenu* PhysX_Contact;
	wxMenu* PhysX_Collision;
	wxMenu* PhysX_Fluid;
	wxMenu* PhysX_Cloth;
	wxMenu* PhysX_SoftBody;
	wxMenu* Apex_Clothing;
	wxMenu* Apex_Destructible;
	wxMenu* Apex_Misc;
#if WITH_APEX_PARTICLES
	wxMenu*	Apex_Emitter;
	wxMenu*	Apex_Iofx;
#endif
	// view sub menus
	wxMenu* TangentSubMenu;
	wxMenu* NormalSubMenu;
	wxMenu* SectionSubMenu;
	wxMenu* UVSetSubMenu;
	wxMenu* MeshMenu;
	wxMenu* AnimSetMenu;
	wxMenu* AnimSeqMenu;
	wxMenu* NotifyMenu;
	wxMenu* MorphSetMenu;
	wxMenu* MorphTargetMenu;
	wxMenu* AnimationCompressionMenu;

	void EnableAltBoneWeightingMenu(UBOOL bEnable, const USkeletalMeshComponent* PreviewSkelComp);
};

/*-----------------------------------------------------------------------------
	WxASVToolBar
-----------------------------------------------------------------------------*/

class WxASVToolBar : public WxToolBar
{
public:
	WxASVToolBar( wxWindow* InParent, wxWindowID InID );


private:
	WxMaskedBitmap SocketMgrB;
	WxMaskedBitmap ShowBonesB;
	WxMaskedBitmap ShowBoneNamesB;
	WxMaskedBitmap ShowWireframeB;
	WxMaskedBitmap ShowRefPoseB;
	WxMaskedBitmap ShowMirrorB;
	WxMaskedBitmap NewNotifyB;
	WxMaskedBitmap ClothB;
	WxMaskedBitmap SoftBodyGenerateB;
	WxMaskedBitmap SoftBodyToggleSimB;

	WxMaskedBitmap ShowRawAnimationB;
	WxMaskedBitmap ShowCompressedAnimationB;

	WxMaskedBitmap LODAutoB;
	WxMaskedBitmap LODBaseB;
	WxMaskedBitmap LOD1B;
	WxMaskedBitmap LOD2B;
	WxMaskedBitmap LOD3B;

	WxMaskedBitmap Speed1B;
	WxMaskedBitmap Speed10B;
	WxMaskedBitmap Speed25B;
	WxMaskedBitmap Speed50B;
	WxMaskedBitmap Speed100B;

	WxMaskedBitmap GoreMode;
	WxMaskedBitmap TangentMode;
	WxMaskedBitmap NormalMode;
	WxMaskedBitmap TriangleSortMode;

	WxBitmap ActiveListenBitmap;
	WxBitmap FOVResetBitmap;

	wxSlider* ProgressiveDrawingSlider;

public:
	wxSlider* FOVSlider;
	WxMaskedBitmap TriangleSortModeL;
	WxMaskedBitmap TriangleSortModeR;

	WxComboBox* ChunkComboBox;
	WxComboBox* SectionComboBox;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxASVStatusBar
-----------------------------------------------------------------------------*/

class WxASVStatusBar : public wxStatusBar
{
public:
	WxASVStatusBar( wxWindow* InParent, wxWindowID InID);

	void UpdateStatusBar( WxAnimSetViewer* AnimSetViewer );
};

/*-----------------------------------------------------------------------------
	WxSkeletonTreePopUpMenu
-----------------------------------------------------------------------------*/
class WxSkeletonTreePopUpMenu : public wxMenu
{
public:
	WxSkeletonTreePopUpMenu(WxAnimSetViewer* AnimSetViewer);
	~WxSkeletonTreePopUpMenu();
};

/*-----------------------------------------------------------------------------
	WxConvertToAdditiveDialog
-----------------------------------------------------------------------------*/

/** 
 * Additive animation creation dialog 
 */
class WxConvertToAdditiveDialog : public wxDialog
{
public:
	/** Constructor */
	WxConvertToAdditiveDialog(wxWindow *parent = NULL, WxAnimSetViewer* InAnimSetViewer = NULL);

	/** Event called when "OK" button is pressed */
	void OnOK( wxCommandEvent& In );
	/** Event called when BuildMethod selection changes in ListBox */
	void OnBuildMethodChange( wxCommandEvent& In );

	/** Return the selected animation to use as a base */
	UAnimSequence* GetSelectedAnimation();
	/** Return Build Method to use to create additive animation */
	EConvertToAdditive GetBuildMethod();
	/** Returns if LoopingAnim checkbox is on or off */
	UBOOL GetLoopingAnim();

private:
	wxListBoxBase*		BuildMethod;
	wxListBoxBase*		AnimList;
	wxCheckBox*			LoopingAnimCB;
	EConvertToAdditive	SelectedBuildMethod;
	WxAnimSetViewer*	AnimSetViewer;

	/** Update AnimList Listbox */
	void UpdateAnimationList();

	// 	DECLARE_DYNAMIC_CLASS_NO_COPY(WxConvertToAdditiveDialog)
	DECLARE_EVENT_TABLE()
};

/** Different ways to add an additive animation */
enum EAddAdditive
{
	/** Scale additive animation to match length of source */
	EAA_ScaleAdditiveToSource,
	/** Scale Source to Additive */
	EAA_ScaleSourceToAdditive,
	/** Always last one */
	EAA_MAX	
};

/** 
 * Add Additive Animation Dialog
 */
class WxAddAdditiveAnimDialog : public wxDialog
{
public:
	/** Constructor */
	WxAddAdditiveAnimDialog(const wxString& StringValue, const wxString& WindowStringValue, wxWindow *parent = NULL, WxAnimSetViewer* InAnimSetViewer = NULL);

	/** Event called when "OK" button is pressed */
	void OnOK( wxCommandEvent& In );
	/** Event called when BuildMethod selection changes in ListBox */
	void OnBuildMethodChange( wxCommandEvent& In );

	/** Return the selected additive animation*/
	UAnimSequence* GetSelectedAnimation();
	/** Returns DestinationAnimationName */
	FName GetDestinationAnimationName();
	/** Return how we'd want that additive animation to be added to the source */
	EAddAdditive GetBuildMethod();

private:
	wxTextCtrl*			AnimNameCtrl;
	wxListBoxBase*		AnimListCtrl;
	wxListBoxBase*		BuildMethodCtrl;
	WxAnimSetViewer*	AnimSetViewer;
	EAddAdditive		SelectedBuildMethod;

	/** Update AnimList Listbox */
	void UpdateAnimationList();

	// 	DECLARE_DYNAMIC_CLASS_NO_COPY(WxAddAdditiveAnimDialog)
	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxASVPreview
-----------------------------------------------------------------------------*/

/**
 * A wxWindows container for FCascadePreviewViewportClient.
 */
class WxASVPreview : public wxWindow
{
public:
	FASVViewportClient* ASVPreviewVC;

	WxASVPreview(wxWindow* InParent, wxWindowID InID, class WxAnimSetViewer* InASV);
	~WxASVPreview();

	/**
	 * Calls DestoryViewport(), then creates a new viewport client and associated viewport.
	 */
	void CreateViewport(class WxAnimSetViewer* InASV);

	/**
	 * Destroys any existing viewport client and associated viewport.
	 */
	void DestroyViewport();

private:
	void OnSize( wxSizeEvent& In );	
	DECLARE_EVENT_TABLE()
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// WxMorphTargetPane : This is responsible for drawing list of morph targets with options
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
class WxMorphTargetPane : public wxScrolledWindow
{
public:
	WxMorphTargetPane(wxWindow* InParent);

	/**
	* Layout all children components for input list
	*/
	void LayoutWindows( const TArray<UMorphTarget*>& MorphTargetList );

	/*
	 * Return TRUE, if the index item is selected
	 */
	UBOOL IsSelected( INT Index );

	/*
	* Select all if bSelect is true. Deselect otherwise. 
	*/
	void SelectAll( UBOOL bSelect );

	/*
	* Reset all weights on slider list
	*/
	void ResetAllWeights();

private:
	wxFlexGridSizer*		MorphTargetSizer;

	// List of controls to handle morph target
	TArray<wxTextCtrl*>		MorphTargetTextNames;
	TArray<wxSlider*>		MorphTargetWeightSliders;
	TArray<wxCheckBox*>		MorphTargetSelectChecks;
};
/*-----------------------------------------------------------------------------
	WxAnimSetViewer
-----------------------------------------------------------------------------*/

#define ASV_SCRUBRANGE (10000)

struct FASVSocketPreview
{
	USkeletalMeshSocket*	Socket;
	UPrimitiveComponent*	PreviewComp;

	FASVSocketPreview( USkeletalMeshSocket* InSocket, UPrimitiveComponent* InPreviewComp ) :
		Socket(InSocket),
		PreviewComp(InPreviewComp)
	{}

	FASVSocketPreview() {}

	friend FArchive& operator<<( FArchive& Ar, FASVSocketPreview& S )
	{
		return Ar << S.Socket << S.PreviewComp;
	}
};

template <> struct TIsPODType<FASVSocketPreview> { enum { Value = true }; };

class WxAnimSetViewer : public WxTrackableFrame, public FNotifyHook, public FSerializableObject, public FDockingParent, public FCallbackEventDevice
{
public:
	WxASVMenuBar*			MenuBar;
	WxASVToolBar*			ToolBar;
	WxASVStatusBar*			StatusBar;
	WxASVPreview*			PreviewWindow;
	WxTreeCtrl*				SkeletonTreeCtrl;
	TMap<wxTreeItemId,INT>	SkeletonTreeItemBoneIndexMap;

	USkeletalMesh*			SelectedSkelMesh;
	UAnimSet*				SelectedAnimSet;
	UAnimSequence*			SelectedAnimSeq;
	UMorphTargetSet*		SelectedMorphSet;
	USkeletalMeshSocket*	SelectedSocket;
	TArray<UMorphTarget*>	SelectedMorphTargets;
	
	/** The main preview SkeletalMeshComponent.  Always plays compressed animation, if present. */
	UASVSkelComponent*		PreviewSkelComp;
	/** The main preview SkeletalMeshComponent.  Always plays uncompressed animation, if present. Only for drawing uncompressed bones. */
	UASVSkelComponent*		PreviewSkelCompRaw;
	USkeletalMeshComponent*	PreviewSkelCompAux1;
	USkeletalMeshComponent*	PreviewSkelCompAux2;
	USkeletalMeshComponent*	PreviewSkelCompAux3;

	UAnimTree*				PreviewAnimTree;
	/** Uncompressed animation tree. */
	UAnimTree*				PreviewAnimTreeRaw;
	UAnimNodeSequence*		PreviewAnimNode;
	/** Uncompressed animation sequence. */
	UAnimNodeSequence*		PreviewAnimNodeRaw;
	UAnimNodeMirror*		PreviewAnimMirror;
	/** Uncompressed animation mirroring. */
	UAnimNodeMirror*		PreviewAnimMirrorRaw;
	UMorphNodeMultiPose*	PreviewMorphPose;

	//A floor for interacting with physics
	UStaticMeshComponent*	EditorFloorComp;

	FRBPhysScene*			RBPhysScene;
	FRotator				WindRot;
	FLOAT					WindStrength;
#if WITH_APEX
	FLOAT					WindVelocityBlendTime;
#endif
	/** Bone manipulation */
	EASVSocketMoveMode		BoneMoveMode;

	struct FLODInfluenceInfo
	{
		/** Gore mesh / bone influence swapping */
		TSet<INT> 				InfluencedBoneVerts;
		/** Vertex indices weighted by a given bone but not part of the vertex influence swap */
		TSet<INT>				NonInfluencedBoneVerts;
		/** Mapping of all bones and the verts weighted to them */
		TArray< TArray<INT> >   BoneVertices;
	};

	TArray<FLODInfluenceInfo> BoneInfluenceLODInfo;

	/** Animation playback speed, in [0.01f,1.0f]. */
	FLOAT					PlaybackSpeed;

	/** Wireframe cycle counter */
	INT						WireframeCycleCounter;
	/** Show Additive Base */
	UBOOL					bShowAdditiveBase;

	/** Show Vertex Info */
	struct VertexInfoList
	{
		INT		SelectedMaterialIndex;
		UBOOL	bShowTangent;
		UBOOL	bShowNormal;
		INT		ColorOption; // 0-none, 1-tangent, 2-normal, 3-mirror
		INT		SelectedVertexIndex;
		FVector	SelectedVertexPosition;
		TArray<INT>		BoneIndices;
		TArray<FLOAT>	BoneWeights;
	};

	VertexInfoList			VertexInfo;

	/** If TRUE, show per-vertex scaling of how far a cloth vert can move from its animated vertex pos. */
	UBOOL					bShowClothMovementScale;

	// Various wxWindows controls
	WxBitmap				StopB, PlayB, LoopB, NoLoopB, UseButtonB, SearchAllB, ClearB, ClearDisabledB;

	wxComboBox*				SkelMeshCombo;
	wxComboBox*				SkelMeshAux1Combo;
	wxComboBox*				SkelMeshAux2Combo;
	wxComboBox*				SkelMeshAux3Combo;

	wxComboBox*				AnimSetCombo;
	wxBitmapButton*			AnimSeqSearchAllButton;
	wxTextCtrl*				AnimSeqFilter;
	wxListBox*				AnimSeqList;
	wxTimer					AnimSeqFilterTimer;
	
	wxComboBox*				MorphSetCombo;
	
	WxMorphTargetPane*		MorphTargetPanel;

	wxNotebook*				PropNotebook;
	wxNotebook*				ResourceNotebook;
	wxSlider*				TimeSlider;
	wxBitmapButton*			PlayButton;
	wxBitmapButton*			LoopButton;

	WxPropertyWindowHost*		MeshProps;
	WxPropertyWindowHost*		AnimSetProps;
	WxPropertyWindowHost*		AnimSeqProps;
	WxPropertyWindowHost*		MorphTargetProps;

#if WITH_SIMPLYGON
	WxSkeletalMeshSimplificationWindow* SimplificationWindow;
#endif // #if WITH_SIMPLYGON

	WxSocketManager*				SocketMgr;
	TArray<FASVSocketPreview>		SocketPreviews;

	WxDlgAnimationCompression*		DlgAnimationCompression;

	/** Toggle usage of instance weights on the preview mesh */
	UBOOL					bPreviewInstanceWeights;
	/** Toggle visibility of all vertices on mesh while editing bone influences */
	UBOOL					bDrawAllBoneInfluenceVertices;
	/** The UV Set index we are currently displaying.  -1 if we should not display any UVs. */
	INT						UVSetToDisplay;

	/** Stores the currently selected sort strip */
	INT		SelectedSortStripIndex;
	INT		SelectedSortStripLodIndex;
	INT		SelectedSortStripSectionIndex;
	INT		MouseOverSortStripIndex;
	UBOOL	bSortStripMoveForward;
	UBOOL	bSortStripMoveBackward;

	/** Flag to indicate the AnimNotify data needs to be resampled */
	UBOOL bResampleAnimNotifyData;

	/** Determines if the user can search through all anim sequences in the sequence combo box. */
	UBOOL bSearchAllAnimSequences;
	/** Flag to indicate if the user turned on the option to view all anim sequences at least once. */
	UBOOL bPromptUserToLoadAllAnimSets;

	WxAnimSetViewer(wxWindow* InParent, wxWindowID InID, USkeletalMesh* InSkelMesh, UAnimSet* InSelectAnimSet, UMorphTargetSet* InMorphSet);
	virtual ~WxAnimSetViewer();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/**
	* Called once to load AnimSetViewer settings, including window position.
	*/
	void LoadSettings();

	/**
	* Writes out values we want to save to the INI file.
	*/
	void SaveSettings();

	/**
	 * Returns a handle to the anim set viewer's preview viewport client.
	 */
	FASVViewportClient* GetPreviewVC();

	/** Returns the current LOD index being viewed. */
	INT GetCurrentLODIndex() const;

	// FSerializableObject interface
	void Serialize(FArchive& Ar);

	// Menu handlers
	/**
	 *	Called when a SIZE event occurs on the window
	 *
	 *	@param	In		The size event information
	 */
	void OnSize( wxSizeEvent& In );
	void OnMenuEditUndo( wxCommandEvent& In );
	void OnMenuEditRedo( wxCommandEvent& In );
	void OnSkelMeshComboChanged( wxCommandEvent& In );
	void OnAuxSkelMeshComboChanged( wxCommandEvent& In );
	void OnAnimSetComboChanged( wxCommandEvent& In );
	void OnAnimSeqListChanged( wxCommandEvent& In );
	void OnAnimSeqListRightClick( wxMouseEvent& In );
	void OnSkelMeshUse( wxCommandEvent& In );
	void OnAnimSetUse( wxCommandEvent& In );
	void OnAnimSeqSearchTypeChange( wxCommandEvent& In );
	void OnAnimSeqFilterTextChanged( wxCommandEvent& In );
	void OnAnimSeqFilterEnterPressed( wxCommandEvent& In );
	void OnClearAnimSeqFilter( wxCommandEvent& In );
	void OnClearAnimSeqFilter_UpdateUI( wxUpdateUIEvent& In );
	void OnAnimSeqFilterTimer( wxTimerEvent& In );
	void OnAuxSkelMeshUse( wxCommandEvent& In );
	void OnImportPSA( wxCommandEvent& In );
#if WITH_FBX
	void OnImportFbxAnim( wxCommandEvent& In );
	void OnExportFbxAnim( wxCommandEvent& In );
#endif // WITH_FBX
#if WITH_SIMPLYGON
	void OnGenerateLOD( wxCommandEvent& In );
#endif // #if WITH_SIMPLYGON
	void OnImportMeshLOD( wxCommandEvent& In );
	void OnImportMeshWeights( wxCommandEvent& In );
	void OnToggleMeshWeights( wxCommandEvent& In );
	void ToggleMeshWeights(UBOOL bEnable);
	void OnToggleShowAllMeshVerts( wxCommandEvent& In );
	void OnNewAnimSet( wxCommandEvent& In );
	void OnTimeScrub( wxScrollEvent& In );
	void OnViewBones( wxCommandEvent& In );
	void OnShowRawAnimation( wxCommandEvent& In );
	void OnViewBoneNames( wxCommandEvent& In );
	void OnViewFloor( wxCommandEvent& In );
	void OnViewWireframe( wxCommandEvent& In );
	void OnViewAdditiveBase(wxCommandEvent& In);
	void OnViewGrid( wxCommandEvent& In );
	void OnViewSockets( wxCommandEvent& In );
	void OnViewMorphKeys( wxCommandEvent& In );
	void OnViewRefPose( wxCommandEvent& In );
	void OnViewMirror( wxCommandEvent& In );
	void OnViewBounds( wxCommandEvent& In );
	void OnViewCollision( wxCommandEvent& In );
	void OnViewSoftBodyTetra(wxCommandEvent& In);
	void OnViewClothMoveDistScale(wxCommandEvent& In);
	void OnEditAltBoneWeightingMode(wxCommandEvent& In);
	void OnSectionSelected(wxCommandEvent& In);
	void SelectSection(INT MaterialID);
	void OnViewVertexMode(wxCommandEvent& In);
	void EnableAltBoneWeighting(UBOOL bEnable);
	void UpdateVertexMode();
	void OnForceLODLevel( wxCommandEvent& In );
	void OnRemoveLOD( wxCommandEvent& In );
	void OnViewChunk( wxCommandEvent& In );
	void OnViewSection( wxCommandEvent& In );
	void OnLoopAnim( wxCommandEvent& In );
	void OnPlayAnim( wxCommandEvent& In );
	void OnEmptySet( wxCommandEvent& In );
	void OnShowUVSet( wxCommandEvent& In );
	void OnShowUVSet_UpdateUI( wxUpdateUIEvent& In );

	/** Copies anim notifies from the selected sequence to a user-named destination sequence. */
	void OnNotifyCopy( wxCommandEvent& In );
	void OnNotifyShift( wxCommandEvent& In );

	void OnSpeed(wxCommandEvent& In);

	void OnPhysXDebug(wxCommandEvent& In);
#if WITH_APEX
	void OnApexDebug(wxCommandEvent& In);
#endif

	void OnDeleteTrack( wxCommandEvent& In );
	void OnDeleteMorphTrack( wxCommandEvent& In );
	void OnCopyTranslationBoneNames(wxCommandEvent& In);
	void OnAnalyzeAnimSet(wxCommandEvent& In);
	void OnRenameSequence( wxCommandEvent& In );
	void OnRemovePrefixFromSequences( wxCommandEvent& In );
	void OnDeleteSequence( wxCommandEvent& In );
	void OnCopySequence( wxCommandEvent& In );
	void OnMoveSequence( wxCommandEvent& In );
	void OnMakeSequencesAdditive(wxCommandEvent& In);
	void OnRebuildAdditiveAnimation(wxCommandEvent& In);
	void OnAddAdditiveAnimationToSelectedSequence(wxCommandEvent& In);
	void OnSequenceApplyRotation( wxCommandEvent& In );
	void OnSequenceReZero( wxCommandEvent& In );
	void OnSequenceCrop( wxCommandEvent& In );
	void OnNotifySort( wxCommandEvent& In );
	void OnNotifiesRemove( wxCommandEvent& In );
	void OnNewNotify( wxCommandEvent& In );
	void AddedNotify( const INT iIndex );
	void OnAllParticleNotifies( wxCommandEvent& In );
	void OnRefreshAllNotifierData( wxCommandEvent& In );
	void OnCopySequenceName( wxCommandEvent& In );
	void OnCopySequenceNameList( wxCommandEvent& In );
	void OnCopyMeshBoneNames( wxCommandEvent& In );
	void OnCopyWeightedMeshBoneNames( wxCommandEvent& In );
	void OnFixupMeshBoneNames( wxCommandEvent& In );
	void OnSwapLODSections( wxCommandEvent& In );
	void OnMergeMaterials( wxCommandEvent& In );
	void OnAutoMirrorTable( wxCommandEvent& In );
	void OnCheckMirrorTable( wxCommandEvent& In );
	void OnCopyMirrorTable( wxCommandEvent& In );
	void OnCopyMirroTableFromMesh( wxCommandEvent& In );
	void OnUpdateBounds( wxCommandEvent& In );
	void OnClearPreviewMeshes( wxCommandEvent& In );
	void OnCopySockets( wxCommandEvent& In );  
	void OnRenameSockets( wxCommandEvent& In );
	void OnPasteSockets( wxCommandEvent& In );

	void OnToggleClothSim( wxCommandEvent& In );
	void OnSoftBodyGenerate( wxCommandEvent& In );
	void OnSoftBodyToggleSim( wxCommandEvent& In );

	void OnNewMorphTargetSet( wxCommandEvent& In );
	void OnImportMorphTarget( wxCommandEvent& In );
	void OnImportMorphTargetLOD( wxCommandEvent& In );
	void OnImportMorphTargets( wxCommandEvent& In );
	void OnImportMorphTargetsLOD( wxCommandEvent& In );
	void OnMorphSetComboChanged( wxCommandEvent& In );
	void OnMorphSetUse( wxCommandEvent& In );
	void OnMorphTargetTextChanged( wxCommandEvent& In );
	void OnMorphTargetWeightChanged( wxCommandEvent& In );
	void OnSelectMorphTarget( wxCommandEvent& In );
	void OnResetMorphTargetPreview( wxCommandEvent& In );
	void OnDeleteMorphTarget( wxCommandEvent& In );
	void OnUpdateMorphTarget( wxCommandEvent& In );
	void OnSelectAllMorphTargets( wxCommandEvent& In );
	void OnToggleTriangleSortMode( wxCommandEvent& In );
	void OnToggleTriangleSortModeLR( wxCommandEvent& In );
	// Socket manager window
	void OnOpenSocketMgr( wxCommandEvent& In );
	void OnNewSocket( wxCommandEvent& In );
	void OnDeleteSocket( wxCommandEvent& In );
	void OnClickSocket( wxCommandEvent& In );
	void OnSocketMoveMode( wxCommandEvent& In );

	void OnToggleActiveFileListen( wxCommandEvent& In );
	void SetFileSystemNotifications(const UBOOL bListenOnOff);
	void UI_ActiveFileListen( wxUpdateUIEvent& In );

	void SetSelectedSocket(USkeletalMeshSocket* InSocket);
	void UpdateSocketList();
	void SetSocketMoveMode( EASVSocketMoveMode NewMode );

	void ClearSocketPreviews();
	void RecreateSocketPreviews();
	void UpdateSocketPreviews();

	// update LOD menu based on PreviewSkelComp->ForcedLODModel
	// enable/disable some menu sets for alternative bone weighting
	void UpdateForceLODMenu();
	void UpdateAltBoneWeightingMenu();

	/**
	 * Refreshes the chunk preview toolbar combo box w/ a listing of chunks belonging to the current LOD
	 */
	void UpdateChunkPreview();
	/**
	 * Sets the index of the chunk to preview
	 */
	void PreviewChunk(INT ChunkIdx);
	/**
	 * Refreshes the section preview toolbar combo box w/ a listing of sections belonging to the current LOD
	 */
	void UpdateSectionPreview();
	/**
	 * Sets the index of the section to preview
	 */
	void PreviewSection(INT SectionIdx);

	// Skeleton Tree Manager
	void FillSkeletonTree();
	void OnFillSkeletonTree(wxCommandEvent &In);
	void OnSkeletonTreeItemRightClick(wxTreeEvent& In);
	/*
	 * Callback when the skeleton tree control items are selected/deselected
	 * @param In - Event parameter data
	 */
	void OnSkeletonTreeSelectionChange(wxTreeEvent& In);
	void OnSkeletonTreeMenuHandleCommand(wxCommandEvent &In);

	/*
	 *  Update the bones of interest in the viewer for blendweight viewing/bone manipulation
	 *  @param NewBoneIndex - bone clicked on in the tree view (since GetSelections isn't always accurate at time of calling)
	 */
	void UpdateBoneManipulationControl(INT NewBoneIndex);

	/*
	 * Get a list of vertex indices that reference equivalent vertex positions in the skeletal mesh
	 * @param LODIdx - mesh LOD to compare against
	 * @param InfluenceIdx - influence track to compare against
	 * @param VertCheckIndex - vertex index to check against
	 * @param EquivalentVertices - array of vertex indices to fill in
	 * @return Count of vertices found to be equivalent
	 */
	INT GetEquivalentVertices(INT LODIdx, INT InfluenceIdx, INT VertCheckIndex, TArray<INT>& EquivalentVertices);
	
	/*
	 *   Calculate the bone to vert list mapping and clear out related data
	 * @param SelectedSkelMesh - mesh of interest
	 */
	void InitBoneWeightInfluenceData(const USkeletalMeshComponent* SkelComp);

	/*
	 *   Update the state of a single vertex in the skeletal mesh
	 * @param LODIdx - LOD this vertex applies to 
	 * @param InfluenceIdx - Influence track to swap
	 * @param VertIdx - Vertex Index to swap
	 * @param bContributingInfluence - whether to swap in (TRUE) or swap back (FALSE)
	 */
	void UpdateInfluenceWeight(INT LODIdx, INT InfluenceIdx, INT VertIndex, UBOOL bContributingInfluence);

	/*
	 * Update the bone influence weights on the skeletal mesh for bone pairs specified
	 * @BonesOfInterest - array of bone indices to update
	 */
	void UpdateInfluenceWeights(const TArray<INT>& BonesOfInterest);

	/** Toggles the modal animation Animation compression dialog. */
	void OnOpenAnimationCompressionDlg(wxCommandEvent& In);

	/** Updates the contents of the status bar, based on e.g. the selected set/sequence. */
	void UpdateStatusBar();

	/** Updates the contents of the status bar, based on e.g. the selected set/sequence. */
	void UpdateMaterialList();

	/** Slides the progressive drawing slider */
	void OnProgressiveSliderChanged(wxScrollEvent & In);

	/** Resets the FOV slider */
	void OnFOVReset( wxCommandEvent& In );

	/** Slides the FOV slider */
	void OnFOVSliderChanged(wxScrollEvent & In);

	/** Sets the FOV and updated the icons */
	void SetFOV(const INT iFOV);

	// Tools
	void SetSelectedSkelMesh(USkeletalMesh* InSkelMesh, UBOOL bClearMorphTarget, UBOOL bReselectAnimSet = TRUE);
	void SetSelectedAnimSet(UAnimSet* InAnimSet, UBOOL bAutoSelectMesh, UBOOL bRefreshAnimSequenceList = TRUE);
	void SetSelectedAnimSequence(UAnimSequence* InAnimSeq);
	void SetSelectedMorphSet(UMorphTargetSet* InMorphSet, UBOOL bAutoSelectMesh);
	void UpdateSelectedMorphTargets();

	void ImportPSA();
#if WITH_FBX
	void ImportFbxAnim();
	void ExportFbxAnim();
#endif // WITH_FBX
	void ImportMeshLOD();
	UBOOL ImportMeshLOD(USkeletalMesh* InSkeletalMesh,INT DesiredLOD);
	void ImportMeshWeights();
	
	/**
	* Displays and handles dialogs necessary for importing 
	* a new morph target mesh from file. The new morph
	* target is placed in the currently selected MorphTargetSet
	*
	* @param bImportToLOD - if TRUE then new files will be treated as morph target LODs 
	* instead of new morph target resources
	*/
	void ImportMorphTarget(UBOOL bImportToLOD, UBOOL UseImportName);
	
	void CreateNewAnimSet();
	void UpdateAnimSeqList();
	void UpdateAnimSetCombo();
	void UpdateMorphTargetList();
	void UpdateMorphSetCombo();
	void RefreshPlaybackUI();
	FLOAT GetCurrentSequenceLength();
	void TickViewer(FLOAT DeltaSeconds);
	void EmptySelectedSet();
	void DeleteTrackFromSelectedSet();
	void DeleteMorphTrackFromSelectedSet();
	void CopyTranslationBoneNamesToAnimSet();
	void AnalyzeAnimSet();
	void RenameSelectedSeq();
	void RemovePrefixFromSequences();
	void DeleteSelectedSequence();
	UBOOL VerifyAdditiveSequencesToMove(TArray<UAnimSequence*>& Sequences, UAnimSet* DestAnimSet, UBOOL bMove);
	INT RemoveAdditiveSequences(TArray<UAnimSequence*>& Sequences, TArray<UAnimSequence*>& SequencesToRemove, UBOOL bRemoveAdditive=TRUE, UBOOL bRemoveBase=TRUE);
	void CopySelectedSequence();
	void MoveSelectedSequence();
	void MakeSelectedSequencesAdditive();
	void RebuildAdditiveAnimation();
	void AddAdditiveAnimationToSelectedSequence(UBOOL bPerformSubtraction);
	UBOOL AddAdditiveAnimation(UAnimSequence* SourceAnimSeq, UAnimSequence* AdditiveAnimSeq, UAnimSequence* DestAnimSeq, USkeletalMesh* SkelMesh, FName AnimationName, EAddAdditive BuildMethod, UBOOL bPerformSubtraction);
	UBOOL CopyAnimSequence(UAnimSequence* SourceAnimSeq, UAnimSequence *DestAnimSeq, UAnimSet* SourceAnimSet, UAnimSet* DestAnimSet, USkeletalMesh* FillInMesh);
	void SequenceApplyRotation();
	void SequenceReZeroToCurrent();
	void SequenceCrop(UBOOL bFromStart=true);
	void UpdateMeshBounds();
	void UpdateSkelComponents();
	void UpdateForceLODButtons();
	void RenameMorphTarget(const INT Index, const TCHAR * NewName);
	void UpdateMorphTargetWeight(const INT Index, const FLOAT Weight);
	void ResetMorphTargets();
	void DeleteSelectedMorphTarget();
	void RemapVerticesSelectedMorphTarget();
	void UpdateClothWind();

	/**
	 *	Update the preview window.
	 */
	void UpdatePreviewWindow();

	// Notification hook.
	virtual void NotifyDestroy( void* Src );
	virtual void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	virtual void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	virtual void NotifyExec( void* Src, const TCHAR* Cmd );

	// Undo/Redo support
	bool BeginTransaction(const TCHAR* pcTransaction);
	bool EndTransaction(const TCHAR* pcTransaction);
	void UndoTransaction();
	void RedoTransaction();
	bool TransactionInProgress();

	/**
	 * Event wrapper around copying selected animseq to the clipboard
	 */
	void OnCopySelectedAnimSeqToClipboard(wxCommandEvent& In);

	/** Handle callback events. */
	virtual void Send( ECallbackEventType InType );
	virtual void Send( ECallbackEventType InType, const FString& InString, UObject* InObject );

protected:
	/**
	 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
	 *  @return A string representing a name to use for this docking parent.
	 */
	virtual const TCHAR* GetDockingParentName() const;

	/**
	 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
	 */
	virtual const INT GetDockingParentVersion() const;

	/**
	 * Populates all the anim sequences in the given anim set to the anim sequence list. 
	 *
	 * @param	SetToAdd	The anim set containing sequences to add. 
	 */
	void AddAnimSequencesToList( UAnimSet* SetToAdd );

	// Undo/Redo support
	bool	bTransactionInProgress;
	FString	kTransactionName;

private:

	/**
	 * Updates the visibility and collision of the floor
	 */
	void UpdateFloorComponent(void);

#if WITH_FBX
    /**
     * Import Morph Target and Target LOD from FBX file.
     * Compare to PSK file, we can import several meshes for FBX file in one time.
     * @param bImportToLOD - if TRUE then new files will be treated as morph target LODs instead of morph targets
     * @param bUseImportName - if TRUE then morph target will be named as file name. This option is meaningless when
     *                         import morph target LOD.
     * @param Filename - FBX file name
     * @param ImportError -
     */
    void ImportFbxMorphTarget(UBOOL bImportToLOD, UBOOL bUseImportName, FFilename Filename, EMorphImportError &ImportError);
#endif // WITH_FBX

    /**
     * Display Error when import Morph target or Morph target LODs
     * @param ImportError -
     * @param Filename -
     */
	void DisplayError(EMorphImportError &ImportError, FFilename &Filename);

	DECLARE_EVENT_TABLE()
};

#endif // __ANIMSETVIEWER_H__
