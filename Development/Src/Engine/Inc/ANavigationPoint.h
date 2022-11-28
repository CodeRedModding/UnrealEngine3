/*=============================================================================
	ANavigationPoint.h: Class functions residing in the ANavigationPoint class.
	Copyright 2000-2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	virtual INT ProscribedPathTo(ANavigationPoint *Dest);
	virtual void addReachSpecs(class AScout *Scout, UBOOL bOnlyChanged=0);
	virtual void InitForPathFinding() {};
	virtual void ClearPaths();
	virtual void FindBase();
	void PostEditMove();
	void Spawned();
	void Destroy();
	void CleanUpPruned();
	INT PrunePaths();
	UBOOL FindAlternatePath(UReachSpec* StraightPath, INT AccumulatedDistance);
	UReachSpec* GetReachSpecTo(ANavigationPoint *Nav);
	UBOOL ShouldBeBased();
	void CheckForErrors();
	virtual UBOOL ReviewPath(APawn* Scout);
	UBOOL CanReach(ANavigationPoint *Dest, FLOAT Dist, UBOOL bUseFlag);
	virtual void CheckSymmetry(ANavigationPoint* Other) {};
	virtual void ClearForPathFinding();
	virtual INT AddMyMarker(AActor *S);
    virtual class APickupFactory* GetAPickupFactory() { return NULL; } 
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	virtual void SetVolumes();
	virtual void ForcePathTo(ANavigationPoint *Nav, AScout *Scout = NULL);
	virtual void ProscribePathTo(ANavigationPoint *Nav, AScout *Scout = NULL);
