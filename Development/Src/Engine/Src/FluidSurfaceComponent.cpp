/*=============================================================================
	FluidComponent.cpp: Fluid surface component.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineFluidClasses.h"
#include "FluidSurface.h"
#include "Engine.h"


IMPLEMENT_CLASS(UFluidSurfaceComponent);


// How many seconds until a fluid can deactivate, given the right conditions.
#define DEACTIVATION_TIME	3.0f

/** When this is TRUE (also controlled by the TOGGLEFLUIDS exec command), all fluids will be forced to be deactivated. */
UBOOL GForceFluidDeactivation = FALSE;

/*=============================================================================
	UFluidSurfaceComponent implementation
=============================================================================*/

UFluidSurfaceComponent::UFluidSurfaceComponent()
{
	FluidSimulation = NULL;
}

UMaterialInterface* UFluidSurfaceComponent::GetMaterial() const
{
	if ( FluidMaterial )
	{
		return FluidMaterial;
	}
	else
	{
		return GEngine->DefaultMaterial;
	}
}

FMaterialViewRelevance UFluidSurfaceComponent::GetMaterialViewRelevance() const
{
	return GetMaterial()->GetViewRelevance();
}

const FFluidGPUResource* UFluidSurfaceComponent::GetFluidGPUResource() const
{
	return FluidSimulation ? FluidSimulation->GetGPUResource() : NULL;
}

// UPrimitiveComponent interface.

void UFluidSurfaceComponent::Attach()
{
	Super::Attach();

	if ( FluidSimulation == NULL )
	{
		InitResources( FALSE );
	}

	// Add the fluid surface to the scene.
	if (Scene)
	{
		Scene->AddFluidSurface(this);
	}
}

void UFluidSurfaceComponent::Detach( UBOOL bWillReattach  )
{
	Super::Detach( bWillReattach );

	// Remove the fluid surface from the scene.
	if( Scene )
	{
		Scene->RemoveFluidSurface(this);
	}
}


UBOOL UFluidSurfaceComponent::PropertyNeedsResourceRecreation(UProperty* Property)
{
	if ( IsTemplate() )
	{
		return FALSE;
	}
	if ( Property == NULL ||
		(appStrcmp( *Property->GetName(), TEXT("bPause")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("NormalLength")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("bShowDetailPosition")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("bShowSimulationPosition")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("LightingContrast")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("bShowFluidSimulation")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("bShowDetailNormals")) != 0 &&
		appStrcmp( *Property->GetName(), TEXT("bShowFluidDetail")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("GPUTessellationFactor")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("TargetDetail")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("TargetSimulation")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("DetailUpdateRate")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("DetailDamping")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("FluidTravelSpeed")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("DetailTravelSpeed")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("DetailTransfer")) != 0  && 
		appStrcmp( *Property->GetName(), TEXT("DetailHeightScale")) != 0))
	{
		return TRUE;
	}
	return FALSE;
}

void UFluidSurfaceComponent::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);

	if ( PropertyNeedsResourceRecreation(PropertyAboutToChange) )
	{
		ReleaseResources(TRUE);
	}
}

void UFluidSurfaceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	GPUTessellationFactor = Clamp<FLOAT>(GPUTessellationFactor, 0.01f, 100.0f);
	FluidTravelSpeed = Clamp<FLOAT>(FluidTravelSpeed, 0.0f, 1.0f);
	DetailTravelSpeed = Clamp<FLOAT>(DetailTravelSpeed, 0.0f, 1.0f);

	Super::PostEditChangeProperty(PropertyChangedEvent);

	if( LightMapResolution > 0 )
	{
		LightMapResolution = Max(LightMapResolution + 3 & ~3,4);
	}
	else
	{
		LightMapResolution = 0;
	}

	FComponentReattachContext Reattach(this);
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if ( PropertyNeedsResourceRecreation(PropertyThatChanged) )
	{
		UBOOL bIsActive = FluidSimulation ? FluidSimulation->IsActive() : FALSE;
		InitResources( bIsActive );
	}
} 

/** 
 * Signals to the object to begin asynchronously releasing resources
 */
