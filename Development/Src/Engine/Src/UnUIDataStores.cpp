/*=============================================================================
	UnUIDataStores.cpp: UI data store class implementations
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

// engine classes
#include "EnginePrivate.h"
#include "FConfigCacheIni.h"

// widgets and supporting UI classes
#include "EngineUserInterfaceClasses.h"
#include "EngineUIPrivateClasses.h"

IMPLEMENT_CLASS(UDataStoreClient);
IMPLEMENT_CLASS(UUIDataProvider);
	IMPLEMENT_CLASS(UUIDataStore);
		IMPLEMENT_CLASS(UUIDataStore_GameResource);
		IMPLEMENT_CLASS(UUIDataStore_DynamicResource);
		IMPLEMENT_CLASS(UUIDataStore_Settings);
		IMPLEMENT_CLASS(UUIDataStore_GameState);
		IMPLEMENT_CLASS(UUIDataStore_Fonts);
		IMPLEMENT_CLASS(UUIDataStore_Remote);
		IMPLEMENT_CLASS(UUIDataStore_StringAliasMap);
		IMPLEMENT_CLASS(UUIDataStore_StringBase);
		IMPLEMENT_CLASS(UUIDataStore_InputAlias);
	IMPLEMENT_CLASS(UUIPropertyDataProvider);
		IMPLEMENT_CLASS(UUIResourceDataProvider);
	IMPLEMENT_CLASS(UUIResourceCombinationProvider);

IMPLEMENT_CLASS(UUIDataStore_Registry);
IMPLEMENT_CLASS(UUIDataStoreSubscriber);
IMPLEMENT_CLASS(UUIDataStorePublisher);

#define ARRAY_DELIMITER TEXT(";")

typedef TMap< FName, TMap<FName,TArray<FString> > > DynamicCollectionValueMap;


/* ==========================================================================================================
	UDataStoreClient
========================================================================================================== */
/**
 * Loads and initializes all data store classes contained by the DataStoreClasses array
 */
void UDataStoreClient::InitializeDataStores()
{
	for ( INT DataStoreIndex = 0; DataStoreIndex < GlobalDataStoreClasses.Num(); DataStoreIndex++ )
	{
		UClass* DataStoreClass = LoadClass<UUIDataStore>(NULL, *GlobalDataStoreClasses(DataStoreIndex), NULL, LOAD_None, NULL);
		if (DataStoreClass != NULL )
		{
			// Allow the data store to load any dependent classes it will need later
			UUIDataStore* DefaultStore = DataStoreClass->GetDefaultObject<UUIDataStore>();
			DefaultStore->LoadDependentClasses();

			UUIDataStore* DataStore = CreateDataStore(DataStoreClass);
			if ( DataStore != NULL )
			{
				RegisterDataStore(DataStore);
			}
		}
		else
		{
			debugf(NAME_Warning, TEXT("Failed to load GlobalDataStoreClass '%s'"), *GlobalDataStoreClasses(DataStoreIndex));
		}
	}

	// PlayerDataStores are instanced when the player is created, so all we need to do is make sure the class is loaded to be compatible with streaming
	for ( INT PlayerDataStoreIndex = 0; PlayerDataStoreIndex < PlayerDataStoreClassNames.Num(); PlayerDataStoreIndex++ )
	{
		UClass* DataStoreClass = LoadClass<UUIDataStore>(NULL, *PlayerDataStoreClassNames(PlayerDataStoreIndex), NULL, LOAD_None, NULL);
		if ( DataStoreClass == NULL )
		{
			debugf(NAME_Warning, TEXT("Failed to load player data store class '%s'"), *PlayerDataStoreClassNames(PlayerDataStoreIndex));
		}
		else
		{
			// store the class in our array so that it doesn't get GC'd.
			PlayerDataStoreClasses.AddUniqueItem(DataStoreClass);
			// Allow the data store to load any dependent classes it will need later
			UUIDataStore* DefaultStore = DataStoreClass->GetDefaultObject<UUIDataStore>();
			DefaultStore->LoadDependentClasses();

			if (GIsEditor && !GIsGame)
			{
				// since we won't have any PlayerControllers in the editor, add all PlayerDataStores to the GlobalDataStores array
				UUIDataStore* PlayerDataStore = CreateDataStore(DataStoreClass);
				if ( PlayerDataStore != NULL )
				{
					RegisterDataStore(PlayerDataStore);
				}
			}
		}
	}
}

/**
 * Creates and initializes an instance of the data store class specified.
 *
 * @param	DataStoreClass	the data store class to create an instance of.  DataStoreClass should be a child class
 *							of UUIDataStore
 *
 * @return	a pointer to an instance of the data store class specified.
 */
UUIDataStore* UDataStoreClient::CreateDataStore( UClass* DataStoreClass )
{
	UUIDataStore* Result = NULL;
	if ( DataStoreClass != NULL && DataStoreClass->IsChildOf(UUIDataStore::StaticClass()) )
	{
		Result = ConstructObject<UUIDataStore>(DataStoreClass, this);
		Result->InitializeDataStore();
	}

	return Result;
}


/**
 * Finds the data store indicated by DataStoreTag and returns a pointer to it.
 *
 * @param	DataStoreTag	A name corresponding to the 'Tag' property of a data store
 * @param	PlayerOwner		used for resolving the correct data stores in split-screen games.
 *
 * @return	a pointer to the data store that has a Tag corresponding to DataStoreTag, or NULL if no data
 *			were found with that tag.
 */
UUIDataStore* UDataStoreClient::FindDataStore( FName DataStoreTag, ULocalPlayer* PlayerOwner/*=NULL*/ )
{
	UUIDataStore* Result = NULL;

	if ( DataStoreTag != NAME_None )
	{
		// search the player data stores first
		if ( PlayerOwner != NULL )
		{
			INT PlayerDataIndex = FindPlayerDataStoreIndex(PlayerOwner);
			if ( PlayerDataIndex != INDEX_NONE )
			{
				FPlayerDataStoreGroup& DataStoreGroup = PlayerDataStores(PlayerDataIndex);
				for ( INT DataStoreIndex = 0; DataStoreIndex < DataStoreGroup.DataStores.Num(); DataStoreIndex++ )
				{
					if ( DataStoreGroup.DataStores(DataStoreIndex)->GetDataStoreID() == DataStoreTag )
					{
						Result = DataStoreGroup.DataStores(DataStoreIndex);
						break;
					}
				}
			}
		}

		if ( Result == NULL )
		{
			// now search the global data stores
			for ( INT DataStoreIndex = 0; DataStoreIndex < GlobalDataStores.Num(); DataStoreIndex++ )
			{
				if ( GlobalDataStores(DataStoreIndex)->GetDataStoreID() == DataStoreTag )
				{
					Result = GlobalDataStores(DataStoreIndex);
					break;
				}
			}
		}
	}

	return Result;
}

/**
 * Adds a new data store to the GlobalDataStores array.
 *
 * @param	DataStore		the data store to add
 * @param	PlayerOwner		the player that this data store should be associated with.  If specified, the data store
 *							is added to the list of data stores for that player, rather than the global data stores array.
 *
 * @return	TRUE if the data store was successfully added, or if the data store was already in the list.
 */
