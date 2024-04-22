/*=============================================================================
	NvApexDestructible.cpp : Handles APEX destructible objects.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// This code contains NVIDIA Confidential Information and is disclosed
// under the Mutual Non-Disclosure Agreement.
//
// Notice
// ALL NVIDIA DESIGN SPECIFICATIONS, CODE ARE PROVIDED "AS IS.". NVIDIA MAKES
// NO WARRANTIES, EXPRESSED, IMPLIED, STATUTORY, OR OTHERWISE WITH RESPECT TO
// THE MATERIALS, AND EXPRESSLY DISCLAIMS ALL IMPLIED WARRANTIES OF NONINFRINGEMENT,
// MERCHANTABILITY, AND FITNESS FOR A PARTICULAR PURPOSE.
//
// Information and code furnished is believed to be accurate and reliable.
// However, NVIDIA Corporation assumes no responsibility for the consequences of use of such
// information or for any infringement of patents or other rights of third parties that may
// result from its use. No license is granted by implication or otherwise under any patent
// or patent rights of NVIDIA Corporation. Details are subject to change without notice.
// This code supersedes and replaces all information previously supplied.
// NVIDIA Corporation products are not authorized for use as critical
// components in life support devices or systems without express written approval of
// NVIDIA Corporation.
//
// Copyright 2009-2010 NVIDIA Corporation. All rights reserved.
// Copyright 2002-2008 AGEIA Technologies, Inc. All rights reserved.
// Copyright 2001-2006 NovodeX. All rights reserved.

#include "EnginePrivate.h"

#if WITH_NOVODEX

#include "NvApexManager.h"

#endif

#include "EngineMeshClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineParticleClasses.h"
#include "EngineMaterialClasses.h"
#include "NvApexScene.h"
#include "UnNovodexSupport.h"
#include "NvApexManager.h"
#include "UnNovodexSupport.h"

#if WITH_APEX

#include <NxApexSDK.h>
#include <NxUserRenderResourceManager.h>
#include "NvApexCommands.h"
#include <NxModuleDestructible.h>
#include <NxDestructibleAsset.h>
#include <NxDestructiblePreview.h>
#include <NxDestructibleActor.h>
#include <NxDestructibleActorJoint.h>
#include <NxResourceProvider.h>
#include "NxParamUtils.h"

using namespace physx::apex;

#endif

IMPLEMENT_CLASS(UFractureMaterial);
IMPLEMENT_CLASS(UApexDestructibleDamageParameters);

/*
 *	UApexAsset - "abstract" class
 *
 */
IMPLEMENT_CLASS(UApexAsset);

TArray<FString> UApexAsset::GetGenericBrowserInfo()
{
	return TArray<FString>();
}

void UApexAsset::OnApexAssetLost(void)
{
	// $ bdudash - removed this as it is a silly check... this method is called in non editor situations (changing a map).
	//check(GIsEditor && !GIsGame);
	TArrayNoInit<class UApexComponentBase*> ApexComponentBackup = ApexComponents;
	UINT NumApexComponents = ApexComponentBackup.Num();
	for(UINT i=0; i<NumApexComponents; i++)
	{
		ApexComponentBackup(i)->OnApexAssetLost();
	}
}

void UApexAsset::OnApexAssetReset(void)
{
	check(GIsEditor && !GIsGame);
	TArrayNoInit<class UApexComponentBase*> ApexComponentBackup = ApexComponents;
	UINT NumApexComponents = ApexComponentBackup.Num();
	for(UINT i=0; i<NumApexComponents; i++)
	{
		ApexComponentBackup(i)->OnApexAssetReset();
	}
}


/*
 *	UApexComponentBase
 *
 */
IMPLEMENT_CLASS(UApexComponentBase);

INT UApexComponentBase::GetNumElements(void) const
{
	return Asset ? (INT)Asset->GetNumMaterials() : 0;
}

UMaterialInterface *UApexComponentBase::GetMaterial(INT MaterialIndex) const
{
	UMaterialInterface* Mat = NULL;

	// MeshComponent's Material array can be used as a per-actor override of the asset's materials
	if ( Materials.Num() > MaterialIndex )
	{
		Mat = Materials(MaterialIndex);
	}
	if ( Mat == NULL)
	{
		Mat = Asset ? Asset->GetMaterial(MaterialIndex): NULL;
	}

	return Mat;
}

void UApexComponentBase::UpdateTransform()
{
	Super::UpdateTransform();
}

void UApexComponentBase::UpdateBounds(void)
{
#if WITH_APEX
	NxApexRenderable *ApexRenderable = GetApexRenderable();
	if(ApexRenderable)
	{
		physx::PxBounds3 PhysXBounds = ApexRenderable->getBounds();

		// the returned bounding volume might be empty. acceptable default
		//  behavior? follows same path as (ApexRenderable == null)
		if (PhysXBounds.isEmpty() == false)
		{
			physx::PxVec3 Center, Extents;
			Center = PhysXBounds.getCenter();
			Extents = PhysXBounds.getExtents();
			Bounds = FBoxSphereBounds(FBox::BuildAABB(N2UPositionApex(Center), N2UPositionApex(Extents)));
		}
		else
		{
			Super::UpdateBounds();
		}
	}
	else
	{
		Super::UpdateBounds();
	}
#endif
}

void UApexComponentBase::InitComponentRBPhys( UBOOL bFixed )
{
}

void UApexComponentBase::TermComponentRBPhys( FRBPhysScene *InScene )
{
}

/**
 * Attaches the component to the scene, and initializes the component's resources if they have not been yet.
 */
void UApexComponentBase::Attach()
{
	Super::Attach();
	if(!GIsGame)
	{
		UpdateApexEditorState();
	}
}

