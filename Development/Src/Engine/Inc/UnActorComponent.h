/*=============================================================================
	UnActorComponent.h: Actor component definitions.
	Copyright 2003 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	UActorComponent
//

class UActorComponent : public UComponent
{
	DECLARE_ABSTRACT_CLASS(UActorComponent,UComponent,0,Engine);
public:

	struct FScene*	Scene;
	class AActor*	Owner;
	BITFIELD		Initialized;

	// UObject interface.

	virtual void Destroy();
	virtual void PreEditChange();
	virtual void PostEditChange(UProperty* PropertyThatChanged);

	// IsValidComponent - Returns whether this component can be safely created.

	virtual UBOOL IsValidComponent() const { return 1; }

	// SetParentToWorld - Updates the component's transform, given the parent's transform.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld) {}

	// Created - Initializes runtime data for this component.  Requires IsValidComponent() == true.

	virtual void Created();

	// Update - Updates runtime data for this component.  Requires Initialized == true.

	virtual void Update();

	// Destroyed - Frees runtimes data for this component.  Requires Initialized == true.

	virtual void Destroyed();

	// BeginPlay - Starts gameplay for this component.  Requires Initialized == true.

	virtual void BeginPlay();

	// Tick - Advances time.  Requires Initialized == true.

	virtual void Tick(FLOAT DeltaTime);

	// CacheLighting - Called when the level is being built to allow this actor to cache static lighting information.

	virtual void CacheLighting() {}

	// InvalidateLightingCache - Called when this actor component has moved, allowing it to discard statically cached lighting information.

	virtual void InvalidateLightingCache() {}

	// Precache - Allows a component to cache resources which it may use for rendering.

	virtual void Precache(FPrecacheInterface* P) { check(Initialized); }
};

//
//	EPrimitiveLayerMask
//

enum EPrimitiveLayerMask
{
	PLM_Background	= 0x01,
	PLM_Opaque		= 0x02,
	PLM_Translucent	= 0x04,
	PLM_Foreground	= 0x08,

	PLM_DepthSorted		= PLM_Opaque | PLM_Translucent
};

//
//	FPrimitiveViewInterface
//

struct FPrimitiveViewInterface
{
	virtual ~FPrimitiveViewInterface() {}
	virtual class UPrimitiveComponent* GetPrimitive() = 0;

	// FinalizeView - Called when the renderer is done with this view.  Independent views should delete themselves.

	virtual void FinalizeView() = 0;

	// GetLayerMask - Returns the layers this primitive needs to be rendered on.

	virtual DWORD GetLayerMask(const struct FSceneContext& Context) const { return PLM_Opaque; }

	// RenderBackground - Called before rendering normal objects, used to render primitives like the grid which is always in the background.
	//	Only called if GetLayerMask()&PLM_Background.

	virtual void RenderBackground(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI) {}

	// RenderForeground - Called after rendering normal objects, used to render primitives like UI elements that are always rendered in front of the normal scene primitives.
	//	Only called if GetLayerMask()&PLM_Foreground.

	virtual void RenderForeground(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI) {}

	// Render - Renders the primitive normally.
	//	Only called if GetLayerMask()&PLM_DepthSorted.

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI) {}

	// RenderShadowVolume - Renders the shadow volume defined by this primitive and a specific light.

	virtual void RenderShadowVolume(const struct FSceneContext& Context,struct FShadowRenderInterface* PRI,class ULightComponent* Light) {}

	// RenderShadowDepth - Renders the shadow-casting geometry of this primitive, for use in shadow depth buffering.

	virtual void RenderShadowDepth(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI) { Render(Context,PRI); }

	// RenderHitTest - Renders the parts of this primitive that can be clicked on in the editor, with associated hit proxies.  By default calls Render with a hit-proxy for the owning actor.
	//	Only called if GetLayerMask()&PLM_DepthSorted.

	virtual void RenderHitTest(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);

	// RenderBackgroundHitTest - Like RenderHitTest, but for the background.

	virtual void RenderBackgroundHitTest(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);

	// RenderForegroundHitTest - Like RenderHitTest, but for the foreground.

	virtual void RenderForegroundHitTest(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
};

//
//	FStaticLightPrimitive
//

struct FStaticLightPrimitive
{
public:

	// GetSource - Returns the primitive component this statically lit primitive is from.

	virtual class UPrimitiveComponent* GetSource() = 0;

	// Render

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveViewInterface* PVI,struct FStaticLightPrimitiveRenderInterface* PRI) = 0;
};

//
//	ULightComponent
//

class ULightComponent : public UActorComponent
{
	DECLARE_ABSTRACT_CLASS(ULightComponent,UActorComponent,0,Engine)
public:

	/** The head node of a linked list of dynamic primitives affected by this light. */
	TDoubleLinkedList<class UPrimitiveComponent*>* FirstDynamicPrimitiveLink;

	/** The head node of a linked list of static primitives affected by this light. */
	TDoubleLinkedList<FStaticLightPrimitive*>* FirstStaticPrimitiveLink;

	FMatrix WorldToLight;
	FMatrix LightToWorld;

	FLOAT					Brightness;
	FColor					Color;
	class ULightFunction*	Function;

	BITFIELD				CastShadows : 1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD				RequireDynamicShadows : 1;
	BITFIELD				bEnabled : 1;

	// AffectsSphere - Tests whether a bounding sphere is hit by the light.

	virtual UBOOL AffectsSphere(const FSphere& Sphere) { return 1; }

	// GetBoundingBox - Returns the world-space bounding box of the light's influence.

	virtual FBox GetBoundingBox() const { return FBox(FVector(-HALF_WORLD_MAX,-HALF_WORLD_MAX,-HALF_WORLD_MAX),FVector(HALF_WORLD_MAX,HALF_WORLD_MAX,HALF_WORLD_MAX)); }

	// GetDirection

	FVector GetDirection() const { return FVector(WorldToLight.M[0][2],WorldToLight.M[1][2],WorldToLight.M[2][2]); }

	// GetOrigin

	FVector GetOrigin() const { return LightToWorld.GetOrigin(); }

	// GetPosition - Returns the homogenous position of the light.

	virtual FPlane GetPosition() const = 0;

	/**
	 * Returns True if a light cannot move or be destroyed during gameplay, and can thus cast static shadowing.
	 */
	UBOOL HasStaticShadowing() const;

	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Update();
	virtual void Destroyed();

	virtual void InvalidateLightingCache();
};