UBOOL UDataStoreClient::RegisterDataStore( UUIDataStore* DataStore, ULocalPlayer* PlayerOwner/*=NULL*/ )
{
	UBOOL bResult = FALSE;
	if ( DataStore != NULL )
	{
		// this is the data store tag that will be used to reference the data store
		FName DataStoreID = DataStore->GetDataStoreID();

		// this is the index into the PlayerDataStores array for PlayerOwner, if specified
		INT PlayerDataIndex = INDEX_NONE;
		UBOOL bDataStoreRegistered = FALSE;

		// if a player was specified, find the location of this player's data stores in the PlayerDataStores array.
		if ( PlayerOwner != NULL )
		{
			PlayerDataIndex = FindPlayerDataStoreIndex(PlayerOwner);
			if ( PlayerDataIndex == INDEX_NONE )
			{
				// this is the first data store we're registering for this player, so add an entry for this player now
				PlayerDataIndex = PlayerDataStores.AddZeroed();
			}

			check(PlayerDataStores.IsValidIndex(PlayerDataIndex));
			FPlayerDataStoreGroup& DataStoreGroup = PlayerDataStores(PlayerDataIndex);
			bDataStoreRegistered = DataStoreGroup.DataStores.ContainsItem(DataStore);

			// if this data store group was just added, we'll need to assign the PlayerOwner.
			DataStoreGroup.PlayerOwner = PlayerOwner;
		}

		bDataStoreRegistered = bDataStoreRegistered || GlobalDataStores.ContainsItem(DataStore);

		// if we already have this data store in the list, indicate success
		if ( bDataStoreRegistered )
		{
			bResult = TRUE;
		}
		else if ( DataStoreID != NAME_None )
		{
			// make sure there isn't already a data store with this tag in the list
			UUIDataStore* ExistingDataStore = FindDataStore(DataStoreID,PlayerOwner);
			if ( ExistingDataStore == NULL )
			{
				// if this data store is associated with a player, add it to the player's data store group
				if ( PlayerOwner != NULL && PlayerDataIndex != INDEX_NONE )
				{
					checkSlow(PlayerDataStores.IsValidIndex(PlayerDataIndex));
					FPlayerDataStoreGroup& DataStoreGroup = PlayerDataStores(PlayerDataIndex);
					DataStoreGroup.DataStores.AddItem(DataStore);

					bResult = TRUE;
				}
				else
				{
                    GlobalDataStores.AddItem(DataStore);
					bResult = TRUE;
				}

				// notify the data store that it has been registered
				DataStore->OnRegister(PlayerOwner);
			}
			else
			{
				debugf(TEXT("Failed to register data store (%s) '%s': existing data store with identical tag '%s'"), *DataStoreID.ToString(), *DataStore->GetFullName(), *ExistingDataStore->GetFullName());
			}
		}
		else
		{
			debugf(TEXT("Failed to register data store '%s': doesn't have a valid tag"), *DataStore->GetFullName());
		}
	}

	return bResult;
}

/**
 * Removes a data store from the GlobalDataStores array.
 *
 * @param	DataStore	the data store to remove
 *
 * @return	TRUE if the data store was successfully removed, or if the data store wasn't in the list.
 */
UBOOL UDataStoreClient::UnregisterDataStore( UUIDataStore* DataStore )
{
	UBOOL bResult = FALSE;

	if ( DataStore != NULL )
	{
		INT DataStoreIndex = GlobalDataStores.FindItemIndex(DataStore);

		// if this data store isn't in the list, treat as successful
		if ( DataStoreIndex == INDEX_NONE )
		{
			// search through the player data stores
			for ( INT PlayerDataIndex = 0; PlayerDataIndex < PlayerDataStores.Num(); PlayerDataIndex++ )
			{
				FPlayerDataStoreGroup& DataStoreGroup = PlayerDataStores(PlayerDataIndex);
				DataStoreIndex = DataStoreGroup.DataStores.FindItemIndex(DataStore);
				if ( DataStoreIndex != INDEX_NONE )
				{
					ULocalPlayer* PlayerOwner = DataStoreGroup.PlayerOwner;

					//@todo - perhaps add a hook to the UIDataStore class for overriding removal
					DataStoreGroup.DataStores.Remove(DataStoreIndex);

					// notify the data store that is has been unregistered
					DataStore->OnUnregister(PlayerOwner);

					// if this is the last data store associated with this player, remove the data store group as well
					if ( DataStoreGroup.DataStores.Num() == 0 )
					{
						PlayerDataStores.Remove(PlayerDataIndex);
					}

					bResult = TRUE;
					break;
				}
			}

			//@todo - hmm...should we return TRUE if the data store wasn't registered?
			bResult = TRUE;
		}
		else
		{
			//@todo - perhaps add a hook to the UIDataStore class for overriding removal
			GlobalDataStores.Remove(DataStoreIndex);

			// notify the data store that is has been unregistered
			DataStore->OnUnregister(NULL);
			bResult = TRUE;
		}
	}

	return bResult;
}

/**
 * Finds the index into the PlayerDataStores array for the data stores associated with the specified player.
 *
 * @param	PlayerOwner		the player to search for associated data stores for.
 */
INT UDataStoreClient::FindPlayerDataStoreIndex( ULocalPlayer* PlayerOwner ) const
{
	INT Result = INDEX_NONE;

	if ( GIsGame )
	{
		for ( INT DataStoreIndex = 0; DataStoreIndex < PlayerDataStores.Num(); DataStoreIndex++ )
		{
			const FPlayerDataStoreGroup& DataStoreGroup = PlayerDataStores(DataStoreIndex);
			if ( DataStoreGroup.PlayerOwner == PlayerOwner )
			{
				Result = DataStoreIndex;
				break;
			}
		}
	}
	else
	{
		if (PlayerDataStores.Num() > 0)
		{
			// Editor only ever has one player
			Result = 0;
		}
	}

	return Result;
}


/* ==========================================================================================================
	UUIDataStore
========================================================================================================== */
/**
 * Hook for performing any initialization required for this data store
 */
void UUIDataStore::InitializeDataStore()
{
	// nothing
}

/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
void UUIDataStore::OnRegister( ULocalPlayer* PlayerOwner )
{
	eventRegistered(PlayerOwner);
}

/**
 * Called when this data store is removed from the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
void UUIDataStore::OnUnregister( ULocalPlayer* PlayerOwner )
{
	eventUnregistered(PlayerOwner);
}


/* ==========================================================================================================
	UUIDataStore_Registry
========================================================================================================== */

/**
 * Creates the data provider for this registry data store.
 */
void UUIDataStore_Registry::InitializeDataStore()
{
	Super::InitializeDataStore();
}


/* ==========================================================================================================
	UUIDataStore_InputAlias
========================================================================================================== */
IMPLEMENT_COMPARE_CONSTREF(FUIDataStoreInputAlias, UnUIDataStores, { return appStricmp(*A.AliasName.ToString(), *B.AliasName.ToString()); })

/* === UUIDataStore_InputAlias interface === */
/**
 * Populates the InputAliasLookupMap based on the elements of the InputAliases array.
 */
void UUIDataStore_InputAlias::InitializeLookupMap()
{
	if ( InputAliases.Num() > 0 )
	{
		Sort<USE_COMPARE_CONSTREF(FUIDataStoreInputAlias,UnUIDataStores)>(&InputAliases(0), InputAliases.Num());
	}

	// remove all existing elements
	InputAliasLookupMap.Empty(InputAliases.Num());

	for ( INT AliasIdx = 0; AliasIdx < InputAliases.Num(); AliasIdx++ )
	{
		FUIDataStoreInputAlias& Alias = InputAliases(AliasIdx);
		InputAliasLookupMap.Set(Alias.AliasName, AliasIdx);
	}
}
	
/**
 * @return	the platform that should be used (by default) when retrieving data associated with input aliases
 */
EInputPlatformType UUIDataStore_InputAlias::GetDefaultPlatform() const
{
	//@todo ronp - if the owning player is using an alternate input device, it may affect which platform we return
#if XBOX
	EInputPlatformType Platform = IPT_360;
#elif PS3
	EInputPlatformType Platform = IPT_PS3;
#else
	EInputPlatformType Platform = IPT_PC;
#endif

	return Platform;
}

/**
 * Retrieves the button icon font markup string for an input alias
 *
 * @param	DesiredAlias		the name of the alias (i.e. Accept) to get the markup string for
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the markup string for the button icon associated with the alias.
 */
