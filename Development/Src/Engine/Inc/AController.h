/*=============================================================================
	AController.h: AI or player.
	Copyright 2000 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void Spawned();

	// Seeing and hearing checks
	int CanHear(FVector NoiseLoc, FLOAT Loudness, AActor *Other);
	void ShowSelf();
	DWORD SeePawn(APawn *Other, UBOOL bMaySkipChecks=true);
	DWORD LineOfSightTo(AActor *Other, INT bUseLOSFlag=0, FVector *chkLocation = NULL);
	void CheckEnemyVisible();
	UBOOL CanHearSound(FVector HearSource, AActor* SoundMaker, FLOAT MaxRadius);

	AActor* HandleSpecial(AActor *bestPath);
	virtual INT AcceptNearbyPath(AActor* goal);
	virtual void AdjustFromWall(FVector HitNormal, AActor* HitActor);
	void SetRouteCache(ANavigationPoint *EndPath, FLOAT StartDist, FLOAT EndDist);
	AActor* FindPath(FVector point, AActor* goal, UBOOL bWeightDetours);
	AActor* SetPath(INT bInitialPath=1);
	virtual void SetAdjustLocation(FVector NewLoc);
	virtual UBOOL LocalPlayerController();
	virtual UBOOL WantsLedgeCheck();
	virtual UBOOL StopAtLedge();
	void CheckFears();
	virtual AActor* GetViewTarget();
	virtual void UpdateEnemyInfo(APawn* AcquiredEnemy) {};

	virtual void AirSteering(float DeltaTime) {};

	virtual void PostBeginPlay();
	virtual void PostScriptDestroyed();

	// Natives.
	DECLARE_FUNCTION(execPollWaitForLanding)
	DECLARE_FUNCTION(execPollMoveTo)
	DECLARE_FUNCTION(execPollMoveToward)
	DECLARE_FUNCTION(execPollFinishRotation)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

