/*=============================================================================
	UnNovodexSupport.h: Novodex support
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef _INC_UNNOVODEXSUPPORT
#define _INC_UNNOVODEXSUPPORT

#if WITH_NOVODEX

class NxGroupsMask;
NxGroupsMask CreateGroupsMask(BYTE Channel, FRBCollisionChannelContainer* CollidesChannels);
 
#define NOMINMAX

// When compiling on XBox, PS3, IPHONE or ANDROID, force static PhysX libs.
#if defined(XBOX) || PS3 || IPHONE || ANDROID || PLATFORM_MACOSX || NGP
#define NX_USE_SDK_STATICLIBS
#endif // XBOX || PS3 || IPHONE || ANDROID || PLATFORM_MACOSX

// Double buffering currently only on PC
#if defined(WIN32) && !defined(_WIN64)
#define SUPPORT_DOUBLE_BUFFERING 1
#else
#define SUPPORT_DOUBLE_BUFFERING 0
#endif

/** 
 *	This define will link in the Debug libraries for Novodex.
 *	It only applies when building _DEBUG.
 *	You will also have to update the SetUpPhysXEnvironment() function
 *	in UE3BuildExternal.cs in order to add the correct PhysXLoader dll
 *	to the delay load list!
 */
//#define	USE_DEBUG_NOVODEX


// Temporarily disable a few warnings due to virtual function abuse in PhysX source files
#pragma warning( push )
#pragma warning( disable : 4263 ) // 'function' : member function does not override any base class virtual member function
#pragma warning( disable : 4264 ) // 'virtual_function' : no override available for virtual member function from base 'class'; function is hidden

// 'check' is used as local var in some PhysX funcs in headers. This causes problems in Shipping buildings,
// so we undefine it here, and restore it after including the headers
#if !defined(IPHONE) && !defined(ANDROID) && !PLATFORM_MACOSX		// #pragma push_macro doesn't work in some compilers
	#pragma push_macro("check")
#endif
#undef check

#if PLATFORM_MACOSX
using namespace std;
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack( push, 8 )
#endif
#include "NxFoundation.h"
#include "NxStream.h"
#include "NxPhysics.h"
#include "NxCooking.h"
#include "NxSceneQuery.h"
#include "NxExtensions.h"
#include "NxExtensionQuickLoad.h"
#ifndef NX_DISABLE_FLUIDS
	#include "fluids/NxFluid.h"
    #include "fluids/NxParticleData.h"
	#include "fluids/NxFluidEmitterDesc.h"	
#endif
#if SUPPORT_DOUBLE_BUFFERING
#include "NxdScene.h"
#endif

#if WITH_APEX
#include <NxApex.h>
#include <NxApexSDK.h>
#include <foundation/PxQuat.h>
#include <foundation/PxVec3.h>
#include <foundation/PxBounds3.h>
#include <PxMat34Legacy.h>
#endif

#if !defined(IPHONE) && !defined(ANDROID) && !PLATFORM_MACOSX && !defined(WIIU)
	#pragma pop_macro("check")
#else
	#if DO_CHECK
		#define check(expr)				{ if(!(expr)) appFailAssert( #expr, __FILE__, __LINE__ ); ANALYSIS_ASSUME(expr); }
	#else
		#define check					__noop
	#endif
#endif

#if SUPPORTS_PRAGMA_PACK
#pragma pack( pop )
#endif

#pragma warning( pop )


// Unreal to Novodex conversion
NxMat34 U2NMatrixCopy(const FMatrix& uTM);
NxMat34 U2NTransform(const FMatrix& uTM);
NxVec3 U2NVectorCopy(const FVector& uVec);
NxVec3 U2NPosition(const FVector& uVec);
NxQuat U2NQuaternion(const FQuat& uQuat);
FVector N2UVectorCopy(const NxVec3& nVec);
FVector N2UPosition(const NxVec3& nVec);
FQuat	N2UQuaternion(const NxQuat& nQuat);
// Novodex to Unreal conversion
FMatrix N2UTransform(const NxMat34& nTM);

