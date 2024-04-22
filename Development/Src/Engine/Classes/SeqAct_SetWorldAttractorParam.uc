/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_SetWorldAttractorParam extends SequenceAction
	native(Sequence)
	dependson(WorldAttractor);

/**
 *  Array of all the attractors to apply these settings to.
 */
var() array<WorldAttractor> Attractor;

var bool bEnabledField;
var bool bFalloffTypeField;
var bool bFalloffExponentField;
var bool bRangeField;
var bool bStrengthField;

/**
 *  TRUE if the attractor should be enabled, FALSE disables the attractor.
 */
var() bool bEnabled<EditCondition=bEnabledField>;

/**
 *  Type of falloff.
 */
var() EWorldAttractorFalloffType FalloffType<EditCondition=bFalloffTypeField>;

/**
 *  Optional falloff exponent for FOFF_Exponent type.
 */
var() rawdistributionfloat FalloffExponent<EditCondition=bFalloffExponentField>;

/**
 *  Range of the attractor.
 */
var() rawdistributionfloat Range<EditCondition=bRangeField>;

/**
 *  Strength of the attraction over time.
 */
var() rawdistributionfloat Strength<EditCondition=bStrengthField>;

defaultproperties
{
	ObjName="Set World Attractor Param"
	ObjCategory="Attractor"

	bEnabledField=false;
	bFalloffTypeField=false;
	bFalloffExponentField=false;
	bRangeField=false;
	bStrengthField=false;

	Begin Object Class=DistributionFloatConstant Name=DistributionFalloffExponent
	End Object
	FalloffExponent=(Distribution=DistributionFalloffExponent)

	Begin Object Class=DistributionFloatConstant Name=DistributionStrength
	End Object
	Strength=(Distribution=DistributionStrength)

	Begin Object Class=DistributionFloatConstant Name=DistributionRange
	End Object
	Range=(Distribution=DistributionRange)

	FalloffType=FOFF_Constant;

	bEnabled=true;
}
