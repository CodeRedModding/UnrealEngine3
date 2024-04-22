/*=============================================================================
	SpeedTree.cpp: SpeedTree implementation
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineMaterialClasses.h"
#include "SpeedTree.h"
#include "ScenePrivate.h"

#if WITH_SPEEDTREE

	#include "../../../External/SpeedTree/Include/Core/Allocator.h"  

	/** The UE3 implementation of the SpeedTree allocator interface, which routes all SpeedTree allocations through UE3's memory allocator. */
	class FSpeedTreeAllocator : public SpeedTree::CAllocator
	{
	public:
		virtual void* Alloc(size_t BlockSize)
		{
			return appMalloc(BlockSize);
		}
		virtual void Free(void* Base)
		{
			appFree(Base);
		}
	};

	FSpeedTreeAllocator SpeedTreeAllocator;
	SpeedTree::CAllocatorInterface SpeedTreeAllocatorInterface(&SpeedTreeAllocator);

#endif

IMPLEMENT_CLASS(ASpeedTreeActor);
IMPLEMENT_CLASS(USpeedTree);

#if WITH_EDITOR
void ASpeedTreeActor::CheckForErrors()
{
	Super::CheckForErrors();
#if WITH_SPEEDTREE
	if (SpeedTreeComponent && SpeedTreeComponent->SpeedTree && SpeedTreeComponent->SpeedTree->IsLegacySpeedTree())
	{
		GWarn->MapCheck_Add( MCTYPE_WARNING, this, *FString::Printf( LocalizeSecure( LocalizeUnrealEd( "MapCheck_Message_LegacySpeedTree" ), *GetName(), *SpeedTreeComponent->SpeedTree->GetPathName() ) ), TEXT( "LegacySpeedTree" ) );
	}
#endif
}
#endif

void USpeedTree::StaticConstructor( )
{
#if WITH_SPEEDTREE
#ifdef EPIC_SPEEDTREE_KEY
	SpeedTree::CCore::Authorize( EPIC_SPEEDTREE_KEY );
	if (!SpeedTree::CCore::IsAuthorized( ))
	{
		GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("SpeedTree is not authorized to run on this computer"));
	}
#endif

	SpeedTree::CCoordSys::SetCoordSys(SpeedTree::CCoordSys::COORD_SYS_LEFT_HANDED_Z_UP);
	SpeedTree::CCore::SetTextureFlip(TRUE);
#endif
}



void USpeedTree::BeginDestroy( )
{
	Super::BeginDestroy();
#if WITH_SPEEDTREE
	if( SRH )
	{
		SRH->CleanUp(TRUE);
	}
#endif
}

UBOOL USpeedTree::IsReadyForFinishDestroy()
{
#if WITH_SPEEDTREE
	return SRH ? SRH->ReleaseResourcesFence.GetNumPendingFences() == 0 : TRUE;
#else
	return TRUE;
#endif
}

void USpeedTree::FinishDestroy()
{
#if WITH_SPEEDTREE
	delete SRH;
	SRH = NULL;
#endif
	Super::FinishDestroy();
}

void USpeedTree::PostLoad()
{
	Super::PostLoad();
#if WITH_SPEEDTREE
	// Create helper object if not already present; will be NULL if duplicated as property is duplicatetransient
	if( !SRH && !bLegacySpeedTree )
	{
		SRH = new FSpeedTreeResourceHelper(this);
	}

	if (SRH)
	{
		SRH->InitResources();
	}
#endif
}

#if WITH_SPEEDTREE
FSpeedTreeResourceHelper::FSpeedTreeResourceHelper( USpeedTree* InOwner )
:	Owner( InOwner )
,	bHasBranches(FALSE)
,	bHasFronds(FALSE)
,	bHasLeafCards(FALSE)
,	bHasLeafMeshes(FALSE)
,	bHasBillboards(FALSE)
,	bHasHorzBillboard(FALSE)
,	bIsInitialized(FALSE)
,	SpeedTree(NULL)
,	BranchVertexFactory(InOwner)
,	FrondVertexFactory(InOwner)
,	LeafCardVertexFactory(InOwner)
,	LeafMeshVertexFactory(InOwner)
,	BillboardVertexFactory(InOwner)
,	Branch1Elements(0)
,	FrondElements(0)
,	LeafCardElements(0)
,	LeafMeshElements(0)
,	WindTimeOffset(0.0f)
,	bHasValidTexelFactors(FALSE)
{
	// to detect time difference even if we start timer at 0.0f
	LastWindTime = -0.01f;

	check(Owner);
}

void FSpeedTreeResourceHelper::CleanUp(BOOL bAll)
{
	if (bIsInitialized)
	{
		// Begin releasing resources.
		BeginReleaseResource(&BranchVertexFactory);
		BeginReleaseResource(&FrondVertexFactory);
		BeginReleaseResource(&BillboardVertexFactory);
		BeginReleaseResource(&LeafCardVertexFactory);
		BeginReleaseResource(&LeafMeshVertexFactory);

		if (bAll)
		{
			BeginReleaseResource(&IndexBuffer);
			BeginReleaseResource(&BranchPositionBuffer);
			BeginReleaseResource(&BranchDataBuffer);
			BeginReleaseResource(&FrondPositionBuffer);
			BeginReleaseResource(&FrondDataBuffer);
			BeginReleaseResource(&LeafCardPositionBuffer);
			BeginReleaseResource(&LeafCardDataBuffer);
			BeginReleaseResource(&LeafMeshPositionBuffer);
			BeginReleaseResource(&LeafMeshDataBuffer);
			BeginReleaseResource(&BillboardPositionBuffer);
			BeginReleaseResource(&BillboardDataBuffer);
			
			// Defer the SpeedTree deletion until the rendering thread is done with it.
			ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
				DeleteSpeedTree,
				SpeedTree::CCore*,SpeedTree,SpeedTree,
				{
					delete SpeedTree;
				});
		}

		// insert a fence to signal when these commands completed
		ReleaseResourcesFence.BeginFence();

		if (bAll)
		{
			// reset flags
			bHasBranches		= FALSE;
			bHasLeafCards		= FALSE;
			bHasLeafMeshes		= FALSE;
			bHasFronds			= FALSE;
			bHasBillboards		= FALSE;
			bHasHorzBillboard	= FALSE;
			bIsInitialized		= FALSE;

			// clear elements
			Branch1Elements.Empty();
			Branch2Elements.Empty();
			FrondElements.Empty();
			LeafCardElements.Empty();
			LeafMeshElements.Empty();
			BillboardElement.Elements(0).NumPrimitives = 0;

			BillboardTexcoordScaleBias.Empty();
		}

		bIsInitialized = FALSE;
	}
}

void FSpeedTreeResourceHelper::Load(const BYTE* Buffer, INT NumBytes)
{
	if (!SpeedTree::CCore::IsAuthorized( ))
	{
		GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("SpeedTree is not authorized to run on this computer"));
		return;
	}

	// clean out any data we have
	CleanUp(TRUE);

	// make a new speedtree
	SpeedTree = new SpeedTree::CCore;

	// load the tree
#if WITH_SPEEDTREE_MANGLE
	// Skip the 27 bytes of garbage at the beginning of 'mangled' .SRT files
	if( !SpeedTree->LoadTree( Buffer + 27, NumBytes - 27, FALSE ) )
#else
	if( !SpeedTree->LoadTree( Buffer, NumBytes, FALSE ) )
