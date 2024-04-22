/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CurveEdPreset_LinearDecay extends CurveEdPresetBase
	native
	editinlinenew
	hidecategories(Object);

var()		float	StartDecay;
var()		float	StartValue;
var()		float	EndDecay;
var()		float	EndValue;

/** Virtual function to get the user-readable name for the curve	*/
function string GetDisplayName()
{
	local string RetVal;

	RetVal = "LinearDecay";

	return RetVal;
}

/** Virtual function to verify the settings are valid */
function bool AreSettingsValid(bool bIsSaving)
{
	if (StartDecay >= EndDecay)
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
	local int PointCount;

	bRetval = true;

	GenerateCount = 2;
	if (StartDecay > 0.0)
	{
		GenerateCount += 1;
	}

	if (EndDecay < 1.0)
	{
		GenerateCount += 1;
	}

	RequiredKeyInTimes.Insert(0, GenerateCount);

	RequiredKeyInTimes[PointCount]	= 0.0;
	PointCount++;

	if (StartDecay != 0.0)
	{
		RequiredKeyInTimes[PointCount]	= StartDecay;
		PointCount++;
	}

	RequiredKeyInTimes[PointCount]	= EndDecay;
	PointCount++;

	if (EndDecay < 1.0)
	{
		RequiredKeyInTimes[PointCount]	= 1.0;
		PointCount++;
	}

	return bRetval;
}

/** Virtual function to generate curve								*/
function bool GenerateCurve(out array<float> RequiredKeyInTimes, out array<PresetGeneratedPoint> GeneratedPoints)
{
	local bool bRetval;
	local int GenerateCount;
	local int PointCount;
	local float CurrentKeyIn;
	local float Difference;
	local float Alpha;

	bRetval = true;

	GenerateCount	= RequiredKeyInTimes.Length;
	GeneratedPoints.Insert(0, GenerateCount);

	Difference = EndDecay - StartDecay;

/***
	`Log("Generated Data for " $ GetDisplayName());
***/

	for (PointCount = 0; PointCount < RequiredKeyInTimes.Length; PointCount++)
	{
		CurrentKeyIn	= RequiredKeyInTimes[PointCount];

		GeneratedPoints[PointCount].KeyIn			= CurrentKeyIn;
		GeneratedPoints[PointCount].TangentsValid	= false;
		GeneratedPoints[PointCount].IntepMode		= CIM_CurveAuto;

		if (CurrentKeyIn < StartDecay)
		{
			GeneratedPoints[PointCount].KeyOut	= StartValue;
		}
		else
		if (CurrentKeyIn > EndDecay)
		{
			GeneratedPoints[PointCount].KeyOut	= EndValue;
		}
		else
		{
			Alpha = (CurrentKeyIn - StartDecay) / Difference;
			GeneratedPoints[PointCount].KeyOut	= Lerp(StartValue, EndValue, Alpha);
		}
/***
		`Log("    Key " $ PointCount $ " - " $ CurrentKeyIn $ " - " $ GeneratedPoints[PointCount].KeyOut);
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
	StartDecay=0.0
	StartValue=1.0
	EndDecay=1.0
	EndValue=0.0
}
