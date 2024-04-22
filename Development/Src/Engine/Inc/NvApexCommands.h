/*=============================================================================
	UnSkeletalMesh.h: Unreal skeletal mesh objects.
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
#ifndef NV_APEX_COMMANDS_H

#define NV_APEX_COMMANDS_H

#include "NvApexManager.h"

#if WITH_APEX

#include <foundation/PxSimpleTypes.h>
#include "NxParamUtils.h"

class FIApexManager;
class FIApexScene;
class FIApexRender;
class FIApexActor;
class UApexClothingAsset;
class UApexDestructibleAsset;
class UApexAsset;
class UApexGenericAsset;

namespace physx
{
	namespace apex
	{
        class NxApexAsset;
        class NxUserRenderer;
        class NxResourceCallback;
    };
};

class NxScene;

namespace NxParameterized
{
	class Interface;
};

namespace physx
{
	namespace pubfnd2
	{
	class PxMat44;
	class PxVec3;
	class PxBounds3;
	};
};



/**
 * The ApexSerializeFormat type
 */
struct ApexSerializeFormat
{
	enum Enum
	{
		INI,
		XML,
		BINARY,
	};
};

/**
 * Interface to  an Apex Asset that provides utilities for applying materials, reference count
 */
class FIApexAsset
{
public:

	/**
	 * @return	The ApexAssetType.
	**/
	virtual 	ApexAssetType         GetType(void) const = 0;

  	/**
  	 * @return	null if it fails, else a pointer to the NxApexAsset. 
  	**/
  	virtual 	physx::apex::NxApexAsset         * GetNxApexAsset(void) const = 0;

  	/**
	 * Remaps the material names to the submeshes
  	 * @param	SubMeshCount -	Number of sub meshes. 
  	 * @param	Names -			The sub mesh names. 
  	**/
  	virtual	void				  ApplyMaterialRemap(physx::PxU32 SubMeshCount,const char **Names) = 0;

  	/**
	 * Causes any actors using this asset to be created due to property changes.
  	**/
  	virtual void				  RefreshActors(void) = 0; 

  	/**
	 * Increases the reference count
  	 * @param [in,out]	Actor	If non-null, it will put the actor on a list for this asset. 
  	 *
  	 * @return	The reference count 
  	**/
  	virtual physx::PxI32					IncRefCount(FIApexActor *Actor) = 0;

  	/**
	 * Decrements the reference count
  	 * @param [in,out]	Actor - If non-null, it will put the actor on a list for this asset. 
  	 *
  	 * @return	The reference count 
  	**/
  	virtual physx::PxI32					DecRefCount(FIApexActor *Actor) = 0;

  	/**
	 * Gets the asset name
  	 * @return	null if it fails, else the asset name. 
  	**/
  	virtual const char * GetAssetName(void) const = 0;

	/***
	* Get the UE3 Object associated with this ApexAsset
	**/
	virtual UApexAsset * GetUE3Object(void) const = 0;

  	/**
	 * Renames the asset
  	 * @param	Fname	the new name. 
  	**/
  	virtual void Rename(const char *Fname) = 0;

  	/**
	 * Gets the submesh count
  	 * @return	The submesh count. 
  	**/
  	virtual physx::PxU32 GetNumMaterials(void) = 0;

  	/**
	 * Returns the material name
  	 * @param	Index - The material index. 
  	 *
  	 * @return	null if it fails, else the material name.
  	**/
  	virtual const char * GetMaterialName(physx::PxU32 Index) = 0;

  	/**
	 * Serialize the asset so the game engine can save it out and restore it later.
  	 * @param [in,out]	Length - the length. 
  	 *
  	 * @return	null if it fails, else. 
  	**/
  	virtual const void * Serialize(physx::PxU32 &Length) = 0; 

  	/**
	 * Releases the serialized memory
  	 * @param	Mem - a pointer to the memory. 
  	**/
  	virtual void ReleaseSerializeMemory(const void *Mem) = 0;

