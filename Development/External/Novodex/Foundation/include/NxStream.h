#ifndef NX_FOUNDATION_NXSTREAM
#define NX_FOUNDATION_NXSTREAM
/*----------------------------------------------------------------------------*\
|
|								NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

class NxStream
{
	public:
								NxStream()				{}
	virtual						~NxStream()				{}

	// Loading API
	virtual		NxU8			readByte()								const	= 0;
	virtual		NxU16			readWord()								const	= 0;
	virtual		NxU32			readDword()								const	= 0;
	virtual		NxF32			readFloat()								const	= 0;
	virtual		NxF64			readDouble()							const	= 0;
	virtual		void			readBuffer(void* buffer, NxU32 size)	const	= 0;

	// Saving API
	virtual		NxStream&		storeByte(NxU8 b)								= 0;
	virtual		NxStream&		storeWord(NxU16 w)								= 0;
	virtual		NxStream&		storeDword(NxU32 d)								= 0;
	virtual		NxStream&		storeFloat(NxF32 f)								= 0;
	virtual		NxStream&		storeDouble(NxF64 f)							= 0;
	virtual		NxStream&		storeBuffer(const void* buffer, NxU32 size)		= 0;

	NX_INLINE	NxStream&		storeByte(NxI8 b)		{ return storeByte(NxU8(b));	}
	NX_INLINE	NxStream&		storeWord(NxI16 w)		{ return storeWord(NxU16(w));	}
	NX_INLINE	NxStream&		storeDword(NxI32 d)		{ return storeDword(NxU32(d));	}
};

#endif
