/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#include "ForceFunctionSample.h"
#include "ForceFunctionRadial.h"
#include "UserForceField.h"
#include "UserForceFieldLinearKernel.h"
#include "UserForceFieldShape.h"
#include "UserForceFieldShapeGroup.h"
#include "ForceFieldExcludeChannel.h"
#include "ForceFunctionTornadoAngular.h"
#endif // WITH_NOVODEX


extern TMap<INT, struct ForceFieldExcludeChannel*> GNovodexForceFieldExcludeChannelsMap;
extern TArray<class UserForceFieldShapeGroup* >	GNovodexPendingKillForceFieldShapeGroups;

IMPLEMENT_CLASS(UNxForceFieldComponent);
IMPLEMENT_CLASS(UNxForceFieldRadialComponent);
IMPLEMENT_CLASS(UNxForceFieldCylindricalComponent);
IMPLEMENT_CLASS(UNxForceFieldGenericComponent);
IMPLEMENT_CLASS(UNxForceFieldTornadoComponent);

void UNxForceFieldComponent::InitComponentRBPhys(UBOOL bFixed)
{
	if( ForceField == NULL )
	{
		if (!GWorld->RBPhysScene)
			return;
		InitForceField(GWorld->RBPhysScene);
	}
}

void UNxForceFieldComponent::TermComponentRBPhys(FRBPhysScene *Scene)
{
#if WITH_NOVODEX
	// If the force field is not in the scene we want, do nothing.
	if( (Scene != NULL) && (SceneIndex != Scene->NovodexSceneIndex) )
	{
		return;
	}

	NxScene* NovodexScene = NULL;
	if( ForceField )
	{
		NovodexScene = GetNovodexPrimarySceneFromIndex(SceneIndex);
	}
	if( NovodexScene )
	{
		if(NovodexScene->checkResults(NX_ALL_FINISHED, false))
		{

			GNovodexPendingKillForceFields.AddItem(ForceField);
		}
		else
		{
			//#define ConvexMeshReferenceBugTest
#ifdef ConvexMeshReferenceBugTest
			//temptest
			int beforeForceFieldDestroy = 1, afterForceFieldDestroy = 0;
			if (ConvexMeshes.Num())
			{
				check(ConvexMeshes.Num() == 1);
				beforeForceFieldDestroy = ((NxConvexMesh*)ConvexMeshes(0))->getReferenceCount();
			}
			// (2)
			ForceField->Destroy();
			if (ConvexMeshes.Num())
			{
				check(ConvexMeshes.Num() == 1);
				afterForceFieldDestroy = ((NxConvexMesh*)ConvexMeshes(0))->getReferenceCount();
			}
			check(beforeForceFieldDestroy - 1  == afterForceFieldDestroy);
#else
			// (2)
			ForceField->Destroy();
#endif
		}
	}
	// Either we have deleted the force field directly (2)
	// or it will be deleted afterwards in the world tick (1)
	// or the scene has already been deleted (and therefore also the force field)
	ForceField = NULL;

	while (ConvexMeshes.Num() > 0)
	{
		NxConvexMesh* convexMesh = (NxConvexMesh*)ConvexMeshes.Pop();
		GNovodexPendingKillConvex.AddItem(convexMesh);
	}
#endif
}

