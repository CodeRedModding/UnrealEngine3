/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FEdObjectPropagator : public FStandardObjectPropagator
{
	void OnPropertyChange(UObject* Object, UProperty* Property, INT PropertyOffset);
	void OnActorMove(AActor* Actor);
	void OnActorCreate(AActor* Actor);
	void OnActorDelete(AActor* Actor);
	void OnObjectRename(UObject* Object, const TCHAR* NewName);
};
