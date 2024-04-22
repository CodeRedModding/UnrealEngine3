/*=============================================================================
	UnNovodexSupport.cpp: Novodex support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EnginePhysicsClasses.h"

#include "NvApexManager.h"

#if WITH_APEX
#include <NvApexScene.h>
#include <PxMat34Legacy.h>

using namespace physx::apex;

#endif

#if WITH_NOVODEX

#include "UnNovodexSupport.h"

/** Util for seeing if PhysX hardware is present in this computer. */
UBOOL IsPhysXHardwarePresent()
{
	if(GEngine->bDisablePhysXHardwareSupport)
	{
		return FALSE;
	}
	else
	{
		return GNovodexSDK->getHWVersion() != NX_HW_VERSION_NONE;
	}
}


// hardware scene support - using NxScenePair
NxScenePair * GetNovodexScenePairFromIndex(INT InNovodexSceneIndex)
{
	return GNovodexSceneMap.Find(InNovodexSceneIndex);
}

/** Get the Primary Scene for a given scen pair index. */
NxScene* GetNovodexPrimarySceneFromIndex(INT InNovodexSceneIndex)
{
	NxScene *Scene = 0;
	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(InNovodexSceneIndex);
	if(ScenePair)
	{
		Scene = ScenePair->PrimaryScene;
	}
	return Scene;
}

/** Wait for a novodex scene to finish all simulations. */
void WaitForNovodexScene(NxScene &nScene)
{
	if(!nScene.isWritable())
	{
		SCOPE_CYCLE_COUNTER(STAT_RBTotalDynamicsTime);
		nScene.checkResults(NX_ALL_FINISHED, true);
	}
}

void WaitForAllNovodexScenes()
{
	for ( TMap<INT,NxScenePair>::TIterator It(GNovodexSceneMap); It; ++It )
	{
		NxScenePair& ScenePair = It.Value();
		if(ScenePair.PrimaryScene)
		{
			WaitForNovodexScene(*ScenePair.PrimaryScene);
		}
	}
}

class NxScene* FRBPhysScene::GetNovodexPrimaryScene(void)
{
	class NxScene *Scene = NULL;
	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(NovodexSceneIndex);
	if(ScenePair)
	{
		Scene = ScenePair->PrimaryScene;
	}
	return Scene;
}

class NxCompartment* FRBPhysScene::GetNovodexRigidBodyCompartment(void)
{
	class NxCompartment *Compartment = NULL;
	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(NovodexSceneIndex);
	if(ScenePair)
	{
		Compartment = ScenePair->RigidBodyCompartment;
	}
	return Compartment;
}

class NxCompartment* FRBPhysScene::GetNovodexFluidCompartment(void)
{
	class NxCompartment *Compartment = NULL;
	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(NovodexSceneIndex);
	if(ScenePair)
	{
		Compartment = ScenePair->FluidCompartment;
	}
	return Compartment;
}

class NxCompartment* FRBPhysScene::GetNovodexClothCompartment(void)
{
	class NxCompartment *Compartment = NULL;
	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(NovodexSceneIndex);
	if(ScenePair)
	{
		Compartment = ScenePair->ClothCompartment;
	}
	return Compartment;
}

class NxCompartment* FRBPhysScene::GetNovodexSoftBodyCompartment(void)
{
	class NxCompartment *Compartment = NULL;
	NxScenePair *ScenePair = GetNovodexScenePairFromIndex(NovodexSceneIndex);
	if(ScenePair)
	{
		Compartment = ScenePair->SoftBodyCompartment;
	}
	return Compartment;
}

static UPhysicalMaterial* GetPhysMatFromShape(NxShape* nShape, NxScene* nScene)
{
	NxMaterial* nMaterial = nScene->getMaterialFromIndex(nShape->getMaterial());
	UPhysicalMaterial* PhysMat = NULL;
	if(nMaterial)
	{
		PhysMat = (UPhysicalMaterial*)nMaterial->userData;
	}
	return PhysMat;
}

/** Utility for getting a Novodex NxActor from an FRigidBodyCollisionInfo. */
NxActor* FRigidBodyCollisionInfo::GetNxActor() const
{
	// If no component - can't do anything.
	if(Component)
	{
		// See if its a skeletal mesh using multiple bodies
		USkeletalMeshComponent* SkelComp = Cast<USkeletalMeshComponent>(Component);
		if (SkelComp != NULL && !SkelComp->bUseSingleBodyPhysics)
		{
			// If so, use BodyIndex to find element in Bodies array.
			if( SkelComp->PhysicsAssetInstance && 
				BodyIndex < SkelComp->PhysicsAssetInstance->Bodies.Num() )
			{
				return SkelComp->PhysicsAssetInstance->Bodies(BodyIndex)->GetNxActor();
			}
		}
		// If not a skel mesh or it is using single body physics, just call GetNxActor.
		else
		{
			return Component->GetNxActor();
		}
	}

	return NULL;
}

static void CreateCollisionInfo(NxActor* nActor, FRigidBodyCollisionInfo& OutInfo)
{
	URB_BodyInstance* BodyInst = (URB_BodyInstance*)(nActor->userData);
	if(BodyInst && BodyInst->OwnerComponent)
	{
		OutInfo.Component = BodyInst->OwnerComponent;
		OutInfo.Actor = OutInfo.Component->GetOwner();
		OutInfo.BodyIndex = BodyInst->BodyIndex;
	}
	else
	{
		OutInfo.Component = NULL;
		OutInfo.Actor = NULL;
		OutInfo.BodyIndex = INDEX_NONE;
	}
}

#if SUPPORT_DOUBLE_BUFFERING
/** 
 *	Handle error caused by double buffering:
 *	Actor has not been created properly.
 */
void FNxdDoubleBufferReport::actorInitializationFailed(NxActor *actor)
{
	debugf(NAME_Error,TEXT("Double Buffered created Actor at address %x was not initialized!"), actor);
}

/** 
 *	Handle error caused by double buffering:
 *	Joint has not been created properly.
 */
void FNxdDoubleBufferReport::jointInitializationFailed(NxJoint *joint)
{
	debugf(NAME_Error,TEXT("Double Buffered created Joint at address %x was not initialized!"), joint);
}

/** 
 *	Handle errors caused by double buffering:
 *	SpringAndDamperEffector has not been created properly.
 */
void FNxdDoubleBufferReport::springAndDamperEffectorInitializationFailed(NxSpringAndDamperEffector *sadEffector)
{
	debugf(NAME_Error,TEXT("Double Buffered created SpringAndDamperEffector at address %x was not initialized!"), sadEffector);
}

