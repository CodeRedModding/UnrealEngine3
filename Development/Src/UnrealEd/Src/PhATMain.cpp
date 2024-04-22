/*=============================================================================
	PhATMain.cpp: Physics Asset Tool main windows code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PhAT.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "..\..\Launch\Resources\resource.h"
#include "MouseDeltaTracker.h"
#include "PropertyWindow.h"

IMPLEMENT_CLASS(UPhATSkeletalMeshComponent);
IMPLEMENT_CLASS(UPhATSimOptions);

static const FLOAT DefaultFloorGap = 25.0f; // Default distance from bottom of asset to floor. Unreal units.
static const FLOAT	PhAT_LightRotSpeed = 40.0f;

/*-----------------------------------------------------------------------------
	WxPhATPreview
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxPhATPreview, wxWindow )
EVT_SIZE( WxPhATPreview::OnSize )
END_EVENT_TABLE()

WxPhATPreview::WxPhATPreview( wxWindow* InParent, wxWindowID InID, class WxPhAT* InPhAT  )
: wxWindow( InParent, InID )
{
	PhATPreviewVC = new FPhATViewportClient(InPhAT);
	PhATPreviewVC->Viewport = GEngine->Client->CreateWindowChildViewport(PhATPreviewVC, (HWND)GetHandle());
	PhATPreviewVC->Viewport->CaptureJoystickInput(false);
}

WxPhATPreview::~WxPhATPreview()
{
	GEngine->Client->CloseViewport(PhATPreviewVC->Viewport);
	PhATPreviewVC->Viewport = NULL;
	delete PhATPreviewVC;
}

void WxPhATPreview::OnSize( wxSizeEvent& In )
{
	wxRect rc = GetClientRect();
	::MoveWindow( (HWND)PhATPreviewVC->Viewport->GetWindow(), 0, 0, rc.GetWidth(), rc.GetHeight(), 1 );
}

/*-----------------------------------------------------------------------------
	WxPhAT
-----------------------------------------------------------------------------*/

/**
 * Versioning Info for the Docking Parent layout file.
 */
namespace
{
	static const TCHAR* PhAT_DockingParent_Name = TEXT("PhAT");
	static const INT PhAT_DockingParent_Version = 0;		//Needs to be incremented every time a new dock window is added or removed from this docking parent.
}


BEGIN_EVENT_TABLE( WxPhAT, WxTrackableFrame )
	EVT_CLOSE( WxPhAT::OnClose )
	EVT_TOOL( IDMN_PHAT_TRANSLATE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ROTATE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SCALE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SNAP, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_COPYPROPERTIES, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_INSTANCEPROPS, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_DRAWGROUND, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SHOWFIXED, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SHOWHIERARCHY, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SHOWCONTACTS, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SHOWINFLUENCES, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SHOWCOM, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_DISABLECOLLISION, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ENABLECOLLISION, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_WELDBODIES, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDNEWBODY, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDSPHERE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDSPHYL, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDBOX, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_DELETEPRIM, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_DUPLICATEPRIM, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_RESETCONFRAME, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SNAPCONTOBONE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SNAPALLCONTOBONE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDBS, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDHINGE, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDPRISMATIC, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_ADDSKELETAL, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_DELETECONSTRAINT, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_TOGGLEPLAYANIM, WxPhAT::OnHandleCommand )
	EVT_COMBOBOX( IDMN_PHAT_ANIMCOMBO, WxPhAT::OnHandleCommand )
	EVT_TOOL( IDMN_PHAT_SHOWANIMSKEL, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_EDITMODE, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_MOVESPACE, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_RUNSIM, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_MESHVIEWMODE, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_COLLISIONVIEWMODE, WxPhAT::OnHandleCommand )
	EVT_BUTTON( IDMN_PHAT_CONSTRAINTVIEWMODE, WxPhAT::OnHandleCommand )
	EVT_MENU( ID_PHAT_UNDO, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_REDO, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_CHANGESKELMESH, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_RESETASSET, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_RESETBONE, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_SETMATERIAL, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_COPYTOALLJOINTS, WxPhAT::OnHandleCommand)
	EVT_MENU( ID_PHAT_SHOWSIMOPTIONS, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_TOGGLEFIXED, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_FIXBELOW, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_UNFIXBELOW, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_DELETEBELOW, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_TOGGLEMOTORISE, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_MOTORISEBELOW, WxPhAT::OnHandleCommand)
	EVT_MENU( IDM_PHAT_UNMOTORISEBELOW, WxPhAT::OnHandleCommand)

	EVT_TREE_ITEM_RIGHT_CLICK( ID_PHAT_TREECTRL, WxPhAT::OnTreeItemRightClick )

	EVT_UPDATE_UI(ID_PHAT_SETMATERIAL, WxPhAT::OnUpdateUI)
	EVT_TREE_ITEM_ACTIVATED( ID_PHAT_TREECTRL, WxPhAT::OnTreeItemDblClick )
	EVT_TREE_SEL_CHANGED(ID_PHAT_TREECTRL, WxPhAT::OnTreeSelChanged )
END_EVENT_TABLE()

