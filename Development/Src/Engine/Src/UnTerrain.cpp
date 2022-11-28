/*=============================================================================
	UnTerrain.cpp: New terrain
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Jack Porter
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"
#include "UnCollision.h"

IMPLEMENT_RESOURCE_TYPE(FStaticTerrainLight);

IMPLEMENT_CLASS(ATerrain);
IMPLEMENT_CLASS(UTerrainComponent);
IMPLEMENT_CLASS(UTerrainMaterial);
IMPLEMENT_CLASS(UTerrainLayerSetup);

static FPatchSampler	GCollisionPatchSampler(TERRAIN_MAXTESSELATION);

//
//	PerlinNoise2D
//

FLOAT Fade(FLOAT T)
{
	return T * T * T * (T * (T * 6 - 15) + 10);
}

static int Permutations[256] =
{
	151,160,137,91,90,15,
	131,13,201,95,96,53,194,233,7,225,140,36,103,30,69,142,8,99,37,240,21,10,23,
	190, 6,148,247,120,234,75,0,26,197,62,94,252,219,203,117,35,11,32,57,177,33,
	88,237,149,56,87,174,20,125,136,171,168, 68,175,74,165,71,134,139,48,27,166,
	77,146,158,231,83,111,229,122,60,211,133,230,220,105,92,41,55,46,245,40,244,
	102,143,54, 65,25,63,161, 1,216,80,73,209,76,132,187,208, 89,18,169,200,196,
	135,130,116,188,159,86,164,100,109,198,173,186, 3,64,52,217,226,250,124,123,
	5,202,38,147,118,126,255,82,85,212,207,206,59,227,47,16,58,17,182,189,28,42,
	223,183,170,213,119,248,152, 2,44,154,163, 70,221,153,101,155,167, 43,172,9,
	129,22,39,253, 19,98,108,110,79,113,224,232,178,185, 112,104,218,246,97,228,
	251,34,242,193,238,210,144,12,191,179,162,241, 81,51,145,235,249,14,239,107,
	49,192,214, 31,181,199,106,157,184, 84,204,176,115,121,50,45,127, 4,150,254,
	138,236,205,93,222,114,67,29,24,72,243,141,128,195,78,66,215,61,156,180
};

FLOAT Grad(INT Hash,FLOAT X,FLOAT Y)
{
	INT		H = Hash & 15;
	FLOAT	U = H < 8 || H == 12 || H == 13 ? X : Y,
			V = H < 4 || H == 12 || H == 13 ? Y : 0;
	return ((H & 1) == 0 ? U : -U) + ((H & 2) == 0 ? V : -V);
}

FLOAT PerlinNoise2D(FLOAT X,FLOAT Y)
{
	INT		TruncX = appTrunc(X),
			TruncY = appTrunc(Y),
			IntX = TruncX & 255,
			IntY = TruncY & 255;
	FLOAT	FracX = X - TruncX,
			FracY = Y - TruncY;

	FLOAT	U = Fade(FracX),
			V = Fade(FracY);

	INT	A = Permutations[IntX] + IntY,
		AA = Permutations[A & 255],
		AB = Permutations[(A + 1) & 255],
		B = Permutations[(IntX + 1) & 255] + IntY,
		BA = Permutations[B & 255],
		BB = Permutations[(B + 1) & 255];

	return	Lerp(	Lerp(	Grad(Permutations[AA],			FracX,	FracY	),
							Grad(Permutations[BA],			FracX-1,FracY	),	U),
					Lerp(	Grad(Permutations[AB],			FracX,	FracY-1	),
							Grad(Permutations[BB],			FracX-1,FracY-1	),	U),	V);
}

//
//	FNoiseParameter::Sample
//

FLOAT FNoiseParameter::Sample(INT X,INT Y) const
{
	FLOAT	Noise = 0.0f;

	if(NoiseScale > DELTA)
	{
		for(UINT Octave = 0;Octave < 4;Octave++)
		{
			FLOAT	OctaveShift = 1 << Octave;
			FLOAT	OctaveScale = OctaveShift / NoiseScale;
			Noise += PerlinNoise2D(X * OctaveScale,Y * OctaveScale) / OctaveShift;
		}
	}

	return Base + Noise * NoiseAmount;
}

//
//	FNoiseParameter::TestGreater
//

UBOOL FNoiseParameter::TestGreater(INT X,INT Y,FLOAT TestValue) const
{
	FLOAT	ParameterValue = Base;

	if(NoiseScale > DELTA)
	{
		for(UINT Octave = 0;Octave < 4;Octave++)
		{
			FLOAT	OctaveShift = 1 << Octave;
			FLOAT	OctaveAmplitude = NoiseAmount / OctaveShift;

			// Attempt to avoid calculating noise if the test value is outside of the noise amplitude.

			if(TestValue > ParameterValue + OctaveAmplitude)
				return 1;
			else if(TestValue < ParameterValue - OctaveAmplitude)
				return 0;
			else
			{
				FLOAT	OctaveScale = OctaveShift / NoiseScale;
				ParameterValue += PerlinNoise2D(X * OctaveScale,Y * OctaveScale) * OctaveAmplitude;
			}
		}
	}

	return TestValue >= ParameterValue;
}

void ATerrain::Allocate()
{
	check(MaxComponentSize > 0);
	check(NumPatchesX > 0);
	check(NumPatchesY > 0);

	INT	OldNumVerticesX = NumVerticesX,
		OldNumVerticesY = NumVerticesY;

	// Calculate the new number of vertices in the terrain.
	NumVerticesX = NumPatchesX + 1;
	NumVerticesY = NumPatchesY + 1;

	// Initialize the terrain size.
	NumSectionsX = (NumPatchesX + MaxComponentSize - 1) / MaxComponentSize;
	NumSectionsY = (NumPatchesY + MaxComponentSize - 1) / MaxComponentSize;

	if(NumVerticesX != OldNumVerticesX || NumVerticesY != OldNumVerticesY)
	{
		// Allocate the height-map.
		TArray<FTerrainHeight>	NewHeights;
		NewHeights.Empty(NumVerticesX * NumVerticesY);
		for(INT Y = 0;Y < NumVerticesY;Y++)
		{
			for(INT X = 0;X < NumVerticesX;X++)
			{
				if(X < OldNumVerticesX && Y < OldNumVerticesY)
					new(NewHeights) FTerrainHeight(Heights(Y * OldNumVerticesX + X).Value);
				else
					new(NewHeights) FTerrainHeight(32768);
			}
		}
		Heights.Empty(NewHeights.Num());
		Heights.Add(NewHeights.Num());
		appMemcpy(&Heights(0),&NewHeights(0),NewHeights.Num() * sizeof(FTerrainHeight));

		// Allocate the alpha-maps.
		for(INT AlphaMapIndex = 0;AlphaMapIndex < AlphaMaps.Num();AlphaMapIndex++)
		{
			TArray<BYTE>	NewAlphas;
			NewAlphas.Empty(NumVerticesX * NumVerticesY);
			for(INT Y = 0;Y < NumVerticesY;Y++)
			{
				for(INT X = 0;X < NumVerticesX;X++)
				{
					if(X < OldNumVerticesX && Y < OldNumVerticesY)
						new(NewAlphas) BYTE(AlphaMaps(AlphaMapIndex).Data(Y * OldNumVerticesX + X));
					else
						new(NewAlphas) BYTE(0);
				}
			}
			AlphaMaps(AlphaMapIndex).Data.Empty(NewAlphas.Num());
			AlphaMaps(AlphaMapIndex).Data.Add(NewAlphas.Num());
			appMemcpy(&AlphaMaps(AlphaMapIndex).Data(0),&NewAlphas(0),NewAlphas.Num());
		}
	}

	// Delete existing components.
	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
	{
		check(!TerrainComponents(ComponentIndex)->Initialized);
		delete TerrainComponents(ComponentIndex);
	}
	TerrainComponents.Empty(NumSectionsX * NumSectionsY);

	// Create components.
	for(INT Y = 0;Y < NumSectionsY;Y++)
	{
		for(INT X = 0;X < NumSectionsX;X++)
		{
			UTerrainComponent*	S = ConstructObject<UTerrainComponent>(UTerrainComponent::StaticClass(),this);
			TerrainComponents.AddItem(S);
			S->Init(
				X * MaxComponentSize,
				Y * MaxComponentSize,
				Min(NumPatchesX - X * MaxComponentSize,MaxComponentSize),
				Min(NumPatchesY - Y * MaxComponentSize,MaxComponentSize)
				);
		}
	}
}

//
//	ATerrain::PostEditChange
//

void ATerrain::PostEditChange(UProperty* PropertyThatChanged)
{
	// Clamp the terrain size properties to valid values.
	NumPatchesX = Clamp(NumPatchesX,1,2048 - 1);
	NumPatchesY = Clamp(NumPatchesY,1,2048 - 1);
	MaxTesselationLevel = Min(1 << appCeilLogTwo(Max(MaxTesselationLevel,1)),TERRAIN_MAXTESSELATION);
	StaticLightingResolution = Min(1 << appCeilLogTwo(Max(StaticLightingResolution,1)),MaxTesselationLevel);
	MaxComponentSize = Clamp(MaxComponentSize,1,(256 - 1) / MaxTesselationLevel); // Limit MaxComponentSize from being 0, negative or large enough to exceed the maximum vertex buffer size.

	// Cleanup and unreferenced alpha maps.
	CompactAlphaMaps();

	// Reallocate height-map and alpha-map data with the new dimensions.
	Allocate();

	// Update the local to mapping transform for each material.
	for(UINT LayerIndex = 0;LayerIndex < (UINT)Layers.Num();LayerIndex++)
		if(Layers(LayerIndex).Setup)
			for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Layers(LayerIndex).Setup->Materials.Num();MaterialIndex++)
				if(Layers(LayerIndex).Setup->Materials(MaterialIndex).Material)
					Layers(LayerIndex).Setup->Materials(MaterialIndex).Material->UpdateMappingTransform();

	// Update cached weightmaps and presampled displacement maps.
	WeightedMaterials.Empty();
	CachedMaterials.Empty();
	CacheWeightMaps(0,0,NumVerticesX - 1,NumVerticesY - 1);
	CacheDisplacements(0,0,NumVerticesX - 1,NumVerticesY - 1);
	CacheDecorations(0,0,NumVerticesX - 1,NumVerticesY - 1);
	if(MaterialInstance)
	{
		delete MaterialInstance;
		MaterialInstance = NULL;
	}

	Super::PostEditChange(PropertyThatChanged);
}

//
//	ATerrain::PostLoad
//

void ATerrain::PostLoad()
{
	Super::PostLoad();

	// Remove terrain components from the main components array.
    for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		if(Components(ComponentIndex) && Components(ComponentIndex)->IsA(UTerrainComponent::StaticClass()))
		{
			Components.Remove(ComponentIndex--);
		}
	}

	if(!NumPatchesX || !NumPatchesY || !MaxComponentSize)
	{
		// For terrain actors saved without a size, use the default.
		NumPatchesX = NumSectionsX * SectionSize;
		NumPatchesY = NumSectionsY * SectionSize;
		NumVerticesX = NumPatchesX + 1;
		NumVerticesY = NumPatchesY + 1;
		MaxComponentSize = 16;
	}

	WeightedMaterials.Empty();
	CacheWeightMaps(0,0,NumVerticesX - 1,NumVerticesY - 1);
	CacheDisplacements(0,0,NumVerticesX - 1,NumVerticesY - 1);
}

//
//	ATerrain::Destroy
//

void ATerrain::Destroy()
{
	Super::Destroy();
	if(MaterialInstance)
	{
		delete MaterialInstance;
		MaterialInstance = NULL;
	}
}

//
//	ATerrain::Serialize
//
void ATerrain::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << Heights;
	Ar << AlphaMaps;

	if(!Ar.IsSaving() && !Ar.IsLoading())
		Ar << WeightedMaterials;

	Ar << CachedMaterials;
}

//
//	ATerrain::Spawned
//

void ATerrain::Spawned()
{
	NumPatchesX = 1;
	NumPatchesY = 1;
	MaxComponentSize = 16;
	DrawScale3D.X = DrawScale3D.Y = DrawScale3D.Z = 256.0f;

	// Allocate persistent terrain data.
	Allocate();

	// Update cached render data.
	PostEditChange(NULL);
}

//
//	ATerrain::ShouldTrace
//

UBOOL ATerrain::ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags)
{
	return (TraceFlags & TRACE_Terrain) ? 1 : 0;
}

//
//	ATerrain::ClearComponents
//

void ATerrain::ClearComponents()
{
	Super::ClearComponents();

	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
	{
		if(TerrainComponents(ComponentIndex))
		{
			if(TerrainComponents(ComponentIndex)->Initialized)
				TerrainComponents(ComponentIndex)->Destroyed();
			TerrainComponents(ComponentIndex)->Scene = NULL;
			TerrainComponents(ComponentIndex)->Owner = NULL;
		}
	}

	for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
	{
		for(UINT DecorationIndex = 0;DecorationIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations.Num();DecorationIndex++)
		{
			FTerrainDecoration&	Decoration = DecoLayers(DecoLayerIndex).Decorations(DecorationIndex);
			for(UINT InstanceIndex = 0;InstanceIndex < (UINT)Decoration.Instances.Num();InstanceIndex++)
			{
				FTerrainDecorationInstance&	DecorationInstance = Decoration.Instances(InstanceIndex);
				check(DecorationInstance.Component);
				if(DecorationInstance.Component->Initialized)
					DecorationInstance.Component->Destroyed();
				DecorationInstance.Component->Scene = NULL;
				DecorationInstance.Component->Owner = NULL;
			}
		}
	}

}

//
//	ATerrain::UpdateComponents
//

void ATerrain::UpdateComponents()
{
	Super::UpdateComponents();

	if(!XLevel || bDeleteMe)
		return;

	const FMatrix&	ActorToWorld = LocalToWorld();

	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
	{
		if(TerrainComponents(ComponentIndex))
		{
			TerrainComponents(ComponentIndex)->SetParentToWorld(ActorToWorld);

			if(!TerrainComponents(ComponentIndex)->Initialized)
			{
				TerrainComponents(ComponentIndex)->Scene = XLevel;
				TerrainComponents(ComponentIndex)->Owner = this;
				if(TerrainComponents(ComponentIndex)->IsValidComponent())
					TerrainComponents(ComponentIndex)->Created();
			}
			else
				TerrainComponents(ComponentIndex)->Update();
		}
	}

	for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
	{
		for(UINT DecorationIndex = 0;DecorationIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations.Num();DecorationIndex++)
		{
			FTerrainDecoration&	Decoration = DecoLayers(DecoLayerIndex).Decorations(DecorationIndex);

			for(UINT InstanceIndex = 0;InstanceIndex < (UINT)Decoration.Instances.Num();InstanceIndex++)
			{
				FTerrainDecorationInstance&	DecorationInstance = Decoration.Instances(InstanceIndex);
				check(DecorationInstance.Component);

				INT				IntX = appTrunc(DecorationInstance.X),
								IntY = appTrunc(DecorationInstance.Y),
								SubX = appTrunc((DecorationInstance.X - IntX) * MaxTesselationLevel),
								SubY = appTrunc((DecorationInstance.Y - IntY) * MaxTesselationLevel);
				FTerrainPatch	Patch = GetPatch(IntX,IntY);
				FVector			Location = ActorToWorld.TransformFVector(GetCollisionVertex(Patch,IntX,IntY,SubX,SubY));
				FRotator		Rotation(0,DecorationInstance.Yaw,0);
				FVector			TangentX = FVector(1,0,GCollisionPatchSampler.SampleDerivX(Patch,SubX,SubY) * TERRAIN_ZSCALE),
								TangentY = FVector(0,1,GCollisionPatchSampler.SampleDerivY(Patch,SubX,SubY) * TERRAIN_ZSCALE);

				DecorationInstance.Component->SetParentToWorld(
					FRotationMatrix(Rotation) *
					FRotationMatrix(
						Lerp(
							ActorToWorld.TransformNormal(FVector(1,0,0)).SafeNormal(),
							(TangentY ^ (TangentX ^ TangentY)).SafeNormal(),
							Clamp(Decoration.SlopeRotationBlend,0.0f,1.0f)
						).SafeNormal().Rotation()
						) *
					FScaleMatrix(FVector(1,1,1) * DecorationInstance.Scale) *
					FTranslationMatrix(Location)
					);

				if(!DecorationInstance.Component->Initialized)
				{
					DecorationInstance.Component->Scene = XLevel;
					DecorationInstance.Component->Owner = this;
					if(DecorationInstance.Component->IsValidComponent())
						DecorationInstance.Component->Created();
				}
				else
					DecorationInstance.Component->Update();
			}
		}
	}
}

//
//	ATerrain::CacheLighting
//

void ATerrain::CacheLighting()
{
	// Cache lighting for each terrain component.
	GWarn->BeginSlowTask(TEXT("Raytracing terrain"),1);
	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
	{
		GWarn->StatusUpdatef(ComponentIndex,(UINT)TerrainComponents.Num(),TEXT("Raytracing terrain"));
		if(TerrainComponents(ComponentIndex))
		{
			TerrainComponents(ComponentIndex)->CacheLighting();
		}
	}
	GWarn->EndSlowTask();

	for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
	{
		for(UINT DecorationIndex = 0;DecorationIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations.Num();DecorationIndex++)
		{
			for(UINT InstanceIndex = 0;InstanceIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations(DecorationIndex).Instances.Num();InstanceIndex++)
			{
				FTerrainDecorationInstance&	DecorationInstance = DecoLayers(DecoLayerIndex).Decorations(DecorationIndex).Instances(InstanceIndex);
				check(DecorationInstance.Component);

				DecorationInstance.Component->CacheLighting();
			}
		}
	}

}

//
//	ATerrain::InvalidateLightingCache
//

void ATerrain::InvalidateLightingCache()
{
	Super::InvalidateLightingCache();

	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
		if(TerrainComponents(ComponentIndex))
			TerrainComponents(ComponentIndex)->InvalidateLightingCache();

	for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
	{
		for(UINT DecorationIndex = 0;DecorationIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations.Num();DecorationIndex++)
		{
			for(UINT InstanceIndex = 0;InstanceIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations(DecorationIndex).Instances.Num();InstanceIndex++)
			{
				FTerrainDecorationInstance&	DecorationInstance = DecoLayers(DecoLayerIndex).Decorations(DecorationIndex).Instances(InstanceIndex);
				check(DecorationInstance.Component);
				
				((UActorComponent*)DecorationInstance.Component)->InvalidateLightingCache();
			}
		}
	}

}

//
//	ATerrain::CompactAlphaMaps
//

void ATerrain::CompactAlphaMaps()
{
	// Build a list of referenced alpha maps.

	TArray<INT>		ReferencedAlphaMaps;
	for(UINT LayerIndex = 0;LayerIndex < (UINT)Layers.Num();LayerIndex++)
		if(Layers(LayerIndex).AlphaMapIndex != INDEX_NONE)
			ReferencedAlphaMaps.AddItem(Layers(LayerIndex).AlphaMapIndex);
	for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
		if(DecoLayers(DecoLayerIndex).AlphaMapIndex != INDEX_NONE)
			ReferencedAlphaMaps.AddItem(DecoLayers(DecoLayerIndex).AlphaMapIndex);

	// If there are any unused alpha maps, remove them and remap indices.

	if(ReferencedAlphaMaps.Num() != AlphaMaps.Num())
	{
		TArray<FAlphaMap>	OldAlphaMaps = AlphaMaps;
		TMap<INT,INT>		IndexMap;
		AlphaMaps.Empty(ReferencedAlphaMaps.Num());
		for(UINT AlphaMapIndex = 0;AlphaMapIndex < (UINT)ReferencedAlphaMaps.Num();AlphaMapIndex++)
		{
			new(AlphaMaps) FAlphaMap(OldAlphaMaps(ReferencedAlphaMaps(AlphaMapIndex)));
			IndexMap.Set(ReferencedAlphaMaps(AlphaMapIndex),AlphaMapIndex);
		}

		for(UINT LayerIndex = 0;LayerIndex < (UINT)Layers.Num();LayerIndex++)
			if(Layers(LayerIndex).AlphaMapIndex != INDEX_NONE)
				Layers(LayerIndex).AlphaMapIndex = IndexMap.FindRef(Layers(LayerIndex).AlphaMapIndex);
		for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
			if(DecoLayers(DecoLayerIndex).AlphaMapIndex != INDEX_NONE)
				DecoLayers(DecoLayerIndex).AlphaMapIndex = IndexMap.FindRef(DecoLayers(DecoLayerIndex).AlphaMapIndex);
	}

}

//
//	FTerrainFilteredMaterial::BuildWeightMap
//

static FLOAT GetSlope(const FVector& A,const FVector& B)
{
	return Abs(B.Z - A.Z) * appInvSqrt(Square(B.X - A.X) + Square(B.Y - A.Y));
}

void FTerrainFilteredMaterial::BuildWeightMap(TArray<BYTE>& BaseWeightMap,UBOOL Highlighted,class ATerrain* Terrain,INT MinX,INT MinY,INT MaxX,INT MaxY) const
{
	if(!Material)
		return;

	INT	SizeX = MaxX - MinX + 1,
		SizeY = MaxY - MinY + 1;
	check(BaseWeightMap.Num() == SizeX * SizeY);

	FLOAT	ClampedAlpha = Clamp(Alpha,0.0f,1.0f);

	// Filter the weightmap.

	TArray<BYTE>	MaterialWeightMap;
	MaterialWeightMap.Add(BaseWeightMap.Num());

	for(INT Y = MinY;Y <= MaxY;Y++)
	{
		BYTE*	BaseWeightPtr = &BaseWeightMap((Y - MinY) * SizeX);
		BYTE*	MaterialWeightPtr = &MaterialWeightMap((Y - MinY) * SizeX);
		for(INT X = MinX;X <= MaxX;X++,MaterialWeightPtr++,BaseWeightPtr++)
		{
			*MaterialWeightPtr = 0;
			if(*BaseWeightPtr)
			{
				if(MaxSlope.Enabled || MinSlope.Enabled)
				{
					FVector	Vertex = Terrain->GetWorldVertex(X,Y);
					FLOAT	Slope = Max(
										Max(
											GetSlope(Terrain->GetWorldVertex(X - 1,Y - 1),Vertex),
											Max(
												GetSlope(Terrain->GetWorldVertex(X,Y - 1),Vertex),
												GetSlope(Terrain->GetWorldVertex(X + 1,Y - 1),Vertex)
												)
											),
										Max(
											Max(
												GetSlope(Terrain->GetWorldVertex(X - 1,Y),Vertex),
												Max(
													GetSlope(Terrain->GetWorldVertex(X,Y),Vertex),
													GetSlope(Terrain->GetWorldVertex(X + 1,Y),Vertex)
													)
												),
											Max(
												GetSlope(Terrain->GetWorldVertex(X + 1,Y - 1),Vertex),
												Max(
													GetSlope(Terrain->GetWorldVertex(X + 1,Y),Vertex),
													GetSlope(Terrain->GetWorldVertex(X + 1,Y + 1),Vertex)
													)
												)
											)
										);
					if(MaxSlope.TestGreater(X,Y,Slope) || MinSlope.TestLess(X,Y,Slope))
						continue;
				}

				if(MaxHeight.Enabled || MinHeight.Enabled)
				{
					FVector	Vertex = Terrain->GetWorldVertex(X,Y);
					if(MaxHeight.TestGreater(X,Y,Vertex.Z) || MinHeight.TestLess(X,Y,Vertex.Z))
						continue;
				}

				if(UseNoise && FNoiseParameter(0.5f,NoiseScale,1.0f).TestLess(X,Y,NoisePercent))
					continue;

				*MaterialWeightPtr = (BYTE)Clamp<INT>((FLOAT)*BaseWeightPtr * Alpha,0,255);
				*BaseWeightPtr -= *MaterialWeightPtr;
			}
		}
	}

	// Check for an existing weighted material to update.

	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Terrain->WeightedMaterials.Num();MaterialIndex++)
	{
		FTerrainWeightedMaterial&	WeightMap = Terrain->WeightedMaterials(MaterialIndex);
		if(WeightMap.Material == Material && WeightMap.Highlighted == Highlighted)
		{
			for(INT Y = MinY;Y <= MaxY;Y++)
				for(INT X = MinX;X <= MaxX;X++)
					Terrain->WeightedMaterials(MaterialIndex).Data(Y * Terrain->NumVerticesX + X) += MaterialWeightMap((Y - MinY) * SizeX + X - MinX);
			return;
		}
	}

	// If generating the entire weightmap, create a new weighted material.

	check(MinX == 0 && MaxX == Terrain->NumVerticesX - 1);
	check(MinY == 0 && MaxY == Terrain->NumVerticesY - 1);

	new(Terrain->WeightedMaterials) FTerrainWeightedMaterial(Terrain,MaterialWeightMap,Material,Highlighted);
}

//
//	ATerrain::CacheWeightMaps
//

void ATerrain::CacheWeightMaps(INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	INT	SizeX = (MaxX - MinX + 1),
		SizeY = (MaxY - MinY + 1);

	// Clear the update rectangle in the weightmaps.

	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)WeightedMaterials.Num();MaterialIndex++)
	{
		if(!WeightedMaterials(MaterialIndex).Data.Num())
		{
			check(MinX == 0 && MinY == 0 && MaxX == NumVerticesX - 1 && MaxY == NumVerticesY - 1);
			WeightedMaterials(MaterialIndex).Data.Add(NumVerticesX * NumVerticesY);
		}
		for(INT Y = MinY;Y <= MaxY;Y++)
			for(INT X = MinX;X <= MaxX;X++)
				WeightedMaterials(MaterialIndex).Data(Y * NumVerticesX + X) = 0;
	}

	// Build a base weightmap containing all texels set.

	TArray<BYTE>	BaseWeightMap(SizeX * SizeY);
	for(INT Y = MinY;Y <= MaxY;Y++)
		for(INT X = MinX;X <= MaxX;X++)
			BaseWeightMap((Y - MinY) * SizeX + X - MinX) = 255;

	for(INT LayerIndex = Layers.Num() - 1;LayerIndex >= 0;LayerIndex--)
	{
		// Build a layer weightmap containing the texels set in the layer's alphamap and the base weightmap, removing the texels set in the layer weightmap from the base weightmap.

		TArray<BYTE>	LayerWeightMap;
		LayerWeightMap.Empty(SizeX * SizeY);
		for(INT Y = MinY;Y <= MaxY;Y++)
		{
			for(INT X = MinX;X <= MaxX;X++)
			{
				FLOAT	LayerAlpha = LayerIndex ? ((FLOAT)Alpha(Layers(LayerIndex).AlphaMapIndex,X,Y) / 255.0f) : 1.0f;
				BYTE&	BaseWeight = BaseWeightMap((Y - MinY) * SizeX + X - MinX);
				BYTE	Weight = (BYTE)Clamp(appTrunc((FLOAT)BaseWeight * LayerAlpha),0,255);

				LayerWeightMap.AddItem(Weight);
				BaseWeight -= Weight;
			}
		}

		// Generate weightmaps for each filtered material in the layer.  BuildWeightMap resets the texels in LayerWeightMap that it sets in the material weightmap.

		if(Layers(LayerIndex).Setup && !Layers(LayerIndex).Hidden)
			for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Layers(LayerIndex).Setup->Materials.Num();MaterialIndex++)
				Layers(LayerIndex).Setup->Materials(MaterialIndex).BuildWeightMap(LayerWeightMap,Layers(LayerIndex).Highlighted,this,MinX,MinY,MaxX,MaxY);

		// Add the texels set in the layer weightmap but not reset by the layer's filtered materials back into the base weightmap.

		for(INT Y = MinY;Y <= MaxY;Y++)
			for(INT X = MinX;X <= MaxX;X++)
				BaseWeightMap((Y - MinY) * SizeX + X - MinX) += LayerWeightMap((Y - MinY) * SizeX + X - MinX);
	}

}

//
//	ATerrain::UpdateRenderData
//

void ATerrain::UpdateRenderData(INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	INT	SizeX = (MaxX - MinX + 1),
		SizeY = (MaxY - MinY + 1);

	// Generate the weightmaps.

	CacheWeightMaps(MinX,MinY,MaxX,MaxY);

	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)WeightedMaterials.Num();MaterialIndex++)
		GResourceManager->UpdateResource(&WeightedMaterials(MaterialIndex));

	// Cache displacements.

	CacheDisplacements(Max(MinX - 1,0),Max(MinY - 1,0),MaxX,MaxY);

	// Cache decorations.

	CacheDecorations(Max(MinX - 1,0),Max(MinY - 1,0),MaxX,MaxY);

	// Update the terrain components.

	for(UINT ComponentIndex = 0;ComponentIndex < (UINT)TerrainComponents.Num();ComponentIndex++)
	{
		UTerrainComponent*	Component = TerrainComponents(ComponentIndex);

		if(Component->SectionBaseX + Component->SectionSizeX >= MinX && Component->SectionBaseX <= MaxX && Component->SectionBaseY + Component->SectionSizeY >= MinY && Component->SectionBaseY <= MaxY)
		{
			// Discard any lightmap cached for the component.

			((UActorComponent*)Component)->InvalidateLightingCache();

			// Reinsert the component in the scene and update the vertex buffer.

			if(Component->Initialized)
			{
				Component->Destroyed();
				Component->Created();
			}
		}
	}

}

//
//	ATerrain::CacheDisplacements
//

void ATerrain::CacheDisplacements(INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	if(MinX == 0 && MinY == 0 && MaxX == NumVerticesX - 1 && MaxY == NumVerticesY - 1)
	{
		CachedDisplacements.Empty((NumPatchesX * MaxTesselationLevel + 1) * (NumPatchesY * MaxTesselationLevel + 1));
		CachedDisplacements.Add((NumPatchesX * MaxTesselationLevel + 1) * (NumPatchesY * MaxTesselationLevel + 1));
	}

	// Build a list of materials with displacement maps and calculate the maximum possible displacement.

	MaxCollisionDisplacement = 0.0f;

	TArray<UINT>	DisplacedMaterials;
	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)WeightedMaterials.Num();MaterialIndex++)
	{
		FTerrainWeightedMaterial&	WeightedMaterial = WeightedMaterials(MaterialIndex);
		if(WeightedMaterial.Material->DisplacementMap)
		{
			MaxCollisionDisplacement = Max(
				MaxCollisionDisplacement,
				Max(Abs(WeightedMaterial.Material->DisplacementMap->UnpackMin.W),Abs(WeightedMaterial.Material->DisplacementMap->UnpackMax.W)) *
					WeightedMaterial.Material->DisplacementScale
				);
			DisplacedMaterials.AddItem(MaterialIndex);
		}
	}

	for(INT VertexY = MinY * MaxTesselationLevel;VertexY <= MaxY * MaxTesselationLevel;VertexY++)
	{
		for(INT VertexX = MinX * MaxTesselationLevel;VertexX <= MaxX * MaxTesselationLevel;VertexX++)
		{
			INT		IntX = VertexX / MaxTesselationLevel,
					IntY = VertexY / MaxTesselationLevel;
			FLOAT	Displacement = 0.0f,
					X = (FLOAT)VertexX / (FLOAT)MaxTesselationLevel,
					Y = (FLOAT)VertexY / (FLOAT)MaxTesselationLevel,
					FracX = X - IntX,
					FracY = Y - IntY;
			for(UINT MaterialIndex = 0;MaterialIndex < (UINT)DisplacedMaterials.Num();MaterialIndex++)
			{
				// Compute the weight of this material on the current vertex.

				const FTerrainWeightedMaterial&	WeightedMaterial = WeightedMaterials(DisplacedMaterials(MaterialIndex));
				FLOAT							Weight = WeightedMaterial.FilteredWeight(
															IntX,
															FracX,
															IntY,
															FracY
															);

				if(Weight > 0.0f)
				{
					// Sample the material's displacement map.

					FVector	UV = WeightedMaterial.Material->LocalToMapping.TransformFVector(FVector(X,Y,0));
					Displacement += Weight / 255.0f * WeightedMaterial.Material->GetDisplacement(UV.X,UV.Y);
				}
			}

			// Quantize and store the displacement for this vertex.

			CachedDisplacements(VertexY * (NumPatchesX * MaxTesselationLevel + 1) + VertexX) = (BYTE)(Clamp<INT>(Displacement * 127.0f / MaxCollisionDisplacement,-127,128) + 127);
		}
	}

}

//
//	ATerrain::CacheDecorations
//

void ATerrain::CacheDecorations(INT MinX,INT MinY,INT MaxX,INT MaxY)
{
	for(UINT DecoLayerIndex = 0;DecoLayerIndex < (UINT)DecoLayers.Num();DecoLayerIndex++)
	{
		FTerrainDecoLayer&	DecoLayer = DecoLayers(DecoLayerIndex);
		for(UINT DecorationIndex = 0;DecorationIndex < (UINT)DecoLayers(DecoLayerIndex).Decorations.Num();DecorationIndex++)
		{
			FTerrainDecoration&	Decoration = DecoLayers(DecoLayerIndex).Decorations(DecorationIndex);

			// Clear old decorations in the update rectangle.

			for(INT InstanceIndex = 0;InstanceIndex < Decoration.Instances.Num();InstanceIndex++)
			{
				FTerrainDecorationInstance&	DecorationInstance = Decoration.Instances(InstanceIndex);
				if(DecorationInstance.X < MinX || DecorationInstance.X > MaxX || DecorationInstance.Y < MinY || DecorationInstance.Y > MaxY)
					continue;

				if(DecorationInstance.Component->Initialized)
					DecorationInstance.Component->Destroyed();
				DecorationInstance.Component->Scene = NULL;
				DecorationInstance.Component->Owner = NULL;

				Decoration.Instances.Remove(InstanceIndex--);
			}

			// Create new decorations.

			if(Decoration.Factory && Decoration.Factory->FactoryIsValid())
			{
				UINT	MaxInstances = (UINT)Max<INT>(Decoration.Density * NumPatchesX * NumPatchesY,0);

				appSRandInit(Decoration.RandSeed);

				for(UINT InstanceIndex = 0;InstanceIndex < MaxInstances;InstanceIndex++)
				{
					FLOAT	X = appSRand() * NumVerticesX,
							Y = appSRand() * NumVerticesY,
							Scale = appSRand();
					INT		IntX = appTrunc(X),
							IntY = appTrunc(Y),
							Yaw = appSRand() * 65536.0f;

					if(appSRand() * 255.0f <= Alpha(DecoLayer.AlphaMapIndex,IntX,IntY) && X >= MinX && X <= MaxX && Y >= MinY && Y <= MaxY)
					{
						FTerrainDecorationInstance*	DecorationInstance = new(Decoration.Instances) FTerrainDecorationInstance;
						DecorationInstance->Component = Decoration.Factory->CreatePrimitiveComponent(this);
						DecorationInstance->X = X;
						DecorationInstance->Y = Y;
						DecorationInstance->Yaw = Yaw;
						DecorationInstance->Scale = Lerp(Decoration.MinScale,Decoration.MaxScale,Scale);
					}
				}
			}
		}
	}

	UpdateComponents();
}

//
//	ATerrain::Height
//

const _WORD& ATerrain::Height(INT X,INT Y) const
{
	X = Clamp(X,0,NumVerticesX - 1);
	Y = Clamp(Y,0,NumVerticesY - 1);

	return Heights(Y * NumVerticesX + X).Value;
}

//
//	ATerrain::Height
//

_WORD& ATerrain::Height(INT X,INT Y)
{
	X = Clamp(X,0,NumVerticesX - 1);
	Y = Clamp(Y,0,NumVerticesY - 1);

	return Heights(Y * NumVerticesX + X).Value;
}

//
//	ATerrain::GetLocalVertex
//

FVector ATerrain::GetLocalVertex(INT X,INT Y) const
{
	return FVector(X,Y,(-32768.0f + (FLOAT)Height(X,Y)) * TERRAIN_ZSCALE);
}

//
//	ATerrain::GetWorldVertex
//

FVector ATerrain::GetWorldVertex(INT X,INT Y) const
{
	return LocalToWorld().TransformFVector(GetLocalVertex(X,Y));
}

//
//	ATerrain::GetPatch
//

FTerrainPatch ATerrain::GetPatch(INT X,INT Y) const
{
	FTerrainPatch	Result;

	for(INT SubY = 0;SubY < 4;SubY++)
		for(INT SubX = 0;SubX < 4;SubX++)
			Result.Heights[SubX][SubY] = Height(X - 1 + SubX,Y - 1 + SubY);

	return Result;
}

//
//	ATerrain::GetCollisionVertex
//

FVector ATerrain::GetCollisionVertex(const FTerrainPatch& Patch,UINT PatchX,UINT PatchY,UINT SubX,UINT SubY) const
{
	FLOAT	FracX = (FLOAT)SubX / (FLOAT)MaxTesselationLevel,
			FracY = (FLOAT)SubY / (FLOAT)MaxTesselationLevel;

	const FVector&	TangentZ = (FVector(1,0,GCollisionPatchSampler.SampleDerivX(Patch,SubX,SubY) * TERRAIN_ZSCALE) ^ FVector(0,1,GCollisionPatchSampler.SampleDerivY(Patch,SubX,SubY) * TERRAIN_ZSCALE)).UnsafeNormal();
	FLOAT			Displacement = GetCachedDisplacement(PatchX,PatchY,SubX,SubY);

	return FVector(
		PatchX + FracX,
		PatchY + FracY,
		(-32768.0f +
			GCollisionPatchSampler.Sample(
				Patch,
				SubX * TERRAIN_MAXTESSELATION / MaxTesselationLevel,
				SubY * TERRAIN_MAXTESSELATION / MaxTesselationLevel
				)
			) *
			TERRAIN_ZSCALE
		) +
			TangentZ * Displacement;
}

//
//	ATerrain::Alpha
//

const BYTE ATerrain::Alpha(INT AlphaMapIndex,INT X,INT Y) const
{
	if(AlphaMapIndex == INDEX_NONE)
		return 0;

	check(AlphaMapIndex >= 0 && AlphaMapIndex < AlphaMaps.Num());

	X = Clamp(X,0,NumVerticesX - 1);
	Y = Clamp(Y,0,NumVerticesY - 1);

	return AlphaMaps(AlphaMapIndex).Data(Y * NumVerticesX + X);
}

//
//	ATerrain::Alpha
//

BYTE& ATerrain::Alpha(INT& AlphaMapIndex,INT X,INT Y)
{
	if(AlphaMapIndex == INDEX_NONE)
	{
		AlphaMapIndex = AlphaMaps.Num();
		(new(AlphaMaps) FAlphaMap)->Data.AddZeroed(NumVerticesX * NumVerticesY);
	}

	check(AlphaMapIndex >= 0 && AlphaMapIndex < AlphaMaps.Num());

	X = Clamp(X,0,NumVerticesX - 1);
	Y = Clamp(Y,0,NumVerticesY - 1);

	return AlphaMaps(AlphaMapIndex).Data(Y * NumVerticesX + X);
}

//
//	ATerrain::GetCachedDisplacement
//

FLOAT ATerrain::GetCachedDisplacement(INT X,INT Y,INT SubX,INT SubY) const
{
	INT		PackedDisplacement = CachedDisplacements((Y * MaxTesselationLevel + SubY) * (NumPatchesX * MaxTesselationLevel + 1) + (X * MaxTesselationLevel + SubX));
	return (FLOAT)(PackedDisplacement - 127) / 127.0f * MaxCollisionDisplacement;
}

//
//	UTerrainComponent::Serialize
//
void UTerrainComponent::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << StaticLights;
}

void UTerrainComponent::PostLoad()
{
	Super::PostLoad();

	if(!SectionSizeX || !SectionSizeY)
	{
		// Restore legacy terrain component sizes.
		SectionSizeX = 16;
		SectionSizeY = 16;
	}
}

//
//	UTerrainComponent::PointCheck
//

UBOOL UTerrainComponent::PointCheck(FCheckResult& Result,const FVector& InPoint,const FVector& InExtent)
{
	if( LineCheck(Result,InPoint - FVector(0,0,InExtent.Z),InPoint + FVector(0,0,InExtent.Z),FVector(InExtent.X,InExtent.Y,0),0)==0 )
	{
		Result.Location.Z += InExtent.Z;
		return 0;
	}
	else
		return 1;
}

//
//	LineCheckWithTriangle
//	Algorithm based on "Fast, Minimum Storage Ray/Triangle Intersection"
//

FORCEINLINE static UBOOL LineCheckWithTriangle(FCheckResult& Result,const FVector& V1,const FVector& V2,const FVector& V3,const FVector& Start,const FVector& End,const FVector& Direction,FLOAT InvDistance)
{
	FVector	Edge1 = V3 - V1,
			Edge2 = V2 - V1,
			P = Direction ^ Edge2;
	FLOAT	Determinant = Edge1 | P;

	if(Determinant < DELTA)
		return 0;

	FVector	T = Start - V1;
	FLOAT	U = T | P;

	if(U < 0.0f || U > Determinant)
		return 0;

	FVector	Q = T ^ Edge1;
	FLOAT	V = Direction | Q;

	if(V < 0.0f || U + V > Determinant)
		return 0;

	FLOAT	Time = (Edge2 | Q) / Determinant;

	if(Time < 0.0f || Time > Result.Time)
		return 0;

	Result.Normal = ((V3-V2)^(V2-V1)).SafeNormal();
	Result.Time = ((V1 - Start)|Result.Normal) / (Result.Normal|Direction);

	return 1;
}

//
//	LineCheckWithBox
//

static UBOOL LineCheckWithBox
(
	const FVector&	BoxCenter,
	const FVector&  BoxRadii,
	const FVector&	Start,
	const FVector&	Direction,
	const FVector&	OneOverDirection
)
{
	//const FVector* boxPlanes = &Box.Min;
	
	FLOAT tf, tb;
	FLOAT tnear = 0.f;
	FLOAT tfar = 1.f;
	
	FVector LocalStart = Start - BoxCenter;

	// X //
	// First - see if ray is parallel to slab.
	if(Direction.X != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.X * OneOverDirection.X) - BoxRadii.X * fabs(OneOverDirection.X);
		tb = - (LocalStart.X * OneOverDirection.X) + BoxRadii.X * fabs(OneOverDirection.X);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		// If it is parallel, early return if start is outiside slab.
		if(!(fabs(LocalStart.X) <= BoxRadii.X))
			return 0;
	}

	// Y //
	if(Direction.Y != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Y * OneOverDirection.Y) - BoxRadii.Y * fabs(OneOverDirection.Y);
		tb = - (LocalStart.Y * OneOverDirection.Y) + BoxRadii.Y * fabs(OneOverDirection.Y);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(Abs(LocalStart.Y) <= BoxRadii.Y))
			return 0;
	}

	// Z //
	if(Direction.Z != 0.f)
	{
		// If not, find the time it hits the front and back planes of slab.
		tf = - (LocalStart.Z * OneOverDirection.Z) - BoxRadii.Z * fabs(OneOverDirection.Z);
		tb = - (LocalStart.Z * OneOverDirection.Z) + BoxRadii.Z * fabs(OneOverDirection.Z);

		if(tf > tnear)
			tnear = tf;

		if(tb < tfar)
			tfar = tb;

		if(tfar < tnear)
			return 0;
	}
	else
	{
		if(!(fabs(LocalStart.Z) <= BoxRadii.Z))
			return 0;
	}

	// we hit!
	return 1;
}

//
//	UTerrainComponent::LineCheck
//

UBOOL UTerrainComponent::LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	UBOOL	ZeroExtent = Extent == FVector(0,0,0);
	BEGINCYCLECOUNTER(ZeroExtent ? GCollisionStats.TerrainZeroExtentTime : GCollisionStats.TerrainExtentTime);

    ATerrain*	Terrain = GetTerrain();

	const FMatrix&	LocalToWorld = Terrain->LocalToWorld(),
					WorldToLocal = Terrain->WorldToLocal();
	const FVector&	LocalBoxX = WorldToLocal.TransformNormal(FVector(1,0,0)),
					LocalBoxY = WorldToLocal.TransformNormal(FVector(0,1,0)),
					LocalBoxZ = WorldToLocal.TransformNormal(FVector(0,0,1));

	// Compute a bounding box for the line.

	FBox	WorldBounds(0);
	WorldBounds += Start;
	WorldBounds += End;
	WorldBounds.Min -= Extent;
	WorldBounds.Max += Extent;

	const FBox&	LocalBounds = WorldBounds.TransformBy(WorldToLocal);
	INT			MinX = Clamp(appTrunc(LocalBounds.Min.X),SectionBaseX,SectionBaseX + SectionSizeX - 1),
				MinY = Clamp(appTrunc(LocalBounds.Min.Y),SectionBaseY,SectionBaseY + SectionSizeY - 1),
				MaxX = Clamp(appTrunc(LocalBounds.Max.X),SectionBaseX,SectionBaseX + SectionSizeX - 1),
				MaxY = Clamp(appTrunc(LocalBounds.Max.Y),SectionBaseY,SectionBaseY + SectionSizeY - 1);

	if(!Result.Actor)
		Result.Time = 1.0f;

	// Check against the patches within the line's bounding box.

	FVector	Direction = End - Start,
			LocalStart = WorldToLocal.TransformFVector(Start),
			LocalEnd = WorldToLocal.TransformFVector(End),
			LocalDirection = LocalEnd - LocalStart,
			LocalOneOverDirection = FVector(
				Square(LocalDirection.X) > Square(DELTA) ? 1.0f / LocalDirection.X : 0.0f,
				Square(LocalDirection.Y) > Square(DELTA) ? 1.0f / LocalDirection.Y : 0.0f,
				Square(LocalDirection.Z) > Square(DELTA) ? 1.0f / LocalDirection.Z : 0.0f
				);
	FLOAT	InvDistance = 1.0f / Direction.Size(),
			ExtentSize = Max(Max(Extent.X,Extent.Y),Extent.Z) / (Terrain->DrawScale * Terrain->DrawScale3D.X);

	for(INT PatchY = MinY;PatchY <= MaxY;PatchY++)
	{
		for(INT PatchX = MinX;PatchX <= MaxX;PatchX++)
		{
			const FTerrainPatch&	Patch = Terrain->GetPatch(PatchX,PatchY);

			const FTerrainPatchBounds&	PatchBound = PatchBounds((PatchY - SectionBaseY) * SectionSizeX + (PatchX - SectionBaseX));
			FLOAT						CenterHeight = (PatchBound.MaxHeight + PatchBound.MinHeight) / 2.0f;

			if(!LineCheckWithBox(
				FVector(PatchX + 0.5f,PatchY + 0.5f,CenterHeight),
				FVector(
					0.5f + PatchBound.MaxDisplacement + ExtentSize,
					0.5f + PatchBound.MaxDisplacement + ExtentSize,
					PatchBound.MaxHeight - CenterHeight + ExtentSize
					),
				LocalStart,
				LocalDirection,
				LocalOneOverDirection
				))
				continue;

			FVector	PatchVertexCache[2][TERRAIN_MAXTESSELATION + 1];
			INT		NextCacheRow = 0;

			for(INT SubX = 0;SubX <= Terrain->MaxTesselationLevel;SubX++)
				PatchVertexCache[NextCacheRow][SubX] = Terrain->GetCollisionVertex(Patch,PatchX,PatchY,SubX,0);

			NextCacheRow = 1 - NextCacheRow;

			for(INT SubY = 0;SubY < Terrain->MaxTesselationLevel;SubY++)
			{
				for(INT SubX = 0;SubX <= Terrain->MaxTesselationLevel;SubX++)
					PatchVertexCache[NextCacheRow][SubX] = Terrain->GetCollisionVertex(Patch,PatchX,PatchY,SubX,SubY + 1);

				for(INT SubX = 0;SubX < Terrain->MaxTesselationLevel;SubX++)
				{
					const FVector&	V00 = PatchVertexCache[1 - NextCacheRow][SubX],
									V10 = PatchVertexCache[1 - NextCacheRow][SubX + 1],
									V01 = PatchVertexCache[NextCacheRow][SubX],
									V11 = PatchVertexCache[NextCacheRow][SubX + 1];
					UBOOL			Hit = 0;

					if(ZeroExtent)
					{
						Hit |= LineCheckWithTriangle(Result,V00,V01,V11,LocalStart,LocalEnd,LocalDirection,InvDistance);
						Hit |= LineCheckWithTriangle(Result,V11,V10,V00,LocalStart,LocalEnd,LocalDirection,InvDistance);
					}
					else
					{
						Hit |= FindSeparatingAxis(V00,V01,V11,LocalStart,LocalEnd,Extent,LocalBoxX,LocalBoxY,LocalBoxZ,Result.Time,Result.Normal);
						Hit |= FindSeparatingAxis(V11,V10,V00,LocalStart,LocalEnd,Extent,LocalBoxX,LocalBoxY,LocalBoxZ,Result.Time,Result.Normal);
					}

					if(Hit)
					{
						Result.Component = this;
						Result.Actor = Owner;
						Result.Material = NULL;

						if((TraceFlags & TRACE_StopAtFirstHit) || Extent == FVector(0,0,0))
							goto Finished;
					}
				}

				NextCacheRow = 1 - NextCacheRow;
			}
		}
	}

Finished:
	if(Result.Component == this)
	{
		Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f / (End - Start).Size(),1.0f / (End - Start).Size()),0.0f,1.0f);
		Result.Normal = LocalToWorld.TransformNormal(Result.Normal).SafeNormal();
		Result.Location	= Start + (End - Start) * Result.Time;
	}
	return Result.Component != this;

	ENDCYCLECOUNTER;
}

UBOOL ATerrain::ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags)
{
	UBOOL Hit = 0;
	for(INT ComponentIndex = 0;ComponentIndex < Components.Num();ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = Cast<UPrimitiveComponent>(Components(ComponentIndex));
		if(Primitive && !Primitive->LineCheck(Result,End,Start,Extent,TraceFlags))
		{
			Hit = 1;
		}
	}
	for(INT ComponentIndex = 0;ComponentIndex < TerrainComponents.Num();ComponentIndex++)
	{
		UPrimitiveComponent* Primitive = TerrainComponents(ComponentIndex);
		if(Primitive && !Primitive->LineCheck(Result,End,Start,Extent,TraceFlags))
		{
			Hit = 1;
		}
	}
	return !Hit;
}

//
//	UTerrainComponent::UpdateBounds
//

void UTerrainComponent::UpdateBounds()
{
	ATerrain*	Terrain = GetTerrain();

	FBox	BoundingBox(0);
	for(INT Y = 0;Y < SectionSizeY;Y++)
	{
		for(INT X = 0;X < SectionSizeX;X++)
		{
			const FTerrainPatchBounds&	Patch = PatchBounds(Y * SectionSizeX + X);
			BoundingBox += FBox(FVector(X - Patch.MaxDisplacement,Y - Patch.MaxDisplacement,Patch.MinHeight),FVector(X + 1 + Patch.MaxDisplacement,Y + 1 + Patch.MaxDisplacement,Patch.MaxHeight));
		}
	}

	Bounds = FBoxSphereBounds(BoundingBox.TransformBy(LocalToWorld).ExpandBy(1.0f));
}

//
//	UTerrainComponent::Init
//

void UTerrainComponent::Init(INT InBaseX,INT InBaseY,INT InSizeX,INT InSizeY)
{
	SectionBaseX = InBaseX;
	SectionBaseY = InBaseY;
	SectionSizeX = InSizeX;
	SectionSizeY = InSizeY;
}

//
//	UTerrainComponent::UpdatePatchBounds
//

void UTerrainComponent::UpdatePatchBounds()
{
	ATerrain*	Terrain = GetTerrain();

	PatchBounds.Empty(SectionSizeX * SectionSizeY);

	for(INT Y = 0;Y < SectionSizeY;Y++)
	{
		for(INT X = 0;X < SectionSizeX;X++)
		{
			const FTerrainPatch&	Patch = Terrain->GetPatch(SectionBaseX + X,SectionBaseY + Y);
			FTerrainPatchBounds		Bounds;

			Bounds.MinHeight = 32768.0f * TERRAIN_ZSCALE;
            Bounds.MaxHeight = -32768.0f * TERRAIN_ZSCALE;
			Bounds.MaxDisplacement = 0.0f;

			INT	GlobalX = SectionBaseX + X,
				GlobalY = SectionBaseY + Y;

			for(INT SubY = 0;SubY <= Terrain->MaxTesselationLevel;SubY++)
			{
				for(INT SubX = 0;SubX <= Terrain->MaxTesselationLevel;SubX++)
				{
					const FVector&	Vertex = Terrain->GetCollisionVertex(Patch,GlobalX,GlobalY,SubX,SubY);

					Bounds.MinHeight = Min(Bounds.MinHeight,Vertex.Z);
					Bounds.MaxHeight = Max(Bounds.MaxHeight,Vertex.Z);

					Bounds.MaxDisplacement = Max(
												Bounds.MaxDisplacement,
												Max(
													Max(Vertex.X - GlobalX - 1.0f,GlobalX - Vertex.X),
													Max(Vertex.Y - GlobalY - 1.0f,GlobalY - Vertex.Y)
													)
												);
				}
			}

			PatchBounds.AddItem(Bounds);
		}
	}
}

//
//	UTerrainComponent::SetParentToWorld
//

void UTerrainComponent::SetParentToWorld(const FMatrix& ParentToWorld)
{
	ATerrain* Terrain = GetTerrain();
	Super::SetParentToWorld(FTranslationMatrix(FVector(SectionBaseX,SectionBaseY,0)) * ParentToWorld);
}

EStaticLightingAvailability UTerrainComponent::GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive)
{
	check(Initialized);

	if(Light->HasStaticShadowing())
	{
		for(INT LightIndex = 0;LightIndex < StaticLights.Num();LightIndex++)
		{
			FStaticTerrainLight*	StaticLight = &StaticLights(LightIndex);
			if(StaticLight->Light == Light)
			{
				OutStaticLightPrimitive = StaticLight;
				return SLA_Available;
			}
		}

		if(IgnoreLights.FindItemIndex(Light) != INDEX_NONE)
			return SLA_Shadowed;
	}

	return SLA_Unavailable;
}

void UTerrainComponent::CacheLighting()
{
	if(!HasStaticShadowing())
		return;

	FComponentRecreateContext	RecreateContext(this);

	StaticLights.Empty();
	IgnoreLights.Empty();

	ATerrain*	Terrain = GetTerrain();

	const FMatrix&	LocalToWorld = Terrain->LocalToWorld();

	TArray<ULightComponent*>	RelevantLights;
	Scene->GetRelevantLights(this,RelevantLights);
	for(UINT LightIndex = 0;LightIndex < (UINT)RelevantLights.Num();LightIndex++)
	{
		ULightComponent*	Light = RelevantLights(LightIndex);

		if(!Light->HasStaticShadowing() || !Light->CastShadows)
			continue;

		// Calculate the homogenous light position.

		FPlane	LightPosition = Light->GetPosition();

		// Cache the lighting at each terrain vertex.

		TArray<BYTE>	ShadowData;
		INT				LightMapSizeX = SectionSizeX * Terrain->StaticLightingResolution + 1,
						LightMapSizeY = SectionSizeY * Terrain->StaticLightingResolution + 1;
		UBOOL			Discard = 1;
		ShadowData.AddZeroed(LightMapSizeX * LightMapSizeY);

		for(INT PatchY = 0;PatchY <= SectionSizeY;PatchY++)
		{
			for(INT PatchX = 0;PatchX <= SectionSizeX;PatchX++)
			{
				const FTerrainPatch&	Patch = Terrain->GetPatch(SectionBaseX + PatchX,SectionBaseY + PatchY);

				for(INT ResX = 0;ResX < Terrain->StaticLightingResolution;ResX++)
				{
					for(INT ResY = 0;ResY < Terrain->StaticLightingResolution;ResY++)
					{
						INT	X = PatchX * Terrain->StaticLightingResolution + ResX,
							Y = PatchY * Terrain->StaticLightingResolution + ResY;
						if(X >= LightMapSizeX || Y >= LightMapSizeY)
							continue;

						const FVector&	Vertex = LocalToWorld.TransformFVector(
													Terrain->GetCollisionVertex(
															Patch,
															SectionBaseX + PatchX,
															SectionBaseY + PatchY,
															ResX * Terrain->MaxTesselationLevel / Terrain->StaticLightingResolution,
															ResY * Terrain->MaxTesselationLevel / Terrain->StaticLightingResolution
															)
														),
										LightVector = (FVector)LightPosition - Vertex * LightPosition.W;

						FCheckResult	Hit(1.0f);
						if(	LineCheck(Hit,Vertex + LightVector.SafeNormal() * 4.0f,Vertex + LightVector,FVector(0,0,0),TRACE_StopAtFirstHit) &&
							Owner->XLevel->SingleLineCheck(Hit,NULL,Vertex + LightVector.SafeNormal() * 4.0f,Vertex + LightVector,TRACE_Level|TRACE_Actors|TRACE_StopAtFirstHit|TRACE_ShadowCast))
						{
							ShadowData(Y * LightMapSizeX + X) = 255;
							Discard = 0;
						}
					}
				}
			}
		}

		if(Discard)
		{
			IgnoreLights.AddItem(Light);
		}
		else
		{
			// Smooth the lightmap and store it.

			FStaticTerrainLight*	StaticLight = new(StaticLights) FStaticTerrainLight(this,Light,LightMapSizeX,LightMapSizeY);

			for(UINT Y = 0;Y < StaticLight->SizeY;Y++)
			{
				for(UINT X = 0;X < StaticLight->SizeX;X++)
				{
					UINT	Numerator = 0,
							Denominator = 0;

					for(UINT FilterY = 0;FilterY < 3;FilterY++)
					{
						INT	OffsetY = Y - 1 + FilterY;
						if(OffsetY >= 0 && OffsetY < (INT)StaticLight->SizeY)
						{
							for(UINT FilterX = 0;FilterX < 3;FilterX++)
							{
								INT	OffsetX = X - 1 + FilterX;
								if(OffsetX >= 0 && OffsetX < (INT)StaticLight->SizeX)
								{
									Numerator += ShadowData(OffsetY * StaticLight->SizeX + OffsetX);
									Denominator++;
								}
							}
						}
					}

					StaticLight->ShadowData(Y * StaticLight->SizeX + X) = (BYTE)Clamp<INT>(appTrunc((FLOAT)Numerator / (FLOAT)Denominator),0,255);
				}
			}

			GResourceManager->UpdateResource(StaticLight);
		}
	}
}

//
//	UTerrainComponent::InvalidateLightingCache
//

void UTerrainComponent::InvalidateLightingCache(ULightComponent* Light)
{
	ATerrain*	Terrain = GetTerrain();

	for(UINT LightIndex = 0;LightIndex < (UINT)StaticLights.Num();LightIndex++)
	{
		if(StaticLights(LightIndex).Light == Light)
		{
			FComponentRecreateContext	RecreateContext(this);
			StaticLights.Remove(LightIndex);
			break;
		}
	}

	if(IgnoreLights.FindItemIndex(Light) != INDEX_NONE)
	{
		FComponentRecreateContext	RecreateContext(this);
		IgnoreLights.RemoveItem(Light);
	}
}

void UTerrainComponent::InvalidateLightingCache()
{
	FComponentRecreateContext RecreateContext(this);
	StaticLights.Empty();
	IgnoreLights.Empty();
}

//
//	UTerrainComponent::GetLocalVertex
//

FVector UTerrainComponent::GetLocalVertex(INT X,INT Y) const
{
	return FVector(X,Y,(-32768.0f + (FLOAT)GetTerrain()->Height(SectionBaseX + X,SectionBaseY + Y)) * TERRAIN_ZSCALE);
}

//
//	UTerrainComponent::GetWorldVertex
//

FVector UTerrainComponent::GetWorldVertex(INT X,INT Y) const
{
	if(Initialized)
		return LocalToWorld.TransformFVector(GetLocalVertex(X,Y));
	else
		return GetTerrain()->GetWorldVertex(SectionBaseX + X,SectionBaseY + Y);
}

//
//	UTerrainMaterial::UpdateMappingTransform
//

void UTerrainMaterial::UpdateMappingTransform()
{
	FMatrix	BaseDirection;
	switch(MappingType)
	{
	case TMT_XZ:
		BaseDirection = FMatrix(FPlane(1,0,0,0),FPlane(0,0,1,0),FPlane(0,1,0,0),FPlane(0,0,0,1));
		break;
	case TMT_YZ:
		BaseDirection = FMatrix(FPlane(0,0,1,0),FPlane(1,0,0,0),FPlane(0,1,0,0),FPlane(0,0,0,1));
		break;
	case TMT_XY:
	default:
		BaseDirection = FMatrix::Identity;
		break;
	};

	LocalToMapping = BaseDirection *
		FScaleMatrix(FVector(1,1,1) * (MappingScale == 0.0f ? 1.0f : 1.0f / MappingScale)) *
		FMatrix(
			FPlane(+appCos(MappingRotation * (FLOAT)PI / 180.0f),	-appSin(MappingRotation * (FLOAT)PI / 180.0f),	0,	0),
			FPlane(+appSin(MappingRotation * (FLOAT)PI / 180.0f),	+appCos(MappingRotation * (FLOAT)PI / 180.0f),	0,	0),
			FPlane(0,												0,												1,	0),
			FPlane(MappingPanU,										MappingPanV,									0,	1)
			);
}

//
//	UTerrainMaterial::GetDisplacement
//

FLOAT UTerrainMaterial::GetDisplacement(FLOAT U,FLOAT V) const
{
	if(DisplacementMap && DisplacementMap->Format == PF_A8R8G8B8)
	{
		FStaticMipMap2D&	MipMap = DisplacementMap->Mips(0);
		if(!MipMap.Data.Num())
			MipMap.Data.Load();

		INT		IntX = appTrunc(U * MipMap.SizeX),
				IntY = appTrunc(V * MipMap.SizeY);
		FLOAT	BaseDisplacement = ((FColor*)&MipMap.Data(0))[(IntY % MipMap.SizeY) * MipMap.SizeX + (IntX % MipMap.SizeX)].A / 255.0f;

		return (BaseDisplacement * (DisplacementMap->UnpackMax.W - DisplacementMap->UnpackMin.W) + DisplacementMap->UnpackMin.W) * DisplacementScale;
	}
	else
	if(DisplacementMap && DisplacementMap->Format == PF_G8)
	{
		FStaticMipMap2D&	MipMap = DisplacementMap->Mips(0);
		if(!MipMap.Data.Num())
			MipMap.Data.Load();

		INT		IntX = appTrunc(U * MipMap.SizeX),
				IntY = appTrunc(V * MipMap.SizeY);
		FLOAT	BaseDisplacement = ((BYTE*)&MipMap.Data(0))[(IntY % MipMap.SizeY) * MipMap.SizeX + (IntX % MipMap.SizeX)] / 255.0f;

		return (BaseDisplacement * (DisplacementMap->UnpackMax.W - DisplacementMap->UnpackMin.W) + DisplacementMap->UnpackMin.W) * DisplacementScale;
	}
	else
		return 0.0f;
}

//
//	FTerrainWeightedMaterial::FTerrainWeightedMaterial
//

FTerrainWeightedMaterial::FTerrainWeightedMaterial()
{
	Format = PF_G8;
	NumMips = 1;
}

FTerrainWeightedMaterial::FTerrainWeightedMaterial(ATerrain* InTerrain,const TArray<BYTE>& InData,UTerrainMaterial* InMaterial,UBOOL InHighlighted):
	Terrain(InTerrain),
	Data(InData),
	Material(InMaterial),
	Highlighted(InHighlighted)
{
	SizeX = Terrain->NumVerticesX;
	SizeY = Terrain->NumVerticesY;
	Format = PF_G8;
	NumMips = 1;
}

//
//	FTerrainWeightedMaterial::Weight
//

const BYTE& FTerrainWeightedMaterial::Weight(INT X,INT Y) const
{
	check(X >= 0 && X < (INT)SizeX && Y >= 0 && Y < (INT)SizeY);
	return Data(Y * SizeX + X);
}

//
//	FTerrainWeightedMaterial::FilteredWeight
//

FLOAT FTerrainWeightedMaterial::FilteredWeight(INT IntX,FLOAT FracX,INT IntY,FLOAT FracY) const
{
	if(IntX < (INT)SizeX - 1 && IntY < (INT)SizeY - 1)
		return BiLerp(
				(FLOAT)Weight(IntX,IntY),
				(FLOAT)Weight(IntX + 1,IntY),
				(FLOAT)Weight(IntX,IntY + 1),
				(FLOAT)Weight(IntX + 1,IntY + 1),
				FracX,
				FracY
				);
	else if(IntX < (INT)SizeX - 1)
		return Lerp(
				(FLOAT)Weight(IntX,IntY),
				(FLOAT)Weight(IntX + 1,IntY),
				FracX
				);
	else if(IntY < (INT)SizeY - 1)
		return Lerp(
				(FLOAT)Weight(IntX,IntY),
				(FLOAT)Weight(IntX,IntY + 1),
				FracY
				);
	else
		return (FLOAT)Weight(IntX,IntY);
}

//
//	FTerrainWeightedMaterial::GetData
//

void FTerrainWeightedMaterial::GetData(void* Buffer,UINT MipIndex,UINT StrideY)
{
	BYTE*	DestWeight = (BYTE*)Buffer;

	for(INT Y = 0;Y < Terrain->NumVerticesY;Y++)
	{
		for(INT X = 0;X < Terrain->NumVerticesX;X++)
			DestWeight[X] = Data(Y * Terrain->NumVerticesX + X);
		DestWeight += StrideY;
	}

}

//
//	operator<< FTerrainWeightedMaterial
//

FArchive& operator<<(FArchive& Ar,FTerrainWeightedMaterial& M)
{
	check(!Ar.IsSaving() && !Ar.IsLoading()); // Weight maps shouldn't be stored.
	return Ar << M.Terrain << M.Data << M.Material << M.Highlighted;
}

/**
 * FStaticTerrainLight implementation.
 */

