/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class InterpTrackInstProperty extends InterpTrackInst
	native(Interpolation);

cpptext
{
	/**
	 * Retrieves the update callback from the interp property's metadata and stores it.
	 *
	 * @param InActor			Actor we are operating on.
	 * @param TrackProperty		Property we are interpolating.
	 */
	void SetupPropertyUpdateCallback(AActor* InActor, const FName& TrackPropertyName);

	/** 
	 * Tries to call the property update callback.
	 *
	 * @return TRUE if the callback existed and was called, FALSE otherwise.
	 */
	UBOOL CallPropertyUpdateCallback();

	/** Called when interpolation is done. Should not do anything else with this TrackInst after this. */
	virtual void TermTrackInst(UInterpTrack* Track);
}


/** Function to call after updating the value of the color property. */
var function	PropertyUpdateCallback;

/** Pointer to the UObject instance that is the outer of the color property we are interpolating on, this is used to process the property update callback. */
var object		PropertyOuterObjectInst;