/**
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */



#ifndef __ASSETSELECTION_H__
#define __ASSETSELECTION_H__

#include "Core.h"
#include "UnSelection.h"


// forward decl.
class UActorFactory;

struct AssetMarshalDefs
{
	static const TCHAR AssetDelimiter = TEXT('|');
	static const TCHAR NameTypeDelimiter = TEXT(','); 
	static const TCHAR* FormatName() { return TEXT("UnrealEd/Assets"); }
};


/**
 * Caches relevant information about an asset in the game.
 */
struct FSelectedAssetInfo
{
public:
	/** the asset's class */
	UClass*		ObjectClass;

	/** the asset's complete path name (Package.Group.Group.Name, etc.) */
	FString		ObjectPathName;

	/** reference to the UObject instance for this asset; might be NULL if the asset isn't loaded */
	UObject*	Object;

	/** Constructor */
	FSelectedAssetInfo( const FString& ClassPathNameString )
		: ObjectClass( NULL ),
		  ObjectPathName(),
		  Object( NULL )
	{
		const TCHAR NameTypeDelimiter[] = { AssetMarshalDefs::NameTypeDelimiter, TEXT('\0') };

		FString ClassName;
		if ( ClassPathNameString.Split(NameTypeDelimiter, &ClassName, &ObjectPathName) )
		{
			ObjectClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
			Object = UObject::StaticFindObject(ObjectClass, ANY_PACKAGE, *ObjectPathName);
		}
	}
	FSelectedAssetInfo( UObject* InObject )
		: ObjectClass( NULL ),
		  ObjectPathName()
	{
		Object = InObject;
		if ( Object != NULL )
		{
			ObjectClass = Object->GetClass();
			ObjectPathName = Object->GetPathName();
		}
	}

	/**
	 * @return	the full name (ClassName PathName) for this asset item.
	 */
	FString GetAssetFullName() const
	{
		if ( IsValid(TRUE) )
		{
			return Object->GetFullName();
		}

		if ( IsValid(FALSE) )
		{
			return ObjectClass->GetName() + TEXT(" ") + ObjectPathName;
		}

		return GNone;
	}

	/**
	 * @return	the object name for this asset item
	 */
	FString GetAssetName() const
	{
		if ( IsValid(TRUE) )
		{
			return Object->GetName();
		}

		if ( IsValid(FALSE) )
		{
			INT pos = ObjectPathName.InStr(TEXT("."), TRUE);
			if ( pos == INDEX_NONE )
			{
				pos = ObjectPathName.InStr(SUBOBJECT_DELIMITER, TRUE);
			}

			if ( pos != INDEX_NONE )
			{
				return ObjectPathName.Mid(pos + 1);
			}
		}

		return GNone;
	}

	/**
	 * @return	TRUE if this refers to a valid asset (though not necessarily loaded)
	 */
	UBOOL IsValid( UBOOL bRequireObjectRef=FALSE ) const
	{
		return Object != NULL || (!bRequireObjectRef && ObjectClass != NULL && ObjectPathName.Len() > 0);
	}

	/**
	 * @return	TRUE if the passed in string is valid for parsing an asset.
	 */
	static UBOOL CanBeParsedFrom( const FString& ClassPathNameString )
	{
		const TCHAR NameTypeDelimiter[] = { AssetMarshalDefs::NameTypeDelimiter, TEXT('\0') };

		FString ClassName;
		FString	ObjectPathName;
		return ClassPathNameString.Split( NameTypeDelimiter, &ClassName, &ObjectPathName ); 
	}
};


/**
 * Custom USelection class designed for use by actor factory code.  Adds objects to a private selection set, but does not mark objects with RF_EdSelected
 */
class UActorFactorySelection : public USelection
{
public:
	DECLARE_CLASS_INTRINSIC(UActorFactorySelection,USelection,CLASS_Transient,UnrealEd);

	/**
	 * Adds an object to the private selection set.
	 * Does not affect object flags related to selection.  Does not send selection related notifications.
	 *
	 * @param	ObjectToSelect		the object to add to the selection set
	 */
	void AddToSelection( UObject* ObjectToSelect );

	/**
	 * Removes an object from this private selection set.
	 * Does not affect object flags related to selection.  Does not send selection related notifications.
	 *
	 * @param	ObjectToDeselect		the object to remove from the selection set
	 */
	void RemoveFromSelection( UObject* ObjectToDeselect );

	/**
	 * Replaces the currently selection set with a new list of objects.
	 * Does not affect object flags related to selection.  Does not send selection related notifications.
	 */
	void SetSelection( const TArray<UObject*>& InSelectionSet );

	/**
	 * Clears this private selection set, removing all objects from the list.
	 * Does not affect object flags related to selection.  Does not send selection related notifications.
	 */
	void ClearSelection();
};

