/*=============================================================================
	FluidSurface.cpp: Fluid surface simulation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineFluidClasses.h"
#include "FluidSurface.h"

// use 16-bit indices on platforms that cannot support 32-bit
#if DISALLOW_32BIT_INDICES
#define PLATFORM_INDEX_TYPE WORD
#else
#define PLATFORM_INDEX_TYPE DWORD
#endif

/** When this define is set, then fluids are simulated in a separated thread. */
#if PS3
	UBOOL GThreadedFluidSimulation = FALSE;
#else
	UBOOL GThreadedFluidSimulation = TRUE;
#endif

/** When this define is set, fluid simulation CPU performance will be dumped to a file on Xbox. */
#define ENABLE_CPUTRACE 0

/** Set to 1 or 0, depending on if you want Xbox to lock GPU resources on the worker thread (warning: there can be issues with the XDK). */
#if defined(_DEBUG)
	#define LOCK_ON_WORKTHREAD	1
#else
	#define LOCK_ON_WORKTHREAD	1
#endif

#if ENABLE_CPUTRACE
	DWORD GGameThreadCore;
	DWORD GRenderThreadCore;
	DWORD GSimulationCore;
#endif


DECLARE_STATS_GROUP(TEXT("Fluids"),STATGROUP_Fluids);
DECLARE_CYCLE_STAT(TEXT("Fluid Simulation"),STAT_FluidSimulation,STATGROUP_Fluids);
DECLARE_CYCLE_STAT(TEXT("Fluid Tessellation"),STAT_FluidTessellation,STATGROUP_Fluids);
DECLARE_CYCLE_STAT(TEXT("Fluid Renderthread Blocked"),STAT_FluidRenderthreadBlocked,STATGROUP_Fluids);
DECLARE_MEMORY_STAT(TEXT("Fluid CPU Memory"),STAT_FluidCPUMemory,STATGROUP_Fluids);
DECLARE_MEMORY_STAT(TEXT("Fluid GPU Memory"),STAT_FluidGPUMemory,STATGROUP_Fluids);
DECLARE_CYCLE_STAT(TEXT("FluidSurfaceComp Tick"),STAT_FluidSurfaceComponentTickTime,STATGROUP_Fluids);
DECLARE_CYCLE_STAT(TEXT("FluidInfluenceComp Tick"),STAT_FluidInfluenceComponentTickTime,STATGROUP_Fluids);



#define SURFHEIGHT(Heightmap, X, Y)			( Heightmap[ (Y)*GridPitch + (X) ] )


/*=============================================================================
	FFluidSimulation implementation
=============================================================================*/

FFluidSimulation::FFluidSimulation( UFluidSurfaceComponent* InComponent, UBOOL bActive, INT InSimulationQuadsX, INT InSimulationQuadsY, FLOAT InCellWidth, FLOAT InCellHeight, INT InTotalNumCellsX, INT InTotalNumCellsY )
:	CurrentHeightMap( 0 )
,	NumCellsX( InSimulationQuadsX )
,	NumCellsY( InSimulationQuadsY )
,	CellWidth( InCellWidth )
,	CellHeight( InCellHeight )
,	UpdateRate( InComponent->FluidUpdateRate )
,	TimeRollover( 0.0f )
,	Component(InComponent)
,	NumVertices( 0 )
,	NumIndices( 0 )
,	SimulationActivity( 0.0f )
,	bShowSimulation( FALSE )
,	GridPitch( Align(InSimulationQuadsX+1, GPUTESSELLATION) )
,	bWorkerThreadUpdateOnly( FALSE )
,	PrevSum( 0.0f )
,	CurrentSum( 0.0f )
,	bSimulationDirty( FALSE )
,	Vertices( NULL )
,	BorderVertices( NULL )
,	DeltaTime( 0.0f )
#if XBOX
,	YFirstIndexBuffer( 0, FALSE, sizeof(DWORD) )
,	XFirstIndexBuffer( 0, FALSE, sizeof(DWORD) )
#else
,	YFirstIndexBuffer( InSimulationQuadsX * InSimulationQuadsY * 6, FALSE, sizeof(PLATFORM_INDEX_TYPE) )
,	XFirstIndexBuffer( InSimulationQuadsX * InSimulationQuadsY * 6, FALSE, sizeof(PLATFORM_INDEX_TYPE) )
#endif
,	SimulationIndex( 0 )
,	SimulationRefCount( FALSE )
,	bSimulationBusy( FALSE )
{
	for (INT i = 0; i < 2; i++)
	{
		bReverseCulling[i] = FALSE;
		bUseYFirstIndexBuffer[i] = TRUE;
		LastViewDirection[i] = FVector(1, 0, 0);
	}

	UMaterial* FluidMaterial = InComponent->GetMaterial()->GetMaterial();
	bOpaqueMaterial = FALSE;
	if ( FluidMaterial && (FluidMaterial->BlendMode == BLEND_Opaque || FluidMaterial->BlendMode == BLEND_Masked || FluidMaterial->BlendMode == BLEND_SoftMasked || FluidMaterial->BlendMode == BLEND_DitheredTranslucent) )
	{
		 bOpaqueMaterial = TRUE;
	}

#if STATS
	STAT_FluidSimulationValue = 0;
	STAT_FluidTessellationValue = 0;
	STAT_FluidSimulationCount = 0;
#endif

#if XBOX
	HeightMapTextures[0] = NULL;
	HeightMapTextures[1] = NULL;
	TextureData = NULL;
	TextureStride = 0;
#endif

	bResourcesLocked = FALSE;

	bEnableCPUSimulation = bActive ? InComponent->EnableSimulation : FALSE;
	bEnableGPUSimulation = bActive ? InComponent->EnableDetail : FALSE;

	GridWidth	= NumCellsX * CellWidth;
	GridHeight	= NumCellsY * CellHeight;
	TotalSize.X	= InTotalNumCellsX;
	TotalSize.Y	= InTotalNumCellsY;
	TotalWidth	= InTotalNumCellsX * CellWidth;
	TotalHeight	= InTotalNumCellsY * CellHeight;
	DetailGPUResource.SetSize(Clamp<INT>(Component->DetailResolution, 16, 2048), Component->DetailSize);

	HeightMapMemSize = GridPitch * (NumCellsY + 1) * sizeof(FLOAT);
	NumVertices		= (NumCellsX + 1) * (NumCellsY + 1);
	NumIndices		= YFirstIndexBuffer.GetNumIndices();
	HeightMap[0]	= (FLOAT*) appMalloc( HeightMapMemSize );
	HeightMap[1]	= (FLOAT*) appMalloc( HeightMapMemSize );
	appMemzero( HeightMap[0], HeightMapMemSize );
	appMemzero( HeightMap[1], HeightMapMemSize );

	RenderDataPosition[0].X = RenderDataPosition[1].X = SimulationPos[0].X = SimulationPos[1].X = PendingSimulationPos.X = (TotalSize.X - NumCellsX) / 2;
	RenderDataPosition[0].Y = RenderDataPosition[1].Y = SimulationPos[0].Y = SimulationPos[1].Y = PendingSimulationPos.Y = (TotalSize.Y - NumCellsY) / 2;

	// Don't use the ClampMap if vertex simulation is disabled.
	if ( bEnableCPUSimulation == FALSE || DISABLE_CLAMPMAP )
	{
		Component->ClampMap.Empty(0);
	}
	else if (Component->ClampMap.Num() != NumVertices)
	{
		Component->ClampMap.Empty( NumVertices );
		Component->ClampMap.AddZeroed( NumVertices );
	}

	// Set up vertexshader parameters.
	UpdateShaderParameters( 0 );

	// Create buffers for the border geometry.
#if XBOX
	VertexBuffers[0].Setup( this, 1, FFluidVertexBuffer::BT_Simulation );
	VertexBuffers[1].Setup( this, 1, FFluidVertexBuffer::BT_Simulation );
#else
	VertexBuffers[0].Setup( this, NumVertices, FFluidVertexBuffer::BT_Simulation );
	VertexBuffers[1].Setup( this, NumVertices, FFluidVertexBuffer::BT_Simulation );
#endif

	// Create buffers for the geometry in the deactivated state.
	INT NumLowResCellsX = Max<INT>(appTrunc(TotalWidth / InComponent->GridSpacingLowRes), 1);
	INT NumLowResCellsY = Max<INT>(appTrunc(TotalHeight / InComponent->GridSpacingLowRes), 1);
	INT NumLowResVertices = (NumLowResCellsX + 1) * (NumLowResCellsY + 1);
	INT NumLowResIndices = NumLowResCellsX * NumLowResCellsY * 6;
	FlatQuadVertexBuffer.Setup( this, NumLowResVertices, FFluidVertexBuffer::BT_Quad, NumLowResCellsX, NumLowResCellsY );
	FlatQuadIndexBuffer.Setup( NumLowResIndices, FALSE );

	// Give each of the 4 border patches 1/4 of the low res cells
	NumLowResCellsPerSideX = Max<INT>(NumLowResCellsX / 2, 1);
	NumLowResCellsPerSideY = Max<INT>(NumLowResCellsY / 2, 1);
	// Total number of vertices in the border vertex buffer
	INT NumLowResFlatVertices = (NumLowResCellsPerSideX + 1) * (NumLowResCellsPerSideY + 1) * 4;
	FlatVertexBuffers[0].Setup( this, NumLowResFlatVertices, FFluidVertexBuffer::BT_Border );
	FlatVertexBuffers[1].Setup( this, NumLowResFlatVertices, FFluidVertexBuffer::BT_Border );
	// Setup the border index buffer
	FlatIndexBuffer.Setup( 4 * NumLowResCellsPerSideX * NumLowResCellsPerSideY * 6, FALSE );

	// Initialize all geometry buffers.
	InitResources();

	// Heightmaps
	INC_MEMORY_STAT_BY(STAT_FluidCPUMemory, 2*HeightMapMemSize);
	// Clampmap
	INC_MEMORY_STAT_BY(STAT_FluidCPUMemory, Component->ClampMap.Num()*sizeof(BYTE));
#if XBOX
	// Heightmap textures
	INC_MEMORY_STAT_BY(STAT_FluidGPUMemory, 2*NumVertices*sizeof(FFloat16)*4);
#else
	// Vertices
	INC_MEMORY_STAT_BY(STAT_FluidGPUMemory, 2*NumVertices*sizeof(FFluidVertex));
	// Indices
	INC_MEMORY_STAT_BY(STAT_FluidGPUMemory, NumIndices*sizeof(DWORD));
#endif
	// LowRes flat surface
	INC_MEMORY_STAT_BY(STAT_FluidGPUMemory, NumLowResVertices*sizeof(FFluidVertex) + NumLowResIndices*sizeof(DWORD));
	if ( bEnableGPUSimulation )
	{
		INC_MEMORY_STAT_BY(STAT_FluidGPUMemory, DetailGPUResource.GetRenderTargetMemorySize());
	}
}

