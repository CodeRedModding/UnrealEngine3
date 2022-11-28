//=============================================================================
// NavigationPoint.
//
// NavigationPoints are organized into a network to provide AIControllers
// the capability of determining paths to arbitrary destinations in a level
//
//=============================================================================
class NavigationPoint extends Actor
	hidecategories(Lighting,LightColor,Force)
	native;

//------------------------------------------------------------------------------
// NavigationPoint variables

var transient bool bEndPoint;	// used by C++ navigation code
var transient bool bTransientEndPoint; // set right before a path finding attempt, cleared afterward.
var transient bool bHideEditorPaths;	// don't show paths to this node in the editor
var transient bool bCanReach;		// used during paths review in editor

var bool taken;					// set when a creature is occupying this spot
var() bool bBlocked;			// this path is currently unuseable
var() bool bPropagatesSound;	// this navigation point can be used for sound propagation (around corners)
var() bool bOneWayPath;			// reachspecs from this path only in the direction the path is facing (180 degrees)
var() bool bNeverUseStrafing;	// shouldn't use bAdvancedTactics going to this point
var() bool bAlwaysUseStrafing;	// shouldn't use bAdvancedTactics going to this point
var const bool bForceNoStrafing;// override any LD changes to bNeverUseStrafing
var const bool bAutoBuilt;		// placed during execution of "PATHS BUILD"
var	bool bSpecialMove;			// if true, pawn will call SuggestMovePreparation() when moving toward this node
var bool bNoAutoConnect;		// don't connect this path to others except with special conditions (used by LiftCenter, for example)
var	const bool	bNotBased;		// used by path builder - if true, no error reported if node doesn't have a valid base
var const bool  bPathsChanged;	// used for incremental path rebuilding in the editor
var bool		bDestinationOnly; // used by path building - means no automatically generated paths are sourced from this node
var	bool		bSourceOnly;	// used by path building - means this node is not the destination of any automatically generated path
var bool		bSpecialForced;	// paths that are forced should call the SpecialCost() and SuggestMovePreparation() functions
var bool		bMustBeReachable;	// used for PathReview code
var bool		bBlockable;		// true if path can become blocked (used by pruning during path building)
var	bool		bFlyingPreferred;	// preferred by flying creatures
var bool		bMayCausePain;		// set in C++ if in PhysicsVolume that may cause pain
var bool	bReceivePlayerToucherDiedNotify;
var bool bAlreadyVisited;	// internal use
var() bool 	bVehicleDestination;	// if true, forced paths to this node will have max width to accomodate vehicles
var() bool bMakeSourceOnly;

var const array<ReachSpec> PathList; //index of reachspecs (used by C++ Navigation code)
/** List of navigation points to prevent paths being built to */
var array<NavigationPoint> ProscribedPaths;
/** List of navigation points to force paths to be built to */
var array<NavigationPoint> ForcedPaths;
var int visitedWeight;
var const int bestPathWeight;
var const NavigationPoint nextNavigationPoint;
var const NavigationPoint nextOrdered;	// for internal use during route searches
var const NavigationPoint prevOrdered;	// for internal use during route searches
var const NavigationPoint previousPath;
var int cost;					// added cost to visit this pathnode
var() int ExtraCost;			// Extra weight added by level designer
var transient int TransientCost;	// added right before a path finding attempt, cleared afterward.
var	transient int FearCost;		// extra weight diminishing over time (used for example, to mark path where bot died)

var DroppedPickup	InventoryCache;		// used to point to dropped weapons
var float	InventoryDist;
var const float LastDetourWeight;

var	CylinderComponent		CylinderComponent;

var Objective NearestObjective; // FIXMESTEVE - determine in path building
var float ObjectiveDistance;
var vector MaxPathSize;

function PostBeginPlay()
{
	// FIXMESTEVE - move to path building
	local int i;
	
	ExtraCost = Max(ExtraCost,0);
	
	for ( i=0; i<PathList.Length; i++ )
	{
		MaxPathSize.X = FMax(MaxPathSize.X, PathList[i].CollisionRadius);
		MaxPathSize.Z = FMax(MaxPathSize.Z, PathList[i].CollisionHeight);
	}
	MaxPathSize.Y = MaxPathSize.X;
	Super.PostBeginPlay();
}

event int SpecialCost(Pawn Seeker, ReachSpec Path);

// Accept an actor that has teleported in.
// used for random spawning and initial placement of creatures
event bool Accept( actor Incoming, actor Source )
{
	// Move the actor here.
	taken = Incoming.SetLocation( Location );
	if (taken)
	{
		Incoming.Velocity = vect(0,0,0);
		Incoming.SetRotation(Rotation);
	}
	Incoming.PlayTeleportEffect(true, false);
	return taken;
}

/* DetourWeight()
value of this path to take a quick detour (usually 0, used when on route to distant objective, but want to grab inventory for example)
*/
event float DetourWeight(Pawn Other,float PathWeight);

/* SuggestMovePreparation()
Optionally tell Pawn any special instructions to prepare for moving to this goal
(called by Pawn.PrepareForMove() if this node's bSpecialMove==true
*/
event bool SuggestMovePreparation(Pawn Other)
{
	return false;
}

/* ProceedWithMove()
Called by Controller to see if move is now possible when a mover reports to the waiting
pawn that it has completed its move
*/
function bool ProceedWithMove(Pawn Other)
{
	return true;
}

/**
 * Returns true if this point is available for chkActor to move to,
 * allowing nodes to control availability.
 */
function bool IsAvailableTo(Actor chkActor)
{
	// default to true
	return true;
}

/**
 * Returns the nearest valid navigation point to the given actor.
 */
static final function NavigationPoint GetNearestNav(Actor chkActor,optional class<NavigationPoint> filterClass,optional array<NavigationPoint> excludeList)
{
	local NavigationPoint nav, bestNav;
	local float dist, bestDist;
	local int idx;
	local bool bExcluded;
	if (chkActor != None)
	{
		// iterate through all points in the level
		for (nav = chkActor.Level.NavigationPointList; nav != None; nav = nav.nextNavigationPoint)
		{
			// looking for the closest one
			if ((filterClass == None ||
				 nav.class == filterClass) &&
				nav.IsAvailableTo(chkActor))
			{
				// make sure it's not in the excluded list
				bExcluded = false;
				for (idx = 0; idx < excludeList.Length && !bExcluded; idx++)
				{
					if (excludeList[idx] == nav)
					{
						bExcluded = true;
					}
				}
				if (!bExcluded)
				{
					// check to see if it's in the exclude list
					dist = VSize(nav.Location-chkActor.Location);
					if (bestNav == None ||
						dist < bestDist)
					{
						bestNav = nav;
						bestDist = dist;
					}
				}
			}
		}
	}
	return bestNav;
}

defaultproperties
{
	Begin Object NAME=Sprite
		Sprite=Texture2D'EngineResources.S_NavP'
	End Object

	Begin Object Name=CollisionCylinder LegacyClassName=NavigationPoint_NavigationPointCylinderComponent_Class
		CollisionRadius=+0050.000000
		CollisionHeight=+0050.000000
	End Object
	CylinderComponent=CollisionCylinder

	Begin Object Class=PathRenderingComponent Name=PathRenderer
		HiddenGame=True
	End Object
	Components.Add(PathRenderer)

	bMayCausePain=true
	bPropagatesSound=true
	bStatic=true
	bNoDelete=true
	bHidden=true
	bCollideWhenPlacing=true
}
