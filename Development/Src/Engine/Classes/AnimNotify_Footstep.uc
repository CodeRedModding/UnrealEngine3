class AnimNotify_Footstep extends AnimNotify
	native(Anim);

var() int FootDown;  // 0=left 1=right.

cpptext
{
	// AnimNotify interface.
	virtual void Notify( class UAnimNodeSequence* NodeSeq );
}

defaultproperties
{
	 FootDown=0;
}