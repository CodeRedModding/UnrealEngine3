/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnIpDrv.h"

#if WITH_UE3_NETWORKING

#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"

IMPLEMENT_CLASS(UOnlinePlaylistManager);

/**
 * Uses the configuration data to create the requested objects and then applies any
 * specific game settings changes to them
 */
void UOnlinePlaylistManager::FinalizePlaylistObjects(void)
{
	// Process the config entries creating and updating as specified
	for (INT PlaylistIndex = 0; PlaylistIndex < Playlists.Num(); PlaylistIndex++)
	{
		FPlaylist& Playlist = Playlists(PlaylistIndex);
		// Create each game setting object and update its data
		for (INT GameIndex = 0; GameIndex < Playlist.ConfiguredGames.Num(); GameIndex++)
		{
			FConfiguredGameSetting& ConfiguredGame = Playlist.ConfiguredGames(GameIndex);
			// If there is a valid class name specified try to load it and instance it
			if (ConfiguredGame.GameSettingsClassName.Len())
			{
				UClass* GameSettingsClass = LoadClass<UOnlineGameSettings>(NULL,
					*ConfiguredGame.GameSettingsClassName,
					NULL,
					LOAD_None,
					NULL);
				if (GameSettingsClass)
				{
					// Now create an instance with that class
					ConfiguredGame.GameSettings = ConstructObject<UOnlineGameSettings>(GameSettingsClass);
					if (ConfiguredGame.GameSettings)
					{
						// Update the game object with these settings, if not using the defaults
						if (ConfiguredGame.URL.Len())
						{
							ConfiguredGame.GameSettings->UpdateFromURL(ConfiguredGame.URL,NULL);
						}
					}
					else
					{
						debugf(NAME_DevOnline,
							TEXT("Failed to create class (%s) for playlist (%s)"),
							*ConfiguredGame.GameSettingsClassName,
							*Playlist.Name);
					}
				}
				else
				{
					debugf(NAME_DevOnline,
						TEXT("Failed to load class (%s) for playlist (%s)"),
						*ConfiguredGame.GameSettingsClassName,
						*Playlist.Name);
				}
			}
		}
	}
	if (DatastoresToRefresh.Num())
	{
		INT DatastoreIndex = INDEX_NONE;
		// Iterate through the registered set of datastores and refresh them
		for (TObjectIterator<UUIDataStore_GameResource> ObjIt; ObjIt; ++ObjIt)
		{
			DatastoresToRefresh.FindItem(ObjIt->Tag,DatastoreIndex);
			// Don't refresh it if it isn't in our list
			if (DatastoreIndex != INDEX_NONE)
			{
				(*ObjIt)->InitializeListElementProviders();
			}
		}
	}
	// Update our time stamp so we know when the data is stale
	LastPlaylistDownloadTime = appSeconds();
}

/** Uses the current loc setting and game ini name to build the download list */
void UOnlinePlaylistManager::DetermineFilesToDownload(void)
{
	PlaylistFileNames.Empty(4);
	// Build the game specific playlist ini
	PlaylistFileNames.AddItem(FString::Printf(TEXT("%sPlaylist.ini"),appGetGameName()));
	FFilename GameIni(GGameIni);
	// Add the game ini for downloading per object config
	PlaylistFileNames.AddItem(GameIni.GetCleanFilename());
	// Now build the loc file name from the ini filename
	PlaylistFileNames.AddItem(FString::Printf(TEXT("Engine.%s"),*appGetLanguageExt()));
	PlaylistFileNames.AddItem(FString::Printf(TEXT("%sGame.%s"),appGetGameName(),*appGetLanguageExt()));
}

/**
 * Converts the data into the structure used by the playlist manager
 *
 * @param Data the data that was downloaded
 */
