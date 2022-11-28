//=============================================================================
// Player: Corresponds to a real player (a local camera or remote net player).
// This is a built-in Unreal class and it shouldn't be modified.
//=============================================================================
class Player extends Object
	native
	config(Engine)
	noexport
	transient;

// Internal.
var native const noexport pointer			vfOut;
var native const noexport pointer			vfExec;

// The actor this player controls.
var transient const playercontroller		Actor;

// Net variables.
var const int CurrentNetSpeed;
var globalconfig int ConfiguredInternetSpeed, ConfiguredLanSpeed;
