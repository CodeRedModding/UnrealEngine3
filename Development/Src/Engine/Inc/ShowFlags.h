/*=============================================================================
	ShowFlags.h: Show Flag Definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_SHOW_FLAGS
#define _INC_SHOW_FLAGS

/*-----------------------------------------------------------------------------
	Size of EShowFlags
-----------------------------------------------------------------------------*/
#if CONSOLE && FINAL_RELEASE
struct FShippingShowFlags
{
	//default constructor
	FShippingShowFlags()
	{
		InternalFlags = 0;
	}
	//default value specified
	FShippingShowFlags(DWORD DefaultFlags)
	{
		InternalFlags = DefaultFlags;
	}

	//binary bitwise AND
	FORCEINLINE DWORD operator&(const DWORD MaskFlags) const
	{
		//force the required bit (see SHOW_RESERVED_FLAG)
		return (InternalFlags | (DWORD)0x01) & (MaskFlags);
	}
	FORCEINLINE DWORD operator&(const FShippingShowFlags Mask) const
	{
		//force the required bit (see SHOW_RESERVED_FLAG)
		return (InternalFlags | (DWORD)0x01) & (Mask.InternalFlags);
	}
	//unary bitwise AND
	FORCEINLINE DWORD operator&=(const FShippingShowFlags Mask)
	{
		InternalFlags &= Mask.InternalFlags;
		return InternalFlags;
	}
	//binary bitwise OR
	FORCEINLINE FShippingShowFlags operator|(const FShippingShowFlags Mask) const
	{
		//force the required bit (see SHOW_RESERVED_FLAG)
		return FShippingShowFlags(InternalFlags | Mask.InternalFlags);
	}
	//unary bitwise OR
	FORCEINLINE void operator|=(const FShippingShowFlags Mask)
	{
		InternalFlags |= Mask.InternalFlags;
	}
	//bitwise negate
	FORCEINLINE DWORD operator~() const
	{
		return ~InternalFlags;
	}
	//binary xor
	FORCEINLINE DWORD operator^(const FShippingShowFlags Mask)
	{
		return InternalFlags ^ Mask.InternalFlags;
	}
	//unary xor
	FORCEINLINE void operator^=(const FShippingShowFlags Mask)
	{
		InternalFlags ^= Mask.InternalFlags;
	}

	//Comparison operators
	FORCEINLINE bool operator==(const FShippingShowFlags Mask) const
	{
		return (InternalFlags == Mask.InternalFlags);
	}
	FORCEINLINE bool operator!=(const FShippingShowFlags Mask) const
	{
		return (InternalFlags != Mask.InternalFlags);
	}

private:
	DWORD InternalFlags;
	//sizes much match between pc and xbox.
	DWORD UnusedDWORDPadding;
	QWORD UnusedQWORDPadding;
};
typedef FShippingShowFlags EShowFlags;
#else
typedef TStaticBitArray<128> EShowFlags;
#endif

#endif // _INC_SHOW_FLAGS

