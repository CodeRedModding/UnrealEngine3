/*=============================================================================
	ParticleBeamTrailVertexFactory.h: Shared Particle Beam and Trail vertex 
										factory definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 *	
 */
class FParticleBeamTrailVertexFactory : public FParticleVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleBeamTrailVertexFactory);

public:
	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	// FRenderResource interface.
	virtual void InitRHI();

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
};

/**
 *	
 */
class FParticleBeamTrailDynamicParameterVertexFactory : public FParticleBeamTrailVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleBeamTrailDynamicParameterVertexFactory);

public:
	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	// FRenderResource interface.
	virtual void InitRHI();

private:
};

/**
 *	
 */
class FParticleBeamTrailVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		// Dump all the parameters...
		CameraWorldPositionParameter.Bind(ParameterMap,TEXT("CameraWorldPosition"),TRUE);
		CameraRightParameter.Bind(ParameterMap,TEXT("CameraRight"),TRUE);
		CameraUpParameter.Bind(ParameterMap,TEXT("CameraUp"),TRUE);
		ScreenAlignmentParameter.Bind(ParameterMap,TEXT("ScreenAlignment"),TRUE);
		LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorld"));
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << CameraWorldPositionParameter;
		Ar << CameraRightParameter;
		Ar << CameraUpParameter;
		Ar << ScreenAlignmentParameter;
		Ar << LocalToWorldParameter;
		
		// set parameter names for platforms that need them
		LocalToWorldParameter.SetShaderParamName(TEXT("LocalToWorld"));
	}

	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const;

	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const;

private:
	FShaderParameter CameraWorldPositionParameter;
	FShaderParameter CameraRightParameter;
	FShaderParameter CameraUpParameter;
	FShaderParameter ScreenAlignmentParameter;
	FShaderParameter LocalToWorldParameter;
};
