// ====================================================================
// (C) 2003, Epic Games
//
// Looks at the physics of it's owner and decides want to blend.
// ====================================================================

class AnimNodeBlendByPhysics extends AnimNodeBlendList
		Native;

var int			LastMap;	    // Track the last physics
var array<int> 	PhysicsMap;		// Maps the PSY enums to indices

cpptext
{
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );
}


defaultproperties
{
}