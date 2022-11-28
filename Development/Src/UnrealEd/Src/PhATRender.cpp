/*=============================================================================
	PhATRender.cpp: Physics Asset Tool rendering support
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by James Golding
=============================================================================*/

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"
#include "..\..\Launch\Resources\resource.h"

static const FColor BoneUnselectedColor(170,155,225);
static const FColor BoneSelectedColor(185,70,0);
static const FColor ElemSelectedColor(255,166,0);
static const FColor NoCollisionColor(200, 200, 200);
static const FColor	ConstraintBone1Color(255,0,0);
static const FColor ConstraintBone2Color(0,0,255);

static const FColor HeirarchyDrawColor(220, 255, 220);

static const FLOAT	WidgetSize(0.15f); // Proportion of the viewport the widget should fill

static const FLOAT	COMRenderSize(5.0f);
static const FColor	COMRenderColor(255,255,100);
static const FColor InertiaTensorRenderColor(255,255,100);

static const FLOAT  InfluenceLineLength(2.0f);
static const FColor	InfluenceLineColor(0,255,0);

static const INT	SphereRenderSides(16);
static const INT	SphereRenderRings(8);

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// FPhATViewportClient ////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////


FPhATViewportClient::FPhATViewportClient(WPhAT* InAssetEditor):
AssetEditor(InAssetEditor)
{
	Level = AssetEditor->EditorLevel;

	SkyLightComponent = ConstructObject<USkyLightComponent>(USkyLightComponent::StaticClass());
	SkyLightComponent->Scene = Level;
	SkyLightComponent->Created();

	DirectionalLightComponent = ConstructObject<UDirectionalLightComponent>(UDirectionalLightComponent::StaticClass());
	DirectionalLightComponent->Scene = Level;
	DirectionalLightComponent->Created();

	ShowFlags = SHOW_DefaultEditor; 

	PhATFont = GEngine->SmallFont;
	check(PhATFont);

	// Body materials
	ElemSelectedMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_ElemSelectedMaterial"), NULL, LOAD_NoFail, NULL);
	check(ElemSelectedMaterial);

	BoneSelectedMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_BoneSelectedMaterial"), NULL, LOAD_NoFail, NULL);
	check(BoneSelectedMaterial);

	BoneUnselectedMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_UnselectedMaterial"), NULL, LOAD_NoFail, NULL);
	check(BoneUnselectedMaterial);

	BoneNoCollisionMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_NoCollisionMaterial"), NULL, LOAD_NoFail, NULL);
	check(BoneNoCollisionMaterial);

	JointLimitMaterial = LoadObject<UMaterialInstance>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_NoFail, NULL);
	check(JointLimitMaterial);
}

FPhATViewportClient::~FPhATViewportClient()
{
	if(SkyLightComponent->Initialized)
		SkyLightComponent->Destroyed();

	delete SkyLightComponent;
	SkyLightComponent = NULL;

	if(DirectionalLightComponent->Initialized)
		DirectionalLightComponent->Destroyed();

	delete DirectionalLightComponent;
	DirectionalLightComponent = NULL;
}

void FPhATViewportClient::Serialize(FArchive& Ar)
{ 
	Ar << DirectionalLightComponent << SkyLightComponent << Input; 
}


FColor FPhATViewportClient::GetBackgroundColor()
{
	return FColor(64,64,64);
}

void FPhATViewportClient::CalcSceneView(FSceneView* View)
{
	FEditorLevelViewportClient::CalcSceneView(View);
}

