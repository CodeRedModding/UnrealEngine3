class UTAnimBlendByWeapon extends AnimNodeBlendPerBone
	native;

/** Is this weapon playing a looped anim */

var(Animation) bool	bLooping;				

/** Blend Times */

var(Animation) float BlendTime;

cpptext
{
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);
}

// Call to trigger the fire sequence.  It will blend to the fire animation AND restart the animation

event AnimFire(name FireSequence, bool bAutoFire, optional float AnimRate, optional float SpecialBlendTime)
{
	// Fix the rate

	if (AnimRate==0)
		AnimRate=1.0;

	if (SpecialBlendTime==0.0f)
		SpecialBlendtime = BlendTime;

	// Activate the child node

	SetBlendTarget(1,SpecialBlendtime);

	// Restart the sequence
	if ( AnimNodeSequence(Children[1].Anim) != none )
	{
		AnimNodeSequence(Children[1].Anim).SetAnim( FireSequence );
		AnimNodeSequence(Children[1].Anim).PlayAnim(bAutoFire,AnimRate);
	}

	bLooping = bAutoFire;
}

// Blends out the fire animation;

event AnimStopFire(optional float SpecialBlendTime)
{
	if (SpecialBlendTime==0.0f)
		SpecialBlendTime = BlendTime;

	SetBlendTarget(0,SpecialBlendTime);

	if ( AnimNodeSequence(Children[1].Anim) != none )
		AnimNodeSequence( Children[1].Anim ).StopAnim();

	bLooping = false;
}

defaultproperties
{
	BlendTime=0.15

	Children(0)=(Name="Not-Firing",Weight=1.0)
	Children(1)=(Name="Firing")
	bFixNumChildren=true



}
