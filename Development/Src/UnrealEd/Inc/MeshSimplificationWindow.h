/*=============================================================================
	MeshSimplificationWindow.h: Static mesh editor's Mesh Simplification tool
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/


#ifndef __MeshSimplificationWindow_h__
#define __MeshSimplificationWindow_h__

#if WITH_SIMPLYGON

class WxStaticMeshEditor;


/**
 * WxMeshSimplificationWindow
 *
 * The Mesh Simplification tool allows users to create and maintain reduced-polycount versions of mesh
 * assets on a per-level basis.
 *
 * If you change functionality here, be sure to update the UDN help page:
 *      https://udn.epicgames.com/Three/MeshSimplificationTool
 */
class WxMeshSimplificationWindow : public wxScrolledWindow
{
public:

	/** Constructor that takes a pointer to the owner object */
	WxMeshSimplificationWindow( WxStaticMeshEditor* InStaticMeshEditor );

	/** Updates the controls in this panel. */
	void UpdateControls();

protected:

	/** Called when 'Simplify' button is pressed in the quality group. */
	void OnSimplifyButton( wxCommandEvent& In );

	/** Called when the 'Recalculate Normals' checkbox state changes */
	void OnRecalcNormalsCheckBox( wxCommandEvent& In );

protected:

	/** Editor that owns us */
	WxStaticMeshEditor* StaticMeshEditor;

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

	/** Normal settings. */
	wxCheckBox* RecalcNormalsTickBox;
	wxStaticText* NormalThresholdLabel;
	WxSpinCtrlReal* NormalThresholdSpinner;

	/** Simplify button. */
	wxButton* SimplifyButton;

	DECLARE_EVENT_TABLE()
};

#endif // #if WITH_SIMPLYGON

#endif // __MeshSimplificationWindow_h__
