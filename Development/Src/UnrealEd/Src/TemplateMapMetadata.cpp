/*=============================================================================
	TemplateMapMetadata.cpp: map template related code
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

/*-----------------------------------------------------------------------------
	UTemplateMapMetadata.
-----------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UTemplateMapMetadata);

/**
 * Create a list of all current template map metadata objects.
 * Current method for this is to add all needed metadata into packages
 * that are always loaded in the editor so we can just iterate over all 
 * UTemplateMapMetadata in memory.
 *
 * @param	Templates - list to which all metadata objects are added.
 *			This should be empty when passed to this method.
 */
void UTemplateMapMetadata::GenerateTemplateMetadataList(TArray<UTemplateMapMetadata*>& Templates)
{
	FString Filename;
	// Ensure the base map template index package is loaded
	if( GPackageFileCache->FindPackageFile( TEXT("MapTemplateIndex"), NULL, Filename ) )
	{
		UPackage* Package = LoadPackage( NULL, *Filename, LOAD_None );
		if( Package && !Package->IsFullyLoaded() )
		{
			Package->FullyLoad();
		}
	}

	for (TObjectIterator<UTemplateMapMetadata> It; It; ++It)
	{
		Templates.AddItem(*It);
	}
}