FStaticTerrainLight::FStaticTerrainLight(UTerrainComponent* InTerrainComponent,ULightComponent* InLight,INT InSizeX,INT InSizeY):
	TerrainComponent(InTerrainComponent),
	Light(InLight)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	ShadowData.AddZeroed(SizeX * SizeY);
	Format = PF_G8;
	NumMips = 1;
}

FStaticTerrainLight::FStaticTerrainLight():
	TerrainComponent(NULL),
	Light(NULL)
{
	Format = PF_G8;
	NumMips = 1;
}

const BYTE& FStaticTerrainLight::Shadow(INT X,INT Y) const
{
	check(X >= 0 && X < (INT)SizeX && Y >= 0 && Y < (INT)SizeY);
	return ShadowData(Y * SizeX + X);
}

FLOAT FStaticTerrainLight::FilteredShadow(INT IntX,FLOAT FracX,INT IntY,FLOAT FracY) const
{
	if(IntX < (INT)SizeX - 1 && IntY < (INT)SizeY - 1)
		return BiLerp(
				(FLOAT)Shadow(IntX,IntY),
				(FLOAT)Shadow(IntX + 1,IntY),
				(FLOAT)Shadow(IntX,IntY + 1),
				(FLOAT)Shadow(IntX + 1,IntY + 1),
				FracX,
				FracY
				) / 255.0f;
	else if(IntX < (INT)SizeX - 1)
		return Lerp(
				(FLOAT)Shadow(IntX,IntY),
				(FLOAT)Shadow(IntX + 1,IntY),
				FracX
				) / 255.0f;
	else if(IntY < (INT)SizeY - 1)
		return Lerp(
				(FLOAT)Shadow(IntX,IntY),
				(FLOAT)Shadow(IntX,IntY + 1),
				FracY
				) / 255.0f;
	else
		return (FLOAT)Shadow(IntX,IntY) / 255.0f;
}

