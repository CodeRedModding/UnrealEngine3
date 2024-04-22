/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class LensFlare extends Object
	native(LensFlare)
	dontcollapsecategories
	hidecategories(Object);

/**
 *	Helper for getting curves from distributions
 */
struct native transient LensFlareElementCurvePair
{
	var		string	CurveName;
	var		object	CurveObject;
};

/**
 *	LensFlare Element
 */
struct native LensFlareElement
{
	/**
	 *	The name of the element. (Optional)
	 */
	var()	name								ElementName;

	/**
	 *	The position along the ray from the source to the viewpoint to render the flare at.
	 *	0.0 = At the source
	 *	1.0 = The source point reflected about the view center.
	 *	< 0 = The point along the ray going away from the center past the source.
	 *	> 1 = The point along the ray beyond the 'end point' of the ray reflection.
	 */
	var()	float								RayDistance;

	/**
	 *	Whether the element is enabled or not
	 */
	var()	bool								bIsEnabled;

	/**
	 *	Whether the element value look ups should use the radial distance
	 *	from the center to the edge of the screen or the ratio of the distance
	 *	from the source element.
	 */
	var()	bool								bUseSourceDistance;

	/**
	 *	Whether the radial distance should be normalized to a unit value.
	 *	Without this, the radial distance will be 0..1 in the horizontal and vertical cases.
	 *	It will be 0..1.4 in the corners.
	 */
	var()	bool								bNormalizeRadialDistance;

	/**
	 *	Whether the element color value should be scaled by the source color.
	 */
	var()	bool								bModulateColorBySource;

	/**
	 *	The 'base' size of the element
	 */
	var()	vector								Size;

	/**
	 *	The material(s) to use for the flare element.
	 */
	var(Material)	array<MaterialInterface>	LFMaterials;

	/**
	 *	Each of the following properties are accessed based on the radial distance from the
	 *	center of the screen to the edge.
	 *	<1 = Opposite the ray direction
	 *	0  = Center view
	 *	1  = Edge of screen
	 *	>1 = Outside of screen edge
	 */

	/** Index of the material to use from the LFMaterial array. */
	var(Material)	rawdistributionfloat		LFMaterialIndex;

	/**	Global scaling.	 */
	var(Scaling)	rawdistributionfloat		Scaling;

	/**	Anamorphic scaling.	*/
	var(Scaling)	rawdistributionvector		AxisScaling;

	/**	Rotation.	 */
	var(Rotation)	rawdistributionfloat		Rotation;
	/** If TRUE, always rotate the element to orient towards the source */
	var(Rotation)	bool						bOrientTowardsSource;

	/** Color (passed to the element material via the VertexColor expression) */
	var(Color)		rawdistributionvector		Color;
	/** Alpha (passed to the element material via the VertexColor expression) */
	var(Color)		rawdistributionfloat		Alpha;

	/** Offset. */
	var(Offset)		rawdistributionvector		Offset;

	/** Source to camera distance scaling. */
	/** Value to scale the AxisScaling by. Uses source to camera distance to look up the value (in Unreal units) */
	var(Scaling)	rawdistributionvector		DistMap_Scale;
	/** Value to scale the Color by. Uses source to camera distance to look up the value (in Unreal units) */
	var(Scaling)	rawdistributionvector		DistMap_Color;
	/** Value to scale the Alpha by. Uses source to camera distance to look up the value (in Unreal units) */
	var(Scaling)	rawdistributionfloat		DistMap_Alpha;

	structcpptext
	{
		void GetCurveObjects(TArray<FLensFlareElementCurvePair>& OutCurves);
		void DuplicateDistribution_Float(const FRawDistributionFloat& SourceDist, UObject* Outer, FRawDistributionFloat& NewDist);
		void DuplicateDistribution_Vector(const FRawDistributionVector& SourceDist, UObject* Outer, FRawDistributionVector& NewDist);
		UBOOL DuplicateFromSource(const FLensFlareElement& InSource, UObject* Outer);
		UObject* GetCurve(FString& CurveName);
	}

