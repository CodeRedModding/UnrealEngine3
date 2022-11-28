
#include "UnrealEd.h"

extern class WxUnrealEdApp* GApp;

FCallbackDeviceEditor::FCallbackDeviceEditor()
{
}

FCallbackDeviceEditor::~FCallbackDeviceEditor()
{
}

void FCallbackDeviceEditor::Send( ECallbackType InType )
{
	switch( InType )
	{
		case CALLBACK_Browse:						GApp->CB_Browse();						break;
		case CALLBACK_UseCurrent:					GApp->CB_UseCurrent();					break;
		case CALLBACK_SelChange:					GApp->CB_SelectionChanged();			break;
		case CALLBACK_ViewportUpdateWindowFrame:	GApp->CB_ViewportUpdateWindowFrame();	break;
		case CALLBACK_SurfProps:					GApp->CB_SurfProps();					break;
		case CALLBACK_ActorProps:					GApp->CB_ActorProps();					break;
		case CALLBACK_SaveMap:						GApp->CB_SaveMap();						break;
		case CALLBACK_LoadMap:						GApp->CB_LoadMap();						break;
		case CALLBACK_PlayMap:						GApp->CB_PlayMap();						break;
		case CALLBACK_ActorPropertiesChange:		GApp->CB_ActorPropertiesChanged();		break;
		case CALLBACK_SaveMapAs:					GApp->CB_SaveMapAs();					break;
		case CALLBACK_DisplayLoadErrors:			GApp->CB_DisplayLoadErrors();			break;
		case CALLBACK_RtClickAnimSeq:				GApp->CB_AnimationSeqRightClicked();	break;
		case CALLBACK_RedrawAllViewports:			GApp->CB_RedrawAllViewports();			break;
		case CALLBACK_UpdateUI:						GApp->EditorFrame->UpdateUI();			break;
		case CALLBACK_Undo:							GApp->CB_Undo();						break;
		default:
			check(0);	// Unknown callback
			break;
	}

}

void FCallbackDeviceEditor::Send( ECallbackType InType, INT InFlag )
{
	switch( InType )
	{
		case CALLBACK_MapChange:			GApp->CB_MapChange( InFlag );					break;
		case CALLBACK_RefreshEditor:		GApp->CB_RefreshEditor( InFlag );				break;
		case CALLBACK_ChangeEditorMode:		GApp->SetCurrentMode( (EEditorMode)InFlag );	break;
		case CALLBACK_ShowDockableWindow:
			GApp->EditorFrame->DockingRoot->ShowDockableWindow( (EDockableWindowType)InFlag );
			break;

		default:
			check(0);	// Unknown callback
			break;
	}

}

void FCallbackDeviceEditor::Send( ECallbackType InType, FVector InVector )
{
    check ( 0 );
	//switch( InType )
	//{

	//	default:
	//		check(0);	// Unknown callback
	//		break;
	//}

}

void FCallbackDeviceEditor::Send( ECallbackType InType, FEdMode* InMode )
{
	if( !InMode->ModeBar )
		return;

	switch( InType )
	{
		case CALLBACK_EditorModeEnter:		InMode->ModeBar->Refresh();			break;
		case CALLBACK_EditorModeExit:		InMode->ModeBar->SaveData();		break;

		default:
			check(0);	// Unknown callback
			break;
	}

}

void FCallbackDeviceEditor::Send( ECallbackType InType, UObject* InObject )
{
	switch( InType )
	{
		case CALLBACK_SelectObject:
		{
			// If selecting an actor, move the pivot location.

			AActor* actor = Cast<AActor>(InObject);
			if( actor )
			{
				GEditorModeTools.PivotLocation = GEditorModeTools.SnappedLocation = actor->Location;
			}
		}
		break;

		default:
			check(0);	// Unknown callback
			break;
	}

}

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/
