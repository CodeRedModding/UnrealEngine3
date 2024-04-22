/*=============================================================================
	ApexComponentBase.uc: PhysX APEX integration. Component Base
	Copyright 2008-2009 NVIDIA Corporation.
=============================================================================*/

class ApexComponentBase extends MeshComponent
	native(Mesh)
	hidecategories(Object)
	editinlinenew;

/** This component's index buffer. */
var protected{protected} const native transient pointer ComponentBaseResources{class FApexBaseResources};

/** A fence used to track when the rendering thread has released the component's resources. */
var protected{protected} native const transient RenderCommandFence_Mirror ReleaseResourcesFence{FRenderCommandFence};

var() const			ApexAsset	Asset;
var() Color			WireframeColor;

var	const			bool		bAssetChanged;

cpptext
{
	public:
		virtual physx::apex::NxApexRenderable *GetApexRenderable(void) const { return 0; }
		
	protected:
		virtual void UpdateApexEditorState(UProperty* PropertyThatChanged = NULL) {}
		
	private:
		friend class UApexAsset;
		// Called when the Asset gets rebuilt (in editor only).
		virtual void OnApexAssetLost(void)  {  }
		virtual void OnApexAssetReset(void) {  }
		
	public:

		//UObject

		//UActorComponent
		virtual UBOOL IsValidComponent() const { return UMeshComponent::IsValidComponent(); }

		// UMeshComponent interface.
		virtual INT                 GetNumElements(void) const;
		virtual UMaterialInterface *GetMaterial(INT MaterialIndex) const;

		//UPrimitiveComponent
		virtual void UpdateTransform();
		virtual void UpdateBounds();
		virtual void InitComponentRBPhys(UBOOL bFixed);
		virtual void TermComponentRBPhys(FRBPhysScene *InScene);
		
		/**
		 * Called after all objects referenced by this object have been serialized. Order of PostLoad routed to 
		 * multiple objects loaded in one set is not deterministic though ConditionalPostLoad can be forced to
		 * ensure an object has been "PostLoad"ed.
		 */
		virtual void PostLoad();

		virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
		void         PostEditMove(UBOOL bFinished);

		/**
		 * Check for asynchronous resource cleanup completion
		 * @return	TRUE if the rendering resources have been released
		 */
		virtual UBOOL IsReadyForFinishDestroy();

	protected:
		/** Attaches the component to the scene, and initializes the component's resources if they have not been yet. */
		virtual void Attach();
		
		/** 
		* Detach the component from the scene and remove its render proxy 
		* @param bWillReattach TRUE if the detachment will be followed by an attachment
		*/
		virtual void Detach(UBOOL bWillReattach = FALSE);

		friend class FApexBaseSceneProxy;
}

defaultproperties
{
	// Various physics related items need to be ticked pre physics update
	TickGroup=TG_PreAsyncWork

	CollideActors=True
	BlockActors=True
	BlockZeroExtent=True
	BlockNonZeroExtent=True
	BlockRigidBody=True
	WireframeColor=(R=255,G=128,B=64,A=255)
}
