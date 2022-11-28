#ifndef NX_PHYSICS_NX_PHYSICS_SDK
#define NX_PHYSICS_NX_PHYSICS_SDK
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"

class NxScene;
class NxSceneDesc;
class NxUserDebugRenderer;
class NxTriangleMesh;
class NxTriangleMeshDesc;
class NxUserOutputStream;
class NxUserAllocator;
class NxMaterial;
class NxActor;
class NxJoint;
class NxPerformanceInspector;

/**
Parameters enums to be used as the 1st arg to setParameter or getParameter.
*/
enum NxParameter
	{
	/** RigidBody-related parameters  */
	NX_PENALTY_FORCE,					//!< The penalty force applied to the bodies in an interpenetrating contact is scaled by this value. (range: [0, inf]) Default: 0.6
	NX_MIN_SEPARATION_FOR_PENALTY,		//!< The minimum contact separation value in order to apply a penalty force. (range: [-inf, 0]) I.e. This must be negative!!  Default: -0.05

	NX_DEFAULT_SLEEP_LIN_VEL_SQUARED,	//!< The default linear velocity, squared, below which objects start going to sleep. (range: [0, inf]) Default: (0.15*0.15)
	NX_DEFAULT_SLEEP_ANG_VEL_SQUARED,	//!< The default angular velocity, squared, below which objects start going to sleep. (range: [0, inf]) Default: (0.14*0.14)

	NX_BOUNCE_TRESHOLD,					//!< A contact with a relative velocity below this will not bounce.	(range: [-inf, 0]) Default: -2

	NX_DYN_FRICT_SCALING,				//!< This lets the user scale the magnitude of the dynamic friction applied to all objects.	(range: [0, inf]) Default: 1
	NX_STA_FRICT_SCALING,				//!< This lets the user scale the magnitude of the static friction applied to all objects.	(range: [0, inf]) Default: 1

	NX_MAX_ANGULAR_VELOCITY,			//!< See the comment for NxBody::setMaxAngularVelocity() for details.	Default: 7

	/** Collision-related parameters:  */

	NX_MESH_MESH_LEVEL,					//!< Level of detail setting for mesh-mesh collision detection. Lower number is less accurate but more robust. Integer in [0, inf]  Default: 4
	NX_ENABLE_MESH_DEBUG,				//!< Set this to nonzero before creating any meshes that you will want to visualize with NxTriangleMeshShape::DebugRender(). Note that this implies a certain data overhead, so don't leave this in your release version!  Default: 0
	NX_COLL_INFINITY,					//!< This is a floating point value that will be used as infinity. For example, the axis aligned bounding box of the z=0 plane is [-inf, inf]x[-inf, inf]x[0,0]. Default: 1e20;
	NX_CONTINUOUS_CD,					//!< Enable/disable continuous collision detection (0.0f to disable)
	NX_MESH_HINT_SPEED,					//!< Favorize speed versus memory consumption for meshes


	/**
	The below settings permit the debug visualization of various simulation properties. 
	The setting is either zero, in which case the property is not drawn. Otherwise it is a scaling factor
	that determines the size of the visualization widgets.

	Only bodies and joints for which visualization is turned on using setFlag(VISUALIZE) are visualized.
	Contacts are visualized if they involve a body which is being visualized.
	Default is 0.

	Notes:
	- to see any visualization, you have to set NX_VISUALIZATION_SCALE to nonzero first.
	- the scale factor has been introduced because it's difficult (if not impossible) to come up with a
	good scale for 3D vectors. Normals are normalized and their length is always 1. But it doesn't mean
	we should render a line of length 1. Depending on your objects/scene, this might be completely invisible
	or extremely huge. That's why the scale factor is here, to let you tune the length until it's ok in
	your scene.
	- however, things like collision shapes aren't ambiguous. They are clearly defined for example by the
	triangles & polygons themselves, and there's no point in scaling that. So the visualization widgets
	are only scaled when it makes sense.
	*/
	NX_VISUALIZATION_SCALE,			//!< This overall visualization scale gets multiplied with the individual scales. Setting to zero turns ignores all visualizations. Default is 0.

	NX_VISUALIZE_WORLD_AXES,
	NX_VISUALIZE_BODY_AXES,
	NX_VISUALIZE_BODY_MASS_AXES,
	NX_VISUALIZE_BODY_LIN_VELOCITY,
	NX_VISUALIZE_BODY_ANG_VELOCITY,
	NX_VISUALIZE_BODY_LIN_MOMENTUM,
	NX_VISUALIZE_BODY_ANG_MOMENTUM,
	NX_VISUALIZE_BODY_LIN_ACCEL,
	NX_VISUALIZE_BODY_ANG_ACCEL,
	NX_VISUALIZE_BODY_LIN_FORCE,
	NX_VISUALIZE_BODY_ANG_FORCE,
	NX_VISUALIZE_BODY_REDUCED,
	NX_VISUALIZE_BODY_JOINT_GROUPS,
	NX_VISUALIZE_BODY_CONTACT_LIST,
	NX_VISUALIZE_BODY_JOINT_LIST,
	NX_VISUALIZE_BODY_DAMPING,
	NX_VISUALIZE_BODY_SLEEP,

	NX_VISUALIZE_JOINT_LOCAL_AXES,
	NX_VISUALIZE_JOINT_WORLD_AXES,
	NX_VISUALIZE_JOINT_LIMITS,
	NX_VISUALIZE_JOINT_ERROR,
	NX_VISUALIZE_JOINT_FORCE,
	NX_VISUALIZE_JOINT_REDUCED,

	NX_VISUALIZE_CONTACT_POINT,
	NX_VISUALIZE_CONTACT_NORMAL,
	NX_VISUALIZE_CONTACT_ERROR,
	NX_VISUALIZE_CONTACT_FORCE,

	NX_VISUALIZE_ACTOR_AXES,

	NX_VISUALIZE_COLLISION_AABBS,		//!< Visualize bounds (AABBs in world space)
	NX_VISUALIZE_COLLISION_SHAPES,		//!< Shape visualization
	NX_VISUALIZE_COLLISION_AXES,		//!< Shape axis visualization
	NX_VISUALIZE_COLLISION_COMPOUNDS,	//!< Compound visualization (compound AABBs in world space)
	NX_VISUALIZE_COLLISION_VNORMALS,	//!< Mesh & convex vertex normals
	NX_VISUALIZE_COLLISION_FNORMALS,	//!< Mesh & convex face normals
	NX_VISUALIZE_COLLISION_SPHERES,		//!< Bounding spheres

	NX_VISUALIZE_COLLISION_SAP,			//!< SAP structures
	NX_VISUALIZE_COLLISION_STATIC,		//!< Static pruning structures
	NX_VISUALIZE_COLLISION_DYNAMIC,		//!< Dynamic pruning structures
	NX_VISUALIZE_COLLISION_FREE,		//!< "Free" pruning structures

#if NX_USE_FLUID_API
	NX_VISUALIZE_FLUID_EMITTERS,		//!< Not available in the current release. Emitter visualization.
	NX_VISUALIZE_FLUID_POSITION,		//!< Not available in the current release. Particle position visualization.
	NX_VISUALIZE_FLUID_VELOCITY,		//!< Not available in the current release. Particle velocity visualization.
	NX_VISUALIZE_FLUID_KERNEL_RADIUS,	//!< Not available in the current release. Particle kernel radius visualization.
	NX_VISUALIZE_FLUID_BOUNDS,			//!< Not available in the current release. Fluid AABB visualization.
#endif

	NX_ADAPTIVE_FORCE,
	NX_PARAMS_NUM_VALUES				//!< This is not a parameter, it just records the current number of parameters.
	};

