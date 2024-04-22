/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "EngineSequenceClasses.h"
#include "FFeedbackContextEditor.h"
#include "DlgMapCheck.h"
#include "DlgBuildProgress.h"
#include "DlgLightingResults.h"
#include "SplashScreen.h"

const FLOAT FFeedbackContextEditor::UIUpdateGatingTime = 1.0f / 60.0f;

FFeedbackContextEditor::FFeedbackContextEditor() : 
	bIsPerformingMapCheck(FALSE), 
	bIsPerformingLightingBuild(FALSE),
	DialogRequestCount(0), 
	DlgProgress(NULL), 
	SlowTaskCount(0)
{
	DialogRequestStack.Reserve(15);
}

void FFeedbackContextEditor::Serialize( const TCHAR* V, EName Event )
{
	if( !GLog->IsRedirectingTo( this ) )
	{
		GLog->Serialize( V, Event );
	}
}

/**
 * Tells the editor that a slow task is beginning
 * 
 * @param Task					The description of the task beginning.
 * @param ShowProgressDialog	Whether to show the progress dialog.
 */
void FFeedbackContextEditor::BeginSlowTask( const TCHAR* Task, UBOOL bShowProgressDialog, UBOOL bShowCancelButton )
{
	//make sure we're not starting a new FFeedbackContext instance within another, 
	//	this will do unkind things with global variables
	check(!GIsSlowTask || SlowTaskCount > 0);

	if( GApp && GApp->EditorFrame )
	{
		GIsSlowTask = ++SlowTaskCount>0;
		GSlowTaskOccurred = GIsSlowTask;

		// Show a wait cursor for long running tasks
		if (SlowTaskCount == 1)
		{
			// NOTE: Any slow tasks that start after the first slow task is running won't display
			//   status text unless 'StatusUpdatef' is called
			StatusMessage.StatusText = Task;

			::wxBeginBusyCursor();

			// Inhibit screen saver from kicking in as it might mess with the task. We do it for all slow tasks and not
			// just the ones that need it as chained slow tasks might rely on the screen saver not being enabled.
			GEngine->EnableScreenSaver( FALSE );
		}

		DialogRequestStack.Push(bShowProgressDialog);
		if( bShowProgressDialog && ++DialogRequestCount == 1 )
		{
			// Don't show the progress dialog if the Build Progress dialog is already visible
			if( !GApp->DlgBuildProgress->IsShown() )
			{
				// Create the dialog - delete any existing dialog first
				if( DlgProgress )
				{
					delete DlgProgress;
				}
				DlgProgress = new WxDlgProgress( GApp->EditorFrame, bShowCancelButton );

				DlgProgress->ShowDialog();
				GCallbackEvent->Send( CALLBACK_EditorPreModal );
				DlgProgress->MakeModal( TRUE );
				DlgProgress->SetStatusText( *StatusMessage.StatusText );
				::wxSafeYield( DlgProgress, TRUE );
			}
		}
	}
}

/** Whether or not the user has canceled out of this dialog */
UBOOL FFeedbackContextEditor::ReceivedUserCancel( void )
{
	return DlgProgress ? DlgProgress->ReceivedUserCancel() : FALSE;
}

/**
 * Tells the editor that the slow task is done.
 */
