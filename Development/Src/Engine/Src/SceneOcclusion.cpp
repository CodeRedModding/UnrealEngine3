/*=============================================================================
	SceneRendering.cpp: Scene rendering.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"

/*-----------------------------------------------------------------------------
	Globals
-----------------------------------------------------------------------------*/

/** Whether precomputed visibility should be used for culling. */
UBOOL GAllowPrecomputedVisibility = TRUE;

/** Whether to draw all precomputed visibility cells. */
UBOOL GShowPrecomputedVisibilityCells = FALSE;

/** Whether to draw relevant precomputed visibility cells only. */
UBOOL GShowRelevantPrecomputedVisibilityCells = FALSE;

#define NUM_CUBE_VERTICES 36


/** Random table for occlusion **/
FOcclusionRandomStream GOcclusionRandomStream;

/**
* A vertex shader for rendering a texture on a simple element.
*/
template<UINT VerticesPerPrimitive>
class FOcclusionQueryVertexShader : public FGlobalShader
{
	DECLARE_SHADER_TYPE(FOcclusionQueryVertexShader,Global);
public:
	static UBOOL ShouldCache(EShaderPlatform Platform) { return TRUE; }

	FOcclusionQueryVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FGlobalShader(Initializer)
	{
	}
	FOcclusionQueryVertexShader() {}

	static void ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment)
	{
		FVertexFactory::ModifyCompilationEnvironment(Platform, OutEnvironment);
		OutEnvironment.Definitions.Set(TEXT("VERTICES_PER_PRIMITIVE_INSTANCE"),*FString::Printf(TEXT("%u"),VerticesPerPrimitive));
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
#if PS3
		//@hack - compiler bug? optimized version crashes during FShader::Serialize call
		static INT RemoveMe=0;	RemoveMe=1;
#endif
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		return bShaderHasOutdatedParameters;
	}
};

// default, non-instanced shader implementation
IMPLEMENT_SHADER_TYPE(template<>,FOcclusionQueryVertexShader<0>,TEXT("OcclusionQueryVertexShader"),TEXT("Main"),SF_Vertex,0,0);
// cube instances shader implementation
IMPLEMENT_SHADER_TYPE(template<>,FOcclusionQueryVertexShader<NUM_CUBE_VERTICES>,TEXT("OcclusionQueryVertexShader"),TEXT("Main"),SF_Vertex,0,0);

/**
* The occlusion query position-only vertex declaration resource type.
*/
class FOcclusionQueryPosOnlyVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FOcclusionQueryPosOnlyVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float3,VEU_Position,0));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};
/** The occlusion query position-only vertex declaration. */
TGlobalResource<FOcclusionQueryPosOnlyVertexDeclaration> GOcclusionQueryPosOnlyVertexDeclaration;

#if XBOX
/**
* The occlusion query instanced primitives vertex declaration resource type.
*/
class FOcclusionQueryInstancingVertexDeclaration : public FRenderResource
{
public:

	FVertexDeclarationRHIRef VertexDeclarationRHI;

	/** Destructor. */
	virtual ~FOcclusionQueryInstancingVertexDeclaration() {}

	virtual void InitRHI()
	{
		FVertexDeclarationElementList Elements;
		Elements.AddItem(FVertexElement(0,0,VET_Float3,VEU_Position,0));
		Elements.AddItem(FVertexElement(1,STRUCT_OFFSET(FOcclusionPrimitive,Origin),VET_Float3,VEU_Position,1));
		Elements.AddItem(FVertexElement(1,STRUCT_OFFSET(FOcclusionPrimitive,Extent),VET_Float3,VEU_Position,2));
		Elements.AddItem(FVertexElement(2,0,VET_UByte4,VEU_Position,3));
		VertexDeclarationRHI = RHICreateVertexDeclaration(Elements);
	}

	virtual void ReleaseRHI()
	{
		VertexDeclarationRHI.SafeRelease();
	}
};

/** 
* Simple vertex buffer class for an occlusion query box
*/
class FOcclusionQueryBoxVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI()
	{
		// create a static vertex buffer
		VertexBufferRHI = RHICreateVertexBuffer(8 * sizeof(FVector), NULL, RUF_Static);
		FVector* Vertices = (FVector*)RHILockVertexBuffer(VertexBufferRHI, 0, 8 * sizeof(FVector), FALSE);
		Vertices[0] = FVector(-1, -1, -1);
		Vertices[1] = FVector(-1, -1, +1);
		Vertices[2] = FVector(-1, +1, -1);
		Vertices[3] = FVector(-1, +1, +1);
		Vertices[4] = FVector(+1, -1, -1);
		Vertices[5] = FVector(+1, -1, +1);
		Vertices[6] = FVector(+1, +1, -1);
		Vertices[7] = FVector(+1, +1, +1);
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};

/** 
* Simple vertex buffer class for the occlusion query primitives
*/
class FOcclusionQueryPrimitivesVertexBuffer : public FVertexBuffer
{
public:
	FOcclusionQueryPrimitivesVertexBuffer() : Primitives(NULL), Size(0) {}

