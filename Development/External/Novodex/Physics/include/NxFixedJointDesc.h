#ifndef NX_PHYSICS_NXFIXEDJOINTDESC
#define NX_PHYSICS_NXFIXEDJOINTDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"
#include "NxJointDesc.h"
/**
Desc class for fixed joint.
*/
class NxFixedJointDesc : public NxJointDesc
	{
	public:
	/**
	constructor sets to default.
	*/
	NX_INLINE NxFixedJointDesc();	
	/**
	(re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	};

NX_INLINE NxFixedJointDesc::NxFixedJointDesc() : NxJointDesc(NX_JOINT_FIXED)	//constructor sets to default
	{
	setToDefault();
	}

NX_INLINE void NxFixedJointDesc::setToDefault()
	{
	NxJointDesc::setToDefault();
	}

NX_INLINE bool NxFixedJointDesc::isValid() const
	{
	return NxJointDesc::isValid();
	}

#endif
