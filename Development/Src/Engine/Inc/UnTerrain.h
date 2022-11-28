/*=============================================================================
	UnTerrain.h: New terrain
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Jack Porter
=============================================================================*/

#define TERRAIN_MAXTESSELATION	16

#define TERRAIN_ZSCALE			(1.0f/128.0f)

#define TERRAIN_UV_XY	2
#define TERRAIN_UV_XZ	3
#define TERRAIN_UV_YZ	4

//
//	FStaticTerrainLight
//

struct FStaticTerrainLight: FStaticLightPrimitive, FTexture2D
{
	DECLARE_RESOURCE_TYPE(FStaticTerrainLight,FTexture2D);

	class UTerrainComponent*	TerrainComponent;
	ULightComponent*			Light;
	TArray<BYTE>				ShadowData;

	// Constructors.

	FStaticTerrainLight(UTerrainComponent* InTerrainComponent,ULightComponent* InLight,INT InSizeX,INT InSizeY);
	FStaticTerrainLight();

	// Accessors.

	FORCEINLINE const BYTE& Shadow(INT X,INT Y) const;
	FLOAT FilteredShadow(INT IntX,FLOAT FracX,INT IntY,FLOAT FracY) const;

	// FTexture2D interface.

	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY);
	virtual UBOOL GetClampX() { return 1; }
	virtual UBOOL GetClampY() { return 1; }

	// FStaticLightPrimitive interface.

	virtual UPrimitiveComponent* GetSource() { return (UPrimitiveComponent*)TerrainComponent; }
	virtual void Render(const struct FSceneContext& Context,FPrimitiveViewInterface* PVI,FStaticLightPrimitiveRenderInterface* PRI);

	// FResource interface.

	virtual FString DescribeResource() { return FString::Printf(TEXT("Terrain lightmap(%s)"),*Light->GetPathName()); }

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FStaticTerrainLight& L);
};

//
//	FTerrainPatchBounds
//

struct FTerrainPatchBounds
{
	FLOAT	MinHeight,
			MaxHeight,
			MaxDisplacement;
};

//
//	FTerrainMaterialMask - Contains a bitmask for selecting weighted materials on terrain.
//

struct FTerrainMaterialMask
{
protected:

	DWORD*	Bits;
	UINT	NumBits;

public:

	// operator=

	void operator=(const FTerrainMaterialMask& InCopy)
	{
		check(NumBits == InCopy.NumBits);
		appMemcpy(Bits,InCopy.Bits,NumDWORDs() * sizeof(DWORD));
	}

	// Constructor/destructor.

	FTerrainMaterialMask(UINT InNumBits):
		NumBits(InNumBits)
	{
		Bits = new DWORD[NumDWORDs()];
		appMemzero(Bits,NumDWORDs() * sizeof(DWORD));
	}

	FTerrainMaterialMask(const FTerrainMaterialMask& InCopy):
		NumBits(InCopy.NumBits)
	{
		Bits = new DWORD[NumDWORDs()];
		*this = InCopy;
	}

	~FTerrainMaterialMask()
	{
		delete Bits;
	}

	// Accessors.

	UINT Num() const { return NumBits; }
	UINT NumDWORDs() const { return (NumBits + 31) / 32; }
	UBOOL Get(UINT Index) const { return Bits[Index / 32] & (1 << (Index & 31)); }
	void Set(UINT Index,UBOOL Value) { if(Value) Bits[Index / 32] |= 1 << (Index & 31); else Bits[Index / 32] &= ~(1 << (Index & 31)); }

	// operator==

	UBOOL operator==(const FTerrainMaterialMask& OtherMask) const
	{
		check(NumDWORDs() == OtherMask.NumDWORDs());
		for(UINT Index = 0;Index < NumDWORDs();Index++)
			if(Bits[Index] != OtherMask.Bits[Index])
				return 0;
		return 1;
	}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FTerrainMaterialMask& M)
	{
		Ar << M.NumBits;

		if(Ar.IsLoading())
		{
			delete M.Bits;
			M.Bits = new DWORD[M.NumDWORDs()];
		}

		for(UINT Index = 0;Index < M.NumDWORDs();Index++)
			Ar << M.Bits[Index];

		return Ar;
	}
};

//
//	UTerrainComponent
//

class UTerrainComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UTerrainComponent,UPrimitiveComponent,0,Engine);

	TIndirectArray<FStaticTerrainLight>	StaticLights;
	TArray<ULightComponent*>			IgnoreLights;

	struct FTerrainObject*	TerrainObject;

	INT						SectionBaseX,
							SectionBaseY,
							SectionSizeX,
							SectionSizeY;

	TArray<FTerrainPatchBounds>		PatchBounds;
	TArray<INT>						PatchBatches;	// Indices into the batches array.
	TArray<FTerrainMaterialMask>	BatchMaterials;	// A list of terrain material masks used by patches in this component.
	INT								FullBatch;		// A batch index containing all materials used by patches in this component.

	// UObject interface.

	void Serialize( FArchive& Ar );
	virtual void PostLoad();

	// UPrimitiveComponent interface.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);
	virtual void UpdateBounds();

	virtual FPrimitiveViewInterface* CreateViewInterface(const struct FSceneContext& Context);

	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Update();
	virtual void Destroyed();

	virtual void Precache(FPrecacheInterface* P);

	virtual void InvalidateLightingCache();

	// UPrimitiveComponent interface.

	virtual EStaticLightingAvailability GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive);
	virtual void CacheLighting();
	virtual void InvalidateLightingCache(ULightComponent* Light);

	// Init

	void Init(INT InBaseX,INT InBaseY,INT InSizeX,INT InSizeY);

	// UpdatePatchBounds

	void UpdatePatchBounds();

	// GetTerrain

	class ATerrain* GetTerrain() const { return CastChecked<ATerrain>(GetOuter()); }

	// GetLocalVertex - Returns a vertex in the component's local space.

	FVector GetLocalVertex(INT X,INT Y) const;

	// GetWorldVertex - Returns a vertex in the component's local space.

	FVector GetWorldVertex(INT X,INT Y) const;
};

//
//	FNoiseParameter
//

struct FNoiseParameter
{
	FLOAT	Base,
			NoiseScale,
			NoiseAmount;

	// Constructors.

	FNoiseParameter() {}
	FNoiseParameter(FLOAT InBase,FLOAT InScale,FLOAT InAmount):
		Base(InBase),
		NoiseScale(InScale),
		NoiseAmount(InAmount)
	{}

	// Sample

	FLOAT Sample(INT X,INT Y) const;

	// TestGreater - Returns 1 if TestValue is greater than the parameter.

	UBOOL TestGreater(INT X,INT Y,FLOAT TestValue) const;

	// TestLess
	
	UBOOL TestLess(INT X,INT Y,FLOAT TestValue) const { return !TestGreater(X,Y,TestValue); }
};

//
//	FTerrainHeight - Needs to be a struct to mirror the UnrealScript Terrain definition properly.
//

struct FTerrainHeight
{
	_WORD	Value;

	// Constructor.

	FTerrainHeight() {}
	FTerrainHeight(_WORD InValue): Value(InValue) {}

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FTerrainHeight& H)
	{
		return Ar << H.Value;
	}
};

//
//	FTerrainWeightedMaterial
//

struct FTerrainWeightedMaterial: FTexture2D
{
	class ATerrain*			Terrain;
	TArray<BYTE>			Data;
	UBOOL					Highlighted;

	class UTerrainMaterial*	Material;

	// Constructors.

	FTerrainWeightedMaterial();
	FTerrainWeightedMaterial(ATerrain* Terrain,const TArray<BYTE>& InData,class UTerrainMaterial* InMaterial,UBOOL InHighlighted);

	// Accessors.

	FORCEINLINE const BYTE& Weight(INT X,INT Y) const;
	FORCEINLINE FLOAT FilteredWeight(INT IntX,FLOAT FracX,INT IntY,FLOAT FracY) const;

	// FTexture2D interface.

	virtual void GetData(void* Buffer,UINT MipIndex,UINT StrideY);
	virtual UBOOL GetClampX() { return 1; }
	virtual UBOOL GetClampY() { return 1; }

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FTerrainWeightedMaterial& M);
};

//
//	FFilterLimit
//

struct FFilterLimit
{
	BITFIELD		Enabled : 1 GCC_PACK(4);
	FNoiseParameter	Noise;

	UBOOL TestLess(INT X,INT Y,FLOAT TestValue) const { return Enabled && Noise.TestLess(X,Y,TestValue); }
	UBOOL TestGreater(INT X,INT Y,FLOAT TestValue) const { return Enabled && Noise.TestGreater(X,Y,TestValue); }
};

//
//	FTerrainFilteredMaterial
//

struct FTerrainFilteredMaterial
{
	BITFIELD				UseNoise : 1;
	FLOAT					NoiseScale,
							NoisePercent;

	FFilterLimit			MinHeight;
	FFilterLimit			MaxHeight;

	FFilterLimit			MinSlope;
	FFilterLimit			MaxSlope;

	FLOAT					Alpha;
	class UTerrainMaterial*	Material;

	// BuildWeightMap - Filters a base weightmap based on the parameters and applies the weighted material to the terrain.  The base weightmap is also modified to remove the pixels used by this material.

	void BuildWeightMap(TArray<BYTE>& BaseWeightMap,UBOOL Highlighted,class ATerrain* Terrain,INT MinX,INT MinY,INT MaxX,INT MaxY) const;
};

