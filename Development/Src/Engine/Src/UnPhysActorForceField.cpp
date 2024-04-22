/**
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"
#include "EngineForceFieldClasses.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif // WITH_NOVODEX

#include "ForceFunctionSample.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#include "UserForceField.h"
#include "UserForceFieldLinearKernel.h"
#include "UserForceFieldShape.h"
#include "UserForceFieldShapeGroup.h"
#include "ForceFieldExcludeChannel.h"
#endif // WITH_NOVODEX

IMPLEMENT_CLASS(ANxForceField);
IMPLEMENT_CLASS(ANxRadialForceField);
IMPLEMENT_CLASS(ANxCylindricalForceField);
IMPLEMENT_CLASS(ANxCylindricalForceFieldCapsule);
IMPLEMENT_CLASS(ANxForceFieldSpawnable);
IMPLEMENT_CLASS(ANxGenericForceField);
IMPLEMENT_CLASS(ANxGenericForceFieldBox);
IMPLEMENT_CLASS(ANxGenericForceFieldCapsule);
IMPLEMENT_CLASS(ANxGenericForceFieldBrush);

extern TMap<INT, struct ForceFieldExcludeChannel*> GNovodexForceFieldExcludeChannelsMap;
extern TArray<class UserForceFieldShapeGroup* >	GNovodexPendingKillForceFieldShapeGroups;
IMPLEMENT_CLASS(ARB_ForceFieldExcludeVolume);

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxRADIALFORCEACTOR /////////////////
void ANxRadialForceField::InitRBPhys()
{
#if WITH_NOVODEX
	NxScene* nxScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	NxForceFieldLinearKernelDesc kernelDesc;
	check(LinearKernel == NULL);
	WaitForNovodexScene(*nxScene);
	LinearKernel = UserForceFieldLinearKernel::Create(nxScene->createForceFieldLinearKernel(kernelDesc), nxScene);

	Super::InitRBPhys();
#endif
}

void ANxRadialForceField::TermRBPhys(FRBPhysScene* Scene) 
{
#if WITH_NOVODEX
	Super::TermRBPhys(Scene);

	if (LinearKernel && Scene)
	{
		NxScene* nxScene = Scene->GetNovodexPrimaryScene();
		if(nxScene->checkResults(NX_ALL_FINISHED, false))
		{
			GNovodexPendingKillForceFieldLinearKernels.AddItem(LinearKernel);
		}
		else
		{
			LinearKernel->Destroy();
		}
	}
	LinearKernel = NULL;
#endif
}

void ANxRadialForceField::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);
}

#if WITH_EDITOR
void ANxRadialForceField::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
	ForceRadius += Multiplier * ModifiedScale.Size();
	ForceRadius = Max( 0.f, ForceRadius );
	PostEditChange();
}
#endif

/** Update the render component to match the force radius. */
void ANxRadialForceField::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		FComponentReattachContext ReattachContext(RenderComponent);
		RenderComponent->SphereRadius = ForceRadius;
	}
}

void ANxRadialForceField::DefineForceFunction(FPointer ForceFieldDesc)
{
#if WITH_NOVODEX
	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	ffDesc.coordinates = NX_FFC_SPHERICAL;

	// Define force function
	if (ForceFalloff == RIF_Linear)
	{
		NxReal nxRadius = U2PScale * ForceRadius;

		NxMat33 m(NxVec3(ForceStrength/nxRadius,0,0), NxVec3(0,0,0), NxVec3(0,0,0));
		
		LinearKernel->setPositionTarget(NxVec3(nxRadius, 0, 0));
		LinearKernel->setPositionMultiplier(m);
	}
	else 
	{
		LinearKernel->setConstant(NxVec3(ForceStrength,0,0));
	}

	ffDesc.kernel = *LinearKernel;
#endif
}

FPointer ANxRadialForceField::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	NxSphereForceFieldShapeDesc* ffShapeDesc = new NxSphereForceFieldShapeDesc();
	
	NxReal nxRadius = U2PScale * ForceRadius;
	ffShapeDesc->radius = nxRadius;

	return ffShapeDesc;
#else
	return NULL;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxCylindricalForceField /////////////////

void ANxCylindricalForceField::DefineForceFunction(FPointer ForceFieldDesc)
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


void ANxCylindricalForceField::InitRBPhys()
{
#if WITH_NOVODEX
	check(Kernel == NULL);
	Kernel = new NxForceFieldKernelSample;

	Super::InitRBPhys();
#endif
}

void ANxCylindricalForceField::TermRBPhys(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	Super::TermRBPhys(Scene);

	delete Kernel;
	Kernel = NULL;
#endif
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxCylindricalForceFieldCapsule /////////////////

void ANxCylindricalForceFieldCapsule::DoInitRBPhys()
{
#if WITH_NOVODEX
	Kernel = NULL;
	ForceField = NULL;
	InitRBPhys();
#endif
}

void ANxCylindricalForceFieldCapsule::InitRBPhys()
{
	Super::InitRBPhys();
}

void ANxCylindricalForceFieldCapsule::TermRBPhys(FRBPhysScene* Scene) 
{
	Super::TermRBPhys(Scene);
}


void ANxCylindricalForceFieldCapsule::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);
}

#if WITH_EDITOR
void ANxCylindricalForceFieldCapsule::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
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

/** Update the render component to match the force radius. */
void ANxCylindricalForceFieldCapsule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		{
			FComponentReattachContext ReattachContext(RenderComponent);
			RenderComponent->CapsuleRadius = ForceRadius;
			RenderComponent->CapsuleHeight = ForceHeight;
		}
	}
}

