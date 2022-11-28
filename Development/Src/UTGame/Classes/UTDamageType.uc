/**
 * UTDamageType
 *
 *
 * Created By:	Joe Wilcox
 * Copyright:	(c) 2004
 * Company:		Epic Games
*/

class UTDamageType extends DamageType;


/**
 * Returns a list of effects to spawn when this damage is applied
 *
 */

static function GetHitEffects(out array<Emitter> HitEffects, int VictimHealth );