FFluidSimulation::~FFluidSimulation()
{
	// Heightmaps
	DEC_MEMORY_STAT_BY(STAT_FluidCPUMemory, 2*HeightMapMemSize);
	// Clampmap
	DEC_MEMORY_STAT_BY(STAT_FluidCPUMemory, Component->ClampMap.Num()*sizeof(BYTE));
#if XBOX
	// Heightmap textures
	DEC_MEMORY_STAT_BY(STAT_FluidGPUMemory, 2*NumVertices*sizeof(FFloat16)*4);
#else
	// Vertices
	DEC_MEMORY_STAT_BY(STAT_FluidGPUMemory, 2*NumVertices*sizeof(FFluidVertex));
	// Indices
	DEC_MEMORY_STAT_BY(STAT_FluidGPUMemory, NumIndices*sizeof(DWORD));
#endif
	// LowRes flat surface
	DEC_MEMORY_STAT_BY(STAT_FluidGPUMemory, FlatQuadVertexBuffer.GetMaxNumVertices()*sizeof(FFluidVertex) + FlatQuadIndexBuffer.GetNumIndices()*sizeof(DWORD));
	if ( bEnableGPUSimulation )
	{
		DEC_MEMORY_STAT_BY(STAT_FluidGPUMemory, DetailGPUResource.GetRenderTargetMemorySize());
	}

	check( !GThreadedFluidSimulation || bSimulationBusy == FALSE );
	check( SimulationRefCount == 0 );

	appFree( HeightMap[0] );
	appFree( HeightMap[1] );
}

UBOOL FFluidSimulation::IsClampedVertex( INT X, INT Y )
{
	return (bEnableCPUSimulation && !DISABLE_CLAMPMAP) ? Component->ClampMap( Y*(NumCellsX + 1) + X ) : FALSE;
}

void FFluidSimulation::ApplyForce( const FVector& InLocalPos, FLOAT Strength, FLOAT LocalRadius )
{
	if ( Component->bPause || bEnableCPUSimulation == FALSE )
	{
		return;
	}

	INT HeightMapIndex = 1 - CurrentHeightMap;
	FLOAT* Height = HeightMap[HeightMapIndex];

	FVector LocalPos( InLocalPos );
	LocalPos.X += TotalWidth*0.5f - SimulationPos[HeightMapIndex].X*CellWidth;
	LocalPos.Y += TotalHeight*0.5f - SimulationPos[HeightMapIndex].Y*CellHeight;
	const FLOAT LocalRadius2 = LocalRadius * LocalRadius;
	FLOAT ForceFactor = /*CellWidth * */CellWidth / PI;
	FLOAT Force = ForceFactor * Strength / (UpdateRate * LocalRadius2);

	// Apply the force to the coarse grid.
	{
		FIntRect ForceRect(
			Max<INT>( appFloor((LocalPos.X - LocalRadius) / CellWidth), 1 ),
			Max<INT>( appFloor((LocalPos.Y - LocalRadius) / CellHeight), 1 ),
			Min<INT>( appCeil((LocalPos.X + LocalRadius) / CellWidth), NumCellsX ),
			Min<INT>( appCeil((LocalPos.Y + LocalRadius) / CellHeight), NumCellsY )
			);
		FVector2D ForceOrigin( ForceRect.Min.X*CellWidth, ForceRect.Min.Y*CellHeight );
		for (INT Y = ForceRect.Min.Y; Y < ForceRect.Max.Y; ++Y)
		{
			FVector2D Pos( ForceOrigin );
			for (INT X = ForceRect.Min.X; X < ForceRect.Max.X; ++X)
			{
//				if ( !IsClampedVertex(X, Y) )
				{
					FLOAT R2 = Square(Pos.X - LocalPos.X) + Square(Pos.Y - LocalPos.Y);
					if (R2 < LocalRadius2)
					{
						FLOAT R = LocalRadius2 - R2;
// 						if ( DetailRect.Contains(DetailPos) )
// 						{
// 							R *= (1.0f - DetailStrengthScale);
// 						}

						SURFHEIGHT(Height, X, Y) += R * Force;
						bSimulationDirty = TRUE;
					}
				}
				Pos.X += CellWidth;
			}
			ForceOrigin.Y += CellHeight;
		}
	}
}

void FFluidSimulation::Simulate( FLOAT DeltaTime )
{
	FIntPoint PrevPosition		= SimulationPos[1 - CurrentHeightMap];
	FIntPoint PrevPrevPosition	= SimulationPos[CurrentHeightMap];
	FIntPoint NewPosition		= PendingSimulationPos;
	FIntPoint PrevPrevOffset	= NewPosition - PrevPrevPosition;
	FIntPoint PrevOffset		= NewPosition - PrevPosition;
	FLOAT* NewHeights			= NULL;

	// Calculate the area in the new placement that was also covered by the two older placements.
	FIntPoint SimulationSize( NumCellsX, NumCellsY );
	FIntRect PrevPrevRect( PrevPrevPosition, PrevPrevPosition + SimulationSize );
	FIntRect PrevRect( PrevPosition, PrevPosition + SimulationSize );
	FIntRect NewRect( NewPosition, NewPosition + SimulationSize );
	FIntRect Rect( PrevPrevRect );
	Rect.Clip( PrevRect );
	Rect.Clip( NewRect );
	Rect -= NewPosition;
	if ( Rect.Width() == 0 )
	{
		Rect.Min.X = Rect.Max.X = 0;
	}
	if ( Rect.Height() == 0 )
	{
		Rect.Min.Y = Rect.Max.Y = 0;
	}

	// Simulate the fluid heightmap.
	NewHeights				= HeightMap[ CurrentHeightMap ];
	FLOAT* PrevPrevHeights	= HeightMap[ CurrentHeightMap ];
	FLOAT* PrevHeights		= HeightMap[ 1 - CurrentHeightMap ];
	const FLOAT DampFactor	= Clamp<FLOAT>(1.0f - (Component->FluidDamping / 30.0f), 0.0f, 1.0f);

	// Setup the iteration order to ensure we don't write to NewHeights before we've read from PrevPrevHeights.
	INT StartX, EndX, StepX, StartY, EndY, StepY;
	if ( PrevPrevOffset.X >= 0 )
	{
		StartX		= Min<INT>(Rect.Min.X+1, Rect.Max.X);
		EndX		= Rect.Max.X;
		StepX		= 1;
	}
	else
	{
		StartX		= Max<INT>(Rect.Max.X-1, Rect.Min.X);
		EndX		= Rect.Min.X;
		StepX		= -1;
	}
	if ( PrevPrevOffset.Y >= 0 )
	{
		StartY		= Min<INT>(Rect.Min.Y+1, Rect.Max.Y);
		EndY		= Rect.Max.Y;
		StepY		= 1;
	}
	else
	{
		StartY		= Max<INT>(Rect.Max.Y-1, Rect.Min.Y);
		EndY		= Rect.Min.Y;
		StepY		= -1;
	}

	INT Y				= StartY;
	INT PrevY			= Y + PrevOffset.Y;
	INT PrevPrevY		= Y + PrevPrevOffset.Y;
	FLOAT TravelSpeed	= Component->FluidTravelSpeed;
	PrevSum				= CurrentSum;
	CurrentSum			= 0.0f;
	while ( Y != EndY )
	{
		INT X			= StartX;
		INT PrevX		= X + PrevOffset.X;
		INT PrevPrevX	= X + PrevPrevOffset.X;
		while ( X != EndX )
		{
//			if ( !IsClampedVertex(X, Y) )	// See if we are simulating this vertex.
			{
				const FLOAT Neighbors =
					SURFHEIGHT(PrevHeights, PrevX-1, PrevY) + 
					SURFHEIGHT(PrevHeights, PrevX+1, PrevY) +
					SURFHEIGHT(PrevHeights, PrevX, PrevY-1) + 
					SURFHEIGHT(PrevHeights, PrevX, PrevY+1);
				const FLOAT Current		= SURFHEIGHT(PrevHeights, PrevX, PrevY);
				const FLOAT Current4	= 4.0f * Current;
				const FLOAT Curve		= Current4 + TravelSpeed * (Neighbors - Current4);
				FLOAT NewHeight			= Curve * 0.5f - SURFHEIGHT(PrevPrevHeights, PrevPrevX, PrevPrevY);
				NewHeight				*= DampFactor;
				CurrentSum				+= Abs<FLOAT>(NewHeight);
				SURFHEIGHT(NewHeights, X, Y) = NewHeight;
			}
			X			+= StepX;
			PrevX		+= StepX;
			PrevPrevX	+= StepX;
		}
		Y				+= StepY;
		PrevY			+= StepY;
		PrevPrevY		+= StepY;
	}

	SimulationPos[ CurrentHeightMap ] = NewPosition;
	bSimulationDirty = FALSE;

	// Zero out any regions that weren't touched by the simulation.
	for ( INT Y=1; Y <= Rect.Min.Y; ++Y )
	{
		for ( INT X=1; X <= NumCellsX; ++X )
		{
			SURFHEIGHT(NewHeights, X, Y) = 0.0f;
		}
	}
	if ( Rect.Min.X > 0 || Rect.Max.X < NumCellsX )
	{
		for ( INT Y=Rect.Min.Y+1; Y < Rect.Max.Y; ++Y )
		{
			for ( INT X=1; X <= Rect.Min.X; ++X )
			{
				SURFHEIGHT(NewHeights, X, Y) = 0.0f;
			}
			for ( INT X=Rect.Max.X; X < NumCellsX; ++X )
			{
				SURFHEIGHT(NewHeights, X, Y) = 0.0f;
			}
		}
	}
	for ( INT Y=Rect.Max.Y; Y < NumCellsY; ++Y )
	{
		for ( INT X=1; X <= NumCellsX; ++X )
		{
			SURFHEIGHT(NewHeights, X, Y) = 0.0f;
		}
	}
}

