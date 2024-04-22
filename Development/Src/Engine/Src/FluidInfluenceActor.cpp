/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineFluidClasses.h"
#include "FluidSurface.h"

IMPLEMENT_CLASS(AFluidInfluenceActor);
IMPLEMENT_CLASS(UFluidInfluenceComponent);

#define TWO_PI	(2.0f*FLOAT(PI))


/*=============================================================================
	AFluidInfluenceActor implementation
=============================================================================*/


/*=============================================================================
	UFluidInfluenceComponent implementation
=============================================================================*/

void UFluidInfluenceComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	UBOOL bUpdateIcon = PropertyThatChanged == NULL || appStrcmp( *PropertyThatChanged->GetName(), TEXT("InfluenceType")) == 0;
	CheckSettings( bUpdateIcon );

	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UFluidInfluenceComponent::PostLoad()
{
	Super::PostLoad();
	CheckSettings( TRUE );
}

void UFluidInfluenceComponent::CheckSettings( UBOOL bUpdateIcon )
{
	WaveFrequency		= Max<FLOAT>( WaveFrequency, 0.0f );
	FlowNumRipples		= Max<INT>( FlowNumRipples, 1 );
	RaindropRate		= Max<FLOAT>( RaindropRate, 0.0001f );
	SphereOuterRadius	= Max<FLOAT>( SphereOuterRadius, 0.0001f );
	SphereInnerRadius	= Clamp<FLOAT>( SphereInnerRadius, 0.0001f, SphereOuterRadius );
	CurrentAngle		= 0.0f;
	CurrentTimer		= 0.0f;

	if ( bUpdateIcon )
	{
		AFluidInfluenceActor* InfluenceActor = Cast<AFluidInfluenceActor>(GetOuter());
		if ( InfluenceActor && InfluenceActor->Sprite && InfluenceActor->InfluenceComponent == this )
		{
			UTexture2D* Texture = NULL;
			switch ( InfluenceType )
			{
				case Fluid_Flow:
					Texture = LoadObject<UTexture2D>(NULL, TEXT("EditorResources.S_FluidFlow"), NULL, LOAD_None, NULL);
					break;
				case Fluid_Raindrops:
					Texture = LoadObject<UTexture2D>(NULL, TEXT("EditorResources.S_FluidRaindrops"), NULL, LOAD_None, NULL);
					break;
				case Fluid_Wave:
					Texture = LoadObject<UTexture2D>(NULL, TEXT("EditorResources.S_FluidSurfOsc"), NULL, LOAD_None, NULL);
					break;
				case Fluid_Sphere:
					Texture = LoadObject<UTexture2D>(NULL, TEXT("EditorResources.S_FluidSphere"), NULL, LOAD_None, NULL);
					break;
			}
			if ( !Texture )
			{
				Texture = LoadObject<UTexture2D>(NULL, TEXT("EditorResources.S_FluidSurfOsc"), NULL, LOAD_None, NULL);
			}
			if ( Texture )
			{
				InfluenceActor->Sprite->Sprite = Texture;
			}
		}
	}
}

UBOOL UFluidInfluenceComponent::IsTouching( AFluidSurfaceActor* Fluid )
{
	if ( Fluid && Fluid->FluidComponent && Fluid->FluidComponent->IsAttached() && Fluid->FluidComponent->GetOwner() )
	{
		FFluidSimulation* FluidSimulation = Fluid->FluidComponent->FluidSimulation;
		if ( FluidSimulation )
		{
			const FLOAT HalfWidth = Fluid->FluidComponent->FluidWidth * 0.5f;
			const FLOAT HalfHeight = Fluid->FluidComponent->FluidHeight * 0.5f;
			FLOAT Radius;
			switch ( InfluenceType )
			{
				case Fluid_Raindrops:
					Radius = RaindropFillEntireFluid ? RaindropAreaRadius : 0.0f;
					break;
				case Fluid_Wave:
					Radius = WaveRadius;
					break;
				case Fluid_Sphere:
					Radius = SphereOuterRadius;
					break;
				case Fluid_Flow:
				default:
					Radius = 0.0f;
			}

			const FVector LocalPosition = FluidSimulation->GetWorldToLocal().TransformFVector( Owner->Location );
			if ( (LocalPosition.Z >= -MaxDistance) &&
				 (LocalPosition.Z <= MaxDistance) &&
				 (LocalPosition.X+Radius) >= -HalfWidth &&
				 (LocalPosition.X-Radius) <=  HalfWidth &&
				 (LocalPosition.Y+Radius) >= -HalfHeight &&
				 (LocalPosition.Y-Radius) <=  HalfHeight )
			{
				return TRUE;
			}
		}
	}
	return FALSE;
}

