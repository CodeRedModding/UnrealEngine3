/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CurveEdPreset_CosWave extends CurveEdPresetBase
	native
	editinlinenew
	hidecategories(Object);

/** The frequency of the wave					*/
var()		float				Frequency;
/** The scale of the wave						*/
var()		float				Scale;
/** The offset of the wave						*/
var()		float				Offset;

/** Virtual function to get the user-readable name for the curve	*/
function string GetDisplayName()
{
	local string RetVal;

	RetVal = "CosWave";

	return RetVal;
}

/** Virtual function to verify the settings are valid */
function bool AreSettingsValid(bool bIsSaving)
{
	if ((Frequency <= 0.0) || (Scale == 0.0))
	{
		return false;
	}

	return true;
}

/** Virtual function to get the required KeyIn times*/
function bool GetRequiredKeyInTimes(out array<float> RequiredKeyInTimes)
{
	local bool bRetval;
	local int GenerateCount;
	local int PointIndex;
	local float	StepSize;
	local float	Freq;
	local float	SourceValue;

	bRetval = true;

	GenerateCount = 0;
	if (Frequency == 0)
	{
		Freq	= 1;
	}
	else
	{
		Freq	= Frequency;
	}

	GenerateCount	= (2 * Freq) + 1;

	RequiredKeyInTimes.Insert(0, GenerateCount);

	StepSize	= 1.0f / (GenerateCount - 1);
	SourceValue	= 0.0f;
	for (PointIndex = 0; PointIndex < GenerateCount; PointIndex++)
	{
		RequiredKeyInTimes[PointIndex]	= SourceValue;
		SourceValue += StepSize;
	}

	return bRetval;
}

/** Virtual function to generate curve								*/
function bool GenerateCurve(out array<float> RequiredKeyInTimes, out array<PresetGeneratedPoint> GeneratedPoints)
{
	local bool bRetval;
	local int GenerateCount;
	local int PointIndex;
	local float	Freq;
	local float	SourceValue;

	bRetval = true;

	if (Frequency == 0)
	{
		Freq	= 1;
	}
	else
	{
		Freq	= Frequency;
	}

	GenerateCount	= RequiredKeyInTimes.Length;
	GeneratedPoints.Insert(0, GenerateCount);

/***
	`Log("Generated Data for " $ GetDisplayName());
***/
	for (PointIndex = 0; PointIndex < GenerateCount; PointIndex++)
	{
		SourceValue	= RequiredKeyInTimes[PointIndex];
		GeneratedPoints[PointIndex].KeyIn			= SourceValue;
		GeneratedPoints[PointIndex].KeyOut			= cos(360.0 * DegToRad * Freq * SourceValue) * Scale + Offset;
		GeneratedPoints[PointIndex].TangentsValid	= false;
		GeneratedPoints[PointIndex].IntepMode		= CIM_CurveAuto;

/***
		`Log("    Key " $ PointIndex $ " - " $ SourceValue $ " - " $ GeneratedPoints[PointIndex].KeyOut);
***/
	}

	return bRetval;
}

/** */
cpptext
{
}

/** */
defaultproperties
{
	Frequency=1.0
	Scale=1.0
	Offset=0.0
}