void FFluidSimulation::DoWork()
{
#if XBOX && ENABLE_CPUTRACE
	GSimulationCore			= GetCurrentProcessorNumber();
	static DOUBLE StartTime	= appSeconds();
	static UBOOL HasTraced	= FALSE;
	DOUBLE CurrentTime		= appSeconds();
	UBOOL DoTrace			= FALSE;
	if ( !HasTraced && (CurrentTime-StartTime) > 10.0 && (TimeRollover + DeltaTime) > (1.0f/UpdateRate) )
	{
		DoTrace = TRUE;
		HasTraced = TRUE;
	}
	if ( DoTrace )
	{
		XTraceSetBufferSize( 40 * 1024 * 1024 );
		XTraceStartRecording( "GAME:\\trace-fluidthread.pix2" );
	}
#endif

	UBOOL bUpdateSuccessful = TRUE;
	if ( bWorkerThreadUpdateOnly == FALSE )
	{
#if STATS
		DWORD Timer = appCycles();
#endif
		// Simulate.
		DOUBLE StartTime = appSeconds();
#if _DEBUG
		DOUBLE SimulationTimeLimit = GEngine->FluidSimulationTimeLimit * 2.0 / 1000.0;
#else
		DOUBLE SimulationTimeLimit = GEngine->FluidSimulationTimeLimit / 1000.0;
#endif
		DWORD NumIterations = 0;
		if ( !Component->bPause && bEnableCPUSimulation )
		{
			// Apply impulse (instantaneous) forces.
			for ( INT ForceIndex=0; ForceIndex < FluidForces[SimulationIndex].Num(); ++ForceIndex )
			{
				FFluidForce& Force = FluidForces[SimulationIndex](ForceIndex);
				if ( Force.bImpulse )
				{
					ApplyForce( Force.LocalPos, Force.Strength, Force.Radius );
				}
			}

			FLOAT TimeStep = 1.0f / UpdateRate;
			TimeRollover += DeltaTime;
			NumIterations = appTrunc( TimeRollover / TimeStep );
			TimeRollover -= NumIterations * TimeStep;
			for ( DWORD Iteration=0; Iteration < NumIterations; ++Iteration )
			{
				for ( INT ForceIndex=0; ForceIndex < FluidForces[SimulationIndex].Num(); ++ForceIndex )
				{
					FFluidForce& Force = FluidForces[SimulationIndex](ForceIndex);
					if ( !Force.bImpulse )
					{
						// Apply continuous forces.
						ApplyForce( Force.LocalPos, Force.Strength, Force.Radius );
					}
				}
				Simulate( TimeStep );
				CurrentHeightMap = 1 - CurrentHeightMap;

				// Limit the Simulation time to avoid spiraling into worsening framerates.
				DOUBLE TimeSpent = appSeconds()-StartTime;
				if ( TimeSpent > SimulationTimeLimit )
				{
					NumIterations = Iteration + 1;
					break;
				}
			}
		}

#if STATS
		STAT_FluidSimulationCount += NumIterations;
		STAT_FluidSimulationValue += appCycles() - Timer;
#endif
	}

	{
#if STATS
		DWORD Timer = appCycles();
#endif
		if ( bEnableCPUSimulation )
		{
			bUpdateSuccessful = UpdateRenderData();
		}
#if STATS
		STAT_FluidTessellationValue += appCycles() - Timer;
#endif
	}
#if XBOX && ENABLE_CPUTRACE
	if ( DoTrace )
	{
		XTraceStopRecording();
	}
#endif

	if ( bUpdateSuccessful )
	{
		// Setting this to FALSE makes Dispose() notify all other threads that we're done.
		bWorkerThreadUpdateOnly = FALSE;
	}
	else
	{
		bWorkerThreadUpdateOnly = TRUE;
		// Potentially yield the thread (letting other threads run on this core if necessary).
		appSleep( 0.0f );
		// Re-add this work to the end of work pool (letting other works execute until we try again).
		GThreadPool->AddQueuedWork( this );
	}
}

void FFluidSimulation::BlockOnSimulation()
{
	AddTrackEvent( TRACK_BlockOnSimulation, SimulationIndex );

	DWORD IdleStart = appCycles();

	//@TODO: Implement with an OS call
	while ( GThreadedFluidSimulation && bSimulationBusy )
	{
		appSleep( 0.0f );
	}

	GRenderThreadIdle += appCycles() - IdleStart;
}

void FFluidSimulation::Abandon()
{
}

/** Called from the worker thread requesting that we do work now. */
void FFluidSimulation::DoThreadedWork()
{
	DoWork();
	// Were we completely done with the simulation in DoWork? (Otherwise DoWork would've queued up another job.)
	if ( bWorkerThreadUpdateOnly == FALSE )
	{
		// Tell the other threads that the worker thread is done, using "release semantics".
		appMemoryBarrier();
		appInterlockedExchange( &bSimulationBusy, FALSE );
	}
}

FORCEINLINE void FFluidSimulation::CalculateNormal(const FLOAT* Height, INT X, INT Y, FLOAT HeightScale, FVector2D& HeightDelta)
{
	FLOAT H0 = SURFHEIGHT( Height, X-1, Y-1 );
	FLOAT H1 = SURFHEIGHT( Height, X,   Y-1 );
	FLOAT H2 = SURFHEIGHT( Height, X+1, Y-1 );
	FLOAT H3 = SURFHEIGHT( Height, X-1, Y   );
	FLOAT H5 = SURFHEIGHT( Height, X+1, Y   );
	FLOAT H6 = SURFHEIGHT( Height, X-1, Y+1 );
	FLOAT H7 = SURFHEIGHT( Height, X,   Y+1 );
	FLOAT H8 = SURFHEIGHT( Height, X+1, Y+1 );
	HeightDelta.X = (H8 - H0 + H2 - H6 + H5 - H3);
	HeightDelta.Y = (H8 - H0 + H6 - H2 + H7 - H1);
// 	FVector VX( 6.0f, 0.0f, HeightDelta.X*HeightScale );
// 	FVector VY( 0.0f, 6.0f, HeightDelta.Y*HeightScale );
// 	Normal = (VX ^ VY).UnsafeNormal();
// 	TangentX = VX.UnsafeNormal();
}

// FORCEINLINE void FFluidSimulation::CalculateNormalClamped(const FLOAT* Height, INT X, INT Y, FLOAT HeightScale, FVector2D& HeightDelta)
// {
// 	FLOAT H0 = (X > 0 && Y > 0) ?					SURFHEIGHT( Height, X-1, Y-1 ) : 0.0f;
// 	FLOAT H1 = (Y > 0) ?							SURFHEIGHT( Height, X,   Y-1 ) : 0.0f;
// 	FLOAT H2 = (X < NumCellsX && Y > 0) ?			SURFHEIGHT( Height, X+1, Y-1 ) : 0.0f;
// 	FLOAT H3 = (X > 0) ?							SURFHEIGHT( Height, X-1, Y   ) : 0.0f;
// 	FLOAT H5 = (X < NumCellsX) ?					SURFHEIGHT( Height, X+1, Y   ) : 0.0f;
// 	FLOAT H6 = (X > 0 && Y < NumCellsY) ?			SURFHEIGHT( Height, X-1, Y+1 ) : 0.0f;
// 	FLOAT H7 = (Y < NumCellsY) ?					SURFHEIGHT( Height, X,   Y+1 ) : 0.0f;
// 	FLOAT H8 = (X < NumCellsX && Y < NumCellsY) ?	SURFHEIGHT( Height, X+1, Y+1 ) : 0.0f;
// 	HeightDelta.X = (H8 - H0 + H2 - H6 + H5 - H3);
// 	HeightDelta.Y = (H8 - H0 + H6 - H2 + H7 - H1);
// }

#if XBOX
#pragma warning(push)
#pragma warning(disable : 4700) // warning C4700: uninitialized local variable 'ident' used									
FORCEINLINE void ConvertToHalf8( FFloat16* RESTRICT Dst, const FLOAT* RESTRICT Src )
{
	VectorRegister Float4;
	VectorRegister Half4;
	Float4	= VectorLoad( Src );
	Half4	= __vpkd3d( Half4, Float4, VPACK_FLOAT16_4, VPACK_64LO, 2 );
	Float4	= VectorLoad( (Src + 4) );
	Half4	= __vpkd3d( Half4, Float4, VPACK_FLOAT16_4, VPACK_64LO, 0 );
	VectorStoreAligned( Half4, Dst );
}
FORCEINLINE void ConvertToTextureData( FFloat16* RESTRICT Dst, const FLOAT* RESTRICT Src, INT Pitch )
{
	const FLOAT* RESTRICT RowPtr1 = Src;
	const FLOAT* RESTRICT RowPtr2 = Src + Pitch;
	const FLOAT* RESTRICT RowPtr3 = Src + 2*Pitch;
	VectorRegister Sum1, Sum2, Sum3, dhdx, dhdy, Height;

	// Read these from the heightmap memory at Src:
	// [h00 h01 h02 h03 h04 h05 h06 h07]
	// [h10 h11 h12 h13 h14 h15 h16 h17]
	// [h10 h11 h12 h13 h14 h15 h16 h17]
	//
	// Row1a = [h00 h01 h02 h03]	Row1b = [h01 h02 h03 h04]	Row1c = [h02 h03 h04 h05]
	// Row2a = [h10 h11 h12 h13]	Row2b = [h11 h12 h13 h14]	Row2c = [h12 h13 h14 h15]
	// Row3a = [h20 h21 h22 h23]	Row3b = [h21 h22 h23 h24]	Row3c = [h22 h23 h24 h25]
	//
	// Height = [h11 h12 h13 h14]
	// dhdx   = h22-h00 + h02-h20 + h12-h10  (similarly for the 3 next pixels)
	// dhdy   = h22-h00 + h20-h02 + h11-h01  (similarly for the 3 next pixels)

	VectorRegister Row1a	= VectorLoadAligned( RowPtr1 );
	VectorRegister Row1d	= VectorLoadAligned( RowPtr1+4 );
	VectorRegister Row2a	= VectorLoadAligned( RowPtr2 );
	VectorRegister Row2d	= VectorLoadAligned( RowPtr2+4 );
	VectorRegister Row3a	= VectorLoadAligned( RowPtr3 );
	VectorRegister Row3d	= VectorLoadAligned( RowPtr3+4 );
	VectorRegister Row1c	= __vsldoi( Row1a, Row1d, 8 );
	VectorRegister Row2c	= __vsldoi( Row2a, Row2d, 8 );
	VectorRegister Row3c	= __vsldoi( Row3a, Row3d, 8 );
	VectorRegister Row1b	= __vsldoi( Row1a, Row1d, 4 );
	VectorRegister Row2b	= __vsldoi( Row2a, Row2d, 4 );
	VectorRegister Row3b	= __vsldoi( Row3a, Row3d, 4 );

	Height	= Row2b;

	Sum1	= VectorSubtract( Row3b, Row1a );
	Sum2	= VectorSubtract( Row1b, Row3a );
	Sum3	= VectorSubtract( Row2b, Row2a );
	dhdx	= VectorAdd( Sum1, VectorAdd( Sum2, Sum3) );

	dhdy	= VectorAdd( VectorSubtract( Sum1, Sum2 ), VectorSubtract( Row3b, Row1b ) );

	// Pack Height, dhdx and dhdy into two fp16 registers:
	// Res1_fp16 = [Height.x dhdx.x dhdy.x 0.0f Height.y dhdx.y dhdy.y 0.0f]
	// Res2_fp16 = [Height.z dhdx.z dhdy.z 0.0f Height.w dhdx.w dhdy.w 0.0f]

	VectorRegister Height_fp16, dhdx_fp16, dhdy_fp16, temp1_fp16, temp2_fp16, Res1_fp16, Res2_fp16;
	Height_fp16	= __vpkd3d( Height_fp16, Height, VPACK_FLOAT16_4, VPACK_64LO, 2 );
	dhdx_fp16	= __vpkd3d( dhdx_fp16, dhdx, VPACK_FLOAT16_4, VPACK_64LO, 2 );
	dhdy_fp16	= __vpkd3d( dhdy_fp16, dhdy, VPACK_FLOAT16_4, VPACK_64LO, 2 );
	temp1_fp16	= __vmrghh( Height_fp16, dhdx_fp16 );
	temp2_fp16	= __vmrghh( dhdy_fp16, __vzero() );
	Res1_fp16	= __vmrghw( temp1_fp16, temp2_fp16 );
	Res2_fp16	= __vmrglw( temp1_fp16, temp2_fp16 );

	// Write two pixels (eight fp16 values)
	VectorStoreAligned( Res1_fp16, Dst );

	// Write two more pixels (eight fp16 values)
	VectorStoreAligned( Res2_fp16, Dst+8 );
}
#pragma warning(pop)
#endif

