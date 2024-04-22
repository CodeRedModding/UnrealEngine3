/*=============================================================================
FogVolumeRendering.h: Definitions for rendering fog volumes.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
enum EFogVolumeDensityFunction
{
	FVDF_None,
	FVDF_Constant,
	FVDF_ConstantHeight,
	FVDF_LinearHalfspace,
	FVDF_Sphere,
	FVDF_Cone
};

/*-----------------------------------------------------------------------------
FFogVolumeShaderParameters - encapsulates the parameters needed to calculate fog from a fog volume in a vertex shader.
-----------------------------------------------------------------------------*/
class FFogVolumeShaderParameters
{
public:

	/** Binds the parameters */
	void Bind(const FShaderParameterMap& ParameterMap);

	/** Sets the parameters on the input VertexShader, using fog volume data from the input DensitySceneInfo. */
	void SetVertexShader(
		const FSceneView& View,
		const FMaterialRenderProxy* MaterialRenderProxy,
		FShader* VertexShader, 
		const class FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
		) const;

#if WITH_D3D11_TESSELLATION
	/** Sets the parameters on the input DomainShader, using fog volume data from the input DensitySceneInfo. */
	void SetDomainShader(
		const FSceneView& View,
		const FMaterialRenderProxy* MaterialRenderProxy,
		FShader* DomainShader, 
		const class FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
		) const;
#endif

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FFogVolumeShaderParameters& P);

private:

	template<typename ShaderRHIParamRef>
	void Set(
		const FSceneView& View,
		const FMaterialRenderProxy* MaterialRenderProxy,
		ShaderRHIParamRef Shader, 
		const class FFogVolumeDensitySceneInfo* FogVolumeSceneInfo
		) const;

	FShaderParameter FirstDensityFunctionParameter;
	FShaderParameter SecondDensityFunctionParameter;
	FShaderParameter StartDistanceParameter;
	FShaderParameter FogVolumeBoxMinParameter;
	FShaderParameter FogVolumeBoxMaxParameter;
	FShaderParameter ApproxFogColorParameter;
};

/**
* Base FogSceneInfo - derivatives store render thread density function data.
*/
class FFogVolumeDensitySceneInfo
{
public:

	/** The fog component the scene info is for. */
	const class UFogVolumeDensityComponent* Component;

	/** 
	* Controls whether the fog volume affects intersecting translucency.  
	* If FALSE, the fog volume will sort normally with translucency and not fog intersecting translucent objects.
	*/
	UBOOL bAffectsTranslucency;

	/** 
	 * Controls whether the fog volume affects opaque pixels, or just intersecting translucency.  
	 */
	UBOOL bOnlyAffectsTranslucency;

	/** 
	* A color used to approximate the material color when fogging intersecting translucency. 
	* This is necessary because we can't evaluate the material when applying fog to the translucent object.
	*/
	FLinearColor ApproxFogColor;

	/** The AABB of the associated fog volume primitive component. */
	FBox VolumeBounds;

	/** The Depth Priority Group of the associated primitive component. */
	UINT DPGIndex;

	/** Distance from the camera at which the fog should start, in world units. */
	FLOAT StartDistance;

	FLOAT MaxDistance;

	/** Name of the owner actor, used for debugging */
	FName OwnerName;

	/** Initialization ctor */
	FFogVolumeDensitySceneInfo(const class UFogVolumeDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual ~FFogVolumeDensitySceneInfo()
	{
	}

	/** Draw a mesh with this density function. */
	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId) = 0;

	/** Gets the number of pixel shader instructions to render the integral. */
	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const = 0;

	/** Get the density function parameters that will be passed to the integral pixel shader. */
	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& View) const = 0;

	/** Get the density function parameters that will be passed to the integral pixel shader. */
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& View) const
	{
		return FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	/** 
	* Returns an estimate of the maximum integral value that will be calculated by this density function. 
	* This will be used to normalize the integral on platforms that it has to be stored in fixed point.
	* Too large of estimates will increase aliasing, too small will cause the fog volume to be clamped.
	*/
	virtual FLOAT GetMaxIntegral() const = 0;

	virtual EFogVolumeDensityFunction GetDensityFunctionType() const = 0;
};

