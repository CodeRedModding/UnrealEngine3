/*=============================================================================
	SpeedTree.h: SpeedTree definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SPEEDTREE_H__
#define __SPEEDTREE_H__

#include "EngineSpeedTreeClasses.h"			// SpeedTree object definitions

#if WITH_SPEEDTREE

//////////////////////////////////////////////////////
// includes

#include "SpeedTreeVertexFactory.h"
#include "../../../External/SpeedTree/Include/Core/Core.h"

class UMaterialInstanceConstant;
class USpeedTreeComponent;

struct FSpeedTreeVertexPosition
{
	FVector			Position;

	friend FArchive& operator<<(FArchive& Ar,FSpeedTreeVertexPosition& VertexPosition)
	{
		Ar << VertexPosition.Position;
		return Ar;
	}
};

struct FSpeedTreeVertexData
{
	FPackedNormal	TangentX;
	FPackedNormal	TangentY;
	FPackedNormal	TangentZ;
	FVector2D		TexCoord;
	FLOAT			WindInfo[4];
	FLOAT			LODInfo[4]; // xyz = lod, w = wind scalar

	friend FArchive& operator<<(FArchive& Ar,FSpeedTreeVertexData& VertexData)
	{
		Ar	<< VertexData.TangentX
			<< VertexData.TangentY
			<< VertexData.TangentZ
			<< VertexData.TexCoord
			<< VertexData.WindInfo[0] << VertexData.WindInfo[1] << VertexData.WindInfo[2] << VertexData.WindInfo[3]
			<< VertexData.LODInfo[0] << VertexData.LODInfo[1] << VertexData.LODInfo[2] << VertexData.LODInfo[3];
		return Ar;
	}
};

struct FSpeedTreeVertexDataFrond : public FSpeedTreeVertexData
{
	FVector2D		FrondRipple;

	friend FArchive& operator<<(FArchive& Ar,FSpeedTreeVertexDataFrond& VertexData)
	{
		Ar << *((FSpeedTreeVertexData*)(&VertexData));
		Ar << VertexData.FrondRipple;
		return Ar;
	}
};

struct FSpeedTreeVertexDataLeafCard : public FSpeedTreeVertexData
{
	FVector			CornerOffset;

	friend FArchive& operator<<(FArchive& Ar,FSpeedTreeVertexDataLeafCard& VertexData)
	{
		Ar << *((FSpeedTreeVertexData*)(&VertexData));
		Ar << VertexData.CornerOffset;
		return Ar;
	}
};

struct FSpeedTreeVertexDataBillboard : public FSpeedTreeVertexData
{
	FLOAT bIsVerticalBillboard;

	friend FArchive& operator<<(FArchive& Ar,FSpeedTreeVertexDataBillboard& VertexData)
	{
		Ar << *((FSpeedTreeVertexData*)(&VertexData));
		Ar << VertexData.bIsVerticalBillboard;
		return Ar;
	}
};

template<class VertexType>
class TSpeedTreeVertexBuffer : public FVertexBuffer
{
public:

	TArray<VertexType> Vertices;

	virtual void InitRHI()
	{
		const INT Size = Vertices.Num() * sizeof(VertexType);
		if(Vertices.Num())
		{
			// Recreate the vertex buffer
			VertexBufferRHI = RHICreateVertexBuffer(Size,NULL,RUF_Static);

			// Copy the vertex data into the vertex buffer.
			void* const Buffer = RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);
			appMemcpy(Buffer,&Vertices(0),Size);
			RHIUnlockVertexBuffer(VertexBufferRHI);
		}
	}

	virtual void ReleaseRHI()
	{
		VertexBufferRHI.SafeRelease(); 
	}

	virtual void Serialize(FArchive& Ar)
	{
		Vertices.BulkSerialize(Ar);
	}
	
	virtual FString GetFriendlyName(void) const	{ return TEXT("SpeedTree vertex buffer"); }
};

/** Chooses from the parameters given based on a given mesh type. */
template<typename T>
typename TCallTraits<T>::ParamType ChooseByMeshType(
	INT MeshType,
	typename TCallTraits<T>::ParamType Branches1Choice,
	typename TCallTraits<T>::ParamType Branches2Choice,
	typename TCallTraits<T>::ParamType FrondsChoice,
	typename TCallTraits<T>::ParamType LeafCardsChoice,
	typename TCallTraits<T>::ParamType LeafMeshesChoice,
	typename TCallTraits<T>::ParamType BillboardsChoice
	)
{
	switch(MeshType)
	{
	case STMT_Branches1: return Branches1Choice;
	case STMT_Branches2: return Branches2Choice;
	case STMT_Fronds: return FrondsChoice;
	case STMT_LeafCards: return LeafCardsChoice;
	case STMT_LeafMeshes: return LeafMeshesChoice;
	default:
	case STMT_Billboards: return BillboardsChoice;
	};
}

/**
 * Reads the SpeedTree vertex data for a specific mesh type.
 * @param SRH - The SpeedTree resource data to access.
 * @param MeshType - The mesh type the vertex is in.
 * @param VertexIndex - The index of the vertex.
 */
const FSpeedTreeVertexData* GetSpeedTreeVertexData(const FSpeedTreeResourceHelper* SRH,INT MeshType,INT VertexIndex);