/** 
 *	Handle errors caused by double buffering:
 *	Material has not been created properly.
 */
void FNxdDoubleBufferReport::materialInitializationFailed(NxMaterial *material)
{
	debugf(NAME_Error,TEXT("Double Buffered created Material at address %x was not initialized!"), material);
}
#endif

/** 
 *	Called after Novodex contact generation and allows us to iterate over contact and extract useful information.
 */
void FNxContactReport::onContactNotify(NxContactPair& pair, NxU32 events)
{
	// Check actors are not destroyed
	if( pair.isDeletedActor[0] || pair.isDeletedActor[1] )
	{
		debugf(NAME_Error, TEXT("%d OnContactNotify(): Actors %d %d has been deleted!"), GFrameCounter, (INT)pair.isDeletedActor[0], (INT)pair.isDeletedActor[1] );
		return;
	}

	NxActor* nActor0 = pair.actors[0];
	NxActor* nActor1 = pair.actors[1];

	check(nActor0 && nActor1);

	URB_BodyInstance* BodyInst0 = (URB_BodyInstance*)(nActor0->userData);
	URB_BodyInstance* BodyInst1 = (URB_BodyInstance*)(nActor1->userData);

#if WITH_APEX
	// APEX filtering
	if( GApexManager )
	{
		const NxApexPhysXObjectDesc *ApexDesc0 = GApexManager->GetApexSDK()->getPhysXObjectInfo( nActor0 );
		if( ApexDesc0 && ApexDesc0->ignoreContacts() )
		{
			return;
		}
		const NxApexPhysXObjectDesc *ApexDesc1 = GApexManager->GetApexSDK()->getPhysXObjectInfo( nActor1 );
		if( ApexDesc1 && ApexDesc1->ignoreContacts() )
		{
			return;
		}
	}
#endif

	// Get the Scene. We need this for turning material IDs into NxMaterial pointers.
	NxScene* nScene = &nActor0->getScene();
	if (nScene != &nActor1->getScene())
	{
		AActor* Actor0 = (BodyInst0 != NULL && BodyInst0->OwnerComponent != NULL) ? BodyInst0->OwnerComponent->GetOwner() : NULL;
		AActor* Actor1 = (BodyInst1 != NULL && BodyInst1->OwnerComponent != NULL) ? BodyInst1->OwnerComponent->GetOwner() : NULL;
		debugf(NAME_Error, TEXT("OnContactNotify(): Actors (%s) and (%s) are not in the same physics scene!"), *Actor0->GetName(), *Actor1->GetName());
		return;
	}

	// We can get to the FRBPhysScene from here.
	FRBPhysScene* RBPhysScene = (FRBPhysScene*)nScene->userData;

	FPushNotifyInfo* PushInfo = NULL;
	FCollisionNotifyInfo* NotifyInfo = NULL;

	// First, look for 'push' body, used for pushing physics objects.
	NxActor* nPusher = NULL;
	NxActor* nPushed = NULL;
	FLOAT NormalFlip = 1.f;
	if (BodyInst0 && BodyInst0->bPushBody && BodyInst1)
	{
		nPusher = nActor0;
		nPushed = nActor1;
	}
	else if (BodyInst1 && BodyInst1->bPushBody && BodyInst0)
	{
		nPusher = nActor1;
		nPushed = nActor0;
		NormalFlip = -1.f;
	}

	TArray<FRigidBodyContactInfo>* ContactInfoArray = NULL;

	// push body case - not a physical contact
	if (nPusher && nPushed)
	{
		// Only care about collisions with dynamic objects (not static or kinematic).
		if(!nPushed->isDynamic() || nPushed->readBodyFlag(NX_BF_KINEMATIC))
		{
			return;
		}
		else
		{
			// Now we allocate an entry in the list of pending notify calls.
			PushInfo = new(RBPhysScene->PendingPushNotifies) FPushNotifyInfo();

			// Fill in pushed info
			CreateCollisionInfo(nPushed, PushInfo->PushedInfo);

			// Fill in pusher info
			FRigidBodyCollisionInfo PusherInfo;
			CreateCollisionInfo(nPusher, PusherInfo);
			PushInfo->Pusher = CastChecked<APawn>(PusherInfo.Actor);

			ContactInfoArray = &(PushInfo->ContactInfos);
		}
	}
	else
	{
		// Discard contacts that don't generate any force (eg. have been rejected through a modify contact callback).
		if( pair.sumNormalForce.magnitudeSquared() < KINDA_SMALL_NUMBER )
		{
			// ... unless it's a sensor body contact, which won't generate force by definition
			if (!nActor0->readActorFlag(NX_AF_DISABLE_RESPONSE) && !nActor1->readActorFlag(NX_AF_DISABLE_RESPONSE))
			{
				return;
			}
		}

		// Now we allocate an entry in the list of pending notify calls.
		NotifyInfo = new(RBPhysScene->PendingCollisionNotifies) FCollisionNotifyInfo();

		// Fill in the NotifyInfo struct.
		NotifyInfo->bCallEvent0 = (nActor0->getGroup() == UNX_GROUP_NOTIFYCOLLIDE) || (nActor0->getGroup() == UNX_GROUP_THRESHOLD_NOTIFY);
		CreateCollisionInfo(nActor0, NotifyInfo->Info0);

		NotifyInfo->bCallEvent1 = (nActor1->getGroup() == UNX_GROUP_NOTIFYCOLLIDE) || (nActor1->getGroup() == UNX_GROUP_THRESHOLD_NOTIFY);
		CreateCollisionInfo(nActor1, NotifyInfo->Info1);

		
		FCollisionImpactData* ImpactInfo =&(NotifyInfo->RigidCollisionData);
		ContactInfoArray = &(ImpactInfo->ContactInfos);
		ImpactInfo->TotalFrictionForceVector = N2UVectorCopy(pair.sumFrictionForce);
		ImpactInfo->TotalNormalForceVector = N2UVectorCopy(pair.sumNormalForce);
	}

	// Iterate through contact points
	NxContactStreamIterator It(pair.stream);
	FLOAT MaxPenetration = 0.f;
	while(It.goNextPair())
	{
		// Get the two shapes that are involved in the collision
		NxShape* Shape0 = It.getShape(0);
		check(Shape0);
		NxShape* Shape1 = It.getShape(1);
		check(Shape1);

		// Determine if this contact is with a wheel shape.
		UBOOL bIsWheelContact = ((Shape0->getType() == NX_SHAPE_WHEEL) || (Shape1->getType() == NX_SHAPE_WHEEL));

		UPhysicalMaterial* PhysMat0 = GetPhysMatFromShape( It.getShape(0), nScene );
		UPhysicalMaterial* PhysMat1 = GetPhysMatFromShape( It.getShape(1), nScene );

		// prepare to override physical materials
		UBOOL bIsShape0Heightfield = (Shape0->getType() == NX_SHAPE_HEIGHTFIELD);
		UBOOL bIsShape1Heightfield = (Shape1->getType() == NX_SHAPE_HEIGHTFIELD);

		while(It.goNextPatch())
		{
			NxVec3 nContactNormal = It.getPatchNormal();

			while(It.goNextPoint())
			{
				NxVec3 nContactPos = It.getPoint();

				// Look up the per-triangle material for heightfields
				if( bIsShape0Heightfield )
				{
					NxHeightField& nHeightfield = ((NxHeightFieldShape*)Shape0)->getHeightField();
					const NxHeightFieldSample* HeightfieldSamples = (const NxHeightFieldSample*)nHeightfield.getCells();
					NxU32 SampleIndex = It.getFeatureIndex0();
					if( SampleIndex < 2 * nHeightfield.getNbColumns() * nHeightfield.getNbRows() )
					{
						const NxHeightFieldSample& nSample = HeightfieldSamples[SampleIndex>>1];
						NxMaterial* nMaterial = nScene->getMaterialFromIndex((SampleIndex & 1) ? nSample.materialIndex1 : nSample.materialIndex0 );
						if(nMaterial)
						{
							PhysMat0 = (UPhysicalMaterial*)nMaterial->userData;
						}
					}
				}
				else
				if( bIsShape1Heightfield )
				{
					NxHeightField& nHeightfield = ((NxHeightFieldShape*)Shape1)->getHeightField();
					const NxHeightFieldSample* HeightfieldSamples = (const NxHeightFieldSample*)nHeightfield.getCells();
					NxU32 SampleIndex = It.getFeatureIndex1();
					if( SampleIndex < 2 * nHeightfield.getNbColumns() * nHeightfield.getNbRows() )
					{
						const NxHeightFieldSample& nSample = HeightfieldSamples[SampleIndex>>1];
						NxMaterial* nMaterial = nScene->getMaterialFromIndex((SampleIndex & 1) ? nSample.materialIndex1 : nSample.materialIndex0 );
						if(nMaterial)
						{
							PhysMat1 = (UPhysicalMaterial*)nMaterial->userData;
						}
					}
				}

				//If not a wheel, add entry to the TempContactInfos array.
				if(!bIsWheelContact)
				{
					FLOAT Penetration = -1.f * P2UScale * It.getSeparation();

					new(*ContactInfoArray)FRigidBodyContactInfo(N2UPosition(nContactPos), 
																NormalFlip * N2UVectorCopy(nContactNormal), 
																Penetration, 
																N2UPosition(nActor0->getPointVelocity(nContactPos)), 
																N2UPosition(nActor1->getPointVelocity(nContactPos)),
																PhysMat0, 
																PhysMat1);
				
				}
			}
		}	
	}
}