WxPhAT::WxPhAT( wxWindow* InParent, wxWindowID InID, class UPhysicsAsset* InAsset )
: WxTrackableFrame( InParent, InID, *LocalizeUnrealEd("PhAT"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_FRAME_STYLE | wxFRAME_FLOAT_ON_PARENT | wxFRAME_NO_TASKBAR ),
  FDockingParent( this )
{
	wxEvtHandler::Connect(wxID_ANY, ID_PHAT_FILLTREE, wxCommandEventHandler(WxPhAT::OnFillTree));

	PhysicsAsset = InAsset;

	PropertyWindow = NULL;
	PhATViewportClient = NULL;

	EditorFloorComp = NULL;
	EditorSkelComp = NULL;
	EditorSeqNode = NULL;

	// Editor variables
	BodyEdit_MeshViewMode = PRM_Solid;
	BodyEdit_CollisionViewMode = PRM_Wireframe;
	BodyEdit_ConstraintViewMode = PCV_AllPositions;

	ConstraintEdit_MeshViewMode = PRM_None;
	ConstraintEdit_CollisionViewMode = PRM_Wireframe;
	ConstraintEdit_ConstraintViewMode = PCV_AllPositions;

	Sim_MeshViewMode = PRM_Solid;
	Sim_CollisionViewMode = PRM_Wireframe;
	Sim_ConstraintViewMode = PCV_None;

	MovementMode = PMM_Translate;
	MovementSpace = PMS_Local;
	EditingMode = PEM_BodyEdit;

	TotalTickTime = 0.f;
	LastPokeTime = 0.f;

	NextSelectEvent = PNS_Normal;

	bShowCOM = FALSE;
	bShowHierarchy = FALSE;
	bShowContacts = FALSE;
	bShowInfluences = FALSE;
	bDrawGround = FALSE;
	bShowFixedStatus = FALSE;
	bShowAnimSkel = FALSE;

	bSelectionLock = FALSE;
	bSnap = FALSE;

	bRunningSimulation = FALSE;
	bManipulating = FALSE;
	ManipulateAxis = AXIS_None;
	bNoMoveCamera = FALSE;
	bShowInstanceProps = FALSE;

	SimOptionsWindow = NULL;

	// Construct mouse handle
	MouseHandle = ConstructObject<URB_Handle>( URB_Handle::StaticClass() );

	// Construct sim options.
	EditorSimOptions = ConstructObject<UPhATSimOptions>( UPhATSimOptions::StaticClass() );
	check(EditorSimOptions);

	EditorSimOptions->HandleLinearDamping = MouseHandle->LinearDamping;
	EditorSimOptions->HandleLinearStiffness = MouseHandle->LinearStiffness;
	EditorSimOptions->HandleAngularDamping = MouseHandle->AngularDamping;
	EditorSimOptions->HandleAngularStiffness = MouseHandle->AngularStiffness;

	RBPhysScene = CreateRBPhysScene( FVector(0, 0, 0) );


	// Create Toolbar
	ToolBar = new WxPhATToolBar( this, -1 );
	SetToolBar(ToolBar);

	// Create 3D preview viewport.
	PreviewWindow = new WxPhATPreview( this, -1, this );
	PhATViewportClient = PreviewWindow->PhATPreviewVC;

	// Init physics for the floor box.
	FMatrix PrimTM = EditorFloorComp->LocalToWorld;
	PrimTM.RemoveScaling();

	EditorFloorComp->BodyInstance = ConstructObject<URB_BodyInstance>( URB_BodyInstance::StaticClass() );
	EditorFloorComp->BodyInstance->InitBody(EditorFloorComp->StaticMesh->BodySetup, PrimTM, FVector(4.f), true, EditorFloorComp, RBPhysScene);

	AddDockingWindow( PreviewWindow, FDockingParent::DH_None, NULL );


	// Create property window.
	PropertyWindow = new WxPropertyWindowHost;
	PropertyWindow->Create( this, this );
	AddDockingWindow(PropertyWindow, FDockingParent::DH_Right, *LocalizeUnrealEd(TEXT("Properties")));

	// Create tree control
	TreeCtrl = new WxTreeCtrl;
	TreeCtrl->Create( this, ID_PHAT_TREECTRL, NULL, wxTR_HAS_BUTTONS|wxTR_MULTIPLE|wxTR_LINES_AT_ROOT );
	AddDockingWindow(TreeCtrl, FDockingParent::DH_Right, *LocalizeUnrealEd(TEXT("Tree")));

	// Try to load a existing layout for the docking windows.
	LoadDockingLayout();


	// Create MenuBar
	MenuBar = new WxPhATMenuBar( this );
	AppendWindowMenu(MenuBar);
	SetMenuBar(MenuBar);

	
	if(PhysicsAsset->DefaultSkelMesh == NULL)
	{
		// Fall back to the default skeletal mesh in the EngineMeshes package.
		// This is statically loaded as the package is likely not fully loaded
		// (otherwise, it would have been found in the above iteration).
		USkeletalMesh* DefaultSkelMesh = (USkeletalMesh*)UObject::StaticLoadObject(
			USkeletalMesh::StaticClass(), NULL, TEXT("EngineMeshes.SkeletalCube"), NULL, LOAD_None, NULL);
		check(DefaultSkelMesh);

		appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd("Error_PhysicsAssetHasNoSkelMesh"), *DefaultSkelMesh->GetFullName() )));	

		PhysicsAsset->DefaultSkelMesh = DefaultSkelMesh;
	}

	check(PhysicsAsset->DefaultSkelMesh);
	EditorSkelMesh = PhysicsAsset->DefaultSkelMesh;

	// Look for body setups with no shapes (how does this happen?).
	// If we find one- just bang on a default box.
	UBOOL bFoundEmptyShape = FALSE;
	for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
	{
		URB_BodySetup* BS = PhysicsAsset->BodySetup(i);
		if(BS->AggGeom.GetElementCount() == 0)
		{
			BS->AggGeom.BoxElems.AddZeroed(1);
			check(BS->AggGeom.BoxElems.Num() == 1);
			FKBoxElem& Box = BS->AggGeom.BoxElems(0);
			Box.TM = FMatrix::Identity;
			Box.X = 15.f;
			Box.Y = 15.f;
			Box.Z = 15.f;

			bFoundEmptyShape = TRUE;
		}
	}

	// Pop up a warning about what we did.
	if(bFoundEmptyShape)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("EmptyBodyFound") );
	}


	// Used for viewing bone influences, resetting bone geometry etc.
	EditorSkelMesh->CalcBoneVertInfos(DominantWeightBoneInfos, true);
	EditorSkelMesh->CalcBoneVertInfos(AnyWeightBoneInfos, false);

	EditorSkelComp->SetSkeletalMesh( EditorSkelMesh );
	EditorSkelComp->SetPhysicsAsset( PhysicsAsset );

	// Ensure PhysicsAsset mass properties are up to date.
	PhysicsAsset->UpdateBodyIndices();
	PhysicsAsset->FixOuters();

	// Update sim options to match asset
	EditorSimOptions->AngularSpringScale = PhysicsAsset->DefaultInstance->AngularSpringScale;
	EditorSimOptions->AngularDampingScale = PhysicsAsset->DefaultInstance->AngularDampingScale;

	// Check if there are any bodies in the Asset which do not have bones in the skeletal mesh.
	// If so, put up a warning.
	FString MissingBones;
	for(INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
	{
		FName BoneName = PhysicsAsset->BodySetup(i)->BoneName;
		INT BoneIndex = EditorSkelMesh->MatchRefBone(BoneName);
		if(BoneIndex == INDEX_NONE)
		{
			MissingBones += FString::Printf( TEXT("\t%s\n"), *BoneName.ToString() );
		}
	}

	if(MissingBones.Len() > 0)
	{
		appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("BodyMissingFromSkelMesh"), *MissingBones) );
	}


	// Get actors asset collision bounding box, and move actor so its not intersection the floor plane at Z = 0.
	FBox CollBox = PhysicsAsset->CalcAABB( EditorSkelComp );	
	FVector SkelCompLocation = FVector(0, 0, -CollBox.Min.Z + EditorSimOptions->FloorGap);

	EditorSkelComp->ConditionalUpdateTransform(FTranslationMatrix(FVector(SkelCompLocation)));

	// Get new bounding box and set view based on that.
	CollBox = PhysicsAsset->CalcAABB( EditorSkelComp );	
	FVector CollBoxExtent = CollBox.GetExtent();

	// Take into account internal mesh translation/rotation/scaling etc.
	FMatrix LocalToWorld = EditorSkelComp->LocalToWorld;
	FSphere WorldSphere = EditorSkelMesh->Bounds.GetSphere().TransformBy(LocalToWorld);

	CollBoxExtent = CollBox.GetExtent();
	if(CollBoxExtent.X > CollBoxExtent.Y)
	{
		PhATViewportClient->ViewLocation = FVector(WorldSphere.Center.X, WorldSphere.Center.Y - 1.5*WorldSphere.W, WorldSphere.Center.Z);
		PhATViewportClient->ViewRotation = FRotator(0,16384,0);	
	}
	else
	{
		PhATViewportClient->ViewLocation = FVector(WorldSphere.Center.X - 1.5*WorldSphere.W, WorldSphere.Center.Y, WorldSphere.Center.Z);
		PhATViewportClient->ViewRotation = FRotator(0,0,0);	
	}

	PhATViewportClient->UpdateLighting();

	// Fill the tree with all bodies
	FillTree();

	// Initially nothing selected.
	SetSelectedBody( INDEX_NONE, KPT_Unknown, INDEX_NONE );
	SetSelectedConstraint( INDEX_NONE );

	SetMovementMode( PMM_Translate );

	ToggleDrawGround();

	UpdateToolBarStatus();

	PhATViewportClient->SetViewLocationForOrbiting( FVector(0.f,0.f,0.f) );

	// Load Window Settings.
	LoadSettings();
}

