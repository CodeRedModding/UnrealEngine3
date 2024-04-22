/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "ViewportsContainer.h"

/*-----------------------------------------------------------------------------
	WxViewportsContainer
-----------------------------------------------------------------------------*/

WxViewportsContainer::WxViewportsContainer( wxWindow* InParent, wxWindowID InId )
: wxWindow( InParent, InId ),
  TopSplitter( NULL ),
  BottomSplitter( NULL )
{
	// Set a black background
	SetBackgroundColour( wxColour( 0, 0, 0 ) );
}

/**
* Connects splitter sash position changed events so we can move them together.
*
* @param InTopSplitter		Top wxSplitterWindow pointer.
* @param InBottomSplitter	Bottom wxSplitterWindow pointer.
*/
void WxViewportsContainer::ConnectSplitterEvents(wxSplitterWindow* InTopSplitter, wxSplitterWindow* InBottomSplitter )
{
	TopSplitter = InTopSplitter;
	BottomSplitter = InBottomSplitter;

	if( TopSplitter != NULL && BottomSplitter != NULL )
	{
		Connect( ID_SPLITTERWINDOW+1, wxEVT_COMMAND_SPLITTER_SASH_POS_CHANGED, wxSplitterEventHandler(WxViewportsContainer::OnTopSplitterSashPositionChanged));
		Connect( ID_SPLITTERWINDOW+2, wxEVT_COMMAND_SPLITTER_SASH_POS_CHANGED, wxSplitterEventHandler(WxViewportsContainer::OnBottomSplitterSashPositionChanged));
	}
	else
	{
		Disconnect( ID_SPLITTERWINDOW+1 );
		Disconnect( ID_SPLITTERWINDOW+2 );
	}
}

/**
* Disconnects previously connected splitter sash position events.
*/
void WxViewportsContainer::DisconnectSplitterEvents()
{
	TopSplitter = NULL;
	BottomSplitter = NULL;

	Disconnect( ID_SPLITTERWINDOW+1 );
	Disconnect( ID_SPLITTERWINDOW+2 );
}

/** 
* Forces the top and bottom splitter positions to match by making the bottom sash position equal the top.
*/
void WxViewportsContainer::MatchSplitterPositions()
{
	if( TopSplitter != NULL && BottomSplitter != NULL )
	{
		BottomSplitter->SetSashPosition( TopSplitter->GetSashPosition() );
	}
}

/** 
*  Used to link the top and bottom splitters together so they split together.
*/
void WxViewportsContainer::OnTopSplitterSashPositionChanged( wxSplitterEvent &In )
{
	if( GApp->EditorFrame->GetViewportsResizeTogether() )
	{
		INT SashPos = In.GetSashPosition();
		BottomSplitter->SetSashPosition( SashPos );
	}
}

/** 
*  Used to link the top and bottom splitters together so they split together.
*/
void WxViewportsContainer::OnBottomSplitterSashPositionChanged( wxSplitterEvent &In )
{
	if( GApp->EditorFrame->GetViewportsResizeTogether() )
	{
		INT SashPos = In.GetSashPosition();
		TopSplitter->SetSashPosition( SashPos );
	}
}