void UNxForceFieldComponent::Tick(FLOAT DeltaSeconds)
{
	if (!ForceField)
	{
		return;
	}

	if(bForceActive)
	{
#if WITH_NOVODEX 
		NxGroupsMask groupsMask = CreateGroupsMask(RBChannel, &CollideWithChannels);
		ForceField->setGroupsMask(groupsMask);

		if( Duration > 0.0 )   //Calculate elapsed time is a duration is set
		{
			ElapsedTime += DeltaSeconds;
			if( ElapsedTime >= Duration )
			{
				bForceActive = FALSE;
				ElapsedTime = 0.0; 
			}
		}
#endif
	}
	else
	{
#if WITH_NOVODEX
		NxGroupsMask groupsMask = CreateGroupsMask(RBCC_Nothing, NULL);
		ForceField->setGroupsMask(groupsMask);

		if( bDestroyWhenInactive && Owner)
		{
			ANxForceFieldSpawnable* SpawnableActor = Cast<ANxForceFieldSpawnable>(Owner);
			if(SpawnableActor)
			{
				GWorld->DestroyActor(SpawnableActor);
			}
			else
			{
				Owner->DetachComponent(this);
				this->TermComponentRBPhys(RBPhysScene);
			}
		}
#endif
	}

}
void UNxForceFieldComponent::InitForceField( FRBPhysScene *InScene )
{
#if WITH_NOVODEX
	RBPhysScene = InScene;
	NxScene* nxScene = InScene->GetNovodexPrimaryScene();

	if (!nxScene)
		return;

	//Create the kernel
	CreateKernel();
	WaitForNovodexScene(*nxScene);

	//Setup the forcefield descriptor
	NxForceFieldDesc ffDesc;
	ffDesc.fluidType = NX_FF_TYPE_GRAVITATIONAL;

	//Setup Collision Parameters
	if (bForceActive)
	{
		ffDesc.groupsMask = CreateGroupsMask(RBChannel, &CollideWithChannels);
	}
	else
	{
		ffDesc.groupsMask = CreateGroupsMask(RBCC_Nothing, NULL);
	}

	// Define Force Function
	DefineForceFunction(&ffDesc);

	//Create the forcefield
	check(ForceField == NULL);
	UBOOL bRotateForceField = FALSE;
	if (ffDesc.coordinates == NX_FFC_CYLINDRICAL)
	{
		bRotateForceField = TRUE;
	}
	ForceField = UserForceField::Create(nxScene->createForceField(ffDesc), bRotateForceField);

	CreateExclusionShapes(nxScene);

	NxForceFieldShapeDesc* ffShapeDesc = (NxForceFieldShapeDesc*)DefineForceFieldShapeDesc();

	if (ffShapeDesc)
	{
		if (ffShapeDesc->getType() == NX_SHAPE_CONVEX)
		{
			NxConvexForceFieldShapeDesc* desc = (NxConvexForceFieldShapeDesc*)ffShapeDesc;
			ConvexMeshes.AddItem(desc->meshData);
		}
		ForceField->getIncludeShapeGroup().createShape(*ffShapeDesc);
		delete ffShapeDesc;
	}

	SceneIndex = InScene->NovodexSceneIndex;

	bNeedsUpdateTransform = true;
#endif
}

void UNxForceFieldComponent::UpdateTransform()
{
	Super::UpdateTransform();
	if(bForceActive && ForceField != NULL)
	{
#if WITH_NOVODEX
		ForceField->setPose(U2NTransform(LocalToWorld));
#endif
	}
}

// If this component was loaded as part of the root set it can not be marked for pending kill
UBOOL UNxForceFieldComponent::AllowBeingMarkedPendingKill()
{

	return !HasAnyFlags( RF_RootSet );
}

void UNxForceFieldComponent::DoInitRBPhys()
{
	InitComponentRBPhys(false);
}

void UNxForceFieldComponent::DefineForceFunction( FPointer ForceFieldDesc )
{

}

FPointer UNxForceFieldComponent::DefineForceFieldShapeDesc()
{
	return NULL;
}

void UNxForceFieldComponent::SetForceFieldPose( FPointer ForceFieldDesc )
{

}

#if WITH_NOVODEX
void UNxForceFieldComponent::CreateExclusionShapes( NxScene* nxScene )
{
	if (!ForceField)
	{
		return;
	}

	// Both ANxForceField and ARB_ForceFieldExcludeVolume will do something in InitRBPhys() because of order is not known in advance.
	ForceFieldExcludeChannel* channel = GNovodexForceFieldExcludeChannelsMap.FindRef(ExcludeChannel);
	if (channel)
	{
		for (TMap<ARB_ForceFieldExcludeVolume*, UserForceFieldShapeGroup*>::TIterator i(channel->Groups); i; ++i)
		{
			UserForceFieldShapeGroup* group = i.Value();
			ForceField->addShapeGroup(*group);
		}
	}
}
#endif

