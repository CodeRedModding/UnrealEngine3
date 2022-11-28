/*=============================================================================
	DlgSearchActors : Searches for actors using various criteria

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

class WDlgSearchActors : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgSearchActors,WDialog,UnrealEd)

	// Variables.
	WButton CloseButton, GotoActorButton, DeleteActorButton;
	WListBox ActorList;
	WEdit NameEdit, EventEdit, TagEdit;

	WDlgSearchActors( UObject* InContext, WWindow* InOwnerWindow );

	void OnInitDialog();
	virtual void OnShowWindow( UBOOL bShow );
	virtual void DoModeless( UBOOL bShow );
	void RefreshActorList( void );
	void OnClose();
	void OnActorListDblClick();
	void OnDeleteActor();
	void OnNameEditChange();
	void OnEventEditChange();
	void OnTagEditChange();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
