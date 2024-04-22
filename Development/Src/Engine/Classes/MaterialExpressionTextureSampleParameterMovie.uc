/**
 * Movie texture paramater for material instance
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class MaterialExpressionTextureSampleParameterMovie extends MaterialExpressionTextureSampleParameter
	native(Material)
	collapsecategories
	hidecategories(Object);

cpptext
{
	/**
	 * Textual description for this material expression
	 *
	 * @return	Caption text
	 */	
	virtual FString GetCaption() const;

	/**
	 * Return true if the texture is a movie texture
	 *
	 * @param	InTexture - texture to test
	 * @return	true/false
	 */	
	virtual UBOOL TextureIsValid( UTexture* InTexture );

    /**
	 * Called when TextureIsValid==false
	 *
	 * @return	Descriptive error text
	 */	
	virtual const TCHAR* GetRequirements();
}

defaultproperties
{
	MenuCategories(0)="Texture"
	MenuCategories(1)="Parameters"
}
