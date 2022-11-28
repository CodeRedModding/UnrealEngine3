/*=============================================================================
	MapCheck : Displays errors/warnings/etc for the map.

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

class WDlgMapCheck : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgMapCheck,WDialog,UnrealEd)

	TMap<DWORD,FWindowAnchor> Anchors;
	FContainer *Container;
	
	HIMAGELIST himl;

	WButton RefreshButton, CloseButton, GotoActorButton, DeleteActorButton;
	WListView ItemList;

	WDlgMapCheck( UObject* InContext, WWindow* InOwnerWindow );

	void OnInitDialog();
	virtual void DoModeless( UBOOL bShow );
	void OnDestroy();
	void OnSize( DWORD Flags, INT NewX, INT NewY );
	void OnCommand( INT Command );
	void OnRefresh();
	void OnClose();
	void OnItemListDblClk();
	void OnDeleteActor();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/