/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "UnTerrain.h"
#include "MouseDeltaTracker.h"
#include "EnginePhysicsClasses.h"

IMPLEMENT_CLASS(UEdModeComponent);
IMPLEMENT_CLASS(UEditorComponent);

/*------------------------------------------------------------------------------
FEditorCommonDrawHelper.
------------------------------------------------------------------------------*/
FEditorCommonDrawHelper::FEditorCommonDrawHelper()
{
	bDrawGrid = true;
	bDrawPivot = true;
	bDrawBaseInfo = true;

	GridColorHi = FColor(0, 0, 127);
	GridColorLo = FColor(0, 0, 63);
	PerspectiveGridSize = HALF_WORLD_MAX1;
	bDrawWorldBox = true;
	bDrawColoredOrigin = false;

	PivotColor = FColor(255,0,0);
	PivotSize = 0.02f;

	BaseBoxColor = FColor(0,255,0);

	DepthPriorityGroup=SDPG_World;
}


void FEditorCommonDrawHelper::Draw(const FSceneView* View,class FPrimitiveDrawInterface* PDI)
{
	if( !PDI->IsHitTesting() )
	{
		if(bDrawBaseInfo)
		{
			DrawBaseInfo(View, PDI);
		}

		// Only draw the pivot if an actor is selected
		if( bDrawPivot && GEditor->GetSelectedActors()->CountSelections<AActor>() > 0 )
		{
			DrawPivot(View, PDI);
		}

		if((View->Family->ShowFlags & SHOW_Grid) && bDrawGrid)
		{
			DrawGrid(View, PDI);
		}
	}
}

/** Draw green lines to indicate what the selected actor(s) are based on. */
void FEditorCommonDrawHelper::DrawBaseInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Early out if we're not suppose to be drawning the base attachment volume for this view
	if (View && View->Family && !View->Family->bDrawBaseInfo)
	{
		return;
	}
	
	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// If this actor is attached to something...
		if ( Actor->Base && !Actor->Base->IsA(AWorldInfo::StaticClass()) )
		{
			FBox BaseBox(0);

			if(Actor->BaseBoneName != NAME_None)
			{
				// This logic should be the same as that in ULevel::BeginPlay...
				USkeletalMeshComponent* BaseSkelComp = Actor->BaseSkelComponent;

				// Ignore BaseSkelComponent if its Owner is not the Base.
				if(BaseSkelComp && BaseSkelComp->GetOwner() != Actor->Base)
				{
					BaseSkelComp = NULL;
				}

				if(!BaseSkelComp)
				{									
					BaseSkelComp = Cast<USkeletalMeshComponent>( Actor->Base->CollisionComponent );
				}

				// If that failed, see if its a pawn, and use its Mesh pointer.
				APawn* Pawn = Cast<APawn>(Actor->Base);
				if(!BaseSkelComp && Pawn)
				{
					BaseSkelComp = Pawn->Mesh;
				}

				// If we found a skeletal mesh component to attach to, look for bone
				if(BaseSkelComp)
				{
					const INT BoneIndex = BaseSkelComp->MatchRefBone(Actor->BaseBoneName);
					if(BoneIndex != INDEX_NONE)
					{
						// Found the bone we want to attach to.
						FMatrix BoneTM = BaseSkelComp->GetBoneMatrix(BoneIndex);
						BoneTM.RemoveScaling();

						const FVector TotalScale = Actor->Base->DrawScale * Actor->Base->DrawScale3D;

						// If it has a PhysicsAsset, use that to calc bounding box for bone.
						if(BaseSkelComp->PhysicsAsset)
						{
							const INT BoneIndex = BaseSkelComp->PhysicsAsset->FindBodyIndex(Actor->BaseBoneName);
							if(BoneIndex != INDEX_NONE)
							{
								BaseBox = BaseSkelComp->PhysicsAsset->BodySetup(BoneIndex)->AggGeom.CalcAABB(BoneTM, TotalScale);
							}
						}

						// Otherwise, just use some smallish box around the bone origin.
						if( !BaseBox.IsValid )
						{
							BaseBox = FBox( BoneTM.GetOrigin() - FVector(10.f), BoneTM.GetOrigin() + FVector(10.f) );
						}
					}
				}				
			}

			// We didn't get a box from bone-base, use the whole actors one.
			if( !BaseBox.IsValid )
			{
				BaseBox = Actor->Base->GetComponentsBoundingBox(true);
			}

			if( !BaseBox.IsValid )
			{
				BaseBox = FBox( Actor->Base->Location, Actor->Base->Location );
			}	

			// Draw box around base and line between actor origin and its base.
			DrawWireBox(PDI, BaseBox, BaseBoxColor, SDPG_World );
			PDI->DrawLine( Actor->Location, BaseBox.GetCenter(), BaseBoxColor, SDPG_World );

			break;
		}
	}
}

