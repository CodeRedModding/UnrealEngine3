/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdBehaviorPoint extends GameCrowdInteractionPoint
	native
	placeable
	config(Crowd)
	dependson(GameCrowdAgentBehavior);

/** Raius of this event */
var config float RadiusOfBehaviorEvent;
/** Duration of how long this event will last **/
var config float DurationOfBehaviorEvent;
/** Event type to pass to crowd within radius */
var() ECrowdBehaviorEvent EventType;
var() bool bRequireLOS;

var   Actor     Initiator;

cpptext
{
	virtual void TickSpecial( FLOAT DeltaSeconds );
	virtual UBOOL IsOverlapping( AActor *Other, FCheckResult* Hit=NULL, UPrimitiveComponent* OtherPrimitiveComponent=NULL, UPrimitiveComponent* MyPrimitiveComponent=NULL );
}

event PostBeginPlay()
{
	Super.PostBeginPlay();

	if( RadiusOfBehaviorEvent > 0.f )
	{
		CylinderComponent.SetCylinderSize( RadiusOfBehaviorEvent, 200.0f );
	}

	if( DurationOfBehaviorEvent > 0.0f )
	{
		SetTimer( DurationOfBehaviorEvent, FALSE, nameof(DestroySelf) );
	}
}

function DestroySelf()
{
	LifeSpan = 0.001f;
}

event Touch( Actor Other, PrimitiveComponent OtherComp, vector HitLocation, vector HitNormal )
{
	local GameCrowdAgent Agent;
	
	Agent = GameCrowdAgent(Other);
	if( Agent != None )
	{
		if( !bRequireLOS || FastTrace( Other.Location, Location ) )
		{
			Agent.HandleBehaviorEvent( EventType, Initiator, FALSE, TRUE );
		}
	}
	Super.Touch(Other, OtherComp, HitLocation, HitNormal);
}

defaultproperties
{
	TickGroup=TG_DuringAsyncWork

	bNoDelete=FALSE
	bCollideActors=TRUE

	Begin Object NAME=CollisionCylinder
		CollideActors=TRUE
		bDrawNonColliding=TRUE
		
		CollisionRadius=512
		CollisionHeight=200

		CylinderColor=(R=0,G=255,B=0)

		bDrawBoundingBox=TRUE
		HiddenGame=FALSE
		HiddenEditor=FALSE
	End Object

	Begin Object Name=Sprite
		Sprite=Texture2D'EditorResources.Crowd.T_Crowd_Behavior'
		HiddenGame=TRUE
		HiddenEditor=FALSE
		AlwaysLoadOnClient=FALSE
		AlwaysLoadOnServer=FALSE

		Scale=0.5
	End Object
}
