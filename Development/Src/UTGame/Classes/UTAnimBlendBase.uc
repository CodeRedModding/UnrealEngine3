class UTAnimBlendBase extends AnimNodeBlendList
	native;

/** How fast show a given child blend in. */

var(Animation) float BlendTime;

/** Also allow for Blend Overrides */

var(Animation) array<float> ChildBlendTimes;

event function float GetBlendTime(int ChildIndex, optional bool bGetDefault)
{
	if (bGetDefault || ChildIndex>=ChildBlendTimes.Length || ChildBlendTimes[ChildIndex]==0.0f)
		return BlendTime;
	else
		return ChildBlendTimes[ChildIndex];
}

defaultproperties
{
	BlendTime=0.25
}
