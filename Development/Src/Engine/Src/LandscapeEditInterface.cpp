/*=============================================================================
LandscapeEditInterface.cpp: Landscape editing interface
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"
#include "UnTerrain.h"
#include "LandscapeDataAccess.h"
#include "LandscapeEdit.h"
#include "LandscapeRender.h"

#if WITH_EDITOR

//
// FLandscapeEditDataInterface
//
FLandscapeEditDataInterface::FLandscapeEditDataInterface(ULandscapeInfo* InLandscapeInfo)
{
	if (InLandscapeInfo && InLandscapeInfo->LandscapeProxy)
	{
		LandscapeInfo = InLandscapeInfo;
		ComponentSizeQuads = InLandscapeInfo->LandscapeProxy->ComponentSizeQuads;
		SubsectionSizeQuads = InLandscapeInfo->LandscapeProxy->SubsectionSizeQuads;
		ComponentNumSubsections = InLandscapeInfo->LandscapeProxy->NumSubsections;
		DrawScale = InLandscapeInfo->LandscapeProxy->DrawScale3D * InLandscapeInfo->LandscapeProxy->DrawScale;
	}
}

FLandscapeEditDataInterface::~FLandscapeEditDataInterface()
{
	Flush();
}

void FLandscapeEditDataInterface::Flush()
{
	UBOOL bNeedToWaitForUpdate = FALSE;

	// Update all textures
	for( TMap<UTexture2D*, FLandscapeTextureDataInfo*>::TIterator It(TextureDataMap); It;  ++It )
	{
		if( It.Value()->UpdateTextureData() )
		{
			bNeedToWaitForUpdate = TRUE;
		}
	}

	if( bNeedToWaitForUpdate )
	{
		FlushRenderingCommands();
	}

	// delete all the FLandscapeTextureDataInfo allocations
	for( TMap<UTexture2D*, FLandscapeTextureDataInfo*>::TIterator It(TextureDataMap); It;  ++It )
	{
		delete It.Value();
	}

	TextureDataMap.Empty();	// FLandscapeTextureDataInfo destructors will unlock any texture data
}

#include "LevelUtils.h"

// Include Components with overlapped vertices
void ALandscape::CalcComponentIndicesOverlap(const INT X1, const INT Y1, const INT X2, const INT Y2, const INT ComponentSizeQuads, 
											 INT& ComponentIndexX1, INT& ComponentIndexY1, INT& ComponentIndexX2, INT& ComponentIndexY2)
{
	// Find component range for this block of data
	ComponentIndexX1 = (X1-1 >= 0) ? (X1-1) / ComponentSizeQuads : (X1) / ComponentSizeQuads - 1;	// -1 because we need to pick up vertices shared between components
	ComponentIndexY1 = (Y1-1 >= 0) ? (Y1-1) / ComponentSizeQuads : (Y1) / ComponentSizeQuads - 1;
	ComponentIndexX2 = (X2 >= 0) ? X2 / ComponentSizeQuads : (X2+1) / ComponentSizeQuads - 1;
	ComponentIndexY2 = (Y2 >= 0) ? Y2 / ComponentSizeQuads : (Y2+1) / ComponentSizeQuads - 1;
}

// Exclude Components with overlapped vertices
void ALandscape::CalcComponentIndices(const INT X1, const INT Y1, const INT X2, const INT Y2, const INT ComponentSizeQuads, 
									  INT& ComponentIndexX1, INT& ComponentIndexY1, INT& ComponentIndexX2, INT& ComponentIndexY2)
{
	// Find component range for this block of data
	ComponentIndexX1 = (X1 >= 0) ? X1 / ComponentSizeQuads : (X1+1) / ComponentSizeQuads - 1;	// -1 because we need to pick up vertices shared between components
	ComponentIndexY1 = (Y1 >= 0) ? Y1 / ComponentSizeQuads : (Y1+1) / ComponentSizeQuads - 1;
	ComponentIndexX2 = (X2-1 >= 0) ? (X2-1) / ComponentSizeQuads : (X2) / ComponentSizeQuads - 1;
	ComponentIndexY2 = (Y2-1 >= 0) ? (Y2-1) / ComponentSizeQuads : (Y2) / ComponentSizeQuads - 1;
	// Shrink indices for shared values
	if ( ComponentIndexX2 < ComponentIndexX1)
	{
		ComponentIndexX2 = ComponentIndexX1;
	}
	if ( ComponentIndexY2 < ComponentIndexY1)
	{
		ComponentIndexY2 = ComponentIndexY1;
	}
}

namespace
{
	// Ugly helper function, all arrays should be only size 4
	template<typename T>
	FORCEINLINE void CalcInterpValue( const INT* Dist, const UBOOL* Exist, const T* Value, FLOAT& ValueX, FLOAT& ValueY)
	{
		if (Exist[0] && Exist[1])
		{
			ValueX = (FLOAT)(Dist[1] * Value[0] + Dist[0] * Value[1]) / (Dist[0] + Dist[1]);
		}
		else
		{
			if (Exist[0])
			{
				ValueX = Value[0];
			}
			else if (Exist[1])
			{
				ValueX = Value[1];
			}
		}

		if (Exist[2] && Exist[3])
		{
			ValueY = (FLOAT)(Dist[3] * Value[2] + Dist[2] * Value[3]) / (Dist[2] + Dist[3]);
		}
		else
		{
			if (Exist[2])
			{
				ValueY = Value[2];
			}
			else if (Exist[3])
			{
				ValueY = Value[3];
			}
		}
	}

	template<typename T>
	FORCEINLINE T CalcValueFromValueXY( const INT* Dist, const T& ValueX, const T& ValueY, const BYTE& CornerSet, const T* CornerValues )
	{
		T FinalValue;
		INT DistX = Min(Dist[0], Dist[1]);
		INT DistY = Min(Dist[2], Dist[3]);
		if (DistX+DistY > 0)
		{
			FinalValue = ((ValueX * DistY) + (ValueY * DistX)) / (FLOAT)(DistX + DistY);
		}
		else
		{
			if ((CornerSet & 1) && Dist[0] == 0 && Dist[2] == 0)
			{
				FinalValue = CornerValues[0];
			}
			else if ((CornerSet & 1 << 1) && Dist[1] == 0 && Dist[2] == 0)
			{
				FinalValue = CornerValues[1];
			}
			else if ((CornerSet & 1 << 2) && Dist[0] == 0 && Dist[3] == 0)
			{
				FinalValue = CornerValues[2];
			}
			else if ((CornerSet & 1 << 3) && Dist[1] == 0 && Dist[3] == 0)
			{
				FinalValue = CornerValues[3];
			}
			else
			{
				FinalValue = ValueX;
			}
		}
		return FinalValue;
	}

};

UBOOL FLandscapeEditDataInterface::GetComponentsInRegion(INT X1, INT Y1, INT X2, INT Y2, TSet<ULandscapeComponent*>* OutComponents /*= NULL*/)
{
	if (ComponentSizeQuads <= 0 || !LandscapeInfo)
	{
		return FALSE;
	}
	// Find component range for this block of data
	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	UBOOL bNotLocked = TRUE;
	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{		
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));
			if( Component )
			{				
				bNotLocked = bNotLocked && ( !FLevelUtils::IsLevelLocked(Component->GetLandscapeProxy()->GetLevel()) ) && FLevelUtils::IsLevelVisible(Component->GetLandscapeProxy()->GetLevel());
				if (OutComponents)
				{
					OutComponents->Add(Component);
				}
			}
		}
	}
	return bNotLocked;
}

