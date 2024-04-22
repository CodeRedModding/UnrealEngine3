/*=============================================================================
UserBoxForceFieldShape.h
Engine object 1 : 1 related with a NxUserBoxForceFieldShape object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_BOX_FORCE_FIELD_SHAPE_H
#define USER_BOX_FORCE_FIELD_SHAPE_H

#if WITH_NOVODEX

class UserBoxForceFieldShape : public UserForceFieldShape
{
	//pubic interface for creating, destroying and getting UserBoxForceFieldShape object.
public:
	static UserForceFieldShape* Create(NxBoxForceFieldShape* shape)
	{
		UserBoxForceFieldShape* result = new UserBoxForceFieldShape;
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
	static UserBoxForceFieldShape* Convert(NxBoxForceFieldShape* shape)
	{
		return (UserBoxForceFieldShape*)shape->userData;
	}

	// common interface for UserBoxForceFieldShape and NxBoxForceFieldShape
public:
	void  setDimensions(const NxVec3 &vec)
	{
		return NxObject->setDimensions(vec);
	}

	NxVec3  getDimensions() const
	{
		return NxObject->getDimensions();
	}

	void  saveToDesc(NxBoxForceFieldShapeDesc &desc) const
	{
		NxObject->saveToDesc(desc);
	}

	// public interface for UserBoxForceFieldShape's extension to NxBoxForceFieldShape.
public:

protected:
	UserBoxForceFieldShape(){}
	virtual ~UserBoxForceFieldShape(){}

protected:
	NxBoxForceFieldShape* NxObject;

protected:
	virtual NxForceFieldShape* getForceFieldShape() const
	{
		return NxObject;
	}
};

#endif // WITH_NOVODEX

#endif //USER_BOX_FORCE_FIELD_SHAPE_H
