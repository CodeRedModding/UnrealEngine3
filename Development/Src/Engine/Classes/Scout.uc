//=============================================================================
// Scout used for path generation.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Scout extends Pawn
	native(Pawn)
	config(Game)
	notplaceable
	transient
	dependsOn(ReachSpec);

cpptext
{
	NO_DEFAULT_CONSTRUCTOR(AScout)

	virtual FVector GetSize(FName desc)
	{
		for (INT idx = 0; idx < PathSizes.Num(); idx++)
		{
			if (PathSizes(idx).Desc == desc)
			{
				return FVector(PathSizes(idx).Radius,PathSizes(idx).Height,0.f);
			}
		}
		return FVector(PathSizes(0).Radius,PathSizes(0).Height,0.f);
	}

	virtual void InitForPathing( ANavigationPoint* Start, ANavigationPoint* End )
	{
		Physics = PHYS_Walking;
		JumpZ = TestJumpZ;
		bCanWalk = 1;
		bJumpCapable = 1;
		bCanJump = 1;
		bCanSwim = 1;
		bCanClimbLadders = 1;
		bCanFly = 0;
		GroundSpeed = TestGroundSpeed;
		MaxFallSpeed = TestMaxFallSpeed;
	}

	virtual void SetPathColor(UReachSpec* ReachSpec)
	{
		FVector CommonSize = GetSize(FName(TEXT("Common"),FNAME_Find));
		if ( ReachSpec->CollisionRadius >= CommonSize.X )
		{
			FVector MaxSize = GetSize(FName(TEXT("Max"),FNAME_Find));
			if ( ReachSpec->CollisionRadius >= MaxSize.X )
			{
				ReachSpec->PathColorIndex = 2;
			}
			else
			{
				ReachSpec->PathColorIndex = 1;
			}
		}
		else
		{
			ReachSpec->PathColorIndex = 0;
		}
	}

	virtual FVector GetDefaultForcedPathSize(UReachSpec* Spec)
	{
		return GetSize(FName(TEXT("Common"),FNAME_Find));
	}

	virtual void PostBeginPlay();
	virtual void SetPrototype();

#if WITH_EDITOR

	/** returns the largest size in the PathSizes list */
	FVector GetMaxSize();


	virtual void AddSpecialPaths(INT NumPaths, UBOOL bOnlyChanged) {};
	/** updates the highest landing Z axis velocity encountered during a reach test */
	virtual void SetMaxLandingVelocity(FLOAT NewLandingVelocity)
	{
		if (-NewLandingVelocity > MaxLandingVelocity)
		{
			MaxLandingVelocity = -NewLandingVelocity;
		}
	}

	virtual UClass* GetDefaultReachSpecClass() { return DefaultReachSpecClass; }

	/**
	* Toggles collision on all actors for path building.
	*/
	virtual void SetPathCollision(UBOOL bEnabled);

	/**
	* Moves all interp actors to the path building position.
	*/
	virtual void UpdateInterpActors(UBOOL &bProblemsMoving, TArray<USeqAct_Interp*> &InterpActs);

	/**
	* Moves all updated interp actors back to their original position.
	*/
	virtual void RestoreInterpActors(TArray<USeqAct_Interp*> &InterpActs);

	/**
	* Clears all the paths and rebuilds them.
	*
	* @param	bReviewPaths	If TRUE, review paths if any were created.
	* @param	bShowMapCheck	If TRUE, conditionally show the Map Check dialog.
	* @param	bUndefinePaths	IF TRUE, paths will be undefined first
	*/
	virtual void DefinePaths( UBOOL bReviewPaths, UBOOL bShowMapCheck, UBOOL bUndefinePaths );

	/**
	* Clears all pathing information in the level.
	*/
	virtual void UndefinePaths();

	virtual void AddLongReachSpecs( INT NumPaths );

	virtual void PrunePaths(INT NumPaths);

	// interface to allow easy overides of path prune behavior (without copy+pasting ;) )
	virtual INT PrunePathsForNav(ANavigationPoint* Nav);
	// called after PrunePathsForNav is called on all pathnodes
	virtual INT SecondPassPrunePathsForNav(ANavigationPoint* Nav){return 0;}


	virtual void ReviewPaths();

	virtual void Exec( const TCHAR* Str );
	virtual void AdjustCover( UBOOL bFromDefinePaths = FALSE );
	virtual void BuildCover(  UBOOL bFromDefinePaths = FALSE );
	virtual void FinishPathBuild();

	/**
	 *	Rebuilds nav meshes
	 *	@param PassNum			Pass number given.
	 *	@param bShowMapCheck	If TRUE, conditionally show the Map Check dialog.
	 *  @param bOnlyBuildSelected if TRUE only pylons which are selected will be built
	 */
	virtual UBOOL GenerateNavMesh( UBOOL bShowMapCheck, UBOOL bOnlyBuildSelected );
	virtual void  AbortNavMeshGeneration( TArray<USeqAct_Interp*>& InterpActs );

#endif

	static AScout* GetGameSpecificDefaultScoutObject();

	// ** Navigation mesh functions follow
typedef UBOOL(APylon::*NavMashPassFunc)();
	
	/**
	 * Prepares list of NavMesh generation passes executed for every pylon
	 * @param PassList - list of passes
	 * @param GeneratorType - NavMesh generator type (check ENavigationMeshGeneratorType for details)
	 */
	virtual void  GetNavMeshPassList( TArray<NavMashPassFunc>& PassList, BYTE GeneratorType = 0 );
	// ** End navigation mesh functions

	virtual UBOOL CanDoMove( const TCHAR* Str, ANavigationPoint* Nav, INT Item = -1, UBOOL inbSeedPylon = FALSE ) { return FALSE; }
	virtual void CreateMantleEdge( struct FNavMeshPolyBase* SrcPoly, FVector& EdgeEndPt1, FVector& EdgeEndPt2, FRotator& EdgeEndRot1, FRotator& EdgeEndRot2, INT Dir, AActor* RelActor, INT RelItem ) {}

	/**
	 * NavMeshGen_IsValidGroundHit
	 * allows the scout to determien if the passed ground hit is a valid spot for navmesh to exist
	 * @param Hit - the hit to determine validity for
	 * @return - TRUE If the passed spot was valid
	 */
	virtual UBOOL NavMeshGen_IsValidGroundHit( FCheckResult& Hit );

	/**
	 * if your game adds custom edge types, you should call Register() on them in your overidden scout class here
	 */
	virtual void InitializeCustomEdgeClasses() {}

protected:
#if WITH_EDITOR
	/**
	* Builds the per-level nav lists and then assembles the world list.
	*/
	virtual void BuildNavLists();
#endif
};

