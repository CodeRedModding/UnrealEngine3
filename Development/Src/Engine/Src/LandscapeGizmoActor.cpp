/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "EngineDecalClasses.h"
#include "LandscapeDataAccess.h"
#include "LandscapeRender.h"

namespace
{
	enum PreviewType
	{
		Invalid = -1,
		Both = 0,
		Add = 1,
		Sub = 2,
	};
}

IMPLEMENT_CLASS(ALandscapeGizmoActor);
IMPLEMENT_CLASS(ALandscapeGizmoActiveActor);
IMPLEMENT_CLASS(ULandscapeGizmoRenderComponent);

class FLandscapeGizmoMeshRenderProxy : public FMaterialRenderProxy
{
public:
	const FMaterialRenderProxy* const Parent;
	const FLOAT TopHeight;
	const FLOAT BottomHeight;
	const UTexture2D* AlphaTexture;
	const FLinearColor ScaleBias;

	/** Initialization constructor. */
	FLandscapeGizmoMeshRenderProxy(const FMaterialRenderProxy* InParent, const FLOAT InTop, const FLOAT InBottom, const UTexture2D* InAlphaTexture, const FLinearColor InScaleBias):
		Parent(InParent),
		TopHeight(InTop),
		BottomHeight(InBottom),
		AlphaTexture(InAlphaTexture),
		ScaleBias(InScaleBias)
	{}

	// FMaterialRenderProxy interface.
	virtual const class FMaterial* GetMaterial() const
	{
		return Parent->GetMaterial();
	}
	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("AlphaScaleBias")))
		{
			*OutValue = ScaleBias;
			return TRUE;
		}
		return Parent->GetVectorValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("Top")))
		{
			*OutValue = TopHeight;
			return TRUE;
		}
		else if (ParameterName == FName(TEXT("Bottom")))
		{
			*OutValue = BottomHeight;
			return TRUE;
		}
		return Parent->GetScalarValue(ParameterName, OutValue, Context);
	}
	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		if (ParameterName == FName(TEXT("AlphaTexture")))
		{
			*OutValue = AlphaTexture ? AlphaTexture->Resource : GBlackTexture;
			return TRUE;
		}
		return Parent->GetTextureValue(ParameterName, OutValue, Context);
	}
};

/** Represents a NavMeshRenderingComponent to the scene manager. */
class FLandscapeGizmoRenderSceneProxy : public FPrimitiveSceneProxy
{
public:
	FLandscapeGizmoRenderSceneProxy(const ULandscapeGizmoRenderComponent* InComponent):
	  FPrimitiveSceneProxy(InComponent)
	  {
		  Gizmo = Cast<ALandscapeGizmoActiveActor>(InComponent->GetOwner());
		  Comp = InComponent;
	  }

