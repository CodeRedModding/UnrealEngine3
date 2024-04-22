//! @file SeqAct_SubstanceSetInputFloat.uc
//! @author Antoine Gonzalez - Allegorithmic
//! @copyright Allegorithmic. All rights reserved.
//!
//! @brief Kismet sequence action to modify a Substance Air input

class SeqAct_SubstanceSetInputFloat extends SequenceAction
	native(Sequence);

var() array<SubstanceAirGraphInstance>	GraphInstances;
var() Name								InputName;
var() array<float>						InputValue;

cpptext
{
	void Activated();
};

defaultproperties
{
	InputLinks(0)=(LinkDesc="Set")
	ObjName="Set Input (Float, Float[2], etc.)"
	ObjCategory="Substance"
}
