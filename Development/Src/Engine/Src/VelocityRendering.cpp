/*=============================================================================
	VelocityRendering.cpp: Velocity rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineParticleClasses.h"
#include "ScenePrivate.h"
#include "UnSkeletalRender.h"			// FSkeletalMeshObject

/** The minimum projected screen radius for a primitive to be drawn in the velocity pass, as a fraction of half the horizontal screen width. */
const FLOAT MinScreenRadiusForVelocityPass = 0.075f;
const FLOAT MinDistanceToDropVelocityPass = 768.0f;
FLOAT MinScreenRadiusForVelocityPassSquared = Square(MinScreenRadiusForVelocityPass);
FLOAT MinDistanceToDropVelocityPassSquared = Square(MinDistanceToDropVelocityPass);

//=============================================================================
/** Shader parameters used by domain and vertex velocity shaders */
class FVelocityShaderParameters
{
public:
	FVelocityShaderParameters(const FShaderParameterMap& ParameterMap)
	{
		PreViewTranslationDeltaParameter.Bind(ParameterMap,TEXT("PreViewTranslationDelta"),TRUE);
	}
	FVelocityShaderParameters() {}

	/** Serializer. */
	friend FArchive& operator<<(FArchive& Ar,FVelocityShaderParameters& P)
	{
		Ar << P.PreViewTranslationDeltaParameter;
		return Ar;
	}

	template<typename ShaderRHIParamRef>
	void Set(ShaderRHIParamRef Shader, const FViewInfo& View, UBOOL bFullMotionBlur)
	{
		FSceneViewState* ViewState = (FSceneViewState*) View.State;

		SetShaderValue(Shader, PreViewTranslationDeltaParameter, View.PreViewTranslation - View.PrevPreViewTranslation);
	}

	/** Set the vertex shader parameter values. */
	void SetVertexShader(FShader* VertexShader, const FViewInfo& View, UBOOL bFullMotionBlur)
	{
		Set(VertexShader->GetVertexShader(), View, bFullMotionBlur);
	}

#if WITH_D3D11_TESSELLATION
	/** Set the domain shader parameter values. */
	void SetDomainShader(FShader* DomainShader, const FViewInfo& View, UBOOL bFullMotionBlur)
	{
		Set(DomainShader->GetDomainShader(), View, bFullMotionBlur);
	}
#endif

private:

	FShaderParameter			PreViewTranslationDeltaParameter;
};

//=============================================================================
/** Encapsulates the Velocity vertex shader. */
class FVelocityVertexShader : public FMeshMaterialVertexShader
{
	DECLARE_SHADER_TYPE(FVelocityVertexShader,MeshMaterial);

public:

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View, UBOOL bFullMotionBlur)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set( this, MaterialRenderContext );

		VertexFactoryParameters.Set( this, VertexFactory, View );

		VelocityParameters.SetVertexShader(this, View, bFullMotionBlur);
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo, const FMeshBatch& Mesh, INT BatchElementIndex, const FViewInfo& View,const FMatrix& PreviousLocalToWorld)
	{
		VertexFactoryParameters.SetMesh(this,Mesh,BatchElementIndex,View);
		MaterialParameters.SetMesh( this, PrimitiveSceneInfo, Mesh, BatchElementIndex, View );
		// LocalToWorld in shader includes PreViewTranslation, this makes Previews.. consistent with that
		SetVertexShaderValue(GetVertexShader(), PreviousLocalToWorldParameter, PreviousLocalToWorld.ConcatTranslation(View.PrevPreViewTranslation));
	}

	UBOOL SupportsVelocity() const
	{
		return PreviousLocalToWorldParameter.IsBound();
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialModifiesMeshPosition()) 
			//and exclude decal materials.
			&& !Material->IsDecalMaterial();
	}

protected:
	FVelocityVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FMeshMaterialVertexShader(Initializer),
		VelocityParameters(Initializer.ParameterMap)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		PreviousLocalToWorldParameter.Bind(Initializer.ParameterMap,TEXT("PreviousLocalToWorld"),TRUE);
	}
	FVelocityVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		bShaderHasOutdatedParameters |= Ar << VertexFactoryParameters;
		Ar << MaterialParameters;
		Ar << VelocityParameters;
		Ar << PreviousLocalToWorldParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FMaterialVertexShaderParameters MaterialParameters;
	FVelocityShaderParameters VelocityParameters;
	FShaderParameter			PreviousLocalToWorldParameter;
};

