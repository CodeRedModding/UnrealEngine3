/*
* Copyright 2009 - 2010 Autodesk, Inc.  All Rights Reserved.
*
* Permission to use, copy, modify, and distribute this software in object
* code form for any purpose and without fee is hereby granted, provided
* that the above copyright notice appears in all copies and that both
* that copyright notice and the limited warranty and restricted rights
* notice below appear in all supporting documentation.
*
* AUTODESK PROVIDES THIS PROGRAM "AS IS" AND WITH ALL FAULTS.
* AUTODESK SPECIFICALLY DISCLAIMS ANY AND ALL WARRANTIES, WHETHER EXPRESS
* OR IMPLIED, INCLUDING WITHOUT LIMITATION THE IMPLIED WARRANTY
* OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR USE OR NON-INFRINGEMENT
* OF THIRD PARTY RIGHTS.  AUTODESK DOES NOT WARRANT THAT THE OPERATION
* OF THE PROGRAM WILL BE UNINTERRUPTED OR ERROR FREE.
*
* In no event shall Autodesk, Inc. be liable for any direct, indirect,
* incidental, special, exemplary, or consequential damages (including,
* but not limited to, procurement of substitute goods or services;
* loss of use, data, or profits; or business interruption) however caused
* and on any theory of liability, whether in contract, strict liability,
* or tort (including negligence or otherwise) arising in any way out
* of such code.
*
* This software is provided to the U.S. Government with the same rights
* and restrictions as described herein.
*/

/*=============================================================================
	Skeletal mesh creation from FBX data.
	Largely based on UnMeshEd.cpp
=============================================================================*/

#include "UnrealEd.h"

#if WITH_FBX
#include "Factories.h"
#include "Engine.h"
#include "UnTextureLayout.h"
#include "UnFracturedStaticMesh.h"
#include "EnginePhysicsClasses.h"
#include "BSPOps.h"
#include "EngineMaterialClasses.h"
#include "SkelImport.h"
#include "EngineAnimClasses.h"
#include "EngineInterpolationClasses.h"
#include "UnLinkedObjDrawUtils.h"
#include "UnFbxImporter.h"
#include "AnimationEncodingFormat.h"

using namespace UnFbx;

#define RESAMPLERATE 30

struct ExistingSkelMeshData;
extern ExistingSkelMeshData* SaveExistingSkelMeshData(USkeletalMesh* ExistingSkelMesh);
extern void RestoreExistingSkelMeshData(ExistingSkelMeshData* MeshData, USkeletalMesh* SkeletalMesh);


// Get the geometry deformation local to a node. It is never inherited by the
// children.
FbxAMatrix GetGeometry(FbxNode* pNode) 
{
	FbxVector4 lT, lR, lS;
	FbxAMatrix lGeometry;

	lT = pNode->GetGeometricTranslation(FbxNode::eSOURCE_SET);
	lR = pNode->GetGeometricRotation(FbxNode::eSOURCE_SET);
	lS = pNode->GetGeometricScaling(FbxNode::eSOURCE_SET);

	lGeometry.SetT(lT);
	lGeometry.SetR(lR);
	lGeometry.SetS(lS);

	return lGeometry;
}

// Scale all the elements of a matrix.
void MatrixScale(FbxAMatrix& pMatrix, double pValue)
{
	INT i,j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pMatrix[i][j] *= pValue;
		}
	}
}

// Add a value to all the elements in the diagonal of the matrix.
void MatrixAddToDiagonal(FbxAMatrix& pMatrix, double pValue)
{
	pMatrix[0][0] += pValue;
	pMatrix[1][1] += pValue;
	pMatrix[2][2] += pValue;
	pMatrix[3][3] += pValue;
}


// Sum two matrices element by element.
void MatrixAdd(FbxAMatrix& pDstMatrix, FbxAMatrix& pSrcMatrix)
{
	INT i,j;

	for (i = 0; i < 4; i++)
	{
		for (j = 0; j < 4; j++)
		{
			pDstMatrix[i][j] += pSrcMatrix[i][j];
		}
	}
}


void CFbxImporter::SkinControlPointsToPose(FSkeletalMeshBinaryImport &SkelMeshImporter, FbxMesh* FbxMesh, FbxShape* FbxShape, UBOOL bUseT0)
{
	FbxTime poseTime = FBXSDK_TIME_INFINITE;
	if(bUseT0)
	{
		poseTime = 0;
	}

	INT VertexCount = FbxMesh->GetControlPointsCount();

	// Create a copy of the vertex array to receive vertex deformations.
	FbxVector4* VertexArray = new FbxVector4[VertexCount];

	// If a shape is provided, then it is the morphed version of the mesh.
	// So we want to deform that instead of the base mesh vertices
	if (FbxShape)
	{
		check(FbxShape->GetControlPointsCount() == VertexCount);
		appMemcpy(VertexArray, FbxShape->GetControlPoints(), VertexCount * sizeof(FbxVector4));
	}
	else
	{
		appMemcpy(VertexArray, FbxMesh->GetControlPoints(), VertexCount * sizeof(FbxVector4));
	}																	 


	INT ClusterCount = 0;
	INT SkinCount = FbxMesh->GetDeformerCount(FbxDeformer::eSKIN);
	for( INT i=0; i< SkinCount; i++)
	{
		ClusterCount += ((FbxSkin *)(FbxMesh->GetDeformer(i, FbxDeformer::eSKIN)))->GetClusterCount();
	}
	
	// Deform the vertex array with the links contained in the mesh.
	if (ClusterCount)
	{
		FbxAMatrix MeshMatrix = ComputeTotalMatrix(FbxMesh->GetNode());
		// All the links must have the same link mode.
		FbxCluster::ELinkMode lClusterMode = ((FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSKIN))->GetCluster(0)->GetLinkMode();

		INT i, j;
		INT lClusterCount=0;

		INT lSkinCount = FbxMesh->GetDeformerCount(FbxDeformer::eSKIN);

		FbxAMatrix* lClusterDeformation = new FbxAMatrix[VertexCount];
		memset(lClusterDeformation, 0, VertexCount * sizeof(FbxAMatrix));
		double* lClusterWeight = new double[VertexCount];
		memset(lClusterWeight, 0, VertexCount * sizeof(double));

		if (lClusterMode == FbxCluster::eADDITIVE)
		{
			for (i = 0; i < VertexCount; i++)
			{
				lClusterDeformation[i].SetIdentity();
			}
		}

		for ( i=0; i<lSkinCount; ++i)
		{
			lClusterCount =( (FbxSkin *)FbxMesh->GetDeformer(i, FbxDeformer::eSKIN))->GetClusterCount();
			for (j=0; j<lClusterCount; ++j)
			{
				FbxCluster* Cluster =((FbxSkin *) FbxMesh->GetDeformer(i, FbxDeformer::eSKIN))->GetCluster(j);
				if (!Cluster->GetLink())
					continue;
					
				FbxNode* Link = Cluster->GetLink();
				
				FbxAMatrix lReferenceGlobalInitPosition;
				FbxAMatrix lReferenceGlobalCurrentPosition;
				FbxAMatrix lClusterGlobalInitPosition;
				FbxAMatrix lClusterGlobalCurrentPosition;
				FbxAMatrix lReferenceGeometry;
				FbxAMatrix lClusterGeometry;

				FbxAMatrix lClusterRelativeInitPosition;
				FbxAMatrix lClusterRelativeCurrentPositionInverse;
				FbxAMatrix lVertexTransformMatrix;

				if (lClusterMode == FbxCluster::eADDITIVE && Cluster->GetAssociateModel())
				{
					Cluster->GetTransformAssociateModelMatrix(lReferenceGlobalInitPosition);
					lReferenceGlobalCurrentPosition = Cluster->GetAssociateModel()->GetScene()->GetEvaluator()->GetNodeGlobalTransform(Cluster->GetAssociateModel(), poseTime);
					// Geometric transform of the model
					lReferenceGeometry = GetGeometry(Cluster->GetAssociateModel());
					lReferenceGlobalCurrentPosition *= lReferenceGeometry;
				}
				else
				{
					Cluster->GetTransformMatrix(lReferenceGlobalInitPosition);
					lReferenceGlobalCurrentPosition = MeshMatrix; //pGlobalPosition;
					// Multiply lReferenceGlobalInitPosition by Geometric Transformation
					lReferenceGeometry = GetGeometry(FbxMesh->GetNode());
					lReferenceGlobalInitPosition *= lReferenceGeometry;
				}
				// Get the link initial global position and the link current global position.
				Cluster->GetTransformLinkMatrix(lClusterGlobalInitPosition);
				lClusterGlobalCurrentPosition = Link->GetScene()->GetEvaluator()->GetNodeGlobalTransform(Link, poseTime);

				// Compute the initial position of the link relative to the reference.
				lClusterRelativeInitPosition = lClusterGlobalInitPosition.Inverse() * lReferenceGlobalInitPosition;

				// Compute the current position of the link relative to the reference.
				lClusterRelativeCurrentPositionInverse = lReferenceGlobalCurrentPosition.Inverse() * lClusterGlobalCurrentPosition;

				// Compute the shift of the link relative to the reference.
				lVertexTransformMatrix = lClusterRelativeCurrentPositionInverse * lClusterRelativeInitPosition;

				INT k;
				INT lVertexIndexCount = Cluster->GetControlPointIndicesCount();

				for (k = 0; k < lVertexIndexCount; ++k) 
				{
					INT lIndex = Cluster->GetControlPointIndices()[k];

					// Sometimes, the mesh can have less points than at the time of the skinning
					// because a smooth operator was active when skinning but has been deactivated during export.
					if (lIndex >= VertexCount)
						continue;

					double lWeight = Cluster->GetControlPointWeights()[k];

					if (lWeight == 0.0)
					{
						continue;
					}

					// Compute the influence of the link on the vertex.
					FbxAMatrix lInfluence = lVertexTransformMatrix;
					MatrixScale(lInfluence, lWeight);

					if (lClusterMode == FbxCluster::eADDITIVE)
					{
						// Multiply with to the product of the deformations on the vertex.
						MatrixAddToDiagonal(lInfluence, 1.0 - lWeight);
						lClusterDeformation[lIndex] = lInfluence * lClusterDeformation[lIndex];

						// Set the link to 1.0 just to know this vertex is influenced by a link.
						lClusterWeight[lIndex] = 1.0;
					}
					else // lLinkMode == KFbxLink::eNORMALIZE || lLinkMode == KFbxLink::eTOTAL1
					{
						// Add to the sum of the deformations on the vertex.
						MatrixAdd(lClusterDeformation[lIndex], lInfluence);

						// Add to the sum of weights to either normalize or complete the vertex.
						lClusterWeight[lIndex] += lWeight;
					}

				}
			}
		}
		
		for (i = 0; i < VertexCount; i++) 
		{
			FbxVector4 lSrcVertex = VertexArray[i];
			FbxVector4& lDstVertex = VertexArray[i];
			double lWeight = lClusterWeight[i];

			// Deform the vertex if there was at least a link with an influence on the vertex,
			if (lWeight != 0.0) 
			{
				lDstVertex = lClusterDeformation[i].MultT(lSrcVertex);

				if (lClusterMode == FbxCluster::eNORMALIZE)
				{
					// In the normalized link mode, a vertex is always totally influenced by the links. 
					lDstVertex /= lWeight;
				}
				else if (lClusterMode == FbxCluster::eTOTAL1)
				{
					// In the total 1 link mode, a vertex can be partially influenced by the links. 
					lSrcVertex *= (1.0 - lWeight);
					lDstVertex += lSrcVertex;
				}
			} 
		}
		
		// change the vertex position
		INT ExistPointNum = SkelMeshImporter.Points.Num();
		INT StartPointIndex = ExistPointNum - VertexCount;
		for(INT ControlPointsIndex = 0 ; ControlPointsIndex < VertexCount ;ControlPointsIndex++ )
		{
			SkelMeshImporter.Points(ControlPointsIndex+StartPointIndex) = Converter.ConvertPos(MeshMatrix.MultT(VertexArray[ControlPointsIndex]));
		}

		delete [] lClusterDeformation;
		delete [] lClusterWeight;
		
	}
	
	delete[] VertexArray;
}

// 3 "ProcessImportMesh..." functions outputing Unreal data from a filled FSkeletalMeshBinaryImport
// and a handfull of other minor stuff needed by these 
// Fully taken from UnMeshEd.cpp

IMPLEMENT_COMPARE_CONSTREF( VRawBoneInfluence, SkelMeshFbx, 
{
	if		( A.VertexIndex > B.VertexIndex	) return  1;
	else if ( A.VertexIndex < B.VertexIndex	) return -1;
	else if ( A.Weight	  < B.Weight		) return  1;
	else if ( A.Weight	  > B.Weight		) return -1;
	else if ( A.BoneIndex   > B.BoneIndex	) return  1;
	else if ( A.BoneIndex   < B.BoneIndex	) return -1;
	else									  return  0;	
}
)

extern void ProcessImportMeshInfluences(FSkeletalMeshBinaryImport& SkelMeshImporter);
extern void ProcessImportMeshMaterials(TArray<UMaterialInterface*>& Materials, FSkeletalMeshBinaryImport& SkelMeshImporter, UBOOL TruncateTags);
extern void ProcessImportMeshSkeleton(TArray<FMeshBone>& RefSkeleton, INT& SkeletalDepth, FSkeletalMeshBinaryImport& SkelMeshImporter);

struct tFaceRecord
{
	INT FaceIndex;
	INT HoekIndex;
	INT WedgeIndex;
	DWORD SmoothFlags;
	DWORD FanFlags;
};

struct VertsFans
{	
	TArray<tFaceRecord> FaceRecord;
	INT FanGroupCount;
};

struct tInfluences
{
	TArray<INT> RawInfIndices;
};

struct tWedgeList
{
	TArray<INT> WedgeList;
};

struct tFaceSet
{
	TArray<INT> Faces;
};

// Check whether faces have at least two vertices in common. These must be POINTS - don't care about wedges.
UBOOL UnFbx::CFbxImporter::FacesAreSmoothlyConnected( FSkeletalMeshBinaryImport &SkelMeshImporter, INT Face1, INT Face2 )
{

	//if( ( Face1 >= Thing->SkinData.Faces.Num()) || ( Face2 >= Thing->SkinData.Faces.Num()) ) return FALSE;

	if( Face1 == Face2 )
	{
		return TRUE;
	}

	// Smoothing groups match at least one bit in binary AND ?
	if( ( SkelMeshImporter.Faces(Face1).SmoothingGroups & SkelMeshImporter.Faces(Face2).SmoothingGroups ) == 0  ) 
	{
		return FALSE;
	}

	INT VertMatches = 0;
	for( INT i=0; i<3; i++)
	{
		INT Point1 = SkelMeshImporter.Wedges( SkelMeshImporter.Faces(Face1).WedgeIndex[i] ).VertexIndex;

		for( INT j=0; j<3; j++)
		{
			INT Point2 = SkelMeshImporter.Wedges( SkelMeshImporter.Faces(Face2).WedgeIndex[j] ).VertexIndex;
			if( Point2 == Point1 )
			{
				VertMatches ++;
			}
		}
	}

	return ( VertMatches >= 2 );
}

