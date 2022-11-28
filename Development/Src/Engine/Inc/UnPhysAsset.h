/*=============================================================================
	UnPhysAsset.h: Physics Asset C++ declarations
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
	* Created by James Golding
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
};

#define		RB_MinSizeToLockDOF				(0.1)
#define		RB_MinAngleToLockDOF			(5.0)


class UPhysicsAsset : public UObject
{
	DECLARE_CLASS(UPhysicsAsset,UObject,0,Engine);

	class USkeletalMesh*							DefaultSkelMesh;

	TArray<class URB_BodySetup*>					BodySetup;
	TArray<class URB_ConstraintSetup*>				ConstraintSetup;

	class UPhysicsAssetInstance*					DefaultInstance;

	// UObject interface
	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );

	// Creates a Physics Asset using the supplied Skeletal Mesh as a starting point.
	UBOOL CreateFromSkeletalMesh( class USkeletalMesh* skelMesh, FPhysAssetCreateParams& Params );
	static void CreateCollisionFromBone( URB_BodySetup* bs, class USkeletalMesh* skelMesh, INT BoneIndex, FPhysAssetCreateParams& Params, TArray<struct FBoneVertInfo>& Infos );

	INT						FindControllingBodyIndex(class USkeletalMesh* skelMesh, INT BoneIndex);

	INT						FindBodyIndex(FName bodyName);
	INT						FindConstraintIndex(FName constraintName);

	FBox					CalcAABB(class USkeletalMeshComponent* SkelComp);
	UBOOL					LineCheck(FCheckResult& Result, class USkeletalMeshComponent* SkelComp, const FVector& Start, const FVector& End, const FVector& Extent);
	void					UpdateMassProps();

	// For PhAT only really...
	INT CreateNewBody(FName bodyName);
	void DestroyBody(INT bodyIndex);

	INT CreateNewConstraint(FName constraintName, URB_ConstraintSetup* constraintSetup = NULL);
	void DestroyConstraint(INT constraintIndex);

	void BodyFindConstraints(INT bodyIndex, TArray<INT>& constraints);
	void ClearShapeCaches();

	void UpdateBodyIndices();

	void WeldBodies(INT BaseBodyIndex, INT AddBodyIndex, USkeletalMeshComponent* SkelComp);

	void DrawCollision(struct FPrimitiveRenderInterface* PRI, class USkeletalMeshComponent* SkelComp, FLOAT Scale);
	void DrawConstraints(struct FPrimitiveRenderInterface* PRI, class USkeletalMeshComponent* SkelComp, FLOAT Scale);

	void FixOuters();
};

class UPhysicsAssetInstance : public UObject
{
	DECLARE_CLASS(UPhysicsAssetInstance,UObject,0,Engine);

	AActor*											Owner;

	INT												RootBodyIndex;

	TArrayNoInit<class URB_BodyInstance*>			Bodies;
	TArrayNoInit<class URB_ConstraintInstance*>		Constraints;

	TMap<QWORD, UBOOL>								CollisionDisableTable;

	// UObject interface
	virtual void			Serialize(FArchive& Ar);

	// UPhysicsAssetInstance interface
	void					InitInstance(class USkeletalMeshComponent* SkelComp, class UPhysicsAsset* PhysAsset);
	void					TermInstance();

	void					DisableCollision(class URB_BodyInstance* BodyA, class URB_BodyInstance* BodyB);
	void					EnableCollision(class URB_BodyInstance* BodyA, class URB_BodyInstance* BodyB);
};

class URB_BodySetup : public UKMeshProps
{
	DECLARE_CLASS(URB_BodySetup,UKMeshProps,0,Engine);

	FName BoneName;
	BITFIELD bFixed:1 GCC_PACK(4);
	BITFIELD bNoCollision:1;
	BITFIELD bBlockZeroExtent:1;
	BITFIELD bBlockNonZeroExtent:1;

	class UClass* PhysicalMaterial GCC_PACK(4);
	FLOAT MassScale;

	FVector ApparentVelocity;	

	// Scaled shape cache. Not serialised.
	TArray<void*>			CollisionGeom;
	TArray<FVector>			CollisionGeomScale3D;

	// UObject interface.
	virtual void Destroy();

	// URB_BodySetup interface.
	void	CopyBodyPropertiesFrom(class URB_BodySetup* fromSetup);
	void	ClearShapeCache();
};