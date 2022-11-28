#ifndef NX_FLUIDS_NXPARTICLEDATA
#define NX_FLUIDS_NXPARTICLEDATA
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/

/**
Not available in the current release.
Particle flags are used to specify the state particles are in.
*/
enum NxParticleFlag
	{
	NX_FP_DIED						= (1<<0),	
	NX_FP_LIFETIME_EXPIRED			= (1<<1),
	NX_FP_DRAINED					= (1<<2),
	NX_FP_SEPARATED					= (1<<3),
	};

/**
Not available in the current release.
Descriptor-like user side class representing a wrapper for a buffer provided to the user. It is used to
submit particles to the simulation and to retreive information about particles in the simulation. 
*/
class NxParticleData
	{
	public:

	/**
	Not available in the current release.
	Specifies the maximal number of particle elements the buffers can hold.
	*/
	NxU32					maxParticles;
    
	/**
	Not available in the current release.
	Has to point to the user allocated memory holding the number of elements stored in the buffers. 
	If the SDK writes to a given particle buffer, it also sets the numbers of elements written. If 
	this is set to NULL, the SDK can't write to the given buffer.
	*/
	NxU32*					numParticlesPtr;

	/**
	Not available in the current release.
	The pointer to the user specified/allocated buffer for particle positions. A position consists 
	of three consequent 32 bit floats. If set to NULL, positions are not read or written to.
	*/
	NxF32*					bufferPos;
	
	/**
	Not available in the current release.
	Same as bufferPos but for particle velocities. A velocity consists of three consequent 32 bit 
	floats. If set to NULL, velocities are not read or written to.
	*/
	NxF32*					bufferVel;
	
	/**
	Not available in the current release.
	Same as bufferPos but for particle lifetimes. A particle lifetime velocity consists of one 32 bit 
	float. If set to NULL, lifetimes are not read or written to.
	*/
	NxF32*					bufferLife;
    
	/**
	Not available in the current release.
	Same as bufferPos but for particle flags of the type NxParticleFlag. The flags are repersended by 
	one 32 bit unsigned. If set to NULL, flags are not read or written to.
	*/
	NxU32*					bufferFlags;


	NxU32					bufferPosByteStride;
	NxU32					bufferVelByteStride;
	NxU32					bufferLifeByteStride;
	NxU32					bufferFlagsByteStride;

	const char*				name;			//!< Possible debug name. The string is not copied by the SDK, only the pointer is stored.

	NX_INLINE ~NxParticleData();
	/**
	Not available in the current release.
	(Re)sets the structure to the default.	
	*/
	NX_INLINE void setToDefault();
	/**
	Not available in the current release.
	Returns true if the current settings are valid
	*/
	NX_INLINE bool isValid() const;

	/**
	Not available in the current release.
	Constructor sets to default.
	*/
	NX_INLINE	NxParticleData();
	};

NX_INLINE NxParticleData::NxParticleData()
	{
	setToDefault();
	}

NX_INLINE NxParticleData::~NxParticleData()
	{
	}

NX_INLINE void NxParticleData::setToDefault()
	{
	maxParticles			= 0;
	numParticlesPtr			= NULL;
	bufferPos				= NULL;
	bufferVel				= NULL;
	bufferLife				= NULL;
	bufferFlags				= NULL;
	bufferPosByteStride		= 0;
	bufferVelByteStride		= 0;
	bufferLifeByteStride	= 0;
	bufferFlagsByteStride	= 0;
	name					= NULL;
	}

NX_INLINE bool NxParticleData::isValid() const
	{
	if (numParticlesPtr && !(bufferPos || bufferVel || bufferLife || bufferFlags)) return false;
	if ((bufferPos || bufferVel || bufferLife || bufferFlags) && !numParticlesPtr) return false;
	if (bufferPos && !bufferPosByteStride) return false;
	if (bufferVel && !bufferVelByteStride) return false;
	if (bufferLife && !bufferLifeByteStride) return false;
	if (bufferFlags && !bufferFlagsByteStride) return false;
    
	return true;
	}


#endif
