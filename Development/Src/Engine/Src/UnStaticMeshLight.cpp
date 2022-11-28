/*=============================================================================
	UnStaticMeshLight.cpp: Static mesh lighting code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"
#include "UnRaster.h"

IMPLEMENT_CLASS(UShadowMap);

/**
 * Serializes a FStaticMeshLight
 *
 * @param	Ar	FArchive the object is serialized with
 * @param	L	FStaticMeshLight to be serialized
 *
 * @return	FArchive FStaticMeshLight has been serialized to/from.
 */
FArchive& operator<<(FArchive& Ar,FStaticMeshLight& L)
{
	Ar << L.Light;
	Ar << L.Visibility;
	return Ar;
}

//
//	FStaticMeshLightingRasterizer - Rasterizes static mesh lighting into a lightmap.
//

struct FStaticMeshLightingRasterizer : FTriangleRasterizer<FVector>
{
	UStaticMeshComponent*	Component;
	FPlane					LightPosition;
	TArray<UINT>			Visibility;
	TArray<UINT>			Coverage;
	UINT					SizeX,
							SizeY;

	// Constructor.

	FStaticMeshLightingRasterizer(UStaticMeshComponent* InComponent,const FPlane& InLightPosition,UINT InSizeX,UINT InSizeY):
		Component(InComponent),
		LightPosition(InLightPosition),
		SizeX(InSizeX),
		SizeY(InSizeY)
	{
		Visibility.AddZeroed(SizeX * SizeY);
		Coverage.AddZeroed(SizeX * SizeY);
	}

	// ProcessPixel

	virtual void ProcessPixel(INT X,INT Y,FVector Vertex)
	{
		if(X < 0 || (UINT)X >= SizeX || Y < 0 || (UINT)Y >= SizeY)
			return;

		Coverage(Y * SizeX + X)++;

		FVector			LightVector = (FVector)LightPosition - LightPosition.W * Vertex;
		FCheckResult	Hit(1);

		if(!Component->Owner->XLevel->SingleLineCheck(Hit,NULL,Vertex + LightVector,Vertex + LightVector.SafeNormal() * 0.25f,TRACE_Level|TRACE_Actors|TRACE_ShadowCast))
			return;
		
		if(!Component->LineCheck(Hit,Vertex + LightVector,Vertex + LightVector.SafeNormal() * 0.25f,FVector(0,0,0),TRACE_Level|TRACE_Actors|TRACE_ShadowCast))
			return;

		Visibility(Y * SizeX + X)++;
	}
};

//
//	UStaticMeshComponent::CacheLighting
//

