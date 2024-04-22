/**
 * Absolute value material expression for user-defined materials
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionAbs extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** Link to the input expression to be evaluated */
var ExpressionInput	Input;

cpptext
{
    /**
	 * Creates the new shader code chunk needed for the Abs expression
	 *
	 * @param	Compiler - Material compiler that knows how to handle this expression
	 * @return	Index to the new FMaterialCompiler::CodeChunk entry for this expression
	 */	
	virtual INT Compile( FMaterialCompiler* Compiler, INT OutputIndex );

	/**
	 * Textual description for this material expression
	 *
	 * @return	Caption text
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
	MenuCategories(0)="Math"
}
