/*================================================================================
	LandscapeEdMode.cpp: Landscape editing mode
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
================================================================================*/

#include "UnrealEd.h"
#include "UnObjectTools.h"
#include "LandscapeEdMode.h"
#include "ScopedTransaction.h"
#include "EngineTerrainClasses.h"
#include "EngineFoliageClasses.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"
#include "LandscapeDataAccess.h"
#include "UnObjectTools.h"

#if _WINDOWS
#include "WinDrv.h"
#endif

#if WITH_MANAGED_CODE
#include "LandscapeEditWindowShared.h"
#endif

UBOOL ALandscapeProxy::GetSelectedComponents(TArray<UObject*>& SelectedObjects)
{
	if (GEditorModeTools().GetActiveMode(EM_Landscape) )
	{
		ULandscapeInfo* Info = GetLandscapeInfo(FALSE);
		if (Info && Info->SelectedComponents.Num())
		{
			for (TSet<ULandscapeComponent*>::TIterator It(Info->SelectedComponents); It; ++It)
			{
				SelectedObjects.AddItem(*It);
			}
			return TRUE;
		}
		else if (bIsProxy && GetLandscapeActor())
		{
			SelectedObjects.AddItem(GetLandscapeActor());
			return TRUE;
		}
	}
	return FALSE;
}

const INT ALandscapeGizmoActiveActor::DataTexSize = 128;

void ALandscape::SplitHeightmap(ULandscapeComponent* Comp, UBOOL bMoveToCurrentLevel /*= FALSE*/)
{
	ULandscapeInfo* Info = Comp->GetLandscapeInfo();
	INT ComponentSizeVerts = Comp->NumSubsections * (Comp->SubsectionSizeQuads+1);
	// make sure the heightmap UVs are powers of two.
	INT HeightmapSizeU = (1<<appCeilLogTwo( ComponentSizeVerts ));
	INT HeightmapSizeV = (1<<appCeilLogTwo( ComponentSizeVerts ));

	UTexture2D* HeightmapTexture;
	TArray<FColor*> HeightmapTextureMipData;
	// Scope for FLandscapeEditDataInterface
	{
		// Read old data and split
		FLandscapeEditDataInterface LandscapeEdit(Info);
		TArray<BYTE> HeightData;
		HeightData.AddZeroed((1+Comp->ComponentSizeQuads)*(1+Comp->ComponentSizeQuads)*sizeof(WORD));
		// Because of edge problem, normal would be just copy from old component data
		TArray<BYTE> NormalData;
		NormalData.AddZeroed((1+Comp->ComponentSizeQuads)*(1+Comp->ComponentSizeQuads)*sizeof(WORD));
		LandscapeEdit.GetHeightDataFast(Comp->SectionBaseX, Comp->SectionBaseY, Comp->SectionBaseX + Comp->ComponentSizeQuads, Comp->SectionBaseY + Comp->ComponentSizeQuads, (WORD*)&HeightData(0), 0, (WORD*)&NormalData(0));

		// Construct the heightmap textures
		UObject* Outer = bMoveToCurrentLevel ? GWorld->CurrentLevel->GetOutermost() : Comp->GetOutermost();
		HeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), Outer, NAME_None, RF_Public);
		HeightmapTexture->Init(HeightmapSizeU, HeightmapSizeV, PF_A8R8G8B8);
		HeightmapTexture->SRGB = FALSE;
		HeightmapTexture->CompressionNone = TRUE;
		HeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
		HeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Heightmap;
		HeightmapTexture->AddressX = TA_Clamp;
		HeightmapTexture->AddressY = TA_Clamp;

		INT MipSubsectionSizeQuads = Comp->SubsectionSizeQuads;
		INT MipSizeU = HeightmapSizeU;
		INT MipSizeV = HeightmapSizeV;
		while( MipSizeU > 1 && MipSizeV > 1 && MipSubsectionSizeQuads >= 1 )
		{
			FColor* HeightmapTextureData;
			if( HeightmapTextureMipData.Num() > 0 )	
			{
				// create subsequent mips
				FTexture2DMipMap* HeightMipMap = new(HeightmapTexture->Mips) FTexture2DMipMap;
				HeightMipMap->SizeX = MipSizeU;
				HeightMipMap->SizeY = MipSizeV;
				HeightMipMap->Data.Lock(LOCK_READ_WRITE);
				HeightmapTextureData = (FColor*)HeightMipMap->Data.Realloc(MipSizeU*MipSizeV*sizeof(FColor));
			}
			else
			{
				HeightmapTextureData = (FColor*)HeightmapTexture->Mips(0).Data.Lock(LOCK_READ_WRITE);
			}

			appMemzero( HeightmapTextureData, MipSizeU*MipSizeV*sizeof(FColor) );
			HeightmapTextureMipData.AddItem(HeightmapTextureData);

			MipSizeU >>= 1;
			MipSizeV >>= 1;

			MipSubsectionSizeQuads = ((MipSubsectionSizeQuads + 1) >> 1) - 1;
		}

		Comp->HeightmapScaleBias = FVector4( 1.f / (FLOAT)HeightmapSizeU, 1.f / (FLOAT)HeightmapSizeV, 0.f, 0.f);
		Comp->HeightmapTexture = HeightmapTexture;

		if (Comp->MaterialInstance)
		{
			Comp->MaterialInstance->SetTextureParameterValue(FName(TEXT("Heightmap")), HeightmapTexture);
		}

		for( INT i=0;i<HeightmapTextureMipData.Num();i++ )
		{
			HeightmapTexture->Mips(i).Data.Unlock();
		}
		LandscapeEdit.SetHeightData(Comp->SectionBaseX, Comp->SectionBaseY, Comp->SectionBaseX + Comp->ComponentSizeQuads, Comp->SectionBaseY + Comp->ComponentSizeQuads, (WORD*)&HeightData(0), 0, FALSE, (WORD*)&NormalData(0));
	}

	// End of LandscapeEdit interface
	HeightmapTexture->UpdateResource();
	// Reattach
	FComponentReattachContext ReattachContext(Comp);
}


//
// FLandscapeHeightCache
//
template<class Accessor, typename AccessorType>
struct TLandscapeEditCache
{
	TLandscapeEditCache(Accessor& InDataAccess)
	:	DataAccess(InDataAccess)
	,	Valid(FALSE)
	{
	}

	void CacheData( INT X1, INT Y1, INT X2, INT Y2 )
	{
		if( !Valid )
		{
			if (Accessor::bUseInterp)
			{
				ValidX1 = CachedX1 = X1;
				ValidY1 = CachedY1 = Y1;
				ValidX2 = CachedX2 = X2;
				ValidY2 = CachedY2 = Y2;

				DataAccess.GetData( ValidX1, ValidY1, ValidX2, ValidY2, CachedData );
				check(ValidX1 <= ValidX2 && ValidY1 <= ValidY2);
			}
			else
			{
				CachedX1 = X1;
				CachedY1 = Y1;
				CachedX2 = X2;
				CachedY2 = Y2;

				DataAccess.GetDataFast( CachedX1, CachedY1, CachedX2, CachedY2, CachedData );
			}

			OriginalData = CachedData;

			Valid = TRUE;
		}
		else
		{
			// Extend the cache area if needed
			if( X1 < CachedX1 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = X1;
					INT x2 = ValidX1;
					INT y1 = Min<INT>(Y1,CachedY1);
					INT y2 = Max<INT>(Y2,CachedY2);

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidX1 = Min<INT>(x1,ValidX1);
				}
				else
				{
					DataAccess.GetDataFast( X1, CachedY1, CachedX1-1, CachedY2, CachedData );
				}

				CacheOriginalData( X1, CachedY1, CachedX1-1, CachedY2 );
				CachedX1 = X1;
			}

			if( X2 > CachedX2 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = ValidX2;
					INT x2 = X2;
					INT y1 = Min<INT>(Y1,CachedY1);
					INT y2 = Max<INT>(Y2,CachedY2);

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidX2 = Max<INT>(x2,ValidX2);
				}
				else
				{
					DataAccess.GetDataFast( CachedX2+1, CachedY1, X2, CachedY2, CachedData );
				}
				CacheOriginalData( CachedX2+1, CachedY1, X2, CachedY2 );			
				CachedX2 = X2;
			}			

			if( Y1 < CachedY1 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = CachedX1;
					INT x2 = CachedX2;
					INT y1 = Y1;
					INT y2 = ValidY1;

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidY1 = Min<INT>(y1,ValidY1);
				}
				else
				{
					DataAccess.GetDataFast( CachedX1, Y1, CachedX2, CachedY1-1, CachedData );
				}
				CacheOriginalData( CachedX1, Y1, CachedX2, CachedY1-1 );			
				CachedY1 = Y1;
			}

			if( Y2 > CachedY2 )
			{
				if (Accessor::bUseInterp)
				{
					INT x1 = CachedX1;
					INT x2 = CachedX2;
					INT y1 = ValidY2;
					INT y2 = Y2;

					DataAccess.GetData( x1, y1, x2, y2, CachedData );
					ValidY2 = Max<INT>(y2,ValidY2);
				}
				else
				{
					DataAccess.GetDataFast( CachedX1, CachedY2+1, CachedX2, Y2, CachedData );
				}

				CacheOriginalData( CachedX1, CachedY2+1, CachedX2, Y2 );			
				CachedY2 = Y2;
			}
		}	
	}

	AccessorType* GetValueRef(INT LandscapeX, INT LandscapeY)
	{
		return CachedData.Find(ALandscape::MakeKey(LandscapeX,LandscapeY));
	}

	FLOAT GetValue(FLOAT LandscapeX, FLOAT LandscapeY)
	{
		INT X = appFloor(LandscapeX);
		INT Y = appFloor(LandscapeY);
		AccessorType* P00 = CachedData.Find(ALandscape::MakeKey(X, Y));
		AccessorType* P10 = CachedData.Find(ALandscape::MakeKey(X+1, Y));
		AccessorType* P01 = CachedData.Find(ALandscape::MakeKey(X, Y+1));
		AccessorType* P11 = CachedData.Find(ALandscape::MakeKey(X+1, Y+1));

		// Search for nearest value if missing data
		FLOAT V00 = P00 ? *P00 : (P10 ? *P10 : (P01 ? *P01 : (P11 ? *P11 : 0.f) ));
		FLOAT V10 = P10 ? *P10 : (P00 ? *P00 : (P11 ? *P11 : (P01 ? *P01 : 0.f) ));
		FLOAT V01 = P01 ? *P01 : (P00 ? *P00 : (P11 ? *P11 : (P10 ? *P10 : 0.f) ));
		FLOAT V11 = P11 ? *P11 : (P10 ? *P10 : (P01 ? *P01 : (P00 ? *P00 : 0.f) ));

		return Lerp(
			Lerp(V00, V10, LandscapeX - X),
			Lerp(V01, V11, LandscapeX - X),
			LandscapeY - Y);
	}

	FVector GetNormal(INT X, INT Y)
	{
		AccessorType* P00 = CachedData.Find(ALandscape::MakeKey(X, Y));
		AccessorType* P10 = CachedData.Find(ALandscape::MakeKey(X+1, Y));
		AccessorType* P01 = CachedData.Find(ALandscape::MakeKey(X, Y+1));
		AccessorType* P11 = CachedData.Find(ALandscape::MakeKey(X+1, Y+1));

		// Search for nearest value if missing data
		FLOAT V00 = P00 ? *P00 : (P10 ? *P10 : (P01 ? *P01 : (P11 ? *P11 : 0.f) ));
		FLOAT V10 = P10 ? *P10 : (P00 ? *P00 : (P11 ? *P11 : (P01 ? *P01 : 0.f) ));
		FLOAT V01 = P01 ? *P01 : (P00 ? *P00 : (P11 ? *P11 : (P10 ? *P10 : 0.f) ));
		FLOAT V11 = P11 ? *P11 : (P10 ? *P10 : (P01 ? *P01 : (P00 ? *P00 : 0.f) ));

		FVector Vert00 = FVector(0.f,0.f, V00);
		FVector Vert01 = FVector(0.f,1.f, V01);
		FVector Vert10 = FVector(1.f,0.f, V10);
		FVector Vert11 = FVector(1.f,1.f, V11);

		FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
		FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal();
		return (FaceNormal1 + FaceNormal2).SafeNormal();
	}

	void SetValue(INT LandscapeX, INT LandscapeY, AccessorType Value)
	{
		CachedData.Set(ALandscape::MakeKey(LandscapeX,LandscapeY), Value);
	}

	void GetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<AccessorType>& OutData)
	{
		INT NumSamples = (1+X2-X1)*(1+Y2-Y1);
		OutData.Empty(NumSamples);
		OutData.Add(NumSamples);

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				AccessorType* Ptr = GetValueRef(X,Y);
				if( Ptr )
				{
					OutData((X-X1) + (Y-Y1)*(1+X2-X1)) = *Ptr;
				}
			}
		}
	}

	void SetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<AccessorType>& Data)
	{
		// Update cache
		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				SetValue( X, Y, Data((X-X1) + (Y-Y1)*(1+X2-X1)) );
			}
		}

		// Update real data
		DataAccess.SetData( X1, Y1, X2, Y2, &Data(0) );
	}

	// Get the original data before we made any changes with the SetCachedData interface.
	void GetOriginalData(INT X1, INT Y1, INT X2, INT Y2, TArray<AccessorType>& OutOriginalData)
	{
		INT NumSamples = (1+X2-X1)*(1+Y2-Y1);
		OutOriginalData.Empty(NumSamples);
		OutOriginalData.Add(NumSamples);

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				AccessorType* Ptr = OriginalData.Find(ALandscape::MakeKey(X,Y));
				if( Ptr )
				{
					OutOriginalData((X-X1) + (Y-Y1)*(1+X2-X1)) = *Ptr;
				}
			}
		}
	}

	void Flush()
	{
		DataAccess.Flush();
	}

protected:
	Accessor& DataAccess;
private:
	void CacheOriginalData(INT X1, INT Y1, INT X2, INT Y2 )
	{
		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				QWORD Key = ALandscape::MakeKey(X,Y);
				AccessorType* Ptr = CachedData.Find(Key);
				if( Ptr )
				{
					check( OriginalData.Find(Key) == NULL );
					OriginalData.Set(Key, *Ptr);
				}
			}
		}
	}
	
	TMap<QWORD, AccessorType> CachedData;
	TMap<QWORD, AccessorType> OriginalData;

	UBOOL Valid;

	INT CachedX1;
	INT CachedY1;
	INT CachedX2;
	INT CachedY2;

	// To store valid region....
	INT ValidX1, ValidX2, ValidY1, ValidY2;
};

//
// FHeightmapAccessor
//
template<UBOOL bInUseInterp>
struct FHeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FHeightmapAccessor( ULandscapeInfo* InLandscapeInfo )
	{
		LandscapeInfo = InLandscapeInfo;
		LandscapeEdit = new FLandscapeEditDataInterface(InLandscapeInfo);
	}

	// accessors
	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, WORD>& Data)
	{
		LandscapeEdit->GetHeightData( X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, WORD>& Data)
	{
		LandscapeEdit->GetHeightDataFast( X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const WORD* Data )
	{
		TSet<ULandscapeComponent*> Components;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy && LandscapeEdit->GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			// Update data
			ChangedComponents.Add(Components);

			// Notify foliage to move any attached instances
			AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActor(FALSE);
			if( IFA )
			{
				// Calculate landscape local-space bounding box of old data, to look for foliage instances.
				TArray<ULandscapeHeightfieldCollisionComponent*> CollisionComponents;
				CollisionComponents.Empty(Components.Num());
				TArray<FBox> PreUpdateLocalBoxes;
				PreUpdateLocalBoxes.Empty(Components.Num());

				INT Index=0;
				for(TSet<ULandscapeComponent*>::TConstIterator It(Components);It;++It)
				{
					CollisionComponents.AddItem( (*It)->GetCollisionComponent() );
					PreUpdateLocalBoxes.AddItem( FBox( FVector((FLOAT)X1, (FLOAT)Y1, (*It)->CachedLocalBox.Min.Z), FVector((FLOAT)X2, (FLOAT)Y2, (*It)->CachedLocalBox.Max.Z) ) );
					Index++;
				}

				// Update landscape.
				LandscapeEdit->SetHeightData( X1, Y1, X2, Y2, Data, 0, TRUE);

				// Snap foliage for each component.
				for( Index=0; Index<CollisionComponents.Num();Index++ )
				{
					IFA->SnapInstancesForLandscape( CollisionComponents(Index), PreUpdateLocalBoxes(Index).TransformBy(Proxy->LocalToWorld()).ExpandBy(1.f) );
				}
			}
			else
			{
				// No foliage, just update landscape.
				LandscapeEdit->SetHeightData( X1, Y1, X2, Y2, Data, 0, TRUE);
			}
		}
		else
		{
			ChangedComponents.Empty();
		}
	}

	void Flush()
	{
		LandscapeEdit->Flush();
	}

	virtual ~FHeightmapAccessor()
	{
		delete LandscapeEdit;
		LandscapeEdit = NULL;

		// Update the bounds for the components we edited
		for(TSet<ULandscapeComponent*>::TConstIterator It(ChangedComponents);It;++It)
		{
			(*It)->UpdateCachedBounds();
			(*It)->ConditionalUpdateTransform();
		}
	}

private:
	ULandscapeInfo* LandscapeInfo;
	FLandscapeEditDataInterface* LandscapeEdit;
	TSet<ULandscapeComponent*> ChangedComponents;
};

struct FLandscapeHeightCache : public TLandscapeEditCache<FHeightmapAccessor<TRUE>,WORD>
{
	typedef WORD DataType;
	static WORD ClampValue( INT Value ) { return Clamp(Value, 0, LandscapeDataAccess::MaxValue); }

	FHeightmapAccessor<TRUE> HeightmapAccessor;

	FLandscapeHeightCache(const FLandscapeToolTarget& InTarget)
	:	HeightmapAccessor(InTarget.LandscapeInfo)
	,	TLandscapeEditCache(HeightmapAccessor)
	{
	}
};

//
// FAlphamapAccessor
//
template<UBOOL bInUseInterp, UBOOL bInUseTotalNormalize>
struct FAlphamapAccessor
{
	enum { bUseInterp = bInUseInterp };
	enum { bUseTotalNormalize = bInUseTotalNormalize };
	FAlphamapAccessor( ULandscapeInfo* InLandscapeInfo, FName InLayerName )
		:	LandscapeEdit(InLandscapeInfo)
		,	LayerName(InLayerName)
		,	bBlendWeight(TRUE)
	{
		// should be no Layer change during FAlphamapAccessor lifetime...
		if (InLandscapeInfo && LayerName != NAME_None)
		{
			FLandscapeLayerStruct* Layer = InLandscapeInfo->LayerInfoMap.FindRef(LayerName);
			ULandscapeLayerInfoObject* LayerInfo = Layer ? Layer->LayerInfoObj : NULL;

			if (LayerName == ALandscape::DataWeightmapName)
			{
				bBlendWeight = FALSE;
			}
			else if (LayerInfo && LayerInfo->LayerName == LayerName)
			{
				bBlendWeight = !LayerInfo->bNoWeightBlend;
			}
		}
	}

	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, BYTE>& Data)
	{
		LandscapeEdit.GetWeightData(LayerName, X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, BYTE>& Data)
	{
		LandscapeEdit.GetWeightDataFast(LayerName, X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data )
	{
		TSet<ULandscapeComponent*> Components;
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2, &Components))
		{
			if (LayerName == ALandscape::DataWeightmapName)
			{
				// Update data
				ChangedComponents.Add(Components);
			}
			LandscapeEdit.SetAlphaData(LayerName, X1, Y1, X2, Y2, Data, 0, bBlendWeight, bUseTotalNormalize);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

	virtual ~FAlphamapAccessor()
	{
		// Update the bounds for the components we edited
		for(TSet<ULandscapeComponent*>::TConstIterator It(ChangedComponents);It;++It)
		{
			FComponentReattachContext ReattachContext(*It);
		}
	}

private:
	FLandscapeEditDataInterface LandscapeEdit;
	FName LayerName;
	BOOL bBlendWeight;
	TSet<ULandscapeComponent*> ChangedComponents;
};

struct FLandscapeAlphaCache : public TLandscapeEditCache<FAlphamapAccessor<TRUE, FALSE>,BYTE>
{
	typedef BYTE DataType;
	static BYTE ClampValue( INT Value ) { return Clamp(Value, 0, 255); }

	FAlphamapAccessor<TRUE, FALSE> AlphamapAccessor;

	FLandscapeAlphaCache(const FLandscapeToolTarget& InTarget)
		:	AlphamapAccessor(InTarget.LandscapeInfo, InTarget.LayerName)
		,	TLandscapeEditCache(AlphamapAccessor)
	{
	}
};

struct FLandscapeVisCache : public TLandscapeEditCache<FAlphamapAccessor<FALSE, FALSE>,BYTE>
{
	typedef BYTE DataType;
	static BYTE ClampValue( INT Value ) { return Clamp(Value, 0, 255); }

	FAlphamapAccessor<FALSE, FALSE> AlphamapAccessor;

	FLandscapeVisCache(const FLandscapeToolTarget& InTarget)
		:	AlphamapAccessor(InTarget.LandscapeInfo, ALandscape::DataWeightmapName)
		,	TLandscapeEditCache(AlphamapAccessor)
	{
	}
};

//
// FFullWeightmapAccessor
//
template<UBOOL bInUseInterp>
struct FFullWeightmapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FFullWeightmapAccessor( ULandscapeInfo* InLandscapeInfo)
		:	LandscapeEdit(InLandscapeInfo)
	{
	}
	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, TArray<BYTE>>& Data)
	{
		// Do not Support for interpolation....
		check(FALSE && TEXT("Do not support interpolation for FullWeightmapAccessor for now"));
	}

	void GetDataFast(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, TArray<BYTE>>& Data)
	{
		DirtyLayerNames.Empty();
		LandscapeEdit.GetWeightDataFast(NAME_None, X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data)
	{
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2))
		{
			LandscapeEdit.SetAlphaData(NAME_None, X1, Y1, X2, Y2, Data, 0, FALSE, FALSE, &DirtyLayerNames);
		}
		DirtyLayerNames.Empty();
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

	TSet<FName> DirtyLayerNames;
private:
	FLandscapeEditDataInterface LandscapeEdit;
};

struct FLandscapeFullWeightCache : public TLandscapeEditCache<FFullWeightmapAccessor<FALSE>,TArray<BYTE>>
{
	typedef TArray<BYTE> DataType;

	FFullWeightmapAccessor<FALSE> WeightmapAccessor;

	FLandscapeFullWeightCache(const FLandscapeToolTarget& InTarget)
		:	WeightmapAccessor(InTarget.LandscapeInfo)
		,	TLandscapeEditCache(WeightmapAccessor)
	{
	}

	// Only for all weight case... the accessor type should be TArray<BYTE>
	void GetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<BYTE>& OutData, INT ArraySize)
	{
		INT NumSamples = (1+X2-X1)*(1+Y2-Y1) * ArraySize;
		OutData.Empty(NumSamples);
		OutData.Add(NumSamples);

		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				TArray<BYTE>* Ptr = GetValueRef(X,Y);
				if( Ptr )
				{
					for (INT Z = 0; Z < ArraySize; Z++)
					{
						OutData(( (X-X1) + (Y-Y1)*(1+X2-X1)) * ArraySize + Z) = (*Ptr)(Z);
					}
				}
			}
		}
	}

	// Only for all weight case... the accessor type should be TArray<BYTE>
	void SetCachedData(INT X1, INT Y1, INT X2, INT Y2, TArray<BYTE>& Data, INT ArraySize)
	{
		// Update cache
		for( INT Y=Y1;Y<=Y2;Y++ )
		{
			for( INT X=X1;X<=X2;X++ )
			{
				TArray<BYTE> Value;
				Value.Empty(ArraySize);
				Value.Add(ArraySize);
				for ( INT Z=0; Z < ArraySize; Z++)
				{
					Value(Z) = Data( ((X-X1) + (Y-Y1)*(1+X2-X1)) * ArraySize + Z);
				}
				SetValue( X, Y, Value );
			}
		}

		// Update real data
		DataAccess.SetData( X1, Y1, X2, Y2, &Data(0) );
	}

	void AddDirtyLayerName(FName LayerName)
	{
		WeightmapAccessor.DirtyLayerNames.Add(LayerName);
	}
};

// 
// FDatamapAccessor
//
template<UBOOL bInUseInterp>
struct FDatamapAccessor
{
	enum { bUseInterp = bInUseInterp };
	FDatamapAccessor( ULandscapeInfo* InLandscapeInfo )
		:	LandscapeEdit(InLandscapeInfo)
	{
	}

