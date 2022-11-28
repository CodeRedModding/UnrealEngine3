/* epic ===============================================
* class SequenceVariable
* Author: rayd
*
* Base class of variable representation for scripted
* sequences.
*
* =====================================================
*/
class SequenceVariable extends SequenceObject
	native(Sequence)
	abstract;

/** This is used by SeqVar_Named to find a variable anywhere in the levels sequence. */
var()	name	VarName;

cpptext
{
	// UObject interface
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	// USequenceObject interface
	virtual void DrawSeqObj(FRenderInterface* RI, UBOOL bSelected, UBOOL bMouseOver, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime);
	virtual FIntRect GetSeqObjBoundingBox();

	//NOTE: yes this sucks, and is tedious, but works as an interim solution
	virtual INT* GetIntRef()
	{
		return NULL;
	}

	virtual UBOOL* GetBoolRef()
	{
		return NULL;
	}

	virtual FLOAT* GetFloatRef()
	{
		return NULL;
	}

	virtual FString* GetStringRef()
	{
		return NULL;
	}

	virtual UObject** GetObjectRef()
	{
		return NULL;
	}

	virtual FString GetValueStr()
	{
		return FString(TEXT("Undefined"));
	}

	// USequenceVariable interface
	virtual void DrawExtraInfo(FRenderInterface* RI, const FVector& CircleCenter) {}

	FIntPoint GetVarConnectionLocation();
}

defaultproperties
{
	ObjName="Undefined Variable"
	ObjColor=(R=0,G=0,B=0,A=255)
	bDrawLast=true
}