/*=============================================================================
	UnStaticMeshCollision.cpp: Static mesh collision code.
	Copyright 2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnCollision.h"

//
//	UStaticMesh::LineCheck
//

UBOOL UStaticMeshComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	if(!StaticMesh)
		return Super::LineCheck(Result,End,Start,Extent,TraceFlags);

	UBOOL	Hit = 0,
			ZeroExtent = Extent == FVector(0,0,0);

	BEGINCYCLECOUNTER(ZeroExtent ? GCollisionStats.StaticMeshZeroExtentTime : GCollisionStats.StaticMeshExtentTime);

	// JTODO: Make line check test against non-convex prims in BodySetup

	if(StaticMesh->CollisionModel && ((StaticMesh->UseSimpleBoxCollision && !ZeroExtent) || (StaticMesh->UseSimpleLineCollision && ZeroExtent)) && !(TraceFlags & TRACE_ShadowCast))
		Hit = !StaticMesh->CollisionModel->LineCheck(Result,Owner,End,Start,Extent,TraceFlags);
	else if(StaticMesh->kDOPTree.Nodes.Num())
	{
		Result.Time = 1.0f;

		if(ZeroExtent)
		{
			FkDOPLineCollisionCheck kDOPCheck(&Result,this,Start,End);
			Hit = StaticMesh->kDOPTree.LineCheck(kDOPCheck);
			if (Hit == 1)
			{
				// Transform the hit normal to world space if there was a hit
				// This is deferred until now because multiple triangles might get
				// hit in the search and I want to delay the expensive transformation
				// as late as possible
				Result.Normal = kDOPCheck.GetHitNormal();
			}
		}
		else
		{
			FkDOPBoxCollisionCheck kDOPCheck(&Result,this,Start,End,Extent);
			Hit = StaticMesh->kDOPTree.BoxCheck(kDOPCheck);
			if( Hit == 1 )
			{
				// Transform the hit normal to world space if there was a hit
				// This is deferred until now because multiple triangles might get
				// hit in the search and I want to delay the expensive transformation
				// as late as possible
				Result.Normal = kDOPCheck.GetHitNormal();
			}
		}

		if(Hit)
		{
			Result.Actor = Owner;
			Result.Component = this;
			Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),4.0f / (End - Start).Size()),0.0f,1.0f);
			Result.Location = Start + (End - Start) * Result.Time;
		}
	}

	return !Hit;

	ENDCYCLECOUNTER;

}

//
//	UStaticMeshComponent::PointCheck
//

UBOOL UStaticMeshComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent)
{
	if(!StaticMesh)
		return Super::PointCheck(Result,Location,Extent);

	BEGINCYCLECOUNTER(GCollisionStats.StaticMeshPointTime);

	UBOOL	Hit = 0;

	if(StaticMesh->CollisionModel && StaticMesh->UseSimpleBoxCollision)
		Hit = !StaticMesh->CollisionModel->PointCheck(Result,Owner,Location,Extent);
	else if(StaticMesh->kDOPTree.Nodes.Num())
	{ 
		FkDOPPointCollisionCheck kDOPCheck(&Result,this,Location,Extent);
		Hit = StaticMesh->kDOPTree.PointCheck(kDOPCheck);
		// Transform the hit normal to world space if there was a hit
		// This is deferred until now because multiple triangles might get
		// hit in the search and I want to delay the expensive transformation
		// as late as possible. Same thing holds true for the hit location
		if (Hit == 1)
		{
			Result.Normal = kDOPCheck.GetHitNormal();
			Result.Location = kDOPCheck.GetHitLocation();
		}

		if(Hit)
		{
			Result.Normal.Normalize();
			// Now calculate the location of the hit in world space
			Result.Actor = Owner;
			Result.Component = this;
		}
	}

	return !Hit;

	ENDCYCLECOUNTER;

}
