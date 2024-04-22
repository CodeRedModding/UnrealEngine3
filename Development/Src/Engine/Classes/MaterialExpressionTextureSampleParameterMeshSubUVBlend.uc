/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTextureSampleParameterMeshSubUVBlend extends MaterialExpressionTextureSampleParameterMeshSubUV
	native(Material)
	collapsecategories
	hidecategories(Object);

cpptext
{
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual FString GetCaption() const;
}

defaultproperties
{
	MenuCategories(0)="Texture"
	MenuCategories(1)="Parameters"
	MenuCategories(2)="Particles"
}
