// This class of blend node will ramp the 'active' child up to 1.0

class AnimNodeBlendList extends AnimNodeBlendBase
	native(Anim)
	hidecategories(Object);

/** Array of target weights for each child. Size must be the same as the Children array. */
var		array<float>			TargetWeight;

/** How long before current blend is complete (ie. active child reaches 100%) */
var		float					BlendTimeToGo;

/** Child currently active - that is, at or ramping up to 100%. */
var		INT						ActiveChildIndex;

cpptext
{
	// AnimNode interface
	virtual void InitAnim( USkeletalMeshComponent* meshComp, UAnimNodeBlendBase* Parent );
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );

	// AnimNodeBlendList interface

	/** Set the child we want to have be active ie. full weight. BlendTime indicates how long to get there. */
	virtual void SetActiveChild( INT ChildIndex, FLOAT BlendTime );
}

native function SetActiveChild( INT ChildIndex, FLOAT BlendTime );

defaultproperties
{
	Children(0)=(Name="Child1")
	bFixNumChildren=false

	DebugIcon=Texture2D'EngineResources.AnimBlendListIcon'
}