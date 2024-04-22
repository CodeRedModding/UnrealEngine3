/*=============================================================================
	AnimationEncodingFormat_PerTrackCompression.cpp: Per-track decompressor
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "AnimationCompression.h"
#include "AnimationEncodingFormat_PerTrackCompression.h"
#include "AnimationUtils.h"

// This define controls whether scalar or vector code is used to decompress keys.  Note that not all key decompression code
// is vectorized yet, so some (seldom used) formats will actually get slower (due to extra LHS stalls) when enabled.
// The code also relies on a flexible permute instruction being available (e.g., PPC vperm)
#define USE_VECTOR_PTC_DECOMPRESSOR XBOX

#if USE_VECTOR_PTC_DECOMPRESSOR

// 32767, stored in 1X, plus biasing is 0
// Perm_Zeros takes the first 4 bytes of the second argument (which should be VectorZero() for this table use)
#define Perm_Zeros 0x10111213

#define Perm_X1 0x00010203
#define Perm_Y1 0x04050607
#define Perm_Z1 0x08090A0B
#define Perm_W2 0x1C1D1E1F

// One float96 key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-3) plus one indicates how many bytes a key contains
static const BYTE Float96KeyBytesMinusOne[16] = { 11, 3, 3, 7, 3, 7, 7, 11, 11, 3, 3, 7, 3, 7, 7, 11 };

// One fixed48 key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-3) plus one indicates how many bytes a key contains
static const BYTE Fixed48KeyBytesMinusOne[16] = { 5, 1, 1, 3, 1, 3, 3, 5, 5, 1, 1, 3, 1, 3, 3, 5 };

// One float96 translation key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-2) indicates how to swizzle the 16 bytes read into one float each in X,Y,Z, and 0.0f in W
static const VectorRegister Trans96OptionalFormatPermMasks[8] =
{
	MakeVectorRegister( Perm_X1, Perm_Y1, Perm_Z1, (DWORD)Perm_W2 ),  // 0 = 7 = all three valid
	MakeVectorRegister( Perm_X1, Perm_W2, Perm_W2, (DWORD)Perm_W2 ),  // 1 = 001 = X
	MakeVectorRegister( Perm_W2, Perm_X1, Perm_W2, (DWORD)Perm_W2 ),  // 2 = 010 = Y
	MakeVectorRegister( Perm_X1, Perm_Y1, Perm_W2, (DWORD)Perm_W2 ),  // 3 = 011 = XY
	MakeVectorRegister( Perm_W2, Perm_W2, Perm_X1, (DWORD)Perm_W2 ),  // 4 = 100 = Z
	MakeVectorRegister( Perm_X1, Perm_W2, Perm_Y1, (DWORD)Perm_W2 ),  // 5 = 101 = XZ
	MakeVectorRegister( Perm_W2, Perm_X1, Perm_Y1, (DWORD)Perm_W2 ),  // 6 = 110 = YZ
	MakeVectorRegister( Perm_X1, Perm_Y1, Perm_Z1, (DWORD)Perm_W2 ),  // 7 = 111 = XYZ
};

#undef Perm_Zeros
#undef Perm_X1
#undef Perm_Y1
#undef Perm_Z1
#undef Perm_W2

// Perm_Zeros takes the first 4 bytes of the second argument (which should be DecompressPTCConstants, resulting in integer 32767, which becomes 0.0f after scaling and biasing)
#define Perm_Zeros 0x10111213

// Each of these takes two bytes of source data, and two zeros from the W of the second argument (which should be DecompressPTCConstants) to pad it out to a 4 byte INT
#define Perm_Data1 0x1F1F0001
#define Perm_Data2 0x1F1F0203
#define Perm_Data3 0x1F1F0405

// One fixed48 rotation key can be stored with implicit 0.0f components, this table (when indexed by format flags 0-2) indicates how to swizzle the 16 bytes read into one short each in X,Y,Z)
static const VectorRegister Fix48FormatPermMasks[8] =
{
	MakeVectorRegister( Perm_Data1, Perm_Data2, Perm_Data3, (DWORD)Perm_Zeros ),  // 0 = 7 = all three valid
	MakeVectorRegister( Perm_Data1, Perm_Zeros, Perm_Zeros, (DWORD)Perm_Zeros ),  // 1 = 001 = X
	MakeVectorRegister( Perm_Zeros, Perm_Data1, Perm_Zeros, (DWORD)Perm_Zeros ),  // 2 = 010 = Y
	MakeVectorRegister( Perm_Data1, Perm_Data2, Perm_Zeros, (DWORD)Perm_Zeros ),  // 3 = 011 = XY
	MakeVectorRegister( Perm_Zeros, Perm_Zeros, Perm_Data1, (DWORD)Perm_Zeros ),  // 4 = 100 = Z
	MakeVectorRegister( Perm_Data1, Perm_Zeros, Perm_Data2, (DWORD)Perm_Zeros ),  // 5 = 101 = XZ
	MakeVectorRegister( Perm_Zeros, Perm_Data1, Perm_Data2, (DWORD)Perm_Zeros ),  // 6 = 110 = YZ
	MakeVectorRegister( Perm_Data1, Perm_Data2, Perm_Data3, (DWORD)Perm_Zeros ),  // 7 = 111 = XYZ
};

#undef Perm_Zeros
#undef Perm_Data1
#undef Perm_Data2
#undef Perm_Data3

// Constants used when decompressing fixed48 translation keys (pre-scale, pre-bias integer representation of a final output 0.0f)
static const VectorRegister DecompressPTCTransConstants = MakeVectorRegister( 255, 255, 255, (DWORD)0 );

// Constants used when decompressing fixed48 rotation keys (pre-scale, pre-bias integer representation of a final output 0.0f)
static const VectorRegister DecompressPTCConstants = MakeVectorRegister( 32767, 32767, 32767, (DWORD)0 );

// Scale-bias factors for decompressing fixed48 data (both rotation and translation)
static const VectorRegister BiasFix48Data = MakeVectorRegister( -32767.0f, -32767.0f, -32767.0f, -32767.0f );
static const VectorRegister ScaleRotData = MakeVectorRegister( 3.0518509475997192297128208258309e-5f, 3.0518509475997192297128208258309e-5f, 3.0518509475997192297128208258309e-5f, 1.0f );

//@TODO: Looks like fixed48 for translation is basically broken right now (using 8 bits instead of 16 bits!).  The scale is omitted below because it's all 1's
static const VectorRegister BiasTransData =  MakeVectorRegister( -255.0f, -255.0f, -255.0f, -255.0f );
static const VectorRegister ScaleTransData = MakeVectorRegister( 1.0f, 1.0f, 1.0f, 1.0f );

/** Decompress a single translation key from a single track that was compressed with the PerTrack codec (vectorized) */
static FORCEINLINE_DEBUGGABLE VectorRegister DecompressSingleTrackTranslationVectorized(INT Format, INT FormatFlags, const BYTE* RESTRICT TopOfStream, const BYTE* RESTRICT KeyData)
{
	if( Format == ACF_Float96NoW )
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Float96KeyBytesMinusOne[FormatFlags]);
		const VectorRegister XYZ = VectorPermute(KeyJumbled, VectorZero(), Trans96OptionalFormatPermMasks[FormatFlags & 7]);

		return XYZ;
	}
	else if (Format == ACF_Fixed48NoW)
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Fixed48KeyBytesMinusOne[FormatFlags]);
		const VectorRegister Key = VectorPermute(KeyJumbled, DecompressPTCTransConstants, Fix48FormatPermMasks[FormatFlags & 7]);
		const VectorRegister FPKey = VectorUitof(Key);

		const VectorRegister BiasedData = VectorAdd(FPKey, BiasTransData);
		//const VectorRegister XYZ = VectorMultiply(BiasedData, ScaleTransData);
		const VectorRegister XYZ = BiasedData;

		return XYZ;
	}
	else if (Format == ACF_IntervalFixed32NoW)
	{
		const FLOAT* RESTRICT SourceBounds = (FLOAT*)TopOfStream;

		FLOAT Mins[3] = {0.0f, 0.0f, 0.0f};
		FLOAT Ranges[3] = {0.0f, 0.0f, 0.0f};

		if (FormatFlags & 1)
		{
			Mins[0] = *SourceBounds++;
			Ranges[0] = *SourceBounds++;
		}
		if (FormatFlags & 2)
		{
			Mins[1] = *SourceBounds++;
			Ranges[1] = *SourceBounds++;
		}
		if (FormatFlags & 4)
		{
			Mins[2] = *SourceBounds++;
			Ranges[2] = *SourceBounds++;
		}

		// This one is still used for ~4% of the cases, so making it faster would be nice
		FVector Out;
		((FVectorIntervalFixed32NoW*)KeyData)->ToVector(Out, Mins, Ranges);
		return VectorLoadAligned(&Out);
	}

	return VectorZero();
}

