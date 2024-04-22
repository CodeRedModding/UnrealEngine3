/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

static const INT DrawCollisionSides = 16;
static const INT DrawConeLimitSides = 40;

static const FLOAT DebugJointPosSize = 5.0f;
static const FLOAT DebugJointAxisSize = 20.0f;

static const FLOAT JointRenderSize = 5.0f;
static const FLOAT LimitRenderSize = 16.0f;

static const FColor JointUnselectedColor(255,0,255);
static const FColor	JointFrame1Color(255,0,0);
static const FColor JointFrame2Color(0,0,255);
static const FColor	JointLimitColor(0,255,0);
static const FColor	JointRefColor(255,255,0);
static const FColor JointLockedColor(255,128,10);

/////////////////////////////////////////////////////////////////////////////////////
// FKSphereElem
/////////////////////////////////////////////////////////////////////////////////////

// NB: ElemTM is assumed to have no scaling in it!
void FKSphereElem::DrawElemWire(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color)
{
	FVector Center = ElemTM.GetOrigin();
	FVector X = ElemTM.GetAxis(0);
	FVector Y = ElemTM.GetAxis(1);
	FVector Z = ElemTM.GetAxis(2);

	DrawCircle(PDI,Center, X, Y, Color, Scale*Radius, DrawCollisionSides, SDPG_World);
	DrawCircle(PDI,Center, X, Z, Color, Scale*Radius, DrawCollisionSides, SDPG_World);
	DrawCircle(PDI,Center, Y, Z, Color, Scale*Radius, DrawCollisionSides, SDPG_World);
}

void FKSphereElem::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FMaterialRenderProxy* MaterialRenderProxy)
{
	DrawSphere(PDI, ElemTM.GetOrigin(), FVector( this->Radius * Scale ), DrawCollisionSides, DrawCollisionSides/2, MaterialRenderProxy, SDPG_World );
}


/////////////////////////////////////////////////////////////////////////////////////
// FKBoxElem
/////////////////////////////////////////////////////////////////////////////////////

// NB: ElemTM is assumed to have no scaling in it!
void FKBoxElem::DrawElemWire(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color)
{
	FVector	B[2], P, Q, Radii;

	// X,Y,Z member variables are LENGTH not RADIUS
	Radii.X = Scale*0.5f*X;
	Radii.Y = Scale*0.5f*Y;
	Radii.Z = Scale*0.5f*Z;

	B[0] = Radii; // max
	B[1] = -1.0f * Radii; // min

	for( INT i=0; i<2; i++ )
	{
		for( INT j=0; j<2; j++ )
		{
			P.X=B[i].X; Q.X=B[i].X;
			P.Y=B[j].Y; Q.Y=B[j].Y;
			P.Z=B[0].Z; Q.Z=B[1].Z;
			PDI->DrawLine( ElemTM.TransformFVector(P), ElemTM.TransformFVector(Q), Color, SDPG_World);

			P.Y=B[i].Y; Q.Y=B[i].Y;
			P.Z=B[j].Z; Q.Z=B[j].Z;
			P.X=B[0].X; Q.X=B[1].X;
			PDI->DrawLine( ElemTM.TransformFVector(P), ElemTM.TransformFVector(Q), Color, SDPG_World);

			P.Z=B[i].Z; Q.Z=B[i].Z;
			P.X=B[j].X; Q.X=B[j].X;
			P.Y=B[0].Y; Q.Y=B[1].Y;
			PDI->DrawLine( ElemTM.TransformFVector(P), ElemTM.TransformFVector(Q), Color, SDPG_World);
		}
	}

}

void FKBoxElem::DrawElemSolid(class FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FMaterialRenderProxy* MaterialRenderProxy)
{
	DrawBox(PDI, ElemTM, 0.5f * FVector(X, Y, Z), MaterialRenderProxy, SDPG_World );
}

/////////////////////////////////////////////////////////////////////////////////////
// FKSphylElem
/////////////////////////////////////////////////////////////////////////////////////

static void DrawHalfCircle(FPrimitiveDrawInterface* PDI, const FVector& Base, const FVector& X, const FVector& Y, const FColor Color, FLOAT Radius)
{
	FLOAT	AngleDelta = 2.0f * (FLOAT)PI / ((FLOAT)DrawCollisionSides);
	FVector	LastVertex = Base + X * Radius;

	for(INT SideIndex = 0; SideIndex < (DrawCollisionSides/2); SideIndex++)
	{
		FVector	Vertex = Base + (X * appCos(AngleDelta * (SideIndex + 1)) + Y * appSin(AngleDelta * (SideIndex + 1))) * Radius;
		PDI->DrawLine(LastVertex, Vertex, Color, SDPG_World);
		LastVertex = Vertex;
	}	
}