void UOnlinePlaylistManager::ParsePlaylistPopulationData(const TArray<BYTE>& Data)
{
	// Make sure the string is null terminated
	((TArray<BYTE>&)Data).AddItem(0);
	// Convert to a string that we can work with
	FString StrData = ANSI_TO_TCHAR((ANSICHAR*)Data.GetTypedData());
	TArray<FString> Lines;
	// Now split into lines
	StrData.ParseIntoArray(&Lines,TEXT("\r\n"),TRUE);
	FString Token(TEXT("="));
	FString Right;
	// Mimic how the config cache stores them by removing the PopulationData= from the entries
	for (INT Index = 0; Index < Lines.Num(); Index++)
	{
		if (Lines(Index).Split(Token,NULL,&Right))
		{
			Lines(Index) = Right;
		}
	}
	// If there was data in the file, then import that into the array
	if (Lines.Num() > 0)
	{
		// Find the property on our object
		UArrayProperty* Array = FindField<UArrayProperty>(GetClass(),FName(TEXT("PopulationData")));
		if (Array != NULL)
		{
			const INT Size = Array->Inner->ElementSize;
			FScriptArray* ArrayPtr = (FScriptArray*)((BYTE*)this + Array->Offset);
			// Destroy any data that was already there
			Array->DestroyValue(ArrayPtr);
			// Size everything to the number of lines that were downloaded
			ArrayPtr->AddZeroed(Lines.Num(),Size);
			// Import each line of the population data
			for (INT ArrayIndex = Lines.Num() - 1, Count = 0; ArrayIndex >= 0; ArrayIndex--, Count++)
			{
				Array->Inner->ImportText(*Lines(ArrayIndex),(BYTE*)ArrayPtr->GetData() + Count * Size,PPF_ConfigOnly,this);
			}
		}
	}
	WorldwideTotalPlayers = RegionTotalPlayers = 0;
	// Total up the worldwide and region data
	for (INT DataIndex = 0; DataIndex < PopulationData.Num(); DataIndex++)
	{
		const FPlaylistPopulation& Data = PopulationData(DataIndex);
		WorldwideTotalPlayers += Data.WorldwideTotal;
		RegionTotalPlayers += Data.RegionTotal;
	}
}

/**
 * Determines whether an update of the playlist population information is needed or not
 *
 * @param DeltaTime the amount of time that has passed since the last tick
 */
void UOnlinePlaylistManager::Tick(FLOAT DeltaTime)
{
	UBOOL bNeedsAnUpdate = FALSE;
	// Determine if we've passed our update window and mark that we need an update
	// NOTE: to handle starting a match, reporting, and quiting, the code has to always
	// tick the interval so we don't over/under report
	NextPlaylistPopulationUpdateTime += DeltaTime;
	if (NextPlaylistPopulationUpdateTime >= PlaylistPopulationUpdateInterval &&
		PlaylistPopulationUpdateInterval > 0.f)
	{
		bNeedsAnUpdate = TRUE;
		NextPlaylistPopulationUpdateTime = 0.f;
	}
	// We can only update if we are the server
	AWorldInfo* WorldInfo = GWorld->GetWorldInfo();
	if (WorldInfo != NULL &&
		WorldInfo->NetMode != NM_Standalone &&
		WorldInfo->NetMode != NM_Client &&
		// Don't send updates when we aren't playing a playlist
		CurrentPlaylistId > MinPlaylistIdToReport)
	{
		if (bNeedsAnUpdate)
		{
			INT NumPlayers = 0;
			// Work through the controller list counting players and skipping bots
			for (AController* Controller = WorldInfo->ControllerList; Controller; Controller = Controller->NextController)
			{
				APlayerController* PC = Cast<APlayerController>(Controller);
				if (PC)
				{
					NumPlayers++;
				}
			}
			eventSendPlaylistPopulationUpdate(NumPlayers);
		}
	}
}


/**
 * Converts the data into the datacenter id
 *
 * @param Data the data that was downloaded
 */