/**
* Detach the component from the scene and remove its render proxy
* @param bWillReattach TRUE if the detachment will be followed by an attachment
*/
void UApexComponentBase::Detach(UBOOL bWillReattach)
{
	Super::Detach(bWillReattach);
	if(!GIsGame && !bWillReattach)
	{
		UpdateApexEditorState();
	}
}

/**
 * Called after all objects referenced by this object have been serialized. Order of PostLoad routed to
 * multiple objects loaded in one set is not deterministic though ConditionalPostLoad can be forced to
 * ensure an object has been "PostLoad"ed.
 */
void UApexComponentBase::PostLoad()
{
	Super::PostLoad();
	if(!GIsGame)
	{
		UpdateApexEditorState();
	}
}

void UApexComponentBase::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	if(!GIsGame)
	{
		UpdateApexEditorState(PropertyChangedEvent.Property);
	}
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UApexComponentBase::PostEditMove(UBOOL bFinished)
{
	if(!GIsGame)
	{
		UpdateApexEditorState();
	}
}

/**
 * Check for asynchronous resource cleanup completion
 * @return	TRUE if the rendering resources have been released
 */
UBOOL UApexComponentBase::IsReadyForFinishDestroy()
{
	return Super::IsReadyForFinishDestroy() && ReleaseResourcesFence.GetNumPendingFences() == 0;
}


/*
 *	UApexStaticComponent
 *
 */
IMPLEMENT_CLASS(UApexStaticComponent);

void UApexStaticComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
}

/*
 *	UApexDynamicComponent
 *
 */
IMPLEMENT_CLASS(UApexDynamicComponent);


/*
 *	UApexStaticDestructibleComponent
 *
 */

IMPLEMENT_CLASS(UApexStaticDestructibleComponent);

void UApexStaticDestructibleComponent::UpdatePhysicsToRBChannels()
{
#if WITH_APEX
	NxGroupsMask NewMask = CreateGroupsMask(RBChannel, &RBCollideWithChannels);
 
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call UpdatePhysicsToRBChannels() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif // !FINAL_RELEASE
	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	if (DestructibleAsset)
	{
		::NxParameterized::Interface *params = (::NxParameterized::Interface *)DestructibleAsset->GetNxParameterized();
		check(params);
		if ( !params )
		{
			return;
		}
 
		// Set up the shape desc template
		NxGroupsMask mask = CreateGroupsMask( RBChannel, &RBCollideWithChannels );
		NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits0", mask.bits0 );
		NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits1", mask.bits1 );
		NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits2", mask.bits2 );
		NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits3", mask.bits3 );
	}
#endif
}

void UApexStaticDestructibleComponent::UpdateApexEditorState(UProperty* PropertyThatChanged)
{
#if WITH_APEX
	check(GIsEditor && !GIsGame);
	if ( GApexCommands )
	{
		GApexCommands->Pump();
	}

	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	UBOOL                   bAttached         = IsAttached();

	if(DestructibleAsset)
	{
		if(bAttached && !ApexDestructiblePreview)
		{
			ApexDestructiblePreview = DestructibleAsset->CreateDestructiblePreview(*this);
			check(ApexDestructiblePreview);
			for(INT i = 0; i < (INT)Asset->GetNumMaterials(); i++)
			{
				UMaterialInterface* DestructibleMaterial = GetMaterial(i);
				InitMaterialForApex(DestructibleMaterial);
				if(ApexDestructiblePreview)
				{
					NxRenderMeshActor* RenderMeshActor = const_cast<NxRenderMeshActor*>(ApexDestructiblePreview->getRenderMeshActor());
					RenderMeshActor->setOverrideMaterial(i, TCHAR_TO_ANSI(*DestructibleMaterial->GetPathName()));
				}
			}
			bNeedsReattach = TRUE;
			if(bIsThumbnailComponent)
			{
				DestructibleAsset->MDestructibleThumbnailComponent = this;
			}
		}
	}
	if(ApexDestructiblePreview && Owner)
	{
		const physx::PxVec3 OwnerScale = U2NPositionApex(Scale * Scale3D*Owner->DrawScale * Owner->DrawScale3D);
		physx::PxMat34Legacy      OwnerPose  = U2NTransformApex(FRotationTranslationMatrix(Owner->Rotation, Owner->Location));
		OwnerPose.M.setColumn(0, OwnerPose.M.getColumn(0)*OwnerScale.x);
		OwnerPose.M.setColumn(1, OwnerPose.M.getColumn(1)*OwnerScale.y);
		OwnerPose.M.setColumn(2, OwnerPose.M.getColumn(2)*OwnerScale.z);
		ApexDestructiblePreview->setPose(OwnerPose);
		ApexDestructiblePreview->setExplodeView(0, 0);
	}
	// In the case of thumbnail rendering the Owner is always NULL
	// this path should only work for thumbnail rendering!
	else if(ApexDestructiblePreview && !Owner)
	{
		const physx::PxVec3 OwnerScale = U2NPositionApex(Scale * Scale3D*1.0f * FVector(1.0f,1.0f,1.0f));
		physx::PxMat44      OwnerPose = U2NTransformApex(FRotationTranslationMatrix(FRotator::ZeroRotator,FVector::ZeroVector));
		OwnerPose.column0.x *= OwnerScale.x;
		OwnerPose.column1.y *= OwnerScale.y;
		OwnerPose.column2.z *= OwnerScale.z;
		ApexDestructiblePreview->setPose(OwnerPose);
		ApexDestructiblePreview->setExplodeView(0, 0);
	}

	if(ApexDestructiblePreview && bAttached)
	{
		UpdateBounds();
		UpdateTransform();
	}

	if( PropertyThatChanged && PropertyThatChanged->GetFName() == FName(TEXT("Asset")))
	{
		bAssetChanged = TRUE;
	}

	if( Owner && bAssetChanged )
	{
		AApexDestructibleActor *DestructibleActor = Cast<AApexDestructibleActor>(Owner);
		if( DestructibleActor )
		{
			DestructibleActor->LoadEditorParametersFromAsset();
			// Force FractureMaterials to be re-initialized from new asset
			DestructibleActor->FractureMaterials.Empty();
			DestructibleActor->FixupActor();
		}
		if( ApexDestructibleActor )
		{
			ApexDestructibleActor->forcePhysicalLod( DestructibleActor->LOD );
		}
		bAssetChanged = FALSE;
	}
#endif
}

