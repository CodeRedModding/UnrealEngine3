/*=============================================================================
	FluidSurface.h: Class definitions for fluid surfaces.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef FLUIDSURFACE_H
#define FLUIDSURFACE_H


#include "FluidSurfaceGPUSimulation.h"


class UFluidSurfaceComponent;
class FFluidSimulation;


/** How many quads a CPU grid cell should be split up into by default, along each side. (Xbox-only) */
#define GPUTESSELLATION 4

/** Set to 1 or 0, depending on if you want to use clampmaps or not. */
#define DISABLE_CLAMPMAP 1


enum EFluidSurfaceStats
{
	STAT_FluidSimulation = STAT_FluidsFirstStat,
	STAT_FluidTessellation,
	STAT_FluidRenderthreadBlocked,
	STAT_FluidCPUMemory,
	STAT_FluidGPUMemory,
	STAT_FluidSurfaceComponentTickTime,
	STAT_FluidInfluenceComponentTickTime,
};


/**
 * Fluid vertex
 */
struct FFluidVertex
{
	FLOAT			Height;
	FVector2D		UV;
	FVector2D		HeightDelta;	// Height difference along the X- and Y-axis
};


/**
 * Fluid vertex buffer
 */
class FFluidVertexBuffer : public FVertexBuffer
{
public:
	enum EBufferType
	{
		BT_Simulation,
		BT_Border,
		BT_Quad,
	};

	FFluidVertexBuffer( );
	void			Setup( FFluidSimulation* InOwner, DWORD InMaxNumVertices, EBufferType InBufferType, INT NumQuadsX=0, INT NumQuadsY=0 );
	FFluidVertex*	Lock();
	void			Unlock();
	UBOOL			IsLocked();
	UBOOL			IsBusy();
	DWORD			GetMaxNumVertices() const;
	INT				GetNumQuadsX() const;
	INT				GetNumQuadsY() const;

	// FRenderResource interface.
	virtual void	InitDynamicRHI();
	virtual void	ReleaseDynamicRHI();

protected:
	FFluidSimulation* Owner;
	DWORD			MaxNumVertices;
	UBOOL			bIsLocked;
	EBufferType		BufferType;
	UBOOL			bBorderGeometry;
	INT				NumQuadsX;
	INT				NumQuadsY;
};


/**
 *	Vertex factory for fluid surfaces, using a vertex buffer.
 */
class FFluidVertexFactory : public FVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FFluidVertexFactory);
public:
	FFluidVertexFactory();
	virtual ~FFluidVertexFactory();

	inline const FVector4&	GetGridSize();
	const FFluidSimulation*	GetSimulation();
#if XBOX
	inline const FVector4&	GetTessellationParameters();
	inline FTexture2DRHIRef& GetHeightmapTexture();
#endif
	void					InitResources( const FFluidVertexBuffer& VertexBuffer, FFluidSimulation* FluidSimulation );

	static FVertexFactoryShaderParameters* ConstructShaderParameters(EShaderFrequency ShaderFrequency);

	// FRenderResource interface.
	virtual void			InitRHI();
	virtual FString			GetFriendlyName() const;

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static UBOOL			ShouldCache(EShaderPlatform Platform, const class FMaterial* Material, const class FShaderType* ShaderType);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void				ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);

	/** Returns whether the vertex shader should generate vertices (TRUE) or if it should use a vertex buffer (FALSE). */
	UBOOL					UseGPUTessellation();

protected:
	/** Whether the vertex shader should generate vertices (TRUE) or if it should use a vertex buffer (FALSE). */
	UBOOL					bUseGPUTessellation;

private:
	/** The stream to read the vertex height from. */
	FVertexStreamComponent	Height;

	/** The streams to read the texture coordinates from. */
	FVertexStreamComponent	TexCoord;

	/** The streams to read the tangent basis from. */
	FVertexStreamComponent	HeightDelta;

	/** Owner FluidSimulation. */
	FFluidSimulation*		FluidSimulation;
};

/**
 *	Vertex factory for fluid surfaces, letting the GPU generate the fluid vertices.
 */
class FFluidTessellationVertexFactory : public FFluidVertexFactory
{
	DECLARE_VERTEX_FACTORY_TYPE(FFluidTessellationVertexFactory);
public:
	FFluidTessellationVertexFactory();

	static void				ModifyCompilationEnvironment(EShaderPlatform Platform, FShaderCompilerEnvironment& OutEnvironment);
};

/**
 * FluidSimulation - the main fluid class that keeps all parts together.
 */
