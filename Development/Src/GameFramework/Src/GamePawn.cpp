/**
*
* Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
*/

#include "GameFramework.h"
#include "UnNet.h"



IMPLEMENT_CLASS(AGamePawn);



INT* AGamePawn::GetOptimizedRepList( BYTE* Recent, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel )
{
	Ptr = Super::GetOptimizedRepList(Recent,Retire,Ptr,Map,Channel);

	checkSlow(StaticClass()->ClassFlags & CLASS_NativeReplication);	


	if( bNetDirty )
	{
		if( !bNetOwner || bDemoRecording )
		{
			DOREP(GamePawn,bLastHitWasHeadShot);
		}
	}

	return Ptr;
}
