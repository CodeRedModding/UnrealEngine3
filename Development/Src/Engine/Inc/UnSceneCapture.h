/*=============================================================================
	UnSceneCapture.h: render scenes to texture
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// forward decls
class UTextureRenderTarget;
class UTextureRenderTarget2D;
class FSceneRenderer;
class USkeletalMeshComponent;

/** 
 * Probes added to the scene for rendering captures to a target texture
 * For use on the rendering thread
 */
class FSceneCaptureProbe
{
public:
	
	/** 
	* Constructor (default) 
	*/
	FSceneCaptureProbe(
		const AActor* InViewActor,
		UTextureRenderTarget* InTextureTarget,
		const EShowFlags& InShowFlags,
		const FLinearColor& InBackgroundColor,
		const FLOAT InFrameRate,
		const UPostProcessChain* InPostProcess,
		UBOOL bInUseMainScenePostProcessSettings,
		const UBOOL bInSkipUpdateIfTextureUsersOccluded,
		const UBOOL bInSkipUpdateIfOwnerOccluded,
		const UBOOL bInSkipDepthPrepass,
		const FLOAT InMaxUpdateDist,
		const FLOAT InMaxStreamingUpdateDist,
		const FLOAT InMaxViewDistanceOverride
		)
		:	ViewActor(InViewActor)
		,	ShowFlags(InShowFlags)
		,	TextureTarget(InTextureTarget)
		,	BackgroundColor(InBackgroundColor)
		,	PostProcess(InPostProcess)
		,	bUseMainScenePostProcessSettings(bInUseMainScenePostProcessSettings)
		,	bSkipUpdateIfTextureUsersOccluded(bInSkipUpdateIfTextureUsersOccluded)
		,	bSkipUpdateIfOwnerOccluded(bInSkipUpdateIfOwnerOccluded)
		,	bSkipRenderingDepthPrepass(bInSkipDepthPrepass)
		,	LastCaptureTime(0)
		,	TimeBetweenCaptures(InFrameRate > 0 ? 1/InFrameRate : 0)
		,	MaxUpdateDistSq(Square(InMaxUpdateDist))
		,	MaxStreamingUpdateDistSq(Square(InMaxStreamingUpdateDist))
		,   MaxViewDistanceOverrideSq(Square(InMaxViewDistanceOverride))
	{
	}

	/** 
	* Destructor 
	*/
	virtual ~FSceneCaptureProbe();

	/**
	* Called by the rendering thread to render the scene for the capture
	* @param	MainSceneRenderer - parent scene renderer with info needed 
	*			by some of the capture types.
	*/
	virtual void CaptureScene( FSceneRenderer* MainSceneRenderer ) = 0;

	/** 
	* Determine if a capture is needed based on the given ViewFamily
	* @param ViewFamily - the main renderer's ViewFamily
	* @return TRUE if the capture needs to be updated
	*/
	virtual UBOOL UpdateRequired(const FSceneViewFamily& ViewFamily);

	/**
	 *	Return the texture target associated with this probe
	 *
	 *	@return	UTextureRenderTarget	The texture target
	 */
	const UTextureRenderTarget* GetTextureTarget() const
	{
		return TextureTarget;
	};

	/**
	 *	Return the location of the probe (the actual portal).
	 *
	 *	@return	FVector		The location of the probes ViewActor
	 */
	virtual FVector GetProbeLocation() const;

	/**
	*	Return the location of the ViewActor of the probe.
	*
	*	@param  FVector		The location of the probes ViewActor
	*	@return	TRUE if view actor exist and location is valid
	*/
	virtual UBOOL GetViewActorLocation(FVector & ViewLocation) const;

	/**
	 *	Return the max update distance squared of the probe.
	 *
	 *	@return FLOAT		The MaxUpdateDistSq
	 */
	virtual FLOAT GetMaxUpdateDistSq() const
	{
		return MaxUpdateDistSq;
	}

	/**
	 *	Return the max streaming update distance squared of the probe.
	 *
	 *	@return	FLOAT		The MaxStreamingUpdateDistSq
	 */
	virtual FLOAT GetMaxStreamingUpdateDistSq() const
	{
		return MaxStreamingUpdateDistSq;
	}