FString UUIDataStore_InputAlias::GetAliasFontMarkup( FName DesiredAlias, /*EInputPlatformType*/BYTE OverridePlatform/*=IPT_MAX*/ ) const
{
	FString Result;

	INT AliasIdx = FindInputAliasIndex(DesiredAlias);
	if ( InputAliases.IsValidIndex(AliasIdx) )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIdx);

		EInputPlatformType Platform = GetDefaultPlatform();
		if ( OverridePlatform < IPT_MAX )
		{
			Platform = static_cast<EInputPlatformType>(OverridePlatform);
		}

		check(Platform<ARRAY_COUNT(Alias.PlatformInputKeys));
		Result = Alias.PlatformInputKeys[Platform].ButtonFontMarkupString;
	}

	return Result;
}

/**
 * Retrieves the button icon font markup string for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the markup string for.
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the markup string for the button icon associated with the alias.
 */
FString UUIDataStore_InputAlias::GetAliasFontMarkupByIndex( INT AliasIndex, /*EInputPlatformType*/BYTE OverridePlatform/*=IPT_MAX*/ ) const
{
	FString Result;

	if ( InputAliases.IsValidIndex(AliasIndex) )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIndex);

		EInputPlatformType Platform = GetDefaultPlatform();
		if ( OverridePlatform < IPT_MAX )
		{
			Platform = static_cast<EInputPlatformType>(OverridePlatform);
		}

		check(Platform<ARRAY_COUNT(Alias.PlatformInputKeys));
		Result = Alias.PlatformInputKeys[Platform].ButtonFontMarkupString;
	}

	return Result;
}

/**
 * Retrieves the associated input key name for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the input key for.
 * @param	OverridePlatform	specifies which platform's input key is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the name of the input key (i.e. LeftMouseButton) which triggers the alias.
 */
FName UUIDataStore_InputAlias::GetAliasInputKeyName( FName DesiredAlias, /*EInputPlatformType*/BYTE OverridePlatform/*=IPT_MAX*/ ) const
{
	FName Result = NAME_None;

	INT AliasIdx = FindInputAliasIndex(DesiredAlias);
	if ( InputAliases.IsValidIndex(AliasIdx) )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIdx);

		EInputPlatformType Platform = GetDefaultPlatform();
		if ( OverridePlatform < IPT_MAX )
		{
			Platform = static_cast<EInputPlatformType>(OverridePlatform);
		}

		check(Platform<ARRAY_COUNT(Alias.PlatformInputKeys));
		Result = Alias.PlatformInputKeys[Platform].InputKeyData.InputKeyName;
	}

	return Result;
}

/**
 * Retrieves the associated input key name for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the input key for.
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the name of the input key (i.e. LeftMouseButton) which triggers the alias.
 */
FName UUIDataStore_InputAlias::GetAliasInputKeyNameByIndex( INT AliasIndex, /*EInputPlatformType*/BYTE OverridePlatform/*=IPT_MAX*/ ) const
{
	FName Result = NAME_None;

	if ( InputAliases.IsValidIndex(AliasIndex) )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIndex);

		EInputPlatformType Platform = GetDefaultPlatform();
		if ( OverridePlatform < IPT_MAX )
		{
			Platform = static_cast<EInputPlatformType>(OverridePlatform);
		}

		check(Platform<ARRAY_COUNT(Alias.PlatformInputKeys));
		Result = Alias.PlatformInputKeys[Platform].InputKeyData.InputKeyName;
	}

	return Result;
}

/**
 * Retrieves both the input key name and modifier keys for an input alias
 *
 * @param	DesiredAlias		the name of the alias (i.e. Accept) to get the input key data for
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the struct containing the input key name and modifier keys associated with the alias.
 */
UBOOL UUIDataStore_InputAlias::GetAliasInputKeyData( FRawInputKeyEventData& out_InputKeyData, FName DesiredAlias, /*EInputPlatformType*/BYTE OverridePlatform/*=IPT_MAX*/ ) const
{
	UBOOL bResult = FALSE;

	INT AliasIdx = FindInputAliasIndex(DesiredAlias);
	if ( InputAliases.IsValidIndex(AliasIdx) )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIdx);

		EInputPlatformType Platform = GetDefaultPlatform();
		if ( OverridePlatform < IPT_MAX )
		{
			Platform = static_cast<EInputPlatformType>(OverridePlatform);
		}

		check(Platform<ARRAY_COUNT(Alias.PlatformInputKeys));
		out_InputKeyData = Alias.PlatformInputKeys[Platform].InputKeyData;
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Retrieves both the input key name and modifier keys for an input alias
 *
 * @param	AliasIndex			the index [into the InputAliases array] for the alias to get the input key data for.
 * @param	OverridePlatform	specifies which platform's markup string is desired; if not specified, uses the current
 *								platform, taking into account whether the player is using a gamepad (PC) or a keyboard (console).
 *
 * @return	the struct containing the input key name and modifier keys associated with the alias.
 */
UBOOL UUIDataStore_InputAlias::GetAliasInputKeyDataByIndex( FRawInputKeyEventData& out_InputKeyData, INT AliasIndex, /*EInputPlatformType*/BYTE OverridePlatform/*=IPT_MAX*/ ) const
{
	UBOOL bResult = FALSE;

	if ( InputAliases.IsValidIndex(AliasIndex) )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIndex);

		EInputPlatformType Platform = GetDefaultPlatform();
		if ( OverridePlatform < IPT_MAX )
		{
			Platform = static_cast<EInputPlatformType>(OverridePlatform);
		}

		check(Platform<ARRAY_COUNT(Alias.PlatformInputKeys));
		out_InputKeyData = Alias.PlatformInputKeys[Platform].InputKeyData;
		bResult = TRUE;
	}

	return bResult;
}

/**
 * Finds the location [in the InputAliases array] for an input alias.
 *
 * @param	DesiredAlias	the name of the alias (i.e. Accept) to find
 *
 * @return	the index into the InputAliases array for the alias, or INDEX_NONE if it doesn't exist.
 */
INT UUIDataStore_InputAlias::FindInputAliasIndex( FName DesiredAlias ) const
{
	INT Result = INDEX_NONE;

	if ( DesiredAlias != NAME_None )
	{
		const INT* pIdx = InputAliasLookupMap.Find(DesiredAlias);
		if ( pIdx != NULL )
		{
			Result = *pIdx;
		}
	}

	return Result;
}

/**
 * Determines whether an input alias is supported on a particular platform.
 *
 * @param	DesiredAlias		the name of the alias (i.e. Accept) to check
 * @param	DesiredPlatform		the platform to check for an input key
 *
 * @return	TRUE if the alias has a corresponding input key for the specified platform.
 */
UBOOL UUIDataStore_InputAlias::HasAliasMappingForPlatform( FName DesiredAlias, /*EInputPlatformType*/BYTE DesiredPlatform ) const
{
	UBOOL bResult = FALSE;

	INT AliasIdx = FindInputAliasIndex(DesiredAlias);
	if ( InputAliases.IsValidIndex(AliasIdx) && DesiredPlatform < IPT_MAX )
	{
		const FUIDataStoreInputAlias& Alias = InputAliases(AliasIdx);
		bResult = Alias.PlatformInputKeys[DesiredPlatform].InputKeyData.InputKeyName != NAME_None;
	}

	return bResult;
}

/* === UUIDataStore interface === */
/**
 * Hook for performing any initialization required for this data store.
 *
 * This version builds the InputAliasLookupMap based on the elements in the InputAliases array.
 */
void UUIDataStore_InputAlias::InitializeDataStore()
{
	Super::InitializeDataStore();

	InitializeLookupMap();
}

/* ==========================================================================================================
	UUIDataStore_GameResource
========================================================================================================== */
/**
 * Finds the index for the GameResourceDataProvider with a tag matching ProviderTag.
 *
 * @return	the index into the ElementProviderTypes array for the GameResourceDataProvider element that has the
 *			tag specified, or INDEX_NONE if there are no elements of the ElementProviderTypes array that have that tag.
 */
INT UUIDataStore_GameResource::FindProviderTypeIndex( FName ProviderTag ) const
{
	INT Result = INDEX_NONE;

	for ( INT ProviderIndex = 0; ProviderIndex < ElementProviderTypes.Num(); ProviderIndex++ )
	{
		const FGameResourceDataProvider& Provider = ElementProviderTypes(ProviderIndex);
		if ( Provider.ProviderTag == ProviderTag )
		{
			Result = ProviderIndex;
			break;
		}
	}

	return Result;
}