void UFluidInfluenceComponent::UpdateBounds()
{
	FVector Corners[8];
	FVector Position = LocalToWorld.GetOrigin();
	FLOAT HalfSize = 300.0f;
	
	Corners[0] = Position + FVector(-HalfSize, -HalfSize, -HalfSize);
	Corners[1] = Position + FVector( HalfSize, -HalfSize, -HalfSize);
	Corners[2] = Position + FVector( HalfSize,  HalfSize, -HalfSize);
	Corners[3] = Position + FVector(-HalfSize,  HalfSize, -HalfSize);
	Corners[4] = Position + FVector(-HalfSize, -HalfSize,  HalfSize);
	Corners[5] = Position + FVector( HalfSize, -HalfSize,  HalfSize);
	Corners[6] = Position + FVector( HalfSize,  HalfSize,  HalfSize);
	Corners[7] = Position + FVector(-HalfSize,  HalfSize,  HalfSize);
	FBox BoundingBox( Corners, 8 );
	Bounds = FBoxSphereBounds(BoundingBox);
}

void UFluidInfluenceComponent::Tick( FLOAT DeltaSeconds )
{
	SCOPE_CYCLE_COUNTER( STAT_FluidInfluenceComponentTickTime );

	if ( bActive && GetOwner() )
	{
		if ( FluidActor == NULL )
		{
			// Check if I'm touching CurrentFluidActor. If not, try to find another fluidactor that I'm touching.
			const TArray<UFluidSurfaceComponent*>* FluidSurfacesGameThread = Scene->GetFluidSurfaces();
			if ( !IsTouching(CurrentFluidActor) && FluidSurfacesGameThread )
			{
				CurrentFluidActor = NULL;
				for( TArray<UFluidSurfaceComponent*>::TConstIterator FluidIt(*FluidSurfacesGameThread); FluidIt; ++FluidIt )
				{
					UFluidSurfaceComponent* FluidSurfaceComponent = *FluidIt;
					AFluidSurfaceActor* FluidSurfaceActor = Cast<AFluidSurfaceActor>(FluidSurfaceComponent->GetOwner());
					if ( IsTouching(FluidSurfaceActor) )
					{
						CurrentFluidActor = FluidSurfaceActor;
						break;
					}
				}
			}
		}
		else
		{
			CurrentFluidActor = FluidActor;
		}

		// Sanity check the CurrentFluidActor.
		if ( CurrentFluidActor == NULL ||
			 CurrentFluidActor->FluidComponent == NULL ||
			 CurrentFluidActor->FluidComponent->IsAttached() == FALSE ||
			 CurrentFluidActor->FluidComponent->GetOwner() == NULL )
		{
			CurrentFluidActor = NULL;
		}

		if ( CurrentFluidActor )
		{
			WaveFrequency		= Max<FLOAT>( WaveFrequency, 0.0f );
			FlowNumRipples		= Max<INT>( FlowNumRipples, 1 );
			RaindropRate		= Max<FLOAT>( RaindropRate, 0.0001f );
			SphereOuterRadius	= Max<FLOAT>( SphereOuterRadius, 0.0001f );
			SphereInnerRadius	= Clamp<FLOAT>( SphereInnerRadius, 0.0001f, SphereOuterRadius );

			switch ( InfluenceType )
			{
				case Fluid_Flow:
					UpdateFlow( DeltaSeconds );
					break;
				case Fluid_Raindrops:
					UpdateRaindrops( DeltaSeconds );
					break;
				case Fluid_Wave:
					UpdateWave( DeltaSeconds );
					break;
				case Fluid_Sphere:
					UpdateSphere( DeltaSeconds );
					break;
			}
		}
	}

	// Return the influence actor to its previous state if it was toggled.
	if ( bIsToggleTriggered )
	{
		bActive = !bActive;
		bIsToggleTriggered = FALSE;
	}
}

