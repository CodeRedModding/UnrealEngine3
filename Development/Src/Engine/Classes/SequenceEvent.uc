/**
 * class SequenceEvent
 *
 * Sequence event is a representation of any event that
 * is used to instigate a sequence.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SequenceEvent extends SequenceOp
	native(Sequence)
	abstract;

cpptext
{
	

	virtual UBOOL CheckActivate(AActor *InOriginator, AActor *InInstigator, UBOOL bTest=FALSE, TArray<INT>* ActivateIndices = NULL, UBOOL bPushTop = FALSE);

	/**
	 * Adds an error message to the map check dialog if this SequenceEvent's EventActivator is bStatic
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();

	// USequenceObject interface
	virtual void DrawSeqObj(FCanvas* Canvas, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime);
	virtual FIntRect GetSeqObjBoundingBox();
	FIntPoint GetCenterPoint(FCanvas* Canvas);

	virtual FString GetDisplayTitle() const;
#endif

	/**
	 * This is a debug version of ActivateEvent which can be used by automated testing tools to Activate
	 * an event for testing purposes.
	 **/
	virtual void DebugActivateEvent(AActor *InOriginator, AActor *InInstigator, TArray<INT> *ActivateIndices = NULL);

	virtual UBOOL RegisterEvent();

	/**
	 * Fills in the value of the "Instigator" VariableLink
	 */
	virtual void InitializeLinkedVariableValues();

	virtual void OnExport()
	{
		Super::OnExport();
		Originator = NULL;
		Instigator = NULL;
	}

	/**
	 * Returns whether this SequenceObject can exist in a sequence without being linked to anything else (i.e. does not require
	 * another sequence object to activate it)
	 */
	virtual UBOOL IsStandalone() const { return TRUE; }

	virtual void ActivateEvent(AActor *InOriginator, AActor *InInstigator, TArray<INT> *ActivateIndices = NULL, UBOOL bPushTop = FALSE, UBOOL bFromQueued = FALSE);
}

//==========================
// Base variables

/** List of events that are in-place duplicates of this event, used to relay messages. */
var array<SequenceEvent> DuplicateEvts;

/** Originator of this event, set at editor time.  Usually the actor that this event is attached to. */
var Actor				Originator;

/**
 * Instigator of the event activation, or the actor that caused the event to be activated.  Can vary depending
 * on the type of event.
 */
var Actor Instigator;

/** Last time this event was activated at */
var float ActivationTime;

/** Number of times this event has been activated */
var int TriggerCount;

/** How many times can this event be activated, 0 for infinite */
var() int				MaxTriggerCount;

/** Delay between allowed activations */
var() float				ReTriggerDelay;

/** Is this event currently enabled? */
var() bool				bEnabled;

/** Used by event managers (such as DialogueManager) to help filter out events that occur at same time */
var() Byte				Priority;

/** Require this event to be activated by a player? */
var() bool 				bPlayerOnly;

/** Editor only, max width of the title bar? */
var	  int				MaxWidth;

/** Has this event been successfully register? */
var transient bool 		bRegistered;

/** if true, this event (and therefore all linked actions) is triggered on the client instead of the server
 * use for events that don't affect gameplay
 * @note: direct references to level placed actors used by client side events/actions require that the actors have
 * bStatic or bNoDelete set; otherwise the reference will be NULL on the client
 */
var() const bool bClientSideOnly;

/**
 * Called when the sequence that contains this event is initialized (@see USequence::InitializeSequence).  For events
 * attached to actors, this will occur at level startup (@see USequence::BeginPlay())
 */
event RegisterEvent();

/**
 * Checks if this event could be activated, and if bTest == false
 * then the event will be activated with the specified actor as the
 * instigator.
 *
 * @param	inOriginator - actor to use as the originator
 *
 * @param	inInstigator - actor to use as the instigator
 *
 * @param	bTest - if true, then the event will not actually be
 * 			activated, only tested for success
 *
 * @param	ActivateIndices - array of indices of output links to activate
 *				if the event is activated. If unspecified, the default
 *				is to activate all of them.
 * @param	bPushTop - if true and the event is activated,
 * 			adds it to the top of the stack (meaning it will be executed first), rather than the bottom
 *
 * @return	true if this event can be activated, or was activate if !bTest
 */
native noexport final function bool CheckActivate(Actor inOriginator, Actor inInstigator, optional bool bTest, optional const out array<int> ActivateIndices, optional bool bPushTop);

/* Reset() - reset to initial state - used when restarting level without reloading */
function Reset()
{
	// reset triggering related properties so that we may be triggered the maximum number of times again
	ActivationTime = 0.f;
	TriggerCount = 0;
	Instigator = None;
}

/**
 * Called once this event is toggled via SeqAct_Toggle.
 */
event Toggled()
{
}

defaultproperties
{
	ObjColor=(R=255,G=0,B=0,A=255)
	MaxTriggerCount=1
	bEnabled=true
	bPlayerOnly=true
	bAutoActivateOutputLinks=false

	InputLinks.Empty
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Instigator",bWriteable=true)
}
