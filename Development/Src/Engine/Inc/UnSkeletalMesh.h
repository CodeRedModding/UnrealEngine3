/*=============================================================================
	UnSkeletalMesh.h: Unreal skeletal mesh objects.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

/*-----------------------------------------------------------------------------
	USkeletalMeshComponent.
-----------------------------------------------------------------------------*/

//
//	FAttachment
//
struct FAttachment
{
	UActorComponent*	Component;
	FName				BoneName;
	FVector				RelativeLocation;
	FRotator			RelativeRotation;
	FVector				RelativeScale;

	// Constructor.

	FAttachment(UActorComponent* InComponent,FName InBoneName,FVector InRelativeLocation,FRotator InRelativeRotation,FVector InRelativeScale):
		Component(InComponent),
		BoneName(InBoneName),
		RelativeLocation(InRelativeLocation),
		RelativeRotation(InRelativeRotation),
		RelativeScale(InRelativeScale)
	{}
	FAttachment() {}
};

//
//	FBoneRotationControl
//
struct FBoneRotationControl
{
	INT			BoneIndex;
	FName		BoneName;
	FRotator	BoneRotation;
	BYTE		BoneSpace;

	// Constructor.
	FBoneRotationControl(INT InBoneIndex, FName InBoneName, FRotator InBoneRotation, BYTE InBoneSpace):
		BoneIndex(InBoneIndex),
		BoneName(InBoneName),
		BoneRotation(InBoneRotation),
		BoneSpace(InBoneSpace)
	{}
	FBoneRotationControl() {}
};

//
//	FBoneTranslationControl
//
struct FBoneTranslationControl
{
	INT			BoneIndex;
	FName		BoneName;
	FVector		BoneTranslation;

	FBoneTranslationControl(INT InBoneIndex,  FName InBoneName, const FVector& InBoneTranslation):
		BoneIndex(InBoneIndex),
		BoneName(InBoneName),
		BoneTranslation(InBoneTranslation)
	{}
	FBoneTranslationControl() {}
};


enum ERootBoneAxis
{
	RBA_Default,
	RBA_Discard,
	RBA_Translate
};

//
//	USkeletalMeshComponent
//
class USkeletalMesh;
class USkeletalMeshComponent : public UMeshComponent
{
	DECLARE_CLASS(USkeletalMeshComponent,UMeshComponent,0,Engine)

	USkeletalMesh*						SkeletalMesh;
	class UAnimTree*					AnimTreeTemplate;
	UAnimNode*							Animations;

	UPhysicsAsset*						PhysicsAsset;
	UPhysicsAssetInstance*				PhysicsAssetInstance;

	FLOAT								PhysicsWeight;

	struct FSkeletalMeshObject*			MeshObject;

	// State of bone bases - set up in local mesh space.
	TArray <FMatrix>					SpaceBases; 

	// Array of AnimSets used to find sequences by name.
	TArrayNoInit<class UAnimSet*>		AnimSets;

	// Attachments.
	TArrayNoInit<FAttachment>				Attachments;
	TArrayNoInit<FBoneRotationControl>		BoneRotationControls;
	TArrayNoInit<FBoneTranslationControl>	BoneTranslationControls;

	// Editor/debugging rendering mode flags.
	INT									ForcedLodModel;		// Force drawing of a specific lodmodel -1    if > 0.
	UBOOL								bForceRefpose;		// Force reference pose.
	UBOOL								bDisplayBones;		// Draw the skeleton heirarchy for this skel mesh.
	UBOOL								bHideSkin;			// Don't bother rendering the skin.
	UBOOL								bForceRawOffset;	// Forces ignoring of the mesh's offset.
	UBOOL								bIgnoreControllers;	// Ignore and bone rotation controllers

	// For rendering physics limits
	UMaterialInstance*					LimitMaterial;
		

	// Root bone options (see SkeletalMeshComponent.uc for details)
	UBOOL								bDiscardRootRotation;
	BYTE								RootBoneOption[3];
	UBOOL								bOldRootInitialized GCC_PACK(PROPERTY_ALIGNMENT);
	FVector								OldRootLocation;

	// Object interface.
	virtual void Serialize( FArchive& Ar );
	virtual void Destroy();

	// USkeletalMeshComponent interface
	void DeleteAnimTree();

	void UpdateSpaceBases();

	void  SetSkeletalMesh(USkeletalMesh* InSkelMesh );
	