// NB: ElemTM is assumed to have no scaling in it!
void FKSphylElem::DrawElemWire(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FColor Color)
{
	FVector Origin = ElemTM.GetOrigin();
	FVector XAxis = ElemTM.GetAxis(0);
	FVector YAxis = ElemTM.GetAxis(1);
	FVector ZAxis = ElemTM.GetAxis(2);

	// Draw top and bottom circles
	FVector TopEnd = Origin + Scale*0.5f*Length*ZAxis;
	FVector BottomEnd = Origin - Scale*0.5f*Length*ZAxis;

	DrawCircle(PDI,TopEnd, XAxis, YAxis, Color, Scale*Radius, DrawCollisionSides, SDPG_World);
	DrawCircle(PDI,BottomEnd, XAxis, YAxis, Color, Scale*Radius, DrawCollisionSides, SDPG_World);

	// Draw domed caps
	DrawHalfCircle(PDI, TopEnd, YAxis, ZAxis, Color,Scale* Radius);
	DrawHalfCircle(PDI, TopEnd, XAxis, ZAxis, Color, Scale*Radius);

	FVector NegZAxis = -ZAxis;

	DrawHalfCircle(PDI, BottomEnd, YAxis, NegZAxis, Color, Scale*Radius);
	DrawHalfCircle(PDI, BottomEnd, XAxis, NegZAxis, Color, Scale*Radius);

	// Draw connecty lines
	PDI->DrawLine(TopEnd + Scale*Radius*XAxis, BottomEnd + Scale*Radius*XAxis, Color, SDPG_World);
	PDI->DrawLine(TopEnd - Scale*Radius*XAxis, BottomEnd - Scale*Radius*XAxis, Color, SDPG_World);
	PDI->DrawLine(TopEnd + Scale*Radius*YAxis, BottomEnd + Scale*Radius*YAxis, Color, SDPG_World);
	PDI->DrawLine(TopEnd - Scale*Radius*YAxis, BottomEnd - Scale*Radius*YAxis, Color, SDPG_World);
}

void FKSphylElem::DrawElemSolid(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, FLOAT Scale, const FMaterialRenderProxy* MaterialRenderProxy)
{
	const INT NumSides = DrawCollisionSides;
	const INT NumRings = (DrawCollisionSides/2) + 1;

	// The first/last arc are on top of each other.
	const INT NumVerts = (NumSides+1) * (NumRings+1);
	FDynamicMeshVertex* Verts = (FDynamicMeshVertex*)appMalloc( NumVerts * sizeof(FDynamicMeshVertex) );

	// Calculate verts for one arc
	FDynamicMeshVertex* ArcVerts = (FDynamicMeshVertex*)appMalloc( (NumRings+1) * sizeof(FDynamicMeshVertex) );

	for(INT RingIdx=0; RingIdx<NumRings+1; RingIdx++)
	{
		FDynamicMeshVertex* ArcVert = &ArcVerts[RingIdx];

		FLOAT Angle;
		FLOAT ZOffset;
		if( RingIdx <= DrawCollisionSides/4 )
		{
			Angle = ((FLOAT)RingIdx/(NumRings-1)) * PI;
			ZOffset = 0.5 * Scale * Length;
		}
		else
		{
			Angle = ((FLOAT)(RingIdx-1)/(NumRings-1)) * PI;
			ZOffset = -0.5 * Scale * Length;
		}

		// Note- unit sphere, so position always has mag of one. We can just use it for normal!		
		FVector SpherePos;
		SpherePos.X = 0.0f;
		SpherePos.Y = Scale * Radius * appSin(Angle);
		SpherePos.Z = Scale * Radius * appCos(Angle);

		ArcVert->Position = SpherePos + FVector(0,0,ZOffset);

		ArcVert->SetTangents(
			FVector(1,0,0),
			FVector(0.0f, -SpherePos.Z, SpherePos.Y),
			SpherePos
			);

		ArcVert->TextureCoordinate.X = 0.0f;
		ArcVert->TextureCoordinate.Y = ((FLOAT)RingIdx/NumRings);
	}

	// Then rotate this arc NumSides+1 times.
	for(INT SideIdx=0; SideIdx<NumSides+1; SideIdx++)
	{
		const FRotator ArcRotator(0, appTrunc(65535.f * ((FLOAT)SideIdx/NumSides)), 0);
		const FRotationMatrix ArcRot( ArcRotator );
		const FLOAT XTexCoord = ((FLOAT)SideIdx/NumSides);

		for(INT VertIdx=0; VertIdx<NumRings+1; VertIdx++)
		{
			INT VIx = (NumRings+1)*SideIdx + VertIdx;

			Verts[VIx].Position = ArcRot.TransformFVector( ArcVerts[VertIdx].Position );

			Verts[VIx].SetTangents(
				ArcRot.TransformNormal( ArcVerts[VertIdx].TangentX ),
				ArcRot.TransformNormal( ArcVerts[VertIdx].GetTangentY() ),
				ArcRot.TransformNormal( ArcVerts[VertIdx].TangentZ )
				);

			Verts[VIx].TextureCoordinate.X = XTexCoord;
			Verts[VIx].TextureCoordinate.Y = ArcVerts[VertIdx].TextureCoordinate.Y;
		}
	}

	FDynamicMeshBuilder MeshBuilder;
	{
		// Add all of the vertices to the mesh.
		for(INT VertIdx=0; VertIdx<NumVerts; VertIdx++)
		{
			MeshBuilder.AddVertex(Verts[VertIdx]);
		}

		// Add all of the triangles to the mesh.
		for(INT SideIdx=0; SideIdx<NumSides; SideIdx++)
		{
			const INT a0start = (SideIdx+0) * (NumRings+1);
			const INT a1start = (SideIdx+1) * (NumRings+1);

			for(INT RingIdx=0; RingIdx<NumRings; RingIdx++)
			{
				MeshBuilder.AddTriangle(a0start + RingIdx + 0, a1start + RingIdx + 0, a0start + RingIdx + 1);
				MeshBuilder.AddTriangle(a1start + RingIdx + 0, a1start + RingIdx + 1, a0start + RingIdx + 1);
			}
		}

	}
	MeshBuilder.Draw(PDI, ElemTM, MaterialRenderProxy, SDPG_World,0.f);


	appFree(Verts);
	appFree(ArcVerts);
}

/////////////////////////////////////////////////////////////////////////////////////
// FKConvexElem
/////////////////////////////////////////////////////////////////////////////////////

