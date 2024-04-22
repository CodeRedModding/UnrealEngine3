/*=============================================================================
	PhAT.h: Physics Asset Tool main header
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PHAT_H__
#define __PHAT_H__

#include "TrackableWindow.h"

//
//	HPhATBoneProxy
//
struct HPhATBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HPhATBoneProxy,HHitProxy);

	INT							BodyIndex;
	EKCollisionPrimitiveType	PrimType;
	INT							PrimIndex;

	HPhATBoneProxy(INT InBodyIndex, EKCollisionPrimitiveType InPrimType, INT InPrimIndex):
		HHitProxy(HPP_UI),
		BodyIndex(InBodyIndex),
		PrimType(InPrimType),
		PrimIndex(InPrimIndex) {}
};

//
//	HPhATConstraintProxy
//
struct HPhATConstraintProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HPhATConstraintProxy,HHitProxy);

	INT							ConstraintIndex;

	HPhATConstraintProxy(INT InConstraintIndex):
		HHitProxy(HPP_UI),
		ConstraintIndex(InConstraintIndex) {}
};

//
//	HPhATWidgetProxy
//
struct HPhATWidgetProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HPhATWidgetProxy,HHitProxy);

	EAxis		Axis;

	HPhATWidgetProxy(EAxis InAxis):
		HHitProxy(HPP_UI),
		Axis(InAxis) {}
};

//
//	HPhATBoneNameProxy
//
struct HPhATBoneNameProxy : public HHitProxy
{
	DECLARE_HIT_PROXY(HPhATBoneNameProxy,HHitProxy);

	INT			BoneIndex;

	HPhATBoneNameProxy(INT InBoneIndex):
		HHitProxy(HPP_UI),
		BoneIndex(InBoneIndex) {}
};


//
//	FPhATViewportClient
//
struct FPhATViewportClient: public FEditorLevelViewportClient
{
	class WxPhAT*			AssetEditor;

	FPreviewScene			PreviewScene;
	
	/** Helper class that draws common scene elements. */
	FEditorCommonDrawHelper		DrawHelper;

	UDirectionalLightComponent*	DirectionalLightComponent;
	USkyLightComponent*			SkyLightComponent;

	UFont*					PhATFont;

	UMaterialInterface*		ElemSelectedMaterial;
	UMaterialInterface*		BoneSelectedMaterial;
	UMaterialInterface*		BoneUnselectedMaterial;
	UMaterialInterface*		BoneNoCollisionMaterial;

	UMaterialInterface*		JointLimitMaterial;

	INT						DistanceDragged;

	FPhATViewportClient(class WxPhAT* InAssetEditor);

	virtual FSceneInterface* GetScene() { return PreviewScene.GetScene(); }
	virtual FLinearColor GetBackgroundColor();
	virtual UBOOL InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport, INT ControllerId, FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad=FALSE);
	virtual void MouseMove(FViewport* Viewport, INT X, INT Y);

	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void Draw(FViewport* Viewport, FCanvas* Canvas);

	virtual void Serialize(FArchive& Ar);

	virtual void DoHitTest(FViewport* Viewport, FName Key, EInputEvent Event);

	virtual void Tick(FLOAT DeltaSeconds);

	void UpdateLighting();
};

enum EPhATMovementMode
{
	PMM_Rotate,
	PMM_Translate,
	PMM_Scale
};

enum EPhATEditingMode
{
	PEM_BodyEdit,
	PEM_ConstraintEdit
};

enum EPhATMovementSpace
{
	PMS_Local,
	PMS_World
};

enum EPhATRenderMode
{
	PRM_Solid,
	PRM_Wireframe,
	PRM_None
};

enum EPhATConstraintViewMode
{
	PCV_None,
	PCV_AllPositions,
	PCV_AllLimits
};

enum EPhATNextSelectEvent
{
	PNS_Normal,
	PNS_DisableCollision,
	PNS_EnableCollision,
	PNS_CopyProperties,
	PNS_WeldBodies,
	PNS_MakeNewBody
};

