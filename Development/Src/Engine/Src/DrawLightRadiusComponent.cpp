/*=============================================================================
	DrawLightRadiusComponent.cpp: DrawLightRadius component implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UDrawLightRadiusComponent);

///////////////////////////////////////////////////////////////////////////////
// UDrawLightRadiusComponent
///////////////////////////////////////////////////////////////////////////////

/**
 * Creates a new scene proxy for the light radius component.
 * @return	Pointer to the FDrawLightRadiusSceneProxy
 */
FPrimitiveSceneProxy* UDrawLightRadiusComponent::CreateSceneProxy()
{
	/** Represents a DrawLightRadiusComponent to the scene manager. */
	class FDrawLightRadiusSceneProxy : public FPrimitiveSceneProxy
	{
	public:

		/** Initialization constructor. */
		FDrawLightRadiusSceneProxy(const UDrawLightRadiusComponent* InComponent)
			: FPrimitiveSceneProxy(InComponent),
			  SphereColor(InComponent->SphereColor),
			  SphereMaterial(InComponent->SphereMaterial),
			  SphereRadius(InComponent->SphereRadius),
			  SphereSides(InComponent->SphereSides),
			  bDrawWireSphere(InComponent->bDrawWireSphere),
			  bDrawLitSphere(InComponent->bDrawLitSphere)
		  {}

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
			if (bSelected)
			{
				if(bDrawWireSphere)
				{
					DrawCircle(PDI,LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(1), SphereColor, SphereRadius, SphereSides, SDPG_World);
					DrawCircle(PDI,LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(0), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides, SDPG_World);
					DrawCircle(PDI,LocalToWorld.GetOrigin(), LocalToWorld.GetAxis(1), LocalToWorld.GetAxis(2), SphereColor, SphereRadius, SphereSides, SDPG_World);
				}

				if(bDrawLitSphere && SphereMaterial && !(View->Family->ShowFlags & SHOW_Wireframe))
				{
					DrawSphere(PDI,LocalToWorld.GetOrigin(), FVector(SphereRadius), SphereSides, SphereSides/2, SphereMaterial->GetRenderProxy(TRUE), SDPG_World);
				}
			}
		}

		virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
		{
			const UBOOL bVisible = (View->Family->ShowFlags & SHOW_LightRadius) != 0;
			FPrimitiveViewRelevance Result;
			Result.bDynamicRelevance = IsShown(View) && bVisible;
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
		const FColor				SphereColor;
		const UMaterialInterface*	SphereMaterial;
		const FLOAT					SphereRadius;
		const INT					SphereSides;
		const BITFIELD				bDrawWireSphere:1;
		const BITFIELD				bDrawLitSphere:1;
	};

	return new FDrawLightRadiusSceneProxy(this);
}

/**
 * Updates the bounds for the component.
 */
void UDrawLightRadiusComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( FVector(0,0,0), FVector(SphereRadius), SphereRadius ).TransformBy(LocalToWorld);
}
