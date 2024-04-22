/**
 *	LensFlareVertexFactory.h: Lens flare vertex factory definitions.
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 *	
 */
class FLensFlareVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FLensFlareVertexFactory);

public:
	FLensFlareVertexFactory()
	{
	}

	virtual ~FLensFlareVertexFactory()
	{
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);
	
	// FRenderResource interface.
	virtual void InitRHI();
	virtual void ReleaseRHI();

	virtual FString GetFriendlyName() const
	{
		return FString( TEXT("LensFlare Vertex Factory") );
	}

	FLOAT GetOcclusionValue() const							{	return OcclusionValue;				}
	void SetOcclusionValue(const FLOAT InOcclusionValue)	{	OcclusionValue = InOcclusionValue;	}

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	FLOAT OcclusionValue;

private:
};

/**
 *	
 */
class FLensFlareVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		CameraRightParameter.Bind(ParameterMap,TEXT("CameraRight"),TRUE);
		CameraUpParameter.Bind(ParameterMap,TEXT("CameraUp"),TRUE);
		LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorld"));
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << CameraRightParameter;
		Ar << CameraUpParameter;
		Ar << LocalToWorldParameter;

		CameraRightParameter.SetShaderParamName(TEXT("CameraRight"));
		CameraUpParameter.SetShaderParamName(TEXT("CameraUp"));
		LocalToWorldParameter.SetShaderParamName(TEXT("LocalToWorld"));
	}

	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const;

	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const;

private:
	FShaderParameter CameraRightParameter;
	FShaderParameter CameraUpParameter;
	FShaderParameter LocalToWorldParameter;
};
