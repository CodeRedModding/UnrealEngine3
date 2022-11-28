class SeqEvent_Used extends SequenceEvent
	native(Sequence);

cpptext
{
	virtual UBOOL CheckActivate(AActor *inOriginator,AActor *inInstigator,UBOOL bTest=0);
}

/** if true, requires player to aim at trigger to be able to interact with it. False, simple radius check will be performed */
var() bool	bAimToInteract;

/** Max distance from instigator to allow interactions. */
var() float InteractDistance;

/** Text to display when looking at this event */
var() string InteractText;

/** Icon to display when looking at this event */
var() Texture2D InteractIcon;

defaultproperties
{
	bAimToInteract=true
	ObjName="Used"
	InteractDistance=96.f
	InteractText="Use"
	VariableLinks(1)=(ExpectedType=class'SeqVar_Float',LinkDesc="Distance",bWriteable=true)
}
