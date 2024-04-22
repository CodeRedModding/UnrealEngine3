/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class Admin extends PlayerController
	config(Game);

simulated event PostBeginPlay()
{
	Super.PostBeginPlay();
	AddCheats();
}

// Execute an administrative console command on the server.
exec function Admin( string CommandLine )
{
	ServerAdmin(CommandLine);
}

reliable server function ServerAdmin( string CommandLine )
{
	local string Result;

	Result = ConsoleCommand( CommandLine );
	if( Result!="" )
		ClientMessage( Result );
}

exec function KickBan( string S )
{
	ServerKickBan(S);
}

reliable server function ServerKickBan( string S )
{
	WorldInfo.Game.KickBan(S);
}

exec function Kick( string S )
{
	ServerKick(S);
}

reliable server function ServerKick( string S )
{
	WorldInfo.Game.Kick(S);
}

exec function PlayerList()
{
	local PlayerReplicationInfo PRI;

	`log("Player List:");
	ForEach DynamicActors(class'PlayerReplicationInfo', PRI)
		`log(PRI.PlayerName@"( ping"@PRI.Ping$")");
}

exec function RestartMap()
{
	ServerRestartMap();
}

reliable server function ServerRestartMap()
{
	ClientTravel( "?restart", TRAVEL_Relative );
}

exec function Switch( string URL )
{
	ServerSwitch(URL);
}

reliable server function ServerSwitch(string URL)
{
	WorldInfo.ServerTravel(URL);
}

defaultproperties
{
}
