/* epic ===============================================
* class SequenceEvent
*
* Sequence event is a representation of any event that
* is used to instigate a sequence.
*
* =====================================================
*/
class SequenceEvent extends SequenceOp
	native(Sequence)
	abstract;

cpptext
{
	// USequenceObject interface
	virtual void DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime);
	virtual FIntRect GetSeqObjBoundingBox();
	FIntPoint GetCenterPoint(FRenderInterface* RI);

	virtual UBOOL CheckActivate(AActor *inOriginator, AActor *inInstigator,UBOOL bTest=0);

	virtual void OnExport()
	{
		Super::OnExport();
		Originator = NULL;
		Instigator = NULL;
	}
}

//==========================
// Base variables

var Actor				Originator;	   		 	// originator of this event, set in the editor at creation
var transient Actor		Instigator;				// last instigator that triggered this event
var transient float		ActivationTime;		 	// time at which this event was generated
var transient int		TriggerCount;			// how many times this event has been activated

var() int				MaxTriggerCount;		// how many times can this event be activated, 0 for infinite
var() float				ReTriggerDelay;			// delay between allowed activations
var() bool				bEnabled;				// is this event currently enabled?

/** Require this event to be activated by a player? */
var() bool bPlayerOnly;

var	  int				MaxWidth;

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
 * @return	true if this event can be activated, or was activate if !bTest
 */
native function bool CheckActivate(Actor inOriginator,Actor inInstigator,optional bool bTest);

defaultproperties
{
	ObjColor=(R=255,G=0,B=0,A=255)
	MaxTriggerCount=1
	bEnabled=true
	bPlayerOnly=true

	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Instigator",bWriteable=true)
}