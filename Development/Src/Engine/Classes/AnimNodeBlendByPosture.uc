/**
 * AnimNodeBlendByPosture.uc
 * Looks at the posture of the Pawn that owns this node and blends accordingly.
 *
 * Created by:	Daniel Vogel
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */

class AnimNodeBlendByPosture extends AnimNodeBlendList
		native(Anim);

/*
 * Note: this is obsolete. This class is going to be removed soon.
 */

cpptext
{
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );
}

defaultproperties
{

}