#if WITH_D3D11_TESSELLATION

//=============================================================================
/** Encapsulates the Velocity hull shader. */
class FVelocityHullShader : public FBaseHullShader
{
	DECLARE_SHADER_TYPE(FVelocityHullShader,MeshMaterial);

protected:
	FVelocityHullShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseHullShader(Initializer)
	{}

	FVelocityHullShader() {}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// same rules as VS
		return FBaseHullShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& FVelocityVertexShader::ShouldCache(Platform, Material, VertexFactoryType);
	}
};

//=============================================================================
/** Encapsulates the Velocity domain shader. */
class FVelocityDomainShader : public FBaseDomainShader
{
	DECLARE_SHADER_TYPE(FVelocityDomainShader,MeshMaterial);

public:

	void SetParameters(const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View, UBOOL bFullMotionBlur)
	{
		FBaseDomainShader::SetParameters(MaterialRenderProxy, View);
		VelocityParameters.SetDomainShader(this, View, bFullMotionBlur);
	}

protected:
	FVelocityDomainShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FBaseDomainShader(Initializer),
		VelocityParameters(Initializer.ParameterMap)
	{}

	FVelocityDomainShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FBaseDomainShader::Serialize(Ar);
		Ar << VelocityParameters;
		return bShaderHasOutdatedParameters;
	}

	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		// same rules as VS
		return FBaseDomainShader::ShouldCache(Platform, Material, VertexFactoryType)
			&& FVelocityVertexShader::ShouldCache(Platform, Material, VertexFactoryType);
	}

private:
	FVelocityShaderParameters VelocityParameters;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityHullShader,TEXT("VelocityShader"),TEXT("MainHull"),SF_Hull,0,0); 
IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityDomainShader,TEXT("VelocityShader"),TEXT("MainDomain"),SF_Domain,0,0);

#endif

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityVertexShader,TEXT("VelocityShader"),TEXT("MainVertexShader"),SF_Vertex,0,0); 

