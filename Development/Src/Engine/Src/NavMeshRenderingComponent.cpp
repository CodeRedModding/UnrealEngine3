/*=============================================================================
	NavMeshRenderingComponent.cpp: A component that renders a nav mesh.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"
#include "DebugRenderSceneProxy.h"

void FNavMeshEdgeBase::DrawEdge( ULineBatchComponent* LineBatcher, FColor C, FVector DrawOffset )
{
	if( NavMesh == NULL )
		return;

	FVector Vert1Loc = NavMesh->GetVertLocation(Vert0);
	FVector Vert2Loc = NavMesh->GetVertLocation(Vert1);

	LineBatcher->DrawLine( Vert1Loc+DrawOffset, Vert2Loc+DrawOffset, C, SDPG_Foreground );
}

void FNavMeshEdgeBase::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset )
{
	if( NavMesh == NULL )
		return;

	if( !IsValid() ) 
	{
		return;
	}

	DrawOffset.Z += EffectiveEdgeLength/3.f;
	FVector Vert1Loc = NavMesh->GetVertLocation(Vert0);
	FVector Vert2Loc = NavMesh->GetVertLocation(Vert1);
	new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Vert1Loc+DrawOffset,Vert2Loc+DrawOffset,C);

	new(DRSP->Stars) FDebugRenderSceneProxy::FWireStar(GetEdgeCenter()+DrawOffset,FColor(C.R,C.G,C.B+50),2.0f);

	if(NavMesh->GetPylon() && NavMesh->GetPylon()->bDrawEdgePolys)
	{
		if(GetPoly0() != NULL) new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset,GetPoly0()->GetPolyCenter()+DrawOffset,C);
		if(GetPoly1() != NULL) new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset,GetPoly1()->GetPolyCenter()+DrawOffset,C);
	}


	//new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset,GetEdgeCenter()+DrawOffset+GetEdgePerpDir()*50.f,C);
}

void FNavMeshBasicOneWayEdge::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset )
{
	FNavMeshEdgeBase::DrawEdge(DRSP,C,DrawOffset);

	if( !IsValid() )
	{
		return;
	}

	// draw an arrow indicating direction as well
	FVector EdgePerp = GetEdgePerpDir();
	const FVector EdgeCtr = GetEdgeCenter();
	if ( (EdgePerp | (GetPoly1()->GetPolyCenter() - EdgeCtr).SafeNormal()) < 0.f )
	{
		EdgePerp *= -1.0f;
	}

	new(DRSP->ArrowLines) FDebugRenderSceneProxy::FArrowLine(EdgeCtr,EdgeCtr + EdgePerp * 15.0f,C); 
}

void FNavMeshBasicOneWayEdge::DrawEdge( ULineBatchComponent* LineBatcher, FColor C, FVector DrawOffset )
{
	if( !IsValid() )
	{
		return;
	}

	FNavMeshEdgeBase::DrawEdge(LineBatcher,C,DrawOffset);

	// draw an arrow indicating direction as well
	FVector EdgePerp = GetEdgePerpDir();
	const FVector EdgeCtr = GetEdgeCenter();
	if ( (EdgePerp | (GetPoly1()->GetPolyCenter() - EdgeCtr).SafeNormal()) < 0.f )
	{
		EdgePerp *= -1.0f;
	}

	LineBatcher->DrawLine(EdgeCtr,EdgeCtr + EdgePerp * 15.0f,C, SDPG_Foreground);
	LineBatcher->DrawLine(EdgeCtr + EdgePerp * 15.0f + FVector(0.f,0.f,5.f),EdgeCtr + EdgePerp * 15.0f,C, SDPG_Foreground);
}


void FNavMeshCrossPylonEdge::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset )
{
 	// if we've been superceded by a submesh edge, hide ourselves!, otherwise always draw even when invalid (we can tell tehre is a dangling CP edge present)
	if( !IsValid(FALSE) && IsValid(TRUE))
	{
		return;
	}

	DrawOffset.Z += EffectiveEdgeLength/3.f;

	UBOOL bLoaded=TRUE;
	if( !Poly0Ref || !Poly1Ref )
	{
		bLoaded=FALSE;
		C = FColor(128,128,128);
		const FVector Norm = GetEdgeNormal();
		const FVector ThisEdgeDir = (GetVertLocation(0) - GetVertLocation(1)).SafeNormal();

		FVector CrossOffset = ThisEdgeDir*10;
		CrossOffset.Z += 10;
		new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset+CrossOffset,GetEdgeCenter()+DrawOffset-CrossOffset,FColor(255,0,0));
		CrossOffset.Z -= 20;
		new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset+CrossOffset,GetEdgeCenter()+DrawOffset-CrossOffset,FColor(255,0,0));
	}
 
	FColor TC = C;
	TC.B = PointerHash(NavMesh);
	FVector Vert1 = GetVertLocation(0);
 	FVector Vert2 = GetVertLocation(1);
	new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(Vert1+DrawOffset/*+VRand()*/,Vert2+DrawOffset/*+VRand()*/,TC,15.0f);
 
 	new(DRSP->Stars) FDebugRenderSceneProxy::FWireStar(GetEdgeCenter()+DrawOffset,FColor(C.R,C.G,C.B+50),2.0f);

	if(bLoaded && NavMesh->GetPylon() && NavMesh->GetPylon()->bDrawEdgePolys)
	{
		if(GetPoly0() != NULL) new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset,GetPoly0()->GetPolyCenter()+DrawOffset,C);
		if(GetPoly1() != NULL) new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset,GetPoly1()->GetPolyCenter()+DrawOffset,C);
	}
}