void UOnlinePlaylistManager::ParseDataCenterId(const TArray<BYTE>& Data)
{
	// Make sure the string is null terminated
	((TArray<BYTE>&)Data).AddItem(0);
	// Convert to a string that we can work with
	const FString StrData = ANSI_TO_TCHAR((ANSICHAR*)Data.GetTypedData());
	// Find the property on our object
	UIntProperty* Property = FindField<UIntProperty>(GetClass(),FName(TEXT("DataCenterId")));
	if (Property != NULL)
	{
		// Import the text sent to us by the network layer
		if (Property->ImportText(*StrData,(BYTE*)this + Property->Offset,PPF_ConfigOnly,this) == NULL)
		{
			debugf(NAME_Error,
				TEXT("LoadConfig (%s): import failed for %s in: %s"),
				*GetPathName(),
				*Property->GetName(),
				*StrData);
		}
	}
}

/** @return true if the playlists should be refreshed, false otherwise */
UBOOL UOnlinePlaylistManager::ShouldRefreshPlaylists(void)
{
	return PlaylistRefreshInterval > 0.f &&
		(appSeconds() - LastPlaylistDownloadTime >= PlaylistRefreshInterval);
}

IMPLEMENT_CLASS(UOnlinePlaylistProvider);
IMPLEMENT_CLASS(UUIDataStore_OnlinePlaylists);

/* === UIDataStore interface === */	
/**
 * Loads the classes referenced by the ElementProviderTypes array.
 */
void UUIDataStore_OnlinePlaylists::LoadDependentClasses()
{
	Super::LoadDependentClasses();

	// for each configured provider type, load the UIResourceProviderClass associated with that resource type
	if ( ProviderClassName.Len() > 0 )
	{
		ProviderClass = LoadClass<UUIResourceDataProvider>(NULL, *ProviderClassName, NULL, LOAD_None, NULL);
		if ( ProviderClass == NULL )
		{
			debugf(NAME_Warning, TEXT("Failed to load class for %s"), *ProviderClassName);
		}
	}
	else
	{
		debugf(TEXT("No ProviderClassName specified for UUIDataStore_OnlinePlaylists"));
	}
}

/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * Initializes the ListElementProviders map
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
void UUIDataStore_OnlinePlaylists::OnRegister( ULocalPlayer* PlayerOwner )
{
	Super::OnRegister(PlayerOwner);

	InitializeListElementProviders();
}


/* === UUIDataStore_OnlinePlaylists interface === */
/**
 * Finds or creates the UIResourceDataProvider instances referenced use by online playlists, and stores the result by ranked or unranked provider types
 */
