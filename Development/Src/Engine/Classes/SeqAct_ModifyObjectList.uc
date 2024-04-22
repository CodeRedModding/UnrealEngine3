/**
 * A ModifyObjectList Action is used to manipulate an ObjectListVar or a set
 * of ObjectListVars.
 *
 * Basically, a ModifyObjectList Action  should be thought of as the operations
 * that can be done to a set of ObjectLists.  You have the Action point to a set of
 * ObjectLists and then you use the normal kismet firing of events to
 * cause the Action to affect the Lists.
 *
 *
 * The action provides a number of actions:
 *
 * Add:  This will add objects that are being referenced to the List(s)
 * Remove:  This will remove objects that are being referenced from the List(s)
 * Empty:  This will remove all objects in the List(s)
 *
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * @todo - this should be a conditional
 */
class SeqAct_ModifyObjectList extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	/**
	* When a ModifyObjectList is Activated() it may do a number of things.
	* In each of those cases we make use of a helper function.
	**/
	void Activated();

private:
	void ActivatedAddRemove();
	void ActivateAddRemove_Helper( INT LinkNum );

public:
	/**
	* SeqAct_ModifyObjectList determines which of its outputs should be
	* set to active
	**/
	void DeActivated();

}

var() editconst int ListEntriesCount;

defaultproperties
{

	ObjName="Modify ObjectList"
	ObjCategory="Object List"
	ObjColor=(R=255,G=0,B=255,A=255)


    // all of the inputs / functionality this Action can do
	InputLinks(0)=(LinkDesc="Add To List")
	InputLinks(1)=(LinkDesc="Remove From List")
	InputLinks(2)=(LinkDesc="Empty List")

	VariableLinks.Empty

	// This is the set of of objects that are used in add, remove actions
	// objects referenced by these kismet objects will be inserted into
	// the list this Action points to
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="ObjectRef",MinVars=1)


	// this is the LIST(S) object that we are going to do stuff too (i.e. add, remove, empty, call actions on all of the objects)
	VariableLinks(1)=(ExpectedType=class'SeqVar_ObjectList',LinkDesc="ObjectListVar",bWriteable=true)


	// I guess we need to push out to a variable the number of spawns?
	VariableLinks(2)=(ExpectedType=class'SeqVar_Int',LinkDesc="ListEntriesCount",bWriteable=true,PropertyName=ListEntriesCount)

}