/** Decompress a single rotation key from a single track that was compressed with the PerTrack codec (vectorized) */
static FORCEINLINE_DEBUGGABLE VectorRegister DecompressSingleTrackRotationVectorized(INT Format, INT FormatFlags, const BYTE* RESTRICT TopOfStream, const BYTE* RESTRICT KeyData)
{
	if (Format == ACF_Fixed48NoW)
	{
		const VectorRegister KeyJumbled = VectorLoadNPlusOneUnalignedBytes(KeyData, Fixed48KeyBytesMinusOne[FormatFlags]);
		const VectorRegister Key = VectorPermute(KeyJumbled, DecompressPTCConstants, Fix48FormatPermMasks[FormatFlags & 7]);
		const VectorRegister FPKey = VectorUitof(Key);

		const VectorRegister BiasedData = VectorAdd(FPKey, BiasFix48Data);
		const VectorRegister XYZ = VectorMultiply(BiasedData, ScaleRotData);
		const VectorRegister LengthSquared = VectorDot3(XYZ, XYZ);

		const VectorRegister WSquared = VectorSubtract(VectorOne(), LengthSquared);
		const VectorRegister WSquaredSqrt = VectorReciprocal(VectorReciprocalSqrt(WSquared));
		const VectorRegister WWWW = VectorSelect(VectorCompareGT(WSquared, VectorZero()), WSquaredSqrt, VectorZero());

		const VectorRegister XYZW = VectorMergeVecXYZ_VecW(XYZ, WWWW);

		return XYZW;
	}
	else if (Format == ACF_Float96NoW)
	{
		const VectorRegister XYZ = VectorLoadFloat3(KeyData);
		const VectorRegister LengthSquared = VectorDot3(XYZ, XYZ);

		const VectorRegister WSquared = VectorSubtract(VectorOne(), LengthSquared);
		const VectorRegister WSquaredSqrt = VectorReciprocal(VectorReciprocalSqrt(WSquared));
		const VectorRegister WWWW = VectorSelect(VectorCompareGT(WSquared, VectorZero()), WSquaredSqrt, VectorZero());

		const VectorRegister XYZW = VectorMergeVecXYZ_VecW(XYZ, WWWW);

		return XYZW;
	}
	else if ( Format == ACF_IntervalFixed32NoW )
	{
		const FLOAT* RESTRICT SourceBounds = (FLOAT*)TopOfStream;

		FLOAT Mins[3] = {0.0f, 0.0f, 0.0f};
		FLOAT Ranges[3] = {0.0f, 0.0f, 0.0f};

		if (FormatFlags & 1)
		{
			Mins[0] = *SourceBounds++;
			Ranges[0] = *SourceBounds++;
		}
		if (FormatFlags & 2)
		{
			Mins[1] = *SourceBounds++;
			Ranges[1] = *SourceBounds++;
		}
		if (FormatFlags & 4)
		{
			Mins[2] = *SourceBounds++;
			Ranges[2] = *SourceBounds++;
		}

		// This one is still used for ~4% of the cases, so making it faster would be nice
		FQuat Out;
		((FQuatIntervalFixed32NoW*)KeyData)->ToQuat( Out, Mins, Ranges );
		return VectorLoadAligned(&Out);
	}
	else if ( Format == ACF_Float32NoW )
	{
		// This isn't used for compression anymore so making it fast isn't very important
		FQuat Out;
		((FQuatFloat32NoW*)KeyData)->ToQuat( Out );
		return VectorLoadAligned(&Out);
	}
	else if (Format == ACF_Fixed32NoW)
	{
		// This isn't used for compression anymore so making it fast isn't very important
		FQuat Out;
		((FQuatFixed32NoW*)KeyData)->ToQuat(Out);
		return VectorLoadAligned(&Out);
	}

	return VectorLoadAligned(&(FQuat::Identity));
}

