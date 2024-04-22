/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionFloatParticleParameter extends DistributionFloatParameterBase
	native(Particle)
	collapsecategories
	hidecategories(Object)
	editinlinenew;
	
cpptext
{
	virtual UBOOL GetParamValue(UObject* Data, FName ParamName, FLOAT& OutFloat);
}