	  virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	  {
		  //FMemMark Mark(GRenderingThreadMemStack);
#if WITH_EDITOR
		  if( (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) && GIsEditor && Gizmo && Gizmo->TargetLandscapeInfo && Gizmo->TargetLandscapeInfo->LandscapeProxy && Gizmo->GizmoMeshMaterial && Gizmo->GizmoMeshMaterial2)
		  {
			  FVector XAxis, YAxis, Origin;
			  ALandscapeProxy* Proxy = Gizmo->TargetLandscapeInfo->LandscapeProxy;
			  FMatrix WToL = Proxy->WorldToLocal();
			  FMatrix LToW = Proxy->LocalToWorld();
			  FVector BaseLocation = WToL.TransformFVector(Gizmo->Location);
			  FLOAT ScaleXY = Proxy->DrawScale * Proxy->DrawScale3D.X;
			  FLOAT ScaleZ = Proxy->DrawScale * Proxy->DrawScale3D.Z;
			  const FLOAT W = Gizmo->GetWidth() / (2 * ScaleXY);
			  const FLOAT H = Gizmo->GetHeight() / (2 * ScaleXY);
			  const FLOAT L = Gizmo->GetLength() / ScaleZ;
			  FMatrix GizmoRT = FRotationTranslationMatrix(FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0)) * LToW;
			  Gizmo->FrustumVerts[0] = GizmoRT.TransformFVector(FVector( - W, - H, BaseLocation.Z + L ));
			  Gizmo->FrustumVerts[1] = GizmoRT.TransformFVector(FVector( + W, - H, BaseLocation.Z + L ));
			  Gizmo->FrustumVerts[2] = GizmoRT.TransformFVector(FVector( + W, + H, BaseLocation.Z + L ));
			  Gizmo->FrustumVerts[3] = GizmoRT.TransformFVector(FVector( - W, + H, BaseLocation.Z + L ));

			  Gizmo->FrustumVerts[4] = GizmoRT.TransformFVector(FVector( - W, - H, BaseLocation.Z ));
			  Gizmo->FrustumVerts[5] = GizmoRT.TransformFVector(FVector( + W, - H, BaseLocation.Z ));
			  Gizmo->FrustumVerts[6] = GizmoRT.TransformFVector(FVector( + W, + H, BaseLocation.Z ));
			  Gizmo->FrustumVerts[7] = GizmoRT.TransformFVector(FVector( - W, + H, BaseLocation.Z ));

			  XAxis = GizmoRT.TransformFVector(FVector( + W,	0,		BaseLocation.Z + L ));
			  YAxis = GizmoRT.TransformFVector(FVector( 0,	+ H,	BaseLocation.Z + L ));
			  Origin = GizmoRT.TransformFVector(FVector( 0,	0,		BaseLocation.Z + L ));

			  // Axis
			  PDI->DrawLine( Origin, XAxis, FLinearColor(1, 0, 0), SDPG_World );
			  PDI->DrawLine( Origin, YAxis, FLinearColor(0, 1, 0), SDPG_World );

			  {
				  FDynamicMeshBuilder MeshBuilder;

				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[0], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[1], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[2], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[3], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[4], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[5], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[6], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[7], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[1], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[0], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[4], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[5], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[3], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[2], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[6], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[7], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[2], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[1], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[5], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[6], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[0], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[3], FVector2D(1, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[7], FVector2D(1, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
				  MeshBuilder.AddVertex(Gizmo->FrustumVerts[4], FVector2D(0, 1), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));

				  for (INT i = 0; i < 6; ++i)
				  {
					  INT Idx = i*4;
					  MeshBuilder.AddTriangle( Idx, Idx+2, Idx+1 );
					  MeshBuilder.AddTriangle( Idx, Idx+3, Idx+2 );
				  }

				  MeshBuilder.Draw(PDI, FMatrix::Identity, Gizmo->DataType != LGT_None ? Gizmo->GizmoDataMaterial->GetRenderProxy(FALSE) : Gizmo->GizmoMaterial->GetRenderProxy(FALSE), SDPG_World, 0, TRUE);
			  }

			  if (Gizmo->DataType & LGT_Height)
			  {		  		
				  FLOAT ScaleX = Gizmo->GetWidth() / Gizmo->CachedWidth / ScaleXY * Gizmo->CachedScaleXY;
				  FLOAT ScaleY = Gizmo->GetHeight() / Gizmo->CachedHeight / ScaleXY * Gizmo->CachedScaleXY;
				  FScaleMatrix Mat(FVector(ScaleX, ScaleY, L));
				  FMatrix NormalM = Mat.Inverse().Transpose();
				  //FMemMark MemStackMark(GRenderingThreadMemStack);

				  FDynamicMeshBuilder MeshBuilder;

				  // Render sampled height
				  for (INT Y = 0; Y < Gizmo->SampleSizeY; ++Y)
				  {
					  for (INT X = 0; X < Gizmo->SampleSizeX; ++X)
					  {
						  FVector SampledPos = Gizmo->SampledHeight(X + Y * ALandscapeGizmoActiveActor::DataTexSize);
						  SampledPos.X *= ScaleX;
						  SampledPos.Y *= ScaleY;
						  SampledPos.Z = Gizmo->GetLandscapeHeight(SampledPos.Z);

						  FVector SampledNormal = NormalM.TransformNormal(Gizmo->SampledNormal(X + Y * ALandscapeGizmoActiveActor::DataTexSize));
						  SampledNormal = SampledNormal.SafeNormal();
						  FVector TangentX(SampledNormal.Z, 0, -SampledNormal.X);
						  TangentX = TangentX.SafeNormal();

						  MeshBuilder.AddVertex(SampledPos, FVector2D((FLOAT)X / (Gizmo->SampleSizeX), (FLOAT)Y / (Gizmo->SampleSizeY)), TangentX, SampledNormal^TangentX, SampledNormal, FColor(255, 255, 255) );
					  }
				  }

				  for (INT Y = 0; Y < Gizmo->SampleSizeY; ++Y)
				  {
					  for (INT X = 0; X < Gizmo->SampleSizeX; ++X)
					  {
						  if (X < Gizmo->SampleSizeX - 1 && Y < Gizmo->SampleSizeY - 1)
						  {
							  MeshBuilder.AddTriangle( (X+0) + (Y+0) * Gizmo->SampleSizeX, (X+1) + (Y+1) * Gizmo->SampleSizeX, (X+1) + (Y+0) * Gizmo->SampleSizeX );
							  MeshBuilder.AddTriangle( (X+0) + (Y+0) * Gizmo->SampleSizeX, (X+0) + (Y+1) * Gizmo->SampleSizeX, (X+1) + (Y+1) * Gizmo->SampleSizeX );
						  }
					  }
				  }

				  FMatrix MeshRT = FTranslationMatrix(FVector(- W + 0.5, - H + 0.5, 0)) * FRotationTranslationMatrix(FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0)) * LToW;
				  
				  FLandscapeGizmoMeshRenderProxy RenderProxy( GLandscapePreviewMeshRenderMode == PreviewType::Sub ? Gizmo->GizmoMeshMaterial2->GetRenderProxy(FALSE) : Gizmo->GizmoMeshMaterial->GetRenderProxy(FALSE), BaseLocation.Z + L, BaseLocation.Z, Gizmo->GizmoTexture, FLinearColor(Gizmo->TextureScale.X, Gizmo->TextureScale.Y, 0, 0) );
				  MeshBuilder.Draw(PDI, MeshRT, &RenderProxy, SDPG_World, 0, FALSE);
			  }
		  }
#endif
	  };

	  virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	  {
		  FPrimitiveViewRelevance Result;
#if WITH_EDITOR
		  const UBOOL bVisible = (View->Family->ShowFlags & SHOW_Terrain) != 0;
		  Result.bDynamicRelevance = IsShown(View) && bVisible && GIsEditor && Gizmo->TargetLandscapeInfo;
		  Result.bTranslucentRelevance = TRUE;
		  Result.SetDPG(SDPG_World,TRUE);
#endif
		  return Result;
	  }

	  virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	  DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

	  ALandscapeGizmoActiveActor* Gizmo;
	  const ULandscapeGizmoRenderComponent* Comp;
};

