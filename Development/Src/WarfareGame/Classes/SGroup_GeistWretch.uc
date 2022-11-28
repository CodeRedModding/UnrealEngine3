class SGroup_GeistWretch extends WarSoundGroup
	config(SoundGroup)
	abstract;

/**
 * Wretch sound group
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

/** taking hit sounds */
var()	config	SoundCue	TakeHitSound;
/** Death sounds */
var()	config	SoundCue	DyingSound;


/** Player taking damage sound */
static function PlayTakeHitSound( Pawn P )
{
	P.PlaySound( default.TakeHitSound );
}

/** Player dying sound */
static function PlayDyingSound( Pawn P )
{
	P.PlaySound( default.DyingSound );
}

defaultproperties
{
	TakeHitSound=SoundCue'Geist_Wretch.Sounds.WretchGrowl'
	DyingSound=SoundCue'Geist_Wretch.Sounds.WretchDeath'
}