void FLandscapeEditDataInterface::SetHeightData(INT X1, INT Y1, INT X2, INT Y2, const WORD* Data, INT Stride, UBOOL CalcNormals, const WORD* NormalData /*= NULL*/, UBOOL CreateComponents /*= FALSE*/)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}

	check(ComponentSizeQuads > 0);
	// Find component range for this block of data
	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	FVector* VertexNormals = NULL;
	if( CalcNormals )
	{
		// Calculate the normals for each of the two triangles per quad.
		// Note that the normals at the edges are not correct because they include normals
		// from triangles outside the current area. They are not updated
		INT NumVertsX = 1+X2-X1;
		INT NumVertsY = 1+Y2-Y1;
		VertexNormals = new FVector[NumVertsX*NumVertsY];
		appMemzero(VertexNormals, NumVertsX*NumVertsY*sizeof(FVector));

		for( INT Y=0;Y<NumVertsY-1;Y++ )
		{
			for( INT X=0;X<NumVertsX-1;X++ )
			{
				FVector Vert00 = FVector(0.f,0.f,((FLOAT)Data[(X+0) + Stride*(Y+0)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;
				FVector Vert01 = FVector(0.f,1.f,((FLOAT)Data[(X+0) + Stride*(Y+1)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;
				FVector Vert10 = FVector(1.f,0.f,((FLOAT)Data[(X+1) + Stride*(Y+0)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;
				FVector Vert11 = FVector(1.f,1.f,((FLOAT)Data[(X+1) + Stride*(Y+1)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;

				FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
				FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

				// contribute to the vertex normals.
				VertexNormals[(X+1 + NumVertsX*(Y+0))] += FaceNormal1;
				VertexNormals[(X+0 + NumVertsX*(Y+1))] += FaceNormal2;
				VertexNormals[(X+0 + NumVertsX*(Y+0))] += FaceNormal1 + FaceNormal2;
				VertexNormals[(X+1 + NumVertsX*(Y+1))] += FaceNormal1 + FaceNormal2;
			}
		}
	}

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{	
			QWORD ComponentKey = ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads);
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ComponentKey);

			// if NULL, it was painted away
			if( Component==NULL )
			{
				if( CreateComponents )
				{
					// not yet implemented
					continue;
				}
				else
				{
					continue;
				}
			}

			Component->Modify();

			FLandscapeTextureDataInfo* TexDataInfo = GetTextureDataInfo(Component->HeightmapTexture);
			FColor* HeightmapTextureData = (FColor*)TexDataInfo->GetMipData(0);

			// Find the texture data corresponding to this vertex
			INT SizeU = Component->HeightmapTexture->SizeX;
			INT SizeV = Component->HeightmapTexture->SizeY;
			INT HeightmapOffsetX = Component->HeightmapScaleBias.Z * (FLOAT)SizeU;
			INT HeightmapOffsetY = Component->HeightmapScaleBias.W * (FLOAT)SizeV;

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			// To adjust bounding box
			WORD MinHeight = MAXWORD;
			WORD MaxHeight = 0;

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;
							checkSlow( LandscapeX >= X1 && LandscapeX <= X2 );
							checkSlow( LandscapeY >= Y1 && LandscapeY <= Y2 );

							// Find the input data corresponding to this vertex
							INT DataIndex = (LandscapeX-X1) + Stride * (LandscapeY-Y1);
							const WORD& Height = Data[DataIndex];

							// for bounding box
							if( Height < MinHeight )
							{
								MinHeight = Height;
							}
							if( Height > MaxHeight )
							{
								MaxHeight = Height;
							}

							INT TexX = HeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
							INT TexY = HeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
							FColor& TexData = HeightmapTextureData[ TexX + TexY * SizeU ];

							// Update the texture
							TexData.R = Height >> 8;
							TexData.G = Height & 255;

							// Update normals if we're not on an edge vertex
							if( VertexNormals && LandscapeX > X1 && LandscapeX < X2 && LandscapeY > Y1 && LandscapeY < Y2 )
							{
								FVector Normal = VertexNormals[DataIndex].SafeNormal();
								TexData.B = appRound( 127.5f * (Normal.X + 1.f) );
								TexData.A = appRound( 127.5f * (Normal.Y + 1.f) );
							}
							else if (NormalData)
							{
								// Need data validation?
								const WORD& Normal = NormalData[DataIndex];
								TexData.B = Normal >> 8;
								TexData.A = Normal & 255;
							}
						}
					}

					// Record the areas of the texture we need to re-upload
					INT TexX1 = HeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX1;
					INT TexY1 = HeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY1;
					INT TexX2 = HeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX2;
					INT TexY2 = HeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY2;
					TexDataInfo->AddMipUpdateRegion(0,TexX1,TexY1,TexX2,TexY2);
				}
			}

			// See if we need to adjust the bounds. Note we never shrink the bounding box at this point
			FLOAT MinLocalZ = LandscapeDataAccess::GetLocalHeight(MinHeight);
			FLOAT MaxLocalZ = LandscapeDataAccess::GetLocalHeight(MaxHeight);

			UBOOL bUpdateBoxSphereBounds = FALSE;
			if( MinLocalZ < Component->CachedLocalBox.Min.Z )
			{
				Component->CachedLocalBox.Min.Z = MinLocalZ;
				bUpdateBoxSphereBounds = TRUE;
			}
			if( MaxLocalZ > Component->CachedLocalBox.Max.Z )
			{
				Component->CachedLocalBox.Max.Z = MaxLocalZ;
				bUpdateBoxSphereBounds = TRUE;
			}

			if( bUpdateBoxSphereBounds )
			{
				Component->CachedBoxSphereBounds = FBoxSphereBounds(Component->CachedLocalBox.TransformBy(Component->LocalToWorld));
				Component->ConditionalUpdateTransform();
			}

			// Update mipmaps

			// Work out how many mips should be calculated directly from one component's data.
			// The remaining mips are calculated on a per texture basis.
			// eg if subsection is 7x7 quads, we need one 3 mips total: (8x8, 4x4, 2x2 verts)
			INT BaseNumMips = appCeilLogTwo(SubsectionSizeQuads+1);
			TArray<FColor*> MipData(BaseNumMips);
			MipData(0) = HeightmapTextureData;
			for( INT MipIdx=1;MipIdx<BaseNumMips;MipIdx++ )
			{
				MipData(MipIdx) = (FColor*)TexDataInfo->GetMipData(MipIdx);
			}
			Component->GenerateHeightmapMips( MipData, ComponentX1, ComponentY1, ComponentX2, ComponentY2, TexDataInfo );

			// Update collision
			Component->UpdateCollisionHeightData(MipData(Component->CollisionMipLevel), ComponentX1, ComponentY1, ComponentX2, ComponentY2, bUpdateBoxSphereBounds );
		}
	}

	if( VertexNormals )
	{
		delete[] VertexNormals;
	}
}

//
// RecalculateNormals - Regenerate normals for the entire landscape. Called after modifying DrawScale3D.
//
void FLandscapeEditDataInterface::RecalculateNormals()
{
	if (!LandscapeInfo) return;
	// Recalculate normals for each component in turn
	for( TMap<QWORD,ULandscapeComponent*>::TIterator It(LandscapeInfo->XYtoComponentMap); It; ++It )
	{
		ULandscapeComponent* Component = It.Value();

		// one extra row of vertex either side of the component
		INT X1 = Component->SectionBaseX-1;
		INT Y1 = Component->SectionBaseY-1;
		INT X2 = Component->SectionBaseX+ComponentSizeQuads+1;
		INT Y2 = Component->SectionBaseY+ComponentSizeQuads+1;
		INT Stride = ComponentSizeQuads+3; 

		WORD* HeightData = new WORD[Square(Stride)];
		FVector* VertexNormals = new FVector[Square(Stride)];
		appMemzero(VertexNormals, Square(Stride)*sizeof(FVector));

		// Get the vertex positions for entire quad
		GetHeightData(X1,Y1,X2,Y2,HeightData,0);

		// Contribute face normals for all triangles contributing to this components' normals
		for( INT Y=0;Y<Stride-1;Y++ )
		{
			for( INT X=0;X<Stride-1;X++ )
			{
				FVector Vert00 = FVector(0.f,0.f,((FLOAT)HeightData[(X+0) + Stride*(Y+0)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;
				FVector Vert01 = FVector(0.f,1.f,((FLOAT)HeightData[(X+0) + Stride*(Y+1)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;
				FVector Vert10 = FVector(1.f,0.f,((FLOAT)HeightData[(X+1) + Stride*(Y+0)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;
				FVector Vert11 = FVector(1.f,1.f,((FLOAT)HeightData[(X+1) + Stride*(Y+1)] - 32768.f)*LANDSCAPE_ZSCALE) * DrawScale;

				FVector FaceNormal1 = ((Vert00-Vert10) ^ (Vert10-Vert11)).SafeNormal();
				FVector FaceNormal2 = ((Vert11-Vert01) ^ (Vert01-Vert00)).SafeNormal(); 

				// contribute to the vertex normals.
				VertexNormals[(X+1 + Stride*(Y+0))] += FaceNormal1;
				VertexNormals[(X+0 + Stride*(Y+1))] += FaceNormal2;
				VertexNormals[(X+0 + Stride*(Y+0))] += FaceNormal1 + FaceNormal2;
				VertexNormals[(X+1 + Stride*(Y+1))] += FaceNormal1 + FaceNormal2;
			}
		}

		// Find the texture data corresponding to this vertex
		INT SizeU = Component->HeightmapTexture->SizeX;
		INT SizeV = Component->HeightmapTexture->SizeY;
		INT HeightmapOffsetX = Component->HeightmapScaleBias.Z * (FLOAT)SizeU;
		INT HeightmapOffsetY = Component->HeightmapScaleBias.W * (FLOAT)SizeV;

		FLandscapeTextureDataInfo* TexDataInfo = GetTextureDataInfo(Component->HeightmapTexture);
		FColor* HeightmapTextureData = (FColor*)TexDataInfo->GetMipData(0);

		// Apply vertex normals to the component
		for( INT SubIndexY=0;SubIndexY<Component->NumSubsections;SubIndexY++ )
		{
			for( INT SubIndexX=0;SubIndexX<Component->NumSubsections;SubIndexX++ )
			{
				for( INT SubY=0;SubY<=SubsectionSizeQuads;SubY++ )
				{
					for( INT SubX=0;SubX<=SubsectionSizeQuads;SubX++ )
					{
						INT X = (SubsectionSizeQuads+1) * SubIndexX + SubX;
						INT Y = (SubsectionSizeQuads+1) * SubIndexY + SubY;
						INT DataIndex = (X+1) + (Y+1) * Stride;

						INT TexX = HeightmapOffsetX + X;
						INT TexY = HeightmapOffsetY + Y;
						FColor& TexData = HeightmapTextureData[ TexX + TexY * SizeU ];

						// Update the texture
						FVector Normal = VertexNormals[DataIndex].SafeNormal();
						TexData.B = appRound( 127.5f * (Normal.X + 1.f) );
						TexData.A = appRound( 127.5f * (Normal.Y + 1.f) );
					}
				}
			}
		}

		delete[] HeightData;
		delete[] VertexNormals;

		// Record the areas of the texture we need to re-upload
		INT TexX1 = HeightmapOffsetX;
		INT TexY1 = HeightmapOffsetY;
		INT TexX2 = HeightmapOffsetX + (SubsectionSizeQuads+1) * Component->NumSubsections - 1;
		INT TexY2 = HeightmapOffsetY + (SubsectionSizeQuads+1) * Component->NumSubsections - 1;
		TexDataInfo->AddMipUpdateRegion(0,TexX1,TexY1,TexX2,TexY2);

		// Work out how many mips should be calculated directly from one component's data.
		// The remaining mips are calculated on a per texture basis.
		// eg if subsection is 7x7 quads, we need one 3 mips total: (8x8, 4x4, 2x2 verts)
		INT BaseNumMips = appCeilLogTwo(SubsectionSizeQuads+1);
		TArray<FColor*> MipData(BaseNumMips);
		MipData(0) = HeightmapTextureData;
		for( INT MipIdx=1;MipIdx<BaseNumMips;MipIdx++ )
		{
			MipData(MipIdx) = (FColor*)TexDataInfo->GetMipData(MipIdx);
		}
		Component->GenerateHeightmapMips( MipData, 0, 0, ComponentSizeQuads, ComponentSizeQuads, TexDataInfo );
	}
}

template<typename TStoreData>
void FLandscapeEditDataInterface::GetHeightDataTemplFast(const INT X1, const INT Y1, const INT X2, const INT Y2, TStoreData& StoreData, TStoreData* NormalData /*= NULL*/)
{
	if (!LandscapeInfo) return;
	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndices(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{		
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));

			FLandscapeTextureDataInfo* TexDataInfo = NULL;
			FColor* HeightmapTextureData = NULL;
			if( Component )
			{
				TexDataInfo = GetTextureDataInfo(Component->HeightmapTexture);
				HeightmapTextureData = (FColor*)TexDataInfo->GetMipData(0);
			}
			else // assumed that data should be initialized with default value
			{
				continue;
			}

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;

							// Find the texture data corresponding to this vertex
							INT SizeU = Component->HeightmapTexture->SizeX;
							INT SizeV = Component->HeightmapTexture->SizeY;
							INT HeightmapOffsetX = Component->HeightmapScaleBias.Z * (FLOAT)SizeU;
							INT HeightmapOffsetY = Component->HeightmapScaleBias.W * (FLOAT)SizeV;

							INT TexX = HeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
							INT TexY = HeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
							FColor& TexData = HeightmapTextureData[ TexX + TexY * SizeU ];

							WORD Height = (((WORD)TexData.R) << 8) | TexData.G;
							StoreData.Store(LandscapeX, LandscapeY, Height);
							if (NormalData)
							{
								WORD Normals = (((WORD)TexData.B) << 8) | TexData.A;
								NormalData->Store(LandscapeX, LandscapeY, Normals);
							}
						}
					}
				}
			}
		}
	}
}

template<typename TData, typename TStoreData>
void FLandscapeEditDataInterface::CalcMissingValues(const INT& X1, const INT& X2, const INT& Y1, const INT& Y2, 
								   const INT& ComponentIndexX1, const INT& ComponentIndexX2, const INT& ComponentIndexY1, const INT& ComponentIndexY2, 
								   const INT& ComponentSizeX, const INT& ComponentSizeY, TData* CornerValues, 
								   TArray<UBOOL>& NoBorderY1, TArray<UBOOL>& NoBorderY2, TArray<UBOOL>& ComponentDataExist, TStoreData& StoreData)
{
	UBOOL NoBorderX1 = FALSE, NoBorderX2 = FALSE;
	// Init data...
	appMemzero(NoBorderY1.GetData(), ComponentSizeX*sizeof(UBOOL));
	appMemzero(NoBorderY2.GetData(), ComponentSizeX*sizeof(UBOOL));
	INT BorderX1 = INT_MAX, BorderX2 = INT_MIN;
	TArray<INT> BorderY1, BorderY2;
	BorderY1.Empty(ComponentSizeX);
	BorderY2.Empty(ComponentSizeX);
	for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
	{
		new (BorderY1) INT(INT_MAX);
		new (BorderY2) INT(INT_MIN);
	}

	// fill up missing values...
	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		NoBorderX1 = FALSE;
		NoBorderX2 = FALSE;
		BorderX1 = INT_MAX;
		BorderX2 = INT_MIN;
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{
			INT ComponentIndexXY = ComponentSizeX*(ComponentIndexY-ComponentIndexY1) + ComponentIndexX-ComponentIndexX1;
			if (!ComponentDataExist(ComponentIndexXY))
			{
				INT ComponentIndexXX = ComponentIndexX - ComponentIndexX1;
				INT ComponentIndexYY = ComponentIndexY - ComponentIndexY1;

				BYTE CornerSet = 0;
				UBOOL ExistLeft = ComponentIndexXX > 0 && ComponentDataExist( ComponentIndexXX-1 + ComponentIndexYY * ComponentSizeX );
				UBOOL ExistUp = ComponentIndexYY > 0 && ComponentDataExist( ComponentIndexXX + (ComponentIndexYY-1) * ComponentSizeX );

				// Search for neighbor component for interpolation
				UBOOL bShouldSearchX = (BorderX2 <= ComponentIndexX);
				UBOOL bShouldSearchY = (BorderY2(ComponentIndexXX) <= ComponentIndexY);
				// Search for left-closest component
				if ( bShouldSearchX || (!NoBorderX1 && BorderX1 == INT_MAX) )
				{
					NoBorderX1 = TRUE;
					BorderX1 = INT_MAX;
					for (INT X = ComponentIndexX-1; X >= ComponentIndexX1; X--)
					{
						if (ComponentDataExist(ComponentSizeX*(ComponentIndexY-ComponentIndexY1) + X-ComponentIndexX1))
						{
							NoBorderX1 = FALSE;
							BorderX1 = X;
							break;
						}
					}
				}
				// Search for right-closest component
				if ( bShouldSearchX || (!NoBorderX2 && BorderX2 == INT_MIN) )
				{
					NoBorderX2 = TRUE;
					BorderX2 = INT_MIN;
					for (INT X = ComponentIndexX+1; X <= ComponentIndexX2; X++)
					{
						if (ComponentDataExist(ComponentSizeX*(ComponentIndexY-ComponentIndexY1) + X-ComponentIndexX1))
						{
							NoBorderX2 = FALSE;
							BorderX2 = X;
							break;
						}
					}
				}
				// Search for up-closest component
				if ( bShouldSearchY || (!NoBorderY1(ComponentIndexXX) && BorderY1(ComponentIndexXX) == INT_MAX))
				{
					NoBorderY1(ComponentIndexXX) = TRUE;
					BorderY1(ComponentIndexXX) = INT_MAX;
					for (INT Y = ComponentIndexY-1; Y >= ComponentIndexY1; Y--)
					{
						if (ComponentDataExist(ComponentSizeX*(Y-ComponentIndexY1) + ComponentIndexX-ComponentIndexX1))
						{
							NoBorderY1(ComponentIndexXX) = FALSE;
							BorderY1(ComponentIndexXX) = Y;
							break;
						}
					}
				}
				// Search for bottom-closest component
				if ( bShouldSearchY || (!NoBorderY2(ComponentIndexXX) && BorderY2(ComponentIndexXX) == INT_MIN))
				{
					NoBorderY2(ComponentIndexXX) = TRUE;
					BorderY2(ComponentIndexXX) = INT_MIN;
					for (INT Y = ComponentIndexY+1; Y <= ComponentIndexY2; Y++)
					{
						if (ComponentDataExist(ComponentSizeX*(Y-ComponentIndexY1) + ComponentIndexX-ComponentIndexX1))
						{
							NoBorderY2(ComponentIndexXX) = FALSE;
							BorderY2(ComponentIndexXX) = Y;
							break;
						}
					}
				}

				if (((ComponentIndexX == ComponentIndexX1) || (ComponentIndexY == ComponentIndexY1)) ? FALSE : ComponentDataExist(ComponentSizeX*(ComponentIndexY-1-ComponentIndexY1) + ComponentIndexX-1-ComponentIndexX1))
				{
					CornerSet |= 1;
					CornerValues[0] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, ComponentIndexY*ComponentSizeQuads);
				}
				if (((ComponentIndexX == ComponentIndexX2) || (ComponentIndexY == ComponentIndexY1)) ? FALSE : ComponentDataExist(ComponentSizeX*(ComponentIndexY-1-ComponentIndexY1) + ComponentIndexX+1-ComponentIndexX1))
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = StoreData.Load( (ComponentIndexX+1)*ComponentSizeQuads, ComponentIndexY*ComponentSizeQuads);
				}
				if (((ComponentIndexX == ComponentIndexX1) || (ComponentIndexY == ComponentIndexY2)) ? FALSE : ComponentDataExist(ComponentSizeX*(ComponentIndexY+1-ComponentIndexY1) + ComponentIndexX-1-ComponentIndexX1))
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, (ComponentIndexY+1)*ComponentSizeQuads);
				}
				if (((ComponentIndexX == ComponentIndexX2) || (ComponentIndexY == ComponentIndexY2)) ? FALSE : ComponentDataExist(ComponentSizeX*(ComponentIndexY+1-ComponentIndexY1) + ComponentIndexX+1-ComponentIndexX1))
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = StoreData.Load((ComponentIndexX+1)*ComponentSizeQuads, (ComponentIndexY+1)*ComponentSizeQuads);
				}

				FillCornerValues(CornerSet, CornerValues);

				// Find coordinates of box that lies inside component
				INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
				INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
				INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
				INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

				// Find subsection range for this box
				INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
				INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
				INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
				INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

				for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
				{
					for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
					{
						// Find coordinates of box that lies inside subsection
						INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
						INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
						INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
						INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

						// Update texture data for the box that lies inside subsection
						for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
						{
							for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
							{
								INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
								INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;

								// Find the texture data corresponding to this vertex
								TData Value[4] = {0, 0, 0, 0};
								INT Dist[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
								FLOAT ValueX = 0.f, ValueY = 0.f;
								UBOOL Exist[4] = {FALSE, FALSE, FALSE, FALSE};

								if (ExistLeft)
								{
									Value[0] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, LandscapeY);
									Dist[0] = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
									Exist[0] = TRUE;
								}
								else if ( BorderX1 != INT_MAX )
								{
									INT BorderIdxX = (BorderX1+1)*ComponentSizeQuads;
									Value[0] = StoreData.Load(BorderIdxX, LandscapeY);
									Dist[0] = LandscapeX - (BorderIdxX-1);
									Exist[0] = TRUE;
								}
								else 
								{
									if ((CornerSet & 1) && (CornerSet & 1 << 2))
									{
										INT Dist1 = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										INT Dist2 = ((ComponentIndexY+1)*ComponentSizeQuads) - LandscapeY;
										Value[0] = (FLOAT)(Dist2 * CornerValues[0] + Dist1 * CornerValues[2]) / (Dist1 + Dist2);
										Dist[0] = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										Exist[0] = TRUE;
									}
								}

								if ( BorderX2 != INT_MIN )
								{
									INT BorderIdxX = BorderX2*ComponentSizeQuads;
									Value[1] = StoreData.Load(BorderIdxX, LandscapeY);
									Dist[1] = (BorderIdxX+1) - LandscapeX;
									Exist[1] = TRUE;
								}
								else 
								{
									if ((CornerSet & 1 << 1) && (CornerSet & 1 << 3))
									{
										INT Dist1 = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										INT Dist2 = ((ComponentIndexY+1)*ComponentSizeQuads) - LandscapeY;
										Value[1] = (FLOAT)(Dist2 * CornerValues[1] + Dist1 * CornerValues[3]) / (Dist1 + Dist2);
										Dist[1] = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
										Exist[1] = TRUE;
									}
								}

								if (ExistUp)
								{
									Value[2] = StoreData.Load( LandscapeX, ComponentIndexY*ComponentSizeQuads);
									Dist[2] = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
									Exist[2] = TRUE;
								}
								else if ( BorderY1(ComponentIndexXX) != INT_MAX )
								{
									INT BorderIdxY = (BorderY1(ComponentIndexXX)+1)*ComponentSizeQuads;
									Value[2] = StoreData.Load(LandscapeX, BorderIdxY);
									Dist[2] = LandscapeY - BorderIdxY;
									Exist[2] = TRUE;
								}
								else 
								{
									if ((CornerSet & 1) && (CornerSet & 1 << 1))
									{
										INT Dist1 = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										INT Dist2 = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
										Value[2] = (FLOAT)(Dist2 * CornerValues[0] + Dist1 * CornerValues[1]) / (Dist1 + Dist2);
										Dist[2] = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										Exist[2] = TRUE;
									}
								}

								if ( BorderY2(ComponentIndexXX) != INT_MIN )
								{
									INT BorderIdxY = BorderY2(ComponentIndexXX)*ComponentSizeQuads;
									Value[3] = StoreData.Load(LandscapeX, BorderIdxY);
									Dist[3] = BorderIdxY - LandscapeY;
									Exist[3] = TRUE;
								}
								else 
								{
									if ((CornerSet & 1 << 2) && (CornerSet & 1 << 3))
									{
										INT Dist1 = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										INT Dist2 = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
										Value[3] = (FLOAT)(Dist2 * CornerValues[2] + Dist1 * CornerValues[3]) / (Dist1 + Dist2);
										Dist[3] = (ComponentIndexY+1)*ComponentSizeQuads - LandscapeY;
										Exist[3] = TRUE;
									}
								}

								CalcInterpValue<TData>(Dist, Exist, Value, ValueX, ValueY);

								WORD FinalValue = 0; // Default Value
								if ( (Exist[0] || Exist[1]) && (Exist[2] || Exist[3]) )
								{
									FinalValue = CalcValueFromValueXY<TData>(Dist, ValueX, ValueY, CornerSet, CornerValues);
								}
								else if ( (Exist[0] || Exist[1]) )
								{
									FinalValue = ValueX;
								}
								else if ( (Exist[2] || Exist[3]) )
								{
									FinalValue = ValueY;
								}

								StoreData.Store(LandscapeX, LandscapeY, FinalValue);
							}
						}
					}
				}
			}
		}
	}
}

WORD FLandscapeEditDataInterface::GetHeightMapData(const ULandscapeComponent* Component, INT TexU, INT TexV, FColor* TextureData /*= NULL*/)
{
	check(Component);
	if (!TextureData)
	{
		FLandscapeTextureDataInfo* TexDataInfo = GetTextureDataInfo(Component->HeightmapTexture);
		TextureData = (FColor*)TexDataInfo->GetMipData(0);	
	}

	INT SizeU = Component->HeightmapTexture->SizeX;
	INT SizeV = Component->HeightmapTexture->SizeY;
	INT HeightmapOffsetX = Component->HeightmapScaleBias.Z * (FLOAT)SizeU;
	INT HeightmapOffsetY = Component->HeightmapScaleBias.W * (FLOAT)SizeV;

	INT TexX = HeightmapOffsetX + TexU;
	INT TexY = HeightmapOffsetY + TexV;
	FColor& TexData = TextureData[ TexX + TexY * SizeU ];

	return ((((WORD)TexData.R) << 8) | TexData.G);
}

template<typename T>
void FillCornerValues(BYTE& CornerSet, T* CornerValues)
{
	BYTE OriginalSet = CornerSet;

	if (CornerSet)
	{
		// Fill unset values
		while (CornerSet != 15)
		{
			if (CornerSet != 15 && (OriginalSet & 1))
			{
				if (!(CornerSet & 1 << 1))
				{
					CornerValues[1] = CornerValues[0];
					CornerSet |= 1 << 1;
				}
				if (!(CornerSet & 1 << 2))
				{
					CornerValues[2] = CornerValues[0];
					CornerSet |= 1 << 2;
				}
			}
			if (CornerSet != 15 && (OriginalSet & 1 << 1))
			{
				if (!(CornerSet & 1))
				{
					CornerValues[0] = CornerValues[1];
					CornerSet |= 1;
				}
				if (!(CornerSet & 1 << 3))
				{
					CornerValues[3] = CornerValues[1];
					CornerSet |= 1 << 3;
				}
			}
			if (CornerSet != 15 && (OriginalSet & 1 << 2))
			{
				if (!(CornerSet & 1))
				{
					CornerValues[0] = CornerValues[2];
					CornerSet |= 1;
				}
				if (!(CornerSet & 1 << 3))
				{
					CornerValues[3] = CornerValues[2];
					CornerSet |= 1 << 3;
				}
			}
			if (CornerSet != 15 && (OriginalSet & 1 << 3))
			{
				if (!(CornerSet & 1 << 1))
				{
					CornerValues[1] = CornerValues[3];
					CornerSet |= 1 << 1;
				}
				if (!(CornerSet & 1 << 2))
				{
					CornerValues[2] = CornerValues[3];
					CornerSet |= 1 << 2;
				}
			}

			OriginalSet = CornerSet;
		}
	}
}

template<typename TStoreData>
void FLandscapeEditDataInterface::GetHeightDataTempl(INT& ValidX1, INT& ValidY1, INT& ValidX2, INT& ValidY2, TStoreData& StoreData)
{
	// Copy variables
	INT X1 = ValidX1, X2 = ValidX2, Y1 = ValidY1, Y2 = ValidY2;
	ValidX1 = INT_MAX; ValidX2 = INT_MIN; ValidY1 = INT_MAX; ValidY2 = INT_MIN;

	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	INT ComponentSizeX = ComponentIndexX2-ComponentIndexX1+1;
	INT ComponentSizeY = ComponentIndexY2-ComponentIndexY1+1;

	// Neighbor Components
	ULandscapeComponent* BorderComponent[4] = {0, 0, 0, 0};
	ULandscapeComponent* CornerComponent[4] = {0, 0, 0, 0};
	UBOOL NoBorderX1 = FALSE, NoBorderX2 = FALSE;
	TArray<UBOOL> NoBorderY1, NoBorderY2, ComponentDataExist;
	TArray<ULandscapeComponent*> BorderComponentY1, BorderComponentY2;
	ComponentDataExist.Empty(ComponentSizeX*ComponentSizeY);
	ComponentDataExist.AddZeroed(ComponentSizeX*ComponentSizeY);
	UBOOL bHasMissingValue = FALSE;

	FLandscapeTextureDataInfo* NeighborTexDataInfo[4] = {0, 0, 0, 0};
	FColor* NeighborHeightmapTextureData[4] = {0, 0, 0, 0};
	WORD CornerValues[4] = {0, 0, 0, 0};

	INT EdgeCoord = (SubsectionSizeQuads+1) * ComponentNumSubsections - 1; //ComponentSizeQuads;

	// initial loop....
	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		NoBorderX1 = FALSE;
		NoBorderX2 = FALSE;
		BorderComponent[0] = BorderComponent[1] = NULL;
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{
			BorderComponent[2] = BorderComponent[3] = NULL;
			INT ComponentIndexXY = ComponentSizeX*(ComponentIndexY-ComponentIndexY1) + ComponentIndexX-ComponentIndexX1;
			INT ComponentIndexXX = ComponentIndexX - ComponentIndexX1;
			INT ComponentIndexYY = ComponentIndexY - ComponentIndexY1;
			ComponentDataExist(ComponentIndexXY) = FALSE;
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));

			FLandscapeTextureDataInfo* TexDataInfo = NULL;
			FColor* HeightmapTextureData = NULL;
			BYTE CornerSet = 0;
			UBOOL ExistLeft = ComponentIndexXX > 0 && ComponentDataExist( ComponentIndexXX-1 + ComponentIndexYY * ComponentSizeX );
			UBOOL ExistUp = ComponentIndexYY > 0 && ComponentDataExist( ComponentIndexXX + (ComponentIndexYY-1) * ComponentSizeX );

			if( Component )
			{
				TexDataInfo = GetTextureDataInfo(Component->HeightmapTexture);
				HeightmapTextureData = (FColor*)TexDataInfo->GetMipData(0);
				ComponentDataExist(ComponentIndexXY) = TRUE;
				// Update valid region
				ValidX1 = Min<INT>(Component->SectionBaseX, ValidX1);
				ValidX2 = Max<INT>(Component->SectionBaseX+ComponentSizeQuads, ValidX2);
				ValidY1 = Min<INT>(Component->SectionBaseY, ValidY1);
				ValidY2 = Max<INT>(Component->SectionBaseY+ComponentSizeQuads, ValidY2);
			}
			else
			{
				if (!bHasMissingValue)
				{
					NoBorderY1.Empty(ComponentSizeX);
					NoBorderY2.Empty(ComponentSizeX);
					NoBorderY1.AddZeroed(ComponentSizeX);
					NoBorderY2.AddZeroed(ComponentSizeX);
					BorderComponentY1.Empty(ComponentSizeX);
					BorderComponentY2.Empty(ComponentSizeX);
					BorderComponentY1.AddZeroed(ComponentSizeX);
					BorderComponentY2.AddZeroed(ComponentSizeX);
					bHasMissingValue = TRUE;
				}

				// Search for neighbor component for interpolation
				UBOOL bShouldSearchX = (BorderComponent[1] && BorderComponent[1]->SectionBaseX / ComponentSizeQuads <= ComponentIndexX);
				UBOOL bShouldSearchY = (BorderComponentY2(ComponentIndexXX) && BorderComponentY2(ComponentIndexXX)->SectionBaseY / ComponentSizeQuads <= ComponentIndexY);
				// Search for left-closest component
				if ( bShouldSearchX || (!NoBorderX1 && !BorderComponent[0]))
				{
					NoBorderX1 = TRUE;
					for (INT X = ComponentIndexX-1; X >= ComponentIndexX1; X--)
					{
						BorderComponent[0] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(X*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));
						if (BorderComponent[0])
						{
							NoBorderX1 = FALSE;
							NeighborTexDataInfo[0] = GetTextureDataInfo(BorderComponent[0]->HeightmapTexture);
							NeighborHeightmapTextureData[0] = (FColor*)NeighborTexDataInfo[0]->GetMipData(0);
							break;
						}
					}
				}
				// Search for right-closest component
				if ( bShouldSearchX || (!NoBorderX2 && !BorderComponent[1]))
				{
					NoBorderX2 = TRUE;
					for (INT X = ComponentIndexX+1; X <= ComponentIndexX2; X++)
					{
						BorderComponent[1] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(X*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));
						if (BorderComponent[1])
						{
							NoBorderX2 = FALSE;
							NeighborTexDataInfo[1] = GetTextureDataInfo(BorderComponent[1]->HeightmapTexture);
							NeighborHeightmapTextureData[1] = (FColor*)NeighborTexDataInfo[1]->GetMipData(0);
							break;
						}
					}
				}
				// Search for up-closest component
				if ( bShouldSearchY || (!NoBorderY1(ComponentIndexXX) && !BorderComponentY1(ComponentIndexXX)))
				{
					NoBorderY1(ComponentIndexXX) = TRUE;
					for (INT Y = ComponentIndexY-1; Y >= ComponentIndexY1; Y--)
					{
						BorderComponentY1(ComponentIndexXX) = BorderComponent[2] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,Y*ComponentSizeQuads));
						if (BorderComponent[2])
						{
							NoBorderY1(ComponentIndexXX) = FALSE;
							NeighborTexDataInfo[2] = GetTextureDataInfo(BorderComponent[2]->HeightmapTexture);
							NeighborHeightmapTextureData[2] = (FColor*)NeighborTexDataInfo[2]->GetMipData(0);
							break;
						}
					}
				}
				else
				{
					BorderComponent[2] = BorderComponentY1(ComponentIndexXX);
					if (BorderComponent[2])
					{
						NeighborTexDataInfo[2] = GetTextureDataInfo(BorderComponent[2]->HeightmapTexture);
						NeighborHeightmapTextureData[2] = (FColor*)NeighborTexDataInfo[2]->GetMipData(0);
					}
				}
				// Search for bottom-closest component
				if ( bShouldSearchY || (!NoBorderY2(ComponentIndexXX) && !BorderComponentY2(ComponentIndexXX)))
				{
					NoBorderY2(ComponentIndexXX) = TRUE;
					for (INT Y = ComponentIndexY+1; Y <= ComponentIndexY2; Y++)
					{
						BorderComponentY2(ComponentIndexXX) = BorderComponent[3] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,Y*ComponentSizeQuads));
						if (BorderComponent[3])
						{
							NoBorderY2(ComponentIndexXX) = FALSE;
							NeighborTexDataInfo[3] = GetTextureDataInfo(BorderComponent[3]->HeightmapTexture);
							NeighborHeightmapTextureData[3] = (FColor*)NeighborTexDataInfo[3]->GetMipData(0);
							break;
						}
					}
				}
				else
				{
					BorderComponent[3] = BorderComponentY2(ComponentIndexXX);
					if (BorderComponent[3])
					{
						NeighborTexDataInfo[3] = GetTextureDataInfo(BorderComponent[3]->HeightmapTexture);
						NeighborHeightmapTextureData[3] = (FColor*)NeighborTexDataInfo[3]->GetMipData(0);
					}
				}

				CornerComponent[0] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX-1)*ComponentSizeQuads,(ComponentIndexY-1)*ComponentSizeQuads));
				CornerComponent[1] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX+1)*ComponentSizeQuads,(ComponentIndexY-1)*ComponentSizeQuads));
				CornerComponent[2] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX-1)*ComponentSizeQuads,(ComponentIndexY+1)*ComponentSizeQuads));
				CornerComponent[3] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX+1)*ComponentSizeQuads,(ComponentIndexY+1)*ComponentSizeQuads));

				if (CornerComponent[0])
				{
					CornerSet |= 1;
					CornerValues[0] = GetHeightMapData(CornerComponent[0], EdgeCoord, EdgeCoord);
				}
				else if ((ExistLeft || ExistUp) && X1 <= ComponentIndexX*ComponentSizeQuads && Y1 <= ComponentIndexY*ComponentSizeQuads  )
				{
					CornerSet |= 1;
					CornerValues[0] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, ComponentIndexY*ComponentSizeQuads);
				}
				else if (BorderComponent[0])
				{
					CornerSet |= 1;
					CornerValues[0] = GetHeightMapData(BorderComponent[0], EdgeCoord, 0, NeighborHeightmapTextureData[0]);
				}
				else if (BorderComponent[2])
				{
					CornerSet |= 1;
					CornerValues[0] = GetHeightMapData(BorderComponent[2], 0, EdgeCoord, NeighborHeightmapTextureData[2]);
				}

				if (CornerComponent[1])
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = GetHeightMapData(CornerComponent[1], 0, EdgeCoord);
				}
				else if (ExistUp && X2 >= (ComponentIndexX+1)*ComponentSizeQuads)
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = StoreData.Load( (ComponentIndexX+1)*ComponentSizeQuads, ComponentIndexY*ComponentSizeQuads);
				}
				else if (BorderComponent[1])
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = GetHeightMapData(BorderComponent[1], 0, 0, NeighborHeightmapTextureData[1]);
				}
				else if (BorderComponent[2])
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = GetHeightMapData(BorderComponent[2], EdgeCoord, EdgeCoord, NeighborHeightmapTextureData[2]);
				}

				if (CornerComponent[2])
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = GetHeightMapData(CornerComponent[2], EdgeCoord, 0);
				}
				else if (ExistLeft && Y2 >= (ComponentIndexY+1)*ComponentSizeQuads) // Use data already stored for 0, 2
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, (ComponentIndexY+1)*ComponentSizeQuads);
				}
				else if (BorderComponent[0])
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = GetHeightMapData(BorderComponent[0], EdgeCoord, EdgeCoord, NeighborHeightmapTextureData[0]);
				}
				else if (BorderComponent[3])
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = GetHeightMapData(BorderComponent[3], 0, 0, NeighborHeightmapTextureData[3]);
				}

				if (CornerComponent[3])
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = GetHeightMapData(CornerComponent[3], 0, 0);
				}
				else if (BorderComponent[1])
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = GetHeightMapData(BorderComponent[1], 0, EdgeCoord, NeighborHeightmapTextureData[1]);
				}
				else if (BorderComponent[3])
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = GetHeightMapData(BorderComponent[3], EdgeCoord, 0, NeighborHeightmapTextureData[3]);
				}

				FillCornerValues(CornerSet, CornerValues);
				ComponentDataExist(ComponentIndexXY) = ExistLeft || ExistUp || (BorderComponent[0] || BorderComponent[1] || BorderComponent[2] || BorderComponent[3]) || CornerSet;
			}

			if (!ComponentDataExist(ComponentIndexXY))
			{
				continue;
			}

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;

							// Find the input data corresponding to this vertex
							if( Component )
							{
								// Find the texture data corresponding to this vertex
								WORD Height = GetHeightMapData(Component, (SubsectionSizeQuads+1) * SubIndexX + SubX, (SubsectionSizeQuads+1) * SubIndexY + SubY, HeightmapTextureData);
								StoreData.Store(LandscapeX, LandscapeY, Height);
							}
							else
							{
								// Find the texture data corresponding to this vertex
								WORD Value[4] = {0, 0, 0, 0};
								INT Dist[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
								FLOAT ValueX = 0.f, ValueY = 0.f;
								UBOOL Exist[4] = {FALSE, FALSE, FALSE, FALSE};

								// Use data already stored for 0, 2
								if (ExistLeft)
								{
									Value[0] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, LandscapeY);
									Dist[0] = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
									Exist[0] = TRUE;
								}
								else if (BorderComponent[0])
								{
									Value[0] = GetHeightMapData(BorderComponent[0], EdgeCoord, (SubsectionSizeQuads+1) * SubIndexY + SubY, NeighborHeightmapTextureData[0]);
									Dist[0] = LandscapeX - (BorderComponent[0]->SectionBaseX + ComponentSizeQuads);
									Exist[0] = TRUE;
								}
								else 
								{
									if ((CornerSet & 1) && (CornerSet & 1 << 2))
									{
										INT Dist1 = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										INT Dist2 = ((ComponentIndexY+1)*ComponentSizeQuads) - LandscapeY;
										Value[0] = (FLOAT)(Dist2 * CornerValues[0] + Dist1 * CornerValues[2]) / (Dist1 + Dist2);
										Dist[0] = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										Exist[0] = TRUE;
									}
								}

								if (BorderComponent[1])
								{
									Value[1] = GetHeightMapData(BorderComponent[1], 0, (SubsectionSizeQuads+1) * SubIndexY + SubY, NeighborHeightmapTextureData[1]);
									Dist[1] = (BorderComponent[1]->SectionBaseX) - LandscapeX;
									Exist[1] = TRUE;
								}
								else
								{
									if ((CornerSet & 1 << 1) && (CornerSet & 1 << 3))
									{
										INT Dist1 = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										INT Dist2 = ((ComponentIndexY+1)*ComponentSizeQuads) - LandscapeY;
										Value[1] = (FLOAT)(Dist2 * CornerValues[1] + Dist1 * CornerValues[3]) / (Dist1 + Dist2);
										Dist[1] = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
										Exist[1] = TRUE;
									}
								}

								if (ExistUp)
								{
									Value[2] = StoreData.Load( LandscapeX, ComponentIndexY*ComponentSizeQuads);
									Dist[2] = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
									Exist[2] = TRUE;
								}
								else if (BorderComponent[2])
								{
									Value[2] = GetHeightMapData(BorderComponent[2], (SubsectionSizeQuads+1) * SubIndexX + SubX, EdgeCoord, NeighborHeightmapTextureData[2]);
									Dist[2] = LandscapeY - (BorderComponent[2]->SectionBaseY + ComponentSizeQuads);
									Exist[2] = TRUE;
								}
								else
								{
									if ((CornerSet & 1) && (CornerSet & 1 << 1))
									{
										INT Dist1 = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										INT Dist2 = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
										Value[2] = (FLOAT)(Dist2 * CornerValues[0] + Dist1 * CornerValues[1]) / (Dist1 + Dist2);
										Dist[2] = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										Exist[2] = TRUE;
									}
								}

								if (BorderComponent[3])
								{
									Value[3] = GetHeightMapData(BorderComponent[3], (SubsectionSizeQuads+1) * SubIndexX + SubX, 0, NeighborHeightmapTextureData[3]);
									Dist[3] = (BorderComponent[3]->SectionBaseY) - LandscapeY;
									Exist[3] = TRUE;
								}
								else
								{
									if ((CornerSet & 1 << 2) && (CornerSet & 1 << 3))
									{
										INT Dist1 = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										INT Dist2 = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
										Value[3] = (FLOAT)(Dist2 * CornerValues[2] + Dist1 * CornerValues[3]) / (Dist1 + Dist2);
										Dist[3] = (ComponentIndexY+1)*ComponentSizeQuads - LandscapeY;
										Exist[3] = TRUE;
									}
								}

								CalcInterpValue<WORD>(Dist, Exist, Value, ValueX, ValueY);

								WORD FinalValue = 0; // Default Value
								if ( (Exist[0] || Exist[1]) && (Exist[2] || Exist[3]) )
								{
									FinalValue = CalcValueFromValueXY<WORD>(Dist, ValueX, ValueY, CornerSet, CornerValues);
								}
								else if ( (BorderComponent[0] || BorderComponent[1]) )
								{
									FinalValue = ValueX;
								}
								else if ( (BorderComponent[2] || BorderComponent[3]) )
								{
									FinalValue = ValueY;
								}
								else if ( (Exist[0] || Exist[1]) )
								{
									FinalValue = ValueX;
								}
								else if ( (Exist[2] || Exist[3]) )
								{
									FinalValue = ValueY;
								}

								StoreData.Store(LandscapeX, LandscapeY, FinalValue);
								//StoreData.StoreDefault(LandscapeX, LandscapeY);
							}
						}
					}
				}
			}
		}
	}

	if (bHasMissingValue)
	{
		CalcMissingValues<WORD, TStoreData>( X1, X2, Y1, Y2,
			ComponentIndexX1, ComponentIndexX2, ComponentIndexY1, ComponentIndexY2, 
			ComponentSizeX, ComponentSizeY, CornerValues,
			NoBorderY1, NoBorderY2, ComponentDataExist, StoreData );
		// Update valid region
		ValidX1 = Max<INT>(X1, ValidX1);
		ValidX2 = Min<INT>(X2, ValidX2);
		ValidY1 = Max<INT>(Y1, ValidY1);
		ValidY2 = Min<INT>(Y2, ValidY2);
	}
	else
	{
		ValidX1 = X1;
		ValidX2 = X2;
		ValidY1 = Y1;
		ValidY2 = Y2;
	}
}