WxPhAT::~WxPhAT()
{
	// Save out window settings.
	SaveSettings();
}

/**
 * This function is called when the window has been selected from within the ctrl + tab dialog.
 */
void WxPhAT::OnSelected()
{
	Raise();
}

/**
* Called once to load PhAT settings, including window position.
*/

void WxPhAT::LoadSettings()
{
	// Load Window Position.
	FWindowUtil::LoadPosSize(TEXT("PhAT"), this, 256,256,1024,768);

	// Load the preview scene
	check( PhATViewportClient );
	PhATViewportClient->PreviewScene.LoadSettings(TEXT("PhAT"));
}

/**
* Writes out values we want to save to the INI file.
*/

void WxPhAT::SaveSettings()
{
	SaveDockingLayout();

	// Save the preview scene
	check( PhATViewportClient );
	PhATViewportClient->PreviewScene.SaveSettings(TEXT("PhAT"));

	// Save Window Position.
	FWindowUtil::SavePosSize(TEXT("PhAT"), this);
}


void WxPhAT::OnClose( wxCloseEvent& In )
{
	// Ensure simulation is not running.
	if(bRunningSimulation)
	{
		ToggleSimulation();
	}

	// Terminate physics for the floor
	EditorFloorComp->BodyInstance = NULL;

	DestroyRBPhysScene(RBPhysScene);

	this->Destroy();
}

void WxPhAT::Serialize(FArchive& Ar)
{
	Ar << PhysicsAsset;
	Ar << EditorSimOptions;

	if(PhATViewportClient)
	{
		PhATViewportClient->Serialize(Ar);
	}

	Ar << MouseHandle;
}

void WxPhAT::OnHandleCommand( wxCommandEvent& In )
{
	INT Command = In.GetId();

	switch( Command )
	{

	////////////////////////////////////// EDITING MODE
	case IDMN_PHAT_EDITMODE:
		ToggleEditingMode();
		break;

	////////////////////////////////////// MOVEMENT SPACE
	case IDMN_PHAT_MOVESPACE:
		ToggleMovementSpace();
		break;

	////////////////////////////////////// MOVEMENT MODE
	case IDMN_PHAT_TRANSLATE:
		SetMovementMode(PMM_Translate);
		break;

	case IDMN_PHAT_ROTATE:
		SetMovementMode(PMM_Rotate);
		break;

	case IDMN_PHAT_SCALE:
		SetMovementMode(PMM_Scale);
		break;

	case IDMN_PHAT_SNAP:
		ToggleSnap();
		break;

	case IDMN_PHAT_COPYPROPERTIES:
		CopyPropertiesToNextSelect();
		break;

	case IDMN_PHAT_INSTANCEPROPS:
		ToggleInstanceProperties();
		break;

	////////////////////////////////////// COLLISION
	case IDMN_PHAT_ADDSPHERE:
		AddNewPrimitive(KPT_Sphere);
		break;

	case IDMN_PHAT_ADDSPHYL:
		AddNewPrimitive(KPT_Sphyl);
		break;

	case IDMN_PHAT_ADDBOX:
		AddNewPrimitive(KPT_Box);
		break;

	case IDMN_PHAT_DUPLICATEPRIM:
		AddNewPrimitive(KPT_Unknown, true);
		break;

	case IDMN_PHAT_DELETEPRIM:
		DeleteCurrentPrim();
		break;

	case IDMN_PHAT_DISABLECOLLISION:
		DisableCollisionWithNextSelect();
		break;

	case IDMN_PHAT_ENABLECOLLISION:
		EnableCollisionWithNextSelect();
		break;

	case IDMN_PHAT_WELDBODIES:
		WeldBodyToNextSelect();
		break;

	case IDMN_PHAT_ADDNEWBODY:
		MakeNewBodyFromNextSelect();
		break;

	////////////////////////////////////// CONSTRAINTS

	case IDMN_PHAT_RESETCONFRAME:
		SetSelectedConstraintRelTM( FMatrix::Identity );
		PhATViewportClient->Viewport->Invalidate();
		break;

	case IDMN_PHAT_SNAPCONTOBONE:
		SnapSelectedConstraintToBone();
		PhATViewportClient->Viewport->Invalidate();
		break;

	case IDMN_PHAT_SNAPALLCONTOBONE:
		SnapAllConstraintsToBone();
		PhATViewportClient->Viewport->Invalidate();
		break;

	case IDMN_PHAT_ADDBS:
		CreateOrConvertConstraint( (URB_ConstraintSetup*)URB_BSJointSetup::StaticClass()->GetDefaultObject() );
		break;

	case IDMN_PHAT_ADDHINGE:
		CreateOrConvertConstraint( (URB_ConstraintSetup*)URB_HingeSetup::StaticClass()->GetDefaultObject()  );
		break;

	case IDMN_PHAT_ADDPRISMATIC:
		CreateOrConvertConstraint( (URB_ConstraintSetup*)URB_PrismaticSetup::StaticClass()->GetDefaultObject()  );
		break;

	case IDMN_PHAT_ADDSKELETAL:
		CreateOrConvertConstraint( (URB_ConstraintSetup*)URB_SkelJointSetup::StaticClass()->GetDefaultObject()  );
		break;

	case IDMN_PHAT_DELETECONSTRAINT:
		DeleteCurrentConstraint();
		break;

	////////////////////////////////////// RENDERING
	case IDMN_PHAT_MESHVIEWMODE:
		CycleMeshViewMode();
		break;

	case IDMN_PHAT_COLLISIONVIEWMODE:
		CycleCollisionViewMode();
		break;

	case IDMN_PHAT_CONSTRAINTVIEWMODE:
		CycleConstraintViewMode();
		break;

	case IDMN_PHAT_DRAWGROUND:
		ToggleDrawGround();
		break;

	case IDMN_PHAT_SHOWFIXED:
		ToggleShowFixed();
		break;

	case IDMN_PHAT_SHOWCOM:
		ToggleViewCOM();
		break;

	case IDMN_PHAT_SHOWHIERARCHY:
		ToggleViewHierarchy();
		break;

	case IDMN_PHAT_SHOWCONTACTS:
		ToggleViewContacts();
		break;

	case IDMN_PHAT_SHOWINFLUENCES:
		ToggleViewInfluences();
		break;

	////////////////////////////////////// ANIM

	case IDMN_PHAT_TOGGLEPLAYANIM:
		ToggleAnimPlayback();
		break;

	case IDMN_PHAT_ANIMCOMBO:
		AnimComboSelected();
		break;

	case IDMN_PHAT_SHOWANIMSKEL:
		ToggleShowAnimSkel();
		break;

	////////////////////////////////////// BODY CONTEXT MENU

	case IDM_PHAT_TOGGLEFIXED:
		ToggleSelectedBodyFixed();
		break;

	case IDM_PHAT_FIXBELOW:
		SetBodiesBelowSelectedFixed(TRUE);
		break;

	case IDM_PHAT_UNFIXBELOW:
		SetBodiesBelowSelectedFixed(FALSE);
		break;

	case IDM_PHAT_DELETEBELOW:
		DeleteBodiesBelowSelected();
		break;

	////////////////////////////////////// CONSTRAINT CONTEXT MENU

	case IDM_PHAT_TOGGLEMOTORISE:
		ToggleSelectedConstraintMotorised();
		break;

	case IDM_PHAT_MOTORISEBELOW:
		SetConstraintsBelowSelectedMotorised(TRUE);
		break;

	case IDM_PHAT_UNMOTORISEBELOW:
		SetConstraintsBelowSelectedMotorised(FALSE);
		break;

	////////////////////////////////////// MISC
	case IDMN_PHAT_RUNSIM:
		ToggleSimulation();
		break;

	case ID_PHAT_UNDO:
		Undo();
		break;

	case ID_PHAT_REDO:
		Redo();
		break;

	case ID_PHAT_CHANGESKELMESH:
		ChangeDefaultSkelMesh();
		break;

	case ID_PHAT_RESETASSET:
		RecalcEntireAsset();
		break;

	case ID_PHAT_RESETBONE:
		ResetBoneCollision(SelectedBodyIndex);
		break;

	case ID_PHAT_SETMATERIAL:
		SetAssetPhysicalMaterial();
		break;

	case ID_PHAT_COPYTOALLJOINTS:
		CopyJointSettingsToAll();
		break;

	case ID_PHAT_SHOWSIMOPTIONS:
		ShowSimOptionsWindow();
		break;
	}

}

