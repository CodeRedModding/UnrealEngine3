/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __MATERIALEDITORTOOLBAR_H__
#define __MATERIALEDITORTOOLBAR_H__


/**
 * The base toolbar for material editor and material instance editor.
 */
class WxMaterialEditorToolBarBase : public WxToolBar
{
public:
	WxMaterialEditorToolBarBase(wxWindow* InParent, wxWindowID InID);

	void SetShowGrid(UBOOL bValue);
	void SetShowBackground(UBOOL bValue);
	void SetRealtimeMaterialPreview(UBOOL bValue);

protected:
	WxMaskedBitmap	BackgroundB;
	WxMaskedBitmap	CylinderB;
	WxMaskedBitmap	CubeB;
	WxMaskedBitmap	SphereB;
	WxMaskedBitmap	PlaneB;
	WxMaskedBitmap	RealTimeB;
	WxMaskedBitmap	RealTimePreviewsB;
	WxMaskedBitmap	CleanB;
	WxMaskedBitmap	UseButtonB;
	WxMaskedBitmap	ToggleGridB;
	WxMaskedBitmap  ShowAllParametersB;
	WxMaskedBitmap  SyncGenericBrowserB;
};

/**
 * The toolbar appearing along the top of the material editor.
 */
class WxMaterialEditorToolBar : public WxMaterialEditorToolBarBase
{
public:
	WxMaterialEditorToolBar(wxWindow* InParent, wxWindowID InID);

	void SetHideConnectors(UBOOL bValue);
	void SetRealtimeExpressionPreview(UBOOL bValue);
	void SetAlwaysRefreshAllPreviews(UBOOL bValue);

	WxSearchControlNextPrev	SearchControl;
protected:
	WxMaskedBitmap	HideConnectorsB;
	WxMaskedBitmap	ShowConnectorsB;
	WxMaskedBitmap	HomeB;
	WxMaskedBitmap	ApplyB;
	WxMaskedBitmap	ApplyDisabledB;
	WxMaskedBitmap	ToggleMaterialStatsB;
	WxMaskedBitmap	ViewSourceB;
	WxMaskedBitmap	FlattenB;

	WxBitmapButton*	ApplyButton;
};

/**
 * The toolbar appearing along the top of the material instance editor.
 */
class WxMaterialInstanceConstantEditorToolBar : public WxMaterialEditorToolBarBase
{
public:
	WxMaterialInstanceConstantEditorToolBar(wxWindow* InParent, wxWindowID InID);
};

class WxMaterialInstanceTimeVaryingEditorToolBar : public WxMaterialEditorToolBarBase
{
public:
	WxMaterialInstanceTimeVaryingEditorToolBar(wxWindow* InParent, wxWindowID InID);
};


#endif // __MATERIALEDITORTOOLBAR_H__
