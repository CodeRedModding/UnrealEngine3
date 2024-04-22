/*=============================================================================
	PhATRender.cpp: Physics Asset Tool rendering support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "PhAT.h"
#include "EnginePhysicsClasses.h"
#include "EngineAnimClasses.h"
#include "ScopedTransaction.h"
#include "..\..\Launch\Resources\resource.h"

static const FColor BoneUnselectedColor(170,155,225);
static const FColor BoneSelectedColor(185,70,0);
static const FColor ElemSelectedColor(255,166,0);
static const FColor NoCollisionColor(200, 200, 200);
static const FColor FixedColor(225,64,64);
static const FColor	ConstraintBone1Color(255,0,0);
static const FColor ConstraintBone2Color(0,0,255);

static const FColor HeirarchyDrawColor(220, 255, 220);
static const FColor AnimSkelDrawColor(255, 64, 64);

extern FLOAT UnrealEd_WidgetSize; // Proportion of the viewport the widget should fill

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


FPhATViewportClient::FPhATViewportClient(WxPhAT* InAssetEditor):
AssetEditor(InAssetEditor)
{
	// Setup defaults for the common draw helper.
	DrawHelper.bDrawPivot = false;
	DrawHelper.bDrawWorldBox = false;
	DrawHelper.bDrawKillZ = false;
	DrawHelper.GridColorHi = FColor(80,80,80);
	DrawHelper.GridColorLo = FColor(72,72,72);
	DrawHelper.PerspectiveGridSize = 32767;

	// No postprocess
	ShowFlags &= ~SHOW_PostProcess;

	// Create SkeletalMeshComponent for rendering skeletal mesh
	InAssetEditor->EditorSkelComp = ConstructObject<UPhATSkeletalMeshComponent>(UPhATSkeletalMeshComponent::StaticClass());
	InAssetEditor->EditorSkelComp->BlockRigidBody = TRUE;
	InAssetEditor->EditorSkelComp->PhATPtr = InAssetEditor;
	PreviewScene.AddComponent(InAssetEditor->EditorSkelComp,FMatrix::Identity);

	InAssetEditor->EditorSeqNode = CastChecked<UAnimNodeSequence>( InAssetEditor->EditorSkelComp->Animations );

	// Create floor component
	UStaticMesh* FloorMesh = LoadObject<UStaticMesh>(NULL, TEXT("EditorMeshes.PhAT_FloorBox"), NULL, LOAD_None, NULL);
	check(FloorMesh);

	InAssetEditor->EditorFloorComp = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass());
	InAssetEditor->EditorFloorComp->StaticMesh = FloorMesh;
	InAssetEditor->EditorFloorComp->BlockRigidBody = TRUE;
	InAssetEditor->EditorFloorComp->Scale = 4.f;
	PreviewScene.AddComponent(InAssetEditor->EditorFloorComp,FMatrix::Identity);
	
	ShowFlags = SHOW_DefaultEditor; 

	// Set the viewport to be lit
	ShowFlags &= ~SHOW_ViewMode_Mask;
	ShowFlags |= SHOW_ViewMode_Lit;

	PhATFont = GEngine->SmallFont;
	check(PhATFont);

	// Body materials
	ElemSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_ElemSelectedMaterial"), NULL, LOAD_None, NULL);
	check(ElemSelectedMaterial);

	BoneSelectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_BoneSelectedMaterial"), NULL, LOAD_None, NULL);
	check(BoneSelectedMaterial);

	BoneUnselectedMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_UnselectedMaterial"), NULL, LOAD_None, NULL);
	check(BoneUnselectedMaterial);

	BoneNoCollisionMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_NoCollisionMaterial"), NULL, LOAD_None, NULL);
	check(BoneNoCollisionMaterial);

	JointLimitMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_None, NULL);
	check(JointLimitMaterial);
	bAllowMayaCam = TRUE;

	SetRealtime( TRUE );

	DistanceDragged = 0;
}

void FPhATViewportClient::Serialize(FArchive& Ar)
{ 
	Ar << Input; 
	Ar << PreviewScene;
}

FLinearColor FPhATViewportClient::GetBackgroundColor()
{
	return FColor(64,64,64);
}

void FPhATViewportClient::DoHitTest(FViewport* Viewport, FName Key, EInputEvent Event)
{
	INT			HitX = Viewport->GetMouseX(),
				HitY = Viewport->GetMouseY();
	HHitProxy*	HitResult = Viewport->GetHitProxy(HitX,HitY);

	FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
	FSceneView* View = CalcSceneView(&ViewFamily);

	if(Event == IE_Pressed)
	{
		if( HitResult && HitResult->IsA(HPhATWidgetProxy::StaticGetType()) )
		{
			HPhATWidgetProxy* WidgetProxy = (HPhATWidgetProxy*)HitResult;

			AssetEditor->StartManipulating( WidgetProxy->Axis, FViewportClick(View, this, Key, Event, HitX, HitY), View->ViewMatrix );
		}
	}
	else if(DistanceDragged < 4)
	{
		if( HitResult && HitResult->IsA(HPhATBoneProxy::StaticGetType()) )
		{
			HPhATBoneProxy* BoneProxy = (HPhATBoneProxy*)HitResult;

			AssetEditor->HitBone( BoneProxy->BodyIndex, BoneProxy->PrimType, BoneProxy->PrimIndex );
		}
		else if( HitResult && HitResult->IsA(HPhATConstraintProxy::StaticGetType()) )
		{
			HPhATConstraintProxy* ConstraintProxy = (HPhATConstraintProxy*)HitResult;

			AssetEditor->HitConstraint( ConstraintProxy->ConstraintIndex );
		}
		else if( HitResult && HitResult->IsA(HPhATBoneNameProxy::StaticGetType()) )
		{
			HPhATBoneNameProxy* NameProxy = (HPhATBoneNameProxy*)HitResult;

			if( AssetEditor->NextSelectEvent == PNS_MakeNewBody )
			{
				AssetEditor->NextSelectEvent = PNS_Normal;
				AssetEditor->MakeNewBody( NameProxy->BoneIndex );

				// Rebuild tree to not show all bones.
				AssetEditor->FillTree();
			}
		}
		else
		{	
			AssetEditor->HitNothing();
		}
	}
}

void FPhATViewportClient::UpdateLighting()
{
	PreviewScene.SetSkyBrightness(AssetEditor->EditorSimOptions->SkyBrightness);
	PreviewScene.SetLightBrightness(AssetEditor->EditorSimOptions->Brightness);
}

void FPhATViewportClient::Draw(FViewport* Viewport, FCanvas* Canvas)
{
	// Turn on/off the ground box
	AssetEditor->EditorFloorComp->SetHiddenEditor(!AssetEditor->bDrawGround);

	// Do main viewport drawing stuff.
	FEditorLevelViewportClient::Draw(Viewport, Canvas);

	FLOAT W, H;
	PhATFont->GetCharSize( TEXT('L'), W, H );

	// Write body/constraint count at top.
	FString StatusString = FString::Printf( LocalizeSecure(LocalizeUnrealEd("BodiesConstraints_F"),
		  AssetEditor->PhysicsAsset->BodySetup.Num()
		, AssetEditor->PhysicsAsset->BoundsBodies.Num()
		, static_cast<FLOAT>(AssetEditor->PhysicsAsset->BoundsBodies.Num())/static_cast<FLOAT>(AssetEditor->PhysicsAsset->BodySetup.Num())
		, AssetEditor->PhysicsAsset->ConstraintSetup.Num()) );
	
	DrawString(Canvas, 3, 3, *StatusString, PhATFont, FLinearColor::White );

	if(AssetEditor->bRunningSimulation)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("Sim"), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_EnableCollision)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("EnableCollision"), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_DisableCollision)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("DisableCollision"), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_CopyProperties)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("CopyPropertiesTo"), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_WeldBodies)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("WeldTo"), PhATFont, FLinearColor::White );
	else if(AssetEditor->NextSelectEvent == PNS_MakeNewBody)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("MakeNewBody"), PhATFont, FLinearColor::White );
	else if(AssetEditor->bSelectionLock)
		DrawString(Canvas, 3, Viewport->GetSizeY() - (3 + H) , *LocalizeUnrealEd("Lock"), PhATFont, FLinearColor::White );

	if(AssetEditor->bManipulating && !AssetEditor->bRunningSimulation)
	{
		if(AssetEditor->MovementMode == PMM_Translate)
		{
			FString TranslateString = FString::Printf( TEXT("%3.2f"), AssetEditor->ManipulateTranslation );
			DrawString(Canvas, 3, Viewport->GetSizeY() - (2 * (3 + H)) , *TranslateString, PhATFont, FLinearColor::White );
		}
		else if(AssetEditor->MovementMode == PMM_Rotate)
		{
			// note : The degree symbol is ASCII code 248 (char code 176)
			FString RotateString = FString::Printf( TEXT("%3.2f%c"), AssetEditor->ManipulateRotation * (180.f/PI), 176 );
			DrawString(Canvas, 3, Viewport->GetSizeY() - (2 * (3 + H)) , *RotateString, PhATFont, FLinearColor::White );
		}
	}

	// Draw current physics weight
	if(AssetEditor->bRunningSimulation)
	{
		FString PhysWeightString = FString::Printf( TEXT("Phys Blend: %3.0f pct"), AssetEditor->EditorSkelComp->PhysicsWeight * 100.f );
		INT PWLW, PWLH;
		StringSize( PhATFont, PWLW, PWLH, *PhysWeightString );
		DrawString( Canvas, Viewport->GetSizeX() - (3 + PWLW + 2*W), Viewport->GetSizeY() - (3 + H), *PhysWeightString, PhATFont, FLinearColor::White );
	}

	// Get the SceneView. Need this for projecting world->screen
	FSceneViewFamilyContext ViewFamily(Viewport,GetScene(),ShowFlags,GWorld->GetTimeSeconds(),GWorld->GetDeltaSeconds(),GWorld->GetRealTimeSeconds());
	FSceneView* View = CalcSceneView(&ViewFamily);

	INT HalfX = Viewport->GetSizeX()/2;
	INT HalfY = Viewport->GetSizeY()/2;

	if((AssetEditor->bShowHierarchy && AssetEditor->EditorSimOptions->bShowNamesInHierarchy)|| AssetEditor->NextSelectEvent == PNS_MakeNewBody)
	{
		// Iterate over each graphics bone.
		for(INT i=0; i<AssetEditor->EditorSkelComp->SpaceBases.Num(); i++)
		{
			FVector BonePos = AssetEditor->EditorSkelComp->LocalToWorld.TransformFVector( AssetEditor->EditorSkelComp->SpaceBases(i).GetOrigin() );

			FPlane proj = View->Project( BonePos );
			if(proj.W > 0.f) // This avoids drawing bone names that are behind us.
			{
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

				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( new HPhATBoneNameProxy(i) );
				DrawString(Canvas,XPos, YPos, *BoneName.ToString(), PhATFont, BoneNameColor);
				if(Canvas->IsHitTesting()) Canvas->SetHitProxy( NULL );
			}
		}
	}

	// If showing center-of-mass, and physics is started up..
	if(AssetEditor->bShowCOM && AssetEditor->EditorSkelComp->PhysicsAssetInstance)
	{
		// iterate over each bone
		for(INT i=0; i<AssetEditor->EditorSkelComp->PhysicsAssetInstance->Bodies.Num(); i++)
		{
			URB_BodyInstance* BodyInst = AssetEditor->EditorSkelComp->PhysicsAssetInstance->Bodies(i);
			check(BodyInst);

			FVector BodyCOMPos = BodyInst->GetCOMPosition();
			FLOAT BodyMass = BodyInst->GetBodyMass();

			FPlane proj = View->Project( BodyCOMPos );
			if(proj.W > 0.f) // This avoids drawing bone names that are behind us.
			{
				INT XPos = HalfX + ( HalfX * proj.X );
				INT YPos = HalfY + ( HalfY * (proj.Y * -1) );

				FString COMString = FString::Printf( TEXT("%3.3f"), BodyMass );

				DrawString(Canvas,XPos, YPos, *COMString, PhATFont, COMRenderColor);
			}
		}
	}
}

void FPhATViewportClient::Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	EPhATRenderMode MeshViewMode = AssetEditor->GetCurrentMeshViewMode();

	if(MeshViewMode != PRM_None)
	{
		AssetEditor->EditorSkelComp->SetHiddenEditor(FALSE);

		if(MeshViewMode == PRM_Wireframe)
		{
			AssetEditor->EditorSkelComp->SetForceWireframe(TRUE);
		}
		else
		{
			AssetEditor->EditorSkelComp->SetForceWireframe(FALSE);
		}
	}
	else
	{
		AssetEditor->EditorSkelComp->SetHiddenEditor(TRUE);
	}


	// Draw common scene elements first.
	DrawHelper.Draw(View, PDI);

	// Draw phat skeletal component.
	if(PDI->IsHitTesting())
	{
		AssetEditor->EditorSkelComp->RenderHitTest(View, PDI);
	}
	else
	{
		AssetEditor->EditorSkelComp->Render(View, PDI);
	}
	
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// UPhATSkeletalMeshComponent /////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////

void UPhATSkeletalMeshComponent::RenderAssetTools(const FSceneView* View, class FPrimitiveDrawInterface* PDI, UBOOL bHitTest)
{
	check(PhysicsAsset);

	WxPhAT* PhysEditor = (WxPhAT*)PhATPtr;
	check(PhysEditor);

	EPhATRenderMode CollisionViewMode = PhysEditor->GetCurrentCollisionViewMode();

	
	// Draw bodies
	for( INT i=0; i<PhysicsAsset->BodySetup.Num(); i++)
	{
		INT BoneIndex = MatchRefBone( PhysicsAsset->BodySetup(i)->BoneName );

		// If we found a bone for it, draw the collision.
		if(BoneIndex != INDEX_NONE)
		{
			FMatrix BoneTM = GetBoneMatrix(BoneIndex);
			BoneTM.RemoveScaling();

			FKAggregateGeom* AggGeom = &PhysicsAsset->BodySetup(i)->AggGeom;

			for(INT j=0; j<AggGeom->SphereElems.Num(); j++)
			{
				if(bHitTest) PDI->SetHitProxy( new HPhATBoneProxy(i, KPT_Sphere, j) );

				FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Sphere, j, 1.f);

				if(CollisionViewMode == PRM_Solid)
				{
					UMaterialInterface*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Sphere, j);
					AggGeom->SphereElems(j).DrawElemSolid( PDI, ElemTM, 1.f, PrimMaterial->GetRenderProxy(0) );
				}

				if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
					AggGeom->SphereElems(j).DrawElemWire( PDI, ElemTM, 1.f, PhysEditor->GetPrimitiveColor(i, KPT_Sphere, j) );

				if(bHitTest) PDI->SetHitProxy(NULL);
			}

			for(INT j=0; j<AggGeom->BoxElems.Num(); j++)
			{
				if(bHitTest) PDI->SetHitProxy( new HPhATBoneProxy(i, KPT_Box, j) );

				FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Box, j, 1.f);

				if(CollisionViewMode == PRM_Solid)
				{
					UMaterialInterface*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Box, j);
					AggGeom->BoxElems(j).DrawElemSolid( PDI, ElemTM, 1.f, PrimMaterial->GetRenderProxy(0) );
				}

				if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
					AggGeom->BoxElems(j).DrawElemWire( PDI, ElemTM, 1.f, PhysEditor->GetPrimitiveColor(i, KPT_Box, j) );

				if(bHitTest) PDI->SetHitProxy(NULL);
			}

			for(INT j=0; j<AggGeom->SphylElems.Num(); j++)
			{
				if(bHitTest) PDI->SetHitProxy( new HPhATBoneProxy(i, KPT_Sphyl, j) );

				FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Sphyl, j, 1.f);

				if(CollisionViewMode == PRM_Solid)
				{
					UMaterialInterface*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Sphyl, j);
					AggGeom->SphylElems(j).DrawElemSolid( PDI, ElemTM, 1.f, PrimMaterial->GetRenderProxy(0) );
				}

				if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
					AggGeom->SphylElems(j).DrawElemWire( PDI, ElemTM, 1.f, PhysEditor->GetPrimitiveColor(i, KPT_Sphyl, j) );

				if(bHitTest) PDI->SetHitProxy(NULL);
			}

			for(INT j=0; j<AggGeom->ConvexElems.Num(); j++)
			{
				if(bHitTest) PDI->SetHitProxy( new HPhATBoneProxy(i, KPT_Convex, j) );

				FMatrix ElemTM = PhysEditor->GetPrimitiveMatrix(BoneTM, i, KPT_Convex, j, 1.f);

#if 0 // JTODO when we add convex support to PhAT.
				if(CollisionViewMode == PRM_Solid )
				{
					UMaterialInterface*	PrimMaterial = PhysEditor->GetPrimitiveMaterial(i, KPT_Convex, j);
					AggGeom->ConvexElems(j).DrawElemSolid( PDI, ElemTM, FVector(1.f), PrimMaterial->GetRenderProxy(0) );
				}
#endif

				if(CollisionViewMode == PRM_Solid || CollisionViewMode == PRM_Wireframe)
					AggGeom->ConvexElems(j).DrawElemWire( PDI, ElemTM, FVector(1.f), PhysEditor->GetPrimitiveColor(i, KPT_Convex, j) );

				if(bHitTest) PDI->SetHitProxy(NULL);
			}

			if(!bHitTest && PhysicsAssetInstance && PhysEditor->bShowCOM)
			{
				PhysicsAssetInstance->Bodies(i)->DrawCOMPosition(PDI, COMRenderSize, COMRenderColor);
			}
		}
	}

	// Draw Constraints
	EPhATConstraintViewMode ConstraintViewMode = PhysEditor->GetCurrentConstraintViewMode();
	if(ConstraintViewMode != PCV_None)
	{
		for(INT i=0; i<PhysicsAsset->ConstraintSetup.Num(); i++)
		{
			if(bHitTest) PDI->SetHitProxy( new HPhATConstraintProxy(i) );
			PhysEditor->DrawConstraint(i, View, PDI, PhysEditor->EditorSimOptions->bShowConstraintsAsPoints);
			if(bHitTest) PDI->SetHitProxy(NULL);
		}
	}

	if(!bHitTest && PhysEditor->EditingMode == PEM_BodyEdit && PhysEditor->bShowInfluences)
	{
		PhysEditor->DrawCurrentInfluences(PDI);
	}

	// If desired, draw bone hierarchy.
	if(!bHitTest && (PhysEditor->bShowHierarchy || PhysEditor->NextSelectEvent == PNS_MakeNewBody) )
	{
		DrawHierarchy(PDI, FALSE);
	}

	// If desired, draw animation skeleton (only when simulating).
	if(!bHitTest && PhysEditor->bShowAnimSkel && PhysEditor->bRunningSimulation)
	{
		DrawHierarchy(PDI, TRUE);
	}

	if(!PhysEditor->bRunningSimulation)
	{
		PhysEditor->DrawCurrentWidget(View, PDI, bHitTest);	
	}
}

/** Renders non-hitproxy elements for the viewport, this function is called in the Game Thread. */
void UPhATSkeletalMeshComponent::Render(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	RenderAssetTools(View, PDI, 0);

	//FBox AssetBox = PhysicsAsset->CalcAABB(this);
	//DrawWireBox(PDI,AssetBox, FColor(255,255,255), 0);
}

