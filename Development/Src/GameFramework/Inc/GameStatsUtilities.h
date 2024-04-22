/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#ifndef __GAMESTATSUTILITIES_H__
#define __GAMESTATSUTILITIES_H__

typedef enum EGameStatsDimensions
{
	Totals=0,			// Match total aggregates
	RoundTotals,		// All round aggregates
	PlayerIndices,		// All player aggregates
	PlayerKillIndices,	// All player kill related aggregates
	PlayerDeathIndices,	// All player death related aggregates
	TeamIndices,		// Team aggregates
	WeaponTypes,		// Weapon aggregates
	ProjectileTypes,	// Projectile aggregates
	KillingDamageTypes,	// Damage types aggregated as the killing damage type (attacker)
	DeathByDamageTypes,	// Damage types aggregated as the killed by damage type (target)
	DamageDoneTypes,	// Damage types aggregated as damage done
	DamageReceivedTypes,// Damage types aggregated as damage received
	KillTypes,			// Kill type aggregates (attacker)
	KilledByKillTypes,  // Kill type aggregates (target)
	PawnTypes,			// Pawn type aggregates
	DimensionLast		// End Marker
} EGameStatsDimensions;

#define MAKE_DIM(x,y) x*100000+y

#endif //__GAMESTATSUTILITIES_H__