/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
 
class ProcBuilding extends Volume
	native(ProcBuilding)
	placeable;

/** If the normal Z component is greater than this, its a roof */
const ROOF_MINZ = 0.7; 

/** Global building version. Increase this to force a re-gen of building meshes. */
const PROCBUILDING_VERSION = 1;

/** Struct that defines a 2D 'scope' - region of a building face */
struct native PBScope2D
{
	/** Transform (in actor space) of the bottom-left corner of scope */
	var Matrix      ScopeFrame;
	/** Size of scope along its X axis */
	var float       DimX;
	/** Size of scope along its Z axis */
	var float       DimZ;	
	
	structcpptext
	{
		/** Draws this scope using the World line batcher */
		void DrawScope(const FColor& DrawColor, const FMatrix& BuildingToWorld, UBOOL bPersistant);
		
		/** Offset the origin of this scope in its local reference frame. */
		void OffsetLocal(const FVector& LocalOffset);

		/** Returns locaiton of the middle point of this scope */
		FVector GetCenter();
	}
};

/** Additional information about each scope of the building */
struct native PBScopeProcessInfo
{
	/** Building (could be 'child' building) that generated this scope. */
	var	ProcBuilding			OwningBuilding;
	/** Which building ruleset is applied to this scope */
	var	ProcBuildingRuleset		Ruleset;
	/** Name of the ruleset variation desired on this scope */
	var name					RulesetVariation;
	/** Whether we want to generate a RTT poly for this scope in the low LOD building */
	var bool					bGenerateLODPoly;
	/** If this scope is within non-rectangular polygon. */
	var bool					bPartOfNonRect;

	structcpptext
	{
		/** Initializing constructor */
		FPBScopeProcessInfo(INT Init)
		{
			OwningBuilding = NULL;
			Ruleset = NULL;
			RulesetVariation = NAME_None;
			bGenerateLODPoly = TRUE;
			bPartOfNonRect = FALSE;
		}
	}
};

/** Struct that contains information about the UVs of one face in the low detail mesh */
struct native PBFaceUVInfo
{
	/** Offset into the texture page */
	var vector2D    Offset;
	
	/** Size of the face's region in the texture page */
	var vector2D    Size;
};

/** Enum used for indicating a particular edge of a scope */
enum EScopeEdge
{
	EPSA_Top,
	EPSA_Bottom,
	EPSA_Left,
	EPSA_Right,
	EPSA_None
};

/** Struct that contains info about an edge between two scopes. */
struct native PBEdgeInfo
{
	/** End point (in building space) of this edge */
	var Vector      EdgeEnd;
	/** Start point (in building space) of this edge */
	var Vector      EdgeStart;

	/** Index of first scope that meets at this edge, in the ToplevelScopes array */
	var int         ScopeAIndex;
	/** What edge of ScopeA this edge forms */
	var EScopeEdge  ScopeAEdge;

	/** Index of second scope that meets at this edge, in the ToplevelScopes array */
	var int         ScopeBIndex;
	/** What edge of ScopeB this edge forms */
	var EScopeEdge  ScopeBEdge;

	/** Angle at this edge, in degrees. 0 means flat, positive is convex (outside) corner, negative is interior */
	var float       EdgeAngle;
};

/** Enum for choosing how to adjust roof/floor poly to fit with corner meshes */
enum EPBCornerType
{
	EPBC_Default,
	EPBC_Chamfer,
	EPBC_Round
};

/** Pointer to ruleset in package used to build facade geometry for building */
var()   editoronly ProcBuildingRuleset				Ruleset;

/** Struct that gives information about each component making up the facades of a building */
struct native PBMeshCompInfo
{
	/** Mesh instance used to make up facade */
	var StaticMeshComponent MeshComp;

	/** Index into TopLevelScopes of scope that this mesh makes up part of */
	var int TopLevelScopeIndex;
};

/** Array of information about each component making up the final building */
var()   const editconst array<PBMeshCompInfo>		BuildingMeshCompInfos;

struct native PBFracMeshCompInfo
{
	/** Fractured mesh instance used to make up facade */
	var FracturedStaticMeshComponent FracMeshComp;