///////////////////// Unreal to Novodex conversion /////////////////////

NxMat34 U2NMatrixCopy(const FMatrix& uTM)
{
	NxMat34 Result;

	// Copy rotation
	NxF32 Entries[9];
	Entries[0] = uTM.M[0][0];
	Entries[1] = uTM.M[0][1];
	Entries[2] = uTM.M[0][2];

	Entries[3] = uTM.M[1][0];
	Entries[4] = uTM.M[1][1];
	Entries[5] = uTM.M[1][2];

	Entries[6] = uTM.M[2][0];
	Entries[7] = uTM.M[2][1];
	Entries[8] = uTM.M[2][2];

	Result.M.setColumnMajor(Entries);

	// Copy translation
	Result.t.x = uTM.M[3][0];
	Result.t.y = uTM.M[3][1];
	Result.t.z = uTM.M[3][2];

	return Result;
}

NxMat34 U2NTransform(const FMatrix& uTM)
{
	NxMat34 Result;

	// Copy rotation
	NxF32 Entries[9];
	Entries[0] = uTM.M[0][0];
	Entries[1] = uTM.M[0][1];
	Entries[2] = uTM.M[0][2];

	Entries[3] = uTM.M[1][0];
	Entries[4] = uTM.M[1][1];
	Entries[5] = uTM.M[1][2];

	Entries[6] = uTM.M[2][0];
	Entries[7] = uTM.M[2][1];
	Entries[8] = uTM.M[2][2];

	Result.M.setColumnMajor(Entries);

	// Copy translation
	Result.t.x = uTM.M[3][0] * U2PScale;
	Result.t.y = uTM.M[3][1] * U2PScale;
	Result.t.z = uTM.M[3][2] * U2PScale;

	return Result;
}


NxVec3 U2NVectorCopy(const FVector& uVec)
{
	return NxVec3(uVec.X, uVec.Y, uVec.Z);
}

NxVec3 U2NPosition(const FVector& uPos)
{
	return NxVec3(uPos.X * U2PScale, uPos.Y * U2PScale, uPos.Z * U2PScale);
}

NxQuat U2NQuaternion(const FQuat& uQuat)
{
	return NxQuat( NxVec3(uQuat.X, uQuat.Y, uQuat.Z), uQuat.W );
}

///////////////////// Novodex to Unreal conversion /////////////////////


FMatrix N2UTransform(const NxMat34& nTM)
{
	FMatrix Result;

	// Copy rotation
	NxF32 Entries[9];
	nTM.M.getColumnMajor(Entries);

	Result.M[0][0] = Entries[0];
	Result.M[0][1] = Entries[1];
	Result.M[0][2] = Entries[2];

	Result.M[1][0] = Entries[3];
	Result.M[1][1] = Entries[4];
	Result.M[1][2] = Entries[5];

	Result.M[2][0] = Entries[6];
	Result.M[2][1] = Entries[7];
	Result.M[2][2] = Entries[8];

	// Copy translation
	Result.M[3][0] = nTM.t.x * P2UScale;
	Result.M[3][1] = nTM.t.y * P2UScale;
	Result.M[3][2] = nTM.t.z * P2UScale;

	// Fix fourth column
	Result.M[0][3] = Result.M[1][3] = Result.M[2][3] = 0.0f;
	Result.M[3][3] = 1.0f;

	return Result;
}


FVector N2UVectorCopy(const NxVec3& nVec)
{
	return FVector(nVec.x, nVec.y, nVec.z);
}