#endif
	{
		// tree failed to load
		GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("Could not load the tree data"));
		CleanUp(TRUE);
	}
	else
	{
		// set flags on geometry
		bHasBranches = SpeedTree->HasGeometryType(SpeedTree::GEOMETRY_TYPE_BRANCHES);
		bHasFronds = SpeedTree->HasGeometryType(SpeedTree::GEOMETRY_TYPE_FRONDS);
		bHasLeafCards = SpeedTree->HasGeometryType(SpeedTree::GEOMETRY_TYPE_LEAF_CARDS);
		bHasLeafMeshes = SpeedTree->HasGeometryType(SpeedTree::GEOMETRY_TYPE_LEAF_MESHES);
		bHasBillboards = SpeedTree->HasGeometryType(SpeedTree::GEOMETRY_TYPE_VERTICAL_BILLBOARDS);

		Bounds = FBoxSphereBounds((FVector*)((const SpeedTree::st_float32*)SpeedTree->GetExtents()), 2);

		if( bHasBranches )
		{
			SetupIndexedGeometry(STMT_Branches1);
		}
		if( bHasFronds )
		{
			SetupIndexedGeometry(STMT_Fronds);
		}
		if( bHasLeafCards )
		{
			SetupLeafCards();
		}
		if( bHasLeafMeshes )
		{
			SetupIndexedGeometry(STMT_LeafMeshes);
		}
		if ( bHasBillboards )
		{
			SetupBillboards();
		}

		SpeedTree->DeleteGeometry(true);

		// fix for beta modeler since it didn't keep the number of lods the same
		INT MaxLODs = Max(Branch1Elements.Num(), Branch2Elements.Num());
		MaxLODs = Max(MaxLODs, FrondElements.Num());
		MaxLODs = Max(MaxLODs, LeafCardElements.Num());
		MaxLODs = Max(MaxLODs, LeafMeshElements.Num());
		while (Branch1Elements.Num() < MaxLODs)
		{
			Branch1Elements.AddZeroed();
		}
		while (Branch2Elements.Num() < MaxLODs)
		{
			Branch2Elements.AddZeroed();
		}
		while (FrondElements.Num() < MaxLODs)
		{
			FrondElements.AddZeroed();
		}
		while (LeafCardElements.Num() < MaxLODs)
		{
			LeafCardElements.AddZeroed();
		}
		while (LeafMeshElements.Num() < MaxLODs)
		{
			LeafMeshElements.AddZeroed();
		}

		InitResources();
	}
}


void FSpeedTreeResourceHelper::InitResources()
{
	CleanUp(FALSE);

	// setup the branch buffers
	if( bHasBranches )
	{
		for (INT i = 0; i < Branch1Elements.Num( ); ++i)
		{
			Branch1Elements(i).VertexFactory = &BranchVertexFactory;
			Branch1Elements(i).Elements(0).IndexBuffer = &IndexBuffer;
		}
		for (INT i = 0; i < Branch2Elements.Num( ); ++i)
		{
			Branch2Elements(i).VertexFactory = &BranchVertexFactory;
			Branch2Elements(i).Elements(0).IndexBuffer = &IndexBuffer;
		}

		BeginInitResource(&BranchPositionBuffer);
		BeginInitResource(&BranchDataBuffer);
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitSpeedTreeBranchVertexFactory,
			FSpeedTreeResourceHelper*,ResourceHelper,this,
			{
				FSpeedTreeBranchVertexFactory::DataType Data;
				Data.PositionComponent			= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchPositionBuffer, FSpeedTreeVertexPosition, Position, VET_Float3);
				Data.TangentBasisComponents[0]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchDataBuffer, FSpeedTreeVertexData, TangentX, VET_PackedNormal);
				Data.TangentBasisComponents[1]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchDataBuffer, FSpeedTreeVertexData, TangentY, VET_PackedNormal);
				Data.TangentBasisComponents[2]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchDataBuffer, FSpeedTreeVertexData, TangentZ, VET_PackedNormal);
				Data.WindInfo					= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchDataBuffer, FSpeedTreeVertexData, WindInfo, VET_Float4);
				while( Data.TextureCoordinates.Num( ) < 2 )
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent( ));
				};
				Data.TextureCoordinates(0) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchDataBuffer, FSpeedTreeVertexData, TexCoord, VET_Float2);
				Data.TextureCoordinates(1) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BranchDataBuffer, FSpeedTreeVertexData, LODInfo, VET_Float4);
				ResourceHelper->BranchVertexFactory.SetData(Data);
			});
		BeginInitResource(&BranchVertexFactory);
	}

	// setup the frond buffers
	if( bHasFronds )
	{
		for (INT i = 0; i < FrondElements.Num( ); ++i)
		{
			FrondElements(i).VertexFactory = &FrondVertexFactory;
			FrondElements(i).Elements(0).IndexBuffer = &IndexBuffer;
		}

		BeginInitResource(&FrondPositionBuffer);
		BeginInitResource(&FrondDataBuffer);
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitSpeedTreeFrondVertexFactory,
			FSpeedTreeResourceHelper*,ResourceHelper,this,
			{
				FSpeedTreeFrondVertexFactory::DataType Data;
				Data.PositionComponent			= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondPositionBuffer, FSpeedTreeVertexPosition, Position, VET_Float3);
				Data.TangentBasisComponents[0]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, TangentX, VET_PackedNormal);
				Data.TangentBasisComponents[1]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, TangentY, VET_PackedNormal);
				Data.TangentBasisComponents[2]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, TangentZ, VET_PackedNormal);
				Data.WindInfo					= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, WindInfo, VET_Float4);
				while( Data.TextureCoordinates.Num( ) < 3 )
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent( ));
				};
				Data.TextureCoordinates(0) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, TexCoord, VET_Float2);
				Data.TextureCoordinates(1) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, LODInfo, VET_Float4);
				Data.TextureCoordinates(2) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->FrondDataBuffer, FSpeedTreeVertexDataFrond, FrondRipple, VET_Float2);
				ResourceHelper->FrondVertexFactory.SetData(Data);
			});
		BeginInitResource(&FrondVertexFactory);
	}

	// setup the leaf card buffers
	if( bHasLeafCards )
	{
		for (INT i = 0; i < LeafCardElements.Num( ); ++i)
		{
			LeafCardElements(i).VertexFactory = &LeafCardVertexFactory;
			LeafCardElements(i).Elements(0).IndexBuffer = &IndexBuffer;
		}

		BeginInitResource(&LeafCardPositionBuffer);
		BeginInitResource(&LeafCardDataBuffer);
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitSpeedTreeLeafCardVertexFactory,
			FSpeedTreeResourceHelper*,ResourceHelper,this,
			{
				FSpeedTreeLeafCardVertexFactory::DataType Data;
				Data.PositionComponent			= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardPositionBuffer, FSpeedTreeVertexPosition, Position, VET_Float3);
				Data.TangentBasisComponents[0]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, TangentX, VET_PackedNormal);
				Data.TangentBasisComponents[1]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, TangentY, VET_PackedNormal);
				Data.TangentBasisComponents[2]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, TangentZ, VET_PackedNormal);
				Data.WindInfo						= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, WindInfo, VET_Float4);
				while( Data.TextureCoordinates.Num( ) < 3 )
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent( ));
				}
				Data.TextureCoordinates(0) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, TexCoord, VET_Float2);
				Data.TextureCoordinates(1) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, LODInfo, VET_Float4);
				Data.TextureCoordinates(2) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafCardDataBuffer, FSpeedTreeVertexDataLeafCard, CornerOffset, VET_Float3);

				ResourceHelper->LeafCardVertexFactory.SetData(Data);
			});
		BeginInitResource(&LeafCardVertexFactory);
	}

	// setup the leaf mesh buffers
	if( bHasLeafMeshes )
	{
		for (INT i = 0; i < LeafMeshElements.Num( ); ++i)
		{
			LeafMeshElements(i).VertexFactory = &LeafMeshVertexFactory;
			LeafMeshElements(i).Elements(0).IndexBuffer = &IndexBuffer;
		}

		BeginInitResource(&LeafMeshPositionBuffer);
		BeginInitResource(&LeafMeshDataBuffer);
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitSpeedTreeLeafMeshVertexFactory,
			FSpeedTreeResourceHelper*,ResourceHelper,this,
			{
				FSpeedTreeBranchVertexFactory::DataType Data;
				Data.PositionComponent			= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshPositionBuffer, FSpeedTreeVertexPosition, Position, VET_Float3);
				Data.TangentBasisComponents[0]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshDataBuffer, FSpeedTreeVertexData, TangentX, VET_PackedNormal);
				Data.TangentBasisComponents[1]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshDataBuffer, FSpeedTreeVertexData, TangentY, VET_PackedNormal);
				Data.TangentBasisComponents[2]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshDataBuffer, FSpeedTreeVertexData, TangentZ, VET_PackedNormal);
				Data.WindInfo					= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshDataBuffer, FSpeedTreeVertexData, WindInfo, VET_Float4);
				while( Data.TextureCoordinates.Num( ) < 2 )
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent( ));
				};
				Data.TextureCoordinates(0) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshDataBuffer, FSpeedTreeVertexData, TexCoord, VET_Float2);
				Data.TextureCoordinates(1) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->LeafMeshDataBuffer, FSpeedTreeVertexData, LODInfo, VET_Float4);
				ResourceHelper->LeafMeshVertexFactory.SetData(Data);
			});
		BeginInitResource(&LeafMeshVertexFactory);
	}

	if ( bHasBillboards )
	{
		BillboardElement.VertexFactory = &BillboardVertexFactory;
		BillboardElement.Elements(0).IndexBuffer = &IndexBuffer;

		// prepare billboard position buffer
		BeginInitResource(&BillboardPositionBuffer);
		BeginInitResource(&BillboardDataBuffer);

		// prepare billboard vertex factory
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitSpeedTreeBillboardVertexFactory,
			FSpeedTreeResourceHelper*,ResourceHelper,this,
			{
				FSpeedTreeBillboardVertexFactory::DataType Data;
				Data.PositionComponent			= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BillboardPositionBuffer, FSpeedTreeVertexPosition, Position, VET_Float3);
				Data.TangentBasisComponents[0]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BillboardDataBuffer, FSpeedTreeVertexDataBillboard, TangentX, VET_PackedNormal);
				Data.TangentBasisComponents[1]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BillboardDataBuffer, FSpeedTreeVertexDataBillboard, TangentY, VET_PackedNormal);
				Data.TangentBasisComponents[2]	= STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BillboardDataBuffer, FSpeedTreeVertexDataBillboard, TangentZ, VET_PackedNormal);
				while( Data.TextureCoordinates.Num( ) < 2 )
				{
					Data.TextureCoordinates.AddItem(FVertexStreamComponent( ));
				}
				Data.TextureCoordinates(0) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BillboardDataBuffer, FSpeedTreeVertexDataBillboard, TexCoord, VET_Float2);
				Data.TextureCoordinates(1) = STRUCTMEMBER_VERTEXSTREAMCOMPONENT(&ResourceHelper->BillboardDataBuffer, FSpeedTreeVertexDataBillboard, bIsVerticalBillboard, VET_Float1);

				ResourceHelper->BillboardVertexFactory.SetData(Data);
			});
		BeginInitResource(&BillboardVertexFactory);
	}

	// prepare index buffer
	BeginInitResource(&IndexBuffer);

	// everything appeared to go well
	bIsInitialized = TRUE;
}

