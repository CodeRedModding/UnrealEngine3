/*=============================================================================
	SplineLoft.cpp
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineSplineClasses.h"
#include "EngineMeshClasses.h"
#include "LocalVertexFactoryShaderParms.h"
#include "UnStaticMeshLight.h"

IMPLEMENT_CLASS(ASplineLoftActor);
IMPLEMENT_CLASS(ASplineLoftActorMovable);

IMPLEMENT_CLASS(USplineMeshComponent);

//////////////////////////////////////////////////////////////////////////
// ASplineLoftActor

void ASplineLoftActor::PostLoad()
{
	Super::PostLoad();
	
	// Make sure components get attached on load, if present
		
	for(INT i=0; i<SplineMeshComps.Num(); i++)
	{
		if(SplineMeshComps(i))
		{
			Components.AddItem(SplineMeshComps(i));	
		}
	}
}

void ASplineLoftActor::UpdateComponentsInternal( UBOOL bCollisionUpdate )
{
	UpdateSplineParams();

	Super::UpdateComponentsInternal(bCollisionUpdate);
}

void ASplineLoftActor::ClearLoftMesh()
{	
	for(INT i=0; i<SplineMeshComps.Num(); i++)
	{
		if(SplineMeshComps(i))
		{
			DetachComponent(SplineMeshComps(i));
		}
	}	
	SplineMeshComps.Empty();	
}

/** Called when spline changes (e.g. handles modified) - to make mesh regen */
void ASplineLoftActor::UpdateSplineComponents()
{
	Super::UpdateSplineComponents();
	
	ClearLoftMesh();

	// Disable light env if not being used
	if(MeshLightEnvironment)
	{
		MeshLightEnvironment->SetEnabled(FALSE);
	}

	// Get all the spline actors we connect to
	TArray<ASplineLoftActor*> ConnectNodes;
	for(INT i=0; i<Connections.Num(); i++)
	{
		ASplineLoftActor* ConnectNode = Cast<ASplineLoftActor>(Connections(i).ConnectTo);		
		if(ConnectNode)
		{
			ConnectNodes.AddItem(ConnectNode);			
		}
	}

	// If no connections, nothing to do
	if(ConnectNodes.Num() == 0)
	{
		return;
	}

	// Create components for each 
	for(INT ConnIdx=0; ConnIdx<ConnectNodes.Num(); ConnIdx++)
	{
		ASplineLoftActor* EndActor = ConnectNodes(ConnIdx);
		USplineComponent* SplineComp = FindSplineComponentTo(EndActor);
		if(EndActor && SplineComp && DeformMesh)
		{
			USplineMeshComponent* SplineMeshComp = ConstructObject<USplineMeshComponent>(USplineMeshComponent::StaticClass(), this);

			// Set mesh and override materials
			SplineMeshComp->SetStaticMesh(DeformMesh);

			for(INT MatIdx=0; MatIdx<DeformMeshMaterials.Num(); MatIdx++)
			{
				SplineMeshComp->SetMaterial(MatIdx, DeformMeshMaterials(MatIdx));
			}

			// Set component to use actor's light env (if present), and enable it
			if(MeshLightEnvironment)
			{
				SplineMeshComp->LightEnvironment = MeshLightEnvironment;
				MeshLightEnvironment->SetEnabled(TRUE);
			}

			SplineMeshComp->bUsePrecomputedShadows = !bMovable;
			SplineMeshComp->bAcceptsLights = bAcceptsLights;
			SplineMeshComp->LDMaxDrawDistance = MeshMaxDrawDistance;
			SplineMeshComp->CachedMaxDrawDistance = MeshMaxDrawDistance;

			AttachComponent(SplineMeshComp);
			SplineMeshComps.AddItem(SplineMeshComp);
		}
		else
		{
			SplineMeshComps.AddItem(NULL);
		}
	}

	// Update position/scale and such
	UpdateSplineParams();
}

