#ifndef NX_PHYSICS_NXINERTIATENSOR
#define NX_PHYSICS_NXINERTIATENSOR
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

#include "Nxp.h"

	/**
	Computes mass of a homogeneous sphere according to sphere density.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeSphereMass			(NxReal radius, NxReal density);
	/**
	Computes density of a homogeneous sphere according to sphere mass.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeSphereDensity		(NxReal radius, NxReal mass);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	Computes mass of a homogeneous box according to box density.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeBoxMass			(const NxVec3& extents, NxReal density);
	/**
	Computes density of a homogeneous box according to box mass.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeBoxDensity			(const NxVec3& extents, NxReal mass);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	Computes mass of a homogeneous ellipsoid according to ellipsoid density.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeEllipsoidMass		(const NxVec3& extents, NxReal density);
	/**
	Computes density of a homogeneous ellipsoid according to ellipsoid mass.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeEllipsoidDensity	(const NxVec3& extents, NxReal mass);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	Computes mass of a homogeneous cylinder according to cylinder density.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeCylinderMass		(NxReal radius, NxReal length, NxReal density);
	/**
	Computes density of a homogeneous cylinder according to cylinder mass.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeCylinderDensity	(NxReal radius, NxReal length, NxReal mass);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	Computes mass of a homogeneous cone according to cone density.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeConeMass			(NxReal radius, NxReal length, NxReal density);
	/**
	Computes density of a homogeneous cone according to cone mass.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT NxReal NX_CALL_CONV NxComputeConeDensity		(NxReal radius, NxReal length, NxReal mass);

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

	/**
	Computes diagonalized inertia tensor for a box.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT void NX_CALL_CONV NxComputeBoxInertiaTensor	(NxVec3& diagInertia, NxReal mass, NxReal xlength, NxReal ylength, NxReal zlength);
	/**
	Computes diagonalized inertia tensor for a sphere.
	*/
	NX_C_EXPORT NXP_DLL_EXPORT void NX_CALL_CONV NxComputeSphereInertiaTensor(NxVec3& diagInertia, NxReal mass, NxReal radius, bool hollow);

#endif