void FSpeedTreeResourceHelper::SetupIndexedGeometry(ESpeedTreeMeshType eType)
{
	// get geometry
	const SpeedTree::SGeometry* SpeedTreeGeometry = SpeedTree->GetGeometry();
	
	// prepare LODs
	INT NumLODs = 0;
	SpeedTree::SIndexedTriangles* IndexedTriangleLODs = NULL;
	TSpeedTreeVertexBuffer<FSpeedTreeVertexPosition>* PositionBuffer = NULL;
	switch (eType)
	{
	case STMT_Branches1:
		// do both branches at the same time (ie branches and caps)
		NumLODs = SpeedTreeGeometry->m_nNumBranchLods;
		IndexedTriangleLODs = SpeedTreeGeometry->m_pBranchLods;
		PositionBuffer = &BranchPositionBuffer;
		Branch1Elements.Empty(NumLODs);
		Branch1Elements.AddZeroed(NumLODs);
		Branch2Elements.Empty(NumLODs);
		Branch2Elements.AddZeroed(NumLODs);
		break;
	case STMT_Fronds:
		NumLODs = SpeedTreeGeometry->m_nNumFrondLods;
		IndexedTriangleLODs = SpeedTreeGeometry->m_pFrondLods;
		PositionBuffer = &FrondPositionBuffer;
		FrondElements.Empty(NumLODs);
		FrondElements.AddZeroed(NumLODs);
		break;
	case STMT_LeafMeshes:
		NumLODs = SpeedTreeGeometry->m_nNumLeafMeshLods;
		IndexedTriangleLODs = SpeedTreeGeometry->m_pLeafMeshLods;
		PositionBuffer = &LeafMeshPositionBuffer;
		LeafMeshElements.Empty(NumLODs);
		LeafMeshElements.AddZeroed(NumLODs);
		break;
	default:
		return;
		break;
	};

	if( NumLODs == 0 )
	{
		return;
	}

	#if (SPEEDTREE_VERSION_MINOR > 0)
		#define SPEEDTREE_WIND_INFO_SKIP 6
	#else
		#define SPEEDTREE_WIND_INFO_SKIP 5
	#endif

	// fill vertex/index arrays for each lod
	UBOOL bWarnedAboutMaterials = FALSE;
	for (INT LodIndex = 0; LodIndex < NumLODs; ++LodIndex)
	{
		SpeedTree::SIndexedTriangles& IndexedGeometry = IndexedTriangleLODs[LodIndex];
		if (!IndexedGeometry.HasGeometry( ))
		{
			continue;
		}
		
		// make vertices
		INT StartVertex = PositionBuffer->Vertices.Num();
		for (INT i = 0; i < IndexedGeometry.m_nNumVertices; ++i)
		{
			const INT Index1 = i * 3;
			const INT Index2 = Index1 + 1;
			const INT Index3 = Index2 + 1;

			// make a new vertex for the position
			FSpeedTreeVertexPosition* NewPosVertex = new(PositionBuffer->Vertices) FSpeedTreeVertexPosition;
			NewPosVertex->Position = FVector(IndexedGeometry.m_pCoords[Index1], IndexedGeometry.m_pCoords[Index2], IndexedGeometry.m_pCoords[Index3]);

			// make a new vertex for the data
			FSpeedTreeVertexData* NewDataVertex = NULL;
			if (eType == STMT_Fronds)
			{
				// fronds have extra ripple parameter
				FSpeedTreeVertexDataFrond* NewDataVertexFrond = new(FrondDataBuffer.Vertices) FSpeedTreeVertexDataFrond;
				NewDataVertexFrond->FrondRipple = FVector2D(IndexedGeometry.m_pFrondRipple[i * 2], IndexedGeometry.m_pFrondRipple[i * 2 + 1]);
				NewDataVertex = NewDataVertexFrond;
			}
			else
			{
				if (eType == STMT_Branches1)
				{
					NewDataVertex = new(BranchDataBuffer.Vertices) FSpeedTreeVertexData;
				}
				else
				{
					NewDataVertex = new(LeafMeshDataBuffer.Vertices) FSpeedTreeVertexData;
				}
			}

			// set vertex normal, binormal, and tangent
			NewDataVertex->TangentZ.Vector.X = IndexedGeometry.m_pNormals[Index1];
			NewDataVertex->TangentZ.Vector.Y = IndexedGeometry.m_pNormals[Index2];
			NewDataVertex->TangentZ.Vector.Z = IndexedGeometry.m_pNormals[Index3];
			NewDataVertex->TangentY.Vector.X = IndexedGeometry.m_pBinormals[Index1];
			NewDataVertex->TangentY.Vector.Y = IndexedGeometry.m_pBinormals[Index2];
			NewDataVertex->TangentY.Vector.Z = IndexedGeometry.m_pBinormals[Index3];
			NewDataVertex->TangentX.Vector.X = IndexedGeometry.m_pTangents[Index1];
			NewDataVertex->TangentX.Vector.Y = IndexedGeometry.m_pTangents[Index2];
			NewDataVertex->TangentX.Vector.Z = IndexedGeometry.m_pTangents[Index3];

			// set diffuse texcoords
			NewDataVertex->TexCoord = FVector2D(IndexedGeometry.m_pTexCoordsDiffuse[i * 2], IndexedGeometry.m_pTexCoordsDiffuse[i * 2 + 1]);

			// uncompress wind info
			NewDataVertex->WindInfo[0] = SpeedTree::CCore::UncompressScalar(IndexedGeometry.m_pWindData[i * SPEEDTREE_WIND_INFO_SKIP]) * IndexedGeometry.m_fWindDataMagnitude;
			NewDataVertex->WindInfo[1] = SpeedTree::CCore::UncompressScalar(IndexedGeometry.m_pWindData[i * SPEEDTREE_WIND_INFO_SKIP + 1]) * IndexedGeometry.m_fWindDataMagnitude;
			NewDataVertex->WindInfo[2] = SpeedTree::CCore::UncompressScalar(IndexedGeometry.m_pWindData[i * SPEEDTREE_WIND_INFO_SKIP + 2]) * IndexedGeometry.m_fWindDataMagnitude;
			NewDataVertex->WindInfo[3] = SpeedTree::CCore::UncompressScalar(IndexedGeometry.m_pWindData[i * SPEEDTREE_WIND_INFO_SKIP + 3]) * PI;
			NewDataVertex->LODInfo[3] = SpeedTree::CCore::UncompressScalar(IndexedGeometry.m_pWindData[i * SPEEDTREE_WIND_INFO_SKIP + 4]) * 10.0f;
			// Wind active flag (m_pWindData[i * 6 + 5], new in v5.1) is ignored for backwards compatibility. It's not used very often anyway

			// LOD position
			NewDataVertex->LODInfo[0] = IndexedGeometry.m_pLodCoords[Index1];
			NewDataVertex->LODInfo[1] = IndexedGeometry.m_pLodCoords[Index2];
			NewDataVertex->LODInfo[2] = IndexedGeometry.m_pLodCoords[Index3];
		}

		// warn we're merging geometry
		if (!bWarnedAboutMaterials)
		{
			if ((IndexedGeometry.m_nNumMaterialGroups > 2) || (eType != STMT_Branches1 && IndexedGeometry.m_nNumMaterialGroups > 1))
			{
				switch (eType)
				{
				case STMT_Branches1:
					GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("Extra branch geometry merged together. For optimal drawing, all branch geometry should use only 2 color sets."));
					break;
				case STMT_Fronds:
					GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("Extra frond geometry merged together. For optimal drawing, all frond geometry should use the same color set."));
					break;
				case STMT_LeafMeshes:
					GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("Extra leaf mesh geometry merged together. For optimal drawing, all leaf mesh geometry should use the same color set."));
					break;
				default:
					break;
				}
			}
			bWarnedAboutMaterials = TRUE;
		}

		// set up for branches 2 to get all geometry with the second material group's material index (most of the time this will be caps)
		// branches1 will get everything else
		const UINT Branches2MaterialIndex = (IndexedTriangleLODs[0].m_nNumMaterialGroups > 1) ? 
			IndexedTriangleLODs[0].m_pDrawCallInfo[1].m_nMaterialIndex :
			(UINT)-1;

		for (INT MaterialGroup = 0; MaterialGroup < IndexedGeometry.m_nNumMaterialGroups; ++MaterialGroup)
		{
			const UINT IndicesOffset = IndexedGeometry.m_pDrawCallInfo[MaterialGroup].m_nOffset;
			const UINT NumIndices = IndexedGeometry.m_pDrawCallInfo[MaterialGroup].m_nLength;
			const UINT MaterialIndex = IndexedGeometry.m_pDrawCallInfo[MaterialGroup].m_nMaterialIndex;

			ESpeedTreeMeshType eThisType = eType;
			if (eThisType == STMT_Branches1 && MaterialIndex == Branches2MaterialIndex)
			{
				eThisType = STMT_Branches2;
			}

			// Set up mesh element
			TArray<FMeshBatch>* Elements = NULL;
			switch (eThisType)
			{
			case STMT_Branches1:
				Elements = &Branch1Elements;
				break;
			case STMT_Branches2:
				Elements = &Branch2Elements;
				break;
			case STMT_Fronds:
				Elements = &FrondElements;
				break;
			case STMT_LeafMeshes:
				Elements = &LeafMeshElements;
				break;
			default:
				break;
			}

			(*Elements)(LodIndex).Type				= PT_TriangleList;
			(*Elements)(LodIndex).Elements(0).FirstIndex		= IndexBuffer.Indices.Num();
			(*Elements)(LodIndex).Elements(0).NumPrimitives		= NumIndices / 3;
			(*Elements)(LodIndex).Elements(0).MinVertexIndex	= UINT(-1);
			(*Elements)(LodIndex).Elements(0).MaxVertexIndex	= 0;

			// Add the triangle's indices to the index buffer.
			for (UINT i = 0; i < NumIndices; ++i)
			{
				const UINT VertexIndex = (IndexedGeometry.m_pTriangleIndices32 ? 
					IndexedGeometry.m_pTriangleIndices32[i + IndicesOffset] :
					IndexedGeometry.m_pTriangleIndices16[i + IndicesOffset]) + StartVertex;
				IndexBuffer.Indices.AddItem(VertexIndex);

				// Update the mesh element's min and max vertex index.
				(*Elements)(LodIndex).Elements(0).MinVertexIndex = Min<UINT>((*Elements)(LodIndex).Elements(0).MinVertexIndex, VertexIndex);
				(*Elements)(LodIndex).Elements(0).MaxVertexIndex = Max<UINT>((*Elements)(LodIndex).Elements(0).MaxVertexIndex, VertexIndex);
			}
		}
	}
}

