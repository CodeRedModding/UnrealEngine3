/*=============================================================================
	StatusBars : All the status bars the editor uses

	Revision history:
		* Created by Warren Marshall

    Work-in-progress todo's:

=============================================================================*/

enum EStatusBar
{
	SB_Standard,
	SB_Progress,
	SB_Max,
};

/*------------------------------------------------------------------------------
    Standard.  Displayed during normal editing.
------------------------------------------------------------------------------*/

class WxStatusBarStandard : public WxStatusBar
{
public:
	wxComboBox *ExecCombo;
	WxMaskedBitmap DragGridB, RotationGridB;
	wxCheckBox *DragGridCB, *RotationGridCB;
	wxStaticBitmap *DragGridSB, *RotationGridSB;
	wxStaticText *DragGridST, *RotationGridST;
	WxMenuButton *DragGridMB, *RotationGridMB;

	WxStatusBarStandard();
	~WxStatusBarStandard();

	virtual void SetUp();
	virtual void UpdateUI();
	void AddToExecCombo( FString InExec );

	void OnSize( wxSizeEvent& InEvent );
	void OnDragGridToggleClick( wxCommandEvent& InEvent );
	void OnRotationGridToggleClick( wxCommandEvent& InEvent );
	void OnExecComboEnter( wxCommandEvent& In );
	void OnExecComboSelChange( wxCommandEvent& In );

	INT DragTextWidth, RotationTextWidth;

	enum
	{
		FIELD_ExecCombo,
		FIELD_Status,
		FIELD_Drag_Grid,
		FIELD_Rotation_Grid,
	};

	DECLARE_EVENT_TABLE()
};

/*------------------------------------------------------------------------------
    Progress.  Displayed during long operations.
------------------------------------------------------------------------------*/

class WxStatusBarProgress : public WxStatusBar
{
public:
	WxStatusBarProgress();
	~WxStatusBarProgress();

	virtual void SetUp();
	virtual void UpdateUI();

	void OnSize( wxSizeEvent& InEvent );

	enum
	{
		FIELD_Progress,
		FIELD_Description,
	};

	wxGauge* Gauge;

	DECLARE_EVENT_TABLE()
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
