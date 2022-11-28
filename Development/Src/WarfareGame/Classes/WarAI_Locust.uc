class WarAI_Locust extends WarAI_Infantry
	abstract;

/** Long range distance */
var() float CombatRange_Long;

/** Medium range distance */
var() float CombatRange_Medium;

/**
 * Returns true if within long range.
 */
function bool IsLongRange()
{
	local Pawn nearEnemy;
	local float enemyDist;
	nearEnemy = GetNearestEnemy(enemyDist);
	return (nearEnemy != None &&
			enemyDist >= CombatRange_Long);
}

/**
 * Returns true if within medium range.
 */
function bool IsMediumRange()
{
	local Pawn nearEnemy;
	local float enemyDist;
	nearEnemy = GetNearestEnemy(enemyDist);
	return (nearEnemy != None &&
			enemyDist >= CombatRange_Medium &&
			enemyDist < CombatRange_Long);
}

/**
 * Returns true if less than medium range.
 */
function bool IsShortRange()
{
	local Pawn nearEnemy;
	local float enemyDist;
	nearEnemy = GetNearestEnemy(enemyDist);
	return (nearEnemy != None &&
			enemyDist < CombatRange_Medium);
}
