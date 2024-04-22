class ApexDestructibleDamageParameters extends Object
	native(Physics)
	hidecategories(Object);

enum EDamageParameterOverrideMode
{
	DPOM_Absolute,      // Parameters of TakeDamage of DamageCauserClass will be overridden with provided absolute values
	DPOM_Multiplier,    // Parameters of TakeDamage of DamageCauserClass will be multiplied with provided values
};

struct native DamageParameters
{
	var() EDamageParameterOverrideMode OverrideMode;
	var() float BaseDamage;
	var() float Radius;
	var() float Momentum;
};

struct native DamagePair
{
	var() name              DamageCauserName;
	var() DamageParameters  Params;
};

var()  Array<DamagePair>	DamageMap;

defaultproperties
{
}
