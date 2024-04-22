/*=============================================================================
LandscapeEdit.h: Classes for the editor to access to Landscape data
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _LANDSCAPEEDIT_H
#define _LANDSCAPEEDIT_H

#if WITH_EDITOR

struct FLandscapeTextureDataInfo
{
	struct FMipInfo
	{
		void* MipData;
		TArray<FUpdateTextureRegion2D> MipUpdateRegions;
	};

	FLandscapeTextureDataInfo(UTexture2D* InTexture);
	virtual ~FLandscapeTextureDataInfo();

	// returns TRUE if we need to block on the render thread before unlocking the mip data
	UBOOL UpdateTextureData();

	INT NumMips() { return MipInfo.Num(); }

	void AddMipUpdateRegion(INT MipNum, INT InX1, INT InY1, INT InX2, INT InY2)
	{
		check( MipNum < MipInfo.Num() );
		new(MipInfo(MipNum).MipUpdateRegions) FUpdateTextureRegion2D(InX1, InY1, InX1, InY1, 1+InX2-InX1, 1+InY2-InY1);
	}

	void* GetMipData(INT MipNum)
	{
		check( MipNum < MipInfo.Num() );
		if( !MipInfo(MipNum).MipData )
		{
			MipInfo(MipNum).MipData = Texture->Mips(MipNum).Data.Lock(LOCK_READ_WRITE);
		}
		return MipInfo(MipNum).MipData;
	}

private:
	UTexture2D* Texture;
	TArray<FMipInfo> MipInfo;
};

struct FLandscapeEditDataInterface
{
	// tors
	FLandscapeEditDataInterface(ULandscapeInfo* InLandscape);
	virtual ~FLandscapeEditDataInterface();

	// Misc
	UBOOL GetComponentsInRegion(INT X1, INT Y1, INT X2, INT Y2, TSet<ULandscapeComponent*>* OutComponents = NULL);

	//
	// Heightmap access
	//
	void SetHeightData(INT X1, INT Y1, INT X2, INT Y2, const WORD* Data, INT Stride, UBOOL CalcNormals, const WORD* NormalData = NULL, UBOOL CreateComponents=FALSE);

	// Helper accessor
	FORCEINLINE WORD GetHeightMapData(const ULandscapeComponent* Component, INT TexU, INT TexV, FColor* TextureData = NULL);
	// Generic
	template<typename TStoreData>
	void GetHeightDataTempl(INT& X1, INT& Y1, INT& X2, INT& Y2, TStoreData& StoreData);
	// Without data interpolation, able to get normal data
	template<typename TStoreData>
	void GetHeightDataTemplFast(const INT X1, const INT Y1, const INT X2, const INT Y2, TStoreData& StoreData, TStoreData* NormalData = NULL);
	// Implementation for fixed array
	void GetHeightData(INT& X1, INT& Y1, INT& X2, INT& Y2, WORD* Data, INT Stride);
	void GetHeightDataFast(const INT X1, const INT Y1, const INT X2, const INT Y2, WORD* Data, INT Stride, WORD* NormalData = NULL);
	// Implementation for sparse array
	void GetHeightData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, WORD>& SparseData);
	void GetHeightDataFast(const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, WORD>& SparseData, TMap<QWORD, WORD>* NormalData = NULL);

	// Recaclulate normals for the entire landscape.
	void RecalculateNormals();

	//
	// Weightmap access
	//
	// Helper accessor
	FORCEINLINE BYTE GetWeightMapData(const ULandscapeComponent* Component, FName LayerName, INT TexU, INT TexV, BYTE Offset = 0, UTexture2D* Texture = NULL, BYTE* TextureData = NULL);
	template<typename TStoreData>
	void GetWeightDataTempl(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, TStoreData& StoreData);
	// Without data interpolation
	template<typename TStoreData>
	void GetWeightDataTemplFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TStoreData& StoreData);
	// Implementation for fixed array
	void GetWeightData(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, BYTE* Data, INT Stride);
	//void GetWeightData(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, TArray<BYTE>* Data, INT Stride);
	void GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, BYTE* Data, INT Stride);
	void GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TArray<BYTE>* Data, INT Stride);
	// Implementation for sparse array
	void GetWeightData(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, BYTE>& SparseData);
	//void GetWeightData(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, TArray<BYTE>>& SparseData);
	void GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, BYTE>& SparseData);
	void GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, TArray<BYTE>>& SparseData);
	// Updates weightmaps for all layers, treating the data as if LayerName is the topmost of multiple alpha maps.
	// Stores the other layers' previous weights as alpha map history in Components' EditingAlphaLayerData array.
	void SetAlphaData(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, const BYTE* Data, INT Stride, UBOOL bWeightAdjust = TRUE, UBOOL bTotalWeightAdjust = FALSE, TSet<FName>* DirtyLayerNames = NULL);
	// Delete a layer and re-normalize other layers
	void DeleteLayer(FName LayerName);

	// Without data interpolation, Select Data 
	template<typename TStoreData>
	void GetSelectDataTempl(const INT X1, const INT Y1, const INT X2, const INT Y2, TStoreData& StoreData);
	void GetSelectData(const INT X1, const INT Y1, const INT X2, const INT Y2, BYTE* Data, INT Stride);
	void GetSelectData(const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, BYTE>& SparseData);
	void SetSelectData(const INT X1, const INT Y1, const INT X2, const INT Y2, const BYTE* Data, INT Stride);

	// Texture data access
	FLandscapeTextureDataInfo* GetTextureDataInfo(UTexture2D* Texture);

	// Flush texture updates
	void Flush();

	// Texture bulk operations for weightmap reallocation
	void CopyTextureChannel(UTexture2D* Dest, INT DestChannel, UTexture2D* Src, INT SrcChannel);
	void ZeroTextureChannel(UTexture2D* Dest, INT DestChannel);
	void ZeroTexture(UTexture2D* Dest);

private:
	INT ComponentSizeQuads;
	INT SubsectionSizeQuads;
	INT ComponentNumSubsections;
	FVector DrawScale;

	TMap<UTexture2D*, FLandscapeTextureDataInfo*> TextureDataMap;
	ULandscapeInfo* LandscapeInfo;

	// Only for Missing Data interpolation... only internal usage
	template<typename TData, typename TStoreData>
	FORCEINLINE void CalcMissingValues(const INT& X1, const INT& X2, const INT& Y1, const INT& Y2, 
		const INT& ComponentIndexX1, const INT& ComponentIndexX2, const INT& ComponentIndexY1, const INT& ComponentIndexY2, 
		const INT& ComponentSizeX, const INT& ComponentSizeY, TData* CornerValues, 
		TArray<UBOOL>& NoBorderY1, TArray<UBOOL>& NoBorderY2, TArray<UBOOL>& ComponentDataExist, TStoreData& StoreData);
};

#endif

#endif // _LANDSCAPEEDIT_H