namespace
{
	struct FArrayStoreData
	{
		INT X1;
		INT Y1;
		WORD* Data;
		INT Stride;

		FArrayStoreData(INT InX1, INT InY1, WORD* InData, INT InStride)
			:	X1(InX1)
			,	Y1(InY1)
			,	Data(InData)
			,	Stride(InStride)
		{}

		inline void Store(INT LandscapeX, INT LandscapeY, WORD Height)
		{
			Data[ (LandscapeY-Y1) * Stride + (LandscapeX-X1) ] = Height;
		}

		// for interpolation
		inline WORD Load(INT LandscapeX, INT LandscapeY)
		{
			return Data[ (LandscapeY-Y1) * Stride + (LandscapeX-X1) ];
		}

		inline void StoreDefault(INT LandscapeX, INT LandscapeY)
		{
			Data[ (LandscapeY-Y1) * Stride + (LandscapeX-X1) ] = 0;
		}
	};

	struct FSparseStoreData
	{
		TMap<QWORD, WORD>& SparseData;

		FSparseStoreData(TMap<QWORD, WORD>& InSparseData)
			:	SparseData(InSparseData)
		{}

		inline void Store(INT LandscapeX, INT LandscapeY, WORD Height)
		{
			SparseData.Set(ALandscape::MakeKey(LandscapeX,LandscapeY), Height);
		}

