/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionVectorParameterBase extends DistributionVectorConstant
	abstract
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;
	
var()	name	ParameterName;
var()	vector	MinInput;
var()	vector	MaxInput;
var()	vector	MinOutput;
var()	vector	MaxOutput;
var()	DistributionFloatParameterBase.DistributionParamMode ParamModes[3];

cpptext
{
	virtual FVector GetValue(FLOAT F = 0.f, UObject* Data = NULL, INT Extreme = 0, class FRandomStream* InRandomStream = NULL);
	
	virtual UBOOL GetParamValue(UObject* Data, FName ParamName, FVector& OutVector) { return false; }

	virtual void GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);

	/**
	 * Return whether or not this distribution can be baked into a FRawDistribution lookup table
	 */
	virtual UBOOL CanBeBaked() const { return FALSE; }
}

defaultproperties
{
	MaxInput=(X=1.0,Y=1.0,Z=1.0)
	MaxOutput=(X=1.0,Y=1.0,Z=1.0)
}
