/*=============================================================================
	ReferencedAssetsToolbar.cpp: Implementation of the toolbar that is used by the ReferencedAsset browser.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "ReferencedAssetsToolbar.h"

WxRABrowserToolBar::WxRABrowserToolBar( wxWindow* InParent, wxWindowID InID )
: WxToolBar( InParent, InID, wxDefaultPosition, wxDefaultSize, wxTB_HORIZONTAL | wxTB_FLAT | wxTB_3DBUTTONS )
{
	// bitmaps
	DirectReferenceB.Load(TEXT("RAB_DirectRefsOnly"));
	AllReferenceB.Load(TEXT("RAB_ShowAllRefs"));
	ReferenceDepthB.Load(TEXT("RAB_DepthRefs"));

	ShowDefaultRefsB.Load(TEXT("RAB_ShowDefaultRefs"));
	ShowScriptRefsB.Load(TEXT("RAB_ShowScriptRefs"));

//	SearchB.Load( TEXT("Search") );
	GroupByClassB.Load( TEXT("GroupByClass") );

	lbl_ReferenceDepth = new wxStaticText( this, -1, *LocalizeUnrealEd("ReferenceDepth") );
	txt_ReferenceDepth = new wxTextCtrl( this, ID_CUSTOM_REFERENCE_DEPTH, TEXT(""), wxDefaultPosition, wxSize(100,-1) );

	// Set up the ToolBar
	AddRadioTool( ID_REFERENCEDEPTH_DIRECT,	TEXT(""), DirectReferenceB,	DirectReferenceB,	*LocalizeUnrealEd("DirectRefsTooltip") );
	AddRadioTool( ID_REFERENCEDEPTH_ALL,	TEXT(""), AllReferenceB,	AllReferenceB,		*LocalizeUnrealEd("AllRefsTooltip") );
	AddRadioTool( ID_REFERENCEDEPTH_CUSTOM,	TEXT(""), ReferenceDepthB,	ReferenceDepthB,	*LocalizeUnrealEd("CustomRefsTooltip") );
	AddSeparator();

	AddControl(lbl_ReferenceDepth);
	AddControl(txt_ReferenceDepth);
	AddSeparator();

	AddCheckTool( IDM_SHOWDEFAULTREFS, TEXT(""), ShowDefaultRefsB, ShowDefaultRefsB, *LocalizeUnrealEd(TEXT("ShowDefaultReferences")) );
	AddCheckTool( IDM_SHOWSCRIPTREFS, TEXT(""), ShowScriptRefsB, ShowScriptRefsB, *LocalizeUnrealEd(TEXT("ShowScriptReferences")) );
	AddSeparator();

//	AddTool( IDM_SEARCH, TEXT(""), SearchB, *LocalizeUnrealEd("ToolTip_66") );
	AddCheckTool( IDM_GROUPBYCLASS, TEXT(""), GroupByClassB, GroupByClassB, *LocalizeUnrealEd("ToolTip_61") );

	Realize();
}


/**
 * Changes the custom reference depth to the specified value.
 *
 * @param	NewDepth	the new value to assign as the custom reference depth; value of 0 indicates infinite
 */
void WxRABrowserToolBar::SetCustomDepth( INT NewDepth )
{
	FString DepthString = appItoa( Max(0, NewDepth) );

	// this will trigger an EVT_TEXT event
	txt_ReferenceDepth->SetValue( *DepthString );
}

/**
 * Retrieves the value of the custom reference depth textbox
 */
INT WxRABrowserToolBar::GetCustomDepth()
{
	FString DepthString = txt_ReferenceDepth->GetValue().c_str();
	return appAtoi(*DepthString);
}

/**
 * Toggles the enabled status of the custom depth text control
 */
void WxRABrowserToolBar::EnableCustomDepth( UBOOL bEnabled )
{
	lbl_ReferenceDepth->Enable(bEnabled == TRUE);
	txt_ReferenceDepth->Enable(bEnabled == TRUE);
}



