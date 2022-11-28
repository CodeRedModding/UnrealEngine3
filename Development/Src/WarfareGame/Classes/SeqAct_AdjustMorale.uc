class SeqAct_AdjustMorale extends SequenceAction;

/** Reason for morale adjustment */
var() string MoraleText;

/** Amount to adjust morale */
var() float MoraleAdjustment;

defaultproperties
{
	ObjName="Adjust Morale"
	MoraleText="Good job soldier!"
	MoraleAdjustment=5.f
}
