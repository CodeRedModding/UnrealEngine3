#ifndef NX_PHYSICS_NXDISTANCEJOINTDESC
#define NX_PHYSICS_NXDISTANCEJOINTDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxJointDesc.h"

enum NxDistanceJointFlag
	{
	NX_DJF_MAX_DISTANCE_ENABLED = 1 << 0,	//!< true if the joint enforces the maximum separate distance.
	NX_DJF_MIN_DISTANCE_ENABLED = 1 << 1,	//!< true if the joint enforces the minimum separate distance.
	NX_DJF_SPRING_ENABLED		= 1 << 2,	//!< true if the spring is enabled
	};

/**
Desc class for distance joint.
*/
class NxDistanceJointDesc : public NxJointDesc
	{
	public:
	NxReal maxDistance;		//!< The maximum rest length of the rope or rod between the two anchor points 
	NxReal minDistance;     //!< The minimum rest length of the rope or rod between the two anchor points
	//NxReal stiffness;		//!< How stiff the constraint is, between 0 and 1 (stiffest)

	NxSpringDesc spring;	//!< makes the joint springy.  The spring.targetValue field is not used.
	NxU32  flags;			//!< This is a combination of the bits defined by ::NxDistanceJointFlag . 
	/**
	constructor sets to default.
	*/
	NX_INLINE NxDistanceJointDesc();	
	/**
	(re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault(bool fromCtor);
	/**
	returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	};

NX_INLINE NxDistanceJointDesc::NxDistanceJointDesc() : NxJointDesc(NX_JOINT_DISTANCE)	//constructor sets to default
	{
	setToDefault(true);
	}

NX_INLINE void NxDistanceJointDesc::setToDefault(bool fromCtor)
	{
	NxJointDesc::setToDefault();
	maxDistance = 0.0f;
	minDistance = 0.0f;
	//stiffness = 1.0f;
	flags = 0;

	if (!fromCtor)
		{
		//this is redundant if we're being called from the ctor:
		spring.setToDefault();
		}
	}

NX_INLINE bool NxDistanceJointDesc::isValid() const
	{
	if (maxDistance < 0) return false;
	if (minDistance < 0) return false;

	// if both distance constrains are on, the min better be less than or equal to the max.
	if ((minDistance > maxDistance) && (flags == (NX_DJF_MIN_DISTANCE_ENABLED | NX_DJF_MAX_DISTANCE_ENABLED))) return false;
//	if (stiffness < 0 || stiffness > 1) return false;
	if (!spring.isValid()) return false;

	return NxJointDesc::isValid();
	}

#endif
