/*=============================================================================
	Utils.cpp: Classes not derived from windows
	Copyright 1997-2001 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#include "Engine.h"
#include "Window.h"

/*-----------------------------------------------------------------------------
	FContainer.
-----------------------------------------------------------------------------*/

// Sets the set of anchors that this container will use.
void FContainer::SetAnchors( TMap<DWORD,FWindowAnchor>* InAnchors )
{
	check(InAnchors);
	Anchors = InAnchors;
	RefreshControls();
}

// Sizes/Positions all controls relative to their reference windows.
// (it is assumed that every window is a child of it's reference window)
void FContainer::RefreshControls()
{
	if( !Anchors ) return;

	FDeferWindowPos* dwp = new FDeferWindowPos;

	HWND PrevRefWindow = NULL;

	for( TMap<DWORD,FWindowAnchor>::TIterator It(*Anchors); It; ++It )
	{
		FWindowAnchor* WA = &It.Value();

		// Some windows depend on previous windows to be resized before them, so
		// if the reference window changes we need to commit the current deferral
		// and create a new one.
		if( PrevRefWindow != WA->RefWindow )
		{
			if( PrevRefWindow )
			{
				delete dwp;
				dwp = new FDeferWindowPos;
			}
			PrevRefWindow = WA->RefWindow;
		}

		RECT rcC;
		::GetClientRect( WA->RefWindow, &rcC );

		RECT rc;
		::GetWindowRect( WA->Window, &rc );
		::ScreenToClient( WA->RefWindow, (POINT*)&rc.left );
		::ScreenToClient( WA->RefWindow, (POINT*)&rc.right );

		if( WA->PosFlags&ANCHOR_LEFT )			rc.left = rcC.left + WA->XPos;
		else if( WA->PosFlags&ANCHOR_RIGHT )	rc.left = rcC.right + WA->XPos;

		if( WA->PosFlags&ANCHOR_TOP )			rc.top = rcC.top + WA->YPos;
		else if( WA->PosFlags&ANCHOR_BOTTOM )	rc.top = rcC.bottom + WA->YPos;

		if( WA->SzFlags&ANCHOR_WIDTH )			rc.right = rc.left + WA->XSz;
		else if( WA->SzFlags&ANCHOR_LEFT )		rc.right = rcC.left + WA->XSz;
		else if( WA->SzFlags&ANCHOR_RIGHT )		rc.right = rcC.right + WA->XSz;

		if( WA->SzFlags&ANCHOR_HEIGHT )			rc.bottom = rc.top + WA->YSz;
		else if( WA->SzFlags&ANCHOR_TOP )		rc.bottom = rcC.top + WA->YSz;
		else if( WA->SzFlags&ANCHOR_BOTTOM )	rc.bottom = rcC.bottom + WA->YSz;

		::MoveWindow( WA->Window, rc.left, rc.top, rc.right-rc.left, rc.bottom-rc.top, 1 );
	}

	delete dwp;

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
