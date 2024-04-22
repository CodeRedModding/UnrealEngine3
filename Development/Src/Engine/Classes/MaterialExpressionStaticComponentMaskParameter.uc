/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionStaticComponentMaskParameter extends MaterialExpressionParameter
	native(Material)
	collapsecategories
	hidecategories(Object);

var ExpressionInput	Input;
var() bool	DefaultR,
			DefaultG,
			DefaultB,
			DefaultA;

//the override that will be set when this expression is being compiled from a static permutation
var const native transient pointer InstanceOverride{const FStaticComponentMaskParameter};

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

#if WITH_EDITOR
	/**
	 *	Called by the CleanupMaterials function, this will clear the inputs of the expression.
	 *	This only needs to be implemented by expressions that have bUsedByStaticParameterSet set to TRUE.
	 */
	virtual void ClearInputExpressions();
#endif

	virtual void SetStaticParameterOverrides(const FStaticParameterSet* Permutation);
	virtual void ClearStaticParameterOverrides();
}

defaultproperties
{
	bUsedByStaticParameterSet=true

	MenuCategories(0)="Parameters"
}
