class UTAnimNodeSequence extends AnimNodeSequence
	native;

var bool bAutoStart;
var bool bResetOnActivate;

cpptext
{
	virtual void OnBecomeActive();
}

event OnInit()
{
	super.OnInit();
	if (bAutoStart)
	{
		PlayAnim(bLooping,Rate);
	}
}

defaultproperties
{
	bAutoStart=false
}
