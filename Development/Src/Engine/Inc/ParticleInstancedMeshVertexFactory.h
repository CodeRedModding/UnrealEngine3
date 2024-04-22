/*=============================================================================
	ParticleInstancedMeshVertexFactory.h: Instanced Mesh Particles Vertex Factory.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_PARTICLEINSTANCEDMESHVERTEXFACTORY_H
#define _INC_PARTICLEINSTANCEDMESHVERTEXFACTORY_H

/** The shader parameters for the instanced mesh particle vertex factory. */
class FParticleInstancedMeshVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:

	// FVertexFactoryShaderParameters interface.
	virtual void Bind(const FShaderParameterMap& ParameterMap);
	virtual void Serialize(FArchive& Ar);
	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const;
	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const;

private:

	FShaderParameter InvNumVerticesPerInstanceParameter;
	FShaderParameter NumVerticesPerInstanceParameter;
	FShaderParameter PreViewTranslationParameter;
};

/**
 * A vertex factory used to render instanced particle meshes.
 */
class FParticleInstancedMeshVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleInstancedMeshVertexFactory);
public:

	struct DataType : public FVertexFactory::DataType
	{
		/** The stream to read the vertex position from. */
		FVertexStreamComponent PositionComponent;

		/** The streams to read the tangent basis from. */
		FVertexStreamComponent TangentBasisComponents[2];

		/** The streams to read the texture coordinates from. */
		FVertexStreamComponent TextureCoordinateComponent;

		/** The stream to read the instance offset from. */
		FVertexStreamComponent InstanceOffsetComponent;

		/** The stream to read the instance axes from. */
		FVertexStreamComponent InstanceAxisComponents[3];

		/** The stream to read the vertex color from. */
		FVertexStreamComponent ColorComponent;
	};

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	* Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	*/
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/**
	 * An implementation of the interface used by TSynchronizedResource to update the resource with new data from the game thread.
	 */
	void SetData(const DataType& InData)
	{
		Data = InData;
		UpdateRHI();
	}

	// FRenderResource interface.
	virtual void InitRHI();

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
	DataType Data;
};

#endif