#if WITH_APEX
physx::PxMat34Legacy U2NMatrixCopyApex(const FMatrix& uTM);
physx::PxMat34Legacy U2NTransformApex(const FMatrix& uTM);
physx::PxVec3 U2NVectorCopyApex(const FVector& uVec);
physx::PxVec3 U2NPositionApex(const FVector& uVec);
physx::PxQuat U2NQuaternionApex(const FQuat& uQuat);
FVector N2UVectorCopyApex(const physx::PxVec3& nVec);
FVector N2UPositionApex(const physx::PxVec3& nVec);
FQuat	N2UQuaternionApex(const physx::PxQuat& nQuat);
FMatrix N2UTransformApex(const physx::PxMat34Legacy& nTM);
FMatrix N2UTransformApex(const physx::PxMat33& nTM);
void N2UTransformApexRotationOnly(const physx::PxMat33& nTM,FMatrix &dest);
UBOOL	MatricesAreEqual(const physx::PxMat44& M1, const physx::PxMat44& M2, FLOAT Tolerance);
#endif


#define PHYSICS_DEBUG 1
#define PHYSICS_DEBUG_MAGNITUDE_THRESHOLD 10000
inline void setLinearVelocity(NxActor* nActor, const NxVec3 &newVel)
{
	#if !FINAL_RELEASE && PHYSICS_DEBUG
		if(newVel.magnitude() > PHYSICS_DEBUG_MAGNITUDE_THRESHOLD)
		{
			warnf(TEXT("Setting Linear Velocity to a large magnitude. Setting to: %f %f %f"),newVel[0],newVel[1],newVel[2]);
		}
	#endif

	if(	appIsNaN(newVel.x) || !appIsFinite(newVel.x) || 
		appIsNaN(newVel.y) || !appIsFinite(newVel.y) || 
		appIsNaN(newVel.z) || !appIsFinite(newVel.z) )
	{
		debugf(TEXT("setLinearVelocity: NaN or Infinite velocity!"));
		return;
	}

#if 0
	NxVec3 Pos = nActor->getGlobalPosition();
	GWorld->PersistentLineBatcher->DrawLine(N2UPosition(Pos), N2UPosition(Pos) + 100.f*N2UPosition(newVel), FColor(255,0,0), SDPG_World);

	if(N2UVectorCopy(newVel).Size() > 1.f)
	{
		debugf(TEXT("BIGVEL"));
	}
#endif 

	nActor->setLinearVelocity(newVel);
}
inline void addForce(NxActor* nActor, const NxVec3 &force, NxForceMode mode=NX_FORCE, bool bWakeUp=true)
{
	
	#if !FINAL_RELEASE && PHYSICS_DEBUG
		if(force.magnitude() > PHYSICS_DEBUG_MAGNITUDE_THRESHOLD)
		{
			warnf(TEXT("Adding a large force ( %f %f %f ) to an object."),force[0],force[1],force[2]);
		}
	#endif

	if(	appIsNaN(force.x) || !appIsFinite(force.x) || 
		appIsNaN(force.y) || !appIsFinite(force.y) || 
		appIsNaN(force.z) || !appIsFinite(force.z) )
	{
		debugf(TEXT("addForce: NaN or Infinite force!"));
		return;
	}

#if 0
	NxVec3 Pos = nActor->getGlobalPosition();
	GWorld->PersistentLineBatcher->DrawLine(N2UPosition(Pos), N2UPosition(Pos) + 100.f*N2UPosition(force), FColor(255,255,0), SDPG_World);
#endif 

	nActor->addForce(force,mode,bWakeUp);
}

/** Global pointer to Novodex SDK objects. */
#include "NvApexManager.h"
/** Global pointer to APEX SDK objects. */
extern NxPhysicsSDK*			GNovodexSDK;
extern NxExtensionQuickLoad*	GNovodeXQuickLoad;
#if WITH_PHYSX_COOKING
extern NxCookingInterface*		GNovodexCooking;
#endif

