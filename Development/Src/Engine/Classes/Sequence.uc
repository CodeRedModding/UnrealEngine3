/* epic ===============================================
* class Sequence
* Author: rayd
*
* =====================================================
*/
class Sequence extends SequenceOp
	native(Sequence);

cpptext
{
	void CheckForErrors();

	void BuildOpStack(TArray<USequenceOp*> &opStack);
	UBOOL ExecuteOpStack(FLOAT deltaTime, TArray<USequenceOp*> &opStack,INT maxSteps = 0);
	UBOOL UpdateOp(FLOAT deltaTime);

	virtual void ScriptLog(const FString &outText);

	virtual void Activated();

	virtual void OnCreated()
	{
		Super::OnCreated();
		// update our connectors
		UpdateConnectors();
	}

	virtual void OnExport()
	{
		Super::OnExport();
		for (INT idx = 0; idx < SequenceObjects.Num(); idx++)
		{
			SequenceObjects(idx)->OnExport();
		}
		for (INT idx = 0; idx < OutputLinks.Num(); idx++)
		{
			OutputLinks(idx).Links.Empty();
		}
		for (INT idx = 0; idx < VariableLinks.Num(); idx++)
		{
			VariableLinks(idx).LinkedVariables.Empty();
		}
		for (INT idx = 0; idx < EventLinks.Num(); idx++)
		{
			EventLinks(idx).LinkedEvents.Empty();
		}
	}

	virtual void UpdateConnectors();
	void UpdateNamedVarStatus();
	void UpdateInterpActionConnectors();

	virtual void BeginPlay();
	virtual void Destroy();
	
	void FindSeqObjectsByClass(UClass* DesiredClass, TArray<USequenceObject*>& OutputObjects);
	void FindSeqObjectsByName(const FString& Name, UBOOL bCheckComment, TArray<USequenceObject*>& OutputObjects);
	void FindSeqObjectsByObjectName(FName Name, TArray<USequenceObject*>& OutputObjects);
	void FindNamedVariables(FName VarName, UBOOL bFindUses, TArray<USequenceVariable*>& OutputVars);

	UBOOL ReferencesObject(UObject* InObject);

	void DrawSequence(FRenderInterface* RI, TArray<USequenceObject*>& SelectedSeqObjs, USequenceObject* MouseOverSeqObj, INT MouseOverConnType, INT MouseOverConnIndex, FLOAT MouseOverTime, UBOOL bCurves);

	// for generic browser
	void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	INT GetThumbnailLabels( TArray<FString>* InLabels );
};

//==========================
// Base variables

/** Dedicated file log for tracking all script execution */
var const pointer LogFile;

// list of all scripting objects contained in this sequence
var const export array<SequenceObject> SequenceObjects;

/** Default position of origin when opening this sequence in Kismet. */
var	int		DefaultViewX;
var	int		DefaultViewY;
var float	DefaultViewZoom;

native final function FindSeqObjectsByClass( class<SequenceObject> DesiredClass, out array<SequenceObject> OutputObjects );

defaultproperties
{
	ObjName="Sequence"
	InputLinks.Empty
	OutputLinks.Empty
	VariableLinks.Empty

	DefaultViewZoom=1.0
}