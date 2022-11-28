/*=============================================================================
	GeomFitUtils.h: Utilities for fitting collision models to static meshes.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

    Revision history:
		* Created by James Golding
=============================================================================*/

// k-DOP (k-Discrete Oriented Polytopes) Direction Vectors
#define RCP_SQRT2 (0.70710678118654752440084436210485f)
#define RCP_SQRT3 (0.57735026918962576450914878050196f)


const FVector KDopDir6[6] = 
{
	FVector( 1.f, 0.f, 0.f),
	FVector(-1.f, 0.f, 0.f),
	FVector( 0.f, 1.f, 0.f),
	FVector( 0.f,-1.f, 0.f),
	FVector( 0.f, 0.f, 1.f),
	FVector( 0.f, 0.f,-1.f)
};

const FVector KDopDir10X[10] = 
{
	FVector( 1.f, 0.f, 0.f),
	FVector(-1.f, 0.f, 0.f),
	FVector( 0.f, 1.f, 0.f),
	FVector( 0.f,-1.f, 0.f),
	FVector( 0.f, 0.f, 1.f),
	FVector( 0.f, 0.f,-1.f),
	FVector( 0.f, RCP_SQRT2,  RCP_SQRT2),
	FVector( 0.f,-RCP_SQRT2, -RCP_SQRT2),
	FVector( 0.f, RCP_SQRT2, -RCP_SQRT2),
	FVector( 0.f,-RCP_SQRT2,  RCP_SQRT2)
};

const FVector KDopDir10Y[10] = 
{
	FVector( 1.f, 0.f, 0.f),
	FVector(-1.f, 0.f, 0.f),
	FVector( 0.f, 1.f, 0.f),
	FVector( 0.f,-1.f, 0.f),
	FVector( 0.f, 0.f, 1.f),
	FVector( 0.f, 0.f,-1.f),
	FVector( RCP_SQRT2, 0.f,  RCP_SQRT2),
	FVector(-RCP_SQRT2, 0.f, -RCP_SQRT2),
	FVector( RCP_SQRT2, 0.f, -RCP_SQRT2),
	FVector(-RCP_SQRT2, 0.f,  RCP_SQRT2)
};

const FVector KDopDir10Z[10] = 
{
	FVector( 1.f, 0.f, 0.f),
	FVector(-1.f, 0.f, 0.f),
	FVector( 0.f, 1.f, 0.f),
	FVector( 0.f,-1.f, 0.f),
	FVector( 0.f, 0.f, 1.f),
	FVector( 0.f, 0.f,-1.f),
	FVector( RCP_SQRT2,  RCP_SQRT2, 0.f),
	FVector(-RCP_SQRT2, -RCP_SQRT2, 0.f),
	FVector( RCP_SQRT2, -RCP_SQRT2, 0.f),
	FVector(-RCP_SQRT2,  RCP_SQRT2, 0.f)
};

const FVector KDopDir18[18] = 
{
	FVector( 1.f, 0.f, 0.f),
	FVector(-1.f, 0.f, 0.f),
	FVector( 0.f, 1.f, 0.f),
	FVector( 0.f,-1.f, 0.f),
	FVector( 0.f, 0.f, 1.f),
	FVector( 0.f, 0.f,-1.f),
	FVector( 0.f, RCP_SQRT2,  RCP_SQRT2),
	FVector( 0.f,-RCP_SQRT2, -RCP_SQRT2),
	FVector( 0.f, RCP_SQRT2, -RCP_SQRT2),
	FVector( 0.f,-RCP_SQRT2,  RCP_SQRT2),
	FVector( RCP_SQRT2, 0.f,  RCP_SQRT2),
	FVector(-RCP_SQRT2, 0.f, -RCP_SQRT2),
	FVector( RCP_SQRT2, 0.f, -RCP_SQRT2),
	FVector(-RCP_SQRT2, 0.f,  RCP_SQRT2),
	FVector( RCP_SQRT2,  RCP_SQRT2, 0.f),
	FVector(-RCP_SQRT2, -RCP_SQRT2, 0.f),
	FVector( RCP_SQRT2, -RCP_SQRT2, 0.f),
	FVector(-RCP_SQRT2,  RCP_SQRT2, 0.f)
};

const FVector KDopDir26[26] = 
{
	FVector( 1.f, 0.f, 0.f),
	FVector(-1.f, 0.f, 0.f),
	FVector( 0.f, 1.f, 0.f),
	FVector( 0.f,-1.f, 0.f),
	FVector( 0.f, 0.f, 1.f),
	FVector( 0.f, 0.f,-1.f),
	FVector( 0.f, RCP_SQRT2,  RCP_SQRT2),
	FVector( 0.f,-RCP_SQRT2, -RCP_SQRT2),
	FVector( 0.f, RCP_SQRT2, -RCP_SQRT2),
	FVector( 0.f,-RCP_SQRT2,  RCP_SQRT2),
	FVector( RCP_SQRT2, 0.f,  RCP_SQRT2),
	FVector(-RCP_SQRT2, 0.f, -RCP_SQRT2),
	FVector( RCP_SQRT2, 0.f, -RCP_SQRT2),
	FVector(-RCP_SQRT2, 0.f,  RCP_SQRT2),
	FVector( RCP_SQRT2,  RCP_SQRT2, 0.f),
	FVector(-RCP_SQRT2, -RCP_SQRT2, 0.f),
	FVector( RCP_SQRT2, -RCP_SQRT2, 0.f),
	FVector(-RCP_SQRT2,  RCP_SQRT2, 0.f),
	FVector( RCP_SQRT3,  RCP_SQRT3,  RCP_SQRT3),
	FVector( RCP_SQRT3,  RCP_SQRT3, -RCP_SQRT3),
	FVector( RCP_SQRT3, -RCP_SQRT3,  RCP_SQRT3),
	FVector( RCP_SQRT3, -RCP_SQRT3, -RCP_SQRT3),
	FVector(-RCP_SQRT3,  RCP_SQRT3,  RCP_SQRT3),
	FVector(-RCP_SQRT3,  RCP_SQRT3, -RCP_SQRT3),
	FVector(-RCP_SQRT3, -RCP_SQRT3,  RCP_SQRT3),
	FVector(-RCP_SQRT3, -RCP_SQRT3, -RCP_SQRT3),
};

// Utilities
void GenerateOBBAsCollisionModel(UStaticMesh* StaticMesh);
void GenerateKDopAsCollisionModel(UStaticMesh* StaticMesh,TArray<FVector> &dirs);
void GenerateSphereAsKarmaCollision(UStaticMesh* StaticMesh);
void GenerateCylinderAsKarmaCollision(UStaticMesh* StaticMesh,INT dir); // X = 0, Y = 1, Z = 2