physx::apex::NxApexRenderable *UApexStaticDestructibleComponent::GetApexRenderable(void) const
{
#if WITH_APEX
	if ( GApexCommands )
	{
		GApexCommands->Pump();
	}
	check(!ApexDestructiblePreview || !ApexDestructibleActor);
	return ApexDestructiblePreview ? static_cast<NxApexRenderable*>(ApexDestructiblePreview) : static_cast<NxApexRenderable*>(ApexDestructibleActor);
#else
	return 0;
#endif
}

void UApexStaticDestructibleComponent::Tick(FLOAT DeltaTime)
{
	Super::Tick( DeltaTime );
	UpdateBounds();
	UpdateTransform();
}

UBOOL UApexStaticDestructibleComponent::IsValidComponent() const
{
	return Asset != NULL && Super::IsValidComponent();
}

UBOOL UApexStaticDestructibleComponent::LineCheck(FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags)
{
#if WITH_APEX
	if( !ApexDestructibleActor )
	{
		if(ApexDestructiblePreview)
		{
			physx::PxBounds3 PhysXBounds = ApexDestructiblePreview->getBounds();
			if (!PhysXBounds.isEmpty())
			{
				physx::PxVec3 Center, Extents;
				Center = PhysXBounds.getCenter();
				Extents = PhysXBounds.getExtents();
				FBox Bounds = (FBox::BuildAABB(N2UPositionApex(Center), N2UPositionApex(Extents)));
				if(FLineExtentBoxIntersection(Bounds, Start, End, Extent, Result.Location, Result.Normal, Result.Time))
				{
					Result.Actor = Owner;
					Result.Component = this;
					return FALSE;
				}
			}
		}
		return Super::LineCheck( Result, End, Start, Extent, TraceFlags );
	}
#if 1	// Questionable bugfix - see comment inside
	if( TraceFlags & TRACE_ShadowCast )
	{
		// APEX can't handle these LineChecks that get called during async ticking, so don't do them
		// We may need to revisit this
		return Super::LineCheck( Result, End, Start, Extent, TraceFlags );
	}
#endif

	FVector Disp = End-Start;
	FLOAT D2 = Disp.SizeSquared();
	FLOAT RecipD = (D2 == 0.0f) ? 0.0f : appInvSqrt( D2 );
	NxI32 ChunkIndex;
	FLOAT Time = 1.0f;
	physx::PxVec3 Normal;
	const NxDestructibleActorRaycastFlags::Enum rayCastFlags = (TraceFlags&TRACE_MoveIgnoresDestruction) != 0 ?
															   NxDestructibleActorRaycastFlags::StaticChunks : NxDestructibleActorRaycastFlags::AllChunks;

	if( Extent.IsZero() )
	{
		NxRay WorldRay( U2NPosition( Start ), U2NPosition( Disp ) );
		ChunkIndex = ApexDestructibleActor->rayCast( Time, Normal, WorldRay,
			(NxDestructibleActorRaycastFlags::Enum)(rayCastFlags | NxDestructibleActorRaycastFlags::SegmentIntersect) );
		
		// if a ray starts inside the apex object we got a zero normal. change it to negate the incoming ray direction
		// according to the UE3 convention
		if(Normal.isZero())
		{
			NxVec3 nNormal = U2NPosition(-Disp);
			Normal = physx::PxVec3(nNormal.x,nNormal.y,nNormal.z);
			Normal.normalize();
		}

		if( ChunkIndex == NxModuleDestructibleConst::INVALID_CHUNK_INDEX )
		{
			return TRUE;
		}
		if( Time <= 0 )
		{
			Time = 0.0f;
		}
	}
	else
	{
		NxMat33 BoxRot;
		BoxRot.id();
		NxBox WorldBox( U2NPosition( Start ), U2NPosition( Extent ), BoxRot );

		// Force accurate ray cast off returns the exact normal of a certain trunk(rather than its children chunk)
		
		ChunkIndex = ApexDestructibleActor->obbSweep( Time, Normal, WorldBox, U2NPositionApex( Disp ), 
			(NxDestructibleActorRaycastFlags::Enum)(rayCastFlags | NxDestructibleActorRaycastFlags::ForceAccurateRaycastsOff) );
		
		
		if( ChunkIndex == NxModuleDestructibleConst::INVALID_CHUNK_INDEX )
		{
			return TRUE;
		}
		if( Time < 0.0f )	// We start with penetrating
		{
			FLOAT ExitTime = 1.0f;
			physx::PxVec3 ExitNormal;
		
			//Reverse the Direction to do the obbSweep again to see if we can go through the face on the back
			ApexDestructibleActor->obbSweep( ExitTime, ExitNormal, WorldBox, U2NPositionApex( -Disp ), 
				(NxDestructibleActorRaycastFlags::Enum)(rayCastFlags | NxDestructibleActorRaycastFlags::ForceAccurateRaycastsOff) );

			// If the it is easier to go exit the object from the other side and the exit normal is along the
			// moving direction we simply let it go
			if( abs(ExitTime)<abs(Time) && (ExitNormal.dot(U2NPositionApex(Disp))) > 0.0f )
			{
				Result.Time = 1.0f;
				return TRUE;
			}
			Time = 0.0f;
			Result.bStartPenetrating = TRUE;
		}
	}
	Result.Item = ChunkIndex;
	Result.Time = Time;
	Result.Time = Clamp(Result.Time - Clamp(0.1f,0.1f*RecipD,4.0f*RecipD),0.0f,1.0f);
	Result.Location = Start + Disp*Result.Time;
	Result.Actor = Owner;
	Result.Component = this;
	Result.Normal = N2UVectorCopyApex( Normal );

	return FALSE;
#else
	return Super::LineCheck( Result, End, Start, Extent, TraceFlags );
#endif
}

