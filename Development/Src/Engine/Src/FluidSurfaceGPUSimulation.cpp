/*=============================================================================
	FluidSurfaceGPUSimulation.cpp - Fluid surface simulation on the GPU.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "ScenePrivate.h"
#include "SceneFilterRendering.h"
#include "FluidSurfaceGPUSimulation.h"

#if XBOX
#include "ScreenRendering.h"
#endif // XBOX


FFluidGPUResource::FFluidGPUResource() :
	CurrentSimulationIndex(0),
	bRenderTargetContentsInitialized(FALSE),
	bFirstForceThisStep(FALSE),
	TimeRollover(0.0f)
{
	for (INT i = 0; i < NumHeightfields; i++)
	{
		DetailPosition[i].Set(0, 0, 0);
	}
	PendingDetailPosition.Set( 0, 0, 0 );


	// x and y of the GPU simulated detail grid normal, needs to be filterable.
	FluidNormalFormat = PF_G16R16F_FILTER;

#if XBOX
	// On xbox we will render to R32F and resolve to a R16F texture
	FluidHeightTextureFormat = PF_R16F;
	// Ideally we would want to use a one channel, high precision format that is alpha blendable for applying forces.
	FluidHeightSurfaceFormat = PF_R32F;
#elif PLATFORM_SUPPORTS_D3D10_PLUS
	// We only need one component, but some cards don't support fp16x1 textures (e.g., NV 7800 and NV 6800)
	FluidHeightTextureFormat = PF_G16R16F;
	FluidHeightSurfaceFormat = PF_G16R16F;
#else
	FluidHeightTextureFormat = PF_R32F;

	// Ideally we would want to use a one channel, high precision format that is alpha blendable for applying forces.
	FluidHeightSurfaceFormat = PF_R32F;
#endif
}

void FFluidGPUResource::SetSize(INT InDetailResolution, FLOAT InDetailSize)
{
	// Can't change these while the render targets are allocated
	check(!bInitialized);
	DetailResolution = InDetailResolution;
	DetailSize = InDetailSize;
}

void FFluidGPUResource::InitDynamicRHI() 
{
	for (INT i = 0; i < NumHeightfields; i++)
	{
		HeightTextures[i] = RHICreateTexture2D(DetailResolution, DetailResolution, FluidHeightTextureFormat, 1, TexCreate_ResolveTargetable, NULL);
		HeightSurfaces[i] = RHICreateTargetableSurface(DetailResolution, DetailResolution, FluidHeightSurfaceFormat, HeightTextures[i], TargetSurfCreate_None, TEXT("FluidHeight"));
	}

	// D3D11 will generate mips from the normal surface so we will need to create it with mip maps
	const INT NumMips = (GRHIShaderPlatform == SP_PCD3D_SM5) ? appCeilLogTwo(DetailResolution) : 1;
	const INT ExtraFlags = (NumMips == 1) ? 0 : TexCreate_GenerateMipCapable;
	NormalTexture = RHICreateTexture2D(DetailResolution, DetailResolution, FluidNormalFormat, NumMips, TexCreate_ResolveTargetable | ExtraFlags, NULL);
	NormalSurface = RHICreateTargetableSurface(DetailResolution, DetailResolution, FluidNormalFormat, NormalTexture, ExtraFlags, TEXT("FluidNormal"));

	// Mark the render targets as uninitialized
	bRenderTargetContentsInitialized = FALSE;
}

void FFluidGPUResource::ReleaseDynamicRHI() 
{
	for (INT i = 0; i < NumHeightfields; i++)
	{
		HeightTextures[i].SafeRelease();
		HeightSurfaces[i].SafeRelease();
	}

	NormalTexture.SafeRelease();
	NormalSurface.SafeRelease();
}

/**
 * Initializes the render target's contents to reasonable values
 */
void FFluidGPUResource::InitializeRenderTargetContents()
{
	if (!bRenderTargetContentsInitialized)
	{
		bRenderTargetContentsInitialized = TRUE;

		for (INT i = 0; i < NumHeightfields; i++)
		{
			RHISetRenderTarget(HeightSurfaces[i], NULL);
			RHIClear(TRUE, FLinearColor::Black, FALSE, 0, FALSE, 0);
			RHICopyToResolveTarget(HeightSurfaces[i], FALSE, FResolveParams());
		}

		RHISetRenderTarget(NormalSurface, NULL);
		RHIClear(TRUE, FLinearColor(0, 0, 1), FALSE, 0, FALSE, 0);
		RHICopyToResolveTarget(NormalSurface, FALSE, FResolveParams());

		if (GIsEditor)
		{
			// Set the current render target back to scene color since we may be in the middle of the Base Pass or Translucent Pass in the editor
			// In game InitializeRenderTargetContents will only ever be called from a render command enqueued from the game thread tick.
			GSceneRenderTargets.BeginRenderingSceneColor(RTUsage_Default);
		}
	}
}

