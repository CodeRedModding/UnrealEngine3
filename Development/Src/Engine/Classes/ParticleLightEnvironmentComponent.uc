/**
 * Light environment class used by particle systems.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class ParticleLightEnvironmentComponent extends DynamicLightEnvironmentComponent
	native(Light);

/** Reference count used to know when this light environment can be detached and cleaned up since it may be shared by multiple particle system components. */
var transient protected{protected} const int ReferenceCount;

/** Number of different particle components this particle light environment has been used by. */
var transient const int NumPooledReuses;

/** Lit particle components created from the emitter pool will only share particle DLE's if they have matching SharedInstigator's. */
var transient const Actor SharedInstigator;

/** Lit particle components created from the emitter pool will only share particle DLE's if they have matching SharedParticleSystem's. */
var transient const ParticleSystem SharedParticleSystem;

/** Whether this DLE can be shared by particle components of the same actor. */
var bool bAllowDLESharing;

cpptext
{
	inline void AddRef() 
	{ 
		ReferenceCount++; 
		NumPooledReuses++;
	}

	inline void RemoveRef() 
	{ 
		check(ReferenceCount > 0);
		ReferenceCount--; 
	}
	inline INT GetRefCount() const { return ReferenceCount; }

	virtual void UpdateLight(const ULightComponent* Light);

	// UActorComponent interface.
	virtual void Tick(FLOAT DeltaTime);
	virtual void BeginDestroy();

protected:

	virtual UBOOL NeedsUpdateBasedOnComponent(UPrimitiveComponent* Component) const;

#if USE_GAMEPLAY_PROFILER
    /** 
     * This function actually does the work for the GetProfilerAssetObject and is virtual.  
     * It should only be called from GetProfilerAssetObject as GetProfilerAssetObject is safe to call on NULL object pointers
     */
	virtual UObject* GetProfilerAssetObjectInternal() const;
#endif

	/**
     * This function actually does the work for the GetDetailInfo and is virtual.
     * It should only be called from GetDetailedInfo as GetDetailedInfo is safe to call on NULL object pointers
     */
	virtual FString GetDetailedInfoInternal() const;
}

defaultproperties
{
	ReferenceCount=1
	NumPooledReuses=1
	// Sharing now works correctly with the emitter pool, default to on
	bAllowDLESharing=true
	// Effects are often moved around
	bDynamic=true
	// Particles are most likely translucent, translucency needs line-check shadowing from dominant lights.
	bForceCompositeAllLights=true
	// Prevents strobing when muzzle flash lights are enabled inside the lit particle system
	bAffectedBySmallDynamicLights=false
	InvisibleUpdateTime=10.0
	MinTimeBetweenFullUpdates=3.0
	// Using DLEB_ActiveComponents instead of DLEB_OwnerComponents to prevent the Owner's position from affecting the bounds, which is necessary for pooled particle components.
	BoundsMethod=DLEB_ActiveComponents
}
