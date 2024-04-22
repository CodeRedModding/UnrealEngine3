/*=============================================================================
	ReferencedAssetsToolbar.h: Class declaration for the toolbar that is used by
								the ReferencedAsset browser.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REFERENCEDASSETSTOOLBAR_H__
#define __REFERENCEDASSETSTOOLBAR_H__

/**
 * The toolbar that sits in the Generic Browser.
 */
class WxRABrowserToolBar : public WxToolBar
{
public:

	WxRABrowserToolBar(class wxWindow* InParent, wxWindowID InID);

	WxMaskedBitmap DirectReferenceB, AllReferenceB, ReferenceDepthB;
	WxMaskedBitmap ShowDefaultRefsB, ShowScriptRefsB;

	WxMaskedBitmap /*SearchB, */GroupByClassB;
	
	/**
	 * Changes the custom reference depth to the specified value.
	 *
	 * @param	NewDepth	the new value to assign as the custom reference depth; value of 0 indicates infinite
	 */
	void SetCustomDepth( INT NewDepth );

	/**
	 * Retrieves the value of the custom reference depth textbox
	 */
	INT GetCustomDepth();

	/**
	 * Toggles the enabled status of the custom depth text control
	 */
	void EnableCustomDepth( UBOOL bEnabled );

private:
	wxStaticText*	lbl_ReferenceDepth;
	wxTextCtrl*		txt_ReferenceDepth;
};

#endif


