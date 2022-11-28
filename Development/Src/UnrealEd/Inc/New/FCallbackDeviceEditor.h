/*=============================================================================
	FCallbackDeviceEditor.h: Callback device specifically for UnrealEd

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	FCallbackDeviceEditor.
-----------------------------------------------------------------------------*/

class FCallbackDeviceEditor : public FCallbackDevice
{
public:
	FCallbackDeviceEditor();
	virtual ~FCallbackDeviceEditor();

	virtual void Send( ECallbackType InType );
	virtual void Send( ECallbackType InType, INT InFlag );
	virtual void Send( ECallbackType InType, FVector InVector );
	virtual void Send( ECallbackType InType, FEdMode* InMode );
	virtual void Send( ECallbackType InType, UObject* InObject );
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