//=============================================================================
/** Encapsulates the Velocity pixel shader. */
class FVelocityPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FVelocityPixelShader,MeshMaterial);
public:
	static UBOOL ShouldCache(EShaderPlatform Platform,const FMaterial* Material,const FVertexFactoryType* VertexFactoryType)
	{
		//Only compile the velocity shaders for the default material or if it's masked,
		return (Material->IsSpecialEngineMaterial() || Material->IsMasked() 
			//or if the material is opaque and two-sided,
			|| (Material->IsTwoSided() && !IsTranslucentBlendMode(Material->GetBlendMode()))
			// or if the material modifies meshes
			|| Material->MaterialModifiesMeshPosition()) 
			//and exclude decal materials.
			&& !Material->IsDecalMaterial();
	}

	FVelocityPixelShader() {}

	FVelocityPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer)
		: FShader(Initializer)
	{
		MaterialParameters.Bind(Initializer.ParameterMap);
		IndividualVelocityScale.Bind( Initializer.ParameterMap, TEXT("IndividualVelocityScale"), TRUE );
		PrevViewProjectionMatrixParameter.Bind(Initializer.ParameterMap,TEXT("PrevViewProjectionMatrix"),TRUE);
	}

	void SetParameters(const FVertexFactory* VertexFactory,const FMaterialRenderProxy* MaterialRenderProxy,const FViewInfo& View)
	{
		FMaterialRenderContext MaterialRenderContext(MaterialRenderProxy, *MaterialRenderProxy->GetMaterial(), View.Family->CurrentWorldTime, View.Family->CurrentRealTime, &View);
		MaterialParameters.Set( this, MaterialRenderContext );
		// Set the view-projection matrix from the previous frame.
		// LocalToWorld in shader includes PreViewTranslation, this makes Previews.. consistent with that
		SetShaderValue(GetPixelShader(), PrevViewProjectionMatrixParameter, FTranslationMatrix(-View.PreViewTranslation) * View.PrevViewProjMatrix );
	}

	void SetMesh(const FPrimitiveSceneInfo* PrimitiveSceneInfo,const FMeshBatch& Mesh,INT BatchElementIndex,const FViewInfo& View,UBOOL bBackFace, FLOAT MotionBlurInstanceScale, UBOOL bFullMotionBlur)
	{
		MaterialParameters.SetMesh( this, PrimitiveSceneInfo, Mesh, BatchElementIndex, View, bBackFace );

		// to mask out motion blur on foreground objects
		UBOOL ForegroundDPG = (View.MotionBlurParams.MaxVelocity == 0.0f);

		FVector4 PackedVelocityScale(0, 0, 0, 0);

		if(!ForegroundDPG)
		{
			// Calculate the maximum velocity (MAX_PIXELVELOCITY is per 30 fps frame).
			FSceneViewState* ViewState = (FSceneViewState*) View.State;
			const FLOAT SizeX	= View.SizeX;
			const FLOAT SizeY	= View.SizeY;
			const FLOAT InvAspectRatio = SizeY / SizeX;

			// 0.5f to map -1..1 to 0..1
			FLOAT ViewMotionBlurScale = 0.5f * ViewState->MotionBlurTimeScale * View.MotionBlurParams.MotionBlurAmount;

			FLOAT ObjectMotionBlurScale	= MotionBlurInstanceScale * ViewMotionBlurScale;
			FLOAT MaxVelocity = MAX_PIXELVELOCITY * View.MotionBlurParams.MaxVelocity;
			FLOAT InvMaxVelocity = 1.0f / MaxVelocity;
			FLOAT ObjectScaleX = ObjectMotionBlurScale * InvMaxVelocity;
			FLOAT ObjectScaleY = ObjectMotionBlurScale * InvMaxVelocity * InvAspectRatio;

			// xy = scale object motion
			PackedVelocityScale.X = ObjectScaleX;
			PackedVelocityScale.Y = -ObjectScaleY;

			if(bFullMotionBlur)
			{
				// to scale the motion vector from -1..1 screen to -1..1 normalized range in the clamp disc
				FLOAT CameraScaleX = ViewMotionBlurScale * InvMaxVelocity;
				FLOAT CameraScaleY = ViewMotionBlurScale * InvMaxVelocity * InvAspectRatio;

				// zw = scale camera motion
				PackedVelocityScale.Z = CameraScaleX;
				PackedVelocityScale.W = -CameraScaleY;
			}
		}

		SetPixelShaderValue( GetPixelShader(), IndividualVelocityScale, PackedVelocityScale );
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << MaterialParameters << IndividualVelocityScale;
		Ar << PrevViewProjectionMatrixParameter;
		return bShaderHasOutdatedParameters;
	}

	virtual UBOOL IsUniformExpressionSetValid(const FUniformExpressionSet& UniformExpressionSet) const 
	{ 
		return MaterialParameters.IsUniformExpressionSetValid(UniformExpressionSet); 
	}
	
private:
	FMaterialPixelShaderParameters	MaterialParameters;
	FShaderParameter				IndividualVelocityScale;
	FShaderParameter PrevViewProjectionMatrixParameter;
};

IMPLEMENT_MATERIAL_SHADER_TYPE(,FVelocityPixelShader,TEXT("VelocityShader"),TEXT("MainPixelShader"),SF_Pixel,VER_IMPROVED_MOTIONBLUR2,0);


//=============================================================================
/** FVelocityDrawingPolicy - Policy to wrap FMeshDrawingPolicy with new shaders. */

