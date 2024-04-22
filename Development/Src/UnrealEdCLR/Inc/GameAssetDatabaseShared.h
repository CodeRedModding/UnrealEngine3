/*===================================================================================
	GameAssetDatabaseShared.cpp: System for globally browsing and tagging game assets
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
===================================================================================*/

#ifndef __GameAssetDatabaseShared_h__
#define __GameAssetDatabaseShared_h__

#ifdef _MSC_VER
	#pragma once
#endif


// NOTE: This file is included as MANAGED for CLR .cpp files, but UNMANAGED for the rest of the app

#include "InteropShared.h"


/**
 * ETagQueryOptions
 */
namespace ETagQueryOptions
{
	enum Type
	{
		/** Invalid/unassigned options */
		Invalid = 0,

		/** Queries system tags only */
		SystemTagsOnly,

		/** Queries user tags only (non-system tags) */
		UserTagsOnly,

		/** Only queries collection tags (both private and shared) */
		CollectionsOnly,

		/** Queries all (system tags and user tags) */
		AllTags,
	};
}


namespace GADDefsNative
{
	/** Character used to separate the system tag type prefix from the actual tag name */
	static const TCHAR SystemTagPreDelimiter = '[';
	static const TCHAR SystemTagPostDelimiter = ']';
}



#ifdef __cplusplus_cli

// ... MANAGED ONLY definitions go here ...
using namespace System;
using namespace System::Collections;
using namespace System::Collections::Generic;
using namespace System::Data::SqlClient;
using namespace System::Runtime::InteropServices;


/**
 * ESystemTagType
 */
enum class ESystemTagType
{
	// NOTE: If you modify this, remember to update SystemTagTypeNames below.

	/** Invalid/unassigned tag type.  This may be set for tags that are not system tags at all. */
	Invalid = 0,

	/** Specifies an Unreal object type (UClass).  Note that Archetypes get an additional tag. */
	ObjectType,
	
	/** The outermost package (not group!) that an object is contained within */
	OutermostPackage,

	/** A shared collection name */
	SharedCollection,

	/** Private collection name */
	PrivateCollection,

	/** Local, unsaved collection name */
	LocalCollection,

	/** Unverified asset.  This is an asset that hasn't yet been seen by the checkpoint process.  This
	    tag is used on local clients only and should never end up in the journal or checkpoint file. */
	Unverified,

	/** "Ghost" system tag.  This flags an object that doesn't actually exist on disk yet.  For example,
	    assets that have been tagged (but not checked in), or assets that have been deleted (but not expired) */
	Ghost,

	/** Archetype object */
	Archetype,

	/** Date that the asset was first added.  We don't bother storing time (to reduce tag clutter) */
	DateAdded,

	/** The asset is Quarantined; it might be pending deletion, needs to be optimized or consolidated. */
	Quarantined
};




/**
 * EJournalEntryType
 */
enum class EJournalEntryType
{
	/** Invalid/unassigned journal entry type */
	Invalid = 0,

	/** Add a new tag */
	AddTag = 1,

	/** Remove a tag */
	RemoveTag = 2,

	/** Create a new collection */
	CreateTag = 3,

	/** Destroy an existing collection */
	DestroyTag = 4,

};




/**
 * MGameAssetJournalEntry
 */
ref struct MGameAssetJournalEntry
{
	/** Ordered database index number */
	int DatabaseIndex;

	/** True if tag/asset strings are actually valid. */
	bool IsValidEntry;

	/** True if this is an "offline" journal entry.  That is, it originated from a local journal file on disk. */
	bool IsOfflineEntry;

	/** Time stamp */
	DateTime^ TimeStamp;

	/** Type of entry */
	EJournalEntryType Type;

	/** Tag to add or remove from this asset */
	String^ Tag;

	/** The asset's full name */
	String^ AssetFullName;

	/** User name who submitted the journal entry */
	String^ UserName;

	/** Branch name that this asset is located in */
	String^ BranchName;
	
	/** Game name that this asset is located in */
	String^ GameName;


	/** Returns a string representation of a journal entry. This method is intended for debug only. */
	virtual String^ ToString() override
	{
		System::Text::StringBuilder^ DebugStringBuilder = gcnew System::Text::StringBuilder();
		DebugStringBuilder->Append(DatabaseIndex);
		DebugStringBuilder->Append("\t");
		DebugStringBuilder->Append(TimeStamp->ToString());
		DebugStringBuilder->Append("\t");
		DebugStringBuilder->Append(UserName);
		DebugStringBuilder->Append("\t");
		DebugStringBuilder->Append(Type);
		DebugStringBuilder->Append("\t");
		DebugStringBuilder->Append(Tag);
		DebugStringBuilder->Append("\t");
		DebugStringBuilder->Append(AssetFullName);
		return DebugStringBuilder->ToString();
	}

	/** Make an entry that describes the tagging of an asset */
	static MGameAssetJournalEntry^ MakeAddTagToAssetEntry( String^ InAssetFullName, String^ InTag )
	{
		MGameAssetJournalEntry^ JournalEntry = gcnew MGameAssetJournalEntry();
		JournalEntry->Type = EJournalEntryType::AddTag;
		JournalEntry->AssetFullName = InAssetFullName;
		JournalEntry->Tag = InTag;
		return JournalEntry;
	}

	/** Make an entry that describes the untagging of an asset */
	static MGameAssetJournalEntry^ MakeRemoveTagFromAssetEntry( String^ InAssetFullName, String^ InTag )
	{
		MGameAssetJournalEntry^ JournalEntry = gcnew MGameAssetJournalEntry();
		JournalEntry->Type = EJournalEntryType::RemoveTag;
		JournalEntry->AssetFullName = InAssetFullName;
		JournalEntry->Tag = InTag;
		return JournalEntry;
	}

	/** Make an entry that describes creation of a collection */
	static MGameAssetJournalEntry^ MakeCreateTagEntry( String^ InTagName )
	{
		MGameAssetJournalEntry^ Entry = gcnew MGameAssetJournalEntry();
		Entry->Type = EJournalEntryType::CreateTag;
		Entry->Tag = InTagName;
		return Entry;
	}

	/** Make an entry that describes destruction of a collection */
	static MGameAssetJournalEntry^ MakeDestroyTagEntry( String^ InTagName )
	{
		MGameAssetJournalEntry^ Entry = gcnew MGameAssetJournalEntry();
		Entry->Type = EJournalEntryType::DestroyTag;
		Entry->Tag = InTagName;
		return Entry;
	}


	/** Delegate for sorting journal entries  */
	static int JournalEntrySortDelegate( MGameAssetJournalEntry^ X, MGameAssetJournalEntry^ Y )
	{
		// Sort by time stamp
		INT64 Diff = ( X->TimeStamp->Ticks - Y->TimeStamp->Ticks );
		if( Diff == 0 )
		{
			// Sort by database index
			Diff = X->DatabaseIndex - Y->DatabaseIndex;
		}

		// Diff likely overflows int, so we must clamp it
		if (Diff < 0)
		{
			return -1;
		}
		else if (Diff > 0)
		{
			return 1;
		}
		else
		{
			return 0;
		}
	}

};



