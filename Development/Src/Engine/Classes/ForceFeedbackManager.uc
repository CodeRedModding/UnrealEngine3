class ForceFeedbackManager extends Object within PlayerController
	native
	abstract
	transient;
/**
 * @author: joeg
 *
 * This class handles various waveform activities. It manages the waveform data
 * that is being played on any given gamepad at any given time. It is called by
 * the player controller to start/stop/pause a waveform for a given gamepad. It
 * is queried by the Viewport to get the current rumble state information
 * to apply that to the gamepad. It does this by evaluating the function
 * defined in the waveform sample data.
 */

/** Whether the player has disabled gamepad rumble or not (TCR C5-3) */
var bool bAllowsForceFeedback;

/** The currently playing waveform */
var ForceFeedbackWaveform FFWaveform;

/** Whether it was paused by the player controller or not */
var bool bIsPaused;

/** The current waveform sample being played */
var int CurrentSample;

/** The amount of time elapsed since the start of this waveform */
var float ElapsedTime;

/** The amount to scale all waveforms by (user settable) (TCR C5-3) */
var float ScaleAllWaveformsBy;

/**
 * Sets the waveform to play for the gamepad
 *
 * @param ForceFeedbackWaveform The waveform data to play
 */
simulated function PlayForceFeedbackWaveform(ForceFeedbackWaveform Waveform)
{
	// Zero out the current sample and duration and unpause if paused
	CurrentSample = 0;
	ElapsedTime = 0.0;
	bIsPaused = false;
	// Make sure the waveform is valid
	if (Waveform != None && Waveform.Samples.Length > 0 &&
		bAllowsForceFeedback == true)
	{
		// Set the wave form to play
		FFWaveform = Waveform;
	}
	else
	{
		FFWaveform = None;
	}
}

/**
 * Stops the waveform by nulling out the waveform
 */
simulated function StopForceFeedbackWaveform(optional ForceFeedbackWaveform Waveform)
{
	if (Waveform == None || Waveform == FFWaveform)
	{
		// Remove the current waveform
		FFWaveform = None;
	}
}

/**
 * Pauses/unpauses the playback of the waveform for the gamepad
 *
 * @param bPause True to pause, False to resume
 */
simulated function PauseWaveform(optional bool bPause)
{
	// Set the paused state for the gamepad
	bIsPaused = bPause;
}

cpptext
{
protected:
	/**
	 * Update the currently playing waveform sample
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 */
	virtual void UpdateWaveformData(FLOAT DeltaTime);

public:
	/**
	 * Applies the current waveform data to the gamepad/mouse/etc
	 * This function is platform specific
	 *
	 * @param DeltaTime The amount of elapsed time since the last update
	 * @param DeviceID The device that needs updating
	 */
	virtual void ApplyForceFeedback(FLOAT DeltaTime,INT DeviceID) {}
}

defaultproperties
{
	bAllowsForceFeedback=true
	ScaleAllWaveformsBy=1.0
}