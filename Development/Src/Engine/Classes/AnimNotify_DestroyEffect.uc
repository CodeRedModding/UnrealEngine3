class AnimNotify_DestroyEffect extends AnimNotify
	native(Anim);

var() name DestroyTag;
var() bool bExpireParticles;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
}

defaultproperties
{
	bExpireParticles=True
}