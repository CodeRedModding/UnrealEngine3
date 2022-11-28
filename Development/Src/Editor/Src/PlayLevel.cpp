/*=============================================================================
	PlayLevel.cpp: In-editor level playing.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

	Revision history:
		* Created by Andrew Scheidecker
=============================================================================*/

#include "EditorPrivate.h"

UBOOL UEditorPlayer::Exec(const TCHAR* Cmd,FOutputDevice& Ar)
{
	if(ParseCommand(&Cmd,TEXT("CloseEditorViewport")) ||
		ParseCommand(&Cmd,TEXT("Exit")) ||
		ParseCommand(&Cmd,TEXT("Quit")))
	{
		CloseRequested(MasterViewport);
		return 1;
	}
	if(Super::Exec(Cmd,Ar))
		return 1;

	return 0;
}
IMPLEMENT_CLASS(UEditorPlayer);

void UEditorEngine::EndPlayMap()
{
	check(PlayLevel);

	// Clear the play level package's dirty flag.
	UPackage*	PlayLevelPackage = CastChecked<UPackage>(PlayLevel->GetOuter());
	PlayLevelPackage->bDirty = 0;

	// Stop all audio and remove references to temp level.
	if( Client && Client->GetAudioDevice() )
	{
		Client->GetAudioDevice()->Flush();
	}

	// Clean up the temporary play level.
	PlayLevel->CleanupLevel();
	PlayLevel = NULL;
	CollectGarbage(RF_Native | RF_Standalone);

	// Make sure that the TempPlayLevel package was entirely garbage collected.
	UObject* TempPackage = UObject::StaticFindObject(UObject::StaticClass(),NULL,TEXT("TempPlayLevel"));
	if(TempPackage)
	{
		for(FObjectIterator It;It;++It)
		{
			if(It->IsIn(TempPackage))
			{
				class FMultiLineStringOutputDevice : public FString, public FOutputDevice
				{
				public:
					void Serialize( const TCHAR* Data, EName Event )
					{
						*this += (TCHAR*)Data;
						*this += TEXT("\n");
					}
				};
				FMultiLineStringOutputDevice RefsString;
				UObject::StaticExec(*FString::Printf(TEXT("obj refs class=%s package=%s name=%s"),It->GetClass()->GetName(),*It->GetOuter()->GetPathName(),It->GetName()),RefsString);
				appErrorf(TEXT("Object from temporary play level still referenced:\n%s"),*RefsString);
			}
		}
	}

	//@fixme - temporary fix for selection crash bug after using "play from here"
	// seems that some of the contents of GSelectionTools.SelectedObjects were
	// being deleted/GC'd
	GSelectionTools.RebuildSelectedList();
}

void UEditorEngine::PlayMap(FVector* StartLocation)
{
	DisableRealtimeViewports();

	// If there's level already being played, close it.

	if(PlayLevel)
	{
		for(INT PlayerIndex = 0;PlayerIndex < Players.Num();PlayerIndex++)
			Players(PlayerIndex)->CloseRequested(Players(PlayerIndex)->MasterViewport);
		EndPlayMap();
	}

	// Make a copy of the current level.

	PlayLevel = CastChecked<ULevel>( StaticDuplicateObject(Level->GetOuter(),Level,NULL,TEXT("TempPlayLevel"),~(RF_Standalone|RF_Transactional)) );

	// If a start location is specified, spawn a temporary Teleporter actor at the start location and use it as the portal.

	if(StartLocation)
	{
		ATeleporter* PlayerStart = Cast<ATeleporter>( PlayLevel->SpawnActor(ATeleporter::StaticClass(),NAME_None,*StartLocation) );
		if(!PlayerStart)
			return;

		PlayerStart->Tag = TEXT("TempEditorStart");
	}

	FURL	URL(NULL,TEXT("TempPlayLevel#TempEditorStart?quickstart=1"),TRAVEL_Absolute);
	PlayLevel->BeginPlay(URL,NULL);

	// Create the local player.

	UEditorPlayer*	LocalPlayer = new(this) UEditorPlayer;
	Players.AddItem(LocalPlayer);

	// Spawn the player.
	FString	Error = TEXT("");
	LocalPlayer->Actor = PlayLevel->SpawnPlayActor(LocalPlayer,ROLE_SimulatedProxy,URL,Error);
	if(!LocalPlayer->Actor)
	{
		appMsgf(0,TEXT("Couldn't spawn player: %s"),*Error);
		return;
	}

	// Open initial Viewport.
	if( Client )
	{
		// Create a viewport for the local player.

		LocalPlayer->Viewport = LocalPlayer->MasterViewport = Client->CreateViewport(
			LocalPlayer,
			*LocalizeGeneral("Product",GPackage),
			Client->StartupResolutionX,
			Client->StartupResolutionY,
			Client->EditorPreviewFullscreen
			);
		LocalPlayer->Viewport->CaptureMouseInput(1);
	}
}
