/*=============================================================================
	FoliageComponent.cpp: Foliage rendering implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineFoliageClasses.h"

IMPLEMENT_CLASS(AInteractiveFoliageActor);
IMPLEMENT_CLASS(UInteractiveFoliageComponent);

/** Scene proxy class for UInteractiveFoliageComponent. */
class FInteractiveFoliageSceneProxy : public FStaticMeshSceneProxy
{
public:

	FInteractiveFoliageSceneProxy(UInteractiveFoliageComponent* InComponent) :
		FStaticMeshSceneProxy(InComponent),
		FoliageImpluseDirection(0,0,0),
		FoliageNormalizedRotationAxisAndAngle(0,0,1,0)
	{}

	/** Accessor used by the rendering thread when setting foliage parameters for rendering. */
	virtual void GetFoliageParameters(FVector& OutFoliageImpluseDirection, FVector4& OutFoliageNormalizedRotationAxisAndAngle) const
	{
		OutFoliageImpluseDirection = FoliageImpluseDirection;
		OutFoliageNormalizedRotationAxisAndAngle = FoliageNormalizedRotationAxisAndAngle;
	}

	/** Updates the scene proxy with new foliage parameters from the game thread. */
	void UpdateParameters_GameThread(const FVector& NewFoliageImpluseDirection, const FVector4& NewFoliageNormalizedRotationAxisAndAngle)
	{
		checkSlow(IsInGameThread());
		ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(
			UpdateFoliageParameters,
			FInteractiveFoliageSceneProxy*,FoliageProxy,this,
			FVector,NewFoliageImpluseDirection,NewFoliageImpluseDirection,
			FVector4,NewFoliageNormalizedRotationAxisAndAngle,NewFoliageNormalizedRotationAxisAndAngle,
			{
				FoliageProxy->FoliageImpluseDirection = NewFoliageImpluseDirection;
				FoliageProxy->FoliageNormalizedRotationAxisAndAngle = NewFoliageNormalizedRotationAxisAndAngle;
			});
	}

protected:

	FVector FoliageImpluseDirection;
	FVector4 FoliageNormalizedRotationAxisAndAngle;
};

FPrimitiveSceneProxy* UInteractiveFoliageComponent::CreateSceneProxy()
{
	// Store the foliage proxy so we can push updates to it during Tick
	FoliageSceneProxy = new FInteractiveFoliageSceneProxy(this);
	return FoliageSceneProxy;
}

void UInteractiveFoliageComponent::Detach( UBOOL bWillReattach )
{
	Super::Detach(bWillReattach);
	FoliageSceneProxy = NULL;
}

void AInteractiveFoliageActor::TakeDamage(INT Damage,class AController* EventInstigator,FVector HitLocation,FVector Momentum,class UClass* DamageType,struct FTraceHitInfo HitInfo,class AActor* DamageCauser)
{
	// Discard the magnitude of the momentum and use Damage as the length instead
	FVector DamageImpulse = Momentum.SafeNormal() * Damage * FoliageDamageImpulseScale;
	// Apply force magnitude clamps
	DamageImpulse.X = Clamp(DamageImpulse.X, -MaxDamageImpulse, MaxDamageImpulse);
	DamageImpulse.Y = Clamp(DamageImpulse.Y, -MaxDamageImpulse, MaxDamageImpulse);
	DamageImpulse.Z = Clamp(DamageImpulse.Z, -MaxDamageImpulse, MaxDamageImpulse);
	FoliageForce += DamageImpulse;
	// Bring this actor out of stasis so that it gets ticked now that a force has been applied
	SetTickIsDisabled(FALSE);
}

void AInteractiveFoliageActor::Touch(class AActor* Other,class UPrimitiveComponent* OtherComp,FVector HitLocation,FVector HitNormal)
{
	if (Other != NULL && Other->CollisionComponent != NULL && (Other->bBlockActors || (Other->GetAProjectile() != NULL && !Other->GetAProjectile()->bIgnoreFoliageTouch)))
	{
		UCylinderComponent* TouchingActorCylinder = Cast<UCylinderComponent>(Other->CollisionComponent);
		UCylinderComponent* CylinderComponent = Cast<UCylinderComponent>(CollisionComponent);
		if (TouchingActorCylinder && CylinderComponent)
		{
			const FVector CenterToTouching = FVector(TouchingActorCylinder->Bounds.Origin.X, TouchingActorCylinder->Bounds.Origin.Y, CylinderComponent->Bounds.Origin.Z) - CylinderComponent->Bounds.Origin;
			// Keep track of the first position on the collision cylinder that the touching actor intersected
			//@todo - need to handle multiple touching actors
			TouchingActorEntryPosition = CollisionComponent->Bounds.Origin + CenterToTouching.SafeNormal() * CylinderComponent->CollisionRadius;
		}
		// Bring this actor out of stasis so that it gets ticked now that a force has been applied
		SetTickIsDisabled(FALSE);
	}
}

