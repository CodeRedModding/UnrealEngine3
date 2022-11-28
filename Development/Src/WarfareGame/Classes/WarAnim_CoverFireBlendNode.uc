class WarAnim_CoverFireBlendNode extends AnimNodeBlendPerBone
	native;

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};

/** Last time weapon was fired */
var transient float LastFireTime;

/** Amount of time to wait before blending out animation */
var float FireBlendOutDelay;

defaultproperties
{
	FireBlendOutDelay=0.15f
}