		inline WORD Load(INT LandscapeX, INT LandscapeY)
		{
			return SparseData.FindRef(ALandscape::MakeKey(LandscapeX,LandscapeY));
		}

		inline void StoreDefault(INT LandscapeX, INT LandscapeY)
		{
		}
	};

};

void FLandscapeEditDataInterface::GetHeightData(INT& X1, INT& Y1, INT& X2, INT& Y2, WORD* Data, INT Stride)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}

	FArrayStoreData ArrayStoreData(X1, Y1, Data, Stride);
	GetHeightDataTempl(X1, Y1, X2, Y2, ArrayStoreData);
}

void FLandscapeEditDataInterface::GetHeightDataFast(const INT X1, const INT Y1, const INT X2, const INT Y2, WORD* Data, INT Stride, WORD* NormalData /*= NULL*/)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}

	FArrayStoreData ArrayStoreData(X1, Y1, Data, Stride);
	if (NormalData)
	{
		FArrayStoreData ArrayNormalData(X1, Y1, NormalData, Stride);
		GetHeightDataTemplFast(X1, Y1, X2, Y2, ArrayStoreData, &ArrayNormalData);
	}
	else
	{
		GetHeightDataTemplFast(X1, Y1, X2, Y2, ArrayStoreData);
	}
}

void FLandscapeEditDataInterface::GetHeightData(INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, WORD>& SparseData)
{
	FSparseStoreData SparseStoreData(SparseData);
	GetHeightDataTempl(X1, Y1, X2, Y2, SparseStoreData);
}

void FLandscapeEditDataInterface::GetHeightDataFast(const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, WORD>& SparseData, TMap<QWORD, WORD>* NormalData /*= NULL*/)
{
	FSparseStoreData SparseStoreData(SparseData);
	if (NormalData)
	{
		FSparseStoreData SparseNormalData(*NormalData);
		GetHeightDataTemplFast(X1, Y1, X2, Y2, SparseStoreData, &SparseNormalData);
	}
	else
	{
		GetHeightDataTemplFast(X1, Y1, X2, Y2, SparseStoreData);
	}
}

void ULandscapeComponent::DeleteLayer(FName LayerName, FLandscapeEditDataInterface* LandscapeEdit /*= NULL*/)
{
	ULandscapeComponent* Component = this; //Landscape->LandscapeComponents(ComponentIdx);

	// Find the index for this layer in this component.
	INT DeleteLayerIdx = INDEX_NONE;

	for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		FWeightmapLayerAllocationInfo& Allocation = Component->WeightmapLayerAllocations(LayerIdx);
		if( Allocation.LayerName == LayerName )
		{
			DeleteLayerIdx = LayerIdx;
		}
	}
	if( DeleteLayerIdx == INDEX_NONE )
	{
		// Layer not used for this component.
		//continue;
		return;
	}

	ULandscapeInfo* Info = GetLandscapeInfo();
	if (!Info)
	{
		return;
	}

	FWeightmapLayerAllocationInfo& DeleteLayerAllocation = Component->WeightmapLayerAllocations(DeleteLayerIdx);
	INT DeleteLayerWeightmapTextureIndex = DeleteLayerAllocation.WeightmapTextureIndex;

	// See if we'll be able to remove the texture completely.
	UBOOL bCanRemoveLayerTexture = TRUE;
	for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
	{
		FWeightmapLayerAllocationInfo& Allocation = Component->WeightmapLayerAllocations(LayerIdx);

		// check if we will be able to remove the texture also
		if( LayerIdx!=DeleteLayerIdx && Allocation.WeightmapTextureIndex == DeleteLayerWeightmapTextureIndex )
		{
			bCanRemoveLayerTexture = FALSE;
		}
	}

	// See if the deleted layer is a NoWeightBlend layer - if not, we don't have to worry about normalization
	FLandscapeLayerStruct* DeleteLayerInfo = Info->LayerInfoMap.FindRef(LayerName);
	UBOOL bDeleteLayerIsNoWeightBlend = (DeleteLayerInfo && DeleteLayerInfo->LayerInfoObj && DeleteLayerInfo->LayerInfoObj->bNoWeightBlend);

	if( !bDeleteLayerIsNoWeightBlend )
	{
		// Lock data for all the weightmaps
		TArray<FLandscapeTextureDataInfo*> TexDataInfos;
		for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
		{
			if (LandscapeEdit)
			{
				TexDataInfos.AddItem(LandscapeEdit->GetTextureDataInfo(Component->WeightmapTextures(WeightmapIdx)));
			}
		}

		// Channel remapping
		INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

		TArray<UBOOL> LayerNoWeightBlends;	// Array of NoWeightBlend flags
		TArray<BYTE*> LayerDataPtrs;		// Pointers to all layers' data 

		// Get the data for each layer
		for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
		{
			FWeightmapLayerAllocationInfo& Allocation = Component->WeightmapLayerAllocations(LayerIdx);
			LayerDataPtrs.AddItem( (BYTE*)TexDataInfos(Allocation.WeightmapTextureIndex)->GetMipData(0) + ChannelOffsets[Allocation.WeightmapTextureChannel] );

			// Find the layer info and record if it is a bNoWeightBlend layer.
			FLandscapeLayerStruct* LayerInfo = Info->LayerInfoMap.FindRef(Allocation.LayerName);
			LayerNoWeightBlends.AddItem( (LayerInfo && LayerInfo->LayerInfoObj && LayerInfo->LayerInfoObj->bNoWeightBlend) );
		}

		// Find the texture data corresponding to this vertex
		INT SizeU = (SubsectionSizeQuads+1) * NumSubsections; //Component->WeightmapTextures(0)->SizeX;		// not exactly correct.  
		INT SizeV = (SubsectionSizeQuads+1) * NumSubsections; //Component->WeightmapTextures(0)->SizeY;
		INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
		INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

		for( INT SubIndexY=0;SubIndexY<NumSubsections;SubIndexY++ )
		{
			for( INT SubIndexX=0;SubIndexX<NumSubsections;SubIndexX++ )
			{
				for( INT SubY=0;SubY<=SubsectionSizeQuads;SubY++ )
				{
					for( INT SubX=0;SubX<=SubsectionSizeQuads;SubX++ )
					{
						INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
						INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
						INT TexDataIndex = 4 * (TexX + TexY * SizeU);

						// Calculate the sum of other layer weights
						INT OtherLayerWeightSum = 0;
						for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
						{
							if( LayerIdx != DeleteLayerIdx && LayerNoWeightBlends(LayerIdx)==FALSE )
							{
								OtherLayerWeightSum += LayerDataPtrs(LayerIdx)[TexDataIndex];
							}
						}

						if( OtherLayerWeightSum == 0 )
						{
							OtherLayerWeightSum = 255;
						}

						// Adjust other layer weights
						for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
						{
							if( LayerIdx != DeleteLayerIdx && LayerNoWeightBlends(LayerIdx)==FALSE )
							{
								BYTE& Weight = LayerDataPtrs(LayerIdx)[TexDataIndex];
								Weight = Clamp<INT>( appRound(255.f * (FLOAT)Weight/(FLOAT)OtherLayerWeightSum), 0, 255 );
							}
						}						
					}
				}
			}
		}

		// Update all the textures and mips
		for( INT Idx=0;Idx<Component->WeightmapTextures.Num();Idx++)
		{
			if( bCanRemoveLayerTexture && Idx==DeleteLayerWeightmapTextureIndex )
			{
				// We're going to remove this texture anyway, so don't bother updating
				continue;
			}

			UTexture2D* WeightmapTexture = Component->WeightmapTextures(Idx);
			FLandscapeTextureDataInfo* WeightmapDataInfo = TexDataInfos(Idx);

			INT NumMips = WeightmapTexture->Mips.Num();
			TArray<FColor*> WeightmapTextureMipData(NumMips);
			for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
			{
				WeightmapTextureMipData(MipIdx) = (FColor*)WeightmapDataInfo->GetMipData(MipIdx);
			}

			ULandscapeComponent::UpdateWeightmapMips(Component->NumSubsections, Component->SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, 0, 0, MAXINT, MAXINT, WeightmapDataInfo);

			WeightmapDataInfo->AddMipUpdateRegion(0,0,0,WeightmapTexture->SizeX-1,WeightmapTexture->SizeY-1);
		}
	}

	ALandscapeProxy* Proxy = GetLandscapeProxy(); //CastChecked<ALandscapeProxy>(Component->GetOuter());
	// Mark the channel as unallocated, so we can reuse it later
	FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(Component->WeightmapTextures(DeleteLayerAllocation.WeightmapTextureIndex));
	//check(Usage);
	if (Usage)
	{
		Usage->ChannelUsage[DeleteLayerAllocation.WeightmapTextureChannel] = NULL;
	}

	// Remove the layer
	Component->WeightmapLayerAllocations.Remove(DeleteLayerIdx);

	// If this layer was the last usage for this channel in this layer, we can remove it.
	if( bCanRemoveLayerTexture )
	{
		Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex)->SetFlags(RF_Transactional);
		Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex)->Modify();
		Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex)->MarkPackageDirty();
		Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex)->ClearFlags(RF_Standalone);

		Component->WeightmapTextures.Remove(DeleteLayerWeightmapTextureIndex);

		// Adjust WeightmapTextureIndex index for other layers
		for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
		{
			FWeightmapLayerAllocationInfo& Allocation = Component->WeightmapLayerAllocations(LayerIdx);

			if( Allocation.WeightmapTextureIndex > DeleteLayerWeightmapTextureIndex )
			{
				Allocation.WeightmapTextureIndex--;
			}

			check( Allocation.WeightmapTextureIndex < Component->WeightmapTextures.Num() );
		}
	}

	// Update the shaders for this component
	Component->UpdateMaterialInstances();
}

void FLandscapeEditDataInterface::DeleteLayer(FName LayerName)
{
	if (!LandscapeInfo) return;
	for( TMap<QWORD,ULandscapeComponent*>::TIterator It(LandscapeInfo->XYtoComponentMap); It; ++It )
	{
		ULandscapeComponent* Component = It.Value();
		Component->DeleteLayer(LayerName, this);

		// Update dominant layer info stored in collision component
		TArray<FColor*> CollisionWeightmapMipData;
		for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
		{
			CollisionWeightmapMipData.AddItem( (FColor*)GetTextureDataInfo(Component->WeightmapTextures(WeightmapIdx))->GetMipData(Component->CollisionMipLevel) );
		}
		Component->UpdateCollisionLayerData(CollisionWeightmapMipData);
	}
}