struct native PathSizeInfo
{
	var Name		Desc;
	var	float		Radius,
					Height,
					CrouchHeight;
	var byte		PathColor;
};
var array<PathSizeInfo>			PathSizes;		// dimensions of reach specs to test for
var float						TestJumpZ,
								TestGroundSpeed,
								TestMaxFallSpeed,
								TestFallSpeed;

var const float MaxLandingVelocity;

var int MinNumPlayerStarts;

/** Specifies the default class to use when constructing reachspecs connecting NavigationPoints */
var class<ReachSpec> DefaultReachSpecClass;

//////////////////////////////////////////////////////////////////////////
// Navigation Mesh generation configuration parameters					//
//////////////////////////////////////////////////////////////////////////

// EdgePathColors                      - color to draw edges at the given index 
var array<Color> EdgePathColors;

// NavMeshGen_StepSize                 - Size of our expansion step. (also the size of the base square added at each step to the mesh)
var float NavMeshGen_StepSize;

// NavMeshGen_EntityHalfHeight         - half height of expansion tests done (this should be the half height of your smallest pathing entity)
var float NavMeshGen_EntityHalfHeight;

// NavMeshGen_StartingHeightOffset     - starting offset for ground checks done during each expansion step
var float NavMeshGen_StartingHeightOffset;

// NavMeshGen_MaxDropHeight            - maximum valid height for ledges to drop before no expansion is allowed
var float NavMeshGen_MaxDropHeight;

// NavMeshGen_MaxStepHeight            - maximum height to consider valid for step-ups
var float NavMeshGen_MaxStepHeight;

// NavMeshGen_VertZDeltaSnapThresh     - when two potential vert locations are within stepheight, but greater than this threshold
//                                       a line check is done to snap the new shared vert to the ground
//										 (should probably be about half of max step height)
var float NavMeshGen_VertZDeltaSnapThresh;

// NavMeshGen_MinPolyArea              - minimum area of polygons (below threshold will be culled)
var float NavMeshGen_MinPolyArea;

// NavMeshGen_BorderBackfill_CheckDist - size around each vertex to check for other verts which might be candidates for backfilling
var float NavMeshGen_BorderBackfill_CheckDist;

