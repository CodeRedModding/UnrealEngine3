/**
 *	LensFlare.h: LensFlare and helper class definitions.
 *	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
#ifndef _LENSFLARE_HEADER_
#define _LENSFLARE_HEADER_

class FLensFlareSceneProxy;

#include "EngineLensFlareClasses.h"
#include "PrimitiveSceneProxyOcclusionTracker.h"

class ULensFlare;
class ULensFlareComponent;
class ALensFlareSource;

/** Vertex definition for lens flare elements */
struct FLensFlareVertex
{
	FVector4		Position;
	FVector4		Size;
	FVector4		RadialDist_SourceRatio_Intensity;
	FVector2D		Rotation;
	FVector2D		TexCoord;
	FLinearColor	Color;
};

/** Helper structure for looking up element values */
struct FLensFlareElementValues
{
	/** The lookup values. */
	FLOAT RadialDistance;
	FLOAT SourceDistance;
	/** The material(s) to use for the flare element. */
	FMaterialRenderProxy* LFMaterial;
	/**	Global scaling.	 */
	FLOAT Scaling;
	/**	Anamorphic scaling.	*/
	FVector AxisScaling;
	/**	Rotation.	 */
	FLOAT Rotation;
	/** Color. */
	FLinearColor Color;
	/** Offset. */
	FVector Offset;
};

/*
 *	Sorting Helper
 */
struct FLensFlareElementOrder
{
	INT		ElementIndex;
	FLOAT	RayDistance;
	
	FLensFlareElementOrder(INT InElementIndex, FLOAT InRayDistance):
		ElementIndex(InElementIndex),
		RayDistance(InRayDistance)
	{}
};

/**
 *	LensFlare Element for the RenderThread
 */
struct FLensFlareRenderElement
{
	/**
	 *	The position along the ray from the source to the viewpoint to render the flare at.
	 *	0.0 = At the source
	 *	1.0 = The source point reflected about the view center.
	 *	< 0 = The point along the ray going away from the center past the source.
	 *	> 1 = The point along the ray beyond the 'end point' of the ray reflection.
	 */
	FLOAT RayDistance;

	/**
	 *	Whether the element is enabled or not
	 */
	BITFIELD bIsEnabled:1;
	/**
	 *	Whether the element value look ups should use the radial distance
	 *	from the center to the edge of the screen or the ratio of the distance
	 *	from the source element.
	 */
	BITFIELD bUseSourceDistance:1;
	/**
	 *	Whether the radial distance should be normalized to a unit value.
	 *	Without this, the radial distance will be 0..1 in the horizontal and vertical cases.
	 *	It will be 0..1.4 in the corners.
	 */
	BITFIELD bNormalizeRadialDistance:1;
	/**
	 *	Whether the element color value should be scaled by the source color.
	 */
	BITFIELD bModulateColorBySource:1;
	/**
	 *	Whether the element should orient towards the source
	 */
	BITFIELD bOrientTowardsSource:1;

	/**
	 *	The 'base' size of the element
	 */
	FVector Size;

	/**
	 *	The material(s) to use for the flare element.
	 */
	TArrayNoInit<FMaterialRenderProxy*> LFMaterials[2];

	/**
	 *	Each of the following properties are accessed based on the radial distance from the
	 *	center of the screen to the edge.
	 *	<1 = Opposite the ray direction
	 *	0  = Center view
	 *	1  = Edge of screen
	 *	>1 = Outside of screen edge
	 */

	/** Index of the material to use from the LFMaterial array. */
	FRawDistributionFloat LFMaterialIndex;

	/**	Global scaling.	 */
	FRawDistributionFloat Scaling;

	/**	Anamorphic scaling.	*/
	FRawDistributionVector AxisScaling;

	/**	Rotation.	 */
	FRawDistributionFloat Rotation;

	/** Color. */
	FRawDistributionVector Color;
	FRawDistributionFloat Alpha;

	/** Offset. */
	FRawDistributionVector Offset;

	/** Source to camera distance scaling. */
	FRawDistributionVector DistMap_Scale;
	FRawDistributionVector DistMap_Color;
	FRawDistributionFloat DistMap_Alpha;

	FLensFlareRenderElement()
	{
		appMemzero(this, sizeof(FLensFlareRenderElement));
	}

	FLensFlareRenderElement(const FLensFlareElement& InElement, const FLensFlareElementMaterials& InElementMaterials)
	{
		appMemzero(this, sizeof(FLensFlareRenderElement));
		CopyFromElement(InElement, InElementMaterials);
	}

	~FLensFlareRenderElement();

	void CopyFromElement(const FLensFlareElement& InElement, const FLensFlareElementMaterials& InElementMaterials);

