#ifndef NX_PHYSICS_NXP
#define NX_PHYSICS_NXP
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

//this header should be included first thing in all headers in physics/include
//@epic_sas_xenon start
#if !defined(_XBOX)
//@epic_sas_xenon stop
#ifndef NXP_DLL_EXPORT
	#if defined NX_PHYSICS_DLL

		#define NXP_DLL_EXPORT __declspec(dllexport)
		#define NXF_DLL_EXPORT __declspec(dllimport)

	#elif defined NX_PHYSICS_STATICLIB

		#define NXP_DLL_EXPORT
		#define NXF_DLL_EXPORT

	#elif defined NX_USE_SDK_DLLS

		#define NXP_DLL_EXPORT __declspec(dllimport)
		#define NXF_DLL_EXPORT __declspec(dllimport)

	#elif defined NX_USE_SDK_STATICLIBS

		#define NXP_DLL_EXPORT
		#define NXF_DLL_EXPORT

	#else

		//#error Please define either NX_USE_SDK_DLLS or NX_USE_SDK_STATICLIB in your project settings depending on the kind of libraries you use!
		#define NXP_DLL_EXPORT __declspec(dllimport)
		#define NXF_DLL_EXPORT __declspec(dllimport)
  			
	#endif
#endif
//@epic_sas_xenon start
#else	//#if !defined(_XBOX)
	#define NXP_DLL_EXPORT 
	#define NXF_DLL_EXPORT 
#endif	//#if !defined(_XBOX)
//@epic_sas_xenon stop

#include "Nxf.h"
#include "NxVec3.h"
#include "NxQuat.h"
#include "NxMat33.h"
#include "NxMat34.h"

#include "../../NxVersionNumber.h"
/**
Pass the constant NX_PHYSICS_SDK_VERSION to the NxCreatePhysicsSDK function. 
This is to ensure that the application is using the same header version as the
library was built with.
*/

#define NX_PHYSICS_SDK_VERSION ((   NX_SDK_VERSION_MAJOR   <<24)+(NX_SDK_VERSION_MINOR    <<16)+(NX_SDK_VERSION_BUGFIX    <<8) + 0)
//2.1.1 Automatic scheme via VersionNumber.h on July 9, 2004.
//2.1.0 (new scheme: major.minor.build.configCode) on May 12, 2004.  ConfigCode can be i.e. 32 vs. 64 bit.
//2.3 on Friday April 2, 2004, starting ag. changes.
//2.2 on Friday Feb 13, 2004
//2.1 on Jan 20, 2004


#define NX_RELEASE(x)	\
	if(x)				\
		{				\
		x->release();	\
		x = NULL;		\
		}

//Note: users can't change these defines as it would need the libraries to be recompiled!
//#define UNDEFINE_ME_BEFORE_SHIPPING

//#define NX_DEBUG_OUTPUT_STATS
//#define MSH_SUPPORT_AERO_MAP

#define NX_SHAPE_DESC_LIST

//#define NX_SHARED_STREAM
//#define NX_LAZY_NPPC
//#define NX_SHARED_MEMORY_UPDATE_LIST
//#define NX_SINGLE_PATCH_PER_PAIR
//#define NX_USE_ACTOR_MTF_UPDATE
//#define NX_USE_ADAPTIVE_SLEEPING
#define USE_SOLVER_CALLBACK
#define NX_USE_ADAPTIVE_FORCE
#define MAINTAIN_PREVIOUS_POSE
//#define NX_SUPPORT_MESH_SCALE		// Experimental mesh scale support
#define ADAM_CCD2					//this is basic CCD mechanism.  Raycast ccd builds on this to work well.
#define ADAM_CCD3					//exact volumetric ccd

// If we are exposing the Fluid API.
#if defined(_XBOX)
#else	//#if defined(_XBOX)
#define NX_USE_FLUID_API 1
#endif	//#if defined(_XBOX)

// If we are compiling in support for Fluid.
// We check to see if this is already defined
// by the Project or Makefile.
#ifndef NX_USE_SDK_FLUIDS
  #define NX_USE_SDK_FLUIDS 1
#endif

#define FLUID_2_1_2 1
//#define FLUID_CCD
//#define NX_LEGACY_LOCAL_FORCE		//for compatibility with space-bug
//#define NX_IMMEDIATE_CONTACT_NOTIFY

// Define those two ones to report normal & friction forces in contact report. It's otherwise not needed for the SDK to run...
	#define REPORT_SUM_NORMAL_FORCE
	#define FRICTION_DEBUG_VIS

#define OBSERVABLE_BODY				// can't get rid of them for now, because of BodyEffectors
#ifdef UNDEFINE_ME_BEFORE_SHIPPING
	//	#define USE_PROFILER	// WARNING: profiling has a significant speed hit (we now profile very low-level stuff)
//	#define DEBUG_DETERMINISM	// WARNING: has a significant speed hit.  Causes us to do direct file system access.
	#define DO_INTERNAL_PROFILING
#endif

// a bunch of simple defines used in several places:

typedef NxU16 NxActorGroup;
typedef NxU16 NxCollisionGroup;		// Must be < 32
typedef NxU16 NxMaterialIndex;

#if NX_USE_FLUID_API
typedef NxU16 NxFluidGroup;
#endif

#endif
