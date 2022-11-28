/*=============================================================================
	UnPlayer.h: Unreal player definition.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

//
//	UPlayer
//

class UPlayer : public UObject, public FOutputDevice, public FExec
{
	DECLARE_ABSTRACT_CLASS(UPlayer,UObject,CLASS_Transient|CLASS_Config,Engine)

	APlayerController*	Actor;

	INT	CurrentNetSpeed,
		ConfiguredInternetSpeed,
		ConfiguredLanSpeed;

	// FExec interface.

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar) { return 0; }
};

//
//	ULocalPlayer
//

class ULocalPlayer : public UPlayer, public FViewportClient
{
	DECLARE_CLASS(ULocalPlayer,UPlayer,CLASS_Transient|CLASS_Config,Engine)
	DECLARE_WITHIN(UEngine)

	FChildViewport*		Viewport;
	FViewport*			MasterViewport;

	TArray<FString>		EnabledStats;
	ESceneViewMode		ViewMode;
	DWORD				ShowFlags;

	UBOOL				UseAutomaticBrightness;

	FSceneViewState*	ViewState;

	// Constructor.

	ULocalPlayer();

	// UObject interface.

	virtual void Destroy();

	// FViewportClient interface.

	virtual void RedrawRequested(FChildViewport* Viewport) {}

	virtual void InputKey(FChildViewport* Viewport,FName Key,EInputEvent Event,FLOAT AmountDepressed = 1.f);
	virtual void InputAxis(FChildViewport* Viewport,FName Key,FLOAT Delta,FLOAT DeltaTime);
	virtual void InputChar(FChildViewport* Viewport,TCHAR Character);
	/** Returns the platform specific forcefeedback manager associated with this viewport */
	virtual class UForceFeedbackManager* GetForceFeedbackManager(void);
	virtual void Precache(FChildViewport* Viewport,FPrecacheInterface* P);
	virtual void Draw(FChildViewport* Viewport,FRenderInterface* RI);

	virtual void LostFocus(FChildViewport* Viewport);
	virtual void ReceivedFocus(FChildViewport* Viewport);

	virtual void CloseRequested(FViewport* Viewport);

	virtual UBOOL RequiresHitProxyStorage() { return 0; }

	// FOutputDevice interface.

	virtual void Serialize(const TCHAR* V,EName Event);

	// FExec interface.

	virtual UBOOL Exec(const TCHAR* Cmd,FOutputDevice& Ar);

	void ExecMacro( const TCHAR* Filename, FOutputDevice& Ar );
};
