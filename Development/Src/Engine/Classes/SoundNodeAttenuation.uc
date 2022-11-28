class SoundNodeAttenuation extends SoundNode
	native(Sound)
	collapsecategories
	hidecategories(Object)
	editinlinenew;


enum SoundDistanceModel
{
	ATTENUATION_Linear
};

var()									SoundDistanceModel	DistanceModel;

var()	editinlinenotify export noclear	distributionfloat	MinRadius;
var()	editinlinenotify export noclear	distributionfloat	MaxRadius;

var()									bool				bSpatialize;
var()									bool				bAttenuate;

cpptext
{
	// USoundNode interface.
			
	virtual void ParseNodes( class UAudioComponent* AudioComponent, TArray<FWaveInstance*>& WaveInstances );
}


defaultproperties
{
	Begin Object Class=DistributionFloatUniform Name=DistributionMinRadius
		Min=0
		Max=0
	End Object
	MinRadius=DistributionMinRadius
	
	Begin Object Class=DistributionFloatUniform Name=DistributionMaxRadius
		Min=0
		Max=0
	End Object
	MaxRadius=DistributionMaxRadius
}