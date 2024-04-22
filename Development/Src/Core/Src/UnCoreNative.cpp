/*=============================================================================
	UnCoreNative.h: Native function lookup table for static libraries.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "CrossLevelReferences.h"

#define STATIC_LINKING_MOJO 1

// Register things.
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name) FName CORE_##name;
#define AUTOGENERATE_FUNCTION(cls,idx,name) IMPLEMENT_FUNCTION(cls,idx,name)
#include "CoreClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NAMES_ONLY

// Register natives.
#define NATIVES_ONLY
#define NAMES_ONLY
#define AUTOGENERATE_NAME(name)
#define AUTOGENERATE_FUNCTION(cls,idx,name)
#include "CoreClasses.h"
#undef AUTOGENERATE_NAME

#undef AUTOGENERATE_FUNCTION
#undef NATIVES_ONLY
#undef NAMES_ONLY

//@script patcher
#include "UnScriptPatcher.h"

/**
 * Initialize registrants, basically calling StaticClass() to create the class and also 
 * populating the lookup table.
 *
 * @param	Lookup	current index into lookup table
 */
void AutoInitializeRegistrantsCore( INT& Lookup )
{
	AUTO_INITIALIZE_REGISTRANTS_CORE
}

/**
 * Auto generates names.
 */
void AutoGenerateNamesCore()
{
	#define NAMES_ONLY
    #define AUTOGENERATE_NAME(name) CORE_##name = FName(TEXT(#name));
		#include "CoreNames.h"
	#undef AUTOGENERATE_NAME

	#define AUTOGENERATE_FUNCTION(cls,idx,name)
	#include "CoreClasses.h"
	#undef AUTOGENERATE_FUNCTION
	#undef NAMES_ONLY
}

#if CHECK_NATIVE_CLASS_SIZES
#if _MSC_VER
#pragma optimize( "", off )
#endif

void AutoCheckNativeClassSizesCore( UBOOL& Mismatch )
{
#define NAMES_ONLY
#define AUTOGENERATE_NAME( name )
#define AUTOGENERATE_FUNCTION( cls, idx, name )
#define VERIFY_CLASS_SIZES
#include "CoreClasses.h"
#undef AUTOGENERATE_FUNCTION
#undef AUTOGENERATE_NAME
#undef NAMES_ONLY
#undef VERIFY_CLASS_SIZES
}

#if _MSC_VER
#pragma optimize( "", on )
#endif
#endif

/** Map of class name to lookup function pointer */
TMap<FName, FNativeFunctionLookup*> GNativeLookupFuncs;

/*-----------------------------------------------------------------------------
	UTextBufferFactory.
-----------------------------------------------------------------------------*/
UTextBufferFactory::UTextBufferFactory()
{
	if ( HasAnyFlags(RF_ClassDefaultObject) )
	{
		new(Formats) FString( TEXT("txt;Text files") );
		SupportedClass = UTextBuffer::StaticClass();
		bCreateNew     = 0;
		bText          = 1;
	}
}
UObject* UTextBufferFactory::FactoryCreateText
(
	UClass*				Class,
	UObject*			InOuter,
	FName				InName,
	EObjectFlags		InFlags,
	UObject*			Context,
	const TCHAR*		Type,
	const TCHAR*&		Buffer,
	const TCHAR*		BufferEnd,
	FFeedbackContext*	Warn
)
{
	// Import.
	UTextBuffer* Result = new(InOuter,InName,InFlags)UTextBuffer(Buffer);
	return Result;
}
IMPLEMENT_CLASS(UTextBufferFactory);


/*-----------------------------------------------------------------------------
	UTextBuffer implementation.
-----------------------------------------------------------------------------*/

UTextBuffer::UTextBuffer( const TCHAR* InText )
: Text( InText )
{}
void UTextBuffer::Serialize( const TCHAR* Data, EName Event )
{
	Text += (TCHAR*)Data;
}
void UTextBuffer::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Pos << Top << Text;
}
IMPLEMENT_CLASS(UTextBuffer);


/*-----------------------------------------------------------------------------
	UMetaData implementation.
-----------------------------------------------------------------------------*/

void UMetaData::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	Ar << ObjectMetaDataMap;

#if 0
	debugf(TEXT("METADATA %s"), *GetPathName());
	for (TMap<UObject*, TMap<FName, FString> >::TIterator It(ObjectMetaDataMap); It; ++It)
	{
		TMap<FName, FString>& MetaDataValues = It.Value();
		for ( TMap<FName, FString>::TIterator MetaDataIt(MetaDataValues); MetaDataIt; ++MetaDataIt )
		{
			debugf(TEXT("%s: %s=%s"), *It.Key()->GetPathName(), *MetaDataIt.Key().ToString(), *MetaDataIt.Value());
		}
	}
#endif
}

/**
 * Return the value for the given key in the given property
 * @param Object the object to lookup the metadata for
 * @param Key The key to lookup
 * @return The value if found, otherwise an empty string
 */
const FString& UMetaData::GetValue(const UObject* Object, FName Key)
{
	// if not found, return a static empty string
	static FString EmptyString;

	// every key needs to be valid
	if (Key == NAME_None)
	{
		return EmptyString;
	}

	// look up the existing map if we have it
	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);

	// if not, return empty
	if (ObjectValues == NULL)
	{
		return EmptyString;
	}

	// look for the property
	FString* ValuePtr = ObjectValues->Find(Key);
	
	// if we didn't find it, return NULL
	if (!ValuePtr)
	{
		return EmptyString;
	}

	// if we found it, return the pointer to the character data
	return *ValuePtr;

}

/**
 * Return the value for the given key in the given property
 * @param Object the object to lookup the metadata for
 * @param Key The key to lookup
 * @return The value if found, otherwise an empty string
 */
const FString& UMetaData::GetValue(const UObject* Object, const TCHAR* Key)
{
	// only find names, don't bother creating a name if it's not already there
	// (GetValue will return an empty string if Key is NAME_None)
	return GetValue(Object, FName(Key, FNAME_Find));
}

/**
 * Return whether or not the Key is in the meta data
 * @param Object the object to lookup the metadata for
 * @param Key The key to query for existence
 * @return TRUE if found
 */
UBOOL UMetaData::HasValue(const UObject* Object, FName Key)
{
	// every key needs to be valid
	if (Key == NAME_None)
	{
		return FALSE;
	}

	// look up the existing map if we have it
	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);

	// if not, return FALSE
	if (ObjectValues == NULL)
	{
		return FALSE;
	}

	// if we had the map, see if we had the key
	return ObjectValues->Find(Key) != NULL;
}

/**
 * Return whether or not the Key is in the meta data
 * @param Object the object to lookup the metadata for
 * @param Key The key to query for existence
 * @return TRUE if found
 */
UBOOL UMetaData::HasValue(const UObject* Object, const TCHAR* Key)
{
	// only find names, don't bother creating a name if it's not already there
	// (HasValue will return FALSE if Key is NAME_None)
	return HasValue(Object, FName(Key, FNAME_Find));
}

/**
 * Is there any metadata for this property?
 * @param Object the object to lookup the metadata for
 * @return TrUE if the property has any metadata at all
 */
UBOOL UMetaData::HasObjectValues(const UObject* Object)
{
	return ObjectMetaDataMap.HasKey(Object);
}

/**
 * Set the key/value pair in the Property's metadata
 * @param Object the object to set the metadata for
 * @Values The metadata key/value pairs
 */
void UMetaData::SetObjectValues(const UObject* Object, const TMap<FName, FString>& ObjectValues)
{
	ObjectMetaDataMap.Set(const_cast<UObject*>(Object), ObjectValues);
}

/**
 * Set the key/value pair in the Property's metadata
 * @param Object the object to set the metadata for
 * @param Key A key to set the data for
 * @param Value The value to set for the key
 * @Values The metadata key/value pairs
 */
void UMetaData::SetValue(const UObject* Object, FName Key, const FString& Value)
{
	check(Key != NAME_None);

	// look up the existing map if we have it
	TMap<FName, FString>* ObjectValues = ObjectMetaDataMap.Find(Object);

	// if not, create an empty map
	if (ObjectValues == NULL)
	{
		ObjectValues = &ObjectMetaDataMap.Set(const_cast<UObject*>(Object), TMap<FName, FString>());
	}

	// set the value for the key
	ObjectValues->Set(Key, *Value);
}

/**
 * Set the key/value pair in the Property's metadata
 * @param Object the object to set the metadata for
 * @param Key A key to set the data for
 * @param Value The value to set for the key
 * @Values The metadata key/value pairs
 */
void UMetaData::SetValue(const UObject* Object, const TCHAR* Key, const FString& Value)
{
	SetValue(Object, FName(Key), Value);
}

/**
 * Method to help support metadata for intrinsic classes. Attempts to find and set meta data for a given intrinsic class and subobject
 * within an ini file. The keys in the ini file are expected to be in the format of ClassToUseName.ObjectToUseName, where all of the characters
 * in the names are alphanumeric.
 *
 * @param	ClassToUse			Intrinsic class to attempt to find meta data for
 * @param	ObjectToUse			Sub-object, such as a property or enum, of the provided intrinsic class
 * @param	MetaDataToAddTo		Metadata object that any parsed metadata found in the ini will be added to
 *
 * @return	TRUE if any data was found, parsed, and added; FALSE otherwise
 */
