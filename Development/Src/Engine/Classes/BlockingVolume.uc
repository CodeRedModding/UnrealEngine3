//=============================================================================
// BlockingVolume:  a bounding volume
// used to block certain classes of actors
// primary use is to provide collision for non-zero extent traces around static meshes
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class BlockingVolume extends Volume
	native
	placeable;

/** GameCameras ignore BlockingVolumes with bBlockCamera=false */
var() bool bBlockCamera;

cpptext
{
	UBOOL IgnoreBlockingBy( const AActor *Other ) const;
}

/**	Handling Toggle event from Kismet. */
simulated function OnToggle(SeqAct_Toggle Action)
{
	// Turn ON
	if (Action.InputLinks[0].bHasImpulse)
	{
		CollisionComponent.SetBlockRigidBody( TRUE );
	}
	// Turn OFF
	else if (Action.InputLinks[1].bHasImpulse)
	{
		CollisionComponent.SetBlockRigidBody( FALSE );
	}
	// Toggle
	else if (Action.InputLinks[2].bHasImpulse)
	{
		CollisionComponent.SetBlockRigidBody( !CollisionComponent.BlockRigidBody );
	}

	Super.OnToggle( Action );
}

defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=true
		BlockZeroExtent=false
		BlockNonZeroExtent=true
		BlockRigidBody=true
		bDisableAllRigidBody=false
		RBChannel=RBCC_BlockingVolume
	End Object

	bWorldGeometry=true
	bCollideActors=True
	bBlockActors=True
	bBlockCamera=true
	bMovable=false
}
