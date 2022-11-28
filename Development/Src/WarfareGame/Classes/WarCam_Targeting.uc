class WarCam_Targeting extends WarCameraMode
	config(Camera);

var() config	vector	CrouchedPawnRelativeOffset;

/** Get Pawn's relative offset (from location based on pawn's rotation */
function vector GetPawnRelativeOffset( WarPawn P )
{
	if( P.bIsCrouched && VSize(P.Velocity) < 1 )
	{
		return CrouchedPawnRelativeOffset;
	}
	else
	{
		return PawnRelativeOffset;
	}
}

defaultproperties
{
	ViewOffsetHigh=(X=16,Y=40,Z=16)
	ViewOffsetLow=(X=-60,Y=40,Z=-44)
	ViewOffsetMid=(X=-70,Y=40,Z=6)

	CrouchedPawnRelativeOffset=(Z=-15)

	FOVAngle=45
	BlendTime=0.15
}