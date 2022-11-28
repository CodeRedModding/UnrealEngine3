class UTAnimBlendByPhysics extends UTAnimBlendBase
		Native;

/** Maps the PSY_enums to child nodes */

var(Animations) int    		PhysicsMap[12];			

/** Holds the last known physics type for the tree's owner. */

var int						LastPhysics;		// Track the last physics

cpptext
{
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight  );
}


defaultproperties
{
	PhysicsMap=(-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1,-1)
	Children(0)=(Name="PHYS_Walking",Weight=1.0)
	Children(1)=(Name="PHYS_Falling")
	bFixNumChildren=true

}