/** 
 * Updates the border geometry.
 * Border geometry needs to be roughly tessellated so that per-vertex fogging on translucency works as expected.
 */
void FFluidSimulation::UpdateBorderGeometry( FFluidVertex* Vertices )
{
	FFluidVertex Vertex;
	Vertex.Height		= 0.0f;
	Vertex.HeightDelta	= FVector2D( 0.0f, 0.0f );
	Vertex.UV			= FVector2D( 0.0f, 0.0f );

	// Calculate the UV-coords for the inner rectangle (which corresponds to the simulation grid).
	FVector2D UpperLeft, LowerRight;
	UpperLeft.X		= FLOAT(SimulationPos[CurrentHeightMap].X) / FLOAT(TotalSize.X);
	UpperLeft.Y		= FLOAT(SimulationPos[CurrentHeightMap].Y) / FLOAT(TotalSize.Y);
	LowerRight.X	= FLOAT(SimulationPos[CurrentHeightMap].X + NumCellsX) / FLOAT(TotalSize.X);
	LowerRight.Y	= FLOAT(SimulationPos[CurrentHeightMap].Y + NumCellsY ) / FLOAT(TotalSize.Y);

	// Add a small offset to make it overlap the simulation grid a bit, to hide the seam.
	// This really only works for single pass, mostly opaque materials.
	static FLOAT OverlapOffset = 0.2f;
	FVector2D UpperLeftOffset, LowerRightOffset;
	UpperLeftOffset.X	= FLOAT(SimulationPos[CurrentHeightMap].X + OverlapOffset) / FLOAT(TotalSize.X);
	UpperLeftOffset.Y	= FLOAT(SimulationPos[CurrentHeightMap].Y + OverlapOffset) / FLOAT(TotalSize.Y);
	LowerRightOffset.X	= FLOAT(SimulationPos[CurrentHeightMap].X + NumCellsX - OverlapOffset) / FLOAT(TotalSize.X);
	LowerRightOffset.Y	= FLOAT(SimulationPos[CurrentHeightMap].Y + NumCellsY - OverlapOffset) / FLOAT(TotalSize.Y);

	const INT NumLowResVertsPerSideX = NumLowResCellsPerSideX + 1;
	const INT NumLowResVertsPerSideY = NumLowResCellsPerSideY + 1;

	// Upper left patch
	// Uses the offset boundary in the X direction to make it overlap with the simulation grid,
	// And the actual boundary in the Y direction to make other patches overlap with it.
	const FLOAT UpperLeftSectionScaleY = LowerRight.Y / (FLOAT)NumLowResCellsPerSideY;
	for (INT Y = 0; Y < NumLowResVertsPerSideY; Y++)
	{
		for (INT X = 0; X < NumLowResVertsPerSideX; X++)
		{
			// Apply a pow(XCurve, 2) so that we will get somewhat better tessellation near the simulated grid
			const FLOAT XCurve = 1.0f - X / (FLOAT)NumLowResCellsPerSideX;
			Vertex.UV.Set((1.0f - XCurve * XCurve) * UpperLeftOffset.X, Y * UpperLeftSectionScaleY);
			Vertices[Y * NumLowResVertsPerSideX + X] = Vertex;
		}
	}
	Vertices += NumLowResVertsPerSideY * NumLowResVertsPerSideX;

	// Lower left patch
	const FLOAT LowerLeftSectionScaleX = LowerRight.X / (FLOAT)NumLowResCellsPerSideX;
	const FLOAT LowerLeftSectionTranslationY = LowerRightOffset.Y;
	for (INT Y = 0; Y < NumLowResVertsPerSideY; Y++)
	{
		const FLOAT YCurve = Y / (FLOAT)NumLowResCellsPerSideY;
		for (INT X = 0; X < NumLowResVertsPerSideX; X++)
		{
			Vertex.UV.Set(X * LowerLeftSectionScaleX, LowerLeftSectionTranslationY + YCurve * YCurve * (1.0f - LowerRightOffset.Y));
			Vertices[Y * NumLowResVertsPerSideX + X] = Vertex;
		}
	}
	Vertices += NumLowResVertsPerSideY * NumLowResVertsPerSideX;
	
	// Lower right patch
	const FLOAT LowerRightSectionTranslationX = LowerRightOffset.X;
	const FLOAT LowerRightSectionTranslationY = UpperLeft.Y;
	const FLOAT LowerRightSectionScaleY = (1.0f - UpperLeft.Y) / (FLOAT)NumLowResCellsPerSideY;
	for (INT Y = 0; Y < NumLowResVertsPerSideY; Y++)
	{
		for (INT X = 0; X < NumLowResVertsPerSideX; X++)
		{
			const FLOAT XCurve = X / (FLOAT)NumLowResCellsPerSideX;
			Vertex.UV.Set(LowerRightSectionTranslationX + XCurve * XCurve * (1.0f - LowerRightOffset.X), LowerRightSectionTranslationY + Y * LowerRightSectionScaleY);
			Vertices[Y * NumLowResVertsPerSideX + X] = Vertex;
		}
	}
	Vertices += NumLowResVertsPerSideY * NumLowResVertsPerSideX;

	// Upper right patch
	const FLOAT UpperRightSectionTranslationX = UpperLeft.X;
	const FLOAT UpperRightSectionScaleX = (1.0f - UpperLeft.X) / (FLOAT)NumLowResCellsPerSideX;
	for (INT Y = 0; Y < NumLowResVertsPerSideY; Y++)
	{
		const FLOAT YCurve = 1.0f - Y / (FLOAT)NumLowResCellsPerSideY;
		for (INT X = 0; X < NumLowResVertsPerSideX; X++)
		{
			Vertex.UV.Set(UpperRightSectionTranslationX + X * UpperRightSectionScaleX, (1.0f - YCurve * YCurve) * UpperLeftOffset.Y);
			Vertices[Y * NumLowResVertsPerSideX + X] = Vertex;
		}
	}
}