	void GetData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, BYTE>& Data)
	{
		LandscapeEdit.GetSelectData(X1, Y1, X2, Y2, Data);
	}

	void GetDataFast(const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, BYTE>& Data)
	{
		LandscapeEdit.GetSelectData(X1, Y1, X2, Y2, Data);
	}

	void SetData(INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data )
	{
		if (LandscapeEdit.GetComponentsInRegion(X1, Y1, X2, Y2))
		{
			LandscapeEdit.SetSelectData(X1, Y1, X2, Y2, Data, 0);
		}
	}

	void Flush()
	{
		LandscapeEdit.Flush();
	}

private:
	FLandscapeEditDataInterface LandscapeEdit;
};

struct FLandscapeDataCache : public TLandscapeEditCache<FDatamapAccessor<FALSE>,BYTE>
{
	typedef BYTE DataType;
	static BYTE ClampValue( INT Value ) { return Clamp(Value, 0, 255); }

	FDatamapAccessor<FALSE> DataAccessor;

	FLandscapeDataCache(const FLandscapeToolTarget& InTarget)
		:	DataAccessor(InTarget.LandscapeInfo)
		,	TLandscapeEditCache(DataAccessor)
	{
	}
};

void FLandscapeTool::SetEditRenderType()
{
	GLandscapeEditRenderMode = ELandscapeEditRenderMode::None | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask);
}

// 
// FLandscapeToolPaintBase
//
template<class ToolTarget>
class FLandscapeToolPaintBase : public FLandscapeTool
{
private:
	FLOAT PrevHitX;
	FLOAT PrevHitY;
protected:
	FLOAT PaintDistance;

public:
	FLandscapeToolPaintBase(FEdModeLandscape* InEdMode)
	:	EdMode(InEdMode)
	,	bToolActive(FALSE)
	,	LandscapeInfo(NULL)
	,	Cache(NULL)
	{
	}

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return Target.TargetType == ToolTarget::TargetType;
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		PaintDistance = 0;
		PrevHitX = InHitX;
		PrevHitY = InHitY;

		Cache = new ToolTarget::CacheClass(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete Cache;
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			PaintDistance += appSqrt(Square(PrevHitX - HitX) + Square(PrevHitY - HitY));
			PrevHitX = HitX;
			PrevHitY = HitY;

			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient) = 0;

protected:
	class FEdModeLandscape* EdMode;
	UBOOL bToolActive;
	class ULandscapeInfo* LandscapeInfo;

	typename ToolTarget::CacheClass* Cache;
};

// 
// FLandscapeToolPaint
//
#define DATA_AT(Array, X, Y) ((Array)((X-X1) + (Y-Y1)*(1+X2-X1)))

template<class ToolTarget>
class FLandscapeToolPaint : public FLandscapeToolPaintBase<ToolTarget>
{
	TMap<QWORD, FLOAT> TotalInfluenceMap;	// amount of time and weight the brush has spent on each vertex.
public:
	FLandscapeToolPaint(class FEdModeLandscape* InEdMode)
	:	FLandscapeToolPaintBase(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Paint"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Paint"); };

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
		{
			return;
		}

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);

		// Invert when holding Shift
		UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);
		UBOOL bUseClayBrush = EdMode->UISettings.GetbUseClayBrush() && ToolTarget::TargetType == LET_Heightmap;
		UBOOL bUseWeightTargetValue = EdMode->UISettings.GetbUseWeightTargetValue() && ToolTarget::TargetType == LET_Weightmap;

		// The data we'll be writing to
		TArray<ToolTarget::CacheClass::DataType> Data;
		Cache->GetCachedData(X1,Y1,X2,Y2,Data);

		// The source data we use for editing. 
		// For heightmaps we use a cached snapshot, for weightmaps we use the live data.
		TArray<ToolTarget::CacheClass::DataType>* SourceDataArrayPtr = &Data;
		TArray<ToolTarget::CacheClass::DataType> OriginalData;

		if( ToolTarget::TargetType == LET_Heightmap )
		{
			// Heightmaps use the original data rather than the data edited during the stroke.
			Cache->GetOriginalData(X1,Y1,X2,Y2,OriginalData);
			SourceDataArrayPtr = &OriginalData;
		}
		else
		if( !bUseWeightTargetValue )
		{
			// When painting weights (and not using target value mode), we use a source value that tends more
			// to the current value as we paint over the same region multiple times.
			Cache->GetOriginalData(X1,Y1,X2,Y2,OriginalData);
			SourceDataArrayPtr = &OriginalData;

			for( INT Y=Y1;Y<Y2;Y++ )
			{
				for( INT X=X1;X<X2;X++ )
				{
					FLOAT VertexInfluence = TotalInfluenceMap.FindRef(ALandscape::MakeKey(X,Y));

					ToolTarget::CacheClass::DataType& CurrentValue = DATA_AT(Data,X,Y);
					ToolTarget::CacheClass::DataType& SourceValue = DATA_AT(OriginalData,X,Y);

					SourceValue = Lerp<ToolTarget::CacheClass::DataType>( SourceValue, CurrentValue, Min<FLOAT>(VertexInfluence * 0.05f, 1.f) );
				}
			}
		}
		
		FMatrix ToWorld = ToolTarget::ToWorldMatrix(LandscapeInfo);
		FMatrix FromWorld = ToolTarget::FromWorldMatrix(LandscapeInfo);

		// Adjust strength based on brush size and drawscale, so strength 1 = one hemisphere
		FLOAT AdjustedStrength = ToolTarget::StrengthMultiplier(LandscapeInfo, EdMode->UISettings.GetBrushRadius());
		ToolTarget::CacheClass::DataType DestValue = ToolTarget::CacheClass::ClampValue(255.f * EdMode->UISettings.GetWeightTargetValue());

		FPlane BrushPlane;
		TArray<FVector> Normals;

		if( bUseClayBrush )
		{
			// Calculate normals for brush verts in data space
			Normals.Empty(OriginalData.Num());
			Normals.AddZeroed(OriginalData.Num());

			for( INT Y=Y1;Y<Y2;Y++ )
			{
				for( INT X=X1;X<X2;X++ )
				{
					FVector Vert00 = ToWorld.TransformFVector( FVector((FLOAT)X+0.f,(FLOAT)Y+0.f,DATA_AT(OriginalData,X+0,Y+0)) );
					FVector Vert01 = ToWorld.TransformFVector( FVector((FLOAT)X+0.f,(FLOAT)Y+1.f,DATA_AT(OriginalData,X+0,Y+1)) );
					FVector Vert10 = ToWorld.TransformFVector( FVector((FLOAT)X+1.f,(FLOAT)Y+0.f,DATA_AT(OriginalData,X+1,Y+0)) );
					FVector Vert11 = ToWorld.TransformFVector( FVector((FLOAT)X+1.f,(FLOAT)Y+1.f,DATA_AT(OriginalData,X+1,Y+1)) );

					FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
					FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

					// contribute to the vertex normals.
					DATA_AT(Normals,X+1,Y+0) += FaceNormal1;
					DATA_AT(Normals,X+0,Y+1) += FaceNormal2;
					DATA_AT(Normals,X+0,Y+0) += FaceNormal1 + FaceNormal2;
					DATA_AT(Normals,X+1,Y+1) += FaceNormal1 + FaceNormal2;
				}
			}
			for( INT Y=Y1;Y<=Y2;Y++ )
			{
				for( INT X=X1;X<=X2;X++ )
				{
					DATA_AT(Normals,X,Y) = DATA_AT(Normals,X,Y).SafeNormal();
				}
			}
				
			// Find brush centroid location
			FVector AveragePoint(0.f,0.f,0.f);
			FVector AverageNormal(0.f,0.f,0.f);
			FLOAT TotalWeight = 0.f;
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);
				FLOAT Weight = It.Value();

				AveragePoint += FVector( (FLOAT)X * Weight, (FLOAT)Y * Weight, (FLOAT)DATA_AT(OriginalData,appFloor(X),appFloor(Y)) * Weight );

				FVector SampleNormal = DATA_AT(Normals,X,Y);
				AverageNormal += SampleNormal * Weight;

				TotalWeight += Weight;
			}

			if( TotalWeight > 0.f )
			{
				AveragePoint /= TotalWeight;
				AverageNormal = AverageNormal.SafeNormal();
			}

			// Convert to world space
			FVector AverageLocation = ToWorld.TransformFVector( AveragePoint );
			FVector StrengthVector = ToWorld.TransformNormal(FVector(0,0,EdMode->UISettings.GetToolStrength() * Pressure * AdjustedStrength));

			// Brush pushes out in the normal direction
			FVector OffsetVector = AverageNormal * StrengthVector.Z;
			if( bInvert )
			{
				OffsetVector *= -1;
			}

			// World space brush plane
			BrushPlane = FPlane( AverageLocation + OffsetVector, AverageNormal );
		}

		// Apply the brush	
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			// Update influence map
			FLOAT VertexInfluence = TotalInfluenceMap.FindRef(It.Key());
			TotalInfluenceMap.Set( It.Key(), VertexInfluence + It.Value() );

			FLOAT PaintAmount = It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * AdjustedStrength;
			ToolTarget::CacheClass::DataType& CurrentValue = DATA_AT(Data,X,Y);
			const ToolTarget::CacheClass::DataType& SourceValue = DATA_AT(*SourceDataArrayPtr,X,Y);

			if( bUseWeightTargetValue )
			{
				if( bInvert )
				{
					CurrentValue = Lerp( CurrentValue, DestValue, PaintAmount / AdjustedStrength );
				}
				else
				{
					CurrentValue = Lerp( CurrentValue, DestValue, PaintAmount / AdjustedStrength );
				}
			}
			else
			if( bUseClayBrush )
			{
				// Brush application starts from original world location at start of stroke
				FVector WorldLoc = ToWorld.TransformFVector(FVector(X,Y,SourceValue));
					
				// Calculate new location on the brush plane
				WorldLoc.Z = (BrushPlane.W - BrushPlane.X*WorldLoc.X - BrushPlane.Y*WorldLoc.Y) / BrushPlane.Z;

				// Painted amount lerps based on brush falloff.
				FLOAT PaintValue = Lerp<FLOAT>( (FLOAT)SourceValue, FromWorld.TransformFVector(WorldLoc).Z, It.Value() );

				if( bInvert )
				{
					CurrentValue = ToolTarget::CacheClass::ClampValue( Min<INT>(appRound(PaintValue), CurrentValue) );
				}
				else
				{
					CurrentValue = ToolTarget::CacheClass::ClampValue( Max<INT>(appRound(PaintValue), CurrentValue) );
				}
			}
			else
			{
				if( bInvert )
				{
					CurrentValue = ToolTarget::CacheClass::ClampValue( Min<INT>(SourceValue - appRound(PaintAmount), CurrentValue) );
				}
				else
				{
					CurrentValue = ToolTarget::CacheClass::ClampValue( Max<INT>(SourceValue + appRound(PaintAmount), CurrentValue) );
				}
			}
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,Data);
		Cache->Flush();
	}
#undef DATA_AT

	virtual void EndTool()
	{
		TotalInfluenceMap.Empty();
		FLandscapeToolPaintBase::EndTool();
	}

};

#if WITH_KISSFFT
#include "tools/kiss_fftnd.h" // Kiss FFT for Real component...
#endif

template<typename DataType>
inline void LowPassFilter(INT X1, INT Y1, INT X2, INT Y2, TMap<QWORD, FLOAT>& BrushInfo, TArray<DataType>& Data, const FLOAT DetailScale, const FLOAT ApplyRatio = 1.f)
{
#if WITH_KISSFFT
	// Low-pass filter
	INT FFTWidth = X2-X1-1;
	INT FFTHeight = Y2-Y1-1;

	const int NDims = 2;
	const INT Dims[NDims] = {FFTHeight-FFTHeight%2, FFTWidth-FFTWidth%2};
	kiss_fftnd_cfg stf = kiss_fftnd_alloc(Dims, NDims, 0, NULL, NULL),
					sti = kiss_fftnd_alloc(Dims, NDims, 1, NULL, NULL);

	kiss_fft_cpx *buf = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);
	kiss_fft_cpx *out = (kiss_fft_cpx *)KISS_FFT_MALLOC(sizeof(kiss_fft_cpx) * Dims[0] * Dims[1]);

	for (int X = X1+1; X <= X2-1-FFTWidth%2; X++)
	{
		for (int Y = Y1+1; Y <= Y2-1-FFTHeight%2; Y++)
		{
			buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].r = Data((X-X1) + (Y-Y1)*(1+X2-X1));
			buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].i = 0;
		}
	}

	// Forward FFT
	kiss_fftnd(stf, buf, out);

	INT CenterPos[2] = {Dims[0]>>1, Dims[1]>>1};
	for (int Y = 0; Y < Dims[0]; Y++)
	{
		FLOAT DistFromCenter = 0.f;
		for (int X = 0; X < Dims[1]; X++)
		{
			if (Y < CenterPos[0])
			{
				if (X < CenterPos[1])
				{
					// 1
					DistFromCenter = X*X + Y*Y;
				}
				else
				{
					// 2
					DistFromCenter = (X-Dims[1])*(X-Dims[1]) + Y*Y;
				}
			}
			else
			{
				if (X < CenterPos[1])
				{
					// 3
					DistFromCenter = X*X + (Y-Dims[0])*(Y-Dims[0]);
				}
				else
				{
					// 4
					DistFromCenter = (X-Dims[1])*(X-Dims[1]) + (Y-Dims[0])*(Y-Dims[0]);
				}
			}
			// High frequency removal
			FLOAT Ratio = 1.f - DetailScale;
			FLOAT Dist = Min<FLOAT>((Dims[0]*Ratio)*(Dims[0]*Ratio), (Dims[1]*Ratio)*(Dims[1]*Ratio));
			FLOAT Filter = 1.0 / (1.0 + DistFromCenter/Dist);
			out[X+Y*Dims[1]].r *= Filter;
			out[X+Y*Dims[1]].i *= Filter;
		}
	}

	// Inverse FFT
	kiss_fftnd(sti, out, buf);

	FLOAT Scale = Dims[0] * Dims[1];
	for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
	{
		INT X, Y;
		ALandscape::UnpackKey(It.Key(), X, Y);

		if (It.Value() > 0.f)
		{
			Data((X-X1) + (Y-Y1)*(1+X2-X1)) = Lerp((FLOAT)Data((X-X1) + (Y-Y1)*(1+X2-X1)), buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].r / Scale, It.Value() * ApplyRatio);
				//buf[(X-X1-1) + (Y-Y1-1)*(Dims[1])].r / Scale;
		}
	}

	// Free FFT allocation
	KISS_FFT_FREE(stf);
	KISS_FFT_FREE(sti);
	KISS_FFT_FREE(buf);
	KISS_FFT_FREE(out);
#endif
}


// 
// FLandscapeToolSmooth
//
template<class ToolTarget>
class FLandscapeToolSmooth : public FLandscapeToolPaintBase<ToolTarget>
{
public:
	FLandscapeToolSmooth(class FEdModeLandscape* InEdMode)
		:	FLandscapeToolPaintBase(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Smooth"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Smooth"); };

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		if (!LandscapeInfo) return;

		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
		{
			return;
		}

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);

		TArray<ToolTarget::CacheClass::DataType> Data;
		Cache->GetCachedData(X1,Y1,X2,Y2,Data);

		// Apply the brush
		if (EdMode->UISettings.GetbDetailSmooth())
		{
			LowPassFilter<ToolTarget::CacheClass::DataType>(X1, Y1, X2, Y2, BrushInfo, Data, EdMode->UISettings.GetDetailScale(), EdMode->UISettings.GetToolStrength() * Pressure);
		}
		else
		{
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					// 3x3 filter
					INT FilterValue = 0;
					for( INT y=Y-1;y<=Y+1;y++ )
					{
						for( INT x=X-1;x<=X+1;x++ )
						{
							FilterValue += Data((x-X1) + (y-Y1)*(1+X2-X1));
						}
					}
					FilterValue /= 9;

					INT HeightDataIndex = (X-X1) + (Y-Y1)*(1+X2-X1);
					Data(HeightDataIndex) = Lerp( Data(HeightDataIndex), (ToolTarget::CacheClass::DataType)FilterValue, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
				}	
			}
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,Data);
		Cache->Flush();
	}
};

//
// FLandscapeToolFlatten
//
template<class ToolTarget>
class FLandscapeToolFlatten : public FLandscapeToolPaintBase<ToolTarget>
{
	UBOOL bInitializedFlattenHeight;
	INT FlattenHeightX;
	INT FlattenHeightY;
	FLOAT FlattenX;
	FLOAT FlattenY;
	typename ToolTarget::CacheClass::DataType FlattenHeight;
	FVector FlattenNormal;
	FLOAT FlattenPlaneDist;

public:
	FLandscapeToolFlatten(class FEdModeLandscape* InEdMode)
	:	FLandscapeToolPaintBase(InEdMode)
	,	bInitializedFlattenHeight(FALSE)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Flatten"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Flatten"); };

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bInitializedFlattenHeight = FALSE;
		FlattenX = InHitX;
		FlattenHeightX = appFloor(FlattenX);
		FlattenY = InHitY;
		FlattenHeightY = appFloor(FlattenY);
		return FLandscapeToolPaintBase::BeginTool(ViewportClient, InTarget, InHitX, InHitY);
	}

	virtual void EndTool()
	{
		bInitializedFlattenHeight = FALSE;
		FLandscapeToolPaintBase::EndTool();
	}

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		if (!LandscapeInfo) return;

		if( !bInitializedFlattenHeight || EdMode->UISettings.GetbPickValuePerApply())
		{
			Cache->CacheData(FlattenHeightX,FlattenHeightY,FlattenHeightX+1,FlattenHeightY+1);
			//ToolTarget::CacheClass::DataType* FlattenHeightPtr = Cache->GetValueRef(FlattenHeightX,FlattenHeightY);
			//check(FlattenHeightPtr);
			FLOAT HeightValue = Cache->GetValue(FlattenX, FlattenY);
			FlattenHeight = HeightValue;
			//FlattenHeight = *FlattenHeightPtr;

			if (EdMode->UISettings.GetbUseSlopeFlatten())
			{
				FlattenNormal = Cache->GetNormal(FlattenHeightX, FlattenHeightY);
				FlattenPlaneDist = -(FlattenNormal | FVector(FlattenX, FlattenY, HeightValue) );
			}

			bInitializedFlattenHeight = TRUE;
		}


		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
		{
			return;
		}

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);

		TArray<ToolTarget::CacheClass::DataType> HeightData;
		Cache->GetCachedData(X1,Y1,X2,Y2,HeightData);

		// For Add or Sub Flatten Mode
		// Apply Ratio...
		TMap<INT, FLOAT> RatioInfo;
		INT MaxDelta = INT_MIN;
		INT MinDelta = INT_MAX;

		// Apply the brush
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				INT HeightDataIndex = (X-X1) + (Y-Y1)*(1+X2-X1);

				// Conserve stiff
				if (!EdMode->UISettings.GetbUseSlopeFlatten())
				{
					INT Delta = HeightData(HeightDataIndex) - FlattenHeight;
					switch(EdMode->UISettings.GetFlattenMode())
					{
					case ELandscapeToolNoiseMode::Add:
						if (Delta < 0)
						{
							MinDelta = Min<INT>(Delta, MinDelta);
							RatioInfo.Set(HeightDataIndex, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * Delta);
						}
						break;
					case ELandscapeToolNoiseMode::Sub:
						if (Delta > 0)
						{
							MaxDelta = Max<INT>(Delta, MaxDelta);
							RatioInfo.Set(HeightDataIndex, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * Delta);
						}
						break;
					default:
					case ELandscapeToolNoiseMode::Both:
						HeightData(HeightDataIndex) = Lerp( HeightData(HeightDataIndex), FlattenHeight, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
						break;
					}
				}
				else
				{
					ToolTarget::CacheClass::DataType DestValue = -( FlattenNormal.X * X + FlattenNormal.Y * Y + FlattenPlaneDist ) / FlattenNormal.Z;
					//FLOAT PlaneDist = FlattenNormal | FVector(X, Y, HeightData(HeightDataIndex)) + FlattenPlaneDist;
					FLOAT PlaneDist = HeightData(HeightDataIndex) - DestValue;
					DestValue = HeightData(HeightDataIndex) - PlaneDist*It.Value()*EdMode->UISettings.GetToolStrength() * Pressure;
					switch(EdMode->UISettings.GetFlattenMode())
					{
					case ELandscapeToolNoiseMode::Add:
						if (PlaneDist < 0)
						{
							HeightData(HeightDataIndex) = Lerp( HeightData(HeightDataIndex), DestValue, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
						}
						break;
					case ELandscapeToolNoiseMode::Sub:
						if (PlaneDist > 0)
						{
							HeightData(HeightDataIndex) = Lerp( HeightData(HeightDataIndex), DestValue, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
						}
						break;
					default:
					case ELandscapeToolNoiseMode::Both:
						HeightData(HeightDataIndex) = Lerp( HeightData(HeightDataIndex), DestValue, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure );
						break;
					}
				}
			}
		}

		if (!EdMode->UISettings.GetbUseSlopeFlatten())
		{
			for( TMap<INT, FLOAT>::TIterator It(RatioInfo); It; ++It )
			{
				switch(EdMode->UISettings.GetFlattenMode())
				{
				case ELandscapeToolNoiseMode::Add:
					HeightData(It.Key()) = Lerp( HeightData(It.Key()), FlattenHeight, It.Value() / (FLOAT)MinDelta );
					break;
				case ELandscapeToolNoiseMode::Sub:
					HeightData(It.Key()) = Lerp( HeightData(It.Key()), FlattenHeight, It.Value() / (FLOAT)MaxDelta );
					break;
				default:
					break;
				}
			}
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,HeightData);
		Cache->Flush();
	}
};

