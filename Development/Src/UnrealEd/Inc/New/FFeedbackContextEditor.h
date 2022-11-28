/*=============================================================================
	FFeedbackContextEditor.h: Feedback context tailored to UnrealEd

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

/*-----------------------------------------------------------------------------
	FFeedbackContextEditor.
-----------------------------------------------------------------------------*/

class FFeedbackContextEditor : public FFeedbackContext
{
public:
	INT SlowTaskCount;

	FFeedbackContextEditor()
	: FFeedbackContext()
	, SlowTaskCount(0)
	{}
	void Serialize( const TCHAR* V, EName Event );
	void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow );
	void EndSlowTask();
	void SetContext( FContextSupplier* InSupplier );

	VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );

		return 1;
	}

	VARARG_BODY( UBOOL VARARGS, StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) );


//joegtemp -- This code needs to be replaced with the correct wxWindows equivalent
	DWORD hWndProgressBar, hWndProgressText, hWndProgressDlg, hWndMapCheckDlg;
	void MapCheck_Show()
	{
		SendMessageX( (HWND)hWndMapCheckDlg, WM_COMMAND, WM_MC_SHOW, 0L );
	}
	// This is the same as MapCheck_Show, except it won't display the error box if there are no errors in it.
	void MapCheck_ShowConditionally()
	{
		SendMessageX( (HWND)hWndMapCheckDlg, WM_COMMAND, WM_MC_SHOW_COND, 0L );
	}
	void MapCheck_Hide()
	{
		SendMessageX( (HWND)hWndMapCheckDlg, WM_COMMAND, WM_MC_HIDE, 0L );
	}
	void MapCheck_Clear()
	{
		SendMessageX( (HWND)hWndMapCheckDlg, WM_COMMAND, WM_MC_CLEAR, 0L );
	}
	void MapCheck_Add( INT InType, void* InActor, const TCHAR* InMessage )
	{
		MAPCHECK MC;
		MC.Type = InType;
		MC.Actor = (AActor*)InActor;
		MC.Message = InMessage;
		SendMessageX( (HWND)hWndMapCheckDlg, WM_COMMAND, WM_MC_ADD, (LPARAM)&MC );
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