/** See if the supplied vector is parallel to one of the edge directions in the convex element. */
UBOOL FKConvexElem::DirIsFaceEdge(const FVector& InDir) const
{
	FVector UnitDir = InDir.SafeNormal();

	for(INT i=0; i<EdgeDirections.Num(); i++)
	{
		FLOAT EdgeDot = UnitDir | EdgeDirections(i);
		if(Abs(1.f - Abs(EdgeDot)) < 0.01f)
		{
			return TRUE;
		}
	}

	return FALSE;
}

// NB: ElemTM is assumed to have no scaling in it!
void FKConvexElem::DrawElemWire(FPrimitiveDrawInterface* PDI, const FMatrix& ElemTM, const FVector& Scale3D, const FColor Color)
{
	FMatrix LocalToWorld = FScaleMatrix(Scale3D) * ElemTM;

	// Transform all convex verts into world space
	TArray<FVector> TransformedVerts;
	TransformedVerts.Add(VertexData.Num());
	for(INT i=0; i<VertexData.Num(); i++)
	{
		TransformedVerts(i) = LocalToWorld.TransformFVector( VertexData(i) );
	}

	// Draw each triangle that makes up the convex hull
	INT NumTris = FaceTriData.Num()/3;
	for(INT i=0; i<NumTris; i++)
	{
		// Get the verts that make up this triangle.
		const INT I0 = FaceTriData((i*3)+0);
		const INT I1 = FaceTriData((i*3)+1);
		const INT I2 = FaceTriData((i*3)+2);

		// To try and keep things simpler, we only draw edges which are parallel to the convex edge directions.

		if( DirIsFaceEdge(VertexData(I0)-VertexData(I1)) )
			PDI->DrawLine( TransformedVerts(I0), TransformedVerts(I1), Color, SDPG_World );

		if( DirIsFaceEdge(VertexData(I1)-VertexData(I2)) )
			PDI->DrawLine( TransformedVerts(I1), TransformedVerts(I2), Color, SDPG_World );

		if( DirIsFaceEdge(VertexData(I2)-VertexData(I0)) )
			PDI->DrawLine( TransformedVerts(I2), TransformedVerts(I0), Color, SDPG_World );
	}


#if 0
	// Draw edge directions
	FMatrix LocalToWorldTA = LocalToWorld.TransposeAdjoint();
	FLOAT Det = LocalToWorld.Determinant();
	
	for(INT i=0; i<EdgeDirections.Num(); i++)
	{
		FVector Dir = LocalToWorldTA.TransformNormal(EdgeDirections(i)).SafeNormal();
		if(Det < 0.f)
		{
			Dir = -Dir;
		}

		PDI->DrawLine(LocalToWorld.GetOrigin(), LocalToWorld.GetOrigin() + (30.f * Dir), FColor(200,200,255), SDPG_World);
	}
#endif

#if 0
	// Draw planes.
	FMatrix LocalToWorldTA = LocalToWorld.TransposeAdjoint();
	FLOAT Det = LocalToWorld.Determinant();

	for(INT i=0; i<FacePlaneData.Num(); i++)
	{
		const FPlane& LocalPlane = FacePlaneData(i);
		FVector LocalPlanePoint = (LocalPlane * LocalPlane.W);
		FVector WorldPlanePoint = LocalToWorld.TransformFVector(LocalPlanePoint);
		FVector WorldPlaneNormal = LocalToWorldTA.TransformNormal(LocalPlane).SafeNormal();
		FVector WorldNormalEnd = WorldPlanePoint + (30.f * WorldPlaneNormal);

		PDI->DrawLine( WorldPlanePoint, WorldNormalEnd, FColor(255, 180, 0), SDPG_World );
	}
#endif
}

