/**
 * Interface for actors that use NavigationHandle
 * - implementations of this interface describe what constraints and parameters to use when searching for a path across the nav mesh
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
interface Interface_NavigationHandle
	native;

cpptext
{
	/** >>>>> here lie functions which take input, and thus can not be cached */
	virtual UBOOL	CanCoverSlip(class ACoverLink* Link, INT SlotIdx)	{ return FALSE; }

	/**
	 * returns the offset from the edge move point this entity should move toward (e.g. how high off the ground we should move to)
	 * @param Edge - the edge we're moving to
	 * @return - the offset to use
	 */
	virtual FVector GetEdgeZAdjust(struct FNavMeshEdgeBase* Edge)=0;

	/**
	 * allows entities to do custom validation before OK'ing mantle edges
	 * @param Edge - the edge we're verifying
	 * @return TRUE if the passed edge is OK to traverse
	 */
	virtual UBOOL CheckMantleValidity(struct FNavMeshMantleEdge* Edge){ return TRUE; }
    /*** <<<<<< */


	/**
	 * this function is responsible for setting all the relevant parmeters used for pathfinding
	 * @param out_ParamCache - the output struct to populate params in
	 * @NOTE: ALL Params FNavMeshPathParams should be populated
	 * 
	 */
	virtual void SetupPathfindingParams( struct FNavMeshPathParams& out_ParamCache )=0;

	/**
	 * Called from FindPath() at the beginning of a path search to give this entity a chance to initialize transient data
	 */
	virtual void InitForPathfinding()=0;


	/**
	 * when this entity is using an edge (e.g. it has been marked active and is in this handle's pathcache) this function allows
	 * extra cost to be added for other entities trying to use that edge.  (e.g. to keep guys from using the same path)
	 * @param Edge - the edge we're about to mark as active
	 */
	virtual INT  ExtraEdgeCostToAddWhenActive(FNavMeshEdgeBase* Edge) { return 0; }

	/**
	 * DebugLog function which is called to log information specific to this AI (call NAVHANDLE_DEBUG_LOG macro, don't call this directly)
	 * @param LogText - text to log for this AI
	 */
	virtual void DebugLogInternal(const TCHAR* LogText) {}
}

/**
 *  this event is called when an edge is deleted that this handle is actively using
 */
event NotifyPathChanged();