void FStaticTerrainLight::GetData(void* Buffer,UINT MipIndex,UINT StrideY)
{
	for(UINT Y = 0;Y < SizeY;Y++)
		appMemcpy((BYTE*)Buffer + StrideY * Y,&ShadowData(Y * SizeX),SizeX * sizeof(BYTE));
}

FArchive& operator<<(FArchive& Ar,FStaticTerrainLight& S)
{
	Ar << S.TerrainComponent << S.Light;
	Ar << S.ShadowData << S.SizeX << S.SizeY;
	return Ar;
}

//
//	FPatchSampler::FPatchSampler
//

FPatchSampler::FPatchSampler(UINT InMaxTesselation):
	MaxTesselation(InMaxTesselation)
{
	for(UINT I = 0;I <= MaxTesselation;I++)
	{
		FLOAT	T = (FLOAT)I / (FLOAT)MaxTesselation;
		CubicBasis[I][0] = -0.5f * (T * T * T - 2.0f * T * T + T);
		CubicBasis[I][1] = (2.0f * T * T * T - 3.0f * T * T + 1.0f) - 0.5f * (T * T * T - T * T);
		CubicBasis[I][2] = (-2.0f * T * T * T + 3.0f * T * T) + 0.5f * (T * T * T - 2.0f * T * T + T);
		CubicBasis[I][3] = +0.5f * (T * T * T - T * T);

		CubicBasisDeriv[I][0] = 0.5f * (-1.0f + 4.0f * T - 3.0f * T * T);
		CubicBasisDeriv[I][1] = -6.0f * T + 6.0f * T * T + 0.5f * (2.0f * T - 3.0f * T * T);
		CubicBasisDeriv[I][2] = +6.0f * T - 6.0f * T * T + 0.5f * (1.0f - 4.0f * T + 3.0f * T * T);
		CubicBasisDeriv[I][3] = 0.5f * (-2.0f * T + 3.0f * T * T);
	}
}

