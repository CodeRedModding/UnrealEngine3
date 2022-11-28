#ifndef NX_PHYSICS_NXCONTACTSTREAMITERATOR
#define NX_PHYSICS_NXCONTACTSTREAMITERATOR
/*----------------------------------------------------------------------------*\
|
|						Public Interface to NovodeX Technology
|
|							     www.novodex.com
|
\*----------------------------------------------------------------------------*/
#include "Nxp.h"

typedef NxU32 * NxContactStream;
typedef const NxU32 * NxConstContactStream;

enum NxShapePairStreamFlags
	{
	NX_SF_HAS_MATS_PER_POINT		= (1<<0),	//!< used when we have materials per triangle in a mesh.  In this case the extData field is used after the point separation value.
	NX_SF_IS_INVALID				= (1<<1),	//!< this pair was invalidated in the system after being generated.  The user should ignore these pairs.
	NX_SF_HAS_FEATURES_PER_POINT	= (1<<2),	//!< the stream includes per-point feature data
	//note: bits 8-15 are reserved for internal use (static ccd pullback counter)
	};

/**
NxContactStreamIterator is for iterating through packed contact streams.

The user code to use this iterator looks like this:
void MyUserContactInfo::onContactNotify(NxPair & pair, NxU32 events)
	{
	NxContactStreamIterator i(pair.stream);
	//user can call getNumPairs() here
	while(i.goNextPair())
		{
		//user can also call getShape() and getNumPatches() here
		while(i.goNextPatch())
			{
			//user can also call getPatchNormal() and getNumPoints() here
			while(i.goNextPoint())
				{
				//user can also call getPoint() and getSeparation() here
				}
			}
		}
	}

Note:  It is NOT OK to skip any of the iteration calls.  For example, you may NOT put a break or a continue
statement in any of the above blocks, short of completely aborting the iteration and deleting the 
NxContactStreamIterator object.
If the stream was received from a NxFluidContactPair then some iterator methods work slightly
different (see comments to the methods below).
*/

class NxContactStreamIterator
	{
	public:
	/**
	Starts the iteration, and returns the number of pairs.
	*/
	NX_INLINE NxContactStreamIterator(NxConstContactStream stream);

	//iteration:
	NX_INLINE bool goNextPair();	//!< Goes on to the next pair, silently skipping invalid pairs.  Returns false if there are no more pairs.  Note that getNumPairs() also includes invalid pairs in the count.
	NX_INLINE bool goNextPatch();	//!< Goes on to the next patch (contacts with the same normal).  Returns false if there are no more.
	NX_INLINE bool goNextPoint();	//!< Goes on to the next contact point.  Returns false if there are no more.

	//accessors:
	NX_INLINE NxU32 getNumPairs();	//!< May be called at any time.  Returns the number of pairs in the structure.  Note: now some of these pairs may be marked invalid using getShapeFlags() & NX_SF_IS_INVALID, so the effective number may be lower.  goNextPair() will automatically skip these!

	/**
	may be called after goNextPair() returned true. ShapeIndex is 0 or 1.
	Fluid actor contact stream: getShape(1) always returns NULL.
	*/
	NX_INLINE NxShape * getShape(NxU32 shapeIndex);
	NX_INLINE NxU16 getShapeFlags();	//!< may be called after goNextPair() returned true
	NX_INLINE NxU32 getNumPatches();	//!< may be called after goNextPair() returned true
	NX_INLINE NxU32 getNumPatchesRemaining();//!< may be called after goNextPair() returned true

	NX_INLINE const NxVec3 & getPatchNormal();	//!< may be called after goNextPatch() returned true
	NX_INLINE NxU32 getNumPoints();//!< may be called after goNextPatch() returned true
	NX_INLINE NxU32 getNumPointsRemaining();//!< may be called after goNextPatch() returned true

	/**
	may be called after goNextPoint() returned true
	returns to contact point position.
	Fluid actor contact stream: If NX_SF_HAS_FEATURES_PER_POINT is specified, this returns
	the barycentric coordinates within the contact triangle. In this case the z coordinate is 0.0.
	*/
	NX_INLINE const NxVec3 & getPoint();
	
	/**
	may be called after goNextPoint() returned true
	Fluid actor contact stream: always returns 0.0f.
	*/
	NX_INLINE NxReal getSeparation();

	/**
	may be called after goNextPoint() returned true
	If NX_SF_HAS_FEATURES_PER_POINT is specified, this method returns a feature belonging to shape 0,
	Fluid actor contact stream: returns the triangle index if shape 0 is a mesh shape.
	*/
	NX_INLINE NxU32 getFeatureIndex0();

	/**
	may be called after goNextPoint() returned true
	If NX_SF_HAS_FEATURES_PER_POINT is specified, this method returns a feature belonging to shape 1,
	Fluid actor contact stream: returns always 0.
	*/
	NX_INLINE NxU32 getFeatureIndex1();

	NX_INLINE NxU32 getExtData();	//!< return a combination of ::NxShapePairStreamFlags

	private:
	//iterator variables -- are only initialized by stream iterator calls:
	//Note: structs are used so that we can leave the iterators vars on the stack as they were
	//and the user's iteration code doesn't have to change when we add members.

	NxU32 numPairs;
	//current pair properties:
	NxShape * shapes[2];
	NxU16 shapeFlags;
	NxU16 numPatches;
	//current patch properties:
	const NxVec3 * patchNormal;
	NxU32  numPoints;
	//current point properties:
	const NxVec3 * point;
	NxReal separation;
	NxU32 featureIndex0;
	NxU32 featureIndex1;
	NxU32 extData;			//only exists if (shapeFlags & NX_SF_HAS_MATS_PER_POINT)

	NxU32 numPairsRemaining, numPatchesRemaining, numPointsRemaining;
	protected:
	NxConstContactStream stream;
	};