	void ClearDistribution_Float(FRawDistributionFloat& Dist);
	void ClearDistribution_Vector(FRawDistributionVector& Dist);
	void SetupDistribution_Float(const FRawDistributionFloat& SourceDist, FRawDistributionFloat& NewDist);
	void SetupDistribution_Vector(const FRawDistributionVector& SourceDist, FRawDistributionVector& NewDist);
};

/** Dynamic data for a lens flare */
struct FLensFlareDynamicData
{
	FLensFlareDynamicData(const ULensFlareComponent* InLensFlareComp, FLensFlareSceneProxy* InProxy);
	~FLensFlareDynamicData();

	DWORD GetMemoryFootprint( void ) const;

	/**
	 */
	void InitializeRenderResources(const ULensFlareComponent* InLensFlareComp, FLensFlareSceneProxy* InProxy);
	void RenderThread_InitializeRenderResources(FLensFlareSceneProxy* InProxy);
	void ReleaseRenderResources(const ULensFlareComponent* InLensFlareComp, FLensFlareSceneProxy* InProxy);
	void RenderThread_ReleaseRenderResources();

	/**
	 *	Render thread only draw call
	 *	
	 *	@param	Proxy		The scene proxy for the lens flare
	 *	@param	PDI			The PrimitiveDrawInterface
	 *	@param	View		The SceneView that is being rendered
	 *	@param	DPGIndex	The DrawPrimitiveGroup being rendered
	 */
	virtual void Render(FLensFlareSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex, DWORD Flags);

	/** Render the source element. */
	virtual void RenderSource(FLensFlareSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex, DWORD Flags);
	/** Render the reflection elements. */
	virtual void RenderReflections(FLensFlareSceneProxy* Proxy, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex, DWORD Flags);

protected:
	/**
	 *	Retrieve the various values for the given element
	 *
	 *	@param	ScreenPosition		The position of the element in screen space
	 *	@param	SourcePosition		The position of the source in screen space
	 *	@param	Element				The element of interest
	 *	@param	Values				The values to fill in
	 *
	 *	@return	UBOOL				TRUE if successful
	 */
	UBOOL GetElementValues(FVector& ScreenPosition, FVector& SourcePosition, const FSceneView* View, 
		FLOAT DistanceToSource, FLensFlareRenderElement* Element, FLensFlareElementValues& Values, const UBOOL bSelected);

	/**
	 *	Sort the contained elements along the ray
	 */
	void SortElements();

    struct FLensFlareRenderElement SourceElement;
    TArray<struct FLensFlareRenderElement> Reflections;

	FLensFlareVertexFactory* VertexFactory;
	FLensFlareVertex* VertexData;

	TArray<FLensFlareElementOrder>	ElementOrder;
};

//
//	Scene Proxies
//
class FLensFlareSceneProxy : public FPrimitiveSceneProxy, public FPrimitiveSceneProxyOcclusionTracker
{
public:
	/** Initialization constructor. */
	FLensFlareSceneProxy(const ULensFlareComponent* Component);
	virtual ~FLensFlareSceneProxy();

	// FPrimitiveSceneProxy interface.
	
	/**
	 * Draws the primitive's static elements.  This is called from the game thread once when the scene proxy is created.
	 * The static elements will only be rendered if GetViewRelevance declares static relevance.
	 * Called in the game thread.
	 * @param PDI - The interface which receives the primitive elements.
	 */
	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI);

	/** 
	* Draw the scene proxy as a dynamic element
	*
	* @param	PDI - draw interface to render to
	* @param	View - current view
	* @param	DPGIndex - current depth priority 
	* @param	Flags - optional set of flags from EDrawDynamicElementFlags
	*/
	virtual void DrawDynamicElements(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View);

	FLensFlareDynamicData* GetDynamicData()
	{
		return DynamicData;
	}

	FLensFlareDynamicData* GetLastDynamicData()
	{
		return LastDynamicData;
	}

	void SetLastDynamicData(FLensFlareDynamicData* InLastDynamicData)
	{
		LastDynamicData  = InLastDynamicData;
	}

	virtual DWORD GetMemoryFootprint( void ) const { return( sizeof( *this ) + GetAllocatedSize() ); }
	DWORD GetAllocatedSize( void ) const
	{ 
		DWORD AdditionalSize = FPrimitiveSceneProxy::GetAllocatedSize();
		if (DynamicData)
		{
			AdditionalSize += DynamicData->GetMemoryFootprint();
		}

		return AdditionalSize; 
	}

	/**
	 *	Set the lens flare active or not...
	 *	@param	bInIsActive		The active state to set the LF to
	 */
	void SetIsActive(UBOOL bInIsActive);
	/**
	 *	Render-thread side of the SetIsActive function
	 *
	 *	@param	bInIsActive		The active state to set the LF to
	 */
	void SetIsActive_RenderThread(UBOOL bInIsActive);

	// This MUST be overriden
	virtual FLOAT GetOcclusionPercentage(const FSceneView& View) const
	{
		if (View.Family->ShowFlags & SHOW_Game)
		{
#if WITH_REALD
			return StereoCoveragePercentage;
#else
			FSceneViewState* State = (FSceneViewState*)(View.State);
			if (State != NULL)
			{
				const FCoverageInfo* Coverage = CoverageMap.Find(State);
				if (Coverage != NULL)
				{
					return Coverage->Percentage;
				}
			}
			return 1.0f;
#endif
		}
		return CoveragePercentage;
	}

