/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SeqAct_Deproject extends SequenceAction
	native;

/** The X location you wish to trace out from */
var() float ScreenX;

/** The Y location you wish to trace out from */
var() float ScreenY;

/** How far out should we trace */
var() float TraceDistance;

/** The object that was hit */
var object HitObject;

/** The location where the hit occured */
var vector HitLocation;

/** The hit normal */
var vector HitNormal;

cpptext
{
	void Activated();
};

defaultproperties
{
	ObjName="Deproject"
	ObjCategory="Level"

	VariableLinks(0)=(ExpectedType=class'SeqVar_float',LinkDesc="X",bWriteable=true,PropertyName=ScreenX)
	VariableLinks(1)=(ExpectedType=class'SeqVar_float',LinkDesc="Y",bWriteable=true,PropertyName=ScreenY)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Hit Object",bWriteable=true,PropertyName=HitObject)
	VariableLinks(3)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Hit Location",bWriteable=true,PropertyName=HitLocation)
	VariableLinks(4)=(ExpectedType=class'SeqVar_Vector',LinkDesc="Hit Normal",bWriteable=true,PropertyName=HitNormal)

	TraceDistance=20480

}