FVelocityDrawingPolicy::FVelocityDrawingPolicy(
	const FVertexFactory* InVertexFactory,
	const FMaterialRenderProxy* InMaterialRenderProxy,
	const FMaterial& InMaterialResource
	)
	:	FMeshDrawingPolicy(InVertexFactory,InMaterialRenderProxy,InMaterialResource)	
{
	const FMaterialShaderMap* MaterialShaderIndex = InMaterialResource.GetShaderMap();
	const FMeshMaterialShaderMap* MeshShaderIndex = MaterialShaderIndex->GetMeshShaderMap(InVertexFactory->GetType());

#if WITH_D3D11_TESSELLATION
	HullShader = NULL;
	DomainShader = NULL;

	const EMaterialTessellationMode MaterialTessellationMode = InMaterialResource.GetD3D11TessellationMode();
	if(GRHIShaderPlatform == SP_PCD3D_SM5
		&& InVertexFactory->GetType()->SupportsTessellationShaders() 
		&& MaterialTessellationMode != MTM_NoTessellation)
	{
		UBOOL HasHullShader = MeshShaderIndex->HasShader(&FVelocityHullShader::StaticType);
		UBOOL HasDomainShader = MeshShaderIndex->HasShader(&FVelocityDomainShader::StaticType);

		HullShader = HasHullShader ? MeshShaderIndex->GetShader<FVelocityHullShader>() : NULL;
		DomainShader = HasDomainShader ? MeshShaderIndex->GetShader<FVelocityDomainShader>() : NULL;
	}

#endif

	UBOOL HasVertexShader = MeshShaderIndex->HasShader(&FVelocityVertexShader::StaticType);
	VertexShader = HasVertexShader ? MeshShaderIndex->GetShader<FVelocityVertexShader>() : NULL;

	UBOOL HasPixelShader = MeshShaderIndex->HasShader(&FVelocityPixelShader::StaticType);
	PixelShader = HasPixelShader ? MeshShaderIndex->GetShader<FVelocityPixelShader>() : NULL;
}

UBOOL FVelocityDrawingPolicy::SupportsVelocity() const
{
	return (VertexShader && PixelShader) ? VertexShader->SupportsVelocity() : FALSE;
}

void FVelocityDrawingPolicy::DrawShared( const FSceneView* SceneView, FBoundShaderStateRHIRef ShaderState ) const
{
	// NOTE: Assuming this cast is always safe!
	FViewInfo* View = (FViewInfo*) SceneView;

	UBOOL bFullMotionBlur = View->UseFullMotionBlur();

	// Set the depth-only shader parameters for the material.
	RHISetBoundShaderState( ShaderState );
	VertexShader->SetParameters( VertexFactory, MaterialRenderProxy, *View, bFullMotionBlur );
	PixelShader->SetParameters( VertexFactory, MaterialRenderProxy, *View );

#if WITH_D3D11_TESSELLATION
	if(HullShader && DomainShader)
	{
		HullShader->SetParameters( MaterialRenderProxy, *View );
		DomainShader->SetParameters( MaterialRenderProxy, *View, bFullMotionBlur );
	}
#endif

	// Set the shared mesh resources.
	FMeshDrawingPolicy::DrawShared( View );
}

void FVelocityDrawingPolicy::SetMeshRenderState(
	const FViewInfo& View,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	const FMeshBatch& Mesh,
	INT BatchElementIndex,
	UBOOL bBackFace,
	const ElementDataType& ElementData
	) const
{
	const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
	FMatrix PreviousLocalToWorld;

	// previous transform can be stored in the mesh element's motion blur info
	if (Mesh.MBInfo != NULL)
	{
		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,Mesh.MBInfo->PreviousLocalToWorld);
	}
	// previous transform can also be stored in the scene for each primitive
	else if ( FScene::GetPrimitiveMotionBlurInfo(PrimitiveSceneInfo, PreviousLocalToWorld, View.MotionBlurParams) == TRUE )
	{
		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,PreviousLocalToWorld);
	}	
	else
	{
		VertexShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,BatchElement.LocalToWorld);
	}

	UBOOL bFullMotionBlur = View.UseFullMotionBlur();

	#if WITH_D3D11_TESSELLATION
		if(HullShader && DomainShader)
		{
			DomainShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
			HullShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View);
		}
	#endif

	PixelShader->SetMesh(PrimitiveSceneInfo,Mesh,BatchElementIndex,View,bBackFace,PrimitiveSceneInfo->MotionBlurInstanceScale, bFullMotionBlur);
	FMeshDrawingPolicy::SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,ElementData);
}

