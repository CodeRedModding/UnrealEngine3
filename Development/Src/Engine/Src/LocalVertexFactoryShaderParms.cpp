/*=============================================================================
	LocalVertexFactoryShaderParms.cpp: Local vertex factory shader parameters
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "LocalVertexFactoryShaderParms.h"

void FLocalVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorld"),TRUE); // optional as instanced meshes don't need it
	LocalToWorldRotDeterminantFlipParameter.Bind(ParameterMap,TEXT("LocalToWorldRotDeterminantFlip"),TRUE);
	WorldToLocalParameter.Bind(ParameterMap,TEXT("WorldToLocal"),TRUE);
}

void FLocalVertexFactoryShaderParameters::Serialize(FArchive& Ar)
{
	Ar << LocalToWorldParameter;
	Ar << LocalToWorldRotDeterminantFlipParameter;
	Ar << WorldToLocalParameter;
	
	// set parameter names for platforms that need them
	LocalToWorldParameter.SetShaderParamName(TEXT("LocalToWorld"));
}

void FLocalVertexFactoryShaderParameters::Set(FShader* Shader,const FVertexFactory* VertexFactory,const FSceneView& View) const
{	
}

void FLocalVertexFactoryShaderParameters::SetMesh(FShader* Shader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
	SetVertexShaderValue(
		Shader->GetVertexShader(),
		LocalToWorldParameter,
		BatchElement.LocalToWorld.ConcatTranslation(View.PreViewTranslation)
		);

	if (LocalToWorldRotDeterminantFlipParameter.IsBound())
	{
		// Used to flip the normal direction if LocalToWorldRotDeterminant is negative.  
		// This prevents non-uniform negative scaling from making vectors transformed with CalcTangentToWorld pointing in the wrong quadrant.
		const FLOAT LocalToWorldRotDeterminant = BatchElement.LocalToWorld.RotDeterminant();
		SetVertexShaderValue(
			Shader->GetVertexShader(),
			LocalToWorldRotDeterminantFlipParameter,
			appFloatSelect(LocalToWorldRotDeterminant, 1, -1)
			);
	}

	// We don't bother removing the view translation from the world-to-local transform because the shader doesn't use it to transform points.
	SetVertexShaderValue(Shader->GetVertexShader(),WorldToLocalParameter,BatchElement.WorldToLocal);
}
