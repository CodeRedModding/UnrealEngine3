/*=============================================================================
	UnGenericDlgOptions.cpp: Option classes for generic dialogs
	Copyright 1997-2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "UnrealEd.h"

/*------------------------------------------------------------------------------
	UOptionsProxy
------------------------------------------------------------------------------*/

// Constructors.
UOptionsProxy::UOptionsProxy()
{}

void UOptionsProxy::InitFields()
{
}
void UOptionsProxy::Destroy()
{
	UObject::Destroy();
}
void UOptionsProxy::StaticConstructor()
{
}
IMPLEMENT_CLASS(UOptionsProxy);

/*------------------------------------------------------------------------------
	UOptions2DShaper
------------------------------------------------------------------------------*/
UOptions2DShaper::UOptions2DShaper()
{}

void UOptions2DShaper::InitFields()
{
	new(GetClass(),TEXT("Axis"), RF_Public)UByteProperty(CPP_PROPERTY(Axis), TEXT(""), CPF_Edit, AxisEnum );

}

void UOptions2DShaper::StaticConstructor()
{
	AxisEnum = new( GetClass(), TEXT("Axis") )UEnum( NULL );
	new(AxisEnum->Names)FName( TEXT("AXIS_X") );
	new(AxisEnum->Names)FName( TEXT("AXIS_Y") );
	new(AxisEnum->Names)FName( TEXT("AXIS_Z") );

	Axis = AXIS_Y;

}
IMPLEMENT_CLASS(UOptions2DShaper);

/*------------------------------------------------------------------------------
	UOptions2DShaperSheet
------------------------------------------------------------------------------*/

UOptions2DShaperSheet::UOptions2DShaperSheet()
{}

void UOptions2DShaperSheet::InitFields()
{
	UOptions2DShaper::InitFields();
}

void UOptions2DShaperSheet::StaticConstructor()
{
}
IMPLEMENT_CLASS(UOptions2DShaperSheet);

/*------------------------------------------------------------------------------
	UOptions2DShaperExtrude
------------------------------------------------------------------------------*/
UOptions2DShaperExtrude::UOptions2DShaperExtrude()
{}

void UOptions2DShaperExtrude::InitFields()
{
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("Depth"), RF_Public)UIntProperty(CPP_PROPERTY(Depth), TEXT(""), CPF_Edit );
}

void UOptions2DShaperExtrude::StaticConstructor()
{
	Depth = 256;
}
IMPLEMENT_CLASS(UOptions2DShaperExtrude);

/*------------------------------------------------------------------------------
	UOptions2DShaperExtrudeToPoint
------------------------------------------------------------------------------*/
UOptions2DShaperExtrudeToPoint::UOptions2DShaperExtrudeToPoint()
{}

void UOptions2DShaperExtrudeToPoint::InitFields()
{
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("Depth"), RF_Public)UIntProperty(CPP_PROPERTY(Depth), TEXT(""), CPF_Edit );
}

void UOptions2DShaperExtrudeToPoint::StaticConstructor()
{
	Depth = 256;
}
IMPLEMENT_CLASS(UOptions2DShaperExtrudeToPoint);

/*------------------------------------------------------------------------------
	UOptions2DShaperExtrudeToBevel
------------------------------------------------------------------------------*/
UOptions2DShaperExtrudeToBevel::UOptions2DShaperExtrudeToBevel()
{}

void UOptions2DShaperExtrudeToBevel::InitFields()
{
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("Height"), RF_Public)UIntProperty(CPP_PROPERTY(Height), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("CapHeight"), RF_Public)UIntProperty(CPP_PROPERTY(CapHeight), TEXT(""), CPF_Edit );
}

void UOptions2DShaperExtrudeToBevel::StaticConstructor()
{
	Height = 128;
	CapHeight = 32;
}
IMPLEMENT_CLASS(UOptions2DShaperExtrudeToBevel);

/*------------------------------------------------------------------------------
	UOptions2DShaperRevolve
------------------------------------------------------------------------------*/
UOptions2DShaperRevolve::UOptions2DShaperRevolve()
{}

void UOptions2DShaperRevolve::InitFields()
{
	UOptions2DShaper::InitFields();
	new(GetClass(),TEXT("SidesPer360"), RF_Public)UIntProperty(CPP_PROPERTY(SidesPer360), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Sides"), RF_Public)UIntProperty(CPP_PROPERTY(Sides), TEXT(""), CPF_Edit );
}

void UOptions2DShaperRevolve::StaticConstructor()
{
	SidesPer360 = 12;
	Sides = 3;
}
IMPLEMENT_CLASS(UOptions2DShaperRevolve);

/*------------------------------------------------------------------------------
	UOptions2DShaperBezierDetail
------------------------------------------------------------------------------*/

UOptions2DShaperBezierDetail::UOptions2DShaperBezierDetail()
{}

void UOptions2DShaperBezierDetail::InitFields()
{
	new(GetClass(),TEXT("DetailLevel"), RF_Public)UIntProperty(CPP_PROPERTY(DetailLevel), TEXT(""), CPF_Edit );
}

void UOptions2DShaperBezierDetail::StaticConstructor()
{
	DetailLevel = 10;
}
IMPLEMENT_CLASS(UOptions2DShaperBezierDetail);

/*------------------------------------------------------------------------------
	UOptionsDupObject
------------------------------------------------------------------------------*/
UOptionsDupObject::UOptionsDupObject()
{}

void UOptionsDupObject::InitFields()
{
	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(Package), TEXT(""), CPF_Edit );

	new(GetClass()->HideCategories) FName(TEXT("Object"));
}

void UOptionsDupObject::StaticConstructor()
{
	Package = TEXT("MyLevel");
	Name = TEXT("NewObject");

}
IMPLEMENT_CLASS(UOptionsDupObject);

/*------------------------------------------------------------------------------
	UOptionsNewClassFromSel
------------------------------------------------------------------------------*/
UOptionsNewClassFromSel::UOptionsNewClassFromSel()
{}

void UOptionsNewClassFromSel::InitFields()
{
	new(GetClass(),TEXT("Name"), RF_Public)UStrProperty(CPP_PROPERTY(Name), TEXT(""), CPF_Edit );
	new(GetClass(),TEXT("Package"), RF_Public)UStrProperty(CPP_PROPERTY(Package), TEXT(""), CPF_Edit );
}

void UOptionsNewClassFromSel::StaticConstructor()
{
	Package = TEXT("MyLevel");
	Name = TEXT("MyClass");

}
IMPLEMENT_CLASS(UOptionsNewClassFromSel);

/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/