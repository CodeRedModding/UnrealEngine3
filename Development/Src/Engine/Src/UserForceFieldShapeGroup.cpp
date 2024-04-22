/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

#include "UserForceFieldShapeGroup.h"

#if WITH_NOVODEX
#include "UserForceField.h"

void UserForceFieldShapeGroup::GiveToForceField(UserForceField * forceField)
{
	if (NxObject->getForceField())
	{
		// this is the main include group for a force field, can not be shared.
		assert(0);
		return;
	}
	forceField->addShapeGroup(*NxObject);
	Connectors.Set(forceField, 0);
}


void UserForceFieldShapeGroup::PassAllForceFieldsTo(UserForceFieldShapeGroup& ffsGroup)
{
	for (TMap<UserForceField*, int>::TIterator i(Connectors); i; ++i)
	{
		UserForceField* ff = i.Key();
		ff->addShapeGroup(ffsGroup);
	}
}

void UserForceFieldShapeGroup::Destroy()
{
	if (NxObject->getForceField())
	{
		// this is the main include group for a force field, the NxObject destroy should be managed by NxForceField itself.
		delete this;
		return;
	}

	check(NxObject->getScene().isWritable());
	for (TMap<UserForceField*,int>::TIterator i(Connectors); i; ++i)
	{
		UserForceField* f = i.Key();
		f->removeShapeGroup(*NxObject);
	}
	NxObject->getScene().releaseForceFieldShapeGroup(*NxObject);
	delete this;
}
#endif