/** Quick function that updates params/positions of the spline mesh components  */
void ASplineLoftActor::UpdateSplineParams()
{
	// 
	const FMatrix MeshToWorldTM = LocalToWorld();
	const FMatrix WorldToMeshTM = MeshToWorldTM.Inverse();

	for(INT i=0; i<Connections.Num(); i++)
	{
		ASplineLoftActor* EndActor = Cast<ASplineLoftActor>(Connections(i).ConnectTo);		

		USplineMeshComponent* SplineMeshComp = NULL;
		if(i<SplineMeshComps.Num())
		{
			SplineMeshComp = SplineMeshComps(i);
		}

		if(EndActor && SplineMeshComp)
		{
			// Need to reattach component so that info is sent to render thread
			FComponentReattachContext ReattachContext(SplineMeshComp);

			SplineMeshComp->SplineParams.StartPos = WorldToMeshTM.TransformFVector(Location);
			SplineMeshComp->SplineParams.StartTangent = WorldToMeshTM.TransformNormal( GetWorldSpaceTangent() );
			SplineMeshComp->SplineParams.StartRoll = Roll * (PI/180.f);
			SplineMeshComp->SplineParams.StartOffset = Offset;
			SplineMeshComp->SplineParams.StartScale.X = ScaleX;
			SplineMeshComp->SplineParams.StartScale.Y = ScaleY;

			SplineMeshComp->SplineParams.EndPos = WorldToMeshTM.TransformFVector(EndActor->Location);
			SplineMeshComp->SplineParams.EndTangent = WorldToMeshTM.TransformNormal( EndActor->GetWorldSpaceTangent() );
			SplineMeshComp->SplineParams.EndRoll = EndActor->Roll * (PI/180.f);
			SplineMeshComp->SplineParams.EndScale.X = EndActor->ScaleX;
			SplineMeshComp->SplineParams.EndScale.Y = EndActor->ScaleY;
			SplineMeshComp->SplineParams.EndOffset = EndActor->Offset;

			SplineMeshComp->SplineXDir = WorldToMeshTM.TransformNormal(WorldXDir);			
			SplineMeshComp->bSmoothInterpRollScale = bSmoothInterpRollAndScale;

			SplineMeshComp->BeginDeferredReattach();
		}
	}
}

//////////////////////////////////////////////////////////////////////////
// USplineMeshComponent

/** A vertex factory for spline-deformed static meshes */
struct FSplineMeshVertexFactory : public FLocalVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory);
public:

	FSplineMeshVertexFactory(class FSplineMeshSceneProxy* InSplineProxy) :
		SplineSceneProxy(InSplineProxy)
		{}

	/** Should we cache the material's shadertype on this platform with this vertex factory? */
	static UBOOL ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
	{
		return (Material->IsUsedWithSplineMeshes() || Material->IsSpecialEngineMaterial())
			&& FLocalVertexFactory::ShouldCache(Platform, Material, ShaderType)
			&& !Material->IsUsedWithDecals(); 	
	}

	/** Modify compile environment to enable spline deformation */
	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		OutEnvironment.Definitions.Set(TEXT("USE_SPLINEDEFORM"),TEXT("1"));
	}

	/** Copy the data from another vertex factory */
	void Copy(const FSplineMeshVertexFactory& Other)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			FInstancedStaticMeshVertexFactoryCopyData,
			FSplineMeshVertexFactory*,VertexFactory,this,
			const DataType*,DataCopy,&Other.Data,
		{
			VertexFactory->Data = *DataCopy;
		});
		BeginUpdateResourceRHI(this);
	}

	// FRenderResource interface.
	virtual void InitRHI()
	{
		FLocalVertexFactory::InitRHI();
	}

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	class FSplineMeshSceneProxy* SplineSceneProxy;

private:
};



/** Factory specific params */
class FSplineMeshVertexFactoryShaderParameters : public FLocalVertexFactoryShaderParameters
{
	void Bind(const FShaderParameterMap& ParameterMap);

	void Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const;

	void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const;

	void Serialize(FArchive& Ar)
	{
		FLocalVertexFactoryShaderParameters::Serialize(Ar);
		Ar << SplineStartPosParam;
		Ar << SplineStartTangentParam;
		Ar << SplineStartRollParam;
		Ar << SplineStartScaleParam;
		Ar << SplineStartOffsetParam;
		
		Ar << SplineEndPosParam;
		Ar << SplineEndTangentParam;
		Ar << SplineEndRollParam;
		Ar << SplineEndScaleParam;
		Ar << SplineEndOffsetParam;

		Ar << SplineXDirParam;
		Ar << SmoothInterpRollScaleParam;

		Ar << MeshMinZParam;
		Ar << MeshRangeZParam;
	}

private:
	FShaderParameter SplineStartPosParam;
	FShaderParameter SplineStartTangentParam;
	FShaderParameter SplineStartRollParam;
	FShaderParameter SplineStartScaleParam;
	FShaderParameter SplineStartOffsetParam;
	