void FSpeedTreeResourceHelper::SetupLeafCards()
{
	// get geometry
	const SpeedTree::SGeometry* SpeedTreeGeometry = SpeedTree->GetGeometry();
	UBOOL bWarnedAboutMaterials = FALSE;
	
	// prepare LODs
	const INT NumLODs = SpeedTreeGeometry->m_nNumLeafCardLods;
	LeafCardElements.Empty(NumLODs);
	LeafCardElements.AddZeroed(NumLODs);

	const FLOAT CornerOffsets[4][2] = { { 0.5f, 0.5f }, { -0.5f, 0.5f }, { -0.5f, -0.5f }, { 0.5f, -0.5f }  };

	for (INT LodIndex = 0; LodIndex < NumLODs; ++LodIndex)
	{
		SpeedTree::SLeafCards& LeafCardGeometry = SpeedTreeGeometry->m_pLeafCardLods[LodIndex];
		if (!LeafCardGeometry.HasGeometry( ))
		{
			continue;
		}

		INT StartVertex = LeafCardPositionBuffer.Vertices.Num();
		
		// make vertices
		for (INT Leaf = 0; Leaf < LeafCardGeometry.m_nTotalNumCards; ++Leaf)
		{
			for (INT Corner = 0; Corner < 4; ++Corner)
			{
				// make a new vertex for the position
				FSpeedTreeVertexPosition* NewPosVertex = new(LeafCardPositionBuffer.Vertices) FSpeedTreeVertexPosition;
				NewPosVertex->Position = FVector(LeafCardGeometry.m_pPositions[Leaf * 3 + 0], LeafCardGeometry.m_pPositions[Leaf * 3 + 1], LeafCardGeometry.m_pPositions[Leaf * 3 + 2]);

				// make a new vertex for the data
				FSpeedTreeVertexDataLeafCard* NewDataVertex = new(LeafCardDataBuffer.Vertices) FSpeedTreeVertexDataLeafCard;

				// set vertex normal, binormal, and tangent
				INT Index1 = Leaf * 12 + Corner * 3;
				INT Index2 = Index1 + 1;
				INT Index3 = Index2 + 1;
				NewDataVertex->TangentZ.Vector.X = LeafCardGeometry.m_pNormals[Index1];
				NewDataVertex->TangentZ.Vector.Y = LeafCardGeometry.m_pNormals[Index2];
				NewDataVertex->TangentZ.Vector.Z = LeafCardGeometry.m_pNormals[Index3];
				NewDataVertex->TangentY.Vector.X = LeafCardGeometry.m_pBinormals[Index1];
				NewDataVertex->TangentY.Vector.Y = LeafCardGeometry.m_pBinormals[Index2];
				NewDataVertex->TangentY.Vector.Z = LeafCardGeometry.m_pBinormals[Index3];
				NewDataVertex->TangentX.Vector.X = LeafCardGeometry.m_pTangents[Index1];
				NewDataVertex->TangentX.Vector.Y = LeafCardGeometry.m_pTangents[Index2];
				NewDataVertex->TangentX.Vector.Z = LeafCardGeometry.m_pTangents[Index3];

				// set diffuse texcoords
				NewDataVertex->TexCoord = FVector2D(LeafCardGeometry.m_pTexCoordsDiffuse[Leaf * 8 + Corner * 2 + 0], LeafCardGeometry.m_pTexCoordsDiffuse[Leaf * 8 + Corner * 2 + 1]);

				// uncompress wind info
				NewDataVertex->WindInfo[0] = SpeedTree::CCore::UncompressScalar(LeafCardGeometry.m_pWindData[Leaf * 5]) * LeafCardGeometry.m_fWindDataMagnitude;
				NewDataVertex->WindInfo[1] = SpeedTree::CCore::UncompressScalar(LeafCardGeometry.m_pWindData[Leaf * 5 + 1]) * LeafCardGeometry.m_fWindDataMagnitude;
				NewDataVertex->WindInfo[2] = SpeedTree::CCore::UncompressScalar(LeafCardGeometry.m_pWindData[Leaf * 5 + 2]) * LeafCardGeometry.m_fWindDataMagnitude;
				NewDataVertex->WindInfo[3] = SpeedTree::CCore::UncompressScalar(LeafCardGeometry.m_pWindData[Leaf * 5 + 3]) * PI;
				NewDataVertex->LODInfo[3] = SpeedTree::CCore::UncompressScalar(LeafCardGeometry.m_pWindData[Leaf * 5 + 4]) * 10.0f;

				// LOD position
				NewDataVertex->LODInfo[0] = LeafCardGeometry.m_pLodScales[Leaf * 2 + 0];
				NewDataVertex->LODInfo[1] = LeafCardGeometry.m_pLodScales[Leaf * 2 + 1];
				NewDataVertex->LODInfo[2] = 0.0f;

				// offsets
				NewDataVertex->CornerOffset = FVector(
					CornerOffsets[Corner][0] * LeafCardGeometry.m_pDimensions[Leaf * 2 + 0] - LeafCardGeometry.m_pPivotPoints[Leaf * 2 + 0],
					CornerOffsets[Corner][1] * LeafCardGeometry.m_pDimensions[Leaf * 2 + 1] - LeafCardGeometry.m_pPivotPoints[Leaf * 2 + 1],
					LeafCardGeometry.m_pLeafCardOffsets[Leaf * 4 + Corner]);
			}
		}

		if (!bWarnedAboutMaterials && LeafCardGeometry.m_nNumMaterialGroups > 1)
		{
			GWarn->Logf(TEXT("SpeedTree Error: %s"), TEXT("Extra leaf card geometry merged together. For optimal drawing, all leaf card geometry should use the same color set."));
			bWarnedAboutMaterials = TRUE;
		}

		// Add the triangle's indices to the index buffer and make the mesh element
		LeafCardElements(LodIndex).Type				= PT_TriangleList;
		LeafCardElements(LodIndex).Elements(0).FirstIndex		= IndexBuffer.Indices.Num();
		LeafCardElements(LodIndex).Elements(0).NumPrimitives	= LeafCardGeometry.m_nTotalNumCards * 2;
		LeafCardElements(LodIndex).Elements(0).MinVertexIndex	= StartVertex;
		LeafCardElements(LodIndex).Elements(0).MaxVertexIndex	= StartVertex + LeafCardGeometry.m_nTotalNumCards * 4 - 1;
		for (INT i = 0; i < LeafCardGeometry.m_nTotalNumCards; ++i)
		{
			INT FirstLeafIndex = StartVertex + i * 4;
			IndexBuffer.Indices.AddItem(FirstLeafIndex + 0);
			IndexBuffer.Indices.AddItem(FirstLeafIndex + 1);
			IndexBuffer.Indices.AddItem(FirstLeafIndex + 2);
			IndexBuffer.Indices.AddItem(FirstLeafIndex + 0);
			IndexBuffer.Indices.AddItem(FirstLeafIndex + 2);
			IndexBuffer.Indices.AddItem(FirstLeafIndex + 3);
		}
	}
}