INT	UnFbx::CFbxImporter::DoUnSmoothVerts(FSkeletalMeshBinaryImport &SkelMeshImporter)
{
	//
	// Connectivity: triangles with non-matching smoothing groups will be physically split.
	//
	// -> Splitting involves: the UV+material-contaning vertex AND the 3d point.
	//
	// -> Tally smoothing groups for each and every (textured) vertex.
	//
	// -> Collapse: 
	// -> start from a vertex and all its adjacent triangles - go over
	// each triangle - if any connecting one (sharing more than one vertex) gives a smoothing match,
	// accumulate it. Then IF more than one resulting section, 
	// ensure each boundary 'vert' is split _if not already_ to give each smoothing group
	// independence from all others.
	//

	INT DuplicatedVertCount = 0;
	INT RemappedHoeks = 0;

	INT TotalSmoothMatches = 0;
	INT TotalConnexChex = 0;

	// Link _all_ faces to vertices.	
	TArray<VertsFans>  Fans;
	TArray<tInfluences> PointInfluences;	
	TArray<tWedgeList>  PointWedges;

	Fans.AddZeroed( SkelMeshImporter.Points.Num() );//Fans.AddExactZeroed(			Thing->SkinData.Points.Num() );
	PointInfluences.AddZeroed( SkelMeshImporter.Points.Num() );//PointInfluences.AddExactZeroed( Thing->SkinData.Points.Num() );
	PointWedges.AddZeroed( SkelMeshImporter.Points.Num() );//PointWedges.AddExactZeroed(	 Thing->SkinData.Points.Num() );

	for(INT i=0; i< SkelMeshImporter.Influences.Num(); i++)
	{
		PointInfluences(SkelMeshImporter.Influences(i).VertexIndex ).RawInfIndices.AddItem( i );
	}

	for(INT i=0; i< SkelMeshImporter.Wedges.Num(); i++)
	{
		PointWedges(SkelMeshImporter.Wedges(i).VertexIndex ).WedgeList.AddItem( i );
	}

	for(INT f=0; f< SkelMeshImporter.Faces.Num(); f++ )
	{
		// For each face, add a pointer to that face into the Fans[vertex].
		for( INT i=0; i<3; i++)
		{
			INT WedgeIndex = SkelMeshImporter.Faces(f).WedgeIndex[i];			
			INT PointIndex = SkelMeshImporter.Wedges( WedgeIndex ).VertexIndex;
			tFaceRecord NewFR;

			NewFR.FaceIndex = f;
			NewFR.HoekIndex = i;			
			NewFR.WedgeIndex = WedgeIndex; // This face touches the point courtesy of Wedges[Wedgeindex].
			NewFR.SmoothFlags = SkelMeshImporter.Faces(f).SmoothingGroups;
			NewFR.FanFlags = 0;
			Fans( PointIndex ).FaceRecord.AddItem( NewFR );
			Fans( PointIndex ).FanGroupCount = 0;
		}		
	}

	// Investigate connectivity and assign common group numbers (1..+) to the fans' individual FanFlags.
	for( INT p=0; p< Fans.Num(); p++) // The fan of faces for each 3d point 'p'.
	{
		// All faces connecting.
		if( Fans(p).FaceRecord.Num() > 0 ) 
		{		
			INT FacesProcessed = 0;
			TArray<tFaceSet> FaceSets; // Sets with indices INTO FANS, not into face array.			

			// Digest all faces connected to this vertex (p) into one or more smooth sets. only need to check 
			// all faces MINUS one..
			while( FacesProcessed < Fans(p).FaceRecord.Num()  )
			{
				// One loop per group. For the current ThisFaceIndex, tally all truly connected ones
				// and put them in a new TArray. Once no more can be connected, stop.

				INT NewSetIndex = FaceSets.Num(); // 0 to start
				FaceSets.AddZeroed(1);						// first one will be just ThisFaceIndex.

				// Find the first non-processed face. There will be at least one.
				INT ThisFaceFanIndex = 0;
				{
					INT SearchIndex = 0;
					while( Fans(p).FaceRecord(SearchIndex).FanFlags == -1 ) // -1 indicates already  processed. 
					{
						SearchIndex++;
					}
					ThisFaceFanIndex = SearchIndex; //Fans[p].FaceRecord[SearchIndex].FaceIndex; 
				}

				// Initial face.
				FaceSets( NewSetIndex ).Faces.AddItem( ThisFaceFanIndex );   // Add the unprocessed Face index to the "local smoothing group" [NewSetIndex].
				Fans(p).FaceRecord(ThisFaceFanIndex).FanFlags = -1;			  // Mark as processed.
				FacesProcessed++; 

				// Find all faces connected to this face, and if there's any
				// smoothing group matches, put it in current face set and mark it as processed;
				// until no more match. 
				INT NewMatches = 0;
				do
				{
					NewMatches = 0;
					// Go over all current faces in this faceset and set if the FaceRecord (local smoothing groups) has any matches.
					// there will be at least one face already in this faceset - the first face in the fan.
					for( INT n=0; n< FaceSets(NewSetIndex).Faces.Num(); n++)
					{				
						INT HookFaceIdx = Fans(p).FaceRecord( FaceSets(NewSetIndex).Faces(n) ).FaceIndex;

						//Go over the fan looking for matches.
						for( INT s=0; s< Fans(p).FaceRecord.Num(); s++)
						{
							// Skip if same face, skip if face already processed.
							if( ( HookFaceIdx != Fans(p).FaceRecord(s).FaceIndex )  && ( Fans(p).FaceRecord(s).FanFlags != -1  ))
							{
								TotalConnexChex++;
								// Process if connected with more than one vertex, AND smooth..
								if( FacesAreSmoothlyConnected( SkelMeshImporter, HookFaceIdx, Fans(p).FaceRecord(s).FaceIndex ) )
								{									
									TotalSmoothMatches++;
									Fans(p).FaceRecord(s).FanFlags = -1; // Mark as processed.
									FacesProcessed++;
									// Add 
									FaceSets(NewSetIndex).Faces.AddItem( s ); // Store FAN index of this face index into smoothing group's faces. 
									// Tally
									NewMatches++;
								}
							} // not the same...
						}// all faces in fan
					} // all faces in FaceSet
				}while( NewMatches );	

			}// Repeat until all faces processed.

			// For the new non-initialized  face sets, 
			// Create a new point, influences, and uv-vertex(-ices) for all individual FanFlag groups with an index of 2+ and also remap
			// the face's vertex into those new ones.
			if( FaceSets.Num() > 1 )
			{
				for( INT f=1; f<FaceSets.Num(); f++ )
				{				
					// We duplicate the current vertex. (3d point)
					INT NewPointIndex = SkelMeshImporter.Points.Num();
					SkelMeshImporter.Points.Add();
					SkelMeshImporter.Points(NewPointIndex) = SkelMeshImporter.Points(p) ;
					
					DuplicatedVertCount++;

					// Duplicate all related weights.
					for( INT t=0; t< PointInfluences(p).RawInfIndices.Num(); t++ )
					{
						// Add new weight
						INT NewWeightIndex = SkelMeshImporter.Influences.Num();
						SkelMeshImporter.Influences.Add();
						SkelMeshImporter.Influences(NewWeightIndex) = SkelMeshImporter.Influences( PointInfluences(p).RawInfIndices(t) );
						SkelMeshImporter.Influences(NewWeightIndex).VertexIndex = NewPointIndex;
					}

					// Duplicate any and all Wedges associated with it; and all Faces' wedges involved.					
					for( INT w=0; w< PointWedges(p).WedgeList.Num(); w++)
					{
						INT OldWedgeIndex = PointWedges(p).WedgeList(w);
						INT NewWedgeIndex = SkelMeshImporter.Wedges.Num();
						SkelMeshImporter.Wedges.Add();
						SkelMeshImporter.Wedges(NewWedgeIndex) = SkelMeshImporter.Wedges( OldWedgeIndex );
						SkelMeshImporter.Wedges( NewWedgeIndex ).VertexIndex = NewPointIndex; 

						//  Update relevant face's Wedges. Inelegant: just check all associated faces for every new wedge.
						for( INT s=0; s< FaceSets(f).Faces.Num(); s++)
						{
							INT FanIndex = FaceSets(f).Faces(s);
							if( Fans(p).FaceRecord( FanIndex ).WedgeIndex == OldWedgeIndex )
							{
								// Update just the right one for this face (HoekIndex!) 
								SkelMeshImporter.Faces( Fans(p).FaceRecord( FanIndex).FaceIndex ).WedgeIndex[ Fans(p).FaceRecord( FanIndex ).HoekIndex ] = NewWedgeIndex;
								RemappedHoeks++;
							}
						}
					}
				}
			} //  if FaceSets.Num(). -> duplicate stuff
		}//	while( FacesProcessed < Fans[p].FaceRecord.Num() )
	} // Fans for each 3d point

	return DuplicatedVertCount; 
}

UBOOL IsUnrealBone(FbxNode* Link)
{
	FbxNodeAttribute* Attr = Link->GetNodeAttribute();
	if (Attr)
	{
		FbxNodeAttribute::EAttributeType AttrType = Attr->GetAttributeType();
		if ( AttrType == FbxNodeAttribute::eSKELETON ||
			AttrType == FbxNodeAttribute::eMESH	 ||
			AttrType == FbxNodeAttribute::eNULL )
		{
			return TRUE;
		}
	}
	
	return FALSE;
}


void UnFbx::CFbxImporter::RecursiveBuildSkeleton(FbxNode* Link, TArray<FbxNode*>& OutSortedLinks)
{
	if (IsUnrealBone(Link))
	{
		OutSortedLinks.AddItem(Link);
		INT ChildIndex;
		for (ChildIndex=0; ChildIndex<Link->GetChildCount(); ChildIndex++)
		{
			RecursiveBuildSkeleton(Link->GetChild(ChildIndex),OutSortedLinks);
		}
	}
}

void UnFbx::CFbxImporter::BuildSkeletonSystem(TArray<FbxCluster*>& ClusterArray, TArray<FbxNode*>& OutSortedLinks)
{
	FbxNode* Link;
	TArray<FbxNode*> RootLinks;
	INT ClusterIndex;
	for (ClusterIndex = 0; ClusterIndex < ClusterArray.Num(); ClusterIndex++)
	{
		Link = ClusterArray(ClusterIndex)->GetLink();
		Link = GetRootSkeleton(Link);
		INT LinkIndex;
		for (LinkIndex=0; LinkIndex<RootLinks.Num(); LinkIndex++)
		{
			if (Link == RootLinks(LinkIndex))
			{
				break;
			}
		}
		
		// this link is a new root, add it
		if (LinkIndex == RootLinks.Num())
		{
			RootLinks.AddItem(Link);
		}
	}

	for (INT LinkIndex=0; LinkIndex<RootLinks.Num(); LinkIndex++)
	{
		RecursiveBuildSkeleton(RootLinks(LinkIndex), OutSortedLinks);
	}
}

