/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "RemoteControlExec.h"
#include "RemoteControlFrame.h"
#include "RemoteControlGamePC.h"

FRemoteControlExec::FRemoteControlExec()
	:	Frame(NULL)
{
	GamePC = new FRemoteControlGamePC;
}

FRemoteControlExec::~FRemoteControlExec()
{
	// wxWindows will clean up the frame; no need to delete.
	delete GamePC;
}

/**
 * Exec override for handling the RemoteControl toggle command.
 */
UBOOL FRemoteControlExec::Exec(const TCHAR *Cmd, FOutputDevice &Ar)
{
	UBOOL bHandled = FALSE;

	if( ParseCommand( &Cmd, TEXT("RC") ) || ParseCommand( &Cmd, TEXT("REMOTECONTROL") ) )
	{
		extern UBOOL GUsewxWindows;
		if( GUsewxWindows )
		{
			if ( IsShown() )
			{
				Show( FALSE );
			}
			else
			{
				// Allow RemoteControl to activate only when a game is running.
				if ( GIsGame )
				{
					Show( TRUE );
				}
			}
		}
		else
		{
			warnf(TEXT("You need to start the game with -WXWINDOWS on the command line for remote control support"));
		}
		bHandled = TRUE;
	}

	return bHandled;
}

/**
 * Called when PIE begins.
 */
void FRemoteControlExec::OnPlayInEditor(UWorld *PlayWorld)
{
	// Keep track of the play world.
	GamePC->SetPlayWorld( PlayWorld );
	GamePC->RepositionRemoteControl();
	SetFocusToGame();
}

/**
 * Called when PIE ends.
 */
void FRemoteControlExec::OnEndPlayMap()
{
	Show( FALSE );
	GamePC->DestroyPropertyWindows();
	GamePC->SetPlayWorld( NULL );
}

/**
 * Called by the game engine, entry point for RemoteControl's game viewport rendering.
 */
void FRemoteControlExec::RenderInGame()
{
	GamePC->RenderInGame();
}

/**
 * Clean-up function for cleaning up after a PIE session.
 */
void FRemoteControlExec::CleanUpAfterPIE()
{
	if ( GLogConsole )
	{
	    GLogConsole->Show( FALSE );
	}
}

/**
 * @return	TRUE	If RemoteControl is shown.
 */
UBOOL FRemoteControlExec::IsShown() const
{
	return Frame && Frame->IsShown();
}

/**
 * @return	TRUE	If RemoteControl has been shown at least once since app startup.
 */
UBOOL FRemoteControlExec::HasEverBeenShown() const
{
	return Frame != NULL;
}

/**
 * Show or hide RemoteControl.
 */
void FRemoteControlExec::Show(UBOOL bShow)
{
	if( bShow )
	{
		CreateRemoteControl();
		check( Frame );
		Frame->Show( TRUE );
		Frame->Raise();
		Frame->SetFocus();
	}
	else
	{
		if( Frame )
		{
			Frame->Show( FALSE );
		}
	}
}

/**
 *  Set window focus to the game.
 */
void FRemoteControlExec::SetFocusToGame()
{
	if ( IsShown() )
	{
		GamePC->SetFocusToGame();
	}
}

/**
 * Create RemoteControl if not already created.
 */
void FRemoteControlExec::CreateRemoteControl()
{
	if( Frame == NULL )
	{
		Frame = new WxRemoteControlFrame( GamePC, NULL, -1 );
		GamePC->SetFrame( Frame );
	}
}