/** Entry into physics scene map for holding pointer to software and possibly hardware scene. */
struct NxScenePair
{
	NxScenePair() 
	{ 
		PrimaryScene = NULL;
		RigidBodyCompartment = NULL;
		FluidCompartment = NULL;
		ClothCompartment = NULL;
		SoftBodyCompartment = NULL;
	}

	NxScene       *PrimaryScene;
	NxCompartment *RigidBodyCompartment;
	NxCompartment *FluidCompartment;
	NxCompartment *ClothCompartment;
	NxCompartment *SoftBodyCompartment;
};

/** Structures for new emitter code path. */
struct PhysXParticle
{
	NxVec3 Pos;
	NxVec3 Vel;
	NxF32  Density;
	NxU32  Id;
	NxU32  _Pad0;
};

struct PhysXParticleEx
{
	class FPhysXParticleSet* PSet;
	NxU16 RenderIndex;
	NxU16 Index;
};

struct PhysXParticleFlagUpdate
{
	NxU32 Id;
	NxU32 Flags;
};

struct PhysXParticleForceUpdate
{
	NxU32	Id;
	NxVec3	Vec;
};


/** 
 *	Map from SceneIndex to actual NxScene. This indirection allows us to set it to null when we kill the scene, 
 *	and therefore abort trying to destroy Novodex objects after the scene has been destroyed (eg. on game exit). 
 */
extern TMap<INT, NxScenePair>	GNovodexSceneMap;	// hardware scene support - using NxScenePair


/** 
 *	Array of Actors waiting to be cleaned up.
 *	Normally this is done right away, but just in case it tries to happen while DuringAsync is going on, they are deferred to here.
 */
extern TArray<NxActor*>			GNovodexPendingKillActor;
extern TArray<NxJoint*>			GNovodexPendingKillJoint;

/** 
 *	Array of NxConvexMesh objects which are awaiting cleaning up.
 *	This needs to be deferred, as you can't destroy them until all uses of the mesh have been destroyed, and GC guarantees no order.
 */
extern TArray<NxConvexMesh*>	GNovodexPendingKillConvex;

/** 
 *	Array of NxTriangleMesh objects which are awaiting cleaning up.
 *	This needs to be deferred, as you can't destroy them until all uses of the mesh have been destroyed, and GC guarantees no order.
 */
extern TArray<NxTriangleMesh*>	GNovodexPendingKillTriMesh;

/** Array of heightfield objects to be cleaned up. */
extern TArray<NxHeightField*>	GNovodexPendingKillHeightfield;

/**
 *	Array of NxCCDSkeleton objects which are awaiting cleaning up.
 *	This needs to be deferred, as you can't destroy them until all uses of the mesh have been destroyed, and GC guarantees no order.
 */
extern TArray<NxCCDSkeleton*>	GNovodexPendingKillCCDSkeletons;

/**
 *	Array of UserForceFields that are ready for destruction.
 */
extern TArray<class UserForceField*>				GNovodexPendingKillForceFields;

/**
 *	Array of UserForceFieldLinearKernel that are ready for destruction.
 */
extern TArray<class UserForceFieldLinearKernel*>	GNovodexPendingKillForceFieldLinearKernels;

/**
 *	Array of UserForceFieldShapeGroup that are ready for destruction.
 */
extern TArray<class UserForceFieldShapeGroup* >		GNovodexPendingKillForceFieldShapeGroups;



#if !NX_DISABLE_CLOTH
/** 
 *	Array of ForceFields waiting to be cleaned up.
 *	Normally this is done right away, but just in case it tries to happen while DuringAsync is going on, they are deferred to here.
 */
extern TArray<NxCloth*>			GNovodexPendingKillCloths;

/** 
*	Array of NxClothMesh objects which are awaiting cleaning up.
*	This needs to be deferred, as you can't destroy them until all uses of the mesh have been destroyed, and GC guarantees no order.
*/
extern TArray<NxClothMesh*>		GNovodexPendingKillClothMesh;
#endif // !NX_DISABLE_CLOTH
#if !NX_DISABLE_SOFTBODY
/** 
*	Array of NxSoftBodyMesh objects which are awaiting cleaning up.
*	This needs to be deferred, as you can't destroy them until all uses of the mesh have been destroyed, and GC guarantees no order.
*/
extern TArray<NxSoftBodyMesh*>		GNovodexPendingKillSoftBodyMesh;
#endif // !NX_DISABLE_SOFTBODY

