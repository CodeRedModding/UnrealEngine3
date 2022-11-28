//=============================================================================
// Mutator.
//
// Mutators allow modifications to gameplay while keeping the game rules intact.
// Mutators are given the opportunity to modify player login parameters with
// ModifyLogin(), to modify player pawn properties with ModifyPlayer(), or to modify, remove,
// or replace all other actors when they are spawned with CheckRelevance(), which
// is called from the PreBeginPlay() function of all actors except those (Decals,
// Effects and Projectiles for performance reasons) which have bGameRelevant==true.
//=============================================================================
class Mutator extends Info
	native;
	
var Mutator NextMutator;
var class<Weapon> DefaultWeapon;
var string DefaultWeaponName;
var() string            GroupName; // Will only allow one mutator with this tag to be selected.
var() localized string  FriendlyName;
var() localized string  Description;
var bool bUserAdded;

/* Don't call Actor PreBeginPlay() for Mutator
*/
event PreBeginPlay()
{
	if ( !MutatorIsAllowed() )
		Destroy();
}

function bool MutatorIsAllowed()
{
	return !Level.IsDemoBuild() || Class==class'Mutator';
}

function Destroyed()
{
	Level.Game.RemoveMutator(self);
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

/* GetInventoryClass()
return an inventory class - either the class specified by InventoryClassName, or a
replacement.  Called when spawning initial inventory for player
*/
function Class<Inventory> GetInventoryClass(string InventoryClassName)
{
	InventoryClassName = GetInventoryClassOverride(InventoryClassName);
	return class<Inventory>(DynamicLoadObject(InventoryClassName, class'Class'));
}

/* GetInventoryClassOverride()
return the string passed in, or a replacement class name string.
*/
function string GetInventoryClassOverride(string InventoryClassName)
{
	// here, in mutator subclass, change InventoryClassName if desired.  For example:
	// if ( InventoryClassName == "Weapons.DorkyDefaultWeapon"
	//		InventoryClassName = "ModWeapons.SuperDisintegrator"

	if ( NextMutator != None )
		return NextMutator.GetInventoryClassOverride(InventoryClassName);
	return InventoryClassName;
}

function AddMutator(Mutator M)
{
	if ( NextMutator == None )
		NextMutator = M;
	else
		NextMutator.AddMutator(M);
}

/* ReplaceWith()
Call this function to replace an actor Other with an actor of aClass.
*/
function bool ReplaceWith(actor Other, string aClassName)
{
	local Actor A;
	local class<Actor> aClass;

	if ( aClassName == "" )
		return true;
		
	aClass = class<Actor>(DynamicLoadObject(aClassName, class'Class'));
	if ( aClass != None )
		A = Spawn(aClass,Other.Owner,Other.tag,Other.Location, Other.Rotation);
	if ( A != None )
	{
		A.tag = Other.tag;
		return true;
	}
	return false;
}

/* Force game to always keep this actor, even if other mutators want to get rid of it
*/
function bool AlwaysKeep(Actor Other)
{
	if ( NextMutator != None )
		return ( NextMutator.AlwaysKeep(Other) );
	return false;
}

function bool IsRelevant(Actor Other, out byte bSuperRelevant)
{
	local bool bResult;

	bResult = CheckReplacement(Other, bSuperRelevant);
	if ( bResult && (NextMutator != None) )
		bResult = NextMutator.IsRelevant(Other, bSuperRelevant);

	return bResult;
}

function bool CheckRelevance(Actor Other)
{
	local bool bResult;
	local byte bSuperRelevant;

	if ( AlwaysKeep(Other) )
		return true;

	// allow mutators to remove actors

	bResult = IsRelevant(Other, bSuperRelevant);

	return bResult;
}

function bool CheckReplacement(Actor Other, out byte bSuperRelevant)
{
	return true;
}

//
// server querying
//
function GetServerDetails( out GameInfo.ServerResponseLine ServerState )
{
	// append the mutator name.
	local int i;
	i = ServerState.ServerInfo.Length;
	ServerState.ServerInfo.Length = i+1;
	ServerState.ServerInfo[i].Key = "Mutator";
	ServerState.ServerInfo[i].Value = GetHumanReadableName();
}

function GetServerPlayers( out GameInfo.ServerResponseLine ServerState )
{
}

// jmw - Allow mod authors to hook in to the %X var parsing

function string ParseChatPercVar(Controller Who, string Cmd)
{
	if (NextMutator !=None)
		Cmd = NextMutator.ParseChatPercVar(Who,Cmd);
		
	return Cmd;
}

function NotifyLogout(PlayerController Exiting )
{
	if ( NextMutator != None )
		NextMutator.NotifyLogout(Exiting);
}

function NotifyLogin(PlayerController NewPlayer)
{
	if ( NextMutator != None )
		NextMutator.NotifyLogin(NewPlayer);
}

function bool CanEnterVehicle(Vehicle V, Pawn P)
{
	if ( NextMutator != None )
		return NextMutator.CanEnterVehicle(V, P);
	return true;
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

defaultproperties
{
    GroupName=""
    FriendlyName=""
    Description=""
}

