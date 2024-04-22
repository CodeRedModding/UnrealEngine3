/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionDepthBiasedAlpha extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** 
 * MaterialExpressionDepthBiasedAlpha: 
 * Determines the alpha based on the following formula:
 *	Alpha = 
 * for use in a material
 */

/** normalize the depth values to [near,far] -> [0,1]	*/
var()	bool		bNormalize;

/** 
 *	The bias scale value
 */
var()	float		BiasScale;

/** 
 *	The source alpha input
 */
var ExpressionInput	Alpha;

/** 
 *	The depth bias input
 *	This can be a constant, texture sample, etc.
 *
 *	NOTE: No bias expression indicates a bias of 1.0f.
 */
var ExpressionInput	Bias;

cpptext
{
	/**
	 *	Compile the material expression
	 *
	 *	@param	Compiler	Pointer to the material compiler to use
	 *
	 *	@return	INT			The compiled code index
	 */	
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);

	/**
	 */	
	virtual INT GetWidth() const;

	/**
	 */	
	virtual FString GetCaption() const;

	/**
	 */	
	virtual INT GetLabelPadding() { return 8; }

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	BiasScale=1.0
	MenuCategories(0)="Depth"
}