	/** 
	* Initialize the dynamic RHI for this rendering resource 
	*/
	virtual void InitDynamicRHI()
	{
		if(Size)
		{
			// create a vertex buffer
			VertexBufferRHI = RHICreateVertexBuffer(Size, NULL, RUF_Dynamic);
			if(Primitives)
			{
				// Lock the buffer.
				void* Buffer = RHILockVertexBuffer(VertexBufferRHI,0,Size,FALSE);
				// copy over the primitive data
				appMemcpy(Buffer, (void*)Primitives, Size);
				// Unlock the buffer.
				RHIUnlockVertexBuffer(VertexBufferRHI);
			}
		}
	}

	/** 
	* Release the dynamic RHI for this rendering resource 
	*/
	virtual void ReleaseDynamicRHI()
	{
		VertexBufferRHI.SafeRelease();
	}

	void SetPrimitives(	FOcclusionPrimitive *InPrimitives, UINT InCount)
	{
		Primitives = InPrimitives;
		Size = InCount * sizeof(FOcclusionPrimitive);
	}

private:
	FOcclusionPrimitive *Primitives;
	UINT Size;
};

/** 
* Simple vertex buffer class for the occlusion query Indices
*/
class FOcclusionQueryIndicesVertexBuffer : public FVertexBuffer
{
public:
	/** 
	* Initialize the RHI for this rendering resource 
	*/
	virtual void InitRHI()
	{
		VertexBufferRHI = RHICreateVertexBuffer(NUM_CUBE_VERTICES, NULL, RUF_Static);
		BYTE* Indices = (BYTE*)RHILockVertexBuffer(VertexBufferRHI, 0, NUM_CUBE_VERTICES, FALSE);
		// pack the cube indices to UBYTE4 for Xbox 
		for(int Index=0; Index < (NUM_CUBE_VERTICES - 3); Index+=4)
		{
			Indices[Index+3]	=	(BYTE)GCubeIndices[Index];
			Indices[Index+2]	=	(BYTE)GCubeIndices[Index+1];
			Indices[Index+1]	=	(BYTE)GCubeIndices[Index+2];
			Indices[Index]		=	(BYTE)GCubeIndices[Index+3];
		}
		RHIUnlockVertexBuffer(VertexBufferRHI);
	}
};

/**
 * FOcclusionQueryDrawResource
 * 
 * Encapsulates global occlusion drawing resources.
 */
class FOcclusionQueryDrawResource : public FRenderResource
{
public:
	void DrawShared( FOcclusionPrimitive *InPrimitives, UINT InCount)
	{
		if( !IsValidRef(BoundShaderState) )
		{
			TShaderMapRef<FOcclusionQueryVertexShader<NUM_CUBE_VERTICES>> VertexShader(GetGlobalShaderMap());
			DWORD Strides[MaxVertexElementCount];
			appMemzero(Strides, sizeof(Strides));
			Strides[0] = sizeof(FVector);
			Strides[1] = sizeof(FOcclusionPrimitive);
			Strides[2] = sizeof(DWORD);
			BoundShaderState = RHICreateBoundShaderState(
				VertexDeclaration.VertexDeclarationRHI, 
				Strides, 
				VertexShader->GetVertexShader(), 
				NULL,
				EGST_None
				);
		}
		PrimitivesVertexBuffer.SetPrimitives(InPrimitives, InCount);
		PrimitivesVertexBuffer.UpdateRHI();
		// setup the streams
		RHISetStreamSource(0, BoxVertexBuffer.VertexBufferRHI, sizeof(FVector),0,FALSE,0,1);
		RHISetStreamSource(1, PrimitivesVertexBuffer.VertexBufferRHI, sizeof(FOcclusionPrimitive),0,FALSE,0,1);
		RHISetStreamSource(2, IndicesVertexBuffer.VertexBufferRHI, sizeof(DWORD),0,FALSE,0,1);
		// set the bound shader state
		RHISetBoundShaderState( BoundShaderState );
	}

	// FRenderResource interface.
	virtual void InitResource()
	{
		FRenderResource::InitResource();
		BoxVertexBuffer.InitResource();
		PrimitivesVertexBuffer.InitResource();;
		IndicesVertexBuffer.InitResource();;
		VertexDeclaration.InitResource();;
	}
	// FRenderResource interface.
	virtual void ReleaseResource()
	{
		FRenderResource::ReleaseResource();
		BoxVertexBuffer.ReleaseResource();
		PrimitivesVertexBuffer.ReleaseResource();
		IndicesVertexBuffer.ReleaseResource();
		VertexDeclaration.ReleaseResource();

		BoundShaderState.SafeRelease();
	}

private:
	FOcclusionQueryBoxVertexBuffer BoxVertexBuffer;
	FOcclusionQueryPrimitivesVertexBuffer PrimitivesVertexBuffer;
	FOcclusionQueryIndicesVertexBuffer IndicesVertexBuffer;
	FOcclusionQueryInstancingVertexDeclaration VertexDeclaration;
	FBoundShaderStateRHIRef BoundShaderState;

};
TGlobalResource<FOcclusionQueryDrawResource> GOcclusionDrawer;

