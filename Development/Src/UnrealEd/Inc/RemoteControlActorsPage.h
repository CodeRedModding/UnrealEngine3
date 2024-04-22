/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLACTORSPAGE_H__
#define __REMOTECONTROLACTORSPAGE_H__

#include "RemoteControlPage.h"

/**
 * A RemoteController page exposing in-game actor browsing and property editing.
 */
class WxRemoteControlActorsPage : public WxRemoteControlPage
{
public:
	WxRemoteControlActorsPage(FRemoteControlGame *InGame, wxNotebook *InNotebook);

	/**
	 * Return's the page's title, displayed on the notebook tab.
	 */
	virtual const TCHAR *GetPageTitle() const
	{
		return TEXT("Actors");
	}

	/**
	 * Refreshes the page.
	 */
	virtual void RefreshPage(UBOOL bForce = FALSE);

private:
	// typedefs
	struct SubObjectItem
	{
		SubObjectItem() {}
		SubObjectItem(const wxTreeItemId& InActorID, const wxTreeItemId& InObjectID) :
			ActorID(InActorID), ObjectID(InObjectID)
		{
		}
		SubObjectItem(const SubObjectItem& InCopy) 
		{
			ActorID = InCopy.ActorID;
			ObjectID = InCopy.ObjectID;
		}
		wxTreeItemId ActorID;
		wxTreeItemId ObjectID;
	};
	typedef TMap<FString, wxTreeItemId> StringItemMap;
	typedef TMap<FString, SubObjectItem> StringSubObjectItemMap;

	WxToolBar      *ToolBar;
	wxTreeCtrl     *TreeCtrl;
	wxImageList    *TreeImages;
	UBOOL		    bDynamicActors;
	UBOOL		    bDebugSubobjects;
	UBOOL			bAutoExpandActorTree;
	UBOOL			bBuildTreeFlag;

	// helpers
	/**
	 * Refresh helper -- adds a class folder if one doesn't already exist.
	 */
	wxTreeItemId AddClassItem(TMap<FString, wxTreeItemId> &ClassItemMap, const FString &ClassName);

	/**
	 * Set the enabled/disabled status on the toolbar.
	 */
	void UpdateToolUI();

	/**
	 * Builds a tree of sub-objects that have debug options.
	 */
	void BuildSubObjectTree(const wxTreeItemId& ParentID,
		                    const FString& Start,
							StringSubObjectItemMap& SubObjectItemMap);
	/**
	 * Does the heavy lifting involved in building a debug option sub-object tree.
	 */
	UBOOL BuildSubObjectTreeHelper(const wxTreeItemId& ParentID,
		                          const FString& Current,
								  StringSubObjectItemMap& SubObjectItemMap,
								  const wxTreeItemId& ParentActorID,
								  UBOOL bIsRoot);

	/**
	 * Returns the number of tree nodes from an Ancestor node to a Child node.
	 * Returns -1 if Child not related to Ancestor.
	 * Ancestor should NOT be set to Root node.
	 */
	INT  GetDistanceToChild(const wxTreeItemId& AncestorID, wxTreeItemId& ChildID);

	/**
	 * Delete the current branch and prune unneeded sub-objects upwards from delete point.
	 */
	void DeleteBranch(wxTreeItemId Node, StringSubObjectItemMap& ObjectMap);

	/**
	 * Builds the actor tree based on the current settings.
	 */
	void BuildActorTree();

	///////////////////////////////
	// Wx event handlers.

	/**
	 * Called when user presses dynamic actors toggle.
	 */
	void OnToggleDynamicActors(wxCommandEvent &In);

	/**
	 * Called when user presses "debug sub-objects" toggle.
	 */
	void OnToggleDebugSubobjects(wxCommandEvent &In);

	/**
	 * Called when user presses "auto-expand actor tree" toggle.
	 */
	void OnAutoExpandActorTree(wxCommandEvent &In);

	/**
	 * Called when user presses "Actor properties" button.
	 */
	void OnEditActor(wxCommandEvent &In);
	
	/**
	 * Called when user presses "Trace actor properties" button.
	 */
	void OnEditActorTrace(wxCommandEvent &In);

	/**
	 * Called when the tree selection changes.
	 */
	void OnTreeSelChanged(wxTreeEvent &In);

	/**
	 * Called when the user double clicks an item.
	 */
	void OnTreeDoubleClick(wxTreeEvent &In);

	DECLARE_EVENT_TABLE()
};

#endif // __REMOTECONTROLACTORSPAGE_H__
