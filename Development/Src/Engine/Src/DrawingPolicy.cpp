/*=============================================================================
	DrawingPolicy.cpp: Base drawing policy implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"


FMeshDrawingPolicy::FMeshDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource,
	UBOOL bInOverrideWithShaderComplexity,
	UBOOL bInTwoSidedOverride,
	FLOAT InDepthBias,
	UBOOL bInTwoSidedSeparatePassOverride
	):
	VertexFactory(InVertexFactory),
	MaterialRenderProxy(InMaterialRenderProxy),
	MaterialResource(&InMaterialResource),
	bIsTwoSidedMaterial(InMaterialResource.IsTwoSided() || bInTwoSidedOverride),
	bIsWireframeMaterial(InMaterialResource.IsWireframe()),
	bNeedsBackfacePass(
		(InMaterialResource.IsTwoSided() || bInTwoSidedOverride) &&
		(InMaterialResource.GetLightingModel() != MLM_NonDirectional) && 
		(InMaterialResource.GetLightingModel() != MLM_Unlit) &&
		(InMaterialResource.RenderTwoSidedSeparatePass() && !bInTwoSidedSeparatePassOverride)
		),
	//convert from signed UBOOL to unsigned BITFIELD
	bOverrideWithShaderComplexity(bInOverrideWithShaderComplexity != FALSE),
	DepthBias(InDepthBias)
{
}

void FMeshDrawingPolicy::SetMeshRenderState(
	const FSceneView& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	static FName NAME_LandscapeDecl = FName(TEXT("FLandscapeDecalVertexFactory"));

	EmitMeshDrawEvents(PrimitiveSceneInfo, Mesh);

	const FRasterizerStateInitializerRHI Initializer = {
		(Mesh.bWireframe || IsWireframe()) ? FM_Wireframe : FM_Solid,
		(IsTwoSided() && !NeedsBackfacePass() || Mesh.bDisableBackfaceCulling) ? CM_None :
			(XOR(XOR(View.bReverseCulling,bBackFace), Mesh.ReverseCulling) ? CM_CCW : CM_CW),
		DepthBias + Mesh.DepthBias,
		Mesh.SlopeScaleDepthBias,
		TRUE
	};
	RHISetRasterizerStateImmediate( Initializer);

	if( Mesh.bIsDecal && 
		Mesh.DecalState && 
		// no need to use a scissor rect for triangles already clipped to the decal frustum
		!Mesh.DecalState->bUseSoftwareClip &&
		!Mesh.bWireframe &&
		Mesh.VertexFactory->GetType() &&
		Mesh.VertexFactory->GetType()->GetFName() != NAME_LandscapeDecl
	  )
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
		// Calculate the screen-space scissor rect that corresponds to the decal and set it.
		FVector2D MinCorner;
		FVector2D MaxCorner;
		if( Mesh.DecalState->QuadToClippedScreenSpaceAABB( &View, MinCorner, MaxCorner, BatchElement.LocalToWorld ) )
		{
			RHISetScissorRect( TRUE, appTrunc(MinCorner.X), appTrunc(MinCorner.Y), appTrunc(MaxCorner.X), appTrunc(MaxCorner.Y) );
		}
		else
		{
			// Decal was projected outside screen or clip planes
			RHISetScissorRect( TRUE, 0, 0, 0, 0 );
		}
	}
}

void FMeshDrawingPolicy::DrawMesh(const FMeshBatch& Mesh, INT BatchElementIndex) const
{
	INC_DWORD_STAT(STAT_MeshDrawCalls);
	const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
	if (Mesh.UseDynamicData)
	{
		if (Mesh.ParticleType == PET_None)
		{
			check(Mesh.DynamicVertexData);

			if (BatchElement.DynamicIndexData)
			{
				RHIDrawIndexedPrimitiveUP(
					Mesh.Type,
					BatchElement.MinVertexIndex,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.NumPrimitives,
					BatchElement.DynamicIndexData,
					BatchElement.DynamicIndexStride,
					Mesh.DynamicVertexData,
					Mesh.DynamicVertexStride
					);
			}
			else
			{
				RHIDrawPrimitiveUP(
					Mesh.Type,
					BatchElement.NumPrimitives,
					Mesh.DynamicVertexData,
					Mesh.DynamicVertexStride
					);
			}
		}
		else if (Mesh.ParticleType == PET_PresuppliedMemory)
		{
			check(Mesh.DynamicVertexData);

			if (BatchElement.DynamicIndexData)
			{
				// for presupplied memory, we don't need any copying, just pass it right to
				// the GPU
#if NGP
				RHIDrawIndexedPrimitiveUP_StaticGPUMemory(
#else
				RHIDrawIndexedPrimitiveUP(
#endif
					Mesh.Type,
					BatchElement.MinVertexIndex,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.NumPrimitives,
					BatchElement.DynamicIndexData,
					BatchElement.DynamicIndexStride,
					Mesh.DynamicVertexData,
					Mesh.DynamicVertexStride
					);
			}
		}
		else if (Mesh.ParticleType == PET_Sprite)
		{
			RHIDrawSpriteParticles(Mesh);
		}
		else if (Mesh.ParticleType == PET_PointSprite)
		{
			RHIDrawPointSpriteParticles(Mesh);
		}
		else if (Mesh.ParticleType == PET_SubUV)
		{
			RHIDrawSubUVParticles(Mesh);
		}
	}
	else
	{
		if(BatchElement.IndexBuffer)
		{
			check(BatchElement.IndexBuffer->IsInitialized());
			// If the RHI supports it, and the mesh requests it, draw it with pre-vertex-shader culling.
			if(Mesh.bUsePreVertexShaderCulling)
			{
				RHIDrawIndexedPrimitive_PreVertexShaderCulling(
					BatchElement.IndexBuffer->IndexBufferRHI,
					Mesh.Type,
					0,
					BatchElement.MinVertexIndex,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives,
					BatchElement.LocalToWorld,
					Mesh.PlatformMeshData
					);
			}
			else
			{
				RHIDrawIndexedPrimitive(
					BatchElement.IndexBuffer->IndexBufferRHI,
					Mesh.Type,
					0,
					BatchElement.MinVertexIndex,
					BatchElement.MaxVertexIndex - BatchElement.MinVertexIndex + 1,
					BatchElement.FirstIndex,
					BatchElement.NumPrimitives
					);
			}
		}
		else
		{
			RHIDrawPrimitive(
				Mesh.Type,
				BatchElement.FirstIndex,
				BatchElement.NumPrimitives
				);
		}
	}

	if ( Mesh.bIsDecal && Mesh.DecalState && !Mesh.DecalState->bUseSoftwareClip )
	{
		// Restore the scissor rect.
		RHISetScissorRect( FALSE, 0, 0, 0, 0 );
	}
}

void FMeshDrawingPolicy::DrawShared(const FSceneView* View) const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->Set();
}

/**
* Get the decl and stream strides for this mesh policy type and vertexfactory
* @param VertexDeclaration - output decl 
* @param StreamStrides - output array of vertex stream strides 
*/
void FMeshDrawingPolicy::GetVertexDeclarationInfo(FVertexDeclarationRHIRef& VertexDeclaration, DWORD *StreamStrides) const
{
	check(VertexFactory && VertexFactory->IsInitialized());
	VertexFactory->GetStreamStrides(StreamStrides);
	VertexDeclaration = VertexFactory->GetDeclaration();
	check(IsValidRef(VertexDeclaration));
}
