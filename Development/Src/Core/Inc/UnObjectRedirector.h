/*=============================================================================
	UnRedirector.h: Object redirector definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * This class will redirect an object load to another object, so if an object is renamed
 * to a different package or group, external references to the object can be found
 */
class UObjectRedirector : public UObject
{
	DECLARE_CLASS_INTRINSIC(UObjectRedirector, UObject, 0, Core)
	NO_DEFAULT_CONSTRUCTOR(UObjectRedirector)

	// Variables.
	UObject*		DestinationObject;

	/**
	 * Static constructor, called once during static initialization of global variables for native 
	 * classes. Used to e.g. register object references for native- only classes required for realtime
	 * garbage collection or to associate UProperties.
	 */
	void StaticConstructor();

	// UObject interface.
	virtual void PreSave();
	void Serialize( FArchive& Ar );

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	return TRUE if property values were added to the map.
	 */
	virtual UBOOL GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags=0 ) const;
};

/**
 * Callback structure to respond to redirect-followed callbacks to determine
 * if a redirector was followed from a single object name. Will auto
 * register and unregister the callback on construction/destruction
 */
class FScopedRedirectorCatcher : public FCallbackEventDevice
{
public:
	/**
	 * Constructor for the callback device, will register itself with the GCallbackEvent
	 *
	 * @param InObjectPathNameToMatch Full pathname of the object refrence that is being compiled by script
	 */
	FScopedRedirectorCatcher(const FString& InObjectPathNameToMatch);

	/**
	 * Destructor. Will unregister the callback
	 */
	~FScopedRedirectorCatcher();

	/**
	 * Returns whether or not a redirector was followed for the ObjectPathName
	 */
	inline UBOOL WasRedirectorFollowed()
	{
		return bWasRedirectorFollowed;
	}

	/**
	 * Responds to CALLBACK_RedirectorFollowed. Records all followed redirections
	 * so they can be cleaned later.
	 *
	 * @param InType Callback type (should only be CALLBACK_RedirectorFollowed)
	 * @param InString Name of the package that pointed to the redirect
	 * @param InObject The UObjectRedirector that was followed
	 */
	virtual void Send( ECallbackEventType InType, const FString& InString, UObject* InObject);

private:
	/** The full path name of the object that we want to match */
	FString ObjectPathNameToMatch;

	/** Was a redirector followed, ONLY for the ObjectPathNameToMatch? */
	UBOOL bWasRedirectorFollowed;
};