	/**
	*	Return the view information update is required or not
	*	When streaming, it needs to consider capture scene view location
	*	If this probe requires that to be considered, return TRUE
	*
	*	@return	UBOOL		TRUE if required
	*/
	virtual UBOOL ViewInfoUpdateRequired() { return TRUE; }

	/**
	*	Callback function to clear anything if dependent 
	*	when PrimitiveComponent is detached
	*/
	virtual void Clear(const UPrimitiveComponent * ComponentToBeDetached) {};

	/**
	*	Callback function to update anything if dependent 
	*	when PrimitiveComponent is reattached
	*/
	virtual void Update(const UPrimitiveComponent * Component) {};

	/**
	 * Sets the given proxies as the post-process proxies to render in the scene.
	 *
	 * @param	InProxies	The proxies to render during the post-process phase. 
	 */
	void SetPostProcessProxies( TArray<class FPostProcessSceneProxy*>& InProxies )
	{
		PostProcessProxies.Empty();
		PostProcessProxies.Append(InProxies);
	}

protected:
	/** The actor which is being viewed from. */
	const AActor* ViewActor;
	/** show flags needed for a scene capture */
	EShowFlags ShowFlags;
	/** render target for scene capture */
	UTextureRenderTarget* TextureTarget;
	/** background scene color */
	FLinearColor BackgroundColor;
	/** The array of post-process proxies created on the game thread from the post process effects. */
	TArray<class FPostProcessSceneProxy*> PostProcessProxies;
	/** Post process chain to be used by this capture */
	const UPostProcessChain* PostProcess;
	/** If TRUE then use the main scene's post process settings when capturing */
	UBOOL bUseMainScenePostProcessSettings;
	/** if true, skip updating the scene capture if the users of the texture have not been rendered recently */
	UBOOL bSkipUpdateIfTextureUsersOccluded;
	/** if true, skip updating the scene capture if the Owner of the component has not been rendered recently */
	UBOOL bSkipUpdateIfOwnerOccluded;
	/** if true, skip the depth prepass when rendering the scene capture */
	UBOOL bSkipRenderingDepthPrepass;
	/** time in seconds when last captured */
	FLOAT LastCaptureTime;
	/** time in seconds between each capture. if == 0 then scene is captured only once */
	FLOAT TimeBetweenCaptures;
	/** if > 0, skip updating the scene capture if the Owner is further than this many units away from the viewer */
	FLOAT MaxUpdateDistSq;
	/** if > 0, skip stream updating the scene capture if the Owner is further than this many units away from the viewer */
	FLOAT MaxStreamingUpdateDistSq;
	/** if > 0, skip rendering of any objects who are further than this many units away from the capture viewer */
	FLOAT MaxViewDistanceOverrideSq;
	/** An array of view states, one for each view, for occlusion.  These are only accessed on the rendering thread. */
	TArray<FSceneViewStateInterface*> ViewStates;
};

/** 
 * Renders a scene to a 2D texture target
 * These can be added to a scene without a corresponding 
 * USceneCaptureComponent since they are not dependent. 
 */
class FSceneCaptureProbe2D : public FSceneCaptureProbe
{
public:

	/** 
	* Constructor (init) 
	*/
	FSceneCaptureProbe2D(
		const AActor* InViewActor,
		UTextureRenderTarget* InTextureTarget,
		const EShowFlags& InShowFlags,
		const FLinearColor& InBackgroundColor,
		const FLOAT InFrameRate,
		const UPostProcessChain* InPostProcess,
		UBOOL bInUseMainScenePostProcessSettings,
		const UBOOL bSkipUpdateIfTextureUsersOccluded,
		const UBOOL bInSkipUpdateIfOwnerOccluded,
		const UBOOL bInSkipDepthPrepass,
		const FLOAT InMaxUpdateDist,
		const FLOAT InMaxStreamingUpdateDist,
		const FLOAT InMaxViewDistanceOverride,
		const FMatrix& InViewMatrix,
		const FMatrix& InProjMatrix
		)
		:	FSceneCaptureProbe(InViewActor,InTextureTarget,InShowFlags,InBackgroundColor,InFrameRate,InPostProcess,bInUseMainScenePostProcessSettings,bSkipUpdateIfTextureUsersOccluded,bInSkipUpdateIfOwnerOccluded,bInSkipDepthPrepass,InMaxUpdateDist,InMaxStreamingUpdateDist,InMaxViewDistanceOverride)
		,	ViewMatrix(InViewMatrix)
		,	ProjMatrix(InProjMatrix)
	{
	}