/**
 * Advances the simulation a step - must be called before any forces are applied for the next step
 */
void FFluidGPUResource::AdvanceStep()
{
	CurrentSimulationIndex = (CurrentSimulationIndex + 1) % NumHeightfields;

	// Update the detail position and align it to cell boundaries. 
	// This ensures that the simulation will remain stable even when the grid is moving and we are using point filtering.
	// Not aligning the detail position would require linear filtering on previous heightfields during the simulation.
	const FLOAT Divisor = DetailSize / DetailResolution;
	DetailPosition[CurrentSimulationIndex].X = PendingDetailPosition.X - appFmod(PendingDetailPosition.X, Divisor);
	DetailPosition[CurrentSimulationIndex].Y = PendingDetailPosition.Y - appFmod(PendingDetailPosition.Y, Divisor);
	DetailPosition[CurrentSimulationIndex].Z = PendingDetailPosition.Z - appFmod(PendingDetailPosition.Z, Divisor);

	bFirstForceThisStep = TRUE;
}

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
void FFluidGPUResource::Tick(
	FLOAT DeltaTime, 
	const TArray<FFluidForce>& FluidForces, 
	FLOAT DetailUpdateRate,
	FLOAT DetailDamping, 
	FLOAT DetailTravelSpeed,
	FLOAT DetailTransfer,
	FLOAT DetailHeightScale,
	UBOOL bTiling)
{
	check(IsInRenderingThread());

	// The simulation requires fixed time steps to remain stable
	FLOAT TimeStep = 1.0f / DetailUpdateRate;
	TimeRollover += DeltaTime;
	INT NumIterations = appTrunc( TimeRollover / TimeStep );
	TimeRollover -= NumIterations * TimeStep;
	// Clamp the number of iterations to a reasonable maximum
	//@todo - need a mechanism to throttle back fluid updating to keep from going into a 'well of despair'
	// since the amount of simulation work is proportional to the previous frame's duration.
	NumIterations = Min(NumIterations, 16);
	const FLOAT ForceFactor = DetailSize * DetailTransfer / (DetailUpdateRate * DetailResolution * PI);

	if (NumIterations == 0 && FluidForces.Num() > 0)
	{
		RHIBeginScene();

		InitializeRenderTargetContents();

		SCOPED_DRAW_EVENT(FluidEventView)(DEC_SCENE_ITEMS,TEXT("ApplyImpulseForces"));

		bFirstForceThisStep = TRUE;
		// Apply impulse forces even if not enough time has passed to run a simulation step.
		// These forces will be applied to the current heightfield, which will be the previous heightfield on the next simulation step.
		for (INT i = 0; i < FluidForces.Num(); i++)
		{
			const FFluidForce& FluidForce = FluidForces(i);
			if (FluidForce.bImpulse)
			{
				FLOAT Force = ForceFactor * FluidForce.Strength / FluidForce.Radius;
				// Transform the force position from local space (origin at the fluid surface actor)
				// to [0,1] texture coordinates for indexing the detail grid.
				FVector DetailForcePos = (FluidForce.LocalPos - DetailPosition[CurrentSimulationIndex] + FVector(.5f * DetailSize)) / DetailSize;
				FFluidForceParams ForceParams(DetailForcePos, FluidForce.Radius / DetailSize, Force);
				ApplyForce(ForceParams, TRUE);
			}
		}

		RHIEndScene();
	}
	else if (NumIterations > 0)
	{
		// We are running on the rendering thread as a result of a queued command from the gamethread Tick,
		// so we are not between a BeginScene()-EndScene() pair.
		RHIBeginScene();

		InitializeRenderTargetContents();

		// Allocate more GPR's for pixel shaders
		RHISetShaderRegisterAllocation(32, 96);

		// Cycle buffers so that we are reading from previous frame's heights and writing to an unused heightfield.
		AdvanceStep();

		{
			SCOPED_DRAW_EVENT(FluidEventView)(DEC_SCENE_ITEMS,TEXT("ApplyImpulseForces"));

			// Apply all of the queued impulse forces once
			for (INT i = 0; i < FluidForces.Num(); i++)
			{
				const FFluidForce& FluidForce = FluidForces(i);
				if (FluidForce.bImpulse)
				{
					FLOAT Force = ForceFactor * FluidForce.Strength / FluidForce.Radius;
					// Transform the force position from local space (origin at the fluid surface actor)
					// to [0,1] texture coordinates for indexing the detail grid.
					// Use last simulation step's detail position since the grid may have moved.
					FVector DetailForcePos = (FluidForce.LocalPos - DetailPosition[GetPreviousIndex1()] + FVector(.5f * DetailSize)) / DetailSize;
					FFluidForceParams ForceParams(DetailForcePos, FluidForce.Radius / DetailSize, Force);
					ApplyForce(ForceParams, FALSE);
				}
			}
		}

		// Run each simulation step
		for (INT Iteration = 0; Iteration < NumIterations; ++Iteration)
		{
			if (Iteration > 0)
			{
				// Cycle buffers so that we are reading from previous frame's heights
				AdvanceStep();
			}

			{
				SCOPED_DRAW_EVENT(FluidEventView)(DEC_SCENE_ITEMS,TEXT("ApplyContinuousForces"));

				// Apply all of the queued continuous forces
				for (INT i = 0; i < FluidForces.Num(); i++)
				{
					const FFluidForce& FluidForce = FluidForces(i);
					if (!FluidForce.bImpulse)
					{
						FLOAT Force = ForceFactor * FluidForce.Strength / FluidForce.Radius;
						// Transform the force position from local space (origin at the fluid surface actor)
						// to [0,1] texture coordinates for indexing the detail grid.
						// Use last simulation step's detail position since the grid may have moved.
						FVector DetailForcePos = (FluidForce.LocalPos - DetailPosition[GetPreviousIndex1()] + FVector(.5f * DetailSize)) / DetailSize;
						FFluidForceParams ForceParams(DetailForcePos, FluidForce.Radius / DetailSize, Force);
						ApplyForce(ForceParams, FALSE);
					}
				}
			}

			// Simulate a step
			const FLOAT DampFactor = Clamp<FLOAT>(1.0f - (DetailDamping / 30.0f), 0.0f, 1.0f);
			FFluidSimulateParams SimulateParams(DampFactor, DetailTravelSpeed, bTiling);
			Simulate(SimulateParams);
		}

		// Generate normals off of the simulated heightfield
		FFluidNormalParams NormalParams(DetailHeightScale, bTiling);
		GenerateNormals(NormalParams);

		// Restore GPR allocations
		RHISetShaderRegisterAllocation(64, 64);

		RHIEndScene();
	}
	else if (!bRenderTargetContentsInitialized)
	{
		// so we are not between a BeginScene()-EndScene() pair.
		RHIBeginScene();
		InitializeRenderTargetContents();
		RHIEndScene();
	}
}

