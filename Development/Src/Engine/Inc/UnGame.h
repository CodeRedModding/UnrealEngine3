/*=============================================================================
	UnGame.h: Unreal game class.
	Copyright 1997-2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Tim Sweeney
=============================================================================*/

//
//	UGameEngine - The Unreal game engine.
//

class UGameEngine : public UEngine
{
	DECLARE_CLASS(UGameEngine,UEngine,CLASS_Config|CLASS_Transient,Engine)

	// Variables.
	ULevel*			GLevel;
	ULevel*			GEntry;
	UPendingLevel*	GPendingLevel;
	FURL			LastURL;
	TArrayNoInit<FString> ServerActors;
	TArrayNoInit<FString> ServerPackages;

	FString		TravelURL;
	ETravelType	TravelType;
	UBOOL		bTravelItems;

	// Constructors.
	UGameEngine();

	// UObject interface.
	void Destroy();

	// UEngine interface.
	void Init();
	void Tick( FLOAT DeltaSeconds );
	UBOOL Exec( const TCHAR* Cmd, FOutputDevice& Ar=*GLog );
	void SetClientTravel( const TCHAR* NextURL, UBOOL bItems, ETravelType TravelType );
	FLOAT GetMaxTickRate();
	INT ChallengeResponse( INT Challenge );
	void SetProgress( const TCHAR* Str1, const TCHAR* Str2, FLOAT Seconds );


	// UGameEngine interface.
	virtual UBOOL Browse( FURL URL, const TMap<FString,FString>* TravelInfo, FString& Error );
	virtual ULevel* LoadMap( const FURL& URL, UPendingLevel* Pending, const TMap<FString,FString>* TravelInfo, FString& Error );
	virtual void SaveGame( INT Position );
	virtual void CancelPending();
	virtual void PaintProgress();
	virtual void UpdateConnectingMessage();
	virtual void BuildServerMasterMap( UNetDriver* NetDriver, ULevel* InLevel );
	virtual void NotifyLevelChange();
	virtual ULevel* GetLevel()
	{
		return GLevel;
	}
};

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

