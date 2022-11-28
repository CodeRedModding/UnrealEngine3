/*=============================================================================
	UnSkeletalRender.cpp: Skeletal mesh skinning/rendering code.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
		* Vectorized versions of CacheVertices by Daniel Vogel
		* Optimizations by Bruce Dawson
		* Split into own file by James Golding
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAnimClasses.h"
#include "UnSkeletalRender.h"


//
//	FFinalSkinVertexBuffer::GetData
//

void FFinalSkinVertexBuffer::GetData(void* Buffer)
{
	if(SkeletalMeshComponent->MeshObject->CachedVertexLOD != LOD)
		SkeletalMeshComponent->MeshObject->CacheVertices(LOD);

	appMemcpy(Buffer,&SkeletalMeshComponent->MeshObject->CachedFinalVertices(0),Size);

}

//
//	FSkinShadowVertexBuffer::GetData
//

void FSkinShadowVertexBuffer::GetData(void* Buffer)
{
	if(SkeletalMeshComponent->MeshObject->CachedVertexLOD != LOD)
		SkeletalMeshComponent->MeshObject->CacheVertices(LOD);

	FSkinShadowVertex*				DestVertex = (FSkinShadowVertex*)Buffer;
	const TArray<FFinalSkinVertex>&	SrcVertices = SkeletalMeshComponent->MeshObject->CachedFinalVertices;

	for(UINT VertexIndex = 0;VertexIndex < (UINT)SrcVertices.Num();VertexIndex++)
		*DestVertex++ = FSkinShadowVertex(SrcVertices(VertexIndex).Position,0);

	for(UINT VertexIndex = 0;VertexIndex < (UINT)SrcVertices.Num();VertexIndex++)
		*DestVertex++ = FSkinShadowVertex(SrcVertices(VertexIndex).Position,1);

}


//
//	USkeletalMeshComponent::Render
//

void USkeletalMeshComponent::Render(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI)
{
	if(MeshObject && !bHideSkin)
	{
		INT	LODLevel = GetLODLevel(Context);

		if(Context.View->ViewMode & SVM_WireframeMask)
		{			
			for(UINT SectionIndex = 0;SectionIndex < (UINT)SkeletalMesh->LODModels(LODLevel).Sections.Num();SectionIndex++)
			{
				FSkelMeshSection&	Section = SkeletalMesh->LODModels(LODLevel).Sections(SectionIndex);
				PRI->DrawWireframe(
					&MeshObject->LODs(LODLevel).VertexFactory,
					&SkeletalMesh->LODModels(LODLevel).IndexBuffer,
					WT_TriList,
					SelectedColor(Context,GEngine->C_AnimMesh),
					Section.FirstIndex,
					Section.TotalFaces,
					Section.MinIndex,
					Section.MaxIndex
					);
			}
		}
		else
		{	
			for(UINT SectionIndex = 0;SectionIndex < (UINT)SkeletalMesh->LODModels(LODLevel).Sections.Num();SectionIndex++)
			{
				FSkelMeshSection&	Section = SkeletalMesh->LODModels(LODLevel).Sections(SectionIndex);
				GEngineStats.SkeletalMeshTriangles.Value += Section.TotalFaces;
				PRI->DrawMesh(
					&MeshObject->LODs(LODLevel).VertexFactory,
					&SkeletalMesh->LODModels(LODLevel).IndexBuffer,
					SelectedMaterial(Context,GetMaterial(Section.MaterialIndex)),
					GetMaterial(Section.MaterialIndex)->GetInstanceInterface(),
					Section.FirstIndex,
					Section.TotalFaces,
					Section.MinIndex,
					Section.MaxIndex
					);
			}
		}
	}

	// Debug drawing for assets.

	if( PhysicsAsset )
	{
		FVector TotalScale;
		if(Owner)
			TotalScale = Owner->DrawScale * Owner->DrawScale3D;
		else
			TotalScale = FVector(1.f);

		// Only valid if scaling if uniform.
		if( TotalScale.IsUniform() )
		{
			if(Context.View->ShowFlags & SHOW_Collision)
				PhysicsAsset->DrawCollision(PRI, this, TotalScale.X);

			if(Context.View->ShowFlags & SHOW_Constraints)
				PhysicsAsset->DrawConstraints(PRI, this, TotalScale.X);
		}
	}
}

//
//	USkeletalMeshComponent::RenderForeground
//

void USkeletalMeshComponent::RenderForeground(const FSceneContext& Context,FPrimitiveRenderInterface* PRI)
{
	// Debug drawing of wire skeleton.
	if( bDisplayBones )
	{
		TArray<FMatrix> WorldBases;
		WorldBases.Add( SpaceBases.Num() );

		WorldBases(0) = SpaceBases(0) * LocalToWorld;

		for(INT i=0; i<SpaceBases.Num(); i++)
		{
			WorldBases(i) = SpaceBases(i) * LocalToWorld;

			if(i == 0)
			{
				PRI->DrawLine(WorldBases(i).GetOrigin(), LocalToWorld.GetOrigin(), FColor(255, 0, 255));
			}
			else
			{
				INT ParentIdx = SkeletalMesh->RefSkeleton(i).ParentIndex;
				PRI->DrawLine(WorldBases(i).GetOrigin(), WorldBases(ParentIdx).GetOrigin(), FColor(230, 230, 255));
			}

			// Display colored coordinate system axes for each joint.
			FVector XAxis =  WorldBases(i).TransformNormal( FVector(1.0f,0.0f,0.0f));
			XAxis.Normalize();
			PRI->DrawLine(  WorldBases(i).GetOrigin(),  WorldBases(i).GetOrigin() + XAxis * 3.75f, FColor( 255, 80, 80) ); // Red = X			
			FVector YAxis =  WorldBases(i).TransformNormal( FVector(0.0f,1.0f,0.0f));
			YAxis.Normalize();
			PRI->DrawLine(  WorldBases(i).GetOrigin(),  WorldBases(i).GetOrigin() + YAxis * 3.75f, FColor( 80, 255, 80) ); // Green = Y
			FVector ZAxis =  WorldBases(i).TransformNormal( FVector(0.0f,0.0f,1.0f));
			ZAxis.Normalize();
			PRI->DrawLine(  WorldBases(i).GetOrigin(),  WorldBases(i).GetOrigin() + ZAxis * 3.75f, FColor( 80, 80, 255) ); // Blue = Z
		}		
	}

	// Debug drawing of bounding primitives.
	if( (Context.View->ShowFlags & SHOW_Bounds) && (!GIsEditor || !Owner || GSelectionTools.IsSelected( Owner ) ) )
	{
		// Draw bounding wireframe box.
		PRI->DrawWireBox( Bounds.GetBox(), FColor(72,72,255));

		PRI->DrawCircle(Bounds.Origin,FVector(1,0,0),FVector(0,1,0),FColor(255,255,0),Bounds.SphereRadius,32);
		PRI->DrawCircle(Bounds.Origin,FVector(1,0,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32);
		PRI->DrawCircle(Bounds.Origin,FVector(0,1,0),FVector(0,0,1),FColor(255,255,0),Bounds.SphereRadius,32);
	}
}

//
//	USkeletalMeshComponent::RenderShadowVolume
//

void USkeletalMeshComponent::RenderShadowVolume(const FSceneContext& Context,struct FShadowRenderInterface* SRI,ULightComponent* Light)
{
	FCycleCounterSection	CycleCounter(GEngineStats.ShadowTime);
	EShadowStencilMode		Mode = SSM_ZFail;

	INT	LODLevel = GetLODLevel(Context);

	if(SkeletalMesh && SkeletalMesh->LODModels(LODLevel).ShadowIndices.Num() && MeshObject)
	{
		// Find the homogenous light position in local space.

		FPlane	LightPosition = LocalToWorld.Inverse().TransformFPlane(Light->GetPosition());

		FStaticLODModel&	LODModel = SkeletalMesh->LODModels(LODLevel);

		if(MeshObject->CachedVertexLOD != LODLevel)
			MeshObject->CacheVertices(LODLevel);

		// Find the orientation of the triangles relative to the light position.

		FLOAT*	PlaneDots = new FLOAT[LODModel.ShadowIndices.Num() / 3];
		for(UINT TriangleIndex = 0;TriangleIndex < (UINT)LODModel.ShadowIndices.Num() / 3;TriangleIndex++)
		{
			const FVector&	V1 = MeshObject->CachedFinalVertices(LODModel.ShadowIndices(TriangleIndex * 3 + 0)).Position,
							V2 = MeshObject->CachedFinalVertices(LODModel.ShadowIndices(TriangleIndex * 3 + 1)).Position,
							V3 = MeshObject->CachedFinalVertices(LODModel.ShadowIndices(TriangleIndex * 3 + 2)).Position;
			PlaneDots[TriangleIndex] = ((V2-V3) ^ (V1-V3)) | (FVector(LightPosition) - V1 * LightPosition.W);
		}

		// Extrude a shadow volume.

		FShadowIndexBuffer	IndexBuffer;
		_WORD				FirstExtrudedVertex = (LODModel.RigidVertices.Num() + LODModel.SoftVertices.Num());

		IndexBuffer.Indices.Empty(LODModel.ShadowIndices.Num() * 2);

		for(UINT TriangleIndex = 0;TriangleIndex < (UINT)LODModel.ShadowIndices.Num() / 3;TriangleIndex++)
		{
			_WORD*	TriangleIndices = &LODModel.ShadowIndices(TriangleIndex * 3);
			_WORD	Offset = IsNegativeFloat(PlaneDots[TriangleIndex]) ? FirstExtrudedVertex : 0;
			if(Mode == SSM_ZFail || IsNegativeFloat(PlaneDots[TriangleIndex]))
				IndexBuffer.AddFace(
					Offset + TriangleIndices[0],
					Offset + TriangleIndices[1],
					Offset + TriangleIndices[2]
					);
			if(LODModel.ShadowTriangleDoubleSided(TriangleIndex) && (Mode == SSM_ZFail || !IsNegativeFloat(PlaneDots[TriangleIndex])))
				IndexBuffer.AddFace(
					(FirstExtrudedVertex - Offset) + TriangleIndices[2],
					(FirstExtrudedVertex - Offset) + TriangleIndices[1],
					(FirstExtrudedVertex - Offset) + TriangleIndices[0]
					);
		}

		for(UINT EdgeIndex = 0;EdgeIndex < (UINT)LODModel.Edges.Num();EdgeIndex++)
		{
			FMeshEdge&	Edge = LODModel.Edges(EdgeIndex);
			if(Edge.Faces[1] == INDEX_NONE || IsNegativeFloat(PlaneDots[Edge.Faces[0]]) != IsNegativeFloat(PlaneDots[Edge.Faces[1]]))
			{
				IndexBuffer.AddEdge(
					Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 1 : 0],
					Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 0 : 1],
					FirstExtrudedVertex
					);
				if(LODModel.ShadowTriangleDoubleSided(Edge.Faces[0]) && Edge.Faces[1] != INDEX_NONE)
					IndexBuffer.AddEdge(
						Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 1 : 0],
						Edge.Vertices[IsNegativeFloat(PlaneDots[Edge.Faces[0]]) ? 0 : 1],
						FirstExtrudedVertex
						);
			}
		}

		IndexBuffer.CalcSize();

		delete [] PlaneDots;

		GEngineStats.ShadowTriangles.Value += IndexBuffer.Indices.Num() / 3;
		SRI->DrawShadowVolume(
			&MeshObject->LODs(LODLevel).ShadowVertexFactory,
			&IndexBuffer,
			0,
			IndexBuffer.Indices.Num() / 3,
			0,
			(LODModel.RigidVertices.Num() + LODModel.SoftVertices.Num()) * 2 -1,
			Mode
			);
	}

}