	// Search through AnimSets to find an animation with the given name
	class UAnimSequence* FindAnimSequence(FName AnimSeqName);

	FMatrix	GetBoneMatrix( DWORD BoneIdx );
	
	// Controller Interface
	void SetBoneRotation( FName BoneName, const FRotator& BoneRotation, BYTE BoneSpace );
	void SetBoneTranslation( FName BoneName, const FVector& BoneTranslation );

	INT	MatchRefBone( FName StartBoneName);		
	
	INT GetLODLevel(const FSceneContext& Context) const;

	virtual void InitArticulated();
	virtual void TermArticulated();

	void GetPhysicsBoneAtoms( TArray<FBoneAtom>& Atoms );

	void SetAnimTreeTemplate(UAnimTree* NewTemplate);


	// Attachment interface.

	void AttachComponent(UActorComponent* Component,FName BoneName,FVector RelativeLocation = FVector(0,0,0),FRotator RelativeRotation = FRotator(0,0,0),FVector RelativeScale = FVector(1,1,1));
	void DetachComponent(UActorComponent* Component);

	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Update();
	virtual void Destroyed();
	virtual void Tick(FLOAT DeltaTime);

	virtual void Precache(FPrecacheInterface* P);

	virtual UBOOL IsValidComponent() const { return SkeletalMesh != NULL && Super::IsValidComponent(); }

	// FPrimitiveViewInterface interface.

    virtual DWORD GetLayerMask(const FSceneContext& Context) const;
	virtual void Render(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderForeground(const FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderShadowVolume(const FSceneContext& Context,struct FShadowRenderInterface* SRI,ULightComponent* Light);

	// UPrimitiveComponent interface.
	
	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	virtual UBOOL Visible(FSceneView* View);
	virtual void UpdateBounds();

	virtual void AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange=false);
	virtual void WakeRigidBody( FName BoneName = NAME_None );
	virtual void SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent=false);
	virtual void SetBlockRigidBody(UBOOL bNewBlockRigidBody);
	virtual void UpdateWindForces();
	virtual void UpdateRBKinematicData();

#ifdef WITH_NOVODEX
	virtual class NxActor* GetNxActor(FName BoneName = NAME_None);
#endif // WITH_NOVODEX

	// UMeshComponent interface.

	virtual UMaterialInstance* GetMaterial(INT MaterialIndex) const;

	// Script functions.

	DECLARE_FUNCTION(execAttachComponent);
	DECLARE_FUNCTION(execDetachComponent);	
	DECLARE_FUNCTION(execSetSkeletalMesh);
	DECLARE_FUNCTION(execFindAnimSequence);
	DECLARE_FUNCTION(execSetBoneRotation);	
	DECLARE_FUNCTION(execClearBoneRotations);
	DECLARE_FUNCTION(execDrawAnimDebug);
	DECLARE_FUNCTION(execGetBoneQuaternion);
	DECLARE_FUNCTION(execGetBoneLocation);
	DECLARE_FUNCTION(execSetAnimTreeTemplate);
};

/*-----------------------------------------------------------------------------
	USkeletalMesh.
-----------------------------------------------------------------------------*/

struct FMeshWedge
{
	_WORD			iVertex;		// Vertex index.
	FLOAT			U,V;			// UVs.
	friend FArchive &operator<<( FArchive& Ar, FMeshWedge& T )
	{
		Ar << T.iVertex << T.U << T.V;
		return Ar;
	}
};
	
struct FMeshFace
{
	_WORD		iWedge[3];			// Textured Vertex indices.
	_WORD		MeshMaterialIndex;	// Source Material (= texture plus unique flags) index.

	friend FArchive &operator<<( FArchive& Ar, FMeshFace& F )
	{
		Ar << F.iWedge[0] << F.iWedge[1] << F.iWedge[2];
		Ar << F.MeshMaterialIndex;
		return Ar;
	}
};

// A bone: an orientation, and a position, all relative to their parent.
struct VJointPos
{
	FQuat   	Orientation;  //
	FVector		Position;     //  needed or not ?

	FLOAT       Length;       //  For collision testing / debugging drawing...
	FLOAT       XSize;
	FLOAT       YSize;
	FLOAT       ZSize;

	friend FArchive &operator<<( FArchive& Ar, VJointPos& V )
	{
		return Ar << V.Orientation << V.Position << V.Length << V.XSize << V.XSize << V.ZSize;
	}
};