void FFeedbackContextEditor::EndSlowTask()
{
#if !_DEBUG
	// This is a soft fail introduced for TTP 222144.  It will prevent release users from loosing their changes in the event of EndSlowTask being called without a BeginSlowTask pair.
	if(SlowTaskCount == 0 && GIsSlowTask == FALSE)
	{
		return;
	} 
#endif

	if( GApp && GApp->EditorFrame )
	{
		check(SlowTaskCount>0);
		GIsSlowTask = --SlowTaskCount>0;

		checkSlow(DialogRequestStack.Num()>0);
		const UBOOL bWasDialogRequest = DialogRequestStack.Pop();
		
		// Restore the cursor now that the long running task is done
		if (SlowTaskCount == 0)
		{
			// Stop inhibiting screen-saver from coming on.
			GEngine->EnableScreenSaver( TRUE );

			::wxEndBusyCursor();

			// Reset cached message
			StatusMessage.StatusText = TEXT( "" );
			StatusMessage.ProgressNumerator = StatusMessage.SavedNumerator = 0;
			StatusMessage.ProgressDenominator = StatusMessage.SavedDenominator = 0;
			StatusMessage.LastUpdateTime  = appSeconds();
		}

		if( bWasDialogRequest && (DialogRequestCount > 0) )
		{
			if( (--DialogRequestCount == 0) && (DlgProgress != NULL) )
			{
				// Hide the window
				DlgProgress->SetProgressPercent(100,100);
				if( DlgProgress->IsShown() )
				{
					// NOTE: It's important to turn off modalness before hiding the window, otherwise a background
					//		 application may unexpectedly be promoted to the foreground, obscuring the editor.  This
					//		 is due to how MakeModal works in WxWidgets (disabling/enabling all other top-level windows.)
					DlgProgress->MakeModal( FALSE );
					GCallbackEvent->Send( CALLBACK_EditorPostModal );
					DlgProgress->Show( FALSE );
					::wxSafeYield( DlgProgress, TRUE );
				}
			}
		}
	}
}

void FFeedbackContextEditor::SetContext( FContextSupplier* InSupplier )
{
}

VARARG_BODY( UBOOL VARARGS, FFeedbackContextEditor::StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) )
{
	// don't update too frequently
	if( GIsSlowTask )
	{
		// limit to update to once every 'y' seconds
		DOUBLE Now = appSeconds();
		DOUBLE Diff = Now - StatusMessage.LastUpdateTime;
		if ( Diff >= UIUpdateGatingTime )
		{
			StatusMessage.LastUpdateTime = Now;
		}
		else
		{
			return TRUE;
		}
	}

	// start with a small buffer size, since status update messages usually aren't very long
	INT		BufferSize	= 256;
	TCHAR*	Buffer		= NULL;
	INT		Result		= -1;

	while ( Result == -1 )
	{
		Buffer = (TCHAR*)appRealloc(Buffer, BufferSize * sizeof(TCHAR));
		GET_VARARGS_RESULT( Buffer, BufferSize, BufferSize-1, Fmt, Fmt, Result );
		BufferSize *= 2;
	};
	Buffer[Result] = 0;

	// Cache the new status message and progress
	StatusMessage.StatusText = Buffer;
	appFree( Buffer );
	Buffer = NULL;

	if( Numerator >= 0 ) // Ignore if set to -1
	{
		StatusMessage.ProgressNumerator = StatusMessage.SavedNumerator = Numerator;
	}
	if( Denominator >= 0 ) // Ignore if set to -1
	{
		StatusMessage.ProgressDenominator = StatusMessage.SavedDenominator = Denominator;
	}

	if( GIsSlowTask )
	{
		GApp->StatusUpdateProgress(
			*StatusMessage.StatusText,
			StatusMessage.ProgressNumerator,
			StatusMessage.ProgressDenominator,
			StatusMessageStack.Num() == 0 || DialogRequestCount == 0
			);

		if( ( DialogRequestCount > 0 ) && (DlgProgress != NULL) )
		{
			if( DlgProgress->IsShown() )
			{
				DlgProgress->SetStatusText( *StatusMessage.StatusText );
				DlgProgress->SetProgressPercent( StatusMessage.ProgressNumerator, StatusMessage.ProgressDenominator );
				::wxSafeYield( DlgProgress, TRUE );
			}
		}
	}

	// Also update the splash screen text (in case we're in the middle of starting up)
	appSetSplashText( SplashTextType::StartupProgress, *StatusMessage.StatusText );
	return TRUE;
}


/**
 * Updates the progress amount without changing the status message text
 *
 * @param Numerator		New progress numerator
 * @param Denominator	New progress denominator
 */