//
//	FPatchSampler::Sample
//

FLOAT FPatchSampler::Sample(const FTerrainPatch& Patch,UINT X,UINT Y) const
{
	return Cubic(
			Cubic(Patch.Heights[0][0],Patch.Heights[1][0],Patch.Heights[2][0],Patch.Heights[3][0],X),
			Cubic(Patch.Heights[0][1],Patch.Heights[1][1],Patch.Heights[2][1],Patch.Heights[3][1],X),
			Cubic(Patch.Heights[0][2],Patch.Heights[1][2],Patch.Heights[2][2],Patch.Heights[3][2],X),
			Cubic(Patch.Heights[0][3],Patch.Heights[1][3],Patch.Heights[2][3],Patch.Heights[3][3],X),
			Y
			);
}

//
//	FPatchSampler::SampleDerivX
//

FLOAT FPatchSampler::SampleDerivX(const FTerrainPatch& Patch,UINT X,UINT Y) const
{
#if 0 // Return a linear gradient, so tesselation changes don't affect lighting.
	return Cubic(
			CubicDeriv(Patch.Heights[0][0],Patch.Heights[1][0],Patch.Heights[2][0],Patch.Heights[3][0],X),
			CubicDeriv(Patch.Heights[0][1],Patch.Heights[1][1],Patch.Heights[2][1],Patch.Heights[3][1],X),
			CubicDeriv(Patch.Heights[0][2],Patch.Heights[1][2],Patch.Heights[2][2],Patch.Heights[3][2],X),
			CubicDeriv(Patch.Heights[0][3],Patch.Heights[1][3],Patch.Heights[2][3],Patch.Heights[3][3],X),
			Y
			);
#else
	return Lerp(
			Lerp(Patch.Heights[2][1] - Patch.Heights[0][1],Patch.Heights[3][1] - Patch.Heights[1][1],(FLOAT)X / (FLOAT)MaxTesselation),
			Lerp(Patch.Heights[2][2] - Patch.Heights[0][2],Patch.Heights[3][2] - Patch.Heights[1][2],(FLOAT)X / (FLOAT)MaxTesselation),
			(FLOAT)Y / (FLOAT)MaxTesselation
			);
#endif
}

