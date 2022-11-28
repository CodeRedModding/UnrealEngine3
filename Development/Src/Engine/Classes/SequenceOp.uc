/* epic ===============================================
* class SequenceOp
*
* Base class of any sequence object that performs any
* sort of operation, such as an action, or condition.
*
* =====================================================
*/
class SequenceOp extends SequenceObject
	native(Sequence)
	abstract;

cpptext
{
	virtual void CheckForErrors();

	// USequenceOp interface
	virtual UBOOL UpdateOp(FLOAT deltaTime);
	virtual void Activated();
	virtual void DeActivated();

	// helper functions
	void GetBoolVars(TArray<UBOOL*> &outBools, const TCHAR *inDesc = NULL);
	void GetIntVars(TArray<INT*> &outInts, const TCHAR *inDesc = NULL);
	void GetFloatVars(TArray<FLOAT*> &outFloats, const TCHAR *inDesc = NULL);
	void GetObjectVars(TArray<UObject**> &outObjects, const TCHAR *inDesc = NULL);
	void GetStringVars(TArray<FString*> &outStrings, const TCHAR *inDesc = NULL);

	INT FindConnectorIndex(const FString& ConnName, INT ConnType);
	void CleanupConnections();

	// USequenceObject interface
	virtual void DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime);
	virtual FIntPoint GetConnectionLocation(INT ConnType, INT ConnIndex);

	FIntPoint GetLogicConnectorsSize(FRenderInterface* RI, INT* InputY=0, INT* OutputY=0);
	FIntPoint GetVariableConnectorsSize(FRenderInterface* RI);
	FColor GetVarConnectorColor(INT LinkIndex);

	void DrawLogicConnectors(FRenderInterface* RI, const FIntPoint& Pos, const FIntPoint& Size, INT MouseOverConnType, INT MouseOverConnIndex);
	void DrawVariableConnectors(FRenderInterface* RI, const FIntPoint& Pos, const FIntPoint& Size, INT MouseOverConnType, INT MouseOverConnIndex);

	virtual void DrawLogicLinks(FRenderInterface* RI, UBOOL bCurves);
	virtual void DrawVariableLinks(FRenderInterface* RI, UBOOL bCurves);

	void MakeLinkedObjDrawInfo(struct FLinkedObjDrawInfo& ObjInfo, INT MouseOverConnType = -1, INT MouseOverConnIndex = INDEX_NONE);
};

/** Is this operation currently active? */
var transient bool bActive;

/**
 * Represents an input link for a SequenceOp, that is
 * connected via another SequenceOp's output link.
 */
struct native SeqOpInputLink
{
	/** Text description of this link */
	var string LinkDesc;

	/** Is this link currently active? */
	var bool bHasImpulse;

	/** Name of the linked action that creates this input, for Sequences */
	var Name LinkAction;

	// Temporary for drawing! Will think of a better way to do this! - James
	var int DrawY;
	var bool bHidden;
};
var array<SeqOpInputLink>		InputLinks;

/**
 * Individual output link entry, for linking an output link
 * to an input link on another operation.
 */
struct native SeqOpOutputInputLink
{
	/** SequenceOp this is linked to */
	var SequenceOp LinkedOp;

	/** Index to LinkedOp's InputLinks array that this is linked to */
	var int InputLinkIdx;
};

/**
 * Actual output link for a SequenceOp, containing connection
 * information to multiple InputLinks in other SequenceOps.
 */
struct native SeqOpOutputLink
{
	/** List of actual connections for this output */
	var array<SeqOpOutputInputLink> Links;

	/** Text description of this link */
	var string					LinkDesc;

	/** Is this link currently active? */
	var bool					bHasImpulse;

	/** Name of the linked action that creates this output, for Sequences */
	var Name					LinkAction;

	// Temporary for drawing! Will think of a better way to do this! - James
	var int						DrawY;
	var bool					bHidden;
};
var array<SeqOpOutputLink>		OutputLinks;

/* epic ===============================================
* struct SeqVarLink
*
* A utility struct that represents an input link to the
* operation.
*
* =====================================================
*/
struct native SeqVarLink
{
	/** Class of variable that can be attached to this connector. */
	var class<SequenceVariable>	ExpectedType;

	/** SequenceVariables that we are linked to. */
	var array<SequenceVariable>	LinkedVariables;

	/** Text description of this variable's use with this op */
	var string					LinkDesc;

	/** Name of the linked external variable that creates this link, for sub-Sequences */
	var Name	LinkVar;

	/** Is this variable written to by this op? */
	var bool	bWriteable;

	/** Should draw this connector in Kismet. */
	var bool	bHidden;

	/** Minimum number of variables that should be attached to this connector. */
	var int		MinVars;

	/** Maximum number of variables that should be attached to this connector. */
	var int		MaxVars;

	/** For drawing. */
	var int		DrawX;

	structdefaults
	{
		ExpectedType=class'Engine.SequenceVariable'
		MinVars = 1
		MaxVars = 255
	}
};

/** All variables used by this operation, both input/output. */
var array<SeqVarLink>			VariableLinks;			

/* epic ===============================================
* struct SeqEventLink
*
* Handles linking events to operations so that they
* can be manipulated at run-time.
*
* =====================================================
*/
struct native SeqEventLink
{
	var class<SequenceEvent>	ExpectedType;
	var array<SequenceEvent>	LinkedEvents;
	var string					LinkDesc;

	// Temporary for drawing! - James
	var int						DrawX;
	var bool					bHidden;

	structdefaults
	{
		ExpectedType=class'Engine.SequenceEvent'
	}
};
var array<SeqEventLink>			EventLinks;

native final function GetObjectVars(out array<Object> objVars,optional string inDesc);

defaultproperties
{
    // define the base input link required for this op to be activated
	InputLinks(0)=(LinkDesc="In")
	// define the base output link that this action generates (always assumed to generate at least a single output)
	OutputLinks(0)=(LinkDesc="Out")
}
