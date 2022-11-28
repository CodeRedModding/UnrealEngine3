#ifndef NX_PHYSICS_NX_ACTOR
#define NX_PHYSICS_NX_ACTOR
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
#include "Nxp.h"
#include "NxArray.h"
#include "NxBounds3.h"
#include "NxActorDesc.h"
#include "NxPhysicsSDK.h"

#define NX_NUM_SLEEP_FRAMES (20.0f*0.02f)		//This is a 'legacy' setup that works out to 20 when using the standard time step.
//#define NX_NUM_SLEEP_FRAMES (999999999.0f)

class NxBodyDesc;
class NxShapeDesc;
class NxJoint;
class NxShape;

/**
Parameter to addForce*() calls, determines the exact operation that is carried out.
*/
enum NxForceMode
	{
	NX_FORCE,                   //!< parameter has unit of mass * distance/ time^2, i.e. a force
	NX_IMPULSE,                 //!< parameter has unit of mass * distance /time
	NX_VELOCITY_CHANGE,			//!< parameter has unit of distance / time, i.e. the effect is mass independent: a velocity change.
	NX_SMOOTH_IMPULSE,          //!< same as NX_IMPULSE but the effect is applied over all substeps.  Use this for motion controllers that repeatedly apply an impulse.
	NX_SMOOTH_VELOCITY_CHANGE	//!< same as NX_VELOCITY_CHANGE but the effect is applied over all substeps.  Use this for motion controllers that repeatedly apply an impulse.
	};

/**
NxActor is the main simulation object in the physics sdk. The actor is owned by and contained in a NxScene .
An actor may optionally encapsulate a dynamic rigid body, otherwise it is static (i.e. fixed in the world).

Instances of this class are created by calling NxScene::createActor() and deleted with NxScene::releaseActor().
*/