UBOOL UApexStaticDestructibleComponent::PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags)
{
	return LineCheck( Result, Location, Location, Extent, TraceFlags );
}



void UApexStaticDestructibleComponent::InitComponentRBPhys(UBOOL bFixed)
{
#if WITH_APEX
	if ( !GApexManager ) return;
	if ( GApexCommands )
	{
		GApexCommands->Pump();
	}
	if( !GIsGame ) return;

 	NxModuleDestructible *moduleDestructible = GApexManager->GetModuleDestructible();
	if( moduleDestructible == NULL )
	{
		debugf( NAME_DevPhysics, TEXT("APEX must be enabled to init UApexDestructibleComponentBase physics.") );
		return;
	}

	AApexDestructibleActor * DestructibleActor = Cast<AApexDestructibleActor>(GetOwner());
	check( DestructibleActor );

	if( ApexDestructibleActor )
	{
		debugf( NAME_DevPhysics, TEXT("NxDestructibleActor already created.") );
		return;
	}

	BeginDeferredReattach();

	// Get the destructible asset
	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	check(DestructibleAsset);

	::NxParameterized::Interface *params = (::NxParameterized::Interface *)DestructibleAsset->GetNxParameterized();
	assert(params);
	if ( !params ) return;

	physx::PxMat34Legacy xform = U2NTransformApex( FRotationTranslationMatrix( DestructibleActor->Rotation, DestructibleActor->Location ) );
	physx::PxVec3  scale = U2NPositionApex( Scale*Scale3D*DestructibleActor->DrawScale*DestructibleActor->DrawScale3D );

	NxParameterized::setParamMat44(*params,"globalPose",xform);
	NxParameterized::setParamVec3(*params,"scale",scale);
	NxParameterized::setParamBool(*params,"dynamic",!bFixed);

	// Set template actor/body/shape properties
	// Find the PhysicalMaterial we need to apply to the physics bodies.
	UPhysicalMaterial * PhysMat = PhysMaterialOverride ? PhysMaterialOverride : (DestructibleAsset->DefaultPhysMaterial ? DestructibleAsset->DefaultPhysMaterial : GEngine->DefaultPhysMaterial);

	// Set up the shape desc template
	NxGroupsMask mask = CreateGroupsMask( RBChannel, &RBCollideWithChannels );
	NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits0", mask.bits0 );
	NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits1", mask.bits1 );
	NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits2", mask.bits2 );
	NxParameterized::setParamU32(*params,"shapeDescTemplate.groupsMask.bits3", mask.bits3 );
	NxParameterized::setParamBool(*params,"shapeDescTemplate.flags.NX_SF_FLUID_DRAIN", bFluidDrain );
	NxParameterized::setParamBool(*params,"shapeDescTemplate.flags.NX_SF_FLUID_TWOWAY", bFluidTwoWay );
	NxParameterized::setParamU16(*params,"shapeDescTemplate.materialIndex", GWorld->RBPhysScene->FindPhysMaterialIndex( PhysMat ) );

	// set up the body desc template
	NxParameterized::setParamF32(*params,"bodyDescTemplate.angularDamping", PhysMat->AngularDamping );
	NxParameterized::setParamF32(*params,"bodyDescTemplate.linearDamping", PhysMat->LinearDamping );
	NxParameterized::setParamF32(*params,"bodyDescTemplate.sleepEnergyThreshold", SleepEnergyThreshold*U2PScale*U2PScale );
	NxParameterized::setParamF32(*params,"bodyDescTemplate.sleepDamping", SleepDamping );
	NxParameterized::setParamBool(*params,"bodyDescTemplate.flags.NX_BF_DISABLE_GRAVITY",FALSE);
	NxParameterized::setParamBool(*params,"bodyDescTemplate.flags.NX_BF_VISUALIZATION",TRUE);
	NxParameterized::setParamBool(*params,"bodyDescTemplate.flags.NX_BF_FILTER_SLEEP_VEL",FALSE);
	NxParameterized::setParamBool(*params,"bodyDescTemplate.flags.NX_BF_ENERGY_SLEEP_TEST",TRUE);

	// set up the actor Desc template
	NxParameterized::setParamU16(*params,"actorDescTemplate.dominanceGroup", Clamp<BYTE>(RBDominanceGroup, 0, 31));
	NxParameterized::setParamF32(*params,"actorDescTemplate.density", 0.001f*PhysMat->Density*P2UScale*P2UScale*P2UScale);	// Kind of a CGS-to-MKS, sort of...
	NxParameterized::setParamBool(*params,"actorDescTemplate.flags.NX_AF_DISABLE_COLLISION",!BlockRigidBody);
	NxParameterized::setParamBool(*params,"actorDescTemplate.flags.NX_AF_FORCE_CONE_FRICTION", PhysMat->bForceConeFriction );
	NxParameterized::Handle OverrideSkinnedMaterials(*params);
	params->getParameterHandle("overrideSkinnedMaterialNames", OverrideSkinnedMaterials);
	OverrideSkinnedMaterials.resizeArray(DestructibleAsset->GetNumMaterials());
	for(UINT i = 0; i < DestructibleAsset->GetNumMaterials(); i++)
	{
		UMaterialInterface* Material = DestructibleAsset->GetMaterial(i);
		InitMaterialForApex(Material);
		NxParameterized::Handle MaterialHandle(*params);
		OverrideSkinnedMaterials.getChildHandle(i, MaterialHandle);
		MaterialHandle.setParamString(TCHAR_TO_ANSI(*Material->GetPathName()));
	}
	if ( bNotifyRigidBodyCollision )
	{
		if ( DestructibleAsset->DestructibleParameters.DamageParameters.ImpactDamage > 0.0f )
		{
			NxParameterized::setParamU16(*params,"actorDescTemplate.group", UNX_GROUP_THRESHOLD_NOTIFY);
		}
	}

	NxCompartment * Compartment = GWorld->RBPhysScene->GetNovodexRigidBodyCompartment();
	UBOOL bUsingCompartment = bUseCompartment && Compartment;
	if( bUsingCompartment )
	{
		NxParameterized::setParamU64(*params,"actorDescTemplate.compartment", (physx::PxU64) Compartment );
	}
	NxApexAsset *a_asset = DestructibleAsset->MApexAsset ? DestructibleAsset->MApexAsset->GetNxApexAsset() : NULL;
	NxApexScene *ApexScene = GWorld->RBPhysScene->GetApexScene();
	ApexDestructibleActor = NULL;
	check(ApexScene && a_asset );
	if( ApexScene && a_asset )
	{
		NxApexActor *apexActor = a_asset->createApexActor(*params,*ApexScene);
		ApexDestructibleActor = static_cast< NxDestructibleActor *>(apexActor);
		check(ApexDestructibleActor);
		if(ApexDestructibleActor)
		{
			ApexDestructibleActor->cacheModuleData();
			ApexDestructibleActor->forcePhysicalLod( DestructibleActor->LOD );
			for(UINT i = 0; i < DestructibleAsset->GetNumMaterials(); i++)
			{
				UMaterialInterface* Material = GetMaterial(i);
				InitMaterialForApex(Material);
				ApexDestructibleActor->setSkinnedOverrideMaterial(i, TCHAR_TO_ANSI(*Material->GetPathName()));
			}
		}
	}
	if(ApexDestructibleActor)
	{	
		DestructibleAsset->MApexAsset->IncRefCount(0);
		DestructibleAsset->ApexComponents.AddItem(this);
		ApexDestructibleActor->userData = static_cast<void*>(DestructibleActor);
	}

//	Super::InitComponentRBPhys( bFixed );

	UpdateBounds();
#endif
}

