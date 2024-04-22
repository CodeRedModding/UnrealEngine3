/**
 * This is the base class for crowd agents.  Allows us to interact with it in the Engine module.
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class CrowdAgentBase extends Actor
	abstract
	native(AI)
    implements( Interface_NavigationHandle )
	dependson( CoverLink )
	ClassGroup(Crowd);


cpptext
{
	virtual UBOOL	CanCoverSlip(ACoverLink* Link, INT SlotIdx)	{ return FALSE; }

	/**
	 * returns the offset from the edge move point this entity should move toward (e.g. how high off the ground we should move to)
	 * @param Edge - the edge we're moving to
	 * @return - the offset to use
	 */
	virtual FVector GetEdgeZAdjust(FNavMeshEdgeBase* Edge)
	{
		return FVector(0.f,0.f,1.f);		
	}

	virtual void SetupPathfindingParams( FNavMeshPathParams& out_ParamCache );
	virtual void InitForPathfinding() {}
	virtual INT ExtraEdgeCostToAddWhenActive(FNavMeshEdgeBase* Edge) { return 0; }
}

// Interface_navigationhandle stub - called when path edge is deleted that this controller is using
event NotifyPathChanged();



defaultproperties
{
}
