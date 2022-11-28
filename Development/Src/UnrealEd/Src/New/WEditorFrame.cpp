/*=============================================================================
	ButtonBar : Class for handling the left hand button bar

	Revision history:
		* Created by Warren Marshall

=============================================================================*/

#include "UnrealEd.h"

WEditorFrame::WEditorFrame()
	: WWindow()
{
}

void WEditorFrame::OnCreate()
{
	WWindow::OnCreate();
	SetText( *FString::Printf( *LocalizeGeneral(TEXT("FrameWindow"),TEXT("UnrealEd")), *LocalizeGeneral(TEXT("Product"),TEXT("Core"))) );

}

void WEditorFrame::OpenWindow()
{
	PerformCreateWindowEx
	(
		0,
		NULL,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN | WS_CLIPSIBLINGS,
		-100,
		-100,
		100,
		100,
		OwnerWindow ? OwnerWindow->hWnd : NULL,
		NULL,
		hInstance
	);
}

void WEditorFrame::OnTimer()
{
	if( GUnrealEd->AutoSave && !GIsSlowTask)
		GUnrealEd->Exec( TEXT("MAYBEAUTOSAVE") );
}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