void WxPhAT::OnUpdateUI( wxUpdateUIEvent& In )
{
	INT Command = In.GetId();

	switch( Command )
	{
	case ID_PHAT_SETMATERIAL:
		{
			// @todo CB: Needs support for unloaded assets (add a fast AnyAssetsOfClassSelected() method?)
			UPhysicalMaterial* SelectedPhysMaterial = GEditor->GetSelectedObjects()->GetTop<UPhysicalMaterial>();

			if(SelectedPhysMaterial)
			{
				FString MenuText = FString::Printf(LocalizeSecure(LocalizeUnrealEd("ApplyPhysicalAssetMaterialF"), *SelectedPhysMaterial->GetName()));
				In.SetText(*MenuText);
				In.Enable(TRUE);
			}
			else
			{
				In.SetText(*LocalizeUnrealEd("ApplyPhysicalAssetMaterial"));
				In.Enable(FALSE);
			}
		}
		break;

	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// FPhATViewportClient ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


UBOOL FPhATViewportClient::InputKey(FViewport* Viewport, INT ControllerId, FName Key, EInputEvent Event,FLOAT /*AmountDepressed*/,UBOOL /*Gamepad*/)
{
	// Hide and lock mouse cursor if we're capturing mouse input
	Viewport->ShowCursor( !Viewport->HasMouseCapture() );
	Viewport->LockMouseToWindow( Viewport->HasMouseCapture() );

	INT HitX = Viewport->GetMouseX();
	INT HitY = Viewport->GetMouseY();
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	if(AssetEditor->bRunningSimulation)
	{
		if(Key == KEY_RightMouseButton || Key == KEY_LeftMouseButton)
		{
			if(Event == IE_Pressed)
			{
				if(bShiftDown)
				{
					AssetEditor->SimMousePress(Viewport, FALSE, Key);
					AssetEditor->bNoMoveCamera = TRUE;
				}
				else if(bCtrlDown)
				{
					AssetEditor->SimMousePress(Viewport, TRUE, Key);
					AssetEditor->bNoMoveCamera = TRUE;
				}
			}
			else if(Event == IE_Released)
			{
				AssetEditor->SimMouseRelease();
				AssetEditor->bNoMoveCamera = FALSE;
			}
		}
		else if(Key == KEY_MouseScrollUp)
		{
			AssetEditor->SimMouseWheelUp();
		}
		else if(Key == KEY_MouseScrollDown)
		{
			AssetEditor->SimMouseWheelDown();
		}
	}
	else
	{
		// If releasing the mouse button, check we are done manipulating
		if(Key == KEY_LeftMouseButton)
		{
			if( Event == IE_Pressed )
			{
				DoHitTest(Viewport, Key, Event);

				// If we are manipulating, don't move the camera as we drag now.
				if(AssetEditor->bManipulating)
				{
					AssetEditor->bNoMoveCamera = TRUE;
				}

				// Reset drag distance.
				DistanceDragged = 0;
			}
			else if(Event == IE_Released)
			{
				if( AssetEditor->bManipulating )
				{
					AssetEditor->EndManipulating();
				}
				else
				{
					DoHitTest(Viewport, Key, Event);
				}

				AssetEditor->bNoMoveCamera = FALSE;
			}
		}
		// If releasing right mouse button - pop up context menu.
		else if(Key == KEY_RightMouseButton)
		{
			if( Event == IE_Pressed )
			{
				DistanceDragged = 0;
			}
			else if(Event == IE_Released)
			{
				// If mouse has not moved, and we are not in some 'click on next' mode - pop up menu.
				if( DistanceDragged < 4 && AssetEditor->NextSelectEvent == PNS_Normal )
				{
					INT HitX = Viewport->GetMouseX();
					INT HitY = Viewport->GetMouseY();
					HHitProxy*	HitResult = Viewport->GetHitProxy(HitX, HitY);
					if( HitResult )
					{
						if( HitResult->IsA(HPhATBoneProxy::StaticGetType()) && AssetEditor->EditingMode == PEM_BodyEdit)
						{
							HPhATBoneProxy* BoneProxy = (HPhATBoneProxy*)HitResult;

							// Select body under cursor if not selection locked.
							if(!AssetEditor->bSelectionLock)
							{
								AssetEditor->SetSelectedBody( BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex );
							}

							// Pop up menu, if we have a body selected.
							if(AssetEditor->SelectedBodyIndex != INDEX_NONE)
							{
								WxBodyContextMenu menu( AssetEditor );
								FTrackPopupMenu tpm( AssetEditor, &menu );
								tpm.Show();
							}
						}
						else if( HitResult->IsA(HPhATConstraintProxy::StaticGetType()) && AssetEditor->EditingMode == PEM_ConstraintEdit)
						{
							HPhATConstraintProxy* ConstraintProxy = (HPhATConstraintProxy*)HitResult;

							// Select constraint under cursor if not selection locked.
							if(!AssetEditor->bSelectionLock)
							{
								AssetEditor->SetSelectedConstraint( ConstraintProxy->ConstraintIndex );
							}

							// Pop up menu, if we have a constraint selected.
							if(AssetEditor->SelectedConstraintIndex != INDEX_NONE)
							{
								WxConstraintContextMenu menu( AssetEditor );
								FTrackPopupMenu tpm( AssetEditor, &menu );
								tpm.Show();
							}
						}
					}
				}
			}
		}
	}

	if( Event == IE_Pressed) 
	{
		if(Key == KEY_W)
			AssetEditor->SetMovementMode(PMM_Translate);
		else if(Key == KEY_E)
			AssetEditor->SetMovementMode(PMM_Rotate);
		else if(Key == KEY_R)
			AssetEditor->SetMovementMode(PMM_Scale);
		else if(Key == KEY_X)
			AssetEditor->ToggleSelectionLock();
		else if(Key == KEY_Z && bCtrlDown)
			AssetEditor->Undo();
		else if(Key == KEY_Y && bCtrlDown)
			AssetEditor->Redo();
		else if(Key == KEY_Delete)
		{
			if(AssetEditor->EditingMode == PEM_BodyEdit)
				AssetEditor->DeleteCurrentPrim();
			else
				AssetEditor->DeleteCurrentConstraint();
		}
		else if(Key == KEY_S)
			AssetEditor->ToggleSimulation();
		else if(Key == KEY_H)
			AssetEditor->CycleMeshViewMode();
		else if(Key == KEY_J)
			AssetEditor->CycleCollisionViewMode();
		else if(Key == KEY_K)
			AssetEditor->CycleConstraintViewMode();
		else if(Key == KEY_B)
			AssetEditor->ToggleEditingMode();
		else if(Key == KEY_I)
			AssetEditor->ToggleInstanceProperties();
		else if(Key == KEY_A)
			AssetEditor->ToggleSnap();
		else if(Key == KEY_D)
			AssetEditor->WeldBodyToNextSelect();
		else if(Key == KEY_Q)
			AssetEditor->CycleSelectedConstraintOrientation();
		else if(Key == KEY_C)
			AssetEditor->CopyPropertiesToNextSelect();
		else if(Key == KEY_SpaceBar)
			AssetEditor->CycleMovementMode();
		else if(Key == KEY_LeftBracket)
			AssetEditor->EnableCollisionWithNextSelect();
		else if(Key == KEY_RightBracket)
			AssetEditor->DisableCollisionWithNextSelect();
		else if(Key == KEY_Home)
			AssetEditor->CenterViewOnSelected();
			

		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton )
		{
			MouseDeltaTracker->StartTracking( this, HitX, HitY );
		}
	}
	else if( Event == IE_Released )
	{
		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton )
		{
			MouseDeltaTracker->EndTracking( this );
		}
	}

	// Handle viewport screenshot.
	InputTakeScreenshot( Viewport, Key, Event );
	
	return TRUE;
}

