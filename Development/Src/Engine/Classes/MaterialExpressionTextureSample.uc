/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTextureSample extends MaterialExpression
	native(Material)
	collapsecategories
	hidecategories(Object);

var() Texture		Texture;
var ExpressionInput	Coordinates;

/** 
 * Texture object input which overrides Texture if specified. 
 * This only shows up in material functions and is used to implement texture parameters without actually putting the texture parameter in the function.
 */
var ExpressionInput	TextureObject;

cpptext
{
	UBOOL CanEditChange(const UProperty* InProperty) const;

	virtual const TArray<FExpressionInput*> GetInputs();
	virtual FExpressionInput* GetInput(INT InputIndex);
	virtual FString GetInputName(INT InputIndex) const;
	virtual INT Compile(FMaterialCompiler* Compiler, INT OutputIndex);
	virtual INT GetWidth() const;
	virtual FString GetCaption() const;
	virtual INT GetLabelPadding() { return 8; }
	
	/**
	 * Updates the material's cached reference to the resource for a given texture.
	 * @param Texture - The UTexture which has a new FTexture.
	 */
	void UpdateTextureResource(class UTexture* Texture);

	/**
	 * Replaces references to the passed in expression with references to a different expression or NULL.
	 * @param	OldExpression		Expression to find reference to.
	 * @param	NewExpression		Expression to replace reference with.
	 */
	virtual void SwapReferenceTo(UMaterialExpression* OldExpression,UMaterialExpression* NewExpression = NULL);

	/**
	 * MatchesSearchQuery: Check this expression to see if it matches the search query
	 * @param SearchQuery - User's search query (never blank)
	 * @return TRUE if the expression matches the search query
     */
	virtual UBOOL MatchesSearchQuery( const TCHAR* SearchQuery );
}

defaultproperties
{
	MenuCategories(0)="Texture"
	Outputs(0)=(OutputName="",Mask=1,MaskR=1,MaskG=1,MaskB=1,MaskA=0)
	Outputs(1)=(OutputName="",Mask=1,MaskR=1,MaskG=0,MaskB=0,MaskA=0)
	Outputs(2)=(OutputName="",Mask=1,MaskR=0,MaskG=1,MaskB=0,MaskA=0)
	Outputs(3)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=1,MaskA=0)
	Outputs(4)=(OutputName="",Mask=1,MaskR=0,MaskG=0,MaskB=0,MaskA=1)
}
