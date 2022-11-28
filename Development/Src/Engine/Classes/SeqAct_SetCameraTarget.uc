class SeqAct_SetCameraTarget extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

var transient Actor			CameraTarget;

defaultproperties
{
	ObjName="Set Camera Target"

	VariableLinks(1)=(ExpectedType=class'SeqVar_Object',LinkDesc="Cam Target")
}