void FSpeedTreeResourceHelper::SetupBillboards( )
{
	// get geometry
	const SpeedTree::SGeometry* SpeedTreeGeometry = SpeedTree->GetGeometry();

	// add vertices to the buffers
	BillboardPositionBuffer.Vertices.Empty(8);
	BillboardPositionBuffer.Vertices.AddZeroed(8);
	BillboardDataBuffer.Vertices.Empty(8);
	BillboardDataBuffer.Vertices.AddZeroed(8);

	// vert billboard positions
	FLOAT Width = SpeedTreeGeometry->m_sVertBBs.m_fWidth * 0.5f;
	BillboardPositionBuffer.Vertices(0).Position.Set(0.0f,  Width, SpeedTreeGeometry->m_sVertBBs.m_fTopCoord);
	BillboardPositionBuffer.Vertices(1).Position.Set(0.0f, -Width, SpeedTreeGeometry->m_sVertBBs.m_fTopCoord);
	BillboardPositionBuffer.Vertices(2).Position.Set(0.0f, -Width, SpeedTreeGeometry->m_sVertBBs.m_fBottomCoord);
	BillboardPositionBuffer.Vertices(3).Position.Set(0.0f,  Width, SpeedTreeGeometry->m_sVertBBs.m_fBottomCoord);
	if( SpeedTreeGeometry->m_sHorzBB.m_bPresent )
	{
		const SpeedTree::Vec3* Positions = SpeedTreeGeometry->m_sHorzBB.m_avCoords;
		BillboardPositionBuffer.Vertices(7).Position = FVector(Positions[0].x, Positions[0].y, Positions[0].z);
		BillboardPositionBuffer.Vertices(6).Position = FVector(Positions[1].x, Positions[1].y, Positions[1].z);
		BillboardPositionBuffer.Vertices(5).Position = FVector(Positions[2].x, Positions[2].y, Positions[2].z);
		BillboardPositionBuffer.Vertices(4).Position = FVector(Positions[3].x, Positions[3].y, Positions[3].z);
	}

	const FVector2D BillboardUnitTexCoords[4] = 
	{
		FVector2D(1.0f, 1.0f),  
		FVector2D(0.0f, 1.0f), 
		FVector2D(0.0f, 0.0f), 
		FVector2D(1.0f, 0.0f),
	};

	for(UINT VertexIndex = 0;VertexIndex < 4;VertexIndex++)
	{
		FSpeedTreeVertexDataBillboard* DataVertex = NULL;

		// vert billboard
		DataVertex = &BillboardDataBuffer.Vertices(VertexIndex);
		DataVertex->TangentX.Set(FVector(0.0f, 1.0f, 0.0f));
		DataVertex->TangentY.Set(FVector(-1.0f, 0.0f, 0.0f));
		DataVertex->TangentZ.Set(FVector(0.0f, 0.0f, 1.0f));
		DataVertex->TexCoord = BillboardUnitTexCoords[VertexIndex];
		DataVertex->bIsVerticalBillboard = 1;

		// horiz billboard
		if( SpeedTreeGeometry->m_sHorzBB.m_bPresent )
		{
			bHasHorzBillboard = TRUE;
			DataVertex = &BillboardDataBuffer.Vertices(VertexIndex + 4);
			DataVertex->TangentX.Set(FVector(-1.0f, 0.0f, 0.0f));
			DataVertex->TangentY.Set(FVector(0.0f, 1.0f, 0.0f));
			DataVertex->TangentZ.Set(FVector(0.0f, 0.0f, -1.0f));
			DataVertex->TexCoord.X = SpeedTreeGeometry->m_sHorzBB.m_afTexCoords[VertexIndex * 2 + 0]; // these texcoords don't change
			DataVertex->TexCoord.Y = SpeedTreeGeometry->m_sHorzBB.m_afTexCoords[VertexIndex * 2 + 1];
			DataVertex->bIsVerticalBillboard = 0;
		}
	}

	UINT NumBillboardTriangles = 2;
	INT StartIndex = IndexBuffer.Indices.Num();
	IndexBuffer.Indices.AddItem(0);
	IndexBuffer.Indices.AddItem(1);
	IndexBuffer.Indices.AddItem(2);
	IndexBuffer.Indices.AddItem(0);
	IndexBuffer.Indices.AddItem(2);
	IndexBuffer.Indices.AddItem(3);

	if( SpeedTreeGeometry->m_sHorzBB.m_bPresent )
	{
		NumBillboardTriangles += 2;
		IndexBuffer.Indices.AddItem(4);
		IndexBuffer.Indices.AddItem(6);
		IndexBuffer.Indices.AddItem(5);
		IndexBuffer.Indices.AddItem(4);
		IndexBuffer.Indices.AddItem(7);
		IndexBuffer.Indices.AddItem(6);
	}

	// setup Element
	BillboardElement.Elements(0).FirstIndex		= StartIndex;
	BillboardElement.Elements(0).NumPrimitives	= NumBillboardTriangles;
	BillboardElement.Elements(0).MinVertexIndex = 0;
	BillboardElement.Elements(0).MaxVertexIndex = 7;
	BillboardElement.Type			= PT_TriangleList;

	// setup vertical billboard texcoord lookups for the shader
	BillboardTexcoordScaleBias.AddZeroed(SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards);
	for(INT BillboardIndex = 0;BillboardIndex < SpeedTreeGeometry->m_sVertBBs.m_nNumBillboards;BillboardIndex++)
	{
		const FLOAT MaxU = SpeedTreeGeometry->m_sVertBBs.m_pTexCoords[BillboardIndex * 4 + 0];
		const FLOAT MaxV = SpeedTreeGeometry->m_sVertBBs.m_pTexCoords[BillboardIndex * 4 + 1];
		const FLOAT Width = SpeedTreeGeometry->m_sVertBBs.m_pTexCoords[BillboardIndex * 4 + 2];
		const FLOAT Height = SpeedTreeGeometry->m_sVertBBs.m_pTexCoords[BillboardIndex * 4 + 3];
		BillboardTexcoordScaleBias(BillboardIndex).Set(Width, Height, MaxU - Width, MaxV - Height);
	}
}

