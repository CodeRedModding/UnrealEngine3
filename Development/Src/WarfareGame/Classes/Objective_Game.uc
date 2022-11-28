class Objective_Game extends Objective
	abstract;

var		bool	bHasShootSpots;

/* !!LD merge
var UnrealScriptedSequence DefenseScripts;
var()	Name	DefenseScriptTags;	// tags of scripts that are defense scripts
var SquadAI DefenseSquad;	// squad defending this objective;
var AssaultPath AlternatePaths;
*/
var()	name	AreaVolumeTag;
var		Volume	MyBaseVolume;
var()	float	BaseExitTime;		// how long it takes to get entirely away from the base
var()	float	BaseRadius;			// radius of base

var()	localized string LocationPrefix, LocationPostfix;


function float GetDifficulty()
{
	return 0;
}

simulated function PostBeginPlay()
{
	//local AssaultPath A;
	//local UnrealScriptedSequence W;

	super.PostBeginPlay();

	if ( Role == Role_Authority )
	{
		/* !!LD merge
		// find defense scripts
		if ( DefenseScriptTags != '' )
			ForEach AllActors(class'UnrealScriptedSequence', DefenseScripts, DefenseScriptTags)
				if ( DefenseScripts.bFirstScript )
				b	reak;

		// clear defense scripts bFreelance
		for ( W=DefenseScripts; W!=None; W=W.NextScript )
			W.bFreelance = false;

		// set up AssaultPaths
		ForEach AllActors(class'AssaultPath', A)
			if ( A.ObjectiveTag == Tag )
				A.AddTo(self);
		*/
	}

	// find AreaVolume
	ForEach AllActors(class'Volume', MyBaseVolume, AreaVolumeTag)
		break;
}

/* !!LD merge
function bool BotNearObjective(Bot B)
{
	if ( ((MyBaseVolume != None) && B.Pawn.IsInVolume(MyBaseVolume))
		|| ((B.RouteGoal == self) && (B.RouteDist < 2500))
		|| ((VSize(Location - B.Pawn.Location) < BaseRadius) && (B.bWasNearObjective || B.LineOfSightTo(self))) )
	{
		B.bWasNearObjective = true;
		return true;
	}
	
	B.bWasNearObjective = false;
	return false;
}


function bool OwnsDefenseScript(UnrealScriptedSequence S)
{
	local UnrealScriptedSequence W;

	for ( W=DefenseScripts; W!=None; W=W.NextScript )
		if ( W == S )
			return true;

	return false;
}
*/


/* TellBotHowToDisable()
tell bot what to do to disable me.
return true if valid/useable instructions were given
*/
/* !!LD merge
function bool TellBotHowToDisable(Bot B)
{
	if ( B.Pawn == None || !IsRelevant(B.Pawn) )
		return false;

	return B.Squad.FindPathToObjective(B,self);
}
	
function int GetNumDefenders()
{
	return 0;

	if ( DefenseSquad == None )
		return 0;
	return DefenseSquad.GetSize();
	// fiXME - max defenders per defensepoint, when all full, report big number
}
*/

function bool BetterObjectiveThan(Objective Best, byte DesiredTeamNum, byte RequesterTeamNum)
{
	if ( bDisabled || (DefenderTeamIndex != DesiredTeamNum) )
		return false;

	if ( (Best == None) || (Best.DefensePriority < DefensePriority) )
		return true;
	
	return false;
}
	

defaultproperties
{
	BaseExitTime=+8.0
	BaseRadius=+2000.0
	ObjectiveName="GameObjective"
	LocationPrefix="Near"
	LocationPostfix=""
}