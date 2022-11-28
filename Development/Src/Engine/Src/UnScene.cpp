/*=============================================================================
	UnScene.cpp: Scene manager implementation.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(ULineBatchComponent);

IMPLEMENT_RESOURCE_TYPE(FSceneViewState);

//
//	FBatchedTriangleIndexBuffer
//

struct FBatchedTriangleIndexBuffer: FIndexBuffer
{
	UINT	NumTriangles;

	// Constructor.

	FBatchedTriangleIndexBuffer(UINT InNumTriangles):
		NumTriangles(InNumTriangles)
	{
		Stride = sizeof(_WORD);
		Size = NumTriangles * 3 * Stride;
	}

	// FIndexBuffer interface.

	virtual void GetData(void* Buffer)
	{
		_WORD*	Index = (_WORD*)Buffer;
		for(UINT TriangleIndex = 0;TriangleIndex < NumTriangles;TriangleIndex++)
		{
			*(Index++) = 3 * TriangleIndex + 0;
			*(Index++) = 3 * TriangleIndex + 1;
			*(Index++) = 3 * TriangleIndex + 2;
		}
	}
};

//
//	FBatchedTriangleVertex
//

struct FBatchedTriangleVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	FVector2D		TexCoord;
};

//
//	FBatchedTriangleVertexBuffer
//

struct FBatchedTriangleVertexBuffer: FVertexBuffer
{
private:

	TArray<FBatchedTriangleVertex>	Vertices;

public:

	// Constructor.

	FBatchedTriangleVertexBuffer()
	{
		Stride = sizeof(FBatchedTriangleVertex);
		Size = 0;
	}

	// AddVertex

	void AddVertex(const FRawTriangleVertex& SourceVertex)
	{
		FBatchedTriangleVertex*	DestVertex = new(Vertices) FBatchedTriangleVertex;
		DestVertex->Position = SourceVertex.Position;
		DestVertex->TangentX = SourceVertex.TangentX;
		DestVertex->TangentY = SourceVertex.TangentY;
		DestVertex->TangentZ = SourceVertex.TangentZ;
		DestVertex->TexCoord = SourceVertex.TexCoord;
		Size += Stride;
	}

	// FVertexBuffer interface.

	void GetData(void* Data)
	{
		appMemcpy(Data,&Vertices(0),Size);
	}
};

//
//	FBatchedTriangleRenderInterface
//

struct FBatchedTriangleRenderInterface: FTriangleRenderInterface
{
	FPrimitiveRenderInterface*		PRI;
	FMatrix							LocalToWorld;
	FMaterial*						Material;
	FMaterialInstance*				MaterialInstance;
	FBatchedTriangleVertexBuffer	VertexBuffer;

	// Constructor.

	FBatchedTriangleRenderInterface(FPrimitiveRenderInterface* InPRI,const FMatrix& InLocalToWorld,FMaterial* InMaterial,FMaterialInstance* InMaterialInstance):
		PRI(InPRI),
		LocalToWorld(InLocalToWorld),
		Material(InMaterial),
		MaterialInstance(InMaterialInstance)
	{}

	// FTriangleRenderInterface interface.

	virtual void DrawTriangle(const FRawTriangleVertex& V0,const FRawTriangleVertex& V1,const FRawTriangleVertex& V2)
	{
		VertexBuffer.AddVertex(V0);
		VertexBuffer.AddVertex(V1);
		VertexBuffer.AddVertex(V2);
	}

	virtual void Finish()
	{
		FLocalVertexFactory	VertexFactory;
		VertexFactory.LocalToWorld = LocalToWorld;
		VertexFactory.WorldToLocal = LocalToWorld.Inverse();
		VertexFactory.PositionComponent = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FBatchedTriangleVertex,Position),VCT_Float3);
		VertexFactory.TangentBasisComponents[0] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FBatchedTriangleVertex,TangentX),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[1] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FBatchedTriangleVertex,TangentY),VCT_PackedNormal);
		VertexFactory.TangentBasisComponents[2] = FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FBatchedTriangleVertex,TangentZ),VCT_PackedNormal);
		VertexFactory.TextureCoordinates.AddItem(FVertexStreamComponent(&VertexBuffer,STRUCT_OFFSET(FBatchedTriangleVertex,TexCoord),VCT_Float2));

		if( VertexBuffer.Num() > 0 )
		{
			FBatchedTriangleIndexBuffer	IndexBuffer(VertexBuffer.Num() / 3);
			PRI->DrawMesh(&VertexFactory,&IndexBuffer,Material,MaterialInstance,0,IndexBuffer.NumTriangles,0,VertexBuffer.Num() - 1);
		}

		delete this;
	}
};

//
//	FPrimitiveRenderInterface::GetTRI
//

FTriangleRenderInterface* FPrimitiveRenderInterface::GetTRI(const FMatrix& LocalToWorld,FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	return new FBatchedTriangleRenderInterface(this,LocalToWorld,Material,MaterialInstance);
}


/////////////////////////////////////////////////////////////////////////////////////
// FPrimitiveRenderInterface::DrawBox
/////////////////////////////////////////////////////////////////////////////////////

void FPrimitiveRenderInterface::DrawBox(const FMatrix& BoxToWorld,const FVector& Radii,FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	// Calculate verts for a face pointing down Z
	FRawTriangleVertex	FaceVerts[4]; 

	FaceVerts[0].Position = FVector( -1, -1, 1);
	FaceVerts[1].Position = FVector( -1,  1, 1);
	FaceVerts[2].Position = FVector(  1,  1, 1);
	FaceVerts[3].Position = FVector(  1, -1, 1);

	for(INT i=0; i<4; i++)
	{
		FaceVerts[i].TangentX = FVector(1,0,0);
		FaceVerts[i].TangentY = FVector(0,1,0);
		FaceVerts[i].TangentZ = FVector(0,0,1); // Tangent Z is basically vertex normal
	}

	FaceVerts[0].TexCoord.X = 0; FaceVerts[0].TexCoord.Y = 0;
	FaceVerts[1].TexCoord.X = 0; FaceVerts[1].TexCoord.Y = 1;
	FaceVerts[2].TexCoord.X = 1; FaceVerts[2].TexCoord.Y = 1;
	FaceVerts[3].TexCoord.X = 1; FaceVerts[3].TexCoord.Y = 0;

	// Then rotate this face 6 times
	FRotator FaceRotations[6];
	FaceRotations[0] = FRotator(0,		0,	0);
	FaceRotations[1] = FRotator(16384,	0,	0);
	FaceRotations[2] = FRotator(-16384,	0,  0);
	FaceRotations[3] = FRotator(0,		0,	16384);
	FaceRotations[4] = FRotator(0,		0,	-16384);
	FaceRotations[5] = FRotator(32768,	0,	0);


	FTriangleRenderInterface* TRI = GetTRI(FScaleMatrix(Radii) * BoxToWorld,Material,MaterialInstance);

	for(INT f=0; f<6; f++)
	{
		FRotationMatrix		FaceRot( FaceRotations[f] );
		FRawTriangleVertex	RotatedFaceVerts[4]; 

		for(INT v=0; v<4; v++)
		{
			RotatedFaceVerts[v].Position = FaceRot.TransformFVector( FaceVerts[v].Position );
			RotatedFaceVerts[v].TangentX = FaceRot.TransformNormal( FaceVerts[v].TangentX );
			RotatedFaceVerts[v].TangentY = FaceRot.TransformNormal( FaceVerts[v].TangentY );
			RotatedFaceVerts[v].TangentZ = FaceRot.TransformNormal( FaceVerts[v].TangentZ );
			RotatedFaceVerts[v].TexCoord = FaceVerts[v].TexCoord;
		}

		TRI->DrawQuad(RotatedFaceVerts[0],RotatedFaceVerts[1],RotatedFaceVerts[2],RotatedFaceVerts[3]);
	}

	TRI->Finish();
}

/////////////////////////////////////////////////////////////////////////////////////
// FPrimitiveRenderInterface::DrawSphere
/////////////////////////////////////////////////////////////////////////////////////

void FPrimitiveRenderInterface::DrawSphere(const FVector& Center, const FVector& Radii, INT NumSides, INT NumRings, FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	// The first/last arc are on top of each other.
	INT NumVerts = (NumSides+1) * (NumRings+1);
	FRawTriangleVertex* Verts = (FRawTriangleVertex*)appMalloc( NumVerts * sizeof(FRawTriangleVertex) );

	// Calculate verts for one arc
	FRawTriangleVertex* ArcVerts = (FRawTriangleVertex*)appMalloc( (NumRings+1) * sizeof(FRawTriangleVertex) );

	for(INT i=0; i<NumRings+1; i++)
	{
		FRawTriangleVertex* ArcVert = &ArcVerts[i];

		FLOAT angle = ((FLOAT)i/NumRings) * PI;

		// Note- unit sphere, so position always has mag of one. We can just use it for normal!			
		ArcVert->Position.X = 0.0f;
		ArcVert->Position.Y = appSin(angle);
		ArcVert->Position.Z = appCos(angle);

		ArcVert->TangentX = FVector(1,0,0);

		ArcVert->TangentY.X = 0.0f;
		ArcVert->TangentY.Y = -ArcVert->Position.Z;
		ArcVert->TangentY.Z = ArcVert->Position.Y;

		ArcVert->TangentZ = ArcVert->Position; 

		ArcVert->TexCoord.X = 0.0f;
		ArcVert->TexCoord.Y = ((FLOAT)i/NumRings);
	}

	// Then rotate this arc NumSides+1 times.
	for(INT s=0; s<NumSides+1; s++)
	{
		FRotator ArcRotator(0, 65535 * ((FLOAT)s/NumSides), 0);
		FRotationMatrix ArcRot( ArcRotator );
		FLOAT XTexCoord = ((FLOAT)s/NumSides);

		for(INT v=0; v<NumRings+1; v++)
		{
			INT VIx = (NumRings+1)*s + v;

			Verts[VIx].Position = ArcRot.TransformFVector( ArcVerts[v].Position );
			Verts[VIx].TangentX = ArcRot.TransformNormal( ArcVerts[v].TangentX );
			Verts[VIx].TangentY = ArcRot.TransformNormal( ArcVerts[v].TangentY );
			Verts[VIx].TangentZ = ArcRot.TransformNormal( ArcVerts[v].TangentZ );
			Verts[VIx].TexCoord.X = XTexCoord;
			Verts[VIx].TexCoord.Y = ArcVerts[v].TexCoord.Y;
		}
	}

	FTriangleRenderInterface* TRI = GetTRI(FScaleMatrix( Radii ) * FTranslationMatrix( Center ), Material, MaterialInstance);

	for(INT s=0; s<NumSides; s++)
	{
		INT a0start = (s+0) * (NumRings+1);
		INT a1start = (s+1) * (NumRings+1);

		for(INT r=0; r<NumRings; r++)
		{
			TRI->DrawQuad( Verts[a0start + r + 0], Verts[a1start + r + 0], Verts[a1start + r + 1], Verts[a0start + r + 1] );
		}
	}

	TRI->Finish();

	appFree(Verts);
	appFree(ArcVerts);
}



//
//	FPrimitiveRenderInterface::DrawFrustumBounds
//

void FPrimitiveRenderInterface::DrawFrustumBounds(const FMatrix& WorldToFrustum,FMaterial* Material,FMaterialInstance* MaterialInstance)
{
	FRawTriangleVertex	Vertices[2][2][2];
	FLOAT				Signs[2] = { -1.0f, 1.0f };
	FMatrix				FrustumToWorld = WorldToFrustum.Inverse();
	FVector				W = FVector(WorldToFrustum.M[0][3],WorldToFrustum.M[1][3],WorldToFrustum.M[2][3]),
						Z = FVector(WorldToFrustum.M[0][2],WorldToFrustum.M[1][2],WorldToFrustum.M[2][2]);

	FLOAT	MinZ = 0.0f,
			MaxZ = 1.0f,
			MinW = 1.0f,
			MaxW = 1.0f;

	if((Z|W) > DELTA*DELTA)
	{
		MinW = (MinZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MinZ) + WorldToFrustum.M[3][3];
		MaxW = (MaxZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MaxZ) + WorldToFrustum.M[3][3];
	}

	for(UINT Z = 0;Z < 2;Z++)
		for(UINT Y = 0;Y < 2;Y++)
			for(UINT X = 0;X < 2;X++)
				Vertices[X][Y][Z].Position = FrustumToWorld.TransformFPlane(
				FPlane(
				Signs[X] * (Z ? MaxW : MinW),
				Signs[Y] * (Z ? MaxW : MinW),
				Z ? (MaxZ * MaxW) : (MinZ * MinW),
				Z ? MaxW : MinW
				)
				);

	FTriangleRenderInterface*	TRI = GetTRI(FMatrix::Identity,Material,MaterialInstance);

	TRI->DrawQuad(Vertices[0][0][0],Vertices[0][1][0],Vertices[0][1][1],Vertices[0][0][1]);	// -X
	TRI->DrawQuad(Vertices[1][0][0],Vertices[1][0][1],Vertices[1][1][1],Vertices[1][1][0]);	// +X
	TRI->DrawQuad(Vertices[0][0][0],Vertices[0][0][1],Vertices[1][0][1],Vertices[1][0][0]);	// -Y
	TRI->DrawQuad(Vertices[0][1][0],Vertices[1][1][0],Vertices[1][1][1],Vertices[0][1][1]);	// +Y
	TRI->DrawQuad(Vertices[0][0][0],Vertices[1][0][0],Vertices[1][1][0],Vertices[0][1][0]);	// -Z
	TRI->DrawQuad(Vertices[0][0][1],Vertices[0][1][1],Vertices[1][1][1],Vertices[1][0][1]);	// +Z

	TRI->Finish();
}

/////////////////////////////////////////////////////////////////////////////////////
// End
/////////////////////////////////////////////////////////////////////////////////////


//
//	FPrimitiveRenderInterface::DrawWireBox
//

void FPrimitiveRenderInterface::DrawWireBox(const FBox& Box,FColor Color)
{
	FVector	B[2],P,Q;
	int i,j;

	B[0]=Box.Min;
	B[1]=Box.Max;

	for( i=0; i<2; i++ ) for( j=0; j<2; j++ )
	{
		P.X=B[i].X; Q.X=B[i].X;
		P.Y=B[j].Y; Q.Y=B[j].Y;
		P.Z=B[0].Z; Q.Z=B[1].Z;
		DrawLine(P,Q,Color);

		P.Y=B[i].Y; Q.Y=B[i].Y;
		P.Z=B[j].Z; Q.Z=B[j].Z;
		P.X=B[0].X; Q.X=B[1].X;
		DrawLine(P,Q,Color);

		P.Z=B[i].Z; Q.Z=B[i].Z;
		P.X=B[j].X; Q.X=B[j].X;
		P.Y=B[0].Y; Q.Y=B[1].Y;
		DrawLine(P,Q,Color);
	}
}

//
//	FPrimitiveRenderInterface::DrawCircle
//
void FPrimitiveRenderInterface::DrawCircle(const FVector& Base,const FVector& X,const FVector& Y,FColor Color,FLOAT Radius,INT NumSides)
{
	FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		FVector	Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		DrawLine(LastVertex,Vertex,Color);
		LastVertex = Vertex;
	}
}

//
//	FPrimitiveRenderInterface::DrawWireCylinder
//

void FPrimitiveRenderInterface::DrawWireCylinder(const FVector& Base,const FVector& X,const FVector& Y,const FVector& Z,FColor Color,FLOAT Radius,FLOAT HalfHeight,INT NumSides)
{
	FLOAT	AngleDelta = 2.0f * PI / NumSides;
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0;SideIndex < NumSides;SideIndex++)
	{
		FVector	Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;

		DrawLine(LastVertex - Z * HalfHeight,Vertex - Z * HalfHeight,Color);
		DrawLine(LastVertex + Z * HalfHeight,Vertex + Z * HalfHeight,Color);
		DrawLine(LastVertex - Z * HalfHeight,LastVertex + Z * HalfHeight,Color);

		LastVertex = Vertex;
	}
}

//
//	FPrimitiveRenderInterface::DrawDirectionalArrow
//

void FPrimitiveRenderInterface::DrawDirectionalArrow(const FMatrix& ArrowToWorld,FColor Color,FLOAT Length,FLOAT ArrowSize)
{
	DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(0,0,0)),Color);
	DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,+ArrowSize,+ArrowSize)),Color);
	DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,+ArrowSize,-ArrowSize)),Color);
	DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,-ArrowSize,+ArrowSize)),Color);
	DrawLine(ArrowToWorld.TransformFVector(FVector(Length,0,0)),ArrowToWorld.TransformFVector(FVector(Length-ArrowSize,-ArrowSize,-ArrowSize)),Color);

}

//
// FPrimitiveRenderInterface::DrawWireStar
//

void FPrimitiveRenderInterface::DrawWireStar(const FVector& Position, FLOAT Size, FColor Color)
{
	DrawLine(Position + Size * FVector(1,0,0), Position - Size * FVector(1,0,0), Color);
	DrawLine(Position + Size * FVector(0,1,0), Position - Size * FVector(0,1,0), Color);
	DrawLine(Position + Size * FVector(0,0,1), Position - Size * FVector(0,0,1), Color);

}

void FPrimitiveRenderInterface::DrawDashedLine(const FVector& Start, const FVector& End, FColor Color, FLOAT DashSize)
{
	FVector LineDir = End - Start;
	FLOAT LineLeft = (End - Start).Size();
	LineDir /= LineLeft;

	while(LineLeft > 0.f)
	{
		FVector DrawStart = End - ( LineLeft * LineDir );
		FVector DrawEnd = DrawStart + ( Min<FLOAT>(DashSize, LineLeft) * LineDir );

		DrawLine(DrawStart, DrawEnd, Color);

		LineLeft -= 2*DashSize;
	}
}

void FPrimitiveRenderInterface::DrawFrustumWireframe(const FMatrix& WorldToFrustum,FColor Color)
{
	FVector		Vertices[2][2][2];
	FLOAT		Signs[2] = { -1.0f, 1.0f };
	FMatrix		FrustumToWorld = WorldToFrustum.Inverse();
	FVector		W = FVector(WorldToFrustum.M[0][3],WorldToFrustum.M[1][3],WorldToFrustum.M[2][3]),
				Z = FVector(WorldToFrustum.M[0][2],WorldToFrustum.M[1][2],WorldToFrustum.M[2][2]);

	FLOAT	MinZ = 0.0f,
			MaxZ = 1.0f,
			MinW = 1.0f,
			MaxW = 1.0f;

	if((Z|W) > DELTA*DELTA)
	{
		MinW = (MinZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MinZ) + WorldToFrustum.M[3][3];
		MaxW = (MaxZ * WorldToFrustum.M[3][3] - WorldToFrustum.M[3][2]) / ((Z|W) - MaxZ) + WorldToFrustum.M[3][3];
	}

	for(UINT Z = 0;Z < 2;Z++)
		for(UINT Y = 0;Y < 2;Y++)
			for(UINT X = 0;X < 2;X++)
				Vertices[X][Y][Z] = FrustumToWorld.TransformFPlane(
					FPlane(
						Signs[X] * (Z ? MaxW : MinW),
						Signs[Y] * (Z ? MaxW : MinW),
						Z ? (MaxZ * MaxW) : (MinZ * MinW),
						Z ? MaxW : MinW
						)
					);

	DrawLine(Vertices[0][0][0],Vertices[0][0][1],Color);
	DrawLine(Vertices[1][0][0],Vertices[1][0][1],Color);
	DrawLine(Vertices[0][1][0],Vertices[0][1][1],Color);
	DrawLine(Vertices[1][1][0],Vertices[1][1][1],Color);

	DrawLine(Vertices[0][0][0],Vertices[0][1][0],Color);
	DrawLine(Vertices[1][0][0],Vertices[1][1][0],Color);
	DrawLine(Vertices[0][0][1],Vertices[0][1][1],Color);
	DrawLine(Vertices[1][0][1],Vertices[1][1][1],Color);

	DrawLine(Vertices[0][0][0],Vertices[1][0][0],Color);
	DrawLine(Vertices[0][1][0],Vertices[1][1][0],Color);
	DrawLine(Vertices[0][0][1],Vertices[1][0][1],Color);
	DrawLine(Vertices[0][1][1],Vertices[1][1][1],Color);
}

//
//	FScene::AddLight
//

void FScene::AddLight(ULightComponent* Light)
{
	if(Light->IsA(USkyLightComponent::StaticClass()))
		SkyLights.AddItem(Cast<USkyLightComponent>(Light));
}

//
//	FScene::RemoveLight
//

void FScene::RemoveLight(ULightComponent* Light)
{
	if(Light->IsA(USkyLightComponent::StaticClass()))
		SkyLights.RemoveItem(Cast<USkyLightComponent>(Light));
}

//
//	FScene::Serialize
//

void FScene::Serialize(FArchive& Ar)
{
	Ar << Fogs << WindPointSources << WindDirectionalSources;
}

//
//	FPreviewScene::AddPrimitive
//

void FPreviewScene::AddPrimitive(UPrimitiveComponent* Primitive)
{
	Primitives.AddItem(Primitive);

	for(UINT LightIndex = 0;LightIndex < (UINT)Lights.Num();LightIndex++)
	{
		if(Lights(LightIndex)->AffectsSphere(Primitive->Bounds.GetSphere()))
			Primitive->AttachLight(Lights(LightIndex));
	}
}

//
//	FPreviewScene::RemovePrimitive
//

void FPreviewScene::RemovePrimitive(UPrimitiveComponent* Primitive)
{
	while(Primitive->Lights.Num())
		Primitive->DetachLight(Primitive->Lights(0).GetLight());

	Primitives.RemoveItem(Primitive);
}

//
//	FPreviewScene::AddLight
//

void FPreviewScene::AddLight(ULightComponent* Light)
{
	FScene::AddLight(Light);

	Lights.AddItem(Light);

	for(UINT PrimitiveIndex = 0;PrimitiveIndex < (UINT)Primitives.Num();PrimitiveIndex++)
		if(Light->AffectsSphere(Primitives(PrimitiveIndex)->Bounds.GetSphere()))
			Primitives(PrimitiveIndex)->AttachLight(Light);
}

//
//	FPreviewScene::RemoveLight
//

void FPreviewScene::RemoveLight(ULightComponent* Light)
{
	FScene::RemoveLight(Light);

	for(UINT PrimitiveIndex = 0;PrimitiveIndex < (UINT)Primitives.Num();PrimitiveIndex++)
		for(UINT LightIndex = 0;LightIndex < (UINT)Primitives(PrimitiveIndex)->Lights.Num();LightIndex++)
			if(Primitives(PrimitiveIndex)->Lights(LightIndex).GetLight() == Light)
			{
				Primitives(PrimitiveIndex)->DetachLight(Light);
				break;
			}

	check(!Light->FirstDynamicPrimitiveLink);
	check(!Light->FirstStaticPrimitiveLink);
	Lights.RemoveItem(Light);
}

//
//	FPreviewScene::GetVisiblePrimitives
//

void FPreviewScene::GetVisiblePrimitives(const FSceneContext& Context,const FConvexVolume& Frustum,TArray<UPrimitiveComponent*>& OutPrimitives)
{
	FCycleCounterSection	CycleCounter(GVisibilityStats.FrustumCheckTime);
	for(INT PrimitiveIndex = 0;PrimitiveIndex < Primitives.Num();PrimitiveIndex++)
		if(Frustum.IntersectBox(Primitives(PrimitiveIndex)->Bounds.Origin,Primitives(PrimitiveIndex)->Bounds.BoxExtent))
			OutPrimitives.AddItem(Primitives(PrimitiveIndex));
}

//
//	FPreviewScene::GetRelevantLights
//

void FPreviewScene::GetRelevantLights(UPrimitiveComponent* Primitive,TArray<ULightComponent*>& OutLights)
{
	FCycleCounterSection	CycleCounter(GVisibilityStats.RelevantLightTime);
	for(INT LightIndex = 0;LightIndex < Lights.Num();LightIndex++)
		if(Lights(LightIndex)->AffectsSphere(Primitive->Bounds.GetSphere()))
			OutLights.AddItem(Lights(LightIndex));
}

//
//	FConvexVolume::ClipPolygon
//

UBOOL FConvexVolume::ClipPolygon(FPoly& Polygon) const
{
	for(UINT PlaneIndex = 0;PlaneIndex < (UINT)Planes.Num();PlaneIndex++)
	{
		const FPlane&	Plane = Planes(PlaneIndex);
		if(!Polygon.Split(-FVector(Plane),Plane * Plane.W,1))
			return 0;
	}
	return 1;
}

//
//	FConvexVolume::GetBoxIntersectionOutcode
//

FOutcode FConvexVolume::GetBoxIntersectionOutcode(const FVector& Origin,const FVector& Extent) const
{
	FOutcode	Result(1,0);
	for(UINT PlaneIndex = 0;PlaneIndex < (UINT)Planes.Num();PlaneIndex++)
	{
		FLOAT	Distance = Planes(PlaneIndex).PlaneDot(Origin),
				PushOut = FBoxPushOut(Planes(PlaneIndex),Extent);
		if(Distance > PushOut)
		{
			Result.SetInside(0);
			Result.SetOutside(1);
			break;
		}
		else if(Distance > -PushOut)
			Result.SetOutside(1);
	}
	return Result;
}

//
//	FConvexVolume::IntersectBox
//

UBOOL FConvexVolume::IntersectBox(const FVector& Origin,const FVector& Extent) const
{
	for(UINT PlaneIndex = 0;PlaneIndex < (UINT)Planes.Num();PlaneIndex++)
		if(Planes(PlaneIndex).PlaneDot(Origin) > FBoxPushOut(Planes(PlaneIndex),Extent))
			return 0;
	return 1;
}

//
//	FConvexVolume::IntersectSphere
//

UBOOL FConvexVolume::IntersectSphere(const FVector& Origin,const FLOAT Radius) const
{
	for(UINT PlaneIndex = 0;PlaneIndex < (UINT)Planes.Num();PlaneIndex++)
		if(Planes(PlaneIndex).PlaneDot(Origin) > Radius)
			return 0;
	return 1;
}

void FConvexVolume::GetViewFrustumBounds(const FMatrix& ViewProjectionMatrix,UBOOL UseNearPlane,FConvexVolume& OutFrustumBounds)
{
	FPlane	Temp;

	// Near clipping plane.

	if(UseNearPlane && ViewProjectionMatrix.GetFrustumNearPlane(Temp))
		OutFrustumBounds.Planes.AddItem(Temp);

	// Left clipping plane.

	if(ViewProjectionMatrix.GetFrustumLeftPlane(Temp))
		OutFrustumBounds.Planes.AddItem(Temp);

	// Right clipping plane.

	if(ViewProjectionMatrix.GetFrustumRightPlane(Temp))
		OutFrustumBounds.Planes.AddItem(Temp);

	// Top clipping plane.

	if(ViewProjectionMatrix.GetFrustumTopPlane(Temp))
		OutFrustumBounds.Planes.AddItem(Temp);

	// Bottom clipping plane.

	if(ViewProjectionMatrix.GetFrustumBottomPlane(Temp))
		OutFrustumBounds.Planes.AddItem(Temp);

	// Far clipping plane.

	if(ViewProjectionMatrix.GetFrustumFarPlane(Temp))
		OutFrustumBounds.Planes.AddItem(Temp);
}

//
//	FSceneView::Project
//

FPlane FSceneView::Project(const FVector& V) const
{
	FPlane	Result = ProjectionMatrix.TransformFPlane(ViewMatrix.TransformFPlane(FPlane(V,1)));
	FLOAT	RHW = 1.0f / Result.W;

	return FPlane(Result.X * RHW,Result.Y * RHW,Result.Z * RHW,Result.W);
}

//
//	FSceneView::Deproject
//

FVector FSceneView::Deproject(const FPlane& P) const
{
	return (ViewMatrix * ProjectionMatrix).Inverse().TransformFPlane(FPlane(P.X * P.W,P.Y * P.W,P.Z * P.W,P.W));
}

//
//	FSceneContext::Project
//

UBOOL FSceneContext::Project(const FVector& V,INT& InX,INT& InY) const
{
	FPlane proj = View->Project(V);
	INT HalfX = SizeX/2,
		HalfY = SizeY/2;
	InX = HalfX + ( HalfX * proj.X );
	InY = HalfY + ( HalfY * (proj.Y * -1) );
	return proj.W > 0.0f;
}

/** Modify an existing FSceneContext to enforce a particular aspect ratio. Resulting draw area will be centered on existing area. */
void FSceneContext::FixAspectRatio(FLOAT InAspectRatio)
{
	FLOAT CurrentAspectRatio = ((FLOAT)SizeX)/((FLOAT)SizeY);

	// If the current screen aspect ratio is sufficiently different from the desired one...
	if( ::Abs( CurrentAspectRatio - InAspectRatio) > 0.01f )
	{
		// If desired aspect ratio is bigger than current - we need black bars on top and bottom.
		if( InAspectRatio > CurrentAspectRatio)
		{
			// Calculate desired Y size.
			INT NewSizeY = appRound( ((FLOAT)SizeX) / InAspectRatio );

			Y = appRound( 0.5f * ((FLOAT)(SizeY - NewSizeY)) );
			SizeY = NewSizeY;
		}
		// Otherwise - will place bars on the sides.
		else
		{
			INT NewSizeX = appRound( ((FLOAT)SizeY) * InAspectRatio );

			X = appRound( 0.5f * ((FLOAT)(SizeX - NewSizeX)) );
			SizeX = NewSizeX;
		}
	}
}

