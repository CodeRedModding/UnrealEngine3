/*=============================================================================
	UnPostProcess.h: Post process scene rendering definitions.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "PostProcessAA.h"

/**
 * Encapsulates the data which is mirrored to render a post process effect parallel to the game thread.
 */
class FPostProcessSceneProxy
{
public:

	/** 
	 * Initialization constructor. 
	 * @param InEffect - post process effect to mirror in this proxy, 0 if there is no UPostProcessEffect object
	 */
	FPostProcessSceneProxy(const UPostProcessEffect* InEffect);

	/** 
	 * Destructor. 
	 */
	virtual ~FPostProcessSceneProxy()
	{

	}

	/**
	 * Indicate that this is the final post-processing effect in the DPG
	 */
	void TerminatesPostProcessChain(UBOOL bIsFinalEffect) 
	{ 
		FinalEffectInGroup = bIsFinalEffect ? 1 : 0; 
	}

	/**
	 * Render the post process effect
	 * Called by the rendering thread during scene rendering
	 * @param InDepthPriorityGroup - scene DPG currently being rendered
	 * @param View - current view
	 * @param CanvasTransform - same canvas transform used to render the scene
 	 * @param LDRInfo - helper information about SceneColorLDR
	 * @return TRUE if anything was rendered
	 */
	virtual UBOOL Render(const class FScene* Scene, UINT InDepthPriorityGroup,class FViewInfo& View,const FMatrix& CanvasTransform,struct FSceneColorLDRInfo& LDRInfo)
	{ 
		return FALSE; 
	}

	/**
	 * Whether the effect may render to SceneColorLDR or not.
	 * @return TRUE if the effect could potentially render to SceneColorLDR, otherwise FALSE.
	 **/
	virtual UBOOL MayRenderSceneColorLDR() const
	{
		return FALSE;
	}

	/**
	 * Whether the effect will setup a deferred post-process anti-aliasing effect.
	 */
	virtual UBOOL HasPostProcessAA(const FViewInfo& View) const
	{
		return FALSE;
	}

	/** 
	 * Accessor
	 * @return DPG the pp effect should be rendered in
	 */
	FORCEINLINE UINT GetDepthPriorityGroup() const
	{
		return DepthPriorityGroup;
	}

	UBOOL GetAffectsLightingOnly() const
	{
		return bAffectsLightingOnly;
	}

	/**
	 * Overriden by the DepthOfField effect.
	 * @param Params - The parameters for the effect are returned in this struct.
	 * @return whether the data was filled in.
	 */
	virtual UBOOL ComputeDOFParams(const FViewInfo& View, struct FDepthOfFieldParams &Params ) const
	{
		return FALSE;
	}

	/**
	 * Informs the view what to do during pre-pass. Overriden by the motion blur effect.
	 * @param MotionBlurParams	- The parameters for the motion blur effect are returned in this struct.
	 * @return whether the effect needs to have velocities written during pre-pass.
	 */
	virtual UBOOL RequiresVelocities( struct FMotionBlurParams &MotionBlurParams ) const
	{
		return FALSE;
	}

	/**
	 * Tells the view whether to store the previous frame's transforms.
	 */
	virtual UBOOL RequiresPreviousTransforms(const FViewInfo& View) const
	{
		return FALSE;
	}

protected:
	/** DPG the pp effect should be rendered in */
	BITFIELD DepthPriorityGroup : UCONST_SDPG_NumBits;
	/** Whether this is the final post processing effect in the group */
	BITFIELD FinalEffectInGroup : 1;
	/** Whether the effect only affects lighting */
	BITFIELD bAffectsLightingOnly : 1;
};

/**
 * Helper struct to ensure that secondary views (e.g. split-screen) renders the final image in
 * the same SceneColorLDR buffer as the main view (view0).
 * This is done by making sure secondary views always do an even number of ping-pongs, by
 * counting them and let the first effect that renders to SceneColorLDR skip the bufferswap
 * if it's an odd number.
 **/
struct FSceneColorLDRInfo
{
	/** Default constructor */
	FSceneColorLDRInfo()
	:	bAdjustPingPong(FALSE)
	,	NumPingPongsRemaining(0)
	{
	}
	/** If TRUE, the first effect that renders to SceneColorLDR won't swap the SceneColorLDR memory pointers. */
	UBOOL	bAdjustPingPong;
	/** Number of LDR-effects left to render. */
	INT		NumPingPongsRemaining;
};