UBOOL UnFbx::CFbxImporter::ImportBone(TArray<FbxNode*>& NodeArray, FSkeletalMeshBinaryImport &SkelMeshImporter, TArray<FbxNode*> &SortedLinks, UBOOL& bOutDiffPose, BOOL bDisableMissingBindPoseWarning)
{
	bOutDiffPose = FALSE;
	INT SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh
	FbxNode* Link = NULL;
	FbxArray<FbxPose*> PoseArray;
	TArray<FbxCluster*> ClusterArray;
	
	if (NodeArray(0)->GetMesh()->GetDeformerCount(FbxDeformer::eSKIN) == 0)
	{
		SkelType = 1;
		Link = NodeArray(0);
		RecursiveBuildSkeleton(GetRootSkeleton(Link),SortedLinks);
	}
	else
	{
		// get bindpose and clusters from FBX skeleton
		
		// let's put the elements to their bind pose! (and we restore them after
		// we have built the ClusterInformation.
		INT Default_NbPoses = SdkManager->GetBindPoseCount(Scene);
		// If there are no BindPoses, the following will generate them.
		SdkManager->CreateMissingBindPoses(Scene);

		//if we created missing bind poses, update the number of bind poses
		INT NbPoses = SdkManager->GetBindPoseCount(Scene);

		if ( NbPoses != Default_NbPoses)
		{
			warnf(NAME_Warning,TEXT("The imported scene has no initial binding position (Bind Pose) for the skin. \
									The plug-in will compute one automatically. However, it may create unexpected results. "));
		}

		//
		// create the bones / skinning
		//

		for ( INT i = 0; i < NodeArray.Num(); i++)
		{
			FbxMesh* FbxMesh = NodeArray(i)->GetMesh();
			const INT SkinDeformerCount = FbxMesh->GetDeformerCount(FbxDeformer::eSKIN);
			for (INT DeformerIndex=0; DeformerIndex < SkinDeformerCount; DeformerIndex++)
			{
				FbxSkin* Skin = (FbxSkin*)FbxMesh->GetDeformer(DeformerIndex, FbxDeformer::eSKIN);
				for ( INT ClusterIndex = 0; ClusterIndex < Skin->GetClusterCount(); ClusterIndex++)
				{
					ClusterArray.AddItem(Skin->GetCluster(ClusterIndex));
				}
			}			
		}

		if (ClusterArray.Num() == 0)
		{
			warnf(NAME_Warning,TEXT("No associated clusters"));
			return FALSE;
		}

		// get bind pose
		const INT PoseCount = Scene->GetPoseCount();
		for (INT PoseIndex = 0; PoseIndex < PoseCount; PoseIndex++)
		{
			FbxPose* CurrentPose = Scene->GetPose(PoseIndex);
			if (CurrentPose && CurrentPose->IsBindPose())
			{
				if(CurrentPose->IsValidBindPose(NodeArray(0)))
				{
					PoseArray.Add(CurrentPose);
				}
				else
				{
					warnf(NAME_Warning, 
						TEXT("FBX bind pose %s is invalid.  It will be ignored"), 
						ANSI_TO_TCHAR(Scene->GetPose(PoseIndex)->GetName()));
				}
			}
		}

		// recurse through skeleton and build ordered table
		BuildSkeletonSystem(ClusterArray, SortedLinks);
	}

	if( SortedLinks.Num() == 0 )
	{
		warnf(NAME_Warning,TEXT("%s has no bones"), ANSI_TO_TCHAR(NodeArray(0)->GetName()) );
		return FALSE;
	}

	INT LinkIndex;

	// Check for duplicate bone names and issue a warning if found
	for(LinkIndex = 0; LinkIndex < SortedLinks.Num(); ++LinkIndex)
	{
		Link = SortedLinks(LinkIndex);

		for(INT AltLinkIndex = LinkIndex+1; AltLinkIndex < SortedLinks.Num(); ++AltLinkIndex)
		{
			FbxNode* AltLink = SortedLinks(AltLinkIndex);

			if(appStrcmpANSI(Link->GetName(), AltLink->GetName()) == 0)
			{
				FString RawBoneName = ANSI_TO_TCHAR(Link->GetName());
				appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("Error_DuplicateBoneName"), ANSI_TO_TCHAR(NodeArray(0)->GetName()), *RawBoneName ) );
				return FALSE;
			}
		}
	}

	FbxArray<FbxAMatrix> GlobalsPerLink;
	GlobalsPerLink.AddMultiple(SortedLinks.Num());
	GlobalsPerLink[0].SetIdentity();
	
	UBOOL GlobalLinkFoundFlag;
	FbxVector4 LocalLinkT;
	FbxQuaternion LocalLinkQ;

	UBOOL NonIdentityScaleFound = FALSE;
	
	SkelMeshImporter.RefBonesBinary.Add(SortedLinks.Num());
	

	UBOOL bAnyLinksNotInBindPose = FALSE;
	FString LinksWithoutBindPoses;

	for (LinkIndex=0; LinkIndex<SortedLinks.Num(); LinkIndex++)
	{
		Link = SortedLinks(LinkIndex);

		// get the link parent and children
		INT ParentIndex = 0; // base value for root if no parent found
		FbxNode* Parent = Link->GetParent();
		if (LinkIndex)
		{
			for (INT ll=0; ll<LinkIndex; ++ll) // <LinkIndex because parent is guaranteed to be before child in sortedLink
			{
				FbxNode* Otherlink	= SortedLinks(ll);
				if (Otherlink == Parent)
				{
					ParentIndex = ll;
					break;
				}
			}
		}
		

		GlobalLinkFoundFlag = FALSE;
		if (!SkelType) //skeletal mesh
		{
			// there are some links, they have no cluster, but in bindpose
			if (PoseArray.GetCount())
			{
				for (INT PoseIndex = 0; PoseIndex < PoseArray.GetCount(); PoseIndex++)
				{
					INT PoseLinkIndex = PoseArray[PoseIndex]->Find(Link);
					if (PoseLinkIndex>=0)
					{
						FbxMatrix NoneAffineMatrix = PoseArray[PoseIndex]->GetMatrix(PoseLinkIndex);
						FbxAMatrix Matrix = *(FbxAMatrix*)(double*)&NoneAffineMatrix;
						GlobalsPerLink[LinkIndex] = Matrix;
						GlobalLinkFoundFlag = TRUE;
						break;
					}
				}
			}

			if (!GlobalLinkFoundFlag)
			{
				if(!ImportOptions->bUseT0AsRefPose && !bDisableMissingBindPoseWarning)
				{
					bAnyLinksNotInBindPose = TRUE;
					LinksWithoutBindPoses +=  ANSI_TO_TCHAR(Link->GetName());
					LinksWithoutBindPoses +=  TEXT("  \n");
				}

				for (INT ClusterIndex=0; ClusterIndex<ClusterArray.Num(); ClusterIndex++)
				{
					FbxCluster* Cluster = ClusterArray(ClusterIndex);
					if (Link == Cluster->GetLink())
					{
						Cluster->GetTransformLinkMatrix(GlobalsPerLink[LinkIndex]);
						GlobalLinkFoundFlag = TRUE;
						break;
					}
				}
			}
		}

		if (!GlobalLinkFoundFlag)
		{
			// The node is not included in a bind pose and is not associated with any cluster, so just use its 'default'
			// matrix as its bind pose.  
			// Passing a time of Infinite (the default value) to EvaluateGlobalTransform returns the 'default' matrix for the node
			GlobalsPerLink[LinkIndex] = Link->EvaluateGlobalTransform();
		}
		
		if (ImportOptions->bUseT0AsRefPose)
		{
			FbxAMatrix& T0Matrix = Scene->GetEvaluator()->GetNodeGlobalTransform(Link, 0);
			if (GlobalsPerLink[LinkIndex] != T0Matrix)
			{
				bOutDiffPose = TRUE;
			}
			
			GlobalsPerLink[LinkIndex] = T0Matrix;
		}
		

		if (LinkIndex)
		{
			FbxAMatrix	Matrix;
			Matrix = GlobalsPerLink[ParentIndex].Inverse() * GlobalsPerLink[LinkIndex];
			LocalLinkT = Matrix.GetT();
			LocalLinkQ = Matrix.GetQ();
		}
		else	// skeleton root
		{
			// for root, this is global coordinate
			LocalLinkT = GlobalsPerLink[LinkIndex].GetT();
			LocalLinkQ = GlobalsPerLink[LinkIndex].GetQ();
		}
		
		// Unreal does not directly support non-uniform scale on bones, so
		// attempt to bake the scale into the local translation for the bones
		FbxVector4 ParentGlobalLinkS = GlobalsPerLink[ParentIndex].GetS();
		if ( !ImportOptions->bImportRigidAnim && 
				(ParentGlobalLinkS[0] > 1.0 + SCALE_TOLERANCE || ParentGlobalLinkS[0] < 1.0 - SCALE_TOLERANCE) || 
				(ParentGlobalLinkS[1] > 1.0 + SCALE_TOLERANCE || ParentGlobalLinkS[1] < 1.0 - SCALE_TOLERANCE) || 
				(ParentGlobalLinkS[2] > 1.0 + SCALE_TOLERANCE || ParentGlobalLinkS[2] < 1.0 - SCALE_TOLERANCE) )
		{
			NonIdentityScaleFound = TRUE;

			if (LinkIndex)
			{
				FbxAMatrix OriginalGlobalScale;
				OriginalGlobalScale.SetS(GlobalsPerLink[LinkIndex].GetS());

				FbxAMatrix InvOriginalGlobalScale = OriginalGlobalScale.Inverse();

				// First, remove all scaling from the link's current global matrix
				FbxAMatrix GlobalMtx_NoScale = GlobalsPerLink[LinkIndex];
				GlobalMtx_NoScale = GlobalMtx_NoScale * InvOriginalGlobalScale;

				// Now use the scale-less version of the bone's world matrix to convert 
				// the parent's scale into the bone's local space
				FbxAMatrix ParentGlobalScale;
				ParentGlobalScale.SetS(ParentGlobalLinkS);

				FbxAMatrix	Matrix;
				Matrix = GlobalMtx_NoScale.Inverse() * ParentGlobalScale;

				FbxVector4 NewLocalS = Matrix.GetS();

				LocalLinkT[0] *= NewLocalS[0];
				LocalLinkT[1] *= NewLocalS[1];
				LocalLinkT[2] *= NewLocalS[2];
			}
		}
		
		// set bone
		VBone& Bone = SkelMeshImporter.RefBonesBinary(LinkIndex);
		ANSICHAR BoneName[64];
		strcpy_s(BoneName,64,MakeName(Link->GetName()));
		strcpy_s(Bone.Name, 64, TCHAR_TO_ANSI(*SkelMeshImporter.FixupBoneName(BoneName)));

		VJointPos& JointMatrix = Bone.BonePos;
		FbxSkeleton* Skeleton = Link->GetSkeleton();
		if (Skeleton)
		{
			JointMatrix.Length = Converter.ConvertDist(Skeleton->LimbLength.Get());
			JointMatrix.XSize = Converter.ConvertDist(Skeleton->Size.Get());
			JointMatrix.YSize = Converter.ConvertDist(Skeleton->Size.Get());
			JointMatrix.ZSize = Converter.ConvertDist(Skeleton->Size.Get());
		}
		else
		{
			JointMatrix.Length = 1. ;
			JointMatrix.XSize = 100. ;
			JointMatrix.YSize = 100. ;
			JointMatrix.ZSize = 100. ;
		}

		// get the link parent and children
		Bone.ParentIndex = ParentIndex;
		Bone.NumChildren = 0;
		for (INT ChildIndex=0; ChildIndex<Link->GetChildCount(); ChildIndex++)
		{
			FbxNode* Child = Link->GetChild(ChildIndex);
			if (IsUnrealBone(Child))
			{
				Bone.NumChildren++;
			}
		}

		JointMatrix.Position = Converter.ConvertPos(LocalLinkT);
		JointMatrix.Orientation = Converter.ConvertRotToQuat(LocalLinkQ);
	}

	
	if(bAnyLinksNotInBindPose)
	{
		warnf( LocalizeSecure(LocalizeUnrealEd("Prompt_FBXImportBonesMissingFromBindPose"), *LinksWithoutBindPoses) );
	}
	if (NonIdentityScaleFound)
	{
		warnf( LocalizeSecure(LocalizeUnrealEd("Prompt_NoIdentityScaleForBone"), ANSI_TO_TCHAR(SortedLinks(0)->GetName())));
	}
	
	return TRUE;
}

void UnFbx::CFbxImporter::ImportMaterialsForSkelMesh(TArray<FbxNode*>& NodeArray, FSkeletalMeshBinaryImport &SkelMeshImporter, TArray<FString>& OutFbxMatList)
{
	FbxNode* Node;
	INT MaterialCount;
	
	// visit all FBX node to get all materials
	for (INT NodeIndex = 0; NodeIndex < NodeArray.Num(); NodeIndex++)
	{
		Node = NodeArray(NodeIndex);
		
		MaterialCount = Node->GetMaterialCount();
		INT ExistMatNum = OutFbxMatList.Num();

		for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			FbxSurfaceMaterial *FbxMaterial = Node->GetMaterial(MaterialIndex);

			INT ExistMatIndex;
			// check if the material has been recorded to the OurFbxMatList
			for ( ExistMatIndex = 0; ExistMatIndex<ExistMatNum; ExistMatIndex++)
			{
				if ( OutFbxMatList(ExistMatIndex) == ANSI_TO_TCHAR(FbxMaterial->GetName()) )
				{
					break;
				}
			}

			// the material has not been imported for this skeletal mesh, import this FBX material
			if (ExistMatIndex == ExistMatNum)
			{
				OutFbxMatList.AddItem( ANSI_TO_TCHAR( FbxMaterial->GetName() ) );

				// Add material slot for the skeletal mesh
				// We will create the unreal material later when import mesh data
				if ( ImportOptions->bImportMaterials )
				{
					SkelMeshImporter.Materials.Add();
					strcpy_s(SkelMeshImporter.Materials.Last().MaterialName,64,MakeName(FbxMaterial->GetName()) );
				}
			}
		}

		// import texture only if UI option is checked
		if ( !ImportOptions->bImportMaterials && ImportOptions->bImportTextures)
		{
			ImportTexturesFromNode(Node);
		}
	}
	

	// no material info, create a default material slot
	if ( OutFbxMatList.Num() == 0 )
	{
		warnf(NAME_Warning,TEXT("No material associated with skeletal mesh - using default"));
		UPackage* EngineMaterialsPackage = UObject::FindPackage(NULL,TEXT("EngineMaterials"));
		UMaterial* DefaultMaterial = FindObject<UMaterial>(EngineMaterialsPackage,TEXT("DefaultMaterial"));
		if (DefaultMaterial)
		{
			SkelMeshImporter.Materials.Add();
			strcpy_s(SkelMeshImporter.Materials.Last().MaterialName,64,TCHAR_TO_ANSI(*DefaultMaterial->GetName()));
		}
	}
	else if (!ImportOptions->bImportMaterials)
	{
		// if we don't import FBX materials, we will create material slots as FBX objects have
		warnf(NAME_Warning,TEXT("Using default materials for material slot"));

		UPackage* EngineMaterialsPackage = UObject::FindPackage(NULL,TEXT("EngineMaterials"));
		UMaterial* DefaultMaterial = FindObject<UMaterial>(EngineMaterialsPackage,TEXT("DefaultMaterial"));
		if (DefaultMaterial)
		{
			for (INT MatIndex = 0; MatIndex < OutFbxMatList.Num(); MatIndex++)
			{
				SkelMeshImporter.Materials.Add();
				strcpy_s(SkelMeshImporter.Materials.Last().MaterialName,64,TCHAR_TO_ANSI(*DefaultMaterial->GetName()));
			}
		}
	}
}