//
// FLandscapeToolErosion
//
class FLandscapeToolErosion : public FLandscapeTool
{
public:
	FLandscapeToolErosion(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
		,	HeightCache(NULL)
		,	WeightCache(NULL)
		,	bWeightApplied(FALSE)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Erosion"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Erosion"); };

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // erosion applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		HeightCache = new FLandscapeHeightCache(InTarget);
		WeightCache = new FLandscapeFullWeightCache(InTarget);

		bWeightApplied = InTarget.TargetType != LET_Heightmap;

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete HeightCache;
		delete WeightCache;
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		if (!LandscapeInfo) return;

		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
		{
			return;
		}

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		const INT NeighborNum = 4;
		const INT Iteration = EdMode->UISettings.GetErodeIterationNum();
		const INT Thickness = EdMode->UISettings.GetErodeSurfaceThickness();
		const INT LayerNum = LandscapeInfo->LayerInfoMap.Num();

		HeightCache->CacheData(X1,Y1,X2,Y2);
		TArray<WORD> HeightData;
		HeightCache->GetCachedData(X1,Y1,X2,Y2,HeightData);

		TArray<BYTE> WeightDatas; // Weight*Layers...
		WeightCache->CacheData(X1,Y1,X2,Y2);
		WeightCache->GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	

		// Invert when holding Shift
		UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

		// Apply the brush	
		WORD Thresh = EdMode->UISettings.GetErodeThresh();
		INT WeightMoveThresh = Min<INT>(Max<INT>(Thickness >> 2, Thresh), Thickness >> 1);

		DWORD SlopeTotal;
		WORD SlopeMax;
		FLOAT TotalHeightDiff;
		FLOAT TotalWeight;

		TArray<FLOAT> CenterWeight;
		CenterWeight.Empty(LayerNum);
		CenterWeight.Add(LayerNum);
		TArray<FLOAT> NeighborWeight;
		NeighborWeight.Empty(NeighborNum*LayerNum);
		NeighborWeight.Add(NeighborNum*LayerNum);

		UBOOL bHasChanged = FALSE;
		for (INT i = 0; i < Iteration; i++)
		{
			bHasChanged = FALSE;
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					INT Center = (X-X1) + (Y-Y1)*(1+X2-X1);
					INT Neighbor[NeighborNum] = {(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)};
/*
					INT Neighbor[NeighborNum] = {
													(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)
													,(X-1-X1) + (Y-1-Y1)*(1+X2-X1), (X+1-X1) + (Y+1-Y1)*(1+X2-X1), (X+1-X1) + (Y-1-Y1)*(1+X2-X1), (X-1-X1) + (Y+1-Y1)*(1+X2-X1)
												};
*/
					SlopeTotal = 0;
					SlopeMax = bInvert ? 0 : Thresh;

					for (INT Idx = 0; Idx < NeighborNum; Idx++)
					{
						if (HeightData(Center) > HeightData(Neighbor[Idx]))
						{
							WORD Slope = HeightData(Center) - HeightData(Neighbor[Idx]);
							if (bInvert ^ (Slope*It.Value() > Thresh))
							{
								SlopeTotal += Slope;
								if (SlopeMax < Slope)
								{
									SlopeMax = Slope;
								}
							}
						}
					}

					if (SlopeTotal > 0)
					{
						FLOAT Softness = 1.f;
						{
							TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
							for (INT Idx = 0; It && Idx < LayerNum; Idx++, ++It)
							{
								ULandscapeLayerInfoObject* LayerInfo = It.Value() ? It.Value()->LayerInfoObj : NULL;
								if (LayerInfo)
								{
									BYTE Weight = WeightDatas(Center*LayerNum + Idx);
									Softness -= (FLOAT)(Weight) / 255.f * LayerInfo->Hardness;
								}
							}
						}
						if (Softness > 0.f)
						{
							//Softness = Clamp<FLOAT>(Softness, 0.f, 1.f);
							TotalHeightDiff = 0;
							INT WeightTransfer = Min<INT>(WeightMoveThresh, (bInvert ? (Thresh - SlopeMax) : (SlopeMax - Thresh)));
							for (INT Idx = 0; Idx < NeighborNum; Idx++)
							{
								TotalWeight = 0.f;
								if (HeightData(Center) > HeightData(Neighbor[Idx]))
								{
									WORD Slope = HeightData(Center) - HeightData(Neighbor[Idx]);
									if (bInvert ^ (Slope > Thresh))
									{
										FLOAT WeightDiff = Softness * EdMode->UISettings.GetToolStrength() * Pressure * ((FLOAT)Slope / SlopeTotal) * It.Value();
										//WORD HeightDiff = (WORD)((SlopeMax - Thresh) * WeightDiff);
										FLOAT HeightDiff = ((bInvert ? (Thresh - SlopeMax) : (SlopeMax - Thresh)) * WeightDiff);
										HeightData(Neighbor[Idx]) += HeightDiff;
										TotalHeightDiff += HeightDiff;

										if (bWeightApplied)
										{
											for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
											{
												FLOAT CenterWeight = (FLOAT)(WeightDatas(Center*LayerNum + LayerIdx)) / 255.f;
												FLOAT Weight = (FLOAT)(WeightDatas(Neighbor[Idx]*LayerNum + LayerIdx)) / 255.f;
												NeighborWeight(Idx*LayerNum + LayerIdx) = Weight*(FLOAT)Thickness + CenterWeight*WeightDiff*WeightTransfer; // transferred + original...
												TotalWeight += NeighborWeight(Idx*LayerNum + LayerIdx);
											}
											// Need to normalize weight...
											for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
											{
												WeightDatas(Neighbor[Idx]*LayerNum + LayerIdx) = (BYTE)(255.f * NeighborWeight(Idx*LayerNum + LayerIdx) / TotalWeight);
											}
										}
									}
								}
							}

							HeightData(Center) -= TotalHeightDiff;

							if (bWeightApplied)
							{
								TotalWeight = 0.f;
								FLOAT WeightDiff = Softness * EdMode->UISettings.GetToolStrength() * Pressure * It.Value();

								for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
								{
									FLOAT Weight = (FLOAT)(WeightDatas(Center*LayerNum + LayerIdx)) / 255.f;
									CenterWeight(LayerIdx) = Weight*Thickness - Weight*WeightDiff*WeightTransfer;
									TotalWeight += CenterWeight(LayerIdx);
								}
								// Need to normalize weight...
								for (INT LayerIdx = 0; LayerIdx < LayerNum; LayerIdx++)
								{
									WeightDatas(Center*LayerNum + LayerIdx) = (BYTE)(255.f * CenterWeight(LayerIdx) / TotalWeight);
								}
							}

							bHasChanged = TRUE;
						} // if Softness > 0.f
					} // if SlopeTotal > 0
				}
			}
			if (!bHasChanged)
			{
				break;
			}
		}

		FLOAT BrushSizeAdjust = 1.0f;
		if (EdMode->UISettings.GetBrushRadius() < EdMode->UISettings.GetMaximumValueRadius())
		{
			BrushSizeAdjust = EdMode->UISettings.GetBrushRadius() / EdMode->UISettings.GetMaximumValueRadius();
		}

		// Make some noise...
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				FNoiseParameter NoiseParam(0, EdMode->UISettings.GetErosionNoiseScale(), It.Value() * Thresh * EdMode->UISettings.GetToolStrength() * BrushSizeAdjust);
				FLOAT PaintAmount = ELandscapeToolNoiseMode::Conversion(EdMode->UISettings.GetErosionNoiseMode(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
				HeightData((X-X1) + (Y-Y1)*(1+X2-X1)) = FLandscapeHeightCache::ClampValue(HeightData((X-X1) + (Y-Y1)*(1+X2-X1)) + PaintAmount);
			}
		}

		HeightCache->SetCachedData(X1,Y1,X2,Y2,HeightData);
		HeightCache->Flush();
		if (bWeightApplied)
		{
			WeightCache->SetCachedData(X1,Y1,X2,Y2,WeightDatas, LayerNum);
			WeightCache->Flush();
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	FLandscapeHeightCache* HeightCache;
	FLandscapeFullWeightCache* WeightCache;

	UBOOL bToolActive;
	UBOOL bWeightApplied;
};

//
// FLandscapeToolHydraErosion
//
class FLandscapeToolHydraErosion : public FLandscapeToolErosion
{
public:
	FLandscapeToolHydraErosion(class FEdModeLandscape* InEdMode)
		: FLandscapeToolErosion(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("HydraulicErosion"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_HydraErosion"); };

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		if (!LandscapeInfo) return;

		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
		{
			return;
		}

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		const INT NeighborNum = 8;
		const INT LayerNum = LandscapeInfo->LayerInfoMap.Num();

		const INT Iteration = EdMode->UISettings.GetHErodeIterationNum();
		const WORD RainAmount = EdMode->UISettings.GetRainAmount();
		const FLOAT DissolvingRatio = 0.07 * EdMode->UISettings.GetToolStrength() * Pressure;  //0.01;
		const FLOAT EvaporateRatio = 0.5;
		const FLOAT SedimentCapacity = 0.10 * EdMode->UISettings.GetSedimentCapacity(); //DissolvingRatio; //0.01;

		HeightCache->CacheData(X1,Y1,X2,Y2);
		TArray<WORD> HeightData;
		HeightCache->GetCachedData(X1,Y1,X2,Y2,HeightData);
/*
		TArray<BYTE> WeightDatas; // Weight*Layers...
		WeightCache->CacheData(X1,Y1,X2,Y2);
		WeightCache->GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	
*/
		// Invert when holding Shift
		UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

		// Apply the brush
		TArray<WORD> WaterData;
		WaterData.Empty((1+X2-X1)*(1+Y2-Y1));
		WaterData.AddZeroed((1+X2-X1)*(1+Y2-Y1));
		TArray<WORD> SedimentData;
		SedimentData.Empty((1+X2-X1)*(1+Y2-Y1));
		SedimentData.AddZeroed((1+X2-X1)*(1+Y2-Y1));

		UBOOL bWaterExist = TRUE;
		UINT TotalHeightDiff;
		UINT TotalAltitudeDiff;
		UINT AltitudeDiff[NeighborNum];
		UINT TotalWaterDiff;
		UINT WaterDiff[NeighborNum];
		UINT TotalSedimentDiff;
		FLOAT AverageAltitude;

		// It's raining men!
		// Only initial raining works better...
		FNoiseParameter NoiseParam(0, EdMode->UISettings.GetRainDistScale(), RainAmount);
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() >= 1.f)
			{
				FLOAT PaintAmount = ELandscapeToolNoiseMode::Conversion(EdMode->UISettings.GetRainDistMode(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
				if (PaintAmount > 0) // Raining only for positive region...
					WaterData((X-X1) + (Y-Y1)*(1+X2-X1)) += PaintAmount;
			}
		}

		for (INT i = 0; i < Iteration; i++)
		{
			bWaterExist = FALSE;
			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f)
				{
					INT Center = (X-X1) + (Y-Y1)*(1+X2-X1);
					//INT Neighbor[NeighborNum] = {(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)};

					INT Neighbor[NeighborNum] = {
						(X-1-X1) + (Y-Y1)*(1+X2-X1), (X+1-X1) + (Y-Y1)*(1+X2-X1), (X-X1) + (Y-1-Y1)*(1+X2-X1), (X-X1) + (Y+1-Y1)*(1+X2-X1)
						,(X-1-X1) + (Y-1-Y1)*(1+X2-X1), (X+1-X1) + (Y+1-Y1)*(1+X2-X1), (X+1-X1) + (Y-1-Y1)*(1+X2-X1), (X-1-X1) + (Y+1-Y1)*(1+X2-X1)
					};

					// Dissolving...				
					FLOAT DissolvedAmount = DissolvingRatio * WaterData(Center) * It.Value();
					if (DissolvedAmount > 0 && HeightData(Center) >= DissolvedAmount)
					{
						HeightData(Center) -= DissolvedAmount;
						SedimentData(Center) += DissolvedAmount;
					}

					TotalHeightDiff = 0;
					TotalAltitudeDiff = 0;
					TotalWaterDiff = 0;
					TotalSedimentDiff = 0;

					UINT Altitude = HeightData(Center)+WaterData(Center);
					AverageAltitude = 0;
					UINT LowerNeighbor = 0;
					for (INT Idx = 0; Idx < NeighborNum; Idx++)
					{
						UINT NeighborAltitude = HeightData(Neighbor[Idx])+WaterData(Neighbor[Idx]);
						if (Altitude > NeighborAltitude)
						{
							AltitudeDiff[Idx] = Altitude - NeighborAltitude;
							TotalAltitudeDiff += AltitudeDiff[Idx];
							LowerNeighbor++;
							AverageAltitude += NeighborAltitude;
							if (HeightData(Center) > HeightData(Neighbor[Idx]))
								TotalHeightDiff += HeightData(Center) - HeightData(Neighbor[Idx]);
						}
						else
						{
							AltitudeDiff[Idx] = 0;
						}
					}

					// Water Transfer
					if (LowerNeighbor > 0)
					{
						AverageAltitude /= (LowerNeighbor);
						// This is not mathematically correct, but makes good result
						if (TotalHeightDiff)
							AverageAltitude *= (1.f - 0.1 * EdMode->UISettings.GetToolStrength() * Pressure);
							//AverageAltitude -= 4000.f * EdMode->UISettings.GetToolStrength();

						UINT WaterTransfer = Min<UINT>(WaterData(Center), Altitude - (UINT)AverageAltitude) * It.Value();

						for (INT Idx = 0; Idx < NeighborNum; Idx++)
						{
							if (AltitudeDiff[Idx] > 0)
							{
								WaterDiff[Idx] = (UINT)(WaterTransfer * (FLOAT)AltitudeDiff[Idx] / TotalAltitudeDiff);
								WaterData(Neighbor[Idx]) += WaterDiff[Idx];
								TotalWaterDiff += WaterDiff[Idx];
								UINT SedimentDiff = (UINT)(SedimentData(Center) * (FLOAT)WaterDiff[Idx] / WaterData(Center));
								SedimentData(Neighbor[Idx]) += SedimentDiff;
								TotalSedimentDiff += SedimentDiff;
							}
						}

						WaterData(Center) -= TotalWaterDiff;
						SedimentData(Center) -= TotalSedimentDiff;
					}

					// evaporation
					if (WaterData(Center) > 0)
					{
						bWaterExist = TRUE;
						WaterData(Center) = (WORD)(WaterData(Center) * (1.f - EvaporateRatio));
						FLOAT SedimentCap = SedimentCapacity*WaterData(Center);
						FLOAT SedimentDiff = SedimentData(Center) - SedimentCap;
						if (SedimentDiff > 0)
						{
							SedimentData(Center) -= SedimentDiff;
							HeightData(Center) = Clamp<WORD>(HeightData(Center)+SedimentDiff, 0, 65535);
						}
					}
				}	
			}

			if (!bWaterExist) 
			{
				break;
			}
		}

		if (EdMode->UISettings.GetbHErosionDetailSmooth())
			//LowPassFilter<WORD>(X1, Y1, X2, Y2, BrushInfo, HeightData, EdMode->UISettings.GetHErosionDetailScale(), EdMode->UISettings.GetToolStrength() * Pressure);
			LowPassFilter<WORD>(X1, Y1, X2, Y2, BrushInfo, HeightData, EdMode->UISettings.GetHErosionDetailScale(), 1.0f);

		HeightCache->SetCachedData(X1,Y1,X2,Y2,HeightData);
		HeightCache->Flush();
		/*
		if (bWeightApplied)
		{
			WeightCache->SetCachedData(X1,Y1,X2,Y2,WeightDatas, LayerNum);
			WeightCache->Flush();
		}
		*/
	}
};

// 
// FLandscapeToolNoise
//
template<class ToolTarget>
class FLandscapeToolNoise : public FLandscapeToolPaintBase<ToolTarget>
{
public:
	FLandscapeToolNoise(class FEdModeLandscape* InEdMode)
		:	FLandscapeToolPaintBase(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Noise"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Noise"); };

	virtual void ApplyTool(FEditorLevelViewportClient* ViewportClient)
	{
		if (!LandscapeInfo) return;

		// Get list of verts to update
		TMap<QWORD, FLOAT> BrushInfo;
		INT X1, Y1, X2, Y2;
		if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
		{
			return;
		}

		// Tablet pressure
		FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

		// expand the area by one vertex in each direction to ensure normals are calculated correctly
		X1 -= 1;
		Y1 -= 1;
		X2 += 1;
		Y2 += 1;

		Cache->CacheData(X1,Y1,X2,Y2);
		TArray<ToolTarget::CacheClass::DataType> Data;
		Cache->GetCachedData(X1,Y1,X2,Y2,Data);

		FLOAT BrushSizeAdjust = 1.0f;
		if (ToolTarget::TargetType != LET_Weightmap && EdMode->UISettings.GetBrushRadius() < EdMode->UISettings.GetMaximumValueRadius())
		{
			BrushSizeAdjust = EdMode->UISettings.GetBrushRadius() / EdMode->UISettings.GetMaximumValueRadius();
		}

		UBOOL bUseWeightTargetValue = EdMode->UISettings.GetbUseWeightTargetValue() && ToolTarget::TargetType == LET_Weightmap;

		// Apply the brush
		for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
		{
			INT X, Y;
			ALandscape::UnpackKey(It.Key(), X, Y);

			if( It.Value() > 0.f )
			{
				FLOAT OriginalValue = Data((X-X1) + (Y-Y1)*(1+X2-X1));
				if (bUseWeightTargetValue)
				{
					FNoiseParameter NoiseParam(0, EdMode->UISettings.GetNoiseScale(), 255.f / 2.f);
					FLOAT DestValue = ELandscapeToolNoiseMode::Conversion(ELandscapeToolNoiseMode::Add, NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y)) * EdMode->UISettings.GetWeightTargetValue();
					switch (EdMode->UISettings.GetNoiseMode())
					{
					case ELandscapeToolNoiseMode::Add:
						if (OriginalValue >= DestValue)
						{
							continue;
						}
						break;
					case ELandscapeToolNoiseMode::Sub:
						DestValue += (1.f - EdMode->UISettings.GetWeightTargetValue()) * NoiseParam.NoiseAmount;
						if (OriginalValue <= DestValue)
						{
							continue;
						}
						break;
					}
					Data((X-X1) + (Y-Y1)*(1+X2-X1)) = ToolTarget::CacheClass::ClampValue( appRound(Lerp( OriginalValue, DestValue, It.Value() * EdMode->UISettings.GetToolStrength() * Pressure)) );
				}
				else
				{
					FLOAT TotalStrength = It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * ToolTarget::StrengthMultiplier(LandscapeInfo, EdMode->UISettings.GetBrushRadius());
					FNoiseParameter NoiseParam(0, EdMode->UISettings.GetNoiseScale(),  TotalStrength * BrushSizeAdjust);
					FLOAT PaintAmount = ELandscapeToolNoiseMode::Conversion(EdMode->UISettings.GetNoiseMode(), NoiseParam.NoiseAmount, NoiseParam.Sample(X, Y));
					Data((X-X1) + (Y-Y1)*(1+X2-X1)) = ToolTarget::CacheClass::ClampValue(OriginalValue + PaintAmount);
				}
			}	
		}

		Cache->SetCachedData(X1,Y1,X2,Y2,Data);
		Cache->Flush();
	}
};

