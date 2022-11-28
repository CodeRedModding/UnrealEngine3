/**
 * WarSoundGroup
 * Defines a character sound group.
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class SGroup_COGGear extends WarSoundGroup
	config(SoundGroup)
	abstract;

/** taking hit sounds */
var()	config	SoundCue	TakeHitSound;
/** Death sounds */
var()	config	SoundCue	DyingSound;
/** Evade Sound */
var()	config	SoundCue	EvadeSound;


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

/** Player evading sound */
static function PlayEvadeSound( Pawn P )
{
	P.PlaySound( default.EvadeSound );
}

defaultproperties
{
	TakeHitSound=SoundCue'LD_TempUTSounds.mm_hit01'
	DyingSound=SoundCue'LD_TempUTSounds.mm_death04'
	EvadeSound=SoundCue'COG_Grunt.EvadeSoundCue'
}