/** Determines whether this primitive has motionblur velocity to render */
UBOOL FVelocityDrawingPolicy::HasVelocity(const FViewInfo& View, const FPrimitiveSceneInfo* PrimitiveSceneInfo)
{
	checkSlow(IsInRenderingThread());

	// No velocity if motionblur is off, or if it's a non-moving object (treat as background in that case)
	if ( !View.bRequiresVelocities || View.bCameraCut || !PrimitiveSceneInfo->Proxy->IsMovable() )
	{
		return FALSE;
	}

	// Foreground primitives can always be drawn (0 velocity)
	const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);
	if ( PrimitiveViewRelevance.GetDPG(SDPG_Foreground) )
	{
		return TRUE;
	}

	if(PrimitiveSceneInfo->bHasVelocity)
	{
		return TRUE;
	}

	if(PrimitiveSceneInfo->bVelocityIsSupressed)
	{
		return FALSE;
	}

	// check if the primitive has moved
	{
		const UPrimitiveComponent* Component = PrimitiveSceneInfo->Component;

		// determine if the primitive will generate multiple mesh elements that
		// may have motion blur transform info
		const UParticleSystemComponent* ParticleComponent = ConstCast<UParticleSystemComponent>(Component);
		if (ParticleComponent != NULL)
		{
			// for particle systems only render velocities for mesh elements that have bAllowMotionBlur=True
			// all other particle systems will not render velocities
			return PrimitiveSceneInfo->Proxy ? PrimitiveSceneInfo->Proxy->HasMotionBlurVelocityMeshes() : FALSE;
		}

		FMatrix PreviousLocalToWorld;

		if(FScene::GetPrimitiveMotionBlurInfo(PrimitiveSceneInfo, PreviousLocalToWorld, View.MotionBlurParams))
		{
			// Hasn't moved (treat as background by not rendering any special velocities)?
			if(Component->LocalToWorld.Equals(PreviousLocalToWorld, 0.0001f))
			{
				return FALSE;
			}
		}
		else 
		{
			return FALSE;
		}
	}

	return TRUE;
}


/** 
 * Create bound shader state using the vertex decl from the mesh draw policy
 * as well as the shaders needed to draw the mesh
 * @param DynamicStride - optional stride for dynamic vertex data
 * @return new bound shader state object
 */
FBoundShaderStateRHIRef FVelocityDrawingPolicy::CreateBoundShaderState(DWORD DynamicStride)
{
	FVertexDeclarationRHIRef VertexDeclaration;
	DWORD StreamStrides[MaxVertexElementCount];

	FMeshDrawingPolicy::GetVertexDeclarationInfo(VertexDeclaration, StreamStrides);
	if (DynamicStride)
	{
		StreamStrides[0] = DynamicStride;
	}

	FBoundShaderStateRHIRef BoundShaderState;

#if WITH_D3D11_TESSELLATION
	BoundShaderState = RHICreateBoundShaderStateD3D11(
		VertexDeclaration, 
		StreamStrides, 
		VertexShader->GetVertexShader(),
		GETSAFERHISHADER_HULL(HullShader), 
		GETSAFERHISHADER_DOMAIN(DomainShader),
		PixelShader->GetPixelShader(),
		FGeometryShaderRHIRef(),
		EGST_None);
#else
		BoundShaderState = RHICreateBoundShaderState(VertexDeclaration, StreamStrides, VertexShader->GetVertexShader(), PixelShader->GetPixelShader(), EGST_None);
#endif

	return BoundShaderState;
}

INT Compare(const FVelocityDrawingPolicy& A,const FVelocityDrawingPolicy& B)
{
	COMPAREDRAWINGPOLICYMEMBERS(VertexShader);
	COMPAREDRAWINGPOLICYMEMBERS(PixelShader);
#if WITH_D3D11_TESSELLATION
	COMPAREDRAWINGPOLICYMEMBERS(HullShader);
	COMPAREDRAWINGPOLICYMEMBERS(DomainShader);
#endif
	COMPAREDRAWINGPOLICYMEMBERS(VertexFactory);
	COMPAREDRAWINGPOLICYMEMBERS(MaterialRenderProxy);
	return 0;
}


//=============================================================================
/** Policy to wrap FMeshDrawingPolicy with new shaders. */

