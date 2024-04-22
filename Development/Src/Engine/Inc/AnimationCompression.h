/*=============================================================================
	AnimationCompression.h: Skeletal mesh animation compression.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/ 

#ifndef __ANIMATIONCOMPRESSION_H__
#define __ANIMATIONCOMPRESSION_H__

#include "FloatPacker.h"

// Thresholds
#define TRANSLATION_ZEROING_THRESHOLD (0.0001f)
#define QUATERNION_ZEROING_THRESHOLD (0.0003f)

/** Size of Dummy bone used, when measuring error at an end effector which has a socket attached to it */
#define END_EFFECTOR_SOCKET_DUMMY_BONE_SIZE	(50.f)

/** This will reduce automatically bAnimRotationOnly Translation Tracks to 1 key, even if they have animated data. */
#define REDUCE_ANIMROTATIONONLY_TRACKS (1)

/** 
 * This will skip storing bAnimRotationOnly Translation Tracks, and assumes the AnimSet->bAnimRotationOnly flag won't be changed at runtime.
 * This is currently only used in the PerTrack compressor.
 */
#define SKIP_ANIMROTATIONONLY_TRACKS (0)

/**
 * Do not store translation tracks when those are marked 'ForceMeshTranslation'
 * TRUE if SKIP_ANIMROTATIONONLY_TRACKS is defined to TRUE.
 */
#define SKIP_FORCEMESHTRANSLATION_TRACKS (1 || SKIP_ANIMROTATIONONLY_TRACKS)

#define Quant16BitDiv     (32767.f)
#define Quant16BitFactor  (32767.f)
#define Quant16BitOffs    (32767)

#define Quant10BitDiv     (511.f)
#define Quant10BitFactor  (511.f)
#define Quant10BitOffs    (511)

#define Quant11BitDiv     (1023.f)
#define Quant11BitFactor  (1023.f)
#define Quant11BitOffs    (1023)

class FQuatFixed48NoW
{
public:
	WORD X;
	WORD Y;
	WORD Z;

	FQuatFixed48NoW()
	{}

	explicit FQuatFixed48NoW(const FQuat& Quat)
	{
		FromQuat( Quat );
	}

	void FromQuat(const FQuat& Quat)
	{
		FQuat Temp( Quat );
		if ( Temp.W < 0.f )
		{
			Temp.X = -Temp.X;
			Temp.Y = -Temp.Y;
			Temp.Z = -Temp.Z;
			Temp.W = -Temp.W;
		}
		Temp.Normalize();

		X = (INT)(Temp.X * Quant16BitFactor) + Quant16BitOffs;
		Y = (INT)(Temp.Y * Quant16BitFactor) + Quant16BitOffs;
		Z = (INT)(Temp.Z * Quant16BitFactor) + Quant16BitOffs;
	}

	void ToQuat(FQuat& Out) const
	{
		const FLOAT FX = ((INT)X - (INT)Quant16BitOffs) / Quant16BitDiv;
		const FLOAT FY = ((INT)Y - (INT)Quant16BitOffs) / Quant16BitDiv;
		const FLOAT FZ = ((INT)Z - (INT)Quant16BitOffs) / Quant16BitDiv;
		const FLOAT WSquared = 1.f - FX*FX - FY*FY - FZ*FZ;

		Out.X = FX;
		Out.Y = FY;
		Out.Z = FZ;
		Out.W = WSquared > 0.f ? appSqrt( WSquared ) : 0.f;
	}

	friend FArchive& operator<<(FArchive& Ar, FQuatFixed48NoW& Quat)
	{
		Ar << Quat.X;
		Ar << Quat.Y;
		Ar << Quat.Z;
		return Ar;
	}
};

class FQuatFixed32NoW
{
public:
	DWORD Packed;

	FQuatFixed32NoW()
	{}

	explicit FQuatFixed32NoW(const FQuat& Quat)
	{
		FromQuat( Quat );
	}

