/*=============================================================================
UserConvexForceFieldShape.h
Engine object 1 : 1 related with a NxUserConvexForceFieldShape object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_CONVEX_FORCE_FIELD_SHAPE_H
#define USER_CONVEX_FORCE_FIELD_SHAPE_H

#if WITH_NOVODEX

class UserConvexForceFieldShape : public UserForceFieldShape
{
	//pubic interface for creating, destroying and getting UserConvexForceFieldShape object.
public:
	static UserForceFieldShape* Create(NxConvexForceFieldShape* shape)
	{
		UserConvexForceFieldShape* result = new UserConvexForceFieldShape;
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
	static UserConvexForceFieldShape* Convert(NxConvexForceFieldShape* shape)
	{
		return (UserConvexForceFieldShape*)shape->userData;
	}

	// common interface for UserConvexForceFieldShape and NxConvexForceFieldShape
public:
	void  saveToDesc(NxConvexForceFieldShapeDesc &desc) const
	{
		NxObject->saveToDesc(desc);
	}

	// public interface for UserConvexForceFieldShape's extension to NxConvexForceFieldShape.
public:

protected:
	UserConvexForceFieldShape(){}
	virtual ~UserConvexForceFieldShape(){}

protected:
	NxConvexForceFieldShape* NxObject;

protected:
	virtual NxForceFieldShape* getForceFieldShape() const
	{
		return NxObject;
	}
};

#endif // WITH_NOVODEX

#endif //USER_CONVEX_FORCE_FIELD_SHAPE_H
