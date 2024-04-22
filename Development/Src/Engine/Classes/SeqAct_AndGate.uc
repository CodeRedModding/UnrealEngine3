/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_AndGate extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void Initialize();
	virtual void Activated();
	virtual void OnReceivedImpulse( class USequenceOp* ActivatorOp, INT InputLinkIndex );
}

/** Is this gate currently open? */
var transient bool bOpen;

/** Mirrors the InputLinks array, hold data whether a specific input has fired. */
var transient array<bool>			LinkedOutputFiredStatus;

/** Cached array of linked input ops for this gate, so we can track that they have all fired. */
var transient native array<pointer>	LinkedOutputs{FSeqOpOutputLink};

defaultproperties
{
	ObjName="AND Gate"
	ObjCategory="Misc"

	bSuppressAutoComment=true
	bOpen=TRUE
	bAutoActivateOutputLinks=false

	InputLinks(0)=(LinkDesc="In")

	VariableLinks.Empty
}
