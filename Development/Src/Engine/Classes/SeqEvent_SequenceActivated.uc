/**
 * Activated once a sequence is activated by another operation.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqEvent_SequenceActivated extends SequenceEvent
	native(Sequence);

cpptext
{
#if WITH_EDITOR
protected:
	FString GetDisplayTitle() const;
#endif
public:
	virtual void OnCreated();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	UBOOL CheckActivateSimple();
}

/** Text label to use on the sequence input link */
var() string InputLabel;

defaultproperties
{
	ObjName="Sequence Activated"
	InputLabel="In"
	bPlayerOnly=false
	MaxTriggerCount=0
}
