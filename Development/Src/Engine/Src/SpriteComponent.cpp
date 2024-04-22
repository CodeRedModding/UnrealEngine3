/*=============================================================================
	SpriteComponent.cpp: USpriteComponent implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "LevelUtils.h"

IMPLEMENT_CLASS(USpriteComponent);

/** Represents a sprite to the scene manager. */
class FSpriteSceneProxy : public FPrimitiveSceneProxy
{
public:

	/** Initialization constructor. */
	FSpriteSceneProxy(const USpriteComponent* InComponent):
		FPrimitiveSceneProxy(InComponent),
		ScreenSize(InComponent->ScreenSize),
		U(InComponent->U),	
		V(InComponent->V),
		Color(255,255,255),
		LevelColor(255,255,255),
		PropertyColor(255,255,255),
		bIsScreenSizeScaled(InComponent->bIsScreenSizeScaled)
	{
#if WITH_EDITORONLY_DATA
		// Extract the sprite category from the component if in the editor
		if ( GIsEditor )
		{
			SpriteCategoryIndex = GEngine->GetSpriteCategoryIndex( InComponent->SpriteCategoryName );
		}
#endif // if WITH_EDITORONLY_DATA

		// Calculate the scale factor for the sprite.
		FLOAT Scale = InComponent->Scale;
		if(InComponent->GetOwner())
		{
			Scale *= InComponent->GetOwner()->DrawScale;
		}

		if(InComponent->Sprite)
		{
			Texture = InComponent->Sprite;
			// Set UL and VL to the size of the texture if they are set to 0.0, otherwise use the given value
			UL = InComponent->UL == 0.0f ? InComponent->Sprite->GetSurfaceWidth() : InComponent->UL;
			VL = InComponent->VL == 0.0f ? InComponent->Sprite->GetSurfaceHeight() : InComponent->VL;
			SizeX = Scale * UL;
			SizeY = Scale * VL;
		}
		else
		{
			Texture = NULL;
			SizeX = SizeY = UL = VL = 0;
		}

        AActor* Owner = InComponent->GetOwner();
		if (Owner)
		{
			// If the owner of this sprite component is an ALight, tint the sprite
			// to match the lights color.  
			// Note: If the actor is a light it ignores the EditorIconColor var and uses the color of the light instead.
			ALight* Light = Cast<ALight>( Owner );
			if( Light )
			{
				if( Light->LightComponent )
				{
					Color = Light->LightComponent->LightColor.ReinterpretAsLinear();
					Color.A = 255;
				}
			}
#if WITH_EDITORONLY_DATA
			else if (GIsEditor)
			{
				// The sprite should be tinted whatever the owners icon color is set to.
				Color = Owner->EditorIconColor;
				// Ensure sprite is not transparent.
				Color.A = 255;
			}
#endif // WITH_EDITORONLY_DATA

			//save off override states
			bIsActorLocked = Owner->bLockLocation;

			// Level colorization
			ULevel* Level = Owner->GetLevel();
			ULevelStreaming* LevelStreaming = FLevelUtils::FindStreamingLevel( Level );
			if ( LevelStreaming )
			{
				// Selection takes priority over level coloration.
				LevelColor = LevelStreaming->DrawColor;
			}
		}

		GEngine->GetPropertyColorationColor( (UObject*)InComponent, PropertyColor );
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
		FTexture* TextureResource = Texture != NULL ? Texture->Resource : NULL;
		// Determine the DPG the primitive should be drawn in for this view.
		if ((GetViewRelevance(View).GetDPG(DPGIndex) == TRUE) && TextureResource)
		{
			// Calculate the view-dependent scaling factor.
			FLOAT ViewedSizeX = SizeX;
			FLOAT ViewedSizeY = SizeY;
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

			FColor ColorToUse = Color;

			// Override the colors for special situations
			if ( bSelected )
			{
				ColorToUse = FColor(128,230,128);
			}
			else if( bHovered )
			{
				ColorToUse = FColor( 220, 255, 220 );
			}

			// Sprites of locked actors draw in red.
			if (bIsActorLocked)
			{
				ColorToUse = FColor(255,0,0);
			}
			FColor LevelColorToUse = bSelected ? ColorToUse : LevelColor;

			const FColor& SpriteColor = (View->Family->ShowFlags & SHOW_LevelColoration) ? LevelColorToUse :
											( (View->Family->ShowFlags & SHOW_PropertyColoration) ? PropertyColor : ColorToUse );

			PDI->DrawSprite(
				Origin,
				ViewedSizeX,
				ViewedSizeY,
				TextureResource,
				SpriteColor,
				DPGIndex,
				U,UL,V,VL
				);
		}
	}
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		UBOOL bVisible = (View->Family->ShowFlags & SHOW_Sprites) ? TRUE : FALSE;
#if !CONSOLE
		if ( GIsEditor && bVisible && SpriteCategoryIndex != INDEX_NONE && SpriteCategoryIndex < View->SpriteCategoryVisibility.Num() )
		{
			bVisible = View->SpriteCategoryVisibility( SpriteCategoryIndex );
		}
#endif
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
		Origin = LocalToWorld.GetOrigin();
	}
	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FVector Origin;
	FLOAT SizeX;
	FLOAT SizeY;
	const FLOAT ScreenSize;
	const UTexture2D* Texture;
	const FLOAT U;
	FLOAT UL;
	const FLOAT V;
	FLOAT VL;
	FColor Color;
	FColor LevelColor;
	FColor PropertyColor;
	const BITFIELD bIsScreenSizeScaled : 1;
	BITFIELD bIsActorLocked : 1;
#if !CONSOLE
	INT SpriteCategoryIndex;
#endif // #if !CONSOLE
};

FPrimitiveSceneProxy* USpriteComponent::CreateSceneProxy()
{
	return new FSpriteSceneProxy(this);
}

void USpriteComponent::UpdateBounds()
{
	const FLOAT NewScale = (Owner ? Owner->DrawScale : 1.0f) * (Sprite ? (FLOAT)Max(Sprite->SizeX,Sprite->SizeY) : 1.0f);
	Bounds = FBoxSphereBounds(GetOrigin(),FVector(NewScale,NewScale,NewScale),appSqrt(3.0f * Square(NewScale)));
}

/** Change the sprite texture used by this component */
void USpriteComponent::SetSprite(UTexture2D* NewSprite)
{
	FComponentReattachContext ReattachContext(this);
	Sprite = NewSprite;
}

/** Change the sprite's UVs */
void USpriteComponent::SetUV(INT NewU, INT NewUL, INT NewV, INT NewVL)
{
	FComponentReattachContext ReattachContext(this);
	U = NewU;
	UL = NewUL;
	V = NewV;
	VL = NewVL;
}

/** Change the sprite texture and the UV's used by this component */
void USpriteComponent::SetSpriteAndUV(UTexture2D* NewSprite, INT NewU, INT NewUL, INT NewV, INT NewVL)
{
	U = NewU;
	UL = NewUL;
	V = NewV;
	VL = NewVL;
	SetSprite(NewSprite);
}