/** Renders hitproxy elements for the viewport, this function is called in the Game Thread. */
void UPhATSkeletalMeshComponent::RenderHitTest(const FSceneView* View, class FPrimitiveDrawInterface* PDI)
{
	RenderAssetTools(View, PDI, 1);
}

/**
 * Creates a proxy to represent the primitive to the scene manager in the rendering thread.
 * @return The proxy object.
 */
FPrimitiveSceneProxy* UPhATSkeletalMeshComponent::CreateSceneProxy()
{
	WxPhAT* PhysEditor = (WxPhAT*)PhATPtr;
	check(PhysEditor);

	FPrimitiveSceneProxy* Proxy = NULL;

	EPhATRenderMode MeshViewMode = PhysEditor->GetCurrentMeshViewMode();
	if(MeshViewMode != PRM_None)
	{
		Proxy = USkeletalMeshComponent::CreateSceneProxy();
	}

	return Proxy;
}

/** Draw the mesh skeleton using lines. bAnimSkel means draw the animation skeleton. */
void UPhATSkeletalMeshComponent::DrawHierarchy(FPrimitiveDrawInterface* PDI, UBOOL bAnimSkel)
{
	for(INT i=1; i<SpaceBases.Num(); i++)
	{
		INT ParentIndex = SkeletalMesh->RefSkeleton(i).ParentIndex;

		FVector ParentPos, ChildPos;
		if(bAnimSkel)
		{
			ParentPos = LocalToWorld.TransformFVector( AnimationSpaceBases(ParentIndex).GetOrigin() );
			ChildPos = LocalToWorld.TransformFVector( AnimationSpaceBases(i).GetOrigin() );
		}
		else
		{
			ParentPos = LocalToWorld.TransformFVector( SpaceBases(ParentIndex).GetOrigin() );
			ChildPos = LocalToWorld.TransformFVector( SpaceBases(i).GetOrigin() );
		}

		FColor DrawColor = bAnimSkel ? AnimSkelDrawColor : HeirarchyDrawColor;
		PDI->DrawLine( ParentPos, ChildPos, DrawColor, SDPG_Foreground);
	}
}