/**
 * Get the UIResourceDataProvider instances associated with the tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ElementProviderTypes array.
 * @param	out_Providers	receives the list of provider instances. this array is always emptied first.
 *
 * @return	the list of UIResourceDataProvider instances registered for ProviderTag.
 */
UBOOL UUIDataStore_GameResource::GetResourceProviders( FName ProviderTag, TArray<UUIResourceDataProvider*>& out_Providers ) const
{
	out_Providers.Empty();
	ListElementProviders.MultiFind(ProviderTag, out_Providers);
	// we may support this data provider type, but there just may not be any instances created, so check our
	// list of providers as well
	return out_Providers.Num() > 0 || FindProviderTypeIndex(ProviderTag) != INDEX_NONE;
}

/* === UUIDataStore_GameResource interface === */
/**
 * Finds or creates the UIResourceDataProvider instances referenced by ElementProviderTypes, and stores the result
 * into the ListElementProvider map.
 */
void UUIDataStore_GameResource::InitializeListElementProviders()
{
	ListElementProviders.Empty();

	// for each configured provider type, retrieve the list of ini sections that contain data for that provider class
	for ( INT ProviderTypeIndex = 0; ProviderTypeIndex < ElementProviderTypes.Num(); ProviderTypeIndex++ )
	{
		FGameResourceDataProvider& ProviderType = ElementProviderTypes(ProviderTypeIndex);

		UClass* ProviderClass = ProviderType.ProviderClass;

		TArray<FString> GameTypeResourceSectionNames;
		if ( GConfig->GetPerObjectConfigSections(*ProviderClass->GetConfigName(), *ProviderClass->GetName(), GameTypeResourceSectionNames) )
		{
			for ( INT SectionIndex = 0; SectionIndex < GameTypeResourceSectionNames.Num(); SectionIndex++ )
			{
				INT POCDelimiterPosition = GameTypeResourceSectionNames(SectionIndex).InStr(TEXT(" "));
				// we shouldn't be here if the name was included in the list
				check(POCDelimiterPosition!=INDEX_NONE);

				FName ObjectName = *GameTypeResourceSectionNames(SectionIndex).Left(POCDelimiterPosition);
				if ( ObjectName != NAME_None )
				{
					UUIResourceDataProvider* Provider = Cast<UUIResourceDataProvider>( StaticFindObject(ProviderClass, ANY_PACKAGE, *ObjectName.ToString(), TRUE) );
					if ( Provider == NULL ||
						// If this object is pending death, resurrect it
						Provider->IsPendingKill() )
					{
						Provider = ConstructObject<UUIResourceDataProvider>(
							ProviderClass,
							this,
							ObjectName
						);
					}

					// Don't add this to the list if it's been disabled from enumeration
					if ( Provider != NULL && !Provider->bSkipDuringEnumeration )
					{
						ListElementProviders.Add(ProviderType.ProviderTag, Provider);
					}
				}
			}
		}
	}

	for ( TMultiMap<FName,UUIResourceDataProvider*>::TIterator It(ListElementProviders); It; ++It )
	{
		UUIResourceDataProvider* Provider = It.Value();
		Provider->eventInitializeProvider(!GIsGame);
	}
}

/* === UIDataStore interface === */	
/**
 * Loads the classes referenced by the ElementProviderTypes array.
 */
void UUIDataStore_GameResource::LoadDependentClasses()
{
	Super::LoadDependentClasses();

	// for each configured provider type, load the UIResourceProviderClass associated with that resource type
	for ( INT ProviderTypeIndex = ElementProviderTypes.Num() - 1; ProviderTypeIndex >= 0; ProviderTypeIndex-- )
	{
		FGameResourceDataProvider& ProviderType = ElementProviderTypes(ProviderTypeIndex);
		if ( ProviderType.ProviderClassName.Len() > 0 )
		{
			ProviderType.ProviderClass = LoadClass<UUIResourceDataProvider>(NULL, *ProviderType.ProviderClassName, NULL, LOAD_None, NULL);
			if ( ProviderType.ProviderClass == NULL )
			{
				debugf(NAME_Warning, TEXT("Failed to load class for ElementProviderType %i: %s (%s)"), ProviderTypeIndex, *ProviderType.ProviderTag.ToString(), *ProviderType.ProviderClassName);

				// if we weren't able to load the specified class, remove this provider type from the list.
				ElementProviderTypes.Remove(ProviderTypeIndex);
			}
		}
		else
		{
			debugf(TEXT("No ProviderClassName specified for ElementProviderType %i: %s"), ProviderTypeIndex, *ProviderType.ProviderTag.ToString());
		}
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
void UUIDataStore_GameResource::OnRegister( ULocalPlayer* PlayerOwner )
{
	Super::OnRegister(PlayerOwner);
#if WIIU
	// @todo wiiu: Why is this needed on wiiu, but apparently not on other platforms?
	// without it, InitListElementProviders will crash with a NULL class. is PostReloadConfig
	// not getting called but maybe it is on other platforms?
	// reload the ProviderClass for each ElementProviderType
	LoadDependentClasses();
#endif
	InitializeListElementProviders();
}

/* === UObject interface === */
/** Required since maps are not yet supported by script serialization */
void UUIDataStore_GameResource::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	for ( TMultiMap<FName,UUIResourceDataProvider*>::TIterator It(ListElementProviders); It; ++It )
	{
		UUIResourceDataProvider* ResourceProvider = It.Value();
		if ( ResourceProvider != NULL )
		{
			AddReferencedObject(ObjectArray, ResourceProvider);
		}
	}
}

void UUIDataStore_GameResource::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if ( !Ar.IsPersistent() )
	{
		for ( TMultiMap<FName,UUIResourceDataProvider*>::TIterator It(ListElementProviders); It; ++It )
		{
			UUIResourceDataProvider* ResourceProvider = It.Value();
			Ar << ResourceProvider;
		}
	}
}


/**
 * Called from ReloadConfig after the object has reloaded its configuration data.
 */