void FNavMeshSpecialMoveEdge::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset )
{
	if( NavMesh == NULL )
		return;

	if( !IsValid() )
	{
		return;
	}

	if( !RelActor )
	{
		C = FColor(128,128,128);
		const FVector Norm = GetEdgeNormal();
		const FVector ThisEdgeDir = (GetVertLocation(0) - GetVertLocation(1)).SafeNormal();

		FVector CrossOffset = ThisEdgeDir*10;
		CrossOffset.Z += 10;
		new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset+CrossOffset,GetEdgeCenter()+DrawOffset-CrossOffset,FColor(255,0,0));
		CrossOffset.Z -= 20;
		new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(GetEdgeCenter()+DrawOffset+CrossOffset,GetEdgeCenter()+DrawOffset-CrossOffset,FColor(255,0,0));
	}

	FNavMeshEdgeBase::DrawEdge( DRSP, C, DrawOffset );

	new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(GetEdgeCenter()+DrawOffset+VRand(),*MoveDest,C,15.0f);
}

void FNavMeshPathObjectEdge::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset )
{
	if( NavMesh == NULL )
		return;

	if( !IsValid() )
	{
		return;
	}

	IInterface_NavMeshPathObject* PO = (*PathObject!=NULL) ? InterfaceCast<IInterface_NavMeshPathObject>(*PathObject) : NULL;
	if(PO!=NULL && PO->DrawEdge(DRSP,C,DrawOffset,this))
	{
		// if the PO is doing all the drawing, don't do any of the rest
		return;
	}

	FNavMeshEdgeBase::DrawEdge( DRSP, FColor(255,128,0), DrawOffset+VRand() );

	// line from center to the PO we're associated with 
	if(*PathObject != NULL)
	{
		new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(PathObject->Location,GetEdgeCenter(),FColor(255,128,0),25.0f);
	}
}


FColor FNavMeshEdgeBase::GetEdgeColor()
{ 
	AScout* ScoutDefaultObject = AScout::GetGameSpecificDefaultScoutObject();
	if( ScoutDefaultObject != NULL )
	{
		for(INT Idx=0;Idx<ScoutDefaultObject->PathSizes.Num()&&Idx<ScoutDefaultObject->EdgePathColors.Num();++Idx)
		{
			if( appIsNearlyEqual(EffectiveEdgeLength,ScoutDefaultObject->PathSizes(Idx).Radius,(FLOAT)KINDA_SMALL_NUMBER))
			{
				return ScoutDefaultObject->EdgePathColors(Idx);
			}
		}
	}
	return FColor(128,0,255); 
}
FColor FNavMeshCrossPylonEdge::GetEdgeColor()
{ 
	//return FColor(128,255,255); 
	return FNavMeshEdgeBase::GetEdgeColor();
}
FColor FNavMeshSpecialMoveEdge::GetEdgeColor()
{ 
	return FColor(255,255,0); 
}
FColor FNavMeshDropDownEdge::GetEdgeColor()
{
	return FColor(0,128,255);
}
FColor FNavMeshBasicOneWayEdge::GetEdgeColor()
{ 
	return FColor(255,0,220); 
}