void FPhATViewportClient::DoHitTest(FChildViewport* Viewport, FName Key, EInputEvent Event)
{
	INT			HitX = Viewport->GetMouseX(),
				HitY = Viewport->GetMouseY();
	HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);
	FSceneView	View;

	UBOOL bCtrlDown = Viewport->KeyState(KEY_LeftControl) || Viewport->KeyState(KEY_RightControl);
	UBOOL bShiftDown = Viewport->KeyState(KEY_LeftShift) || Viewport->KeyState(KEY_RightShift);

	CalcSceneView(&View);

	if( HitResult && HitResult->IsA(TEXT("HPhATBoneProxy")) )
	{
		HPhATBoneProxy* BoneProxy = (HPhATBoneProxy*)HitResult;

		if(AssetEditor->EditingMode == PEM_BodyEdit)
		{
			if(AssetEditor->NextSelectEvent == PNS_EnableCollision)
			{
				AssetEditor->NextSelectEvent = PNS_Normal;
				AssetEditor->SetCollisionBetween( AssetEditor->SelectedBodyIndex, BoneProxy->BodyIndex, true );
			}
			else if(AssetEditor->NextSelectEvent == PNS_DisableCollision)
			{
				AssetEditor->NextSelectEvent = PNS_Normal;
				AssetEditor->SetCollisionBetween( AssetEditor->SelectedBodyIndex, BoneProxy->BodyIndex, false );
			}
			else if(AssetEditor->NextSelectEvent == PNS_CopyProperties)
			{
				AssetEditor->NextSelectEvent = PNS_Normal;
				AssetEditor->CopyBodyProperties( BoneProxy->BodyIndex, AssetEditor->SelectedBodyIndex );
			}
			else if(AssetEditor->NextSelectEvent == PNS_WeldBodies)
			{
				AssetEditor->NextSelectEvent = PNS_Normal;
				AssetEditor->WeldBodyToSelected( BoneProxy->BodyIndex );
			}
			else if(!AssetEditor->bSelectionLock)
			{
				AssetEditor->SetSelectedBody( BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex );
			}
		}
	}
	else if( HitResult && HitResult->IsA(TEXT("HPhATConstraintProxy")) )
	{
		HPhATConstraintProxy* ConstraintProxy = (HPhATConstraintProxy*)HitResult;

		if(AssetEditor->EditingMode == PEM_ConstraintEdit)
		{
			if(AssetEditor->NextSelectEvent == PNS_CopyProperties)
			{
				AssetEditor->NextSelectEvent = PNS_Normal;
				AssetEditor->CopyConstraintProperties( ConstraintProxy->ConstraintIndex, AssetEditor->SelectedConstraintIndex );
			}
			else if(!AssetEditor->bSelectionLock)
			{
				AssetEditor->SetSelectedConstraint( ConstraintProxy->ConstraintIndex );
			}
		}
	}
	else if( HitResult && HitResult->IsA(TEXT("HPhATWidgetProxy")) )
	{
		HPhATWidgetProxy* WidgetProxy = (HPhATWidgetProxy*)HitResult;

		AssetEditor->StartManipulating( WidgetProxy->Axis, FViewportClick(&View, this, Key, Event, HitX, HitY), View.ViewMatrix );
	}
	else if( HitResult && HitResult->IsA(TEXT("HPhATBoneNameProxy")) )
	{
		HPhATBoneNameProxy* NameProxy = (HPhATBoneNameProxy*)HitResult;

		if( AssetEditor->NextSelectEvent == PNS_MakeNewBody )
		{
			AssetEditor->NextSelectEvent = PNS_Normal;
			AssetEditor->MakeNewBody( NameProxy->BoneIndex );
		}
	}
	else
	{	
		if(AssetEditor->NextSelectEvent != PNS_Normal)
		{
			AssetEditor->NextSelectEvent = PNS_Normal;
			Viewport->Invalidate();
		}
		else if(!AssetEditor->bSelectionLock)
		{
			if(AssetEditor->EditingMode == PEM_BodyEdit)
				AssetEditor->SetSelectedBody( INDEX_NONE, KPT_Unknown, INDEX_NONE );	
			else
				AssetEditor->SetSelectedConstraint( INDEX_NONE );
		}
	}
}

void FPhATViewportClient::UpdateLighting()
{
	SkyLightComponent->PreEditChange();
	DirectionalLightComponent->PreEditChange();

	SkyLightComponent->Brightness = AssetEditor->EditorSimOptions->SkyBrightness;
	SkyLightComponent->Color = AssetEditor->EditorSimOptions->SkyColor;
	SkyLightComponent->SetParentToWorld( FMatrix::Identity );

	DirectionalLightComponent->Brightness = AssetEditor->EditorSimOptions->Brightness;
	DirectionalLightComponent->Color = AssetEditor->EditorSimOptions->Color;
	DirectionalLightComponent->SetParentToWorld( FRotationMatrix( FRotator(-AssetEditor->EditorSimOptions->LightElevation*(65535.f/360.f), AssetEditor->EditorSimOptions->LightDirection*(65535.f/360.f), 0.f) ) );

	SkyLightComponent->PostEditChange(NULL);
	DirectionalLightComponent->PostEditChange(NULL);
}

