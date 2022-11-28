// This node blends two animations together. 
// It also optionally allows you to only take animation below a particular bone from Child2.

class AnimNodeBlend extends AnimNodeBlendBase
	native(Anim)
	hidecategories(Object);

var		float		Child2Weight;

var		float		Child2WeightTarget;
var		float		BlendTimeToGo; // Seconds

cpptext
{
	// AnimNode interface
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );

	// AnimNodeBlend interface

	/** Set the desired weight for the second animation (element 1 in the Children array) and how long to get there. */
	virtual void SetBlendTarget( FLOAT BlendTarget, FLOAT BlendTime );
}

/**
 * Set desired balance of this blend.
 * 
 * @param BlendTarget	Target amount of weight to put on Children(1) (second child). Between 0.0 and 1.0.
 *						1.0 means take all animation from second child.
 * @param BlendTime		How long to take to get to BlendTarget.
 */
native final function SetBlendTarget( float BlendTarget, float BlendTime );

defaultproperties
{
	Children(0)=(Name="Child1",Weight=1.0)
	Children(1)=(Name="Child2")
	bFixNumChildren=true
}