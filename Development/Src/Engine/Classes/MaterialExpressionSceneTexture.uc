/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionSceneTexture extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

/** 
 * MaterialExpressionSceneTexture: 
 * samples the current scene texture (lighting,etc)
 * for use in a material
 */

/** texture coordinate inputt expression for this node */
var ExpressionInput	Coordinates;

var() enum ESceneTextureType
{
	// 16bit component lighting target
	SceneTex_Lighting
} SceneTextureType;

/** Matches [0,1] UVs to the view within the back buffer. */
var() bool ScreenAlign;

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);

	virtual FString GetCaption() const;
}

defaultproperties
{
	SceneTextureType=SceneTex_Lighting
	MenuCategories(0)="Texture"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=1,MaskB=1,MaskA=0)
	Outputs(1)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
	Outputs(2)=(OutputName="",Mask=1,MaskR=0,MaskG=1,MaskB=0,MaskA=0)
	Outputs(3)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=1,MaskA=0)
	Outputs(4)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=0,MaskA=1)
}