/**
* Constant density fog
*/
class FFogVolumeConstantDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The constant density factor */
	FLOAT Density;

	/** Default constructor for creating a fog volume scene info without the corresponding component */
	FFogVolumeConstantDensitySceneInfo() :
		FFogVolumeDensitySceneInfo(NULL, FBox(0), SDPG_World),
		Density(0.005f)
	{}

	/** Initialization constructor. */
	FFogVolumeConstantDensitySceneInfo(const class UFogVolumeConstantDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& View) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_Constant;
	}
};

/**
* Constant density fog, limited to a height
*/
class FFogVolumeConstantHeightDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The constant density factor */
	FLOAT Density;

	/** Maximum height */
	FLOAT Height;

	/** Initialization constructor. */
	FFogVolumeConstantHeightDensitySceneInfo(const class UFogVolumeConstantHeightDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_ConstantHeight;
	}
};

/** Halfspace fog defined by a plane, with density increasing linearly away from the plane. */
class FFogVolumeLinearHalfspaceDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** Linear density factor, scales the density contribution from the distance to the plane. */
	FLOAT PlaneDistanceFactor;

	/** Plane in worldspace that defines the fogged halfspace, whose normal points away from the fogged halfspace. */
	FPlane HalfspacePlane;

	/** Initialization constructor. */
	FFogVolumeLinearHalfspaceDensitySceneInfo(const class UFogVolumeLinearHalfspaceDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_LinearHalfspace;
	}
};


/** Spherical fog with density decreasing toward the edges of the sphere for soft edges. */
class FFogVolumeSphericalDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The density at the center of the sphere, which is the maximum density. */
	FLOAT MaxDensity;

	/** The sphere in worldspace */
	FSphere Sphere;

	/** Initialization constructor. */
	FFogVolumeSphericalDensitySceneInfo(const class UFogVolumeSphericalDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_Sphere;
	}
};



/**  */
class FFogVolumeConeDensitySceneInfo : public FFogVolumeDensitySceneInfo
{
public:

	/** The density along the axis, which is the maximum density. */
	FLOAT MaxDensity;

	/** World space position of the cone vertex. */
	FVector ConeVertex;

	/** Distance from the vertex at which the cone ends. */
	FLOAT ConeRadius;

	/** Axis defining the direction of the cone. */
	FVector ConeAxis;

	/** Angle from axis that defines the cone size. */
	FLOAT ConeMaxAngle;

	/** Initialization constructor. */
	FFogVolumeConeDensitySceneInfo(const class UFogVolumeConeDensityComponent* InComponent, const FBox &InVolumeBounds, UINT InDPGIndex);

	virtual UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId);

	virtual UINT GetNumIntegralShaderInstructions(const FMaterial* MaterialResource, const FVertexFactory* InVertexFactory) const;

	virtual FVector4 GetFirstDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FVector4 GetSecondDensityFunctionParameters(const FSceneView& Vie) const;
	virtual FLOAT GetMaxIntegral() const;
	virtual EFogVolumeDensityFunction GetDensityFunctionType() const
	{
		return FVDF_Cone;
	}
};

class FNoDensityPolicy
{	
public:

	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;
	
	/** Empty parameter component. */
	class ShaderParametersType
	{
	public:
		void Bind(const FShaderParameterMap& ParameterMap)
		{
		}
		void SetVertexShader(
			const FSceneView& View,
			const FMaterialRenderProxy* MaterialRenderProxy,
			FShader* VertexShader,
			ElementDataType ElementData
			) const
		{
		}

#if WITH_D3D11_TESSELLATION
		void SetDomainShader(
			const FSceneView& View,
			const FMaterialRenderProxy* MaterialRenderProxy,
			FShader* VertexShader,
			ElementDataType ElementData
			) const
		{
		}
#endif

