/*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnPackageTools.h"
#include "EnginePrefabClasses.h"

struct EditorClassEntry
{
	FString ClassName;
	FString PackageName;
	FString ClassGroupString;
	INT ParentClassIndex;
	INT NumChildren;
	INT NumPlaceableChildren;
	UBOOL bIsFactory;
	UBOOL bCanCreateClasses;
	UBOOL bIsHidden;
	UBOOL bIsPlaceable;
	UBOOL bIsAbstract;

	//loaded first time it's requested
	UClass* Class;
};

TArray<EditorClassEntry> EditorClassArray;

/**
 * Builds the tree from the class manifest
 * @return - whether the class hierarchy was successfully built
 */
UBOOL FEditorClassHierarchy::Init (void)
{
	EditorClassArray.Empty();

	FString ManifestFileName = appScriptManifestFile();
	
	//verify file is there
	FString Text;
	if( !appLoadFileToString( Text, *ManifestFileName, GFileManager, 0 /* LoadFileHash_EnableVerify */ ) )
	{
		checkf(FALSE, TEXT("Couldn't find Manifest.txt, it should have been placed in the Scripts folder when compiling scripts!"));
		return FALSE;
	}

	TArray<INT> ParentClassStack;

	TCHAR* Ptr = const_cast<TCHAR*>( *Text );
	UBOOL Done = 0;
	UBOOL bPastVersionIndex = FALSE;
	while( !Done )
	{
		// Advance past new line characters
		while( *Ptr=='\r' || *Ptr=='\n' )
			Ptr++;

		// Store the location of the first character of this line
		TCHAR* Start = Ptr;

		// Advance the char pointer until we hit a newline character
		while( *Ptr && *Ptr!='\r' && *Ptr!='\n' )
			Ptr++;

		// If this is the end of the file, we're done
		if( *Ptr==0 )
			Done = 1;

		// Terminate the current line, and advance the pointer to the next character in the stream
		*Ptr++ = 0;

		if (!bPastVersionIndex)
		{
			bPastVersionIndex = TRUE;
			continue;
		}

		FString ManifestEntryString = Start;
		if (ManifestEntryString.Len())
		{
			ManifestEntryString = ManifestEntryString.Trim();

			UBOOL bUseEspace = FALSE;
			const TCHAR* ConstStart = Start;
			FString DepthString(ParseToken( ConstStart, bUseEspace ));
			FString ClassNameString(ParseToken( ConstStart, bUseEspace ));
			FString PackageNameString(ParseToken( ConstStart, bUseEspace ));
			FString FlagString(ParseToken( ConstStart, bUseEspace ));
			check(FlagString.Len() >= 2);
			FlagString = FlagString.Mid(1, FlagString.Len()-1);
			
			// Get the class group string for this class.
			FString ClassGroupString(ParseToken( ConstStart, bUseEspace ));
			// make sure there are at least two characters
			check( ClassGroupString.Len() >= 2 );
			if( ClassGroupString.Len() > 2)
			{
				// Get the string without the brackets
				ClassGroupString = ClassGroupString.Mid(1, ClassGroupString.Len()-2);
			}
			else
			{
				// The string contains no group information
				ClassGroupString.Empty();
			}

			INT DepthValue = appAtoi(*DepthString);

			if (DepthValue < ParentClassStack.Num())
			{
				//back up to the right parent element
				ParentClassStack.Remove(DepthValue, ParentClassStack.Num() - DepthValue);
			}

			EditorClassEntry ClassEntry;
			ClassEntry.ClassName = ClassNameString;
			ClassEntry.PackageName = PackageNameString;
			ClassEntry.ClassGroupString = ClassGroupString;
			ClassEntry.NumChildren = 0;
			ClassEntry.NumPlaceableChildren = 0;
			ClassEntry.bIsFactory = (FlagString.InStr(TEXT("F"))!=INDEX_NONE) ? TRUE : FALSE;
			ClassEntry.bCanCreateClasses = (FlagString.InStr(TEXT("C"))!=INDEX_NONE) ? TRUE : FALSE;
			ClassEntry.bIsHidden = (FlagString.InStr(TEXT("H"))!=INDEX_NONE) ? TRUE : FALSE;
			ClassEntry.bIsPlaceable = (FlagString.InStr(TEXT("P"))!=INDEX_NONE) ? TRUE : FALSE;
			ClassEntry.bIsAbstract = (FlagString.InStr(TEXT("A"))!=INDEX_NONE) ? TRUE : FALSE;
			ClassEntry.Class = NULL;

			if (ParentClassStack.Num())
			{
				ClassEntry.ParentClassIndex = ParentClassStack(ParentClassStack.Num()-1);
				EditorClassArray(ClassEntry.ParentClassIndex).NumChildren++;
				if (ClassEntry.bIsPlaceable)
				{
					EditorClassArray(ClassEntry.ParentClassIndex).NumPlaceableChildren++;
				}
			}
			else
			{
				ClassEntry.ParentClassIndex = INDEX_NONE;
			}

			EditorClassArray.AddItem(ClassEntry);

			//add entry to the stack
			check(DepthValue == ParentClassStack.Num());
			ParentClassStack.AddItem(EditorClassArray.Num()-1);
		}
	}

	return TRUE;
}