void FSpeedTreeResourceHelper::GetVertBillboardTexcoordBiasOffset(const FLOAT Angle, FVector4& BiasOffset) const
{
	const INT Billboards = BillboardTexcoordScaleBias.Num();
	if (!Billboards)
	{
		return;
	}
	
	INT TexcoordChoice = appRound(0.5f * (Billboards * Angle / PI - 1.0f));
	if (TexcoordChoice < 0)
	{
		TexcoordChoice += Billboards;
	}
	if (TexcoordChoice > Billboards - 1)
	{
		TexcoordChoice -= Billboards;
	}
	BiasOffset = BillboardTexcoordScaleBias(TexcoordChoice);
}

void FSpeedTreeResourceHelper::UpdateWind(const FVector& WindDirection, FLOAT WindStrength, FLOAT CurrentTime)
{
	if( !bIsInitialized || !SpeedTree)
	{
		return;
	}

	if(LastWindTime != CurrentTime)
	{
		// Compute an offset to disallow negative time differences.
		const FLOAT PreviousWindTime = LastWindTime + WindTimeOffset;
		const FLOAT NewWindTime = Clamp(CurrentTime + WindTimeOffset,PreviousWindTime,PreviousWindTime + 1.0f);
		WindTimeOffset = NewWindTime - CurrentTime;

		LastWindTime = CurrentTime;

		SpeedTree::CWind& cWind = SpeedTree->GetWind( );
		cWind.SetDirection(&WindDirection.X);
		cWind.SetStrength(WindStrength);
		cWind.Advance(TRUE, CurrentTime + WindTimeOffset);
	}
}

INT FSpeedTreeResourceHelper::GetNumCollisionPrimitives( )
{
	if( SpeedTree && bIsInitialized )
	{
		INT NumCollisionObjects;
		SpeedTree->GetCollisionObjects(NumCollisionObjects);
		return NumCollisionObjects;
	}
	else
	{
		return 0;
	}
}

const SpeedTree::SCollisionObject* FSpeedTreeResourceHelper::GetCollisionPrimitive( INT Index )
{
	if( SpeedTree && bIsInitialized )
	{
		INT NumCollisionObjects;
		const SpeedTree::SCollisionObject* CollisionObjects = SpeedTree->GetCollisionObjects(NumCollisionObjects);
		if (Index < NumCollisionObjects)
		{
			return &(CollisionObjects[Index]);
		}
	}
	
	return NULL;
}

#endif // WITH_SPEEDTREE

UBOOL USpeedTree::IsInitialized() const
{ 
#if WITH_SPEEDTREE
	return SRH ? SRH->bIsInitialized : FALSE; 
#else
	return FALSE;
#endif
}

/** Subset of a FMeshBatch that needs to be saved for speedtrees. */
struct FSpeedTreeSavedMeshElement
{
	UINT FirstIndex;
	UINT NumPrimitives;
	UINT MinVertexIndex;
	UINT MaxVertexIndex;
	EPrimitiveType Type;

