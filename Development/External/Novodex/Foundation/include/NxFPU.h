#ifndef NX_FOUNDATION_NXFPU
#define NX_FOUNDATION_NXFPU
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nx.h"

#ifndef WIN32
#include "math.h"
#endif

	#define	NX_SIGN_BITMASK		0x80000000

	//Integer representation of a floating-point value.
	#define NX_IR(x)			((NxU32&)(x))

	//Floating-point representation of a integer value.
	#define NX_FR(x)			((NxF32&)(x))

	//Absolute integer representation of a floating-point value
	#define NX_AIR(x)			(NX_IR(x)&0x7fffffff)

	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPUPrecision24();
	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPUPrecision53();
	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPUPrecision64();

	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPURoundingChop();
	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPURoundingUp();
	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPURoundingDown();
	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPURoundingNear();

	NX_C_EXPORT NXF_DLL_EXPORT void NX_CALL_CONV NxSetFPUExceptions(bool);

	NX_C_EXPORT NXF_DLL_EXPORT int NX_CALL_CONV NxIntChop(const NxF32& f);
	NX_C_EXPORT NXF_DLL_EXPORT int NX_CALL_CONV NxIntFloor(const NxF32& f);
	NX_C_EXPORT NXF_DLL_EXPORT int NX_CALL_CONV NxIntCeil(const NxF32& f);

	// Calling fsincos instead of fsin+fcos
	NX_INLINE void NxSinCos(NxF32& c, NxF32& s, NxF32 f)
	{
#ifdef WIN32
		NxF32 localCos, localSin;
		NxF32 local = f;
		_asm	fld		local
		_asm	fsincos
		_asm	fstp	localCos
		_asm	fstp	localSin
		c = localCos;
		s = localSin;
#else
		c = cosf(f);
		s = sinf(f);
#endif
	}

#endif
