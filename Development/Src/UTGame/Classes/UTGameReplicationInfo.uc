class UTGameReplicationInfo extends GameReplicationInfo
	config(game);

var float WeaponBerserk;
var int MinNetPlayers;
var int BotDifficulty;		// for bPlayersVsBots

replication
{
	reliable if ( bNetInitial && (Role==ROLE_Authority) )
		WeaponBerserk, MinNetPlayers, BotDifficulty;
}

defaultproperties
{
    WeaponBerserk=+1.0
    BotDifficulty=-1
}
