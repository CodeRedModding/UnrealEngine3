/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Trigger_LOS extends Trigger;

var array<PlayerController> PCsWithLOS;

/**
 * Overridden to check for any players looking at this
 * trigger.
 */
simulated event Tick(float DeltaTime)
{
	local array<SequenceEvent> losEvents;
	local SeqEvent_LOS evt;
	local PlayerController Player;
	local int idx;
	local vector cameraLoc;
	local rotator cameraRot;
	local float cameraDist;
	local array<int> ActivateIndices;

	// if any valid los events are attached,
	if (FindEventsOfClass(class'SeqEvent_LOS',losEvents))
	{
		// look through each player
		foreach WorldInfo.AllControllers(class'PlayerController', Player)
		{
			if (Player.Pawn != None)
			{
				player.GetPlayerViewPoint(cameraLoc, cameraRot);
				cameraDist = PointDistToLine(Location,vector(cameraRot),cameraLoc);
				// iterate through each event and see if this meets the activation requirements
				for (idx = 0; idx < losEvents.Length; idx++)
				{
					evt = SeqEvent_LOS(losEvents[idx]);
					if ( cameraDist <= evt.ScreenCenterDistance &&
						VSize(player.Pawn.Location-Location) <= evt.TriggerDistance &&
						Normal(Location - cameraLoc) dot vector(cameraRot) > 0.f &&
						(!evt.bCheckForObstructions || Player.LineOfSightTo(self, cameraLoc)) )
					{
						// attempt to activate the event
						ActivateIndices[0] = 0;
						if ( PCsWithLOS.Find(Player) == INDEX_NONE &&
							losEvents[idx].CheckActivate(self, Player.Pawn, false, ActivateIndices) )
						{
							PCsWithLOS.AddItem(Player);
						}
					}
					else if (PCsWithLOS.Find(Player) != INDEX_NONE)
					{
						ActivateIndices[0] = 1;
						if (losEvents[idx].CheckActivate(self, Player.Pawn, false, ActivateIndices))
						{
							PCsWithLOS.RemoveItem(Player);
						}
					}
				}
			}
		}
	}
}

defaultproperties
{
	bStatic=false

	SupportedEvents.Empty
	SupportedEvents.Add(class'SeqEvent_LOS')
}
