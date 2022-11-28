/*=============================================================================
	LoadErrors : Displays the results from a map load that had problems.

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

class WDlgLoadErrors : public WDialog
{
	DECLARE_WINDOWCLASS(WDlgLoadErrors,WDialog,UnrealEd)

	// Variables.
	WButton OKButton;
	WListBox PackageList, ResourceList;

	WDlgLoadErrors( UObject* InContext, WWindow* InOwnerWindow );

	void OnInitDialog();
	virtual void DoModeless( UBOOL bShow );
	void OnClose();
	void Refresh();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/