//
//	FCachedShadowVolume
//

struct FCachedShadowVolume
{
	class ULightComponent*	Light;
	FShadowIndexBuffer		IndexBuffer;
};

enum ERadialImpulseFalloff
{
	RIF_Constant = 0,
	RIF_Linear = 1
};

/**
 * Keeps track of the linked list nodes used to attach a light to a primitive, for fast detachment.
 */
class FPrimitiveLightAttachment
{
public:

	/**
	 * Dynamic primitive constructor.  Adds a primitive to a light's dynamic primitive list.
	 */
	FPrimitiveLightAttachment(ULightComponent* InLight,UPrimitiveComponent* InPrimitive);

	/**
	 * Static primitive constructor.  Adds a primitive to a light's static primitive list.
	 */
	FPrimitiveLightAttachment(ULightComponent* InLight,FStaticLightPrimitive* InStaticPrimitive);

	/**
	 * Destructor.  Removes the primitive from the light's dynamic primitive or static primitive list.
	 */
	~FPrimitiveLightAttachment();

	// Accessors.
    
	ULightComponent* GetLight() const { return Light; }

 private:

	/** The light affecting the primitive. */
	ULightComponent* Light;

	/** The linked list node for this primitive in the light's dynamic primitive list. */
	TDoubleLinkedList<class UPrimitiveComponent*> DynamicPrimitiveLink;