void FPhATViewportClient::Draw(FChildViewport* Viewport, FRenderInterface* RI)
{
	// Turn on/off the ground box
	if(AssetEditor->bDrawGround)
		AssetEditor->EditorFloor->bHiddenEd = false;
	else
		AssetEditor->EditorFloor->bHiddenEd = true;

	// Do main viewport drawing stuff.
	FEditorLevelViewportClient::Draw(Viewport, RI);

	UBOOL bHitTesting = RI->IsHitTesting();

	INT W, H;
	PhATFont->GetCharSize( TEXT('L'), W, H );

	// Write body/constraint count at top.
	FString StatusString = FString::Printf( TEXT("%d BODIES, %d CONSTRAINTS"), AssetEditor->PhysicsAsset->BodySetup.Num(), AssetEditor->PhysicsAsset->ConstraintSetup.Num() );
	RI->DrawString( 3, 3, *StatusString, PhATFont, FLinearColor::White );

	if(AssetEditor->bRunningSimulation)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("SIM"), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_EnableCollision)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("ENABLE COLLISION..."), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_DisableCollision)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("DISABLE COLLISION..."), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_CopyProperties)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("COPY PROPERTIES TO..."), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_WeldBodies)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("WELD TO..."), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_MakeNewBody)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("MAKE NEW BODY..."), PhATFont, FLinearColor::White );
	else if(AssetEditor->bSelectionLock)
		RI->DrawString( 3, Viewport->GetSizeY() - (3 + H) , TEXT("LOCK"), PhATFont, FLinearColor::White );

	if(AssetEditor->bManipulating && !AssetEditor->bRunningSimulation)
	{
		if(AssetEditor->MovementMode == PMM_Translate)
		{
			FString TranslateString = FString::Printf( TEXT("%3.2f"), AssetEditor->ManipulateTranslation );
			RI->DrawString( 3, Viewport->GetSizeY() - (2 * (3 + H)) , *TranslateString, PhATFont, FLinearColor::White );
		}
		else if(AssetEditor->MovementMode == PMM_Rotate)
		{
			// note : The degree symbol is ASCII code 248 (char code 176)
			FString RotateString = FString::Printf( TEXT("%3.2f%c"), AssetEditor->ManipulateRotation * (180.f/PI), 176 );
			RI->DrawString( 3, Viewport->GetSizeY() - (2 * (3 + H)) , *RotateString, PhATFont, FLinearColor::White );
		}
	}

	if(AssetEditor->bShowHierarchy || AssetEditor->NextSelectEvent == PNS_MakeNewBody)
	{
		FSceneView	View;
		CalcSceneView(&View);

		for(INT i=0; i<AssetEditor->EditorSkelComp->SpaceBases.Num(); i++)
		{
			FVector BonePos = AssetEditor->EditorSkelComp->LocalToWorld.TransformFVector( AssetEditor->EditorSkelComp->SpaceBases(i).GetOrigin() );

			FPlane proj = View.Project( BonePos );
			if(proj.W > 0.f) // This avoids drawing bone names that are behind us.
			{
				INT HalfX = Viewport->GetSizeX()/2;
				INT HalfY = Viewport->GetSizeY()/2;
				INT XPos = HalfX + ( HalfX * proj.X );
				INT YPos = HalfY + ( HalfY * (proj.Y * -1) );

				FName BoneName = AssetEditor->EditorSkelMesh->RefSkeleton(i).Name;

				FColor BoneNameColor = FColor(255,255,255);
				if(AssetEditor->SelectedBodyIndex != INDEX_NONE)
				{
					FName SelectedBodyName = AssetEditor->PhysicsAsset->BodySetup(AssetEditor->SelectedBodyIndex)->BoneName;
					if(SelectedBodyName == BoneName)
					{
						BoneNameColor = FColor(0,255,0);
					}
				}

				if(bHitTesting) RI->SetHitProxy( new HPhATBoneNameProxy(i) );
				RI->DrawString(XPos, YPos, *BoneName, PhATFont, BoneNameColor);
				if(bHitTesting) RI->SetHitProxy( NULL );
			}
		}
	}
}


