/**
 * Handles determing which part of the tree to blend based
 * on the current state of the WarfarePawn, either using the
 * normal movement blending or the various cover blends.
 * 
 * @see		AnimTree_COGGear
 */
class WarAnim_BaseBlendNode extends AnimNodeBlendList
	native;

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);
}

var WarPawn.EEvadeDirection LastEvadeDirection;
