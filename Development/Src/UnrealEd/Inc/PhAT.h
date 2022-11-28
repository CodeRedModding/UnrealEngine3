/*=============================================================================
	PhAT.h: Physics Asset Tool main header
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

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
	BoneIndex(InBoneIndex) {}
};

//
//	FPhATViewportClient
//
struct FPhATViewportClient: public FEditorLevelViewportClient
{
	class WPhAT*			AssetEditor;

	ULevel*						Level;
	UDirectionalLightComponent*	DirectionalLightComponent;
	USkyLightComponent*			SkyLightComponent;

	UFont*					PhATFont;

	UMaterialInstance*		ElemSelectedMaterial;
	UMaterialInstance*		BoneSelectedMaterial;
	UMaterialInstance*		BoneUnselectedMaterial;
	UMaterialInstance*		BoneNoCollisionMaterial;

	UMaterialInstance*		JointLimitMaterial;

	FPhATViewportClient(WPhAT* InAssetEditor);
	~FPhATViewportClient();

	virtual FScene* GetScene() { return Level; }
	virtual ULevel* GetLevel() { return Level; }
	virtual FColor GetBackgroundColor();
	virtual void CalcSceneView(FSceneView* View);
	virtual void InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime);

	virtual void Draw(FChildViewport* Viewport, FRenderInterface* RI);

	virtual void Serialize(FArchive& Ar);

	virtual void DoHitTest(FChildViewport* Viewport, FName Key, EInputEvent Event);

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

//
//	WPhAT
//
class WPhAT : public WWindow, public FNotifyHook
{
	DECLARE_WINDOWCLASS(WPhAT,WWindow,UnrealEd)

	HWND hWndToolBar; // Main ToolBar.
	WToolTip* ToolTipCtrl;

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer*							Container;
	WLabel*								WindowLabel; // Overall window
	WObjectProperties*					PropertyWindow;

	FPhATViewportClient*				PhATViewportClient;

	UPhysicsAsset*						PhysicsAsset;
	TArray<FBoneVertInfo>				DominantWeightBoneInfos;
	TArray<FBoneVertInfo>				AnyWeightBoneInfos;
	TArray<INT>							ControlledBones; // Array of graphics bone indices that are controlled by currently selected body.
	TArray<INT>							NoCollisionBodies; // Array of physics bodies that have no collision with selected body.

	ULevel*								EditorLevel;
	APhATActor*							EditorActor;
	APhATFloor*							EditorFloor;
	UPhATSkeletalMeshComponent*			EditorSkelComp;
	USkeletalMesh*						EditorSkelMesh;
	UPhATSimOptions*					EditorSimOptions;

	FVector								EditorActorStartLocation;

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
	UBOOL								bShowCOM;
	UBOOL								bShowInfluences;
	UBOOL								bDrawGround;

	UBOOL								bSnap;
	UBOOL								bSelectionLock;
	UBOOL								bRunningSimulation;

	EPhATMovementMode					MovementMode;
	EPhATEditingMode					EditingMode;
	EPhATMovementSpace					MovementSpace;

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
	WPhAT(UPhysicsAsset* InAsset);

	// WWindow interface

	void Serialize(FArchive& Ar);
	void OpenWindow();
	void OnCreate();
	void OnDestroy();
	void OnCommand( INT Command );
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void OnPaint();
	void PositionChildControls();

	// FNotify interface

	void NotifyDestroy( void* Src );
	void NotifyPreChange( void* Src, UProperty* PropertyAboutToChange );
	void NotifyPostChange( void* Src, UProperty* PropertyThatChanged );
	void NotifyExec( void* Src, const TCHAR* Cmd );

	// WPhAT interface
		
	void ToggleSelectionLock();

	void ToggleEditingMode();
	void ToggleMovementSpace();
	void SetMovementMode(EPhATMovementMode NewMovementMode);
	void ToggleSnap();
	
	void UpdateToolBarStatus(); // Update enabled status of ToolBar.

	void RecalcEntireAsset();
	void CopyPropertiesToNextSelect(); // Works for bodies or constraints.

	void Undo();
	void Redo();

	////////////////////// Rendering support //////////////////////
	void CycleMeshViewMode();
	void CycleCollisionViewMode();
	void CycleConstraintViewMode();
	void ToggleViewCOM();
	void ToggleViewHierarchy();
	void ToggleViewContacts();

	void ToggleDrawGround();
	void ToggleViewInfluences();
	void UpdateControlledBones();

	EPhATRenderMode GetCurrentMeshViewMode();
	EPhATRenderMode GetCurrentCollisionViewMode();
	EPhATConstraintViewMode GetCurrentConstraintViewMode();

	// Low level
	void SetCurrentMeshViewMode(EPhATRenderMode NewViewMode);
	void SetCurrentCollisionViewMode(EPhATRenderMode  NewViewMode);
	void SetCurrentConstraintViewMode(EPhATConstraintViewMode  NewViewMode);

	void DrawCurrentWidget(const FSceneContext& Context, FPrimitiveRenderInterface* PRI, UBOOL bHitTest);

	////////////////////// Constraint editing //////////////////////
	void SetSelectedConstraint(INT ConstraintIndex);

	FMatrix GetSelectedConstraintWorldTM(INT BodyIndex);
	void SetSelectedConstraintRelTM(const FMatrix& RelTM);
	void CycleSelectedConstraintOrientation();

	FMatrix GetConstraintMatrix(INT ConstraintIndex, INT BodyIndex, FLOAT Scale);

	void DrawConstraint(INT ConstraintIndex, FLOAT Scale, FPrimitiveRenderInterface* PRI);

	void CreateOrConvertConstraint(URB_ConstraintSetup* NewSetup);
	void DeleteCurrentConstraint();

	void CopyConstraintProperties(INT ToConstraintIndex, INT FromConstraintIndex);

	////////////////////// Collision geometry editing //////////////////////
	void SetSelectedBody(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex);
	void SetSelectedBodyAnyPrim(INT BodyIndex);

	void StartManipulating(EAxis Axis, const FViewportClick& ViewportClick, const FMatrix& WorldToCamera);
	void UpdateManipulation(FLOAT DeltaX, FLOAT DeltaY, UBOOL bCtrlDown);
	void EndManipulating();

	void ModifyPrimitiveSize(INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FVector DeltaSize);
	void AddNewPrimitive(EKCollisionPrimitiveType PrimitiveType, UBOOL bCopySelected = false);
	void DeleteCurrentPrim();
	
	FMatrix GetPrimitiveMatrix(FMatrix& BoneTM, INT BodyIndex, EKCollisionPrimitiveType PrimType, INT PrimIndex, FLOAT Scale);
	FColor GetPrimitiveColor(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex);
	UMaterialInstance* GetPrimitiveMaterial(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex);

	void UpdateNoCollisionBodies();
	void DrawCurrentInfluences(FPrimitiveRenderInterface* PRI);

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

	////////////////////// Simulation //////////////////////

	void ToggleSimulation();
	void TickSimulation(FLOAT DeltaSeconds);

	// Simulation mouse forces
	void SimMousePress(FChildViewport* Viewport, UBOOL bConstrainRotation);
	void SimMouseMove(FLOAT DeltaX, FLOAT DeltaY);
	void SimMouseRelease();
	void SimMouseWheelUp();
	void SimMouseWheelDown();
};

/*-----------------------------------------------------------------------------
	WxDlgNewPhysicsAsset.
-----------------------------------------------------------------------------*/

