class GameMessage extends LocalMessage;

var localized string	      SwitchLevelMessage;
var localized string	      LeftMessage;
var localized string	      FailedTeamMessage;
var localized string	      FailedPlaceMessage;
var localized string	      FailedSpawnMessage;
var localized string	      EnteredMessage;
var	localized string	      MaxedOutMessage;
var localized string OvertimeMessage;
var localized string GlobalNameChange;
var localized string NewTeamMessage;
var localized string NewTeamMessageTrailer;
var localized string	NoNameChange;
var localized string VoteStarted;
var localized string VotePassed;
var localized string MustHaveStats;
var localized string CantBeSpectator;
var localized string CantBePlayer;
var localized string BecameSpectator;

var localized string NewPlayerMessage;
var localized string KickWarning;
var localized string NewSpecMessage, SpecEnteredMessage;

//
// Messages common to GameInfo derivatives.
//
static function string GetString(
	optional int Switch,
	optional bool bPRI1HUD,
	optional PlayerReplicationInfo RelatedPRI_1, 
	optional PlayerReplicationInfo RelatedPRI_2,
	optional Object OptionalObject
	)
{
	switch (Switch)
	{
		case 0:
			return Default.OverTimeMessage;
			break;
		case 1:
			if (RelatedPRI_1 == None)
                return Default.NewPlayerMessage;

			return RelatedPRI_1.playername$Default.EnteredMessage;
			break;
		case 2:
			if (RelatedPRI_1 == None)
				return "";

			return RelatedPRI_1.OldName@Default.GlobalNameChange@RelatedPRI_1.PlayerName;
			break;
		case 3:
			if (RelatedPRI_1 == None)
				return "";
			if (OptionalObject == None)
				return "";

            return RelatedPRI_1.playername@Default.NewTeamMessage@TeamInfo(OptionalObject).GetHumanReadableName()$Default.NewTeamMessageTrailer;
			break;
		case 4:
			if (RelatedPRI_1 == None)
				return "";

			return RelatedPRI_1.playername$Default.LeftMessage;
			break;
		case 5:
			return Default.SwitchLevelMessage;
			break;
		case 6:
			return Default.FailedTeamMessage;
			break;
		case 7:
			return Default.MaxedOutMessage;
			break;
		case 8:
			return Default.NoNameChange;
			break;
        case 9:
            return RelatedPRI_1.playername@Default.VoteStarted;
            break;
        case 10:
            return Default.VotePassed;
            break;
        case 11:
			return Default.MustHaveStats;
			break;
	case 12:
		return Default.CantBeSpectator;
		break;
	case 13:
		return Default.CantBePlayer;
		break;
	case 14:
		return RelatedPRI_1.PlayerName@Default.BecameSpectator;
		break;
	case 15:
		return Default.KickWarning;
		break;
	case 16:
            if (RelatedPRI_1 == None)
                return Default.NewSpecMessage;

            return RelatedPRI_1.playername$Default.SpecEnteredMessage;
            break;
	}
	return "";
}

defaultproperties
{
	NewPlayerMessage="A new player entered the game."
	NewSpecMessage="A spectator entered the game/"
	OverTimeMessage="Score tied at the end of regulation. Sudden Death Overtime!!!"
	GlobalNameChange="changed name to"
	NewTeamMessage="is now on"
	NewTeamMessageTrailer=""
    SwitchLevelMessage="Switching Levels"
    MaxedOutMessage="Server is already at capacity."
    EnteredMessage=" entered the game."
    SpecEnteredMessage=" joined as a spectator."
	FailedTeamMessage="Could not find team for player"
	FailedPlaceMessage="Could not find a starting spot"
	FailedSpawnMessage="Could not spawn player"
    LeftMessage=" left the game."
    NoNameChange="Name is already in use."
    MustHaveStats="Must have stats enabled to join this server."
    VoteStarted="started a vote."
    VotePassed="Vote passed."
    CantBeSpectator="Sorry, you cannot become a spectator at this time."
    CantBePlayer="Sorry, you cannot become an active player at this time."
    BecameSpectator="became a spectator."
	bIsConsoleMessage=true
	
	KickWarning="You are about to be kicked for idling!"
}