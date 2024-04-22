
/**
 * Contains the shared data that is used by all SkeletalMeshComponents (instances).
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class SkeletalMesh extends Object
	native(SkeletalMesh)
	noexport
	dependson(ApexClothingAsset)
	hidecategories(Object);

var()	const native			BoxSphereBounds			Bounds;

/** List of materials applied to this mesh. */
var()	const native			array<MaterialInterface>	Materials;
/** List of clothing assets associated with each material int this mesh. */
var()	editfixedsize			array<ApexClothingAsset>    ClothingAssets;

struct native ApexClothingLodInfo
{
	/** The mesh section that the clothing submesh will override. */
	var()	editfixedsize	array<int>	ClothingSectionInfo;
};

struct native ApexClothingAssetInfo
{
	/** Graphical Lod Info for the clothing asset */
	var()	editfixedsize	array<ApexClothingLodInfo> ClothingLodInfo;
	/** Clothing Asset Name */
	var		Name	ClothingAssetName;
};

/** List of Clothing LOD and the mapping of clothing to skeletal mesh section */
var()	editfixedsize			array<ApexClothingAssetInfo>	ClothingLodMap;
/** Origin in original coordinate system */
var()	const native			vector					Origin;
/** Amount to rotate when importing (mostly for yawing) */
var()	const native			rotator					RotOrigin;

var		const native			array<int>				RefSkeleton;	// FMeshBone
var		const native			int						SkeletalDepth;
var		const native			map{FName,INT}			NameIndexMap;

var		const native private	IndirectArray_Mirror	LODModels;		// FStaticLODModel
var		const native private	pointer					SourceData;		// FSkeletalMeshSourceData
var		const native			array<AnimNode.BoneAtom>			RefBasesInvMatrix;

struct native BoneMirrorInfo
{
	/** The bone to mirror. */
	var()	int		SourceIndex <ArrayClamp=RefSkeleton>;
	/** Axis the bone is mirrored across. */
	var()	EAxis	BoneFlipAxis;
};

/** Structure to export/import bone mirroring information */
struct native BoneMirrorExport
{
	var()	Name	BoneName;
	var()	Name	SourceBoneName;
	var()	EAxis	BoneFlipAxis;
};

/** List of bones that should be mirrored. */
var()	editfixedsize	array<BoneMirrorInfo>	SkelMirrorTable;
var()	EAxis									SkelMirrorAxis;
var()	EAxis									SkelMirrorFlipAxis;

var		array<SkeletalMeshSocket>		Sockets;

/** Array of bone names that break for use in game/editor */
var()	editconst const native array<string>		BoneBreakNames;

enum BoneBreakOption
{
	BONEBREAK_SoftPreferred, 
	BONEBREAK_AutoDetect,
	BONEBREAK_RigidPreferred
};

/** Array of options that break bones for use in game/editor */
/** Match with BoneBreakNames array **/
var()	const native array<BoneBreakOption>		BoneBreakOptions;

enum TriangleSortOption
{
	TRISORT_None,						//0
	TRISORT_CenterRadialDistance,		//1
	TRISORT_Random,						//2
	TRISORT_MergeContiguous,			//3
	TRISORT_Custom,						//4
	TRISORT_CustomLeftRight,			//5
};

/** Enum indicating which method to use to generate per-vertex cloth vert movement scale (ClothMovementScale) */
enum ClothMovementScaleGen
{
	ECMDM_DistToFixedVert,
	ECMDM_VertexBoneWeight,
	ECMDM_Empty,
};

/** Enum to specify which axis to use for the forward vector when using TRISORT_CustomLeftRight sort mode */
enum TriangleSortAxis
{
	TSA_X_Axis,
	TSA_Y_Axis,
	TSA_Z_Axis,
};

/** Struct containing triangle sort settings for a particular section */
struct TriangleSortSettings
{
	var() TriangleSortOption TriangleSorting;
	var() TriangleSortAxis CustomLeftRightAxis;
	var() name CustomLeftRightBoneName;
};