	/** Index into TopLevelScopes of scope that this mesh makes up part of */
	var int TopLevelScopeIndex;
};

/** Array of information about each fractured mesh making up the final building */
var()   const editconst array<PBFracMeshCompInfo>	BuildingFracMeshCompInfos;


/** Component used to display simple one-mesh version of building */
var()   const editconst StaticMeshComponent     SimpleMeshComp;

/** If TRUE, generate a poly to fill the hole on the top of the building */
var()   bool                                    bGenerateRoofMesh;

/** If TRUE, generate a poly to fill the hole on the bottom of the building volume */
var()   bool                                    bGenerateFloorMesh;

/** If TRUE, meshing rules are applied to roof of building, instead of just leaving it as a flat poly */
var()	bool									bApplyRulesToRoof;
/** If TRUE, meshing rules are applied to floor of building, instead of just leaving it as a flat poly */
var()	bool									bApplyRulesToFloor;

/** If TRUE, wall scopes will be split at each roof/floor level in the building group. */
var()	bool									bSplitWallsAtRoofLevels;

/** If TRUE, wall scopes will be split when another wall ends in the middle of a face. */
var()	bool									bSplitWallsAtWallEdges;

/** Components that are used for intermediate LOD, which should be hidden when generating render-to-texture */
var     const array<StaticMeshComponent>        LODMeshComps;

/** UV Information about quads used for intermediate LOD - each element corresponds to element in LODMeshComps */
var     editoronly array<PBFaceUVInfo>          LODMeshUVInfos;

/** List of the top level rectangular scopes building */
var		editoronly array<PBScope2D>				TopLevelScopes;
	
/** This is the divider between TopLevelScopes that are used for meshing, and those used as bounds not non-rect polys for generating texture. */
var     int                                     NumMeshedTopLevelScopes;

/** List of UV info for each top level scope, should match size of TopLevelScopes. */
var     editoronly array<PBFaceUVInfo>          TopLevelScopeUVInfos;

/** Array of rulesets applied to each TopLevelScope of building */
var     editoronly array<PBScopeProcessInfo>	TopLevelScopeInfos;

/** Set of all edges between scopes, indicating which scopes the edge connects, as well as angle and location */
var     editoronly array<PBEdgeInfo>            EdgeInfos;

/** Top-most z value of facade scopes */
var     float                                   MaxFacadeZ;
/** Bottom-most z value of facade scopes */
var     float                                   MinFacadeZ;

/** Temporarty set of buildings that overlap this building. */
var     transient array<ProcBuilding>           OverlappingBuildings;

/** If TRUE, this actor has been edited in 'quick' mode, and needs regen-ing when quick mode exits. */
var     transient bool                          bQuickEdited;

/** Distance at which MassiveLOD will kick in and change between high detail meshes and the SimpleMeshComp / LowLODPersistentActor */
var()   float                                   SimpleMeshMassiveLODDistance;

/** Amount to pull back from the face to render from (caging depth). Nearby meshes closer than this will be rendered into the buildings RTT. */
var()	float									RenderToTexturePullBackAmount;

/** Light map resolution used for generated roof plane mesh */
var()   int                                     RoofLightmapRes;

/** Light map resolution used for generated non-rectangular wall meshes */
var()   int                                     NonRectWallLightmapRes;

/** Amount to scale the resolution of LOD color and lighting textures, generated using render-to-texture */
var()   editoronly float						LODRenderToTextureScale <UIMin=0.25 | UIMax=4.0>;

/** Struct used to store information for building-wide material instances */
struct native PBMaterialParam
{
	/** Name of parameter to set in all building MICs */
	var()   name            ParamName;
	/** Value to set parameter to in all building MICs */
	var()   LinearColor     Color;
};

/** Name of the parameter 'swatch' (stored in the Ruleset) that is applied to this building */
var()   name                                    ParamSwatchName;

/** Optional parameters than are set on all MICs applied to building. */
var()   array<PBMaterialParam>                  BuildingMaterialParams;

/** Array of MICs created to set BuildingMaterialParams on meshes in this building. */
var		editoronly array<MaterialInstanceConstant>				BuildingMatParamMICs;

