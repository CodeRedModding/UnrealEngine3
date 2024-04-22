/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class NxRadialCustomForceField extends NxRadialForceField
	native(ForceField)
	placeable;


/** */
var()	interp float	SelfRotationStrength;

/** custom force field kernel */
var const native transient pointer		Kernel{class NxForceFieldKernelRadial};

cpptext
{
	virtual void DefineForceFunction(FPointer ForceFieldDesc);
	virtual void InitRBPhys();
	virtual void TermRBPhys(FRBPhysScene* Scene);
}


defaultproperties
{
}