void UFluidSurfaceComponent::BeginDestroy()
{
	Super::BeginDestroy();
	ReleaseResources(FALSE);
}

/**
 * Check for asynchronous resource cleanup completion
 * @return	TRUE if the rendering resources have been released
 */
UBOOL UFluidSurfaceComponent::IsReadyForFinishDestroy()
{
	UBOOL bReady = Super::IsReadyForFinishDestroy();
	return bReady && (FluidSimulation == NULL || FluidSimulation->IsReleased());
}

void UFluidSurfaceComponent::FinishDestroy()
{
	if ( FluidSimulation )
	{
		delete FluidSimulation;
		FluidSimulation = NULL;
	}

	Super::FinishDestroy();
}

void UFluidSurfaceComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.Ver() >= VER_ADDED_FLUID_LIGHTMAPS)
	{
		Ar << LightMap;
	}
}

void UFluidSurfaceComponent::AddReferencedObjects(TArray<UObject*>& ObjectArray)
{
	Super::AddReferencedObjects(ObjectArray);
	if(LightMap != NULL)
	{
		LightMap->AddReferencedObjects(ObjectArray);
	}
}


void UFluidSurfaceComponent::UpdateBounds()
{
	FVector Corners[8];
	FLOAT HalfWidth = FluidWidth * 0.5f;
	FLOAT HalfHeight = FluidHeight * 0.5f;
	Corners[0] = LocalToWorld.TransformFVector( FVector(-HalfWidth, -HalfHeight, -10.0f) );
	Corners[1] = LocalToWorld.TransformFVector( FVector( HalfWidth, -HalfHeight, -10.0f) );
	Corners[2] = LocalToWorld.TransformFVector( FVector( HalfWidth,  HalfHeight, -10.0f) );
	Corners[3] = LocalToWorld.TransformFVector( FVector(-HalfWidth,  HalfHeight, -10.0f) );
	Corners[4] = LocalToWorld.TransformFVector( FVector(-HalfWidth, -HalfHeight,  10.0f) );
	Corners[5] = LocalToWorld.TransformFVector( FVector( HalfWidth, -HalfHeight,  10.0f) );
	Corners[6] = LocalToWorld.TransformFVector( FVector( HalfWidth,  HalfHeight,  10.0f) );
	Corners[7] = LocalToWorld.TransformFVector( FVector(-HalfWidth,  HalfHeight,  10.0f) );
	FBox BoundingBox( Corners, 8 );
	Bounds = FBoxSphereBounds(BoundingBox);

	if ( FluidSimulation )
	{
		Corners[0] = LocalToWorld.TransformFVector( FVector(-HalfWidth, -HalfHeight, 0.0f) );
		Corners[1] = LocalToWorld.TransformFVector( FVector( HalfWidth, -HalfHeight, 0.0f) );
		Corners[2] = LocalToWorld.TransformFVector( FVector( HalfWidth,  HalfHeight, 0.0f) );
		Corners[3] = LocalToWorld.TransformFVector( FVector(-HalfWidth,  HalfHeight, 0.0f) );
		FPlane Plane( Corners[0], Corners[1], Corners[2] );
		const FVector& Normal = Plane;
		FPlane Edges[4] =
		{
			FPlane( Corners[0], ((Corners[1] - Corners[0]) ^ Normal).UnsafeNormal() ),
			FPlane( Corners[1], ((Corners[2] - Corners[1]) ^ Normal).UnsafeNormal() ),
			FPlane( Corners[2], ((Corners[3] - Corners[2]) ^ Normal).UnsafeNormal() ),
			FPlane( Corners[3], ((Corners[0] - Corners[3]) ^ Normal).UnsafeNormal() )
		};
		FluidSimulation->SetExtents( LocalToWorld, Plane, Edges );
	}
}

