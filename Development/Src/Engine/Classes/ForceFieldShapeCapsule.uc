/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class ForceFieldShapeCapsule extends ForceFieldShape
	native(ForceField)
	;

var DrawCapsuleComponent Shape;

// UE3 capsule is x up by default. The fill... functions are not right because of the direction. Todo.
event float GetHeight()
{
	return Shape.CapsuleHeight;
}

event float GetRadius()
{
	return Shape.CapsuleRadius;
}

event FillBySphere(float Radius)
{
	Shape.CapsuleRadius = Radius;
	Shape.CapsuleHeight = 0;
}

event FillByBox(vector Extent)
{
	Shape.CapsuleRadius = Sqrt(Extent.X*Extent.X + Extent.Y*Extent.Y);
	Shape.CapsuleHeight = Extent.Z*2;
}

event FillByCapsule(float Height, float Radius)
{
	Shape.CapsuleHeight = Height;
	Shape.CapsuleRadius = Radius;
}

event FillByCylinder(float BottomRadius, float TopRadius, float Height, float HeightOffset)
{
	Shape.CapsuleRadius = FMax(BottomRadius, TopRadius);
	Shape.CapsuleHeight = Height;
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
	Begin Object Class=DrawCapsuleComponent Name=DrawCapsule0
		CapsuleRadius=200.0
		CapsuleHeight=200.0
		Rotation=(Pitch=0, Yaw=0, Roll=16384)//make it z upwards like cylinder
	End Object

	Shape = DrawCapsule0
}
