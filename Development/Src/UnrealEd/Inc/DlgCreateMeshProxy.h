/*=============================================================================
	DlgCreateMeshProxy.h: Dialog for creating mesh proxies.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DlgCreateMeshProxy_h__
#define __DlgCreateMeshProxy_h__

#if WITH_SIMPLYGON
#include "SimplygonMeshUtilities.h"
#endif // #if WITH_SIMPLYGON

#if ENABLE_SIMPLYGON_MESH_PROXIES

/**
 * WxDlgSimplifyLOD - Helps the user determine a quality setting based on desired view distance
 * and the amount of error allowed.
 */
class WxDlgCreateMeshProxy : public wxDialog
{
public:
	WxDlgCreateMeshProxy( wxWindow* InParent, const TCHAR* DialogTitle );
	virtual ~WxDlgCreateMeshProxy();

	/**
	 * Mark the dialog as dirty and in need of an update
	 */
	void MarkDirty() { bDirty = TRUE; }

	/**
	 * Show or hide the dialog
	 *
	 * @param	bShow	If TRUE, show the dialog; if FALSE, hide it
	 * @return	bool	
	 */
	bool Show(bool bShow);

	/**
	 * Called when an actor has left a group (and possibly moved to another)
	 *
	 * @param	JoinedGroup	The group the actor joined (if any)
	 * @param	LeftGroup	The group the actor left
	 * @param InActor	The actor that has moved (if not specified, it searches the group for a proxy to revert)
	 * @param	bMaintainProxy Special case for when were removing the proxy from the group, but don't want to unmerge it
	 * @param bMaintainGroup Special case for when were reverting the proxy but want to keep it's meshes part of the group
	 * @param bIncProxyFlag If TRUE, modify the flag which indicates this was hidden by the proxy
	 * @return	bool	If the group contained a proxy
	 */
	UBOOL UpdateActorGroup( AGroupActor *JoinedGroup, AGroupActor *LeftGroup, AActor *InActor, const UBOOL bMaintainProxy = FALSE, const UBOOL bMaintainGroup = FALSE, const UBOOL bIncProxyFlag = TRUE );

	/**
	 * Checks the group parsed to see if it can be proxied in any way
	 *
	 * @param	pSelectedGroup	The group to evaluate the actors from, if NULL looks-up selected)
	 * @return	UBOOL TRUE if merge possible
	 */
	UBOOL EvaluateGroup(AGroupActor *pSelectedGroup);

private:

	/**
	 * Spawns the elements which make up the dialog
	 */
	void Setup();

	/**
	 * Clears the result of the group search
	 *
	 * @param	refresh	If TRUE, refresh the browser
	 */
	void Reset(const UBOOL bRefresh = FALSE);

	/**
	 * Show or hide the controls
	 *
	 * @param	show	If TRUE, show the controls; if FALSE, hide them
	 */
	void ShowControls(const bool bShow);

	/**
	 * Show or hide the actors in the group
	 *
	 * @param	bShow		If TRUE, show the actors; if FALSE, hide them
	 * @param bIncProxyFlag If TRUE, modify the flag which indicates this was hidden by the proxy
	 * @param	Actors	An array of actors to either show or hide
	 */
	void ShowActors(const UBOOL bShow, const UBOOL bIncProxyFlag, TArray<AStaticMeshActor*> &Actors) const;

	/**
	 * Event handler for when a the window is idle.
	 *
	 * @param	In	Information about the event.
	 */
	void OnIdle(wxIdleEvent &In);

	/**
	 * Event handler for when a the window is drawn.
	 *
	 * @param	In	Information about the event.
	 */
	void OnPaint(wxPaintEvent &In);

	/**
	 * Loops through the parsed group pulling out the actors we're interested in
	 *
	 * @param	Actors	The actors to evaluate for compatibility
	 * @param	OutP	The proxies actors in the group (if any, typically 0 or 1)
	 * @param	OutVA	The visible actors to merge in the group (if any)
	 * @param	OutHA	The hidden actors to unmerge in the group (if any)
	 * @param	OutVC	The visible components to merge in the group (if any)
	 * @param	OutHC	The hidden components to unmerge in the group (if any)
	 * @return TRUE if a proxy exists
	 */
	UBOOL PopulateActors( TArray<AActor*> &Actors,
																		TArray<AStaticMeshActor*> *OutP,
																		TArray<AStaticMeshActor*> *OutVA,
																		TArray<AStaticMeshActor*> *OutHA,
																		TArray<UStaticMeshComponent*> *OutVC,
																		TArray<UStaticMeshComponent*> *OutHC ) const;

