/**
 * WarSoundGroup
 * Defines a character sound group.
 *
 * Created by:	Laurent Delayen
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class WarSoundGroup extends Object
	abstract;

/** Player taking damage sound */
static function PlayTakeHitSound( Pawn P );

/** Player dying sound */
static function PlayDyingSound( Pawn P );

/** Player evading sound */
static function PlayEvadeSound( Pawn P );