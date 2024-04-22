/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __UNEDVIEWPORT_H__
#define __UNEDVIEWPORT_H__

// Forward declarations.
class FEditorCameraController;
class FCameraControllerUserImpulseData;
class FMouseDeltaTracker;
class FWidget;
class FCachedJoystickState;
class WxInterpEd;
class FCameraControllerConfig;
class FObserverInterface;
struct HLevelSocketProxy;

struct FEditorLevelViewportClient : public FViewportClient, public FViewElementDrawer, public FCallbackEventDevice, public FObserverInterface
{
public:
	////////////////////////////
	// Viewport parameters

	FViewport*				Viewport;
	/** Viewport location. */
	FVector					ViewLocation;
	/** The viewport location used for rendering last frame. */
	FVector					LastViewLocation;
	/** Viewport orientation; valid only for perspective projections. */
	FRotator				ViewRotation;
	/** Viewport's horizontal field of view. */
	FLOAT					ViewFOV;
	/** The viewport type. */
	ELevelViewportType		ViewportType;
	FLOAT					OrthoZoom;

	/** The viewport's scene view state. */
	FSceneViewStateInterface* ViewState;

	/** A set of flags that determines visibility for various scene elements. */
	EShowFlags				ShowFlags;

	/** Previous value for show flags, used for toggling. */
	EShowFlags				LastShowFlags;

	/** If TRUE,  we are in Game View mode*/
	UBOOL					bInGameViewMode;

	/** Store the old show flags for lighting settings prior to switching modes so we can use them to switch back on exit */
	EShowFlags				PreviousEdModeVertColorShowFlags;

	/** Special volume actor visibility settings. Each bit represents a visibility state for a specific volume class. 1 = visible, 0 = hidden */
	TBitArray<>				VolumeActorVisibility;

	/** If TRUE, render unlit while viewport camera is moving. */
	UBOOL					bMoveUnlit;

	/** If TRUE, this viewport is being used to previs level streaming volumes in the editor. */
	UBOOL					bLevelStreamingVolumePrevis;

	/** If TRUE, this viewport is being used to previs post process volumes in the editor. */
	UBOOL					bPostProcessVolumePrevis;

	/** If TRUE, the viewport's location and rotation is loced and can only be moved by user input. */
	UBOOL					bViewportLocked;

	/** If FALSE, the far clipping plane of persepective views is at infitiy; if TRUE, the far plane is adjustable. */
	UBOOL					bVariableFarPlane;

	/** If FALCE, the go to actor style focus will not do anything for this viewport. */
	UBOOL					bAllowAlignViewport;

	/** Stores the current ViewMode while moving unlit. */
	EShowFlags				CachedShowFlags;

	/** TRUE if we've forced the SHOW_Lighting show flag off because there are no lights in the scene */
	UBOOL					bForcedUnlitShowFlags;

	/** If TRUE, move any selected actors to the cameras location/rotation. */
	UBOOL					bLockSelectedToCamera;

	/** If FALSE, ambient occlusion is disabled. */
	UBOOL					bAllowAmbientOcclusion;

	/** Blend value for the OverrideProcessSettings settings stored below.  0.f = no override, 1.f = total override */
	FLOAT					OverridePostProcessSettingsAlpha;

	/** The number of frames since this viewport was last drawn.  Only applies to linked orthographic movement. */
	INT						FramesSinceLastDraw;
	
	/** TRUE if this viewport needs redrawing.  Only applies to linked orthographic movement. */
	UBOOL					bNeedsLinkedRedraw;

	/** If TRUE, created by the editor frame as either a main viewport OR a floating viewport */
	UBOOL					bEditorFrameClient;

	/** PostProcess settings to use for this viewport in the editor if OverridePostProcessSettingsAlpha > 0.f. */
	FPostProcessSettings	OverrideProcessSettings;

	/** The last post process settings used for the viewport. */
	FPostProcessSettings	PostProcessSettings;

	/** Rendering overrides for this viewport. */
	FRenderingPerformanceOverrides	RenderingOverrides;

	/** The world space coordinates of the mouse pointer.  These are updated as the mouse moves across the viewport and are accurate for ortho viewports only. */
	FVector MouseWorldSpacePos;

	/** The current position of the mouse in pixel coordinates.  Updated every mouse move.*/
	FIntPoint CurrentMousePos;