/** Struct containing information for a particular LOD level, such as materials and info for when to use it. */
struct native SkeletalMeshLODInfo
{
	/**	Indicates when to use this LOD. A smaller number means use this LOD when further away. */
	var()	float						DisplayFactor;
	/**	Used to avoid 'flickering' when on LOD boundary. Only taken into account when moving from complex->simple. */
	var()	float						LODHysteresis;
	/** Mapping table from this LOD's materials to the SkeletalMesh materials array. */
	var()	editfixedsize array<INT>	LODMaterialMap;
	/** Per-section control over whether to enable shadow casting. */
	var()	editfixedsize array<bool>	bEnableShadowCasting;
	/** Per-section sorting options */
	var		deprecated array<TriangleSortOption>	TriangleSorting;
	var()	editfixedsize array<TriangleSortSettings> TriangleSortSettings;
	/** Use compressed position XYZs(4 bytes saving 8 bytes). This is only useful for GPU skinning.*/
	var()	bool						bDisableCompressions;
	/** Whether to disable morph targets for this LOD. */
	var		bool						bHasBeenSimplified;
};

/** Struct containing information for each LOD level, such as materials to use, whether to cast shadows, and when use the LOD. */
var()	editfixedsize array<SkeletalMeshLODInfo>	LODInfo;

/** Enum specifying the importance of properties when simplifying skeletal meshes. */
enum SkeletalMeshOptimizationImportance
{
	SMOI_Off,
	SMOI_Lowest,
	SMOI_Low,
	SMOI_Normal,
	SMOI_High,
	SMOI_Highest
};

/** Enum specifying the reduction type to use when simplifying skeletal meshes. */
enum SkeletalMeshOptimizationType
{
	SMOT_NumOfTriangles,
	SMOT_MaxDeviation,
	SMOT_MAX,
};

/** Enum specifying how to generate normals when simplifying skeletal meshes. */
enum SkeletalMeshOptimizationNormalMode
{
	SMONM_Recalculate,
	SMONM_RecalculateSoft,
	SMONM_RecalculateHard
};

/**
 * FSkeletalMeshOptimizationSettings - The settings used to optimize a skeletal mesh LOD.
 */
struct native SkeletalMeshOptimizationSettings
{

	/**If ReductionMethod equals SMOT_MaxDeviation this value is the maximum deviation from the base mesh as a percentage of the bounding sphere. */
	var float MaxDeviationPercentage;
	/** How important the shape of the geometry is. */
	var SkeletalMeshOptimizationImportance SilhouetteImportance;
	/** How important texture density is. */
	var SkeletalMeshOptimizationImportance TextureImportance;
	/** How important shading quality is. */
	var SkeletalMeshOptimizationImportance ShadingImportance;
	/** How important skinning quality is. */
	var SkeletalMeshOptimizationImportance SkinningImportance;
	/** How to compute normals for the optimized mesh. */
	var deprecated SkeletalMeshOptimizationNormalMode NormalMode;
	/** The ratio of bones that will be removed from the mesh */
	var float BoneReductionRatio;
	/** Maximum number of bones that can be assigned to each vertex. */
	var int MaxBonesPerVertex;
	/** The method to use when optimizing the skeletal mesh LOD */
	var SkeletalMeshOptimizationType ReductionMethod;
	/** If ReductionMethod equals SMOT_NumOfTriangles this value is the ratio of triangles [0-1] to remove from the mesh */
	var float NumOfTrianglesPercentage;
	/** The welding threshold distance. Vertices under this distance will be welded. */
	var float WeldingThreshold; 
	/** Whether Normal smoothing groups should be preserved. If false then NormalsThreshold is used **/
	var bool bRecalcNormals;
	/** If the angle between two triangles are above this value, the normals will not be
	smooth over the edge between those two triangles. Set in degrees. This is only used when PreserveNormals is set to false*/
	var float NormalsThreshold;
};

/** Optimization settings used to simplify LODs of this mesh. */
var		array<SkeletalMeshOptimizationSettings> OptimizationSettings;

/** For each bone specified here, all triangles rigidly weighted to that bone are entered into a kDOP, allowing per-poly collision checks. */
var()	array<name>	PerPolyCollisionBones;

/** For each of these bones, find the parent that is in PerPolyCollisionBones and add its polys to that bone. */
var()	array<name>	AddToParentPerPolyCollisionBone;

/**
 *	KDOP tree's used for storing rigid triangle information for a subset of bones.
 *	Length of this array matches PerPolyCollisionBones
 */
