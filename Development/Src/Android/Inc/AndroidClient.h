/*=============================================================================
	AndroidClient.h: Unreal Android platform interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __ANDROIDCLIENT_H__
#define __ANDROIDCLIENT_H__

#include "AndroidViewport.h"

//
//	UAndroidClient - Android-specific client code.
//

// Global for the holding the Engine's MaxDeltaTime when it's switched during benchmark mode
extern FLOAT		GSavedMaxDeltaTime;

class UKdClient : public UClient
{
	DECLARE_CLASS_INTRINSIC(UKdClient, UClient, CLASS_Transient|CLASS_Config, KdDrv)

	/** Android will only have one viewport, even in split screen */
	FKdViewport*	Viewport;

	// Variables.
	UEngine*		Engine;

	// Audio device.
	UAudioDevice*	AudioDevice;

	// Constructors.
	UKdClient();
	void StaticConstructor();

	// UObject interface.
	virtual void Serialize(FArchive& Ar);
	virtual void FinishDestroy();

	// UClient interface.
	virtual void Init(UEngine* InEngine);
	virtual void Tick( FLOAT DeltaTime );
	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	virtual void AllowMessageProcessing( UBOOL InValue ) { }

	virtual FViewportFrame* CreateViewportFrame(FViewportClient* ViewportClient,const TCHAR* InName,UINT SizeX,UINT SizeY,UBOOL Fullscreen = 0);
	virtual FViewport* CreateWindowChildViewport(FViewportClient* ViewportClient,void* ParentWindow,UINT SizeX=0,UINT SizeY=0,INT InPosX = -1, INT InPosY = -1);
	virtual void CloseViewport(FViewport* Viewport);

	virtual class UAudioDevice* GetAudioDevice() { return AudioDevice; }

	virtual void ForceClearForceFeedback() {}

	/**
	* Retrieves the name of the key mapped to the specified character code.
	*
	* @param	KeyCode	the key code to get the name for; should be the key's ANSI value
	*/
	virtual FName GetVirtualKeyName( INT KeyCode ) const;
};

/**
 * Plays an mp3 in hardware
 *
 * @param SongName Name of the mp3 to play, WITHOUT path or extension info
 */
extern void AndroidPlaySong(const char* SongName);

/**
 * Stops any current mp3 playing
 */
extern void AndroidStopSong();

#endif
