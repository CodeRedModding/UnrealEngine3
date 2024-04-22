/*=============================================================================
	ApexStaticDestructibleComponent.uc: PhysX APEX integration. Destructible component
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

/***
* This is the base class for static destructible components
*/
class ApexStaticDestructibleComponent extends ApexStaticComponent
	native(Mesh);

/** Increasing this value will cause fracture chunks to be put to sleep more quickly. */
var(Physics)	float SleepEnergyThreshold<ClampMin=0.0>;

/** Increasing this value will cause fracture chunks to be gradually slowed down before putting them to sleep. */
var(Physics)	float SleepDamping<ClampMin=0.0>;

/** The APEX destructible actor (instantiated destructible asset) (duplicatetransients to avoid a pointer copy) */
var native duplicatetransient pointer ApexDestructibleActor   { physx::apex::NxDestructibleActor   };
/** The APEX preview class which can render a preview of a destructible asset */
var native duplicatetransient pointer ApexDestructiblePreview { physx::apex::NxDestructiblePreview };

/** If this component is being used for a thumbnail render */
var native bool bIsThumbnailComponent;


cpptext
{
	protected:
		/** Internal function that updates physics objects to match the RBChannel/RBCollidesWithChannel info. */
		virtual void UpdatePhysicsToRBChannels();

		/** This method handles property changes to the component */
		virtual void UpdateApexEditorState(UProperty* PropertyThatChanged = NULL);

	public:
		/** This method returns the APEX renderable for this destructible component */
		virtual physx::apex::NxApexRenderable *GetApexRenderable(void) const;

		/** Performs a per-frame tick operation on the apex static destructible component */
		virtual void Tick(FLOAT DeltaTime);
		/** Returns true if the component is valid. */
		virtual UBOOL IsValidComponent() const;

		// PrimitiveComponent interface
		/** Performs a LineCheck against this component object.
		*  @param Result : a reference to an FCheckResults class that contains the results.
		*  @param End    : The end of the line check
		*  @param Start  : The start of the line check
		*  @param Extent : The extent of the line check
		*  @param TraceFlags : Defines a bit sequence of which objects are considered for the line check.
		*
		*  @return : Returns TRUE if the LineCheck hit.
		**/
		virtual UBOOL LineCheck(FCheckResult& Result, const FVector& End, const FVector& Start, const FVector& Extent, DWORD TraceFlags);

		/** Performs a single Point inside/outside check against this component.
		*  @param Result : a reference to an FCheckResults class that contains the results.
		*  @param Location : The point location to check against.
		*  @param Extent : The extent of the point check
		*  @param TraceFlags : Defines a bit sequence of which objects are considered for the point check.
		*
		*  @return : Returns TRUE if the point hit the component.
		**/
		virtual UBOOL PointCheck(FCheckResult& Result,const FVector& Location,const FVector& Extent,DWORD TraceFlags);

		/** Initializes the rigid body physics components of this object.
		*
		* @param : bFixed : indicates whether or not the object is fixed
		**/
		virtual void  InitComponentRBPhys(UBOOL bFixed);

		/** Terminates the rigid body components in this object
		*
		* @param InScene : A pointer to the rigid body physics scene.
		**/
		virtual void  TermComponentRBPhys(FRBPhysScene *InScene);

		// StaticMeshComponent interface
		/*** Cooks the convex hull data for this object at a specific scale.
		*
		* @param Level : A pointer to the ULevel this object is contained within.
		* @param TotalScale3D : The scale this object is being cooked for.
		* @param TriByteCount : Reference returns the number of bytes the trianngle data takes.
		* @param TriMeshCount : A reference which returns the number of triangles meshes created
		* @param HullByteCount : A reference which returns the number of bytes the convex hull took up.
		* @param HullCount : A reference which returns the number of hulls which were created.
		*
		**/
		virtual void  CookPhysConvexDataForScale(ULevel* Level, const FVector& TotalScale3D, INT& TriByteCount, INT& TriMeshCount, INT& HullByteCount, INT& HullCount);

		// We will release the ApexRenderable in the destroying phase
		virtual void BeginDestroy();

		virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

		virtual void SetMaterial(int ElementIndex, UMaterialInterface* Material);

		virtual void UpdateRBKinematicData();
	private:
		/***
		* Called when the asset is lost or reset
		**/
		virtual void OnApexAssetLost(void);
		virtual void OnApexAssetReset(void);
}

defaultproperties
{
	SleepEnergyThreshold=1250.0
	SleepDamping=0.2
	bIsThumbnailComponent=FALSE
	bUsePrecomputedShadows=FALSE
}
