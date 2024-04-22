/*=============================================================================
	NvApexManager.cpp : Handles creating and destroying of the APEX SDK and modules
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

#define WITH_APEX_FRAMEWORK 1

#if WITH_NOVODEX

#include "NvApexManager.h"
#include "UnNovodexSupport.h"

#if WITH_APEX

#ifdef PX_WINDOWS
#define WITH_CUDA_CONTEXT 0
#else
#define WITH_CUDA_CONTEXT 0
#endif

//#define USE_DEBUG_APEX        // Declare this pre-processor define to link against the debug APEX libraries.
//#define USE_DEBUG_EDITOR_WIDGETS // Declare this pre-processor define if you want to dynamically load the debug version of the Editor Widgets DLL

#include "NvApexCommands.h"
#include "NvApexScene.h"
#include "EngineMeshClasses.h"

FIApexManager *GApexManager=0;

#pragma pack(push,8)
#include <foundation/Px.h>
#include <foundation/PxAllocatorCallback.h>
#include <foundation/PxErrorCallback.h>
#include <NxApexSDK.h>
#include <NxUserRenderResourceManager.h>
#include <NxResourceCallback.h>
#include <NxModuleDestructible.h>
#include <NxModuleClothing.h>
#include <NxDestructibleActor.h>
#include <NxApexUtils.h>
#include <NxFromPx.h>
#include <pxtask/PxCudaContextManager.h>
#include <pxtask/PxTaskManager.h>
#pragma pack(pop)
#define USE_LEGACY_MODULES	(_WINDOWS && !FINAL_RELEASE)


using namespace physx::apex;


#ifdef PX_WINDOWS

#if WITH_APEX_PARTICLES
#include "PxEditorPanel.h"
#include "PxEditorPanelDesc.h"
#include "PxEditorWidgetManager.h"
#include "PxGenericPropertiesEditorPanel.h"
#endif

#ifdef USE_DEBUG_APEX

#ifdef PX_X64
#pragma comment(lib,"ApexFrameworkDEBUG_x64.lib")
#else
#pragma comment(lib,"ApexFrameworkDEBUG_x86.lib")
#endif

#else

#ifdef PX_X64

#if WITH_APEX_SHIPPING
#pragma comment(lib,"ApexFramework_x64.lib")
#else
#pragma comment(lib,"ApexFrameworkCHECKED_x64.lib")
#endif

#else

#if WITH_APEX_SHIPPING
#pragma comment(lib,"ApexFramework_x86.lib")
#else
#pragma comment(lib,"ApexFrameworkCHECKED_x86.lib")
#endif

#endif

#endif

#endif // PX_WINDOWS

#ifdef PX_X360

#ifdef USE_DEBUG_APEX

#pragma comment(lib,"APEX_ClothingDEBUG.lib")
#pragma comment(lib,"APEX_DestructibleDEBUG.lib")
#pragma comment(lib,"ApexCommonDEBUG.lib")
#pragma comment(lib,"ApexSharedDEBUG.lib")
#pragma comment(lib,"ApexFrameworkDEBUG.lib")

#else

#if WITH_APEX_SHIPPING

#pragma comment(lib,"APEX_Clothing.lib")
#pragma comment(lib,"APEX_Destructible.lib")
#pragma comment(lib,"ApexCommon.lib")
#pragma comment(lib,"ApexShared.lib")
#pragma comment(lib,"ApexFramework.lib")

#else

#pragma comment(lib,"APEX_ClothingCHECKED.lib")
#pragma comment(lib,"APEX_DestructibleCHECKED.lib")
#pragma comment(lib,"ApexCommonCHECKED.lib")
#pragma comment(lib,"ApexSharedCHECKED.lib")
#pragma comment(lib,"ApexFrameworkCHECKED.lib")

#endif

#endif

#endif // PX_X360

#define DEFAULT_APEX_GUID	"3BF05740-AC62-4044-9B83-1CF0A51901B4"

class FNxUserChunkReport : public NxUserChunkReport
{
public:
	void onDamageNotify( const NxApexDamageEventReportData& damageEvent );
};

class FApexManager : public FIApexManager, public physx::PxAllocatorCallback, public NxUserRenderResourceManager, public physx::PxErrorCallback, public NxResourceCallback
{
public:

  FApexManager(NxPhysicsSDK *sdk,NxCookingInterface *cooking)
  {
	MImportFileName[0] = 0;
	GApexManager = this;
	MPhysXSDK = sdk;
	MModuleDestructible = NULL;
	MModuleClothing = NULL;
#if USE_LEGACY_MODULES
	MModuleClothingLegacy = NULL;
	MModuleDestructibleLegacy = NULL;
	MModuleFrameworkLegacy = NULL;
	MModuleCommonLegacy	= NULL;
#endif
	MApexLODResourceBudget = FLT_MAX;
	MApexSDK = NULL;

#if WITH_APEX_PARTICLES
#ifdef PX_WINDOWS
	mEditorWidgetManager = NULL;
#endif
#endif

#if WITH_APEX_FRAMEWORK
	  /* Fill out the Apex SDK descriptor */
	  NxApexSDKDesc apexDesc;

	  /* Apex needs an allocation and error stream.  Here we'll use the same ones that PhysX uses */
	  apexDesc.allocator = this;
	  apexDesc.outputStream = this;

	  /* Let Apex know about our PhysX SDK and cooking library */
	  apexDesc.physXSDK = MPhysXSDK;
	  apexDesc.cooking = cooking;

	  /* Our custom render resource manager */
	  apexDesc.renderResourceManager = this;

	  /* Our custom named resource handler */
	  apexDesc.resourceCallback = this;

	  /* Application-specific GUID. Only affects Windows. */
	  /* If you have changed the APEX DLLs your application needs its own GUID. */
	  apexDesc.appGuid = DEFAULT_APEX_GUID;

	  /* Finally, create the Apex SDK */
	  NxApexCreateError errorCode;
	  MApexSDK = NxCreateApexSDK(apexDesc, &errorCode);
	  if ( MApexSDK == NULL )
	  {
		  debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the ApexSDK: error code: %d"),(INT)(errorCode));
	  }