void FFeedbackContextEditor::UpdateProgress( INT Numerator, INT Denominator )
{
	// Cache the new progress
	if( Numerator >= 0 ) // Ignore if set to -1
	{
		StatusMessage.ProgressNumerator = Numerator;
	}
	if( Denominator >= 0 ) // Ignore if set to -1
	{
		StatusMessage.ProgressDenominator = Denominator;
	}

	if( GIsSlowTask )
	{
		// calculate our previous percentage and our new one
		FLOAT SavedRatio = (FLOAT)StatusMessage.SavedNumerator / (FLOAT)StatusMessage.SavedDenominator;
		FLOAT NewRatio = (FLOAT)StatusMessage.ProgressNumerator / (FLOAT)StatusMessage.ProgressDenominator;
		DOUBLE Now = appSeconds();

		// update the progress bar if we've moved enough since last time, or we are going to start or end of bar,
		// or if a second has passed
		if (Abs<FLOAT>(SavedRatio - NewRatio) > 0.1f || Numerator == 0 || (Numerator != -1 && Numerator >= (Denominator - 1)) ||
			Now >= StatusMessage.LastUpdateTime + UIUpdateGatingTime)
		{
			StatusMessage.SavedNumerator = StatusMessage.ProgressNumerator;
			StatusMessage.SavedDenominator = StatusMessage.ProgressDenominator;
			StatusMessage.LastUpdateTime = Now;
			GApp->StatusUpdateProgress(
				NULL,
				StatusMessage.ProgressNumerator,
				StatusMessage.ProgressDenominator,
				StatusMessageStack.Num() == 0 || DialogRequestCount == 0
				);

			if( ( DialogRequestCount > 0 ) && (DlgProgress != NULL) )
			{
				if( DlgProgress->IsShown() )
				{
					DlgProgress->SetProgressPercent( StatusMessage.ProgressNumerator, StatusMessage.ProgressDenominator );
					::wxSafeYield( DlgProgress, TRUE );
				}
			}
		}
	}
}



/** Pushes the current status message/progress onto the stack so it can be restored later */
void FFeedbackContextEditor::PushStatus()
{
	// Push the current message onto the stack.  This doesn't change the current message though.  You should
	// call StatusUpdatef after calling PushStatus to update the message.
	StatusMessageStack.AddItem( StatusMessage );
}



/** Restores the previously pushed status message/progress */
void FFeedbackContextEditor::PopStatus()
{
	if( StatusMessageStack.Num() > 0 )
	{
		// Pop from stack
		StatusMessageStackItem PoppedStatusMessage = StatusMessageStack.Pop();

		// Update the message text
		StatusMessage.StatusText = PoppedStatusMessage.StatusText;

		// Only overwrite progress if the item on the stack actually had some progress set
		if( PoppedStatusMessage.ProgressDenominator > 0 )
		{
			StatusMessage.ProgressNumerator = PoppedStatusMessage.ProgressNumerator;
			StatusMessage.ProgressDenominator = PoppedStatusMessage.ProgressDenominator;
			StatusMessage.SavedNumerator = PoppedStatusMessage.SavedNumerator;
			StatusMessage.SavedDenominator = PoppedStatusMessage.SavedDenominator;
			StatusMessage.LastUpdateTime = PoppedStatusMessage.LastUpdateTime;
		}

		// Update the GUI!
		if( GIsSlowTask )
		{
			StatusMessage.LastUpdateTime = appSeconds();
			StatusMessage.SavedNumerator = StatusMessage.ProgressNumerator;
			StatusMessage.SavedDenominator = StatusMessage.ProgressDenominator;
			GApp->StatusUpdateProgress(
				*StatusMessage.StatusText,
				StatusMessage.ProgressNumerator,
				StatusMessage.ProgressDenominator);

			if( ( DialogRequestCount > 0 ) && (DlgProgress != NULL) )
			{
				if( DlgProgress->IsShown() )
				{
					DlgProgress->SetStatusText( *StatusMessage.StatusText );
					DlgProgress->SetProgressPercent( StatusMessage.ProgressNumerator, StatusMessage.ProgressDenominator );
					DlgProgress->Update();
					::wxSafeYield( DlgProgress, TRUE );
				}
			}
		}
	}
}