// simple classes for the template....
namespace
{
	template<typename TDataType>
	struct TArrayStoreData
	{
		INT X1;
		INT Y1;
		TDataType* Data;
		INT Stride;
		INT ArraySize;

		TArrayStoreData(INT InX1, INT InY1, TDataType* InData, INT InStride)
			:	X1(InX1)
			,	Y1(InY1)
			,	Data(InData)
			,	Stride(InStride)
			,	ArraySize(1)
		{}

		inline void Store(INT LandscapeX, INT LandscapeY, BYTE Weight) {}
		inline void Store(INT LandscapeX, INT LandscapeY, BYTE Weight, INT LayerIdx) {}
		inline BYTE Load(INT LandscapeX, INT LandscapeY) { return 0; }
		inline void PreInit(INT InArraySize) { ArraySize = InArraySize; }
	};

	void TArrayStoreData<BYTE>::Store(INT LandscapeX, INT LandscapeY, BYTE Weight)
	{
		Data[ (LandscapeY-Y1) * Stride + (LandscapeX-X1) ] = Weight;
	}

	BYTE TArrayStoreData<BYTE>::Load(INT LandscapeX, INT LandscapeY)
	{
		return Data[ (LandscapeY-Y1) * Stride + (LandscapeX-X1) ];
	}

	// Data items should be initialized with ArraySize
	void TArrayStoreData<TArray<BYTE>>::Store(INT LandscapeX, INT LandscapeY, BYTE Weight, INT LayerIdx)
	{
		TArray<BYTE>& Value = Data[ ((LandscapeY-Y1) * Stride + (LandscapeX-X1)) ];
		if (Value.Num() != ArraySize)
		{
			Value.Empty(ArraySize);
			Value.AddZeroed(ArraySize);
		}
		Value(LayerIdx) = Weight;
	}

	template<typename TDataType>
	struct TSparseStoreData
	{
		TMap<QWORD, TDataType>& SparseData;
		INT ArraySize;

		TSparseStoreData(TMap<QWORD, TDataType>& InSparseData)
			:	SparseData(InSparseData)
			,	ArraySize(1)
		{}

		inline void Store(INT LandscapeX, INT LandscapeY, BYTE Weight) {}
		inline void Store(INT LandscapeX, INT LandscapeY, BYTE Weight, INT LayerIdx) {}
		inline BYTE Load(INT LandscapeX, INT LandscapeY) { return 0; }
		inline void PreInit(INT InArraySize) { ArraySize = InArraySize; }
	};

	void TSparseStoreData<BYTE>::Store(INT LandscapeX, INT LandscapeY, BYTE Weight)
	{
		SparseData.Set(ALandscape::MakeKey(LandscapeX,LandscapeY), Weight);
	}

	BYTE TSparseStoreData<BYTE>::Load(INT LandscapeX, INT LandscapeY)
	{
		return SparseData.FindRef(ALandscape::MakeKey(LandscapeX,LandscapeY));
	}

	void TSparseStoreData<TArray<BYTE>>::Store(INT LandscapeX, INT LandscapeY, BYTE Weight, INT LayerIdx)
	{
		TArray<BYTE>* Value = SparseData.Find(ALandscape::MakeKey(LandscapeX,LandscapeY));
		if (Value)
		{
			(*Value)(LayerIdx) = Weight;
		}
		else
		{
			TArray<BYTE> Value;
			Value.Empty(ArraySize);
			Value.AddZeroed(ArraySize);
			Value(LayerIdx) = Weight;
			SparseData.Set(ALandscape::MakeKey(LandscapeX,LandscapeY), Value);
		}
	}
};

void FLandscapeEditDataInterface::SetAlphaData(FName LayerName, INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data, INT Stride, UBOOL bWeightAdjust/* = TRUE */, UBOOL bTotalWeightAdjust/* = FALSE */, TSet<FName>* DirtyLayerNames /*= NULL*/)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	check(ComponentSizeQuads > 0);
	// Find component range for this block of data
	INT ComponentIndexX1 = (X1-1 >= 0) ? (X1-1) / ComponentSizeQuads : (X1) / ComponentSizeQuads - 1;	// -1 because we need to pick up vertices shared between components
	INT ComponentIndexY1 = (Y1-1 >= 0) ? (Y1-1) / ComponentSizeQuads : (Y1) / ComponentSizeQuads - 1;
	INT ComponentIndexX2 = (X2 >= 0) ? X2 / ComponentSizeQuads : (X2+1) / ComponentSizeQuads - 1;
	INT ComponentIndexY2 = (Y2 >= 0) ? Y2 / ComponentSizeQuads : (Y2+1) / ComponentSizeQuads - 1;

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{	
			QWORD ComponentKey = ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads);
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ComponentKey);

			// if NULL, it was painted away
			if( Component==NULL )
			{
				continue;
			}

			INT UpdateLayerIdx = INDEX_NONE;
			TArray<FName> NeedAllocationNames;

			// If LayerName is passed in as NAME_None, we are updating all layers simultaneously.
			if( LayerName == NAME_None )
			{
				if (!DirtyLayerNames)
				{
					check(FALSE); // Invalid function usage: Need candidate for allocation 
				}
				else
				{
					for (TSet<FName>::TIterator It(*DirtyLayerNames); It; ++It)
					{
						UBOOL bFound = FALSE;
						for( INT LayerIdx=0;LayerIdx < Component->WeightmapLayerAllocations.Num();LayerIdx++ )
						{
							if( Component->WeightmapLayerAllocations(LayerIdx).LayerName == *It )
							{
								bFound = TRUE;
								break;
							}
						}
						if (!bFound)
						{
							NeedAllocationNames.AddItem(*It);
						}
					}
				}
			}
			else
			{
				for( INT LayerIdx=0;LayerIdx < Component->WeightmapLayerAllocations.Num();LayerIdx++ )
				{
					if( Component->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
					{
						UpdateLayerIdx = LayerIdx;
						break;
					}
				}
			}

			// See if we need to reallocate our weightmaps
			if( UpdateLayerIdx == INDEX_NONE && LayerName != NAME_None )
			{
				NeedAllocationNames.AddItem(LayerName);
			}

			// Need allocation for weightmaps
			if (NeedAllocationNames.Num())
			{
				Component->Modify();
				for (INT i = 0; i < NeedAllocationNames.Num(); ++i)
				{
					UpdateLayerIdx = Component->WeightmapLayerAllocations.Num();
					new (Component->WeightmapLayerAllocations) FWeightmapLayerAllocationInfo(NeedAllocationNames(i));
					Component->ReallocateWeightmaps(this);
				}
				Component->UpdateMaterialInstances();
				if( Component->EditToolRenderData )
				{
					Component->EditToolRenderData->UpdateDebugColorMaterial();
				}
			}

			// Lock data for all the weightmaps
			TArray<FLandscapeTextureDataInfo*> TexDataInfos;
			for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
			{
				TexDataInfos.AddItem(GetTextureDataInfo(Component->WeightmapTextures(WeightmapIdx)));
			}

			TArray<BYTE*> LayerDataPtrs;		// Pointers to all layers' data 
			TArray<UBOOL> LayerNoWeightBlends;	// NoWeightBlend flags
			TArray<UBOOL> LayerEditDataAllZero; // Whether the data we are editing for this layer is all zero 
			TArray<UBOOL> LayerEditDataPrevNonzero; // Whether some data we are editing for this layer was previously non-zero 
			for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				FWeightmapLayerAllocationInfo& Allocation = Component->WeightmapLayerAllocations(LayerIdx);

				LayerDataPtrs.AddItem( (BYTE*)TexDataInfos(Allocation.WeightmapTextureIndex)->GetMipData(0) + ChannelOffsets[Allocation.WeightmapTextureChannel] );

				// Find the layer info and record if it is a bNoWeightBlend layer.
				FLandscapeLayerStruct* LayerInfo = LandscapeInfo->LayerInfoMap.FindRef(Allocation.LayerName);
				LayerNoWeightBlends.AddItem( (LayerInfo && LayerInfo->LayerInfoObj && LayerInfo->LayerInfoObj->bNoWeightBlend) || Allocation.LayerName == ALandscape::DataWeightmapName );
				LayerEditDataAllZero.AddItem(TRUE);
				LayerEditDataPrevNonzero.AddItem(FALSE);
			}

			for( INT LayerIdx=0;LayerIdx < Component->WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				if( Component->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
				{
					UpdateLayerIdx = LayerIdx;
					break;
				}
			}

			// Find the texture data corresponding to this vertex
			INT SizeU = Component->WeightmapTextures(0)->SizeX;		// not exactly correct.  
			INT SizeV = Component->WeightmapTextures(0)->SizeY;
			INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
			INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;
							checkSlow( LandscapeX >= X1 && LandscapeX <= X2 );
							checkSlow( LandscapeY >= Y1 && LandscapeY <= Y2 );

							// Find the input data corresponding to this vertex
							INT DataIndex = (LandscapeX-X1) + Stride * (LandscapeY-Y1);
							BYTE NewWeight = Data[DataIndex];

							// Adjust all layer weights
							INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
							INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;

							INT TexDataIndex = 4 * (TexX + TexY * SizeU);

							INT OtherLayerWeightSum = 0;

							if( bWeightAdjust )
							{
								if (bTotalWeightAdjust)
								{
									INT MaxLayerIdx = -1;
									INT MaxWeight = INT_MIN;

									// Adjust other layers' weights accordingly
									for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
									{
										BYTE& ExistingWeight = LayerDataPtrs(LayerIdx)[TexDataIndex];

										if (ExistingWeight != 0)
										{
											LayerEditDataPrevNonzero(LayerIdx) = TRUE;
										}

										if (LayerIdx == UpdateLayerIdx)
										{
											ExistingWeight = NewWeight;
										}
										// Exclude bNoWeightBlend layers
										if( LayerNoWeightBlends(LayerIdx)==FALSE )
										{
											OtherLayerWeightSum += ExistingWeight;
											if (MaxWeight < ExistingWeight)
											{
												MaxWeight = ExistingWeight;
												MaxLayerIdx = LayerIdx;
											}
										}
									}

									if (OtherLayerWeightSum != 255)
									{
										FLOAT Factor = 255.f / OtherLayerWeightSum;
										OtherLayerWeightSum = 0;

										// Normalize
										for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
										{
											BYTE& ExistingWeight = LayerDataPtrs(LayerIdx)[TexDataIndex];

											if( LayerNoWeightBlends(LayerIdx)==FALSE )
											{
												// normalization...
												ExistingWeight = (BYTE)(Factor * ExistingWeight);
												OtherLayerWeightSum += ExistingWeight;
											}

											if( ExistingWeight != 0 )
											{
												LayerEditDataAllZero(LayerIdx) = FALSE;
											}
										}

										if (255 - OtherLayerWeightSum && MaxLayerIdx >= 0)
										{
											LayerDataPtrs(MaxLayerIdx)[TexDataIndex] += 255 - OtherLayerWeightSum;
										}
									}
								}
								else
								{
									// Adjust other layers' weights accordingly
									for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
									{
										BYTE ExistingWeight = LayerDataPtrs(LayerIdx)[TexDataIndex];
										// Exclude bNoWeightBlend layers
										if( LayerIdx != UpdateLayerIdx && LayerNoWeightBlends(LayerIdx)==FALSE )
										{
											OtherLayerWeightSum += ExistingWeight;
										}
									}

									if( OtherLayerWeightSum == 0 )
									{
										NewWeight = 255;
										OtherLayerWeightSum = 1;
									}

									for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
									{
										BYTE& Weight = LayerDataPtrs(LayerIdx)[TexDataIndex];

										if( Weight != 0 )
										{
											LayerEditDataPrevNonzero(LayerIdx) = TRUE;
										}

										if( LayerIdx == UpdateLayerIdx )
										{
											Weight = NewWeight;
										}
										else
										{
											// Exclude bNoWeightBlend layers
											if( LayerNoWeightBlends(LayerIdx)==FALSE )
											{
												Weight = Clamp<BYTE>( appRound((FLOAT)(255 - NewWeight) * (FLOAT)Weight/(FLOAT)OtherLayerWeightSum), 0, 255 );
											}
										}

										if( Weight != 0 )
										{
											LayerEditDataAllZero(LayerIdx) = FALSE;
										}
									}
								}
							}
							else
							{
								// Weight value set without adjusting other layers' weights

								// Apply weights to all layers simultaneously.
								if( LayerName == NAME_None ) 
								{
									for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
									{
										BYTE& Weight = LayerDataPtrs(LayerIdx)[TexDataIndex];

										if( Weight != 0 )
										{
											LayerEditDataPrevNonzero(LayerIdx) = TRUE;
										}

										// Find Index in LayerNames
										{
											TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
											for( INT NameIdx = 0; It && NameIdx < LandscapeInfo->LayerInfoMap.Num(); ++It, NameIdx++ )
											{
												ULandscapeLayerInfoObject* LayerInfo = It.Value() ? It.Value()->LayerInfoObj : NULL;
												if( LayerInfo && LayerInfo->LayerName == Component->WeightmapLayerAllocations(LayerIdx).LayerName )
												{
													Weight = Data[DataIndex * LandscapeInfo->LayerInfoMap.Num() + NameIdx]; // Only for whole weight
													if( Weight != 0 )
													{
														LayerEditDataAllZero(LayerIdx) = FALSE;
													}
													break;
												}
											}
										}
									}
								}
								else
								{
									BYTE& Weight = LayerDataPtrs(UpdateLayerIdx)[TexDataIndex];
									if( Weight != 0 )
									{
										LayerEditDataPrevNonzero(UpdateLayerIdx) = TRUE;
									}

									Weight = NewWeight;
									if( Weight != 0 )
									{
										LayerEditDataAllZero(UpdateLayerIdx) = FALSE;
									}
								}
							}
						}
					}

					// Record the areas of the texture we need to re-upload
					INT TexX1 = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX1;
					INT TexY1 = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY1;
					INT TexX2 = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX2;
					INT TexY2 = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY2;
					for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
					{
						TexDataInfos(WeightmapIdx)->AddMipUpdateRegion(0,TexX1,TexY1,TexX2,TexY2);
					}
				}
			}

			// Update mipmaps
			TArray<FColor*> CollisionWeightmapMipData(Component->WeightmapTextures.Num());
			for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
			{
				UTexture2D* WeightmapTexture = Component->WeightmapTextures(WeightmapIdx);

				INT NumMips = WeightmapTexture->Mips.Num();
				TArray<FColor*> WeightmapTextureMipData(NumMips);
				for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
				{
					FColor* MipData = (FColor*)TexDataInfos(WeightmapIdx)->GetMipData(MipIdx);
					WeightmapTextureMipData(MipIdx) = MipData;
					if( MipIdx == Component->CollisionMipLevel )
					{
						CollisionWeightmapMipData(WeightmapIdx) = MipData;
					}
				}
				ULandscapeComponent::UpdateWeightmapMips(ComponentNumSubsections, SubsectionSizeQuads, WeightmapTexture, WeightmapTextureMipData, ComponentX1, ComponentY1, ComponentX2, ComponentY2, TexDataInfos(WeightmapIdx));
			}

			// Update dominant layer info stored in collision component
			Component->UpdateCollisionLayerData(CollisionWeightmapMipData, ComponentX1, ComponentY1, ComponentX2, ComponentY2);

			// Check if we need to remove weightmap allocations for layers that were completely painted away
			UBOOL bRemovedLayer = FALSE;
			for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				if( LayerEditDataAllZero(LayerIdx) && LayerEditDataPrevNonzero(LayerIdx) )
				{
					// Check the data for the entire component and to see if it's all zero
					for( INT SubIndexY=0;SubIndexY<ComponentNumSubsections;SubIndexY++ )
					{
						for( INT SubIndexX=0;SubIndexX<ComponentNumSubsections;SubIndexX++ )
						{
							for( INT SubY=0;SubY<=SubsectionSizeQuads;SubY++ )
							{
								for( INT SubX=0;SubX<=SubsectionSizeQuads;SubX++ )
								{
									INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
									INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
									INT TexDataIndex = 4 * (TexX + TexY * SizeU);

									// Stop the first time we see any non-zero data
									BYTE& Weight = LayerDataPtrs(LayerIdx)[TexDataIndex];
									if( Weight != 0 )
									{
										goto NextLayer;
									}
								}
							}
						}
					}

					// Mark the channel as unallocated, so we can reuse it later
					INT DeleteLayerWeightmapTextureIndex = Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex;
					ALandscapeProxy* Proxy = CastChecked<ALandscapeProxy>(Component->GetOuter());
					FLandscapeWeightmapUsage* Usage = Proxy->WeightmapUsageMap.Find(Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex));
					check(Usage);
					Usage->ChannelUsage[Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel] = NULL;

					// Remove the layer as it's totally painted away.
					Component->WeightmapLayerAllocations.Remove(LayerIdx);
					LayerEditDataAllZero.Remove(LayerIdx);
					LayerEditDataPrevNonzero.Remove(LayerIdx);
					LayerDataPtrs.Remove(LayerIdx);

					// Check if the weightmap texture used by the layer we just removed is used by any other layer, and if so, remove the texture too
					UBOOL bCanRemoveLayerTexture = TRUE;
					for( INT OtherLayerIdx=0;OtherLayerIdx<Component->WeightmapLayerAllocations.Num();OtherLayerIdx++ )
					{
						if( Component->WeightmapLayerAllocations(OtherLayerIdx).WeightmapTextureIndex == DeleteLayerWeightmapTextureIndex )
						{
							bCanRemoveLayerTexture = FALSE;
							break;
						}
					}
					if( bCanRemoveLayerTexture )
					{
						Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex)->MarkPackageDirty();
						Component->WeightmapTextures(DeleteLayerWeightmapTextureIndex)->ClearFlags(RF_Standalone);
						Component->WeightmapTextures.Remove(DeleteLayerWeightmapTextureIndex);

						// Adjust WeightmapTextureChannel index for other layers
						for( INT OtherLayerIdx=0;OtherLayerIdx<Component->WeightmapLayerAllocations.Num();OtherLayerIdx++ )
						{
							FWeightmapLayerAllocationInfo& Allocation = Component->WeightmapLayerAllocations(OtherLayerIdx);
							if( Allocation.WeightmapTextureIndex > DeleteLayerWeightmapTextureIndex )
							{
								Allocation.WeightmapTextureIndex--;
							}
						}
					}

					bRemovedLayer = TRUE;
					LayerIdx--;
				}