FPointer ANxCylindricalForceFieldCapsule::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	// Capsule
	NxCapsuleForceFieldShapeDesc* ffShapeDesc = new NxCapsuleForceFieldShapeDesc();
	//NxCapsuleForceFieldShapeDesc ffShapeDesc;
	ffShapeDesc->radius = U2PScale*ForceRadius;
	ffShapeDesc->height = U2PScale*ForceHeight;
	ffShapeDesc->pose.t.y += U2PScale*HeightOffset;

	return ffShapeDesc;
#else
	return NULL;
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxForceField /////////////////

void ANxForceField::DoInitRBPhys()
{
#if WITH_NOVODEX
	InitRBPhys();
#endif
}


void ANxForceField::InitRBPhys()
{
#if WITH_NOVODEX
	NxMat33* rot = new NxMat33(); rot->id();
	U2NRotation = rot;

	if (!GWorld->RBPhysScene)
		return;

	NxScene* nxScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();

	if (!nxScene)
		return;

	WaitForNovodexScene(*nxScene);

	// Fill out ForceField Descriptor
	NxForceFieldDesc ffDesc;
	ffDesc.actor = NULL; // attachment handled by UE

	ffDesc.fluidType = NX_FF_TYPE_GRAVITATIONAL;

	if (bForceActive)
		ffDesc.groupsMask = CreateGroupsMask(RBChannel, &CollideWithChannels);
	else
		ffDesc.groupsMask = CreateGroupsMask(RBCC_Nothing, NULL);

	ffDesc.pose.M = U2NQuaternion(Rotation.Quaternion()); 
	ffDesc.pose.t = U2NPosition(Location);

	DefineForceFunction(&ffDesc);
	SetForceFieldPose(&ffDesc);
	
/*rongbo. temptest.	if (!bForceApplyToRigidBodies)
		ffDesc.rigidBodyScale = 0.0f;
	else
		ffDesc.rigidBodyScale = RigidBodyScale;

	if (!bForceApplyToCloth)
		ffDesc.clothScale = 0.0f;
	else
		ffDesc.clothScale = ClothScale;

	if (!bForceApplyToFluid)
		ffDesc.fluidScale = 0.0f;
	else
		ffDesc.fluidScale = FluidScale;*/
	

	// Create ForceField
	check(ForceField == NULL);
	UBOOL bRotateForceField = FALSE;
	if ( this->IsA(ANxForceFieldTornado::StaticClass()) )
	{
		bRotateForceField = TRUE;
	}
	ForceField = UserForceField::Create(nxScene->createForceField(ffDesc), bRotateForceField);

	CreateExclusionShapes(nxScene);

	NxForceFieldShapeDesc* ffShapeDesc = (NxForceFieldShapeDesc*)DefineForceFieldShapeDesc();

	if (ffShapeDesc)
	{
		if (ffShapeDesc->getType() == NX_SHAPE_CONVEX)
		{// a little hack
			NxConvexForceFieldShapeDesc* desc = (NxConvexForceFieldShapeDesc*)ffShapeDesc;
			ConvexMeshes.AddItem(desc->meshData);
		}
		ForceField->getIncludeShapeGroup().createShape(*ffShapeDesc);
		delete ffShapeDesc;
	}

	SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;
#endif
}