/*-----------------------------------------------------------------------------
	WxPhATMenuBar
-----------------------------------------------------------------------------*/

class WxPhATMenuBar : public wxMenuBar
{
public:
	wxMenu	*EditMenu, *ToolsMenu;

	WxPhATMenuBar(class WxPhAT* InPhAT);
	~WxPhATMenuBar();
};

/*-----------------------------------------------------------------------------
	WxBodyContextMenu
-----------------------------------------------------------------------------*/
class WxBodyContextMenu : public wxMenu
{
public:
	WxBodyContextMenu(WxPhAT* AssetEditor);
	~WxBodyContextMenu();
};

/*-----------------------------------------------------------------------------
	WxConstraintContextMenu
-----------------------------------------------------------------------------*/
class WxConstraintContextMenu : public wxMenu
{
public:
	WxConstraintContextMenu(WxPhAT* AssetEditor);
	~WxConstraintContextMenu();
};

/*-----------------------------------------------------------------------------
	WxPhATToolBar
-----------------------------------------------------------------------------*/

class WxPhATToolBar : public WxToolBar
{
public:
	WxPhATToolBar( wxWindow* InParent, wxWindowID InID );
	~WxPhATToolBar();

	WxMaskedBitmap BodyModeB, ConstraintModeB;
	WxMaskedBitmap HighlightVertB, StartSimB, StopSimB;
	WxMaskedBitmap RotModeB, TransModeB, ScaleModeB;
	WxMaskedBitmap WorldSpaceB, LocalSpaceB;
	WxMaskedBitmap AddSphereB, AddSphylB, AddBoxB, DupPrimB, DelPrimB;
	WxMaskedBitmap ShowSkelB, ShowCOMB;
	WxMaskedBitmap ShowContactsB;
	WxMaskedBitmap HideCollB, WireCollB, ShowCollB;
	WxMaskedBitmap HideMeshB, WireMeshB, ShowMeshB;
	WxMaskedBitmap ConFrameB, SnapB;
	WxMaskedBitmap ConSnapToBoneB, ConSnapAllToBoneB;
	WxMaskedBitmap ConHideB, ConPosB, ConLimitB;
	WxMaskedBitmap BSJointB, HingeB, PrismaticB, SkelB, DelJointB;
	WxMaskedBitmap DisablePairB, EnablePairB;
	WxMaskedBitmap CopyPropsB, InstancePropsB, WeldB, AddBoneB, ShowFloorB;
	WxMaskedBitmap LockB, ShowAnimSkelB;

	wxBitmapButton *ModeButton, *SpaceButton, *SimButton, *MeshViewButton, *CollisionViewButton, *ConstraintViewButton, *AnimPlayButton;
	WxComboBox *AnimCombo;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxPhATPreview
-----------------------------------------------------------------------------*/

// wxWindows Holder for FPhATViewportClient
class WxPhATPreview : public wxWindow
{
public:
	FPhATViewportClient* PhATPreviewVC;

	WxPhATPreview( wxWindow* InParent, wxWindowID InID, class WxPhAT* InPhAT );
	~WxPhATPreview();

	void OnSize( wxSizeEvent& In );

	DECLARE_EVENT_TABLE()
};


/*-----------------------------------------------------------------------------
	FPhATTreeBoneItem
-----------------------------------------------------------------------------*/

struct FPhATTreeBoneItem
{
	INT							BodyIndex;
	EKCollisionPrimitiveType	PrimType;
	INT							PrimIndex;

	FPhATTreeBoneItem( INT InBodyIndex, EKCollisionPrimitiveType InPrimType, INT InPrimIndex )
	:	BodyIndex(InBodyIndex)
	,	PrimType(InPrimType)
	,	PrimIndex(InPrimIndex)
	{}	
};

/*-----------------------------------------------------------------------------
	WxPhAT
-----------------------------------------------------------------------------*/
class WxPhAT : public WxTrackableFrame, public FNotifyHook, public FSerializableObject, public FDockingParent
{
public:
	WxPhATToolBar*						ToolBar;
	WxPhATMenuBar*						MenuBar;
	WxPhATPreview*						PreviewWindow;
	WxTreeCtrl*							TreeCtrl;