/**
	Abstract singleton factory class used for instancing objects in the Physics SDK. You can get an instance of this
	class by calling NxCreatePhysicsSDK().
*/

class NxPhysicsSDK
	{
	protected:
			NxPhysicsSDK()	{}
	virtual	~NxPhysicsSDK()	{}

	public:

	/**
	Destroys the instance it is called on.

	Use this release method to destroy an instance of this class. Be sure
	to not keep a reference to this object after calling release.
	*/
	virtual	void release() = 0;

	/**
	Function that lets you set global simulation parameters.
	Returns false if the value passed is out of range for usage specified by the enum.
	*/
	virtual bool setParameter(NxParameter paramEnum, NxReal paramValue) = 0;

	/**
	Function that lets you query global simulation parameters.
	*/
	virtual NxReal getParameter(NxParameter paramEnum) const = 0;

	/**
	Creates a scene. The scene can then create its contained entities.
	*/
	virtual NxScene* createScene(const NxSceneDesc&) = 0;

	/**
	Deletes the instance passed.
	Be sure	to not keep a reference to this object after calling release.
	*/
	virtual void releaseScene(NxScene&) = 0;

	/**
	Gets number of created scenes.
	*/
	virtual NxU32 getNbScenes()			const	= 0;

	/**
	Retrieves pointer to created scenes.
	*/
	virtual NxScene* getScene(NxU32)			= 0;

	/**
	Creates a triangle mesh object. This can then be instanced into TriangleMeshShape objects.
	*/
	virtual NxTriangleMesh* createTriangleMesh(const NxTriangleMeshDesc&) = 0;

	/**
	Destroys the instance passed.
	Be sure	to not keep a reference to this object after calling release.
	Do not release the triangle mesh before all its instances are released first!
	*/
	virtual	void	releaseTriangleMesh(NxTriangleMesh&) = 0;

	/**
	It is possible to assign each shape to a collision groups using NxShape::setGroup().
	With this method one can set whether collisions should be detected between shapes 
	belonging to a given pair of groups. Initially all pairs are enabled.

	Collision detection between two shapes a and b occurs if: 
	getGroupCollisionFlag(a->getGroup(), b->getGroup()) && isEnabledPair(a,b) is true.

	Fluids can be assigned to collision groups as well.

	NxCollisionGroup is an integer between 0 and 31.
	*/
	virtual void setGroupCollisionFlag(NxCollisionGroup group1, NxCollisionGroup group2, bool enable) = 0;

	/**
	This reads the value set with the above call.
	NxCollisionGroup is an integer between 0 and 31.
	*/
	virtual bool getGroupCollisionFlag(NxCollisionGroup group1, NxCollisionGroup group2) const = 0;

	/**
	It is possible to assign each actor to a group using NxActor::setGroup().  This is a different
	set of groups from the shape groups despite the similar name.  Here more up to 0xffff different groups are permitted,
	With this method one can set contact reporting flags between actors belonging to a pair of groups.

	The following flags are permitted:

	NX_NOTIFY_ON_START_TOUCH
	NX_NOTIFY_ON_END_TOUCH	
	NX_NOTIFY_ON_TOUCH		
	NX_NOTIFY_ON_IMPACT		
	NX_NOTIFY_ON_ROLL		
	NX_NOTIFY_ON_SLIDE	

	See ::NxContactPairFlag.

	Note that finer grain control of pairwise flags is possible using the functions
	NxScene::setShapePairFlags() and NxScene::setActorPairFlags().
	*/
	virtual void setActorGroupPairFlags(NxActorGroup group1, NxActorGroup group2, NxU32 flags) = 0;

	/**
	This reads the value set with the above call.
	*/
	virtual NxU32 getActorGroupPairFlags(NxActorGroup group1, NxActorGroup group2) const = 0;


#if NX_USE_FLUID_API

	/**
	Not available in the current release.
	Fluids can be assigned to fluid groups by using NxFluid::setFluidGroup(). This method sets
	fluid actor group pairs to configure collision and collision reports between 
	fluids and actors using the flags:

	NX_IGNORE_PAIR
	NX_NOTIFY_ON_COLLISION

	See ::NxFluidContactFlag.
	*/
	virtual void setFluidGroupPairFlags(NxActorGroup actorGroup, NxFluidGroup fluidGroup, NxU32 flags) = 0;

	/**
	Not available in the current release.
	This reads the value set with the above call.
	*/
	virtual NxU32 getFluidGroupPairFlags(NxActorGroup actorGroup, NxFluidGroup fluidGroup) const = 0;

#endif

	/**
	Renders visualization graphics to the passed rendering device.
	*/
	virtual void visualize(const NxUserDebugRenderer&) = 0;

	/**
	Adds a new material to the material list, and returns its index.

	The material library consists of an array of material objects.  Each
	material has a well defined index that can be used to refer to it.
	If an object (shape or triangle) references an undefined material,
	the default material with index 0 is used instead.
	*/
	virtual NxMaterialIndex addMaterial(const NxMaterial &) = 0;

	/**
	Grows the material array up to slot index, and adds it there.
	If the material pointer is 0, the material at that index is deleted.
	*/
	virtual void setMaterialAtIndex(NxMaterialIndex, const NxMaterial *) = 0;

	/**
	Retrieves the material at the given index.  If the given slot is unused, 0 is returned.
	*/
	virtual NxMaterial * getMaterial(NxMaterialIndex) = 0;

	/**
	Returns the size of the internal material array, i.e. one greater than the highest current material index.
	*/
	virtual NxU32 getNbMaterials() = 0;

	/**
	Deletes all materials except for the default one at index 0.
	*/
	virtual void purgeMaterials() = 0;

	/**
	Emits a dump of the physics SDK state into the specified file.  The extension of
  '.PSC' is automatically appended.  If you set 'binary' to true then all floating
  point values will be recorded at full resolution.  PSC files can be loaded into the NovodexRocket
  testing framework for debugging purposes.
	*/
	virtual bool	coreDump(const char* fname, bool binary, const char* addendum=0) = 0;

	virtual	bool	setPerformanceInspector(NxPerformanceInspector* npi) = 0;
	};

/**
Creates an instance of this class. May not be a class member to avoid name mangling.
Pass the constant NX_PHYSICS_SDK_VERSION as the argument.
Because the class is a singleton class, multiple calls return the same object.
*/
NX_C_EXPORT NXP_DLL_EXPORT NxPhysicsSDK* NX_CALL_CONV NxCreatePhysicsSDK(NxU32 sdkVersion, NxUserAllocator* allocator = NULL, NxUserOutputStream* outputStream = NULL);

#endif