///////////////////////////////////////////////////////////////////////////////////////////////////
////////////////// WPhAT //////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////



void WxPhAT::DrawConstraint(INT ConstraintIndex, const FSceneView* View, FPrimitiveDrawInterface* PDI, UBOOL bDrawAsPoint)
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

	FMatrix Con1Frame = GetConstraintMatrix(ConstraintIndex, 0, 1.f);
	FMatrix Con2Frame = GetConstraintMatrix(ConstraintIndex, 1, 1.f);

	FLOAT ZoomFactor = Min<FLOAT>(View->ProjectionMatrix.M[0][0], View->ProjectionMatrix.M[1][1]);
	FLOAT DrawScale = View->Project(Con1Frame.GetOrigin()).W * (EditorSimOptions->ConstraintDrawSize / ZoomFactor);

	cs->DrawConstraint(PDI, 1.f, DrawScale, bDrawLimits, bDrawSelected, PhATViewportClient->JointLimitMaterial, Con1Frame, Con2Frame, bDrawAsPoint);
}

void WxPhAT::CycleMeshViewMode()
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

void WxPhAT::CycleCollisionViewMode()
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

void WxPhAT::CycleConstraintViewMode()
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

void WxPhAT::ReattachPhATComponent()
{
	FComponentReattachContext ReattachContext(EditorSkelComp);
}

