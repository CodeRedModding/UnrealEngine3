/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class DistributionFloatParameterBase extends DistributionFloatConstant
	abstract
	native
	collapsecategories
	hidecategories(Object)
	editinlinenew;
	
var()	name	ParameterName;
var()	float	MinInput;
var()	float	MaxInput;
var()	float	MinOutput;
var()	float	MaxOutput;

enum DistributionParamMode
{
	DPM_Normal,
	DPM_Abs,
	DPM_Direct
};

var()	DistributionParamMode	ParamMode;

cpptext
{
	virtual FLOAT GetValue( FLOAT F = 0.f, UObject* Data = NULL, class FRandomStream* InRandomStream = NULL );
	
	virtual UBOOL GetParamValue(UObject* Data, FName ParamName, FLOAT& OutFloat) { return false; }

	virtual void GetOutRange(FLOAT& MinOut, FLOAT& MaxOut);

	/**
	 * Return whether or not this distribution can be baked into a FRawDistribution lookup table
	 */
	virtual UBOOL CanBeBaked() const { return FALSE; }
}

defaultproperties
{
	MaxInput=1.0
	MaxOutput=1.0
}
