/*=============================================================================
Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

namespace ExportMeshUtils
{
	/**
	 * Writes out all StaticMeshes passed in (and all LoDs) for light map editing
	 * @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be exported
	 * @param InDirectory - The directory to attempt the export to
	 * @return If any errors occured during export
	 */
	UBOOL ExportAllLightmapModels (TArray<UStaticMesh*>& AllMeshes, const FString& Directory, UBOOL IsFBX);

	/**
	 * Reads in all StaticMeshes passed in (and all LoDs) for light map editing
	 * @param InAllMeshes - All Static Mesh objects that need their LOD's lightmaps to be imported
	 * @param InDirectory - The directory to attempt the import from
	 * @return If any errors occured during import
	 */
	UBOOL ImportAllLightmapModels (TArray<UStaticMesh*>& AllMeshes, const FString& Directory, UBOOL IsFBX);

	/**
	 * Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	 * @param InStaticMesh - The mesh to be exported
	 * @param InOutputDevice - The output device to write the data to.  Current usage is a FOutputDeviceFile
	 * @param InLODIndex - The LOD of the model we want exported
	 * @return If any errors occured during export
	 */
	UBOOL ExportSingleLightmapModel (UStaticMesh* CurrentStaticMesh, FOutputDevice* Out, const INT LODIndex);

	/**
	 * Writes out a single LOD of a StaticMesh(LODIndex) for light map editing
	 * @param InCurrentStaticMesh - The mesh to be exported
	 * @param InFilename - The name of the file to write the data to.
	 * @param InLODIndex - The LOD of the model we want exported
	 * @return If any errors occured during export
	 */
	UBOOL ExportSingleLightmapModelFBX (UStaticMesh* CurrentStaticMesh, FFilename& Filename, const INT LODIndex);

	/**
	 * Reads in a temp ASE formatted mesh who's texture coordinates will REPLACE that of the active static mesh's LightMapCoordinates
	 * @param InStaticMesh - The mesh to be inported
	 * @param InData- A string that contains the files contents
	 * @param InLODIndex - The LOD of the model we want imported
	 * @return If any errors occured during import
	 */
	UBOOL ImportSingleLightmapModelASE(UStaticMesh* CurrentStaticMesh, FString& Data, const INT LODIndex);

	/**
	 * Reads in a temp FBX formatted mesh who's texture coordinates will REPLACE that of the active static mesh's LightMapCoordinates
	 * @param InStaticMesh - The mesh to be inported
	 * @param InData- An array of bytes that contains the files contents
	 * @param InLODIndex - The LOD of the model we want imported
	 * @return If any errors occured during import
	 */
	UBOOL ImportSingleLightmapModelFBX(UStaticMesh* CurrentStaticMesh, FString& Filename, const INT LODIndex );

}  //end namespace ExportMeshUtils
