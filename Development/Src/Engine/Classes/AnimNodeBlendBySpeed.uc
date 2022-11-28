/**
 * AnimNodeBlendBySpeed
 *
 * Blends between child nodes based on the owners speed and the defined constraints.
 *
 * Created by:	Daniel Vogel
 * Copyright:	(c) 2004
 * Company:		Epic Games, Inc.
 */
class AnimNodeBlendBySpeed extends AnimNodeBlendList
		native(Anim);

/** How fast were they moving last frame							*/
var float			LastSpeed;			
/** Last Channel being used											*/
var int				LastChannel;		
/** How fast to blend when going up									*/
var() float			BlendUpTime;		
/** How fast to blend when going down								*/
var() float			BlendDownTime;
/** When should we start blending back down							*/
var() float			BlendDownPerc;
/** Weights/ constraints used for transition between child nodes	*/
var() array<float>	Constraints;

cpptext
{
	/**
	 * Blend animations based on an Owner's velocity.
	 *
	 * @param DeltaSeconds	Time since last tick in seconds.
	 */
	virtual	void TickAnim( FLOAT DeltaSeconds, FLOAT TotalWeight );

	/**
	 * Resets the last channel on becoming active.	
	 */
	virtual void OnBecomeActive();
}

defaultproperties
{
	BlendUpTime=0.1;
    BlendDownTime=0.1;
    BlendDownPerc=0.2;
	Constraints=(0,180,350,900);
}