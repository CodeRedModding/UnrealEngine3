/**
* A IsInObjectList Action is used to test whether an object is in the referenced
* object list or not.
*
* The default is to check for all objects in the list.  If the CheckForAllObjects is
* unchecked then if ANY of the objects attached to Object(s)ToTest are found, then we will
* set "In List" to hot
*
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_IsInObjectList extends SequenceAction
	native(Sequence);

/** Determines whether or not we check for ALL or ANY objects referenced via Object(s)ToTest **/
var() bool bCheckForAllObjects;

var private transient bool bObjectFound;

cpptext
{
	/**
	* When a IsInObjectList is Activated() it will look in the referenced
	* list and then determine if the referenced object is in it
	**/
	void Activated();

	/**
	* IsInObjectList determines which of its outputs should be
	* set to active
	**/
	void DeActivated();


private:
	/**
	* Helper functions to determine if objects are in the list for each of the cases
	**/
	UBOOL TestForAllObjectsInList();
	UBOOL TestForAnyObjectsInList();
}


defaultproperties
{
    bCheckForAllObjects=true


	ObjName="IsIn ObjectList"
	ObjCategory="Object List"
	ObjColor=(R=255,G=0,B=255,A=255)


    // all of the inputs / functionality this Action can do
	InputLinks(0)=(LinkDesc="Test if in List")

    // outputs that are set to hot depending
	OutputLinks(0)=(LinkDesc="In List")
	OutputLinks(1)=(LinkDesc="Not in List")


	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Object(s)ToTest",MinVars=1)

	// this is the LIST(S) object that we are going to check existence against
	VariableLinks(1)=(ExpectedType=class'SeqVar_ObjectList',LinkDesc="ObjectListVar",bWriteable=true)


}