class FSpeedTreeResourceHelper
{
public:

	FSpeedTreeResourceHelper( class USpeedTree* InOwner );

	void Load( const BYTE* Buffer, INT NumBytes );
	void SetupIndexedGeometry(ESpeedTreeMeshType eType);
	void SetupLeafCards();
	void SetupBillboards();
	void InitResources();

	void UpdateWind(const FVector& WindDirection, FLOAT WindStrength, FLOAT CurrentTime);
	void UpdateGeometry(const FVector& CurrentCameraOrigin,const FVector& CurrentCameraZ,UBOOL bApplyWind);

	void GetVertBillboardTexcoordBiasOffset(const FLOAT Angle, FVector4& BiasOffset) const;

	void CleanUp(BOOL bAll);

	INT GetNumCollisionPrimitives();
	const SpeedTree::SCollisionObject* GetCollisionPrimitive( INT Index );

	void BuildTexelFactors();

	class USpeedTree*					Owner;

	/** TRUE if branches should be drawn. */
	UBOOL bHasBranches;

	/** TRUE if fronds should be drawn. */
	UBOOL bHasFronds;

	/** TRUE if leaf cards should be drawn. */
	UBOOL bHasLeafCards;

	/** TRUE if leaf meshes should be drawn. */
	UBOOL bHasLeafMeshes;

	/** TRUE if vertical billboards should be drawn. */
	UBOOL bHasBillboards;
	UBOOL bHasHorzBillboard;

	/** TRUE if the resources have been initialized. */
	UBOOL bIsInitialized;

	SpeedTree::CCore*					SpeedTree;
	FBoxSphereBounds					Bounds;

	TSpeedTreeVertexBuffer<FSpeedTreeVertexPosition>		BranchPositionBuffer;		// buffer for vertex position
	TSpeedTreeVertexBuffer<FSpeedTreeVertexData>			BranchDataBuffer;			// buffer for normals, texcoords, etc

	TSpeedTreeVertexBuffer<FSpeedTreeVertexPosition>		FrondPositionBuffer;		// buffer for vertex position
	TSpeedTreeVertexBuffer<FSpeedTreeVertexDataFrond>		FrondDataBuffer;			// buffer for normals, texcoords, etc

	TSpeedTreeVertexBuffer<FSpeedTreeVertexPosition>		LeafCardPositionBuffer;		// buffer for leaf card position
	TSpeedTreeVertexBuffer<FSpeedTreeVertexDataLeafCard>	LeafCardDataBuffer;			// buffer for leaf card normals, texcoords, etc

	TSpeedTreeVertexBuffer<FSpeedTreeVertexPosition>		LeafMeshPositionBuffer;		// buffer for leaf mesh position
	TSpeedTreeVertexBuffer<FSpeedTreeVertexData>			LeafMeshDataBuffer;			// buffer for leaf mesh normals, texcoords, etc

	TSpeedTreeVertexBuffer<FSpeedTreeVertexPosition>		BillboardPositionBuffer;	// buffer for leaves' and billboards' position
	TSpeedTreeVertexBuffer<FSpeedTreeVertexDataBillboard>	BillboardDataBuffer;		// buffer for leaves' and billboards' data 


	FSpeedTreeBranchVertexFactory		BranchVertexFactory;		// vertex factory for branches
	FSpeedTreeFrondVertexFactory		FrondVertexFactory;			// vertex factory for fronds
	FSpeedTreeLeafCardVertexFactory		LeafCardVertexFactory;		// vertex factory for the leaf cards
	FSpeedTreeLeafMeshVertexFactory		LeafMeshVertexFactory;		// vertex factory for the leaf meshes
	FSpeedTreeBillboardVertexFactory	BillboardVertexFactory;		// vertex factory for billboards

	FRawIndexBuffer						IndexBuffer;				// the index buffer (holds everything)

	TArray<FMeshBatch>				Branch1Elements;			// sections to draw per branch LOD
	TArray<FMeshBatch>				Branch2Elements;			// sections to draw per branch LOD
	TArray<FMeshBatch>				FrondElements;				// sections to draw per frond LOD
	TArray<FMeshBatch>				LeafCardElements;			// sections to draw per leaf LOD
	TArray<FMeshBatch>				LeafMeshElements;			// sections to draw per leaf LOD
	FMeshBatch						BillboardElement;			// sections to draw per billboard (and horizontal billboard)

	TArray<FVector4>					BillboardTexcoordScaleBias;	// billboard texcoord offsets				

    FRenderCommandFence					ReleaseResourcesFence;

	FVector								CachedCameraOrigin;
	FVector								CachedCameraZ;
	FLOAT								LastWindTime;
	FLOAT								WindTimeOffset;

	/** The maximum ratio between local space coordinates and texture coordinates for the highest LOD of each mesh type the SpeedTree has.  Not valid unless bHasValidTexelFactors==TRUE. */
	FLOAT								TexelFactors[STMT_Max];

	/** TRUE if the cached texel factors above are valid. */
	UBOOL								bHasValidTexelFactors;

	FVector2D							LeafAngleScalars;

	FLOAT								BranchFadingDistance;
	FLOAT								FrondFadingDistance;
};

#endif // WITH_SPEEDTREE

#endif