class FActorFactoryAssetProxy
{
public:

	/**
	 * Accessor for retrieving the ActorFactoryAssetProxy's selection object.  Creates a new one if necessary.
	 */
	static UActorFactorySelection* GetActorFactorySelector();

	/**
	 * Builds a list of strings for populating the actor factory context menu items.  This menu is shown when
	 * the user right-clicks in a level viewport.
	 *
	 * @param	SelectedAssets			the list of loaded assets which are currently selected
	 * @param	OutQuickMenuItems		receives the list of strings to use for populating the actor factory context menu.
	 * @param	OutAdvancedMenuItems	receives the list of strings to use for populating the actor factory advanced context menu [All Templates].
	 * @param	OutSelectedMenuItems	receives the list of strings to use for populating the actor factory selected asset menu. Can be null if this menu is not used.
	 * @param	bRequireAsset			if TRUE, only factories that can create actors with the currently selected assets will be added
	 * @param	bCheckPlaceable			if TRUE, only bPlaceable factories will be added
	 */
	static void GenerateActorFactoryMenuItems( const TArray<FSelectedAssetInfo>& SelectedAssets, TArray<FString>& OutQuickMenuItems, TArray<FString>& OutAdvancedMenuItems, TArray<FString>* OutSelectedMenuItems, UBOOL bRequireAsset, UBOOL bCheckIfPlaceable, UBOOL bCheckInQuickMenu );

	/**
	 * Find the appropriate actor factory for an asset by type.
	 *
	 * @param	AssetData			contains information about an asset that to get a factory for
	 * @param	bRequireValidObject	indicates whether a valid asset object is required.  specify FALSE to allow the asset
	 *								class's CDO to be used in place of the asset if no asset is part of the drag-n-drop
	 *
	 * @return	the factory that is responsible for creating actors for the specified asset type.
	 */
	static UActorFactory* GetFactoryForAsset( const FSelectedAssetInfo& DropData, UBOOL bRequireValidObject=FALSE );

	/**
	 * Find the appropriate actor factory for an asset.
	 *
	 * @param	AssetObj	The asset that to find the appropriate actor factory for
	 *
	 * @return	The factory that is responsible for creating actors for the specified asset
	 */
	static UActorFactory* GetFactoryForAssetObject( UObject* AssetObj );

	/**
	 * Places an actor instance using the factory appropriate for the type of asset
	 *
	 * @param	AssetObj						the asset that is contained in the d&d operation
	 * @param	bUseSurfaceOrientation			specify TRUE to indicate that the factory should align the actor
	 *											instance with the target surface.
	 * @param	FactoryToUse					optional actor factory to use to create the actor; if not specified,
	 *											the highest priority factory that is valid will be used
	 *
	 * @return	the actor that was created by the factory, or NULL if there aren't any factories for this asset (or
	 *			the actor couldn't be created for some other reason)
	 */
	static AActor* AddActorForAsset( UObject* AssetObj, UBOOL bUseSurfaceOrientation, UActorFactory* FactoryToUse = NULL );

	/**
	 * Places an actor instance using the factory appropriate for the type of asset using the current object selection as the asset
	 *
	 * @param	ActorClass						The type of actor to create
	 * @param	bUseSurfaceOrientation			specify TRUE to indicate that the factory should align the actor
	 *											instance with the target surface.
	 * @param	FactoryToUse					optional actor factory to use to create the actor; if not specified,
	 *											the highest priority factory that is valid will be used
	 *
	 * @return	the actor that was created by the factory, or NULL if there aren't any factories for this asset (or
	 *			the actor couldn't be created for some other reason)
	 */
	static AActor* AddActorFromSelection( UClass* ActorClass, UBOOL bUseSurfaceOrientation, UActorFactory* ActorFactory = NULL );

	/**
	 * Determines if the provided actor is capable of having a material applied to it.
	 *
	 * @param	TargetActor	Actor to check for the validity of material application
	 *
	 * @return	TRUE if the actor is valid for material application; FALSE otherwise
	 */
	static UBOOL IsActorValidForMaterialApplication( AActor* TargetActor );

	/**
	 * Attempts to apply the material to the specified actor.
	 *
	 * @param	TargetActor		the actor to apply the material to
	 * @param	MaterialToApply	the material to apply to the actor
	 *
	 * @return	TRUE if the material was successfully applied to the actor
	 */
	static UBOOL ApplyMaterialToActor( AActor* TargetActor, UMaterialInterface* MaterialToApply );


private:
	/**
	 * Constructor
	 *
	 * Private, as this class is [currently] not intended to be instantiated
	 */
	FActorFactoryAssetProxy()
	{
	}

	/** Used by actor factories when placing assets which aren't necessarily  */
	static UActorFactorySelection* ActorFactorySelection;
};

#endif

// EOF