	TMap<wxTreeItemId,FPhATTreeBoneItem> TreeItemBodyIndexMap;
	TMap<wxTreeItemId,INT>				TreeItemConstraintIndexMap;
	TMap<wxTreeItemId,INT>				TreeItemBoneIndexMap;

	WxPropertyWindowHost*				PropertyWindow;

	FPhATViewportClient*				PhATViewportClient;

	FRBPhysScene*						RBPhysScene;

	UPhysicsAsset*						PhysicsAsset;
	TArray<FBoneVertInfo>				DominantWeightBoneInfos;
	TArray<FBoneVertInfo>				AnyWeightBoneInfos;
	TArray<INT>							ControlledBones; // Array of graphics bone indices that are controlled by currently selected body.
	TArray<INT>							NoCollisionBodies; // Array of physics bodies that have no collision with selected body.

	UStaticMeshComponent*				EditorFloorComp;
	UPhATSkeletalMeshComponent*			EditorSkelComp;
	UAnimNodeSequence*					EditorSeqNode;
	USkeletalMesh*						EditorSkelMesh;
	class URB_Handle*					MouseHandle;
	UPhATSimOptions*					EditorSimOptions;

	class WxPropertyWindowFrame*		SimOptionsWindow;

	// We have a different set of view setting per editing mode.
	EPhATRenderMode						BodyEdit_MeshViewMode;
	EPhATRenderMode						BodyEdit_CollisionViewMode;
	EPhATConstraintViewMode				BodyEdit_ConstraintViewMode;

	EPhATRenderMode						ConstraintEdit_MeshViewMode;
	EPhATRenderMode						ConstraintEdit_CollisionViewMode;
	EPhATConstraintViewMode				ConstraintEdit_ConstraintViewMode;

	EPhATRenderMode						Sim_MeshViewMode;
	EPhATRenderMode						Sim_CollisionViewMode;
	EPhATConstraintViewMode				Sim_ConstraintViewMode;

	UBOOL								bShowHierarchy;
	UBOOL								bShowContacts;
	UBOOL								bShowCOM;
	UBOOL								bShowInfluences;
	UBOOL								bDrawGround;
	UBOOL								bShowFixedStatus;
	UBOOL								bShowAnimSkel;

	UBOOL								bSnap;
	UBOOL								bSelectionLock;
	UBOOL								bRunningSimulation;
	UBOOL								bShowInstanceProps;

	EPhATMovementMode					MovementMode;
	EPhATEditingMode					EditingMode;
	EPhATMovementSpace					MovementSpace;

	FLOAT								TotalTickTime;
	FLOAT								LastPokeTime;


	// Collision editing
	INT									SelectedBodyIndex;
	EKCollisionPrimitiveType			SelectedPrimitiveType;
	INT									SelectedPrimitiveIndex;

	EPhATNextSelectEvent				NextSelectEvent;

	// Constraint editing
	INT									SelectedConstraintIndex;

	// Manipulation (rotate, translate, scale)
	FMatrix								WidgetTM;
	UBOOL								bManipulating;
	UBOOL								bNoMoveCamera;
	EAxis								ManipulateAxis;
	FVector								ManipulateDir; // ELEMENT SPACE direction that we want to manipulat the thing in.
	FMatrix								ManipulateMatrix;
	FLOAT								ManipulateTranslation;
	FLOAT								ManipulateRotation;
	FLOAT								DragDirX;
	FLOAT								DragDirY;
	FLOAT								DragX;
	FLOAT								DragY;
	FLOAT								CurrentScale;

	FMatrix								StartManRelConTM;
	FMatrix								StartManParentConTM;
	FMatrix								StartManChildConTM;

	// Simulation mouse forces
	FLOAT								SimGrabPush;
	FLOAT								SimGrabMinPush;
	FVector								SimGrabLocation;
	FVector								SimGrabX;
	FVector								SimGrabY;
	FVector								SimGrabZ;