var		private const native array<int> PerPolyBoneKDOPs;

/** If true, include triangles that are soft weighted to bones. */
var()	bool		bPerPolyUseSoftWeighting;

/** If true, use PhysicsAsset for line collision checks. If false, use per-poly bone collision (if present). */
var()	bool		bUseSimpleLineCollision;

/** If true, use PhysicsAsset for extent (swept box) collision checks. If false, use per-poly bone collision (if present). */
var()	bool		bUseSimpleBoxCollision;

/** All meshes default to GPU skinning. Set to True to enable CPU skinning. If CPU skinning is enabled, bUsePackedPosition can't be enabled */
var()	const bool	bForceCPUSkinning;

/** If true, use 32 bit UVs. If false, use 16 bit UVs to save memory */
var()	const bool	bUseFullPrecisionUVs;

/** TRUE if this mesh has ever been simplified with Simplygon. */
var		bool		bHasBeenSimplified;

/** The FaceFX asset the skeletal mesh uses for FaceFX operations. */
var() FaceFXAsset FaceFXAsset;

/** Asset used for previewing bounds in AnimSetViewer. Makes setting up LOD distance factors more reliable. */
var()	editoronly PhysicsAsset		BoundsPreviewAsset;

/** Asset used for previewing morph target animations in AnimSetViewer. Only for editor. */
var()	editoronly array<MorphTargetSet>	PreviewMorphSets;

/** LOD bias to use for PC. */
var() int LODBiasPC;
/** LOD bias to use for PS3. */
var() int LODBiasPS3;
/** LOD bias to use for Xbox 360. */
var() int LODBiasXbox360;

/** Path to the resource used to construct this skeletal mesh */
var() const editconst editoronly string	SourceFilePath;

/** Date/Time-stamp of the file from the last import */
var() const editconst editoronly string	SourceFileTimestamp;

/** Cache of ClothMesh objects at different scales. */
var	const native transient array<pointer>	ClothMesh;

/** Scale of each of the ClothMesh objects in cache. This array is same size as ClothMesh. */
var const native transient array<float>		ClothMeshScale;

/** 
 *	Mapping between each vertex in the simulation mesh and the graphics mesh. 
 *	This is ordered so that 'free' vertices are first, and then after NumFreeClothVerts they are 'fixed' to the skinned mesh.
 */
var const array<int>		ClothToGraphicsVertMap;

/** Scaling (per vertex) for how far cloth vert can move from its animated position  */
var const array<float>		ClothMovementScale;

/** Method to use to generate the ClothMovementScale table */
var(Cloth)	ClothMovementScaleGen	ClothMovementScaleGenMode;

/** How far a simulated vertex can move from its animated location */
var(Cloth)	float					ClothToAnimMeshMaxDist;

/** If TRUE, simulated verts are limited to a certain distance from */
var(Cloth)	bool					bLimitClothToAnimMesh;

/**
 * Mapping from index of rendered mesh to index of simulated mesh.
 * This mapping applies before ClothToGraphicsVertMap which can then operate normally
 * The reason for this mapping is to weld several vertices with the same position but different texture coordinates into one
 * simulated vertex which makes it possible to run closed meshes for cloth.
 */
var const array<int>		ClothWeldingMap;

/**
 * This is the highest value stored in ClothWeldingMap
 */
var const int				ClothWeldingDomain;

/**
 * This will hold the indices to the reduced number of cloth vertices used for cooking the NxClothMesh.
 */
var const array<int>		ClothWeldedIndices;

/**
 * Forces the Welding Code to be turned off even if the mesh has doubled vertices
 */
var(ClothAdvanced)	const bool		bForceNoWelding;

/** Point in the simulation cloth vertex array where the free verts finish and we start having 'fixed' verts. */
var const int				NumFreeClothVerts;

/** Index buffer for simulation cloth. */
var const array<int>		ClothIndexBuffer;

/** Vertices with any weight to these bones are considered 'cloth'. */
var(Cloth)	const array<name>		ClothBones;

/** If greater than 1, will generate smaller meshes internally, used to improve simulation time and reduce stretching. */
var(Cloth)	const int				ClothHierarchyLevels;