//
//	FPatchSampler::SampleDerivY
//

FLOAT FPatchSampler::SampleDerivY(const FTerrainPatch& Patch,UINT X,UINT Y) const
{
#if 0 // Return a linear gradient, so tesselation changes don't affect lighting.
	return CubicDeriv(
			Cubic(Patch.Heights[0][0],Patch.Heights[1][0],Patch.Heights[2][0],Patch.Heights[3][0],X),
			Cubic(Patch.Heights[0][1],Patch.Heights[1][1],Patch.Heights[2][1],Patch.Heights[3][1],X),
			Cubic(Patch.Heights[0][2],Patch.Heights[1][2],Patch.Heights[2][2],Patch.Heights[3][2],X),
			Cubic(Patch.Heights[0][3],Patch.Heights[1][3],Patch.Heights[2][3],Patch.Heights[3][3],X),
			Y
			);
#else
	return Lerp(
			Lerp(Patch.Heights[1][2] - Patch.Heights[1][0],Patch.Heights[2][2] - Patch.Heights[2][0],(FLOAT)X / (FLOAT)MaxTesselation),
			Lerp(Patch.Heights[1][3] - Patch.Heights[1][1],Patch.Heights[2][3] - Patch.Heights[2][1],(FLOAT)X / (FLOAT)MaxTesselation),
			(FLOAT)Y / (FLOAT)MaxTesselation
			);
#endif
}

