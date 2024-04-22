/**********************************************************************

Filename    :   GFxInteraction.uc
Content     :   GFx Interaction class, its instance is maintained
                in GFxGameViewportClient

Copyright   :   (c) 2006-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :   Since 'ucc' will prefix all class names with 'U'
                there is not conflict with GFx file / class naming.

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING 
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/


class GFxInteraction extends Interaction
    inherits(FCallbackEventDevice)
	native;

/** If true, then we ignore normal input, and only use touch input, as if it was a touch device */
var bool bFakeMobileTouches;

/** Set focus movie and input capture mode */
native function GFxMoviePlayer GetFocusMovie(int ControllerId);

native function NotifyGameSessionEnded();

native function NotifyPlayerAdded(int PlayerIndex, LocalPlayer AddedPlayer);

native function NotifyPlayerRemoved(int PlayerIndex, LocalPlayer RemovedPlayer);

native function NotifySplitscreenLayoutChanged();

native function CloseAllMoviePlayers();

cpptext
{
#if WITH_GFx
	/** Initializes this interaction, allocates the GFxEngine */
    virtual void Init();

    virtual void BeginDestroy();
    virtual UBOOL IsReadyForFinishDestroy();
    virtual void FinishDestroy();

	/** Set the Engine's viewport to the viewport specified */
	virtual void SetRenderViewport(class FViewport* InViewport);

	virtual UBOOL InputKey(INT ControllerId,FName Key,EInputEvent Event,FLOAT AmountDepressed,UBOOL bGamepad);
	virtual UBOOL InputAxis(INT ControllerId,FName Key,FLOAT Delta,FLOAT DeltaTime, UBOOL bGamepad);
	virtual UBOOL InputChar(INT ControllerId,TCHAR Character);
	virtual UBOOL InputTouch(INT ControllerId, UINT Handle, ETouchType Type, FVector2D TouchLocation, DOUBLE DeviceTimestamp, UINT TouchpadIndex);

	/**
	 * Called once a frame to update the interaction's state.
	 * @param	DeltaTime - The time since the last frame.
	 */
	virtual void Tick(FLOAT DeltaTime);

	/**	FExec interface */
	virtual UBOOL Exec(const TCHAR* Cmd, FOutputDevice& Ar);

    /* === FCallbackEventDevice interface === */
    /**
     * Called when the viewport has been resized.
     */
    virtual void Send( ECallbackEventType InType, class FViewport* InViewport, UINT InMessage);
    virtual void Send(ECallbackEventType InType, DWORD InFlag);

	/** Statistics Gathering */
	void CaptureRenderFrameStats();

#endif // WITH_GFx
}


defaultproperties
{

}