/** Enable constraints that attempt to minimize curvature or folding of the cloth. */
var(Cloth)	const bool				bEnableClothBendConstraints;

/** Enable damping forces on the cloth. */
var(Cloth)	const bool				bEnableClothDamping;

/** Enable center of mass damping of cloth internal velocities.  */
var(Cloth)	const bool				bUseClothCOMDamping;

/** Controls strength of springs that attempts to keep particles in the cloth together. */
var(Cloth)	const float				ClothStretchStiffness <UIMin=0.0 | UIMax=1.0 | ClampMin=0.0 | ClampMax=1.0>;

/** 
 *	Controls strength of springs that stop the cloth from bending. 
 *	bEnableClothBendConstraints must be true to take affect. 
 */
var(Cloth)	const float				ClothBendStiffness <UIMin=0.0 | UIMax=1.0 | ClampMin=0.0 | ClampMax=1.0>;

/** 
 *	This is multiplied by the size of triangles sharing a point to calculate the points mass.
 *	This cannot be modified after the cloth has been created.
 */
var(Cloth)	const float				ClothDensity;

/** How thick the cloth is considered when doing collision detection. */
var(Cloth)	const float				ClothThickness;

/** 
 *	Controls how much damping force is applied to cloth particles.
 *	bEnableClothDamping must be true to take affect.
 */
var(Cloth)	const float				ClothDamping  <UIMin=0.0 | UIMax=1.0 | ClampMin=0.0 | ClampMax=1.0>;

/** Increasing the number of solver iterations improves how accurately the cloth is simulated, but will also slow down simulation. */
var(Cloth)	const int				ClothIterations;

/** If ClothHierarchyLevels is more than 0, this number controls the number of iterations of the hierarchical solver. */
var(Cloth)	const int				ClothHierarchicalIterations;

/** Controls movement of cloth when in contact with other bodies. */
var(Cloth)	const float				ClothFriction <UIMin=0.0 | UIMax=1.0 | ClampMin=0.0 | ClampMax=1.0>;

/** 
 * Controls the size of the grid cells a cloth is divided into when performing broadphase collision. 
 * The cell size is relative to the AABB of the cloth.
 */
var(ClothAdvanced)	const float				ClothRelativeGridSpacing;

/** Adjusts the internal "air" pressure of the cloth. Only has affect when bEnableClothPressure. */
var(ClothAdvanced)	const float				ClothPressure;

/** Response coefficient for cloth/rb collision */
var(ClothAdvanced)	const float				ClothCollisionResponseCoefficient;

/** How much an attachment to a rigid body influences the cloth */
var(ClothAdvanced)	const float				ClothAttachmentResponseCoefficient;

/** How much extension an attachment can undergo before it tears/breaks */
var(ClothAdvanced)	const float				ClothAttachmentTearFactor;

/**
 * Maximum linear velocity at which cloth can go to sleep.
 * If negative, the global default will be used.
 */
var(ClothAdvanced)	const float				ClothSleepLinearVelocity;

/** If bHardStretchLimit is TRUE, how much stretch is allowed in the cloth. 1.0 is no stretch (but will cause jitter) */
var(Cloth)	const float				HardStretchLimitFactor;

/** 
 *	If TRUE, limit the total amount of stretch that is allowed in the cloth, based on HardStretchLimitFactor. 
 *	Note that bLimitClothToAnimMesh must be TRUE for this to work.
 */
var(Cloth)	const bool				bHardStretchLimit;


/** Enable orthogonal bending resistance to minimize curvature or folding of the cloth. 
 *  This technique uses angular springs instead of distance springs as used in
 *  'bEnableClothBendConstraints'. This mode is slower but independent of stretching resistance.
 */
var(ClothAdvanced)	const bool				bEnableClothOrthoBendConstraints;

/** Enables cloth self collision. */
var(ClothAdvanced)	const bool				bEnableClothSelfCollision;

/** Enables pressure support. Simulates inflated objects like balloons. */
var(ClothAdvanced)	const bool				bEnableClothPressure;

/** Enables two way collision with rigid-bodies. */
var(ClothAdvanced)	const bool				bEnableClothTwoWayCollision;

