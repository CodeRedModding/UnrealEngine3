class SoundNodeOscillator extends SoundNode
	native(Sound)
	collapsecategories
	hidecategories(Object)
	editinlinenew;

var()	editinlinenotify export noclear	distributionfloat	Amplitude;
var()	editinlinenotify export noclear	distributionfloat	Frequency;
var()	editinlinenotify export noclear	distributionfloat	Offset;
var()	editinlinenotify export noclear	distributionfloat	Center;

var()									bool				bModulatePitch;
var()									bool				bModulateVolume;

cpptext
{
	// USoundNode interface.
		
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
}

defaultproperties
{
	Begin Object Class=DistributionFloatUniform Name=DistributionAmplitude
		Min=0
		Max=0
	End Object
	Amplitude=DistributionAmplitude
	
	Begin Object Class=DistributionFloatUniform Name=DistributionFrequency
		Min=0
		Max=0
	End Object
	Frequency=DistributionFrequency
	
	Begin Object Class=DistributionFloatUniform Name=DistributionOffset
		Min=0
		Max=0
	End Object
	Offset=DistributionOffset
	
	Begin Object Class=DistributionFloatUniform Name=DistributionCenter
		Min=0
		Max=0
	End Object
	Center=DistributionCenter
}