FPrimitiveSceneProxy* ULandscapeGizmoRenderComponent::CreateSceneProxy()
{
	return new FLandscapeGizmoRenderSceneProxy(this);
}

#if WITH_EDITOR

void ALandscapeGizmoActor::Duplicate(ALandscapeGizmoActor* Gizmo)
{
	Gizmo->Width = Width;
	Gizmo->Height = Height;
	Gizmo->LengthZ = LengthZ;
	Gizmo->MarginZ = MarginZ;
	//Gizmo->TargetLandscapeInfo = TargetLandscapeInfo;

	Gizmo->Location = Location;
	Gizmo->Rotation = Rotation;

	Gizmo->DrawScale = DrawScale;
	Gizmo->DrawScale3D = DrawScale3D;

	Gizmo->MinRelativeZ = MinRelativeZ;
	Gizmo->RelativeScaleZ = RelativeScaleZ;
}

void ALandscapeGizmoActiveActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	const FName PropertyName = PropertyChangedEvent.Property ? PropertyChangedEvent.Property->GetFName() : NAME_None;
	if( PropertyName == FName(TEXT("LengthZ")) )
	{
		if (LengthZ < 0)
		{
			LengthZ = MarginZ;
		}
	}
	else if ( PropertyName == FName(TEXT("TargetLandscapeInfo")) )
	{
		SetTargetLandscape(TargetLandscapeInfo);
	}
	// AActor::PostEditChange will ForceUpdateComponents()
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

/**
* Whenever the decal actor has moved:
*  - Copy the actor rot/pos info over to the decal component
*  - Trigger updates on the decal component to recompute its matrices and generate new geometry.
*/
void ALandscapeGizmoActiveActor::PostEditMove(UBOOL bFinished)
{
	Super::PostEditMove( bFinished );
}

ALandscapeGizmoActor* ALandscapeGizmoActiveActor::SpawnGizmoActor()
{
	// ALandscapeGizmoActor is history for ALandscapeGizmoActiveActor
	ALandscapeGizmoActor* NewActor = Cast<ALandscapeGizmoActor>(GWorld->SpawnActor(ALandscapeGizmoActor::StaticClass()));
	Duplicate(NewActor);
	return NewActor;
}

void ALandscapeGizmoActiveActor::PostLoad()
{
	Super::PostLoad();
}