UBOOL FFluidSimulation::UpdateRenderData()
{
#if XBOX
#if LOCK_ON_WORKTHREAD
	if ( TextureData == NULL )
	{
		if ( RHIIsBusyTexture2D(HeightMapTextures[SimulationIndex], 0) )
		{
			return FALSE;	// Failure. Retry later.
		}
		else
		{
			AddTrackEvent( TRACK_TextureLocked, SimulationIndex );
			TextureData = RHILockTexture2D(HeightMapTextures[SimulationIndex],0,TRUE,TextureStride,FALSE);
		}
	}
	if ( BorderVertices == NULL )
	{
		if ( FlatVertexBuffers[SimulationIndex].IsBusy() )
		{
			return FALSE;	// Failure. Retry later.
		}
		else
		{
			BorderVertices = FlatVertexBuffers[SimulationIndex].Lock();
		}
	}
#endif

	FLOAT* Height		= HeightMap[CurrentHeightMap];
	INT Stride			= TextureStride / sizeof(FFloat16);
	FFloat16* Dest		= (FFloat16*) TextureData;

	static UBOOL NewMethod = 1;
	AddTrackEvent( TRACK_TextureStartUpdate, SimulationIndex );
	UpdateBorderGeometry(BorderVertices);

//#if 1
	if (NewMethod)
	{
	for ( INT Y=1; Y < NumCellsY; ++Y )
	{
		FFloat16* Dst	= Dest + Y*Stride;
		FLOAT* Src		= Height + (Y-1)*GridPitch;
		for ( INT X=1; X < NumCellsX; )
		{
			ConvertToTextureData( Dst, Src, GridPitch );
			Dst		+= 16;
			Src		+= 4;
			X		+= 4;
		}
	}
	}
	else
	{
//#else
	for ( INT Y=0; Y <= NumCellsY; ++Y )
	{
		for ( INT X=0; X <= NumCellsX; X += 8 )
		{
			ConvertToHalf8( Dest + X, Height + X );
		}
		Dest		+= Stride;
		Height		+= GridPitch;
	}
	}
	AddTrackEvent( TRACK_TextureStopUpdate, SimulationIndex );

#if LOCK_ON_WORKTHREAD
	// Store the simulation position that was used to update render data for the rendering thread
	RenderDataPosition[SimulationIndex] = SimulationPos[CurrentHeightMap];
	FlatVertexBuffers[SimulationIndex].Unlock();
	RHIUnlockTexture2D(HeightMapTextures[SimulationIndex], 0, FALSE);
	TextureData = NULL;
	BorderVertices = NULL;
	AddTrackEvent( TRACK_TextureUnlocked, SimulationIndex );
#endif

//#endif
#else
	UpdateBorderGeometry(BorderVertices);
	// Store the simulation position that was used to update render data for the rendering thread
	RenderDataPosition[SimulationIndex] = SimulationPos[CurrentHeightMap];

	// Update the vertices on the grid, iterating over the grid vertices.
	// Setup default packing order, starting at the negative limits of the grid and working toward the positive
	FVector2D UVOrigin( FLOAT(SimulationPos[CurrentHeightMap].X)/FLOAT(TotalSize.X), FLOAT(SimulationPos[CurrentHeightMap].Y)/FLOAT(TotalSize.Y) );
	FVector2D StepUV( 1.0f/TotalSize.X, 1.0f/TotalSize.Y );
	INT StartY = 0;
	INT EndY = NumCellsY;
	INT IncY = 1;
	INT StartX = 0;
	INT EndX = NumCellsX;
	INT IncX = 1;

	INT NormalStartY = 1;
	INT NormalEndY = NumCellsY;
	INT NormalStartX = 1;
	INT NormalEndX = NumCellsX;

	// Pack the render data from back-to-front based on the view direction if the material is translucent
	FVector EffectiveViewDirection = LastViewDirection[SimulationIndex];
	if ( bOpaqueMaterial )
	{
		// Reverse the view direction so that the render data will be packed from front-to-back for opaque materials
		EffectiveViewDirection = -LastViewDirection[SimulationIndex];
	}

	const UBOOL bPositiveViewDirX = EffectiveViewDirection.X > 0.0f;
	// If the view direction's X component is positive, pack from the positive x limit to the negative x limit
	if (bPositiveViewDirX)
	{
		UVOrigin.X = FLOAT(SimulationPos[CurrentHeightMap].X + NumCellsX) / FLOAT(TotalSize.X);
		StepUV.X = -1.0f / TotalSize.X;
		StartX = NumCellsX;
		EndX = 0;
		IncX = -1;
		NormalStartX = NumCellsX - 1;
		NormalEndX = 1;
	}

	const UBOOL bPositiveViewDirY = EffectiveViewDirection.Y > 0.0f;
	// If the view direction's Y component is positive, pack from the positive y limit to the negative y limit
	if (bPositiveViewDirY)
	{
		UVOrigin.Y = FLOAT(SimulationPos[CurrentHeightMap].Y + NumCellsY) / FLOAT(TotalSize.Y);
		StepUV.Y = -1.0f / TotalSize.Y;
		StartY = NumCellsY;
		EndY = 0;
		IncY = -1;
		NormalStartY = NumCellsY - 1;
		NormalEndY = 1;
	}

	// Culling needs to be reversed if either (but not both) of the packing directions were swapped.
	bReverseCulling[SimulationIndex] = XOR(bPositiveViewDirX, bPositiveViewDirY);

	INT VertexIndex = 0;
	FFluidVertex Vertex;
	Vertex.HeightDelta = FVector2D( 0.0f, 0.0f );
	FLOAT* Height = HeightMap[CurrentHeightMap];
	const FLOAT MainScale = (Component->bShowFluidSimulation && bEnableCPUSimulation) ? Component->FluidHeightScale : 0.0f;

	const UBOOL bIterateYFirst = Abs(EffectiveViewDirection.Y) > Abs(EffectiveViewDirection.X);
	// Iterate over Y first if the view direction is closer to the Y axis
	if (bIterateYFirst)
	{
		for ( INT Y=StartY; Y >= 0 && Y <= NumCellsY; Y += IncY )
		{
			Vertex.UV	= UVOrigin;
			const INT IndexY = Y * GridPitch;
			for ( INT X=StartX; X >= 0 && X <= NumCellsX; X += IncX, ++VertexIndex )
			{
				const INT Index			= IndexY + X;
				Vertex.Height			= Height[ Index ] * MainScale;
				Vertices[VertexIndex]	= Vertex;
				Vertex.UV.X				+= StepUV.X;
			}
			UVOrigin.Y	+= StepUV.Y;
		}
	}
	else
	{
		// Reverse culling if it is not reversed already
		bReverseCulling[SimulationIndex] = XOR(bReverseCulling[SimulationIndex], TRUE);
		for ( INT X=StartX; X >= 0 && X <= NumCellsX; X += IncX )
		{
			Vertex.UV	= UVOrigin;
			for ( INT Y=StartY; Y >= 0 && Y <= NumCellsY; Y += IncY, ++VertexIndex )
			{
				const INT Index			= Y * GridPitch + X;
				Vertex.Height			= Height[ Index ] * MainScale;
				Vertices[VertexIndex]	= Vertex;
				Vertex.UV.Y				+= StepUV.Y;
			}
			UVOrigin.X	+= StepUV.X;
		}
	}

	UBOOL bShowNormals = Component->bShowSimulationNormals;
	if (bShowNormals && DebugPositions.Num() == 0)
	{
		DebugPositions.AddZeroed( NumVertices );
		DebugNormals.AddZeroed( NumVertices );
	}

	// Calculate surface normals
	FLOAT HeightScale = Component->LightingContrast * MainScale/CellWidth;

	// Iterate in the same order that vertex positions were packed
	if (bIterateYFirst)
	{
		bUseYFirstIndexBuffer[SimulationIndex] = TRUE;
		// Start at [1, 1]
		VertexIndex = NumCellsX + 2;
		for ( INT Y=NormalStartY; Y > 0 && Y < NumCellsY; Y += IncY )
		{
			for ( INT X=NormalStartX; X > 0 && X < NumCellsX; X += IncX )
			{
				CalculateNormal( Height, X, Y, HeightScale, Vertices[VertexIndex].HeightDelta );

				if (bShowNormals)
				{
					FVector VX( 6.0f, 0.0f, Vertices[VertexIndex].HeightDelta.X*HeightScale );
					FVector VY( 0.0f, 6.0f, Vertices[VertexIndex].HeightDelta.Y*HeightScale );
					FVector Normal = (VX ^ VY).UnsafeNormal();
					FVector Position;
					Position.X = (Vertices[VertexIndex].UV.X - 0.5f) * TotalWidth;
					Position.Y = (Vertices[VertexIndex].UV.Y - 0.5f) * TotalHeight;
					Position.Z = SURFHEIGHT( Height, X, Y ) * MainScale;
					DebugPositions(VertexIndex) = Component->LocalToWorld.TransformFVector(Position);
					DebugNormals(VertexIndex) = Normal;
				}
				VertexIndex++;
			}
			// Skip over the last and first columns
			VertexIndex += 2;
		}
	}
	else
	{
		bUseYFirstIndexBuffer[SimulationIndex] = FALSE;
		// Start at [1, 1]
		VertexIndex = NumCellsY + 2;
		for ( INT X=NormalStartX; X > 0 && X < NumCellsX; X += IncX )
		{
			for ( INT Y=NormalStartY; Y > 0 && Y < NumCellsY; Y += IncY )
			{
				CalculateNormal( Height, X, Y, HeightScale, Vertices[VertexIndex].HeightDelta );

				if (bShowNormals)
				{
					FVector VX( 6.0f, 0.0f, Vertices[VertexIndex].HeightDelta.X*HeightScale );
					FVector VY( 0.0f, 6.0f, Vertices[VertexIndex].HeightDelta.Y*HeightScale );
					FVector Normal = (VX ^ VY).UnsafeNormal();
					FVector Position;
					Position.X = (Vertices[VertexIndex].UV.X - 0.5f) * TotalWidth;
					Position.Y = (Vertices[VertexIndex].UV.Y - 0.5f) * TotalHeight;
					Position.Z = SURFHEIGHT( Height, X, Y ) * MainScale;
					DebugPositions(VertexIndex) = Component->LocalToWorld.TransformFVector(Position);
					DebugNormals(VertexIndex) = Normal;
				}
				VertexIndex++;
			}
			// Skip over the last and first rows
			VertexIndex += 2;
		}
	}
#endif
	return TRUE;
}

void FFluidSimulation::AddForce( const FVector& LocalPos, FLOAT Strength, FLOAT LocalRadius, UBOOL bImpulse/*=FALSE*/ )
{
	if ( bEnableCPUSimulation || bEnableGPUSimulation )
	{
		INT Index = FluidForces[1 - SimulationIndex].Add(1);
		FFluidForce& Force = FluidForces[1 - SimulationIndex]( Index );
		Force.LocalPos	= LocalPos;
		Force.Strength	= bImpulse ? (Strength*40.0f) : (Strength/2.0f);
		Force.Radius	= LocalRadius;
		Force.bImpulse	= bImpulse;
	}
}

void FFluidSimulation::GameThreadTick( FLOAT InDeltaTime )
{
#if XBOX && ENABLE_CPUTRACE
	GGameThreadCore = GetCurrentProcessorNumber();
#endif

	appInterlockedIncrement( &SimulationRefCount );

	ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
		TickSimulation,
		FFluidSimulation*, FluidSimulation, this,
		FLOAT, DeltaTime, InDeltaTime,
	{
		FluidSimulation->RenderThreadTick( DeltaTime );
	});
}

