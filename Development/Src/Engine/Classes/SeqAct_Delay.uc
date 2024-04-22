/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Delay extends SeqAct_Latent
	native(Sequence);

cpptext
{
	void Activated();
	UBOOL UpdateOp(FLOAT deltaTime);
	void DeActivated();
	virtual void PostLoad();

#if WITH_EDITOR
	virtual FString GetDisplayTitle() const;

	// Gives op a chance to add realtime debugging information (when enabled)
	virtual void GetRealtimeComments(TArray<FString> &OutComments);
#endif
};

/** Is this delay currently active? */
var const bool bDelayActive;

var const float DefaultDuration;

/** Default duration to use if no variables are linked */
var() float Duration<autocomment=true>;

/** When set triggering start again with restart the time without triggering the finished output, otherwise default behavior of not changing the initial delay time */
var() bool	bStartWillRestart;

/** Time at which this op was last updated, to prevent multiple updates per tick */
var const float LastUpdateTime;

/** Remaining time left on the duration */
var const float RemainingTime;

function Reset()
{
	ResetDelayActive();
}

native function ResetDelayActive();

defaultproperties
{
	ObjName="Delay"
	ObjCategory="Misc"

	InputLinks(0)=(LinkDesc="Start")
	InputLinks(1)=(LinkDesc="Stop")
	InputLinks(2)=(LinkDesc="Pause")

	DefaultDuration=1.f
	Duration=1.f

	VariableLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Float',LinkDesc="Duration",PropertyName=Duration)
}
