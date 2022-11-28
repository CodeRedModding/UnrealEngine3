/*=============================================================================
	FFeedbackContextWindows.h: Unreal Windows user interface interaction.
	Copyright 1997-1999 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

/*-----------------------------------------------------------------------------
	FFeedbackContextWindows.
-----------------------------------------------------------------------------*/

//
// Feedback context.
//
class FFeedbackContextWindows : public FFeedbackContext
{
	/** Context information for warning and error messages */
	FContextSupplier*	Context;

public:
	// Variables.
	INT					SlowTaskCount;
	DWORD				hWndProgressBar, hWndProgressText, hWndProgressDlg, hWndMapCheckDlg;

	// Constructor.
	FFeedbackContextWindows()
		: FFeedbackContext()
		, SlowTaskCount( 0 )
		, hWndProgressBar( 0 )
		, hWndProgressText( 0 )
		, hWndProgressDlg( 0 )
		, hWndMapCheckDlg( 0 )
	{}
	void Serialize( const TCHAR* V, EName Event )
	{
		TCHAR Temp[1024]=TEXT("");

		if( Event==NAME_UserPrompt && (GIsClient || GIsEditor) )
		{
			::MessageBox( NULL, V, (TCHAR *)*LocalizeError("Warning",TEXT("Core")), MB_OK|MB_TASKMODAL );
		}
		else if( Event==NAME_Title )
		{
			return;
		}
		else if( Event==NAME_Heading )
		{
			appSprintf( Temp, TEXT("--------------------%s--------------------"), (TCHAR*)V );
			V = Temp;
		}
		else if( Event==NAME_SubHeading )
		{
			appSprintf( Temp, TEXT("%s..."), (TCHAR*)V );
			V = Temp;
		}
		else if( Event==NAME_Error || Event==NAME_Warning || Event==NAME_ExecWarning || Event==NAME_ScriptWarning )
		{
			if( Context )
			{
				appSprintf( Temp, TEXT("%s : %s, %s"), *Context->GetContext(), *FName(Event), (TCHAR*)V );
				V = Temp;
			}
			if(Event == NAME_Error || TreatWarningsAsErrors)
				ErrorCount++;
			else
				WarningCount++;
		}

		if( GLogConsole && GIsUCC )
			GLogConsole->Serialize( V, NAME_None );
		if( !GLog->IsRedirectingTo( this ) )
			GLog->Serialize( V, Event );
	}
	VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		if( GIsClient || GIsEditor )
			return( ::MessageBox( NULL, TempStr, (TCHAR *)*LocalizeError("Question",TEXT("Core")), MB_YESNO|MB_TASKMODAL ) == IDYES);
		else
			return 0;
	}
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
	void BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )
	{
		::ShowWindow( (HWND)hWndProgressDlg, SW_SHOW );
		if( hWndProgressBar && hWndProgressText )
		{
			SendMessageLX( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, Task );
			SendMessageX( (HWND)hWndProgressBar, PBM_SETRANGE, (WPARAM)0, MAKELPARAM(0, 100) );

			UpdateWindow( (HWND)hWndProgressDlg );
			UpdateWindow( (HWND)hWndProgressText );
			UpdateWindow( (HWND)hWndProgressText );

			{	// flush all messages
				MSG mfm_msg;
				while(::PeekMessage(&mfm_msg, (HWND)hWndProgressDlg, 0, 0, PM_REMOVE))
				{
					TranslateMessage(&mfm_msg);
					DispatchMessage(&mfm_msg);
				}
			}
		}
		GIsSlowTask = ++SlowTaskCount>0;
	}
	void EndSlowTask()
	{
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;
		if( !GIsSlowTask )
			::ShowWindow( (HWND)hWndProgressDlg, SW_HIDE );
	}
	VARARG_BODY( UBOOL VARARGS, StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );
		if( GIsSlowTask && hWndProgressBar && hWndProgressText )
		{
			SendMessageLX( (HWND)hWndProgressText, WM_SETTEXT, (WPARAM)0, TempStr );
			SendMessageX( (HWND)hWndProgressBar, PBM_SETPOS, (WPARAM)(Denominator ? 100*Numerator/Denominator : 0), (LPARAM)0 );
			UpdateWindow( (HWND)hWndProgressDlg );
			UpdateWindow( (HWND)hWndProgressText );
			UpdateWindow( (HWND)hWndProgressBar );

			{	// flush all messages
				MSG mfm_msg;
				while(::PeekMessage(&mfm_msg, (HWND)hWndProgressDlg, 0, 0, PM_REMOVE)) {
					TranslateMessage(&mfm_msg);
					DispatchMessage(&mfm_msg);
				}
			}
		}
		return 1;
	}
	void SetContext( FContextSupplier* InSupplier )
	{
		Context = InSupplier;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

