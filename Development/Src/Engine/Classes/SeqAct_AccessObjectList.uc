/**
 * An AccessObjectList Action is used to access the contents of an ObjectListVar.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_AccessObjectList extends SeqAct_SetSequenceVariable
	native(Sequence);

cpptext
{
	/**
	* When a AccessObjectList is Activated() it may do a number of things.
	* In each of those cases we make use of a helper function.
	**/
	void Activated();

public:
	/**
	* SeqAct_AccessObjectList determines which of its outputs should be
	* set to active
	**/
	void DeActivated();

}

var() editconst Object OutputObject;
var() int ObjectIndex;

defaultproperties
{

	ObjName="Access ObjectList"
	ObjCategory="Object List"
	ObjColor=(R=255,G=0,B=255,A=255)

    // all of the inputs / functionality this Action can do
	InputLinks(0)=(LinkDesc="Random")
	InputLinks(1)=(LinkDesc="First")
	InputLinks(2)=(LinkDesc="Last")
	InputLinks(3)=(LinkDesc="At Index")

	VariableLinks.Empty

	// this is the LIST(S) object that we are going to do stuff too (i.e. add, remove, empty, call actions on all of the objects)
	VariableLinks(0)=(ExpectedType=class'SeqVar_ObjectList',LinkDesc="Object List",bWriteable=false,MinVars=1,MaxVars=1)

	// 
	VariableLinks(1)=(ExpectedType=class'SeqVar_Int',LinkDesc="Index",bWriteable=FALSE,PropertyName=ObjectIndex)

	// 
	VariableLinks(2)=(ExpectedType=class'SeqVar_Object',LinkDesc="Output Object",bWriteable=true,PropertyName=OutputObject)
}
