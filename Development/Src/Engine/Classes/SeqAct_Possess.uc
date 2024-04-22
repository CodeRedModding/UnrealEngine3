/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Possess extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

var transient	Pawn	PawnToPossess;

/** true if should kill old pawn */
var()			bool	bKillOldPawn;

/** Try to leave vehicle if manning one */
var()			bool	bTryToLeaveVehicle;

defaultproperties
{
	ObjName="Possess Pawn"
	ObjCategory="Pawn"
	bTryToLeaveVehicle=TRUE

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Pawn Target")
}