	FSpeedTreeSavedMeshElement() {}

	FSpeedTreeSavedMeshElement(const FMeshBatch& Element)
	{
		const FMeshBatchElement& BatchElement = Element.Elements(0);
		FirstIndex = BatchElement.FirstIndex;
		NumPrimitives = BatchElement.NumPrimitives;
		MinVertexIndex = BatchElement.MinVertexIndex;
		MaxVertexIndex = BatchElement.MaxVertexIndex;
		Type = (EPrimitiveType)Element.Type;
	}

	void InitializeElement(FMeshBatch& OutElement) const
	{
		FMeshBatchElement& BatchElement = OutElement.Elements(0);
		BatchElement.FirstIndex = FirstIndex;
		BatchElement.NumPrimitives = NumPrimitives;
		BatchElement.MinVertexIndex = MinVertexIndex;
		BatchElement.MaxVertexIndex = MaxVertexIndex;
		OutElement.Type = Type;
	}

	friend FArchive& operator<<(FArchive& Ar,FSpeedTreeSavedMeshElement& Element)
	{
		Ar << Element.FirstIndex;
		Ar << Element.NumPrimitives;
		Ar << Element.MinVertexIndex;
		Ar << Element.MaxVertexIndex;
		DWORD ElementType = (DWORD)Element.Type;
		Ar << ElementType;
		Element.Type = (EPrimitiveType)ElementType;
		return Ar;
	}
};

/** Serializes an array of FMeshBatch's. */
void SerializeSpeedtreeElements(FArchive& Ar, TArray<FMeshBatch>& RenderElements)
{
	TArray<FSpeedTreeSavedMeshElement> SavedElements;
	if (Ar.IsSaving())
	{
		SavedElements.Empty(RenderElements.Num());
		for (INT i = 0; i < RenderElements.Num(); i++)
		{
			SavedElements.AddItem(FSpeedTreeSavedMeshElement(RenderElements(i)));
		}
	}
	Ar << SavedElements;
	if (Ar.IsLoading())
	{
		RenderElements.Empty(SavedElements.Num());
		for (INT i = 0; i < SavedElements.Num(); i++)
		{
			FMeshBatch Mesh;
			SavedElements(i).InitializeElement(Mesh);
			RenderElements.AddItem(Mesh);
		}
	}
}

void USpeedTree::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

#if WITH_SPEEDTREE
	if (Ar.Ver() >= VER_SPEEDTREE_5_INTEGRATION && !bLegacySpeedTree)
	{
		if(Ar.IsLoading() && !SRH)
		{
			SRH = new FSpeedTreeResourceHelper(this);
		}

		if(Ar.IsLoading() && !SRH->SpeedTree)
		{
			SRH->SpeedTree = new SpeedTree::CCore;
		}

		if (SRH)
		{
			SpeedTree::CCore::SSupportingData Data;
			if (Ar.IsSaving( ))
			{
				SRH->SpeedTree->PopulateSupportingDataBlock(Data);
			}

			INT NumBytes = 0;
			// Track where we wrote NumBytes
			const INT NumBytesLocation = Ar.Tell();
			Ar << NumBytes;

			// serialize only the parts of the supporting data that we need (collision and wind)
			Ar << Data.m_nNumCollisionObjects;
			for (INT i = 0; i < Data.m_nNumCollisionObjects; ++i)
			{
				Ar << Data.m_asCollisionObjects[i].m_fRadius;
				Ar << Data.m_asCollisionObjects[i].m_vCenter1.x;
				Ar << Data.m_asCollisionObjects[i].m_vCenter1.y;
				Ar << Data.m_asCollisionObjects[i].m_vCenter1.z;
				Ar << Data.m_asCollisionObjects[i].m_vCenter2.x;
				Ar << Data.m_asCollisionObjects[i].m_vCenter2.y;
				Ar << Data.m_asCollisionObjects[i].m_vCenter2.z;
			}
			Ar << Data.m_sWindParams.m_fStrengthResponse;
			Ar << Data.m_sWindParams.m_fDirectionResponse;
			Ar << Data.m_sWindParams.m_fWindHeight;
			Ar << Data.m_sWindParams.m_fWindHeightExponent;
			Ar << Data.m_sWindParams.m_fWindHeightOffset;
			Ar << Data.m_sWindParams.m_fGustFrequency;
			Ar << Data.m_sWindParams.m_fGustPrimaryDistance;
			Ar << Data.m_sWindParams.m_fGustScale;
			Ar << Data.m_sWindParams.m_fGustStrengthMin;
			Ar << Data.m_sWindParams.m_fGustStrengthMax;
			Ar << Data.m_sWindParams.m_fGustDurationMin;
			Ar << Data.m_sWindParams.m_fGustDurationMax;
			Ar << Data.m_sWindParams.m_fGustDirectionAdjustment;
			Ar << Data.m_sWindParams.m_fGustUnison;
			Ar << Data.m_sWindParams.m_fFrondUTile;
			Ar << Data.m_sWindParams.m_fFrondVTile;
			Ar << Data.m_sWindParams.m_fLeavesLightingChange;
			Ar << Data.m_sWindParams.m_fLeavesWindwardScalar;
			for (INT i = 0; i < SpeedTree::CWind::NUM_COMPONENTS; ++i)
			{
				Ar << Data.m_sWindParams.m_afExponents[i];
				for (INT j = 0; j < SpeedTree::CWind::NUM_OSCILLATION_PARAMS; ++j)
				{
					Ar << Data.m_sWindParams.m_afOscillationValues[i][j];
				}
			}

			if (Ar.IsLoading( ))
			{
				SRH->CleanUp(FALSE);
				SRH->SpeedTree->ApplySupportingDataBlock(Data);
			}

			Ar << SRH->bHasBranches;
			Ar << SRH->bHasFronds;
			Ar << SRH->bHasLeafCards;
			Ar << SRH->bHasLeafMeshes;
			Ar << SRH->bHasBillboards;
			Ar << SRH->bHasHorzBillboard;
			Ar << SRH->bIsInitialized;

			Ar << SRH->Bounds;

			SRH->BranchPositionBuffer.Serialize(Ar);
			SRH->FrondPositionBuffer.Serialize(Ar);
			SRH->LeafCardPositionBuffer.Serialize(Ar);
			SRH->LeafMeshPositionBuffer.Serialize(Ar);
			SRH->BillboardPositionBuffer.Serialize(Ar);

			SRH->BranchDataBuffer.Serialize(Ar);
			SRH->FrondDataBuffer.Serialize(Ar);
			SRH->LeafCardDataBuffer.Serialize(Ar);
			SRH->LeafMeshDataBuffer.Serialize(Ar);
			SRH->BillboardDataBuffer.Serialize(Ar);

			SRH->IndexBuffer.Indices.BulkSerialize(Ar);

			SerializeSpeedtreeElements(Ar, SRH->Branch1Elements);
			SerializeSpeedtreeElements(Ar, SRH->Branch2Elements);
			SerializeSpeedtreeElements(Ar, SRH->FrondElements);
			SerializeSpeedtreeElements(Ar, SRH->LeafCardElements);
			SerializeSpeedtreeElements(Ar, SRH->LeafMeshElements);

			FSpeedTreeSavedMeshElement BillboardSavedElement(SRH->BillboardElement);
			Ar << BillboardSavedElement;
			if (Ar.IsLoading())
			{
				BillboardSavedElement.InitializeElement(SRH->BillboardElement);
			}
		
			SRH->BillboardTexcoordScaleBias.BulkSerialize(Ar);

			if (Ar.IsSaving())
			{
				const INT CurrentArchiveLocation = Ar.Tell();
				Ar.Seek(NumBytesLocation);
				INT ActualNumBytes = CurrentArchiveLocation - NumBytesLocation - sizeof(NumBytes);
				Ar << ActualNumBytes;
				Ar.Seek(CurrentArchiveLocation);
			}
		}

		// Measure the memory allocated and used by the speedtree.
		if( Ar.IsCountingMemory() )
		{
			if ( SRH )
			{
				if ( SRH->SpeedTree )
				{
					#if (SPEEDTREE_VERSION_MINOR > 0)
						Ar.CountBytes(SRH->SpeedTree->GetSdkHeapUsage(), SRH->SpeedTree->GetSdkHeapUsage() );
					#else
						Ar.CountBytes(SRH->SpeedTree->GetTotalHeapUsage(), SRH->SpeedTree->GetTotalHeapUsage() );
					#endif
				}

				SRH->Branch1Elements.CountBytes( Ar );
				SRH->Branch2Elements.CountBytes( Ar );
				SRH->FrondElements.CountBytes( Ar );
				SRH->LeafCardElements.CountBytes( Ar );
				SRH->LeafMeshElements.CountBytes( Ar );

				Ar.CountBytes( sizeof(FSpeedTreeResourceHelper ), sizeof( FSpeedTreeResourceHelper ) );
			}
		}
	}
	else
	{
		if (Ar.IsLoading())
		{
			bLegacySpeedTree = TRUE;
			// Skip over the serialized data.
			INT NumBytes = 0;
			Ar << NumBytes;
			Ar.Seek( Ar.Tell() + NumBytes );
		}
		else
		{
			INT NumBytes = 0;
			Ar << NumBytes;
		}
	}