// 
// FLandscapeToolSelect
//
class FLandscapeToolSelect : public FLandscapeTool
{
public:
	FLandscapeToolSelect(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
		,	PreviousBrushType(FLandscapeBrush::BT_Normal)
		,	Cache(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Selection"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Selection"); };
	virtual void SetEditRenderType() { GLandscapeEditRenderMode = ELandscapeEditRenderMode::SelectComponent | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual UBOOL GetMaskEnable()	{ return FALSE;	}

	virtual EToolType GetToolType() { return TT_Mask; }

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		Cache = new FLandscapeDataCache(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		PreviousBrushType = EdMode->CurrentBrush->GetBrushType();
		delete Cache;
		Cache = NULL;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		//ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		if( LandscapeInfo )
		{
			LandscapeInfo->Modify();
			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
			{
				return;
			}

			// Invert when holding Shift
			UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

			if ( EdMode->CurrentBrush->GetBrushType() == FLandscapeBrush::BT_Component )
			{
				// Todo hold selection... static?
				TSet<ULandscapeComponent*> NewComponents;
				LandscapeInfo->GetComponentsInRegion(X1+1,Y1+1,X2-1,Y2-1,NewComponents);

				TSet<ULandscapeComponent*> NewSelection;

				if (bInvert)
				{
					NewSelection = LandscapeInfo->SelectedComponents.Difference(NewComponents);
				}
				else
				{
					NewSelection = LandscapeInfo->SelectedComponents.Union(NewComponents);
				}

				LandscapeInfo->UpdateSelectedComponents(NewSelection);
			}
			else // Select various shape regions
			{
				X1 -= 1;
				Y1 -= 1;
				X2 += 1;
				Y2 += 1;

				// Tablet pressure
				FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

				Cache->CacheData(X1,Y1,X2,Y2);
				TArray<BYTE> Data;
				Cache->GetCachedData(X1,Y1,X2,Y2,Data);

				TSet<ULandscapeComponent*> NewComponents;
				// Remove invalid regions
				LandscapeInfo->GetComponentsInRegion(X1,Y1,X2,Y2,NewComponents);
				LandscapeInfo->UpdateSelectedComponents(NewComponents, FALSE);

				for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
				{
					INT X, Y;
					ALandscape::UnpackKey(It.Key(), X, Y);

					if( It.Value() > 0.f && LandscapeInfo->IsValidPosition(X, Y) )
					{
						FLOAT PaintValue = It.Value() * EdMode->UISettings.GetToolStrength() * Pressure;
						FLOAT Value = LandscapeInfo->SelectedRegion.FindRef(It.Key());
						if (bInvert)
						{
							if (Value - PaintValue > 0.f)
							{
								LandscapeInfo->SelectedRegion.Set(It.Key(), Max(Value - PaintValue, 0.f));
							}
							else
							{
								LandscapeInfo->SelectedRegion.Remove(It.Key());
							}

						}
						else
						{
							LandscapeInfo->SelectedRegion.Set(It.Key(), Min(Value + PaintValue, 1.0f));
						}

						Data((X-X1) + (Y-Y1)*(1+X2-X1)) = Clamp<INT>(appRound(LandscapeInfo->SelectedRegion.FindRef(It.Key()) * 255), 0, 255);
					}
				}

				Cache->SetCachedData(X1,Y1,X2,Y2,Data);
				Cache->Flush();
				EdMode->SetMaskEnable(LandscapeInfo->SelectedRegion.Num());
			}
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	FLandscapeDataCache* Cache;

	UBOOL bToolActive;
	FLandscapeBrush::EBrushType PreviousBrushType;
};

class FLandscapeToolMask : public FLandscapeToolSelect
{
public:
	FLandscapeToolMask(class FEdModeLandscape* InEdMode)
		: FLandscapeToolSelect(InEdMode)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Mask"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Mask"); };
	virtual void SetEditRenderType() { GLandscapeEditRenderMode = ELandscapeEditRenderMode::SelectRegion | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual UBOOL GetMaskEnable()	{ return TRUE;	}
};

// 
// FLandscapeToolVisibility
//
class FLandscapeToolVisibility : public FLandscapeTool
{
public:
	FLandscapeToolVisibility(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
		,	Cache(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Visibility"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Visibility"); };
	//virtual UBOOL GetMaskEnable()	{ return FALSE;	}

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		Cache = new FLandscapeVisCache(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		delete Cache;
		Cache = NULL;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		if( LandscapeInfo )
		{
			LandscapeInfo->Modify();
			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
			{
				return;
			}

			// Invert when holding Shift
			UBOOL bInvert = IsShiftDown(ViewportClient->Viewport);

			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;

			// Tablet pressure
			FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

			Cache->CacheData(X1,Y1,X2,Y2);
			TArray<BYTE> Data;
			Cache->GetCachedData(X1,Y1,X2,Y2,Data);

			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					//FLOAT PaintValue = It.Value() * EdMode->UISettings.GetToolStrength() * Pressure * 255;
					FLOAT Value = Data((X-X1) + (Y-Y1)*(1+X2-X1));
					if (bInvert)
					{
						//Value = Value - PaintValue;
						Value = 0;
					}
					else
					{
						//Value = Value + PaintValue;
						Value = 255;
					}

					Data((X-X1) + (Y-Y1)*(1+X2-X1)) = Clamp<INT>(appRound(Value), 0, 255);
				}
			}

			Cache->SetCachedData(X1,Y1,X2,Y2,Data);
			Cache->Flush();
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;
	FLandscapeVisCache* Cache;

	UBOOL bToolActive;
};

// 
// FLandscapeToolMoveToLevel
//
class FLandscapeToolMoveToLevel : public FLandscapeTool
{
public:
	FLandscapeToolMoveToLevel(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("MoveToLevel"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_MoveToLevel"); };
	virtual void SetEditRenderType() { GLandscapeEditRenderMode = ELandscapeEditRenderMode::SelectComponent | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }
	virtual UBOOL GetMaskEnable() { return FALSE; }

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}
		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscape* Landscape = LandscapeInfo ? Cast<ALandscape>(LandscapeInfo->LandscapeProxy) : NULL;

		if( Landscape )
		{
			Landscape->Modify();
			LandscapeInfo->Modify();

			TArray<UObject*> RenameObjects;
			FString MsgBoxList;

			// Check the Physical Material is same package with Landscape
			if (Landscape->DefaultPhysMaterial && Landscape->DefaultPhysMaterial->GetOutermost() == Landscape->GetOutermost())
			{
				//appMsgf(AMT_OK, *LocalizeUnrealEd("LandscapePhyMaterial_Warning"));
				RenameObjects.AddUniqueItem(Landscape->DefaultPhysMaterial);
				MsgBoxList += Landscape->DefaultPhysMaterial->GetPathName();
				MsgBoxList += FString::Printf(TEXT("\n"));
			}

			// Check the LayerInfoObjects are same package with Landscape
			for (int i = 0; i < Landscape->LayerInfoObjs.Num(); ++i)
			{
				ULandscapeLayerInfoObject* LayerInfo = Landscape->LayerInfoObjs(i).LayerInfoObj;
				if (LayerInfo && LayerInfo->GetOutermost() == Landscape->GetOutermost())
				{
					RenameObjects.AddUniqueItem(LayerInfo);
					MsgBoxList += LayerInfo->GetPathName();
					MsgBoxList += FString::Printf(TEXT("\n"));
				}
			}

			UBOOL bBrush = FALSE;
			if (!LandscapeInfo->SelectedComponents.Num())
			{
				// Get list of verts to update
				TMap<QWORD, FLOAT> BrushInfo;
				INT X1, Y1, X2, Y2;
				if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
				{
					return;
				}
				LandscapeInfo->GetComponentsInRegion(X1+1,Y1+1,X2-1,Y2-1, LandscapeInfo->SelectedComponents);
				for (TSet<ULandscapeComponent*>::TIterator It(LandscapeInfo->SelectedComponents); It; ++It)
				{
					if (*It)
					{
						ULandscapeHeightfieldCollisionComponent* Comp = LandscapeInfo->XYtoCollisionComponentMap.FindRef(ALandscape::MakeKey((*It)->SectionBaseX, (*It)->SectionBaseY));
						if (Comp)
						{
							LandscapeInfo->SelectedCollisionComponents.Add(Comp);
						}
					}
				}
				bBrush = TRUE;
			}

			if (LandscapeInfo->SelectedComponents.Num())
			{
				if (Landscape->GetLevel() != GWorld->PersistentLevel)
				{
					appMsgf(AMT_OK, *LocalizeUnrealEd("LandscapeMoveToStreamingLevel_Warning"));
					return;
				}

				UBOOL bIsAllCurrentLevel = TRUE;
				for (TSet<ULandscapeComponent*>::TIterator It(LandscapeInfo->SelectedComponents); It; ++It)
				{
					if ( (*It)->GetLandscapeProxy()->GetLevel() != GWorld->CurrentLevel )
					{
						bIsAllCurrentLevel = FALSE;
					}
				}

				if (bIsAllCurrentLevel)
				{
					// Need to fix double WM
					if (!bBrush)
					{
						// Remove Selection
						LandscapeInfo->ClearSelectedRegion(TRUE);
					}
					return;
				}

				for (TSet<ULandscapeComponent*>::TIterator It(LandscapeInfo->SelectedComponents); It; ++It)
				{
					UMaterialInterface* LandscapeMaterial = (*It)->GetLandscapeMaterial();
					if ( LandscapeMaterial && LandscapeMaterial->GetOutermost() == (*It)->GetOutermost() )
					{
						ULandscapeComponent* Comp = *It;
						RenameObjects.AddUniqueItem(LandscapeMaterial);
						MsgBoxList += Comp->GetName() + TEXT("'s ") + LandscapeMaterial->GetPathName();
						MsgBoxList += FString::Printf(TEXT("\n"));
						//It.RemoveCurrent();
					}
				}

				if (RenameObjects.Num())
				{
					if (appMsgf(AMT_OKCancel, LocalizeSecure(LocalizeUnrealEd("LandscapeMoveToStreamingLevel_SharedResources"), *MsgBoxList), TEXT("Move to Streaming Level")))
					{
						UBOOL bSucceed =  ObjectTools::RenameObjectsWithRefs( RenameObjects, FALSE, NULL );
						if (!bSucceed)
						{
							appMsgf(AMT_OK, *LocalizeUnrealEd("LandscapeMoveToStreamingLevel_RenameFailed"));
							return;
						}
					}
					else
					{
						return;
					}
				}

				GWarn->BeginSlowTask( TEXT("Moving Landscape components to current level"), TRUE);

				TSet<ALandscapeProxy*> SelectProxies;
				TSet<UTexture2D*> OldTextureSet;
				TSet<ULandscapeComponent*> TargetSelectedComponents;
				TArray<ULandscapeHeightfieldCollisionComponent*> TargetSelectedCollisionComponents;
				TSet<ULandscapeComponent*> HeightmapUpdateComponents;

				INT Progress = 0;
				LandscapeInfo->SortSelectedComponents();
				INT ComponentSizeVerts = Landscape->NumSubsections * (Landscape->SubsectionSizeQuads+1);
				INT NeedHeightmapSize = 1<<appCeilLogTwo( ComponentSizeVerts );

				for(TSet<ULandscapeComponent*>::TConstIterator It(LandscapeInfo->SelectedComponents);It;++It)
				{
					ULandscapeComponent* Comp = *It;
					SelectProxies.Add(Comp->GetLandscapeProxy());
					if (Comp->GetLandscapeProxy()->GetOuter() != GWorld->CurrentLevel)
					{
						TargetSelectedComponents.Add(Comp);
					}
				}

				for(TSet<ULandscapeHeightfieldCollisionComponent*>::TConstIterator It(LandscapeInfo->SelectedCollisionComponents);It;++It)
				{
					ULandscapeHeightfieldCollisionComponent* Comp = *It;
					SelectProxies.Add(Comp->GetLandscapeProxy());
					if (Comp->GetLandscapeProxy()->GetOuter() != GWorld->CurrentLevel)
					{
						TargetSelectedCollisionComponents.AddItem(Comp);
					}
				}

				INT TotalProgress = TargetSelectedComponents.Num() * TargetSelectedCollisionComponents.Num();

				//Landscape->Modify();

				// Check which ones are need for height map change
				for(TSet<ULandscapeComponent*>::TConstIterator It(TargetSelectedComponents);It;++It)
				{
					ULandscapeComponent* Comp = *It;
					Comp->Modify();				
					OldTextureSet.Add(Comp->HeightmapTexture);
				}

				// Need to split all the component which share Heightmap with selected components
				// Search neighbor only
				for(TSet<ULandscapeComponent*>::TConstIterator It(TargetSelectedComponents);It;++It)
				{
					ULandscapeComponent* Comp = *It;
					INT SearchX = Comp->HeightmapTexture->Mips(0).SizeX / NeedHeightmapSize;
					INT SearchY = Comp->HeightmapTexture->Mips(0).SizeY / NeedHeightmapSize;

					for (INT Y = 0; Y < SearchY; ++Y)
					{
						for (INT X = 0; X < SearchX; ++X)
						{
							// Search for four directions...
							for (INT Dir = 0; Dir < 4; ++Dir)
							{
								INT XDir = (Dir>>1) ? 1 : -1;
								INT YDir = (Dir%2) ? 1 : -1;
								ULandscapeComponent* Neighbor = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(Comp->SectionBaseX + XDir*X*Comp->ComponentSizeQuads, Comp->SectionBaseY + YDir*Y*Comp->ComponentSizeQuads));
								if (Neighbor && Neighbor->HeightmapTexture == Comp->HeightmapTexture && !HeightmapUpdateComponents.Contains(Neighbor))
								{
									Neighbor->Modify();
									if (!TargetSelectedComponents.Contains(Neighbor))
									{
										Neighbor->HeightmapScaleBias.X = -1.f; // just mark this component is for original level, not current level
									}
									HeightmapUpdateComponents.Add(Neighbor);
								}
							}
						}
					}
				}

				// Changing Heightmap format for selected components
				for(TSet<ULandscapeComponent*>::TConstIterator It(HeightmapUpdateComponents);It;++It)
				{
					ULandscapeComponent* Comp = *It;
					ALandscape::SplitHeightmap(Comp, (Comp->HeightmapScaleBias.X > 0.f));
				}

				// Delete if it is no referenced textures...
				for(TSet<UTexture2D*>::TIterator It(OldTextureSet);It;++It)
				{
					(*It)->SetFlags(RF_Transactional);
					(*It)->Modify();
					(*It)->MarkPackageDirty();
					(*It)->ClearFlags(RF_Standalone);
				}

				ALandscapeProxy* LandscapeProxy = NULL;
				// Find there is already a LandscapeProxy Actor
				for (FActorIterator It; It; ++It)
				{
					ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
					if (Proxy && Proxy->GetOuter() == GWorld->CurrentLevel )
					{
						if ((Proxy->bIsProxy && Proxy->IsValidLandscapeActor(Landscape)) || (Proxy == Landscape))
						{
							LandscapeProxy = Proxy;
							break;
						}
					}
				}

				if (!LandscapeProxy)
				{
					LandscapeProxy = Cast<ALandscapeProxy>( GWorld->SpawnActor( ALandscapeProxy::StaticClass() ) );
				}

				for(TSet<ALandscapeProxy*>::TIterator It(SelectProxies);It;++It)
				{
					(*It)->Modify();
				}

				LandscapeProxy->Modify();
				LandscapeProxy->MarkPackageDirty();
				if (LandscapeProxy->bIsProxy)
				{
					LandscapeProxy->UpdateLandscapeActor(Landscape);
				}

				// Change Weight maps...
				{
					FLandscapeEditDataInterface LandscapeEdit(LandscapeInfo);
					for(TSet<ULandscapeComponent*>::TConstIterator It(TargetSelectedComponents);It;++It)
					{
						ULandscapeComponent* Comp = *It;
						INT TotalNeededChannels = Comp->WeightmapLayerAllocations.Num();
						INT CurrentLayer = 0;
						TArray<UTexture2D*> NewWeightmapTextures;

						// Code from ULandscapeComponent::ReallocateWeightmaps
						// Move to other channels left
						while( TotalNeededChannels > 0 )
						{
							// debugf(TEXT("Still need %d channels"), TotalNeededChannels);

							UTexture2D* CurrentWeightmapTexture = NULL;
							FLandscapeWeightmapUsage* CurrentWeightmapUsage = NULL;

							if( TotalNeededChannels < 4 )
							{
								// debugf(TEXT("Looking for nearest"));

								// see if we can find a suitable existing weightmap texture with sufficient channels
								INT BestDistanceSquared = MAXINT;
								for( TMap<UTexture2D*,struct FLandscapeWeightmapUsage>::TIterator It(LandscapeProxy->WeightmapUsageMap); It; ++It )
								{
									FLandscapeWeightmapUsage* TryWeightmapUsage = &It.Value();
									if( TryWeightmapUsage->FreeChannelCount() >= TotalNeededChannels )
									{
										// See if this candidate is closer than any others we've found
										for( INT ChanIdx=0;ChanIdx<4;ChanIdx++ )
										{
											if( TryWeightmapUsage->ChannelUsage[ChanIdx] != NULL  )
											{
												INT TryDistanceSquared = Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - Comp->SectionBaseX) + Square(TryWeightmapUsage->ChannelUsage[ChanIdx]->SectionBaseX - Comp->SectionBaseX);
												if( TryDistanceSquared < BestDistanceSquared )
												{
													CurrentWeightmapTexture = It.Key();
													CurrentWeightmapUsage = TryWeightmapUsage;
													BestDistanceSquared = TryDistanceSquared;
												}
											}
										}
									}
								}
							}

							UBOOL NeedsUpdateResource=FALSE;
							// No suitable weightmap texture
							if( CurrentWeightmapTexture == NULL )
							{
								Comp->MarkPackageDirty();

								// Weightmap is sized the same as the component
								INT WeightmapSize = (Comp->SubsectionSizeQuads+1) * Comp->NumSubsections;

								// We need a new weightmap texture
								CurrentWeightmapTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), GWorld->CurrentLevel->GetOutermost(), NAME_None, RF_Public);
								CurrentWeightmapTexture->Init(WeightmapSize,WeightmapSize,PF_A8R8G8B8);
								CurrentWeightmapTexture->SRGB = FALSE;
								CurrentWeightmapTexture->CompressionNone = TRUE;
								CurrentWeightmapTexture->MipGenSettings = TMGS_LeaveExistingMips;
								CurrentWeightmapTexture->AddressX = TA_Clamp;
								CurrentWeightmapTexture->AddressY = TA_Clamp;
								CurrentWeightmapTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
								// Alloc dummy mips
								Comp->CreateEmptyTextureMips(CurrentWeightmapTexture);
								CurrentWeightmapTexture->UpdateResource();

								// Store it in the usage map
								CurrentWeightmapUsage = &LandscapeProxy->WeightmapUsageMap.Set(CurrentWeightmapTexture, FLandscapeWeightmapUsage());

								// debugf(TEXT("Making a new texture %s"), *CurrentWeightmapTexture->GetName());
							}

							NewWeightmapTextures.AddItem(CurrentWeightmapTexture);

							for( INT ChanIdx=0;ChanIdx<4 && TotalNeededChannels > 0;ChanIdx++ )
							{
								// debugf(TEXT("Finding allocation for layer %d"), CurrentLayer);

								if( CurrentWeightmapUsage->ChannelUsage[ChanIdx] == NULL  )
								{
									// Use this allocation
									FWeightmapLayerAllocationInfo& AllocInfo = Comp->WeightmapLayerAllocations(CurrentLayer);

									if( AllocInfo.WeightmapTextureIndex == 255 )
									{
										// New layer - zero out the data for this texture channel
										LandscapeEdit.ZeroTextureChannel( CurrentWeightmapTexture, ChanIdx );
									}
									else
									{
										UTexture2D* OldWeightmapTexture = Comp->WeightmapTextures(AllocInfo.WeightmapTextureIndex);

										// Copy the data
										LandscapeEdit.CopyTextureChannel( CurrentWeightmapTexture, ChanIdx, OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );
										LandscapeEdit.ZeroTextureChannel( OldWeightmapTexture, AllocInfo.WeightmapTextureChannel );

										// Remove the old allocation
										FLandscapeWeightmapUsage* OldWeightmapUsage = Comp->GetLandscapeProxy()->WeightmapUsageMap.Find(OldWeightmapTexture);
										OldWeightmapUsage->ChannelUsage[AllocInfo.WeightmapTextureChannel] = NULL;
									}

									// Assign the new allocation
									CurrentWeightmapUsage->ChannelUsage[ChanIdx] = Comp;
									AllocInfo.WeightmapTextureIndex = NewWeightmapTextures.Num()-1;
									AllocInfo.WeightmapTextureChannel = ChanIdx;
									CurrentLayer++;
									TotalNeededChannels--;
								}
							}
						}

						// Replace the weightmap textures
						Comp->WeightmapTextures = NewWeightmapTextures;

						// Update the mipmaps for the textures we edited
						for( INT Idx=0;Idx<Comp->WeightmapTextures.Num();Idx++)
						{
							UTexture2D* WeightmapTexture = Comp->WeightmapTextures(Idx);
							FLandscapeTextureDataInfo* WeightmapDataInfo = LandscapeEdit.GetTextureDataInfo(WeightmapTexture);

							INT NumMips = WeightmapTexture->Mips.Num();
							TArray<FColor*> WeightmapTextureMipData(NumMips);
							for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
							{
								WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo->GetMipData(MipIdx);
							}

							ULandscapeComponent::UpdateWeightmapMips(Comp->NumSubsections, Comp->SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, WeightmapDataInfo);
						}
					}
					// Need to Repacking all the Weight map (to make it packed well...)
					Landscape->RemoveInvalidWeightmaps();
				}


				// Move the components to the Proxy actor
				// This does not use the MoveSelectedActorsToCurrentLevel path as there is no support to only move certain components.
				for(TSet<ULandscapeComponent*>::TConstIterator It(TargetSelectedComponents);It;++It)
				{
					// Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
					ULandscapeComponent* Comp = *It;
					Comp->GetLandscapeProxy()->LandscapeComponents.RemoveItem(Comp);
					Comp->InvalidateLightingCache();
					Comp->Rename(NULL, LandscapeProxy);
					LandscapeProxy->LandscapeComponents.AddItem( Comp );
					Comp->UpdateMaterialInstances();	
					Comp->ConditionalDetach();

					GWarn->StatusUpdatef( Progress++, TotalProgress, *FString::Printf( TEXT("Moving Component : %s"), *Comp->GetName() ) );
				}

				for(TArray<ULandscapeHeightfieldCollisionComponent*>::TConstIterator It(TargetSelectedCollisionComponents);It;++It)
				{
					// Need to move or recreate all related data (Height map, Weight map, maybe collision components, allocation info)
					ULandscapeHeightfieldCollisionComponent* Comp = *It;

					// Move any foliage associated
					AInstancedFoliageActor* OldIFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel( Comp->GetLandscapeProxy()->GetLevel() );
					if (OldIFA)
					{
						OldIFA->MoveInstancesForComponentToCurrentLevel(Comp);
					}

					Comp->GetLandscapeProxy()->CollisionComponents.RemoveItem(*It);
					Comp->Rename(NULL, LandscapeProxy);
					LandscapeProxy->CollisionComponents.AddItem(*It);
					Comp->ConditionalDetach();

					GWarn->StatusUpdatef( Progress++, TotalProgress, *FString::Printf( TEXT("Moving Component : %s"), *Comp->GetName() ) );
				}

				GEditor->SelectNone(FALSE, TRUE);
				GEditor->SelectActor( LandscapeProxy, TRUE, NULL, FALSE, TRUE );
				const UBOOL bUseCurrentLevelGridVolume = FALSE;

				GEditor->SelectNone(FALSE, TRUE);

				// Update our new components
				LandscapeProxy->ConditionalUpdateComponents();
				// Init RB physics for editor collision
				LandscapeProxy->InitRBPhysEditor();

				for(TSet<ALandscapeProxy*>::TIterator It(SelectProxies);It;++It)
				{
					(*It)->ConditionalUpdateComponents();
					(*It)->InitRBPhysEditor();
				}

				Landscape->bLockLocation = (LandscapeInfo->XYtoComponentMap.Num() != Landscape->LandscapeComponents.Num());

				if (Landscape != LandscapeProxy)
				{
					for (int i = 0; i < Landscape->LayerInfoObjs.Num(); ++i )
					{
						LandscapeProxy->LayerInfoObjs.AddItem(FLandscapeLayerStruct(Landscape->LayerInfoObjs(i).LayerInfoObj, LandscapeProxy, NULL));
					}
				}

				GWarn->EndSlowTask();

				// Remove Selection
				LandscapeInfo->ClearSelectedRegion(TRUE);

				//EdMode->SetMaskEnable(Landscape->SelectedRegion.Num());
				GCallbackEvent->Send( CALLBACK_RefreshEditor_LevelBrowser );
			}
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	UBOOL bToolActive;
};

// 
// FLandscapeToolAddComponent
//
class FLandscapeToolAddComponent : public FLandscapeTool
{
public:
	FLandscapeToolAddComponent(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
		,	HeightCache(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("AddComponent"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_AddComponent"); };
	virtual UBOOL GetMaskEnable()	{ return FALSE;	}

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		HeightCache = new FLandscapeHeightCache(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete HeightCache;
		HeightCache = NULL;
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscapeProxy* Landscape = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if( Landscape && EdMode->LandscapeRenderAddCollision)
		{
			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
			{
				return;
			}

			// expand the area to get valid data from regions...
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;

			HeightCache->CacheData(X1,Y1,X2,Y2);
			TArray<WORD> Data;
			HeightCache->GetCachedData(X1,Y1,X2,Y2,Data);

			// Find component range for this block of data, non shared vertices
			INT ComponentIndexX1 = (X1+1 >= 0) ? (X1+1) / Landscape->ComponentSizeQuads : (X1+2) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexY1 = (Y1+1 >= 0) ? (Y1+1) / Landscape->ComponentSizeQuads : (Y1+2) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexX2 = (X2-2 >= 0) ? (X2-2) / Landscape->ComponentSizeQuads : (X2-1) / Landscape->ComponentSizeQuads - 1;
			INT ComponentIndexY2 = (Y2-2 >= 0) ? (Y2-2) / Landscape->ComponentSizeQuads : (Y2-1) / Landscape->ComponentSizeQuads - 1;

			TArray<ULandscapeComponent*> NewComponents;
			Landscape->Modify();
			LandscapeInfo->Modify();
			for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
			{
				for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
				{		
					ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*Landscape->ComponentSizeQuads,ComponentIndexY*Landscape->ComponentSizeQuads));
					if( !Component )
					{
						// Add New component...
						ULandscapeComponent* LandscapeComponent = ConstructObject<ULandscapeComponent>(ULandscapeComponent::StaticClass(), Landscape, NAME_None, RF_Transactional);
						Landscape->LandscapeComponents.AddItem(LandscapeComponent);
						NewComponents.AddItem(LandscapeComponent);
						LandscapeComponent->Init(
							ComponentIndexX*Landscape->ComponentSizeQuads,ComponentIndexY*Landscape->ComponentSizeQuads,
							Landscape->ComponentSizeQuads,
							Landscape->NumSubsections,
							Landscape->SubsectionSizeQuads
							);
						LandscapeComponent->SetupActor(TRUE);

						INT ComponentVerts = (Landscape->SubsectionSizeQuads+1) * Landscape->NumSubsections;
						// Update Weightmap Scale Bias
						LandscapeComponent->WeightmapScaleBias = FVector4( 1.f / (FLOAT)ComponentVerts , 1.f / (FLOAT)ComponentVerts, 0.5f / (FLOAT)ComponentVerts , 0.5f / (FLOAT)ComponentVerts );
						LandscapeComponent->WeightmapSubsectionOffset =  (FLOAT)(LandscapeComponent->SubsectionSizeQuads+1) / (FLOAT)ComponentVerts ;

						TArray<FColor> HeightData;
						HeightData.Empty( Square(ComponentVerts) );
						HeightData.AddZeroed( Square(ComponentVerts) );
						LandscapeComponent->InitHeightmapData(HeightData, TRUE);
						LandscapeComponent->UpdateMaterialInstances();
					}
				}
			}

			HeightCache->SetCachedData(X1,Y1,X2,Y2,Data);
			HeightCache->Flush();

			for ( INT Idx = 0; Idx < NewComponents.Num(); Idx++ )
			{
				QWORD Key = ALandscape::MakeKey(NewComponents(Idx)->SectionBaseX, NewComponents(Idx)->SectionBaseY);
				NewComponents(Idx)->UpdateComponent(GWorld->Scene, Landscape, Landscape->LocalToWorld());
				ULandscapeHeightfieldCollisionComponent* Comp = LandscapeInfo->XYtoCollisionComponentMap.FindRef( Key );
				if (Comp)
				{
					Comp->UpdateComponent(GWorld->Scene, Landscape, Landscape->LocalToWorld());
					Comp->RecreateHeightfield();
				}
			}
			EdMode->LandscapeRenderAddCollision = NULL;

			if (GCallbackEvent && NewComponents.Num())
			{
				// For Landscape List Update
				GCallbackEvent->Send( CALLBACK_MapChange );
			}
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	FLandscapeHeightCache* HeightCache;
	UBOOL bToolActive;
};

// 
// FLandscapeToolDeleteComponent
//
class FLandscapeToolDeleteComponent : public FLandscapeTool
{
public:
	FLandscapeToolDeleteComponent(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("DeleteComponent"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_DeleteComponent"); };
	virtual void SetEditRenderType() { GLandscapeEditRenderMode = ELandscapeEditRenderMode::SelectComponent | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask); }

	virtual UBOOL GetMaskEnable() { return FALSE; }

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return TRUE; // applied to all...
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		EdMode->CurrentBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
			}

			if( bToolActive )
			{
				// Apply tool
				ApplyTool(ViewportClient);
			}
		}
		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		ALandscapeProxy* Landscape = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if( Landscape )
		{
			Landscape->Modify();
			LandscapeInfo->Modify();

			if (!LandscapeInfo->SelectedComponents.Num())
			{
				// Get list of verts to update
				TMap<QWORD, FLOAT> BrushInfo;
				INT X1, Y1, X2, Y2;
				if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
				{
					return;
				}
				LandscapeInfo->GetComponentsInRegion(X1+1,Y1+1,X2-1,Y2-1, LandscapeInfo->SelectedComponents);
			}

			INT ComponentSizeVerts = Landscape->NumSubsections * (Landscape->SubsectionSizeQuads+1);
			INT NeedHeightmapSize = 1<<appCeilLogTwo( ComponentSizeVerts );

			TSet<ULandscapeComponent*> HeightmapUpdateComponents;
			// Need to split all the component which share Heightmap with selected components
			// Search neighbor only
			for(TSet<ULandscapeComponent*>::TConstIterator It(LandscapeInfo->SelectedComponents);It;++It)
			{
				ULandscapeComponent* Comp = *It;
				INT SearchX = Comp->HeightmapTexture->Mips(0).SizeX / NeedHeightmapSize;
				INT SearchY = Comp->HeightmapTexture->Mips(0).SizeY / NeedHeightmapSize;

				for (INT Y = 0; Y < SearchY; ++Y)
				{
					for (INT X = 0; X < SearchX; ++X)
					{
						// Search for four directions...
						for (INT Dir = 0; Dir < 4; ++Dir)
						{
							INT XDir = (Dir>>1) ? 1 : -1;
							INT YDir = (Dir%2) ? 1 : -1;
							ULandscapeComponent* Neighbor = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(Comp->SectionBaseX + XDir*X*Comp->ComponentSizeQuads, Comp->SectionBaseY + YDir*Y*Comp->ComponentSizeQuads));
							if (Neighbor && Neighbor->HeightmapTexture == Comp->HeightmapTexture && !HeightmapUpdateComponents.Contains(Neighbor))
							{
								Neighbor->Modify();
								HeightmapUpdateComponents.Add(Neighbor);
							}
						}
					}
				}
			}

			// Changing Heightmap format for selected components
			for(TSet<ULandscapeComponent*>::TConstIterator It(HeightmapUpdateComponents);It;++It)
			{
				ULandscapeComponent* Comp = *It;
				ALandscape::SplitHeightmap(Comp, FALSE);
			}

			TArray<QWORD> DeletedNeighborKeys;
			// Check which ones are need for height map change
			for(TSet<ULandscapeComponent*>::TIterator It(LandscapeInfo->SelectedComponents);It;++It)
			{
				ULandscapeComponent* Comp = *It;
				ALandscapeProxy* Proxy = Comp->GetLandscapeProxy();
				Proxy->Modify();
				//Comp->Modify();

				// Remove Selected Region in deleted Component
				for (INT Y = 0; Y < Comp->ComponentSizeQuads; ++Y )
				{
					for (INT X = 0; X < Comp->ComponentSizeQuads; ++X)
					{
						LandscapeInfo->SelectedRegion.Remove(ALandscape::MakeKey(X + Comp->SectionBaseX, Y + Comp->SectionBaseY));
					}
				}

				if (Comp->HeightmapTexture)
				{
					Comp->HeightmapTexture->SetFlags(RF_Transactional);
					Comp->HeightmapTexture->Modify();
					Comp->HeightmapTexture->MarkPackageDirty();
					Comp->HeightmapTexture->ClearFlags(RF_Standalone); // Remove when there is no reference for this Heightmap...
				}

				for (INT i = 0; i < Comp->WeightmapTextures.Num(); ++i)
				{
					Comp->WeightmapTextures(i)->SetFlags(RF_Transactional);
					Comp->WeightmapTextures(i)->Modify();
					Comp->WeightmapTextures(i)->MarkPackageDirty();
					Comp->WeightmapTextures(i)->ClearFlags(RF_Standalone);
				}

				QWORD Key = ALandscape::MakeKey(Comp->SectionBaseX, Comp->SectionBaseY);
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX-Comp->ComponentSizeQuads,	Comp->SectionBaseY-Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX,							Comp->SectionBaseY-Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX+Comp->ComponentSizeQuads,	Comp->SectionBaseY-Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX-Comp->ComponentSizeQuads,	Comp->SectionBaseY));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX+Comp->ComponentSizeQuads,	Comp->SectionBaseY));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX-Comp->ComponentSizeQuads,	Comp->SectionBaseY+Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX,							Comp->SectionBaseY+Comp->ComponentSizeQuads));
				DeletedNeighborKeys.AddUniqueItem(ALandscape::MakeKey(Comp->SectionBaseX+Comp->ComponentSizeQuads,	Comp->SectionBaseY+Comp->ComponentSizeQuads));

				Proxy->LandscapeComponents.RemoveItem(Comp);
				ULandscapeHeightfieldCollisionComponent* CollisionComp = LandscapeInfo->XYtoCollisionComponentMap.FindRef(Key);
				if (CollisionComp)
				{
					Proxy->CollisionComponents.RemoveItem(CollisionComp);
					LandscapeInfo->XYtoCollisionComponentMap.Remove(Key);
					CollisionComp->ConditionalDetach();
				}
				LandscapeInfo->XYtoComponentMap.Remove(Key);
				Comp->ConditionalDetach();
			}

			// Update AddCollisions...
			for (INT i = 0; i < DeletedNeighborKeys.Num(); ++i)
			{
				LandscapeInfo->XYtoAddCollisionMap.Remove(DeletedNeighborKeys(i));
			}

			for (INT i = 0; i < DeletedNeighborKeys.Num(); ++i)
			{
				ULandscapeHeightfieldCollisionComponent* CollisionComp = LandscapeInfo->XYtoCollisionComponentMap.FindRef(DeletedNeighborKeys(i));
				if (CollisionComp)
				{
					CollisionComp->UpdateAddCollisions();
				}
			}

			if (GCallbackEvent && DeletedNeighborKeys.Num())
			{
				// For Landscape List Update
				GCallbackEvent->Send( CALLBACK_MapChange );
			}

			// Remove Selection
			LandscapeInfo->ClearSelectedRegion(TRUE);
			//EdMode->SetMaskEnable(Landscape->SelectedRegion.Num());
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	UBOOL bToolActive;
};