void FKConvexElem::AddCachedSolidConvexGeom(TArray<FDynamicMeshVertex>& VertexBuffer, TArray<INT>& IndexBuffer, const FColor VertexColor)
{
	INT StartVertOffset = VertexBuffer.Num();

	// Draw each triangle that makes up the convex hull
	INT NumTris = FaceTriData.Num()/3;
	for(INT i=0; i<NumTris; i++)
	{
		// Get the verts that make up this triangle.
		const INT I0 = FaceTriData((i*3)+0);
		const INT I1 = FaceTriData((i*3)+1);
		const INT I2 = FaceTriData((i*3)+2);

		const FVector Edge0 = VertexData(I1)-VertexData(I0);
		const FVector Edge1 = VertexData(I2)-VertexData(I1);
		const FVector Normal = Edge1 ^ Edge0;

		for(INT j=0; j<3; j++)
		{
			FDynamicMeshVertex Vert;
			Vert.Position = VertexData( FaceTriData((i*3)+j) );
			Vert.Color = VertexColor;
			Vert.SetTangents(
				Edge0.SafeNormal(),
				(Normal ^ Edge0).SafeNormal(),
				Normal.SafeNormal()
				);
			VertexBuffer.AddItem(Vert);
		}

		IndexBuffer.AddItem(StartVertOffset+(i*3)+0);
		IndexBuffer.AddItem(StartVertOffset+(i*3)+1);
		IndexBuffer.AddItem(StartVertOffset+(i*3)+2);
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// FKAggregateGeom
/////////////////////////////////////////////////////////////////////////////////////


void FConvexCollisionVertexBuffer::InitRHI()
{
	VertexBufferRHI = RHICreateVertexBuffer(Vertices.Num() * sizeof(FDynamicMeshVertex),NULL,RUF_Static);

	// Copy the vertex data into the vertex buffer.
	void* VertexBufferData = RHILockVertexBuffer(VertexBufferRHI,0,Vertices.Num() * sizeof(FDynamicMeshVertex), FALSE);
	appMemcpy(VertexBufferData,&Vertices(0),Vertices.Num() * sizeof(FDynamicMeshVertex));
	RHIUnlockVertexBuffer(VertexBufferRHI);
}

void FConvexCollisionIndexBuffer::InitRHI()
{
	IndexBufferRHI = RHICreateIndexBuffer(sizeof(INT),Indices.Num() * sizeof(INT),NULL,RUF_Static);

	// Write the indices to the index buffer.
	void* Buffer = RHILockIndexBuffer(IndexBufferRHI,0,Indices.Num() * sizeof(INT));
	appMemcpy(Buffer,&Indices(0),Indices.Num() * sizeof(INT));
	RHIUnlockIndexBuffer(IndexBufferRHI);
}

void FConvexCollisionVertexFactory::InitConvexVertexFactory(const FConvexCollisionVertexBuffer* VertexBuffer)
{
	if(IsInRenderingThread())
	{
		// Initialize the vertex factory's stream components.
		DataType NewData;
		NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
		NewData.TextureCoordinates.AddItem(
			FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
			);
		NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
		NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
		SetData(NewData);
	}
	else
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			InitConvexCollisionVertexFactory,
			FConvexCollisionVertexFactory*,VertexFactory,this,
			const FConvexCollisionVertexBuffer*,VertexBuffer,VertexBuffer,
			{
				// Initialize the vertex factory's stream components.
				DataType NewData;
				NewData.PositionComponent = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,Position,VET_Float3);
				NewData.TextureCoordinates.AddItem(
					FVertexStreamComponent(VertexBuffer,STRUCT_OFFSET(FDynamicMeshVertex,TextureCoordinate),sizeof(FDynamicMeshVertex),VET_Float2)
					);
				NewData.TangentBasisComponents[0] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentX,VET_PackedNormal);
				NewData.TangentBasisComponents[1] = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(VertexBuffer,FDynamicMeshVertex,TangentZ,VET_PackedNormal);
				VertexFactory->SetData(NewData);
			});
	}
}

// NB: ParentTM is assumed to have no scaling in it!
void FKAggregateGeom::DrawAggGeom(FPrimitiveDrawInterface* PDI, const FMatrix& ParentTM, const FVector& Scale3D, const FColor Color, const FMaterialRenderProxy* MatInst, UBOOL bPerHullColor, UBOOL bDrawSolid)
{
	if( Scale3D.IsUniform() )
	{
		for(INT i=0; i<SphereElems.Num(); i++)
		{
			FMatrix ElemTM = SphereElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= ParentTM;
			if(bDrawSolid)
				SphereElems(i).DrawElemSolid(PDI, ElemTM, Scale3D.X, MatInst);
			else
				SphereElems(i).DrawElemWire(PDI, ElemTM, Scale3D.X, Color);
		}

		for(INT i=0; i<BoxElems.Num(); i++)
		{
			FMatrix ElemTM = BoxElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= ParentTM;
			if(bDrawSolid)
				BoxElems(i).DrawElemSolid(PDI, ElemTM, Scale3D.X, MatInst);
			else
				BoxElems(i).DrawElemWire(PDI, ElemTM, Scale3D.X, Color);
		}

		for(INT i=0; i<SphylElems.Num(); i++)
		{
			FMatrix ElemTM = SphylElems(i).TM;
			ElemTM.ScaleTranslation(Scale3D);
			ElemTM *= ParentTM;
			if(bDrawSolid)
				SphylElems(i).DrawElemSolid(PDI, ElemTM, Scale3D.X, MatInst);
			else
				SphylElems(i).DrawElemWire(PDI, ElemTM, Scale3D.X, Color);
		}
	}

	if(ConvexElems.Num() > 0)
	{
		if(bDrawSolid)
		{
			// Cache collision vertex/index buffer
			if(!RenderInfo)
			{
				RenderInfo = new FKConvexGeomRenderInfo();
				RenderInfo->VertexBuffer = new FConvexCollisionVertexBuffer();
				RenderInfo->IndexBuffer = new FConvexCollisionIndexBuffer();

				for(INT i=0; i<ConvexElems.Num(); i++)
				{
					// Get vertices/triangles from this hull.
					ConvexElems(i).AddCachedSolidConvexGeom(RenderInfo->VertexBuffer->Vertices, RenderInfo->IndexBuffer->Indices, FColor(255,255,255));
				}

				RenderInfo->VertexBuffer->InitResource();
				RenderInfo->IndexBuffer->InitResource();
                
				RenderInfo->CollisionVertexFactory = new FConvexCollisionVertexFactory(RenderInfo->VertexBuffer);
				RenderInfo->CollisionVertexFactory->InitResource();
			}

			// Calculate matrix
			FMatrix LocalToWorld = FScaleMatrix(Scale3D) * ParentTM;

			// Draw the mesh.
			FMeshBatch Mesh;
			FMeshBatchElement& BatchElement = Mesh.Elements(0);
			BatchElement.IndexBuffer = RenderInfo->IndexBuffer;
			Mesh.VertexFactory = RenderInfo->CollisionVertexFactory;
			Mesh.MaterialRenderProxy = MatInst;
			BatchElement.LocalToWorld = LocalToWorld;
			BatchElement.WorldToLocal = LocalToWorld.Inverse();
			// previous l2w not used so treat as static
			BatchElement.FirstIndex = 0;
			BatchElement.NumPrimitives = RenderInfo->IndexBuffer->Indices.Num() / 3;
			BatchElement.MinVertexIndex = 0;
			BatchElement.MaxVertexIndex = RenderInfo->VertexBuffer->Vertices.Num() - 1;
			Mesh.ReverseCulling = LocalToWorld.Determinant() < 0.0f ? TRUE : FALSE;
			Mesh.Type = PT_TriangleList;
			Mesh.DepthPriorityGroup = SDPG_World;
			Mesh.bUsePreVertexShaderCulling = FALSE;
			Mesh.PlatformMeshData       = NULL;
			PDI->DrawMesh(Mesh);
		}
		else
		{
			for(INT i=0; i<ConvexElems.Num(); i++)
			{
				FColor ConvexColor = bPerHullColor ? DebugUtilColor[i%NUM_DEBUG_UTIL_COLORS] : Color;
				ConvexElems(i).DrawElemWire(PDI, ParentTM, Scale3D, ConvexColor);
			}
		}
	}
}