void FNavMeshDropDownEdge::DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset/*=FVector(0,0,0)*/ )
{
	FNavMeshCrossPylonEdge::DrawEdge(DRSP,C,DrawOffset);

	// if we've been superceded by a submesh edge, hide ourselves!, otherwise always draw even when invalid (we can tell tehre is a dangling CP edge present)
	if( !IsValid(FALSE) && IsValid(TRUE))
	{
		return;
	}
	// draw an arrow
	FNavMeshPolyBase* this_poly0=GetPoly0();
	FNavMeshPolyBase* this_poly1=GetPoly1();

	if( this_poly0==NULL || this_poly1 ==NULL )
	{
		return;
	}

	const FVector EdgeCtr = GetEdgeCenter()+DrawOffset;
	FVector Dir = (this_poly1->GetPolyCenter()-EdgeCtr);
	Dir.Z = 0.f;
	Dir = Dir.SafeNormal();

	const FLOAT StepSize = AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_StepSize;
	const FVector MidPt = EdgeCtr+Dir*StepSize;
	const FVector EndPt = MidPt-FVector(0.f,0.f,StepSize);

	new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(MidPt, EdgeCtr,C);

	new(DRSP->ArrowLines) FDebugRenderSceneProxy::FArrowLine(MidPt,EndPt,C); 
}

FColor FNavMeshPathObjectEdge::GetEdgeColor()
{
	return FColor(255,128,0);
}

void FNavMeshPolyBase::DrawPoly( ULineBatchComponent* LineBatcher, FColor C, FVector DrawOffset )
{
	if( NavMesh == NULL )
		return;

	if(NumObstaclesAffectingThisPoly > 0)
	{
		UNavigationMeshBase* SubMesh = GetSubMesh();
		if(SubMesh != NULL)
		{
			for(INT SubIdx=0;SubIdx<SubMesh->Polys.Num();++SubIdx)
			{
				SubMesh->Polys(SubIdx).DrawPoly(LineBatcher,C,DrawOffset);
			}
		}
	}
	else
	{
		for( INT VertIdx = 0; VertIdx < PolyVerts.Num(); VertIdx++ )
		{
			FVector Vert = NavMesh->GetVertLocation(PolyVerts(VertIdx));


			if(VertIdx==0)
			{
				FVector VertDrawLoc = Vert+DrawOffset;
				LineBatcher->DrawLine(VertDrawLoc,Vert+FVector(0.f,0.f,10.f)+DrawOffset,FColor(255,0,255),SDPG_Foreground);
				// draw a line to center of poly so we can tell which poly this is vert 0 for
				LineBatcher->DrawLine(VertDrawLoc,VertDrawLoc+((CalcCenter(WORLD_SPACE)+DrawOffset) - VertDrawLoc).SafeNormal() * 5.0f,FColor(255,0,255),SDPG_Foreground);

			}

			if(VertIdx==1)
			{
				FVector VertDrawLoc = Vert+DrawOffset;
				LineBatcher->DrawLine(VertDrawLoc,Vert+FVector(0.f,0.f,10.f)+DrawOffset,FColor(255,255,255),SDPG_Foreground);
				// draw a line to center of poly so we can tell which poly this is vert 0 for
				LineBatcher->DrawLine(VertDrawLoc,VertDrawLoc+((CalcCenter(WORLD_SPACE)+DrawOffset) - VertDrawLoc).SafeNormal() * 2.0f,FColor(255,255,255),SDPG_Foreground);

			}
			WORD NextVertIdx = VertIdx+1;
			if(NextVertIdx >= (WORD)PolyVerts.Num())
			{
				NextVertIdx=0;
			}
			FVector NextVert = NavMesh->GetVertLocation(PolyVerts(NextVertIdx));

			LineBatcher->DrawLine( Vert+DrawOffset, NextVert+DrawOffset, C, SDPG_Foreground );
		}

		LineBatcher->DrawLine(GetPolyCenter()+DrawOffset,GetPolyCenter()+GetPolyNormal()*20.f+DrawOffset,C,SDPG_Foreground);
	}
}