void UNxForceFieldComponent::PostLoad()
{
	Super::PostLoad();

	if( DrawComponent != NULL )
	{
		if (Owner)
		{
			Owner->Components.AddItem(DrawComponent);
		}
	}
}

void UNxForceFieldComponent::CreateKernel()
{

}

void UNxForceFieldRadialComponent::CreateKernel()
{
#if WITH_NOVODEX
	check(Kernel == NULL);
	Kernel = new NxForceFieldKernelRadial;
#endif
}

void UNxForceFieldRadialComponent::TermComponentRBPhys(FRBPhysScene *Scene)
{
	Super::TermComponentRBPhys(Scene);
#if WITH_NOVODEX
	if ( Kernel )
	{
		delete Kernel;
		Kernel = NULL;
	}
#endif
}

void UNxForceFieldRadialComponent::DefineForceFunction( FPointer ForceFieldDesc )
{
#if WITH_NOVODEX
	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	Kernel->setRadialStrength(ForceStrength);
	Kernel->setRadius(U2PScale*ForceRadius);
	Kernel->setRadiusRecip(1.0f/(U2PScale*ForceRadius));
	Kernel->setSelfRotationStrength(SelfRotationStrength);
	Kernel->setBLinearFalloff(ForceFalloff==RIF_Linear);

	ffDesc.kernel = Kernel;
	ffDesc.coordinates = NX_FFC_SPHERICAL;
#endif
}

FPointer UNxForceFieldRadialComponent::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	return Shape ? Shape->CreateNxDesc() : NULL;
#else
	return NULL;
#endif
}

void UNxForceFieldRadialComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("Shape")) != NULL)
		{
			// original Shape object is gone
			if(Owner)
			{
				Owner->DetachComponent(DrawComponent);
			}
			
			DrawComponent = NULL;

			// if new Shape object is available
			if (Shape && Shape->eventGetDrawComponent())
			{
				DrawComponent = Shape->eventGetDrawComponent();
				Shape->eventFillBySphere(ForceRadius);
				// attach after the size is ready.
				if(Owner)
				{
					Owner->AttachComponent(DrawComponent);
				}
			}
		}
		else if (Shape && Shape->eventGetDrawComponent())
		{
			FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
			if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("ForceRadius")) != NULL)
			{
				Shape->eventFillBySphere(ForceRadius);
			}
		}
	}

}

#if WITH_EDITOR
void UNxForceFieldRadialComponent::EditorApplyScale( const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown )
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
	ForceRadius += Multiplier * ModifiedScale.Size();
	ForceRadius = Max( 0.f, ForceRadius );

	if (Shape && Shape->eventGetDrawComponent())
	{
		FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
		Shape->eventFillBySphere(ForceRadius);
	}
}
#endif

void UNxForceFieldCylindricalComponent::CreateKernel()
{
	check(Kernel == NULL);
#if WITH_NOVODEX
	Kernel = new NxForceFieldKernelSample;
#endif
}

void UNxForceFieldCylindricalComponent::TermComponentRBPhys( FRBPhysScene *Scene )
{
	Super::TermComponentRBPhys(Scene);
#if WITH_NOVODEX
	if ( Kernel )
	{
		delete Kernel;
		Kernel = NULL;
	}
#endif
}

void UNxForceFieldCylindricalComponent::DefineForceFunction( FPointer ForceFieldDesc )
{
#if WITH_NOVODEX
	Kernel->setBaseRadius(U2PScale * ForceRadius);
	Kernel->setEscapeVelocitySq(U2PScale*U2PScale*EscapeVelocity*EscapeVelocity);
	Kernel->setEyeRadius(0.0f); //?
	Kernel->setLiftFalloffHeight(U2PScale*LiftFalloffHeight);
	Kernel->setLiftStrength(LiftStrength);
	Kernel->setMinOutwardVelocity(0.0f); //?
	Kernel->setRadialStrength(RadialStrength);
	Kernel->setRadiusDelta((ForceRadius-ForceTopRadius)*U2PScale); //?
	Kernel->setRecipOneMinusLiftFalloffHeight(1.0f/(U2PScale*LiftFalloffHeight));
	Kernel->setRecipTornadoHeight(1.0f/(U2PScale*ForceHeight));
	Kernel->setRotationalStrength(RotationalStrength);
	Kernel->setTornadoHeight(U2PScale*ForceHeight);
	Kernel->setUseSpecialRadialForce(UseSpecialRadialForce);

	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	ffDesc.kernel = Kernel;
	ffDesc.coordinates = NX_FFC_CYLINDRICAL;
#endif
}

