//=============================================================================
// GroupActor: Collects a group of actors, allowing for management and universal transformation.
// Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
//=============================================================================

class GroupActor extends Actor
	hidedropdown
	native;

var editoronly public{private} bool bLocked;
var editoronly public{private} bool bResetProxy;
var editoronly public{private} bool bRemergeProxy;
var editoronly public{private} byte eMaterialType;
var editoronly public{private} byte eVertexColorMode;
var editoronly public{private} int iOnScreenSize;
var editoronly public{private} int iTextureSize;
var editoronly public{private} array<Actor> GroupActors;
var editoronly public{private} array<GroupActor> SubGroups;

cpptext
{
	virtual void Spawned();
	virtual void PostLoad();
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent);
	virtual void PostScriptDestroyed();
	virtual UBOOL Tick( FLOAT DeltaTime, enum ELevelTick TickType );
	virtual UBOOL IsSelected() const;

	/**
		Checks the group/proxy to make sure it doesn't conflict with other groups or is attempting anything invalid
	 */
	UBOOL ValidateGroup();
	void VerifyProxy( const UBOOL bSkipProxy, const UBOOL bSkipHidden );

	/**
	 * Apply given deltas to all actors and subgroups for this group.
	 * @param	Viewport		The viewport to draw to apply our deltas
	 * @param	InDrag			Delta Transition
	 * @param	InRot			Delta Rotation
	 * @param	InScale			Delta Scale
	 */
	void GroupApplyDelta(FEditorLevelViewportClient* Viewport, const FVector& InDrag, const FRotator& InRot, const FVector& InScale );

	/**
	 * Draw brackets around all selected groups
	 * @param	PDI				FPrimitiveDrawInterface used to draw lines in active viewports
	 * @param	Viewport		The viewport to draw brackets in.
	 * @param	bMustBeSelected	Flag to only draw currently selected groups. Defaults to TRUE.
	 */
	static void DrawBracketsForGroups( FPrimitiveDrawInterface* PDI, FViewport* Viewport, UBOOL bMustBeSelected=TRUE );

	/**
	 * Changes the given array to remove any existing subgroups
	 * @param	GroupArray	Array to remove subgroups from
	 */
	static void RemoveSubGroupsFromArray(TArray<AGroupActor*>& GroupArray);
	
	/**
	 * Returns the highest found root for the given actor or null if one is not found. Qualifications of root can be specified via optional parameters.
	 * @param	InActor			Actor to find a group root for.
	 * @param	bMustBeLocked	Flag designating to only return the topmost locked group.
	 * @param	bMustBeSelected	Flag designating to only return the topmost selected group.
	 * @return	The topmost group actor for this actor. Returns null if none exists using the given conditions.
	 */
	static AGroupActor* GetRootForActor(AActor* InActor, UBOOL bMustBeLocked=FALSE, UBOOL bMustBeSelected=FALSE);

	/**
	 * Returns the direct parent for the actor or null if one is not found.
	 * @param	InActor	Actor to find a group parent for.
	 * @return	The direct parent for the given actor. Returns null if no group has this actor as a child.
	 */
	static AGroupActor* GetParentForActor(AActor* InActor);

	/**
	 * Query to find how many active groups are currently in the editor.
	 * @param	bSelected	Flag to only return currently selected groups (defaults to FALSE).
	 * @param	bDeepSearch	Flag to do a deep search when checking group selections (defaults to TRUE).
	 * @return	Number of active groups currently in the editor.
	 */
	static const INT NumActiveGroups( UBOOL bSelected=FALSE, UBOOL bDeepSearch=TRUE );

	/**
	 * Get the selected group matching the supplied index
	 * @param iGroupIdx		Index of selected groups to gather the actors from.
	 * @param	bDeepSearch	Flag to do a deep search when checking group selections (defaults to TRUE).
	 * @return	Selected group actor, NULL if failed to find it
	 */
	static AGroupActor* GetSelectedGroup( INT iGroupIdx, UBOOL bDeepSearch=TRUE );

	/**
	 * Adds selected ungrouped actors to a selected group. Does nothing if more than one group is selected.
	 */
	static UBOOL AddSelectedActorsToSelectedGroup();

	/**
	 * Removes selected ungrouped actors from a selected group. Does nothing if more than one group is selected.
	 */
	static UBOOL RemoveSelectedActorsFromSelectedGroup();

	/**
	 * Report stats for selected group.
	 */
	static UBOOL ReportStatsForSelectedGroups();

	/**
	 * Report stats for selected actors.
	 */
	static UBOOL ReportStatsForSelectedActors();

	/**
	 * Locks the lowest selected groups in the current selection.
	 */
	static UBOOL LockSelectedGroups();

	/**
	 * Unlocks the highest locked parent groups for actors in the current selection.
	 */
	static UBOOL UnlockSelectedGroups();
	
	/**
	 * Toggle group mode
	 */
	static void ToggleGroupMode();

	/**
	 * Reselects any valid groups based on current editor selection
	 */
	static void SelectGroupsInSelection();
	
	/**
	 * Loops through the active groups and checks to see if any need remerging
	 */
	static void RemergeActiveGroups();

	/**
	 * Lock this group and all subgroups.
	 */
	void Lock();
	
	/**
	 * Unlock this group
	 */
	FORCEINLINE void Unlock()
	{
		bLocked = false;
	};
	
	/**
	 * @return	Group's locked state
	 */
	FORCEINLINE UBOOL IsLocked() const
	{
		return bLocked;
	};

	/**
	 * Indicate that this group needs it's proxy remerging
	 */
	FORCEINLINE void SetRemergeProxy(UBOOL bRemerge)
	{
		bRemergeProxy = bRemerge;
	};

	/**
	 * Get whether this group needs it's proxy remerging
	 */
	FORCEINLINE UBOOL GetRemergeProxy()
	{
		return bRemergeProxy;
	};

	/**
	 * Reset the params which constructed the proxy
	 */
	FORCEINLINE void ResetProxyParams()
	{
		eMaterialType = 255;
		eVertexColorMode = 255;
		iOnScreenSize = -1;
		iTextureSize = -1;
	};

	/**
	 * Set the params which constructed the proxy
	 */
	FORCEINLINE void SetProxyParams(BYTE eMaterialTypeIn, BYTE eVertexColorModeIn, INT iOnScreenSizeIn, INT iTextureSizeIn)
	{
		eMaterialType = eMaterialTypeIn;
		eVertexColorMode = eVertexColorModeIn;
		iOnScreenSize = iOnScreenSizeIn;
		iTextureSize = iTextureSizeIn;
	};

	/**
	 * Get the params which constructed the proxy
	 */
	FORCEINLINE UBOOL GetProxyParams(BYTE &eMaterialTypeOut, BYTE &eVertexColorModeOut, INT &iOnScreenSizeOut, INT &iTextureSizeOut)
	{
		eMaterialTypeOut = eMaterialType;
		eVertexColorModeOut = eVertexColorMode;
		iOnScreenSizeOut = iOnScreenSize;
		iTextureSizeOut = iTextureSize;
		return ( ( eMaterialTypeOut != 255 && eVertexColorModeOut != 255 && iOnScreenSizeOut != -1 && iTextureSizeOut != -1 ) ? TRUE : FALSE );
	};

	/**
	 * @param	InActor	Actor to add to this group
	 * @param	bInformProxy Should the proxy dialog be informed of this change (necessary to stop recursion)
	 */
	void Add(AActor& InActor, const UBOOL bInformProxy = TRUE, const UBOOL bMaintainProxy = FALSE);
	
	/**
	 * Removes the given actor from this group. If the group has no actors after this transaction, the group itself is removed.
	 * @param	InActor	Actor to remove from this group
	 * @param	bInformProxy Should the proxy dialog be informed of this change (necessary to stop recursion & only when it's not moved to another group)
	 * @param	bMaintainProxy Special case for when were removing the proxy from the group, but don't want to unmerge it
	 * @param	bMaintainGroup Special case for when were reverting the proxy but want to keep it's meshes part of the group
	 */
	void Remove(AActor& InActor, const UBOOL bInformProxy = TRUE, const UBOOL bMaintainProxy = FALSE, const UBOOL bMaintainGroup = FALSE);

	/**
	 * @param InActor	Actor to search for
	 * @return True if the group contains the given actor.
	 */
	UBOOL Contains(AActor& InActor) const;

	/**
	 * Searches the group for a proxy
	 * @return The proxy, if found
	 */
	AStaticMeshActor* ContainsProxy() const;

	/**
	 * @param bDeepSearch	Flag to check all subgroups as well. Defaults to TRUE.
	 * @return True if the group contains any selected actors.
	 */
	UBOOL HasSelectedActors(UBOOL bDeepSearch=TRUE) const;

	/**
	 * Detaches all children (actors and subgroups) from this group and then removes it.
	 * @param	bMaintainProxy Special case for when were removing the proxy from the group, but don't want to unmerge it
	 */
	void ClearAndRemove(const UBOOL bMaintainProxy = FALSE);

	/**
	 * Sets this group's location to the center point based on current location of its children.
	 */
	void CenterGroupLocation();
	
	/**
	 * @param	OutGroupActors	Array to fill with all actors for this group.
	 * @param	bRecurse		Flag to recurse and gather any actors in this group's subgroups.
	 */
	void GetGroupActors(TArray<AActor*>& OutGroupActors, UBOOL bRecurse=FALSE) const;

	/**
	 * @param	OutSubGroups	Array to fill with all subgroups for this group.
	 * @param	bRecurse	Flag to recurse and gather any subgroups in this group's subgroups.
	 */
	void GetSubGroups(TArray<AGroupActor*>& OutSubGroups, UBOOL bRecurse=FALSE) const;

	/**
	 * @param	OutChildren	Array to fill with all children for this group.
	 * @param	bRecurse	Flag to recurse and gather any children in this group's subgroups.
	 */
	void GetAllChildren(TArray<AActor*>& OutChildren, UBOOL bRecurse=FALSE) const;
}

defaultproperties
{
	bLocked = true;
	bResetProxy = false;
	bRemergeProxy = false;
	eMaterialType = 255;
	iOnScreenSize = -1;
	iTextureSize = -1;
}
