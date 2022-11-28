class VictoryMessage extends UTLocalMessage;

var name VictorySoundName[6];

static function byte AnnouncementLevel(byte MessageIndex)
{
	return 2;
}

static function Name AnnouncementSound(byte MessageIndex)
{
	return Default.VictorySoundName[MessageIndex];
}
defaultproperties
{
    VictorySoundName(0)=Flawless_victory_Cue
    VictorySoundName(1)=Humiliating_defeat_Cue
    VictorySoundName(2)=You_Have_Won_the_Match_Cue
    VictorySoundName(3)=You_Have_Lost_the_Match_Cue
    VictorySoundName(4)=red_team_is_the_winner_Cue
    VictorySoundName(5)=blue_team_is_the_winner_Cue

}
