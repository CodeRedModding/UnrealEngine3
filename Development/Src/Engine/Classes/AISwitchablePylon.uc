//=============================================================================
// AISwitchablePylon
//
// represents a mesh which is turned on/off via an AI triggerable switch at runtime.. e.g. an electronic gate, or a laser fence
//
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class AISwitchablePylon extends Pylon
	placeable
	native(inherit);
cpptext
{
	/** returns TRUE if the path from Poly back to start has an edge which is linked to a switch which is linked to this 
	 * pylon
	 * @param Edge - the edge linking Poly to the next neighbor in question
	 * @param Poly - the source poly (the current end-of-line poly in the chain)
	 * @return - TRUE if the previousPath chain of Poly has a switch linked to this pylon in it
	 */
	UBOOL HasSwitchLinkedToMeInPath(struct FNavMeshEdgeBase* Edge, struct FNavMeshPolyBase* Poly);

	// overidden to deny access to edges when we're disabled and the path doesn't incorporate a switch linked to this pylon
	virtual UBOOL CostFor( const FNavMeshPathParams& PathParams,
						 const FVector& PreviousPoint,
						 FVector& out_PathEdgePoint,
						 struct FNavMeshEdgeBase* Edge,
						 struct FNavMeshPolyBase* SourcePoly,
						 INT& out_Cost);

}


var() bool bOpen;

function PostBeginPlay()
{
	Super.PostBeginPlay();
	SetEnabled(bOpen);
}

event SetEnabled(bool bEnabled)
{
	bOpen = bEnabled;
	bForceObstacleMeshCollision = !bOpen;
}

event bool IsEnabled()
{
	return bOpen;
}


defaultproperties
{
	bNeedsCostCheck=true
	bRouteBeginPlayEvenIfStatic=true
}