/*-----------------------------------------------------------------------------
	FFluidVertexShader
-----------------------------------------------------------------------------*/

class FFluidVertexShader : public FShader
{
	DECLARE_SHADER_TYPE(FFluidVertexShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE; 
	}

	FFluidVertexShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
	}

	FFluidVertexShader() {}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		return FShader::Serialize(Ar);;
	}

private:
	FShaderParameter HalfSceneColorTexelSizeParameter;
};

IMPLEMENT_SHADER_TYPE(,FFluidVertexShader,TEXT("FluidSurfaceSimulation"),TEXT("VertexMain"),SF_Vertex,0,0);


/*-----------------------------------------------------------------------------
	FApplyForcePixelShader
-----------------------------------------------------------------------------*/

class FApplyForcePixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FApplyForcePixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE; 
	}

	FApplyForcePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		ForcePositionParameter.Bind(Initializer.ParameterMap,TEXT("ForcePosition"), TRUE);
		ForceRadiusParameter.Bind(Initializer.ParameterMap,TEXT("ForceRadius"), TRUE);
		ForceMagnitudeParameter.Bind(Initializer.ParameterMap,TEXT("ForceMagnitude"), TRUE);
		PreviousHeights1Parameter.Bind(Initializer.ParameterMap,TEXT("PreviousHeights1"), TRUE);
	}

	FApplyForcePixelShader() {}

	void SetParameters(const FFluidGPUResource& FluidResource, const FFluidForceParams& FluidForceParams, UBOOL bApplyToCurrentHeight)
	{
		FVector2D ForcePosition(FluidForceParams.Position.X, FluidForceParams.Position.Y);
		SetPixelShaderValue(GetPixelShader(), ForcePositionParameter, ForcePosition);

		SetPixelShaderValue(GetPixelShader(), ForceRadiusParameter, FluidForceParams.Radius);

		SetPixelShaderValue(GetPixelShader(), ForceMagnitudeParameter, FluidForceParams.Strength);

		FTexture2DRHIRef ApplyForceHeightfield;

		if (bApplyToCurrentHeight)
		{
			ApplyForceHeightfield = FluidResource.HeightTextures[FluidResource.CurrentSimulationIndex];
		}
		else
		{
			ApplyForceHeightfield = FluidResource.HeightTextures[FluidResource.GetPreviousIndex1()];
		}

		SetTextureParameter(
			GetPixelShader(),
			PreviousHeights1Parameter,
			TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			ApplyForceHeightfield
			);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << ForcePositionParameter;
		Ar << ForceRadiusParameter;
		Ar << ForceMagnitudeParameter;
		Ar << PreviousHeights1Parameter;
		return bShaderHasOutdatedParameters;
	}