		/** Serializer. */
		friend FArchive& operator<<(FArchive& Ar,ShaderParametersType& P)
		{
			return Ar;
		}
	};

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_None;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return TRUE;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_NONE"),TEXT("1"));
	}
};

class FConstantDensityPolicy
{	
public:

	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_Constant;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//don't compile the translucency vertex shader for GPU skinned vertex factories with this density function since it will run out of constant registers
		if (!Material->IsUsedWithFogVolumes() && appStrstr(VertexFactoryType->GetName(), TEXT("FGPUSkin")))
		{
			return FALSE;
		}
		return !Material->IsUsedWithDecals();
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_CONSTANT"),TEXT("1"));
	}
};

class FLinearHalfspaceDensityPolicy
{	
public:
	
	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_LinearHalfspace;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//don't compile the translucency vertex shader for GPU skinned vertex factories with this density function since it will run out of constant registers
		if (!Material->IsUsedWithFogVolumes() && appStrstr(VertexFactoryType->GetName(), TEXT("FGPUSkin")))
		{
			return FALSE;
		}
		return !Material->IsUsedWithDecals();
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_LINEARHALFSPACE"),TEXT("1"));
	}
};

class FSphereDensityPolicy
{	
public:
	
	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_Sphere;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//don't compile the translucency vertex shader for GPU skinned vertex factories with this density function since it will run out of constant registers
		if (!Material->IsUsedWithFogVolumes() && appStrstr(VertexFactoryType->GetName(), TEXT("FGPUSkin")))
		{
			return FALSE;
		}
		return !Material->IsUsedWithDecals();
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_SPHEREDENSITY"),TEXT("1"));
	}
};

class FConeDensityPolicy
{	
public:
	
	typedef FFogVolumeShaderParameters ShaderParametersType;
	typedef const FFogVolumeDensitySceneInfo* ElementDataType;

	static const EFogVolumeDensityFunction DensityFunctionType = FVDF_Cone;

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//not fully implemented
		return FALSE;
	}
	
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("FOGVOLUMEDENSITY_CONEDENSITY"),TEXT("1"));
	}
};

