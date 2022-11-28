
#include "EditorPrivate.h"
#include "UnTerrain.h"

IMPLEMENT_CLASS(UEdModeComponent);
IMPLEMENT_CLASS(UEditorComponent);

/*------------------------------------------------------------------------------
    UEditorComponent.
------------------------------------------------------------------------------*/

void UEditorComponent::StaticConstructor()
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
}

void UEditorComponent::GetZoneMask(UModel* Model)
{
	ZoneMask = FZoneSet::AllZones();
}

DWORD UEditorComponent::GetLayerMask(const FSceneContext& Context) const
{
	DWORD LayerMask = 0;

	if((Context.View->ShowFlags & SHOW_Grid) && bDrawGrid)
	{
		LayerMask |= PLM_Background;
	}

	if(bDrawPivot)
	{
		LayerMask |= PLM_Foreground;
	}

	return LayerMask;
}

void UEditorComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	Super::RenderForeground(Context, PRI);

	if( !PRI->IsHitTesting() )
	{
		if(bDrawPivot)
		{
			RenderPivot(Context, PRI);
		}
	}
}

void UEditorComponent::RenderBackground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	Super::RenderBackground(Context, PRI);

	if( !PRI->IsHitTesting() )
	{
		if((Context.View->ShowFlags & SHOW_Grid) && bDrawGrid)
		{
			RenderGrid(Context, PRI);
		}
	}
}

void UEditorComponent::Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI)
{
	Super::Render(Context, PRI);

	if( !PRI->IsHitTesting() )
	{
		if(bDrawBaseInfo)
		{
			RenderBaseInfo(Context, PRI);
		}
	}
}