#endif // USE_VECTOR_PTC_DECOMPRESSOR

/**
 * Handles Byte-swapping a single track of animation data from a MemoryReader or to a MemoryWriter
 *
 * @param	Seq					The Animation Sequence being operated on.
 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
 * @param	Offset				The starting offset into the compressed byte stream for this track (can be INDEX_NONE to indicate an identity track)
 */
void AEFPerTrackCompressionCodec::ByteSwapOneTrack(UAnimSequence& Seq, FMemoryArchive& MemoryStream, INT Offset)
{
	// Translation data.
	if (Offset != INDEX_NONE)
	{
		checkSlow( (Offset % 4) == 0 && "CompressedByteStream not aligned to four bytes" );

		BYTE* TrackData = Seq.CompressedByteStream.GetTypedData() + Offset;

		// Read the header
		AC_UnalignedSwap(MemoryStream, TrackData, sizeof(INT));

		const INT Header = *(reinterpret_cast<INT*>(TrackData - sizeof(INT)));


		INT KeyFormat;
		INT NumKeys;
		INT FormatFlags;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags);

		INT FixedComponentSize = 0;
		INT FixedComponentCount = 0;
		INT KeyComponentSize = 0;
		INT KeyComponentCount = 0;
		FAnimationCompression_PerTrackUtils::GetAllSizesFromFormat(KeyFormat, FormatFlags, /*OUT*/ KeyComponentCount, /*OUT*/ KeyComponentSize, /*OUT*/ FixedComponentCount, /*OUT*/ FixedComponentSize);

		// Handle per-track metadata
		for (INT i = 0; i < FixedComponentCount; ++i)
		{
			AC_UnalignedSwap(MemoryStream, TrackData, FixedComponentSize);
		}

		// Handle keys
		for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			for (INT i = 0; i < KeyComponentCount; ++i)
			{
				AC_UnalignedSwap(MemoryStream, TrackData, KeyComponentSize);
			}
		}

		// Handle the key frame table if present
		if ((FormatFlags & 0x8) != 0)
		{
			// Make sure the key->frame table is 4 byte aligned
			PreservePadding(TrackData, MemoryStream);

			const INT FrameTableEntrySize = (Seq.NumFrames <= 0xFF) ? sizeof(BYTE) : sizeof(WORD);
			for (INT i = 0; i < NumKeys; ++i)
			{
				AC_UnalignedSwap(MemoryStream, TrackData, FrameTableEntrySize);
			}
		}

		// Make sure the next track is 4 byte aligned
		PreservePadding(TrackData, MemoryStream);
	}
}

