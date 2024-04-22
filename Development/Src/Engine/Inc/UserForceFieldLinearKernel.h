/*=============================================================================
UserForceFieldLinearKernel.h
Engine object 1 : 1 related with a NxUserForceFieldLinearKernel object
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_FORCE_FIELD_LINEAR_KERNEL_H
#define USER_FORCE_FIELD_LINEAR_KERNEL_H

#if WITH_NOVODEX
#include "UserForceFieldKernel.h"

class UserForceFieldLinearKernel : public UserForceFieldKernel
{
	//pubic interface for creating, destroying and getting UserForceFieldLinearKernel object.
public:
	static UserForceFieldLinearKernel* Create(NxForceFieldLinearKernel* linearKernel, NxScene* scene)
	{
		UserForceFieldLinearKernel* result = new UserForceFieldLinearKernel;
		result->NxObject = linearKernel;
		linearKernel->userData = result;
		return result;
	}
	void Destroy()
	{
		check(NxObject->getScene().isWritable());
		NxObject->getScene().releaseForceFieldLinearKernel(*NxObject);
		delete this;
	}
	static UserForceFieldLinearKernel* Convert(NxForceFieldLinearKernel* linearKernel)
	{
		return (UserForceFieldLinearKernel*)linearKernel->userData;
	}

	// this operator is only for NxForceFieldDesc.kernel. I do not see other use.
	operator NxForceFieldLinearKernel*()
	{
		return NxObject;
	}


	// common interface for UserForceFieldLinearKernel and NxForceFieldLinearKernel
public:
	NxVec3 getConstant() const
	{
		return NxObject->getConstant();
	}

	void setConstant(const NxVec3 & constant)
	{
		NxObject->setConstant(constant);
	}

	NxMat33 getPositionMultiplier() const
	{
		return NxObject->getPositionMultiplier();
	}

	void setPositionMultiplier(const NxMat33 & multiplier)
	{
		return NxObject->setPositionMultiplier(multiplier);
	}

	NxMat33 getVelocityMultiplier() const
	{
		return NxObject->getVelocityMultiplier();
	}

	void setVelocityMultiplier(const NxMat33 &multiplier)
	{
		NxObject->setVelocityMultiplier(multiplier);
	}

	NxVec3 getPositionTarget() const
	{
		return NxObject->getPositionTarget();
	}

	void setPositionTarget(const NxVec3 & target)
	{
		NxObject->setPositionTarget(target);
	}

	NxVec3 getVelocityTarget() const
	{
		return NxObject->getVelocityTarget();
	}

	void setVelocityTarget(const NxVec3 & target)
	{
		NxObject->setVelocityTarget(target);
	}

	NxVec3 getFalloffLinear() const
	{
		return NxObject->getFalloffLinear();
	}

	void setFalloffLinear(const NxVec3 & fallOffLinear)
	{
		NxObject->setFalloffLinear(fallOffLinear);
	}

	NxVec3 getFalloffQuadratic() const
	{
		return NxObject->getFalloffQuadratic();
	}

	void setFalloffQuadratic(const NxVec3 &fallOffQuadratic)
	{
		return NxObject->setFalloffQuadratic(fallOffQuadratic);
	}

	NxVec3 getNoise() const
	{
		return NxObject->getNoise();
	}

	void setNoise(const NxVec3 & noise)
	{
		NxObject->setNoise(noise);
	}

	NxReal getTorusRadius() const
	{
		return NxObject->getTorusRadius();
	}

	void setTorusRadius(NxReal torusRadius)
	{
		NxObject->setTorusRadius(torusRadius);
	}

	void saveToDesc(NxForceFieldLinearKernelDesc &desc)
	{
		NxObject->saveToDesc(desc);
	}

	void setName(const char *name)
	{
		NxObject->setName(name);
	}

	const char * getName() const
	{
		return NxObject->getName();
	}

//helper function for exposing NxForceFieldKernel interface
protected:
	virtual NxForceFieldKernel* GetForceFieldKernel() const
	{
		return NxObject;
	}


	// public interface for UserForceFieldLinearKernel's extension to NxForceFieldLinearKernel.
public:
	NxForceFieldLinearKernel* NxObject;

protected:
	UserForceFieldLinearKernel(){}
	virtual ~UserForceFieldLinearKernel(){}

};

#endif // WITH_NOVODEX

#endif //USER_FORCE_FIELD_LINEAR_KERNEL_H