private:

	FShaderParameter ForcePositionParameter;
	FShaderParameter ForceRadiusParameter;
	FShaderParameter ForceMagnitudeParameter;
	FShaderResourceParameter PreviousHeights1Parameter;
};

IMPLEMENT_SHADER_TYPE(,FApplyForcePixelShader,TEXT("FluidSurfaceSimulation"),TEXT("ApplyForcePixelMain"),SF_Pixel,0,0);

FGlobalBoundShaderState ForceCopyBoundShaderState;
FGlobalBoundShaderState ApplyForceBoundShaderState;

extern TGlobalResource<FFilterVertexDeclaration> GFilterVertexDeclaration;

/**
 * Applies a force to the fluid simulation
 */
void FFluidGPUResource::ApplyForce(const FFluidForceParams& FluidForceParams, UBOOL bApplyToCurrentHeight)
{
	// Calculate the bounding rect of the force
	FLOAT MinX = (FluidForceParams.Position.X - FluidForceParams.Radius) * DetailResolution;
	FLOAT MinY = (FluidForceParams.Position.Y - FluidForceParams.Radius) * DetailResolution;
	FLOAT MaxX = (FluidForceParams.Position.X + FluidForceParams.Radius) * DetailResolution;
	FLOAT MaxY = (FluidForceParams.Position.Y + FluidForceParams.Radius) * DetailResolution;

	// Reject forces that are completely outside the detail grid.
	if ( MinX >= DetailResolution || MaxX <= 0.0f || MinY >= DetailResolution || MaxY <= 0.0f )
	{
		return;
	}

	// Clamp the rect so that there is a one texel boundary of unsimulated cells
	MinX = Max(MinX, 1.0f);
	MinY = Max(MinY, 1.0f);
	MaxX = Min(MaxX, DetailResolution - 1.0f);
	MaxY = Min(MaxY, DetailResolution - 1.0f);
	FLOAT SizeX = MaxX - MinX;
	FLOAT SizeY = MaxY - MinY;

	// Only continue if the force rect is valid
	if (SizeX <= 0.0f || SizeY <= 0.0f)
	{
		return;
	}

	// Apply a force by copying the old height texture to itself and adding an offset
	// proportional to the force strength and radius
	if (bApplyToCurrentHeight)
	{
		BeginRenderingFluidHeight();
	}
	else
	{
		BeginRenderingScratchHeight();
	}

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	// Disable depth test and writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());

	// PC applies the force through alpha blending
	if (IsPCPlatform(GRHIShaderPlatform))
	{
		RHISetBlendState(TStaticBlendState<BO_Add,BF_One,BF_One>::GetRHI());
	}
	else
	{
		RHISetBlendState(TStaticBlendState<>::GetRHI());
	}

	RHISetViewport(0, 0, 0.0f, DetailResolution, DetailResolution, 1.0f);	

	TShaderMapRef<FFluidVertexShader> VertexShader(GetGlobalShaderMap());

#if XBOX
 	// The first force in the simulation step needs to copy the heightfield into EDRAM so that the following forces don't have to render to the entire simulation grid.
 	if (bFirstForceThisStep)
 	{
 		TShaderMapRef<FScreenPixelShader> ScreenPixelShader(GetGlobalShaderMap());
 		SetGlobalBoundShaderState(ForceCopyBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *ScreenPixelShader, sizeof(FFilterVertex));
 
 		FTexture ForceCopyTexture;
 		if (bApplyToCurrentHeight)
 		{
 			ForceCopyTexture.TextureRHI = HeightTextures[CurrentSimulationIndex];
 		}
 		else
 		{
 			ForceCopyTexture.TextureRHI = HeightTextures[GetPreviousIndex1()];
 		}
 
 		ForceCopyTexture.SamplerStateRHI = TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI();
 		ScreenPixelShader->SetParameters(&ForceCopyTexture);
 
 		DrawDenormalizedQuad(
 			0, 0,
 			DetailResolution, DetailResolution,
 			0, 0,
 			DetailResolution, DetailResolution,
 			DetailResolution, DetailResolution,
 			DetailResolution, DetailResolution
 			);
 
 		bFirstForceThisStep = FALSE;
 	}
#endif

	TShaderMapRef<FApplyForcePixelShader> PixelShader(GetGlobalShaderMap());
	SetGlobalBoundShaderState(ApplyForceBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));
	PixelShader->SetParameters(*this, FluidForceParams, bApplyToCurrentHeight);

	// Render the bounding rectangle of the force
	DrawDenormalizedQuad(
		MinX, MinY,
		SizeX, SizeY,
		MinX, MinY,
		SizeX, SizeY,
		DetailResolution, DetailResolution,
		DetailResolution, DetailResolution
		);

	// Only resolve the bounding rectangle of the force
	FResolveParams ResolveParams(FResolveRect(appTrunc(MinX), appTrunc(MinY), appTrunc(MinX + SizeX), appTrunc(MinY + SizeY)));

	if (bApplyToCurrentHeight)
	{
		FinishRenderingFluidHeight(ResolveParams);
	}
	else
	{
		FinishRenderingScratchHeight(ResolveParams);
	}
}

