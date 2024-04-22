/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LensFlareComponent extends PrimitiveComponent
	native(LensFlare)
	hidecategories(Object)
	hidecategories(Physics)
	hidecategories(Collision)
	editinlinenew
	dependson(LensFlare);

var()	const			LensFlare							Template;
var		const			DrawLightConeComponent				PreviewInnerCone;
var		const			DrawLightConeComponent				PreviewOuterCone;
var		const			DrawLightRadiusComponent			PreviewRadius;

struct LensFlareElementInstance
{
	// No UObject reference
};

/** If TRUE, automatically enable this flare when it is attached */
var()								bool					bAutoActivate;

/** Internal variables */
var transient						bool					bIsActive;
var	transient						bool					bHasTranslucency;
var	transient						bool					bHasUnlitTranslucency;
var	transient						bool					bHasUnlitDistortion;
var	transient						bool					bUsesSceneColor;
var	transient						bool					bHasSeparateTranslucency;

/** Viewing cone angles. */
var transient						float					OuterCone;
var transient						float					InnerCone;
var transient						float					ConeFudgeFactor;
var transient						float					Radius;
/** When true the new algorithm is used (NOTE: The new algorithm does not use ConeFudgeFactor). */
var transient						bool					bUseTrueConeCalculation;
/** (New Algorithm only) If this is non-zero the lens flare will always draw with at least the strength specified, even behind or outside outer cone. */
var transient						float					MinStrength;

/** The color of the source	*/
var(Rendering)						linearcolor				SourceColor;

/** Storage for mobile as to whether this lens flare was visible based on a line check on previous check*/
var bool bVisibleForMobile;

struct native LensFlareElementMaterials
{
	var() array<MaterialInterface>	ElementMaterials;
};

/** Per-element material overrides.  These must NOT be set directly or a race condition can occur between GC and the rendering thread. */
var transient array<LensFlareElementMaterials>				Materials;

/** Command fence used to shut down properly */
var		native				const	pointer					ReleaseResourcesFence{class FRenderCommandFence};

/** Used to determine when to trace on mobile platforms */
var float NextTraceTime;

native final function SetTemplate(LensFlare NewTemplate, bool bForceSet=FALSE);
native		 function SetSourceColor(linearcolor InSourceColor);
native		 function SetIsActive(bool bInIsActive);

cpptext
{
	// UObject interface
	virtual void BeginDestroy();
	virtual UBOOL IsReadyForFinishDestroy();
	virtual void FinishDestroy();
	virtual void PreEditChange(UProperty* PropertyThatWillChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();

	// UActorComponent interface.
	virtual void Attach();

public:
	// UPrimitiveComponent interface
	virtual void UpdateBounds();
	virtual void Tick(FLOAT DeltaTime);

	/** 
	 *	Setup the Materials array for the lens flare component.
	 *	
	 *	@param	bForceReset		If TRUE, reset the array and refill it from the template.
	 */
	void SetupMaterialsArray(UBOOL bForceReset);

	virtual INT GetNumElements() const;
	virtual UMaterialInterface* GetElementMaterial(INT MaterialIndex) const;
	virtual void SetElementMaterial(INT ElementIndex, UMaterialInterface* InMaterial);

	/**
	 * Retrieves the materials used in this component
	 *
	 * @param OutMaterials	The list of used materials.
	 */
	virtual void GetUsedMaterials( TArray<UMaterialInterface*>& OutMaterials ) const;

	/** Returns true if the prim is using a material with unlit distortion */
	virtual UBOOL HasUnlitDistortion() const;
	/** Returns true if the prim is using a material with unlit translucency */
	virtual UBOOL HasUnlitTranslucency() const;
	/** Returns true if the prim is using a material with lit translucency */
	virtual UBOOL HasLitTranslucency() const;
	/** Returns true if the prim is using a material with separate translucency */
 	virtual UBOOL HasSeparateTranslucency() const;

	/**
	* Returns true if the prim is using a material that samples the scene color texture.
	* If true then these primitives are drawn after all other translucency
	*/
	virtual UBOOL UsesSceneColor() const;
	
	/** 
	 * Initialize the draw data that gets used when creating the visualization scene proxys.
	 *
	 * @param bUseTemplate		If true, will initialize with the data found in the lens flare template object.
	 */
	virtual void InitializeVisualizationData(UBOOL bUseTemplate);

	virtual FPrimitiveSceneProxy* CreateSceneProxy();

	// InstanceParameters interface
	void	AutoPopulateInstanceProperties();
}

/**
 * @param ElementIndex - The element to access the material of.
 * @return the material used by the indexed element of this mesh.
 */
native function MaterialInterface GetMaterial(int ElementIndex);

/**
 * Changes the material applied to an element of the mesh.
 * @param ElementIndex - The element to access the material of.
 * @return the material used by the indexed element of this mesh.
 */
native virtual function SetMaterial(int ElementIndex, MaterialInterface Material);

/**
 * Creates a material instance for the specified element index.  The parent of the instance is set to the material being replaced.
 * @param ElementIndex - The index of the skin to replace the material for.
 */
function MaterialInstanceConstant CreateAndSetMaterialInstanceConstant(int ElementIndex)
{
	local MaterialInstanceConstant Instance;

	// Create the material instance.
	Instance = new(self) class'MaterialInstanceConstant';
	Instance.SetParent(GetMaterial(ElementIndex));

	// Assign it to the given mesh element.
	// This MUST be done after setting the parent; otherwise the component will use the default material in place of the invalid material instance.
	SetMaterial(ElementIndex,Instance);

	return Instance;
}


defaultproperties
{
	NextTraceTime=0.0
	bAutoActivate=true
	bTickInEditor=true
	TickGroup=TG_PostAsyncWork
	bAllowApproximateOcclusion=false
	bFirstFrameOcclusion=true
	bIgnoreNearPlaneIntersection=true

	SourceColor=(R=1.0,G=1.0,B=1.0,A=1.0)

	bVisibleForMobile=false;
}