/** Release the RenderInfo (if its there) and safely clean up any resources. Call on the game thread. */
void FKAggregateGeom::FreeRenderInfo()
{
	// See if we have rendering resources to free
	if(RenderInfo)
	{
		// Should always have these if RenderInfo exists
		check(RenderInfo->VertexBuffer);
		check(RenderInfo->IndexBuffer);
		check(RenderInfo->CollisionVertexFactory);

		// Fire off commands to free these resources
		BeginReleaseResource(RenderInfo->VertexBuffer);
		BeginReleaseResource(RenderInfo->IndexBuffer);
		BeginReleaseResource(RenderInfo->CollisionVertexFactory);

		// Wait until those commands have been processed
		FRenderCommandFence Fence;
		Fence.BeginFence();
		Fence.Wait();

		// Release memory.
		delete RenderInfo->VertexBuffer;
		delete RenderInfo->IndexBuffer;
		delete RenderInfo->CollisionVertexFactory;
		delete RenderInfo;
		RenderInfo = NULL;
	}
}

/////////////////////////////////////////////////////////////////////////////////////
// UPhysicsAsset
/////////////////////////////////////////////////////////////////////////////////////

FMatrix GetSkelBoneMatrix(INT BoneIndex, const TArray<FBoneAtom>& SpaceBases, const FMatrix& LocalToWorld)
{
	if(BoneIndex != INDEX_NONE && BoneIndex < SpaceBases.Num())
	{
		return SpaceBases(BoneIndex).ToMatrix() * LocalToWorld;
	}
	else
	{
		return FMatrix::Identity;
	}
}

void UPhysicsAsset::DrawCollision(FPrimitiveDrawInterface* PDI, const USkeletalMesh* SkelMesh, const TArray<FBoneAtom>& SpaceBases, const FMatrix& LocalToWorld, FLOAT Scale)
{
	for( INT i=0; i<BodySetup.Num(); i++)
	{
		INT BoneIndex = SkelMesh->MatchRefBone( BodySetup(i)->BoneName );
		
		FColor* BoneColor = (FColor*)( &BodySetup(i) );

		FMatrix BoneMatrix = GetSkelBoneMatrix(BoneIndex, SpaceBases, LocalToWorld);
		BoneMatrix.RemoveScaling();

		BodySetup(i)->AggGeom.DrawAggGeom( PDI, BoneMatrix, FVector(Scale, Scale, Scale), *BoneColor, NULL, FALSE, FALSE );
	}
}

void UPhysicsAsset::DrawConstraints(FPrimitiveDrawInterface* PDI, const USkeletalMesh* SkelMesh, const TArray<FBoneAtom>& SpaceBases, const FMatrix& LocalToWorld, FLOAT Scale)
{
	for( INT i=0; i<ConstraintSetup.Num(); i++ )
	{
		URB_ConstraintSetup* cs = ConstraintSetup(i);

		// Get each constraint frame in world space.
		FMatrix Con1Frame = FMatrix::Identity;
		INT Bone1Index = SkelMesh->MatchRefBone(cs->ConstraintBone1);
		if(Bone1Index != INDEX_NONE)
		{	
			FMatrix Body1TM = GetSkelBoneMatrix(Bone1Index, SpaceBases, LocalToWorld);
			Body1TM.RemoveScaling();
			Con1Frame = cs->GetRefFrameMatrix(0) * Body1TM;
		}

		FMatrix Con2Frame = FMatrix::Identity;
		INT Bone2Index = SkelMesh->MatchRefBone(cs->ConstraintBone2);
		if(Bone2Index != INDEX_NONE)
		{	
			FMatrix Body2TM = GetSkelBoneMatrix(Bone2Index, SpaceBases, LocalToWorld);
			Body2TM.RemoveScaling();
			Con2Frame = cs->GetRefFrameMatrix(1) * Body2TM;
		}

#if GEMINI_TODO
		if(!SkelComp->LimitMaterial)
		{
			SkelComp->LimitMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_None, NULL);
		}

		cs->DrawConstraint(PDI, Scale, true, true, SkelComp->LimitMaterial, Con1Frame, Con2Frame, FALSE);
#endif
	}
}
/////////////////////////////////////////////////////////////////////////////////////
// URB_ConstraintSetup
/////////////////////////////////////////////////////////////////////////////////////

static void DrawOrientedStar(FPrimitiveDrawInterface* PDI, const FMatrix& Matrix, FLOAT Size, const FColor Color)
{
	FVector Position = Matrix.GetOrigin();

	PDI->DrawLine(Position + Size * Matrix.GetAxis(0), Position - Size * Matrix.GetAxis(0), Color, SDPG_World);
	PDI->DrawLine(Position + Size * Matrix.GetAxis(1), Position - Size * Matrix.GetAxis(1), Color, SDPG_World);
	PDI->DrawLine(Position + Size * Matrix.GetAxis(2), Position - Size * Matrix.GetAxis(2), Color, SDPG_World);
}

