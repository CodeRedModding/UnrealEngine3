#ifndef NX_PHYSICS_NXPULLEYJOINTDESC
#define NX_PHYSICS_NXPULLEYJOINTDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxJointDesc.h"

enum NxPulleyJointFlag
	{
	NX_PJF_IS_RIGID = 1 << 0	//!< true if the joint also maintains a minimum distance, not just a maximum.
	};

/**
Desc class for pulley joint.
*/
class NxPulleyJointDesc : public NxJointDesc
	{
	public:
	NxVec3 pulley[2];		//!< suspension points of two bodies in world space.
	NxReal distance;		//!< the rest length of the rope connecting the two objects.  The distance is computed as ||(pulley0 - anchor0)|| +  ||(pulley1 - anchor1)|| * ratio.
	NxReal stiffness;		//!< how stiff the constraint is, between 0 and 1 (stiffest)
	NxReal ratio;			//!< transmission ratio
	NxU32  flags;			//!< This is a combination of the bits defined by ::NxPulleyJointFlag. 

	/**
	constructor sets to default.
	*/
	NX_INLINE NxPulleyJointDesc();	
	/**
	(re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	};

NX_INLINE NxPulleyJointDesc::NxPulleyJointDesc() : NxJointDesc(NX_JOINT_PULLEY)	//constructor sets to default
	{
	setToDefault();
	}

NX_INLINE void NxPulleyJointDesc::setToDefault()
	{
	pulley[0].zero();
	pulley[1].zero();

	distance = 0.0f;
	stiffness = 1.0f;
	ratio = 1.0f;
	flags = 0;

	NxJointDesc::setToDefault();
	}

NX_INLINE bool NxPulleyJointDesc::isValid() const
	{
	if (distance < 0) return false;
	if (stiffness < 0 || stiffness > 1) return false;
	if (ratio < 0) return false;

	return NxJointDesc::isValid();
	}

#endif
