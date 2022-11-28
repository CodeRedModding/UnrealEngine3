/*=============================================================================
	UnTerrain.cpp: Terrain rendering code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnTerrain.h"

//
//	TesselationLevel - Determines the level of tesselation to use for a terrain patch.
//

static UINT TesselationLevel(FLOAT Z)
{
	if(Z < 4096.0f)
		return 16;
	else if(Z < 8192.0f)
		return 8;
	else if(Z < 16384.0f)
		return 4;
	else if(Z < 32768.0f)
		return 2;
	else
		return 1;
}

//
//	FTerrainMaterialResource::CompileTerrainMaterial - Compiles a single terrain material.
//

INT FTerrainMaterialResource::CompileTerrainMaterial(EMaterialProperty Property,FMaterialCompiler* Compiler,UTerrainMaterial* TerrainMaterial,UBOOL Highlighted)
{
	// FTerrainMaterialCompiler - A proxy compiler that overrides the texture coordinates used by the layer's material with the layer's texture coordinates.

	struct FTerrainMaterialCompiler: FProxyMaterialCompiler
	{
		UTerrainMaterial*	TerrainMaterial;

		FTerrainMaterialCompiler(FMaterialCompiler* InCompiler,UTerrainMaterial* InTerrainMaterial):
			FProxyMaterialCompiler(InCompiler),
			TerrainMaterial(InTerrainMaterial)
		{}

		// Override texture coordinates used by the material with the texture coordinate specified by the terrain material.

		virtual INT TextureCoordinate(UINT CoordinateIndex)
		{
			INT	BaseUV;
			switch(TerrainMaterial->MappingType)
			{
			case TMT_Auto:
			case TMT_XY: BaseUV = Compiler->TextureCoordinate(TERRAIN_UV_XY); break;
			case TMT_XZ: BaseUV = Compiler->TextureCoordinate(TERRAIN_UV_XZ); break;
			case TMT_YZ: BaseUV = Compiler->TextureCoordinate(TERRAIN_UV_YZ); break;
			default: appErrorf(TEXT("Invalid mapping type %u"),TerrainMaterial->MappingType); return INDEX_NONE;
			};

			FLOAT	Scale = TerrainMaterial->MappingScale == 0.0f ? 1.0f : TerrainMaterial->MappingScale;

			return Compiler->Add(
					Compiler->AppendVector(
						Compiler->Dot(BaseUV,Compiler->Constant2(+appCos(TerrainMaterial->MappingRotation * PI / 180.0) / Scale,+appSin(TerrainMaterial->MappingRotation * PI / 180.0) / Scale)),
						Compiler->Dot(BaseUV,Compiler->Constant2(-appSin(TerrainMaterial->MappingRotation * PI / 180.0) / Scale,+appCos(TerrainMaterial->MappingRotation * PI / 180.0) / Scale))
						),
					Compiler->Constant2(TerrainMaterial->MappingPanU,TerrainMaterial->MappingPanV)
					);
		}

		// Assign unique parameter names to each material.

		virtual INT VectorParameter(FName ParameterName)
		{
			return Compiler->VectorParameter(FName(*FString::Printf(TEXT("%s_%s"),*TerrainMaterial->GetPathName(),*ParameterName)));
		}

		virtual INT ScalarParameter(FName ParameterName)
		{
			return Compiler->ScalarParameter(FName(*FString::Printf(TEXT("%s_%s"),*TerrainMaterial->GetPathName(),*ParameterName)));
		}
	};

	UMaterialInstance*			Material = TerrainMaterial->Material ? TerrainMaterial->Material : GEngine->DefaultMaterial;
	FTerrainMaterialCompiler	ProxyCompiler(Compiler,TerrainMaterial);

	INT	Result = Compiler->ForceCast(Material->GetMaterialInterface(0)->CompileProperty(Property,&ProxyCompiler),FMaterial::GetPropertyType(Property));

	if(Highlighted)
	{
		FPlane	SelectionColor = FColor(10,5,60).Plane();
		switch(Property)
		{
		case MP_EmissiveColor:
			Result = Compiler->Add(Result,Compiler->Constant3(SelectionColor.X,SelectionColor.Y,SelectionColor.Z));
			break;
		case MP_DiffuseColor:
			Result = Compiler->Mul(Result,Compiler->Constant3(1.0f - SelectionColor.X,1.0f - SelectionColor.Y,1.0f - SelectionColor.Z));
			break;
		};
	}

	return Result;
}

//
//	FTerrainMaterialResource::CompileProperty
//

INT FTerrainMaterialResource::CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler)
{
	// Count the number of terrain materials included in this material.

	UINT	NumMaterials = 0;
	for(UINT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
		if(Mask.Get(MaterialIndex))
			NumMaterials++;

	if(NumMaterials == 1)
	{
		for(UINT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
			if(Mask.Get(MaterialIndex))
				return CompileTerrainMaterial(Property,Compiler,Terrain->WeightedMaterials(MaterialIndex).Material,Terrain->WeightedMaterials(MaterialIndex).Highlighted);
		appErrorf(TEXT("Single material has disappeared!"));
		return INDEX_NONE;
	}
	else if(NumMaterials > 1)
	{
		INT	Result = INDEX_NONE;
		for(UINT MaterialIndex = 0;MaterialIndex < Mask.Num();MaterialIndex++)
		{
			if(Mask.Get(MaterialIndex))
			{
				INT	IndividualResult = Compiler->Mul(
										Compiler->ComponentMask(Compiler->TextureSample(Compiler->Texture(&Terrain->WeightedMaterials(MaterialIndex)),Compiler->TextureCoordinate(0)),1,0,0,0),
										CompileTerrainMaterial(Property,Compiler,Terrain->WeightedMaterials(MaterialIndex).Material,Terrain->WeightedMaterials(MaterialIndex).Highlighted)
										);
				if(Result == INDEX_NONE)
					Result = IndividualResult;
				else
					Result = Compiler->Add(Result,IndividualResult);
			}
		}
		return Result;
	}
	else
		return GEngine->DefaultMaterial->MaterialResources[0]->CompileProperty(Property,Compiler);
}

//
//	ATerrain::GetCachedMaterial
//

FTerrainMaterialResource* ATerrain::GetCachedMaterial(const FTerrainMaterialMask& Mask)
{
	for(UINT MaterialIndex = 0;MaterialIndex < (UINT)CachedMaterials.Num();MaterialIndex++)
		if(CachedMaterials(MaterialIndex).Mask == Mask)
			return &CachedMaterials(MaterialIndex);

	return new(CachedMaterials) FTerrainMaterialResource(this,Mask);
}

//
//	FTerrainMaterialInstance
//

struct FTerrainMaterialInstance: FMaterialInstance
{
	FTerrainMaterialInstance(ATerrain* Terrain)
	{
		for(INT MaterialIndex = 0;MaterialIndex < Terrain->WeightedMaterials.Num();MaterialIndex++)
		{
			UTerrainMaterial*	TerrainMaterial = Terrain->WeightedMaterials(MaterialIndex).Material;
			UMaterialInstance*	Material = TerrainMaterial->Material ? TerrainMaterial->Material : GEngine->DefaultMaterial;
			FMaterialInstance*	MaterialInstance = Material->GetInstanceInterface();
			for(TMap<FName,FLinearColor>::TIterator It(MaterialInstance->VectorValueMap);It;++It)
				VectorValueMap.Set(
					FName(*FString::Printf(TEXT("%s_%s"),*TerrainMaterial->GetPathName(),*It.Key())),
					It.Value()
					);
			for(TMap<FName,FLOAT>::TIterator It(MaterialInstance->ScalarValueMap);It;++It)
				ScalarValueMap.Set(
					FName(*FString::Printf(TEXT("%s_%s"),*TerrainMaterial->GetPathName(),*It.Key())),
					It.Value()
					);
		}
	}
};

//
//	ATerrain::GetCachedMaterialInstance
//

FMaterialInstance* ATerrain::GetCachedMaterialInstance()
{
	if(!MaterialInstance)
		MaterialInstance = new FTerrainMaterialInstance(this);
	return MaterialInstance;
}

//
//	FTerrainIndexBuffer
//

struct FTerrainIndexBuffer: FIndexBuffer
{
	INT	SectionSizeX,
		SectionSizeY;

	// Constructor.

	FTerrainIndexBuffer(UTerrainComponent* Component)
	{
		SectionSizeX = Component->SectionSizeX;
		SectionSizeY = Component->SectionSizeY;
		Stride = sizeof(_WORD);
		Size = 2 * 3 * SectionSizeX * SectionSizeY * Stride;
	}

	// GetData

	void GetData(void* Buffer)
	{
		_WORD*	DestIndex = (_WORD*)Buffer;

		for(INT Y = 0;Y < SectionSizeY;Y++)
		{
			for(INT X = 0;X < SectionSizeX;X++)
			{
				INT	V1 = Y*(SectionSizeX+1) + X,
					V2 = V1+1,
					V3 = (Y+1)*(SectionSizeX+1) + (X+1),
					V4 = V3-1;

				*DestIndex++ = V1;
				*DestIndex++ = V4;
				*DestIndex++ = V3;

				*DestIndex++ = V3;
				*DestIndex++ = V2;
				*DestIndex++ = V1;
			}
		}
	}
};

//
//	FTerrainVertex
//

struct FTerrainVertex
{
#if __INTEL_BYTE_ORDER__
	BYTE	X,
			Y,
			Z_LOBYTE,
			Z_HIBYTE;
#else
BYTE		Z_HIBYTE,
			Z_LOBYTE,
			Y,
			X;
#endif
	FLOAT	Displacement;
	SWORD	GradientX,
			GradientY;
};

//
//	FTerrainVertexBuffer
//

struct FTerrainVertexBuffer: FVertexBuffer
{
	UTerrainComponent*	Component;
	UINT				MaxTesselation;

	// Constructor.

	FTerrainVertexBuffer(UTerrainComponent* InComponent,UINT InMaxTesselation):
		Component(InComponent),
		MaxTesselation(InMaxTesselation)
	{
		Stride = sizeof(FTerrainVertex);
		Size = (Component->SectionSizeX * MaxTesselation + 1) * (Component->SectionSizeY * MaxTesselation + 1) * Stride;
		Dynamic = MaxTesselation > 1;
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		BEGINCYCLECOUNTER(GEngineStats.TerrainSmoothTime);

		FTerrainVertex*	DestVertex = (FTerrainVertex*)Buffer;
		ATerrain*		Terrain = Component->GetTerrain();

		FPatchSampler	PatchSampler(MaxTesselation);

		for(INT Y = 0;Y <= Component->SectionSizeY;Y++)
		{
			for(INT X = 0;X <= Component->SectionSizeX;X++)
			{
				const FTerrainPatch&	Patch = Terrain->GetPatch(Component->SectionBaseX + X,Component->SectionBaseY + Y);

				for(UINT SmoothY = 0;SmoothY < (Y < Component->SectionSizeY ? MaxTesselation : 1);SmoothY++)
				{
					FLOAT	FracY = (FLOAT)SmoothY / (FLOAT)MaxTesselation;

					for(UINT SmoothX = 0;SmoothX < (X < Component->SectionSizeX ? MaxTesselation : 1);SmoothX++)
					{
						FLOAT	FracX = (FLOAT)SmoothX / (FLOAT)MaxTesselation;

						DestVertex->X = X * MaxTesselation + SmoothX;
						DestVertex->Y = Y * MaxTesselation + SmoothY;
						
						_WORD Z = (_WORD)appTrunc(PatchSampler.Sample(Patch,SmoothX,SmoothY));
						DestVertex->Z_LOBYTE = Z & 255;
						DestVertex->Z_HIBYTE = Z >> 8;

						DestVertex->Displacement = Terrain->GetCachedDisplacement(
														(Component->SectionBaseX + X),
														(Component->SectionBaseY + Y),
														SmoothX * Terrain->MaxTesselationLevel / MaxTesselation,
														SmoothY * Terrain->MaxTesselationLevel / MaxTesselation
														);

						DestVertex->GradientX = (SWORD)appTrunc(PatchSampler.SampleDerivX(Patch,SmoothX,SmoothY));
						DestVertex->GradientY = (SWORD)appTrunc(PatchSampler.SampleDerivY(Patch,SmoothX,SmoothY));
						DestVertex++;
					}
				}
			}
		}

		ENDCYCLECOUNTER;
	}
};

//
//	FTerrainSmoothIndexBuffer
//

struct FTerrainSmoothIndexBuffer: FIndexBuffer
{
	ATerrain*			Terrain;
	UTerrainComponent*	Component;
	const UINT			MaxTesselationLevel;
	const TArray<BYTE>&	TesselationLevels;
	INT					NumTriangles;

	UINT				BatchIndex;

	// GetCachedTesselationLevel

	UINT GetCachedTesselationLevel(INT X,INT Y) const
	{
		return TesselationLevels((Y + 1) * (Component->SectionSizeX + 2) + (X + 1));
	}

	// GetVertexIndex

	FORCEINLINE _WORD GetVertexIndex(INT PatchX,INT PatchY,INT InteriorX,INT InteriorY) const
	{
		if(InteriorX >= (INT)MaxTesselationLevel)
			return GetVertexIndex(PatchX + 1,PatchY,InteriorX - MaxTesselationLevel,InteriorY);
	
		if(InteriorY >= (INT)MaxTesselationLevel)
			return GetVertexIndex(PatchX,PatchY + 1,InteriorX,InteriorY - MaxTesselationLevel);

		UINT	VertexColumnStride = Square(MaxTesselationLevel),
				VertexRowStride = Component->SectionSizeX * VertexColumnStride + MaxTesselationLevel;

		return PatchY * VertexRowStride + PatchX * (PatchY < Component->SectionSizeY ? VertexColumnStride : MaxTesselationLevel) + InteriorY * (PatchX < Component->SectionSizeX ? MaxTesselationLevel : 1) + InteriorX;
	}

	// Constructor.

	FTerrainSmoothIndexBuffer(UTerrainComponent* InComponent,const FSceneContext& Context,const TArray<BYTE>& InTesselationLevels,UINT InMaxTesselationLevel,UINT InBatchIndex):
		Component(InComponent),
		Terrain(InComponent->GetTerrain()),
		TesselationLevels(InTesselationLevels),
		MaxTesselationLevel(InMaxTesselationLevel),
		BatchIndex(InBatchIndex)
	{
		Stride = sizeof(_WORD);

		BEGINCYCLECOUNTER(GEngineStats.TerrainSmoothTime);

		// Determine the number of triangles at this tesselation level.

		NumTriangles = 0;

		for(INT Y = 0;Y < Component->SectionSizeY;Y++)
		{
			for(INT X = 0;X < Component->SectionSizeX;X++)
			{
				if(Component->PatchBatches(Y * Component->SectionSizeX + X) != BatchIndex)
					continue;

				UINT	TesselationLevel = GetCachedTesselationLevel(X,Y);

				NumTriangles += 2 * Square(TesselationLevel - 2); // Interior triangles.

				for(UINT EdgeComponent = 0;EdgeComponent < 2;EdgeComponent++)
					for(UINT EdgeSign = 0;EdgeSign < 2;EdgeSign++)
						NumTriangles += TesselationLevel - 2 + Min(
																TesselationLevel,
																GetCachedTesselationLevel(
																	X + (EdgeComponent == 0 ? (EdgeSign ? 1 : -1) : 0),
																	Y + (EdgeComponent == 1 ? (EdgeSign ? 1 : -1) : 0)
																	)
																);
			}
		}

		Size = NumTriangles * 3 * Stride;
		Dynamic = 1;

		ENDCYCLECOUNTER;
	}

	// TesselateEdge

	void TesselateEdge(_WORD*& DestIndex,UINT EdgeTesselation,UINT TesselationLevel,UINT X,UINT Y,UINT EdgeX,UINT EdgeY,UINT EdgeInnerX,UINT EdgeInnerY,UINT InnerX,UINT InnerY,UINT DeltaX,UINT DeltaY,UINT VertexOrder)
	{
		check(EdgeTesselation <= TesselationLevel);

		UINT	EdgeVertices[TERRAIN_MAXTESSELATION + 1];
		UINT	InnerVertices[TERRAIN_MAXTESSELATION - 1];

		for(UINT VertexIndex = 0;VertexIndex <= EdgeTesselation;VertexIndex++)
			EdgeVertices[VertexIndex] = GetVertexIndex(
				EdgeX,
				EdgeY,
				EdgeInnerX + VertexIndex * DeltaX * MaxTesselationLevel / EdgeTesselation,
				EdgeInnerY + VertexIndex * DeltaY * MaxTesselationLevel / EdgeTesselation
				);
		for(UINT VertexIndex = 1;VertexIndex <= TesselationLevel - 1;VertexIndex++)
			InnerVertices[VertexIndex - 1] = GetVertexIndex(
				X,
				Y,
				InnerX + (VertexIndex - 1) * DeltaX * MaxTesselationLevel / TesselationLevel,
				InnerY + (VertexIndex - 1) * DeltaY * MaxTesselationLevel / TesselationLevel
				);

		UINT	EdgeVertexIndex = 0,
				InnerVertexIndex = 0;
		while(EdgeVertexIndex < EdgeTesselation || InnerVertexIndex < (TesselationLevel - 2))
		{
			UINT	EdgePercent = EdgeVertexIndex * (TesselationLevel - 1),
					InnerPercent = (InnerVertexIndex + 1) * EdgeTesselation;

			if(EdgePercent < InnerPercent)
			{
				check(EdgeVertexIndex < EdgeTesselation);
				EdgeVertexIndex++;
				*DestIndex++ = EdgeVertices[EdgeVertexIndex - (1 - VertexOrder)];
				*DestIndex++ = EdgeVertices[EdgeVertexIndex - VertexOrder];
				*DestIndex++ = InnerVertices[InnerVertexIndex];
			}
			else
			{
				check(InnerVertexIndex < TesselationLevel - 2);
				InnerVertexIndex++;
				*DestIndex++ = InnerVertices[InnerVertexIndex - VertexOrder];
				*DestIndex++ = InnerVertices[InnerVertexIndex - (1 - VertexOrder)];
				*DestIndex++ = EdgeVertices[EdgeVertexIndex];
			}
		};
	}

	// FIndexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		BEGINCYCLECOUNTER(GEngineStats.TerrainSmoothTime);

		_WORD*		DestIndex = (_WORD*)Buffer;

		for(INT Y = 0;Y < Component->SectionSizeY;Y++)
		{
			for(INT X = 0;X < Component->SectionSizeX;X++)
			{
				if(Component->PatchBatches(Y * Component->SectionSizeX + X) != BatchIndex)
					continue;

				UINT	TesselationLevel = GetCachedTesselationLevel(X,Y),
						EdgeTesselationNegX = Min(TesselationLevel,GetCachedTesselationLevel(X - 1,Y)),
						EdgeTesselationPosX = Min(TesselationLevel,GetCachedTesselationLevel(X + 1,Y)),
						EdgeTesselationNegY = Min(TesselationLevel,GetCachedTesselationLevel(X,Y - 1)),
						EdgeTesselationPosY = Min(TesselationLevel,GetCachedTesselationLevel(X,Y + 1));

				if(TesselationLevel == EdgeTesselationNegX && EdgeTesselationNegX == EdgeTesselationPosX && EdgeTesselationPosX == EdgeTesselationNegY && EdgeTesselationNegY == EdgeTesselationPosY)
				{
					UINT	TesselationFactor = MaxTesselationLevel / TesselationLevel;

					_WORD	IndexCache[2][TERRAIN_MAXTESSELATION + 1];
					UINT	NextCacheLine = 1;

					IndexCache[0][0] = GetVertexIndex(X,Y,0,0);
					for(UINT SubX = 1;SubX < TesselationLevel;SubX++)
						IndexCache[0][SubX] = IndexCache[0][SubX - 1] + TesselationFactor;
					IndexCache[0][TesselationLevel] = GetVertexIndex(X + 1,Y,0,0);

					for(UINT SubY = 0;SubY < TesselationLevel;SubY++)
					{
						IndexCache[NextCacheLine][0] = GetVertexIndex(X,Y,0,(SubY + 1) * TesselationFactor);
						for(UINT SubX = 1;SubX < TesselationLevel;SubX++)
							IndexCache[NextCacheLine][SubX] = IndexCache[NextCacheLine][SubX - 1] + TesselationFactor;
						IndexCache[NextCacheLine][TesselationLevel] = GetVertexIndex(X + 1,Y,0,(SubY + 1) * TesselationFactor);

						for(UINT SubX = 0;SubX < TesselationLevel;SubX++)
						{
							_WORD	V00 = IndexCache[1 - NextCacheLine][SubX],
									V10 = IndexCache[1 - NextCacheLine][SubX + 1],
									V01 = IndexCache[NextCacheLine][SubX],
									V11 = IndexCache[NextCacheLine][SubX + 1];

							*DestIndex++ = V00;
							*DestIndex++ = V01;
							*DestIndex++ = V11;

							*DestIndex++ = V00;
							*DestIndex++ = V11;
							*DestIndex++ = V10;
						}

						NextCacheLine = 1 - NextCacheLine;
					}
				}
				else
				{
					UINT	TesselationFactor = MaxTesselationLevel / TesselationLevel;

					// Interior triangles.

					for(UINT SubX = 1;SubX < TesselationLevel - 1;SubX++)
					{
						for(UINT SubY = 1;SubY < TesselationLevel - 1;SubY++)
						{
							_WORD	V00 = GetVertexIndex(X,Y,SubX * TesselationFactor,SubY * TesselationFactor),
									V10 = GetVertexIndex(X,Y,(SubX + 1) * TesselationFactor,SubY * TesselationFactor),
									V01 = GetVertexIndex(X,Y,SubX * TesselationFactor,(SubY + 1) * TesselationFactor),
									V11 = GetVertexIndex(X,Y,(SubX + 1) * TesselationFactor,(SubY + 1) * TesselationFactor);

							*DestIndex++ = V00;
							*DestIndex++ = V01;
							*DestIndex++ = V11;

							*DestIndex++ = V00;
							*DestIndex++ = V11;
							*DestIndex++ = V10;
						}
					}

					// Edges.

					TesselateEdge(DestIndex,EdgeTesselationNegX,TesselationLevel,X,Y,X,Y,0,0,MaxTesselationLevel / TesselationLevel,MaxTesselationLevel / TesselationLevel,0,1,0);
					TesselateEdge(DestIndex,EdgeTesselationPosX,TesselationLevel,X,Y,X + 1,Y,0,0,MaxTesselationLevel - MaxTesselationLevel / TesselationLevel,MaxTesselationLevel / TesselationLevel,0,1,1);
					TesselateEdge(DestIndex,EdgeTesselationNegY,TesselationLevel,X,Y,X,Y,0,0,MaxTesselationLevel / TesselationLevel,MaxTesselationLevel / TesselationLevel,1,0,1);
					TesselateEdge(DestIndex,EdgeTesselationPosY,TesselationLevel,X,Y,X,Y + 1,0,0,MaxTesselationLevel / TesselationLevel,MaxTesselationLevel - MaxTesselationLevel / TesselationLevel,1,0,0);
				}
			}
		}

		ENDCYCLECOUNTER;
	}
};

//
//	FTerrainFoliageInstance
//

struct FTerrainFoliageInstance
{
	FVector		Location;
	BYTE		Scale[3],
				Yaw;
	FVector		Lighting;
	FVector2D	LightMapCoordinate;
};

//
//	FTerrainFoliageInstanceList
//

struct FTerrainFoliageInstanceList
{
	TArray<FTerrainFoliageInstance>	Instances;
	FTerrainFoliageMesh*			Mesh;
	UMaterialInstance*				Material;

	// Constructor.

	FTerrainFoliageInstanceList(FTerrainFoliageMesh* InMesh):
		Mesh(InMesh)
	{
		if(Mesh->Material)
		{
			Material = Mesh->Material;
		}
		else if(Mesh->StaticMesh->Materials.Num() && Mesh->StaticMesh->Materials(0).Material)
		{
			Material = Mesh->StaticMesh->Materials(0).Material;
		}
		else
		{
			Material = GEngine->DefaultMaterial;
		}
	}
};

//
//	FTerrainFoliageInstanceCache - A cache for instances of a foliage mesh on a single terrain component.
//

struct FTerrainFoliageInstanceCache: FTerrainFoliageInstanceList
{
	// Constructor.

	FTerrainFoliageInstanceCache(UTerrainComponent* Component,const FTerrainWeightedMaterial& WeightedMaterial,FTerrainFoliageMesh* InMesh):
		FTerrainFoliageInstanceList(InMesh)
	{
		BEGINCYCLECOUNTER(GEngineStats.TerrainFoliageTime);

		ATerrain*	Terrain = Component->GetTerrain();

		for(INT PatchY = 0;PatchY < Component->SectionSizeY;PatchY++)
		{
			for(INT PatchX = 0;PatchX < Component->SectionSizeX;PatchX++)
			{
				FTerrainPatch	Patches[2][2] =
				{
					{
						Terrain->GetPatch(Component->SectionBaseX + PatchX + 0,Component->SectionBaseY + PatchY + 0),
						Terrain->GetPatch(Component->SectionBaseX + PatchX + 0,Component->SectionBaseY + PatchY + 1),
					},
					{
						Terrain->GetPatch(Component->SectionBaseX + PatchX + 1,Component->SectionBaseY + PatchY + 0),
						Terrain->GetPatch(Component->SectionBaseX + PatchX + 1,Component->SectionBaseY + PatchY + 1),
					},
				};

				FVector	PatchVertices[TERRAIN_MAXTESSELATION + 1][TERRAIN_MAXTESSELATION + 1];

				for(INT SubY = 0;SubY <= Terrain->MaxTesselationLevel;SubY++)
					for(INT SubX = 0;SubX <= Terrain->MaxTesselationLevel;SubX++)
						PatchVertices[SubX][SubY] = Component->LocalToWorld.TransformFVector(
														Terrain->GetCollisionVertex(
															Patches[SubX / Terrain->MaxTesselationLevel][SubY / Terrain->MaxTesselationLevel],
															Component->SectionBaseX + PatchX + SubX / Terrain->MaxTesselationLevel,
															Component->SectionBaseY + PatchY + SubY / Terrain->MaxTesselationLevel,
															SubX & (Terrain->MaxTesselationLevel - 1),
															SubY & (Terrain->MaxTesselationLevel - 1)
															) -
														FVector(Component->SectionBaseX,Component->SectionBaseY,0)
														);

				FLOAT	Weights[2][2] =
				{
					{
						WeightedMaterial.Weight(Component->SectionBaseX + PatchX + 0,Component->SectionBaseY + PatchY + 0) / 255.0f,
						WeightedMaterial.Weight(Component->SectionBaseX + PatchX + 0,Component->SectionBaseY + PatchY + 1) / 255.0f,
					},
					{
						WeightedMaterial.Weight(Component->SectionBaseX + PatchX + 1,Component->SectionBaseY + PatchY + 0) / 255.0f,
						WeightedMaterial.Weight(Component->SectionBaseX + PatchX + 1,Component->SectionBaseY + PatchY + 1) / 255.0f,
					},
				};

				appSRandInit(Mesh->Seed + (Component->SectionBaseY + PatchY) * Terrain->NumVerticesX + Component->SectionBaseX + PatchX);

				for(INT InstanceIndex = 0;InstanceIndex < Mesh->Density;InstanceIndex++)
				{
					FLOAT	X = appSRand(),
							Y = appSRand(),
							ScaleX = appSRand(),
							ScaleY = appSRand(),
							ScaleZ = appSRand();
					FLOAT	Yaw = appSRand(),
							Random = appSRand();

					if(Random <= Lerp(Lerp(Weights[0][0],Weights[1][0],X),Lerp(Weights[0][1],Weights[1][1],X),Y))
					{
						UINT	SubX = Min(appTrunc(X * Terrain->MaxTesselationLevel),Terrain->MaxTesselationLevel - 1),
								SubY = Min(appTrunc(Y * Terrain->MaxTesselationLevel),Terrain->MaxTesselationLevel - 1);
						FVector	Location = QuadLerp(
											PatchVertices[SubX + 0][SubY + 0],
											PatchVertices[SubX + 1][SubY + 0],
											PatchVertices[SubX + 0][SubY + 1],
											PatchVertices[SubX + 1][SubY + 1],
											X * Terrain->MaxTesselationLevel - SubX,
											Y * Terrain->MaxTesselationLevel - SubY
											);

						FTerrainFoliageInstance*	FoliageInstance = new(Instances) FTerrainFoliageInstance;
						FoliageInstance->Location = Location;
						FoliageInstance->Scale[0] = (BYTE)Clamp<INT>(appTrunc(ScaleX * 255.0f),0,255);
						FoliageInstance->Scale[1] = (BYTE)Clamp<INT>(appTrunc(ScaleY * 255.0f),0,255);
						FoliageInstance->Scale[2] = (BYTE)Clamp<INT>(appTrunc(ScaleZ * 255.0f),0,255);
						FoliageInstance->Yaw = (BYTE)Clamp<INT>(appTrunc(Yaw * 255.0f),0,255);
						FoliageInstance->Lighting = FVector(0,0,0);

						// Calculate instance lighting.

						FoliageInstance->LightMapCoordinate = FVector2D(
							(PatchX + X) / (FLOAT)Component->SectionSizeX,
							(PatchY + Y) / (FLOAT)Component->SectionSizeY
							);
						for(UINT LightIndex = 0;LightIndex < (UINT)Component->Lights.Num();LightIndex++)
						{
							ULightComponent*		Light = Component->Lights(LightIndex).GetLight();
							UPointLightComponent*	PointLight = Cast<UPointLightComponent>(Light);
							FStaticTerrainLight*	StaticLight = NULL;
							FLinearColor			LightColor = (FLinearColor)Light->Color * Light->Brightness;

							for(INT StaticLightIndex = 0;StaticLightIndex < Component->StaticLights.Num();StaticLightIndex++)
							{
								if(Component->StaticLights(StaticLightIndex).Light == Light)
								{
									StaticLight = &Component->StaticLights(StaticLightIndex);
									break;
								}
							}

							if(StaticLight)
							{
								INT		IntX = appTrunc(FoliageInstance->LightMapCoordinate.X * StaticLight->SizeX),
										IntY = appTrunc(FoliageInstance->LightMapCoordinate.Y * StaticLight->SizeY);
								FLOAT	FracX = FoliageInstance->LightMapCoordinate.X * StaticLight->SizeX - IntX,
										FracY = FoliageInstance->LightMapCoordinate.Y * StaticLight->SizeY - IntY;

								if(PointLight)
									FoliageInstance->Lighting +=
										FVector(LightColor.R,LightColor.G,LightColor.B) *
										StaticLight->FilteredShadow(IntX,FracX,IntY,FracY) *
										Square(Max(0.0f,1.0f - (PointLight->GetOrigin() - FoliageInstance->Location).SizeSquared() / Square(PointLight->Radius)));
								else
									FoliageInstance->Lighting +=
										FVector(LightColor.R,LightColor.G,LightColor.B) *
										StaticLight->FilteredShadow(IntX,FracX,IntY,FracY);
							}
							else
							{
								if(PointLight)
									FoliageInstance->Lighting +=
										FVector(LightColor.R,LightColor.G,LightColor.B) *
										Square(Max(0.0f,1.0f - (PointLight->GetOrigin() - FoliageInstance->Location).SizeSquared() / Square(PointLight->Radius)));
								else
									FoliageInstance->Lighting += 
										FVector(LightColor.R,LightColor.G,LightColor.B);
							}
						}
					}
				}
			}
		}

		ENDCYCLECOUNTER;
	}
};

//
//	FTerrainObject
//

struct FTerrainObject
{
	FTerrainVertexFactory	VertexFactory;
	UTerrainComponent*		Component;
	FTerrainVertexBuffer	VertexBuffer;
	FTerrainIndexBuffer		IndexBuffer;

	FTerrainVertexBuffer*	SmoothVertexBuffer;

	TIndirectArray<FTerrainFoliageInstanceCache>	FoliageInstanceCaches;

	// Constructor.

	FTerrainObject(UTerrainComponent* InComponent):
		VertexFactory(
			InComponent->SectionSizeX,
			InComponent->SectionSizeY,
			1,
			InComponent->GetTerrain()->NumVerticesX,
			InComponent->GetTerrain()->NumVerticesY,
			InComponent->SectionBaseX,
			InComponent->SectionBaseY,
			1,
			InComponent->GetTerrain()->StaticLightingResolution,
			TERRAIN_ZSCALE
			),
		Component(InComponent),
		VertexBuffer(InComponent,1),
		IndexBuffer(InComponent),
		SmoothVertexBuffer(NULL)
	{
#if __INTEL_BYTE_ORDER__
		VertexFactory.PositionComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainVertex,X),VCT_UByte4);
#else
		VertexFactory.PositionComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainVertex,Z_HIBYTE),VCT_UByte4);
#endif
		VertexFactory.DisplacementComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainVertex,Displacement),VCT_Float1);
		VertexFactory.GradientsComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainVertex,GradientX),VCT_Short2);
	}

	// Destructor.

	~FTerrainObject()
	{
		if(SmoothVertexBuffer)
			delete SmoothVertexBuffer;
	}
};

//
//	FTerrainFoliageBatch - A batch of visible instances of a foliage mesh.
//

struct FTerrainFoliageBatch: FTerrainFoliageInstanceList
{
	// Constructor.

	FTerrainFoliageBatch(FTerrainFoliageInstanceList* InstanceList,const FVector& ViewOrigin):
		FTerrainFoliageInstanceList(InstanceList->Mesh)
	{
		BEGINCYCLECOUNTER(GEngineStats.TerrainFoliageTime);

		Instances.Empty(InstanceList->Instances.Num());

		FLOAT	DrawRadiusSquared = Square(Mesh->MaxDrawRadius);
		for(UINT InstanceIndex = 0;InstanceIndex < (UINT)InstanceList->Instances.Num();InstanceIndex++)
		{
			const FTerrainFoliageInstance&	Instance = InstanceList->Instances(InstanceIndex);
			if((Instance.Location - ViewOrigin).SizeSquared() < DrawRadiusSquared)
				new(Instances) FTerrainFoliageInstance(Instance);
		}

		GEngineStats.TerrainFoliageInstances.Value += Instances.Num();

		ENDCYCLECOUNTER;
	}
};

//
//	FTerrainFoliageVertex
//

struct FTerrainFoliageVertex
{
	FVector			Position;
	FVector2D		TexCoord,
					LightMapCoord;
	FVector			Lighting;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	BYTE			PointSources[4];
	FLOAT			SwayStrength;
};

//
//	FTerrainFoliageVertexBuffer
//

struct FTerrainFoliageVertexBuffer: FVertexBuffer
{
	UTerrainComponent*				Component;
	FTerrainFoliageInstanceList*	InstanceList;
	FVector							ViewOrigin;

	// Constructor.

	FTerrainFoliageVertexBuffer(UTerrainComponent* InComponent,FTerrainFoliageInstanceList* InInstanceList,const FVector& InViewOrigin):
		Component(InComponent),
		InstanceList(InInstanceList),
		ViewOrigin(InViewOrigin)
	{
		Dynamic = 1;
		Stride = sizeof(FTerrainFoliageVertex);
		Size = Stride * InstanceList->Instances.Num() * InstanceList->Mesh->StaticMesh->Vertices.Num();
	}

	// FVertexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		BEGINCYCLECOUNTER(GEngineStats.TerrainFoliageTime);

		FTerrainFoliageVertex*	DestVertex = (FTerrainFoliageVertex*)Buffer;

		check(InstanceList->Mesh->StaticMesh->UVBuffers.Num());
		check(InstanceList->Mesh->StaticMesh->UVBuffers(0).UVs.Num() == InstanceList->Mesh->StaticMesh->Vertices.Num());

		ATerrain*	Terrain = Component->GetTerrain();
		FLOAT		MinTransitionRadiusSquared = Square(InstanceList->Mesh->MinTransitionRadius),
					InvTransitionSize = 1.0f / Max(InstanceList->Mesh->MaxDrawRadius - InstanceList->Mesh->MinTransitionRadius,0.0f);
		FVector2D	LightMapCoordinateOffset(0.5f / (Component->SectionSizeX * Terrain->StaticLightingResolution + 1),0.5f / (Component->SectionSizeY * Terrain->StaticLightingResolution + 1));

		// Generate the instance vertices.

		ULevel*	Level = Component->Owner->XLevel;

		for(UINT InstanceIndex = 0;InstanceIndex < (UINT)InstanceList->Instances.Num();InstanceIndex++)
		{
			const FTerrainFoliageInstance&	Instance = InstanceList->Instances(InstanceIndex);
			FRotator						InstanceRotation(0,Instance.Yaw * 65535 / 255,0);
			FLOAT							DistanceSquared = (ViewOrigin - Instance.Location).SizeSquared(),
											TransitionScale = DistanceSquared < MinTransitionRadiusSquared ? 1.0f : (1.0f - (appSqrt(DistanceSquared) - InstanceList->Mesh->MinTransitionRadius) * InvTransitionSize);
			FVector							InstanceScale = Lerp(FVector(1,1,1) * InstanceList->Mesh->MinScale,FVector(1,1,1) * InstanceList->Mesh->MaxScale,FVector(Instance.Scale[0] / 255.0f,Instance.Scale[1] / 255.0f,Instance.Scale[2] / 255.0f)) * TransitionScale;
			FMatrix							NormalLocalToWorld = FRotationMatrix(InstanceRotation),
											LocalToWorld = NormalLocalToWorld * FScaleMatrix(InstanceScale) * FTranslationMatrix(Instance.Location);
			BYTE							PointSources[4] = { 255, 255, 255, 255 };
			UINT							NumPointSources = 0;

			for(UINT SourceIndex = 0;SourceIndex < (UINT)Level->WindPointSources.Num() && NumPointSources < 4 && SourceIndex < 100;SourceIndex++)
			{
				UWindPointSourceComponent*	PointSource = Level->WindPointSources(SourceIndex);
				if((PointSource->Position - Instance.Location).SizeSquared() < Square(PointSource->Radius))
					PointSources[NumPointSources++] = SourceIndex;
			}

			for(UINT VertexIndex = 0;VertexIndex < (UINT)InstanceList->Mesh->StaticMesh->Vertices.Num();VertexIndex++)
			{
				const FStaticMeshVertex&	SourceVertex = InstanceList->Mesh->StaticMesh->Vertices(VertexIndex);
				DestVertex->Position = LocalToWorld.TransformFVector(SourceVertex.Position);
				DestVertex->TexCoord = InstanceList->Mesh->StaticMesh->UVBuffers(0).UVs(VertexIndex);
				DestVertex->LightMapCoord = Instance.LightMapCoordinate + LightMapCoordinateOffset;
				DestVertex->TangentX = NormalLocalToWorld.TransformNormal(SourceVertex.TangentX);
				DestVertex->TangentY = NormalLocalToWorld.TransformNormal(SourceVertex.TangentY);
				DestVertex->TangentZ = NormalLocalToWorld.TransformNormal(SourceVertex.TangentZ);
				DestVertex->Lighting = Instance.Lighting;
				DestVertex->PointSources[0] = PointSources[0];
				DestVertex->PointSources[1] = PointSources[1];
				DestVertex->PointSources[2] = PointSources[2];
				DestVertex->PointSources[3] = PointSources[3];
				DestVertex->SwayStrength = SourceVertex.Position.Z * InstanceList->Mesh->SwayScale;
				DestVertex++;
			}
		}

		ENDCYCLECOUNTER;
	}
};

//
//	FTerrainFoliageIndexBuffer
//

struct FTerrainFoliageIndexBuffer: FIndexBuffer
{
	FTerrainFoliageInstanceList*	InstanceList;

	// Constructor.

	FTerrainFoliageIndexBuffer(FTerrainFoliageInstanceList* InInstanceList):
		InstanceList(InInstanceList)
	{
		Dynamic = 1;
		Stride = sizeof(_WORD);
		Size = Stride * InstanceList->Instances.Num() * InstanceList->Mesh->StaticMesh->IndexBuffer.Indices.Num();
	}

	// FIndexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		BEGINCYCLECOUNTER(GEngineStats.TerrainFoliageTime);

		_WORD*	DestIndex = (_WORD*)Buffer;

		for(UINT InstanceIndex = 0;InstanceIndex < (UINT)InstanceList->Instances.Num();InstanceIndex++)
		{
			_WORD	BaseVertexIndex = InstanceIndex * InstanceList->Mesh->StaticMesh->Vertices.Num();
			_WORD*	SourceIndex = &InstanceList->Mesh->StaticMesh->IndexBuffer.Indices(0);
			for(UINT Index = 0;Index < (UINT)InstanceList->Mesh->StaticMesh->IndexBuffer.Indices.Num();Index++)
				*DestIndex++ = BaseVertexIndex + *SourceIndex++;
		}

		ENDCYCLECOUNTER;
	}
};

//
//	FTerrainFoliageRenderBatch - Rendering data for a foliage instance batch that must be constructed after the batch is created.
//

struct FTerrainFoliageRenderBatch: FTerrainFoliageBatch
{
	FTerrainFoliageVertexBuffer	VertexBuffer;
	FTerrainFoliageVertexBuffer	LitVertexBuffer;
	FTerrainFoliageIndexBuffer	IndexBuffer;
	FFoliageVertexFactory		VertexFactory;

	UINT	NumVertices,
			NumTriangles;

	// Constructor.

	FTerrainFoliageRenderBatch(UTerrainComponent* Component,FTerrainFoliageInstanceList* InstanceList,FVector ViewOrigin):
		FTerrainFoliageBatch(InstanceList,ViewOrigin),
		VertexBuffer(Component,this,ViewOrigin),
		LitVertexBuffer(Component,this,ViewOrigin),
		IndexBuffer(this)
	{
		VertexFactory.DynamicVertexBuffer = &LitVertexBuffer;
		VertexFactory.PositionComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,Position),VCT_Float3);
		VertexFactory.TextureCoordinateComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,TexCoord),VCT_Float2);
		VertexFactory.LightMapCoordinateComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,LightMapCoord),VCT_Float2);
		VertexFactory.StaticLightingComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,Lighting),VCT_Float3);
		VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,TangentX),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,TangentY),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,TangentZ),VCT_PackedNormal);
		VertexFactory.PointSourcesComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,PointSources),VCT_UByte4);
		VertexFactory.SwayStrengthComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FTerrainFoliageVertex,SwayStrength),VCT_Float1);

		for(UINT SourceIndex = 0;SourceIndex < (UINT)Component->Owner->XLevel->WindPointSources.Num() && SourceIndex < 100;SourceIndex++)
			VertexFactory.WindPointSources.AddItem(Component->Owner->XLevel->WindPointSources(SourceIndex)->GetRenderData());

		for(UINT SourceIndex = 0;SourceIndex < (UINT)Component->Owner->XLevel->WindDirectionalSources.Num() && SourceIndex < 100;SourceIndex++)
			VertexFactory.WindDirectionalSources.AddItem(Component->Owner->XLevel->WindDirectionalSources(SourceIndex)->GetRenderData());

		GResourceManager->UpdateResource(&VertexFactory);

		NumVertices = VertexBuffer.Num();
		NumTriangles = IndexBuffer.Num() / 3;
	}
};

//
//	FTerrainViewInterface
//

struct FTerrainViewInterface: FPrimitiveViewInterface
{
	ATerrain*			Terrain;
	UTerrainComponent*	Component;

	TArray<BYTE>						TesselationLevels;
	UINT								MaxTesselation;
	FTerrainVertexFactory*				SmoothVertexFactory;
	TArray<FTerrainSmoothIndexBuffer>	BatchIndexBuffers;

	TArray<FTerrainFoliageRenderBatch>	FoliageRenderBatches;

	// Constructor/destructor.

	FTerrainViewInterface(UTerrainComponent* InComponent,const FSceneContext& Context):
		Component(InComponent)
	{
		Terrain = Component->GetTerrain();

		// Create foliage instance batches.

		if(Context.View->ProjectionMatrix.M[3][3] < 1.0f)
		{
			FVector	ViewOrigin = Context.View->ViewMatrix.Inverse().GetOrigin();
			FLOAT	Distance = (Component->Bounds.Origin - ViewOrigin).Size() - Component->Bounds.SphereRadius;

			// Clean out cached foliage instances which are no longer needed for this component.

			for(UINT CacheIndex = 0;CacheIndex < (UINT)Component->TerrainObject->FoliageInstanceCaches.Num();CacheIndex++)
				if(Distance >= Component->TerrainObject->FoliageInstanceCaches(CacheIndex).Mesh->MaxDrawRadius)
				{
					Component->TerrainObject->FoliageInstanceCaches.Remove(CacheIndex--);
					continue;
				}

			// Build batches of foliage mesh instances which are within their MaxDrawRadius.

			for(UINT MaterialIndex = 0;MaterialIndex < (UINT)Terrain->WeightedMaterials.Num();MaterialIndex++)
			{
				UTerrainMaterial*	TerrainMaterial = Terrain->WeightedMaterials(MaterialIndex).Material;
				for(UINT MeshIndex = 0;MeshIndex < (UINT)TerrainMaterial->FoliageMeshes.Num();MeshIndex++)
				{
					FTerrainFoliageMesh*	Mesh = &TerrainMaterial->FoliageMeshes(MeshIndex);

					if(Distance < Mesh->MaxDrawRadius && Mesh->StaticMesh)
					{
						FTerrainFoliageInstanceCache*	InstanceCache = NULL;
						for(UINT CacheIndex = 0;CacheIndex < (UINT)Component->TerrainObject->FoliageInstanceCaches.Num();CacheIndex++)
						{
							if(Component->TerrainObject->FoliageInstanceCaches(CacheIndex).Mesh == Mesh)
							{
								InstanceCache = &Component->TerrainObject->FoliageInstanceCaches(CacheIndex);
								break;
							}
						}
						if(!InstanceCache)
							InstanceCache = new(Component->TerrainObject->FoliageInstanceCaches) FTerrainFoliageInstanceCache(Component,Terrain->WeightedMaterials(MaterialIndex),Mesh);

						new(FoliageRenderBatches) FTerrainFoliageRenderBatch(Component,InstanceCache,ViewOrigin);
					}
				}
			}
		}

		// Calculate the maximum tesselation level for this sector.

		const FMatrix&	LocalToView = Component->LocalToWorld * Context.View->ViewMatrix;

		MaxTesselation = 1;

		if(Context.View->ProjectionMatrix.M[3][3] < 1.0f)
		{
			for(INT X = 0;X < 2;X++)
				for(INT Y = 0;Y < 2;Y++)
					for(INT Z = 0;Z < 2;Z++)
						MaxTesselation = Max(
							MaxTesselation,
							Min(
								(UINT)Terrain->MaxTesselationLevel,
								TesselationLevel(Context.View->ViewMatrix.TransformFVector(FVector(Component->Bounds.GetBoxExtrema(X).X,Component->Bounds.GetBoxExtrema(Y).Y,Component->Bounds.GetBoxExtrema(Z).Z)).Z * Terrain->TesselationDistanceScale)
								)
							);
		}

		// Setup a vertex buffer/factory for this tesselation level.

		if(MaxTesselation > 1)
		{
			if(!Component->TerrainObject->SmoothVertexBuffer || Component->TerrainObject->SmoothVertexBuffer->MaxTesselation != MaxTesselation)
			{
				if(Component->TerrainObject->SmoothVertexBuffer)
				{
					delete Component->TerrainObject->SmoothVertexBuffer;
					Component->TerrainObject->SmoothVertexBuffer = NULL;
				}
				Component->TerrainObject->SmoothVertexBuffer = new FTerrainVertexBuffer(Component,MaxTesselation);
			}

			SmoothVertexFactory = new FTerrainVertexFactory(
				Component->SectionSizeX,
				Component->SectionSizeY,
				MaxTesselation,
				Terrain->NumVerticesX,
				Terrain->NumVerticesY,
				Component->SectionBaseX,
				Component->SectionBaseY,
				1,
				Terrain->StaticLightingResolution,
				TERRAIN_ZSCALE
				);
#if __INTEL_BYTE_ORDER__
			SmoothVertexFactory->PositionComponent = FVertexStreamComponent(Component->TerrainObject->SmoothVertexBuffer,STRUCT_OFFSET(FTerrainVertex,X),VCT_UByte4);
#else
			SmoothVertexFactory->PositionComponent = FVertexStreamComponent(Component->TerrainObject->SmoothVertexBuffer,STRUCT_OFFSET(FTerrainVertex,Z_HIBYTE),VCT_UByte4);
#endif
			SmoothVertexFactory->DisplacementComponent = FVertexStreamComponent(Component->TerrainObject->SmoothVertexBuffer,STRUCT_OFFSET(FTerrainVertex,Displacement),VCT_Float1);
			SmoothVertexFactory->GradientsComponent = FVertexStreamComponent(Component->TerrainObject->SmoothVertexBuffer,STRUCT_OFFSET(FTerrainVertex,GradientX),VCT_Short2);
			SmoothVertexFactory->LocalToWorld = Component->LocalToWorld;
			SmoothVertexFactory->DynamicVertexBuffer = Component->TerrainObject->SmoothVertexBuffer;

			// Determine the tesselation level of each terrain quad.

			const FMatrix&	LocalToView = Component->LocalToWorld * Context.View->ViewMatrix;

			TesselationLevels.Empty((Component->SectionSizeX + 2) * (Component->SectionSizeY + 2));
			for(INT Y = -1;Y <= Component->SectionSizeY;Y++)
			{
				for(INT X = -1;X <= Component->SectionSizeX;X++)
				{
					if(Component->SectionBaseX + X >= Terrain->NumVerticesX || Component->SectionBaseY + Y >= Terrain->NumVerticesY || Component->SectionBaseX + X < 0 || Component->SectionBaseY + Y < 0)
					{
						TesselationLevels.AddItem(Terrain->MaxTesselationLevel);
						continue;
					}

					TesselationLevels.AddItem(
						(BYTE)Min<UINT>(
							Terrain->MaxTesselationLevel,
							TesselationLevel(
								LocalToView.TransformFVector(FVector(X,Y,(-32768.0f + Terrain->Height(Component->SectionBaseX + X,Component->SectionBaseY + Y)) * TERRAIN_ZSCALE)).Z * Terrain->TesselationDistanceScale
								)
							)
						);
				}
			}

			// Create batch index buffers.

			for(UINT BatchIndex = 0;BatchIndex < (UINT)Component->BatchMaterials.Num();BatchIndex++)
				new(BatchIndexBuffers) FTerrainSmoothIndexBuffer(Component,Context,TesselationLevels,MaxTesselation,BatchIndex);
		}
		else if(Component->TerrainObject->SmoothVertexBuffer)
		{
			delete Component->TerrainObject->SmoothVertexBuffer;
			Component->TerrainObject->SmoothVertexBuffer = NULL;
		}
	}

	virtual ~FTerrainViewInterface()
	{
		if(MaxTesselation > 1)
			delete SmoothVertexFactory;
	}

	// FPrimitiveViewInterface interface.

	virtual UPrimitiveComponent* GetPrimitive() { return Component; }

	virtual void FinalizeView() { delete this; }

	virtual DWORD GetLayerMask(const FSceneContext& Context) const
	{
		DWORD	LayerMask = 0;
		if(Context.View->ShowFlags & SHOW_Terrain)
			LayerMask |= PLM_Opaque;
		if(Context.View->ShowFlags & SHOW_Foliage)
		{
			for(INT FoliageIndex = 0;FoliageIndex < FoliageRenderBatches.Num();FoliageIndex++)
			{
				UMaterialInstance*	Material = FoliageRenderBatches(FoliageIndex).Material;
				if(Material)
					LayerMask |= Material->GetLayerMask();
			}
		}
		return LayerMask;
	}

	void RenderTerrain(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
	{
		if(Context.View->ShowFlags & SHOW_Terrain)
		{
			if(Context.View->ViewMode & SVM_WireframeMask)
			{
				GEngineStats.TerrainTriangles.Value += Component->SectionSizeX * Component->SectionSizeY * 2;

				if(MaxTesselation > 1)
				{
					for(UINT BatchIndex = 0;BatchIndex < (UINT)BatchIndexBuffers.Num();BatchIndex++)
					{
						if(!BatchIndexBuffers(BatchIndex).NumTriangles)
							continue;

						GEngineStats.TerrainTriangles.Value += BatchIndexBuffers(BatchIndex).NumTriangles;
						PRI->DrawWireframe(
							SmoothVertexFactory,
							&BatchIndexBuffers(BatchIndex),
							WT_TriList,
							FColor(255,255,255),
							0,
							BatchIndexBuffers(BatchIndex).NumTriangles,
							0,
							Component->TerrainObject->SmoothVertexBuffer->Num() - 1,
							0
							);
					}
				}
				else
				{
					GEngineStats.TerrainTriangles.Value += Component->SectionSizeX * Component->SectionSizeY * 2;
					PRI->DrawWireframe(
						&Component->TerrainObject->VertexFactory,
						&Component->TerrainObject->IndexBuffer,
						WT_TriList,
						FColor(255,255,255),
						0,
						Component->SectionSizeX * Component->SectionSizeY * 2,
						0,
						(Component->SectionSizeX + 1) * (Component->SectionSizeY + 1) - 1,
						0
						);
				}
			}
			else
			{
				if(MaxTesselation > 1)
				{
					for(UINT BatchIndex = 0;BatchIndex < (UINT)BatchIndexBuffers.Num();BatchIndex++)
					{
						if(!BatchIndexBuffers(BatchIndex).NumTriangles)
							continue;

						GEngineStats.TerrainTriangles.Value += BatchIndexBuffers(BatchIndex).NumTriangles;
						PRI->DrawMesh(
							SmoothVertexFactory,
							&BatchIndexBuffers(BatchIndex),
							Terrain->GetCachedMaterial(Component->BatchMaterials(BatchIndex)),
							Terrain->GetCachedMaterialInstance(),
							0,
							BatchIndexBuffers(BatchIndex).NumTriangles,
							0,
							Component->TerrainObject->SmoothVertexBuffer->Num() - 1
							);
					}
				}
				else
				{
					GEngineStats.TerrainTriangles.Value += Component->SectionSizeX * Component->SectionSizeY * 2;
					PRI->DrawMesh(
						&Component->TerrainObject->VertexFactory,
						&Component->TerrainObject->IndexBuffer,
						Terrain->GetCachedMaterial(Component->BatchMaterials(Component->FullBatch)),
						Terrain->GetCachedMaterialInstance(),
						0,
						Component->SectionSizeX * Component->SectionSizeY * 2,
						0,
						(Component->SectionSizeX + 1) * (Component->SectionSizeY + 1) - 1
						);
				}
			}
		}
	}

	virtual void RenderShadowDepth(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
	{
		RenderTerrain(Context,PRI);
	}

	virtual void Render(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
	{
		if(Context.View->ShowFlags & SHOW_Foliage)
		{
			FVector	ViewOrigin = Context.View->ViewMatrix.Inverse().GetOrigin();

			for(UINT BatchIndex = 0;BatchIndex < (UINT)FoliageRenderBatches.Num();BatchIndex++)
			{
				FTerrainFoliageRenderBatch&	RenderBatch = FoliageRenderBatches(BatchIndex);
				if(RenderBatch.NumTriangles > 0)
				{
					if(RenderBatch.Material->GetMaterialInterface(0)->NonDirectionalLighting && RenderBatch.Mesh->StaticLighting)
						PRI->DrawVertexLitMesh(
							&RenderBatch.VertexFactory,
							&RenderBatch.IndexBuffer,
							Component->SelectedMaterial(Context,RenderBatch.Material),
							RenderBatch.Material->GetInstanceInterface(),
							0,
							RenderBatch.NumTriangles,
							0,
							RenderBatch.NumVertices - 1
							);
					else
						PRI->DrawMesh(
							&RenderBatch.VertexFactory,
							&RenderBatch.IndexBuffer,
							Component->SelectedMaterial(Context,RenderBatch.Material),
							RenderBatch.Material->GetInstanceInterface(),
							0,
							RenderBatch.NumTriangles,
							0,
							RenderBatch.NumVertices - 1
							);
				}
			}
		}

		RenderTerrain(Context,PRI);
	}
};

//
//	FStaticTerrainLight::Render
//

void FStaticTerrainLight::Render(const struct FSceneContext& Context,FPrimitiveViewInterface* PVI,FStaticLightPrimitiveRenderInterface* PRI)
{
	if(Context.View->ShowFlags & SHOW_Terrain)
	{
		ATerrain*				Terrain = TerrainComponent->GetTerrain();
		FTerrainViewInterface*	TerrainPVI = (FTerrainViewInterface*)PVI;

		if(Context.View->ShowFlags & SHOW_Foliage)
		{
			for(UINT BatchIndex = 0;BatchIndex < (UINT)TerrainPVI->FoliageRenderBatches.Num();BatchIndex++)
			{
				FTerrainFoliageRenderBatch&	RenderBatch = TerrainPVI->FoliageRenderBatches(BatchIndex);
				if(RenderBatch.NumTriangles > 0 && (!RenderBatch.Material->GetMaterial()->NonDirectionalLighting || !RenderBatch.Mesh->StaticLighting))
					PRI->DrawVisibilityTexture(
						&RenderBatch.VertexFactory,
						&RenderBatch.IndexBuffer,
						TerrainComponent->SelectedMaterial(Context,RenderBatch.Material),
						Terrain->GetCachedMaterialInstance(),
						this,
						0,
						RenderBatch.NumTriangles,
						0,
						RenderBatch.NumVertices - 1
						);
			}
		}

		if(TerrainPVI->MaxTesselation > 1)
		{
			for(INT BatchIndex = 0;BatchIndex < TerrainPVI->BatchIndexBuffers.Num();BatchIndex++)
			{
				FTerrainSmoothIndexBuffer&	IndexBuffer = TerrainPVI->BatchIndexBuffers(BatchIndex);
				if(!IndexBuffer.NumTriangles)
					continue;

				GEngineStats.TerrainTriangles.Value += IndexBuffer.NumTriangles;
				PRI->DrawVisibilityTexture(
					TerrainPVI->SmoothVertexFactory,
					&IndexBuffer,
					Terrain->GetCachedMaterial(TerrainComponent->BatchMaterials(BatchIndex)),
					Terrain->GetCachedMaterialInstance(),
					this,
					0,
					IndexBuffer.NumTriangles,
					0,
					TerrainComponent->TerrainObject->SmoothVertexBuffer->Num() - 1
					);
			}
		}
		else
		{
			GEngineStats.TerrainTriangles.Value += TerrainComponent->SectionSizeX * TerrainComponent->SectionSizeY * 2;
			PRI->DrawVisibilityTexture(
				&TerrainComponent->TerrainObject->VertexFactory,
				&TerrainComponent->TerrainObject->IndexBuffer,
				Terrain->GetCachedMaterial(TerrainComponent->BatchMaterials(TerrainComponent->FullBatch)),
				Terrain->GetCachedMaterialInstance(),
				this,
				0,
				TerrainComponent->SectionSizeX * TerrainComponent->SectionSizeY * 2,
				0,
				(TerrainComponent->SectionSizeX + 1) * (TerrainComponent->SectionSizeY + 1) - 1
				);
		}
	}

}

//
//	UTerrainComponent::Created
//

INT AddUniqueMask(TArray<FTerrainMaterialMask>& Array,const FTerrainMaterialMask& Mask)
{
	INT	Index = Array.FindItemIndex(Mask);
	if(Index == INDEX_NONE)
	{
		Index = Array.Num();
		new(Array) FTerrainMaterialMask(Mask);
	}
	return Index;
}

void UTerrainComponent::Created()
{
	UpdatePatchBounds();

	Super::Created();

	TerrainObject = new FTerrainObject(this);
	TerrainObject->VertexFactory.LocalToWorld = LocalToWorld;
	GResourceManager->UpdateResource(&TerrainObject->VertexFactory);

	// Build a list of unique blended material combinations used by quads in this terrain section.

	ATerrain*				Terrain = GetTerrain();
	FTerrainMaterialMask	FullMask(Terrain->WeightedMaterials.Num());

	PatchBatches.Empty(SectionSizeX * SectionSizeY);
	BatchMaterials.Empty();

	for(INT Y = SectionBaseY;Y < SectionBaseY + SectionSizeY;Y++)
	{
		for(INT X = SectionBaseX;X < SectionBaseX + SectionSizeX;X++)
		{
			FTerrainMaterialMask	Mask(Terrain->WeightedMaterials.Num());

			for(INT MaterialIndex = 0;MaterialIndex < Terrain->WeightedMaterials.Num();MaterialIndex++)
			{
				FTerrainWeightedMaterial&	WeightedMaterial = Terrain->WeightedMaterials(MaterialIndex);
				UINT						TotalWeight =	(UINT)WeightedMaterial.Weight(X + 0,Y + 0) +
															(UINT)WeightedMaterial.Weight(X + 1,Y + 0) +
															(UINT)WeightedMaterial.Weight(X + 0,Y + 1) +
															(UINT)WeightedMaterial.Weight(X + 1,Y + 1);
				Mask.Set(MaterialIndex,Mask.Get(MaterialIndex) || TotalWeight > 0);
				FullMask.Set(MaterialIndex,FullMask.Get(MaterialIndex) || TotalWeight > 0);
			}

			PatchBatches.AddItem(AddUniqueMask(BatchMaterials,Mask));
		}
	}

	FullBatch = AddUniqueMask(BatchMaterials,FullMask);

}

//
//	UTerrainComponent::Update
//

void UTerrainComponent::Update()
{
	Super::Update();

	TerrainObject->VertexFactory.LocalToWorld = LocalToWorld;
	GResourceManager->UpdateResource(&TerrainObject->VertexBuffer);
	GResourceManager->UpdateResource(&TerrainObject->VertexFactory);

}

//
//	UTerrainComponent::Destroyed
//

void UTerrainComponent::Destroyed()
{
	Super::Destroyed();

	delete TerrainObject;
	TerrainObject = NULL;

}

//
//	UTerrainComponent::Precache
//

void UTerrainComponent::Precache(FPrecacheInterface* P)
{
	check(Initialized);

	for(INT LightIndex = 0;LightIndex < StaticLights.Num();LightIndex++)
		P->CacheResource(&StaticLights(LightIndex));

	ATerrain*	Terrain = GetTerrain();
	for(INT MaterialIndex = 0;MaterialIndex < BatchMaterials.Num();MaterialIndex++)
		P->CacheResource(Terrain->GetCachedMaterial(BatchMaterials(MaterialIndex)));

	P->CacheResource(&TerrainObject->VertexFactory);
	P->CacheResource(&TerrainObject->VertexBuffer);
	P->CacheResource(&TerrainObject->IndexBuffer);
}

//
//	UTerrainComponent::CreateViewInterface
//

FPrimitiveViewInterface* UTerrainComponent::CreateViewInterface(const struct FSceneContext& Context)
{
	return new FTerrainViewInterface(this,Context);
}