/**
 * TRUE, if the manifest file loaded classes successfully
 */
UBOOL FEditorClassHierarchy::WasLoadedSuccessfully(void) const
{
	return (EditorClassArray.Num() > 0);
}

/**
 * Gets the direct children of the class
 * @param InClassIndex - the index of the class in question
 * @param OutIndexArray - the array to fill in with child indices
 */
void FEditorClassHierarchy::GetChildrenOfClass(const INT InClassIndex, TArray<INT> &OutIndexArray)
{
	OutIndexArray.Empty();
	for (INT i = 0; i < EditorClassArray.Num(); ++i)
	{
		if (EditorClassArray(i).ParentClassIndex == InClassIndex)
		{
			OutIndexArray.AddItem(i);
		}
	}
}

/** 
 * Adds the node and all children recursively to the list of OutAllClasses 
 * @param InClassIndex - The node in the hierarchy to start with
 * @param OutAllClasses - The list of classes generated recursively
 */
void FEditorClassHierarchy::GetAllClasses(const INT InClassIndex, OUT TArray<UClass*> OutAllClasses)
{
	//add this class
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));
	UClass* CurrentClass = EditorClassArray(InClassIndex).Class;
	if (CurrentClass)
	{
		OutAllClasses.AddItem(CurrentClass);

		//recurse
		TArray<INT> ChildIndexArray;
		GetChildrenOfClass(InClassIndex, ChildIndexArray);
		for (INT i = 0; i < ChildIndexArray.Num(); ++i)
		{
			INT ChildClassIndex = ChildIndexArray(i);
			GetAllClasses(ChildClassIndex, OutAllClasses);
		}
	}
}


/**
 * Gets the list of classes that are supported factory classes
 * @param OutIndexArray - the array to fill in with factory class indices
 */
void FEditorClassHierarchy::GetFactoryClasses(TArray<INT> &OutIndexArray)
{
	OutIndexArray.Empty();
	for (INT i = 0; i < EditorClassArray.Num(); ++i)
	{
		if (EditorClassArray(i).bIsFactory)
		{
			OutIndexArray.AddItem(i);
		}
	}
}

/**
 * Find the class index that matches the requested name
 * @param InClassName - Name of the class to find
 * @return - The index of the desired class
 */
INT FEditorClassHierarchy::Find(const FString& InClassName)
{
	for (INT i = 0; i < EditorClassArray.Num(); ++i)
	{
		if (EditorClassArray(i).ClassName == InClassName)
		{
			return i;
		}
	}
	return -1;
}

/**
 * returns the name of the class in the tree
 * @param InClassIndex - the index of the class in question
 */
FString FEditorClassHierarchy::GetClassName(const INT InClassIndex)
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));
	return EditorClassArray(InClassIndex).ClassName;
}

/**
 * returns the UClass of the class in the tree
 * @param InClassIndex - the index of the class in question
 */
UClass* FEditorClassHierarchy::GetClass(const INT InClassIndex)
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));
	if (!EditorClassArray(InClassIndex).Class)
	{
		if (EditorClassArray(InClassIndex).ParentClassIndex == -1)
		{
			EditorClassArray(InClassIndex).Class = UObject::StaticClass();
		}
		else
		{
			UClass* ParentClass = GetClass(EditorClassArray(InClassIndex).ParentClassIndex);

			FString PackageName = EditorClassArray(InClassIndex).PackageName;
			UPackage* ParentPackage = UObject::FindPackage(NULL, *PackageName);
			if (!ParentPackage)
			{
				ParentPackage = PackageTools::LoadPackage(*PackageName);
			}
			EditorClassArray(InClassIndex).Class = UObject::StaticLoadClass( ParentClass, ParentPackage, *EditorClassArray(InClassIndex).ClassName, *PackageName, 0, NULL );
		}
	}
	return EditorClassArray(InClassIndex).Class;
}