/** Cloth bone type, used when attaching to the physics asset. */
enum ClothBoneType
{
	CLOTHBONE_Fixed,						//0
	CLOTHBONE_BreakableAttachment,			//1
	CLOTHBONE_TearLine						//2
};

/** Used to specify a set of special cloth bones which are attached to the physics asset */
struct native ClothSpecialBoneInfo
{
	/** The bone name to attach to a cloth vertex */
	var() name BoneName;
	
	/** The type of attachment */
	var() ClothBoneType BoneType;
	
	/** Array used to cache cloth indices which will be attached to this bone, created in BuildClothMapping(),
	 * Note: These are welded indices.
	 */
	var const array<int> AttachedVertexIndices;
};

/** 
 * Vertices with any weight to these bones are considered cloth with special behavoir, currently
 * they are attached to the physics asset with fixed or breakable attachments or tearlines.
 */
var(ClothAdvanced)	const array<ClothSpecialBoneInfo>		ClothSpecialBones; 

/** 
 * Enable cloth line/extent/point checks. 
 * Note: line checks are performed with a raycast against the cloth, but point and swept extent checks are performed against the cloth AABB 
 */
var(Cloth)	const bool				bEnableClothLineChecks;

/**
 *  Whether cloth simulation should be wrapped inside a Rigid Body and only be used upon impact
 */
var(ClothAdvanced)  const bool				bClothMetal;

/** Threshold for when deformation is allowed */
var(ClothAdvanced)	const float				ClothMetalImpulseThreshold;
/** Amount by which colliding objects are brought closer to the cloth */
var(ClothAdvanced)	const float				ClothMetalPenetrationDepth;
/** Maximum deviation of cloth particles from initial position */
var(ClothAdvanced)	const float				ClothMetalMaxDeformationDistance;

/** 
 *  Used to enable cloth tearing. Note, extra vertices/indices must be reserved using ClothTearReserve 
 *  Also cloth tearing is not available when welding is enabled.
 */
var(Cloth)	const bool				bEnableClothTearing;

/** Stretch factor beyond which a cloth edge/vertex will tear. Should be greater than 1. */
var(Cloth)	const float				ClothTearFactor;

/** Number of vertices/indices to set aside to accomodate new triangles created as a result of tearing */
var(Cloth)	const int				ClothTearReserve;

/** Any cloth vertex that exceeds its valid bounds will be deleted if bEnableValidBounds is set. Tune ValidBoundMin and ValidBoundMax if valid bound is enabled.*/
var(Cloth)	bool			bEnableValidBounds;

/** The minimum coordinates triplet of the cloth valid bound */
var(Cloth)	Vector			ValidBoundsMin;

/** The maximum coordinates triplet of the cloth valid bound */
var(Cloth)	Vector			ValidBoundsMax;

/** Map which maps from a set of 3 triangle indices packet in a 64bit to the location in the index buffer,
 *  Used to update indices for torn triangles.
 *  Note: This structure is lazy initialized when a torn cloth mesh is created. (But could be precomputed
 *  in BuildClothMapping() if serialization is handled correctly).
 */
var			const native Map_Mirror ClothTornTriMap {TMap<QWORD,INT>};

struct native SoftBodyTetraLink
{
	var int Index;
	var vector Bary;
};

/** Mapping between each vertex of the simulated soft-body's surface-mesh and the graphics mesh. */ 
var const array<int>											SoftBodySurfaceToGraphicsVertMap;

/** Index buffer of the triangles of the soft-body's surface mesh. Indices refer to entries in SoftBodySurfaceToGraphicsVertMap. */
var const array<int>											SoftBodySurfaceIndices;

/** Base array of tetrahedron vertex positions. Used to generate the scaled versions from. */
var const array<vector>											SoftBodyTetraVertsUnscaled;

/** Index buffer of the tetrahedra of the soft-body's tetra-mesh. Indices refer to the vertices in SoftBodyTetraVertsUnscaled.*/
var const array<int>											SoftBodyTetraIndices;

/** Mapping between each vertex of the surface-mesh and its tetrahedron, with local positions given in barycentric coordinates. */
var const array<SoftBodyTetraLink>								SoftBodyTetraLinks;

/** Cache of pointers to NxSoftBodyMesh objects at different scales. */
var	const native transient array<pointer>						CachedSoftBodyMeshes;

