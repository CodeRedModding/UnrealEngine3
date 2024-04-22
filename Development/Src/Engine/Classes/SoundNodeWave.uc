/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 * Sound node that contains sample data
 */

class SoundNodeWave extends SoundNode
	PerObjectConfig
	native( Sound )
	dependson( AudioDevice )
	hidecategories( Object )
	editinlinenew;

enum EDecompressionType
{
	DTYPE_Setup,
	DTYPE_Invalid,
	DTYPE_Preview,
	DTYPE_Native,
	DTYPE_RealTime,
	DTYPE_Procedural,
	DTYPE_Xenon
};

/** Platform agnostic compression quality. 1..100 with 1 being best compression and 100 being best quality */
var( Compression )		int								CompressionQuality<Tooltip=1 smallest size, 100 is best quality.>;
/** If set, forces wave data to be decompressed during playback instead of upfront on platforms that have a choice. */
var( Compression )		bool							bForceRealTimeDecompression<Tooltip=Forces on the fly sound decompression, even for short duration sounds.>;
/** If set, the compressor does everything required to make this a seamlessly looping sound */
var( Compression )		bool							bLoopingSound<Tooltip=Informs the compression routimes to process this sound to allow it to loop.>;

/** Whether to free the resource data after it has been uploaded to the hardware */
var		transient const bool							bDynamicResource;

/** Set to true to speak SpokenText using TTS */
var( TTS )	bool										bUseTTS<Tooltip=Use Text To Speech to verbalise SpokenText.>;
/** Speaker to use for TTS */
var( TTS )	ETTSSpeaker									TTSSpeaker<Tooltip=The voice to use for Text To Speech.>;
/** A localized version of the text that is actually spoken in the audio. */
var( TTS )	localized string							SpokenText<ToolTip=The phonetic version of the dialog.>;

/** Set to true for programmatically-generated, streamed audio. Not used from the editor; you should use SoundNodeWaveStreaming.uc for this. */
var	transient bool										bProcedural;

/** Playback volume of sound 0 to 1 */
var( Info )	editconst const	float						Volume<Tooltip=Default is 0.75.>;
/** Playback pitch for sound 0.4 to 2.0 */
var( Info )	editconst const	float						Pitch<Tooltip=Minimum is 0.4, maximum is 2.0 - it is a simple linear multiplier to the SampleRate.>;
/** Duration of sound in seconds. */
var( Info )	editconst const	float						Duration;
/** Number of channels of multichannel data; 1 or 2 for regular mono and stereo files */
var( Info )	editconst const	int							NumChannels;
/** Cached sample rate for displaying in the tools */
var( Info )	editconst const int							SampleRate;

/** Offsets into the bulk data for the source wav data */
var	editoronly   const	array<int>						ChannelOffsets;
/** Sizes of the bulk data for the source wav data */
var	editoronly   const	array<int>						ChannelSizes;
/** Uncompressed wav data 16 bit in mono or stereo - stereo not allowed for multichannel data */
var		native const	UntypedBulkData_Mirror			RawData{FByteBulkData};

/** Type of buffer this wave uses. Set once on load */
var		transient const	EDecompressionType				DecompressionType;
/** Async worker that decompresses the vorbis data on a different thread */
var		native const pointer							VorbisDecompressor{FAsyncVorbisDecompress};
/** Pointer to 16 bit PCM data - used to decompress data to and preview sounds */
var		native const	pointer							RawPCMData{BYTE};
/** Size of RawPCMData, or what RawPCMData would be if the sound was fully decompressed */
var			   const int								RawPCMDataSize;

/** Cached ogg vorbis data. */
var		native const	UntypedBulkData_Mirror			CompressedPCData{FByteBulkData};
/** Cached cooked Xbox 360 data to speed up iteration times. */
var		native const	UntypedBulkData_Mirror			CompressedXbox360Data{FByteBulkData};
/** Cached cooked PS3 data to speed up iteration times. */
var		native const	UntypedBulkData_Mirror			CompressedPS3Data{FByteBulkData};
/** Cached cooked WiiU data to speed up iteration times. */
var		native const	UntypedBulkData_Mirror			CompressedWiiUData{FByteBulkData};
/** Cached cooked IPhone data to speed up iteration times. */
var		native const	UntypedBulkData_Mirror			CompressedIPhoneData{FByteBulkData};
/** Cached cooked Flash data to speed up iteration times. */
var		native const	UntypedBulkData_Mirror			CompressedFlashData{FByteBulkData};

/** Resource index to cross reference with buffers */
var		transient const int								ResourceID;
/** Size of resource copied from the bulk data */
var		transient const int								ResourceSize;
/** Memory containing the data copied from the compressed bulk data */
var		native const pointer							ResourceData{BYTE};

/**
 * Subtitle cues.  If empty, use SpokenText as the subtitle.  Will often be empty,
 * as the contents of the subtitle is commonly identical to what is spoken.
 */
var( Subtitles )	localized array<SubtitleCue>		Subtitles;

/** TRUE if this sound is considered to contain mature content. */
var( Subtitles )	localized bool						bMature<ToolTip=For marking any adult language.>;

/** Provides contextual information for the sound to the translator. */
var( Subtitles )	editoronly localized string			Comment<ToolTip=Contextual information for the sound to the translator.>;

/** TRUE if the subtitles have been split manually. */
var( Subtitles )	localized bool						bManualWordWrap<ToolTip=Disable automatic generation of line breaks.>;

/** TRUE if the subtitles display as a sequence of single lines as opposed to multiline */
var( Subtitles )	localized bool						bSingleLine<ToolTip=Display the subtitle as a sequence of single lines.>;

/**
 * The array of the subtitles for each language. Generated at cook time.
 * The index for a specific language extenstion can be retrieved via the Localization_GetLanguageExtensionIndex function in UnMisc.cpp.
 */
var array<LocalizedSubtitle> 							LocalizedSubtitles;

/** if on mobile and the platform's DetailMode < this value, the sound will be discarded to conserve memory */
var() const EDetailMode MobileDetailMode;

/** Path to the resource used to construct this sound node wave */
var() const editconst editoronly string					SourceFilePath;

/** Date/Time-stamp of the file from the last import */
var() const editconst editoronly string					SourceFileTimestamp;
/**
 * This is only for DTYPE_Procedural audio. Override this function.
 *  Put SamplesNeeded PCM samples into Buffer. If put less,
 *  silence will be filled in at the end of the buffer. If you
 *  put more, data will be truncated.
 * Please note that "samples" means individual channels, not
 *  sample frames! If you have stereo data and SamplesNeeded
 *  is 1, you're writing two SWORDs, not four!
 * Due to UnrealScript limitations, this is an array<byte>, but
 *  you should supply 16-bit, signed data.
 */
event GeneratePCMData(out Array<byte> Buffer, int SamplesNeeded)
{
	// no-op; override this method!
}

defaultproperties
{
	Volume=0.75
	Pitch=1.0
	CompressionQuality=40
	bLoopingSound=TRUE
}