static void DrawArc(const FVector& Base, const FVector& X, const FVector& Y, FLOAT MinAngle, FLOAT MaxAngle, FLOAT Radius, INT Sections, const FColor Color, FPrimitiveDrawInterface* PDI)
{
	FLOAT AngleStep = (MaxAngle - MinAngle)/((FLOAT)(Sections));
	FLOAT CurrentAngle = MinAngle;

	FVector LastVertex = Base + Radius * ( appCos(CurrentAngle * (PI/180.0f)) * X + appSin(CurrentAngle * (PI/180.0f)) * Y );
	CurrentAngle += AngleStep;

	for(INT i=0; i<Sections; i++)
	{
		FVector ThisVertex = Base + Radius * ( appCos(CurrentAngle * (PI/180.0f)) * X + appSin(CurrentAngle * (PI/180.0f)) * Y );
		PDI->DrawLine( LastVertex, ThisVertex, Color, SDPG_World );
		LastVertex = ThisVertex;
		CurrentAngle += AngleStep;
	}
}

static void DrawLinearLimit(FPrimitiveDrawInterface* PDI, const FVector& Origin, const FVector& Axis, const FVector& Orth, FLOAT LinearLimitRadius, UBOOL bLinearLimited, FLOAT DrawScale)
{
	FLOAT ScaledLimitSize = LimitRenderSize * DrawScale;

	if(bLinearLimited)
	{
		FVector Start = Origin - LinearLimitRadius * Axis;
		FVector End = Origin + LinearLimitRadius * Axis;

		PDI->DrawLine(  Start, End, JointLimitColor, SDPG_World );

		// Draw ends indicating limit.
		PDI->DrawLine(  Start - (0.2f * ScaledLimitSize * Orth), Start + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
		PDI->DrawLine(  End - (0.2f * ScaledLimitSize * Orth), End + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
	}
	else
	{
		FVector Start = Origin - 1.5f * ScaledLimitSize * Axis;
		FVector End = Origin + 1.5f * ScaledLimitSize * Axis;

		PDI->DrawLine(  Start, End, JointRefColor, SDPG_World );

		// Draw arrow heads.
		PDI->DrawLine(  Start, Start + (0.2f * ScaledLimitSize * Axis) + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
		PDI->DrawLine(  Start, Start + (0.2f * ScaledLimitSize * Axis) - (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );

		PDI->DrawLine(  End, End - (0.2f * ScaledLimitSize * Axis) + (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
		PDI->DrawLine(  End, End - (0.2f * ScaledLimitSize * Axis) - (0.2f * ScaledLimitSize * Orth), JointLimitColor, SDPG_World );
	}
}

void URB_ConstraintSetup::DrawConstraint(FPrimitiveDrawInterface* PDI, 
										 FLOAT Scale, FLOAT LimitDrawScale, UBOOL bDrawLimits, UBOOL bDrawSelected, UMaterialInterface* LimitMaterial,
										 const FMatrix& Con1Frame, const FMatrix& Con2Frame, UBOOL bDrawAsPoint)
{

	FVector Con1Pos = Con1Frame.GetOrigin();
	FVector Con2Pos = Con2Frame.GetOrigin();

	FLOAT ScaledLimitSize = LimitRenderSize * LimitDrawScale;

	// Special mode for drawing joints just as points..
	if(bDrawAsPoint)
	{
		if(bDrawSelected)
		{
			PDI->DrawPoint( Con1Frame.GetOrigin(), JointFrame1Color, 3.f, SDPG_World );
			PDI->DrawPoint( Con2Frame.GetOrigin(), JointFrame2Color, 3.f, SDPG_World );
		}
		else
		{
			PDI->DrawPoint( Con1Frame.GetOrigin(), JointUnselectedColor, 3.f, SDPG_World );
			PDI->DrawPoint( Con2Frame.GetOrigin(), JointUnselectedColor, 3.f, SDPG_World );
		}

		// do nothing else in this mode.
		return;
	}

	if(bDrawSelected)
	{
		DrawOrientedStar( PDI, Con1Frame, LimitDrawScale * JointRenderSize, JointFrame1Color );
		DrawOrientedStar( PDI, Con2Frame, LimitDrawScale * JointRenderSize, JointFrame2Color );
	}
	else
	{
		DrawOrientedStar( PDI, Con1Frame, LimitDrawScale * JointRenderSize, JointUnselectedColor );
		DrawOrientedStar( PDI, Con2Frame, LimitDrawScale * JointRenderSize, JointUnselectedColor );
	}

	//////////////////////////////////////////////////////////////////////////
	// LINEAR DRAWING

	UBOOL bLinearXLocked = LinearXSetup.bLimited && (LinearXSetup.LimitSize < RB_MinSizeToLockDOF);
	UBOOL bLinearYLocked = LinearYSetup.bLimited && (LinearYSetup.LimitSize < RB_MinSizeToLockDOF);
	UBOOL bLinearZLocked = LinearZSetup.bLimited && (LinearZSetup.LimitSize < RB_MinSizeToLockDOF);

	// Limit is max of all limit sizes.
	FLOAT LinearLimitRadius = 0.f;
	if( LinearXSetup.bLimited && (LinearXSetup.LimitSize > LinearLimitRadius) )
		LinearLimitRadius = LinearXSetup.LimitSize;
	if( LinearYSetup.bLimited && (LinearYSetup.LimitSize > LinearLimitRadius) )
		LinearLimitRadius = LinearYSetup.LimitSize;
	if( LinearZSetup.bLimited && (LinearZSetup.LimitSize > LinearLimitRadius) )
		LinearLimitRadius = LinearZSetup.LimitSize;

	UBOOL bLinearLimited = false;
	if( LinearLimitRadius > RB_MinSizeToLockDOF )
		bLinearLimited = true;

	if(!bLinearXLocked)
	{
        DrawLinearLimit(PDI, Con2Frame.GetOrigin(), Con2Frame.GetAxis(0), Con2Frame.GetAxis(2), LinearXSetup.LimitSize, bLinearLimited, LimitDrawScale);
	}


	if(!bLinearYLocked)
	{
        DrawLinearLimit(PDI, Con2Frame.GetOrigin(), Con2Frame.GetAxis(1), Con2Frame.GetAxis(2), LinearYSetup.LimitSize, bLinearLimited, LimitDrawScale);
	}


	if(!bLinearZLocked)
	{
		DrawLinearLimit(PDI, Con2Frame.GetOrigin(), Con2Frame.GetAxis(2), Con2Frame.GetAxis(0), LinearZSetup.LimitSize, bLinearLimited, LimitDrawScale);
	}


	if(!bDrawLimits)
		return;


	//////////////////////////////////////////////////////////////////////////
	// ANGULAR DRAWING

	UBOOL bLockTwist = bTwistLimited && (TwistLimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockSwing1 = bSwingLimited && (Swing1LimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockSwing2 = bSwingLimited && (Swing2LimitAngle < RB_MinAngleToLockDOF);
	UBOOL bLockAllSwing = bLockSwing1 && bLockSwing2;

	UBOOL bDrawnAxisLine = false;
	FVector RefLineEnd = Con1Frame.GetOrigin() + (1.2f * ScaledLimitSize * Con1Frame.GetAxis(0));

	// If swing is limited (but not locked) - draw the limit cone.
	if(bSwingLimited)
	{
		FMatrix ConeLimitTM;
		ConeLimitTM = Con2Frame;
		ConeLimitTM.SetOrigin( Con1Frame.GetOrigin() );

		if(bLockAllSwing)
		{
			// Draw little red 'V' to indicate locked swing.
			PDI->DrawLine( ConeLimitTM.GetOrigin(), ConeLimitTM.TransformFVector( 0.3f * ScaledLimitSize * FVector(1,1,0) ), JointLockedColor, SDPG_World);
			PDI->DrawLine( ConeLimitTM.GetOrigin(), ConeLimitTM.TransformFVector( 0.3f * ScaledLimitSize * FVector(1,-1,0) ), JointLockedColor, SDPG_World);
		}
		else
		{
			FLOAT ang1 = Swing1LimitAngle;
			if(ang1 < RB_MinAngleToLockDOF)
				ang1 = 0.f;
			else
				ang1 *= ((FLOAT)PI/180.f); // convert to radians

			FLOAT ang2 = Swing2LimitAngle;
			if(ang2 < RB_MinAngleToLockDOF)
				ang2 = 0.f;
			else
				ang2 *= ((FLOAT)PI/180.f);

			FMatrix ConeToWorld = FScaleMatrix( FVector(ScaledLimitSize) ) * ConeLimitTM;
			DrawCone(PDI, ConeToWorld, ang1, ang2, DrawConeLimitSides, true, JointLimitColor, LimitMaterial->GetRenderProxy(false), SDPG_World );

			// Draw reference line
			PDI->DrawLine( Con1Frame.GetOrigin(), RefLineEnd, JointRefColor, SDPG_World );
			bDrawnAxisLine = true;
		}
	}

	// If twist is limited, but not completely locked, draw 
	if(bTwistLimited)
	{
		if(bLockTwist)
		{
			// If there is no axis line draw already - add a little one now to sit the 'twist locked' cross on
			if(!bDrawnAxisLine)
				PDI->DrawLine( Con1Frame.GetOrigin(), RefLineEnd, JointLockedColor, SDPG_World );

			PDI->DrawLine(  RefLineEnd + Con1Frame.TransformNormal( 0.3f * ScaledLimitSize * FVector(0.f,-0.5f,-0.5f) ), 
							RefLineEnd + Con1Frame.TransformNormal( 0.3f * ScaledLimitSize * FVector(0.f, 0.5f, 0.5f) ), JointLockedColor, SDPG_World);

			PDI->DrawLine(  RefLineEnd + Con1Frame.TransformNormal( 0.3f * ScaledLimitSize * FVector(0.f, 0.5f,-0.5f) ), 
							RefLineEnd + Con1Frame.TransformNormal( 0.3f * ScaledLimitSize * FVector(0.f,-0.5f, 0.5f) ), JointLockedColor, SDPG_World);
		}
		else
		{
			// If no axis yet drawn - do it now
			if(!bDrawnAxisLine)
				PDI->DrawLine( Con1Frame.GetOrigin(), RefLineEnd, JointRefColor, SDPG_World );

			// Draw twist limit.
			FVector ChildTwistRef = Con1Frame.GetAxis(1);
			FVector ParentTwistRef = Con2Frame.GetAxis(1);
			PDI->DrawLine( RefLineEnd, RefLineEnd + ChildTwistRef * ScaledLimitSize, JointRefColor, SDPG_World );

			// Rotate parent twist ref axis
			FQuat ParentToChildRot = FQuatFindBetween( Con2Frame.GetAxis(0), Con1Frame.GetAxis(0) );
			FVector ChildTwistLimit = ParentToChildRot.RotateVector( ParentTwistRef );

			FQuat RotateLimit = FQuat( Con1Frame.GetAxis(0), TwistLimitAngle * (PI/180.0f) );
			FVector WLimit = RotateLimit.RotateVector(ChildTwistLimit);
			PDI->DrawLine( RefLineEnd, RefLineEnd + WLimit * ScaledLimitSize, JointLimitColor, SDPG_World );

			RotateLimit = FQuat( Con1Frame.GetAxis(0), -TwistLimitAngle * (PI/180.0f) );
			WLimit = RotateLimit.RotateVector(ChildTwistLimit);
			PDI->DrawLine( RefLineEnd, RefLineEnd + WLimit * ScaledLimitSize, JointLimitColor, SDPG_World );

			DrawArc(RefLineEnd, ChildTwistLimit, -ChildTwistLimit ^ Con1Frame.GetAxis(0), -TwistLimitAngle, TwistLimitAngle, 0.8f * ScaledLimitSize, 8, JointLimitColor, PDI);
		}
	}
	else
	{
		// For convenience, in the hinge case where swing is locked and twist is unlimited, draw the twist axis.
		if(bLockAllSwing)
			PDI->DrawLine(  Con2Frame.GetOrigin() - ScaledLimitSize * Con2Frame.GetAxis(0), 
							Con2Frame.GetOrigin() + ScaledLimitSize * Con2Frame.GetAxis(0), JointLimitColor, SDPG_World );
	}


}

/////////////////////////////////////////////////////////////////////////////////////
// URB_ConstraintDrawComponent
/////////////////////////////////////////////////////////////////////////////////////

/** Represents a constraint to draw in the rendering thread. */
class FConstraintDrawSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** RB_ConstraintSetup to draw */
	URB_ConstraintSetup* Setup;

	/** Frame one of the constraint in the world ref frame. */
	FMatrix Con1Frame;

	/** Frame two of the constraint in the world ref frame. */
	FMatrix Con2Frame;

	/** Material to use for drawing joint limit surface. */
	UMaterialInterface* LimitMaterial;

	/** Bounding box of body1 */
	FBox Body1Box;

	/** Bounding box of body2 */
	FBox Body2Box;
	
	/** Constructor copies important information into scene proxy. */
	FConstraintDrawSceneProxy(const URB_ConstraintDrawComponent* InComponent):
		FPrimitiveSceneProxy(InComponent)
	{
		ARB_ConstraintActor* CA = Cast<ARB_ConstraintActor>(InComponent->GetOwner());
		check(CA);
		check(CA->ConstraintSetup);

		Setup = CA->ConstraintSetup;
		Con1Frame = Setup->GetRefFrameMatrix(0) * FindBodyMatrix(CA->ConstraintActor1, Setup->ConstraintBone1);
		Con2Frame = Setup->GetRefFrameMatrix(1) * FindBodyMatrix(CA->ConstraintActor2, Setup->ConstraintBone2);

		LimitMaterial = InComponent->LimitMaterial;
		if(!LimitMaterial)
		{
			LimitMaterial = LoadObject<UMaterialInterface>(NULL, TEXT("EditorMaterials.PhAT_JointLimitMaterial"), NULL, LOAD_None, NULL);
		}

		Body1Box = FindBodyBox(CA->ConstraintActor1, Setup->ConstraintBone1);
		Body2Box = FindBodyBox(CA->ConstraintActor2, Setup->ConstraintBone2);
	}


	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags)
	{
		// Draw constraint information
		Setup->DrawConstraint(PDI, 1.f, 1.f, TRUE, TRUE, LimitMaterial, Con1Frame, Con2Frame, FALSE);

		// Draw boxes to indicate bodies connected by joint.
		if(Body1Box.IsValid)
		{
			PDI->DrawLine( Con1Frame.GetOrigin(), Body1Box.GetCenter(), JointFrame1Color, SDPG_World );
			DrawWireBox(PDI, Body1Box, JointFrame1Color, SDPG_World );
		}

		if(Body2Box.IsValid)
		{
			PDI->DrawLine( Con2Frame.GetOrigin(), Body2Box.GetCenter(), JointFrame2Color, SDPG_World );
			DrawWireBox(PDI, Body2Box, JointFrame2Color, SDPG_World );
		}
	}

	/** Function that determines if this scene proxy should be drawn in this scene. */
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View)
	{
		const UBOOL bVisible = (View->Family->ShowFlags & SHOW_Constraints) ? TRUE : FALSE;
		const UBOOL bRelevant = IsShown(View) && bVisible;
		FPrimitiveViewRelevance Result;
		Result.bDynamicRelevance = bRelevant;
		Result.bTranslucentRelevance = bRelevant;
		Result.SetDPG(SDPG_World,TRUE);
		if (IsShadowCast(View))
		{
			Result.bShadowRelevance = TRUE;
		}
		return Result;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }
};

/** Create a constraint drawing proxy. */
FPrimitiveSceneProxy* URB_ConstraintDrawComponent::CreateSceneProxy()
{
	return new FConstraintDrawSceneProxy(this);
}

 /**
  * Update the bounds of the component.
  */
 void URB_ConstraintDrawComponent::UpdateBounds()
 {
	 // The component doesn't really have a legitimate bounds, so just set it to zero
	Bounds.Origin = LocalToWorld.GetOrigin();
	Bounds.SphereRadius = 0.0f;
	Bounds.BoxExtent.X = Bounds.BoxExtent.Y = Bounds.BoxExtent.Z = 0.0f;
 }

/** 
 * Retrieves the materials used in this component 
 *  
 * @param OutMaterials	The list of used materials.
 */
 void URB_ConstraintDrawComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
 {
	 OutMaterials.AddItem( LimitMaterial );
 }

