/*=============================================================================
	UnStandardObjectPropagator.h: Code that can be shared by multiple
	object propagators to reduce code duplication
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"

/**
 * Handle updating an object after the property has been modified due to propagation
 * Note that the actual modification of the property is not sharable between
 * propagator, so this function is simply handling what happens after the 
 * property changes
 *
 * @param Object	The object that has just had it's property changed
 * @param Property	The property that was just changed
 */
void FStandardObjectPropagator::PostPropertyChange(UObject* Object, UProperty* Property)
{
	// if the object was an actor, refresh it's components
	AActor* Actor = Cast<AActor>(Object);
	if (Actor)
	{
		// make sure actor components are dirty so they are recreated
		for(INT ComponentIndex = 0;ComponentIndex < Actor->Components.Num();ComponentIndex++)
		{
			UActorComponent* Component = Actor->Components(ComponentIndex); 
			if (Component)
			{
				Component->BeginDeferredReattach();
			}
		}
		Actor->ConditionalForceUpdateComponents(FALSE,FALSE);

		if (Property->GetFName() == NAME_Name)
		{
			// check for special case of ambient sounds
			AAmbientSoundSimple* Sound = Cast<AAmbientSoundSimple>(Actor);
			if (Sound && Sound->AudioComponent)
			{
				// Propagate instanced objects.
				Sound->SoundCueInstance->FirstNode	= Sound->SoundNodeInstance;
				Sound->AudioComponent->SoundCue		= Sound->SoundCueInstance;
				Sound->AmbientProperties			= Sound->SoundNodeInstance;
				Sound->AudioComponent->Play();
			}
		}
	}
	else
	{
		// otherwise, check if it's an actor component
		UActorComponent* Component = Cast<UActorComponent>(Object);
		if (Component)
		{
			Component->BeginDeferredReattach();
			// if it has an actor owner, tell the actor to update all components (this will also initialize uninitialized components)
			if(Component->GetOwner())
			{
				Component->GetOwner()->ConditionalForceUpdateComponents(FALSE,FALSE);
			}
			// otherwise, update it directly
			else
			{
				FComponentReattachContext ReattachContext(Component);
			}
		}
	}
}

/**
 * Rename an object
 * @param Object	The object to rename
 * @param NewName	The name to give to the given object
 */
void FStandardObjectPropagator::ProcessObjectRename(UObject* Object, const TCHAR* NewName)
{
	Object->Rename(NewName, NULL);
}

/** 
 * Handle moving an actor due to object propagation
 *
 * @param Actor		The Actor that will be moved
 * @param Location	The location to move the actor to
 * @param Roation	The rotation to set the actor to
 */
