/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
interface Interface_RVO
	native(AI);

cpptext
{
	virtual FLOAT   GetAvoidRadius()=0;
	virtual INT     GetInfluencePriority()=0;
	virtual UBOOL   IsActiveObstacle()=0;
	virtual FColor  GetDebugAgentColor()=0;

	FORCEINLINE AActor* GetActor() 
	{ 
		return Cast<AActor>(GetUObjectInterfaceInterface_RVO()); 
	}
	FORCEINLINE FVector GetLocation() 
	{ 
		return GetActor()->Location;
	}
	FORCEINLINE FVector GetVelocity() 
	{ 
		return GetActor()->Velocity;
	}

	virtual void GetVelocityObstacleStats( TArray<FVelocityObstacleStat>& out_Array, AActor* RelActor );
}