	////////////////////////////
	// Maya camera movement

	UBOOL		bAllowMayaCam;
	UBOOL		bUsingMayaCam;
	FRotator	MayaRotation;
	FVector		MayaLookAt;
	FVector		MayaZoom;
	AActor*		MayaActor;

	////////////////////////////

	class UEditorViewportInput*	Input;

	/** Override for the global realtime audio setting.  If this is true the bWantAudioRealtime setting will be used.  If this is false, the global setting will be used. */
	UBOOL					bAudioRealtimeOverride;
	/** If TRUE we want real time audio **/
	UBOOL					bWantAudioRealtime;
	/** If TRUE, the listener position will be set */
	UBOOL					bSetListenerPosition;

	/** The number of pending viewport redraws. */
	UINT NumPendingRedraws;

	/** If TRUE, draw the axis indicators when the viewport is perspective. */
	UBOOL					bDrawAxes;

	/** If TRUE, draw vertices for selected BSP brushes and static meshes if the large vertices show flag is set. */
	UBOOL					bDrawVertices;

	/**
	 * Used for actor drag duplication.  Set to TRUE on Alt+LMB so that the selected
	 * actors will be duplicated as soon as the widget is displaced.
	 */
	UBOOL					bDuplicateActorsOnNextDrag;

	/**
	* bDuplicateActorsOnNextDrag will not be set again while bDuplicateActorsInProgress is TRUE.
	* The user needs to release ALT and all mouse buttons to clear bDuplicateActorsInProgress.
	*/
	UBOOL					bDuplicateActorsInProgress;

	/**
	 * TRUE when within a FMouseDeltaTracker::StartTracking/EndTracking block.
	 */
	UBOOL					bIsTracking;

	/**
	 * TRUE when a brush is being transformed by its Widget
	 */
	UBOOL					bIsTrackingBrushModification;

	/**
	 * TRUE if the user is dragging by a widget handle.
	 */
	UBOOL					bDraggingByHandle;

	/**
	 * TRUE if all input is rejected from this viewport
	 */
	UBOOL					bDisableInput;

	////////////////////////////
	// Aspect ratio
	UBOOL					bConstrainAspectRatio;
	FLOAT					AspectRatio;

	/** near plane adjustable for each editor view */
	FLOAT					NearPlane;

	UBOOL					bEnableFading;
	FLOAT					FadeAmount;
	FColor					FadeColor;

	UBOOL					bEnableColorScaling;
	FVector					ColorScale;

	/** Whether to override material diffuse and specular with constants, used by the Detail Lighting viewmode. */
	UBOOL					bOverrideDiffuseAndSpecular;

	/** Whether to disable material diffuse and set a constant specular color, used by the Image Reflections Only viewmode. */
	UBOOL					bShowReflectionsOnly;

	FWidget*				Widget;
	FMouseDeltaTracker*		MouseDeltaTracker;

	/** TRUE if the widget's axis is being controlled by an active mouse drag. */
	UBOOL					bWidgetAxisControlledByDrag;

	/** Variable holds the scalar value for the camera speed in the viewport */
	FLOAT					CameraSpeed;

	/** List of layers that are hidden in this view */
	TArray<FName>			ViewHiddenLayers;

	/** Index of this view in the editor's list of views */
	INT						ViewIndex;


	/** Describes an object that's currently hovered over in the level viewport */
	struct FViewportHoverTarget
	{
		/** The actor we're drawing the hover effect for, or NULL */
		AActor* HoveredActor;

		/** The BSP model we're drawing the hover effect for, or NULL */
		UModel* HoveredModel;

		/** Surface index on the BSP model that currently has a hover effect */
		UINT ModelSurfaceIndex;


		/** Construct from an actor */
		FViewportHoverTarget( AActor* InActor )
			: HoveredActor( InActor ),
			  HoveredModel( NULL ),
			  ModelSurfaceIndex( INDEX_NONE )
		{
		}

		/** Construct from an BSP model and surface index */
		FViewportHoverTarget( UModel* InModel, INT InSurfaceIndex )
			: HoveredActor( NULL ),
			  HoveredModel( InModel ),
			  ModelSurfaceIndex( InSurfaceIndex )
		{
		}