///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// UPhATSkeletalMeshComponent /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void UPhATSkeletalMeshComponent::RenderAssetTools(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI, UBOOL bHitTest, UBOOL bDrawShadows)
{
	check(PhysicsAsset);

	// None of the widgets/collision stuff really needs shadows...
	if( bDrawShadows )
		return;

	// Should always have an Owner in PhAT
	check(Owner);
	FVector Scale3D = Owner->DrawScale * Owner->DrawScale3D;
	check( Scale3D.IsUniform() ); // Something wrong if this is the case...

	APhATActor* PhATActor = Cast<APhATActor>(Owner);
	check(PhATActor);
	WPhAT* PhysEditor = (WPhAT*)PhATActor->PhysicsAssetEditor;

	EPhATRenderMode CollisionViewMode = PhysEditor->GetCurrentCollisionViewMode();

	// Draw bodies
	for( INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
	{
		INT BoneIndex = MatchRefBone( PhysicsAsset->BodySetup(i)->BoneName );
		check(BoneIndex != INDEX_NONE);
		FMatrix BoneTM = GetBoneMatrix(BoneIndex);
		BoneTM.RemoveScaling();

		FKAggregateGeom* AggGeom = &PhysicsAsset->BodySetup(i)->AggGeom;

		for(INT j=0; j<AggGeom->SphereElems.Num(); j++)
		{
			if(bHitTest) PRI->SetHitProxy( new HPhATBoneProxy(i, KPT_Sphere, j) );

			FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Sphere, j, Scale3D.X);

			if(CollisionViewMode == PRM_Solid)
			{
				UMaterialInstance*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Sphere, j);
				AggGeom->SphereElems(j).DrawElemSolid( PRI, ElemTM, Scale3D.X, PrimMaterial->GetMaterialInterface(0), PrimMaterial->GetInstanceInterface() );
			}

			if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
				AggGeom->SphereElems(j).DrawElemWire( PRI, ElemTM, Scale3D.X, PhysEditor->GetPrimitiveColor(i, KPT_Sphere, j) );

			if(bHitTest) PRI->SetHitProxy(NULL);
		}

		for(INT j=0; j<AggGeom->BoxElems.Num(); j++)
		{
			if(bHitTest) PRI->SetHitProxy( new HPhATBoneProxy(i, KPT_Box, j) );

			FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Box, j, Scale3D.X);

			if(CollisionViewMode == PRM_Solid)
			{
				UMaterialInstance*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Box, j);
				AggGeom->BoxElems(j).DrawElemSolid( PRI, ElemTM, Scale3D.X, PrimMaterial->GetMaterialInterface(0), PrimMaterial->GetInstanceInterface() );
			}

			if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
				AggGeom->BoxElems(j).DrawElemWire( PRI, ElemTM, Scale3D.X, PhysEditor->GetPrimitiveColor(i, KPT_Box, j) );

			if(bHitTest) PRI->SetHitProxy(NULL);
		}

		for(INT j=0; j<AggGeom->SphylElems.Num(); j++)
		{
			if(bHitTest) PRI->SetHitProxy( new HPhATBoneProxy(i, KPT_Sphyl, j) );

			FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Sphyl, j, Scale3D.X);

			if(CollisionViewMode == PRM_Solid)
			{
				UMaterialInstance*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Sphyl, j);
				AggGeom->SphylElems(j).DrawElemSolid( PRI, ElemTM, Scale3D.X, PrimMaterial->GetMaterialInterface(0), PrimMaterial->GetInstanceInterface() );
			}

			if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
				AggGeom->SphylElems(j).DrawElemWire( PRI, ElemTM, Scale3D.X, PhysEditor->GetPrimitiveColor(i, KPT_Sphyl, j) );
	
			if(bHitTest) PRI->SetHitProxy(NULL);
		}

		for(INT j=0; j<AggGeom->ConvexElems.Num(); j++)
		{
			if(bHitTest) PRI->SetHitProxy( new HPhATBoneProxy(i, KPT_Convex, j) );

			FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Convex, j, Scale3D.X);

			if(CollisionViewMode == PRM_Solid )
			{
				UMaterialInstance*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Convex, j);
				AggGeom->ConvexElems(j).DrawElemSolid( PRI, ElemTM, Scale3D, PrimMaterial->GetMaterialInterface(0), PrimMaterial->GetInstanceInterface() );
			}

			if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
				AggGeom->ConvexElems(j).DrawElemWire( PRI, ElemTM, Scale3D, PhysEditor->GetPrimitiveColor(i, KPT_Convex, j) );

			if(bHitTest) PRI->SetHitProxy(NULL);
		}

		if(!bHitTest && PhysicsAssetInstance && PhysEditor->bShowCOM)
		{
			PhysicsAssetInstance->Bodies(i)->DrawCOMPosition(PRI, COMRenderSize, COMRenderColor);
		}
	}

	// Draw Constraints
	EPhATConstraintViewMode ConstraintViewMode = PhysEditor->GetCurrentConstraintViewMode();
	if(ConstraintViewMode != PCV_None)
	{
		for(INT i=0; i<PhysicsAsset->ConstraintSetup.Num(); i++)
		{
			if(bHitTest) PRI->SetHitProxy( new HPhATConstraintProxy(i) );

			PhysEditor->DrawConstraint(i, Scale3D.X, PRI);

			if(bHitTest) PRI->SetHitProxy(NULL);
		}
	}

	if(!bHitTest && PhysEditor->EditingMode == PEM_BodyEdit && PhysEditor->bShowInfluences)
		PhysEditor->DrawCurrentInfluences(PRI);
}

void UPhATSkeletalMeshComponent::RenderForegroundAssetTools(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI, UBOOL bHitTest, UBOOL bDrawShadows)
{
	check(PhysicsAsset);

	// None of the widgets/collision stuff really needs shadows...
	if( bDrawShadows )
		return;

	// Should always have an Owner in PhAT
	check(Owner);
	FVector Scale3D = Owner->DrawScale * Owner->DrawScale3D;
	check( Scale3D.IsUniform() ); // Something wrong if this is the case...

	APhATActor* PhATActor = Cast<APhATActor>(Owner);
	check(PhATActor);
	WPhAT* PhysEditor = (WPhAT*)PhATActor->PhysicsAssetEditor;

	// If desired, draw bone hierarchy.
	if(!bHitTest && (PhysEditor->bShowHierarchy || PhysEditor->NextSelectEvent == PNS_MakeNewBody) )
		DrawHierarchy(PRI);

	if(!PhysEditor->bRunningSimulation)
		PhysEditor->DrawCurrentWidget(Context, PRI, bHitTest);	
}