	FShaderParameter SplineEndPosParam;
	FShaderParameter SplineEndTangentParam;
	FShaderParameter SplineEndRollParam;
	FShaderParameter SplineEndScaleParam;
	FShaderParameter SplineEndOffsetParam;
	
	FShaderParameter SplineXDirParam;
	FShaderParameter SmoothInterpRollScaleParam;
	
	FShaderParameter MeshMinZParam;
	FShaderParameter MeshRangeZParam;
};

/** Scene proxy for SplineMesh instance */
class FSplineMeshSceneProxy : public FStaticMeshSceneProxy
{
public:

	FSplineMeshSceneProxy(USplineMeshComponent* InComponent):
		FStaticMeshSceneProxy(InComponent),
		VertexFactory(this),
		StaticMeshRenderData(InComponent->StaticMesh->LODModels(0)),
		Component(InComponent)
	{
		// make sure all the materials are okay to be rendered as an instanced mesh
		for (INT LODIndex = 0; LODIndex < LODs.Num(); LODIndex++)
		{
			FStaticMeshSceneProxy::FLODInfo& LODInfo = LODs(LODIndex);
			for (INT ElementIndex = 0; ElementIndex < LODInfo.Elements.Num(); ElementIndex++)
			{
				FStaticMeshSceneProxy::FLODInfo::FElementInfo& Element = LODInfo.Elements(ElementIndex);
				if (!Element.Material->CheckMaterialUsage(MATUSAGE_SplineMesh))
				{
					Element.Material = GEngine->DefaultMaterial;
				}
			}
		}
	
		// Copy spline params from component
		SplineParams = InComponent->SplineParams;
		SplineXDir = InComponent->SplineXDir;
		bSmoothInterpRollScale = InComponent->bSmoothInterpRollScale;

		// Fill in info about the mesh
		MeshMinZ = StaticMesh->Bounds.Origin.Z - StaticMesh->Bounds.BoxExtent.Z;
		MeshRangeZ = 2.f * StaticMesh->Bounds.BoxExtent.Z;

		InitResources();
	}

	virtual ~FSplineMeshSceneProxy()
	{
		ReleaseResources();
	}

	void InitResources();

	void ReleaseResources();

	/** Sets up a FMeshBatch for a specific LOD and element. */
	virtual UBOOL GetMeshElement(INT LODIndex,INT ElementIndex,INT FragmentIndex,BYTE InDepthPriorityGroup,const FMatrix& WorldToLocal,FMeshBatch& OutMeshElement, const UBOOL bUseSelectedMaterial, const UBOOL bUseHoveredMaterial) const
	{
		checkf(LODIndex == 0 && /*ElementIndex == 0 &&*/ FragmentIndex == 0, TEXT("Getting spline static mesh element with invalid params [%d, %d, %d]"), LODIndex, ElementIndex, FragmentIndex);

		if (FStaticMeshSceneProxy::GetMeshElement(LODIndex, ElementIndex, FragmentIndex, InDepthPriorityGroup, WorldToLocal, OutMeshElement, bUseSelectedMaterial, bUseHoveredMaterial))
		{
			OutMeshElement.VertexFactory = &VertexFactory;
			return TRUE;
		}
		return FALSE;
	}

	/** Sets up a wireframe FMeshBatch for a specific LOD. */
	virtual UBOOL GetWireframeMeshElement(INT LODIndex, const FMaterialRenderProxy* WireframeRenderProxy, BYTE InDepthPriorityGroup, const FMatrix& WorldToLocal, FMeshBatch& OutMeshElement) const
	{
		checkf(LODIndex == 0, TEXT("Getting spline static mesh element with invalid LOD [%d]"), LODIndex);

		if (FStaticMeshSceneProxy::GetWireframeMeshElement(LODIndex, WireframeRenderProxy, InDepthPriorityGroup, WorldToLocal, OutMeshElement))
		{
			OutMeshElement.VertexFactory = &VertexFactory;
			return TRUE;
		}
		return FALSE;
	}

	// 	  virtual DWORD GetMemoryFootprint( void ) const { return 0; }

	/** Parameters that define the spline, used to deform mesh */
	FSplineMeshParams SplineParams;
	/** Axis (in component space) that is used to determine X axis for co-ordinates along spline */
	FVector SplineXDir;
	/** Smoothly (cubic) interpolate the Roll and Scale params over spline. */
	UBOOL bSmoothInterpRollScale;
	
