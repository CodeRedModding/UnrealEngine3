/*=============================================================================
	UnCodecBink.h: Bink movie codec definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef CODEC_BINK_H
#define CODEC_BINK_H

#include "BinkHeaders.h"

class UCodecMovieBink : public UCodecMovie
{
	DECLARE_CLASS_INTRINSIC(UCodecMovieBink,UCodecMovie,0|CLASS_Transient,Engine)

	/**
	* Constructor
	*/
	UCodecMovieBink();

	// UCodecMovie interface.

	/**
	* Not all codec implementations are available
	*
	* @return TRUE if the current codec is supported
	*/
	virtual UBOOL IsSupported();

	/**
	* Returns the movie width.
	*
	* @return width of movie.
	*/
	virtual UINT GetSizeX();
	/**
	* Returns the movie height.
	*
	* @return height of movie.
	*/
	virtual UINT GetSizeY();
	/** 
	* Returns the movie format,
	*
	* @return format of movie.
	*/
	virtual EPixelFormat GetFormat();
	/**
	* Returns the framerate the movie was encoded at.
	*
	* @return framerate the movie was encoded at.
	*/
	virtual FLOAT GetFrameRate();

	/**
	* Initializes the decoder to stream from disk.
	*
	* @param	Filename	Filename of compressed media.
	* @param	Offset		Offset into file to look for the beginning of the compressed data.
	* @param	Size		Size of compressed data.
	*
	* @return	TRUE if initialization was successful, FALSE otherwise.
	*/
	virtual UBOOL Open( const FString& Filename, DWORD Offset, DWORD Size );
	/**
	* Initializes the decoder to stream from memory.
	*
	* @param	Source		Beginning of memory block holding compressed data.
	* @param	Size		Size of memory block.
	*
	* @return	TRUE if initialization was successful, FALSE otherwise.
	*/
	virtual UBOOL Open( void* Source, DWORD Size );
	/**
	* Tears down stream, closing file handle if there was an open one.	
	*/
	virtual void Close();

	/**
	* Resets the stream to its initial state so it can be played again from the beginning.
	*/
	virtual void ResetStream();
	/**
	* Queues the request to retrieve the next frame.
	*
	* @param InTextureMovieResource - output from movie decoding is written to this resource
	*/
	virtual void GetFrame( class FTextureMovieResource* InTextureMovieResource );

	/** 
	* Begin playback of the movie stream 
	* (This is only called by the rendering thread)
	*
	* @param bLooping - if TRUE then the movie loops back to the start when finished
	* @param bOneFrameOnly - if TRUE then the decoding is paused after the first frame is processed 
	* @param bResetOnLastFrame - if TRUE then the movie frame is set to 0 when playback finishes
	*/
	virtual void Play(UBOOL bLooping, UBOOL bOneFrameOnly, UBOOL bResetOnLastFrame);

	/** 
	* Pause or resume the movie playback.
	* (This is only called by the rendering thread)
	*
	* @param bPause - if TRUE then decoding will be paused otherwise it resumes
	*/
	virtual void Pause(UBOOL bPause);

	/**
	* Stop playback from the movie stream
	* (This is only called by the rendering thread)
	*/ 
	virtual void Stop();

	/**
	* Release any dynamic rendering resources created by this codec
	* (This is only called by the rendering thread)
	*/
	virtual void ReleaseDynamicResources();

	// UObject interface.

	/**
	* Called before destroying the object.  This is called immediately upon deciding to destroy the object, to allow the object to begin an
	* asynchronous cleanup process.
	*
	* We need to ensure that the decoder doesn't have any references to the movie texture resource before destructing it.
	*/
	virtual void BeginDestroy();

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

private:
	/** 
	* Accessor for current BINK info 
	*/
	FORCEINLINE BINK* GetBinkInfo() { check(Bink); return (BINK*)Bink; }

	/**
	* Do the decoding for the next movie frame
	*/
	void DecodeFrame();

	/**
	* Render the current movie frame to the target movie texture 
	*
	* @param InTextureMovieResource - output from movie decoding is written to this resource
	*/
	void RenderFrame( class FTextureMovieResource* InTextureMovieResource );

	/** 
	* Creates the internal bink frame buffer textures
	*/
	struct FInternalBinkTextures
	{
		FInternalBinkTextures() : bInitialized(FALSE) {}
		void Init(HBINK Bink);
		void Clear();

		/** internal frame buffers created for bink decoding. These are also used to render to the final surface */
		FBinkTextureSet* TextureSet;
	
		/** TRUE if internal bink frame buffer textures have been initialized */
		UBOOL bInitialized;
	};
	FInternalBinkTextures BinkTextures;

	/**
	* Creates the shaders needed to copy the YUV texture results to a target
	*/
	struct FInternalBinkShaders
	{
		FInternalBinkShaders() : bInitialized(FALSE) {}
		~FInternalBinkShaders() { Clear(); }
		void Init();
		void Clear();

		/** TRUE if the internal bink shaders have been initialized. */
		UBOOL bInitialized;
	};
	static FInternalBinkShaders BinkShaders;

	/** Bink handle for use by all the Bink API functions. Also represents a ptr to the BINK struct */
	HBINK Bink;	

	/** Handle to file we're streaming from, NULL if streaming directly from memory */
	void* FileHandle;
	/** Pointer to memory we're streaming from, NULL if streaming from disk */
	BYTE* MemorySource;

	/** if TRUE pause playback after decoding the next frame */
	UBOOL bPauseNextFrame;
	/** if TRUE loop to start of movie after end of playback */
	UBOOL bIsLooping;
	/** if TRUE the movie should reset to first frame when it reaches the end */
	UBOOL bResetOnLastFrame;

	/** fence to flag ready for destroy */
	FRenderCommandFence* DestroyFence;
};

#endif //CODEC_BINK_H




