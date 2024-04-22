/*=============================================================================
	FloatPacker.h:
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FLOATPACKER_H__
#define __FLOATPACKER_H__

/**
 *
 */
class FFloatInfo_IEEE32
{
public:
	enum { MantissaBits	= 23 };
	enum { ExponentBits	= 8 };
	enum { SignShift	= 31 };
	enum { ExponentBias	= 127 };

	enum { MantissaMask	= 0x007fffff };
	enum { ExponentMask	= 0x7f800000 };
	enum { SignMask		= 0x80000000 };

	typedef FLOAT	FloatType;
	typedef DWORD	PackedType;

	static PackedType ToPackedType(FloatType Value)
	{
		return *(PackedType*)&Value;
	}

	static FloatType ToFloatType(PackedType Value)
	{
		return *(FloatType*)&Value;
	}
};

/**
 *
 */
template<DWORD NumExponentBits, DWORD NumMantissaBits, UBOOL bRound, typename FloatInfo=FFloatInfo_IEEE32>
class TFloatPacker
{
public:
	enum { NumOutputsBits	= NumExponentBits + NumMantissaBits + 1			};

	enum { MantissaShift	= FloatInfo::MantissaBits - NumMantissaBits	};
	enum { ExponentBias		= (1 << (NumExponentBits-1)) - 1				};
	enum { SignShift		= NumExponentBits + NumMantissaBits				};

	enum { MantissaMask		= (1 << NumMantissaBits) - 1					};
	enum { ExponentMask		= ((1 << NumExponentBits)-1) << NumMantissaBits	};
	enum { SignMask			=  1 << SignShift								};

	enum { MinExponent		= -ExponentBias - 1								};
	enum { MaxExponent		= ExponentBias									};

	typedef typename FloatInfo::PackedType	PackedType;
	typedef typename FloatInfo::FloatType	FloatType;

	PackedType Encode(FloatType Value) const
	{
		if ( Value == (FloatType) 0.0 )
		{
			return (PackedType) 0;
		}

		const PackedType ValuePacked = FloatInfo::ToPackedType( Value );

		// Extract mantissa, exponent, sign.
				PackedType	Mantissa	= ValuePacked & FloatInfo::MantissaMask;
				INT			Exponent	= (ValuePacked & FloatInfo::ExponentMask) >> FloatInfo::MantissaBits;
		const	PackedType	Sign		= ValuePacked >> FloatInfo::SignShift;

		// Subtract IEEE's bias.
		Exponent -= FloatInfo::ExponentBias;

		if ( bRound )
		{
			Mantissa += (1 << (MantissaShift-1));
			if ( Mantissa & (1 << FloatInfo::MantissaBits) )
			{
				Mantissa = 0;
				++Exponent;
			}
		}

		// Shift the mantissa to the right
		Mantissa >>= MantissaShift;

		//debugf( TEXT("fp: exp: %i (%i,%i)"), Exponent, (INT)MinExponent, (INT)MaxExponent );
		if ( Exponent < MinExponent )
		{
			return (PackedType) 0;
		}
		if ( Exponent > MaxExponent )
		{
			Exponent = MaxExponent;
		}

		// Add our bias.
		Exponent -= MinExponent;

		return (Sign << SignShift) | (Exponent << NumMantissaBits) | (Mantissa);
	}

	FloatType Decode(PackedType Value) const
	{
		if ( Value == (PackedType) 0 )
		{
			return (FloatType) 0.0;
		}

		// Extract mantissa, exponent, sign.
				PackedType	Mantissa	= Value & MantissaMask;
				INT			Exponent	= (Value & ExponentMask) >> NumMantissaBits;
		const	PackedType	Sign		= Value >> SignShift;

		// Subtract our bias.
		Exponent += MinExponent;
		// Add IEEE's bias.
		Exponent += FloatInfo::ExponentBias;

		Mantissa <<= MantissaShift;

		return FloatInfo::ToFloatType( (Sign << FloatInfo::SignShift) | (Exponent << FloatInfo::MantissaBits) | (Mantissa) );
	}
#if 0
	PackedType EncodeNoSign(FloatType Value)
	{
		if ( Value == (FloatType) 0.0 )
		{
			return (PackedType) 0;
		}

		const PackedType ValuePacked = FloatInfo::ToPackedType( Value );

		// Extract mantissa, exponent, sign.
				PackedType	Mantissa	= ValuePacked & FloatInfo::MantissaMask;
				INT			Exponent	= (ValuePacked & FloatInfo::ExponentMask) >> FloatInfo::MantissaBits;
		//const	PackedType	Sign		= ValuePacked >> FloatInfo::SignShift;

		// Subtract IEEE's bias.
		Exponent -= FloatInfo::ExponentBias;

		if ( bRound )
		{
			Mantissa += (1 << (MantissaShift-1));
			if ( Mantissa & (1 << FloatInfo::MantissaBits) )
			{
				Mantissa = 0;
				++Exponent;
			}
		}

		// Shift the mantissa to the right
		Mantissa >>= MantissaShift;

		//debugf( TEXT("fp: exp: %i (%i,%i)"), Exponent, (INT)MinExponent, (INT)MaxExponent );
		if ( Exponent < MinExponent )
		{
			if ( Exponent < MinExponent-1 )
			{
				return (PackedType) 0;
			}
			Exponent = MinExponent;
		}
		if ( Exponent > MaxExponent )
		{
			Exponent = MaxExponent;
		}

		// Add our bias.
		Exponent -= MinExponent;

		return (Exponent << NumMantissaBits) | (Mantissa);
	}

	FloatType DecodeNoSign(PackedType Value)
	{
		if ( Value == (PackedType) 0 )
		{
			return (FloatType) 0.0;
		}

		// Extract mantissa, exponent, sign.
		PackedType	Mantissa	= Value & MantissaMask;
		INT			Exponent	= (Value & ExponentMask) >> NumMantissaBits;
		//const	PackedType	Sign		= Value >> SignShift;

		// Subtract our bias.
		Exponent += MinExponent;
		// Add IEEE's bias.
		Exponent += FloatInfo::ExponentBias;

		Mantissa <<= MantissaShift;

		return FloatInfo::ToFloatType( (Exponent << FloatInfo::MantissaBits) | (Mantissa) );
	}
#endif
private:
//#define checkAtCompileTime2(expr)  typedef BYTE CompileTimeCheckType##__LINE__[(expr) ? 1 : -1]
	//checkAtCompileTime2(NumExponentBits>0 && NumExponentBits<=FFloatInfo::ExponentBits);
	//checkAtCompileTime2(NumMantissaBits>0 && NumMantissaBits<=FFloatInfo::MantissaBits);
};

#endif	// __FLOATPACKER_H__
