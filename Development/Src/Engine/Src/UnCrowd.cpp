/**
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/
#include "EnginePrivate.h"
#include "EngineSequenceClasses.h"
#include "EngineAnimClasses.h"
#include "EngineAIClasses.h"

IMPLEMENT_CLASS(ACrowdAgentBase);
IMPLEMENT_CLASS(ACrowdPopulationManagerBase);
IMPLEMENT_CLASS(UInterface_RVO);

/** AGameCrowdAgent IMPLEMENT Interface_NavigationHandle */

void ACrowdAgentBase::SetupPathfindingParams( FNavMeshPathParams& out_ParamCache )
{
	VERIFY_NAVMESH_PARAMS(9);

	out_ParamCache.bAbleToSearch=TRUE;
	out_ParamCache.SearchExtent=FVector(1,1,1);
	out_ParamCache.SearchLaneMultiplier = 0.f;
	out_ParamCache.SearchStart=Location;
	out_ParamCache.MaxDropHeight=0.f;
	out_ParamCache.bCanMantle=FALSE;
	out_ParamCache.bNeedsMantleValidityTest = FALSE;
	out_ParamCache.MinWalkableZ = 0.7f;
	out_ParamCache.MaxHoverDistance = -1.f;
}


