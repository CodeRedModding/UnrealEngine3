/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

class DecalComponent extends PrimitiveComponent
	native(Decal)
	editinlinenew
	hidecategories(Collision,Object,Physics,PrimitiveComponent);

/** Decal material. */
var(Decal) private{private} const MaterialInterface	DecalMaterial;

/** Decal world space width. */
var(Decal) float Width;

/** Decal world space height. */
var(Decal) float Height;

/** Decal tiling along the tangent. */
var(Decal) float TileX;

/** Decal tiling along the binormal. */
var(Decal) float TileY;

/** Decal offset along the tangent. */
var(Decal) float OffsetX;

/** Decal offset along the binormal. */
var(Decal) float OffsetY;

/** Decal in-plane rotation, in degrees. */
var(Decal) float DecalRotation;

/** Horizontal field of view. */
var float FieldOfView;

/** Near plane clip distance. */
var(Decal) float NearPlane;

/** Far plane clip distance. */
var(Decal) float FarPlane;

/** Decal's frustum location, set in code or copied from DecalActor in UnrealEd. */
var transient vector Location;

/** Decal's frustum orientation, set in code or copied from DecalActor in UnrealEd. */
var transient rotator Orientation;

/** Decal's impact location, as computed by eg weapon trace. */
var vector		HitLocation;

/** Decal's impact normal, as computed by eg weapon trace.*/
var vector		HitNormal;

/** Decal's impact tangent, as computed by eg weapon trace.*/
var vector		HitTangent;

/** Decal's impact binormal, as computed by eg weapon trace.*/
var vector		HitBinormal;

/**
 * If FALSE (the default), use precise clipping to compute the decal geometry.
 * If TRUE, decal geometry generation is faster, but the decal material will have
 * to use clamped texture coordinates.
 */
var(Decal) bool bNoClip;

/**
 * TRUE for decals created in the editor, FALSE for decals created at runtime.
 */
var const bool bStaticDecal;

/**
 * If non-NULL, consider HitComponent only when computing receivers.
 */
var transient PrimitiveComponent HitComponent;

/**
 * The name of hit bone.
 */
var transient name HitBone;

/** If not -1, specifies the index of the BSP node that was hit. */
var transient int HitNodeIndex;

/** If not -1, specifies the level into the world's level array of the BSP node that was hit. */
var transient int HitLevelIndex;

/** If not -1, specifies the index of the FracturedStaticMeshComponent we hit */
var transient int FracturedStaticMeshComponentIndex;

/** Used to pass information of which BSP nodes where hit */
var private const transient array<int> HitNodeIndices;

/**
 * A decal receiver and its associated render data.
 */
struct native DecalReceiver
{
	var const PrimitiveComponent Component;
	var native const pointer RenderData{class FDecalRenderData};
};

/** List of receivers to which this decal is attached. */
var private noimport duplicatetransient const array<DecalReceiver> DecalReceivers;

/** List of receivers for static decals.  Empty if the decal has bStaticDecal=FALSE.*/
var native private noimport transient duplicatetransient const array<pointer> StaticReceivers{class FStaticReceiverData};

/** Command fence used to shut down properly. */
var native const transient duplicatetransient pointer		ReleaseResourcesFence{FRenderCommandFence};

/** Ortho planes. */
var private transient array<Plane> Planes;

var(DecalRender) float DepthBias;
var(DecalRender) float SlopeScaleDepthBias;

/** Controls the order in which decal elements are rendered.  Higher values draw later (on top). */
var(DecalRender) int	SortOrder;

/** Dot product of the minimum angle that surfaces can make with the decal normal to be considered backfacing. */
var(DecalRender) float	BackfaceAngle;

/** Start/End blend range specified as an angle in degrees. Controls where to start blending out the decal on a surface */
var(DecalRender) Vector2D	BlendRange;

/**
 * Allows artists to adjust the distance where textures using UV 0 are streamed in/out.
 * 1.0 is the default, whereas a higher value increases the streamed-in resolution.
 */
var(DecalRender) const float StreamingDistanceMultiplier;

