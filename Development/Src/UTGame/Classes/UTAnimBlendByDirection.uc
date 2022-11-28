class UTAnimBlendByDirection extends UTAnimBlendBase
	native;

enum EBlendDirTypes
{
	FBDir_Forward,
	FBDir_Back,
	FBDir_Left,
	FBDir_Right,
	FBDir_None
};

/** If True, this node will automatically adjust the animation rate on any UTAnimSequence
 *  Children by the owner's velocity 
 */

var (Animations)	bool	bAdjustRateByVelocity;

/** Holds the last used direction */

var const EBlendDirTypes	LastDirection;


cpptext
{
	// AnimNode interface
	virtual	void TickAnim( float DeltaSeconds, float TotalWeight  );
	virtual void SetActiveChild( INT ChildIndex, FLOAT BlendTime );
	virtual EBlendDirTypes Get4WayDir();
}


defaultproperties
{
	bAdjustRateByVelocity=false
	LastDirection=FBDir_None

	Children(0)=(Name="Forward",Weight=1.0)
	Children(1)=(Name="Backward")
	Children(2)=(Name="Left")
	Children(3)=(Name="Right")
	bFixNumChildren=true
}