//
//	FPatchSampler::Cubic
//

FLOAT FPatchSampler::Cubic(FLOAT P0,FLOAT P1,FLOAT P2,FLOAT P3,UINT I) const
{
	return	P0 * CubicBasis[I][0] +
			P1 * CubicBasis[I][1] +
			P2 * CubicBasis[I][2] +
			P3 * CubicBasis[I][3];
}

//
//	FPatchSampler::CubicDeriv
//

FLOAT FPatchSampler::CubicDeriv(FLOAT P0,FLOAT P1,FLOAT P2,FLOAT P3,UINT I) const
{
	return	P0 * CubicBasisDeriv[I][0] +
			P1 * CubicBasisDeriv[I][1] +
			P2 * CubicBasisDeriv[I][2] +
			P3 * CubicBasisDeriv[I][3];
}

//
//	UTerrainMaterial::PostEditChange
//

void UTerrainMaterial::PostEditChange(UProperty* PropertyThatChanged)
{
	// Find any terrain actors using this material.
	TArray<ATerrain*> Actors;
	for(TObjectIterator<UTerrainLayerSetup> LayerIt;LayerIt;++LayerIt)
	{
		// Check if this layer setup uses this material.
		for(INT MaterialIndex = 0;MaterialIndex < LayerIt->Materials.Num();MaterialIndex++)
		{
			if(LayerIt->Materials(MaterialIndex).Material == this)
			{
				// The layer setup uses this material, find any terrain actors using the layer setup.
				for(TObjectIterator<ATerrain> ActorIt;ActorIt;++ActorIt)
				{
					for(INT LayerIndex = 0;LayerIndex < ActorIt->Layers.Num();LayerIndex++)
					{
						if(ActorIt->Layers(LayerIndex).Setup == *LayerIt)
						{
							// This terrain actor uses the layer setup, add the actor to the list.
							Actors.AddUniqueItem(*ActorIt);
							break;
						}
					}
				}
				break;
			}
		}
	}

	// Pass the PostEditChange to the terrain actors.
	for(INT ActorIndex = 0;ActorIndex < Actors.Num();ActorIndex++)
	{
		Actors(ActorIndex)->PreEditChange();
		Actors(ActorIndex)->PostEditChange(PropertyThatChanged);
	}
}