UBOOL FPhATViewportClient::InputAxis(FViewport* Viewport, INT ControllerId,  FName Key, FLOAT Delta, FLOAT DeltaTime, UBOOL bGamepad)
{
	if( Key == KEY_MouseX || Key == KEY_MouseY )
	{
		DistanceDragged += Abs(Delta);
	}

	const UBOOL bLightMoveDown = Viewport->KeyState( KEY_L );

	// If we are 'manipulating' don't move the camera but do something else with mouse input.
	if( AssetEditor->bManipulating )
	{
		UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);

		if(AssetEditor->bRunningSimulation)
		{
			if(Key == KEY_MouseX)
				AssetEditor->SimMouseMove(Delta, 0.0f);
			else if(Key == KEY_MouseY)
				AssetEditor->SimMouseMove(0.0f, Delta);
		}
		else
		{
			if(Key == KEY_MouseX)
				AssetEditor->UpdateManipulation(Delta, 0.0f, bCtrlDown);
			else if(Key == KEY_MouseY)
				AssetEditor->UpdateManipulation(0.0f, Delta, bCtrlDown);
		}
	}
	else if(bLightMoveDown)
	{
		// Look at which axis is being dragged and by how much
		const FLOAT DragX = (Key == KEY_MouseX) ? Delta : 0.f;
		const FLOAT DragY = (Key == KEY_MouseY) ? Delta : 0.f;

		FRotator LightDir = PreviewScene.GetLightDirection();

		LightDir.Yaw += -DragX * PhAT_LightRotSpeed;
		LightDir.Pitch += -DragY * PhAT_LightRotSpeed;

		PreviewScene.SetLightDirection( LightDir );
	}
	else if(!AssetEditor->bNoMoveCamera)
	{
		MouseDeltaTracker->AddDelta( this, Key, Delta, 0 );
		const FVector DragDelta = MouseDeltaTracker->GetDelta();

		GEditor->MouseMovement += DragDelta;

		if( !DragDelta.IsZero() )
		{
			// Convert the movement delta into drag/rotation deltas
			if ( bAllowMayaCam && GEditor->bUseMayaCameraControls )
			{
				FVector TempDrag;
				FRotator TempRot;
				InputAxisMayaCam( Viewport, DragDelta, TempDrag, TempRot  );
			}
			else
			{
				FVector Drag;
				FRotator Rot;
				FVector Scale;
				MouseDeltaTracker->ConvertMovementDeltaToDragRot( this, DragDelta, Drag, Rot, Scale );
				MoveViewportCamera( Drag, Rot );
			}

			MouseDeltaTracker->ReduceBy( DragDelta );
		}
	}

	Viewport->Invalidate();

	return TRUE;
}

