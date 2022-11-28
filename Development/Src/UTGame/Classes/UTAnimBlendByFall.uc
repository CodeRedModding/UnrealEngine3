class UTAnimBlendByFall extends UTAnimBlendBase
	Native;

enum EBlendFallTypes
{
	FBT_Up,
	FBT_Down,
	FBT_PreLand,
	FBT_Land,
	FBT_None
};


/** This fall is used for dodging.  When it's true, the Falling node progress through 
  * it's states in an alternate manner
  */

var bool 					bDodgeFall;		

/** The current state this node believes the pawn to be in */

var const EBlendFallTypes 	FallState;				

/** Set internally, this variable holds the size of the velocity at the last tick */

var const float				LastFallingVelocity;	

cpptext
{
	virtual	void TickAnim( float DeltaSeconds, float TotalWeight  );
	virtual void SetActiveChild( INT ChildIndex, FLOAT BlendTime );
	virtual void OnChildAnimEnd(UAnimNodeSequence* Child);
	virtual void ChangeFallState(EBlendFallTypes NewState);
}


defaultproperties
{
	FallState=FBT_None
	bDodgeFall=false
	

	Children(0)=(Name="Up",Weight=1.0)
	Children(1)=(Name="Down")
	Children(2)=(Name="Pre-Land")
	Children(3)=(Name="Land")
	bFixNumChildren=true


}
