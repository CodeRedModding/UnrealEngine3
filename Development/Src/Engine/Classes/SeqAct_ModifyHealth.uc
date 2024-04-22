/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_ModifyHealth extends SequenceAction
		native(Sequence);

cpptext
{
	void Activated();
	virtual void UpdateObject();
}

/** Type of damage to apply */
var() class<DamageType>		DamageType;

/** Amount of momentum to apply */
var() float		Momentum;

/** Change in health */
var() float		Amount<autocomment=true>;

/** Distance to Instigator within which to damage actors */
var() float		Radius;

/** If true, Amount will be healed */
var() bool		bHeal;

/** If true, health change will be radial */
var() bool		bRadial;

/** Whether amount should decay linearly based on distance from the target. */
var() bool		bFalloff;

/** Player that should take credit for the health change (Controller or Pawn) */
var Actor Instigator;

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
	return Super.GetObjClassVersion() + 1;
}

defaultproperties
{
	ObjName="Modify Health"
	ObjCategory="Actor"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Amount",PropertyName=Amount)
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Instigator",PropertyName=Instigator)
	Momentum=500.f
}