	void FromQuat(const FQuat& Quat)
	{
		FQuat Temp( Quat );
		if ( Temp.W < 0.f )
		{
			Temp.X = -Temp.X;
			Temp.Y = -Temp.Y;
			Temp.Z = -Temp.Z;
			Temp.W = -Temp.W;
		}
		Temp.Normalize();

		const DWORD PackedX = (INT)(Temp.X * Quant11BitFactor) + Quant11BitOffs;
		const DWORD PackedY = (INT)(Temp.Y * Quant11BitFactor) + Quant11BitOffs;
		const DWORD PackedZ = (INT)(Temp.Z * Quant10BitFactor) + Quant10BitOffs;

		// 21-31 X, 10-20 Y, 0-9 Z.
		const DWORD XShift = 21;
		const DWORD YShift = 10;
		Packed = (PackedX << XShift) | (PackedY << YShift) | (PackedZ);
	}

	void ToQuat(FQuat& Out) const
	{
		const DWORD XShift = 21;
		const DWORD YShift = 10;
		const DWORD ZMask = 0x000003ff;
		const DWORD YMask = 0x001ffc00;
		const DWORD XMask = 0xffe00000;

		const DWORD UnpackedX = Packed >> XShift;
		const DWORD UnpackedY = (Packed & YMask) >> YShift;
		const DWORD UnpackedZ = (Packed & ZMask);

		const FLOAT X = ((INT)UnpackedX - (INT)Quant11BitOffs) / Quant11BitDiv;
		const FLOAT Y = ((INT)UnpackedY - (INT)Quant11BitOffs) / Quant11BitDiv;
		const FLOAT Z = ((INT)UnpackedZ - (INT)Quant10BitOffs) / Quant10BitDiv;
		const FLOAT WSquared = 1.f - X*X - Y*Y - Z*Z;

		Out.X = X;
		Out.Y = Y;
		Out.Z = Z;
		Out.W = WSquared > 0.f ? appSqrt( WSquared ) : 0.f;
	}

	friend FArchive& operator<<(FArchive& Ar, FQuatFixed32NoW& Quat)
	{
		Ar << Quat.Packed;
		return Ar;
	}
};

class FQuatFloat96NoW
{
public:
	FLOAT X;
	FLOAT Y;
	FLOAT Z;

	FQuatFloat96NoW()
	{}

	explicit FQuatFloat96NoW(const FQuat& Quat)
	{
		FromQuat( Quat );
	}

	FQuatFloat96NoW(FLOAT InX, FLOAT InY, FLOAT InZ)
		:	X( InX )
		,	Y( InY )
		,	Z( InZ )
	{}

	void FromQuat(const FQuat& Quat)
	{
		FQuat Temp( Quat );
		if ( Temp.W < 0.f )
		{
			Temp.X = -Temp.X;
			Temp.Y = -Temp.Y;
			Temp.Z = -Temp.Z;
			Temp.W = -Temp.W;
		}
		Temp.Normalize();
		X = Temp.X;
		Y = Temp.Y;
		Z = Temp.Z;
	}

	void ToQuat(FQuat& Out) const
	{
		const FLOAT WSquared = 1.f - X*X - Y*Y - Z*Z;

		Out.X = X;
		Out.Y = Y;
		Out.Z = Z;
		Out.W = WSquared > 0.f ? appSqrt( WSquared ) : 0.f;
	}

	friend FArchive& operator<<(FArchive& Ar, FQuatFloat96NoW& Quat)
	{
		Ar << Quat.X;
		Ar << Quat.Y;
		Ar << Quat.Z;
		return Ar;
	}
};



class FVectorFixed48
{
public:
	WORD X;
	WORD Y;
	WORD Z;

	FVectorFixed48()
	{}

	explicit FVectorFixed48(const FVector& Vec)
	{
		FromVector( Vec );
	}

	void FromVector(const FVector& Vec)
	{
		FVector Temp( Vec / 128.0f );

		X = (INT)(Temp.X * Quant16BitFactor) + Quant16BitOffs;
		Y = (INT)(Temp.Y * Quant16BitFactor) + Quant16BitOffs;
		Z = (INT)(Temp.Z * Quant16BitFactor) + Quant16BitOffs;
	}

