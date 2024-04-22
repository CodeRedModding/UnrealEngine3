/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class CurveEdPresetBase extends Object
	abstract
	native;

/** Virtual function to get the user-readable name for the curve	*/
function string GetDisplayName()
{
	local string RetVal;
	
	RetVal = "*** ERROR ***";
	
	return RetVal;
}

/** Virtual function to verify the settings are valid */
function bool AreSettingsValid(bool bIsSaving)
{
	return true;
}

/** Virtual function to get the required KeyIn times*/
function bool GetRequiredKeyInTimes(out array<float> RequiredKeyInTimes)
{
	return false;
}

/** Virtual function to generate curve								*/
function bool GenerateCurve(out array<float> RequiredKeyInTimes, out array<PresetGeneratedPoint> GeneratedPoints)
{
	local bool bRetval;

	bRetval = true;
	
	return bRetval;
}

/** Event to allow C++ to call GetDisplayName						*/
event FetchDisplayName(out string OutName)
{
	OutName = GetDisplayName();
}

/** */
event bool CheckAreSettingsValid(bool bIsSaving)
{
	return AreSettingsValid(bIsSaving);
}

/** */
event bool FetchRequiredKeyInTimes(out array<float> RequiredKeyInTimes)
{
	return GetRequiredKeyInTimes(RequiredKeyInTimes);
}

/** Event to allow C++ to call GenerateCurve						*/
event bool GenerateCurveData(out array<float> RequiredKeyInTimes, out array<PresetGeneratedPoint> GeneratedPoints)
{
	return GenerateCurve(RequiredKeyInTimes, GeneratedPoints);
}

/** */
cpptext
{
}

/** */
defaultproperties
{
}
