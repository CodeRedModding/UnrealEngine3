/*=============================================================================
	UnModelLight.cpp: Unreal model lighting.
	Copyright 1997-2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_RESOURCE_TYPE(FStaticModelLightTexture);

//
//	Definitions.
//

#define LIGHTMAP_MAX_WIDTH			2048
#define LIGHTMAP_MAX_HEIGHT			2048

#define LIGHTMAP_TEXTURE_WIDTH		2048
#define LIGHTMAP_TEXTURE_HEIGHT		2048

//
//	FStaticModelLightSurface::FStaticModelLightSurface
//

FStaticModelLightSurface::FStaticModelLightSurface(
		UINT InSurfaceIndex,
		const TArray<INT>& InNodes,
		const FVector& InLightMapX,
		const FVector& InLightMapY,
		const FPlane& Plane,
		FLOAT MinX,
		FLOAT MinY,
		FLOAT MaxX,
		FLOAT MaxY,
		UINT InSizeX,
		UINT InSizeY
		):
	SurfaceIndex(InSurfaceIndex),
	Nodes(InNodes),
	LightMapX(InLightMapX),
	LightMapY(InLightMapY),
	Min(MinX,MinY),
	Max(MaxX,MaxY),
	BaseX(0),
	BaseY(0),
	SizeX(InSizeX),
	SizeY(InSizeY)
{
	LightMapBase = LightMapX * MinX + LightMapY * MinY + (FVector)Plane * Plane.W;
}

//
//	FStaticModelLightSurface::WorldToTexture
//

FMatrix FStaticModelLightSurface::WorldToTexture() const
{
	FVector	LightMapZ = (LightMapX ^ LightMapY).SafeNormal();
	FLOAT	InvScaleX = 1.0f / (Max.X - Min.X),
			InvScaleY = 1.0f / (Max.Y - Min.Y);
	return FMatrix(
			FPlane(LightMapX.X * InvScaleX,	LightMapY.X * InvScaleY,	LightMapZ.X,				0),
			FPlane(LightMapX.Y * InvScaleX,	LightMapY.Y * InvScaleY,	LightMapZ.Y,				0),
			FPlane(LightMapX.Z * InvScaleX,	LightMapY.Z * InvScaleY,	LightMapZ.Z,				0),
			FPlane(-Min.X * InvScaleX,		-Min.Y * InvScaleY,			-LightMapZ|LightMapBase,	1)
			);
}

//
//	FStaticModelLightSurface::TextureToWorld
//

FMatrix FStaticModelLightSurface::TextureToWorld() const
{
	return WorldToTexture().Inverse();
}

//
//	FStaticModelLightTexture::FStaticModelLightTexture
//

FStaticModelLightTexture::FStaticModelLightTexture(UINT InSizeX,UINT InSizeY)
{
	SizeX = InSizeX;
	SizeY = InSizeY;
	Format = PF_G8;
	NumMips = 1;
}

//
//	FStaticModelLightTexture::FStaticModelLightTexture
//

FStaticModelLightTexture::FStaticModelLightTexture()
{
	Format = PF_G8;
	NumMips = 1;
}

//
//	FStaticModelLightTexture::GetData
//

void FStaticModelLightTexture::GetData(void* Buffer,UINT MipIndex,UINT StrideY)
{
	for(UINT SurfaceIndex = 0;SurfaceIndex < (UINT)Surfaces.Num();SurfaceIndex++)
	{
		FStaticModelLightSurface&	Surface = Surfaces(SurfaceIndex);
		BYTE*						Dest = (BYTE*)Buffer + Surface.BaseX + Surface.BaseY * StrideY;
		UINT						X = 0,
									Y = 0,
									StripIndex = 0;

		Surface.BitmapStrips.Load();

		while(Y < Surface.SizeY)
		{
			check(StripIndex < (UINT)Surface.BitmapStrips.Num());

			const FBitmapStrip&	Strip = Surface.BitmapStrips(StripIndex);

			BYTE	Value = (BYTE)Clamp<UINT>(appTrunc((FLOAT)Strip.GetValue() / 15.0f * 255.0f),0,255);
			for(UINT StripX = 0;StripX < Strip.GetLength();StripX++)
				*Dest++ = Value;

			X += Strip.GetLength();
			StripIndex++;

			if(X >= Surface.SizeX)
			{
				check(X == Surface.SizeX);
				Y++;
				X = 0;
				Dest += (StrideY - Surface.SizeX);
			}
		};

		Surface.BitmapStrips.Unload();
	}
}

//
//	FStaticModelLight::FStaticModelLight
//

FStaticModelLight::FStaticModelLight(UModelComponent* InModelComponent,ULightComponent* InLight):
	ModelComponent(InModelComponent),
	Light(InLight)
{}

//
//	FTextureLayoutNode
//

struct FTextureLayoutNode
{
	INT		ChildA,
			ChildB;
	_WORD	MinX,
			MinY,
			SizeX,
			SizeY;
	UBOOL	Used;

	// Constructor.

	FTextureLayoutNode(_WORD InMinX,_WORD InMinY,_WORD InSizeX,_WORD InSizeY):
		ChildA(INDEX_NONE),
		ChildB(INDEX_NONE),
		MinX(InMinX),
		MinY(InMinY),
		SizeX(InSizeX),
		SizeY(InSizeY),
		Used(0)
	{}
};

//
//	FTextureLayout
//

class FTextureLayout
{
public:

	UINT								SizeX,
										SizeY,
										UsedX,
										UsedY;
	TIndirectArray<FTextureLayoutNode>	Nodes;

	// Constructors.

	FTextureLayout() {}

	FTextureLayout(UINT InSizeX,UINT InSizeY):
		SizeX(InSizeX),
		SizeY(InSizeY),
		UsedX(1),
		UsedY(1)
	{
		new(Nodes) FTextureLayoutNode(0,0,SizeX,SizeY);
	}

	// AddSurfaceInner

	INT AddSurfaceInner(INT NodeIndex,UINT SurfaceSizeX,UINT SurfaceSizeY)
	{
		FTextureLayoutNode&	Node = Nodes(NodeIndex);

		if(Node.ChildA != INDEX_NONE)
		{
			check(Node.ChildB != INDEX_NONE);
			INT	Result = AddSurfaceInner(Node.ChildA,SurfaceSizeX,SurfaceSizeY);
			if(Result != INDEX_NONE)
				return Result;
			return AddSurfaceInner(Node.ChildB,SurfaceSizeX,SurfaceSizeY);
		}
		else
		{
			if(Node.Used)
				return INDEX_NONE;

			if(Node.SizeX < SurfaceSizeX || Node.SizeY < SurfaceSizeY)
				return INDEX_NONE;

			if(Node.SizeX == SurfaceSizeX && Node.SizeY == SurfaceSizeY)
				return NodeIndex;

			UINT	ExcessWidth = Node.SizeX - SurfaceSizeX,
					ExcessHeight = Node.SizeY - SurfaceSizeY;

			if(ExcessWidth > ExcessHeight)
			{
				Node.ChildA = Nodes.Num();
                new(Nodes) FTextureLayoutNode(Node.MinX,Node.MinY,SurfaceSizeX,Node.SizeY);

				Node.ChildB = Nodes.Num();
				new(Nodes) FTextureLayoutNode(Node.MinX + SurfaceSizeX,Node.MinY,Node.SizeX - SurfaceSizeX,Node.SizeY);
			}
			else
			{
				Node.ChildA = Nodes.Num();
                new(Nodes) FTextureLayoutNode(Node.MinX,Node.MinY,Node.SizeX,SurfaceSizeY);

				Node.ChildB = Nodes.Num();
				new(Nodes) FTextureLayoutNode(Node.MinX,Node.MinY + SurfaceSizeY,Node.SizeX,Node.SizeY - SurfaceSizeY);
			}

			return AddSurfaceInner(Node.ChildA,SurfaceSizeX,SurfaceSizeY);
		}
	}

	// AddSurface

	UBOOL AddSurface(UINT* BaseX,UINT* BaseY,UINT SurfaceSizeX,UINT SurfaceSizeY)
	{
		INT	NodeIndex = AddSurfaceInner(0,SurfaceSizeX,SurfaceSizeY);

		if(NodeIndex != INDEX_NONE)
		{
			FTextureLayoutNode&	Node = Nodes(NodeIndex);
			Node.Used = 1;
			*BaseX = Node.MinX;
			*BaseY = Node.MinY;
			UsedX = Max(UsedX,Node.MinX + SurfaceSizeX);
			UsedY = Max(UsedY,Node.MinY + SurfaceSizeY);
			return 1;
		}
		else
			return 0;

	}
};

//
//	SphereOnNode
//

static UBOOL SphereOnNode(UModel* Model,UINT NodeIndex,FVector Point,FLOAT Radius)
{
	FBspNode&	Node = Model->Nodes(NodeIndex);
	FBspSurf&	Surf = Model->Surfs(Node.iSurf);

	for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
	{
		// Create plane perpendicular to both this side and the polygon's normal.
		FVector	Edge = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex) - Model->Points(Model->Verts(Node.iVertPool + ((VertexIndex + Node.NumVertices - 1) % Node.NumVertices)).pVertex),
				EdgeNormal = Edge ^ (FVector)Surf.Plane;
		FLOAT	VertexDot = Node.Plane.PlaneDot(Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex));

		// Ignore degenerate edges.
		if(Edge.SizeSquared() < 2.0f*2.0f)
			continue;

		// If point is not behind all the planes created by this polys edges, it's outside the poly.
		if(FPointPlaneDist(Point,Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex),EdgeNormal.SafeNormal()) > Radius)
			return 0;
	}

	return 1;
}

//
//	FStaticModelLightSurface Compare - Sorts descending by size
//

IMPLEMENT_COMPARE_POINTER( FStaticModelLightSurface, UnModelLight, { return B->SizeX * B->SizeY - A->SizeX * A->SizeY; } )


//
//	UModelComponent::CacheLighting
//

void UModelComponent::CacheLighting()
{
	((UActorComponent*)this)->InvalidateLightingCache();

	if(!HasStaticShadowing())
		return;

	FComponentRecreateContext	RecreateContext(this);

	StaticLights.Empty();

	TArray<ULightComponent*>	RelevantLights;
	Scene->GetRelevantLights(this,RelevantLights);
	for(UINT LightIndex = 0;LightIndex < (UINT)RelevantLights.Num();LightIndex++)
	{
		ULightComponent*	Light = RelevantLights(LightIndex);
		if(Light->HasStaticShadowing() && Light->CastShadows )
		{
			UPointLightComponent*				PointLight = Cast<UPointLightComponent>(Light);
			UDirectionalLightComponent*			DirectionalLight = Cast<UDirectionalLightComponent>(Light);
			FStaticModelLight*					StaticLight = new(StaticLights) FStaticModelLight(this,Light);
			TArray<FStaticModelLightSurface*>	LightSurfaces;

			GWarn->BeginSlowTask(*FString::Printf(TEXT("Raytracing %s -> %s"),*Light->GetPathName(),*GetPathName()),1);

			// Create surface light visibility bitmaps.

			for(UINT SurfaceIndex = 0;SurfaceIndex < (UINT)Model->Surfs.Num();SurfaceIndex++)
			{
				FBspSurf&	Surf = Model->Surfs(SurfaceIndex);

				GWarn->StatusUpdatef(SurfaceIndex,Model->Surfs.Num(),TEXT("Raytracing %s -> %s"),*Light->GetPathName(),*GetPathName());

				// Find a plane parallel to the surface.

				FVector		LightMapX,
							LightMapY;
				Surf.Plane.FindBestAxisVectors(LightMapX,LightMapY);

				// Find the surface's nodes and the part of the plane they map to.

				TArray<INT>	SurfaceNodes;
				FLOAT		MinX = WORLD_MAX,
							MinY = WORLD_MAX,
							MaxX = -WORLD_MAX,
							MaxY = -WORLD_MAX;
				UINT		NumSurfaceVertices = 0,
							NumSurfaceIndices = 0;

				for(UINT NodeIndex = 0;NodeIndex < (UINT)Model->Nodes.Num();NodeIndex++)
				{
					FBspNode&	Node = Model->Nodes(NodeIndex);

					if(Node.iSurf == SurfaceIndex && Node.NumVertices)
					{
						UBOOL	IsFrontVisible = Node.iZone[1] == ZoneIndex || ZoneIndex == INDEX_NONE,
								IsBackVisible = Node.iZone[0] == ZoneIndex || ZoneIndex == INDEX_NONE;

						// Ignore nodes which are outside the light's radius.

						if(PointLight)
						{
							FLOAT	PlaneDot = Node.Plane.PlaneDot(PointLight->GetOrigin());
							if(!(Surf.PolyFlags & PF_TwoSided) && (PlaneDot < 0.0f || PlaneDot > PointLight->Radius || !SphereOnNode(Model,NodeIndex,PointLight->GetOrigin(),PointLight->Radius) || !IsFrontVisible))
								continue;
							else if((Surf.PolyFlags & PF_TwoSided) && (Abs(PlaneDot) > PointLight->Radius || !SphereOnNode(Model,NodeIndex,PointLight->GetOrigin(),PointLight->Radius) || !(IsFrontVisible || IsBackVisible)))
								continue;
						}
						else if(DirectionalLight)
						{
							if(!(Surf.PolyFlags & PF_TwoSided) && ((DirectionalLight->GetDirection() | Node.Plane) > 0.0f || !IsFrontVisible))
								continue;
							else if(!(IsFrontVisible || IsBackVisible))
								continue;
						}

						// Compute the bounds of the node's vertices on the surface plane.

						for(UINT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
						{
							FVector	Position = Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex);
							FLOAT	X = LightMapX | Position,
									Y = LightMapY | Position;

							MinX = Min(X,MinX);
							MinY = Min(Y,MinY);
							MaxX = Max(X,MaxX);
							MaxY = Max(Y,MaxY);
						}

						SurfaceNodes.AddItem(NodeIndex);
						NumSurfaceVertices += Node.NumVertices;
						NumSurfaceIndices += (Node.NumVertices - 2) * 3;
					}
				}

				if(!SurfaceNodes.Num())
					continue;

				// Construct a FSurfaceVisibilityBitmap with the final world->lightmap transform.

				UINT						SizeX = Clamp(appCeil((MaxX - MinX) / Surf.LightMapScale),2,LIGHTMAP_MAX_WIDTH),
											SizeY = Clamp(appCeil((MaxY - MinY) / Surf.LightMapScale),2,LIGHTMAP_MAX_HEIGHT);
				FStaticModelLightSurface*	LightSurface = new FStaticModelLightSurface(
					SurfaceIndex,
					SurfaceNodes,
					LightMapX,
					LightMapY,
					Surf.Plane,
					MinX,
					MinY,
					MaxX,
					MaxY,
					SizeX,
					SizeY
					);

				// Raytrace the surface.

				FPlane	LightPosition = Light->GetPosition();

				UBOOL			Discard = 1;
				TArray<UBOOL>	CoverageMap;
				TArray<UBOOL>	VisibilityMap;
				FMatrix			LightMapToWorld = LightSurface->TextureToWorld();

				CoverageMap.AddZeroed(SizeX * SizeY);
				VisibilityMap.AddZeroed(SizeX * SizeY);
					
				for(UINT Y = 0;Y < SizeY;Y++)
				{
					for(UINT X = 0;X < SizeX;X++)
					{
						// Determine the world position this lightmap texel maps to.

						FVector	SamplePosition = LightMapToWorld.TransformFVector(
													FVector(
														(FLOAT)X / (FLOAT)(SizeX - 1),
														(FLOAT)Y / (FLOAT)(SizeY - 1),
														0.0f
														)
													),
								LightVector = (FVector)LightPosition - SamplePosition * LightPosition.W;

						// Determine whether the texel maps to a point on one of the surface's polygons.

						UBOOL	Coverage = 0;

						for(UINT NodeIndex = 0;NodeIndex < (UINT)SurfaceNodes.Num();NodeIndex++)
						{
							if(SphereOnNode(Model,SurfaceNodes(NodeIndex),SamplePosition,THRESH_POINT_ON_PLANE))
							{
								Coverage = 1;
								break;
							}
						}

						if(!Coverage)
							continue;

						// Check whether the light has clear visibility to the sample point.

						CoverageMap(X + Y * SizeX)++;

						FCheckResult	Hit(1.0f);
						if(Level->SingleLineCheck(Hit,Level->GetLevelInfo(),SamplePosition + LightVector.SafeNormal() * 0.25f,SamplePosition + LightVector,TRACE_Level|TRACE_Actors|TRACE_ShadowCast,FVector(0,0,0)))
						{
							VisibilityMap(X + Y * SizeX)++;
							Discard = 0;
						}
					}
				}

				if(Discard)
					delete LightSurface;
				else
				{
					// Filter the visibility bitmap to a RLE encoded version.

					UINT	FilterSizeX = 5,
							FilterSizeY = 5,
							FilterMiddleX = (FilterSizeX - 1) / 2,
							FilterMiddleY = (FilterSizeY - 1) / 2;
					UINT	Filter[5][5] =
					{
						{ 58,  85,  96,  85, 58 },
						{ 85, 123, 140, 123, 85 },
						{ 96, 140, 159, 140, 96 },
						{ 85, 123, 140, 123, 85 },
						{ 58,  85,  96,  85, 58 }
					};

					TArray<UINT>	FilteredVisibilityMap;

					FilteredVisibilityMap.AddZeroed(SizeX * SizeY);

					for(UINT Y = 0;Y < SizeY;Y++)
					{
						FBitmapStrip*	Strip = NULL;

						for(UINT X = 0;X < SizeX;X++)
						{
							UINT	Numerator = 0,
									Denominator = 0;

							for(UINT FilterY = 0;FilterY < FilterSizeX;FilterY++)
							{
								for(UINT FilterX = 0;FilterX < FilterSizeY;FilterX++)
								{
									INT	SubX = (INT)X - FilterMiddleX + FilterX,
										SubY = (INT)Y - FilterMiddleY + FilterY;
									if(SubX >= 0 && SubX < (INT)SizeX && SubY >= 0 && SubY < (INT)SizeY)
									{
										Numerator += Filter[FilterX][FilterY] * VisibilityMap(SubX + SubY * SizeX);
										Denominator += Filter[FilterX][FilterY] * CoverageMap(SubX + SubY * SizeX);
									}
								}
							}

							UINT	Visibility;
							if(Denominator > 0)
								Visibility = Min<UINT>(appTrunc((FLOAT)Numerator / (FLOAT)Denominator * 15.0f),15);
							else
								Visibility = 0;

							if(Strip && Strip->GetValue() == Visibility && Strip->GetLength() < 16)
								Strip->IncLength();
							else
								Strip = new(LightSurface->BitmapStrips) FBitmapStrip(Visibility);
						}
					}

					// Detach lazy loader so changes won't be clobbered.
					LightSurface->BitmapStrips.Detach();

					LightSurfaces.AddItem(LightSurface);
				}
			}

			// Pack the light surfaces into textures.

			Sort<USE_COMPARE_POINTER(FStaticModelLightSurface,UnModelLight)>(&LightSurfaces(0),LightSurfaces.Num());

			TIndirectArray<FTextureLayout>	TextureLayouts;
			UINT							NumUsedTexels = 0;
			for(UINT SurfaceIndex = 0;SurfaceIndex < (UINT)LightSurfaces.Num();SurfaceIndex++)
			{
				FStaticModelLightSurface*	LightSurface = LightSurfaces(SurfaceIndex);
				INT							FinalTextureIndex = INDEX_NONE;
				for(UINT TextureIndex = 0;TextureIndex < (UINT)StaticLight->Textures.Num();TextureIndex++)
				{
					if(TextureLayouts(TextureIndex).AddSurface(&LightSurface->BaseX,&LightSurface->BaseY,LightSurface->SizeX,LightSurface->SizeY))
					{
						FinalTextureIndex = TextureIndex;
						break;
					}
				}

				if(FinalTextureIndex == INDEX_NONE)
				{
					FinalTextureIndex = StaticLight->Textures.Num();
					new(StaticLight->Textures) FStaticModelLightTexture(LIGHTMAP_TEXTURE_WIDTH,LIGHTMAP_TEXTURE_HEIGHT);
					FTextureLayout*	TextureLayout = new(TextureLayouts) FTextureLayout(LIGHTMAP_TEXTURE_WIDTH,LIGHTMAP_TEXTURE_HEIGHT);
					verify(TextureLayout->AddSurface(&LightSurface->BaseX,&LightSurface->BaseY,LightSurface->SizeX,LightSurface->SizeY));
				}
				new(StaticLight->Textures(FinalTextureIndex).Surfaces) FStaticModelLightSurface(*LightSurface);
				NumUsedTexels += LightSurface->SizeX * LightSurface->SizeY;
			}

			UINT	NumTotalTexels = 0;
			for(UINT TextureIndex = 0;TextureIndex < (UINT)StaticLight->Textures.Num();TextureIndex++)
			{
				StaticLight->Textures(TextureIndex).SizeX = TextureLayouts(TextureIndex).UsedX;
				StaticLight->Textures(TextureIndex).SizeY = TextureLayouts(TextureIndex).UsedY;
				NumTotalTexels += StaticLight->Textures(TextureIndex).SizeX * StaticLight->Textures(TextureIndex).SizeY;
			}

			GWarn->EndSlowTask();
		}
	}
}

void UModelComponent::InvalidateLightingCache(ULightComponent* Light)
{
	for(UINT LightIndex = 0;LightIndex < (UINT)StaticLights.Num();LightIndex++)
	{
		if(StaticLights(LightIndex).Light == Light)
		{
			FComponentRecreateContext	RecreateContext(this);
			StaticLights.Remove(LightIndex);
			break;
		}
	}
}

void UModelComponent::InvalidateLightingCache()
{
	FComponentRecreateContext RecreateContext(this);
	StaticLights.Empty();
	IgnoreLights.Empty();
}