UBOOL UMetaData::AttemptParseIntrinsicMetaData( const UClass& ClassToUse, const UObject& ObjectToUse, UMetaData& MetaDataToAddTo )
{
	// Assume no data will be parsed
	UBOOL bParsedData = FALSE;

	// Only try to parse meta data inside the editor
	if ( GIsEditor )
	{
		static TSet<FString> ParsedIntrinsicMetaData;

		const FString& ObjectName = ObjectToUse.GetName();

		// Construct a string in the form of ClassToUseName_ObjectToUseName in order to uniquely identify this object
		const FString& ClassAndObjName = FString::Printf( TEXT("%s_%s"), *ClassToUse.GetName(), *ObjectName );

		// If class to use is intrinsic and the class/object combo hasn't had an attempted parse performed for it before, try to parse metadata from an ini file
		if ( ClassToUse.HasAnyClassFlags( CLASS_Intrinsic ) && !ParsedIntrinsicMetaData.Contains( ClassAndObjName ) && GConfig )
		{
			// Add this ClassToUseName_ObjectToUseName to the TSet to mark it as having been parsed
			ParsedIntrinsicMetaData.Add( ClassAndObjName );

			// Construct a string in the form of ClassToUseName_ObjectToUseName where ObjectToUseName is modified to only contain alphanumeric characters
			FString AlphaNumClassAndObjName = ClassToUse.GetName() + TEXT("_");
			for ( INT CharIndex = 0; CharIndex < ObjectName.Len(); ++CharIndex )
			{
				if ( appIsAlnum( ObjectName[CharIndex] ) )
				{
					AlphaNumClassAndObjName += ObjectName[CharIndex];
				}
			}

			// Query the ini file for a key in the form ClassToUseName_ObjectToUseName
			FString IntrinsicMetaData;
			if ( GConfig->GetString( TEXT("IntrinsicMetaData"), *AlphaNumClassAndObjName, IntrinsicMetaData, GEditorIni ) )
			{
				// If the ini file had a string for the class/object combo, it should specify any number of metadata keys/values, each delimited
				// by the | character. Parse the string into an array split on the | character.
				TArray<FString> MetaDataSubStrings;
				IntrinsicMetaData.ParseIntoArray( &MetaDataSubStrings, TEXT("|"), TRUE );

				// Ensure that the parsed array has an even number of elements. If it doesn't, then each metadata key does not have a corresponding
				// value and the data is likely incorrect and should be ignored.
				if ( ( MetaDataSubStrings.Num() & 1 ) == 0 )
				{
					// Loop through each key/value pair, adding them to the meta data for the provided object
					for ( INT MetaDataIndex = 0; MetaDataIndex + 1 < MetaDataSubStrings.Num(); MetaDataIndex += 2 )
					{
						// Trim any stray whitespace from the key/value pairs
						FString& CurKey = MetaDataSubStrings(MetaDataIndex);
						CurKey = CurKey.Trim();
						CurKey = CurKey.TrimTrailing();

						FString& CurValue = MetaDataSubStrings(MetaDataIndex + 1);
						CurValue = CurValue.Trim();
						CurValue = CurValue.TrimTrailing();

						// Ensure the current key doesn't already have metadata for it, and if it doesn't, add the new metadata
						if ( !MetaDataToAddTo.HasValue( &ObjectToUse, *CurKey ) )
						{
							MetaDataToAddTo.SetValue( &ObjectToUse, *CurKey, CurValue );

							// Confirm that data has been parsed
							bParsedData = TRUE;
						}
					}
				}
			}
		}
	}

	return bParsedData;
}

/**
 * Removes any metadata entries that are to objects not inside the same package as this UMetaData object.
 */
void UMetaData::RemoveMetaDataOutsidePackage()
{
	TArray<UObject*> ObjectsToRemove;

	// Get the package that this MetaData is in
	UPackage* MetaDataPackage = GetOutermost();

	// Iterate over all entries..
	for (TMap<UObject*, TMap<FName, FString> >::TIterator It(ObjectMetaDataMap); It; ++It)
	{
		UObject* Obj = It.Key();
		// See if its package is not the same as the MetaData's
		if((Obj != NULL) && (Obj->GetOutermost() != MetaDataPackage))
		{
			// Add to list of things to remove
			ObjectsToRemove.AddItem(Obj);
		}
	}

	// Go through and remove any objects that need it
	for(INT i=0; i<ObjectsToRemove.Num(); i++)
	{
		UObject* Obj = ObjectsToRemove(i);

		debugf(TEXT("Removing '%s' ref from Metadata '%s'"), *Obj->GetPathName(), *GetPathName());
		ObjectMetaDataMap.Remove( Obj );
	}
}

IMPLEMENT_CLASS(UMetaData);

/*-----------------------------------------------------------------------------
	UEnum implementation.
-----------------------------------------------------------------------------*/

/** Mapping from enum names to their index used to speed up lookup in the make commandlet. Only valid if GIsUCCMake == TRUE. */ 
TMap<FName,INT> GUCCMakeEnumNameToIndexMap;

void UEnum::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);
	Ar << Names;

#if SUPPORTS_SCRIPTPATCH_LOADING
	//@script patcher
	// if we are loading from disk, check to see if this enum has been patched with new values
	if ( Ar.IsLoading() )
	{
		ULinker* LinkerAr = Ar.GetLinker();
		if ( LinkerAr != NULL )
		{
			ULinkerLoad* Linker = GetLinker();
			checkSlow(LinkerAr == Linker);

			FEnumPatchData* ValuePatch = Linker->FindEnumPatch(GetLinkerIndex());
			if ( ValuePatch != NULL )
			{
				Names = ValuePatch->EnumValues;
			}
		}
	}
#endif

	// Populate enum name to index mapping.
	if( GIsUCCMake )
	{
		for( INT NameIndex=0; NameIndex<Names.Num(); NameIndex++ )
		{
			GUCCMakeEnumNameToIndexMap.Set( Names(NameIndex), NameIndex );
		}
	}
}

/**
 * Find the longest common prefix of all items in the enumeration.
 * 
 * @return	the longest common prefix between all items in the enum.  If a common prefix
 *			cannot be found, returns the full name of the enum.
 */
FString UEnum::GenerateEnumPrefix() const
{
	FString Prefix;
	if (Names.Num() > 0)
	{
		Prefix = Names(0).ToString();

		// For each item in the enumeration, trim the prefix as much as necessary to keep it a prefix.
		// This ensures that once all items have been processed, a common prefix will have been constructed.
		// This will be the longest common prefix since as little as possible is trimmed at each step.
		for (INT NameIdx = 1; NameIdx < Names.Num(); NameIdx++)
		{
			FString EnumItemName = *Names(NameIdx).ToString();

			// Find the length of the longest common prefix of Prefix and EnumItemName.
			INT PrefixIdx = 0;
			while (PrefixIdx < Prefix.Len() && PrefixIdx < EnumItemName.Len() && Prefix[PrefixIdx] == EnumItemName[PrefixIdx])
			{
				PrefixIdx++;
			}

			// Trim the prefix to the length of the common prefix.
			Prefix = Prefix.Left(PrefixIdx);
		}

		// Find the index of the rightmost underscore in the prefix.
		INT UnderscoreIdx = Prefix.InStr(TEXT("_"), TRUE);

		// If an underscore was found, trim the prefix so only the part before the rightmost underscore is included.
		if (UnderscoreIdx > 0)
		{
			Prefix = Prefix.Left(UnderscoreIdx);
		}
		else
		{
			// no underscores in the common prefix - this probably indicates that the names
			// for this enum are not using Epic's notation, so just empty the prefix so that
			// the max item will use the full name of the enum
			Prefix.Empty();
		}
	}

	// If no common prefix was found, or if the enum does not contain any entries,
	// use the name of the enumeration instead.
	if (Prefix.Len() == 0)
	{
		Prefix = GetName();
	}
	return Prefix;
}

/**
 * Sets the array of enums.
 *
 * @return	TRUE unless the MAX enum already exists and isn't the last enum.
 */
UBOOL UEnum::SetEnums(TArray<FName>& InNames)
{
	Names.Empty();
	Names = InNames;
	return GenerateMaxEnum();
}

/**
 * Adds a virtual _MAX entry to the enum's list of names, unless the
 * enum already contains one.
 *
 * @return	TRUE unless the MAX enum already exists and isn't the last enum.
 */