/**
 * Preserves 4 byte alignment within a stream
 *
 * @param	TrackData [inout]	The current data offset (will be returned four byte aligned from the start of the compressed byte stream)
 * @param	MemoryStream		The MemoryReader or MemoryWriter object to read from/write to.
 */
void AEFPerTrackCompressionCodec::PreservePadding(BYTE*& TrackData, FMemoryArchive& MemoryStream)
{
	// Preserve padding
	const PTRINT ByteStreamLoc = (PTRINT) TrackData;
	const INT PadCount = static_cast<INT>( Align(ByteStreamLoc, 4) - ByteStreamLoc );
	if (MemoryStream.IsSaving())
	{
		const BYTE PadSentinel = 85; // (1<<1)+(1<<3)+(1<<5)+(1<<7)

		for (INT PadByteIndex = 0; PadByteIndex < PadCount; ++PadByteIndex)
		{
			MemoryStream.Serialize( (void*)&PadSentinel, sizeof(BYTE) );
		}
		TrackData += PadCount;
	}
	else
	{
		MemoryStream.Serialize(TrackData, PadCount);
		TrackData += PadCount;
	}
}

/**
 * Handles Byte-swapping incoming animation data from a MemoryReader
 *
 * @param	Seq					An Animation Sequence to contain the read data.
 * @param	MemoryReader		The MemoryReader object to read from.
 * @param	SourceArVersion		The version of the archive that the data is coming from.
 */
void AEFPerTrackCompressionCodec::ByteSwapIn(
	UAnimSequence& Seq, 
	FMemoryReader& MemoryReader,
	INT SourceArVersion)
{
	INT OriginalNumBytes = MemoryReader.TotalSize();
	Seq.CompressedByteStream.Empty(OriginalNumBytes);
	Seq.CompressedByteStream.Add(OriginalNumBytes);

	const INT NumTracks = Seq.CompressedTrackOffsets.Num() / 2;
	for ( INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const INT OffsetTrans = Seq.CompressedTrackOffsets(TrackIndex*2+0);
		ByteSwapOneTrack(Seq, MemoryReader, OffsetTrans);

		const INT OffsetRot = Seq.CompressedTrackOffsets(TrackIndex*2+1);
		ByteSwapOneTrack(Seq, MemoryReader, OffsetRot);
	}
}


/**
 * Handles Byte-swapping outgoing animation data to an array of BYTEs
 *
 * @param	Seq					An Animation Sequence to write.
 * @param	SerializedData		The output buffer.
 * @param	ForceByteSwapping	TRUE is byte swapping is not optional.
 */