/** Since we want the low lod mesh of the building to be always loaded, we need an actor in the P map (or other always loaded level); this is that actor */
var()	const editconst duplicatetransient crosslevelpassive StaticMeshActor		LowLODPersistentActor;

/** This is the low detail component, either owned by this actor or in another level (transient as it's only really valid while updating building) */
var		transient StaticMeshComponent			CurrentSimpleMeshComp;

/** This is the actor that owns the simple mesh component (either the building itself, or the LowLODPersistentActor) (transient as it's only valid while updating building) */
var		transient Actor							CurrentSimpleMeshActor;

/** Set of buildings which are directly attached to this one (using Base pointer) */
var     editoronly array<ProcBuilding>          AttachedBuildings;

/** Controls if the simple brush has collision. */
var()   bool                                    bBuildingBrushCollision;

/** Current version of building - set to PROCBUILDING_VERSION when building meshed. */
var		const int								BuildingInstanceVersion;

// EDITOR

/** If TRUE, show face->edge relationships when this building is selected. */
var(Debug)	bool								bDebugDrawEdgeInfo;

/** If TRUE, show scopes extracted from brushes. */
var(Debug)	bool								bDebugDrawScopes;

/** Enumeration of stats for columns in Building Stats Browser. */
enum EBuildingStatsBrowserColumns
{
	BSBC_Name,
	BSBC_Ruleset,
	BSBC_NumStaticMeshComps,
	BSBC_NumInstancedStaticMeshComps,
	BSBC_NumInstancedTris,
	BSBC_LightmapMemBytes,
	BSBC_ShadowmapMemBytes,
	BSBC_LODDiffuseMemBytes,
	BSBC_LODLightingMemBytes
};

struct native PBMemUsageInfo
{
	var		ProcBuilding		Building;
	var		ProcBuildingRuleset	Ruleset;
	var		int		NumStaticMeshComponent;
	var		int		NumInstancedStaticMeshComponents;
	var		int		NumInstancedTris;
	var		int		LightmapMemBytes;
	var		int		ShadowmapMemBytes;
	var		int		LODDiffuseMemBytes;
	var		int		LODLightingMemBytes;

	structcpptext
	{
		FPBMemUsageInfo(INT Init)
		{
			appMemzero(this, sizeof(FPBMemUsageInfo));
		}

		/** Add the supplied info to this one */
		void AddInfo(FPBMemUsageInfo& Info);

		/** Return comma-separated string indicating names of each category */
		static FString GetHeaderString();

		/** Returns a comma-separated string version of the info in this struct. */
		FString GetString();

		/** Get column data  */
		FString GetColumnDataString( INT Column ) const;

		/** */
		INT GetColumnData( INT Column) const;

		/** */
		INT Compare( const FPBMemUsageInfo& Other, INT SortIndex ) const;
	}
};