void FNavMeshPolyBase::DrawPoly( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset )
{
	if( NavMesh == NULL )
		return;

	if(NumObstaclesAffectingThisPoly > 0)
	{
		UNavigationMeshBase* SubMesh = GetSubMesh();
		if(SubMesh != NULL)
		{
			for(INT SubIdx=0;SubIdx<SubMesh->Polys.Num();++SubIdx)
			{
				SubMesh->Polys(SubIdx).DrawPoly(DRSP,C,DrawOffset);
			}
		}
	}
	else
	{
		FVector Center = GetPolyCenter();
		FLOAT PolyHeight = GetPolyHeight();
		FVector HeightBoundsOffset = NavMesh->GetPylon()->Up(this) * PolyHeight;
		for( INT VertIdx = 0; VertIdx < PolyVerts.Num(); VertIdx++ )
		{
			FVector Vert = NavMesh->GetVertLocation(PolyVerts(VertIdx));
			// 		if(VertIdx==0)
			// 		{
			// 			new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Vert,Vert+FVector(0.f,0.f,10.f),FColor(255,0,255));
			// 			// draw a line to center of poly so we can tell which poly this is vert 0 for
			// 			new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Vert,Vert+((Center- Vert).SafeNormal() * 5.0f),FColor(255,0,255));
			// 		}
			// 
			// 
			// 		if(VertIdx==1)
			// 		{
			// 			// draw a line to center of poly so we can tell which poly this is vert 0 for
			// 			new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Vert,Vert+((Center- Vert).SafeNormal() * 2.0f),FColor(255,255,255));
			// 		}
			WORD NextVertIdx = (VertIdx+1)%(WORD)PolyVerts.Num();
			FVector NextVert = NavMesh->GetVertLocation(PolyVerts(NextVertIdx));

			//new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Vert,Vert+VRand() * 10.0f,FColor::MakeRandomColor());

			// if this is obstacle geo, and we have a cross pylon edge draw dashes, and draw grey
			if( ( NavMesh->IsObstacleMesh() || NavMesh->IsDynamicObstacleMesh()) && PolyEdges.Num() > 0)
			{
				C = FColor(200,200,200);
				new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(Vert+DrawOffset, NextVert+DrawOffset, C, 16.f );
			}
			else
			{
				new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine( Vert+DrawOffset, NextVert+DrawOffset, C );

				APylon* Py = NavMesh->GetPylon();
				FLOAT MaxHeight = (Py != NULL && Py->MaxPolyHeight_Optional > 0.f) ? Py->MaxPolyHeight_Optional : AScout::GetGameSpecificDefaultScoutObject()->NavMeshGen_MaxPolyHeight;
				if(!( NavMesh->IsObstacleMesh() || NavMesh->IsDynamicObstacleMesh()) && PolyHeight < MaxHeight) 
				{
					new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine( Vert+HeightBoundsOffset, NextVert+HeightBoundsOffset, FColor(255,0,255) );

				}
			}
		}

		if(GetPolyNormal().IsNearlyZero())
		{
			new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Center+DrawOffset,Center+FVector(0.f,0.f,1.f)*250.f+DrawOffset,C);
		}

		//new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Center,Center+CalcNormal()*20.f+DrawOffset,FColor(255,255,255));

		new(DRSP->Lines) FDebugRenderSceneProxy::FDebugLine(Center,Center+GetPolyNormal()*20.f+DrawOffset,C);

		for( INT CoverIdx = 0; CoverIdx < PolyCover.Num(); CoverIdx++ )
		{
			ACoverLink* Link = Cast<ACoverLink>(PolyCover(CoverIdx).Actor);
			INT SlotIdx = PolyCover(CoverIdx).SlotIdx;
			if( Link == NULL )
				continue;

			new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(Center+DrawOffset, Link->GetSlotLocation(SlotIdx), FColor(0,255,255), 16.f );
		}

		if( NavMesh->IsSubMesh() )
		{
			// draw line back to parent 
			UNavigationMeshBase* ParentMesh = NavMesh->GetTopLevelMesh();
			WORD* ParentID = ParentMesh->SubMeshToParentPolyMap.Find(NavMesh);

			FNavMeshPolyBase* ParentPoly = ParentMesh->GetPolyFromId(*ParentID);

			new(DRSP->DashedLines) FDebugRenderSceneProxy::FDashedLine(Center+DrawOffset, ParentPoly->GetPolyCenter()+DrawOffset, FColor(200,200,200), 16.f );

		}
	}


}	