UBOOL UEnum::GenerateMaxEnum()
{
	const FString EnumPrefix = GenerateEnumPrefix();
	checkSlow(EnumPrefix.Len());

	const FName MaxEnumItem = *(EnumPrefix + TEXT("_MAX"));
	const INT MaxEnumItemIndex = Names.FindItemIndex(MaxEnumItem);
	if ( MaxEnumItemIndex == INDEX_NONE )
	{
		Names.AddItem(MaxEnumItem);
	}

	// Populate enum name to index mapping.
	if( GIsUCCMake )
	{
		for( INT NameIndex=0; NameIndex<Names.Num(); NameIndex++ )
		{
			GUCCMakeEnumNameToIndexMap.Set( Names(NameIndex), NameIndex );
		}
	}

	if ( MaxEnumItemIndex != INDEX_NONE && MaxEnumItemIndex != Names.Num() - 1 )
	{
		// The MAX enum already exists, but isn't the last enum.
		return FALSE;
	}

	return TRUE;
}

/**
 * Wrapper method for easily determining whether this enum has metadata associated with it.
 * 
 * @param	Key			the metadata tag to check for
 * @param	NameIndex	if specified, will search for metadata linked to a specified value in this enum; otherwise, searches for metadata for the enum itself
 *
 * @return TRUE if the specified key exists in the list of metadata for this enum, even if the value of that key is empty
 */
UBOOL UEnum::HasMetaData( const TCHAR* Key, INT NameIndex/*=INDEX_NONE*/ ) const
{
	UBOOL bResult = FALSE;

	UPackage* Package = GetOutermost();
	check(Package);

	UMetaData* MetaData = Package->GetMetaData();
	check(MetaData);

	FString KeyString;

	// If an index was specified, search for metadata linked to a specified value
	if ( NameIndex != INDEX_NONE )
	{
		check(Names.IsValidIndex(NameIndex));
		KeyString = Names(NameIndex).ToString() + TEXT(".") + Key;
	}
	// If no index was specified, search for metadata for the enum itself
	else
	{
		KeyString = Key;
	}
	bResult = MetaData->HasValue( this, *KeyString );
	
	// If the metadata wasn't found, see if it's available as intrinsic class meta data in an ini file
	if ( !bResult )
	{
		UClass* OwnerClass = GetOwnerClass();
		check(OwnerClass);

		if ( UMetaData::AttemptParseIntrinsicMetaData( *OwnerClass, *this, *MetaData ) )
		{
			bResult = MetaData->HasValue( this, *KeyString );
		}
	}

	return bResult;
}

/**
 * Return the metadata value associated with the specified key.
 * 
 * @param	Key			the metadata tag to find the value for
 * @param	NameIndex	if specified, will search the metadata linked for that enum value; otherwise, searches the metadata for the enum itself
 *
 * @return	the value for the key specified, or an empty string if the key wasn't found or had no value.
 */
const FString& UEnum::GetMetaData( const TCHAR* Key, INT NameIndex/*=INDEX_NONE*/ ) const
{
	UPackage* Package = GetOutermost();
	check(Package);

	UMetaData* MetaData = Package->GetMetaData();
	check(MetaData);

	FString KeyString;

	// If an index was specified, search for metadata linked to a specified value
	if ( NameIndex != INDEX_NONE )
	{
		check(Names.IsValidIndex(NameIndex));
		KeyString = Names(NameIndex).ToString() + TEXT(".") + Key;
	}
	// If no index was specified, search for metadata for the enum itself
	else
	{
		KeyString = Key;
	}

	const FString& ResultString = MetaData->GetValue( this, *KeyString );
	
	// If the metadata wasn't found, see if it's available as intrinsic class meta data in an ini file
	if ( ResultString.Len() == 0 )
	{
		UClass* OwnerClass = GetOwnerClass();
		check(OwnerClass);

		if ( UMetaData::AttemptParseIntrinsicMetaData( *OwnerClass, *this, *MetaData ) )
		{
			return MetaData->GetValue( this, *KeyString );
		}
	}

	return ResultString;
}

IMPLEMENT_CLASS(UEnum);

/*-----------------------------------------------------------------------------
	UCommandlet.
-----------------------------------------------------------------------------*/

INT UCommandlet::Main( const FString& Params )
{
	return eventMain( Params );
}
IMPLEMENT_CLASS(UCommandlet);

/*-----------------------------------------------------------------------------
	UPackage.
-----------------------------------------------------------------------------*/
// BEGIN PIXELMINE
// Required condition for nFringe debugging support
/*****
* In some of the late 2007 QA builds, I had to add a "dummy" int field to UPackage to get this offset.
* If you add a member to UPackage before PackageFlags and the offset of PackageFlags changes, the nFringe
* debugger will no longer work.
*****
*/
int nFringePFO = STRUCT_OFFSET(UPackage,PackageFlags);	// default offset in win32 is 0x98
// END PIXELMINE


/** array of UPackages that currently have NetObjects that are relevant to netplay */
TArray<UPackage*> UPackage::NetPackages;
/** objects that should be informed of NetPackage/NetObject changes */
TArray<FNetObjectNotify*> UPackage::NetObjectNotifies;

FNetObjectNotify::~FNetObjectNotify()
{
	UPackage::NetObjectNotifies.RemoveItem(this);
}

UPackage::UPackage()
{
	if ( !HasAnyFlags(RF_ClassDefaultObject) )
	{
		// Bind to a matching DLL, if any.
		BindPackage( this );
		bDirty = 0;
	}

	MetaData = NULL;
}

/**
 * Static constructor called once per class during static initialization via IMPLEMENT_CLASS
 * macro. Used to e.g. emit object reference tokens for realtime garbage collection or expose
 * properties for native- only classes.
 */
void UPackage::StaticConstructor()
{
	UClass* TheClass = GetClass();
	TheClass->EmitObjectReference(STRUCT_OFFSET(UPackage, MetaData));
}

/**
 * Marks/Unmarks the package's bDirty flag
 */
void UPackage::SetDirtyFlag( UBOOL bIsDirty )
{
	if ( GetOutermost() != GetTransientPackage() )
	{
		if ( GUndo != NULL
		// PIE world objects should never end up in the transaction buffer as we cannot undo during gameplay.
		&& !(GetOutermost()->PackageFlags & (PKG_PlayInEditor|PKG_ContainsScript)) )
		{
			// make sure we're marked as transactional
			SetFlags(RF_Transactional);

			// don't call Modify() since it calls SetDirtyFlag()
			GUndo->SaveObject( this );
		}

		const UBOOL bSendNotfication = bDirty != bIsDirty && GCallbackEvent != NULL;

		// Update dirty bit
		bDirty = bIsDirty;
		if( bIsDirty )
		{
			// We'll also mark the package as dirty for "Play-In-Editor", which means that the PIE map for this
			// object will need to be re-saved.
			bDirtyForPIE = TRUE;
		}

		if ( bSendNotfication )
		{
			// notify the editor that this package has been modified
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageListUI, this));
		}
	}
}

/** initializes net info for this package from the ULinkerLoad associated with it
 * @param InLinker the linker associated with this package to grab info from
 * @param ExportIndex the index of the export in that linker's ExportMap where this package's export info can be found
 */
void UPackage::InitNetInfo(ULinkerLoad* InLinker, INT ExportIndex)
{
	// initialize some values for package downloading
	if (ExportIndex == INDEX_NONE)
	{
		// this package does not have a separate base
		ForcedExportBasePackageName = NAME_None;

		// get size and GUID from linker
		FileSize = InLinker->Loader->TotalSize();
		Guid = InLinker->Summary.Guid;
	}
	else
	{
		// get GUID from export
		Guid = InLinker->ExportMap(ExportIndex).PackageGuid;

		if (InLinker->LinkerRoot != this)
		{
			// remember name of the base package this forced export package was loaded from
			ForcedExportBasePackageName = InLinker->LinkerRoot->GetFName();

			// get package flags from export
			PackageFlags = InLinker->ExportMap(ExportIndex).PackageFlags;
		}
	}

	// skip if package is not meant to be replicated
	if( !(PackageFlags & PKG_ServerSideOnly) )
	{
		if (ExportIndex == INDEX_NONE)
		{
			checkSlow(InLinker->LinkerRoot == this);
			// preallocate space for object references
			// use Reserve() first so that we allocate exactly as much space as we require, none extra
			NetObjects.Reserve(InLinker->Summary.Generations.Last().NetObjectCount);
			if( InLinker->Summary.Generations.Last().NetObjectCount > NetObjects.Num() )
			{
				NetObjects.AddZeroed(InLinker->Summary.Generations.Last().NetObjectCount - NetObjects.Num());
			}
			// get how many objects are in each generation from the linker
			GenerationNetObjectCount.Empty(InLinker->Summary.Generations.Num());
			for (INT i = 0; i < InLinker->Summary.Generations.Num(); i++)
			{
				GenerationNetObjectCount.AddItem(InLinker->Summary.Generations(i).NetObjectCount);
			}
		}
		else
		{
			// get how many objects are in each generation from the linker
			const TArray<INT>& NewGenerationNetObjectCount = InLinker->ExportMap(ExportIndex).GenerationNetObjectCount;
			if (NewGenerationNetObjectCount.Num() > 0)
			{
				// only set net object info if we haven't already, so we don't disrupt any current networking
				//@todo: we probably shouldn't support reloading a different version of a package on top of one in a memory during the game,
				//		but seekfree loading currently requires this in the case of localized packages where the localized version might be a forced export
				//		in some seekfree packages while the base version is a forced export in others
				if (GenerationNetObjectCount.Num() == 0)
				{
					GenerationNetObjectCount = NewGenerationNetObjectCount;
				}
				// preallocate space for object references
				// use Reserve() first so that we allocate exactly as much space as we require, none extra
				//@note: using NewGenerationNetObjectCount here instead of GenerationNetObjectCount because even if we don't actually use the new generations for networking,
				//		we still need to make sure we have enough entries in the NetObjects array to hold everything in the package
				NetObjects.Reserve(NewGenerationNetObjectCount.Last());
				if (NewGenerationNetObjectCount.Last() > NetObjects.Num())
				{
					NetObjects.AddZeroed(NewGenerationNetObjectCount.Last() - NetObjects.Num());
				}
			}
			else
			{
				// if there is no object count, this package must be server side only
				//@todo: might need to serialize PackageFlags in export map
				PackageFlags |= PKG_ServerSideOnly;
			}
		}
	}
}

