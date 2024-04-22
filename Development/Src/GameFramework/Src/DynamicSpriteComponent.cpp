/*=============================================================================
	DynamicSpriteComponent.cpp: UDynamicSpriteComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "GameFramework.h"

IMPLEMENT_CLASS(UDynamicSpriteComponent);

/** Represents a sprite to the scene manager. */
class FDynamicSpriteSceneProxy : public FPrimitiveSceneProxy, public FTickableObjectRenderThread
{
public:

	/** Initialization constructor. */
	FDynamicSpriteSceneProxy(const UDynamicSpriteComponent* InComponent):
		FPrimitiveSceneProxy(InComponent),
		FTickableObjectRenderThread(FALSE),	// Don't register immediately so we can do it on the rendering thread
		ScreenSize(InComponent->ScreenSize),
		U(InComponent->U),	
		V(InComponent->V),
		LoopCount(InComponent->LoopCount),
		LocationOffset(InComponent->LocationOffset),
		bIsScreenSizeScaled(InComponent->bIsScreenSizeScaled),
		bInitialized(FALSE)
	{
		// FInterpCurves contain TArrayNoInit. We must zero out their memory on initialization.
		appMemzero( &AnimatedScale.Points, sizeof(AnimatedScale.Points) );
		appMemzero( &AnimatedColor.Points, sizeof(AnimatedColor.Points) );
		appMemzero( &AnimatedPosition.Points, sizeof(AnimatedPosition.Points) );

		// Calculate the scale factor for the sprite.
		FLOAT Scale = InComponent->Scale;
		if(InComponent->GetOwner())
		{
			Scale *= InComponent->GetOwner()->DrawScale;
		}

		if(InComponent->Sprite)
		{
			Texture = InComponent->Sprite->Resource;
			// Set UL and VL to the size of the texture if they are set to 0.0, otherwise use the given value
			UL = InComponent->UL == 0.0f ? InComponent->Sprite->GetSurfaceWidth() : InComponent->UL;
			VL = InComponent->VL == 0.0f ? InComponent->Sprite->GetSurfaceHeight() : InComponent->VL;
			SizeX = Scale * UL;
			SizeY = Scale * VL;

			// Copy the simple animation data over so this can all run on the render thread
			AnimatedScale = InComponent->AnimatedScale;
		    AnimatedColor = InComponent->AnimatedColor;
			AnimatedPosition = InComponent->AnimatedPosition;
		}
		else
		{
			// Don't copy any arrays if there is no texture to render
			Texture = NULL;
			SizeX = SizeY = UL = VL = 0;
		}

		CurrentTime = 0.f;

		// Figure out the total time, which will be the time of whichever animated element is longest
		FLOAT ScaleTime = AnimatedScale.Points.Num() ? AnimatedScale.Points(AnimatedScale.Points.Num()-1).InVal : 0.f;
		FLOAT ColorTime = AnimatedColor.Points.Num() ? AnimatedColor.Points(AnimatedColor.Points.Num()-1).InVal : 0.f;
		FLOAT PosTime = AnimatedPosition.Points.Num() ? AnimatedPosition.Points(AnimatedPosition.Points.Num()-1).InVal : 0.f;
		TotalTime = Max(ScaleTime, Max(ColorTime, PosTime));
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
		if (Texture && (GetViewRelevance(View).GetDPG(DPGIndex) == TRUE))
		{
			FLOAT Scale = AnimatedScale.Eval(CurrentTime, 1.f);

			// Calculate the view-dependent scaling factor.
			FLOAT ViewedSizeX = SizeX * Scale;
			FLOAT ViewedSizeY = SizeY * Scale;
			if (bIsScreenSizeScaled && (View->ProjectionMatrix.M[3][3] != 1.0f))
			{
				const FLOAT ZoomFactor	= Min<FLOAT>(View->ProjectionMatrix.M[0][0], View->ProjectionMatrix.M[1][1]);
				const FLOAT Radius		= View->WorldToScreen(Origin).W * (ScreenSize / ZoomFactor);
				if (Radius < 1.0f)
				{
					ViewedSizeX *= Radius;
					ViewedSizeY *= Radius;
				}
			}

			FColor SpriteColor = FColor(AnimatedColor.Eval(CurrentTime, FLinearColor::White));

			// TODO - AnimatedPosition
			// Because the camera calculations are done during the sprite batch rendering,
			// it would probably be most efficient to pass in a 2D offset to DrawSprite

			PDI->DrawSprite(
				Origin,
				ViewedSizeX,
				ViewedSizeY,
				Texture,
				SpriteColor,
				DPGIndex,
				U,UL,V,VL,
				SE_BLEND_Translucent
				);
		}
	}
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		const UBOOL bVisible = (View->Family->ShowFlags & SHOW_Sprites) ? TRUE : FALSE;
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = IsShown(View);
		Result.SetDPG(GetDepthPriorityGroup(View),bVisible);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}
	virtual void OnTransformChanged()
	{
		Origin = LocalToWorld.TransformFVector(LocationOffset);
	}
	virtual void Tick(FLOAT DeltaTime)
	{
		CurrentTime += DeltaTime;
		if( CurrentTime > TotalTime )
		{
			if( LoopCount < 0 )
			{
				// Loop forever
				CurrentTime = 0.f;
			}
			else if( LoopCount > 0 )
			{
				// Loop until LoopCount is exhausted
				--LoopCount;
				if( LoopCount > 0 )
				{
					CurrentTime = 0.f;
				}
			}
		}
	}
	UBOOL IsTickable() const
	{
		return LoopCount && TotalTime > 0.f;
	}
	UBOOL CreateRenderThreadResources()
	{
		if( !bInitialized )
		{
			FTickableObjectRenderThread::Register();
			bInitialized = TRUE;
		}
		return TRUE;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FVector Origin;
	FLOAT SizeX;
	FLOAT SizeY;
	const FLOAT ScreenSize;
	const FTexture* Texture;
	const FLOAT U;
	FLOAT UL;
	const FLOAT V;
	FLOAT VL;
	FLOAT CurrentTime;
	FLOAT TotalTime;
	INT LoopCount;
	FColor Color;
    FInterpCurveFloat AnimatedScale;
    FInterpCurveLinearColor AnimatedColor;
    FInterpCurveVector2D AnimatedPosition;
    FVector LocationOffset;
	const BITFIELD bIsScreenSizeScaled : 1;
	BITFIELD bInitialized : 1;
};

FPrimitiveSceneProxy* UDynamicSpriteComponent::CreateSceneProxy()
{
	FPrimitiveSceneProxy* Proxy = NULL;
	if (Sprite)
	{
		Proxy = new FDynamicSpriteSceneProxy(this);
	}
	return Proxy;
}

void UDynamicSpriteComponent::UpdateBounds()
{
	FLOAT ScaleMin, ScaleMax;
	AnimatedScale.CalcBounds(ScaleMax, ScaleMin, 0.f);

	const FLOAT NewScale = ScaleMax * (Owner ? Owner->DrawScale : 1.0f) * (Sprite ? (FLOAT)Max(Sprite->SizeX,Sprite->SizeY) : 1.0f);
	Bounds = FBoxSphereBounds(GetOrigin(),FVector(NewScale,NewScale,NewScale),appSqrt(3.0f * Square(NewScale)));
}
