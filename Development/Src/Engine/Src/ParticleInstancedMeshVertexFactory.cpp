/*=============================================================================
	ParticleInstancedMeshVertexFactory.cpp: Instanced Mesh Particles Vertex Factory.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ParticleInstancedMeshVertexFactory.h"

void FParticleInstancedMeshVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	InvNumVerticesPerInstanceParameter.Bind(ParameterMap,TEXT("InvNumVerticesPerInstance"),TRUE);
	NumVerticesPerInstanceParameter.Bind(ParameterMap,TEXT("NumVerticesPerInstance"),TRUE);
	PreViewTranslationParameter.Bind(ParameterMap,TEXT("InstancedPreViewTranslation"),TRUE);
}

void FParticleInstancedMeshVertexFactoryShaderParameters::Serialize(FArchive& Ar)
{
	Ar << InvNumVerticesPerInstanceParameter;
	Ar << NumVerticesPerInstanceParameter;
	if( Ar.Ver() >= VER_ADDED_PRE_VIEW_TRANSLATION_PARAMETER )
	{
		Ar << PreViewTranslationParameter;
	}
	else if( Ar.IsLoading() )
	{
		PreViewTranslationParameter = FShaderParameter();
	}
}

void FParticleInstancedMeshVertexFactoryShaderParameters::Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
{
	SetVertexShaderValue(VertexShader->GetVertexShader(),InvNumVerticesPerInstanceParameter,1.0f / (FLOAT)VertexFactory->GetNumVerticesPerInstance());
	SetVertexShaderValue(VertexShader->GetVertexShader(),NumVerticesPerInstanceParameter,(FLOAT)VertexFactory->GetNumVerticesPerInstance());
	SetVertexShaderValue(VertexShader->GetVertexShader(),PreViewTranslationParameter,View.PreViewTranslation);
}

void FParticleInstancedMeshVertexFactoryShaderParameters::SetMesh(FShader* VertexShader,const FMeshBatch& Mesh,INT BatchElementIndex,const FSceneView& View) const
{
}

UBOOL FParticleInstancedMeshVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Material->IsUsedWithInstancedMeshParticles() || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals();
}

void FParticleInstancedMeshVertexFactory::InitRHI()
{
	FVertexDeclarationElementList Elements;

	Elements.AddItem(AccessStreamComponent(Data.PositionComponent,VEU_Position));

	EVertexElementUsage TangentBasisUsages[2] = { VEU_Tangent, VEU_Normal };
	for(INT AxisIndex = 0;AxisIndex < 2;AxisIndex++)
	{
		if(Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL)
		{
			Elements.AddItem(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex],TangentBasisUsages[AxisIndex]));
		}
	}

	if(Data.TextureCoordinateComponent.VertexBuffer)
	{
		Elements.AddItem(AccessStreamComponent(Data.TextureCoordinateComponent,VEU_TextureCoordinate));
	}

	if(Data.ColorComponent.VertexBuffer)
	{
		Elements.AddItem(AccessStreamComponent(Data.ColorComponent,VEU_Color, 1));
	}
	else
	{
		//If the mesh has no color component, set the null color buffer on a new stream with a stride of 0.
		//This wastes 4 bytes of bandwidth per vertex, but prevents having to compile out twice the number of vertex factories.
		FVertexStreamComponent NullColorComponent(&GNullColorVertexBuffer, 0, 0, VET_Color);
		Elements.AddItem(AccessStreamComponent(NullColorComponent,VEU_Color,1));
	}

	if(Data.TextureCoordinateComponent.VertexBuffer)
	{
		Elements.AddItem(AccessStreamComponent(Data.TextureCoordinateComponent,VEU_Color));
	}

	Elements.AddItem(AccessStreamComponent(Data.InstanceOffsetComponent,VEU_TextureCoordinate,1));
	Elements.AddItem(AccessStreamComponent(Data.InstanceAxisComponents[0],VEU_TextureCoordinate,2));
	Elements.AddItem(AccessStreamComponent(Data.InstanceAxisComponents[1],VEU_TextureCoordinate,3));
	Elements.AddItem(AccessStreamComponent(Data.InstanceAxisComponents[2],VEU_TextureCoordinate,4));

	InitDeclaration(Elements,Data);
}

FVertexFactoryShaderParameters* FParticleInstancedMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FParticleInstancedMeshVertexFactoryShaderParameters() : NULL;
}

/**
* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
*/
void FParticleInstancedMeshVertexFactory::ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
{
	FVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
	OutEnvironment.Definitions.Set(TEXT("COLOR_OVER_LIFE"),TEXT("1"));
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FParticleInstancedMeshVertexFactory,"MeshInstancedVertexFactory",TRUE,FALSE,TRUE,FALSE,TRUE,VER_SM2_BLENDING_SHADER_FIXES, 0);

