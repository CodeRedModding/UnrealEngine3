class WarCameraMode_Default extends WarCameraMode
	config(Camera);

var()	config	vector	EvadePawnRelativeOffset;

/** Get Pawn's relative offset (from location based on pawn's rotation */
function vector GetPawnRelativeOffset( WarPawn P )
{
	if( P.EvadeDirection != ED_None )
	{
		return EvadePawnRelativeOffset;
	}
	else
	{
		return PawnRelativeOffset;
	}
}


defaultproperties
{
	EvadePawnRelativeOffset=(Z=-40)

	ViewOffsetHigh=(X=-16,Y=48,Z=128)
	ViewOffsetLow=(X=-96,Y=48,Z=-96)
	ViewOffsetMid=(X=-160,Y=48,Z=25)
	FOVAngle=70

	BlendTime=0.67
}