/**
* A vertex shader for rendering meshes during the integral accumulation passes.
*/
template<class DensityFunctionPolicy>
class TFogIntegralVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(TFogIntegralVertexShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes()
			&& DensityFunctionPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TFogIntegralVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	TFogIntegralVertexShader()
	{
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,BatchElementIndex,View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
};

/**
* A pixel shader for rendering meshes during the integral accumulation passes.
* Density function-specific parameters are packed into FirstDensityFunctionParameters and SecondDensityFunctionParameters
*/
template<class DensityFunctionPolicy>
class TFogIntegralPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(TFogIntegralPixelShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes()
			&& DensityFunctionPolicy::ShouldCache(Platform,Material,VertexFactoryType);
	}

	TFogIntegralPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		DepthFilterSampleOffsets.Bind(Initializer.ParameterMap,TEXT("DepthFilterSampleOffsets"),TRUE);
		ScreenToWorldParameter.Bind(Initializer.ParameterMap,TEXT("ScreenToWorld"),TRUE);
		CameraPosParameter.Bind(Initializer.ParameterMap,TEXT("FogCameraPosition"),TRUE);
		FaceParameter.Bind(Initializer.ParameterMap,TEXT("FaceScale"),TRUE);
		FirstDensityFunctionParameters.Bind(Initializer.ParameterMap,TEXT("FirstDensityFunctionParameters"),TRUE);
		SecondDensityFunctionParameters.Bind(Initializer.ParameterMap,TEXT("SecondDensityFunctionParameters"),TRUE);
		StartDistanceParameter.Bind(Initializer.ParameterMap,TEXT("StartDistance"),TRUE);
		MaxDistanceParameter.Bind(Initializer.ParameterMap,TEXT("MaxDistance"),TRUE);
		InvMaxIntegralParameter.Bind(Initializer.ParameterMap,TEXT("InvMaxIntegral"), TRUE);
	}

	TFogIntegralPixelShader()
	{
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo,
		UBOOL bBackFace)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);

		const FVector2D SceneDepthTexelSize = FVector2D( 1.0f / (FLOAT)GSceneRenderTargets.GetBufferSizeX(), 1.0f / (FLOAT)GSceneRenderTargets.GetBufferSizeY());

		const UINT NumDepthFilterSamples = 2;
		static const FVector4 SampleOffsets[NumDepthFilterSamples] =
		{
			//sample the texel to the left and above
			FVector4(-SceneDepthTexelSize.X, 0, 0, SceneDepthTexelSize.Y), 
			//sample the texel to the right and below
			FVector4(SceneDepthTexelSize.X, 0, 0, -SceneDepthTexelSize.Y)
		};

		SetPixelShaderValues(GetPixelShader(),DepthFilterSampleOffsets,SampleOffsets,NumDepthFilterSamples);

		//Transform from post projection space to world space, used to reconstruct world space positions to calculate the fog integral
		FMatrix ScreenToWorld = 
			FMatrix(
			FPlane(1,0,0,0),
			FPlane(0,1,0,0),
			FPlane(0,0,(1.0f - Z_PRECISION),1),
			FPlane(0,0,-View.NearClippingDistance * (1.0f - Z_PRECISION),0)
			) * View.InvTranslatedViewProjectionMatrix;

		SetPixelShaderValue( GetPixelShader(), ScreenToWorldParameter, ScreenToWorld );

		const FVector4 TranslatedViewOrigin = View.ViewOrigin + FVector4(View.PreViewTranslation,0);
		SetPixelShaderValue( GetPixelShader(), CameraPosParameter, TranslatedViewOrigin);

		//set the face parameter with 1 if a backface is being rendered, or -1 if a frontface is being rendered
		SetPixelShaderValue( GetPixelShader(), FaceParameter, bBackFace ? 1.0f : -1.0f);
		
		//set the parameters specific to the DensityFunctionPolicy
		SetPixelShaderValue( GetPixelShader(), FirstDensityFunctionParameters, DensitySceneInfo->GetFirstDensityFunctionParameters(View));
		SetPixelShaderValue( GetPixelShader(), SecondDensityFunctionParameters, DensitySceneInfo->GetSecondDensityFunctionParameters(View));
		SetPixelShaderValue( GetPixelShader(), StartDistanceParameter, DensitySceneInfo->StartDistance);
		SetPixelShaderValue( GetPixelShader(), MaxDistanceParameter, DensitySceneInfo->MaxDistance);
		SetPixelShaderValue( GetPixelShader(), InvMaxIntegralParameter, 1.0f / DensitySceneInfo->GetMaxIntegral());
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters;	
		Ar << DepthFilterSampleOffsets;
		Ar << ScreenToWorldParameter;
		Ar << CameraPosParameter;
		Ar << FaceParameter;
		Ar << FirstDensityFunctionParameters;
		Ar << SecondDensityFunctionParameters;
		Ar << StartDistanceParameter;
		Ar << MaxDistanceParameter;
		Ar << InvMaxIntegralParameter;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderParameter DepthFilterSampleOffsets;
	FShaderParameter ScreenToWorldParameter;
	FShaderParameter CameraPosParameter;
	FShaderParameter FaceParameter;
	FShaderParameter FirstDensityFunctionParameters;
	FShaderParameter SecondDensityFunctionParameters;
	FShaderParameter StartDistanceParameter;
	FShaderParameter MaxDistanceParameter;
	FShaderParameter InvMaxIntegralParameter;
};


/**
* Policy for accumulating fog line integrals
*/
template<class DensityFunctionPolicy>
class TFogIntegralDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	*/
	TFogIntegralDrawingPolicy(const FVertexFactory* InVertexFactory,const FMaterialRenderProxy* InMaterialRenderProxy,const FMaterial& InMaterialResource);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return TRUE if the draw policies are a match
	*/
	UBOOL Matches(const TFogIntegralDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void DrawShared(
		const FViewInfo* View,
		FBoundShaderStateRHIParamRef BoundShaderState, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo,
		UBOOL bBackFace) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

private:
	TFogIntegralVertexShader<DensityFunctionPolicy>* VertexShader;
	TFogIntegralPixelShader<DensityFunctionPolicy>* PixelShader;
};