	/** The linked list node for this primitive in the light's static primitive list. */
	TDoubleLinkedList<FStaticLightPrimitive*> StaticPrimitiveLink;
};

/**
 * Specifies what static lighting information is available for a light attached to a primitive.
 */
enum EStaticLightingAvailability
{
	SLA_Unavailable, // There is no static shadowing information available, use dynamic lighting.
	SLA_Shadowed, // There is static shadowing information, but this primitive is entirely shadowed.
	SLA_Available // This is static shadowing information.
};

//
//	UPrimitiveComponent
//

class UPrimitiveComponent : public UActorComponent, protected FPrimitiveViewInterface
{
	DECLARE_ABSTRACT_CLASS(UPrimitiveComponent,UActorComponent,0,Engine);
public:

	FLOAT								LocalToWorldDeterminant;
	FMatrix								LocalToWorld;

	/** The lights which are attached to this primitive. */
	TIndirectArray<FPrimitiveLightAttachment> Lights;

	TArrayNoInit<FCachedShadowVolume*>	CachedShadowVolumes;

	UPrimitiveComponent*	ShadowParent;
	UPrimitiveComponent*	FirstShadowChild;
	UPrimitiveComponent*	NextShadowChild;

	FBoxSphereBounds	Bounds;

	FLOAT CullDistance;

	BITFIELD	HiddenGame:1 GCC_PACK(4);
	BITFIELD	HiddenEditor:1;
	BITFIELD	CastShadow:1;
	BITFIELD	bOwnerNoSee:1;
	BITFIELD	bOnlyOwnerSee:1;

	BITFIELD	CollideActors:1;
	BITFIELD	BlockActors:1;
	BITFIELD	BlockZeroExtent:1;
	BITFIELD	BlockNonZeroExtent:1;
	BITFIELD	BlockRigidBody:1;

	BITFIELD	AlwaysLoadOnClient:1;
	BITFIELD	AlwaysLoadOnServer:1;

	BITFIELD							bWasSNFiltered:1;
	TArrayNoInit<class FOctreeNode*>	OctreeNodes;
	INT									OctreeTag;
	FZoneSet							ZoneMask;

	class UClass*				PhysicalMaterialOverride;
	class URB_BodyInstance*		BodyInstance;

	// Should this Component be in the Octree for collision
	UBOOL ShouldCollide() const;

	// GetCachedShadowVolume

	FCachedShadowVolume* GetCachedShadowVolume(ULightComponent* Light);

	// AttachLight/DetachLight - Called by the scene to add or remove a light relevant to this primitive component.  Requires Initialized == true.

	void AttachLight(ULightComponent* Light);
	void DetachLight(ULightComponent* Light);

	/**
	 * Returns True if a primitive cannot move or be destroyed during gameplay, and can thus cast and receive static shadowing.
	 */
	UBOOL HasStaticShadowing() const;

	/**
	 * Provides the static light primitive for a specific light, if one is available.  Override in classes which support caching static lighting.
	 *
	 * @param Light						The light to return the static light primitive for.
	 * @param OutStaticLightPrimitive	If the return value is SLA_Available, this pointer is set to the FStaticPrimitive implementing the static lighting render interface.
	 *
	 * @return Whether static lighting information is available.
	 */
	virtual EStaticLightingAvailability GetStaticLightPrimitive(ULightComponent* Light,FStaticLightPrimitive*& OutStaticLightPrimitive) { return SLA_Unavailable; }

	// InvalidateLightingCache - Called when statically cached lighting is no longer valid.

	virtual void InvalidateLightingCache(ULightComponent* Light) {}

	// Visible - Based on the view's show flags, determines whether this primitive component should be rendered.

	virtual UBOOL Visible(struct FSceneView* View);

	// CreationTime - Returns the time of the primitive component's creation.