FVector N2UPosition(const NxVec3& nVec)
{
	return FVector(nVec.x * P2UScale, nVec.y * P2UScale, nVec.z * P2UScale);
}

FQuat N2UQuaternion(const NxQuat& nQuat)
{
	return FQuat(nQuat.x, nQuat.y, nQuat.z, nQuat.w);
}

/** Util for creating simple, collisionless kinematic actor, usually for creating springs to and animating etc. */
NxActor* CreateDummyKinActor(NxScene* NovodexScene, const FMatrix& ActorTM)
{
	// Create kinematic actor we are going to create joint with. This will be moved around with calls to SetLocation/SetRotation.
	NxActorDesc KinActorDesc;
	KinActorDesc.globalPose = U2NTransform(ActorTM);
	KinActorDesc.density = 1.f;
	KinActorDesc.flags |= NX_AF_DISABLE_COLLISION;

	NxSphereShapeDesc KinActorShapeDesc;
	KinActorShapeDesc.radius = 1.f;
	KinActorDesc.shapes.push_back(&KinActorShapeDesc);

	NxBodyDesc KinBodyDesc;
	KinBodyDesc.flags |= NX_BF_KINEMATIC;
	KinActorDesc.body = &KinBodyDesc;

	NxActor* KinActor = NovodexScene->createActor(KinActorDesc);
	KinActor->userData = NULL;

	return KinActor;
}

/** Util to destroy an actor created with the code above. */
void DestroyDummyKinActor(NxActor* KinActor)
{
	check(KinActor);
	// If physics is running, defer destruction of NxActor
	if (GWorld && GWorld->InTick && GWorld->TickGroup == TG_DuringAsyncWork)
	{
		DeferredReleaseNxActor(KinActor, TRUE);
	}
	else
	{
		KinActor->getScene().releaseActor(*KinActor);
	}
}


//////////////////////////////////////////////////////////////////////////

UBOOL MatricesAreEqual(const NxMat34& M1, const NxMat34& M2, FLOAT Tolerance)
{
	// Each row
	for(INT i=0; i<3; i++)
	{
		// Each column
		for(INT j=0; j<3; j++)
		{
			if( Abs(M1.M(i,j) - M2.M(i,j)) > Tolerance )
			{
				return FALSE;
			}
		}

		// Check translation
		if( Abs(M1.t[i] - M2.t[i]) > Tolerance )
		{
			return FALSE;
		}
	}

	// No differences found
	return TRUE;
}


NxU32 GetNxTriMeshRefCount( const NxTriangleMesh * tm )
{
	class RefCountable
	{
	public:
		NxU32	numRefs;
	};

	class NpTriangleMesh : public NxTriangleMesh, public RefCountable
	{	
		void *	data;
	};

	return ((const NpTriangleMesh *)tm)->numRefs;
}

NxU32 GetNxConvexMeshRefCount( const NxConvexMesh * cm )
{
	class RefCountable
	{
	public:
		NxU32	numRefs;
	};

	class NpConvexMesh : public NxConvexMesh, public RefCountable
	{	
		void *	data;
	};

	return ((const NpConvexMesh *)cm)->numRefs;
}

//////////////////////////////////////////////////////////////////////////

#define FNX_READ_TYPE( type ) \
check(Data); \
const UINT OldPos = ReadPos; \
ReadPos += sizeof(type); \
if( ReadPos > (UINT)(*Data).Num() ) \
{ \
	ReadPos = OldPos; \
	return (type)0; \
} \
return *(type*)&(*Data)(OldPos)


NxU8 FNxMemoryBuffer::readByte(void) const
{
	FNX_READ_TYPE( NxU8 );
}

NxU16 FNxMemoryBuffer::readWord(void) const
{
	FNX_READ_TYPE( NxU16 );
}

NxU32 FNxMemoryBuffer::readDword(void)    const
{
	FNX_READ_TYPE( NxU32 );
}

float FNxMemoryBuffer::readFloat(void)    const
{
	FNX_READ_TYPE( float );
}

double FNxMemoryBuffer::readDouble(void) const
{
	FNX_READ_TYPE( double );
}

void FNxMemoryBuffer::readBuffer(void* buffer, NxU32 size)  const
{
	check(Data);
	check(size);
	const UINT EndPos = ReadPos + size;
	if( EndPos <= (UINT)(*Data).Num() )
	{
		appMemcpy( buffer, &(*Data)(ReadPos), size );
		ReadPos = EndPos;
	}
}

NxStream& FNxMemoryBuffer::storeByte(NxU8 b)
{
	storeBuffer(&b, sizeof(NxU8) );
	return *this;
}

NxStream& FNxMemoryBuffer::storeWord(NxU16 w)
{
	storeBuffer(&w,sizeof(NxU16));
	return *this;
}

NxStream& FNxMemoryBuffer::storeDword(NxU32 d)
{
	storeBuffer(&d,sizeof(NxU32));
	return *this;
}

NxStream& FNxMemoryBuffer::storeFloat(NxReal f)
{
	storeBuffer(&f,sizeof(NxReal));
	return *this;
}

NxStream& FNxMemoryBuffer::storeDouble(NxF64 f)
{
	storeBuffer(&f,sizeof(NxF64));
	return *this;
}

NxStream& FNxMemoryBuffer::storeBuffer(const void* buffer, NxU32 size)
{
	check(Data);
	INT CurrentNum = (*Data).Num();
	(*Data).Add(size);
	appMemcpy( &(*Data)(CurrentNum), buffer, size );
	return *this;
}

//////////////////////////////////////////////////////////////////////////


NxGroupsMask CreateGroupsMask(BYTE Channel, FRBCollisionChannelContainer* CollidesChannels)
{
	NxGroupsMask NewMask;
	appMemzero(&NewMask, sizeof(NxGroupsMask));

	INT ChannelShift = (INT)Channel;

#if !__INTEL_BYTE_ORDER__
	NewMask.bits0 = (1 << (31 - ChannelShift));
#else
	NewMask.bits0 = (1 << ChannelShift);
#endif

	if(CollidesChannels)
	{
		NewMask.bits2 = CollidesChannels->Bitfield;
	}

	return NewMask;
}

