/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
class NavMeshPath_WithinDistanceEnvelope extends NavMeshPathConstraint
	native(AI);

cpptext
{
	// Interface
	virtual UBOOL EvaluatePath( FNavMeshEdgeBase* Edge, FNavMeshEdgeBase* PredecessorEdge, FNavMeshPolyBase* SrcPoly, FNavMeshPolyBase* DestPoly, const FNavMeshPathParams& PathParams, INT& out_PathCost, INT& out_HeuristicCost, const FVector& EdgePoint );
}

/** outer distance of envelope (distance from test actor) */
var() float	MaxDistance;

/** inner distance of envelope (distance from test actor) */
var() float MinDistance;

/** if this is on instead of throwing out nodes outside traversal distance they will be gradiently penalized the further out they are */
var() bool bSoft;
/** when a path exceeds specified traversal distance this penalty will be applied, and scaled up depending on how far outside the dist it is */
var() float SoftStartPenalty;

var() vector EnvelopeTestPoint; 

/** when bSoft is false, should we throw out nodes whose start and end are both outside the envelope? */
var() bool bOnlyThrowOutNodesThatLeaveEnvelope;

static function bool StayWithinEnvelopeToLoc( NavigationHandle NavHandle, vector InEnvelopeTestPoint, float InMaxDistance, float InMinDistance, bool bInSoft=true, optional float InSoftStartPenalty=-1.f, optional bool bOnlyTossOutSpecsThatLeave )
{
	local NavMeshPath_WithinDistanceEnvelope Con;

	if( NavHandle != None )
	{
		Con = NavMeshPath_WithinDistanceEnvelope(NavHandle.CreatePathConstraint(default.class));
		if( Con != None )
		{
			Con.EnvelopeTestPoint = InEnvelopeTestPoint;
			Con.bSoft = bInSoft;
			Con.MaxDistance = InMaxDistance;
			Con.MinDistance = InMinDistance;
			Con.bOnlyThrowOutNodesThatLeaveEnvelope = bOnlyTossOutSpecsThatLeave;
			if(InSoftStartPenalty > -1.f)
			{
				Con.SoftStartPenalty = InSoftStartpenalty;
			}
			NavHandle.AddPathConstraint( Con );
			return TRUE;
		}
	}

	return FALSE;
}

function Recycle()
{
	Super.Recycle();
	MaxDistance=default.maxdistance;
	MinDistance=default.MinDistance;
	bSoft=default.bSoft;
	SoftStartPenalty=default.SoftStartPenalty;
	EnvelopeTestPoint=default.EnvelopeTestPoint;
	bOnlyThrowOutNodesThatLeaveEnvelope=default.bOnlyThrowOutNodesThatLeaveEnvelope;
}

defaultproperties
{
	bSoft=true
	bOnlyThrowOutNodesThatLeaveEnvelope=false
	SoftStartPenalty=320.0f
}

