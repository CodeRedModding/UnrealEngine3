/**
 * WarGameDM
 * Warfare Game deathmatch
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class WarGameDM extends WarGame
	config(Game);

function Reset()
{
	super.Reset();
	GotoState('PendingMatch');
}

auto State PendingMatch
{
Begin:
	StartMatch();
}

defaultproperties
{
    MapPrefix="DM"
	GameName="Deathmatch"
}