UObject* UnFbx::CFbxImporter::ImportSkeletalMesh(UObject* InParent, TArray<FbxNode*>& NodeArray, const FName& Name, EObjectFlags Flags, FString Filename, TArray<FbxShape*> *FbxShapeArray, FSkelMeshOptionalImportData *OptionalImportData, FSkeletalMeshBinaryImport* OutData, UBOOL bCreateRenderData )
{
	if (NodeArray.Num() == 0)
	{
		return NULL;
	}

	INT SkelType = 0; // 0 for skeletal mesh, 1 for rigid mesh
	
	FbxNode* Node = NodeArray(0);
	// find the mesh by its name
	FbxMesh* FbxMesh = Node->GetMesh();

	if( !FbxMesh )
	{
		warnf(TEXT("Fbx node: '%s' is not a valid skeletal mesh"), ANSI_TO_TCHAR(Node->GetName()));
		return NULL;
	}
	if (FbxMesh->GetDeformerCount(FbxDeformer::eSKIN) == 0)
	{
		SkelType = 1;
	}

	Parent = InParent;
	

	struct ExistingSkelMeshData* ExistSkelMeshDataPtr = NULL;
	if ( !FbxShapeArray && !OptionalImportData )
	{
		USkeletalMesh* ExistingSkelMesh = (USkeletalMesh*)UObject::StaticFindObjectFast(USkeletalMesh::StaticClass(), InParent, *Name.ToString(), TRUE, FALSE, RF_PendingKill);

		if (ExistingSkelMesh)
		{
			ExistingSkelMesh->PreEditChange(NULL);
			ExistSkelMeshDataPtr = SaveExistingSkelMeshData(ExistingSkelMesh);
		}
	}

	// [from USkeletalMeshFactory::FactoryCreateBinary]
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>( UObject::StaticConstructObject( USkeletalMesh::StaticClass(), InParent, Name, Flags ) );
	
	// Store the current file path and timestamp for re-import purposes
	SkeletalMesh->SourceFilePath = GFileManager->ConvertToRelativePath( *UFactory::CurrentFilename );
	FFileManager::FTimeStamp Timestamp;
	if ( GFileManager->GetTimestamp( *UFactory::CurrentFilename, Timestamp ) )
	{
		FFileManager::FTimeStamp::TimestampToFString(Timestamp, /*out*/ SkeletalMesh->SourceFileTimestamp);
	}
	
	SkeletalMesh->PreEditChange(NULL);

	SkeletalMesh->RotOrigin = FRotator(0, -16384, 0);
	
	// warning for missing smoothing group info
	CheckSmoothingInfo(FbxMesh);
	
	FSkeletalMeshBinaryImport TempData;
	// Fill with data from buffer - contains the full .FBX file. 	
	FSkeletalMeshBinaryImport* SkelMeshImporterPtr = &TempData;
	if( OutData )
	{
		SkelMeshImporterPtr = OutData;
	}

	// Get all FBX material names here, 
	// then we can set material index mapping correctly in FillSkelMeshImporterFromFbx() even if we do not import FBX materials
	TArray<FString> FbxMatList;
	ImportMaterialsForSkelMesh(NodeArray, *SkelMeshImporterPtr, FbxMatList);
	
	UBOOL bDiffPose;

	TArray<FbxNode*> SortedLinkArray;
	FbxArray<FbxAMatrix> GlobalsPerLink;

	// Note: importing morph data causes additional passes through this function, so disable the warning dialogs
	// from popping up again on each additional pass.  
	ImportBone(NodeArray, *SkelMeshImporterPtr, SortedLinkArray, bDiffPose, (FbxShapeArray || OptionalImportData) );
	for ( INT i = 0; i < NodeArray.Num(); i++)
	{
		Node = NodeArray(i);
		FbxMesh = Node->GetMesh();
		FbxSkin* Skin = (FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSKIN);
		FbxShape* FbxShape = NULL;
		if (FbxShapeArray)
		{
			FbxShape = (*FbxShapeArray)(i);
		}

		// NOTE: This function may invalidate FbxMesh and set it to point to a an updated version
		if (!FillSkelMeshImporterFromFbx(*SkelMeshImporterPtr, FbxMesh, Skin, FbxShape, SortedLinkArray, FbxMatList))
		{
			SkeletalMesh->MarkPendingKill();
			return NULL;
		}
		
		if (ImportOptions->bUseT0AsRefPose && bDiffPose)
		{
			// deform skin vertex to the frame 0 from bind pose
			SkinControlPointsToPose( *SkelMeshImporterPtr, FbxMesh, FbxShape, TRUE );
		}
	}
	
	if( SkelMeshImporterPtr->Materials.Num() == FbxMatList.Num() )
	{
		// reorder material according to "SKinXX" in material name
		SetMaterialSkinXXOrder(*SkelMeshImporterPtr, FbxMatList );
	}
	
	if( ImportOptions->bSplitNonMatchingTriangles )
	{
		DoUnSmoothVerts(*SkelMeshImporterPtr);
	}

	// process materials from import data
	ProcessImportMeshMaterials(SkeletalMesh->Materials,*SkelMeshImporterPtr, FALSE);
	
	SkeletalMesh->ClothingAssets.Empty();
	if ( SkeletalMesh->Materials.Num() > 0 )
	{
		SkeletalMesh->ClothingAssets.Add(SkeletalMesh->Materials.Num());
		for (INT i=0; i<SkeletalMesh->Materials.Num(); i++)
		{
			SkeletalMesh->ClothingAssets(i) = NULL;
		}
	}

	// process reference skeleton from import data
	ProcessImportMeshSkeleton(SkeletalMesh->RefSkeleton,SkeletalMesh->SkeletalDepth,*SkelMeshImporterPtr);
	warnf(NAME_Log, TEXT("Bones digested - %i  Depth of hierarchy - %i"), SkeletalMesh->RefSkeleton.Num(), SkeletalMesh->SkeletalDepth );

	// Build map between bone name and bone index now.
	SkeletalMesh->InitNameIndexMap();

	// process bone influences from import data
	ProcessImportMeshInfluences(*SkelMeshImporterPtr);

	check(SkeletalMesh->LODModels.Num() == 0);
	SkeletalMesh->LODModels.Empty();
	new(SkeletalMesh->LODModels)FStaticLODModel();

	SkeletalMesh->LODInfo.Empty();
	SkeletalMesh->LODInfo.AddZeroed();
	SkeletalMesh->LODInfo(0).LODHysteresis = 0.02f;

	// Create initial bounding box based on expanded version of reference pose for meshes without physics assets. Can be overridden by artist.
	FBox BoundingBox( &SkelMeshImporterPtr->Points(0), SkelMeshImporterPtr->Points.Num() );
	FBox Temp = BoundingBox;
	FVector MidMesh		= 0.5f*(Temp.Min + Temp.Max);
	BoundingBox.Min		= Temp.Min + 1.0f*(Temp.Min - MidMesh);
	BoundingBox.Max		= Temp.Max + 1.0f*(Temp.Max - MidMesh);
	// Tuck up the bottom as this rarely extends lower than a reference pose's (e.g. having its feet on the floor).
	// Maya has Y in the vertical, other packages have Z.
	//BEN const INT CoordToTuck = bAssumeMayaCoordinates ? 1 : 2;
	//BEN BoundingBox.Min[CoordToTuck]	= Temp.Min[CoordToTuck] + 0.1f*(Temp.Min[CoordToTuck] - MidMesh[CoordToTuck]);
	BoundingBox.Min[2]	= Temp.Min[2] + 0.1f*(Temp.Min[2] - MidMesh[2]);
	SkeletalMesh->Bounds= FBoxSphereBounds(BoundingBox);


	// copy vertex data needed to generate skinning streams for LOD
	TArray<FVector> LODPoints;
	TArray<FMeshWedge> LODWedges;
	TArray<FMeshFace> LODFaces;
	TArray<FVertInfluence> LODInfluences;
	if( bCreateRenderData )
	{
		SkelMeshImporterPtr->CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences);
	}

	// Store whether or not this mesh has vertex colors
	SkeletalMesh->bHasVertexColors = SkelMeshImporterPtr->bHasVertexColors;

	// process optional import data if available and use it when generating the skinning streams
	FSkelMeshExtraInfluenceImportData* ExtraInfluenceDataPtr = NULL;
	FSkelMeshExtraInfluenceImportData ExtraInfluenceData;
	if( OptionalImportData != NULL )
	{
		if( OptionalImportData->RawMeshInfluencesData.Wedges.Num() > 0 )
		{
			// process reference skeleton from import data
			INT TempSkelDepth=0;
			ProcessImportMeshSkeleton(ExtraInfluenceData.RefSkeleton,TempSkelDepth,OptionalImportData->RawMeshInfluencesData);
			// process bone influences from import data
			ProcessImportMeshInfluences(OptionalImportData->RawMeshInfluencesData);
			// copy vertex data needed for processing the extra vertex influences from import data
			OptionalImportData->RawMeshInfluencesData.CopyLODImportData(
				ExtraInfluenceData.Points,
				ExtraInfluenceData.Wedges,
				ExtraInfluenceData.Faces,
				ExtraInfluenceData.Influences
				);
			ExtraInfluenceDataPtr = &ExtraInfluenceData;
			ExtraInfluenceDataPtr->Usage = OptionalImportData->IntendedUsage;
			ExtraInfluenceDataPtr->MaxBoneCountPerChunk = OptionalImportData->MaxBoneCountPerChunk;
		}
	}


	FStaticLODModel& LODModel = SkeletalMesh->LODModels(0);
	
	// Pass the number of texture coordinate sets to the LODModel.  Ensure there is at least one UV coord
	LODModel.NumTexCoords = Max<UINT>(1,SkelMeshImporterPtr->NumTexCoords);
	if( bCreateRenderData )
	{
		// Create actual rendering data.
		if (!SkeletalMesh->CreateSkinningStreams(LODInfluences,LODWedges,LODFaces,LODPoints,ExtraInfluenceDataPtr))
		{
			SkeletalMesh->MarkPendingKill();
			return NULL;
		}
		
		SkeletalMesh->CalculateRequiredBones(0);

		// Presize the per-section shadow casting array with the number of sections in the imported LOD.
		const INT NumSections = LODModel.Sections.Num();
		SkeletalMesh->LODInfo(0).bEnableShadowCasting.Empty( NumSections );
		for ( INT SectionIndex = 0 ; SectionIndex < NumSections ; ++SectionIndex )
		{
			SkeletalMesh->LODInfo(0).bEnableShadowCasting.AddItem( TRUE );
			SkeletalMesh->LODInfo(0).TriangleSortSettings.AddZeroed();
		}

		// Make RUID.
		SkeletalMesh->SkelMeshRUID = appCreateRuntimeUID();

		if (ExistSkelMeshDataPtr)
		{
			RestoreExistingSkelMeshData(ExistSkelMeshDataPtr, SkeletalMesh);
		}

		SkeletalMesh->CalculateInvRefMatrices();
		SkeletalMesh->PostEditChange();
		SkeletalMesh->MarkPackageDirty();

		// We have to go and fix any AnimSetMeshLinkup objects that refer to this skeletal mesh, as the reference skeleton has changed.
		for(TObjectIterator<UAnimSet> It;It;++It)
		{
			UAnimSet* AnimSet = *It;

			// Get SkeletalMesh path name
			FName SkelMeshName = FName( *SkeletalMesh->GetPathName() );

			// See if we have already cached this Skeletal Mesh.
			const INT* IndexPtr = AnimSet->SkelMesh2LinkupCache.Find( SkelMeshName );

			if( IndexPtr )
			{
				AnimSet->LinkupCache( *IndexPtr ).BuildLinkup( SkeletalMesh, AnimSet );
			}
		}

		// Now iterate over all skeletal mesh components re-initialising them.
		for(TObjectIterator<USkeletalMeshComponent> It; It; ++It)
		{
			USkeletalMeshComponent* SkelComp = *It;
			if(SkelComp->SkeletalMesh == SkeletalMesh && SkelComp->GetScene())
			{
				FComponentReattachContext ReattachContext(SkelComp);
			}
		}
	}

	// create the animset if requested
	// don't import animation when import a morph target
	if (ImportOptions->bImportAnimSet && !FbxShapeArray)
	{
		ImportAnimSet(SkeletalMesh, SortedLinkArray, Filename, NodeArray);
	}
	
	return SkeletalMesh;
}

UObject* UnFbx::CFbxImporter::ReimportSkeletalMesh(USkeletalMesh* Mesh)
{
	char MeshName[1024];
	appStrcpy(MeshName,1024,TCHAR_TO_ANSI(*Mesh->GetName()));
	TArray<FbxNode*>* FbxNodes = NULL;
	UObject* NewMesh = NULL;

	// support to update rigid animation mesh
	ImportOptions->bImportRigidAnim = TRUE;

	// get meshes in Fbx file
	//the function also fill the collision models, so we can update collision models correctly
	TArray< TArray<FbxNode*>* > FbxSkelMeshArray;
	FillFbxSkelMeshArrayInScene(Scene->GetRootNode(), FbxSkelMeshArray, FALSE);

	// if there is only one mesh, use it without name checking 
	// (because the "Used As Full Name" option enables users name the Unreal mesh by themselves
	if (FbxSkelMeshArray.Num() == 1)
	{
		FbxNodes = FbxSkelMeshArray(0);
	}
	else
	{
		// find the Fbx mesh node that the Unreal Mesh matches according to name
		UBOOL bMeshFound = FALSE;
		INT MeshArrayIndex;
		for (MeshArrayIndex = 0; MeshArrayIndex < FbxSkelMeshArray.Num() && !bMeshFound; MeshArrayIndex++)
		{
			FbxNodes = FbxSkelMeshArray(MeshArrayIndex);
			INT NodeIndex;
			for (NodeIndex = 0; NodeIndex < FbxNodes->Num(); NodeIndex++ )
			{
				const char* FbxMeshName = (*FbxNodes)(NodeIndex)->GetName();
				// The name of Unreal mesh may have a prefix, so we match from end
				UINT i = 0;
				char* MeshPtr = MeshName + strlen(MeshName) - 1;
				if (strlen(FbxMeshName) <= strlen(MeshName))
				{
					const char* FbxMeshPtr = FbxMeshName + strlen(FbxMeshName) - 1;
					while (i < strlen(FbxMeshName))
					{
						if (*MeshPtr != *FbxMeshPtr)
						{
							break;
						}
						else
						{
							i++;
							MeshPtr--;
							FbxMeshPtr--;
						}
					}
				}

				if (i == strlen(FbxMeshName)) // matched
				{
					// check further
					if ( strlen(FbxMeshName) == strlen(MeshName) ||  // the name of Unreal mesh is full match
						*MeshPtr == '_')							 // or the name of Unreal mesh has a prefix
					{
						bMeshFound = TRUE;
						break;
					}
				}
			}
		}

		if (!bMeshFound)
		{
			FbxNodes = NULL;
		}
	}

	if (FbxNodes)
	{
		// set import options, how about others?
		ImportOptions->bImportMaterials = FALSE;
		ImportOptions->bImportTextures = FALSE;
		ImportOptions->bImportAnimSet = FALSE;

		// check if there is LODGroup for this skeletal mesh
		INT MaxLODLevel = 1;
		for (INT j = 0; j < (*FbxNodes).Num(); j++)
		{
			FbxNode* Node = (*FbxNodes)(j);
			if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
			{
				// get max LODgroup level
				if (MaxLODLevel < Node->GetChildCount())
				{
					MaxLODLevel = Node->GetChildCount();
				}
			}
		}

		INT LODIndex;
		for (LODIndex = 0; LODIndex < MaxLODLevel; LODIndex++)
		{
			TArray<FbxNode*> SkelMeshNodeArray;
			for (INT j = 0; j < (*FbxNodes).Num(); j++)
			{
				FbxNode* Node = (*FbxNodes)(j);
				if (Node->GetNodeAttribute() && Node->GetNodeAttribute()->GetAttributeType() == FbxNodeAttribute::eLODGROUP)
				{
					if (Node->GetChildCount() > LODIndex)
					{
						SkelMeshNodeArray.AddItem(Node->GetChild(LODIndex));
					}
					else // in less some LODGroups have less level, use the last level
					{
						SkelMeshNodeArray.AddItem(Node->GetChild(Node->GetChildCount() - 1));
					}
				}
				else
				{
					SkelMeshNodeArray.AddItem(Node);
				}
			}

			if (LODIndex == 0)
			{
				NewMesh = ImportSkeletalMesh( Mesh->GetOuter(), SkelMeshNodeArray, *Mesh->GetName(), RF_Public|RF_Standalone, Mesh->SourceFilePath );
			}
			else if (NewMesh) // the base skeletal mesh is imported successfully
			{
				USkeletalMesh* BaseSkeletalMesh = Cast<USkeletalMesh>(NewMesh);
				UObject *LODObject = ImportSkeletalMesh( UObject::GetTransientPackage(), SkelMeshNodeArray, NAME_None, 0, Mesh->SourceFilePath );
				ImportSkeletalMeshLOD( Cast<USkeletalMesh>(LODObject), BaseSkeletalMesh, LODIndex);

				// Set LOD Model's DisplayFactor
				
				// TODO: reserve the DisplayFactor instead of resetting
				//BaseSkeletalMesh->LODInfo(LODIndex).DisplayFactor = 1.0 / MaxLODLevel * LODIndex;
			}

			// import morph target
			if ( NewMesh)
			{
				ImportFbxMorphTarget(SkelMeshNodeArray, Cast<USkeletalMesh>(NewMesh), Mesh->SourceFilePath, LODIndex);
			}
		}
	}
	else
	{
		// no mesh found in the FBX file
		warnf(NAME_Log,TEXT("No FBX mesh matches the Unreal mesh %s."), *Mesh->GetName());
	}

	return NewMesh;
}