#endif
	if ( MApexSDK )
	{
#if WITH_CUDA_CONTEXT
		physx::pxtask::CudaContextManagerDesc ctxMgrDesc;
		mCtxMgr = MApexSDK->createCudaContextManager(ctxMgrDesc);
#endif

#if WITH_APEX_DESTRUCTIBLE

#ifndef PX_WINDOWS
		instantiateModuleDestructible();
#endif // PX_WINDOWS
		MModuleDestructible = static_cast<NxModuleDestructible*>(MApexSDK->createModule("Destructible"));
		if ( MModuleDestructible )
		{
			NxParameterized::Interface* params = MModuleDestructible->getDefaultModuleDesc();
#if WITH_APEX_GRB
			//Select whether to use GPU rigid bodies or CPU rigid bodies
			UBOOL bDisablePhysXHardware = FALSE;
			UBOOL bEnableGRB = TRUE;
			verify(GConfig->GetBool(TEXT("Engine.Engine"),	TEXT("bDisablePhysXHardwareSupport"	), bDisablePhysXHardware, GEngineIni));
			verify(GConfig->GetBool(TEXT("SystemSettings"),	TEXT("ApexGRBEnable"), bEnableGRB, GSystemSettingsIni));
			if(!bDisablePhysXHardware && bEnableGRB)
			{
				//Find a suitable GPU. If it can't find one, it will use the CPU
				NxParameterized::setParamI32(*params, "gpuRigidBodySettings.gpuDeviceOrdinal", -2);

				NxParameterized::setParamU32(*params,"gpuRigidBodySettings.gpuMemSceneSize", GSystemSettings.ApexGRBGpuMemSceneSize);
				NxParameterized::setParamU32(*params,"gpuRigidBodySettings.gpuMemTempDataSize",GSystemSettings.ApexGRBGpuMemTempDataSize);
				NxParameterized::setParamU32(*params,"gpuRigidBodySettings.nonPenSolverPosIterCount", GSystemSettings.ApexGRBNonPenSolverPosIterCount);
				NxParameterized::setParamU32(*params,"gpuRigidBodySettings.frictionSolverPosIterCount", GSystemSettings.ApexGRBFrictionSolverPosIterCount);
				NxParameterized::setParamU32(*params,"gpuRigidBodySettings.frictionSolverVelIterCount", GSystemSettings.ApexGRBFrictionSolverVelIterCount);

				NxParameterized::setParamF32(*params, "gpuRigidBodySettings.meshCellSize", GSystemSettings.ApexGRBMeshCellSize);
				NxParameterized::setParamF32(*params, "gpuRigidBodySettings.skinWidth", GSystemSettings.ApexGRBSkinWidth);
				NxParameterized::setParamF32(*params, "gpuRigidBodySettings.maxLinAcceleration", GSystemSettings.ApexGRBMaxLinAcceleration);
			}
			else
			{
				NxParameterized::setParamI32(*params, "gpuRigidBodySettings.gpuDeviceOrdinal", -1);	
			}
#endif
			MModuleDestructible->init(*params);
			MModuleDestructible->setChunkReport(&MNxUserChunkReport);
			MModuleDestructible->setChunkReportBitMask(physx::NxApexChunkFlag::FRACTURED);
			MModuleDestructible->setMaxDynamicChunkIslandCount(GSystemSettings.ApexDestructionMaxChunkIslandCount);
			MModuleDestructible->setMaxChunkCount(GSystemSettings.ApexDestructionMaxShapeCount);
			MModuleDestructible->setMaxChunkSeparationLOD(GSystemSettings.ApexDestructionMaxChunkSeparationLOD);
			MModuleDestructible->setSortByBenefit(GSystemSettings.bApexDestructionSortByBenefit? true : false);
			MModuleDestructible->setMaxActorCreatesPerFrame(GSystemSettings.ApexDestructionMaxActorCreatesPerFrame);
			MModuleDestructible->setMaxFracturesProcessedPerFrame(GSystemSettings.ApexDestructionMaxFracturesProcessedPerFrame);
		}
		else
		{
			debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the Apex Destructible Module"));
		}
#endif // WITH_APEX_DESTRUCTIBLE
#if WITH_APEX_CLOTHING

#ifndef PX_WINDOWS
		instantiateModuleClothing();
#endif // PX_WINDOWS

		MModuleClothing  = static_cast<NxModuleClothing*>(MApexSDK->createModule("Clothing"));
		if ( MModuleClothing )
		{
			NxParameterized::Interface* params = MModuleClothing->getDefaultModuleDesc();
#ifndef PX_WINDOWS // On non-Windows platforms the maximum number of compartments should be set to zero due to compartment performance on consoles.
			NxParameterized::setParamU32(*params, "maxNumCompartments", 0);
#endif
			//debugf(TEXT("--- bEnableParallelApexClothingFetch %d"), GSystemSettings.bEnableParallelApexClothingFetch);
			if(GSystemSettings.bEnableParallelApexClothingFetch)
			{
				NxParameterized::setParamBool(*params, "parallelizeFetchResults", true);
			}
			NxParameterized::setParamBool(*params, "allowAsyncCooking", GSystemSettings.bApexClothingAllowAsyncCooking? true:false);
			NxParameterized::setParamBool(*params, "allowApexWorkBetweenSubsteps", GSystemSettings.bApexClothingAllowApexWorkBetweenSubsteps? true:false);
			NxParameterized::setParamU32(*params, "avgSimFrequencyWindow", GSystemSettings.ApexClothingAvgSimFrequencyWindow);
			// asyncFetchResults has no effect unless parallelizeFetchResults is also true.
			NxParameterized::setParamBool(*params, "asyncFetchResults", GSystemSettings.bApexClothingAsyncFetchResults ? true : false);
			
			//Reducing clothing LOD weights so they are comparable to destruction and particles
			// MModuleClothing->setLODWeights(0.001f, 1.0f); // 1000 particles uses 1 budget unit? [TSC_MERGE]
			
			// init only after all params set/overridden
			MModuleClothing->init(*params);
		}
		else
		{
			debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the Apex Clothing Module"));
		}

#endif // WITH_APEX_CLOTHING
#if WITH_APEX_PARTICLES

#ifndef PX_WINDOWS
		instantiateModuleParticles();
#endif // PX_WINDOWS

		MModuleParticles  = static_cast<NxModuleParticles*>(MApexSDK->createModule("Particles"));
		if ( !MModuleParticles )
		{
			debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the Apex Particles Module"));
		}
#endif // WITH_APEX_PARTICLES
#if WITH_APEX_EMITTER

#ifndef PX_WINDOWS
		instantiateModuleEmitter();
#endif // PX_WINDOWS

    MModuleEmitter  = static_cast<NxModuleEmitter*>(MApexSDK->createModule("Emitter"));
    if ( !MModuleEmitter )
    {
      debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the Apex Particles Module"));
    }
#endif // WITH_APEX_EMITTER

#if WITH_APEX_IOFX

#ifndef PX_WINDOWS
	instantiateModuleIofx();
#endif // PX_WINDOWS

	MModuleIofx  = static_cast<NxModuleIofx*>(MApexSDK->createModule("IOFX"));
	if ( !MModuleIofx )
	{
		debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the Apex Particles Module"));
	}
#endif // WITH_APEX_IOFX

#if WITH_APEX_BASIC_IOS

#ifndef PX_WINDOWS
	instantiateModuleBasicIos();
#endif // PX_WINDOWS

	mModuleBasicIos = static_cast<NxModuleBasicIos*>(MApexSDK->createModule("BasicIOS"));
	PX_ASSERT(mModuleBasicIos);
	if(mModuleBasicIos)
	{
		mModuleBasicIos->setLODWeights(1.0f, 1.0f);
	}
	else
	{
		debugf(NAME_DevPhysics, TEXT("ApexManager: Failed to create the Apex Basic IOS Module"));
	}
#endif // WITH_APEX_IOS
	}