/** MGameAssetJournalEntryList */
typedef List< MGameAssetJournalEntry^ > MGameAssetJournalEntryList;



/** GameAssetDatabase design constraints */
ref struct GADDefs
{
	/** Journal entry ver.2: Added support for asset 'full names' */
	initonly static int JournalEntryVersionNumber_AssetFullNames = 2;

	/** Current journal entry version number */
	initonly static int JournalEntryVersionNumber = JournalEntryVersionNumber_AssetFullNames;


	/** Checkpoint file ver.2: Added persistent collections */
	initonly static int CheckpointFileVersionNumber_PersistentCollections = 2;

	/** Checkpoint file ver.3: Added support for asset 'full names' */
	initonly static int CheckpointFileVersionNumber_AssetFullNames = 3;

	/** Current checkpoint file version number */
	initonly static int CheckpointFileVersionNumber = CheckpointFileVersionNumber_AssetFullNames;


	/** Current journal file version number */
	initonly static int JournalFileVersionNumber = 1;


	/** Name of each type */
	initonly static cli::array< String^ >^ SystemTagTypeNames =
		{ "<Invalid>",
		  "ObjectType",
		  "Package",
		  "Collection",
		  "PrivateCollection",
		  "LocalCollection",
		  "Unverified",
		  "Ghost",
		  "Archetype",
		  "DateAdded",
		  "Quarantined"
		};

	/** Character used to separate a private collection tag from the user's name it's associated with */
	initonly static TCHAR PrivateCollectionUserDelimiter = '@';

	/** Name of each journal entry type */
	// NOTE: Type name should be 10 characters or less (for SQL performance)
	initonly static cli::array< String^ >^ JournalEntryTypeNames = { "Invalid", "Add", "Remove", "CreateTag", "DestroyTag" };

	/** Character used to separate text elements in a journal entry field */
	initonly static TCHAR JournalEntryFieldDelimiter = '|';

	/** Checkpoint file header bytes ('G'ame 'A'sset 'D'atabase 'C'heckpoint) */
	initonly static cli::array< unsigned char >^ CheckpointFileHeaderChars = { 'G', 'A', 'D', 'C' };

	/** How many days before already-checkpointed journal entries will be deleted from the SQL server.  This
		means that a user must sync up to a later checkpoint file within this span to have correct data */
	initonly static int DeleteJournalEntriesOlderThanDays = 3;

	/** How many days before we'll purge a non-existent asset from the database.  These are usually assets that
	    were deleted, but could also be an asset that was tagged by a user but never submitted to source control */
	initonly static int DeleteGhostAssetsOlderThanDays = 7;
	
	/** How many days old an asset tag has to be before a "PurgeAllOld" command will delete it */
	initonly static int PurgeJournalEntriesOlderThanDays = 14;

	/** Maximum supported length of a tag name */
	initonly static int MaxTagLength = NAME_SIZE;

};


typedef Dictionary< String^, int > RefCountedStringDictionary;



/**
 * MGameAssetJournalBase
 */
ref class MGameAssetJournalBase abstract
{

public:

	/** Loads journal entries from a file or server */
	virtual bool QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame ) = 0;

	/** Writes journal entries to a file or server */
	virtual bool SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries ) = 0;


public:

	/** Writes a single journal entry to a file or server */
	bool SendJournalEntry( MGameAssetJournalEntry^ JournalEntry );

	/** Creates a text string that represents the specified journal entry */
	static bool CreateStringFromJournalEntry( MGameAssetJournalEntry^ JournalEntry, [Out] String^% OutString );

	/** Creates a journal entry from the specified journal entry string */
	static bool CreateJournalEntryFromString( String^ Text,  List<String^>^ AllClassNames, [Out] MGameAssetJournalEntry^% OutJournalEntry, bool bFilterBranchAndGame );


protected:

	/** Determines the working name of the current development branch */
	static String^ QueryBranchName();
	
	/** Use this too track info when a branch hasn't been specified */
	initonly static String^ UnknownBranchName = TEXT("UNKNOWN_BRANCH");
private:

};




/**
 * MGameAssetJournalClient
 */
