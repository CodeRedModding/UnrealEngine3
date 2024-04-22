/*=============================================================================
UserForceField.h
Engine object 1 : 1 related with a NxUserForceField object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_FORCE_FIELD_H
#define USER_FORCE_FIELD_H

#if WITH_NOVODEX

class UserForceField
{
//pubic interface for creating, destroying and getting UserForceField object.
public:
	static UserForceField* Create(NxForceField* forceField, UBOOL bRotatePose);
	void Destroy();
	static UserForceField* Convert(NxForceField* forceField)
	{
		return (UserForceField*)forceField->userData;
	}

// common interface for UserForceField and NxForceField
public:
	void saveToDesc(NxForceFieldDesc& desc)
	{
		NxObject->saveToDesc(desc);
	}

	NxMat34 getPose() const
	{
		NxMat34 ZupPose = NxObject->getPose();
		ZupPose.M.multiply(ZupPose.M, RotationInv);
		return ZupPose;
	}

	void setPose(const NxMat34& pose)
	{
		NxMat34 YupPose = pose;
		YupPose.M.multiply(pose.M, Rotation);
		NxObject->setPose(YupPose);
	}

	NxActor* getActor() const
	{
		return NxObject->getActor();
	}

	void setActor(NxActor *actor)
	{
		NxObject->setActor(actor);
	}

	void setForceFieldKernel(NxForceFieldKernel *kernel)
	{
		NxObject->setForceFieldKernel(kernel);
	}

	NxForceFieldKernel * getForceFieldKernel()
	{
		return NxObject->getForceFieldKernel();
	}

// override it
//	NxForceFieldShapeGroup & getIncludeShapeGroup()
//	{
//		return NxObject->getIncludeShapeGroup();
//	}

	void addShapeGroup(NxForceFieldShapeGroup & group)
	{
		NxObject->addShapeGroup(group);
	}

	void removeShapeGroup(NxForceFieldShapeGroup & group)
	{
		NxObject->removeShapeGroup(group);
	}

	NxU32 getNbShapeGroups() const
	{
		return NxObject->getNbShapeGroups();
	}

	void resetShapeGroupsIterator()
	{
		NxObject->resetShapeGroupsIterator();
	}

	NxForceFieldShapeGroup * getNextShapeGroup()
	{
		return NxObject->getNextShapeGroup();
	}

	NxCollisionGroup getGroup() const
	{
		return NxObject->getGroup();
	}

	void setGroup(NxCollisionGroup collisionGroup)
	{
		NxObject->setGroup(collisionGroup);
	}

	NxGroupsMask getGroupsMask() const
	{
		return NxObject->getGroupsMask();
	}

	void setGroupsMask(NxGroupsMask mask)
	{
		NxObject->setGroupsMask(mask);
	}

	NxForceFieldCoordinates getCoordinates() const
	{
		return NxObject->getCoordinates();
	}

	void setCoordinates(NxForceFieldCoordinates coordinates)
	{
		return NxObject->setCoordinates(coordinates);
	}

	void setName(const char *name)
	{
		NxObject->setName(name);
	}

	const char * getName() const
	{
		return NxObject->getName();
	}

	NxForceFieldType getFluidType() const
	{
		return NxObject->getFluidType();
	}

	void setFluidType(NxForceFieldType type)
	{
		NxObject->setFluidType(type);
	}

	NxForceFieldType getClothType() const
	{
		return NxObject->getClothType();
	}

	void setClothType(NxForceFieldType type)
	{
		NxObject->setClothType(type);
	}

	NxForceFieldType getSoftBodyType() const
	{
		return NxObject->getSoftBodyType();
	}

	void setSoftBodyType(NxForceFieldType type)
	{
		NxObject->setSoftBodyType(type);
	}

	NxForceFieldType getRigidBodyType() const
	{
		return NxObject->getRigidBodyType();
	}

	void setRigidBodyType(NxForceFieldType type)
	{
		NxObject->setRigidBodyType(type);
	}

	NxU32 getFlags() const
	{
		return NxObject->getFlags();
	}

	void setFlags(NxU32 flags)
	{
		NxObject->setFlags(flags);
	}

	void samplePoints(NxU32 numPoints, const NxVec3 *points, const NxVec3 *velocities, NxVec3 *outForces, NxVec3 *outTorques) const
	{
		NxObject->samplePoints(numPoints, points, velocities, outForces, outTorques);
	}

	NxScene & getScene() const
	{
		return NxObject->getScene();
	}

	NxForceFieldVariety	getForceFieldVariety() const
	{
		return NxObject->getForceFieldVariety();
	}

	void setForceFieldVariety(NxForceFieldVariety variety)
	{
		NxObject->setForceFieldVariety(variety);
	}


// public interface for UserForceField's extension to NxForceField.
public:
	void addShapeGroup(class UserForceFieldShapeGroup& ffsGroup);

	UserForceFieldShapeGroup & getIncludeShapeGroup();

protected:
	UserForceField(){}
	virtual ~UserForceField(){}

protected:
	NxForceField* NxObject;
	UserForceFieldShapeGroup* MainGroup;

	NxMat33 Rotation;
	NxMat33 RotationInv;
};

#endif // WITH_NOVODEX

#endif //USER_FORCE_FIELD_H