void ANxForceField::TermRBPhys(FRBPhysScene* Scene) 
{
#if WITH_NOVODEX
	// If the force field is not in the scene we want, do nothing.
	if( (Scene != NULL) && (SceneIndex != Scene->NovodexSceneIndex) )
	{
		return;
	}

	if (U2NRotation)
	{
		NxMat33* nxU2NRotation = (NxMat33*) U2NRotation;
		delete nxU2NRotation;
		U2NRotation = NULL;
	}

	/*
	while (ExclusionShapePoses.Num() > 0)
	{
		NxMat34* m = (NxMat34*)ExclusionShapePoses.Pop();
		delete m;
	}
	*/

	NxScene* NovodexScene = NULL;
	if( ForceField )
	{
		NovodexScene = GetNovodexPrimarySceneFromIndex(SceneIndex);
	}
	if( NovodexScene )
	{
		if(NovodexScene->checkResults(NX_ALL_FINISHED, false))
		{
			// (1)
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


void ANxForceField::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);

#if WITH_NOVODEX
	if (!ForceField)
		return;

	//if (bHardAttach && Base)
	//if (Base)
	if (1)
	{
		NxMat34 m;

		if (bIgnoreBaseRotation)
		{
			m.M = ForceField->getPose().M;
		}
		else
		{
			NxMat33* rot = (NxMat33*)U2NRotation;
			NxQuat q(NxVec3(0.0f,0.0f,0.0f),1.0f); rot->toQuat(q);
			m.M = U2NQuaternion(Rotation.Quaternion()) * q;
		}

		m.t = U2NPosition(Location);
#if 0//SUPPORT_DOUBLE_BUFFERING
		ForceFieldPosePair pair;
		pair.ForceField = ForceField;
		pair.Pose = m;
		GNovodexPendingForceFieldSetPose.AddItem(pair);
#else
		ForceField->setPose(m);
#endif
	}

	if (bForceActive)
	{
		NxGroupsMask groupsMask = CreateGroupsMask(RBChannel, &CollideWithChannels);
#if 0//SUPPORT_DOUBLE_BUFFERING
		ForceFieldGroupsMaskPair pair;
		pair.ForceField = ForceField;
		pair.GroupsMask = groupsMask;
		GNovodexPendingForceFieldSetGroupsMask.AddItem(pair);
#else
		ForceField->setGroupsMask(groupsMask);
#endif

		// hack to make the collision volume static
		/*
		for (int i = 0; i < ExclusionShapes.Num(); i++)
		{
			NxForceFieldShape* ffShape = (NxForceFieldShape*)ExclusionShapes(i);
			NxMat34* globalPose = (NxMat34*)ExclusionShapePoses(i);
			NxMat34 invForcePose; nxForceField->getPose().getInverse(invForcePose);

#if SUPPORT_DOUBLE_BUFFERING
			ForceFieldShapePosePair pair;
			pair.ForceFieldShape = ffShape;
			pair.Pose = invForcePose * *globalPose;
			GNovodexPendingForceFieldShapeSetPose.AddItem(pair);
#else
			ffShape->setPose(invForcePose * *globalPose);
#endif
		}
		*/

	}
	else 
	{
		NxGroupsMask groupsMask = CreateGroupsMask(RBCC_Nothing, NULL);
#if 0//SUPPORT_DOUBLE_BUFFERING
		ForceFieldGroupsMaskPair pair;
		pair.ForceField = ForceField;
		pair.GroupsMask = groupsMask;
		GNovodexPendingForceFieldSetGroupsMask.AddItem(pair);
#else
		ForceField->setGroupsMask(groupsMask);
#endif
	}
#endif
}

void ANxForceField::DefineForceFunction(FPointer ForceFieldDesc)
{
	// implemented in subclass
}

FPointer ANxForceField::DefineForceFieldShapeDesc()
{
	// implemented in subclass
	return NULL;
}

void ANxForceField::SetForceFieldPose(FPointer ForceFieldDesc)
{
}

#if WITH_NOVODEX
void ANxForceField::CreateExclusionShapes(NxScene* nxScene)
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


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxGenericForceField /////////////////
void ANxGenericForceField::InitRBPhys()
{
#if WITH_NOVODEX
	NxScene* nxScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	NxForceFieldLinearKernelDesc kernelDesc;
	check(LinearKernel == NULL);
	WaitForNovodexScene(*nxScene);
	LinearKernel = UserForceFieldLinearKernel::Create(nxScene->createForceFieldLinearKernel(kernelDesc), nxScene);

	Super::InitRBPhys();
#endif
}

void ANxGenericForceField::TermRBPhys(FRBPhysScene* Scene)
{
#if WITH_NOVODEX
	Super::TermRBPhys(Scene);

	if (LinearKernel && Scene)
	{
		NxScene* nxScene = Scene->GetNovodexPrimaryScene();
		if(nxScene->checkResults(NX_ALL_FINISHED, false))
		{
			GNovodexPendingKillForceFieldLinearKernels.AddItem(LinearKernel);
		}
		else
		{
			LinearKernel->Destroy();
		}
	}
	LinearKernel = NULL;
#endif
}

void ANxGenericForceField::TickSpecial(FLOAT DeltaSeconds)
{
	Super::TickSpecial(DeltaSeconds);
}

void ANxGenericForceField::DefineForceFunction(FPointer ForceFieldDesc)
{
#if WITH_NOVODEX
	NxForceFieldLinearKernelDesc lKernelDesc;

	NxForceFieldDesc& ffDesc = *(NxForceFieldDesc*)ForceFieldDesc;

	// Define force function
	switch (Coordinates)
	{
	case FFB_CARTESIAN		: ffDesc.coordinates = NX_FFC_CARTESIAN; break;
	case FFB_SPHERICAL		: ffDesc.coordinates = NX_FFC_SPHERICAL; break;
	case FFB_CYLINDRICAL	: ffDesc.coordinates = NX_FFC_CYLINDRICAL; break;
	case FFB_TOROIDAL		: ffDesc.coordinates = NX_FFC_TOROIDAL; break;
	}

	LinearKernel->setConstant(U2NVectorCopy(Constant));

	LinearKernel->setPositionMultiplier(NxMat33(
		U2NVectorCopy(PositionMultiplierX),
		U2NVectorCopy(PositionMultiplierY),
		U2NVectorCopy(PositionMultiplierZ)
		));
	LinearKernel->setPositionTarget(U2NPosition(PositionTarget));

	LinearKernel->setVelocityMultiplier(NxMat33(
		U2NVectorCopy(VelocityMultiplierX),
		U2NVectorCopy(VelocityMultiplierY),
		U2NVectorCopy(VelocityMultiplierZ)
		));
	LinearKernel->setVelocityTarget(U2NPosition(VelocityTarget));

	LinearKernel->setNoise(U2NVectorCopy(Noise));

	LinearKernel->setFalloffLinear(U2NVectorCopy(FalloffLinear));
	LinearKernel->setFalloffQuadratic(U2NVectorCopy(FalloffQuadratic));

	//TODO hermes
	//lKernelDesc.torusRadius = TorusRadius;

	ffDesc.kernel = *LinearKernel;
#endif
}

FPointer ANxGenericForceField::DefineForceFieldShapeDesc()
{
	// implemented in subclass
	return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxGenericForceFieldBox /////////////////

void ANxGenericForceFieldBox::DoInitRBPhys()
{
#if WITH_NOVODEX
	ForceField = NULL;
	LinearKernel = NULL;
	InitRBPhys();
#endif
}

#if WITH_EDITOR
void ANxGenericForceFieldBox::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	const FLOAT XMultiplier = ( ModifiedScale.X > 0.0f ) ? 1.0f : -1.0f;
	const FLOAT YMultiplier = ( ModifiedScale.Y > 0.0f ) ? 1.0f : -1.0f;
	const FLOAT ZMultiplier = ( ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

	BoxExtent.X += XMultiplier * Abs(ModifiedScale.X);
	BoxExtent.Y += YMultiplier * Abs(ModifiedScale.Y);
	BoxExtent.Z += ZMultiplier * Abs(ModifiedScale.Z);

	PostEditChange();
}
#endif

/** Update the render component to match the force radius. */
void ANxGenericForceFieldBox::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		FComponentReattachContext ReattachContext(RenderComponent);
		RenderComponent->BoxExtent = BoxExtent;
	}
}

