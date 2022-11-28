/*=============================================================================
	UnInteraction.h: Interface definition for client interactions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker.
=============================================================================*/

//
//	UInteraction
//

class UInteraction: public UObject
{
	DECLARE_CLASS(UInteraction,UObject,0,Engine);
public:

	APlayerController*	Player;

	// Input event notifications: by default, these just call the script notifications.

	/**
	 * Forwards the input event on to script to process it
	 *
	 * @param Key The name of the key/button being pressed
	 * @param Event The type of key/button event being fired
	 * @param AmountDepressed The amount the button/key is being depressed (0.0 to 1.0)
	 *
	 * @return Whether the event was processed or not
	 */
	virtual UBOOL InputKey(FName Key,EInputEvent Event,FLOAT AmountDepressed);

	virtual UBOOL InputAxis(FName Key,FLOAT Delta,FLOAT DeltaTime);
	virtual UBOOL InputChar(TCHAR Character);
	virtual void Tick(FLOAT DeltaTime);
};


//
//	FKeyBind
//

struct FKeyBind
{
	FName		Name;
	FString		Command;
	BITFIELD	Control : 1 GCC_PACK(4),
				Shift : 1,
				Alt : 1;

	FKeyBind():
		Name(),
		Command(E_NoInit),
		Control(0),
		Shift(0),
		Alt(0)
	{}
};

//
//	UInput - An interaction with keys bound to script execs and variables.
//

class UInput : public UInteraction
{
	DECLARE_CLASS(UInput,UInteraction,CLASS_Transient|CLASS_Config,Engine)
	static const TCHAR* StaticConfigName() {return TEXT("Input");}

	TArrayNoInit<FKeyBind>	Bindings;
	TArrayNoInit<FName>		PressedKeys;

	// UInteraction interface.

	virtual UBOOL InputKey(FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual UBOOL InputAxis(FName Key,FLOAT Delta,FLOAT DeltaTime);
	virtual void Tick(FLOAT DeltaTime);
	UBOOL IsPressed( FName InKey );
	UBOOL IsCtrlPressed();
	UBOOL IsShiftPressed();
	UBOOL IsAltPressed();

	// UInput interface.

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);
	virtual void ResetInput();

	// Protected.

	BYTE* FindButtonName(const TCHAR* ButtonName);
	FLOAT* FindAxisName(const TCHAR* ButtonName);
	FString GetBind(FName Key);
	void ExecInputCommands(const TCHAR* Cmd,FOutputDevice& Ar);

	EInputEvent				CurrentEvent;
	FLOAT					CurrentDelta,
							CurrentDeltaTime;
	
	TMap<FName,void*>		NameToPtr;
	TArray<FLOAT*>			AxisArray;
};
