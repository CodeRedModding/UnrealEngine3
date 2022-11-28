/*=============================================================================
	FCallbackDevice.h:  Allows the engine to do callbacks into the editor

	Revision history:
		* Created by Warren Marshall
=============================================================================*/

#ifndef _FCALLBACKDEVICE_H_
#define _FCALLBACKDEVICE_H_

enum ECallbackType
{
	CALLBACK_None,
	CALLBACK_Browse,
	CALLBACK_UseCurrent,
	CALLBACK_SelChange,
	CALLBACK_MapChange,
	CALLBACK_ViewportUpdateWindowFrame,
	CALLBACK_SurfProps,
	CALLBACK_ActorProps,
	CALLBACK_SaveMap,
	CALLBACK_SaveMapAs,
	CALLBACK_LoadMap,
	CALLBACK_PlayMap,
	CALLBACK_ChangeEditorMode,
	CALLBACK_RedrawAllViewports,
	CALLBACK_ViewportsDisableRealtime,
	CALLBACK_ActorPropertiesChange,
	CALLBACK_RefreshEditor,
	CALLBACK_DisplayLoadErrors,
	CALLBACK_RtClickAnimSeq,
	CALLBACK_EditorModeEnter,
	CALLBACK_EditorModeExit,
	CALLBACK_UpdateUI,						// Sent when events happen that affect how the editors UI looks (mode changes, grid size changes, etc)
	CALLBACK_SelectObject,					// An object has been selected (generally an actor)
	CALLBACK_ShowDockableWindow,			// Brings a specific dockable window to the front
	CALLBACK_Undo,							// Sent after an undo/redo operation takes place
};

/*-----------------------------------------------------------------------------
	CallbackDevice.

	Base class for callback devices.
-----------------------------------------------------------------------------*/

class FCallbackDevice
{
public:
	FCallbackDevice()
	{
	}
	virtual ~FCallbackDevice()
	{
	}

	virtual void Send( ECallbackType InType )	{}
	virtual void Send( ECallbackType InType, UBOOL InFlag )	{}
	virtual void Send( ECallbackType InType, FVector InVector ) {}
	virtual void Send( ECallbackType InType, class FEdMode* InMode ) {}
	virtual void Send( ECallbackType InType, UObject* InObject ) {}
};

#endif

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