FPointer ANxGenericForceFieldBox::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	NxBoxForceFieldShapeDesc* ffShapeDesc = new NxBoxForceFieldShapeDesc();

	ffShapeDesc->dimensions = U2NPosition(BoxExtent);
	return ffShapeDesc;
#else
	return NULL;
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxGenericForceFieldCapsule /////////////////

#if WITH_EDITOR
void ANxGenericForceFieldCapsule::EditorApplyScale(const FVector& DeltaScale, const FMatrix& ScaleMatrix, const FVector* PivotLocation, UBOOL bAltDown, UBOOL bShiftDown, UBOOL bCtrlDown)
{
	const FVector ModifiedScale = DeltaScale * 500.0f;

	if(!ModifiedScale.IsUniform())
	{
		const FLOAT XZMultiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;
		const FLOAT YMultiplier = ( ModifiedScale.Y > 0.0f ) ? 1.0f : -1.0f;

		float x = ModifiedScale.X;
		float z = ModifiedScale.Z;
		CapsuleRadius += XZMultiplier * appSqrt(x*x+z*z);
		CapsuleHeight += YMultiplier * Abs(ModifiedScale.Y);
	}
	else
	{
		const FLOAT Multiplier = ( ModifiedScale.X > 0.0f || ModifiedScale.Y > 0.0f || ModifiedScale.Z > 0.0f ) ? 1.0f : -1.0f;

		CapsuleRadius += Multiplier * ModifiedScale.Size();
		CapsuleHeight += Multiplier * ModifiedScale.Size();
	}

	CapsuleRadius = Max( 0.0f, CapsuleRadius );
	CapsuleHeight = Max( 0.0f, CapsuleHeight );

	PostEditChange();
}
#endif

/** Update the render component to match the force radius. */
void ANxGenericForceFieldCapsule::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(RenderComponent)
	{
		FComponentReattachContext ReattachContext(RenderComponent);
		RenderComponent->CapsuleRadius = CapsuleRadius;
		RenderComponent->CapsuleHeight = CapsuleHeight;
	}
}