//@script patcher
/** 
 * This will add a number of NetObjects to the NetObjects list if the new count is 
 * greater than the current size of the list.
 *
 * @param NewNumNetObjects new size the NetObjects list should be
 **/
void UPackage::PatchNetObjectList( INT NewNumNetObjects )
{
	const INT NumToAdd = NewNumNetObjects - NetObjects.Num();

	if( NumToAdd > 0 )
	{
		NetObjects.AddZeroed( NumToAdd );

		check(GenerationNetObjectCount.Num()>0);
		GenerationNetObjectCount.Last() += NumToAdd;
	}
}


/** adds an object to the NetObjects list
 * @param OutObject the object to remove
 */
void UPackage::AddNetObject(UObject* InObject)
{
	if (InObject->GetNetIndex() < 0 || InObject->GetNetIndex() >= NetObjects.Num())
	{
		debugf(GConvertNetIndexErrorsToWarnings ? NAME_Warning : NAME_Error, TEXT("(AddNetObject) Object %s with invalid NetIndex %i (max: %i)"), *InObject->GetFullName(), InObject->GetNetIndex(), NetObjects.Num());
	}
	else if (NetObjects(InObject->GetNetIndex()) != NULL)
	{
		debugf(GConvertNetIndexErrorsToWarnings ? NAME_Warning : NAME_Error, TEXT("Objects %s and %s have duplicate NetIndex %i"), *InObject->GetFullName(), *NetObjects(InObject->GetNetIndex())->GetFullName(), InObject->GetNetIndex());
	}
	else
	{
		NetObjects(InObject->GetNetIndex()) = InObject;
		CurrentNumNetObjects++;

		// if we now have net objects in this package, add to GNetPackages and call callback
		if (CurrentNumNetObjects == 1)
		{
			NetPackages.AddItem(this);
			for (INT i = 0; i < NetObjectNotifies.Num(); i++)
			{
				NetObjectNotifies(i)->NotifyNetPackageAdded(this);
			}
		}
	}
}

/** removes an object from the NetObjects list
 * @param OutObject the object to remove
 */
void UPackage::RemoveNetObject(UObject* OutObject)
{
	if (OutObject->GetNetIndex() < 0 || OutObject->GetNetIndex() >= NetObjects.Num())
	{
		debugf(GConvertNetIndexErrorsToWarnings ? NAME_Warning : NAME_Error, TEXT("(RemoveNetObject) Object %s with invalid NetIndex %i (max: %i)"), *OutObject->GetFullName(), OutObject->GetNetIndex(), NetObjects.Num());
	}
	else if (NetObjects(OutObject->GetNetIndex()) != OutObject)
	{
		debugf(GConvertNetIndexErrorsToWarnings ? NAME_Warning : NAME_Error, TEXT("Objects %s and %s have duplicate NetIndex %i"), *OutObject->GetFullName(), *NetObjects(OutObject->GetNetIndex())->GetFullName(), OutObject->GetNetIndex());
	}
	else
	{
		NetObjects(OutObject->GetNetIndex()) = NULL;
		CurrentNumNetObjects--;

		for (INT i = 0; i < NetObjectNotifies.Num(); i++)
		{
			NetObjectNotifies(i)->NotifyNetObjectRemoved(OutObject);
		}

		// if we no longer have net objects in this package, remove from GNetPackages and call callback
		if (CurrentNumNetObjects == 0)
		{
			NetPackages.RemoveItem(this);
			for (INT i = 0; i < NetObjectNotifies.Num(); i++)
			{
				NetObjectNotifies(i)->NotifyNetPackageRemoved(this);
			}
		}
	}
}

/** clears NetIndex on all objects in our NetObjects list that are inside the passed in object
 * @param OuterObject the Outer to check for
 */
void UPackage::ClearAllNetObjectsInside(UObject* OuterObject)
{
	for (INT i = 0; i < NetObjects.Num(); i++)
	{
		if (NetObjects(i) != NULL && NetObjects(i)->IsIn(OuterObject))
		{
			NetObjects(i)->SetNetIndex(INDEX_NONE);
		}
	}
}

/**
 * Serializer
 * Save the value of bDirty into the transaction buffer, so that undo/redo will also mark/unmark the package as dirty, accordingly
 */
void UPackage::Serialize( FArchive& Ar )
{
	Super::Serialize(Ar);

	if ( Ar.IsTransacting() )
	{
		Ar << bDirty << bDirtyForPIE;
	}

	if (Ar.IsObjectReferenceCollector())
	{
		Ar << MetaData;
	}

	if (Ar.IsCountingMemory())
	{
		NetObjects.CountBytes(Ar);
		ExportGuids.CountBytes(Ar);
		ImportGuids.CountBytes(Ar);
	}
}

/**
 * Get (after possibly creating) a metadata object for this package
 *
 * @return A valid UMetaData pointer for all objects in this package
 */
UMetaData* UPackage::GetMetaData()
{
	if (MetaData == NULL)
	{
		// first try to load it
		if ( (PackageFlags&PKG_Compiling) == 0 )
		{
			MetaData = LoadObject<UMetaData>(this, *UMetaData::StaticClass()->GetName(), NULL, LOAD_NoWarn|LOAD_Quiet, NULL);
		}

		// if it wasn't found, then create it
		if (MetaData == NULL)
		{
			// make a metadata object, but only allow it to be loaded in the editor
			MetaData = ConstructObject<UMetaData>(UMetaData::StaticClass(), this, UMetaData::StaticClass()->GetFName(), RF_NotForClient | RF_NotForServer | RF_Standalone);
		}
		check(MetaData);
	}

	return MetaData;
}

/**
 * Fully loads this package. Safe to call multiple times and won't clobber already loaded assets.
 */
void UPackage::FullyLoad()
{
	// Make sure we're a topmost package.
	check(GetOuter()==NULL);

	// Only perform work if we're not already fully loaded.
	if( !IsFullyLoaded() )
	{
		// Mark package so exports are found in memory first instead of being clobbered.
		UBOOL bSavedState = ShouldFindExportsInMemoryFirst();
		FindExportsInMemoryFirst( TRUE );

		// Re-load this package.
		LoadPackage( NULL, *GetName(), LOAD_None );

		// Restore original state.
		FindExportsInMemoryFirst( bSavedState );

		// Refresh
		if ( GCallbackEvent != NULL )
		{
			const DWORD UpdateMask = CBR_UpdatePackageList|CBR_UpdateAssetList;
			GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, UpdateMask, this));
		}
	}
}

/**
 * Looks up all exports in the Linker's ExportGuidsAwaitingLookup map, and fills out ExportGuids
 * 
 * @param Linker The Linker that loaded this package (the package's _Linker may be NULL)
 */