	/** Minimum Z value of the entire mesh */
	FLOAT MeshMinZ;
	/** Range of Z values over entire mesh */
	FLOAT MeshRangeZ;

private:
	/** Pointer to vertex factory object */
	FSplineMeshVertexFactory VertexFactory;
	/** Pointer to render data used for this deformed mesh */
	const FStaticMeshRenderData& StaticMeshRenderData;

	/** The owning component. */
	USplineMeshComponent* Component;
};

FVertexFactoryShaderParameters* FSplineMeshVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FSplineMeshVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FSplineMeshVertexFactory, "LocalVertexFactory", TRUE, TRUE, TRUE, TRUE, TRUE, VER_ADDED_SPLINE_MESH_OFFSET, 0);


void FSplineMeshVertexFactoryShaderParameters::Bind(const FShaderParameterMap& ParameterMap)
{
	FLocalVertexFactoryShaderParameters::Bind(ParameterMap);

	SplineStartPosParam.Bind(ParameterMap,TEXT("SplineStartPos"), FALSE);
	SplineStartTangentParam.Bind(ParameterMap,TEXT("SplineStartTangent"), FALSE);
	SplineStartRollParam.Bind(ParameterMap,TEXT("SplineStartRoll"), FALSE);
	SplineStartScaleParam.Bind(ParameterMap,TEXT("SplineStartScale"), FALSE);
	SplineStartOffsetParam.Bind(ParameterMap,TEXT("SplineStartOffset"), FALSE);

	SplineEndPosParam.Bind(ParameterMap,TEXT("SplineEndPos"), FALSE);
	SplineEndTangentParam.Bind(ParameterMap,TEXT("SplineEndTangent"), FALSE);
	SplineEndRollParam.Bind(ParameterMap,TEXT("SplineEndRoll"), FALSE);
	SplineEndScaleParam.Bind(ParameterMap,TEXT("SplineEndScale"), FALSE);
	SplineEndOffsetParam.Bind(ParameterMap,TEXT("SplineEndOffset"), FALSE);
	
	SplineXDirParam.Bind(ParameterMap,TEXT("SplineXDir"), FALSE);
	SmoothInterpRollScaleParam.Bind(ParameterMap,TEXT("SmoothInterpRollScale"), FALSE);

	MeshMinZParam.Bind(ParameterMap,TEXT("MeshMinZ"), FALSE);
	MeshRangeZParam.Bind(ParameterMap,TEXT("MeshRangeZ"), FALSE);
}

void FSplineMeshVertexFactoryShaderParameters::Set(FShader* VertexShader,const FVertexFactory* VertexFactory,const FSceneView& View) const
{
	FLocalVertexFactoryShaderParameters::Set(VertexShader, VertexFactory, View);
	
	FSplineMeshVertexFactory* SplineVertexFactory = (FSplineMeshVertexFactory*)VertexFactory;
	FSplineMeshSceneProxy* SplineProxy = SplineVertexFactory->SplineSceneProxy;
	FSplineMeshParams& SplineParams = SplineProxy->SplineParams;

	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineStartPosParam, FVector4(SplineParams.StartPos, 0.f));
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineStartTangentParam, FVector4(SplineParams.StartTangent, 0.f));
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineStartRollParam, FVector4(SplineParams.StartRoll, 0.f));
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineStartScaleParam, SplineParams.StartScale);
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineStartOffsetParam, SplineParams.StartOffset);

	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineEndPosParam, FVector4(SplineParams.EndPos, 0.f));
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineEndTangentParam, FVector4(SplineParams.EndTangent, 0.f));	
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineEndRollParam, FVector4(SplineParams.EndRoll, 0.f));	
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineEndScaleParam, SplineParams.EndScale);
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineEndOffsetParam, SplineParams.EndOffset);
	
	SetVertexShaderValue(VertexShader->GetVertexShader(), SplineXDirParam, FVector4(SplineProxy->SplineXDir, 0.f));	
	//SetVertexShaderBool(VertexShader->GetVertexShader(), SmoothInterpRollScaleParam, SplineProxy->bSmoothInterpRollScale);	
	SetVertexShaderValue(VertexShader->GetVertexShader(), SmoothInterpRollScaleParam, SplineProxy->bSmoothInterpRollScale ? 1.f : 0.f);

	SetVertexShaderValue(VertexShader->GetVertexShader(), MeshMinZParam, SplineProxy->MeshMinZ);
	SetVertexShaderValue(VertexShader->GetVertexShader(), MeshRangeZParam, SplineProxy->MeshRangeZ);	
}

