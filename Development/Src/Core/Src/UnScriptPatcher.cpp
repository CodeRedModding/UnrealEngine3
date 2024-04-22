/*=============================================================================
	UnScriptPatcher.cpp: Implementation for script bytecode patcher and helper classes
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "CorePrivate.h"
#include "UnScriptPatcher.h"

#if SUPPORTS_SCRIPTPATCH_LOADING

/*
	TODO:

	- support for patching changes to property flags
	- support for patching changes to class flags
*/

/*----------------------------------------------------------------------------
	FPatchData / FScriptPatchData
----------------------------------------------------------------------------*/

/** Serializer */
FArchive& operator<<( FArchive& Ar, FPatchData& Patch )
{
	return Ar << Patch.DataName << Patch.Data;
}

FArchive& operator<<( FArchive& Ar, FScriptPatchData& Patch )
{
	return Ar << Patch.StructName << (FPatchData&)(Patch);
}

/** Serializer */
FArchive& operator<<( FArchive& Ar, FEnumPatchData& EnumData)
{
	return Ar << EnumData.EnumName << EnumData.EnumPathName << EnumData.EnumValues;
}

/** Serializer */
FArchive& operator<<( FArchive& Ar, FLinkerPatchData& LinkerData )
{
	Ar << LinkerData.PackageName << LinkerData.Names << LinkerData.Exports << LinkerData.Imports;
	Ar << LinkerData.NewObjects << LinkerData.ModifiedClassDefaultObjects << LinkerData.ModifiedEnums << LinkerData.ScriptPatches;
	return Ar;
}


/**
 * Constructor
 */
FPatchBinaryReader::FPatchBinaryReader(TArray<BYTE>& PatchMemory)
: FMemoryReader(PatchMemory)
{

}

/**
 * Override FName serializer to use strings
 */
FArchive& FPatchBinaryReader::operator<<(FName& N)
{
	FString TempString;
	*this << TempString;
	N = FName(*TempString);

	return *this;
}


/**
 * Constructor
 */
FPatchBinaryWriter::FPatchBinaryWriter(TArray<BYTE>& PatchMemory)
: FMemoryWriter(PatchMemory)
{

}

/**
 * Override FName serializer to use strings
 */
FArchive& FPatchBinaryWriter::operator<<(FName& N)
{
	FString StructNameString;
	StructNameString = N.ToString();
	return *this << StructNameString;
}


/*----------------------------------------------------------------------------
	FScriptPatcher.
----------------------------------------------------------------------------*/
FScriptPatcher::FScriptPatcher()
{
//	BuildPatchList(PackageUpdates);
}

FScriptPatcher::~FScriptPatcher()
{
	for ( INT i = PackageUpdates.Num() - 1; i >= 0; i-- )
	{
		delete PackageUpdates(i);
		PackageUpdates(i) = NULL;
	}
}


/**
 * Retrieves the patch data for the specified package.
 *
 * @param	PackageName			the package name to retrieve a patch for
 * @param	out_LinkerPatch		receives the patch associated with the specified package
 *
 * @return	TRUE if the script patcher contains a patch for the specified package.
 */
UBOOL FScriptPatcher::GetLinkerPatch( const FName& PackageName, FLinkerPatchData*& out_LinkerPatch )
{
#if !CONSOLE && !DEDICATED_SERVER
	// PC programs don't need to patch this way, so make sure the PC never finds a console's script patch
	return FALSE;
#endif

	UBOOL bResult = FALSE;

	// look in the cached patch list
	for ( INT UpdateIndex = 0; UpdateIndex < PackageUpdates.Num(); UpdateIndex++ )
	{
		FLinkerPatchData* Update = PackageUpdates(UpdateIndex);
		if ( PackageName == Update->PackageName )
		{
			out_LinkerPatch = Update;
			bResult = TRUE;
			break;
		}
	}

	if (bResult == FALSE)
	{
		UBOOL bDisablePatching = ParseParam(appCmdLine(), TEXT("NOPATCH"));
		if (!bDisablePatching)
		{
			FString PatchFile = FString::Printf(TEXT("%sPatches\\ScriptPatch_%s.bin"), *appGameDir(), *PackageName.ToString());
			TArray<BYTE> InBytes;
			INT FileSize = GFileManager->UncompressedFileSize(*PatchFile);
			// attempt to load a patch blob
			if (FileSize > 0)
			{
				FArchive* RawReader = GFileManager->CreateFileReader(*PatchFile);
				InBytes.Add(GFileManager->UncompressedFileSize(*PatchFile) * 4);
				RawReader->SerializeCompressed(InBytes.GetData(), FileSize, GBaseCompressionMethod);

				// parse it with patch reader
				FPatchBinaryReader Ar(InBytes);

				// read into a new linker patch data object
				out_LinkerPatch = new FLinkerPatchData;
				debugf(NAME_DevPatch, TEXT("Loading compressed patch data for package %s:"), *PackageName.ToString());
				Ar << *out_LinkerPatch;

				// adding the data to list of existing patch data
				PackageUpdates.AddItem(out_LinkerPatch);

				bResult = TRUE;
			}
		}
	}

	return bResult;
}

/**
 * Frees the data associated with the patch data, the linker is done with it
 * @param	PackageName			the package name to free the patch data for
 */
void FScriptPatcher::FreeLinkerPatch( const FName& PackageName )
{
	// look in the cached patch list
	for ( INT UpdateIndex = 0; UpdateIndex < PackageUpdates.Num(); UpdateIndex++ )
	{
		FLinkerPatchData* Update = PackageUpdates(UpdateIndex);
		if ( PackageName == Update->PackageName )
		{
			debugf(NAME_DevPatch, TEXT("UNLOADING script patch for package %s"), *PackageName.ToString());

			// free the patch data memory
			delete Update;
			PackageUpdates.Remove(UpdateIndex);
		}
	}
}

#endif