ref class MGameAssetJournalClient
	: public MGameAssetJournalBase
{

public:

	/** Constructor */
	MGameAssetJournalClient();

	/** Destructor */
	~MGameAssetJournalClient();


public:

	/** Loads journal entries from the server */
	virtual bool QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame ) override;	

	/** Bulk uploads journal entries to the server */
	virtual bool SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries ) override;

	/** Deletes journal entries with the specified database indices from the server */
	bool DeleteJournalEntries( List< int >^ DatabaseIndices );

	/** Connect to the database server */
	bool ConnectToServer();

	/** Disconnects from the database server */
	bool DisconnectFromServer();


protected:

	/** Loads journal entries from the server */
	bool _QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame );	

	/** Bulk uploads journal entries to the server */
	bool _SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries );

	/** Deletes journal entries with the specified database indices from the server */
	bool _DeleteJournalEntries( List< int >^ DatabaseIndices );


private:

	/** Connection to SQL database */
	auto_handle< SqlConnection > MySqlConnection;	

};



/**
 * MGameAssetJournalFile
 */
ref class MGameAssetJournalFile
	: public MGameAssetJournalBase
{

public:

	/** Constructor */
	MGameAssetJournalFile();

	/** Destructor */
	~MGameAssetJournalFile();


public:

	/** Loads journal entries from the journal file */
	virtual bool QueryJournalEntries( [Out] MGameAssetJournalEntryList^% OutJournalEntries, bool bFilterBranchAndGame ) override;

	/** Stores multiple journal entries in the journal file */
	virtual bool SendJournalEntries( List<MGameAssetJournalEntry^>^ JournalEntries ) override;


public:

	/** Deletes the journal file from disk */
	bool DeleteJournalFile();


private:


};



#else // #ifdef __cplusplus_cli


// ... NATIVE ONLY definitions go here ...


#endif // #else





#ifdef __cplusplus_cli
#pragma unmanaged
#endif


/** TSetMap; similar to TMultiMap, but never allows duplicate values for a key and has O(1) lookups/removes */
template< typename KeyType, typename ValueType >
class TSetMap
	: private TMap< KeyType, TSet< ValueType > >	// We hide TMap so we can provide an appropriate API
{

public:

	typedef TMap< KeyType, ValueType > Super;
	typedef typename Super::KeyInitType KeyInitType;
	typedef typename Super::ValueInitType ValueInitType;
	typedef TSet< ValueType > ValueSetType;
	typedef TArray< ValueType > ValueArrayType;


	// @todo: Expose iterators!



  	/** Default constructor */
	TSetMap()
	{
	}


	/** Copy constructor */
	TSetMap( const TSetMap& InCopy )
		: Super( InCopy )
	{
	}


	/** Assignment operator */
	TSetMap& operator=(const TSetMap& Other )
	{
		Super::operator=( Other );
		return *this;
	}


	/** Returns the number of keys. */
	INT NumKeys() const
	{
		return TMap::Num();
	}


	/** Clears the entire map. */
	void Empty()
	{
		TMap::Empty();
	}


	/** Maps a unique value to a key.  O(1) */
	void Add( KeyInitType InKey, ValueInitType InValue )
	{
		ValueSetType* FoundValueSet = TMap::Find( InKey );
		if( FoundValueSet != NULL )
		{
			FoundValueSet->Add( InValue );
		}
		else
		{
			ValueSetType& NewValueSet = TMap::Set( InKey, ValueSetType() );
			NewValueSet.Add( InValue );
		}
	}


	/** Removes the mapping of a value to a key, and removes the key if no more values are mapped to it.  O(1) */
	void Remove( KeyInitType InKey, ValueInitType InValue )
	{
		ValueSetType* FoundValueSet = TMap::Find( InKey );
		if( FoundValueSet != NULL )
		{
			FoundValueSet->RemoveKey( InValue );

			// If the value array is now empty, go ahead and remove the key, too!
			if( FoundValueSet->Num() == 0 )
			{
				TMap::Remove( InKey );
			}
		}
	}



	/** Removes a key and values associated with it.  O(1) */
	void RemoveKey( KeyInitType InKey )
	{
		TMap::Remove( InKey );
	}


	/** Creates a copy of a list of keys in this table.  O(N) copy */
	void GetKeys( TLookupMap< KeyType >& OutKeys ) const
	{
		const INT NumKeys = TMap::GetKeys( OutKeys );
	}


	/** Returns true if the specified key was found.  O(1) */
	UBOOL ContainsKey( KeyInitType InKey ) const
	{
		return ( TMap::Find( InKey ) != NULL );
	}


	/** Returns true if the specified value is mapped to the specified key.  O(1) */
	UBOOL Contains( KeyInitType InKey, ValueInitType Value ) const
	{
		const ValueSetType* FoundValueSet = TMap::Find( InKey );
		if( FoundValueSet != NULL )
		{
			return FoundValueSet->Contains( Value );
		}

		return FALSE;
	}


	/** Returns a list of values that are mapped to the specified key.  O(1) search, O(N) copy */
	void FindValuesForKey( KeyInitType InKey, ValueArrayType& OutFoundValues ) const
	{
		OutFoundValues.Reset();

		const ValueSetType* FoundValueSet = TMap::Find( InKey );
		if( FoundValueSet != NULL )
		{
			for( ValueSetType::TConstIterator ValueIt( *FoundValueSet ); ValueIt; ++ValueIt )
			{
				OutFoundValues.AddItem( *ValueIt );
			}
		}
	}	

	
	/** Returns a set of values that are mapped to the specified key.  O(1) search, O(N) copy */
	void FindValuesForKey( KeyInitType InKey, ValueSetType& OutFoundValues ) const
	{
		const ValueSetType* FoundValueSet = TMap::Find( InKey );
		if( FoundValueSet != NULL )
		{
			OutFoundValues = *FoundValueSet;
		}
		else
		{
			OutFoundValues = ValueSetType();
		}
	}	


	/** Returns the number of values associated with the key */
	INT NumValuesForKey( KeyInitType InKey ) const
	{
		INT NumValues = 0;

		const ValueSetType* FoundValueSet = TMap::Find( InKey );
		if( FoundValueSet != NULL )
		{
			NumValues += FoundValueSet->Num();
		}

		return NumValues;
	}	
};