/** Total number of PhysX convex meshes around currently. */
extern INT						GNumPhysXConvexMeshes;

/** Total number of PhysX triangle meshes around currently. */
extern INT						GNumPhysXTriMeshes;

#define		UNX_GROUP_DEFAULT			(0)
#define		UNX_GROUP_NOTIFYCOLLIDE		(2)
#define		UNX_GROUP_MODIFYCONTACT		(3)
#define		UNX_GROUP_THRESHOLD_NOTIFY	(4)


/** Shape groups */
#define		SHAPE_GROUP_DEFAULT			(0)
#define		SHAPE_GROUP_LANDSCAPE		(1)


#define		PERF_SHOW_PHYS_INIT_COSTS		LOOKING_FOR_PERF_ISSUES
//#define		SHOW_SLOW_CONVEX

// Some platforms do not yet support PhysX extensions like QuickLoadConvex
#if IPHONE || ANDROID || WIIU || NGP || FLASH
	#define		USE_QUICKLOAD_CONVEX	0
#else
	#define		USE_QUICKLOAD_CONVEX	1
#endif
//#define		ENABLE_CCD

/** Number of frames to wait before releasing a mesh - to avoid any problems where it is still being used. */
const INT DelayNxMeshDestruction = 2;

/** Allows you to force data to be re-cooked on load, instead of using cooked data in packages. */
const UBOOL bUsePrecookedPhysData = TRUE;

/** Loads BSP as convex hulls instead of tri-meshes. */
const UBOOL bUseConvexBSP = FALSE;

/** Util for seeing if PhysX hardware is present in this computer. */
UBOOL IsPhysXHardwarePresent();

/** Get a pointer to the scene-container struct holding software/hardware NxScene pointers. */
NxScenePair* GetNovodexScenePairFromIndex(INT InNovodexSceneIndex);

/** Get the Primary Scene for a given scen pair index. */
NxScene* GetNovodexPrimarySceneFromIndex(INT InNovodexSceneIndex);

/** Wait for a novodex scene to finish all simulations. */
void WaitForNovodexScene(NxScene &nScene);

/** Wait for all novodex scenes to finish all simulations. */
void WaitForAllNovodexScenes();

/** PhysX Contact notify implementation */
class FNxContactReport : public NxUserContactReport
{
public:
	virtual void  onContactNotify(NxContactPair& pair, NxU32 events);
};

#if SUPPORT_DOUBLE_BUFFERING
class FNxdDoubleBufferReport : public NxdDoubleBufferReport
{
public:
	void actorInitializationFailed(NxActor *actor);
	void jointInitializationFailed(NxJoint *joint);
	void springAndDamperEffectorInitializationFailed(NxSpringAndDamperEffector *sadEffector);
	void materialInitializationFailed(NxMaterial *material);
};
#endif

/** Other PhysX notification implementation */
class FNxNotify : public NxUserNotify
{
public:
	virtual bool onJointBreak(NxReal breakingForce, NxJoint& brokenJoint);
	virtual void onWake(NxActor** actors, NxU32 count) {}
	virtual void onSleep(NxActor** actors, NxU32 count) {}
};

/** PhysX contact modification callback. */
class FNxModifyContact : public NxUserContactModify
{
public:
	virtual bool onContactConstraint(
		NxU32& changeFlags, 
		const NxShape* shape0, 
		const NxShape* shape1, 
		const NxU32 featureIndex0, 
		const NxU32 featureIndex1,
		NxContactCallbackData& data);
};

/** Utility wrapper for a BYTE TArray for loading/saving from Novodex. */
class FNxMemoryBuffer : public NxStream
{
public:
	TArray<BYTE>			*Data;
	mutable UINT			ReadPos;

	FNxMemoryBuffer(TArray<BYTE> *InData)
	{
		ReadPos = 0;
		Data = InData;
	}