	/**
	* Called by the rendering thread to render the scene for the capture
	* @param	MainSceneRenderer - parent scene renderer with info needed 
	*			by some of the capture types.
	*/
	virtual void CaptureScene( FSceneRenderer* MainSceneRenderer );

private:
	/** view matrix for capture render */
	FMatrix ViewMatrix;
	/** projection matrix for capture render */
	FMatrix ProjMatrix;
};

/* Hit Mask Material Info for render target texture */
struct FHitMaskMaterialInfo
{
	FVector						MaskLocation;
	FLOAT						MaskRadius; 
	FVector						MaskStartPosition;
	UBOOL						MaskOnlyWhenFacing;
	/* Need this SceneInfo to keep the valid sceneinfo when render thread gets */
	FPrimitiveSceneInfo*		SceneInfo;
		
	FHitMaskMaterialInfo(const FVector & InMaskLocation, const FLOAT InMaskRadius, const FVector & InMaskStartPosition, const UBOOL& InMaskOnlyWhenFacing, FPrimitiveSceneInfo*	InSceneInfo)
		:	MaskLocation(InMaskLocation), 
			MaskRadius(InMaskRadius), 
			MaskStartPosition(InMaskStartPosition), 
			MaskOnlyWhenFacing(InMaskOnlyWhenFacing),
			SceneInfo(InSceneInfo) {}
};

/** 
* Renders a scene to a 2D texture target
* These can be added to a scene without a corresponding 
* USceneCaptureComponent since they are not dependent. 
*/
class FSceneCaptureProbe2DHitMask : public FSceneCaptureProbe
{
public:
	/** 
	* Constructor (init) 
	*/
	FSceneCaptureProbe2DHitMask(
		USkeletalMeshComponent * InMeshComponent,
		UTextureRenderTarget* InTextureTarget, 
		INT	InMaterialIndex,
		INT InForceLOD,
		FLOAT InFadingStartTimeAfterHit, 
		FLOAT InFadingPercentage, 
		FLOAT InFadingDurationTime,
		FLOAT InFadingIntervalTime, 
		FLOAT InMaskCullDistance
		);

	/**
	* Added by the game thread via render thread to add mask
	* @param	HitMask			HitMask information 
	*/
	void AddMask(const FHitMaskMaterialInfo& HitMask, FLOAT CurrentTime);

	/**
	*	Callback function to clear anything if dependent 
	*	when PrimitiveComponent is detached
	*/
	void Clear(const UPrimitiveComponent * ComponentToBeDetached);

	/**
	*	Callback function to update anything if dependent 
	*	when PrimitiveComponent is reattached
	*/
	void Update(const UPrimitiveComponent * Component);

	/** 
	 * Update FadingStarttimesinceHit
	 */
	 void SetFadingStartTimeSinceHit(const FLOAT InFadingStartTimeSinceHit);
	 
	/**
	* Called by the rendering thread to render the scene for the capture
	* @param	MainSceneRenderer - parent scene renderer with info needed 
	*			by some of the capture types.
	*/
	virtual void CaptureScene( FSceneRenderer* MainSceneRenderer );

	/**
	*	Return the view information update is required or not
	*	When streaming, it needs to consider capture scene view location
	*	If this probe requires that to be considered, return TRUE
	*
	*	@return	UBOOL		TRUE if required
	*/
	virtual UBOOL ViewInfoUpdateRequired() { return FALSE; }

private:
	/** Skeletalmesh it renders **/
	USkeletalMeshComponent *	MeshComponent;
	/** Mask List added by game thread **/
	TIndirectArray<FHitMaskMaterialInfo>	MaskList;
	/** Last Time added - this information will be used for fading out **/
	FLOAT LastAddedTime;
	/** Which section to render for mask **/
	INT	MaterialIndex;
	/** Should I force certain LOD for rendering - if -1 == Use Current **/
	INT ForceLOD;
	/** Fading related variable **/
	/** Fading start time after hit - in second - by default 10 seconds **/
	FLOAT FadingStartTimeSinceHit;
	/** What % of color to apply - Range of 0 to 1 **/
	FLOAT FadingPercentage;
	/** Fading duration time since fading starts - in second **/
	FLOAT FadingDurationTime;
	/** Fading interval - in second **/
	FLOAT FadingIntervalTime;
	/** Mask Cull Distance - in real world size. When generating mask, if further than this, it will ignore.**/
	FLOAT MaskCullDistance;
};