void ALandscapeGizmoActiveActor::SetTargetLandscape(ULandscapeInfo* LandscapeInfo)
{
	ULandscapeInfo* PrevInfo = TargetLandscapeInfo;
	if (!LandscapeInfo || LandscapeInfo->HasAnyFlags(RF_BeginDestroyed))
	{
		TargetLandscapeInfo = NULL;
		if (GWorld)
		{
			for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
			{
				LandscapeInfo = It.Value();
				if (LandscapeInfo && !LandscapeInfo->HasAnyFlags(RF_BeginDestroyed))
				{
					if (LandscapeInfo->LandscapeProxy && LandscapeInfo->LandscapeProxy->LandscapeGuid == It.Key() && !LandscapeInfo->LandscapeProxy->HasAnyFlags(RF_BeginDestroyed) )
					{
						TargetLandscapeInfo = LandscapeInfo;
						break;
					}
				}
			}
		}
	}
	else
	{
		TargetLandscapeInfo = LandscapeInfo;
	}

	if (TargetLandscapeInfo && TargetLandscapeInfo != PrevInfo && TargetLandscapeInfo->LandscapeProxy && TargetLandscapeInfo->LandscapeProxy->ComponentSizeQuads > 0)
	{
		ALandscapeProxy* Proxy = TargetLandscapeInfo->LandscapeProxy;
		MarginZ = Proxy->DrawScale * Proxy->DrawScale3D.Z * 3;
		Width = Height = Proxy->DrawScale * Proxy->DrawScale3D.X * (Proxy->ComponentSizeQuads+1);

		FLOAT LengthZ;
		FVector NewLocation = TargetLandscapeInfo->GetLandscapeCenterPos(LengthZ);
		SetLength(LengthZ);
		SetLocation( NewLocation );
		SetRotation(FRotator(0, 0, 0));
	}
}

void ALandscapeGizmoActiveActor::ClearGizmoData()
{
	DataType = LGT_None;
	SelectedData.Empty();
	LayerNames.Empty();
}

void ALandscapeGizmoActiveActor::FitToSelection()
{
	if (TargetLandscapeInfo)
	{
		// Find fit size
		INT MinX = MAXINT, MinY = MAXINT, MaxX = MININT, MaxY = MININT;
		TargetLandscapeInfo->GetSelectedExtent(MinX, MinY, MaxX, MaxY);
		if (MinX != MAXINT && TargetLandscapeInfo->LandscapeProxy)
		{
			ALandscapeProxy* Proxy = TargetLandscapeInfo->LandscapeProxy;
			FLOAT ScaleXY = Proxy->DrawScale * Proxy->DrawScale3D.X;
			Width = ScaleXY * (MaxX - MinX + 1) / (DrawScale * DrawScale3D.X);
			Height = ScaleXY * (MaxY - MinY + 1) / (DrawScale * DrawScale3D.Y);
			FLOAT LengthZ;
			FVector NewLocation = TargetLandscapeInfo->GetLandscapeCenterPos(LengthZ, MinX, MinY, MaxX, MaxY);
			SetLength(LengthZ);
			SetLocation(NewLocation);
			SetRotation(FRotator(0, 0, 0));
			// Reset Z render scale values...
			MinRelativeZ = 0.f;
			RelativeScaleZ = 1.f;
		}
	}
}

void ALandscapeGizmoActiveActor::FitMinMaxHeight()
{
	if (TargetLandscapeInfo)
	{
		FLOAT MinZ = HALF_WORLD_MAX, MaxZ = -HALF_WORLD_MAX;
		// Change MinRelativeZ and RelativeZScale to fit Gizmo Box
		for (TMap<QWORD, FGizmoSelectData>::TConstIterator It(SelectedData); It; ++It )
		{
			const FGizmoSelectData& Data = It.Value();
			MinZ = Min(MinZ, Data.HeightData);
			MaxZ = Max(MaxZ, Data.HeightData);
		}

		if (MinZ != HALF_WORLD_MAX && MaxZ > MinZ + KINDA_SMALL_NUMBER)
		{
			MinRelativeZ = MinZ;
			RelativeScaleZ = 1.f / (MaxZ - MinZ);
		}
	}
}

FLOAT ALandscapeGizmoActiveActor::GetNormalizedHeight(WORD LandscapeHeight)
{
	if (TargetLandscapeInfo && TargetLandscapeInfo->LandscapeProxy)
	{
		// Need to make it scale...?
		ALandscapeProxy* Proxy = TargetLandscapeInfo->LandscapeProxy;
		FLOAT ZScale = GetLength();
		if (ZScale > KINDA_SMALL_NUMBER)
		{
			FVector LocalGizmoPos = Proxy->WorldToLocal().TransformFVector(Location);
			return Clamp<FLOAT>( (( LandscapeDataAccess::GetLocalHeight(LandscapeHeight) - LocalGizmoPos.Z) * Proxy->DrawScale * Proxy->DrawScale3D.Z) / ZScale, 0.f, 1.f );
		}
	}
	return 0.f;
}

