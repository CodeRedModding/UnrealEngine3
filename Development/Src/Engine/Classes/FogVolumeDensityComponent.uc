/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FogVolumeDensityComponent extends ActorComponent
	native(FogVolume)
	hidecategories(Object)
	abstract
	editinlinenew;

/** Fog Material to use on the AutomaticComponent.  This will not be used on FogVolumeActors, they will use their existing materials. */
var() MaterialInterface FogMaterial;

var MaterialInterface DefaultFogVolumeMaterial;

/** True if the fog is enabled. */
var()	const	bool	bEnabled;

/** 
 * Controls whether the fog volume affects intersecting translucency.  
 * If FALSE, the fog volume will sort normally with translucency and not fog intersecting translucent objects.
 */
var()   bool	bAffectsTranslucency;

/** 
 * Controls whether the fog volume affects opaque pixels, or just intersecting translucency.  
 */
var()   bool	bOnlyAffectsTranslucency;

/** 
 * Sets the 'EmissiveColor' Vector Parameter of FogMaterial.
 * This will have no effect if FogMaterial has been overridden with a material that does not have a 'EmissiveColor' parameter.  
 */
var()	interp	LinearColor	SimpleLightColor;

/** 
 * Color used to approximate fog material color on transparency. 
 * Important: Set this color to match the overall color of the fog material, otherwise transparency will not be fogged correctly.
 */
var()	interp	LinearColor	ApproxFogLightColor;

/** Distance from the camera that the fog should start, in world units. */
var()	interp	float	StartDistance;

/** 
 * MaxDistance can be tweaked to be as low as it can go without making the fog noticeably less opaque, 
 * And that will greatly improve the anti aliasing effect on opaque geometry in the foreground. 
 */
var()	float	MaxDistance;

/** 
 * Optional array of actors that will define the shape of the fog volume. 
 * These actors will not be moved along with the fog volume, and they can be selected directly.
 */
var()	array<Actor>	FogVolumeActors;

cpptext
{
private:
	/** Adds the fog volume components to the scene */
	void AddFogVolumeComponents();

	/** Removes the fog volume components from the scene */
	void RemoveFogVolumeComponents();

	/** 
	 * Sets up FogVolumeActors's mesh components to defaults that are common usage with fog volumes.  
	 * Collision is disabled for the actor, each component gets assigned the default fog volume material,
	 * lighting, shadowing, decal accepting, and occluding are disabled.
	 */
	void SetFogActorDefaults(const INT FogActorIndex);

protected:
	// ActorComponent interface.
	virtual void Attach();
	virtual void UpdateTransform();
	virtual void Detach( UBOOL bWillReattach = FALSE );

	// UObject interface
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& PropertyChangedEvent);

public:
	// FogVolumeDensityComponent interface.
	virtual class FFogVolumeDensitySceneInfo* CreateFogVolumeDensityInfo(const UPrimitiveComponent* MeshComponent) const PURE_VIRTUAL(UFogVolumeDensityComponent::CreateFogVolumeDensityInfo,return NULL;);

	/** Checks for partial fog volume setup that will not render anything. */
#if WITH_EDITOR
	virtual void CheckForErrors();
#endif

	/** Returns FogMaterial if it is valid, otherwise DefaultFogVolumeMaterial */
	UMaterialInterface*	GetMaterial() const;
}

/**
 * Changes the enabled state of the height fog component.
 * @param bSetEnabled - The new value for bEnabled.
 */
final native function SetEnabled(bool bSetEnabled);

defaultproperties
{
	DefaultFogVolumeMaterial=Material'EngineMaterials.FogVolumeMaterial'
	bEnabled=TRUE
	bAffectsTranslucency=TRUE
	bOnlyAffectsTranslucency=FALSE
	SimpleLightColor=(R=0.5,G=0.5,B=0.7,A=1.0)
	ApproxFogLightColor=(R=0.5,G=0.5,B=0.7,A=1.0)
	StartDistance=0.0
	MaxDistance=65535.0
}