NextLayer:;
			}

			if( bRemovedLayer )
			{
				Component->UpdateMaterialInstances();
			}
		}
	}
}

template<typename TStoreData>
void FLandscapeEditDataInterface::GetWeightDataTemplFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TStoreData& StoreData)
{
	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndices(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{		
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));

			if( !Component )
			{
				continue;
			}

			UTexture2D* WeightmapTexture = NULL;
			FLandscapeTextureDataInfo* TexDataInfo = NULL;
			BYTE* WeightmapTextureData = NULL;
			BYTE WeightmapChannelOffset = 0;
			TArray<FLandscapeTextureDataInfo*> TexDataInfos; // added for whole weight case...

			if (LayerName != NAME_None)
			{
				for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
				{
					if( Component->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
					{
						WeightmapTexture = Component->WeightmapTextures(Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
						TexDataInfo = GetTextureDataInfo(WeightmapTexture);
						WeightmapTextureData = (BYTE*)TexDataInfo->GetMipData(0);
						WeightmapChannelOffset = ChannelOffsets[Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
						break;
					}
				}
			}
			else
			{
				// Lock data for all the weightmaps
				for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
				{
					TexDataInfos.AddItem(GetTextureDataInfo(Component->WeightmapTextures(WeightmapIdx)));
				}
			}

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;

							if (LayerName != NAME_None)
							{
								// Find the input data corresponding to this vertex
								BYTE Weight;
								if( WeightmapTexture )
								{
									// Find the texture data corresponding to this vertex
									INT SizeU = WeightmapTexture->SizeX;
									INT SizeV = WeightmapTexture->SizeY;
									INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
									INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

									INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
									INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
									Weight = WeightmapTextureData[ 4 * (TexX + TexY * SizeU) + WeightmapChannelOffset ];
								}
								else
								{
									Weight = 0;
								}

								StoreData.Store(LandscapeX, LandscapeY, Weight);
							}
							else // Whole weight map case...
							{
								StoreData.PreInit(LandscapeInfo->LayerInfoMap.Num());
								for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
								{
									INT Idx = Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex;
									UTexture2D* WeightmapTexture = Component->WeightmapTextures(Idx);
									BYTE* WeightmapTextureData = (BYTE*)TexDataInfos(Idx)->GetMipData(0);
									BYTE WeightmapChannelOffset = ChannelOffsets[Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];

									// Find the texture data corresponding to this vertex
									INT SizeU = WeightmapTexture->SizeX;
									INT SizeV = WeightmapTexture->SizeY;
									INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
									INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

									INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
									INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;

									BYTE Weight = WeightmapTextureData[ 4 * (TexX + TexY * SizeU) + WeightmapChannelOffset ];

									// Find in index in LayerName
									{
										TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
										for ( INT NameIdx = 0; NameIdx < LandscapeInfo->LayerInfoMap.Num() && It; NameIdx++, ++It )
										{
											ULandscapeLayerInfoObject* LayerInfo = It.Value() ? It.Value()->LayerInfoObj : NULL;
											if (LayerInfo && LayerInfo->LayerName == Component->WeightmapLayerAllocations(LayerIdx).LayerName)
											{
												StoreData.Store(LandscapeX, LandscapeY, Weight, NameIdx);
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}
}

BYTE FLandscapeEditDataInterface::GetWeightMapData(const ULandscapeComponent* Component, FName LayerName, INT TexU, INT TexV, BYTE Offset /*= 0*/, UTexture2D* Texture /*= NULL*/, BYTE* TextureData /*= NULL*/)
{
	check(Component);
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};
	if (!Texture || !TextureData)
	{
		if (LayerName != NAME_None)
		{
			for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
			{
				if( Component->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
				{
					Texture = Component->WeightmapTextures(Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
					FLandscapeTextureDataInfo* TexDataInfo = GetTextureDataInfo(Texture);
					TextureData = (BYTE*)TexDataInfo->GetMipData(0);
					Offset = ChannelOffsets[Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
					break;
				}
			}
		}
	}

	if (Texture && TextureData)
	{
		INT SizeU = Texture->SizeX;
		INT SizeV = Texture->SizeY;
		INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
		INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

		INT TexX = WeightmapOffsetX + TexU;
		INT TexY = WeightmapOffsetY + TexV;
		return TextureData[ 4 * (TexX + TexY * SizeU) + Offset ];
	}
	return 0;
}

template<typename TStoreData>
void FLandscapeEditDataInterface::GetWeightDataTempl(FName LayerName, INT& ValidX1, INT& ValidY1, INT& ValidX2, INT& ValidY2, TStoreData& StoreData)
{
	// Copy variables
	INT X1 = ValidX1, X2 = ValidX2, Y1 = ValidY1, Y2 = ValidY2;
	ValidX1 = INT_MAX; ValidX2 = INT_MIN; ValidY1 = INT_MAX; ValidY2 = INT_MIN;

	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndices(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	INT ComponentSizeX = ComponentIndexX2-ComponentIndexX1+1;
	INT ComponentSizeY = ComponentIndexY2-ComponentIndexY1+1;

	// Neighbor Components
	ULandscapeComponent* BorderComponent[4] = {0, 0, 0, 0};
	ULandscapeComponent* CornerComponent[4] = {0, 0, 0, 0};
	UBOOL NoBorderX1 = FALSE, NoBorderX2 = FALSE;
	TArray<UBOOL> NoBorderY1, NoBorderY2, ComponentDataExist;
	TArray<ULandscapeComponent*> BorderComponentY1, BorderComponentY2;
	ComponentDataExist.Empty(ComponentSizeX*ComponentSizeY);
	ComponentDataExist.AddZeroed(ComponentSizeX*ComponentSizeY);
	UBOOL bHasMissingValue = FALSE;

	UTexture2D* NeighborWeightmapTexture[4] = {0, 0, 0, 0};
	FLandscapeTextureDataInfo* NeighborTexDataInfo[4] = {0, 0, 0, 0};
	BYTE* NeighborWeightmapTextureData[4] = {0, 0, 0, 0};
	BYTE NeighborWeightmapChannelOffset[4] = {0, 0, 0, 0};
	BYTE CornerValues[4] = {0, 0, 0, 0};
	INT EdgeCoord = (SubsectionSizeQuads+1) * ComponentNumSubsections - 1; //ComponentSizeQuads;

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	// initial loop....
	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		NoBorderX1 = FALSE;
		NoBorderX2 = FALSE;
		BorderComponent[0] = BorderComponent[1] = NULL;
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{	
			BorderComponent[2] = BorderComponent[3] = NULL;
			INT ComponentIndexXY = ComponentSizeX*(ComponentIndexY-ComponentIndexY1) + ComponentIndexX-ComponentIndexX1;
			INT ComponentIndexXX = ComponentIndexX - ComponentIndexX1;
			INT ComponentIndexYY = ComponentIndexY - ComponentIndexY1;
			ComponentDataExist(ComponentIndexXY) = FALSE;
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));

			UTexture2D* WeightmapTexture = NULL;
			FLandscapeTextureDataInfo* TexDataInfo = NULL;
			BYTE* WeightmapTextureData = NULL;
			BYTE WeightmapChannelOffset = 0;
			TArray<FLandscapeTextureDataInfo*> TexDataInfos; // added for whole weight case...
			BYTE CornerSet = 0;
			UBOOL ExistLeft = ComponentIndexXX > 0 && ComponentDataExist( ComponentIndexXX-1 + ComponentIndexYY * ComponentSizeX );
			UBOOL ExistUp = ComponentIndexYY > 0 && ComponentDataExist( ComponentIndexXX + (ComponentIndexYY-1) * ComponentSizeX );

			if( Component )
			{
				if (LayerName != NAME_None)
				{
					for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
					{
						if( Component->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
						{
							WeightmapTexture = Component->WeightmapTextures(Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
							TexDataInfo = GetTextureDataInfo(WeightmapTexture);
							WeightmapTextureData = (BYTE*)TexDataInfo->GetMipData(0);
							WeightmapChannelOffset = ChannelOffsets[Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
							break;
						}
					}
				}
				else
				{
					// Lock data for all the weightmaps
					for( INT WeightmapIdx=0;WeightmapIdx < Component->WeightmapTextures.Num();WeightmapIdx++ )
					{
						TexDataInfos.AddItem(GetTextureDataInfo(Component->WeightmapTextures(WeightmapIdx)));
					}
				}
				ComponentDataExist(ComponentIndexXY) = TRUE;
				// Update valid region
				ValidX1 = Min<INT>(Component->SectionBaseX, ValidX1);
				ValidX2 = Max<INT>(Component->SectionBaseX+ComponentSizeQuads, ValidX2);
				ValidY1 = Min<INT>(Component->SectionBaseY, ValidY1);
				ValidY2 = Max<INT>(Component->SectionBaseY+ComponentSizeQuads, ValidY2);
			}
			else
			{
				if (!bHasMissingValue)
				{
					NoBorderY1.Empty(ComponentSizeX);
					NoBorderY2.Empty(ComponentSizeX);
					NoBorderY1.AddZeroed(ComponentSizeX);
					NoBorderY2.AddZeroed(ComponentSizeX);
					BorderComponentY1.Empty(ComponentSizeX);
					BorderComponentY2.Empty(ComponentSizeX);
					BorderComponentY1.AddZeroed(ComponentSizeX);
					BorderComponentY2.AddZeroed(ComponentSizeX);
					bHasMissingValue = TRUE;
				}

				// Search for neighbor component for interpolation
				UBOOL bShouldSearchX = (BorderComponent[1] && BorderComponent[1]->SectionBaseX / ComponentSizeQuads <= ComponentIndexX);
				UBOOL bShouldSearchY = (BorderComponentY2(ComponentIndexXX) && BorderComponentY2(ComponentIndexXX)->SectionBaseY / ComponentSizeQuads <= ComponentIndexY);
				// Search for left-closest component
				if ( bShouldSearchX || (!NoBorderX1 && !BorderComponent[0]))
				{
					NoBorderX1 = TRUE;
					for (INT X = ComponentIndexX-1; X >= ComponentIndexX1; X--)
					{
						BorderComponent[0] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(X*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));
						if (BorderComponent[0])
						{
							NoBorderX1 = FALSE;
							if (LayerName != NAME_None)
							{
								for( INT LayerIdx=0;LayerIdx<BorderComponent[0]->WeightmapLayerAllocations.Num();LayerIdx++ )
								{
									if( BorderComponent[0]->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
									{
										NeighborWeightmapTexture[0] = BorderComponent[0]->WeightmapTextures(BorderComponent[0]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
										NeighborTexDataInfo[0] = GetTextureDataInfo(NeighborWeightmapTexture[0]);
										NeighborWeightmapTextureData[0] = (BYTE*)NeighborTexDataInfo[0]->GetMipData(0);
										NeighborWeightmapChannelOffset[0] = ChannelOffsets[BorderComponent[0]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
										break;
									}
								}
							}
							break;
						}
					}
				}
				// Search for right-closest component
				if ( bShouldSearchX || (!NoBorderX2 && !BorderComponent[1]))
				{
					NoBorderX2 = TRUE;
					for (INT X = ComponentIndexX+1; X <= ComponentIndexX2; X++)
					{
						BorderComponent[1] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(X*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));
						if (BorderComponent[1])
						{
							NoBorderX2 = FALSE;
							if (LayerName != NAME_None)
							{
								for( INT LayerIdx=0;LayerIdx<BorderComponent[1]->WeightmapLayerAllocations.Num();LayerIdx++ )
								{
									if( BorderComponent[1]->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
									{
										NeighborWeightmapTexture[1] = BorderComponent[1]->WeightmapTextures(BorderComponent[1]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
										NeighborTexDataInfo[1] = GetTextureDataInfo(NeighborWeightmapTexture[1]);
										NeighborWeightmapTextureData[1] = (BYTE*)NeighborTexDataInfo[1]->GetMipData(0);
										NeighborWeightmapChannelOffset[1] = ChannelOffsets[BorderComponent[1]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
										break;
									}
								}
							}
						}
					}
				}
				// Search for up-closest component
				if ( bShouldSearchY || (!NoBorderY1(ComponentIndexXX) && !BorderComponentY1(ComponentIndexXX)))
				{
					NoBorderY1(ComponentIndexXX) = TRUE;
					for (INT Y = ComponentIndexY-1; Y >= ComponentIndexY1; Y--)
					{
						BorderComponentY1(ComponentIndexXX) = BorderComponent[2] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,Y*ComponentSizeQuads));
						if (BorderComponent[2])
						{
							NoBorderY1(ComponentIndexXX) = FALSE;
							if (LayerName != NAME_None)
							{
								for( INT LayerIdx=0;LayerIdx<BorderComponent[2]->WeightmapLayerAllocations.Num();LayerIdx++ )
								{
									if( BorderComponent[2]->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
									{
										NeighborWeightmapTexture[2] = BorderComponent[2]->WeightmapTextures(BorderComponent[2]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
										NeighborTexDataInfo[2] = GetTextureDataInfo(NeighborWeightmapTexture[2]);
										NeighborWeightmapTextureData[2] = (BYTE*)NeighborTexDataInfo[2]->GetMipData(0);
										NeighborWeightmapChannelOffset[2] = ChannelOffsets[BorderComponent[2]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
										break;
									}
								}
							}
						}
					}
				}
				else
				{
					BorderComponent[2] = BorderComponentY1(ComponentIndexXX);
					if (BorderComponent[2])
					{
						if (LayerName != NAME_None)
						{
							for( INT LayerIdx=0;LayerIdx<BorderComponent[2]->WeightmapLayerAllocations.Num();LayerIdx++ )
							{
								if( BorderComponent[2]->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
								{
									NeighborWeightmapTexture[2] = BorderComponent[2]->WeightmapTextures(BorderComponent[2]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
									NeighborTexDataInfo[2] = GetTextureDataInfo(NeighborWeightmapTexture[2]);
									NeighborWeightmapTextureData[2] = (BYTE*)NeighborTexDataInfo[2]->GetMipData(0);
									NeighborWeightmapChannelOffset[2] = ChannelOffsets[BorderComponent[2]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
									break;
								}
							}
						}
					}
				}
				// Search for bottom-closest component
				if ( bShouldSearchY || (!NoBorderY2(ComponentIndexXX) && !BorderComponentY2(ComponentIndexXX)))
				{
					NoBorderY2(ComponentIndexXX) = TRUE;
					for (INT Y = ComponentIndexY+1; Y <= ComponentIndexY2; Y++)
					{
						BorderComponentY2(ComponentIndexXX) = BorderComponent[3] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,Y*ComponentSizeQuads));
						if (BorderComponent[3])
						{
							NoBorderY2(ComponentIndexXX) = FALSE;
							if (LayerName != NAME_None)
							{
								for( INT LayerIdx=0;LayerIdx<BorderComponent[3]->WeightmapLayerAllocations.Num();LayerIdx++ )
								{
									if( BorderComponent[3]->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
									{
										NeighborWeightmapTexture[3] = BorderComponent[3]->WeightmapTextures(BorderComponent[3]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
										NeighborTexDataInfo[3] = GetTextureDataInfo(NeighborWeightmapTexture[3]);
										NeighborWeightmapTextureData[3] = (BYTE*)NeighborTexDataInfo[3]->GetMipData(0);
										NeighborWeightmapChannelOffset[3] = ChannelOffsets[BorderComponent[3]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
										break;
									}
								}
							}
							break;
						}
					}
				}
				else
				{
					BorderComponent[3] = BorderComponentY2(ComponentIndexXX);
					if (BorderComponent[3])
					{
						if (LayerName != NAME_None)
						{
							for( INT LayerIdx=0;LayerIdx<BorderComponent[3]->WeightmapLayerAllocations.Num();LayerIdx++ )
							{
								if( BorderComponent[3]->WeightmapLayerAllocations(LayerIdx).LayerName == LayerName )
								{
									NeighborWeightmapTexture[3] = BorderComponent[3]->WeightmapTextures(BorderComponent[3]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex);
									NeighborTexDataInfo[3] = GetTextureDataInfo(NeighborWeightmapTexture[3]);
									NeighborWeightmapTextureData[3] = (BYTE*)NeighborTexDataInfo[3]->GetMipData(0);
									NeighborWeightmapChannelOffset[3] = ChannelOffsets[BorderComponent[3]->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];
									break;
								}
							}
						}
					}
				}

				CornerComponent[0] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX-1)*ComponentSizeQuads,(ComponentIndexY-1)*ComponentSizeQuads));
				CornerComponent[1] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX+1)*ComponentSizeQuads,(ComponentIndexY-1)*ComponentSizeQuads));
				CornerComponent[2] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX-1)*ComponentSizeQuads,(ComponentIndexY+1)*ComponentSizeQuads));
				CornerComponent[3] = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey((ComponentIndexX+1)*ComponentSizeQuads,(ComponentIndexY+1)*ComponentSizeQuads));

				if (CornerComponent[0])
				{
					CornerSet |= 1;
					CornerValues[0] = GetWeightMapData(CornerComponent[0], LayerName, EdgeCoord, EdgeCoord);
				}
				else if ((ExistLeft || ExistUp) && X1 <= ComponentIndexX*ComponentSizeQuads && Y1 <= ComponentIndexY*ComponentSizeQuads  )
				{
					CornerSet |= 1;
					CornerValues[0] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, ComponentIndexY*ComponentSizeQuads);
				}
				else if (BorderComponent[0])
				{
					CornerSet |= 1;
					CornerValues[0] = GetWeightMapData(BorderComponent[0], LayerName, EdgeCoord, 0, NeighborWeightmapChannelOffset[0], NeighborWeightmapTexture[0], NeighborWeightmapTextureData[0]);
				}
				else if (BorderComponent[2])
				{
					CornerSet |= 1;
					CornerValues[0] = GetWeightMapData(BorderComponent[2], LayerName, 0, EdgeCoord, NeighborWeightmapChannelOffset[2], NeighborWeightmapTexture[2], NeighborWeightmapTextureData[2]);
				}

				if (CornerComponent[1])
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = GetWeightMapData(CornerComponent[1], LayerName, 0, EdgeCoord);
				}
				else if (ExistUp && X2 >= (ComponentIndexX+1)*ComponentSizeQuads)
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = StoreData.Load( (ComponentIndexX+1)*ComponentSizeQuads, ComponentIndexY*ComponentSizeQuads);
				}
				else if (BorderComponent[1])
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = GetWeightMapData(BorderComponent[1], LayerName, 0, 0, NeighborWeightmapChannelOffset[1], NeighborWeightmapTexture[1], NeighborWeightmapTextureData[1]);
				}
				else if (BorderComponent[2])
				{
					CornerSet |= 1 << 1;
					CornerValues[1] = GetWeightMapData(BorderComponent[2], LayerName, EdgeCoord, EdgeCoord, NeighborWeightmapChannelOffset[2], NeighborWeightmapTexture[2], NeighborWeightmapTextureData[2]);
				}

				if (CornerComponent[2])
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = GetWeightMapData(CornerComponent[2], LayerName, EdgeCoord, 0);
				}
				else if (ExistLeft && Y2 >= (ComponentIndexY+1)*ComponentSizeQuads) // Use data already stored for 0, 2
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, (ComponentIndexY+1)*ComponentSizeQuads);
				}
				else if (BorderComponent[0])
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = GetWeightMapData(BorderComponent[0], LayerName, EdgeCoord, EdgeCoord, NeighborWeightmapChannelOffset[0], NeighborWeightmapTexture[0], NeighborWeightmapTextureData[0]);
				}
				else if (BorderComponent[3])
				{
					CornerSet |= 1 << 2;
					CornerValues[2] = GetWeightMapData(BorderComponent[3], LayerName, 0, 0, NeighborWeightmapChannelOffset[3], NeighborWeightmapTexture[3], NeighborWeightmapTextureData[3]);
				}

				if (CornerComponent[3])
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = GetWeightMapData(CornerComponent[3], LayerName, 0, 0);
				}
				else if (BorderComponent[1])
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = GetWeightMapData(BorderComponent[1], LayerName, 0, EdgeCoord, NeighborWeightmapChannelOffset[1], NeighborWeightmapTexture[1], NeighborWeightmapTextureData[1]);
				}
				else if (BorderComponent[3])
				{
					CornerSet |= 1 << 3;
					CornerValues[3] = GetWeightMapData(BorderComponent[3], LayerName, EdgeCoord, 0, NeighborWeightmapChannelOffset[3], NeighborWeightmapTexture[3], NeighborWeightmapTextureData[3]);
				}

				FillCornerValues(CornerSet, CornerValues);
				ComponentDataExist(ComponentIndexXY) = ExistLeft || ExistUp || (BorderComponent[0] || BorderComponent[1] || BorderComponent[2] || BorderComponent[3]) || CornerSet;
			}

			if (!ComponentDataExist(ComponentIndexXY))
			{
				continue;
			}

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;

							if (LayerName != NAME_None)
							{
								// Find the input data corresponding to this vertex
								BYTE Weight;
								if( WeightmapTexture )
								{
									// Find the texture data corresponding to this vertex
									Weight = GetWeightMapData(Component, LayerName, (SubsectionSizeQuads+1) * SubIndexX + SubX, (SubsectionSizeQuads+1) * SubIndexY + SubY, WeightmapChannelOffset, WeightmapTexture, WeightmapTextureData );
									StoreData.Store(LandscapeX, LandscapeY, Weight);
								}
								else
								{
									// Find the texture data corresponding to this vertex
									BYTE Value[4] = {0, 0, 0, 0};
									INT Dist[4] = {INT_MAX, INT_MAX, INT_MAX, INT_MAX};
									FLOAT ValueX = 0.f, ValueY = 0.f;
									UBOOL Exist[4] = {FALSE, FALSE, FALSE, FALSE};

									// Use data already stored for 0, 2
									if (ExistLeft)
									{
										Value[0] = StoreData.Load( ComponentIndexX*ComponentSizeQuads, LandscapeY);
										Dist[0] = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
										Exist[0] = TRUE;
									}
									else if (BorderComponent[0] && NeighborWeightmapTexture[0])
									{
										Value[0] = GetWeightMapData(BorderComponent[0], LayerName, EdgeCoord, (SubsectionSizeQuads+1) * SubIndexY + SubY, NeighborWeightmapChannelOffset[0], NeighborWeightmapTexture[0], NeighborWeightmapTextureData[0]);
										Dist[0] = LandscapeX - (BorderComponent[0]->SectionBaseX + ComponentSizeQuads);
										Exist[0] = TRUE;
									}
									else 
									{
										if ((CornerSet & 1) && (CornerSet & 1 << 2))
										{
											INT Dist1 = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
											INT Dist2 = ((ComponentIndexY+1)*ComponentSizeQuads) - LandscapeY;
											Value[0] = (FLOAT)(Dist2 * CornerValues[0] + Dist1 * CornerValues[2]) / (Dist1 + Dist2);
											Dist[0] = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
											Exist[0] = TRUE;
										}
									}

									if (BorderComponent[1] && NeighborWeightmapTexture[1])
									{
										Value[1] = GetWeightMapData(BorderComponent[1], LayerName, 0, (SubsectionSizeQuads+1) * SubIndexY + SubY, NeighborWeightmapChannelOffset[1], NeighborWeightmapTexture[1], NeighborWeightmapTextureData[1]);
										Dist[1] = (BorderComponent[1]->SectionBaseX) - LandscapeX;
										Exist[1] = TRUE;
									}
									else
									{
										if ((CornerSet & 1 << 1) && (CornerSet & 1 << 3))
										{
											INT Dist1 = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
											INT Dist2 = ((ComponentIndexY+1)*ComponentSizeQuads) - LandscapeY;
											Value[1] = (FLOAT)(Dist2 * CornerValues[1] + Dist1 * CornerValues[3]) / (Dist1 + Dist2);
											Dist[1] = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
											Exist[1] = TRUE;
										}
									}

									if (ExistUp)
									{
										Value[2] = StoreData.Load( LandscapeX, ComponentIndexY*ComponentSizeQuads);
										Dist[2] = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
										Exist[2] = TRUE;
									}
									else if (BorderComponent[2] && NeighborWeightmapTexture[2])
									{
										Value[2] = GetWeightMapData(BorderComponent[2], LayerName, (SubsectionSizeQuads+1) * SubIndexX + SubX, EdgeCoord, NeighborWeightmapChannelOffset[2], NeighborWeightmapTexture[2], NeighborWeightmapTextureData[2]);
										Dist[2] = LandscapeY - (BorderComponent[2]->SectionBaseY + ComponentSizeQuads);
										Exist[2] = TRUE;
									}
									else
									{
										if ((CornerSet & 1) && (CornerSet & 1 << 1))
										{
											INT Dist1 = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
											INT Dist2 = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
											Value[2] = (FLOAT)(Dist2 * CornerValues[0] + Dist1 * CornerValues[1]) / (Dist1 + Dist2);
											Dist[2] = LandscapeY - (ComponentIndexY*ComponentSizeQuads);
											Exist[2] = TRUE;
										}
									}

									if (BorderComponent[3] && NeighborWeightmapTexture[3])
									{
										Value[3] = GetWeightMapData(BorderComponent[3], LayerName, (SubsectionSizeQuads+1) * SubIndexX + SubX, 0, NeighborWeightmapChannelOffset[3], NeighborWeightmapTexture[3], NeighborWeightmapTextureData[3]);
										Dist[3] = (BorderComponent[3]->SectionBaseY) - LandscapeY;
										Exist[3] = TRUE;
									}
									else
									{
										if ((CornerSet & 1 << 2) && (CornerSet & 1 << 3))
										{
											INT Dist1 = LandscapeX - (ComponentIndexX*ComponentSizeQuads);
											INT Dist2 = (ComponentIndexX+1)*ComponentSizeQuads - LandscapeX;
											Value[3] = (FLOAT)(Dist2 * CornerValues[2] + Dist1 * CornerValues[3]) / (Dist1 + Dist2);
											Dist[3] = (ComponentIndexY+1)*ComponentSizeQuads - LandscapeY;
											Exist[3] = TRUE;
										}
									}

									CalcInterpValue<BYTE>(Dist, Exist, Value, ValueX, ValueY);

									BYTE FinalValue = 0; // Default Value
									if ( (Exist[0] || Exist[1]) && (Exist[2] || Exist[3]) )
									{
										FinalValue = CalcValueFromValueXY<BYTE>(Dist, ValueX, ValueY, CornerSet, CornerValues);
									}
									else if ( (Exist[0] || Exist[1]) )
									{
										FinalValue = ValueX;
									}
									else if ( (Exist[2] || Exist[3]) )
									{
										FinalValue = ValueY;
									}

									Weight = FinalValue;
								}

								StoreData.Store(LandscapeX, LandscapeY, Weight);
							}
							else // Whole weight map case... no interpolation now...
							{
								StoreData.PreInit(LandscapeInfo->LayerInfoMap.Num());
								for( INT LayerIdx=0;LayerIdx<Component->WeightmapLayerAllocations.Num();LayerIdx++ )
								{
									INT Idx = Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureIndex;
									UTexture2D* WeightmapTexture = Component->WeightmapTextures(Idx);
									BYTE* WeightmapTextureData = (BYTE*)TexDataInfos(Idx)->GetMipData(0);
									BYTE WeightmapChannelOffset = ChannelOffsets[Component->WeightmapLayerAllocations(LayerIdx).WeightmapTextureChannel];

									// Find the texture data corresponding to this vertex
									INT SizeU = WeightmapTexture->SizeX;
									INT SizeV = WeightmapTexture->SizeY;
									INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
									INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

									INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
									INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;

									BYTE Weight = WeightmapTextureData[ 4 * (TexX + TexY * SizeU) + WeightmapChannelOffset ];

									// Find in index in LayerName
									{
										TMap<FName, FLandscapeLayerStruct*>::TIterator It(LandscapeInfo->LayerInfoMap);
										for ( INT NameIdx = 0; NameIdx < LandscapeInfo->LayerInfoMap.Num() && It; NameIdx++, ++It )
										{
											ULandscapeLayerInfoObject* LayerInfo = It.Value() ? It.Value()->LayerInfoObj : NULL ;
											if (LayerInfo && LayerInfo->LayerName == Component->WeightmapLayerAllocations(LayerIdx).LayerName)
											{
												StoreData.Store(LandscapeX, LandscapeY, Weight, NameIdx);
												break;
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}
	}

	if (bHasMissingValue)
	{
		CalcMissingValues<BYTE, TStoreData>( X1, X2, Y1, Y2,
			ComponentIndexX1, ComponentIndexX2, ComponentIndexY1, ComponentIndexY2, 
			ComponentSizeX, ComponentSizeY, CornerValues,
			NoBorderY1, NoBorderY2, ComponentDataExist, StoreData );
		// Update valid region
		ValidX1 = Max<INT>(X1, ValidX1);
		ValidX2 = Min<INT>(X2, ValidX2);
		ValidY1 = Max<INT>(Y1, ValidY1);
		ValidY2 = Min<INT>(Y2, ValidY2);
	}
	else
	{
		ValidX1 = X1;
		ValidX2 = X2;
		ValidY1 = Y1;
		ValidY2 = Y2;
	}
}

void FLandscapeEditDataInterface::GetWeightData(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, BYTE* Data, INT Stride)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}
	TArrayStoreData<BYTE> ArrayStoreData(X1, Y1, Data, Stride);
	GetWeightDataTempl(LayerName, X1, Y1, X2, Y2, ArrayStoreData);
}

void FLandscapeEditDataInterface::GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, BYTE* Data, INT Stride)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}
	TArrayStoreData<BYTE> ArrayStoreData(X1, Y1, Data, Stride);
	GetWeightDataTemplFast(LayerName, X1, Y1, X2, Y2, ArrayStoreData);
}

