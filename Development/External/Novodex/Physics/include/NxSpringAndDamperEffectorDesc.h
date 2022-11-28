#ifndef NX_PHYSICS_NXSPRINGANDDAMPEREFFECTORDESC
#define NX_PHYSICS_NXSPRINGANDDAMPEREFFECTORDESC
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"

/**
Desc class for NxSpringAndDamperEffector.  TODO: This is not yet/anymore used for anything useful!!
*/
class NxSpringAndDamperEffectorDesc
	{
	public:

	/**
	constructor sets to default.
	*/
	NX_INLINE NxSpringAndDamperEffectorDesc();	
	/**
	(re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	NxActor* body1;
	NxActor* body2;

	NxVec3 pos1;
	NxVec3 pos2;

	//linear spring parameters:
	NxReal springDistCompressSaturate;
	NxReal springDistRelaxed;
	NxReal springDistStretchSaturate;
	NxReal springMaxCompressForce;
	NxReal springMaxStretchForce;

	//linear damper parameters:
	NxReal damperVelCompressSaturate;
	NxReal damperVelStretchSaturate;
	NxReal damperMaxCompressForce;
	NxReal damperMaxStretchForce;
	};

NX_INLINE NxSpringAndDamperEffectorDesc::NxSpringAndDamperEffectorDesc()	//constructor sets to default
	{
	setToDefault();
	}

NX_INLINE void NxSpringAndDamperEffectorDesc::setToDefault()
	{
	body1 = NULL;
    body2 = NULL;

	pos1.zero();
	pos2.zero();

	springDistCompressSaturate = 0;
	springDistRelaxed = 0;
	springDistStretchSaturate = 0;
	springMaxCompressForce = 0;
	springMaxStretchForce = 0;

	damperVelCompressSaturate = 0;
    damperVelStretchSaturate = 0;
	damperMaxCompressForce = 0;
	damperMaxStretchForce = 0;
	}

NX_INLINE bool NxSpringAndDamperEffectorDesc::isValid() const
	{
	return true;
	}

#endif