	FLOAT CreationTime();

	// SelectedColor - Returns BaseColor if the actor is selected, BaseColor * 0.5 otherwise.

	FColor SelectedColor(const FSceneContext& Context,FColor BaseColor) const;

	// SelectedMaterial - Returns Material->MaterialResources[1] if the actor is selected, Material->MaterialResources[0] otherwise.

	FMaterial* SelectedMaterial(const FSceneContext& Context,UMaterialInstance* Material) const;

	// UpdateBounds - Sets Bounds.  Default behavior is a bounding box/sphere the size of the world.

	virtual void UpdateBounds();

	// GetZoneMask - Sets ZoneMask to the zones in Model containing the primitive.

	virtual void GetZoneMask(UModel* Model);

	// Collision tests.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent) { return 1; }
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags) { return 1; }

	// Rigid-Body Physics
	
	class URB_BodySetup* FindRBBodySetup();
	void AddImpulse(FVector Impulse, FVector Position = FVector(0,0,0), FName BoneName = NAME_None, UBOOL bVelChange=false);
	virtual void AddRadialImpulse(const FVector& Origin, FLOAT Radius, FLOAT Strength, BYTE Falloff, UBOOL bVelChange=false);
	void AddForce(FVector Force, FVector Position = FVector(0,0,0), FName BoneName = NAME_None);
	virtual void SetRBLinearVelocity(const FVector& NewVel, UBOOL bAddToCurrent=false);
	virtual void SetRBPosition(const FVector& NewPos, const FRotator& NewRot);
	virtual void WakeRigidBody( FName BoneName = NAME_None );
	virtual void UpdateWindForces();
	UBOOL RigidBodyIsAwake( FName BoneName = NAME_None );
	virtual void SetBlockRigidBody(UBOOL bNewBlockRigidBody);

	virtual void UpdateRBKinematicData();

	DECLARE_FUNCTION(execAddImpulse);
	DECLARE_FUNCTION(execAddRadialImpulse);
	DECLARE_FUNCTION(execAddForce);
	DECLARE_FUNCTION(execSetRBLinearVelocity);
	DECLARE_FUNCTION(execSetRBPosition);
	DECLARE_FUNCTION(execWakeRigidBody);
	DECLARE_FUNCTION(execUpdateWindForces);
	DECLARE_FUNCTION(execRigidBodyIsAwake);
	DECLARE_FUNCTION(execSetBlockRigidBody);

	DECLARE_FUNCTION(execSetHidden);

#ifdef WITH_NOVODEX
	virtual class NxActor* GetNxActor(FName BoneName = NAME_None);
#endif // WITH_NOVODEX

	// CreateViewInterface - Returns an interface to a view-specific renderer for the primitive.
	//	By default returns a pointer to the primitive component, which implements FPrimitiveViewInterface.  Requires Initialized == true.

	virtual FPrimitiveViewInterface* CreateViewInterface(const struct FSceneContext& Context)
	{
		check(Initialized);
		return this;
	}

	// FPrimitiveViewInterface interface.

	virtual UPrimitiveComponent* GetPrimitive() { return this; }
	virtual void FinalizeView() {}

	// UActorComponent interface.

	virtual void SetParentToWorld(const FMatrix& ParentToWorld);
	virtual void Created();
	virtual void Update();
	virtual void Destroyed();

	// UObject interface.

	virtual void Serialize(FArchive& Ar);
	virtual void Destroy();
	virtual UBOOL NeedsLoadForClient() const;
	virtual UBOOL NeedsLoadForServer() const;
};

//
//	UMeshComponent
//

class UMeshComponent : public UPrimitiveComponent
{
	DECLARE_ABSTRACT_CLASS(UMeshComponent,UPrimitiveComponent,0,Engine);
public:

	TArrayNoInit<UMaterialInstance*>	Materials;

	// UMeshComponent interface.