void FStandardObjectPropagator::ProcessActorMove(AActor* Actor, const FVector& Location, const FRotator& Rotation)
{
	// we need to shutdown the actor's physics to let us move it
	Actor->TermRBPhys(NULL);

	// Detach the actor's components, since their attachment state assumes that bStatic and bMovable never change at runtime.
	Actor->ClearComponents();

	// a hacky way to let staticmesh actors get moved in a world that has already BegunPlay
	Actor->Location = Location;
	Actor->Rotation = Rotation;
	
	// make lights update
	Actor->ConditionalForceUpdateComponents();
	Actor->InvalidateLightingCache();

	// restart the actor's physics
	Actor->InitRBPhys();
}

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
AActor* FStandardObjectPropagator::ProcessActorCreate(UClass* ActorClass, FName ActorName, const FVector& Location, const FRotator& Rotation, const TArray<FString>& ComponentTemplatesAndNames)
{
	// if the class isn't an actor, fail silently
	if (!ActorClass->IsChildOf(AActor::StaticClass()))
	{
		return NULL;
	}

	// a hacky way to let static actors get spawn in a world that has already BegunPlay
	UBOOL bOldBegunPlay = GWorld->GetWorldInfo()->bBegunPlay;
	if (ActorClass->GetDefaultActor()->IsStatic() || ActorClass->GetDefaultActor()->bNoDelete)
	{
		GWorld->GetWorldInfo()->bBegunPlay = FALSE;
	}
	// spawn the actor
	AActor* Actor = GWorld->SpawnActor(ActorClass, ActorName, Location, Rotation);

	GWorld->GetWorldInfo()->bBegunPlay = bOldBegunPlay;

	if (!Actor)
	{
		return NULL;
	}

	// loop through the templates names, setting the names in our new components as needed
	// Note the array needs to be TemplateName, ComponentName, Template Name, Component Name, etc
	for (INT TemplateIndex = 0; TemplateIndex < ComponentTemplatesAndNames.Num(); TemplateIndex += 2)
	{
		// make an FName of the string we are searching on
		FName KeyName(*ComponentTemplatesAndNames(TemplateIndex));

		// look to see if it's a property (for non-component subobjects)
		// @todo: split components and subobjects, as keynames could conflict
		UBOOL bFoundProperty = FALSE;
		for (TFieldIterator<UObjectProperty> It(ActorClass); It && !bFoundProperty; ++It)
		{
			// did we find a property by that name?
			if (It->GetFName() == KeyName)
			{
				UObject* ObjValue;
				// if so, rename the objhect its pointing to to what is specified
				It->CopySingleValue(&ObjValue, (BYTE*)Actor + It->Offset);
				if (ObjValue)
				{
					//debugf(TEXT("Renaming subobject '%s' to '%s'"), *ObjValue->GetFullName(), *ComponentTemplatesAndNames(TemplateIndex+1));
					ObjValue->Rename(*ComponentTemplatesAndNames(TemplateIndex+1), NULL);
				}
				bFoundProperty = TRUE;
			}
		}
		

		for (INT CompIndex = 0; CompIndex < Actor->Components.Num(); CompIndex++)
		{
			if (!Actor->Components(CompIndex))
			{
				debugf(TEXT("%s has a null component in slot %d"), *Actor->GetName(), CompIndex);
				continue;
			}
			// if we found a component with the same name as the template...
			if (KeyName == Actor->Components(CompIndex)->GetArchetype()->GetFName())
			{
				// ... set the component name to what was passed in
				UComponent* Comp = Actor->Components(CompIndex);
				//debugf(TEXT("Renaming component '%s' to '%s'"), *Comp->GetFullName(), *ComponentTemplatesAndNames(TemplateIndex+1));
				Comp->Rename(*ComponentTemplatesAndNames(TemplateIndex+1), NULL);
				break;
			}
		}
	}
	
	return Actor;
}

/** 
 * Handle deleting the given actor
 *
 * @param Actor		The actor to destroy
 */
void FStandardObjectPropagator::ProcessActorDelete(AActor* Actor)
{
	if (!Actor->bDeleteMe)
	{
		// a hacky way to let static actors get deleted in a world that has already BegunPlay
		UBOOL bOldBegunPlay = GWorld->GetWorldInfo()->bBegunPlay;
		if (Actor->IsStatic() || Actor->bNoDelete)
		{
			GWorld->GetWorldInfo()->bBegunPlay = FALSE;
		}

		// destroy it!
		GWorld->DestroyActor(Actor);

		GWorld->GetWorldInfo()->bBegunPlay = bOldBegunPlay;
	}
}

/**
 * Send an entire actor over the propagator, including any special properties
 * @param Actor The actor to propagate
 */
void FStandardObjectPropagator::PropagateActor(class AActor* Actor)
{
	// toss unneeded components
	UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);

	// report to the propagator that an actor was created
	OnActorCreate(Actor);

	// send over all editable properties
	for (INT ObjIndex = 0; ObjIndex < Actor->Components.Num() + 1; ObjIndex++)
	{
		UObject* PropObj;
		if (ObjIndex == Actor->Components.Num())
		{
			PropObj = Actor;
		}
		else
		{
			PropObj = Actor->Components(ObjIndex);
		}

		// loop over the properties of the object (actor or component)
		for (TFieldIterator<UProperty> It(PropObj->GetClass()); It; ++It)
		{
			UProperty* Prop = *It;
			// propagate editor props
			if ((Prop->PropertyFlags & CPF_Edit) || Prop->IsA(UComponentProperty::StaticClass()))
			{
				//FString Val;
				//Prop->ExportText(0, Val, (BYTE*)PropObj, NULL, PropObj, 0);
				//warnf(TEXT("Proping property %s -> %s"), *Prop->GetFullName(), *Val);
				OnPropertyChange(PropObj, Prop, Prop->Offset);
			}
		}
	}

	// handle some special properties

	AAmbientSoundSimple* AmbientSound = Cast<AAmbientSoundSimple>(Actor);
	// send ambientsound's wave property
	if (AmbientSound)
	{
		UProperty* WaveProp = FindField<UProperty>(AmbientSound->AmbientProperties->GetClass(), TEXT("Wave"));
		check(WaveProp);
		OnPropertyChange(AmbientSound->AmbientProperties, WaveProp, WaveProp->Offset);
	}
}
