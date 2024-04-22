/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "EnginePrivate.h"

#if WITH_NOVODEX
#include "UnNovodexSupport.h"
#endif

#include "UserForceField.h"
#include "UserForceFieldShapeGroup.h"

#if WITH_NOVODEX

UserForceField* UserForceField::Create(NxForceField* forceField, UBOOL bRotatePose)
{
	UserForceField* result = new UserForceField;
	result->NxObject = forceField;
	result->MainGroup = UserForceFieldShapeGroup::Create(&forceField->getIncludeShapeGroup(), &forceField->getScene());
	forceField->userData = result;
	if (bRotatePose)
	{
		result->Rotation.rotX(NxPi/2);
		result->RotationInv.rotX(-NxPi/2);
	}
	else
	{
		result->Rotation.id();
		result->RotationInv.id();
	}
	return result;
}

void UserForceField::Destroy()
{
	check(NxObject->getScene().isWritable());
	MainGroup->Destroy();
	NxObject->getScene().releaseForceField(*NxObject);
	delete this;
}

void UserForceField::addShapeGroup(UserForceFieldShapeGroup& ffsGroup)
{
	ffsGroup.GiveToForceField(this);
}

UserForceFieldShapeGroup & UserForceField::getIncludeShapeGroup()
{
	return *UserForceFieldShapeGroup::Convert(&NxObject->getIncludeShapeGroup());
}

#endif
