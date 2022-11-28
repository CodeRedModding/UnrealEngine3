//=============================================================================
// ZoneInfo, the built-in Unreal class for defining properties
// of zones.  If you place one ZoneInfo actor in a
// zone you have partioned, the ZoneInfo defines the
// properties of the zone.
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class ZoneInfo extends Info
	native
	placeable;

//-----------------------------------------------------------------------------
// Zone properties.

var() name ZoneTag;
var() localized String LocationName;

var() float KillZ;		// any actor falling below this level gets destroyed
var() float SoftKill;
var() class<KillZDamageType> KillZDamageType;
var() bool bSoftKillZ;	// SoftKill units of grace unless land

/** A list of zones which shouldn't be rendered when the view point is in this zone. */
var() array<name> ForceCullZones;

/** A name for this zone which can be used to reference it from another zone's ForceCullZones. */
var() name CullTag;

//=============================================================================
// Iterator functions.

// Iterate through all actors in this zone.
native(308) final iterator function ZoneActors( class<actor> BaseClass, out actor Actor );

//=============================================================================
// Engine notification functions.

// When an actor enters this zone.
event ActorEntered( actor Other );

// When an actor leaves this zone.
event ActorLeaving( actor Other );

simulated function string GetLocationStringFor(PlayerReplicationInfo PRI)
{
	return LocationName;
}

defaultproperties
{
	Begin Object NAME=Sprite LegacyClassName=ZoneInfo_ZoneInfoSprite_Class
		Sprite=Texture2D'EngineResources.S_ZoneInfo'
	End Object

	KillZ=-1000000.0
	SoftKill=2500.0
	bStatic=true
	bNoDelete=true
}