#ifdef __cplusplus_cli
#pragma managed
#endif



/**
 * Startup config for Game Asset Database
 */
struct FGameAssetDatabaseStartupConfig
{
	/** True if we're running as a commandlet */
	UBOOL bIsCommandlet;

	/** True if we should retain the existing checkpoint data where appropriate */
	UBOOL bShouldLoadCheckpointFile;

	/** True if we should load journal data from the SQL server or journal file */
	UBOOL bShouldLoadJournalEntries;

	/** True if we should only load journal entries that match my user name */
	UBOOL bOnlyLoadMyJournalEntries;

	/** True if we should update the checkpoint file */
	UBOOL bShouldCheckpoint;

	/** Checkpoint option: True we're allowed to delete entries from the SQL server */
	UBOOL bAllowServerDeletesAfterCheckpoint;

	/** Checkpoint option: True if we should purge assets from the database that don't exist on disk */
	UBOOL bPurgeNonExistentAssets;

	/** True if we should verify the database state */
	UBOOL bShouldVerifyIntegrity;

	/** True if we should try to automatically repair the local database if there are any problems */
	UBOOL bShouldRepairIfNeeded;

	/** Should we dump the contents of the game asset database to console */
	UBOOL bShouldDumpDatabase;

	/** Should private collections be removed? */
	UBOOL bDeletePrivateCollections;

	/** Should shared collections that a prefixed with "UDK" be removed? */
	UBOOL bDeleteNonUDKCollections;

	/** Should we show all old content from ALL branches and games */
	UBOOL bShowAllOldContent;
	
	/** Should we delete all old content from ALL branches and games */
	UBOOL bPurgeAllOldContent;

	FGameAssetDatabaseStartupConfig()
		: bIsCommandlet( FALSE ),
		  bShouldLoadCheckpointFile( TRUE ),
		  bShouldLoadJournalEntries( TRUE ),
		  bOnlyLoadMyJournalEntries( TRUE ),
		  bShouldCheckpoint( FALSE ),
		  bAllowServerDeletesAfterCheckpoint( TRUE ),
		  bPurgeNonExistentAssets( FALSE ),
		  bShouldVerifyIntegrity( FALSE ),
		  bShouldRepairIfNeeded( FALSE ),
		  bShouldDumpDatabase(FALSE),
		  bDeletePrivateCollections(FALSE),
		  bDeleteNonUDKCollections(FALSE),
		  bShowAllOldContent(FALSE),
		  bPurgeAllOldContent(FALSE)
	{
	}
};



/**
 * FGameAssetDatabase
 */
class FGameAssetDatabase
{

public:


	/**
	 * Static: Allocates and initializes the game asset database
	 *
	 * @param	InConfig					Startup configuration
	 * @param	OutInitErrorMessageText		A localized string containing warnings or errors during initialization
	 */
	static void Init( const FGameAssetDatabaseStartupConfig& InConfig, FString& OutInitErrorMessageText );


	/**
	 * Static: Shuts down and destroys the game asset database
	 */
	static void Destroy();


	/**
	 * Static: Checks if the game asset database singleton has been allocated and initialized
	 *
	 * @return	TRUE if the game asset database is allocated
	 */
	static UBOOL IsInitialized()
	{
		return ( GameAssetDatabaseSingleton != NULL );
	}


	/**
	 * Static: Returns the global instance of the game asset database object.  Must only be called 
	 * after the database is initialized.  The IsInitialized() function can tell you whether it's
	 * safe to query the singleton.
	 *
	 * @return	Singleton instance
	 */
	static FGameAssetDatabase& Get()
	{
		check( GameAssetDatabaseSingleton != NULL );
		return *GameAssetDatabaseSingleton;
	}


	/** Static: Checks to see if the journal file should prompt for checking for unverified assets*/
	static void CheckJournalAlarm(void);

#ifdef __cplusplus_cli




	/**
	 * Queries all tags
	 *
	 * @param	OutTags		List of all tags
	 * @param	InOptions	Types of tags to query
	 */
	void QueryAllTags( [Out] List< String^ >^% OutTags, ETagQueryOptions::Type InOptions ) const;



	/**
	 * Queries all asset full names (warning: this can potentially be a LOT of data!)
	 *
	 * @param	OutAssetFullNames		List of all asset full names
	 */
	void QueryAllAssets( [Out] List< String^ >^% OutAssetFullNames ) const;



	/**
	* Associates a tag with a group of assets
	*
	* @param	InAssetFullNames	Full names of the assets to tag
	* @param	InTag				Tag to assign
	*
	* @return	True if successful
	*/
	bool AddTagToAssets( Generic::ICollection<String^>^ InAssetFullNames, String^ InTag );



	/**
	 * Removes a tag from an asset
	 *
	 * @param	InAssetFullName	Full name of the asset
	 * @param	InTag			Tag to remove
	 *
	 * @return	True if successful
	 */
	bool RemoveTagFromAssets( Generic::ICollection<String^>^ InAssetFullName, String^ InTag );