//
//	UTerrainMaterial::DrawThumbnail
//

void UTerrainMaterial::DrawThumbnail(EThumbnailPrimType PrimType,INT X,INT Y,struct FChildViewport* Viewport,struct FRenderInterface* RI,FLOAT Zoom,UBOOL ShowBackground,FLOAT ZoomPct,INT InFixedSz)
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticLoadObject(UTexture2D::StaticClass(),NULL,TEXT("EngineResources.UnrealEdIcon_TerrainMaterial"),NULL,LOAD_NoFail,NULL));
	RI->DrawTile(
		X,Y,
		InFixedSz ? InFixedSz : 256*Zoom,
		InFixedSz ? InFixedSz : 256*Zoom,
		0.0f,0.0f,1.0f,1.0f,FLinearColor::White,Icon,0);
}

//
//	UTerrainMaterial::GetThumbnailDesc
//

FThumbnailDesc UTerrainMaterial::GetThumbnailDesc(FRenderInterface* RI,FLOAT Zoom,INT InFixedSz)
{
	FThumbnailDesc td;
	td.Width = InFixedSz ? InFixedSz : 256*Zoom;
	td.Height = InFixedSz ? InFixedSz : 256*Zoom;
	return td;
}

//
//	UTerrainMaterial::GetThumbnailLabels
//