void UNavigationMeshBase::DrawMesh(FDebugRenderSceneProxy* DRSP, APylon* Pylon)
{
	for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
	{
		FColor Color = (Pylon->bDisabled) ? FColor(128,128,128) : FColor::MakeRedToGreenColorFromScalar((FLOAT)PolyIdx/float(Polys.Num()));
		Color.B = PointerHash(Pylon);
		Polys(PolyIdx).DrawPoly( DRSP, Color );
	}

	for( PolyList::TIterator It(BuildPolys.GetHead());It;++It)
	{
		FNavMeshPolyBase* Poly = *It;
		FColor Color = (Pylon->bDisabled) ? FColor(128,128,128) : FColor::MakeRedToGreenColorFromScalar((FLOAT)Poly->Item/float(BuildPolys.Num()));
		Color.B = PointerHash(Pylon);

		Poly->DrawPoly(DRSP,Color);
	}

	if(GetPylon() != NULL && !(IsObstacleMesh()||IsDynamicObstacleMesh()))
	{
		for (INT EdgeIdx=0;EdgeIdx<GetNumEdges();EdgeIdx++)
		{
			FNavMeshEdgeBase* Edge = GetEdgeAtIdx(EdgeIdx);
			Edge->DrawEdge(DRSP,Edge->GetEdgeColor(),FVector(0.f,0.f,5.f));
		}

		// dynamic edges
		for( DynamicEdgeList::TIterator Itt(DynamicEdges);Itt;++Itt)
		{
			FNavMeshCrossPylonEdge* Edge = Itt.Value();
			FColor C = Edge->GetEdgeColor();
			Edge->DrawEdge(DRSP,C,FVector(0.f,0.f,6.f));
		}
	}
	
	// draw edges for dynamic sub-meshes
	for(PolyObstacleInfoList::TIterator It(PolyObstacleInfoMap);It;++It)
	{
		FPolyObstacleInfo* Info = &It.Value();

		if(Info->SubMesh != NULL)
		{
			for (INT EdgeIdx=0;EdgeIdx<Info->SubMesh->GetNumEdges();EdgeIdx++)
			{
				FNavMeshEdgeBase* Edge = Info->SubMesh->GetEdgeAtIdx(EdgeIdx);
				Edge->DrawEdge(DRSP,Edge->GetEdgeColor(),FVector(0.f,0.f,5.f));
			}

			// dynamic edges
			for( DynamicEdgeList::TIterator Itt(Info->SubMesh->DynamicEdges);Itt;++Itt)
			{
				FNavMeshCrossPylonEdge* Edge = Itt.Value();
				FColor C = Edge->GetEdgeColor();
				Edge->DrawEdge(DRSP,C,FVector(0.f,0.f,6.f));
			}
		}
	}
}

void UNavigationMeshBase::DrawMesh(FDebugRenderSceneProxy* DRSP, APylon* Pylon, FColor C)
{
	for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
	{
		Polys(PolyIdx).DrawPoly( DRSP, C );
	}

	for( PolyList::TIterator It(BuildPolys.GetHead());It;++It)
	{
		FNavMeshPolyBase* Poly = *It;
		Poly->DrawPoly(DRSP,C);
	}

}


void FNavMeshPolyBase::DrawSolidPoly(FDynamicMeshBuilder& MeshBuilder)
{
	if(NumObstaclesAffectingThisPoly > 0)
	{
		UNavigationMeshBase* SubMesh = GetSubMesh();
		if(SubMesh != NULL)
		{
			for(INT SubIdx=0;SubIdx<SubMesh->Polys.Num();++SubIdx)
			{
				SubMesh->Polys(SubIdx).DrawSolidPoly(MeshBuilder);
			}
		}
	}
	else
	{
		TArray<INT> VertIndices;
		for(INT VertIdx=0;VertIdx<PolyVerts.Num();VertIdx++)
		{
			FVector Vert = NavMesh->GetVertLocation(PolyVerts(VertIdx));
			VertIndices.AddItem(MeshBuilder.AddVertex(Vert, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255)));
		}

		for(INT VertIdx=PolyVerts.Num()-3;VertIdx>=0;VertIdx--)
		{
			MeshBuilder.AddTriangle(VertIndices(VertIndices.Num()-1),VertIndices(VertIdx+1),VertIndices(VertIdx));
		}
	}
}
void DrawBlockingPoly( FNavMeshPolyBase* Poly, UNavigationMeshBase* NormalMesh, const FSceneView* View, FDynamicMeshBuilder& MeshBuilder ) 
{
	// skip polys which are fully loaded or non-cross pylon
	UBOOL bCPBlock = FALSE;
	for(INT EdgeIdx=0;EdgeIdx<Poly->PolyEdges.Num();EdgeIdx++)
	{
		// EdgeID is from the NORMAL mesh
		FNavMeshEdgeBase* LinkedEdge = Poly->GetEdgeFromIdx(EdgeIdx,NormalMesh);
		if(LinkedEdge!=NULL)
		{
			FNavMeshCrossPylonEdge* CPEdge = (FNavMeshCrossPylonEdge*)LinkedEdge;

			// if both references aren't valid, then consider this poly as valid collision
			if(!CPEdge->Poly0Ref.OwningPylon.Guid.IsValid() || !CPEdge->Poly1Ref.OwningPylon.Guid.IsValid())
			{
				bCPBlock=TRUE;
				break;
			}
		}
	}

	// if this is a CP poly, and all pylons are loaded then don't draw solid
	if(!bCPBlock && Poly->GetNumEdges() > 0 && View->ViewFrustum.IntersectBox(Poly->GetPolyCenter(), Poly->BoxBounds.GetExtent()))
	{
		return;
	}

	Poly->DrawSolidPoly( MeshBuilder );
}