	/**
	 * Finds the tags associated with the specified asset
	 *
	 * @param	InAssetFullName	Full name of the asset
	 * @param	InOptions		Types of tags to query
	 * @param	OutTags			[Out] List of tags found
	 *
	 * @return	True unless the specified asset was not known to the database
	 */
	void QueryTagsForAsset( String^ InAssetFullName, ETagQueryOptions::Type InOptions, [Out] List< String^ >^% OutTags ) const;



	/**
	 * Finds the tags associated with the specified asset
	 *
	 * @param	InAssetFullNameFName	Full name of the asset
	 * @param	InOptions		Types of tags to query
	 * @param	OutTags			[Out] List of tags found
	 */
	void QueryTagsForAsset( FName InAssetFullNameFName, ETagQueryOptions::Type InOptions, [Out] List< String^ >^% OutTags ) const;


	/**
	 * Finds all of the assets with the specified tag
	 *
	 * @param	InTag			Tag
	 * @param	OutAssetFullNames	[Out] List of asset full names found
	 */
	void QueryAssetsWithTag( String^ InTag, [Out] List< String^ >^% OutAssetFullNames ) const;



	/**
	 * Finds all of the assets with all of the specified set of tags, and any of another set of tags
	 *
	 * @param	InAllTags			An asset must have all of these tags assigned to be returned
	 * @param	InAnyTags			An asset may have any of these tags to be returned
	 * @param	OutAssetFullNames	[Out] List of asset full names found
	 */
	void QueryAssetsWithTags( Generic::ICollection< String^ >^ InAllTags, Generic::ICollection< String^ >^ InAnyTags, [Out] List< String^ >^% OutAssetFullNames ) const;


	
	/**
	 * Finds all of the assets with all of the specified tags
	 *
	 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with all of these tags.
	 * @param	OutAssetFullNames	[Out] List of assets found
	 */
	void QueryAssetsWithAllTags( List< String^ >^ InTags, [Out] List< String^ >^% OutAssetFullNames ) const;



	/**
	 * Finds all of the assets with any of the specified tags
	 *
	 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with any of these tags.
	 * @param	OutAssetFullNames	[Out] List of assets found
	 */
	void QueryAssetsWithAnyTags( Generic::ICollection< String^ >^ InTags, [Out] List< String^ >^% OutAssetFullNames ) const;

	/**
	 * Create a new (persistent) tag.
	 *
	 * @param InTag   The tag to create.
	 */
	bool CreateTag( System::String^ InTag );

	/**
	 * Destroy a tag. Also untags all assets tagged with this tag.
	 *
	 * @param InTag   The tag to destroy.
	 */
	bool DestroyTag( System::String^ InTag );

	/**
	 * Copies (or renames/moves) a tag
	 * 
	 * @param InCurrentTagName The tag to rename
	 * @param InNewTagName The new tag name
	 * @param bInMove True if the old tag should be destroyed after copying it
	 *
	 * @return TRUE is succeeded, FALSE if failed.
	 */
	bool CopyTag( System::String^ InCurrentTagName, System::String^ InNewTagName, bool bInMove );

	/**
	 * Create a new collection into which assets can be added.
	 * The collection name must be unique.
	 *
	 * @param InCollectionName Name of collection to create 
	 * @param InType Type of collection to create
	 * 
	 * @return True if collection was created successfully
	 */
	bool CreateCollection( System::String^ InCollectionName, EGADCollection::Type InType );

	/**
	 * Destroy a collection.
	 * 
	 * @param InCollectionName The name of collection to remove
	 * @param InType Type of collection to destroy
	 *
	 * @return True if collection was destroyed successfully.
	 */
	bool DestroyCollection( System::String^ InCollectionName, EGADCollection::Type InType );

	/**
	 * Copies (or renames/moves) a collection
	 * 
	 * @param InCurrentCollectionName The collection to rename
	 * @param InCurrentType The type of collection specified by InCurrentCollectionName
	 * @param InNewCollectionName The new name of the collection
	 * @param InNewType The type of collection specified by InNewCollectionName
	 * @param bInMove True if the old collection should be destroyed after copying it
	 *
	 * @return TRUE is succeeded, FALSE if failed.
	 */
	bool CopyCollection( String^ InCurrentCollectionName, EGADCollection::Type InCurrentType, String^ InNewCollectionName, EGADCollection::Type InNewType, bool bInMove );

	/**
	 * Add a list of assets to a collection.
	 *
	 * @param InCollectionName  The name of collection to which to add
	 * @param InType			The type of collection specified by InCollectionName
	 * @param InAssetFullNames  The full names of the assets to add
	 *
	 * @return True if successful
	 */
	bool AddAssetsToCollection( System::String^ InCollectionName, EGADCollection::Type InType, Generic::ICollection<System::String^>^ InAssetFullNames );


	/**
	 * Remove a list of assets from a collection.
	 *
	 * @param InCollectionName  The name of collection from which to remove
	 * @param InType			The type of collection specified by InCollectionName
	 * @param InAssetFullNames  The full names of the assets to remove
     *
	 * @return True if successful
	 */
	bool RemoveAssetsFromCollection( System::String^ InCollectionName, EGADCollection::Type InType, Generic::ICollection<System::String^>^ InAssetFullNames );


	/**  
	 * Find all the assets in a given group of collections
	 * 
	 * @param InCollectionName  Names of collections
	 * @param InType			The type of collection specified by InCollectionName
	 * @param OutAssetFullNames	[Out] Set of assets in specified collection
	 */
	void QueryAssetsInCollections( Generic::ICollection<String^>^ InCollectionName, EGADCollection::Type InType, [Out] List<String^>^% OutAssetFullNames ) const;