void UUIDataStore_OnlinePlaylists::InitializeListElementProviders()
{
	RankedDataProviders.Empty();
	UnrankedDataProviders.Empty();
	RecModeDataProviders.Empty();
	PrivateDataProviders.Empty();

	TArray<class UUIResourceDataProvider*>* CurrDataProviderList = NULL;

	// retrieve the list of ini sections that contain data for that provider class	
	TArray<FString> PlaylistSectionNames;
	if ( GConfig->GetPerObjectConfigSections(*ProviderClass->GetConfigName(), *ProviderClass->GetName(), PlaylistSectionNames) )
	{
		for ( INT SectionIndex = 0; SectionIndex < PlaylistSectionNames.Num(); SectionIndex++ )
		{
			INT POCDelimiterPosition = PlaylistSectionNames(SectionIndex).InStr(TEXT(" "));
			// we shouldn't be here if the name was included in the list
			check(POCDelimiterPosition!=INDEX_NONE);

			FName ObjectName = *PlaylistSectionNames(SectionIndex).Left(POCDelimiterPosition);
			if ( ObjectName != NAME_None )
			{
				UOnlinePlaylistProvider* Provider = Cast<UOnlinePlaylistProvider>( StaticFindObject(ProviderClass, ANY_PACKAGE, *ObjectName.ToString(), TRUE) );
				if ( Provider == NULL )
				{
					Provider = ConstructObject<UOnlinePlaylistProvider>(
						ProviderClass,
						this,
						ObjectName
						);
				}

				// Don't add this to the list if it's been disabled from enumeration
				if ( Provider != NULL && !Provider->bSkipDuringEnumeration )
				{
					INT MatchType = eventGetMatchTypeForPlaylistId(Provider->PlaylistId);

					// See the defines in OnlinePlaylistManager
					//Figure out what provider list this provider is going into
					CurrDataProviderList = NULL;
					switch (MatchType)
					{
						// unranked
						case UCONST_PLAYER_MATCH:
						{
							CurrDataProviderList = &UnrankedDataProviders;
							break;
						}
						// rec mode
						case UCONST_REC_MATCH: 
						{
							CurrDataProviderList = &RecModeDataProviders;					
							break;
						}
						// ranked
						case UCONST_RANKED_MATCH:
						{
							CurrDataProviderList = &RankedDataProviders;
							break;
						}
						// private
						case UCONST_PRIVATE_MATCH:
						{
							CurrDataProviderList = &PrivateDataProviders;
							break;
						}
					}
					//Insertion sort the provider into the appropriate list
					if (CurrDataProviderList != NULL)
					{
						INT Idx = 0;
						while(Idx < CurrDataProviderList->Num() && Provider->Priority < Cast<UOnlinePlaylistProvider>((*CurrDataProviderList)(Idx))->Priority )
						{
							Idx++;
						}
						CurrDataProviderList->InsertItem(Provider,Idx);
					}
				}
			}
		}
	}

	for ( INT ProviderIdx = 0; ProviderIdx < RankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = RankedDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < UnrankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = UnrankedDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < RecModeDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = RecModeDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < PrivateDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = PrivateDataProviders(ProviderIdx);
		Provider->eventInitializeProvider(!GIsGame);
	}
}

/**
 * Get the UIResourceDataProvider instances associated with the tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	out_Providers	receives the list of provider instances. this array is always emptied first.
 *
 * @return	the list of UIResourceDataProvider instances registered for ProviderTag.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetResourceProviders( FName ProviderTag, TArray<UUIResourceDataProvider*>& out_Providers ) const
{
	out_Providers.Empty();

	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
		{
			 out_Providers.AddItem(RankedDataProviders(ProviderIndex));
		}
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
		{
			out_Providers.AddItem(UnrankedDataProviders(ProviderIndex));
		}
	}
	else if ( ProviderTag == UCONST_RECMODEPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < RecModeDataProviders.Num(); ProviderIndex++ )
		{
			out_Providers.AddItem(RecModeDataProviders(ProviderIndex));
		}
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		for ( INT ProviderIndex = 0; ProviderIndex < PrivateDataProviders.Num(); ProviderIndex++ )
		{
			out_Providers.AddItem(PrivateDataProviders(ProviderIndex));
		}
	}


	return out_Providers.Num() > 0;
}

UBOOL UUIDataStore_OnlinePlaylists::GetPlaylistProvider(FName ProviderTag, INT ProviderIndex, UUIResourceDataProvider*& out_Provider)
{
	out_Provider = NULL;

	if (ProviderTag == UCONST_RANKEDPROVIDERTAG)
	{
		if ( RankedDataProviders.IsValidIndex(ProviderIndex) )
		{
			out_Provider = RankedDataProviders(ProviderIndex);
		}
	}
	else if ( ProviderTag == UCONST_UNRANKEDPROVIDERTAG)
	{
		if ( UnrankedDataProviders.IsValidIndex(ProviderIndex) )
		{
			out_Provider = UnrankedDataProviders(ProviderIndex);
		}
	}
	else if ( ProviderTag == UCONST_RECMODEPROVIDERTAG)
	{
		if ( RecModeDataProviders.IsValidIndex(ProviderIndex) )
		{
			out_Provider = RecModeDataProviders(ProviderIndex);
		}
	}
	else if ( ProviderTag == UCONST_PRIVATEPROVIDERTAG)
	{
		if ( PrivateDataProviders.IsValidIndex(ProviderIndex) )
		{
			out_Provider = PrivateDataProviders(ProviderIndex);
		}
	}

	return out_Provider != NULL;
}

/* === UObject interface === */
/** Required since maps are not yet supported by script serialization */
void UUIDataStore_OnlinePlaylists::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
	{
		UUIResourceDataProvider* ResourceProvider = RankedDataProviders(ProviderIndex);
		if ( ResourceProvider != NULL )
		{
			AddReferencedObject(ObjectArray, ResourceProvider);
		}
	}

	for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
	{
		UUIResourceDataProvider* ResourceProvider = UnrankedDataProviders(ProviderIndex);
		if ( ResourceProvider != NULL )
		{
			AddReferencedObject(ObjectArray, ResourceProvider);
		}
	}
}