// 
// FLandscapeToolCopy
//
template<class ToolTarget>
class FLandscapeToolCopy : public FLandscapeTool
{
public:
	FLandscapeToolCopy(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
		,	Cache(NULL)
		,	HeightCache(NULL)
		,	WeightCache(NULL)
	{}

	struct FGizmoPreData 
	{
		FLOAT Ratio;
		FLOAT Data;
	};

	virtual const TCHAR* GetIconString() { return TEXT("Copy"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Copy"); };
	virtual void SetEditRenderType() 
	{ 
		GLandscapeEditRenderMode = ELandscapeEditRenderMode::Gizmo | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask);
		GLandscapeEditRenderMode |= (EdMode && EdMode->CurrentToolTarget.LandscapeInfo && EdMode->CurrentToolTarget.LandscapeInfo->SelectedRegion.Num()) ? ELandscapeEditRenderMode::SelectRegion : ELandscapeEditRenderMode::SelectComponent;
	}

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return Target.TargetType == ToolTarget::TargetType;
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		//EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		EdMode->GizmoBrush->Tick(ViewportClient, 0.1f);
		EdMode->GizmoBrush->BeginStroke(InHitX, InHitY, this);

		Cache = new ToolTarget::CacheClass(InTarget);
		HeightCache = new FLandscapeHeightCache(InTarget);
		WeightCache = new FLandscapeFullWeightCache(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete Cache;
		delete HeightCache;
		delete WeightCache;
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		//EdMode->CurrentBrush->EndStroke();
		EdMode->GizmoBrush->EndStroke();
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		//ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		if (Proxy && Gizmo && Gizmo->GizmoTexture)
		{
			UTexture2D* DataTexture = Gizmo->GizmoTexture;
			Gizmo->TargetLandscapeInfo = LandscapeInfo;

			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			//EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2);
			if (!EdMode->GizmoBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
			{
				return;
			}

			//Gizmo->Modify(); // No transaction for Copied data as other tools...
			//Gizmo->SelectedData.Empty();
			Gizmo->ClearGizmoData();

			// Tablet pressure
			//FLOAT Pressure = ViewportClient->Viewport->IsPenActive() ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

			// expand the area by one vertex in each direction to ensure normals are calculated correctly
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;

			UBOOL bApplyToAll = EdMode->UISettings.GetbApplyToAllTargets();
			const INT LayerNum = LandscapeInfo->LayerInfoMap.Num();

			TArray<WORD> HeightData;
			TArray<BYTE> WeightDatas; // Weight*Layers...
			TArray<ToolTarget::CacheClass::DataType> Data;

			TSet<FName> LayerNameSet;

			if (bApplyToAll)
			{
				HeightCache->CacheData(X1,Y1,X2,Y2);
				HeightCache->GetCachedData(X1,Y1,X2,Y2,HeightData);

				WeightCache->CacheData(X1,Y1,X2,Y2);
				WeightCache->GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	
			}
			else
			{
				Cache->CacheData(X1,Y1,X2,Y2);
				Cache->GetCachedData(X1,Y1,X2,Y2,Data);
			}

			FLOAT ScaleXY = Proxy->DrawScale * Proxy->DrawScale3D.X;
			FLOAT Width = Gizmo->GetWidth();
			FLOAT Height = Gizmo->GetHeight();

			Gizmo->CachedWidth = Width;
			Gizmo->CachedHeight = Height;
			Gizmo->CachedScaleXY = ScaleXY;

			// Rasterize Gizmo regions
			INT SizeX = appCeil(Width  / ScaleXY);
			INT SizeY = appCeil(Height / ScaleXY);

			const FLOAT W = (Width - ScaleXY) / (2 * ScaleXY);
			const FLOAT H = (Height - ScaleXY) / (2 * ScaleXY);

			FMatrix WToL = Proxy->WorldToLocal();
			//FMatrix LToW = Landscape->LocalToWorld();

			FVector BaseLocation = WToL.TransformFVector(Gizmo->Location);
			FMatrix GizmoLocalToLandscape = FRotationTranslationMatrix(FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0));

			const INT NeighborNum = 4;
			UBOOL bDidCopy = FALSE;
			UBOOL bFullCopy = !EdMode->UISettings.GetbUseSelectedRegion() || !LandscapeInfo->SelectedRegion.Num();
			//UBOOL bInverted = EdMode->UISettings.GetbUseSelectedRegion() && EdMode->UISettings.GetbUseNegativeMask();

			for (INT Y = 0; Y < SizeY; ++Y)
			{
				for (INT X = 0; X < SizeX; ++X)
				{
					FVector LandscapeLocal = GizmoLocalToLandscape.TransformFVector(FVector( -W + X, -H + Y, 0 ));
					INT LX = appFloor(LandscapeLocal.X);
					INT LY = appFloor(LandscapeLocal.Y);

					{
						TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
						for (INT i = -1; (!bApplyToAll && i < 0) || i < LayerNum; ++i )
						{
							FGizmoPreData GizmoData[NeighborNum];

							for (INT LocalY = 0; LocalY < 2; ++LocalY)
							{
								for (INT LocalX = 0; LocalX < 2; ++LocalX)
								{
									INT x = Clamp(LX + LocalX, X1, X2);
									INT y = Clamp(LY + LocalY, Y1, Y2);
									GizmoData[LocalX + LocalY*2].Ratio = LandscapeInfo->SelectedRegion.FindRef(ALandscape::MakeKey(x, y));
									INT index = (x-X1) + (y-Y1)*(1+X2-X1);

									if (bApplyToAll)
									{
										if ( i < 0 )
										{
											GizmoData[LocalX + LocalY*2].Data = Gizmo->GetNormalizedHeight( HeightData(index) );
										}
										else
										{
											GizmoData[LocalX + LocalY*2].Data = WeightDatas(index*LayerNum + i );
										}
									}
									else
									{
										ToolTarget::CacheClass::DataType OriginalValue = Data(index);
										if ( EdMode->CurrentToolTarget.TargetType == LET_Heightmap )
										{
											GizmoData[LocalX + LocalY*2].Data = Gizmo->GetNormalizedHeight(OriginalValue);
										}
										else
										{
											GizmoData[LocalX + LocalY*2].Data = OriginalValue;
										}
									}
								}
							}

							FGizmoPreData LerpedData;
							FLOAT FracX = LandscapeLocal.X - LX;
							FLOAT FracY = LandscapeLocal.Y - LY;
							LerpedData.Ratio = bFullCopy ? 1.f : 
								Lerp(
								Lerp(GizmoData[0].Ratio, GizmoData[1].Ratio, FracX),
								Lerp(GizmoData[2].Ratio, GizmoData[3].Ratio, FracX),
								FracY
								);

							LerpedData.Data = Lerp(
								Lerp(GizmoData[0].Data, GizmoData[1].Data, FracX),
								Lerp(GizmoData[2].Data, GizmoData[3].Data, FracX),
								FracY
								);

							if (!bDidCopy && LerpedData.Ratio > 0.f)
							{
								bDidCopy = TRUE;
							}

							if (LerpedData.Data > 0.f)
							{
								// Added for LayerNames
								if (bApplyToAll)
								{
									if (i >= 0)
									{
										LayerNameSet.Add(It.Key());
									}
								}
								else
								{
									if (EdMode->CurrentToolTarget.TargetType == LET_Weightmap)
									{
										LayerNameSet.Add(EdMode->CurrentToolTarget.LayerName);
									}
								}

								FGizmoSelectData* Data = Gizmo->SelectedData.Find(ALandscape::MakeKey(X, Y));
								if (Data)
								{
									if (bApplyToAll)
									{
										if (i < 0)
										{
											Data->HeightData = LerpedData.Data;
										}
										else
										{
											Data->WeightDataMap.Set(It.Key(), LerpedData.Data);
										}
									}
									else
									{
										if (EdMode->CurrentToolTarget.TargetType == LET_Heightmap)
										{
											Data->HeightData = LerpedData.Data;
										}
										else
										{
											Data->WeightDataMap.Set(EdMode->CurrentToolTarget.LayerName, LerpedData.Data);
										}
									}
								}
								else
								{
									FGizmoSelectData NewData;
									NewData.Ratio = LerpedData.Ratio;
									if (bApplyToAll)
									{
										if (i < 0)
										{
											NewData.HeightData = LerpedData.Data;
										}
										else
										{
											NewData.WeightDataMap.Set(It.Key(), LerpedData.Data);
										}
									}
									else
									{
										if (EdMode->CurrentToolTarget.TargetType == LET_Heightmap)
										{
											NewData.HeightData = LerpedData.Data;
										}
										else
										{
											NewData.WeightDataMap.Set(EdMode->CurrentToolTarget.LayerName, LerpedData.Data);
										}
									}
									Gizmo->SelectedData.Set(ALandscape::MakeKey(X, Y), NewData);
								}
							}

							if (i >= 0)
							{
								++It;
							}
						}
					}
				}
			}

			if (bDidCopy)
			{
				if (!bApplyToAll)
				{
					if ( EdMode->CurrentToolTarget.TargetType == LET_Heightmap )
					{
						Gizmo->DataType |= LGT_Height;
					}
					else
					{
						Gizmo->DataType |= LGT_Weight;
					}
				}
				else
				{
					if ( LayerNum > 0 )
					{
						Gizmo->DataType |= LGT_Height;
						Gizmo->DataType |= LGT_Weight;
					}
					else
					{
						Gizmo->DataType |= LGT_Height;
					}
				}

				Gizmo->SampleData(SizeX, SizeY);

				// Update LayerNames
				for (TSet<FName>::TIterator It(LayerNameSet); It; ++It )
				{
					Gizmo->LayerNames.AddItem(*It);
				}
			}

			//// Clean up Ratio 0 regions... (That was for sampling...)
			//for ( TMap<QWORD, FGizmoSelectData>::TIterator It(Gizmo->SelectedData); It; ++It )
			//{
			//	FGizmoSelectData& Data = It.Value();
			//	if (Data.Ratio <= 0.f)
			//	{
			//		Gizmo->SelectedData.Remove(It.Key());
			//	}
			//}

			Gizmo->ExportToClipboard();
		}
	}

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	UBOOL bToolActive;
	typename ToolTarget::CacheClass* Cache;
	FLandscapeHeightCache* HeightCache;
	FLandscapeFullWeightCache* WeightCache;
};

// 
// FLandscapeToolPaste
//
template<class ToolTarget>
class FLandscapeToolPaste : public FLandscapeTool
{
public:
	FLandscapeToolPaste(class FEdModeLandscape* InEdMode)
		:EdMode(InEdMode)
		,	bToolActive(FALSE)
		,	LandscapeInfo(NULL)
		,	bUseGizmoRegion(FALSE)
		,	Cache(NULL)
		,	HeightCache(NULL)
		,	WeightCache(NULL)
	{}

	virtual const TCHAR* GetIconString() { return TEXT("Paste"); }
	virtual FString GetTooltipString() { return LocalizeUnrealEd("LandscapeMode_Region"); };
	virtual void SetEditRenderType() 
	{ 
		GLandscapeEditRenderMode = ELandscapeEditRenderMode::Gizmo | (GLandscapeEditRenderMode & ELandscapeEditRenderMode::BitMaskForMask);
		GLandscapeEditRenderMode |= (EdMode && EdMode->CurrentToolTarget.LandscapeInfo && EdMode->CurrentToolTarget.LandscapeInfo->SelectedRegion.Num()) ? ELandscapeEditRenderMode::SelectRegion : ELandscapeEditRenderMode::SelectComponent;
	}

	void SetGizmoMode(UBOOL InbUseGizmoRegion)
	{
		bUseGizmoRegion = InbUseGizmoRegion;
	}

	virtual UBOOL IsValidForTarget(const FLandscapeToolTarget& Target)
	{
		return Target.TargetType == ToolTarget::TargetType;
	}

	virtual UBOOL BeginTool( FEditorLevelViewportClient* ViewportClient, const FLandscapeToolTarget& InTarget, FLOAT InHitX, FLOAT InHitY )
	{
		bToolActive = TRUE;

		LandscapeInfo = InTarget.LandscapeInfo;
		EdMode->GizmoBrush->Tick(ViewportClient, 0.1f);
		if (bUseGizmoRegion)
		{
			EdMode->GizmoBrush->BeginStroke(InHitX, InHitY, this);
		}
		else
		{
			EdMode->CurrentBrush->BeginStroke(InHitX, InHitY, this);
		}

		Cache = new ToolTarget::CacheClass(InTarget);
		HeightCache = new FLandscapeHeightCache(InTarget);
		WeightCache = new FLandscapeFullWeightCache(InTarget);

		ApplyTool(ViewportClient);

		return TRUE;
	}

	virtual void EndTool()
	{
		delete Cache;
		delete HeightCache;
		delete WeightCache;
		LandscapeInfo = NULL;
		bToolActive = FALSE;
		if (bUseGizmoRegion)
		{
			EdMode->GizmoBrush->EndStroke();
		}
		else
		{
			EdMode->CurrentBrush->EndStroke();
		}
	}

	virtual UBOOL MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT x, INT y )
	{
		if (bUseGizmoRegion)
		{
			return TRUE;
		}
		FLOAT HitX, HitY;
		if( EdMode->LandscapeMouseTrace(ViewportClient, x, y, HitX, HitY)  )
		{
			if( EdMode->CurrentBrush )
			{
				// Move brush to current location
				EdMode->CurrentBrush->MouseMove(HitX, HitY);
				if( EdMode->CurrentBrush->GetBrushType() != FLandscapeBrush::BT_Gizmo && bToolActive )
				{
					// Apply tool
					ApplyTool(ViewportClient);
				}
			}
		}

		return TRUE;
	}	

	virtual UBOOL CapturedMouseMove( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, INT InMouseX, INT InMouseY )
	{
		return MouseMove(InViewportClient,InViewport,InMouseX,InMouseY);
	}

	virtual void ApplyTool( FEditorLevelViewportClient* ViewportClient )
	{
		//ULandscapeInfo* LandscapeInfo = EdMode->CurrentToolTarget.LandscapeInfo;
		ALandscapeGizmoActiveActor* Gizmo = EdMode->CurrentGizmoActor;
		ALandscapeProxy* Proxy = LandscapeInfo ? LandscapeInfo->LandscapeProxy : NULL;
		// Cache and copy in Gizmo's region...
		if (Proxy && Gizmo)
		{
			Gizmo->TargetLandscapeInfo = LandscapeInfo;
			FLOAT ScaleXY = Proxy->DrawScale * Proxy->DrawScale3D.X;

			//LandscapeInfo->Modify();

			// Get list of verts to update
			TMap<QWORD, FLOAT> BrushInfo;
			INT X1, Y1, X2, Y2;
			if (bUseGizmoRegion)
			{
				if (!EdMode->GizmoBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
				{
					return;
				}
			}
			else
			{
				if (!EdMode->CurrentBrush->ApplyBrush(BrushInfo, X1, Y1, X2, Y2))
				{
					return;
				}
			}

			// Tablet pressure
			FLOAT Pressure = (ViewportClient && ViewportClient->Viewport->IsPenActive()) ? ViewportClient->Viewport->GetTabletPressure() : 1.f;

			// expand the area by one vertex in each direction to ensure normals are calculated correctly
			X1 -= 1;
			Y1 -= 1;
			X2 += 1;
			Y2 += 1;

			UBOOL bApplyToAll = EdMode->UISettings.GetbApplyToAllTargets();
			const INT LayerNum = LandscapeInfo->LayerInfoMap.Num();

			TArray<WORD> HeightData;
			TArray<BYTE> WeightDatas; // Weight*Layers...
			TArray<ToolTarget::CacheClass::DataType> Data;

			if (bApplyToAll)
			{
				HeightCache->CacheData(X1,Y1,X2,Y2);
				HeightCache->GetCachedData(X1,Y1,X2,Y2,HeightData);

				WeightCache->CacheData(X1,Y1,X2,Y2);
				WeightCache->GetCachedData(X1,Y1,X2,Y2, WeightDatas, LayerNum);	
			}
			else
			{
				Cache->CacheData(X1,Y1,X2,Y2);
				Cache->GetCachedData(X1,Y1,X2,Y2,Data);
			}

			const FLOAT Width = Gizmo->GetWidth();
			const FLOAT Height = Gizmo->GetHeight();

			const FLOAT W = Gizmo->GetWidth() / (2 * ScaleXY);
			const FLOAT H = Gizmo->GetHeight() / (2 * ScaleXY);

			const FLOAT SignX = Gizmo->DrawScale * Gizmo->DrawScale3D.X > 0.f ? 1.0f : -1.0f;
			const FLOAT SignY = Gizmo->DrawScale * Gizmo->DrawScale3D.Y > 0.f ? 1.0f : -1.0f;

			const FLOAT ScaleX = Gizmo->CachedWidth / Width * ScaleXY / Gizmo->CachedScaleXY;
			const FLOAT ScaleY = Gizmo->CachedHeight / Height * ScaleXY / Gizmo->CachedScaleXY;

			FMatrix WToL = Proxy->WorldToLocal();
			//FMatrix LToW = Landscape->LocalToWorld();
			FVector BaseLocation = WToL.TransformFVector(Gizmo->Location);
			//FMatrix LandscapeLocalToGizmo = FRotationTranslationMatrix(FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X - W + 0.5, BaseLocation.Y - H + 0.5, 0));
			FMatrix LandscapeToGizmoLocal = 
				(FTranslationMatrix(FVector((- W + 0.5)*SignX, (- H + 0.5)*SignY, 0)) * FScaleRotationTranslationMatrix(FVector(SignX, SignY, 1.f), FRotator(0, Gizmo->Rotation.Yaw, 0), FVector(BaseLocation.X, BaseLocation.Y, 0))).Inverse();

			for( TMap<QWORD, FLOAT>::TIterator It(BrushInfo); It; ++It )
			{
				INT X, Y;
				ALandscape::UnpackKey(It.Key(), X, Y);

				if( It.Value() > 0.f )
				{
					// Value before we apply our painting
					INT index = (X-X1) + (Y-Y1)*(1+X2-X1);
					FLOAT PaintAmount = (bUseGizmoRegion || EdMode->CurrentBrush->GetBrushType() == FLandscapeBrush::BT_Gizmo) ? It.Value() : It.Value() * EdMode->UISettings.GetToolStrength() * Pressure;

					FVector GizmoLocal = LandscapeToGizmoLocal.TransformFVector(FVector(X, Y, 0));
					GizmoLocal.X *= ScaleX * SignX;
					GizmoLocal.Y *= ScaleY * SignY;

					INT LX = appFloor(GizmoLocal.X);
					INT LY = appFloor(GizmoLocal.Y);

					FLOAT FracX = GizmoLocal.X - LX;
					FLOAT FracY = GizmoLocal.Y - LY;

					FGizmoSelectData* Data00 = Gizmo->SelectedData.Find(ALandscape::MakeKey(LX, LY));
					FGizmoSelectData* Data10 = Gizmo->SelectedData.Find(ALandscape::MakeKey(LX+1, LY));
					FGizmoSelectData* Data01 = Gizmo->SelectedData.Find(ALandscape::MakeKey(LX, LY+1));
					FGizmoSelectData* Data11 = Gizmo->SelectedData.Find(ALandscape::MakeKey(LX+1, LY+1));

					{
						TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
						for (INT i = -1; (!bApplyToAll && i < 0) || i < LayerNum; ++i )
						{
							if ( (bApplyToAll && (i < 0)) || (!bApplyToAll && EdMode->CurrentToolTarget.TargetType == LET_Heightmap) )
							{
								FLOAT OriginalValue;
								if (bApplyToAll)
								{
									OriginalValue = HeightData(index);
								}
								else
								{
									OriginalValue = Data(index);
								}

								FLOAT Value = LandscapeDataAccess::GetLocalHeight(OriginalValue);

								FLOAT DestValue = FLandscapeHeightCache::ClampValue(
									LandscapeDataAccess::GetTexHeight(
									Lerp(
									Lerp(Data00 ? Lerp(Value, Gizmo->GetLandscapeHeight(Data00->HeightData), Data00->Ratio) : Value, 
									Data10 ? Lerp(Value, Gizmo->GetLandscapeHeight(Data10->HeightData), Data10->Ratio) : Value, FracX),
									Lerp(Data01 ? Lerp(Value, Gizmo->GetLandscapeHeight(Data01->HeightData), Data01->Ratio) : Value, 
									Data11 ? Lerp(Value, Gizmo->GetLandscapeHeight(Data11->HeightData), Data11->Ratio) : Value, FracX),
									FracY
									) ) 
									);

								switch(EdMode->UISettings.GetPasteMode())
								{
								case ELandscapeToolNoiseMode::Add:
									PaintAmount = OriginalValue < DestValue ? PaintAmount : 0.f;
									break;
								case ELandscapeToolNoiseMode::Sub:
									PaintAmount = OriginalValue > DestValue ? PaintAmount : 0.f;
									break;
								default:
									break;
								}

								if (bApplyToAll)
								{
									HeightData(index) = Lerp( OriginalValue, DestValue, PaintAmount );
								}
								else
								{
									Data(index) = Lerp( OriginalValue, DestValue, PaintAmount );
								}
							}
							else
							{
								FName LayerName;
								FLOAT OriginalValue;
								if (bApplyToAll)
								{
									LayerName = It.Key();
									OriginalValue = WeightDatas(index*LayerNum + i);
								}
								else
								{
									LayerName = EdMode->CurrentToolTarget.LayerName;
									OriginalValue = Data(index);
								}

								FLOAT DestValue = FLandscapeAlphaCache::ClampValue(
									Lerp(
									Lerp(Data00 ? Lerp(OriginalValue, Data00->WeightDataMap.FindRef(LayerName), Data00->Ratio) : OriginalValue, 
									Data10 ? Lerp(OriginalValue, Data10->WeightDataMap.FindRef(LayerName), Data10->Ratio) : OriginalValue, FracX),
									Lerp(Data01 ? Lerp(OriginalValue, Data01->WeightDataMap.FindRef(LayerName), Data01->Ratio) : OriginalValue, 
									Data11 ? Lerp(OriginalValue, Data11->WeightDataMap.FindRef(LayerName), Data11->Ratio) : OriginalValue, FracX),
									FracY
									));

								if (bApplyToAll)
								{
									WeightDatas(index*LayerNum + i) = Lerp( OriginalValue, DestValue, PaintAmount );
								}
								else
								{
									Data(index) = Lerp( OriginalValue, DestValue, PaintAmount );
								}
							}

							if (i >= 0)
							{
								++It;
							}
						}
					}
				}
			}

			for (int i = 0; i < Gizmo->LayerNames.Num(); ++i)
			{
				WeightCache->AddDirtyLayerName(Gizmo->LayerNames(i));
			}

			if (bApplyToAll)
			{
				HeightCache->SetCachedData(X1,Y1,X2,Y2,HeightData);
				HeightCache->Flush();
				if (WeightDatas.Num())
				{
					WeightCache->SetCachedData(X1,Y1,X2,Y2,WeightDatas, LayerNum);
					WeightCache->Flush();
				}
			}
			else
			{
				Cache->SetCachedData(X1,Y1,X2,Y2,Data);
				Cache->Flush();
			}
		}
	}
	

protected:
	class FEdModeLandscape* EdMode;
	class ULandscapeInfo* LandscapeInfo;

	UBOOL bToolActive;
	UBOOL bUseGizmoRegion;
	typename ToolTarget::CacheClass* Cache;
	FLandscapeHeightCache* HeightCache;
	FLandscapeFullWeightCache* WeightCache;
};

template<class ToolTarget>
class FLandscapeToolCopyPaste : public FLandscapeToolPaste<ToolTarget>
{
public:
	FLandscapeToolCopyPaste(class FEdModeLandscape* InEdMode)
		: FLandscapeToolPaste<ToolTarget>(InEdMode)
	{
		pCopyTool = new FLandscapeToolCopy<ToolTarget>(InEdMode);
	}

	~FLandscapeToolCopyPaste()
	{
		if (pCopyTool)
		{
			delete pCopyTool;
		}
	}

	// Just hybrid of Copy and Paste tool
	// Copy tool doesn't use any view information, so just do it as one function
	virtual void Process(INT Index, INT Arg)
	{
		switch(Index)
		{
		case 0: // Copy
			{
				if (pCopyTool && EdMode)
				{
					pCopyTool->BeginTool(NULL, EdMode->CurrentToolTarget, 0, 0 );
					pCopyTool->EndTool();
				}
			}
			break;
		case 1:// Paste
			{
				if (EdMode)
				{
					SetGizmoMode(TRUE);
					BeginTool(NULL, EdMode->CurrentToolTarget, 0, 0 );
					EndTool();
					SetGizmoMode(FALSE);
				}
			}
		}
	}

protected:
	FLandscapeToolCopy<ToolTarget>* pCopyTool;
};

//
// Tool targets
//
struct FHeightmapToolTarget
{
	typedef FLandscapeHeightCache CacheClass;
	enum EToolTargetType { TargetType = LET_Heightmap };

	static FLOAT StrengthMultiplier(ULandscapeInfo* LandscapeInfo, FLOAT BrushRadius)
	{
		if (LandscapeInfo && LandscapeInfo->LandscapeProxy)
		{
			// Adjust strength based on brush size and drawscale, so strength 1 = one hemisphere
			return BrushRadius * LANDSCAPE_INV_ZSCALE / (LandscapeInfo->LandscapeProxy->DrawScale3D.Z * LandscapeInfo->LandscapeProxy->DrawScale);
		}
		return 5.f * LANDSCAPE_INV_ZSCALE;
	}

	static FMatrix ToWorldMatrix(ULandscapeInfo* LandscapeInfo)
	{
		FMatrix Result = FTranslationMatrix(FVector(0,0,-32768.f));
		Result *= FScaleMatrix( FVector(1.f,1.f,LANDSCAPE_ZSCALE) * LandscapeInfo->LandscapeProxy->DrawScale3D * LandscapeInfo->LandscapeProxy->DrawScale );
		return Result;
	}

	static FMatrix FromWorldMatrix(ULandscapeInfo* LandscapeInfo)
	{
		FMatrix Result = FScaleMatrix( FVector(1.f,1.f,LANDSCAPE_INV_ZSCALE) / (LandscapeInfo->LandscapeProxy->DrawScale3D * LandscapeInfo->LandscapeProxy->DrawScale) );
		Result *= FTranslationMatrix(FVector(0,0,32768.f));
		return Result;
	}
};


struct FWeightmapToolTarget
{
	typedef FLandscapeAlphaCache CacheClass;
	enum EToolTargetType { TargetType = LET_Weightmap };

	static FLOAT StrengthMultiplier(ULandscapeInfo* LandscapeInfo, FLOAT BrushRadius)
	{
		return 255.f;
	}

	static FMatrix ToWorldMatrix(ULandscapeInfo* LandscapeInfo) { return FMatrix::Identity; }
	static FMatrix FromWorldMatrix(ULandscapeInfo* LandscapeInfo) { return FMatrix::Identity; }
};



//
// FEdModeLandscape
//

/** Constructor */
FEdModeLandscape::FEdModeLandscape() 
:	FEdMode()
,	bToolActive(FALSE)
,	LandscapeRenderAddCollision(NULL)
,	CurrentGizmoActor(NULL)
,	GizmoMaterial(NULL)
,	AddComponentToolIndex(INDEX_NONE)
,	MoveToLevelToolIndex(INDEX_NONE)
,	CopyPasteToolSet(NULL)
{
	ID = EM_Landscape;
	Desc = TEXT( "Landscape" );

	//GizmoMaterial = LoadObject<UMaterial>(NULL, TEXT("EditorLandscapeResources.LandscapeGizmo_Mat"), NULL, LOAD_None, NULL);
	GizmoMaterial = LoadObject<UMaterial>(NULL, TEXT("EditorLandscapeResources.GizmoMaterial"), NULL, LOAD_None, NULL);
	if (GizmoMaterial)
	{
		GizmoMaterial->AddToRoot();
	}

	GMaskRegionMaterial = LoadObject<UMaterialInstanceConstant>(NULL, TEXT("EditorLandscapeResources.MaskBrushMaterial_MaskedRegion"), NULL, LOAD_None, NULL);
	if (GMaskRegionMaterial)
	{
		GMaskRegionMaterial->AddToRoot();
	}

	// Initialize tools.
	FLandscapeToolSet* ToolSet_Paint = new FLandscapeToolSet(TEXT("ToolSet_Paint"));
	ToolSet_Paint->AddTool(new FLandscapeToolPaint<FHeightmapToolTarget>(this));
	ToolSet_Paint->AddTool(new FLandscapeToolPaint<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Paint);

	FLandscapeToolSet* ToolSet_Smooth = new FLandscapeToolSet(TEXT("ToolSet_Smooth"));
	ToolSet_Smooth->AddTool(new FLandscapeToolSmooth<FHeightmapToolTarget>(this));
	ToolSet_Smooth->AddTool(new FLandscapeToolSmooth<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Smooth);

	FLandscapeToolSet* ToolSet_Flatten = new FLandscapeToolSet(TEXT("ToolSet_Flatten"));
	ToolSet_Flatten->AddTool(new FLandscapeToolFlatten<FHeightmapToolTarget>(this));
	ToolSet_Flatten->AddTool(new FLandscapeToolFlatten<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Flatten);

	FLandscapeToolSet* ToolSet_Erosion = new FLandscapeToolSet(TEXT("ToolSet_Erosion"));
	ToolSet_Erosion->AddTool(new FLandscapeToolErosion(this));
	LandscapeToolSets.AddItem(ToolSet_Erosion);

	FLandscapeToolSet* ToolSet_HydraErosion = new FLandscapeToolSet(TEXT("ToolSet_HydraErosion"));
	ToolSet_HydraErosion->AddTool(new FLandscapeToolHydraErosion(this));
	LandscapeToolSets.AddItem(ToolSet_HydraErosion);

	FLandscapeToolSet* ToolSet_Noise = new FLandscapeToolSet(TEXT("ToolSet_Noise"));
	ToolSet_Noise->AddTool(new FLandscapeToolNoise<FHeightmapToolTarget>(this));
	ToolSet_Noise->AddTool(new FLandscapeToolNoise<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_Noise);

	FLandscapeToolSet* ToolSet_Select = new FLandscapeToolSet(TEXT("ToolSet_Select"));
	ToolSet_Select->AddTool(new FLandscapeToolSelect(this));
	LandscapeToolSets.AddItem(ToolSet_Select);

	AddComponentToolIndex = LandscapeToolSets.Num();
	FLandscapeToolSet* ToolSet_AddComponent = new FLandscapeToolSet(TEXT("ToolSet_AddComponent"));
	ToolSet_AddComponent->AddTool(new FLandscapeToolAddComponent(this));
	LandscapeToolSets.AddItem(ToolSet_AddComponent);

	FLandscapeToolSet* ToolSet_DeleteComponent = new FLandscapeToolSet(TEXT("ToolSet_DeleteComponent"));
	ToolSet_DeleteComponent->AddTool(new FLandscapeToolDeleteComponent(this));
	LandscapeToolSets.AddItem(ToolSet_DeleteComponent);

	MoveToLevelToolIndex = LandscapeToolSets.Num();
	FLandscapeToolSet* ToolSet_MoveToLevel = new FLandscapeToolSet(TEXT("ToolSet_MoveToLevel"));
	ToolSet_MoveToLevel->AddTool(new FLandscapeToolMoveToLevel(this));
	LandscapeToolSets.AddItem(ToolSet_MoveToLevel);

	FLandscapeToolSet* ToolSet_Mask = new FLandscapeToolSet(TEXT("ToolSet_Mask"));
	ToolSet_Mask->AddTool(new FLandscapeToolMask(this));
	LandscapeToolSets.AddItem(ToolSet_Mask);

	FLandscapeToolSet* ToolSet_CopyPaste = new FLandscapeToolSet(TEXT("ToolSet_CopyPaste"));
	ToolSet_CopyPaste->AddTool(new FLandscapeToolCopyPaste<FHeightmapToolTarget>(this));
	ToolSet_CopyPaste->AddTool(new FLandscapeToolCopyPaste<FWeightmapToolTarget>(this));
	LandscapeToolSets.AddItem(ToolSet_CopyPaste);
	CopyPasteToolSet = ToolSet_CopyPaste;

	FLandscapeToolSet* ToolSet_Visibility = new FLandscapeToolSet(TEXT("ToolSet_Visibility"));
	ToolSet_Visibility->AddTool(new FLandscapeToolVisibility(this));
	LandscapeToolSets.AddItem(ToolSet_Visibility);

	CurrentToolSet = NULL;
	CurrentToolIndex = -1;

	// Initialize brushes
	InitializeBrushes();

	CurrentBrush = LandscapeBrushSets(0).Brushes(0);
	CurrentBrushIndex = 0;

	CurrentToolTarget.LandscapeInfo = NULL;
	CurrentToolTarget.TargetType = LET_Heightmap;
	CurrentToolTarget.LayerName = NAME_None;
}


/** Destructor */
FEdModeLandscape::~FEdModeLandscape()
{
	if (GizmoMaterial)
	{
		GizmoMaterial->RemoveFromRoot();
	}

	if (GMaskRegionMaterial)
	{
		GMaskRegionMaterial->RemoveFromRoot();
	}

	// Save UI settings to config file
	UISettings.Save();

	// Destroy tools.
	for( INT ToolIdx=0;ToolIdx<LandscapeToolSets.Num();ToolIdx++ )
	{
		delete LandscapeToolSets(ToolIdx);
	}
	LandscapeToolSets.Empty();

	// Destroy brushes
	for( INT BrushSetIdx=0;BrushSetIdx<LandscapeBrushSets.Num();BrushSetIdx++ )
	{
		FLandscapeBrushSet& BrushSet = LandscapeBrushSets(BrushSetIdx);

		for( INT BrushIdx=0;BrushIdx < BrushSet.Brushes.Num();BrushIdx++ )
		{
			delete BrushSet.Brushes(BrushIdx);
		}
	}
	LandscapeBrushSets.Empty();
}

/** FSerializableObject: Serializer */
void FEdModeLandscape::Serialize( FArchive &Ar )
{
	// Call parent implementation
	FEdMode::Serialize( Ar );
}

void FEdModeLandscape::CopyDataToGizmo()
{
	// For Copy operation...
	if (CopyPasteToolSet /*&& CopyPasteToolSet == CurrentToolSet*/ )
	{
		if (CopyPasteToolSet->SetToolForTarget(CurrentToolTarget) && CopyPasteToolSet->GetTool())
		{
			CopyPasteToolSet->GetTool()->Process(0, 0);
		}
	}
	if (CurrentGizmoActor)
	{
		GEditor->SelectNone(FALSE, TRUE);
		GEditor->SelectActor(CurrentGizmoActor, TRUE, NULL, TRUE, TRUE );
	}
}

void FEdModeLandscape::PasteDataFromGizmo()
{
	// For Paste for Gizmo Region operation...
	if (CopyPasteToolSet /*&& CopyPasteToolSet == CurrentToolSet*/ )
	{
		if (CopyPasteToolSet->SetToolForTarget(CurrentToolTarget) && CopyPasteToolSet->GetTool())
		{
			CopyPasteToolSet->GetTool()->Process(1, 0);
		}
	}
	if (CurrentGizmoActor)
	{
		GEditor->SelectNone(FALSE, TRUE);
		GEditor->SelectActor(CurrentGizmoActor, TRUE, NULL, TRUE, TRUE );
	}
}

/** FEdMode: Called when the mode is entered */
void FEdModeLandscape::Enter()
{
	// Call parent implementation
	FEdMode::Enter();

	// Disable mobile rendering emulation if it's enabled
	if( GEditor->GetUserSettings().bEmulateMobileFeatures )
	{
		SetMobileRenderingEmulation( FALSE, GWorld->GetWorldInfo()->bUseGammaCorrection );
		GEditor->AccessUserSettings().bEmulateMobileFeatures = FALSE;
	}

	GEditor->SelectNone( FALSE, TRUE );

	for (FActorIterator It; It; ++It)
	{
		ALandscapeGizmoActiveActor* Gizmo = Cast<ALandscapeGizmoActiveActor>(*It);
		if (Gizmo)
		{
			CurrentGizmoActor = Gizmo;
			break;
		}
	}

	if (!CurrentGizmoActor)
	{
		CurrentGizmoActor = Cast<ALandscapeGizmoActiveActor>(GWorld->SpawnActor(ALandscapeGizmoActiveActor::StaticClass()));
		if (CurrentGizmoActor)
		{
			CurrentGizmoActor->ImportFromClipboard();
		}
	}

/*
	if (CurrentGizmoActor)
	{
		CurrentGizmoActor->SetTargetLandscape(CurrentToolTarget.LandscapeInfo);
	}
*/

	INT SquaredDataTex = ALandscapeGizmoActiveActor::DataTexSize * ALandscapeGizmoActiveActor::DataTexSize;

	if (CurrentGizmoActor && !CurrentGizmoActor->GizmoTexture)
	{
		// Init Gizmo Texture...
		CurrentGizmoActor->GizmoTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), CurrentGizmoActor->GetOutermost(), NAME_None, RF_Public|RF_Standalone);
		if (CurrentGizmoActor->GizmoTexture)
		{
			CurrentGizmoActor->GizmoTexture->Init(ALandscapeGizmoActiveActor::DataTexSize, ALandscapeGizmoActiveActor::DataTexSize, PF_G8);
			CurrentGizmoActor->GizmoTexture->SRGB = FALSE;
			CurrentGizmoActor->GizmoTexture->CompressionNone = TRUE;
			CurrentGizmoActor->GizmoTexture->MipGenSettings = TMGS_NoMipmaps;
			CurrentGizmoActor->GizmoTexture->AddressX = TA_Clamp;
			CurrentGizmoActor->GizmoTexture->AddressY = TA_Clamp;
			CurrentGizmoActor->GizmoTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
			CurrentGizmoActor->GizmoTexture->SizeX = ALandscapeGizmoActiveActor::DataTexSize;
			CurrentGizmoActor->GizmoTexture->SizeY = ALandscapeGizmoActiveActor::DataTexSize;
			//ULandscapeComponent::CreateEmptyTextureMips(DataTexture, TRUE);
			FTexture2DMipMap* MipMap = &CurrentGizmoActor->GizmoTexture->Mips(0);
			BYTE* TexData = (BYTE*)MipMap->Data.Lock(LOCK_READ_WRITE);
			appMemset(TexData, 0, SquaredDataTex*sizeof(BYTE));
			// Restore Sampled Data if exist...
			if (CurrentGizmoActor->CachedScaleXY > 0.f)
			{
				INT SizeX = appCeil(CurrentGizmoActor->CachedWidth  / CurrentGizmoActor->CachedScaleXY);
				INT SizeY = appCeil(CurrentGizmoActor->CachedHeight / CurrentGizmoActor->CachedScaleXY);
				for (INT Y = 0; Y < CurrentGizmoActor->SampleSizeY; ++Y)
				{
					for (INT X = 0; X < CurrentGizmoActor->SampleSizeX; ++X)
					{
						FLOAT TexX = X * SizeX / CurrentGizmoActor->SampleSizeX;
						FLOAT TexY = Y * SizeY / CurrentGizmoActor->SampleSizeY;
						INT LX = appFloor(TexX);
						INT LY = appFloor(TexY);

						FLOAT FracX = TexX - LX;
						FLOAT FracY = TexY - LY;

						FGizmoSelectData* Data00 = CurrentGizmoActor->SelectedData.Find(ALandscape::MakeKey(LX, LY));
						FGizmoSelectData* Data10 = CurrentGizmoActor->SelectedData.Find(ALandscape::MakeKey(LX+1, LY));
						FGizmoSelectData* Data01 = CurrentGizmoActor->SelectedData.Find(ALandscape::MakeKey(LX, LY+1));
						FGizmoSelectData* Data11 = CurrentGizmoActor->SelectedData.Find(ALandscape::MakeKey(LX+1, LY+1));

						TexData[X + Y*ALandscapeGizmoActiveActor::DataTexSize] = Lerp(
							Lerp(Data00 ? Data00->Ratio : 0, Data10 ? Data10->Ratio : 0, FracX),
							Lerp(Data01 ? Data01->Ratio : 0, Data11 ? Data11->Ratio : 0, FracX),
							FracY
							) * 255;
					}
				}
			}
			MipMap->Data.Unlock();
			CurrentGizmoActor->GizmoTexture->UpdateResource();
			FlushRenderingCommands();
		}
	}