/** Utility for setting the material on all shapes that make up this NxActor physics object. */
void SetNxActorMaterial(NxActor* nActor, NxMaterialIndex NewMaterial, const UPhysicalMaterial* PhysMat)
{
	INT NumShapes = nActor->getNbShapes();
	NxShape *const * Shapes = nActor->getShapes();
	for(INT j=0; j<NumShapes; j++)
	{
		NxShape* nShape = Shapes[j];
		nShape->setMaterial(NewMaterial);
	}	

	// Set damping properties.
	nActor->setAngularDamping(PhysMat->AngularDamping);
	nActor->setLinearDamping(PhysMat->LinearDamping);
}


/** Util to force-set the ref count of an NxTriangleMesh */
void SetNxTriMeshRefCount( NxTriangleMesh * tm, int refCount )
{
	class RefCountable
	{
	public:
		NxU32   numRefs;
	};

	class NpTriangleMesh : public NxTriangleMesh, public
		RefCountable
	{      
		void *  data;
	};

	((NpTriangleMesh *)tm)->numRefs = refCount;
	check(tm->getReferenceCount() == refCount);
}

/** Util to force-set the ref count of an NxConvexMesh */
void SetNxConvexMeshRefCount( NxConvexMesh * cm, int refCount )
{
	class RefCountable
	{
	public:
		NxU32   numRefs;
	};

	class NpConvexMesh : public NxConvexMesh, public RefCountable
	{      
		void *  data;
	};

	((NpConvexMesh *)cm)->numRefs = refCount;
	check(cm->getReferenceCount() == refCount);
}

/** Add a force to an NxActor, but do not modify its waking state. */
void AddForceNoWake(NxActor *Actor, const NxVec3 &force) 
{
	if(!Actor->isSleeping())
	{
		Actor->addForce(force, NX_FORCE, false);
	}
}

/** Add a force to an NxActor, but only if the force is non-zero. */
void AddForceZeroCheck(NxActor *Actor, const NxVec3 &force)
{
	if(	(Abs(force.x)>KINDA_SMALL_NUMBER) || 
		(Abs(force.y)>KINDA_SMALL_NUMBER) || 
		(Abs(force.z)>KINDA_SMALL_NUMBER) )
	{
		addForce(Actor,force);
	}
}


/** Perform a cloth line check, uses an approximate obb for non zero extent checks. */
UBOOL ClothLineCheck(
				USkeletalMeshComponent* SkelComp,
				FCheckResult &Result,
                const FVector& End,
                const FVector& Start,
                const FVector& Extent,
				DWORD TraceFlags)
{
	UBOOL Retval = TRUE;


#if WITH_NOVODEX && !NX_DISABLE_CLOTH
	
	check(SkelComp);
	check(SkelComp->ClothSim);
	

	UBOOL bIsZeroExtent = Extent.IsZero();

	Result.Item = INDEX_NONE;
	Result.Time = 1.0f;
	Result.BoneName = NAME_None;
	Result.Component = NULL;
	Result.Material = NULL;
	Result.PhysMaterial = NULL;

	NxCloth* Cloth = (NxCloth *)SkelComp->ClothSim;

	if(bIsZeroExtent)
	{
		/* Simple raycast. */

		NxRay nWorldRay;
		NxVec3 nHit;
		NxU32 nVertexId;

		//What about Start==End??
		nWorldRay.orig = U2NPosition(Start);
		nWorldRay.dir = U2NPosition(End) - nWorldRay.orig;

		if(Cloth->raycast(nWorldRay, nHit, nVertexId))
		{
			NxReal nMag = (nHit - nWorldRay.orig).magnitude();
			NxReal nDirMag = nWorldRay.dir.magnitude();

			if(nMag <= nDirMag)
			{
				Result.Location = N2UPosition(nHit);
				if(SkelComp->ClothMeshNormalData.Num() > 0)
				{
					Result.Normal   = SkelComp->ClothMeshNormalData((INT)nVertexId);
					if((Result.Normal | (End - Start)) > 0.0f)
					{
						Result.Normal = -Result.Normal;
					}
					
				}
				Result.Time     = nMag / nDirMag;
				Result.Component= SkelComp;
				Result.Actor    = SkelComp->GetOwner();
				Retval          = FALSE;
			}
		}

	}
	else
	{
		/* 
		Swept box against cloth mesh. 
		We use the cloth AABB as an approximation since doing a full sweep test against the cloth is 
		expensive and painful.
		*/

		NxBounds3 WorldBounds;
		Cloth->getWorldBounds(WorldBounds);
		FLOAT ClothThickness = Cloth->getThickness();

		WorldBounds.min -= NxVec3(ClothThickness, ClothThickness, ClothThickness);
		WorldBounds.max += NxVec3(ClothThickness, ClothThickness, ClothThickness);

		FVector WorldExtent = N2UPosition(WorldBounds.max - WorldBounds.min);
		FVector WorldCentre = N2UPosition((WorldBounds.max + WorldBounds.min) * 0.5f);

		FKBoxElem ClothCollisionBox;
		
		ClothCollisionBox.X = WorldExtent.X;
		ClothCollisionBox.Y = WorldExtent.Y;
		ClothCollisionBox.Z = WorldExtent.Z;

		ClothCollisionBox.TM = FMatrix(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), WorldCentre);

		/* LineCheck appears to disregard the FKBoxElem transform, so feed in here */
		Retval = ClothCollisionBox.LineCheck(Result, ClothCollisionBox.TM, 1.0f, End, Start, Extent);
		if(!Retval)
		{
			Result.Component = SkelComp;
			Result.Actor = SkelComp->GetOwner();
		}
	}
#endif //WITH_NOVODEX && !NX_DISABLE_CLOTH

	return Retval;
}

/** Perform a point check against a cloth(uses an approximate OBB) */
UBOOL ClothPointCheck(FCheckResult &Result, USkeletalMeshComponent* SkelComp, const FVector& Location, const FVector& Extent)
{
#if WITH_NOVODEX && !NX_DISABLE_CLOTH

	check(SkelComp);
	check(SkelComp->ClothSim);

	NxCloth* Cloth = (NxCloth *)SkelComp->ClothSim;

	NxBounds3 WorldBounds;
	Cloth->getWorldBounds(WorldBounds);
	FLOAT ClothThickness = Cloth->getThickness();

	WorldBounds.min -= NxVec3(ClothThickness, ClothThickness, ClothThickness);
	WorldBounds.max += NxVec3(ClothThickness, ClothThickness, ClothThickness);

	FVector WorldExtent = N2UPosition(WorldBounds.max - WorldBounds.min);
	FVector WorldCentre = N2UPosition((WorldBounds.max + WorldBounds.min) * 0.5f);

	FKBoxElem ClothCollisionBox;
	
	ClothCollisionBox.X = WorldExtent.X;
	ClothCollisionBox.Y = WorldExtent.Y;
	ClothCollisionBox.Z = WorldExtent.Z;

	ClothCollisionBox.TM = FMatrix(FVector(1.0f, 0.0f, 0.0f), FVector(0.0f, 1.0f, 0.0f), FVector(0.0f, 0.0f, 1.0f), WorldCentre);

	FLOAT OutBestDist = BIG_NUMBER;

	UBOOL Retval = ClothCollisionBox.PointCheck(Result, OutBestDist, ClothCollisionBox.TM, 1.0f, Location, Extent);
	if(!Retval)
	{
		Result.Component = SkelComp;
		Result.Actor = SkelComp->GetOwner();
	}

	return Retval;
#else //WITH_NOVODEX && !NX_DISABLE_CLOTH
	return TRUE;
#endif //WITH_NOVODEX && !NX_DISABLE_CLOTH
}

