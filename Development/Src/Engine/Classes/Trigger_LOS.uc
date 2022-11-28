class Trigger_LOS extends Trigger;

/**
 * Overridden to check for any players looking at this
 * trigger.
 */
simulated function Tick(float DeltaTime)
{
	local array<SequenceEvent> losEvents;
	local SeqEvent_LOS evt;
	local Controller player;
	local int idx;
	local vector cameraLoc;
	local rotator cameraRot;
	local float cameraDist;
	// if any valid los events are attached,
	if (FindEventsOfClass(class'SeqEvent_LOS',losEvents))
	{
		// look through each player
		for (player = Level.ControllerList; player != None; player = player.nextController)
		{
			if (player.IsA('PlayerController') &&
				player.Pawn != None)
			{
				player.GetPlayerViewPoint(cameraLoc, cameraRot);
				cameraDist = PointDistToLine(Location,vector(cameraRot),cameraLoc);
				// iterate through each event and see if this meets the activation requirements
				for (idx = 0; idx < losEvents.Length; idx++)
				{
					evt = SeqEvent_LOS(losEvents[idx]);
					if (cameraDist <= evt.ScreenCenterDistance &&
						VSize(player.Pawn.Location-Location) <= evt.TriggerDistance &&
						Normal(Location - cameraLoc) dot vector(cameraRot) > 0.f &&
						(!evt.bCheckForObstructions ||
						 player.LineOfSightTo(self,cameraLoc)))
					{
						// attempt to activate the event
						losEvents[idx].CheckActivate(self,player.Pawn,false);
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