/*-----------------------------------------------------------------------------
	FFluidSimulatePixelShader
-----------------------------------------------------------------------------*/

class FFluidSimulatePixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FFluidSimulatePixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE; 
	}

	FFluidSimulatePixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		CellSizeParameter.Bind(Initializer.ParameterMap,TEXT("CellSize"), TRUE);
		DampFactorParameter.Bind(Initializer.ParameterMap,TEXT("DampFactor"), TRUE);
		TravelSpeedParameter.Bind(Initializer.ParameterMap,TEXT("TravelSpeed"), TRUE);
		PreviousOffset1Parameter.Bind(Initializer.ParameterMap,TEXT("PreviousOffset1"), TRUE);
		PreviousOffset2Parameter.Bind(Initializer.ParameterMap,TEXT("PreviousOffset2"), TRUE);
		PreviousHeights1Parameter.Bind(Initializer.ParameterMap,TEXT("PreviousHeights1"), TRUE);
		PreviousHeights2Parameter.Bind(Initializer.ParameterMap,TEXT("PreviousHeights2"), TRUE);
	}

	FFluidSimulatePixelShader() {}

	void SetParameters(const FFluidGPUResource& FluidResource, const FFluidSimulateParams& FluidSimulateParams)
	{
		const FLOAT InvHeightfieldResolution = 1.0f / (FLOAT)FluidResource.DetailResolution;
		FVector2D CellSize(InvHeightfieldResolution, InvHeightfieldResolution);
		SetPixelShaderValue(GetPixelShader(), CellSizeParameter, CellSize);

		SetPixelShaderValue(GetPixelShader(), DampFactorParameter, FluidSimulateParams.DampingFactor);

		SetPixelShaderValue(GetPixelShader(), TravelSpeedParameter, FluidSimulateParams.TravelSpeed);

		// Calculate the offset that the detail position was at last simulation step.
		// This will be used when looking up values from that heightfield, to take movement into account.
		const FVector& CurrentDetailPos = FluidResource.DetailPosition[FluidResource.CurrentSimulationIndex];
		const FVector& Previous1DetailPos = FluidResource.DetailPosition[FluidResource.GetPreviousIndex1()];
		const FLOAT InvDetailSize = 1.0f / FluidResource.DetailSize;
		FVector2D Previous1Offset(
			(Previous1DetailPos.X - CurrentDetailPos.X) * InvDetailSize, 
			(Previous1DetailPos.Y - CurrentDetailPos.Y) * InvDetailSize);
		SetPixelShaderValue(GetPixelShader(), PreviousOffset1Parameter, Previous1Offset);

		const FVector& Previous2DetailPos = FluidResource.DetailPosition[FluidResource.GetPreviousIndex2()];
		FVector2D Previous2Offset(
			(Previous2DetailPos.X - CurrentDetailPos.X) * InvDetailSize, 
			(Previous2DetailPos.Y - CurrentDetailPos.Y) * InvDetailSize);
		SetPixelShaderValue(GetPixelShader(), PreviousOffset2Parameter, Previous2Offset);

		// Linear filtering is not needed since we align the detail position to cell sizes and therefore always sample from texel centers.
		SetTextureParameter(
			GetPixelShader(),
			PreviousHeights1Parameter,
			FluidSimulateParams.bTiling ? 
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI() :
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FluidResource.HeightTextures[FluidResource.GetPreviousIndex1()]
			);

		SetTextureParameter(
			GetPixelShader(),
			PreviousHeights2Parameter,
			FluidSimulateParams.bTiling ? 
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI() :
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FluidResource.HeightTextures[FluidResource.GetPreviousIndex2()]
			);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << CellSizeParameter;
		Ar << DampFactorParameter;
		Ar << TravelSpeedParameter;
		Ar << PreviousOffset1Parameter;
		Ar << PreviousOffset2Parameter;
		Ar << PreviousHeights1Parameter;
		Ar << PreviousHeights2Parameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter CellSizeParameter;
	FShaderParameter DampFactorParameter;
	FShaderParameter TravelSpeedParameter;
	FShaderParameter PreviousOffset1Parameter;
	FShaderParameter PreviousOffset2Parameter;
	FShaderResourceParameter PreviousHeights1Parameter;
	FShaderResourceParameter PreviousHeights2Parameter;
};

