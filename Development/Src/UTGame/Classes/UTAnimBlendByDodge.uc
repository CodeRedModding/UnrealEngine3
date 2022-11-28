class UTAnimBlendByDodge extends UTAnimBlendByDirection
	native;

enum EDodgeBlends
{
	DODGEBLEND_None,
	DODGEBLEND_Forward,
	DODGEBLEND_Backward,
	DODGEBLEND_Left,
	DODGEBLEND_Right
};

/** These arrays anim seq names for the 4 directional sets (4 names per set)  */

var(Animations) name DodgeAnims[16];

/**  Which child is currently in control, -1 for none */

var const EDodgeBlends CurrentDodge;


cpptext
{
	virtual	void TickAnim( float DeltaSeconds, float TotalWeight  );
}

defaultproperties
{
	Children.Empty
	Children(0)=(Name="Falling",Weight=1.0)
	Children(1)=(Name="DodgeFalling",Weight=0.0)
	bFixNumChildren=true

}