	void ToVector(FVector& Out) const
	{
		const FLOAT FX = ((INT)X - (INT)Quant16BitOffs) / Quant16BitDiv;
		const FLOAT FY = ((INT)Y - (INT)Quant16BitOffs) / Quant16BitDiv;
		const FLOAT FZ = ((INT)Z - (INT)Quant16BitOffs) / Quant16BitDiv;

		Out.X = FX * 128.0f;
		Out.Y = FY * 128.0f;
		Out.Z = FZ * 128.0f;
	}

	friend FArchive& operator<<(FArchive& Ar, FVectorFixed48& Vec)
	{
		Ar << Vec.X;
		Ar << Vec.Y;
		Ar << Vec.Z;
		return Ar;
	}
};

class FVectorIntervalFixed32NoW
{
public:
	DWORD Packed;

	FVectorIntervalFixed32NoW()
	{}

	explicit FVectorIntervalFixed32NoW(const FVector& Value, const FLOAT* Mins, const FLOAT *Ranges)
	{
		FromVector( Value, Mins, Ranges );
	}

	void FromVector(const FVector& Value, const FLOAT* Mins, const FLOAT *Ranges)
	{
		FVector Temp( Value );

		Temp.X -= Mins[0];
		Temp.Y -= Mins[1];
		Temp.Z -= Mins[2];

		const DWORD PackedX = (INT)((Temp.X / Ranges[0]) * Quant10BitFactor ) + Quant10BitOffs;
		const DWORD PackedY = (INT)((Temp.Y / Ranges[1]) * Quant11BitFactor ) + Quant11BitOffs;
		const DWORD PackedZ = (INT)((Temp.Z / Ranges[2]) * Quant11BitFactor ) + Quant11BitOffs;

		// 21-31 Z, 10-20 Y, 0-9 X.
		const DWORD ZShift = 21;
		const DWORD YShift = 10;
		Packed = (PackedZ << ZShift) | (PackedY << YShift) | (PackedX);
	}

	void ToVector(FVector& Out, const FLOAT* Mins, const FLOAT *Ranges) const
	{
		const DWORD ZShift = 21;
		const DWORD YShift = 10;
		const DWORD XMask = 0x000003ff;
		const DWORD YMask = 0x001ffc00;
		const DWORD ZMask = 0xffe00000;

		const DWORD UnpackedZ = Packed >> ZShift;
		const DWORD UnpackedY = (Packed & YMask) >> YShift;
		const DWORD UnpackedX = (Packed & XMask);

		const FLOAT X = ( (((INT)UnpackedX - (INT)Quant10BitOffs) / Quant10BitDiv) * Ranges[0] + Mins[0] );
		const FLOAT Y = ( (((INT)UnpackedY - (INT)Quant11BitOffs) / Quant11BitDiv) * Ranges[1] + Mins[1] );
		const FLOAT Z = ( (((INT)UnpackedZ - (INT)Quant11BitOffs) / Quant11BitDiv) * Ranges[2] + Mins[2] );

		Out.X = X;
		Out.Y = Y;
		Out.Z = Z;
	}

	friend FArchive& operator<<(FArchive& Ar, FVectorIntervalFixed32NoW& Value)
	{
		Ar << Value.Packed;
		return Ar;
	}
};


class FQuatIntervalFixed32NoW
{
public:
	DWORD Packed;

	FQuatIntervalFixed32NoW()
	{}

	explicit FQuatIntervalFixed32NoW(const FQuat& Quat, const FLOAT* Mins, const FLOAT *Ranges)
	{
		FromQuat( Quat, Mins, Ranges );
	}