void FFluidSimulation::RenderThreadTick( FLOAT InDeltaTime )
{
	SCOPE_CYCLE_COUNTER( STAT_FluidRenderthreadBlocked );

	AddTrackEvent( TRACK_RenderTick, SimulationIndex );

#if XBOX && ENABLE_CPUTRACE
	GRenderThreadCore = GetCurrentProcessorNumber();
#endif

	// Resources aren't locked the first time, or if the device has been lost.
	if ( bResourcesLocked == FALSE )
	{
		if ( ShouldSimulate() )
		{
			// Start the next simulation step.
			appInterlockedExchange( &bSimulationBusy, TRUE );
			appMemoryBarrier();
			DeltaTime = InDeltaTime;
			LockResources();
			if ( GThreadedFluidSimulation )
			{
				GThreadPool->AddQueuedWork( this );
			}
		}
	}

	if ( GThreadedFluidSimulation )
	{
		// Wait until the current simulation is complete.
		BlockOnSimulation();
	}
	else if ( ShouldSimulate() )
	{
		DoWork();
	}

	UnlockResources();

	// Render the full simulation geometry or a simple flat quad (should still show the simulation if bPause is TRUE).
	bShowSimulation = Component->bShowFluidSimulation && ShouldSimulate();

#if STATS
	{
		const FCycleStat* Stat = GStatManager.GetCycleStat(STAT_FluidSimulation);
		DWORD CallCount = Stat ? Stat->NumCallsPerFrame : 0;
		DWORD Value = Stat ? Stat->Cycles : 0;
		SET_CYCLE_COUNTER(STAT_FluidSimulation, Value + STAT_FluidSimulationValue, CallCount + STAT_FluidSimulationCount);
		STAT_FluidSimulationValue = 0;
		STAT_FluidSimulationCount = 0;
	}
	{
		const FCycleStat* Stat = GStatManager.GetCycleStat(STAT_FluidTessellation);
		DWORD CallCount = Stat ? Stat->NumCallsPerFrame : 0;
		DWORD Value = Stat ? Stat->Cycles : 0;
		SET_CYCLE_COUNTER(STAT_FluidTessellation, Value + STAT_FluidTessellationValue, CallCount + 1);
		STAT_FluidTessellationValue = 0;
	}
#endif

	// Run the GPU simulation of the detail grid
	if ( bEnableGPUSimulation && !Component->bPause )
	{
		DetailGPUResource.Tick( 
			DeltaTime, 
			FluidForces[SimulationIndex], 
			Component->DetailUpdateRate,
			Component->DetailDamping,
			Component->DetailTravelSpeed,
			Component->DetailTransfer,
			Component->DetailHeightScale,
			Component->bTiling
			);
	}

	// Transfer some results from the simulation to the renderthread.
	SimulationActivity = bSimulationDirty ? 100.0f : Abs<FLOAT>(CurrentSum + PrevSum);

	// Swap buffers.
	SimulationIndex = 1 - SimulationIndex;

	FluidForces[1 - SimulationIndex].Reset();
	DeltaTime = InDeltaTime;

	if ( ShouldSimulate() )
	{
		// Start the next simulation step.
		appInterlockedExchange( &bSimulationBusy, TRUE );
		appMemoryBarrier();
		LockResources();
		if ( GThreadedFluidSimulation )
		{
			GThreadPool->AddQueuedWork( this );
		}
	}

	appInterlockedDecrement( &SimulationRefCount );
}

void FFluidSimulation::LockResources()
{
	if ( bResourcesLocked == FALSE && ShouldSimulate() )
	{
#if XBOX
	#if !LOCK_ON_WORKTHREAD
		AddTrackEvent( TRACK_TextureLocked, SimulationIndex );
		TextureData			= RHILockTexture2D(HeightMapTextures[SimulationIndex],0,TRUE,TextureStride,FALSE);
		BorderVertices		= FlatVertexBuffers[SimulationIndex].Lock();
	#endif
#else
		Vertices			= VertexBuffers[SimulationIndex].Lock();
		BorderVertices		= FlatVertexBuffers[SimulationIndex].Lock();
#endif
		bResourcesLocked	= TRUE;
	}
}

void FFluidSimulation::UnlockResources()
{
	if ( bResourcesLocked == TRUE )
	{
#if XBOX
	#if !LOCK_ON_WORKTHREAD
		RHIUnlockTexture2D(HeightMapTextures[SimulationIndex], 0, FALSE);
		AddTrackEvent( TRACK_TextureUnlocked, SimulationIndex );
		FlatVertexBuffers[SimulationIndex].Unlock();
		TextureData			= NULL;
		BorderVertices		= NULL;
	#endif
#else
		VertexBuffers[SimulationIndex].Unlock();
		FlatVertexBuffers[SimulationIndex].Unlock();
		Vertices			= NULL;
		BorderVertices		= NULL;
#endif
		bResourcesLocked	= FALSE;
	}
}

void FFluidSimulation::RenderThreadInitResources( INT BufferIndex, FTexture2DResourceMem* ResourceMem )
{
	VertexFactories[BufferIndex].InitResources( VertexBuffers[BufferIndex], this );
	FlatVertexFactories[BufferIndex].InitResources( FlatVertexBuffers[BufferIndex], this );
	FlatQuadVertexFactory.InitResources( FlatQuadVertexBuffer, this );

#if XBOX
	FTexture2DRHIRef& Texture = HeightMapTextures[BufferIndex];
	INT SizeX = NumCellsX + 1;
	INT SizeY = NumCellsY + 1;
	UINT TextureStride;
	Texture = RHICreateTexture2D( SizeX, SizeY, PF_FloatRGBA, 1, TexCreate_Dynamic|TexCreate_NoTiling, ResourceMem );
	void* TextureData = RHILockTexture2D( Texture, 0, TRUE, TextureStride, FALSE );
	appMemzero( TextureData, SizeY * TextureStride );
	RHIUnlockTexture2D( Texture, 0, FALSE );
#endif
}

void FFluidSimulation::InitIndexBufferX()
{
	// Generate the indices on the grid, iterating over the grid cells.
	PLATFORM_INDEX_TYPE* Indices = (PLATFORM_INDEX_TYPE*) YFirstIndexBuffer.Lock();
	PLATFORM_INDEX_TYPE* CurrentIndices = Indices;
	PLATFORM_INDEX_TYPE IndexOrigin	= 0;
	INT NumX = GetNumCellsX();
	INT NumY = GetNumCellsY();

	// validate data
#if DISALLOW_32BIT_INDICES
	if ((NumX + 1) * (NumY + 1) > 65535)
	{
		appErrorf(TEXT("Fluid surface of size %d x %d is too big for the iPhone"), NumX, NumY);
	}
#endif
	
	DWORD Pitch = NumX + 1;
	UBOOL bReverseTriangulation = FALSE;
	for ( INT Y=0; Y < NumY; ++Y )
	{
		bReverseTriangulation = FALSE;
		PLATFORM_INDEX_TYPE Index = IndexOrigin;
		for ( INT X=0; X < NumX; ++X )
		{
			if ( bReverseTriangulation )
			{
				CurrentIndices[0] = Index + 0;
				CurrentIndices[1] = Index + Pitch + 1;
				CurrentIndices[2] = Index + 1;
				CurrentIndices[3] = Index + 0;
				CurrentIndices[4] = Index + Pitch + 0;
				CurrentIndices[5] = Index + Pitch + 1;
			}
			else
			{
				CurrentIndices[0] = Index + 0;
				CurrentIndices[1] = Index + Pitch + 0;
				CurrentIndices[2] = Index + 1;
				CurrentIndices[3] = Index + Pitch + 0;
				CurrentIndices[4] = Index + Pitch + 1;
				CurrentIndices[5] = Index + 1;
			}
			CurrentIndices		+= 6;
			Index++;
			bReverseTriangulation = !bReverseTriangulation;
		}
		IndexOrigin		+= Pitch;
	}
	YFirstIndexBuffer.Unlock();
}

void FFluidSimulation::InitIndexBufferY()
{
	// Generate the indices on the grid, iterating over the grid cells.
	PLATFORM_INDEX_TYPE* Indices = (PLATFORM_INDEX_TYPE*) XFirstIndexBuffer.Lock();
	PLATFORM_INDEX_TYPE* CurrentIndices = Indices;
	INT NumX = GetNumCellsX();
	INT NumY = GetNumCellsY();

#if DISALLOW_32BIT_INDICES
	if ((NumX + 1) * (NumY + 1) > 65535)
	{
		appErrorf(TEXT("Fluid surface of size %d x %d is too big for this platform (must be less than 65535 verts)"), NumX, NumY);
	}
#endif	
	
	PLATFORM_INDEX_TYPE IndexOrigin	= 0;
	DWORD Pitch			= NumY + 1;
	UBOOL bReverseTriangulation = FALSE;
	for ( INT X=0; X < NumX; ++X )
	{
		bReverseTriangulation = FALSE;
		PLATFORM_INDEX_TYPE Index = IndexOrigin;
		for ( INT Y=0; Y < NumY; ++Y )
		{
			if ( bReverseTriangulation )
			{
				CurrentIndices[0] = Index + 0;
				CurrentIndices[1] = Index + Pitch + 1;
				CurrentIndices[2] = Index + 1;
				CurrentIndices[3] = Index + 0;
				CurrentIndices[4] = Index + Pitch + 0;
				CurrentIndices[5] = Index + Pitch + 1;
			}
			else
			{
				CurrentIndices[0] = Index + 0;
				CurrentIndices[1] = Index + Pitch + 0;
				CurrentIndices[2] = Index + 1;
				CurrentIndices[3] = Index + Pitch + 0;
				CurrentIndices[4] = Index + Pitch + 1;
				CurrentIndices[5] = Index + 1;
			}
			CurrentIndices		+= 6;
			Index++;
			bReverseTriangulation = !bReverseTriangulation;
		}
		IndexOrigin		+= Pitch;
	}
	XFirstIndexBuffer.Unlock();
}

void FFluidSimulation::InitFlatIndexBuffer()
{
	// Populate the index buffer for the geometry in the deactivated state (vertices are stored row-by-row).

	WORD* Indices = (WORD*) FlatIndexBuffer.Lock();
	INT VertexPitch = NumLowResCellsPerSideX + 1;
	INT LowResIndex = 0;
	// This index buffer contains the indices of all 4 border geometry patches
	for ( INT QuadrantIndex=0; QuadrantIndex < 4; ++QuadrantIndex )
	{
		const INT VertexOffset = QuadrantIndex * VertexPitch * (NumLowResCellsPerSideY + 1);
		for ( WORD Y=0; Y < NumLowResCellsPerSideY; ++Y )
		{
			for ( WORD X=0; X < NumLowResCellsPerSideX; ++X )
			{
				Indices[LowResIndex++] = (Y+0)*VertexPitch + (X+1) + VertexOffset;
				Indices[LowResIndex++] = (Y+0)*VertexPitch + (X+0) + VertexOffset;
				Indices[LowResIndex++] = (Y+1)*VertexPitch + (X+0) + VertexOffset;
				Indices[LowResIndex++] = (Y+0)*VertexPitch + (X+1) + VertexOffset;
				Indices[LowResIndex++] = (Y+1)*VertexPitch + (X+0) + VertexOffset;
				Indices[LowResIndex++] = (Y+1)*VertexPitch + (X+1) + VertexOffset;
			}
		}
		
	}
	FlatIndexBuffer.Unlock();

	// Populate the index buffer for the geometry in the deactivated state (vertices are stored row-by-row).
	{
		WORD* Indices = (WORD*) FlatQuadIndexBuffer.Lock();
		INT NumLowResQuadsX = FlatQuadVertexBuffer.GetNumQuadsX();
		INT NumLowResQuadsY = FlatQuadVertexBuffer.GetNumQuadsY();
		INT VertexPitch = NumLowResQuadsX + 1;
		INT LowResIndex = 0;
		for ( WORD Y=0; Y < NumLowResQuadsY; ++Y )
		{
			for ( WORD X=0; X < NumLowResQuadsX; ++X )
			{
				Indices[LowResIndex++] = (Y+0)*VertexPitch + (X+1);
				Indices[LowResIndex++] = (Y+0)*VertexPitch + (X+0);
				Indices[LowResIndex++] = (Y+1)*VertexPitch + (X+0);
				Indices[LowResIndex++] = (Y+0)*VertexPitch + (X+1);
				Indices[LowResIndex++] = (Y+1)*VertexPitch + (X+0);
				Indices[LowResIndex++] = (Y+1)*VertexPitch + (X+1);
			}
		}
		FlatQuadIndexBuffer.Unlock();
	}
}

