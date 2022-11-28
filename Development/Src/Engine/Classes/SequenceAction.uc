/* epic ===============================================
* class SequenceAction
* Author: rayd
*
* =====================================================
*/
class SequenceAction extends SequenceOp
	native(Sequence)
	abstract;

cpptext
{
	virtual void Activated();
	virtual void PreActorHandle(AActor *inActor) {}
}

defaultproperties
{
	ObjName="Unknown Action"
	ObjColor=(R=255,G=0,B=255,A=255)
	VariableLinks(0)=(ExpectedType=class'SeqVar_Object',LinkDesc="Target")
}