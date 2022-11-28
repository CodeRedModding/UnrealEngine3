class Objective_Locked extends Objective_Proximity;

var() name KeyTag;	// tag of key which unlocks this objective
/* !!LD merge
var KeyPickup MyKey;

function FindKey()
{
	if ( (MyKey != None) && !MyKey.bDeleteMe )
		return;

	MyKey = None;

	ForEach AllActors(class'KeyPickup',MyKey,KeyTag)
		break;
}

// TellBotHowToDisable()
// tell bot what to do to disable me.
// return true if valid/useable instructions were given

function bool TellBotHowToDisable(Bot B)
{
	local KeyInventory K;
	local Controller C;

	K = KeyInventory(B.Pawn.FindInventoryType(class'KeyInventory'));

	// if bot has key, tell bot to come find me
	if ( (K != None) && (K.Tag == KeyTag) )
		return Super.TellBotHowToDisable(B);

	// does other player on bot's team have key - if so follow him
	for ( C=Level.ControllerList; C!=None; C=C.NextController )
		if ( (C.PlayerReplicationInfo != None) && (C.Pawn != None)
			&& (C.PlayerReplicationInfo.Team == B.PlayerReplicationInfo.Team) )
		{
			K = KeyInventory(C.Pawn.FindInventoryType(class'KeyInventory'));
			if ( (K != None) && (K.Tag == KeyTag) )
			{
				B.Squad.TellBotToFollow(B,C);
				return true;
			}
		}

	// if not find key
	FindKey();
	if ( MyKey == None )
		return false;

	if ( B.ActorReachable(MyKey) )
	{
		B.GoalString = "almost at "$MyKey;
		B.MoveTarget = MyKey;
		B.SetAttractionState();
		return true;
	}

	B.GoalString = "No path to key "$MyKey;
	if ( !B.FindBestPathToward(MyKey,false,true) )
		return false;
	B.GoalString = "Follow path to "$MyKey;
	B.SetAttractionState();
	return true;
}


function bool IsRelevant( Pawn Instigator )
{
	local KeyInventory K;

	if ( Instigator != None )
	{
		K = KeyInventory(Instigator.FindInventoryType(class'KeyInventory'));
		if ( (K != None) && (K.Tag == KeyTag) )
			K.UnLock( Self );
	}

	// Return false, because DisableObjective is called from KeyInventory.Unlock()
	return false;
}
*/
defaultproperties
{
	KeyTag=KeyPickup
}