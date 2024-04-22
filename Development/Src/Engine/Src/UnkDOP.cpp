/*=============================================================================
	UnkDOP.cpp: k-DOP collision
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

/** Collision stats */
DECLARE_STATS_GROUP(TEXT("Collision"),STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Terrain Line Check"),STAT_TerrainZeroExtentTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Terrain Extent Check"),STAT_TerrainExtentTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("Terrain Point Check"),STAT_TerrainPointTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("SM Line Check"),STAT_StaticMeshZeroExtentTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("SM Extent Check"),STAT_StaticMeshExtentTime,STATGROUP_Collision);
DECLARE_CYCLE_STAT(TEXT("SM Point Check"),STAT_StaticMeshPointTime,STATGROUP_Collision);

// These are the plane normals for the kDOP that we use (bounding box)
FVector FkDOPPlanes::PlaneNormals[NUM_PLANES] =
{
	FVector(1.f,0.f,0.f),
	FVector(0.f,1.f,0.f),
	FVector(0.f,0.f,1.f)
};

/** these are self commenting, they have no semantics */
const VectorRegister KDopSIMD::VMaxMergeMask = MakeVectorRegister( (DWORD)0, (DWORD)-1, (DWORD)-1, (DWORD)0 );
const VectorRegister KDopSIMD::VMinMergeMask = MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)0, (DWORD)0 );
const VectorRegister KDopSIMD::VReplace4thMask = MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)0 );
const VectorRegister KDopSIMD::V_127p5 = { 127.5f,127.5f,127.5f,127.5f };
const VectorRegister KDopSIMD::V_p5 = { .5f,.5f,.5f,.5f };
const VectorRegister KDopSIMD::V_p5Neg = { -.5f,-.5f,-.5f,-.5f };
const VectorRegister KDopSIMD::V_127Inv = { 1.0f / 127.0f, 1.0f / 127.0f, 1.0f / 127.0f, 1.0f / 127.0f };
const VectorRegister KDopSIMD::VMinMergeOut = MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)0 );
const VectorRegister KDopSIMD::VFudgeFactor = MakeVectorRegister( FUDGE_SIZE, FUDGE_SIZE, FUDGE_SIZE, FUDGE_SIZE );
const VectorRegister KDopSIMD::VTwoFudgeFactor = MakeVectorRegister( 2.0f * FUDGE_SIZE, 2.0f * FUDGE_SIZE, 2.0f * FUDGE_SIZE, 2.0f * FUDGE_SIZE );
const VectorRegister KDopSIMD::VNegFudgeFactor = MakeVectorRegister( -FUDGE_SIZE, -FUDGE_SIZE, -FUDGE_SIZE, -FUDGE_SIZE );
const VectorRegister KDopSIMD::VOnePlusFudgeFactor = MakeVectorRegister( 1.0f + FUDGE_SIZE, 1.0f + FUDGE_SIZE, 1.0f + FUDGE_SIZE, 1.0f + FUDGE_SIZE );
const VectorRegister KDopSIMD::V_p5Neg_m_127Inv = { -.5f / 127.0f, -.5f / 127.0f, -.5f / 127.0f, -.5f / 127.0f };
const VectorRegister KDopSIMD::V_127InvNeg = { -1.0f / 127.0f, -1.0f / 127.0f, -1.0f / 127.0f, -1.0f / 127.0f };

const VectorRegister KDopSIMD::VAlignMasks[2] = 
{
	MakeVectorRegister( (DWORD)-1, (DWORD)-1, (DWORD)-1, (DWORD)-1 ),
	MakeVectorRegister( (DWORD)0, (DWORD)0, (DWORD)0, (DWORD)0 )
};


#if TEST_COMPACT_KDOP_SLOW
/** List of triangles we have verified so we can guarantee coverage */
TSet<INT> GVerifyTriangleArray;
#endif