FPointer UNxForceFieldCylindricalComponent::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	NxForceFieldShapeDesc* ShapeDesc = (Shape ? Shape->CreateNxDesc() : NULL);
	ShapeDesc->pose.t.y += U2PScale*HeightOffset;
	// make the PhysX capsule z upwards.
	NxMat34 rot;
	rot.M.rotX(-NxPi/2);
	ShapeDesc->pose.multiply(rot, ShapeDesc->pose);
	return ShapeDesc;
#else
	return NULL;
#endif
}

void UNxForceFieldCylindricalComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("Shape")) != NULL)
		{
			// original Shape object is gone
			if(Owner)
			{
				Owner->DetachComponent(DrawComponent);
			}
			DrawComponent = NULL;

			// if new Shape object is available
			if (Shape && Shape->eventGetDrawComponent())
			{
				DrawComponent = Shape->eventGetDrawComponent();
				Shape->eventFillByCapsule(ForceHeight, ForceRadius);
				// attach after the size is ready.
				if(Owner)
				{
					Owner->AttachComponent(DrawComponent);
				}
			}
		}
		else if (Shape && Shape->eventGetDrawComponent())
		{
			FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
			if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("ForceRadius")) != NULL
				|| appStrstr(*(PropertyThatChanged->GetName()), TEXT("ForceHeight")) != NULL)
			{
				Shape->eventFillByCapsule(ForceHeight, ForceRadius);
			}
		}
	}
}
#if WITH_EDITOR
void UNxForceFieldCylindricalComponent::EditorApplyScale( const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown )
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if(!ModifiedScale.IsUniform())
	{
		const FLOAT XZMultiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
		const FLOAT YMultiplier = ( ModifiedScale.Y > 0.0f ) ? 1.0f : -1.0f;

		float x = ModifiedScale.X;
		float z = ModifiedScale.Z;
		ForceRadius += XZMultiplier * appSqrt(x*x+z*z);
		ForceTopRadius += XZMultiplier * appSqrt(x*x+z*z);
		ForceHeight += YMultiplier * Abs(ModifiedScale.Y);
	}
	else
	{
		const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		ForceRadius += Multiplier * ModifiedScale.Size();
		ForceTopRadius += Multiplier * ModifiedScale.Size();
		ForceHeight += Multiplier * ModifiedScale.Size();
	}

	ForceRadius = Max( 0.0f, ForceRadius );
	ForceHeight = Max( 0.0f, ForceHeight );

	PostEditChange();
}
#endif

void UNxForceFieldGenericComponent::CreateKernel()
{
#if WITH_NOVODEX
	//Create the Kernel with the appropriate settings
	check(Kernel == NULL);
	NxScene* nxScene = RBPhysScene->GetNovodexPrimaryScene();
	NxForceFieldLinearKernelDesc kernelDesc;
	Kernel = UserForceFieldLinearKernel::Create(nxScene->createForceFieldLinearKernel(kernelDesc), nxScene);
#endif
}

void UNxForceFieldGenericComponent::TermComponentRBPhys( FRBPhysScene *Scene )
{
	Super::TermComponentRBPhys(Scene);
#if WITH_NOVODEX
	if (Kernel && Scene)
	{
		NxScene* nxScene = Scene->GetNovodexPrimaryScene();
		if(nxScene->checkResults(NX_ALL_FINISHED, false))
		{
			GNovodexPendingKillForceFieldLinearKernels.AddItem(Kernel);
		}
		else
		{
			Kernel->Destroy();
		}
	}
	Kernel = NULL;
#endif
}

