/*=============================================================================
	UnActor.h: AActor class inlines.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
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
inline UBOOL AActor::IsStaticBrush() const {return ((AActor *)this)->IsABrush() && bStatic && !((AActor *)this)->IsAVolume() && !((AActor *)this)->IsABrushShape(); }
inline UBOOL AActor::IsVolumeBrush() const {return ((AActor *)this)->IsAVolume();}
inline UBOOL AActor::IsBrushShape() const {return ((AActor *)this)->IsABrushShape();}
inline UBOOL AActor::IsEncroacher()  const {return bCollideActors && (Physics==PHYS_RigidBody || Physics==PHYS_Interpolating || bCollideAsEncroacher);}

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
	return Base;
}

/** Get Pawn from the given Actor **/
extern APawn * GetPawn(AActor *Actor);
extern USkeletalMeshComponent * GetSkeletalMeshComp( AActor * Actor );