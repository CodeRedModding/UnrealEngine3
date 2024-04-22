/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

/**
 *	Utility component for drawing an interactive body of fluid.
 *  Origin is at the component location.
 */
class FluidSurfaceComponent extends PrimitiveComponent
	native(Fluid)
	AutoExpandCategories(FluidSurfaceComponent,Fluid,FluidDetail)
	hidecategories(Object)
	dependson(LightmassPrimitiveSettingsObject)
	editinlinenew;

/** Surface material */
var() 	materialinterface	FluidMaterial;

/** Resolution of the fluid's texture lightmap. */
var(Lighting) int			LightmapResolution;

/** The Lightmass settings for this object. */
var(Lightmass) LightmassPrimitiveSettings	LightmassSettings <ScriptOrder=true>;

/** Whether the vertex positions in the simulation grid should be animated or not */
var(Fluid) 	bool			EnableSimulation;

/** Number of quads in the simulated grid (along the X-axis) */
var(Fluid)	int				SimulationQuadsX;

/** Number of quads in the simulated grid (along the Y-axis) */
var(Fluid)	int				SimulationQuadsY;

/** The size of a grid cell in the vertex simulation (in world space units) */
var(Fluid)	float			GridSpacing;

/** Fluids automatically draw a low-resolution grid when they are deactivated. A reasonable value is needed for vertex fogging to work when the fluid is translucent. A maximum of 65000 vertices are allowed before GridSpacingLowRes is clamped. */
var(Fluid)	float			GridSpacingLowRes;

/** Target actor which the simulation grid will center around.  If none is provided, the simulation grid will center around the active camera. */
var(Fluid) actor			TargetSimulation;

/** How much the GPU should tessellate the fluid grid. (Only used on platforms that completely supports GPU tessellation.) */
var(Fluid)	float			GPUTessellationFactor;

/** How much to dampen the amplitude of waves in the fluid (0.0-30.0) */
var(Fluid) 	float			FluidDamping;

/** Wave travel speed factor for the simulation grid (0.0-1.0) */
var(Fluid) 	float			FluidTravelSpeed;

/** Wave height scale - higher value produces higher waves */
var(Fluid) 	float			FluidHeightScale;

/** Fluid update rate in number of updates per second */
var(Fluid) 	float			FluidUpdateRate;

/** How much ripple to make when fluid is hit by a weapon or touched by a object for the first time. */
var(Fluid)	float			ForceImpact;

/** How much ripple to make when an Actor moves through the fluid. */
var(Fluid)	float			ForceContinuous;

/** Increasing this value adds more contrast to the lighting by exaggerating the curvature for the fluid normals. */
var(Fluid)	float			LightingContrast;

/** Target actor which the detail texture will center around.  If none is provided, the detail texture will center around the active camera. */
var(Fluid)	actor			TargetDetail;

/** Whether the detail simulation grid should be used or not */
var(Fluid) 	bool			EnableDetail;

/** Distance between the camera and the closest fluid edge where the fluid will deactivate and start rendering as a simple flat quad. */
var(Fluid)	float			DeactivationDistance;

/** Number of simulation cells along each axis in the detail texture */
var(FluidDetail) 	int		DetailResolution;

/** World space size of one edge of the detail texture */
var(FluidDetail) 	float	DetailSize;

/** How much to dampen the amplitude of waves in the detail texture (0.0-30.0) */
var(FluidDetail) 	float	DetailDamping;

/** Wave travel speed factor for the detail texture (0.0-1.0) */
var(FluidDetail) 	float	DetailTravelSpeed;

/** How much of an applied force should be transferred to the detail texture (0.0-1.0) */
var(FluidDetail)	float	DetailTransfer;

/** Wave height scale for the detail texture - higher value produces higher waves */
var(FluidDetail) 	float	DetailHeightScale;

/** Fluid update rate in number of updates per second */
var(FluidDetail) 	float	DetailUpdateRate;

/** Whether to make the detail simulation tiled. */
var(FluidDetail)	bool	bTiling;

/** Whether to update the fluid or pause it */
var(FluidDebug)	transient bool	bPause;

/** Whether to render lines for normals */
var(FluidDebug)	transient bool	bShowSimulationNormals;

/** Whether to visualize the placement of the simulated grid */
var(FluidDebug)	bool			bShowSimulationPosition;

/** The length of the visualized normals, when bShowSimulationNormals is turned on */
var(FluidDebug)	float			NormalLength;

/** Whether to render an overlay of the detail normal for debugging */
var(FluidDebug)	bool			bShowDetailNormals;

/** Whether to visualize the placement of the detail texture */
var(FluidDebug)	bool			bShowDetailPosition;

/** Whether to visualize the height of the main fluid grid */
var(FluidDebug)	transient bool	bShowFluidSimulation;

/** Whether to show the detail normalmap on the fluid */
var(FluidDebug)	transient bool	bShowFluidDetail;

/** Whether to enable a force for debugging */
var(FluidDebug)	bool			bTestRipple;

/** Whether the test ripple should center on the detail texture or the main grid. */
var(FluidDebug)	bool			bTestRippleCenterOnDetail;

/** Angular speed of the test ripple */
var(FluidDebug)	float			TestRippleSpeed;

/** Number of seconds between each pling on the test ripple. 0 makes it continuous. */
var(FluidDebug)	float			TestRippleFrequency;

/** Radius of the test ripple, in world space */
var(FluidDebug)	float			TestRippleRadius;

var private float			FluidWidth;
var private float			FluidHeight;
var private native transient float	TestRippleTime;
var	private native transient float	TestRippleAngle;
var private native transient float	DeactivationTimer;
var private native transient float	ViewDistance;
var private native transient vector	SimulationPosition;
var private native transient vector	DetailPosition;

