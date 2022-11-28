class SkeletalMesh extends Object
	native
	noexport
	hidecategories(Object);

var		const native	BoxSphereBounds			Bounds;

var()	const native	array<MaterialInstance>	Materials;
var()	const native	vector					Origin;
var()	const native	rotator					RotOrigin;

var		const native	array<int>				RefSkeleton;	// FMeshBone
var		const native	int						SkeletalDepth;

var		const native	array<int>				LODModels;		// FStaticLODModel
var		const native	array<matrix>			RefBasesInvMatrix;