class WxDlgNewPhysicsAsset : public wxDialog
{
public:
	WxDlgNewPhysicsAsset();
	~WxDlgNewPhysicsAsset();

	FString Package, Name;
	FPhysAssetCreateParams Params;
	USkeletalMesh* Mesh;
	UBOOL bOpenAssetNow;

	wxTextCtrl *PackageEdit, *NameEdit, *MinSizeEdit;
	wxCheckBox *AlignBoneCheck, *OpenAssetNowCheck, *MakeJointsCheck, *WalkPastSmallCheck;
	wxComboBox *GeomTypeCombo, *VertWeightCombo;

	int ShowModal( FString InPackage, FString InName, USkeletalMesh* InMesh, UBOOL bReset );

	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgSetAssetPhysMaterial.
-----------------------------------------------------------------------------*/

class WxDlgSetAssetPhysMaterial : public wxDialog
{
public:
	WxDlgSetAssetPhysMaterial();
	~WxDlgSetAssetPhysMaterial();

	UClass* PhysMaterialClass;
	wxComboBox *PhysMaterialCombo;

	int ShowModal();
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	WxDlgSetAssetDisableParams.
-----------------------------------------------------------------------------*/

class WxDlgSetAssetDisableParams : public wxDialog
{
public:
	WxDlgSetAssetDisableParams();
	~WxDlgSetAssetDisableParams();

	FLOAT LinVel, LinAcc, AngVel, AngAcc;
	wxTextCtrl *LinVelEdit, *LinAccEdit, *AngVelEdit, *AngAccEdit;

	int ShowModal(FLOAT InLinVel, FLOAT InLinAcc, FLOAT InAngVel, FLOAT InAngAcc);
	void OnOK( wxCommandEvent& In );

	DECLARE_EVENT_TABLE()
};