	virtual UMaterialInstance* GetMaterial(INT MaterialIndex) const;

	DECLARE_FUNCTION(execGetMaterial)
	DECLARE_FUNCTION(execSetMaterial)
};

//
//	USpriteComponent
//

class USpriteComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(USpriteComponent,UPrimitiveComponent,0,Engine);
public:
	
	UTexture2D*	Sprite;

	// UPrimitiveComponent interface.

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void UpdateBounds();
};

//
//	UBrushComponent
//

class UBrushComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UBrushComponent,UPrimitiveComponent,0,Engine);

	UModel*	Brush;
	struct FBrushWireObject*	WireObject;

	// FPrimitiveViewInterface interface.

	virtual DWORD GetLayerMask(const struct FSceneContext& Context) const;
	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void RenderForeground(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual UBOOL Visible(struct FSceneView* View);

	// UPrimitiveComponent interface.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	virtual void UpdateBounds();
	virtual void GetZoneMask(UModel* Model);

	// UActorComponent interface.

	virtual void Created();
	virtual void Update();
	virtual void Destroyed();

	virtual UBOOL IsValidComponent() const { return Brush != NULL; }
};

//
//	UCylinderComponent
//

class UCylinderComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UCylinderComponent,UPrimitiveComponent,0,Engine);

	FLOAT	CollisionHeight;
	FLOAT	CollisionRadius;

	// UPrimitiveComponent interface.

	virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent);
	virtual UBOOL LineCheck(FCheckResult& Result,const FVector& End,const FVector& Start,const FVector& Extent,DWORD TraceFlags);

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void UpdateBounds();

	// UCylinderComponent interface.

	void SetSize(FLOAT NewHeight, FLOAT NewRadius);

	// Native script functions.

	DECLARE_FUNCTION(execSetSize);
};

//
//	UArrowComponent
//

class UArrowComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UArrowComponent,UPrimitiveComponent,0,Engine);

	FColor	ArrowColor;
	FLOAT	ArrowSize;

	// UPrimitiveComponent interface.

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void UpdateBounds();
};

//
//	UDrawSphereComponent
//

class UDrawSphereComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UDrawSphereComponent,UPrimitiveComponent,0,Engine);

	FColor				SphereColor;
	UMaterialInstance*	SphereMaterial;
	FLOAT				SphereRadius;
	INT					SphereSides;
	BITFIELD			bDrawWireSphere:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD			bDrawLitSphere:1;

	// UPrimitiveComponent interface.

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void UpdateBounds();
};

//
//	UDrawFrustumComponent
//

class UDrawFrustumComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UDrawFrustumComponent,UPrimitiveComponent,0,Engine);

	FColor			FrustumColor;
	FLOAT			FrustumAngle;
	FLOAT			FrustumAspectRatio;
	FLOAT			FrustumStartDist;
	FLOAT			FrustumEndDist;

	// UPrimitiveComponent interface.

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void UpdateBounds();
};

//
//	UCameraConeComponent
//

class UCameraConeComponent : public UPrimitiveComponent
{
	DECLARE_CLASS(UCameraConeComponent,UPrimitiveComponent,0,Engine);

	// UPrimitiveComponent interface.

	virtual void Render(const struct FSceneContext& Context,struct FPrimitiveRenderInterface* PRI);
	virtual void UpdateBounds();
};

//
//	UAudioComponent
//

class USoundCue;
class USoundNode;
class USoundNodeWave;
class UAudioComponent;

struct FWaveInstance
{
	static DWORD TypeHashCounter;

	USoundNodeWave*		WaveData;
	USoundNode*			NotifyFinished;
	UAudioComponent*	AudioComponent;

	FLOAT				Volume,
						Priority,
						Pitch;
	FVector				Velocity,
						Location;
	UBOOL				IsFinished,
						UseSpatialization,
						RequestRestart,
						PotentiallyLooping;
	DWORD				TypeHash;
	INT					NodeIndex;

