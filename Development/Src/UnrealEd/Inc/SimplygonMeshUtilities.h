/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SIMPLYGONMESHUTILITIES_H__
#define __SIMPLYGONMESHUTILITIES_H__

#if WITH_SIMPLYGON

/** Enable to allow the creation of mesh proxies with Simplygon. */
#ifndef ENABLE_SIMPLYGON_MESH_PROXIES
#define ENABLE_SIMPLYGON_MESH_PROXIES (1)
#endif

namespace SimplygonMeshUtilities
{
	/**
	 * Optimizes a static mesh.
	 * @param SrcStaticMesh - The input mesh.  Can be the same as the output mesh.
	 * @param DestStaticMesh - Output mesh.  Can be the same as the input mesh.
	 * @param Settings - Settings that control the optimization process.
	 * @return TRUE if successful.
	 */
	UBOOL OptimizeStaticMesh(
		UStaticMesh* SrcStaticMesh,
		UStaticMesh* DestStaticMesh,
		INT DestLOD,
		const FStaticMeshOptimizationSettings& Settings );

	/**
	 * Optimizes a skeletal mesh.
	 * @param SkeletalMesh - The skeletal mesh to optimize.
	 * @param LODIndex - The LOD to optimize.
	 * @param Settings - Settings that control the optimization process.
	 * @return TRUE if successful.
	 */
	UBOOL OptimizeSkeletalMesh(
		USkeletalMesh* SkeletalMesh,
		INT LODIndex,
		const FSkeletalMeshOptimizationSettings& Settings );

#if ENABLE_SIMPLYGON_MESH_PROXIES
	/**
	 * What type of material to generate for the mesh proxy.
	 */
	enum EMeshProxyMaterialType
	{
		MPMT_DiffuseOnly,
		MPMT_DiffuseAndNormal,
		MPMT_MAX
	};

	/**
	 * How to handle vertex colors.
	 */
	enum EMeshProxyVertexColorMode
	{
		/** Modulate diffuse by the mesh's vertex color. */
		MPVCM_ModulateDiffuse,
		/** Ignore the mesh's vertex color. */
		MPVCM_Ignore
	};

	/**
	 * Creates a mesh proxy for the provided mesh instances.
	 * @param MeshComponentsToMerge - The mesh instances for which the proxy will be created.
	 * @param MaterialType - What type of material to generate for the mesh proxy.
	 * @param OnScreenSizeInPixels - The on-screen size in pixels at which the mesh will be viewed.
	 * @param DesiredTextureSize - The size of the texture to use for the proxy mesh.
	 * @param VertexColorMode - How to deal with the meshes vertex colors.
	 * @param MeshProxyPackage - The package in to which the mesh proxy will be created.
	 * @param OutMeshProxy - The mesh proxy that has been created.
	 * @param OutProxyLocation - The location at which to place the proxy.
	 * @return TRUE if successful.
	 */
	UBOOL CreateMeshProxy(
		const TArray<UStaticMeshComponent*> MeshComponentsToMerge,
		EMeshProxyMaterialType MaterialType,
		INT OnScreenSizeInPixels,
		INT DesiredTextureSize,
		EMeshProxyVertexColorMode VertexColorMode,
		UPackage* MeshProxyPackage,
		UStaticMesh** OutMeshProxy,
		FVector* OutProxyLocation );
#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES
};

#endif // WITH_SIMPLYGON

#endif // __SIMPLYGONMESHUTILITIES_H__
