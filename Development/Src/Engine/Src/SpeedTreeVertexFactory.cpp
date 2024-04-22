/*=============================================================================
SpeedTreeVertexFactory.cpp: SpeedTree vertex factory implementation.
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "SpeedTree.h"
#include "ScenePrivate.h"

#if WITH_SPEEDTREE

UBOOL FSpeedTreeVertexFactory::ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType)
{
	return (Platform != SP_WIIU) && 
		((Material->IsUsedWithSpeedTree() || Material->IsSpecialEngineMaterial()) && !Material->IsUsedWithDecals());
}

void FSpeedTreeVertexFactory::InitRHI()
{
	FVertexDeclarationElementList Elements;

	Elements.AddItem(AccessStreamComponent(Data.PositionComponent, VEU_Position));

	if( Data.WindInfo.VertexBuffer )
		Elements.AddItem(AccessStreamComponent(Data.WindInfo, VEU_BlendIndices));

	EVertexElementUsage TangentBasisUsages[3] = { VEU_Tangent, VEU_Binormal, VEU_Normal };
	for( INT AxisIndex=0; AxisIndex<3; AxisIndex++ )
	{
		if( Data.TangentBasisComponents[AxisIndex].VertexBuffer != NULL )
		{
			Elements.AddItem(AccessStreamComponent(Data.TangentBasisComponents[AxisIndex], TangentBasisUsages[AxisIndex]));
		}
	}

	if( Data.TextureCoordinates.Num() )
	{
		for( UINT CoordinateIndex=0; CoordinateIndex<Data.TextureCoordinates.Num(); CoordinateIndex++ )
		{
			Elements.AddItem(AccessStreamComponent(Data.TextureCoordinates(CoordinateIndex),VEU_TextureCoordinate,CoordinateIndex));
		}

		for( UINT CoordinateIndex=Data.TextureCoordinates.Num(); CoordinateIndex<MAX_TEXCOORDS; CoordinateIndex++ )
		{
			Elements.AddItem(AccessStreamComponent(Data.TextureCoordinates(Data.TextureCoordinates.Num()-1),VEU_TextureCoordinate,CoordinateIndex));
		}
	}

	if( Data.ShadowMapCoordinateComponent.VertexBuffer )
	{
		Elements.AddItem(AccessStreamComponent(Data.ShadowMapCoordinateComponent, VEU_Color));
	}
	else if( Data.TextureCoordinates.Num() )
	{
		Elements.AddItem(AccessStreamComponent(Data.TextureCoordinates(0), VEU_Color));
	}

	InitDeclaration(Elements, FVertexFactory::DataType(), TRUE, TRUE);
}

class FSpeedTreeVertexFactoryShaderParameters : public FVertexFactoryShaderParameters
{
public:
	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		LocalToWorldParameter.Bind( ParameterMap, TEXT("LocalToWorld") );
		WorldToLocalParameter.Bind( ParameterMap, TEXT("WorldToLocal"), TRUE );
		RotationOnlyMatrixParameter.Bind( ParameterMap, TEXT("RotationOnlyMatrix"), TRUE );
		CameraAlignMatrixParameter.Bind( ParameterMap, TEXT("CameraAlignMatrix"), TRUE );
		LODDataParameter.Bind( ParameterMap, TEXT("LODData"), TRUE );

		WindDir.Bind( ParameterMap, TEXT("WindDir"), TRUE );		
		WindTimes.Bind( ParameterMap, TEXT("WindTimes"), TRUE );
		WindDistances.Bind( ParameterMap, TEXT("WindDistances"), TRUE );
		WindLeaves.Bind( ParameterMap, TEXT("WindLeaves"), TRUE );
		WindFrondRipple.Bind( ParameterMap, TEXT("WindFrondRipple"), TRUE );
		WindGust.Bind( ParameterMap, TEXT("WindGust"), TRUE );
		WindGustHints.Bind( ParameterMap, TEXT("WindGustHints"), TRUE );
	}

	virtual void Serialize(FArchive& Ar)
	{
		Ar << LocalToWorldParameter;
		Ar << WorldToLocalParameter;
		Ar << RotationOnlyMatrixParameter;
		Ar << CameraAlignMatrixParameter;
		Ar << LODDataParameter;
		Ar << WindDir;		
		Ar << WindTimes;		
		Ar << WindDistances;	
		Ar << WindLeaves;		
		Ar << WindFrondRipple;
		Ar << WindGust;		
		Ar << WindGustHints;	
	}

	virtual void Set(FShader* VertexShader, const FVertexFactory* VertexFactory, const FSceneView& View) const
	{
		FSpeedTreeVertexFactory* SpeedTreeVertexFactory = (FSpeedTreeVertexFactory*)VertexFactory;
		const USpeedTree* SpeedTree = SpeedTreeVertexFactory->GetSpeedTree();
		check(SpeedTree);
		check(SpeedTree->SRH);

		if(WindDir.IsBound())
		{
			// Update wind if time has passed.
			SpeedTree->SRH->UpdateWind(SpeedTree->WindDirection, SpeedTree->WindStrength, View.Family->CurrentWorldTime);

			SpeedTree::CWind& Wind = SpeedTree->SRH->SpeedTree->GetWind( );
			const FLOAT* WindShaderValues = Wind.GetShaderValues( );

			SetVertexShaderValue( VertexShader->GetVertexShader(), WindDir, *(FVector*)&WindShaderValues[SpeedTree::CWind::SH_WIND_DIR_X] );
			SetVertexShaderValue( VertexShader->GetVertexShader(), WindTimes, *(FVector4*)&WindShaderValues[SpeedTree::CWind::SH_TIME_PRIMARY] );
			SetVertexShaderValue( VertexShader->GetVertexShader(), WindDistances, *(FVector4*)&WindShaderValues[SpeedTree::CWind::SH_DIST_PRIMARY] );
			SetVertexShaderValue( VertexShader->GetVertexShader(), WindGust, *(FVector*)&WindShaderValues[SpeedTree::CWind::SH_STRENGTH_COMBINED] );
			SetVertexShaderValue( VertexShader->GetVertexShader(), WindGustHints, *(FVector*)&WindShaderValues[SpeedTree::CWind::SH_HEIGHT_OFFSET] );
			SetVertexShaderValue( VertexShader->GetVertexShader(), WindLeaves, *(FVector*)&WindShaderValues[SpeedTree::CWind::SH_DIST_LEAVES] );
			SetVertexShaderValue( VertexShader->GetVertexShader(), WindFrondRipple, *(FVector*)&WindShaderValues[SpeedTree::CWind::SH_DIST_FROND_RIPPLE] );
		}

		if( CameraAlignMatrixParameter.IsBound() )
		{
			FMatrix CameraToWorld = View.ViewMatrix;
			CameraToWorld.SetOrigin(FVector(0,0,0));
			CameraToWorld = CameraToWorld.Transpose();
			SetVertexShaderValues<FVector4>( VertexShader->GetVertexShader(), CameraAlignMatrixParameter, (FVector4*)&CameraToWorld,3);
		}
	}

	virtual void SetMesh(FShader* VertexShader, const FMeshBatch& Mesh, INT BatchElementIndex, const FSceneView& View) const
	{
		const FMeshBatchElement& BatchElement = Mesh.Elements(BatchElementIndex);
		FSpeedTreeVertexFactory* SpeedTreeVertexFactory = (FSpeedTreeVertexFactory*)Mesh.VertexFactory;
		const USpeedTree* SpeedTree = SpeedTreeVertexFactory->GetSpeedTree();
		check(SpeedTree && SpeedTree->SRH);
		check(BatchElement.ElementUserData != NULL);
		const FSpeedTreeVertexFactory::MeshUserDataType& MeshUserData = *(FSpeedTreeVertexFactory::MeshUserDataType*)BatchElement.ElementUserData;

		// Call the parameter setting function with the derived data.
		SetMeshInner(VertexShader, BatchElement, View, MeshUserData, SpeedTree->SRH);
	}

	virtual void SetMeshInner(
		FShader* VertexShader,
		const FMeshBatchElement& Mesh,
		const FSceneView& View,
		const FSpeedTreeVertexFactory::MeshUserDataType& MeshUserData,
		const FSpeedTreeResourceHelper* SRH
		) const
	{
		SetVertexShaderValue( 
			VertexShader->GetVertexShader(),
			LocalToWorldParameter,
			Mesh.LocalToWorld.ConcatTranslation(View.PreViewTranslation)
			);
		SetVertexShaderValue( VertexShader->GetVertexShader(), WorldToLocalParameter, Mesh.WorldToLocal);
		SetVertexShaderValue( VertexShader->GetVertexShader(), RotationOnlyMatrixParameter, MeshUserData.RotationOnlyMatrix );

		if( LODDataParameter.IsBound() )
		{
			// Note: These distance calculations much match up with the main renderer, and the way that culling is done in FSpeedTreeSceneProxy::ConditionalDrawElement!
			const FLOAT DistanceSquared = CalculateDistanceSquaredForLOD(MeshUserData.Bounds, View.ViewOrigin);
			const FLOAT Distance = appSqrt(DistanceSquared) * View.LODDistanceFactor;

			const FVector2D LOD(Clamp((Distance - MeshUserData.LODDistances.X) / (MeshUserData.LODDistances.Y - MeshUserData.LODDistances.X), 0.0f, 1.0f),
				-2.0f * Clamp((Distance - MeshUserData.BillboardDistances.X) / (MeshUserData.BillboardDistances.Y - MeshUserData.BillboardDistances.X), 0.0f, 1.0f) / 3.0f);
			SetVertexShaderValue( VertexShader->GetVertexShader(), LODDataParameter, LOD );
		}
	}

private:
	FShaderParameter LocalToWorldParameter;
	FShaderParameter WorldToLocalParameter;
	FShaderParameter RotationOnlyMatrixParameter;
	FShaderParameter CameraAlignMatrixParameter;
	FShaderParameter LODDataParameter;
	FShaderParameter WindDir;		
	FShaderParameter WindTimes;		
	FShaderParameter WindDistances;	
	FShaderParameter WindLeaves;		
	FShaderParameter WindFrondRipple;
	FShaderParameter WindGust;		
	FShaderParameter WindGustHints;	
};

FVertexFactoryShaderParameters* FSpeedTreeVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FSpeedTreeVertexFactoryShaderParameters() : NULL;
}

class FSpeedTreeBillboardVertexFactoryShaderParameters : public FSpeedTreeVertexFactoryShaderParameters
{
public:

	virtual void Bind(const FShaderParameterMap& ParameterMap)
	{
		FSpeedTreeVertexFactoryShaderParameters::Bind(ParameterMap);
		TextureCoordinateScaleBiasParameter.Bind(ParameterMap,TEXT("TextureCoordinateScaleBias"),TRUE);
		ViewToLocalParameter.Bind(ParameterMap,TEXT("ViewToLocal"),TRUE);
		BillboardMaskClipValuesParameter.Bind(ParameterMap,TEXT("BillboardMaskClipValues"),TRUE);
	}

	virtual void Serialize(FArchive& Ar)
	{
		FSpeedTreeVertexFactoryShaderParameters::Serialize(Ar);
		Ar << TextureCoordinateScaleBiasParameter;
		Ar << ViewToLocalParameter;
		Ar << BillboardMaskClipValuesParameter;
	}

	virtual void SetMeshInner(
		FShader* VertexShader,
		const FMeshBatchElement& Mesh,
		const FSceneView& View,
		const FSpeedTreeVertexFactory::MeshUserDataType& MeshUserData,
		const FSpeedTreeResourceHelper* SRH
		) const
	{
		FSpeedTreeVertexFactoryShaderParameters::SetMeshInner(VertexShader,Mesh,View,MeshUserData,SRH);

		// Set the current camera position and direction.
		const FMatrix InvViewMatrix = View.ViewMatrix.Inverse();
		const FVector CurrentCameraOrigin = InvViewMatrix.GetOrigin();
		const FVector CurrentCameraZ = MeshUserData.RotationOnlyMatrix.TransformNormal(InvViewMatrix.GetAxis(2));

		// Compute and set the view-to-local transform.
		const FMatrix ViewToLocal = InvViewMatrix * Mesh.WorldToLocal * MeshUserData.RotationOnlyMatrix;
		SetVertexShaderValues(VertexShader->GetVertexShader(),ViewToLocalParameter,(FVector4*)&ViewToLocal,3);

		// get the correct texcoord info
		FVector4 TextureCoordinateScaleBias;
		SRH->GetVertBillboardTexcoordBiasOffset(appAtan2(CurrentCameraZ.Y, -CurrentCameraZ.X), TextureCoordinateScaleBias);
		SetVertexShaderValue(VertexShader->GetVertexShader(),TextureCoordinateScaleBiasParameter,TextureCoordinateScaleBias);

		// Compute and set the billboard mask clip values
		const FLOAT Distance = (CurrentCameraOrigin - Mesh.LocalToWorld.GetOrigin()).Size();
		const FLOAT BillboardFade = Clamp((Distance - MeshUserData.LODDistances.X) / (MeshUserData.LODDistances.Y - MeshUserData.LODDistances.X), 0.0f, 1.0f);
		const FLOAT HorzFade = Clamp(-2.0f * (CurrentCameraZ.Z + 0.4f), 0.0f, 1.0f);
		const FLOAT BillboardMaskClipValues[2] =
		{
			BillboardFade * HorzFade,
			BillboardFade * (1.0f - HorzFade)
		};
		for(UINT BillboardIndex = 0;BillboardIndex < 2;BillboardIndex++)
		{
			SetVertexShaderValue(
				VertexShader->GetVertexShader(),
				BillboardMaskClipValuesParameter,
				-2.0f * (1.0f - BillboardMaskClipValues[BillboardIndex]) / 3.0f,
				BillboardIndex
				);
		}
	}

private:
	FShaderParameter TextureCoordinateScaleBiasParameter;
	FShaderParameter ViewToLocalParameter;
	FShaderParameter BillboardMaskClipValuesParameter;
};

FVertexFactoryShaderParameters* FSpeedTreeBillboardVertexFactory::ConstructShaderParameters(EShaderFrequency ShaderFrequency)
{
	return ShaderFrequency == SF_Vertex ? new FSpeedTreeBillboardVertexFactoryShaderParameters() : NULL;
}

IMPLEMENT_VERTEX_FACTORY_TYPE(FSpeedTreeBillboardVertexFactory, "SpeedTreeBillboardVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_SPEEDTREE_5_INTEGRATION,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FSpeedTreeBranchVertexFactory, "SpeedTreeBranchVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_SPEEDTREE_5_INTEGRATION,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FSpeedTreeFrondVertexFactory, "SpeedTreeFrondVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_SPEEDTREE_5_INTEGRATION,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FSpeedTreeLeafCardVertexFactory, "SpeedTreeLeafCardVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_SPEEDTREE_5_INTEGRATION,0);
IMPLEMENT_VERTEX_FACTORY_TYPE(FSpeedTreeLeafMeshVertexFactory, "SpeedTreeLeafMeshVertexFactory", TRUE, TRUE, TRUE, FALSE, TRUE, VER_SPEEDTREE_5_INTEGRATION,0);

#endif // WITH_SPEEDTREE

