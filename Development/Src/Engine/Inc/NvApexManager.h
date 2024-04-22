/*=============================================================================
	NvApexManager.h : Header file for NvApexManager.cpp, manages the APEX SDK and modules
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
#ifndef NV_APEX_MANAGER_H

#define NV_APEX_MANAGER_H

#include "UnBuild.h"
#include "Core.h"



class NxPhysicsSDK;
class NxCookingInterface;

namespace physx
{
	namespace apex
	{
		class NxApexSDK; // forward reference the APEX SDK
		class NxApexScene;
		class NxModuleDestructible;
		class NxModuleClothing;
		class NxResourceCallback;
		class NxUserRenderResourceManager;
		class NxApexRenderable;
	}
	namespace pxtask
	{
		class CudaContextManager;
	};
}

#if WITH_APEX
class FIApexScene;

#if WITH_APEX_PARTICLES
namespace physx
{
	class PxEditorWidgetManager;
};
#endif
#pragma pack( push, 8 )
#include "foundation/PxSimpleTypes.h"
#pragma pack(pop)

/**
 * The type of the Apex Asset
 */
enum ApexAssetType
{
  AAT_DESTRUCTIBLE,	// used to construct destructible actors
  AAT_CLOTHING,		// asset used to construct clothing actors
  AAT_APEX_EMITTER,		// asset used to construct emitters.
  AAT_GROUND_EMITTER,
  AAT_IMPACT_EMITTER,
  AAT_BASIC_IOS,
  AAT_FLUID_IOS,
  AAT_IOFX,
  AAT_RENDER_MESH,
  AAT_GENERIC, // just generic data, we don't resolve it to a specific type.
  AAT_LAST
};

enum ApexAssetFormat
{
	AAF_DESTRUCTIBLE_LEGACY,		// PDA
	AAF_CLOTHING_LEGACY,			// ACA
	AAF_CLOTHING_MATERIAL_LEGACY,   // ACML
	AAF_RENDER_MESH_LEGACY,			// ARM
	AAF_DESTRUCTIBLE_XML,			// single destructible asset in an XML serialized file.
	AAF_DESTRUCTIBLE_BINARY,		// single destructible asset in a binary serialized file.
	AAF_CLOTHING_XML,				// single clothing asset in an XML serialized file.
	AAF_CLOTHING_BINARY,			// single clothing asset in a binary serialized file.
	AAF_XML,						// XML serialized format
	AAF_BINARY,						// binary serialized format
	AAF_LAST
};

class FIApexCommands;

/**
    FIApexManager is a singleton class for managing global APEX SDK and modules
*/
class FIApexManager
{
public:

  /**
   * Gets a pointer to the Apex SDK
   * @return	null if it fails, else a pointer to NxApexSDK. 
  **/
  virtual physx::apex::NxApexSDK            * GetApexSDK(void) const = 0;

  /**
   * Gets a pointer to the Destructible Module
   * @return	null if it fails, else a pointer to the NxModuleDestructible. 
  **/
  virtual physx::apex::NxModuleDestructible * GetModuleDestructible(void) const = 0;

  /**
   * Gets a pointer to the Clothing Module
   * @return	null if it fails, else a pointer to the NxModuleClothing. 
  **/
  virtual physx::apex::NxModuleClothing     * GetModuleClothing(void) const = 0;
   /**
   * Sets the Apex resource callback function
   * @param [in,out]	Callback - pointer to the callback function. 
  **/
  virtual void					 SetResourceCallback(physx::apex::NxResourceCallback *Callback) = 0;

  /**
   * Sets the Apex render resource manager
   * @param [in,out]	Manager - pointer to the NxUserRenderResourceManager. 
  **/
  virtual void					 SetRenderResourceManager(physx::apex::NxUserRenderResourceManager *Manager) = 0;

	/**
	 * Sets the current default LOD Resource Budget
	 * @param	LODResourceBudget - The default LOD resource budget per scene.
	 **/
	virtual void				SetApexLODResourceBudget(FIApexScene* Scene, physx::PxF32 LODResourceBudget) = 0;

	/***
	* A pump loop (tick) to make sure any outstanding APEX resource processing is being handled.
	*/
	virtual void				Pump(void) = 0;

	/***
	* Inspects the buffer and figures out what asset format it points to.
	*/
	virtual ApexAssetFormat GetApexAssetFormat(const void *Buffer,physx::PxU32 dlen,physx::PxU32 &numObj) = 0;

#if !WITH_APEX_SHIPPING
	/****
	* Retrieve a memory buffer for a specific data asset 
	*/
	virtual const char * GetApexAssetBuffer(physx::PxU32 objNumber,const void *Buffer,physx::PxU32 dlen,const void *&buffer,physx::PxU32 &bufferLen) = 0;
#endif

	virtual void SetCurrentImportFileName(const char *fname) = 0;
	virtual const char * GetCurrentImportFileName(void) const = 0;
#if WITH_APEX_PARTICLES
	virtual physx::PxEditorWidgetManager * GetPxEditorWidgetManager(void) = 0;
#endif
	virtual physx::pxtask::CudaContextManager	*		GetCudaContextManager(void) = 0;
};

/**
 * Creates and Apex Manager
 * @param [in,out]	Sdk	- A pointer to the NxPhysicsSDK.
 * @param [in,out]	Cooking	- A pointer to the NxCookingInterface.
 *
 * @return	null if it fails, else.
**/
FIApexManager * CreateApexManager(NxPhysicsSDK *Sdk, NxCookingInterface *Cooking);

/**
 * Releases the Apex Manager
 * @param [in,out]	Iface - A poniter to the FIApexManager.
**/
void           ReleaseApexManager(FIApexManager *Iface);

//! Global pointer for the Apex Manager
extern FIApexManager *GApexManager;
//! Global var for the number of Apex Scenes
extern physx::PxU32	   GActiveApexSceneCount;

void InitializeApex(void);

#if WITH_APEX_PARTICLES

class UApexAsset;

namespace NxParameterized
{
	class Handle;
	class Interface;
}

class ApexEditNotify
{
public:
	virtual UBOOL  OnPostPropertyEdit(NxParameterized::Interface *iface,NxParameterized::Handle &handle) = 0;
	virtual UBOOL  NotifyEditorClosed(NxParameterized::Interface *iface) = 0;
};

class ApexEditInterface
{
public:
	virtual void SetApexEditNotify(UApexAsset *obj,ApexEditNotify *notify) = 0;
	// Notify the apex object editor that an asset object is being destroyed.
	virtual void NotifyObjectDestroy(UApexAsset *obj) = 0;
protected:
	virtual ~ApexEditInterface(void) { };
};

#endif


#endif // endif WITH_APEX

#endif