#endif // XBOX


INT GNumQueriesAllocated = 0;
INT GNumQueriesInPools = 0;
INT GNumQueriesOutstanding = 0;
FOcclusionQueryPool::~FOcclusionQueryPool()
{
	Release();
}

void FOcclusionQueryPool::Release()
{
	OcclusionQueries.Empty();
	GNumQueriesInPools = 0;
}

FOcclusionQueryRHIRef FOcclusionQueryPool::AllocateQuery()
{
	GNumQueriesOutstanding++;

	// Are we out of available occlusion queries?
	if ( OcclusionQueries.Num() == 0 )
	{
		++GNumQueriesAllocated;
		// Create a new occlusion query.
		return RHICreateOcclusionQuery();
	}

	GNumQueriesInPools--;
	return OcclusionQueries.Pop();
}

void FOcclusionQueryPool::ReleaseQuery( FOcclusionQueryRHIRef &Query )
{
	if ( IsValidRef(Query) )
	{
		// Is no one else keeping a refcount to the query?
		if ( Query.GetRefCount() == 1 )
		{
			// Return it to the pool.
			OcclusionQueries.AddItem( Query );
			GNumQueriesInPools++;
			GNumQueriesOutstanding--;

			// Tell RHI we don't need the result anymore.
			RHIResetOcclusionQuery( Query );
		}

		// De-ref without deleting.
		Query = NULL;
	}
}

FGlobalBoundShaderState FSceneRenderer::OcclusionTestBoundShaderState;

/** 
 * Returns an array of visibility data for the given view position, or NULL if none exists. 
 * The data bits are indexed by VisibilityId of each primitive in the scene.
 * This method decompresses data if necessary and caches it based on the bucket and chunk index in the view state.
 */