class FFluidSimulation : public FQueuedWork
{
public:
	FFluidSimulation( UFluidSurfaceComponent* InComponent, UBOOL bActive, INT InSimulationQuadsX, INT InSimulationQuadsY, FLOAT InCellWidth, FLOAT InCellHeight, INT InTotalNumCellsX, INT InTotalNumCellsY );
	virtual ~FFluidSimulation();

	// FQueuedWork API
	virtual void		DoWork( );
	virtual void		Abandon( );
	virtual void		DoThreadedWork( );

	// Called from the Gamethread
	void				ReleaseResources( UBOOL bBlockOnRelease );
	UBOOL				IsReleased( );
	FLOAT				GetWidth( ) const;
	FLOAT				GetHeight( ) const;
	INT					GetNumCellsX( ) const;
	INT					GetNumCellsY( ) const;
	const FIntPoint&	GetTotalSize( ) const;
	void				GameThreadTick( FLOAT DeltaTime );
	UBOOL				LineCheck( FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags );
	UBOOL				PointCheck( FCheckResult& Result, const FVector& Location, const FVector& Extent, DWORD TraceFlags );
	void				SetExtents( const FMatrix& LocalToWorld, const FPlane& Plane, const FPlane* Edges );
	UBOOL				IsActive( ) const;
	const FFluidGPUResource* GetGPUResource() const { return &DetailGPUResource; }

	// Called from the Renderthread
	void				AddForce( const FVector& LocalPos, FLOAT Strength, FLOAT LocalRadius, UBOOL bImpulse=FALSE );
	void				SetDetailPosition(FVector LocalPos);
	void				SetSimulationPosition(FVector LocalPos);
	void				RenderThreadTick( FLOAT DeltaTime );
	void				UpdateBorderGeometry( FFluidVertex* Vertices );

	const FMatrix&		GetWorldToLocal( ) const;
	const FVector4&		GetGridSize( ) const;
	const FIntPoint&	GetSimulationPosition( ) const;
#if XBOX
	const FVector4&		GetTessellationParameters() const;
	const FVector4&		GetTessellationFactors1() const;
	const FVector4&		GetTessellationFactors2() const;
	const FVector4&		GetTexcoordScaleBias() const;
#endif

	// Called from any thread
	void				BlockOnSimulation();
	void				GetSimulationRect( FVector2D& TopLeft, FVector2D& LowerRight );			// Returns the rectangle of the simulation grid, in fluid local space.
	void				GetDetailRect( FVector2D& TopLeft, FVector2D& LowerRight );				// Returns the rectangle of the detailmap, in fluid local space.
	UBOOL				IsWithinSimulationGrid( const FVector& LocalPos, FLOAT Radius );
	UBOOL				IsWithinDetailGrid( const FVector& LocalPos, FLOAT Radius );

protected:
	// Called from the simulation thread
	UBOOL			IsClampedVertex( INT X, INT Y );
	void			ApplyForce( const FVector& LocalPos, FLOAT Strength, FLOAT LocalRadius );
	void			Simulate( FLOAT DeltaTime );
	UBOOL			UpdateRenderData();

	// Called from the Gamethread
	void			InitResources();
	/**
	 * Allocates texture memory on the gamethread. Will try to stream out high-res mip-levels if there's not enough room.
	 * @return	A new FTexture2DResourceMem object, representing the allocated memory, or NULL if the allocation failed.
	 */
	FTexture2DResourceMem* CreateTextureResourceMemory();

	// Called from the Renderthread
	void			RenderThreadInitResources( INT BufferIndex, FTexture2DResourceMem* ResourceMem );
	void			InitIndexBufferX();
	void			InitIndexBufferY();
	void			InitFlatIndexBuffer();
	void			UpdateShaderParameters( INT OctantID );
	static INT		ClassifyOctant( const FVector& LocalDirection );
	UBOOL			ShouldSimulate();
	void			LockResources();
	void			UnlockResources();

	/* 
	 * Indicates whether culling should be reversed when rendering the fluid surface. 
	 * These are set on the simulation thread and read on the render thread when rendering the fluid surface.
	 */
	UBOOL					bReverseCulling[2];

	/** 
	 * If TRUE, packing will be done from front-to-back relative to the view direction.  
	 * If FALSE, packing will be done from back-to-front to avoid artifacts with translucency.
	 */
	UBOOL					bOpaqueMaterial;

	/* 
	 * Indicates whether the YFirstIndexBuffer should be used for rendering, or the XFirstIndexBuffer.
	 * These are set on the simulation thread and read on the render thread when rendering the fluid surface.
	 */
	UBOOL					bUseYFirstIndexBuffer[2];

	/* 
	 * Stores the view direction that the fluid surface was last rendered with, 
	 * used for packing the render data from back to front to behave correctly with translucency.
	 * These are set on the render thread and read from the simulation thread.
	 */
	FVector					LastViewDirection[2];

