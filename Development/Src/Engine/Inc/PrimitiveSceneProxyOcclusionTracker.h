/*=============================================================================
	PrimitiveSceneProxyOcclusionTracker.h: Scene proxy for occlusion percentage definition.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#ifndef _PRIMITIVESCENEPROXYOCCLUSIONTRACKER_HEADER_
#define _PRIMITIVESCENEPROXYOCCLUSIONTRACKER_HEADER_

/**
 *	An extension of the PrimitiveSceneProxy class that allows for tracking the
 *	percentage of occlusion of an object.
 *	Would be an interface, but it contains data.
 *
 *	IMPORTANT: 
 *  Any scene proxy that utilizes this class to track occlusion values 
 *  ***MUST*** implement implement the FPrimitiveSceneProxy virtual:
 *      virtual FLOAT GetOcclusionPercentage(const FSceneView& View) const
 *   Any scene proxy that utilizes the custom occlusion bounds 
 *  ***MUST*** implement the FPrimitiveSceneProxy virtual
 *      virtual FBoxSphereBounds GetCustomOcclusionBounds() const
 */
class FSceneViewState;

class FPrimitiveSceneProxyOcclusionTracker
{
public:
	/** Initialization constructor. */
	FPrimitiveSceneProxyOcclusionTracker(const UPrimitiveComponent* InComponent);
	/** Virtual destructor. */
	virtual ~FPrimitiveSceneProxyOcclusionTracker();

	virtual DWORD GetMemoryFootprint(void) const { return(sizeof(*this)); }

	/**
	 *	Update the occlusion bounds of the proxy
	 *
	 *	@param	InOcclusionBounds - the new bounds for occlusion
	 */
	void UpdateOcclusionBounds(const FBoxSphereBounds& InOcclusionBounds);

protected:
	/**
	 *	Update the occlusion bounds of the proxy on the render thread
	 *
	 *	@param	InOcclusionBounds - the new bounds for occlusion
	 */
	void UpdateOcclusionBounds_RenderThread(const FBoxSphereBounds& InOcclusionBounds);

	/** 
	 *	Get the results of the last frames occlusion and kick off the one for the next frame
	 *
	 *	@param	PrimitiveComponent - the primitive component being rendered
	 *	@param	PDI - draw interface to render to
	 *	@param	View - current view
	 *	@param	DPGIndex - current depth priority 
	 *	@param	Flags - optional set of flags from EDrawDynamicElementFlags
	 */
	UBOOL UpdateAndRenderOcclusionData(UPrimitiveComponent* PrimitiveComponent, FPrimitiveDrawInterface* PDI,const FSceneView* View,UINT DPGIndex,DWORD Flags);

	struct FCoverageInfo
	{
		FLOAT	Percentage;
		FLOAT	UnmappedPercentage;
		FLOAT	LastSampleTime;

		FCoverageInfo() : 
			  Percentage(0.0f)
			, UnmappedPercentage(0.0f)
			, LastSampleTime(-1.0f)
		{

		}

		UBOOL operator==(const FCoverageInfo& Other) const
		{
			return (
				appIsNearlyZero(Percentage - Other.Percentage) &&
				appIsNearlyZero(UnmappedPercentage - Other.UnmappedPercentage) &&
				appIsNearlyZero(LastSampleTime - Other.LastSampleTime)
				);
		}

		FCoverageInfo& operator=(const FCoverageInfo& Other)
		{
			Percentage = Other.Percentage;
			UnmappedPercentage = Other.UnmappedPercentage;
			LastSampleTime = Other.LastSampleTime;
			return *this;
		}
	};

	/** The occlusion coverage percentage */
	TMap<const FSceneViewState*, FCoverageInfo>	CoverageMap;
	FLOAT	CoveragePercentage;

	/** Bounds for occlusion rendering. */
	FBoxSphereBounds OcclusionBounds;
};

#endif	//_PRIMITIVESCENEPROXYOCCLUSIONTRACKER_HEADER_