void UNavigationMeshBase::DrawSolidMesh(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
{

	UBOOL bObstacleMesh = IsObstacleMesh() || IsDynamicObstacleMesh();
	if(bObstacleMesh)
	{
		UNavigationMeshBase* NormalMesh = GetPylon()->NavMeshPtr;

		// *---> non-cross pylon pass or blocking polys (red)
		{

			FDynamicMeshBuilder MeshBuilder;
		

			FMaterialRenderProxy* NormalBorderMat = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FColor(255,64,64));
			for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
			{
				FNavMeshPolyBase* Poly = &Polys(PolyIdx);

				DrawBlockingPoly(Poly, NormalMesh, View, MeshBuilder);
			}

			for( PolyList::TIterator It(BuildPolys.GetHead());It;++It)
			{
				FNavMeshPolyBase* Poly = *It;

				DrawBlockingPoly(Poly, NormalMesh, View, MeshBuilder);
			}
			MeshBuilder.Draw(PDI,FMatrix::Identity,NormalBorderMat,DPGIndex,0.f);
		}

		// *---> cross-pylon polys which are fully loaded and non-blocking (white)
		{

			FDynamicMeshBuilder MeshBuilder;

			FMaterialRenderProxy* UnBlockedCPMat = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FColor(255,255,255));

			for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
			{
				FNavMeshPolyBase& Poly = Polys(PolyIdx);

				// skip normal polys
				if(Poly.GetNumEdges() < 1)
				{
					continue;

				}
				// skip polys which are fully loaded or non-cross pylon
				UBOOL bBlocked = TRUE;
				for(INT EdgeIdx=0;EdgeIdx<Poly.GetNumEdges();EdgeIdx++)
				{
					FNavMeshEdgeBase* LinkedEdge = Poly.GetEdgeFromIdx(EdgeIdx,NormalMesh);
					if(LinkedEdge != NULL)
					{
						FNavMeshCrossPylonEdge* CPEdge = (FNavMeshCrossPylonEdge*)LinkedEdge;

						// if both references aren't valid, then consider this poly as valid collision
						if(CPEdge->GetPoly0() && CPEdge->GetPoly1())
						{
							bBlocked=FALSE;
							break;
						}
					}
				}


				if(!bBlocked && View->ViewFrustum.IntersectBox(Poly.GetPolyCenter(), Poly.BoxBounds.GetExtent()))
				{
					Poly.DrawSolidPoly( MeshBuilder );
				}
			}

			MeshBuilder.Draw(PDI,FMatrix::Identity,UnBlockedCPMat,DPGIndex,0.f);

		}

		// *---> cross-pylon polys which are valid but not fully loaded (yellow)
		{

			FDynamicMeshBuilder MeshBuilder;

			FMaterialRenderProxy* BlockingCPPolyMat = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FColor(200,255,0));

			for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
			{
				FNavMeshPolyBase& Poly = Polys(PolyIdx);

				// skip normal polys
				if(Poly.GetNumEdges() < 1)
				{
					continue;

				}
				// skip polys which are fully loaded or non-cross pylon
				UBOOL bBlocked = TRUE;
				for(INT EdgeIdx=0;EdgeIdx<Poly.GetNumEdges();EdgeIdx++)
				{
					FNavMeshEdgeBase* LinkedEdge = Poly.GetEdgeFromIdx(EdgeIdx,NormalMesh);
					if(LinkedEdge != NULL)
					{
						FNavMeshCrossPylonEdge* CPEdge = (FNavMeshCrossPylonEdge*)LinkedEdge;

						// if both references aren't valid, then consider this poly as valid collision
						if(CPEdge->Poly0Ref.OwningPylon.Guid.IsValid() && CPEdge->Poly1Ref.OwningPylon.Guid.IsValid())
						{
							bBlocked=FALSE;
							break;
						}
					}
				}


				if(!bBlocked && View->ViewFrustum.IntersectBox(Poly.GetPolyCenter(), Poly.BoxBounds.GetExtent()))
				{
					Poly.DrawSolidPoly( MeshBuilder );
				}
			}

			MeshBuilder.Draw(PDI,FMatrix::Identity,BlockingCPPolyMat,DPGIndex,0.f);

		}

	}
	else // this is the normal mesh, draw everything green
	{


		FDynamicMeshBuilder MeshBuilder;
		
		FMaterialRenderProxy* GroundColorInstance = NULL;
		
		if(GetPylon()->IsValid())
		{
			GroundColorInstance = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FColor(72,255,64));
		}
		else // if the pylon is invalid, draw it grey
		{
			GroundColorInstance = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FColor(50,50,50));
		}

		for( INT PolyIdx = 0; PolyIdx < Polys.Num(); PolyIdx++ )
		{
			FNavMeshPolyBase& Poly = Polys(PolyIdx);

			FBox PolyBounds = Poly.GetPolyBounds(WORLD_SPACE);
			if( View->ViewFrustum.IntersectBox(PolyBounds.GetCenter(), PolyBounds.GetExtent()) )
			{
				Poly.DrawSolidPoly( MeshBuilder );
			}
		}

		MeshBuilder.Draw(PDI,FMatrix::Identity,GroundColorInstance,DPGIndex,0.f);
	}
}


