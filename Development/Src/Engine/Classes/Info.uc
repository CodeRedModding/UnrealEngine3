/**
 * Info, the root of all information holding klasses.
 * Doesn't have any movement / collision related code.
 */

class Info extends Actor
	abstract
	hidecategories(Movement,Collision)
	native;

//------------------------------------------------------------------------------
// Structs for reporting server state data

struct native export KeyValuePair
{
	var() string Key;
	var() string Value;
};

struct native export PlayerResponseLine
{
	var() int PlayerNum;
	var() int PlayerID;
	var() string PlayerName;
	var() int Ping;
	var() int Score;
	var() int StatsID;
	var() array<KeyValuePair> PlayerInfo;

};

struct native export ServerResponseLine
{
	var() int ServerID;
	var() string IP;
	var() int Port;
	var() int QueryPort;
	var() string ServerName;
	var() string MapName;
	var() string GameType;
	var() int CurrentPlayers;
	var() int MaxPlayers;
	var() int Ping;
	
	var() array<KeyValuePair> ServerInfo;
	var() array<PlayerResponseLine> PlayerInfo;
};


defaultproperties
{
	CollisionComponent=None
	Components.Remove(CollisionCylinder)
	RemoteRole=ROLE_None
	NetUpdateFrequency=10
	bHidden=true
	bOnlyDirtyReplication=true
	bSkipActorPropertyReplication=true
}
