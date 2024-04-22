/*=============================================================================
	SkeletalMeshSimplificationWindow.h: Skeletal mesh simplification UI.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __SkeletalMeshSimplificationWindow_h__
#define __SkeletalMeshSimplificationWindow_h__

#if WITH_SIMPLYGON

class WxAnimSetViewer;

/**
 * WxSkeletalMeshSimplificationWindow
 *
 * Subwindow of the AnimSetViewer allowing users to simplify skeletal meshes from within the Editor.
 */
class WxSkeletalMeshSimplificationWindow : public wxScrolledWindow
{
public:

	/** Constructor that takes a pointer to the owner object */
	WxSkeletalMeshSimplificationWindow( WxAnimSetViewer* InAnimSetViewer, wxWindow* Parent );

	/** Updates the controls in this panel. */
	void UpdateControls();

protected:

	/** Called when 'Simplify' button is pressed in the quality group. */
	void OnSimplifyButton( wxCommandEvent& In );

	/** Called when the 'Recalculate Normals' checkbox state changes */
	void OnRecalcNormalsCheckBox( wxCommandEvent& In );

protected:

	/** Editor that owns us */
	WxAnimSetViewer* AnimSetViewer;

	/** Label displaying the original triangle count of the mesh. */
	wxStaticText* OriginalTriCountLabel;

	/** Label displaying the original vertex count of the mesh. */
	wxStaticText* OriginalVertCountLabel;

	/** The type of mesh reduction to use: % tris or deviation */
	WxComboBox* QualityReductionTypeComboBox;

	/** Desired quality slider. */
	wxSlider* QualitySlider;

	/** Desired welding threshold value. */
	WxSpinCtrlReal* WeldingThresholdSpinner;

	/** Importance settings. */
	WxComboBox* SilhouetteComboBox;
	WxComboBox* TextureComboBox;
	WxComboBox* ShadingComboBox;
	WxComboBox* SkinningComboBox;

	/** Normal settings. */
	wxCheckBox* RecalcNormalsTickBox;
	wxStaticText* NormalThresholdLabel;
	WxSpinCtrlReal* NormalThresholdSpinner;

	/** Skeleton Simplficiation settings. */
	wxSpinCtrl* MaxBonesSpinner;
	wxSlider* BonePercentageSlider;

	/** Simplify button. */
	wxButton* SimplifyButton;

	DECLARE_EVENT_TABLE()
};

#endif // #if WITH_SIMPLYGON

#endif // #ifndef __SkeletalMeshSimplificationWindow_h__