enum EDecalTransform
{
	/** Location/Orientation are obtained from the owning actor's absolute world coordinates */
	DecalTransform_OwnerAbsolute,
	/** Location/Orientation are transformed relative to the owning actor's coordinates */
	DecalTransform_OwnerRelative,
	/** Location/Orientation are relative to the original spawn point in world coordinates */
	DecalTransform_SpawnRelative
};
/** Determines how the Location/Orientation of the decal are used */
var const EDecalTransform DecalTransform;

/**
 * Specifies how the decal application filter should be interpreted.
 */
enum EFilterMode
{
	/** Filter is ignored. */
	FM_None,
	/** Filter specifies list of components to ignore. */
	FM_Ignore,
	/** Filter specifies list of components to affect.*/
	FM_Affect
};

/** Current filter application mode. */
var(DecalFilter) EFilterMode	FilterMode;

/** Component filter. */
var(DecalFilter) array<Actor>	Filter;

/** @hack: Gears hack to avoid an octree look-up for level-placed decals. To be replaced with receiver serialization after ship.*/
var(DecalFilter) array<PrimitiveComponent>	ReceiverImages;

/** If FALSE (the default), don't project decal onto back-facing polygons. */
var(DecalFilter) bool bProjectOnBackfaces;

/** If FALSE (the default), don't project decal onto hidden receivers. */
var(DecalFilter) bool bProjectOnHidden;

/** If FALSE, don't project decal onto BSP. */
var(DecalFilter) bool bProjectOnBSP;

/** If FALSE, don't project decal onto static meshes. */
var(DecalFilter) bool bProjectOnStaticMeshes;

/** If FALSE, don't project decal onto skeletal meshes. */
var(DecalFilter) bool bProjectOnSkeletalMeshes;

/** If FALSE, don't project decal onto terrain. */
var(DecalFilter) bool bProjectOnTerrain;

/** If TRUE, invert the direction considered to be backfacing receiver triangles.  Set e.g. when decal actors are mirrored. */
var bool bFlipBackfaceDirection;

/** If TRUE, then the decal will recompute its receivers whenever its transform is updated. Allowing for dynamic movable decals */
var bool bMovableDecal;

/** TRUE if the decal has already been attached once. Allows for static decals to be reattached */
var private transient bool bHasBeenAttached;

/** Decal location relative to parent transform used with DecalTransform_OwnerRelative mode */
var(DecalRender) vector ParentRelativeLocation;
/** Decal orientation vector relative to parent transform used with DecalTransform_OwnerRelative mode */
var(DecalRender) rotator ParentRelativeOrientation;

/** Decal location/orientation relative to parent transform when first attached */
var const private transient matrix ParentRelLocRotMatrix;

/**
 * So for some DecalComponents (usually residing in GamePlay objects) we set the DecalMaterial from an Archetype or some other default property.
 * For those decals we do not want to MapCheckForErrors on them.
 **/
var() bool bDecalMaterialSetAtRunTime;


/** detaches the component and resets the component's properties to the values of its template */
native final function ResetToDefaults();

/** setting decal material on decal component. This will force the decal to reattach */
native final function SetDecalMaterial(MaterialInterface NewDecalMaterial);
/** Accessor for decal material */
native final function MaterialInterface GetDecalMaterial() const;

/** Needed for proper synchronization of pooled decals */
native final function bool IsWaitingForResetToDefaultsToComplete();