	/**  
	 * Find all the assets in a given collection
	 * 
	 * @param InCollectionName  Name of collection
	 * @param InType			The type of collection specified by InCollectionName
	 * @param OutAssetFullNames	[Out] Set of assets in specified collection
	 */
	void QueryAssetsInCollection( String^ InCollectionName, EGADCollection::Type InType, [Out] List<String^>^% OutAssetFullNames ) const;



	/**
	 * Adds or updates the default system tags for an asset in the local database
	 *
	 * @param	InAssetFullName			Asset full name
	 * @param	InObjectTypeName		Object type name
	 * @param	InOutermostPackageName	Object's outermost package
	 * @param	bInIsArchetype			True if the object is an Archetype
	 * @param	bSendToJournalIfNeeded	If true, default tags for new assets will be uploaded to the journal server
 	 */
	void SetDefaultTagsForAsset( String^ InAssetFullName, String^ InObjectTypeName, String^ InOutermostPackageName, bool bInIsArchetype, bool bSendToJournalIfNeeded );


	/** Adds an asset -> tag name mapping to our in-memory hash tables */
	void AddTagMapping( String^ InAssetFullName, String^ InTag, bool bIsAuthoritative );

	/** Removes an asset -> tag name mapping from our in-memory hash tables */
	void RemoveTagMapping( String^ InAssetFullName, String^ InTag );


	/** Queries a set of unique asset paths */
	RefCountedStringDictionary^ GetUniqueAssetPaths()
	{
		return UniqueAssetPaths.get();
	}


	/** Static: Returns true if the specified tag is a system tag */
	static bool IsSystemTag( String^ Tag );

	
	/** Static: Returns the system tag type for the specified tag, or Invalid if it's not a system tag */
	static ESystemTagType GetSystemTagType( String^ Tag );

	/** Converts a GADCollectionType to an ESystemTagType. */
	static ESystemTagType GADCollectionTypeToSystemTag( EGADCollection::Type InType );

	/** Static: Queries the value part of a system tag, or returns an empty string if not a system tag */
	static String^ GetSystemTagValue( String^ Tag );

	
	/** Static: Creates a system tag name for the specified system tag type and tag value */
	static String^ MakeSystemTag( ESystemTagType InSystemTagType, String^ TagValue );

	
	/** Static: Returns true if the Tag is a collection system tag*/
	static bool IsCollectionTag( String^ Tag, EGADCollection::Type InType );

	/** Static: Returns true if the specified private collection tag is valid for the current user */
	static bool IsMyPrivateCollection( String^ PrivateCollectionTag );

	/** Sets the global error message text.  Used internally only. */
	void SetErrorMessageText( String^ InErrorString );

	/** Print the current state of the database for debug */
	void FGameAssetDatabase::DumpDatabase();


	/** Returns the global error message text. */
	String^ GetErrorMessageText() const;


	/** Static: Returns the file name of the offline journal file we should use */
	static String^ MakeOfflineJournalFileName();


#endif	// __cplusplus_cli



	/**
	 * Returns true if the journal server is unavailable for some reason and tag writes should be disabled
	 *
	 * @return	True if database is in read-only mode
	 */
	bool IsReadOnly() const;



	/**
	 * Queries all tags
	 *
	 * @param	OutTags		List of all tags
	 */
	void QueryAllTags( TArray< FString >& OutTags ) const;



	/**
	 * Queries all asset names (warning: this can potentially be a LOT of data!)
	 *
	 * @param	OutAssetFullNames		List of all asset full names
	 */
	void QueryAllAssets( TArray< FString >& OutAssetFullNames ) const;



	/**
	 * Queries all asset names (warning: this can potentially be a LOT of data!)
	 *
	 * @param	OutAssetFullNameFNames		List of all asset full names
	 */
	void QueryAllAssets( TLookupMap< FName >& OutAssetFullNameFNames ) const;



	/**
	 * Checks to see if the specified asset is in the database
	 *
	 * @param	InAssetFullName		Full name of the asset
	 *
	 * @return	True if the asset is known to the database
	 */
	UBOOL IsAssetKnown( const FString& InAssetFullName ) const;


	/**
	 * Create a new (persistent) tag.
	 *
	 * @param InTag   The tag to create.
	 */
	UBOOL CreateTag( const FString& InTag );

	/**
	 * Destroy a tag. Also untags all assets tagged with this tag.
	 *
	 * @param InTag   The tag to destroy.
	 */
	UBOOL DestroyTag( const FString& InTag );
	
	/**
	 * Removes a tag from an asset
	 *
	 * @param  InAssetFullNames Full names of the asset from which to remove tags
	 * @param  InTag            Tag to remove
	 * 
	 * @return True if successful
	 */
	UBOOL FGameAssetDatabase::RemoveTagFromAssets( const TArray<FString>& InAssetFullNames, const FString& InTag );

	/**
	 * Associates a tag with a group of assets
	 *
	 * @param   InAssetFullNames    Full names of the assets to tag
	 * @param   InTag               Tag to assign
	 *
	 * @return  True if successful
	 */
	UBOOL FGameAssetDatabase::AddTagToAssets( const TArray<FString>& InAssetFullNames, const FString& InTag );

	/**
	 * Finds the tags associated with the specified asset
	 *
	 * @param	InAssetFullName	Full name of the asset
	 * @param	OutTags			[Out] List of tags found
	 */
	void QueryTagsForAsset( const FString& InAssetFullName, TArray< FString >& OutTags ) const;


