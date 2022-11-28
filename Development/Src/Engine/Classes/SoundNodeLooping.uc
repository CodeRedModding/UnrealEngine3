class SoundNodeLooping extends SoundNode
	native(Sound)
	collapsecategories
	hidecategories(Object)
	editinlinenew;


var()	editinlinenotify export noclear	distributionfloat	LoopCount;

cpptext
{
	// USoundNode interface.

	virtual UBOOL Finished(  class UAudioComponent* AudioComponent );

	/**
	 * Returns whether this sound node will potentially loop leaf nodes.
	 *
	 * @return TRUE
	 */
	virtual UBOOL IsPotentiallyLooping() { return TRUE; }

	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
}


defaultproperties
{
	Begin Object Class=DistributionFloatUniform Name=DistributionLoopCount
		Min=0
		Max=0
	End Object
	LoopCount=DistributionLoopCount
}