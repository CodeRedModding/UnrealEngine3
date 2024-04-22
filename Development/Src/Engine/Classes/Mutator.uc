//=============================================================================
// Mutator.
//
// Mutators allow modifications to gameplay while keeping the game rules intact.
// Mutators are given the opportunity to modify player login parameters with
// ModifyLogin(), to modify player pawn properties with ModifyPlayer(), or to modify, remove,
// or replace all other actors when they are spawned with CheckRelevance(), which
// is called from the PreBeginPlay() function of all actors except those (Decals,
// Effects and Projectiles for performance reasons) which have bGameRelevant==true.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================
class Mutator extends Info
	native
	abstract;

/** Next in list of mutators linked from GameInfo.BaseMutator */
var Mutator NextMutator;

/** list of groups this mutator is in. Mutators that share any group cannot be activated simultaneously */
var() array<string> GroupNames;

/** Meant to verify if this mutator was from Command Line parameters or added from other Actors */
var bool bUserAdded;

/** 
  * Make sure mutator is allowed in this session.
  * Don't call Actor.PreBeginPlay() for Mutator.
 */
event PreBeginPlay()
{
	if ( !MutatorIsAllowed() )
		Destroy();
}

/**
  *  Returns whether mutator is allowed in this session.
  */
function bool MutatorIsAllowed()
{
	// by default, disallow mutators in demo builds
	return !WorldInfo.IsDemoBuild();
}

/** 
  *  Make sure this is removed from the game's mutator list
  */
event Destroyed()
{
	WorldInfo.Game.RemoveMutator(self);
	Super.Destroyed();
}

function Mutate(string MutateString, PlayerController Sender)
{
	if ( NextMutator != None )
		NextMutator.Mutate(MutateString, Sender);
}

function ModifyLogin(out string Portal, out string Options)
{
	if ( NextMutator != None )
		NextMutator.ModifyLogin(Portal, Options);
}

/* called by GameInfo.RestartPlayer()
	change the players jumpz, etc. here
*/
function ModifyPlayer(Pawn Other)
{
	if ( NextMutator != None )
		NextMutator.ModifyPlayer(Other);
}

/** 
  *  Add Mutator M to the mutator list.
  */
function AddMutator(Mutator M)
{
	if ( NextMutator == None )
		NextMutator = M;
	else
		NextMutator.AddMutator(M);
}

/**
  * Force game to always keep this actor, even if other mutators want to get rid of it
  */
function bool AlwaysKeep(Actor Other)
{
	if ( NextMutator != None )
		return ( NextMutator.AlwaysKeep(Other) );
	return false;
}

/**
  * Returns whether Other (being spawned) is relevant (should be allowed to exist)
  */
function bool IsRelevant(Actor Other)
{
	local bool bResult;

	bResult = CheckReplacement(Other);
	if ( bResult && (NextMutator != None) )
		bResult = NextMutator.IsRelevant(Other);

	return bResult;
}

/** 
  *  Check whether Other (being spawned) should be allowed to exist
  */
function bool CheckRelevance(Actor Other)
{
	local bool bResult;

	if ( AlwaysKeep(Other) )
		return true;

	// allow mutators to remove actors
	bResult = IsRelevant(Other);

	return bResult;
}

/**
 * Returns true to keep this actor
 */
function bool CheckReplacement(Actor Other)
{
	return true;
}

function NotifyLogout(Controller Exiting)
{
	if ( NextMutator != None )
		NextMutator.NotifyLogout(Exiting);
}

function NotifyLogin(Controller NewPlayer)
{
	if ( NextMutator != None )
		NextMutator.NotifyLogin(NewPlayer);
}

function DriverEnteredVehicle(Vehicle V, Pawn P)
{
	if ( NextMutator != None )
		NextMutator.DriverEnteredVehicle(V, P);
}

function bool CanLeaveVehicle(Vehicle V, Pawn P)
{
	if ( NextMutator != None )
		return NextMutator.CanLeaveVehicle(V, P);
	return true;
}

function DriverLeftVehicle(Vehicle V, Pawn P)
{
	if ( NextMutator != None )
		NextMutator.DriverLeftVehicle(V, P);
}

