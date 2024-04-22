/*=============================================================================
	AnimationEncodingFormat_ConstantKeyLerp.cpp: Skeletal mesh animation functions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "AnimationEncodingFormat_ConstantKeyLerp.h"
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
void AEFConstantKeyLerpShared::ByteSwapRotationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	BYTE*& TrackData,
	INT NumKeys,
	INT SourceArVersion)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const INT EffectiveFormat = (NumKeys == 1) ? ACF_Float96NoW : Seq.RotationCompressionFormat;
	const INT KeyComponentSize = CompressedRotationStrides[EffectiveFormat];
	const INT KeyNumComponents = CompressedRotationNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (INT i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(FLOAT));
		}
	}

	// Load the keys
	for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (INT i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapTranslationIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	BYTE*& TrackData,
	INT NumKeys,
	INT SourceArVersion)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const INT EffectiveFormat = (NumKeys == 1) ? ACF_None : Seq.TranslationCompressionFormat;
	const INT KeyComponentSize = CompressedTranslationStrides[EffectiveFormat];
	const INT KeyNumComponents = CompressedTranslationNum[EffectiveFormat];

	// Load the bounds if present
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (INT i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, sizeof(FLOAT));
		}
	}

	// Load the keys
	for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (INT i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryReader, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapRotationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	BYTE*& TrackData,
	INT NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const INT EffectiveFormat = (NumKeys == 1) ? ACF_Float96NoW : Seq.RotationCompressionFormat;
	const INT KeyComponentSize = CompressedRotationStrides[EffectiveFormat];
	const INT KeyNumComponents = CompressedRotationNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (INT i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(FLOAT));
		}
	}

	// Store the keys
	for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (INT i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
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
void AEFConstantKeyLerpShared::ByteSwapTranslationOut(
	UAnimSequence& Seq, 
	FMemoryWriter& MemoryWriter,
	BYTE*& TrackData,
	INT NumKeys)
{
	// Calculate the effective compression (in a track with only one key, it's always stored lossless)
	const INT EffectiveFormat = (NumKeys == 1) ? ACF_None : Seq.TranslationCompressionFormat;
	const INT KeyComponentSize = CompressedTranslationStrides[EffectiveFormat];
	const INT KeyNumComponents = CompressedTranslationNum[EffectiveFormat];

	// Store the bounds if needed
	if (EffectiveFormat == ACF_IntervalFixed32NoW)
	{
		for (INT i = 0; i < 6; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, sizeof(FLOAT));
		}
	}

	// Store the keys
	for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
	{
		for (INT i = 0; i < KeyNumComponents; ++i)
		{
			AC_UnalignedSwap(MemoryWriter, TrackData, KeyComponentSize);
		}
	}
}