void UFluidSurfaceComponent::InitResources( UBOOL bActive )
{
	if ( GForceFluidDeactivation )
	{
		bActive = FALSE;
	}

	if ( FluidSimulation )
	{
		ReleaseResources(TRUE);
	}

#if FINAL_RELEASE
	// Clear out debug showflags in finalrelease builds.
	bShowDetailNormals		= FALSE;
	bShowDetailPosition		= FALSE;
	bShowSimulationNormals	= FALSE;
	bShowSimulationPosition	= FALSE;
#endif

	FVector TopLeft		= LocalToWorld.TransformFVector( FVector(-FluidWidth*0.5f, -FluidHeight*0.5f, 0.0f) );
	FVector TopRight	= LocalToWorld.TransformFVector( FVector(FluidWidth*0.5f, -FluidHeight*0.5f, 0.0f) );
	FVector BottomLeft	= LocalToWorld.TransformFVector( FVector(-FluidWidth*0.5f, FluidHeight*0.5f, 0.0f) );
	FLOAT Width			= (TopLeft - TopRight).Size();
	FLOAT Height		= (TopLeft - BottomLeft).Size();

	// During a copy/paste, LocalToWorld scale factor may be zero. :/
	if ( appIsNearlyZero(Width) || appIsNearlyZero(Height) )
	{
		Width	= FluidWidth;
		Height	= FluidHeight;
	}

	// Limit GridSpacingLowRes for a maximum of ~64K vertices
	GridSpacingLowRes	= Max<FLOAT>(GridSpacingLowRes, 1.0f);
	INT NumLowResCellsX = Max<INT>(appTrunc(Width / GridSpacingLowRes), 1);
	INT NumLowResCellsY = Max<INT>(appTrunc(Height / GridSpacingLowRes), 1);
	INT NumLowResVertices = (NumLowResCellsX + 1) * (NumLowResCellsY + 1);
	if ( NumLowResVertices > 65000 )
	{
		// Solve: 65530 = (k*NumLowResCellsX + 1) * (k*NumLowResCellsY + 1)
		FLOAT A = FLOAT(NumLowResCellsX) * FLOAT(NumLowResCellsY);
		FLOAT B = FLOAT(NumLowResCellsX) + FLOAT(NumLowResCellsY);
		FLOAT C = -(65000 - 1);
		FLOAT ScaleFactor = (-B + appSqrt(B*B - 4.0f*A*C)) / (2.0f*A);

		// Adjust the low-res grid.
		NumLowResCellsX = appTrunc(ScaleFactor * NumLowResCellsX);
		NumLowResCellsY = appTrunc(ScaleFactor * NumLowResCellsY);
		GridSpacingLowRes = Max<FLOAT>(Width / NumLowResCellsX, Height / NumLowResCellsY);
	}

	FluidUpdateRate		= Max<FLOAT>(FluidUpdateRate, 1.0f);
	GridSpacing			= Max<FLOAT>(GridSpacing, 1.0f);
	FLOAT CellWidth		= GridSpacing;
	FLOAT CellHeight	= GridSpacing;
	INT TotalNumCellsX	= Max<INT>(appTrunc(Width / CellWidth), 1);
	INT TotalNumCellsY	= Max<INT>(appTrunc(Height / CellHeight), 1);
	if ( EnableSimulation == FALSE || bActive == FALSE )
	{
		TotalNumCellsX	= GPUTESSELLATION + 1;
		TotalNumCellsY	= GPUTESSELLATION + 1;
		CellWidth		= Width / TotalNumCellsX;
		CellHeight		= Height / TotalNumCellsY;
	}

	// Limit number of vertices to avoid driver crashes
	INT GridNumCellsX	= SimulationQuadsX;
	INT GridNumCellsY	= SimulationQuadsY;
	INT NumVertices		= (GridNumCellsX+1) * (GridNumCellsY+1);
	if ( NumVertices > GEngine->MaxFluidNumVerts )
	{
		FLOAT K			= appInvSqrt( FLOAT(NumVertices) / FLOAT(GEngine->MaxFluidNumVerts) );
		GridNumCellsX	= appTrunc( GridNumCellsX * K );
		GridNumCellsY	= appTrunc( GridNumCellsY * K );
	}

	// Align to default tessellation level (making the number of inner vertices a multiple of GPUTESSELLATION).
	TotalNumCellsX	= Max<INT>( TotalNumCellsX, GPUTESSELLATION + 1 );
	TotalNumCellsY	= Max<INT>( TotalNumCellsY, GPUTESSELLATION + 1 );
	TotalNumCellsX	= Align<INT>(TotalNumCellsX - 1, GPUTESSELLATION) + 1;
	TotalNumCellsY	= Align<INT>(TotalNumCellsY - 1, GPUTESSELLATION) + 1;
	GridNumCellsX	= Align<INT>(GridNumCellsX - 1, GPUTESSELLATION) + 1;
	GridNumCellsY	= Align<INT>(GridNumCellsY - 1, GPUTESSELLATION) + 1;

	// Clamp the simulation grid to the total fluid plane
	GridNumCellsX	= Min<INT>(GridNumCellsX, TotalNumCellsX);
	GridNumCellsY	= Min<INT>(GridNumCellsY, TotalNumCellsY);

	FluidWidth		= TotalNumCellsX * CellWidth;
	FluidHeight		= TotalNumCellsY * CellHeight;

	// Dont show fluid surfaces on mobile
	if ( GIsClient && !GUsingMobileRHI )
	{
		FluidSimulation = new FFluidSimulation( this, bActive, GridNumCellsX, GridNumCellsY, CellWidth, CellHeight, TotalNumCellsX, TotalNumCellsY );
	}
	TestRippleTime	= TestRippleFrequency;
	TestRippleAngle = 0.0f;

	DeactivationTimer = DEACTIVATION_TIME;

	UpdateBounds();
}

