/*=============================================================================
	UnEdFactMorph.cpp: Morph target mesh factory import code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "EngineAnimClasses.h"
#include "SkelImport.h"

#define SET_MORPH_ERROR( ErrorType ) { if( Error ) *Error = ErrorType; }

/** 
* Constructor
* 
* @param	InSrcMesh - source skeletal mesh that the new morph mesh morphs
* @param	LODIndex - lod entry of base mesh
* @param	InWarn - for outputing warnings
*/
FMorphTargetBinaryImport::FMorphTargetBinaryImport( USkeletalMesh* InSrcMesh, INT LODIndex, FFeedbackContext* InWarn )
:	BaseMesh(InSrcMesh)
,	Warn(InWarn)
,	BaseMeshRawData( InSrcMesh, LODIndex )
,	BaseLODIndex(LODIndex)
{	
}

/** 
* Constructor
* 
* @param	InSrcMesh - source static mesh that the new morph mesh morphs
* @param	LODIndex - lod entry of base mesh
* @param	InWarn - for outputing warnings
*/
FMorphTargetBinaryImport::FMorphTargetBinaryImport( UStaticMesh* InSrcMesh, INT LODIndex, FFeedbackContext* InWarn )
:	BaseMesh(InSrcMesh)
,	Warn(InWarn)
,	BaseMeshRawData( InSrcMesh, LODIndex )
,	BaseLODIndex(LODIndex)
{	
}


/**
* Imports morph target data from file into a new morph target
* object and adds it to the morph target set
*
* @param	InDestMorphSet - destination morph target set to import into
* @param	Name - name of the new object
* @param	SrcFilename - file to import
* @param	bReplaceExisting - file to import
* @param	Error - optional error value
*
* @return	newly created morph target mesh
*/
UMorphTarget* FMorphTargetBinaryImport::ImportMorphMeshToSet( 
	UMorphTargetSet* DestMorphSet,
	FName Name, 
	const TCHAR* SrcFilename, 
	UBOOL bReplaceExisting,
	EMorphImportError* Error )
{
	check(DestMorphSet);

	SET_MORPH_ERROR( MorphImport_OK );
	UMorphTarget* Result =  NULL;
	UMorphTarget* ExistingTarget = DestMorphSet->FindMorphTarget(Name);

	// see if this morph target already exists in the set
	if( ExistingTarget && !bReplaceExisting )
	{
		SET_MORPH_ERROR( MorphImport_AlreadyExists );
	}
	// check for valid meta data for the base mesh
	else if( BaseMeshRawData.Indices.Num() && !BaseMeshRawData.WedgePointIndices.Num() )
	{
		SET_MORPH_ERROR( MorphImport_ReimportBaseMesh );        
	}
	else
	{
		USkeletalMesh* TargetSkelMesh = this->CreateSkeletalMesh(SrcFilename, Error);

		if( !TargetSkelMesh )
		{
			SET_MORPH_ERROR( MorphImport_InvalidMeshFormat );
		}
		else
		{
			Warn->BeginSlowTask( TEXT("Generating Morph Model"), 1);

			// convert the morph target mesh to the raw vertex data
			FMorphMeshRawSource TargetMeshRawData( TargetSkelMesh );

			// check to see if the vertex data is compatible
			if( !BaseMeshRawData.IsValidTarget(TargetMeshRawData) )
			{
				SET_MORPH_ERROR( MorphImport_MismatchBaseMesh );                
			}
			else
			{
				if( bReplaceExisting )
				{
					// overwrite an existing target mesh
					Result = ExistingTarget;
				}
				else
				{
					// create the new morph target mesh
					Result = ConstructObject<UMorphTarget>( UMorphTarget::StaticClass(), DestMorphSet, Name );
					// add it to the set
					DestMorphSet->Targets.AddItem( Result );
				}					

				// populate the vertex data for the morph target mesh using its base mesh
				// and the newly imported mesh				
				check(Result);
				Result->CreateMorphMeshStreams( BaseMeshRawData, TargetMeshRawData, 0 );
			}

			Warn->EndSlowTask();
		}
	}

	return Result;
}