#else
	// Skip over the serialized data.
	INT NumBytes = 0;
	Ar << NumBytes;
	if (Ar.IsLoading())
	{
		Ar.Seek( Ar.Tell() + NumBytes );
	}
#endif

	// fix up guid if it's not been set
	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		SetLightingGuid();
	}
}

void USpeedTree::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	// make it has a new guid
	SetLightingGuid();

	// Ensure the rendering thread isn't accessing the object while it is being edited.
	FlushRenderingCommands();
}

void USpeedTree::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
#if WITH_SPEEDTREE
	if( PropertyThatChanged )
	{
		const FString PropertyName = PropertyThatChanged->GetName();

		// only take action on these properties
		if(	PropertyName == TEXT("Branch1Material")
			|| PropertyName == TEXT("Branch2Material")
			|| PropertyName == TEXT("FrondMaterial")
			|| PropertyName == TEXT("LeafCardMaterial")
			|| PropertyName == TEXT("LeafMeshMaterial")
			|| PropertyName == TEXT("BillboardMaterial"))
		{
			if( SRH && SRH->SpeedTree )
			{
				// Detach all instances of this SpeedTree, to ensure there aren't any static mesh elements remaining that reference the soon to be freed resources.
				// They will be reattached when the array is destructed.
				TArray<FComponentReattachContext> PrimitiveReattachContexts;
				for(TObjectIterator<USpeedTreeComponent> PrimitiveIt;PrimitiveIt;++PrimitiveIt)
				{
					if(PrimitiveIt->SpeedTree == this)
					{
						new(PrimitiveReattachContexts) FComponentReattachContext(*PrimitiveIt);
					}
				}

				SRH->CleanUp(FALSE);
				SRH->InitResources( );
			}
		}
	}
#endif
}

FString USpeedTree::GetDesc()
{
#if WITH_SPEEDTREE
	INT Triangles = 0;
	INT Verts = 0;

	if( SRH && SRH->bIsInitialized )
	{
		if( SRH->bHasBranches )
		{
			Triangles += SRH->Branch1Elements(0).GetNumPrimitives();	
			Triangles += SRH->Branch2Elements(0).GetNumPrimitives();	
			Verts += SRH->BranchDataBuffer.Vertices.Num();
		}

		if( SRH->bHasFronds )
		{
			Triangles += SRH->FrondElements(0).GetNumPrimitives();		
			Verts += SRH->FrondDataBuffer.Vertices.Num();
		}

		if( SRH->bHasLeafCards )
		{
			Triangles += SRH->LeafCardElements(0).GetNumPrimitives();	
			Verts += SRH->LeafCardDataBuffer.Vertices.Num( );
		}

		if( SRH->bHasLeafMeshes )
		{
			Triangles += SRH->LeafMeshElements(0).GetNumPrimitives();	
			Verts += SRH->LeafMeshDataBuffer.Vertices.Num( );
		}

		if( SRH->bHasBillboards )
		{
			Triangles += SRH->BillboardElement.GetNumPrimitives();
			Verts += SRH->BillboardDataBuffer.Vertices.Num( );
		}
	}

	return FString::Printf(TEXT("%d Triangles, %d Vertices"), Triangles, Verts);
#else
	return FString();
#endif
}

FString USpeedTree::GetDetailedDescription( INT InIndex )
{
#if WITH_SPEEDTREE
	INT Triangles = 0;
	INT Verts = 0;

	if( SRH && SRH->bIsInitialized )
	{
		if( SRH->bHasBranches )
		{
			Triangles += SRH->Branch1Elements(0).GetNumPrimitives();	
			Triangles += SRH->Branch2Elements(0).GetNumPrimitives();	
			Verts += SRH->BranchDataBuffer.Vertices.Num();
		}

		if( SRH->bHasFronds )
		{
			Triangles += SRH->FrondElements(0).GetNumPrimitives();	
			Verts += SRH->FrondDataBuffer.Vertices.Num();
		}

		if( SRH->bHasLeafCards )
		{
			Triangles += SRH->LeafCardElements(0).GetNumPrimitives();	
			Verts += SRH->LeafCardDataBuffer.Vertices.Num( );
		}

		if( SRH->bHasLeafMeshes )
		{
			Triangles += SRH->LeafMeshElements(0).GetNumPrimitives();	
			Verts += SRH->LeafMeshDataBuffer.Vertices.Num( );
		}

		if( SRH->bHasBillboards )
		{
			Triangles += SRH->BillboardElement.GetNumPrimitives();
			Verts += SRH->BillboardDataBuffer.Vertices.Num( );
		}
	}

	FString Description = TEXT("");

	switch(InIndex)
	{
	case 0:
		Description = FString::Printf(TEXT("%d Triangles"), Triangles);
		break;
	case 1: 
		Description = FString::Printf(TEXT("%d Vertices"), Verts);
		break;
	}

	return Description;
#else
	return FString();
#endif
}

INT USpeedTree::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		return 0;
	}
	
	FArchiveCountMem CountBytesSize(this);
	const INT ResourceSize = CountBytesSize.GetNum();
	return ResourceSize;
}

#if WITH_SPEEDTREE
const FSpeedTreeVertexData* GetSpeedTreeVertexData(const FSpeedTreeResourceHelper* SRH,INT MeshType,INT VertexIndex)
{
	switch(MeshType)
	{
	case STMT_Branches1:
	case STMT_Branches2:
		return &SRH->BranchDataBuffer.Vertices(VertexIndex);
	case STMT_Fronds:
		return &SRH->FrondDataBuffer.Vertices(VertexIndex);
	case STMT_LeafMeshes:
		return &SRH->LeafMeshDataBuffer.Vertices(VertexIndex);
	case STMT_LeafCards:
		return &SRH->LeafCardDataBuffer.Vertices(VertexIndex);
	case STMT_Billboards:
		return &SRH->BillboardDataBuffer.Vertices(VertexIndex);
	default:
		appErrorf(TEXT("Unknown SpeedTree mesh type: %u"),MeshType);
		return NULL;
	};
}
#endif