	virtual const char * GetOriginalApexName(void) = 0;

#if !WITH_APEX_SHIPPING
	/**
	 * Applies a transformation to the asset
	 * @param	Transformation - The transformation. 
	 * @param	Scale -	The scale. 
	 * @param	bApplyToGraphics - TRUE to apply to graphics. 
	 * @param	bApplyToPhysics	- TRUE to apply to physics. 
	 * @param	bClockWise - TRUE to clock wise. 
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL ApplyTransformation(const physx::PxMat44 &Transformation,physx::PxF32 Scale,UBOOL bApplyToGraphics,UBOOL bApplyToPhysics,UBOOL bClockWise) = 0;

	/**
	 * Flips a clothing asset's index buffer triangle winding (used for 0.9 assets that were never flipped)
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL MirrorLegacyClothingNormalsAndTris() = 0;

#endif

	/**
	 * Loads the asset from memory
	 * @param	Mem	- a pointer to the memory location. 
	 * @param	Length - 	the length of the asset. 
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL LoadFromMemory(const void *Mem,physx::PxU32 Length,const char *AssetName,const char *originalName,UApexAsset *obj) = 0;

	/**
	 * Loads the asset from memory
	 * @param	AssetName - Name of the asset.
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL LoadFromParams(const void *Params,const char *AssetName,const char *originalName,UApexAsset *obj) = 0;


	/**
	 * Gets the default parameterized interface
	 * @return	null if it fails, else a pointer to the ::NxParameterized::Interface. 
	**/
	virtual ::NxParameterized::Interface * GetDefaultApexActorDesc(void) = 0;

	/**
	 * Gets the bind transformation for the asset
	 * @return	The bind transformation. 
	**/
	virtual const physx::PxMat44& GetBindTransformation(void) = 0;

	/**
	 * Check to see if the asset is ready to be used.
	 * @return	true if the asset is ready to be used 
	**/
	virtual UBOOL IsReady(void) const = 0; // 

	/****
	* Returns a descriptive label for this asset.
	*/
	virtual UBOOL GetDesc(char *dest,physx::PxU32 slen) = 0;

#if WITH_APEX_PARTICLES
	virtual void NotifyApexEditMode(class ApexEditInterface *iface) = 0;
#endif


};

struct NxDebugPoint;
struct NxDebugLine;
struct NxDebugTriangle;


/**
 * Interface for the APEX Commands
**/
class FIApexCommands
{
public:
	/**
	 * Process commands
	 * @param	Format - Describes the format to use. 
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL ProcessCommand(const char* Format, FOutputDevice* Ar) = 0;


  	/**
	 * Returns the Apex Asset
  	 * @param	AssetName - Name of the asset. 
  	 *
  	 * @return	null if it fails, else a pointer to the APEX asset. 
  	**/
  	virtual FIApexAsset * GetApexAsset(const char *AssetName) = 0;

  	/**
	 * Returns the Apex Asset from memory
  	 * @param	AssetName - Name of the asset.
  	 * @param	Data - The data.
  	 * @param	DataLength - Length of the data.
  	 *
  	 * @return	null if it fails, else returns the FIApexAsset from memory.
  	**/
  	virtual FIApexAsset * GetApexAssetFromMemory(const char *AssetName,
											   const void *Data,
											   physx::PxU32 DataLength,
											   const char *originalName,
											   UApexAsset *ue3Object) = 0;

  	/**
	 * Returns the Apex Asset from NxParameterized::Interface
  	 * @param	AssetName - Name of the asset.
  	 * @param	Params - NxParameterized::Interface.
  	 *
  	 * @return	null if it fails, else returns the FIApexAsset from NxParameterized::Interface.
  	**/
  	virtual FIApexAsset * GetApexAssetFromParams(const char *AssetName,
											   const void *Params,
											   const char *originalName,
											   UApexAsset *ue3Object) = 0;

	/**
	 * Returns the Apex Manager
	 * @return	null if it fails, else returns a pointer to the FIApexManager.
	**/
	virtual FIApexManager *	GetApexManager(void) const = 0;

	/**
	 * Returns the NxUserRenderer
	 * @return	null if it fails, a pointer to the NxUserRenderer
	**/
	virtual physx::apex::NxUserRenderer * GetNxUserRenderer(void) = 0;

	/**
	 * Returns the Apex Render
	 * @return	null if it fails, else returns a pointer to the FIApexRender. 
	**/
	virtual FIApexRender* GetApexRender(void) = 0;

