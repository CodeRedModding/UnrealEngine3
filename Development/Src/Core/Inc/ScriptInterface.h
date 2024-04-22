/*=============================================================================
	ScriptInterface.h: Script interface definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SCRIPTINTERFACE_H__
#define __SCRIPTINTERFACE_H__

/**
 * FScriptInterface
 *
 * This utility class stores the UProperty data for a native interface property.  ObjectPointer and InterfacePointer point to different locations in the same UObject.
 */
class FScriptInterface
{
private:
	/**
	 * A pointer to a UObject that implements a native interface.
	 */
	UObject*	ObjectPointer;

	/**
	 * Pointer to the location of the interface object within the UObject referenced by ObjectPointer.
	 */
	void*		InterfacePointer;

public:
	/**
	 * Default constructor
	 */
	FScriptInterface( UObject* InObjectPointer=NULL, void* InInterfacePointer=NULL )
	: ObjectPointer(InObjectPointer), InterfacePointer(InInterfacePointer)
	{}

	/**
	 * Returns the ObjectPointer contained by this FScriptInterface
	 */
	FORCEINLINE UObject* GetObject() const
	{
		return ObjectPointer;
	}

	/**
	 * Returns the ObjectPointer contained by this FScriptInterface
	 */
	FORCEINLINE UObject*& GetObjectRef()
	{
		return ObjectPointer;
	}

	/**
	 * Returns the pointer to the interface
	 */
	FORCEINLINE void* GetInterface() const
	{
		// only allow access to InterfacePointer if we have a valid ObjectPointer.  This is necessary because the garbage collector will set ObjectPointer to NULL
		// without using the accessor methods
		return ObjectPointer != NULL ? InterfacePointer : NULL;
	}

	/**
	 * Sets the value of the ObjectPointer for this FScriptInterface
	 */
	FORCEINLINE void SetObject( UObject* InObjectPointer )
	{
		ObjectPointer = InObjectPointer;
		if ( ObjectPointer == NULL )
		{
			SetInterface(NULL);
		}
	}

	/**
	 * Sets the value of the InterfacePointer for this FScriptInterface
	 */
	FORCEINLINE void SetInterface( void* InInterfacePointer )
	{
		InterfacePointer = InInterfacePointer;
	}

	/**
	 * Comparison operator, taking a reference to another FScriptInterface
	 */
	FORCEINLINE UBOOL operator==( const FScriptInterface& Other ) const
	{
		return GetInterface() == Other.GetInterface() && ObjectPointer == Other.GetObject();
	}
	FORCEINLINE UBOOL operator!=( const FScriptInterface& Other ) const
	{
		return GetInterface() != Other.GetInterface() || ObjectPointer != Other.GetObject();
	}
};

/**
 * Templated version of FScriptInterface, which provides accessors and operators for referencing the interface portion of a UObject that implements a native interface.
 */
template< class InterfaceType > class TScriptInterface : public FScriptInterface
{
public:
	/**
	 * Default constructor
	 */
	TScriptInterface() {}
	/**
	 * Standard constructor.
	 *
	 * @param	SourceObject	a pointer to a UObject that implements the InterfaceType native interface class.
	 */
	template <class UObjectType> TScriptInterface( UObjectType* SourceObject )
	{
		(*this) = SourceObject;
	}
	/**
	 * Copy constructor
	 */
	TScriptInterface( const TScriptInterface& Other )
	{
		SetObject(Other.GetObject());
		SetInterface(Other.GetInterface());
	}

	/**
	 * Assignment operator.
	 *
	 * @param	SourceObject	a pointer to a UObject that implements the InterfaceType native interface class.
	 */
	template<class UObjectType> InterfaceType& operator=( UObjectType* SourceObject )
	{
		SetObject(SourceObject);
		
		InterfaceType* SourceInterface = SourceObject;
		SetInterface( SourceInterface );

		return *((InterfaceType*)GetInterface());
	}

	/**
	 * Comparison operator, taking a pointer to InterfaceType
	 */
	FORCEINLINE UBOOL operator==( const InterfaceType* Other ) const
	{
		return GetInterface() == Other;
	}
	FORCEINLINE UBOOL operator!=( const InterfaceType* Other ) const
	{
		return GetInterface() != Other;
	}

	/**
	 * Comparison operator, taking a reference to another TScriptInterface
	 */
	FORCEINLINE UBOOL operator==( const TScriptInterface& Other ) const
	{
		return GetInterface() == Other.GetInterface() && GetObject() == Other.GetObject();
	}
	FORCEINLINE UBOOL operator!=( const TScriptInterface& Other ) const
	{
		return GetInterface() != Other.GetInterface() || GetObject() != Other.GetObject();
	}

	/**
	 * Member access operator.  Provides transparent access to the interface pointer contained by this TScriptInterface
	 */
	FORCEINLINE InterfaceType* operator->() const
	{
		return (InterfaceType*)GetInterface();
	}

	/**
	 * Dereference operator.  Provides transparent access to the interface pointer contained by this TScriptInterface
	 *
	 * @return	a reference (of type InterfaceType) to the object pointed to by InterfacePointer
	 */
	FORCEINLINE InterfaceType& operator*() const
	{
		return *((InterfaceType*)GetInterface());
	}

	/**
	 * Boolean operator.  Provides transparent access to the interface pointer contained by this TScriptInterface.
	 *
	 * @return	TRUE if InterfacePointer is non-NULL.
	 */
	FORCEINLINE operator UBOOL() const
	{
		return GetInterface() != NULL;
	}

	friend FArchive& operator<<( FArchive& Ar, TScriptInterface& Interface )
	{
		UObject* ObjectValue = Interface.GetObject();
		Ar << ObjectValue;
		Interface.SetObject(ObjectValue);
		return Ar;
	}
};

#endif