/**
 * Allocates texture memory on the gamethread. Will try to stream out high-res mip-levels if there's not enough room.
 * @return	A new FTexture2DResourceMem object, representing the allocated memory, or NULL if the allocation failed.
 */
FTexture2DResourceMem* FFluidSimulation::CreateTextureResourceMemory()
{
#if XBOX  && !USE_NULL_RHI
	INT SizeX = NumCellsX + 1;
	INT SizeY = NumCellsY + 1;

	// Allows failure the first attempt.
	FTexture2DResourceMem* XeResourceMem = UTexture2D::CreateResourceMem(SizeX, SizeY, 1, PF_FloatRGBA, TexCreate_Dynamic|TexCreate_NoTiling, NULL );
	return XeResourceMem;
#else
	return NULL;
#endif
}

void FFluidSimulation::InitResources()
{
	BeginInitResource(&FlatIndexBuffer);
	BeginInitResource(&YFirstIndexBuffer);
	BeginInitResource(&XFirstIndexBuffer);
	BeginInitResource(&FlatQuadIndexBuffer);
	BeginInitResource(&FlatQuadVertexBuffer);

	for ( INT BufferIndex=0; BufferIndex < 2; ++BufferIndex )
	{
		BeginInitResource( &VertexBuffers[BufferIndex] );
		BeginInitResource( &FlatVertexBuffers[BufferIndex] );

		// Allocate texture memory on the gamethread, so we can stream out high-res mips if we need to make room.
		FTexture2DResourceMem* ResourceMem = CreateTextureResourceMemory();

		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			CreateHeightmapTexture,
			FFluidSimulation*, FluidSimulation, this,
			INT,BufferIndex,BufferIndex,
			FTexture2DResourceMem*, ResourceMem, ResourceMem,
			{
				FluidSimulation->RenderThreadInitResources( BufferIndex, ResourceMem );
			});

		BeginInitResource(&VertexFactories[BufferIndex]);
		BeginInitResource(&FlatVertexFactories[BufferIndex]);
		BeginInitResource(&FlatQuadVertexFactory);
	}

#if !XBOX
	// Don't try to lock the index buffer if we are running a commandlet
	if (!GIsUCC)
	{
		//@todo: Use indexed triangle strips to save on index buffer memory
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitYFirstIndexBuffer,
			FFluidSimulation*, FluidSimulation, this,
		{
			FluidSimulation->InitIndexBufferX();
			FluidSimulation->InitIndexBufferY();
		});
	}
#endif

	if (!GIsUCC)
	{
		ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
			InitFlatIndexBufferCommand,
			FFluidSimulation*, FluidSimulation, this,
		{
			FluidSimulation->InitFlatIndexBuffer();
		});
	}

	if ( bEnableGPUSimulation )
	{
		BeginInitResource(&DetailGPUResource);
	}
}

void FFluidSimulation::ReleaseResources( UBOOL bBlockOnRelease )
{
	ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
		StopSimulation,
		FFluidSimulation*, FluidSimulation, this,
	{
		FluidSimulation->BlockOnSimulation();
	});

	BeginReleaseResource(&FlatIndexBuffer);
	BeginReleaseResource(&YFirstIndexBuffer);
	BeginReleaseResource(&XFirstIndexBuffer);
	BeginReleaseResource(&VertexBuffers[0]);
	BeginReleaseResource(&VertexBuffers[1]);
	BeginReleaseResource(&FlatVertexBuffers[0]);
	BeginReleaseResource(&FlatVertexBuffers[1]);
	BeginReleaseResource(&FlatQuadVertexBuffer);
	BeginReleaseResource(&FlatQuadIndexBuffer);
	BeginReleaseResource(&VertexFactories[0]);
	BeginReleaseResource(&VertexFactories[1]);
	BeginReleaseResource(&FlatVertexFactories[0]);
	BeginReleaseResource(&FlatVertexFactories[1]);
	BeginReleaseResource(&DetailGPUResource);
	BeginReleaseResource(&FlatQuadVertexFactory);

	ReleaseResourcesFence.BeginFence();
	if ( bBlockOnRelease )
	{
		ReleaseResourcesFence.Wait();
	}
}

UBOOL FFluidSimulation::IsReleased()
{
	UBOOL bStillSimulating = bSimulationBusy && GThreadedFluidSimulation;
	return (ReleaseResourcesFence.GetNumPendingFences() == 0) && !bStillSimulating && (SimulationRefCount == 0);
}

/** @return TRUE if the linecheck passed (didn't hit anything) */
UBOOL FFluidSimulation::LineCheck( FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags )
{
	FVector Direction = End - Start;
	const FVector& Normal = Plane;

	if ( Extent.IsZero() )
	{
		// Calculate the intersection point (line vs plane).
		FLOAT Denom = Normal | Direction;
		if ( Abs(Denom) < KINDA_SMALL_NUMBER )	// Parallel to the fluid surface?
		{
			return TRUE;
		}
		FLOAT HitTime = (Normal | (Normal * Plane.W - Start)) / Denom;
		if ( HitTime < 0.0f || HitTime > 1.0f )	// Didn't intersect the fluid surface plane?
		{
			return TRUE;
		}
		FVector Intersection = Start + HitTime*Direction;

		// Check if the intersection point is inside the fluid (i.e. it's inside all of the edges).
		if ( Edges[0].PlaneDot(Intersection) <= 0 &&
			 Edges[1].PlaneDot(Intersection) <= 0 &&
			 Edges[2].PlaneDot(Intersection) <= 0 &&
			 Edges[3].PlaneDot(Intersection) <= 0 )
		{
			Result.Time = HitTime;
			Result.Normal = Normal;
			Result.Location = Intersection;
			return FALSE;
		}
	}
	else
	{
		//@TODO optimize... All transforms are very expensive, especially TransformBy().
		FVector LocalStart = FluidWorldToLocal.TransformFVector( Start );
		FVector LocalEnd = FluidWorldToLocal.TransformFVector( End );
		FBox LocalBox( -Extent, Extent );
		LocalBox = LocalBox.TransformBy( FluidWorldToLocal );
		FVector LocalExtent = LocalBox.GetExtent();
		FBox Bbox( FVector(-TotalWidth*0.5f, -TotalHeight*0.5f, -10.0f), FVector(TotalWidth*0.5f, TotalHeight*0.5f, 10.0f) );
		FVector HitLocation, HitNormal;
		FLOAT HitTime;
		if ( FLineExtentBoxIntersection( Bbox, LocalStart, LocalEnd, LocalExtent, HitLocation, HitNormal, HitTime ) )
		{
			Result.Time = HitTime;
			Result.Normal = Normal;
			Result.Location = Start + HitTime*Direction;
			return FALSE;
		}
	}

	return TRUE;
}

UBOOL FFluidSimulation::PointCheck( FCheckResult& Result, const FVector& Location, const FVector& Extent, DWORD TraceFlags )
{
	FBox MyBbox( FVector(-TotalWidth*0.5f, -TotalHeight*0.5f, -10.0f), FVector(TotalWidth*0.5f, TotalHeight*0.5f, 10.0f) );
	FBox OtherBbox( Location - Extent, Location + Extent );
	OtherBbox = OtherBbox.TransformBy( FluidWorldToLocal );
	if ( MyBbox.Intersect( OtherBbox ) )
	{
		FLOAT HeightDist = Plane.PlaneDot(Location);
		Result.Normal = Plane;
		Result.Location = Location + Result.Normal * Max<FLOAT>(20.0f - HeightDist, 0.0f);
		return FALSE;
	}
	return TRUE;
}

void FFluidSimulation::SetExtents( const FMatrix& InLocalToWorld, const FPlane& InPlane, const FPlane* InEdges )
{
	FluidWorldToLocal = InLocalToWorld.Inverse();
	Plane = InPlane;
	Edges[0] = InEdges[0];
	Edges[1] = InEdges[1];
	Edges[2] = InEdges[2];
	Edges[3] = InEdges[3];
}

UBOOL FFluidSimulation::IsActive() const
{
	return (bEnableCPUSimulation || bEnableGPUSimulation);
}