FPointer ANxGenericForceFieldCapsule::DefineForceFieldShapeDesc()
{
#if WITH_NOVODEX
	NxCapsuleForceFieldShapeDesc* ffShapeDesc = new NxCapsuleForceFieldShapeDesc();

	// Create ForceFieldShape
	ffShapeDesc->height = U2PScale * CapsuleHeight;
	ffShapeDesc->radius = U2PScale * CapsuleRadius;

	return ffShapeDesc;
#else
	return NULL;
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ANxGenericForceFieldBrush /////////////////

void ANxGenericForceFieldBrush::InitRBPhys()
{
#if WITH_NOVODEX
	Super::InitRBPhys();

	if (!GWorld->RBPhysScene)
		return;

	NxScene* nxScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();

	if (!nxScene)
		return;
	WaitForNovodexScene(*nxScene);

	NxForceFieldDesc ffDesc;
	ffDesc.pose.t = U2NPosition(Location);

	ffDesc.fluidType = NX_FF_TYPE_GRAVITATIONAL;

	ffDesc.groupsMask = CreateGroupsMask(RBCC_Default, &CollideWithChannels);

	ffDesc.pose.M = U2NQuaternion(Rotation.Quaternion()); 
	ffDesc.pose.t = U2NPosition(Location);


// Define force function
	
	switch (Coordinates)
	{
	case FFB_CARTESIAN		: ffDesc.coordinates = NX_FFC_CARTESIAN; break;
	case FFB_SPHERICAL		: ffDesc.coordinates = NX_FFC_SPHERICAL; break;
	case FFB_CYLINDRICAL	: ffDesc.coordinates = NX_FFC_CYLINDRICAL; break;
	case FFB_TOROIDAL		: ffDesc.coordinates = NX_FFC_TOROIDAL; break;
	}

	NxForceFieldLinearKernelDesc lKernelDesc;

	lKernelDesc.constant = U2NVectorCopy(Constant);

	lKernelDesc.positionMultiplier = NxMat33(
		U2NVectorCopy(PositionMultiplierX),
		U2NVectorCopy(PositionMultiplierY),
		U2NVectorCopy(PositionMultiplierZ)
		);
	lKernelDesc.positionTarget = U2NPosition(PositionTarget);

	lKernelDesc.velocityMultiplier = NxMat33(
		U2NVectorCopy(VelocityMultiplierX),
		U2NVectorCopy(VelocityMultiplierY),
		U2NVectorCopy(VelocityMultiplierZ)
		);
	lKernelDesc.velocityTarget = U2NPosition(VelocityTarget);

	lKernelDesc.noise = U2NVectorCopy(Noise);

	lKernelDesc.falloffLinear = U2NVectorCopy(FalloffLinear);
	lKernelDesc.falloffQuadratic = U2NVectorCopy(FalloffQuadratic);

	// lKernelDesc.torusRadius = TorusRadius; //?

	
/*rongbo.temptest.	if (!bForceApplyToRigidBodies)
		ffDesc.rigidBodyScale = 0.0f;
	else
		ffDesc.rigidBodyScale = RigidBodyScale;

	if (!bForceApplyToCloth)
		ffDesc.clothScale = 0.0f;
	else
		ffDesc.clothScale = ClothScale;

	if (!bForceApplyToFluid)
		ffDesc.fluidScale = 0.0f;
	else
		ffDesc.fluidScale = FluidScale;*/
	

	// Create the force field

	WaitForNovodexScene(*nxScene);
	check(LinearKernel == NULL);
	LinearKernel = UserForceFieldLinearKernel::Create(nxScene->createForceFieldLinearKernel(lKernelDesc), nxScene);
	ffDesc.kernel = *LinearKernel;

	check(ForceField == NULL);
	ForceField = UserForceField::Create(nxScene->createForceField(ffDesc), FALSE);

	// Iterate over all prim components in this actor, creating collision geometry for each one.
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UActorComponent* ActorComp = Components(ComponentIndex);
		if(ActorComp && ActorComp->IsA(UBrushComponent::StaticClass()))
		{
			// Initialize any physics for this component.
			UBrushComponent* BrushComp = (UBrushComponent*)ActorComp;


			// If we don't have cooked data, cook now and warn.
			if( BrushComp->CachedPhysBrushData.CachedConvexElements.Num() == 0 || 
				BrushComp->CachedPhysBrushDataVersion != GCurrentCachedPhysDataVersion ||
				!bUsePrecookedPhysData)
			{
				checkMsg(Brush != NULL, "Cannot recook brush data that has already been cooked (the model has been stripped out)");

				debugf( TEXT("No Cached Brush Physics Data Found Or Out Of Date (%s) (Owner: %s) - Cooking Now."), *Brush->GetName(), *Owner->GetName() );

				BrushComp->BuildSimpleBrushCollision();
				BrushComp->BuildPhysBrushData();
			}


			// Only continue if we got some valid hulls for this model.
			if(BrushComp->CachedPhysBrushData.CachedConvexElements.Num() > 0)
			{
				//BrushComp->BrushAggGeom.InstanceNovodexForceField(*ForceField, BrushComp->Scale * BrushComp->Scale3D, &BrushComp->CachedPhysBrushData, FALSE, *GetFullName() );
				BrushComp->BrushAggGeom.InstanceNovodexForceField(ForceField->getIncludeShapeGroup(), DrawScale * DrawScale3D, &BrushComp->CachedPhysBrushData, FALSE, *GetFullName() );
			}

			// We don't need the cached physics data any more, so clear it
			BrushComp->CachedPhysBrushData.CachedConvexElements.Empty();
		}
	}

	// Both ANxGenericForceFieldBrush and ARB_ForceFieldExcludeVolume will do something in InitRBPhys() because of order is not known in advance.
	ForceFieldExcludeChannel* channel = GNovodexForceFieldExcludeChannelsMap.FindRef(ExcludeChannel);
	if (channel)
	{
		for (TMap<ARB_ForceFieldExcludeVolume*, UserForceFieldShapeGroup*>::TIterator i(channel->Groups); i; ++i)
		{
			UserForceFieldShapeGroup* group = i.Value();
			ForceField->addShapeGroup(*group);
		}
	}
#endif
}


void ANxGenericForceFieldBrush::TermRBPhys(FRBPhysScene* Scene) 
{
#if WITH_NOVODEX
	/*
	while (ExclusionShapePoses.Num() > 0)
	{
	NxMat34* m = (NxMat34*)ExclusionShapePoses.Pop();
	delete m;
	}
	*/

	if(ForceField && Scene)
	{
		check(LinearKernel);
		NxScene* nxScene = Scene->GetNovodexPrimaryScene();
		if(nxScene->checkResults(NX_ALL_FINISHED, false))
		{
			// (1)
			GNovodexPendingKillForceFields.AddItem(ForceField);
			GNovodexPendingKillForceFieldLinearKernels.AddItem(LinearKernel);
		}
		else
		{
			// (2)
			ForceField->Destroy();
			LinearKernel->Destroy();
		}

	}
	// Either we have deleted the force field directly (2)
	// or it will be deleted afterwards in the world tick (1)
	// or the scene has already been deleted (and therefore also the force field)
	ForceField = NULL;
	LinearKernel = NULL;

	while (ConvexMeshes.Num() > 0)
	{
		NxConvexMesh* convexMesh = (NxConvexMesh*)ConvexMeshes.Pop();
		GNovodexPendingKillConvex.AddItem(convexMesh);
	}

	Super::TermRBPhys(Scene);
#endif
}

