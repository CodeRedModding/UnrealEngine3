/*=============================================================================
	UnSoftBodySupport.cpp: SoftBody support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY

#include "UnNovodexSupport.h"
#include "UnSoftBodySupport.h"

void ScaleSoftBodyTetras(const TArray<FVector>& InTetraVerts, TArray<FVector>& OutTetraVerts, FLOAT InScale)
{
	OutTetraVerts.Empty();
	OutTetraVerts.AddZeroed(InTetraVerts.Num());

	for(INT i=0; i<InTetraVerts.Num(); i++)
	{
		OutTetraVerts(i) = InTetraVerts(i) * InScale;
	}
}

#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY

NxSoftBodyMesh* USkeletalMesh::GetSoftBodyMeshForScale(FLOAT InScale)
{
#if WITH_PHYSX_COOKING
	check(CachedSoftBodyMeshes.Num() == CachedSoftBodyMeshScales.Num());

	// Look to see if we already have this mesh at this scale.
	for(INT i=0; i<CachedSoftBodyMeshes.Num(); i++)
	{
		if( Abs(InScale - CachedSoftBodyMeshScales(i)) < KINDA_SMALL_NUMBER )
		{
			return (NxSoftBodyMesh*)CachedSoftBodyMeshes(i);
		}
	}

	if(SoftBodySurfaceToGraphicsVertMap.Num() == 0)
	{
		debugf(TEXT("Cannot instantiate soft-body mesh, no soft-body vertices present."));
		return NULL;
	}

	TArray<FVector> TetraVerts;
	ScaleSoftBodyTetras(SoftBodyTetraVertsUnscaled, TetraVerts, InScale);

	check((SoftBodyTetraIndices.Num() % 4) == 0);

	NxSoftBodyMeshDesc SoftBodyMeshDesc;

	SoftBodyMeshDesc.numVertices = TetraVerts.Num();
	SoftBodyMeshDesc.numTetrahedra = SoftBodyTetraIndices.Num() / 4;
	SoftBodyMeshDesc.vertexStrideBytes = sizeof(NxVec3);
	SoftBodyMeshDesc.tetrahedronStrideBytes = sizeof(NxU32) * 4;
	SoftBodyMeshDesc.vertices = TetraVerts.GetData(); 
	SoftBodyMeshDesc.tetrahedra = SoftBodyTetraIndices.GetData();
	SoftBodyMeshDesc.flags = 0;
	SoftBodyMeshDesc.vertexMassStrideBytes = 0;
	SoftBodyMeshDesc.vertexFlagStrideBytes = 0;
	SoftBodyMeshDesc.vertexMasses = NULL;
	SoftBodyMeshDesc.vertexFlags = NULL;

	check(SoftBodyMeshDesc.isValid());

	TArray<BYTE> TempData;
	FNxMemoryBuffer Buffer(&TempData);
	bool bSuccess = GNovodexCooking->NxCookSoftBodyMesh(SoftBodyMeshDesc, Buffer);
	check(bSuccess);

	NxSoftBodyMesh* NewSoftBodyMesh = GNovodexSDK->createSoftBodyMesh(Buffer);
	check(NewSoftBodyMesh);

	CachedSoftBodyMeshes.AddItem( NewSoftBodyMesh );
	CachedSoftBodyMeshScales.AddItem( InScale );

	return NewSoftBodyMesh;
#else
	debugf(TEXT("Cannot instantiate soft-body mesh.  PhysX Cooking has been disabled."));
	return NULL;
#endif
}

#endif //WITH_NOVODEX && !NX_DISABLE_SOFTBODY

/** Reset the store of cooked SoftBody meshes. Need to make sure you are not actually using any when you call this. */
void USkeletalMesh::ClearSoftBodyMeshCache()
{
#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY
	for (INT i = 0; i < CachedSoftBodyMeshes.Num(); i++)
	{
		NxSoftBodyMesh* SM = (NxSoftBodyMesh*)CachedSoftBodyMeshes(i);
		check(SM);
		GNovodexPendingKillSoftBodyMesh.AddItem(SM);
	}
	CachedSoftBodyMeshes.Empty();
	CachedSoftBodyMeshScales.Empty();

#endif // WITH_NOVODEX && !NX_DISABLE_SOFTBODY
}

/** 
 * Replaces the given data with current data to recreate the soft body representation for the mesh
 *
 * @param	MeshInfo	A newly-generated soft-body mesh. This data is used to overwrite the existing mesh data.
 */
void USkeletalMesh::RecreateSoftBody(FSoftBodyMeshInfo& MeshInfo)
{
	// If the given mesh info is not valid, then this likely cause crashes 
	// later when simulating the soft body. 
	check(MeshInfo.IsValid());

	for(INT SpecialBoneIndex = 0; SpecialBoneIndex < SoftBodySpecialBones.Num(); SpecialBoneIndex++)
	{
		// Clear out any existing data
		SoftBodySpecialBones(SpecialBoneIndex).AttachedVertexIndices.Empty();

		TArray<INT>& CurrentIndexArray = MeshInfo.SpecialBoneAttachedVertexIndicies(SpecialBoneIndex);
		const INT NumOfAttachedVerticies = MeshInfo.SpecialBoneAttachedVertexIndicies(SpecialBoneIndex).Num();

		for(INT AttachedIndex = 0; AttachedIndex < NumOfAttachedVerticies; AttachedIndex++)
		{
			SoftBodySpecialBones(SpecialBoneIndex).AttachedVertexIndices.AddItem(CurrentIndexArray(AttachedIndex));
		}
	}

	// Clear out any existing data so we don't have old data when we start adding our new data.
	SoftBodySurfaceToGraphicsVertMap.Empty();
	SoftBodySurfaceIndices.Empty();
	SoftBodyTetraVertsUnscaled.Empty();
	SoftBodyTetraIndices.Empty();
	SoftBodyTetraLinks.Empty();

	// Now, we can copy over the new mesh data
	SoftBodySurfaceIndices.Append(MeshInfo.SurfaceIndices);
	SoftBodySurfaceToGraphicsVertMap.Append(MeshInfo.SurfaceToGraphicsVertMap);
	SoftBodyTetraIndices.Append(MeshInfo.TetraIndices);
	SoftBodyTetraLinks.Append(MeshInfo.TetraLinks);
	SoftBodyTetraVertsUnscaled.Append(MeshInfo.TetraVertsUnscaled);
}


#endif // WITH_NOVODEX
