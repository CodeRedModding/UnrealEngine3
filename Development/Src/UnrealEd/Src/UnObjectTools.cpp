/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnObjectTools.h"
#include "UnPackageTools.h"
#include "Factories.h"

#include "BusyCursor.h"
#include "DlgMoveAssets.h"
#include "DlgReferenceTree.h"
#include "EnginePhysicsClasses.h"
#include "EngineAudioDeviceClasses.h"
#include "EngineSoundClasses.h"
#include "PropertyWindow.h"
#include "ReferencedAssetsBrowser.h"
#include "UnEdTran.h"								// needed in order to call UTransactor::Reset
#include "PropertyWindowManager.h"					// required for access to GPropertyWindowManager
#include "FileHelpers.h"

#if WITH_SUBSTANCE_AIR
	#include "SubstanceAirTypedefs.h"
	#include "SubstanceAirPackage.h"
	#include "SubstanceAirTextureClasses.h"
	#include "SubstanceAirInstanceFactoryClasses.h"
	#include "SubstanceAirImageInputClasses.h"
	#include "SubstanceAirEdFactoryClasses.h"
#endif // WITH_SUBSTANCE_AIR

#if WITH_GFx
	#include "GFxUIEditor.h"
#endif

#if WITH_FBX
#include "UnFbxImporter.h"
#endif // WITH_FBX

#if WITH_MANAGED_CODE
	#include "PackageHelperFunctions.h"
#endif

/** Finds the language extension index of the passed in extension */
extern INT Localization_GetLanguageExtensionIndex(const TCHAR* Ext);

namespace ObjectTools
{

	/** Returns TRUE if the specified object can be displayed in a content browser */
	UBOOL IsObjectBrowsable( UObject* Obj, const TMap< UClass*, TArray< UGenericBrowserType* > >* InResourceTypes )	// const
	{
		UBOOL bIsSupported = FALSE;

		// @todo CB [reviewed; discuss]: Support object usage filters?
		const UBOOL bUsageFilterEnabled = FALSE;

		// Check object prerequisites
		const UBOOL bIsRedirector = (Obj->GetClass() == UObjectRedirector::StaticClass());
		const UBOOL bIsPackageObj = (Obj->GetClass() == UPackage::StaticClass());
		UPackage* ObjectPackage = Obj->GetOutermost();
		if( ObjectPackage != NULL && !bIsRedirector && !bIsPackageObj )
		{
			const UBOOL bUnreferencedObject = bUsageFilterEnabled && !Obj->HasAnyFlags(RF_TagExp);
			const UBOOL bIndirectlyReferencedObject = bUsageFilterEnabled && Obj->HasAnyFlags(RF_TagImp);
			if( ObjectPackage != UObject::GetTransientPackage()
				&& (ObjectPackage->PackageFlags & PKG_PlayInEditor) == 0
				&& !Obj->HasAnyFlags(RF_Transient)
				&& !Obj->IsPendingKill()
 				&& (Obj->HasAnyFlags(RF_Public) || ObjectPackage->ContainsMap())
				&& !(bUnreferencedObject || bIndirectlyReferencedObject)
				&& (bUsageFilterEnabled || (ObjectPackage->PackageFlags & PKG_Trash) == 0)
				// Don't display objects embedded in other objects (e.g. font textures, sequences)
				&& (Obj->GetOuter()->IsA(UPackage::StaticClass()))
				&& !Obj->HasAnyFlags( RF_ClassDefaultObject ) )
			{
				bIsSupported = TRUE;
			}
		}


		// Check browsable object types
		if( bIsSupported && InResourceTypes != NULL )
		{
			bIsSupported = FALSE;

			const TArray< UGenericBrowserType* >* AllowedTypeList = InResourceTypes->Find( Obj->GetClass() );
			if( AllowedTypeList != NULL )
			{
				for( INT CurBrowsableTypeIndex = 0; CurBrowsableTypeIndex < AllowedTypeList->Num(); ++CurBrowsableTypeIndex )
				{
					if( ( *AllowedTypeList )( CurBrowsableTypeIndex )->Supports( Obj ) )
					{
						bIsSupported = TRUE;
					}
				}
			}
		}

		return bIsSupported;
	}



	/** Creates dictionaries mapping object types to the actual classes associated with that type, and the reverse map */
	void CreateBrowsableObjectTypeMaps( TArray< UGenericBrowserType* >& OutBrowsableObjectTypeList,
										TMap< UGenericBrowserType*, TArray< UClass* > >& OutBrowsableObjectTypeToClassMap,
										TMap< UClass*, TArray< UGenericBrowserType* > >& OutBrowsableClassToObjectTypeMap )
	{
		// Create the list of available resource types.
		TArray< UGenericBrowserType* > ArchetypeGBTypes;
		for( TObjectIterator<UClass> ItC ; ItC ; ++ItC )
		{
			if( ItC->IsChildOf(UGenericBrowserType::StaticClass()) &&
				!ItC->HasAnyClassFlags(CLASS_Abstract) )
			{
				const UBOOL bIsAllType = (*ItC == UGenericBrowserType_All::StaticClass());
				const UBOOL bIsCustomType = (*ItC == UGenericBrowserType_Custom::StaticClass());

				// Ignore certain "fake" generic browser types (filters) that are legacy
				// @todo CB [reviewed; discuss]: We should support MaterialLackingPhysMat filter (etc) using a separate bit of meta data
				const UBOOL bIsUnwantedType =
					(*ItC == UGenericBrowserType_CurveEdPresetCurve::StaticClass() );
				
				if( !bIsAllType &&
					!bIsCustomType &&
					!bIsUnwantedType )
				{
					UGenericBrowserType* ResourceType = ConstructObject<UGenericBrowserType>( *ItC );
					if( ResourceType != NULL )
					{
						// Init the resource type object
						ResourceType->Init();

						if(ResourceType->GetBrowserTypeDescription().IsEmpty())
						{
							continue;
						}

						// Is this an Archetype GB type?  If so we'll handle it specially.
						if( Cast<UGenericBrowserType_Archetype>( ResourceType ) != NULL )
						{
							// Force the Archetype type to always be added last because we'll always prefer
							// another browser type's actions before this type's.
							ArchetypeGBTypes.AddItem( ResourceType );
						}
						else
						{
							// Find the best place to insert this item in our list.  We want child classes to be
							// inserted before their parents so that in cases where multiple GenericBrowserTypes support
							// the same classes we'll prefer to use the child type's functionality (context menu, etc.)
							UBOOL bWasInserted = FALSE;
							for( INT CurTypeIndex = 0; CurTypeIndex < OutBrowsableObjectTypeList.Num(); ++CurTypeIndex )
							{
								UGenericBrowserType* ExistingType = OutBrowsableObjectTypeList( CurTypeIndex );

								// The resource type we're adding a child of this existing type?
								if( ResourceType->GetClass()->IsChildOf( ExistingType->GetClass() ) )
								{
									// Insert it before this entry in our list!
									OutBrowsableObjectTypeList.InsertItem( ResourceType, CurTypeIndex );
									bWasInserted = TRUE;
									break;
								}
							}

							// Add it to the end of the list if we didn't find a parent class
							if( !bWasInserted )
							{
								OutBrowsableObjectTypeList.AddItem( ResourceType );
							}
						}
					}
				}
			}
		}


		// Also add the Archetype GB Type if we have one.  This should always be at the end of
		// the list as we'll always prefer a specific GB type's custom actions
		for( TArray< UGenericBrowserType* >::TIterator CurGBTypeIter( ArchetypeGBTypes ); CurGBTypeIter != NULL; ++CurGBTypeIter )
		{
			OutBrowsableObjectTypeList.AddItem( *CurGBTypeIter );
		}


		// Build the list of classes represented by each BrowsableObjectType
		for( TArray<UGenericBrowserType*>::TIterator ItBrowserType( OutBrowsableObjectTypeList ); ItBrowserType; ++ItBrowserType )
		{
			OutBrowsableObjectTypeToClassMap.Set(*ItBrowserType, TArray<UClass*>());
		}

		for( int CurBrowserTypeIdx = 0; CurBrowserTypeIdx < OutBrowsableObjectTypeList.Num() ; ++CurBrowserTypeIdx )
		{
			UGenericBrowserType* CurGenericBrowserType = OutBrowsableObjectTypeList( CurBrowserTypeIdx );

			TArray<UClass*>* SupportedChildClasses = OutBrowsableObjectTypeToClassMap.Find(CurGenericBrowserType);

			for( int CurSupportInfoIndex=0; CurSupportInfoIndex < CurGenericBrowserType->SupportInfo.Num(); ++CurSupportInfoIndex )
			{
				UClass* SupportedClass = CurGenericBrowserType->SupportInfo(CurSupportInfoIndex).Class;
				for( TObjectIterator<UClass> ItClass ; ItClass ; ++ItClass )
				{
					if ( *ItClass == SupportedClass || ItClass->IsChildOf(SupportedClass) )
					{
						SupportedChildClasses->AddItem(*ItClass);

						// Also update the reverse map
						{
							TArray< UGenericBrowserType* >* ObjectTypeList = OutBrowsableClassToObjectTypeMap.Find( *ItClass );
							if( ObjectTypeList == NULL )
							{
								ObjectTypeList = &OutBrowsableClassToObjectTypeMap.Set( *ItClass, TArray< UGenericBrowserType* >() );
							}

							// Add to list.  Note that order is still relevant here as we want child browser types
							// to appear after parents (the original order of the list)
							ObjectTypeList->AddUniqueItem( CurGenericBrowserType );
						}
					}
				}
			}
		}



		// @debug ObjectType Filter.
		//for( TArray<UGenericBrowserType*>::TIterator ItBrowserType(*BrowsableObjectTypes.Get()) ; ItBrowserType ; ++ItBrowserType )
		//{
		//	TArray<UClass*>* SupportedClassList = OutBrowsableObjectTypeToClassMap.Find(*ItBrowserType);
		//	warnf( TEXT("GenericBrowserType %s"), *(*ItBrowserType)->GetName() );
		//	for(int i=0; i<SupportedClassList->Num(); i++)
		//	{
		//		warnf( TEXT("\t\t%s"), *(*SupportedClassList)(i)->GetName() );
		//	}
		//}

	}



	/** 
	 * FArchiveTopLevelReferenceCollector constructor
	 * @todo: comment
	 */
	FArchiveTopLevelReferenceCollector::FArchiveTopLevelReferenceCollector(
		TArray<UObject*>* InObjectArray,
		const TArray<UObject*>& InIgnoreOuters,
		const TArray<UClass*>& InIgnoreClasses,
		const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes )
			:	ObjectArray( InObjectArray )
			,	IgnoreOuters( InIgnoreOuters )
			,	IgnoreClasses( InIgnoreClasses )
			,   BrowsableObjectTypes( InBrowsableObjectTypes )
	{
		// Mark objects.
		for ( FObjectIterator It ; It ; ++It )
		{
			if ( ShouldSearchForAssets(*It) )
			{
				It->SetFlags(RF_TagExp);
			}
			else
			{
				It->ClearFlags(RF_TagExp);
			}
		}
	}

	/** 
	 * UObject serialize operator implementation
	 *
	 * @param Object	reference to Object reference
	 * @return reference to instance of this class
	 */
	FArchive& FArchiveTopLevelReferenceCollector::operator<<( UObject*& Obj )
	{
		if ( Obj != NULL && Obj->HasAnyFlags(RF_TagExp) )
		{
			// Clear the search flag so we don't revisit objects
			Obj->ClearFlags(RF_TagExp);
			if ( Obj->IsA(UField::StaticClass()) )
			{
				// skip all of the other stuff because the serialization of UFields will quickly overflow
				// our stack given the number of temporary variables we create in the below code
				Obj->Serialize(*this);
			}
			else
			{
				// Only report this object reference if it supports display in a browser.
				// this eliminates all of the random objects like functions, properties, etc.
				const UBOOL bShouldReportAsset = ObjectTools::IsObjectBrowsable( Obj, BrowsableObjectTypes );
				if (Obj->IsValid())
				{
					if ( bShouldReportAsset )
					{
						ObjectArray->AddItem( Obj );
					}
					// Check this object for any potential object references.
					Obj->Serialize(*this);
				}
			}
		}
		return *this;
	}


	void FMoveInfo::Set(const TCHAR* InFullPackageName, const TCHAR* InNewObjName)
	{
		FullPackageName = InFullPackageName;
		NewObjName = InNewObjName;
		check( IsValid() );
	}
	
	/** @return		TRUE once valid (non-empty) move info exists. */
	UBOOL FMoveInfo::IsValid() const
	{
		return ( FullPackageName.Len() > 0 && NewObjName.Len() > 0 );
	}

	void GetReferencedTopLevelObjects(UObject* InObject, TArray<UObject*>& OutTopLevelRefs, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes )
	{
		OutTopLevelRefs.Empty();

		// Set up a list of classes to ignore.
		TArray<UClass*> IgnoreClasses;
		IgnoreClasses.AddItem(ULevel::StaticClass());
		IgnoreClasses.AddItem(UWorld::StaticClass());
		IgnoreClasses.AddItem(UPhysicalMaterial::StaticClass());

		// Set up a list of packages to ignore.
		TArray<UObject*> IgnoreOuters;
		IgnoreOuters.AddItem(FindObject<UPackage>(NULL,TEXT("EditorMaterials"),TRUE));
		IgnoreOuters.AddItem(FindObject<UPackage>(NULL,TEXT("EditorMeshes"),TRUE));
		IgnoreOuters.AddItem(FindObject<UPackage>(NULL,TEXT("EditorResources"),TRUE));
		IgnoreOuters.AddItem(FindObject<UPackage>(NULL,TEXT("EngineMaterials"),TRUE));
		IgnoreOuters.AddItem(FindObject<UPackage>(NULL,TEXT("EngineFonts"),TRUE));
		IgnoreOuters.AddItem(FindObject<UPackage>(NULL,TEXT("EngineResources"),TRUE));

		IgnoreOuters.AddItem(UObject::GetTransientPackage());

		// Add script packages to the IgnoreOuters list...
		// Get combined list of all script package names, including editor- only ones.
		TArray<FString> AllScriptPackageNames;
		appGetScriptPackageNames(AllScriptPackageNames, SPT_AllScript);
		for (INT ScriptNameIndex = 0; ScriptNameIndex < AllScriptPackageNames.Num(); ScriptNameIndex++)
		{
			IgnoreOuters.AddItem(FindObject<UPackage>(NULL,*(AllScriptPackageNames(ScriptNameIndex)),TRUE));
		}

		TArray<UObject*> PendingObjects;
		PendingObjects.AddItem( InObject );
		while ( PendingObjects.Num() > 0 )
		{
			// Pop the pending object and mark it for duplication.
			UObject* CurObject = PendingObjects.Pop();
			OutTopLevelRefs.AddItem( CurObject );

			// Collect asset references.
			TArray<UObject*> NewRefs;
			FArchiveTopLevelReferenceCollector Ar( &NewRefs, IgnoreOuters, IgnoreClasses, InBrowsableObjectTypes );
			CurObject->Serialize( Ar );

			// Now that the object has been serialized, add it to the list of IgnoreOuters for subsequent runs.
			IgnoreOuters.AddItem( CurObject );

			// Enqueue any referenced objects that haven't already been processed for top-level determination.
			for ( INT RefIdx = 0 ; RefIdx < NewRefs.Num() ; ++RefIdx )
			{
				UObject* NewRef = NewRefs(RefIdx);
				if ( !OutTopLevelRefs.ContainsItem(NewRef) )
				{
					PendingObjects.AddUniqueItem(NewRef);
				}
			}				
		}
	}

	/**
	 * Handles fully loading packages for a set of passed in objects.
	 *
	 * @param	Objects				Array of objects whose packages need to be fully loaded
	 * @param	OperationString		Localization key for a string describing the operation; appears in the warning string presented to the user.
	 * 
	 * @return TRUE if all packages where fully loaded, FALSE otherwise
	 */
	UBOOL HandleFullyLoadingPackages( const TArray<UObject*>& Objects, const TCHAR* OperationString )
	{
		// Get list of outermost packages.
		TArray<UPackage*> TopLevelPackages;
		for( INT ObjectIndex=0; ObjectIndex<Objects.Num(); ObjectIndex++ )
		{
			UObject* Object = Objects(ObjectIndex);
			if( Object )
			{
				TopLevelPackages.AddUniqueItem( Object->GetOutermost() );
			}
		}

		return PackageTools::HandleFullyLoadingPackages( TopLevelPackages, OperationString );
	}



	void DuplicateWithRefs( TArray<UObject*>& SelectedObjects, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes, TArray<UObject*>* OutObjects )
	{
		if ( SelectedObjects.Num() < 1 )
		{
			return;
		}

		// Present the user with a move dialog
		const FString DialogTitle( LocalizeUnrealEd("DuplicateWithReferences") );
		WxDlgMoveAssets MoveDialog;

		// The desired behavour for "Name Suffix?" checkbox is to default it to 
		// false, even when multiple objects are selected
		const UBOOL bEnableTreatNameAsSuffix = FALSE;
		MoveDialog.ConfigureNameField( bEnableTreatNameAsSuffix );

		FString PreviousPackage;
		FString PreviousGroup;
		FString PreviousName;
		UBOOL bPreviousNameAsSuffix = bEnableTreatNameAsSuffix;
		UBOOL bSawOKToAll = FALSE;
		UBOOL bSawSuccessfulDuplicate = FALSE;

		TArray<UPackage*> PackagesUserRefusedToFullyLoad;
		TArray<UPackage*> OutermostPackagesToSave;
		for( INT ObjectIndex = 0 ; ObjectIndex < SelectedObjects.Num() ; ++ObjectIndex )
		{
			UBOOL bSawDialogMessage = FALSE;
			UObject* Object = SelectedObjects(ObjectIndex);
			if( !Object )
			{
				continue;
			}

			// Initialize the move dialog with the previously entered pkg/grp, or the pkg/grp of the object to move.
			const FString DlgPackage = PreviousPackage.Len() ? PreviousPackage : Object->GetOutermost()->GetName();
			FString DlgGroup = PreviousGroup.Len() ? PreviousGroup : (Object->GetOuter()->GetOuter() ? Object->GetFullGroupName( 1 ) : TEXT(""));

			if( !bSawOKToAll )
			{
				bSawDialogMessage = TRUE;
				MoveDialog.SetTitle( *FString::Printf(TEXT("%s: %s"),*DialogTitle,*Object->GetPathName()) );
				const int MoveDialogResult = MoveDialog.ShowModal( DlgPackage, DlgGroup, ( bPreviousNameAsSuffix && PreviousName.Len() ? PreviousName : Object->GetName() ), TRUE );
				// Abort if the user cancelled.
				if ( MoveDialogResult != wxID_OK && MoveDialogResult != ID_OK_ALL)
				{
					return;
				}
				// Don't show the dialog again if "Ok to All" was selected.
				if ( MoveDialogResult == ID_OK_ALL )
				{
					bSawOKToAll = TRUE;
				}
				// Store the entered package/group/name for later retrieval.
				PreviousPackage = MoveDialog.GetNewPackage();
				PreviousGroup = MoveDialog.GetNewGroup();
				bPreviousNameAsSuffix = MoveDialog.GetNewName( PreviousName );
			}
			const FScopedBusyCursor BusyCursor;

			// Make a list of objects to duplicate.
			TArray<UObject*> ObjectsToDuplicate;

			// Include references?
			if ( MoveDialog.GetIncludeRefs() )
			{
				GetReferencedTopLevelObjects( Object, ObjectsToDuplicate, InBrowsableObjectTypes );
			}
			else
			{
				// Add just the object itself.
				ObjectsToDuplicate.AddItem( Object );
			}

			// Check validity of each reference dup name.
			FString ErrorMessage;
			FString Reason;
			FString ObjectsToOverwriteName;
			FString ObjectsToOverwritePackage;
			FString ObjectsToOverwriteClass;
			UBOOL	bUserDeclinedToFullyLoadPackage = FALSE;

			TArray<FMoveInfo> MoveInfos;
			for (INT RefObjectIndex = 0 ; RefObjectIndex < ObjectsToDuplicate.Num() ; ++RefObjectIndex )
			{
				const UObject *RefObject = ObjectsToDuplicate(RefObjectIndex);
				FMoveInfo* MoveInfo = new(MoveInfos) FMoveInfo;

				// Determine the target package.
				FString ClassPackage;
				FString ClassGroup;
				MoveDialog.DetermineClassPackageAndGroup( RefObject, ClassPackage, ClassGroup );

				FString TgtPackage;
				FString TgtGroup;
				// If a class-specific package was specified, use the class-specific package/group combo.
				if ( ClassPackage.Len() )
				{
					TgtPackage = ClassPackage;
					TgtGroup = ClassGroup;
				}
				else
				{
					// No class-specific package was specified, so use the 'universal' destination package.
					TgtPackage = MoveDialog.GetNewPackage();
					TgtGroup = ClassGroup.Len() ? ClassGroup : MoveDialog.GetNewGroup();
				}

				// Make sure that a traget package exists.
				if ( !TgtPackage.Len() )
				{
					ErrorMessage += TEXT("Invalid package name supplied\n");
				}
				else
				{
					// Make a new object name by concatenating the source object name with the optional name suffix.
					FString DlgName;
					FString BaseName = *RefObject->GetName();
					const UBOOL bTreatDlgNameAsSuffix = MoveDialog.GetNewName( DlgName );
					if(bTreatDlgNameAsSuffix)
					{
						// If we are appending a suffix check to see if it already has this suffix
						// and if so just increment a number on the end.
						INT SuffixPosition = BaseName.InStr(DlgName, TRUE);
						if(SuffixPosition != INDEX_NONE)
						{
							UBOOL bAlreadyHasSuffix = TRUE;

							// This loop checks to make sure that the copy of the suffix we found is actually
							// at the end.  We check to make sure that everything after the suffix we found is a digit.
							for( const TCHAR* Character = &BaseName[SuffixPosition+DlgName.Len()]; *Character; Character++ )
							{
								if(!appIsDigit(*Character))
								{
									bAlreadyHasSuffix = FALSE;
								}
							}

							if(bAlreadyHasSuffix)
							{
								INT OldValue = appAtoi(*BaseName.Mid(SuffixPosition+DlgName.Len()));

								DlgName = FString::Printf(TEXT("%s%d"), *DlgName, OldValue+1);

								// If it already has the suffix we need to remove the original suffix from the base string
								BaseName = BaseName.Mid(0, SuffixPosition);
							}
						}
					}

					FString ObjName;
					if (bTreatDlgNameAsSuffix)
					{
						ObjName = FString::Printf(TEXT("%s%s"), *BaseName, *DlgName);
					}
					else
					{
						// if we have selected OKToAll and haven't seen the Dialog Message
						// we need to get the object name from the base name since the dialog name
						// was never updated
						if (bSawOKToAll && !bSawDialogMessage)
						{
							ObjName = BaseName;
						}
						else
						{
							ObjName = DlgName;
						}
					}

					// Make a full path from the target package and group.
					const FString FullPackageName = TgtGroup.Len()
						? FString::Printf(TEXT("%s.%s"), *TgtPackage, *TgtGroup)
						: TgtPackage;

					// Make sure the packages being duplicated into are fully loaded.
					TArray<UPackage*> TopLevelPackages;
					UPackage* ExistingPackage = UObject::FindPackage(NULL, *FullPackageName);
					if( ExistingPackage )
					{
						TopLevelPackages.AddItem( ExistingPackage->GetOutermost() );
					}

					if( (ExistingPackage && PackagesUserRefusedToFullyLoad.ContainsItem(ExistingPackage)) ||
						!PackageTools::HandleFullyLoadingPackages( TopLevelPackages, TEXT("Duplicate") ) )
					{
						// HandleFullyLoadingPackages should never return FALSE for empty input.
						check( ExistingPackage );
						PackagesUserRefusedToFullyLoad.AddItem( ExistingPackage );
						bUserDeclinedToFullyLoadPackage = TRUE;
					}
					else
					{
						UObject* ExistingObject = ExistingPackage ? UObject::StaticFindObject(UObject::StaticClass(), ExistingPackage, *ObjName) : NULL;

						if( !ObjName.Len() )
						{
							ErrorMessage += TEXT("Invalid object name\n");
						}
						else if(!FIsValidObjectName( *ObjName,Reason ) 
							||	!FIsValidGroupName(*TgtPackage,Reason)
							||	!FIsValidGroupName(*TgtGroup,Reason,TRUE) )
						{
							// Make sure the object name is valid.
							ErrorMessage += FString::Printf(TEXT("    %s to %s.%s: %s\n"), *RefObject->GetPathName(), *FullPackageName, *ObjName, *Reason );
						}
						else if (ExistingObject == RefObject)
						{
							ErrorMessage += TEXT("Can't duplicate an object onto itself!\n");
						}
						else
						{
							// If the object already exists in this package with the given name, give the user 
							// the opportunity to overwrite the object. So, don't treat this as an error.
							if ( ExistingPackage && !FIsUniqueObjectName(*ObjName, ExistingPackage, Reason) )
							{
								ObjectsToOverwriteName += *ObjName;
								ObjectsToOverwritePackage += *FullPackageName;
								ObjectsToOverwriteClass += *ExistingObject->GetClass()->GetName();
							}

							// NOTE: Set the move info if this object already exists in-case the user wants to 
							// overwrite the existing asset. To overwrite the object, the move info is needed.

							// No errors!  Set asset move info.
							MoveInfo->Set( *FullPackageName, *ObjName );
						}
					}
				}
			}

			// User declined to fully load the target package; no need to display message box.
			if( bUserDeclinedToFullyLoadPackage )
			{
				continue;
			}

			// If any errors are present, display them and abort this object.
			if( ErrorMessage.Len() > 0 )
			{
				appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("CannotDuplicateList"), *Object->GetName(), *ErrorMessage) );
				continue;
			}