void ANxGenericForceFieldBrush::TickSpecial(FLOAT DeltaSeconds)
{
#if WITH_NOVODEX
	Super::TickSpecial(DeltaSeconds);

	if (!ForceField)
		return;

	//if (bHardAttach && Base)
	if (Base)
	{
		NxMat34 m;

		if (bIgnoreBaseRotation)
		{
			m.M = ForceField->getPose().M;
		}
		else
		{
			m.M = U2NQuaternion(Rotation.Quaternion());
		}

		m.t = U2NPosition(Location);

#if 0//SUPPORT_DOUBLE_BUFFERING
		ForceFieldPosePair pair;
		pair.ForceField = ForceField;
		pair.Pose = m;
		GNovodexPendingForceFieldSetPose.AddItem(pair);
#else
		ForceField->setPose(m);
#endif
	}

	if (1)
	{
		NxGroupsMask groupsMask = CreateGroupsMask(RBChannel, &CollideWithChannels);
#if 0//SUPPORT_DOUBLE_BUFFERING
		ForceFieldGroupsMaskPair pair;
		pair.ForceField = ForceField;
		pair.GroupsMask = groupsMask;
		GNovodexPendingForceFieldSetGroupsMask.AddItem(pair);
#else
		ForceField->setGroupsMask(groupsMask);
#endif

		// hack to make the collision volume static
		/*
		for (int i = 0; i < ExclusionShapes.Num(); i++)
		{
			NxForceFieldShape* ffShape = (NxForceFieldShape*)ExclusionShapes(i);
			NxMat34* globalPose = (NxMat34*)ExclusionShapePoses(i);
			NxMat34 invForcePose; nxForceField->getPose().getInverse(invForcePose);
#if SUPPORT_DOUBLE_BUFFERING
			ForceFieldShapePosePair pair;
			pair.ForceFieldShape = ffShape;
			pair.Pose = invForcePose * *globalPose;
			GNovodexPendingForceFieldShapeSetPose.AddItem(pair);
#else
			ffShape->setPose(invForcePose * *globalPose);
#endif
		}
		*/

	}
#endif
}


///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////// ARB_ForceFieldExcludeVolume /////////////////

void ARB_ForceFieldExcludeVolume::InitRBPhys()
{
#if WITH_NOVODEX
	if (!GWorld->RBPhysScene)
	{
		return;
	}

	// TODO hermes ifs
	NxScene* nxScene = GWorld->RBPhysScene->GetNovodexPrimaryScene();
	WaitForNovodexScene(*nxScene);

	NxForceFieldShapeGroupDesc FFSGDesc;
	FFSGDesc.flags |= NX_FFSG_EXCLUDE_GROUP;
	UserForceFieldShapeGroup* ffsGroup = UserForceFieldShapeGroup::Create(nxScene->createForceFieldShapeGroup(FFSGDesc), nxScene);

	if (1)
	{
		ForceFieldExcludeChannel * channel = GNovodexForceFieldExcludeChannelsMap.FindRef(ForceFieldChannel);
		if (!channel)
		{
			channel = new ForceFieldExcludeChannel;
			GNovodexForceFieldExcludeChannelsMap.Set(ForceFieldChannel, channel);
		}
		else
		{
			check(!channel->Groups.HasKey(this));
			// every existing group in the ExcludeChannel should have the same force fields. Just pick up the first group and find all related force fields.
			TMap<ARB_ForceFieldExcludeVolume*, UserForceFieldShapeGroup*>::TIterator first(channel->Groups);
			first.Value()->PassAllForceFieldsTo(*ffsGroup);
		}

		channel->Groups.Set(this, ffsGroup);
	}

	// Iterate over all prim components in this actor, creating collision geometry for each one.
	for(UINT ComponentIndex = 0; ComponentIndex < (UINT)Components.Num(); ComponentIndex++)
	{
		UActorComponent* ActorComp = Components(ComponentIndex);
		if(ActorComp && ActorComp->IsA(UBrushComponent::StaticClass()))
		{
			// Initialize any physics for this component.
			UBrushComponent* BrushComp = (UBrushComponent*)ActorComp;


			// If we don't have cooked data, cook now and warn.
			if( BrushComp->CachedPhysBrushData.CachedConvexElements.Num() == 0 || 
				BrushComp->CachedPhysBrushDataVersion != GCurrentCachedPhysDataVersion ||
				!bUsePrecookedPhysData)
			{
				checkMsg(Brush != NULL, "Cannot recook brush data that has already been cooked (the model has been stripped out)");

				debugf( TEXT("No Cached Brush Physics Data Found Or Out Of Date (%s) (Owner: %s) - Cooking Now."), *Brush->GetName(), *Owner->GetName() );

				BrushComp->BuildSimpleBrushCollision();
				BrushComp->BuildPhysBrushData();
			}


			// Only continue if we got some valid hulls for this model.
			if(BrushComp->CachedPhysBrushData.CachedConvexElements.Num() > 0)
			{
				//BrushComp->BrushAggGeom.InstanceNovodexForceField(*nxForceField, BrushComp->Scale * BrushComp->Scale3D, &BrushComp->CachedPhysBrushData, FALSE, *GetFullName() );
				BrushComp->BrushAggGeom.InstanceNovodexForceField(*ffsGroup, DrawScale * DrawScale3D, &BrushComp->CachedPhysBrushData, FALSE, *GetFullName() );

				// set pose for the shapes.
				ffsGroup->resetShapesIterator();
				for (int i = 0; i < (int)ffsGroup->getNbShapes(); i++)
				{
					//UserForceFieldShape is not created during InstanceNovodexForceField yet. todo.
					NxForceFieldShape* shape = ffsGroup->getNextShape();
					NxMat34 pose;
					pose.t = U2NPosition(Location);
					pose.M = U2NQuaternion(Rotation.Quaternion());
					shape->setPose(pose);
				}
			}

			// We don't need the cached physics data any more, so clear it
			BrushComp->CachedPhysBrushData.CachedConvexElements.Empty();
		}
	}
	SceneIndex = GWorld->RBPhysScene->NovodexSceneIndex;
#endif
}