	void FromQuat(const FQuat& Quat, const FLOAT* Mins, const FLOAT *Ranges)
	{
		FQuat Temp( Quat );
		if ( Temp.W < 0.f )
		{
			Temp.X = -Temp.X;
			Temp.Y = -Temp.Y;
			Temp.Z = -Temp.Z;
			Temp.W = -Temp.W;
		}
		Temp.Normalize();

		Temp.X -= Mins[0];
		Temp.Y -= Mins[1];
		Temp.Z -= Mins[2];

		const DWORD PackedX = (INT)((Temp.X / Ranges[0]) * Quant11BitFactor ) + Quant11BitOffs;
		const DWORD PackedY = (INT)((Temp.Y / Ranges[1]) * Quant11BitFactor ) + Quant11BitOffs;
		const DWORD PackedZ = (INT)((Temp.Z / Ranges[2]) * Quant10BitFactor ) + Quant10BitOffs;

		// 21-31 X, 10-20 Y, 0-9 Z.
		const DWORD XShift = 21;
		const DWORD YShift = 10;
		Packed = (PackedX << XShift) | (PackedY << YShift) | (PackedZ);
	}

	void ToQuat(FQuat& Out, const FLOAT* Mins, const FLOAT *Ranges) const
	{
		const DWORD XShift = 21;
		const DWORD YShift = 10;
		const DWORD ZMask = 0x000003ff;
		const DWORD YMask = 0x001ffc00;
		const DWORD XMask = 0xffe00000;

		const DWORD UnpackedX = Packed >> XShift;
		const DWORD UnpackedY = (Packed & YMask) >> YShift;
		const DWORD UnpackedZ = (Packed & ZMask);

		const FLOAT X = ( (((INT)UnpackedX - (INT)Quant11BitOffs) / Quant11BitDiv) * Ranges[0] + Mins[0] );
		const FLOAT Y = ( (((INT)UnpackedY - (INT)Quant11BitOffs) / Quant11BitDiv) * Ranges[1] + Mins[1] );
		const FLOAT Z = ( (((INT)UnpackedZ - (INT)Quant10BitOffs) / Quant10BitDiv) * Ranges[2] + Mins[2] );
		const FLOAT WSquared = 1.f - X*X - Y*Y - Z*Z;

		Out.X = X;
		Out.Y = Y;
		Out.Z = Z;
		Out.W = WSquared > 0.f ? appSqrt( WSquared ) : 0.f;
	}

	friend FArchive& operator<<(FArchive& Ar, FQuatIntervalFixed32NoW& Quat)
	{
		Ar << Quat.Packed;
		return Ar;
	}
};

class FQuatFloat32NoW
{
public:
	DWORD Packed;

	FQuatFloat32NoW()
	{}

	explicit FQuatFloat32NoW(const FQuat& Quat)
	{
		FromQuat( Quat );
	}

	void FromQuat(const FQuat& Quat)
	{
		FQuat Temp( Quat );
		if ( Temp.W < 0.f )
		{
			Temp.X = -Temp.X;
			Temp.Y = -Temp.Y;
			Temp.Z = -Temp.Z;
			Temp.W = -Temp.W;
		}
		Temp.Normalize();

		TFloatPacker<3, 7, TRUE> Packer7e3;
		TFloatPacker<3, 6, TRUE> Packer6e3;

		const DWORD PackedX = Packer7e3.Encode( Temp.X );
		const DWORD PackedY = Packer7e3.Encode( Temp.Y );
		const DWORD PackedZ = Packer6e3.Encode( Temp.Z );

		// 21-31 X, 10-20 Y, 0-9 Z.
		const DWORD XShift = 21;
		const DWORD YShift = 10;
		Packed = (PackedX << XShift) | (PackedY << YShift) | (PackedZ);
	}