void UNxForceFieldGenericComponent::DefineForceFunction( FPointer ForceFieldDesc )
{
#if WITH_NOVODEX
	NxForceFieldLinearKernelDesc lKernelDesc;

	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	// Define force function
	switch (Coordinates)
	{
	case FFG_CARTESIAN		: ffDesc.coordinates = NX_FFC_CARTESIAN; break;
	case FFG_SPHERICAL		: ffDesc.coordinates = NX_FFC_SPHERICAL; break;
	case FFG_CYLINDRICAL	: ffDesc.coordinates = NX_FFC_CYLINDRICAL; break;
	case FFG_TOROIDAL		: ffDesc.coordinates = NX_FFC_TOROIDAL; break;
	}

	Kernel->setConstant(U2NVectorCopy(Constant));

	Kernel->setPositionMultiplier(NxMat33(
		U2NVectorCopy(PositionMultiplierX),
		U2NVectorCopy(PositionMultiplierY),
		U2NVectorCopy(PositionMultiplierZ)
		));
	Kernel->setPositionTarget(U2NPosition(PositionTarget));

	Kernel->setVelocityMultiplier(NxMat33(
		U2NVectorCopy(VelocityMultiplierX),
		U2NVectorCopy(VelocityMultiplierY),
		U2NVectorCopy(VelocityMultiplierZ)
		));
	Kernel->setVelocityTarget(U2NPosition(VelocityTarget));

	Kernel->setNoise(U2NVectorCopy(Noise));

	Kernel->setFalloffLinear(U2NVectorCopy(FalloffLinear));
	Kernel->setFalloffQuadratic(U2NVectorCopy(FalloffQuadratic));

	Kernel->setTorusRadius(U2PScale*TorusRadius);

	ffDesc.kernel = *Kernel;
#endif
}

FPointer UNxForceFieldGenericComponent::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	return Shape ? Shape->CreateNxDesc() : NULL;
#else
	return NULL;
#endif
}

void UNxForceFieldGenericComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("Shape")) != NULL)
		{
			// original Shape object is gone
			if(Owner)
			{
				Owner->DetachComponent(DrawComponent);
			}
			DrawComponent = NULL;

			// if new Shape object is available
			if (Shape && Shape->eventGetDrawComponent())
			{
				DrawComponent = Shape->eventGetDrawComponent();
				Shape->eventFillByBox(FVector(RoughExtentX, RoughExtentY, RoughExtentZ));
				if(Owner)
				{
					Owner->AttachComponent(DrawComponent);
				}
			}
		}
		else if (Shape && Shape->eventGetDrawComponent())
		{
			FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
			if ((appStrstr(*(PropertyThatChanged->GetName()), TEXT("RoughExtentX")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("RoughExtentY")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("RoughExtentZ")) != NULL))
			{
				Shape->eventFillByBox(FVector(RoughExtentX, RoughExtentY, RoughExtentZ));
			}
		}
	}
}
#if WITH_EDITOR
void UNxForceFieldGenericComponent::EditorApplyScale( const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown )
{
	FVector ModifiedScale = DeltaScale*500;

	RoughExtentX += ModifiedScale.X;
	RoughExtentY += ModifiedScale.Y;
	RoughExtentZ += ModifiedScale.Z;

	RoughExtentX = Max(0.0f, RoughExtentX);
	RoughExtentY = Max(0.0f, RoughExtentY);
	RoughExtentZ = Max(0.0f, RoughExtentZ);

	if (Shape && Shape->eventGetDrawComponent())
	{
		FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
		Shape->eventFillByBox(FVector(RoughExtentX, RoughExtentY, RoughExtentZ));
	}
}
#endif

void UNxForceFieldTornadoComponent::CreateKernel()
{
	check(Kernel == NULL);
#if WITH_NOVODEX
	Kernel = new NxForceFieldKernelTornadoAngular;
#endif
}

void UNxForceFieldTornadoComponent::TermComponentRBPhys( FRBPhysScene *Scene )
{
	Super::TermComponentRBPhys(Scene);
#if WITH_NOVODEX
	if ( Kernel )
	{
		delete Kernel;
		Kernel = NULL;
	}
#endif
}

