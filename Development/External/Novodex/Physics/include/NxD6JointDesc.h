#ifndef NX_PHYSICS_NXD6JOINTDESC
#define NX_PHYSICS_NXD6JOINTDESC

#include "NxJointDesc.h"
#include "NxJointLimitDesc.h"
#include "NxJointLimitPairDesc.h"

/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

enum NxD6JointDriveType
	{
	NX_D6JOINT_DRIVE_POSITION	= 1<<0,   /**< not supported. */
	NX_D6JOINT_DRIVE_VELOCITY	= 1<<1,   /**< not supported. */
	NX_D6JOINT_DRIVE_SLERP		= 1<<2    /**< not supported. */
	};



///////////////////////////////////////////////////////////


/**
 * A D6 joint is a configurable constraint supporting free movement along and
 * around any selected combination of the coordinate axes.
 */

enum NxD6JointMotion
	{
	NX_D6JOINT_MOTION_LOCKED,              /**< Joint is locked around (angular) or along (linear) the given axis. */
	NX_D6JOINT_MOTION_LIMITED,             /**< Joint is limited around (angular) or along (linear) the given axis. */
	NX_D6JOINT_MOTION_FREE                 /**< Joint is free around (angular) or along (linear) given axis. */
	};
 

/** not supported */
class NxJointDriveDesc
{
public:
	NxBitField32			driveType;      /**< not supported. */
	NxReal					spring;         /**< not supported. */
	NxReal					damping;        /**< not supported. */
	NxReal					forceLimit;     /**< not supported. */

	NxJointDriveDesc()                      /**< not supported. */
	{
		spring = 0;
		damping = 0;
		forceLimit = FLT_MAX;
		driveType = 0;
	}
};

/**
 * A D6 joint is a configurable constraint supporting free movement along and
 * around any selected combination of the coordinate axes. An symmetrical twist
 * limit is supported around the x-axis, and an elliptical cone limit around the
 * y- and z- axes. The linear limit is distance based, the distance being measured
 * across all limited linear degrees of freedom.
 */


class NxD6JointDesc : public NxJointDesc
{
public:

	
	NxD6JointMotion			xMotion;       /**< Motion type allowed along x-axis. */
	NxD6JointMotion         yMotion;       /**< Motion type allowed along y-axis. */
	NxD6JointMotion         zMotion;       /**< Motion type allowed along z-axis. */
	NxD6JointMotion         twistMotion;   /**< Motion type allowed around x-axis. */
	NxD6JointMotion			swing1Motion;  /**< Motion type allowed around y-axis. */
	NxD6JointMotion         swing2Motion;  /**< Motion type allowed around z-axis. */

	NxJointLimitDesc		linearLimit;   /**< Limit distance descriptor. Currently no softness or restitution. */
	NxJointLimitPairDesc	twistLimit;    /**< Twist angular limit. Currently no softness or restitution. Currently symmetrical, low limit not supported. */
	NxJointLimitDesc		swing1Limit;   /**< Swing1 angular limit. Currently no softness or restitution. */
	NxJointLimitDesc		swing2Limit;   /**< Swing2 angular limit. Currently no softness or restitution. */

	/* drive */
	NxJointDriveDesc		xDrive, yDrive, zDrive; /**< Not supported. */

	NxJointDriveDesc		swingDrive, twistDrive; /**< Not supported. */
	NxJointDriveDesc		sphericalDrive;			/**< Not supported. */

	bool					useSpherical;           /**< Not supported. */

	NxVec3					drivePosition;          /**< Not supported. */
	NxQuat					driveOrientation;       /**< Not supported. */

	NxVec3					driveLinearVelocity;    /**< Not supported. */
	NxVec3					driveAngularVelocity;   /**< Not supported. */

	NxReal					projectionDistance;	    /**< Distance beyond which projection takes place. */
	NxReal					projectionAngle;	    /**< Not supported. */
	NxJointProjectionMode	projectionMode;	        /**< Not supported. */


	/**
	constructor sets to default.
	*/

	NX_INLINE			NxD6JointDesc();

	/**
	(re)sets the structure to the default.	
	*/
	NX_INLINE	void	setToDefault();

	/**

	returns true if the current settings are valid

	*/

	NX_INLINE	bool	isValid() const;
};

NxD6JointDesc::NxD6JointDesc() : NxJointDesc(NX_JOINT_D6)
{
	setToDefault();
}

void NxD6JointDesc::setToDefault() 
{
	xMotion = NX_D6JOINT_MOTION_FREE;
	yMotion = NX_D6JOINT_MOTION_FREE;
	zMotion = NX_D6JOINT_MOTION_FREE;
	twistMotion = NX_D6JOINT_MOTION_FREE;
	swing1Motion = NX_D6JOINT_MOTION_FREE;
	swing2Motion = NX_D6JOINT_MOTION_FREE;

	drivePosition.set(0,0,0);
	driveOrientation.id();

	driveLinearVelocity.set(0,0,0);
	driveAngularVelocity.set(0,0,0);
}

bool NxD6JointDesc::isValid() const
{
	return true;
}

#endif