/**
* Fog integral mesh drawing policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
template<class DensityFunctionPolicy>
class TFogIntegralDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };

	/**
	* Render a dynamic mesh using a fog integral mesh drawing policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo)
	{
		TFogIntegralDrawingPolicy<DensityFunctionPolicy> DrawingPolicy(
			Mesh.VertexFactory,
			Mesh.MaterialRenderProxy,
			*Mesh.MaterialRenderProxy->GetMaterial()
			);
		DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()), DensitySceneInfo, bBackFace);
		for( INT BatchElementIndex=0; BatchElementIndex < Mesh.Elements.Num(); BatchElementIndex++ )
		{
			DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,typename TFogIntegralDrawingPolicy<DensityFunctionPolicy>::ElementDataType());
			DrawingPolicy.DrawMesh(Mesh,BatchElementIndex);
		}

		return TRUE;
	}
};


/**
* A vertex shader for rendering meshes during the fog apply pass.
*/
class FFogVolumeApplyVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FFogVolumeApplyVertexShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes();
	}

	FFogVolumeApplyVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FMeshMaterialVertexShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
	}

	FFogVolumeApplyVertexShader()
	{
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		return bShaderHasOutdatedParameters;
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FSceneView& View)
	{
		VertexFactoryParameters.Set(this,VertexFactory,View);
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View)
	{
		VertexFactoryParameters.SetMesh(this, Mesh, BatchElementIndex, View);
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
};


/**
* A pixel shader for rendering meshes during the fog apply pass.
*/
class FFogVolumeApplyPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FFogVolumeApplyPixelShader,MeshMaterial);

