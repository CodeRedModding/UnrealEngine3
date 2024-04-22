/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionLensFlareSourceDistance extends MaterialExpression
	native(Material);

cpptext
{
	/**
	 *	Compile this expression with the given compiler.
	 *	
	 *	@return	INT			The code index for this expression.
	 */
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	
	/**
	 *	Get the width required by this expression (in the material editor).
	 *
	 *	@return	INT			The width in pixels.
	 */
	virtual INT GetWidth() const;
	
	/**
	 *	Returns the text to display on the material expression (in the material editor).
	 *
	 *	@return	FString		The text to display.
	 */
	virtual FString GetCaption() const;
	
	/**
	 *	Returns the amount of padding to use for the label.
	 *
	 *	@return INT			The padding (in pixels).
	 */
	virtual INT GetLabelPadding() { return 8; }
}

defaultproperties
{
	MenuCategories(0)="LensFlare"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
}
