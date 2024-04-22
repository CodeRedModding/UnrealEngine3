/*=============================================================================
	PathRenderingComponent.cpp: A component that renders a path.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 =============================================================================*/

#include "EnginePrivate.h"
#include "UnPath.h"
#include "EngineAIClasses.h"
#include "DebugRenderSceneProxy.h"


//=============================================================================
// FPathRenderingSceneProxy

/** Represents a PathRenderingComponent to the scene manager. */
class FPathRenderingSceneProxy : public FDebugRenderSceneProxy
{
public:

	FPathRenderingSceneProxy(const UPathRenderingComponent* InComponent):
	FDebugRenderSceneProxy(InComponent)
	{
		// draw reach specs
		ANavigationPoint *Nav = Cast<ANavigationPoint>(InComponent->GetOwner());

		if( Nav != NULL )
		{
			if( Nav->PathList.Num() > 0 )
			{
				for (INT Idx = 0; Idx < Nav->PathList.Num(); Idx++)
				{
					UReachSpec* ReachSpec = Nav->PathList(Idx);
					if( ReachSpec != NULL && !ReachSpec->bDisabled )
					{
						ReachSpec->AddToDebugRenderProxy(this);
					}
				}
			}

			if( Nav->bBlocked )
			{
				new(Stars) FWireStar(Nav->Location + FVector(0,0,40), FColor(255,0,0), 5);
			}
			if( Nav->FearCost > 0 )
			{
				new(Stars) FWireStar(Nav->Location + FVector(0,0,60), FColor(255,128,255), 5);
			}

			// draw cylinder
			if (Nav->IsSelected() && Nav->CylinderComponent != NULL)
			{
				const INT Idx = Cylinders.Add();
				FWireCylinder &Cylinder = Cylinders(Idx);

				Cylinder.Base = Nav->CylinderComponent->GetOrigin();
				Cylinder.Color = GEngine->C_ScaleBoxHi;
				Cylinder.Radius = Nav->CylinderComponent->CollisionRadius;
				Cylinder.HalfHeight = Nav->CylinderComponent->CollisionHeight;
			}

		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		const UBOOL bVisible = (View->Family->ShowFlags & SHOW_Paths) != 0;
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View) && bVisible /*&& bSelected*/;
		Result.SetDPG(SDPG_World,TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FDebugRenderSceneProxy::GetAllocatedSize() ); }
};

//=============================================================================
// UPathRenderingComponent

IMPLEMENT_CLASS(UPathRenderingComponent);

/**
 * Creates a new scene proxy for the path rendering component.
 * @return	Pointer to the FPathRenderingSceneProxy
 */
FPrimitiveSceneProxy* UPathRenderingComponent::CreateSceneProxy()
{
	return new FPathRenderingSceneProxy(this);
}

void UPathRenderingComponent::UpdateBounds()
{
	FBox BoundingBox(0);

	ANavigationPoint *Nav = Cast<ANavigationPoint>(Owner);
	if (Nav != NULL &&
		Nav->PathList.Num() > 0)
	{
		for (INT Idx = 0; Idx < Nav->PathList.Num(); Idx++)
		{
			UReachSpec* Reach = Nav->PathList(Idx);
			if(  Reach != NULL &&
				!Reach->bDisabled &&
				 Reach->Start != NULL &&
				*Reach->End != NULL )
			{
				BoundingBox += Reach->Start->Location;
				BoundingBox += Reach->End->Location;
			}
		}
	}
	Bounds = FBoxSphereBounds(BoundingBox);
}