	structdefaultproperties
	{
		RayDistance=0.0
		bNormalizeRadialDistance=true

		Begin Object Class=DistributionFloatConstant Name=DistributionLFMaterialIndex
			Constant=0.0;
		End Object
		LFMaterialIndex=(Distribution=DistributionLFMaterialIndex)

		Begin Object Class=DistributionFloatConstant Name=DistributionScaling
			Constant=1.0;
		End Object
		Scaling=(Distribution=DistributionScaling)

		Begin Object Class=DistributionVectorConstant Name=DistributionAxisScaling
			Constant=(X=1.0,Y=1.0,Z=0.0)
		End Object
		AxisScaling=(Distribution=DistributionAxisScaling)

		Begin Object Class=DistributionFloatConstant Name=DistributionRotation
			Constant=1.0;
		End Object
		Rotation=(Distribution=DistributionRotation)

		Begin Object Class=DistributionVectorConstant Name=DistributionColor
			Constant=(X=1.0,Y=1.0,Z=1.0)
		End Object
		Color=(Distribution=DistributionColor)

		Begin Object Class=DistributionFloatConstant Name=DistributionAlpha
			Constant=1.0f;
		End Object
		Alpha=(Distribution=DistributionAlpha)

		Begin Object Class=DistributionVectorConstant Name=DistributionOffset
			Constant=(X=0.0,Y=0.0,Z=0.0)
		End Object
		Offset=(Distribution=DistributionOffset)

		Begin Object Class=DistributionVectorConstant Name=DistributionDistMap_Scale
			Constant=(X=1.0,Y=1.0,Z=1.0)
		End Object
		DistMap_Scale=(Distribution=DistributionDistMap_Scale)

		Begin Object Class=DistributionVectorConstant Name=DistributionDistMap_Color
			Constant=(X=1.0,Y=1.0,Z=1.0)
		End Object
		DistMap_Color=(Distribution=DistributionDistMap_Color)

		Begin Object Class=DistributionFloatConstant Name=DistributionDistMap_Alpha
			Constant=1.0f;
		End Object
		DistMap_Alpha=(Distribution=DistributionDistMap_Alpha)
	}
};

/** The Source of the lens flare */
var					editinline	export	LensFlareElement			SourceElement;
/** The StaticMesh to use as the source (optional) */
var(Source)								StaticMesh					SourceMesh;
/** The scene depth priority group to draw the source primitive in. */
var					const				ESceneDepthPriorityGroup	SourceDPG;

/** The individual reflection elements of the lens flare */
var					editinline	export	array<LensFlareElement>		Reflections;
/** The scene depth priority group to draw the reflection primitive(s) in. */
var(Reflections)	const				ESceneDepthPriorityGroup	ReflectionsDPG;

/** Viewing cone angles. */
var(Visibility)							float						OuterCone;
var(Visibility)							float						InnerCone;
var(Visibility)							float						ConeFudgeFactor;
var(Visibility)							float						Radius;
/** When true the new algorithm is used (NOTE: The new algorithm does not use ConeFudgeFactor). */
var(Visibility)							bool						bUseTrueConeCalculation;
/** (New Algorithm only) If this is non-zero the lens flare will always draw with at least the strength specified, even behind or outside outer cone. */
var(Visibility)							float						MinStrength;

/** Occlusion. */
/** 
 *	The mapping of screen coverage percentage (the result returned by occlusion checks)
 *	to the value passed into the materials for LensFlareOcclusion.
 */
var(Occlusion)							rawdistributionfloat		ScreenPercentageMap;

/**
 *	If TRUE, use the given bounds.
 *	If FALSE and a static mesh is set for the source, the static mesh bounds will be used.
 *	If FALSE and no static mesh is set, it will use the default bounds (likely not a good thing).
 */
var(Bounds)								bool						bUseFixedRelativeBoundingBox;
/** The fixed bounding box to use when bUseFixedRelativeBoundingBox is TRUE */
var(Bounds)								box							FixedRelativeBoundingBox;

/** Debugging helpers */
var(Debug)								bool						bRenderDebugLines;

/** Used for curve editor to remember curve-editing setup.						*/
var					export				InterpCurveEdSetup			CurveEdSetup;

/** Internal variable used to initialize new entries in the Reflectsions array	*/
var					transient			int							ReflectionCount;