void UPackage::LookupAllOutstandingCrossLevelExports(ULinkerLoad* Linker)
{
	// we need a linker if this is going to be called
	check(Linker);

	// go through any outstanding exports
	for (TMap<FGuid, INT>::TIterator It(Linker->ExportGuidsAwaitingLookup); It; ++It)
	{
		UObject* Obj = Linker->ExportMap(It.Value() - 1)._Object;
		if (Obj != NULL)
		{
			// and set the cached map's object pointer
			ExportGuids.Set(It.Key(), Obj);

#if !CONSOLE
			// the editor needs to keep track of objects and their guids for saving packages
			// so, for every cross level export object that is loaded, remember its Guid
			if (GIsEditor)
			{
				GCrossLevelReferenceManager->CrossLevelObjectToGuidMap.Set(Obj, It.Key());
			}
#endif
		}

		else
		{
			// if the object failed to be found, then for somehow it's failed to be loaded, even tho the entire package is now loaded
			debugf(NAME_DevCrossLevel, TEXT("'%s' is marked as a cross level ref target, but it did not load"), *Linker->GetExportFullName(It.Value() - 1));
		}
	}

	// done with the pending lookups
	Linker->ExportGuidsAwaitingLookup.Empty();

	// now look for any references in other levels that have cross-level references to the objects in this level 
	for (TMap<FGuid, UObject*>::TIterator It(ExportGuids); It; ++It)
	{
		// look for any objects that need to be fixed up for this object's guid
		TArray<FDelayedCrossLevelRef> ObjectsToFixup;
		GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.MultiFind(It.Key(), ObjectsToFixup);

		if (ObjectsToFixup.Num())
		{
			UObject* Obj = It.Value();
			// now, for all references to this object (by the Guid), push the value into the memory location
			for (INT FixupIndex = 0; FixupIndex < ObjectsToFixup.Num(); FixupIndex++)
			{
				// stick the now-loaded object into the object property's memory for one object
				FDelayedCrossLevelRef& Fixup = ObjectsToFixup(FixupIndex);
				UObject** PtrLoc = (UObject**)((BYTE*)Fixup.Object + Fixup.Offset);
				*PtrLoc = Obj;

				// let the object handle the pointer update
				Fixup.Object->PostCrossLevelFixup();

				// remember this location, so if the object is unloaded, we can NULL it out
				GCrossLevelReferenceManager->DelayedCrossLevelTeardownMap.Add(Obj, Fixup);

				// mark that the object has had a cross level reference to it
				Obj->SetFlags(RF_IsCrossLevelReferenced);
			}

			// remove all the entries we just filled out from the delayed fixup list
			GCrossLevelReferenceManager->DelayedCrossLevelFixupMap.Remove(It.Key());
		}
	}
}


/**
 * Returns whether the package is fully loaded.
 *
 * @return TRUE if fully loaded or no file associated on disk, FALSE otherwise
 */
UBOOL UPackage::IsFullyLoaded()
{
	// Newly created packages aren't loaded and therefore haven't been marked as being fully loaded. They are treated as fully
	// loaded packages though in this case, which is why we are looking to see whether the package exists on disk and assume it
	// has been fully loaded if it doesn't.
	if( !bHasBeenFullyLoaded )
	{
		FString DummyFilename;
		// Try to find matching package in package file cache.
		if( !GPackageFileCache->FindPackageFile( *GetName(), NULL, DummyFilename ) 
		||	(GIsEditor && GFileManager->FileSize(*DummyFilename) < 0) )
		{
			// Package has NOT been found, so we assume it's a newly created one and therefore fully loaded.
			bHasBeenFullyLoaded = TRUE;
		}
	}

	return bHasBeenFullyLoaded;
}


IMPLEMENT_CLASS(UPackage);

/*-----------------------------------------------------------------------------
	UComponent.
-----------------------------------------------------------------------------*/

/**
 * Given a component and an owner class, save a reference to it for retrieveing defaults on load
 * @param OriginalComponent		The original template for this subobject (or another instance for a duplication?)
 * @param OwnerClass			The class that contains the original template 
 */
void UComponent::LinkToSourceDefaultObject(UComponent* OriginalComponent, UClass* OwnerClass, FName ComponentName)
{
	if ( GetArchetype()->GetOuter() != GetOuter()->GetArchetype() )
	{
		UComponent* RealArchetype = ResolveSourceDefaultObject();
		
		if ( RealArchetype != NULL )
		{
			SetArchetype(RealArchetype,TRUE);
		}
		else
		{
			SetArchetype(GetClass()->GetDefaultObject(),TRUE);
		}

		MarkPackageDirty();
	}
}

/**
 * Find the subobject default object in the defaultactorclass, or in some base class.
 * This function goes away once the version is incremented.
 *
 * @return The object pointed to by the SourceDefaultActorClass and SourceDefaultSubObjectName
 */
UComponent* UComponent::ResolveSourceDefaultObject()
{
	FName ComponentName = TemplateName != NAME_None
		? TemplateName
		: GetFName();

	UComponent* Result = GetOuter()->GetArchetype()->FindComponent(ComponentName,TRUE);

	// this can probably go away
	UComponent* ComponentPtr = NULL;
	if ( TemplateOwnerClass != NULL )
	{
		// look in the class's map for a subobject with the saved name
		ComponentPtr = TemplateOwnerClass->ComponentNameToDefaultObjectMap.FindRef(ComponentName);
	}

	// if it wasn't in the map, then it doesn't exist
	UBOOL bResolvedUsingOldMethod = ComponentPtr != NULL;
	UBOOL bResolvedUsingNewMethod = Result != NULL;
	if ( bResolvedUsingOldMethod != bResolvedUsingNewMethod ||
		(bResolvedUsingNewMethod && Result != ComponentPtr) )
	{
		debugfSuppressed(NAME_DevComponents, TEXT("Mismatch between ResolveSourceDefaultObject result!"));
		debugfSuppressed(NAME_DevComponents, TEXT("Component: '%s'\r\nOldResult: '%s'\r\nNewResult: '%s'"),
			*GetFullName(),
			ComponentPtr  ? *ComponentPtr->GetFullName() : TEXT("NULL"),
			Result ? *Result->GetFullName() : TEXT("NULL")
			);
	}

	return Result;
}

/**
 * Copies the SourceDefaultObject onto our own memory to propagate any modified defaults
 *
 * @param Ar	The archive used to serialize the pointer to the subobject template
 */
void UComponent::PreSerialize(FArchive& Ar)
{
	// load or save the pointer to the template (via class and name of sub object)
	//@fixme components - this should no longer be necessary
	Ar << TemplateOwnerClass;

	// serialize the TemplateName if this component is a subobject of a class default object
	// or this isn't a persistent array or we're duplicating an object
	if ( IsTemplate(RF_ClassDefaultObject) || !Ar.IsPersistent() || (Ar.GetPortFlags() & PPF_Duplicate) != 0 )
	{
		Ar << TemplateName;
	}

	// if we are loading, we need to copy the template onto us
	// SourceDefaultActorClass is NULL if this is the templated object, or if the components
	// was created in the level package (ie created via en EditInlineNew operation in the prop
	// broswer). In either case, there is no need to copy defaults.

	//@fixme components - figure out if this is still needed for anything
	// RF_ZombieComponent is definitely not needed anymore, since components are no longer late-bound to their templates
	// all components now have a direct reference to their template
	if ( Ar.IsLoading() && Ar.IsPersistent() && (Ar.GetPortFlags() & PPF_Duplicate) == 0 )
	{
		if ( TemplateOwnerClass == NULL )
		{
			// this check is intended to fix components of actors contained within prefabs which have somehow lost their archetype pointers...
			if ( TemplateName == NAME_None && IsTemplate() )
			{
				UObject* SourceDefaultObject = ResolveSourceDefaultObject();
				if ( SourceDefaultObject != NULL )
				{
					// make sure the source object is fully serialized
					Ar.Preload(SourceDefaultObject);

					debugf(TEXT("%s: Restoring archetype reference to '%s'"), *GetFullName(), *SourceDefaultObject->GetPathName());
					SetArchetype(SourceDefaultObject, TRUE);

					if ( TemplateName != NAME_None )
					{
						MarkPackageDirty(TRUE);
					}
				}
			}
		}
		else
		{
			if ( GetArchetype() == GetClass()->GetDefaultObject() )
			{
				// first make sure that the template is fully serialized
				Ar.Preload(TemplateOwnerClass); // does this guarantee it's subojects are loaded?

				// hunt down the subobject pointed to by the class/name loaded above
				UObject* SourceDefaultObject = ResolveSourceDefaultObject();

				if (SourceDefaultObject == NULL)
				{
					// if we're compiling and TemplateOwnerClass is contained in the package being compiled (and we're not in the same package), this is a harmless error
					if ( !GIsUCCMake || ((TemplateOwnerClass->GetOuterUPackage()->PackageFlags&PKG_Compiling) == 0 && TemplateOwnerClass->GetOuterUPackage() != GetOutermost()) )
					{
						warnf( NAME_Warning, TEXT("%s Could not find source default object (%s:%s). Possibly deleted?"), *GetFullName(), *TemplateOwnerClass->GetName(), *TemplateName.ToString());
					}
					// mark those object to be destroyed after loading is complete (to not break serialization)
					SetFlags(RF_ZombieComponent);
					return;
				}

				if ( SourceDefaultObject != GetArchetype() )
				{
					// make sure the source object is fully serialized
					Ar.Preload(SourceDefaultObject);

					// if our source object changed classes on us, we can't do anything
					if (GetClass() != SourceDefaultObject->GetClass())
					{
						debugf(TEXT("The source default object (%s) isn't the same class as the instance (%s)"), *SourceDefaultObject->GetFullName(), *GetFullName());
						return;
					}

					MarkPackageDirty();
					SetArchetype(SourceDefaultObject, TRUE);
				}
			}
		}
	}
}

/**
 * Returns name to use for this component in component instancing maps.
 *
 * @return 	a name for this component which is unique within a single object graph.
 */
FName UComponent::GetInstanceMapName() const
{
	FName InstanceMapName = GetFName();
	if ( IsInstanced() )
	{
		InstanceMapName = TemplateName;
	}

	return InstanceMapName;
}