#if USE_LEGACY_MODULES
	MModuleFrameworkLegacy = MApexSDK->createModule("Framework_Legacy");
	if( MModuleFrameworkLegacy == NULL )
	{
		debugf(NAME_DevPhysics, TEXT("ApexManager: Unable to load legacy Framework module; loading Apex 1.0 assets may fail."));
	}

	MModuleCommonLegacy = MApexSDK->createModule("Common_Legacy");
	if( MModuleCommonLegacy == NULL )
	{
		debugf(NAME_DevPhysics, TEXT("ApexManager: Unable to load legacy common module; loading Apex 1.1 assets may fail."));
	}
	
	MModuleClothingLegacy = MApexSDK->createModule("Clothing_Legacy");
	if( MModuleClothingLegacy == NULL )
	{
		debugf(NAME_DevPhysics, TEXT("ApexManager: Unable to load legacy clothing module; loading old clothing assets may fail."));
	}

	MModuleDestructibleLegacy = MApexSDK->createModule("Destructible_Legacy");
	if( MModuleDestructibleLegacy == NULL )
	{
		debugf(NAME_DevPhysics, TEXT("ApexManager: Unable to load legacy Destructible module; loading old Destructible assets may fail."));
	}
#endif


#if WITH_APEX_PARTICLES
#ifdef PX_WINDOWS
	if ( GIsEditor ) // only load the editor widget manager if we are in editor mode!
	{
		UINT errorMode = SEM_FAILCRITICALERRORS;
		UINT oldErrorMode = SetErrorMode(errorMode);
		const char *dll;
#ifdef PX_X64

#ifdef USE_DEBUG_EDITOR_WIDGETS
		dll = "libapexeditorwidgets_x64_debug.dll";
#else
		dll = "libapexeditorwidgets_x64.dll";
#endif

#else

#ifdef USE_DEBUG_EDITOR_WIDGETS
		dll = "libapexeditorwidgets_x86_debug.dll";
#else
		dll = "libapexeditorwidgets_x86.dll";
#endif

#endif
		HMODULE module = LoadLibraryA(dll);
		SetErrorMode(oldErrorMode);
		if ( module )
		{
			void *proc = GetProcAddress(module,"PxCreateEditorWidgetManager");
			if ( proc )
			{
				typedef physx::PxEditorWidgetManager * (__cdecl * PLUGIN_INTERFACE_LIST_FUNC)(void);
				 mEditorWidgetManager = ((PLUGIN_INTERFACE_LIST_FUNC)proc)();
			}
		}

	}