void UFluidSurfaceComponent::ReleaseResources( UBOOL bBlockOnRelease )
{
	if ( FluidSimulation )
	{
		// The scene is tracking fluid surfaces and storing a pointer to the fluid resource, so we can only call ReleaseResources if detached
		check(!IsAttached());

		FluidSimulation->ReleaseResources( bBlockOnRelease );
		if ( bBlockOnRelease )
		{
			delete FluidSimulation;
			FluidSimulation = NULL;
		}
	}
}

/**
 *	Calculates the distance from the fluid's edge to the specified position.
 *	@param WorldPosition	A world-space position to measure the distance to.
 *	@return					Distance from the fluid's edge to the specified position, or 0 if it's inside the fluid.
 */
FLOAT UFluidSurfaceComponent::CalcDistance( const FVector& WorldPosition )
{
	FVector LocalPosition = FluidSimulation->GetWorldToLocal().TransformFVector( WorldPosition );
	FLOAT Dx = Max<FLOAT>( Abs<FLOAT>(LocalPosition.X) - FluidWidth * 0.5f, 0.0f );
	FLOAT Dy = Max<FLOAT>( Abs<FLOAT>(LocalPosition.Y) - FluidHeight * 0.5f, 0.0f );
	FLOAT Distance = appSqrt( Dx*Dx + Dy*Dy );
	return Distance;
}

