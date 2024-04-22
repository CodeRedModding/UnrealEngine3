/** 
 * TextureMovie
 * Movie texture support base class.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class TextureMovie extends Texture
	native(Texture)
	hidecategories(Object);

/** The width of the texture. */
var const int SizeX;

/** The height of the texture. */
var const int SizeY;

/** The format of the texture data. */
var const EPixelFormat Format;

/** The addressing mode to use for the X axis. */
var() TextureAddress AddressX;

/** The addressing mode to use for the Y axis. */
var() TextureAddress AddressY;

/** Class type of Decoder that will be used to decode Data. */
var	const class<CodecMovie> DecoderClass;
/** Instance of decoder. */
var	const transient	CodecMovie Decoder;

/** Whether the movie is currently paused. */
var const transient bool Paused;
/** Whether the movie is currently stopped. */
var const transient bool Stopped;
/** Whether the movie should loop when it reaches the end. */
var() bool Looping;
/** Whether the movie should reset to first frame when it reaches the end. */
var() bool ResetOnLastFrame;
/** Whether the movie should automatically start playing when it is loaded. */
var() bool AutoPlay;

/** Raw compressed data as imported. */
var	native	const UntypedBulkData_Mirror	Data{FByteBulkData};

/** Set in order to synchronize codec access to this movie texture resource from the render thread */
var native const transient pointer ReleaseCodecFence{FRenderCommandFence};

/** Select streaming movie from memory or from file for playback */
var() enum EMovieStreamSource
{
	/** stream directly from file */
	MovieStream_File,
	/** load movie contents to memory */
	MovieStream_Memory,
} MovieStreamSource;

/** Plays the movie and also unpauses. */
native function Play();
/** Pauses the movie. */	
native function Pause();
/** Stops movie playback. */
native function Stop();

cpptext
{
	// USurface interface
	
	/**
	* @return width of surface
	*/
	virtual FLOAT GetSurfaceWidth() const { return SizeX; }

	/**
	* @return height of surface
	*/
	virtual FLOAT GetSurfaceHeight() const { return SizeY; }

	// UTexture interface

	/**
	* Create a new movie texture resource
	*
	* @return newly created FTextureMovieResource
	*/
	virtual FTextureResource* CreateResource();

	/**
	* Materials should treat a movie texture like a regular 2D texture resource.
	*
	* @return EMaterialValueType for this resource
	*/
	virtual EMaterialValueType GetMaterialType();

	// UObject interface.
	
	/**
	 * Serializes the compressed movie data.
	 *
	 * @param Ar	FArchive to serialize RawData with.
	 */
	virtual void Serialize(FArchive& Ar);	
	
	/**
	 * Postload initialization of movie texture. Creates decoder object and retriever first frame.
	 */
	virtual void PostLoad();
	
	/**
	* Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	* asynchronous cleanup process.
	*
	* We need to ensure that the decoder doesn't have any references to the movie texture resource before destructing it.
	*/
	virtual void BeginDestroy();
	
	/**
	* Called when a property on this object has been modified externally
	*
	* @param PropertyThatChanged the property that will be modified
	*/
	virtual void PreEditChange(UProperty* PropertyAboutToChange);

	/**
	* Called when a property on this object has been modified externally
	*
	* @param PropertyThatChanged the property that was modified
	*/
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/**
	* Called to check if the object is ready for FinishDestroy.  This is called after BeginDestroy to check the completion of the
	* potentially asynchronous object cleanup.
	* @return True if the object's asynchronous cleanup has completed and it is ready for FinishDestroy to be called.
	*/
	virtual UBOOL IsReadyForFinishDestroy();

	/**
	 * Called to finish destroying the object.  After UObject::FinishDestroy is called, the object's memory should no longer be accessed.
	 *
	 * note: because ExitProperties() is called here, Super::FinishDestroy() should always be called at the end of your child class's
	 * FinishDestroy() method, rather than at the beginning.
	 */
	virtual void FinishDestroy();

	// Thumbnail interface.

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	/**
	 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
	 *
	 * @return size of resource as to be displayed to artists/ LDs in the Editor.
	 */
	virtual INT GetResourceSize();

	// UTextureMovie

	/**
	 * Access the movie target resource for this movie texture object
	 * @return pointer to resource or NULL if not initialized
	 */
	class FTextureMovieResource* GetTextureMovieResource();
	
	/** 
	 * Creates a new codec and checks to see if it has a valid stream
	 */
	void InitDecoder();
}

defaultproperties
{
	MovieStreamSource=MovieStream_Memory
	DecoderClass=class'CodecMovieFallback'
	Looping=True
	AutoPlay=True
	NeverStream=True
}
