/*=============================================================================
	UnNovodexSupport.h: Novodex support
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Per Vognsen 3/04
=============================================================================*/

#undef check

#define NOMINMAX

#include "NxFoundation.h"
#include "NxPhysics.h"

//JOEGTODO : Please fix me!
#if DO_CHECK
	#define check(expr)  {if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ );}
#else
#if _MSC_VER
	#define check __noop
#else
	#define check(expr) {}
#endif
#endif

extern NxPhysicsSDK*	GNovodexSDK;


// Unreal to Novodex conversion
NxMat34 U2NMatrixCopy(const FMatrix& uTM);
NxMat34 U2NTransform(const FMatrix& uTM);

NxVec3 U2NVectorCopy(const FVector& uVec);
NxVec3 U2NPosition(const FVector& uVec);

// Novodex to Unreal conversion
FMatrix N2UTransform(const NxMat34& nTM);

FVector N2UVectorCopy(const NxVec3& nVec);
FVector N2UPosition(const NxVec3& nVec);