/** */
UBOOL SoftBodyLineCheck(
					 USkeletalMeshComponent* SkelComp,
					 FCheckResult &Result,
					 const FVector& End,
					 const FVector& Start,					 
					 const FVector& Extent,
					 DWORD TraceFlags)
{
	UBOOL Retval = TRUE;

#if WITH_NOVODEX && !NX_DISABLE_SOFTBODY

	check(SkelComp);
	check(SkelComp->SoftBodySim);

	UBOOL bIsZeroExtent = Extent.IsZero();

	Result.Item = INDEX_NONE;
	Result.Time = 1.0f;
	Result.BoneName = NAME_None;
	Result.Component = NULL;
	Result.Material = NULL;
	Result.PhysMaterial = NULL;

	NxSoftBody* SoftBody = (NxSoftBody *)SkelComp->SoftBodySim;

	if(bIsZeroExtent)
	{
		/* Simple raycast. */

		NxRay nWorldRay;
		NxVec3 nHit;
		NxU32 nVertexId;

		//What about Start==End??
		nWorldRay.orig = U2NPosition(Start);
		nWorldRay.dir = U2NPosition(End) - nWorldRay.orig;

		if(SoftBody->raycast(nWorldRay, nHit, nVertexId))
		{
			NxReal nMag = (nHit - nWorldRay.orig).magnitude();
			NxReal nDirMag = nWorldRay.dir.magnitude();

			if(nMag <= nDirMag)
			{
				Result.Location = N2UPosition(nHit);
				Result.Normal = (Start - End).SafeNormal(); // TODO: Use better normal...
				Result.Time     = nMag / nDirMag;
				Result.Component= SkelComp;
				Result.Actor    = SkelComp->GetOwner();
				Retval          = FALSE;
			}
		}
	}

#endif //WITH_NOVODEX && !NX_DISABLE_CLOTH

	return Retval;
}

DWORD FindNovodexSceneStat(NxScene* NovodexScene, const TCHAR* StatNxName, UBOOL bMaxValue)
{
	const NxSceneStats2* SceneStats2 = NovodexScene->getStats2();

	if(SceneStats2 == NULL)
	{
		return 0;
	}
//HACK: Some workarounds for broken NxStats2...
#if WITH_NOVODEX && !defined(NX_DISABLE_FLUIDS)
	if(appStricmp(StatNxName, TEXT("TotalFluidParticles")) == 0)
	{
		NxU32 ManualTotal = 0;

		for(NxU32 i=0; i<NovodexScene->getNbFluids(); i++)
		{
			NxFluid* Fluid = NovodexScene->getFluids()[i];

			NxParticleData ParticleData = Fluid->getParticlesWriteData();

			NxU32 Num = *ParticleData.numParticlesPtr;
			ManualTotal+=Num;
		}

		return ManualTotal;
	}
	else if(appStricmp(StatNxName, TEXT("ActiveFluidParticles")) == 0)
	{
		NxU32 ManualTotal = 0;

		for(NxU32 i=0; i<NovodexScene->getNbFluids(); i++)
		{
			NxFluid* Fluid = NovodexScene->getFluids()[i];

			if(!Fluid->getFlag(NX_FF_ENABLED))
				continue;

			NxParticleData ParticleData = Fluid->getParticlesWriteData();

			NxU32 Num = *ParticleData.numParticlesPtr;
			ManualTotal+=Num;
		}

		return ManualTotal;
	}
	else 
#endif //WITH_NOVODEX && !defined(NX_DISABLE_FLUIDS)
#if WITH_NOVODEX && !defined(NX_DISABLE_CLOTH)
	if(appStricmp(StatNxName, TEXT("TotalCloths")) == 0)
	{
		return NovodexScene->getNbCloths();
	}
	else if(appStricmp(StatNxName, TEXT("ActiveCloths")) == 0)
	{
		INT ManualTotal = 0;

		for(NxU32 i=0; i<NovodexScene->getNbCloths(); i++)
		{
			NxCloth* Cloth = NovodexScene->getCloths()[i];


			if(!Cloth->isSleeping())
			{
				ManualTotal++;
			}
		}

		return ManualTotal;
	}
	else
#endif	// WITH_NOVODEX && !defined(NX_DISABLE_CLOTH)
	if(appStricmp(StatNxName, TEXT("GpuHeapUsageTotal")) == 0 ||
			appStricmp(StatNxName, TEXT("GpuHeapUsageFluid")) == 0 ||
			appStricmp(StatNxName, TEXT("GpuHeapUsageDeformable")) == 0 ||
			appStricmp(StatNxName, TEXT("GpuHeapUsageUtils")) == 0)
	{
		/* GPU mem usage is another special case, since it's a "singular"
		   value but duplicated several times in the list. see below for
		   details. 

		   curValue isn't that useful according to the docs. however, the
		   interface allows the user to choose current vs max, so preserve
		   that feature in case the caller has a good reason to ask for it */
		NxI32 MaxValue = 0;
		NxI32 Value = 0;

		for(UINT i=0; i<SceneStats2->numStats; i++)
		{
			/* multiple GpuHeapUsageTotal records will likely exist, but they should
			   all agree. at the time of writing, a few leading records have a max
			   of 0, while subsequent records (consistently) contain the correct value.
			   skip to the meaningful data if necessary */
			if(SceneStats2->stats[i].name == NULL ||
			   SceneStats2->stats[i].maxValue == 0)
				continue;

			FANSIToTCHAR StatName(SceneStats2->stats[i].name);
			if(appStricmp(StatName, StatNxName) == 0)
			{
				MaxValue += SceneStats2->stats[i].maxValue;
				Value += SceneStats2->stats[i].curValue;
				
				// found our non-zero value - all done. 
				break;
			}
		}

		return bMaxValue ? MaxValue :
						   Value;
	}

	//HACK: Some workarounds for broken NxStats2...

	/* Sum up all values of the stat, some versions of PhysX have multiple stats in the array from different compartments */
	
	NxI32 SumMaxValue = 0;
	NxI32 SumValue = 0;
	
	for(UINT i=0; i<SceneStats2->numStats; i++)
	{
		if(SceneStats2->stats[i].name == NULL)
			continue;

		FANSIToTCHAR StatName(SceneStats2->stats[i].name);

		if(appStricmp(StatName, StatNxName) == 0)
		{
			SumMaxValue += SceneStats2->stats[i].maxValue;
			SumValue += SceneStats2->stats[i].curValue;
		}
	}

	if(bMaxValue)
		return SumMaxValue;
	else
		return SumValue;
}

