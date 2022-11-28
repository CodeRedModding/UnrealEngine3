/* epic ===============================================
* class SequenceObject
* Author: rayd
*
* Abstract object used as a base for any objects contained
* within a sequence.
*
* =====================================================
*/
class SequenceObject extends Object
	native(Sequence)
	abstract
	hidecategories(Object);

cpptext
{
	virtual void CheckForErrors() {};
	ULevel* GetLevel();

	virtual void OnExport();
	virtual void OnConnect(USequenceObject *connObj,INT connIdx) {}

	virtual void ScriptLog(const FString &outText);

	// USequenceObject interface
	virtual void DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime) {};
	virtual void DrawLogicLinks(FRenderInterface *RI, UBOOL bCurves) {};
	virtual void DrawVariableLinks(FRenderInterface *RI, UBOOL bCurves) {};
	virtual void OnCreated() {};
	virtual void OnSelected() {};
	virtual FIntRect GetSeqObjBoundingBox();
	void SnapPosition(INT Gridsize);
	FString GetSeqObjFullName();
	USequence* GetRootSequence();

	FIntPoint GetTitleBarSize(FRenderInterface* RI);
	FColor GetBorderColor(UBOOL bSelected, UBOOL bMouseOver);

	void DrawTitleBar(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, const FIntPoint& Pos, const FIntPoint& Size);
}

// The Outer of a SequenceObject is the Sequence

//==========================
// Editor variables

var		int						ObjPosX, ObjPosY;	// Visual pos within sequence
var		editconst string		ObjName;			// Text label that describes this object
var		color					ObjColor;			// Color to use when drawing this object
var()	string					ObjComment;			// User-editable text comment

var		bool					bDrawFirst;			// Draw in first pass
var		bool					bDrawLast;			// Draw in last pass.
var		int						DrawWidth;
var		int						DrawHeight;

/**
 * Writes out the specified text to a dedicated scripting log file.
 * 
 * @param	outText - string to log
 */
native final function ScriptLog(string outText);

defaultproperties
{
	ObjName="Undefined"
	ObjColor=(R=255,G=255,B=255,A=255)
}