NX_INLINE NxContactStreamIterator::NxContactStreamIterator(NxConstContactStream s)
	{
	stream = s;
	numPairsRemaining = numPairs = stream ? *stream++ : 0;
	}

NX_INLINE NxU32 NxContactStreamIterator::getNumPairs()
	{
	return numPairs;
	}

NX_INLINE NxShape * NxContactStreamIterator::getShape(NxU32 shapeIndex)
	{
	NX_ASSERT(shapeIndex<=1);
	return shapes[shapeIndex];
	}

NX_INLINE NxU16 NxContactStreamIterator::getShapeFlags()
	{
	return shapeFlags;
	}

NX_INLINE NxU32 NxContactStreamIterator::getNumPatches()
	{
	return numPatches;
	}

NX_INLINE NxU32 NxContactStreamIterator::getNumPatchesRemaining()
	{
	return numPatchesRemaining;
	}

NX_INLINE const NxVec3 & NxContactStreamIterator::getPatchNormal()
	{
	return *patchNormal;
	}

NX_INLINE NxU32 NxContactStreamIterator::getNumPoints()
	{
	return numPoints;
	}

NX_INLINE NxU32 NxContactStreamIterator::getNumPointsRemaining()
	{
	return numPointsRemaining;
	}

NX_INLINE const NxVec3 & NxContactStreamIterator::getPoint()
	{
	return *point;
	}

NX_INLINE NxReal NxContactStreamIterator::getSeparation()
	{
	return separation;
	}

NX_INLINE NxU32 NxContactStreamIterator::getFeatureIndex0()
	{
	return featureIndex0;
	}
NX_INLINE NxU32 NxContactStreamIterator::getFeatureIndex1()
	{
	return featureIndex1;
	}

NX_INLINE NxU32 NxContactStreamIterator::getExtData()
	{
	return extData;
	}

NX_INLINE bool NxContactStreamIterator::goNextPair()
	{
	while (numPairsRemaining--)
		{
#ifdef NX32
		size_t bin0 = *stream++;
		size_t bin1 = *stream++;
		shapes[0] = (NxShape*)bin0;
		shapes[1] = (NxShape*)bin1;
//		shapes[0] = (NxShape*)*stream++;
//		shapes[1] = (NxShape*)*stream++;
#else
		NxU64 low = (NxU64)*stream++;
		NxU64 high = (NxU64)*stream++;
		NxU64 bits = low|(high<<32);
		shapes[0] = (NxShape*)bits;
		low = (NxU64)*stream++;
		high = (NxU64)*stream++;
		bits = low|(high<<32);
		shapes[1] = (NxShape*)bits;
#endif
		NxU32 t = *stream++;
		numPatchesRemaining = numPatches = t & 0xffff;
		shapeFlags = t >> 16;

		if (!(shapeFlags & NX_SF_IS_INVALID))
			return true;
		}
	return false;
	}

NX_INLINE bool NxContactStreamIterator::goNextPatch()
	{
	if (numPatches--)
		{
		patchNormal = reinterpret_cast<const NxVec3 *>(stream);
		stream += 3;
		numPointsRemaining = numPoints = *stream++;
		return true;
		}
	else
		return false;
	}

NX_INLINE bool NxContactStreamIterator::goNextPoint()
	{
	if (numPointsRemaining--)
		{
		// Get contact point
		point = reinterpret_cast<const NxVec3 *>(stream);
		stream += 3;

		// Get separation
		NxU32 binary = *stream++;
		NxU32 is32bits = binary & NX_SIGN_BITMASK;
		binary |= NX_SIGN_BITMASK;	// PT: separation is always negative, but the sign bit is used
									// for other purposes in the stream.
		separation = NX_FR(binary);

		// Get extra data
		if (shapeFlags & NX_SF_HAS_MATS_PER_POINT)
			extData = *stream++;
		else
			extData = 0xffffffff;	//means that there is no ext data.

		if (shapeFlags & NX_SF_HAS_FEATURES_PER_POINT)
			{
			if(is32bits)
				{
				featureIndex0 = *stream++;
				featureIndex1 = *stream++;
				}
			else
				{
				featureIndex0 = *stream++;
				featureIndex1 = featureIndex0>>16;
				featureIndex0 &= 0xffff;
				}
			}
		else
			{
			featureIndex0 = 0xffffffff;
			featureIndex1 = 0xffffffff;
			}

		//bind = *stream++;
		//materialIDs[0] = bind & 0xffff;
		//materialIDs[1] = bind >> 16;

		return true;
		}
	else
		return false;
	}


#endif
