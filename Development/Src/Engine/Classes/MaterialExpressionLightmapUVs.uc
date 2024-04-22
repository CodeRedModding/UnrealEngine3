/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 *	A material expression that routes LightmapUVs to the material.
 */
class MaterialExpressionLightmapUVs extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

cpptext
{
	// UMaterialExpression interface.
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
}

defaultproperties
{
	bShowOutputNameOnPin=true
	bHidePreviewWindow=true

	MenuCategories(0)="Coordinates"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=1,MaskB=0,MaskA=0)
}