#endif
#endif
  }

  ~FApexManager(void)
  {
	if ( MModuleDestructible )
	{
		MModuleDestructible->release();
	}
	if ( MModuleClothing )
	{
		MModuleClothing->release();
	}
#if USE_LEGACY_MODULES
	if ( MModuleClothingLegacy )
	{
		MModuleClothingLegacy->release();
		MModuleClothingLegacy = NULL;
	}
	if ( MModuleDestructibleLegacy )
	{
		MModuleDestructibleLegacy->release();
		MModuleDestructibleLegacy = NULL;
	}
	if (MModuleFrameworkLegacy)
	{
		MModuleFrameworkLegacy->release();
		MModuleFrameworkLegacy = NULL;
	}
	if (MModuleCommonLegacy)
	{
		MModuleCommonLegacy->release();
		MModuleCommonLegacy = NULL;
	}
#endif
	if ( MApexSDK )
    {
      MApexSDK->release();
    }
#if WITH_CUDA_CONTEXT
	if ( mCtxMgr )
	{
		mCtxMgr->release();
	}
#endif
  }


  virtual NxApexSDK * GetApexSDK(void) const
  {
    return MApexSDK;
  }

	virtual void* allocate(size_t size, const char *typeName, const char* filename, int line) 
	{
		size_t N = 16;
		size_t pad = N - 1 + sizeof(size_t); // store offset for delete.
		physx::PxU8* base = (physx::PxU8*)appMalloc(size+pad);

		physx::PxU8* ptr = (physx::PxU8*)(size_t(base + pad) & ~(size_t(N) - 1)); // aligned pointer, ensuring N is a size_t wide mask
		((size_t*)ptr)[-1] = ptr - base; // store offset

		return (void*)ptr;
	}

	virtual void* allocate(size_t size, physx::PxU32 handle, const char* filename, int line) 
	{
		return allocate(size, (const char *)NULL, filename, line);
	}

	/**
	\brief Frees memory previously allocated by allocate().

	<b>Threading:</b> This function should be thread safe as it can be called in the context of the user thread 
	and physics processing thread(s).

	\param ptr Memory to free.
	*/
	virtual void deallocate(void* ptr) 
	{
		if(ptr == NULL)
			return;

		physx::PxU8* base = ((physx::PxU8*)ptr) - ((size_t*)ptr)[-1];
		appFree((void*)base);
	}

  // Implement NxUserOuputStream interface
  virtual  void reportError(physx::PxErrorCode::Enum code, const char * message, const char *file, int line) 
  {
	  const char* pErrorText = "";
	  UBOOL isWarning = FALSE;

	  switch (code)
	  {
	    case physx::PxErrorCode::eNO_ERROR:
		    pErrorText = "No Error";
		    isWarning = TRUE;
		    break;
	    case physx::PxErrorCode::eINVALID_PARAMETER:
		    pErrorText = "Invalid Parameter";
		    break;
	    case physx::PxErrorCode::eINVALID_OPERATION:
		    pErrorText = "Invalid Operation";
		    break;
	    case physx::PxErrorCode::eOUT_OF_MEMORY:
		    pErrorText = "Out of Memory";
		    break;
	    case physx::PxErrorCode::eINTERNAL_ERROR:
		    pErrorText = "Internal Error";
		    break;
	    case physx::PxErrorCode::eDEBUG_WARNING:
		    pErrorText = "Debug Warning";
			isWarning = TRUE;
		    break;
	    case physx::PxErrorCode::eDEBUG_INFO:
		    pErrorText = "Debug Info";
		    isWarning = TRUE;
		    break;
	  }
	  if (isWarning)
	  {
		  debugf(NAME_DevPhysics, TEXT("APEX %s: %s"), ANSI_TO_TCHAR(pErrorText), ANSI_TO_TCHAR(message));
	  }
	  else
	  {
		  // Ignore nuisance error message from loading APEX 0.9 assets.
		  if( appStrstr( ANSI_TO_TCHAR(message), TEXT("ClothingMaterialLibrary::ClothingMaterialLibrary: This method is deprecated") ) == NULL )
		  {
			  debugf(NAME_DevPhysics, TEXT("APEX Error: %s: %s"), ANSI_TO_TCHAR(pErrorText), ANSI_TO_TCHAR(message));
		  }
	  }

  }