	FMatrix					FluidWorldToLocal;		// World-to-local matrix for the whole fluid
	FPlane					Plane;					// Representing the fluid plane in world space.
	FPlane					Edges[4];				// Outward planes for the four edges of the fluid, in world space.
	FLOAT*					HeightMap[2];			// Heights for each vertex. History of two simulation steps, plus one scratch.
	INT						HeightMapMemSize;		// Number of bytes in each height map.
	INT						CurrentHeightMap;		// Indexes the current height map, owned by the simulation thread.
	INT						NumCellsX;				// Number of simulation grid cells along the X-axis
	INT						NumCellsY;				// Number of simulation grid cells along the Y-axis
	INT						NumLowResCellsPerSideX; // Number of low res cells in each of the 4 border patches along the X axis
	INT						NumLowResCellsPerSideY; // Number of low res cells in each of the 4 border patches along the Y axis
	FLOAT					CellWidth;				// Width of a grid cell
	FLOAT					CellHeight;				// Height of a grid cell (normally the same as width)
	FLOAT					GridWidth;				// Width of the simulation grid, in localspace units
	FLOAT					GridHeight;				// Height of the simulation grid, in localspace units
	FLOAT					UpdateRate;
	FLOAT					TimeRollover;
	TArray<FVector>			DebugPositions;
	TArray<FVector>			DebugNormals;
	UFluidSurfaceComponent* Component;
	INT						NumVertices;			// Number of generated vertices this frame
	INT						NumIndices;				// Number of generated indices this frame
	UBOOL					bEnableCPUSimulation;	// Copy of FluidSurfaceComponent::EnableSimulation
	UBOOL					bEnableGPUSimulation;	// Copy of FluidSurfaceComponent::EnableDetail

	FIntPoint				PendingSimulationPos;	// Pending upper-left corner of the simulation grid (in cells), as queued up from the gamethread
	FIntPoint				SimulationPos[2];		// Upper-left corner of the simulation grid, for each of the two heightmaps
	FIntPoint				TotalSize;				// Total size of the fluid, including both simulation part and the surrounding flat border (in cells)
	FLOAT					TotalWidth;				// Total width of the fluid, including both simulation part and the surrounding flat border (in world-space units)
	FLOAT					TotalHeight;			// Total height of the fluid, including both simulation part and the surrounding flat border (in world-space units)

	// Used by the Renderthread
	FVector4				GridSize;				// Vertexshader parameter: X=GridWidth/2, Y=GridHeight/2, Z=1/NumCellsX, W=1/NumCellsY
	UBOOL					bResourcesLocked;		// Whether the GPU resources are locked or not
	FLOAT					SimulationActivity;		// Total absolute sum of the two latest heightmap simulations
	UBOOL					bShowSimulation;		// Whether to render the simulation geometry or just a flat quad.

	// Read by all threads
	const INT				GridPitch;				// Number of FLOATs between each row in the simulation heightmap

	// Used by the worker (simulation) thread
	UBOOL					bWorkerThreadUpdateOnly;	// Whether the simulation thread should only retry UpdateRenderData()
	FLOAT					PrevSum;				// Total absolute sum of the previous heightmap simulation
	FLOAT					CurrentSum;				// Total absolute sum of the latest heightmap simulation
	UBOOL					bSimulationDirty;

#if XBOX
	// Xbox-specific variables
	FTexture2DRHIRef		HeightMapTextures[2];	// These textures use HeightMap[] as texture data.
	void*					TextureData;			// Currently locked texture data.
	UINT					TextureStride;
	INT						NumTessQuadsX;			// Number of tessellation quads along X-axis
	INT						NumTessQuadsY;			// Number of tessellation quads along Y-axis
	FLOAT					TessellationLevel;		// Number of sub-quads within each tessellation quad
	FVector4				TessellationParameters;	// Vertexshader parameter: X=TessellationLevel, Y=NumQuadsX, Z=NumQuadsY
	FVector4				TessellationFactors1;	// Vertexshader parameter: X=TessellationLevel, Y=NumQuadsX, Z=NumQuadsY
	FVector4				TessellationFactors2;	// Vertexshader parameter: X=TessellationLevel, Y=NumQuadsX, Z=NumQuadsY
	FVector4				TexcoordScaleBias;		// Vertexshader parameter: Converts from heightmap UV to fluid UV
	UBOOL					bReverseCullingXbox;	// Whether the triangle winding order is reversed due to back-to-front sorting
#endif