	void ToQuat(FQuat& Out) const
	{
		const DWORD XShift = 21;
		const DWORD YShift = 10;
		const DWORD ZMask = 0x000003ff;
		const DWORD YMask = 0x001ffc00;
		const DWORD XMask = 0xffe00000;

		const DWORD UnpackedX = Packed >> XShift;
		const DWORD UnpackedY = (Packed & YMask) >> YShift;
		const DWORD UnpackedZ = (Packed & ZMask);

		TFloatPacker<3, 7, TRUE> Packer7e3;
		TFloatPacker<3, 6, TRUE> Packer6e3;

		const FLOAT X = Packer7e3.Decode( UnpackedX );
		const FLOAT Y = Packer7e3.Decode( UnpackedY );
		const FLOAT Z = Packer6e3.Decode( UnpackedZ );
		const FLOAT WSquared = 1.f - X*X - Y*Y - Z*Z;

		Out.X = X;
		Out.Y = Y;
		Out.Z = Z;
		Out.W = WSquared > 0.f ? appSqrt( WSquared ) : 0.f;
	}

	friend FArchive& operator<<(FArchive& Ar, FQuatFloat32NoW& Quat)
	{
		Ar << Quat.Packed;
		return Ar;
	}
};

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Handy Template Decompressors
//
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Templated Rotation Decompressor. Generates a unique decompressor per known quantization format
 *
 * @param	Out				The FQuat to fill in.
 * @param	TopOfStream		The start of the compressed rotation stream data.
 * @param	KeyData			The compressed rotation data to decompress.
 * @return	None. 
 */
template <INT FORMAT>
FORCEINLINE void DecompressRotation(FQuat& Out, const BYTE* RESTRICT TopOfStream, const BYTE* RESTRICT KeyData)
{
	// this if-else stack gets compiled away to a single result based on the template parameter
	if ( FORMAT == ACF_None )
	{
		Out = *((FQuat*)KeyData);
	}
	else if ( FORMAT == ACF_Float96NoW )
	{
		((FQuatFloat96NoW*)KeyData)->ToQuat( Out );
	}
	else if ( FORMAT == ACF_Fixed32NoW )
	{
		((FQuatFixed32NoW*)KeyData)->ToQuat( Out );
	}
	else if ( FORMAT == ACF_Fixed48NoW )
	{
		((FQuatFixed48NoW*)KeyData)->ToQuat( Out );
	}
	else if ( FORMAT == ACF_IntervalFixed32NoW )
	{
		const FLOAT* RESTRICT Mins = (FLOAT*)TopOfStream;
		const FLOAT* RESTRICT Ranges = (FLOAT*)(TopOfStream+sizeof(FLOAT)*3);
		((FQuatIntervalFixed32NoW*)KeyData)->ToQuat( Out, Mins, Ranges );
	}
	else if ( FORMAT == ACF_Float32NoW )
	{
		((FQuatFloat32NoW*)KeyData)->ToQuat( Out );
	}
	else if ( FORMAT == ACF_Identity )
	{
		Out = FQuat::Identity;
	}
	else
	{
		appErrorf( TEXT("%i: unknown or unsupported animation compression format"), (INT)FORMAT );
		Out = FQuat::Identity;
	}
}

/**
 * Templated Translation Decompressor. Generates a unique decompressor per known quantization format
 *
 * @param	Out				The FVector to fill in.
 * @param	TopOfStream		The start of the compressed translation stream data.
 * @param	KeyData			The compressed translation data to decompress.
 * @return	None. 
 */
template <INT FORMAT>
FORCEINLINE void DecompressTranslation(FVector& Out, const BYTE* RESTRICT TopOfStream, const BYTE* RESTRICT KeyData)
{
	if ( (FORMAT == ACF_None) || (FORMAT == ACF_Float96NoW) )
	{
		Out = *((FVector*)KeyData);
	}
	else if ( FORMAT == ACF_IntervalFixed32NoW )
	{
		const FLOAT* RESTRICT Mins = (FLOAT*)TopOfStream;
		const FLOAT* RESTRICT Ranges = (FLOAT*)(TopOfStream+sizeof(FLOAT)*3);
		((FVectorIntervalFixed32NoW*)KeyData)->ToVector( Out, Mins, Ranges );
	}
	else if ( FORMAT == ACF_Identity )
	{
		Out = FVector::ZeroVector;
	}
	else if ( FORMAT == ACF_Fixed48NoW )
	{
		((FVectorFixed48*)KeyData)->ToVector( Out );
	}
	else
	{
		appErrorf( TEXT("%i: unknown or unsupported animation compression format"), (INT)FORMAT );
		// Silence compilers warning about a value potentially not being assigned.
		Out = FVector::ZeroVector;
	}
}



