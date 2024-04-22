/**
 * Interface for (primarily) dynamic objects which should affect the navigation mesh at runtime.
 *
 * Used to split the navmesh poly with the shape provided.  And then Interface_NavMeshPathObject will do the
 * work for determining whether or not a path is valid IF you have PreserveInternalPolys returning TRUE.  
 * Otherwise those polys have been removed from the navmesh (e.g. a giant metal cube landing on the ground).
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
interface Interface_NavMeshPathObstacle
	native(AI);

// enum used to describe what this path obstacle did WRT to edges being added for it
enum EEdgeHandlingStatus
{
	EHS_AddedBothDirs, // added edges for both directions (nothing further needed)
	EHS_Added0to1,     // added edge from source poly to dest poly (poly 0 -> poly 1)
	EHS_Added1to0,    // added edge from dest poly to source poly (Poly 1 -> Poly 0)
	EHS_AddedNone     // didn't add anything
};

cpptext
{
	/**
	 * this will register the passed shape/bounds with the passed polys 
	 * @param BoundingShape - bounding shape of the obstacle
 	 * @param Bounds - bounds of the bounding shape (for octree queries)	 
	 * @param Polys - polys to register this obstacle with 
	 * @return - TRUE If registration was succesful 
	 */
	UBOOL RegisterObstacleWithPolys( const TArray<FVector>& BoundingShape, const TArray<FNavMeshPolyBase*>& Polys);

	/**
	 * Rebuilds all submesh data for any submeshes within the passed top level polygons
	 * @param Polys - polys to trigger rebuild for
	 */
	static void TriggerRebuildForPassedTLPolys( const TArray<FNavMeshPolyBase*>& Polys );

	/**
	 * this is called on polys which have just had all obstacles cleared and won't get a normal build step
	 * and thus need to have edges created to adjacent sub-meshes
	 */
	UBOOL DoEdgeFixupForNewlyClearedPolys(const TArray<FNavMeshPolyBase*> PolysThatNeedFixup);

	/**
	 * given a list of pylons will update all the obstacles that need updating within it
	 * also does post steps after update is finished
	 * @param Pylons - list of pylons to update obstacles for
	 */
	static void UpdateAllDynamicObstaclesInPylonList(TArray<APylon*>& Pylons);

	/**
	 * this will register this shape with the obstacle mesh, indicating it should be considered
	 * when generating paths
	 * @return - TRUE If registration was successful
	 */
	UBOOL RegisterObstacleWithNavMesh();

	/**
	 * this will register the passed list of obstacles, and then perform a build at the end.
	 * useful for registering a big list of obstacles all at once and paying much less cost (mesh doesn't have to be rebuilt each registration, only at the end)
	 * @param ObstaclesToRegister - list of obstacles to register
	 * @return - TRUE if registration and build was successful
	 */
	static UBOOL RegisterObstacleListWithNavMesh(TArray<IInterface_NavMeshPathObstacle*>& Obstacles);

	/**
	 * this will remove this shape from the obstacle mesh, indicating it is no longer relevant to 
	 * generating paths
	 * @return TRUE if unregistration was successful
	 */
	UBOOL UnregisterObstacleWithNavMesh();

	/** 
	 * called when the owner of this interface is being unloaded or destroyed and this obstacle needs to be cleaned up
	 */
	virtual void CleanupOnRemoval();

	/**
	 * this function should populate out_polyshape with a list of verts which describe this object's 
	 * convex bounding shape
	 * (verts should be clockwise wound)
	 * @param out_PolyShape - output array which holds the vertex buffer for this obstacle's bounding polyshape
	 * @param ShapeIdx - index of the shape being requested
	 * @return TRUE if this object should block things right now (FALSE means this obstacle shouldn't affect the mesh)
	 */
	virtual UBOOL GetBoundingShape(TArray<FVector>& out_PolyShape,INT ShapeIdx)=0;

	/**
	 * return the number of shapes this obstacle needs to represent itself 
	 */
	virtual INT GetNumBoundingShapes(){ return 1; }

	/**
	 * when TRUE polys internal to this obstacle will be preserved, but still split. (useful for things like cost volumes that 
	 * need to adjust cost but not completely destroy parts of the mesh
	 * @return TRUE if polys should be preserved internal to this obstacle
	 */
	virtual UBOOL PreserveInternalPolys() { return FALSE; }

	/**
	 * This function is called when an edge is going to be added connecting a polygon internal to this obstacle to another polygon which is not
	 * Default behavior just a normal edge, override to add special costs or behavior (e.g. link a pathobject to the obstacle)
	 * @param Status - current status of edges (e.g. what still needs adding)	 
	 * @param inV1 - vertex location of first vert in the edge
	 * @param inV2 - vertex location of second vert in the edge
	 * @param ConnectedPolys - the polys this edge links
	 * @param bEdgesNeedToBeDynamic - whether or not added edges need to be dynamic (e.g. we're adding edges between meshes)
	 * @param PolyAssocatedWithThisPO - the index into the connected polys array parmaeter which tells us which poly from that array is associated with this path object
	 * @(optional) param SupportedEdgeWidth - width of unit that this edge supports, defaults to -1.0f meaning the length of the edge itself will be used
	 * @(optional) param EdgeGroupID - ID of the edgegroup this edge is a part of (defaults to no group)
	 * @return returns an enum describing what just happened (what actions did we take) - used to determien what accompanying actions need to be taken 
	 *         by other obstacles and calling code
	 */
	virtual EEdgeHandlingStatus AddObstacleEdge( EEdgeHandlingStatus Status, const FVector& inV1, const FVector& inV2, TArray<FNavMeshPolyBase*>& ConnectedPolys, UBOOL bEdgesNeedToBeDynamic, INT PolyAssocatedWithThisPO, FLOAT SupportedEdgeWidth=-1.0f, BYTE EdgeGroupID=MAXBYTE);

	/**
	 * this function is called after a top level mesh's submeshes have all been built (e.g. at the end of UNavigationMeshBase::UpdateDynamicObstacles) 
	 * and that mesh is affected by this obstacle
	 * and it gives this obstacle a chance to do any extra work after the mesh is built (e.g. add specialized edges)
	 * @param MeshThatWasUpdated - the top level navmesh that just had all submeshes built
	 */
	virtual void PostSubMeshUpdateForTopLevelMesh(UNavigationMeshBase* MeshThatWasUpdated) {}

	/**
	 * For debugging.  Verifies that this pathobject is still alive and well and not orphaned or deleted
	 * @return - TRUE If this path object is in good working order
	 */
	virtual UBOOL VerifyObstacle()
	{
		return FALSE;
	}
}