			// If there are objects that already exist with the same name, give the user the option to overwrite the 
			// object. This is useful if the user is duplicating objects with references that share the same references.
			if( ObjectsToOverwriteName.Len() > 0 )
			{
				UBOOL bOverwriteExistingObjects =
					appMsgf(
						AMT_YesNo,
						LocalizeSecure(
							LocalizeUnrealEd( "ReplaceExistingObjectInPackage_F" ),
							*ObjectsToOverwriteName,
							*ObjectsToOverwriteClass,
							*ObjectsToOverwritePackage ) );					

				// The user didn't want to overwrite the existing opitons, so bail out of the duplicate operation.
				if( !bOverwriteExistingObjects )
				{
					continue;
				}
			}

			// Duplicate each reference (including the main object itself)
			// And create ReplacementMap for replacing references.
			TArray<UObject*> Duplicates;
			TMap<UObject*, UObject*> ReplacementMap;

			for ( INT RefObjectIndex = 0 ; RefObjectIndex < ObjectsToDuplicate.Num() ; ++RefObjectIndex )
			{
				UObject *RefObject = ObjectsToDuplicate(RefObjectIndex);
				const FMoveInfo& MoveInfo = MoveInfos(RefObjectIndex);
				check( MoveInfo.IsValid() );

				const FString& PkgName = MoveInfo.FullPackageName;
				const FString& ObjName = MoveInfo.NewObjName;

				// @hack: Make sure the Outers of parts of PhysicsAssets are correct before trying to duplicate it.
				UPhysicsAsset* PhysicsAsset = Cast<UPhysicsAsset>(RefObject);
				if( PhysicsAsset )
				{
					PhysicsAsset->FixOuters();
				}

				// Make sure the referenced object is deselected before duplicating it.
				GEditor->GetSelectedObjects()->Deselect( RefObject );

				UObject* DupObject = NULL;

				UPackage* ExistingPackage = UObject::FindPackage(NULL, *PkgName);
				UObject* ExistingObject = ExistingPackage ? UObject::StaticFindObject(UObject::StaticClass(), ExistingPackage, *ObjName) : NULL;
				if (ExistingObject)
				{
					// This case should have been prevented by a earlier check and user friendly message
					check(ExistingObject != RefObject);
					// If we are overwriting an existing object, it may have rendering resources which are referenced by components in the scene.
					// We need to detach components before overwriting the object in this case.
					FGlobalComponentReattachContext ReattachContext;
					DupObject = GEditor->StaticDuplicateObject( RefObject, RefObject, UObject::CreatePackage(NULL,*PkgName), *ObjName );
				}
				else
				{
					DupObject = GEditor->StaticDuplicateObject( RefObject, RefObject, UObject::CreatePackage(NULL,*PkgName), *ObjName );
				}

				if( DupObject )
				{
					//If desired, maintain a list of objects we duplicate.
					if(OutObjects)
					{
						OutObjects->AddItem(DupObject);
					}
					Duplicates.AddItem( DupObject );
					ReplacementMap.Set( RefObject, DupObject );
					DupObject->MarkPackageDirty();
					OutermostPackagesToSave.AddUniqueItem( DupObject->GetOutermost() );
					bSawSuccessfulDuplicate = TRUE;

					// if the source object is in the MyLevel package and it's being duplicated into a content package, we need
					// to mark it RF_Standalone so that it will be saved (UWorld::CleanupWorld() clears this flag for all objects
					// inside the package)
					if (!RefObject->HasAnyFlags(RF_Standalone)
					&&	RefObject->GetOutermost()->ContainsMap()
					&&	!DupObject->GetOutermost()->ContainsMap() )
					{
						DupObject->SetFlags(RF_Standalone);
					}

					// Refresh content browser
					GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, DupObject));
				}

				GEditor->GetSelectedObjects()->Select( RefObject );
			}

			// Replace all references
			for (INT RefObjectIndex = 0 ; RefObjectIndex < ObjectsToDuplicate.Num() ; ++RefObjectIndex )
			{
				UObject* RefObject = ObjectsToDuplicate(RefObjectIndex);
				UObject* Duplicate = Duplicates(RefObjectIndex);

				FArchiveReplaceObjectRef<UObject> ReplaceAr( Duplicate, ReplacementMap, FALSE, TRUE, TRUE );
			}
		}

		// Update the browser if something was actually moved.
		if ( bSawSuccessfulDuplicate )
		{
			DWORD CallbackFlags = CBR_UpdatePackageList|CBR_UpdateAssetList;
			if ( MoveDialog.GetCheckoutPackages() )
			{
				PackageTools::CheckOutRootPackages(OutermostPackagesToSave);
				CallbackFlags |= CBR_UpdateSCCState;
			}
			if ( MoveDialog.GetSavePackages() )
			{
				PackageTools::SavePackages(OutermostPackagesToSave, TRUE);
			}

			FCallbackEventParameters Parms(NULL, CALLBACK_RefreshContentBrowser, CallbackFlags);
			GCallbackEvent->Send(Parms);
		}
	}
		
	/**
	 * Helper struct for passing multiple arrays to and from ForceReplaceReferences
	 */
	struct FForceReplaceInfo
	{
		// A list of packages which were dirtied as a result of a force replace
		TArray<UPackage*> DirtiedPackages;
		// Objects whose references were successfully replaced
		TArray<UObject*> ReplaceableObjects;
		// Objects whose references could not be successfully replaced
		TArray<UObject*> UnreplaceableObjects;
	};

	/**
	 * Forcefully replaces references to passed in objects
	 *
	 * @param ObjectToReplaceWith	Any references found to 'ObjectsToReplace' will be replaced with this object.  If the object is NULL references will be nulled.
	 * @param ObjectsToReplace		An array of objects that should be replaced with 'ObjectToReplaceWith'
	 * @param OutInfo				FForceReplaceInfo struct containing useful information about the result of the call to this function
	 * @param bWarnAboutRootSet		If True a message will be displayed to a user asking them if they would like to remove the rootset flag from objects which have it set.  
									If False, the message will not be displayed and rootset is automatically removed 
	 */
	static void ForceReplaceReferences( UObject* ObjectToReplaceWith, TArray<UObject*>& ObjectsToReplace, FForceReplaceInfo& OutInfo, UBOOL bWarnAboutRootSet = TRUE)				
	{
		GPropertyWindowManager->ClearAllThatContainObjects( ObjectsToReplace );

		TSet<UObject*> RootSetObjects;

		GWarn->StatusUpdatef( 0, 0, *LocalizeUnrealEd("ConsolidateAssetsUpdate_RootSetCheck") );

		// Iterate through all the objects to replace and see if they are in the root set.  If they are, offer to remove them from the root set.
		for ( TArray<UObject*>::TConstIterator ReplaceItr( ObjectsToReplace ); ReplaceItr; ++ReplaceItr )
		{
			UObject* CurObjToReplace = *ReplaceItr;
			if ( CurObjToReplace )
			{
				const UBOOL bFlaggedRootSet = CurObjToReplace->HasAnyFlags( RF_RootSet );
				if ( bFlaggedRootSet )
				{
					RootSetObjects.Add( CurObjToReplace );
				}
			}
		}
		if ( RootSetObjects.Num() )
		{
			if( bWarnAboutRootSet )
			{
				// Collect names of root set assets
				FString RootSetObjectNames;
				for ( TSet<UObject*>::TConstIterator RootSetIter( RootSetObjects ); RootSetIter; ++RootSetIter )
				{
					UObject* CurRootSetObject = *RootSetIter;
					RootSetObjectNames += CurRootSetObject->GetName() + TEXT("\n");
				}

				// Prompt the user to see if they'd like to remove the root set flag from the assets and attempt to replace them
				FString RootSetMessage = LocalizeUnrealEd("ConsolidateAssetsRootSetDlg_Msg") + RootSetObjectNames;
				WxLongChoiceDialog RootSetDlg(
					*RootSetMessage,
					*LocalizeUnrealEd("ConsolidateAssetsRootSetDlg_Title"),
					WxChoiceDialogBase::Choice( AMT_OK, LocalizeUnrealEd( TEXT("GenericDialog_Yes") ), WxChoiceDialogBase::DCT_DefaultAffirmative ),
					WxChoiceDialogBase::Choice( AMT_OKCancel, LocalizeUnrealEd( TEXT("GenericDialog_No") ), WxChoiceDialogBase::DCT_DefaultCancel ) );

				RootSetDlg.ShowModal();


				// The user elected to not remove the root set flag, so cancel the replacement
				if ( RootSetDlg.GetChoice().ReturnCode == AMT_OKCancel )
				{
					GWarn->EndSlowTask();
					return;
				}
			}

			for ( FObjectIterator ObjIter; ObjIter; ++ObjIter )
			{
				UObject* CurrentObject = *ObjIter;
				if ( CurrentObject )
				{
					// If the current object is one of the objects the user is attempting to replace but is marked RF_RootSet, strip the flag by removing it
					// from root
					if ( RootSetObjects.Find( CurrentObject ) )
					{
						CurrentObject->RemoveFromRoot();
					}
					// If the current object is inside one of the objects to replace but is marked RF_RootSet, strip the flag by removing it from root
					else
					{
						for( UObject* CurObjOuter = CurrentObject->GetOuter(); CurObjOuter; CurObjOuter = CurObjOuter->GetOuter() )
						{
							if ( RootSetObjects.Find( CurObjOuter ) )
							{
								CurrentObject->RemoveFromRoot();
								break;
							}
						}
					}
				}
			}
		}

		TMap<UObject*, INT> ObjToNumRefsMap;
		if( ObjectToReplaceWith != NULL )
		{
			GWarn->StatusUpdatef( 0, 0, *LocalizeUnrealEd("ConsolidateAssetsUpdate_CheckAssetValidity") );
			// Determine if the "object to replace with" has any references to any of the "objects to replace," if so, we don't
			// want to allow those objects to be replaced, as the object would end up referring to itself!
			// We can skip this check if "object to replace with" is NULL since it is not useful to check for null references
			FFindReferencersArchive FindRefsAr( ObjectToReplaceWith, ObjectsToReplace );
			FindRefsAr.GetReferenceCounts( ObjToNumRefsMap );
		}

		// Objects already loaded and in memory have to have any of their references to the objects to replace swapped with a reference to
		// the "object to replace with". FArchiveReplaceObjectRef can serve this purpose, but it expects a TMap of object to replace : object to replace with. 
		// Therefore, populate a map with all of the valid objects to replace as keys, with the object to replace with as the value for each one.
		TMap<UObject*, UObject*> ReplacementMap;
		for ( TArray<UObject*>::TConstIterator ReplaceItr( ObjectsToReplace ); ReplaceItr; ++ReplaceItr )
		{
			UObject* CurObjToReplace = *ReplaceItr;
			if ( CurObjToReplace )
			{				
				// If any of the objects to replace are marked RF_RootSet at this point, an error has occurred
				const UBOOL bFlaggedRootSet = CurObjToReplace->HasAnyFlags( RF_RootSet );
				check( !bFlaggedRootSet );

				// Exclude root packages from being replaced
				const UBOOL bRootPackage = ( CurObjToReplace->GetClass() == UPackage::StaticClass() ) && !( CurObjToReplace->GetOuter() );

				// Additionally exclude any objects that the "object to replace with" contains references to, in order to prevent the "object to replace with" from
				// referring to itself
				INT NumRefsInObjToReplaceWith = 0;
				INT* PtrToNumRefs = ObjToNumRefsMap.Find( CurObjToReplace );
				if ( PtrToNumRefs )
				{
					NumRefsInObjToReplaceWith = *PtrToNumRefs;
				}

				if ( !bRootPackage && NumRefsInObjToReplaceWith == 0 )
				{
					ReplacementMap.Set( CurObjToReplace, ObjectToReplaceWith );

					// Fully load the packages of objects to replace
					CurObjToReplace->GetOutermost()->FullyLoad();
				}
				// If an object is "unreplaceable" store it separately to warn the user about later
				else
				{
					OutInfo.UnreplaceableObjects.AddItem( CurObjToReplace );
				}
			}
		}

		GWarn->StatusUpdatef( 0, 0, *LocalizeUnrealEd("ConsolidateAssetsUpdate_FindingReferences") );

		ReplacementMap.GenerateKeyArray( OutInfo.ReplaceableObjects );

		// Find all the properties (and their corresponding objects) that refer to any of the objects to be replaced
		TMap< UObject*, TArray<UProperty*> > ReferencingPropertiesMap;
		for ( FObjectIterator ObjIter; ObjIter; ++ObjIter )
		{
			UObject* CurObject = *ObjIter;

			// Unless the "object to replace with" is null, ignore any of the objects to replace to themselves
			if ( ObjectToReplaceWith == NULL || !ReplacementMap.Find( CurObject ) )
			{
				// Find the referencers of the objects to be replaced
				FFindReferencersArchive FindRefsArchive( CurObject, OutInfo.ReplaceableObjects );

				// Inform the object referencing any of the objects to be replaced about the properties that are being forcefully
				// changed, and store both the object doing the referencing as well as the properties that were changed in a map (so that
				// we can correctly call PostEditChange later)
				TMap<UObject*, INT> CurNumReferencesMap;
				TMultiMap<UObject*, UProperty*> CurReferencingPropertiesMMap;
				if ( FindRefsArchive.GetReferenceCounts( CurNumReferencesMap, CurReferencingPropertiesMMap ) > 0  )
				{
					TArray<UProperty*> CurReferencedProperties;
					CurReferencingPropertiesMMap.GenerateValueArray( CurReferencedProperties );
					ReferencingPropertiesMap.Set( CurObject, CurReferencedProperties );
					for ( TArray<UProperty*>::TConstIterator RefPropIter( CurReferencedProperties ); RefPropIter; ++RefPropIter )
					{
						CurObject->PreEditChange( *RefPropIter );
					}
				}
			}
		}

		// Iterate over the map of referencing objects/changed properties, forcefully replacing the references and then
		// alerting the referencing objects the change has completed via PostEditChange
		INT NumObjsReplaced = 0;
		for ( TMap< UObject*, TArray<UProperty*> >::TConstIterator MapIter( ReferencingPropertiesMap ); MapIter; ++MapIter )
		{
			++NumObjsReplaced;
			GWarn->StatusUpdatef( NumObjsReplaced, ReferencingPropertiesMap.Num(), *LocalizeUnrealEd("ConsolidateAssetsUpdate_ReplacingReferences") );

			UObject* CurReplaceObj = MapIter.Key();
			const TArray<UProperty*>& RefPropArray = MapIter.Value();

			FArchiveReplaceObjectRef<UObject> ReplaceAr( CurReplaceObj, ReplacementMap, FALSE, TRUE, FALSE );

			for ( TArray<UProperty*>::TConstIterator RefPropIter( RefPropArray ); RefPropIter; ++RefPropIter )
			{
				FPropertyChangedEvent PropertyEvent(*RefPropIter);
				CurReplaceObj->PostEditChangeProperty( PropertyEvent );
			}
			CurReplaceObj->MarkPackageDirty();
			OutInfo.DirtiedPackages.AddUniqueItem( CurReplaceObj->GetOutermost() );
		}
	}

	/**
	 * Consolidates objects by replacing all references/uses of the provided "objects to consolidate" with references to the "object to consolidate to." This is
	 * useful for situations such as when a particular asset is duplicated in multiple places and it would be handy to allow all uses to point to one particular copy
	 * of the asset. When executed, the function first attempts to directly replace all relevant references located within objects that are already loaded and in memory.
	 * Next, it deletes the "objects to consolidate," leaving behind object redirectors to the "object to consolidate to" in their wake.
	 *
	 * @param	ObjectToConsolidateTo	Object to which all references of the "objects to consolidate" will instead refer to after this operation completes
	 * @param	ObjectsToConsolidate	Objects which all references of which will be replaced with references to the "object to consolidate to"; each will also be deleted
	 * @param	InResourceTypes			Resource/generic browser types associated with the "objects to consolidate"
	 *
	 * @note	This function performs NO type checking, by design. It is potentially dangerous to replace references of one type with another, so utilize caution.
	 * @note	The "objects to consolidate" are DELETED by this function.
	 *
	 * @todo	This function utilizes many existing FArchive-derived classes to perform its task. This could be greatly optimized with a new, custom FArchive-derivation
	 *			designed specifically with consolidation in mind.
	 *
	 * @return	Structure of consolidation results, specifying which packages were dirtied, which objects failed consolidation (if any), etc.
	 */
	FConsolidationResults ConsolidateObjects( UObject* ObjectToConsolidateTo, TArray<UObject*>& ObjectsToConsolidate, const TArray< UGenericBrowserType* >& InResourceTypes )
	{
		FConsolidationResults ConsolidationResults;

		// Ensure the consolidation is headed toward a valid object and this isn't occurring in game
		if ( ObjectToConsolidateTo && !GIsGame )
		{
			GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT("ConsolidateAssetsUpdate_Consolidating") ), TRUE );

			// Clear audio components to allow previewed sounds to be consolidated
			GEditor->ClearPreviewAudioComponents();

			// Clear thumbnail manager components
			GUnrealEd->GetThumbnailManager()->ClearComponents();

			// Keep track of which objects, if any, cannot be consolidated, in order to notify the user later
			TArray<UObject*> UnconsolidatableObjects;

			// Keep track of objects which became partially consolidated but couldn't be deleted for some reason;
			// these are critical failures, and the user needs to be alerted
			TArray<UObject*> CriticalFailureObjects;

			// Keep track of which packages the consolidate operation has dirtied so the user can be alerted to them
			// during a critical failure
			TArray<UPackage*> DirtiedPackages;

			// Keep track of root set objects so the user can be prompted about stripping the flag from them
			TSet<UObject*> RootSetObjects;

			// Track which generic browser types are affected by consolidation
			TArray<UGenericBrowserType*> UpdatedResources;

			// Scope the reattach context below to complete after object deletion and before garbage collection
			{
				// Replacing references inside already loaded objects could cause rendering issues, so globally detach all components from their scenes for now
				FGlobalComponentReattachContext ReattachContext;
				
				FForceReplaceInfo ReplaceInfo;
				ForceReplaceReferences( ObjectToConsolidateTo, ObjectsToConsolidate, ReplaceInfo );
				DirtiedPackages.Append( ReplaceInfo.DirtiedPackages );
				UnconsolidatableObjects.Append( ReplaceInfo.UnreplaceableObjects );

				// With all references to the objects to consolidate to eliminated from objects that are currently loaded, it should now be safe to delete
				// the objects to be consolidated themselves, leaving behind a redirector in their place to fix up objects that were not currently loaded at the time
				// of this operation.
				for ( TArray<UObject*>::TConstIterator ConsolIter( ReplaceInfo.ReplaceableObjects ); ConsolIter; ++ConsolIter )
				{
					GWarn->StatusUpdatef( ConsolIter.GetIndex(), ReplaceInfo.ReplaceableObjects.Num(), *LocalizeUnrealEd("ConsolidateAssetsUpdate_DeletingObjects") );

					UObject* CurObjToConsolidate = *ConsolIter;
					UObject* CurObjOuter = CurObjToConsolidate->GetOuter();
					UPackage* CurObjPackage = CurObjToConsolidate->GetOutermost();
					FName CurObjName = CurObjToConsolidate->GetFName();

					// Attempt to delete the object that was consolidated
					if ( DeleteSingleObject( CurObjToConsolidate, InResourceTypes ) )
					{
						// Create a redirector with the same name as the object that was consolidated
						UObjectRedirector* Redirector = Cast<UObjectRedirector>( UObject::StaticConstructObject( UObjectRedirector::StaticClass(), CurObjOuter, CurObjName, RF_Standalone | RF_Public ) );
						check( Redirector );

						// Set the redirector to redirect to the object to consolidate to
						Redirector->DestinationObject = ObjectToConsolidateTo;

						DirtiedPackages.AddUniqueItem( CurObjPackage );

						// Track which generic browser types were affected by the deletion
						for ( TArray<UGenericBrowserType*>::TConstIterator ResourceIter( InResourceTypes ); ResourceIter; ++ResourceIter )
						{
							UGenericBrowserType* CurGBT = *ResourceIter;
							if ( CurGBT->Supports( CurObjToConsolidate ) )
							{
								UpdatedResources.AddUniqueItem( CurGBT );
							}
						}
					}
					// If the object couldn't be deleted, store it in the array that will be used to show the user which objects had errors
					else
					{
						CriticalFailureObjects.AddItem( CurObjToConsolidate );
					}
				}
			}

			// Collect garbage to clean up after the deletions
			UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );
			
			// Inform the relevant generic browser types of the object deletions
			for ( TArray<UGenericBrowserType*>::TIterator UpdatedResourceIter( UpdatedResources ); UpdatedResourceIter; ++UpdatedResourceIter )
			{
				UGenericBrowserType* CurGBT = *UpdatedResourceIter;
				CurGBT->NotifyPostDeleteObject();
			}

			// Empty the provided array so it's not full of pointers to deleted objects
			ObjectsToConsolidate.Empty();

			GWarn->EndSlowTask();

			ConsolidationResults.DirtiedPackages = DirtiedPackages;
			ConsolidationResults.FailedConsolidationObjs = CriticalFailureObjects;
			ConsolidationResults.InvalidConsolidationObjs = UnconsolidatableObjects;

			// If some objects failed to consolidate, notify the user of the failed objects
			if ( UnconsolidatableObjects.Num() > 0 )
			{
				FString FailedObjectNames;
				for ( TArray<UObject*>::TConstIterator FailedIter( UnconsolidatableObjects ); FailedIter; ++FailedIter )
				{
					UObject* CurFailedObject = *FailedIter;
					FailedObjectNames += CurFailedObject->GetName() + TEXT("\n");
				}

				FString FailedConsolidationMessage = LocalizeUnrealEd("ConsolidateAssetsFailureDlg_Msg") + FailedObjectNames;

				WxLongChoiceDialog FailedConsolidationDlg(
					*FailedConsolidationMessage,
					*LocalizeUnrealEd("ConsolidateAssetsFailureDlg_Title"),
					WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd("OK"), WxChoiceDialogBase::DCT_DefaultCancel ) );

				FailedConsolidationDlg.ShowModal();
			}

			// Alert the user to critical object failure
			if ( CriticalFailureObjects.Num() > 0 )
			{
				FString CriticalFailedObjectNames;
				for ( TArray<UObject*>::TConstIterator FailedIter( CriticalFailureObjects ); FailedIter; ++FailedIter )
				{
					const UObject* CurFailedObject = *FailedIter;
					CriticalFailedObjectNames += CurFailedObject->GetName() + TEXT("\n");
				}

				FString DirtiedPackageNames;
				for ( TArray<UPackage*>::TConstIterator DirtyPkgIter( DirtiedPackages ); DirtyPkgIter; ++DirtyPkgIter )
				{
					const UPackage* CurDirtyPkg = *DirtyPkgIter;
					DirtiedPackageNames += CurDirtyPkg->GetName() + TEXT("\n");
				}

				FString CriticalFailureConsolMsg = 
					FString::Printf( LocalizeSecure( LocalizeUnrealEd("ConsolidateAssetsCriticalFailureDlg_Msg"), *CriticalFailedObjectNames, *DirtiedPackageNames  ) );

				WxLongChoiceDialog CriticalFailedConsolidationDlg(
					*CriticalFailureConsolMsg,
					*LocalizeUnrealEd("ConsolidateAssetsCriticalFailureDlg_Title"),
					WxChoiceDialogBase::Choice( AMT_OK, *LocalizeUnrealEd("OK"), WxChoiceDialogBase::DCT_DefaultCancel ) );

				CriticalFailedConsolidationDlg.ShowModal();
			}
		}

		return ConsolidationResults;
	}

	/**
	 * Copies references for selected generic browser objects to the clipboard.
	 */
	void CopyReferences( const TArray< UObject* >& SelectedObjects ) // const
	{
		FString Ref;
		for ( INT Index = 0 ; Index < SelectedObjects.Num() ; ++Index )
		{
			if( Ref.Len() )
			{
				Ref += LINE_TERMINATOR;
			}
			Ref += SelectedObjects(Index)->GetPathName();
		}

		appClipboardCopy( *Ref );
	}

	/**
	 * Show the referencers of a selected object
	 *
	 * @param SelectedObjects	Array of the currently selected objects; the referencers of the first object are shown
	 */
	void ShowReferencers( const TArray< UObject* >& SelectedObjects ) // const
	{
		if( SelectedObjects.Num() > 0 )
		{
			UObject* Object = SelectedObjects( 0 );
			if ( Object )
			{
				GEditor->GetSelectedObjects()->Deselect( Object );

				// Remove potential references from thumbnail manager preview components.
				GUnrealEd->GetThumbnailManager()->ClearComponents();

				if ( UObject::IsReferenced( Object,RF_Native | RF_Public ) )
				{
					FStringOutputDevice Ar;
					Object->OutputReferencers( Ar,FALSE );
					warnf( TEXT("%s"), *Ar );  // also print the objects to the log so you can actually utilize the data
					
					// Display a dialog containing all referencers; the dialog is designed to destroy itself upon being closed, so this
					// allocation is ok and not a memory leak
					WxModelessPrompt* ReferencerDlg = new WxModelessPrompt( Ar, LocalizeUnrealEd("ShowReferencers") );
					ReferencerDlg->Show();
				}
				else
				{
					appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("ObjectNotReferenced"), *Object->GetName()));
				}

				GEditor->GetSelectedObjects()->Select( Object );
			}
		}
	}

	/**
 	 * Displays a tree(currently) of all assets which reference the passed in object.  
	 *
	 * @param ObjectToGraph		The object to find references to.
	 * @param InBrowsableTypes	A mapping of classes to browsable types.  The tool only shows browsable types or actors
	 */
	void ShowReferenceGraph( UObject* ObjectToGraph, TMap<UClass*, TArray<UGenericBrowserType*> >& InBrowsableTypes )
	{
		WxReferenceTreeDialog::ShowReferenceTree( ObjectToGraph, InBrowsableTypes );
	}

	/**
	 * Displays all of the objects the passed in object references
	 *
	 * @param	Object	Object whose references should be displayed
	 * @param	bGenerateCollection If true, generate a collection 
	 */
	void ShowReferencedObjs( UObject* Object, UBOOL bGenerateCollection )
	{
		if( Object )
		{
			FString CollectionName;
			if( bGenerateCollection )
			{
#if WITH_MANAGED_CODE
				WxDlgGenericStringEntry NameDlg;
				//Present the rename dialog
				FString DefaultName = FString::Printf(TEXT("%s_%s"), *(Object->GetPathName()), *(LocalizeUnrealEd(TEXT("Resources"))));
				if (NameDlg.ShowModal(TEXT(""), TEXT("ResourceCollectionName"), *DefaultName) == wxID_OK)
				{
					CollectionName = NameDlg.GetEnteredString();
				}
#endif
			}

			GEditor->GetSelectedObjects()->Deselect( Object );

			// Remove potential references from thumbnail manager preview components.
			GUnrealEd->GetThumbnailManager()->ClearComponents();

			// Find references.
			TArray<UObject*> ReferencedObjects;
			{
				const FScopedBusyCursor BusyCursor;
				TArray<UClass*> IgnoreClasses;
				TArray<FString> IgnorePackageNames;
				TArray<UObject*> IgnorePackages;

				// Assemble an ignore list.
				IgnoreClasses.AddItem( ULevel::StaticClass() );
				IgnoreClasses.AddItem( UWorld::StaticClass() );
				IgnoreClasses.AddItem( UPhysicalMaterial::StaticClass() );

				IgnorePackageNames.AddItem( FString(TEXT("EditorMaterials")) );
				IgnorePackageNames.AddItem( FString(TEXT("EditorMeshes")) );
				IgnorePackageNames.AddItem( FString(TEXT("EditorResources")) );
				IgnorePackageNames.AddItem( FString(TEXT("EngineMaterials")) );
				IgnorePackageNames.AddItem( FString(TEXT("EngineFonts")) );
				IgnorePackageNames.AddItem( FString(TEXT("EngineResources")) );

				// Construct the ignore package list.
				for( INT PackageNameItr = 0; PackageNameItr < IgnorePackageNames.Num(); ++PackageNameItr )
				{
					UObject* PackageToIgnore = FindObject<UPackage>(NULL,*(IgnorePackageNames(PackageNameItr)),TRUE);

					if( PackageToIgnore == NULL )
					{// An invalid package name was provided.
						debugf( TEXT("Package to ignore \"%s\" in the list of referenced objects is NULL and should be removed from the list"), *(IgnorePackageNames(PackageNameItr)) );
					}
					else
					{
						IgnorePackages.AddItem(PackageToIgnore);
					}
				}

				WxReferencedAssetsBrowser::BuildAssetList( Object, IgnoreClasses, IgnorePackages, ReferencedObjects );
			}

			const INT NumReferencedObjects = ReferencedObjects.Num();
			
			// Make sure that the only referenced object (if there's only one) isn't the object itself before outputting object references
			if ( NumReferencedObjects > 1 || ( NumReferencedObjects == 1 && !ReferencedObjects.ContainsItem( Object ) ) )
			{
				if (CollectionName.Len() == 0)
				{
					FString OutString( FString::Printf( TEXT("\nObjects referenced by %s:\r\n"), *Object->GetFullName() ) );
					for ( INT ObjectIndex = 0 ; ObjectIndex < ReferencedObjects.Num() ; ++ObjectIndex )
					{
						const UObject *ReferencedObject = ReferencedObjects( ObjectIndex );
						// Don't list an object as referring to itself.
						if ( ReferencedObject != Object )
						{
							OutString += FString::Printf( TEXT("\t%s:\r\n"), *ReferencedObject->GetFullName() );
						}
					}

					warnf( TEXT("%s"), *OutString );

					// Display the object references in a copy-friendly dialog; the dialog is designed to destroy itself upon being closed, so this
					// allocation is ok and not a memory leak
					WxModelessPrompt* ReferencesDlg = new WxModelessPrompt( OutString, LocalizeUnrealEd("ShowReferences") );
					ReferencesDlg->Show();
				}
				else
				{
#if WITH_MANAGED_CODE
					TArray<FString> ObjectsToAdd;
					for (INT RefIdx = 0; RefIdx < ReferencedObjects.Num(); RefIdx++)
					{
						UObject* RefObj = ReferencedObjects(RefIdx);
						if (RefObj != NULL)
						{
							if (RefObj != Object)
							{
								ObjectsToAdd.AddItem(RefObj->GetFullName());
							}
						}
					}

					if (ObjectsToAdd.Num() > 0)
					{
						FGADHelper* GADHelper = new FGADHelper();
						if (GADHelper->Initialize() == TRUE)
						{
							GADHelper->ClearCollection(CollectionName, EGADCollection::Private);
							GADHelper->SetCollection(CollectionName, EGADCollection::Private, ObjectsToAdd);
							GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdateCollectionList|CBR_UpdateCollectionListUI, NULL));
						}
					}
#endif
				}
			}
			else
			{
				appMsgf( AMT_OK, LocalizeSecure( LocalizeUnrealEd("ObjectNoReferences"), *Object->GetName() ) );
			}

			GEditor->GetSelectedObjects()->Select( Object );
		}
	}

	/**
	 * Select the object referencers in the level
	 *
	 * @param	Object			Object whose references are to be selected
	 *
	 */
	void SelectActorsInLevelDirectlyReferencingObject( UObject* RefObj )
	{
		UPackage* Package = Cast<UPackage>(RefObj->GetOutermost());
		if (Package && ((Package->PackageFlags & PKG_ContainsMap) != 0))
		{
			// Walk the chain of outers to find the object that is 'in' the level...
			UObject* ObjToSelect = NULL;
			UObject* CurrObject = RefObj;
			UObject* Outer = RefObj->GetOuter();
			while ((ObjToSelect == NULL) && (Outer != NULL) && (Outer != Package))
			{
				ULevel* Level = Cast<ULevel>(Outer);
				if (Level)
				{
					// We found it!
					ObjToSelect = CurrObject;
				}
				else
				{
					UObject* TempObject = Outer;
					Outer = Outer->GetOuter();
					CurrObject = TempObject;
				}
			}

			if (ObjToSelect)
			{
				AActor* ActorToSelect = Cast<AActor>(ObjToSelect);
				if (ActorToSelect)
				{
					GEditor->SelectActor( ActorToSelect, TRUE, NULL, TRUE );
				}
			}
		}
	}

	/**
	 * Select the object and it's external referencers' referencers in the level.
	 * This function calls AccumulateObjectReferencersForObjectRecursive to
	 * recursively build a list of objects to check for referencers in the level
	 *
	 * @param	Object			Object whose references are to be selected
	 *
	 */
	void SelectObjectAndExternalReferencersInLevel( UObject* Object )
	{
		if(Object)
		{
			if(UObject::IsReferenced(Object,RF_Native | RF_Public))
			{
				TArray<UObject*> ObjectsToSelect;

				GEditor->SelectNone( TRUE, TRUE );
				
				// Generate the list of objects.  This function is necessary if the object
				//	in question is indirectly referenced by an actor.  For example, a
				//	material used on a static mesh that is instanced in the level
				AccumulateObjectReferencersForObjectRecursive( Object, ObjectsToSelect );

				// Select the objects in the world
				for ( TArray<UObject*>::TConstIterator ObjToSelectItr( ObjectsToSelect ); ObjToSelectItr; ++ObjToSelectItr )
				{
					UObject* ObjToSelect = *ObjToSelectItr;
					SelectActorsInLevelDirectlyReferencingObject(ObjToSelect);
				}

				GEditor->GetSelectedObjects()->Select( Object );
			}
			else
			{
				appMsgf(AMT_OK, LocalizeSecure(LocalizeUnrealEd("ObjectNotReferenced"), *Object->GetName()));
			}
		}
	}


	/**
	 * Recursively add the objects referencers to a single array
	 *
	 * @param	Object			Object whose references are to be selected
	 * @param	Referencers		Array of objects being referenced in level
	 *
	 */
	void AccumulateObjectReferencersForObjectRecursive( UObject* Object, TArray<UObject*>& Referencers )
	{
		TArray<FReferencerInformation> OutInternalReferencers;
		TArray<FReferencerInformation> OutExternalReferencers;
		Object->RetrieveReferencers(&OutInternalReferencers, &OutExternalReferencers, TRUE);

		// dump the referencers
		for (INT ExtIndex = 0; ExtIndex < OutExternalReferencers.Num(); ExtIndex++)
		{
			UObject* RefdObject = OutExternalReferencers(ExtIndex).Referencer;
			if (RefdObject)
			{
				Referencers.Push( RefdObject );
				// Recursively search for static meshes and materials so that textures and materials will recurse back
				// to the meshes in which they are used
				if	( !(Object->IsA(UStaticMesh::StaticClass()) ) // Added this check for safety in case of a circular reference
					&& (	(RefdObject->IsA(UStaticMesh::StaticClass())) 
						||	(RefdObject->IsA(UMaterial::StaticClass())) 
						||	(RefdObject->IsA(UMaterialInstance::StaticClass()))
						)
					)
				{
					AccumulateObjectReferencersForObjectRecursive( RefdObject, Referencers );
				}
			}
		}
	}


	/**
	 * Deletes the list of objects
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 * @param	InResourceTypes		the resource types that are associated with the objects being deleted
	 *
	 * @return The number of objects successfully deleted
	 */
	INT DeleteObjects( const TArray< UObject* >& ObjectsToDelete, const TArray< UGenericBrowserType* >& InResourceTypes )
	{
		// Allows deleting of sounds after they have been previewed
		GEditor->ClearPreviewAudioComponents();

		const FScopedBusyCursor BusyCursor;

		// Make sure packages being saved are fully loaded.
		if( !HandleFullyLoadingPackages( ObjectsToDelete, TEXT("Delete") ) )
		{
			return 0;
		}

		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("Deleting")), TRUE );

		TArray<UGenericBrowserType*> UpdatedResources;
		TArray<UObject*> ObjectsDeletedSuccessfully;

		UBOOL bSawSuccessfulDelete = FALSE;

		for (INT Index = 0; Index < ObjectsToDelete.Num(); Index++)
		{
			GWarn->StatusUpdatef( Index, ObjectsToDelete.Num(), *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Deletingf"), Index, ObjectsToDelete.Num()) ) );
			UObject* ObjectToDelete = ObjectsToDelete(Index);

			if ( DeleteSingleObject( ObjectToDelete, InResourceTypes ) )
			{
				bSawSuccessfulDelete = TRUE;
				ObjectsDeletedSuccessfully.Push( ObjectToDelete );
				
				for ( INT ResourceIndex = 0; ResourceIndex < InResourceTypes.Num(); ResourceIndex++ )
				{
					UGenericBrowserType* gbt = InResourceTypes(ResourceIndex);
					if ( gbt->Supports(ObjectToDelete) )
					{
						UpdatedResources.AddUniqueItem( gbt );
					}
				}
			}
		}

		GWarn->EndSlowTask();

		// Update the browser if something was actually deleted.
		if ( bSawSuccessfulDelete )
		{
			// Collect garbage.
			UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

			// Call NotifyPostDeleteObject on all deleted resource types *after* GC
			for( INT Index = 0; Index < UpdatedResources.Num(); Index++ )
			{
				UGenericBrowserType* gbt = UpdatedResources( Index );
				gbt->NotifyPostDeleteObject();
			}
		}
		
		return ObjectsDeletedSuccessfully.Num();
	}


	/**
	 * Delete a single object
	 *
	 * @param	ObjectToDelete		The object to delete
	 * @param	InResourceTypes		Resource types that are associated with the objects being deleted
	 *
	 * @return If the object was successfully
	 */
	UBOOL DeleteSingleObject( UObject* ObjectToDelete, const TArray< UGenericBrowserType* >& InResourceTypes )
	{
		// Give the generic browser type and opportunity to abort the delete.
		UBOOL bDeleteAborted = FALSE;
		UBOOL bDeleteSuccessful = FALSE;
		for ( INT ResourceIndex = 0; ResourceIndex < InResourceTypes.Num(); ResourceIndex++ )
		{
			UGenericBrowserType* gbt = InResourceTypes(ResourceIndex);
			if ( gbt->Supports(ObjectToDelete) && !gbt->NotifyPreDeleteObject(ObjectToDelete) )
			{
				bDeleteAborted = TRUE;
				break;
			}
		}

		if ( !bDeleteAborted )
		{
			GEditor->GetSelectedObjects()->Deselect( ObjectToDelete );

			// Remove potential references to to-be deleted objects from thumbnail manager preview components.
			GUnrealEd->GetThumbnailManager()->ClearComponents();

			// this archive will contain the debug output in case the object is referenced
			FStringOutputDevice Ar;

			// Check and see whether we are referenced by any objects that won't be garbage collected. 
			UBOOL bIsReferenced = UObject::IsReferenced( ObjectToDelete, GARBAGE_COLLECTION_KEEPFLAGS );
			if ( bIsReferenced )
			{
				// determine whether the transaction buffer is the only thing holding a reference to the object
				// and if so, offer the user the option to reset the transaction buffer.
				GEditor->Trans->DisableObjectSerialization();
				bIsReferenced = UObject::IsReferenced(ObjectToDelete, GARBAGE_COLLECTION_KEEPFLAGS);
				GEditor->Trans->EnableObjectSerialization();

				// only ref to this object is the transaction buffer - let the user choose whether to clear the undo buffer
				if ( !bIsReferenced )
				{
					if ( appMsgf(AMT_YesNo, *LocalizeUnrealEd(TEXT("ResetUndoBufferForObjectDeletionPrompt"))) )
					{
						GEditor->Trans->Reset(*LocalizeUnrealEd(TEXT("DeleteSelectedItem")));
					}
					else
					{
						bIsReferenced = TRUE;
					}
				}

				if ( bIsReferenced )
				{
					// We cannot safely delete this object. Print out a list of objects referencing this one
					// that prevent us from being able to delete it.
					FReferencerInformationList Refs;
					ObjectToDelete->OutputReferencers(Ar, FALSE, &Refs);
				}
			}

			if ( bIsReferenced )
			{
				// We cannot safely delete this object. Print out a list of objects referencing this one
				// that prevent us from being able to delete it.
				FStringOutputDevice Ar;
				ObjectToDelete->OutputReferencers(Ar,FALSE);
				appMsgf(AMT_OK,LocalizeSecure(LocalizeUnrealEd("Error_InUse"), *ObjectToDelete->GetFullName(), *Ar));

				// Reselect the object as it failed to be deleted
				GEditor->GetSelectedObjects()->Select( ObjectToDelete );
			}
			else
			{
				bDeleteSuccessful = TRUE;

				// Mark its package as dirty as we're going to delete it.
				ObjectToDelete->MarkPackageDirty();

				// Remove standalone flag so garbage collection can delete the object.
				ObjectToDelete->ClearFlags( RF_Standalone );

				// notify the content browser
				GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectDeleted, ObjectToDelete));
			}
		}

		return bDeleteSuccessful;
	}

	/**
	 * Forcefully deletes the passed in list of objects.  Attempts to NULL out references to these objects to allow them to be deleted
	 *
	 * @param	ObjectsToDelete		The list of objects to delete
	 * @param	InResourceTypes		Resource types that are associated with the objects being deleted
	 *
	 * @return The number of objects successfully deleted
	 */
	INT ForceDeleteObjects( const TArray< UObject* >& InObjectsToDelete, const TArray< UGenericBrowserType* >& InResourceTypes )
	{
		INT NumDeletedObjects = 0;
		UBOOL ForceDeleteAll = FALSE;

		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("Deleting")), TRUE );

		TArray<UObject*> ObjectsToDelete;
	
		// Clear audio components to allow previewed sounds to be consolidated
		GEditor->ClearPreviewAudioComponents();

		// Remove potential references to to-be deleted objects from thumbnail manager preview components.
		GUnrealEd->GetThumbnailManager()->ClearComponents();

		for ( TArray<UObject*>::TConstIterator ObjectItr(InObjectsToDelete); ObjectItr; ++ObjectItr )
		{
			UObject* CurrentObject = *ObjectItr;

			GEditor->GetSelectedObjects()->Deselect( CurrentObject );

			if( !ForceDeleteAll )
			{
				// Check and see whether we are referenced by any objects that won't be garbage collected. 
				UBOOL bIsReferenced = UObject::IsReferenced( CurrentObject, GARBAGE_COLLECTION_KEEPFLAGS );

				FReferencerInformationList Refs;

				if ( bIsReferenced )
				{
					// Create a list of objects referencing this one that prevent us from being able to safely delete it.
					FStringOutputDevice Ar;

					// Get a list of the objects to delete
					CurrentObject->OutputReferencers(Ar,FALSE, &Refs);

					// Create a string list of all referenced properties.
					// Check if this object is referenced in default properties
					FString RefObjNames;
					FString DefaultPropertiesObjNames;
					ComposeStringOfReferencingObjects( Refs.ExternalReferences, RefObjNames, DefaultPropertiesObjNames );
					ComposeStringOfReferencingObjects( Refs.InternalReferences, RefObjNames, DefaultPropertiesObjNames );


					INT YesNoCancelReply = appMsgf( AMT_YesNoYesAllNoAll, LocalizeSecure(LocalizeQuery(TEXT("Warning_ForceDelete"),TEXT("Core")), *CurrentObject->GetName(), *RefObjNames) );
					switch ( YesNoCancelReply )
					{
					case ART_Yes: // Yes
						{
							ObjectsToDelete.AddItem( CurrentObject );
							break;
						}

					case ART_YesAll: // Yes to All
						{
							ForceDeleteAll = TRUE;
							ObjectsToDelete.AddItem( CurrentObject );
							break;
						}

					case ART_No: // No
						{
							//Skip to the next object and proceed
							continue;
							break;
						}

					case ART_NoAll: // No to All
						{
							GWarn->EndSlowTask();
							return NumDeletedObjects;
						}

					default:
						break;
					}
				}
				else
				{
					ObjectsToDelete.AddItem( CurrentObject );
				}
			}
			else
			{
				ObjectsToDelete.AddItem( CurrentObject );
			}
		}

		TArray< UGenericBrowserType* > UpdatedResources;
		{
			// Replacing references inside already loaded objects could cause rendering issues, so globally detach all components from their scenes for now
			FGlobalComponentReattachContext ReattachContext;

			FForceReplaceInfo ReplaceInfo;
			ForceReplaceReferences( NULL, ObjectsToDelete, ReplaceInfo, FALSE );

			for( TArray<UObject*>::TIterator ObjectItr(ObjectsToDelete); ObjectItr; ++ObjectItr )
			{
				UObject* CurObject = *ObjectItr; 

				if( DeleteSingleObject( CurObject, InResourceTypes ) )
				{
					// Track which generic browser types were affected by the deletion
					for ( TArray<UGenericBrowserType*>::TConstIterator ResourceIter( InResourceTypes ); ResourceIter; ++ResourceIter )
					{
						UGenericBrowserType* CurGBT = *ResourceIter;
						if ( CurGBT->Supports( CurObject ) )
						{
							UpdatedResources.AddUniqueItem( CurGBT );
						}
					}
				}
				// Update return val
				++NumDeletedObjects;

				GWarn->StatusUpdatef( ObjectItr.GetIndex(), ReplaceInfo.ReplaceableObjects.Num(), *LocalizeUnrealEd("ConsolidateAssetsUpdate_DeletingObjects") );

			}
		}

		// Collect garbage.
		UObject::CollectGarbage( GARBAGE_COLLECTION_KEEPFLAGS );

		// Inform the relevant generic browser types of the object deletions
		for ( TArray<UGenericBrowserType*>::TIterator UpdatedResourceIter( UpdatedResources ); UpdatedResourceIter; ++UpdatedResourceIter )
		{
			UGenericBrowserType* CurGBT = *UpdatedResourceIter;
			CurGBT->NotifyPostDeleteObject();
		}


		GWarn->EndSlowTask();
		return NumDeletedObjects;
	}	

	
	/**
	 * Utility function to compose a string list of referencing objects
	 *
	 * @param References			Array of references to the relevant object
	 * @param RefObjNames			String list of all objects
	 * @param DefObjNames			String list of all objects referenced in default properties
	 *
     * @return Whether or not any objects are in default properties
	 */
	UBOOL ComposeStringOfReferencingObjects( TArray<FReferencerInformation>& References, FString& RefObjNames, FString& DefObjNames )
	{
		UBOOL bInDefaultProperties = FALSE;

		for ( TArray<FReferencerInformation>::TConstIterator ReferenceInfoItr( References ); ReferenceInfoItr; ++ReferenceInfoItr )
		{
			FReferencerInformation RefInfo = *ReferenceInfoItr;
			UObject* ReferencingObject = RefInfo.Referencer;
			RefObjNames = RefObjNames + TEXT("\n") + ReferencingObject->GetPathName();

			if( ReferencingObject->GetPathName().InStr( FString(DEFAULT_OBJECT_PREFIX), FALSE, FALSE, 0) >= 0 )
			{
				DefObjNames = DefObjNames + TEXT("\n") + ReferencingObject->GetName();
				bInDefaultProperties = TRUE;
			}
		}

		return bInDefaultProperties;
	}


	/**
	 * Internal implementation of rename objects with refs
	 * 
	 * @param Objects		The objects to rename
	 * @param bLocPackages	If true, the objects belong in localized packages
	 * @param ObjectToLanguageExtMap	An array of mappings of object to matching language (for fixing up localization if the objects are moved ).  Note: Not used if bLocPackages is false
	 * @param InBrowsableObjectTypes	A mapping of classes to their generic browser type for finding object references only if they are browsable.
	 */
	UBOOL RenameObjectsWithRefsInternal( TArray<TArray<UObject*> >& Objects, UBOOL bLocPackages, const TArray<TMap< UObject*, FString > >* ObjectToLanguageExtMap, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes )
	{
		// Present the user with a rename dialog for each asset.
		const FString DialogTitle( LocalizeUnrealEd( "MoveWithReferences" ) );
		WxDlgMoveAssets MoveDialog;
		const UBOOL bEnableTreatNameAsSuffix = FALSE;
		MoveDialog.ConfigureNameField( bEnableTreatNameAsSuffix );

		FString ErrorMessage;
		FString Reason;
		FString PreviousPackage;
		FString PreviousGroup;
		FString PreviousName;
		UBOOL bPreviousNameAsSuffix = bEnableTreatNameAsSuffix;
		UBOOL bSawOKToAll = FALSE;
		UBOOL bSawSuccessfulRename = FALSE;

		TArray<UPackage*> PackagesUserRefusedToFullyLoad;
		TArray<UPackage*> OutermostPackagesToSave;
		for( INT Index = 0; Index < Objects.Num(); Index++ )
		{
			// Pull out the object list for this selected object, it could have multiple objects in the case of localized packages.
			TArray<UObject*>* ObjectList;
			ObjectList = &(Objects( Index ));

			// Go through all files available in this list and do the rename on them.
			for( INT FileIndex = 0; FileIndex < ObjectList->Num(); FileIndex++)
			{
				UObject* Object = (*ObjectList)( FileIndex );
				if( !Object )
				{
					continue;
				}

				FString DlgPackage = Object->GetOutermost()->GetName();
				if( !bLocPackages && PreviousPackage.Len() )
				{
					// Initialize the move dialog with the previously entered pkg/grp
					DlgPackage = PreviousPackage;
				}

				FString DlgGroup = PreviousGroup.Len() ? PreviousGroup : ( Object->GetOuter()->GetOuter() ? Object->GetFullGroupName( 1 ) : TEXT( "" ) );
				if( !bSawOKToAll )
				{
					MoveDialog.SetTitle( *FString::Printf( TEXT( "%s: %s" ), *DialogTitle, *Object->GetPathName() ) );
					const int MoveDialogResult = MoveDialog.ShowModal( DlgPackage, DlgGroup, ( bPreviousNameAsSuffix && PreviousName.Len() ? PreviousName : Object->GetName() ), TRUE );
					// Abort if the user cancelled.
					if( MoveDialogResult != wxID_OK && MoveDialogResult != ID_OK_ALL )
					{
						return FALSE;
					}
					// Don't show the dialog again if "Ok to All" was selected.
					if( MoveDialogResult == ID_OK_ALL )
					{
						bSawOKToAll = TRUE;
					}

					// Store the entered package/group/name for later retrieval.
					PreviousPackage = MoveDialog.GetNewPackage();
					PreviousGroup = MoveDialog.GetNewGroup();
					bPreviousNameAsSuffix = MoveDialog.GetNewName( PreviousName );

					bSawOKToAll |= bLocPackages;
				}
				const FScopedBusyCursor BusyCursor;

				// Find references.
				TArray<UObject*> ReferencedTopLevelObjects;
				const UBOOL bIncludeRefs = MoveDialog.GetIncludeRefs();
				if ( bIncludeRefs )
				{
					GetReferencedTopLevelObjects( Object, ReferencedTopLevelObjects, InBrowsableObjectTypes );
				}
				else
				{
					// Add just the object itself.
					ReferencedTopLevelObjects.AddItem( Object );
				}

				UBOOL bMoveFailed = FALSE;
				UBOOL bMoveRedirectorFailed = FALSE;
				TArray<FMoveInfo> MoveInfos;
				for ( INT ObjectIndex = 0 ; ObjectIndex < ReferencedTopLevelObjects.Num() && !bMoveFailed ; ++ObjectIndex )
				{
					UObject *RefObject = ReferencedTopLevelObjects(ObjectIndex);
					FMoveInfo* MoveInfo = new(MoveInfos) FMoveInfo;

					// Determine the target package.
					FString ClassPackage;
					FString ClassGroup;
					MoveDialog.DetermineClassPackageAndGroup( RefObject, ClassPackage, ClassGroup );

					FString TgtPackage;
					FString TgtGroup;
				
					// The language extension for localized packages. Defaults to INT
					FString LanguageExt = TEXT("INT");
				
					// If the package the object is being moved to is new
					UBOOL bPackageIsNew = FALSE;

					// If a class-specific package was specified, use the class-specific package/group combo.
					if ( ClassPackage.Len() )
					{
						TgtPackage = ClassPackage;
						TgtGroup = ClassGroup;
					}
					else
					{
						// No class-specific package was specified, so use the 'universal' destination package.
						TgtPackage = MoveDialog.GetNewPackage();
					
						if( bLocPackages && TgtPackage != RefObject->GetOutermost()->GetName() )
						{
							// If localized sounds are being moved to a different package 
							// make sure the package they are being moved to is valid

							if( (*ObjectToLanguageExtMap)( Index ).Num() )
							{
								// Language extension package this object is in
								const FString* FoundLanguageExt = (*ObjectToLanguageExtMap)( Index ).Find( RefObject );

								if( FoundLanguageExt && *FoundLanguageExt != TEXT("INT") )
								{
									// A language extension has been found for this object.
									// Append the package name with the language extension.
									// Do not append INT packages as they have no extension
									LanguageExt = *FoundLanguageExt->ToUpper();
									TgtPackage += FString::Printf( TEXT("_%s"), *LanguageExt );
								}

							}

							// Check to see if the language specific path is the same as the path in the filename
							const FString LanguageSpecificPath = FString::Printf( TEXT("%s\\%s"), TEXT("Sounds"), *LanguageExt );

							// Filename of the package we are moving from
							FString OriginPackageFilename;
							// If the object was is in a localized directory.  SoundNodeWaves in non localized package file paths should  be able to move anywhere.
							UBOOL bOriginPackageInLocalizedDir = FALSE;
							if ( GPackageFileCache->FindPackageFile( *RefObject->GetOutermost()->GetName(), NULL, OriginPackageFilename ) )
							{
								// if the language specific path cant be found in the origin package filename, this package is not in a directory for only localized packages
								bOriginPackageInLocalizedDir = (OriginPackageFilename.InStr( LanguageSpecificPath, FALSE, TRUE ) != INDEX_NONE);
							}

							// Filename of the package we are moving to
							FString DestPackageName;
							// Find the package filename of the package we are moving to.
							bPackageIsNew = !GPackageFileCache->FindPackageFile( *TgtPackage, NULL, DestPackageName );
							if( !bPackageIsNew && bOriginPackageInLocalizedDir && DestPackageName.InStr( LanguageSpecificPath, FALSE, TRUE ) == INDEX_NONE )
							{	
								// Skip new packages or packages not in localized dirs (objects in these can move anywhere)
								// If the the language specific path cannot be found in the destination package filename
								// This package is being moved to an invalid location.
								bMoveFailed = TRUE;
								ErrorMessage += FString::Printf( LocalizeSecure(LocalizeUnrealEd( TEXT("Error_InvalidMoveOfLocalizedObject") ),  *RefObject->GetName() ) );
								// no reason to continue the move.  
								break;
							}
						}

						TgtGroup = ClassGroup.Len() ? ClassGroup : MoveDialog.GetNewGroup();
					}

					// Make sure that a target package exists.
					if ( !TgtPackage.Len() )
					{
						ErrorMessage += TEXT("Invalid package name supplied\n");
						bMoveFailed = TRUE;
					}
					else
					{
						// Make a new object name by concatenating the source object name with the optional name suffix.
						// DlgName will be ignored with it's not to be treaded as suffix and 'ok to all' has been selected
						FString DlgName;
						const UBOOL bTreatDlgNameAsSuffix = MoveDialog.GetNewName( DlgName );
						const FString ObjName = bTreatDlgNameAsSuffix ? FString::Printf(TEXT("%s%s"), *RefObject->GetName(), *DlgName) : (bSawOKToAll ? *RefObject->GetName() : *DlgName);

						// Make a full path from the target package and group.
						const FString FullPackageName = TgtGroup.Len()
							? FString::Printf(TEXT("%s.%s"), *TgtPackage, *TgtGroup)
							: TgtPackage;

						// Make sure the target package is fully loaded.
						TArray<UPackage*> TopLevelPackages;
						UPackage* ExistingPackage = UObject::FindPackage(NULL, *FullPackageName);
						UPackage* ExistingOutermostPackage = TgtGroup.Len() ? UObject::FindPackage(NULL, *TgtPackage) : ExistingPackage;

						if( ExistingPackage )
						{
							TopLevelPackages.AddItem( ExistingPackage->GetOutermost() );
						}

						// If there's an existing outermost package, try to find its filename
						FString ExistingOutermostPackageFilename;
						if ( ExistingOutermostPackage )
						{
							GPackageFileCache->FindPackageFile( *ExistingOutermostPackage->GetName(), NULL, ExistingOutermostPackageFilename );
						}

						if( RefObject )
						{
							// Fully load the ref objects package
							TopLevelPackages.AddItem( RefObject->GetOutermost() );
						}

						if( (ExistingPackage && PackagesUserRefusedToFullyLoad.ContainsItem(ExistingPackage)) ||
							!PackageTools::HandleFullyLoadingPackages( TopLevelPackages, TEXT("Rename") ) )
						{
							// HandleFullyLoadingPackages should never return FALSE for empty input.
							check( ExistingPackage );
							PackagesUserRefusedToFullyLoad.AddItem( ExistingPackage );
							bMoveFailed = TRUE;
						}
						// Don't allow a move/rename to occur into a package that has a filename invalid for saving. This is a rare case
						// that should not happen often, but could occur using packages created before the editor checked against file name length
						else if ( ExistingOutermostPackage && ExistingOutermostPackageFilename.Len() > 0 && !FEditorFileUtils::IsFilenameValidForSaving( ExistingOutermostPackageFilename, ErrorMessage ) )
						{
							bMoveFailed = TRUE;
						}
						else if( !ObjName.Len() )
						{
							ErrorMessage += TEXT("Invalid object name\n");
							bMoveFailed = TRUE;
						}
						else if(!FIsValidObjectName( *ObjName,Reason ) 
							||	!FIsValidGroupName(*TgtPackage,Reason)
							||	!FIsValidGroupName(*TgtGroup,Reason,TRUE) )
						{
							// Make sure the object name is valid.
							ErrorMessage += FString::Printf(TEXT("    %s to %s.%s: %s\n"), *RefObject->GetPathName(), *FullPackageName, *ObjName, *Reason );
							bMoveFailed = TRUE;
						}
						else
						{
							// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
							const FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *FullPackageName, *ObjName);
							UObjectRedirector* Redirector = Cast<UObjectRedirector>( UObject::StaticFindObject(UObjectRedirector::StaticClass(), NULL, *FullObjectPath) );
							// If we found a redirector, check that the object it points to is of the same class.
							if ( Redirector
								&& Redirector->DestinationObject
								&& Redirector->DestinationObject->GetClass() == RefObject->GetClass() )
							{
								// Test renaming the redirector into a dummy package.
								if ( !Redirector->Rename(*Redirector->GetName(), UObject::CreatePackage(NULL, TEXT("DeletedRedirectors")), REN_Test) )
								{
									bMoveFailed = TRUE;
									bMoveRedirectorFailed = TRUE;
								}
							}

							if ( !bMoveFailed )
							{
								UPackage* NewPackage = UObject::CreatePackage( NULL, *FullPackageName );
								// Test to see if the rename will succeed.
								if ( RefObject->Rename(*ObjName, NewPackage, REN_Test) )
								{
									// No errors!  Set asset move info.
									MoveInfo->Set( *FullPackageName, *ObjName );
									if( bLocPackages && bPackageIsNew )
									{
										// Setup the path this localized package should be saved to.
										FString Path = appGameDir() * TEXT("Content") * TEXT("Sounds") * LanguageExt * TgtPackage + TEXT( ".upk" );
										// Move the package into the correct file location by saving it
										GUnrealEd->Exec( *FString::Printf(TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), *TgtPackage, *Path) );
									}
								}
								else
								{
									ErrorMessage += FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_ObjectNameAlreadyExists"), *(Object->GetFullName())) );
									bMoveFailed = TRUE;
								}
							}
						}
					} // Tgt package valid?
				} // Referenced top-level objects

				if ( !bMoveFailed )
				{
					// Actually perform the move!
					for ( INT RefObjectIndex = 0 ; RefObjectIndex < ReferencedTopLevelObjects.Num() ; ++RefObjectIndex )
					{
						UObject *RefObject = ReferencedTopLevelObjects(RefObjectIndex);
						const FMoveInfo& MoveInfo = MoveInfos(RefObjectIndex);
						check( MoveInfo.IsValid() );

						const FString& PkgName = MoveInfo.FullPackageName;
						const FString& ObjName = MoveInfo.NewObjName;
						const FString FullObjectPath = FString::Printf(TEXT("%s.%s"), *PkgName, *ObjName);

						// We can rename on top of an object redirection (basically destroy the redirection and put us in its place).
						UObjectRedirector* Redirector = Cast<UObjectRedirector>( UObject::StaticFindObject(UObjectRedirector::StaticClass(), NULL, *FullObjectPath) );
						// If we found a redirector, check that the object it points to is of the same class.
						if ( Redirector
							&& Redirector->DestinationObject
							&& Redirector->DestinationObject->GetClass() == RefObject->GetClass() )
						{
							// Remove public flag if set to ensure below rename doesn't create a redirect.
							Redirector->ClearFlags( RF_Public );
							// Instead of deleting we rename the redirector into a dummy package.
							Redirector->Rename(*Redirector->GetName(), UObject::CreatePackage( NULL, TEXT("DeletedRedirectors")) );
						}
						UPackage* NewPackage = UObject::CreatePackage( NULL, *PkgName );
						OutermostPackagesToSave.AddUniqueItem( RefObject->GetOutermost() );

						// if this object is being renamed out of the MyLevel package into a content package, we need to mark it RF_Standalone
						// so that it will be saved (UWorld::CleanupWorld() clears this flag for all objects inside the package)
						if (!RefObject->HasAnyFlags(RF_Standalone)
							&&	RefObject->GetOutermost()->ContainsMap()
							&&	!NewPackage->GetOutermost()->ContainsMap() )
						{
							RefObject->SetFlags(RF_Standalone);
						}

						FString OldObjectFullName = RefObject->GetFullName();
						GEditor->RenameObject( RefObject, NewPackage, *ObjName, REN_None );

						// Send the "rename object" event to the content browser.  We pass the newly-renamed object to the event,
						// along with the object's old path name.
						GCallbackEvent->Send( FCallbackEventParameters( NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectRenamed, RefObject, OldObjectFullName ) );

						OutermostPackagesToSave.AddUniqueItem( NewPackage->GetOutermost() );
						bSawSuccessfulRename = TRUE;
						GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_UpdatePackageList|CBR_UpdateAssetList, RefObject));
					} // Referenced top-level objects
				}
				else
				{
					if(bMoveRedirectorFailed)
					{
						ErrorMessage += FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_CouldntRenameObjectRedirectorF"), *Object->GetFullName()) );
					}
					else
					{
						ErrorMessage += FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_CouldntRenameObjectF"), *Object->GetFullName()) );
					}

					if( bLocPackages )
					{
						// Inform the user that no localized objects will be moved or renamed
						ErrorMessage += FString::Printf( TEXT("No localized objects could be moved"));
						// break out of the main loop, 
						break;
					}
				}
			}
		} // Selected objects.

		// Display any error messages that accumulated.
		if ( ErrorMessage.Len() > 0 )
		{
			appMsgf( AMT_OK, *ErrorMessage );
		}

		// Update the browser if something was actually renamed.
		if ( bSawSuccessfulRename )
		{
			DWORD CallbackFlags = CBR_UpdatePackageList|CBR_UpdateAssetList;
			if ( MoveDialog.GetCheckoutPackages() )
			{
				PackageTools::CheckOutRootPackages(OutermostPackagesToSave);
				CallbackFlags |= CBR_UpdateSCCState;
			}
			if ( MoveDialog.GetSavePackages() )
			{
				PackageTools::SavePackages(OutermostPackagesToSave, TRUE);
			}

			FCallbackEventParameters Parms(NULL, CALLBACK_RefreshContentBrowser, CallbackFlags);
			GCallbackEvent->Send(Parms);
		}

		return (ErrorMessage.Len() <= 0);
	}

	/** 
	 * Finds all language variants for the passed in sound wave
	 * 
	 * @param OutObjects	A list of found localized sound wave objects
	 * @param OutObjectToLanguageExtMap	A mapping of sound wave objects to their language extension
	 * @param Wave	The sound wave to search for
	 */
	void AddLanguageVariants( TArray<UObject*>& OutObjects, TMap< UObject*, FString >& OutObjectToLanguageExtMap, USoundNodeWave* Wave )
	{
		if( Wave->GetClass()->HasAnyClassFlags( CLASS_Localized | CLASS_PerObjectLocalized ) || Wave->HasAnyFlags( RF_PerObjectLocalized ) )
		{
			// Determine what localized package the SoundNodeWave belongs to
			// This must be done in order to find any existing language extension on the package and find other localized versions.

			// Name of the package that the wave is in.
			FString PackageName = Wave->GetOutermost()->GetName().ToLower();
	
			// Find and knock off the localization extension off this package name if it exists.
			// The extension should be an underscore followed by 3 letters and be at the end of the package name

			// Get the last 4 letters of the package name and see if it is in the format "_EXT"
			FString PossibleExtension = PackageName.Right( 4 ).ToUpper();
			if( PossibleExtension[0] == '_' && Localization_GetLanguageExtensionIndex( &PossibleExtension[1] ) != -1 )
			{
				// get rid of the extension so we can add extensions onto this package when we search for localized sound waves
				PackageName = PackageName.Left( PackageName.Len()-4 );
			}
			
			// Get a list of known language extensions
			const TArray<FString>& KnownLanguageExtensions = appGetKnownLanguageExtensions();

			// Add all the localized variants to the list of objects to rename
			for( INT LangIndex = 0; LangIndex < KnownLanguageExtensions.Num(); LangIndex++ )
			{
				FString LocPackageName = PackageName;
				if ( LangIndex > 0)
				{
					// Add on each extension to form a localized package name to search in.
					LocPackageName += FString::Printf( TEXT( "_%s" ), *KnownLanguageExtensions(LangIndex) );
				}
				// Full pathname that the localized sound wave we are looking for should be in.
				FString LocFullName = LocPackageName + TEXT(".") + Wave->GetPathName( Wave->GetOutermost() );
				
				// Attempt to load the object at the path name we generated
				USoundNodeWave* LocObject = LoadObject<USoundNodeWave>( NULL, *LocFullName, NULL, LOAD_None, NULL );
				if( LocObject )
				{
					// If the object was successfully loaded, we found a valid language variant.
					OutObjectToLanguageExtMap.Set( LocObject, KnownLanguageExtensions(LangIndex) );
					OutObjects.AddUniqueItem( LocObject );
				}
			}
		}
	}


	/**
	 * Renames an object and leaves redirectors so other content that references it does not break
	 * Also renames all loc instances of the same asset
	 * @param Objects		The objects to rename
	 * @param bLocPackages	If true, the objects belong in localized packages
	 * @param InBrowsableObjectTypes	A mapping of classes to their generic browser type for finding object references only if they are browsable.
	 */
	UBOOL RenameObjectsWithRefs( TArray< UObject* >& SelectedObjects, UBOOL bIncludeLocInstances, const TMap< UClass*, TArray< UGenericBrowserType* > >* InBrowsableObjectTypes ) 
	{
		if( !bIncludeLocInstances )
		{
			// Add the selected objects to an array.
			TArray< TArray< UObject*> > ObjectList;
			ObjectList.AddItem(SelectedObjects);

			// Prompt the user, and rename the files.
			return RenameObjectsWithRefsInternal( ObjectList, bIncludeLocInstances, NULL, InBrowsableObjectTypes );
		}
		else
		{
			TArray< TArray< UObject*> > ObjectList;
			TArray< TMap< UObject*, FString > > ObjectMapList;
			TArray<UObject*> LocObjects;

			UBOOL bSucceed = TRUE;
			// For each object, find any localized variations and rename them as well
			for( INT Index = 0; Index < SelectedObjects.Num(); Index++ )
			{
				UObject* Object = SelectedObjects( Index );
				if( Object )
				{
					// NOTE: Only supported for SoundNodeWaves right now
					USoundNodeWave* Wave = ExactCast<USoundNodeWave>( Object );
					if( Wave )
					{
						// Add an empty array to the list, it will be filled out.
						ObjectList.AddItem(LocObjects);

						// A mapping of object to language extension, so we know where to move the localized sounds to if the user requests it.
						TMap< UObject*, FString > ObjectToLanguageExtMap;

						// Find if this is localized and add in the other languages
						AddLanguageVariants( ObjectList( Index ), ObjectToLanguageExtMap, Wave );

						// Add the map to the array of maps.
						ObjectMapList.AddItem(ObjectToLanguageExtMap);
					}

				}
			}
			
			// Prompt the user, and rename the files.
			return RenameObjectsWithRefsInternal( ObjectList, bIncludeLocInstances, &ObjectMapList, InBrowsableObjectTypes );
		}
	}

	/**
	 * Generates a list of all valid factory classes.
	 */
	void AssembleListOfImportFactories( TArray<UFactory*>& out_Factories, FString& out_Filetypes, FString& out_Extensions, TMultiMap<INT, UFactory*>& out_FilterIndexToFactory )
	{
		// Get the list of file filters from the factories
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			UClass* CurrentClass = (*It);

			if( CurrentClass->IsChildOf(UFactory::StaticClass()) && !(CurrentClass->ClassFlags & CLASS_Abstract) )
			{
				UFactory* Factory = ConstructObject<UFactory>( CurrentClass );
				if( Factory->bEditorImport && !Factory->bCreateNew && Factory->ValidForCurrentGame() )
				{
					out_Factories.AddItem( Factory );
				}
			}
		}

		// Generate the file types and extensions represented by the selected factories
		GenerateFactoryFileExtensions( out_Factories, out_Filetypes, out_Extensions, out_FilterIndexToFactory );
	}

	/**
	 * Internal helper function to obtain format descriptions and extensions of formats supported by the provided factory
	 *
	 * @param	InFactory			Factory whose formats should be retrieved
	 * @param	out_Descriptions	Array of format descriptions associated with the current factory; should equal the number of extensions
	 * @param	out_Extensions		Array of format extensions associated with the current factory; should equal the number of descriptions
	 */
	void InternalGetFactoryFormatInfo( const UFactory* InFactory, TArray<FString>& out_Descriptions, TArray<FString>& out_Extensions )
	{
		check(InFactory);

		// Iterate over each format the factory accepts
		for ( TArray<FString>::TConstIterator FormatIter( InFactory->Formats ); FormatIter; ++FormatIter )
		{
			const FString& CurFormat = *FormatIter;

			// Parse the format into its extension and description parts
			TArray<FString> FormatComponents;
			CurFormat.ParseIntoArray( &FormatComponents, TEXT(";"), FALSE );

			for ( INT ComponentIndex = 0; ComponentIndex < FormatComponents.Num(); ComponentIndex += 2 )
			{
				check( FormatComponents.IsValidIndex( ComponentIndex + 1 ) );
				out_Extensions.AddItem( FormatComponents(ComponentIndex) );
				out_Descriptions.AddItem( FormatComponents(ComponentIndex + 1) );
			}
		}
	}

	// Implement FString sorting for this file
	IMPLEMENT_COMPARE_CONSTREF( FString, UnObjectTools, { return appStricmp( *A, *B ); } )

	/**
	 * Populates two strings with all of the file types and extensions the provided factory supports.
	 *
	 * @param	InFactory		Factory whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	void GenerateFactoryFileExtensions( UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions, TMultiMap<INT, UFactory*>& out_FilterToFactory )
	{
		// Place the factory in an array and call the overloaded version of this function
		TArray<UFactory*> FactoryArray;
		FactoryArray.AddItem( InFactory );
		GenerateFactoryFileExtensions( FactoryArray, out_Filetypes, out_Extensions, out_FilterToFactory );
	}

	/**
	 * Populates two strings with all of the file types and extensions the provided factories support.
	 *
	 * @param	InFactories		Factories whose supported file types and extensions should be retrieved
	 * @param	out_Filetypes	File types supported by the provided factory, concatenated into a string
	 * @param	out_Extensions	Extensions supported by the provided factory, concatenated into a string
	 */
	void GenerateFactoryFileExtensions( const TArray<UFactory*>& InFactories, FString& out_Filetypes, FString& out_Extensions, TMultiMap<INT, UFactory*>& out_FilterIndexToFactory )
	{
		// Store all the descriptions and their corresponding extensions in a map
		TMultiMap<FString, FString> DescToExtensionMap;
		TMultiMap<FString, UFactory*> DescToFactory;

		// Iterate over each factory, retrieving their supported file descriptions and extensions, and storing them into the map
		for ( TArray<UFactory*>::TConstIterator FactoryIter(InFactories); FactoryIter; ++FactoryIter )
		{
			UFactory* CurFactory = *FactoryIter;
			check(CurFactory);

			TArray<FString> Descriptions;
			TArray<FString> Extensions;
			InternalGetFactoryFormatInfo( CurFactory, Descriptions, Extensions );
			check( Descriptions.Num() == Extensions.Num() );

			// Make sure to only store each key, value pair once
			for ( INT FormatIndex = 0; FormatIndex < Descriptions.Num() && FormatIndex < Extensions.Num(); ++FormatIndex )
			{
				DescToExtensionMap.AddUnique( Descriptions(FormatIndex), Extensions(FormatIndex ) );
				DescToFactory.AddUnique(Descriptions(FormatIndex), CurFactory);
			}
		}
		
		// Zero out the output strings in case they came in with data already
		out_Filetypes = ""; 
		out_Extensions = "";

		// Sort the map's keys alphabetically
		DescToExtensionMap.KeySort<COMPARE_CONSTREF_CLASS( FString, UnObjectTools )>();
		
		// Retrieve an array of all of the unique keys within the map
		TLookupMap<FString> DescriptionKeyMap;
		DescToExtensionMap.GetKeys( DescriptionKeyMap );
		const TArray<FString>& DescriptionKeys = DescriptionKeyMap.GetUniqueElements();

		INT IdxFilter = 1;

		// Iterate over each unique map key, retrieving all of each key's associated values in order to populate the strings
		for ( TArray<FString>::TConstIterator DescIter( DescriptionKeys ); DescIter; ++DescIter )
		{
			const FString& CurDescription = *DescIter;
			
			// Retrieve each value associated with the current key
			TArray<FString> Extensions;
			DescToExtensionMap.MultiFind( CurDescription, Extensions );
			if ( Extensions.Num() > 0 )
			{
				// Sort each extension alphabetically, so that the output is alphabetical by description, and in the event of
				// a description with multiple extensions, alphabetical by extension as well
				Sort<USE_COMPARE_CONSTREF( FString, UnObjectTools )>( &Extensions(0), Extensions.Num() );
				
				for ( TArray<FString>::TConstIterator ExtIter( Extensions ); ExtIter; ++ExtIter )
				{
					const FString& CurExtension = *ExtIter;
					const FString& CurLine = FString::Printf( TEXT("%s (*.%s)|*.%s"), *CurDescription, *CurExtension, *CurExtension );

					// The same extension could be used for multiple types (like with t3d), so ensure any given extension is only added to the string once
					if ( out_Extensions.InStr( CurExtension, FALSE, TRUE ) == INDEX_NONE )
					{
						if ( out_Extensions.Len() > 0 )
						{
							out_Extensions += TEXT(";");
						}
						out_Extensions += FString::Printf(TEXT("*.%s"), *CurExtension);
					}

					// Each description-extension pair can only appear once in the map, so no need to check the string for duplicates
					if ( out_Filetypes.Len() > 0 )
					{
						out_Filetypes += TEXT("|");
					}
					out_Filetypes += CurLine;


#if WITH_SUBSTANCE_AIR == 1
					// save the mapping for the Substance Factories
					TArray<UFactory*> Factories;
					DescToFactory.MultiFind( CurDescription, Factories );

					TArray<UFactory*>::TIterator FactIt(Factories);
					for (;FactIt;++FactIt)
					{
						if (ExactCast<USubstanceAirImportFactory>(*FactIt) != NULL ||
							ExactCast<USubstanceAirImageInputFactory>(*FactIt) != NULL)
						{
							out_FilterIndexToFactory.Set( IdxFilter, *FactIt );
							break;
						}
					}

					++IdxFilter;
#endif // WITH_SUBSTANCE_AIR

				}
			}
		}
	}

	/**
	 * Generates a list of file types for a given class.
	 */
	void AppendFactoryFileExtensions ( UFactory* InFactory, FString& out_Filetypes, FString& out_Extensions )
	{
		TArray<FString> Descriptions;
		TArray<FString> Extensions;
		InternalGetFactoryFormatInfo( InFactory, Descriptions, Extensions );
		check( Descriptions.Num() == Extensions.Num() );

		for ( INT FormatIndex = 0; FormatIndex < Descriptions.Num() && FormatIndex < Extensions.Num(); ++FormatIndex )
		{
			const FString& CurDescription = Descriptions(FormatIndex);
			const FString& CurExtension = Extensions(FormatIndex);
			const FString& CurLine = FString::Printf( TEXT("%s (*.%s)|*.%s"), *CurDescription, *CurExtension, *CurExtension );

			// Only append the extension if it's not already one of the found extensions
			if ( out_Extensions.InStr( CurExtension, FALSE, TRUE ) == INDEX_NONE )
			{
				if ( out_Extensions.Len() > 0 )
				{
					out_Extensions += TEXT(";");
				}
				out_Extensions += FString::Printf(TEXT("*.%s"), *CurExtension);
			}

			// Only append the line if it's not already one of the found filetypes
			if ( out_Filetypes.InStr( CurLine, FALSE, TRUE ) == INDEX_NONE )
			{
				if ( out_Filetypes.Len() > 0 )
				{
					out_Filetypes += TEXT("|");
				}
				out_Filetypes += CurLine;
			}
		}
	}

	/**
	 * Iterates over all classes and assembles a list of non-abstract UExport-derived type instances.
	 */
	void AssembleListOfExporters(TArray<UExporter*>& OutExporters)
	{
		// @todo DB: Assemble this set once.
		OutExporters.Empty();
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if( It->IsChildOf(UExporter::StaticClass()) && !It->HasAnyClassFlags(CLASS_Abstract) )
			{
				UExporter* Exporter = ConstructObject<UExporter>( *It );
				OutExporters.AddItem( Exporter );		
			}
		}
	}

	/**
	 * Assembles a path from the outer chain of the specified object.
	 */
	void GetDirectoryFromObjectPath(const UObject* Obj, FString& OutResult)
	{
		if( Obj )
		{
			GetDirectoryFromObjectPath( Obj->GetOuter(), OutResult );
			OutResult *= Obj->GetName();
		}
	}

	/**
	 * Opens a File Dialog based on the extensions requested in a factory
	 * @param InFactory - Factory with the appropriate extensions
	 * @param InMessage - Message to display in the dialog
	 * @param OutFileName - Filename that was selected by the dialog (if there was one)
	 * @return - TRUE if a file was successfully selected
	 */
	UBOOL FindFileFromFactory (UFactory* InFactory, const FString& InMessage, FString& OutFileName)
	{
		FString FileTypes, AllExtensions;
		TMultiMap<INT, UFactory*> FilterToFactory;

		ObjectTools::GenerateFactoryFileExtensions(InFactory, FileTypes, AllExtensions, FilterToFactory);
		FileTypes = FString::Printf(TEXT("All Files (%s)|%s|%s"),*AllExtensions,*AllExtensions,*FileTypes);
		
		//offer a find file dialog
		WxFileDialog OpenFileDialog(GApp->EditorFrame, *InMessage,
			*GApp->LastDir[LD_GENERIC_IMPORT],
			TEXT(""),
			*FileTypes,
			wxOPEN | wxFILE_MUST_EXIST,
			wxDefaultPosition
			);
		if( OpenFileDialog.ShowModal() == wxID_OK )
		{
			OutFileName = OpenFileDialog.GetPath();
			return TRUE;
		}
		return FALSE;
	}

	/**
	 * Calls Init on the given resource type
	 * Used when the resource type needs to be updated due to new data being available
	 */
	void RefreshResourceType( UClass* ResourceType )
	{
		for( TObjectIterator<UGenericBrowserType> It; It; ++It )
		{
			UGenericBrowserType* GBT = *It;
			if( GBT->IsA( ResourceType ) )
			{
				GBT->Clear();
				GBT->Init();
			}
		}
	}

	/*-----------------------------------------------------------------------------
		WxDlgExportGeneric.
	-----------------------------------------------------------------------------*/

	BEGIN_EVENT_TABLE(WxDlgImportGeneric, wxDialog)
		EVT_BUTTON( wxID_OK, WxDlgImportGeneric::OnOK )
		EVT_BUTTON( wxID_CANCEL, WxDlgImportGeneric::OnCancel )
	END_EVENT_TABLE()

	WxDlgImportGeneric::WxDlgImportGeneric(UBOOL bInOKToAll, UBOOL bInBulkImportMode)
	{
		const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GENERIC_IMPORT") );
		check( bSuccess );

		PGNPanel = (wxPanel*)FindWindow( XRCID( "ID_PKGGRPNAME" ) );
		check( PGNPanel != NULL );
		//wxSizer* szr = PGNPanel->GetSizer();


		PGNSizer = new wxBoxSizer(wxHORIZONTAL);
		{
			PGNCtrl = new WxPkgGrpNameCtrl( PGNPanel, -1, NULL, TRUE );
			PGNCtrl->SetSizer(PGNCtrl->FlexGridSizer);
			PGNSizer->Add(PGNCtrl, 1, wxEXPAND);
		}
		PGNPanel->SetSizer(PGNSizer);
		PGNPanel->SetAutoLayout(true);
		PGNPanel->Layout();

		//Has to be done before property window is created
		FLocalizeWindow( this );

		// Add Property Window to the panel we created for it.
		wxPanel* PropertyWindowPanel = (wxPanel*)FindWindow( XRCID( "ID_PROPERTY_WINDOW_PANEL" ) );
		check( PropertyWindowPanel != NULL );

		PropsPanelSizer = new wxBoxSizer(wxVERTICAL);
		{
			PropertyWindow = new WxPropertyWindowHost;
			PropertyWindow->Create( PropertyWindowPanel, GUnrealEd );
			
			//disable show/hide categories in this menu, they are not used.
			PropertyWindow->EnableCategoryOptions(FALSE);
			
			PropertyWindow->Show();
			PropsPanelSizer->Add( PropertyWindow, wxEXPAND, wxEXPAND );
		}
		PropertyWindowPanel->SetSizer( PropsPanelSizer );
		PropertyWindowPanel->SetAutoLayout(true);

		wxWindow* SceneWindow = FindWindow( XRCID( "IDPB_SCENE_INFO" ) );
		if (SceneWindow)
		{
			SceneWindow->Show( FALSE );
		}

		if( bInBulkImportMode )
		{
			// hide the build from path button.  It will do nothing in bulk import mode and might cause confusion among users.
			wxWindow* PathWindow = FindWindow( XRCID( "IDPB_BUILD_FROM_PATH" ) );
			if (PathWindow)
			{
				PathWindow->Show( FALSE );
			}

			// Show the cancel all button in bulk import mode.
			wxWindow* CancelAllWindow = FindWindow( XRCID( "ID_CANCEL_ALL" ) );
			if (CancelAllWindow)
			{
				CancelAllWindow->Show( TRUE );
			}
		}

		ADDEVENTHANDLER( XRCID("ID_OK_ALL"), wxEVT_COMMAND_BUTTON_CLICKED , &WxDlgImportGeneric::OnOKAll );
		ADDEVENTHANDLER( XRCID("ID_CANCEL_ALL"), wxEVT_COMMAND_BUTTON_CLICKED , &WxDlgImportGeneric::OnCancelAll );
		ADDEVENTHANDLER( XRCID("IDPB_BUILD_FROM_PATH"), wxEVT_COMMAND_BUTTON_CLICKED , &WxDlgImportGeneric::OnBuildFromPath );
		ADDEVENTHANDLER( XRCID("IDPB_SCENE_INFO"), wxEVT_COMMAND_BUTTON_CLICKED , &WxDlgImportGeneric::OnSceneInfo );

		Factory = NULL;
		bOKToAll = bInOKToAll;
		bBulkImportMode = bInBulkImportMode;
		bImporting = NULL;

		wxRect ThisRect = GetRect();
		ThisRect.width = 500;
		ThisRect.height = 500;
		SetSize( ThisRect );

		FWindowUtil::LoadPosSize( TEXT("DlgImportGeneric"), this, -1, -1, ThisRect.width, ThisRect.height );
	}

	int WxDlgImportGeneric::ShowModal(const FFilename& InFilename, const FString& InPackage, const FString& InGroup, UClass* InClass, UFactory* InFactory, UBOOL* bInImporting )
	{
		Filename = InFilename;
		Package = InPackage;
		Group = InGroup;
		Factory = InFactory;
		Class = InClass;

		// Special case handling for bulk package file.
		if (Filename.GetExtension() == TEXT("T3DPKG"))
		{
			// Special case handling!
			UObject* Result = UFactory::StaticImportObject( Class, NULL, FName( *Name ), RF_Public|RF_Standalone, *Filename, NULL, Factory );
			if ( !Result )
			{
				// only open a message box if not in bulk import mode so we dont block the rest of the imports
				if( !bBulkImportMode )
				{
					appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ImportFailed") );
				}
			}
			return wxID_OK;
		}

#if WITH_GFx
		if ( Filename.GetExtension() == TEXT("SWF") )
		{
			// The package and group of SwfMovies should mirror the directory structure in the game's Flash/ directory.
			// Therefore we:
			//   1) automatically set the group and package
			//   2) disable the option to change package/group

			FString FlashRoot = UGFxMovieFactory::GetGameFlashDir();			
			UGFxMovieFactory::SwfImportInfo SwfInfo( InFilename );
			
			if ( SwfInfo.bIsValidForImport )
			{
				Package = SwfInfo.OutermostPackageName;
				Group = SwfInfo.GroupOnlyPath;
			}
			else
			{
				FString ErrorString = FString::Printf( TEXT("SWF Files must be located in a directory under %s. e.g. %s\\SomeDir\\SomeFile.swf"), *FlashRoot, *FlashRoot );
				warnf( *ErrorString );
				appMsgf( AMT_OK, *ErrorString );
				return wxID_CANCEL;
			}
			
			PGNCtrl->Enable( FALSE );
		}
#endif

#if !WITH_ACTORX
		if( Factory->IsA( UStaticMeshFactory::StaticClass() ) && Filename.GetExtension() == TEXT("ASE") ||
			Factory->IsA( USkeletalMeshFactory::StaticClass() ) && Filename.GetExtension() == TEXT("PSK") )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ActorXDeprecated") );
			return wxID_CANCEL;
		}