/** 
* Renders a scene to a cube texture target
* These can be added to a scene without a corresponding 
* USceneCaptureComponent since they are not dependent. 
*/
class FSceneCaptureProbeCube : public FSceneCaptureProbe
{
public:

	/** 
	* Constructor (init) 
	*/
	FSceneCaptureProbeCube(
		const AActor* InViewActor,
		UTextureRenderTarget* InTextureTarget,
		const EShowFlags& InShowFlags,
		const FLinearColor& InBackgroundColor,
		const FLOAT InFrameRate,
		const UPostProcessChain* InPostProcess,
		UBOOL bInUseMainScenePostProcessSettings,
		const UBOOL bInSkipUpdateIfTextureUsersOccluded,
		const UBOOL bInSkipUpdateIfOwnerOccluded,
		const UBOOL bInSkipPrepass,
		const FLOAT InMaxUpdateDist,
		const FLOAT InMaxStreamingUpdateDist,
		const FLOAT InMaxViewDistanceOverride,
		const FVector& InLocation,
		FLOAT InNearPlane,
		FLOAT InFarPlane
		)
		:	FSceneCaptureProbe(InViewActor,InTextureTarget,InShowFlags,InBackgroundColor,InFrameRate,InPostProcess,bInUseMainScenePostProcessSettings,bInSkipUpdateIfTextureUsersOccluded,bInSkipUpdateIfOwnerOccluded,bInSkipPrepass,InMaxUpdateDist,InMaxStreamingUpdateDist,InMaxViewDistanceOverride)
		,	WorldLocation(InLocation)
		,	NearPlane(InNearPlane)
		,	FarPlane(InFarPlane)
	{
	}

	/**
	* Called by the rendering thread to render the scene for the capture
	* @param	MainSceneRenderer - parent scene renderer with info needed 
	*			by some of the capture types.
	*/
	virtual void CaptureScene( FSceneRenderer* MainSceneRenderer );	

private:

	/**
	* Generates a view matrix for a cube face direction 
	* @param	Face - enum for the cube face to use as the facing direction
	* @return	view matrix for the cube face direction
	*/
	FMatrix CalcCubeFaceViewMatrix( ECubeFace Face );

	/** world position of the cube capture */
	FVector WorldLocation;
	/** for proj matrix calc */
	FLOAT NearPlane;
	/** far plane cull distance used for calculating proj matrix and thus the view frustum */
	FLOAT FarPlane;
};

/** 
* Renders a scene as a reflection to a 2d texture target
* These can be added to a scene without a corresponding 
* USceneCaptureComponent since they are not dependent. 
*/
class FSceneCaptureProbeReflect : public FSceneCaptureProbe
{
public:
	/** 
	* Constructor (init) 
	*/
	FSceneCaptureProbeReflect(
		const AActor* InViewActor,
		UTextureRenderTarget* InTextureTarget,
		const EShowFlags& InShowFlags,
		const FLinearColor& InBackgroundColor,
		const FLOAT InFrameRate,
		const UPostProcessChain* InPostProcess,
		UBOOL bInSkipUpdateIfTextureUsersOccluded,
		UBOOL bInUseMainScenePostProcessSettings,
		const UBOOL bInSkipUpdateIfOwnerOccluded,
		const UBOOL bInSkipPrepass,
		const FLOAT InMaxUpdateDist,
		const FLOAT InMaxStreamingUpdateDist,
		const FLOAT InMaxViewDistanceOverride,
		const FPlane& InMirrorPlane
		)
		:	FSceneCaptureProbe(InViewActor,InTextureTarget,InShowFlags,InBackgroundColor,InFrameRate,InPostProcess,bInUseMainScenePostProcessSettings,bSkipUpdateIfTextureUsersOccluded,bInSkipUpdateIfOwnerOccluded,bInSkipPrepass,InMaxUpdateDist,InMaxStreamingUpdateDist,InMaxViewDistanceOverride)
		,	MirrorPlane(InMirrorPlane)
	{
	}