// Utility for ScanGeom, below
inline UBOOL ClipRay( const FKConvexElem & Convex, const FVector & Start, const FVector & End, FLOAT & In, FLOAT & Out ) 
{
	FCheckResult Hit;
	const FVector Zero(0,0,0);

	Hit.Time = 1.0f;
	if( 0 == Convex.LineCheck( Hit, FMatrix::Identity, End, Start, Zero ) )
	{
		In = Hit.Time;
		Hit.Time = 1.0f;
		if( 0 == Convex.LineCheck( Hit, FMatrix::Identity, Start, End, Zero ) )
		{
			Out = 1.0f-Hit.Time;
			return TRUE;
		}
	}

	return FALSE;
}

/** Scan FKAggregateGeom to discretize volume.  Scan is done across minimum AABB area.
 *	Positions array is appended, not initialized, by this function.
 */
void ScanGeom( FKAggregateGeom & Geom, const FMatrix & GeomLocalToWorld, const FVector & Scale3D, FLOAT ParticleSize, INT MaxParticles, TArray<FVector> & Positions )
{
	if( ParticleSize <= 0.0f )
	{
		return;
	}

	const FVector nScale3D = Scale3D*U2PScale;
	const FVector RecipScale( (Scale3D[0]?1.0f/Scale3D[0]:0.0f), (Scale3D[1]?1.0f/Scale3D[1]:0.0f), (Scale3D[2]?1.0f/Scale3D[2]:0.0f) );
	const FVector BoundsEpsilon = 0.01f*RecipScale;
	const FVector RecipScaleDelta = ParticleSize*RecipScale;
	const FLOAT nParticleSize = ParticleSize*U2PScale;

	// Only handle convex elements for now.  Others are easy to fill.
	for( INT i = 0; i < Geom.ConvexElems.Num(); ++i )
	{
		FKConvexElem & Convex = Geom.ConvexElems(i);

		FBox ShapeBounds = Convex.CalcAABB( FMatrix::Identity, FVector(1,1,1) );	// Perform raycast in local space
		FVector Extents = ShapeBounds.GetExtent();
		FVector Areas(Extents.Y*Extents.Z, Extents.Z*Extents.X, Extents.X*Extents.Y);
		INT Axis = Areas.X < Areas.Y && Areas.X < Areas.Z ? 0 : (Areas.Y < Areas.Z ? 1 : 2);
		INT Axis1 = (Axis+1)%3;
		INT Axis2 = (Axis+2)%3;
		// if the ray is 'on' the surface, it counts as a miss. this covers up for that.
		ShapeBounds.Min[Axis1] += BoundsEpsilon[Axis1];
		ShapeBounds.Max[Axis1] -= BoundsEpsilon[Axis1];
		ShapeBounds.Min[Axis2] += BoundsEpsilon[Axis2];
		ShapeBounds.Max[Axis2] -= BoundsEpsilon[Axis2];
		if(ShapeBounds.Min[Axis1] > ShapeBounds.Max[Axis1] || ParticleSize <= SMALL_NUMBER)
		{
			ShapeBounds.Min[Axis1] = ShapeBounds.Max[Axis1] = (ShapeBounds.Min[Axis1] + ShapeBounds.Max[Axis1]) * 0.5f;
		}
		if(ShapeBounds.Min[Axis2] > ShapeBounds.Max[Axis2] || ParticleSize <= SMALL_NUMBER)
		{
			ShapeBounds.Min[Axis2] = ShapeBounds.Max[Axis2] = (ShapeBounds.Min[Axis2] + ShapeBounds.Max[Axis2]) * 0.5f;
		}
		// make sure our rays don't start inside the surface...
		ShapeBounds.Min[Axis] -= BoundsEpsilon[Axis];
		ShapeBounds.Max[Axis] += BoundsEpsilon[Axis];
		const FLOAT nDist = nScale3D[Axis]*(ShapeBounds.Max[Axis]-ShapeBounds.Min[Axis]);
		const FLOAT Dist = P2UScale*nDist;
		const FVector Disp = GeomLocalToWorld.GetAxis( Axis )*nDist;
		const FVector Delta = GeomLocalToWorld.GetAxis( Axis )*nParticleSize;
		const FVector Delta1 = GeomLocalToWorld.GetAxis( Axis1 )*nParticleSize;
		const FVector Delta2 = GeomLocalToWorld.GetAxis( Axis2 )*nParticleSize;
		FVector EdgePosition = GeomLocalToWorld.TransformFVector( ShapeBounds.Min*Scale3D )*U2PScale; // Start on vertex
		for( FLOAT D2 = ShapeBounds.Min[Axis2]; D2 <= ShapeBounds.Max[Axis2]; D2 += RecipScaleDelta[Axis2], EdgePosition += Delta2 )
		{
			FVector FacePosition = EdgePosition;	// Start on edge
			for( FLOAT D1 = ShapeBounds.Min[Axis1]; D1 <= ShapeBounds.Max[Axis1]; D1 += RecipScaleDelta[Axis1], FacePosition += Delta1 )
			{
				FTwoVectors LineSeg;
				LineSeg.v1[Axis] = ShapeBounds.Min[Axis];
				LineSeg.v2[Axis] = ShapeBounds.Max[Axis];
				LineSeg.v1[Axis1] = LineSeg.v2[Axis1] = D1;
				LineSeg.v1[Axis2] = LineSeg.v2[Axis2] = D2;
				FLOAT In, Out;
				if( ClipRay( Convex, LineSeg.v1, LineSeg.v2, In, Out ) )
				{
					FVector Position = FacePosition + Disp*In;
					In *= Dist;
					Out *= Dist;
					for( FLOAT D = In; D <= Out; D += ParticleSize, Position += Delta )
					{
						if( Positions.Num() >= MaxParticles )
						{
							return;
						}
						Positions.AddItem( Position );
					}
				}
			}
		}
	}
}

