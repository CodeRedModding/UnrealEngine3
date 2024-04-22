/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionStaticSwitch extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() bool	DefaultValue;
var() bool	ExtendedCaptionDisplay;

var ExpressionInput A;
var ExpressionInput B;
var ExpressionInput Value;

cpptext
{
	virtual UBOOL CanEditChange( const UProperty* InProperty ) const;
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
	virtual FString GetInputName(INT InputIndex) const;

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);
}

defaultproperties
{
	MenuCategories(0)="FunctionUtility"
	MenuCategories(1)="WorldPosOffset"
}