void FLandscapeEditDataInterface::GetWeightData(FName LayerName, INT& X1, INT& Y1, INT& X2, INT& Y2, TMap<QWORD, BYTE>& SparseData)
{
	TSparseStoreData<BYTE> SparseStoreData(SparseData);
	GetWeightDataTempl(LayerName, X1, Y1, X2, Y2, SparseStoreData);
}

void FLandscapeEditDataInterface::GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, BYTE>& SparseData)
{
	TSparseStoreData<BYTE> SparseStoreData(SparseData);
	GetWeightDataTemplFast(LayerName, X1, Y1, X2, Y2, SparseStoreData);
}

void FLandscapeEditDataInterface::GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TArray<BYTE>* Data, INT Stride)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}
	TArrayStoreData<TArray<BYTE>> ArrayStoreData(X1, Y1, Data, Stride);
	GetWeightDataTemplFast(LayerName, X1, Y1, X2, Y2, ArrayStoreData);
}

void FLandscapeEditDataInterface::GetWeightDataFast(FName LayerName, const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, TArray<BYTE>>& SparseData)
{
	TSparseStoreData<TArray<BYTE>> SparseStoreData(SparseData);
	GetWeightDataTemplFast(LayerName, X1, Y1, X2, Y2, SparseStoreData);
}