void FSplineMeshVertexFactoryShaderParameters::SetMesh(FShader* VertexShader,const FMeshBatch& Mesh, INT BatchElementIndex,const FSceneView& View) const
{
	FLocalVertexFactoryShaderParameters::SetMesh(VertexShader, Mesh, BatchElementIndex, View);
}


void FSplineMeshSceneProxy::InitResources()
{
	// Initialize the static mesh's vertex factory.
	ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
		InitSplineMeshVertexFactory,
		FSplineMeshVertexFactory*,VertexFactory,&VertexFactory,
		const FStaticMeshRenderData*,RenderData,&StaticMeshRenderData,
		UStaticMesh*,Parent,Component->StaticMesh,
	{
		FLocalVertexFactory::DataType Data;

		Data.PositionComponent = FVertexStreamComponent(
			&RenderData->PositionVertexBuffer,
			STRUCT_OFFSET(FPositionVertex,Position),
			RenderData->PositionVertexBuffer.GetStride(),
			VET_Float3
			);
		Data.TangentBasisComponents[0] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			STRUCT_OFFSET(FStaticMeshFullVertex,TangentX),
			RenderData->VertexBuffer.GetStride(),
			VET_PackedNormal
			);
		Data.TangentBasisComponents[1] = FVertexStreamComponent(
			&RenderData->VertexBuffer,
			STRUCT_OFFSET(FStaticMeshFullVertex,TangentZ),
			RenderData->VertexBuffer.GetStride(),
			VET_PackedNormal
			);

		if( RenderData->ColorVertexBuffer.GetNumVertices() > 0 )
		{
			Data.ColorComponent = FVertexStreamComponent(
				&RenderData->ColorVertexBuffer,
				0,	// Struct offset to color
				RenderData->ColorVertexBuffer.GetStride(),
				VET_Color
				);
		}

		Data.TextureCoordinates.Empty();

		if( !RenderData->VertexBuffer.GetUseFullPrecisionUVs() )
		{
			for(UINT UVIndex = 0;UVIndex < RenderData->VertexBuffer.GetNumTexCoords();UVIndex++)
			{
				Data.TextureCoordinates.AddItem(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half2
					));
			}
			if(	Parent->LightMapCoordinateIndex >= 0 && (UINT)Parent->LightMapCoordinateIndex < RenderData->VertexBuffer.GetNumTexCoords())
			{
				Data.ShadowMapCoordinateComponent = FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat16UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2DHalf) * Parent->LightMapCoordinateIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Half2
					);
			}
		}
		else
		{
			for(UINT UVIndex = 0;UVIndex < RenderData->VertexBuffer.GetNumTexCoords();UVIndex++)
			{
				Data.TextureCoordinates.AddItem(FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2D) * UVIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float2
					));
			}

			if(	Parent->LightMapCoordinateIndex >= 0 && (UINT)Parent->LightMapCoordinateIndex < RenderData->VertexBuffer.GetNumTexCoords())
			{
				Data.ShadowMapCoordinateComponent = FVertexStreamComponent(
					&RenderData->VertexBuffer,
					STRUCT_OFFSET(TStaticMeshFullVertexFloat32UVs<MAX_TEXCOORDS>,UVs) + sizeof(FVector2D) * Parent->LightMapCoordinateIndex,
					RenderData->VertexBuffer.GetStride(),
					VET_Float2
					);
			}
		}	

		VertexFactory->SetData(Data);
	});

	BeginInitResource(&VertexFactory);
}


void FSplineMeshSceneProxy::ReleaseResources()
{
	VertexFactory.ReleaseResource();
}


/** FSplineMeshSceneProxy */


FPrimitiveSceneProxy* USplineMeshComponent::CreateSceneProxy()
{
	// Verify that the mesh is valid before using it.
	const UBOOL bMeshIsValid = 
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->LODModels(0).NumVertices > 0 && 
		StaticMesh->LODModels(0).IndexBuffer.Indices.Num() > 0;

	if(bMeshIsValid)
	{
		return ::new FSplineMeshSceneProxy(this);
	}
	else
	{
		return NULL;
	}
}