// View mode accessors
EPhATRenderMode WxPhAT::GetCurrentMeshViewMode()
{
	if(bRunningSimulation)
		return Sim_MeshViewMode;
	else if(EditingMode == PEM_BodyEdit)
		return BodyEdit_MeshViewMode;
	else
		return ConstraintEdit_MeshViewMode;
}

EPhATRenderMode WxPhAT::GetCurrentCollisionViewMode()
{
	if(bRunningSimulation)
		return Sim_CollisionViewMode;
	else if(EditingMode == PEM_BodyEdit)
		return BodyEdit_CollisionViewMode;
	else
		return ConstraintEdit_CollisionViewMode;
}

EPhATConstraintViewMode WxPhAT::GetCurrentConstraintViewMode()
{
	if(bRunningSimulation)
		return Sim_ConstraintViewMode;
	else if(EditingMode == PEM_BodyEdit)
		return BodyEdit_ConstraintViewMode;
	else
		return ConstraintEdit_ConstraintViewMode;
}

// View mode mutators
void WxPhAT::SetCurrentMeshViewMode(EPhATRenderMode NewViewMode)
{
	if(bRunningSimulation)
		Sim_MeshViewMode = NewViewMode;
	else if(EditingMode == PEM_BodyEdit)
		BodyEdit_MeshViewMode = NewViewMode;
	else
		ConstraintEdit_MeshViewMode = NewViewMode;
}

