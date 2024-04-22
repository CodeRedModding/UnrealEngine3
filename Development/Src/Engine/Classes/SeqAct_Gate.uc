/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class SeqAct_Gate extends SequenceAction
	native(Sequence);

cpptext
{
	virtual void PostLoad();

#if WITH_EDITOR
	virtual FString GetAutoComment() const;

	// Gives op a chance to add realtime debugging information (when enabled)
	virtual void GetRealtimeComments(TArray<FString> &OutComments);
#endif

	void Activated()
	{
		UBOOL bWasOpen = bOpen;
		// first look for an open/close impulse
		if (InputLinks(1).bHasImpulse)
		{
			// open the gate
			bOpen = TRUE;

			//Setup the next autocount threshold
			CurrentCloseCount = ActivateCount + AutoCloseCount;
			if (InputLinks(0).bHasImpulse)
			{
				//One of the uses occurs this go around
				CurrentCloseCount--;
			}
		}
		else
		if (InputLinks(2).bHasImpulse)
		{
			// close the gate
			bOpen = FALSE;
		}
		else
		if (InputLinks(3).bHasImpulse)
		{
			// toggle the gate
			bOpen = !bOpen;

			if (bOpen)
			{
				//Setup the next autocount threshold
				CurrentCloseCount = ActivateCount + AutoCloseCount;
				if (InputLinks(0).bHasImpulse)
				{
					//One of the uses occurs this go around
					CurrentCloseCount--;
				}
			}
		}
		KISMET_LOG(TEXT("- Gate status: %s (was: %s)"),bOpen?TEXT("Open"):TEXT("Closed"),bWasOpen?TEXT("Open"):TEXT("Closed"));
		// next check for an activation impulse
		if (bOpen && InputLinks(0).bHasImpulse)
		{
			if (!OutputLinks(0).bDisabled && 
				!(OutputLinks(0).bDisabledPIE && GIsEditor))
			{
				OutputLinks(0).bHasImpulse = TRUE;
			}
			if (AutoCloseCount > 0 && ActivateCount >= CurrentCloseCount)
			{
				//Closed due to autocount exceeded
				bOpen = FALSE;
			}
		}
	}
}

/** Is this gate currently open? */
var() bool bOpen<autocomment=true>;

/** Auto close after this many activations */
var() int AutoCloseCount;

/** Current threshold for closing the gate (increments to keep pace with ActivateCount)  */
var int CurrentCloseCount;

defaultproperties
{
	ObjName="Gate"
	ObjCategory="Misc"

	bSuppressAutoComment=false
	bOpen=TRUE
	bAutoActivateOutputLinks=false

	InputLinks(0)=(LinkDesc="In")
	InputLinks(1)=(LinkDesc="Open")
	InputLinks(2)=(LinkDesc="Close")
	InputLinks(3)=(LinkDesc="Toggle")

	VariableLinks.Empty
}