/** Draw green lines to indicate what the selected actor(s) are based on. */
void UEditorComponent::RenderBaseInfo(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	ULevel* Level = GEngine->GetLevel();

	for( TSelectedActorIterator It(Level) ; It ; ++It )
	{
		AActor* Actor = *It;

		// If this actor is attached to something...
		if(Actor->AttachTag != NAME_None)
		{
			for(INT i=0; i<Level->Actors.Num(); i++)
			{
				AActor* BaseActor = Level->Actors(i);
				if( BaseActor && ((BaseActor->Tag == Actor->AttachTag) || (BaseActor->GetFName() == Actor->AttachTag))  )
				{
					// Found the base.
					FBox BaseBox(0);

					if(Actor->BaseBoneName != NAME_None)
					{
						// This logic should be the same as that in ULevel::BeginPlay...
						USkeletalMeshComponent* BaseSkelComp = Actor->BaseSkelComponent;
						if(!BaseSkelComp)
						{									
							BaseSkelComp = Cast<USkeletalMeshComponent>( BaseActor->CollisionComponent );
						}

						// If we found a skeletal mesh component to attach to, look for bone
						if(BaseSkelComp)
						{
							INT BoneIndex = BaseSkelComp->MatchRefBone(Actor->BaseBoneName);
							if(BoneIndex != INDEX_NONE)
							{
								// Found the bone we want to attach to.
								FMatrix BoneTM = BaseSkelComp->GetBoneMatrix(BoneIndex);
								BoneTM.RemoveScaling();

								FVector TotalScale = BaseActor->DrawScale * BaseActor->DrawScale3D;

								// If it has a PhysicsAsset, use that to calc bounding box for bone.
								if(BaseSkelComp->PhysicsAsset)
								{
									INT BoneIndex = BaseSkelComp->PhysicsAsset->FindBodyIndex(Actor->BaseBoneName);
									if(BoneIndex != INDEX_NONE)
									{
										BaseBox = BaseSkelComp->PhysicsAsset->BodySetup(BoneIndex)->AggGeom.CalcAABB(BoneTM, TotalScale.X);
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
						BaseBox = BaseActor->GetComponentsBoundingBox(true);
					}

					// Draw box around base and line between actor origin and its base.
					PRI->DrawWireBox( BaseBox, BaseBoxColor );
					PRI->DrawLine( Actor->Location, BaseBox.GetCenter(), BaseBoxColor );

					break;
				}
			}
		}
	}
}

void UEditorComponent::RenderGrid(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	// Vector defining worldbox lines.
	FVector	Origin = Context.View->ViewMatrix.Inverse().GetOrigin();
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

	// Draw 3D perspective grid
	if( Context.View->ProjectionMatrix.M[3][3] < 1.0f )
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
				PRI->DrawLine(A,B,(i==j)?GridColorHi:GridColorLo);

				A.Y=A.X;							B.Y=B.X;
				A.X=(PerspectiveGridSize/4.f);		B.X=-(PerspectiveGridSize/4.f);
				PRI->DrawLine(A,B,(i==j)?GridColorHi:GridColorLo);
			}
		}
	}
	// Draw ortho grid.
	else
	{
		for( int AlphaCase=0; AlphaCase<=1; AlphaCase++ )
		{
			if( Abs(Context.View->ViewMatrix.M[2][2]) > 0.0f )
			{
				// Do Y-Axis lines.
				A.Y=+HALF_WORLD_MAX1; A.Z=0.0;
				B.Y=-HALF_WORLD_MAX1; B.Z=0.0;
				DrawGridSection( Origin.X, GEditor->Constraints.GetGridSize(), &A, &B, &A.X, &B.X, 0, AlphaCase, Context.View, PRI );

				// Do X-Axis lines.
				A.X=+HALF_WORLD_MAX1; A.Z=0.0;
				B.X=-HALF_WORLD_MAX1; B.Z=0.0;
				DrawGridSection( Origin.Y, GEditor->Constraints.GetGridSize(), &A, &B, &A.Y, &B.Y, 1, AlphaCase, Context.View, PRI );
			}
			else if( Abs(Context.View->ViewMatrix.M[1][2]) > 0.0f )
			{
				// Do Z-Axis lines.
				A.Z=+HALF_WORLD_MAX1; A.Y=0.0;
				B.Z=-HALF_WORLD_MAX1; B.Y=0.0;
				DrawGridSection( Origin.X, GEditor->Constraints.GetGridSize(), &A, &B, &A.X, &B.X, 0, AlphaCase, Context.View, PRI );

				// Do X-Axis lines.
				A.X=+HALF_WORLD_MAX1; A.Y=0.0;
				B.X=-HALF_WORLD_MAX1; B.Y=0.0;
				DrawGridSection( Origin.Z, GEditor->Constraints.GetGridSize(), &A, &B, &A.Z, &B.Z, 2, AlphaCase, Context.View, PRI );
			}
			else if( Abs(Context.View->ViewMatrix.M[0][2]) > 0.0f )
			{
				// Do Z-Axis lines.
				A.Z=+HALF_WORLD_MAX1; A.X=0.0;
				B.Z=-HALF_WORLD_MAX1; B.X=0.0;
				DrawGridSection( Origin.Y, GEditor->Constraints.GetGridSize(), &A, &B, &A.Y, &B.Y, 1, AlphaCase, Context.View, PRI );

				// Do Y-Axis lines.
				A.Y=+HALF_WORLD_MAX1; A.X=0.0;
				B.Y=-HALF_WORLD_MAX1; B.X=0.0;
				DrawGridSection( Origin.Z, GEditor->Constraints.GetGridSize(), &A, &B, &A.Z, &B.Z, 2, AlphaCase, Context.View, PRI );
			}
		}
	}

	if(bDrawColoredOrigin)
	{
		// Draw axis lines.
		A = FVector(0,0,HALF_WORLD_MAX1);B = FVector(0,0,0);
		PRI->DrawLine(A,B,FColor(64,64,255));
		A = FVector(0,0,0);B = FVector(0,0,-HALF_WORLD_MAX1);
		PRI->DrawLine(A,B,FColor(32,32,128));

		A = FVector(0,HALF_WORLD_MAX1,0);B = FVector(0,0,0);
		PRI->DrawLine(A,B,FColor(64,255,64));
		A = FVector(0,0,0);B = FVector(0,-HALF_WORLD_MAX1,0);
		PRI->DrawLine(A,B,FColor(32,128,32));

		A = FVector(HALF_WORLD_MAX1,0,0);B = FVector(0,0,0);
		PRI->DrawLine(A,B,FColor(255,64,64));
		A = FVector(0,0,0);B = FVector(-HALF_WORLD_MAX1,0,0);
		PRI->DrawLine(A,B,FColor(128,32,32));
	}

	// Draw orthogonal worldframe.
	if(bDrawWorldBox)
	{
		PRI->DrawWireBox( FBox( FVector(-HALF_WORLD_MAX1,-HALF_WORLD_MAX1,-HALF_WORLD_MAX1),FVector(HALF_WORLD_MAX1,HALF_WORLD_MAX1,HALF_WORLD_MAX1) ), GEngine->C_WorldBox );
	}
}

void UEditorComponent::DrawGridSection(INT ViewportLocX,INT ViewportGridY,FVector* A,FVector* B,FLOAT* AX,FLOAT* BX,INT Axis,INT AlphaCase,FSceneView* View,FPrimitiveRenderInterface* PRI)
{
	if( !ViewportGridY )
		return;

	FMatrix	InvViewProjMatrix = View->ProjectionMatrix.Inverse() * View->ViewMatrix.Inverse();

	FLOAT	Start = (int)(InvViewProjMatrix.TransformFVector(FVector(-1,-1,0.5f)).Component(Axis) / ViewportGridY);
	FLOAT	End   = (int)(InvViewProjMatrix.TransformFVector(FVector(+1,+1,0.5f)).Component(Axis) / ViewportGridY);

	if(Start > End)
		Exchange(Start,End);

	FLOAT	SizeX = PRI->GetViewport()->GetSizeX(),
			Zoom = (1.0f / View->ProjectionMatrix.M[0][0]) * 2.0f / SizeX;
	INT     Dist  = (int)(SizeX * Zoom / ViewportGridY);

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

	INT iStart  = ::Max<INT>((int)Start - 1,-HALF_WORLD_MAX/ViewportGridY) >> IncBits;
	INT iEnd    = ::Min<INT>((int)End + 1,  +HALF_WORLD_MAX/ViewportGridY) >> IncBits;

	for( INT i=iStart; i<iEnd; i++ )
	{
		*AX = (i * ViewportGridY) << IncBits;
		*BX = (i * ViewportGridY) << IncBits;
		if( (i&1) != AlphaCase )
		{
			FPlane Background = View->BackgroundColor.Plane();
			FPlane Grid       = FPlane(.5,.5,.5,0);
			FPlane Color      = Background + (Grid-Background) * (((i<<IncBits)&7) ? 0.5 : 1.0);
			if( i&1 ) Color = Background + (Color-Background) * Alpha;

			PRI->DrawLine(*A,*B,FColor(Color));
		}
	}
}

void UEditorComponent::RenderPivot(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	FMatrix CameraToWorld = Context.View->ViewMatrix.Inverse();

	FVector PivLoc = GEditorModeTools.SnappedLocation;

	FLOAT ZoomFactor = Min<FLOAT>(Context.View->ProjectionMatrix.M[0][0], Context.View->ProjectionMatrix.M[1][1]);
	FLOAT WidgetRadius = Context.View->Project(PivLoc).W * (PivotSize / ZoomFactor);

	FVector CamX = CameraToWorld.TransformNormal( FVector(1,0,0) );
	FVector CamY = CameraToWorld.TransformNormal( FVector(0,1,0) );


	PRI->DrawLine( PivLoc - (WidgetRadius*CamX), PivLoc + (WidgetRadius*CamX), PivotColor );
	PRI->DrawLine( PivLoc - (WidgetRadius*CamY), PivLoc + (WidgetRadius*CamY), PivotColor );
}


/*------------------------------------------------------------------------------
    UEdModeComponent.
------------------------------------------------------------------------------*/


DWORD UEdModeComponent::GetLayerMask(const FSceneContext& Context) const
{
	return Super::GetLayerMask(Context) | GEditorModeTools.GetCurrentMode()->GetLayerMask(Context);
}

void UEdModeComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	Super::RenderForeground(Context,PRI);

	GEditorModeTools.GetCurrentMode()->RenderForeground(Context,PRI);

	// If there is an active drag tool, let it render

	FDragTool* dt = ((FEditorLevelViewportClient*)(PRI->GetViewport()->ViewportClient))->MouseDeltaTracker.DragTool;
	if( dt )
	{
		dt->RenderForeground(Context,PRI);
	}

	// Draw the viewports widget

	((FEditorLevelViewportClient*)(PRI->GetViewport()->ViewportClient))->Widget->RenderForeground( Context, PRI );
}

void UEdModeComponent::RenderBackground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	Super::RenderBackground(Context,PRI);
	GEditorModeTools.GetCurrentMode()->RenderBackground(Context,PRI);
}

void UEdModeComponent::Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI)
{
	Super::Render(Context,PRI);
	GEditorModeTools.GetCurrentMode()->Render(Context,PRI);
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