void FVelocityDrawingPolicyFactory::AddStaticMesh(FScene* Scene,FStaticMesh* StaticMesh,ContextType)
{
	// Velocity only needs to be directly rendered for movable meshes.
	if (StaticMesh->PrimitiveSceneInfo->Proxy->IsMovable())
	{
	    const FMaterialRenderProxy* MaterialRenderProxy = StaticMesh->MaterialRenderProxy;
	    const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	    EBlendMode BlendMode = Material->GetBlendMode();
	    if ( (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked || BlendMode == BLEND_SoftMasked || BlendMode == BLEND_DitheredTranslucent) && !Material->IsDecalMaterial() )
	    {
		    if ( !Material->IsMasked() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition())
		    {
			    // Default material doesn't handle masked or mesh-mod, and doesn't have the correct bIsTwoSided setting.
			    MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
		    }

			FVelocityDrawingPolicy DrawingPolicy(StaticMesh->VertexFactory,MaterialRenderProxy,*MaterialRenderProxy->GetMaterial());
			if (DrawingPolicy.SupportsVelocity())
			{
				// Add the static mesh to the depth-only draw list.
				Scene->DPGs[StaticMesh->DepthPriorityGroup].VelocityDrawList.AddMesh(
					StaticMesh,
					FVelocityDrawingPolicy::ElementDataType(),
					DrawingPolicy
					);
			}
	    }
	}
}

UBOOL FVelocityDrawingPolicyFactory::DrawDynamicMesh(
	const FViewInfo& View,
	ContextType DrawingContext,
	const FMeshBatch& Mesh,
	UBOOL bBackFace,
	UBOOL bPreFog,
	const FPrimitiveSceneInfo* PrimitiveSceneInfo,
	FHitProxyId HitProxyId
	)
{
	// Only draw opaque materials in the depth pass.
	const FMaterialRenderProxy* MaterialRenderProxy = Mesh.MaterialRenderProxy;
	const FMaterial* Material = MaterialRenderProxy->GetMaterial();
	EBlendMode BlendMode = Material->GetBlendMode();
	if ( (BlendMode == BLEND_Opaque || BlendMode == BLEND_Masked || BlendMode == BLEND_SoftMasked || BlendMode == BLEND_DitheredTranslucent) && !Material->IsDecalMaterial() )
	{
		// This should be enforced at a higher level
		//@todo - figure out why this is failing and re-enable
		//check(FVelocityDrawingPolicy::HasVelocity(View, PrimitiveSceneInfo));
		if ( !Material->IsMasked() && !Material->IsTwoSided() && !Material->MaterialModifiesMeshPosition())
		{
			// Default material doesn't handle masked, and doesn't have the correct bIsTwoSided setting.
			MaterialRenderProxy = GEngine->DefaultMaterial->GetRenderProxy(FALSE);
		}
		FVelocityDrawingPolicy DrawingPolicy(Mesh.VertexFactory, MaterialRenderProxy, *MaterialRenderProxy->GetMaterial());
		if ( DrawingPolicy.SupportsVelocity() )
		{			
			DrawingPolicy.DrawShared(&View,DrawingPolicy.CreateBoundShaderState(Mesh.GetDynamicVertexStride()));
			for( INT BatchElementIndex=0;BatchElementIndex < Mesh.Elements.Num();BatchElementIndex++ )
			{
				DrawingPolicy.SetMeshRenderState(View,PrimitiveSceneInfo,Mesh,BatchElementIndex,bBackFace,FMeshDrawingPolicy::ElementDataType());
				DrawingPolicy.DrawMesh(Mesh, BatchElementIndex);
			}
			return TRUE;
		}
		return FALSE;
	}
	else
	{
		return FALSE;
	}
}

