class AnimNotify_Script extends AnimNotify
	native(Anim);

var() name NotifyName;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
}