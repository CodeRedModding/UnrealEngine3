
#include "UnrealEd.h"

extern class WxUnrealEdApp* GApp;

void FFeedbackContextEditor::Serialize( const TCHAR* V, EName Event )
{
	if( !GLog->IsRedirectingTo( this ) )
		GLog->Serialize( V, Event );
}

/**
 * Tells the editor that a slow task is beginning
 * 
 * @param Task The description of the task beginning
 * @param StatusWindow Whether to use a status window
 */
void FFeedbackContextEditor::BeginSlowTask( const TCHAR* Task, UBOOL StatusWindow )
{
	GIsSlowTask = ++SlowTaskCount>0;
	// Show a wait cursor for long running tasks
	if (SlowTaskCount == 1)
	{
		::wxBeginBusyCursor();
	}
	GApp->EditorFrame->SetStatusBar( SB_Progress );

	WxStatusBarProgress* sbp = (WxStatusBarProgress*)GApp->EditorFrame->StatusBars[ SB_Progress ];
	if( sbp->GetFieldsCount() > WxStatusBarProgress::FIELD_Description )
		sbp->SetStatusText( TEXT(""), WxStatusBarProgress::FIELD_Description );
	sbp->Gauge->SetValue( 0 );
}

/**
 * Tells the editor that the slow task is done
 */
void FFeedbackContextEditor::EndSlowTask()
{
	check(SlowTaskCount>0);
	GIsSlowTask = --SlowTaskCount>0;
	// Restore the cursor now that the long running task is done
	if (SlowTaskCount == 0)
	{
		::wxEndBusyCursor();
	}
	GApp->EditorFrame->SetStatusBar( SB_Standard );
}

void FFeedbackContextEditor::SetContext( FContextSupplier* InSupplier )
{
}

VARARG_BODY( UBOOL VARARGS, FFeedbackContextEditor::StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) )
{
	TCHAR TempStr[4096];
	GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), Fmt, Fmt );

	if( GIsSlowTask )
	{
		WxStatusBarProgress* sbp = (WxStatusBarProgress*)GApp->EditorFrame->StatusBars[ SB_Progress ];

		if( sbp->Gauge->GetRange() != Denominator )
			sbp->Gauge->SetRange( Denominator );
		sbp->Gauge->SetValue( Numerator );
		if( sbp->GetFieldsCount() > WxStatusBarProgress::FIELD_Description )
		{
			if( sbp->GetStatusText( WxStatusBarProgress::FIELD_Description ) != TempStr )
				sbp->SetStatusText( TempStr, WxStatusBarProgress::FIELD_Description );
		}

		wxSafeYield();
	}
	return 1;
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