BYTE* FSceneViewState::GetPrecomputedVisibilityData(FViewInfo& View, const FScene* Scene)
{
	extern UBOOL GIsCurrentlyPrecaching;

	BYTE* PrecomputedVisibilityData = NULL;
	if (Scene->PrecomputedVisibilityHandler && GAllowPrecomputedVisibility && !GIsCurrentlyPrecaching)
	{
		const FPrecomputedVisibilityHandler& Handler = *Scene->PrecomputedVisibilityHandler;
		FViewElementPDI VisibilityCellsPDI(&View, NULL);

		// Draw visibility cell bounds for debugging if enabled
		if (GShowPrecomputedVisibilityCells && !GShowRelevantPrecomputedVisibilityCells)
		{
			for (INT BucketIndex = 0; BucketIndex < Handler.PrecomputedVisibilityCellBuckets.Num(); BucketIndex++)
			{
				for (INT CellIndex = 0; CellIndex < Handler.PrecomputedVisibilityCellBuckets(BucketIndex).Cells.Num(); CellIndex++)
				{
					const FPrecomputedVisibilityCell& CurrentCell = Handler.PrecomputedVisibilityCellBuckets(BucketIndex).Cells(CellIndex);
					// Construct the cell's bounds
					const FBox CellBounds(CurrentCell.Min, CurrentCell.Min + FVector(Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeZ));
					if (View.ViewFrustum.IntersectBox(CellBounds.GetCenter(), CellBounds.GetExtent()))
					{
						DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 50, 255), SDPG_World);
					}
				}
			}
		}

		// Calculate the bucket that ViewOrigin falls into
		// Cells are hashed into buckets to reduce search time
		const FLOAT FloatOffsetX = (View.ViewOrigin.X - Handler.PrecomputedVisibilityCellBucketOriginXY.X) / Handler.PrecomputedVisibilityCellSizeXY;
		// appTrunc rounds toward 0, we want to always round down
		const INT BucketIndexX = Abs((appTrunc(FloatOffsetX) - (FloatOffsetX < 0.0f ? 1 : 0)) / Handler.PrecomputedVisibilityCellBucketSizeXY % Handler.PrecomputedVisibilityNumCellBuckets);
		const FLOAT FloatOffsetY = (View.ViewOrigin.Y -Handler.PrecomputedVisibilityCellBucketOriginXY.Y) / Handler.PrecomputedVisibilityCellSizeXY;
		const INT BucketIndexY = Abs((appTrunc(FloatOffsetY) - (FloatOffsetY < 0.0f ? 1 : 0)) / Handler.PrecomputedVisibilityCellBucketSizeXY % Handler.PrecomputedVisibilityNumCellBuckets);
		const INT PrecomputedVisibilityBucketIndex = BucketIndexY * Handler.PrecomputedVisibilityCellBucketSizeXY + BucketIndexX;

		check(PrecomputedVisibilityBucketIndex < Handler.PrecomputedVisibilityCellBuckets.Num());
		const FPrecomputedVisibilityBucket& CurrentBucket = Handler.PrecomputedVisibilityCellBuckets(PrecomputedVisibilityBucketIndex);
		for (INT CellIndex = 0; CellIndex < CurrentBucket.Cells.Num(); CellIndex++)
		{
			const FPrecomputedVisibilityCell& CurrentCell = CurrentBucket.Cells(CellIndex);
			// Construct the cell's bounds
			const FBox CellBounds(CurrentCell.Min, CurrentCell.Min + FVector(Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeXY, Handler.PrecomputedVisibilityCellSizeZ));
			// Check if ViewOrigin is inside the current cell
			if (CellBounds.IsInside(View.ViewOrigin))
			{
				// Reuse a cached decompressed chunk if possible
				if (CachedVisibilityChunk
					&& CachedVisibilityHandlerId == Scene->PrecomputedVisibilityHandler->GetId()
					&& CachedVisibilityBucketIndex == PrecomputedVisibilityBucketIndex
					&& CachedVisibilityChunkIndex == CurrentCell.ChunkIndex)
				{
					checkSlow(CachedVisibilityChunk->Num() >= CurrentCell.DataOffset + CurrentBucket.CellDataSize);
					PrecomputedVisibilityData = &(*CachedVisibilityChunk)(CurrentCell.DataOffset);
				}
				else
				{
					const FCompressedVisibilityChunk& CompressedChunk = Handler.PrecomputedVisibilityCellBuckets(PrecomputedVisibilityBucketIndex).CellDataChunks(CurrentCell.ChunkIndex);
					CachedVisibilityBucketIndex = PrecomputedVisibilityBucketIndex;
					CachedVisibilityChunkIndex = CurrentCell.ChunkIndex;
					CachedVisibilityHandlerId = Scene->PrecomputedVisibilityHandler->GetId();
					if (!CachedVisibilityChunk)
					{
						// Allocate memory for the cached decompressed chunk the first time
						CachedVisibilityChunk = new TArray<BYTE>();
					}

					if (CompressedChunk.bCompressed)
					{
						// Decompress the needed visibility data chunk
						CachedVisibilityChunk->Reset();
						CachedVisibilityChunk->Add(CompressedChunk.UncompressedSize);
						verify(appUncompressMemory(
							COMPRESS_ZLIB, 
							CachedVisibilityChunk->GetData(),
							CompressedChunk.UncompressedSize,
							CompressedChunk.Data.GetData(),
							CompressedChunk.Data.Num()));
					}
					else
					{
						(*CachedVisibilityChunk) = CompressedChunk.Data;
					}

					checkSlow(CachedVisibilityChunk->Num() >= CurrentCell.DataOffset + CurrentBucket.CellDataSize);
					// Return a pointer to the cell containing ViewOrigin's decompressed visibility data
					PrecomputedVisibilityData = &(*CachedVisibilityChunk)(CurrentCell.DataOffset);
				}

				if (GShowRelevantPrecomputedVisibilityCells)
				{
					// Draw the currently used visibility cell with green wireframe for debugging
					DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 255, 50), SDPG_Foreground);
				}
				else
				{
					break;
				}
			}
			else if (GShowRelevantPrecomputedVisibilityCells)
			{
				// Draw all cells in the current visibility bucket as blue wireframe
				DrawWireBox(&VisibilityCellsPDI, CellBounds, FColor(50, 50, 255), SDPG_World);
			}
		}
	}
	return PrecomputedVisibilityData;
}

void FSceneViewState::TrimOcclusionHistory(FLOAT MinHistoryTime,FLOAT MinQueryTime,INT FrameNumber)
{
	// Only trim every few frames, since stale entries won't cause problems
	if (FrameNumber % 6 == 0)
	{
		for(TSet<FPrimitiveOcclusionHistory,FPrimitiveOcclusionHistoryKeyFuncs>::TIterator PrimitiveIt(PrimitiveOcclusionHistorySet);
			PrimitiveIt;
			++PrimitiveIt
			)
		{
			// If the primitive has an old pending occlusion query, release it.
			if(PrimitiveIt->LastConsideredTime < MinQueryTime)
			{
				PrimitiveIt->ReleaseQueries(OcclusionQueryPool);
			}

			// If the primitive hasn't been considered for visibility recently, remove its history from the set.
			if(PrimitiveIt->LastConsideredTime < MinHistoryTime)
			{
				PrimitiveIt.RemoveCurrent();
			}
		}
	}
}

UBOOL FSceneViewState::IsShadowOccluded(const UPrimitiveComponent* Primitive, const ULightComponent* Light, INT SplitIndex) const
{
	// Find the shadow's occlusion query from the previous frame.
	const FSceneViewState::FProjectedShadowKey Key(Primitive, Light, SplitIndex);
	const FOcclusionQueryRHIRef* Query = ShadowOcclusionQueryMap.Find(Key);

	// Read the occlusion query results.
	DWORD NumPixels = 0;
	// Only block on the query if not running SLI
	const UBOOL bWaitOnQuery = GNumActiveGPUsForRendering == 1;

	if(Query && RHIGetOcclusionQueryResult(*Query, NumPixels, bWaitOnQuery))
	{
		// If the shadow's occlusion query didn't have any pixels visible the previous frame, it's occluded.
		return NumPixels == 0;
	}
	else
	{
		// If the shadow wasn't queried the previous frame, it isn't occluded.

		return FALSE;
	}
}