class NxActor
	{
	protected:
	NX_INLINE					NxActor() : userData(NULL)
											{}
	virtual						~NxActor()	{}

	public:

	// Runtime modifications

	/** Methods for static and dynamic actors  */

	/**
	Methods for setting a dynamic actor's pose in the world.	These methods instantaneously change the 
	actor space to world space transformation. 

	One should exercise restraint in making use of these methods. 

	Static actors should not be moved at all.  There are various internal data structures for static actors
	which may need to be recomputed when one moves.  Also, moving static actors will not interact correctly
	with dynamic actors or joints.  If you would like to directly control an actor's position and would like to 
	have it correctly interact with dynamic bodies and joints, you should create a dynamic body with the 
	NX_BF_KINEMATIC flag, and then use the moveGlobal*() commands to move it along a path!

	When briefly moving dynamic actors, one should not:
	
	* move actors into other actors, thus causing interpenetration (an invalid physical state)
	
	* move an actor  that is connected by a joint to another away from the other (thus causing joint error)

	* When moving jointed actors the joints' cached transform information is destroyed and recreated next frame;
	thus this call is expensive for jointed actors.
	
	setGlobalPose(m) has the same effect as calling	
	setGlobalOrientation(m.M);	and setGlobalPosition(m.t); 
	but setGlobalPose() may be faster as it doesn't recompute some internal values twice.
	*/
	virtual		void			setGlobalPose(const NxMat34&)			= 0;

	/**
	Sets a dynamic actor's pose in the world.
	see ::setGlobalPose() for information.
	*/
	virtual		void			setGlobalPosition(const NxVec3&)		= 0;

	/**
	Sets a dynamic actor's pose in the world.
	see ::setGlobalPose() for information.
	*/
	virtual		void			setGlobalOrientation(const NxMat33&)	= 0;

	/**
	Sets a dynamic actor's pose in the world.
	see ::setGlobalPose() for information.
	*/
	virtual		void			setGlobalOrientationQuat(const NxQuat&)	= 0;

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getGlobalPose(NxMat34&d)			const	{d = getGlobalPoseVal();}

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getGlobalPosition(NxVec3&d)			const	{d = getGlobalPositionVal();}

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getGlobalOrientation(NxMat33&d)		const	{d = getGlobalOrientationVal();}

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getGlobalOrientationQuat(NxQuat&d)	const	{d = getGlobalOrientationQuatVal();}

#ifndef DOXYGEN
	virtual		NxMat34 		getGlobalPoseVal()			const = 0;
	virtual		NxVec3 			getGlobalPositionVal()		const = 0;
	virtual		NxMat33 		getGlobalOrientationVal()	const = 0;
	virtual		NxQuat 			getGlobalOrientationQuatVal()const= 0;
#endif

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	*/
	NX_INLINE	NxMat34 		getGlobalPose()			  const	{ return getGlobalPoseVal()			;}

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	*/
	NX_INLINE	NxVec3 			getGlobalPosition()		  const	{ return getGlobalPositionVal()		;}

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	*/
	NX_INLINE	NxMat33 		getGlobalOrientation()	  const	{ return getGlobalOrientationVal()	;}

	/**
	The getGlobal*() methods retrieve the actor's current actor space to world space transformation.
	*/
	NX_INLINE	NxQuat 			getGlobalOrientationQuat()const	{ return getGlobalOrientationQuatVal();}


	/**
	\deprecated { Use getGlobalPose() instead. }
	*/
	virtual const NxMat34 &	getGlobalPoseReference()	const = 0;

	/**
	The moveGlobal* calls serve to move kinematically controlled
	dynamic actors through the game world.  

	You set a dynamic actor to be kinematic using the NX_BF_KINEMATIC body flag,
	used either in the NxBodyDesc or with raiseBodyFlag().
	
	The move command will result in a velocity that, when successfully carried 
	out (i.e. the motion is not blocked due to joints or collisions) inside run*(),
	will move the body into the desired pose.  After the move is carried out during
	a single time step, the velocity is returned to zero.  Thus, you must
	continuously call this in every time step for kinematic actors so that they 
	move along a path.

	These functions simply store the move destination until run*() is called,
	so consecutive calls will simply overwrite the stored target variable.

	Note that in the future we will provide a mechanism for the motion to be blocked
	in certain cases (such as when a box jams in an automatic door), but currently
	the motion is always fully carried out.	
	*/
	virtual		void			moveGlobalPose(const NxMat34&)			= 0;

	/**
	The moveGlobal* calls serve to move kinematically controlled
	dynamic actors through the game world.  

	See ::moveGlobalPose() for more information.
	*/
	virtual		void			moveGlobalPosition(const NxVec3&)		= 0;

	/**
	The moveGlobal* calls serve to move kinematically controlled
	dynamic actors through the game world.  

	See ::moveGlobalPose() for more information.
	*/
	virtual		void			moveGlobalOrientation(const NxMat33&)	= 0;

	/**
	Creates a new shape and adds it to the list of shapes of this actor.
	This invalidates the pointer returned by getShapes().

	Note: mass properties of dynamic actors will not automatically be recomputed
	to reflect the new mass distribution implied by the shape.  Follow 
	this call with a call to updateMassFromShapes() to do that.
	*/
	virtual		NxShape*		createShape(const NxShapeDesc&)	= 0;

	/**
	Deletes the specified shape. This invalidates the pointer returned by getShapes().
	*/
	virtual void				releaseShape(NxShape&) = 0;

	/**
	Returns the number of shapes assigned to the actor.
	*/
	virtual		NxU32			getNbShapes()		const	= 0;


	/**
	Returns an array of shape pointers of size getNbShapes(). These are the shapes
	used by the actor for collision detection.
	*/
	virtual		NxShape**		getShapes()			const	= 0;

	/**
	Recomputes a dynamic actor's mass properties from its shapes, given
	a constant density or a total mass.  I.e. if you want to set a total mass, 
	leave density at zero and specify a nonzero mass.  Do the opposite to specify a density.
	*/
	virtual		void			updateMassFromShapes(NxReal density, NxReal totalMass)		= 0;

	/**
	Makes the actor dynamic by creating a body from the passed descriptor.
	It overwrites the current body state if any. NxBodyDesc::isValid() must return true.
	*/
	virtual		void			setDynamic(const NxBodyDesc&)		= 0;

	/**
	Returns true if the actor is dynamic.
	*/
	virtual		bool			isDynamic()	const			= 0;

	/*
	AM: This doesn't work yet: 

	Makes a dynamic body static.  (Opposite of setDynamic() ).  Does nothing 
	if the body is already static.  Otherwise it deletes all the dynamics information and effectively
	renders the body immovable.
	virtual		void			setStatic()					= 0;
	*/

	/** Methods for dynamic actors only.  Call setDynamic() to make a static actor dynamic.  */

	/**
	The setCMassLocal*() methods set the pose of the center of mass relative to the actor.
	Methods that automatically compute the center of mass such as updateMassFromShapes() as well as constructing
	the actor using shapes with a given density will set this pose automatically.
	Changing this transform will not move the actor in the world!
	The actor must be dynamic.
	*/
	virtual		void			setCMassOffsetLocalPose(const NxMat34&)			= 0;

	/**
	The setCMassLocal*() methods set the pose of the center of mass relative to the actor.
	See ::setCMassOffsetLocalPose() for more information.
	*/
	virtual		void			setCMassOffsetLocalPosition(const NxVec3&)		= 0;

	/**
	The setCMassLocal*() methods set the pose of the center of mass relative to the actor.
	See ::setCMassOffsetLocalPose() for more information.
	*/
	virtual		void			setCMassOffsetLocalOrientation(const NxMat33&)	= 0;

	/**
	The setCMassGlobal*Offset() methods set the pose of the center of mass relative to world space.
	Note that this will simply transform the parameter to actor space and then call 
	setCMassLocal*(). In other words it only shifts the center of mass but does not move the actor.
	The actor must be dynamic.
	*/
	virtual		void			setCMassOffsetGlobalPose(const NxMat34&)		= 0;

	/**
	The setCMassGlobal*Offset() methods set the pose of the center of mass relative to world space.
	See ::setCMassOffsetGlobalPose() for more information.
	*/
	virtual		void			setCMassOffsetGlobalPosition(const NxVec3&)		= 0;

	/**
	The setCMassGlobal*Offset() methods set the pose of the center of mass relative to world space.
	See ::setCMassOffsetGlobalPose() for more information.
	*/
	virtual		void			setCMassOffsetGlobalOrientation(const NxMat33&)	= 0;

	/**
	The setCMassGlobal*() methods move the actor by setting the pose of the center of mass.
	Here the transform between the center of mass and the actor frame is held fixed and the actor
	to world transform is updated.
	The actor must be dynamic.
	*/
	virtual		void			setCMassGlobalPose(const NxMat34&)			= 0;

	/**
	The setCMassGlobal*() methods move the actor by setting the pose of the center of mass.
	See ::setCMassGlobalPose() for more information.
	*/
	virtual		void			setCMassGlobalPosition(const NxVec3&)		= 0;

	/**
	The setCMassGlobal*() methods move the actor by setting the pose of the center of mass.
	See ::setCMassGlobalPose() for more information.
	*/
	virtual		void			setCMassGlobalOrientation(const NxMat33&)	= 0;

	/**
	The getCMassLocal*() methods retrieve the center of mass pose relative to the actor.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getCMassLocalPose(NxMat34&d)		const	{ d = getCMassLocalPoseVal();		}

	/**
	The getCMassLocal*() methods retrieve the center of mass pose relative to the actor.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getCMassLocalPosition(NxVec3&d)		const	{ d = getCMassLocalPositionVal();	}

	/**
	The getCMassLocal*() methods retrieve the center of mass pose relative to the actor.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getCMassLocalOrientation(NxMat33&d)	const	{ d = getCMassLocalOrientationVal();}

#ifndef DOXYGEN
	virtual		NxMat34 		getCMassLocalPoseVal()				const	= 0;
	virtual		NxVec3 			getCMassLocalPositionVal()			const	= 0;
	virtual		NxMat33 		getCMassLocalOrientationVal()		const	= 0;
#endif

	/**
	The getCMassLocal*() methods retrieve the center of mass pose relative to the actor.
	*/
	NX_INLINE	NxMat34 		getCMassLocalPose()					const	{ return getCMassLocalPoseVal();		}

	/**
	The getCMassLocal*() methods retrieve the center of mass pose relative to the actor.
	*/
	NX_INLINE	NxVec3 			getCMassLocalPosition()				const	{ return getCMassLocalPositionVal();	}

	/**
	The getCMassLocal*() methods retrieve the center of mass pose relative to the actor.
	*/
	NX_INLINE	NxMat33 		getCMassLocalOrientation()			const	{ return getCMassLocalOrientationVal();	}

	/**
	The getCMassGlobal*() methods retrieve the center of mass pose in world space.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getCMassGlobalPose(NxMat34&d)		const	{ d = getCMassGlobalPoseVal();			}

	/**
	The getCMassGlobal*() methods retrieve the center of mass pose in world space.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getCMassGlobalPosition(NxVec3&d)	const	{ d = getCMassGlobalPositionVal();		}

	/**
	The getCMassGlobal*() methods retrieve the center of mass pose in world space.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getCMassGlobalOrientation(NxMat33&d)const	{ d = getCMassGlobalOrientationVal();	}

#ifndef DOXYGEN
	virtual		NxMat34 		getCMassGlobalPoseVal()				const	= 0;
	virtual		NxVec3 			getCMassGlobalPositionVal()			const	= 0;
	virtual		NxMat33 		getCMassGlobalOrientationVal()		const	= 0;
#endif

	/**
	The getCMassGlobal*() methods retrieve the center of mass pose in world space.
	The actor must be dynamic.
	*/
	NX_INLINE	NxMat34 		getCMassGlobalPose()				const { return getCMassGlobalPoseVal();			}

	/**
	The getCMassGlobal*() methods retrieve the center of mass pose in world space.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3 			getCMassGlobalPosition()			const { return getCMassGlobalPositionVal();		}

	/**
	The getCMassGlobal*() methods retrieve the center of mass pose in world space.
	The actor must be dynamic.
	*/
	NX_INLINE	NxMat33 		getCMassGlobalOrientation()			const { return getCMassGlobalOrientationVal();	}

	/**
	Sets the mass of a dynamic actor. Mass must be positive.	The actor must be dynamic.
	The actor must be dynamic.
	*/
	virtual		void			setMass(NxReal) = 0;

	/**
	Retrieves the mass of the actor. Static actors return 0, dynamic actors return a positive value.
	The actor must be dynamic.
	*/
	virtual		NxReal			getMass() const = 0;

	/**
	Sets the inertia tensor, using a parameter specified in mass space coordinates. Note that such matrices are diagonal --
	the passed vector is this diagonal. If your local matrix becomes nondiagonal, you need to diagonalize it and
	rotate the mass space using the setCMass*() methods.
	The actor must be dynamic.
	*/
	virtual		void			setMassSpaceInertiaTensor(const NxVec3& m) = 0;

	/**
	Retrieves the diagonal inertia tensor of the actor relative to the mass coordinate frame.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE	void			getMassSpaceInertiaTensor(NxVec3& dest) const { dest = getMassSpaceInertiaTensorVal(); }

	/**
	Retrieves the inertia tensor of the actor relative to the world coordinate frame.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE		void		getGlobalInertiaTensor(NxMat33& dest) const { dest = getGlobalInertiaTensorVal(); }

	/**
	Retrieves the inverse of the inertia tensor of the actor relative to the world coordinate frame.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE		void		getGlobalInertiaTensorInverse(NxMat33& dest) const { dest = getGlobalInertiaTensorInverseVal(); }

#ifndef DOXYGEN
	virtual		NxVec3			getMassSpaceInertiaTensorVal()		const = 0;
	virtual		NxMat33			getGlobalInertiaTensorVal()			const = 0;
	virtual		NxMat33			getGlobalInertiaTensorInverseVal()	const = 0;
#endif

	/**
	Retrieves the diagonal inertia tensor of the actor relative to the mass coordinate frame.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getMassSpaceInertiaTensor()			const { return getMassSpaceInertiaTensorVal()	; };

	/**
	Retrieves the inertia tensor of the actor relative to the world coordinate frame.
	The actor must be dynamic.
	*/
	NX_INLINE	NxMat33			getGlobalInertiaTensor()			const { return getGlobalInertiaTensorVal()		; };

	/**
	Retrieves the inverse of the inertia tensor of the actor relative to the world coordinate frame.
	The actor must be dynamic.
	*/
	NX_INLINE	NxMat33			getGlobalInertiaTensorInverse()		const { return getGlobalInertiaTensorInverseVal(); };

	/**
	Sets the linear damping coefficient. 0 means no damping, must be nonnegative.
	The default is 0.
	The actor must be dynamic.
	*/
	virtual		void			setLinearDamping(NxReal) = 0;

	/**
	Retrieves the linear damping coefficient.
	The actor must be dynamic.
	*/
	virtual		NxReal			getLinearDamping() const = 0;

	/**
	Sets the angular damping coefficient. 0 means no damping, must be nonnegative.
	The default is 0.05
	The actor must be dynamic.
	*/
	virtual		void			setAngularDamping(NxReal) = 0;

	/**
	Retrieves the angular damping coefficient.
	The actor must be dynamic.
	*/
	virtual		NxReal			getAngularDamping() const = 0;

	/**
	Sets the linear velocity of the actor. Note that if you continuously set the velocity of an actor yourself, 
	forces such as gravity or friction will not be able to manifest themselves, because forces directly
	influence only the velocity of an actor.

	The velocities / momenta of jointed actors can not be set. You should remove the joint, set the velocities,
	and then reconnect the actors. Future versions should perform this automatically.
	The actor must be dynamic.
	*/
	virtual		void			setLinearVelocity(const NxVec3&) = 0;

	/**
	Retrieves the linear velocity of an actor.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE		void		getLinearVelocity(NxVec3& dest) const { dest = getLinearVelocityVal(); }

	/**
	Sets the angular velocity of the actor. Note that if you continuously set the angular velocity of an actor yourself, 
	forces such as friction will not be able to rotate the actor, because forces directly influence only the velocity.

	The velocities / momenta of jointed actors can not be set. You should remove the joint, set the velocities,
	and then reconnect the actors. Future versions should perform this automatically.
	The actor must be dynamic.
	*/
	virtual		void			setAngularVelocity(const NxVec3&) = 0;

	/**
	Retrieves the angular velocity of the actor.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE		void		getAngularVelocity(NxVec3& dest) const { dest = getAngularVelocityVal();  }

#ifndef DOXYGEN
	virtual		NxVec3			getLinearVelocityVal()	const = 0;
	virtual		NxVec3			getAngularVelocityVal()	const = 0;
#endif
	/**
	Retrieves the linear velocity of an actor.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getLinearVelocity()		const { return getLinearVelocityVal();}

	/**
	Retrieves the angular velocity of the actor.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getAngularVelocity()	const { return getAngularVelocityVal();}

	/**
	Lets you set the maximum angular velocity permitted for this actor. Because for various computations, the rotation
	of an object is linearized, quickly rotating actors introduce error into the simulation, which leads to bad things.

	With NxPhysicsSDK::setParameter(MAX_ANGULAR_VELOCITY) you can set the default maximum velocity for actors created
	after the call. Bodies' high angular velocities are clamped to this value. 

	However, because some actors, such as car wheels, should be able to rotate quickly, you can override the default setting
	on a per-actor basis with the below call. Note that objects such as wheels which are approximated with spherical or 
	other smooth collision primitives can be simulated with stability at a much higher angular velocity than, say, a box that
	has corners.
	The actor must be dynamic.
	*/
	virtual		void			setMaxAngularVelocity(NxReal) = 0;

	/**
	Sets the linear momentum of the actor. 
	
	Note that if you continuously set the velocity of a actor yourself, 
	forces such as gravity or friction will not be able to manifest themselves, because forces directly
	influence only the velocity of a actor.

	The velocities / momenta of jointed actors can not be set. You should remove the joint, set the velocities,
	and then reconnect the actors. Future versions should perform this automatically.

	This should only be called outside of NxScene::run().
	The actor must be dynamic.
	*/
	virtual		void			setLinearMomentum(const NxVec3&) = 0;

	/**
	Retrieves the linear momentum of an actor. The momentum is equal to the velocity times the mass.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE		void			getLinearMomentum(NxVec3&dest) const { dest = getLinearMomentumVal(); }

	/**
	Sets the angular momentum of the actor. Note that if you continuously set the angular velocity of an actor yourself, 
	forces such as friction will not be able to rotate the actor, because forces directly
	influence only the velocity of actor.

	The velocities / momenta of jointed actors can not be set. You should remove the joint, set the velocities,
	and then reconnect the actors. Future versions should perform this automatically.

	This should only be called outside of NxScene::run().
	The actor must be dynamic.
	*/
	virtual		void			setAngularMomentum(const NxVec3&) = 0;

	/**
	Retrieves the angular velocity of an actor. The angular momentum is
	equal to the angular velocity times the global space inertia tensor.
	The momentum of an object cannot be set directly: you should set the 
	inertia tensor and the angular velocity separately instead.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	NX_INLINE		void			getAngularMomentum(NxVec3&dest) const {dest = getAngularMomentumVal(); }

#ifndef DOXYGEN
	virtual		NxVec3			getLinearMomentumVal()	const = 0;
	virtual		NxVec3			getAngularMomentumVal()	const = 0;
#endif

	/**
	Retrieves the linear momentum of an actor. The momentum is equal to the velocity times the mass.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getLinearMomentum()		const {return getLinearMomentumVal(); };

	/**
	Retrieves the angular velocity of an actor. The angular momentum is
	equal to the angular velocity times the global space inertia tensor.
	The momentum of an object cannot be set directly: you should set the 
	inertia tensor and the angular velocity separately instead.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getAngularMomentum()	const {return getAngularMomentumVal(); };

	/**
	Applies a force (or impulse) defined in the global coordinate frame, 
	acting at a particular point in global coordinates, to the actor. 
	Note that if the force does not act along the center of mass of the actor, this
	will also add the corresponding torque. Because forces are reset at the end of every timestep, you can maintain a
	total external force on an object by calling this once every frame.

    ::NxForceMode determines if the force is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addForceAtPos(const NxVec3& force, const NxVec3& pos, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies a force (or impulse) defined in the global coordinate frame,
	acting at a particular point in local coordinates, to the actor. 
	Note that if the force does not act along the center of mass of the actor, this
	will also add the corresponding torque. Because forces are reset at the end of every timestep, you can maintain a
	total external force on an object by calling this once every frame.

	::NxForceMode determines if the force is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addForceAtLocalPos(const NxVec3& force, const NxVec3& pos, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies a force (or impulse) defined in the actor local coordinate frame,
	acting at a particular point in global coordinates, to the actor. 
	Note that if the force does not act along the center of mass of the actor, this
	will also add the corresponding torque. Because forces are reset at the end of every timestep, you can maintain a
	total external force on an object by calling this once every frame.

	::NxForceMode determines if the force is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addLocalForceAtPos(const NxVec3& force, const NxVec3& pos, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies a force (or impulse) defined in the actor local coordinate frame,
	acting at a particular point in local coordinates, to the actor. 
	Note that if the force does not act along the center of mass of the actor, this
	will also add the corresponding torque. Because forces are reset at the end of every timestep, you can maintain a
	total external force on an object by calling this once every frame.

	::NxForceMode determines if the force is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addLocalForceAtLocalPos(const NxVec3& force, const NxVec3& pos, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies a force (or impulse) defined in the global coordinate frame to the actor.
	This will not induce a torque.

	::NxForceMode determines if the force is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addForce(const NxVec3&, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies a force (or impulse) defined in the actor local coordinate frame to the actor.

	::NxForceMode determines if the force is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addLocalForce(const NxVec3&, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies an (eventually impulsive) torque defined in the global coordinate frame to the actor.

	::NxForceMode determines if the torque is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addTorque(const NxVec3&, NxForceMode mode = NX_FORCE) = 0;

	/**
	Applies an (eventually impulsive) torque defined in the actor local coordinate frame to the actor.

	::NxForceMode determines if the torque is to be conventional or impulsive.

	The actor must be dynamic.
	*/
	virtual		void			addLocalTorque(const NxVec3&, NxForceMode mode = NX_FORCE) = 0;

	/**
	Computes the total kinetic (rotational and translational) energy of the object.
	The actor must be dynamic.
	*/
	virtual		NxReal			computeKineticEnergy() const = 0;

	/**
	Computes the velocity of a point given in world coordinates if it were attached to the actor and moving with it.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	virtual		void			getPointVelocity(const NxVec3& point, NxVec3& result) const { result = getPointVelocityVal(point); }

	/**
	Computes the velocity of a point given in body local coordinates if it were attached to the actor and moving with it.
	The actor must be dynamic.
	\deprecated { theGet*(x) methods are deprecated.  Use the new convention x = get*() instead. }
	*/
	virtual		void			getLocalPointVelocity(const NxVec3& point, NxVec3& result) const { result = getLocalPointVelocityVal(point); }

#ifndef DOXYGEN
	virtual		NxVec3			getPointVelocityVal(const NxVec3& point)		const = 0;
	virtual		NxVec3			getLocalPointVelocityVal(const NxVec3& point)	const = 0;
#endif

	/**
	Computes the velocity of a point given in world coordinates if it were attached to the actor and moving with it.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getPointVelocity(const NxVec3& point)			{ return getPointVelocityVal(point); };

	/**
	Computes the velocity of a point given in body local coordinates if it were attached to the actor and moving with it.
	The actor must be dynamic.
	*/
	NX_INLINE	NxVec3			getLocalPointVelocity(const NxVec3& point)		{ return getLocalPointVelocityVal(point); };

	/**
	Returns true if this body and all the actors it is touching or is linked to with joints are sleeping.
	When an actor does not move for a period of time, it is no longer simulated in order to save time. This state
	is called sleeping. However, because the object automatically wakes up when it is either touched by an awake object,
	or one of its properties is changed by the user, the entire sleep mechanism should be transparent to the user.
	Note: If an actor is not asleep, its group won't be either, but if it is asleep, its group may not be.
	The actor must be dynamic.
	*/
	virtual		bool			isGroupSleeping() const = 0;

	/**
	Returns true if this body is sleeping.
	When an actor does not move for a period of time, it is no longer simulated in order to save time. This state
	is called sleeping. However, because the object automatically wakes up when it is either touched by an awake object,
	or one of its properties is changed by the user, the entire sleep mechanism should be transparent to the user.
	Note: If an actor is not asleep, its group won't be either, but if it is asleep, its group may not be.

	If an actor is asleep after the call to NxScene::run() returns, it is guaranteed that the pose of the actor 
	was not changed in NxScene::run(). 	You can use this information to avoid updatingthe pose of shapes 
	in the collision detection library.
	The actor must be dynamic.
	*/
	virtual		bool			isSleeping() const = 0;

	/**
	Returns the linear velocity below which an actor may go to sleep. Actors whose linear velocity is above
    this threshold will not be put to sleep.
    
    The actor must be dynamic.

    @see isSleeping

	*/
    virtual		NxReal			getSleepLinearVelocity() const = 0;

    /**

	Sets the linear velocity below which an actor may go to sleep. Actors whose linear velocity is above
    this threshold will not be put to sleep. If the threshold value is -1, the velocity threshold is
    set to the NxPhysicsSDK's NX_DEFAULT_SLEEP_LIN_VEL parameter.
    
    The actor must be dynamic.

    @see isSleeping

	*/
    virtual		void			setSleepLinearVelocity(NxReal threshold) = 0;

	/**
	Returns the angular velocity below which an actor may go to sleep. Actors whose angular velocity is above
    this threshold will not be put to sleep. 
    
    The actor must be dynamic.

    @see isSleeping

	*/
    virtual		NxReal			getSleepAngularVelocity() const = 0;

 	/**
	Sets the angular velocity below which an actor may go to sleep. Actors whose angular velocity is above
    this threshold will not be put to sleep. If the threshold value is -1, the velocity threshold is
    set to the NxPhysicsSDK's NX_DEFAULT_SLEEP_ANG_VEL parameter.
    
    The actor must be dynamic.

    @see isSleeping

	*/
    virtual		void			setSleepAngularVelocity(NxReal threshold) = 0;

	/**
	Wakes up the actor if it is sleeping.  
	*/
	virtual		void			wakeUp(NxReal wakeCounterValue=NX_NUM_SLEEP_FRAMES)	= 0;

	/**
	Forces the actor to sleep.  
	*/
	virtual		void			putToSleep()	= 0;

	/**
	Raises a particular actor flag. See the list of flags in NxActorDesc.h.
	*/
	virtual		void			raiseActorFlag(NxActorFlag)			= 0;
	/**
	Clears a particular actor flag. See the list of flags in NxActorDesc.h.
	*/
	virtual		void			clearActorFlag(NxActorFlag)			= 0;
	/**
	Reads a particular actor flag. See the list of flags in NxActorDesc.h.
	*/
	virtual		bool			readActorFlag(NxActorFlag)	const	= 0;

	/**
	Raises a particular body flag. See the list of flags in NxBodyDesc.h.
	*/
	virtual		void			raiseBodyFlag(NxBodyFlag)				= 0;
	/**
	Clears a particular body flag. See the list of flags in NxBodyDesc.h.
	*/
	virtual		void			clearBodyFlag(NxBodyFlag)				= 0;
	/**
	Reads a particular body flag. See the list of flags in NxBodyDesc.h.
	*/
	virtual		bool			readBodyFlag(NxBodyFlag)		const	= 0;

	/**
	saves the body information of a dynamic actor to the passed body
	descriptor.
	*/
	virtual		bool			saveBodyToDesc(NxBodyDesc&) = 0;

	/**
	Saves the state of the object to the passed descriptor.
	Does not save out any eventual shapes of the actor to the descriptor's
	shape
	vector, nor does it write to its body member.  You have to iterate through
	the shapes of the actor and save them manually, and you have to call the
	saveBodyToDesc() method for dynamic actors.
	*/
	virtual		void			saveToDesc(NxActorDescBase&) = 0;

	/**
	Sets a name string for the object that can be retrieved with getName().  This is for debugging and is not used
	by the SDK.  The string is not copied by the SDK, only the pointer is stored.
	*/
	virtual		void			setName(const char*)		= 0;

	/**
	retrieves the name string set with setName().
	*/
	virtual		const char*		getName()			const	= 0;

	/**
	Assigns the actor to a user defined group of actors.  NxActorGroup is a 16 bit group identifier.
	This is similar to NxShape groups, except those are only five bits and serve a different purpose.
	The NxPhysicsSDK::setActorGroupPairFlags() lets you set certain behaviors for pairs of actor groups.
	By default every actor is created in group 0.
	*/
	virtual		void			setGroup(NxActorGroup)		 = 0;
	/**
	retrieves the value set with setGroup().
	*/
	virtual		NxActorGroup	getGroup() const			 = 0;

	//public variables:
				void*			userData;	//!< user can assign this to whatever, usually to create a 1:1 relationship with a user object.
	};

#endif