FLandscapeTextureDataInfo* FLandscapeEditDataInterface::GetTextureDataInfo(UTexture2D* Texture)
{
	FLandscapeTextureDataInfo* Result = TextureDataMap.FindRef(Texture);
	if( !Result )
	{
		Result = TextureDataMap.Set(Texture, new FLandscapeTextureDataInfo(Texture));
	}
	return Result;
}

void FLandscapeEditDataInterface::CopyTextureChannel(UTexture2D* Dest, INT DestChannel, UTexture2D* Src, INT SrcChannel)
{
	FLandscapeTextureDataInfo* DestDataInfo = GetTextureDataInfo(Dest);
	FLandscapeTextureDataInfo* SrcDataInfo = GetTextureDataInfo(Src);
	INT MipSize = Dest->SizeX;
	check(Dest->SizeX == Dest->SizeY && Src->SizeX == Dest->SizeX);

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	for( INT MipIdx=0;MipIdx<DestDataInfo->NumMips();MipIdx++ )
	{
		BYTE* DestTextureData = (BYTE*)DestDataInfo->GetMipData(MipIdx) + ChannelOffsets[DestChannel];
		BYTE* SrcTextureData = (BYTE*)SrcDataInfo->GetMipData(MipIdx) + ChannelOffsets[SrcChannel];

		for( INT i=0;i<Square(MipSize);i++ )
		{
			DestTextureData[i*4] = SrcTextureData[i*4];
		}

		DestDataInfo->AddMipUpdateRegion(MipIdx, 0, 0, MipSize-1, MipSize-1);
		MipSize >>= 1;
	}
}

void FLandscapeEditDataInterface::ZeroTextureChannel(UTexture2D* Dest, INT DestChannel)
{
	FLandscapeTextureDataInfo* DestDataInfo = GetTextureDataInfo(Dest);
	INT MipSize = Dest->SizeX;
	check(Dest->SizeX == Dest->SizeY);

	// Channel remapping
	INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

	for( INT MipIdx=0;MipIdx<DestDataInfo->NumMips();MipIdx++ )
	{
		BYTE* DestTextureData = (BYTE*)DestDataInfo->GetMipData(MipIdx) + ChannelOffsets[DestChannel];

		for( INT i=0;i<Square(MipSize);i++ )
		{
			DestTextureData[i*4] = 0;
		}

		DestDataInfo->AddMipUpdateRegion(MipIdx, 0, 0, MipSize-1, MipSize-1);
		MipSize >>= 1;
	}
}

void FLandscapeEditDataInterface::ZeroTexture(UTexture2D* Dest)
{
	FLandscapeTextureDataInfo* DestDataInfo = GetTextureDataInfo(Dest);
	INT MipSize = Dest->SizeX;
	check(Dest->SizeX == Dest->SizeY);

	for( INT MipIdx=0;MipIdx<DestDataInfo->NumMips();MipIdx++ )
	{
		BYTE* DestTextureData = (BYTE*)DestDataInfo->GetMipData(MipIdx);

		for( INT i=0;i<Square(MipSize);i++ )
		{
			DestTextureData[i] = 0;
		}

		DestDataInfo->AddMipUpdateRegion(MipIdx, 0, 0, MipSize-1, MipSize-1);
		MipSize >>= 1;
	}
}

template<typename TStoreData>
void FLandscapeEditDataInterface::GetSelectDataTempl(const INT X1, const INT Y1, const INT X2, const INT Y2, TStoreData& StoreData)
{
	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndices(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{		
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads));

			FLandscapeTextureDataInfo* TexDataInfo = NULL;
			BYTE* SelectTextureData = NULL;
			if( Component && Component->EditToolRenderData && Component->EditToolRenderData->DataTexture )
			{
				TexDataInfo = GetTextureDataInfo(Component->EditToolRenderData->DataTexture);
				SelectTextureData = (BYTE*)TexDataInfo->GetMipData(0);
			}

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;

							// Find the input data corresponding to this vertex
							if( Component && SelectTextureData )
							{
								// Find the texture data corresponding to this vertex
								INT SizeU = Component->EditToolRenderData->DataTexture->SizeX;
								INT SizeV = Component->EditToolRenderData->DataTexture->SizeY;
								INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
								INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

								INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
								INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
								BYTE& TexData = SelectTextureData[ TexX + TexY * SizeU ];

								StoreData.Store(LandscapeX, LandscapeY, TexData);
							}
							else
							{
								StoreData.Store(LandscapeX, LandscapeY, 0);
							}

						}
					}
				}
			}
		}
	}
}

void FLandscapeEditDataInterface::GetSelectData(const INT X1, const INT Y1, const INT X2, const INT Y2, TMap<QWORD, BYTE>& SparseData)
{
	TSparseStoreData<BYTE> SparseStoreData(SparseData);
	GetSelectDataTempl(X1, Y1, X2, Y2, SparseStoreData);
}

void FLandscapeEditDataInterface::GetSelectData(const INT X1, const INT Y1, const INT X2, const INT Y2, BYTE* Data, INT Stride)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}
	TArrayStoreData<BYTE> ArrayStoreData(X1, Y1, Data, Stride);
	GetSelectDataTempl(X1, Y1, X2, Y2, ArrayStoreData);
}

void FLandscapeEditDataInterface::SetSelectData(INT X1, INT Y1, INT X2, INT Y2, const BYTE* Data, INT Stride)
{
	if( Stride==0 )
	{
		Stride = (1+X2-X1);
	}

	check(ComponentSizeQuads > 0);
	// Find component range for this block of data
	INT ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2;
	ALandscape::CalcComponentIndicesOverlap(X1, Y1, X2, Y2, ComponentSizeQuads, ComponentIndexX1, ComponentIndexY1, ComponentIndexX2, ComponentIndexY2);

	for( INT ComponentIndexY=ComponentIndexY1;ComponentIndexY<=ComponentIndexY2;ComponentIndexY++ )
	{
		for( INT ComponentIndexX=ComponentIndexX1;ComponentIndexX<=ComponentIndexX2;ComponentIndexX++ )
		{	
			QWORD ComponentKey = ALandscape::MakeKey(ComponentIndexX*ComponentSizeQuads,ComponentIndexY*ComponentSizeQuads);
			ULandscapeComponent* Component = LandscapeInfo->XYtoComponentMap.FindRef(ComponentKey);

			UTexture2D* DataTexture = NULL;
			// if NULL, it was painted away
			if( Component==NULL || Component->EditToolRenderData==NULL)
			{
				continue;
			}
			else if (Component->EditToolRenderData->DataTexture==NULL)
			{
				//FlushRenderingCommands();
				// Construct Texture...
				INT WeightmapSize = (Component->SubsectionSizeQuads+1) * Component->NumSubsections;
				DataTexture = ConstructObject<UTexture2D>(UTexture2D::StaticClass(), Component->GetOutermost(), NAME_None, RF_Public|RF_Standalone);
				DataTexture->Init(WeightmapSize,WeightmapSize,PF_G8);
				DataTexture->SRGB = FALSE;
				DataTexture->CompressionNone = TRUE;
				DataTexture->MipGenSettings = TMGS_LeaveExistingMips;
				DataTexture->AddressX = TA_Clamp;
				DataTexture->AddressY = TA_Clamp;
				DataTexture->LODGroup = TEXTUREGROUP_Terrain_Weightmap;
				// Alloc dummy mips
				ULandscapeComponent::CreateEmptyTextureMips(DataTexture, TRUE);
				FTexture2DMipMap* WeightMipMap = &DataTexture->Mips(0);
				appMemzero(WeightMipMap->Data.Lock(LOCK_READ_WRITE), WeightmapSize*WeightmapSize);
				WeightMipMap->Data.Unlock();
				DataTexture->UpdateResource();

				//FlushRenderingCommands();
				ZeroTexture(DataTexture);
				FLandscapeTextureDataInfo* TexDataInfo = GetTextureDataInfo(DataTexture);
				INT NumMips = DataTexture->Mips.Num();
				TArray<BYTE*> TextureMipData(NumMips);
				for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
				{
					TextureMipData(MipIdx) = (BYTE*)TexDataInfo->GetMipData(MipIdx);
				}
				ULandscapeComponent::UpdateDataMips(ComponentNumSubsections, SubsectionSizeQuads, DataTexture, TextureMipData, 0, 0, MAXINT, MAXINT, TexDataInfo);
				
				ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
					UpdateEditToolRenderDataDataTexture,
					FLandscapeEditToolRenderData*, LandscapeEditToolRenderData, Component->EditToolRenderData,
					UTexture2D*, InDataTexture, DataTexture,
				{
					LandscapeEditToolRenderData->DataTexture  = InDataTexture;
				});			
			}
			else
			{
				DataTexture = Component->EditToolRenderData->DataTexture;
			}

			FLandscapeTextureDataInfo* TexDataInfo = GetTextureDataInfo(DataTexture);
			BYTE* SelectTextureData = (BYTE*)TexDataInfo->GetMipData(0);

			// Find the texture data corresponding to this vertex
			INT SizeU = DataTexture->SizeX;
			INT SizeV = DataTexture->SizeY;
			INT WeightmapOffsetX = Component->WeightmapScaleBias.Z * (FLOAT)SizeU;
			INT WeightmapOffsetY = Component->WeightmapScaleBias.W * (FLOAT)SizeV;

			// Find coordinates of box that lies inside component
			INT ComponentX1 = Clamp<INT>(X1-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY1 = Clamp<INT>(Y1-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentX2 = Clamp<INT>(X2-ComponentIndexX*ComponentSizeQuads, 0, ComponentSizeQuads);
			INT ComponentY2 = Clamp<INT>(Y2-ComponentIndexY*ComponentSizeQuads, 0, ComponentSizeQuads);

			// Find subsection range for this box
			INT SubIndexX1 = Clamp<INT>((ComponentX1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);	// -1 because we need to pick up vertices shared between subsections
			INT SubIndexY1 = Clamp<INT>((ComponentY1-1) / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexX2 = Clamp<INT>(ComponentX2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);
			INT SubIndexY2 = Clamp<INT>(ComponentY2 / SubsectionSizeQuads,0,ComponentNumSubsections-1);

			for( INT SubIndexY=SubIndexY1;SubIndexY<=SubIndexY2;SubIndexY++ )
			{
				for( INT SubIndexX=SubIndexX1;SubIndexX<=SubIndexX2;SubIndexX++ )
				{
					// Find coordinates of box that lies inside subsection
					INT SubX1 = Clamp<INT>(ComponentX1-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY1 = Clamp<INT>(ComponentY1-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);
					INT SubX2 = Clamp<INT>(ComponentX2-SubsectionSizeQuads*SubIndexX, 0, SubsectionSizeQuads);
					INT SubY2 = Clamp<INT>(ComponentY2-SubsectionSizeQuads*SubIndexY, 0, SubsectionSizeQuads);

					// Update texture data for the box that lies inside subsection
					for( INT SubY=SubY1;SubY<=SubY2;SubY++ )
					{
						for( INT SubX=SubX1;SubX<=SubX2;SubX++ )
						{
							INT LandscapeX = SubIndexX*SubsectionSizeQuads + ComponentIndexX*ComponentSizeQuads + SubX;
							INT LandscapeY = SubIndexY*SubsectionSizeQuads + ComponentIndexY*ComponentSizeQuads + SubY;
							checkSlow( LandscapeX >= X1 && LandscapeX <= X2 );
							checkSlow( LandscapeY >= Y1 && LandscapeY <= Y2 );

							// Find the input data corresponding to this vertex
							INT DataIndex = (LandscapeX-X1) + Stride * (LandscapeY-Y1);
							const BYTE& Value = Data[DataIndex];

							INT TexX = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX;
							INT TexY = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY;
							BYTE& TexData = SelectTextureData[ TexX + TexY * SizeU ];

							TexData = Value;
						}
					}

					// Record the areas of the texture we need to re-upload
					INT TexX1 = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX1;
					INT TexY1 = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY1;
					INT TexX2 = WeightmapOffsetX + (SubsectionSizeQuads+1) * SubIndexX + SubX2;
					INT TexY2 = WeightmapOffsetY + (SubsectionSizeQuads+1) * SubIndexY + SubY2;
					TexDataInfo->AddMipUpdateRegion(0,TexX1,TexY1,TexX2,TexY2);
				}
			}
			// Update mipmaps
			INT NumMips = DataTexture->Mips.Num();
			TArray<BYTE*> TextureMipData(NumMips);
			for( INT MipIdx=0;MipIdx<NumMips;MipIdx++ )
			{
				TextureMipData(MipIdx) = (BYTE*)TexDataInfo->GetMipData(MipIdx);
			}
			ULandscapeComponent::UpdateDataMips(ComponentNumSubsections, SubsectionSizeQuads, DataTexture, TextureMipData, ComponentX1, ComponentY1, ComponentX2, ComponentY2, TexDataInfo);
		}
	}
}

//
// FLandscapeTextureDataInfo
//

FLandscapeTextureDataInfo::FLandscapeTextureDataInfo(UTexture2D* InTexture)
:	Texture(InTexture)
{
	MipInfo.AddZeroed(Texture->Mips.Num());
	for( INT MipIdx=0;MipIdx<Texture->Mips.Num();MipIdx++ )
	{
		Texture->Mips(MipIdx).Data.ForceBulkDataResident();
	}

	if( Texture->bHasBeenLoadedFromPersistentArchive )
	{
		// We want to prevent the texture from being streamed again as we've just edited the data
		Texture->bHasBeenLoadedFromPersistentArchive = FALSE;

		// Update the entire resource, which will update all mip levels.
		Texture->UpdateResource();
	}

	Texture->SetFlags(RF_Transactional);
	Texture->Modify();
}

UBOOL FLandscapeTextureDataInfo::UpdateTextureData()
{
	UBOOL bNeedToWaitForUpdate = FALSE;

	INT DataSize = sizeof(FColor);
	if (Texture->Format == PF_G8)
	{
		DataSize = sizeof(BYTE);
	}

	for( INT i=0;i<MipInfo.Num();i++ )
	{
		if( MipInfo(i).MipData && MipInfo(i).MipUpdateRegions.Num()>0 )
		{
			Texture->UpdateTextureRegions( i, MipInfo(i).MipUpdateRegions.Num(), &MipInfo(i).MipUpdateRegions(0), ((Texture->SizeX)>>i)*DataSize, DataSize, (BYTE*)MipInfo(i).MipData, FALSE );
			bNeedToWaitForUpdate = TRUE;
		}
	}

	return bNeedToWaitForUpdate;
}

FLandscapeTextureDataInfo::~FLandscapeTextureDataInfo()
{
	// Unlock any mips still locked.
	for( INT i=0;i<MipInfo.Num();i++ )
	{
		if( MipInfo(i).MipData )
		{
			if( MipInfo(i).MipData )
			{
				Texture->Mips(i).Data.Unlock();
				MipInfo(i).MipData = NULL;
			}
		}
	}
	Texture->ClearFlags(RF_Transactional);
}

#endif // WITH_EDITOR