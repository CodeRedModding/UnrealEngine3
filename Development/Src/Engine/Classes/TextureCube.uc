/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TextureCube extends Texture
	native(Texture)
	hidecategories(Object);

/** Cached width of the cubemap. */
var transient const int SizeX;

/** Cached height of the cubemap. */
var transient const int SizeY;

/** Cached format of the cubemap */
var transient const EPixelFormat Format;

/** Cached number of mips in the cubemap */
var transient const int NumMips;

/** Cached information on whether the cubemap is valid, aka all faces are non NULL and match in width, height and format. */
var transient const bool bIsCubemapValid;

var() const Texture2D FacePosX;
var() const Texture2D FaceNegX;
var() const Texture2D FacePosY;
var() const Texture2D FaceNegY;
var() const Texture2D FacePosZ;
var() const Texture2D FaceNegZ;

cpptext
{
	// UObject interface.
	void InitializeIntrinsicPropertyValues();
	virtual void Serialize(FArchive& Ar);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();

	// Thumbnail interface.
	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	// USurface interface
	virtual FLOAT GetSurfaceWidth() const { return SizeX; }
	virtual FLOAT GetSurfaceHeight() const { return SizeY; }

	// UTexture interface
	virtual FTextureResource* CreateResource();
	virtual EMaterialValueType GetMaterialType() { return MCT_TextureCube; }
	
	// UTextureCube interface

	/**
	 * Validates cubemap which entails verifying that all faces are non-NULL and share the same format, width, height and number of
	 * miplevels. The results are cached in the respective mirrored properties and bIsCubemapValid is set accordingly.
	 */
	void Validate();

	/**
	 * Returns the face associated with the passed in index.
	 *
	 * @param	FaceIndex	index of face to return
	 * @return	texture object associated with passed in face index
	 */
	UTexture2D* GetFace( INT FaceIndex ) const;

	/**
	 * Sets the face associated with the passed in index.
	 *
	 * @param	FaceIndex	index of face to return
	 * @param	FaceTexture	texture object to associate with passed in face index
	 */
	void SetFace(INT FaceIndex,UTexture2D* FaceTexture);

	/**
	 * Returns the size of this texture in bytes on 360 if it had MipCount miplevels streamed in.
	 *
	 * @param	MipCount	Number of toplevel mips to calculate size for
	 * @return	size of top mipcount mips in bytes
	 */
	INT Get360Size( INT MipCount ) const;

	/**
	 * Calculates the size of this texture if it had MipCount miplevels streamed in.
	 *
	 * @param	MipCount	Which mips to calculate size for.
	 * @return	Total size of all specified mips, in bytes
	 */
	virtual INT CalcTextureMemorySize( ETextureMipCount MipCount ) const;
}