void UNxForceFieldTornadoComponent::DefineForceFunction( FPointer ForceFieldDesc )
{
#if WITH_NOVODEX
	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	Kernel->setTornadoHeight(U2PScale*ForceHeight);
	Kernel->setRadius(U2PScale*ForceRadius);
	Kernel->setRadiusTop(U2PScale*ForceTopRadius);
	Kernel->setEscapeVelocitySq(U2PScale*U2PScale*EscapeVelocity*EscapeVelocity);
	Kernel->setRotationalStrength(RotationalStrength);
	Kernel->setRadialStrength(RadialStrength);
	Kernel->setBSpecialRadialForce(BSpecialRadialForceMode);
	Kernel->setLiftFallOffHeight(U2PScale*LiftFalloffHeight);
	Kernel->setLiftStrength(LiftStrength);
	Kernel->setSelfRotationStrength(SelfRotationStrength);

	ffDesc.kernel = Kernel;
	ffDesc.coordinates = NX_FFC_CYLINDRICAL;
#endif
}

FPointer UNxForceFieldTornadoComponent::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	NxForceFieldShapeDesc* ShapeDesc = (Shape ? Shape->CreateNxDesc() : NULL);
	ShapeDesc->pose.t.y += U2PScale*HeightOffset;
	// make the PhysX capsule z upwards.
	NxMat34 rot;
	rot.M.rotX(-NxPi/2);
	ShapeDesc->pose.multiply(rot, ShapeDesc->pose);
	return ShapeDesc;
#else
	return NULL;
#endif
}

void UNxForceFieldTornadoComponent::PostEditChangeProperty( FPropertyChangedEvent& PropertyChangedEvent )
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if (PropertyThatChanged)
	{
		if (appStrstr(*(PropertyThatChanged->GetName()), TEXT("Shape")) != NULL)
		{
			// original Shape object is gone
			if(Owner)
			{
				Owner->DetachComponent(DrawComponent);
			}
			DrawComponent = NULL;

			// if new Shape object is available
			if (Shape && Shape->eventGetDrawComponent())
			{
				DrawComponent = Shape->eventGetDrawComponent();
				Shape->eventFillByCylinder(ForceRadius, ForceTopRadius, ForceHeight, HeightOffset);
				// attach after the size is ready.
				if(Owner)
				{
					Owner->AttachComponent(DrawComponent);
				}	
			}
		}
		else if (Shape && Shape->eventGetDrawComponent())
		{
			FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
			if ((appStrstr(*(PropertyThatChanged->GetName()), TEXT("ForceRadius")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("ForceTopRadius")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("ForceHeight")) != NULL)
				||(appStrstr(*(PropertyThatChanged->GetName()), TEXT("HeightOffset")) != NULL))
			{
				Shape->eventFillByCylinder(ForceRadius, ForceTopRadius, ForceHeight, HeightOffset);
			}

		}
	}
}

#if WITH_EDITOR
void UNxForceFieldTornadoComponent::EditorApplyScale( const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown )
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if(!ModifiedScale.IsUniform())
	{
		const FLOAT XYMultiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f ) ? 1.0f : -1.0f;
		const FLOAT ZMultiplier = ( ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		ForceRadius += XYMultiplier * ModifiedScale.Size2D();
		ForceTopRadius += XYMultiplier * ModifiedScale.Size2D();
		ForceHeight += ZMultiplier * Abs(ModifiedScale.Z);
	}
	else
	{
		const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		ForceRadius += Multiplier * ModifiedScale.Size();
		ForceTopRadius += Multiplier * ModifiedScale.Size();
		ForceHeight += Multiplier * ModifiedScale.Size();
	}

	ForceRadius = Max( 0.f, ForceRadius );
	ForceTopRadius = Max(0.0f, ForceTopRadius);
	ForceHeight = Max( 0.0f, ForceHeight );

	if (Shape && Shape->eventGetDrawComponent())
	{
		FComponentReattachContext ReattachContext(Shape->eventGetDrawComponent());
		Shape->eventFillByCylinder(ForceRadius, ForceTopRadius, ForceHeight, HeightOffset);
	}
}
#endif

