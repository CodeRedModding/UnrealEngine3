class SeqVar_External extends SequenceVariable
	native(Sequence);

cpptext
{
	// UObject interface
	virtual void PostLoad();

	// SequenceObject interface
	virtual void OnConnect(USequenceObject *connObj,INT connIdx);

	// SequenceVariable interface
	virtual FString GetValueStr();
}

/** */
var class<SequenceVariable> ExpectedType;

/** Name of the variable link to create on the parent sequence */
var() string VariableLabel;

defaultproperties
{
	ObjName="External Variable"
	VariableLabel="Default Var"
}
