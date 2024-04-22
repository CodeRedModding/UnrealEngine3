/*=============================================================================
UserCapsuleForceFieldShape.h
Engine object 1 : 1 related with a NxUserCapsuleForceFieldShape object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_CAPSULE_FORCE_FIELD_SHAPE_H
#define USER_CAPSULE_FORCE_FIELD_SHAPE_H

#if WITH_NOVODEX

class UserCapsuleForceFieldShape : public UserForceFieldShape
{
	//pubic interface for creating, destroying and getting UserCapsuleForceFieldShape object.
public:
	static UserForceFieldShape* Create(NxCapsuleForceFieldShape* shape)
	{
		UserCapsuleForceFieldShape* result = new UserCapsuleForceFieldShape;
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
	static UserCapsuleForceFieldShape* Convert(NxCapsuleForceFieldShape* shape)
	{
		return (UserCapsuleForceFieldShape*)shape->userData;
	}

	// common interface for UserCapsuleForceFieldShape and NxCapsuleForceFieldShape
public:

	// public interface for UserCapsuleForceFieldShape's extension to NxCapsuleForceFieldShape.
public:
	void setDimensions(NxReal radius, NxReal height)
	{
		NxObject->setDimensions(radius, height);
	}

	void setRadius(NxReal radius)
	{
		NxObject->setRadius(radius);
	}

	NxReal getRadius() const
	{
		return NxObject->getRadius();
	}

	void setHeight(NxReal height)
	{
		NxObject->setHeight(height);
	}

	NxReal getHeight() const
	{
		return NxObject->getHeight();
	}

	void saveToDesc(NxCapsuleForceFieldShapeDesc &desc) const
	{
		NxObject->saveToDesc(desc);
	}


protected:
	UserCapsuleForceFieldShape(){}
	virtual ~UserCapsuleForceFieldShape(){}

protected:
	NxCapsuleForceFieldShape* NxObject;

protected:
	virtual NxForceFieldShape* getForceFieldShape() const
	{
		return NxObject;
	}
};

#endif // WITH_NOVODEX

#endif //USER_CAPSULE_FORCE_FIELD_SHAPE_H
