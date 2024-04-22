/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionSceneDepth extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** 
 * MaterialExpressionSceneDepth: 
 * samples the current scene texture depth target
 * for use in a material
 */

/** texture coordinate inputt expression for this node */
var ExpressionInput	Coordinates;

/** normalize the depth values to [near,far] -> [0,1] */
var() bool bNormalize;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);

	virtual FString GetCaption() const;
}

defaultproperties
{
	MenuCategories(0)="Depth"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
}