//
//	FTerrainLayer
//

struct FTerrainLayer
{
	FString						Name;
	class UTerrainLayerSetup*	Setup;
	INT							AlphaMapIndex;
	BITFIELD					Highlighted : 1 GCC_PACK(4);
	BITFIELD					Hidden : 1;
};

//
//	FAlphaMap
//

struct FAlphaMap
{
	TArray<BYTE>	Data;

	friend FArchive& operator<<(FArchive& Ar,FAlphaMap& M)
	{
		return Ar << M.Data;
	}
};

//
//	FTerrainDecorationInstance
//

struct FTerrainDecorationInstance
{
	class UPrimitiveComponent*	Component;
	FLOAT						X,
								Y,
								Scale;
	INT							Yaw;
};

//
//	FTerrainDecoration
//

struct FTerrainDecoration
{
	class UPrimitiveComponentFactory*	Factory;
	FLOAT								MinScale,
										MaxScale,
										Density,
										SlopeRotationBlend;
	INT									RandSeed;

	TArrayNoInit<FTerrainDecorationInstance>	Instances;
};

//
//	FTerrainDecoLayer
//

struct FTerrainDecoLayer
{
	FStringNoInit						Name;
	TArrayNoInit<FTerrainDecoration>	Decorations;
	INT									AlphaMapIndex;
};

//
//	QuadLerp - Linearly interpolates between four adjacent terrain vertices, taking into account triangulation.
//

template<class T> T QuadLerp(const T& P00,const T& P10,const T& P01,const T& P11,FLOAT U,FLOAT V)
{
	if(U > V)
	{
		if(V < 1.0f)
			return Lerp(Lerp(P00,P11,V),Lerp(P10,P11,V),(U - V) / (1.0f - V));
		else
			return P11;
	}
	else
	{
		if(V > 0.0f)
			return Lerp(Lerp(P00,P01,V),Lerp(P00,P11,V),U / V);
		else
			return P00;
	}
}

//
//	FTerrainPatch - A displaced bicubic fit to 4x4 terrain samples.
//

struct FTerrainPatch
{
	FLOAT	Heights[4][4];

	FTerrainPatch() {}
};

//
//	FPatchSampler - Used to sample a FTerrainPatch at a fixed frequency.
//

struct FPatchSampler
{
	FLOAT	CubicBasis[TERRAIN_MAXTESSELATION + 1][4];
	FLOAT	CubicBasisDeriv[TERRAIN_MAXTESSELATION + 1][4];
	UINT	MaxTesselation;

	// Constructor.

	FPatchSampler(UINT InMaxTesselation);

	// Sample - Samples a terrain patch's height at a given position.

	FLOAT Sample(const FTerrainPatch& Patch,UINT X,UINT Y) const;

	// SampleDerivX - Samples a terrain patch's dZ/dX at a given position.

	FLOAT SampleDerivX(const FTerrainPatch& Patch,UINT X,UINT Y) const;

	// SampleDerivY - Samples a terrain patch's dZ/dY at a given position.

	FLOAT SampleDerivY(const FTerrainPatch& Patch,UINT Y,UINT X) const;

private:

	FLOAT Cubic(FLOAT P0,FLOAT P1,FLOAT P2,FLOAT P3,UINT I) const;
	FLOAT CubicDeriv(FLOAT P0,FLOAT P1,FLOAT P2,FLOAT P3,UINT I) const;
};

//
//	FTerrainMaterialResource
//

struct FTerrainMaterialResource: FMaterial
{
	ATerrain*				Terrain;
	FTerrainMaterialMask	Mask;
	FGuid					PersistentGuid;

	// Constructors.

	FTerrainMaterialResource(): Mask(0) {}
	FTerrainMaterialResource(ATerrain* InTerrain,const FTerrainMaterialMask& InMask):
		Terrain(InTerrain),
		Mask(InMask),
		PersistentGuid(appCreateGuid())
	{}

	// CompileTerrainMaterial - Compiles a single terrain material.

	INT CompileTerrainMaterial(EMaterialProperty Property,FMaterialCompiler* Compiler,UTerrainMaterial* TerrainMaterial,UBOOL Highlighted);

	// FMaterial interface.

	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler);

	// FResource interface.

	virtual FGuid GetPersistentId() { return PersistentGuid; }

	// Serializer

	friend FArchive& operator<<(FArchive& Ar,FTerrainMaterialResource& R)
	{
		return Ar << (UObject*&)R.Terrain << R.Mask << R.PersistentGuid;
	}
};

//
//	FTerrainMaterialInstancePointer
//

typedef FMaterialInstance*	FTerrainMaterialInstancePointer;

#include "EngineTerrainClasses.h"