void UFluidSurfaceComponent::Tick(FLOAT DeltaTime)
{
	SCOPE_CYCLE_COUNTER( STAT_FluidSurfaceComponentTickTime );

	Super::Tick(DeltaTime);

	// Calculate the position and distance of the closest player.
	ViewDistance = 0.0f;
	FVector ClosestPlayerViewLocation = GetOwner()->Location;
	if(FluidSimulation)
	{
		INT NumObservers = GWorld->Observers.Num();
		ViewDistance = WORLD_MAX;
		for (INT ObserverIndex = 0; ObserverIndex < NumObservers; ObserverIndex++ )
		{
			if ( GWorld->Observers(ObserverIndex) )
			{
				const FVector& ViewLocation = GWorld->Observers(ObserverIndex)->GetObserverViewLocation();
				const FLOAT Distance = CalcDistance( ViewLocation );
				if( Distance < ViewDistance )
				{
					ClosestPlayerViewLocation = ViewLocation;
					ViewDistance = Distance;
				}
			}
		}
	}

	// Update the position of the detail grid (GPU-simulation)
	DetailPosition = GetOwner()->Location;
	if ( EnableDetail )
	{
		// If a detail target has been specified, use that for the detail position
		DetailPosition = TargetDetail ? TargetDetail->Location : ClosestPlayerViewLocation;
		SetDetailPosition( DetailPosition );
	}

	// Update the position of the simulation grid (CPU-simulation)
	if ( EnableSimulation )
	{
		// If a detail target has been specified, use that for the detail position
		SetSimulationPosition( TargetSimulation ? TargetSimulation->Location : ClosestPlayerViewLocation );
	}

	if ( FluidSimulation )
	{
		UpdateMemory( DeltaTime );

		// Apply Test Ripple force
		if( bTestRipple && !bPause )
		{
			TestRippleAngle += DeltaTime * TestRippleSpeed;
			TestRippleTime -= DeltaTime;
			if ( TestRippleTime < 0.0f )
			{
				FLOAT RippleRadius;
				if ( bTestRippleCenterOnDetail )
				{
					RippleRadius = 0.3f * DetailSize;
				}
				else
				{
					RippleRadius = 0.3f * Min(FluidSimulation->GetWidth(), FluidSimulation->GetHeight());
				}

				FLOAT Force;
				FVector WorldRipplePos, LocalRipplePos;
				LocalRipplePos = FVector(RippleRadius * appSin(TestRippleAngle), RippleRadius * appCos(TestRippleAngle), 0);

				if ( bTestRippleCenterOnDetail )
				{
					FVector LocalDetailPos = FluidSimulation->GetWorldToLocal().TransformFVector(DetailPosition);
					LocalRipplePos += LocalDetailPos;
				}

				WorldRipplePos = LocalToWorld.TransformFVector(LocalRipplePos);

				UBOOL bImpulse;
				if ( Abs(TestRippleFrequency) < 0.01f )
				{
					Force = ForceContinuous;
					bImpulse = FALSE;
				}
				else
				{
					Force = ForceImpact;
					bImpulse = TRUE;
				}

				ApplyForce(WorldRipplePos, Force, TestRippleRadius, bImpulse);
				TestRippleTime = TestRippleFrequency;
			}
		}

		//enqueue GPUApplyForce();
		//enqueue GPUSimulate(DeltaTime);
		//enqueue renderthreadCPUTick(DeltaTime);
		FluidSimulation->GameThreadTick( DeltaTime );
	}
}

void UFluidSurfaceComponent::ApplyForce(FVector WorldPos,FLOAT Strength,FLOAT WorldRadius,UBOOL bImpulse/*=FALSE*/)
{
	if ( FluidSimulation )
	{
		struct FParameters
		{
			FParameters(const FVector& InLocalPos, FLOAT InStrength, FLOAT InLocalRadius, UBOOL bInImpulse)
				: LocalPos(InLocalPos), Strength(InStrength), LocalRadius(InLocalRadius), bImpulse(bInImpulse) {}
			FVector LocalPos;
			FLOAT Strength;
			FLOAT LocalRadius;
			UBOOL bImpulse;
		};

		FVector DrawScale3D	= GetOwner()->DrawScale3D;
		FLOAT AvgScale		= (DrawScale3D.X + DrawScale3D.Y + DrawScale3D.Z) / 3.0f * GetOwner()->DrawScale;
		FLOAT LocalRadius	= WorldRadius / AvgScale;
		FVector LocalPos	= FluidSimulation->GetWorldToLocal().TransformFVector(WorldPos);

		if ( (EnableSimulation && FluidSimulation->IsWithinSimulationGrid( LocalPos, LocalRadius )) ||
			 (EnableDetail && FluidSimulation->IsWithinDetailGrid( LocalPos, LocalRadius )) )
		{
			// Check to see if we should enable full CPU/GPU simulation.
			if ( FluidSimulation->IsActive() == FALSE && GForceFluidDeactivation == FALSE )
			{
				FLOAT Distance = ViewDistance;
				if ( Distance < DeactivationDistance )
				{
					{
						FComponentReattachContext Reattach(this);
						InitResources( TRUE );
					}
					
					SetDetailPosition( DetailPosition );
					SetSimulationPosition( SimulationPosition );
				}
				else
				{
					return;
				}
			}

			ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
				ApplyForceCommand,
				FFluidSimulation*, FluidSimulation, FluidSimulation,
				FParameters, Parameters, FParameters(LocalPos,Strength,LocalRadius,bImpulse),
			{
				FluidSimulation->AddForce( Parameters.LocalPos, Parameters.Strength, Parameters.LocalRadius, Parameters.bImpulse );
			});
		}
	}
}