void UnFbx::CFbxImporter::ImportAlterWeightSkelMesh(FSkeletalMeshBinaryImport& OutRawMeshInfluencesData)
{
	TArray< TArray<FbxNode*>* > SkelMeshArray;
	TArray<FbxNode*> Nodes;

	FillFbxSkelMeshArrayInScene(Scene->GetRootNode(), SkelMeshArray, TRUE);
	
	if (SkelMeshArray.Num() == 0)
	{
		appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_NoFbxSkelMeshFound") );
	}
	else if (SkelMeshArray.Num() >= 1)
	{
		if (SkelMeshArray.Num() > 1)
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Prompt_FirstOneFbxMeshUsed") );
		}
		
		FbxNode* Node;
		FbxMesh* FbxMesh;
		FbxSkin* Skin;
		
		Nodes = *SkelMeshArray(0);
		
		TArray<FString> FbxMatList;
		ImportMaterialsForSkelMesh(Nodes, OutRawMeshInfluencesData, FbxMatList);

		TArray<FbxNode*> SortedLinkArray;

		UBOOL bDiffPose;
		ImportBone(Nodes, OutRawMeshInfluencesData, SortedLinkArray, bDiffPose, FALSE);
	
		for ( INT i = 0; i < Nodes.Num(); i++)
		{
			Node = Nodes(i);
			FbxMesh = Node->GetMesh();

			Skin = (FbxSkin*)FbxMesh->GetDeformer(0, FbxDeformer::eSKIN);
			check (Skin != NULL);
			FillSkelMeshImporterFromFbx( OutRawMeshInfluencesData, FbxMesh, Skin, NULL, SortedLinkArray, FbxMatList );
		}
		
		if( ImportOptions->bSplitNonMatchingTriangles )
		{
			DoUnSmoothVerts(OutRawMeshInfluencesData);
		}

		for (INT i=0; i<SkelMeshArray.Num(); i++)
		{
			delete SkelMeshArray(i);
		}
	}
}


void UnFbx::CFbxImporter::SetMaterialSkinXXOrder(FSkeletalMeshBinaryImport& SkelMeshImporter, const TArray<FString>& FbxMatList )
{
	TArray<INT> SkinIndex;
	TArray<INT> MaterialIndexMapping;

	check( SkelMeshImporter.Materials.Num() == FbxMatList.Num() );
	INT MaterialCount = SkelMeshImporter.Materials.Num();
	SkinIndex.Add(MaterialCount);
	
	for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		// get skin index
		FString MatName = FbxMatList(MaterialIndex);
		UBOOL Found = FALSE;
		if (MatName.Len() > 6)
		{
			FString SkinXX = MatName.Right( 6 ).ToUpper();
			if (SkinXX[0] == 'S' && SkinXX[1] == 'K' && SkinXX[2] == 'I' && SkinXX[3] == 'N')
			{
				if ( appIsDigit( SkinXX[4] ) && appIsDigit( SkinXX[5] ) )
				{
					Found = TRUE;
					
					
					INT TmpIndex = appAtoi(&SkinXX[4]);
					SkinIndex(MaterialIndex) = TmpIndex;

					// remove the 'skinXX' suffix from the material name					
					INT MatNameLen = appStrlen(SkelMeshImporter.Materials(MaterialIndex).MaterialName);
					SkelMeshImporter.Materials(MaterialIndex).MaterialName[MatNameLen - 6] = '\0';
				}
			}
		}
		if (!Found)
		{
			SkinIndex(MaterialIndex) = 0;
		}
		
		INT i;
		for ( i=0; i<MaterialIndex; i++)
		{
			if ( SkinIndex(MaterialIndexMapping(i)) > SkinIndex(MaterialIndex) )
			{
				MaterialIndexMapping.InsertItem(MaterialIndex, i);
				break;
			}
		}

		if ( i == MaterialIndex )
		{
			MaterialIndexMapping.AddItem(MaterialIndex);
		}
	}
	
	// check if reorder is needed
	UBOOL bNeedReorder = FALSE;
	for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		if (MaterialIndexMapping(MaterialIndex) != MaterialIndex)
		{
			bNeedReorder = TRUE;
			break;
		}
	}
	
	if (bNeedReorder)
	{
		// re-order the materials
		TArray<char *> MatNames;
		MatNames.Add(MaterialCount);
		
		for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			// get skin index
			// Unreal Material name has less 64 chars
			char* MatName = new char[64];
			strcpy_s(MatName, strlen(SkelMeshImporter.Materials(MaterialIndex).MaterialName) + 1, SkelMeshImporter.Materials(MaterialIndex).MaterialName);
			MatNames(MaterialIndex) = MatName;
		}
		
		for(INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
		{
			strcpy_s (SkelMeshImporter.Materials(MaterialIndex).MaterialName,
					  strlen(MatNames(MaterialIndexMapping(MaterialIndex))) + 1,
					  MatNames(MaterialIndexMapping(MaterialIndex)));
			delete[] MatNames(MaterialIndexMapping(MaterialIndex));
		}
		
		// remapping the material index for each triangle
		INT FaceNum = SkelMeshImporter.Faces.Num();
		for( INT TriangleIndex = 0; TriangleIndex < FaceNum ; TriangleIndex++)
		{
			VTriangle& Triangle = SkelMeshImporter.Faces(TriangleIndex);
			Triangle.MatIndex = MaterialIndexMapping(Triangle.MatIndex);
		}
	}
}