	if (CurrentGizmoActor && CurrentGizmoActor->SampledHeight.Num() != SquaredDataTex)
	{
		CurrentGizmoActor->SampledHeight.Empty(SquaredDataTex);
		CurrentGizmoActor->SampledHeight.AddZeroed(SquaredDataTex);
		CurrentGizmoActor->DataType = LGT_None;
	}

	GLandscapeEditRenderMode = ELandscapeEditRenderMode::None;
	// Load UI settings from config file
	UISettings.Load();

#if WITH_MANAGED_CODE
	// Create the mesh paint window
	HWND EditorFrameWindowHandle = (HWND)GApp->EditorFrame->GetHandle();
	LandscapeEditWindow.Reset( FLandscapeEditWindow::CreateLandscapeEditWindow( this, EditorFrameWindowHandle ) );
	check( LandscapeEditWindow.IsValid() );
#endif

	// Force real-time viewports.  We'll back up the current viewport state so we can restore it when the
	// user exits this mode.
	const UBOOL bWantRealTime = TRUE;
	const UBOOL bRememberCurrentState = TRUE;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	CurrentBrush->EnterBrush();
	if (GizmoBrush)
	{
		GizmoBrush->EnterBrush();
	}

	SetCurrentTool(CurrentToolIndex >= 0 ? CurrentToolIndex : 0);
}


/** FEdMode: Called when the mode is exited */
void FEdModeLandscape::Exit()
{
	// Restore real-time viewport state if we changed it
	const UBOOL bWantRealTime = FALSE;
	const UBOOL bRememberCurrentState = FALSE;
	ForceRealTimeViewports( bWantRealTime, bRememberCurrentState );

	// Save any settings that may have changed
#if WITH_MANAGED_CODE
	if( LandscapeEditWindow.IsValid() )
	{
		LandscapeEditWindow->SaveWindowSettings();
	}

	// Kill the mesh paint window
	LandscapeEditWindow.Reset();
#endif

	LandscapeList.Empty();
	LandscapeTargetList.Empty();

	CurrentBrush->LeaveBrush();
	if (GizmoBrush)
	{
		GizmoBrush->LeaveBrush();
	}

	// Save UI settings to config file
	UISettings.Save();
	GLandscapeViewMode = ELandscapeViewMode::Normal;
	GLandscapeEditRenderMode = ELandscapeEditRenderMode::None;

	CurrentGizmoActor = NULL;

	GEditor->SelectNone( FALSE, TRUE );

	// Clear all GizmoActors if there is no Landscape in World
	UBOOL bIsLandscapeExist = FALSE;
	for (FActorIterator It; It; ++It)
	{
		ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
		if (Proxy)
		{
			bIsLandscapeExist = TRUE;
			break;
		}
	}

	if (!bIsLandscapeExist)
	{
		for (FActorIterator It; It; ++It)
		{
			ALandscapeGizmoActor* Gizmo = Cast<ALandscapeGizmoActor>(*It);
			if (Gizmo)
			{
				GWorld->DestroyActor(Gizmo, FALSE, FALSE);
			}
		}
	}

	// Call parent implementation
	FEdMode::Exit();
}

/** FEdMode: Called once per frame */
void FEdModeLandscape::Tick(FEditorLevelViewportClient* ViewportClient,FLOAT DeltaTime)
{
	FEdMode::Tick(ViewportClient,DeltaTime);

	if( CurrentToolSet && CurrentToolSet->GetTool() )
	{
		CurrentToolSet->GetTool()->Tick(ViewportClient,DeltaTime);
	}
	if( CurrentBrush )
	{
		CurrentBrush->Tick(ViewportClient,DeltaTime);
	}
	if ( CurrentBrush!=GizmoBrush && CurrentGizmoActor && GizmoBrush && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo )
	{
		GizmoBrush->Tick(ViewportClient, DeltaTime);
	}

#if _WINDOWS
	// Process Deferred Messages
	if (GLastKeyLevelEditingViewportClient)
	{
		for (int i = 0; i < WindowMessages.Num(); ++i)
		{
			FDeferredWindowMessage& Message = WindowMessages(i);

			FWindowsViewport* Viewport = static_cast<FWindowsViewport*>(GLastKeyLevelEditingViewportClient->Viewport);
			if (Viewport)
			{
				Viewport->ViewportWndProc(Message.Message, Message.wParam, Message.lParam);
			}
		}
	}
	WindowMessages.Empty();
#endif
}


/** FEdMode: Called when the mouse is moved over the viewport */
UBOOL FEdModeLandscape::MouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT MouseX, INT MouseY )
{
	UBOOL Result = FALSE;
	if( CurrentToolSet && CurrentToolSet->GetTool() )
	{
		Result = CurrentToolSet && CurrentToolSet->GetTool()->MouseMove(ViewportClient, Viewport, MouseX, MouseY);
		ViewportClient->Invalidate( FALSE, FALSE );
	}
	return Result;
}

/**
 * Called when the mouse is moved while a window input capture is in effect
 *
 * @param	InViewportClient	Level editor viewport client that captured the mouse input
 * @param	InViewport			Viewport that captured the mouse input
 * @param	InMouseX			New mouse cursor X coordinate
 * @param	InMouseY			New mouse cursor Y coordinate
 *
 * @return	TRUE if input was handled
 */
UBOOL FEdModeLandscape::CapturedMouseMove( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, INT MouseX, INT MouseY )
{
	UBOOL Result = FALSE;
	if( CurrentToolSet && CurrentToolSet->GetTool() )
	{
		Result = CurrentToolSet && CurrentToolSet->GetTool()->CapturedMouseMove(ViewportClient, Viewport, MouseX, MouseY);
		ViewportClient->Invalidate( FALSE, FALSE );
	}
	return Result;
}

namespace
{
	UBOOL GIsGizmoDragging = FALSE;
}

/** FEdMode: Called when a mouse button is pressed */
UBOOL FEdModeLandscape::StartTracking()
{
	if (CurrentGizmoActor && CurrentGizmoActor->IsSelected() && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo)
	{
		GIsGizmoDragging = TRUE;
	}
	return TRUE;
}



/** FEdMode: Called when the a mouse button is released */
UBOOL FEdModeLandscape::EndTracking()
{
	GIsGizmoDragging = FALSE;
	return TRUE;
}

namespace
{
	UBOOL RayIntersectTriangle(const FVector& Start, const FVector& End, const FVector& A, const FVector& B, const FVector& C, FVector& IntersectPoint )
	{
		const FVector BA = A - B;
		const FVector CB = B - C;
		const FVector TriNormal = BA ^ CB;

		UBOOL bCollide = SegmentPlaneIntersection(Start, End, FPlane(A, TriNormal), IntersectPoint );
		if (!bCollide)
		{
			return FALSE;
		}

		FVector BaryCentric = ComputeBaryCentric2D(IntersectPoint, A, B, C);
		if (BaryCentric.X > 0.f && BaryCentric.Y > 0.f && BaryCentric.Z > 0.f )
		{
			return TRUE;
		}
		return FALSE;
	}
};

