/*=============================================================================
	WEditorFrame : Temp file until I can kill this class entirely

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

class WEditorFrame : public WWindow, public FNotifyHook
{
	DECLARE_WINDOWCLASS(WEditorFrame,WWindow,UnrealEd)

	WEditorFrame();

	void OnCreate();
	void OpenWindow();

	virtual void OnTimer();
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