/** The angle to use when rendering the thumbnail image							*/
var										rotator						ThumbnailAngle;

/** The distance to place the system when rendering the thumbnail image			*/
var										float						ThumbnailDistance;

/** Internal: Indicates the thumbnail image is out of date						*/
var										bool						ThumbnailImageOutOfDate;
/** Internal: The thumbnail image												*/
var										Texture2D					ThumbnailImage;

//
cpptext
{
	// UObject interface.
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostLoad();
	
	// CurveEditor helper interface
	void	AddElementCurvesToEditor(INT ElementIndex, UInterpCurveEdSetup* EdSetup);
	void	RemoveElementCurvesFromEditor(INT ElementIndex, UInterpCurveEdSetup* EdSetup);
	void	AddElementCurveToEditor(INT ElementIndex, FString& CurveName, UInterpCurveEdSetup* EdSetup);
	UObject* GetElementCurve(INT ElementIndex, FString& CurveName);
	
	//
	const FLensFlareElement* GetElement(INT ElementIndex) const;
	
	/** Return TRUE if element was found and bIsEnabled set to given value. */
	UBOOL SetElementEnabled(INT ElementIndex, UBOOL bInIsEnabled);
	
	/** Initialize the element at the given index */
	UBOOL InitializeElement(INT ElementIndex);

	/** Get the curve objects associated with the LensFlare itself */
	void GetCurveObjects(TArray<FLensFlareElementCurvePair>& OutCurves);
}

//
defaultproperties
{
	Begin Object Class=DistributionFloatConstant Name=DistributionLFMaterialIndex
		Constant=0.0;
	End Object
	Begin Object Class=DistributionFloatConstant Name=DistributionScaling
		Constant=1.0;
	End Object
	Begin Object Class=DistributionVectorConstant Name=DistributionAxisScaling
		Constant=(X=1.0,Y=1.0,Z=0.0)
	End Object
	Begin Object Class=DistributionFloatConstant Name=DistributionRotation
		Constant=0.0;
	End Object
	Begin Object Class=DistributionVectorConstant Name=DistributionColor
		Constant=(X=1.0,Y=1.0,Z=1.0)
	End Object
	Begin Object Class=DistributionFloatConstant Name=DistributionAlpha
		Constant=1.0f;
	End Object
	Begin Object Class=DistributionVectorConstant Name=DistributionOffset
		Constant=(X=0.0,Y=0.0,Z=0.0)
	End Object
	Begin Object Class=DistributionVectorConstant Name=DistributionDistMap_Scale
		Constant=(X=1.0,Y=1.0,Z=1.0)
	End Object
	Begin Object Class=DistributionVectorConstant Name=DistributionDistMap_Color
		Constant=(X=1.0,Y=1.0,Z=1.0)
	End Object
	Begin Object Class=DistributionFloatConstant Name=DistributionDistMap_Alpha
		Constant=1.0f;
	End Object

	SourceElement=(ElementName="Source",RayDistance=0.0,bIsEnabled=true,Size=(X=75.0f,Y=75.0f,Z=75.0f),LFMaterialIndex=(Distribution=DistributionLFMaterialIndex),Scaling=(Distribution=DistributionScaling),AxisScaling=(Distribution=DistributionAxisScaling),Rotation=(Distribution=DistributionRotation),Color=(Distribution=DistributionColor),Alpha=(Distribution=DistributionAlpha),Offset=(Distribution=DistributionOffset),DistMap_Scale=(Distribution=DistributionDistMap_Scale),DistMap_Color=(Distribution=DistributionDistMap_Color),DistMap_Alpha=(Distribution=DistributionDistMap_Alpha))
	SourceDPG=SDPG_World
	ReflectionsDPG=SDPG_Foreground

	OuterCone=0.0
	InnerCone=0.0
	ConeFudgeFactor=0.5
	Radius=0.0
	bUseTrueConeCalculation=false
	MinStrength=0.0

	Begin Object Class=DistributionFloatConstantCurve Name=DistributionScreenPercentageMap
		ConstantCurve=(Points=((InVal=0.0,OutVal=0.0),(InVal=1.0,OutVal=1.0)))
	End Object
	ScreenPercentageMap=(Distribution=DistributionScreenPercentageMap)
}