/** Trace under the mouse cursor and return the landscape hit and the hit location (in landscape quad space) */
UBOOL FEdModeLandscape::LandscapeMouseTrace( FEditorLevelViewportClient* ViewportClient, FLOAT& OutHitX, FLOAT& OutHitY )
{
	INT MouseX = ViewportClient->Viewport->GetMouseX();
	INT MouseY = ViewportClient->Viewport->GetMouseY();

	return LandscapeMouseTrace( ViewportClient, MouseX, MouseY, OutHitX, OutHitY );
}

/** Trace under the specified coordinates and return the landscape hit and the hit location (in landscape quad space) */
UBOOL FEdModeLandscape::LandscapeMouseTrace( FEditorLevelViewportClient* ViewportClient, INT MouseX, INT MouseY, FLOAT& OutHitX, FLOAT& OutHitY )
{
	// Compute a world space ray from the screen space mouse coordinates
	FSceneViewFamilyContext ViewFamily(
		ViewportClient->Viewport, ViewportClient->GetScene(),
		ViewportClient->ShowFlags,
		GWorld->GetTimeSeconds(),
		GWorld->GetDeltaSeconds(),
		GWorld->GetRealTimeSeconds(),
		ViewportClient->IsRealtime()
		);
	FSceneView* View = ViewportClient->CalcSceneView( &ViewFamily );
	FViewportCursorLocation MouseViewportRay( View, ViewportClient, MouseX, MouseY );

	FVector Start = MouseViewportRay.GetOrigin();
	FVector End = Start + WORLD_MAX * MouseViewportRay.GetDirection();

	FMemMark		Mark(GMainThreadMemStack);
	FCheckResult*	FirstHit	= NULL;
	DWORD			TraceFlags	= TRACE_Terrain|TRACE_TerrainIgnoreHoles;

	FirstHit	= GWorld->MultiLineCheck(GMainThreadMemStack, End, Start, FVector(0.f,0.f,0.f), TraceFlags, NULL);
	for( FCheckResult* Test = FirstHit; Test; Test = Test->GetNext() )
	{
		ULandscapeHeightfieldCollisionComponent* CollisionComponent = Cast<ULandscapeHeightfieldCollisionComponent>(Test->Component);
		if( CollisionComponent )
		{
			ALandscapeProxy* HitLandscape = CollisionComponent->GetLandscapeProxy();	

			if( HitLandscape && CurrentToolTarget.LandscapeInfo && CurrentToolTarget.LandscapeInfo->LandscapeProxy && HitLandscape->LandscapeGuid == CurrentToolTarget.LandscapeInfo->LandscapeGuid )
			{
				FVector LocalHit = CurrentToolTarget.LandscapeInfo->LandscapeProxy->WorldToLocal().TransformFVector(Test->Location);
				OutHitX = LocalHit.X;
				OutHitY = LocalHit.Y;

				Mark.Pop();

				return TRUE;
			}
		}
	}

	// For Add Landscape Component Mode
	if (CurrentToolIndex == AddComponentToolIndex && CurrentToolTarget.LandscapeInfo)
	{
		UBOOL bCollided = FALSE;
		FVector IntersectPoint;
		LandscapeRenderAddCollision = NULL;
		// Need to optimize collision for AddLandscapeComponent...?
		for (TMap<QWORD, FLandscapeAddCollision>::TIterator It(CurrentToolTarget.LandscapeInfo->XYtoAddCollisionMap); It; ++It )
		{
			FLandscapeAddCollision& AddCollision = It.Value();
			// Triangle 1
			bCollided = RayIntersectTriangle(Start, End, AddCollision.Corners[0], AddCollision.Corners[3], AddCollision.Corners[1], IntersectPoint );
			if (bCollided)
			{
				LandscapeRenderAddCollision = &AddCollision;
				break;
			}
			// Triangle 2
			bCollided = RayIntersectTriangle(Start, End, AddCollision.Corners[0], AddCollision.Corners[2], AddCollision.Corners[3], IntersectPoint );
			if (bCollided)
			{
				LandscapeRenderAddCollision = &AddCollision;
				break;
			}
		}
		if (bCollided && CurrentToolTarget.LandscapeInfo && CurrentToolTarget.LandscapeInfo->LandscapeProxy )
		{
			FVector LocalHit = CurrentToolTarget.LandscapeInfo->LandscapeProxy->WorldToLocal().TransformFVector(IntersectPoint);
			OutHitX = LocalHit.X;
			OutHitY = LocalHit.Y;

			Mark.Pop();

			return TRUE;
		}
	}

	Mark.Pop();
	return FALSE;
}

namespace
{
	const INT SelectionSizeThresh = 2 * 256 * 256;
	FORCEINLINE UBOOL IsSlowSelect(ULandscapeInfo* LandscapeInfo)
	{
		if (LandscapeInfo)
		{
			INT MinX = MAXINT, MinY = MAXINT, MaxX = MININT, MaxY = MININT;
			LandscapeInfo->GetSelectedExtent(MinX, MinY, MaxX, MaxY);
			return (MinX != MAXINT && ( (MaxX - MinX) * (MaxY - MinY) ));
		}
		return FALSE;
	}
};

UBOOL FEdModeLandscape::ProcessEditCut()
{
	// Just prevent Cut in Landscape Mode
	return TRUE;
}

UBOOL FEdModeLandscape::ProcessEditCopy()
{
	if (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo || GLandscapeEditRenderMode & (ELandscapeEditRenderMode::Select))
	{
		if (CurrentGizmoActor && GizmoBrush && CurrentGizmoActor->TargetLandscapeInfo)
		{
			UBOOL IsSlowTask = IsSlowSelect(CurrentGizmoActor->TargetLandscapeInfo);

			if (IsSlowTask)
			{
				GWarn->BeginSlowTask( TEXT("Fit Gizmo to Selected Region and Copy Data..."), TRUE);
			}
			CurrentGizmoActor->FitToSelection();
			CopyDataToGizmo();
			SetCurrentTool(FName(TEXT("ToolSet_CopyPaste")));

			if (IsSlowTask)
			{
				GWarn->EndSlowTask();
			}
		}
	}
	return TRUE;
}

UBOOL FEdModeLandscape::ProcessEditPaste()
{
	if (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo || GLandscapeEditRenderMode & (ELandscapeEditRenderMode::Select))
	{
		if (CurrentGizmoActor && GizmoBrush && CurrentGizmoActor->TargetLandscapeInfo)
		{
			UBOOL IsSlowTask = IsSlowSelect(CurrentGizmoActor->TargetLandscapeInfo);
			if (IsSlowTask)
			{
				GWarn->BeginSlowTask( TEXT("Paste Gizmo Data..."), TRUE);
			}
			PasteDataFromGizmo();
			SetCurrentTool(FName(TEXT("ToolSet_CopyPaste")));
			if (IsSlowTask)
			{
				GWarn->EndSlowTask();
			}
		}
	}
	return TRUE;
}

/** FEdMode: Called when a key is pressed */
UBOOL FEdModeLandscape::InputKey( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, FName Key, EInputEvent Event )
{
	// Override Key Input for Selection Brush
	if( CurrentBrush && CurrentBrush->InputKey(ViewportClient, Viewport, Key, Event)==TRUE )
	{
		return TRUE;
	}

	if ((Event == IE_Pressed || Event == IE_Released) && IsCtrlDown(Viewport))
	{
		// Cheat, but works... :)
		UBOOL bNeedSelectGizmo = CurrentGizmoActor && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) && CurrentGizmoActor->IsSelected();
		GEditor->SelectNone( FALSE, TRUE );
		if (bNeedSelectGizmo)
		{
			GEditor->SelectActor( CurrentGizmoActor, TRUE, ViewportClient, FALSE );
		}
	}

	if( Key == KEY_LeftMouseButton )
	{
		if( Event == IE_Pressed && (IsCtrlDown(Viewport) || (Viewport->IsPenActive() && Viewport->GetTabletPressure() > 0.f)) )
		{
			if( CurrentToolSet )
			{				
				if( CurrentToolSet->SetToolForTarget(CurrentToolTarget) )
				{
					FLOAT HitX, HitY;
					if( LandscapeMouseTrace(ViewportClient, HitX, HitY) )
					{
						bToolActive = CurrentToolSet->GetTool()->BeginTool(ViewportClient, CurrentToolTarget, HitX, HitY);
					}
				}
			}
			return TRUE;
		}
	}

	if( Key == KEY_LeftMouseButton || Key==KEY_LeftControl || Key==KEY_RightControl )
	{
		if( Event == IE_Released && CurrentToolSet && CurrentToolSet->GetTool() && bToolActive )
		{
			CurrentToolSet->GetTool()->EndTool();
			// Check for validation....
			CurrentToolTarget.LandscapeInfo->CheckValidate();
			bToolActive = FALSE;
			return (Key == KEY_LeftMouseButton);
		}
	}

	// Change Brush Size
	if ((Event == IE_Pressed || Event == IE_Repeat) && (Key == KEY_LeftBracket || Key == KEY_RightBracket) )
	{
		if (CurrentBrush->GetBrushType() == FLandscapeBrush::BT_Component)
		{
			INT Radius = UISettings.GetBrushComponentSize();
			if (Key == KEY_LeftBracket)
			{
				--Radius;
			}
			else
			{
				++Radius;
			}
			Radius = (INT)Clamp(Radius, 1, 64);
			UISettings.SetBrushComponentSize(Radius);
#if WITH_MANAGED_CODE
			if( LandscapeEditWindow.IsValid() )
			{
				LandscapeEditWindow->NotifyBrushComponentSizeChanged(Radius);
			}
#endif
		}
		else
		{
			FLOAT Radius = UISettings.GetBrushRadius();
			FLOAT SliderMin = 0.f;
			FLOAT SliderMax = 8192.f;
			FLOAT LogPosition = Clamp(Radius / SliderMax, 0.0f, 1.0f);
			FLOAT Diff = 0.05f; //6.f / SliderMax;
			if (Key == KEY_LeftBracket)
			{
				Diff = -Diff;
			}

			FLOAT NewValue = Radius*(1.f+Diff);

			if (Key == KEY_LeftBracket)
			{
				NewValue = Min(NewValue, Radius - 1.f);
			}
			else
			{
				NewValue = Max(NewValue, Radius + 1.f);
			}

			NewValue = (INT)Clamp(NewValue, SliderMin, SliderMax);
			// convert from Exp scale to linear scale
			//FLOAT LinearPosition = 1.0f - appPow(1.0f - LogPosition, 1.0f / 3.0f);
			//LinearPosition = Clamp(LinearPosition + Diff, 0.f, 1.f);
			//FLOAT NewValue = Clamp((1.0f - appPow(1.0f - LinearPosition, 3.f)) * SliderMax, SliderMin, SliderMax);
			//FLOAT NewValue = Clamp((SliderMax - SliderMin) * LinearPosition + SliderMin, SliderMin, SliderMax);

			UISettings.SetBrushRadius(NewValue);
#if WITH_MANAGED_CODE
			if( LandscapeEditWindow.IsValid() )
			{
				LandscapeEditWindow->NotifyBrushSizeChanged(NewValue);
			}
#endif
		}
		return TRUE;
	}

	// Prev tool 
	if( Event == IE_Pressed && Key == KEY_Comma )
	{
		if( CurrentToolSet && CurrentToolSet->GetTool() && bToolActive )
		{
			CurrentToolSet->GetTool()->EndTool();
			bToolActive = FALSE;
		}
		INT NewToolIndex = Clamp(LandscapeToolSets.FindItemIndex(CurrentToolSet) - 1, 0, LandscapeToolSets.Num()-1);
		SetCurrentTool( LandscapeToolSets.IsValidIndex(NewToolIndex) ? NewToolIndex : 0 );

		return TRUE;
	}

	// Next tool 
	if( Event == IE_Pressed && Key == KEY_Period )
	{
		if( CurrentToolSet && CurrentToolSet->GetTool() && bToolActive )
		{
			CurrentToolSet->GetTool()->EndTool();
			bToolActive = FALSE;
		}

		INT NewToolIndex = Clamp(LandscapeToolSets.FindItemIndex(CurrentToolSet) + 1, 0, LandscapeToolSets.Num()-1);
		SetCurrentTool( LandscapeToolSets.IsValidIndex(NewToolIndex) ? NewToolIndex : 0 );

		return TRUE;
	}

	if( CurrentToolSet && CurrentToolSet->GetTool() && CurrentToolSet->GetTool()->InputKey(ViewportClient, Viewport, Key, Event)==TRUE )
	{
		return TRUE;
	}

	if( CurrentBrush && CurrentBrush->InputKey(ViewportClient, Viewport, Key, Event)==TRUE )
	{
		return TRUE;
	}

	return FALSE;
}

void FEdModeLandscape::SetCurrentTool( FName ToolSetName )
{
	INT ToolIndex = 0;
	for (; ToolIndex < LandscapeToolSets.Num(); ++ToolIndex )
	{
		if ( ToolSetName == FName(LandscapeToolSets(ToolIndex)->GetToolSetName()))
		{
			break;
		}
	}

	SetCurrentTool(ToolIndex);
}

void FEdModeLandscape::SetCurrentTool( INT ToolIndex )
{
	if (CurrentToolSet)
	{
		CurrentToolSet->PreviousBrushIndex = CurrentBrushIndex;
	}
	CurrentToolIndex = LandscapeToolSets.IsValidIndex(ToolIndex) ? ToolIndex : 0;
	CurrentToolSet = LandscapeToolSets( ToolIndex );
	if (ToolIndex != AddComponentToolIndex)
	{
		LandscapeRenderAddCollision = NULL;
	}
	CurrentToolSet->SetToolForTarget( CurrentToolTarget );
	if (CurrentToolSet->GetTool())
	{
		CurrentToolSet->GetTool()->SetEditRenderType();
		UBOOL MaskEnabled = CurrentToolSet->GetTool()->GetMaskEnable() && CurrentToolTarget.LandscapeInfo && CurrentToolTarget.LandscapeInfo->SelectedRegion.Num();
		SetMaskEnable(MaskEnabled);
	}

	// Set Brush
	if (CurrentToolSet->PreviousBrushIndex >= 0)
	{
		CurrentBrushIndex = CurrentToolSet->PreviousBrushIndex;
	}

#if WITH_MANAGED_CODE
	if( LandscapeEditWindow.IsValid() )
	{
		LandscapeEditWindow->NotifyCurrentToolChanged(CurrentToolSet->GetToolSetNameString());
		//LandscapeEditWindow->NotifyMaskEnableChanged(CurrentToolTarget.Landscape ? CurrentToolTarget.Landscape->SelectedRegion.Num() :  FALSE);
	}
#endif
}

void FEdModeLandscape::SetMaskEnable( UBOOL bMaskEnabled )
{
	if (UISettings.GetbMaskEnabled() != bMaskEnabled)
	{
#if WITH_MANAGED_CODE
		if( LandscapeEditWindow.IsValid() )
		{
			LandscapeEditWindow->NotifyMaskEnableChanged(bMaskEnabled);
		}
#endif
	}
}

TArray<FLandscapeTargetListInfo>* FEdModeLandscape::GetTargetList()
{
	return &LandscapeTargetList;
}

TArray<FLandscapeListInfo>* FEdModeLandscape::GetLandscapeList()
{
	return &LandscapeList;
}

void FEdModeLandscape::AddLayerInfo(ULandscapeLayerInfoObject* LayerInfo)
{
	if ( CurrentToolTarget.LandscapeInfo && !CurrentToolTarget.LandscapeInfo->LayerInfoMap.FindRef(LayerInfo->LayerName) )
	{
		if (CurrentToolTarget.LandscapeInfo->LandscapeProxy)
		{
			CurrentToolTarget.LandscapeInfo->LandscapeProxy->LayerInfoObjs.AddItem(FLandscapeLayerStruct(LayerInfo, CurrentToolTarget.LandscapeInfo->LandscapeProxy, NULL));
			CurrentToolTarget.LandscapeInfo->UpdateLayerInfoMap(CurrentToolTarget.LandscapeInfo->LandscapeProxy);
			//CurrentToolTarget.LandscapeInfo->LayerInfoMap.Set(LayerInfo->LayerName, &CurrentToolTarget.LandscapeInfo->LandscapeProxy->LayerInfoObjs.Last());
			UpdateTargetList();
		}
	}
}

INT FEdModeLandscape::UpdateLandscapeList()
{
	LandscapeList.Empty();

	FGuid CurrentGuid;
	if ( CurrentToolTarget.LandscapeInfo )
	{
		CurrentGuid = CurrentToolTarget.LandscapeInfo->LandscapeGuid;
	}

	INT CurrentIndex = -1, Index = 0;
	if (GWorld && GWorld->GetWorldInfo())
	{
		for (TMap<FGuid, ULandscapeInfo*>::TIterator It(GWorld->GetWorldInfo()->LandscapeInfoMap); It; ++It)
		{
			ULandscapeInfo* LandscapeInfo = It.Value();
			if( LandscapeInfo )
			{
				if( LandscapeInfo->LandscapeProxy )
				{
					if (CurrentGuid == LandscapeInfo->LandscapeGuid || CurrentIndex == -1)
					{
						CurrentIndex = Index;
						//CurrentToolTarget.LandscapeInfo = LandscapeInfo;
					}

					INT MinX, MinY, MaxX, MaxY;
					INT Width = 0, Height = 0;
					if (LandscapeInfo->GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
					{
						Width = MaxX - MinX + 1;
						Height = MaxY - MinY + 1;
					}

					LandscapeList.AddItem(FLandscapeListInfo(*FString::Printf(TEXT("%s.%s"), *LandscapeInfo->LandscapeProxy->GetOutermost()->GetName(), *LandscapeInfo->LandscapeProxy->GetName()), LandscapeInfo->LandscapeGuid, LandscapeInfo, 
										LandscapeInfo->LandscapeProxy->ComponentSizeQuads, LandscapeInfo->LandscapeProxy->NumSubsections, Width, Height));

					Index++;
				}
			}
		}
	}

	return CurrentIndex;
}

void FEdModeLandscape::UpdateTargetList()
{
	LandscapeTargetList.Empty();
	
	if( CurrentToolTarget.LandscapeInfo )
	{
		UBOOL bFoundSelected = FALSE;
		// Add heightmap
		new(LandscapeTargetList) FLandscapeTargetListInfo(TEXT("Height map"), LET_Heightmap, NULL, TRUE, CurrentToolTarget.LandscapeInfo);

		// Add layers
		UTexture2D* ThumbnailWeightmap = NULL;
		UTexture2D* ThumbnailHeightmap = NULL;

		for( TMap<FName, FLandscapeLayerStruct*>::TIterator It(CurrentToolTarget.LandscapeInfo->LayerInfoMap); It; ++It )
		{
			FLandscapeLayerStruct* LayerInfo =  It.Value();
			if (LayerInfo && LayerInfo->LayerInfoObj)
			{
				FName LayerName = LayerInfo->LayerInfoObj->LayerName;

				if( LayerInfo->bSelected )
				{
					if( bFoundSelected )
					{
						LayerInfo->bSelected = FALSE;
					}
					else
					{
						bFoundSelected = TRUE;
						CurrentToolTarget.TargetType = LET_Weightmap;
						CurrentToolTarget.LayerName  = LayerName;
					}
				}

				// Ensure thumbnails are up valid
				if( LayerInfo->ThumbnailMIC == NULL )
				{
					if( ThumbnailWeightmap == NULL )
					{
						ThumbnailWeightmap = LoadObject<UTexture2D>(NULL, TEXT("EditorLandscapeResources.LandscapeThumbnailWeightmap"), NULL, LOAD_None, NULL);
					}
					if( ThumbnailHeightmap == NULL )
					{
						ThumbnailHeightmap = LoadObject<UTexture2D>(NULL, TEXT("EditorLandscapeResources.LandscapeThumbnailHeightmap"), NULL, LOAD_None, NULL);
					}

					// Construct Thumbnail MIC
					ULandscapeMaterialInstanceConstant* ThumbnailMIC = ConstructObject<ULandscapeMaterialInstanceConstant>(ULandscapeMaterialInstanceConstant::StaticClass(), LayerInfo->Owner ? LayerInfo->Owner->GetOutermost() : INVALID_OBJECT, NAME_None, RF_Public|RF_Standalone);
					ThumbnailMIC->bIsLayerThumbnail = TRUE;
					LayerInfo->ThumbnailMIC = ThumbnailMIC;

					LayerInfo->ThumbnailMIC->SetParent( LayerInfo->Owner ? LayerInfo->Owner->GetLandscapeMaterial() : GEngine->DefaultMaterial);
					FStaticParameterSet StaticParameters;
					LayerInfo->ThumbnailMIC->GetStaticParameterValues(&StaticParameters);

					for( INT LayerParameterIdx=0;LayerParameterIdx<StaticParameters.TerrainLayerWeightParameters.Num();LayerParameterIdx++ )
					{
						FStaticTerrainLayerWeightParameter& LayerParameter = StaticParameters.TerrainLayerWeightParameters(LayerParameterIdx);
						if( LayerParameter.ParameterName == LayerName )
						{
							LayerParameter.WeightmapIndex = 0;
							LayerParameter.bOverride = TRUE;
						}
						else
						{
							LayerParameter.WeightmapIndex = INDEX_NONE;
						}
					}

					LayerInfo->ThumbnailMIC->SetStaticParameterValues(&StaticParameters);
					LayerInfo->ThumbnailMIC->InitResources();
					LayerInfo->ThumbnailMIC->UpdateStaticPermutation();
					LayerInfo->ThumbnailMIC->SetTextureParameterValue(FName("Weightmap0"), ThumbnailWeightmap); 
					LayerInfo->ThumbnailMIC->SetTextureParameterValue(FName("Heightmap"), ThumbnailHeightmap);
				}

				// Add the layer
				new(LandscapeTargetList) FLandscapeTargetListInfo(*LayerName.ToString(), LET_Weightmap, LayerInfo, LayerInfo->bSelected);
			}
		}

		if( !bFoundSelected )
		{
			LandscapeTargetList(0).bSelected = TRUE;
			CurrentToolTarget.TargetType = LET_Heightmap;
			CurrentToolTarget.LayerName  = NAME_None;
		}
	}
}

/** FEdMode: Called when mouse drag input it applied */
UBOOL FEdModeLandscape::InputDelta( FEditorLevelViewportClient* InViewportClient, FViewport* InViewport, FVector& InDrag, FRotator& InRot, FVector& InScale )
{
	return FALSE;
}

/** FEdMode: Render the mesh paint tool */
void FEdModeLandscape::Render( const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI )
{
	/** Call parent implementation */
	FEdMode::Render( View, Viewport, PDI );

	if (LandscapeRenderAddCollision)
	{
		PDI->DrawLine(LandscapeRenderAddCollision->Corners[0], LandscapeRenderAddCollision->Corners[3], FColor(0, 255, 128), SDPG_Foreground);
		PDI->DrawLine(LandscapeRenderAddCollision->Corners[3], LandscapeRenderAddCollision->Corners[1], FColor(0, 255, 128), SDPG_Foreground);
		PDI->DrawLine(LandscapeRenderAddCollision->Corners[1], LandscapeRenderAddCollision->Corners[0], FColor(0, 255, 128), SDPG_Foreground);

		PDI->DrawLine(LandscapeRenderAddCollision->Corners[0], LandscapeRenderAddCollision->Corners[2], FColor(0, 255, 128), SDPG_Foreground);
		PDI->DrawLine(LandscapeRenderAddCollision->Corners[2], LandscapeRenderAddCollision->Corners[3], FColor(0, 255, 128), SDPG_Foreground);
		PDI->DrawLine(LandscapeRenderAddCollision->Corners[3], LandscapeRenderAddCollision->Corners[0], FColor(0, 255, 128), SDPG_Foreground);
	}

	if (!GIsGizmoDragging && GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo && CurrentToolTarget.LandscapeInfo && CurrentGizmoActor && CurrentGizmoActor->TargetLandscapeInfo)
	{
		FDynamicMeshBuilder MeshBuilder;

		for (INT i = 0; i < 8; ++i)
		{
			MeshBuilder.AddVertex(CurrentGizmoActor->FrustumVerts[i], FVector2D(0, 0), FVector(1,0,0), FVector(0,1,0), FVector(0,0,1), FColor(255,255,255));
		}

		// Upper box.
		MeshBuilder.AddTriangle( 0, 2, 1 );
		MeshBuilder.AddTriangle( 0, 3, 2 );
		// Lower box.
		MeshBuilder.AddTriangle( 4, 6, 5 );
		MeshBuilder.AddTriangle( 4, 7, 6 );
		// Others
		MeshBuilder.AddTriangle( 1, 4, 0 );
		MeshBuilder.AddTriangle( 1, 5, 4 );

		MeshBuilder.AddTriangle( 3, 6, 2 );
		MeshBuilder.AddTriangle( 3, 7, 6 );

		MeshBuilder.AddTriangle( 2, 5, 1 );
		MeshBuilder.AddTriangle( 2, 6, 5 );

		MeshBuilder.AddTriangle( 0, 7, 3 );
		MeshBuilder.AddTriangle( 0, 4, 7 );

		PDI->SetHitProxy(new HTranslucentActor(CurrentGizmoActor));
		MeshBuilder.Draw(PDI, FMatrix::Identity, GizmoMaterial->GetRenderProxy(FALSE), SDPG_World, 0, TRUE);
		PDI->SetHitProxy(NULL);
	}
}

/** FEdMode: Render HUD elements for this tool */
void FEdModeLandscape::DrawHUD( FEditorLevelViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas )
{

}

FVector FEdModeLandscape::GetWidgetLocation() const
{
	if (CurrentGizmoActor && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) && CurrentGizmoActor->TargetLandscapeInfo && CurrentGizmoActor->TargetLandscapeInfo->LandscapeProxy && CurrentGizmoActor->IsSelected())
	{
		return CurrentGizmoActor->Location + FRotationMatrix(CurrentGizmoActor->TargetLandscapeInfo->LandscapeProxy->Rotation).TransformFVector(FVector(0, 0, CurrentGizmoActor->GetLength()));
	}
	return FEdMode::GetWidgetLocation();
}

