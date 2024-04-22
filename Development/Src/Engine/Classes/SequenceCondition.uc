/**
 * Base class of any sequence operation that acts as a conditional statement, such as simple boolean expression.
 * When a SequenceCondition is activated, the values for each variable linked to this conditional are retrieved.
 * The appropriate output link (which is specific to each conditional type) is then activated based on the value of the
 * those variables.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SequenceCondition extends SequenceOp
	native(Sequence)
	abstract;


defaultproperties
{
	ObjName="Undefined Condition"
	ObjColor=(R=0,G=0,B=255,A=255)
	bAutoActivateOutputLinks=false
}
