/*=============================================================================
	UnPhysAsset.h: Physics Asset C++ declarations
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

enum EPhysAssetFitGeomType
{
	EFG_Box,
	EFG_SphylSphere
};

enum EPhysAssetFitVertWeight
{
	EVW_AnyWeight,
	EVW_DominantWeight
};

#define		PHYSASSET_DEFAULT_MINBONESIZE		(1.0f)
#define		PHYSASSET_DEFAULT_GEOMTYPE			(EFG_Box)
#define		PHYSASSET_DEFAULT_ALIGNBONE			(true)
#define		PHYSASSET_DEFAULT_VERTWEIGHT		(EVW_DominantWeight)
#define		PHYSASSET_DEFAULT_MAKEJOINTS		(true)
#define		PHYSASSET_DEFAULT_WALKPASTSMALL		(true)

struct FPhysAssetCreateParams
{
	FLOAT						MinBoneSize;
	EPhysAssetFitGeomType		GeomType;
	EPhysAssetFitVertWeight		VertWeight;
	UBOOL						bAlignDownBone;
	UBOOL						bCreateJoints;
	UBOOL						bWalkPastSmall;
	UBOOL						bBodyForAll;
};

#define		RB_MinSizeToLockDOF				(0.1)
#define		RB_MinAngleToLockDOF			(5.0)

/**
 * Endian save storage for a pair of rigid body indices used as a key in the CollisionDisableTable TMap.
 */
struct FRigidBodyIndexPair
{
	/** Pair of indices */
	INT		Indices[2];
	
	/** Default constructor required for use with TMap */
	FRigidBodyIndexPair()
	{}

	/**
	 * Constructor, taking unordered pair of indices and generating a key.
	 *
	 * @param	Index1	1st unordered index
	 * @param	Index2	2nd unordered index
	 */
	FRigidBodyIndexPair( INT Index1, INT Index2 )
	{
		Indices[0] = Min( Index1, Index2 );
		Indices[1] = Max( Index1, Index2 );
	}

	/**
	 * == operator required for use with TMap
	 *
	 * @param	Other	FRigidBodyIndexPair to compare this one to.
	 * @return	TRUE if the passed in FRigidBodyIndexPair is identical to this one, FALSE otherwise
	 */
	UBOOL operator==( const FRigidBodyIndexPair &Other ) const
	{
		return (Indices[0] == Other.Indices[0]) && (Indices[1] == Other.Indices[1]);
	}

	/**
	 * Serializes the rigid body index pair to the passed in archive.	
	 *
	 * @param	Ar		Archive to serialize data to.
	 * @param	Pair	FRigidBodyIndexPair to serialize
	 * @return	returns the passed in FArchive after using it for serialization
	 */
	friend FArchive& operator<< ( FArchive &Ar, FRigidBodyIndexPair &Pair )
	{
		Ar << Pair.Indices[0] << Pair.Indices[1];
		return Ar;
	}
};

/**
 * Generates a hash value in accordance with the QWORD implementation of GetTypeHash which is
 * required for backward compatibility as older versions of UPhysicsAssetInstance stored
 * a TMap<QWORD,UBOOL>.
 * 
 * @param	Pair	FRigidBodyIndexPair to calculate the hash value for
 * @return	the hash value for the passed in FRigidBodyIndexPair
 */
inline DWORD GetTypeHash( const FRigidBodyIndexPair Pair )
{
	return Pair.Indices[0] + (Pair.Indices[1] * 23);
}
