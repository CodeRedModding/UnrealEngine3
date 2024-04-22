/*=============================================================================	TessellationRendering.cpp: Functions to support Tessellation Rendering.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "TessellationRendering.h"

#if WITH_D3D11_TESSELLATION

#if WITH_EDITOR
#include "nvtess.h"
#endif // #if WITH_EDITOR

/** Returns TRUE if the Material and Vertex Factory combination require adjacency information. */
UBOOL FTessellationMaterialPolicy::RequiresAdjacencyInformation( UMaterialInterface* Material, const FVertexFactoryType* VertexFactoryType )
{
	EMaterialTessellationMode TessellationMode = MTM_NoTessellation;
	UBOOL bEnableCrackFreeDisplacement = FALSE;
	if ( GRHIShaderPlatform == SP_PCD3D_SM5 && VertexFactoryType->SupportsTessellationShaders() && Material )
	{
		if ( IsInGameThread() )
		{
			UMaterial* BaseMaterial = Material->GetMaterial();
			check( BaseMaterial );
			TessellationMode = (EMaterialTessellationMode)BaseMaterial->D3D11TessellationMode;
			bEnableCrackFreeDisplacement = BaseMaterial->bEnableCrackFreeDisplacement;
		}
		else
		{
			check( IsInRenderingThread() );
			FMaterialRenderProxy* MaterialRenderProxy = Material->GetRenderProxy( FALSE, FALSE );
			check( MaterialRenderProxy );
			const FMaterial* MaterialResource = MaterialRenderProxy->GetMaterial();
			check( MaterialResource );
			TessellationMode = MaterialResource->GetD3D11TessellationMode();
			bEnableCrackFreeDisplacement = MaterialResource->IsCrackFreeDisplacementEnabled();
		}
	}

	return TessellationMode == MTM_PNTriangles || ( TessellationMode == MTM_FlatTessellation && bEnableCrackFreeDisplacement );
}

void FBaseHullShader::SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View)
{
	const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
	MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View);

	if(DisplacementNonUniformScaleParameter.IsBound())
	{
		// Extract per axis scales from LocalToWorld transform
		FVector4 WorldX = FVector4(BatchElement.LocalToWorld.M[0][0],BatchElement.LocalToWorld.M[0][1],BatchElement.LocalToWorld.M[0][2],0);
		FVector4 WorldY = FVector4(BatchElement.LocalToWorld.M[1][0],BatchElement.LocalToWorld.M[1][1],BatchElement.LocalToWorld.M[1][2],0);
		FVector4 WorldZ = FVector4(BatchElement.LocalToWorld.M[2][0],BatchElement.LocalToWorld.M[2][1],BatchElement.LocalToWorld.M[2][2],0);
		FLOAT ScaleX = FVector(WorldX).Size();
		FLOAT ScaleY = FVector(WorldY).Size();
		FLOAT ScaleZ = FVector(WorldZ).Size();
		FVector4 LocalScales = FVector4(ScaleX,ScaleX,ScaleX,0);

		// Now transform local scaling into world space
		FVector4 WorldScales = BatchElement.LocalToWorld.GetMatrixWithoutScale().TransformFVector4(LocalScales); WorldScales.W = 0;

		SetHullShaderValue(GetHullShader(),DisplacementNonUniformScaleParameter,WorldScales);
	}
}

#if WITH_EDITOR
/**
 * Provides static mesh render data to the NVIDIA tessellation library.
 */
class FStaticMeshNvRenderBuffer : public nv::RenderBuffer
{
public:

	/** Construct from static mesh render buffers. */
	FStaticMeshNvRenderBuffer(
		const FPositionVertexBuffer& InPositionVertexBuffer,
		const FStaticMeshVertexBuffer& InVertexBuffer,
		const TResourceArray<WORD, INDEXBUFFER_ALIGNMENT>& Indices )
		: PositionVertexBuffer( InPositionVertexBuffer )
		, VertexBuffer( InVertexBuffer )
	{
		check( PositionVertexBuffer.GetNumVertices() == VertexBuffer.GetNumVertices() );
		mIb = new nv::IndexBuffer( (void*)Indices.GetTypedData(), nv::IBT_U16, Indices.Num(), false );
	}

	/** Retrieve the position and first texture coordinate of the specified index. */
	virtual nv::Vertex getVertex( unsigned int Index ) const
	{
		nv::Vertex Vertex;

		check( Index < PositionVertexBuffer.GetNumVertices() );

		const FVector& Position = PositionVertexBuffer.VertexPosition( Index );
		Vertex.pos.x = Position.X;
		Vertex.pos.y = Position.Y;
		Vertex.pos.z = Position.Z;

		if( VertexBuffer.GetNumTexCoords() )
		{
			const FVector2D UV = VertexBuffer.GetVertexUV( Index, 0 );
			Vertex.uv.x = UV.X;
			Vertex.uv.y = UV.Y;
		}
		else
		{
			Vertex.uv.x = 0.0f;
			Vertex.uv.y = 0.0f;
		}

		return Vertex;
	}

private:

	/** The position vertex buffer for the static mesh. */
	const FPositionVertexBuffer& PositionVertexBuffer;

	/** The vertex buffer for the static mesh. */
	const FStaticMeshVertexBuffer& VertexBuffer;

	/** Copying is forbidden. */
	FStaticMeshNvRenderBuffer( const FStaticMeshNvRenderBuffer& ); 
	FStaticMeshNvRenderBuffer& operator=( const FStaticMeshNvRenderBuffer& );
};

/**
 * Provides skeletal mesh render data to the NVIDIA tessellation library.
 */
class FSkeletalMeshNvRenderBuffer : public nv::RenderBuffer
{
public:

	/** Construct from static mesh render buffers. */
	FSkeletalMeshNvRenderBuffer( 
		const TArray<FSoftSkinVertex>& InVertexBuffer,
		const UINT InTexCoordCount,
		const TArray<DWORD>& Indices )
		: VertexBuffer( InVertexBuffer )
		, TexCoordCount( InTexCoordCount )
	{
		mIb = new nv::IndexBuffer( (void*)Indices.GetTypedData(), nv::IBT_U32, Indices.Num(), false );
	}

	/** Retrieve the position and first texture coordinate of the specified index. */
	virtual nv::Vertex getVertex( unsigned int Index ) const
	{
		nv::Vertex Vertex;

		check( Index < (unsigned int)VertexBuffer.Num() );

		const FSoftSkinVertex& SrcVertex = VertexBuffer( Index );

		Vertex.pos.x = SrcVertex.Position.X;
		Vertex.pos.y = SrcVertex.Position.Y;
		Vertex.pos.z = SrcVertex.Position.Z;

		if( TexCoordCount > 0 )
		{
			Vertex.uv.x = SrcVertex.UVs[0].X;
			Vertex.uv.y = SrcVertex.UVs[0].Y;
		}
		else
		{
			Vertex.uv.x = 0.0f;
			Vertex.uv.y = 0.0f;
		}

		return Vertex;
	}

private:
	/** The vertex buffer for the skeletal mesh. */
	const TArray<FSoftSkinVertex>& VertexBuffer;
	const UINT TexCoordCount;

	/** Copying is forbidden. */
	FSkeletalMeshNvRenderBuffer(const FSkeletalMeshNvRenderBuffer&); 
	FSkeletalMeshNvRenderBuffer& operator=(const FSkeletalMeshNvRenderBuffer&);
};

void BuildStaticAdjacencyIndexBuffer( const FPositionVertexBuffer& PositionVertexBuffer, const FStaticMeshVertexBuffer& VertexBuffer, const TResourceArray<WORD, INDEXBUFFER_ALIGNMENT>& Indices, TResourceArray<WORD, INDEXBUFFER_ALIGNMENT>& OutPnAenIndices )
{
	if ( Indices.Num() )
	{
		FStaticMeshNvRenderBuffer StaticMeshRenderBuffer( PositionVertexBuffer, VertexBuffer, Indices );
		nv::IndexBuffer* PnAENIndexBuffer = nv::tess::buildTessellationBuffer( &StaticMeshRenderBuffer, nv::DBM_PnAenDominantCorner, true );
		check( PnAENIndexBuffer );
		const INT IndexCount = (INT)PnAENIndexBuffer->getLength();
		OutPnAenIndices.Empty( IndexCount );
		OutPnAenIndices.Add( IndexCount );
		for ( INT Index = 0; Index < IndexCount; ++Index )
		{
			OutPnAenIndices( Index ) = (*PnAENIndexBuffer)[Index];
		}
		delete PnAENIndexBuffer;
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}

void BuildSkeletalAdjacencyIndexBuffer( const TArray<FSoftSkinVertex>& VertexBuffer, const UINT TexCoordCount, const TArray<DWORD>& Indices, TArray<DWORD>& OutPnAenIndices )
{
	if ( Indices.Num() )
	{
		FSkeletalMeshNvRenderBuffer SkeletalMeshRenderBuffer( VertexBuffer, TexCoordCount, Indices );
		nv::IndexBuffer* PnAENIndexBuffer = nv::tess::buildTessellationBuffer( &SkeletalMeshRenderBuffer, nv::DBM_PnAenDominantCorner, true );
		check( PnAENIndexBuffer );
		const INT IndexCount = (INT)PnAENIndexBuffer->getLength();
		OutPnAenIndices.Empty( IndexCount );
		OutPnAenIndices.Add( IndexCount );
		for ( INT Index = 0; Index < IndexCount; ++Index )
		{
			OutPnAenIndices( Index ) = (*PnAENIndexBuffer)[Index];
		}
		delete PnAENIndexBuffer;
	}
	else
	{
		OutPnAenIndices.Empty();
	}
}
#endif // #if WITH_EDITOR

#endif // #if WITH_D3D11_TESSELLATION