DWORD UPhATSkeletalMeshComponent::GetLayerMask(const FSceneContext& Context) const
{
	DWORD	LayerMask = Super::GetLayerMask(Context) | PLM_Foreground;

	if(((WPhAT*)CastChecked<APhATActor>(Owner)->PhysicsAssetEditor)->GetCurrentCollisionViewMode() == PRM_Solid)
		LayerMask |= PLM_Translucent;

	return LayerMask;
}

void UPhATSkeletalMeshComponent::Render(const FSceneContext& Context, FPrimitiveRenderInterface* PRI)
{
	WPhAT* PhysEditor = (WPhAT*)( ((APhATActor*)Owner)->PhysicsAssetEditor );
	EPhATRenderMode MeshViewMode = PhysEditor->GetCurrentMeshViewMode();

	if(MeshViewMode != PRM_None)
	{
		DWORD	OldViewMode = Context.View->ViewMode;
		if(MeshViewMode == PRM_Wireframe)
			Context.View->ViewMode = SVM_Wireframe;

		Super::Render(Context, PRI);

		Context.View->ViewMode = OldViewMode;
	}

	RenderAssetTools(Context, PRI, 0, 0);

	//FBox AssetBox = PhysicsAsset->CalcAABB(this);
	//PRI->DrawWireBox(AssetBox, FColor(255,255,255), 0);
}

void UPhATSkeletalMeshComponent::RenderForeground(const FSceneContext& Context, FPrimitiveRenderInterface* PRI)
{
	RenderForegroundAssetTools(Context, PRI, 0, 0);
}

void UPhATSkeletalMeshComponent::RenderHitTest(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI)
{
	RenderAssetTools(Context, PRI, 1, 0);
}

void UPhATSkeletalMeshComponent::RenderForegroundHitTest(const FSceneContext& Context, struct FPrimitiveRenderInterface* PRI)
{
	RenderForegroundAssetTools(Context, PRI, 1, 0);
}

void UPhATSkeletalMeshComponent::RenderShadowDepth(const struct FSceneContext& Context, struct FPrimitiveRenderInterface* PRI)
{
	WPhAT* PhysEditor = (WPhAT*)( ((APhATActor*)Owner)->PhysicsAssetEditor );
	EPhATRenderMode MeshViewMode = PhysEditor->GetCurrentMeshViewMode();

	if(MeshViewMode != PRM_None)
	{
		DWORD	OldViewMode = Context.View->ViewMode;
		if(MeshViewMode == PRM_Wireframe)
			Context.View->ViewMode = SVM_Wireframe;

		Super::Render(Context, PRI);

		Context.View->ViewMode = OldViewMode;
	}

	RenderAssetTools(Context, PRI, 0, 1);
	RenderForegroundAssetTools(Context,PRI,0,1);
}

