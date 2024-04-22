/*=============================================================================
ViewportsContainer.h: Container to hold viewport splitters that handles splitter events.

Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __VIEWPORTSCONTAINER_H__
#define __VIEWPORTSCONTAINER_H__

/**
* Container for numerous viewports and their splitters.
*/
class WxViewportsContainer : public wxWindow
{
public:
	WxViewportsContainer( wxWindow* InParent, wxWindowID InId );

	/**
	* Connects splitter sash position changed events so we can move them together.
	*
	* @param InTopSplitter		Top wxSplitterWindow pointer.
	* @param InBottomSplitter	Bottom wxSplitterWindow pointer.
	*/
	void ConnectSplitterEvents( wxSplitterWindow* InTopSplitter, wxSplitterWindow* InBottomSplitter );

	/**
	* Disconnects previously connected splitter sash position events.
	*/
	void DisconnectSplitterEvents();

	/** 
	* Forces the top and bottom splitter positions to match by making the bottom sash position equal the top.
	*/
	void MatchSplitterPositions();

private:
	wxSplitterWindow* TopSplitter;
	wxSplitterWindow* BottomSplitter;

	/** 
	*  Used to link the top and bottom splitters together so they split together.
	*/
	void	OnTopSplitterSashPositionChanged( wxSplitterEvent &In );

	/** 
	*  Used to link the top and bottom splitters together so they split together.
	*/
	void	OnBottomSplitterSashPositionChanged( wxSplitterEvent &In );
};

#endif