void WxPhAT::SetCurrentCollisionViewMode(EPhATRenderMode NewViewMode)
{
	if(bRunningSimulation)
		Sim_CollisionViewMode = NewViewMode;
	else if(EditingMode == PEM_BodyEdit)
		BodyEdit_CollisionViewMode = NewViewMode;
	else
		ConstraintEdit_CollisionViewMode = NewViewMode;
}

void WxPhAT::SetCurrentConstraintViewMode(EPhATConstraintViewMode NewViewMode)
{
	if(bRunningSimulation)
		Sim_ConstraintViewMode = NewViewMode;
	else if(EditingMode == PEM_BodyEdit)
		BodyEdit_ConstraintViewMode = NewViewMode;
	else
		ConstraintEdit_ConstraintViewMode = NewViewMode;
}



void WxPhAT::ToggleViewCOM()
{
	bShowCOM = !bShowCOM;
	ToolBar->ToggleTool( IDMN_PHAT_SHOWCOM, bShowCOM == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::ToggleViewHierarchy()
{
	bShowHierarchy = !bShowHierarchy;
	ToolBar->ToggleTool( IDMN_PHAT_SHOWHIERARCHY, bShowHierarchy == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::ToggleViewContacts()
{
	bShowContacts = !bShowContacts;
	ViewContactsToggle();
	ToolBar->ToggleTool( IDMN_PHAT_SHOWCONTACTS, bShowContacts == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}


void WxPhAT::ToggleViewInfluences()
{
	bShowInfluences = !bShowInfluences;
	ToolBar->ToggleTool( IDMN_PHAT_SHOWINFLUENCES, bShowInfluences == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::ToggleDrawGround()
{
	bDrawGround = !bDrawGround;
	ToolBar->ToggleTool( IDMN_PHAT_DRAWGROUND, bDrawGround == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

void WxPhAT::ToggleShowFixed()
{
	bShowFixedStatus = !bShowFixedStatus;
	ToolBar->ToggleTool( IDMN_PHAT_SHOWFIXED, bShowFixedStatus == TRUE );
	PhATViewportClient->Viewport->Invalidate();
}

static UBOOL ShapeHasNoRBCollision(URB_BodySetup* BS, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
{
	if(PrimitiveType == KPT_Box)
	{
		return BS->AggGeom.BoxElems(PrimitiveIndex).bNoRBCollision;
	}
	else if(PrimitiveType == KPT_Sphere)
	{
		return BS->AggGeom.SphereElems(PrimitiveIndex).bNoRBCollision;
	}
	else if(PrimitiveType == KPT_Sphyl)
	{
		return BS->AggGeom.SphylElems(PrimitiveIndex).bNoRBCollision;
	}
	else
	{
		return FALSE;
	}
}

FColor WxPhAT::GetPrimitiveColor(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
{
	URB_BodySetup* bs = PhysicsAsset->BodySetup( BodyIndex );

	if(!bRunningSimulation && EditingMode == PEM_ConstraintEdit && SelectedConstraintIndex != INDEX_NONE)
	{
		URB_ConstraintSetup* cs = PhysicsAsset->ConstraintSetup( SelectedConstraintIndex );

		if(cs->ConstraintBone1 == bs->BoneName)
		{
			return ConstraintBone1Color;
		}
		else if(cs->ConstraintBone2 == bs->BoneName)
		{
			return ConstraintBone2Color;
		}
	}

	if(bRunningSimulation)
	{
		if(bShowFixedStatus && bs->bFixed)
		{
			return FixedColor;
		}
		else
		{
			return BoneUnselectedColor;
		}
	}

	if(EditingMode == PEM_ConstraintEdit)
	{
		return BoneUnselectedColor;
	}

	if(BodyIndex == SelectedBodyIndex)
	{
		if(PrimitiveType == SelectedPrimitiveType && PrimitiveIndex == SelectedPrimitiveIndex)
		{
			return ElemSelectedColor;
		}
		else
		{
			return BoneSelectedColor;
		}
	}
	else
	{
		if(bShowFixedStatus)
		{
			if(bs->bFixed)
			{
				return FixedColor;
			}
			else
			{
				return BoneUnselectedColor;
			}
		}
		else
		{
			// If there is no collision with this body, use 'no collision material'.
			if( NoCollisionBodies.FindItemIndex(BodyIndex) != INDEX_NONE || ShapeHasNoRBCollision(bs, PrimitiveType, PrimitiveIndex) )
			{
				return NoCollisionColor;
			}
			else
			{
				return BoneUnselectedColor;
			}
		}
	}
}

UMaterialInterface* WxPhAT::GetPrimitiveMaterial(INT BodyIndex, EKCollisionPrimitiveType PrimitiveType, INT PrimitiveIndex)
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
void WxPhAT::DrawCurrentInfluences(FPrimitiveDrawInterface* PDI)
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

			PDI->DrawLine(WPos, WPos + WNorm * InfluenceLineLength, InfluenceLineColor, SDPG_World);
		}
	}
}

// This will update WxPhAT::WidgetTM
void WxPhAT::DrawCurrentWidget(const FSceneView* View, FPrimitiveDrawInterface* PDI, UBOOL bHitTest)
{
	if(EditingMode == PEM_BodyEdit) /// BODY EDITING ///
	{
		// Don't draw widget if nothing selected.
		if(SelectedBodyIndex == INDEX_NONE)
			return;

		INT BoneIndex = EditorSkelComp->MatchRefBone( PhysicsAsset->BodySetup(SelectedBodyIndex)->BoneName );

		FMatrix BoneTM = EditorSkelComp->GetBoneMatrix(BoneIndex);
		BoneTM.RemoveScaling();

		WidgetTM = GetPrimitiveMatrix(BoneTM, SelectedBodyIndex, SelectedPrimitiveType, SelectedPrimitiveIndex, 1.f);
	}
	else  /// CONSTRAINT EDITING ///
	{
		if(SelectedConstraintIndex == INDEX_NONE)
			return;

		WidgetTM = GetConstraintMatrix(SelectedConstraintIndex, 1, 1.f);
	}

	FVector WidgetOrigin = WidgetTM.GetOrigin();

	FLOAT ZoomFactor = Min<FLOAT>(View->ProjectionMatrix.M[0][0], View->ProjectionMatrix.M[1][1]);
	FLOAT WidgetRadius = View->Project(WidgetOrigin).W * (UnrealEd_WidgetSize / ZoomFactor);

	FColor XColor(255, 0, 0);
	FColor YColor(0, 255, 0);
	FColor ZColor(0, 0, 255);

	if(ManipulateAxis == AXIS_X)
		XColor = FColor(255, 255, 0);
	else if(ManipulateAxis == AXIS_Y)
		YColor = FColor(255, 255, 0);
	else if(ManipulateAxis == AXIS_Z)
		ZColor = FColor(255, 255, 0);

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
			PDI->SetHitProxy( new HPhATWidgetProxy(AXIS_X) );
			WidgetMatrix = FMatrix(XAxis, YAxis, ZAxis, WidgetOrigin);
			DrawDirectionalArrow(PDI,WidgetMatrix, XColor, WidgetRadius, 1.f, SDPG_Foreground);
			PDI->SetHitProxy( NULL );

			PDI->SetHitProxy( new HPhATWidgetProxy(AXIS_Y) );
			WidgetMatrix = FMatrix(YAxis, ZAxis, XAxis, WidgetOrigin);
			DrawDirectionalArrow(PDI,WidgetMatrix, YColor, WidgetRadius, 1.f, SDPG_Foreground);
			PDI->SetHitProxy( NULL );

			PDI->SetHitProxy( new HPhATWidgetProxy(AXIS_Z) );
			WidgetMatrix = FMatrix(ZAxis, XAxis, YAxis, WidgetOrigin);
			DrawDirectionalArrow(PDI,WidgetMatrix, ZColor, WidgetRadius, 1.f, SDPG_Foreground);
			PDI->SetHitProxy( NULL );
		}
		else
		{
			WidgetMatrix = FMatrix(XAxis, YAxis, ZAxis, WidgetOrigin);
			DrawDirectionalArrow(PDI,WidgetMatrix, XColor, WidgetRadius, 1.f, SDPG_Foreground);

			WidgetMatrix = FMatrix(YAxis, ZAxis, XAxis, WidgetOrigin);
			DrawDirectionalArrow(PDI,WidgetMatrix, YColor, WidgetRadius, 1.f, SDPG_Foreground);

			WidgetMatrix = FMatrix(ZAxis, XAxis, YAxis, WidgetOrigin);
			DrawDirectionalArrow(PDI,WidgetMatrix, ZColor, WidgetRadius, 1.f, SDPG_Foreground);
		}
	}
	////////////////// CIRCLES WIDGET //////////////////
	else if(MovementMode == PMM_Rotate)
	{
		// ViewMatrix is WorldToCamera, so invert to get CameraToWorld
		FVector LookDir = View->ViewMatrix.Inverse().TransformNormal( FVector(0,0,1) );

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
			PDI->SetHitProxy( new HPhATWidgetProxy(AXIS_X));
			DrawCircle(PDI,WidgetOrigin, YAxis, ZAxis, XColor, WidgetRadius, 24, SDPG_Foreground);
			PDI->SetHitProxy( NULL );

			PDI->SetHitProxy( new HPhATWidgetProxy(AXIS_Y) );
			DrawCircle(PDI,WidgetOrigin, XAxis, ZAxis, YColor, WidgetRadius, 24, SDPG_Foreground);
			PDI->SetHitProxy( NULL );

			PDI->SetHitProxy( new HPhATWidgetProxy(AXIS_Z) );
			DrawCircle(PDI,WidgetOrigin, XAxis, YAxis, ZColor, WidgetRadius, 24, SDPG_Foreground);
			PDI->SetHitProxy( NULL );
		}
		else
		{
			DrawCircle(PDI,WidgetOrigin, YAxis, ZAxis, XColor, WidgetRadius, 24, SDPG_Foreground);
			DrawCircle(PDI,WidgetOrigin, XAxis, ZAxis, YColor, WidgetRadius, 24, SDPG_Foreground);
			DrawCircle(PDI,WidgetOrigin, XAxis, YAxis, ZColor, WidgetRadius, 24, SDPG_Foreground);
		}
	}
}
