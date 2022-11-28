class UTPlayerReplicationInfo extends PlayerReplicationInfo;

var bool bAdrenalineEnabled; 
var byte Adrenaline;	
var byte AdrenalineMax; 

var UTLinkedReplicationInfo CustomReplicationInfo;	// for use by mod authors

//FIXMESTEVE var class<VoicePack>	VoiceType;
var string				VoiceTypeName;
var repnotify string	CharacterName;

replication
{
	reliable if ( Role==ROLE_Authority )
		CustomReplicationInfo, CharacterName;
	reliable if ( bNetOwner && (Role==ROLE_Authority) )
		Adrenaline;
}

simulated event ReplicatedEvent(string VarName)
{
	if ( VarName ~= "CharacterName" )
	{
		UpdateCharacter();
	}
	else
	{
		Super.ReplicatedEvent(VarName);
	}
}

function UpdateCharacter();

function AwardAdrenaline(float amount)
{
	if ( bAdrenalineEnabled )
	{
		//FIXMESTEVE if ( (Adrenaline < AdrenalineMax) && (Adrenaline+amount >= AdrenalineMax) && ((Pawn == None) || !Pawn.InCurrentCombo()) )
		//	ClientDelayedAnnouncementNamed('Adrenalin',15);
		Adrenaline += Amount;
		Adrenaline = Clamp( Adrenaline, 0, AdrenalineMax );
	}
}

function bool NeedsAdrenaline()
{
	return false;
	//FIXMESTEVE return ( (Pawn != None) && !Pawn.InCurrentCombo() && (Adrenaline < AdrenalineMax) );
}

function Reset()
{
	Super.Reset();
	
	Adrenaline = 0;
}


simulated function string GetCallSign()
{
	if ( TeamID > 14 )
		return "";
	return class'UTGame'.default.CallSigns[TeamID];
}

simulated event string GetNameCallSign()
{
	if ( TeamID > 14 )
		return PlayerName;
	return PlayerName$" ["$class'UTGame'.default.CallSigns[TeamID]$"]";
}

/* FIXMESTEVE

function SetCharacterVoice(string S)
{
	local class<VoicePack> NewVoiceType;

	if ( (Level.NetMode == NM_DedicatedServer) && (VoiceType != None) )
	{
		VoiceTypeName = S;
		return;
	}
	if ( S == "" )
	{
		VoiceTypeName = "";
		return;
	}

	NewVoiceType = class<VoicePack>(DynamicLoadObject(S,class'Class'));
	if ( NewVoiceType != None )
	{
		VoiceType = NewVoiceType;
		VoiceTypeName = S;
	}
}
*/
defaultproperties
{
    Adrenaline=0
    AdrenalineMax=100
    bAdrenalineEnabled=false
}