/**
 * Returns whether this component was instanced from a component template.
 *
 * @return	TRUE if this component was instanced from a template.  FALSE if this component was created manually at runtime.
 */	
UBOOL UComponent::IsInstanced() const
{
	UBOOL bResult = FALSE;

	// TemplateName should only be none if the component wasn't instanced
	if ( TemplateName != NAME_None )
	{
		// components whose Outer is a CDO should never be considered instanced
		bResult = !GetOuter()->HasAnyFlags(RF_ClassDefaultObject);
	}


	return bResult;
}

/**
 * Returns whether native properties are identical to the one of the passed in component.
 *
 * @param	Other	Other component to compare against
 *
 * @return TRUE if native properties are identical, FALSE otherwise
 */
UBOOL UComponent::AreNativePropertiesIdenticalTo( UComponent* Other ) const
{
	return TRUE;
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
UBOOL UComponent::GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, DWORD ExportFlags/*=0*/ ) const
{
	FString OwnerClassValue = TEXT("None");
	if ( TemplateOwnerClass != NULL )
	{
		UObject* StopOuter = NULL;

		// determine how the caller wants object values to be formatted
		if ( (ExportFlags&PPF_SimpleObjectText) != 0 )
		{
			StopOuter = GetOutermost();
		}

		// Value should be formatted like Class'Engine.PointLight' or Class'PointLight' (when using simple object text)
		OwnerClassValue = TemplateOwnerClass->GetClass()->GetName() + TEXT("'") + TemplateOwnerClass->GetPathName(StopOuter) + TEXT("'");
	}

	out_PropertyValues.Set(TEXT("TemplateOwnerClass"), *OwnerClassValue);
	out_PropertyValues.Set(TEXT("TemplateName"), *TemplateName.ToString());
	return TRUE;
}

UBOOL UComponent::IsPendingKill() const
{ 
	check(GetOuter()); 
	return HasAnyFlags( RF_PendingKill ) || GetOuter()->IsPendingKill(); 
}

/*-----------------------------------------------------------------------------
	FComponentInstanceParameters.
-----------------------------------------------------------------------------*/
/**
 * Fills this struct's ComponentMap with the components referenced by ComponentRoot which have an Outer of ComponentRoot.
 *
 * @param	bIncludeNestedComponents		controls whether nested components will be added to the component map
 *
 * @return	TRUE if the map was successfully populated
 */
UBOOL FComponentInstanceParameters::PopulateComponentMap( UBOOL bIncludeNestedComponents/*=FALSE*/ )
{
	UBOOL bResult = FALSE;
	if ( ComponentRoot != NULL )
	{
		ComponentMap.Empty();
		ComponentRoot->CollectComponents(ComponentMap, bIncludeNestedComponents);
		bResult = TRUE;
	}

	return bResult;
}

/*-----------------------------------------------------------------------------
	FObjectInstancingGraph.
-----------------------------------------------------------------------------*/
/** Default Constructor */
FObjectInstancingGraph::FObjectInstancingGraph()
: SourceRoot(NULL), DestinationRoot(NULL), InstanceFlags(0), bCreatingArchetype(FALSE), bUpdatingArchetype(FALSE)
, bEnableComponentInstancing(TRUE), bEnableObjectInstancing(TRUE), bLoadingObject(FALSE)
{
}

/**
 * Standard constructor
 *
 * @param	DestinationSubobjectRoot	the object that components are being instanced for
 * @param	SourceSubobjectRoot			the top-level object that is the source for the object construction; if unspecified, uses DestinationSubobjectRoot's
 *										ObjectArchetype
 */
FObjectInstancingGraph::FObjectInstancingGraph( UObject* DestinationSubobjectRoot, UObject* SourceSubobjectRoot/*=NULL*/ )
: SourceRoot(NULL), DestinationRoot(NULL), InstanceFlags(0), bCreatingArchetype(FALSE), bUpdatingArchetype(FALSE)
, bEnableComponentInstancing(TRUE), bEnableObjectInstancing(TRUE), bLoadingObject(FALSE)
{
	SetDestinationRoot(DestinationSubobjectRoot, SourceSubobjectRoot);
}

/**
 * Sets the DestinationRoot for this instancing graph.
 *
 * @param	DestinationSubobjectRoot	the top-level object that is being created
 * @param	SourceSubobjectRoot			the top-level object that is the source for the object construction; if unspecified, uses DestinationSubobjectRoot's
 *										ObjectArchetype
 */
void FObjectInstancingGraph::SetDestinationRoot( UObject* DestinationSubobjectRoot, UObject* SourceSubobjectRoot/*=NULL*/ )
{
	DestinationRoot = DestinationSubobjectRoot;
	check(DestinationRoot);

	if ( SourceSubobjectRoot == NULL )
	{
		SourceSubobjectRoot = DestinationRoot->GetArchetype();
	}

	SourceRoot = SourceSubobjectRoot;
	check(SourceRoot);

	// add the subobject roots to the Source -> Destination mapping
	SourceToDestinationMap.Set(SourceRoot, DestinationRoot);

	bCreatingArchetype = DestinationSubobjectRoot->HasAnyFlags(RF_ArchetypeObject);
	bUpdatingArchetype = bCreatingArchetype && (GUglyHackFlags&HACK_UpdateArchetypeFromInstance) != 0;
}

/**
 * Finds the object instance corresponding to the specified source object.
 *
 * @param	SourceObject			the object to find the corresponding instance for
 * @param	bSearchSourceObject		by default, this method searches for an instance corresponding to a source object; specifying TRUE for this parameter
 *									reverses the logic of the search and instead returns a source corresponding to the instance specified by the first parameter.
 */
UObject* FObjectInstancingGraph::GetDestinationObject( UObject* SourceObject, UBOOL bSearchSourceObjects/*=FALSE*/ )
{
	UObject* Result = NULL;
	if ( SourceObject != NULL )
	{
		if ( bSearchSourceObjects == FALSE )
		{
			Result = SourceToDestinationMap.FindRef(SourceObject);
		}
		else
		{
			UObject* const* pInstance = SourceToDestinationMap.FindKey(SourceObject);
			if ( pInstance != NULL )
			{
				Result = *pInstance;
			}
		}
	}

	return Result;
}

/**
 * Returns the component that has SourceComponent as its archetype.
 *
 * @param	SourceComponent		the component to find the corresponding component instance for
 * @param	CurrentValue		the component currently assigned as the value for the component property
 *								being instanced.  Used when updating archetypes to ensure that the new instanced component
 *								replaces the existing component instance in memory.
 * @param	CurrentObject		the object that owns the component property currently being instanced;  this is NOT necessarily the object
 *								that should be the Outer for the new component.
 *
 * @return	a component that has SourceComponent as its ObjectArchetype, or NULL if SourceComponent is not contained within
 *			SourceRoot.
 */