// Reference-skeleton bone, the package-serializable version.
struct FMeshBone
{
	FName 		Name;		  // Bone's name.
	DWORD		Flags;        // reserved
	VJointPos	BonePos;      // reference position
	INT         ParentIndex;  // 0/NULL if this is the root bone.  
	INT 		NumChildren;  // children  // only needed in animation ?
	INT         Depth;        // Number of steps to root in the skeletal hierarcy; root=0.
	friend FArchive &operator<<( FArchive& Ar, FMeshBone& F)
	{
		return Ar << F.Name << F.Flags << F.BonePos << F.NumChildren << F.ParentIndex;
	}
};

// Textured triangle.
struct VTriangle
{
	_WORD   WedgeIndex[3];	 // Point to three vertices in the vertex list.
	BYTE    MatIndex;	     // Materials can be anything.
	BYTE    AuxMatIndex;     // Second material from exporter (unused)
	DWORD   SmoothingGroups; // 32-bit flag for smoothing groups.

	friend FArchive &operator<<( FArchive& Ar, VTriangle& V )
	{
		Ar << V.WedgeIndex[0] << V.WedgeIndex[1] << V.WedgeIndex[2];
		Ar << V.MatIndex << V.AuxMatIndex;
		Ar << V.SmoothingGroups;
		return Ar;
	}

	VTriangle& operator=( const VTriangle& Other)
	{
		this->AuxMatIndex = Other.AuxMatIndex;
		this->MatIndex        =  Other.MatIndex;
		this->SmoothingGroups =  Other.SmoothingGroups;
		this->WedgeIndex[0]   =  Other.WedgeIndex[0];
		this->WedgeIndex[1]   =  Other.WedgeIndex[1];
		this->WedgeIndex[2]   =  Other.WedgeIndex[2];
		return *this;
	}
};

struct FVertInfluence 
{
	FLOAT Weight;
	_WORD VertIndex;
	_WORD BoneIndex;
	friend FArchive &operator<<( FArchive& Ar, FVertInfluence& F )
	{
		return Ar << F.Weight << F.VertIndex << F.BoneIndex;
	}
};

//
//	FSoftSkinVertex
//

struct FSoftSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	FLOAT			U,
					V;
	BYTE			InfluenceBones[4];
	BYTE			InfluenceWeights[4];

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FSoftSkinVertex& V)
	{
		Ar << V.Position;
		Ar << V.TangentX << V.TangentY << V.TangentZ;
		Ar << V.U << V.V;

		for(UINT InfluenceIndex = 0;InfluenceIndex < 4;InfluenceIndex++)
		{
			Ar << V.InfluenceBones[InfluenceIndex];
			Ar << V.InfluenceWeights[InfluenceIndex];
		}

		return Ar;
	}
};

//
//	FRigidSkinVertex
//

struct FRigidSkinVertex
{
	FVector			Position;
	FPackedNormal	TangentX,
					TangentY,
					TangentZ;
	FLOAT			U,
					V;
	BYTE			Bone;

	// Serializer.

	friend FArchive& operator<<(FArchive& Ar,FRigidSkinVertex& V)
	{
		Ar << V.Position;
		Ar << V.TangentX << V.TangentY << V.TangentZ;
		Ar << V.U << V.V << V.Bone;
		return Ar;
	}
};

// Mesh section structure - for both animating and static parts.
struct FSkelMeshSection
{
	_WORD			MaterialIndex,  // Material (texture) used for this section. Usually an actor's multi-skins.
	     			FirstIndex,		// The offset of the first index in the index buffer.
					MinIndex,		// The smallest vertex index used by this section.
					MaxIndex,		// The largest vertex index used by this section.
					TotalVerts,     // The total number of vertices 
					TotalWedges,    //
					MaxInfluences,  // Largest number of per-vertex bone influences
					ShaderIndex,    // This section might need a custom shader.
					FirstFace,      // First face for this section.
					TotalFaces;     // Faces - at full LOD - for this section
	TArray<_WORD>	ActiveBoneIndices;

	// Serialization.
	friend FArchive& operator<<(FArchive& Ar,FSkelMeshSection& S)
	{		
		Ar		<< S.MaterialIndex
				<< S.FirstIndex
				<< S.MinIndex
				<< S.MaxIndex
				<< S.TotalVerts
				<< S.MaxInfluences
				<< S.ShaderIndex
				<< S.FirstFace
				<< S.TotalFaces
				<< S.ActiveBoneIndices;
		return Ar;
	}	
};