/**
 * Returns the class index of the provided index's parent class
 *
 * @param	InClassIndex	Class index to find the parent of
 *
 * @return	Class index of the parent class of the provided index, if any; INDEX_NONE if not
 */
INT FEditorClassHierarchy::GetParentIndex(INT InClassIndex) const
{
	check( EditorClassArray.IsValidIndex( InClassIndex ) );
	return EditorClassArray(InClassIndex).ParentClassIndex;
}

/** 
 * Returns a list of class group names for the provided class index 
 *
 * @param InClassIndex	The class index to find group names for
 * @param OutGroups		The list of class groups found.
 */
void FEditorClassHierarchy::GetClassGroupNames( INT InClassIndex, TArray<FString>& OutGroups ) const
{
	const EditorClassEntry& Entry = EditorClassArray(InClassIndex);
	Entry.ClassGroupString.ParseIntoArray( &OutGroups, TEXT(","), TRUE );
}

/**
 * returns if the class is hidden or not
 * @param InClassIndex - the index of the class in question
 */
UBOOL FEditorClassHierarchy::IsHidden(const INT InClassIndex) const
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));
	return EditorClassArray(InClassIndex).bIsHidden;
}
/**
 * returns if the class is placeable or not
 * @param InClassIndex - the index of the class in question
 */
UBOOL FEditorClassHierarchy::IsPlaceable(const INT InClassIndex) const
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));
	return EditorClassArray(InClassIndex).bIsPlaceable;
}
/**
 * returns if the class is abstract or not
 * @param InClassIndex - the index of the class in question
 */
UBOOL FEditorClassHierarchy::IsAbstract(const INT InClassIndex) const
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));
	return EditorClassArray(InClassIndex).bIsAbstract;
}
/**
 * returns if the class is a brush or not
 * @param InClassIndex - the index of the class in question
 */
UBOOL FEditorClassHierarchy::IsBrush(const INT InClassIndex)
{
	UClass* Class = GetClass( InClassIndex );
	return Class->IsChildOf( ABrush::StaticClass() );
}
/**
 * Returns if the class is visible 
 * @param InClassIndex - the index of the class in question
 * @param bInPlaceable - if TRUE, return number of placeable children, otherwise returns all children
 * @return - Number of children
 */
UBOOL FEditorClassHierarchy::IsClassVisible(const INT InClassIndex, const UBOOL bInPlaceableOnly)
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));

	const EditorClassEntry &TempEntry = EditorClassArray(InClassIndex);

	if ((TempEntry.bIsHidden) || (TempEntry.bIsAbstract))
	{
		return FALSE;
	}
	else if ( !bInPlaceableOnly || (TempEntry.bIsPlaceable) )
	{
		return TRUE;
	}
	return FALSE;
}

/**
 * Returns if the class has any children (placeable or all)
 * @param InClassIndex - the index of the class in question
 * @param bInPlaceable - if TRUE, return if has placeable children, otherwise returns if has any children
 * @return - Whether this node has children (recursively) that are visible
 */
UBOOL FEditorClassHierarchy::HasChildren(const INT InClassIndex, const UBOOL bInPlaceableOnly)
{
	check(IsWithin(InClassIndex, 0, EditorClassArray.Num()));

	TArray<INT> ChildIndexArray;
	GetChildrenOfClass(InClassIndex, ChildIndexArray);

	for ( INT i = 0 ; i < ChildIndexArray.Num() ; ++i )
	{
		INT ChildClassIndex = ChildIndexArray(i);
		UBOOL bIsClassVisible = IsClassVisible(ChildClassIndex, bInPlaceableOnly);
		if (bIsClassVisible)
		{
			return TRUE;
		}
		else if ( HasChildren( ChildClassIndex, bInPlaceableOnly ) )
		{
			return TRUE;
		}
	}
	return FALSE;
}