UBOOL FEdModeLandscape::UsesTransformWidget() const
{
	return (CurrentGizmoActor && CurrentGizmoActor->IsSelected() && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) );
}

UBOOL FEdModeLandscape::ShouldDrawWidget() const
{
	return (CurrentGizmoActor && CurrentGizmoActor->IsSelected() && (GLandscapeEditRenderMode & ELandscapeEditRenderMode::Gizmo) );
}

UBOOL FEdModeLandscape::Select( AActor* InActor, UBOOL bInSelected )
{
	if (InActor->IsA(ALandscapeProxy::StaticClass()) )
	{
		return FALSE;
	}
	else if (InActor->IsA(ALandscapeGizmoActor::StaticClass()))
	{
		return FALSE;
	}
	else if (InActor->IsA(ALight::StaticClass()))
	{
		return FALSE;
	}
	else if (!bInSelected)
	{
		return FALSE;
	}
	return TRUE;
}

/** FEdMode: Called when the currently selected actor has changed */
void FEdModeLandscape::ActorSelectionChangeNotify()
{
	if (CurrentGizmoActor && CurrentGizmoActor->IsSelected())
	{
		GEditor->SelectNone(FALSE, TRUE);
		GEditor->SelectActor(CurrentGizmoActor, TRUE, NULL, FALSE, TRUE);
	}
/*
	USelection* EditorSelection = GEditor->GetSelectedActors();
	for ( USelection::TObjectIterator Itor = EditorSelection->ObjectItor() ; Itor ; ++Itor )
	{
		if (((*Itor)->IsA(ALandscapeGizmoActor::StaticClass())) )
		{
			bIsGizmoSelected = TRUE;
			break;
		}
	}
*/
}

void FEdModeLandscape::ActorMoveNotify()
{
	//GUnrealEd->UpdatePropertyWindows();
}

INT FEdModeLandscape::GetWidgetAxisToDraw( FWidget::EWidgetMode InWidgetMode ) const
{
	switch(InWidgetMode)
	{
	case FWidget::WM_Translate:
		return AXIS_XYZ;
	case FWidget::WM_Rotate:
		return AXIS_Z;
	case FWidget::WM_Scale:
	case FWidget::WM_ScaleNonUniform:
		return AXIS_XYZ;
	default:
		return 0;
	}
}

/** Forces real-time perspective viewports */
void FEdModeLandscape::ForceRealTimeViewports( const UBOOL bEnable, const UBOOL bStoreCurrentState )
{
	// Force perspective views to be real-time
	for( INT CurViewportIndex = 0; CurViewportIndex < GApp->EditorFrame->ViewportConfigData->GetViewportCount(); ++CurViewportIndex )
	{
		WxLevelViewportWindow* CurLevelViewportWindow =
			GApp->EditorFrame->ViewportConfigData->AccessViewport( CurViewportIndex ).ViewportWindow;
		if( CurLevelViewportWindow != NULL )
		{
			if( CurLevelViewportWindow->ViewportType == LVT_Perspective )
			{				
				if( bEnable )
				{
					CurLevelViewportWindow->SetRealtime( bEnable, bStoreCurrentState );
				}
				else
				{
					CurLevelViewportWindow->RestoreRealtime(TRUE);
				}
			}
		}
	}
}

// Region
UBOOL FLandscapeUISettings::GetbUseSelectedRegion() { return bUseSelectedRegion; }
void FLandscapeUISettings::SetbUseSelectedRegion(UBOOL InbUseSelectedRegion)
{ 
	bUseSelectedRegion = InbUseSelectedRegion;
	if (bUseSelectedRegion)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::Mask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::Mask);
	}
}
UBOOL FLandscapeUISettings::GetbUseNegativeMask() { return bUseNegativeMask; }
void FLandscapeUISettings::SetbUseNegativeMask(UBOOL InbUseNegativeMask) 
{ 
	bUseNegativeMask = InbUseNegativeMask; 
	if (bUseNegativeMask)
	{
		GLandscapeEditRenderMode |= ELandscapeEditRenderMode::InvertedMask;
	}
	else
	{
		GLandscapeEditRenderMode &= ~(ELandscapeEditRenderMode::InvertedMask);
	}
}

void FLandscapeUISettings::SetPasteMode(ELandscapeToolNoiseMode::Type InPasteMode)
{
	GLandscapePreviewMeshRenderMode = PasteMode = InPasteMode;
}

/** Load UI settings from ini file */
void FLandscapeUISettings::Load()
{
	FString WindowPositionString;
	if( GConfig->GetString( TEXT("LandscapeEdit"), TEXT("WindowPosition"), WindowPositionString, GEditorUserSettingsIni ) )
	{
		TArray<FString> PositionValues;
		if( WindowPositionString.ParseIntoArray( &PositionValues, TEXT( "," ), TRUE ) == 4 )
		{
			WindowX = appAtoi( *PositionValues(0) );
			WindowY = appAtoi( *PositionValues(1) );
			WindowWidth = appAtoi( *PositionValues(2) );
			WindowHeight = appAtoi( *PositionValues(3) );
		}
	}

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("WeightTargetValue"), WeightTargetValue, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bUseWeightTargetValue"), bUseWeightTargetValue, GEditorUserSettingsIni );

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("BrushComponentSize"), BrushComponentSize, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bUseClayBrush"), bUseClayBrush, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorUserSettingsIni );
	GConfig->GetString( TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), AlphaTextureName, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), AlphaTextureChannel, GEditorUserSettingsIni );
	SetAlphaTexture(*AlphaTextureName, AlphaTextureChannel);

	INT InFlattenMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("FlattenMode"), InFlattenMode, GEditorUserSettingsIni );
	FlattenMode = (ELandscapeToolNoiseMode::Type)InFlattenMode;
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bUseSlopeFlatten"), bUseSlopeFlatten, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bPickValuePerApply"), bPickValuePerApply, GEditorUserSettingsIni );

	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorUserSettingsIni );
	INT InErosionNoiseMode = ELandscapeToolNoiseMode::Sub; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), InErosionNoiseMode, GEditorUserSettingsIni );
	ErosionNoiseMode = (ELandscapeToolNoiseMode::Type)InErosionNoiseMode;
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorUserSettingsIni );

	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorUserSettingsIni );
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), HErodeIterationNum, GEditorUserSettingsIni );
	INT InRainDistMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("RainDistNoiseMode"), InRainDistMode, GEditorUserSettingsIni );
	RainDistMode = (ELandscapeToolNoiseMode::Type)InRainDistMode;
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorUserSettingsIni );
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), bHErosionDetailSmooth, GEditorUserSettingsIni );

	INT InNoiseMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("NoiseMode"), InNoiseMode, GEditorUserSettingsIni );
	NoiseMode = (ELandscapeToolNoiseMode::Type)InNoiseMode;
	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorUserSettingsIni );

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), bDetailSmooth, GEditorUserSettingsIni );

	GConfig->GetFloat( TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorUserSettingsIni );

	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bSmoothGizmoBrush"), bSmoothGizmoBrush, GEditorUserSettingsIni );

	INT InPasteMode = ELandscapeToolNoiseMode::Both; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("PasteMode"), InPasteMode, GEditorUserSettingsIni );
	//PasteMode = (ELandscapeToolNoiseMode::Type)InPasteMode;
	SetPasteMode((ELandscapeToolNoiseMode::Type)InPasteMode);

	INT InConvertMode = ELandscapeConvertMode::Expand; 
	GConfig->GetInt( TEXT("LandscapeEdit"), TEXT("ConvertMode"), InConvertMode, GEditorUserSettingsIni );
	SetConvertMode((ELandscapeConvertMode::Type)InConvertMode);

	// Region
	//GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bUseSelectedRegion"), bUseSelectedRegion, GEditorUserSettingsIni );
	//GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bUseNegativeMask"), bUseNegativeMask, GEditorUserSettingsIni );
	GConfig->GetBool( TEXT("LandscapeEdit"), TEXT("bApplyToAllTargets"), bApplyToAllTargets, GEditorUserSettingsIni );

	// Set EditRenderMode
	SetbUseSelectedRegion(bUseSelectedRegion);
	SetbUseNegativeMask(bUseNegativeMask);

	// Gizmo History (not saved!)
	GizmoHistories.Empty();
	for (FActorIterator It; It; ++It)
	{
		ALandscapeGizmoActor* Gizmo = Cast<ALandscapeGizmoActor>(*It);
		if( Gizmo && !Gizmo->bEditable )
		{
			new(GizmoHistories) FGizmoHistory(Gizmo);
		}
	}
}

/** Save UI settings to ini file */
void FLandscapeUISettings::Save()
{
	FString WindowPositionString = FString::Printf(TEXT("%d,%d,%d,%d"), WindowX, WindowY, WindowWidth, WindowHeight );
	GConfig->SetString( TEXT("LandscapeEdit"), TEXT("WindowPosition"), *WindowPositionString, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("ToolStrength"), ToolStrength, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("WeightTargetValue"), WeightTargetValue, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bUseWeightTargetValue"), bUseWeightTargetValue, GEditorUserSettingsIni );

	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("BrushRadius"), BrushRadius, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("BrushComponentSize"), BrushComponentSize, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("BrushFalloff"), BrushFalloff, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bUseClayBrush"), bUseClayBrush, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushScale"), AlphaBrushScale, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushRotation"), AlphaBrushRotation, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanU"), AlphaBrushPanU, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("AlphaBrushPanV"), AlphaBrushPanV, GEditorUserSettingsIni );
	GConfig->SetString( TEXT("LandscapeEdit"), TEXT("AlphaTextureName"), *AlphaTextureName, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("AlphaTextureChannel"), AlphaTextureChannel, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("FlattenMode"), FlattenMode, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bUseSlopeFlatten"), bUseSlopeFlatten, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bPickValuePerApply"), bPickValuePerApply, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErodeThresh"), ErodeThresh, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErodeIterationNum"), ErodeIterationNum, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErodeSurfaceThickness"), ErodeSurfaceThickness, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ErosionNoiseMode"), (INT)ErosionNoiseMode, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("ErosionNoiseScale"), ErosionNoiseScale, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("RainAmount"), RainAmount, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("SedimentCapacity"), SedimentCapacity, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("HErodeIterationNum"), ErodeIterationNum, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("RainDistMode"), (INT)RainDistMode, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("RainDistScale"), RainDistScale, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("HErosionDetailScale"), HErosionDetailScale, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bHErosionDetailSmooth"), bHErosionDetailSmooth, GEditorUserSettingsIni );

	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("NoiseMode"), (INT)NoiseMode, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("NoiseScale"), NoiseScale, GEditorUserSettingsIni );
	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("DetailScale"), DetailScale, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bDetailSmooth"), bDetailSmooth, GEditorUserSettingsIni );

	GConfig->SetFloat( TEXT("LandscapeEdit"), TEXT("MaximumValueRadius"), MaximumValueRadius, GEditorUserSettingsIni );

	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bSmoothGizmoBrush"), bSmoothGizmoBrush, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("PasteMode"), (INT)PasteMode, GEditorUserSettingsIni );
	GConfig->SetInt( TEXT("LandscapeEdit"), TEXT("ConvertMode"), (INT)ConvertMode, GEditorUserSettingsIni );
	//GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bUseSelectedRegion"), bUseSelectedRegion, GEditorUserSettingsIni );
	//GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bUseNegativeMask"), bUseNegativeMask, GEditorUserSettingsIni );
	GConfig->SetBool( TEXT("LandscapeEdit"), TEXT("bApplyToAllTargets"), bApplyToAllTargets, GEditorUserSettingsIni );
}

UBOOL FLandscapeUISettings::SetAlphaTexture(const TCHAR* InTextureName, INT InTextureChannel)
{
	UBOOL Result = TRUE;

	if( AlphaTexture )
	{
		AlphaTexture->RemoveFromRoot();
	}

	TArray<BYTE> NewTextureData;

	// Try to load specified texture.
	UTexture2D* NewAlphaTexture = NULL;
	if( InTextureName != NULL) 
	{
		NewAlphaTexture = LoadObject<UTexture2D>(NULL, InTextureName, NULL, LOAD_None, NULL);
	}

	// No texture or no source art, try to use the previous texture.
	if( NewAlphaTexture == NULL || !NewAlphaTexture->HasSourceArt() )
	{
		NewAlphaTexture = AlphaTexture;
		Result = FALSE;
	}

	// Use the previous texture
	if( NewAlphaTexture != NULL && NewAlphaTexture->HasSourceArt() )
	{
		NewAlphaTexture->GetUncompressedSourceArt(NewTextureData);
	}

	// Load fallback if there's no texture or data
	if( NewAlphaTexture == NULL || (NewTextureData.Num() != 4 * NewAlphaTexture->SizeX * NewAlphaTexture->SizeY) )
	{
		NewAlphaTexture = LoadObject<UTexture2D>(NULL, TEXT("EditorLandscapeResources.DefaultAlphaTexture"), NULL, LOAD_None, NULL);
		NewAlphaTexture->GetUncompressedSourceArt(NewTextureData);
		Result = FALSE;
	}

	check( NewAlphaTexture );
	AlphaTexture = NewAlphaTexture;
	AlphaTextureName = AlphaTexture->GetPathName();
	AlphaTextureSizeX = NewAlphaTexture->SizeX;
	AlphaTextureSizeY = NewAlphaTexture->SizeY;
	AlphaTextureChannel = InTextureChannel;
	AlphaTexture->AddToRoot();
	check( NewTextureData.Num() == 4 *AlphaTextureSizeX*AlphaTextureSizeY );

	AlphaTextureData.Empty(AlphaTextureSizeX*AlphaTextureSizeY);

	FLOAT UnpackMin;
	FLOAT UnpackMax;
	BYTE* SrcPtr;
	switch(AlphaTextureChannel)
	{
	case 1:
		SrcPtr = &((FColor*)&NewTextureData(0))->G;
		UnpackMin = NewAlphaTexture->UnpackMin[1];
		UnpackMax = NewAlphaTexture->UnpackMax[1];
		break;
	case 2:
		SrcPtr = &((FColor*)&NewTextureData(0))->B;
		UnpackMin = NewAlphaTexture->UnpackMin[2];
		UnpackMax = NewAlphaTexture->UnpackMax[2];
		break;
	case 3:
		SrcPtr = &((FColor*)&NewTextureData(0))->A;
		UnpackMin = NewAlphaTexture->UnpackMin[3];
		UnpackMax = NewAlphaTexture->UnpackMax[3];
		break;
	default:
		SrcPtr = &((FColor*)&NewTextureData(0))->R;
		UnpackMin = NewAlphaTexture->UnpackMin[0];
		UnpackMax = NewAlphaTexture->UnpackMax[0];
		break;
	}

	for( INT i=0;i<AlphaTextureSizeX*AlphaTextureSizeY;i++ )
	{
		BYTE Value = Clamp<INT>(appRound( 255.f * (UnpackMin + (UnpackMax-UnpackMin) * (FLOAT)(*SrcPtr) / 255.f)), 0, 255);
		AlphaTextureData.AddItem(Value);
		SrcPtr += 4;
	}

	return Result;
}

// Reimport
UBOOL ULandscapeInfo::ReimportHeightmap(INT DataSize, const WORD* DataPtr)
{
	INT MinX, MinY, MaxX, MaxY;
	if (GetLandscapeExtent(MinX, MinY, MaxX, MaxY) && DataSize == (1+MaxX-MinX)*(1+MaxY-MinY)*2)
	{
		FHeightmapAccessor<FALSE> HeightmapAccessor(this);
		HeightmapAccessor.SetData(MinX, MinY, MaxX, MaxY, DataPtr);
		return TRUE;
	}
	return FALSE;
}

UBOOL ULandscapeInfo::ReimportLayermap(FName LayerName, TArray<BYTE>& Data)
{
	INT MinX, MinY, MaxX, MaxY;
	if (GetLandscapeExtent(MinX, MinY, MaxX, MaxY) && Data.Num() == (1+MaxX-MinX)*(1+MaxY-MinY))
	{
		FAlphamapAccessor<FALSE, TRUE> AlphamapAccessor(this, LayerName);
		AlphamapAccessor.SetData(MinX, MinY, MaxX, MaxY, &Data(0));
		return TRUE;
	}
	return FALSE;
}

ALandscape* ULandscapeInfo::ChangeComponentSetting(INT VertsX, INT VertsY, INT InNumSubsections, INT InSubsectionSizeQuads)
{
	// Check if this setting is valid, Error check
	if( !
		(
		VertsX > 0 && VertsY > 0 &&
				((VertsX-1) % (InSubsectionSizeQuads * InNumSubsections)) == 0 &&
				((VertsY-1) % (InSubsectionSizeQuads * InNumSubsections)) == 0 )
		)
		
	{
		return NULL; // Don't do anything
	}

	ALandscape* Landscape = NULL;
	INT MinX, MinY, MaxX, MaxY;
	if (GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	{
		MaxX = MinX + VertsX - 1;
		MaxY = MinY + VertsY - 1;

		INT TMinX, TMinY, TMaxX, TMaxY;

		TMinX = MinX; TMinY = MinY; TMaxX = MaxX; TMaxY = MaxY;

		FLandscapeEditDataInterface LandscapeEdit(this);
		TArray<WORD> HeightData;
		HeightData.AddZeroed(VertsX*VertsY*sizeof(WORD));

		// This interface changes original value for cache interface usage.... hmm...
		LandscapeEdit.GetHeightData(TMinX, TMinY, TMaxX, MaxY, &HeightData(0), 0);

		TArray<FLandscapeLayerInfo> LayerInfos;
		TArray<TArray<BYTE> > LayerDataArrays;
		TArray<BYTE*> LayerDataPtrs;
		for ( TMap<FName, struct FLandscapeLayerStruct*>::TIterator It(LayerInfoMap); It; ++It )
		{
			FLandscapeLayerStruct* LayerStruct = It.Value();
			if (LayerStruct && LayerStruct->LayerInfoObj)
			{
				TMinX = MinX; TMinY = MinY; TMaxX = MaxX; TMaxY = MaxY;

				TArray<BYTE>* LayerData = new(LayerDataArrays)(TArray<BYTE>);
				LayerData->AddZeroed(VertsX*VertsY*sizeof(BYTE));
				LandscapeEdit.GetWeightData(It.Key(), TMinX, TMinY, TMaxX, TMaxY, &(*LayerData)(0), 0 );
				new(LayerInfos) FLandscapeLayerInfo(It.Key(), LayerStruct->LayerInfoObj->Hardness, LayerStruct->LayerInfoObj->bNoWeightBlend, *LayerStruct->SourceFilePath);
				LayerDataPtrs.AddItem(&(*LayerData)(0));
			}
		}

		if (LandscapeProxy)
		{
			Landscape = Cast<ALandscape>(GWorld->SpawnActor(ALandscape::StaticClass(), NAME_None, LandscapeProxy->Location, LandscapeProxy->Rotation));
			Landscape->SetDrawScale(LandscapeProxy->DrawScale);
			Landscape->SetDrawScale3D(LandscapeProxy->DrawScale3D);
			Landscape->LandscapeMaterial = LandscapeProxy->LandscapeMaterial;
			Landscape->Import( VertsX, VertsY, InNumSubsections*InSubsectionSizeQuads, InNumSubsections, InSubsectionSizeQuads,(WORD*)&HeightData(0), *HeightmapFilePath, LayerInfos, LayerDataPtrs.Num() ? &LayerDataPtrs(0) : NULL);
		}

		// Delete this Landscape...
		if (GWorld)
		{
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy && Proxy->LandscapeGuid == LandscapeGuid)
				{
					GWorld->DestroyActor(Proxy);
				}
			}
		}
	}
	return Landscape;
}
#if 0
ALandscape* ULandscapeInfo::DownsampleLandscape()
{
	ALandscape* Landscape = NULL;
	INT MinX, MinY, MaxX, MaxY;
	if (GetLandscapeExtent(MinX, MinY, MaxX, MaxY))
	
		INT VertsX = (MaxX - MinX);
		INT VertsY = (MaxY - MinY);

		INT TMinX, TMinY, TMaxX, TMaxY;
		TMinX = MinX; TMinY = MinY; TMaxX = MaxX; TMaxY = MaxY;

		FLandscapeEditDataInterface LandscapeEdit(this);
		TArray<WORD> HeightData;
		HeightData.AddZeroed(VertsX*VertsY*sizeof(WORD));

		// This interface changes original value for cache interface usage.... hmm...
		LandscapeEdit.GetHeightData(TMinX, TMinY, TMaxX, MaxY, &HeightData(0), 0);

		TArray<FLandscapeLayerInfo> LayerInfos;
		TArray<TArray<BYTE> > LayerDataArrays;
		TArray<BYTE*> LayerDataPtrs;
		for ( TMap<FName, struct FLandscapeLayerStruct*>::TIterator It(LayerInfoMap); It; ++It )
		{
			FLandscapeLayerStruct* LayerStruct = It.Value();
			if (LayerStruct && LayerStruct->LayerInfoObj)
			{
				TMinX = MinX; TMinY = MinY; TMaxX = MaxX; TMaxY = MaxY;

				TArray<BYTE>* LayerData = new(LayerDataArrays)(TArray<BYTE>);
				LayerData->AddZeroed(VertsX*VertsY*sizeof(BYTE));
				LandscapeEdit.GetWeightData(It.Key(), TMinX, TMinY, TMaxX, TMaxY, &(*LayerData)(0), 0 );
				new(LayerInfos) FLandscapeLayerInfo(It.Key(), LayerStruct->LayerInfoObj->Hardness, LayerStruct->LayerInfoObj->bNoWeightBlend, *LayerStruct->SourceFilePath);
				LayerDataPtrs.AddItem(&(*LayerData)(0));
			}
		}

		if (LandscapeProxy)
		{
			Landscape = Cast<ALandscape>(GWorld->SpawnActor(ALandscape::StaticClass(), NAME_None, LandscapeProxy->Location, LandscapeProxy->Rotation));
			Landscape->SetDrawScale(LandscapeProxy->DrawScale);
			Landscape->SetDrawScale3D(LandscapeProxy->DrawScale3D * 2.f);
			Landscape->LandscapeMaterial = LandscapeProxy->LandscapeMaterial;

			INT NewSubsectionSizeQuads = ((LandscapeProxy->SubsectionSizeQuads+1)>>1)-1;
			INT NewComponentSizeQuads = LandscapeProxy->NumSubsections*NewSubsectionSizeQuads;
			INT ComponentsX = (MaxX - MinX) / LandscapeProxy->ComponentSizeQuads;
			INT ComponentsY = (MaxY - MinY) / LandscapeProxy->ComponentSizeQuads;
			
			Landscape->Import( ComponentsX*NewComponentSizeQuads + 1, ComponentsY*NewComponentSizeQuads + 1, NewComponentSizeQuads, LandscapeProxy->NumSubsections, NewSubsectionSizeQuads,(WORD*)&HeightData(0), *HeightmapFilePath, LayerInfos, LayerDataPtrs.Num() ? &LayerDataPtrs(0) : NULL);
		}

		// Delete this Landscape...
		if (GWorld)
		{
			for (FActorIterator It; It; ++It)
			{
				ALandscapeProxy* Proxy = Cast<ALandscapeProxy>(*It);
				if (Proxy && Proxy->LandscapeGuid == LandscapeGuid)
				{
					GWorld->DestroyActor(Proxy);
				}
			}
		}
	}
	return Landscape;
}
#endif