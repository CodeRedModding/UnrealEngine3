/*=============================================================================
	UnInterpCurveEdSetup.cpp: Implementation of distribution classes.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by James Golding
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UInterpCurveEdSetup);

void UInterpCurveEdSetup::PostLoad()
{
	Super::PostLoad();

	// Remove any curve entries that do not refer to FCurveEdInterface-derived objects.
	for(INT i=0; i<Tabs.Num(); i++)
	{
		FCurveEdTab& Tab = Tabs(i);
		for(INT j=Tab.Curves.Num()-1; j>=0; j--)
		{
			FCurveEdInterface* EdInterface = GetCurveEdInterfacePointer( Tab.Curves(j) );
			if(!EdInterface)
			{
				Tab.Curves.Remove(j);
			}
		}
	}
}

FCurveEdInterface* UInterpCurveEdSetup::GetCurveEdInterfacePointer(const FCurveEdEntry& Entry)
{
	UDistributionFloat* FloatDist = Cast<UDistributionFloat>(Entry.CurveObject);
	if(FloatDist)
	{
		return FloatDist;
	}

	UDistributionVector* VectorDist = Cast<UDistributionVector>(Entry.CurveObject);
	if(VectorDist)
	{
		return VectorDist;
	}

	UInterpTrack* InterpTrack = Cast<UInterpTrack>(Entry.CurveObject);
	if(InterpTrack)
	{
		return InterpTrack;
	}

	return NULL;
}

/** Add a new curve property to the current tab. */
void UInterpCurveEdSetup::AddCurveToCurrentTab(UObject* InCurve, const FString& InCurveName, const FColor& InCurveColor, UBOOL bColorCurve)
{
	FCurveEdTab& Tab = Tabs(ActiveTab);

	// See if curve is already on tab. If so, do nothing.
	for( INT i=0; i<Tab.Curves.Num(); i++ )
	{
		if( Tab.Curves(i).CurveObject == InCurve  )
		{
			return;
		}
	}

	// Curve not there, so make new entry and record details.
	FCurveEdEntry* NewCurve = new(Tab.Curves) FCurveEdEntry();
	appMemzero(NewCurve, sizeof(FCurveEdEntry));

	NewCurve->CurveObject = InCurve;
	NewCurve->CurveName = InCurveName;
	NewCurve->CurveColor = InCurveColor;
	NewCurve->bColorCurve = bColorCurve;
}

/** Remove a partiuclar curve from all tabs. */
void UInterpCurveEdSetup::RemoveCurve(UObject* InCurve)
{
	for(INT i=0; i<Tabs.Num(); i++)
	{
		FCurveEdTab& Tab = Tabs(i);
		for(INT j=Tab.Curves.Num()-1; j>=0; j--)
		{			
			if(Tab.Curves(j).CurveObject == InCurve)
			{
				Tab.Curves.Remove(j);
			}
		}
	}
}
	
void UInterpCurveEdSetup::ReplaceCurve(UObject* RemoveCurve, UObject* AddCurve)
{
	check(RemoveCurve);
	check(AddCurve);

	for(INT i=0; i<Tabs.Num(); i++)
	{
		FCurveEdTab& Tab = Tabs(i);
		for(INT j=0; j<Tab.Curves.Num(); j++)
		{			
			if(Tab.Curves(j).CurveObject == RemoveCurve)
			{
				Tab.Curves(j).CurveObject = AddCurve;
			}
		}
	}
}


/** Create a new tab in the CurveEdSetup. */
void UInterpCurveEdSetup::CreateNewTab(const FString& InTabName)
{

}

/** Look through CurveEdSetup and see if any properties of selected object is being shown. */
UBOOL UInterpCurveEdSetup::ShowingCurve(UObject* InCurve)
{
	for(INT i=0; i<Tabs.Num(); i++)
	{
		FCurveEdTab& Tab = Tabs(i);

		for(INT j=0; j<Tab.Curves.Num(); j++)
		{
			if(Tab.Curves(j).CurveObject == InCurve)
				return true;
		}
	}

	return false;
}

/** Remove all tabs and re-add the 'default' one */
void UInterpCurveEdSetup::ResetTabs()
{
	Tabs.Empty();

	FCurveEdTab Tab;
	
	appMemzero(&Tab, sizeof(Tab));
	
	Tab.TabName			= TEXT("Default");
	Tab.ViewStartInput	=  0.0f;
	Tab.ViewEndInput	=  1.0f;
	Tab.ViewStartOutput = -1.0;
	Tab.ViewEndOutput	=  1.0;

	Tabs.AddItem(Tab);
}