/**
 *	Retrieves the percentage of the views render target the primitive touched last time it was rendered.
 *
 *	@param	Primitive				The primitive of interest.
 *	@param	OutCoveragePercentage	REFERENCE: The screen coverate percentage. (OUTPUT)
 *	@return	UBOOL					TRUE if the primitive was found and the results are valid, FALSE is not
 */
UBOOL FSceneViewState::GetPrimitiveCoveragePercentage(const UPrimitiveComponent* Primitive, FLOAT& OutCoveragePercentage)
{
	FPrimitiveOcclusionHistory* PrimitiveOcclusionHistory = PrimitiveOcclusionHistorySet.Find(Primitive);
	if (PrimitiveOcclusionHistory)
	{
		OutCoveragePercentage = PrimitiveOcclusionHistory->LastPixelsPercentage;
		return TRUE;
	}

	return FALSE;
}

SIZE_T FSceneViewState::GetSizeBytes() const
{
	return sizeof(*this) 
		+ ShadowOcclusionQueryMap.GetAllocatedSize() 
		+ ParentPrimitives.GetAllocatedSize() 
		+ PrimitiveVisibilityStates.GetAllocatedSize() 
		+ PrimitiveFadingStates.GetAllocatedSize()
		+ PrimitiveOcclusionHistorySet.GetAllocatedSize();
}

FOcclusionQueryBatcher::FOcclusionQueryBatcher(class FSceneViewState* ViewState,UINT InMaxBatchedPrimitives)
:	CurrentBatchOcclusionQuery(FOcclusionQueryRHIRef())
,	MaxBatchedPrimitives(InMaxBatchedPrimitives)
,	NumBatchedPrimitives(0)
,	OcclusionQueryPool(ViewState ? &ViewState->OcclusionQueryPool : NULL)
{}

FOcclusionQueryBatcher::~FOcclusionQueryBatcher()
{
	check(!Primitives.Num());
}

void FOcclusionQueryBatcher::Flush()
{
	if(BatchOcclusionQueries.Num())
	{
#if !XBOX
		FMemMark MemStackMark(GRenderingThreadMemStack);

		// Create the indices for MaxBatchedPrimitives boxes.
		WORD* BakedIndices = new(GRenderingThreadMemStack) WORD[MaxBatchedPrimitives * 12 * 3];
		for(UINT PrimitiveIndex = 0;PrimitiveIndex < MaxBatchedPrimitives;PrimitiveIndex++)
		{
			for(INT Index = 0;Index < NUM_CUBE_VERTICES;Index++)
			{
				BakedIndices[PrimitiveIndex * NUM_CUBE_VERTICES + Index] = PrimitiveIndex * 8 + GCubeIndices[Index];
			}
		}

		// Draw the batches.
		for(INT BatchIndex = 0;BatchIndex < BatchOcclusionQueries.Num();BatchIndex++)
		{
			FOcclusionQueryRHIParamRef BatchOcclusionQuery = BatchOcclusionQueries(BatchIndex);
			const INT NumPrimitivesInBatch = Clamp<INT>( Primitives.Num() - BatchIndex * MaxBatchedPrimitives, 0, MaxBatchedPrimitives );
				
			RHIBeginOcclusionQuery(BatchOcclusionQuery);

			FLOAT* RESTRICT Vertices;
			WORD* RESTRICT Indices;
			RHIBeginDrawIndexedPrimitiveUP(PT_TriangleList,NumPrimitivesInBatch * 12,NumPrimitivesInBatch * 8,sizeof(FVector),*(void**)&Vertices,0,NumPrimitivesInBatch * 12 * 3,sizeof(WORD),*(void**)&Indices);

			for(INT PrimitiveIndex = 0;PrimitiveIndex < NumPrimitivesInBatch;PrimitiveIndex++)
			{
				const FOcclusionPrimitive& Primitive = Primitives(BatchIndex * MaxBatchedPrimitives + PrimitiveIndex);
				const UINT BaseVertexIndex = PrimitiveIndex * 8;
				const FVector PrimitiveBoxMin = Primitive.Origin - Primitive.Extent;
				const FVector PrimitiveBoxMax = Primitive.Origin + Primitive.Extent;

				Vertices[ 0] = PrimitiveBoxMin.X; Vertices[ 1] = PrimitiveBoxMin.Y; Vertices[ 2] = PrimitiveBoxMin.Z;
				Vertices[ 3] = PrimitiveBoxMin.X; Vertices[ 4] = PrimitiveBoxMin.Y; Vertices[ 5] = PrimitiveBoxMax.Z;
				Vertices[ 6] = PrimitiveBoxMin.X; Vertices[ 7] = PrimitiveBoxMax.Y; Vertices[ 8] = PrimitiveBoxMin.Z;
				Vertices[ 9] = PrimitiveBoxMin.X; Vertices[10] = PrimitiveBoxMax.Y; Vertices[11] = PrimitiveBoxMax.Z;
				Vertices[12] = PrimitiveBoxMax.X; Vertices[13] = PrimitiveBoxMin.Y; Vertices[14] = PrimitiveBoxMin.Z;
				Vertices[15] = PrimitiveBoxMax.X; Vertices[16] = PrimitiveBoxMin.Y; Vertices[17] = PrimitiveBoxMax.Z;
				Vertices[18] = PrimitiveBoxMax.X; Vertices[19] = PrimitiveBoxMax.Y; Vertices[20] = PrimitiveBoxMin.Z;
				Vertices[21] = PrimitiveBoxMax.X; Vertices[22] = PrimitiveBoxMax.Y; Vertices[23] = PrimitiveBoxMax.Z;

				Vertices += 24;
			}

			appMemcpy(Indices,BakedIndices,sizeof(WORD) * NumPrimitivesInBatch * 12 * 3);

			RHIEndDrawIndexedPrimitiveUP();

			RHIEndOcclusionQuery(BatchOcclusionQuery);
		}
#else // XBOX
		// prepare RHI for drawing
		GOcclusionDrawer.DrawShared(Primitives.GetTypedData(), Primitives.Num());
		// Draw the batches.
		for(INT BatchIndex = 0;BatchIndex < BatchOcclusionQueries.Num();BatchIndex++)
		{
			FOcclusionQueryRHIParamRef BatchOcclusionQuery = BatchOcclusionQueries(BatchIndex);
			FOcclusionPrimitive *BatchPrimitives = Primitives.GetTypedData() + BatchIndex * MaxBatchedPrimitives;
			INT BatchVertexIndex = NUM_CUBE_VERTICES * BatchIndex * MaxBatchedPrimitives;
			INT NumPrimitivesInBatch = 12 * Clamp<INT>( Primitives.Num() - BatchIndex * MaxBatchedPrimitives, 0, MaxBatchedPrimitives );
			RHIBeginOcclusionQuery(BatchOcclusionQuery);
			RHIDrawPrimitive(PT_TriangleList,BatchVertexIndex,NumPrimitivesInBatch);
			RHIEndOcclusionQuery(BatchOcclusionQuery);
		}
#endif
		INC_DWORD_STAT_BY(STAT_OcclusionQueries,BatchOcclusionQueries.Num());

		// Reset the batch state.
		BatchOcclusionQueries.Empty(BatchOcclusionQueries.Num());
		Primitives.Empty(Primitives.Num());
		CurrentBatchOcclusionQuery = FOcclusionQueryRHIRef();
	}
}