void ARB_ForceFieldExcludeVolume::TermRBPhys(FRBPhysScene* Scene) 
{
#if WITH_NOVODEX
	// If the forcefield ExcludeVolume is not in the scene we want, do nothing.
	if( (Scene != NULL) && (SceneIndex != Scene->NovodexSceneIndex) )
	{
		return;
	}
	ForceFieldExcludeChannel * channel = GNovodexForceFieldExcludeChannelsMap.FindRef(ForceFieldChannel);
	if (channel)
	{
		UserForceFieldShapeGroup * ffsGroup = channel->Groups.FindRef(this);
		if (ffsGroup)
		{
			NxScene* NovodexScene = NULL;
			NovodexScene = GetNovodexPrimarySceneFromIndex(SceneIndex);
			if (NovodexScene)
			{
				channel->Groups.Remove(this);

				if(NovodexScene->checkResults(NX_ALL_FINISHED, false))
				{
					GNovodexPendingKillForceFieldShapeGroups.AddItem(ffsGroup);
				}
				else
				{
					ffsGroup->Destroy();
				}
			}
		}
		if (channel->Groups.Num() == 0)
		{
			GNovodexForceFieldExcludeChannelsMap.Remove(ForceFieldChannel);
			delete channel;
		}
	}

	Super::TermRBPhys(Scene);
#endif
}

#if WITH_NOVODEX
void ScaleNovodexTMPosition(NxMat34& TM, const FVector& Scale3D)
{
	TM.t.x *= Scale3D.X;
	TM.t.y *= Scale3D.Y;
	TM.t.z *= Scale3D.Z;
}
#endif // WITH_NOVODEX
extern INT TotalConvexGeomCount;
extern DOUBLE TotalInstanceGeomTime;

