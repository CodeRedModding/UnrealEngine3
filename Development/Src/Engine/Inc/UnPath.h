/*=============================================================================
	UnPath.h: Path node creation and ReachSpec creations and management specific classes
	Copyright 1997-2002 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Steven Polge 3/97
=============================================================================*/

// NOTE - There are some assumptions about these values in UReachSpec::findBestReachable().
// It is assumed that the MINCOMMON, CROUCHEDHUMAN, and HUMAN sizes are all smaller than
// COMMON and MAXCOMMON sizes.  If this isn't the case, findBestReachable needs to be modified.
#define MAXPATHDIST		1200 // maximum distance for paths between two nodes
#define MAXPATHDISTSQ	MAXPATHDIST*MAXPATHDIST
#define	PATHPRUNING		1.2f // maximum relative length of indirect path to allow pruning of direct reachspec between two pathnodes
#define MINMOVETHRESHOLD 4.1f // minimum distance to consider an AI predicted move valid
#define SWIMCOSTMULTIPLIER 2.f // cost multiplier for paths which require swimming
#define CROUCHCOSTMULTIPLIER 1.1f // cost multiplier for paths which require crouching

//Reachability flags - using bits to save space

enum EReachSpecFlags
{
	R_WALK = 1,	//walking required
	R_FLY = 2,   //flying required 
	R_SWIM = 4,  //swimming required
	R_JUMP = 8,   // jumping required
	R_DOOR = 16,
	R_SPECIAL = 32,
	R_LADDER = 64,
	R_PROSCRIBED = 128,
	R_FORCED = 256,
	R_PLAYERONLY = 512
}; 

#define MAXSORTED 32
class FSortedPathList
{
public:
	ANavigationPoint *Path[MAXSORTED];
	INT Dist[MAXSORTED];
	int numPoints;

	FSortedPathList() { numPoints = 0; }
	ANavigationPoint* findStartAnchor(APawn *Searcher); 
	ANavigationPoint* findEndAnchor(APawn *Searcher, AActor *GoalActor, FVector EndLocation, UBOOL bAnyVisible, UBOOL bOnlyCheckVisible); 
	void addPath(ANavigationPoint * node, INT dist);
};

class FPathBuilder
{
public:
	static AScout* GetScout(ULevel *Level);
	static void DestroyScout(ULevel *Level);

private:
	static AScout *Scout;
	ULevel *Level;
	void SetPathCollision(INT bEnabled);

public:
	void definePaths (ULevel *ownerLevel,UBOOL bOnlyChanged=0);
	void undefinePaths (ULevel *ownerLevel,UBOOL bOnlyChanged=0);
	void ReviewPaths(ULevel *ownerLevel);
	void buildCover(ULevel *ownerLevel);
};

