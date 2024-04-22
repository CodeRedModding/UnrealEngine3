class SoundNodeEnveloper extends SoundNode
	native( Sound )
	hidecategories( Object )
	editinlinenew;


var( Looping )		float	LoopStart;
var( Looping )		float	LoopEnd;
var( Looping )		float	DurationAfterLoop;
var( Looping )		int		LoopCount;
var( Looping )		bool	bLoopIndefinitely;
var( Looping )		bool	bLoop;

var( Envelope )		DistributionFloatConstantCurve VolumeInterpCurve;
var( Envelope )		DistributionFloatConstantCurve PitchInterpCurve;

defaultproperties
{
	Begin Object Class=DistributionFloatConstantCurve Name=VolumeInterpCurve
		ConstantCurve=(Points=((InVal=0.0,OutVal=1.0)))
	End Object
	VolumeInterpCurve=VolumeInterpCurve

	Begin Object Class=DistributionFloatConstantCurve Name=PitchInterpCurve
		ConstantCurve=(Points=((InVal=0.0,OutVal=1.0)))
	End Object
	PitchInterpCurve=PitchInterpCurve
}
