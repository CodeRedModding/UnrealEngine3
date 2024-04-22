/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionDesaturation extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

// Outputs: Lerp(Input,dot(Input,LuminanceFactors)),Percent)

var ExpressionInput	Input;
var ExpressionInput	Percent;
var() LinearColor	LuminanceFactors;	// Color component factors for converting a color to greyscale.

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const
	{
		return TEXT("Desaturation");
	}

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	LuminanceFactors=(R=0.3,G=0.59,B=0.11,A=0)
	MenuCategories(0)="Color"
	MenuCategories(1)="Utility"
}