/**
 * @return	TRUE if a map check is currently active.
 */
UBOOL FFeedbackContextEditor::MapCheck_IsActive() const
{
	return bIsPerformingMapCheck;
}

void FFeedbackContextEditor::MapCheck_Show()
{
	WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
	check(MapCheck);
	MapCheck->Show( true );
}

/**
 * Same as MapCheck_Show, except it won't display the map check dialog if there are no errors in it.
 */
void FFeedbackContextEditor::MapCheck_ShowConditionally()
{
	WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
	check(MapCheck);
	MapCheck->ShowConditionally();
}

/**
 * Hides the map check dialog.
 */
void FFeedbackContextEditor::MapCheck_Hide()
{
	WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
	check(MapCheck);
	MapCheck->Show( false );
}

/**
 * Clears out all errors/warnings.
 */
void FFeedbackContextEditor::MapCheck_Clear()
{
	WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
	check(MapCheck);
	MapCheck->ClearMessageList();
}

/**
 * Called around bulk MapCheck_Add calls.
 */
void FFeedbackContextEditor::MapCheck_BeginBulkAdd()
{
	WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
	check(MapCheck);
	MapCheck->FreezeMessageList();
	bIsPerformingMapCheck = TRUE;
}

/**
 * Called around bulk MapCheck_Add calls.
 */
void FFeedbackContextEditor::MapCheck_EndBulkAdd()
{
	bIsPerformingMapCheck = FALSE;
	WxDlgMapCheck* MapCheck = GApp->GetDlgMapCheck();
	check(MapCheck);
	MapCheck->ThawMessageList();
}

/**
 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 * @param	InActor					Actor associated with the message; can be NULL.
 * @param	InMessage				The message to display.
 * @param	InUDNPage				[opt] UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage. 
 */
void FFeedbackContextEditor::MapCheck_Add(MapCheckType InType, UObject* InActor, const TCHAR* InMessage, const TCHAR* InUDNPage, MapCheckGroup InGroup)
{
	// check that InType is one of the MCTYPEs ... and that nobody is doing anything "crazy" like trying to combine them ( MCTYPE_ERROR | MCTYPE_WARNING, for example )
	check( ( InType & ( InType - 1 ) ) == 0 ); // this mystical check will make sure that InType is of the form (1 << A)
	WxDlgMapCheck::AddItem( InType, InGroup, InActor, InMessage, InUDNPage );
}

/**
 * @return	TRUE if a lighting build is currently active.
 */
UBOOL FFeedbackContextEditor::LightingBuild_IsActive() const
{
	return bIsPerformingLightingBuild;
}

void FFeedbackContextEditor::LightingBuild_Show()
{
	WxDlgLightingResults* LightingResultsWindow = GApp->GetDlgLightingResults();
	check(LightingResultsWindow);
	LightingResultsWindow->Show( true );
}

/**
 * Shows the dialog only if there are warnings or errors to display.
 */
void FFeedbackContextEditor::LightingBuild_ShowConditionally()
{
	WxDlgLightingResults* LightingResultsWindow = GApp->GetDlgLightingResults();
	check(LightingResultsWindow);
	LightingResultsWindow->ShowConditionally();
}

/**
 * Hides the LightingBuild dialog.
 */
void FFeedbackContextEditor::LightingBuild_Hide()
{
	WxDlgLightingResults* LightingResultsWindow = GApp->GetDlgLightingResults();
	check(LightingResultsWindow);
	LightingResultsWindow->Show( false );
}

/**
 * Clears out all errors/warnings.
 */