// Implement the NxUserRenderResourceManager inteface
  virtual NxUserRenderVertexBuffer   *createVertexBuffer( const physx::NxUserRenderVertexBufferDesc &desc )
  {
    return MRenderResourceManager ? MRenderResourceManager->createVertexBuffer(desc) : 0;
  }

  virtual void                        releaseVertexBuffer( NxUserRenderVertexBuffer &buffer )
  {
    MRenderResourceManager->releaseVertexBuffer(buffer);
  }

  virtual NxUserRenderIndexBuffer    *createIndexBuffer( const NxUserRenderIndexBufferDesc &desc )
  {
    return MRenderResourceManager ? MRenderResourceManager->createIndexBuffer(desc) : 0;
  }

  virtual void                        releaseIndexBuffer( NxUserRenderIndexBuffer &buffer )
  {
    MRenderResourceManager->releaseIndexBuffer(buffer);
  }

  virtual NxUserRenderBoneBuffer     *createBoneBuffer( const NxUserRenderBoneBufferDesc &desc )
  {
    return MRenderResourceManager ? MRenderResourceManager->createBoneBuffer(desc) : 0;
  }

  virtual void                        releaseBoneBuffer( NxUserRenderBoneBuffer &buffer )
  {
    MRenderResourceManager->releaseBoneBuffer(buffer);
  }

  virtual NxUserRenderInstanceBuffer *createInstanceBuffer( const NxUserRenderInstanceBufferDesc &desc )
  {
    return MRenderResourceManager ? MRenderResourceManager->createInstanceBuffer(desc) : 0;
  }

  virtual void                        releaseInstanceBuffer( NxUserRenderInstanceBuffer &buffer )
  {
    MRenderResourceManager->releaseInstanceBuffer(buffer);
  }

  virtual NxUserRenderSpriteBuffer   *createSpriteBuffer( const NxUserRenderSpriteBufferDesc &desc )
  {
    return MRenderResourceManager ? MRenderResourceManager->createSpriteBuffer(desc) : 0;
  }

  virtual void                        releaseSpriteBuffer( NxUserRenderSpriteBuffer &buffer )
  {
    MRenderResourceManager->releaseSpriteBuffer(buffer);
  }

  virtual NxUserRenderResource       *createResource( const physx::NxUserRenderResourceDesc &desc )
  {
    return MRenderResourceManager ? MRenderResourceManager->createResource(desc) : 0;
  }

  virtual void                        releaseResource( NxUserRenderResource &resource )
  {
    MRenderResourceManager->releaseResource(resource);
  }

  virtual physx::PxU32                       getMaxBonesForMaterial(void *material)
  {
    return MRenderResourceManager ? MRenderResourceManager->getMaxBonesForMaterial(material) : 0;
  }

  // Implements the NxResourceCallback interface
  virtual void *requestResource(const char *nameSpace, const char *name)
  {
	  return MResourceCallback ? MResourceCallback->requestResource(nameSpace,name) : 0;
  }

  virtual void  releaseResource(const char *nameSpace, const char *name, void *resource)
  {
	  if ( MResourceCallback )
		  MResourceCallback->releaseResource(nameSpace,name,resource);
  }

  virtual NxModuleDestructible * GetModuleDestructible(void) const
  {
	  return MModuleDestructible;
  }

  virtual NxModuleClothing     * GetModuleClothing(void) const
  {
	  return MModuleClothing;
  }
  virtual void SetResourceCallback(NxResourceCallback *callback) 
  {
	  MResourceCallback = callback;
  }

  virtual void SetRenderResourceManager(NxUserRenderResourceManager *manager) 
  {
	  MRenderResourceManager = manager;
  }

	/**
	 * Sets the current default LOD Resource Budget
	 * @param	LODResourceBudget - The default LOD resource budget per scene.
	 **/
	virtual void SetApexLODResourceBudget(FIApexScene* Scene, physx::PxF32 LODResourceBudget) 
	{
		NxApexScene* ApexScene = Scene->GetApexScene();
		if ( ApexScene != NULL )
		{
			ApexScene->setLODResourceBudget( LODResourceBudget );
		}
	}

	virtual void Pump(void)
	{
		if ( GApexCommands )
		{
			GApexCommands->Pump();
		}
	}

		/***
	* Inspects the buffer and figures out what asset format it points to.
	*/
	virtual ApexAssetFormat GetApexAssetFormat(const void *Buffer,physx::PxU32 dlen,physx::PxU32 &numObj) 
	{
		return GApexCommands->GetApexAssetFormat(Buffer,dlen,numObj);
	}


	virtual void SetCurrentImportFileName(const char *fname)
	{
		appStrncpyANSI( MImportFileName, fname, 512 );
	}

	const char * GetCurrentImportFileName() const
	{
		return MImportFileName;
	}