	/**
	 * Sets the resource callback
	 * @param [in,out]	Callback - If non-null, a pointer to the callback function. 
	**/
	virtual void SetNxResourceCallback(physx::apex::NxResourceCallback *Callback) = 0;

	/**
	 * Releases the NxApexAsset
	 * @param [in,out]	Asset	If non-null, the asset. 
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL ReleaseNxApexAsset(physx::apex::NxApexAsset *Asset) = 0; // this apex asset instance is being requested to be released.

	/**
	 * Notifies the Apex Asset that ithas been removed
	 * @param [in,out]	Asset - If non-null, the asset. 
	**/
	virtual void NotifyApexAssetGone(FIApexAsset *Asset) = 0;

	/**
	 * Returns the persistent name
	 * @param	String - The string. 
	 *
	 * @return	null if it fails, else the persistent string. 
	**/
	virtual const char * GetPersistentString(const char *String) = 0;

	/**
	 * Check to see if Apex is being shown
	 * @return	TRUE if show apex, FALSE if not.
	**/
	virtual UBOOL IsShowApex(void) const = 0;

	/**
	 * Check to see if a command is an APEX command
	 * @param	Command	The command.
	 *
	 * @return	TRUE if apex command, FALSE if not.
	**/
	virtual UBOOL IsApexCommand(const char *Command) = 0;

	/**
	* Returns TRUE if apex debug visualization is enabled.
	*/
	virtual UBOOL IsVisualizeApex(void) const = 0;

	/**
	* Returns TRUE if clothing recording is enabled.
	*/
	virtual UBOOL	IsClothingRecording() = 0;

	/**
	* Pump the ApexCommands system which will detect if any pending assets which have been recently loaded
	* need to have a 'force load' performed on them.  This is done because it must happen in the game thread.
	*/
	virtual void Pump(void) = 0;

	/***
	* Adds this clothing asset to a queue to have the material pointers resolved at the next pump cycle.
	*/
	virtual void AddUpdateMaterials(UApexAsset *Asset) = 0;
	/***
	Notification that this clothing asset is about to be destroyed and should be removed from the pending material resolution queue.
	*/
	virtual void NotifyDestroy(UApexAsset *Asset) = 0;

	virtual void AddApexScene(FIApexScene *scene) = 0;
	virtual void RemoveApexScene(FIApexScene *scene) = 0;

	virtual UINT GetNumDebugVisualizationNames(const char* moduleName) = 0;
	virtual const char* GetDebugVisualizationName(const char* moduleName, UINT i) = 0;
	virtual void GetDebugVisualizationNamePretty(FString& prettyName, const char* moduleName, UINT i) = 0;

	/***
	* Inspects the buffer and figures out what asset format it points to.
	*/
	virtual ApexAssetFormat GetApexAssetFormat(const void *Buffer,physx::PxU32 dlen,physx::PxU32 &numObj) = 0;

#if !WITH_APEX_SHIPPING
	virtual const char * GetApexAssetBuffer(physx::PxU32 objNumber,const void *Buffer,physx::PxU32 dlen,const void *&buffer,physx::PxU32 &bufferLen) = 0;

#if WITH_APEX_PARTICLES
	virtual FIApexAsset * CreateDefaultApexAsset(const char *assetName,ApexAssetType type,UApexAsset *ueObject,UApexGenericAsset *parent) = 0;
#endif

#endif

	virtual const char * namedReferenceCallback(const char *className,const char *namedReference)  = 0;

protected:

	/**
     * Destructor
	**/
	virtual ~FIApexCommands(void) { };
};

/**
 * Creates a pointer to the FIApexCommands
 * @return	null if it fails, else returns a pointer to an FIApexCommands.
**/
FIApexCommands * CreateApexCommands(void);

/**
 * Released the FIApexCommands
 * @param [in,out]	InterfaceCommands	If non-null, the InterfaceCommands.
**/
void ReleaseApexCommands(FIApexCommands *InterfaceCommands);

//! Global pointer to the Apex Commands
extern FIApexCommands *GApexCommands;

void InitializeApex(void);

#endif

#endif