void FFeedbackContextEditor::LightingBuild_Clear()
{
	WxDlgLightingResults* LightingResultsWindow = GApp->GetDlgLightingResults();
	check(LightingResultsWindow);
	LightingResultsWindow->ClearMessageList();
}

/**
 * Called around bulk LightingBuild_Add calls.
 */
void FFeedbackContextEditor::LightingBuild_BeginBulkAdd()
{
//	GApp->DlgLightingResults->FreezeMessageList();
	bIsPerformingLightingBuild = TRUE;
}

/**
 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
 *
 * @param	InType					The message type (error/warning/...).
 * @param	InGroup					The message group (kismet/mobile/...).
 * @param	InActor					Actor associated with the message; can be NULL.
 * @param	InMessage				The message to display.
 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage.
 */
void FFeedbackContextEditor::LightingBuild_Add( MapCheckType InType, UObject* InObject, const TCHAR* InMessage, const TCHAR* InUDNPage, MapCheckGroup InGroup)
{
	WxDlgLightingResults::AddItem(InType, InObject, InMessage, InUDNPage, InGroup);
}

/**
 * Called around bulk MapCheck_Add calls.
 */
void FFeedbackContextEditor::LightingBuild_EndBulkAdd()
{
	bIsPerformingLightingBuild = FALSE;
//	GApp->DlgLightingResults->ThawMessageList();
}

/** Refresh the lighting build list. */
void FFeedbackContextEditor::LightingBuild_Refresh()
{
	WxDlgLightingResults* LightingResultsWindow = GApp->GetDlgLightingResults();
	check(LightingResultsWindow);
	LightingResultsWindow->RefreshWindow();
}

/**
 *	Lighting Build Info handlers
 */
/**
 * @return	TRUE if the LightingBuildInfo is currently active
 */
UBOOL FFeedbackContextEditor::LightingBuildInfo_IsActive() const
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	return LightingBuildInfoWindow->IsShown();
}

void FFeedbackContextEditor::LightingBuildInfo_Show()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->Show(1);
}

/** Hides the LightingBuildInfo dialog. */
void FFeedbackContextEditor::LightingBuildInfo_Hide()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->Show(0);
}

/** Clears out all errors/warnings.*/
void FFeedbackContextEditor::LightingBuildInfo_Clear()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->ClearMessageList();
}

/** Called around bulk *_Add calls.*/
void FFeedbackContextEditor::LightingBuildInfo_BeginBulkAdd()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->FreezeMessageList();
}

/**
 * Adds a message to the LightingBuildInfo dialog, to be displayed when the dialog is shown.
 *
 *	@param	InObject	Actor associated with the message; can be NULL.
 *	@param	InTime		The time taken to light this object.
 *	@param	InUnmappedPercentage	The unmapped texel percentage for this object, -1.f for 
 *	@param	InUnmappedMemory		The memory taken up by unmapped texels for this object
 *	@param	InTotalTexelMemory				The memory consumed by all texels for this object.
 */
void FFeedbackContextEditor::LightingBuildInfo_Add(UObject* InObject, DOUBLE InTime, FLOAT InUnmappedPercentage, 
	INT InUnmappedMemory, INT InTotalTexelMemory)
{
	WxDlgLightingBuildInfo::AddItem(InObject, InTime, InUnmappedPercentage, InUnmappedMemory, InTotalTexelMemory);
}

/** Called around bulk *_Add calls. */
void FFeedbackContextEditor::LightingBuildInfo_EndBulkAdd()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->ThawMessageList();
}

/** Refresh the lighting times list. */
void FFeedbackContextEditor::LightingBuildInfo_Refresh()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->RefreshWindow();
}

/** Clears out all static mesh lighting info.*/
void FFeedbackContextEditor::LightingBuildInfoList_Clear()
{
	WxDlgLightingBuildInfo* LightingBuildInfoWindow = GApp->GetDlgLightingBuildInfo();
	check(LightingBuildInfoWindow);
	LightingBuildInfoWindow->ClearMessageList();
}
