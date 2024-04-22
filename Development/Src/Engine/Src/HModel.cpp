/*=============================================================================
	HModel.cpp: HModel implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "HModel.h"
#include "UnRaster.h"

/**
 * The state of the model hit test.
 */
class FModelHitState
{
public:

	UINT SurfaceIndex;
	FLOAT SurfaceZ;
	UBOOL bHitSurface;

	INT HitX;
	INT HitY;

	/** Initialization constructor. */
	FModelHitState(INT InHitX,INT InHitY):
		SurfaceZ(FLT_MAX),
		bHitSurface(FALSE),
		HitX(InHitX),
		HitY(InHitY)
	{}
};

/**
 * A rasterization policy which is used to determine the BSP surface clicked on.
 */
class FModelHitRasterPolicy
{
public:
	typedef FVector4 InterpolantType;

	FModelHitRasterPolicy(UINT InSurfaceIndex,FModelHitState& InHitState):
		SurfaceIndex(InSurfaceIndex),
		HitState(InHitState)
	{}

	void ProcessPixel(INT X,INT Y,const FVector4& Vertex,UBOOL BackFacing)
	{
		if(!BackFacing && Vertex.Z < HitState.SurfaceZ)
		{
			HitState.SurfaceZ = Vertex.Z;
			HitState.SurfaceIndex = SurfaceIndex;
			HitState.bHitSurface = TRUE;
		}
	}

	INT GetMinX() const { return HitState.HitX; }
	INT GetMaxX() const { return HitState.HitX; }
	INT GetMinY() const { return HitState.HitY; }
	INT GetMaxY() const { return HitState.HitY; }

private:
	UINT SurfaceIndex;
	FModelHitState& HitState;
};

UBOOL HModel::ResolveSurface(const FSceneView* View,INT X,INT Y,UINT& OutSurfaceIndex) const
{
	FModelHitState HitState(X,Y);

	for(INT NodeIndex = 0;NodeIndex < Model->Nodes.Num();NodeIndex++)
	{
		FBspNode& Node = Model->Nodes(NodeIndex);
		FBspSurf& Surf = Model->Surfs(Node.iSurf);

		const UBOOL bIsPortal = (Surf.PolyFlags & PF_Portal) != 0;
		if(!bIsPortal)
		{
			// Convert the BSP node to a FPoly.
			FPoly NodePolygon;
			for(INT VertexIndex = 0;VertexIndex < Node.NumVertices;VertexIndex++)
			{
				NodePolygon.Vertices.AddItem(Model->Points(Model->Verts(Node.iVertPool + VertexIndex).pVertex));
			}

			// Clip the node's FPoly against the view's near clipping plane.
			if(	!View->bHasNearClippingPlane ||
				NodePolygon.Split(-FVector(View->NearClippingPlane),View->NearClippingPlane * View->NearClippingPlane.W))
			{
				for(INT LeadingVertexIndex = 2;LeadingVertexIndex < NodePolygon.Vertices.Num();LeadingVertexIndex++)
				{
					const INT TriangleVertexIndices[3] = { 0, LeadingVertexIndex, LeadingVertexIndex - 1 };
					FVector4 Vertices[3];
					for(UINT VertexIndex = 0;VertexIndex < 3;VertexIndex++)
					{
						FVector4 ScreenPosition = View->WorldToScreen(NodePolygon.Vertices(TriangleVertexIndices[VertexIndex]));
						FLOAT InvW = 1.0f / ScreenPosition.W;
						Vertices[VertexIndex] = FVector4(
							View->X + (0.5f + ScreenPosition.X * 0.5f * InvW) * View->SizeX,
							View->Y + (0.5f - ScreenPosition.Y * 0.5f * InvW) * View->SizeY,
							ScreenPosition.Z,
							ScreenPosition.W
							);
					}

					const FVector4 EdgeA = Vertices[2] - Vertices[0];
					const FVector4 EdgeB = Vertices[1] - Vertices[0];
					FTriangleRasterizer<FModelHitRasterPolicy> Rasterizer(FModelHitRasterPolicy((UINT)Node.iSurf,HitState));
					Rasterizer.DrawTriangle(
						Vertices[0],
						Vertices[1],
						Vertices[2],
						FVector2D(Vertices[0].X,Vertices[0].Y),
						FVector2D(Vertices[1].X,Vertices[1].Y),
						FVector2D(Vertices[2].X,Vertices[2].Y),
						(Surf.PolyFlags & PF_TwoSided) ? FALSE : IsNegativeFloat(EdgeA.X * EdgeB.Y - EdgeA.Y * EdgeB.X) // Check if a surface is twosided when it is selected in the editor viewport
						);
				}
			}
		}
	}

	OutSurfaceIndex = HitState.SurfaceIndex;
	return HitState.bHitSurface;
}
