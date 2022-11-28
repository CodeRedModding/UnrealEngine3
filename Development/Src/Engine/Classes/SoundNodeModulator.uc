class SoundNodeModulator extends SoundNode
	native(Sound)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	editinlinenotify export noclear	distributionfloat	VolumeModulation;
var()	editinlinenotify export noclear	distributionfloat	PitchModulation;

cpptext
{
	// USoundNode interface.
		
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
}

defaultproperties
{
	Begin Object Class=DistributionFloatUniform Name=DistributionVolume
		Min=1
		Max=1
	End Object
	VolumeModulation=DistributionVolume
	
	Begin Object Class=DistributionFloatUniform Name=DistributionPitch
		Min=1
		Max=1
	End Object
	PitchModulation=DistributionPitch
}