UBOOL UnFbx::CFbxImporter::FillSkelMeshImporterFromFbx( FSkeletalMeshBinaryImport& SkelMeshImporter, FbxMesh*& Mesh, FbxSkin* Skin, FbxShape* FbxShape, TArray<FbxNode*> &SortedLinks, TArray<FString>& FbxMatList)
{
	FbxNode* Node = Mesh->GetNode();

	//remove the bad polygons before getting any data from mesh
	Mesh->RemoveBadPolygons();

	//Get the base layer of the mesh
	FbxLayer* BaseLayer = Mesh->GetLayer(0);
	if (BaseLayer == NULL)
	{
		warnf(NAME_Error,TEXT("There is no geometry information in mesh"),ANSI_TO_TCHAR(Mesh->GetName()));
		return FALSE;
	}

	// Do some checks before proceeding, check to make sure the number of bones does not exceed the maximum supported
	if(SortedLinks.Num() > MAX_BONES)
	{
		warnf(NAME_Error,LocalizeSecure(LocalizeUnrealEd("SkelMeshImporter_MaxBonesExceededError"),ANSI_TO_TCHAR(Node->GetName()),SortedLinks.Num(),MAX_BONES));
		return FALSE;
	}

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	// mapping from UVSets to Fbx LayerElementUV
	// Fbx UVSets may be duplicated, remove the duplicated UVSets in the mapping 
	INT LayerCount = Mesh->GetLayerCount();
	TArray<FString> UVSets;

	UVSets.Empty();
	if (LayerCount > 0)
	{
		INT UVLayerIndex;
		for (UVLayerIndex = 0; UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			INT UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (INT UVIndex = 0; UVIndex<UVSetCount; UVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[UVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = ANSI_TO_TCHAR(UVSetName);

						UVSets.AddUniqueItem(LocalUVSetName);
					}
				}
			}
		}
	}

	// If the the UV sets are named using the following format (UVChannel_X; where X ranges from 1 to 4)
	// we will re-order them based on these names.  Any UV sets that do not follow this naming convention
	// will be slotted into available spaces.
	if( UVSets.Num() > 0 )
	{
		for(INT ChannelNumIdx = 0; ChannelNumIdx < 4; ChannelNumIdx++)
		{
			FString ChannelName = FString::Printf( TEXT("UVChannel_%d"), ChannelNumIdx+1 );
			INT SetIdx = UVSets.FindItemIndex( ChannelName );

			// If the specially formatted UVSet name appears in the list and it is in the wrong spot,
			// we will swap it into the correct spot.
			if( SetIdx != INDEX_NONE && SetIdx != ChannelNumIdx )
			{
				// If we are going to swap to a position that is outside the bounds of the
				// array, then we pad out to that spot with empty data.
				for(INT ArrSize = UVSets.Num(); ArrSize < ChannelNumIdx+1; ArrSize++)
				{
					UVSets.AddItem( FString(TEXT("")) );
				}
				//Swap the entry into the appropriate spot.
				UVSets.SwapItems( SetIdx, ChannelNumIdx );
			}
		}
	}

	//
	// setup materials index mapping
	//
	INT MaterialCount = Node->GetMaterialCount();
	INT ExistedMatNum = SkelMeshImporter.Materials.Num();
	// for skeletal mesh, only one UVSet is used, so the variable UVSets is useless, just used as parameter
	TArray<INT> MaterialMapping;

	MaterialMapping.Add(MaterialCount);

	for (INT MaterialIndex=0; MaterialIndex < MaterialCount; ++MaterialIndex)
	{
		FbxSurfaceMaterial *FbxMaterial = Node->GetMaterial(MaterialIndex);

		// create unreal materials here because we get UVset data now
		if (ImportOptions->bImportMaterials)
		{
			TArray<UMaterialInterface*> Materials;
			Materials.Empty();

			CreateUnrealMaterial(FbxMaterial,Materials,UVSets);
		}

		INT ExistMatIndex;
		for ( ExistMatIndex = 0; ExistMatIndex<ExistedMatNum; ExistMatIndex++)
		{
			if ( FbxMatList(ExistMatIndex) == ANSI_TO_TCHAR(FbxMaterial->GetName()) )
			{
				MaterialMapping(MaterialIndex) = ExistMatIndex;
				break;
			}
		}
	}

	if( ImportOptions->bSplitNonMatchingTriangles )
	{
		// Check and see if the smooothing data is valid.  If not generate it from the normals
		BaseLayer = Mesh->GetLayer(0);
		const FbxLayerElementSmoothing* SmoothingLayer = BaseLayer->GetSmoothing();

		if( SmoothingLayer )
		{
			UBOOL bValidSmoothingData = FALSE;
			FbxLayerElementArrayTemplate<int>& Array = SmoothingLayer->GetDirectArray();
			for( INT SmoothingIndex = 0; SmoothingIndex < Array.GetCount(); ++SmoothingIndex )
			{
				if( Array[SmoothingIndex] != 0 )
				{
					bValidSmoothingData = TRUE;
					break;
				}
			}

			if( !bValidSmoothingData )
			{
				GeometryConverter->ComputeEdgeSmoothingFromNormals(Mesh);
			}
		}
	}

	// Must do this before triangulating the mesh due to an FBX bug in TriangulateMeshAdvance
	INT LayerSmoothingCount = Mesh->GetLayerCount(FbxLayerElement::eSMOOTHING);
	for(INT i = 0; i < LayerSmoothingCount; i++)
	{
		GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing (Mesh, i);
	}

	//
	// Convert data format to unreal-compatible
	//
	UBOOL bDestroyMesh = FALSE;
	if (!Mesh->IsTriangleMesh())
	{
		warnf(NAME_Log,TEXT("Triangulating skeletal mesh %s"), ANSI_TO_TCHAR(Node->GetName()));
		
		UBOOL bSuccess = GeometryConverter->TriangulateInPlace(Mesh->GetNode());

		if (bSuccess == FALSE)
		{
			warnf(NAME_Error,TEXT("Unable to triangulate mesh"));
			const FbxError& Error = Mesh->GetError();
			const INT ErrorStrCount = Error.GetErrorCount();
			for (INT ErrorStrIndex = 0 ; ErrorStrIndex < ErrorStrCount; ++ErrorStrIndex)
			{
				warnf(NAME_Error,TEXT(" Fbx error %d/%d: [%s]"), ErrorStrIndex+1, ErrorStrIndex, ANSI_TO_TCHAR(Error.GetErrorString(ErrorStrIndex)));
			}
			return FALSE;
		}
		else
		{
			Mesh = Node->GetMesh();
		}
	}
	
	// renew the base layer
	BaseLayer = Mesh->GetLayer(0);
	Skin = (FbxSkin*)static_cast<FbxGeometry*>(Mesh)->GetDeformer(0, FbxDeformer::eSKIN);

	//
	//	store the UVs in arrays for fast access in the later looping of triangles 
	//
	UINT UniqueUVCount = UVSets.Num();
	FbxLayerElementUV** LayerElementUV = NULL;
	FbxLayerElement::EReferenceMode* UVReferenceMode = NULL;
	FbxLayerElement::EMappingMode* UVMappingMode = NULL;
	if (UniqueUVCount > 0)
	{
		LayerElementUV = new FbxLayerElementUV*[UniqueUVCount];
		UVReferenceMode = new FbxLayerElement::EReferenceMode[UniqueUVCount];
		UVMappingMode = new FbxLayerElement::EMappingMode[UniqueUVCount];
	}
	else
	{
		warnf(NAME_Log,TEXT("Mesh %s has no UV set. Creating a default set."), ANSI_TO_TCHAR(Node->GetName()) );
	}
	
	LayerCount = Mesh->GetLayerCount();
	for (UINT UVIndex = 0; UVIndex < UniqueUVCount; UVIndex++)
	{
		UBOOL bFoundUV = FALSE;
		LayerElementUV[UVIndex] = NULL;
		for (INT UVLayerIndex = 0; !bFoundUV && UVLayerIndex<LayerCount; UVLayerIndex++)
		{
			FbxLayer* lLayer = Mesh->GetLayer(UVLayerIndex);
			INT UVSetCount = lLayer->GetUVSetCount();
			if(UVSetCount)
			{
				FbxArray<FbxLayerElementUV const*> EleUVs = lLayer->GetUVSets();
				for (INT FbxUVIndex = 0; FbxUVIndex<UVSetCount; FbxUVIndex++)
				{
					FbxLayerElementUV const* ElementUV = EleUVs[FbxUVIndex];
					if (ElementUV)
					{
						const char* UVSetName = ElementUV->GetName();
						FString LocalUVSetName = ANSI_TO_TCHAR(UVSetName);
						if (LocalUVSetName == UVSets(UVIndex))
						{
							LayerElementUV[UVIndex] = const_cast<FbxLayerElementUV*>(ElementUV);
							UVReferenceMode[UVIndex] = LayerElementUV[FbxUVIndex]->GetReferenceMode();
							UVMappingMode[UVIndex] = LayerElementUV[FbxUVIndex]->GetMappingMode();
							break;
						}
					}
				}
			}
		}
	}

	//
	// get the smoothing group layer
	//
	UBOOL bSmoothingAvailable = FALSE;

	FbxLayerElementSmoothing const* SmoothingInfo = BaseLayer->GetSmoothing();
	FbxLayerElement::EReferenceMode SmoothingReferenceMode(FbxLayerElement::eDIRECT);
	FbxLayerElement::EMappingMode SmoothingMappingMode(FbxLayerElement::eBY_EDGE);
	if (SmoothingInfo)
	{
 		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eBY_EDGE )
		{
			if (!GeometryConverter->ComputePolygonSmoothingFromEdgeSmoothing(Mesh))
			{
				warnf(NAME_Warning,TEXT("Unable to fully convert the smoothing groups for mesh %s"),ANSI_TO_TCHAR(Mesh->GetName()));
				bSmoothingAvailable = FALSE;
			}
		}

		if( SmoothingInfo->GetMappingMode() == FbxLayerElement::eBY_POLYGON )
		{
			bSmoothingAvailable = TRUE;
		}


		SmoothingReferenceMode = SmoothingInfo->GetReferenceMode();
		SmoothingMappingMode = SmoothingInfo->GetMappingMode();
	}


	//
	//	get the "material index" layer
	//
	FbxLayerElementMaterial* LayerElementMaterial = BaseLayer->GetMaterials();
	FbxLayerElement::EMappingMode MaterialMappingMode = LayerElementMaterial ? 
		LayerElementMaterial->GetMappingMode() : FbxLayerElement::eBY_POLYGON;

	UniqueUVCount = Min<UINT>( UniqueUVCount, MAX_TEXCOORDS );

	// One UV set is required but only import up to MAX_TEXCOORDS number of uv layers
	SkelMeshImporter.NumTexCoords = Max<UINT>( SkelMeshImporter.NumTexCoords, UniqueUVCount );

	//
	// get the first vertex color layer
	//
	FbxLayerElementVertexColor* LayerElementVertexColor = BaseLayer->GetVertexColors();
	FbxLayerElement::EReferenceMode VertexColorReferenceMode(FbxLayerElement::eDIRECT);
	FbxLayerElement::EMappingMode VertexColorMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
	if (LayerElementVertexColor)
	{
		VertexColorReferenceMode = LayerElementVertexColor->GetReferenceMode();
		VertexColorMappingMode = LayerElementVertexColor->GetMappingMode();
		SkelMeshImporter.bHasVertexColors = TRUE;
	}

	//
	// get the first normal layer
	//
	FbxLayerElementNormal* LayerElementNormal = BaseLayer->GetNormals();
	FbxLayerElementTangent* LayerElementTangent = BaseLayer->GetTangents();
	FbxLayerElementBinormal* LayerElementBinormal = BaseLayer->GetBinormals();

	//whether there is normal, tangent and binormal data in this mesh
	UBOOL bHasNTBInformation = LayerElementNormal && LayerElementTangent && LayerElementBinormal;

	FbxLayerElement::EReferenceMode NormalReferenceMode(FbxLayerElement::eDIRECT);
	FbxLayerElement::EMappingMode NormalMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
	if (LayerElementNormal)
	{
		NormalReferenceMode = LayerElementNormal->GetReferenceMode();
		NormalMappingMode = LayerElementNormal->GetMappingMode();
	}

	FbxLayerElement::EReferenceMode TangentReferenceMode(FbxLayerElement::eDIRECT);
	FbxLayerElement::EMappingMode TangentMappingMode(FbxLayerElement::eBY_CONTROL_POINT);
	if (LayerElementTangent)
	{
		TangentReferenceMode = LayerElementTangent->GetReferenceMode();
		TangentMappingMode = LayerElementTangent->GetMappingMode();
	}

	//
	// create the points / wedges / faces
	//
	INT ControlPointsCount = Mesh->GetControlPointsCount();
	INT ExistPointNum = SkelMeshImporter.Points.Num();
	SkelMeshImporter.Points.Add(ControlPointsCount);

	// Construct the matrices for the conversion from right handed to left handed system
	FbxAMatrix TotalMatrix;
	FbxAMatrix TotalMatrixForNormal;
	TotalMatrix = ComputeTotalMatrix(Node);
	TotalMatrixForNormal = TotalMatrix.Inverse();
	TotalMatrixForNormal = TotalMatrixForNormal.Transpose();

	INT ControlPointsIndex;
	for( ControlPointsIndex = 0 ; ControlPointsIndex < ControlPointsCount ;ControlPointsIndex++ )
	{
		FbxVector4 Position;
		if (FbxShape)
		{
			Position = FbxShape->GetControlPoints()[ControlPointsIndex];
		}
		else
		{
			Position = Mesh->GetControlPoints()[ControlPointsIndex];
		}																	 
		FbxVector4 FinalPosition;
		FinalPosition = TotalMatrix.MultT(Position);
		SkelMeshImporter.Points(ControlPointsIndex+ExistPointNum) = Converter.ConvertPos(FinalPosition);
	}
	
	UBOOL OddNegativeScale = IsOddNegativeScale(TotalMatrix);
	
	INT VertexIndex;
	INT TriangleCount = Mesh->GetPolygonCount();
	INT ExistFaceNum = SkelMeshImporter.Faces.Num();
	SkelMeshImporter.Faces.Add( TriangleCount );
	INT ExistWedgesNum = SkelMeshImporter.Wedges.Num();
	VVertex TmpWedges[3];

	for( INT TriangleIndex = ExistFaceNum, LocalIndex = 0 ; TriangleIndex < ExistFaceNum+TriangleCount ; TriangleIndex++, LocalIndex++ )
	{

		VTriangle& Triangle = SkelMeshImporter.Faces(TriangleIndex);

		Triangle.bOverrideTangentBasis = bHasNTBInformation && (ImportOptions->bOverrideTangents);

		//
		// smoothing mask
		//
		// set the face smoothing by default. It could be any number, but not zero
		Triangle.SmoothingGroups = 255; 
		if ( bSmoothingAvailable)
		{
			if (SmoothingInfo)
			{
				if (SmoothingMappingMode == FbxLayerElement::eBY_POLYGON)
				{
					INT lSmoothingIndex = (SmoothingReferenceMode == FbxLayerElement::eDIRECT) ? LocalIndex : SmoothingInfo->GetIndexArray().GetAt(LocalIndex);
					Triangle.SmoothingGroups = SmoothingInfo->GetDirectArray().GetAt(lSmoothingIndex);
				}
				else
				{
					warnf(NAME_Warning,TEXT("Unsupported Smoothing group mapping mode on mesh %s"),ANSI_TO_TCHAR(Mesh->GetName()));
				}
			}
		}

		for (VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			INT ControlPointIndex = Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
			//
			// normals, tangents and binormals
			//
			if( Triangle.bOverrideTangentBasis )
			{
				INT TmpIndex = LocalIndex*3 + VertexIndex;
				//normals may have different reference and mapping mode than tangents and binormals
				INT NormalMapIndex = (NormalMappingMode == FbxLayerElement::eBY_CONTROL_POINT) ? 
										ControlPointIndex : TmpIndex;
				INT NormalValueIndex = (NormalReferenceMode == FbxLayerElement::eDIRECT) ? 
										NormalMapIndex : LayerElementNormal->GetIndexArray().GetAt(NormalMapIndex);

				//tangents and binormals share the same reference, mapping mode and index array
				INT TangentMapIndex = TmpIndex;

				FbxVector4 TempValue = LayerElementTangent->GetDirectArray().GetAt(TangentMapIndex);
				Triangle.TangentX[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);

				TempValue = LayerElementBinormal->GetDirectArray().GetAt(TangentMapIndex);
				Triangle.TangentY[ UnrealVertexIndex ] = -Converter.ConvertDir(TempValue);

				TempValue = LayerElementNormal->GetDirectArray().GetAt(NormalValueIndex);
				TempValue = TotalMatrixForNormal.MultT(TempValue);
				Triangle.TangentZ[ UnrealVertexIndex ] = Converter.ConvertDir(TempValue);
			}
			else
			{
				INT NormalIndex;
				for( NormalIndex = 0; NormalIndex < 3; ++NormalIndex )
				{
					Triangle.TangentX[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
					Triangle.TangentY[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
					Triangle.TangentZ[ NormalIndex ] = FVector( 0.0f, 0.0f, 0.0f );
				}
			}
		}
		
		//
		// material index
		//
		Triangle.MatIndex = 0; // default value
		if (MaterialCount>0)
		{
			if (LayerElementMaterial)
			{
				switch(MaterialMappingMode)
				{
				// material index is stored in the IndexArray, not the DirectArray (which is irrelevant with 2009.1)
				case FbxLayerElement::eALL_SAME:
					{	
						Triangle.MatIndex = MaterialMapping(LayerElementMaterial->GetIndexArray().GetAt(0));
					}
					break;
				case FbxLayerElement::eBY_POLYGON:
					{	
						INT Index = LayerElementMaterial->GetIndexArray().GetAt(LocalIndex);
						if( !MaterialMapping.IsValidIndex(Index) )
						{
							warnf(NAME_Log,TEXT("Face material index inconsistency - forcing to 0"));
						}
						else
						{
							Triangle.MatIndex = MaterialMapping(Index);
						}
					}
					break;
				}
			}
			// When import morph, we don't check the material index 
			// because we don't import material for morph, so the SkelMeshImporter.Materials contains zero material
			if ( !FbxShape && (Triangle.MatIndex < 0 ||  Triangle.MatIndex >= SkelMeshImporter.Materials.Num() ) )
			{
				warnf(NAME_Log,TEXT("Face material index inconsistency - forcing to 0"));
				Triangle.MatIndex = 0;
			}
		}
		Triangle.AuxMatIndex = 0;
		for (VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			// If there are odd number negative scale, invert the vertex order for triangles
			INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

			TmpWedges[UnrealVertexIndex].MatIndex = Triangle.MatIndex;
			TmpWedges[UnrealVertexIndex].VertexIndex = ExistPointNum + Mesh->GetPolygonVertex(LocalIndex,VertexIndex);
			// Initialize all colors to white.
			TmpWedges[UnrealVertexIndex].Color = FColor(255,255,255);
		}

		//
		// uvs
		//
		UINT UVLayerIndex;
		// Some FBX meshes can have no UV sets, so also check the UniqueUVCount
		for ( UVLayerIndex = 0; UVLayerIndex< UniqueUVCount; UVLayerIndex++ )
		{
			// ensure the layer has data
			if (LayerElementUV[UVLayerIndex] != NULL) 
			{
				// Get each UV from the layer
				for (VertexIndex=0;VertexIndex<3;VertexIndex++)
				{
					// If there are odd number negative scale, invert the vertex order for triangles
					INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

					INT lControlPointIndex = Mesh->GetPolygonVertex(LocalIndex, VertexIndex);
					INT UVMapIndex = (UVMappingMode[UVLayerIndex] == FbxLayerElement::eBY_CONTROL_POINT) ? 
							lControlPointIndex : LocalIndex*3+VertexIndex;
					INT UVIndex = (UVReferenceMode[UVLayerIndex] == FbxLayerElement::eDIRECT) ? 
							UVMapIndex : LayerElementUV[UVLayerIndex]->GetIndexArray().GetAt(UVMapIndex);
					FbxVector2	UVVector = LayerElementUV[UVLayerIndex]->GetDirectArray().GetAt(UVIndex);

					TmpWedges[UnrealVertexIndex].UVs[ UVLayerIndex ].X = static_cast<float>(UVVector[0]);
					TmpWedges[UnrealVertexIndex].UVs[ UVLayerIndex ].Y = 1.f - static_cast<float>(UVVector[1]);
				}
			}
			else if( UVLayerIndex == 0 )
			{
				// Set all UV's to zero.  If we are here the mesh had no UV sets so we only need to do this for the
				// first UV set which always exists.
				TmpWedges[VertexIndex].UVs[ UVLayerIndex ].X = 0.0f;
				TmpWedges[VertexIndex].UVs[ UVLayerIndex ].Y = 0.0f;
			}
		}

		// Read vertex colors if they exist.
		if( LayerElementVertexColor )
		{
			switch(VertexColorMappingMode)
			{
			case FbxLayerElement::eBY_CONTROL_POINT:
				{
					INT VertexIndex;
					for (VertexIndex=0;VertexIndex<3;VertexIndex++)
					{
						INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

						FbxColor VertexColor = (VertexColorReferenceMode == FbxLayerElement::eDIRECT)
							?	LayerElementVertexColor->GetDirectArray().GetAt(Mesh->GetPolygonVertex(LocalIndex,VertexIndex))
							:	LayerElementVertexColor->GetDirectArray().GetAt(LayerElementVertexColor->GetIndexArray().GetAt(Mesh->GetPolygonVertex(LocalIndex,VertexIndex)));

						TmpWedges[UnrealVertexIndex].Color =	FColor(	BYTE(255.f*VertexColor.mRed),
																BYTE(255.f*VertexColor.mGreen),
																BYTE(255.f*VertexColor.mBlue),
																BYTE(255.f*VertexColor.mAlpha));
					}
				}
				break;
			case FbxLayerElement::eBY_POLYGON_VERTEX:
				{	
					INT VertexIndex;
					for (VertexIndex=0;VertexIndex<3;VertexIndex++)
					{
						INT UnrealVertexIndex = OddNegativeScale ? 2 - VertexIndex : VertexIndex;

						FbxColor VertexColor = (VertexColorReferenceMode == FbxLayerElement::eDIRECT)
							?	LayerElementVertexColor->GetDirectArray().GetAt(LocalIndex*3+VertexIndex)
							:	LayerElementVertexColor->GetDirectArray().GetAt(LayerElementVertexColor->GetIndexArray().GetAt(LocalIndex*3+VertexIndex));

						TmpWedges[UnrealVertexIndex].Color =	FColor(	BYTE(255.f*VertexColor.mRed),
																BYTE(255.f*VertexColor.mGreen),
																BYTE(255.f*VertexColor.mBlue),
																BYTE(255.f*VertexColor.mAlpha));
					}
				}
				break;
			}
		}
		
		//
		// basic wedges matching : 3 unique per face. TODO Can we do better ?
		//
		for (VertexIndex=0; VertexIndex<3; VertexIndex++)
		{
			INT w;
			
			w = SkelMeshImporter.Wedges.Add();
			SkelMeshImporter.Wedges(w).VertexIndex = TmpWedges[VertexIndex].VertexIndex;
			SkelMeshImporter.Wedges(w).MatIndex = TmpWedges[VertexIndex].MatIndex;
			SkelMeshImporter.Wedges(w).Color = TmpWedges[VertexIndex].Color;
			SkelMeshImporter.Wedges(w).Reserved = 0;
			appMemcpy( SkelMeshImporter.Wedges(w).UVs, TmpWedges[VertexIndex].UVs, sizeof(FVector2D)*MAX_TEXCOORDS );
			
			Triangle.WedgeIndex[VertexIndex] = w;
		}
		
	}
	
	// now we can work on a per-cluster basis with good ordering
	if (Skin) // skeletal mesh
	{
		// create influences for each cluster
		INT ClusterIndex;
		for (ClusterIndex=0; ClusterIndex<Skin->GetClusterCount(); ClusterIndex++)
		{
			FbxCluster* Cluster = Skin->GetCluster(ClusterIndex);
			// When Maya plug-in exports rigid binding, it will generate "CompensationCluster" for each ancestor links.
			// FBX writes these "CompensationCluster" out. The CompensationCluster also has weight 1 for vertices.
			// Unreal importer should skip these clusters.
			if(Cluster && appStrcmpANSI(Cluster->GetUserDataID(), "Maya_ClusterHint") == 0 && appStrcmpANSI(Cluster->GetUserData(), "CompensationCluster")  == 0)
			{
				continue;
			}
			
			FbxNode* Link = Cluster->GetLink();
			// find the bone index
			INT BoneIndex = -1;
			for (INT LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
			{
				if (Link == SortedLinks(LinkIndex))
				{
					BoneIndex = LinkIndex;
					break;
				}
			}

			//	get the vertex indices
			INT ControlPointIndicesCount = Cluster->GetControlPointIndicesCount();
			INT* ControlPointIndices = Cluster->GetControlPointIndices();
			DOUBLE* Weights = Cluster->GetControlPointWeights();

			//	for each vertex index in the cluster
			for (INT ControlPointIndex = 0; ControlPointIndex < ControlPointIndicesCount; ++ControlPointIndex) 
			{
				SkelMeshImporter.Influences.Add();
				SkelMeshImporter.Influences.Last().BoneIndex = BoneIndex;
				SkelMeshImporter.Influences.Last().Weight = static_cast<float>(Weights[ControlPointIndex]);
				SkelMeshImporter.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndices[ControlPointIndex];
			}
		}
	}
	else // for rigid mesh
	{
		// find the bone index
		INT BoneIndex = -1;
		for (INT LinkIndex = 0; LinkIndex < SortedLinks.Num(); LinkIndex++)
		{
			// the bone is the node itself
			if (Node == SortedLinks(LinkIndex))
			{
				BoneIndex = LinkIndex;
				break;
			}
		}
		
		//	for each vertex in the mesh
		for (INT ControlPointIndex = 0; ControlPointIndex < ControlPointsCount; ++ControlPointIndex) 
		{
			SkelMeshImporter.Influences.Add();
			SkelMeshImporter.Influences.Last().BoneIndex = BoneIndex;
			SkelMeshImporter.Influences.Last().Weight = 1.0;
			SkelMeshImporter.Influences.Last().VertexIndex = ExistPointNum + ControlPointIndex;
		}
	}

	//
	// clean up
	//
	if (UniqueUVCount > 0)
	{
		delete[] LayerElementUV;
		delete[] UVReferenceMode;
		delete[] UVMappingMode;
	}
	
	if( bDestroyMesh )
	{
		Mesh->Destroy( TRUE );
	}

	return TRUE;
}

extern void CalculateBoneWeightInfluences(USkeletalMesh* SkeletalMesh, TArray<FBoneIndexPair>& BonePairs);

void UnFbx::CFbxImporter::ImportSkeletalMeshLOD(USkeletalMesh* InSkeletalMesh, USkeletalMesh* BaseSkeletalMesh, INT DesiredLOD)
{
	check(InSkeletalMesh);
	// Now we copy the base FStaticLODModel from the imported skeletal mesh as the new LOD in the selected mesh.
	check(InSkeletalMesh->LODModels.Num() == 1);

	// Names of root bones must match.
	// If the names of root bones don't match, the LOD Mesh does not share skeleton with base Mesh. So rename 
	// the root bone's name of LOD Mesh.
	if(InSkeletalMesh->RefSkeleton(0).Name != BaseSkeletalMesh->RefSkeleton(0).Name)
	{
		InSkeletalMesh->RefSkeleton(0).Name = BaseSkeletalMesh->RefSkeleton(0).Name;	   
	}

	// We do some checking here that for every bone in the mesh we just imported, it's in our base ref skeleton, and the parent is the same.
	for(INT i=0; i<InSkeletalMesh->RefSkeleton.Num(); i++)
	{
		INT LODBoneIndex = i;
		FName LODBoneName = InSkeletalMesh->RefSkeleton(LODBoneIndex).Name;
		INT BaseBoneIndex = BaseSkeletalMesh->MatchRefBone(LODBoneName);
		if( BaseBoneIndex == INDEX_NONE )
		{
			// If we could not find the bone from this LOD in base mesh - we fail.
			appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODBoneDoesNotMatch"), *LODBoneName.ToString(), *BaseSkeletalMesh->GetName()) );
			return;
		}

		if(i>0)
		{
			INT LODParentIndex = InSkeletalMesh->RefSkeleton(LODBoneIndex).ParentIndex;
			FName LODParentName = InSkeletalMesh->RefSkeleton(LODParentIndex).Name;

			INT BaseParentIndex = BaseSkeletalMesh->RefSkeleton(BaseBoneIndex).ParentIndex;
			FName BaseParentName = BaseSkeletalMesh->RefSkeleton(BaseParentIndex).Name;

			if(LODParentName != BaseParentName)
			{
				// If bone has different parents, display an error and don't allow import.
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODBoneHasIncorrectParent"), *LODBoneName.ToString(), *LODParentName.ToString(), *BaseParentName.ToString()) );
				return;
			}
		}
	}

	FStaticLODModel& NewLODModel = InSkeletalMesh->LODModels(0);

	// TODO: remove redundant code. When import LOD, some check is unnecessary. Also considerate update workflow!!

	// Enforce LODs having only single-influence vertices.
	UBOOL bCheckSingleInfluence;
	GConfig->GetBool( TEXT("AnimSetViewer"), TEXT("CheckSingleInfluenceLOD"), bCheckSingleInfluence, GEditorIni );
	if( bCheckSingleInfluence && 
		DesiredLOD > 0 )
	{
		for(INT ChunkIndex = 0;ChunkIndex < NewLODModel.Chunks.Num();ChunkIndex++)
		{
			if(NewLODModel.Chunks(ChunkIndex).SoftVertices.Num() > 0)
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("LODHasSoftVertices") );
			}
		}
	}

	// If this LOD is going to be the lowest one, we check all bones we have sockets on are present in it.
	if( DesiredLOD == BaseSkeletalMesh->LODModels.Num() || 
		DesiredLOD == BaseSkeletalMesh->LODModels.Num()-1 )
	{
		for(INT i=0; i<BaseSkeletalMesh->Sockets.Num(); i++)
		{
			// Find bone index the socket is attached to.
			USkeletalMeshSocket* Socket = BaseSkeletalMesh->Sockets(i);
			INT SocketBoneIndex = InSkeletalMesh->MatchRefBone( Socket->BoneName );

			// If this LOD does not contain the socket bone, abort import.
			if( SocketBoneIndex == INDEX_NONE )
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("LODMissingSocketBone"), *Socket->BoneName.ToString(), *Socket->SocketName.ToString()) );
				return;
			}
		}
	}

	// Fix up the ActiveBoneIndices array.
	for(INT i=0; i<NewLODModel.ActiveBoneIndices.Num(); i++)
	{
		INT LODBoneIndex = NewLODModel.ActiveBoneIndices(i);
		FName LODBoneName = InSkeletalMesh->RefSkeleton(LODBoneIndex).Name;
		INT BaseBoneIndex = BaseSkeletalMesh->MatchRefBone(LODBoneName);
		NewLODModel.ActiveBoneIndices(i) = BaseBoneIndex;
	}

	// Fix up the chunk BoneMaps.
	for(INT ChunkIndex = 0;ChunkIndex < NewLODModel.Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = NewLODModel.Chunks(ChunkIndex);
		for(INT i=0; i<Chunk.BoneMap.Num(); i++)
		{
			INT LODBoneIndex = Chunk.BoneMap(i);
			FName LODBoneName = InSkeletalMesh->RefSkeleton(LODBoneIndex).Name;
			INT BaseBoneIndex = BaseSkeletalMesh->MatchRefBone(LODBoneName);
			Chunk.BoneMap(i) = BaseBoneIndex;
		}
	}

	// Create the RequiredBones array in the LODModel from the ref skeleton.
	NewLODModel.RequiredBones.Empty();
	for(INT i=0; i<InSkeletalMesh->RefSkeleton.Num(); i++)
	{
		FName LODBoneName = InSkeletalMesh->RefSkeleton(i).Name;
		INT BaseBoneIndex = BaseSkeletalMesh->MatchRefBone(LODBoneName);
		if(BaseBoneIndex != INDEX_NONE)
		{
			NewLODModel.RequiredBones.AddItem(BaseBoneIndex);
		}
	}
	/*
	// Also sort the RequiredBones array to be strictly increasing.
	Sort<USE_COMPARE_CONSTREF(BYTE, AnimSetViewerTools)>( &NewLODModel.RequiredBones(0), NewLODModel.RequiredBones.Num() );

	// To be extra-nice, we apply the difference between the root transform of the meshes to the verts.
	FMatrix LODToBaseTransform = InSkeletalMesh->GetRefPoseMatrix(0).Inverse() * BaseSkeletalMesh->GetRefPoseMatrix(0);

	for(INT ChunkIndex = 0;ChunkIndex < NewLODModel.Chunks.Num();ChunkIndex++)
	{
		FSkelMeshChunk& Chunk = NewLODModel.Chunks(ChunkIndex);
		// Fix up rigid verts.
		for(INT i=0; i<Chunk.RigidVertices.Num(); i++)
		{
			Chunk.RigidVertices(i).Position = LODToBaseTransform.TransformFVector( Chunk.RigidVertices(i).Position );
			Chunk.RigidVertices(i).TangentX = LODToBaseTransform.TransformNormal( Chunk.RigidVertices(i).TangentX );
			Chunk.RigidVertices(i).TangentY = LODToBaseTransform.TransformNormal( Chunk.RigidVertices(i).TangentY );
			Chunk.RigidVertices(i).TangentZ = LODToBaseTransform.TransformNormal( Chunk.RigidVertices(i).TangentZ );
		}

		// Fix up soft verts.
		for(INT i=0; i<Chunk.SoftVertices.Num(); i++)
		{
			Chunk.SoftVertices(i).Position = LODToBaseTransform.TransformFVector( Chunk.SoftVertices(i).Position );
			Chunk.SoftVertices(i).TangentX = LODToBaseTransform.TransformNormal( Chunk.SoftVertices(i).TangentX );
			Chunk.SoftVertices(i).TangentY = LODToBaseTransform.TransformNormal( Chunk.SoftVertices(i).TangentY );
			Chunk.SoftVertices(i).TangentZ = LODToBaseTransform.TransformNormal( Chunk.SoftVertices(i).TangentZ );
		}
	}
	*/
	{
		// If we want to add this as a new LOD to this mesh - add to LODModels/LODInfo array.
		if(DesiredLOD == BaseSkeletalMesh->LODModels.Num())
		{
			new(BaseSkeletalMesh->LODModels)FStaticLODModel();

			// Add element to LODInfo array.
			BaseSkeletalMesh->LODInfo.AddZeroed();
			check( BaseSkeletalMesh->LODInfo.Num() == BaseSkeletalMesh->LODModels.Num() );
			BaseSkeletalMesh->LODInfo(DesiredLOD) = InSkeletalMesh->LODInfo(0);
		}

		// Set up LODMaterialMap to number of materials in new mesh.
		FSkeletalMeshLODInfo& LODInfo = BaseSkeletalMesh->LODInfo(DesiredLOD);
		LODInfo.LODMaterialMap.Empty();

		// Now set up the material mapping array.
		for(INT MatIdx = 0; MatIdx < InSkeletalMesh->Materials.Num(); MatIdx++)
		{
			// Try and find the auto-assigned material in the array.
			INT LODMatIndex = BaseSkeletalMesh->Materials.FindItemIndex( InSkeletalMesh->Materials(MatIdx) );

			// If we didn't just use the index - but make sure its within range of the Materials array.
			if(LODMatIndex == INDEX_NONE)
			{
				LODMatIndex = ::Clamp(MatIdx, 0, BaseSkeletalMesh->Materials.Num() - 1);
			}

			LODInfo.LODMaterialMap.AddItem(LODMatIndex);
		}

		BaseSkeletalMesh->PreEditChange(NULL);

		// Copy data from the index buffer as it cant be directly copied.
		FMultiSizeIndexContainerData BufferData;
		NewLODModel.MultiSizeIndexContainer.GetIndexBufferData( BufferData );

		// Assign new FStaticLODModel to desired slot in selected skeletal mesh.
		BaseSkeletalMesh->LODModels(DesiredLOD) = NewLODModel;

		// Regenerate the index buffer.
		BaseSkeletalMesh->LODModels(DesiredLOD).MultiSizeIndexContainer.RebuildIndexBuffer( BufferData );
		
		// rebuild vertex buffers and reinit RHI resources
		
		BaseSkeletalMesh->PostEditChange();		

		// ReattachContexts go out of scope here, reattaching skel components to the scene.
	}
	/*
	// Now that the mesh is back together, recalculate the bone break weighting if we previously had done them
	if (BaseSkeletalMesh->BoneBreakNames.Num() > 0)
	{
		TArray<FBoneIndexPair> BoneIndexPairs;
		for (INT BoneNameIdx=0; BoneNameIdx < BaseSkeletalMesh->BoneBreakNames.Num(); BoneNameIdx++)
		{
			const FString& BoneName = BaseSkeletalMesh->BoneBreakNames(BoneNameIdx);
			const FName BoneFName(*BoneName);
			INT BoneIndex = BaseSkeletalMesh->MatchRefBone(BoneFName);
			if (BoneIndex != INDEX_NONE)
			{
				FBoneIndexPair* NewBoneIndexPair = new(BoneIndexPairs) FBoneIndexPair;
				NewBoneIndexPair->BoneIdx[0] = BoneIndex;
				NewBoneIndexPair->BoneIdx[1] = BaseSkeletalMesh->RefSkeleton(BoneIndex).ParentIndex;
			}
		}

		if (BoneIndexPairs.Num() > 0)
		{
			//Add the collection of bones to the bone influences list
			CalculateBoneWeightInfluences(BaseSkeletalMesh, BoneIndexPairs);
		}
	}
	*/
}

