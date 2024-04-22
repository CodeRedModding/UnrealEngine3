/*=============================================================================
	DlgSoundNodeWaveOptions.h: Dialog for controlling various USoundNodeWave options.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGSOUNDNODEWAVEOPTIONS_H__
#define __DLGSOUNDNODEWAVEOPTIONS_H__

#include "TrackableWIndow.h"

// PCF Begin
/**
 * wxWidgets timer ID
 */
#define SOUNDNODEWAVE_TIMER_ID 9999

class FSoundPreviewThread;

// PCF End

/**
 * Dialog for controlling various USoundNodeWave options.
 */
class WxDlgSoundNodeWaveOptions : public WxTrackableDialog, public FSerializableObject
{
public:
	WxDlgSoundNodeWaveOptions( wxWindow* InParent, USoundNodeWave* Wave );
	virtual ~WxDlgSoundNodeWaveOptions( void );

	/**
	 * Used to serialize any UObjects contained that need to be to kept around.
	 *
	 * @param Ar The archive to serialize with
	 */
	void Serialize( FArchive& Ar );

	/** Static guard for previewer */
	static UBOOL bQualityPreviewerActive;

private:
	/** Compresses and decompresses sound data */
	void CreateCompressedWaves( void );

	/** Refreshes the options list. */
	void RefreshOptionsList( void );

	/** Starts playing the sound preview. */
	void PlayPreviewSound( void );

	/** Stops the sound preview if it is playing. */
	void StopPreviewSound( void );

	/** Frees all the allocated assets */
	void Cleanup( void );

	/** Callback for when the user clicks OK. */
	void OnOK( wxCommandEvent& In );

	/** Callback for when the user clicks Cancel. */
	void OnCancel( wxCommandEvent& In );

	/** Callback for when the user clicks the close button */
	void OnClose( wxCloseEvent& In );

	/** Callback for when the user single clicks an item on the list. */
	void OnListItemSingleClicked( wxListEvent& In );

	/** Callback for when the user double clicks an item on the list. */
	void OnListItemDoubleClicked( wxListEvent& In );

	/**
	 * Calculate column number from window local mouse position.
	 *
	 * @param Position		mouse position
	 * @return				column number
	 */
	INT CalculateColumnNumber( wxPoint Position );

	/**
	 * Called on every timer impulse, call RefreshOptionsList() when thread job finished.
	 */
	void OnTimerImpulse( wxTimerEvent &Event );

	/**
	 * Updates the options list. 
	 */
	void UpdateOptionsList( void );

	/** Node to perform all of our modifications on. */
	USoundNodeWave* SoundNode;

	/** Main List for this dialog */
	wxListCtrl* ListOptions;

	/** OK and Cancel Controls */
	wxButton* ButtonOK;
	wxButton* ButtonCancel;

	/** Selected item */
	INT ItemIndex;

	/** Original quality setting of SoundNodeWave */
	INT OriginalQuality;

	/** Set to TRUE when the all waves have finished converting */
	UBOOL bTaskEnded;

	/** Sound quality preview thread */
	FSoundPreviewThread* SoundPreviewThreadRunnable;
	FRunnableThread* SoundPreviewThread;
	
	/** wxWidgets timer to detect thread finished job */
	wxTimer		ThreadTimer;
	/** Keeps current compressed quality row */
	INT CurrentIndex;

	/** Cache the dialog title for easy updating with compression info */
	FString OriginalDialogTitle;

	static  FPreviewInfo PreviewInfo[];

	DECLARE_EVENT_TABLE()
};

#endif