/** Scale of each of the NxSoftBodyMesh objects in cache. This array is same size as CachedSoftBodyMeshes. */
var const native transient array<float>							CachedSoftBodyMeshScales;

/** Vertices with any weight to these bones are considered 'soft-body'. */
var(SoftBody)	const array<name>								SoftBodyBones;

/** Cloth bone type, used when attaching to the physics asset. */
enum SoftBodyBoneType
{
	SOFTBODYBONE_Fixed,						//0
	SOFTBODYBONE_BreakableAttachment,		//1
	SOFTBODYBONE_TwoWayAttachment,			//2
};

/** Used to specify a set of special softbody bones which are attached to the physics asset */
struct native SoftBodySpecialBoneInfo
{
	/** The bone name to attach to a cloth vertex */
	var() name BoneName;
	
	/** The type of attachment */
	var() SoftBodyBoneType BoneType;
	
	/** Array used to cache softbody indices which will be attached to this bone, created in BuildSoftBodyMapping(),
	 * Note: These are welded indices.
	 */
	var const array<int> AttachedVertexIndices;
};

/** 
 * Vertices with any weight to these bones are considered softbody with special behavoir, currently
 * they are attached to the physics asset with fixed or breakable attachments.
 */
var(SoftBody)	const array<SoftBodySpecialBoneInfo>		SoftBodySpecialBones; 

/** Defines how strongly the soft-body resists motion that changes the rest volume. Range (0,1]. */
var(SoftBody)	const float				SoftBodyVolumeStiffness;

/** Defines how strongly the soft-body resists stretching motions. Range (0,1]. */
var(SoftBody)	const float				SoftBodyStretchingStiffness;

/** Density of the soft-body (mass per volume). */
var(SoftBody)	const float				SoftBodyDensity;

/** Size of the soft-body particles used for collision detection. */
var(SoftBody)	const float				SoftBodyParticleRadius;

/** 
 *	Controls how much damping force is applied to soft-body particles.
 *	bEnableSoftBodyDamping must be true to take affect.
 */
var(SoftBody)	const float				SoftBodyDamping;

/** Increasing the number of solver iterations improves how accurately the soft-body is simulated, but will also slow down simulation. */
var(SoftBody)	const int				SoftBodySolverIterations;

/** Controls movement of soft-body when in contact with other bodies. */
var(SoftBody)	const float				SoftBodyFriction;

/** 
 * Controls the size of the grid cells a soft-body is divided into when performing broadphase collision. 
 * The cell size is relative to the AABB of the soft-body.
 */
var(SoftBody)	const float				SoftBodyRelativeGridSpacing;

/**
 * Maximum linear velocity at which a soft-body can go to sleep.
 * If negative, the global default will be used.
 */
var(SoftBody)	const float				SoftBodySleepLinearVelocity;

/** Enables soft-body self collision. */
var(SoftBody)	const bool				bEnableSoftBodySelfCollision;

/** 
 * Defines a factor for the impulse transfer from the soft body to attached rigid bodies. 
 * bEnableSoftBodyTwoWayCollision must be true to take effect.
 */
var(SoftBody)	const float				SoftBodyAttachmentResponse;

/** 
 * Defines a factor for the impulse transfer from the soft body to colliding rigid bodies. 
 * bEnableSoftBodyTwoWayCollision must be true to take effect.
 */
var(SoftBody)	const float				SoftBodyCollisionResponse;

/** 
 * Controls how much the original graphics mesh is simplified before it is used
 * to seed to tetrahedron-mesh generator.
 */
var(SoftBody)	const float				SoftBodyDetailLevel<ClampMin=0.0 | ClampMax=1.0>;

/** Controls how many tetrahedra are generated to approximate the surface-mesh. */
var(SoftBody)	const int				SoftBodySubdivisionLevel<ClampMin=1.0>;

/** If enabled, an iso-surface is generated around the original graphics-mesh before
 * the tetrahedron-mesh is created.
 */
var(SoftBody)	const bool				bSoftBodyIsoSurface;

/** Enable damping forces on the softbody. */
var(SoftBody)	const bool				bEnableSoftBodyDamping;

/** Enable center of mass damping of SoftBody internal velocities.  */
var(SoftBody)	const bool				bUseSoftBodyCOMDamping;

