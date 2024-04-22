/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __FLUIDSURFACEGPUSIMULATION_H__
#define __FLUIDSURFACEGPUSIMULATION_H__


struct FFluidForce
{
	FVector				LocalPos;
	FLOAT				Strength;
	FLOAT				Radius;
	UBOOL				bImpulse;
};

/* 
 * Parameters for applying a force to the GPU simulation
 */
struct FFluidForceParams
{
	/* Position in detail grid cells */
	FVector Position;

	/* Radius in detail grid cells */
	FLOAT Radius;
	FLOAT Strength;

	FFluidForceParams(FVector InPosition, FLOAT Radius, FLOAT InStrength) :
		Position(InPosition),
		Radius(Radius),
		Strength(InStrength)
	{}
};

/* 
 * Parameters for advancing the GPU simulation a step
 */
struct FFluidSimulateParams
{
	FLOAT DampingFactor;
	FLOAT TravelSpeed;
	UBOOL bTiling;

	FFluidSimulateParams(FLOAT InDampingFactor, FLOAT InTravelSpeed, UBOOL bInTiling)
	:	DampingFactor(InDampingFactor)
	,	TravelSpeed(InTravelSpeed)
	,	bTiling(bInTiling)
	{}
};

/* 
 * Parameters for generating normals on the GPU
 */
struct FFluidNormalParams
{
	FLOAT HeightScale;
	UBOOL bTiling;

	FFluidNormalParams(FLOAT InHeightScale, UBOOL bInTiling) :
		HeightScale(InHeightScale),
		bTiling(bInTiling)
	{}
};

/* 
 * Encapsulates GPU fluid simulation
 */
class FFluidGPUResource : public FRenderResource
{
private:

	const static INT NumHeightfields = 3;

	/* Index into DetailPosition, HeightTextures and HeightSurfaces of the current simulation step. */
	INT CurrentSimulationIndex;

	/* Number of cells to simulate in one dimension. */
	INT DetailResolution;

	/* World space size of the detail grid in one dimension. */
	FLOAT DetailSize;

	/* New detail position that will be used next Tick. */
	FVector PendingDetailPosition;

	/* Detail positions stored for the last 2 simulation steps, used to handle the grid moving. */
	FVector DetailPosition[NumHeightfields];

	/* Pixel formats for the fluid simulation textures */
	EPixelFormat FluidHeightTextureFormat;
	EPixelFormat FluidHeightSurfaceFormat;
	EPixelFormat FluidNormalFormat;

	/* Textures and surfaces for the simulated heightfields */
	FTexture2DRHIRef HeightTextures[NumHeightfields];
	FSurfaceRHIRef HeightSurfaces[NumHeightfields];

	/* Texture and surface for the generated normal map */
	FTexture2DRHIRef NormalTexture;
	FSurfaceRHIRef NormalSurface;

	/* Indicates whether render targets are dirty and need to be initialized on the first simulation step */
	UBOOL bRenderTargetContentsInitialized;

	UBOOL bFirstForceThisStep;

	/* Time rollover from last Tick that did not result in a simulation step */
	FLOAT TimeRollover;

public:

	FFluidGPUResource();

	/**
	 * Sets the size that simulation render targets will be allocated at.
	 * Must be called before initializing the resource.
	 */
	void SetSize(INT InDetailResolution, FLOAT InDetailSize);

	// FRenderResource interface.
	virtual void InitDynamicRHI();
	virtual void ReleaseDynamicRHI();

	/**
	 * Initializes the render target's contents to reasonable values
	 */
	void InitializeRenderTargetContents();

	/**
	 * Updates the GPU fluid surface simulation.
	 * @param DeltaTime - amount of time passed since last update
	 * @param FluidForces - array of forces that should be applied
	 * @param DetailUpdateRate - frequency of simulation steps
	 * @param DetailDamping - amount to uniformly dampen the simulation
	 * @param DetailTravelSpeed - wave travel speed factor
	 * @param DetailTransfer - amount of each force to apply
	 * @param DetailHeightScale - scale on the height before calculating normals
	 */
	void Tick(
		FLOAT DeltaTime, 
		const TArray<FFluidForce>& FluidForces, 
		FLOAT DetailUpdateRate,
		FLOAT DetailDamping,
		FLOAT DetailTravelSpeed,
		FLOAT DetailTransfer,
		FLOAT DetailHeightScale,
		UBOOL bTiling);

	/**
	 * Used to visualize the simulation results by rendering directly to the screen
	 */
	void Visualize(const FSceneView* View);

	/**
	 * Returns the amount of memory used by the detail grid render targets and textures, in bytes.
	 */
	INT GetRenderTargetMemorySize();

	// Accessors

	/**
	 * Gets the detail grid min and max in local space of the fluid surface actor.
	 */
	void GetDetailRect(FVector2D& DetailMin, FVector2D& DetailMax, UBOOL bEnableGPUSimulation) const;
	const FTexture2DRHIRef& GetNormalTexture() const { return NormalTexture; }
	void SetDetailPosition(const FVector& LocalPos, UBOOL bEnableGPUSimulation);

	/**
	 * @return The resource's friendly name.  Typically a UObject name.
	 */
	virtual FString GetFriendlyName() const { return TEXT("FFluidGPUResource"); }

private:

	/**
	 * Advances the simulation a step - must be called before any forces are applied for the next step
	 */
	void AdvanceStep();

	/**
	 * Applies a force to the fluid simulation
	 */
	void ApplyForce(const FFluidForceParams& FluidForceParams, UBOOL bApplyToCurrentHeight);

	/**
	 * Runs a simulation step on the fluid surface
	 */
	void Simulate(const FFluidSimulateParams& FluidSimulateParams);

	/**
	 * Generates normals now that the height simulation is complete
	 */
	void GenerateNormals(const FFluidNormalParams& FluidNormalParams);

	INT GetPreviousIndex1() const
	{
		return (CurrentSimulationIndex - 1 + NumHeightfields) % NumHeightfields;
	}

	INT GetPreviousIndex2() const
	{
		return (CurrentSimulationIndex - 2 + NumHeightfields) % NumHeightfields;
	}

	void BeginRenderingFluidHeight() const
	{
		RHISetRenderTarget(HeightSurfaces[CurrentSimulationIndex], NULL);
	}

	void FinishRenderingFluidHeight(FResolveParams ResolveParams) const
	{
		RHICopyToResolveTarget(HeightSurfaces[CurrentSimulationIndex], FALSE, ResolveParams);
	}

	void BeginRenderingScratchHeight() const
	{
		INT Index = GetPreviousIndex1();
		RHISetRenderTarget(HeightSurfaces[Index], NULL);
	}

	void FinishRenderingScratchHeight(FResolveParams ResolveParams) const
	{
		INT Index = GetPreviousIndex1();
		RHICopyToResolveTarget(HeightSurfaces[Index], FALSE, ResolveParams);
	}

	void BeginRenderingFluidNormals() const
	{
		RHISetRenderTarget(NormalSurface, NULL);
	}

	void FinishRenderingFluidNormals() const
	{
		RHICopyToResolveTarget(NormalSurface, FALSE, FResolveParams());
	}

	friend class FApplyForcePixelShader;
	friend class FFluidSimulatePixelShader;
	friend class FFluidNormalPixelShader;
	friend class FFluidApplyPixelShader;
};

#endif