FLOAT ALandscapeGizmoActiveActor::GetWorldHeight(FLOAT NormalizedHeight)
{
	if (TargetLandscapeInfo && TargetLandscapeInfo->LandscapeProxy)
	{
		ALandscapeProxy* Proxy = TargetLandscapeInfo->LandscapeProxy;
		FLOAT ZScale = GetLength();
		if (ZScale > KINDA_SMALL_NUMBER)
		{
			FVector LocalGizmoPos = Proxy->WorldToLocal().TransformFVector(Location);
			return NormalizedHeight * ZScale + LocalGizmoPos.Z * Proxy->DrawScale * Proxy->DrawScale3D.Z;
		}
	}
	return 0.f;
}

FLOAT ALandscapeGizmoActiveActor::GetLandscapeHeight(FLOAT NormalizedHeight)
{
	if (TargetLandscapeInfo && TargetLandscapeInfo->LandscapeProxy)
	{
		ALandscapeProxy* Proxy = TargetLandscapeInfo->LandscapeProxy;
		NormalizedHeight = (NormalizedHeight - MinRelativeZ) * RelativeScaleZ;
		FLOAT ScaleZ = Proxy->DrawScale * Proxy->DrawScale3D.Z;
		return (GetWorldHeight(NormalizedHeight) / ScaleZ);
	}
	return 0.f;
}

void ALandscapeGizmoActiveActor::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

void ALandscapeGizmoActiveActor::CalcNormal()
{
	INT SquaredDataTex = DataTexSize * DataTexSize;
	if (SampledHeight.Num() == SquaredDataTex && SampleSizeX > 0 && SampleSizeY > 0 )
	{
		if (SampledNormal.Num() != SquaredDataTex)
		{
			SampledNormal.Empty(SquaredDataTex);
			SampledNormal.AddZeroed(SquaredDataTex);
		}
		for (INT Y = 0; Y < SampleSizeY-1; ++Y)
		{
			for (INT X = 0; X < SampleSizeX-1; ++X)
			{
				FVector Vert00 = SampledHeight(X + Y*DataTexSize);
				FVector Vert01 = SampledHeight(X + (Y+1)*DataTexSize);
				FVector Vert10 = SampledHeight(X+1 + Y*DataTexSize);
				FVector Vert11 = SampledHeight(X+1 + (Y+1)*DataTexSize);

				FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
				FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

				// contribute to the vertex normals.
				SampledNormal(X + Y*DataTexSize) += FaceNormal1;
				SampledNormal(X + (Y+1)*DataTexSize) += FaceNormal2;
				SampledNormal(X+1 + Y*DataTexSize) += FaceNormal1 + FaceNormal2;
				SampledNormal(X+1 + (Y+1)*DataTexSize) += FaceNormal1 + FaceNormal2;
			}
		}
		for (INT Y = 0; Y < SampleSizeY; ++Y)
		{
			for (INT X = 0; X < SampleSizeX; ++X)
			{
				SampledNormal(X + Y*DataTexSize) = SampledNormal(X + Y*DataTexSize).SafeNormal();
			}
		}
	}
}

