/*=============================================================================
	UAnimationCompressionAlgorithm_PerTrackCompression.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "AnimationUtils.h"
#include "AnimationCompression.h"
#include "AnimationEncodingFormat.h"

IMPLEMENT_CLASS(UAnimationCompressionAlgorithm_PerTrackCompression);

struct FPerTrackCachedInfo
{
	/** Used as a sanity check to validate the cache */
	const FAnimSetMeshLinkup* AnimLinkup;

	/** Contains the maximum end effector errors from probe perturbations throughout the skeleton */
	TArray<FAnimPerturbationError> PerTrackErrors;

	/** Contains the height of each track within the skeleton */
	TArray<INT> TrackHeights;
};


/**
 * Structure that carries compression settings used in FPerTrackCompressor
 */
struct FPerTrackParams
{
	FLOAT MaxZeroingThreshold;

	const UAnimSequence* AnimSeq;

	UBOOL bIncludeKeyTable;
};

/**
 * This class compresses a single rotation or translation track into an internal buffer, keeping error metrics as it goes.
 */
class FPerTrackCompressor
{
public:
	// Used during compression
	FLOAT MaxError;
	DOUBLE SumError;

	// Results of compression
	TArray<BYTE> CompressedBytes;
	INT ActualCompressionMode;
protected:
	/** Does the compression scheme need a key->frame table (needed if the keys are spaced non-uniformly in time) */
	UBOOL bReallyNeedsFrameTable;

	/** Resets the compression buffer to defaults (no data) */
	void Reset()
	{
		MaxError = 0.0f;
		SumError = 0.0;
		bReallyNeedsFrameTable = FALSE;
		ActualCompressionMode = ACF_None;
		CompressedBytes.Empty();
	}

	/**
	 * Creates a header integer with four fields:
	 *   NumKeys can be no more than 24 bits (positions 0..23)
	 *   KeyFlags can be no more than 3 bits (positions 24..27)
	 *   bReallyNeedsFrameTable is a single bit (position 27)
	 *   KeyFormat can be no more than 4 bits (positions 31..28)
	 *
	 *   Also updates the ActualCompressionMode field
	 */
	INT MakeHeader(const INT NumKeys, const INT KeyFormat, const INT KeyFlags)
	{
		ActualCompressionMode = KeyFormat;
		return FAnimationCompression_PerTrackUtils::MakeHeader(NumKeys, KeyFormat, KeyFlags, bReallyNeedsFrameTable);
	}

	/** Ensures that the CompressedBytes output stream is a multiple of 4 bytes long */
	void PadOutputStream()
	{
		const BYTE PadSentinel = 85; //(1<<1)+(1<<3)+(1<<5)+(1<<7)

		const INT PadLength = Align(CompressedBytes.Num(), 4) - CompressedBytes.Num();
		for (INT i = 0; i < PadLength; ++i)
		{
			CompressedBytes.AddItem(PadSentinel);
		}
	}

	/** Writes Length bytes from Data to the output stream */
	void AppendBytes(const void* Data, INT Length)
	{
		const INT Offset = CompressedBytes.Add(Length);
		appMemcpy(CompressedBytes.GetTypedData() + Offset, Data, Length);
	}