void UApexStaticDestructibleComponent::CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount)
{
#if WITH_APEX
	if ( !GApexManager ) return;

	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	if(DestructibleAsset && DestructibleAsset->MApexAsset)
	{
		NxApexAsset* aasset = DestructibleAsset->MApexAsset->GetNxApexAsset();
		if( aasset != NULL )
		{
			NxModuleDestructible* module = GApexManager->GetModuleDestructible();
			if( module != NULL )
			{
				NxApexSDK *apexSDK = GApexManager->GetApexSDK();
				NxApexModuleCachedData* cache = apexSDK->getCachedData().getCacheForModule( module->getModuleID() );
				if( cache != NULL )
				{
					cache->getCachedDataForAssetAtScale( *aasset, U2NPositionApex( TotalScale3D ) );
				}
			}
		}
	}
#endif
}

void UApexStaticDestructibleComponent::TermComponentRBPhys(FRBPhysScene *InScene)
{
	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	if(DestructibleAsset)
	{
		if(ApexDestructibleActor)
		{
			DestructibleAsset->ReleaseDestructibleActor(*ApexDestructibleActor, *this);
			ApexDestructibleActor = NULL;
		}
	}
	Super::TermComponentRBPhys( InScene );
}

void UApexStaticDestructibleComponent::OnApexAssetLost(void)
{
	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	if(DestructibleAsset)
	{
		if ( bAttached )
		  Detach( TRUE );
		DetachFence.Wait();

		if(ApexDestructibleActor)
		{
			DestructibleAsset->ReleaseDestructibleActor(*ApexDestructibleActor, *this);
			ApexDestructibleActor = NULL;
		}
		if(ApexDestructiblePreview)
		{
			DestructibleAsset->ReleaseDestructiblePreview(*ApexDestructiblePreview, *this);
			ApexDestructiblePreview = NULL;
		}
	}
}

void UApexStaticDestructibleComponent::OnApexAssetReset(void)
{
	UpdateApexEditorState();
}

// We release the actor ore preview here instead of in the Detach phase
// This is for the sake of thumbnail rendering
void UApexStaticDestructibleComponent::BeginDestroy(void)
{
	Super::BeginDestroy();
	UApexDestructibleAsset *DestructibleAsset = Asset ? Cast<UApexDestructibleAsset>(Asset) : NULL;
	if(DestructibleAsset)
	{
		if ( bAttached )
		  Detach( TRUE );
		DetachFence.Wait();

		if(ApexDestructibleActor)
		{
			DestructibleAsset->ReleaseDestructibleActor(*ApexDestructibleActor, *this);
			ApexDestructibleActor = NULL;
		}
		if(ApexDestructiblePreview)
		{
			DestructibleAsset->ReleaseDestructiblePreview(*ApexDestructiblePreview, *this);
			ApexDestructiblePreview = NULL;
		}
	}
}
void UApexStaticDestructibleComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
#if WITH_APEX
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if(PropertyThatChanged)
	{
		for(INT i = 0; i < (INT)Asset->GetNumMaterials(); i++)
		{
			UMaterialInterface* DestructibleMaterial = GetMaterial(i);
			InitMaterialForApex(DestructibleMaterial);
			if(ApexDestructibleActor)
			{
				ApexDestructibleActor->setSkinnedOverrideMaterial(i, TCHAR_TO_ANSI(*DestructibleMaterial->GetPathName()));
			}
			if(ApexDestructiblePreview)
			{
				NxRenderMeshActor* RenderMeshActor = const_cast<NxRenderMeshActor*>(ApexDestructiblePreview->getRenderMeshActor());
				RenderMeshActor->setOverrideMaterial(i, TCHAR_TO_ANSI(*DestructibleMaterial->GetPathName()));
			}
		}
	}
