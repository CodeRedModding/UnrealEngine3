/**
 * FracturedBaseComponent.uc - Declaration of the base fractured component which handles rendering with a dynamic index buffer.
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */
class FracturedBaseComponent extends StaticMeshComponent
	native(Mesh)
	abstract;

/** This component's index buffer, used for rendering when bUseDynamicIndexBuffer is true. */
var protected{protected} const native transient pointer ComponentBaseResources{class FFracturedBaseResources};

/** A fence used to track when the rendering thread has released the component's resources. */
var protected{protected} native const transient RenderCommandFence_Mirror ReleaseResourcesFence{FRenderCommandFence};

/** Stores non-zero for each fragment that is visible, and 0 otherwise. */
var protected{protected} transient const array<byte> VisibleFragments;

/** If true, VisibleFragments has changed since the last attach and the dynamic index buffer needs to be updated. */
var protected{protected} transient bool	bVisibilityHasChanged;

/** True if VisibleFragments was reset to bInitialVisibilityValue since the last component attach. */
var protected{protected} transient const bool bVisibilityReset;

/** Initial visibility value for this component. */
var protected{protected} const bool bInitialVisibilityValue;

/** 
 *	If true, each element will be rendered with one draw call by using a dynamic index buffer that is repacked when visibility changes.
 *  If false, each element will be rendered with n draw calls, where n is the number of consecutive index ranges, and there will be no memory overhead.
 */
var protected{protected} const bool bUseDynamicIndexBuffer;

/** 
 *	If true, bUseDynamicIndexBuffer will be enabled when at least one fragment is hidden, otherwise it will be disabled.
 *  If false, bUseDynamicIndexBuffer will not be overridden.
 */
var protected{protected} const bool bUseDynamicIBWithHiddenFragments;

/**
 * Number of indices in the resource's index buffer the last time the component index buffer was built. 
 * Used to detect when the resource's index buffer has changed and the component's index buffer should be rebuilt.
 */
var private{private} const int NumResourceIndices;

/** TRUE whenever the static mesh is being reset during Reattach */
var protected{protected} transient const int bResetStaticMesh;

cpptext
{
public:
	//UObject

	/** Blocks until the component's render resources have been released so that they can safely be modified */
	virtual void PreEditChange(UProperty* PropertyAboutToChange);
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);

	/** 
	 * Signals to the object to begin asynchronously releasing resources
	 */
	virtual void BeginDestroy();

	/**
	 * Check for asynchronous resource cleanup completion
	 * @return	TRUE if the rendering resources have been released
	 */
	virtual UBOOL IsReadyForFinishDestroy();

	//Accessors
	INT GetNumVisibleTriangles() const;
	UBOOL GetInitialVisibilityValue() const;

protected:

	/**
	 * Called after all objects referenced by this object have been serialized. Order of PostLoad routed to 
	 * multiple objects loaded in one set is not deterministic though ConditionalPostLoad can be forced to
	 * ensure an object has been "PostLoad"ed.
	 */
	virtual void PostLoad();

	virtual void InitResources();
	virtual void ReleaseResources();
	void ReleaseBaseResources();

	/** Attaches the component to the scene, and initializes the component's resources if they have not been yet. */
	virtual void Attach();	

	/** Checks if the given fragment is visible. */
	virtual UBOOL IsElementFragmentVisible(INT ElementIndex, INT FragmentIndex, INT InteriorElementIndex, INT CoreFragmentIndex, UBOOL bAnyFragmentsHidden) const;

	/** 
	 * Updates the fragments of this component that are visible.  
	 * @param NewVisibleFragments - visibility factors for this component, corresponding to FracturedStaticMesh's Fragments array
	 * @param bForceUpdate - whether to update this component's resources even if no fragments have changed visibility
	 */
	virtual void UpdateVisibleFragments(const TArray<BYTE>& NewVisibleFragments, UBOOL bForceUpdate);

	/** 
	 * Resets VisibleFragments to bInitialVisibilityValue. 
	 * Does not cause a reattach, so the results won't be propagated to the render thread until the next reattach. 
	 */
	void ResetVisibility();

	/** 
	* Determine if the mesh currently has any hidden fragments
	* @return TRUE if >0 hidden fragments
	*/
	UBOOL HasHiddenFragments() const;

private:

	/** Enqueues a rendering command to update the component's dynamic index buffer. */
	void UpdateComponentIndexBuffer();

	friend class FFracturedBaseSceneProxy;
}

/** 
 * Change the StaticMesh used by this instance, and resets VisibleFragments to all be visible if NewMesh is valid.
 * @param NewMesh - StaticMesh to set.  If this is not also a UFracturedStaticMesh, assignment will fail.
 * @return bool - TRUE if assignment succeeded.
 */
simulated native function bool SetStaticMesh( StaticMesh NewMesh, optional bool bForce );

/** Returns array of currently visible fragments. */
simulated native function array<byte> GetVisibleFragments() const;

/** Returns whether the specified fragment is currently visible or not. */
simulated native function bool IsFragmentVisible(INT FragmentIndex) const;

/** Get the number of chunks in the assigned fractured mesh. */
native function int GetNumFragments() const;

/** Get the number of chunks that are currently visible. */
native function int GetNumVisibleFragments() const;

defaultproperties
{
	bUseDynamicIndexBuffer=true
	bInitialVisibilityValue=true
	bAcceptsDecalsDuringGameplay=FALSE
	bAcceptsStaticDecals=FALSE
	bAcceptsDynamicDecals=TRUE
}
