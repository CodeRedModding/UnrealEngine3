/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __CURVEEDPRESETDLG_H__
#define __CURVEEDPRESETDLG_H__

// Forward declarations.
class WxCurveEditor;

class WxCurveEdPresetDlg : public wxDialog, public FSerializableObject
{
public:
	enum ValidCurveList
	{
		VCL_XMax				= 0x00000001,
		VCL_YMax				= 0x00000002,
		VCL_ZMax				= 0x00000004,
		VCL_XMin				= 0x00000008,
		VCL_YMin				= 0x00000010,
		VCL_ZMin				= 0x00000020,
		VCL_FloatConstantCurve	= VCL_XMax,
		VCL_FloatUniformCurve	= VCL_XMax | VCL_XMin,
		VCL_VectorConstantCurve	= VCL_XMax | VCL_YMax | VCL_ZMax,
		VCL_VectorUniformCurve	= VCL_XMax | VCL_YMax | VCL_ZMax | VCL_XMin | VCL_YMin | VCL_ZMin
	};

	/** Empty default constructor to satisfy wx class registration. */
	WxCurveEdPresetDlg()	{}

	/**
	 * @param	parent			The parent window.
	 * @param	InNotifyHook	An optional callback that receives property PreChange and PostChange notifies.
	 */
	WxCurveEdPresetDlg(WxCurveEditor* parent);

	virtual ~WxCurveEdPresetDlg();

	void Serialize(FArchive& Ar);

	void CreateControls();

	bool ShowDialog(UBOOL bShow, const TCHAR* CurveName, INT ValidCurves, TArray<UClass*>& CurveEdPresets, UBOOL bInIsSaveDlg = FALSE);

	void OnDestroy(wxWindowDestroyEvent& In);
	void OnOK(wxCommandEvent& In);
	void OnPresetCombo(wxCommandEvent& In);

	DECLARE_EVENT_TABLE()

protected:
	void FillPresetCombos(TArray<UClass*>& CurveEdPresets);
	void SetDefaultValues();

public:
	UBOOL				IsCurveValid(INT CurveIndex);
	UCurveEdPresetBase*	GetPreset(INT CurveIndex);
	UBOOL				GetIsSaveDlg() const
	{
		return bIsSaveDlg;
	}

protected:
	INT					ValidityFlags;
	UBOOL				bIsSaveDlg;

	wxTextCtrl*			CurveNameCtrl;

public:

	enum
	{
		NUMDATACONTROLS = 7
	};

	struct PresetCurveControls
	{
		UCurveEdPresetBase*		CurrentPreset;
		wxCheckBox*				EnabledCheckBox;
		wxComboBox*				PresetCombo;
		wxPanel*				PropertyPanel;
		WxPropertyWindowHost*	PropertyWindow;

		~PresetCurveControls();
	};

	enum CurveBlocks
	{
		CEP_CURVE_XMAX	= 0,
		CEP_CURVE_YMAX,
		CEP_CURVE_ZMAX,
		CEP_CURVE_XMIN,
		CEP_CURVE_YMIN,
		CEP_CURVE_ZMIN,
		CEP_CURVE_MAX
	};
	
protected:
	WxCurveEditor*		CurveEditor;

	wxTextCtrl*			NumSamples;
	wxTextCtrl*			StartTimeValue;
	wxTextCtrl*			EndTimeValue;

	TArray<UClass*>*			LocalCurveEdPresets;
	PresetCurveControls			CurveControls[CEP_CURVE_MAX];
};

#endif // __CURVEEDPRESETDLG_H__