cpptext
{
public:
	/**
	 * Builds orthogonal planes from HitLocation, HitNormal, Width, Height and Depth.
	 */
	void UpdateOrthoPlanes();

	/**
	 * @return	TRUE if the decal is enabled as specified in GEngine, FALSE otherwise.
	 */
	UBOOL IsEnabled() const;

	/**
	 * If the decal is non-static, allocates a sort key and, if the decal is lit, a random
	 * depth bias to the decal.
	 */
	void AllocateSortKey();

	/**
	 * Fills in the specified vertex list with the local-space decal frustum vertices.
	 */
	void GenerateDecalFrustumVerts(FVector Verts[8]) const;

	/**
	 * Fills in the specified decal state object with this decal's state.
	 */
	void CaptureDecalState(class FDecalState* DecalState) const;

	void AttachReceiver(UPrimitiveComponent* Receiver);

	/**
	 * Updates ortho planes, computes the receiver set, and connects to a decal manager.
	 */
	void ComputeReceivers();

	/**
	 * Attaches the decal to static receivers, according to the data stored in StaticReceivers.
	 */
	void AttachToStaticReceivers();

	/**
	 * Disconnects the decal from its manager and detaches receivers.
	 */
	void DetachFromReceivers();

	/**
	 * Disconnects the decal from its manager and detaches receivers matching the given primitive component.
	 * This will also release the resources associated with the receiver component.
	 *
	 * @param Receiver	The receiving primitive to detach decal render data for
	 */
	void DetachFromReceiver(UPrimitiveComponent* Receiver);

	/**
	 * @return		TRUE if the application filter passes the specified component, FALSE otherwise.
	 */
	UBOOL FilterComponent(UPrimitiveComponent* Component) const;

	/**
	 * Frees any StaticReceivers data.
	 */
	void FreeStaticReceivers();

	// PrimitiveComponent interface.
	/**
	 * Sets the component's bounds based on the vertices of the decal frustum.
	 */
	virtual void UpdateBounds();

	/**
	 * Enumerates the streaming textures used by the primitive.
	 * @param OutStreamingTextures - Upon return, contains a list of the streaming textures used by the primitive.
	 */
	virtual void GetStreamingTextureInfo(TArray<FStreamingTexturePrimitiveInfo>& OutStreamingTextures) const;

	/**
	 * Retrieves the materials used in this component
	 *
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	/**
	 * Creates a FPrimitiveSceneProxy proxy to implement debug rendering of the decal in the rendering thread.
	 *
	 * @return		A heap-allocated proxy rendering object corresponding to this decal component.
	 */
	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	/**
	 * @return		TRUE if both IsEnabled() and Super::IsValidComponent() return TRUE.
	 */
	virtual UBOOL IsValidComponent() const;

	/**
	 * Determines whether the proxy for this primitive type needs to be recreated whenever the primitive moves.
	 * @return TRUE to recreate the proxy when UpdateTransform is called.
	 */
	virtual UBOOL ShouldRecreateProxyOnUpdateTransform() const;

	/**
	* convert interval specified in degrees to clamped dot product range
	* @return min,max range of blending decal
	*/
	virtual FVector2D CalcDecalDotProductBlendRange() const;

	virtual INT GetNumElements() const
	{
		return 1; // DecalMaterial
	}
	virtual UMaterialInterface* GetElementMaterial(INT ElementIndex) const
	{
		return (ElementIndex == 0) ? DecalMaterial : NULL;
	}
	virtual void SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial)
	{
		if (ElementIndex == 0)
		{
			SetDecalMaterial(InMaterial);
		}
	}

	/** EditorLinkSelectionInterface */
	virtual UBOOL LinkSelection(USelection* SelectedActors);
	virtual UBOOL UnLinkSelection(USelection* SelectedActors);

	// ActorComponent interface.
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	// UObject interface;
	virtual void BeginPlay();
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();
	virtual void PreSave();
	virtual void Serialize(FArchive& Ar);
	virtual	INT	GetResourceSize(void);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void AddReferencedObjects(TArray<UObject*>& ObjectArray);

protected:
	void ReleaseResources(UBOOL bBlockOnRelease, UPrimitiveComponent* Receiver=NULL);

	// ActorComponent interface.
	virtual void Attach();
	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );
}

defaultproperties
{
	bAcceptsLights=FALSE
	bAcceptsDynamicLights=FALSE
	bCastDynamicShadow=FALSE

	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=FALSE

	bMovableDecal=FALSE

	Width=200
	Height=200
	TileX=1
	TileY=1
	NearPlane=0
	FarPlane=300
	FieldOfView=80
	HitNodeIndex=-1
	HitLevelIndex=-1
	DepthBias=-0.00006
	SlopeScaleDepthBias=0.0
	BackfaceAngle=0.001			// Corresponds to an angle of 89.94 degrees

	bProjectOnBSP=TRUE
	bProjectOnSkeletalMeshes=TRUE
	bProjectOnStaticMeshes=TRUE
	bProjectOnTerrain=TRUE

	BlendRange=(X=89.5,Y=180)

	StreamingDistanceMultiplier=1.0

	DecalTransform=DecalTransform_SpawnRelative
}