	// Loading API
	virtual		NxU8			readByte()								const;
	virtual		NxU16			readWord()								const;
	virtual		NxU32			readDword()								const;
	virtual		NxF32			readFloat()								const;
	virtual		NxF64			readDouble()							const;
	virtual		void			readBuffer(void* buffer, NxU32 size)	const;

	// Saving API
	virtual		NxStream&		storeByte(NxU8 b);
	virtual		NxStream&		storeWord(NxU16 w);
	virtual		NxStream&		storeDword(NxU32 d);
	virtual		NxStream&		storeFloat(NxF32 f);
	virtual		NxStream&		storeDouble(NxF64 f);
	virtual		NxStream&		storeBuffer(const void* buffer, NxU32 size);
};



/** Utility for creating a PhysX 'groups mask' for filtering collision from the Unreal filtering info. */
NxGroupsMask CreateGroupsMask(BYTE Channel, FRBCollisionChannelContainer* CollidesChannels);

/** Utility for setting the material on all shapes that make up this NxActor physics object. */
void SetNxActorMaterial(NxActor* nActor, NxMaterialIndex NewMaterial, const UPhysicalMaterial* PhysMat);


/** Utility for comparing two Novodex matrices, to see if they are equal (to some supplied tolerance). */
UBOOL	MatricesAreEqual(const NxMat34& M1, const NxMat34& M2, FLOAT Tolerance);


/** Util for creating simple, collisionless kinematic actor, usually for creating springs to and animating etc. */
NxActor* CreateDummyKinActor(NxScene* NovodexScene, const FMatrix& ActorTM);

/** Util to destroy an actor created with the code above. */
void DestroyDummyKinActor(NxActor* KinActor);


// Utils
UBOOL RepresentConvexAsBox( NxActorDesc* ActorDesc, NxConvexMesh* ConvexMesh, UBOOL bCreateCCDSkel );
void MakeCCDSkelForSphere(NxSphereShapeDesc* SphereDesc);
void MakeCCDSkelForBox(NxBoxShapeDesc* BoxDesc);
void MakeCCDSkelForSphyl(NxCapsuleShapeDesc* SphylDesc);
void MakeCCDSkelForConvex(NxConvexShapeDesc* ConvexDesc);

/** Util to force-set the ref count of an NxTriangleMesh */
void SetNxTriMeshRefCount( NxTriangleMesh * tm, int refCount );

/** Util to force-set the ref count of an NxConvexMesh */
void SetNxConvexMeshRefCount( NxConvexMesh * cm, int refCount );

/** Add a force to an NxActor, but do not modify its waking state. */
void AddForceNoWake(NxActor *Actor, const NxVec3 &force);

/** Add a force to an NxActor, but only if the force is non-zero. */
void AddForceZeroCheck(NxActor *Actor, const NxVec3 &force);

/** Perform a cloth line check, uses an approximate obb for non zero extent checks. */
UBOOL ClothLineCheck(
				USkeletalMeshComponent* SkelComp,
				FCheckResult &Result,
                const FVector& End,
                const FVector& Start,
                const FVector& Extent,
				DWORD TraceFlags);

/** Perform a point check against a cloth(uses an approximate OBB) */
UBOOL ClothPointCheck(FCheckResult &Result, USkeletalMeshComponent* SkelComp, const FVector& Location, const FVector& Extent);

/** Perform a soft body line check. */
UBOOL SoftBodyLineCheck(
					 USkeletalMeshComponent* SkelComp,
					 FCheckResult &Result,
					 const FVector& End,
					 const FVector& Start,
					 const FVector& Extent,
					 DWORD TraceFlags);

DWORD FindNovodexSceneStat(NxScene* NovodexScene, const TCHAR* StatNxName, UBOOL bMaxValue);

/** Scan FKAggregateGeom to discretize volume.  Scan is done across minimum AABB area.
	Positions array is appended, not initialized, by this function.
*/
void ScanGeom( FKAggregateGeom & Geom, const FMatrix & GeomLocalToWorld, const FVector & Scale3D, FLOAT ParticleSize, INT MaxParticles, TArray<FVector> & Positions );

#endif

#endif