void USplineMeshComponent::UpdateBounds()
{
	// Use util to generate bounds of spline
	FInterpCurvePoint<FVector> Start(0.f, SplineParams.StartPos, SplineParams.StartTangent, SplineParams.StartTangent, CIM_CurveUser);
	FInterpCurvePoint<FVector> End(1.f, SplineParams.EndPos, SplineParams.EndTangent, SplineParams.EndTangent, CIM_CurveUser);
	
	FVector CurveMax(-BIG_NUMBER, -BIG_NUMBER, -BIG_NUMBER);
	FVector CurveMin(BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
	CurveVectorFindIntervalBounds(Start, End, CurveMin, CurveMax);

	FBox LocalBox(CurveMin, CurveMax);
		
	// Find largest extent of mesh in XY, and add on all around
	if(StaticMesh)
	{
		FVector MeshExtent = StaticMesh->Bounds.BoxExtent;		
		FLOAT XYMaxDim = Max<FLOAT>(MeshExtent.X, MeshExtent.Y);
		
		FLOAT MaxScale = Max(SplineParams.StartScale.GetMax(), SplineParams.EndScale.GetMax());
		
		LocalBox = LocalBox.ExpandBy(MaxScale * XYMaxDim);		
	}
		
	Bounds = FBoxSphereBounds( LocalBox.TransformBy(LocalToWorld) );
}

/** Currently do not support point checks against spline mesh components */
UBOOL USplineMeshComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
	return 1;
}

/** Currently do not support line traces against spline mesh components */
UBOOL USplineMeshComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	return 1;
}


/** Note:  This is mirrored to Lightmass::CalcSliceTransform() and LocalVertexShader.usf.  If you update one of these, please update them all! */
FMatrix USplineMeshComponent::CalcSliceTransform(const USplineComponent* SplineComp, const FLOAT DistanceAlong)
{
	if( SplineComp == NULL )
	{
		return FMatrix::Identity;
	}

	FVector SplinePos = SplineComp->GetLocationAtDistanceAlongSpline(DistanceAlong);
	FVector SplineDir = SplineComp->GetTangentAtDistanceAlongSpline(DistanceAlong);

	// Find base frenet frame
	FVector BaseYVec = (SplineDir ^ SplineXDir).SafeNormal();
	FVector BaseXVec = (BaseYVec ^ SplineDir).SafeNormal();

	const FLOAT Alpha = SplineComp->GetSplineLength() / DistanceAlong;

	// Add in the offset, along the frenet frame
	const FVector2D SliceOffset = Lerp<FVector2D>(SplineParams.StartOffset, SplineParams.EndOffset, Alpha);
	SplinePos += SliceOffset.X * BaseXVec;
	SplinePos += SliceOffset.Y * BaseYVec;

	// Apply roll to frame around spline
	const FLOAT Roll = Lerp<FLOAT>(SplineParams.StartRoll, SplineParams.EndRoll, Alpha);
	FLOAT CosAng = appCos(Roll);
	FLOAT SinAng = appSin(Roll);
	FVector XVec = (CosAng * BaseXVec) - (SinAng * BaseYVec);
	FVector YVec = (CosAng * BaseYVec) + (SinAng * BaseXVec);

	// Build overall transform
	const FVector2D Scale = Lerp<FVector2D>(SplineParams.StartScale, SplineParams.EndScale, Alpha);
	return FMatrix(Scale.X*XVec, Scale.Y*YVec, SplineDir, SplinePos);
}

/** */
class FSplineLoftStaticLightingMesh : public FStaticMeshStaticLightingMesh
{
public:

	FSplineLoftStaticLightingMesh(const USplineMeshComponent* InPrimitive,INT InLODIndex,const TArray<ULightComponent*>& InRelevantLights):
		FStaticMeshStaticLightingMesh(InPrimitive, InLODIndex, InRelevantLights),
		SplineComponent(InPrimitive)
	{}

#if WITH_EDITOR

	virtual const struct FSplineMeshParams* GetSplineParameters() const
	{
		return &SplineComponent->SplineParams;
	}

#endif	//WITH_EDITOR

private:
	const USplineMeshComponent* SplineComponent;
};

/** Allocates an implementation of FStaticLightingMesh that will handle static lighting for this component */
FStaticMeshStaticLightingMesh* USplineMeshComponent::AllocateStaticLightingMesh(INT LODIndex, const TArray<ULightComponent*>& InRelevantLights)
{
	return new FSplineLoftStaticLightingMesh(this, LODIndex, InRelevantLights);
}
