/*=============================================================================
	PhATMain.cpp: Physics Asset Tool main windows code
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"
#include "..\..\Launch\Resources\resource.h"

IMPLEMENT_CLASS(APhATActor);
IMPLEMENT_CLASS(APhATFloor);
IMPLEMENT_CLASS(UPhATSkeletalMeshComponent);
IMPLEMENT_CLASS(UPhATSimOptions);

static const FLOAT DefaultFloorGap = 25.0f; // Default distance from bottom of asset to floor. Unreal units.

WPhAT::WPhAT(UPhysicsAsset* InAsset)
{
	PhysicsAsset = InAsset;

	Container = NULL;

	WindowLabel = NULL;
	PropertyWindow = NULL;

	PhATViewportClient = NULL;

	EditorLevel = NULL;

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

	NextSelectEvent = PNS_Normal;

	bShowCOM = 0;
	bShowHierarchy = 0;
	bShowInfluences = 0;
	bDrawGround = 0;
	
	bSelectionLock = 0;
	bSnap = 0;

	bRunningSimulation = 0;
	bManipulating = false;
	bNoMoveCamera = 0;

}

void WPhAT::Serialize(FArchive& Ar)
{
	WWindow::Serialize(Ar);

	Ar<<PhysicsAsset;
	Ar<<EditorLevel;
	Ar<<EditorSimOptions;

	if(PhATViewportClient)
	{
		PhATViewportClient->Serialize(Ar);
	}
}

void WPhAT::OpenWindow()
{
	PerformCreateWindowEx(
		0,
		*FString::Printf( TEXT("PhAT  ( %s )"), PhysicsAsset->GetName() ),
		WS_VISIBLE | WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_THICKFRAME,
		100,
		100,
		850,
		550,
		NULL,
		NULL,
		hInstance
		);
}

// Dear Warren,
// One day you will probably port this to wxWindows. I hope its not too painful. Any chance you could make the Body Editing
// and Constraint Editing sections a modal toolbar instead of all this yucky disabling/enabling?
// Thankyou!
// James

TBBUTTON tbPHATButtons[] = 
{
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // --- TOOLS
	{ 0,	IDMN_PHAT_EDITMODE,				TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Editing mode (body/constraint toggle)
	{ 8,	IDMN_PHAT_MOVESPACE,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Movement space (world/local toggle)
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // ---
	{ 6,	IDMN_PHAT_TRANSLATE,			TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Translate
	{ 5,	IDMN_PHAT_ROTATE,				TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Rotate
	{ 12,	IDMN_PHAT_SCALE,				TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Scale
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // ---
	{ 27,	IDMN_PHAT_SNAP,					TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Snap toggle
	{ 38,	IDMN_PHAT_COPYPROPERTIES,		TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Copy properties
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // ---
	{ 3,	IDMN_PHAT_RUNSIM,				TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Run simulation
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // --- RENDERING
	{ 21,	IDMN_PHAT_MESHVIEWMODE,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Mesh view mode
	{ 17,	IDMN_PHAT_COLLISIONVIEWMODE,	TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Collision view mode
	{ 29,	IDMN_PHAT_CONSTRAINTVIEWMODE,	TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Constraint view mode
	{ 41,	IDMN_PHAT_DRAWGROUND,			TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Draw the ground box
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // ---
	{ 13,	IDMN_PHAT_SHOWHIERARCHY,		TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Toggle hierarchy
	{ 2,	IDMN_PHAT_SHOWINFLUENCES,		TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Toggle influences
	{ 15,	IDMN_PHAT_SHOWCOM,				TBSTATE_ENABLED,	TBSTYLE_CHECK,		0L, 0}, // Toggle COM view
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // --- BODY EDITING
	{ 36,	IDMN_PHAT_DISABLECOLLISION,		TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Disable collision
	{ 37,	IDMN_PHAT_ENABLECOLLISION,		TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Enable collision
	{ 39,	IDMN_PHAT_WELDBODIES,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Weld Bodies
	{ 40,	IDMN_PHAT_ADDNEWBODY,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // New Body
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // ---
	{ 9,	IDMN_PHAT_ADDSPHERE,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add Sphere
	{ 10,	IDMN_PHAT_ADDSPHYL,				TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add Sphyl
	{ 11,	IDMN_PHAT_ADDBOX,				TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add Box
	{ 14,	IDMN_PHAT_DELETEPRIM,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Delete Prim
	{ 42,	IDMN_PHAT_DUPLICATEPRIM,		TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Duplicate Selected Prim
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // --- CONSTRAINT EDITING
	{ 24,	IDMN_PHAT_RESETCONFRAME,		TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Duplicate Selected Prim
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}, // --- CONSTRAINT EDITING
	{ 31,	IDMN_PHAT_ADDBS,				TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add/convert to Ball-and-Socket
	{ 32,	IDMN_PHAT_ADDHINGE,				TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add/convert to Hinge
	{ 33,	IDMN_PHAT_ADDPRISMATIC,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add/convert to Prismatic
	{ 35,	IDMN_PHAT_ADDSKELETAL,			TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Add/convert to Skeletal
	{ 34,	IDMN_PHAT_DELETECONSTRAINT,		TBSTATE_ENABLED,	TBSTYLE_BUTTON,		0L, 0}, // Delete current constraint
	{ 0,	0,								TBSTATE_ENABLED,	TBSTYLE_SEP,		0L, 0}  // ---
};

struct ToolTipStrings
{
	TCHAR ToolTip[64];
	INT ID;
};

ToolTipStrings ToolTips_PHAT[] = 
{
	TEXT("Editing Mode [B]"),						IDMN_PHAT_EDITMODE,
	TEXT("Movement Space"),							IDMN_PHAT_MOVESPACE,
	TEXT("Rotation Mode [E]"),						IDMN_PHAT_ROTATE,
	TEXT("Translation Mode [W]"),					IDMN_PHAT_TRANSLATE,
	TEXT("Scaling Mode [R]"),						IDMN_PHAT_SCALE,
	TEXT("Snap [A]"),								IDMN_PHAT_SNAP,
	TEXT("Copy Properties To.. [C]"),				IDMN_PHAT_COPYPROPERTIES,
	TEXT("Toggle Simulation [S]"),					IDMN_PHAT_RUNSIM,
	TEXT("Cycle Mesh Rendering Mode [H]"),			IDMN_PHAT_MESHVIEWMODE,
	TEXT("Cycle Collision Rendering Mode [J]"),		IDMN_PHAT_COLLISIONVIEWMODE,
	TEXT("Cycle Constraint Rendering Mode [K]"),	IDMN_PHAT_CONSTRAINTVIEWMODE,
	TEXT("Draw Ground Box"),						IDMN_PHAT_DRAWGROUND,
	TEXT("Toggle Graphics Hierarchy"),				IDMN_PHAT_SHOWHIERARCHY,
	TEXT("Show Selected Bone Influences"),			IDMN_PHAT_SHOWINFLUENCES,
	TEXT("Toggle Mass Properties"),					IDMN_PHAT_SHOWCOM,
	TEXT("Disable Collision With.."),				IDMN_PHAT_DISABLECOLLISION,
	TEXT("Enable Collision With.."),				IDMN_PHAT_ENABLECOLLISION,
	TEXT("Weld To This Body.. [D]"),				IDMN_PHAT_WELDBODIES,
	TEXT("Add New Body"),							IDMN_PHAT_ADDNEWBODY,
	TEXT("Add Sphere"),								IDMN_PHAT_ADDSPHERE,
	TEXT("Add Sphyl"),								IDMN_PHAT_ADDSPHYL,
	TEXT("Add Box"),								IDMN_PHAT_ADDBOX,
	TEXT("Delete Current Primitive [DEL]"),			IDMN_PHAT_DELETEPRIM,
	TEXT("Duplicate Current Primitive"),			IDMN_PHAT_DUPLICATEPRIM,
	TEXT("Reset Constraint Reference"),				IDMN_PHAT_RESETCONFRAME,
	TEXT("Convert To Ball-And-Socket"),				IDMN_PHAT_ADDBS,
	TEXT("Convert To Hinge"),						IDMN_PHAT_ADDHINGE,
	TEXT("Convert To Prismatic"),					IDMN_PHAT_ADDPRISMATIC,
	TEXT("Convert To Skeletal"),					IDMN_PHAT_ADDSKELETAL,
	TEXT("Delete Current Constraint"),				IDMN_PHAT_DELETECONSTRAINT,
	NULL, 0
};

static INT PhATLevelCount = 0;

void WPhAT::OnCreate()
{
	SetMenu( hWnd, LoadMenuIdX(hInstance, IDMENU_PhAT) );

	Container = new FContainer();

	// Create the toolbar.
	hWndToolBar = CreateToolbarEx( 
		hWnd, 
		WS_CHILD | WS_BORDER | WS_VISIBLE | CCS_ADJUSTABLE,
		IDB_PhAT_TOOLBAR,
		15,
		hInstance,
		IDB_PhAT_TOOLBAR,
		(LPCTBBUTTON)&tbPHATButtons,
		ARRAY_COUNT(tbPHATButtons), // Total number of buttons and dividers in toolbar. 
		16,16,
		16,16,
		sizeof(TBBUTTON) );
	check(hWndToolBar);

	// Create tool tips.
	ToolTipCtrl = new WToolTip(this);
	ToolTipCtrl->OpenWindow();

	for(INT tooltip = 0; ToolTips_PHAT[tooltip].ID > 0; tooltip++)
	{
		// Figure out the rectangle for the toolbar button.
		INT index = SendMessageX( hWndToolBar, TB_COMMANDTOINDEX, ToolTips_PHAT[tooltip].ID, 0 );
		RECT rect;
		SendMessageX( hWndToolBar, TB_GETITEMRECT, index, (LPARAM)&rect);
		if( rect.left != rect.right ) // any real area?
			ToolTipCtrl->AddTool( hWndToolBar, ToolTips_PHAT[tooltip].ToolTip, tooltip, &rect );
		else
			appMsgf(0,TEXT("Invalid tooltip area or ID"));
	}

	// Create main window label
	WindowLabel = new WLabel(this, IDSC_VIEWPORT);
	WindowLabel->OpenWindow(1,0);

	// Create property window
	PropertyWindow = new WObjectProperties(TEXT("PhysAssetProperties"), CPF_Edit,TEXT("Properties"), this, 1);
	PropertyWindow->ShowTreeLines = 0;
	PropertyWindow->OpenChildWindow(0);
	PropertyWindow->Root.Sorted = 1;
	PropertyWindow->SetNotifyHook(this);

	// Create level we are going to view into
	FString NewLevelName = FString::Printf( TEXT("PhATLevel%d"), PhATLevelCount );
	EditorLevel = new( UObject::GetTransientPackage(), *NewLevelName )ULevel( GUnrealEd, 1 );
	PhATLevelCount++;

	check(EditorLevel);

	EditorLevel->InitLevelRBPhys(); // Allow physics to run in this level.

	// Spawn ground box static-mesh actor.
	EditorFloor = (APhATFloor*)EditorLevel->SpawnActor( APhATFloor::StaticClass() );

	check(PhysicsAsset->DefaultSkelMesh);
	EditorSkelMesh = PhysicsAsset->DefaultSkelMesh;

	// Used for viewing bone influences, resetting bone geometry etc.
	EditorSkelMesh->CalcBoneVertInfos(DominantWeightBoneInfos, true);
	EditorSkelMesh->CalcBoneVertInfos(AnyWeightBoneInfos, false);

	// Spawn actor into the level, set mesh, and set pointer back to the editor.
	EditorActor = (APhATActor*)EditorLevel->SpawnActor( APhATActor::StaticClass() );
	check(EditorActor);

	EditorActor->PhysicsAssetEditor = (INT)this;

	EditorSkelComp = EditorActor->SkelComponent;
	check(EditorSkelComp);

	EditorSkelComp->SetSkeletalMesh( EditorSkelMesh );
	EditorSkelComp->PhysicsAsset = PhysicsAsset;

	// Ensure PhysicsAsset mass properties are up to date.
	PhysicsAsset->UpdateBodyIndices();
	PhysicsAsset->FixOuters();

	// Create 3D viewport into Level
	PhATViewportClient = new FPhATViewportClient(this);
	PhATViewportClient->Viewport = GEngine->Client->CreateWindowChildViewport(PhATViewportClient,WindowLabel->hWnd);
	PhATViewportClient->Viewport->CaptureJoystickInput(false);

	// Spawn sim options.
	EditorSimOptions = ConstructObject<UPhATSimOptions>( UPhATSimOptions::StaticClass() );
	check(EditorSimOptions);

	EditorSimOptions->MouseLinearDamping = EditorActor->MouseHandle->LinearDamping;
	EditorSimOptions->MouseLinearStiffness = EditorActor->MouseHandle->LinearStiffness;
	EditorSimOptions->MouseLinearMaxDistance = EditorActor->MouseHandle->LinearMaxDistance;
	EditorSimOptions->MouseAngularDamping = EditorActor->MouseHandle->AngularDamping;
	EditorSimOptions->MouseAngularStiffness = EditorActor->MouseHandle->AngularStiffness;

	// Get actors asset collision bounding box, and move actor so its not intersection the floor plane at Z = 0.
	FBox CollBox = PhysicsAsset->CalcAABB( EditorSkelComp );	
	EditorActorStartLocation = FVector(0, 0, EditorActor->Location.Z-CollBox.Min.Z + EditorSimOptions->FloorGap);

	FCheckResult Hit(0);
	EditorLevel->FarMoveActor(EditorActor, EditorActorStartLocation, 0, 0, 0);
	EditorLevel->MoveActor(EditorActor, FVector(0.f), FRotator(0,0,0), Hit);


	// Get new bounding box and set view based on that.
	CollBox = PhysicsAsset->CalcAABB( EditorSkelComp );	
	FVector CollBoxExtent = CollBox.GetExtent();

	// Take into account internal mesh translation/rotation/scaling etc.
	FMatrix LocalToWorld = EditorSkelComp->LocalToWorld;
	FSphere WorldSphere = EditorSkelMesh->Bounds.GetSphere().TransformBy(LocalToWorld);

	CollBoxExtent = CollBox.GetExtent();
	if(CollBoxExtent.X > CollBoxExtent.Y)
	{
		PhATViewportClient->ViewLocation = FVector(WorldSphere.X, WorldSphere.Y - 1.5*WorldSphere.W, WorldSphere.Z);
		PhATViewportClient->ViewRotation = FRotator(0,16384,0);	
	}
	else
	{
		PhATViewportClient->ViewLocation = FVector(WorldSphere.X - 1.5*WorldSphere.W, WorldSphere.Y, WorldSphere.Z);
		PhATViewportClient->ViewRotation = FRotator(0,0,0);	
	}

	PhATViewportClient->UpdateLighting();

	// Set up window positions
	INT Top = 30;
	INT PropWidth = 350;
	HWND PreviewWindow = (HWND)PhATViewportClient->Viewport->GetWindow();

	Anchors.Set((DWORD)WindowLabel->hWnd,		FWindowAnchor(hWnd, WindowLabel->hWnd,		ANCHOR_TL,0,Top,						ANCHOR_BR,-PropWidth,0					) );
	Anchors.Set((DWORD)PreviewWindow,			FWindowAnchor(hWnd, PreviewWindow,			ANCHOR_TL,0,0,							ANCHOR_BR,-PropWidth,-Top				) );
	Anchors.Set((DWORD)PropertyWindow->hWnd,	FWindowAnchor(hWnd, PropertyWindow->hWnd,	ANCHOR_RIGHT|ANCHOR_TOP,-PropWidth,Top,	ANCHOR_WIDTH|ANCHOR_BOTTOM,PropWidth,0	) );

	Container->SetAnchors( &Anchors );

	PositionChildControls();

	// Initially nothing selected.
	SetSelectedBody( INDEX_NONE, KPT_Unknown, INDEX_NONE );
	SetSelectedConstraint( INDEX_NONE );

	SetMovementMode( PMM_Translate );

	ToggleDrawGround();

	UpdateToolBarStatus();

}

void WPhAT::OnDestroy()
{
	WWindow::OnDestroy();

	// Ensure simulation is not running.
	if(bRunningSimulation)
		ToggleSimulation();

	delete Container;

	::DestroyWindow( hWndToolBar );
	delete ToolTipCtrl;

	delete WindowLabel;

	PropertyWindow->Root.SetObjects( NULL, 0 );
	delete PropertyWindow;

	GEngine->Client->CloseViewport(PhATViewportClient->Viewport);
	PhATViewportClient->Viewport = NULL;

	delete PhATViewportClient;
	PhATViewportClient = NULL;

	EditorActor = NULL;
	EditorFloor = NULL;

	if(EditorLevel)
	{
		EditorLevel->CleanupLevel();
		EditorLevel = NULL;
	}
}

void WPhAT::OnCommand( INT Command )
{
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

	case IDMN_PHAT_SHOWCOM:
		ToggleViewCOM();
		break;

	case IDMN_PHAT_SHOWHIERARCHY:
		ToggleViewHierarchy();
		break;

	case IDMN_PHAT_SHOWINFLUENCES:
		ToggleViewInfluences();
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

	case ID_PHAT_RESETASSET:
		RecalcEntireAsset();
		break;

	case ID_PHAT_RESETBONE:
		ResetBoneCollision(SelectedBodyIndex);
		break;

	case ID_PHAT_SETMATERIAL:
		SetAssetPhysicalMaterial();
		break;

	case WM_POSITIONCHILDCONTROLS:
		PositionChildControls();
		break;

	default:
		WWindow::OnCommand(Command);
		break;
	}

}

void WPhAT::OnSize( DWORD Flags, INT NewX, INT NewY )
{
	PositionChildControls();

}

void WPhAT::OnPaint()
{
	PhATViewportClient->Viewport->Invalidate();

}

void WPhAT::PositionChildControls()
{
	if( Container )
		Container->RefreshControls();

	InvalidateRect( hWnd, NULL, 1 );

}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// FPhATViewportClient ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


void FPhATViewportClient::InputKey(FChildViewport* Viewport, FName Key, EInputEvent Event,FLOAT)
{
	// Capture the mouse input if on of the mouse buttons are held down.
	Viewport->CaptureMouseInput( Viewport->KeyState(KEY_LeftMouseButton) || Viewport->KeyState(KEY_MiddleMouseButton) || Viewport->KeyState(KEY_RightMouseButton) );

	INT HitX = Viewport->GetMouseX();
	INT HitY = Viewport->GetMouseY();
	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	if(AssetEditor->bRunningSimulation)
	{
		if(Key == KEY_RightMouseButton)
		{
			if(Event == IE_Pressed)
			{
				if(bShiftDown)
				{
					AssetEditor->SimMousePress(Viewport, false);
				}
				else if(bCtrlDown)
				{
					AssetEditor->SimMousePress(Viewport, true);
				}
			}
			else if(Event == IE_Released)
			{
				AssetEditor->SimMouseRelease();
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
			if( Event == IE_Pressed && (bCtrlDown || bShiftDown) )
			{
				DoHitTest(Viewport, Key, Event);

				AssetEditor->bNoMoveCamera = true;
			}
			else if(Event == IE_Released)
			{
				if( AssetEditor->bManipulating )
					AssetEditor->EndManipulating();

				AssetEditor->bNoMoveCamera = false;
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
		else if(Key == KEY_SpaceBar)
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
		else if(Key == KEY_A)
			AssetEditor->ToggleSnap();
		else if(Key == KEY_D)
			AssetEditor->WeldBodyToNextSelect();
		else if(Key == KEY_Q)
			AssetEditor->CycleSelectedConstraintOrientation();
		else if(Key == KEY_C)
			AssetEditor->CopyPropertiesToNextSelect();

		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton )
		{
			MouseDeltaTracker.StartTracking( this, HitX, HitY );
		}
	}
	else if( Event == IE_Released )
	{
		if( Key == KEY_LeftMouseButton || Key == KEY_RightMouseButton )
		{
			MouseDeltaTracker.EndTracking( this );
		}
	}

}

void FPhATViewportClient::InputAxis(FChildViewport* Viewport, FName Key, FLOAT Delta, FLOAT DeltaTime)
{
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
	else if(!AssetEditor->bNoMoveCamera)
	{
		MouseDeltaTracker.AddDelta( this, Key, Delta, 0 );
		FVector DragDelta = MouseDeltaTracker.GetDelta();

		GEditor->MouseMovement += DragDelta;

		if( !DragDelta.IsZero() )
		{
			// Convert the movement delta into drag/rotation deltas

			FVector Drag;
			FRotator Rot;
			FVector Scale;
			MouseDeltaTracker.ConvertMovementDeltaToDragRot( this, DragDelta, Drag, Rot, Scale );

			MoveViewportCamera( Drag, Rot );

			MouseDeltaTracker.ReduceBy( DragDelta );
		}
	}

	Viewport->Invalidate();
}

void FPhATViewportClient::Tick(FLOAT DeltaSeconds)
{
	FEditorLevelViewportClient::Tick(DeltaSeconds);

	if(AssetEditor->bRunningSimulation)
		AssetEditor->TickSimulation(DeltaSeconds);

}


// Properties window NotifyHook stuff

void WPhAT::NotifyDestroy( void* Src )
{
	if(Src == PropertyWindow)
		PropertyWindow = NULL;

}

void WPhAT::NotifyPreChange( void* Src, UProperty* PropertyAboutToChange )
{
	if(!bRunningSimulation)
	{
		if(EditingMode == PEM_BodyEdit)
			GEditor->Trans->Begin( TEXT("Edit Body Properties") );
		else
			GEditor->Trans->Begin( TEXT("Edit Constraint Properties") );
	}

}

void WPhAT::NotifyPostChange( void* Src, UProperty* PropertyThatChanged )
{
	if(!bRunningSimulation)
		GEditor->Trans->End();

	EditorSimOptions->SaveConfig();

	UpdateNoCollisionBodies();
	PhATViewportClient->UpdateLighting();
	PhATViewportClient->Viewport->Invalidate();

	if (!bRunningSimulation)
	{
		// Update floor gap
		EditorSkelComp = EditorActor->SkelComponent;
		check(EditorSkelComp);

		EditorSkelComp->SetSkeletalMesh( EditorSkelMesh );
		EditorSkelComp->PhysicsAsset = PhysicsAsset;

		// Get actors asset collision bounding box, and move actor so its not intersection the floor plane at Z = 0.
		FBox CollBox = PhysicsAsset->CalcAABB( EditorSkelComp );	
		EditorActorStartLocation = FVector(0, 0, EditorActor->Location.Z-CollBox.Min.Z + EditorSimOptions->FloorGap);

		FCheckResult Hit(0);
		EditorLevel->FarMoveActor(EditorActor, EditorActorStartLocation, 0, 0, 0);
		EditorLevel->MoveActor(EditorActor, FVector(0.f), FRotator(0,0,0), Hit);
	}

}

void WPhAT::NotifyExec( void* Src, const TCHAR* Cmd )
{
}
