/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"

UObject* FindOtherObject(UObject* InObject)
{
	// get the source object's full name
	FString FullName(InObject->GetPathName());

	// don't do anything if the object is the destination object (to avoid accidental recursion)
	if (FullName.StartsWith(FString(PLAYWORLD_PACKAGE_PREFIX)))
	{
        return NULL;
	}
	else
	{
		// munge the name to find the destination object in the PlayWorld
		return UObject::StaticFindObject(UObject::StaticClass(), ANY_PACKAGE, *(FString(PLAYWORLD_PACKAGE_PREFIX) + FullName) );
	}
}

void FEdObjectPropagator::OnPropertyChange(UObject* Object, UProperty* Property, INT PropertyOffset)
{
	// do nothing if paused
	if (Paused || !GEditor->PlayWorld)
	{
		return;
	}

	check(Object);
	check(Property);

	// if we are modifying an actor's location, then we should use the ActorMove function instead
	if (appStricmp(*Property->GetOuter()->GetName(), TEXT("Location")) == 0)
	{
		OnActorMove(Cast<AActor>(Object));
		return;
	}

	// look for this same object in the PlayFromHere package:
	UObject* OtherObject = FindOtherObject(Object);

	// if we didn't find it, return early
	if (!OtherObject)
	{
		return;
	}

	debugf(TEXT("Propagating %s.%s [%d bytes offset]..."), *Object->GetFullName(), *Property->GetName(), PropertyOffset);

	// copy the property to the other object, using the property-specific copying copde
	Property->CopyCompleteValue((BYTE*)OtherObject + PropertyOffset, (BYTE*)Object + PropertyOffset);

	// handle any component updating that is necessary
	PostPropertyChange(OtherObject, Property);
}

void FEdObjectPropagator::OnActorMove(AActor* Actor)
{
	// do nothing if paused
	if (Paused || GWorld == GEditor->PlayWorld || !GEditor->PlayWorld)
	{
		return;
	}

	// look for this same actor in the PlayFromHere package:
	AActor* OtherActor = (AActor*)FindOtherObject(Actor);

	// if we didn't find it, return
	if (!OtherActor)
	{
		return;
	}

	// save off the old gworld, because physics needs the GWorld with the physics running in it
	UWorld* OldGWorld = SetPlayInEditorWorld( GEditor->PlayWorld );

	debugf(TEXT("Phys updating %s"), *OtherActor->GetFullName());

	Pause();

	ARB_ConstraintActor* OtherConstraint = Cast<ARB_ConstraintActor>(OtherActor);
	if (OtherConstraint)
	{
		// if we are a constraint, we need to copy constraint values from the original constraint, there's not a nice way to do this
		ARB_ConstraintActor* Constraint = Cast<ARB_ConstraintActor>(Actor);

		OtherConstraint->ConstraintSetup->Pos1 = Constraint->ConstraintSetup->Pos1;
		OtherConstraint->ConstraintSetup->PriAxis1 = Constraint->ConstraintSetup->PriAxis1;
		OtherConstraint->ConstraintSetup->SecAxis1 = Constraint->ConstraintSetup->SecAxis1;

		OtherConstraint->ConstraintSetup->Pos2 = Constraint->ConstraintSetup->Pos2;
		OtherConstraint->ConstraintSetup->PriAxis2 = Constraint->ConstraintSetup->PriAxis2;
		OtherConstraint->ConstraintSetup->SecAxis2 = Constraint->ConstraintSetup->SecAxis2;

		// restart the physics on the constraint
		OtherConstraint->TermRBPhys(NULL);
		OtherConstraint->InitRBPhys();

		// make lights update
		OtherConstraint->ForceUpdateComponents(FALSE,FALSE);
		OtherConstraint->InvalidateLightingCache();
	}
	else
	{
		ProcessActorMove(OtherActor, Actor->Location, Actor->Rotation);
	}

	Unpause();

	// restore the original (editor) GWorld
	RestoreEditorWorld( OldGWorld );
}

void FEdObjectPropagator::OnActorCreate(AActor* Actor)
{
	// do nothing if paused
	if (Paused || GWorld == GEditor->PlayWorld || !GEditor->PlayWorld)
	{
		return;
	}

	// save off the old gworld, because physics needs the GWorld with the physics running in it
	UWorld* OldGWorld = SetPlayInEditorWorld( GEditor->PlayWorld );

	// build an array that contains the components along with their template names so we can 
	// match up components in the other actor (we match up by template name as that is a good 
	// unique identifier, and not all components in the new actor as are in the old)
	TArray<FString> ComponentTemplatesAndNames;
	for (INT CompIndex = 0; CompIndex < Actor->Components.Num(); CompIndex++)
	{
		new(ComponentTemplatesAndNames) FString(Actor->Components(CompIndex)->GetArchetype()->GetName());
		new(ComponentTemplatesAndNames) FString(Actor->Components(CompIndex)->GetName());
	}

	Pause();
	// create a new actor in the play world
	ProcessActorCreate(Actor->GetClass(), Actor->GetFName(), Actor->Location, Actor->Rotation, ComponentTemplatesAndNames);
	Unpause();

	// restore the original (editor) GWorld
	RestoreEditorWorld( OldGWorld );
}

void FEdObjectPropagator::OnActorDelete(AActor* Actor)
{
	// do nothing if paused
	if (Paused || GWorld == GEditor->PlayWorld || !GEditor->PlayWorld)
	{
		return;
	}

	// look for this same actor in the PlayFromHere package:
	AActor* OtherActor = (AActor*)FindOtherObject(Actor);

	// if we didn't find it, return
	if (!OtherActor)
	{
		return;
	}

	// save off the old gworld, because physics needs the GWorld with the physics running in it
	UWorld* OldGWorld = SetPlayInEditorWorld( GEditor->PlayWorld );

	// create a new actor in the play world
	ProcessActorDelete(OtherActor);

	// restore the original (editor) GWorld
	RestoreEditorWorld( OldGWorld );
}

void FEdObjectPropagator::OnObjectRename(UObject* Object, const TCHAR* NewName)
{
	// do nothing if paused
	if (Paused || GWorld == GEditor->PlayWorld || !GEditor->PlayWorld)
	{
		return;
	}

	// look for this same object in the PlayFromHere package:
	UObject* OtherObject = FindOtherObject(Object);

	// if we didn't find it, return early
	if (!OtherObject)
	{
		return;
	}

	Pause();
	// handle any component updating that is necessary
	ProcessObjectRename(OtherObject, NewName);
	Unpause();
	
}