		/** Equality operator */
		UBOOL operator==( const FViewportHoverTarget& RHS ) const
		{
			return RHS.HoveredActor == HoveredActor &&
				   RHS.HoveredModel == HoveredModel &&
				   RHS.ModelSurfaceIndex == ModelSurfaceIndex;
		}

		friend DWORD GetTypeHash( const FViewportHoverTarget& Key )
		{
			return Key.HoveredActor ? GetTypeHash(Key.HoveredActor) : GetTypeHash(Key.HoveredModel)+Key.ModelSurfaceIndex;
		}

	};

	/** Static: List of objects we're hovering over */
	static TSet< FViewportHoverTarget > HoveredObjects;


private:
	UBOOL bIsRealtime;
	UBOOL bStoredRealtime;
	UBOOL bStoredAudioRealtime;
	/** True if we should draw FPS info over the viewport */
	UBOOL bShowFPS;

	/** True if we should draw stats over the viewport */
	UBOOL bShowStats;

	/** Used for ortho-viewports mouse movement */
	INT LastMouseX;
	INT LastMouseY;

	/** TRUE if squint mode is active */
	UBOOL bUseSquintMode;

	/** TRUE if this is a floating viewport window */
	UBOOL bIsFloatingViewport;

	/** TRUE if this window is allowed to be possessed by Matinee for previewing sequences in real-time */
	UBOOL bAllowMatineePreview;

	/** Camera controller object that's used for piloting the camera around */
	FEditorCameraController* CameraController;

	/** Current cached impulse state */
	FCameraControllerUserImpulseData* CameraUserImpulseData;

	/** Cached state of joystick axes and buttons*/
	TMap<INT, FCachedJoystickState*> JoystickStateMap;

	/** Extra camera speed scale for flight camera */
	FLOAT FlightCameraSpeedScale;

	/** Real time that camera speed scale was changed list */
	DOUBLE RealTimeFlightCameraSpeedScaleChanged;

	/**WxInterpEd, should only be set if used for matinee recording*/
	WxInterpEd* RecordingInterpEd;

	/**Pitch history used for averaging/dampening*/
	TArray<FLOAT> PitchAngleHistory;
	/**Roll history used for averaging/dampening*/
	TArray<FLOAT> RollAngleHistory;

	/** If this view was controlled by another view this/last frame, don't update itself */
	UBOOL bWasControlledByOtherViewport;

	/** When we have LOD locking, it's slow to force redraw of other viewports, so we delay invalidates to reduce the number of redraws */
	DOUBLE TimeForForceRedraw;

	/** Option for whether the viewport should support Play in Viewport */
	UBOOL bAllowPlayInViewport;


	/** Bit array representing the visibility of every sprite category in the current viewport */
	TBitArray<>					SpriteCategoryVisibility;

	/** The socket under the mouse on the last MouseMove event or NULL if none */
	USkeletalMeshComponent* MouseOverSocketOwner;
	INT MouseOverSocketIndex;

protected:
	/** Indicates whether, of not, the base attachment volume should be drawn for this viewport. */
	UBOOL bDrawBaseInfo;

	/** If TRUE, the canvas has been been moved using bMoveCanvas Mode*/
	UBOOL bHasMouseMovedSinceClick;

public:

	/**
	 * Constructor
	 *
	 * @param	ViewportInputClass	the input class to use for creating this viewport's input object
	 */
	FEditorLevelViewportClient( UClass* ViewportInputClass=NULL );

	/**
	 * Destructor
	 */
	virtual ~FEditorLevelViewportClient();

	/** FCallbackEventDevice: Called when a registered global event is fired */
	virtual void Send( ECallbackEventType InType );

