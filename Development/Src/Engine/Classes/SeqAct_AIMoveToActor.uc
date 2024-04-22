/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_AIMoveToActor extends SeqAct_Latent
	native(Sequence);

cpptext
{
	virtual UBOOL UpdateOp(FLOAT deltaTime);
	virtual void  Activated();
}

/** Should this move be interruptable? */
var() bool bInterruptable;

/** Should the AI pick the closest destination? */
var() bool bPickClosest;

/** List of destinations to pick from */
var() array<Actor> Destination;

/** Controls the max speed of the AI while moving */
var() float MovementSpeedModifier;

var() Actor LookAt;

/** Last destination chosen by an AI */
var transient int LastDestinationChoice;

/**
 * Return the version number for this class.  Child classes should increment this method by calling Super then adding
 * a individual class version to the result.  When a class is first created, the number should be 0; each time one of the
 * link arrays is modified (VariableLinks, OutputLinks, InputLinks, etc.), the number that is added to the result of
 * Super.GetObjClassVersion() should be incremented by 1.
 *
 * @return	the version number for this specific class.
 */
static event int GetObjClassVersion()
{
	return Super.GetObjClassVersion() + 2;
}

function Actor PickDestination(Actor Requestor)
{
	local float Dist, BestDist;
	local Actor Dest, BestDest;
	if (bPickClosest)
	{
		// just pick the closest one
		foreach Destination(Dest)
		{
			Dist = VSize(Dest.Location - Requestor.Location);
			if (BestDest == None || Dist < BestDist)
			{
				BestDest = Dest;
				BestDist = Dist;
			}
		}
		return BestDest;
	}
	else
	{
		// otherwise cycle through possible destinations
		if (LastDestinationChoice < 0 || LastDestinationChoice >= Destination.Length)
		{
			LastDestinationChoice = 0;
		}
		return Destination[LastDestinationChoice++];
	}
}

defaultproperties
{
	ObjName="Move To Actor"
	ObjCategory="AI"
	ObjRemoveInProject(0)="Gear"

	OutputLinks(2)=(LinkDesc="Out")

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Destination",PropertyName=Destination)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Look At",PropertyName=LookAt)

	MovementSpeedModifier=1.f
}
