class UTRosterEntry extends Object
		editinlinenew;

var() class<UTPawn> PawnClass;
var() string PawnClassName;
var() string PlayerName;
var() enum EOrders
{
	ORDERS_None,
	ORDERS_Attack,
	ORDERS_Defend,
	ORDERS_Freelance,
	ORDERS_Support,
	ORDERS_Roam
} Orders;
var() bool bTaken;

var() class<Weapon> FavoriteWeapon;
var() float Aggressiveness;		// 0 to 1 (0.3 default, higher is more aggressive)
var() float Accuracy;			// -1 to 1 (0 is default, higher is more accurate)
var() float CombatStyle;		// 0 to 1 (0= stay back more, 1 = charge more)
var() float StrafingAbility;	// -1 to 1 (higher uses strafing more)
var() float Tactics;			// -1 to 1 (higher uses better team tactics)
var() float ReactionTime;

function Init() //amb
{
    if( PawnClassName != "" )
        PawnClass = class<UTPawn>(DynamicLoadObject(PawnClassName, class'class'));
    //log(self$" PawnClass="$PawnClass);
}

function PrecacheRosterFor(UTTeamInfo T);

function bool RecommendSupport()
{
	return ( Orders == ORDERS_Support );
}

function bool NoRecommendation()
{
	return ( Orders == ORDERS_None );
}

function bool RecommendDefense()
{
	return ( Orders == ORDERS_Defend );
}

function bool RecommendFreelance()
{
	return ( Orders == ORDERS_Freelance );
}

function bool RecommendAttack()
{
	return ( Orders == ORDERS_Attack );
}
/*FIXME - Merge in GamePlay.U

function InitBot(Bot B)
{
	B.FavoriteWeapon = FavoriteWeapon;
	B.Aggressiveness = FClamp(Aggressiveness, 0, 1);
	B.BaseAggressiveness = B.Aggressiveness;
	B.Accuracy = FClamp(Accuracy, -5, 5);
	B.StrafingAbility = FClamp(StrafingAbility, -5, 5);
	B.CombatStyle = FClamp(CombatStyle, 0, 1);
	B.Tactics = FClamp(Tactics, -5, 5);
	B.ReactionTime = FClamp(ReactionTime, -5, 5);
}
*/
defaultproperties
{
	Aggressiveness=+0.3
	Accuracy=+0.0
	CombatStyle=+0.2
}