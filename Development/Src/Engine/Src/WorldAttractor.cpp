/*=============================================================================
	WorldAttractor.cpp: 
	Implementations related to the placeable WorldAttractor.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

/*-----------------------------------------------------------------------------
	AWorldAttractor implementation.
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(AWorldAttractor);

/**
 *  Get the attraction data at the provided time.
 *  @param Time This is time relative to whatever the calling function desires (particle life, emitter life, matinee start, etc.)
 *  @param Data Returned data for the given time.
 */
void AWorldAttractor::GetAttractorDataAtTime(const FLOAT Time, FWorldAttractorData& Data)
{
	Data.bEnabled = bEnabled;
	Data.Location = Location;
	Data.FalloffType = FalloffType;
	Data.FalloffExponent = FalloffExponent.GetValue(Time);
	Data.Range = Range.GetValue(Time);
	Data.Strength = Strength.GetValue(Time);
}

/**
 *  Generate the attraction velocity to add to an actor's current velocity.
 *  @param CurrentLocation The location of the actor at the start of this tick.
 *  @param CurrentTime This is time relative to whatever the calling function desires (particle life, emitter life, matinee start, etc.)
 *  @param DeltaTime This is the time since the last update call.
 *  @param ParticleBoundingRadius Used to calculate drag.
 *  @returns The velocity to add to the actor's current velocity.
 */
FVector AWorldAttractor::GetVelocityForAttraction(const FVector CurrentLocation, const FLOAT CurrentTime, const FLOAT DeltaTime, const FLOAT ParticleBoundingRadius /*= 0.0f*/)
{
	FVector VelToAdd(0.0f, 0.0f, 0.0f);
	FVector Temp(Location - CurrentLocation);
	FLOAT Distance(Temp.Size());

	if(Distance > Range.GetValue(CurrentTime))
	{
		return FVector::ZeroVector;
	}

	Temp.Normalize();
	switch(FalloffType)
	{
	case FOFF_Constant:
		VelToAdd += Temp*Strength.GetValue(CurrentTime);
		break;
	case FOFF_Linear:
		VelToAdd += Temp*(Strength.GetValue(CurrentTime)*(1.0f - (Distance/Range.GetValue(CurrentTime))));
		break;
	case FOFF_Exponent:
		VelToAdd += Temp*(Strength.GetValue(CurrentTime)*(1.0f - (Distance/appPow(Range.GetValue(CurrentTime), Max( (FLOAT) KINDA_SMALL_NUMBER, FalloffExponent.GetValue(CurrentTime))))));
		break;
	}

	return VelToAdd;
}

/**
 *  Override these to manage the list of AWorldAttractor instances.  These will call either RegisterAttractor or
 *  UnregisterAttractor as necessary and then call the base class implementation.
 */
void AWorldAttractor::Spawned()
{
	Super::Spawned();

	if(GWorld && GWorld->GetWorldInfo())
	{
		GWorld->GetWorldInfo()->RegisterAttractor(this);
	}
}

void AWorldAttractor::PostLoad()
{
	Super::PostLoad();

	if(GWorld && GWorld->GetWorldInfo())
	{
		GWorld->GetWorldInfo()->RegisterAttractor(this);
	}
	else if(WorldInfo)
	{
		WorldInfo->RegisterAttractor(this);
	}
}

void AWorldAttractor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	if(GWorld && GWorld->GetWorldInfo())
	{
		GWorld->GetWorldInfo()->RegisterAttractor(this);
	}
}

void AWorldAttractor::SetZone( UBOOL bTest, UBOOL bForceRefresh )
{
	Super::SetZone(bTest, bForceRefresh);

	if(GWorld && GWorld->GetWorldInfo())
	{
		GWorld->GetWorldInfo()->RegisterAttractor(this);
	}
}

void AWorldAttractor::BeginDestroy()
{
	Super::BeginDestroy();

	if(GWorld && GWorld->GetWorldInfo())
	{
		GWorld->GetWorldInfo()->UnregisterAttractor(this);
	}
}

/** ticks the actor
* @return TRUE if the actor was ticked, FALSE if it was aborted (e.g. because it's in stasis)
*/
UBOOL AWorldAttractor::Tick( FLOAT DeltaTime, enum ELevelTick TickType )
{
	UBOOL ReturnValue = Super::Tick(DeltaTime, TickType);

	CurrentTime += DeltaTime;

	if(CurrentTime > LoopDuration)
	{
		CurrentTime -= LoopDuration;
	}

	return ReturnValue;
}