void UPhATSkeletalMeshComponent::DrawHierarchy(FPrimitiveRenderInterface* PRI)
{
	for(INT i=1; i<SpaceBases.Num(); i++)
	{
		INT ParentIndex = SkeletalMesh->RefSkeleton(i).ParentIndex;
		FVector ParentPos = LocalToWorld.TransformFVector( SpaceBases(ParentIndex).GetOrigin() );
		FVector ChildPos = LocalToWorld.TransformFVector( SpaceBases(i).GetOrigin() );

		PRI->DrawLine( ParentPos, ChildPos, HeirarchyDrawColor);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// WPhAT //////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////



void WPhAT::DrawConstraint(INT ConstraintIndex, FLOAT Scale, FPrimitiveRenderInterface* PRI)
{
	EPhATConstraintViewMode ConstraintViewMode = GetCurrentConstraintViewMode();
	UBOOL bDrawLimits = false;
	if(ConstraintViewMode == PCV_AllLimits)
		bDrawLimits = true;
	else if(EditingMode == PEM_ConstraintEdit && ConstraintIndex == SelectedConstraintIndex)
		bDrawLimits = true;

	UBOOL bDrawSelected = false;
	if(!bRunningSimulation && EditingMode == PEM_ConstraintEdit && ConstraintIndex == SelectedConstraintIndex)
		bDrawSelected = true;

	URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup(ConstraintIndex);

	FMatrix Con1Frame = GetConstraintMatrix(ConstraintIndex, 0, Scale);
	FMatrix Con2Frame = GetConstraintMatrix(ConstraintIndex, 1, Scale);

	cs->DrawConstraint(PRI, Scale, bDrawLimits, bDrawSelected, PhATViewportClient->JointLimitMaterial, Con1Frame, Con2Frame);
}

void WPhAT::CycleMeshViewMode()
{
	EPhATRenderMode MeshViewMode = GetCurrentMeshViewMode();
	if(MeshViewMode == PRM_Solid)
		SetCurrentMeshViewMode(PRM_Wireframe);
	else if(MeshViewMode == PRM_Wireframe)
		SetCurrentMeshViewMode(PRM_None);
	else if(MeshViewMode == PRM_None)
		SetCurrentMeshViewMode(PRM_Solid);

	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::CycleCollisionViewMode()
{
	EPhATRenderMode CollisionViewMode = GetCurrentCollisionViewMode();
	if(CollisionViewMode == PRM_Solid)
		SetCurrentCollisionViewMode(PRM_Wireframe);
	else if(CollisionViewMode == PRM_Wireframe)
		SetCurrentCollisionViewMode(PRM_None);
	else if(CollisionViewMode == PRM_None)
		SetCurrentCollisionViewMode(PRM_Solid);

	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::CycleConstraintViewMode()
{
	EPhATConstraintViewMode ConstraintViewMode = GetCurrentConstraintViewMode();
	if(ConstraintViewMode == PCV_None)
		SetCurrentConstraintViewMode(PCV_AllPositions);
	else if(ConstraintViewMode == PCV_AllPositions)
		SetCurrentConstraintViewMode(PCV_AllLimits);
	else if(ConstraintViewMode == PCV_AllLimits)
		SetCurrentConstraintViewMode(PCV_None);

	UpdateToolBarStatus();
	PhATViewportClient->Viewport->Invalidate();
}

// View mode accessors
EPhATRenderMode WPhAT::GetCurrentMeshViewMode()
{
	if(bRunningSimulation)
		return Sim_MeshViewMode;
	else if(EditingMode == PEM_BodyEdit)
		return BodyEdit_MeshViewMode;
	else
		return ConstraintEdit_MeshViewMode;
}

EPhATRenderMode WPhAT::GetCurrentCollisionViewMode()
{
	if(bRunningSimulation)
		return Sim_CollisionViewMode;
	else if(EditingMode == PEM_BodyEdit)
		return BodyEdit_CollisionViewMode;
	else
		return ConstraintEdit_CollisionViewMode;
}

EPhATConstraintViewMode WPhAT::GetCurrentConstraintViewMode()
{
	if(bRunningSimulation)
		return Sim_ConstraintViewMode;
	else if(EditingMode == PEM_BodyEdit)
		return BodyEdit_ConstraintViewMode;
	else
		return ConstraintEdit_ConstraintViewMode;
}

// View mode mutators
void WPhAT::SetCurrentMeshViewMode(EPhATRenderMode NewViewMode)
{
	if(bRunningSimulation)
		Sim_MeshViewMode = NewViewMode;
	else if(EditingMode == PEM_BodyEdit)
		BodyEdit_MeshViewMode = NewViewMode;
	else
		ConstraintEdit_MeshViewMode = NewViewMode;
}

void WPhAT::SetCurrentCollisionViewMode(EPhATRenderMode NewViewMode)
{
	if(bRunningSimulation)
		Sim_CollisionViewMode = NewViewMode;
	else if(EditingMode == PEM_BodyEdit)
		BodyEdit_CollisionViewMode = NewViewMode;
	else
		ConstraintEdit_CollisionViewMode = NewViewMode;
}

void WPhAT::SetCurrentConstraintViewMode(EPhATConstraintViewMode NewViewMode)
{
	if(bRunningSimulation)
		Sim_ConstraintViewMode = NewViewMode;
	else if(EditingMode == PEM_BodyEdit)
		BodyEdit_ConstraintViewMode = NewViewMode;
	else
		ConstraintEdit_ConstraintViewMode = NewViewMode;
}



void WPhAT::ToggleViewCOM()
{
	bShowCOM = !bShowCOM;

	if(bShowCOM)
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SHOWCOM, (LPARAM) MAKELONG(1, 0));
	else
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SHOWCOM, (LPARAM) MAKELONG(0, 0));

	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::ToggleViewHierarchy()
{
	bShowHierarchy = !bShowHierarchy;

	if(bShowHierarchy)
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SHOWHIERARCHY, (LPARAM) MAKELONG(1, 0));
	else
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SHOWHIERARCHY, (LPARAM) MAKELONG(0, 0));

	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::ToggleViewInfluences()
{
	bShowInfluences = !bShowInfluences;

	if(bShowInfluences)
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SHOWINFLUENCES, (LPARAM) MAKELONG(1, 0));
	else
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_SHOWINFLUENCES, (LPARAM) MAKELONG(0, 0));

	PhATViewportClient->Viewport->Invalidate();
}

void WPhAT::ToggleDrawGround()
{
	bDrawGround = !bDrawGround;

	if(bDrawGround)
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_DRAWGROUND, (LPARAM) MAKELONG(1, 0));
	else
		SendMessageX( hWndToolBar, TB_CHECKBUTTON,  IDMN_PHAT_DRAWGROUND, (LPARAM) MAKELONG(0, 0));

	PhATViewportClient->Viewport->Invalidate();
}