INT UTerrainMaterial::GetThumbnailLabels(TArray<FString>* Labels)
{
	Labels->Empty();

	new(*Labels)FString(FString::Printf(TEXT("%s"),GetName()));

	return Labels->Num();
}

//
//	UTerrainLayerSetup::PostEditChange
//

void UTerrainLayerSetup::PostEditChange(UProperty* PropertyThatChanged)
{
	// Find any terrain actors using this layer setup.
	for(TObjectIterator<ATerrain> ActorIt;ActorIt;++ActorIt)
	{
		for(INT LayerIndex = 0;LayerIndex < ActorIt->Layers.Num();LayerIndex++)
		{
			if(ActorIt->Layers(LayerIndex).Setup == this)
			{
				// This terrain actor uses the layer setup, pass the PostEditChange to it.
				ActorIt->PreEditChange();
				ActorIt->PostEditChange(PropertyThatChanged);
				break;
			}
		}
	}
}

//
//	UTerrainLayerSetup::DrawThumbnail
//

void UTerrainLayerSetup::DrawThumbnail(EThumbnailPrimType PrimType,INT X,INT Y,struct FChildViewport* Viewport,struct FRenderInterface* RI,FLOAT Zoom,UBOOL ShowBackground,FLOAT ZoomPct,INT InFixedSz)
{
	UTexture2D* Icon = CastChecked<UTexture2D>(UObject::StaticLoadObject(UTexture2D::StaticClass(),NULL,TEXT("EngineResources.UnrealEdIcon_TerrainLayerSetup"),NULL,LOAD_NoFail,NULL));
	RI->DrawTile(
		X,Y,
		InFixedSz ? InFixedSz : 256*Zoom,
		InFixedSz ? InFixedSz : 256*Zoom,
		0.0f,0.0f,1.0f,1.0f,FLinearColor::White,Icon,0);
}

//
//	UTerrainLayerSetup::GetThumbnailDesc
//

FThumbnailDesc UTerrainLayerSetup::GetThumbnailDesc(FRenderInterface* RI,FLOAT Zoom,INT InFixedSz)
{
	FThumbnailDesc td;
	td.Width = InFixedSz ? InFixedSz : 256*Zoom;
	td.Height = InFixedSz ? InFixedSz : 256*Zoom;
	return td;
}

//
//	UTerrainLayerSetup::GetThumbnailLabels
//

INT UTerrainLayerSetup::GetThumbnailLabels(TArray<FString>* Labels)
{
	Labels->Empty();

	new(*Labels)FString(FString::Printf(TEXT("%s"),GetName()));

	return Labels->Num();
}
