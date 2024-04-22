/**
 * AnimNodeBlendBySpeed
 *
 * Blends between child nodes based on the owners speed and the defined constraints.
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class AnimNodeBlendBySpeed extends AnimNodeBlendList
		native(Anim);

/** How fast they are moving this frame.							*/
var float			Speed;
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
/** Use acceleration instead of Velocity to determine speed */
var() bool	bUseAcceleration;
/** Optional delay before blending to the next channel */
var() float			BlendUpDelay, BlendDownDelay;
var transient float	BlendDelayRemaining;

cpptext
{
	// AnimNode interface
	
	/**
	 * Blend animations based on an Owner's velocity.
	 *
	 * @param DeltaSeconds	Time since last tick in seconds.
	 */
	virtual	void TickAnim(FLOAT DeltaSeconds);

	/**
	 * Resets the last channel on becoming active.	
	 */
	virtual void OnBecomeRelevant();

	virtual INT GetNumSliders() const { return 1; }
	virtual FLOAT GetSliderPosition(INT SliderIndex, INT ValueIndex);
	virtual void HandleSliderMove(INT SliderIndex, INT ValueIndex, FLOAT NewSliderValue);
	virtual FString GetSliderDrawValue(INT SliderIndex);
	
	// AnimNodeBlendBySpeed interface

	/** 
	 *	Function called to calculate the speed that should be used for this node. 
	 *	Allows subclasses to easily modify the speed used.
	 */
	 virtual FLOAT CalcSpeed();
}

defaultproperties
{
	BlendUpTime=0.1;
    BlendDownTime=0.1;
    BlendDownPerc=0.2;
	Constraints=(0,180,350,900);
}
