/**
 * TextureRenderTargetCube
 *
 * Cube render target texture resource. This can be used as a target
 * for rendering as well as rendered as a regular cube texture resource.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class TextureRenderTargetCube extends TextureRenderTarget
	native(Texture)
	hidecategories(Object)
	hidecategories(Texture);

/** The width of the texture.												*/
var() int SizeX;

/** The format of the texture data.											*/
var const EPixelFormat Format;

cpptext
{
	/** 
	* Initialize the settings needed to create a render target texture
	* and create its resource
	* @param	InSizeX - width of the texture
	* @param	InFormat - format of the texture
	*/
	void Init(UINT InSizeX, EPixelFormat InFormat);

	/**
	*	Utility for creating a new UTextureCube from a TextureRenderTargetCube.
	*	TextureRenderTargetCube must be square and a power of two size.
	*	@param	Outer			Outer to use when constructing the new TextureCube.
	*	@param	NewTexName		Name of new UTextureCube object.
	*	@param	Flags			Various control flags for operation (see EObjectFlags)
	*	@param	bSaveSourceArt	If true, source art will be saved.
	*	@return					New UTextureCube object.
	*/
	class UTextureCube* ConstructTextureCube(UObject* Outer, const FString& NewTexName, EObjectFlags InFlags, UBOOL bSaveSourceArt=FALSE);

	// USurface interface

	/**
	* @return width of surface
	*/
	virtual FLOAT GetSurfaceWidth() const { return SizeX; }
	
	/**
	* @return height of surface
	*/
	virtual FLOAT GetSurfaceHeight() const { return SizeX; }	

	// UTexture interface.

	/**
	* Create a new 2D render target texture resource
	* @return newly created FTextureRenderTarget2DResource
	*/
	virtual FTextureResource* CreateResource();
	
	/**
	* Materials should treat a render target 2D texture like a regular 2D texture resource.
	* @return EMaterialValueType for this resource
	*/
	virtual EMaterialValueType GetMaterialType();

	// UObject interface

	/** 
	* Called when any property in this object is modified in UnrealEd
	* @param	PropertyThatChanged - changed property
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** 
	 * Called after the object has been loaded
 	 */
	virtual void PostLoad();

	// Editor thumbnail interface.

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/**
	 * Serialize properties (used for backwards compatibility with main branch)
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	INT GetResourceSize();
}

defaultproperties
{
	// must be a supported format
	Format=PF_A8R8G8B8
}
