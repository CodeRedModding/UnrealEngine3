/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionSphereMask extends MaterialExpression
	native(Material);

/** 1 to 4 dimensional vector, should be the same type as B */
var ExpressionInput	A;
/** 1 to 4 dimensional vector, should be the same type as A */
var ExpressionInput	B;
/** in the units that A and B are measured, if not hooked up the internal constant is used */
var ExpressionInput	Radius;
/** 0..1 for the range of 0% to 100%, if not hooked up the internal constant is used */
var ExpressionInput	Hardness;
/** in the units that A and B are measured */
var() float AttenuationRadius;
/** in percent 0%..100% */
var() float HardnessPercent <UIMin=0.0 | UIMax=100.0 | ClampMin=0.0 | ClampMax=100.0>;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	virtual void Serialize(FArchive& Ar);

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	AttenuationRadius=256
	HardnessPercent=100
	MenuCategories(0)="HighLevel"   
}
