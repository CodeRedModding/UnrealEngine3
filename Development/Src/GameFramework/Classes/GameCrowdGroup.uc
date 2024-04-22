/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class GameCrowdGroup extends Object
	native;

var array<GameCrowdAgent> Members;

function AddMember(GameCrowdAgent Agent)
{
	Members[Members.Length] = Agent;
	Agent.MyGroup = self;
}

function RemoveMember(GameCrowdAgent Agent)
{
	Members.RemoveItem(Agent);
	Agent.MyGroup = None;
}

function UpdateDestinations(GameCrowdDestination NewDestination)
{
	local int i;
	
	for ( i=0; i<Members.Length; i++ )
	{
		if ( (Members[i] != None) && (Members[i].CurrentDestination != NewDestination) )
		{
			Members[i].SetCurrentDestination(NewDestination);
			Members[i].UpdateIntermediatePoint();
		}
	}
}

defaultproperties
{
}