void ALandscapeGizmoActiveActor::SampleData(INT SizeX, INT SizeY)
{
	if (TargetLandscapeInfo && GizmoTexture)
	{
		// Rasterize rendering Texture...
		INT TexSizeX = Min(ALandscapeGizmoActiveActor::DataTexSize, SizeX);
		INT TexSizeY = Min(ALandscapeGizmoActiveActor::DataTexSize, SizeY);
		SampleSizeX = TexSizeX;
		SampleSizeY = TexSizeY;

		// Update Data Texture...
		//DataTexture->SetFlags(RF_Transactional);
		//DataTexture->Modify();

		TextureScale = FVector2D( (FLOAT)SizeX / Max(ALandscapeGizmoActiveActor::DataTexSize, SizeX), (FLOAT)SizeY / Max(ALandscapeGizmoActiveActor::DataTexSize, SizeY));
		FTexture2DMipMap* MipMap = &GizmoTexture->Mips(0);
		BYTE* TexData = (BYTE*)GizmoTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
		for (INT Y = 0; Y < TexSizeY; ++Y)
		{
			for (INT X = 0; X < TexSizeX; ++X)
			{
				FLOAT TexX = X * SizeX / TexSizeX;
				FLOAT TexY = Y * SizeY / TexSizeY;
				INT LX = appFloor(TexX);
				INT LY = appFloor(TexY);

				FLOAT FracX = TexX - LX;
				FLOAT FracY = TexY - LY;

				FGizmoSelectData* Data00 = SelectedData.Find(ALandscape::MakeKey(LX, LY));
				FGizmoSelectData* Data10 = SelectedData.Find(ALandscape::MakeKey(LX+1, LY));
				FGizmoSelectData* Data01 = SelectedData.Find(ALandscape::MakeKey(LX, LY+1));
				FGizmoSelectData* Data11 = SelectedData.Find(ALandscape::MakeKey(LX+1, LY+1));

				// Invert Tex Data to show selected region more visible
				TexData[X + Y*GizmoTexture->SizeX] = 255 - Lerp(
					Lerp(Data00 ? Data00->Ratio : 0, Data10 ? Data10->Ratio : 0, FracX),
					Lerp(Data01 ? Data01->Ratio : 0, Data11 ? Data11->Ratio : 0, FracX),
					FracY
					) * 255;

				if (DataType & LGT_Height)
				{
					FLOAT NormalizedHeight = Lerp(
						Lerp(Data00 ? Data00->HeightData : 0, Data10 ? Data10->HeightData : 0, FracX),
						Lerp(Data01 ? Data01->HeightData : 0, Data11 ? Data11->HeightData : 0, FracX),
						FracY
						);

					SampledHeight(X + Y*GizmoTexture->SizeX) = FVector(LX, LY, NormalizedHeight);
				}
			}
		}

		if (DataType & LGT_Height)
		{
			CalcNormal();
		}

		FUpdateTextureRegion2D Region(0, 0, 0, 0, TexSizeX, TexSizeY);
		GizmoTexture->UpdateTextureRegions(0, 1, &Region, GizmoTexture->SizeX, sizeof(BYTE), TexData, FALSE);
		FlushRenderingCommands();
		GizmoTexture->Mips(0).Data.Unlock();
	}
}

void ALandscapeGizmoActiveActor::Import(INT VertsX, INT VertsY, WORD* HeightData, TArray<FName> ImportLayerNames, BYTE* LayerDataPointers[] )
{
	if (VertsX <= 0 || VertsY <= 0 || HeightData == NULL || TargetLandscapeInfo == NULL || TargetLandscapeInfo->LandscapeProxy == NULL || GizmoTexture == NULL || (ImportLayerNames.Num() && !LayerDataPointers) )
	{
		return;
	}

	GWarn->BeginSlowTask( TEXT("Importing Gizmo Data"), TRUE);

	ClearGizmoData();

	ALandscapeProxy* Proxy = TargetLandscapeInfo->LandscapeProxy;
	CachedScaleXY = Proxy->DrawScale3D.X * Proxy->DrawScale;
	CachedWidth = CachedScaleXY * VertsX; // (DrawScale * DrawScale3D.X);
	CachedHeight = CachedScaleXY * VertsY; // (DrawScale * DrawScale3D.Y);
	
	FLOAT CurrentWidth = GetWidth();
	FLOAT CurrentHeight = GetHeight();
	LengthZ = GetLength();
	DrawScale3D.Z = DrawScale = 1.f;
	DrawScale3D.X = CurrentWidth / CachedWidth;
	DrawScale3D.Y = CurrentHeight / CachedHeight;
	Width = CachedWidth;
	Height = CachedHeight;

	DataType |= LGT_Height;
	if (ImportLayerNames.Num())
	{
		DataType |= LGT_Weight;
	}

	for (INT Y = 0; Y < VertsY; ++Y)
	{
		for (INT X = 0; X < VertsX; ++X)
		{
			FGizmoSelectData Data;
			Data.Ratio = 1.f;
			Data.HeightData = (FLOAT)HeightData[X + Y*VertsX] / 65535.f; //GetNormalizedHeight(HeightData[X + Y*VertsX]);
			for (INT i = 0; i < ImportLayerNames.Num(); ++i)
			{
				Data.WeightDataMap.Set( ImportLayerNames(i), LayerDataPointers[i][X + Y*VertsX] );
			}
			SelectedData.Set(ALandscape::MakeKey(X, Y), Data);
		}
	}

	SampleData(VertsX, VertsY);

	for (TArray<FName>::TIterator It(ImportLayerNames); It; ++It )
	{
		LayerNames.AddItem(*It);
	}

	GWarn->EndSlowTask();
}