//-------------------------------------------------------------------------
//
//-------------------------------------------------------------------------
/*UBOOL CFbxImporter::CreateMatineeSkeletalAnimation(ASkeletalMeshActor* Actor, UAnimSet* AnimSet)
{
	INT TakeIndex;
	for(TakeIndex = 0; TakeIndex < Takes.GetCount(); TakeIndex++)
	{
		// It's useless to parse the default animation because it is always empty.
		if(Takes.GetAt(t)->Compare(KFBXTAKENODE_DEFAULT_NAME) == 0)
		{
			continue;
		}

		FbxScene->SetCurrentTake(m_takes.GetAt(t)->Buffer());
		KTime StartTime(KTIME_INFINITE);
		KTime EndTime(KTIME_MINUS_INFINITE);
		if (FbxScene->GetRootNode()->GetAnimationInterval(StartTime,EndTime))
		{
			USequence* KismetSequence = GetKismetSequence(GWorld->PersistentLevel);

			// create a matinee action triggered by the level Start in Kismet
			USeqAct_Interp* MatineeSequence = ConstructObject<USeqAct_Interp>( USeqAct_Interp::StaticClass(), KismetSequence, ANSI_TO_TCHAR(Takes.GetAt(t)->Buffer()), RF_Standalone|RF_Transactional );
			MatineeSequence->bLooping = TRUE;
			MatineeSequence->ObjPosX = 0;
			KismetSequence->AddSequenceObject(MatineeSequence);
			MatineeSequence->OnCreated();
			USeqEvent_LevelBeginning* Begin = ConstructObject<USeqEvent_LevelBeginning>( USeqEvent_LevelBeginning::StaticClass(), kismetSeq, NAME_None, RF_Standalone|RF_Transactional );
			Begin->ObjPosX = -200;
			KismetSequence->AddSequenceObject(begin);
			Begin->OnCreated();
			LinkSeq(Begin,0,MatineeSequence,0);

			// fill the matinee data
			MatineeSequence->InterpData = Cast<UInterpData>(MatineeSequence->VariableLinks(0).LinkedVariables(0));
			if (!MatineeSequence->InterpData)
			{
				warnf(NAME_Error,TEXT("No Matinee Interpdata"));
				return false;
			}
			UInterpData* MatineeData = MatineeSequence->InterpData;
			MatineeData->InterpLength = (EndTime.GetMilliSeconds() - StartTime.GetMilliSeconds())/1000.f;

			// create a new group & group instance
			UInterpGroup* Group = ConstructObject<UInterpGroup>(UInterpGroup::StaticClass(),MatineeData,*Actor->GetName(),RF_Transactional);
			MatineeData->InterpGroups.AddItem(Group); 
			Group->GroupName = *Actor->GetName();
			Group->GroupAnimSets.AddItem(AnimSet);

			UInterpGroupInst* GroupInst = ConstructObject<UInterpGroupInst>( UInterpGroupInst::StaticClass(), MatineeSequence, NAME_None, RF_Transactional );
			MatineeSequence->GroupInst.AddItem(GroupInst);
			GroupInst->InitGroupInst(Group, Actor);
			GroupInst->SaveGroupActorState();
			MatineeSequence->UpdateConnectorsFromData();

			// create a new skeletal anim track & track instance
			UInterpTrackAnimControl* Track = ConstructObject<UInterpTrackAnimControl>(UInterpTrackAnimControl::StaticClass(),Group,TEXT("Take Data"),RF_Transactional);
			Group->InterpTracks.AddItem(Track);
			UInterpTrackInstAnimControl* TrackInst = ConstructObject<UInterpTrackInstAnimControl>( UInterpTrackInstAnimControl::StaticClass(), GroupInst, NAME_None, RF_Transactional );
			GroupInst->TrackInst.AddItem(TrackInst);
			TrackInst->InitTrackInst(Track);

			// add a key to Start anim
			Track->AnimSets.AddItem(AnimSet);
			Track->AnimSeqs.AddZeroed();
			FAnimControlTrackKey& Key = Track->AnimSeqs.Last();
			Key.StartTime = (FLOAT)StartTime.GetSecondDouble();
			Key.AnimSeqName = FName(ANSI_TO_TCHAR(Takes.GetAt(t)->Buffer()));
			Key.AnimPlayRate = 1.f;

			// Retrieve the Kismet connector for the new group.
			INT ConnectorIndex = MatineeSequence->FindConnectorIndex(Group->GroupName.ToString(), LOC_VARIABLE);
			FIntPoint ConnectorPos = MatineeSequence->GetConnectionLocation(LOC_VARIABLE, ConnectorIndex);	

			// Add a variable for the interpActor and connect
			USeqVar_Object* ActorVar = ConstructObject<USeqVar_Object>( USeqVar_Object::StaticClass(), KismetSequence, NAME_None, RF_Transactional );
			ActorVar->ObjValue = Actor;
			ActorVar->ObjPosX = MatineeSequence->ObjPosX + ConnectorIndex * LO_MIN_SHAPE_SIZE * 3 / 2; // ConnectorPos.X is not yet valid. It becomes valid only at the first render.
			ActorVar->ObjPosY = ConnectorPos.Y + LO_MIN_SHAPE_SIZE * 2;
			KismetSequence->AddSequenceObject(actorVar);
			ActorVar->OnCreated();

			// Connect this new object variable with the Matinee group data connector.
			FSeqVarLink& VariableConnector = MatineeSequence->VariableLinks(ConnectorIndex);
			VariableConnector.LinkedVariables.AddItem(ActorVar);
			ActorVar->OnConnect(MatineeSequence, ConnectorIndex);
		}
	}

	return true;
}*/