	/**
	 * Grabs the list of all the actors (and their components) for the merge from the parsed group
	 *
	 * @param	pSelectedGroup	The group to extract the actors from, if NULL looks-up selected)
	 * @param	bCanMerge	Whether or not we can merge the meshes in the group
	 * @param	bCanRemerge	Whether or not we can remerge the meshes in the group
	 * @param	bCanUnmerge	Whether or not we can unmerge the meshes in the group
	 * @param	Error	Localized string if an error occurs
	 * @return	UBOOL return TRUE unless an error has occurred
	 */
	UBOOL GetActorsForProxy(AGroupActor *pSelectedGroup, bool &bCanMerge, bool &bCanRemerge, bool &bCanUnmerge, FString &Error);

	/**
	 * When the user has hit the Merge meshes button 
	 * (available only when a merged mesh isn't already present)
	 *
	 * @param	bReset	Should reset members when done
	 */
	void OnMerge(wxCommandEvent& Event);
	UBOOL Merge(const UBOOL bReset);

	/**
	 * When the user has hit the OnRemerge meshes button 
	 * (available only when a merged mesh is already present, and there's non-merged meshes present)
	 *
	 * @param	bReset	Should reset members when done
	 * @param	bAllActors	Should we include all actors, or just the originally merged?
	 */
	void OnRemerge(wxCommandEvent& Event);
public:	// This needs to be public for the 'move between levels' workaround
	UBOOL Remerge(const UBOOL bReset, const UBOOL bAllActors);
private:

	/**
	 * When the user has hit the OnUnmerge meshes button 
	 * (available only when a merged mesh is already present)
	 *
	 * @param	bReset	Should reset members when done
	 */
	void OnUnmerge(wxCommandEvent& Event);
	UBOOL Unmerge(const UBOOL bReset);

	/**
	 * Retrieves/Fetches the chosen on screen size in pixels.
	 */
	INT GetOnScreenSize() const;
	void SetOnScreenSize( const INT iOnScreenSize );

	/**
	 * Retrieves the chosen texture size.
	 */
	INT GetTextureSize() const;
	void SetTextureSize( const INT iTextureSize );

	/**
	 * Retrieves the chosen material type.
	 */
	SimplygonMeshUtilities::EMeshProxyMaterialType GetMaterialType() const;
	void SetMaterialType( const SimplygonMeshUtilities::EMeshProxyMaterialType eMaterialType );

	/**
	 * Retrieves whether to ignore vertex colors.
	 */
	UBOOL ShouldIgnoreVertexColors() const;
	void SetIgnoreVertexColors(UBOOL bShouldIgnoreVertexColors);

	UBOOL bSetup;										// TRUE if the windows controls have been initialized
	UBOOL bDirty;										// TRUE if the window layout needs reevaluating
	wxSize ClientSize;									// The original size of the client window, post setup
	wxButton* MergeButton;
	wxButton* RemergeButton;
	wxButton* UnmergeButton;
	wxSlider* OnScreenSizeSlider;
	WxComboBox* TextureSizeComboBox;
	WxComboBox* MaterialTypeComboBox;
	wxCheckBox* IgnoreVertexColorsCheckBox;
	wxStaticText* ErrorText;
	WxMaskedBitmap SimplygonLogo;
	AGroupActor* SelectedGroup;							// The group which the actors belong to
	AStaticMeshActor* Proxy;							// The static mesh proxy that need to be unmerged
	TArray<wxControl*> AllControls;						// All the control objects in the window (inc those above)
	TArray<AStaticMeshActor*> ActorsToMerge;			// All the static mesh actors that need to be merged
	TArray<AStaticMeshActor*> ActorsToUnmerge;			// All the hidden static mesh actors that need to be unmerged
	TArray<UStaticMeshComponent*> ComponentsToMerge;	// All the static mesh components that need to be merged
	TArray<UStaticMeshComponent*> ComponentsToUnmerge;	// All the hidden static mesh components that need to be unmerged

	DECLARE_EVENT_TABLE()
};

#endif // #if ENABLE_SIMPLYGON_MESH_PROXIES

#endif // #ifndef __DlgCreateMeshProxy_h__
