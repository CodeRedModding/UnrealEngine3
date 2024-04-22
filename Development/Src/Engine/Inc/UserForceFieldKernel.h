/*=============================================================================
UserForceFieldKernel.h
Interface for NxUserForceFieldLinearKernel
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef USER_FORCE_FIELD_KERNEL_H
#define USER_FORCE_FIELD_KERNEL_H

#if WITH_NOVODEX

class UserForceFieldKernel
{
protected:
	virtual NxForceFieldKernel * GetForceFieldKernel() const = 0;

public:
	void parse() const
	{
		GetForceFieldKernel()->parse();
	}
	bool evaluate(NxVec3 &force, NxVec3 &torque, const NxVec3 &position, const NxVec3 &velocity)
	{
		return GetForceFieldKernel()->evaluate(force, torque, position, velocity);
	}
	NxU32 getType() const
	{
		return GetForceFieldKernel()->getType();
	}
	NxForceFieldKernel * clone() const
	{
		return GetForceFieldKernel()->clone();
	}
	void update(NxForceFieldKernel &in) const
	{
		GetForceFieldKernel()->update(in);
	}

protected:
	UserForceFieldKernel(){}
	virtual ~UserForceFieldKernel(){}
};

#endif // WITH_NOVODEX

#endif //USER_FORCE_FIELD_KERNEL_H