public:
	// While this isn't good OO design, access to everything is made public.
	// This is to allow custom emitter instances to easily be written when extending the engine.
	FPrimitiveSceneInfo* GetPrimitiveSceneInfo() const	{	return PrimitiveSceneInfo;	}

	FMatrix& GetLocalToWorld()					{	return LocalToWorld;			}
	FMatrix GetWorldToLocal()					{	return LocalToWorld.Inverse();	}
	FLOAT GetLocalToWorldDeterminant()			{	return LocalToWorldDeterminant;	}
	AActor* GetOwner()							{	return Owner;					}
	UBOOL GetSelected()							{	return bSelected;				}
	FLOAT GetCullDistance()						{	return CullDistance;			}
	UBOOL GetCastShadow()						{	return bCastShadow;				}
	UBOOL GetHasTranslucency()					{	return bHasTranslucency;		}
	UBOOL GetHasDistortion()					{	return bHasDistortion;			}
	UBOOL GetUsesSceneColor()					{	return bUsesSceneColor;			}
	UBOOL GetRenderDebug()						{	return bRenderDebug;			}
	const FLinearColor& GetSourceColor()		{	return SourceColor;				}

	FLOAT	GetConeStrength() const				{	return ConeStrength;			}

	/**
	 * Helper function to allow mobile platforms to control perceived occlusion of the lens flare
	 * Positive deltas mean the flare is MORE visible
	 * Negative deltas mean the flare is LESS visible
	 */
	void ChangeMobileOcclusionPercentage(const FLOAT DeltaPercent);

protected:
	/** 
	 *	Get the results of the last frames occlusion and kick off the one for the next frame
	 *
	 *	@param	PDI - draw interface to render to
	 *	@param	View - current view
	 *	@param	DPGIndex - current depth priority 
	 *	@param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	UBOOL UpdateAndRenderOcclusionData(FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	/** 
	 *	Check if the flare is relevant for this view
	 *
	 *	@param	View - current view
	 */
	UBOOL CheckViewStatus(const FSceneView* View);

	AActor* Owner;

	UBOOL bSelected;
	UBOOL bIsActive;

	FLOAT CullDistance;

	BITFIELD bCastShadow : 1;
	BITFIELD bHasTranslucency : 1;
	BITFIELD bHasUnlitTranslucency : 1;
	BITFIELD bHasSeparateTranslucency : 1;
	BITFIELD bHasLitTranslucency : 1;
	BITFIELD bHasDistortion : 1;
	BITFIELD bUsesSceneColor : 1;
	BITFIELD bRenderDebug : 1;
	/** When true the new algorithm is used (NOTE: The new algorithm does not use ConeFudgeFactor). */
	BITFIELD bUseTrueConeCalculation : 1;

	/** The scene depth priority group to draw the source primitive in. */
	BYTE	SourceDPG;
	/** The scene depth priority group to draw the reflection primitive(s) in. */
	BYTE	ReflectionsDPG;

	/** View cone */
	FLOAT	OuterCone;
	FLOAT	InnerCone;
	FLOAT	ConeFudgeFactor;
	FLOAT	Radius;
	FLOAT	ConeStrength;
	/** This is used to keep some lens flare even when behind or outside of the outer cone. */
	FLOAT	MinStrength;

	FLOAT	MobileOcclusionPercentage;

	/** Occlusion mapping - screen percentage coverage will be looked up in here */
	FRawDistributionFloat* ScreenPercentageMap;

	/** Source color */
	FLinearColor SourceColor;

	FLensFlareDynamicData* DynamicData;
	FLensFlareDynamicData* LastDynamicData;

#if WITH_REALD
	FLOAT StereoCoveragePercentage;
#endif
};

#endif	//#ifndef _LENSFLARE_HEADER_
