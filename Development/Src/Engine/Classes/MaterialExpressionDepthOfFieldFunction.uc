/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionDepthOfFieldFunction extends MaterialExpression
	native(Material);


enum EDepthOfFieldFunctionValue
{
	TDOF_NearAndFarMask,		// 0:in Focus .. 1:Near or Far
	TDOF_NearMask,				// 0:in Focus or Far .. 1:Near
	TDOF_FarMask				// 0:in Focus or Near .. 1:Far
};

/** Determines the mapping place to use on the terrain. */
var() EDepthOfFieldFunctionValue	FunctionValue;

/** usually nothing or PixelDepth */
var ExpressionInput	Depth;

cpptext
{
	/**
	 *	Compile this expression with the given compiler.
	 *	
	 *	@return	INT			The code index for this expression.
	 */
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	
	/**
	 *	Returns the text to display on the material expression (in the material editor).
	 *
	 *	@return	FString		The text to display.
	 */
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
	MenuCategories(0)="Utility"
}