#if WITH_NOVODEX
void FKAggregateGeom::InstanceNovodexForceField(UserForceFieldShapeGroup& ffsGroup, const FVector& uScale3D, FKCachedConvexData* InCacheData, UBOOL bCreateCCDSkel, const TCHAR* debugName)
{
#if PERF_SHOW_PHYS_INIT_COSTS || defined(SHOW_SLOW_CONVEX)
	DOUBLE Start = appSeconds();
#endif

	// Convert scale to physical units.
	FVector pScale3D = uScale3D * U2PScale;

	UINT NumElems;
	if (pScale3D.IsUniform())
	{
		NumElems = GetElementCount();
	}
	else
	{
		NumElems = ConvexElems.Num();
	}

	if (NumElems == 0)
	{
		if (!pScale3D.IsUniform() && GetElementCount() > 0)
		{
			debugf(TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) Cannot 3D-Scale rigid-body primitives (sphere, box, sphyl)."), debugName);
		}
		else
		{
			debugf(TEXT("FKAggregateGeom::InstanceNovodexGeom: (%s) No geometries in FKAggregateGeom."), debugName);
		}
	}

	// Include spheres, boxes and sphyls only when the scale is uniform.
	if (pScale3D.IsUniform())
	{
		// Sphere primitives
		for (int i = 0; i < SphereElems.Num(); i++)
		{
			FKSphereElem* SphereElem = &SphereElems(i);
			if(!SphereElem->bNoRBCollision)
			{
				NxMat34 RelativeTM = U2NMatrixCopy(SphereElem->TM);
				ScaleNovodexTMPosition(RelativeTM, pScale3D);

				NxSphereForceFieldShapeDesc* SphereDesc = new NxSphereForceFieldShapeDesc;
				SphereDesc->radius = (SphereElem->Radius * pScale3D.X) + PhysSkinWidth;
				SphereDesc->pose = RelativeTM; // TODO there is no local pose..
				/*
				if(bCreateCCDSkel)
				{
				MakeCCDSkelForSphere(SphereDesc);
				}
				*/

				ffsGroup.createShape(*SphereDesc);
			}
		}

		// Box primitives
		for (int i = 0; i < BoxElems.Num(); i++)
		{
			FKBoxElem* BoxElem = &BoxElems(i);
			if(!BoxElem->bNoRBCollision)
			{
				NxMat34 RelativeTM = U2NMatrixCopy(BoxElem->TM);
				ScaleNovodexTMPosition(RelativeTM, pScale3D);

				NxBoxForceFieldShapeDesc* BoxDesc = new NxBoxForceFieldShapeDesc;
				BoxDesc->dimensions = (0.5f * NxVec3(BoxElem->X * pScale3D.X, BoxElem->Y * pScale3D.X, BoxElem->Z * pScale3D.X)) + NxVec3(PhysSkinWidth, PhysSkinWidth, PhysSkinWidth);
				BoxDesc->pose = RelativeTM;
				/*
				if(bCreateCCDSkel)
				{
				MakeCCDSkelForBox(BoxDesc);
				}
				*/

				ffsGroup.createShape(*BoxDesc);
			}
		}

		// Sphyl (aka Capsule) primitives
		for (int i =0; i < SphylElems.Num(); i++)
		{
			FKSphylElem* SphylElem = &SphylElems(i);
			if(!SphylElem->bNoRBCollision)
			{
				// The stored sphyl transform assumes the sphyl axis is down Z. In Novodex, it points down Y, so we twiddle the matrix a bit here (swap Y and Z and negate X).
				FMatrix SphylRelTM = FMatrix::Identity;
				SphylRelTM.SetAxis( 0, -1.f * SphylElem->TM.GetAxis(0) );
				SphylRelTM.SetAxis( 1, SphylElem->TM.GetAxis(2) );
				SphylRelTM.SetAxis( 2, SphylElem->TM.GetAxis(1) );
				SphylRelTM.SetOrigin( SphylElem->TM.GetOrigin() );

				NxMat34 RelativeTM = U2NMatrixCopy(SphylRelTM);
				ScaleNovodexTMPosition(RelativeTM, pScale3D);

				NxCapsuleForceFieldShapeDesc* CapsuleDesc = new NxCapsuleForceFieldShapeDesc;
				CapsuleDesc->radius = (SphylElem->Radius * pScale3D.X) + PhysSkinWidth;
				CapsuleDesc->height = SphylElem->Length * pScale3D.X;
				CapsuleDesc->pose = RelativeTM;
				/*
				if(bCreateCCDSkel)
				{
				MakeCCDSkelForSphyl(CapsuleDesc);
				}
				*/

				ffsGroup.createShape(*CapsuleDesc);
			}
		}
	}

	// Convex mesh primitives

	FKCachedConvexData TempCacheData;
	FKCachedConvexData* UseCacheData = NULL;

	// If we were passed in the cooked convex data, use that.
	if(InCacheData && bUsePrecookedPhysData)
	{
		UseCacheData = InCacheData;
	}
	// If not, cook it now into TempCacheData and use that
	else
	{
#if !FINAL_RELEASE
		if((!GIsEditor || GIsPlayInEditorWorld) && ConvexElems.Num() > 0)
		{
			debugf( TEXT("WARNING: Cooking Convex For %s (Scale: %f %f %f) Please go and set PreCachedPhysScale on the mesh to have this scale for RunTime spawned objects."), debugName, uScale3D.X, uScale3D.Y, uScale3D.Z );
		}
#endif

		MakeCachedConvexDataForAggGeom(&TempCacheData, ConvexElems, uScale3D, debugName);
		UseCacheData = &TempCacheData;
	}

	// Iterate over each element in the cached data.
	for (INT i=0; i<UseCacheData->CachedConvexElements.Num(); i++)
	{
#if XBOX
		if( GetCookedPhysDataEndianess(UseCacheData->CachedConvexElements(i).ConvexElementData) == CPDE_LittleEndian )
		{
			debugf( TEXT("InstanceNovodexGeom: Found Little Endian Data: %s"), debugName );
		}

		check( GetCookedPhysDataEndianess(UseCacheData->CachedConvexElements(i).ConvexElementData) != CPDE_LittleEndian );
#endif

		// Create convex mesh from the cached data
		FNxMemoryBuffer Buffer( &(UseCacheData->CachedConvexElements(i).ConvexElementData) );
#if USE_QUICKLOAD_CONVEX
		NxConvexMesh* ConvexMesh = GNovodeXQuickLoad->createConvexMesh(Buffer);
#else 
		NxConvexMesh* ConvexMesh = GNovodexSDK->createConvexMesh(Buffer);
#endif
#if PERF_SHOW_PHYS_INIT_COSTS
		TotalConvexGeomCount++;
#endif

		SetNxConvexMeshRefCount(ConvexMesh, DelayNxMeshDestruction);
		GNumPhysXConvexMeshes++;

		// If we have a convex, and this object is uniformly scaled, we see if we could use a box instead.
		if( ConvexMesh && pScale3D.IsUniform() )
		{
			// TODO hermes// to study, rongbo.
			/*
			if( RepresentConvexAsBox( ActorDesc, ConvexMesh, bCreateCCDSkel ) )
			{
			#if USE_QUICKLOAD_CONVEX
			NxReleaseQLConvexMesh( *ConvexMesh );
			#else
			GNovodexPendingKillConvex.AddItem(ConvexMesh);
			#endif
			ConvexMesh = NULL;				

			GNumPhysXConvexMeshes--;
			}
			*/
		}

		// Convex mesh creation may fail, or may have decided to use box - in which case we do nothing more.
		if(ConvexMesh)
		{
			NxConvexForceFieldShapeDesc* ConvexShapeDesc = new NxConvexForceFieldShapeDesc;
			ConvexShapeDesc->meshData = ConvexMesh;
			//ConvexShapeDesc->meshFlags = 0;
			/*
			if(bCreateCCDSkel)
			{
			MakeCCDSkelForConvex(ConvexShapeDesc);
			}
			*/

			ffsGroup.createShape(*ConvexShapeDesc);
		}
	}

#if PERF_SHOW_PHYS_INIT_COSTS || defined(SHOW_SLOW_CONVEX)
	// Update total time.
	DOUBLE InstanceGeomTime = (appSeconds() - Start);
#endif

#ifdef SHOW_SLOW_CONVEX
	if((InstanceGeomTime*1000.f) > 1.f)
	{
		debugf( TEXT("INIT SLOW GEOM: %s took %f ms (%d hulls)"), debugName, InstanceGeomTime * 1000.f, UseCacheData->CachedConvexElements.Num());
	}
#endif

#if PERF_SHOW_PHYS_INIT_COSTS
	TotalInstanceGeomTime += InstanceGeomTime;
#endif

}
#endif // WITH_NOVODEX