void UFluidSurfaceComponent::SetDetailPosition(FVector InWorldPos)
{
	DetailPosition = InWorldPos;
	if ( FluidSimulation )
	{
		FVector LocalPos = FluidSimulation->GetWorldToLocal().TransformFVector(InWorldPos);
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SetDetailPositionCommand,
			FFluidSimulation*, FluidSimulation, FluidSimulation,
			FVector, LocalPos, LocalPos,
		{
			FluidSimulation->SetDetailPosition( LocalPos );
		});
	}
}

void UFluidSurfaceComponent::SetSimulationPosition(FVector InWorldPos)
{
	SimulationPosition = InWorldPos;
	if ( FluidSimulation )
	{
		FVector LocalPos = FluidSimulation->GetWorldToLocal().TransformFVector(InWorldPos);
		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			SetDetailPositionCommand,
			FFluidSimulation*, FluidSimulation, FluidSimulation,
			FVector, LocalPos, LocalPos,
		{
			FluidSimulation->SetSimulationPosition( LocalPos );
		});
	}
}

/** @return TRUE if the linecheck passed (didn't hit anything) */
UBOOL UFluidSurfaceComponent::LineCheck(FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags)
{
	if ( FluidSimulation && FluidSimulation->LineCheck( Result, End, Start, Extent, TraceFlags ) == FALSE )
	{
		Result.Actor = GetOwner();
		Result.Component = this;
		Result.PhysMaterial = PhysMaterialOverride ? PhysMaterialOverride : GetMaterial()->GetPhysicalMaterial();
		return FALSE;
	}
	return TRUE;
}

/** @return TRUE if the linecheck passed (didn't hit anything) */
UBOOL UFluidSurfaceComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
	if ( FluidSimulation && FluidSimulation->PointCheck( Result, Location, Extent, TraceFlags ) == FALSE )
	{
		Result.Actor = GetOwner();
		Result.Component = this;
		Result.PhysMaterial = PhysMaterialOverride ? PhysMaterialOverride : GetMaterial()->GetPhysicalMaterial();
		return FALSE;
	}
	return TRUE;
}

void UFluidSurfaceComponent::InvalidateLightingCache()
{
	const UBOOL bHasStaticLightingData = LightMap.GetReference() || ShadowMaps.Num() > 0;
	if (bHasStaticLightingData)
	{
		Modify();

		// Mark lighting as requiring a rebuilt.
		MarkLightingRequiringRebuild();

		// Detach the component from the scene for the duration of this function.
		FComponentReattachContext ReattachContext(this);
		FlushRenderingCommands();
		Super::InvalidateLightingCache();

		// Discard all cached lighting.
		LightMap = NULL;
		ShadowMaps.Empty();
	}
}

/** 
 * Retrieves the materials used in this component 
 * 
 * @param OutMaterials	The list of used materials.
 */
void UFluidSurfaceComponent::GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const
{
	OutMaterials.AddItem( GetMaterial() );
}

/**
 * Returns the lightmap resolution used for this primivite instnace in the case of it supporting texture light/ shadow maps.
 * 0 if not supported or no static shadowing.
 *
 * @param	Width	[out]	Width of light/shadow map
 * @param	Height	[out]	Height of light/shadow map
 *
 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
 */
UBOOL UFluidSurfaceComponent::GetLightMapResolution( INT& Width, INT& Height ) const
{
	Width = LightMapResolution;
	Height = LightMapResolution;
	return FALSE;
}

/**
 *	Returns the static lightmap resolution used for this primitive.
 *	0 if not supported or no static shadowing.
 *
 * @return	INT		The StaticLightmapResolution for the component
 */
INT UFluidSurfaceComponent::GetStaticLightMapResolution() const
{
	return LightMapResolution;
}

