/*=============================================================================
UserSphereForceFieldShape.h
Engine object 1 : 1 related with a NxUserSphereForceFieldShape object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_SPHERE_FORCE_FIELD_SHAPE_H
#define USER_SPHERE_FORCE_FIELD_SHAPE_H

#if WITH_NOVODEX
#include "UserForceFieldShape.h"

class UserSphereForceFieldShape : public UserForceFieldShape
{
	//pubic interface for creating, destroying and getting UserSphereForceFieldShape object.
public:
	static UserForceFieldShape* Create(NxSphereForceFieldShape* shape)
	{
		UserSphereForceFieldShape* result = new UserSphereForceFieldShape;
		result->NxObject = shape;
		shape->userData = result;
		return result;
	}
	void Destroy()
	{
		assert(0);
//SDK todo.		check(NxObject->getScene().isWritable());
		NxForceFieldShapeGroup& ffsGroup =NxObject->getShapeGroup();
		ffsGroup.releaseShape(*NxObject);
		delete this;
	}
	static UserSphereForceFieldShape* Convert(NxSphereForceFieldShape* shape)
	{
		return (UserSphereForceFieldShape*)shape->userData;
	}

	// common interface for UserSphereForceFieldShape and NxSphereForceFieldShape
public:
	void setRadius(NxReal radius)
	{
		NxObject->setRadius(radius);
	}

	NxReal getRadius() const
	{
		return NxObject->getRadius();
	}

	void  saveToDesc(NxSphereForceFieldShapeDesc &desc) const
	{
		NxObject->saveToDesc(desc);
	}

	// public interface for UserSphereForceFieldShape's extension to NxSphereForceFieldShape.
public:

protected:
	UserSphereForceFieldShape(){}
	virtual ~UserSphereForceFieldShape(){}

protected:
	NxSphereForceFieldShape* NxObject;

protected:
	virtual NxForceFieldShape* getForceFieldShape() const
	{
		return NxObject;
	}
};

#endif // WITH_NOVODEX

#endif //USER_SPHERE_FORCE_FIELD_SHAPE_H