//
//	FStaticLODModel - All data to define a certain LOD model for a mesh.
//

class FStaticLODModel
{
public:
	//
	// All necessary data to render smooth-parts is in SkinningStream, SmoothVerts, SmoothSections and SmoothIndexbuffer.
	// For rigid parts: RigidVertexStream, RigidIndexBuffer, and RigidSections.
	//

	// Sections.
	TArray<FSkelMeshSection>	Sections;

	// Bone hierarchy subset active for this chunk.
	TArray<_WORD> ActiveBoneIndices;  
	
	// Rendering data.

	FRawIndexBuffer				IndexBuffer;
	TArray<FMeshEdge>			Edges;
	TArray<FSoftSkinVertex>		SoftVertices;
	TArray<FRigidSkinVertex>	RigidVertices;
	TArray<_WORD>				ShadowIndices;
	TArray<BYTE>				ShadowTriangleDoubleSided;

	// Raw data. Never loaded unless converting / updating.
	TLazyArray<FVertInfluence>	Influences;
	TLazyArray<FMeshWedge>		Wedges;
	TLazyArray<FMeshFace>		Faces;
	TLazyArray<FVector>			Points;	
		
	friend FArchive &operator<<( FArchive& Ar, FStaticLODModel& F )
	{
		Ar << F.Sections;
		Ar << F.IndexBuffer;
		Ar << F.SoftVertices << F.RigidVertices << F.ShadowIndices << F.ActiveBoneIndices << F.ShadowTriangleDoubleSided;
		Ar << F.Edges << F.Influences << F.Wedges << F.Faces << F.Points;
		return Ar;
	}
};

//
//	FBoneVertInfo
//  Contains the vertices that are most dominated by that bone. Vertices are in Bone space.
//  Not used at runtime, but useful for fitting physics assets etc.
//
struct FBoneVertInfo
{
	// Invariant: Arrays should be same length!
	TArray<FVector>	Positions;
	TArray<FVector>	Normals;
};

//
// Skeletal mesh.
//

class USkeletalMesh : public UObject
{
	DECLARE_CLASS(USkeletalMesh, UObject, CLASS_SafeReplace, Engine)

	FBoxSphereBounds			Bounds;

	TArray<UMaterialInstance*>	Materials;	
	
	// Scaling.
	FVector 					Origin;				// Origin in original coordinate system.
	FRotator					RotOrigin;			// Amount to rotate when importing (mostly for yawing).

	// Skeletal specific data
	TArray<FMeshBone>			RefSkeleton;	// Reference skeleton.
	INT							SkeletalDepth;  // The max hierarchy depth.

	// Static LOD models
	TArray<FStaticLODModel>		LODModels;

	// Reference skeleton precomputed bases. @todo: wasteful ?!
	TArray<FMatrix>				RefBasesInvMatrix;

	// Object interface.
	void Serialize( FArchive& Ar );
	virtual void PostLoad();
	
	virtual FString GetDesc();
	virtual void DrawThumbnail( EThumbnailPrimType InPrimType, INT InX, INT InY, struct FChildViewport* InViewport, struct FRenderInterface* InRI, FLOAT InZoom, UBOOL InShowBackground, FLOAT InZoomPct, INT InFixedSz );
	virtual FThumbnailDesc GetThumbnailDesc( FRenderInterface* InRI, FLOAT InZoom, INT InFixedSz );
	virtual INT GetThumbnailLabels( TArray<FString>* InLabels );

	// Setup-only routines - not concerned with the instance.
	
	/**
	 * Create all render specific (but serializable) data like e.g. the 'compiled' rendering stream,
	 * mesh sections and index buffer.
	 *
	 * @todo: currently only handles LOD level 0.
	 */
	void CreateSkinningStreams();
	void CalculateInvRefMatrices();
	void CalcBoneVertInfos( TArray<FBoneVertInfo>& Infos, UBOOL bOnlyDominant );
  
	INT		MatchRefBone( FName StartBoneName);
	UBOOL	BoneIsChildOf( INT ChildBoneIndex, INT ParentBoneIndex );

	FMatrix	GetRefPoseMatrix( INT BoneIndex );
};


/*----------------------------------------------------------------------------
	The End.
----------------------------------------------------------------------------*/