	////////////////////////////
	// FViewElementDrawer interface
	virtual void DrawTools(const FSceneView* View,FPrimitiveDrawInterface* PDI);
	virtual void Draw(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	////////////////////////////
	// FViewportClient interface

	/**
	* Converts the character to a suitable FName and has the app check if a global hotkey was pressed.
	* @param	Viewport - The viewport which the keyboard input is from.
	* @param	KeyName - The key that was pressed
	*/
	virtual void CheckIfGlobalHotkey(FViewport* Viewport, FName KeyName);

	virtual void RedrawRequested(FViewport* Viewport);
	virtual void Draw(FViewport* Viewport,FCanvas* Canvas);
	virtual UBOOL InputKey(FViewport* Viewport,INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f,UBOOL bGamepad=FALSE);
	virtual UBOOL InputAxis(FViewport* Viewport,INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad=FALSE);

	/**
	 * Returns TRUE if this viewport is a realtime viewport.
	 * @returns TRUE if realtime, FALSE otherwise.
	 */
	UBOOL GetIsRealtime() const;

	/**
	 * Sets whether or not a controller is actively plugged in
	 * @param InControllID - Unique ID of the joystick
	 * @param bInConnected - TRUE, if the joystick is valid for input
	 */
	virtual void OnJoystickPlugged(const UINT InControllerID, const UINT InType, const UINT bInConnected);

	virtual void GetViewportDimensions( FIntPoint& out_Origin, FIntPoint& out_Size );
	virtual void MouseMove(FViewport* Viewport,INT x, INT y);

	/**
	 * Reset the camera position and rotation.  Used when creating a new level.
	 */
	void ResetCamera();

	/**
	 * Sets the cursor to be visible or not.  Meant to be called as the mouse moves around in "move canvas" mode (not just on button clicks)
	 * returns if the cursor should be visible or not
	 */
	UBOOL UpdateCursorVisibility (void);
	/**
	 * Given that we're in "move canvas" mode, set the snap back visible mouse position to clamp to the viewport
	 */
	void UpdateMousePosition(void);

    /** Enables the real time audio override for this viewport */
	void OverrideRealtimeAudio( UBOOL bOverride ) { bAudioRealtimeOverride = bOverride; }

	/** Sets the audio realtime settings for this viewport. */
	void SetAudioRealtime( UBOOL bAudioRealtime, UBOOL bSaveExisting )
	{
		if( bSaveExisting )
		{
			bStoredAudioRealtime = bWantAudioRealtime;
		}
		bWantAudioRealtime = bAudioRealtime;
	}

	/**
	 * Updates the audio listener for this viewport 
	 *
 	 * @param View	The scene view to use when calculate the listener position
	 */
	void UpdateAudioListener( const FSceneView& View );

	/** Determines if the cursor should presently be visible*/
	UBOOL ShouldCursorBeVisible (void);

	/** Determines if the new MoveCanvas movement should be used */
	UBOOL ShouldUseMoveCanvasMovement (void);

	/** If TRUE, this is an editor frame viewport */
	UBOOL IsEditorFrameClient (void) const { return bEditorFrameClient; }
	/** Sets whether this is an editor frame viewport.  Used for staggering viewport refreshes*/
	void SetEditorFrameClient (const UBOOL bInOnOff) { bEditorFrameClient = bInOnOff; }

	/** True if the window is maximized or floating */
	UBOOL IsVisible() const;

	/**
	 * Called when the mouse is moved while a window input capture is in effect
	 *
	 * @param	InViewport	Viewport that captured the mouse input
	 * @param	InMouseX	New mouse cursor X coordinate
	 * @param	InMouseY	New mouse cursor Y coordinate
	 */
	virtual void CapturedMouseMove( FViewport* InViewport, INT InMouseX, INT InMouseY );


	virtual EMouseCursor GetCursor(FViewport* Viewport,INT X,INT Y);

	////////////////////////////
	// FEditorLevelViewportClient interface

	/**
	 * Configures the specified FSceneView object with the view and projection matrices for this viewport.
	 * @param	View		The view to be configured.  Must be valid.
	 * @return	A pointer to the view within the view family which represents the viewport's primary view.
	 */
	virtual FSceneView* CalcSceneView(FSceneViewFamily* ViewFamily);

	/** Returns true if this viewport is orthogonal. */
	virtual UBOOL IsOrtho() const
	{
		return (ViewportType == LVT_OrthoXY || ViewportType == LVT_OrthoXZ || ViewportType == LVT_OrthoYZ );
	}

	/** Returns true if this viewport is perspective. */
	UBOOL IsPerspective() const
	{
		return (ViewportType == LVT_Perspective);
	}
	
	/**
	 * Returns true if FPS information should be displayed over the viewport
	 *
	 * @return	TRUE if frame rate should be displayed
	 */
	UBOOL ShouldShowFPS() const
	{
		return bShowFPS;
	}


	/**
	 * Sets whether or not frame rate info is displayed over the viewport
	 *
	 * @param	bWantFPS	TRUE if frame rate should be displayed
	 */
	void SetShowFPS( UBOOL bWantFPS )
	{
		bShowFPS = bWantFPS;
	}



	/**
	 * Returns true if status information should be displayed over the viewport
	 *
	 * @return	TRUE if stats should be displayed
	 */
	UBOOL ShouldShowStats() const
	{
		return bShowStats;
	}


	/**
	 * Sets whether or not stats info is displayed over the viewport
	 *
	 * @param	bWantStats	TRUE if stats should be displayed
	 */
	void SetShowStats( UBOOL bWantStats )
	{
		bShowStats = bWantStats;
	}



	/**
	 * Determines if InComponent is inside of InSelBBox.  This check differs depending on the type of component.
	 * If InComponent is NULL, FALSE is returned.
	 *
	 * @param	InActor							Used only when testing sprite components.
	 * @param	InComponent						The component to query.  If NULL, FALSE is returned.
	 * @param	InSelBox						The selection box.
	 * @param	bConsiderOnlyBSP				If TRUE, consider only BSP.
	 * @param	bMustEncompassEntireComponent	If TRUE, the entire component must be encompassed by the selection box in order to return TRUE.
	 */
	UBOOL ComponentIsTouchingSelectionBox( AActor* InActor, UPrimitiveComponent* InComponent, const FBox& InSelBBox, UBOOL bConsiderOnlyBSP, UBOOL bMustEncompassEntireComponent );

	/** 
	 * Returns TRUE if the passed in volume is visible in the viewport (due to volume actor visibility flags)
	 *
	 * @param VolumeActor	The volume to check
	 */
	UBOOL IsVolumeVisibleInViewport( const AActor& VolumeActor ) const;

	/**
	 * Invalidates this viewport and optionally child views.
	 *
	 * @param	bInvalidateChildViews		[opt] If TRUE (the default), invalidate views that see this viewport as their parent.
	 * @param	bInvalidateHitProxies		[opt] If TRUE (the default), invalidates cached hit proxies too.
	 */
	void Invalidate(UBOOL bInvalidateChildViews=TRUE, UBOOL bInvalidateHitProxies=TRUE);


	////////////////////////////
	// Realtime update

	/**
	 * Toggles whether or not the viewport updates in realtime and returns the updated state.
	 *
	 * @return		The current state of the realtime flag.
	 */
	UBOOL ToggleRealtime()
	{ 
		bIsRealtime = !bIsRealtime; 
		return bIsRealtime;
	}

	/** Sets whether or not the viewport updates in realtime. */
	void SetRealtime( UBOOL bInRealtime, UBOOL bStoreCurrentValue = FALSE )
	{ 
		if( bStoreCurrentValue )
		{
			bStoredRealtime = bIsRealtime;
		}
		bIsRealtime	= bInRealtime;
	}

	/** @return		True if viewport is in realtime mode, false otherwise. */
	UBOOL IsRealtime() const				
	{ 
		return bIsRealtime; 
	}

	/**
	 * Restores realtime setting to stored value. This will only enable realtime and 
	 * never disable it (unless bAllowDisable is TRUE)
	 */
	void RestoreRealtime( const UBOOL bAllowDisable = FALSE )
	{
		if( bAllowDisable )
		{
			bIsRealtime = bStoredRealtime;
		}
		else
		{
			bIsRealtime |= bStoredRealtime;
		}
	}

	/** Restores the audio realtime setting to stored value. */
	void RestoreAudioRealtime()
	{
		bWantAudioRealtime = bStoredAudioRealtime;
	}

	/**
	 * Allows custom disabling of camera recoil
	 */
	void SetMatineeRecordingWindow (WxInterpEd* InInterpEd);
	/**
	 * Returns TRUE if camera recoil is currently allowed
	 */
	UBOOL IsMatineeRecordingWindow (void) const
	{
		return (RecordingInterpEd != NULL);
	}

	/*
	 *	Select whether to Allow PIV for this viewport
	 */
	void AllowPlayInViewport( UBOOL bAllow )
	{
		bAllowPlayInViewport = bAllow;
	}

	/*
	 * Returns TRUE if this Vieport supports PIV
	 */
	UBOOL IsPlayInViewportAllowed()
	{
		return bAllowPlayInViewport;
	}

	virtual FSceneInterface* GetScene();
	virtual void ProcessClick(FSceneView* View,HHitProxy* HitProxy,FName Key,EInputEvent Event,UINT HitX,UINT HitY);
	virtual void Tick(FLOAT DeltaSeconds);
	void UpdateMouseDelta();
	/**
	 * forces a cursor update and marks the window as a move has occurred
	 */
	void MarkMouseMovedSinceClick();


	/** Sets whether or not the viewport uses squint mode. */
	void SetSquintMode( UBOOL bNewSquintMode )
	{ 
		bUseSquintMode	= bNewSquintMode;
	}

	/** @return True if viewport is in squint mode, false otherwise. */
	UBOOL IsUsingSquintMode() const				
	{ 
		return bUseSquintMode; 
	}

	/**
	 * Calculates absolute transform for mouse position status text using absolute mouse delta.
	 *
	 * @param bUseSnappedDelta Whether or not to use absolute snapped delta for transform calculations.
	 */
	void UpdateMousePositionTextUsingDelta( UBOOL bUseSnappedDelta );

	/** Determines whether this viewport is currently allowed to use Absolute Movement */
	UBOOL IsUsingAbsoluteTranslation (void);

	/**
	 * Returns the horizontal axis for this viewport.
	 */
	EAxis GetHorizAxis() const;

	/**
	 * Returns the vertical axis for this viewport.
	 */
	EAxis GetVertAxis() const;

	/** Returns true if this viewport is floating */
	UBOOL IsFloatingViewport() const { return bIsFloatingViewport; }

	/** Sets whether or not we're a floating viewport */
	void SetFloatingViewport( const UBOOL bInFloatingViewport )
	{
		bIsFloatingViewport = bInFloatingViewport;
	}

	/** Returns true if the viewport is allowed to be possessed by Matinee for previewing sequences */
	UBOOL AllowMatineePreview() const { return bAllowMatineePreview; }

	/** Sets whether or not this viewport is allowed to be possessed by Matinee */
	void SetAllowMatineePreview( const UBOOL bInAllowMatineePreview )
	{
		bAllowMatineePreview = bInAllowMatineePreview;
	}

	FEditorCameraController* GetCameraController(void) { return CameraController; }

	void InputAxisMayaCam(FViewport* Viewport, const FVector& DragDelta, FVector& Drag, FRotator& Rot);

	/**
	 * Implements screenshot capture for editor viewports.  Should be called by derived class' InputKey.
	 */
	void InputTakeScreenshot(FViewport* Viewport, FName Key, EInputEvent Event);

	/**
	 * Determines which axis InKey and InDelta most refer to and returns a corresponding FVector.  This
	 * vector represents the mouse movement translated into the viewports/widgets axis space.
	 *
	 * @param	InNudge		If 1, this delta is coming from a keyboard nudge and not the mouse
	 */
	virtual FVector TranslateDelta( FName InKey, FLOAT InDelta, UBOOL InNudge );

	/**
	 * Converts a generic movement delta into drag/rotation deltas based on the viewport and keys held down.
	 */
	void ConvertMovementToDragRot( const FVector& InDelta, FVector& InDragDelta, FRotator& InRotDelta );

	void ConvertMovementToDragRotMayaCam(const FVector& InDelta, FVector& InDragDelta, FRotator& InRotDelta);
	void SwitchStandardToMaya();
	void SwitchMayaToStandard();
	void SwitchMayaCam();

	/**
	 * Sets the camera view location such that the MayaLookAt point is at the specified location.
	 */
	void SetViewLocationForOrbiting(const FVector& Loc);

	/**
	 * Moves the viewport camera according to the specified drag and rotation.
	 *
	 * @param bUnlitMovement	If TRUE, go into unlit movement if the level viewport settings permit.
	 */
	void MoveViewportCamera( const FVector& InDrag, const FRotator& InRot, UBOOL bUnlitMovement=TRUE );

	void ApplyDeltaToActors( const FVector& InDrag, const FRotator& InRot, const FVector& InScale );
	void ApplyDeltaToActor( AActor* InActor, const FVector& InDeltaDrag, const FRotator& InDeltaRot, const FVector& InDeltaScale );
	virtual FLinearColor GetBackgroundColor();

	/** Updates the rotate widget with the passed in delta rotation. */
	void ApplyDeltaToRotateWidget( const FRotator& InRot );

	/**
	 * Draws a screen space bounding box around the specified actor
	 *
	 * @param	InCanvas		Canvas to draw on
	 * @param	InView			View to render
	 * @param	InViewport		Viewport we're rendering into
	 * @param	InActor			Actor to draw a bounding box for
	 * @param	InColor			Color of bounding box
	 * @param	bInDrawBracket	True to draw a bracket, otherwise a box will be rendered
	 * @param	bInLabelText	Optional label text to draw
	 */
	void DrawActorScreenSpaceBoundingBox( FCanvas* InCanvas, const FSceneView* InView, FViewport* InViewport, AActor* InActor, const FLinearColor& InColor, const UBOOL bInDrawBracket, const FString& InLabelText = TEXT( "" ) );

	/**
	 * Draws a screen-space box around each Actor in the Kismet sequence of the specified level.
	 */
	void DrawKismetRefs(ULevel* Level, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas);

	/** Draw an Actors text Tag next to it in the viewport. */
	void DrawActorTags(FViewport* Viewport, const FSceneView* View, FCanvas* Canvas);

	void DrawAxes(FViewport* Viewport,FCanvas* Canvas, const FRotator* InRotation = NULL);

	/**
	 *	Draw the texture streaming bounds.
	 */
	void DrawTextureStreamingBounds(const FSceneView* View,FPrimitiveDrawInterface* PDI);

	/** Serialization. */
	friend FArchive& operator<<(FArchive& Ar,FEditorLevelViewportClient& Viewport);
	
	/**
	 * Copies layout and camera settings from the specified viewport
	 *
	 * @param InViewport The viewport to copy settings from
	 */
	void CopyLayoutFromViewport( const FEditorLevelViewportClient& InViewport );

	/**
	 * Set the viewport type of the client
	 *
	 * @param InViewportType	The viewport type to set the client to
	 */
	virtual void SetViewportType( ELevelViewportType InViewportType );

	/**
	 * Static: Adds a hover effect to the specified object
	 *
	 * @param	InHoverTarget	The hoverable object to add the effect to
	 */
	static void AddHoverEffect( FEditorLevelViewportClient::FViewportHoverTarget& InHoverTarget );

	/**
	 * Static: Removes a hover effect to the specified object
	 *
	 * @param	InHoverTarget	The hoverable object to remove the effect from
	 */
	static void RemoveHoverEffect( FViewportHoverTarget& InHoverTarget );

	/**
	 * Static: Clears viewport hover effects from any objects that currently have that
	 */
	static void ClearHoverFromObjects();

	// FObserverInterface interface
	virtual FVector	GetObserverViewLocation()
	{
		return ViewLocation;
	}

	/**
	 * Returns whether the provided unlocalized sprite category is visible in the viewport or not
	 *
	 * @param	UnlocalizedCategory	The unlocalized sprite category name
	 *
	 * @return	TRUE if the specified category is visible in the viewport; FALSE if it is not
	 */
	UBOOL GetSpriteCategoryVisibility( const FName& UnlocalizedCategory ) const;

	/**
	 * Returns whether the sprite category specified by the provided index is visible in the viewport or not
	 *
	 * @param	Index	Index of the sprite category to check
	 *
	 * @return	TRUE if the category specified by the index is visible in the viewport; FALSE if it is not
	 */
	UBOOL GetSpriteCategoryVisibility( INT Index ) const;

	/**
	 * Sets the visibility of the provided unlocalized category to the provided value
	 *
	 * @param	UnlocalizedCategory	The unlocalized sprite category name to set the visibility of
	 * @param	bVisible			TRUE if the category should be made visible, FALSE if it should be hidden
	 */
	void SetSpriteCategoryVisibility( const FName& UnlocalizedCategory, UBOOL bVisible );

	/**
	 * Sets the visibility of the category specified by the provided index to the provided value
	 *
	 * @param	Index		Index of the sprite category to set the visibility of
	 * @param	bVisible	TRUE if the category should be made visible, FALSE if it should be hidden
	 */
	void SetSpriteCategoryVisibility( INT Index, UBOOL bVisible );

	/**
	 * Sets the visibility of all sprite categories to the provided value
	 *
	 * @param	bVisible	TRUE if all the categories should be made visible, FALSE if they should be hidden
	 */
	void SetAllSpriteCategoryVisibility( UBOOL bVisible );

	/**
	 * Moves the viewport camera to the given point, offset based on the given point and current facing direction of the viewport camera
	 *
	 * @param	NewTargetLocation	Location to move the viewport camera
	 * @param	TargetActor			Target actor to view (used for better calculating of camera offset)
	 */
	void TeleportViewportCamera(FVector NewTargetLocation, const AActor* TargetActor = NULL);

protected:
	
	/**
	 * Checks to see if the current input event modified any show flags.
	 * @param Key				Key that was pressed.
	 * @param bControlDown		Flag for whether or not the control key is held down.
	 * @param bAltDown			Flag for whether or not the alt key is held down.
	 * @param bShiftDown		Flag for whether or not the shift key is held down.
	 * @return					Flag for whether or not we handled the input.
	 */
	virtual UBOOL CheckForShowFlagInput(FName Key, UBOOL bControlDown, UBOOL bAltDown, UBOOL bShiftDown);

private:

	/** Returns true if perspective flight camera input mode is currently active in this viewport */
	UBOOL IsFlightCameraInputModeActive() const;

	/** Returns true if we're currently allowed to update the 'Mouse position' status bar text at this time */
	UBOOL CanUpdateStatusBarText() const
	{
		// Make sure the 'camera speed changed' display is visible for awhile before we clear it
		return ( appSeconds() - RealTimeFlightCameraSpeedScaleChanged ) > 2.0f;
	}

	/** 
	 * Updates any orthographic viewport movement to use the same location as this viewport
	 *
	 * @param bInvalidate	TRUE to invalidate viewports which will be required if this function is called outside of Draw()
	 */
	void UpdateLinkedOrthoViewports( UBOOL bInvalidate = FALSE );

	/** Moves a perspective camera */
	void MoveViewportPerspectiveCamera( const FVector& InDrag, const FRotator& InRot, UBOOL bUnlitMovement );

	/**Applies Joystick axis control to camera movement*/
	void UpdateCameraMovementFromJoystick(const UBOOL bRelativeMovement, FCameraControllerConfig& InConfig);

	/**
	 * Updates real-time camera movement.  Should be called every viewport tick!
	 *
	 * @param	DeltaTime	Time interval in seconds since last update
	 */
	void UpdateCameraMovement( FLOAT DeltaTime );

	/**
	 * Forcibly disables lighting show flags if there are no lights in the scene, or restores lighting show
	 * flags if lights are added to the scene.
	 */
	void UpdateLightingShowFlags();

};

///////////////////////////////////////////////////////////////////////////////

/**
 * Contains information about a mouse cursor position within a viewport, transformed into the correct coordinate
 * system for the viewport.
 */
struct FViewportCursorLocation
{
public:
	FViewportCursorLocation( const class FSceneView* View, FEditorLevelViewportClient* ViewportClient, INT X, INT Y );