#endif


#if WITH_FBX
		if (Filename.GetExtension() == TEXT("FBX"))
		{
			wxWindow* SceneWindow = FindWindow( XRCID( "IDPB_SCENE_INFO" ) );
			if (SceneWindow)
			{
				SceneWindow->Show( TRUE );
			}
			
			UFbxFactory* FbxFactory = Cast<UFbxFactory>(Factory);
			if (FbxFactory)
			{
				FString FbxFilename = Filename;
				if (!FbxFactory->DetectImportType(Filename))
				{
					appMsgf( AMT_OK, LocalizeSecure(LocalizeUnrealEd("Error_ParseFBXFailed"), *FbxFilename ));
					return wxID_OK;
				}
			}
		}
#endif

		PGNCtrl->PkgCombo->SetValue( *Package );
		PGNCtrl->GrpEdit->SetValue( *Group );
		PGNCtrl->NameEdit->SetValue( *InFilename.GetBaseFilename() );

		// Copy off the location of the flag which marks DoImport() being active.
		bImporting = bInImporting;

		if( bOKToAll || GIsUnitTesting )
		{
			FlaggedDoImport();
			return wxID_OK;
		}
#if WITH_FBX
		if (Filename.GetExtension() == TEXT("FBX"))
		{
			UFbxFactory* FbxFactory = Cast<UFbxFactory>(Factory);
			PropertyWindow->SetObject( FbxFactory->ImportUI, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
		}
		else
#endif
		{
			PropertyWindow->SetObject( Factory, EPropertyWindowFlags::Sorted | EPropertyWindowFlags::ShouldShowCategories  );
		}
		PropertyWindow->Raise();
		Refresh();

		return wxDialog::ShowModal();
	}

	void WxDlgImportGeneric::OnBuildFromPath( wxCommandEvent& In )
	{
		TArray<FString> Split;

		if( Filename.ParseIntoArray( &Split, TEXT("\\"), 0 ) > 0 )
		{
			FString Field, Group, Name;
			UBOOL bPackageFound = FALSE;

			FString PackageName = PGNCtrl->PkgCombo->GetValue().c_str();

			for( INT x = 0 ; x < Split.Num() ; ++x )
			{
				Field = Split(x);

				if( x == Split.Num()-1 )
				{
					Name = Filename.GetBaseFilename();
				}
				else if( bPackageFound )
				{
					if( Group.Len() > 0 )
					{
						Group += TEXT(".");
					}
					Group += Field;
				}
				else if( PackageName == Field )
				{
					bPackageFound = TRUE;
				}
			}

			PGNCtrl->GrpEdit->SetValue( *Group );
			PGNCtrl->NameEdit->SetValue( *Name );
		}
	}

	void WxDlgImportGeneric::OnSceneInfo( wxCommandEvent& In )
	{
#if WITH_FBX
		if (Filename.GetExtension() == TEXT("FBX"))
		{
			UFbxFactory* FbxFactory = Cast<UFbxFactory>(Factory);
			if (FbxFactory)
			{
				FString FbxFilename = Filename;
				FbxSceneInfo SceneInfo;
				UBOOL Result = UnFbx::CFbxImporter::GetInstance()->GetSceneInfo(FbxFilename, SceneInfo);
				
				if (Result)
				{
					WxDlgFbxSceneInfo DlgSceneInfo(this, SceneInfo);
					DlgSceneInfo.ShowModal();
				}
				else
				{
				
				}
			}
		}
#endif	
	}
	
	void WxDlgImportGeneric::OnCancel(wxCommandEvent& In)
	{
#if WITH_FBX
		if (Filename.GetExtension() == TEXT("FBX"))
		{
			UnFbx::CFbxImporter::GetInstance()->ReleaseScene();
		}
#endif
		EndModal(wxID_CANCEL);
	}

	void WxDlgImportGeneric::DoImport()
	{
		// Make sure the property window has applied all outstanding changes
		if( PropertyWindow != NULL )
		{
			PropertyWindow->FlushLastFocused();
			PropertyWindow->ClearLastFocused();
		}


		const FScopedBusyCursor BusyCursor;
		Package = PGNCtrl->PkgCombo->GetValue();
		Group = PGNCtrl->GrpEdit->GetValue();
		Name = PGNCtrl->NameEdit->GetValue();

		FString	QualifiedName;
		if( Group.Len() )
		{
			QualifiedName = Package + TEXT(".") + Group + TEXT(".") + Name;
		}
		else
		{
			QualifiedName = Package + TEXT(".") + Name;
		}
		FString Reason;
		if (!FIsValidObjectName( *Name, Reason )
		||	!FIsValidGroupName( *Package, Reason )
		||	!FIsValidGroupName( *Group, Reason, TRUE))
		{
			appMsgf( AMT_OK, *FString::Printf(LocalizeSecure(LocalizeUnrealEd(TEXT("Error_ImportFailed_f")), *(QualifiedName + TEXT(": ") + Reason))) );
			return;
		}

		UPackage* Pkg = GEngine->CreatePackage(NULL,*Package);
		if( Group.Len() )
		{
			Pkg = GEngine->CreatePackage(Pkg,*Group);
		}


		// If a class wasn't specified, the factory can return multiple types.
		// Let factory use properties set in the import dialog to determine a class.
		if ( !Class )
		{
			Class = Factory->ResolveSupportedClass();
			check( Class );
		}

		check( Pkg );

		if( !Pkg->IsFullyLoaded() )
		{	
			// Ask user to fully load
			if(appMsgf( AMT_YesNo, *FString::Printf( LocalizeSecure(LocalizeUnrealEd(TEXT("NeedsToFullyLoadPackageF")), *Pkg->GetName(), *LocalizeUnrealEd("Import")) ) ) )
			{
				// Fully load package.
				const FScopedBusyCursor BusyCursor;
				GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("FullyLoadingPackages")), TRUE );
				Pkg->FullyLoad();
				GWarn->EndSlowTask();
			}
			// User declined abort operation.
			else
			{
				debugf(TEXT("Aborting operation as %s was not fully loaded."),*Pkg->GetName());
				return;
			}
		}

		UObject* Result = UFactory::StaticImportObject( Class, Pkg, FName( *Name ), RF_Public|RF_Standalone, *Filename, NULL, Factory );
		if ( !Result )
		{
			appMsgf( AMT_OK, *LocalizeUnrealEd("Error_ImportFailed") );
		}
		else
		{
			Result->MarkPackageDirty(TRUE);
			NewObjects.AddItem( Result );

			// do not sync the content browser in bulk import mode
			if( !bBulkImportMode )
			{
				// Refresh content browser
				GCallbackEvent->Send(FCallbackEventParameters(NULL, CALLBACK_RefreshContentBrowser, CBR_ObjectCreated, Result));
			}

		}

		if( IsModal() )
		{
			EndModal( wxID_OK );
		}
	}

	/**
	 * Imports all of the files in the string array passed in.
	 *
	 * @param InFiles					Files to import
	 * @param InFactories				Array of UFactory classes to be used for importing.
	 */
	UBOOL ImportFiles( const wxArrayString& InFiles, const TArray<UFactory*>& InFactories, FString* out_ImportPath/*=NULL*/, FString PackageName/*=TEXT("MyPackage")*/, FString GroupName/*=TEXT("")*/ )
	{
		// Detach all components while importing objects.
		// This is necessary for the cases where the import replaces existing objects which may be referenced by the attached components.
		FGlobalComponentReattachContext ReattachContext;

		UBOOL bSawSuccessfulImport = FALSE;		// Used to flag whether at least one import was successful
		UFactory* Factory = NULL;

		// Reset the 'Do you want to overwrite the existing object?' Yes to All / No to All prompt, to make sure the
		// user gets a chance to select something
		UFactory::ResetState();

		FString& LastImportPath = out_ImportPath != NULL ? *out_ImportPath : GApp->LastDir[LD_GENERIC_IMPORT];

		// For each filename, open up the import dialog and get required user input.
		WxDlgImportGeneric ImportDialog;
		for( UINT FileIndex = 0 ; FileIndex < InFiles.Count() ; FileIndex++ )
		{
			GWarn->StatusUpdatef( FileIndex, InFiles.Count(), *FString::Printf( LocalizeSecure(LocalizeUnrealEd( "Importingf" ), FileIndex,(UINT)InFiles.Count()) ) );

			const FFilename Filename = InFiles[FileIndex].c_str();
			const FString FileExtension = Filename.GetExtension();

			LastImportPath = Filename.GetPath();

			// Find a set of factories used to import files with this extension
			TArray<UFactory*> Factories;
			Factories.Empty(InFactories.Num());
			for( INT FactoryIdx = 0; FactoryIdx < InFactories.Num(); ++FactoryIdx )
			{
				UFactory* NextFactory = InFactories(FactoryIdx);
				for( INT FormatIndex = 0; FormatIndex < NextFactory->Formats.Num(); FormatIndex++ )
				{
					const FString& FactoryFormat = NextFactory->Formats(FormatIndex);
					if( FactoryFormat.Left( FileExtension.Len() ) == FileExtension )
					{
						Factories.AddItem(NextFactory);
						break;
					}
				}
			}

			// Handle the potential of multiple factories being found
			if( Factories.Num() == 1 )
			{
				Factory = Factories( 0 );
			}
			else if( Factories.Num() > 1 )
			{
				Factory = Factories(0);
				for( INT i = 0; i < Factories.Num(); i++ )
				{
					UFactory* TestFactory = Factories(i);
					if( TestFactory->FactoryCanImport( Filename ) )
					{
						Factory = TestFactory;
						break;
					}
				}
			}

			// Found or chosen a factory
			if( Factory )
			{
				if( ImportDialog.ShowModal( Filename, PackageName, GroupName, Factory->GetSupportedClass(), Factory, NULL ) == wxID_OK )
				{
					bSawSuccessfulImport = TRUE;
				}

				// Copy off the package and group for the next import.
				PackageName = ImportDialog.GetPackage();
				GroupName = ImportDialog.GetGroup();
			}
			else
			{
				// Couldn't find a factory for a class, so throw up an error message.
				appMsgf( AMT_OK, TEXT( "Unable to import file: %s\nCould not find an appropriate actor factory for this filetype." ), ( const TCHAR* )InFiles.Item( FileIndex ) );
			}
		}

		// Clean up the overwrite bit now that we're done importing files
		UFactory::ResetState();

		// Only update the generic browser if we imported something successfully.
		if ( bSawSuccessfulImport )
		{
			ImportDialog.SyncAssetViewToNewObjects();
		}

		return bSawSuccessfulImport;
	}

	/** Helper struct for bulk importing */
	struct FBulkImportInfo
	{
		FString FileToImport;
		FString ToPackageName;
		FString ToGroupName;

		FBulkImportInfo(const FString& File, const FString& Package, const FString& Group=TEXT("")) :
			FileToImport(File),
			ToPackageName(Package),
			ToGroupName(Group)
		{
		}
	};

	static UBOOL DoBulkImport( const TArray< FBulkImportInfo >& ImportData, const TArray< UFactory* >& InFactories )						
	{
		// Maintain a list of used factories to store settings during bulk importing so the user isn't asked each time.
		TArray< UFactory* > UsedFactories;

		// Return value.
		UBOOL bAllFilesImported = TRUE;

		INT NumFilesImported = 0;
		INT NumFilesToImport = ImportData.Num();

		// Import each file!
		for ( INT ImportIdx = 0; ImportIdx < NumFilesToImport; ++ImportIdx )
		{
			const FString& FileToImport = ImportData(ImportIdx).FileToImport;
			const FFilename Filename = FileToImport;
			const FString FileExtension = Filename.GetExtension();
			
			const FString& PackageName = ImportData(ImportIdx).ToPackageName;
			const FString& GroupName = ImportData(ImportIdx).ToGroupName;

			// The factory to use for importing
			UFactory* FactoryToUse = NULL;

			// Find factories supporting the file extension in question
			TArray<UFactory*> Factories;
			Factories.Empty(InFactories.Num());
			for( INT FactoryIdx = 0; FactoryIdx < InFactories.Num(); ++FactoryIdx )
			{
				UFactory* NextFactory = InFactories( FactoryIdx );
				for( INT FormatIndex = 0; FormatIndex < NextFactory->Formats.Num(); FormatIndex++ )
				{
					const FString& FactoryFormat = NextFactory->Formats( FormatIndex );
					if( FactoryFormat.Left( FileExtension.Len() ) == FileExtension )
					{
						Factories.AddItem( NextFactory );
						break;
					}
				}
			}

			// Handle the potential of multiple factories being found
			if( Factories.Num() == 1 )
			{
				FactoryToUse = Factories( 0 );
			}
			else if( Factories.Num() > 1 )
			{
				FactoryToUse = Factories(0);
				for( INT i = 0; i < Factories.Num(); i++ )
				{
					UFactory* TestFactory = Factories(i);
					if( TestFactory->FactoryCanImport( Filename ) )
					{
						FactoryToUse = TestFactory;
						break;
					}
				}
			}

			if( FactoryToUse )
			{
				// Try to see if the factory has already used. In bulk import mode, factory settings will be set from a dialog box the first time a file type is found.
				// After the file type has been found once, the settings are propagated to files of the same type. (Provided the user presses ok to all. Otherwise they are asked each time).
				if( UsedFactories.FindItemIndex( FactoryToUse ) != -1 )
				{
					// Factory has already been used.  Import the file.
					UPackage* Package = GEngine->CreatePackage(NULL, *PackageName);
					if( GroupName.Len() )
					{
						Package = GEngine->CreatePackage( Package, *GroupName );
					}

					UClass* Class = FactoryToUse->ResolveSupportedClass();
					check( Class );
					UObject *Result = UFactory::StaticImportObject(Class, Package, FName( *Filename.GetBaseFilename() ), RF_Public|RF_Standalone, *FileToImport, NULL, FactoryToUse );

					if( !Result )
					{
						GWarn->Logf( NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_CouldNotImportFile" ), *FileToImport ) );
						bAllFilesImported = FALSE;
					}
					else
					{
						Result->MarkPackageDirty(TRUE);
					}
				}
				else
				{
					// We have not used the factory yet.  Ask the user for the settings.
					WxDlgImportGeneric ImportDlg(FALSE, TRUE);

					INT Result = ImportDlg.ShowModal( Filename, PackageName, GroupName, FactoryToUse->GetSupportedClass(), FactoryToUse, NULL );
					if( Result == wxID_OK )
					{
						// If the user pressed ok to all they want to apply the factory settings to all imported files with the specific type
						// Add the factories to the list of factories in use
						if( ImportDlg.GetOKToAll() )
						{
							UsedFactories.AddUniqueItem( FactoryToUse );
						}

						// Find the package we just imported.  WxDlgImportGeneric doesn't store a reference to the package so it must be found manually
						UPackage *Package = UObject::FindPackage(NULL, *PackageName);
						if( !Package )
						{
							GWarn->Logf( NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_CouldNotImportFile" ), *FileToImport ) );
							bAllFilesImported = FALSE;
						}
					}
					else
					{
						bAllFilesImported = FALSE;
						if( Result == ID_CANCEL_ALL )
						{
							GWarn->Logf( NAME_Warning, *LocalizeUnrealEd( "BulkImport_Cancelled" ) );
							// break out of the import loop.  The user wants to cancel all imports.
							break;
						}
						else
						{
							GWarn->Logf( NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_FileCancelled" ), *Filename.GetCleanFilename() ) );
						}
					}
				}	
			}
			else
			{
				// Couldn't find a factory for a class, since this is bulk importing mode, we don't want to throw up a warning message since it would block operation.
				// Just write to the log
				GWarn->Logf( LocalizeSecure( LocalizeUnrealEd( "BulkImport_NoValidFactory" ), *FileToImport ) );
				bAllFilesImported = FALSE;
			}

			++NumFilesImported;
			GWarn->StatusUpdatef( NumFilesImported,  NumFilesToImport, *FString::Printf( LocalizeSecure(LocalizeUnrealEd( "Importingf" ), NumFilesImported, NumFilesToImport ) ) );
		}

		return bAllFilesImported;
	}

	UBOOL BulkImportFiles( const FString& InImportPath, const TArray<UFactory*>& InFactories )
	{
		FGlobalComponentReattachContext ReattachContext;

		// A mapping of package names to file names so the packages can be saved correctly
		TMap< FString, FString > PackageNameToPackageFileMap;

		// List of files to import and their mapping to packages/groups
		TArray< FBulkImportInfo > ImportData;

		FString CurrentPath = InImportPath;
	
		//Find the relative base folder name. 
		//This is the base folder where packages will be put. I.E: \Content\BaseFolderName
		INT LastSlashIdx = InImportPath.InStr( TEXT("\\"), TRUE );
		FString BaseFolderName = InImportPath.Mid( LastSlashIdx+1, InImportPath.Len()-LastSlashIdx );

		// True if invalid files exist in the directory structure we are parsing
		UBOOL bInvalidFilesExist = FALSE;

		GWarn->BeginSlowTask( *LocalizeUnrealEd( TEXT("Importing") ), TRUE );

		// Find all sub dir's inside the import path
		TArray< FString > SubDirs; 
		GFileManager->FindFiles( SubDirs, *(CurrentPath * TEXT("*.*")), FALSE, TRUE );

		for( INT SubDirIdx = 0; SubDirIdx < SubDirs.Num(); ++SubDirIdx )
		{
			const FString& SubDir = SubDirs( SubDirIdx );

			TArray< FString > PackageNames;

			//update the current path with the current sub directory
			CurrentPath = InImportPath*SubDir;
			GFileManager->FindFiles( PackageNames, *(CurrentPath * TEXT("*.*")), FALSE, TRUE );
			
			for( INT PkgIdx = 0; PkgIdx < PackageNames.Num(); ++PkgIdx )
			{
				const FString& PackageName = PackageNames( PkgIdx );

				// A String containing the reason why we could not import something.
				FString Reason;
			
				// Check for package names containing invalid characters.  (Like whitespace)
				if( !FIsValidGroupName( *PackageName, Reason, FALSE ) )
				{
					GWarn->Logf( NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_InvalidPackageSymbol" ), *PackageName, *Reason ) );
					bInvalidFilesExist = TRUE;

					// Skip this package because it has an invalid name
					continue;
				}

				TArray< FString > GroupNames;

				// Update the current path with the current package name directory
				CurrentPath = InImportPath * SubDir * PackageName;
				GFileManager->FindFiles( GroupNames, *(CurrentPath * TEXT("*.*")), FALSE, TRUE );

				// Whether or not we found any files in the directory.
				UBOOL bFoundAnyFiles = FALSE;
		
				// find files that may not be in a group
				TArray < FString > FoundFiles;
				GFileManager->FindFiles( FoundFiles, *(CurrentPath * TEXT("*.*")), TRUE, FALSE );

				if( FoundFiles.Num() > 0 )
				{
					// Iterate through the files found
					for( INT FileIdx = 0; FileIdx < FoundFiles.Num(); ++FileIdx)
					{
						const FString& Filename = FoundFiles( FileIdx );

						// FFilename will turn the string we need into just the filename without paths or extensions.
						FFilename FileToCheck = Filename;
						if( !FIsValidObjectName( *FileToCheck.GetBaseFilename(), Reason ) )
						{
							GWarn->Logf(NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_InvalidFileSymbol" ), *FileToCheck.GetCleanFilename(), *Reason ) );
							bInvalidFilesExist = TRUE;

							// this file contains invalid symbols
							continue;
						}

						bFoundAnyFiles = TRUE;

						// FindFiles does not keep the absolute path
						ImportData.AddItem( FBulkImportInfo( CurrentPath*Filename, PackageName ) );
					}
				}

				// there are groups, iterate through them finding files
				for( INT GroupIdx = 0; GroupIdx < GroupNames.Num(); ++GroupIdx )
				{
					const FString& GroupName = GroupNames( GroupIdx );

					if( !FIsValidGroupName( *GroupName, Reason, TRUE ) )
					{
						GWarn->Logf( NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_InvalidGroupSymbol" ), *GroupName, *Reason ) );
						bInvalidFilesExist = TRUE;

						// this group contains invalid symbols
						continue;
					}

					CurrentPath = InImportPath * SubDir * PackageName * GroupName;

					FoundFiles.Empty();
					appFindFilesInDirectory( FoundFiles, *CurrentPath, FALSE, TRUE );

					if( FoundFiles.Num() > 0 )
					{
						for( INT FileIdx = 0; FileIdx < FoundFiles.Num(); ++FileIdx )
						{
							const FString& Filename = FoundFiles( FileIdx );

							//FFilename will turn the string we need into just the filename without paths or extensions.
							FFilename FileToCheck = Filename;
							if( !FIsValidObjectName( *FileToCheck.GetBaseFilename(), Reason ) )
							{
								
								GWarn->Logf( NAME_Warning, LocalizeSecure( LocalizeUnrealEd( "BulkImport_InvalidFileSymbol" ), *FileToCheck.GetCleanFilename(), *Reason ) );
								bInvalidFilesExist = TRUE;
								// this file contains invalid symbols
								continue;
							}

							bFoundAnyFiles = TRUE;
							ImportData.AddItem( FBulkImportInfo( Filename, PackageName, GroupName ) );
						}
					}
				}
				
				// If any files were found in this package directory, add it to the mapping so we can save the package.
				if( bFoundAnyFiles )
				{
					PackageNameToPackageFileMap.Set( PackageName, appGameDir()*TEXT("Content")*BaseFolderName*SubDir*PackageName+TEXT(".upk") );
				}
			}
		}

		// all files imported successfully if no errors happened in DoBulkImport and if no invalid files exist.
		UBOOL bAllFilesImported = DoBulkImport( ImportData, InFactories ) && !bInvalidFilesExist;
		
		// Save each package.  
		for( TMap<FString, FString>::TConstIterator PkgMapIter(PackageNameToPackageFileMap); PkgMapIter; ++PkgMapIter )
		{
			GUnrealEd->Exec( *FString::Printf( TEXT("OBJ SAVEPACKAGE PACKAGE=\"%s\" FILE=\"%s\""), *PkgMapIter.Key(), *PkgMapIter.Value() ) );
		}

		GWarn->EndSlowTask();

		return bAllFilesImported;
	}

	

	/*-----------------------------------------------------------------------------
		WxDlgExportGeneric.
	-----------------------------------------------------------------------------*/

	BEGIN_EVENT_TABLE(WxDlgExportGeneric, wxDialog)
		EVT_BUTTON( wxID_OK, WxDlgExportGeneric::OnOK )
	END_EVENT_TABLE()


	WxDlgExportGeneric::WxDlgExportGeneric()
	{
		const bool bSuccess = wxXmlResource::Get()->LoadDialog( this, GApp->EditorFrame, TEXT("ID_DLG_GENERIC_EXPORT") );
		check( bSuccess );

		NameText = (wxTextCtrl*)FindWindow( XRCID( "IDEC_NAME" ) );
		check( NameText != NULL );

		// Replace the placeholder window with a property window

		PropertyWindow = new WxPropertyWindowHost;
		PropertyWindow->Create( this, GUnrealEd );
		wxWindow* win = (wxWindow*)FindWindow( XRCID( "ID_PROPERTY_WINDOW" ) );
		check( win != NULL );
		const wxRect rc = win->GetRect();
		PropertyWindow->SetSize( rc );
		win->Show(0);

		Exporter = NULL;

		FWindowUtil::LoadPosSize( TEXT("DlgExportGeneric"), this );

		FLocalizeWindow( this );
	}

	WxDlgExportGeneric::~WxDlgExportGeneric()
	{
		FWindowUtil::SavePosSize( TEXT("DlgExportGeneric"), this );
	}

	/**
	 * Performs the export.  Modal.
	 *
	 * @param	InFilename	The filename to export the object as.
	 * @param	InObject	The object to export.
	 * @param	InExporter	The exporter to use.
	 * @param	bPrompt		If TRUE, display the export dialog and property window.
	 */
	int WxDlgExportGeneric::ShowModal(const FFilename& InFilename, UObject* InObject, UExporter* InExporter, UBOOL bPrompt)
	{
		Filename = InFilename;
		Exporter = InExporter;
		Object = InObject;

		if ( bPrompt )
		{
			NameText->SetLabel( *Object->GetFullName() );
			NameText->SetEditable(false);

			PropertyWindow->SetObject( Exporter, EPropertyWindowFlags::Sorted );
			//PropertyWindow->SetSize(200,200);
			Refresh();

			return wxDialog::ShowModal();
		}
		else
		{
			DoExport();
			return wxID_OK;
		}
	}

	void WxDlgExportGeneric::OnOK( wxCommandEvent& In )
	{
		DoExport();
		EndModal( wxID_OK );
	}

	void WxDlgExportGeneric::DoExport()
	{
		const FScopedBusyCursor BusyCursor;
		//UExporter::ExportToFile( Object, Exporter, *Filename, FALSE );

		UExporter::FExportToFileParams Params;
		Params.Object = Object;
		Params.Exporter = Exporter;
		Params.Filename = *Filename;
		Params.InSelectedOnly = FALSE;
		Params.NoReplaceIdentical = FALSE;
		Params.Prompt = FALSE;
		Params.bUseFileArchive = Object->IsA(UPackage::StaticClass());
		Params.WriteEmptyFiles = FALSE;
		UExporter::ExportToFileEx(Params);
	}


	/**
	 * Exports the specified objects to file.
	 *
	 * @param	ObjectsToExport					The set of objects to export.
	 * @param	bPromptIndividualFilenames		If TRUE, prompt individually for filenames.  If FALSE, bulk export to a single directory.
	 * @param	ExportPath						receives the value of the path the user chose for exporting.
	 * @param	bUseProvidedExportPath			If TRUE and out_ExportPath is specified, use the value in out_ExportPath as the export path w/o prompting for a directory when applicable
	 */
	void ExportObjects(const TArray<UObject*>& ObjectsToExport, UBOOL bPromptIndividualFilenames, FString* ExportPath/*=NULL*/, UBOOL bUseProvidedExportPath /*= FALSE*/ )
	{
		// @todo CB: Share this with the rest of the editor (see GB's use of this)
		FString LastExportPath = ExportPath != NULL ? *ExportPath : GApp->LastDir[LD_GENERIC_EXPORT];

		if ( ObjectsToExport.Num() == 0 )
		{
			return;
		}

		// Disallow export from cooked packages.
		for( INT Index = 0 ; Index < ObjectsToExport.Num() ; ++Index )
		{
			if( ObjectsToExport(Index)->GetOutermost()->PackageFlags & PKG_Cooked )
			{
				appMsgf( AMT_OK, *LocalizeUnrealEd("Error_OperationDisallowedOnCookedContent") );
				return;
			}
		}

		FFilename SelectedExportPath;
		UBOOL bExportGroupsAsSubDirs = FALSE;
		if ( !bPromptIndividualFilenames )
		{
			if ( !bUseProvidedExportPath || !ExportPath )
			{
				// If not prompting individual files, prompt the user to select a target directory.
				wxDirDialog ChooseDirDialog(
					GApp->EditorFrame,
					*LocalizeUnrealEd("ChooseADirectory"),
					*LastExportPath
					);

				if ( ChooseDirDialog.ShowModal() != wxID_OK )
				{
					return;
				}
				SelectedExportPath = FFilename( ChooseDirDialog.GetPath() );
			}
			else if ( bUseProvidedExportPath )
			{
				SelectedExportPath = *ExportPath;
			}

			// Copy off the selected path for future export operations.
			LastExportPath = SelectedExportPath;

			// Prompt the user if group contents should be exported to their own subdirectories.
			bExportGroupsAsSubDirs = appMsgf( AMT_YesNo, *LocalizeUnrealEd(TEXT("Prompt_ExportGroupsAsSubDirs")) );
		}

		GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("Exporting")), TRUE );

		// Create an array of all available exporters.
		TArray<UExporter*> Exporters;
		AssembleListOfExporters( Exporters );

		// Export the objects.
		UBOOL bAnyObjectMissingSourceData = FALSE;
		for (INT Index = 0; Index < ObjectsToExport.Num(); Index++)
		{
			GWarn->StatusUpdatef( Index, ObjectsToExport.Num(), *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Exportingf"), Index, ObjectsToExport.Num()) ) );

			UObject* ObjectToExport = ObjectsToExport(Index);
			if ( !ObjectToExport )
			{
				continue;
			}

			// Can't export cooked content.
			const UPackage* ObjectPackage = ObjectToExport->GetOutermost();
			if( ObjectPackage->PackageFlags & PKG_Cooked )
			{
				return;
			}

			//For mod creators, the source art will be stripped out
#if UDK
			if (ObjectPackage->PackageFlags & PKG_NoExportAllowed)
			{
				bAnyObjectMissingSourceData = TRUE;
				warnf(NAME_Warning, TEXT("Source data missing for '%s'"), *ObjectToExport->GetName());
				continue;
			}
#endif

			// Find all the exporters that can export this type of object and construct an export file dialog.
			FString FileTypes;
			FString AllExtensions;
			FString FirstExtension;

			// Iterate in reverse so the most relevant file formats are considered first.
			for( INT ExporterIndex = Exporters.Num()-1 ; ExporterIndex >=0 ; --ExporterIndex )
			{
				UExporter* Exporter = Exporters(ExporterIndex);
				if( Exporter->SupportedClass )
				{
					const UBOOL bObjectIsSupported = ObjectToExport->IsA( Exporter->SupportedClass );
					if ( bObjectIsSupported )
					{
						// Seed the default extension with the exporter's preferred format.
						if ( FirstExtension.Len() == 0 && Exporter->PreferredFormatIndex < Exporter->FormatExtension.Num() )
						{
							FirstExtension = Exporter->FormatExtension(Exporter->PreferredFormatIndex);
						}

						// Get a string representing of the exportable types.
						check( Exporter->FormatExtension.Num() == Exporter->FormatDescription.Num() );
						for( INT FormatIndex = Exporter->FormatExtension.Num()-1 ; FormatIndex >= 0 ; --FormatIndex )
						{
							const FString& FormatExtension = Exporter->FormatExtension(FormatIndex);
							const FString& FormatDescription = Exporter->FormatDescription(FormatIndex);

							if ( FirstExtension.Len() == 0 )
							{
								FirstExtension = FormatExtension;
							}
							if( FileTypes.Len() )
							{
								FileTypes += TEXT("|");
							}
							FileTypes += FString::Printf( TEXT("%s (*.%s)|*.%s"), *FormatDescription, *FormatExtension, *FormatExtension );

							if( AllExtensions.Len() )
							{
								AllExtensions += TEXT(";");
							}
							AllExtensions += FString::Printf( TEXT("*.%s"), *FormatExtension );
						}
					}
				}
			}

			// Skip this object if no exporter found for this resource type.
			if ( FirstExtension.Len() == 0 )
			{
				continue;
			}

			FileTypes = FString::Printf(TEXT("%s|All Files (%s)|%s"),*FileTypes, *AllExtensions, *AllExtensions);

			FFilename SaveFileName;
			if ( bPromptIndividualFilenames )
			{
				// Open dialog so user can chose save filename.
				WxFileDialog SaveFileDialog( GApp->EditorFrame, 
					*FString::Printf( LocalizeSecure(LocalizeUnrealEd("Save_F"), *ObjectToExport->GetName()) ),
					*LastExportPath,
					*ObjectToExport->GetName(),
					*FileTypes,
					wxSAVE,
					wxDefaultPosition);

				if( SaveFileDialog.ShowModal() != wxID_OK )
				{
					INT NumObjectsLeftToExport = ObjectsToExport.Num() - Index - 1;
					if( NumObjectsLeftToExport > 0 )
					{
						FString ConfirmText = FString::Printf( LocalizeSecure( LocalizeUnrealEd( TEXT( "ObjectTools_ExportObjects_CancelRemaining" ) ), NumObjectsLeftToExport ) );
						if( wxYES == wxMessageBox( wxString( *ConfirmText ), wxString( *LocalizeUnrealEd( TEXT( "ObjectTools_ExportObjects" ) ) ), wxYES_NO ) )
						{
							break;
						}
					}
					continue;
				}
				SaveFileName = FFilename( SaveFileDialog.GetPath() );

				// Copy off the selected path for future export operations.
				LastExportPath = SaveFileName;
			}
			else
			{
				// Assemble a filename from the export directory and the object path.
				SaveFileName = SelectedExportPath;
				if ( bExportGroupsAsSubDirs )
				{
					// Assemble a path from the complete outer chain.
					GetDirectoryFromObjectPath( ObjectToExport, SaveFileName );
				}
				else
				{
					// Assemble a path only from the package name.
					SaveFileName *= ObjectToExport->GetOutermost()->GetName();
					SaveFileName *= ObjectToExport->GetName();
				}
				SaveFileName += FString::Printf( TEXT(".%s"), *FirstExtension );
				debugf(TEXT("Exporting \"%s\" to \"%s\""), *ObjectToExport->GetPathName(), *SaveFileName );
			}

			// Create the path, then make sure the target file is not read-only.
			const FString ObjectExportPath( SaveFileName.GetPath() );
			const UBOOL bFileInSubdirectory = ( ObjectExportPath.InStr( TEXT("\\") ) != -1 );
			if ( bFileInSubdirectory && ( !GFileManager->MakeDirectory( *ObjectExportPath, TRUE ) ) )
			{
				appMsgf( AMT_OK, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_FailedToMakeDirectory"), *ObjectExportPath)) );
			}
			else if( GFileManager->IsReadOnly( *SaveFileName ) )
			{
				appMsgf( AMT_OK, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("Error_CouldntWriteToFile_F"), *SaveFileName)) );
			}
			else
			{
				// We have a writeable file.  Now go through that list of exporters again and find the right exporter and use it.
				TArray<UExporter*>	ValidExporters;

				for( INT ExporterIndex = 0 ; ExporterIndex < Exporters.Num(); ++ExporterIndex )
				{
					UExporter* Exporter = Exporters(ExporterIndex);
					if( Exporter->SupportedClass && ObjectToExport->IsA(Exporter->SupportedClass) )
					{
						check( Exporter->FormatExtension.Num() == Exporter->FormatDescription.Num() );
						for( INT FormatIndex = 0 ; FormatIndex < Exporter->FormatExtension.Num() ; ++FormatIndex )
						{
							const FString& FormatExtension = Exporter->FormatExtension(FormatIndex);
							if(	appStricmp( *FormatExtension, *SaveFileName.GetExtension() ) == 0 ||
								appStricmp( *FormatExtension, TEXT("*") ) == 0 )
							{
								ValidExporters.AddItem( Exporter );
								break;
							}
						}
					}
				}

				// Handle the potential of multiple exporters being found
				UExporter* ExporterToUse = NULL;
				if( ValidExporters.Num() == 1 )
				{
					ExporterToUse = ValidExporters( 0 );
				}
				else if( ValidExporters.Num() > 1 )
				{
					// Set up the first one as default
					ExporterToUse = ValidExporters( 0 );

					// ...but search for a better match if available
					for( INT ExporterIdx = 0; ExporterIdx < ValidExporters.Num(); ExporterIdx++ )
					{
						if( ValidExporters( ExporterIdx )->GetClass()->GetFName() == ObjectToExport->GetExporterName() )
						{
							ExporterToUse = ValidExporters( ExporterIdx );
							break;
						}
					}
				}

				// If an exporter was found, use it.
				if( ExporterToUse )
				{
					WxDlgExportGeneric dlg;
					dlg.ShowModal( SaveFileName, ObjectToExport, ExporterToUse, FALSE );
				}
			}
		}

		if (bAnyObjectMissingSourceData)
		{
			appMsgf( AMT_OK, *FString::Printf( *LocalizeUnrealEd("Exporter_Error_SourceDataUnavailable")) );
		}

		GWarn->EndSlowTask();

		if ( ExportPath != NULL )
		{
			*ExportPath = LastExportPath;
		}
	}

	/**
	 * Tags objects which are in use by levels specified by the search option 
	 *
	 * @param SearchOption	 The search option for finding in use objects
	 */
	void TagInUseObjects( EInUseSearchOption SearchOption )
	{
		TSet<UObject*> LevelPackages;
		TSet<UObject*> Levels;

		if( !GWorld )
		{
			// Don't do anything if there is no GWorld.  This could be called during a level load transition
			return;
		}

		AWorldInfo* WorldInfo = GWorld->GetWorldInfo();

		switch( SearchOption )
		{
		case SO_CurrentLevel:
			LevelPackages.Add( GWorld->CurrentLevel->GetOutermost() );
			Levels.Add( GWorld->CurrentLevel );
			break;
		case SO_VisibleLevels:
			// Add the persistent level if its visible
			if( FLevelUtils::IsLevelVisible( GWorld->PersistentLevel ) ) 
			{
				LevelPackages.Add( GWorld->PersistentLevel->GetOutermost() );
				Levels.Add( GWorld->PersistentLevel );
			}
			// Add all other levels if they are visible
			for( INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );

				if( StreamingLevel != NULL && StreamingLevel->LoadedLevel != NULL && FLevelUtils::IsLevelVisible( StreamingLevel ) )
				{
					LevelPackages.Add( StreamingLevel->LoadedLevel->GetOutermost() );
					Levels.Add( StreamingLevel->LoadedLevel );
				}
			}
			break;
		case SO_LoadedLevels:
			// Add the persistent level as its always loaded
			LevelPackages.Add( GWorld->PersistentLevel->GetOutermost() );
			Levels.Add( GWorld->PersistentLevel );
			
			// Add all other levels
			for( INT LevelIndex = 0; LevelIndex < WorldInfo->StreamingLevels.Num(); ++LevelIndex )
			{
				ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels( LevelIndex );

				if( StreamingLevel != NULL && StreamingLevel->LoadedLevel != NULL )
				{
					LevelPackages.Add( StreamingLevel->LoadedLevel->GetOutermost() );
					Levels.Add( StreamingLevel->LoadedLevel );
				}
			}
			break;
		default:
			// A bad option was passed in.
			check(0);
		}	

		TArray<UObject*> ObjectsInLevels;

		for( FObjectIterator It; It; ++It )
		{
			UObject* Obj = *It;

			// Clear all marked flags that could have been tagged in a previous search or by another system.
			Obj->ClearFlags(RF_TagExp|RF_TagImp);

		
			// If the object is not flagged for GC and it is in one of the level packages do an indepth search to see what references it.
			if( !Obj->HasAnyFlags( RF_PendingKill | RF_Unreachable ) && LevelPackages.Find( Obj->GetOutermost() ) != NULL )
			{
				// Determine if the current object is in one of the search levels.  This is the same as UObject::IsIn except that we can
				// search through many levels at once.
				for ( UObject* ObjectOuter = Obj->GetOuter(); ObjectOuter; ObjectOuter = ObjectOuter->GetOuter() )
				{
					if ( Levels.Find(ObjectOuter) != NULL )
					{
						// this object was contained within one of our ReferenceRoots
						ObjectsInLevels.AddItem( Obj );
						break;
					}
				}
			}
			else if( Obj->IsA( AWorldInfo::StaticClass() ) )
			{
				// If a skipped object is a world info ensure it is not serialized because it may contain 
				// references to levels (and by extension, their actors) that we are not searching for references to.
				Obj->SetFlags( RF_TagImp );
			}
		}

		// Tag all objects that are referenced by objects in the levels were are searching.
		FArchiveReferenceMarker Marker( ObjectsInLevels );
	}
}





namespace ThumbnailTools
{

	/** Renders a thumbnail for the specified object */
	void RenderThumbnail( UObject* InObject, const UINT InImageWidth, const UINT InImageHeight, EThumbnailTextureFlushMode::Type InFlushMode, FObjectThumbnail& OutThumbnail )
	{
		// Renderer must be initialized before generating thumbnails
		check( GIsRHIInitialized );

		// Store dimensions
		OutThumbnail.SetImageSize( InImageWidth, InImageHeight );

		const UINT MinRenderTargetSize = Max( InImageWidth, InImageHeight );
		UTextureRenderTarget2D* RenderTargetTexture = GEditor->GetScratchRenderTarget( MinRenderTargetSize );
		check( RenderTargetTexture != NULL );

		// Make sure the input dimensions are OK.  The requested dimensions must be less than or equal to
		// our scratch render target size.
		check( InImageWidth <= RenderTargetTexture->GetSurfaceWidth() );
		check( InImageHeight <= RenderTargetTexture->GetSurfaceHeight() );


		// Grab the actual render target resource from the texture.  Note that we're absolutely NOT ALLOWED to
		// dereference this pointer.  We're just passing it along to other functions that will use it on the render
		// thread.  The only thing we're allowed to do is check to see if it's NULL or not.
		FTextureRenderTargetResource* RenderTargetResource = RenderTargetTexture->GameThread_GetRenderTargetResource();
		check( RenderTargetResource != NULL );

		// Manually call RHIBeginScene since we are issuing draw calls outside of the main rendering function
		ENQUEUE_UNIQUE_RENDER_COMMAND(
			BeginCommand,
		{
			RHIBeginScene();
		});

		// Create a canvas for the render target and clear it to black
		FCanvas Canvas( RenderTargetResource, NULL );
		Clear( &Canvas, FLinearColor::Black );


		// Get the rendering info for this object
		FThumbnailRenderingInfo* RenderInfo =
			GUnrealEd->GetThumbnailManager()->GetRenderingInfo( InObject );



		// Wait for all textures to be streamed in before we render the thumbnail
		// @todo CB: This helps but doesn't result in 100%-streamed-in resources every time! :(
		if( InFlushMode == EThumbnailTextureFlushMode::AlwaysFlush )
		{
	 		UObject::FlushAsyncLoading();

			GStreamingManager->StreamAllResources( FALSE, 100.0f );
		}

		// If this object's thumbnail will be rendered to a texture on the GPU.
		UBOOL bUseGPUGeneratedThumbnail = TRUE;

		if( RenderInfo != NULL && RenderInfo->Renderer != NULL )
		{
			const FLOAT ZoomFactor = 1.0f;
			const EThumbnailPrimType ThumbnailPrimType = TPT_Plane;

			const UBOOL bWantLabels = FALSE;

			// Find how big the thumbnail WANTS to be
			DWORD DesiredWidth = 0;
			DWORD DesiredHeight = 0;
			{
				// Currently we only allow textures/icons (and derived classes) to override our desired size
				// @todo CB: Some thumbnail renderers (like particles and lens flares) hard code their own
				//	   arbitrary thumbnail size even though they derive from TextureThumbnailRenderer
				if( RenderInfo->Renderer->IsA( UTextureThumbnailRenderer::StaticClass() ) ||
					RenderInfo->Renderer->IsA( UIconThumbnailRenderer::StaticClass() ) )
				{
					RenderInfo->Renderer->GetThumbnailSize(
						InObject,
						ThumbnailPrimType,
						ZoomFactor,
						DesiredWidth,		// Out
						DesiredHeight );	// Out
				}
			}

			// Does this thumbnail have a size associated with it?  Materials and textures often do!
			if( DesiredWidth > 0 && DesiredHeight > 0 )
			{
				// Scale the desired size down if it's too big, preserving aspect ratio
				if( DesiredWidth > InImageWidth )
				{
					DesiredHeight = ( DesiredHeight * InImageWidth ) / DesiredWidth;
					DesiredWidth = InImageWidth;
				}
				if( DesiredHeight > InImageHeight )
				{
					DesiredWidth = ( DesiredWidth * InImageHeight ) / DesiredHeight;
					DesiredHeight = InImageHeight;
				}

				// Update dimensions
				OutThumbnail.SetImageSize( Max<DWORD>(1, DesiredWidth), Max<DWORD>(1, DesiredHeight) );
			}
			
			if( RenderInfo->Renderer->SupportsCPUGeneratedThumbnail( InObject ) )
			{
				// This object's should be rendered on the CPU.
				bUseGPUGeneratedThumbnail = FALSE;
				// Copy data for thumbnail rendering directly to the thumbnail.
				RenderInfo->Renderer->DrawCPU( InObject, OutThumbnail );

			}
			else
			{
				// This objects thumbnail should be rendered on the GPU

				// Draw the thumbnail
				const INT XPos = 0;
				const INT YPos = 0;
				RenderInfo->Renderer->Draw(
					InObject,
					ThumbnailPrimType,
					XPos,
					YPos,
					OutThumbnail.GetImageWidth(),
					OutThumbnail.GetImageHeight(),
					RenderTargetResource,
					&Canvas,
					TBT_None,
					GEditor->GetUserSettings().PreviewThumbnailBackgroundColor,
					GEditor->GetUserSettings().PreviewThumbnailTranslucentMaterialBackgroundColor);



				if( bWantLabels )
				{
					// This is the object that will render the labels
					UThumbnailLabelRenderer* LabelRenderer = RenderInfo->LabelRenderer;

					// Setup thumbnail options
					UThumbnailLabelRenderer::ThumbnailOptions ThumbnailOptions;

					// @todo CB: Support this? ==post APR09 QA build
					UMemCountThumbnailLabelRenderer* MemCountLabelRenderer = NULL;

					// Handle the case where the user wants memory information too
					// Use the memory counting label renderer to append the information
					if (MemCountLabelRenderer != NULL)
					{
						// Assign the aggregated label renderer
						MemCountLabelRenderer->AggregatedLabelRenderer = LabelRenderer;
						LabelRenderer = MemCountLabelRenderer;
					}

					// See if there is a registered label renderer
					DWORD LabelsWidth = 0;
					DWORD LabelsHeight = 0;
					if (LabelRenderer)
					{
						// Get the size of the labels
						LabelRenderer->GetThumbnailLabelSize(
							InObject,
							GEngine->SmallFont,
							&Canvas,
							ThumbnailOptions,
							LabelsWidth,
							LabelsHeight );
					}
					// See if there is a registered label renderer
					if (LabelRenderer != NULL)
					{
						// Now render the labels for this thumbnail
						LabelRenderer->DrawThumbnailLabels(
							InObject,
							GEngine->SmallFont,XPos,YPos + InImageHeight, &Canvas, ThumbnailOptions);
					}

					// Zero any ref since it isn't needed anymore
					if (MemCountLabelRenderer != NULL)
					{
						MemCountLabelRenderer->AggregatedLabelRenderer = NULL;
					}
				}
			}
		}

		// GPU based thumbnail rendering only
		if( bUseGPUGeneratedThumbnail )
		{
			// Tell the rendering thread to draw any remaining batched elements
			Canvas.Flush();


			{
				ENQUEUE_UNIQUE_RENDER_COMMAND_ONEPARAMETER(
					UpdateThumbnailRTCommand,
					FTextureRenderTargetResource*, RenderTargetResource, RenderTargetResource,
				{
					// Copy (resolve) the rendered thumbnail from the render target to its texture
					RHICopyToResolveTarget(
						RenderTargetResource->GetRenderTargetSurface(),		// Source texture
						FALSE,												// Do we need the source image content again?
						FResolveParams() );									// Resolve parameters
				});



				// Copy the contents of the remote texture to system memory
				// NOTE: OutRawImageData must be a preallocated buffer!
				RenderTargetResource->ReadPixels(
					OutThumbnail.AccessImageData(),		// Out: Image data
					FReadSurfaceDataFlags(),			// Cube face index
					0,									// Top left X
					0,									// Top left Y
					OutThumbnail.GetImageWidth(),		// Width
					OutThumbnail.GetImageHeight() );	// Height
			}
		}

		ENQUEUE_UNIQUE_RENDER_COMMAND(
			BeginCommand,
		{
			RHIEndScene();
		});
	}


	/** Generates a thumbnail for the specified object and caches it */
	FObjectThumbnail* GenerateThumbnailForObject( UObject* InObject )
	{
		// Does the object support thumbnails?
		if( GUnrealEd->GetThumbnailManager()->GetRenderingInfo( InObject ) != NULL )
		{
			// Set the size of cached thumbnails
			const INT ImageWidth = 	ThumbnailTools::DefaultThumbnailSize;
			const INT ImageHeight = ThumbnailTools::DefaultThumbnailSize;

			// For cached thumbnails we want to make sure that textures are fully streamed in so that the
			// thumbnail we're saving won't have artifacts
			ThumbnailTools::EThumbnailTextureFlushMode::Type TextureFlushMode = ThumbnailTools::EThumbnailTextureFlushMode::AlwaysFlush;

			// Generate the thumbnail
			FObjectThumbnail NewThumbnail;
			ThumbnailTools::RenderThumbnail(
				InObject, ImageWidth, ImageHeight, TextureFlushMode,
				NewThumbnail );		// Out

			UPackage* MyOutermostPackage = CastChecked< UPackage >( InObject->GetOutermost() );
			return CacheThumbnail( InObject->GetFullName(), &NewThumbnail, MyOutermostPackage );
		}

		return NULL;
	}

	/**
	 * Caches a thumbnail into a package's thumbnail map.
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	Thumbnail		the thumbnail to cache; specify NULL to remove the current cached thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 *
	 * @return	pointer to the thumbnail data that was cached into the package
	 */
	FObjectThumbnail* CacheThumbnail( const FString& ObjectFullName, FObjectThumbnail* Thumbnail, UPackage* DestPackage )
	{
		FObjectThumbnail* Result = NULL;

		if ( ObjectFullName.Len() > 0 && DestPackage != NULL )
		{
			// Create a new thumbnail map if we don't have one already
			if( !DestPackage->ThumbnailMap.IsValid() )
			{
				DestPackage->ThumbnailMap.Reset( new FThumbnailMap() );
			}

			// @todo thumbnails: Backwards compat
			FName ObjectFullNameFName( *ObjectFullName );
			FObjectThumbnail* CachedThumbnail = DestPackage->ThumbnailMap->Find( ObjectFullNameFName );
			if ( Thumbnail != NULL )
			{
				// Cache the thumbnail (possibly replacing an existing thumb!)
				Result = &DestPackage->ThumbnailMap->Set( ObjectFullNameFName, *Thumbnail );
			}
			//only let thumbnails loaded from disk to be removed.  
			//When capturing thumbnails from the content browser, it will only exist in memory until it is saved out to a package.
			//Don't let the recycling purge them
			else if ((CachedThumbnail != NULL) && (CachedThumbnail->IsLoadedFromDisk()))
			{
				DestPackage->ThumbnailMap->RemoveKey( ObjectFullNameFName );
			}
		
		}

		return Result;
	}



	/**
	 * Caches an empty thumbnail entry
	 *
	 * @param	ObjectFullName	the full name for the object to associate with the thumbnail
	 * @param	DestPackage		the package that will hold the cached thumbnail
	 */
	void CacheEmptyThumbnail( const FString& ObjectFullName, UPackage* DestPackage )
	{
		FObjectThumbnail EmptyThumbnail;
		CacheThumbnail( ObjectFullName, &EmptyThumbnail, DestPackage );
	}



	UBOOL QueryPackageFileNameForObject( const FString& InFullName, FString& OutPackageFileName )
	{
		// First strip off the class name
		INT FirstSpaceIndex = InFullName.InStr( TEXT( " " ) );
		if( FirstSpaceIndex == INDEX_NONE || FirstSpaceIndex <= 0 )
		{
			// Malformed full name
			return FALSE;
		}

		// Determine the package file path/name for the specified object
		FString ObjectPathName = InFullName.Mid( FirstSpaceIndex + 1 );

		// Pull the package out of the fully qualified object path
		INT FirstDotIndex = ObjectPathName.InStr( TEXT( "." ) );
		if( FirstDotIndex == INDEX_NONE || FirstDotIndex <= 0 )
		{
			// Malformed object path
			return FALSE;
		}

		FString PackageName = ObjectPathName.Left( FirstDotIndex );

		// Ask the package file cache for the full path to this package
		if( !GPackageFileCache->FindPackageFile( *PackageName, NULL, OutPackageFileName ) )
		{
			// Couldn't find the package in our cache
			return FALSE;
		}

		return TRUE;
	}



	/** Searches for an object's thumbnail in memory and returns it if found */
	FObjectThumbnail* FindCachedThumbnailInPackage( UPackage* InPackage, const FName InObjectFullName )
	{
		FObjectThumbnail* FoundThumbnail = NULL;

		// We're expecting this to be an outermost package!
		check( InPackage->GetOutermost() == InPackage );

		// Does the package have any thumbnails?
		if( InPackage->HasThumbnailMap() )
		{
			// @todo thumbnails: Backwards compat
			FThumbnailMap& PackageThumbnailMap = InPackage->AccessThumbnailMap();
			FoundThumbnail = PackageThumbnailMap.Find( InObjectFullName );
		}

		return FoundThumbnail;
	}



	/** Searches for an object's thumbnail in memory and returns it if found */
	FObjectThumbnail* FindCachedThumbnailInPackage( const FString& InPackageFileName, const FName InObjectFullName )
	{
		FObjectThumbnail* FoundThumbnail = NULL;

		// First check to see if the package is already in memory.  If it is, some or all of the thumbnails
		// may already be loaded and ready.
		UObject* PackageOuter = NULL;
		UPackage* Package = UObject::FindPackage( PackageOuter, *FPackageFileCache::PackageFromPath( *InPackageFileName ) );
		if( Package != NULL )
		{
			FoundThumbnail = FindCachedThumbnailInPackage( Package, InObjectFullName );
		}

		return FoundThumbnail;
	}



	/** Searches for an object's thumbnail in memory and returns it if found */
	const FObjectThumbnail* FindCachedThumbnail( const FString& InFullName )
	{
		// Determine the package file path/name for the specified object
		FString PackageFilePathName;
		if( !QueryPackageFileNameForObject( InFullName, PackageFilePathName ) )
		{
			// Couldn't find the package in our cache
			return NULL;
		}

		return FindCachedThumbnailInPackage( PackageFilePathName, FName( *InFullName ) );
	}



	/** Returns the thumbnail for the specified object or NULL if one doesn't exist yet */
	FObjectThumbnail* GetThumbnailForObject( UObject* InObject )
	{
		UPackage* ObjectPackage = CastChecked< UPackage >( InObject->GetOutermost() );
		return FindCachedThumbnailInPackage( ObjectPackage, FName( *InObject->GetFullName() ) );
	}



	/** Loads thumbnails from the specified package file name */
	UBOOL LoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails )
	{
		// Create a file reader to load the file
		TScopedPointer< FArchive > FileReader( GFileManager->CreateFileReader( *InPackageFileName ) );
		if( FileReader == NULL )
		{
			// Couldn't open the file
			return FALSE;
		}


		// Read package file summary from the file
		FPackageFileSummary FileSummary;
		(*FileReader) << FileSummary;


		// Make sure this is indeed a package
		if( FileSummary.Tag != PACKAGE_FILE_TAG )
		{
			// Unrecognized or malformed package file
			return FALSE;
		}

		
		// Make sure the package was created with a version of the engine that supports thumbnails
		if( FileSummary.GetFileVersion() < VER_ASSET_THUMBNAILS_IN_PACKAGES )
		{
			// Package was created by an older version of the engine; no thumbnails to load!
			return FALSE;
		}

		
		// Does the package contains a thumbnail table?
		if( FileSummary.ThumbnailTableOffset == 0 )
		{
			// No thumbnails to be loaded
			return FALSE;
		}

		
		// Seek the the part of the file where the thumbnail table lives
		FileReader->Seek( FileSummary.ThumbnailTableOffset );

		//make sure the filereader gets the corect version number (it defaults to latest version)
		FileReader->SetVer(FileSummary.GetFileVersion());

		INT LastFileOffset = -1;
		// Load the thumbnail table of contents
		TMap< FName, INT > ObjectNameToFileOffsetMap;
		{
			// Load the thumbnail count
			INT ThumbnailCount = 0;
			*FileReader << ThumbnailCount;

			// Load the names and file offsets for the thumbnails in this package
			for( INT CurThumbnailIndex = 0; CurThumbnailIndex < ThumbnailCount; ++CurThumbnailIndex )
			{
				bool bHaveValidClassName = FALSE;
				FString ObjectClassName;
				if( FileSummary.GetFileVersion() >= VER_CONTENT_BROWSER_FULL_NAMES )
				{
					// Newer packages always store the class name for each asset
					*FileReader << ObjectClassName;
				}
				else
				{
					// We're loading an older package which didn't store the class name for each
					// asset.  Only the relative path was stored for these guys.  We'll try to
					// fix this up after we load the path name.
				}


				// Object path
				FString ObjectPathWithoutPackageName;
				*FileReader << ObjectPathWithoutPackageName;
				const FString ObjectPath( FFilename( InPackageFileName ).GetBaseFilename() + TEXT( "." ) + ObjectPathWithoutPackageName );


				// If the thumbnail was stored with a missing class name ("???") when we'll catch that here
				if( ObjectClassName.Len() > 0 && ObjectClassName != TEXT( "???" ) )
				{
					bHaveValidClassName = TRUE;
				}
				else
				{
					// Class name isn't valid.  Probably legacy data.  We'll try to fix it up below.
				}


				if( !bHaveValidClassName )
				{
					// Try to figure out a class name based on input assets.  This should really only be needed
					// for packages saved by older versions of the editor (VER_CONTENT_BROWSER_FULL_NAMES)
					for ( TSet<FName>::TConstIterator It(InObjectFullNames); It; ++It )
					{
						const FName& CurObjectFullNameFName = *It;

						FString CurObjectFullName;
						CurObjectFullNameFName.ToString( CurObjectFullName );

						if( CurObjectFullName.EndsWith( ObjectPath ) )
						{
							// Great, we found a path that matches -- we just need to add that class name
							const INT FirstSpaceIndex = CurObjectFullName.InStr( TEXT( " " ) );
							check( FirstSpaceIndex != -1 );
							ObjectClassName = CurObjectFullName.Left( FirstSpaceIndex );
							
							// We have a useful class name now!
							bHaveValidClassName = TRUE;
							break;
						}
					}
				}


				// File offset to image data
				INT FileOffset = 0;
				*FileReader << FileOffset;

				if ( FileOffset != -1 && FileOffset < LastFileOffset )
				{
					warnf(NAME_Warning, TEXT("Loaded thumbnail '%s' out of order!: FileOffset:%i    LastFileOffset:%i"), *ObjectPath, FileOffset, LastFileOffset);
				}


				if( bHaveValidClassName )
				{
					// Create a full name string with the object's class and fully qualified path
					const FString ObjectFullName( ObjectClassName + TEXT( " " ) + ObjectPath );


					// Add to our map
					ObjectNameToFileOffsetMap.Set( FName( *ObjectFullName ), FileOffset );
				}
				else
				{
					// Oh well, we weren't able to fix the class name up.  We won't bother making this
					// thumbnail available to load
				}
			}
		}


		// @todo CB: Should sort the thumbnails to load by file offset to reduce seeks [reviewed; pre-qa release]
		for ( TSet<FName>::TConstIterator It(InObjectFullNames); It; ++It )
		{
			const FName& CurObjectFullName = *It;

			// Do we have this thumbnail in the file?
			// @todo thumbnails: Backwards compat
			const INT* pFileOffset = ObjectNameToFileOffsetMap.Find(CurObjectFullName);
			if ( pFileOffset != NULL )
			{
				// Seek to the location in the file with the image data
				FileReader->Seek( *pFileOffset );

				// Load the image data
				FObjectThumbnail LoadedThumbnail;
				LoadedThumbnail.Serialize( *FileReader );

				// Store the data!
				InOutThumbnails.Set( CurObjectFullName, LoadedThumbnail );
			}
			else
			{
				// Couldn't find the requested thumbnail in the file!
			}		
		}


		return TRUE;
	}



	/** Loads thumbnails from a package unless they're already cached in that package's thumbnail map */
	UBOOL ConditionallyLoadThumbnailsFromPackage( const FString& InPackageFileName, const TSet< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails )
	{
		// First check to see if any of the requested thumbnails are already in memory
		TSet< FName > ObjectFullNamesToLoad;
		ObjectFullNamesToLoad.Empty(InObjectFullNames.Num());
		for ( TSet<FName>::TConstIterator It(InObjectFullNames); It; ++It )
		{
			const FName& CurObjectFullName = *It;

			// Do we have this thumbnail in our cache already?
			// @todo thumbnails: Backwards compat
			const FObjectThumbnail* FoundThumbnail = FindCachedThumbnailInPackage( InPackageFileName, CurObjectFullName );
			if( FoundThumbnail != NULL )
			{
				// Great, we already have this thumbnail in memory!  Copy it to our output map.
				InOutThumbnails.Set( CurObjectFullName, *FoundThumbnail );
			}
			else
			{
				ObjectFullNamesToLoad.Add(CurObjectFullName);
			}
		}


		// Did we find all of the requested thumbnails in our cache?
		if( ObjectFullNamesToLoad.Num() == 0 )
		{
			// Done!
			return TRUE;
		}

		// OK, go ahead and load the remaining thumbnails!
		return LoadThumbnailsFromPackage( InPackageFileName, ObjectFullNamesToLoad, InOutThumbnails );
	}



	/** Loads thumbnails for the specified objects (or copies them from a cache, if they're already loaded.) */
	UBOOL LoadThumbnailsForObjects( const TArray< FName >& InObjectFullNames, FThumbnailMap& InOutThumbnails )
	{
		// Create a list of unique package file names that we'll need to interrogate
		struct FObjectFullNamesForPackage
		{
			TSet< FName > ObjectFullNames;
		};

		typedef TMap< FString, FObjectFullNamesForPackage > PackageFileNameToObjectPathsMap;
		PackageFileNameToObjectPathsMap PackagesToProcess;
		for( INT CurObjectIndex = 0; CurObjectIndex < InObjectFullNames.Num(); ++CurObjectIndex )
		{
			const FName ObjectFullName = InObjectFullNames( CurObjectIndex );


			// Determine the package file path/name for the specified object
			FString PackageFilePathName;
			if( !QueryPackageFileNameForObject( ObjectFullName.ToString(), PackageFilePathName ) )
			{
				// Couldn't find the package in our cache
				return FALSE;
			}


			// Do we know about this package yet?
			FObjectFullNamesForPackage* ObjectFullNamesForPackage = PackagesToProcess.Find( PackageFilePathName );
			if( ObjectFullNamesForPackage == NULL )
			{
				ObjectFullNamesForPackage = &PackagesToProcess.Set( PackageFilePathName, FObjectFullNamesForPackage() );
			}

			if ( ObjectFullNamesForPackage->ObjectFullNames.Find(ObjectFullName) == NULL )
			{
				ObjectFullNamesForPackage->ObjectFullNames.Add(ObjectFullName);
			}
		}


		// Load thumbnails, one package at a time
		for( PackageFileNameToObjectPathsMap::TConstIterator PackageIt( PackagesToProcess ); PackageIt; ++PackageIt )
		{
			const FString& CurPackageFileName = PackageIt.Key();
			const FObjectFullNamesForPackage& CurPackageObjectPaths = PackageIt.Value();
			
			if( !ConditionallyLoadThumbnailsFromPackage( CurPackageFileName, CurPackageObjectPaths.ObjectFullNames, InOutThumbnails ) )
			{
				// Failed to load thumbnail data
				return FALSE;
			}
		}


		return TRUE;
	}





}