	/** 
	* Determine if a capture is needed based on the given ViewFamily
	* @param ViewFamily - the main renderer's ViewFamily
	* @return TRUE if the capture needs to be updated
	*/
	virtual UBOOL UpdateRequired( const FSceneViewFamily& ViewFamily );

	/**
	* Called by the rendering thread to render the scene for the capture
	* @param	MainSceneRenderer - parent scene renderer with info needed 
	*			by some of the capture types.
	*/
	virtual void CaptureScene( FSceneRenderer* MainSceneRenderer );	

private:
	/** plane to reflect against */
	FPlane MirrorPlane;
};

/** 
* Renders a scene as if viewed through a sister portal to a 2d texture target
* These can be added to a scene without a corresponding 
* USceneCaptureComponent since they are not dependent. 
*/
class FSceneCaptureProbePortal : public FSceneCaptureProbe
{
public:

	/** 
	* Constructor (init) 
	*/
	FSceneCaptureProbePortal(
		const AActor* InViewActor,
		UTextureRenderTarget* InTextureTarget,
		const EShowFlags& InShowFlags,
		const FLinearColor& InBackgroundColor,
		const FLOAT InFrameRate,
		const UPostProcessChain* InPostProcess,
		UBOOL bInUseMainScenePostProcessSettings,
		const UBOOL bInSkipUpdateIfTextureUsersOccluded,
		const UBOOL bInSkipUpdateIfOwnerOccluded,
		const UBOOL bInSkipPrepass,
		const FLOAT InMaxUpdateDist,
		const FLOAT InMaxStreamingUpdateDist,
		const FLOAT InMaxViewDistanceOverride,
		const FMatrix& InSrcToDestChangeBasisM,
        const AActor* InDestViewActor,
		const FPlane& InClipPlane
		)
		:	FSceneCaptureProbe(InViewActor,InTextureTarget,InShowFlags,InBackgroundColor,InFrameRate,InPostProcess,bInUseMainScenePostProcessSettings,bSkipUpdateIfTextureUsersOccluded,bInSkipUpdateIfOwnerOccluded,bInSkipPrepass,InMaxUpdateDist,InMaxStreamingUpdateDist,InMaxViewDistanceOverride)
		,	SrcToDestChangeBasisM(InSrcToDestChangeBasisM)
		,	DestViewActor(InDestViewActor ? InDestViewActor : InViewActor)
		,	ClipPlane(InClipPlane)
	{
	}

	/**
	 * Called by the rendering thread to render the scene for the capture
	 * @param	MainSceneRenderer - parent scene renderer with info needed 
	 *			by some of the capture types.
	 */
	virtual void CaptureScene( FSceneRenderer* MainSceneRenderer );	

	/**
	 *	Return the location of the ViewActor of the probe.
	 *
	 *	@param  FVector		The location of the probes ViewActor
	 *	@return	TRUE if view actor exist and location is valid
	 */
	virtual UBOOL GetViewActorLocation(FVector & ViewLocation) const;

private:
	/** Transform for source to destination view */
	FMatrix SrcToDestChangeBasisM;	
	/** the destination actor for this portal */
	const AActor* DestViewActor;
	/** plane aligned to destination view location to clip rendering */
	FPlane ClipPlane;
};

/**
* Simple scene capture utility helper class that just renders the given scene capture using the parent view family
*/
class FSceneCaptureProxy
{
public:
	/**
	* Constructor
	*
	* @param InViewport - parent viewport to use for rendering
	* @param InParentViewFamily - view family of parent needed for rendering a scene capture
	*/
	FSceneCaptureProxy(FViewport* InViewport, const FSceneViewFamily* InParentViewFamily);
	/**
	* Render the scene capture probe without relying on regular viewport rendering of the scene
	* Note that this call will Begin/End frame without swapping
	*
	* @param CaptureProbe - the probe to render, will be deleted after rendering is completed on the render thread
	* @param bFlushRendering - TRUE if render commands should be flushed and block for it to finish
	*/
	void Render(FSceneCaptureProbe* CaptureProbe,UBOOL bFlushRendering);
private:
	/** parent viewport to use for rendering */
	FViewport* Viewport;
	/** view family of parent needed for rendering a scene capture */
	const FSceneViewFamily* ParentViewFamily;
};
