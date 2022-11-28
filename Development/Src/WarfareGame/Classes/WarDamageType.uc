class WarDamageType extends DamageType
	abstract;
/** 
 * This class is the base class for Warfare damage types
 *
 * Created by:	Joe Graf
 * © 2004 Epic Games, Inc.
 */

/** True if this is a melee damage */
var() bool	bMeleeDamage;

defaultproperties
{
	KDamageImpulse=100

	// Short "pop" of damage
	Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform0
        Samples(0)=(LeftAmplitude=64,RightAmplitude=96,LeftFunction=WF_LinearDecreasing,RightFunction=WF_LinearDecreasing,Duration=0.25)
	End Object
	DamagedFFWaveform=ForceFeedbackWaveform0
	// Pretty violent rumble
    Begin Object Class=ForceFeedbackWaveform Name=ForceFeedbackWaveform1
        Samples(0)=(LeftAmplitude=100,RightAmplitude=100,LeftFunction=WF_Constant,RightFunction=WF_Constant,Duration=0.75)
	End Object
	KilledFFWaveform=ForceFeedbackWaveform1
}