IMPLEMENT_SHADER_TYPE(,FFluidSimulatePixelShader,TEXT("FluidSurfaceSimulation"),TEXT("SimulatePixelMain"),SF_Pixel,VER_FLUID_DETAIL_UPDATE2,0);

FGlobalBoundShaderState FluidSimulateBoundShaderState;

/**
 * Runs a simulation step on the fluid surface
 */
void FFluidGPUResource::Simulate(const FFluidSimulateParams& FluidSimulateParams)
{
	SCOPED_DRAW_EVENT(FluidEventView)(DEC_SCENE_ITEMS,TEXT("FluidSimulate"));

	BeginRenderingFluidHeight();

#if XBOX
	// The Simulation doesn't write to the border pixels but they still have to be cleared,
	// (since we're resolving the whole surface).
	RHIClear( TRUE, FLinearColor::Black, FALSE, 0, FALSE, 0 );
#endif

	TShaderMapRef<FFluidVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FFluidSimulatePixelShader> PixelShader(GetGlobalShaderMap());

	SetGlobalBoundShaderState(FluidSimulateBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	// Disable depth test and writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	RHISetViewport(0, 0, 0.0f, DetailResolution, DetailResolution, 1.0f);				

	PixelShader->SetParameters(*this, FluidSimulateParams);

	// Leave a one texel boundary of unsimulated cells
	const INT Padding = FluidSimulateParams.bTiling ? 0 : 1;

	DrawDenormalizedQuad(
		Padding, Padding,
		DetailResolution - 2 * Padding, DetailResolution - 2 * Padding,
		Padding, Padding,
		DetailResolution - 2 * Padding, DetailResolution - 2 * Padding,
		DetailResolution, DetailResolution,
		DetailResolution, DetailResolution
		);

	FinishRenderingFluidHeight(FResolveParams());
}

/*-----------------------------------------------------------------------------
	FFluidNormalPixelShader
-----------------------------------------------------------------------------*/

class FFluidNormalPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FFluidNormalPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE; 
	}

	FFluidNormalPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		CellSizeParameter.Bind(Initializer.ParameterMap,TEXT("CellSize"), TRUE);
		HeightScaleParameter.Bind(Initializer.ParameterMap,TEXT("HeightScale"), TRUE);
		FluidHeightTextureParameter.Bind(Initializer.ParameterMap,TEXT("HeightTexture"), TRUE);
		SplineMarginParameter.Bind(Initializer.ParameterMap,TEXT("SplineMargin"), TRUE);
	}

	FFluidNormalPixelShader() {}

	void SetParameters(const FFluidGPUResource& FluidResource, const FFluidNormalParams& FluidNormalParams)
	{
		const FVector2D CellSize(1.0f / (FLOAT)FluidResource.DetailResolution, 1.0f / (FLOAT)FluidResource.DetailResolution);
		SetPixelShaderValue(GetPixelShader(), CellSizeParameter, CellSize);

		SetPixelShaderValue(GetPixelShader(), HeightScaleParameter, FluidNormalParams.HeightScale);

		const FLOAT SplineMargin = FluidNormalParams.bTiling ? DELTA : .3f;
		SetPixelShaderValue(GetPixelShader(), SplineMarginParameter, SplineMargin);

		SetTextureParameter(
			GetPixelShader(),
			FluidHeightTextureParameter,
			FluidNormalParams.bTiling ? 
				TStaticSamplerState<SF_Point,AM_Wrap,AM_Wrap,AM_Wrap>::GetRHI() :
				TStaticSamplerState<SF_Point,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FluidResource.HeightTextures[FluidResource.CurrentSimulationIndex]
			);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << CellSizeParameter;
		Ar << HeightScaleParameter;
		Ar << FluidHeightTextureParameter;
		Ar << SplineMarginParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderParameter CellSizeParameter;
	FShaderParameter HeightScaleParameter;
	FShaderParameter SplineMarginParameter;
	FShaderResourceParameter FluidHeightTextureParameter;
};

