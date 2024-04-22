/*=============================================================================
	UnRedirector.cpp: Object redirector implementation.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"

/*-----------------------------------------------------------------------------
	UObjectRedirector
-----------------------------------------------------------------------------*/

/**
 * Static constructor, called once during static initialization of global variables for native 
 * classes. Used to e.g. register object references for native- only classes required for realtime
 * garbage collection or to associate UProperties.
 */
void UObjectRedirector::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference( STRUCT_OFFSET( UObjectRedirector, DestinationObject ) );
}

// If this object redirector is pointing to an object that won't be serialized anyway, set the RF_Transient flag
// so that this redirector is also removed from the package.
void UObjectRedirector::PreSave()
{
	if (DestinationObject == NULL
	||	DestinationObject->HasAnyFlags(RF_Transient)
	||	DestinationObject->IsIn(GetTransientPackage()) )
	{
		Modify();
		SetFlags(RF_Transient);

		if ( DestinationObject != NULL )
		{
			DestinationObject->Modify();
			DestinationObject->SetFlags(RF_Transient);
		}
	}
}

void UObjectRedirector::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	Ar << DestinationObject;
}

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
UBOOL UObjectRedirector::GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags/*=0*/ ) const
{
	UObject* StopOuter = NULL;

	// determine how the caller wants object values to be formatted
	if ( (ExportFlags&PPF_SimpleObjectText) != 0 )
	{
		StopOuter = GetOutermost();
	}

	out_PropertyValues.Set(TEXT("DestinationObject"), DestinationObject->GetFullName(StopOuter));
	return TRUE;
}

IMPLEMENT_CLASS(UObjectRedirector);

/**
 * Constructor for the callback device, will register itself with the GCallbackEvent
 *
 * @param InObjectPathNameToMatch Full pathname of the object refrence that is being compiled by script
 */
FScopedRedirectorCatcher::FScopedRedirectorCatcher(const FString& InObjectPathNameToMatch)
: ObjectPathNameToMatch(InObjectPathNameToMatch)
, bWasRedirectorFollowed(FALSE)
{
	// register itself on construction to see if the object is a redirector 
	GCallbackEvent->Register(CALLBACK_RedirectorFollowed, this);
}

/**
 * Destructor. Will unregister the callback
 */
FScopedRedirectorCatcher::~FScopedRedirectorCatcher()
{
	// register itself on construction to see if the object is a redirector 
	GCallbackEvent->Unregister(CALLBACK_RedirectorFollowed, this);
}


/**
 * Responds to CALLBACK_RedirectorFollowed. Records all followed redirections
 * so they can be cleaned later.
 *
 * @param InType Callback type (should only be CALLBACK_RedirectorFollowed)
 * @param InString Name of the package that pointed to the redirect
 * @param InObject The UObjectRedirector that was followed
 */
void FScopedRedirectorCatcher::Send( ECallbackEventType InType, const FString& InString, UObject* InObject)
{
	check(InType == CALLBACK_RedirectorFollowed);
	// this needs to be the redirector
	check(InObject->IsA(UObjectRedirector::StaticClass()));

	// if the path of the redirector was the same as the path to the object constant
	// being compiled, then the script code has a text reference to a redirector, 
	// which will cause FixupRedirects to break the reference
	if (InObject->GetClass() != UClass::StaticClass()
	||	!(InObject->HasAnyFlags(RF_ClassDefaultObject) && GIsUCCMake)
	||	InObject->GetPathName() == ObjectPathNameToMatch)
	{
		bWasRedirectorFollowed = TRUE;
	}
}
