/* epic ===============================================
* class SequenceCondition
* Author: rayd
*
* Base class of any sequence operation that acts as a
* conditional statement, such as boolean expressions.
*
* =====================================================
*/
class SequenceCondition extends SequenceOp
	native(Sequence)
	abstract;

cpptext
{
	virtual void DeActivated()
	{
		// don't do anything, conditions handle their inputs
		// depending on the operation result
	}
}

defaultproperties
{
	ObjName="Undefined Condition"
	ObjColor=(R=0,G=0,B=255,A=255)
}