void UFluidInfluenceComponent::UpdateFlow( FLOAT DeltaSeconds )
{
	// Align the flow matrix to the fluid plane.
	const FMatrix& FluidMatrix = CurrentFluidActor->FluidComponent->LocalToWorld;
	const FMatrix& FlowMatrix = GetOwner()->LocalToWorld();
	FMatrix WorldToFluid = FluidMatrix.Inverse();
	FVector WorldDirection = FlowMatrix.GetAxis(0);
	FVector LocalDirection = WorldToFluid.TransformNormal(WorldDirection);
	LocalDirection.Z = 0.0f;
	LocalDirection.Normalize();
	FVector LocalSide = FVector(0,0,1) ^ LocalDirection;

	LocalDirection = FluidMatrix.TransformNormal(LocalDirection);
	LocalSide = FluidMatrix.TransformNormal(LocalSide);
	FMatrix AlignedFlowMatrix( LocalDirection, LocalSide, FluidMatrix.GetAxis(2), FluidMatrix.GetOrigin() );

	// Calculate bounding quad, in flow direction space.
	FMatrix FluidToFlow = AlignedFlowMatrix.Inverse() * FluidMatrix;
	const FLOAT HalfWidth = CurrentFluidActor->FluidComponent->FluidWidth * 0.5f;
	const FLOAT HalfHeight = CurrentFluidActor->FluidComponent->FluidHeight * 0.5f;
	FBox Box( FVector(-HalfWidth,-HalfHeight,0), FVector(HalfWidth,HalfHeight,0) );
	Box = Box.TransformBy( FluidToFlow );
	FLOAT FlowWidth = Box.Max.X - Box.Min.X;
	FLOAT FlowHeight = Box.Max.Y - Box.Min.Y;
	FLOAT SideMotionFrequency = FlowFrequency;
	FRandomStream Rand( 0x1ee7c0de );
	for ( INT RippleIndex=0; RippleIndex < FlowNumRipples; ++RippleIndex )
	{
		FLOAT X = Rand.GetFraction() * FlowWidth;
		FLOAT Y = Rand.GetFraction() * FlowHeight;
		FLOAT SideMotionOffset = Rand.GetFraction();
		FLOAT AmplitudeOffset = Rand.GetFraction();
		FVector FlowPos;
		FlowPos.X = appFmod( X + FlowSpeed * CurrentTimer, FlowHeight ) - 0.5f*FlowWidth;
		FlowPos.Y = Y + FlowSideMotionRadius * appSin( (SideMotionFrequency * CurrentTimer + SideMotionOffset) * TWO_PI ) - 0.5f*FlowHeight;
		FlowPos.Z = 0.0f;
		FLOAT Force = FlowStrength * appSin( (FlowFrequency * CurrentTimer + AmplitudeOffset) * TWO_PI );

		FVector WorldPos = AlignedFlowMatrix.TransformFVector( FlowPos );
		CurrentFluidActor->FluidComponent->ApplyForce( WorldPos, Force, FlowWaveRadius, FALSE );
	}

	CurrentTimer += DeltaSeconds;

	static UBOOL bShowFlowBox = FALSE;
	if ( bShowFlowBox )
	{
		FVector P1 = AlignedFlowMatrix.TransformFVector( FVector(-0.5f*FlowWidth, -0.5f*FlowHeight, 0.0f) );
		FVector P2 = AlignedFlowMatrix.TransformFVector( FVector( 0.5f*FlowWidth, -0.5f*FlowHeight, 0.0f) );
		FVector P3 = AlignedFlowMatrix.TransformFVector( FVector( 0.5f*FlowWidth,  0.5f*FlowHeight, 0.0f) );
		FVector P4 = AlignedFlowMatrix.TransformFVector( FVector(-0.5f*FlowWidth,  0.5f*FlowHeight, 0.0f) );
		GWorld->LineBatcher->DrawLine( P1, P2, FColor(255,0,0), SDPG_World );
		GWorld->LineBatcher->DrawLine( P2, P3, FColor(255,0,0), SDPG_World );
		GWorld->LineBatcher->DrawLine( P3, P4, FColor(255,0,0), SDPG_World );
		GWorld->LineBatcher->DrawLine( P4, P1, FColor(255,0,0), SDPG_World );
	}
}