#if !WITH_APEX_SHIPPING
	virtual const char * GetApexAssetBuffer(physx::PxU32 objNumber,const void *Buffer,physx::PxU32 dlen,const void *&buffer,physx::PxU32 &bufferLen)
	{
		return GApexCommands->GetApexAssetBuffer(objNumber,Buffer,dlen,buffer,bufferLen);
	}
#endif

#if WITH_APEX_PARTICLES
	virtual physx::PxEditorWidgetManager * GetPxEditorWidgetManager(void)
	{
		return mEditorWidgetManager;
	}
#endif

		virtual physx::pxtask::CudaContextManager	*		GetCudaContextManager(void) 
		{
#if WITH_CUDA_CONTEXT
			return mCtxMgr;
#else
			return NULL;
#endif
		}

private:
	char						MImportFileName[512];
	NxPhysicsSDK                *MPhysXSDK;
	NxApexSDK                   *MApexSDK;
	NxModuleDestructible        *MModuleDestructible;
	NxModuleClothing            *MModuleClothing;
#if USE_LEGACY_MODULES
	// Modules used for loading older version APEX assets.
	// All APEX assets will be upgraded to the current version during cooking.
	NxModule					*MModuleClothingLegacy;
	NxModule					*MModuleDestructibleLegacy;
	NxModule					*MModuleFrameworkLegacy;
	NxModule					*MModuleCommonLegacy;
