//=============================================================================
// StaticMeshActor.
// An actor that is drawn using a static mesh(a mesh that never changes, and
// can be cached in video memory, resulting in a speed boost).
// Note that PostBeginPlay() and SetInitialState() are never called for StaticMeshActors
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class StaticMeshActor extends StaticMeshActorBase
	native
	placeable;

var() const editconst StaticMeshComponent	StaticMeshComponent;

/** If checked, this actor will not try to set its base to any proc building below it.*/
var() editoronly bool bDisableAutoBaseOnProcBuilding;

/** 
 * If checked, this actor is a proxy built from other (now hidden) actors.
 * This should only be true, if it can be reverted to the actors it was constructed from!
 */
var private editoronly bool bProxy;

/** 
 * If checked, this actor is one of the actors that was used to create a proxy
 * This should only be true, if it can be reverted to it's previous state
 */
var private editoronly bool bHiddenByProxy;

/** 
 * We need to disable lighting/shadows when bHiddenByProxy is true, but we need to 
 * revert to it's previous state when it returns to false, so use this to track it
 */
var private editoronly bool OldCastShadow;
var private editoronly bool OldAcceptsLights;
var private editoronly ECollisionType OldCollisionType;

cpptext
{
	/**
	 * Function that gets called from within Map_Check to allow this actor to check itself
	 * for any potential errors and register them with map check dialog.
	 */
#if WITH_EDITOR
	virtual void CheckForErrors();
	
	/** tells this Actor to set its collision for the path building state
	 * for normally colliding Actors that AI should path through (e.g. doors) or vice versa
	 * @param bNowPathBuilding - whether we are now building paths
	 */
	virtual void SetCollisionForPathBuilding(UBOOL bNowPathBuilding);
	/**
	 * Show or hide the actors the proxy actor
	 *
	 * @param	bShow		If TRUE, show the actors; if FALSE, hide them
	 * @param bIncProxyFlag If TRUE, set the flag which indicates this was hidden by the proxy
	 */
	void ShowProxy(const UBOOL bShow, const UBOOL bIncProxyFlag);

	/**
	 * Flag that this mesh is a proxy
	 */
	void SetProxy(const UBOOL Proxy);

	/**
	 * Get whether this is a proxy
	 */
	UBOOL IsProxy() const;

	/**
	 * Flag that this mesh is hidden because we have a proxy in it's place
	 */
	void SetHiddenByProxy(const UBOOL HiddenByProxy);

	/**
	 * Get whether this is a hidden because we have a proxy
	 */
	UBOOL IsHiddenByProxy() const;
#endif

	/** Used to parent this StaticMeshComponent to a base building's low LOD. */
	virtual void SetBase(AActor *NewBase, FVector NewFloor = FVector(0,0,1), INT bNotifyActor=1, USkeletalMeshComponent* SkelComp=NULL, FName BoneName=NAME_None );

	virtual void PostEditMove( UBOOL bFinished );

protected:
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

event PreBeginPlay() {}

defaultproperties
{
	Begin Object Class=StaticMeshComponent Name=StaticMeshComponent0
		bAllowApproximateOcclusion=TRUE
		bForceDirectLightMap=TRUE
		bUsePrecomputedShadows=TRUE
	End Object
	CollisionComponent=StaticMeshComponent0
	StaticMeshComponent=StaticMeshComponent0
	Components.Add(StaticMeshComponent0)

	bDisableAutoBaseOnProcBuilding=false;
	bProxy=false;
	bHiddenByProxy=false;
	OldCastShadow=false;
	OldAcceptsLights=false;
	OldCollisionType=COLLIDE_NoCollision;
}
