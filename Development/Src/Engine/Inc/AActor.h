/*=============================================================================
	AActor.h.
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

	// Constructors.
	AActor() {}
	void Destroy();

	// UObject interface.
	virtual INT* GetOptimizedRepList( BYTE* InDefault, FPropertyRetirement* Retire, INT* Ptr, UPackageMap* Map, UActorChannel* Channel );
	void ProcessEvent( UFunction* Function, void* Parms, void* Result=NULL );
	void ProcessState( FLOAT DeltaSeconds );
	UBOOL ProcessRemoteFunction( UFunction* Function, void* Parms, FFrame* Stack );
	void Serialize( FArchive& Ar );
	void InitExecution();
	void PreEditChange();
	void PostEditChange(UProperty* PropertyThatChanged);
	virtual void PreEditUndo();
	virtual void PostEditUndo();
	void PostLoad();
	void NetDirty(UProperty* property);
	virtual void ExecutingBadStateCode(FFrame& Stack);

	// AActor interface.
	class ULevel* GetLevel() const;
	virtual APawn* GetPlayerPawn() const {return NULL;}
	virtual UBOOL IsPlayer() {return false;}
	virtual UBOOL IgnoreBlockingBy( const AActor *Other) const;
	UBOOL IsOwnedBy( const AActor *TestOwner ) const;
	UBOOL IsBlockedBy( const AActor* Other, const UPrimitiveComponent* Primitive ) const;
	UBOOL IsInZone( const AZoneInfo* Other ) const;
	UBOOL IsBasedOn( const AActor *Other ) const;
	AActor* GetBase() const;

	// Editor specific
	UBOOL IsHiddenEd();
	virtual UBOOL IsSelected()
	{
		return (UObject::IsSelected() && !bDeleteMe);
	}
	
	virtual FLOAT GetNetPriority( FVector& ViewPos, FVector& ViewDir, AActor* Sent, FLOAT Time, FLOAT Lag );
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual void PostEditMove();
	virtual void PostRename();
	virtual void PostEditLoad() {}
	virtual void PreRaytrace() {}
	virtual void PostRaytrace() {}
	virtual void Spawned() {}
	virtual void PreNetReceive();
	virtual void PostNetReceive();
	virtual void PostNetReceiveLocation();
	virtual UBOOL ShouldTickInEntry() { return false; }

	// Rendering info.

	virtual FMatrix LocalToWorld() const
	{
#if 0
		FTranslationMatrix LToW( -PrePivot );
		FScaleMatrix TempScale( DrawScale3D * DrawScale );
		FRotationMatrix TempRot( Rotation );
		FTranslationMatrix TempTrans( Location );
		LToW *= TempScale;
		LToW *= TempRot;
		LToW *= TempTrans;
		return LToW;
#else
		FMatrix Result;

		FLOAT	SR	= GMath.SinTab(Rotation.Roll),
				SP	= GMath.SinTab(Rotation.Pitch),
				SY	= GMath.SinTab(Rotation.Yaw),
				CR	= GMath.CosTab(Rotation.Roll),
				CP	= GMath.CosTab(Rotation.Pitch),
				CY	= GMath.CosTab(Rotation.Yaw);
		
		FLOAT	LX	= Location.X,
				LY	= Location.Y,
				LZ	= Location.Z,
				PX	= PrePivot.X,
				PY	= PrePivot.Y,
				PZ	= PrePivot.Z;
				
		FLOAT	DX	= DrawScale3D.X * DrawScale,
				DY	= DrawScale3D.Y * DrawScale,
				DZ	= DrawScale3D.Z * DrawScale;
				
		Result.M[0][0] = CP * CY * DX;
		Result.M[0][1] = CP * DX * SY;
		Result.M[0][2] = DX * SP;
		Result.M[0][3] = 0.f;

		Result.M[1][0] = DY * ( CY * SP * SR - CR * SY );
		Result.M[1][1] = DY * ( CR * CY + SP * SR * SY );
		Result.M[1][2] = -CP * DY * SR;
		Result.M[1][3] = 0.f;

		Result.M[2][0] = -DZ * ( CR * CY * SP + SR * SY );
		Result.M[2][1] =  DZ * ( CY * SR - CR * SP * SY );
		Result.M[2][2] = CP * CR * DZ;
		Result.M[2][3] = 0.f;

		Result.M[3][0] = LX - CP * CY * DX * PX + CR * CY * DZ * PZ * SP - CY * DY * PY * SP * SR + CR * DY * PY * SY + DZ * PZ * SR * SY;
		Result.M[3][1] = LY - (CR * CY * DY * PY + CY * DZ * PZ * SR + CP * DX * PX * SY - CR * DZ * PZ * SP * SY + DY * PY * SP * SR * SY);
		Result.M[3][2] = LZ - (CP * CR * DZ * PZ + DX * PX * SP - CP * DY * PY * SR);
		Result.M[3][3] = 1.f;

		return Result;
#endif
	}
	virtual FMatrix WorldToLocal() const
	{
		return	FTranslationMatrix(-Location) *
				FInverseRotationMatrix(Rotation) *
				FScaleMatrix(FVector( 1.f / DrawScale3D.X, 1.f / DrawScale3D.Y, 1.f / DrawScale3D.Z) / DrawScale) *
				FTranslationMatrix(PrePivot);
	}

	FVector GetCylinderExtent() const;
	void GetBoundingCylinder(FLOAT& CollisionRadius, FLOAT& CollisionHeight);

	AActor* GetTopOwner();
	UBOOL IsPendingKill() {return bDeleteMe;}
	virtual void PostScriptDestroyed() {} // C++ notification that the script Destroyed() function has been called.
	
	// AActor collision functions.
	virtual UBOOL ShouldTrace(UPrimitiveComponent* Primitive,AActor *SourceActor, DWORD TraceFlags);
	UBOOL IsOverlapping( AActor *Other, FCheckResult* Hit=NULL );

	FBox GetComponentsBoundingBox(UBOOL bNonColliding=0);

	// AActor general functions.
	void BeginTouch(AActor *Other, const FVector &HitLocation, const FVector &HitNormal);
	void EndTouch(AActor *Other, UBOOL NoNotifySelf);
	void SetOwner( AActor *Owner );
	UBOOL IsBrush()       const;
	UBOOL IsStaticBrush() const;
	UBOOL IsVolumeBrush() const;
	UBOOL IsEncroacher() const;
	UBOOL IsShapeBrush()       const;
	
	UBOOL FindInterpMoveTrack(class UInterpTrackMove** MoveTrack, class UInterpTrackInstMove** MoveTrackInst, class USeqAct_Interp** OutSeq);

	/**
	 * Returns True if an actor cannot move or be destroyed during gameplay, and can thus cast and receive static shadowing.
	 */
	UBOOL HasStaticShadowing() const { return bStatic || (bNoDelete && !bMovable); }

	virtual void NotifyBump(AActor *Other, const FVector &HitNormal);
	void SetCollision( UBOOL NewCollideActors, UBOOL NewBlockActors);
	void SetCollisionSize( FLOAT NewRadius, FLOAT NewHeight );
	void SetDrawScale( FLOAT NewScale);
	void SetDrawScale3D( FVector NewScale3D);
	void SetPrePivot( FVector NewPrePivot );
	virtual void SetBase(AActor *NewBase, FVector NewFloor = FVector(0,0,1), int bNotifyActor=1, USkeletalMeshComponent* SkelComp=NULL, FName BoneName=NAME_None );
	void UpdateTimers(FLOAT DeltaSeconds);
	virtual UBOOL CheckOwnerUpdated();
	virtual void TickAuthoritative( FLOAT DeltaSeconds );
	virtual void TickSimulated( FLOAT DeltaSeconds );
	virtual void TickSpecial( FLOAT DeltaSeconds );
	virtual UBOOL PlayerControlled();
	virtual UBOOL IsNetRelevantFor(APlayerController* RealViewer, AActor* Viewer, FVector SrcLocation );
	virtual UBOOL DelayScriptReplication(FLOAT LastFullUpdateTime) { return false; }

	// Level functions
	virtual void SetZone( UBOOL bTest, UBOOL bForceRefresh );
	virtual void SetVolumes();
	virtual void SetVolumes(const TArray<class AVolume*>& Volumes);
	virtual void PostBeginPlay();

	// Physics functions.
	virtual void setPhysics(BYTE NewPhysics, AActor *NewFloor = NULL, FVector NewFloorV = FVector(0,0,1) );
	void FindBase();
	virtual void performPhysics(FLOAT DeltaSeconds);
	virtual void physProjectile(FLOAT deltaTime, INT Iterations);
	virtual void BoundProjectileVelocity();
	virtual void processHitWall(FVector HitNormal, AActor *HitActor);
	virtual void processLanded(FVector HitNormal, AActor *HitActor, FLOAT remainingTime, INT Iterations);
	virtual void physFalling(FLOAT deltaTime, INT Iterations);
	void physicsRotation(FLOAT deltaTime);
	int fixedTurn(int current, int desired, int deltaRate);
	inline void TwoWallAdjust(const FVector &DesiredDir, FVector &Delta, const FVector &HitNormal, const FVector &OldHitNormal, FLOAT HitTime)
	{
		if ((OldHitNormal | HitNormal) <= 0) //90 or less corner, so use cross product for dir
		{
			FVector NewDir = (HitNormal ^ OldHitNormal);
			NewDir = NewDir.SafeNormal();
			Delta = (Delta | NewDir) * (1.f - HitTime) * NewDir;
			if ((DesiredDir | Delta) < 0)
				Delta = -1 * Delta;
		}
		else //adjust to new wall
		{
			Delta = (Delta - HitNormal * (Delta | HitNormal)) * (1.f - HitTime);
			if ((Delta | DesiredDir) <= 0)
				Delta = FVector(0,0,0);
		}
	}
	UBOOL moveSmooth(FVector Delta);
	virtual FRotator FindSlopeRotation(FVector FloorNormal, FRotator NewRotation);
	void UpdateRelativeRotation();
	void GetNetBuoyancy(FLOAT &NetBuoyancy, FLOAT &NetFluidFriction);
	virtual void SmoothHitWall(FVector HitNormal, AActor *HitActor);
	virtual void stepUp(const FVector& GravDir, const FVector& DesiredDir, const FVector& Delta, FCheckResult &Hit);
	virtual UBOOL ShrinkCollision(AActor *HitActor, const FVector &StartLocation);
	virtual void physInterpolating(FLOAT DeltaTime);
	virtual void physInterpolatingCorrectPosition( const FVector &NewLocation );

	virtual void physRigidBody(FLOAT DeltaTime);
	virtual void physArticulated(FLOAT DeltaTime);

	virtual void InitRBPhys();
	virtual void TermRBPhys();
	void UpdateRBPhysKinematicData();

