/**
 * FracturedSkinnedMeshComponent.uc - Uses skinning to draw detached parts with as few sections as possible.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FracturedSkinnedMeshComponent extends FracturedBaseComponent
	native(Mesh);

/* Render resources used by this component, and whose release progress is tracked by the FRenderCommandFence in FracturedBaseComponent. */
var protected{protected} const native transient pointer ComponentSkinResources{class FFracturedSkinResources};

/* A transform for each fragment, used to skin vertices from each fragment into position while minimizing draw calls. */
var protected{protected} transient const array<Matrix> FragmentTransforms;

/* An array of components whose visibility information will be used. */
var protected{protected} transient const array<FracturedStaticMeshComponent> DependentComponents;

/* TRUE when at least one fragment is unhidden after visibility is reset. */
var protected{protected} transient const bool bBecameVisible;

/** TRUE if fragment transforms have changed and the GPU should be refreshed */
var protected{protected} transient const bool bFragmentTransformsChanged;


cpptext
{
public:
	//UPrimitiveComponent
	virtual void UpdateBounds();
	virtual FPrimitiveSceneProxy* CreateSceneProxy();
	virtual UBOOL ShouldRecreateProxyOnUpdateTransform() const;

	/* Sets the visiblity of a single fragment, and starts a deferred reattach if visiblity changed. */
	void SetFragmentVisibility(INT FragmentIndex, UBOOL bVisibility);

	/* Updates the transform of a single fragment. */
	void SetFragmentTransform(INT FragmentIndex, const FMatrix& LocalToWorld);

	/* Adds a dependent component whose visibility will affect this component's visibility. */
	void RegisterDependentComponent(UFracturedStaticMeshComponent* InComponent);

	/* Removes a dependent component whose visibility will affect this component's visibility. */
	void RemoveDependentComponent(UFracturedStaticMeshComponent* InComponent);

	/** Static: Updates the GPU with bone matrices for this skinned fractured mesh */
	static void UpdateDynamicBoneData_RenderThread(FFracturedSkinResources* ComponentSkinResources, const TArray<FMatrix>& FragmentTransforms);

protected:

	virtual void InitResources();
	virtual void ReleaseResources();
	void ReleaseSkinResources();

	/** Attaches the component to the scene, and initializes the component's resources if they have not been yet. */
	virtual void Attach();

	virtual void UpdateTransform();

	friend class FFracturedSkinnedMeshSceneProxy;
}

defaultproperties
{
	bAllowCullDistanceVolume=FALSE
	bInitialVisibilityValue=FALSE
	CastShadow=FALSE
	bCastDynamicShadow=FALSE
	bForceDirectLightMap=FALSE
	bAllowApproximateOcclusion=TRUE
	RBCollideWithChannels=(Default=FALSE,GameplayPhysics=FALSE,EffectPhysics=FALSE,FracturedMeshPart=FALSE)
	CollideActors=FALSE
	BlockActors=FALSE
	BlockZeroExtent=FALSE
	BlockNonZeroExtent=FALSE
	BlockRigidBody=FALSE
	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=FALSE // this is set to FALSE as we will get floating decals after the various fracture pieces break off
	bOverrideLightMapResolution=FALSE
	bOverrideLightMapRes=FALSE
	bUsePrecomputedShadows=FALSE
}