/** Specifies the maximum distance a tetra-vertex is allowed to have from the 
 *  surface-mesh to still end up attached to a bone. 
 */
var(SoftBody)	const float				SoftBodyAttachmentThreshold;

/** Enables two way collision with rigid-bodies. */
var(SoftBody)	const bool				bEnableSoftBodyTwoWayCollision;

/** How much extension an attachment can undergo before it tears/breaks */
var(SoftBody)	const float				SoftBodyAttachmentTearFactor;

/** Enable soft body line checks. */
var(SoftBody)	const bool				bEnableSoftBodyLineChecks;

/** Whether or not the mesh has vertex colors */
var bool bHasVertexColors;

/** Saves if Graphics Vertex is simulated cloth or not */
var const native array<bool>			GraphicsIndexIsCloth;

/** The cached streaming texture factors.  If the array doesn't have MAX_TEXCOORDS entries in it, the cache is outdated. */
var const native array<float>			CachedStreamingTextureFactors;

/**
 * Allows artists to adjust the distance where textures using UV 0 are streamed in/out.
 * 1.0 is the default, whereas a higher value increases the streamed-in resolution.
 */
var() const float						StreamingDistanceMultiplier;

var const native transient int			ReleaseResourcesFence;

/** Runtime UID for this SkeletalMeshm, used when linking meshes to AnimSets. */
var const transient qword				SkelMeshRUID;

/** When enabled the material in the APEX clothing asset will override the skeletal mesh material. */
var(ApexClothing) const bool			bUseClothingAssetMaterial;

defaultproperties
{
	SkelMirrorAxis=AXIS_X
	SkelMirrorFlipAxis=AXIS_Z
	// Cloth params
	ClothThickness = 0.5
	ClothDensity = 1.0
	ClothBendStiffness = 1.0
	ClothStretchStiffness = 1.0
    ClothDamping = 0.5
	ClothFriction = 0.5
    ClothIterations = 5
	ClothHierarchicalIterations = 2

	HardStretchLimitFactor = 1.1

	ClothMovementScaleGenMode=ECMDM_DistToFixedVert

	bUseSimpleLineCollision=true
	bUseSimpleBoxCollision=true
	
	bEnableClothOrthoBendConstraints = FALSE
	bEnableClothSelfCollision = FALSE
	bEnableClothPressure = FALSE
	bEnableClothTwoWayCollision = FALSE
	bForceNoWelding = FALSE
	
	ClothRelativeGridSpacing = 1.0
	ClothPressure = 1.0
	ClothCollisionResponseCoefficient = 0.2
	ClothAttachmentResponseCoefficient = 0.2
	ClothAttachmentTearFactor = 1.5
	ClothSleepLinearVelocity =-1.0

	ClothMetalImpulseThreshold=10.0
	ClothMetalPenetrationDepth=0.0
	ClothMetalMaxDeformationDistance=0.0

	bEnableClothTearing = FALSE
	ClothTearFactor = 3.5
	ClothTearReserve = 128
	
	bEnableValidBounds=FALSE
	ValidBoundsMin=(X=0,Y=0,Z=0)
	ValidBoundsMax=(X=0,Y=0,Z=0)
	
	SoftBodyVolumeStiffness = 1.0;
	SoftBodyStretchingStiffness = 1.0;
	SoftBodyDensity = 1.0;
	SoftBodyParticleRadius = 0.1;
	SoftBodyDamping = 0.5;
	SoftBodySolverIterations = 5;
	SoftBodyFriction = 0.5
	SoftBodyRelativeGridSpacing = 1.0
	SoftBodySleepLinearVelocity = -1.0
	bEnableSoftBodySelfCollision = FALSE
	SoftBodyAttachmentResponse = 0.2;
	SoftBodyCollisionResponse = 0.2;
	
	SoftBodyDetailLevel = 0.5f;
	SoftBodySubdivisionLevel = 4
	bSoftBodyIsoSurface = TRUE
	
	SoftBodyAttachmentThreshold = 0.5;
	bEnableSoftBodyTwoWayCollision = TRUE;

	SoftBodyAttachmentTearFactor = 1.5;

	StreamingDistanceMultiplier = 1.0;
}