	TArray<FFluidForce>		FluidForces[2];
	FFluidVertex*			Vertices;
	FFluidVertex*			BorderVertices;
	FLOAT					DeltaTime;

    FRenderCommandFence					ReleaseResourcesFence;
	FFluidTessellationVertexFactory		VertexFactories[2];
	FFluidVertexFactory					FlatVertexFactories[2];
	FFluidVertexFactory					FlatQuadVertexFactory;
	FFluidVertexBuffer		VertexBuffers[2];
	FFluidVertexBuffer		FlatVertexBuffers[2];	// Vertex buffer for the surrounding flat area (dynamic double-buffered)
	FFluidVertexBuffer		FlatQuadVertexBuffer;	// Vertex buffer for rendering the simulation grid as a simple flat quad
	FRawGPUIndexBuffer		FlatIndexBuffer;		// Index buffer for the surrounding flat area (static).
	FRawGPUIndexBuffer		YFirstIndexBuffer;		// Index buffer packed iterating over Y first, then X
	FRawGPUIndexBuffer		XFirstIndexBuffer;		// Index buffer packed iterating over X first, then Y
	FRawGPUIndexBuffer		FlatQuadIndexBuffer;
	FFluidGPUResource		DetailGPUResource;

	/** 
	 * Used to double buffer data for thread-safe interactions between the rendering thread and the simulation thread.
	 * The simulation thread always writes to [SimulationIndex] and the rendering thread reads from [1 - SimulationIndex].
	 * SimulationIndex is flipped in RenderThreadTick(), after the rendering thread has blocked on the simulation thread.
	 */
	INT						SimulationIndex;

	/** Stores the value of SimulationPos at the time that rendering data (such as FlatVertexBuffers) were generated by the simulation thread.  Indexed by SimulationIndex. */
	FIntPoint				RenderDataPosition[2];

	volatile INT			SimulationRefCount;
	volatile INT			bSimulationBusy;

#if STATS
	DWORD					STAT_FluidSimulationValue;
	DWORD					STAT_FluidTessellationValue;
	DWORD					STAT_FluidSimulationCount;
#endif

private:
	void					CalculateNormal(const FLOAT* Height, INT X, INT Y, FLOAT HeightScale, FVector2D& HeightDelta);

	friend class FFluidVertexFactory;
	friend class FFluidSurfaceSceneProxy;
	friend class FFluidMaterialRenderProxy;
	friend class FFluidSurfaceStaticLightingMesh;
	friend class FFluidVertexBuffer;
};


/*=============================================================================
	FFluidSimulation inline functions
=============================================================================*/

FORCEINLINE FLOAT FFluidSimulation::GetWidth() const
{
	return GridWidth;
}

FORCEINLINE FLOAT FFluidSimulation::GetHeight() const
{
	return GridHeight;
}

FORCEINLINE INT FFluidSimulation::GetNumCellsX() const
{
	return NumCellsX;
}

FORCEINLINE INT FFluidSimulation::GetNumCellsY() const
{
	return NumCellsY;
}

FORCEINLINE const FIntPoint& FFluidSimulation::GetTotalSize( ) const
{
	return TotalSize;
}

FORCEINLINE const FIntPoint& FFluidSimulation::GetSimulationPosition( ) const
{
	return SimulationPos[ SimulationIndex ];
}

FORCEINLINE const FMatrix& FFluidSimulation::GetWorldToLocal( ) const
{
	return FluidWorldToLocal;
}

FORCEINLINE const FVector4& FFluidSimulation::GetGridSize() const
{
	return GridSize;
}

#if XBOX
FORCEINLINE const FVector4& FFluidSimulation::GetTessellationParameters() const
{
	return TessellationParameters;
}

FORCEINLINE const FVector4& FFluidSimulation::GetTessellationFactors1() const
{
	return TessellationFactors1;
}

FORCEINLINE const FVector4& FFluidSimulation::GetTessellationFactors2() const
{
	return TessellationFactors2;
}

FORCEINLINE const FVector4& FFluidSimulation::GetTexcoordScaleBias() const
{
	return TexcoordScaleBias;
}

#endif	//XBOX

//@DEBUG
enum ETrackEventType
{
	TRACK_TextureLocked,
	TRACK_TextureUnlocked,
	TRACK_BlockOnSimulation,
	TRACK_TextureStartUpdate,
	TRACK_TextureStopUpdate,
	TRACK_TextureRendered,
	TRACK_InsertFence,
	TRACK_RenderTick,
};
struct FTrackEvent
{
	ETrackEventType	Type;
	INT				ID;
	DOUBLE			Time;	// ms
};
void AddTrackEvent( ETrackEventType Type, INT ID );


#endif	//FLUIDSURFACE_H
