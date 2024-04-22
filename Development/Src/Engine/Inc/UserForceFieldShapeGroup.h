/*=============================================================================
UserForceFieldShapeGroup.h
Engine object 1 : 1 related with a NxUserForceFieldShapeGroup object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_FORCE_FIELD_SHAPE_GROUP_H
#define USER_FORCE_FIELD_SHAPE_GROUP_H

#if WITH_NOVODEX

class UserForceFieldShapeGroup
{
	//pubic interface for creating, destroying and getting UserForceFieldShapeGroup object.
public:
	static UserForceFieldShapeGroup* Create(NxForceFieldShapeGroup* ffsGroup, NxScene* scene)
	{
		UserForceFieldShapeGroup* result = new UserForceFieldShapeGroup;
		result->NxObject = ffsGroup;
		ffsGroup->userData = result;
		return result;
	}
	void Destroy();
	static UserForceFieldShapeGroup* Convert(NxForceFieldShapeGroup* ffsGroup)
	{
		return (UserForceFieldShapeGroup*)ffsGroup->userData;
	}

	// common interface for UserForceFieldShapeGroup and NxForceFieldShapeGroup
public:
	NxForceFieldShape * createShape(const NxForceFieldShapeDesc &desc)
	{
		return NxObject->createShape(desc);
	}

	void releaseShape(const NxForceFieldShape &shape)
	{
		NxObject->releaseShape(shape);
	}

	NxU32 getNbShapes() const
	{
		return NxObject->getNbShapes();
	}

	void resetShapesIterator()
	{
		NxObject->resetShapesIterator();
	}

	NxForceFieldShape * getNextShape()
	{
		return NxObject->getNextShape();
	}

	NxForceField * getForceField() const
	{
		return NxObject->getForceField();
	}

	NxU32 getFlags() const
	{
		return NxObject->getFlags();
	}


	// public interface for UserForceFieldShapeGroup's extension to NxForceFieldShapeGroup.
public:
	void GiveToForceField(UserForceField * forceField);
	void PassAllForceFieldsTo(UserForceFieldShapeGroup& ffsGroup);

protected:
	UserForceFieldShapeGroup(){}
	virtual ~UserForceFieldShapeGroup(){}

protected:
	NxForceFieldShapeGroup* NxObject;
	TMap<UserForceField*, int> Connectors;
};

#endif // WITH_NOVODEX

#endif //USER_FORCE_FIELD_SHAPE_GROUP_H
