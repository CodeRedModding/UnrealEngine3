/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionBumpOffset extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

// Outputs: Coordinate + Eye.xy * (Height - ReferencePlane) * HeightRatio

var ExpressionInput	Coordinate;
var ExpressionInput	Height;
var ExpressionInput	HeightRatioInput;
var() float			HeightRatio;	// Perceived height as a fraction of width.
var() float			ReferencePlane;	// Height at which no offset is applied.

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	HeightRatio=0.05
	ReferencePlane=0.5

	MenuCategories(0)="Utility"
}