void UUIDataStore_GameResource::PostReloadConfig( UProperty* PropertyThatWasLoaded )
{
	Super::PostReloadConfig( PropertyThatWasLoaded );

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if ( PropertyThatWasLoaded == NULL || PropertyThatWasLoaded->GetFName() == TEXT("ElementProviderTypes") )
		{
			// reload the ProviderClass for each ElementProviderType
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
UBOOL UUIDataStore_GameResource::GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags/*=0*/ ) const
{
	UBOOL bResult = Super::GetNativePropertyValues(out_PropertyValues, ExportFlags);

	INT Count=0, LongestProviderTag=0;

	TMap<FString,FString> PropertyValues;
	for ( INT ResourceIdx = 0; ResourceIdx < ElementProviderTypes.Num(); ResourceIdx++ )
	{
		const FGameResourceDataProvider& Definition = ElementProviderTypes(ResourceIdx);
		TArray<UUIResourceDataProvider*> Providers;
		ListElementProviders.MultiFind(Definition.ProviderTag, Providers);

		for ( INT ProviderIdx = 0; ProviderIdx < Providers.Num(); ProviderIdx++ )
		{
			UUIResourceDataProvider* Provider = Providers(ProviderIdx);
			FString PropertyName = *FString::Printf(TEXT("ListElementProviders[%i].%s[%i]"), ResourceIdx, *Definition.ProviderTag.ToString(), ProviderIdx);
			FString PropertyValue = Provider->GetName();

			LongestProviderTag = Max(LongestProviderTag, PropertyName.Len());
			PropertyValues.Set(*PropertyName, PropertyValue);
		}
	}

	for ( TMap<FString,FString>::TConstIterator It(PropertyValues); It; ++It )
	{
		const FString& ProviderTag = It.Key();
		const FString& ProviderName = It.Value();

		out_PropertyValues.Set(*ProviderTag, ProviderName.LeftPad(LongestProviderTag));
		bResult = TRUE;
	}

	return bResult || ListElementProviders.Num() > 0;
}

/* ==========================================================================================================
	UUIDataStore_DynamicResource
========================================================================================================== */
/**
 * Finds the index for the GameResourceDataProvider with a tag matching ProviderTag.
 *
 * @return	the index into the ElementProviderTypes array for the GameResourceDataProvider element that has the
 *			tag specified, or INDEX_NONE if there are no elements of the ResourceProviderDefinitions array that have that tag.
 */
INT UUIDataStore_DynamicResource::FindProviderTypeIndex( FName ProviderTag ) const
{
	INT Result = INDEX_NONE;

	for ( INT ProviderIndex = 0; ProviderIndex < ResourceProviderDefinitions.Num(); ProviderIndex++ )
	{
		const FDynamicResourceProviderDefinition& Provider = ResourceProviderDefinitions(ProviderIndex);
		if ( Provider.ProviderTag == ProviderTag )
		{
			Result = ProviderIndex;
			break;
		}
	}

	return Result;
}

/**
 * Get the UIResourceCombinationProvider instances associated with the tag.
 *
 * @param	ProviderTag		the tag to find instances for; should match the ProviderTag value of an element in the ResourceProviderDefinitions array.
 * @param	out_Providers	receives the list of provider instances. this array is always emptied first.
 *
 * @return	the list of UIResourceDataProvider instances registered for ProviderTag.
 */
UBOOL UUIDataStore_DynamicResource::GetResourceProviders( FName ProviderTag, TArray<UUIResourceCombinationProvider*>& out_Providers ) const
{
	out_Providers.Empty();
 	ResourceProviders.MultiFind(ProviderTag, out_Providers);

	// we may support this data provider type, but there just may not be any instances created, so check our
	// list of providers as well
	return out_Providers.Num() > 0 || FindProviderTypeIndex(ProviderTag) != INDEX_NONE;
}

/**
 * Re-initializes all dynamic providers.
 */
void UUIDataStore_DynamicResource::OnLoginChange(BYTE LocalUserNum)
{
	InitializeListElementProviders();
}

/* === UUIDataStore_DynamicResource interface === */
/**
 * Finds or creates the UIResourceDataProvider instances referenced by ResourceProviderDefinitions, and stores the result
 * into the ListElementProvider map.
 */
void UUIDataStore_DynamicResource::InitializeListElementProviders()
{
	ResourceProviders.Empty();

	if ( GameResourceDataStore != NULL )
	{
		TMap<UUIResourceCombinationProvider*,UUIResourceDataProvider*> StaticProviderMap;

		// for each configured provider type, retrieve the list of ini sections that contain data for that provider class
		for ( INT ProviderTypeIndex = 0; ProviderTypeIndex < ResourceProviderDefinitions.Num(); ProviderTypeIndex++ )
		{
			FDynamicResourceProviderDefinition& ProviderType = ResourceProviderDefinitions(ProviderTypeIndex);
			UClass* ProviderClass = ProviderType.ProviderClass;

			TArray<UUIResourceDataProvider*> StaticDataProviders;
			GameResourceDataStore->GetResourceProviders(ProviderType.ProviderTag, StaticDataProviders);

			for ( INT ProviderIdx = StaticDataProviders.Num() - 1; ProviderIdx >= 0; ProviderIdx-- )
			{
				UUIResourceDataProvider* StaticProvider = StaticDataProviders(ProviderIdx);
				UUIResourceCombinationProvider* Provider = Cast<UUIResourceCombinationProvider>(StaticFindObject(ProviderClass, this, *StaticProvider->GetName(), FALSE));
				if ( Provider == NULL )
				{
					Provider = ConstructObject<UUIResourceCombinationProvider>(ProviderClass, this, StaticProvider->GetFName());
				}

				if ( Provider != NULL )
				{
					StaticProviderMap.Set(Provider, StaticProvider);
					ResourceProviders.Add(ProviderType.ProviderTag, Provider);
				}
			}
		}

		for ( TMultiMap<FName,UUIResourceCombinationProvider*>::TIterator It(ResourceProviders); It; ++It )
		{
			UUIResourceCombinationProvider* Provider = It.Value();
			Provider->eventInitializeProvider(!GIsGame, StaticProviderMap.FindRef(Provider), ProfileProvider);
		}
	}
}


/* === UIDataStore interface === */	
/**
 * Loads the classes referenced by the ResourceProviderDefinitions array.
 */
void UUIDataStore_DynamicResource::LoadDependentClasses()
{
	Super::LoadDependentClasses();

	// for each configured provider type, load the UIResourceProviderClass associated with that resource type
	for ( INT ProviderTypeIndex = ResourceProviderDefinitions.Num() - 1; ProviderTypeIndex >= 0; ProviderTypeIndex-- )
	{
		FDynamicResourceProviderDefinition& ProviderType = ResourceProviderDefinitions(ProviderTypeIndex);
		if ( ProviderType.ProviderClassName.Len() > 0 )
		{
			ProviderType.ProviderClass = LoadClass<UUIResourceCombinationProvider>(NULL, *ProviderType.ProviderClassName, NULL, LOAD_None, NULL);
			if ( ProviderType.ProviderClass == NULL )
			{
				debugf(NAME_Warning, TEXT("Failed to load class for ElementProviderType %i: %s (%s)"), ProviderTypeIndex, *ProviderType.ProviderTag.ToString(), *ProviderType.ProviderClassName);

				// if we weren't able to load the specified class, remove this provider type from the list.
				ResourceProviderDefinitions.Remove(ProviderTypeIndex);
			}
		}
		else
		{
			debugf(TEXT("No ProviderClassName specified for ElementProviderType %i: %s"), ProviderTypeIndex, *ProviderType.ProviderTag.ToString());
		}
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
void UUIDataStore_DynamicResource::OnRegister( ULocalPlayer* PlayerOwner )
{
	Super::OnRegister(PlayerOwner);
	InitializeListElementProviders();
}

/* === UObject interface === */
/** Required since maps are not yet supported by script serialization */
void UUIDataStore_DynamicResource::AddReferencedObjects( TArray<UObject*>& ObjectArray )
{
	Super::AddReferencedObjects(ObjectArray);

	for ( TMultiMap<FName,UUIResourceCombinationProvider*>::TIterator It(ResourceProviders); It; ++It )
	{
		UUIResourceCombinationProvider* ResourceProvider = It.Value();
		if ( ResourceProvider != NULL )
		{
			AddReferencedObject(ObjectArray, ResourceProvider);
		}
	}
}

void UUIDataStore_DynamicResource::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if ( !Ar.IsPersistent() )
	{
		for ( TMultiMap<FName,UUIResourceCombinationProvider*>::TIterator It(ResourceProviders); It; ++It )
		{
			UUIResourceCombinationProvider*& ResourceProvider = It.Value();
			Ar << ResourceProvider;
		}
	}
}


/**
 * Called from ReloadConfig after the object has reloaded its configuration data.
 */
void UUIDataStore_DynamicResource::PostReloadConfig( UProperty* PropertyThatWasLoaded )
{
	Super::PostReloadConfig( PropertyThatWasLoaded );

	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		if ( PropertyThatWasLoaded == NULL || PropertyThatWasLoaded->GetFName() == TEXT("ResourceProviderDefinitions") )
		{
			// reload the ProviderClass for each ElementProviderType
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
UBOOL UUIDataStore_DynamicResource::GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags/*=0*/ ) const
{
	UBOOL bResult = Super::GetNativePropertyValues(out_PropertyValues, ExportFlags);

	INT Count=0, LongestProviderTag=0;

	TMap<FString,FString> PropertyValues;
	for ( INT ResourceIdx = 0; ResourceIdx < ResourceProviderDefinitions.Num(); ResourceIdx++ )
	{
		const FDynamicResourceProviderDefinition& Definition = ResourceProviderDefinitions(ResourceIdx);
		TArray<UUIResourceCombinationProvider*> Providers;
		ResourceProviders.MultiFind(Definition.ProviderTag, Providers);

		for ( INT ProviderIdx = 0; ProviderIdx < Providers.Num(); ProviderIdx++ )
		{
			UUIResourceCombinationProvider* Provider = Providers(ProviderIdx);
			FString PropertyName = *FString::Printf(TEXT("ResourceProviders[%i].%s[%i]"), ResourceIdx, *Definition.ProviderTag.ToString(), ProviderIdx);
			FString PropertyValue = Provider->GetName();

			LongestProviderTag = Max(LongestProviderTag, PropertyName.Len());
			PropertyValues.Set(*PropertyName, PropertyValue);
		}
	}

	for ( TMap<FString,FString>::TConstIterator It(PropertyValues); It; ++It )
	{
		const FString& ProviderTag = It.Key();
		const FString& ProviderName = It.Value();

		out_PropertyValues.Set(*ProviderTag, ProviderName.LeftPad(LongestProviderTag));
		bResult = TRUE;
	}

	return bResult || ResourceProviders.Num() > 0;
}

/* ==========================================================================================================
	UUIDynamicDataProvider
========================================================================================================== */

IMPLEMENT_COMPARE_POINTER(UProperty,UnUIDataStores_DynamicPropertyBinding,{ return appStricmp(*A->GetName(),*B->GetName()); });

/* ==========================================================================================================
	UUIDataStore_StringAliasMap
========================================================================================================== */

/**
 * Returns a pointer to the ULocalPlayer that this PlayerSettingsProvdier provider settings data for
 */
ULocalPlayer* UUIDataStore_StringAliasMap::GetPlayerOwner() const
{
	ULocalPlayer* Result = NULL;

	if ( GEngine != NULL && GEngine->GamePlayers.IsValidIndex(PlayerIndex) )
	{
		Result = GEngine->GamePlayers(PlayerIndex);
	}

	return Result;
}

/**
 * Called when this data store is added to the data store manager's list of active data stores.
 *
 * @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
 *							associated with a particular player; NULL if this is a global data store.
 */
void UUIDataStore_StringAliasMap::OnRegister( ULocalPlayer* PlayerOwner )
{
	if ( GEngine != NULL && PlayerOwner != NULL )
	{
		// set the PlayerIndex for this player
		PlayerIndex = GEngine->GamePlayers.FindItemIndex(PlayerOwner);
	}

	// add a field for each mapping in the array
	for ( INT MapIdx = 0; MapIdx < MenuInputMapArray.Num(); MapIdx++ )
	{
		FUIMenuInputMap &InputMap = MenuInputMapArray(MapIdx);
		TMap<FName, INT> *SetMap = MenuInputSets.Find(InputMap.Set);

		if(SetMap==NULL)
		{
			SetMap = &MenuInputSets.Set(InputMap.Set, TMap<FName, INT>());
		}

		if(SetMap)
		{
			SetMap->Set(InputMap.FieldName, MapIdx);
		}
		
	}

	Super::OnRegister(PlayerOwner);
}

/**
 * Attempts to find a mapping index given a field name.
 *
 * @param FieldName		Fieldname to search for.
 *
 * @return Returns the index of the mapping in the mapping array, otherwise INDEX_NONE if the mapping wasn't found.
 */
INT UUIDataStore_StringAliasMap::FindMappingWithFieldName( const FString& FieldName, const FString &SetName )
{
	INT Result = INDEX_NONE;

	// add a field for each mapping in the array
	TMap<FName, INT> *SetMap = MenuInputSets.Find(*SetName);

	if(SetMap)
	{
		INT *FieldIdx = SetMap->Find(*FieldName);

		if(FieldIdx)
		{
			Result = *FieldIdx;
		}
	}

	return Result;
}


/**
 * Set MappedString to be the localized string using the FieldName as a key
 * Returns the index into the mapped string array of where it was found.
 */
INT UUIDataStore_StringAliasMap::GetStringWithFieldName( const FString& FieldName, FString& MappedString )
{
	INT FieldIdx = FindMappingWithFieldName(FieldName);

	if(FieldIdx != INDEX_NONE)
	{
		MappedString = MenuInputMapArray(FieldIdx).MappedText;
	}

	return FieldIdx;
}

/* ==========================================================================================================
	UUIDataStore_OnlineGameSettings
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataStore_OnlineGameSettings);

/**
 * Loads and creates an instance of the registered settings class(es)
 */
void UUIDataStore_OnlineGameSettings::InitializeDataStore(void)
{
	UClass* SettingsClass = SettingsProviderClass;
	if ( SettingsClass == NULL || !SettingsClass->IsChildOf(UUIDataProvider_Settings::StaticClass()) )
	{
		debugf(NAME_Warning, TEXT("%s::InitializeDataStore: Invalid SettingsProviderClass specified.  Falling back to UIDataProvider_Settings."), *GetClass()->GetName());
		SettingsClass = UUIDataProvider_Settings::StaticClass();
	}

	// Create the objects for each registered setting and provider
	for (INT Index = 0; Index < GameSettingsCfgList.Num(); Index++)
	{
		FGameSettingsCfg& Cfg = GameSettingsCfgList(Index);
		// Create an instance of a the registered OnlineGameSettings class
		// and create a provider to handle the data for it
		Cfg.GameSettings = ConstructObject<UOnlineGameSettings>(Cfg.GameSettingsClass);
		if (Cfg.GameSettings != NULL)
		{
			// Create an instance of the data provider that will handle it
			Cfg.Provider = ConstructObject<UUIDataProvider_Settings>(SettingsClass);
			if (Cfg.Provider == NULL)
			{
				debugf(NAME_Error,TEXT("Failed to create UUIDataProvider_Settings instance for %s"),
					*Cfg.GameSettingsClass->GetName());
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Failed to create instance of class %s"),
				*Cfg.GameSettingsClass->GetName());
		}
	}
}

/* ==========================================================================================================
	UUIDataProvider_Settings
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_Settings);

/* ==========================================================================================================
	UUIDataProvider_SettingsArray
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_SettingsArray);

/**
 * Binds the new settings object and id to this provider.
 *
 * @param NewSettings the new object to bind
 * @param NewSettingsId the id of the settings array to expose
 *
 * @return TRUE if the call worked, FALSE otherwise
 */
UBOOL UUIDataProvider_SettingsArray::BindStringSetting(USettings* NewSettings,INT NewSettingsId)
{
	// Copy the values
	Settings = NewSettings;
	SettingsId = NewSettingsId;
	// And figure out the name of this setting for perf reasons
	SettingsName = Settings->GetStringSettingName(SettingsId);
	ColumnHeaderText = Settings->GetStringSettingColumnHeader(SettingsId);
	// Ditto for the various values
	Settings->GetStringSettingValueNames(SettingsId,Values);
	return SettingsName != NAME_None;
}

/**
 * Binds the property id as an array item. Requires that the property
 * has a mapping type of PVMT_PredefinedValues
 *
 * @param NewSettings the new object to bind
 * @param PropertyId the id of the property to expose as an array
 *
 * @return TRUE if the call worked, FALSE otherwise
 */
UBOOL UUIDataProvider_SettingsArray::BindPropertySetting(USettings* NewSettings,INT PropertyId)
{
	// Copy the values
	Settings = NewSettings;
	SettingsId = PropertyId;
	FSettingsPropertyPropertyMetaData* PropMeta = NewSettings->FindPropertyMetaData(PropertyId);
	if (PropMeta)
	{
		SettingsName = PropMeta->Name;
		ColumnHeaderText = PropMeta->ColumnHeaderText;
		// Iterate through the possible values adding them to our array of choices
		Values.Empty(PropMeta->PredefinedValues.Num());
		Values.AddZeroed(PropMeta->PredefinedValues.Num());
		for (INT Index = 0; Index < PropMeta->PredefinedValues.Num(); Index++)
		{
			const FString& StrVal = PropMeta->PredefinedValues(Index).ToString();
			Values(Index).Id = Index;
			Values(Index).Name = FName(*StrVal);
		}
	}
	return SettingsName != NAME_None;
}

/* ==========================================================================================================
	UUIDataProvider_OnlineFriends
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_OnlinePlayerDataBase);
IMPLEMENT_CLASS(UUIDataProvider_OnlineFriends);
IMPLEMENT_CLASS(UUIDataProvider_OnlinePartyChatList);

/* ==========================================================================================================
	UUIDataProvider_OnlineFriendMessages
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_OnlineFriendMessages);

/* ==========================================================================================================
	UUIDataProvider_PlayerAchievements
========================================================================================================== */
IMPLEMENT_CLASS(UUIDataProvider_PlayerAchievements);

static UScriptStruct* GetAchievementDetailsStruct()
{
	static UScriptStruct* AchievementsDetailsStruct = FindField<UScriptStruct>(UOnlineSubsystem::StaticClass(),TEXT("AchievementDetails"));
	checkSlow(AchievementsDetailsStruct);

	return AchievementsDetailsStruct;
}

/**
 * Returns the number of gamer points this profile has accumulated across all achievements
 *
 * @return	a number between 0 and the maximum number of gamer points allocated for each game (currently 1175), representing the total
 * gamer points earned from all achievements for this profile.
 */
INT UUIDataProvider_PlayerAchievements::GetTotalGamerScore() const
{
	INT Result = 0;
	INT MaxValue = 0;

	for ( INT Idx = 0; Idx < Achievements.Num(); Idx++ )
	{
		const FAchievementDetails& Achievement = Achievements(Idx);
		if ( Achievement.bWasAchievedOffline || Achievement.bWasAchievedOnline )
		{
			Result += Achievement.GamerPoints;
		}
		MaxValue += Achievement.GamerPoints;
	}

	return Min(MaxValue, Result);
}

/**
* Returns the number of gamer points that can be acquired in this game across all achievements
*
* @return	The maximum number of gamer points allocated for each game.
*/
INT UUIDataProvider_PlayerAchievements::GetMaxTotalGamerScore() const
{
	INT Result = 0;

	for ( INT Idx = 0; Idx < Achievements.Num(); Idx++ )
	{
		Result += Achievements(Idx).GamerPoints;
	}

	return Result;
}

/* ==========================================================================================================
	UUIDataStore_OnlinePlayerData
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataStore_OnlinePlayerData);

/**
 * Loads the game specific OnlineProfileSettings class
 */
void UUIDataStore_OnlinePlayerData::LoadDependentClasses(void)
{
	if (ProfileSettingsClassName.Len() > 0)
	{
		// Try to load the specified class
		ProfileSettingsClass = LoadClass<UOnlineProfileSettings>(NULL,*ProfileSettingsClassName,NULL,LOAD_None,NULL);
		if (ProfileSettingsClass == NULL)
		{
			debugf(NAME_Error,TEXT("Failed to load OnlineProfileSettings class %s"),*ProfileSettingsClassName);
		}
	}
	if (PlayerStorageClassName.Len() > 0)
	{
		// Try to load the specified class
		PlayerStorageClass = LoadClass<UOnlinePlayerStorage>(NULL,*PlayerStorageClassName,NULL,LOAD_None,NULL);
		if (PlayerStorageClass == NULL)
		{
			debugf(NAME_Error,TEXT("Failed to load OnlinePlayerStorage class %s"),*PlayerStorageClassName);
		}
	}
	if (FriendsProviderClassName.Len() > 0)
	{
		// Try to load the specified class
		FriendsProviderClass = LoadClass<UUIDataProvider_OnlineFriends>(NULL,*FriendsProviderClassName,NULL,LOAD_None,NULL);
	}
	if (FriendsProviderClass == NULL)
	{
		FriendsProviderClass = UUIDataProvider_OnlineFriends::StaticClass();
	}
	if (FriendMessagesProviderClassName.Len() > 0)
	{
		// Try to load the specified class
		FriendMessagesProviderClass = LoadClass<UUIDataProvider_OnlineFriendMessages>(NULL,*FriendMessagesProviderClassName,NULL,LOAD_None,NULL);
	}
	if (FriendMessagesProviderClass == NULL)
	{
		FriendMessagesProviderClass = UUIDataProvider_OnlineFriendMessages::StaticClass();
	}
	if (AchievementsProviderClassName.Len() > 0)
	{
		// Try to load the specified class
		AchievementsProviderClass = LoadClass<UUIDataProvider_PlayerAchievements>(NULL,*AchievementsProviderClassName,NULL,LOAD_None,NULL);
	}
	if (AchievementsProviderClass == NULL)
	{
		AchievementsProviderClass = UUIDataProvider_PlayerAchievements::StaticClass();
	}
	if (PartyChatProviderClassName.Len() > 0)
	{
		// Try to load the specified class
		PartyChatProviderClass = LoadClass<UUIDataProvider_OnlinePartyChatList>(NULL,*PartyChatProviderClassName,NULL,LOAD_None,NULL);
	}
	if (PartyChatProviderClass == NULL)
	{
		PartyChatProviderClass = UUIDataProvider_OnlinePartyChatList::StaticClass();
	}
	if (ProfileProviderClassName.Len() > 0)
	{
		// Try to load the specified class
		ProfileProviderClass = LoadClass<UUIDataProvider_OnlineProfileSettings>(NULL,*ProfileProviderClassName,NULL,LOAD_None,NULL);
	}
	if (ProfileProviderClass == NULL)
	{
		ProfileProviderClass = UUIDataProvider_OnlineProfileSettings::StaticClass();
	}
	if (StorageProviderClassName.Len() > 0)
	{
		// Try to load the specified class
		StorageProviderClass = LoadClass<UUIDataProvider_OnlinePlayerStorage>(NULL,*StorageProviderClassName,NULL,LOAD_None,NULL);
	}
	if (StorageProviderClass == NULL)
	{
		StorageProviderClass = UUIDataProvider_OnlinePlayerStorage::StaticClass();
	}
}

/**
 * Creates the data providers exposed by this data store
 */
void UUIDataStore_OnlinePlayerData::InitializeDataStore(void)
{
#if WIIU
	LoadDependentClasses();
#endif
	if (FriendsProvider == NULL)
	{
		FriendsProvider = ConstructObject<UUIDataProvider_OnlineFriends>(FriendsProviderClass);
	}
	if (ProfileProvider == NULL)
	{
		ProfileProvider = ConstructObject<UUIDataProvider_OnlineProfileSettings>(ProfileProviderClass);
	}
	if (StorageProvider == NULL)
	{
		StorageProvider = ConstructObject<UUIDataProvider_OnlinePlayerStorage>(StorageProviderClass);
	}
	if (FriendMessagesProvider == NULL)
	{
		FriendMessagesProvider = ConstructObject<UUIDataProvider_OnlineFriendMessages>(FriendMessagesProviderClass);
	}
	if (AchievementsProvider == NULL)
	{
		AchievementsProvider = ConstructObject<UUIDataProvider_PlayerAchievements>(AchievementsProviderClass);
	}
	if (PartyChatProvider == NULL)
	{
		PartyChatProvider = ConstructObject<UUIDataProvider_OnlinePartyChatList>(PartyChatProviderClass);
	}
	check(FriendsProvider && FriendMessagesProvider && AchievementsProvider && PartyChatProvider);
}

/**
 * Forwards the calls to the data providers so they can do their start up
 *
 * @param Player the player that will be associated with this DataStore
 */
void UUIDataStore_OnlinePlayerData::OnRegister(ULocalPlayer* Player)
{
	if (FriendsProvider)
	{
		FriendsProvider->eventOnRegister(Player);
	}
	if (FriendMessagesProvider)
	{
		FriendMessagesProvider->eventOnRegister(Player);
	}
	if (PartyChatProvider)
	{
		PartyChatProvider->eventOnRegister(Player);
	}
	if (ProfileProvider && ProfileSettingsClass)
	{
		UOnlineProfileSettings* Profile = NULL;
		if (Player != NULL)
		{
			// If a cached profile exists, bind to that profile instead
			eventGetCachedPlayerProfile(Player->ControllerId);
		}
		// Create one if we don't have a profile
		if (Profile == NULL)
		{
			Profile = ConstructObject<UOnlineProfileSettings>(ProfileSettingsClass);
		}
		ProfileProvider->Profile = Profile;
		// Now kick off the read for it
		ProfileProvider->eventOnRegister(Player);
	}
	if (StorageProvider && PlayerStorageClass)
	{
		UOnlinePlayerStorage* Storage = NULL;
		if (Player != NULL)
		{
			// If a cached storage exists, bind to that storage instead
			eventGetCachedPlayerStorage(Player->ControllerId);
		}
		if (Storage == NULL)
		{
			Storage = ConstructObject<UOnlinePlayerStorage>(PlayerStorageClass);
		}
		StorageProvider->Profile = Storage;
		// Now kick off the read for it
		StorageProvider->eventOnRegister(Player);
	}
	if (AchievementsProvider != NULL)
	{
		AchievementsProvider->eventOnRegister(Player);
	}
	// Our local events
	eventOnRegister(Player);
}

/**
 * Tells all of the child providers to clear their player data
 *
 * @param Player ignored
 */
void UUIDataStore_OnlinePlayerData::OnUnregister(ULocalPlayer*)
{
	if (FriendsProvider)
	{
		FriendsProvider->eventOnUnregister();
	}
	if (FriendMessagesProvider)
	{
		FriendMessagesProvider->eventOnUnregister();
	}
	if (ProfileProvider)
	{
		ProfileProvider->eventOnUnregister();
	}
	if (StorageProvider)
	{
		StorageProvider->eventOnUnregister();
	}
	if ( AchievementsProvider != NULL )
	{
		AchievementsProvider->eventOnUnregister();
	}
	if (PartyChatProvider)
	{
		PartyChatProvider->eventOnUnregister();
	}
	// Our local events
	eventOnUnregister();
}

/* ==========================================================================================================
	UUIDataProvider_OnlineProfileSettings
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_OnlineProfileSettings);

/* ==========================================================================================================
UUIDataProvider_OnlinePlayerStorage
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_OnlinePlayerStorage);

/* ==========================================================================================================
UUIDataProvider_OnlinePlayerStorageArray
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataProvider_OnlinePlayerStorageArray);

/* ==========================================================================================================
	UUIDataStore_OnlineGameSearch
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataStore_OnlineGameSearch);

/**
 * Loads and creates an instance of the registered provider object for the
 * registered OnlineGameSearch class
 */
void UUIDataStore_OnlineGameSearch::InitializeDataStore(void)
{
	// Iterate through the registered searches creating their data
	for (INT Index = 0; Index < GameSearchCfgList.Num(); Index++)
	{
		FGameSearchCfg& Cfg = GameSearchCfgList(Index);
		// Create an instance of a the registered OnlineGameSearch class
		Cfg.Search = ConstructObject<UOnlineGameSearch>(Cfg.GameSearchClass);
		if (Cfg.Search != NULL)
		{
			// Create an instance of the data provider that will handle it
			Cfg.DesiredSettingsProvider = ConstructObject<UUIDataProvider_Settings>(UUIDataProvider_Settings::StaticClass());
			if (Cfg.DesiredSettingsProvider == NULL)
			{
				debugf(NAME_Error,TEXT("Failed to create provider context instance for %s"),
					*Cfg.GameSearchClass->GetName());
			}
		}
		else
		{
			debugf(NAME_Error,TEXT("Failed to create instance of class %s"),
				*Cfg.GameSearchClass->GetName());
		}
	}
	// Do script initialization
	eventInit();
}

/* ==========================================================================================================
	UUIDataStore_OnlineStats
========================================================================================================== */

IMPLEMENT_CLASS(UUIDataStore_OnlineStats);

// UIDataStore interface

/**
 * Loads and creates an instance of the registered stats read object
 */
void UUIDataStore_OnlineStats::InitializeDataStore(void)
{
	// Create each of the stats classes that are registered
	for (INT Index = 0; Index < StatsReadClasses.Num(); Index++)
	{
		UClass* Class = StatsReadClasses(Index);
		if (Class)
		{
			StatsRead = ConstructObject<UOnlineStatsRead>(Class);
			if (StatsRead != NULL)
			{
				StatsReadObjects.AddItem(StatsRead);
#if !CONSOLE
				// Create one search result in the editor
				if (GIsEditor && !GIsGame && StatsRead->Rows.Num() == 0)
				{
					StatsRead->Rows.AddZeroed();
					// Set up the list of columns that are expected back
					for (INT Index2 = 0; Index2 < StatsRead->ColumnIds.Num(); Index2++)
					{
						StatsRead->Rows(0).Columns.AddZeroed();
						StatsRead->Rows(0).Columns(Index2).ColumnNo = StatsRead->ColumnIds(Index2);
						StatsRead->Rows(0).Columns(Index2).StatValue.SetData((INT)100);
					}
				}
#endif
			}
			else
			{
				debugf(NAME_Error,TEXT("Failed to create instance of class %s"),*Class->GetName());
			}
		}
		else
		{
			//Placeholder for None entry
			StatsReadObjects.AddItem(NULL);
		}

	}
	// Now kick off the script initialization
	eventInit();
}

/** Sort class that compares stats rows by their rank */
struct FStatRowSorter
{
	static inline INT Compare(const FOnlineStatsRow& A,const FOnlineStatsRow& B)
	{
		INT AVal = 0;
		// If the rank is not an integer, it's not set
		if (A.Rank.Type == SDT_Int32)
		{
			A.Rank.GetData(AVal);
		}
		else
		{
			// Not set, so assume max
			AVal = MAXINT;
		}
		INT BVal = 0;
		// If the rank is not an integer, it's not set
		if (B.Rank.Type == SDT_Int32)
		{
			B.Rank.GetData(BVal);
		}
		else
		{
			// Not set, so assume max
			BVal = MAXINT;
		}
		return AVal - BVal;
	}
};

/**
 * Sorts the returned results by their rank (lowest to highest)
 */
void UUIDataStore_OnlineStats::SortResultsByRank(UOnlineStatsRead* StatsToSort)
{
	if (StatsToSort != NULL && StatsToSort->Rows.Num() > 0)
	{
		// Use the stats row comparator to sort the results by rank
		::Sort<FOnlineStatsRow,FStatRowSorter>(&StatsToSort->Rows(0),StatsToSort->Rows.Num());
	}
}

/* ==========================================================================================================
	UUIDataStore_MenuItems
========================================================================================================== */
IMPLEMENT_CLASS(UUIDataStore_MenuItems);

/**
* Called when this data store is added to the data store manager's list of active data stores.
*
* Initializes the ListElementProviders map
*
* @param	PlayerOwner		the player that will be associated with this DataStore.  Only relevant if this data store is
*							associated with a particular player; NULL if this is a global data store.
*/
void UUIDataStore_MenuItems::OnRegister( ULocalPlayer* PlayerOwner )
{
	Super::OnRegister(PlayerOwner);

	// Initialize all of the option providers, go backwards because of the way MultiMap appends items.
	TArray<UUIResourceDataProvider*> Providers;
	ListElementProviders.MultiFind(TEXT("OptionCategory"), Providers);

	for ( INT ProviderIndex = Providers.Num()-1; ProviderIndex >= 0; ProviderIndex-- )
	{
		UUIDataProvider_MenuItem* DataProvider = Cast<UUIDataProvider_MenuItem>(Providers(ProviderIndex));
		if(DataProvider)
		{
			for (INT OptionIndex=0;OptionIndex<DataProvider->OptionSet.Num();OptionIndex++)
			{
				OptionProviders.Add(DataProvider->OptionSet(OptionIndex), DataProvider);
			}
		}
	}
}


/* ==========================================================================================================
	UUIDataProvider_MenuItem
========================================================================================================== */
IMPLEMENT_CLASS(UUIDataProvider_MenuItem);
