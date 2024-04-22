/**
 * Version of KActor that can be dynamically spawned and destroyed during gameplay
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 **/
class KActorSpawnable extends KActor
	native(Physics)
	notplaceable;


/** If this is true then the KActor will scale to zero before hiding self */
var bool bRecycleScaleToZero;

/** Whether or not we are scaling to zero (in C++ TickSpecial()) */
var protected bool bScalingToZero;

cpptext
{
	virtual void TickSpecial(FLOAT DeltaSeconds);
}


simulated function Initialize()
{
	bScalingToZero = FALSE;
	SetDrawScale( default.DrawScale );

	ClearTimer('Recycle');
	SetHidden(FALSE);
	StaticMeshComponent.SetHidden(FALSE);
	SetTickIsDisabled(false); 
	SetPhysics(PHYS_RigidBody);
	SetCollision(true, false);
}

/** This will reset the KActorSpawnable to its default state either first scaling to zero or by just hiding the object. **/
simulated function Recycle()
{
	if( bRecycleScaleToZero == TRUE )
	{
		bScalingToZero = TRUE;
	}
	else
	{
		RecycleInternal();
	}
}

/** This will reset the KActorSpawnable to its default state.  This is useful for pooling. **/
simulated event RecycleInternal()
{
	SetHidden(TRUE);
	StaticMeshComponent.SetHidden(TRUE);
	SetPhysics(PHYS_None);
	SetCollision(false, false);
	ClearTimer('Recycle');
	SetTickIsDisabled(true);
}



/** Used when the actor is pulled from a cache for use. */
native final function ResetComponents();

defaultproperties
{
	bNoDelete=FALSE
}