#endif
	NxResourceCallback          *MResourceCallback;
	NxUserRenderResourceManager *MRenderResourceManager;
	FNxUserChunkReport		    MNxUserChunkReport;
	physx::PxF32				MApexLODResourceBudget;
#if WITH_APEX_PARTICLES
#ifdef PX_WINDOWS
	physx::PxEditorWidgetManager	*mEditorWidgetManager;
#endif
#endif
#if WITH_CUDA_CONTEXT
	physx::pxtask::CudaContextManager *	mCtxMgr;
#endif
};


FIApexManager * CreateApexManager(NxPhysicsSDK *sdk,
								 NxCookingInterface *cooking)
{
  	FApexManager *am = new FApexManager(sdk,cooking);
	if ( am->GetApexSDK() == NULL )
	{
		delete am;
		am = NULL;
	}
  	return static_cast< FIApexManager *>(am);
}

void           ReleaseApexManager(FIApexManager *iface)
{
  FApexManager *am = static_cast< FApexManager *>(iface);
  delete am;
}


void FNxUserChunkReport::onDamageNotify( const NxApexDamageEventReportData& damageEvent )
{
	AApexDestructibleActor * DestructibleActor = static_cast<AApexDestructibleActor*>(damageEvent.destructible->userData);
	check( DestructibleActor );

	UApexDestructibleAsset* DestructibleAsset = DestructibleActor->StaticDestructibleComponent->Asset ? Cast<UApexDestructibleAsset>(DestructibleActor->StaticDestructibleComponent->Asset) : NULL;
	check( DestructibleAsset );

	const UBOOL bPlaySingleFractureMaterialEffect = DestructibleActor->bFractureMaterialOverride ? DestructibleActor->bPlaySingleFractureMaterialEffect : DestructibleAsset->bPlaySingleFractureMaterialEffect;

	if (!bPlaySingleFractureMaterialEffect)
	{
		for( physx::PxU32 eventN = 0; eventN < damageEvent.fractureEventListSize; ++eventN )
		{
			const NxApexChunkData& ChunkData = damageEvent.fractureEventList[eventN];
			if( ChunkData.flags & NxApexChunkFlag::FRACTURED )
			{
				if( DestructibleActor )
				{
					physx::PxVec3 ChunkLocationN = ChunkData.worldBounds.getCenter();
					FVector ChunkLocation = N2UPosition(NxFromPxVec3Fast(ChunkLocationN));
					FVector HitDirection = N2UVectorCopy(NxFromPxVec3Fast(-damageEvent.hitDirection));
					DestructibleActor->SpawnFractureEffects( ChunkLocation, HitDirection, static_cast<INT>(ChunkData.depth) );
				}
			}
		}
	}
	else
	{
		physx::PxBounds3 AllChunkBounds;
		AllChunkBounds.setEmpty();
		physx::PxVec3 AverageHitDirection(0.0f);
		physx::PxU32 Count = 0;
		for( physx::PxU32 eventN = 0; eventN < damageEvent.fractureEventListSize; ++eventN )
		{
			const NxApexChunkData& ChunkData = damageEvent.fractureEventList[eventN];
			if( ChunkData.flags & NxApexChunkFlag::FRACTURED )
			{
				if( DestructibleActor )
				{
					AllChunkBounds.include(ChunkData.worldBounds);
					AverageHitDirection += damageEvent.hitDirection;
					++Count;
				}
			}
		}
		if (Count > 0)
		{
			const INT DepthToUse = DestructibleAsset->DestructibleParameters.SupportDepth;
			AverageHitDirection /= (physx::PxF32)Count;
			physx::PxVec3 ChunkLocationN = AllChunkBounds.getCenter();
			FVector ChunkLocation = N2UPosition(NxFromPxVec3Fast(ChunkLocationN));
			FVector HitDirection = N2UVectorCopy(NxFromPxVec3Fast(-AverageHitDirection));
			DestructibleActor->SpawnFractureEffects( ChunkLocation, HitDirection, DepthToUse );
		}
	}
}

#endif

#endif

