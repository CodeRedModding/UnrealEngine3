/*=============================================================================
	UnStandardObjectPropagator.h: Code that can be shared by multiple
	object propagators to reduce code duplication
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/**
 * A class containing some shared code that most object propagators will want to use.
 * This is defined in Engine because it needs to act on actors. 
 */
class FStandardObjectPropagator : public FObjectPropagator
{
public:
	/**
	 * Handle updating an object after the property has been modified due to propagation
	 * Note that the actual modification of the property is not sharable between
	 * propagator, so this function is simply handling what happens after the 
	 * property changes
	 *
	 * @param Object	The object that has just had it's property changed
	 * @param Property	The property that was just changed
	 */
	virtual void PostPropertyChange(UObject* Object, UProperty* Property);

	/**
	 * Rename an object
	 * @param Object	The object to rename
	 * @param NewName	The name to give to the given object
	 */
	virtual void ProcessObjectRename(UObject* Object, const TCHAR* NewName);

	/** 
	 * Handle moving an actor due to object propagation
	 *
	 * @param Actor		The Actor that will be moved
	 * @param Location	The location to move the actor to
	 * @param Roation	The rotation to set the actor to
	 */
	virtual void ProcessActorMove(AActor* Actor, const FVector& Location, const FRotator& Rotation);

	/**
	 * Create an actor of the given class. This also handles renaming the components inside
	 * the new actor to match the components in the source actor
	 *
	 * @param ActorClass	The class of actor to spawn
	 * @param Location		The initial location to place the actor
	 * @param Rotation		The initial rotaiton of the actor
	 * @param ComponentTemplateAndNames		An array of FStrings that contains all the names of the component templates along with the components so that we can make sure the components in the new actor have the same name as the components in the old actor 
	 *
	 * @return The new actor that was spawned (might be NULL if ActorClass is invalid)
	 */
	virtual AActor* ProcessActorCreate(UClass* ActorClass, FName ActorName, const FVector& Location, const FRotator& Rotation, const TArray<FString>& ComponentTemplatesAndNames);

	/** 
	 * Handle deleting the given actor
	 *
	 * @param Actor		The actor to destroy
	 */
	virtual void ProcessActorDelete(AActor* Actor);

	/**
	 * Send an entire actor over the propagator, including any special properties
	 * @param Actor The actor to propagate
	 */
	virtual void PropagateActor(class AActor* Actor);
};
