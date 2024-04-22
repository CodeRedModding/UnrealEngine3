/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Texture2DComposite extends Texture
	native(Texture)
	hidecategories(Object);

/**
 * Defines a source texture and UV region in that texture
 */
struct native SourceTexture2DRegion
{
	var int OffsetX;
	var int OffsetY;
	var int SizeX;
	var int SizeY;
	var int DestOffsetX;
	var int DestOffsetY;
	var Texture2D Texture2D;
};

/** list of source textures and UV regions for compositing */
var array<SourceTexture2DRegion> SourceRegions;

/** 
* Optional max texture size clamp for the composite texture. A value of 0 is ignored and 
* defaults to deriving the texture size using texture LOD bias settings from the source textures 
*/
var int	MaxTextureSize;

/** 
*  Dimensions of the composite texture. A value of 0 is ignored and defaults to deriving the 
*  texture size using the size of the source textures 
*/
var int DestSizeX;
var int DestSizeY;

/** Utility that checks to see if all Texture2Ds specified in the SourceRegions array are fully streamed in. */
native final function bool SourceTexturesFullyStreamedIn();

/**
* Regenerates this composite texture using the list of source texture regions.
* The existing mips are reallocated and the RHI resource for the texture is updated
*
* @param NumMipsToGenerate - number of mips to generate. if 0 then all mips are created
*/
native final function UpdateCompositeTexture(int NumMipsToGenerate);

/** Utils to reset all source region info. */
native final function ResetSourceRegions();

cpptext
{
private:
	/**
	 * Calculate the first available mip from a set of textures based on the LOD bias for each
	 * texture.
	 *
	 * @return first available mip index from the source regions
	 */
	INT GetFirstAvailableMipIndex(const TArray<FSourceTexture2DRegion>& ValidRegions);

	/**
	* Locks each region of the source RHI texture 2d resources and copies the block of data
	* for that region to the destination mip buffer. This is done for all mip levels.
	*
	* (Only called by the rendering thread)
	*/
	void RenderThread_CopyRectRegions(const TArray<FSourceTexture2DRegion>& ValidRegions);

	// UTexture interface.
	virtual FTextureResource* CreateResource();
	virtual EMaterialValueType GetMaterialType()  { return MCT_Texture2D; }

	// USurface interface.
	virtual FLOAT GetSurfaceWidth() const;
	virtual FLOAT GetSurfaceHeight() const;

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
}

defaultproperties
{
	// all mip levels will be resident in memory
	NeverStream=True
}
