class RB_ConstraintInstance extends Object
	hidecategories(Object)
	native(Physics);

cpptext
{
	void InitConstraint(AActor* actor1, AActor* actor2, class URB_ConstraintSetup* setup, FLOAT Scale, AActor* inOwner);
	void TermConstraint();
}

/** 
 *	Actor that owns this constraint instance.
 *	Could be a ConstraintActor, or an actor using a PhysicsAsset containing this constraint.
 */
var		const transient Actor		Owner;			

/** Internal use. Physics-engine representation of this constraint. */
var		const native pointer		ConstraintData;

defaultproperties
{

}