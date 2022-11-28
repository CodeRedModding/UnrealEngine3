//=============================================================================
// ScoreBoard
//=============================================================================

class ScoreBoard extends Hud
	config(Game);

var bool bDisplayMessages;

/**
 * The Main Draw loop for the hud.  Get's called before any messaging.  Should be subclassed
 */
function DrawHUD()
{
	UpdateGRI();
    UpdateScoreBoard();
}

function bool UpdateGRI()
{
    if (Level.GRI == None)
    {
		return false;
	}
    SortPRIArray();
    return true;
}

simulated function String FormatTime( int Seconds )
{
    local int Minutes, Hours;
    local String Time;

    if( Seconds > 3600 )
    {
        Hours = Seconds / 3600;
        Seconds -= Hours * 3600;

        Time = Hours$":";
	}
	Minutes = Seconds / 60;
    Seconds -= Minutes * 60;

    if( Minutes >= 10 )
        Time = Time $ Minutes $ ":";
    else
        Time = Time $ "0" $ Minutes $ ":";

    if( Seconds >= 10 )
        Time = Time $ Seconds;
    else
        Time = Time $ "0" $ Seconds;

    return Time;
}

simulated function UpdateScoreBoard();

simulated function bool InOrder( PlayerReplicationInfo P1, PlayerReplicationInfo P2 )
{
    if( P1.bOnlySpectator )
    {
        if( P2.bOnlySpectator )
            return true;
        else
            return false;
    }
    else if ( P2.bOnlySpectator )
        return true;

    if( P1.Score < P2.Score )
        return false;
    if( P1.Score == P2.Score )
    {
		if ( P1.Deaths > P2.Deaths )
			return false;
		if ( (P1.Deaths == P2.Deaths) && (PlayerController(P2.Owner) != None) && (LocalPlayer(PlayerController(P2.Owner).Player) != None) )
			return false;
	}
    return true;
}

simulated function SortPRIArray()
{
    local int i,j;
    local PlayerReplicationInfo tmp;

    for (i=0; i<Level.GRI.PRIArray.Length-1; i++)
    {
        for (j=i+1; j<Level.GRI.PRIArray.Length; j++)
        {
            if( !InOrder( Level.GRI.PRIArray[i], Level.GRI.PRIArray[j] ) )
            {
                tmp = Level.GRI.PRIArray[i];
                Level.GRI.PRIArray[i] = Level.GRI.PRIArray[j];
                Level.GRI.PRIArray[j] = tmp;
            }
        }
    }
}

defaultproperties
{
}