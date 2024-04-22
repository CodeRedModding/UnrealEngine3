/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ForceFieldShape extends Object
	native(ForceField)
	editinlinenew
	abstract;

event FillBySphere(float Radius);
event FillByBox(vector Dimension);
event FillByCapsule(float Height, float Radius);
event FillByCylinder(float BottomRadius, float TopRadius, float Height, float HeightOffset);

event PrimitiveComponent GetDrawComponent();

cpptext
{
#if WITH_NOVODEX
	virtual class NxForceFieldShapeDesc * CreateNxDesc(){ return NULL; }
#endif
}

defaultproperties
{
}