//=============================================================================
// FNavMeshRenderingSceneProxy

/** Represents a NavMeshRenderingComponent to the scene manager. */
class FNavMeshRenderingSceneProxy : public FDebugRenderSceneProxy
{
public:

	FNavMeshRenderingSceneProxy(const UNavMeshRenderingComponent* InComponent):
	FDebugRenderSceneProxy(InComponent)
	{
		bIsNavigationPoint=FALSE;
		// draw nav mesh
		Pylon = Cast<APylon>(InComponent->GetOwner());
		Comp = InComponent;
		if( Pylon != NULL && Pylon->bRenderInShowPaths )
		{
			if( Pylon->NavMeshPtr != NULL )
			{
				Pylon->NavMeshPtr->DrawMesh(this,Pylon);
			}

			if( Pylon->ObstacleMesh != NULL)
			{
				Pylon->ObstacleMesh->DrawMesh(this,Pylon);
			}

			if ( Pylon->DynamicObstacleMesh != NULL )
			{
				Pylon->DynamicObstacleMesh->DrawMesh(this,Pylon);
			}
		}
	}

	FORCEINLINE UBOOL LineInView(const FVector& start, const FVector& End, const FSceneView* View)
	{
		for(INT PlaneIdx=0;PlaneIdx<View->ViewFrustum.Planes.Num();++PlaneIdx)
		{
			const FPlane& CurPlane = View->ViewFrustum.Planes(PlaneIdx);
			if(CurPlane.PlaneDot(start) > 0.f && CurPlane.PlaneDot(End) > 0.f)
			{
				return FALSE;
			}
		}

		return TRUE;
	}

	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		//FMemMark Mark(GRenderingThreadMemStack);