void FPhATViewportClient::MouseMove(FViewport* Viewport, INT X, INT Y)
{
	// If not manipulating, do a check each frame to highlight the widget when we are over it.
	if(!AssetEditor->bManipulating)
	{
		HHitProxy* HitResult = Viewport->GetHitProxy(X,Y);

		if( HitResult && HitResult->IsA(HPhATWidgetProxy::StaticGetType()) )
		{
			HPhATWidgetProxy* WidgetProxy = (HPhATWidgetProxy*)HitResult;

			AssetEditor->ManipulateAxis = WidgetProxy->Axis;
		}
		else
		{
			AssetEditor->ManipulateAxis = AXIS_None;
		}
	}
}

void FPhATViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	if(AssetEditor->bRunningSimulation)
	{
		FLOAT UseSimSpeed = ::Clamp(AssetEditor->EditorSimOptions->SimSpeed, 0.1f, 2.f);
		FLOAT UseDelta = DeltaSeconds * UseSimSpeed;

		// Update total time
		AssetEditor->TotalTickTime += UseDelta;

		// Update the physics/animation blend.
		AssetEditor->UpdatePhysBlend();

		// Advance the animation if its playing and and update the skeletal pose.
		AssetEditor->EditorSkelComp->TickAnimNodes(UseDelta);

		// This will update any kinematic bones from animation.
		AssetEditor->EditorSkelComp->UpdateSkelPose(); 
		AssetEditor->EditorSkelComp->UpdateRBBonesFromSpaceBases(AssetEditor->EditorSkelComp->LocalToWorld, FALSE, FALSE);

		// We back up the transforms array now, so we can show what the animation is doing without physics for debugging.
		AssetEditor->EditorSkelComp->AnimationSpaceBases = AssetEditor->EditorSkelComp->SpaceBases;

		// Use default WorldInfo to define the gravity and stepping params.
		AWorldInfo* Info = (AWorldInfo*)(AWorldInfo::StaticClass()->GetDefaultObject());
		check(Info);


		// Set gravity from WorldInfo
		FVector Gravity(0, 0, Info->DefaultGravityZ * Info->RBPhysicsGravityScaling * AssetEditor->EditorSimOptions->GravScale );

		AssetEditor->RBPhysScene->SetGravity( Gravity );

		TickRBPhysScene(AssetEditor->RBPhysScene, UseDelta);
		WaitRBPhysScene(AssetEditor->RBPhysScene);

		// Update graphics from the physics now.
		AssetEditor->EditorSkelComp->BlendInPhysics();
		AssetEditor->EditorSkelComp->ConditionalUpdateTransform();

#if WITH_NOVODEX
		ULineBatchComponent* LineBatcher = PreviewScene.GetLineBatcher();
		if( LineBatcher != NULL )
		{
			// remove the novodex debug line draws
			PreviewScene.ClearLineBatcher();
			AssetEditor->RBPhysScene->AddNovodexDebugLines( LineBatcher );
		}
#endif // WITH_NOVODEX	
	}
}


// Properties window NotifyHook stuff

void WxPhAT::NotifyDestroy( void* Src )
{
	if(Src == PropertyWindow)
		PropertyWindow = NULL;

	if(Src == SimOptionsWindow)
		SimOptionsWindow = NULL;
}

void WxPhAT::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	if(!bRunningSimulation)
	{
		if(EditingMode == PEM_BodyEdit)
			GEditor->BeginTransaction( *LocalizeUnrealEd("EditBodyProperties") );
		else
			GEditor->BeginTransaction( *LocalizeUnrealEd("EditConstraintProperties") );
	}

}

void WxPhAT::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	if(!bRunningSimulation)
		GEditor->EndTransaction();

	EditorSimOptions->SaveConfig();

	PhysicsAsset->UpdateBoundsBodiesArray();

	UpdateNoCollisionBodies();
	PhATViewportClient->UpdateLighting();
	PhATViewportClient->Viewport->Invalidate();

	if( PropertyWindow->IsEditingObject(EditorSimOptions) && PropertyThatChanged )
	{
		// If we are editing the anim options - keep skeletal mesh component sync'd with AnimSets.
		FName PropName = PropertyThatChanged->GetFName();
		if(PropName == FName(TEXT("PreviewAnimSet")))
		{
			// Stop playing any animation
			SetAnimPlayback(FALSE);

			// Copy in the components AnimSet array
			EditorSkelComp->AnimSets.Empty();
			EditorSkelComp->AnimSets.AddItem(EditorSimOptions->PreviewAnimSet);
			EditorSkelComp->InitAnimTree();

			// Update combo
			UpdateAnimCombo();

			// Update tool from new combo state.
			AnimComboSelected();
		}
		// Push changes in overall motor stength down to the asset instance
		else if(PropName == FName(TEXT("AngularSpringScale")) || PropName == FName(TEXT("AngularDampingScale")))
		{
			// If running, push it to the running instance
			if(bRunningSimulation)
			{
				check(EditorSkelComp->PhysicsAssetInstance);
				EditorSkelComp->PhysicsAssetInstance->SetAngularDriveScale( EditorSimOptions->AngularSpringScale, EditorSimOptions->AngularDampingScale, 1.f);
			}

			// Store in default physics instance.
			PhysicsAsset->DefaultInstance->SetAngularDriveScale( EditorSimOptions->AngularSpringScale, EditorSimOptions->AngularDampingScale, 1.f);
		}
	}

	if (!bRunningSimulation)
	{
		// Move actor to origin, find how far box goes underground, then shift it up so its FloorGap above the ground.
		EditorSkelComp->ConditionalUpdateTransform(FMatrix::Identity);

		FBox CollBox = PhysicsAsset->CalcAABB( EditorSkelComp );	
		FVector SkelCompLocation = FVector(0, 0,-CollBox.Min.Z + EditorSimOptions->FloorGap);

		EditorSkelComp->ConditionalUpdateTransform( FTranslationMatrix(FVector(SkelCompLocation)) );
	}

}

void WxPhAT::NotifyExec( void* Src, const TCHAR* Cmd )
{
}

/**
 *	This function returns the name of the docking parent.  This name is used for saving and loading the layout files.
 *  @return A string representing a name to use for this docking parent.
 */
const TCHAR* WxPhAT::GetDockingParentName() const
{
	return TEXT("PhAT");
}