/** Stores a 1 for each clamped vertex that should not be simulated, and a 0 for each vertex that should be simulated. */
var			const array<byte> ClampMap;

var private const array<ShadowMap2D> ShadowMaps;

/** Reference to the texture lightmap resource. */
var native private const LightMapRef LightMap;

/** All transient member variables are contained inside the FFluidSurfaceInfo object. */
var private native transient const pointer FluidSimulation{class FFluidSimulation};

/** Apply a force to the fluid. */
native final function ApplyForce( vector WorldPos, float Strength, float Radius, optional bool bImpulse );

/** Set the position of the origin of the detail texture, within the fluid. */
native final function SetDetailPosition( vector WorldPos );

/** Set the position of the origin of the simulation grid, within the fluid. */
native final function SetSimulationPosition( vector WorldPos );

cpptext
{
public:
	UFluidSurfaceComponent();

	UMaterialInterface*		GetMaterial() const;
	FMaterialViewRelevance	GetMaterialViewRelevance() const;
	const class FFluidGPUResource* GetFluidGPUResource() const;					
	void					InitResources( UBOOL bActive );
	void					ReleaseResources( UBOOL bBlockOnRelease );
	void					RebuildClampMap();
	void					OnScaleChange();

	// Base class interfaces.
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual void			Serialize(FArchive& Ar);
	virtual void			AddReferencedObjects(TArray<UObject*>& ObjectArray);
	virtual void			UpdateBounds();
	virtual void			Attach();
	virtual void			Detach( UBOOL bWillReattach );
	virtual void			PreEditChange(UProperty* PropertyAboutToChange);
	virtual void			PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void			BeginDestroy();
	virtual UBOOL			IsReadyForFinishDestroy();
	virtual void			FinishDestroy();
	virtual void			Tick(FLOAT DeltaTime);
	virtual UBOOL			LineCheck(FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags);
	virtual UBOOL			PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);
	virtual void			InvalidateLightingCache();

	/** 
	 * Retrieves the materials used in this component 
	 * 
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	/**
	 * Returns the lightmap resolution used for this primivite instnace in the case of it supporting texture light/ shadow maps.
	 * 0 if not supported or no static shadowing.
	 *
	 * @param	Width	[out]	Width of light/shadow map
	 * @param	Height	[out]	Height of light/shadow map
	 *
	 * @return	UBOOL			TRUE if LightMap values are padded, FALSE if not
	 */
	virtual UBOOL			GetLightMapResolution( INT& Width, INT& Height ) const;
	virtual INT				GetStaticLightMapResolution() const;
	virtual void			GetLightAndShadowMapMemoryUsage( INT& LightMapMemoryUsage, INT& ShadowMapMemoryUsage ) const;
	virtual void			GetStaticLightingInfo(FStaticLightingPrimitiveInfo& OutPrimitiveInfo,const TArray<ULightComponent*>& InRelevantLights,const FLightingBuildOptions& Options);
	virtual ELightMapInteractionType GetStaticLightingType() const	{ return LMIT_Texture;	}
	/** Gets the emissive boost for the primitive component. */
	virtual FLOAT			GetEmissiveBoost(INT ElementIndex) const;
	/** Gets the diffuse boost for the primitive component. */
	virtual FLOAT			GetDiffuseBoost(INT ElementIndex) const;
	/** Gets the specular boost for the primitive component. */
	virtual FLOAT			GetSpecularBoost(INT ElementIndex) const;
	virtual void			GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

private:
	UBOOL					PropertyNeedsResourceRecreation(UProperty* Property);
	void					UpdateMemory(FLOAT DeltaTime);

	/**
	 *	Calculates the distance from the fluid's edge to the specified position.
	 *	@param WorldPosition	A world-space position to measure the distance to.
	 *	@return					Distance from the fluid's edge to the specified position, or 0 if it's inside the fluid.
	 */
	FLOAT					CalcDistance( const FVector& WorldPosition );
}

defaultproperties
{
	LightmapResolution=128
	EnableSimulation=true
	EnableDetail=true
	DeactivationDistance=3000.0
	FluidUpdateRate=30.0
	DetailUpdateRate=30.0
	bTiling=false
	FluidHeightScale=1.0
	DetailHeightScale=1.0
	bPause=false
	SimulationQuadsX=200
	SimulationQuadsY=200
	GridSpacing=10.0
	GridSpacingLowRes=800.0
	DetailResolution=256
	DetailSize=500
	FluidDamping=1.0
	FluidTravelSpeed=1.0
	DetailDamping=1.0
	DetailTravelSpeed=1.0
	DetailTransfer=0.5
	bTestRipple=false
	TestRippleSpeed=1
	TestRippleRadius=30
	TestRippleFrequency=1.0
	bShowFluidDetail=true
	bShowFluidSimulation=true
	bShowDetailNormals=false
	bShowDetailPosition=false
	bShowSimulationNormals=false
	bShowSimulationPosition=false
	NormalLength=10.0
	LightingContrast=1.0
	GPUTessellationFactor=1.0

	ForceImpact=-3.0
	ForceContinuous=-200.0

	FluidWidth=2000.0
	FluidHeight=2000.0
	TestRippleTime=0.0
	TestRippleAngle=0.0
	DeactivationTimer=10.0
	ViewDistance=0.0

	bTickInEditor=true
	CollideActors=true
	BlockZeroExtent=true
	BlockNonZeroExtent=true
	BlockRigidBody=false

	CastShadow=false
	bForceDirectLightMap=true
	bAcceptsLights=true
	bUsePrecomputedShadows=true

	// Fluid surfaces often have expensive vertex shaders so we want to make sure they get occlusion queried
	bIgnoreNearPlaneIntersection=true
}
