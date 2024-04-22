/*=============================================================================
	NvApexScene.h : Declares the FIApexClothing interface and FIApexScene clases.
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
#ifndef NV_APEX_SCENE_H

#define NV_APEX_SCENE_H

#include "NvApexManager.h"

#if WITH_APEX

#include <foundation/PxSimpleTypes.h>

class FIApexManager;
class FIApexAsset;
class FIApexRender;
class FIApexActor;
class FIApexScene;

namespace physx
{
	namespace pubfnd2
	{
		class PxVec3;
	};
};
#include "NxClothingVelocityCallback.h"
class NxScene;
class NxDebugRenderable;

namespace physx
{
	namespace apex
	{
        class NxApexScene;
        class NxApexActor;
        class NxUserRenderer;
		class NxApexInterface;
    };
};

namespace NxParameterized
{
	class Interface;
}

/**
 * Apex scene stats
**/
enum EApexStats
{
	STAT_ApexBeforeTickTime = STAT_ApexFirstStat,
	STAT_ApexDuringTickTime,
	STAT_ApexAfterTickTime,
	STAT_ApexPhysXSimTime,
	STAT_ApexClothingSimTime,
	STAT_ApexPhysXFetchTime,
	STAT_ApexUserDelayedFetchTime,
	STAT_ApexNumActors,
	STAT_ApexNumShapes,
	STAT_ApexNumAwakeShapes,
	STAT_ApexNumCPUPairs,
#if WITH_APEX_GRB
	STAT_ApexGrbSimTime,
	STAT_ApexNumGPUPairs,
#endif
#if _WINDOWS
	STAT_ApexGpuHeapTotal,
	STAT_ApexGpuHeapUsed
#endif
};


/**
 * Apex Actor types
**/
struct ApexActorType
{
  enum Enum
  {
    UNKNOWN,
    DESTRUCTIBLE,
    CLOTHING,
	EMITTER,
  };
};

/**
 * Helper function to determine if a string from APEX is a valid object name.
**/
static UBOOL IsValidObjectPathName(const FString& Name)
{
	// Modified from FIsValidXName()

	// Must have a .
	if( Name.InStr( TEXT(".") ) == INDEX_NONE )
	{
		return FALSE;
	}

	return FIsValidXString( Name, INVALID_GROUPNAME_CHARACTERS );
}

/**
 * Helper function to find a material using an ASCII Material name provided by APEX.
 * NB: This function will return NULL if the package containing the material hasn't
 * been loaded yet. Beware of calling this during async package loading.
**/
static UMaterialInterface* FindMaterialForApex(const char* MaterialNameA)
{
	if( MaterialNameA == NULL )
	{
		MaterialNameA = "";
	}
	UMaterialInterface *Material      = NULL;
	UBOOL               bIsValidName   = IsValidObjectPathName(ANSI_TO_TCHAR(MaterialNameA));

	if( bIsValidName )
	{
		Material = FindObject<UMaterialInterface>(0, ANSI_TO_TCHAR(MaterialNameA), 0);
		if( Material == NULL )
		{
			debugf( NAME_DevPhysics, TEXT("Unable to find material %s for APEX"), ANSI_TO_TCHAR(MaterialNameA) );
		}
	}
	
	return Material;
}
void InitMaterialForApex(UMaterialInterface *&MaterialInterface);
/**
 * Helper function to find and initialize a material using an ASCII Material name provided by APEX.
**/
static void LoadMaterialForApex(const char* MaterialNameA, TArray<UMaterialInterface*>& Materials)
{
	UMaterialInterface *Material      = FindMaterialForApex(MaterialNameA);
	if( Material == NULL )
	{
		Material = FindObject<UMaterialInterface>(0, TEXT("EngineMaterials.DefaultMaterial"), 0);
	}

	if( ensure(Material != NULL) )
	{
		InitMaterialForApex(Material);
		Materials.AddItem(Material);
	}
}


/**
 * @class	FIApexActor
 *
 * @brief	Fi apex actor. 
 *
 * @author	Nvidiauser
 * @date	1/27/2010
**/
class FIApexActor
{
public:

  /**
   * Returns the Apex Actor
   * @return	null if it fails, else a pointer to the NxApexActor. 
  **/
  virtual physx::apex::NxApexActor *    GetNxApexActor(void) const = 0;

  /**
   * Gets the Apex Actor Type
   * @return	The apex actor type. 
  **/
  virtual ApexActorType::Enum			GetApexActorType(void) const = 0;

  /**
   * Releases the actor
  **/
  virtual void							Release(void) = 0;

