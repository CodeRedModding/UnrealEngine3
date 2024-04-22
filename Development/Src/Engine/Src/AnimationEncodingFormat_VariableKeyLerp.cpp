/*=============================================================================
	AnimationEncodingFormat_VariableKeyLerp.cpp: Skeletal mesh animation functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "AnimationEncodingFormat_VariableKeyLerp.h"
#include "AnimationUtils.h"

/**
 * Handles the ByteSwap of compressed rotation data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 * @param	SourceArVersion	The version number of the source archive stream.
 */
void AEFVariableKeyLerpShared::ByteSwapRotationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	BYTE*& TrackData,
	INT NumKeys,
	INT SourceArVersion)
{
	AEFConstantKeyLerpShared::ByteSwapRotationIn(Seq, MemoryReader, TrackData, NumKeys, SourceArVersion);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(WORD) : sizeof(BYTE);
		for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed translation data on import
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryReader	The FMemoryReader to read from.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys present in the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapTranslationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	BYTE*& TrackData,
	INT NumKeys,
	INT SourceArVersion)
{
	AEFConstantKeyLerpShared::ByteSwapTranslationIn(Seq, MemoryReader, TrackData, NumKeys, SourceArVersion);

	// Load the track table if present
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryReader(&MemoryReader, TrackData, 4); 

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(WORD) : sizeof(BYTE);
		for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed rotation data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapRotationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	BYTE*& TrackData,
	INT NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapRotationOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(WORD) : sizeof(BYTE);
		for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}

/**
 * Handles the ByteSwap of compressed translation data on export
 *
 * @param	Seq				The UAnimSequence container.
 * @param	MemoryWriter	The FMemoryWriter to write to.
 * @param	TrackData		The compressed data stream.
 * @param	NumKeys			The number of keys to write to the stream.
 */
void AEFVariableKeyLerpShared::ByteSwapTranslationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	BYTE*& TrackData,
	INT NumKeys)
{
	AEFConstantKeyLerpShared::ByteSwapTranslationOut(Seq, MemoryWriter, TrackData, NumKeys);

	// Store the track table if needed
	if (NumKeys > 1)
	{
		// Like the compressed byte stream, pad the serialization stream to four bytes.
		// As a sanity check, each pad byte can be checked to be the PadSentinel.
		PadMemoryWriter(&MemoryWriter, TrackData, 4);

		// swap the track table
		const size_t EntryStride = (Seq.NumFrames > 0xFF) ? sizeof(WORD) : sizeof(BYTE);
		for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, EntryStride);
		}
	}
}

