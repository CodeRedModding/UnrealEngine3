/*=============================================================================
	BrowserGroup : Browser window for actor groups

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

// --------------------------------------------------------------
//
// WBrowserGroup
//
// --------------------------------------------------------------

class WBrowserGroup : public WBrowser
{
	DECLARE_WINDOWCLASS(WBrowserGroup,WBrowser,Window)

	TMap<DWORD,FWindowAnchor> Anchors;

	FContainer *Container;
	WCheckListBox *ListGroups;
	HWND hWndToolBar;
	WToolTip *ToolTipCtrl;

	WBrowserGroup( FName InPersistentName, WWindow* InOwnerWindow, HWND InEditorFrame );

	void OpenWindow( UBOOL bChild );
	virtual void UpdateMenu();
	void OnCreate();
	virtual void UpdateAll();
	void OnDestroy();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void GetFromVisibleGroups();
	void SendToVisibleGroups();
	void UpdateGroupList();
	void PositionChildControls( void );
	void UpdateVisibility();
	void OnCommand( INT Command );
	void SelectActorsInGroups( UBOOL Select );
	void NewGroup();
	void DeleteGroup();
	void AddToGroup( FString InGroupName);
	void DeleteFromGroup();
	void RenameGroup();
	void SwapGroupNames( FString Src, FString Dst );
	INT NumActorsSelected();
	void UpdateSelectedGroupList();
	void OnNewGroup();
	void OnDeleteGroup();
	void OnAddToGroup();
	void OnDeleteFromGroup();
	void OnRefreshGroups();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