/**
 * @return The current version of the docking parent, this value needs to be increased every time new docking windows are added or removed.
 */
const INT WxPhAT::GetDockingParentVersion() const
{
	return 0;
}

/*-----------------------------------------------------------------------------
	WxPhATMenuBar
-----------------------------------------------------------------------------*/

WxPhATMenuBar::WxPhATMenuBar(WxPhAT* InPhAT)
{
	EditMenu = new wxMenu();
	Append( EditMenu, *LocalizeUnrealEd("Edit") );

	EditMenu->Append(ID_PHAT_UNDO, *LocalizeUnrealEd("Undo"), TEXT("") );
	EditMenu->Append(ID_PHAT_REDO, *LocalizeUnrealEd("Redo"), TEXT("") );
	EditMenu->AppendSeparator();
	EditMenu->Append(ID_PHAT_CHANGESKELMESH, *LocalizeUnrealEd("ChangeSkelMesh"), TEXT("") );

	ToolsMenu = new wxMenu();
	Append( ToolsMenu, *LocalizeUnrealEd("Tools") );

	ToolsMenu->Append(ID_PHAT_RESETASSET, *LocalizeUnrealEd("ResetEntireAsset"), TEXT("") );
	ToolsMenu->Append(ID_PHAT_RESETBONE, *LocalizeUnrealEd("ResetSelectedBoneCollision"), TEXT(""));
	ToolsMenu->Append(ID_PHAT_SETMATERIAL, *LocalizeUnrealEd("ApplyPhysicalAssetMaterial"), *LocalizeUnrealEd("ApplyPhysicalAssetMaterial_HelpText"));
	ToolsMenu->Append(ID_PHAT_COPYTOALLJOINTS, *LocalizeUnrealEd("CopyToAllJoints"), TEXT(""));
}

WxPhATMenuBar::~WxPhATMenuBar()
{

}

/*-----------------------------------------------------------------------------
	WxPhATToolBar.
-----------------------------------------------------------------------------*/

BEGIN_EVENT_TABLE( WxPhATToolBar, WxToolBar )
END_EVENT_TABLE()

#define		BUTTON_RECT(a)	(wxRect(wxPoint(a*16, 0), wxSize(16, 16)))