void UStaticMeshComponent::CacheLighting()
{
	if(!HasStaticShadowing() || !StaticMesh)
	{
		StaticLights.Empty();
		StaticLightMaps.Empty();
		IgnoreLights.Empty();
		return;
	}

	FComponentRecreateContext	RecreateContext(this);

	StaticLights.Empty();
	StaticLightMaps.Empty();
	IgnoreLights.Empty();

	TArray<ULightComponent*>	RelevantLights;
	Scene->GetRelevantLights(this,RelevantLights);
	for(UINT LightIndex = 0;LightIndex < (UINT)RelevantLights.Num();LightIndex++)
	{
		ULightComponent*	Light = RelevantLights(LightIndex);

		if(!Light->HasStaticShadowing() || !Light->CastShadows)
			continue;

		FPlane	LightPosition = Light->GetPosition();

		GWarn->BeginSlowTask(*FString::Printf(TEXT("Raytracing %s -> %s"),*Light->GetPathName(),*GetPathName()),1);

		if(StaticMesh->LightMapResolution > 0 && StaticMesh->LightMapCoordinateIndex >= 0 && StaticMesh->LightMapCoordinateIndex < StaticMesh->UVBuffers.Num())
		{
			// Sample the lightmap.

			FStaticMeshLightingRasterizer	LightMapRasterizer(this,LightPosition,(UINT)StaticMesh->LightMapResolution,(UINT)StaticMesh->LightMapResolution);

			for(UINT TriangleIndex = 0;TriangleIndex < (UINT)StaticMesh->IndexBuffer.Indices.Num() / 3;TriangleIndex++)
			{
				GWarn->StatusUpdatef(TriangleIndex,StaticMesh->IndexBuffer.Indices.Num() / 3,TEXT("Raytracing %s -> %s"),*Light->GetPathName(),*GetPathName());

				FVector		Vertices[3];
				FVector2D	TexCoords[3];
				for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					INT	Index = StaticMesh->IndexBuffer.Indices(TriangleIndex * 3 + VertexIndex);
					Vertices[VertexIndex] = LocalToWorld.TransformFVector(StaticMesh->Vertices(Index).Position);
					TexCoords[VertexIndex] = StaticMesh->UVBuffers(StaticMesh->LightMapCoordinateIndex).UVs(Index) * StaticMesh->LightMapResolution - FVector2D(0.5f,0.5f);
				}

				LightMapRasterizer.DrawTriangle(Vertices[0],Vertices[1],Vertices[2],TexCoords[0],TexCoords[1],TexCoords[2]);
			}

			UBOOL			Discard			= 1;

			// Create new lightmap texture, initialize it and retrieve coverage offset into source art.
			UShadowMap*		StaticLightMap	= ConstructObject<UShadowMap>( UShadowMap::StaticClass(), GetOuter(), NAME_None );
			DWORD			CoverageOffset	= StaticLightMap->Init( StaticMesh->LightMapResolution, Light );
			StaticLightMap->PostLoad();

			// Filter the lightmap.

			UINT	Filter[5][5] =
			{
				{ 1, 1, 2, 1, 1 },
				{ 1, 2, 4, 2, 1 },
				{ 2, 4, 8, 4, 2 },
				{ 1, 2, 4, 2, 1 },
				{ 1, 1, 2, 1, 1 },
			};

			for(UINT Y = 0;Y < LightMapRasterizer.SizeY;Y++)
			{
				for(UINT X = 0;X < LightMapRasterizer.SizeX;X++)
				{
					UINT	Visibility	= 0,
							Coverage	= 0;

					for(INT FilterY = -2;FilterY <= 2;FilterY++)
					{
						if(Y + FilterY < 0 || Y + FilterY >= LightMapRasterizer.SizeY)
							continue;

						for(INT FilterX = -2;FilterX <= 2;FilterX++)
						{
							if(X + FilterX < 0 || X + FilterX >= LightMapRasterizer.SizeX)
								continue;

							Visibility	+= LightMapRasterizer.Visibility((Y + FilterY) * LightMapRasterizer.SizeX + X + FilterX);
							Coverage	+= LightMapRasterizer.Coverage((Y + FilterY) * LightMapRasterizer.SizeX + X + FilterX);
						}
					}

					if(Coverage > 0)
					{
						if(Visibility > 0)
							Discard = 0;

						StaticLightMap->SourceArt(Y * StaticLightMap->SizeX + X) = (BYTE)Clamp<INT>(appTrunc((FLOAT)Visibility / (FLOAT)Coverage * 255.0f),0,255);
					}

					// We treat coverage as binary information so we can later sum over it to calculate the amount of pixels covered.
					StaticLightMap->SourceArt(Y * StaticLightMap->SizeX + X + CoverageOffset) = Coverage ? 1 : 0;
				}
			}

			if(Discard)
			{
				StaticLightMap = NULL;
				IgnoreLights.AddItem(Light);
			}
			else
			{
				StaticLightMaps.AddItem( StaticLightMap );
				StaticLightMap->CreateFromSourceArt( TRUE );
			}
		}
		else
		{
			FStaticMeshLight*	StaticLight = new(StaticLights) FStaticMeshLight;
			TArray<FLOAT>		Denominators;
			UBOOL				Discard = 1;

			StaticLight->Light = Light;
			StaticLight->Visibility.AddZeroed(StaticMesh->Vertices.Num());

			Denominators.AddZeroed(StaticMesh->Vertices.Num());

			for(UINT TriangleIndex = 0;TriangleIndex < (UINT)StaticMesh->IndexBuffer.Indices.Num() / 3;TriangleIndex++)
			{
				GWarn->StatusUpdatef(TriangleIndex,StaticMesh->IndexBuffer.Indices.Num() / 3,TEXT("Raytracing %s -> %s"),*Light->GetPathName(),*GetPathName());

				FVector	Vertices[3];
				FVector	LocalVertices[3];
				for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					LocalVertices[VertexIndex] = StaticMesh->Vertices(StaticMesh->IndexBuffer.Indices(TriangleIndex * 3 + VertexIndex)).Position;
					Vertices[VertexIndex] = LocalToWorld.TransformFVector(LocalVertices[VertexIndex]);
				}

#define SUBDIVISION_SIZE	16.0f

				INT		NumSubdivisionsS = Clamp<INT>((Vertices[1] - Vertices[0]).Size() / SUBDIVISION_SIZE,2,16),
						NumSubdivisionsT = Clamp<INT>((Vertices[2] - Vertices[0]).Size() / SUBDIVISION_SIZE,2,16);
				FLOAT	Numerators[3] = { 0, 0, 0 },
						VertexDenominators[3] = { 0, 0, 0 };
				for(INT S = 0;S < NumSubdivisionsS;S++)
				{
					for(INT T = 0;T < NumSubdivisionsT - S * ((Vertices[2] - Vertices[0]).Size() / (Vertices[1] - Vertices[0]).Size());T++)
					{
						FVector			SamplePoint = Vertices[0] + (Vertices[1] - Vertices[0]) * ((S + 0.5f) / NumSubdivisionsS) + (Vertices[2] - Vertices[0]) * ((T + 0.5f) / NumSubdivisionsT),
										LightVector = (FVector)LightPosition - SamplePoint * LightPosition.W;
						FCheckResult	Hit(0);

						FLOAT	SFrac = ((S + 0.5f) / NumSubdivisionsS),
								TFrac = ((T + 0.5f) / NumSubdivisionsT),
								UFrac = 1.0f - SFrac - TFrac;

						if(Owner->XLevel->SingleLineCheck(Hit,NULL,SamplePoint + LightVector.SafeNormal() * 0.25f,SamplePoint + LightVector,TRACE_Level|TRACE_Actors|TRACE_ShadowCast) && LineCheck(Hit,SamplePoint + LightVector.SafeNormal() * 0.25f,SamplePoint + LightVector,FVector(0,0,0),TRACE_Level|TRACE_Actors))
						{
							Numerators[0] += UFrac;
							Numerators[1] += SFrac;
							Numerators[2] += TFrac;
							Discard = 0;
						}

						VertexDenominators[0] += UFrac;
						VertexDenominators[1] += SFrac;
						VertexDenominators[2] += TFrac;
					}
				}

				for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
				{
					StaticLight->Visibility(StaticMesh->IndexBuffer.Indices(TriangleIndex * 3 + VertexIndex)) += Numerators[VertexIndex];
					Denominators(StaticMesh->IndexBuffer.Indices(TriangleIndex * 3 + VertexIndex)) += VertexDenominators[VertexIndex];
				}
			}

			for(UINT VertexIndex = 0;VertexIndex < (UINT)StaticMesh->Vertices.Num();VertexIndex++)
				if(Denominators(VertexIndex) > 0.0f)
					StaticLight->Visibility(VertexIndex) /= Denominators(VertexIndex);

			// Detach the lazy loader so Load/Unload won't clobber new data.
			StaticLight->Visibility.Detach();

			if(Discard)
			{
				StaticLight = NULL;
				StaticLights.Remove(StaticLights.Num() - 1);
				IgnoreLights.AddItem(Light);
			}
		}

		GWarn->EndSlowTask();
	}
}

void UStaticMeshComponent::InvalidateLightingCache(ULightComponent* Light)
{
	for(UINT LightIndex = 0;LightIndex < (UINT)StaticLights.Num();LightIndex++)
	{
		if(StaticLights(LightIndex).Light == Light)
		{
			FComponentRecreateContext RecreateContext(this);
			StaticLights.Remove(LightIndex);
			break;
		}
	}
	for(UINT LightIndex = 0;LightIndex < (UINT)StaticLightMaps.Num();LightIndex++)
	{	
		if(StaticLightMaps(LightIndex)->Light == Light)
		{
			FComponentRecreateContext RecreateContext(this);
			StaticLightMaps.Remove(LightIndex);
			break;
		}
	}
	if(IgnoreLights.FindItemIndex(Light) != INDEX_NONE)
	{
		FComponentRecreateContext RecreateContext(this);
		IgnoreLights.RemoveItem(Light);
	}
}

void UStaticMeshComponent::InvalidateLightingCache()
{
	FComponentRecreateContext RecreateContext(this);
	StaticLights.Empty();
	StaticLightMaps.Empty();
	IgnoreLights.Empty();
}