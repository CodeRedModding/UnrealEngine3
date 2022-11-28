class WarAnim_CoverSequenceNode extends AnimNodeSequence
	native;

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
}

/** Should we be performing an intro transition? */
var transient bool bIntroTransition;

/** Should we be performing an outro transition? */
var transient bool bOutroTransition;

/** Intro transition animation */
var() Name IntroAnimSeqName;

/** Looping idle animation to play once the transition is finished */
var() Name IdleAnimSeqName;

/** Outro transition animation */
var() Name OutroAnimSeqName;