/**
 * A class encapsulating morph target processing that occurs during import on a separate thread
 */
class FAsyncImportMorphTargetWork : public FNonAbandonableTask
{
public:
	FAsyncImportMorphTargetWork( USkeletalMesh* InTempSkelMesh, INT InLODIndex, FSkeletalMeshBinaryImport& InImportData )
		: TempSkeletalMesh(InTempSkelMesh)
		, ImportData(InImportData)
		, LODIndex(InLODIndex)
	{

	}

	void DoWork()
	{
		TArray<FVector> LODPoints;
		TArray<FMeshWedge> LODWedges;
		TArray<FMeshFace> LODFaces;
		TArray<FVertInfluence> LODInfluences;
		ImportData.CopyLODImportData(LODPoints,LODWedges,LODFaces,LODInfluences);
	
		ImportData.Empty();
		TempSkeletalMesh->CreateSkinningStreams( LODInfluences, LODWedges, LODFaces, LODPoints, NULL );
	}

	static const TCHAR *Name()
	{
		return TEXT("FAsyncImportMorphTargetWork");
	}

private:
	USkeletalMesh* TempSkeletalMesh;
	FSkeletalMeshBinaryImport ImportData;
	INT LODIndex;

};

void UnFbx::CFbxImporter::ImportMorphTargetsInternal( TArray<FbxNode*>& SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, UMorphTargetSet* MorphTargetSet, const FFilename& InFilename, INT LODIndex )
{
	FbxString ShapeNodeName;
	TArray<FbxShape*> FbxShapeArray;
	FbxShapeArray.Add(SkelMeshNodeArray.Num());

	// Initialize the shape array.
	// In the shape array, only one geometry has shape at a time. Other geometries has no shape
	for (INT NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		FbxShapeArray(NodeIndex) = NULL;
	}

	// Temp arrays to keep track of data being used by threads
	TArray<USkeletalMesh*> TempMeshes;
	TArray<UMorphTarget*> MorphTargets;

	// Array of pending tasks that are not complete
	TArray<FAsyncTask<FAsyncImportMorphTargetWork> > PendingWork;

	// Reserve a reasonable amount of space in the temp arrays so they are not resized frequently 
	TempMeshes.Reserve(100);
	MorphTargets.Reserve(100);
	PendingWork.Reserve(100);

	GWarn->BeginSlowTask( TEXT("Generating Morph Models"), TRUE);

	// For each morph in FBX geometries, we create one morph target for the Unreal skeletal mesh
	for (INT NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		// reset the shape array
		if (NodeIndex > 0)
		{
			FbxShapeArray(NodeIndex-1) = NULL;
		}

		UINT GlobalShapeCount = 0;
		FbxGeometry* Geometry = (FbxGeometry*)SkelMeshNodeArray(NodeIndex)->GetNodeAttribute();
		if (Geometry)
		{
			const INT BlendShapeDeformerCount = Geometry->GetDeformerCount(FbxDeformer::eBLENDSHAPE);

			/************************************************************************/
			/* 1. count shapes to properly update progress                          */
			/************************************************************************/
			for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
			{
				const FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);

				const INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();
				for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
				{
					const FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);
					if(Channel)
					{
						//Find which shape should we use according to the weight.
						const INT CurrentChannelShapeCount = Channel->GetTargetShapeCount();
						GlobalShapeCount += CurrentChannelShapeCount;
					}
				}
			}

			/************************************************************************/
			/* 2. do the job actually                                               */
			/************************************************************************/
			UINT CurrentShapeIndex = 0;
			for(INT BlendShapeIndex = 0; BlendShapeIndex<BlendShapeDeformerCount; ++BlendShapeIndex)
			{
				FbxBlendShape* BlendShape = (FbxBlendShape*)Geometry->GetDeformer(BlendShapeIndex, FbxDeformer::eBLENDSHAPE);
				const INT BlendShapeChannelCount = BlendShape->GetBlendShapeChannelCount();

				FString BlendShapeName = ANSI_TO_TCHAR(MakeName(BlendShape->GetName()));

				for(INT ChannelIndex = 0; ChannelIndex<BlendShapeChannelCount; ++ChannelIndex)
				{
					FbxBlendShapeChannel* Channel = BlendShape->GetBlendShapeChannel(ChannelIndex);
					if(Channel)
					{
						//Find which shape should we use according to the weight.
						const INT CurrentChannelShapeCount = Channel->GetTargetShapeCount();
						
						FString ChannelName = ANSI_TO_TCHAR(MakeName(Channel->GetName()));

						// Maya adds the name of the blendshape and an underscore to the front of the channel name, so remove it
						if(ChannelName.StartsWith(BlendShapeName))
						{
							ChannelName = ChannelName.Right(ChannelName.Len() - (BlendShapeName.Len()+1));
						}

						for(INT ShapeIndex = 0; ShapeIndex<CurrentChannelShapeCount; ++ShapeIndex)
						{
							FbxShape* Shape = Channel->GetTargetShape(ShapeIndex);
			
							FbxShapeArray(NodeIndex) = Shape;

							FString ShapeName;
							if( CurrentChannelShapeCount > 1 )
							{
								ShapeName = ANSI_TO_TCHAR(MakeName(Shape->GetName() ) );
							}
							else
							{
								// Maya concatenates the number of the shape to the end of its name, so instead use the name of the channel
								ShapeName = ChannelName; 
							}

							GWarn->StatusUpdatef( CurrentShapeIndex+1, GlobalShapeCount, *FString::Printf( TEXT("Generating morph target mesh %s (%d of %d)"), *ShapeName, CurrentShapeIndex+1, GlobalShapeCount )  );

							FSkeletalMeshBinaryImport ImportData;

							// now we get a shape for whole mesh, import to unreal as a morph target
							USkeletalMesh* TmpSkeletalMesh = (USkeletalMesh*)ImportSkeletalMesh( UObject::GetTransientPackage(), SkelMeshNodeArray, NAME_None, 0, InFilename.GetBaseFilename(), &FbxShapeArray, NULL, &ImportData, FALSE );
							TempMeshes.AddItem( TmpSkeletalMesh );

							UMorphTarget* Result = NULL;
							if (LODIndex == 0)
							{
								// add it to the set
								Result = ConstructObject<UMorphTarget>( UMorphTarget::StaticClass(), MorphTargetSet, *ShapeName );

								// create the new morph target mesh
								MorphTargetSet->Targets.AddItem( Result );

							}
							else
							{
								Result = MorphTargetSet->Targets(CurrentShapeIndex);
							}

							MorphTargets.AddItem(Result);
					
							// Process the skeletal mesh on a separate thread
							FAsyncTask<FAsyncImportMorphTargetWork>* NewWork = new (PendingWork) FAsyncTask<FAsyncImportMorphTargetWork>( TmpSkeletalMesh, LODIndex, ImportData );
							NewWork->StartBackgroundTask();
							++CurrentShapeIndex;
						}
					}
				}
			}
		}
	} // for NodeIndex

	// Wait for all importing tasks to complete
	INT NumCompleted = 0;
	INT NumTasks = PendingWork.Num();
	do
	{
		// Check for completed async compression tasks.
		INT NumNowCompleted = 0;
		for ( INT TaskIndex=0; TaskIndex < PendingWork.Num(); ++TaskIndex )
		{
			if ( PendingWork(TaskIndex).IsDone() )
			{
				NumNowCompleted++;
			}
		}
		if (NumNowCompleted > NumCompleted)
		{
			NumCompleted = NumNowCompleted;
			GWarn->StatusUpdatef( NumCompleted, NumTasks, *FString::Printf( TEXT("Importing Morph Target: %d of %d"), NumCompleted, NumTasks ) );
		}
		appSleep(0.1f);
	} while ( NumCompleted < NumTasks );

	// Create morph streams for each morph target we are importing.
	// This has to happen on a single thread since the skeletal meshes' bulk data is locked and cant be accessed by multiple threads simultaneously
	for (INT Index = 0; Index < TempMeshes.Num(); Index++)
	{
		GWarn->StatusUpdatef( Index+1, TempMeshes.Num(), *FString::Printf( TEXT("Building Morph Target Render Data: %d of %d"),  Index+1, TempMeshes.Num() ) );

		UMorphTarget* MorphTarget = MorphTargets(Index);
		USkeletalMesh* TmpSkeletalMesh = TempMeshes( Index );

		FMorphMeshRawSource TargetMeshRawData( TmpSkeletalMesh );
		FMorphMeshRawSource BaseMeshRawData( BaseSkelMesh, LODIndex );

		// populate the vertex data for the morph target mesh using its base mesh
		// and the newly imported mesh
		MorphTarget->CreateMorphMeshStreams( BaseMeshRawData, TargetMeshRawData, LODIndex );
	}

	GWarn->EndSlowTask();
}	

// Import Morph target
void UnFbx::CFbxImporter::ImportFbxMorphTarget(TArray<FbxNode*> &SkelMeshNodeArray, USkeletalMesh* BaseSkelMesh, const FFilename& Filename, INT LODIndex)
{
	UBOOL bHasMorph = FALSE;
	INT NodeIndex;
	// check if there are morph in this geometry
	for (NodeIndex = 0; NodeIndex < SkelMeshNodeArray.Num(); NodeIndex++)
	{
		FbxGeometry* Geometry = (FbxGeometry*)SkelMeshNodeArray(NodeIndex)->GetNodeAttribute();
		if (Geometry)
		{
			bHasMorph = Geometry->GetDeformerCount(FbxDeformer::eBLENDSHAPE) > 0;
			if (bHasMorph)
			{
				break;
			}
		}
	}
	
	if (bHasMorph)
	{
		UMorphTargetSet* NewMorphSet = NULL;
		if ( LODIndex == 0 )
		{
			// create morph set
			FString MorphTargetName = BaseSkelMesh->GetName();
			MorphTargetName += "_MorphTargetSet";
			NewMorphSet = ConstructObject<UMorphTargetSet>( UMorphTargetSet::StaticClass(), BaseSkelMesh->GetOutermost(), *MorphTargetName, RF_Public|RF_Standalone );

			// Remember this as the base mesh that the morph target will modify
			NewMorphSet->BaseSkelMesh = BaseSkelMesh;
			SkelMeshToMorphMap.Set(BaseSkelMesh, NewMorphSet);
		}
		else
		{
			NewMorphSet = *SkelMeshToMorphMap.Find(BaseSkelMesh);
		}
		
		if (NewMorphSet)
		{
			ImportMorphTargetsInternal( SkelMeshNodeArray, BaseSkelMesh, NewMorphSet, Filename, LODIndex );
		}
	}
}

#endif // WITH_FBX
