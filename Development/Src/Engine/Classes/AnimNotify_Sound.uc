class AnimNotify_Sound extends AnimNotify
	native(Anim);

var()	SoundCue	SoundCue;
var()	bool		bFollowActor;

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
}

defaultproperties
{
	bFollowActor=true
}