void AEFPerTrackCompressionCodec::ByteSwapOut(
	UAnimSequence& Seq,
	TArray<BYTE>& SerializedData, 
	UBOOL ForceByteSwapping)
{
	FMemoryWriter MemoryWriter(SerializedData, TRUE);
	MemoryWriter.SetByteSwapping(ForceByteSwapping);

	const INT NumTracks = Seq.CompressedTrackOffsets.Num() / 2;
	for ( INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex )
	{
		const INT OffsetTrans = Seq.CompressedTrackOffsets(TrackIndex*2+0);
		ByteSwapOneTrack(Seq, MemoryWriter, OffsetTrans);

		const INT OffsetRot = Seq.CompressedTrackOffsets(TrackIndex*2+1);
		ByteSwapOneTrack(Seq, MemoryWriter, OffsetRot);
	}
}



/**
 * Extracts a single BoneAtom from an Animation Sequence.
 *
 * @param	OutAtom			The BoneAtom to fill with the extracted result.
 * @param	Seq				An Animation Sequence to extract the BoneAtom from.
 * @param	TrackIndex		The index of the track desired in the Animation Sequence.
 * @param	Time			The time (in seconds) to calculate the BoneAtom for.
 * @param	bLooping		TRUE if the animation should be played in a cyclic manner.
 */
void AEFPerTrackCompressionCodec::GetBoneAtom(
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	INT TrackIndex,
	FLOAT Time,
	UBOOL bLooping)
{
	// Initialize to identity to set the scale and in case of a missing rotation or translation codec
	OutAtom.SetIdentity();

	// Use the CompressedTrackOffsets stream to find the data addresses
	const INT* RESTRICT TrackData= Seq.CompressedTrackOffsets.GetTypedData() + (TrackIndex * 2);
	const INT TransKeysOffset = TrackData[0];
	const INT RotKeysOffset = TrackData[1];
	const FLOAT RelativePos = Time / (FLOAT)Seq.SequenceLength;

	GetBoneAtomTranslation(OutAtom, Seq, TransKeysOffset, Time, RelativePos, bLooping);
	GetBoneAtomRotation(OutAtom, Seq, RotKeysOffset, Time, RelativePos, bLooping);
}
	





