/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_SetParticleSysParam extends SequenceAction;

var() editinline array<ParticleSystemComponent.ParticleSysParam>	InstanceParameters;

/** Should ScalarValue override any entries to InstanceParameters? */
var() bool bOverrideScalar;

/** Override scalar value */
var() float ScalarValue;

defaultproperties
{
	ObjName="Set Particle Param"
	ObjCategory="Particles"

	bOverrideScalar=TRUE

	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Scalar Value",PropertyName=ScalarValue)
}