/**
 * This function can be used to parse the command line parameters when a server
 * starts up
 */
function InitMutator(string Options, out string ErrorMessage)
{
	if (NextMutator != None)
	{
		NextMutator.InitMutator(Options, ErrorMessage);
	}
}

/** called on the server during seamless level transitions to get the list of Actors that should be moved into the new level
 * PlayerControllers, Role < ROLE_Authority Actors, and any non-Actors that are inside an Actor that is in the list
 * (i.e. Object.Outer == Actor in the list)
 * are all automatically moved regardless of whether they're included here
 * only dynamic (!bStatic and !bNoDelete) actors in the PersistentLevel may be moved (this includes all actors spawned during gameplay)
 * this is called for both parts of the transition because actors might change while in the middle (e.g. players might join or leave the game)
 * @param bToEntry true if we are going from old level -> entry, false if we are going from entry -> new level
 * @param ActorList (out) list of actors to maintain
 */
function GetSeamlessTravelActorList(bool bToEntry, out array<Actor> ActorList)
{
	// by default, keep ourselves around until we switch to the new level
	if (bToEntry)
	{
		ActorList[ActorList.length] = self;
	}

	if (NextMutator != None)
	{
		NextMutator.GetSeamlessTravelActorList(bToEntry, ActorList);
	}
}

/* Override GameInfo FindPlayerStart() - called by GameInfo.FindPlayerStart()
if a NavigationPoint is returned, it will be used as the playerstart
*/
function NavigationPoint FindPlayerStart(Controller Player, optional byte InTeam, optional string incomingName)
{
	if (NextMutator != None)
	{
		return NextMutator.FindPlayerStart(Player, InTeam, incomingName);
	}
	else
	{
		return None;
	}
}

//
// Restart the game.
//
function bool HandleRestartGame()
{
	return (NextMutator != None && NextMutator.HandleRestartGame());
}

/* CheckEndGame()
Allows modification of game ending conditions.  Return false to prevent game from ending
*/
function bool CheckEndGame(PlayerReplicationInfo Winner, string Reason)
{
	return (NextMutator == None || NextMutator.CheckEndGame(Winner, Reason));
}

/** OverridePickupQuery()
 * when pawn wants to pickup something, mutators are given a chance to modify it.  If this function
 * returns true, bAllowPickup will determine if the object can be picked up.
 * @param Other the Pawn that wants the item
 * @param ItemClass the Inventory class the Pawn can pick up
 * @param Pickup the Actor containing that item (this may be a PickupFactory or it may be a DroppedPickup)
 * @param bAllowPickup (out) whether or not the Pickup actor should give its item to Other (0 == false, anything else == true)
 * @return whether or not to override the default behavior with the value of
 */
function bool OverridePickupQuery(Pawn Other, class<Inventory> ItemClass, Actor Pickup, out byte bAllowPickup)
{
	return (NextMutator != None && NextMutator.OverridePickupQuery(Other, ItemClass, Pickup, bAllowPickup));
}

function bool PreventDeath(Pawn Killed, Controller Killer, class<DamageType> damageType, vector HitLocation)
{
	return (NextMutator != None && NextMutator.PreventDeath(Killed, Killer, damageType, HitLocation));
}

function ScoreObjective(PlayerReplicationInfo Scorer, int Score)
{
	if (NextMutator != None)
	{
		NextMutator.ScoreObjective(Scorer, Score);
	}
}

function ScoreKill(Controller Killer, Controller Killed)
{
	if (NextMutator != None)
	{
		NextMutator.ScoreKill(Killer, Killed);
	}
}

function NetDamage(int OriginalDamage, out int Damage, Pawn Injured, Controller InstigatedBy, vector HitLocation, out vector Momentum, class<DamageType> DamageType, Actor DamageCauser)
{
	if (NextMutator != None)
	{
		NextMutator.NetDamage(OriginalDamage, Damage, Injured, InstigatedBy, HitLocation, Momentum, DamageType, DamageCauser);
	}
}

defaultproperties
{
}

