/*=============================================================================
	CoverGroupRenderingComponent.cpp: Unreal pathnode placement

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.

=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAIClasses.h"
#include "DebugRenderSceneProxy.h"

IMPLEMENT_CLASS(UCoverGroupRenderingComponent);

/** Represents a CoverGroupRenderingComponent to the scene manager. */
class FCoverGroupRenderingSceneProxy : public FDebugRenderSceneProxy
{
public:

	FCoverGroupRenderingSceneProxy(const UCoverGroupRenderingComponent* InComponent):
		FDebugRenderSceneProxy(InComponent)
	{
		ACoverGroup *Group = Cast<ACoverGroup>(InComponent->GetOwner());
		check(Group);
		for( INT Idx = 0; Idx < Group->CoverLinkRefs.Num(); Idx++ )
		{
			ACoverLink* Link = Cast<ACoverLink>(~Group->CoverLinkRefs(Idx));
		  
			// If link is valid
			if( Link && (Link->IsSelected() || Group->IsSelected()) )
			{
				// Red link if disabled
				FColor LinkColor = FColor(255,0,0);
				if( Link->IsEnabled() )
				{
					// Green link if enabled
					LinkColor = FColor(0,255,0);
				}
				// Draw line to it
				new(DashedLines) FDashedLine( Group->Location, Link->Location, LinkColor, 32.f );
			}
		}

		if( Group->IsSelected() && 
			Group->AutoSelectHeight > 0.f && 
			Group->AutoSelectRadius > 0.f )
		{
			new(Cylinders) FWireCylinder( Group->Location - FVector(0,0,Group->AutoSelectHeight*0.5f), Group->AutoSelectRadius, Group->AutoSelectHeight*0.5f, FColor(0,255,0) );
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View);
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

void UCoverGroupRenderingComponent::UpdateBounds()
{
	FBox BoundingBox(0);
	ACoverGroup *Group = Cast<ACoverGroup>(Owner);

	if( Group )
	{
		BoundingBox += Group->Location;
		for( INT LinkIdx = 0; LinkIdx < Group->CoverLinkRefs.Num(); LinkIdx++ )
		{
			ACoverLink* Link = Cast<ACoverLink>(~Group->CoverLinkRefs(LinkIdx));
			if( Link )
			{
				BoundingBox += Link->Location;
				for( INT SlotIdx = 0; SlotIdx < Link->Slots.Num(); SlotIdx++ )
				{
					BoundingBox += Link->GetSlotLocation(SlotIdx);
				}
			}
		}
	}
	Bounds = FBoxSphereBounds(BoundingBox.ExpandBy(64.f));
}

FPrimitiveSceneProxy* UCoverGroupRenderingComponent::CreateSceneProxy()
{
	return new FCoverGroupRenderingSceneProxy(this);
}

UBOOL UCoverGroupRenderingComponent::ShouldRecreateProxyOnUpdateTransform() const
{
	// The cover group scene proxy caches a lot of transform dependent info, so it's easier to just recreate it when the transform changes.
	return TRUE;
}
