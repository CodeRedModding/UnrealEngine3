/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class MaterialExpressionMeshEmitterDynamicParameter extends MaterialExpressionDynamicParameter
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
	MenuCategories.Add("WorldPosOffset")
}
