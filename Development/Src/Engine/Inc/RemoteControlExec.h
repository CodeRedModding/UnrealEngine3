/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __REMOTECONTROLEXEC_H__
#define __REMOTECONTROLEXEC_H__

// Forward declarations.
class WxRemoteControlFrame;
class FRemoteControlPageFactory;
class FRemoteControlGamePC;

/**
 * FExec handler for the remote control system.
 */
class FRemoteControlExec : public FExec
{
public:
	FRemoteControlExec();
	virtual ~FRemoteControlExec();

	/**
	 * Exec override for handling the RemoteControl toggle command.
	 */
	virtual UBOOL Exec(const TCHAR *Cmd, FOutputDevice &Ar);

	/**
	 * Called when PIE begins.
	 */
	void OnPlayInEditor(UWorld *PlayWorld);

	/**
	 * Called when PIE ends.
	 */
	void OnEndPlayMap();

	/**
	 * Called by the game engine, entry point for RemoteControl's game viewport rendering.
	 */
	void RenderInGame();

	/**
	 * Clean-up function for cleaning up after a PIE session.
	 */
	virtual void CleanUpAfterPIE();

	/**
	 * @return	TRUE	If RemoteControl is shown.
	 */
	UBOOL IsShown() const;

	/**
	 * @return	TRUE	If RemoteControl has been shown at least once since app startup.
	 */
	UBOOL HasEverBeenShown() const;

	/**
	 * Show or hide RemoteControl.
	 */
	void Show(UBOOL bShow);

	/**
	 *  Set window focus to the game.
	 */
	void SetFocusToGame();

private:
	/**
	 * Create RemoteControl if not already created.
	 */
	void CreateRemoteControl();

	WxRemoteControlFrame	*Frame;
	FRemoteControlGamePC	*GamePC;
};

#endif // __REMOTECONTROLEXEC_H__
