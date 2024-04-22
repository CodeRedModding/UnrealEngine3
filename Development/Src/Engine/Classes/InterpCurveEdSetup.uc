/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpCurveEdSetup extends Object
	native;

// Information about a particule curve being viewed.
// Property could be an FInterpCurve, a DistributionFloat or a DistributionVector
struct native CurveEdEntry
{
	var	Object	CurveObject;

	var color	CurveColor;
	var string	CurveName;

	var int		bHideCurve;
	var int		bColorCurve;
	var int		bFloatingPointColorCurve;
	var int		bClamp;
	var float	ClampLow;
	var float	ClampHigh;
};

struct native CurveEdTab
{
	var string					TabName;

	var array<CurveEdEntry>		Curves;

	// Remember the view setting for each tab.
	var float					ViewStartInput;
	var float					ViewEndInput;
	var float					ViewStartOutput;
	var float					ViewEndOutput;
};


var array<CurveEdTab>			Tabs;
var int							ActiveTab;

cpptext
{
	// UObject interface
	void PostLoad();
	void Serialize(FArchive& Ar);
	
	// InterpCurveEdSetup interface
	static FCurveEdInterface* GetCurveEdInterfacePointer(const FCurveEdEntry& Entry);
	void AddCurveToCurrentTab(UObject* InCurve, const FString& CurveName, const FColor& CurveColor, 
			UBOOL bInColorCurve=false, UBOOL bInFloatingPointColor=false, UBOOL bInClamp=false,
			FLOAT InClampLow=0.f, FLOAT InClampHigh=0.f);
	void RemoveCurve(UObject* InCurve);
	void ReplaceCurve(UObject* RemoveCurve, UObject* AddCurve);
	void CreateNewTab(const FString& InTabName);
	void RemoveTab(const FString& InTabName);
	UBOOL ShowingCurve(UObject* InCurve);

	void ChangeCurveColor(UObject* InCurve, const FColor& CurveColor);
	void ChangeCurveName(UObject* InCurve, const FString& NewCurveName);

	void ResetTabs();
}

defaultproperties
{
	Tabs(0)=(TabName="Default",ViewStartInput=0.0,ViewEndInput=1.0,ViewStartOutput=-1.0,ViewEndOutput=1.0)
}
