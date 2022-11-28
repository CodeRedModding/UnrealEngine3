//=============================================================================
// ReachSpec.
//
// A Reachspec describes the reachability requirements between two NavigationPoints
//
//=============================================================================
class ReachSpec extends Object
	native;

cpptext
{
	/*
	supports() -
	returns true if it supports the requirements of aPawn.  Distance is not considered.
	*/
	inline UBOOL supports (INT iRadius, INT iHeight, INT moveFlags, INT iMaxFallVelocity)
	{
		return ( (CollisionRadius >= iRadius) 
			&& (CollisionHeight >= iHeight)
			&& ((reachFlags & moveFlags) == reachFlags)
			&& ((MaxLandingVelocity <= iMaxFallVelocity) || bForced) );
	}

	int defineFor (class ANavigationPoint *begin, class ANavigationPoint * dest, class APawn * Scout);
	int operator<= (const UReachSpec &Spec);
	FPlane PathColor();
	void Init();
	int findBestReachable(class AScout *Scout);
	UBOOL PlaceScout(class AScout *Scout); 
}

var	int		Distance; 
var	const NavigationPoint	Start;		// navigationpoint at start of this path
var	const NavigationPoint	End;		// navigationpoint at endpoint of this path (next waypoint or goal)
var	int		CollisionRadius; 
var	int		CollisionHeight; 
var	int		reachFlags;			// see EReachSpecFlags definition in UnPath.h
var	int		MaxLandingVelocity;
var	byte	bPruned;
var	const bool	bForced;