/**
 * This class contains helper methods for dealing with animations compressed with the per-track codec
 */
class FAnimationCompression_PerTrackUtils
{
public:
	// Log2MaxValue of 0 => -1..1
	// Log2MaxValue of 7 => -128..128
	// Can be 0..15
	/**
	 * Compresses a float into a signed fixed point number, which can range from the symmetrical values of
	 * -2^Log2MaxValue .. 2^Log2MaxValue.  No clamping is done, values that don't fit will overflow.
	 *
	 * For example, a Log2MaxValue of 0 can encode -1..1, and 7 can encode -128..128.
	 *
	 * @param Value			Value to encode
	 * @param Log2MaxValue	Encoding range (can be 0..15)
	 *
	 * @return The quantized value
	 */
	static WORD CompressFixed16(FLOAT Value, INT Log2MaxValue = 0)
	{
		const INT QuantOffset = (1 << (15 - Log2MaxValue)) - 1;
		const FLOAT QuantFactor = (FLOAT)(QuantOffset >> Log2MaxValue);
		return (WORD)((INT)(Value * QuantFactor) + QuantOffset);
	}

	/**
	 * Decompresses a fixed point number encoded by ComrpessFixed16
	 *
	 * @param Value			Value to decode
	 * @param Log2MaxValue	Encoding range (can be 0..15)
	 *
	 * @return The decompressed value
	 */
	template <INT Log2MaxValue>
	static FLOAT DecompressFixed16(WORD Value)
	{
		const INT QuantOffset = (1 << (15 - Log2MaxValue)) - 1;
		const FLOAT InvQuantFactor = 1.0f / (FLOAT)(QuantOffset >> Log2MaxValue);

		return ((INT)Value - QuantOffset) * InvQuantFactor;
	}

	/**
	 * Creates a header integer with four fields:
	 *   NumKeys can be no more than 24 bits (positions 0..23)
	 *   KeyFlags can be no more than 3 bits (positions 24..27)
	 *   bReallyNeedsFrameTable is a single bit (position 27)
	 *   KeyFormat can be no more than 4 bits (positions 31..28)
	 */
	static INT MakeHeader(const INT NumKeys, const INT KeyFormat, const INT KeyFlags, UBOOL bReallyNeedsFrameTable)
	{
		return (NumKeys & 0x00FFFFFF) | ((KeyFormat & 0xF) << 28) | ((KeyFlags & 0x7) << 24) | ((bReallyNeedsFrameTable & 0x1) << 27);
	}

	/**
	 * Extracts the number of keys from a header created by MakeHeader
	 *
	 * @param Header	Header to extract the number of keys from
	 * @return			The number of keys encoded in the header
	 */
	static INT GetKeyCountFromHeader(INT Header)
	{
		return Header & 0x00FFFFFF;
	}

	/**
	 * Figures out the size of various parts of a compressed track from the format and format flags combo
	 *   @param KeyFormat		The encoding format used for each key
	 *   @param FormatFlags		Three bits of format-specific information and a single bit to indicate if a key->frame table follows the keys
	 *
	 */
	static void GetAllSizesFromFormat(INT KeyFormat, INT FormatFlags,
		INT& KeyComponentCount, INT& KeyComponentSize,
		INT& FixedComponentCount, INT& FixedComponentSize)
	{
		extern const INT CompressedRotationStrides[ACF_MAX];
		extern const BYTE PerTrackNumComponentTable[ACF_MAX * 8];

		// Note: this method can be used for translation too, but animation sequences compressed with this codec will
		// use ACF_Float96NoW for uncompressed translation, so using the rotation table is still valid
		KeyComponentSize = CompressedRotationStrides[KeyFormat];
		FixedComponentSize = sizeof(FLOAT);

		const INT ComponentLookup = PerTrackNumComponentTable[(FormatFlags & 0x7) | (KeyFormat << 3)];

		if (KeyFormat != ACF_IntervalFixed32NoW)
		{
			FixedComponentCount = 0;
			KeyComponentCount = ComponentLookup;
		}
		else
		{
			// Min/Range floats for all non-zero channels
			FixedComponentCount = ComponentLookup;
			KeyComponentCount = 1;
		}
	}