	/**
	 * Finds the tags associated with the specified asset
	 *
	 * @param	InAssetFullName	Full name of the asset
	 * @param	OutTags			[Out] List of tags found
	 */
	void QueryTagsForAsset( const FName InAssetFullNameFName, const ETagQueryOptions::Type InOptions, TArray< FString >& OutTags ) const;


	/**
	 * Finds all of the assets with the specified tag
	 *
	 * @param	InTag				Tag
	 * @param	OutAssetFullNames	[Out] List of asset full names found
	 */
	void QueryAssetsWithTag( const FString& InTag, TArray< FString >& OutAssetFullNames ) const;



	/**
	 * Finds all of the assets with all of the specified set of tags, and any of another set of tags.  If
	 * the "any tags" list is empty, ALL assets will be considered.
	 *
	 * @param	InAllTags			An asset must have all of these tags assigned to be returned
	 * @param	InAnyTags			An asset may have any of these tags to be returned
	 * @param	OutAssetFullNames	[Out] List of asset full names found
	 */
	void QueryAssetsWithTags( const TArray< FString >& InAllTags, const TArray< FString >& InAnyTags, TArray< FString >& OutAssetFullNames ) const;



	/**
	 * Finds all of the assets with all of the specified set of tags, and any of another set of tags
	 *
	 * @param	InAllTags				An asset must have all of these tags assigned to be returned
	 * @param	InAnyTags				An asset may have any of these tags to be returned
	 * @param	OutAssetFullNameFNames	[Out] List of asset full names found
	 */
	void QueryAssetsWithTags( const TArray< FString >& InAllTags, const TArray< FString >& InAnyTags, TLookupMap< FName >& OutAssetFullNameFNames ) const;



	/**
	 * Finds all of the assets with all of the specified tags
	 *
	 * @param	InAllTags			List of tags.  For an asset to be returned, it must be associated with all of these tags.
	 * @param	OutAssetFullNames	[Out] List of assets found
	 */
	void QueryAssetsWithAllTags( const TArray< FString >& InAllTags, TArray< FString >& OutAssetFullNames ) const;



	/**
	 * Finds all of the assets with any of the specified set of tags.  If the "any tags" list is empty, ALL assets
	 * will be considered.
	 *
	 * @param	InTags				List of tags.  For an asset to be returned, it must be associated with any of these tags.
	 * @param	OutAssetFullNames	[Out] List of assets found
	 */
	void QueryAssetsWithAnyTags( const TArray< FString >& InTags, TArray< FString >& OutAssetFullNames ) const;


	/**
	 * Finds all of the assets with at least one tag in each of the specified tag sets
	 *
	 * @param	InTagSetList		List of tag sets.  Returned assets must have at least one tag in each set.
	 * @param	OutAssetFullNames	[Out] List of assets found (full names)
	 */
	void QueryAssetsWithTagInAllSets( const TArray< TArray< FString >* >& InTagSetList, TArray< FString >& OutAssetFullNames ) const;


	/**
	 * Finds all of the assets with at least one tag in each of the specified tag sets
	 *
	 * @param	InTagSetList	List of tag sets.  Returned assets must have at least one tag in each set.
	 * @param	OutAssetFNames	[Out] List of assets found (full names)
	 */
	void QueryAssetsWithTagInAllSets( const TArray< TArray< FString >* >& InTagSetList, TLookupMap< FName >& OutAssetFullNameFNames ) const;


	/**
	 * Create a new collection into which assets can be added.
	 * The collection name must be unique.
	 *
	 * @param InCollectionName Name of collection to create 
	 * @param InType Type of collection to create
	 * 
	 * @return True if collection was created successfully
	 */
	UBOOL CreateCollection( const FString& InCollectionName, const EGADCollection::Type InType );


	/**
	 * Destroy a collection.
	 * 
	 * @param InCollectionName The name of collection to remove
	 * @param InType Type of collection to destroy
	 *
	 * @return True if collection was destroyed successfully.
	 */
	UBOOL DestroyCollection( const FString& InCollectionName, const EGADCollection::Type InType );


	/**
	 * Add an asset to a collection.
	 *
	 * @param InCollectionName  The name of collection to which to add
	 * @param InType			The type of collection specified by InCollectionName
	 * @param InAssetFullNames  The full names of the assets to add
	 *
	 * @return True if successful
	 */
	UBOOL AddAssetsToCollection( const FString& InCollectionName, const EGADCollection::Type InType, const TArray< FString >& InAssetFullNames );


	/**
	 * Remove a list of assets from a collection.
	 *
	 * @param InCollectionName  The name of collection from which to remove
	 * @param InType			The type of collection specified by InCollectionName
	 * @param InAssetFullNames  The full names of the assets to remove
     *
	 * @return True if successful
	 */
	UBOOL RemoveAssetsFromCollection( const FString& InCollectionName, const EGADCollection::Type InType, const TArray< FString >& InAssetFullNames );


	/**
	 * Create a new collection into which assets can be added.
	 * The collection name must be unique.
	 *
	 * @param InCollectionName Name of collection to create 
	 * @param InType Type of collection to create
	 */
	void QueryAssetsInCollection( const FString& InCollectionName, const EGADCollection::Type InType, TArray< FString >& OutAssetFullNames );


	/** Static: Returns true if the specified tag is a system tag */
	static bool IsSystemTag( const FString& Tag );

	/** Static: Returns an object type system tag given a class name. */
	static FString MakeObjectTypeSystemTag( const FString& ClassName );

	/**
	 * Returns an error message string that describes in detail why the previous method failed.  The text
	 * will already be localized, where possible
	 *
	 * @return	Localized error message text
	 */
	const FString& GetErrorString() const;