void AEFPerTrackCompressionCodec::GetBoneAtomRotation(
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	INT Offset,
	FLOAT Time,
	FLOAT RelativePos,
	UBOOL bLooping)
{
	if (Offset != INDEX_NONE)
	{
		const BYTE* RESTRICT TrackData = Seq.CompressedByteStream.GetTypedData() + Offset + 4;
		const INT Header = *((INT*)(Seq.CompressedByteStream.GetTypedData() + Offset));

		INT KeyFormat;
		INT NumKeys;
		INT FormatFlags;
		INT BytesPerKey;
		INT FixedBytes;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);

		// Figure out the key indexes
		INT Index0 = 0;
		INT Index1 = 0;

		// Alpha is volatile to force the compiler to store it to memory immediately, so it is ready to be loaded into a vector register without a LHS after decompressing a track 
		volatile FLOAT Alpha = 0.0f;

		if (NumKeys > 1)
		{
			if ((FormatFlags & 0x8) == 0)
			{
				Alpha = TimeToIndex(Seq, RelativePos, bLooping, NumKeys, Index0, Index1);
			}
			else
			{
				const BYTE* RESTRICT FrameTable = Align(TrackData + FixedBytes + BytesPerKey * NumKeys, 4);
				Alpha = TimeToIndex(Seq, FrameTable, RelativePos, bLooping, NumKeys, Index0, Index1);
			}
		}

		// Unpack the first key
		const BYTE* RESTRICT KeyData0 = TrackData + FixedBytes + (Index0 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
		const VectorRegister R0 = DecompressSingleTrackRotationVectorized(KeyFormat, FormatFlags, TrackData, KeyData0);
#else
		FQuat R0;
		FAnimationCompression_PerTrackUtils::DecompressRotation(KeyFormat, FormatFlags, R0, TrackData, KeyData0);
#endif

		// If there is a second key, figure out the lerp between the two of them
		if (Index0 != Index1)
		{
			const BYTE* RESTRICT KeyData1 = TrackData + FixedBytes + (Index1 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
			ScalarRegister VAlpha(static_cast<FLOAT>(Alpha));

			const VectorRegister R1 = DecompressSingleTrackRotationVectorized(KeyFormat, FormatFlags, TrackData, KeyData1);

			const VectorRegister BlendedQuat = VectorLerpQuat(R0, R1, VAlpha);
			const VectorRegister BlendedNormalizedQuat = VectorNormalizeQuaternion(BlendedQuat);
			OutAtom.SetRotation(BlendedNormalizedQuat);
#else
			FQuat R1;
			FAnimationCompression_PerTrackUtils::DecompressRotation(KeyFormat, FormatFlags, R1, TrackData, KeyData1);

			// Fast linear quaternion interpolation.
			// To ensure the 'shortest route', we make sure the dot product between the two keys is positive.
			const FLOAT DotResult = (R0 | R1);
			const FLOAT Bias = appFloatSelect(DotResult, 1.0f, -1.0f);

			FQuat BlendedQuat = (R0 * (1.f-Alpha)) + (R1 * (Alpha * Bias));
			OutAtom.SetRotation( BlendedQuat );
			OutAtom.NormalizeRotation();
#endif
		}
		else // (Index0 == Index1)
		{
			OutAtom.SetRotation( R0 );
			OutAtom.NormalizeRotation();
		}
	}
	else
	{
		// Identity track
		OutAtom.SetRotation(FQuat::Identity);
	}
}



void AEFPerTrackCompressionCodec::GetBoneAtomTranslation(
	FBoneAtom& OutAtom,
	const UAnimSequence& Seq,
	INT Offset,
	FLOAT Time,
	FLOAT RelativePos,
	UBOOL bLooping)
{
	if (Offset != INDEX_NONE)
	{
		const BYTE* RESTRICT TrackData = Seq.CompressedByteStream.GetTypedData() + Offset + 4;
		const INT Header = *((INT*)(Seq.CompressedByteStream.GetTypedData() + Offset));

		INT KeyFormat;
		INT NumKeys;
		INT FormatFlags;
		INT BytesPerKey;
		INT FixedBytes;
		FAnimationCompression_PerTrackUtils::DecomposeHeader(Header, /*OUT*/ KeyFormat, /*OUT*/ NumKeys, /*OUT*/ FormatFlags, /*OUT*/BytesPerKey, /*OUT*/ FixedBytes);

		// Figure out the key indexes
		INT Index0 = 0;
		INT Index1 = 0;

		// Alpha is volatile to force the compiler to store it to memory immediately, so it is ready to be loaded into a vector register without a LHS after decompressing a track 
		volatile FLOAT Alpha = 0.0f;

		if (NumKeys > 1)
		{
			if ((FormatFlags & 0x8) == 0)
			{
				Alpha = TimeToIndex(Seq, RelativePos, bLooping, NumKeys, Index0, Index1);
			}
			else
			{
				const BYTE* RESTRICT FrameTable = Align(TrackData + FixedBytes + BytesPerKey * NumKeys, 4);
				Alpha = TimeToIndex(Seq, FrameTable, RelativePos, bLooping, NumKeys, Index0, Index1);
			}
		}

		// Unpack the first key
		const BYTE* RESTRICT KeyData0 = TrackData + FixedBytes + (Index0 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
		const VectorRegister R0 = DecompressSingleTrackTranslationVectorized(KeyFormat, FormatFlags, TrackData, KeyData0);
#else
		FVector R0;
		FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, R0, TrackData, KeyData0);
#endif

		// If there is a second key, figure out the lerp between the two of them
		if (Index0 != Index1)
		{
			const BYTE* RESTRICT KeyData1 = TrackData + FixedBytes + (Index1 * BytesPerKey);

#if USE_VECTOR_PTC_DECOMPRESSOR
			ScalarRegister VAlpha(static_cast<FLOAT>(Alpha));

			const VectorRegister R1 = DecompressSingleTrackTranslationVectorized(KeyFormat, FormatFlags, TrackData, KeyData1);

			const VectorRegister BlendedTranslation = Lerp(R0, R1, VAlpha);
			OutAtom.SetTranslation(BlendedTranslation);
#else
			FVector R1;
			FAnimationCompression_PerTrackUtils::DecompressTranslation(KeyFormat, FormatFlags, R1, TrackData, KeyData1);

			OutAtom.SetTranslation(Lerp(R0, R1, Alpha));
#endif
		}
		else // (Index0 == Index1)
		{
			OutAtom.SetTranslation(R0);
		}
	}
	else
	{
		// Identity track
#if USE_VECTOR_PTC_DECOMPRESSOR
		OutAtom.SetTranslation(VectorZero());
#else
		OutAtom.SetTranslation(FVector::ZeroVector);
#endif
	}
}