void ALandscapeGizmoActiveActor::Export(INT Index, TArray<FString>& Filenames)
{
	//guard around case where landscape has no layer structs
	if (Filenames.Num() == 0)
	{
		return;
	}

	UBOOL bExportOneTarget = (Filenames.Num() == 1);

	if (TargetLandscapeInfo)
	{
		INT MinX = MAXINT, MinY = MAXINT, MaxX = MININT, MaxY = MININT;
		for (TMap<QWORD, FGizmoSelectData>::TConstIterator It(SelectedData); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);
			if (MinX > X) MinX = X;
			if (MaxX < X) MaxX = X;
			if (MinY > Y) MinY = Y;
			if (MaxY < Y) MaxY = Y;
		}

		if (MinX != MAXINT)
		{
			GWarn->BeginSlowTask( TEXT("Exporting Gizmo Data"), TRUE);

			TArray<BYTE> HeightData;
			if (!bExportOneTarget || Index == -1)
			{
				HeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY)*sizeof(WORD));
			}
			WORD* pHeightData = (WORD*)&HeightData(0);

			TArray<TArray<BYTE> > WeightDatas;
			for( INT i=1;i<Filenames.Num();i++ )
			{
				TArray<BYTE> WeightData;
				if (!bExportOneTarget || Index == i-1)
				{
					WeightData.AddZeroed((1+MaxX-MinX)*(1+MaxY-MinY));
				}
				WeightDatas.AddItem(WeightData);
			}

			for (INT Y = MinY; Y <= MaxY; ++Y)
			{
				for (INT X = MinX; X <= MaxX; ++X)
				{
					const FGizmoSelectData* Data = SelectedData.Find(ALandscape::MakeKey(X, Y));
					if (Data)
					{
						INT Idx = (X-MinX) + Y *(1+MaxX-MinX);
						if (!bExportOneTarget || Index == -1)
						{
							pHeightData[Idx] = Clamp<WORD>(Data->HeightData * 65535.f, 0, 65535);
						}

						for( INT i=1;i<Filenames.Num();i++ )
						{
							if (!bExportOneTarget || Index == i-1)
							{
								TArray<BYTE>& WeightData = WeightDatas(i-1);
								WeightData(Idx) = Clamp<BYTE>(Data->WeightDataMap.FindRef(LayerNames(i-1)), 0, 255);
							}
						}
					}
				}
			}

			if (!bExportOneTarget || Index == -1)
			{
				appSaveArrayToFile(HeightData,*Filenames(0));
			}

			for( INT i=1;i<Filenames.Num();i++ )
			{
				if (!bExportOneTarget || Index == i-1)
				{
					appSaveArrayToFile(WeightDatas(i-1),*Filenames(bExportOneTarget ? 0 : i));
				}
			}

			GWarn->EndSlowTask();
		}
		else
		{
			appMsgf(AMT_OK, *LocalizeUnrealEd("LandscapeGizmoExport_Warning"));
		}
	}
}

void ALandscapeGizmoActiveActor::ExportToClipboard()
{
	if (TargetLandscapeInfo && DataType != LGT_None)
	{
		//GWarn->BeginSlowTask( TEXT("Exporting Gizmo Data From Clipboard"), TRUE);

		FString ClipboardString(TEXT("GizmoData="));

		ClipboardString += FString::Printf(TEXT(" Type=%d,TextureScaleX=%g,TextureScaleY=%g,SampleSizeX=%d,SampleSizeY=%d,CachedWidth=%g,CachedHeight=%g,CachedScaleXY=%g "), 
			DataType, TextureScale.X, TextureScale.Y, SampleSizeX, SampleSizeY, CachedWidth, CachedHeight, CachedScaleXY);

		for (INT Y = 0; Y < SampleSizeY; ++Y )
		{
			for (INT X = 0; X < SampleSizeX; ++X)
			{
				FVector& V = SampledHeight(X + Y * DataTexSize);
				ClipboardString += FString::Printf(TEXT("%d %d %d "), (INT)V.X, (INT)V.Y, *(INT*)(&V.Z) );
			}
		}

		ClipboardString += FString::Printf(TEXT("LayerNames= "));

		for (TArray<FName>::TConstIterator It(LayerNames); It; ++It )
		{
			ClipboardString += FString::Printf(TEXT("%s "), *(*It).ToString() );
		}

		ClipboardString += FString::Printf(TEXT("Region= "));

		for (TMap<QWORD, FGizmoSelectData>::TConstIterator It(SelectedData); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);
			const FGizmoSelectData& Data = It.Value();
			ClipboardString += FString::Printf(TEXT("%d %d %d %d %d "), X, Y, *(INT*)(&Data.Ratio), *(INT*)(&Data.HeightData), Data.WeightDataMap.Num());

			for (TMap<FName, FLOAT>::TConstIterator It2(Data.WeightDataMap); It2; ++It2)
			{
				ClipboardString += FString::Printf(TEXT("%s %d "), *It2.Key().ToString(), *(INT*)(&It2.Value()));
			}
		}

		appClipboardCopy(*ClipboardString);

		//GWarn->EndSlowTask();
	}
}