UComponent* FObjectInstancingGraph::GetInstancedComponent( UComponent* SourceComponent, UComponent* CurrentValue, UObject* CurrentObject )
{
	checkSlow(SourceComponent);

	UComponent* InstancedComponent = (UComponent*)INVALID_OBJECT;

	if ( SourceComponent != NULL && CurrentValue != NULL && IsComponentInstancingEnabled() )
	{
		if ( bUpdatingArchetype == FALSE && SourceComponent->IsBasedOnArchetype(CurrentValue) )
		{
			/*
			This block of code is intended to catch cases where the InitProperties() was called on DestinationRoot before SourceRoot
			had called PostLoad/InstanceComponents.  What happens in this case is that CurrentValue doesn't match SourceComponent;
			CurrentValue is actually an archetype of SourceComponent.  This seems to only happen with archetypes/prefabs, where e.g.

			1. PackageA is loaded, creating PrefabInstanceA, which causes PrefabA to be created
			2. PrefabInstanceA copies the properties from PrefabA, whose components are still the values inherited from PrefabA's archetype
				since PostLoad() hasn't yet been called on PrefabA.
			3. PostLoad() is called on PrefabA, which calls InstanceComponents and gives PrefabA its own copies of the components
			4. PostLoad() is called on PrefabInstanceA, and CurrentValue [for PrefabInstanceA] is still pointing to the inherited value
				from PrefabA's archetype.

			However, if we are updating a Prefab from a PrefabInstance, then SourceComponent->IsBasedOnArchetype(CurrentValue) will always return TRUE
			since SourceComponent will be the instance and CurrentValue will be its archetype.
			*/

			CurrentValue = SourceComponent;
		}

		UBOOL bShouldInstance = SourceComponent->IsIn(SourceRoot);
		if ( !bShouldInstance && CurrentValue->GetOuter() == CurrentObject->GetArchetype() )
		{
			// this code is intended to catch cases where SourceRoot contains subobjects assigned to instanced object properties, where the subobject's class
			// contains components, and the class of the subobject is outside of the inheritance hierarchy of the SourceRoot, for example, a weapon
			// class which contains UIObject subobject definitions in its defaultproperties, where the property referencing the UIObjects is marked instanced.
			bShouldInstance = TRUE;

			// if this case is triggered, ensure that the CurrentValue of the component property is still pointing to the template component.
			check(SourceComponent == CurrentValue);
		}

		if ( bShouldInstance == TRUE )
		{
			// search for the unique component instance that corresponds to this component template
			InstancedComponent = ComponentInstanceMap.FindRef(SourceComponent);
			if ( InstancedComponent == NULL )
			{
				// if the Outer for the component currently assigned to this property is the same as the object that we're instancing components for,
				// the component does not need to be instanced; otherwise, there are two possiblities:
				// 1. CurrentValue is a template and needs to be instanced
				// 2. CurrentValue is an instanced component, in which case it should already be in InstanceGraph, UNLESS the component was created
				//		at runtime (editinline export properties, for example).  If that is the case, CurrentValue will be an instance that is not linked
				//		to the component template referenced by CurrentObject's archetype, and in this case, we also don't want to re-instance the component template

				UBOOL bIsRuntimeInstance = CurrentValue != SourceComponent && CurrentValue->GetOuter() == CurrentObject && !IsUpdatingArchetype();
				if ( bIsRuntimeInstance )
				{
					InstancedComponent = CurrentValue;

					// if this component is instanced (that is, if its ObjectArchetype is a component template in a CDO or archetype), then it has the wrong archetype, as its
					// archetype should be SourceComponent, in which case we would have never gotten into this block to begin with because ComponentInstanceMap.FindRef(SourceComponent)
					// would have found it.  Fix those cases here
					if ( CurrentValue->IsInstanced() )
					{
						debugf(NAME_DevComponents, TEXT("Fixing archeytpe for component '%s'.  Current:%s  New:%s"), *CurrentValue->GetFullName(),
							*CurrentValue->GetArchetype()->GetPathName(), *SourceComponent->GetPathName());

						CurrentValue->MarkPackageDirty(TRUE);
						CurrentValue->SetArchetype(SourceComponent);

						// these values are used for linking instances to their templates, so update these now
						// if SourceComponent's Outer is a CDO, it means that SourceComponent was defined in class defaultproperties with the class as the outer
						// (as opposed to being a nested component instanced for some other subobject).  These types of components should have a valid value for
						// TemplateOwnerClass (though with the single component instancing method, I'm not sure that TemplateOwnerClass is actually even required any more)
						if ( SourceComponent->GetOuter()->HasAnyFlags(RF_ClassDefaultObject) )
						{
							CurrentValue->TemplateOwnerClass = SourceComponent->GetOuter()->GetClass();
						}
						else
						{
							CurrentValue->TemplateOwnerClass = SourceComponent->TemplateOwnerClass;
						}
						
						CurrentValue->TemplateName = SourceComponent->TemplateName;
					}
				}
				else
				{

					// UDistributions shouldn't be instanced in the game; only UDistributions that were saved to disk [because their values were different from
					// the component template] should ever be loaded
					// @fixme by making a ShouldInstanceOnClient/ShouldInstanceOnServer method that calls through to NeedsLoadForClient/NeedsLoadForServer by default,
					// then override that method in UDistributionFloat/DistributionVector to return false if !GIsEditor
					if (!GIsEditor && (CurrentValue->IsA(UDistributionFloat::StaticClass()) || CurrentValue->IsA(UDistributionVector::StaticClass())))
					{
						return NULL;
					}

					// If the component template is relevant in this context(client vs server vs editor), instance it.
					const UBOOL bShouldLoadForClient = SourceComponent->NeedsLoadForClient();
					const UBOOL bShouldLoadForServer = SourceComponent->NeedsLoadForServer();
					const UBOOL bComponentCreationDisabled = (GUglyHackFlags&HACK_DisableComponentCreation) != 0;
					const UBOOL bShouldLoadForEditor = ( GIsEditor && ( bShouldLoadForClient || !CurrentObject->IsInPIEPackage() ) );

					if ( !bComponentCreationDisabled && ((GIsClient && bShouldLoadForClient) || (GIsServer && bShouldLoadForServer) || bShouldLoadForEditor) )
					{
						// this is the first time the instance corresponding to SourceComponent has been requested

						// get the object instance corresponding to the source component's Outer - this is the object that
						// will be used as the Outer for the destination component
						UObject* ComponentOuter = SourceToDestinationMap.FindRef(SourceComponent->GetOuter());
						if ( ComponentOuter == NULL )
						{
							if ( bUpdatingArchetype == TRUE )
							{
								ComponentOuter = SourceComponent->GetOuter()->GetArchetype();
								check(ComponentOuter);

								// now add this object to the lookup map for faster searching in the future
								SourceToDestinationMap.Set(SourceComponent->GetOuter(), ComponentOuter);
							}
							else
							{
								checkf(ComponentOuter, TEXT("No corresponding destination object found for '%s' while attempting to instance component '%s'"), *SourceComponent->GetOuter()->GetFullName(), *SourceComponent->GetFullName());
							}
						}

						// determine what the new name for the component should be
						FName ComponentName = NAME_None;
						if ( IsUpdatingArchetype() )
						{
							if ( CurrentValue == SourceComponent )
							{
								// CurrentValue/SourceComponent = component contained by the actor instance
								// CurrentObject = actor instance's archetype

								// if the CurrentValue is the same as SourceComponent, this should indicate that the component that we're currently updating
								// was added to the actor instance at runtime (see UComponentProperty::InstanceComponents, @note A).  There are a couple of other
								// conditions which this implies that we should assert for (in case there is a different circumstance which might result in ending up here)
								check(SourceComponent->GetArchetype() == SourceComponent->GetClass()->GetDefaultObject());
								check(SourceComponent->TemplateName==NAME_None);
								check(ComponentOuter->IsTemplate(RF_ArchetypeObject));

								// now do the same thing as if we were just creating an archetype.
								ComponentName = SourceComponent->GetInstanceMapName();
							}
							else
							{
								// when updating an archetype component, we want to reconstruct the existing component so
								// we need to use the same name and outer as the component currently assigned as the value for
								// this component property
								ComponentName = CurrentValue->GetFName();

								// quick sanity check - if we are updating an archetype, the current value of the component is already an archetype, and its Outer should be the
								// same object that is mapped as the corresponding instance for the SourceComponent's Outer
								check(ComponentOuter == CurrentValue->GetOuter());
							}
						}
						else if ( IsCreatingArchetype(FALSE) )
						{
							// when we're creating an archetype, the name for the new component should match the name of the source component's
							// original template, so that it's easy to tell which components are related
							ComponentName = SourceComponent->GetInstanceMapName();
						}
						// this code must remain in place until the next package version bump
						else if ( GIsUCCMake == TRUE && ComponentOuter->IsTemplate(RF_ClassDefaultObject) )
						{
							// here we're creating a nested component inside a CDO - always use template names in this case
							ComponentName = SourceComponent->GetFName();
						}

						// Build the flags to use for new component.
						EObjectFlags NewComponentFlags = ComponentOuter->GetMaskedFlags(RF_PropagateToSubObjects);

						// if we are creating or updating an archetype, SourceComponent will be a component instance placed in a map
						// so we don't want SourceComponent to be set as the ObjectArchetype for the component we're creating here.  Remember the
						// ObjectArchetype of the component currently assigned to this component property; after we've created the new component, we'll
						// restore its ObjectArchetype pointer to this value
						UObject* FinalComponentArchetype = CurrentValue->GetArchetype();

						// finally, create the component instance
						InstancedComponent = ConstructObject<UComponent>(
							SourceComponent->GetClass(),
							ComponentOuter,
							ComponentName,
							NewComponentFlags,
							SourceComponent,
							DestinationRoot,
							this
							);

						// if appropriate, restore the ObjectArchetype to the old value
						if ( IsCreatingArchetype() )
						{
							// if the SourceRoot is a CDO or archetype object, then we're not creating an archetype from an instance - we're creating
							// an archetype straight from a CDO or another archetype, so we don't need to modify the new component's archetype
							if ( !SourceRoot->IsTemplate() )
							{
								InstancedComponent->SetArchetype(FinalComponentArchetype);
							}

							//@todo ronp - hmmm, we probably don't want to set the TemplateName for components created inside archetypes if the component isn't based on a template
							InstancedComponent->TemplateName = SourceComponent->GetInstanceMapName();
						}
						else if ( !SourceComponent->IsInstanced() && SourceComponent->IsTemplate(RF_ArchetypeObject) )
						{
							// When a component is created at runtime (via the "create new" button in the editor) and the component's outer is an archetype object,
							// the component is technically an archetype but its TemplateName will be NAME_None (because it wasn't instanced from a template itself)
							// here's where we handle that case - in this case we're instancing a component based on the component template contained within the archetype
							// object - the instance won't inherit a TemplateName from the archetype component (because the arc component doesn't have one) so we need to
							// set it manually
							InstancedComponent->TemplateName = SourceComponent->GetInstanceMapName();
						}

						// now add this component mapping
						AddComponentPair(SourceComponent,InstancedComponent);
					}
				}
			}
			else if ( IsLoadingObject() && InstancedComponent->GetClass()->HasAnyClassFlags(CLASS_HasComponents) )
			{
				/* When loading an object from disk, in some cases we have a component which has a reference to another component in DestinationObject which
					wasn't serialized and hasn't yet been instanced.  For example, the PointLight class declared two component templates:

						Begin DrawLightRadiusComponent0
						End
						Components.Add(DrawLightRadiusComponent0)

						Begin MyPointLightComponent
							SomeProperty=DrawLightRadiusComponent
						End
						LightComponent=MyPointLightComponent

					The components array will be processed by UClass::InstanceComponentTemplates after the LightComponent property is processed.  If the instance
					of DrawLightRadiusComponent0 that was created during the last session (i.e. when this object was saved) was identical to the component template
					from the PointLight class's defaultproperties, and the instance of MyPointLightComponent was serialized, then the MyPointLightComponent instance will
					exist in the InstanceGraph, but the instance of DrawLightRadiusComponent0 will not.  To handle this case and make sure that the SomeProperty variable of
					the MyPointLightComponent instance is correctly set to the value of the DrawLightRadiusComponent0 instance that will be created as a result of calling
					InstanceComponentTemplates on the PointLight actor from ConditionalPostLoad, we must call ConditionalPostLoad on each existing component instance that we
					encounter, while we still have access to all of the component instances owned by the PointLight.
				*/
				InstancedComponent->ConditionalPostLoadSubobjects(this);
			}
		}
		else
		{
			// SourceComponent might actually be the instanced component, in which case SourceComponent->IsIn(DestinationRoot) should return TRUE

			// else this is a component that is not contained within this object graph - it is an external component that is simply referenced by this graph
		}
	}

	return InstancedComponent;
}