	static FORCEINLINE void GetByteSizesFromFormat(INT KeyFormat, INT FormatFlags, INT& BytesPerKey, INT& FixedBytes)
	{
		INT FixedComponentSize = 0;
		INT FixedComponentCount = 0;
		INT KeyComponentSize = 0;
		INT KeyComponentCount = 0;

		GetAllSizesFromFormat(KeyFormat, FormatFlags, /*OUT*/ KeyComponentCount, /*OUT*/ KeyComponentSize, /*OUT*/ FixedComponentCount, /*OUT*/ FixedComponentSize);

		BytesPerKey = KeyComponentCount * KeyComponentSize;
		FixedBytes = FixedComponentCount * FixedComponentSize;
	}

	/**
	 * Decomposes a header created with MakeHeader into three/four fields (two are still left packed into FormatFlags):
	 *   @param Header				The header to decompose
	 *   @param KeyFormat [OUT]		The encoding format used for each key
	 *   @param	NumKeys	[OUT]		The number of keys in this track
	 *   @param FormatFlags [OUT]	Three bits of format-specific information and a single bit to indicate if a key->frame table follows the keys
	 */
	static FORCEINLINE void DecomposeHeader(INT Header, INT& KeyFormat, INT& NumKeys, INT& FormatFlags)
	{
		NumKeys = Header & 0x00FFFFFF;
		FormatFlags = (Header >> 24) & 0x0F;
		KeyFormat = (Header >> 28) & 0x0F;
	}

	/**
	 * Decomposes a header created with MakeHeader into three/four fields (two are still left packed into FormatFlags):
	 *   @param Header				The header to decompose
	 *   @param KeyFormat [OUT]		The encoding format used for each key
	 *   @param	NumKeys	[OUT]		The number of keys in this track
	 *   @param FormatFlags [OUT]	Three bits of format-specific information and a single bit to indicate if a key->frame table follows the keys
	 *
	 * And some derived values:
	 *   @param	BytesPerKey [OUT]	The number of bytes each key takes up
	 *   @param	FixedBytes [OUT]	The number of fixed bytes at the head of the track stream
	 */
	static FORCEINLINE void DecomposeHeader(INT Header, INT& KeyFormat, INT& NumKeys, INT& FormatFlags, INT& BytesPerKey, INT& FixedBytes)
	{
		NumKeys = Header & 0x00FFFFFF;
		FormatFlags = (Header >> 24) & 0x0F;
		KeyFormat = (Header >> 28) & 0x0F;

		// Figure out the component sizes / counts (they can be changed per-format)
		GetByteSizesFromFormat(KeyFormat, FormatFlags, /*OUT*/ BytesPerKey, /*OUT*/ FixedBytes);
	}