void FEditorCommonDrawHelper::DrawGrid(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	ESceneDepthPriorityGroup eDPG = (ESceneDepthPriorityGroup)DepthPriorityGroup;

	// Vector defining worldbox lines.
	FVector	Origin = View->ViewMatrix.Inverse().GetOrigin();
	FVector B1( HALF_WORLD_MAX, HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B2(-HALF_WORLD_MAX, HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B3( HALF_WORLD_MAX,-HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B4(-HALF_WORLD_MAX,-HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B5( HALF_WORLD_MAX, HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B6(-HALF_WORLD_MAX, HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B7( HALF_WORLD_MAX,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B8(-HALF_WORLD_MAX,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector A,B;
	INT i,j;

	UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );
	UBOOL bIsOrthoXY = false;
	UBOOL bIsOrthoXZ = false;
	UBOOL bIsOrthoYZ = false;

	if(!bIsPerspective)
	{
		bIsOrthoXY = ( Abs(View->ViewMatrix.M[2][2]) > 0.0f );
		bIsOrthoXZ = ( Abs(View->ViewMatrix.M[1][2]) > 0.0f );
		bIsOrthoYZ = ( Abs(View->ViewMatrix.M[0][2]) > 0.0f );
	}

	// Draw 3D perspective grid
	if( bIsPerspective)
	{
		// Index of middle line (axis).
		j=(63-1)/2;
		for( i=0; i<63; i++ )
		{
			if( !bDrawColoredOrigin || i != j )
			{
				A.X=(PerspectiveGridSize/4.f)*(-1.0+2.0*i/(63-1));	B.X=A.X;

				A.Y=(PerspectiveGridSize/4.f);		B.Y=-(PerspectiveGridSize/4.f);
				A.Z=0.0;							B.Z=0.0;
				PDI->DrawLine(A,B,(i==j)?GridColorHi:GridColorLo,eDPG);

				A.Y=A.X;							B.Y=B.X;
				A.X=(PerspectiveGridSize/4.f);		B.X=-(PerspectiveGridSize/4.f);
				PDI->DrawLine(A,B,(i==j)?GridColorHi:GridColorLo,eDPG);
			}
		}
	}
	// Draw ortho grid.
	else
	{
		for( int AlphaCase=0; AlphaCase<=1; AlphaCase++ )
		{
			if( bIsOrthoXY )
			{
				// Do Y-Axis lines.
				A.Y=+HALF_WORLD_MAX1; A.Z=0.0;
				B.Y=-HALF_WORLD_MAX1; B.Z=0.0;
				DrawGridSection( Origin.X, GEditor->Constraints.GetGridSize(), &A, &B, &A.X, &B.X, 0, AlphaCase, View, PDI );

				// Do X-Axis lines.
				A.X=+HALF_WORLD_MAX1; A.Z=0.0;
				B.X=-HALF_WORLD_MAX1; B.Z=0.0;
				DrawGridSection( Origin.Y, GEditor->Constraints.GetGridSize(), &A, &B, &A.Y, &B.Y, 1, AlphaCase, View, PDI );
			}
			else if( bIsOrthoXZ )
			{
				// Do Z-Axis lines.
				A.Z=+HALF_WORLD_MAX1; A.Y=0.0;
				B.Z=-HALF_WORLD_MAX1; B.Y=0.0;
				DrawGridSection( Origin.X, GEditor->Constraints.GetGridSize(), &A, &B, &A.X, &B.X, 0, AlphaCase, View, PDI );

				// Do X-Axis lines.
				A.X=+HALF_WORLD_MAX1; A.Y=0.0;
				B.X=-HALF_WORLD_MAX1; B.Y=0.0;
				DrawGridSection( Origin.Z, GEditor->Constraints.GetGridSize(), &A, &B, &A.Z, &B.Z, 2, AlphaCase, View, PDI );
			}
			else if( bIsOrthoYZ )
			{
				// Do Z-Axis lines.
				A.Z=+HALF_WORLD_MAX1; A.X=0.0;
				B.Z=-HALF_WORLD_MAX1; B.X=0.0;
				DrawGridSection( Origin.Y, GEditor->Constraints.GetGridSize(), &A, &B, &A.Y, &B.Y, 1, AlphaCase, View, PDI );

				// Do Y-Axis lines.
				A.Y=+HALF_WORLD_MAX1; A.X=0.0;
				B.Y=-HALF_WORLD_MAX1; B.X=0.0;
				DrawGridSection( Origin.Z, GEditor->Constraints.GetGridSize(), &A, &B, &A.Z, &B.Z, 2, AlphaCase, View, PDI );
			}
		}
	}

	if(bDrawColoredOrigin)
	{
		// Draw axis lines.
		A = FVector(0,0,HALF_WORLD_MAX1);B = FVector(0,0,0);
		PDI->DrawLine(A,B,FColor(64,64,255),eDPG);
		A = FVector(0,0,0);B = FVector(0,0,-HALF_WORLD_MAX1);
		PDI->DrawLine(A,B,FColor(32,32,128),eDPG);

		A = FVector(0,HALF_WORLD_MAX1,0);B = FVector(0,0,0);
		PDI->DrawLine(A,B,FColor(64,255,64),eDPG);
		A = FVector(0,0,0);B = FVector(0,-HALF_WORLD_MAX1,0);
		PDI->DrawLine(A,B,FColor(32,128,32),eDPG);

		A = FVector(HALF_WORLD_MAX1,0,0);B = FVector(0,0,0);
		PDI->DrawLine(A,B,FColor(255,64,64),eDPG);
		A = FVector(0,0,0);B = FVector(-HALF_WORLD_MAX1,0,0);
		PDI->DrawLine(A,B,FColor(128,32,32),eDPG);
	}

	if( bIsOrthoXZ || bIsOrthoYZ)
	{
		if(bDrawKillZ)
		{
			FLOAT KillZ = GWorld->GetWorldInfo()->KillZ;

			PDI->DrawLine( FVector(-HALF_WORLD_MAX,0,KillZ), FVector(HALF_WORLD_MAX,0,KillZ), FColor(255,0,0), SDPG_Foreground );
			PDI->DrawLine( FVector(0,-HALF_WORLD_MAX,KillZ), FVector(0,HALF_WORLD_MAX,KillZ), FColor(255,0,0), SDPG_Foreground );
		}
	}

	// Draw orthogonal worldframe.
	if(bDrawWorldBox)
	{
		DrawWireBox(PDI, FBox( FVector(-HALF_WORLD_MAX1,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1),FVector(HALF_WORLD_MAX1,HALF_WORLD_MAX1,HALF_WORLD_MAX1) ), GEngine->C_WorldBox, eDPG );
	}
}

void FEditorCommonDrawHelper::DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	if( !ViewportGridY )
		return;

	FMatrix	InvViewProjMatrix = View->ProjectionMatrix.Inverse() * View->ViewMatrix.Inverse();

	INT	Start = appTrunc(InvViewProjMatrix.TransformFVector(FVector(-1,-1,0.5f)).Component(Axis) / ViewportGridY);
	INT	End   = appTrunc(InvViewProjMatrix.TransformFVector(FVector(+1,+1,0.5f)).Component(Axis) / ViewportGridY);

	if(Start > End)
		Exchange(Start,End);

	FLOAT	SizeX = View->SizeX,
		Zoom = (1.0f / View->ProjectionMatrix.M[0][0]) * 2.0f / SizeX;
	INT     Dist  = appTrunc(SizeX * Zoom / ViewportGridY);

	// Figure out alpha interpolator for fading in the grid lines.
	FLOAT Alpha;
	INT IncBits=0;
	if( Dist+Dist >= SizeX/4 )
	{
		while( (Dist>>IncBits) >= SizeX/4 )
			IncBits++;
		Alpha = 2 - 2*(FLOAT)Dist / (FLOAT)((1<<IncBits) * SizeX/4);
	}
	else
		Alpha = 1.0;

	INT iStart  = ::Max<INT>(Start - 1,-HALF_WORLD_MAX/ViewportGridY) >> IncBits;
	INT iEnd    = ::Min<INT>(End + 1,  +HALF_WORLD_MAX/ViewportGridY) >> IncBits;

	for( INT i=iStart; i<iEnd; i++ )
	{
		*AX = (i * ViewportGridY) << IncBits;
		*BX = (i * ViewportGridY) << IncBits;
		if( (i&1) != AlphaCase )
		{
			FLinearColor Background = FColor(View->BackgroundColor).ReinterpretAsLinear();
			FLinearColor Grid(.5,.5,.5,0);
			FLinearColor Color  = Background + (Grid-Background) * (((i<<IncBits)&7) ? 0.5 : 1.0);
			if( i&1 ) Color = Background + (Color-Background) * Alpha;

			PDI->DrawLine(*A,*B,FLinearColor(Color.Quantize()),SDPG_UnrealEdBackground);
		}
	}
}

void FEditorCommonDrawHelper::DrawPivot(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	const FMatrix CameraToWorld = View->ViewMatrix.Inverse();

	const FVector PivLoc = GEditorModeTools().SnappedLocation;

	const FLOAT ZoomFactor = Min<FLOAT>(View->ProjectionMatrix.M[0][0], View->ProjectionMatrix.M[1][1]);
	const FLOAT WidgetRadius = View->ProjectionMatrix.TransformFVector(PivLoc).W * (PivotSize / ZoomFactor);

	const FVector CamX = CameraToWorld.TransformNormal( FVector(1,0,0) );
	const FVector CamY = CameraToWorld.TransformNormal( FVector(0,1,0) );


	PDI->DrawLine( PivLoc - (WidgetRadius*CamX), PivLoc + (WidgetRadius*CamX), PivotColor, SDPG_Foreground );
	PDI->DrawLine( PivLoc - (WidgetRadius*CamY), PivLoc + (WidgetRadius*CamY), PivotColor, SDPG_Foreground );
}



/*------------------------------------------------------------------------------
    UEditorComponent.
------------------------------------------------------------------------------*/

void UEditorComponent::StaticConstructor()
{
/* now in defprops
	bDrawGrid = true;
	bDrawPivot = true;
	bDrawBaseInfo = true;

	GridColorHi = FColor(0, 0, 127);
	GridColorLo = FColor(0, 0, 63);
	PerspectiveGridSize = HALF_WORLD_MAX1;
	bDrawWorldBox = true;
	bDrawColoredOrigin = false;

	PivotColor = FColor(255,0,0);
	PivotSize = 0.02f;

	BaseBoxColor = FColor(0,255,0);
*/
}

void UEditorComponent::Draw(const FSceneView* View,class FPrimitiveDrawInterface* PDI)
{
	if( !PDI->IsHitTesting() )
	{
		if(bDrawBaseInfo)
		{
			DrawBaseInfo(View, PDI);
		}

		// Only draw the pivot if an actor is selected
		if( bDrawPivot && GEditor->GetSelectedActors()->CountSelections<AActor>() > 0 )
		{
			DrawPivot(View, PDI);
		}

		if((View->Family->ShowFlags & SHOW_Grid) && bDrawGrid)
		{
			DrawGrid(View, PDI);
		}


	}
}

/** Draw green lines to indicate what the selected actor(s) are based on. */
void UEditorComponent::DrawBaseInfo(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	// Early out if we're not suppose to be drawning the base attachment volume for this view
	if (View && View->Family && !View->Family->bDrawBaseInfo)
	{
		return;
	}

	for ( FSelectionIterator It( GEditor->GetSelectedActorIterator() ) ; It ; ++It )
	{
		AActor* Actor = static_cast<AActor*>( *It );
		checkSlow( Actor->IsA(AActor::StaticClass()) );

		// If this actor is attached to something...
		if ( Actor->Base && !Actor->Base->IsA(AWorldInfo::StaticClass()) )
		{
			FBox BaseBox(0);

			if(Actor->BaseBoneName != NAME_None)
			{
				// This logic should be the same as that in ULevel::BeginPlay...
				USkeletalMeshComponent* BaseSkelComp = Actor->BaseSkelComponent;

				// Ignore BaseSkelComponent if its Owner is not the Base.
				if(BaseSkelComp && BaseSkelComp->GetOwner() != Actor->Base)
				{
					BaseSkelComp = NULL;
				}

				if(!BaseSkelComp)
				{									
					BaseSkelComp = Cast<USkeletalMeshComponent>( Actor->Base->CollisionComponent );
				}

				// If that failed, see if its a pawn, and use its Mesh pointer.
				APawn* Pawn = Cast<APawn>(Actor->Base);
				if(!BaseSkelComp && Pawn)
				{
					BaseSkelComp = Pawn->Mesh;
				}

				// If we found a skeletal mesh component to attach to, look for bone
				if(BaseSkelComp)
				{
					const INT BoneIndex = BaseSkelComp->MatchRefBone(Actor->BaseBoneName);
					if(BoneIndex != INDEX_NONE)
					{
						// Found the bone we want to attach to.
						FMatrix BoneTM = BaseSkelComp->GetBoneMatrix(BoneIndex);
						BoneTM.RemoveScaling();

						const FVector TotalScale = Actor->Base->DrawScale * Actor->Base->DrawScale3D;

						// If it has a PhysicsAsset, use that to calc bounding box for bone.
						if(BaseSkelComp->PhysicsAsset)
						{
							const INT BoneIndex = BaseSkelComp->PhysicsAsset->FindBodyIndex(Actor->BaseBoneName);
							if(BoneIndex != INDEX_NONE)
							{
								BaseBox = BaseSkelComp->PhysicsAsset->BodySetup(BoneIndex)->AggGeom.CalcAABB(BoneTM, TotalScale);
							}
						}

						// Otherwise, just use some smallish box around the bone origin.
						if( !BaseBox.IsValid )
						{
							BaseBox = FBox( BoneTM.GetOrigin() - FVector(10.f), BoneTM.GetOrigin() + FVector(10.f) );
						}
					}
				}				
			}

			// We didn't get a box from bone-base, use the whole actors one.
			if( !BaseBox.IsValid )
			{
				BaseBox = Actor->Base->GetComponentsBoundingBox(true);
			}

			if( !BaseBox.IsValid )
			{
				BaseBox = FBox( Actor->Base->Location, Actor->Base->Location );
			}	

			// Draw box around base and line between actor origin and its base.
			DrawWireBox(PDI, BaseBox, BaseBoxColor, SDPG_World );
			PDI->DrawLine( Actor->Location, BaseBox.GetCenter(), BaseBoxColor, SDPG_World );

			break;
		}
	}
}

void UEditorComponent::DrawGrid(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	ESceneDepthPriorityGroup eDPG = (ESceneDepthPriorityGroup)DepthPriorityGroup;

	// Vector defining worldbox lines.
	FVector	Origin = View->ViewMatrix.Inverse().GetOrigin();
	FVector B1( HALF_WORLD_MAX, HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B2(-HALF_WORLD_MAX, HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B3( HALF_WORLD_MAX,-HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B4(-HALF_WORLD_MAX,-HALF_WORLD_MAX1, HALF_WORLD_MAX1);
	FVector B5( HALF_WORLD_MAX, HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B6(-HALF_WORLD_MAX, HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B7( HALF_WORLD_MAX,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector B8(-HALF_WORLD_MAX,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1);
	FVector A,B;
	INT i,j;

	UBOOL bIsPerspective = ( View->ProjectionMatrix.M[3][3] < 1.0f );
	UBOOL bIsOrthoXY = false;
	UBOOL bIsOrthoXZ = false;
	UBOOL bIsOrthoYZ = false;

	if(!bIsPerspective)
	{
		bIsOrthoXY = ( Abs(View->ViewMatrix.M[2][2]) > 0.0f );
		bIsOrthoXZ = ( Abs(View->ViewMatrix.M[1][2]) > 0.0f );
		bIsOrthoYZ = ( Abs(View->ViewMatrix.M[0][2]) > 0.0f );
	}

	// Draw 3D perspective grid
	if( bIsPerspective)
	{
		// Index of middle line (axis).
		j=(63-1)/2;
		for( i=0; i<63; i++ )
		{
			if( !bDrawColoredOrigin || i != j )
			{
				A.X=(PerspectiveGridSize/4.f)*(-1.0+2.0*i/(63-1));	B.X=A.X;

				A.Y=(PerspectiveGridSize/4.f);		B.Y=-(PerspectiveGridSize/4.f);
				A.Z=0.0;							B.Z=0.0;
				PDI->DrawLine(A,B,(i==j)?GridColorHi:GridColorLo,eDPG);

				A.Y=A.X;							B.Y=B.X;
				A.X=(PerspectiveGridSize/4.f);		B.X=-(PerspectiveGridSize/4.f);
				PDI->DrawLine(A,B,(i==j)?GridColorHi:GridColorLo,eDPG);
			}
		}
	}
	// Draw ortho grid.
	else
	{
		for( int AlphaCase=0; AlphaCase<=1; AlphaCase++ )
		{
			if( bIsOrthoXY )
			{
				// Do Y-Axis lines.
				A.Y=+HALF_WORLD_MAX1; A.Z=0.0;
				B.Y=-HALF_WORLD_MAX1; B.Z=0.0;
				DrawGridSection( Origin.X, GEditor->Constraints.GetGridSize(), &A, &B, &A.X, &B.X, 0, AlphaCase, View, PDI );

				// Do X-Axis lines.
				A.X=+HALF_WORLD_MAX1; A.Z=0.0;
				B.X=-HALF_WORLD_MAX1; B.Z=0.0;
				DrawGridSection( Origin.Y, GEditor->Constraints.GetGridSize(), &A, &B, &A.Y, &B.Y, 1, AlphaCase, View, PDI );
			}
			else if( bIsOrthoXZ )
			{
				// Do Z-Axis lines.
				A.Z=+HALF_WORLD_MAX1; A.Y=0.0;
				B.Z=-HALF_WORLD_MAX1; B.Y=0.0;
				DrawGridSection( Origin.X, GEditor->Constraints.GetGridSize(), &A, &B, &A.X, &B.X, 0, AlphaCase, View, PDI );

				// Do X-Axis lines.
				A.X=+HALF_WORLD_MAX1; A.Y=0.0;
				B.X=-HALF_WORLD_MAX1; B.Y=0.0;
				DrawGridSection( Origin.Z, GEditor->Constraints.GetGridSize(), &A, &B, &A.Z, &B.Z, 2, AlphaCase, View, PDI );
			}
			else if( bIsOrthoYZ )
			{
				// Do Z-Axis lines.
				A.Z=+HALF_WORLD_MAX1; A.X=0.0;
				B.Z=-HALF_WORLD_MAX1; B.X=0.0;
				DrawGridSection( Origin.Y, GEditor->Constraints.GetGridSize(), &A, &B, &A.Y, &B.Y, 1, AlphaCase, View, PDI );

				// Do Y-Axis lines.
				A.Y=+HALF_WORLD_MAX1; A.X=0.0;
				B.Y=-HALF_WORLD_MAX1; B.X=0.0;
				DrawGridSection( Origin.Z, GEditor->Constraints.GetGridSize(), &A, &B, &A.Z, &B.Z, 2, AlphaCase, View, PDI );
			}
		}
	}

	if(bDrawColoredOrigin)
	{
		// Draw axis lines.
		A = FVector(0,0,HALF_WORLD_MAX1);B = FVector(0,0,0);
		PDI->DrawLine(A,B,FColor(64,64,255),eDPG);
		A = FVector(0,0,0);B = FVector(0,0,-HALF_WORLD_MAX1);
		PDI->DrawLine(A,B,FColor(32,32,128),eDPG);

		A = FVector(0,HALF_WORLD_MAX1,0);B = FVector(0,0,0);
		PDI->DrawLine(A,B,FColor(64,255,64),eDPG);
		A = FVector(0,0,0);B = FVector(0,-HALF_WORLD_MAX1,0);
		PDI->DrawLine(A,B,FColor(32,128,32),eDPG);

		A = FVector(HALF_WORLD_MAX1,0,0);B = FVector(0,0,0);
		PDI->DrawLine(A,B,FColor(255,64,64),eDPG);
		A = FVector(0,0,0);B = FVector(-HALF_WORLD_MAX1,0,0);
		PDI->DrawLine(A,B,FColor(128,32,32),eDPG);
	}

	if( bIsOrthoXZ || bIsOrthoYZ)
	{
		if(bDrawKillZ)
		{
			FLOAT KillZ = GWorld->GetWorldInfo()->KillZ;

			PDI->DrawLine( FVector(-HALF_WORLD_MAX,0,KillZ), FVector(HALF_WORLD_MAX,0,KillZ), FColor(255,0,0), SDPG_Foreground );
			PDI->DrawLine( FVector(0,-HALF_WORLD_MAX,KillZ), FVector(0,HALF_WORLD_MAX,KillZ), FColor(255,0,0), SDPG_Foreground );
		}
	}

	// Draw orthogonal worldframe.
	if(bDrawWorldBox)
	{
		DrawWireBox(PDI, FBox( FVector(-HALF_WORLD_MAX1,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1),FVector(HALF_WORLD_MAX1,HALF_WORLD_MAX1,HALF_WORLD_MAX1) ), GEngine->C_WorldBox, eDPG );
	}
}

void UEditorComponent::DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	if( !ViewportGridY )
		return;

	FMatrix	InvViewProjMatrix = View->ProjectionMatrix.Inverse() * View->ViewMatrix.Inverse();

	INT	Start = appTrunc(InvViewProjMatrix.TransformFVector(FVector(-1,-1,0.5f)).Component(Axis) / ViewportGridY);
	INT	End   = appTrunc(InvViewProjMatrix.TransformFVector(FVector(+1,+1,0.5f)).Component(Axis) / ViewportGridY);

	if(Start > End)
		Exchange(Start,End);

	FLOAT	SizeX = View->SizeX,
			Zoom = (1.0f / View->ProjectionMatrix.M[0][0]) * 2.0f / SizeX;
	INT     Dist  = appTrunc(SizeX * Zoom / ViewportGridY);

	// Figure out alpha interpolator for fading in the grid lines.
	FLOAT Alpha;
	INT IncBits=0;
	if( Dist+Dist >= SizeX/4 )
	{
		while( (Dist>>IncBits) >= SizeX/4 )
			IncBits++;
		Alpha = 2 - 2*(FLOAT)Dist / (FLOAT)((1<<IncBits) * SizeX/4);
	}
	else
		Alpha = 1.0;

	INT iStart  = ::Max<INT>(Start - 1,-HALF_WORLD_MAX/ViewportGridY) >> IncBits;
	INT iEnd    = ::Min<INT>(End + 1,  +HALF_WORLD_MAX/ViewportGridY) >> IncBits;

	for( INT i=iStart; i<iEnd; i++ )
	{
		*AX = (i * ViewportGridY) << IncBits;
		*BX = (i * ViewportGridY) << IncBits;
		if( (i&1) != AlphaCase )
		{
			FLinearColor Background = FColor(View->BackgroundColor).ReinterpretAsLinear();
			FLinearColor Grid(.5,.5,.5,0);
			FLinearColor Color  = Background + (Grid-Background) * (((i<<IncBits)&7) ? 0.5 : 1.0);
			if( i&1 ) Color = Background + (Color-Background) * Alpha;

			PDI->DrawLine(*A,*B,FLinearColor(Color.Quantize()),SDPG_UnrealEdBackground);
		}
	}
}

void UEditorComponent::DrawPivot(const FSceneView* View,FPrimitiveDrawInterface* PDI)
{
	const FMatrix CameraToWorld = View->ViewMatrix.Inverse();

	const FVector PivLoc = GEditorModeTools().SnappedLocation;

	const FLOAT ZoomFactor = Min<FLOAT>(View->ProjectionMatrix.M[0][0], View->ProjectionMatrix.M[1][1]);
	const FLOAT WidgetRadius = View->Project(PivLoc).W * (PivotSize / ZoomFactor);

	const FVector CamX = CameraToWorld.TransformNormal( FVector(1,0,0) );
	const FVector CamY = CameraToWorld.TransformNormal( FVector(0,1,0) );


	PDI->DrawLine( PivLoc - (WidgetRadius*CamX), PivLoc + (WidgetRadius*CamX), PivotColor, SDPG_Foreground );
	PDI->DrawLine( PivLoc - (WidgetRadius*CamY), PivLoc + (WidgetRadius*CamY), PivotColor, SDPG_Foreground );
}


/*------------------------------------------------------------------------------
    UEdModeComponent.
------------------------------------------------------------------------------*/

void UEdModeComponent::Draw(const FSceneView* View,class FPrimitiveDrawInterface* PDI)
{
	Super::Draw(View,PDI);
}

