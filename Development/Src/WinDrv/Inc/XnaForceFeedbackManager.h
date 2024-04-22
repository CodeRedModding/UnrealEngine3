/*=============================================================================
	XnaForceFeedbackManager.h: UXnaForceFeedbackManager definition
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * This class manages the waveform data for a forcefeedback device,
 * specifically for the xbox gamepads.
 */
class UXnaForceFeedbackManager : public UForceFeedbackManager
{
#if XBOX
	DECLARE_CLASS_INTRINSIC(UXnaForceFeedbackManager,UForceFeedbackManager,CLASS_Config|CLASS_Transient|0,XeDrv)
#else
	DECLARE_CLASS_INTRINSIC(UXnaForceFeedbackManager,UForceFeedbackManager,CLASS_Config|CLASS_Transient|0,WinDrv)
#endif

	/**
	 * Sets the defaults for this class since native only classes can't inherit
	 * script class defaults.
	 */
	UXnaForceFeedbackManager(void)
	{
		bAllowsForceFeedback = 1;
		ScaleAllWaveformsBy = 1.f;
	}

	/**
	 * Applies the current waveform data to the gamepad/mouse/etc
	 * This function is platform specific
	 *
	 * @param DeviceID The device that needs updating
	 * @param DeltaTime The amount of elapsed time since the last update
	 */
	virtual void ApplyForceFeedback(INT DeviceID,FLOAT DeltaTime);

private:
	/**
	 * Determines the amount of rumble to apply to the left and right motors of the
	 * specified gamepad
	 *
	 * @param flDelta The amount of time that has elapsed
	 * @param LeftAmount The amount to make the left motor spin
	 * @param RightAmount The amount to make the right motor spin
	 */
	void GetRumbleState(FLOAT flDelta,WORD& LeftAmount,WORD& RightAmount);

	/**
	 * Figures out which function to call to evaluate the current rumble amount. It
	 * internally handles the constant function type.
	 *
	 * @param eFunc Which function to apply to the rumble
	 * @param byAmplitude The max value to apply for this function
	 * @param flDelta The amount of time that has elapsed
	 * @param flDuration The max time for this waveform
	 *
	 * @return The amount to rumble the gamepad by
	 */
	WORD EvaluateFunction(BYTE eFunc,BYTE byAmplitude,FLOAT flDelta,FLOAT flDuration);


	/** Clear any vibration going on this device right away. */
	virtual void ForceClearWaveformData(INT DeviceID);
};

