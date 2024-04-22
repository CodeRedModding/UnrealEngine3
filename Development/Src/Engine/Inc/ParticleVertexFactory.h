/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 *	
 */
class FParticleVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleVertexFactory);

public:

	FParticleVertexFactory() :
		EmitterNormalsMode(0),
		bFactoryInUse(0),
		bUsesPointSprites(FALSE)
	{}

	// FRenderResource interface.
	virtual void InitRHI();

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	//
	FORCEINLINE void SetScreenAlignment(BYTE InScreenAlignment)
	{
		ScreenAlignment = InScreenAlignment;
	}

	FORCEINLINE void SetLockAxesFlag(BYTE InLockAxisFlag)
	{
		LockAxisFlag = InLockAxisFlag;
	}

	FORCEINLINE void SetLockAxes(FVector& InLockAxisUp, FVector& InLockAxisRight)
	{
		LockAxisUp		= InLockAxisUp;
		LockAxisRight	= InLockAxisRight;
	}

	FORCEINLINE void SetNormalsData(BYTE InEmitterNormalsMode, FVector InNormalsSphereCenter, FVector InNormalsCylinderDirection)
	{
		EmitterNormalsMode = InEmitterNormalsMode;
		NormalsSphereCenter = InNormalsSphereCenter;
		NormalsCylinderDirection = InNormalsCylinderDirection;
	}

	FORCEINLINE BYTE		GetScreenAlignment()				{	return ScreenAlignment;	}
	FORCEINLINE BYTE		GetLockAxisFlag()					{	return LockAxisFlag;	}
	FORCEINLINE FVector&	GetLockAxisUp()						{	return LockAxisUp;		}
	FORCEINLINE FVector&	GetLockAxisRight()					{	return LockAxisRight;	}

	/**For mobile, whether or not to use point sprites for this particle system*/
	FORCEINLINE	UBOOL		UsePointSprites(void) const			{	return bUsesPointSprites; } 

	/** Helper function that can be called externally to get screen alignment */
	static ESpriteScreenAlignment StaticGetSpriteScreenAlignment(BYTE LockAxisFlag, BYTE ScreenAlignment);

	/** Return the sprite screen alignment */
	virtual ESpriteScreenAlignment GetSpriteScreenAlignment() const;

	/** Set the vertex factory type */
	FORCEINLINE void SetVertexFactoryType(BYTE InVertexFactoryType) { VertexFactoryType = InVertexFactoryType; }
	/** Return the vertex factory type */
	FORCEINLINE BYTE GetVertexFactoryType() const { return VertexFactoryType; }

	/** Set for the "In Use" Array Index*/
	FORCEINLINE void SetInUse(UBOOL bInUse) { bFactoryInUse = bInUse; }
	/** Return the vertex factory type */
	FORCEINLINE UBOOL GetInUse() const { return bFactoryInUse; }

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

private:
	BYTE		ScreenAlignment;
	BYTE		LockAxisFlag;
	BYTE		EmitterNormalsMode;
	BYTE		VertexFactoryType;
	FVector		LockAxisUp;
	FVector		LockAxisRight;
	FVector		NormalsSphereCenter;
	FVector		NormalsCylinderDirection;

protected:
	BITFIELD	bFactoryInUse:1;
	BITFIELD	bUsesPointSprites:1;

	friend class FParticleVertexFactoryShaderParameters;
};

/**
 *	
 */
class FParticleDynamicParameterVertexFactory : public FParticleVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticleDynamicParameterVertexFactory);

public:
	// FRenderResource interface.
	virtual void InitRHI();

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);
};

class FParticlePointSpriteVertexFactory : public FParticleVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FParticlePointSpriteVertexFactory);

	FParticlePointSpriteVertexFactory() :
		  FParticleVertexFactory()
	{
		bUsesPointSprites = TRUE;
	}

public:
	// FRenderResource interface.
	virtual void InitRHI();

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);
};

/**
 *	
 */
class FParticleVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		CameraWorldPositionParameter.Bind(ParameterMap,TEXT("CameraWorldPosition"),TRUE);
		CameraRightParameter.Bind(ParameterMap,TEXT("CameraRight"),TRUE);
		CameraUpParameter.Bind(ParameterMap,TEXT("CameraUp"),TRUE);
		ScreenAlignmentParameter.Bind(ParameterMap,TEXT("ScreenAlignment"),TRUE);
		LocalToWorldParameter.Bind(ParameterMap,TEXT("LocalToWorld"));
		AxisRotationVectorSourceIndexParameter.Bind(ParameterMap, TEXT("AxisRotationVectorSourceIndex"));
		AxisRotationVectorsArrayParameter.Bind(ParameterMap, TEXT("AxisRotationVectors"));
		ParticleUpRightResultScalarsParameter.Bind(ParameterMap, TEXT("ParticleUpRightResultScalars"));
		NormalsTypeParameter.Bind(ParameterMap, TEXT("NormalsType"),TRUE);
		NormalsSphereCenterParameter.Bind(ParameterMap, TEXT("NormalsSphereCenter"),TRUE);
		NormalsCylinderUnitDirectionParameter.Bind(ParameterMap, TEXT("NormalsCylinderUnitDirection"),TRUE);
		CornerUVsParameter.Bind(ParameterMap, TEXT("CornerUVs"), (GRHIShaderPlatform != SP_PS3 && GRHIShaderPlatform != SP_PCOGL) ? TRUE : FALSE);
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << CameraWorldPositionParameter;
		Ar << CameraRightParameter;
		Ar << CameraUpParameter;
		Ar << ScreenAlignmentParameter;
		Ar << LocalToWorldParameter;
		Ar << AxisRotationVectorSourceIndexParameter;
		Ar << AxisRotationVectorsArrayParameter;
		Ar << ParticleUpRightResultScalarsParameter;
		Ar << NormalsTypeParameter;
		Ar << NormalsSphereCenterParameter;
		Ar << NormalsCylinderUnitDirectionParameter;
		Ar << CornerUVsParameter;
		
		// set parameter names for platforms that need them
		CameraWorldPositionParameter.SetShaderParamName(TEXT("CameraWorldPosition"));
		CameraRightParameter.SetShaderParamName(TEXT("CameraRight"));
		CameraUpParameter.SetShaderParamName(TEXT("CameraUp"));
		ScreenAlignmentParameter.SetShaderParamName(TEXT("ScreenAlignment"));
		LocalToWorldParameter.SetShaderParamName(TEXT("LocalToWorld"));
		AxisRotationVectorSourceIndexParameter.SetShaderParamName(TEXT("AxisRotationVectorSourceIndex"));
		AxisRotationVectorsArrayParameter.SetShaderParamName(TEXT("AxisRotationVectors"));
		ParticleUpRightResultScalarsParameter.SetShaderParamName(TEXT("ParticleUpRightResultScalars"));
		CornerUVsParameter.SetShaderParamName(TEXT("CornerUVs"));
	}

	virtual void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const;

	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const;

private:
	FShaderParameter CameraWorldPositionParameter;
	FShaderParameter CameraRightParameter;
	FShaderParameter CameraUpParameter;
	FShaderParameter ScreenAlignmentParameter;
	FShaderParameter LocalToWorldParameter;
	FShaderParameter AxisRotationVectorSourceIndexParameter;
	FShaderParameter AxisRotationVectorsArrayParameter;
	FShaderParameter ParticleUpRightResultScalarsParameter;
	FShaderParameter NormalsTypeParameter;
	FShaderParameter NormalsSphereCenterParameter;
	FShaderParameter NormalsCylinderUnitDirectionParameter;
	FShaderParameter CornerUVsParameter;
};