#if WITH_APEX
class NxApexScene* FRBPhysScene::GetApexScene(void)
{
	NxApexScene *ret = 0;
	if ( ApexScene )
	{ 
		ret = ApexScene->GetApexScene();
	}
	return ret;
}
#endif

#if WITH_APEX
FMatrix N2UTransformApex(const physx::PxMat34Legacy& nTM)
{
	FMatrix Result;

	// Copy rotation
	NxF32 Entries[9];
	nTM.M.getColumnMajor(Entries);

	Result.M[0][0] = Entries[0];
	Result.M[0][1] = Entries[1];
	Result.M[0][2] = Entries[2];

	Result.M[1][0] = Entries[3];
	Result.M[1][1] = Entries[4];
	Result.M[1][2] = Entries[5];

	Result.M[2][0] = Entries[6];
	Result.M[2][1] = Entries[7];
	Result.M[2][2] = Entries[8];

	// Copy translation
	Result.M[3][0] = nTM.t.x * P2UScale;
	Result.M[3][1] = nTM.t.y * P2UScale;
	Result.M[3][2] = nTM.t.z * P2UScale;

	// Fix fourth column
	Result.M[0][3] = Result.M[1][3] = Result.M[2][3] = 0.0f;
	Result.M[3][3] = 1.0f;

	return Result;
}

FMatrix N2UTransformApex(const physx::PxMat33& nTM)
{
	FMatrix Result;

	// Copy rotation
	const physx::PxF32 *Entries = &nTM.column0.x;

	Result.M[0][0] = Entries[0];
	Result.M[0][1] = Entries[1];
	Result.M[0][2] = Entries[2];

	Result.M[1][0] = Entries[3];
	Result.M[1][1] = Entries[4];
	Result.M[1][2] = Entries[5];

	Result.M[2][0] = Entries[6];
	Result.M[2][1] = Entries[7];
	Result.M[2][2] = Entries[8];

	// Copy translation
	Result.M[3][0] = 0;
	Result.M[3][1] = 0;
	Result.M[3][2] = 0;

	// Fix fourth column
	Result.M[0][3] = Result.M[1][3] = Result.M[2][3] = 0.0f;
	Result.M[3][3] = 1.0f;

	return Result;
}

void N2UTransformApexRotationOnly(const physx::PxMat33& nTM,FMatrix &Result)
{
	// Copy rotation
	const physx::PxF32 *Entries = &nTM.column0.x;

	Result.M[0][0] = Entries[0];
	Result.M[0][1] = Entries[1];
	Result.M[0][2] = Entries[2];

	Result.M[1][0] = Entries[3];
	Result.M[1][1] = Entries[4];
	Result.M[1][2] = Entries[5];

	Result.M[2][0] = Entries[6];
	Result.M[2][1] = Entries[7];
	Result.M[2][2] = Entries[8];

	// Fix fourth column
	Result.M[0][3] = Result.M[1][3] = Result.M[2][3] = 0.0f;
	Result.M[3][3] = 1.0f;

}


FVector N2UVectorCopyApex(const physx::PxVec3& nVec)
{
	return FVector(nVec.x, nVec.y, nVec.z);
}

FVector N2UPositionApex(const physx::PxVec3& nVec)
{
	return FVector(nVec.x * P2UScale, nVec.y * P2UScale, nVec.z * P2UScale);
}

FQuat N2UQuaternionApex(const physx::PxQuat& nQuat)
{
	return FQuat(nQuat.x, nQuat.y, nQuat.z, nQuat.w);
}

physx::PxMat34Legacy U2NMatrixCopyApex(const FMatrix& uTM)
{
	physx::PxMat34Legacy Result;

	// Copy rotation
	physx::PxF32 Entries[9];
	Entries[0] = uTM.M[0][0];
	Entries[1] = uTM.M[0][1];
	Entries[2] = uTM.M[0][2];

	Entries[3] = uTM.M[1][0];
	Entries[4] = uTM.M[1][1];
	Entries[5] = uTM.M[1][2];

	Entries[6] = uTM.M[2][0];
	Entries[7] = uTM.M[2][1];
	Entries[8] = uTM.M[2][2];

	Result.M.setColumnMajor(Entries);

	// Copy translation
	Result.t.x = uTM.M[3][0];
	Result.t.y = uTM.M[3][1];
	Result.t.z = uTM.M[3][2];

	return Result;
}

physx::PxMat34Legacy U2NTransformApex(const FMatrix& uTM)
{
	physx::PxMat34Legacy Result;

	// Copy rotation
	physx::PxF32 Entries[9];
	Entries[0] = uTM.M[0][0];
	Entries[1] = uTM.M[0][1];
	Entries[2] = uTM.M[0][2];

	Entries[3] = uTM.M[1][0];
	Entries[4] = uTM.M[1][1];
	Entries[5] = uTM.M[1][2];

	Entries[6] = uTM.M[2][0];
	Entries[7] = uTM.M[2][1];
	Entries[8] = uTM.M[2][2];

	Result.M.setColumnMajor(Entries);

	// Copy translation
	Result.t.x = uTM.M[3][0] * U2PScale;
	Result.t.y = uTM.M[3][1] * U2PScale;
	Result.t.z = uTM.M[3][2] * U2PScale;

	return Result;
}


physx::PxVec3 U2NVectorCopyApex(const FVector& uVec)
{
	return physx::PxVec3(uVec.X, uVec.Y, uVec.Z);
}

physx::PxVec3 U2NPositionApex(const FVector& uPos)
{
	return physx::PxVec3(uPos.X * U2PScale, uPos.Y * U2PScale, uPos.Z * U2PScale);
}

physx::PxQuat U2NQuaternionApex(const FQuat& uQuat)
{
	return physx::PxQuat( uQuat.X, uQuat.Y, uQuat.Z, uQuat.W );
}

UBOOL MatricesAreEqual(const physx::PxMat44& M1, const physx::PxMat44& M2, FLOAT Tolerance)
{
	// Each row
	for(INT i=0; i<4; i++)
	{
		// Each column
		for(INT j=0; j<4; j++)
		{
			if( Abs(M1(i,j) - M2(i,j)) > Tolerance )
			{
				return FALSE;
			}
		}
	}

	// No differences found
	return TRUE;
}
#endif


#endif // WITH_NOVODEX