void UFluidSurfaceComponent::GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const
{
	ShadowMapMemoryUsage	= 0;
	LightMapMemoryUsage		= 0;

	INT LightMapWidth		= 0;
	INT	LightMapHeight		= 0;
	GetLightMapResolution( LightMapWidth, LightMapHeight );

	if( HasStaticShadowing() && LightMapWidth > 0 && LightMapHeight > 0 )
	{
		// Stored in texture.
		const FLOAT MipFactor = 1.33f;
		const UINT NumLightMapCoefficients = GSystemSettings.bAllowDirectionalLightMaps ? NUM_DIRECTIONAL_LIGHTMAP_COEF : NUM_SIMPLE_LIGHTMAP_COEF;
		LightMapMemoryUsage	= appTrunc( NumLightMapCoefficients * MipFactor * LightMapWidth * LightMapHeight / 2 ); // DXT1
	}
}

void UFluidSurfaceComponent::GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const
{
	const FSphere BoundingSphere = Bounds.GetSphere();
	const FLOAT WorldTexelFactor = Max<FLOAT>( FluidWidth, FluidHeight );

	UMaterialInterface* Material = GetMaterial();

	// Enumerate the textures used by the material.
	TArray<UTexture*> Textures;
	
	// get textures for all quality levels
	Material->GetUsedTextures(Textures, MSQ_UNSPECIFIED, TRUE);

	// Add each texture to the output with the appropriate parameters.
	for(INT TextureIndex = 0;TextureIndex < Textures.Num();TextureIndex++)
	{
		FStreamingTexturePrimitiveInfo& StreamingTexture = *new(OutStreamingTextures) FStreamingTexturePrimitiveInfo;
		StreamingTexture.Bounds = BoundingSphere;
		StreamingTexture.TexelFactor = WorldTexelFactor;
		StreamingTexture.Texture = Textures(TextureIndex);
	}
}

void UFluidSurfaceComponent::RebuildClampMap()
{
#if !DISABLE_CLAMPMAP
	if ( EnableSimulation )
	{
		GridSpacing = Max<FLOAT>(GridSpacing, 1.0f);
		INT NumCellsX = Max<INT>(appTrunc(FluidWidth / GridSpacing), 1);
		INT NumCellsY = Max<INT>(appTrunc(FluidHeight / GridSpacing), 1);
		INT NumVertices = (NumCellsX + 1) * (NumCellsY + 1);
		if (ClampMap.Num() != NumVertices)
		{
			ClampMap.Empty( NumVertices );
			ClampMap.AddZeroed( NumVertices );
		}

		for ( INT Y=0; Y < NumCellsY; ++Y )
		{
			for ( INT X=0; X < NumCellsX; ++X )
			{
				FVector LocalPos( X * GridSpacing - FluidWidth * 0.5f, Y * GridSpacing - FluidHeight * 0.5f, 0.0f );
				FVector WorldPos = LocalToWorld.TransformFVector(LocalPos);
				FCheckResult Result;
				UBOOL bCollision = GWorld->EncroachingWorldGeometry( Result, WorldPos, FVector(0,0,0), FALSE );
				ClampMap(Y * (NumCellsX + 1) + X) = bCollision;
			}
		}
	}
	else
#endif
	{
		ClampMap.Empty( 0 );
	}
}

void UFluidSurfaceComponent::OnScaleChange()
{
	// Re-create the simulation if the scale has changed.
	if ( GetOwner()->DrawScale != 1.0f || GetOwner()->DrawScale3D != FVector(1.0f, 1.0f,1.0f) )
	{
		// Re-attach to make sure the UI gets updated, the proxy gets re-created, etc.
		PostEditChange();
	}
}

void UFluidSurfaceComponent::UpdateMemory(FLOAT DeltaTime)
{
	// Force deactivation of the fluid?
	if ( FluidSimulation->IsActive() && GForceFluidDeactivation )
	{
		FComponentReattachContext Reattach(this);
		InitResources( FALSE );
	}

	// Check to see if we should disable full CPU/GPU simulation again.
	if ( (EnableSimulation || EnableDetail) && FluidSimulation->IsActive() && ViewDistance > DeactivationDistance )
	{
		DeactivationTimer -= DeltaTime;
		if ( DeactivationTimer < 0.0f )
		{
			FComponentReattachContext Reattach(this);
			InitResources( FALSE );
		}
	}
	else
	{
		DeactivationTimer = DEACTIVATION_TIME;
	}
}
