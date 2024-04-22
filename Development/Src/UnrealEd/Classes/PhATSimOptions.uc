/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class PhATSimOptions extends Object	
	hidecategories(Object)
	config(EditorUserSettings)
	native;	

/** AnimSet used for playing animations . */
var(Anim)			transient AnimSet	PreviewAnimSet;

/** Lets you manually control the physics/animation */
var(Anim)			transient float		PhysicsBlend;

/** Whether we should just show animation until poked, then snap to physics, wait PokePauseTime, then blend back to animation over PokeBlendTime.  */
var(Anim)			transient bool		bBlendOnPoke;

/** Time between poking ragdoll and starting to blend back. */
var(Anim)			config float		PokePauseTime;

/** Time taken to blend from physics to animation. */
var(Anim)			config float		PokeBlendTime;

/** Overall spring scaling applied to all motors. */
var(Anim)			transient float		AngularSpringScale;

/** Overall damping scaling applied to all motors. */
var(Anim)			transient float		AngularDampingScale;

var(Simulation)		config float		SimSpeed;
var(Simulation)		config bool			bDrawContacts;
var(Simulation)		config float		FloorGap;
var(Simulation)		config float		GravScale;

var(MouseSpring)	config float		HandleLinearDamping;
var(MouseSpring)	config float		HandleLinearStiffness;
var(MouseSpring)	config float		HandleAngularDamping;
var(MouseSpring)	config float		HandleAngularStiffness;

var(Poking)			config float		PokeStrength;

var(Lighting)		config float		SkyBrightness;
var(Lighting)		config float		Brightness;

var(Snap)			config float		AngularSnap;
var(Snap)			config float		LinearSnap;

var(Advanced)		config bool			bPromptOnBoneDelete;
var(Advanced)		config bool			bShowConstraintsAsPoints;
var(Advanced)		config bool			bShowNamesInHierarchy;

/** Controls how large constraints are drawn in PhAT */
var(Drawing)		config float		ConstraintDrawSize;

defaultproperties
{
	PhysicsBlend=1.0
	AngularSpringScale=1.0
	AngularDampingScale=1.0
}