void UFluidInfluenceComponent::UpdateRaindrops( FLOAT DeltaSeconds )
{
	FLOAT NumCellsX = CurrentFluidActor->FluidComponent->FluidWidth / CurrentFluidActor->FluidComponent->GridSpacing;
	FLOAT NumCellsY = CurrentFluidActor->FluidComponent->FluidHeight / CurrentFluidActor->FluidComponent->GridSpacing;
	FLOAT Delay = 1.0f / RaindropRate;

	CurrentTimer -= DeltaSeconds;
	while ( CurrentTimer < 0.0f )
	{
		FVector Position;
		if ( RaindropFillEntireFluid )
		{
			FLOAT X = (appRound( appFrand() * NumCellsX ) - 0.5f*NumCellsX) * CurrentFluidActor->FluidComponent->GridSpacing;
			FLOAT Y = (appRound( appFrand() * NumCellsY ) - 0.5f*NumCellsY) * CurrentFluidActor->FluidComponent->GridSpacing;
			Position = CurrentFluidActor->FluidComponent->LocalToWorld.TransformFVector( FVector(X, Y, 0.0f) );
		}
		else
		{
			FLOAT X = appFrand() * RaindropAreaRadius * appCos( appFrand() * TWO_PI ) / CurrentFluidActor->FluidComponent->GridSpacing;
			FLOAT Y = appFrand() * RaindropAreaRadius * appSin( appFrand() * TWO_PI ) / CurrentFluidActor->FluidComponent->GridSpacing;
			X = appRound( X ) * CurrentFluidActor->FluidComponent->GridSpacing;
			Y = appRound( Y ) * CurrentFluidActor->FluidComponent->GridSpacing;
			Position = LocalToWorld.TransformFVector( FVector(X, Y, 0.0f) );
		}
		CurrentFluidActor->FluidComponent->ApplyForce( Position, RaindropStrength, RaindropRadius, TRUE );
		CurrentTimer += Delay * (appFrand() + 0.5f);
	}
}

void UFluidInfluenceComponent::UpdateWave( FLOAT DeltaSeconds )
{
	CurrentAngle += WaveFrequency*DeltaSeconds;
	FLOAT Phase = WavePhase / 360.0f;
	FLOAT Force = WaveStrength * appCos( TWO_PI * (CurrentAngle + Phase) );
	CurrentFluidActor->FluidComponent->ApplyForce( Owner->Location, Force, WaveRadius, FALSE );
}

void UFluidInfluenceComponent::UpdateSphere( FLOAT DeltaSeconds )
{
	FVector Up		= CurrentFluidActor->FluidComponent->LocalToWorld.GetAxis(2);
	FVector Vec		= Owner->Location - CurrentFluidActor->Location;
	FLOAT Dist		= Up | Vec;
	FLOAT DistSign	= (Dist >= 0.0f ) ? 1.0f : -1.0f;
	Dist			= Abs(Dist);
	if ( Dist < SphereOuterRadius )
	{
		FLOAT IntersectionRadius = appSqrt( SphereOuterRadius*SphereOuterRadius - Dist*Dist );
		FLOAT ForceFactor;
		if ( Dist > SphereInnerRadius )
		{
			ForceFactor = 1.0f - (Dist - SphereInnerRadius) / (SphereOuterRadius - SphereInnerRadius);
		} else
		{
			ForceFactor = Dist / SphereInnerRadius;
		}
		CurrentFluidActor->FluidComponent->ApplyForce( Owner->Location, DistSign * SphereStrength * ForceFactor, IntersectionRadius, FALSE );
	}
}
