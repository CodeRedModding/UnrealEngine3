/**
 * this object's sole purpose is to bind the lifetime of itself with that of a pathobstacle.. such that when this
 * object is garbage collected the path obstacle is unregistered. Only the object implementing Interface_navMeshpathObstacle should
 * reference this, so that the lifetime of this object is direclty coupled with that object
 * 
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AutoNavMeshPathObstacleUnregister extends Object
	native(AI);

var native Interface_NavMeshPathObstacle PathObstacleRef;

cpptext
{
	virtual void BeginDestroy()
	{
		Super::BeginDestroy();
		if( PathObstacleRef.GetInterface() != NULL )
		{
			PathObstacleRef->UnregisterObstacleWithNavMesh();
		}
	}
};