/**
* Imports morph target data from file for a specific LOD entry
* of an existing morph target
*
* @param	MorphTarget - existing morph target to modify
* @param	SrcFilename - file to import
* @param	LODIndex - LOD entry to import to
* @param	Error - optional error value
*/
void FMorphTargetBinaryImport::ImportMorphLODModel( UMorphTarget* MorphTarget, const TCHAR* SrcFilename, INT LODIndex, EMorphImportError* Error )
{
	check(MorphTarget);

	SET_MORPH_ERROR( MorphImport_OK );

	// check for valid LOD index 
	if( LODIndex > MorphTarget->MorphLODModels.Num() ||
		LODIndex != BaseLODIndex )
	{
		SET_MORPH_ERROR( MorphImport_InvalidLODIndex );
	}
	// check for valid meta data for the base mesh
	else if( BaseMeshRawData.Indices.Num() && !BaseMeshRawData.WedgePointIndices.Num() )
	{
		SET_MORPH_ERROR( MorphImport_ReimportBaseMesh );
	}
	else
	{
		USkeletalMesh* TargetSkelMesh = this->CreateSkeletalMesh(SrcFilename, Error);
				
		if( !TargetSkelMesh )
		{
			SET_MORPH_ERROR( MorphImport_InvalidMeshFormat );
		}		
		else
		{
			Warn->BeginSlowTask( TEXT("Generating Morph Model"), 1);

			// convert the morph target mesh to the raw vertex data
			FMorphMeshRawSource TargetMeshRawData( TargetSkelMesh );

			// check to see if the vertex data is compatible
			if( !BaseMeshRawData.IsValidTarget(TargetMeshRawData) )
			{
				SET_MORPH_ERROR( MorphImport_MismatchBaseMesh );                
			}
			else
			{
				// populate the vertex data for the morph target mesh using its base mesh
				// and the newly imported mesh				
				MorphTarget->CreateMorphMeshStreams( BaseMeshRawData, TargetMeshRawData, LODIndex );
			}

			Warn->EndSlowTask();
		}
	}
}

FMorphTargetBinaryPSKImport::FMorphTargetBinaryPSKImport( USkeletalMesh* InSrcMesh, INT LODIndex, FFeedbackContext* InWarn )
:   FMorphTargetBinaryImport( InSrcMesh, LODIndex, InWarn)
{

}

USkeletalMesh* FMorphTargetBinaryPSKImport::CreateSkeletalMesh(const TCHAR* SrcFilename, EMorphImportError* Error )
{
	USkeletalMesh* TargetSkelMesh = NULL;

	// load the file to byte array
	TArray<BYTE> Data;
	if( !appLoadFileToArray( Data, SrcFilename ) )
	{
		SET_MORPH_ERROR( MorphImport_CantLoadFile );
	}
	else
	{
		// null terminate it
		Data.AddItem( 0 );
		// start of byte array
		const BYTE* Buffer = &Data( 0 );
		// end of byte array
		const BYTE* BufferEnd = Buffer+Data.Num()-1;

		Warn->BeginSlowTask( TEXT("Importing Mesh Model"), 1);

		// Use the SkeletalMeshFactory to load this SkeletalMesh into a temporary SkeletalMesh.
		USkeletalMeshFactory* SkelMeshFact = new USkeletalMeshFactory();
		TargetSkelMesh = (USkeletalMesh*)SkelMeshFact->FactoryCreateBinary( 
			USkeletalMesh::StaticClass(), 
			UObject::GetTransientPackage(), 
			NAME_None, 
			RF_Transient, 
			NULL, 
			NULL, 
			Buffer, 
			BufferEnd, 
			Warn );

		Warn->EndSlowTask();
	}

	return TargetSkelMesh;
}