IMPLEMENT_SHADER_TYPE(,FFluidNormalPixelShader,TEXT("FluidSurfaceSimulation"),TEXT("NormalPixelMain"),SF_Pixel,VER_FLUID_DETAIL_UPDATE,0);

FGlobalBoundShaderState GenerateNormalBoundShaderState;

UBOOL DWGenerateMips = TRUE;

/**
 * Generates normals now that the height simulation is complete
 */
void FFluidGPUResource::GenerateNormals(const FFluidNormalParams& FluidNormalParams)
{
	SCOPED_DRAW_EVENT(FluidEventView)(DEC_SCENE_ITEMS,TEXT("GenNormals"));
	BeginRenderingFluidNormals();

	TShaderMapRef<FFluidVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FFluidNormalPixelShader> PixelShader(GetGlobalShaderMap());

	SetGlobalBoundShaderState(GenerateNormalBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	// Disable depth test and writes
	RHISetDepthState(TStaticDepthState<FALSE,CF_Always>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());

	RHISetViewport(0, 0, 0.0f, DetailResolution, DetailResolution, 1.0f);				

	PixelShader->SetParameters(*this, FluidNormalParams);

	// Generate normals, but leave a one texel boundary of default normals pointing up in tangent space.
	const INT Padding = FluidNormalParams.bTiling ? 0 : 1;

	DrawDenormalizedQuad(
		Padding, Padding,
		DetailResolution - 2 * Padding, DetailResolution - 2 * Padding,
		Padding, Padding,
		DetailResolution - 2 * Padding, DetailResolution - 2 * Padding,
		DetailResolution, DetailResolution,
		DetailResolution, DetailResolution
		);

	if (GNumActiveGPUsForRendering > 1)
	{
		// Force disable fluid simulation by clearing the normal to an upright normal
		// Currently the driver doesn't detect that it needs to transfer the previous frame's heightfields between GPUs,
		// And fluid surfaces just flicker.
		RHIClear(TRUE, FLinearColor(0, 0, 1), FALSE, 0, FALSE, 0);
	}

	FinishRenderingFluidNormals();

#if PLATFORM_SUPPORTS_D3D10_PLUS
	if (DWGenerateMips)
	{
		RHIGenerateMips(NormalSurface);
	}
#endif
}

/*-----------------------------------------------------------------------------
	FFluidApplyPixelShader
-----------------------------------------------------------------------------*/

class FFluidApplyPixelShader : public FShader
{
	DECLARE_SHADER_TYPE(FFluidApplyPixelShader,Global);
public:

	static UBOOL ShouldCache(EShaderPlatform Platform) 
	{ 
		return TRUE; 
	}

	FFluidApplyPixelShader(const ShaderMetaType::CompiledShaderInitializerType& Initializer):
		FShader(Initializer)
	{
		FluidHeightTextureParameter.Bind(Initializer.ParameterMap,TEXT("HeightTexture"), TRUE);
		FluidNormalTextureParameter.Bind(Initializer.ParameterMap,TEXT("NormalTexture"), TRUE);
	}

	FFluidApplyPixelShader() {}

	void SetParameters(const FFluidGPUResource& FluidResource)
	{
		SetTextureParameter(
			GetPixelShader(),
			FluidHeightTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FluidResource.HeightTextures[FluidResource.CurrentSimulationIndex]
			);

		SetTextureParameter(
			GetPixelShader(),
			FluidNormalTextureParameter,
			TStaticSamplerState<SF_Bilinear,AM_Clamp,AM_Clamp,AM_Clamp>::GetRHI(),
			FluidResource.NormalTexture
			);
	}

	virtual UBOOL Serialize(FArchive& Ar)
	{
		UBOOL bShaderHasOutdatedParameters = FShader::Serialize(Ar);
		Ar << FluidHeightTextureParameter;
		Ar << FluidNormalTextureParameter;
		return bShaderHasOutdatedParameters;
	}

private:
	FShaderResourceParameter FluidHeightTextureParameter;
	FShaderResourceParameter FluidNormalTextureParameter;
};

IMPLEMENT_SHADER_TYPE(,FFluidApplyPixelShader,TEXT("FluidSurfaceSimulation"),TEXT("ApplyPixelMain"),SF_Pixel,VER_FLUID_DETAIL_UPDATE,0);

FGlobalBoundShaderState FluidApplyBoundShaderState;

/**
 * Used to visualize the simulation results by rendering directly to the screen
 */
void FFluidGPUResource::Visualize(const FSceneView* View)
{
	RHISetRasterizerState(TStaticRasterizerState<FM_Solid,CM_None>::GetRHI());
	RHISetBlendState(TStaticBlendState<>::GetRHI());
	RHISetColorWriteMask(CW_RED|CW_GREEN|CW_BLUE);

	const INT SimScale = 4;

	TShaderMapRef<FFluidVertexShader> VertexShader(GetGlobalShaderMap());
	TShaderMapRef<FFluidApplyPixelShader> PixelShader(GetGlobalShaderMap());
	PixelShader->SetParameters(*this);
	SetGlobalBoundShaderState(FluidApplyBoundShaderState, GFilterVertexDeclaration.VertexDeclarationRHI, *VertexShader, *PixelShader, sizeof(FFilterVertex));

	DrawDenormalizedQuad(
		0, 0,
		View->RenderTargetSizeX / SimScale, View->RenderTargetSizeY / SimScale,
		0, 0,
		DetailResolution, DetailResolution,
		View->RenderTargetSizeX, View->RenderTargetSizeY,
		DetailResolution, DetailResolution
		);

	RHISetColorWriteMask(CW_RGBA);
}

/**
 * Returns the amount of memory used by the detail grid render targets and textures, in bytes.
 */
INT FFluidGPUResource::GetRenderTargetMemorySize()
{
	INT HeightfieldBytes = DetailResolution * DetailResolution * GPixelFormats[FluidHeightTextureFormat].BlockBytes * NumHeightfields;
	INT NormalBytes = DetailResolution * DetailResolution * GPixelFormats[FluidNormalFormat].BlockBytes;
	return HeightfieldBytes + NormalBytes;
}

/**
 * Gets the detail grid min and max in local space of the fluid surface actor.
 */
void FFluidGPUResource::GetDetailRect(FVector2D& DetailMin, FVector2D& DetailMax, UBOOL bEnableGPUSimulation) const
{
	if ( bEnableGPUSimulation )
	{
		FVector Min(DetailPosition[CurrentSimulationIndex] - FVector(.5f * DetailSize));
		FVector Max(DetailPosition[CurrentSimulationIndex] + FVector(.5f * DetailSize));
		DetailMin = FVector2D(Min.X, Min.Y);
		DetailMax = FVector2D(Max.X, Max.Y);
	}
	else
	{
		FVector Min(PendingDetailPosition - FVector(0.5f * DetailSize));
		FVector Max(PendingDetailPosition + FVector(0.5f * DetailSize));
		DetailMin = FVector2D(Min.X, Min.Y);
		DetailMax = FVector2D(Max.X, Max.Y);
	}
}


void FFluidGPUResource::SetDetailPosition(const FVector& LocalPos, UBOOL bEnableGPUSimulation)
{
	PendingDetailPosition = LocalPos;
	if ( !bRenderTargetContentsInitialized )
	{
		// Propagate the new position directly to the simulation, since we're not updating.
		for ( INT HeightMapIndex=0; HeightMapIndex < NumHeightfields; ++HeightMapIndex )
		{
			AdvanceStep();
		}
	}
}
