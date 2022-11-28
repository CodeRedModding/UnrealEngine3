/*=============================================================================
	UnNovodexSupport.cpp: Novodex support
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Per Vognsen 3/04
=============================================================================*/

#include "EnginePrivate.h"

#ifdef WITH_NOVODEX

#include "UnNovodexSupport.h"

///////////////////// Unreal to Novodex conversion /////////////////////

NxMat34 U2NMatrixCopy(const FMatrix& uTM)
{
	NxMat34 Result;

	// Copy rotation
	NxF32 Entries[9];
	Entries[0] = uTM.M[0][0];
	Entries[1] = uTM.M[0][1];
	Entries[2] = uTM.M[0][2];

	Entries[3] = uTM.M[1][0];
	Entries[4] = uTM.M[1][1];
	Entries[5] = uTM.M[1][2];

	Entries[6] = uTM.M[2][0];
	Entries[7] = uTM.M[2][1];
	Entries[8] = uTM.M[2][2];

	Result.M.setColumnMajor(Entries);

	// Copy translation
	Result.t.x = uTM.M[3][0];
	Result.t.y = uTM.M[3][1];
	Result.t.z = uTM.M[3][2];

	return Result;
}

NxMat34 U2NTransform(const FMatrix& uTM)
{
	NxMat34 Result;

	// Copy rotation
	NxF32 Entries[9];
	Entries[0] = uTM.M[0][0];
	Entries[1] = uTM.M[0][1];
	Entries[2] = uTM.M[0][2];

	Entries[3] = uTM.M[1][0];
	Entries[4] = uTM.M[1][1];
	Entries[5] = uTM.M[1][2];

	Entries[6] = uTM.M[2][0];
	Entries[7] = uTM.M[2][1];
	Entries[8] = uTM.M[2][2];

	Result.M.setColumnMajor(Entries);

	// Copy translation
	Result.t.x = uTM.M[3][0] * U2PScale;
	Result.t.y = uTM.M[3][1] * U2PScale;
	Result.t.z = uTM.M[3][2] * U2PScale;

	return Result;
}


NxVec3 U2NVectorCopy(const FVector& uVec)
{
	return NxVec3(uVec.X, uVec.Y, uVec.Z);
}

NxVec3 U2NPosition(const FVector& uPos)
{
	return NxVec3(uPos.X * U2PScale, uPos.Y * U2PScale, uPos.Z * U2PScale);
}

///////////////////// Novodex to Unreal conversion /////////////////////


FMatrix N2UTransform(const NxMat34& nTM)
{
	FMatrix Result;

	// Copy rotation
	NxF32 Entries[9];
	nTM.M.getColumnMajor(Entries);

	Result.M[0][0] = Entries[0];
	Result.M[0][1] = Entries[1];
	Result.M[0][2] = Entries[2];

	Result.M[1][0] = Entries[3];
	Result.M[1][1] = Entries[4];
	Result.M[1][2] = Entries[5];

	Result.M[2][0] = Entries[6];
	Result.M[2][1] = Entries[7];
	Result.M[2][2] = Entries[8];

	// Copy translation
	Result.M[3][0] = nTM.t.x * P2UScale;
	Result.M[3][1] = nTM.t.y * P2UScale;
	Result.M[3][2] = nTM.t.z * P2UScale;

	// Fix fourth column
	Result.M[0][3] = Result.M[1][3] = Result.M[2][3] = 0.0f;
	Result.M[3][3] = 1.0f;

	return Result;
}


FVector N2UVectorCopy(const NxVec3& nVec)
{
	return FVector(nVec.x, nVec.y, nVec.z);
}

FVector N2UPosition(const NxVec3& nVec)
{
	return FVector(nVec.x * P2UScale, nVec.y * P2UScale, nVec.z * P2UScale);
}

#endif // WITH_NOVODEX