class SoundNodeMixer extends SoundNode
	native(Sound)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

cpptext
{
	// USoundNode interface.

	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
	virtual INT GetMaxChildNodes() { return -1; }
}

defaultproperties
{
}