	FWaveInstance( UAudioComponent* InAudioComponent );
	UBOOL Finished();

	friend FArchive& operator<<( FArchive& Ar, FWaveInstance* WaveInstance );
};


inline DWORD GetTypeHash( FWaveInstance* A ) { return A->TypeHash; }

struct FAudioComponentSavedState
{
	USoundNode*								CurrentNotifyFinished;
	FVector									CurrentLocation;
	FLOAT									CurrentDelay,
											CurrentVolume,
											CurrentPitch;
	UBOOL									CurrentUseSpatialization;

	void Set( UAudioComponent* AudioComponent );
	void Restore( UAudioComponent* AudioComponent );
	
	static void Reset( UAudioComponent* AudioComonent );
};

class UAudioComponent : public UActorComponent
{
	DECLARE_CLASS(UAudioComponent,UActorComponent,0,Engine);

	// Variables.
	class USoundCue*						SoundCue;
	USoundNode*								CueFirstNode;

	BITFIELD								bUseOwnerLocation:1 GCC_PACK(PROPERTY_ALIGNMENT);
	BITFIELD								bAutoPlay:1;
	BITFIELD								bAutoDestroy:1;
	BITFIELD								bFinished:1;
	BITFIELD								bNonRealtime:1;
	BITFIELD								bWasPlaying:1;
	BITFIELD								bAllowSpatialization:1;

	TArray<FWaveInstance*>					WaveInstances GCC_PACK(PROPERTY_ALIGNMENT);
	TArray<BYTE>							SoundNodeData;
	TMap<USoundNode*,UINT>					SoundNodeOffsetMap;
	TMultiMap<USoundNodeWave*,FWaveInstance*> SoundNodeWaveMap;
	TMultiMap<USoundNode*,FWaveInstance*>	SoundNodeResetWaveMap;
	struct FListener*						Listener;

	FLOAT									PlaybackTime;
	FVector									Location;
	FVector									ComponentLocation;

	// Temporary variables for node traversal.
	USoundNode*								CurrentNotifyFinished;
	FVector									CurrentLocation;
	FLOAT									CurrentDelay,
											CurrentVolume,
											CurrentPitch;
	UBOOL									CurrentUseSpatialization;
	INT										CurrentNodeIndex;

	// UObject interface.
	void PostEditChange(UProperty* PropertyThatChanged);
	void Serialize(FArchive& Ar);
	void Destroy();

	// UActorComponent interface.
	void Created();
	void Destroyed();
	void SetParentToWorld(const FMatrix& ParentToWorld);

	// UAudioComponent interface.
	void Play();
	void Stop();
	void UpdateWaveInstances( TArray<FWaveInstance*> &WaveInstances, struct FListener* Listener, FLOAT DeltaTime );

	DECLARE_FUNCTION(execPlay);
    DECLARE_FUNCTION(execPause);
    DECLARE_FUNCTION(execStop);
};

/**
 * Removes a component from the scene for the lifetime of this class.
 *
 * Typically used by constructing the class on the stack:
 * {
 *		FComponentRecreateContext RecreateContext(this);
 *		// The component is removed from the scene here as RecreateContext is constructed.
 *		...
 * }	// The component is returned to the scene here as RecreateContext is destructed.
 */
struct FComponentRecreateContext
{
private:
	UActorComponent*	Component;

public:
	FComponentRecreateContext(UActorComponent* InComponent)
	{
		check(InComponent);
		if((InComponent->Initialized || !InComponent->IsValidComponent()) && InComponent->Scene)
		{
			Component = InComponent;
			if(Component->Initialized)
				Component->Destroyed();
		}
		else
			Component = NULL;
	}
	~FComponentRecreateContext()
	{
		if(Component && Component->IsValidComponent())
		{
			check(Component->Scene);
			Component->Created();
		}
	}
};
