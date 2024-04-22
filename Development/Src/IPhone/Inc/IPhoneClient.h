/*=============================================================================
	IPhoneClient.h: Unreal IPhone platform interface
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#ifndef __PS3CLIENT_H__
#define __PS3CLIENT_H__

// Forward declarations
struct FIPhoneViewport;

//
//	UIPhoneClient - IPhone-specific client code.
//

class UIPhoneClient : public UClient
{
	DECLARE_CLASS_INTRINSIC(UIPhoneClient, UClient, CLASS_Transient|CLASS_Config, IPhoneDrv)

	/** IPhone will only have one viewport, even in split screen */
	TArray<FIPhoneViewport*> Viewports;

	// Variables.
	UEngine*		Engine;

	// Audio device.
	UAudioDevice*	AudioDevice;

	// Constructors.
	UIPhoneClient();
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

#endif
