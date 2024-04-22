/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * A font class that stores multiple font pages for different resolutions
 */

class MultiFont extends Font
	native;

/** Holds a list of resolutions that map to a given set of font pages */
var() editinline  array<float> ResolutionTestTable;

cpptext
{
	void Serialize( FArchive& Ar );

	/**
	* Called after object and all its dependencies have been serialized.
	*/
	virtual void PostLoad();

    /**
     * Caches the character count and maximum character height for this font (as well as sub-fonts, in the multi-font case)
     */
    virtual void CacheCharacterCountAndMaxCharHeight();
    
	/**
	 * Calulate the index for the texture page containing the multi-font character set to use, based on the specified screen resolution.
	 *
	 * @param	HeightTest	the height (in pixels) of the viewport being rendered to.
	 *
	 * @return	the index of the multi-font "subfont" that most closely matches the specified resolution.  this value is used
	 *			as the value for "ResolutionPageIndex" when calling other font-related methods.
	 */
	virtual INT GetResolutionPageIndex(FLOAT HeightTest) const;

	/**
	 * Determine the height of the mutli-font resolution page which will be used for the specified resolution.
	 *
	 * @param	ViewportHeight	the height (in pixels) of the viewport being rendered to.
	 */
	virtual FLOAT GetAuthoredViewportHeight( float ViewportHeight ) const;

	/**
	 * Calculate the amount of scaling necessary to match the authored resolution for the multi-font level which most closely matches
	 * the specified resolution.
	 *
	 * @param	HeightTest	the height (in pixels) of the viewport being rendered to.
	 *
	 * @return	the percentage scale required to match the size of the multi-font's closest matching subfont.
	 */
	virtual FLOAT GetScalingFactor(FLOAT HeightTest) const;
}

/**
 * Calulate the index into the ResolutionTestTable which is closest to the specified screen resolution.
 *
 * @param	HeightTest	the height (in pixels) of the viewport being rendered to.
 *
 * @return	the index [into the ResolutionTestTable array] of the resolution which is closest to the specified resolution.
 */
native function int GetResolutionTestTableIndex(float HeightTest) const;

defaultproperties
{
}
