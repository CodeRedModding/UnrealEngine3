/**
 * Queries the current CoverType and activates the proper child
 * associated with each type.  Currently the mapping of ECoverType
 * is:
 * 
 * Children(0) - CT_Standing
 * Children(1) - CT_MidLevel
 * Children(2) - CT_Crouching
 */
class WarAnim_CoverBlendNode extends AnimNodeBlendList
	native;

cpptext
{
	virtual void TickAnim(FLOAT DeltaSeconds, FLOAT TotalWeight);
};

/** Cached version of the last cover type, to detect changes */
var CoverNode.ECoverType LastCoverType;