#endif
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void UApexStaticDestructibleComponent::SetMaterial(INT ElementIndex,UMaterialInterface* Material)
{
	Super::SetMaterial(ElementIndex, Material);
#if WITH_APEX
	UMaterialInterface* DestructibleMaterial = GetMaterial(ElementIndex);
	InitMaterialForApex(DestructibleMaterial);
	if(ApexDestructibleActor)
	{
		ApexDestructibleActor->setSkinnedOverrideMaterial(ElementIndex, TCHAR_TO_ANSI(*DestructibleMaterial->GetPathName()));
	}
	if(ApexDestructiblePreview)
	{
		NxRenderMeshActor* RenderMeshActor = const_cast<NxRenderMeshActor*>(ApexDestructiblePreview->getRenderMeshActor());
		RenderMeshActor->setOverrideMaterial(ElementIndex, TCHAR_TO_ANSI(*DestructibleMaterial->GetPathName()));
	}
#endif
}

/*
 *	AApexDestructibleActor
 *
 */

void UApexStaticDestructibleComponent::UpdateRBKinematicData()
{
#if WITH_APEX
	AApexDestructibleActor * DestructibleActor = Cast<AApexDestructibleActor>(GetOwner());
	
	if(!ApexDestructibleActor || !DestructibleActor) 
	{
		return;
	}
#if !FINAL_RELEASE
	// Check to see if this physics call is illegal during this tick group
	if (GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		debugf(NAME_Error,TEXT("Can't call UpdateRBKinematicData() on (%s)->(%s) during async work!"), *Owner->GetName(), *GetName());
	}
#endif

	// Synchronize the position and orientation of the destructible actor to match the actor.
	physx::PxMat34Legacy newPose = U2NTransformApex( FRotationTranslationMatrix( DestructibleActor->Rotation, DestructibleActor->Location ) );

	// Don't call setGlobalPose if we are already in the correct pose.
	physx::PxMat44 currentPose;
	ApexDestructibleActor->getGlobalPose(currentPose);
	if( !MatricesAreEqual(newPose, currentPose, (FLOAT)KINDA_SMALL_NUMBER) )
	{
		ApexDestructibleActor->setGlobalPose( newPose );
	}
#endif // WITH_APEX
}

IMPLEMENT_CLASS(AApexDestructibleActor);

// Check whether Actor still has chunks, if it doesn't destroy actor.
void AApexDestructibleActor::TickAuthoritative( FLOAT DeltaSeconds )
{
#if WITH_APEX
	NxDestructibleActor *ApexDestructibleActor = NULL;
	if(StaticDestructibleComponent)  
	{
		ApexDestructibleActor = StaticDestructibleComponent->ApexDestructibleActor;
	}

	if(ApexDestructibleActor)
	{
		if(ApexDestructibleActor->getNumVisibleChunks() == 0 && GWorld)
		{
			GWorld->DestroyActor( this );
		}
	}
#endif
	Super::TickAuthoritative( DeltaSeconds );
}

void AApexDestructibleActor::SyncActorToRBPhysics()
{
#if WITH_APEX
	if(!CollisionComponent)
	{
		debugf(TEXT("AApexDestructibleActor::SyncActorToRBPhysics (%s) : No CollisionComponent."), *GetName());
		return;
	}

	FBoxSphereBounds	bounds = CollisionComponent->Bounds;
	// CheckStillInWorld
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo( TRUE );
	if( bounds.Origin.Z < ((WorldInfo->bSoftKillZ && Physics == PHYS_Falling) ? (WorldInfo->KillZ - WorldInfo->SoftKill) : WorldInfo->KillZ) )
	{
		eventFellOutOfWorld(WorldInfo->KillZDamageType);
	}
#endif
}

void AApexDestructibleActor::OverrideDamageParams(FLOAT& BaseDamage, FLOAT& DamageRadius, FLOAT& Momentum, const AActor* DamageCauser) const
{
#if WITH_APEX
	if ( GEngine->ApexDamageParams )
	{
		for (INT i = 0; i < GEngine->ApexDamageParams->DamageMap.Num(); ++i)
		{
			const FDamagePair& DamagePair = GEngine->ApexDamageParams->DamageMap(i);
			UClass* DCC = FindObject<UClass>(ANY_PACKAGE, *DamagePair.DamageCauserName.ToString());

			if ( DCC && DamageCauser && DamageCauser->IsA(DCC) )
			{
				const FDamageParameters& Params = DamagePair.Params;

				// found entry for this DamageCauser, override
				if (Params.OverrideMode == DPOM_Absolute)
				{
					BaseDamage = Params.BaseDamage;
					DamageRadius = Params.Radius;
					Momentum = Params.Momentum;
				}
				else if (Params.OverrideMode == DPOM_Multiplier)
				{
					BaseDamage *= Params.BaseDamage;
					DamageRadius *= Params.Radius;
					Momentum *= Params.Momentum;
				}
				else
				{
					assert(0);
				}
				
				break;
			}
		}
	}
#endif
}

