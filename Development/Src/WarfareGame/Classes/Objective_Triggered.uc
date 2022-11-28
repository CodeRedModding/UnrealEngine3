class Objective_Triggered extends Objective_Game
	placeable;

var Array<Actor>	MyTriggerList;
var Actor			MyTrigger;

var bool	bInitialized;

function InitTriggerList(name NewEvent)
{
	/*FIXME: update for new scripting system
	local Actor A;
	local name NextEvent;
	local ScriptedTrigger S;
	
	bInitialized = true;
	log(self$" InitTriggerList event "$NewEvent);
	ForEach AllActors(class'Actor', A)
		if ( A.Event == NewEvent )
		{
			MyTriggerList[MyTriggerList.Length] = A;
			if ( A.SelfTriggered() )
				return;
			InitTriggerList(A.Tag);
			return;
		}
		
	ForEach AllActors(class'ScriptedTrigger', S)
		if ( S.TriggersEvent(NewEvent) )
		{
			MyTriggerList[MyTriggerList.Length] = S;
			NextEvent = S.NextNeededEvent();
			if ( NextEvent != NewEvent )
				InitTriggerList(NextEvent);
			break;
		}
	*/
}		

/* FindTrigger()
Find Trigger or ScriptedTrigger that triggers me.
*/
function Actor FindTrigger()
{
	/*
	local int i;
	local name NewEvent;
	
	if ( !bInitialized )
		InitTriggerList(tag);
		
	for ( i=0; i<MyTriggerList.Length; i++ )
	{
		if ( MyTriggerList[i] == None )
			return None;
		if ( MyTriggerList[i].SelfTriggered() )
			return MyTriggerList[i];
		if ( ScriptedTrigger(MyTriggerList[i]) != None )
		{
			if ( (i == MyTriggerList.Length - 1)
				|| (MyTriggerList[i+1] == None) )
				return MyTriggerList[i];
				
			if ( ScriptedTrigger(MyTriggerList[i+1]) == None )
			{
				NewEvent = ScriptedTrigger(MyTriggerList[i]).NextNeededEvent();
				if ( NewEvent != MyTriggerList[i+1].Event )
				{
					MyTriggerList.Remove(i+1, MyTriggerList.Length-i-1);
					log("Init trigger for "$ScriptedTrigger(MyTriggerList[i]));
					InitTriggerList(NewEvent);
				}
			}
		}
	}
	*/
	return None;
}

/* TellBotHowToDisable()
tell bot what to do to disable me.
return true if valid/useable instructions were given
*/
/* !!LD merge
function bool TellBotHowToDisable(Bot B)
{
	local actor MoveTarget;
	
	MyTrigger = FindTrigger();

	if ( MyTrigger == None )
		return false;

	if ( Triggers(MyTrigger) != None )
	{
		if ( B.Pawn.ReachedDestination(MyTrigger) )
		{
			if ( MyTrigger.bCollideActors )
				MyTrigger.Touch(B.Pawn);
			if ( (B.Enemy != None) && B.EnemyVisible() )
			{
				B.DoRangedAttackOn(B.Enemy);
				return true;
			}
		}
	}
	
	MoveTarget = MyTrigger.SpecialHandling(B.Pawn);
	if ( MoveTarget == None )
		return false;
	
	if ( B.ActorReachable(MoveTarget) )
	{
		B.MoveTarget = MoveTarget;
		B.GoalString = "Go to activate trigger "$MyTrigger;
		B.SetAttractionState();
		return true;
	}
	else if ( (Vehicle(B.Pawn) != None) && !B.Squad.NeverBail(B.Pawn)
				&& (VSize(MoveTarget.Location - B.Pawn.Location) < 1000) )
	{
		Vehicle(B.Pawn).TeamUseTime = Level.TimeSeconds + 6;
		Vehicle(B.Pawn).KDriverLeave(false);
		if ( B.ActorReachable(MoveTarget) )
		{
			B.MoveTarget = MoveTarget;
			B.GoalString = "Go to activate trigger "$MyTrigger;
			B.SetAttractionState();
			return true;
		}
	}

	if ( (Vehicle(B.Pawn) != None) && (VehiclePath != None) )
		return Super.TellBotHowToDisable(B);

	B.FindBestPathToward(MoveTarget, true,true);
	if ( B.MoveTarget == None )
		return false;
	B.GoalString = "Follow path to "$MyTrigger;
	B.SetAttractionState();
	return true;
}


function bool BotNearObjective(Bot B)
{
	if ( (MyBaseVolume != None)
		&& B.Pawn.IsInVolume(MyBaseVolume) )
		return true;

	if ( MyTrigger == None )
		return false;
	
	return ( (VSize(MyTrigger.Location - B.Pawn.Location) < 2000) && B.LineOfSightTo(MyTrigger) );
}
*/
defaultproperties
{
	ObjectiveName="TriggeredObjective"
	bStatic=false
	bNoDelete=true
}