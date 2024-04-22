
/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class PhysicsAsset extends Object
	hidecategories(Object)
	native(Physics);

/** 
 *	Default skeletal mesh to use when previewing this PhysicsAsset etc. 
 *	Is the one that was used as the basis for creating this Asset.
 */
var		const editoronly SkeletalMesh				DefaultSkelMesh;

/** 
 *	Array of RB_BodySetup objects. Stores information about collision shape etc. for each body.
 *	Does not include body position - those are taken from mesh.
 */
var		const instanced array<RB_BodySetup>			BodySetup;

/**
 * 	This caches the BodySetup Index by BodyName to speed up FindBodyIndex
 */
var 	native const Map_Mirror						BodySetupIndexMap{TMap<FName, INT>};

/** Index of bodies that are marked bConsiderForBounds */
var		const array<int>							BoundsBodies;

/** 
 *	Array of RB_ConstraintSetup objects. 
 *	Stores information about a joint between two bodies, such as position relative to each body, joint limits etc.
 */
var		const instanced array<RB_ConstraintSetup>	ConstraintSetup;

/** Default per-instance paramters for this PhysicsAsset. */
var		const instanced PhysicsAssetInstance		DefaultInstance; 

cpptext
{
	// UObject interface
	virtual void PostLoad();

	/** 
	 * Returns a one line description of an object for viewing in the thumbnail view of the generic browser
	 */
	virtual FString GetDesc();

	/** 
	 * Returns detailed info to populate listview columns
	 */
	virtual FString GetDetailedDescription( INT InIndex );

	// Creates a Physics Asset using the supplied Skeletal Mesh as a starting point.
	UBOOL CreateFromSkeletalMesh( class USkeletalMesh* skelMesh, FPhysAssetCreateParams& Params );
	static void CreateCollisionFromBone( URB_BodySetup* bs, class USkeletalMesh* skelMesh, INT BoneIndex, FPhysAssetCreateParams& Params, TArray<struct FBoneVertInfo>& Infos );

	INT						FindControllingBodyIndex(class USkeletalMesh* skelMesh, INT BoneIndex);
	INT						FindConstraintIndex(FName ConstraintName);
	FName					FindConstraintBoneName(INT ConstraintIndex);

	/** Utility for getting indices of all bodies below (and including) the one with the supplied name. */
	void					GetBodyIndicesBelow(TArray<INT>& OutBodyIndices, FName InBoneName, USkeletalMesh* InSkelMesh);

	FBox					CalcAABB(class USkeletalMeshComponent* SkelComp);
	UBOOL					LineCheck(FCheckResult& Result, class USkeletalMeshComponent* SkelComp, const FVector& Start, const FVector& End, const FVector& Extent, UBOOL bOnlyPerPolyShapes);
	FCheckResult*			LineCheckAllInteractions( FMemStack& Mem, class USkeletalMeshComponent* SkelComp, const FVector& Start, const FVector& End, const FVector& Extent, UBOOL bPerPolyShapes );
	UBOOL					PointCheck(FCheckResult& Result, class USkeletalMeshComponent* SkelComp, const FVector& Location, const FVector& Extent);
	void					UpdateMassProps();

	// For PhAT only really...
	INT CreateNewBody(FName InBodyName);
	void DestroyBody(INT BodyIndex);

	INT CreateNewConstraint(FName InConstraintName, URB_ConstraintSetup* InConstraintSetup = NULL);
	void DestroyConstraint(INT ConstraintIndex);

	void BodyFindConstraints(INT BodyIndex, TArray<INT>& Constraints);
	void ClearShapeCaches();

	void UpdateBodyIndices();

	void WeldBodies(INT BaseBodyIndex, INT AddBodyIndex, USkeletalMeshComponent* SkelComp);

	void DrawCollision(class FPrimitiveDrawInterface* PDI, const USkeletalMesh* SkelMesh, const TArray<FBoneAtom>& SpaceBases, const FMatrix& LocalToWorld, FLOAT Scale);
	void DrawConstraints(class FPrimitiveDrawInterface* PDI, const USkeletalMesh* SkelMesh, const TArray<FBoneAtom>& SpaceBases, const FMatrix& LocalToWorld, FLOAT Scale);

	void FixOuters();

	/** Update the BoundsBodies array and cache the indices of bodies marked with bConsiderForBounds to BoundsBodies array. */
	void UpdateBoundsBodiesArray();

	/** Update the BodySetup Array Index Map.  */
	void UpdateBodySetupIndexMap();
}

native final function INT FindBodyIndex(Name BodyName);
