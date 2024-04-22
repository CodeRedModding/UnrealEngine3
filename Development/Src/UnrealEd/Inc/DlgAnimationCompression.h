/*=============================================================================
	DlgAnimationCompression.h: AnimSet Viewer's animation compression dialog.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __DLGANIMATIONCOMPRESSION_H__
#define __DLGANIMATIONCOMPRESSION_H__

// Forward declarations.
class FArchive;
class WxAnimSetViewer;
class WxPropertyWindow;
class UAnimationCompressionAlgorithm;

/**
 * AnimSet Viewer's animation compression dialog.
 */
class WxDlgAnimationCompression : public wxFrame
{
public:
	wxListBox*							AlgorithmList;
	WxPropertyWindowHost*				PropertyWindow;
	WxAnimSetViewer*					AnimSetViewer;
	wxButton*							ApplyToSetButton;
	wxButton*							ApplyToSequenceButton;
	UAnimationCompressionAlgorithm*		SelectedAlgorithm;

	WxDlgAnimationCompression(WxAnimSetViewer* InASV, wxWindowID InID);

private:
	DECLARE_EVENT_TABLE()

	/**
	 * Called when the window is closed.  Clears the AnimSet Viewer's reference to this dialog.
	 */
	void OnClose(wxCloseEvent& In);

	/** 
	 * Called when an animation compression algorithm is selected from the list.
	 * Sets the active algorithm and updates the property window with algorithm parameters.
	 */
	void OnClickAlgorithm(wxCommandEvent& In);

	/**
	 * Applies the active algorithm to the selected set.
	 */
	void OnApplyAlgorithmToSet(wxCommandEvent& In);

	/**
	 * Applies the active algorithm to the selected sequence.
	 */
	void OnApplyAlgorithmToSequence(wxCommandEvent& In);

	/**
	 * Populates the algorithm list with the set of instanced algorithms.
	 */
	void UpdateAlgorithmList();

	/**
	 * Sets the active algorithm and updates the property window with algorithm parameters.
	 */
	void SetSelectedAlgorithm(UAnimationCompressionAlgorithm* InAlgorithm);

	/**
	 * Populates the AnimationCompressionAlgorithms list with instances of each UAnimationCompresionAlgorithm-derived class.
	 */
	void InitAnimCompressionAlgorithmClasses();
};

#endif // __DLGANIMATIONCOMPRESSION_H__