	// Constructor.
	WxPhAT( wxWindow* InParent, wxWindowID InID, class UPhysicsAsset* InAsset );
	~WxPhAT();

	/**
	 * This function is called when the window has been selected from within the ctrl + tab dialog.
	 */
	virtual void OnSelected();

	/**
	* Called once to load PhAT settings, including window position.
	*/
	void LoadSettings();

	/**
	* Writes out values we want to save to the INI file.
	*/
	void SaveSettings();

	// wxWidgets Events
	void OnClose( wxCloseEvent& In );
	void OnHandleCommand( wxCommandEvent& In );
	void OnUpdateUI( wxUpdateUIEvent& In );
	void OnFillTree(wxCommandEvent &In);

	// FSerializableObject interface
	void Serialize(FArchive& Ar);


	// FNotify interface

	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// WxPhAT interface
		
	void ToggleSelectionLock();

	void ToggleEditingMode();
	void ToggleMovementSpace();
	void SetMovementMode(EPhATMovementMode NewMovementMode);
	void CycleMovementMode();
	void ToggleSnap();
	
	void UpdateToolBarStatus(); // Update enabled status of ToolBar.

	void ChangeDefaultSkelMesh();
	void RecalcEntireAsset();
	void CopyPropertiesToNextSelect(); // Works for bodies or constraints.
	void ToggleInstanceProperties();
	void ShowSimOptionsWindow();
	void FillTree();

	void Undo();
	void Redo();

	// Selection

	void HitBone( INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex );
	void HitConstraint( INT ConstraintIndex );
	void HitNothing();
	void OnTreeSelChanged( wxTreeEvent& In );
	void OnTreeItemDblClick(wxTreeEvent& InEvent);

	/** Displays a context menu for the bone tree. */
	void OnTreeItemRightClick( wxTreeEvent& In );


	////////////////////// Rendering support //////////////////////
	void CycleMeshViewMode();
	void CycleCollisionViewMode();
	void CycleConstraintViewMode();
	void ToggleViewCOM();
	void ToggleViewHierarchy();
	void ToggleViewContacts();

	void ToggleDrawGround();
	void ToggleShowFixed();
	void ToggleViewInfluences();
	void UpdateControlledBones();
	void CenterViewOnSelected();

	EPhATRenderMode GetCurrentMeshViewMode();
	EPhATRenderMode GetCurrentCollisionViewMode();
	EPhATConstraintViewMode GetCurrentConstraintViewMode();

	// Low level
	void ReattachPhATComponent();
	void SetCurrentMeshViewMode(EPhATRenderMode NewViewMode);
	void SetCurrentCollisionViewMode(EPhATRenderMode  NewViewMode);
	void SetCurrentConstraintViewMode(EPhATConstraintViewMode  NewViewMode);

	void DrawCurrentWidget(const FSceneView* View, FPrimitiveDrawInterface* PDI, UBOOL bHitTest);

	////////////////////// Constraint editing //////////////////////
	void SetSelectedConstraint(INT ConstraintIndex);

	FMatrix GetSelectedConstraintWorldTM(INT BodyIndex);
	void SetSelectedConstraintRelTM(const FMatrix& RelTM);
	void SnapSelectedConstraintToBone();
	void SnapAllConstraintsToBone();
	void SnapConstraintToBone(INT ConstraintIndex, const FMatrix& WParentFrame);
	void CycleSelectedConstraintOrientation();

	FMatrix GetConstraintMatrix(INT ConstraintIndex, INT BodyIndex, FLOAT Scale);

	void DrawConstraint(INT ConstraintIndex, const FSceneView* View, FPrimitiveDrawInterface* PDI, UBOOL bDrawAsPoint);

	void CreateOrConvertConstraint(URB_ConstraintSetup* NewSetup);
	void DeleteCurrentConstraint();

	void CopyConstraintProperties(INT ToConstraintIndex, INT FromConstraintIndex);

	void GetConstraintIndicesBelow(TArray<INT>& OutConstraintIndices, FName InBoneName);
	void ToggleSelectedConstraintMotorised();
	void SetConstraintsBelowSelectedMotorised(UBOOL bMotorised);

