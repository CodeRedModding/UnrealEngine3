/*=============================================================================
	LocalVertexFactoryShaderParms.cpp: Local vertex factory shader parameters.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LOCALVERTEXFACTORYSHADERPARMS_H__
#define __LOCALVERTEXFACTORYSHADERPARMS_H__

class FLocalVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	/**
	 * Bind shader constants by name for vertex shader params
	 * @param	ParameterMap - mapping of named shader constants to indices
	 */
	virtual void Bind(const FShaderParameterMap& ParameterMap);

	/**
	 * Serialize shader params to an archive
	 * @param	Ar - archive to serialize to
	 */
	virtual void Serialize(FArchive& Ar);

	/**
	 * Set any shader data specific to this vertex factory
	 */
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const;
	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const;

protected:
	FShaderParameter LocalToWorldParameter;
	FShaderParameter LocalToWorldRotDeterminantFlipParameter;
	FShaderParameter WorldToLocalParameter;
};

#endif // __LOCALVERTEXFACTORYSHADERPARMS_H__
