/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionPixelDepth extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** 
 * MaterialExpressionPixelDepth: 
 * Gives the depth of the current pixel being drawn
 * for use in a material
 */

/** normalize the depth values to [near,far] -> [0,1] */
var() bool bNormalize;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
}

defaultproperties
{
	MenuCategories(0)="Depth"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
}