	void CompressTranslation_Identity(const FTranslationTrack& TranslationData)
	{
		// Compute the error when using this compression type (how far off from (0,0,0) are they?)
		const INT NumKeys = TranslationData.PosKeys.Num();
		for (INT i = 0; i < NumKeys; ++i)
		{
			FLOAT Error = TranslationData.PosKeys(i).Size();
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
		ActualCompressionMode = ACF_Identity;

		// Add nothing to compressed bytes; this type gets flagged extra-special, back at the offset table
	}

	void CompressTranslation_16_16_16(const FTranslationTrack& TranslationData, FLOAT ZeroingThreshold)
	{
		//@TODO: Explore different scales
		const INT LogScale = 7;

		const INT NumKeys = TranslationData.PosKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(TranslationData.PosKeys.GetTypedData(), NumKeys);
		const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressTranslation_Identity(TranslationData);
			return;
		}

		// Write the header out
		const INT Header = MakeHeader(NumKeys, ACF_Fixed48NoW, bHasX | (bHasY<<1) | (bHasZ<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys for the non-zero components
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FVector& V = TranslationData.PosKeys(i);
			
			WORD X = 0;
			WORD Y = 0;
			WORD Z = 0;

			if (bHasX)
			{
				X = FAnimationCompression_PerTrackUtils::CompressFixed16(V.X, LogScale);
				AppendBytes(&X, sizeof(X));
			}
			if (bHasY)
			{
				Y = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Y, LogScale);
				AppendBytes(&Y, sizeof(Y));
			}
			if (bHasZ)
			{
				Z = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Z, LogScale);
				AppendBytes(&Z, sizeof(Z));
			}

			const FVector DecompressedV(
				bHasX ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(X) : 0.0f,
				bHasY ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(Y) : 0.0f,
				bHasZ ? FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(Z) : 0.0f);

			const FLOAT Error = (V - DecompressedV).Size();
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressTranslation_Uncompressed(const FTranslationTrack& TranslationData, FLOAT ZeroingThreshold)
	{
		const INT NumKeys = TranslationData.PosKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(TranslationData.PosKeys.GetTypedData(), NumKeys);
		const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if( !bHasX && !bHasY && !bHasZ )
		{
			// No point in using this over the identity encoding
			CompressTranslation_Identity(TranslationData);
			return;
		}

		// Write the header out
		const INT Header = MakeHeader(NumKeys, ACF_Float96NoW, bHasX | (bHasY<<1) | (bHasZ<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FVector& V = TranslationData.PosKeys(i);
			if( bHasX )
			{
				AppendBytes(&(V.X), sizeof(FLOAT));
			}
			if( bHasY )
			{
				AppendBytes(&(V.Y), sizeof(FLOAT));
			}
			if( bHasZ )
			{
				AppendBytes(&(V.Z), sizeof(FLOAT));
			}
		}

		// No error, it's a perfect encoding
		MaxError = 0.0f;
		SumError = 0.0;
	}

	// Encode a 0..1 interval in 10:11:11 (X and Z swizzled in the 11:11:10 source because Z is more important in most animations)
	// and store an uncompressed bounding box at the start of the track to scale that 0..1 back up
	void CompressTranslation_10_11_11(const FTranslationTrack& TranslationData, FLOAT ZeroingThreshold)
	{
		const INT NumKeys = TranslationData.PosKeys.Num();

		// Determine the bounds
		const FBox KeyBounds(TranslationData.PosKeys.GetTypedData(), NumKeys);
		const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressTranslation_Identity(TranslationData);
			return;
		}

		// Write the header out
		const INT Header = MakeHeader(NumKeys, ACF_IntervalFixed32NoW, bHasX | (bHasY<<1) | (bHasZ<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the bounds out
		FLOAT Mins[3];
		FLOAT Ranges[3];
		FVector Range(KeyBounds.Max - KeyBounds.Min);
		Mins[0] = KeyBounds.Min.X;
		Mins[1] = KeyBounds.Min.Y;
		Mins[2] = KeyBounds.Min.Z;
		Ranges[0] = Range.X;
		Ranges[1] = Range.Y;
		Ranges[2] = Range.Z;
		if (bHasX)
		{
			AppendBytes(Mins + 0, sizeof(FLOAT));
			AppendBytes(Ranges + 0, sizeof(FLOAT));
		}
		else
		{
			Ranges[0] = Mins[0] = 0.0f;
		}

		if (bHasY)
		{
			AppendBytes(Mins + 1, sizeof(FLOAT));
			AppendBytes(Ranges + 1, sizeof(FLOAT));
		}
		else
		{
			Ranges[1] = Mins[1] = 0.0f;
		}

		if (bHasZ)
		{
			AppendBytes(Mins + 2, sizeof(FLOAT));
			AppendBytes(Ranges + 2, sizeof(FLOAT));
		}
		else
		{
			Ranges[2] = Mins[2] = 0.0f;
		}

		// Write the keys out
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FVector& V = TranslationData.PosKeys(i);
			const FVectorIntervalFixed32NoW Compressor(V, Mins, Ranges);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and update the error stats
			FVector DecompressedV;
			Compressor.ToVector(DecompressedV, Mins, Ranges);

			const FLOAT Error = (DecompressedV - V).Size();
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
	}

	static FBox CalculateQuatACF96Bounds(const FQuat* Points, INT NumPoints)
	{
		FBox Results;

		for (INT i = 0; i < NumPoints; ++i)
		{
			const FQuatFloat96NoW Converter(Points[i]);

			Results += FVector(Converter.X, Converter.Y, Converter.Z);
		}


		return Results;
	}

	void CompressRotation_Identity(const FRotationTrack& RotationData)
	{
		// Compute the error when using this compression type (how far off from identity are they?)
		const INT NumKeys = RotationData.RotKeys.Num();
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FLOAT Error = FQuatErrorAutoNormalize(RotationData.RotKeys(i), FQuat::Identity);
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
		ActualCompressionMode = ACF_Identity;

		// Add nothing to compressed bytes; this type gets flagged extra-special, back at the offset table
	}

	template <typename CompressorType>
	void InnerCompressRotation(const FRotationTrack& RotationData)
	{
		// Write the keys out
		const INT NumKeys = RotationData.RotKeys.Num();
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FQuat& Q = RotationData.RotKeys(i);
			check(Q.IsNormalized());

			// Compress and write out the quaternion
			const CompressorType Compressor(Q);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and check the error caused by the compression
			FQuat DecompressedQ;
			Compressor.ToQuat(DecompressedQ);

			checkf(DecompressedQ.IsNormalized());
			const FLOAT Error = FQuatErrorAutoNormalize(Q, DecompressedQ);
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
	}

	// Uncompressed packing still drops the W component, storing a rotation in 3 floats (ACF_Float96NoW)
	void CompressRotation_Uncompressed(const FRotationTrack& RotationData)
	{
		const INT NumKeys = RotationData.RotKeys.Num();

		// Write the header out
		INT Header = MakeHeader(NumKeys, ACF_Float96NoW, 7);
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		InnerCompressRotation<FQuatFloat96NoW>(RotationData);
	}

	void CompressRotation_16_16_16(const FRotationTrack& RotationData, FLOAT ZeroingThreshold)
	{
		const INT NumKeys = RotationData.RotKeys.Num();

		// Determine the bounds
		const FBox KeyBounds = CalculateQuatACF96Bounds(RotationData.RotKeys.GetTypedData(), NumKeys);
		const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if (!bHasX && !bHasY && !bHasZ)
		{
			// No point in using this over the identity encoding
			CompressRotation_Identity(RotationData);
			return;
		}


		// Write the header out
		const INT Header = MakeHeader(NumKeys, ACF_Fixed48NoW, bHasX | (bHasY<<1) | (bHasZ<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the keys for the non-zero components
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FQuat& Q = RotationData.RotKeys(i);

			FQuat QRenorm(Q);
			if (!bHasX)
			{
				QRenorm.X = 0;
			}
			if (!bHasY)
			{
				QRenorm.Y = 0;
			}
			if (!bHasZ)
			{
				QRenorm.Z = 0;
			}
			QRenorm.Normalize();

			const FQuatFloat96NoW V(QRenorm);

			WORD X = 0;
			WORD Y = 0;
			WORD Z = 0;

			if (bHasX)
			{
				X = FAnimationCompression_PerTrackUtils::CompressFixed16(V.X);
				AppendBytes(&X, sizeof(X));
			}
			if (bHasY)
			{
				Y = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Y);
				AppendBytes(&Y, sizeof(Y));
			}
			if (bHasZ)
			{
				Z = FAnimationCompression_PerTrackUtils::CompressFixed16(V.Z);
				AppendBytes(&Z, sizeof(Z));
			}

			FQuatFloat96NoW Decompressor;
			Decompressor.X = bHasX ? FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(X) : 0.0f;
			Decompressor.Y = bHasY ? FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(Y) : 0.0f;
			Decompressor.Z = bHasZ ? FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(Z) : 0.0f;

			FQuat DecompressedQ;
			Decompressor.ToQuat(DecompressedQ);

			if (!DecompressedQ.IsNormalized())
			{
				debugf(TEXT("Error: Loss of normalization!"));
				debugf(TEXT("  Track: %i, Key: %i"), 0, i);
				debugf(TEXT("  Q : %s"), *Q.ToString());
				debugf(TEXT("  Q': %s"), *DecompressedQ.ToString());
				debugf(TEXT(" XYZ: %i, %i, %i"), X, Y, Z);
			}

			checkf(DecompressedQ.IsNormalized());
			const FLOAT Error = FQuatErrorAutoNormalize(Q, DecompressedQ);
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressRotation_11_11_10(const FRotationTrack& RotationData, FLOAT ZeroingThreshold)
	{
		const INT NumKeys = RotationData.RotKeys.Num();

		// Determine the bounds
		const FBox KeyBounds = CalculateQuatACF96Bounds(RotationData.RotKeys.GetTypedData(), NumKeys);
		FVector Range(KeyBounds.Max - KeyBounds.Min);

		const UBOOL bHasX = (Abs(KeyBounds.Max.X) >= ZeroingThreshold) || (Abs(KeyBounds.Min.X) >= ZeroingThreshold);
		const UBOOL bHasY = (Abs(KeyBounds.Max.Y) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Y) >= ZeroingThreshold);
		const UBOOL bHasZ = (Abs(KeyBounds.Max.Z) >= ZeroingThreshold) || (Abs(KeyBounds.Min.Z) >= ZeroingThreshold);

		if ((!bHasX && !bHasY && !bHasZ) || (Range.Size() > 4.0f))
		{
			// If there are no components, then there is no point in using this over the identity encoding
			// If the range is insane, error out early (error metric will be high)
			CompressRotation_Identity(RotationData);
			return;
		}

		// Write the header out
		const INT Header = MakeHeader(NumKeys, ACF_IntervalFixed32NoW, bHasX | (bHasY<<1) | (bHasZ<<2));
		AppendBytes(&Header, sizeof(Header));

		// Write the bounds out
		FLOAT Mins[3];
		FLOAT Ranges[3];
		Mins[0] = KeyBounds.Min.X;
		Mins[1] = KeyBounds.Min.Y;
		Mins[2] = KeyBounds.Min.Z;
		Ranges[0] = Range.X;
		Ranges[1] = Range.Y;
		Ranges[2] = Range.Z;
		if (bHasX)
		{
			AppendBytes(Mins + 0, sizeof(FLOAT));
			AppendBytes(Ranges + 0, sizeof(FLOAT));
		}
		else
		{
			Ranges[0] = Mins[0] = 0.0f;
		}

		if (bHasY)
		{
			AppendBytes(Mins + 1, sizeof(FLOAT));
			AppendBytes(Ranges + 1, sizeof(FLOAT));
		}
		else
		{
			Ranges[1] = Mins[1] = 0.0f;
		}

		if (bHasZ)
		{
			AppendBytes(Mins + 2, sizeof(FLOAT));
			AppendBytes(Ranges + 2, sizeof(FLOAT));
		}
		else
		{
			Ranges[2] = Mins[2] = 0.0f;
		}

		// Write the keys out
		for (INT i = 0; i < NumKeys; ++i)
		{
			const FQuat& Q = RotationData.RotKeys(i);

			FQuat QRenorm(Q);
			if (!bHasX)
			{
				QRenorm.X = 0;
			}
			if (!bHasY)
			{
				QRenorm.Y = 0;
			}
			if (!bHasZ)
			{
				QRenorm.Z = 0;
			}
			QRenorm.Normalize();


			// Compress and write out the quaternion
			const FQuatIntervalFixed32NoW Compressor(QRenorm, Mins, Ranges);
			AppendBytes(&Compressor, sizeof(Compressor));

			// Decompress and check the error caused by the compression
			FQuat DecompressedQ;
			Compressor.ToQuat(DecompressedQ, Mins, Ranges);

			if (!DecompressedQ.IsNormalized())
			{
				debugf(TEXT("Error: Loss of normalization!"));
				debugf(TEXT("  Track: %i, Key: %i"), 0, i);
				debugf(TEXT("  Q : %s"), *Q.ToString());
				debugf(TEXT("  Q': %s"), *DecompressedQ.ToString());
				debugf(TEXT(" XYZ: %f, %f, %f, %f"), QRenorm.X, QRenorm.Y, QRenorm.Z, QRenorm.W);
				debugf(TEXT(" Mins(%f, %f, %f)   Maxs(%f, %f,%f)"), KeyBounds.Min.X, KeyBounds.Min.Y, KeyBounds.Min.Z, KeyBounds.Max.X, KeyBounds.Max.Y, KeyBounds.Max.Z);
			}
			checkf(DecompressedQ.IsNormalized());
			const FLOAT Error = FQuatErrorAutoNormalize(Q, DecompressedQ);
			MaxError = Max(MaxError, Error);
			SumError += Error;
		}
	}

	void CompressRotation_Fixed32(const FRotationTrack& RotationData)
	{
		// Write the header out
		const INT NumKeys = RotationData.RotKeys.Num();
		const INT Header = MakeHeader(NumKeys, ACF_Fixed32NoW, 7);
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		InnerCompressRotation<FQuatFixed32NoW>(RotationData);
	}

	void CompressRotation_Float32(const FRotationTrack& RotationData)
	{
		// Write the header out
		const INT NumKeys = RotationData.RotKeys.Num();
		const INT Header = MakeHeader(NumKeys, ACF_Float32NoW, 7);
		AppendBytes(&Header, sizeof(Header));

		// Write the keys out
		InnerCompressRotation<FQuatFloat32NoW>(RotationData);
	}

	/** Helper method for writing out the key->frame mapping table with a given index type */
	template <typename FrameIndexType>
	void EmitKeyToFrameTable(INT NumFrames, FLOAT FramesPerSecond, const TArrayNoInit<FLOAT>& Times)
	{
		PadOutputStream();

		// write the key table
		const INT NumKeys = Times.Num();
		for (INT KeyIndex = 0; KeyIndex < NumKeys; ++KeyIndex)
		{
			// Convert the frame time into a frame index and write it out
			FrameIndexType FrameIndex = (FrameIndexType)Clamp(appTrunc(Times(KeyIndex) * FramesPerSecond), 0, NumFrames - 1);
			AppendBytes(&FrameIndex, sizeof(FrameIndexType));
		}

		PadOutputStream();
	}

	/** Writes out the key->frame mapping table if it is needed for the current compression type */
	void ProcessKeyToFrameTable(const FPerTrackParams& Params, const TArrayNoInit<FLOAT>& FrameTimes)
	{
		if (bReallyNeedsFrameTable && (CompressedBytes.Num() > 0))
		{
			const INT NumFrames = Params.AnimSeq->NumFrames;
			const FLOAT SequenceLength = Params.AnimSeq->SequenceLength;
			const FLOAT FramesPerSecond = NumFrames / SequenceLength;

			if (NumFrames <= 0xFF)
			{
				EmitKeyToFrameTable<BYTE>(NumFrames, FramesPerSecond, FrameTimes);
			}
			else
			{
				EmitKeyToFrameTable<WORD>(NumFrames, FramesPerSecond, FrameTimes);
			}
		}
	}

public:
	/** Constructs a compressed track of translation data */
	FPerTrackCompressor(INT InCompressionType, const FTranslationTrack& TranslationData, const FPerTrackParams& Params)
	{
		Reset();
		bReallyNeedsFrameTable = Params.bIncludeKeyTable && (TranslationData.PosKeys.Num() > 1) && (TranslationData.PosKeys.Num() < Params.AnimSeq->NumFrames);

		switch (InCompressionType)
		{
		case ACF_Identity:
			CompressTranslation_Identity(TranslationData);
			break;
		case ACF_None:
		case ACF_Float96NoW:
			CompressTranslation_Uncompressed(TranslationData, Params.MaxZeroingThreshold);
			break;
		case ACF_Fixed48NoW:
			CompressTranslation_16_16_16(TranslationData, Params.MaxZeroingThreshold);
			break;
		case ACF_IntervalFixed32NoW:
			CompressTranslation_10_11_11(TranslationData, Params.MaxZeroingThreshold);
			break;
			// The following two formats don't work well for translation (fixed range & low precision)
			//case ACF_Fixed32NoW:
			//case ACF_Float32NoW:
		default:
			appErrorf(TEXT("Unsupported translation compression format"));
			break;
		}

		PadOutputStream();

		ProcessKeyToFrameTable(Params, TranslationData.Times);
	}

	/** Constructs a compressed track of rotation data */
	FPerTrackCompressor(INT InCompressionType, const FRotationTrack& RotationData, const FPerTrackParams& Params)
	{
		Reset();
		bReallyNeedsFrameTable = Params.bIncludeKeyTable && (RotationData.RotKeys.Num() > 1) && (RotationData.RotKeys.Num() < Params.AnimSeq->NumFrames);

		switch (InCompressionType)
		{
		case ACF_Identity:
			CompressRotation_Identity(RotationData);
			break;
		case ACF_None:
		case ACF_Float96NoW:
			CompressRotation_Uncompressed(RotationData);
			break;
		case ACF_Fixed48NoW:
			CompressRotation_16_16_16(RotationData, Params.MaxZeroingThreshold);
			break;
		case ACF_IntervalFixed32NoW:
			CompressRotation_11_11_10(RotationData, Params.MaxZeroingThreshold);
			break;
		case ACF_Fixed32NoW:
			CompressRotation_Fixed32(RotationData);
			break;
		case ACF_Float32NoW:
			CompressRotation_Float32(RotationData);
			break;
		default:
			appErrorf(TEXT("Unsupported rotation compression format"));
			break;
		}

		PadOutputStream();

		ProcessKeyToFrameTable(Params, RotationData.Times);
	}
};

/**
 * Compresses the tracks passed in using the underlying compressor for this key removal codec
 */
void UAnimationCompressionAlgorithm_PerTrackCompression::CompressUsingUnderlyingCompressor(
	UAnimSequence* AnimSeq, 
	USkeletalMesh* SkelMesh,
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FBoneData>& BoneData, 
	const TArray<FTranslationTrack>& TranslationData,
	const TArray<FRotationTrack>& RotationData,
	const UBOOL bFinalPass)
{
	// If not doing final pass, then do the RemoveLinearKey version that is less destructive.
	// We're potentially removing whole tracks here, and that doesn't work well with LinearKeyRemoval algorithm.
	if( !bFinalPass )
	{
		UAnimationCompressionAlgorithm_RemoveLinearKeys::CompressUsingUnderlyingCompressor(
			AnimSeq,
			SkelMesh,
			AnimLinkup,
			BoneData,
			TranslationData,
			RotationData,
			bFinalPass);
		return;
	}

	// Grab the cache
	check(PerReductionCachedData != NULL);
	FPerTrackCachedInfo* Cache = (FPerTrackCachedInfo*)PerReductionCachedData;
	check(Cache->AnimLinkup == &AnimLinkup);

	// record the proper runtime decompressor to use
	AnimSeq->KeyEncodingFormat = AKF_PerTrackCompression;
	AnimSeq->RotationCompressionFormat = ACF_Identity;
	AnimSeq->TranslationCompressionFormat = ACF_Identity;
	AnimationFormat_SetInterfaceLinks(*AnimSeq);

	// Prime the compression buffers
	check(TranslationData.Num() == RotationData.Num());
	const INT NumTracks = TranslationData.Num();

	AnimSeq->CompressedTrackOffsets.Empty(NumTracks*2);
	AnimSeq->CompressedTrackOffsets.Add(NumTracks*2);
	AnimSeq->CompressedByteStream.Empty();

	// Compress each track independently
	for (INT TrackIndex = 0; TrackIndex < NumTracks; ++TrackIndex)
	{
		// Compression parameters / thresholds
		FPerTrackParams Params;
		Params.AnimSeq = AnimSeq;
		Params.MaxZeroingThreshold = MaxZeroingThreshold;
		Params.bIncludeKeyTable = FALSE;

		// Determine the local-space error cutoffs
		FLOAT MaxPositionErrorCutoff = MaxPosDiffBitwise;
		FLOAT MaxAngleErrorCutoff = MaxAngleDiffBitwise;
		if (bUseAdaptiveError)
		{
			// The height of the track is the distance from an end effector.  It's used to reduce the acceptable error the
			// higher in the skeleton we get, since a higher bone will cause cascading errors everywhere.
			const INT PureTrackHeight = Cache->TrackHeights(TrackIndex);
			const INT EffectiveTrackHeight = Max(0, PureTrackHeight + TrackHeightBias);

			const FLOAT Scaler = 1.0f / appPow(Max(ParentingDivisor, 1.0f), EffectiveTrackHeight * Max(0.0f, ParentingDivisorExponent));

			MaxPositionErrorCutoff = Max<FLOAT>(MaxZeroingThreshold, MaxPosDiff * Scaler);
			MaxAngleErrorCutoff = Max<FLOAT>(MaxZeroingThreshold, MaxAngleDiff * Scaler);

			if (bUseOverrideForEndEffectors && (PureTrackHeight == 0))
			{
				MaxPositionErrorCutoff = MinEffectorDiff;
			}
		}
		else if (bUseAdaptiveError2)
		{
			const FAnimPerturbationError& TrackError = Cache->PerTrackErrors(TrackIndex);

			FLOAT ThresholdT_DueR = (TrackError.MaxErrorInTransDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToRot) : 1.0f;
			FLOAT ThresholdT_DueT = (TrackError.MaxErrorInTransDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInTransDueToTrans) : 1.0f;

			//@TODO: Mixing spaces (target angle error is in radians, perturbation is in quaternion component units)
			FLOAT ThresholdR_DueR = (TrackError.MaxErrorInRotDueToRot > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToRot) : 1.0f;
			FLOAT ThresholdR_DueT = (TrackError.MaxErrorInRotDueToTrans > SMALL_NUMBER) ? (PerturbationProbeSize / TrackError.MaxErrorInRotDueToTrans) : 1.0f;

			MaxAngleErrorCutoff = Min(MaxAngleDiffBitwise, MaxErrorPerTrackRatio * MaxAngleDiff * Lerp(ThresholdR_DueR, ThresholdT_DueR, RotationErrorSourceRatio));
			MaxPositionErrorCutoff = Min(MaxPosDiffBitwise, MaxErrorPerTrackRatio * MaxPosDiff * Lerp(ThresholdR_DueT, ThresholdT_DueT, TranslationErrorSourceRatio));
		}

		// Start compressing translation using a totally lossless float32x3
		const FTranslationTrack& TranslationTrack = TranslationData(TrackIndex);

		Params.bIncludeKeyTable = bActuallyFilterLinearKeys && !FAnimationUtils::HasUniformKeySpacing(AnimSeq, TranslationTrack.Times);
		FPerTrackCompressor BestTranslation(ACF_Float96NoW, TranslationTrack, Params);

		// Try the other translation formats
		for (INT FormatIndex = 0; FormatIndex < AllowedTranslationFormats.Num(); ++FormatIndex)
		{
			FPerTrackCompressor TrialCompression(AllowedTranslationFormats(FormatIndex), TranslationTrack, Params);

			if (TrialCompression.MaxError <= MaxPositionErrorCutoff)
			{
				// Swap if it's smaller or equal-sized but lower-max-error
				const INT BytesSaved = BestTranslation.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
				const UBOOL bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestTranslation.MaxError));

				if (bIsImprovement)
				{
					BestTranslation = TrialCompression;
				}
			}
		}

		// Start compressing rotation, first using lossless float32x3
		const FRotationTrack& RotationTrack = RotationData(TrackIndex);

		Params.bIncludeKeyTable = bActuallyFilterLinearKeys && !FAnimationUtils::HasUniformKeySpacing(AnimSeq, RotationTrack.Times);
		FPerTrackCompressor BestRotation(ACF_Float96NoW, RotationTrack, Params);

		//UBOOL bLeaveRotationUncompressed = (RotationTrack.Times.Num() <= 1) && (GHighQualityEmptyTracks != 0);
		// Try the other rotation formats
		//if (!bLeaveRotationUncompressed)
		{
			for (INT FormatIndex = 0; FormatIndex < AllowedRotationFormats.Num(); ++FormatIndex)
			{
				FPerTrackCompressor TrialCompression(AllowedRotationFormats(FormatIndex), RotationTrack, Params);

				if (TrialCompression.MaxError <= MaxAngleErrorCutoff)
				{
					// Swap if it's smaller or equal-sized but lower-max-error
					const INT BytesSaved = BestRotation.CompressedBytes.Num() - TrialCompression.CompressedBytes.Num();
					const UBOOL bIsImprovement = (BytesSaved > 0) || ((BytesSaved == 0) && (TrialCompression.MaxError < BestRotation.MaxError));

					if (bIsImprovement)
					{
						BestRotation = TrialCompression;
					}
				}
			}
		}

		// See if we can skip saving translation track.
		UBOOL bSkipTranslationTrack = FALSE;
#if( SKIP_FORCEMESHTRANSLATION_TRACKS || SKIP_ANIMROTATIONONLY_TRACKS )
		{
			UAnimSet* AnimSet = AnimSeq->GetAnimSet();
			FName const BoneName = AnimSet->TrackBoneNames(TrackIndex);
	#if( SKIP_FORCEMESHTRANSLATION_TRACKS )
			bSkipTranslationTrack = (AnimSet->ForceMeshTranslationBoneNames.FindItemIndex( BoneName ) != INDEX_NONE);
	#endif

	#if( SKIP_ANIMROTATIONONLY_TRACKS )
			INT const BoneIndex = AnimLinkup.BoneToTrackTable.FindItemIndex( TrackIndex );
			UBOOL const bIsRootBone = (BoneIndex == 0);
			bSkipTranslationTrack = bSkipTranslationTrack || (!bIsRootBone && AnimSet->bAnimRotationOnly && AnimSet->UseTranslationBoneNames.FindItemIndex(BoneName) == INDEX_NONE);
	#endif
		}
#endif

		// Now write out compression and translation frames into the stream
		INT TranslationOffset = INDEX_NONE;
		if( !bSkipTranslationTrack && BestTranslation.CompressedBytes.Num() > 0 )
		{
			checkf(BestTranslation.ActualCompressionMode < ACF_MAX);
			TranslationOffset = AnimSeq->CompressedByteStream.Num();
			AnimSeq->CompressedByteStream.Append(BestTranslation.CompressedBytes);
		}
		AnimSeq->CompressedTrackOffsets(TrackIndex*2 + 0) = TranslationOffset;

		INT RotationOffset = INDEX_NONE;
		if (BestRotation.CompressedBytes.Num() > 0)
		{
			checkf(BestRotation.ActualCompressionMode < ACF_MAX);
			RotationOffset = AnimSeq->CompressedByteStream.Num();
			AnimSeq->CompressedByteStream.Append(BestRotation.CompressedBytes);
		}
		AnimSeq->CompressedTrackOffsets(TrackIndex*2 + 1) = RotationOffset;

#if 0
		// This block outputs information about each individual track during compression, which is useful for debugging the compressors
		warnf(TEXT("   Compressed track %i, Trans=%s_%i (#keys=%i, err=%f), Rot=%s_%i (#keys=%i, err=%f)  (height=%i max pos=%f, angle=%f)"), 
			TrackIndex,
			*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(BestTranslation.ActualCompressionMode)),
			BestTranslation.ActualCompressionMode != ACF_Identity ? ( ( *( (const INT*)BestTranslation.CompressedBytes.GetTypedData() ) ) >> 24) & 0xF : 0,
			TranslationTrack.PosKeys.Num(),
			BestTranslation.MaxError,
			*FAnimationUtils::GetAnimationCompressionFormatString(static_cast<AnimationCompressionFormat>(BestRotation.ActualCompressionMode)),
			BestRotation.ActualCompressionMode != ACF_Identity ? ( ( *( (const INT*)BestRotation.CompressedBytes.GetTypedData() ) ) >> 24) & 0xF : 0,
			RotationTrack.RotKeys.Num(),
			BestRotation.MaxError,
			(bUseAdaptiveError)? (Cache->TrackHeights(TrackIndex)): -1,
			MaxPositionErrorCutoff,
			MaxAngleErrorCutoff
			);
#endif
	}
}

/** Resamples a track of position keys */
void ResamplePositionKeys(
	FTranslationTrack& Track, 
	FLOAT StartTime,
	FLOAT IntervalTime)
{
	const INT KeyCount = Track.Times.Num();

	// Oddness about the original data: 30 keys will have times from 0..1 *inclusive*, and 30 Hz isn't
	// This means the key spacing needs a boost
	if (KeyCount > 1)
	{
		IntervalTime = IntervalTime * (KeyCount / (FLOAT)(KeyCount - 1));
	}

	check(Track.Times.Num() == Track.PosKeys.Num());

	TArray<FVector> NewPosKeys;
	TArray<FLOAT> NewTimes;

	NewTimes.Empty(KeyCount);
	NewPosKeys.Empty(KeyCount);

	FLOAT FinalTime = Track.Times(KeyCount - 1);

	// step through and retain the desired interval
	INT CachedIndex = 0;

	FLOAT Time = StartTime;
	while (Time <= FinalTime)
	{
		// Find the bracketing current keys
		if (CachedIndex < KeyCount - 1)
		{
			while ((CachedIndex < KeyCount - 1) && (Track.Times(CachedIndex+1) < Time))
			{
				CachedIndex++;
			}
		}

		FVector Value;

		checkf(Track.Times(CachedIndex) <= Time);
		if (CachedIndex + 1 < KeyCount)
		{
			checkf(Track.Times(CachedIndex+1) >= Time);

			FVector A = Track.PosKeys(CachedIndex);
			FVector B = Track.PosKeys(CachedIndex + 1);

			FLOAT Alpha = (Time - Track.Times(CachedIndex)) / (Track.Times(CachedIndex+1) - Track.Times(CachedIndex));
			Value = Lerp(A, B, Alpha);
		}
		else
		{
			Value = Track.PosKeys(CachedIndex);
		}

		NewPosKeys.AddItem(Value);
		NewTimes.AddItem(Time);

		Time += IntervalTime;
	}

	NewTimes.Shrink();
	NewPosKeys.Shrink();

	Track.Times = NewTimes;
	Track.PosKeys = NewPosKeys;
}

/**
 * Resamples a track of rotation keys
 */
void ResampleRotationKeys(
	FRotationTrack& Track,
	FLOAT StartTime,
	FLOAT IntervalTime)
{
	const INT KeyCount = Track.Times.Num();
	check(Track.Times.Num() == Track.RotKeys.Num());

	// Oddness about the original data: 30 keys will have times from 0..1 *inclusive*, and 30 Hz isn't
	// This means the key spacing needs a boost
	if (KeyCount > 1)
	{
		IntervalTime = IntervalTime * (KeyCount / (FLOAT)(KeyCount - 1));
	}

	TArray<FQuat> NewRotKeys;
	TArray<FLOAT> NewTimes;

	NewTimes.Empty(KeyCount);
	NewRotKeys.Empty(KeyCount);

	FLOAT FinalTime = Track.Times(KeyCount - 1);

	// step through and retain the desired interval
	INT CachedIndex = 0;

	FLOAT Time = StartTime;
	while (Time <= FinalTime)
	{
		// Find the bracketing current keys
		if (CachedIndex < KeyCount - 1)
		{
			while ((CachedIndex < KeyCount - 1) && (Track.Times(CachedIndex+1) < Time))
			{
				CachedIndex++;
			}
		}

		FQuat Value;

		checkf(Track.Times(CachedIndex) <= Time);
		if (CachedIndex + 1 < KeyCount)
		{
			checkf(Track.Times(CachedIndex+1) >= Time);

			FQuat A = Track.RotKeys(CachedIndex);
			FQuat B = Track.RotKeys(CachedIndex + 1);

			FLOAT Alpha = (Time - Track.Times(CachedIndex)) / (Track.Times(CachedIndex+1) - Track.Times(CachedIndex));
			Value = Lerp(A, B, Alpha);
			Value.Normalize();
		}
		else
		{
			Value = Track.RotKeys(CachedIndex);
		}

		NewRotKeys.AddItem(Value);
		NewTimes.AddItem(Time);

		Time += IntervalTime;
	}

	NewTimes.Shrink();
	NewRotKeys.Shrink();

	Track.Times = NewTimes;
	Track.RotKeys = NewRotKeys;
}




void ResampleKeys(
	TArray<FTranslationTrack>& PositionTracks, 
	TArray<FRotationTrack>& RotationTracks,
	FLOAT Interval,
	FLOAT Time0 = 0.0f)
{
	check(PositionTracks.Num() == RotationTracks.Num());
	check((Time0 >= 0.0f) && (Interval > 0.0f));
	for (INT TrackIndex = 0; TrackIndex < PositionTracks.Num(); ++TrackIndex)
	{
		ResamplePositionKeys(PositionTracks(TrackIndex), Time0, Interval);
		ResampleRotationKeys(RotationTracks(TrackIndex), Time0, Interval);
	}
}




/**
 * Pre-filters the tracks before running the main key removal algorithm
 */
void UAnimationCompressionAlgorithm_PerTrackCompression::FilterBeforeMainKeyRemoval(
	UAnimSequence* AnimSeq, 
	USkeletalMesh* SkelMesh, 
	const struct FAnimSetMeshLinkup& AnimLinkup,
	const TArray<FBoneData>& BoneData, 
	TArray<FTranslationTrack>& TranslationData,
	TArray<FRotationTrack>& RotationData)
{
	const INT NumTracks = TranslationData.Num();

	// Downsample the keys if enabled
	if ((AnimSeq->NumFrames >= MinKeysForResampling) && bResampleAnimation)
	{
		ResampleKeys(TranslationData, RotationData, 1.0f / ResampledFramerate, 0.0f);
	}

	// Create the cache
	check(PerReductionCachedData == NULL);
	FPerTrackCachedInfo* Cache = new FPerTrackCachedInfo();
	Cache->AnimLinkup = &AnimLinkup;
	PerReductionCachedData = Cache;
	
	// Calculate how far each track is from controlling an end effector
	if (bUseAdaptiveError)
	{
		FAnimationUtils::CalculateTrackHeights(AnimLinkup, BoneData, NumTracks, /*OUT*/ Cache->TrackHeights);
	}

	// Find out how a small change affects the maximum error in the end effectors
	if (bUseAdaptiveError2)
	{
		FVector TranslationProbe(PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize);
		FQuat RotationProbe(PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize, PerturbationProbeSize);

		FAnimationUtils::TallyErrorsFromPerturbation(
			AnimSeq,
			NumTracks,
			SkelMesh,
			AnimLinkup,
			BoneData,
			TranslationProbe,
			RotationProbe,
			/*OUT*/ Cache->PerTrackErrors);
	}

	// remove obviously redundant keys from the source data
	FilterTrivialKeys(TranslationData, RotationData, TRANSLATION_ZEROING_THRESHOLD, QUATERNION_ZEROING_THRESHOLD);
}

/**
 * Animation compression algorithm that optionally:
 *   1) Forcefully removes a portion of keys (every other key, 2 out of every 3, etc...)
 *   2) Removes any keys which can be linearly approximated by neighboring keys
 * but always packs each track using per-track compression settings.
 */
void UAnimationCompressionAlgorithm_PerTrackCompression::DoReduction(UAnimSequence* AnimSeq, USkeletalMesh* SkelMesh, const TArray<FBoneData>& BoneData)
{
#if !CONSOLE
	ensure((MaxPosDiffBitwise > 0.0f) && (MaxAngleDiffBitwise > 0.0f) && (MaxZeroingThreshold >= 0.0f));
	ensure(MaxZeroingThreshold <= MaxPosDiffBitwise);
	ensure(!(bUseAdaptiveError2 && bUseAdaptiveError));

	// Compress
	UAnimationCompressionAlgorithm_RemoveLinearKeys::DoReduction(AnimSeq, SkelMesh, BoneData);

	// Delete the cache
	if (PerReductionCachedData != NULL)
	{
		delete PerReductionCachedData;
	}
	PerReductionCachedData = NULL;
#endif
}
