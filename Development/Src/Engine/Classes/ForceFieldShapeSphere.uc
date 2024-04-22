/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ForceFieldShapeSphere extends ForceFieldShape
	native(ForceField);

var DrawSphereComponent Shape;

event float GetRadius()
{
	return Shape.SphereRadius;
}

event FillBySphere(float Radius)
{
	Shape.SphereRadius = Radius;
}

event FillByBox(vector Extent)
{
	Shape.SphereRadius = vsize(Extent);
}

event FillByCapsule(float Height, float Radius)
{
	Shape.SphereRadius = Height/2 + Radius;
}

event FillByCylinder(float BottomRadius, float TopRadius, float Height, float HeightOffset)
{
	local float topDistance, bottomDistance, centerBelowTop, centerAboveBottom;
	centerBelowTop = Height/2 + HeightOffset;
	centerAboveBottom = Height/2 - HeightOffset;
	topDistance = Sqrt(TopRadius*TopRadius + centerBelowTop*centerBelowTop);
	bottomDistance = Sqrt(BottomRadius*BottomRadius + centerAboveBottom*centerAboveBottom);
	Shape.SphereRadius = FMax(topDistance, bottomDistance);
}

event PrimitiveComponent GetDrawComponent()
{
	return Shape;
}

cpptext
{
#if WITH_NOVODEX
	virtual class NxForceFieldShapeDesc * CreateNxDesc();
#endif
}

defaultproperties
{
	Begin Object Class=DrawSphereComponent Name=DrawSphere0
		SphereRadius=200.0
	End Object

	Shape = DrawSphere0
}