void AApexDestructibleActor::TakeDamage( INT Damage, AController * EventInstigator, FVector HitLocation, FVector Momentum, UClass * DamageType, FTraceHitInfo HitInfo, AActor * DamageCauser )
{
#if WITH_APEX
	NxDestructibleActor *ApexDestructibleActor = NULL;
	if(StaticDestructibleComponent)  ApexDestructibleActor = StaticDestructibleComponent->ApexDestructibleActor;
	check(ApexDestructibleActor);
	if(ApexDestructibleActor)
	{
		FVector Dir = Momentum;
		Dir.Normalize();
		FLOAT MomentumMagnitude = Momentum.Size();
#if 1	// BRG - this is ridiculous.  Why can't I get the Item from UApexStaticDestructibleComponent::LineCheck?
		NxRay WorldRay( U2NPosition( HitLocation-Dir*0.01f*StaticDestructibleComponent->Bounds.SphereRadius ), U2NVectorCopy( Dir ) );
		NxReal Time;
		physx::PxVec3 Normal;
		NxI32 ChunkIndex = ApexDestructibleActor->rayCast( Time, Normal, WorldRay, NxDestructibleActorRaycastFlags::AllChunks );
#endif
		FLOAT DummyRadius = 0.0f;
		FLOAT DamageReal = (NxReal)Damage;
		OverrideDamageParams(DamageReal, DummyRadius, MomentumMagnitude, DamageCauser);
		ApexDestructibleActor->applyDamage( DamageReal, MomentumMagnitude*U2PScale, U2NPositionApex( HitLocation ), U2NVectorCopyApex( Dir ), ChunkIndex );
	}
#endif
}

void  AApexDestructibleActor::TakeRadiusDamage( AController * InstigatedBy, FLOAT BaseDamage, FLOAT DamageRadius, UClass * DamageType, FLOAT Momentum, FVector HurtOrigin, UBOOL bFullDamage, AActor * DamageCauser, FLOAT DamageFalloffExponent )
{
#if WITH_APEX
	NxDestructibleActor *ApexDestructibleActor = NULL;
	if(StaticDestructibleComponent)  ApexDestructibleActor = StaticDestructibleComponent->ApexDestructibleActor;
	check(ApexDestructibleActor);
	if(ApexDestructibleActor)
	{
		OverrideDamageParams(BaseDamage, DamageRadius, Momentum, DamageCauser);
		ApexDestructibleActor->applyRadiusDamage(BaseDamage, Momentum*U2PScale, U2NPositionApex( HurtOrigin ), DamageRadius*U2PScale, bFullDamage ? true : false);
	}
#endif
}

void AApexDestructibleActor::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
}

void AApexDestructibleActor::PostEditMove(UBOOL bFinished)
{
	if(StaticDestructibleComponent)
	{
		StaticDestructibleComponent->PostEditMove(bFinished);
	}
	Super::PostEditMove(bFinished);
}

void AApexDestructibleActor::PostLoad()
{
#if WITH_APEX 
	FixupActor();
	NxDestructibleAsset* nDestructibleAsset = NULL;
	if( StaticDestructibleComponent )
	{
		UApexDestructibleAsset* DestructibleAsset = StaticDestructibleComponent->Asset ? Cast<UApexDestructibleAsset>(StaticDestructibleComponent->Asset) : NULL;
		if( DestructibleAsset )
		{
			FIApexAsset *apexAsset = DestructibleAsset->GetApexGenericAsset();
			NxApexAsset *a_asset = apexAsset->GetNxApexAsset();
			nDestructibleAsset = static_cast< NxDestructibleAsset *>(a_asset);
		}
	}
#endif
	Super::PostLoad();
}	

void AApexDestructibleActor::EditorApplyMirror(const FVector& MirrorScale, const FVector& PivotLocation)
{
	// Do nothing. APEX destructible actors can't handle mirror transforms. Since we know it 
	// would fail at runtime anyway, we just don't allow them to be mirrored.
	debugf( NAME_DevPhysics, TEXT("Mirroring operations not supported on destructible actors!") );
}

void AApexDestructibleActor::setPhysics(BYTE NewPhysics, AActor *NewFloor, FVector NewFloorV)
{
#if WITH_APEX 
	if( NewPhysics == PHYS_RigidBody && NewPhysics != Physics )
	{
		if( StaticDestructibleComponent && StaticDestructibleComponent->ApexDestructibleActor )
		{
			StaticDestructibleComponent->ApexDestructibleActor->setDynamic();
			Physics = NewPhysics;
		}
	}
#endif
}

#define GetNonDefaultFractureBehaviorFromAsset( _UField, _value ) \
	if( FractureBehavior._UField == DefaultFractureBehavior._UField ) FractureBehavior._UField = _value

#define GetNonDefaultDestructionSettingsFromAsset( _UField, _value ) \
	if( DestructionSettings._UField == DefaultDestructionSettings._UField ) DestructionSettings._UField = _value

void AApexDestructibleActor::LoadEditorParametersFromAsset()
{
#if WITH_APEX
	NxDestructibleAsset* nDestructibleAsset = NULL;
	if( StaticDestructibleComponent )
	{
		UApexDestructibleAsset* DestructibleAsset = StaticDestructibleComponent->Asset ? Cast<UApexDestructibleAsset>(StaticDestructibleComponent->Asset) : NULL;
		if( DestructibleAsset )
		{
			FIApexAsset *apexAsset = DestructibleAsset->GetApexGenericAsset();
			NxApexAsset *a_asset = apexAsset->GetNxApexAsset();
			nDestructibleAsset =  static_cast< NxDestructibleAsset *>(a_asset);
		}
	}
	check(nDestructibleAsset);
#endif
}

