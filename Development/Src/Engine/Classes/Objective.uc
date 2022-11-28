class Objective extends NavigationPoint
	native
	abstract;

var		bool	bDisabled;			// true when objective has been completed/disabled/reached/destroyed
var		bool	bFirstObjective;	// First objective in list of objectives defended by same team
var		bool	bOptionalObjective;

var()	byte	DefenderTeamIndex;	// 0 = defended by team 0
var		byte	StartTeam;
var()	byte	DefensePriority;	// Higher priority defended/attacked first
var()	int		Score;				// score given to player that completes this objective

var		Objective	NextObjective;			// list of objectives defended by the same team
var()	class<Pawn> ConstraintPawnClass;	// Objective restricted to this specific Pawn class

var()	localized string ObjectiveName;
var		localized string ObjectiveStringPrefix, ObjectiveStringSuffix;

var localized string ColorNames[4];

replication
{
	unreliable if ( (Role==ROLE_Authority) && bNetDirty )
		bDisabled, DefenderTeamIndex;
}

simulated function PostBeginPlay()
{
	local Objective O, CurrentObjective;

	super.PostBeginPlay();
	StartTeam = DefenderTeamIndex;

	// add to objective list
	if ( bFirstObjective )
	{
		Level.ObjectiveList = Self;
		CurrentObjective = Self;
		ForEach AllActors(class'Objective',O)
			if ( O != CurrentObjective )
			{
				CurrentObjective.NextObjective = O;
				O.bFirstObjective = false;
				CurrentObjective = O;
			}
	}
}

/* Relevancy check for instigator, used to determine if objective can be completed */
function bool IsInstigatorRelevant( Pawn P )
{
	//log("Objective::IsInstigatorRelevant P:" $ P.GetHumanReadableName() @ "P.Class:" $ P.Class @ "P.Team:" @ P.GetTeamNum()
	//	@ "bDisabled:" $ bDisabled @ "CanCompleteObjective():" $ Level.Game.CanCompleteObjective( Self ));

	if ( bDisabled || !Level.Game.CanCompleteObjective( Self ) )
		return false;

	if ( P == None )
		return true;

	if ( !ClassIsChildOf(P.Class, ConstraintPawnClass) )
		return false;

	if ( Level.GRI.OnSameTeam(P,self) )
		return false;

	return true;
}

function byte GetTeamNum()
{
	return DefenderTeamIndex;
}

/* auto complete objective (forced by automated events. like scripted events, cinematics or debugging) */
function ForceCompleteObjective( Pawn InstigatedBy )
{
	CompleteObjective( InstigatedBy );
}

function bool CompleteObjective( Pawn InstigatedBy )
{
	if ( bDisabled || !Level.Game.CanCompleteObjective( Self ) )
		return false;

	log( GetHumanReadableName() @ "completed by" @ InstigatedBy.GetHumanReadableName() );
	bDisabled = true;
	ObjectiveCompleted();
	Level.Game.Broadcast(Self, ObjectiveName, 'CriticalEvent');
	Level.Game.ScoreObjective(InstigatedBy.PlayerReplicationInfo, Score);
	Level.Game.ObjectiveCompleted( Self );
	return true;
}

simulated function ObjectiveCompleted()
{
}

/* Reset() - reset actor to initial state - used when restarting level without reloading. */
function Reset()
{
	super.Reset();
	bDisabled			= false;
	DefenderTeamIndex	= StartTeam;
	NetUpdateTime		= Level.TimeSeconds - 1;
}

/* returns objective's progress status 1->0 (=disabled) */
simulated function float GetObjectiveProgress()
{
	return float(!bDisabled);
}

simulated function string GetHumanReadableName()
{
	if ( default.ObjectiveName != "" )
		return default.ObjectiveName;

	return ObjectiveStringPrefix $ ColorNames[DefenderTeamIndex] $ ObjectiveStringSuffix;
}

simulated function DrawObjectiveDebug( Canvas C )
{
	local string ObjectiveInfo;

	ObjectiveInfo = ObjectiveName @ "bDisabled:" @ bDisabled @ "ObjectiveProgress:" @ GetObjectiveProgress();
	C.DrawText( ObjectiveInfo );
}

defaultproperties
{
	bFirstObjective=true
	ConstraintPawnClass=class'Pawn'

	bReplicateMovement=false
	bOnlyDirtyReplication=true

	bMustBeReachable=true

	ObjectiveName="Objective"
	ObjectiveStringPrefix=""
	ObjectiveStringSuffix=" Team Base"
	
	ColorNames(0)="Red"
	ColorNames(1)="Blue"
	ColorNames(2)="Green"
	ColorNames(3)="Gold"
}