void AInteractiveFoliageActor::SetupCollisionCylinder()
{
	if (StaticMeshComponent->StaticMesh)
	{
		const FBoxSphereBounds MeshBounds = StaticMeshComponent->StaticMesh->Bounds;
		// Set the cylinder's radius based off of the static mesh's bounds radius
		// CollisionRadius is in world space so apply the actor's scale
		CylinderComponent->CollisionRadius = MeshBounds.SphereRadius * .7f * DrawScale * Max(DrawScale3D.X, DrawScale3D.Y);
		// Set the cylinder's height based off of the static mesh's bounds height
		// CollisionHeight is in world space so apply the actor's scale
		CylinderComponent->CollisionHeight = MeshBounds.BoxExtent.Z * DrawScale * DrawScale3D.Z;
	}
}

void AInteractiveFoliageActor::TickSpecial(FLOAT DeltaSeconds)
{
	UInteractiveFoliageComponent* FoliageComponent = CastChecked<UInteractiveFoliageComponent>(StaticMeshComponent);
	// Can only push updates to the scene proxy if we are being ticked while attached
	// The proxy will be NULL on dedicated server
	if (FoliageComponent->IsAttached() && FoliageComponent->FoliageSceneProxy)
	{
		for (INT TouchingIndex = 0; TouchingIndex < Touching.Num(); TouchingIndex++)
		{
			AActor* TouchingActor = Touching(TouchingIndex);
			if (TouchingActor != NULL && TouchingActor->CollisionComponent != NULL
				// Hacks to ignore certain types of touching actors
				//@todo - allow games to exclude certain types of actors
				&& (TouchingActor->bBlockActors || TouchingActor->GetAProjectile() != NULL))
			{
				const FVector TouchingActorPosition(TouchingActor->CollisionComponent->Bounds.Origin.X, TouchingActor->CollisionComponent->Bounds.Origin.Y, CollisionComponent->Bounds.Origin.Z);
				//DrawDebugLine(TouchingActorPosition, CollisionComponent->Bounds.Origin, 255, 255, 255, FALSE);
				// Operate on the touching actor's collision cylinder
				//@todo - handle touching actors without collision cylinders
				UCylinderComponent* TouchingActorCylinder = Cast<UCylinderComponent>(TouchingActor->CollisionComponent);
				UCylinderComponent* CylinderComponent = Cast<UCylinderComponent>(CollisionComponent);
				if (TouchingActorCylinder && CylinderComponent)
				{
					FVector TouchingToCenter = CollisionComponent->Bounds.Origin - TouchingActorPosition;
					// Force the simulated position to be in the XY plane for simplicity
					TouchingToCenter.Z = 0;
					// Position on the collision cylinder mirrored across the cylinder's center from the position that the touching actor entered
					const FVector OppositeTouchingEntryPosition = CollisionComponent->Bounds.Origin + CollisionComponent->Bounds.Origin - TouchingActorEntryPosition;

					// Project the touching actor's center onto the vector from where it first entered to OppositeTouchingEntryPosition
					// This results in the same directional force being applied for the duration of the other actor touching this foliage actor,
					// Which prevents strange movement that results from just comparing cylinder centers.
					const FVector ProjectedTouchingActorPosition = (TouchingActorPosition - OppositeTouchingEntryPosition).ProjectOnTo(TouchingActorEntryPosition - OppositeTouchingEntryPosition) + OppositeTouchingEntryPosition;
					// Find the furthest position on the cylinder of the touching actor from OppositeTouchingEntryPosition
					const FVector TouchingActorFurthestPosition = ProjectedTouchingActorPosition + (TouchingActorEntryPosition - OppositeTouchingEntryPosition).SafeNormal() * TouchingActorCylinder->CollisionRadius;
					// Construct the impulse as the distance between the furthest cylinder positions minus the two cylinder's diameters
					const FVector ImpulseDirection = 
						- (OppositeTouchingEntryPosition - TouchingActorFurthestPosition 
						- (OppositeTouchingEntryPosition - TouchingActorFurthestPosition).SafeNormal() * 2.0f * (TouchingActorCylinder->CollisionRadius + CylinderComponent->CollisionRadius));

					//DrawDebugLine(CollisionComponent->Bounds.Origin + FVector(0,0,100), CollisionComponent->Bounds.Origin + ImpulseDirection + FVector(0,0,100), 100, 255, 100, FALSE);

					// Scale and clamp the touch force
					FVector Impulse = ImpulseDirection * FoliageTouchImpulseScale;
					Impulse.X = Clamp(Impulse.X, -MaxTouchImpulse, MaxTouchImpulse);
					Impulse.Y = Clamp(Impulse.Y, -MaxTouchImpulse, MaxTouchImpulse);
					Impulse.Z = Clamp(Impulse.Z, -MaxTouchImpulse, MaxTouchImpulse);
					FoliageForce += Impulse;
				}
			}
		}

		// Apply spring stiffness, which is the force that pushes the simulated particle back to the origin
		FoliageForce += -FoliageStiffness * FoliagePosition;
		// Apply spring quadratic stiffness, which increases in magnitude with the square of the distance to the origin
		// This prevents the spring from being displaced too much by touch and damage forces
		FoliageForce += -FoliageStiffnessQuadratic * FoliagePosition.SizeSquared() * FoliagePosition.SafeNormal();
		// Apply spring damping, which is like air resistance and causes the spring to lose energy over time
		FoliageForce += -FoliageDamping * FoliageVelocity;

		FoliageForce.X = Clamp(FoliageForce.X, -MaxForce, MaxForce);
		FoliageForce.Y = Clamp(FoliageForce.Y, -MaxForce, MaxForce);
		FoliageForce.Z = Clamp(FoliageForce.Z, -MaxForce, MaxForce);

		FoliageVelocity += FoliageForce * DeltaSeconds;
		FoliageForce = FVector(0,0,0);

		const FLOAT MaxVelocity = 1000.0f;
		FoliageVelocity.X = Clamp(FoliageVelocity.X, -MaxVelocity, MaxVelocity);
		FoliageVelocity.Y = Clamp(FoliageVelocity.Y, -MaxVelocity, MaxVelocity);
		FoliageVelocity.Z = Clamp(FoliageVelocity.Z, -MaxVelocity, MaxVelocity);

		FoliagePosition += FoliageVelocity * DeltaSeconds;

		//DrawDebugLine(CollisionComponent->Bounds.Origin + FVector(0,0,100), CollisionComponent->Bounds.Origin + FVector(0,0,100) + FoliagePosition, 255, 100, 100, FALSE);

		//@todo - derive this height from the static mesh
		const FLOAT IntersectionHeight = 100.0f;
		// Calculate the rotation angle using Sin(Angle) = Opposite / Hypotenuse
		const FLOAT RotationAngle = -appAsin(FoliagePosition.Size() / IntersectionHeight);
		// Use a rotation angle perpendicular to the impulse direction and the z axis
		const FVector NormalizedRotationAxis = FoliagePosition.SizeSquared() > KINDA_SMALL_NUMBER ? 
			(FoliagePosition ^ FVector(0,0,1)).SafeNormal() :
			FVector(0,0,1);

		// Propagate the new rotation axis and angle to the rendering thread
		FoliageComponent->FoliageSceneProxy->UpdateParameters_GameThread(FoliagePosition, FVector4(NormalizedRotationAxis, RotationAngle));

		if (FoliagePosition.SizeSquared() < Square(KINDA_SMALL_NUMBER * 10.0f)
			&& FoliageVelocity.SizeSquared() < Square(KINDA_SMALL_NUMBER * 10.0f))
		{
			// Go into stasis (will no longer be ticked) if this actor's spring simulation has stabilized
			SetTickIsDisabled(TRUE);
		}
	}
	Super::TickSpecial(DeltaSeconds);
}

void AInteractiveFoliageActor::Spawned()
{
	Super::Spawned();
	SetupCollisionCylinder();
}

void AInteractiveFoliageActor::PostLoad()
{
	Super::PostLoad();
	SetupCollisionCylinder();
}