/**
 * Adds an object instance to the map of source objects to their instances.  If there is already a mapping for this object, it will be replaced
 * and the value corresponding to ObjectInstance's archetype will now point to ObjectInstance.
 *
 * @param	ObjectInstance	the object instance to add a mapping for
 * @param	ObjectSource	the object that should be added as the key for the pair; if not specified, will use
 *							the ObjectArchetype of ObjectInstance
 */
void FObjectInstancingGraph::AddObjectPair( UObject* ObjectInstance, UObject* ObjectSource/*=NULL*/ )
{
	check(SourceRoot);
	check(DestinationRoot);

	if ( ObjectInstance != NULL )
	{
		UObject* SourceObject = ObjectSource;
		if ( SourceObject == NULL )
		{
			SourceObject = ObjectInstance->GetArchetype();
		}
		check(SourceObject);

		// Disabled, as there are many cases where an object will have several components with the same archetype that are not error cases (e.g., a StaticMeshCollectionActor)
		// This logging is also relatively expensive
#if 0 /* SUPPORT_SUPPRESSED_LOGGING */
		UObject* ExistingInstance = SourceToDestinationMap.FindRef(SourceObject);
		if ( ExistingInstance != NULL && ExistingInstance != ObjectInstance )
		{
			debugfSuppressed(NAME_DevComponents, TEXT("Changing instance mapped for '%s' from '%s' to '%s'"), *SourceObject->GetFullName(), *ExistingInstance->GetFullName(), *ObjectInstance->GetFullName());
		}
#endif
		SourceToDestinationMap.Set(SourceObject, ObjectInstance);
	}
}

/**
 * Adds a component template/instance pair directly to the tracking map.  Used only when pre-initializing a object instance graph, such as when loading
 * an existing object from disk.
 *
 * @param	ComponentTemplate	the component that should be added as the key for this pair
 * @param	ComponentInstance	the component instance corresponding to ComponentTemplate
 */
void FObjectInstancingGraph::AddComponentPair( UComponent* ComponentTemplate, UComponent* ComponentInstance )
{
	check(SourceRoot);
	check(DestinationRoot);

	if ( ComponentTemplate != NULL )
	{
		AddObjectPair(ComponentInstance, ComponentTemplate);

		// if we're instancing a component using the CDO as the source component, don't add it to the component map as this is used to contain
		// unique template -> unique instance mappings, and when instancing components from the default component template, that mapping is not unique.
		// the only exception to this is when we are instancing a component that we're inheriting from a parent class, while importing defaultproperties for the child class
		// in this case, the template -> instance pair IS unique, so we'll want to add it to the list so that it doesn't get re-instanced when CopyInheritedComponents is called
		UBOOL bIsDefaultComponentTemplate = ComponentTemplate->HasAnyFlags(RF_ClassDefaultObject);
		if ( !bIsDefaultComponentTemplate && GIsUCCMake && !ComponentTemplate->IsIn(SourceRoot) )
		{
			bIsDefaultComponentTemplate = ComponentInstance->IsTemplate(RF_ClassDefaultObject);
		}

		if ( !bIsDefaultComponentTemplate )
		{
			// Disabled, as there are many cases where an object will have several components with the same archetype that are not error cases (e.g., a StaticMeshCollectionActor)
			// This logging is also relatively expensive
#if 0 /* SUPPORT_SUPPRESSED_LOGGING */
			UComponent* ExistingInstance = ComponentInstanceMap.FindRef(ComponentTemplate);
			if (ExistingInstance != NULL && ExistingInstance != ComponentInstance )
			{
				debugfSuppressed(NAME_DevComponents, TEXT("Changing component instance mapped for '%s' from '%s' to '%s'"), *ComponentTemplate->GetFullName(), *ExistingInstance->GetFullName(), *ComponentInstance->GetFullName());
			}
#endif
			ComponentInstanceMap.Set(ComponentTemplate, ComponentInstance);
		}
		else
		{
			// nothing - can't add CDOs as the key in an instance map
			// this will really spam the log if we leave it in, so only enable it for debugging purposes
// 			debugfSlow(NAME_DevComponents, TEXT("Not adding component pair Source:%s Destination:%s"), *ComponentTemplate->GetPathName(), *ComponentInstance->GetFullName());
		}
	}
}

/**
 * Removes the specified component from the component source -> instance mapping.
 *
 * @param	SourceComponent		the component to remove from the mapping.
 */
void FObjectInstancingGraph::RemoveComponent( UComponent* SourceComponent )
{
	check(SourceRoot);
	check(DestinationRoot);

	if ( SourceComponent != NULL )
	{
		ComponentInstanceMap.Remove(SourceComponent);
	}
}

/**
 * Clears the mapping of component templates to component instances.
 */
void FObjectInstancingGraph::ClearComponentMap()
{
	ComponentInstanceMap.Empty();
}

/**
 * Retrieves a list of objects that have the specified Outer
 *
 * @param	SearchOuter		the object to retrieve object instances for
 * @param	out_Components	receives the list of objects contained by SearchOuter
 * @param	bIncludeNested	if FALSE, the output array will only contain objects that have SearchOuter as their Outer;
 *							if TRUE, the output array will contain objects that have SearchOuter anywhere in their Outer chain
 */
void FObjectInstancingGraph::RetrieveObjectInstances( UObject* SearchOuter, TArray<UObject*>& out_Objects, UBOOL bIncludeNested/*=FALSE*/ )
{
	if ( IsInitialized() && SearchOuter != NULL && (SearchOuter == DestinationRoot || SearchOuter->IsIn(DestinationRoot)) )
	{
		for ( TMap<UObject*,UObject*>::TIterator It(SourceToDestinationMap); It; ++It )
		{
			UObject* InstancedObject = It.Value();
			UBOOL bMatchesCriteria = bIncludeNested == TRUE
				? InstancedObject->IsIn(SearchOuter)
				: InstancedObject->GetOuter() == SearchOuter;

			if ( bMatchesCriteria == TRUE )
			{
				out_Objects.AddUniqueItem(InstancedObject);
			}
		}
	}
}

/**
 * Retrieves a list of components that have the specified Outer
 *
 * @param	SearchOuter		the object to retrieve components for
 * @param	out_Components	receives the list of components contained by SearchOuter
 * @param	bIncludeNested	if FALSE, the output array will only contain components that have SearchOuter as their Outer;
 *							if TRUE, the output array will contain components that have SearchOuter anywhere in their Outer chain
 */
void FObjectInstancingGraph::RetrieveComponents( UObject* SearchOuter, TArray<UComponent*>& out_Components, UBOOL bIncludeNested/*=FALSE*/ )
{
	if ( IsInitialized() && SearchOuter != NULL && (SearchOuter == DestinationRoot || SearchOuter->IsIn(DestinationRoot)) )
	{
		for ( TMap<UComponent*,UComponent*>::TIterator It(ComponentInstanceMap); It; ++It )
		{
			UComponent* InstancedComponent = It.Value();
			UBOOL bMatchesCriteria = bIncludeNested == TRUE
				? InstancedComponent->IsIn(SearchOuter)
				: InstancedComponent->GetOuter() == SearchOuter;

			if ( bMatchesCriteria == TRUE )
			{
				out_Components.AddUniqueItem(InstancedComponent);
			}
		}
	}
}

IMPLEMENT_CLASS(UComponent);
IMPLEMENT_CLASS(USubsystem);
IMPLEMENT_CLASS(UInterface);

