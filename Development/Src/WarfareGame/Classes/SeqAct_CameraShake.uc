class SeqAct_CameraShake extends SequenceAction;

var() float	Duration; 
var() vector RotAmplitude;
var() vector RotFrequency;
var() vector LocAmplitude; 
var() vector LocFrequency; 
var() float	FOVAmplitude;
var() float	FOVFrequency;

defaultproperties
{
	ObjName="Camera Shake"

	Duration=1.f
	RotAmplitude=(X=100,Y=100,Z=200)
	RotFrequency=(X=10,Y=10,Z=25)
	LocAmplitude=(X=0,Y=3,Z=5)
	LocFrequency=(X=1,Y=10,Z=20)
	FOVAmplitude=2
	FOVFrequency=5
}