/** Renders the velocities of movable objects for the motion blur effect. */
void FSceneRenderer::RenderVelocities(UINT DPGIndex)
{
	SCOPED_DRAW_EVENT(EventVelocities)(DEC_SCENE_ITEMS,TEXT("RenderVelocities"));
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_VelocityDrawTime, !bIsSceneCapture);

	UBOOL bWroteVelocities = FALSE;
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		FViewInfo& View = Views(ViewIndex);
		if ( View.bRequiresVelocities )
		{
			SCOPED_CONDITIONAL_DRAW_EVENT(EventView,Views.Num() > 1)(DEC_SCENE_ITEMS,TEXT("View%d"),ViewIndex);

			const FSceneViewState* ViewState = (FSceneViewState*)View.State;

			if ( !bWroteVelocities )
			{
				GSceneRenderTargets.BeginRenderingVelocities();
				bWroteVelocities = TRUE;
			}

			// Set the device viewport for the view.
			const UINT BufferSizeX = GSceneRenderTargets.GetBufferSizeX();
			const UINT BufferSizeY = GSceneRenderTargets.GetBufferSizeY();
			const UINT VelocityBufferSizeX = GSceneRenderTargets.GetVelocityBufferSizeX();
			const UINT VelocityBufferSizeY = GSceneRenderTargets.GetVelocityBufferSizeY();
			const UINT MinX = View.RenderTargetX * VelocityBufferSizeX / BufferSizeX;
			const UINT MinY = View.RenderTargetY * VelocityBufferSizeY / BufferSizeY;
			const UINT MaxX = (View.RenderTargetX + View.RenderTargetSizeX) * VelocityBufferSizeX / BufferSizeX;
			const UINT MaxY = (View.RenderTargetY + View.RenderTargetSizeY) * VelocityBufferSizeY / BufferSizeY;
			RHISetViewport(MinX,MinY,0.0f,MaxX,MaxY,1.0f);
			RHISetViewParameters(View);
			RHISetMobileHeightFogParams(View.HeightFogParams);

			// Clear the velocity buffer (0.0f means "use static background velocity").
			RHIClear(TRUE,FLinearColor(0.0f, 0.0f, 0.0f, 0.0f),FALSE,1.0f,FALSE,0);

			// Blending is not supported with the velocity buffer format on other platforms
			RHISetBlendState(TStaticBlendState<>::GetRHI());
			// opaque velocities in R|G channels
			RHISetColorWriteMask(CW_RED|CW_GREEN);	
			// Use depth tests, no z-writes, backface-culling. 
			RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
			static FLOAT DepthBias = 0.0f;	//-0.01f;
			const FRasterizerStateInitializerRHI RasterState = { FM_Solid,CM_CW, DepthBias, 0.0f, TRUE };
			RHISetRasterizerStateImmediate( RasterState);

			// Draw velocities for movable static meshes.
			bWroteVelocities |= Scene->DPGs[DPGIndex].VelocityDrawList.DrawVisible(View,View.StaticMeshVelocityMap);

			// Draw velocities for movable dynamic meshes.
			TDynamicPrimitiveDrawer<FVelocityDrawingPolicyFactory> Drawer(
				&View,DPGIndex,FVelocityDrawingPolicyFactory::ContextType(DDM_AllOccluders),TRUE,FALSE,TRUE
				);
			for(INT PrimitiveIndex = 0;PrimitiveIndex < View.VisibleDynamicPrimitives.Num();PrimitiveIndex++)
			{
				const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives(PrimitiveIndex);
				const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);
				const UBOOL bVisible = View.PrimitiveVisibilityMap(PrimitiveSceneInfo->Id);
				const FLOAT LODFactorDistanceSquared = (PrimitiveSceneInfo->Bounds.Origin - View.ViewOrigin).SizeSquared() * Square(View.LODDistanceFactor);

				// Only render if visible.
				if( bVisible 
					// Used to determine whether object is movable or not.
					&& PrimitiveSceneInfo->Proxy->IsMovable() 
					// Skip translucent objects as they don't support velocities and in the case of particles have a significant CPU overhead.
					&& PrimitiveViewRelevance.bOpaqueRelevance 
					// Only render if primitive is relevant to the current DPG.
					&& PrimitiveViewRelevance.GetDPG(DPGIndex) 
					// Skip primitives that only cover a small amount of screenspace, motion blur on them won't be noticeable.
					&& ( MinDistanceToDropVelocityPassSquared > LODFactorDistanceSquared  ||
					Square(PrimitiveSceneInfo->Bounds.SphereRadius) > MinScreenRadiusForVelocityPassSquared * LODFactorDistanceSquared )
					// Only render primitives with velocity.
					&& FVelocityDrawingPolicy::HasVelocity(View, PrimitiveSceneInfo) )
				{
					Drawer.SetPrimitive(PrimitiveSceneInfo);
					PrimitiveSceneInfo->Proxy->DrawDynamicElements(
						&Drawer,
						&View,
						DPGIndex
						);
				}
			}				
			bWroteVelocities |= Drawer.IsDirty();

			// For each view we draw the foreground DPG dynamic objects into the velocity buffer so that we can do the motion blur shader after all DPGs
			// are finished.
			//
			// Note that we simply write a velocity of 0 without any regard to the depth buffer.

			static UBOOL bDrawForegroundVelocities = TRUE;
			if (bDrawForegroundVelocities)
			{
				RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

				FLOAT OldMaxVelocity = View.MotionBlurParams.MaxVelocity;
				View.MotionBlurParams.MaxVelocity = 0.f;

				// Draw velocities for movable dynamic meshes.
				TDynamicPrimitiveDrawer<FVelocityDrawingPolicyFactory> ForegroundDrawer(&View,SDPG_Foreground,FVelocityDrawingPolicyFactory::ContextType(DDM_AllOccluders),TRUE);

				for(INT PrimitiveIndex = 0;PrimitiveIndex < View.VisibleDynamicPrimitives.Num();PrimitiveIndex++)
				{
					const FPrimitiveSceneInfo* PrimitiveSceneInfo = View.VisibleDynamicPrimitives(PrimitiveIndex);
					const FPrimitiveViewRelevance& PrimitiveViewRelevance = View.PrimitiveViewRelevanceMap(PrimitiveSceneInfo->Id);

					// Used to determine whether object is movable or not.
					if( PrimitiveSceneInfo->Proxy->IsMovable() 
						// Skip translucent objects as they don't support velocities and in the case of particles have a significant CPU overhead.
						&& PrimitiveViewRelevance.bOpaqueRelevance 
						// Only render if primitive is relevant to the foreground DPG.
						&& PrimitiveViewRelevance.GetDPG(SDPG_Foreground)
						// Only render primitives with velocity.
						&& FVelocityDrawingPolicy::HasVelocity(View, PrimitiveSceneInfo) )
					{
						ForegroundDrawer.SetPrimitive(PrimitiveSceneInfo);
						PrimitiveSceneInfo->Proxy->DrawDynamicElements(&ForegroundDrawer,&View,SDPG_Foreground);
					}
				}
				bWroteVelocities |= ForegroundDrawer.IsDirty();
				View.MotionBlurParams.MaxVelocity = OldMaxVelocity;

				RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
			}

			if (Scene->RadialBlurInfos.Num() > 0)
			{
				SCOPED_DRAW_EVENT(EventRadialBlurVelocities)(DEC_SCENE_ITEMS,TEXT("RadialBlur"));				
				// disable depth tests and writes
				RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
				// no blending supported since velocities stored as biased unsigned values (ie. [-1,+1] mapped to [0,1] range)
				RHISetBlendState(TStaticBlendState<>::GetRHI());							
				// translucent velocities stored in B|A channels since they are scaled independent of opaque motion-based velocities
				RHISetColorWriteMask(CW_BLUE|CW_ALPHA);
				// since the radial blur effects can not be blended
				// just select the strongest effect in the scene relative to the view location
				FRadialBlurSceneProxy* RadialBlurToRender = NULL;
				FLOAT MaxRadialBlurScale = 0.0f;
				for( TMap<const URadialBlurComponent*, FRadialBlurSceneProxy*>::TIterator It(Scene->RadialBlurInfos); It; ++It)
				{
					FRadialBlurSceneProxy* RadialBlur = It.Value();
					if (RadialBlur->bRenderAsVelocity && 
						RadialBlur->IsRenderable(&View,DPGIndex,0))
					{
						// keep track of the radial blur effect with the highest blur scale
						FLOAT RadialBlurScale = RadialBlur->CalcBlurScale(&View);
						if (RadialBlurScale > MaxRadialBlurScale)
						{
							MaxRadialBlurScale = RadialBlurScale;
							RadialBlurToRender = RadialBlur;
						}
					}
				}
				// render radial blur as velocities
				if (RadialBlurToRender != NULL)
				{
					bWroteVelocities |= RadialBlurToRender->DrawVelocity(&View,DPGIndex,0);
				}
				// restore to default depth test
				RHISetDepthState(TStaticDepthState<TRUE,CF_LessEqual>::GetRHI());
			}

			// restore any color write state changes
			RHISetColorWriteMask(CW_RGBA);
		}
	}

	if ( bWroteVelocities )
	{
		RHISetRasterizerState( TStaticRasterizerState<>::GetRHI());
		GSceneRenderTargets.FinishRenderingVelocities();
	}
}
