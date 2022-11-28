//==============================================================================
// ProximityObjective
//==============================================================================
// Created by Laurent Delayen
// © 2003, Epic Games, Inc.  All Rights Reserved
//==============================================================================

class Objective_Proximity extends Objective_Game
	placeable;

simulated function PostBeginPlay()
{
	super.PostBeginPlay();
	SetCollision( true, bBlockActors );
}

event Touch( Actor Other, vector HitLocation, vector HitNormal )
{
	local Pawn P;

	P = Pawn(Other);
	if ( P != None && IsInstigatorRelevant(P) )
		CompleteObjective( P );
}

function bool IsInstigatorRelevant( Pawn P )
{
	//Instigator = FindInstigator( P );
	//log("ProximityObjective::IsInstigatorRelevant Health:" $ P.Health @ "bDeleteMe:" $ P.bDeleteMe @ "IsPlayerPawn():" $ P.IsPlayerPawn() );
	if ( P.Health < 1 || P.bDeleteMe || !P.IsPlayerPawn() )
		return false;

	return super.IsInstigatorRelevant(P);
}

/* hack if Objective is disabled by a vehicle, with no active controller */
function Pawn FindInstigator( Pawn Other )
{
	// !!LD merge
	// Hack if player is in turret and not controlling vehicle...
	//if ( Vehicle(Other) != None && Vehicle(Other).Controller == None )
	//	return Vehicle(Other).GetInstigator();

	return Other;
}

simulated function ObjectiveCompleted()
{
	super.ObjectiveCompleted();
	SetCollision( false, bBlockActors );
}

/* Reset() - reset actor to initial state - used when restarting level without reloading. */
function Reset()
{
	super.Reset();
	SetCollision( true, bBlockActors );
}

defaultproperties
{
	bCollideActors=true
	bNotBased=true
	bShouldBaseAtStartup=false
	bIgnoreEncroachers=true
	bCollideWhenPlacing=false
	bOnlyAffectPawns=true
	bStatic=false
	bNoDelete=true
	ObjectiveName="Proximity Objective"
}