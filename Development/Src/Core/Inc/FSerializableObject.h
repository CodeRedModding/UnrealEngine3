/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
/*=============================================================================
	FSerializableObject.h: Abstract base class for serializable non- UObject
	                       objects.
=============================================================================*/

#ifndef _OBJECTSERIALIZER_H_
#define _OBJECTSERIALIZER_H_

class FSerializableObject;

/**
 * This nested class is used to provide a UObject interface between non
 * UObject classes and the UObject system. It handles forwarding all
 * calls of Serialize() to objects/ classes that register with it.
 */
class UObjectSerializer : public UObject
{
	/**
	 * This is the list of objects that are requesting serialization events
	 */
	TArray<FSerializableObject*> SerializableObjects;

public:
	DECLARE_CLASS_INTRINSIC(UObjectSerializer,UObject,CLASS_Transient,Core);

#if ANDROID
	// Not having this default constructor is apparently fine on almost every platform
	// except Android, possibly because of the particular GCC version they're using
	UObjectSerializer() { }
#endif

	/**
	 * Callback used to allow object register its direct object references that are not already covered by
	 * the token stream.
	 *
	 * @param ObjectArray	array to add referenced objects to via AddReferencedObject
	 */
	void AddReferencedObjects( TArray<UObject*>& ObjectArray );

	/**
	 * Adds an object to the serialize list
	 *
	 * @param Object The object to add to the list
	 */
	void AddObject(FSerializableObject* Object);

	/**
	 * Removes a window from the list so it won't receive serialization events
	 *
	 * @param Object The object to remove from the list
	 */
	void RemoveObject(FSerializableObject* Object);

	/**
	 * Forwards this call to all registered objects so they can serialize
	 * any UObjects they depend upon
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar);
	
	/**
	 * Destroy function that gets called before the object is freed. This might
	 * be as late as from the destructor.
	 */
	virtual void FinishDestroy();
};


/**
 * This class provides common registration for serialization during garbage
 * collection. It is an abstract base class requiring you to implement the
 * Serialize() method.
 */
class FSerializableObject
{
public:
	/**
	 * The static object serializer object that is shared across all
	 * serializable objects.
	 */
	static UObjectSerializer* GObjectSerializer;

	/**
	 * Initializes the global object serializer and adds it to the root set.
	 */
	static void StaticInit(void)
	{
		if (GObjectSerializer == NULL)
		{
			GObjectSerializer = new UObjectSerializer;
			GObjectSerializer->AddToRoot();
		}
	}

	/**
	 * Tells the global object that forwards serialization calls on to objects
	 * that a new object is requiring serialization.
	 */
	FSerializableObject(void)
	{
		StaticInit();
		check(GObjectSerializer);
		// Add this instance to the serializer's list
		GObjectSerializer->AddObject(this);
	}

	/**
	 * Removes this instance from the global serializer's list
	 */
	virtual ~FSerializableObject(void)
	{
		// GObjectSerializer will be NULL if this object gets destroyed after the exit purge.
		if( GObjectSerializer )
		{
			// Remove this instance from the serializer's list
			GObjectSerializer->RemoveObject(this);
		}
	}

	/**
	 * Pure virtual that must be overloaded by the inheriting class. Use this
	 * method to serialize any UObjects contained that you wish to keep around.
	 *
	 * @param Ar The archive to serialize with
	 */
	virtual void Serialize(FArchive& Ar) = 0;
};

#endif