#ifdef WITH_NOVODEX
	class NxActor* GetNxActor(FName BoneName = NAME_None);
	virtual void ModifyNxActorDesc(NxActorDesc& ActorDesc) {}
	virtual void PostInitRigidBody(NxActor* nActor) {}
	void SyncActorToRBPhysics();
#endif // WITH_NOVODEX

	// AI functions.
	void CheckNoiseHearing(FLOAT Loudness);
	int TestCanSeeMe(class APlayerController *Viewer);
	virtual INT AddMyMarker(AActor *S) { return 0; };
	virtual void ClearMarker() {};
	virtual AActor* AssociatedLevelGeometry();
	virtual UBOOL HasAssociatedLevelGeometry(AActor *Other);
	virtual FVector GetDestination();
	FVector SuggestFallVelocity(FVector Dest, FVector Start, FLOAT XYSpeed, FLOAT BaseZ, FLOAT JumpZ, FLOAT MaxXYSpeed);

	void ChartData(const FString& DataName, FLOAT DataValue);


	// Special editor behavior
	AActor* GetHitActor();
	virtual void CheckForErrors();

	// path creation
	virtual void PrePath() {};
	virtual void PostPath() {};

	//FIXMESTEVE - remove all these, superceded by GetA*()
	virtual UBOOL IsABrush()  {return false;}
	virtual UBOOL IsAVolume() {return false;}
    virtual UBOOL IsAPlayerController() { return false; }
    virtual UBOOL IsAPawn() { return false; }
    virtual UBOOL IsAProjectile() { return false; }
	virtual UBOOL IsAShape()  {return false;}

	virtual APlayerController* GetAPlayerController() { return NULL; }
    virtual APawn* GetAPawn() { return NULL; }
    virtual class AProjectile* GetAProjectile() { return NULL; }

    virtual APlayerController* GetTopPlayerController()
    {
        AActor* TopActor = GetTopOwner();
        return (TopActor && TopActor->IsAPlayerController()) ? Cast<APlayerController>(TopActor) : NULL;
    }

	virtual void ClearComponents();
	virtual void UpdateComponents();

	virtual void CacheLighting();
	virtual void InvalidateLightingCache();

	virtual UBOOL ActorLineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	// Natives.
	DECLARE_FUNCTION(execPollSleep)
	DECLARE_FUNCTION(execPollFinishAnim)

	// Kismet
	USequenceEvent* GetEventOfClass(UClass *inEventClass,USequenceEvent* inLastEvent = NULL);

	// Matinee
	void GetInterpPropertyNames(TArray<FName> &outNames);
	FLOAT* GetInterpPropertyRef(FName inName);

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

