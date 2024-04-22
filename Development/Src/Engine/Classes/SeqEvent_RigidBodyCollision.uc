/**
 * Activated when an receives the OnRigidBodyCollision notification from the physics system.
 * Originator: the actor that was just sitting there
 * Instigator: the actor that did the colliding
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_RigidBodyCollision extends SequenceEvent
	native(Sequence);

cpptext
{
	void CheckRBCollisionActivate( const FRigidBodyCollisionInfo& OriginatorInfo, const FRigidBodyCollisionInfo& InstigatorInfo1,
					const TArray<FRigidBodyContactInfo>& ContactInfos, FLOAT VelMag );
}

/** Minimum impact speed (along contact normal) for this event to fire. */
var()	float	MinCollisionVelocity;

defaultproperties
{
	ObjName="Rigid Body Collision"
	ObjCategory="Physics"
	bPlayerOnly=false
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="ImpactVelocity",bWriteable=true)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Vector',LinkDesc="ImpactLocation",bWriteable=true)
}
