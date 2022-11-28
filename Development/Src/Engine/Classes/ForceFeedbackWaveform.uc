class ForceFeedbackWaveform extends Object
	noexport
	native;
/**
 * @author joeg
 *
 * This class manages the waveform data for a forcefeedback device,
 * specifically for the xbox gamepads.
 */

/** The type of function that generates the waveform sample */
enum EWaveformFunction
{
	WF_Constant,
	WF_LinearIncreasing,
	WF_LinearDecreasing,
	WF_Sin0to90,
	WF_Sin90to180,
	WF_Sin0to180,
	WF_Noise
};

/** Holds a single sample's information */
struct native WaveformSample
{
	/**
	 * Use a byte with a range of 0 to 100 to represent the percentage of
	 * "on". This cuts the data needed to store the waveforms in half.
	 */
	var() byte LeftAmplitude;
	var() byte RightAmplitude;
	/** For function generated samples, the type of function */
	var() EWaveformFunction LeftFunction;
	var() EWaveformFunction RightFunction;
	/** The amount of time this sample plays */
	var() float Duration;
};

/** Whether this waveform should be looping or not */
var() bool bIsLooping;

/** The list of samples that make up this waveform */
var() array<WaveformSample> Samples;
