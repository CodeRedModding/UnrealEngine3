//! @file SeqAct_SubstanceSetInputInt.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief Kismet sequence action to modify a Substance Air input

class SeqAct_SubstanceSetInputInt extends SequenceAction
	native(Sequence);

var() array<SubstanceAirGraphInstance>	GraphInstances;
var() Name								InputName;
var() array<int>						InputValue;

cpptext
{
	void Activated();
};

defaultproperties
{
	InputLinks(0)=(LinkDesc="Set")
	ObjName="Set Input (Int, Int[2], etc.)"
	ObjCategory="Substance"
}