// NavMeshGen_MinMergeDotAreaThreshold - multiplier of NAVMESHGEN_STEP_SIZE used to determine if small area mindot or large area mindot should be used
var float NavMeshGen_MinMergeDotAreaThreshold;

// NavMeshGen_MinMergeDotSmallArea     - minimum dot product necessary for merging polys of an area below NAVMESHGEN_MERGE_DOT_AREA_THRESH
var float NavMeshGen_MinMergeDotSmallArea;

// NavMeshGen_MinMergeDotLargeArea     - minimum dot product necessary for merging polys of an area above NAVMESHGEN_MERGE_DOT_AREA_THRESH
var float NavMeshGen_MinMergeDotLargeArea;

// NavMeshGen_MaxPolyHeight		       - maximum height to check height against (should be the height of your biggest entity)
var float NavMeshGen_MaxPolyHeight;

// NavMeshGen_HeightMergeThreshold     - height threshold used when determining if two polys can be merged (e.g. if the two poly heights are within this value, they are OK to merge)
var float NavMeshGen_HeightMergeThreshold;

// NavMeshGen_EdgeCreationThreshold    - the maximum distance off projected points along paralell edges
var float NavMeshGen_EdgeMaxDelta;

// NavMeshGen_MaxGroundCheckSize       - the maximum size (used as Extent X/Y on ground check) to be used for ground checks. this is useful to allow large step sizes, while still maintaining
//										 ground check resolution.
var float NavMeshGen_MaxGroundCheckSize;

// NavMeshGen_MinEdgeLength				- minimum length for an edge.  Edges shorter than this value will be thrown out
var float NavMeshGen_MinEdgeLength;
///////////////////////////////////////////////////////////////////

// NavMeshGen_ExpansionDoObstacleMeshSimplification           - simplify the obstacle mesh with a basic poly merge??
var bool NavMeshGen_ExpansionDoObstacleMeshSimplification;

/** when this is TRUE a dashed-red line will be drawn across the gap of a one-way edge to highlight those situations */
var() bool bHightlightOneWayReachSpecs;


//**  these vars are used when automatically generating mantles.
var		float			 MinMantleFallDist;
var		float			 MaxMantleFallDist;
var		float			 MinMantleLateralDist;
var		float			 MaxMantleLateralDist;
var     float            MaxMantleFallTime;

simulated event PreBeginPlay()
{
	// make sure this scout has all collision disabled
	if (bCollideActors)
	{
		SetCollision(FALSE,FALSE);
	}
}

defaultproperties
{
	Components.Remove(Sprite)
	Components.Remove(Arrow)

	RemoteRole=ROLE_None
	AccelRate=+00001.000000
	bCollideActors=false
	bCollideWorld=false
	bBlockActors=false
	bProjTarget=false
	bPathColliding=true

	PathSizes(0)=(Desc=Human,Radius=48,Height=80)
	PathSizes(1)=(Desc=Common,Radius=72,Height=100)
	PathSizes(2)=(Desc=Max,Radius=120,Height=120)
	PathSizes(3)=(Desc=Vehicle,Radius=260,Height=120)

	EdgePathColors(0)=(R=0,G=0,B=255)
	EdgePathColors(1)=(R=0,G=255,B=0)
	EdgePathColors(2)=(R=255,G=0,B=0)
	EdgePathColors(3)=(R=0,G=255,B=255)

	TestJumpZ=420
	TestGroundSpeed=600
	TestMaxFallSpeed=2500
	TestFallSpeed=1200
	MinNumPlayerStarts=1
	DefaultReachSpecClass=class'Engine.Reachspec'


	NavMeshGen_StepSize=30.0
	NavMeshGen_MaxGroundCheckSize=30.0f
	NavMeshGen_EntityHalfHeight=72.0
	NavMeshGen_StartingHeightOffset=65.0
	NavMeshGen_MaxDropHeight=60.0
	NavMeshGen_MaxStepHeight=35.0
	NavMeshGen_VertZDeltaSnapThresh=20.0
	NavMeshGen_MinPolyArea=25
	NavMeshGen_BorderBackfill_CheckDist=70.0
	NavMeshGen_MinMergeDotAreaThreshold=2.0
	NavMeshGen_MinMergeDotSmallArea=0.0
	NavMeshGen_MinMergeDotLargeArea=0.95
	NavMeshGen_MaxPolyHeight=160.0
	NavMeshGen_HeightMergeThreshold=10
	NavMeshGen_EdgeMaxDelta=2.0
	NavMeshGen_MinEdgeLength=7.0
	NavMeshGen_ExpansionDoObstacleMeshSimplification=TRUE
}
