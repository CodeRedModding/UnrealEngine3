/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CurveEdPreset_UserSet extends CurveEdPresetBase
	native
	editinlinenew
	hidecategories(Object);

var()	CurveEdPresetCurve		UserCurve;

/** Virtual function to get the user-readable name for the curve	*/
function string GetDisplayName()
{
	local string RetVal;

	RetVal = "User-Set";

	return RetVal;
}

/** Virtual function to verify the settings are valid */
function bool AreSettingsValid(bool bIsSaving)
{
	if (bIsSaving)
	{
		if (UserCurve == None)
		{
			return false;
		}
	}
	else
	{
		if (UserCurve == None)
		{
			return false;
		}
	}

	return true;
}

/** Virtual function to get the required KeyIn times*/
function bool GetRequiredKeyInTimes(out array<float> RequiredKeyInTimes)
{
	local bool bRetval;
	local int GenerateCount;
	local int PointCount;

	bRetval	= true;

	if (UserCurve != None)
	{
		GenerateCount = UserCurve.Points.Length;

		if (GenerateCount > 0)
		{
			RequiredKeyInTimes.Insert(0, GenerateCount);

			for (PointCount = 0; PointCount < GenerateCount; PointCount++)
			{
				RequiredKeyInTimes[PointCount]	= UserCurve.Points[PointCount].KeyIn;
			}
		}
		else
		{
			bRetval = false;
		}
	}
	else
	{
		bRetval = false;
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

	bRetval = true;

	GenerateCount	= RequiredKeyInTimes.Length;
	GeneratedPoints.Insert(0, GenerateCount);

	for (PointCount = 0; PointCount < RequiredKeyInTimes.Length; PointCount++)
	{
		CurrentKeyIn	= RequiredKeyInTimes[PointCount];

		GeneratedPoints[PointCount].KeyIn			= CurrentKeyIn;
		GeneratedPoints[PointCount].TangentsValid	= false;
		GeneratedPoints[PointCount].IntepMode		= CIM_CurveAuto;
		GeneratedPoints[PointCount].KeyOut			= UserCurve.Points[PointCount].KeyOut;
		GeneratedPoints[PointCount].TangentIn		= UserCurve.Points[PointCount].TangentIn;
		GeneratedPoints[PointCount].TangentOut		= UserCurve.Points[PointCount].TangentOut;

		`Log("    Key " $ PointCount $ " - " $ CurrentKeyIn $ " - " $ GeneratedPoints[PointCount].KeyOut);
	}

	return bRetval;
}

/** Fill-in from a set curve															*/
function bool SetCurve(array<PresetGeneratedPoint> GeneratedPoints)
{
	local bool bRetval;

	bRetval = true;

	return bRetval;
}

/** */
function bool LoadUserSetPointFile()
{
	local bool bRetval;

	bRetval = true;

	return bRetval;
}

/** */
function bool SaveUserSetPointFile()
{
	local bool bRetval;

	bRetval = true;

	return bRetval;
}

/** */
cpptext
{
}

/** */
defaultproperties
{
}
