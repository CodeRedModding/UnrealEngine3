class SeqAct_SetApexClothingParam extends SequenceAction
	native(Sequence);

var() bool bEnableApexClothingSimulation;

cpptext
{
	virtual void Activated();
};

defaultproperties
{
	ObjName="Set Apex Clothing Parameter"
	ObjCategory="Physics"
}