WxPhATToolBar::WxPhATToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{

	BodyModeB.Load( TEXT("PhAT_BodyMode") );
	ConstraintModeB.Load( TEXT("PhAT_ConstraintMode") );
	HighlightVertB.Load( TEXT("PhAT_HighlightVert") );
	StartSimB.Load( TEXT("PhAT_StartSim") );
	StopSimB.Load( TEXT("PhAT_StopSim") );
	RotModeB.Load( TEXT("PhAT_RotMode") );
	TransModeB.Load( TEXT("PhAT_TransMode") );
	ScaleModeB.Load( TEXT("PhAT_ScaleMode") );
	WorldSpaceB.Load( TEXT("PhAT_WorldSpace") );
	LocalSpaceB.Load( TEXT("PhAT_LocalSpace") );
	AddSphereB.Load( TEXT("PhAT_AddSphere") );
	AddSphylB.Load( TEXT("PhAT_AddSphyl") );
	AddBoxB.Load( TEXT("PhAT_AddBox") );
	DupPrimB.Load( TEXT("PhAT_DupPrim") );
	DelPrimB.Load( TEXT("PhAT_DelPrim") );
	ShowSkelB.Load( TEXT("PhAT_ShowSkel") );
	ShowContactsB.Load( TEXT("PhAT_ShowContacts") );
	ShowCOMB.Load( TEXT("PhAT_ShowCOM") );
	HideCollB.Load( TEXT("PhAT_HideColl") );
	WireCollB.Load( TEXT("PhAT_WireColl") );
	ShowCollB.Load( TEXT("PhAT_ShowColl") );
	HideMeshB.Load( TEXT("PhAT_HideMesh") );
	WireMeshB.Load( TEXT("PhAT_WireMesh") );
	ShowMeshB.Load( TEXT("PhAT_ShowMesh") );
	ConFrameB.Load( TEXT("PhAT_ConFrame") );
	ConSnapToBoneB.Load( TEXT("PhAT_ConSnapToBone") );
	ConSnapAllToBoneB.Load( TEXT("PhAT_ConSnapAllToBone") );
	SnapB.Load( TEXT("PhAT_Snap") );
	ConHideB.Load( TEXT("PhAT_ConHide") );
	ConPosB.Load( TEXT("PhAT_ConPos") );
	ConLimitB.Load( TEXT("PhAT_ConLimit") );
	BSJointB.Load( TEXT("PhAT_BSJoint") );
	HingeB.Load( TEXT("PhAT_Hinge") );
	PrismaticB.Load( TEXT("PhAT_Prismatic") );
	SkelB.Load( TEXT("PhAT_Skel") );
	DelJointB.Load( TEXT("PhAT_DelJoint") );
	DisablePairB.Load( TEXT("PhAT_DisablePair") );
	EnablePairB.Load( TEXT("PhAT_EnablePair") );
	CopyPropsB.Load( TEXT("PhAT_CopyProps") );
	InstancePropsB.Load( TEXT("PhAT_InstanceProps") );
	WeldB.Load( TEXT("PhAT_Weld") );
	AddBoneB.Load( TEXT("PhAT_AddBone") );
	ShowFloorB.Load( TEXT("PhAT_ShowFloor") );
	LockB.Load( TEXT("Lock") );
	ShowAnimSkelB.Load( TEXT("PhAT_ShowAnimSkel") );

	SetToolBitmapSize( wxSize(16, 16) );

	// Create bitmap buttons. These are the button we change the picture on.
	ModeButton = new wxBitmapButton(this, IDMN_PHAT_EDITMODE, BodyModeB);
	ModeButton->SetToolTip( *LocalizeUnrealEd("ToolTip_10") );

	SpaceButton = new wxBitmapButton(this, IDMN_PHAT_MOVESPACE, WorldSpaceB);
	SpaceButton->SetToolTip( *LocalizeUnrealEd("ToolTip_11") );

	SimButton = new wxBitmapButton(this, IDMN_PHAT_RUNSIM, StartSimB);
	SimButton->SetToolTip( *LocalizeUnrealEd("ToolTip_12") );

	MeshViewButton = new wxBitmapButton(this, IDMN_PHAT_MESHVIEWMODE, ShowMeshB);
	MeshViewButton->SetToolTip( *LocalizeUnrealEd("ToolTip_13") );

	CollisionViewButton = new wxBitmapButton(this, IDMN_PHAT_COLLISIONVIEWMODE, WireCollB);
	CollisionViewButton->SetToolTip( *LocalizeUnrealEd("ToolTip_14") );

	ConstraintViewButton = new wxBitmapButton(this, IDMN_PHAT_CONSTRAINTVIEWMODE, ConPosB);
	ConstraintViewButton->SetToolTip( *LocalizeUnrealEd("ToolTip_15") );

	AnimPlayButton = new wxBitmapButton(this, IDMN_PHAT_TOGGLEPLAYANIM, StartSimB);
	AnimPlayButton->SetToolTip( *LocalizeUnrealEd("PhATPlayAnim") );

	AddSeparator();
	AddControl(ModeButton);
	AddControl(SpaceButton);
	AddSeparator();
	AddCheckTool(IDMN_PHAT_TRANSLATE, *LocalizeUnrealEd("TranslationMode"), TransModeB, wxNullBitmap, *LocalizeUnrealEd("TranslationMode") );
	AddCheckTool(IDMN_PHAT_ROTATE, *LocalizeUnrealEd("RotationMode"), RotModeB, wxNullBitmap, *LocalizeUnrealEd("RotationMode") );
	AddCheckTool(IDMN_PHAT_SCALE, *LocalizeUnrealEd("ScalingMode"), ScaleModeB, wxNullBitmap, *LocalizeUnrealEd("ScalingMode") );
	AddSeparator();
	AddCheckTool(IDMN_PHAT_SNAP, *LocalizeUnrealEd("Snap"), SnapB, wxNullBitmap, *LocalizeUnrealEd("Snap") );
	AddTool(IDMN_PHAT_COPYPROPERTIES, CopyPropsB, *LocalizeUnrealEd("CopyPropertiesTo"));
	AddCheckTool(IDMN_PHAT_INSTANCEPROPS, *LocalizeUnrealEd("InstanceProps"), InstancePropsB, wxNullBitmap, *LocalizeUnrealEd("InstanceProps") );
	AddSeparator();
	AddControl(SimButton);
	AddSeparator();
	AddControl(MeshViewButton);
	AddControl(CollisionViewButton);
	AddControl(ConstraintViewButton);
	AddCheckTool(IDMN_PHAT_SHOWFIXED, *LocalizeUnrealEd("ShowFixed"), LockB, wxNullBitmap, *LocalizeUnrealEd("ShowFixed") );
	AddCheckTool(IDMN_PHAT_DRAWGROUND, *LocalizeUnrealEd("DrawGroundBox"), ShowFloorB, wxNullBitmap, *LocalizeUnrealEd("DrawGroundBox") );
	AddSeparator();
	AddCheckTool(IDMN_PHAT_SHOWHIERARCHY, *LocalizeUnrealEd("ToggleGraphicsHierarchy"), ShowSkelB, wxNullBitmap, *LocalizeUnrealEd("ToggleGraphicsHierarchy") );
	AddCheckTool(IDMN_PHAT_SHOWCONTACTS, *LocalizeUnrealEd("ToggleViewContacts"), ShowContactsB, wxNullBitmap, *LocalizeUnrealEd("ToggleViewContacts") );
	AddCheckTool(IDMN_PHAT_SHOWINFLUENCES, *LocalizeUnrealEd("ShowSelectedBoneInfluences"), HighlightVertB, wxNullBitmap, *LocalizeUnrealEd("ShowSelectedBoneInfluences") );
	AddCheckTool(IDMN_PHAT_SHOWCOM, *LocalizeUnrealEd("ToggleMassProperties"), ShowCOMB, wxNullBitmap, *LocalizeUnrealEd("ToggleMassProperties") );
	AddSeparator();
	AddTool(IDMN_PHAT_DISABLECOLLISION, DisablePairB, *LocalizeUnrealEd("DisableCollisionWith"));
	AddTool(IDMN_PHAT_ENABLECOLLISION, EnablePairB, *LocalizeUnrealEd("EnableCollisionWith"));
	AddTool(IDMN_PHAT_WELDBODIES, WeldB, *LocalizeUnrealEd("WeldToThisBody"));
	AddTool(IDMN_PHAT_ADDNEWBODY, AddBoneB, *LocalizeUnrealEd("AddNewBody"));
	AddSeparator();
	AddTool(IDMN_PHAT_ADDSPHERE, AddSphereB, *LocalizeUnrealEd("AddSphere"));
	AddTool(IDMN_PHAT_ADDSPHYL, AddSphylB, *LocalizeUnrealEd("AddSphyl"));
	AddTool(IDMN_PHAT_ADDBOX, AddBoxB, *LocalizeUnrealEd("AddBox"));
	AddTool(IDMN_PHAT_DELETEPRIM, DelPrimB, *LocalizeUnrealEd("DeleteCurrentPrimitive"));
	AddTool(IDMN_PHAT_DUPLICATEPRIM, DupPrimB, *LocalizeUnrealEd("DuplicateCurrentPrimitive"));
	AddSeparator();
	AddTool(IDMN_PHAT_RESETCONFRAME, ConFrameB, *LocalizeUnrealEd("ResetConstraintReference"));
	AddTool(IDMN_PHAT_SNAPCONTOBONE, ConSnapToBoneB, *LocalizeUnrealEd("SnapConstraintToBone"));
	AddTool(IDMN_PHAT_SNAPALLCONTOBONE, ConSnapAllToBoneB, *LocalizeUnrealEd("SnapAllConstraintsToBone"));
	AddSeparator();
	AddTool(IDMN_PHAT_ADDBS, BSJointB, *LocalizeUnrealEd("ConvertToBallAndSocket"));
	AddTool(IDMN_PHAT_ADDHINGE, HingeB, *LocalizeUnrealEd("ConvertToHinge"));
	AddTool(IDMN_PHAT_ADDPRISMATIC, PrismaticB, *LocalizeUnrealEd("ConvertToPrismatic"));
	AddTool(IDMN_PHAT_ADDSKELETAL, SkelB, *LocalizeUnrealEd("ConvertToSkeletal"));
	AddTool(IDMN_PHAT_DELETECONSTRAINT, DelJointB, *LocalizeUnrealEd("DeleteCurrentConstraint"));

	// Animation stuff

	AddSeparator();
	AddControl(AnimPlayButton);
	AnimPlayButton->Enable(FALSE);

	AnimCombo = new WxComboBox(this, IDMN_PHAT_ANIMCOMBO, TEXT(""), wxDefaultPosition, wxSize(150, -1), 0, NULL, wxCB_READONLY);
	AddControl(AnimCombo);
	AnimCombo->Enable(FALSE);

	AddCheckTool(IDMN_PHAT_SHOWANIMSKEL, *LocalizeUnrealEd("ShowAnimSkel"), ShowAnimSkelB, wxNullBitmap, *LocalizeUnrealEd("ShowAnimSkel") );

	Realize();
}

WxPhATToolBar::~WxPhATToolBar()
{
}