	/** Decompress a single translation key from a single track that was compressed with the PerTrack codec (scalar) */
	static FORCEINLINE_DEBUGGABLE void DecompressTranslation(INT Format, INT FormatFlags, FVector& Out, const BYTE* RESTRICT TopOfStream, const BYTE* RESTRICT KeyData)
	{
		if( Format == ACF_Float96NoW )
		{
			// Legacy Format, all components stored
			if( (FormatFlags & 7) == 0 )
			{
				Out = *((FVector*)KeyData);
			}
			// Stored per components
			else
			{
				const FLOAT* RESTRICT TypedKeyData = (const FLOAT*)KeyData;
				if (FormatFlags & 1)
				{
					Out.X = *TypedKeyData++;
				}
				else
				{
					Out.X = 0.0f;
				}

				if (FormatFlags & 2)
				{
					Out.Y = *TypedKeyData++;
				}
				else
				{
					Out.Y = 0.0f;
				}

				if (FormatFlags & 4)
				{
					Out.Z = *TypedKeyData++;
				}
				else
				{
					Out.Z = 0.0f;
				}
			}
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

			((FVectorIntervalFixed32NoW*)KeyData)->ToVector(Out, Mins, Ranges);
		}
		else if (Format == ACF_Fixed48NoW)
		{
			//@TODO: Explore different scales
			const INT LogScale = 7;
			const WORD* RESTRICT TypedKeyData = (const WORD*)KeyData;
			if (FormatFlags & 1)
			{
				Out.X = FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(*TypedKeyData++);
			}
			else
			{
				Out.X = 0.0f;
			}

			if (FormatFlags & 2)
			{
				Out.Y = FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(*TypedKeyData++);
			}
			else
			{
				Out.Y = 0.0f;
			}

			if (FormatFlags & 4)
			{
				Out.Z = FAnimationCompression_PerTrackUtils::DecompressFixed16<LogScale>(*TypedKeyData++);
			}
			else
			{
				Out.Z = 0.0f;
			}
		}
		else if ( Format == ACF_Identity )
		{
			Out = FVector::ZeroVector;
		}
		else
		{
			appErrorf( TEXT("%i: unknown or unsupported animation compression format"), (INT)Format );
			// Silence compilers warning about a value potentially not being assigned.
			Out = FVector::ZeroVector;
		}
	}

	/** Decompress a single rotation key from a single track that was compressed with the PerTrack codec (scalar) */
	static FORCEINLINE_DEBUGGABLE void DecompressRotation(INT Format, INT FormatFlags, FQuat& Out, const BYTE* RESTRICT TopOfStream, const BYTE* RESTRICT KeyData)
	{
		if (Format == ACF_Fixed48NoW)
		{
			const WORD* RESTRICT TypedKeyData = (const WORD*)KeyData;

			const FLOAT Xa = (FormatFlags & 1) ? (FLOAT)(*TypedKeyData++) : 32767.0f;
			const FLOAT Ya = (FormatFlags & 2) ? (FLOAT)(*TypedKeyData++) : 32767.0f;
			const FLOAT Za = (FormatFlags & 4) ? (FLOAT)(*TypedKeyData++) : 32767.0f;

			const FLOAT X = (Xa - 32767.0f) * 3.0518509475997192297128208258309e-5f;
			const FLOAT XX = X*X;
			const FLOAT Y = (Ya - 32767.0f) * 3.0518509475997192297128208258309e-5f;
			const FLOAT YY = Y*Y;
			const FLOAT Z = (Za - 32767.0f) * 3.0518509475997192297128208258309e-5f;
			const FLOAT ZZ = Z*Z;

			const FLOAT WSquared = 1.0f - XX - YY - ZZ;

			const FLOAT W = appFloatSelect(WSquared, appSqrt(WSquared), 0.0f);

			Out = FQuat(X, Y, Z, W);
		}
		else if (Format == ACF_Float96NoW)
		{
			((FQuatFloat96NoW*)KeyData)->ToQuat(Out);
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

			((FQuatIntervalFixed32NoW*)KeyData)->ToQuat( Out, Mins, Ranges );
		}
		else if ( Format == ACF_Float32NoW )
		{
			((FQuatFloat32NoW*)KeyData)->ToQuat( Out );
		}
		else if (Format == ACF_Fixed32NoW)
		{
			((FQuatFixed32NoW*)KeyData)->ToQuat(Out);
		}
		else if ( Format == ACF_Identity )
		{
			Out = FQuat::Identity;
		}
		else
		{
			appErrorf( TEXT("%i: unknown or unsupported animation compression format"), (INT)Format );
			Out = FQuat::Identity;
		}
	}
};

template <>
inline FLOAT FAnimationCompression_PerTrackUtils::DecompressFixed16<0>(WORD Value)
{
	return ((INT)Value - 32767) * 3.0518509475997192297128208258309e-5f;
}

#endif // __ANIMATIONCOMPRESSION_H__