public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		return Material->IsUsedWithFogVolumes();
	}

	FFogVolumeApplyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		:	FShader(Initializer)
	{
		MaxIntegralParameter.Bind(Initializer.ParameterMap,TEXT("MaxIntegral"), TRUE);
		MaterialParameters.Bind(Initializer.ParameterMap);
		AccumulatedFrontfacesLineIntegralTextureParam.Bind(Initializer.ParameterMap,TEXT("AccumulatedFrontfacesLineIntegralTexture"), TRUE);
		AccumulatedBackfacesLineIntegralTextureParam.Bind(Initializer.ParameterMap,TEXT("AccumulatedBackfacesLineIntegralTexture"), TRUE);
	}

	FFogVolumeApplyPixelShader()
	{
	}

	void SetParameters(
		const FVertexFactory* VertexFactory,
		const FMaterialRenderProxy* MaterialRenderProxy,
		const FSceneView& View,
		const FFogVolumeDensitySceneInfo* DensitySceneInfo)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set(this,MaterialRenderContext);

		//set the fog integral accumulation samplers so that the apply pass can lookup the result of the accumulation passes
		//linear filter since we're upsampling
		SetTextureParameter(
			GetPixelShader(),
			AccumulatedFrontfacesLineIntegralTextureParam,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetFogFrontfacesIntegralAccumulationTexture()
			);

		SetTextureParameter(
			GetPixelShader(),
			AccumulatedBackfacesLineIntegralTextureParam,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			GSceneRenderTargets.GetFogBackfacesIntegralAccumulationTexture()
			);

		SetPixelShaderValue( GetPixelShader(), MaxIntegralParameter, DensitySceneInfo->GetMaxIntegral());
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh,INT BatchElementIndex,const FSceneView& View,UBOOL bBackFace)
	{
		MaterialParameters.SetMesh(this,PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaxIntegralParameter;
		Ar << MaterialParameters;	
		Ar << AccumulatedFrontfacesLineIntegralTextureParam;
		Ar << AccumulatedBackfacesLineIntegralTextureParam;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

private:
	FShaderParameter MaxIntegralParameter;
	FMaterialPixelShaderParameters MaterialParameters;
	FShaderResourceParameter AccumulatedFrontfacesLineIntegralTextureParam;
	FShaderResourceParameter AccumulatedBackfacesLineIntegralTextureParam;
};


/**
* Policy for applying fog contribution to scene color
*/
class FFogVolumeApplyDrawingPolicy : public FMeshDrawingPolicy
{
public:
	/** context type */
	typedef FMeshDrawingPolicy::ElementDataType ElementDataType;

	/**
	* Constructor
	* @param InIndexBuffer - index buffer for rendering
	* @param InVertexFactory - vertex factory for rendering
	* @param InMaterialRenderProxy - material instance for rendering
	*/
	FFogVolumeApplyDrawingPolicy(
		const FVertexFactory* InVertexFactory, 
		const FMaterialRenderProxy* InMaterialRenderProxy, 
		const FMaterial& InMaterialResource,
		const FFogVolumeDensitySceneInfo* DensitySceneInfo,
		UBOOL bOverrideWithShaderComplexity);

	// FMeshDrawingPolicy interface.

	/**
	* Match two draw policies
	* @param Other - draw policy to compare
	* @return TRUE if the draw policies are a match
	*/
	UBOOL Matches(const FFogVolumeApplyDrawingPolicy& Other) const;

	/**
	* Executes the draw commands which can be shared between any meshes using this drawer.
	* @param CI - The command interface to execute the draw commands on.
	* @param View - The view of the scene being drawn.
	*/
	void DrawShared(
		const FViewInfo* View,
		FBoundShaderStateRHIParamRef BoundShaderState, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo) const;

	/** 
	* Create bound shader state using the vertex decl from the mesh draw policy
	* as well as the shaders needed to draw the mesh
	* @param DynamicStride - optional stride for dynamic vertex data
	* @return new bound shader state object
	*/
	FBoundShaderStateRHIRef CreateBoundShaderState(DWORD DynamicStride = 0);

	/**
	* Sets the render states for drawing a mesh.
	* @param PrimitiveSceneInfo - The primitive drawing the dynamic mesh.  If this is a view element, this will be NULL.
	* @param Mesh - mesh element with data needed for rendering
	* @param ElementData - context specific data for mesh rendering
	*/
	void SetMeshRenderState(
		const FSceneView& View,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		const FMeshBatch& Mesh,
		INT BatchElementIndex,
		UBOOL bBackFace,
		const ElementDataType& ElementData
		) const;

private:
	FFogVolumeApplyVertexShader* VertexShader;
	FFogVolumeApplyPixelShader* PixelShader;
	UINT NumIntegralInstructions;
};

/**
* Fog apply mesh drawing policy factory. 
* Creates the policies needed for rendering a mesh based on its material
*/
class FFogVolumeApplyDrawingPolicyFactory
{
public:
	enum { bAllowSimpleElements = FALSE };
	struct ContextType {};

	/**
	* Render a dynamic mesh using a fog apply mesh draw policy 
	* @return TRUE if the mesh rendered
	*/
	static UBOOL DrawDynamicMesh(
		const FViewInfo& View,
		ContextType DrawingContext,
		const FMeshBatch& Mesh,
		UBOOL bBackFace,
		UBOOL bPreFog,
		const FPrimitiveSceneInfo* PrimitiveSceneInfo,
		FHitProxyId HitProxyId, 
		const FFogVolumeDensitySceneInfo* DensitySceneInfo
		);
};

extern void ResetFogVolumeIndex();

/**
* Render a Fog Volume.  The density function to use is found in PrimitiveSceneInfo->Scene's FogVolumes map.
* @return TRUE if the mesh rendered
*/
extern UBOOL RenderFogVolume(
	 const FViewInfo* View,
	 const FMeshBatch& Mesh,
	 UBOOL bBackFace,
	 UBOOL bPreFog,
	 const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	 FHitProxyId HitProxyId);
