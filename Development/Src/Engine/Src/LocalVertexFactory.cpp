/*=============================================================================
	LocalVertexFactory.cpp: Local vertex factory implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "LocalVertexFactoryShaderParms.h"
#include "LocalDecalVertexFactory.h"

/**
 * Should we cache the material's shadertype on this platform with this vertex factory? 
 */
UBOOL FLocalVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	//don't compile for terrain materials, since they are never rendered in preview windows
	return !Material->IsTerrainMaterial(); 
}

/**
* Copy the data from another vertex factory
* @param Other - factory to copy from
*/
void FLocalVertexFactory::Copy(const FLocalVertexFactory& Other)
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		FLocalVertexFactoryCopyData,
		FLocalVertexFactory*,VertexFactory,this,
		const DataType*,DataCopy,&Other.Data,
	{
		VertexFactory->Data = *DataCopy;
	});
	BeginUpdateResourceRHI(this);
}

/**
* Vertex factory interface for creating a corresponding decal vertex factory
* Copies the data from this existing vertex factory.
*
* @return new allocated decal vertex factory
*/
FDecalVertexFactoryBase* FLocalVertexFactory::CreateDecalVertexFactory() const
{
	FLocalDecalVertexFactory* DecalFactory = new FLocalDecalVertexFactory();
	DecalFactory->Copy(*this);
	return DecalFactory;
}

void FLocalVertexFactory::InitRHI()
{
	// If the vertex buffer containing position is not the same vertex buffer containing the rest of the data,
	// then initialize PositionStream and PositionDeclaration.
	if(Data.PositionComponent.VertexBuffer != Data.TangentBasisComponents[0].VertexBuffer)
	{
		FVertexDeclarationElementList PositionOnlyStreamElements;
		PositionOnlyStreamElements.AddItem(AccessPositionStreamComponent(Data.PositionComponent,VEU_Position));
		InitPositionDeclaration(PositionOnlyStreamElements);
	}

	FVertexDeclarationElementList Elements;
	if(Data.PositionComponent.VertexBuffer != NULL)
	{
		Elements.AddItem(AccessStreamComponent(Data.PositionComponent,VEU_Position));
	}

	// only tangent,normal are used by the stream. the binormal is derived in the shader
	EVertexElementUsage TangentBasisUsages[2] = { VEU_Tangent, VEU_Normal };
	for(INT AxisIndex = 0;AxisIndex < 2;AxisIndex++)
	{
		if(Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.AddItem(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex],TangentBasisUsages[AxisIndex]));
		}
	}

	if(Data.ColorComponent.VertexBuffer)
	{
		Elements.AddItem(AccessStreamComponent(Data.ColorComponent,VEU_Color,1));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.AddItem(AccessStreamComponent(NullColorComponent,VEU_Color,1));
	}

	if(Data.TextureCoordinates.Num())
	{
		for(UINT CoordinateIndex = 0;CoordinateIndex < Data.TextureCoordinates.Num();CoordinateIndex++)
		{
			Elements.AddItem(AccessStreamComponent(
				Data.TextureCoordinates(CoordinateIndex),
				VEU_TextureCoordinate,
				CoordinateIndex
				));
		}

		for(UINT CoordinateIndex = Data.TextureCoordinates.Num();CoordinateIndex < MAX_TEXCOORDS;CoordinateIndex++)
		{
			Elements.AddItem(AccessStreamComponent(
				Data.TextureCoordinates(Data.TextureCoordinates.Num() - 1),
				VEU_TextureCoordinate,
				CoordinateIndex
				));
		}
	}

	if(Data.ShadowMapCoordinateComponent.VertexBuffer)
	{
		Elements.AddItem(AccessStreamComponent(Data.ShadowMapCoordinateComponent,VEU_Color));
	}
	else if(Data.TextureCoordinates.Num())
	{
		Elements.AddItem(AccessStreamComponent(Data.TextureCoordinates(0),VEU_Color));
	}

	InitDeclaration(Elements,Data);
}

FVertexFactoryShaderParameters* FLocalVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FLocalVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FLocalVertexFactory,"LocalVertexFactory",TRUE,TRUE,TRUE,TRUE,TRUE, VER_VERTEX_FACTORY_LOCALTOWORLD_FLIP,0);