void AApexDestructibleActor::FixupActor()
{
#if WITH_APEX
	if( GIsEditor && !GIsGame && (StaticDestructibleComponent != NULL) )
	{
		UApexDestructibleAsset* DestructibleAsset = StaticDestructibleComponent->Asset ? Cast<UApexDestructibleAsset>(StaticDestructibleComponent->Asset) : NULL;
		if( (DestructibleAsset != NULL) )
		{
			// Make sure the asset is up to date first
			DestructibleAsset->FixupAsset();
			if( DestructibleAsset->FractureMaterials.Num() != FractureMaterials.Num() )
			{
				// Fix up FractureMaterials if the number of fracture levels has changed
				FractureMaterials.Empty();
				for( INT Depth = 0; Depth < DestructibleAsset->FractureMaterials.Num(); ++Depth)
				{
					FractureMaterials.AddItem( DestructibleAsset->FractureMaterials(Depth) );
				}
				MarkPackageDirty();
			}
		}
		if (StaticDestructibleComponent->ApexDestructibleActor!= NULL)
		{
			StaticDestructibleComponent->ApexDestructibleActor->forcePhysicalLod((physx::PxF32)LOD);
		}
	}
#endif
}

void AApexDestructibleActor::CacheFractureEffects()
{
#if WITH_APEX
	FractureSounds.Empty();
	FractureParticleEffects.Empty();

	TArrayNoInit<class UFractureMaterial*>* FracMats = NULL;

	if( bFractureMaterialOverride )
	{
		// load from actor
		FracMats = &FractureMaterials;
	}
	else if( StaticDestructibleComponent )
	{
		// load from asset
		UApexDestructibleAsset* DestructibleAsset = StaticDestructibleComponent->Asset ? Cast<UApexDestructibleAsset>(StaticDestructibleComponent->Asset) : NULL;
		if( (DestructibleAsset != NULL) )
		{
			FracMats = &DestructibleAsset->FractureMaterials;
		}
	}

	if( FracMats != NULL )
	{
		for( INT Depth=0; Depth<FracMats->Num(); ++Depth )
		{
			UFractureMaterial* FracMat = FracMats->GetTypedData()[Depth];
			if( FracMat )
			{
				FractureSounds.AddItem( FracMat->FractureSound );
				FractureParticleEffects.AddItem( FracMat->FractureEffect );
			}
			else
			{
				FractureSounds.AddItem( NULL );
				FractureParticleEffects.AddItem( NULL );
			}
		}
	}
#endif	// WITH_APEX
}
void AApexDestructibleActor::SpawnFractureEffects(FVector& SpawnLocation, FVector& SpawnDirection, INT Depth)
{
#if WITH_APEX
	check( FractureSounds.Num() == FractureParticleEffects.Num() );
	if( FractureSounds.Num() > Depth )
	{
		if( FractureSounds(Depth) != NULL )
		{
			// spawn sound
			PlaySound(FractureSounds(Depth), FALSE, FALSE, FALSE, &SpawnLocation, FALSE);
		}
		if( FractureParticleEffects(Depth) != NULL )
		{
			// spawn particle system
			eventSpawnFractureEmitter(FractureParticleEffects(Depth), SpawnLocation, SpawnDirection);
		}
	}
#endif
}

/** Customize Apex Destructible's physRigidBody so that it checks the collision with volumes. */
void AApexDestructibleActor::physRigidBody(FLOAT DeltaTime)
{
	Super::physRigidBody(DeltaTime);
#if WITH_APEX
	// look for blocking volumes overlapping this mesh, and disable collision with it
	FMemMark Mark(GMainThreadMemStack);
	FCheckResult* Link = GWorld->Hash->ActorOverlapCheck(GMainThreadMemStack, this, this->CollisionComponent->Bounds.Origin, this->CollisionComponent->Bounds.SphereRadius);
	for ( ; Link!=NULL; Link=Link->GetNext() )
	{
		if( Link->Actor
			&& !Link->Actor->bDeleteMe
			&& Link->Actor->CollisionComponent )
		{
			AVolume* volume = Cast<AVolume>(Link->Actor);
			if ( volume )
			{
				this->BeginTouch(Link->Actor,Link->Component,Link->Location,Link->Normal,Link->SourceComponent);
				Link->Actor->Touching.Empty();
			}
		}
	}
	Mark.Pop();
#endif

}

UBOOL AApexDestructibleActor::ShouldTrace(UPrimitiveComponent* Primitive, AActor *SourceActor, DWORD TraceFlags)
{
	// This is the AActor ShouldTrace function, except the first conditional has "bWorldGeometry" removed.  This way, destructibles will always accept TRACE_LevelGeometry traces
	if( TraceFlags & TRACE_LevelGeometry )
	{
		return TRUE;
	}
	else if( !bWorldGeometry && (TraceFlags & TRACE_Others) )
	{
		if( TraceFlags & TRACE_OnlyProjActor )
		{
			return (bProjTarget || (bBlockActors && Primitive->BlockActors));
		}
		else
		{
			return (!(TraceFlags & TRACE_Blocking) || (SourceActor && SourceActor->IsBlockedBy(this,Primitive)));
		}
	}
	return FALSE;
}

void InitMaterialForApex(UMaterialInterface *&MaterialInterface)
{
#if WITH_APEX
	if ( GApexManager )
	{
		NxApexSDK *apexSDK = GApexManager->GetApexSDK();
		NxResourceProvider* ApexResourceProvider = apexSDK->getNamedResourceProvider();
		check( ApexResourceProvider );
		if ( MaterialInterface == NULL )
		{
			MaterialInterface = GEngine->DefaultMaterial;
		}
		UMaterialInterface* UseMat = MaterialInterface;
		if( !UseMat->CheckMaterialUsage(MATUSAGE_APEXMesh) )
		{
			// In APEX we map the material name to the default material since the requested material is unusable
			UseMat = GEngine->DefaultMaterial;
		}
		ApexResourceProvider->setResource( "ApexMaterials", TCHAR_TO_ANSI(*MaterialInterface->GetPathName()), UseMat);
	}
#endif
}
