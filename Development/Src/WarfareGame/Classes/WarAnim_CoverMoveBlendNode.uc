/**
 * Handles blending between idle and the left/right moves for
 * cover nodes.  Movement is determined by looking at 
 * the current CoverAction, and the pawn velocity.
 * 
 * Nodes:
 * 0 => Idle
 * 1 => Move Right
 * 2 => Move Left
 * 3 => Lean Right
 * 4 => Lean Left
 * 5 => Step Right
 * 6 => Step Left
 * 7 => Blind Right
 * 8 => Blind Left
 * 9 => Stand up
 * 10 => Reload
 */
class WarAnim_CoverMoveBlendNode extends AnimNodeBlendList
	native;

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};

defaultproperties
{
}