  /**
   * Return the user data
   * @return	null if it fails, else a pointer to the user data. 
  **/
  virtual void *						GetUserData(void) = 0;

  /**
   * Notifies the actor that the asset which is was created with is now gone.
  **/
  virtual void							NotifyAssetGone(void) = 0; 

  /**
   * Notifies the actor that a fresh asset has been imported and the actor can be re-created.
  **/
  virtual void							NotifyAssetImport(void) = 0; 

  /**
   * Renders the Actor
   * @param [in,out]	Renderer - The NxUserRenderer.
  **/
  virtual void							Render(physx::apex::NxUserRenderer &Renderer) = 0;

  /**
  * Gathers the rendering resources for this APEX actor
  **/
  virtual void							UpdateRenderResources( void* ApexUserData = NULL ) = 0;
};

/**
 * Represents a single piece of clothing
**/
class FIApexClothingPiece : public physx::apex::NxClothingVelocityCallback
{
public:

	/**
	 * Get the Apex Asset
	 * @return	null if it fails, else a pointer to the FIApexAsset. 
	**/
	virtual FIApexAsset	* GetApexAsset(void) = 0;

	/**
	 * Returns TRUE if this piece of clothing is associated with this material index.
	**/
	virtual UBOOL UsesMaterialIndex(physx::PxU32 materialIndex) const = 0;

	/***
	* Sets the current wind vector and wind adaptation time.
	**/
	virtual void SetWind(physx::PxF32 adaptTime,const physx::PxF32 *windVector) = 0;

	/***
	* Sets the current MaxDistance Scale.
	**/
	virtual void SetMaxDistanceScale(FLOAT scale, INT ScaleMode) = 0;

	virtual void ToggleClothingPieceSimulation(const UBOOL& bEnableSimulation) = 0;
	/**
	\brief This callback will be fired in Apex threads. It must not address any user data, just operate on the data.
	*/
	virtual bool velocityShader(physx::PxVec3* velocities, const physx::PxVec3* positions, physx::PxU32 numVelocities) = 0;
	virtual UBOOL GetBounds(FBoxSphereBounds& Bounds) = 0;
	virtual void SetMaterial(INT MaterialIndex, UMaterialInterface *Material ) = 0;
	/** 
	* Gets number of clothing graphical vertices at the specified lod and submesh
	*/
	virtual INT	GetNumVertices(INT LodIndex, INT SectionIndex) = 0;
protected:

	/**
	 * Destructor
	**/
	virtual ~FIApexClothingPiece(void) { };
};

/**
 * Class which provides control over multiple pieces of clothing for one asset
**/
class FIApexClothing
{
public:

	/**
	 * Adds a piece of clothing
	 * @param [in,out]	Asset - The Apex Asset.
	 * @param [in,out]	Iface - The interface.
	 *
	 * @return	null if it fails, else a pointer to the FIApexClothingPiece.
	**/
	virtual FIApexClothingPiece *AddApexClothingPiece(FIApexAsset *Asset,::NxParameterized::Interface *Iface,physx::PxU32 materialIndex, USkeletalMeshComponent* skeletalMeshComp) = 0;
	/**
	 * Removes a piece of clothing
	 * @param [in,out]	ClothingPiece	The piece of clothing.
	**/
	virtual void  				RemoveApexClothingPiece(FIApexClothingPiece *ClothingPiece) = 0;

	/**
	 * Returns TRUE if all pieces of clothing are ready for this actor.  Check this before rendering.
	 * @param [in,out]	Renderer - the renderer.
	 * #param eyePos : A pointer to the position of the camera X/Y/Z
	 *
	 * @return	TRUE if ready, FALSE if not.
	**/
	virtual UBOOL IsReadyToRender(physx::apex::NxUserRenderer &Renderer,const physx::PxF32 *eyePos, INT ClothingAssetIndex) = 0;
    /**
	 * Syncs the transforms for the clothing.  Uses 4x4 transforms.
     * @param	BoneCount - Number of bones.
     * @param	Matrices -  The matrices.
     * @param	Stride -    The stride.
	 * @param	LocalToWorld -  Transform matrix in order to teleport clothing without resetting.
    **/
    virtual void SyncTransforms(physx::PxU32 BoneCount,const physx::PxF32 *Matrices,physx::PxU32 Stride, const FMatrix& LocalToWorld) = 0;

	/** Force the next call to SyncTransform to use the 'teleport and reset' mode */
	virtual void ForceNextUpdateTeleportAndReset() = 0;
	/** Force the next call to SyncTransform to use the 'teleport' mode */
	virtual void ForceNextUpdateTeleport() = 0;

