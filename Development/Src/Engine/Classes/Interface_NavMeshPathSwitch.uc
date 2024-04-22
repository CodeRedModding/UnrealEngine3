/**
 * Interface for path objects which need to interface with the navmesh
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
interface Interface_NavMeshPathSwitch extends Interface_NavMeshPathObject
	native(AI);


cpptext
{
	// cost slightly more then normal edges so other edges are preferred unless this one is needed
	virtual INT CostFor( const FNavMeshPathParams& PathParams, const FVector& PreviousPoint, FVector& out_PathEdgePoint, FNavMeshPathObjectEdge* Edge, FNavMeshPolyBase* SourcePoly );

	// call the switch's support function 
	virtual UBOOL Supports( const FNavMeshPathParams& PathParams,
							struct FNavMeshPolyBase* CurPoly,
							struct FNavMeshPathObjectEdge* Edge,
							struct FNavMeshEdgeBase* PredecessorEdge);

	// if bot is in the same poly as the trigger, go to the trigger itself
	virtual UBOOL GetEdgeDestination( const FNavMeshPathParams& PathParams,
										FLOAT EntityRadius,
										const FVector& InfluencePosition,
										const FVector& EntityPosition,
										FVector& out_EdgeDest,
										struct FNavMeshPathObjectEdge* Edge,
										UNavigationHandle* Handle);

	// overidden to activate the switch when the bot needs to 
	virtual UBOOL PrepareMoveThru( class IInterface_NavigationHandle* Interface,
									FVector& out_MovePt,
									struct FNavMeshPathObjectEdge* Edge );

	/**
	 * called to allow this PO to draw custom stuff for edges linked to it
	 * @param DRSP          - the sceneproxy we're drawing for
	 * @param DrawOffset    - offset from the actual location we should be drawing 
	 * @param Edge          - the edge we're drawing
	 * @return - whether this PO is doing custom drawing for the passed edge (FALSE indicates the default edge drawing functionality should be used)
	 */
	virtual UBOOL DrawEdge( FDebugRenderSceneProxy* DRSP, FColor C, FVector DrawOffset, FNavMeshPathObjectEdge* Edge );

	/**
	 * called after edge creation is complete for each pylon to allow this PO to add edges for itself
	 * @param Py - the pylon which we are creating edges for
	 */
	virtual void CreateEdgesForPathObject( APylon* Py );

	// returns the spot that an AI should run to to operate this switch
	virtual FVector GetDestination(FLOAT EntityRadius){return FVector(0.f);}

	// returns TRUE if the passed bot is able to operate this switch
	virtual UBOOL CanBotUseThisSwitch(AAIController* AI){return TRUE;}

	// returns TRUE if this switch is 'open' in that the fence/gate it controls is pathable right now
	virtual UBOOL IsSwitchOpen(){return TRUE;}

	// returns TRUE if this switch is linked (e.g. opens) the passed switchable pylon
	virtual UBOOL IsLinkedTo(AAISwitchablePylon* Py){return FALSE;}

	virtual INT GetNumLinkedPylons() const{return 0;}
	virtual class AAISwitchablePylon* GetLinkedPylonAtIdx(INT Idx){return NULL;}
}



// called to tell the AI to do wahtever it needs to to activate this switch
event bool AIActivateSwitch(AIController AI);


