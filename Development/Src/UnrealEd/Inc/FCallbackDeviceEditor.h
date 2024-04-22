/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 *
 * FCallbackDeviceEditor.h: Callback device specifically for UnrealEd
 */


/**
 * This device forwards events to objects that have registered with it. It
 * maintains a list of Observers for each type of ECallbackEventType that is defined.
 */
class FCallbackEventDeviceEditor :
	public FCallbackEventObserver
{

	/**
	 * Caches the GWorld pointer for times when it's switched out
	 */
	UWorld* SavedGWorld;

public:
	FCallbackEventDeviceEditor() :
	  SavedGWorld(NULL)
	{
	}
	virtual ~FCallbackEventDeviceEditor()
	{
	}

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired. This version stores/restores the GWorld
	 * object before passing on the message.
	 *
	 * @param InType the event that was fired
	 * @param InViewport the viewport associated with this event
	 * @param InMessage the message for this event
	 */
	virtual void Send(ECallbackEventType InType,FViewport* InViewport,UINT InMessage);

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired. This version handles updating FEdMode's
	 * mode bars
	 *
	 * @param InType the event that was fired
	 * @param InEdMode the FEdMode that is changing
	 */
	virtual void Send(ECallbackEventType InType,FEdMode* InEdMode);

	/**
	 * Notifies all observers that are registered for this event type
	 * that the event has fired.
	 *
	 * If EventType is a windows message, this version stores/restores the GWorld object before passing on the message
	 * If EventMode is EventEditorMode is set, this version handles updating FEdMode's mode bars.
	 *
	 * @param	Parms	the parameters for the event
	 */
	virtual void Send( const FCallbackEventParameters& Parms );
};

/**
 * This device handles any callback queries from the engine
 */
class FCallbackQueryDeviceEditor :
	public FCallbackQueryDevice
{
public:
	virtual UBOOL Query(ECallbackQueryType InType,const FString& InString);
	virtual UBOOL Query( ECallbackQueryType InType, UObject* QueryObject );
	virtual INT Query( ECallbackQueryType InType, const FString& InString, UINT InMessage );
	virtual UBOOL Query( FCallbackQueryParameters& Parms );	
};