FColor WPhAT::GetPrimitiveColor(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
{
	if(!bRunningSimulation && EditingMode == PEM_ConstraintEdit && SelectedConstraintIndex != INDEX_NONE)
	{
		URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup( SelectedConstraintIndex );
		URB_BodySetup* bs = PhysicsAsset->BodySetup( BodyIndex );

		if(cs->ConstraintBone1 == bs->BoneName)
			return ConstraintBone1Color;
		else if(cs->ConstraintBone2 == bs->BoneName)
			return ConstraintBone2Color;
	}

	if(bRunningSimulation || EditingMode == PEM_ConstraintEdit)
		return BoneUnselectedColor;

	if(BodyIndex == SelectedBodyIndex)
	{
		if(PrimitiveType == SelectedPrimitiveType && PrimitiveIndex == SelectedPrimitiveIndex)
			return ElemSelectedColor;
		else
			return BoneSelectedColor;
	}
	else
	{
		// If there is no collision with this body, use 'no collision material'.
		if( NoCollisionBodies.FindItemIndex(BodyIndex) != INDEX_NONE )
			return NoCollisionColor;
		else
			return BoneUnselectedColor;
	}
}

UMaterialInstance* WPhAT::GetPrimitiveMaterial(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
{
	if(bRunningSimulation || EditingMode == PEM_ConstraintEdit)
		return PhATViewportClient->BoneUnselectedMaterial;

	if(BodyIndex == SelectedBodyIndex)
	{
		if(PrimitiveType == SelectedPrimitiveType && PrimitiveIndex == SelectedPrimitiveIndex)
			return PhATViewportClient->ElemSelectedMaterial;
		else
			return PhATViewportClient->BoneSelectedMaterial;
	}
	else
	{
		// If there is no collision with this body, use 'no collision material'.
		if( NoCollisionBodies.FindItemIndex(BodyIndex) != INDEX_NONE )
			return PhATViewportClient->BoneNoCollisionMaterial;
		else
			return PhATViewportClient->BoneUnselectedMaterial;
	}
}

// Draw little coloured lines to indicate which vertices are influenced by currently selected physics body.
void WPhAT::DrawCurrentInfluences(FPrimitiveRenderInterface* PRI)
{
	// For each influenced bone, draw a little line for each vertex
	for( INT i=0; i<ControlledBones.Num(); i++)
	{
		INT BoneIndex = ControlledBones(i);
		FMatrix BoneTM = EditorSkelComp->GetBoneMatrix( BoneIndex );

		FBoneVertInfo* BoneInfo = &DominantWeightBoneInfos( BoneIndex );
		//FBoneVertInfo* BoneInfo = &AnyWeightBoneInfos( BoneIndex );
		check( BoneInfo->Positions.Num() == BoneInfo->Normals.Num() );

		for(INT j=0; j<BoneInfo->Positions.Num(); j++)
		{
			FVector WPos = BoneTM.TransformFVector( BoneInfo->Positions(j) );
			FVector WNorm = BoneTM.TransformNormal( BoneInfo->Normals(j) );

			PRI->DrawLine(WPos, WPos + WNorm * InfluenceLineLength, InfluenceLineColor);
		}
	}
}

// This will update WPhAT::WidgetTM
void WPhAT::DrawCurrentWidget(const FSceneContext& Context, FPrimitiveRenderInterface* PRI, UBOOL bHitTest)
{
	FVector Scale3D = EditorActor->DrawScale3D * EditorActor->DrawScale;

	if(EditingMode == PEM_BodyEdit) /// BODY EDITING ///
	{
		// Don't draw widget if nothing selected.
		if(SelectedBodyIndex == INDEX_NONE)
			return;

		INT BoneIndex = EditorSkelComp->MatchRefBone( PhysicsAsset->BodySetup(SelectedBodyIndex)->BoneName );

		FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
		BoneTM.RemoveScaling();

		WidgetTM = GetPrimitiveMatrix(BoneTM, SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex, Scale3D.X);
	}
	else  /// CONSTRAINT EDITING ///
	{
		if(SelectedConstraintIndex == INDEX_NONE)
			return;

		WidgetTM = GetConstraintMatrix(SelectedConstraintIndex, 1, Scale3D.X);
	}

	FVector WidgetOrigin = WidgetTM.GetOrigin();

	FLOAT ZoomFactor = Min<FLOAT>(Context.View->ProjectionMatrix.M[0][0], Context.View->ProjectionMatrix.M[1][1]);
	FLOAT WidgetRadius = Context.View->Project(WidgetOrigin).W * (WidgetSize / ZoomFactor);

	FColor XColor(255, 0, 0);
	FColor YColor(0, 255, 0);
	FColor ZColor(0, 0, 255);

	if(bManipulating)
	{
		if(ManipulateAxis == AXIS_X)
			XColor = FColor(255, 255, 0);
		else if(ManipulateAxis == AXIS_Y)
			YColor = FColor(255, 255, 0);
		else
			ZColor = FColor(255, 255, 0);
	}

	FVector XAxis, YAxis, ZAxis;

	////////////////// ARROW WIDGET //////////////////
	if(MovementMode == PMM_Translate || MovementMode == PMM_Scale)
	{
		if(MovementMode == PMM_Translate)
		{
			if(MovementSpace == PMS_Local)
			{
				XAxis = WidgetTM.GetAxis(0); YAxis = WidgetTM.GetAxis(1); ZAxis = WidgetTM.GetAxis(2);
			}
			else
			{
				XAxis = FVector(1,0,0); YAxis = FVector(0,1,0); ZAxis = FVector(0,0,1);
			}
		}
		else
		{
			// Can only scale primitives in their local reference frame.
			XAxis = WidgetTM.GetAxis(0); YAxis = WidgetTM.GetAxis(1); ZAxis = WidgetTM.GetAxis(2);
		}

		FMatrix WidgetMatrix;

		if(bHitTest)
		{
			PRI->SetHitProxy( new HPhATWidgetProxy(AXIS_X) );
			WidgetMatrix = FMatrix(XAxis, YAxis, ZAxis, WidgetOrigin);
			PRI->DrawDirectionalArrow(WidgetMatrix, XColor, WidgetRadius);
			PRI->SetHitProxy( NULL );

			PRI->SetHitProxy( new HPhATWidgetProxy(AXIS_Y) );
			WidgetMatrix = FMatrix(YAxis, ZAxis, XAxis, WidgetOrigin);
			PRI->DrawDirectionalArrow(WidgetMatrix, YColor, WidgetRadius);
			PRI->SetHitProxy( NULL );

			PRI->SetHitProxy( new HPhATWidgetProxy(AXIS_Z) );
			WidgetMatrix = FMatrix(ZAxis, XAxis, YAxis, WidgetOrigin);
			PRI->DrawDirectionalArrow(WidgetMatrix, ZColor, WidgetRadius);
			PRI->SetHitProxy( NULL );
		}
		else
		{
			WidgetMatrix = FMatrix(XAxis, YAxis, ZAxis, WidgetOrigin);
			PRI->DrawDirectionalArrow(WidgetMatrix, XColor, WidgetRadius);

			WidgetMatrix = FMatrix(YAxis, ZAxis, XAxis, WidgetOrigin);
			PRI->DrawDirectionalArrow(WidgetMatrix, YColor, WidgetRadius);

			WidgetMatrix = FMatrix(ZAxis, XAxis, YAxis, WidgetOrigin);
			PRI->DrawDirectionalArrow(WidgetMatrix, ZColor, WidgetRadius);
		}
	}
	////////////////// CIRCLES WIDGET //////////////////
	else if(MovementMode == PMM_Rotate)
	{
		// ViewMatrix is WorldToCamera, so invert to get CameraToWorld
		FVector LookDir = Context.View->ViewMatrix.Inverse().TransformNormal( FVector(0,0,1) );

		if(MovementSpace == PMS_Local)
		{
			XAxis = WidgetTM.GetAxis(0); YAxis = WidgetTM.GetAxis(1); ZAxis = WidgetTM.GetAxis(2);
		}
		else
		{
			XAxis = FVector(1,0,0); YAxis = FVector(0,1,0); ZAxis = FVector(0,0,1);
		}

		if(bHitTest)
		{
			PRI->SetHitProxy( new HPhATWidgetProxy(AXIS_X));
			PRI->DrawCircle(WidgetOrigin, YAxis, ZAxis, XColor, WidgetRadius, 24);
			PRI->SetHitProxy( NULL );

			PRI->SetHitProxy( new HPhATWidgetProxy(AXIS_Y) );
			PRI->DrawCircle(WidgetOrigin, XAxis, ZAxis, YColor, WidgetRadius, 24);
			PRI->SetHitProxy( NULL );

			PRI->SetHitProxy( new HPhATWidgetProxy(AXIS_Z) );
			PRI->DrawCircle(WidgetOrigin, XAxis, YAxis, ZColor, WidgetRadius, 24);
			PRI->SetHitProxy( NULL );
		}
		else
		{
			PRI->DrawCircle(WidgetOrigin, YAxis, ZAxis, XColor, WidgetRadius, 24);
			PRI->DrawCircle(WidgetOrigin, XAxis, ZAxis, YColor, WidgetRadius, 24);
			PRI->DrawCircle(WidgetOrigin, XAxis, YAxis, ZColor, WidgetRadius, 24);
		}
	}
}
