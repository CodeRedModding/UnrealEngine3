/*=============================================================================
	UnActor.h: AActor class inlines.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
        * Aug 30, 1996: Mark added PLevel
		* Oct 19, 1996: Tim redesigned to eliminate redundency
=============================================================================*/

/*-----------------------------------------------------------------------------
	FActorLink.
-----------------------------------------------------------------------------*/

//
// Linked list of actors.
//
struct FActorLink
{
	// Variables.
	AActor*     Actor;
	FActorLink* Next;

	// Functions.
	FActorLink( AActor* InActor, FActorLink* InNext )
	: Actor(InActor), Next(InNext)
	{}
};

/*-----------------------------------------------------------------------------
	AActor inlines.
-----------------------------------------------------------------------------*/

//
// Brush checking.
//
inline UBOOL AActor::IsBrush()       const {return ((AActor *)this)->IsABrush();}
inline UBOOL AActor::IsStaticBrush() const {return ((AActor *)this)->IsABrush() && bStatic && !((AActor *)this)->IsAVolume(); }
inline UBOOL AActor::IsVolumeBrush() const {return ((AActor *)this)->IsAVolume();}
inline UBOOL AActor::IsEncroacher()  const {return bCollideActors && 
											(Physics==PHYS_Articulated || Physics==PHYS_RigidBody || Physics==PHYS_Interpolating);}
inline UBOOL AActor::IsShapeBrush() const
{
	return ((AActor *)this)->IsAShape();
}

//
// See if this actor is owned by TestOwner.
//
inline UBOOL AActor::IsOwnedBy( const AActor* TestOwner ) const
{
	for( const AActor* Arg=this; Arg; Arg=Arg->Owner )
	{
		if( Arg == TestOwner )
			return 1;
	}
	return 0;
}

//
// Get the top-level owner of an actor.
//
inline AActor* AActor::GetTopOwner()
{
	AActor* Top;
	for( Top=this; Top->Owner; Top=Top->Owner );
	return Top;
}


//
// See if this actor is in the specified zone.
//
inline UBOOL AActor::IsInZone( const AZoneInfo* TestZone ) const
{
	return Region.Zone!=Level ? Region.Zone==TestZone : 1;
}

//
// Return whether this actor's movement is based on another actor.
//
inline UBOOL AActor::IsBasedOn( const AActor* Other ) const
{
	for( const AActor* Test=this; Test!=NULL; Test=Test->Base )
		if( Test == Other )
			return 1;
	return 0;
}

inline AActor* AActor::GetBase() const
{
	if(Base)
	{
		return Base;
	}
	else if(AttachTag != NAME_None)
	{
		ULevel* Level = GetLevel();
		for(INT i=0; i<Level->Actors.Num(); i++)
		{
			AActor* TestActor = Level->Actors(i);
			if( TestActor && (TestActor->Tag == AttachTag || TestActor->GetFName() == AttachTag) )
			{
				return TestActor;
			}
		}

		return NULL;
	}
	else
	{
		return NULL;
	}
}

//
// Return the level of an actor.
//
inline class ULevel* AActor::GetLevel() const
{
	return XLevel;
}

/*-----------------------------------------------------------------------------
	AActor audio.
-----------------------------------------------------------------------------*/

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

