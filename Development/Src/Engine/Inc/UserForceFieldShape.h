/*=============================================================================
UserForceFieldShape.h
Interface for concrete UserUserForceFieldShape object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_FORCE_FIELD_SHAPE_H
#define USER_FORCE_FIELD_SHAPE_H

#if WITH_NOVODEX

class UserForceFieldShape
{
public:
	NxMat34 getPose() const
	{
		return GetForceFieldShape()->getPose();
	}

	void setPose(const NxMat34 & pose)
	{
		GetForceFieldShape()->setPose(pose);
	}

	NxForceField * getForceField() const
	{
		return GetForceFieldShape()->getForceField();
	}

	NxForceFieldShapeGroup & getShapeGroup() const
	{
		return GetForceFieldShape()->getShapeGroup();
	}

	void setName(const char *name)
	{
		GetForceFieldShape()->setName(name);
	}

	const char * getName() const
	{
		return GetForceFieldShape()->getName();
	}

	NxShapeType getType() const
	{
		return GetForceFieldShape()->getType();
	}

	void * is(NxShapeType type)
	{
		return GetForceFieldShape()->is(type);
	}

	const void * is(NxShapeType type) const
	{
		return GetForceFieldShape()->is(type);
	}

	NxSphereForceFieldShape * isSphere()
	{
		return GetForceFieldShape()->isSphere();
	}

	const NxSphereForceFieldShape * isSphere() const
	{
		return GetForceFieldShape()->isSphere();
	}

	NxBoxForceFieldShape * isBox()
	{
		return GetForceFieldShape()->isBox();
	}

	const NxBoxForceFieldShape * isBox() const
	{
		return GetForceFieldShape()->isBox();
	}

	NxCapsuleForceFieldShape * isCapsule()
	{
		return GetForceFieldShape()->isCapsule();
	}

	const NxCapsuleForceFieldShape * isCapsule() const
	{
		return GetForceFieldShape()->isCapsule();
	}

	NxConvexForceFieldShape * isConvex()
	{
		return GetForceFieldShape()->isConvex();
	}

	const NxConvexForceFieldShape * isConvex() const
	{
		return GetForceFieldShape()->isConvex();
	}


	// public interface for UserForceFieldShape's extension to NxForceFieldShape.
public:

protected:
	UserForceFieldShape(){}
	virtual ~UserForceFieldShape(){}

protected:
	virtual NxForceFieldShape* GetForceFieldShape() const = 0;
};

#endif // WITH_NOVODEX

#endif //USER_FORCE_FIELD_SHAPE_H