	////////////////////// Collision geometry editing //////////////////////
	void SetSelectedBody(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex);
	void SetSelectedBodyAnyPrim(INT BodyIndex);

	void StartManipulating(EAxis Axis, const FViewportClick& ViewportClick, const FMatrix& WorldToCamera);
	void UpdateManipulation(FLOAT DeltaX, FLOAT DeltaY, UBOOL bCtrlDown);
	void EndManipulating();

	void ModifyPrimitiveSize(INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FVector DeltaSize);
	void AddNewPrimitive(EKCollisionPrimitiveType PrimitiveType, UBOOL bCopySelected = false);
	void DeleteCurrentPrim();
	void DeleteBody(INT DelBodyIndex);
	
	FMatrix GetPrimitiveMatrix(FMatrix& BoneTM, INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FLOAT Scale);
	FColor GetPrimitiveColor(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex);
	UMaterialInterface* GetPrimitiveMaterial(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex);

	void UpdateNoCollisionBodies();
	void DrawCurrentInfluences(FPrimitiveDrawInterface* PDI);

	void DisableCollisionWithNextSelect();
	void EnableCollisionWithNextSelect();
	void SetCollisionBetween(INT Body1Index, INT Body2Index, UBOOL bEnableCollision);
	void ResetBoneCollision(INT BoneIndex);

	void WeldBodyToNextSelect();
	void WeldBodyToSelected(INT AddBodyIndex);

	void MakeNewBodyFromNextSelect();
	void MakeNewBody(INT NewBoneIndex);

	void CopyBodyProperties(INT ToBodyIndex, INT FromBodyIndex);
	void SetAssetPhysicalMaterial();
	void CopyJointSettingsToAll();

	void ToggleSelectedBodyFixed();
	void SetBodiesBelowSelectedFixed(UBOOL bFix);

	void DeleteBodiesBelowSelected();

	////////////////////// Simulation //////////////////////

	void ToggleSimulation();
	void ViewContactsToggle();

	// Simulation mouse forces
	void SimMousePress(FViewport* Viewport, UBOOL bConstrainRotation, FName Key);
	void SimMouseMove(FLOAT DeltaX, FLOAT DeltaY);
	void SimMouseRelease();
	void SimMouseWheelUp();
	void SimMouseWheelDown();

	////////////////////// Animation //////////////////////
	void AnimComboSelected();
	void ToggleAnimPlayback();
	void SetAnimPlayback(UBOOL bPlayAnim);
	void UpdateAnimCombo();
	void ToggleShowAnimSkel();
	void UpdatePhysBlend();

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


	DECLARE_EVENT_TABLE()

private:

	/**
	 * Helper method to initialize a constraint setup between two bodies.
	 *
	 * @param	ConstraintSetup		Constraint setup to initialize
	 * @param	ChildBodyIndex		Index of the child body in the physics asset body setup array
	 * @param	ParentBodyIndex		Index of the parent body in the physics asset body setup array
	 */
	void InitConstraintSetup( URB_ConstraintSetup* ConstraintSetup, INT ChildBodyIndex, INT ParentBodyIndex );
};

/*-----------------------------------------------------------------------------
	WxDlgNewPhysicsAsset.
-----------------------------------------------------------------------------*/

class WxDlgNewPhysicsAsset : public wxDialog
{
public:
	WxDlgNewPhysicsAsset();
	virtual ~WxDlgNewPhysicsAsset();

	FPhysAssetCreateParams Params;
	USkeletalMesh* Mesh;
	UBOOL bOpenAssetNow;

	wxTextCtrl *MinSizeEdit;
	wxCheckBox *AlignBoneCheck, *OpenAssetNowCheck, *MakeJointsCheck, *WalkPastSmallCheck, *BodyForAllCheck;
	wxComboBox *GeomTypeCombo, *VertWeightCombo;

	int ShowModal( USkeletalMesh* InMesh, UBOOL bReset );
private:
	using wxDialog::ShowModal;		// Hide parent implementation
public:

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};


#endif // __PHAT_H__
