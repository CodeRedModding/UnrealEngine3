/*=============================================================================
	FFeedbackContextEditor.h: Feedback context tailored to UnrealEd

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __FFEEDBACKCONTEXTEDITOR_H__
#define __FFEEDBACKCONTEXTEDITOR_H__

#include "DlgProgress.h"



/**
 * A FFeedbackContext implementation for use in UnrealEd.
 */
class FFeedbackContextEditor : public FFeedbackContext
{
	/** minimum time between progress updates displayed - more frequent updates are ignored */
	static const FLOAT UIUpdateGatingTime;

	UBOOL			bIsPerformingMapCheck;
	UBOOL			bIsPerformingLightingBuild;
	/** the number of times a caller requested the progress dialog be shown */
	INT				DialogRequestCount;
	/** keeps track of the order in which calls were made requesting the progress dialog */
	TArray<UBOOL>	DialogRequestStack;
	WxDlgProgress*	DlgProgress;

	/**
	 * StatusMessageStackItem
	 */
	struct StatusMessageStackItem
	{
		/** Status message text */
		FString StatusText;

		/** Progress numerator */
		INT ProgressNumerator;

		/** Progress denominator */
		INT ProgressDenominator;

		/** Cached numerator so we can update less frequently */
		INT SavedNumerator;
		
		/** Cached denominator so we can update less frequently */
		INT SavedDenominator;

		/** The list time we updated the progress, so we can make sure to update at least once a second */
		DOUBLE LastUpdateTime;

		StatusMessageStackItem()
		: ProgressNumerator(0), ProgressDenominator(0), SavedNumerator(0), SavedDenominator(0), LastUpdateTime(0.0)
		{
		}
	};

	/** Current status message and progress */
	StatusMessageStackItem StatusMessage;

	/** Stack of status messages and progress values */
	TArray< StatusMessageStackItem > StatusMessageStack;

public:
	INT SlowTaskCount;

	FFeedbackContextEditor();

	void Serialize( const TCHAR* V, EName Event );

	void BeginSlowTask( const TCHAR* Task, UBOOL bShouldShowProgressDialog, UBOOL bShowCancelButton=FALSE );
	void EndSlowTask();
	void SetContext( FContextSupplier* InSupplier );

	/** Whether or not the user has canceled out of this dialog */
	UBOOL ReceivedUserCancel();

	VARARG_BODY( UBOOL, YesNof, const TCHAR*, VARARG_NONE )
	{
		TCHAR TempStr[4096];
		GET_VARARGS( TempStr, ARRAY_COUNT(TempStr), ARRAY_COUNT(TempStr)-1, Fmt, Fmt );
		return appMsgf( AMT_YesNo, TempStr );
	}

	VARARG_BODY( UBOOL VARARGS, StatusUpdatef, const TCHAR*, VARARG_EXTRA(INT Numerator) VARARG_EXTRA(INT Denominator) );

	/**
	 * Updates the progress amount without changing the status message text
	 *
	 * @param Numerator		New progress numerator
	 * @param Denominator	New progress denominator
	 */
	virtual void UpdateProgress( INT Numerator, INT Denominator );

	/** Pushes the current status message/progress onto the stack so it can be restored later */
	virtual void PushStatus();

	/** Restores the previously pushed status message/progress */
	virtual void PopStatus();


	/**
	 * @return	TRUE if a map check is currently active.
	 */
	virtual UBOOL MapCheck_IsActive() const;
	virtual void MapCheck_Show();

	/**
	 * Same as MapCheck_Show, except it won't display the map check dialog if there are no errors in it.
	 */
	virtual void MapCheck_ShowConditionally();

	/**
	 * Hides the map check dialog.
	 */
	virtual void MapCheck_Hide();

	/**
	 * Clears out all errors/warnings.
	 */
	virtual void MapCheck_Clear();

	/**
	 * Called around bulk MapCheck_Add calls.
	 */
	virtual void MapCheck_BeginBulkAdd();

	/**
	 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InActor					Actor associated with the message; can be NULL.
	 * @param	InMessage				The message to display.
	 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage.
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	virtual void MapCheck_Add( MapCheckType InType, UObject* InActor, const TCHAR* InMessage, const TCHAR* InUDNPage, MapCheckGroup InGroup=MCGROUP_DEFAULT );

	/**
	 * Called around bulk MapCheck_Add calls.
	 */
	virtual void MapCheck_EndBulkAdd();


	/**
	 * @return	TRUE if a lighting build is currently active.
	 */
	virtual UBOOL LightingBuild_IsActive() const;
	virtual void LightingBuild_Show();

	/**
	 * Shows the dialog only if there are warnings or errors to display.
	 */
	virtual void LightingBuild_ShowConditionally();

	/**
	 * Hides the LightingBuild dialog.
	 */
	virtual void LightingBuild_Hide();

	/**
	 * Clears out all errors/warnings.
	 */
	virtual void LightingBuild_Clear();

	/**
	 * Called around bulk LightingBuild_Add calls.
	 */
	virtual void LightingBuild_BeginBulkAdd();

	/**
	 * Adds a message to the map check dialog, to be displayed when the dialog is shown.
	 *
	 * @param	InType					The message type (error/warning/...).
	 * @param	InActor					Actor associated with the message; can be NULL.
	 * @param	InMessage				The message to display.
	 * @param	InUDNPage				UDN Page to visit if the user needs more info on the warning.  This will send the user to https://udn.epicgames.com/Three/MapErrors#InUDNPage.
	 * @param	InGroup					The message group (kismet/mobile/...).
	 */
	virtual void LightingBuild_Add( MapCheckType InType, UObject* InObject, const TCHAR* InMessage, const TCHAR* InUDNPage=TEXT(""), MapCheckGroup InGroup=MCGROUP_DEFAULT );

	/**
	 * Called around bulk MapCheck_Add calls.
	 */
	virtual void LightingBuild_EndBulkAdd();

	/**
	 * Refresh the lighting build list.
	 */
	virtual void LightingBuild_Refresh();

	/**
	 *	Lighting Build Info handlers
	 */
	/**
	 * @return	TRUE if the LightingBuildInfo is currently active
	 */
	virtual UBOOL LightingBuildInfo_IsActive() const;
	virtual void LightingBuildInfo_Show();

	/** Hides the LightingBuildInfo dialog. */
	virtual void LightingBuildInfo_Hide();
	/** Clears out all errors/warnings.*/
	virtual void LightingBuildInfo_Clear();
	/** Called around bulk *_Add calls.*/
	virtual void LightingBuildInfo_BeginBulkAdd();

	/**
	 * Adds a message to the LightingBuildInfo dialog, to be displayed when the dialog is shown.
	 *
	 *	@param	InObject				Actor associated with the message; can be NULL.
	 *	@param	InTime					The time taken to light this object.
	 *	@param	InUnmappedPercentage	The unmapped texel percentage for this object, -1.f for 
	 *	@param	InUnmappedMemory		The memory taken up by unmapped texels for this object
	 *	@param	InTotalTexelMemory				The memory consumed by all texels for this object.
	 */
	virtual void LightingBuildInfo_Add(UObject* InObject, DOUBLE InTime, FLOAT InUnmappedPercentage, INT InUnmappedMemory, INT InTotalTexelMemory);

	/** Called around bulk *_Add calls. */
	virtual void LightingBuildInfo_EndBulkAdd();

	/** Refresh the lighting times list. */
	virtual void LightingBuildInfo_Refresh();

	/** Clears out all static mesh lighting info.*/
	virtual void LightingBuildInfoList_Clear();
};

#endif // __FFEEDBACKCONTEXTEDITOR_H__