cpptext
{
	virtual void PostLoad();	
	virtual void PostCrossLevelFixup();
	virtual void SetZone( UBOOL bTest, UBOOL bForceRefresh );

	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostEditMove(UBOOL bFinished);
	virtual void PostEditImport();
	
	virtual void PostScriptDestroyed();

	/** Called after using geom mode to edit thie brush's geometry */
	virtual void PostEditBrush();
	
	virtual void ClearCrossLevelReferences();

	virtual void SetBase(AActor *NewBase, FVector NewFloor = FVector(0,0,1), INT bNotifyActor=1, USkeletalMeshComponent* SkelComp=NULL, FName BoneName=NAME_None );

	/** Find other buildings that are grouped on, or overlap, this one */
	void FindOverlappingBuildings(TArray<AProcBuilding*>& OutOverlappingBuildings);
	
	/** Update TopLevelScopes/TopLevelScopeInfos arrays based on brush, and all buildings based on this one. Also outputs any polygons needed to fill in 'holes', in actor space. */
	void UpdateTopLevelScopes(TArray<AProcBuilding*> GroupBuildings, TArray<FPoly>& OutHighDetailPolys, TArray<FPoly>& OutLowDetailPolys);
	
	/** Update the internal EdgeInfos array, using the ToplevelScopes array. Only scope-scope edges currently. */
	void UpdateEdgeInfos();

	/** Before we save, if the LOQ quad material points to another package, we NULL it out */
	void ClearLODQuadMaterial();

	/**
	 * After saving (when it was NULLed) or loading (after NULL was loaded), we reset any
	 * LOD Quad materials that should be pointing to another level, we reset it to the 
	 * RTT mateial on the low LOD mesh
	 */
	void ResetLODQuadMaterial();

	/** In PreSave, the LODQuad material pointers are NULLed out, this will fix them up again */
	static void FixupProcBuildingLODQuadsAfterSave();
	
	/** Returns TRUE if this building would like to set some additional params on MICs applied to it */
	UBOOL HasBuildingParamsForMIC();
	
	/** Set any building-wide optional MIC params on the supplied MIC. */
	void SetBuildingMaterialParamsOnMIC(UMaterialInstanceConstant* InMIC);

	/** 
	 *  Util for finding getting an MIC with the supplied parent, and parameters set correctly for this building. 
	 *  Will either return one from the cache, or create a new one and set params if not found.
	 */
	UMaterialInstanceConstant* GetBuildingParamMIC(AProcBuilding* ScopeBuilding, UMaterialInterface* ParentMat);

	/** Get the ruleset used for this building volume. Will look at this override, and then base building if none set. */
	UProcBuildingRuleset* GetRuleset();

	/** Update brush color, to indicate whether this is the 'base' building of a group */
	void UpdateBuildingBrushColor();

	// EDITOR

	/** Draw face->edge information in 3D viewport */
	void DrawDebugEdgeInfo(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);

	/** Draw scopes in 3D viewport */
	void DrawDebugScopes(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI);

	/** Get information about the amount of memory used by this building for various things */
	FPBMemUsageInfo GetBuildingMemUsageInfo();
}

/** Remove all the building meshes from this building */
native function ClearBuildingMeshes();

/** Util for finding all building components that form one top level scope. */
native function array<StaticMeshComponent> FindComponentsForTopLevelScope(INT TopLevelScopeIndex);

/** Walks up Base chain to find the root building of the attachment chain */
native function ProcBuilding GetBaseMostBuilding();

/** Get the set of all ProcBuildings (including this one) that are grouped together (using Base pointer) */
native function GetAllGroupedProcBuildings(out array<ProcBuilding> OutSet);

/** Will break pieces off the specified fracture component that are within the specified box. */
native function BreakFractureComponent(FracturedStaticMeshComponent Comp, vector BoxMin, vector BoxMax);

/** 
 * Given an index of a scope in the TopLevelsScopes array (and which edge of that scope), returns index into EdgeInfos with that edge's info.
 * Value of INDEX_NONE may be returned, indicating edge could not be found, which may indicate this is a scope-poly edge instead of scope-scope.
 */
native function int FindEdgeForTopLevelScope(int TopLevelScopeIndex, EScopeEdge Edge);



defaultproperties
{
	Begin Object Name=BrushComponent0
		CollideActors=true
		BlockActors=true
		BlockZeroExtent=false
		BlockNonZeroExtent=true
		BlockRigidBody=true
		RBChannel=RBCC_BlockingVolume
		bDisableAllRigidBody=false
	End Object

	bColored=TRUE
	BrushColor=(R=222,G=255,B=135,A=255)
	
	bForceOctreeSNFilter=TRUE

	bHidden=False
	bEdShouldSnap=true
	bStatic=TRUE
	bNoDelete=TRUE
	bMovable=false
	bCollideActors=true
	bBlockActors=true
	bWorldGeometry=true
	bGameRelevant=true
	bRouteBeginPlayEvenIfStatic=false
	bCollideWhenPlacing=false		
	bPathColliding=TRUE

	bGenerateRoofMesh=TRUE
	bSplitWallsAtRoofLevels=TRUE
	bSplitWallsAtWallEdges=TRUE

	bBuildingBrushCollision=TRUE
	
	SimpleMeshMassiveLODDistance=10000.0
	
	RenderToTexturePullBackAmount=125
	LODRenderToTextureScale=1.0
	
	RoofLightmapRes=64
	NonRectWallLightmapRes=64
}