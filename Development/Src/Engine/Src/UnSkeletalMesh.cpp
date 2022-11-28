/*=============================================================================
	UnSkeletalMesh.cpp: Unreal skeletal mesh and animation implementation.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Erik de Neve
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(USkeletalMesh);

/*-----------------------------------------------------------------------------
	USkeletalMesh, USkeletalMeshComponent implementation.
-----------------------------------------------------------------------------*/

void USkeletalMesh::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	Ar << Bounds;

	if( Ar.Ver() < 180 )
	{
		URB_BodySetup* BodySetup;
		Ar << BodySetup;
	}

	Ar << Materials;
	Ar << Origin << RotOrigin;
	Ar << RefSkeleton;			// Reference skeleton.
	Ar << SkeletalDepth;		// How many bones beep the heirarchy goes.		
	Ar << LODModels;
	
	// Serialize - only for garbage collection.
	if( !Ar.IsPersistent() )
	{		
		Ar << RefBasesInvMatrix;
	}	
}


//
// Postload
//
void USkeletalMesh::PostLoad()
{
	Super::PostLoad();
	
	//@todo skeletal mesh lod: code currently only supports single LOD level.
	check( LODModels.Num() );
	if( LODModels.Num() > 1 )
	{
		LODModels.Remove( 1, LODModels.Num() - 1 );
	}

	CalculateInvRefMatrices();
}

//
// Match up startbone by name. Also allows the attachment tag aliases.
// Pretty much the same as USkeletalMeshComponent::MatchRefBone
//
INT USkeletalMesh::MatchRefBone( FName StartBoneName)
{
	INT Bone = -1;
	if ( StartBoneName != NAME_None)
	{
		// Search though regular bone names.
		for( INT p=0; p< RefSkeleton.Num(); p++)
		{	
			if( RefSkeleton(p).Name == StartBoneName )
			{
				Bone = p;
				break;
			}
		}	
	}
	return Bone;
}

FMatrix USkeletalMesh::GetRefPoseMatrix( INT BoneIndex )
{
	check( BoneIndex >= 0 && BoneIndex < RefSkeleton.Num() );
	FMatrix Result = FMatrix( RefSkeleton(BoneIndex).BonePos.Position, RefSkeleton(BoneIndex).BonePos.Orientation );
	return Result;
}


/*-----------------------------------------------------------------------------
	The end.
-----------------------------------------------------------------------------*/