	const FVector&		GetOrigin()			const	{ return Origin; }
	const FVector&		GetDirection()		const	{ return Direction; }
	const FIntPoint&	GetCursorPos()		const	{ return CursorPos; }
	ELevelViewportType	GetViewportType()	const	{ return ViewportType; }
private:
	FVector				Origin,
						Direction;
	FIntPoint			CursorPos;
	ELevelViewportType	ViewportType;
};

struct FViewportClick : public FViewportCursorLocation
{
public:
	FViewportClick( const class FSceneView* View, FEditorLevelViewportClient* ViewportClient, FName InKey, EInputEvent InEvent, INT X, INT Y );

	/** @return The 2D screenspace cursor position of the mouse when it was clicked. */
	const FIntPoint&	GetClickPos()	const	{ return GetCursorPos(); }
	const FName&		GetKey()		const	{ return Key; }
	EInputEvent			GetEvent()		const	{ return Event; }

	UBOOL	IsControlDown()	const			{ return ControlDown; }
	UBOOL	IsShiftDown()	const			{ return ShiftDown; }
	UBOOL	IsAltDown()		const			{ return AltDown; }

private:
	FName		Key;
	EInputEvent	Event;
	UBOOL		ControlDown,
				ShiftDown,
				AltDown;
};

#endif // __UNEDVIEWPORT_H__