FOcclusionQueryRHIParamRef FOcclusionQueryBatcher::BatchPrimitive(const FVector& BoundsOrigin,const FVector& BoundsBoxExtent)
{
	// Check if we need a new batch or if the current batch is full.
	if( NumBatchedPrimitives == 0 || NumBatchedPrimitives >= MaxBatchedPrimitives )
	{
		check(OcclusionQueryPool);
		INT Index = BatchOcclusionQueries.AddItem( OcclusionQueryPool->AllocateQuery() );
		CurrentBatchOcclusionQuery = BatchOcclusionQueries( Index );
		NumBatchedPrimitives = 0;
	}

	// Add the primitive to the current batch.
	FOcclusionPrimitive* const Primitive = new(Primitives) FOcclusionPrimitive;
	Primitive->Origin = BoundsOrigin;
	Primitive->Extent = BoundsBoxExtent;
	NumBatchedPrimitives++;

	return CurrentBatchOcclusionQuery;
}

void FSceneRenderer::BeginOcclusionTests()
{
	SCOPE_CONDITIONAL_CYCLE_COUNTER(STAT_BeginOcclusionTestsTime, !bIsSceneCapture);
	const UBOOL bUseDownsampledDepth = IsValidRef(GSceneRenderTargets.GetSmallDepthSurface()) && GSceneRenderTargets.UseDownsizedOcclusionQueries();
	if (bUseDownsampledDepth)
	{
#if CONSOLE
		// Bind the small depth buffer for occlusion queries which are fill bound and benefit from the lower resolution
		RHISetRenderTarget(NULL, GSceneRenderTargets.GetSmallDepthSurface());
#else
		// Bind the downsampled translucency buffer as a color buffer
		// D3D requires a color buffer of the same size to be bound, even though we are not writing to color
		RHISetRenderTarget(GSceneRenderTargets.GetTranslucencyBufferSurface(), GSceneRenderTargets.GetSmallDepthSurface());
#endif
	}
	else
	{
		// Use the full resolution depth buffer
		GSceneRenderTargets.BeginRenderingSceneColor();
	}

	// Perform occlusion queries for each view
	for(INT ViewIndex = 0;ViewIndex < Views.Num();ViewIndex++)
	{
		SCOPED_DRAW_EVENT(EventBeginOcclude)(DEC_SCENE_ITEMS,TEXT("BeginOcclusionTests"));
		FViewInfo& View = Views(ViewIndex);

		RHISetViewParameters(View);
		RHISetMobileHeightFogParams(View.HeightFogParams);

		if (bUseDownsampledDepth)
		{
			const UINT DownsampledX = appTrunc(View.RenderTargetX / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const UINT DownsampledY = appTrunc(View.RenderTargetY / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const UINT DownsampledSizeX = appTrunc(View.RenderTargetSizeX / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());
			const UINT DownsampledSizeY = appTrunc(View.RenderTargetSizeY / GSceneRenderTargets.GetSmallColorDepthDownsampleFactor());

			// Setup the viewport for rendering to the downsampled depth buffer
			RHISetViewport(DownsampledX, DownsampledY, 0.0f, DownsampledX + DownsampledSizeX, DownsampledY + DownsampledSizeY, 1.0f);
		}
		else
		{
			RHISetViewport(View.RenderTargetX,View.RenderTargetY,0.0f,View.RenderTargetX + View.RenderTargetSizeX,View.RenderTargetY + View.RenderTargetSizeY,1.0f);
		}
    
	    FSceneViewState* ViewState = (FSceneViewState*)View.State;
    
	    if(ViewState && !View.bDisableQuerySubmissions)
	    {
			{
				SCOPED_DRAW_EVENT(EventShadowQueries)(DEC_SCENE_ITEMS,TEXT("ShadowFrustumQueries"));

				// Lookup the vertex shader.
				TShaderMapRef<FOcclusionQueryVertexShader<0> > VertexShader(GetGlobalShaderMap());

				// Issue this frame's occlusion queries (occlusion queries from last frame may still be in flight)
				typedef TMap<FSceneViewState::FProjectedShadowKey, FOcclusionQueryRHIRef> TShadowOcclusionQueryMap;
				TShadowOcclusionQueryMap& ShadowOcclusionQueryMap = ViewState->ShadowOcclusionQueryMap;

				// Clear primitives which haven't been visible recently out of the occlusion history, and reset old pending occlusion queries.
				ViewState->TrimOcclusionHistory(ViewFamily.CurrentRealTime - GEngine->PrimitiveProbablyVisibleTime,ViewFamily.CurrentRealTime,FrameNumber);

				// Depth tests, no depth writes, no color writes, opaque, solid rasterization wo/ backface culling.
				RHISetDepthState(TStaticDepthState<FALSE,CF_LessEqual>::GetRHI());
				RHISetColorWriteEnable(FALSE);
				// We only need to render the front-faces of the culling geometry (this halves the amount of pixels we touch)
				RHISetRasterizerState(
					View.bReverseCulling ? TStaticRasterizerState<FM_Solid,CM_CCW>::GetRHI() : TStaticRasterizerState<FM_Solid,CM_CW>::GetRHI()); 
				RHISetBlendState(TStaticBlendState<>::GetRHI());

				SetGlobalBoundShaderState(OcclusionTestBoundShaderState,GOcclusionQueryPosOnlyVertexDeclaration.VertexDeclarationRHI,*VertexShader,NULL,sizeof(FVector));

				// Give back all these occlusion queries to the pool.
				for ( TShadowOcclusionQueryMap::TIterator QueryIt(ShadowOcclusionQueryMap); QueryIt; ++QueryIt )
				{
					//FOcclusionQueryRHIParamRef Query = QueryIt.Value();
					//check( Query.GetRefCount() == 1 );
					ViewState->OcclusionQueryPool.ReleaseQuery( QueryIt.Value() );
				}
				ShadowOcclusionQueryMap.Reset();

				for(TSparseArray<FLightSceneInfoCompact>::TConstIterator LightIt(Scene->Lights);LightIt;++LightIt)
				{
					const FVisibleLightInfo& VisibleLightInfo = VisibleLightInfos(LightIt.GetIndex());

					for(INT ShadowIndex = 0;ShadowIndex < VisibleLightInfo.AllProjectedShadows.Num();ShadowIndex++)
					{
						const FProjectedShadowInfo& ProjectedShadowInfo = *VisibleLightInfo.AllProjectedShadows(ShadowIndex);

						if (ProjectedShadowInfo.DependentView && ProjectedShadowInfo.DependentView != &View)
						{
							continue;
						}

						if (ShouldRenderOnePassPointLightShadow(&ProjectedShadowInfo))
						{
							// Query one pass point light shadows separately because they don't have a shadow frustum, they have a bounding sphere instead.
							const FSphere LightBounds = ProjectedShadowInfo.LightSceneInfo->GetBoundingSphere();
							const UBOOL bCameraInsideLightGeometry = ((FVector)View.ViewOrigin - LightBounds.Center).SizeSquared() < Square(LightBounds.W * 1.05f + View.NearClippingDistance * 2.0f);
							if (!bCameraInsideLightGeometry)
							{
								const FOcclusionQueryRHIRef ShadowOcclusionQuery = ViewState->OcclusionQueryPool.AllocateQuery();
								RHIBeginOcclusionQuery(ShadowOcclusionQuery);

								FSceneViewState::FProjectedShadowKey Key(
									ProjectedShadowInfo.ParentSceneInfo ? 
									ProjectedShadowInfo.ParentSceneInfo->Component :
									NULL,
									ProjectedShadowInfo.LightSceneInfo->LightComponent,
									ProjectedShadowInfo.SplitIndex
									);
								checkSlow(ShadowOcclusionQueryMap.Find(Key) == NULL);
								ShadowOcclusionQueryMap.Set(Key, ShadowOcclusionQuery);

								DrawStencilingSphere(LightBounds, View.PreViewTranslation);

								RHIEndOcclusionQuery(ShadowOcclusionQuery);
							}
						}
						// Don't query preshadows, since they are culled if their subject is occluded.
						// Also don't query if any subjects are visible because the shadow frustum will be definitely unoccluded
						else if (!ProjectedShadowInfo.IsWholeSceneDominantShadow() && !ProjectedShadowInfo.bPreShadow && !ProjectedShadowInfo.SubjectsVisible(View))
						{
							// The shadow transforms and view transforms are relative to different origins, so the world coordinates need to
							// be translated.
							const FVector4 PreShadowToPreViewTranslation(View.PreViewTranslation - ProjectedShadowInfo.PreShadowTranslation,0);

							// If the shadow frustum is farther from the view origin than the near clipping plane,
							// it can't intersect the near clipping plane.
							const UBOOL bIntersectsNearClippingPlane = ProjectedShadowInfo.ReceiverFrustum.IntersectSphere(
								-View.PreViewTranslation + ProjectedShadowInfo.PreShadowTranslation,
								View.NearClippingDistance * appSqrt(3.0f)
								);
							if( !bIntersectsNearClippingPlane )
							{
								// Allocate an occlusion query for the primitive from the occlusion query pool.
								const FOcclusionQueryRHIRef ShadowOcclusionQuery = ViewState->OcclusionQueryPool.AllocateQuery();

								// Draw the primitive's bounding box, using the occlusion query.
								RHIBeginOcclusionQuery(ShadowOcclusionQuery);

								void* VerticesPtr;
								void* IndicesPtr;
								// preallocate memory to fill out with vertices and indices
								RHIBeginDrawIndexedPrimitiveUP( PT_TriangleList, 12, 8, sizeof(FVector), VerticesPtr, 0, NUM_CUBE_VERTICES, sizeof(WORD), IndicesPtr);
								FVector* Vertices = (FVector*)VerticesPtr;
								WORD* Indices = (WORD*)IndicesPtr;

								// Generate vertices for the shadow's frustum.
								for(UINT Z = 0;Z < 2;Z++)
								{
									for(UINT Y = 0;Y < 2;Y++)
									{
										for(UINT X = 0;X < 2;X++)
										{
											const FVector4 UnprojectedVertex = ProjectedShadowInfo.InvReceiverMatrix.TransformFVector4(
												FVector4(
												(X ? -1.0f : 1.0f),
												(Y ? -1.0f : 1.0f),
												(Z ?  1.0f : 0.0f),
												1.0f
												)
												);
											const FVector ProjectedVertex = UnprojectedVertex / UnprojectedVertex.W + PreShadowToPreViewTranslation;
											Vertices[GetCubeVertexIndex(X,Y,Z)] = ProjectedVertex;
										}
									}
								}

								// we just copy the indices right in
								appMemcpy(Indices, GCubeIndices, sizeof(GCubeIndices));

								FSceneViewState::FProjectedShadowKey Key(
									ProjectedShadowInfo.ParentSceneInfo ? 
									ProjectedShadowInfo.ParentSceneInfo->Component :
									NULL,
									ProjectedShadowInfo.LightSceneInfo->LightComponent,
									ProjectedShadowInfo.SplitIndex
									);
								checkSlow(ShadowOcclusionQueryMap.Find(Key) == NULL);
								ShadowOcclusionQueryMap.Set(Key, ShadowOcclusionQuery);

								RHIEndDrawIndexedPrimitiveUP();
								RHIEndOcclusionQuery(ShadowOcclusionQuery);
							}
						}
					}
				}
			}
    
		    // Don't do primitive occlusion if we have a view parent or are frozen.
    #if !FINAL_RELEASE
		    if ( !ViewState->HasViewParent() && !ViewState->bIsFrozen )
    #endif
		    {
				{
					SCOPED_DRAW_EVENT(EventIndividualQueries)(DEC_SCENE_ITEMS,TEXT("IndividualQueries"));
					View.IndividualOcclusionQueries.Flush();
				}
				{
					SCOPED_DRAW_EVENT(EventGroupedQueries)(DEC_SCENE_ITEMS,TEXT("GroupedQueries"));
					View.GroupedOcclusionQueries.Flush();
				}
		    }
		    
		    // Reenable color writes.
		    RHISetColorWriteEnable(TRUE);
    
		    // Kick the commands.
		    RHIKickCommandBuffer();
	    }
    }

	if (bUseDownsampledDepth)
	{
		// Restore default render target
		GSceneRenderTargets.BeginRenderingSceneColor();
	}
}