	/**
	  * Gather Render Resources
	  *
	  *
	 **/
	virtual void UpdateRenderResources(void) = 0;


	/**
	 * Gets the Apex Scene
	 * @return	null if it fails, else the FIApexScene. 
	**/
	virtual FIApexScene * GetApexScene(void) = 0;

	/**
	 * Gets the number of times the clothing has been simulated
	 * @return	The pump count. 
	**/
	virtual physx::PxU32			GetPumpCount(void) const = 0;

	/**
	 * Semahpore, TRUE if any of the underlying assets have been released and need to be re-loaded.
	 * @return	TRUE if it succeeds, FALSE if it fails.
	**/
	virtual UBOOL NeedsRefreshAsset(void) = 0;

	/**
	 * Refreshes the Apex Clothing pieces
	 * @param [in,out]	Asset - the FIApexAsset.
	 * @param [in,out]	Iface -	::NxParameterized::Interface.
	**/
	virtual void RefreshApexClothingPiece(FIApexAsset *Asset,::NxParameterized::Interface *Iface) = 0;

	/**
	 * Adds the bone name so the clothing system can remap internal clothing transforms to the application
	 * copy of the skeleton.
	 * @param	BoneName - Name of the bone.
	**/
	// correalate this bone name with the following index.
	virtual void AddBoneName(const char *BoneName) = 0;

	/**
	 * Releases the clothing
	**/
	virtual void Release(void) = 0;

	/**
	 * Returns TRUE if a piece of clothing is associated with this material index.
	**/
	virtual UBOOL UsesMaterialIndex(physx::PxU32 materialIndex) const = 0;

	/**
	* Returns the number of bones actively used/referenced by clothing and
	* a pointer to an array of indices pointing back to the application source skeleton.
	*
	* @param : BoneCount a reference which returns the number of bones actively used.
	*
	* @return : A const pointer of indices mapping back to the original application skeleton.
	*/
	virtual const physx::PxU32 * GetBonesUsed(physx::PxU32 &BoneCount) const = 0;

	/***
	Returns the name of the used bone at this index number.
	*/
	virtual const char * GetBoneUsedName(physx::PxU32 BoneIndex) const = 0;

	/***
	Returns TRUE if this clothing is recently visible.
	* @param resetBias - If resetBias is true, then children pieces of clothing have their LOD bias set to zero if they are newly non-visible.
	*/
	virtual UBOOL IsVisible(UBOOL resetBias) = 0;

	/**
	* Returns true if the clothing is in a frame greater than the frame passed in.
	* Updates the frame passed in with the current frame.
	* @param NewFrame - a reference to an unsigned integer which contains the last known frame and is refreshed with the current frame.
	*/
	virtual UBOOL IsNewFrame(physx::PxU32 &NewFrame) const = 0;

	/***
	* Sets the current wind vector and wind adaptation time.
	**/
	virtual void SetWind(physx::PxF32 adaptTime,const physx::PxF32 *windVector) = 0;

	virtual void Tick(FLOAT DeltaTime) = 0;
	virtual void SetMaxDistanceScale(FLOAT StartScale, FLOAT EndScale, INT ScaleMode, FLOAT Duration) = 0;

	virtual void ToggleClothingSimulation(const UBOOL& bEnableSimulation) = 0;
	virtual void SetGraphicalLod(physx::PxU32 lod) = 0;

	/***
	* Sets the clothing visible or not
	**/
	virtual void SetVisible(UBOOL bEnable) = 0;
	virtual UBOOL GetBounds(FBoxSphereBounds& Bounds) = 0;
	virtual void BuildBoneMapping() = 0;
	virtual void SetMaterial(INT MaterialIndex, UMaterialInterface *Material ) = 0;
	/** 
	* Gets number of clothing graphical vertices at the specified lod and submesh
	*/
	virtual INT	GetNumVertices(INT LodIndex, INT SectionIndex) = 0;
protected:

	/**
     * Destructor
	**/
	virtual ~FIApexClothing(void) { };
};

/****
* This class manages a single APEX emitter
*/
class FApexEmitter
{
public:
	virtual void release(void) = 0;
	  /**
   * Renders the Actor
   * @param [in,out]	Renderer - The NxUserRenderer.
  **/
  virtual void							Render(physx::apex::NxUserRenderer &Renderer) = 0;

  /**
  * Gathers the rendering resources for this APEX actor
  **/
  virtual void							UpdateRenderResources(void) = 0;

  virtual UBOOL	IsEmpty(void) = 0;

protected:
	virtual ~FApexEmitter(void) { };
};

