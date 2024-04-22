/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Texture2DDynamic extends Texture
	native(Texture)
	hidecategories(Object);


/** The width of the texture. */
var native transient int SizeX;

/** The height of the texture. */
var native transient int SizeY;

/** The format of the texture. */
var native transient EPixelFormat Format;

/** The number of mip-maps in the texture. */
var native transient int NumMips;

/** Whether the texture can be used as a resolve target. */
var native transient bool bIsResolveTarget;


/**
 * Initializes the texture with 1 mip-level and creates the render resource.
 *
 * @param InSizeX			- Width of the texture, in texels
 * @param InSizeY			- Height of the texture, in texels
 * @param InFormat			- Format of the texture, defaults to PF_A8R8G8B8
 * @param InIsResolveTarget	- Whether the texture can be used as a resolve target
 */
native final function Init(int InSizeX, int InSizeY, optional EPixelFormat InFormat = PF_A8R8G8B8, optional bool InIsResolveTarget = FALSE);

/**
 * Update the mip data for the texture and its render resource
 * It is assumed that the data is being copied is already of the right format/size for the mip.
 *
 * @param MipIdx 0 based index to the mip level to update
 * @param MipData byte array of data to copy to mip
 */
native function UpdateMip(int MipIdx,const out array<byte> MipData);

/**
 * Update the mip data for the texture and its render resource from JPEG data.
 * If the decoded JPEG size doesn't match the existing texture size 
 * then the texture is resized.
 *
 * @param MipIdx 0 based index to the mip level to update
 * @param MipData byte array of JPEG to decode and then copy to mip
 */
native function UpdateMipFromJPEG(int MipIdx,const out array<byte> MipData);

cpptext
{
public:
	// UTexture interface.
	virtual FTextureResource* CreateResource();
	virtual EMaterialValueType GetMaterialType()  { return MCT_Texture2D; }

	// USurface interface.
	virtual FLOAT GetSurfaceWidth() const;
	virtual FLOAT GetSurfaceHeight() const;

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
}

/** Creates and initializes a new Texture2DDynamic with the requested settings */
static native noexport final function Texture2DDynamic Create(int InSizeX, int InSizeY, optional EPixelFormat InFormat = PF_A8R8G8B8, optional bool InIsResolveTarget = FALSE);

defaultproperties
{
	// all mip levels will be resident in memory
	NeverStream=True

	// must be a supported format
	Format=PF_A8R8G8B8
}