#define MAX_GIZMO_PROP_TEXT_LENGTH			1024*1024*8

void ALandscapeGizmoActiveActor::ImportFromClipboard()
{
	const FString ClipboardString = appClipboardPaste();
	const TCHAR* Str = *ClipboardString;
	
	if(ParseCommand(&Str,TEXT("GizmoData=")))
	{
		INT ClipBoardSize = ClipboardString.Len();
		if (ClipBoardSize > MAX_GIZMO_PROP_TEXT_LENGTH)
		{
			INT PopupResult = appMsgf( AMT_YesNo, LocalizeSecure(LocalizeUnrealEd("LandscapeGizmoImport_Warning"), ClipBoardSize >> 20 ) );
			switch( PopupResult )
			{
				case ART_No:
					return;
					break;
				case ART_Yes:
					break;
			}
		}

		GWarn->BeginSlowTask( TEXT("Importing Gizmo Data From Clipboard"), TRUE);

		ParseNext(&Str);
		INT ReadNum = appSSCANF( Str, TEXT("Type=%d,TextureScaleX=%g,TextureScaleY=%g,SampleSizeX=%d,SampleSizeY=%d,CachedWidth=%g,CachedHeight=%g,CachedScaleXY=%g "), 
			&DataType, &TextureScale.X, &TextureScale.Y, &SampleSizeX, &SampleSizeY, &CachedWidth, &CachedHeight, &CachedScaleXY );

		if (ReadNum > 0)
		{
			while (!appIsWhitespace(*Str))
			{
				Str++;
			}
			ParseNext(&Str);

			INT SquaredDataTex = DataTexSize * DataTexSize;
			if (SampledHeight.Num() != SquaredDataTex)
			{
				SampledHeight.Empty(SquaredDataTex);
				SampledHeight.AddZeroed(SquaredDataTex);
			}

			// For Sample Height...
			TCHAR* StopStr;
			for (INT Y = 0; Y < SampleSizeY; ++Y )
			{
				for (INT X = 0; X < SampleSizeX; ++X)
				{
					FVector& V = SampledHeight(X + Y * DataTexSize);
					V.X = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					V.Y = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					//V.Z = appAtof(Str);
					*((INT*)(&V.Z)) = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
				}
			}

			CalcNormal();

			TCHAR StrBuf[1024];
			if(ParseCommand(&Str,TEXT("LayerNames=")))
			{
				while( !ParseCommand(&Str,TEXT("Region=")) )
				{
					ParseNext(&Str);
					int i = 0;
					while (!appIsWhitespace(*Str))
					{
						StrBuf[i++] = *Str;
						Str++;
					}
					StrBuf[i] = 0;
					LayerNames.AddItem( FName(StrBuf) );
				}
			}

			//if(ParseCommand(&Str,TEXT("Region=")))
			{
				while (*Str)
				{
					ParseNext(&Str);
					INT X, Y, LayerNum;
					FGizmoSelectData Data;
					X = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					Y = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					*((INT*)(&Data.Ratio)) = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					*((INT*)(&Data.HeightData)) = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					LayerNum = appStrtoi(Str, &StopStr, 10);
					while (!appIsWhitespace(*Str))
					{
						Str++;
					}
					ParseNext(&Str);
					for (INT i = 0; i < LayerNum; ++i)
					{
						FName LayerName = FName(*ParseToken(Str, 0));
						ParseNext(&Str);
						FLOAT Weight;
						*((INT*)(&Weight)) = appStrtoi(Str, &StopStr, 10);
						while (!appIsWhitespace(*Str))
						{
							Str++;
						}
						ParseNext(&Str);
						Data.WeightDataMap.Set(LayerName, Weight);
					}
					SelectedData.Set(ALandscape::MakeKey(X, Y), Data);
				}
			}
		}

		GWarn->EndSlowTask();
	}
}

#endif