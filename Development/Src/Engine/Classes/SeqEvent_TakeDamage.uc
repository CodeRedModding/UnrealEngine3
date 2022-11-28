class SeqEvent_TakeDamage extends SequenceEvent;

/** Damage must exceed this value to be counted */
var() float MinDamageAmount;

/** Total amount of damage to take before activating the event */
var() float DamageThreshold;

/** Types of damage that are counted */
var() array<class<DamageType> > DamageTypes;

/** Current damage amount */
var transient float CurrentDamage;

/**
 * Searches DamageTypes[] for the specified damage type.
 */
final function bool IsValidDamageType(class<DamageType> inDamageType)
{
	local bool bValid;
	local int idx;
	bValid = true;
	if (DamageTypes.Length > 0)
	{
		bValid = false;
		for (idx = 0; idx < DamageTypes.Length && !bValid; idx++)
		{
			if (inDamageType == DamageTypes[idx])
			{
				bValid = true;
			}
		}
	}
	return bValid;
}

/**
 * Applies the damage and checks for activation of the event.
 */
final function HandleDamage(Actor inOriginator, Actor inInstigator, class<DamageType> inDamageType, int inAmount)
{
	if (inOriginator != None &&
		bEnabled &&
		inAmount >= MinDamageAmount &&
		IsValidDamageType(inDamageType) &&
		(!bPlayerOnly ||
		 class'Actor'.static.IsPlayerOwned(inInstigator)))
	{
		CurrentDamage += inAmount;
		if (CurrentDamage > DamageThreshold)
		{
			if (CheckActivate(inOriginator,inInstigator,false))
			{
				// reset the damage counter on activation
				CurrentDamage = 0.f;
			}
		}
	}
}

defaultproperties
{
	ObjName="Take Damage"

	DamageThreshold=100.f
}
