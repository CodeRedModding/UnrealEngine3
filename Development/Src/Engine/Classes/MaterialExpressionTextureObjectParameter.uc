/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 * Node which creates a texture parameter and outputs the texture object itself, instead of sampling the texture first.
 * This is used with material functions to implement texture parameters without actually putting the parameter in the function.
 */
class MaterialExpressionTextureObjectParameter extends MaterialExpressionTextureSampleParameter
	native(Material)
	collapsecategories
	hidecategories(Object);

cpptext
{
	virtual FString GetCaption() const;
	virtual const TCHAR* GetRequirements();
	virtual const TArray<FExpressionInput*> GetInputs();
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT CompilePreview(FMaterialCompiler* Compiler, INT OutputIndex);
}

defaultproperties
{
	Texture=Texture2D'EngineResources.DefaultTexture'
	MenuCategories(0)="Texture"
	MenuCategories(1)="Parameters"

	// Clear the existing outputs from the parent class
	Outputs.Empty
	Outputs(0)=(OutputName="")
}