		if ( !Pylon->bSolidObstaclesInGame || GIsEditor )
		{
			// Draw Lines
			for(INT LineIdx=0; LineIdx<Lines.Num(); LineIdx++)
			{
				FDebugLine& Line = Lines(LineIdx);

				if( LineInView(Line.Start,Line.End,View) )
				{
					PDI->DrawLine(Line.Start, Line.End, Line.Color, SDPG_World);
				}
			}

			// Draw Arrows
			for(INT LineIdx=0; LineIdx<ArrowLines.Num(); LineIdx++)
			{
				FArrowLine &Line = ArrowLines(LineIdx);
			
				if( LineInView(Line.Start,Line.End,View) )
				{
					DrawLineArrow(PDI, Line.Start, Line.End, Line.Color, 8.0f);
				}
			}

			// Draw Cylinders
			for(INT CylinderIdx=0; CylinderIdx<Cylinders.Num(); CylinderIdx++)
			{
				FWireCylinder& Cylinder = Cylinders(CylinderIdx);

				if( View->ViewFrustum.IntersectSphere(Cylinder.Base, Cylinder.Radius))
				{
					DrawWireCylinder( PDI, Cylinder.Base, FVector(1,0,0), FVector(0,1,0), FVector(0,0,1),
						Cylinder.Color, Cylinder.Radius, Cylinder.HalfHeight, 16, SDPG_World );
				}
			}

			// Draw Stars
			for(INT StarIdx=0; StarIdx<Stars.Num(); StarIdx++)
			{
				FWireStar& Star = Stars(StarIdx);

				if( View->ViewFrustum.IntersectSphere(Star.Position, Star.Size))
				{
					DrawWireStar(PDI, Star.Position, Star.Size, Star.Color, SDPG_World);
				}
			}

			// Draw Dashed Lines
			for(INT DashIdx=0; DashIdx<DashedLines.Num(); DashIdx++)
			{
				FDashedLine& Dash = DashedLines(DashIdx);
				
				if( LineInView(Dash.Start,Dash.End,View) )
				{
					DrawDashedLine(PDI, Dash.Start, Dash.End, Dash.Color, Dash.DashSize, SDPG_World);
				}
			}	

			if(Pylon->bDrawPolyBounds && Pylon->NavMeshPtr != NULL)
			{
				for( INT PolyIdx=0;PolyIdx<Pylon->NavMeshPtr->Polys.Num();++PolyIdx)
				{
					FNavMeshPolyBase* CurPoly = &Pylon->NavMeshPtr->Polys(PolyIdx);
					DrawWireBox(PDI,CurPoly->BoxBounds.TransformBy(Pylon->NavMeshPtr->LocalToWorld),FColor(255,255,0),SDPG_World);
				}
			}
		}
		if( Pylon->bRenderInShowPaths )
		{
			if ( GIsEditor )
			{
				// draw walkable ground first
				if( Pylon->NavMeshPtr != NULL && Pylon->bDrawWalkableSurface )
				{
					Pylon->NavMeshPtr->DrawSolidMesh(PDI,View,DPGIndex,Flags);
				}
			}
			if ( Pylon->bSolidObstaclesInGame || GIsEditor )
			{
				// draw obstacle mesh
				if( Pylon->ObstacleMesh != NULL && Pylon->bDrawObstacleSurface )
				{
					Pylon->ObstacleMesh->DrawSolidMesh(PDI,View,DPGIndex,Flags);
				}
				// draw dynamic obstacle mesh
				if( Pylon->DynamicObstacleMesh != NULL && Pylon->bDrawObstacleSurface )
				{
					Pylon->DynamicObstacleMesh->DrawSolidMesh(PDI,View,DPGIndex,Flags);
				}
			}
		}
	};

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		const UBOOL bVisible = (View->Family->ShowFlags & SHOW_Paths) != 0;
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View) && bVisible;
		Result.bTranslucentRelevance = IsShown(View) && bVisible && GIsEditor;
		Result.SetDPG(SDPG_World,TRUE);
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FDebugRenderSceneProxy::GetAllocatedSize() ); }

	APylon* Pylon;
	const UNavMeshRenderingComponent* Comp;
};

//=============================================================================
// UNavMeshRenderingComponent

IMPLEMENT_CLASS(UNavMeshRenderingComponent);

/**
 * Creates a new scene proxy for the NavMesh rendering component.
 * @return	Pointer to the FNavMeshRenderingSceneProxy
 */
FPrimitiveSceneProxy* UNavMeshRenderingComponent::CreateSceneProxy()
{
	return new FNavMeshRenderingSceneProxy(this);
}

void UNavMeshRenderingComponent::UpdateBounds()
{
	FBox BoundingBox(0);

	APylon* Pylon = Cast<APylon>(Owner);
	if( Pylon != NULL )
	{
		if( Pylon->NavMeshPtr != NULL && Pylon->NavMeshPtr->Polys.Num() > 0 )
		{
			if( Pylon->NavMeshPtr->bNeedsTransform )
			{
				BoundingBox = Pylon->NavMeshPtr->BoxBounds.TransformBy(Pylon->NavMeshPtr->LocalToWorld);
			}
			else
			{
				BoundingBox = Pylon->NavMeshPtr->BoxBounds;
			}
		}
		else
		{
			BoundingBox = Pylon->GetExpansionBounds();

			// Pylon ExpansionVolumes are not necessarily loaded when this function is first called, so
			// if bounding box is of size 0, try using the ExpansionRadius instead
			if( (BoundingBox.Max - BoundingBox.Min).SizeSquared() < KINDA_SMALL_NUMBER && Pylon->ExpansionRadius > KINDA_SMALL_NUMBER )
			{
				BoundingBox = FBox::BuildAABB(Pylon->GetExpansionSphereCenter(),FVector(Pylon->ExpansionRadius));
			}
		}
	}

	Bounds = FBoxSphereBounds(BoundingBox);
}