void FFluidSimulation::UpdateShaderParameters( INT OctantID )
{
	// Set up vertexshader tessellation parameters.
	const FLOAT HeightScale	= (bShowSimulation && bEnableCPUSimulation) ? Component->FluidHeightScale : 0.0f;
	const FLOAT TweakScale	= Component->LightingContrast * HeightScale/CellWidth;
	GridSize.Set(TotalWidth, TotalHeight, TweakScale, 0.0f);

#if XBOX
	// Back-to-front sorting is performed by remapping the x/y quad coordinates in the vertex shader:
	//
	// xy' = (a,b)*x + (c,d)*y + (e,f)
	//
	// (a,b) and (c,d) lets us swap x/y and choose direction (+/-). At the same time, they scale the
	// quad coordinates to texture space (0,1).
	//
	// (e,f) is used for inverting coordinates - i.e. 1-x or 1-y.
	//
	// TessellationValues1.xy holds (a,b)
	// TessellationValues2.xy holds (c,d)
	// TessellationValues2.zw holds (e,f)
	static const FVector4 TessellationValues1[8] =
	{
		FVector4(  0, -1, 0, 0 ),	// Tessellate Left, Up			(Viewdir Right, Down, 0-45 degrees)
		FVector4( -1,  0, 0, 0 ),	// Tessellate Up, left			(Viewdir Down, Right, 45-90 degrees)
		FVector4(  1,  0, 0, 0 ),	// Tessellate Up, right			(Viewdir Down, Left, 90-135 degrees)
		FVector4(  0, -1, 0, 0 ),	// Tessellate Right, up			(Viewdir Left, Down, 135-180 degrees)
		FVector4(  0,  1, 0, 0 ),	// Tessellate Right, down		(Viewdir Left, Up, 180-225 degrees)
		FVector4(  1,  0, 0, 0 ),	// Tessellate Down, right		(Viewdir Up, Left, 225-270 degrees)
		FVector4( -1,  0, 0, 0 ),	// Tessellate Down, left		(Viewdir Up, Right, 270-315 degrees)
		FVector4(  0,  1, 0, 0 ),	// Tessellate Left, Down		(Viewdir Right, Up, 315-360 degrees)
	};
	static const FVector4 TessellationValues2[8] =
	{
		FVector4( -1,  0, 1, 1 ),	// Tessellate Left, Up			(Viewdir Right, Down, 0-45 degrees)
		FVector4(  0, -1, 1, 1 ),	// Tessellate Up, left			(Viewdir Down, Right, 45-90 degrees)
		FVector4(  0, -1, 0, 1 ),	// Tessellate Up, right			(Viewdir Down, Left, 90-135 degrees)
		FVector4(  1,  0, 0, 1 ),	// Tessellate Right, up			(Viewdir Left, Down, 135-180 degrees)
		FVector4(  1,  0, 0, 0 ),	// Tessellate Right, down		(Viewdir Left, Up, 180-225 degrees)
		FVector4(  0,  1, 0, 0 ),	// Tessellate Down, right		(Viewdir Up, Left, 225-270 degrees)
		FVector4(  0,  1, 1, 0 ),	// Tessellate Down, left		(Viewdir Up, Right, 270-315 degrees)
		FVector4( -1,  0, 1, 0 ),	// Tessellate Left, Down		(Viewdir Right, Up, 315-360 degrees)
	};
	static const UBOOL ShouldReverseCulling[8] =
	{
		FALSE,
		TRUE,
		FALSE,
		TRUE,
		FALSE,
		TRUE,
		FALSE,
		TRUE,
	};

	// Let the TessellationLevel ramp from 4.0 to 15.999 before changing number of tessellation quads,
	// and then ramp it all over again from 4.0 to 15.999. Etc.
	FLOAT QuadFactor		= appFloor( appLog2( Component->GPUTessellationFactor ) / appLog2( GPUTESSELLATION ) );
	QuadFactor				= appPow( GPUTESSELLATION, QuadFactor - 1.0f );
	TessellationLevel		= Component->GPUTessellationFactor / QuadFactor;
	NumTessQuadsX			= Max<INT>(appCeil( NumCellsX * QuadFactor ), 1);
	NumTessQuadsY			= Max<INT>(appCeil( NumCellsY * QuadFactor ), 1);

	TessellationFactors1	= TessellationValues1[ OctantID ];
	TessellationFactors2	= TessellationValues2[ OctantID ];
	bReverseCullingXbox		= ShouldReverseCulling[ OctantID ];
	TexcoordScaleBias[0]	= FLOAT(NumCellsX) / FLOAT(TotalSize.X);
	TexcoordScaleBias[1]	= FLOAT(NumCellsY) / FLOAT(TotalSize.Y);
	TexcoordScaleBias[2]	= FLOAT(RenderDataPosition[1 - SimulationIndex].X) / FLOAT(TotalSize.X);
	TexcoordScaleBias[3]	= FLOAT(RenderDataPosition[1 - SimulationIndex].Y) / FLOAT(TotalSize.Y);

	// Are we swapping X and Y coordinates?
	if ( Abs<FLOAT>(TessellationFactors1[0]) < 0.1f )
	{
		TessellationParameters.Set( HeightScale, NumTessQuadsY, 1.0f/NumTessQuadsY, 1.0f/NumTessQuadsX );
	}
	else
	{
		TessellationParameters.Set( HeightScale, NumTessQuadsX, 1.0f/NumTessQuadsX, 1.0f/NumTessQuadsY );
	}

	// Multiply (a,b) and (c,d) by (1/NumQuadsX, 1/NumQuadsY), to scale to texture space.
	TessellationFactors1[0]	/= NumTessQuadsX;
	TessellationFactors1[1]	/= NumTessQuadsY;
	TessellationFactors2[0]	/= NumTessQuadsX;
	TessellationFactors2[1]	/= NumTessQuadsY;
#endif
}

void FFluidSimulation::SetDetailPosition(FVector LocalPos)
{
	FVector ClampedPos;
	ClampedPos.X = Clamp<FLOAT>(LocalPos.X, -(TotalWidth - Component->DetailSize)*0.5f, (TotalWidth - Component->DetailSize)*0.5f);
	ClampedPos.Y = Clamp<FLOAT>(LocalPos.Y, -(TotalHeight - Component->DetailSize)*0.5f, (TotalHeight - Component->DetailSize)*0.5f);
	ClampedPos.Z = 0.0f;

	DetailGPUResource.SetDetailPosition(ClampedPos, bEnableGPUSimulation);
}

void FFluidSimulation::SetSimulationPosition(FVector LocalPos)
{
	if ( bEnableCPUSimulation )
	{
		INT CenterX = appTrunc((LocalPos.X + TotalWidth*0.5f) / CellWidth);
		INT CenterY = appTrunc((LocalPos.Y + TotalHeight*0.5f) / CellHeight);
		INT X = Max<INT>( CenterX - NumCellsX/2, 0 );
		INT Y = Max<INT>( CenterY - NumCellsY/2, 0 );
		PendingSimulationPos.X = Min<INT>( X, TotalSize.X - NumCellsX );
		PendingSimulationPos.Y = Min<INT>( Y, TotalSize.Y - NumCellsY );
	}
	else
	{
		// In this case, NumCellsX/Y and CellWidth/CellHeight has been faked to contain the entire fluid
		// (and SimulationPos[] isn't been updated), so we'll use the component settings as an approximation.
		INT TotalQuadsX = appTrunc( TotalWidth / Component->GridSpacing );
		INT TotalQuadsY = appTrunc( TotalHeight / Component->GridSpacing );
		INT SimQuadsX = Min<INT>(Component->SimulationQuadsX, TotalQuadsX);
		INT SimQuadsY = Min<INT>(Component->SimulationQuadsY, TotalQuadsY);
		INT CenterX = appTrunc((LocalPos.X + TotalWidth*0.5f) / Component->GridSpacing);
		INT CenterY = appTrunc((LocalPos.Y + TotalHeight*0.5f) / Component->GridSpacing);
		INT X = Max<INT>( CenterX - SimQuadsX/2, 0 );
		INT Y = Max<INT>( CenterY - SimQuadsY/2, 0 );
		PendingSimulationPos.X = Min<INT>( X, TotalQuadsX - SimQuadsX );
		PendingSimulationPos.Y = Min<INT>( Y, TotalQuadsY - SimQuadsY );
	}

	if ( !bShowSimulation )
	{
		SimulationPos[0] = SimulationPos[1] = PendingSimulationPos;
	}
}

/** Returns the rectangle of the simulation grid, in fluid local space. */
void FFluidSimulation::GetSimulationRect( FVector2D& TopLeft, FVector2D& BottomRight )
{
	if ( bEnableCPUSimulation && bShowSimulation )
	{
		TopLeft.X = SimulationPos[CurrentHeightMap].X * CellWidth - 0.5f * TotalWidth;
		TopLeft.Y = SimulationPos[CurrentHeightMap].Y * CellHeight - 0.5f * TotalHeight;
		BottomRight.X = TopLeft.X + GridWidth;
		BottomRight.Y = TopLeft.Y + GridHeight;
	}
	else
	{
		// In this case, NumCellsX/Y and CellWidth/CellHeight has been faked to contain the entire fluid
		// (and SimulationPos[] isn't been updated), so we'll use the component settings as an approximation.
		INT TotalQuadsX = appTrunc( TotalWidth / Component->GridSpacing );
		INT TotalQuadsY = appTrunc( TotalHeight / Component->GridSpacing );
		INT SimQuadsX = Min<INT>(Component->SimulationQuadsX, TotalQuadsX);
		INT SimQuadsY = Min<INT>(Component->SimulationQuadsY, TotalQuadsY);
		TopLeft.X = SimulationPos[CurrentHeightMap].X * Component->GridSpacing - 0.5f * TotalWidth;
		TopLeft.Y = SimulationPos[CurrentHeightMap].Y * Component->GridSpacing - 0.5f * TotalHeight;
		BottomRight.X = TopLeft.X + SimQuadsX * Component->GridSpacing;
		BottomRight.Y = TopLeft.Y + SimQuadsY * Component->GridSpacing;
	}
}

/** Returns the rectangle of the detail grid, in fluid local space. */
void FFluidSimulation::GetDetailRect( FVector2D& TopLeft, FVector2D& BottomRight )
{
	DetailGPUResource.GetDetailRect( TopLeft, BottomRight, bEnableGPUSimulation );
}

UBOOL FFluidSimulation::IsWithinSimulationGrid( const FVector& LocalPos, FLOAT Radius )
{
	FVector2D TopLeft, BottomRight;
	GetSimulationRect( TopLeft, BottomRight );
	return ((LocalPos.X - Radius) > TopLeft.X && (LocalPos.X + Radius) < BottomRight.X &&
			(LocalPos.Y - Radius) > TopLeft.Y && (LocalPos.Y + Radius) < BottomRight.Y);
}

UBOOL FFluidSimulation::IsWithinDetailGrid( const FVector& LocalPos, FLOAT Radius )
{
	FVector2D TopLeft, BottomRight;
	GetDetailRect( TopLeft, BottomRight );
	return ((LocalPos.X - Radius) > TopLeft.X && (LocalPos.X + Radius) < BottomRight.X &&
			(LocalPos.Y - Radius) > TopLeft.Y && (LocalPos.Y + Radius) < BottomRight.Y);
}

/**
 *	Octant 0 is [0..45) degrees
 *	Octant 1 is [45..90) degrees
 *	Octant 2 is [90..135) degrees
 *	Etc.
 */
INT FFluidSimulation::ClassifyOctant( const FVector& LocalDirection )
{
	// appAtan2 returns a value [-PI..+PI]
	FLOAT Angle = appAtan2( LocalDirection.Y, LocalDirection.X );
	INT Octant = appFloor( Angle / (FLOAT(PI)/4.0f) ) + 8;
	return (Octant % 8);
}

UBOOL FFluidSimulation::ShouldSimulate()
{
	static const FLOAT ActivityLimit = 5.0f;
	if ( !bEnableCPUSimulation || Component->bPause || (SimulationActivity < ActivityLimit && FluidForces[SimulationIndex].Num() == 0) )
	{
		return FALSE;
	}
	else
	{
		return TRUE;
	}
}