/**
 * The Apex scene which is called in order to simulate, and render Apex Actors
**/
class FIApexScene
{
public:

	/**
	 * @param	DeltaTime	The amount of time to simulate. 
	**/
	virtual void			Simulate(physx::PxF32 DeltaTime) = 0;

	/**
	 * @param	bForceSync - True to block until the simulation has completed. 
	 * @param [in,out]	ErrorResult - The error result. 
	 *
	 * @return	TRUE if it succeeds, FALSE if it fails. 
	**/
	virtual UBOOL			FetchResults(UBOOL bForceSync,physx::PxU32 *ErrorResult) = 0;

	/**
	 * Gets the APEX scene
	 * @return	null if it fails, else the apex scene. 
	**/
	virtual physx::apex::NxApexScene		*GetApexScene(void) const = 0;

  	/**
	 * Creates and Apex Actor
  	 * @param [in,out]	Asset - If non-null, the asset. 
  	 * @param	Params - Options for controlling the operation. 
  	 *
  	 * @return	null if it fails, else. 
  	**/
  	virtual FIApexActor		*CreateApexActor(FIApexAsset *Asset,const ::NxParameterized::Interface *Params) = 0;

  	/**
	 * Recreates the Apex Actor
  	 * @param [in,out]	Actor - If non-null, the actor. 
  	 * @param	Params - Options for controlling the operation. 
  	 *
  	 * @return	TRUE if it succeeds, FALSE if it fails. 
  	**/
  	virtual UBOOL			RecreateApexActor(FIApexActor *Actor,const ::NxParameterized::Interface *Params) = 0;

	/**
	 * Releases the actor
	 * @param [in,out]	Actor - the actor. 
	**/
	virtual void			ReleaseApexActor(FIApexActor &Actor) = 0;

  	/**
	 * Gets the Debug Renderable
  	 * @return	null if it fails, else the debug renderable. 
  	**/
  	virtual const NxDebugRenderable	*GetDebugRenderable(void) = 0;

	/**
	 * Sets the Debug Render State
	 * @param	VisScale - The visualization scale. 
	**/
	virtual void SetDebugRenderState(physx::PxF32 VisScale) = 0;

	/**
	 * Creates the Apex Clothing
	 * @return	null if it fails, else returns a pointer to the FIApexClothing. 
	**/
	virtual	FIApexClothing * CreateApexClothing(void) = 0;

	/***
	* Returns the current simulation counter
	* @return Returns the current frame count of the simulation.
	*/
	virtual physx::PxU32 GetSimulationCount(void) const = 0;
#if WITH_APEX_PARTICLES
	virtual FApexEmitter * CreateApexEmitter(const char *assetName) = 0;
#endif

	/***
	* Update the projection matrix for APEX LOD.
	*/
	virtual void UpdateProjection(const FMatrix& ProjMatrix, FLOAT FOV, FLOAT Width, FLOAT Height, FLOAT MinZ) = 0;

	/***
	* Update the view matrix for APEX LOD.
	*/
	virtual void UpdateView(const FMatrix& ViewMatrix) = 0;

	/**
	 * Updates values of Apex stats
	**/
	virtual void UpdateStats() = 0;
};

/**
 * Creates the APEX Scene
 * @param [in,out]	Scene - If non-null, the NxScene.
 * @param [in,out]	Sdk	- If non-null, the FIApexManager.
 * @param	bUseDebugRenderable - TRUE to use debug renderable.
 *
 * @return	null if it fails, else a pointer to the FIApexScene.
**/
FIApexScene * CreateApexScene(NxScene *Scene,FIApexManager *Sdk,UBOOL bUseDebugRenderable);

/**
 * Releases the APEX Scene
 * @param [in,out]	scene	If non-null, the scene.
**/
void          ReleaseApexScene(FIApexScene *scene);

//! Global variable for the number of active apex scenes
extern physx::PxU32	   GActiveApexSceneCount;

/**
* @class	FApexCleanUp
*
* @brief	Deferred Clean Up Interface to clean up Apex Actors and Apex Asset Preview
*
* @author	Nvidiauser
* @date	1/27/2010
**/
class FApexCleanUp: public FDeferredCleanupInterface
{
public:
	FApexCleanUp(physx::apex::NxApexInterface *ApexObj, FIApexAsset *Asset);
	~FApexCleanUp();
	virtual void FinishCleanup();
	static volatile INT	 PendingObjects;
protected:
	physx::apex::NxApexInterface	*ApexObject;
	FIApexAsset						*ApexAsset;
};
#endif

#endif
