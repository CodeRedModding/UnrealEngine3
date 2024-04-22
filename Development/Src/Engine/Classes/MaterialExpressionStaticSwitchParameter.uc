/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionStaticSwitchParameter extends MaterialExpressionStaticBoolParameter
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput A;
var ExpressionInput B;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	virtual FString GetInputName(INT InputIndex) const;

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);

#if WITH_EDITOR
	/**
	 *	Called by the CleanupMaterials function, this will clear the inputs of the expression.
	 *	This only needs to be implemented by expressions that have bUsedByStaticParameterSet set to TRUE.
	 */
	virtual void ClearInputExpressions();
#endif
}

defaultproperties
{
	MenuCategories.Add("WorldPosOffset")
}
