/*=============================================================================
	LayerBrowser.h: UnrealEd's layer browser.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __LAYERBROWSER_H__
#define __LAYERBROWSER_H__

// Forward declarations.
class WxListView;

class WxLayerBrowser :
	public WxBrowser,
	public FSerializableObject
{
	DECLARE_DYNAMIC_CLASS(WxLayerBrowser);

public:
	WxLayerBrowser();

	/** Clears references the actor list and references to any actors. */
	void ClearActorReferences();

	/**
	 * Forwards the call to our base class to create the window relationship.
	 * Creates any internally used windows after that
	 *
	 * @param DockID the unique id to associate with this dockable window
	 * @param FriendlyName the friendly name to assign to this window
	 * @param Parent the parent of this window (should be a Notebook)
	 */
	virtual void Create(INT DockID,const TCHAR* FriendlyName,wxWindow* Parent);

	/**
	 * Loops through all the actors in the world and assembles a list of unique layer names.
	 * Actors can belong to multiple layers by separating the layer names with commas.
	 */
	virtual void Update();

	virtual void Activated();

	/** Returns the key to use when looking up values. */
	virtual const TCHAR* GetLocalizationKey() const;

	////////////////////////////////
	// FCallbackEventDevice interface

	virtual void Send(ECallbackEventType Event);

	////////////////////////////////
	// FSerializableObject interface

	/** Serialize actor references held by the browser. */
	virtual void Serialize(FArchive& Ar);

	/** Presents a context menu for layer list items. */
	void OnRightButtonDown( wxMouseEvent& In );

protected:
	/**
	 * When FALSE, we don't process toggle messages for the checked list box.  This is necessary
	 * at certain times when processing things in batch.
	 */
	UBOOL bAllowToggling;

	/**
	 * TRUE if an update was requested while the browser tab wasn't active.
	 * The browser will Update() the next time this browser tab is Activated().
	 */
	UBOOL bUpdateOnActivated;

	wxCheckListBox*		LayerList;
	WxListView*			ActorList;
	wxSplitterWindow*	SplitterWindow;

	TArray<AActor*> ReferencedActors;

	/**
	 * Selects/de-selects actors by layer.
	 * @return		TRUE if at least one actor was selected.
	 */
	UBOOL SelectActors(UBOOL InSelect);

	/** Loops through all actors in the world and updates their visibility based on which layers are checked. */
	void UpdateActorVisibility();

	/** Populates the actor list with actors in the selected layers. */
	void PopulateActorList();

private:
	////////////////////
	// Wx events.

	/** Adds level-selected actors to the selected layers. */
	void OnAddToLayer( wxCommandEvent& In );
	/** Deletes level-selected actors from the selected layers. */
	void OnDeleteSelectedActorsFromLayers( wxCommandEvent& In );
	/** Called when a list item check box is toggles. */
	void OnToggled( wxCommandEvent& In );
	/** Called when item selection in the layer list changes. */
	void OnLayerSelectionChange( wxCommandEvent& In );
	/** Called when a list item in the layer browser is double clicked. */
	void OnDoubleClick( wxCommandEvent& In );
	/** Creates a new layer. */
	void OnNewLayer( wxCommandEvent& In );
	/** Deletes a layer. */
	void OnDeleteLayer( wxCommandEvent& In );
	/** Handler for IDM_RefreshBrowser events; updates the browser contents. */
	void OnRefresh( wxCommandEvent& In );
	/** Selects actors in the selected layers. */
	void OnSelect( wxCommandEvent& In );
	/** De-selects actors in the selected layers. */
	void OnDeselect( wxCommandEvent& In );
	/** Sets all layers to visible. */
	void OnAllLayersVisible( wxCommandEvent& In );
	/** Presents a rename dialog for each selected layers. */
	void OnRename( wxCommandEvent& In );
	/** Responds to size events by updating the splitter. */
	void OnSize(wxSizeEvent& In);
	/** Called when the tile of a column in the actor list is clicked. */
	void OnActorListColumnClicked(wxListEvent& In);
	/** Called when an actor in the actor list is clicked or double-clicked. */
	void OnActorListItemClicked(wxListEvent& In);
	void OnActorListItemDblClicked(wxListEvent& In);
	void ActorListSelectAndFocus(UBOOL bFocus);

	DECLARE_EVENT_TABLE();
};

#endif // __LAYERBROWSER_H__
