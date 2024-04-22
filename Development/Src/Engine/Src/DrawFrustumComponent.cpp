/*=============================================================================
	DrawFrsutumComponent.cpp: UDrawFrsutumComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UDrawFrustumComponent);

/** Represents a draw frustum to the scene manager. */
class FDrawFrustumSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** 
	* Initialization constructor. 
	* @param	InComponent - game component to draw in the scene
	*/
	FDrawFrustumSceneProxy(const UDrawFrustumComponent* InComponent)
	:	FPrimitiveSceneProxy(InComponent)
	,	LevelColor(255,255,255)
	,	FrustumColor(InComponent->FrustumColor)
	,	FrustumAngle(InComponent->FrustumAngle)
	,	FrustumAspectRatio(InComponent->FrustumAspectRatio)
	,	FrustumStartDist(InComponent->FrustumStartDist)
	,	FrustumEndDist(InComponent->FrustumEndDist)
	,	Texture(InComponent->Texture ? InComponent->Texture->Resource : NULL)
	{		
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
		// Determine the DPG the primitive should be drawn in for this view.
		if (GetDepthPriorityGroup(View) == DPGIndex)
		{
			FVector Direction(1,0,0);
			FVector LeftVector(0,1,0);
			FVector UpVector(0,0,1);

			FVector Verts[8];

			// FOVAngle controls the horizontal angle.
			FLOAT HozHalfAngle = (FrustumAngle) * ((FLOAT)PI/360.f);
			FLOAT HozLength = FrustumStartDist * appTan(HozHalfAngle);
			FLOAT VertLength = HozLength/FrustumAspectRatio;

			// near plane verts
			Verts[0] = (Direction * FrustumStartDist) + (UpVector * VertLength) + (LeftVector * HozLength);
			Verts[1] = (Direction * FrustumStartDist) + (UpVector * VertLength) - (LeftVector * HozLength);
			Verts[2] = (Direction * FrustumStartDist) - (UpVector * VertLength) - (LeftVector * HozLength);
			Verts[3] = (Direction * FrustumStartDist) - (UpVector * VertLength) + (LeftVector * HozLength);

			HozLength = FrustumEndDist * appTan(HozHalfAngle);
			VertLength = HozLength/FrustumAspectRatio;

			// far plane verts
			Verts[4] = (Direction * FrustumEndDist) + (UpVector * VertLength) + (LeftVector * HozLength);
			Verts[5] = (Direction * FrustumEndDist) + (UpVector * VertLength) - (LeftVector * HozLength);
			Verts[6] = (Direction * FrustumEndDist) - (UpVector * VertLength) - (LeftVector * HozLength);
			Verts[7] = (Direction * FrustumEndDist) - (UpVector * VertLength) + (LeftVector * HozLength);

			for( INT x = 0 ; x < 8 ; ++x )
			{
				Verts[x] = LocalToWorld.TransformFVector( Verts[x] );
			}

			PDI->DrawLine( Verts[0], Verts[1], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[1], Verts[2], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[2], Verts[3], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[3], Verts[0], FrustumColor, DPGIndex );

			PDI->DrawLine( Verts[4], Verts[5], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[5], Verts[6], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[6], Verts[7], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[7], Verts[4], FrustumColor, DPGIndex );

			PDI->DrawLine( Verts[0], Verts[4], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[1], Verts[5], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[2], Verts[6], FrustumColor, DPGIndex );
			PDI->DrawLine( Verts[3], Verts[7], FrustumColor, DPGIndex );

			// GEMINI_TODO: handle rendering texture to near plane
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View) && (View->Family->ShowFlags & SHOW_CamFrustums);
		Result.SetDPG(GetDepthPriorityGroup(View),TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FColor LevelColor;
	FColor FrustumColor;
	FLOAT FrustumAngle;
	FLOAT FrustumAspectRatio;
	FLOAT FrustumStartDist;
	FLOAT FrustumEndDist;
	const FTexture* Texture;
	BITFIELD DepthPriorityGroup : UCONST_SDPG_NumBits;
};

/**
* Create the scene proxy needed to render the draw frustum component
*/
FPrimitiveSceneProxy* UDrawFrustumComponent::CreateSceneProxy()
{
	return new FDrawFrustumSceneProxy(this);
}

/**
* Update the bounding box/sphere for this component
*/
void UDrawFrustumComponent::UpdateBounds()
{
	Bounds = FBoxSphereBounds( LocalToWorld.TransformFVector(FVector(0,0,0)), FVector(FrustumEndDist), FrustumEndDist );
}