	/**
	 * Removes all tag mappings for the specified asset.  Called when an asset is deleted.
	 *
	 * @param	AssetFullNameFName	the Unreal full name for the asset to remove tag mappings for.
	 */
	void RemoveAssetTagMappings( const FName& AssetFullNameFName );


	/**
	 * Get a list of asset classes which are valid for displaying in the content browser.
	 * The list will be generated on demand the first time this method is called.
	 *
	 *  @return	the list of class names which are valid asset types
	 */
	static const TSet<FName>& GetAllowedClassList();


	/** Static: Queries a local user name to use for journal entries and private collections */
	static FString GetLocalUserName();


protected:

	/**
	* Utility method for generating a list of asset classes which are valid for displaying in the content browser.
	*
	* @param	AllowedClassesNames		receives the list of class names which are valid asset types
	*/
	static void GenerateAllowedClassList( TSet<FName>& AllowedClassesNames );

	/** Adds an asset -> tag name mapping to our in-memory hash tables */
	void AddTagMapping( const FName& InAssetFullNameFName, const FName& InTagFName, bool bIsAuthoritative );


private:

	/** Constructor */
	FGameAssetDatabase();

	/** Destructor */
	virtual ~FGameAssetDatabase();




#ifdef __cplusplus_cli

private:
	/** Updates the game asset database from the content journal server */
	bool UpdateDatabase( const FGameAssetDatabaseStartupConfig& InConfig );

	/** Searches for assets in the database that don't exist on disk and warns about (or purges) them */
	void CheckForGhostAssets( TSet< FName > &CurrentlyExistingAssets, const FGameAssetDatabaseStartupConfig &InConfig );

	/** Static: Returns the file name of the checkpoint file we should use */
	static String^ MakeCheckpointFileName();

	/** Locally create a tag */
	void LocalCreateTag( String^ InTag );

	/** Locally destroy a tag */
	void LocalDestroyTag( String^ InTag );

	/** Locally destroy an asset from the database */
	void LocalDestroyAsset( String^ InAssetFullName );

	/** Static: Get JournalAlarmTime from ini file*/
	static DateTime GetJournalAlarmTime (void);

	/** Static: Sets JournalAlaramTime to ini file*/
	static void SetJournalAlarmTime (DateTime InAlarmTime);

	/** Static: Update the journal */
	static UBOOL JournalUpdate (void);

	/** Static: Show and react to dialog when journal alarm goes off */
	static void ShowJournalAlarmDialog (void);

	/** Static: Finds if in offline mode or not*/
	static UBOOL GetInOfflineMode (void);

	/** Attempts to load the checkpoint file from disk */
	bool LoadCheckpointFile( [Out] DateTime^% OutCheckpointFileTimeStamp );

	/** Attempts to save a new checkpoint file to disk */
	bool SaveCheckpointFile();

	/** Attempts to load journal data from disk and from the server */
	bool LoadJournalData( String^ RestrictUserName, DateTime^ IgnoreServerEntriesAsOldAs, DateTime^ DeleteServerEntriesOlderThan, bool bIsAuthoritative, List< int >^& OutDatabaseIndicesToDelete );

	/** Loads all server journal data from all branches and games */
	bool LoadAllServerJournalData( DateTime^ DeleteServerEntriesOlderThan, List< int >^& OutDatabaseIndicesToDelete );

	/** Loads asset names from a specific package.  Returns the number of assets found. */
	int GatherAssetsFromPackageLinker( ULinkerLoad* InLinker, const TSet< FName >& InAllowedClasses, TSet< FName >& InOutCurrentlyExistingAssets );

	/** Rebuilds all default tags for the entire game */
	void RebuildDefaultTags( TSet< FName >& OutCurrentlyExistingAssets );

	/** Static: Returns a proper tag name for the specified private collection name */
	static String^ MakeTagNameForPrivateCollection( String^ InCollectionName );

	/** Globally renames the specified asset to the specified new name */
	void RenameAssetInAllTagMappings( String^ OldAssetName, String^ NewAssetName );

	/** Fixes any broken asset names in the database (backwards compatibility) */
	void FixBrokenAssetNames();

	/** Checks the integrity of the asset database */
	void VerifyIntegrityOfDatabase( UBOOL bShouldTryToRepair );

#endif	// __cplusplus_cli


	/** Static: Global instance of the game asset database */
	static FGameAssetDatabase* GameAssetDatabaseSingleton;


	/** Cached error string, updated when most methods fail.  Stored localized where possible. */
	FString ErrorMessageText;


	typedef TSetMap< FName, FName > FNameMultiMap;

	/** Maps content tags to game asset full names */
	FNameMultiMap TagToAssetMap;

	/** Maps game asset full names to the content tags associated with the asset */
	FNameMultiMap AssetToTagMap;

	typedef TSet<FName> FNameSet;

	/** A set of all existing tags */
	FNameSet KnownTags;

	/** Transient dictionary of unique asset paths (without the object names -- e.g.  "Package.Group.Group").  This
	    is automatically generated and maintained by the asset database.  Each key maps to an integer that is the
		number of assets in the GAD still referencing that path. */
	AutoGCRoot( RefCountedStringDictionary^ ) UniqueAssetPaths;

	/** True if the journal server is available.  This is checked when the system is initialized and cached. */
	UBOOL bIsJournalServerAvailable;

	/** Client for interacting with a journal server */
	AutoGCRoot( MGameAssetJournalClient^ ) JournalClient;

	/** True if we're in "offline mode".  This is checked when the system is initialized and cached. */
	UBOOL bInOfflineMode;

	/** Journal file for offline actions */
	AutoGCRoot( MGameAssetJournalFile^ ) OfflineJournalFile;

};



#endif	// __GameAssetDatabaseCLR_h__


