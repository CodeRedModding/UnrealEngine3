class AnimNotify_Scripted extends AnimNotify
	native(Anim)
	abstract;

event Notify( Actor Owner, AnimNodeSequence AnimSeqInstigator );

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
}