void UUIDataStore_OnlinePlaylists::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if ( !Ar.IsPersistent() )
	{
		for ( INT ProviderIndex = 0; ProviderIndex < RankedDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* ResourceProvider = RankedDataProviders(ProviderIndex);
			Ar << ResourceProvider;
		}

		for ( INT ProviderIndex = 0; ProviderIndex < UnrankedDataProviders.Num(); ProviderIndex++ )
		{
			UUIResourceDataProvider* ResourceProvider = UnrankedDataProviders(ProviderIndex);
			Ar << ResourceProvider;
		}
	}
}

/**
 * Called from ReloadConfig after the object has reloaded its configuration data.
 */
void UUIDataStore_OnlinePlaylists::PostReloadConfig( UProperty* PropertyThatWasLoaded )
{
	Super::PostReloadConfig( PropertyThatWasLoaded );

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if ( PropertyThatWasLoaded == NULL || PropertyThatWasLoaded->GetFName() == TEXT("ProviderClassName") )
		{
			// reload the ProviderClass
			LoadDependentClasses();

			// the current list element providers are potentially no longer accurate, so we'll need to reload that list as well.
			InitializeListElementProviders();

			// now notify any widgets that are subscribed to this datastore that they need to re-resolve their bindings
			eventRefreshSubscribers();
		}
	}
}

/**
 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
 * to have natively serialized property values included in things like diffcommandlet output.
 *
 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
 *								the property and the map's value should be the textual representation of the property's value.  The property value should
 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
 *								as the delimiter between elements, etc.)
 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
 *
 * @return	return TRUE if property values were added to the map.
 */
UBOOL UUIDataStore_OnlinePlaylists::GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags/*=0*/ ) const
{
	UBOOL bResult = Super::GetNativePropertyValues(out_PropertyValues, ExportFlags);

	INT Count=0, LongestProviderTag=0;
	TMap<FString,FString> PropertyValues;
	for ( INT ProviderIdx = 0; ProviderIdx < RankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = RankedDataProviders(ProviderIdx);
		FString PropertyName = *FString::Printf(TEXT("RankedPlaylistProviders[%i]"), ProviderIdx);
		FString PropertyValue = Provider->GetName();

		LongestProviderTag = Max(LongestProviderTag, PropertyName.Len());
		PropertyValues.Set(*PropertyName, PropertyValue);
	}

	for ( INT ProviderIdx = 0; ProviderIdx < UnrankedDataProviders.Num(); ProviderIdx++ )
	{
		UUIResourceDataProvider* Provider = UnrankedDataProviders(ProviderIdx);
		FString PropertyName = *FString::Printf(TEXT("UnrankedPlaylistProviders[%i]"), ProviderIdx);
		FString PropertyValue = Provider->GetName();

		LongestProviderTag = Max(LongestProviderTag, PropertyName.Len());
		PropertyValues.Set(*PropertyName, PropertyValue);
	}

	for ( TMap<FString,FString>::TConstIterator It(PropertyValues); It; ++It )
	{
		const FString& ProviderTag = It.Key();
		const FString& ProviderName = It.Value();
	
		out_PropertyValues.Set(*ProviderTag, ProviderName.LeftPad(LongestProviderTag));
		bResult = TRUE;
	}

	return bResult;
}

#endif
