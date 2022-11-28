class SeqAct_PlaySound extends SequenceAction
	native(Sequence);

cpptext
{
	void Activated();
};

var() SoundCue		PlaySound;

defaultproperties
{
	ObjName="Play Sound"
}
