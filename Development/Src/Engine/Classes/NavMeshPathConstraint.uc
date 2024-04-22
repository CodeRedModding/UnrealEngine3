/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class NavMeshPathConstraint extends Object
	native(AI);

cpptext
{
	/**
	 * EvaluatePath - this function is called for every A* successor edge.  This gives constraints a chance
	 * to both modify the heuristic cost (h), the computed actual cost (g), as well as deny use of this edge altogether
	 * @param Edge - the successor candidate edge
	 * @param PredecessorEdge - Edge we traversed from to reach `Edge` 
	 * @param SrcPoly - the poly we are expanding from (e.g. the poly from which we want to traverse the Edge)
	 * @param DestPoly - The poly we're trying to traverse to (from SrcPoly)
	 * @param PathParams - the cached pathing parameters of the pathing entity 
	 * @param out_PathCost - the computed path cost of this edge (the current value is supplied as input, and can be modified in this function) 
	 *                       (this constitutes G of the pathfinding heuristic function F=G+H)
	 * @param out_HeuristicCost - the heuristic cost to be applied to this edge (the current heuristic is supplied as input, and can be modified in this function)
	 *                          (this constitutes H of the pathfindign heuristic function F=G+H)
	 * @param EdgePoint - the point on the edge that was used for cost calculations, useful starting point if computing distance to something
	 * @return - TRUE if this edge is a valid successor candidate and should be added to the open list, FALSE if it should be thrown out
	 */
	virtual UBOOL EvaluatePath( FNavMeshEdgeBase* Edge,
								FNavMeshEdgeBase* PredecessorEdge,
								FNavMeshPolyBase* SrcPoly,
								FNavMeshPolyBase* DestPoly,
								const FNavMeshPathParams& PathParams,								
								INT& out_PathCost, INT& out_HeuristicCost,
								const FVector& EdgePoint );
}

/** Next constraint in the list */
var NavMeshPathConstraint NextConstraint;

/** >> Debugging vars - keep track of stats about what we threw out, etc.. */

/** number of nodes this constraint has processed */
var int NumNodesProcessed;
/** number of nodes this constraint has returned FALSE for */
var int NumThrownOutNodes;
/** total cost added by this constraint to the saved *real* cost of nodes */
var float AddedDirectCost;
/** total cost added by this constraint to the heuristic cost of nodes */
var float AddedHeuristicCost;

// called when this object is about to be re-used from the cache
event Recycle()
{
	NextConstraint = None;
	
	NumThrownOutNodes=0;
	AddedDirectCost=0;
	AddedHeuristicCost=0;
	NumNodesProcessed=0;
}

event String GetDumpString()
{
	return String(self);
}

defaultproperties
{
}
