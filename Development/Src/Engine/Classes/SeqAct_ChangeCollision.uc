/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */


class SeqAct_ChangeCollision extends SequenceAction
	native(Sequence);

cpptext
{
	void UpdateObject()
	{
		Super::UpdateObject();
		if (bBlockActors)
		{
			CollisionType = COLLIDE_BlockAll;
		}
		else
		if (bCollideActors)
		{
			CollisionType = COLLIDE_TouchAll;
		}
		else
		{
			CollisionType = COLLIDE_NoCollision;
		}
	}
};

var() editconst const bool bCollideActors;
var() editconst const bool bBlockActors;
var() editconst const bool bIgnoreEncroachers;

var() Actor.ECollisionType CollisionType;

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
	return Super.GetObjClassVersion() + 4;
}

defaultproperties
{
	ObjName="Change Collision"
	ObjCategory="Actor"
}
