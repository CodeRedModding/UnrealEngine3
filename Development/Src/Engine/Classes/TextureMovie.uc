/** 
 * TextureMovie
 * Movie texture support base class.
 *
 * Created by:	Daniel Vogel
 * Copyright :	(c) 2004
 * Company   :	Epic Games, Inc.
 */

class TextureMovie extends Texture
	native
	noexport
	hidecategories(Object);

// Virtual function tables.
var		native	const	noexport	pointer				FTexture2DVfTable;
var		native	const	noexport	pointer				FTickableObjectVfTable;

// FResource
var		native	const				int					ResourceIndex;
var		native	const				int					Dynamic;

// FSurface
var		native	const				int					SizeX;
var		native	const				int					SizeY;
var		native	const				byte				Format;

// FTextureBase
var		native	const				int					SizeZ;
var		native	const				int					NumMips;
var		native	const				int					CurrentMips;


// TextureMovie

/** Whether to clamp the texture horizontally. Note that non power of two textures can't be clamped.					*/
var()								bool				ClampX;
/** Whether to clamp the texture vertically. Note that non power of two textures can't be clamped.						*/
var()								bool				ClampY;					

/** Class type of Decoder that will be used to decode RawData.															*/
var				const				class<CodecMovie>	DecoderClass;
/** Instance of decoder.																								*/
var				const	transient	CodecMovie			Decoder;

/** Time into the movie in seconds.																						*/
var				const	transient	float				TimeIntoMovie;
/** Time that has passed since the last frame. Will be adjusted by decoder to combat drift.								*/
var				const	transient	float				TimeSinceLastFrame;

/** Whether the movie is currently paused.																				*/
var				const				bool				Paused;
/** Whether the movie is currently stopped.																				*/
var				const				bool				Stopped;
/** Whether the movie should loop when it reaches the end.																*/
var()								bool				Looping;
/** Whether the movie should automatically start playing when it is loaded.												*/
var()								bool				AutoPlay;

/** Used to hold the currently decoded frame. Requires syncronization as both the decoder and this object access it.	*/
var		native	const				array<byte>			CurrentFrame;
/** Raw data containing raw compressed data as imported. Never call unload on this lazy array!							*/
var		native	const				LazyArray_Mirror	RawData;
/** Size of raw data, needed for streaming directly from disk															*/
var				const				int					RawSize;

/** Plays the movie and also unpauses.																					*/
native function Play();
/** Pauses the movie.																									*/	
native function Pause();
/** Stops movie playback.																								*/
native function Stop();

defaultproperties
{
	DecoderClass=class'CodecMovieFallback'
	Stopped=true
	Looping=true
	AutoPlay=true
}
