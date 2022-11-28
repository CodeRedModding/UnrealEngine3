//=============================================================================
// Copyright 2004 Epic Games - All Rights Reserved.
// Confidential.
//=============================================================================

#include "UTGame.h"

IMPLEMENT_CLASS(UUTCheatManager);

#define STATIC_LINKING_MOJO 1
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName UTGAME_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "UTGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "UTGameClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NATIVES_ONLY
#undef NAMES_ONLY

IMPLEMENT_CLASS(AUTPawn);


/* TryJumpUp()
Check if could jump up over obstruction
*/
UBOOL AUTPawn::TryJumpUp(FVector Dir, DWORD TraceFlags)
{
	FVector Out = 14.f * Dir;
	FCheckResult Hit(1.f);
	FLOAT JumpScale = bCanDoubleJump ? 2.f : 1.f;
	FVector Up = FVector(0.f,0.f,JumpScale*MaxJumpHeight);
	GetLevel()->SingleLineCheck(Hit, this, Location + Up, Location, TRACE_World, GetCylinderExtent());
	FLOAT FirstHit = Hit.Time;
	if ( FirstHit > 0.5f/JumpScale )
	{
		GetLevel()->SingleLineCheck(Hit, this, Location + Up * Hit.Time + Out, Location + Up * Hit.Time, TraceFlags, GetCylinderExtent());
		if ( (Hit.Time < 1.f) && (JumpScale > 1.f) && (FirstHit > 1.f/JumpScale) )
		{
			Up = FVector(0.f,0.f,MaxJumpHeight);
			GetLevel()->SingleLineCheck(Hit, this, Location + Up + Out, Location + Up, TraceFlags, GetCylinderExtent());
		}
		return (Hit.Time == 1.f);
	}
	return false;
}

/*
void APlayerController::PostRender(FSceneNode* SceneNode)
{
    // Render teammate names

    if (PlayerNameArray.Num() <= 0)
        return;

    for (int i=0; i<PlayerNameArray.Num(); i++)
    {
        SceneNode->Viewport->Canvas->Color = PlayerNameArray(i).mColor;
        SceneNode->Viewport->Canvas->CurX  = PlayerNameArray(i).mXPos;
		SceneNode->Viewport->Canvas->CurY  = PlayerNameArray(i).mYPos;
		SceneNode->Viewport->Canvas->ClippedPrint(
            SceneNode->Viewport->Canvas->SmallFont, 1.f, 1.f, 0, 
            *(PlayerNameArray(i).mInfo));
    }

    PlayerNameArray.Empty();
}

void AActor::UpdateOverlay(FLOAT DeltaSeconds)
{
    if( OverlayMaterial )
    {
        if( ClientOverlayTimer != OverlayTimer )
        {
            ClientOverlayTimer = OverlayTimer;
            ClientOverlayCounter = OverlayTimer;
        }
        if( ClientOverlayCounter > 0.0f )
        {
            ClientOverlayCounter -= DeltaSeconds;
            if( ClientOverlayCounter <= 0.0f ) // count down
                ClientOverlayCounter = 0.0f;
        }
        else
        {
            ClientOverlayCounter += DeltaSeconds;
            if( ClientOverlayCounter >= 0.0f ) // count up
                ClientOverlayCounter = 0.0f;
        }
        if( ClientOverlayCounter == 0.0 )
		{
			if ( Role == ROLE_Authority )
				OverlayTimer = 0.f;
			ClientOverlayTimer = 0.f;
            OverlayMaterial = NULL;
		}
		else if ( (Role == ROLE_Authority) && (Abs(OverlayTimer - ClientOverlayCounter) > 1.f) )
		{
			ClientOverlayTimer = ClientOverlayCounter;
			OverlayTimer = ClientOverlayCounter;
		}
    }
}

UBOOL AActor::Tick( FLOAT DeltaSeconds, ELevelTick TickType )
{
	if( (TickType!=LEVELTICK_ViewportsOnly) || PlayerControlled() )
	    UpdateOverlay(DeltaSeconds); //sjs
}
*/
