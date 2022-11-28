class Msg_CashBonuses extends LocalMessage;

const MAX_MSG = 6;
var localized string	BonusMessage[MAX_MSG];

static function string GetString
(
	optional	int						Switch,
	optional	bool					bPRI1HUD,
	optional	PlayerReplicationInfo	RelatedPRI_1, 
	optional	PlayerReplicationInfo	RelatedPRI_2,
	optional	Object					OptionalObject
)
{
	if( Switch < MAX_MSG )
	{
		return default.BonusMessage[Switch];
	}

	return "";
}

static function ClientReceive( 
	PlayerController P,
	optional int Switch,
	optional PlayerReplicationInfo RelatedPRI_1, 
	optional PlayerReplicationInfo RelatedPRI_2,
	optional Object OptionalObject
	)
{
	local string MessageString;

	MessageString = static.GetString(Switch, (RelatedPRI_1 == P.PlayerReplicationInfo), RelatedPRI_1, RelatedPRI_2, OptionalObject);
	
	// Hack, force side console messages until we figure out how we want to display these rewards.
	P.TeamMessage(P.PlayerReplicationInfo, MessageString, 'Event');
}

defaultproperties
{
	BonusMessage(0)="Head Shot Kill Cash Reward"
	BonusMessage(1)="Elite Kill Cash Reward"
	BonusMessage(2)="Two-In-One"
	BonusMessage(3)="Trio"
	BonusMessage(4)="MultiKill"
	BonusMessage(5)="Melee Kill Cash Reward"

	PosY=0.25
	bIsConsoleMessage=false
	FontSize=3
}