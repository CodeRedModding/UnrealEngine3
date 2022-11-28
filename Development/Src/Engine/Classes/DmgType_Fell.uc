class DmgType_Fell extends DamageType
	abstract;

/* FIXMESTEVE     
    GibPerterbation=0.5
    bCausedByWorld=true
*/
defaultproperties
{
	DeathString="%k pushed %o over the edge."
	MaleSuicide="%o left a small crater"
	FemaleSuicide="%o left a small crater"

    GibModifier=2.0
    bLocationalHit=false
}
