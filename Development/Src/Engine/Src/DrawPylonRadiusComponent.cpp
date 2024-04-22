/*=============================================================================
DrawPylonRadiusComponent.cpp: DrawPylonRadius component implementation.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UDrawPylonRadiusComponent);

///////////////////////////////////////////////////////////////////////////////
// UDrawPylonRadiusComponent
///////////////////////////////////////////////////////////////////////////////

/**
* Creates a new scene proxy for the pylon radius component.
* @return	Pointer to the FDrawLightRadiusSceneProxy
*/
FPrimitiveSceneProxy* UDrawPylonRadiusComponent::CreateSceneProxy()
{
	/** Represents a DrawPylonRadiusComp to the scene manager. */
	class FDrawPylonRadiusSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FDrawPylonRadiusSceneProxy(const UDrawPylonRadiusComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			SphereColor(InComponent->SphereColor),
			SphereMaterial(InComponent->SphereMaterial),
			SphereRadius(InComponent->SphereRadius),
			SphereSides(InComponent->SphereSides),
			bDrawWireSphere(InComponent->bDrawWireSphere),
			bDrawLitSphere(InComponent->bDrawLitSphere)
		{
			OwningPylon = Cast<APylon>(InComponent->GetOwner());
		}

		// FPrimitiveSceneProxy interface.

		/** 
		* Draw the scene proxy as a dynamic element
		*
		* @param	PDI - draw interface to render to
		* @param	View - current view
		* @param	DPGIndex - current depth priority 
		* @param	Flags - optional set of flags from EDrawDynamicElementFlags
		*/
		virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
		{
			FVector SphereOrigin(0.f);

			if(OwningPylon)
			{
				if(OwningPylon->ExpansionVolumes.Num() > 0)
				{
					// if we have volumes, draw them instead of the radius
					for(INT VolumeIdx=0;VolumeIdx<OwningPylon->ExpansionVolumes.Num();++VolumeIdx)
					{
						AVolume* CurVolume = OwningPylon->ExpansionVolumes(VolumeIdx);

						if((CurVolume == NULL) || (CurVolume->Brush == NULL))
						{
							continue;
						}

						// Draw forground line to this brush
						PDI->DrawLine(OwningPylon->Location,CurVolume->GetComponentsBoundingBox(TRUE).GetCenter(),FColor(255,255,0),DPGIndex,2.0f);

						FDynamicMeshBuilder MeshBuilder;
						//FMaterialRenderProxy* ExpansionVolumeMat = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->GeomMaterial->GetRenderProxy(FALSE),FColor(0,0,200));
						FMaterialRenderProxy* ExpansionVolumeMat = new(GRenderingThreadMemStack) FColoredMaterialRenderProxy(GEngine->EditorBrushMaterial->GetRenderProxy(FALSE),FColor(0,0,200));

						INT VertexOffset = 0;

						for( INT PolyIdx = 0 ; PolyIdx < CurVolume->Brush->Polys->Element.Num() ; ++PolyIdx )
						{
							const FPoly* Poly = &CurVolume->Brush->Polys->Element(PolyIdx);

							if( Poly->Vertices.Num() > 2 )
							{
								const FVector Vertex0 = Poly->Vertices(0);
								FVector Vertex1 = Poly->Vertices(1);

								MeshBuilder.AddVertex(Vertex0, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
								MeshBuilder.AddVertex(Vertex1, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

								for( INT VertexIdx = 2 ; VertexIdx < Poly->Vertices.Num() ; ++VertexIdx )
								{
									const FVector Vertex2 = Poly->Vertices(VertexIdx);
									MeshBuilder.AddVertex(Vertex2, FVector2D(0,0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
									MeshBuilder.AddTriangle(VertexOffset,VertexOffset + VertexIdx,VertexOffset+VertexIdx-1);
									Vertex1 = Vertex2;
								}

								// Increment the vertex offset so the next polygon uses the correct vertex indices.
								VertexOffset += Poly->Vertices.Num();
							}
						}

						// Flush the mesh triangles.
						MeshBuilder.Draw(PDI, CurVolume->LocalToWorld(), ExpansionVolumeMat, DPGIndex, 0.f);
					}
				}
				else
				{
					if(OwningPylon->bUseExpansionSphereOverride)
					{
						SphereOrigin = OwningPylon->ExpansionSphereCenter;
						PDI->DrawLine(LocalToWorld.GetOrigin(),SphereOrigin,FColor(255,255,255),DPGIndex,2.0f);
					}
					else
					{
						SphereOrigin = LocalToWorld.GetOrigin();
					}

					if(bDrawWireSphere)
					{
						FVector Scale = OwningPylon->DrawScale3D * OwningPylon->DrawScale * OwningPylon->ExpansionRadius;
 						DrawWireBox(PDI,FBox::BuildAABB(SphereOrigin,Scale),SphereColor,DPGIndex);
// 						DrawCircle(PDI,SphereOrigin, LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1), SphereColor, SphereRadius, SphereSides, DPGIndex);
// 						DrawCircle(PDI,SphereOrigin, LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides, DPGIndex);
// 						DrawCircle(PDI,SphereOrigin, LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides, DPGIndex);
					}

					if(bDrawLitSphere && SphereMaterial && !(View->Family->ShowFlags & SHOW_Wireframe))
					{
						DrawSphere(PDI,SphereOrigin, FVector(SphereRadius), SphereSides, SphereSides/2, SphereMaterial->GetRenderProxy(TRUE), DPGIndex);
					}

				}

				// ** draw lines to conflicting pylons
				TArray<APylon*> ConflictingPylons;
				OwningPylon->CheckBoundsValidityWithOtherPylons(&ConflictingPylons);
				APylon* CurPylon = NULL;

				for( INT Idx=0;Idx<ConflictingPylons.Num();++Idx )
				{
					CurPylon = ConflictingPylons(Idx);
					PDI->DrawLine(OwningPylon->Location,CurPylon->Location,FColor(255,0,0),DPGIndex,10.0f);
				}


				// ** draw line to imposter pylon if one is present
				for( INT Idx=0;Idx<OwningPylon->ImposterPylons.Num();++Idx )
				{
					CurPylon = OwningPylon->ImposterPylons(Idx);
				
					if ( CurPylon != NULL )
					{
						PDI->DrawLine( OwningPylon->Location, CurPylon->Location, FColor(0,0,255),DPGIndex,5.0f);
					}
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bSelected;
			Result.bTranslucentRelevance = IsShown(View);
			Result.bDistortionRelevance = IsShown(View);

			Result.SetDPG(SDPG_World,TRUE);
			if (IsShadowCast(View))
			{
				Result.bShadowRelevance = TRUE;
			}
			return Result;
		}

		virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
		DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	private:
		APylon* OwningPylon;
		const FColor				SphereColor;
		const UMaterialInterface*	SphereMaterial;
		const FLOAT					SphereRadius;
		const INT					SphereSides;
		const BITFIELD				bDrawWireSphere:1;
		const BITFIELD				bDrawLitSphere:1;
	};

	return new FDrawPylonRadiusSceneProxy(this);
}

/**
* Updates the bounds for the component.
*/
void UDrawPylonRadiusComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( FVector(0,0,0), FVector(SphereRadius), SphereRadius ).TransformBy(LocalToWorld);
}

void UDrawPylonRadiusComponent::Attach()
{
	Super::Attach();
	APylon* OwnerPylon = Cast<APylon>(GetOwner());
	if(OwnerPylon != NULL)
	{
		SphereRadius = OwnerPylon->ExpansionRadius;
	}

}


