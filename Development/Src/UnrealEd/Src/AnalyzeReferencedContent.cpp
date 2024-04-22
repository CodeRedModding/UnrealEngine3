/** 
 *
 * Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
 */

#include "UnrealEd.h"
#include "EnginePhysicsClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineSoundClasses.h"
#include "PerfMem.h"
#include "AnimationEncodingFormat.h"
#include "AnimationUtils.h"

IMPLEMENT_CLASS(UAnalyzeReferencedContentCommandlet);
IMPLEMENT_CLASS(UAnalyzeReferencedObjectCommandlet);

// PLATFORM parameter, by default Xbox
/** What platform are we cooking for?														*/
UE3::EPlatformType				Platform;

/*-----------------------------------------------------------------------------
UAnalyzeReferencedContentCommandlet
-----------------------------------------------------------------------------*/
void FAnalyzeReferencedContentStat::WriteOutAllAvailableStatData( const FString& CSVDirectory )
{
	if ((IgnoreObjects & IGNORE_StaticMesh) == 0)
	{
		WriteOutCSVs<FStaticMeshStats>( ResourceNameToStaticMeshStats, CSVDirectory, TEXT( "StaticMeshStats" ) );
		WriteOutSummaryCSVs<FStaticMeshStats> (ResourceNameToStaticMeshStats, CSVDirectory, TEXT( "StaticMeshStats" ) );
	}

	if ((IgnoreObjects & IGNORE_SkeletalMesh) == 0)
	{
		WriteOutCSVs<FSkeletalMeshStats>( ResourceNameToSkeletalMeshStats, CSVDirectory, TEXT( "SkeletalMeshStats" ) );
		WriteOutSummaryCSVs<FSkeletalMeshStats> (ResourceNameToSkeletalMeshStats, CSVDirectory, TEXT( "SkeletalMeshStats" ) );
	}

	if ((IgnoreObjects & IGNORE_Texture) == 0)
	{
		WriteOutCSVs<FTextureStats>( ResourceNameToTextureStats, CSVDirectory, TEXT( "TextureStats" ) );
		WriteOutSummaryCSVs<FTextureStats>( ResourceNameToTextureStats, CSVDirectory, TEXT( "TextureStats" ) );
	}

	if ((IgnoreObjects & IGNORE_Material) == 0)
	{
		WriteOutCSVs<FMaterialStats>( ResourceNameToMaterialStats, CSVDirectory, TEXT( "MaterialStats" ) );
		WriteOutSummaryCSVs<FMaterialStats> (ResourceNameToMaterialStats, CSVDirectory, TEXT( "MaterialStats" ) );
	}

	if ((IgnoreObjects & IGNORE_Particle) == 0)
	{
		WriteOutCSVs<FParticleStats>( ResourceNameToParticleStats, CSVDirectory, TEXT( "ParticleStats" ) );
		WriteOutSummaryCSVs<FParticleStats>( ResourceNameToParticleStats, CSVDirectory, TEXT( "ParticleStats" ) );
	}

	if ((IgnoreObjects & IGNORE_Anim) == 0)
	{
		WriteOutCSVs<FAnimSequenceStats>( ResourceNameToAnimStats, CSVDirectory, TEXT( "AnimStats" ) );
		WriteOutSummaryCSVs<FAnimSequenceStats>( ResourceNameToAnimStats, CSVDirectory, TEXT( "AnimStats" ) );
	}

	if ((IgnoreObjects & IGNORE_Primitive) == 0)
	{
		WriteOutCSVs<FPrimitiveStats>( ResourceNameToPrimitiveStats, CSVDirectory, TEXT( "PrimitiveStats" ) );
		WriteOutSummaryCSVs<FPrimitiveStats>( ResourceNameToPrimitiveStats, CSVDirectory, TEXT( "PrimitiveStats" ) );
	}

#if WITH_FACEFX
	if ((IgnoreObjects & IGNORE_FaceFXAnimSet) == 0)
	{
		WriteOutCSVs<FFaceFXAnimSetStats>( ResourceNameToFaceFXAnimSetStats, CSVDirectory, TEXT( "FaceFXAnimSetStats" ) );
		WriteOutSummaryCSVs<FFaceFXAnimSetStats>( ResourceNameToFaceFXAnimSetStats, CSVDirectory, TEXT( "FaceFXAnimSetStats" ) );
	}
#endif //WITH_FACEFX

	if ((IgnoreObjects & IGNORE_StaticMeshActor) == 0)
	{
		WriteOutCSVs<FLightingOptimizationStats>( ResourceNameToLightingStats, CSVDirectory, TEXT( "LightMapStats" ) );
	}

	if ((IgnoreObjects & IGNORE_Particle) == 0)
	{
		WriteOutCSVs<FTextureToParticleSystemStats>( ResourceNameToTextureToParticleSystemStats, CSVDirectory, TEXT( "TextureToParticleStats" ) );
	}

	if ((IgnoreObjects & IGNORE_SoundCue) == 0)
	{
		WriteOutCSVs<FSoundCueStats>( ResourceNameToSoundCueStats, CSVDirectory, TEXT( "SoundCueStats" ) );
		WriteOutSummaryCSVs<FSoundCueStats>( ResourceNameToSoundCueStats, CSVDirectory, TEXT( "SoundCueStats" ) );
	}

	if ((IgnoreObjects & IGNORE_SoundNodeWave) == 0)
	{
		WriteOutCSVs<FSoundNodeWaveStats>( ResourceNameToSoundNodeWaveStats, CSVDirectory, TEXT( "SoundNodeWaveStats" ) );
		WriteOutSummaryCSVs<FSoundNodeWaveStats>( ResourceNameToSoundNodeWaveStats, CSVDirectory, TEXT( "SoundNodeWaveStats" ) );
	}

	if ((IgnoreObjects & IGNORE_ShadowMap) == 0 )
	{
		WriteOutCSVs<FShadowMap1DStats>( ResourceNameToShadowMap1DStats, CSVDirectory, TEXT( "ShadowMap1DStats" ) );
		WriteOutCSVs<FShadowMap2DStats>( ResourceNameToShadowMap2DStats, CSVDirectory, TEXT( "ShadowMap2DStats" ) );
	}		

	// Write PerLevel Data
	if ((IgnoreObjects & IGNORE_StaticMesh) == 0)
	{
		WriteOutCSVsPerLevel<FStaticMeshStats>(ResourceNameToStaticMeshStats, CSVDirectory, TEXT("StaticMeshStats"));
	}

	if ((IgnoreObjects & IGNORE_SoundCue) == 0)
	{
		WriteOutCSVsPerLevel<FSoundCueStats>(ResourceNameToSoundCueStats, CSVDirectory, TEXT("SoundCueStats"));
	}

	if ((IgnoreObjects & IGNORE_SoundNodeWave) == 0)
	{
		WriteOutCSVsPerLevel<FSoundNodeWaveStats>(ResourceNameToSoundNodeWaveStats, CSVDirectory, TEXT("SoundNodeWaveStats"));
	}

	if ((IgnoreObjects & IGNORE_Anim) == 0)
	{
		WriteOutCSVsPerLevel<FAnimSequenceStats>(ResourceNameToAnimStats, CSVDirectory, TEXT("AnimStats"));
	}

	if ((IgnoreObjects & IGNORE_SkeletalMesh) == 0)
	{
		WriteOutCSVsPerLevel<FSkeletalMeshStats>(ResourceNameToSkeletalMeshStats, CSVDirectory, TEXT("SkeletalMeshStats"));
	}

	if ((IgnoreObjects & IGNORE_Primitive) == 0)
	{
		WriteOutCSVsPerLevel<FPrimitiveStats>( ResourceNameToPrimitiveStats, CSVDirectory, TEXT( "PrimitiveStats" ) );
	}

	WriteOutSummary(CSVDirectory);

#if WITH_FACEFX
	if ((IgnoreObjects & IGNORE_FaceFXAnimSet) == 0)
	{
		WriteOutCSVsPerLevel<FFaceFXAnimSetStats>( ResourceNameToFaceFXAnimSetStats, CSVDirectory, TEXT("FaceFXAnimSetStats"));
	}
#endif //WITH_FACEFX

#if 0
	debugf(TEXT("%s"),*FStaticMeshStats::GetCSVHeaderRow());
	for( TMap<FString,FStaticMeshStats>::TIterator It(ResourceNameToStaticMeshStats); It; ++ It )
	{
		const FStaticMeshStats& StatsEntry = It.Value();
		debugf(TEXT("%s"),*StatsEntry.ToCSV());
	}

	debugf(TEXT("%s"),*FTextureStats::GetCSVHeaderRow());
	for( TMap<FString,FTextureStats>::TIterator It(ResourceNameToTextureStats); It; ++ It )
	{
		const FTextureStats& StatsEntry	= It.Value();
		debugf(TEXT("%s"),*StatsEntry.ToCSV());
	}

	debugf(TEXT("%s"),*FMaterialStats::GetCSVHeaderRow());
	for( TMap<FString,FMaterialStats>::TIterator It(ResourceNameToMaterialStats); It; ++ It )
	{
		const FMaterialStats& StatsEntry = It.Value();
		debugf(TEXT("%s"),*StatsEntry.ToCSV());
	}
#endif
}

template< typename STAT_TYPE >
static INT FAnalyzeReferencedContentStat::GetTotalCountPerLevel( const TMap<FString,STAT_TYPE>& StatsData, const FString& LevelName )
{
	INT TotalCount=0;

	for( TMap<FString,STAT_TYPE>::TConstIterator It(StatsData); It; ++ It )
	{
		const STAT_TYPE& StatsEntry = It.Value();

		const UINT* NumInst = StatsEntry.LevelNameToInstanceCount.Find( LevelName );
		if( NumInst != NULL )
		{
			++TotalCount;
		}
	}

	return TotalCount;
}

void FAnalyzeReferencedContentStat::WriteOutSummary(const FString& CSVDirectory)
{
	// Re-used helper variables for writing to CSV file.
	FString		CSVFilename		= TEXT("");
	FArchive*	CSVFile			= NULL;


	// Create CSV folder in case it doesn't exist yet.
	GFileManager->MakeDirectory( *CSVDirectory );

	// CSV: Human-readable spreadsheet format.
	CSVFilename	= FString::Printf(TEXT("%sTotalSummary-%s-%i.csv"), *CSVDirectory, GGameName, GetChangeListNumberForPerfTesting() );
	CSVFile	= GFileManager->CreateFileWriter( *CSVFilename );
	if( CSVFile != NULL )
	{	
		// Write out header row.
		const FString& HeaderRow = FString::Printf(TEXT(", StaticMeshes, SkeletalMeshes, Textures, Materials, Particles, AnimSequence, FaceFXAnimSet, SoundCue, SoundNodeWaves, ShadowMap1D, ShadowMap2D%s"), LINE_TERMINATOR);
		CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

		for (INT MapId=0; MapId<MapFileList.Num(); ++MapId)
		{
			FFilename LevelName= FFilename(MapFileList(MapId)).GetBaseFilename(TRUE);

			INT TotalStaticMesh = GetTotalCountPerLevel(ResourceNameToStaticMeshStats, LevelName);
			INT TotalSkelMesh = GetTotalCountPerLevel(ResourceNameToSkeletalMeshStats, LevelName);
			INT TotalTextures = GetTotalCountPerLevel(ResourceNameToTextureStats, LevelName);
			INT TotalMaterials = GetTotalCountPerLevel(ResourceNameToMaterialStats, LevelName);
			INT TotalParticles = GetTotalCountPerLevel(ResourceNameToParticleStats, LevelName);
			INT TotalAnims = GetTotalCountPerLevel(ResourceNameToAnimStats, LevelName);
			INT TotalFaceFXs = GetTotalCountPerLevel(ResourceNameToFaceFXAnimSetStats, LevelName);
			INT TotalSoundCues = GetTotalCountPerLevel(ResourceNameToSoundCueStats, LevelName);
			INT TotalSoundNodeWavess = GetTotalCountPerLevel(ResourceNameToSoundNodeWaveStats, LevelName);
			INT TotalShadowMap1D = GetTotalCountPerLevel(ResourceNameToShadowMap1DStats, LevelName);
			INT TotalShadowMap2D = GetTotalCountPerLevel(ResourceNameToShadowMap2DStats, LevelName);

			if (TotalStaticMesh + TotalSkelMesh + TotalTextures + TotalMaterials + TotalParticles + TotalAnims +
				TotalFaceFXs + TotalSoundCues + TotalSoundNodeWavess + TotalShadowMap1D + TotalShadowMap2D != 0)
			{
				FString Row = FString::Printf(TEXT("%s, %d, %d, %d, %d, %d, %d, %d, %d, %d, %d,%d%s"), 
					*LevelName, 
					TotalStaticMesh,  
					TotalSkelMesh, 
					TotalTextures,  
					TotalMaterials, 
					TotalParticles, 
					TotalAnims, 
					TotalFaceFXs, 
					TotalSoundCues,
					TotalSoundNodeWavess,
					TotalShadowMap1D, 
					TotalShadowMap2D, 
					LINE_TERMINATOR);
				if( Row.Len() > 0 )
				{
					CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
				}
			}
		}

		// Close and delete archive.
		CSVFile->Close();
		delete CSVFile;
	}
	else
	{
		debugf(NAME_Warning,TEXT("Could not create CSV file %s for writing."), *CSVFilename);
	}
}

/**
* This function fills up MapsUsedIn and LevelNameToInstanceCount if bAddPerLevelDataMap is TRUE. 
*
* @param	LevelPackage	Level Package this object belongs to
* @param	bAddPerLevelDataMap	Set this to be TRUE if you'd like to collect this stat per level (in the Level folder)
* 
*/
void FAnalyzeReferencedContentStat::FAssetStatsBase::AddLevelInfo( UPackage* LevelPackage, UBOOL bAddPerLevelDataMap )
{
	if ( LevelPackage != NULL )
	{
		// set MapUsedIn
		MapsUsedIn.AddUniqueItem( LevelPackage->GetFullName() );

		// if to collect data per level, save level map data
		if ( bAddPerLevelDataMap )
		{
			UINT* NumInst = LevelNameToInstanceCount.Find( LevelPackage->GetOutermost()->GetName() );
			if( NumInst != NULL )
			{
				LevelNameToInstanceCount.Set( LevelPackage->GetOutermost()->GetName(), ++(*NumInst) );
			}
			else
			{
				LevelNameToInstanceCount.Set( LevelPackage->GetOutermost()->GetName(), 1 );
			}
		}
	}
}

/** Constructor, initializing all members. */
FAnalyzeReferencedContentStat::FStaticMeshStats::FStaticMeshStats( UStaticMesh* StaticMesh )
:	ResourceType(StaticMesh->GetClass()->GetName())
,	ResourceName(StaticMesh->GetPathName())
,	NumInstances(0)
,	NumTriangles(0)
,	NumSections(0)
,   NumConvexPrimitives(0)
,   bUsesSimpleRigidBodyCollision(StaticMesh->UseSimpleRigidBodyCollision)
,   NumElementsWithCollision(0)
,	bIsReferencedByScript(FALSE)
,   bIsReferencedByParticles(FALSE)
,	ResourceSize(StaticMesh->GetResourceSize())
,   bIsMeshNonUniformlyScaled(FALSE)
,   bShouldConvertBoxColl(FALSE)
,	bUsesStripkDOPForConsole( StaticMesh->bStripkDOPForConsole )
{
	// Update triangle and section counts.
	for( INT ElementIndex=0; ElementIndex<StaticMesh->LODModels(0).Elements.Num(); ElementIndex++ )
	{
		const FStaticMeshElement& StaticMeshElement = StaticMesh->LODModels(0).Elements(ElementIndex);
		NumElementsWithCollision += StaticMeshElement.EnableCollision ? 1 : 0;
		NumTriangles += StaticMeshElement.NumTriangles;
		NumSections++;
	}

	if(StaticMesh->BodySetup)
	{
		NumConvexPrimitives = StaticMesh->BodySetup->AggGeom.ConvexElems.Num();
	}
}



/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FStaticMeshStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,NumInstances,NumTriangles,NumSections,NumSectionsWithCollision,UsesSimpleRigidBodyCollision,ConvexCollisionPrims,NumMapsUsedIn,ResourceSize,ScalesUsed,NonUniformScale,ShouldConvertBoxColl,bIsReferencedByParticles,UsesStripkDOPForConsole") LINE_TERMINATOR;

	// we would like to have this in the "Instanced Triangles" column but the commas make it auto parsed into the next column for the CSV
	//=Table1[[#This Row],[NumInstances]]*Table1[[#This Row],[NumTriangles]]
}

/**
*  This is for summary
*/
FString FAnalyzeReferencedContentStat::FStaticMeshStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,NumInstances(Total),NumTriangles(Avg),NumTriangles(Std),NumSections(Avg),NumSectionsWithCollision(Avg),UsesSimpleRigidBodyCollision(%),ConvexCollisionPrims(Avg),ResourceSize(Avg),ResourceSize(Std),ScalesUsed(%),UseNonUniformScale(%),UseShouldConvertBoxColl(%),IsReferencedByParticles(%),UsesStripkDOPForConsole(%)") LINE_TERMINATOR;

	// we would like to have this in the "Instanced Triangles" column but the commas make it auto parsed into the next column for the CSV
	//=Table1[[#This Row],[NumInstances]]*Table1[[#This Row],[NumTriangles]]
}

FString FAnalyzeReferencedContentStat::FStaticMeshStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FStaticMeshStats>& StatsData)
{
	FLOAT TotalNumInstances=0, ResultNumInstances=0;
	FLOAT TotalNumTriangles=0, ResultNumTriangles=0;
	FLOAT TotalNumSections=0, ResultNumSections=0;
	FLOAT TotalNumConvexPrimitives=0, ResultNumConvexPrimitives=0;
	FLOAT TotalNumElementsWithCollision=0, ResultNumElementsWithCollision=0;
	FLOAT TotalResourceSize=0, ResultResourceSize=0;
	FLOAT TotalUsedAtScales=0, ResultUsedAtScales=0;

	FLOAT TotalIsMeshNonUniformlyScaled=0, ResultIsMeshNonUniformlyScaled=0;
	FLOAT TotalShouldConvertBoxColl=0, ResultShouldConvertBoxColl=0;
	FLOAT TotalUsesSimpleRigidBodyCollision=0, ResultUsesSimpleRigidBodyCollision=0;
	FLOAT TotalIsReferencedByParticles=0, ResultIsReferencedByParticles=0;
	FLOAT TotalUsesStripkDOPForConsole=0, ResultUsesStripkDOPForConsole=0;

	INT TotalCount=0;

	for( TMap<FString,FStaticMeshStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FStaticMeshStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName )!=NULL )
		{
			TotalNumInstances+=StatsEntry.NumInstances;
			TotalNumTriangles+=StatsEntry.NumTriangles;
			TotalNumSections+=StatsEntry.NumSections;
			TotalNumConvexPrimitives+=StatsEntry.NumConvexPrimitives;
			TotalUsesSimpleRigidBodyCollision+=(StatsEntry.bUsesSimpleRigidBodyCollision==TRUE);
			TotalNumElementsWithCollision+=StatsEntry.NumElementsWithCollision;
			TotalIsReferencedByParticles+=(StatsEntry.bIsReferencedByParticles==TRUE);
			TotalResourceSize+=StatsEntry.ResourceSize;
			TotalIsMeshNonUniformlyScaled+=(StatsEntry.bIsMeshNonUniformlyScaled==TRUE);
			TotalShouldConvertBoxColl+=(StatsEntry.bShouldConvertBoxColl==TRUE);
			TotalUsedAtScales+=(StatsEntry.UsedAtScales.Num()>0);
			TotalUsesStripkDOPForConsole+=(StatsEntry.bUsesStripkDOPForConsole==TRUE);
			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		ResultNumInstances = TotalNumInstances;
		ResultNumTriangles = TotalNumTriangles/TotalCount;
		ResultNumSections = TotalNumSections/TotalCount;
		ResultNumConvexPrimitives = TotalNumConvexPrimitives/TotalCount;
		ResultNumElementsWithCollision = TotalNumElementsWithCollision/TotalCount;
		ResultResourceSize = TotalResourceSize/TotalCount;
		ResultUsedAtScales = TotalUsedAtScales/TotalCount;

		ResultIsMeshNonUniformlyScaled = TotalIsMeshNonUniformlyScaled/TotalCount;
		ResultShouldConvertBoxColl = TotalShouldConvertBoxColl/TotalCount;
		ResultUsesSimpleRigidBodyCollision = TotalUsesSimpleRigidBodyCollision/TotalCount;
		ResultIsReferencedByParticles = TotalIsReferencedByParticles/TotalCount;
		ResultUsesStripkDOPForConsole = TotalUsesStripkDOPForConsole/TotalCount;
	}

	// Get STD DEV for NumTriangles and resourcesize
	FLOAT AvgResourceSize = ResultResourceSize;
	FLOAT AvgNumTriangles = ResultNumTriangles;

	if (TotalCount > 1)
	{
		TotalResourceSize = 0;
		TotalNumTriangles = 0;

		for( TMap<FString,FStaticMeshStats>::TConstIterator It(StatsData); It; ++ It )
		{
			const FStaticMeshStats& StatsEntry = It.Value();

			if (LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find(LevelName) != 0)
			{
				TotalResourceSize += (StatsEntry.ResourceSize-AvgResourceSize)*(StatsEntry.ResourceSize-AvgResourceSize);
				TotalNumTriangles += (StatsEntry.NumTriangles-AvgNumTriangles)*(StatsEntry.NumTriangles-AvgNumTriangles);
			}
		}

		ResultResourceSize = appPow(TotalResourceSize/(TotalCount-1), 0.5f);
		ResultNumTriangles = appPow(TotalNumTriangles/(TotalCount-1), 0.5f);
	}

	return FString::Printf(TEXT("%s,%i,%i,%0.2f,%0.2f,%0.2f,%0.2f,%i,%0.2f,%0.2f,%0.2f,%i,%i,%i,%i,%i%s"), 
		*LevelName, (INT)TotalCount, (INT)ResultNumInstances, AvgNumTriangles, ResultNumTriangles, ResultNumSections, ResultNumElementsWithCollision, (INT)(ResultUsesSimpleRigidBodyCollision*100), 
		ResultNumConvexPrimitives, AvgResourceSize, ResultResourceSize, (INT)(ResultUsedAtScales*100), (INT)(ResultIsMeshNonUniformlyScaled*100), (INT)(ResultShouldConvertBoxColl*100), 
		(INT)(ResultIsReferencedByParticles*100),
		(INT)(ResultUsesStripkDOPForConsole*100),
		LINE_TERMINATOR);
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FStaticMeshStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%d,%i,%i,%i,%i%s"),
		*ResourceType,
		*ResourceName,
		NumInstances,
		NumTriangles,
		NumSections,
		NumElementsWithCollision,
		bUsesSimpleRigidBodyCollision,
		NumConvexPrimitives,
		MapsUsedIn.Num(),
		ResourceSize,
		UsedAtScales.Num(),
		bIsMeshNonUniformlyScaled,
		bShouldConvertBoxColl,
		bIsReferencedByParticles,
		bUsesStripkDOPForConsole,
		LINE_TERMINATOR);
}

/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
FString FAnalyzeReferencedContentStat::FStaticMeshStats::ToCSV( const FString& LevelName ) const
{
	UINT* NumInst = const_cast<UINT*>(LevelNameToInstanceCount.Find( LevelName ));
	if( NumInst == NULL )
	{
		*NumInst = 0;
	}

	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%d,%i,%i,%i,%i%s"),
		*ResourceType,
		*ResourceName,
		*NumInst,
		NumTriangles,
		NumSections,
		NumElementsWithCollision,
		bUsesSimpleRigidBodyCollision,
		NumConvexPrimitives,
		MapsUsedIn.Num(),
		ResourceSize,
		UsedAtScales.Num(),
		bIsMeshNonUniformlyScaled,
		bShouldConvertBoxColl,
		bIsReferencedByParticles,
		bUsesStripkDOPForConsole,
		LINE_TERMINATOR);
}

/** Constructor, initializing all members. */
FAnalyzeReferencedContentStat::FPrimitiveStats::FPrimitiveStats( UModel * Model )
:	ResourceType(Model->GetClass()->GetName())
,	ResourceName(Model->GetPathName())
,	NumTriangles(0)
,	NumInstances(1)
,	NumSections(0)
,	ResourceSize(Model->GetResourceSize())
{
	// We count Model as one resource, but triangle counts = sum of all modelcomponents
	// similar as what terrain does. 
}

FAnalyzeReferencedContentStat::FPrimitiveStats::FPrimitiveStats( UDecalComponent * DecalComponent )
:	ResourceType(DecalComponent->GetClass()->GetName())
,	ResourceName(DecalComponent->GetPathName())
,	NumTriangles(0)
,	NumInstances(0)
,	NumSections(0)
,	ResourceSize(DecalComponent->GetResourceSize())
{
	for( INT ReceiverIdx=0; ReceiverIdx < DecalComponent->StaticReceivers.Num(); ReceiverIdx++ )
	{
		const FStaticReceiverData* DecalReceiver = DecalComponent->StaticReceivers(ReceiverIdx);
		
		if ( DecalReceiver->Component->IsA(UTerrainComponent::StaticClass()) )
		{
			// Terrain decal gets created during run time. Here, we mimic the behavior and gets the triangle counts
			// Do not increase section count if triangle == 0
			INT TerrainNumTriangle = GetNumTrianglesForTerrain( DecalComponent, Cast<UTerrainComponent>(DecalReceiver->Component), DecalReceiver->NumTriangles );			
			if ( TerrainNumTriangle > 0 )
			{
				NumTriangles += TerrainNumTriangle;
				++NumSections;
			}
		}
		else
		{
			// every receiver counts as a section since it will be rendered separately
			++NumSections;
			NumTriangles += DecalReceiver->NumTriangles;
		}
	}
}

// get # of triangle for terrain
UINT FAnalyzeReferencedContentStat::FPrimitiveStats::GetNumTrianglesForTerrain( UDecalComponent * DecalComponent, UTerrainComponent * TerrainComponent, UINT DefaultNumTriangle )
{
	check ( TerrainComponent );

	return TerrainComponent->GetTriangleCountForDecal( DecalComponent );
}

FAnalyzeReferencedContentStat::FPrimitiveStats::FPrimitiveStats( ATerrain* Terrain )
:	NumTriangles(0)
// For each terrain, it should be only one, but each component counts for section
,	NumInstances(1)
,	NumSections(0)
{
	ResourceType = Terrain->GetClass()->GetName();
	ResourceName = Terrain->GetPathName();
	ResourceSize = Terrain->GetResourceSize();
}

FAnalyzeReferencedContentStat::FPrimitiveStats::FPrimitiveStats( USpeedTreeComponent * SpeedTreeComponent )
:	NumTriangles(0)
,	NumInstances(0)
,	NumSections(0)
{
#if WITH_SPEEDTREE
	if( SpeedTreeComponent->SpeedTree )
	{
		ResourceType = SpeedTreeComponent->SpeedTree->GetClass()->GetName();
		ResourceName = SpeedTreeComponent->SpeedTree->GetPathName();
		ResourceSize = SpeedTreeComponent->SpeedTree->GetResourceSize();

		FSpeedTreeResourceHelper* SRH = SpeedTreeComponent->SpeedTree->SRH;

		if ( SRH )
		{
			if( SpeedTreeComponent->bUseBranches && SRH->bHasBranches )
			{
				if (SRH->Branch1Elements.Num() > 0)
				{
					NumSections++;
					NumTriangles += SRH->Branch1Elements(0).GetNumPrimitives();	
				}
				if (SRH->Branch2Elements.Num() > 0)
				{
					NumSections++;
					NumTriangles += SRH->Branch2Elements(0).GetNumPrimitives();	
				}
			}
			if( SpeedTreeComponent->bUseFronds && SRH->bHasFronds )
			{
				NumSections++;
				NumTriangles += SRH->FrondElements(0).GetNumPrimitives();	
			}
			if( SpeedTreeComponent->bUseLeafCards && SRH->bHasLeafCards && SRH->LeafCardElements.Num() )
			{
				NumTriangles += SRH->LeafCardElements(0).GetNumPrimitives();	
				NumSections++;
			}
			if( SpeedTreeComponent->bUseLeafMeshes && SRH->bHasLeafMeshes && SRH->LeafMeshElements.Num() )
			{
				NumTriangles += SRH->LeafMeshElements(0).GetNumPrimitives();	
				NumSections++;
			}
		}
	}
	else	
	{
		ResourceType = SpeedTreeComponent->GetClass()->GetName();
		ResourceName = SpeedTreeComponent->GetPathName();
		ResourceSize = SpeedTreeComponent->GetResourceSize();
	}

#endif
}

FAnalyzeReferencedContentStat::FPrimitiveStats::FPrimitiveStats( USkeletalMesh * SkeletalMesh, FAnalyzeReferencedContentStat::FSkeletalMeshStats * SkeletalMeshStats )
:	ResourceType(SkeletalMesh->GetClass()->GetName())
,	ResourceName(SkeletalMesh->GetPathName())
,	NumTriangles(0)
,	NumInstances(0)
,	NumSections(0)
,	ResourceSize(SkeletalMesh->GetResourceSize())
{
	// this code assumes that skeletalmesh stat is added before primitive stat is added
	// please make sure not to change order of which one getting added first
	// if this doesn't exist, the code doesn't work
	// to avoid dup code, I'm getting value from skeletalmesh stat
	check (SkeletalMeshStats );
	NumTriangles = SkeletalMeshStats ->NumTriangles;
	NumSections = SkeletalMeshStats ->NumSections;
}

FAnalyzeReferencedContentStat::FPrimitiveStats::FPrimitiveStats( UStaticMesh* StaticMesh, FAnalyzeReferencedContentStat::FStaticMeshStats * StaticMeshStats  )
:	ResourceType(StaticMesh->GetClass()->GetName())
,	ResourceName(StaticMesh->GetPathName())
,	NumTriangles(0)
,	NumInstances(0)
,	NumSections(0)
,	ResourceSize(StaticMesh->GetResourceSize())
{
	// this code assumes that Staticmesh stat is added before primitive stat is added
	// please make sure not to change order of which one getting added first
	// if this doesn't exist, the code doesn't work
	// to avoid dup code, I'm getting value from skeletalmesh stat
	check (StaticMeshStats);
	NumTriangles = StaticMeshStats->NumTriangles;
	NumSections = StaticMeshStats->NumSections;
}
/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FPrimitiveStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,Instance Count,Section Count,Triangles Count,Number of Maps Used In,Resource Size") LINE_TERMINATOR;

	// we would like to have this in the "Instanced Triangles" column but the commas make it auto parsed into the next column for the CSV
	//=Table1[[#This Row],[NumInstances]]*Table1[[#This Row],[NumTriangles]]
}

/**
*  This is for summary
*/
FString FAnalyzeReferencedContentStat::FPrimitiveStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,Total Count(Unique),Total Instances Count,Total Triangles Count(Unique),Total Section Count(Unique), Total Instanced Section Count,Total Instanced Triangle Count,Total Resource Size(Unique)") LINE_TERMINATOR;

	// we would like to have this in the "Instanced Triangles" column but the commas make it auto parsed into the next column for the CSV
	//=Table1[[#This Row],[NumInstances]]*Table1[[#This Row],[NumTriangles]]
}

FString FAnalyzeReferencedContentStat::FPrimitiveStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FPrimitiveStats>& StatsData)
{
	FLOAT TotalNumTriangles=0;
	FLOAT TotalNumInstances=0;
	FLOAT TotalNumSections=0;
	FLOAT TotalResourceSize=0, AverageResourceSize=0;
	FLOAT TotalNumInstancedTriangles=0;
	FLOAT TotalNumInstancedSections=0;

	INT TotalCount=0;

	for( TMap<FString,FPrimitiveStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FPrimitiveStats& StatsEntry = It.Value();

		UINT NumInstance=0;
		const UINT* NumInst = StatsEntry.LevelNameToInstanceCount.Find( LevelName);
		if ( LevelName==TEXT("Total") )
		{
			NumInstance = StatsEntry.NumInstances;
		}
		if ( NumInst!=NULL )
		{
			NumInstance = *NumInst;
		}

		if( NumInstance > 0 )
		{
			TotalNumTriangles+=StatsEntry.NumTriangles;
			TotalNumInstances+=NumInstance;
			TotalNumSections+=StatsEntry.NumSections;
			TotalResourceSize+=StatsEntry.ResourceSize;
			TotalNumInstancedTriangles+=NumInstance*StatsEntry.NumTriangles;
			TotalNumInstancedSections+=NumInstance*StatsEntry.NumSections;

			++TotalCount;
		}
	}

	return FString::Printf(TEXT("%s,%i,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f%s"), 
		*LevelName, (INT)TotalCount, TotalNumInstances, TotalNumTriangles, TotalNumSections, TotalNumInstancedSections, TotalNumInstancedTriangles, TotalResourceSize,LINE_TERMINATOR);
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FPrimitiveStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i%s"),
		*ResourceType,
		*ResourceName,
		NumInstances,
		NumSections,
		NumTriangles,
		MapsUsedIn.Num(),
		ResourceSize,
		LINE_TERMINATOR);
}

/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
FString FAnalyzeReferencedContentStat::FPrimitiveStats::ToCSV( const FString& LevelName ) const
{
	UINT* NumInst = const_cast<UINT*>(LevelNameToInstanceCount.Find( LevelName ));

	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i%s"),
		*ResourceType,
		*ResourceName,
		*NumInst,
		NumSections,
		NumTriangles,
		MapsUsedIn.Num(),
		ResourceSize,
		LINE_TERMINATOR);
}



/** Constructor, initializing all members. */
FAnalyzeReferencedContentStat::FSkeletalMeshStats::FSkeletalMeshStats( USkeletalMesh* SkeletalMesh )
:	ResourceType(SkeletalMesh->GetClass()->GetName())
,	ResourceName(SkeletalMesh->GetPathName())
,   NumInstances(0)
,   NumTriangles(0)
,   NumVertices(0)
,   NumRigidVertices(0)
,   NumSoftVertices(0)
,   NumSections(0)
,   NumChunks(0)
,	MaxBoneInfluences(0)
,   NumActiveBoneIndices(0)
,   NumRequiredBones(0)
,   NumMaterials(0)
,   bUsesPerPolyBoneCollision(FALSE)
,	bIsReferencedByScript(FALSE)
,   bIsReferencedByParticles(FALSE)
,	ResourceSize(SkeletalMesh->GetResourceSize())
,	VertexMemorySize(0)
,	IndexMemorySize(0)
,	VertexInfluencsSize(0)
{
	// currently we only count for LOD 0
	NumTriangles = SkeletalMesh->LODModels(0).GetTotalFaces();
	NumSections = SkeletalMesh->LODModels(0).Sections.Num();
	NumChunks = SkeletalMesh->LODModels(0).Chunks.Num();
	for (INT ChunkIndex = 0; ChunkIndex < NumChunks; ChunkIndex++)
	{
		const FSkelMeshChunk& SkelMeshChunk = SkeletalMesh->LODModels(0).Chunks(ChunkIndex);
		NumRigidVertices += SkelMeshChunk.GetNumRigidVertices();
		NumSoftVertices += SkelMeshChunk.GetNumSoftVertices();
		NumVertices += SkelMeshChunk.GetNumVertices();

		// If this keeps coming up zero, need to call CalcMaxBoneInfluences first
		MaxBoneInfluences = (MaxBoneInfluences > SkelMeshChunk.MaxBoneInfluences ? MaxBoneInfluences : SkelMeshChunk.MaxBoneInfluences);
	}

	VertexMemorySize = SkeletalMesh->LODModels(0).VertexBufferGPUSkin.GetNumVertices() * SkeletalMesh->LODModels(0).VertexBufferGPUSkin.GetStride();
	IndexMemorySize = SkeletalMesh->LODModels(0).MultiSizeIndexContainer.GetIndexBuffer()->GetResourceDataSize();
	
	for (INT I=0; I<SkeletalMesh->LODModels(0).VertexInfluences.Num(); ++I)
	{
		VertexInfluencsSize += SkeletalMesh->LODModels(0).VertexInfluences(I).Influences.Num()*sizeof(FVertexInfluence);
	}

	NumActiveBoneIndices = SkeletalMesh->LODModels(0).ActiveBoneIndices.Num();
	NumRequiredBones = SkeletalMesh->LODModels(0).RequiredBones.Num();
	NumMaterials = SkeletalMesh->Materials.Num();

	if ((SkeletalMesh->bUseSimpleLineCollision == 0) &&
		(SkeletalMesh->bUseSimpleBoxCollision == 0) &&
		(SkeletalMesh->PerPolyCollisionBones.Num() > 0))
	{
		bUsesPerPolyBoneCollision = TRUE;
	}
}



/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FSkeletalMeshStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,NumInstances,NumTriangles,NumVertices,NumRigidVertices,NumSoftVertices,NumSections,NumChunks,VertexBufferSize,IndexBufferSize,VertexInfluencesSize,MaxBoneInfluences,NumActiveBoneIndices,NumRequiredBones,NumMaterials,bUsesPerPolyBoneCollision,bIsReferencedByScript,bIsReferencedByParticles,ResourceSize") LINE_TERMINATOR;
}


/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FSkeletalMeshStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n"),
		*ResourceType,
		*ResourceName,
		NumInstances,
		NumTriangles,
		NumVertices,
		NumRigidVertices,
		NumSoftVertices,
		NumSections,
		NumChunks,
		VertexMemorySize, 
		IndexMemorySize, 
		VertexInfluencsSize,
		MaxBoneInfluences,
		NumActiveBoneIndices,
		NumRequiredBones,
		NumMaterials,
		bUsesPerPolyBoneCollision,
		bIsReferencedByScript,
		bIsReferencedByParticles,
		ResourceSize);
}

/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
FString FAnalyzeReferencedContentStat::FSkeletalMeshStats::ToCSV( const FString& LevelName ) const
{
	UINT* NumInst = const_cast<UINT*>(LevelNameToInstanceCount.Find( LevelName ));
	if( NumInst == NULL )
	{
		*NumInst = 0;
	}

	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i\r\n"),
		*ResourceType,
		*ResourceName,
		*NumInst,
		NumTriangles,
		NumVertices,
		NumRigidVertices,
		NumSoftVertices,
		NumSections,
		NumChunks,
		VertexMemorySize, 
		IndexMemorySize, 
		VertexInfluencsSize,
		MaxBoneInfluences,
		NumActiveBoneIndices,
		NumRequiredBones,
		NumMaterials,
		bUsesPerPolyBoneCollision,
		bIsReferencedByScript,
		bIsReferencedByParticles,
		ResourceSize);
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FSkeletalMeshStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,NumInstances(Total),NumTriangles(Avg),NumVertices(Avg),NumRigidVertices(Avg),NumSoftVertices(Avg),NumSections(Avg),NumChunks(Avg),VertexBufferSize(Avg),IndexBufferSize(Avg),MaxBoneInfluences(Avg),NumActiveBoneIndices(Avg),NumRequiredBones(Avg),NumMaterials(Avg),UsesPerPolyBoneCollision(%),IsReferencedByScript(%),IsReferencedByParticles(%),ResourceSize(Avg)") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FSkeletalMeshStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FSkeletalMeshStats>& StatsData)
{
	FLOAT TotalNumInstances=0,
		TotalNumTriangles=0,
		TotalNumVertices=0,
		TotalNumRigidVertices=0,
		TotalNumSoftVertices=0,
		TotalNumSections=0,
		TotalVertexBufferSize=0,
		TotalIndexBufferSize=0,
		TotalNumChunks=0,
		TotalMaxBoneInfluences=0,
		TotalNumActiveBoneIndices=0,
		TotalNumRequiredBones=0,
		TotalNumMaterials=0,
		TotalUsesPerPolyBoneCollision=0,
		TotalIsReferencedByScript=0,
		TotalIsReferencedByParticles=0,
		TotalResourceSize=0;

	INT TotalCount=0;

	for( TMap<FString,FSkeletalMeshStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FSkeletalMeshStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName )!=NULL )
		{
			TotalNumInstances+=StatsEntry.NumInstances;
			TotalNumTriangles+=StatsEntry.NumTriangles;
			TotalNumVertices+=StatsEntry.NumVertices;
			TotalNumRigidVertices+=StatsEntry.NumRigidVertices;
			TotalNumSoftVertices+=StatsEntry.NumSoftVertices;
			TotalVertexBufferSize+=StatsEntry.VertexMemorySize;
			TotalIndexBufferSize+=StatsEntry.IndexMemorySize;
			TotalNumSections+=StatsEntry.NumSections;
			TotalNumChunks+=StatsEntry.NumChunks;
			TotalMaxBoneInfluences+=StatsEntry.MaxBoneInfluences;
			TotalNumActiveBoneIndices+=StatsEntry.NumActiveBoneIndices;
			TotalNumRequiredBones+=StatsEntry.NumRequiredBones;
			TotalNumMaterials+=StatsEntry.NumMaterials;
			TotalUsesPerPolyBoneCollision+=(StatsEntry.bUsesPerPolyBoneCollision==TRUE)?1:0;
			TotalIsReferencedByScript+=(StatsEntry.bIsReferencedByScript==TRUE)?1:0;
			TotalIsReferencedByParticles+=(StatsEntry.bIsReferencedByParticles==TRUE)?1:0;
			TotalResourceSize+=StatsEntry.ResourceSize;
			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		TotalNumTriangles/=TotalCount;
		TotalNumVertices/=TotalCount;
		TotalNumRigidVertices/=TotalCount;
		TotalNumSoftVertices/=TotalCount;
		TotalNumSections/=TotalCount;
		TotalNumChunks/=TotalCount;
		TotalMaxBoneInfluences/=TotalCount;
		TotalNumActiveBoneIndices/=TotalCount;
		TotalNumRequiredBones/=TotalCount;
		TotalNumMaterials/=TotalCount;
		TotalUsesPerPolyBoneCollision/=TotalCount;
		TotalIsReferencedByScript/=TotalCount;
		TotalIsReferencedByParticles/=TotalCount;
		TotalResourceSize/=TotalCount;
		TotalVertexBufferSize/=TotalCount;
		TotalIndexBufferSize/=TotalCount;
	}

	return FString::Printf(TEXT("%s,%i,%i,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%i,%i,%i,%0.2f%s"), 
		*LevelName, TotalCount, (INT)TotalNumInstances, TotalNumTriangles,TotalNumVertices,	TotalNumRigidVertices,TotalNumSoftVertices,TotalNumSections,TotalNumChunks,TotalVertexBufferSize,TotalIndexBufferSize,TotalMaxBoneInfluences,
		TotalNumActiveBoneIndices,TotalNumRequiredBones,TotalNumMaterials,(INT)(TotalUsesPerPolyBoneCollision*100),(INT)(TotalIsReferencedByScript*100),(INT)(TotalIsReferencedByParticles*100),
		TotalResourceSize, LINE_TERMINATOR);
}


//Initialization of static member in FLightingOptimizationInfo
const INT FAnalyzeReferencedContentStat::FLightingOptimizationStats::LightMapSizes[NumLightmapTextureSizes] = { 256, 128, 64, 32 };
static const INT BytesUsedThreshold = 5000;

/**
*   Calculate the memory required to light a mesh with given NumVertices using vertex lighting
*/
INT FAnalyzeReferencedContentStat::FLightingOptimizationStats::CalculateVertexLightingBytesUsed(INT NumVertices)
{
	//3 color channels are  (3 colors * 4 bytes/color = 12 bytes)
	//1 color channel is    (1 color * 4 bytes/color = 4 bytes)
	const INT VERTEX_LIGHTING_DATA_SIZE = GSystemSettings.bAllowDirectionalLightMaps ? sizeof(FQuantizedDirectionalLightSample) : sizeof(FQuantizedSimpleLightSample);

	//BytesUsed = (Number of Vertices) * sizeof(VertexLightingData)
	INT BytesUsed = NumVertices * VERTEX_LIGHTING_DATA_SIZE;

	return BytesUsed;
}

/** Assuming DXT1 lightmaps...
*   4 bits/pixel * width * height = Highest MIP Level * 1.333 MIP Factor for approx usage for a full mip chain
*   Either 1 or 3 textures if we're doing simple or directional (3-axis) lightmap
*   Most lightmaps require a second UV channel which is probably an extra 4 bytes (2 floats compressed to SHORT) 
*/
INT FAnalyzeReferencedContentStat::FLightingOptimizationStats::CalculateLightmapLightingBytesUsed(INT Width, INT Height, INT NumVertices, INT UVChannelIndex)
{
	if (Width <= 0 || Height <= 0 || NumVertices <= 0)
	{
		return 0;
	}

	const FLOAT MIP_FACTOR = 4.0f / 3.0f;

	FLOAT BYTES_PER_PIXEL = 0.0f;
	if (GSystemSettings.bAllowDirectionalLightMaps)
	{
		//DXT1 4bits/pixel * 4/3 mipfactor * 3 channels = 16 bits = 2 bytes / pixel
		BYTES_PER_PIXEL = 0.5f * MIP_FACTOR * NUM_DIRECTIONAL_LIGHTMAP_COEF;
	}
	else
	{      
		//DXT1 4bits/pixel * 4/3 mipfactor * 1 channel = 16/3 bits = 0.6666 bytes / pixel
		BYTES_PER_PIXEL = 0.5f * MIP_FACTOR * NUM_SIMPLE_LIGHTMAP_COEF;
	}

	//BytesUsed = (SizeOfTexture) + (SizeOfUVData)
	//SizeOfTexture = (Width * Height * BytesPerTexel) * MIP_FACTOR
	//SizeOfUVData = (Number of Vertices * SizeOfUVCoordData)
	INT SizeOfTexture = Width * Height * BYTES_PER_PIXEL;

	INT BytesUsed = 0;
	if ( TRUE )
	{
		BytesUsed = SizeOfTexture;
	}
	else
	{
		//I'm told by Dan that most static meshes will probably already have a 2nd uv channel and it wouldn't necessarily go away
		//with the addition/subtraction of a light map, otherwise we can reenable this
		const FLOAT UV_COORD_DATA_SIZE = UVChannelIndex * 2 * sizeof(SHORT); //Index * (Tuple / UVChannel) * (2 bytes / Tuple)
		INT SizeOfUVData = NumVertices * UV_COORD_DATA_SIZE;
		BytesUsed = SizeOfTexture + SizeOfUVData;
	}

	return BytesUsed;
}

/** 
*	For a given list of parameters, compute a full spread of potential savings values using vertex light, or 256, 128, 64, 32 pixel square light maps
*  @param LMType - Current type of lighting being used
*  @param NumVertices - Number of vertices in the given mesh
*  @param Width - Width of current lightmap
*  @param Height - Height of current lightmap
*  @param TexCoordIndex - channel index of the uvs currently used for lightmaps
*  @param LOI - A struct to be filled in by the function with the potential savings
*/
void FAnalyzeReferencedContentStat::FLightingOptimizationStats::CalculateLightingOptimizationInfo(ELightMapInteractionType LMType, INT NumVertices, INT Width, INT Height, INT TexCoordIndex, FLightingOptimizationStats& LOStats)
{
	// Current Values
	LOStats.IsType = LMType;
	LOStats.TextureSize = Width;

	if (LMType == LMIT_Vertex)
	{
		LOStats.CurrentBytesUsed = CalculateVertexLightingBytesUsed(NumVertices);
	}
	else if (LMType == LMIT_Texture)
	{
		LOStats.CurrentBytesUsed = CalculateLightmapLightingBytesUsed(Width, Height, NumVertices, TexCoordIndex);
	}

	//Potential savings values
	INT VertexLitBytesUsed = CalculateVertexLightingBytesUsed(NumVertices);

	INT TextureMapBytesUsed[FLightingOptimizationStats::NumLightmapTextureSizes];
	for (INT i=0; i<FLightingOptimizationStats::NumLightmapTextureSizes; i++)
	{
		const INT TexCoordIndexAssumed = 1; //assume it will require 2 texcoord channels to do the lightmap
		TextureMapBytesUsed[i] = CalculateLightmapLightingBytesUsed(LightMapSizes[i], LightMapSizes[i], NumVertices, TexCoordIndexAssumed);
	}

	//Store this all away in a nice little struct
	for (INT i=0; i<NumLightmapTextureSizes; i++)
	{
		LOStats.BytesSaved[i] = LOStats.CurrentBytesUsed - TextureMapBytesUsed[i];
	}

	LOStats.BytesSaved[NumLightmapTextureSizes] = LOStats.CurrentBytesUsed - VertexLitBytesUsed;
}

/** 
* Calculate the potential savings for a given static mesh component by using an alternate static lighting method
*/
FAnalyzeReferencedContentStat::FLightingOptimizationStats::FLightingOptimizationStats(AStaticMeshActor* StaticMeshActor)   :
LevelName(StaticMeshActor->GetOutermost()->GetName())
,	ActorName(StaticMeshActor->GetName())
,	SMName(StaticMeshActor->StaticMeshComponent->StaticMesh->GetName())
,   IsType(LMIT_None)        
,   TextureSize(0)  
,	CurrentBytesUsed(0)
{
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->StaticMeshComponent;

	appMemzero(BytesSaved, ARRAYSIZE(BytesSaved));
	if (StaticMeshComponent && StaticMeshComponent->StaticMesh && StaticMeshComponent->HasStaticShadowing())
	{
		UStaticMesh* StaticMesh = StaticMeshComponent->StaticMesh;

		INT NumLODModels = StaticMesh->LODModels.Num();
		for (INT LODModelIndex = 0; LODModelIndex < NumLODModels; LODModelIndex++)
		{
			const FStaticMeshRenderData& StaticMeshRenderData = StaticMesh->LODModels(LODModelIndex);

			//Mesh has to have LOD data in order to even consider this calculation?
			if (LODModelIndex < StaticMeshComponent->LODData.Num())
			{
				const FStaticMeshComponentLODInfo& StaticMeshComponentLODInfo = StaticMeshComponent->LODData(LODModelIndex);

				//Again without a lightmap, don't bother?
				if (StaticMeshComponentLODInfo.LightMap != NULL)
				{
					//What is the mesh currently using
					FLightMapInteraction LMInteraction = StaticMeshComponentLODInfo.LightMap->GetInteraction();
					IsType = LMInteraction.GetType();

					INT Width  = 0;
					INT Height = 0;
					//Returns the correct (including overrides) width/height for the lightmap
					StaticMeshComponent->GetLightMapResolution(Width, Height);

					//Get the number of vertices used by this static mesh
					INT NumVertices = StaticMeshRenderData.NumVertices;

					//Get the number of uv coordinates stored in the vertex buffer
					INT TexCoordIndex = StaticMesh->LightMapCoordinateIndex; 

					CalculateLightingOptimizationInfo(IsType, NumVertices, Width, Height, TexCoordIndex, *this);
				}
			}
		} //for each lodmodel
	}
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FLightingOptimizationStats::ToCSV() const
{
	FString CSVString;
	if (CurrentBytesUsed > BytesUsedThreshold)
	{
		FString CurrentType(TEXT("Unknown"));
		if (IsType == LMIT_Vertex)
		{
			CurrentType = TEXT("Vertex");
		}
		else
		{
			CurrentType = FString::Printf(TEXT("%d-Texture"), TextureSize);
		}

		FString BytesSavedString;
		UBOOL FoundSavings = FALSE;
		for (INT i=0; i<NumLightmapTextureSizes + 1; i++)
		{
			if (BytesSaved[i] > 0)
			{
				BytesSavedString += FString::Printf(TEXT(",%1.3f"), (FLOAT)BytesSaved[i] / 1024.0f);
				FoundSavings = TRUE;
			}
			else
			{
				BytesSavedString += TEXT(",");
			}
		}

		if (FoundSavings)
		{
			CSVString = FString::Printf(TEXT("%s,%s,%s,%s,%1.3f"), *LevelName, *ActorName, *SMName, *CurrentType, (FLOAT)CurrentBytesUsed/1024.0f);
			CSVString += BytesSavedString;
			CSVString += LINE_TERMINATOR;
		}
	}

	return CSVString;
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FLightingOptimizationStats::GetCSVHeaderRow()
{
	FString CSVHeaderString = FString::Printf(TEXT("LevelName,StaticMeshActor,StaticMesh,CurrentLightingType,CurrentUsage(kb)"));
	for (INT i=0; i<NumLightmapTextureSizes; i++)
	{
		CSVHeaderString += FString::Printf(TEXT(",%d-Texture Savings"), LightMapSizes[i]);
	}

	CSVHeaderString += FString::Printf(TEXT(",Vertex Savings"));
	CSVHeaderString += LINE_TERMINATOR;
	return CSVHeaderString;
}

/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FShadowMap1DStats::FShadowMap1DStats(UShadowMap1D* ShadowMap1D)
:	ResourceType(ShadowMap1D->GetClass()->GetName())
,	ResourceName(ShadowMap1D->GetPathName())
,	ResourceSize(0)
,	NumSamples(0)
,	UsedByLight(TEXT("None"))
{
	NumSamples = ShadowMap1D->NumSamples();
	FArchiveCountMem CountAr(ShadowMap1D);
	ResourceSize = CountAr.GetMax();

	// find the light for this shadow map	
	for( TObjectIterator<ULightComponent> It; It; ++It )
	{
		const ULightComponent* Light = *It;
		if( Light->LightGuid == ShadowMap1D->GetLightGuid() )
		{
			if( Light->GetOwner() )
			{
				UsedByLight = Light->GetOwner()->GetName();
			}
			break;
		}
	}
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FShadowMap1DStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%s%s"),
		*ResourceType,
		*ResourceName,						
		ResourceSize,
		NumSamples,		
		*UsedByLight,
		LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FShadowMap1DStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,ResourceSize,NumSamples,UsedByLight") LINE_TERMINATOR;
}

/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FShadowMap2DStats::FShadowMap2DStats(UShadowMap2D* ShadowMap2D)
:	ResourceType(ShadowMap2D->GetClass()->GetName())
,	ResourceName(ShadowMap2D->GetPathName())
,	ShadowMapTexture2D(TEXT("None"))
,	ShadowMapTexture2DSizeX(0)
,	ShadowMapTexture2DSizeY(0)
,	ShadowMapTexture2DFormat(TEXT("None"))
,	UsedByLight(TEXT("None"))
{	
	if( ShadowMap2D->IsValid() )
	{
		UShadowMapTexture2D* ShadowTex2D = ShadowMap2D->GetTexture();
		ShadowMapTexture2DSizeX = ShadowTex2D->SizeX;
		ShadowMapTexture2DSizeY = ShadowTex2D->SizeY;
		ShadowMapTexture2DFormat = FString(GPixelFormats[ShadowTex2D->Format].Name ? GPixelFormats[ShadowTex2D->Format].Name : TEXT("None"));
		ShadowMapTexture2D = ShadowTex2D->GetPathName();
	}

	// find the light for this shadow map	
	for( TObjectIterator<ULightComponent> It; It; ++It )
	{
		const ULightComponent* Light = *It;
		if( Light->LightGuid == ShadowMap2D->GetLightGuid() )
		{
			if( Light->GetOwner() )
			{
				UsedByLight = Light->GetOwner()->GetName();
			}
			break;
		}
	}
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FShadowMap2DStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%s,%i,%i,%s,%s%s"),
		*ResourceType,
		*ResourceName,						
		*ShadowMapTexture2D,		
		ShadowMapTexture2DSizeX,
		ShadowMapTexture2DSizeY,
		*ShadowMapTexture2DFormat,
		*UsedByLight,
		LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FShadowMap2DStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,ShadowMapTexture2D,SizeX,SizeY,Format,UsedByLight") LINE_TERMINATOR;
}

/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FTextureStats::FTextureStats( UTexture* Texture )
:	ResourceType(Texture->GetClass()->GetName())
,	ResourceName(Texture->GetPathName())
,	bIsReferencedByScript(FALSE)
,	ResourceSize(Texture->GetResourceSize())
,	LODBias(Texture->LODBias)
,	LODGroup(Texture->LODGroup)
,	Format(TEXT("UNKOWN"))
{
	// Update format.
	UTexture2D* Texture2D = Cast<UTexture2D>(Texture);
	if( Texture2D )
	{
		Format = GPixelFormats[Texture2D->Format].Name;
	}
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FTextureStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%s%s"),
		*ResourceType,
		*ResourceName,						
		MaterialsUsedBy.Num(),
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		LODBias,
		LODGroup,
		*Format,
		LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FTextureStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,NumMaterialsUsedBy,ScriptReferenced,NumMapsUsedIn,ResourceSize,LODBias,LODGroup,Format") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FTextureStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,NumMaterialsUsedBy(Total),ScriptReferenced(%),ResourceSize(Avg),UseLODBias(%),UseLODGroup(%),DXT1(%),DXT5(%),A8R8G8B8(%),G8(%),UNKNOWN(%)") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FTextureStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FTextureStats>& StatsData)
{
	FLOAT TotalMaterialUsedBy=0, 
		TotalIsReferencedByScript=0,
		TotalResourceSize=0,
		TotalUseLODBias=0,
		TotalUseLODGroup=0,
		TotalFormatDXT1=0,
		TotalFormatDXT5=0,
		TotalFormatA8R8G8B8=0,
		TotalFormatG8=0,
		TotalFormatUnknown=0;

	INT TotalCount=0;

	for( TMap<FString,FTextureStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FTextureStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
			TotalMaterialUsedBy+=StatsEntry.MaterialsUsedBy.Num();
			TotalIsReferencedByScript+=(StatsEntry.bIsReferencedByScript==TRUE)?1:0;
			TotalResourceSize+=StatsEntry.ResourceSize;
			TotalUseLODBias+=(StatsEntry.LODBias!=0);
			TotalUseLODGroup+=(StatsEntry.LODGroup!=0);
			if (StatsEntry.Format == TEXT("DXT1"))
			{
				TotalFormatDXT1+=1;
			}
			else if (StatsEntry.Format == TEXT("DXT5"))
			{
				TotalFormatDXT5+=1;
			}
			else if (StatsEntry.Format == TEXT("A8R8G8B8"))
			{
				TotalFormatA8R8G8B8+=1;
			}
			else if (StatsEntry.Format == TEXT("G8"))
			{
				TotalFormatG8+=1;
			}
			else
			{
				TotalFormatUnknown+=1;
			}

			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		TotalIsReferencedByScript/=TotalCount;
		TotalResourceSize/=TotalCount;
		TotalUseLODBias/=TotalCount;
		TotalUseLODGroup/=TotalCount;
		TotalFormatDXT1/=TotalCount;
		TotalFormatDXT5/=TotalCount;
		TotalFormatA8R8G8B8/=TotalCount;
		TotalFormatG8/=TotalCount;
		TotalFormatUnknown/=TotalCount;
	}

	return FString::Printf(TEXT("%s,%i,%0.2f,%i,%0.2f,%i,%i,%i,%i,%i,%i,%i%s"), 
		*LevelName, (INT)TotalCount, TotalMaterialUsedBy, (INT)(TotalIsReferencedByScript*100), TotalResourceSize,(INT)(TotalUseLODBias*100),
		(INT)(TotalUseLODGroup*100), (INT)(TotalFormatDXT1*100),(INT)(TotalFormatDXT5*100),(INT)(TotalFormatA8R8G8B8*100),(INT)(TotalFormatG8*100), (INT)(TotalFormatUnknown*100), LINE_TERMINATOR);
}
/**
* Static helper to return instructions used by shader type.
*
* @param	MeshShaderMap	Shader map to use to find shader of passed in type
* @param	ShaderType		Type of shader to query instruction count for
* @return	Instruction count if found, 0 otherwise
*/
static INT GetNumInstructionsForShaderType( const FMeshMaterialShaderMap* MeshShaderMap, FShaderType* ShaderType )
{
	INT NumInstructions = 0;
	const FShader* Shader = MeshShaderMap->GetShader(ShaderType);
	if( Shader )
	{
		NumInstructions = Shader->GetNumInstructions();
	}
	return NumInstructions;
}

/** Shader type for base pass pixel shader (no lightmap).  */
FShaderType*	FAnalyzeReferencedContentStat::FMaterialStats::ShaderTypeBasePassNoLightmap = NULL;
/** Shader type for base pass pixel shader (including lightmap). */
FShaderType*	FAnalyzeReferencedContentStat::FMaterialStats::ShaderTypeBasePassAndLightmap = NULL;
/** Shader type for point light with shadow map pixel shader. */
FShaderType*	FAnalyzeReferencedContentStat::FMaterialStats::ShaderTypePointLightWithShadowMap = NULL;
/** Examine flag - if this is TRUE, then it will compile material to get parameter information*/
UBOOL			FAnalyzeReferencedContentStat::FMaterialStats::TestUniqueSpecularTexture = FALSE;
/** Keeps one list of all unique shader description								*/
TArray<FString> FAnalyzeReferencedContentStat::FMaterialStats::ShaderUniqueDescriptions;

// To get specular and diffuse texture
class FTextureMaterialResource : public FMaterialResource
{
public:

	FTextureMaterialResource(UMaterial* InMaterial, UBOOL bInCompileDiffuse) :
	  FMaterialResource(InMaterial),
		  bCompileDiffuse(bInCompileDiffuse)
	  {
		  CacheShaders();
	  }

	  virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	  {
		  return FALSE;
	  }

	  // Material properties.
	  /** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	  virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	  {
		  // If the property is not active, don't compile it
		  if (!IsActiveMaterialProperty(Material, Property))
		  {
			  return INDEX_NONE;
		  }

		  Compiler->SetMaterialProperty(Property);
		  INT SelectionColorIndex = Compiler->ComponentMask(Compiler->VectorParameter(NAME_SelectionColor,FLinearColor::Black),1,1,1,0);

		  switch(Property)
		  {
		  case MP_DiffuseColor:
			  if (bCompileDiffuse)
			  {
				  return Compiler->Mul(Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3),Compiler->Sub(Compiler->Constant(1.0f),SelectionColorIndex));
			  }
			  else
			  {
				  return Compiler->Constant(1.0f);
			  }
		  case MP_SpecularColor: 
			  if (!bCompileDiffuse)
			  {
				  return Material->SpecularColor.Compile(Compiler,FColor(0,0,0));
			  }
			  else
			  {
				  return Compiler->Constant(1.0f);
			  }
		  default:
			  return Compiler->Constant(1.0f);
		  };
	  }

	  void GetTextures(TArray<UTexture*> & OutTextures, UBOOL bOnlyAddOnce)
	  {
		  OutTextures.Empty();

		  // Iterate over both the 2D textures and cube texture expressions.
		  const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >* ExpressionsByType[2] =
		  {
			  &this->GetUniform2DTextureExpressions(),
			  &this->GetUniformCubeTextureExpressions()
		  };

		  for(INT TypeIndex = 0;TypeIndex < ARRAY_COUNT(ExpressionsByType);TypeIndex++)
		  {
			  const TArray<TRefCountPtr<FMaterialUniformExpressionTexture> >& Expressions = *ExpressionsByType[TypeIndex];

			  // Iterate over each of the material's texture expressions.
			  for(INT ExpressionIndex = 0;ExpressionIndex < Expressions.Num();ExpressionIndex++)
			  {
				  FMaterialUniformExpressionTexture* Expression = Expressions(ExpressionIndex);

				  // Evaluate the expression in terms of this material instance.
				  UTexture* Texture = NULL;
				  Expression->GetGameThreadTextureValue(Material,*this,Texture);

				  // Add the expression's value to the output array.
				  if( bOnlyAddOnce )
				  {
					  OutTextures.AddUniqueItem(Texture);
				  }
				  else
				  {
					  OutTextures.AddItem(Texture);
				  }
			  }
		  }
	  }
private:
	UBOOL bCompileDiffuse;
};
/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FMaterialStats::FMaterialStats( UMaterial* Material )
:	ResourceType(Material->GetClass()->GetName())
,	ResourceName(Material->GetPathName())
,	NumBrushesAppliedTo(0)
,	NumStaticMeshInstancesAppliedTo(0)
,	NumSkeletalMeshInstancesAppliedTo(0)
,	bIsReferencedByScript(FALSE)
,	ResourceSizeOfReferencedTextures(0)
,	UseUniqueSpecularTexture(FALSE)
,	BlendMode(EBlendMode(Material->BlendMode))
{
	// Keep track of unique textures and texture sample count.
	TArray<UTexture*> SampledTextures, UniqueTextures;
	
	Material->GetUsedTextures(UniqueTextures);

	// Update texture samplers count.
	NumTextureSamples = SampledTextures.Num();

	// Update dependency chain stats.
	EMaterialShaderQuality Quality = Material->GetQualityLevel();
	check( Material->MaterialResources[Quality]);
	Material->MaterialResources[Quality]->GetRepresentativeInstructionCounts(ShaderDescriptions, InstructionCounts);

	// Keeps this list for output
	for (INT I=0; I<ShaderDescriptions.Num(); ++I)
	{
		ShaderUniqueDescriptions.AddUniqueItem(ShaderDescriptions(I));
	}

#if 0 

	MaxTextureDependencyLength = Material->MaterialResources[Quality]->GetMaxTextureDependencyLength();

	// Update instruction counts.
	const FMaterialShaderMap* MaterialShaderMap = Material->MaterialResources[Quality]->GetShaderMap();
	if(MaterialShaderMap)
	{
		// Use the local vertex factory shaders.
		const FMeshMaterialShaderMap* MeshShaderMap = MaterialShaderMap->GetMeshShaderMap(&FLocalVertexFactory::StaticType);
		check(MeshShaderMap);

		// Get intruction counts.
		NumInstructionsBasePassNoLightmap		= GetNumInstructionsForShaderType( MeshShaderMap, ShaderTypeBasePassNoLightmap	);
		NumInstructionsBasePassAndLightmap		= GetNumInstructionsForShaderType( MeshShaderMap, ShaderTypeBasePassAndLightmap );
		NumInstructionsPointLightWithShadowMap	= GetNumInstructionsForShaderType( MeshShaderMap, ShaderTypePointLightWithShadowMap );
	}
#endif
	// Iterate over unique texture refs and update resource size.
	for( INT TextureIndex=0; TextureIndex<UniqueTextures.Num(); TextureIndex++ )
	{
		UTexture* Texture = UniqueTextures(TextureIndex);
		if (Texture != NULL)
		{
			ResourceSizeOfReferencedTextures += Texture->GetResourceSize();
			TexturesUsed.AddItem( Texture->GetFullName() );
		}
		else
		{
			warnf(TEXT("Material %s has a NULL texture reference..."), *(Material->GetFullName()));
		}
	}

	// if test UniqueSpecularTexture is on, and if lit, then examine specular textures
	if (TestUniqueSpecularTexture && !IsTranslucentBlendMode((EBlendMode)Material->BlendMode) && (Material->LightingModel == MLM_Phong || Material->LightingModel == MLM_NonDirectional))
	{
		TArray<UTexture*> DiffuseTexs, SpecularTexs;
			
		// create material resource only for diffuse and get textures
		FTextureMaterialResource MatDiff(Material, TRUE);
		MatDiff.GetTextures(DiffuseTexs, TRUE);

		// create material resource only for specular and get textures
		FTextureMaterialResource MatSpec(Material, FALSE);
		MatSpec.GetTextures(SpecularTexs, TRUE);

		// if specular texture exists
		if (SpecularTexs.Num() > 0)
		{
			UseUniqueSpecularTexture = FALSE;

			for (INT I=0; I<SpecularTexs.Num(); ++I)
			{
				// if this specular texture I isn't found in diffuse, 
				// then this has unique specular textures
				if ( !DiffuseTexs.ContainsItem(SpecularTexs(I)) )
				{
					UseUniqueSpecularTexture = TRUE;
					break;
				}
			}
		}
	}
}

void FAnalyzeReferencedContentStat::FMaterialStats::SetupShaders()
{
#if 0 
	ShaderTypeBasePassNoLightmap		= FindShaderTypeByName(TEXT("TBasePassPixelShaderFNoLightMapPolicyNoSkyLight"));
	ShaderTypeBasePassAndLightmap		= FindShaderTypeByName(TEXT("TBasePassPixelShaderFDirectionalVertexLightMapPolicyNoSkyLight"));
	ShaderTypePointLightWithShadowMap	= FindShaderTypeByName(TEXT("TLightPixelShaderFPointLightPolicyFShadowTexturePolicy"));

	check( ShaderTypeBasePassNoLightmap	);
	check( ShaderTypeBasePassAndLightmap );
	check( ShaderTypePointLightWithShadowMap );
#endif
}
/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FMaterialStats::ToCSV() const
{
	FString Result = FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i"),
		*ResourceType,
		*ResourceName,
		NumStaticMeshInstancesAppliedTo,
		StaticMeshesAppliedTo.Num(),
		NumSkeletalMeshInstancesAppliedTo,
		SkeletalMeshesAppliedTo.Num(),
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		TexturesUsed.Num(),
		NumTextureSamples,
		ResourceSizeOfReferencedTextures,
		UseUniqueSpecularTexture, 
		(BlendMode == BLEND_Opaque)?1:0, 
		(BlendMode == BLEND_Masked || BlendMode == BLEND_SoftMasked || BlendMode == BLEND_DitheredTranslucent)?1:0, 
		(StaticMeshesAppliedTo.Num()>0 && SkeletalMeshesAppliedTo.Num()<=0)?1:0,
		(StaticMeshesAppliedTo.Num()<=0 && SkeletalMeshesAppliedTo.Num()>0)?1:0);

	for (INT I=0;I<ShaderUniqueDescriptions.Num(); ++I)
	{
		INT Idx=0;
		if ( ShaderDescriptions.FindItem(ShaderUniqueDescriptions(I), Idx) )
		{
			Result += FString::Printf(TEXT(",%i"), InstructionCounts(Idx));
		}
		else
		{
			Result += TEXT(",0");
		}
	}

	return FString::Printf(TEXT("%s%s"), *Result, LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FMaterialStats::GetCSVHeaderRow()
{
	FString HeaderRow = TEXT("ResourceType,ResourceName,NumStaticMeshInstancesAppliedTo,NumStaticMeshesAppliedTo,NumSkeletalMeshInstancesAppliedTo,NumSkeletalMeshesAppliedTo,ScriptReferenced,NumMapsUsedIn,NumTextures,NumTextureSamples,ResourceSizeOfReferencedTextures,UniqueSpecularMap,Opaque,Masked,OnlyStaticMesh,OnlySkeletalMesh");

	for (INT I=0;I<ShaderUniqueDescriptions.Num(); ++I)
	{
		HeaderRow += FString::Printf(TEXT(",Instruction#:%s"), *ShaderUniqueDescriptions(I));
	}

	HeaderRow += LINE_TERMINATOR;

	return HeaderRow;
}

FString FAnalyzeReferencedContentStat::FMaterialStats::GetSummaryCSVHeaderRow()
{
	FString HeaderRow = TEXT("Level,TotalCount,NumStaticMeshInstancesAppliedTo(Total),NumStaticMeshesAppliedTo(Total),NumSkeletalMeshInstancesAppliedTo(Total),NumSkeletalMeshesAppliedTo(Total),ScriptReferenced(%),NumTextures(Avg),NumTextureSamples(Avg),ResourceSizeOfReferencedTextures(Avg),UseUniqueSpecularMap(%),UniqueSpecularMapWhenOpaque(%),UniqueSpecularMapWhenMasked(%)");

	for (INT I=0;I<ShaderUniqueDescriptions.Num(); ++I)
	{
		HeaderRow += FString::Printf(TEXT(",Instruction#:%s(Avg)"), *ShaderUniqueDescriptions(I));
	}

	HeaderRow += LINE_TERMINATOR;

	return HeaderRow;
}

FString FAnalyzeReferencedContentStat::FMaterialStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FMaterialStats>& StatsData)
{
#if 0 
	FLOAT TotalNumBrushesAppliedTo=0;
#endif
	FLOAT TotalNumStaticMeshInstancesAppliedTo=0;
	FLOAT TotalNumStaticMeshesAppliedTo=0;
	FLOAT TotalNumSkeletalMeshInstancesAppliedTo=0;
	FLOAT TotalNumSkeletalMeshesAppliedTo=0;
	FLOAT TotalIsReferencedByScript=0;
	FLOAT TotalTexturesUsed=0;
	FLOAT TotalNumTextureSamples=0;
#if 0 
	FLOAT TotalMaxTextureDependencyLength=0;
	FLOAT TotalNumInstructionsBasePassNoLightmap=0;
	FLOAT TotalNumInstructionsBasePassAndLightmap=0;
	FLOAT TotalNumInstructionsPointLightWithShadowMap=0;
#endif
	FLOAT TotalResourceSizeOfReferencedTextures=0;
	FLOAT TotalUseUniqueSpecularTexture=0;
	FLOAT TotalUseUniqueSpecularTextureWithOpaque=0;
	FLOAT TotalUseUniqueSpecularTextureWithMasked=0;
	TArray<FLOAT>	TotalInstructionCounts;
	TArray<INT >	TotalCountsPerShader;

	INT TotalCount=0;

	// initialize
	TotalInstructionCounts.Add(ShaderUniqueDescriptions.Num());
	TotalCountsPerShader.Add(ShaderUniqueDescriptions.Num());
	for (INT I=0; I<TotalInstructionCounts.Num(); ++I)
	{
		TotalInstructionCounts(I) = 0;
		TotalCountsPerShader(I) = 0;
	}

	for( TMap<FString,FMaterialStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FMaterialStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
#if 0 
			TotalNumBrushesAppliedTo+=StatsEntry.NumBrushesAppliedTo;
#endif
			TotalNumStaticMeshInstancesAppliedTo+=StatsEntry.NumStaticMeshInstancesAppliedTo;
			TotalNumStaticMeshesAppliedTo+=StatsEntry.StaticMeshesAppliedTo.Num();
			TotalNumSkeletalMeshInstancesAppliedTo+=StatsEntry.NumSkeletalMeshInstancesAppliedTo;
			TotalNumSkeletalMeshesAppliedTo+=StatsEntry.SkeletalMeshesAppliedTo.Num();
			TotalIsReferencedByScript+=(StatsEntry.bIsReferencedByScript==TRUE);
			TotalTexturesUsed+=StatsEntry.TexturesUsed.Num();
			TotalNumTextureSamples+=StatsEntry.NumTextureSamples;
#if 0 
			TotalMaxTextureDependencyLength+=StatsEntry.MaxTextureDependencyLength;
			TotalNumInstructionsBasePassNoLightmap+=(StatsEntry.NumInstructionsBasePassNoLightmap>0);
			TotalNumInstructionsBasePassAndLightmap+=(StatsEntry.NumInstructionsBasePassAndLightmap>0);
			TotalNumInstructionsPointLightWithShadowMap+=(StatsEntry.NumInstructionsPointLightWithShadowMap>0);
#endif

			for (INT I=0;I<ShaderUniqueDescriptions.Num(); ++I)
			{
				INT Idx=0;
				if ( StatsEntry.ShaderDescriptions.FindItem(ShaderUniqueDescriptions(I), Idx) )
				{
					TotalInstructionCounts(I) += StatsEntry.InstructionCounts(Idx);
					if (StatsEntry.InstructionCounts(Idx) > 0)
					{
						++TotalCountsPerShader(I);
					}
				}
				else
				{
					TotalInstructionCounts(I) += 0;
				}
			}

			TotalResourceSizeOfReferencedTextures+=StatsEntry.ResourceSizeOfReferencedTextures;
			TotalUseUniqueSpecularTexture+=(StatsEntry.UseUniqueSpecularTexture);
			TotalUseUniqueSpecularTextureWithOpaque+=(StatsEntry.BlendMode==BLEND_Opaque && StatsEntry.UseUniqueSpecularTexture);
			TotalUseUniqueSpecularTextureWithMasked+=((StatsEntry.BlendMode==BLEND_Masked || StatsEntry.BlendMode==BLEND_SoftMasked || StatsEntry.BlendMode == BLEND_DitheredTranslucent) && StatsEntry.UseUniqueSpecularTexture);

			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		TotalIsReferencedByScript/=TotalCount;
		TotalTexturesUsed/=TotalCount;
		TotalNumTextureSamples/=TotalCount;
#if 0 
		TotalMaxTextureDependencyLength/=TotalCount;
		TotalNumInstructionsBasePassNoLightmap/=TotalCount;
		TotalNumInstructionsBasePassAndLightmap/=TotalCount;
		TotalNumInstructionsPointLightWithShadowMap/=TotalCount;
#endif
		TotalResourceSizeOfReferencedTextures/=TotalCount;
		TotalUseUniqueSpecularTexture/=TotalCount;
		TotalUseUniqueSpecularTextureWithOpaque/=TotalCount;
		TotalUseUniqueSpecularTextureWithMasked/=TotalCount;

		for (INT I=0; I<TotalInstructionCounts.Num(); ++I)
		{
			if ( TotalCountsPerShader(I)!=0 )
			{
				TotalInstructionCounts(I) /= TotalCountsPerShader(I);
			}
		}
	}

	FString Result = FString::Printf(TEXT("%s,%i,%i,%i,%i,%i,%i,%0.2f,%0.2f,%0.2f,%i,%i,%i"), 
		*LevelName, (INT)TotalCount, (INT)TotalNumSkeletalMeshInstancesAppliedTo, (INT)TotalNumStaticMeshesAppliedTo, 
		(INT)TotalNumSkeletalMeshInstancesAppliedTo, (INT)TotalNumSkeletalMeshesAppliedTo, (INT)(TotalIsReferencedByScript*100), TotalTexturesUsed, 
		TotalNumTextureSamples, TotalResourceSizeOfReferencedTextures, (INT)(TotalUseUniqueSpecularTexture*100), 
		(INT)(TotalUseUniqueSpecularTextureWithOpaque*100), (INT)(TotalUseUniqueSpecularTextureWithMasked*100));

	for (INT I=0; I<TotalInstructionCounts.Num(); ++I)
	{
		Result += FString::Printf(TEXT(",%0.2f"), TotalInstructionCounts(I));
	}

	return FString::Printf(TEXT("%s%s"), *Result, LINE_TERMINATOR);
}

/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FParticleStats::FParticleStats( UParticleSystem* ParticleSystem )
:	ResourceType(ParticleSystem->GetClass()->GetName())
,	ResourceName(ParticleSystem->GetPathName())
,	bIsReferencedByScript(FALSE)
,	NumEmitters(0)
,	NumModules(0)
,	NumPeakActiveParticles(0)
,   NumEmittersUsingCollision(0)
,   NumEmittersUsingPhysics(0)
,   MaxNumDrawnPerFrame(0)
,   PeakActiveToMaxDrawnRatio(0.0f)
,   NumBytesUsed(0)
,	bUsesDistortionMaterial(FALSE)
,	bUsesSceneTextureMaterial(FALSE)
,   bMeshEmitterHasDoCollisions(FALSE)
,   bMeshEmitterHasCastShadows(FALSE)
,   WarmUpTime( 0.0f )
,	bHasPhysXEmitters(FALSE)
{

	if( ParticleSystem->WarmupTime > 0.0f )
	{
		WarmUpTime = ParticleSystem->WarmupTime;
	}

	// Iterate over all sub- emitters and update stats.
	for( INT EmitterIndex=0; EmitterIndex<ParticleSystem->Emitters.Num(); EmitterIndex++ )
	{
		UParticleEmitter* ParticleEmitter = ParticleSystem->Emitters(EmitterIndex);
		if( ParticleEmitter )
		{
			if (ParticleEmitter->LODLevels.Num() > 0)
			{
				NumEmitters++;
				UParticleLODLevel* HighLODLevel = ParticleEmitter->LODLevels(0);
				check(HighLODLevel);
				NumModules += HighLODLevel->Modules.Num();

				for (INT LODIndex = 0; LODIndex < ParticleEmitter->LODLevels.Num(); LODIndex++)
				{
					UParticleLODLevel* LODLevel = ParticleEmitter->LODLevels(LODIndex);
					if (LODLevel)
					{
						if (LODLevel->bEnabled)
						{
							// Get peak active particles from LOD 0.
							if (LODIndex == 0)
							{
								INT PeakParticles = 0;
								if (ParticleEmitter->InitialAllocationCount > 0)
								{
									//If this value is non-zero it was overridden by user in the editor
									PeakParticles = ParticleEmitter->InitialAllocationCount;
								}
								else
								{
									//Peak number of particles simulated
									PeakParticles = LODLevel->PeakActiveParticles;
								}

								NumPeakActiveParticles += PeakParticles;                     

								if (LODLevel->RequiredModule && LODLevel->RequiredModule->bUseMaxDrawCount)
								{
									//Maximum number of particles allowed to draw per frame by this emitter
									MaxNumDrawnPerFrame += LODLevel->RequiredModule->MaxDrawCount;
								}
								else
								{
									//Make the "max drawn" equal to the number of particles simulated
									MaxNumDrawnPerFrame += PeakParticles;
								}
							}

							// flag distortion and scene color usage of materials
							if( LODLevel->RequiredModule && 
								LODLevel->RequiredModule->Material &&
								LODLevel->RequiredModule->Material->GetMaterial() )
							{
								if( LODLevel->RequiredModule->Material->GetMaterial()->HasDistortion() )
								{
									bUsesDistortionMaterial = TRUE;
									DistortMaterialNames.AddUniqueItem(LODLevel->RequiredModule->Material->GetPathName());
								}
								if( LODLevel->RequiredModule->Material->GetMaterial()->UsesSceneColor() )
								{
									bUsesSceneTextureMaterial = TRUE;
									SceneColorMaterialNames.AddUniqueItem(LODLevel->RequiredModule->Material->GetPathName());
								}
							}

							if( LODLevel->TypeDataModule)
							{
								UParticleModuleTypeDataMesh* MeshType = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
								UParticleModuleTypeDataPhysX* PhysXType = Cast<UParticleModuleTypeDataPhysX>(LODLevel->TypeDataModule);
								UParticleModuleTypeDataMeshPhysX* PhysXMeshType = Cast<UParticleModuleTypeDataMeshPhysX>(LODLevel->TypeDataModule);
								if (PhysXType || PhysXMeshType)
								{
									bHasPhysXEmitters = TRUE;
								}

								if (MeshType)
								{
									if( MeshType->DoCollisions == TRUE )
									{
										bMeshEmitterHasDoCollisions = TRUE;
									}

									if( MeshType->CastShadows == TRUE )
									{
										bMeshEmitterHasCastShadows = TRUE;
									}

									if( MeshType->Mesh )
									{
										for( INT MeshLODIdx=0; MeshLODIdx < MeshType->Mesh->LODInfo.Num(); MeshLODIdx++ )
										{
											const FStaticMeshLODInfo& MeshLOD = MeshType->Mesh->LODInfo(MeshLODIdx);
											for( INT ElementIdx=0; ElementIdx < MeshLOD.Elements.Num(); ElementIdx++ )
											{
												// flag distortion and scene color usage of materials
												const FStaticMeshLODElement& MeshElement = MeshLOD.Elements(ElementIdx);
												if( MeshElement.Material &&
													MeshElement.Material->GetMaterial() )
												{
													if( MeshElement.Material->GetMaterial()->HasDistortion() )
													{
														bUsesDistortionMaterial = TRUE;
														DistortMaterialNames.AddUniqueItem(MeshElement.Material->GetPathName());
													}
													if( MeshElement.Material->GetMaterial()->UsesSceneColor() )
													{
														bUsesSceneTextureMaterial = TRUE;
														SceneColorMaterialNames.AddUniqueItem(MeshElement.Material->GetPathName());
													}
												}
											}
										}
									}
								}
							}

							for (INT ModuleIndex = 0; ModuleIndex < LODLevel->Modules.Num(); ModuleIndex++)
							{
								UParticleModuleCollision* CollisionModule = Cast<UParticleModuleCollision>(LODLevel->Modules(ModuleIndex));                                      
								if (CollisionModule && CollisionModule->bEnabled)
								{
									NumEmittersUsingCollision++;
									if (CollisionModule->bApplyPhysics == TRUE)
									{
										NumEmittersUsingPhysics++;
									}
								}

								UParticleModuleMeshMaterial* MeshModule = Cast<UParticleModuleMeshMaterial>(LODLevel->Modules(ModuleIndex));
								if( MeshModule )
								{
									for( INT MatIdx=0; MatIdx < MeshModule->MeshMaterials.Num(); MatIdx++ )
									{
										// flag distortion and scene color usage of materials
										UMaterialInterface* Mat = MeshModule->MeshMaterials(MatIdx);									
										if( Mat && Mat->GetMaterial() )
										{
											if( Mat->GetMaterial()->HasDistortion() )
											{
												bUsesDistortionMaterial = TRUE;
												DistortMaterialNames.AddUniqueItem(Mat->GetPathName());
											}
											if( Mat->GetMaterial()->UsesSceneColor() )
											{
												bUsesSceneTextureMaterial = TRUE;
												SceneColorMaterialNames.AddUniqueItem(Mat->GetPathName());
											}
										}
									}
								}
								UParticleModuleMaterialByParameter* MaterialByModule = Cast<UParticleModuleMaterialByParameter>(LODLevel->Modules(ModuleIndex));
								if( MaterialByModule )
								{
									for( INT MatIdx=0; MatIdx < MaterialByModule->DefaultMaterials.Num(); MatIdx++ )
									{
										// flag distortion and scene color usage of materials
										UMaterialInterface* Mat = MaterialByModule->DefaultMaterials(MatIdx);
										if( Mat && Mat->GetMaterial() )
										{
											if( Mat->GetMaterial()->HasDistortion() )
											{
												bUsesDistortionMaterial = TRUE;
												DistortMaterialNames.AddUniqueItem(Mat->GetPathName());
											}
											if( Mat->GetMaterial()->UsesSceneColor() )
											{
												bUsesSceneTextureMaterial = TRUE; 
												SceneColorMaterialNames.AddUniqueItem(Mat->GetPathName());
											}
										}
									}
								}
							}
						}
						else
						{
							if (LODLevel->TypeDataModule)
							{
								UParticleModuleTypeDataPhysX* PhysXType = Cast<UParticleModuleTypeDataPhysX>(LODLevel->TypeDataModule);
								UParticleModuleTypeDataMeshPhysX* PhysXMeshType = Cast<UParticleModuleTypeDataMeshPhysX>(LODLevel->TypeDataModule);
								if (PhysXType || PhysXMeshType)
								{
									bHasPhysXEmitters = TRUE;
								}
							}
						}
					}
				} //for each lod
			}
		}
	} //for each emitter

	// @todo add this into the .xls
	//ParticleSystem->CalculateMaxActiveParticleCounts();

	//A number greater than 1 here indicates more particles simulated than drawn each frame
	if (MaxNumDrawnPerFrame > 0)
	{
		PeakActiveToMaxDrawnRatio = (FLOAT)NumPeakActiveParticles / (FLOAT)MaxNumDrawnPerFrame;
	}


	// determine the number of bytes this ParticleSystem uses
	FArchiveCountMem Count( ParticleSystem );
	NumBytesUsed = Count.GetNum();

	// Determine the number of bytes a PSysComp would use w/ this PSys as the template...
	if (ParticleSystem->IsTemplate() == FALSE)
	{
		UParticleSystemComponent* PSysComp = ConstructObject<UParticleSystemComponent>(UParticleSystemComponent::StaticClass());
		if (PSysComp)
		{
			PSysComp->SetTemplate(ParticleSystem);
			PSysComp->ActivateSystem(TRUE);

			FArchiveCountMem CountPSysComp(PSysComp);
			NumBytesUsed += CountPSysComp.GetNum();

			PSysComp->DeactivateSystem();
		}
	}
}

/** @return TRUE if this asset type should be logged */
UBOOL FAnalyzeReferencedContentStat::FParticleStats::ShouldLogStat() const
{
	return TRUE;
	//return bUsesDistortionMaterial || bUsesSceneTextureMaterial;
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FParticleStats::ToCSV() const
{
	FString MatNames;
	for( INT MatIdx=0; MatIdx < SceneColorMaterialNames.Num(); MatIdx++ )
	{
		MatNames += FString(TEXT("(scenecolor)")) + SceneColorMaterialNames(MatIdx); 
		MatNames += FString(TEXT(","));
	}
	for( INT MatIdx=0; MatIdx < DistortMaterialNames.Num(); MatIdx++ )
	{
		MatNames += FString(TEXT("(distort)")) + DistortMaterialNames(MatIdx);
		MatNames += FString(TEXT(","));
	}

	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%i,%i,%i,%1.2f,%i,%i,%i,%s,%s,%s,%s,%1.2f,%s,%s,%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		NumEmitters,
		NumModules,
		NumPeakActiveParticles,
		MaxNumDrawnPerFrame,
		PeakActiveToMaxDrawnRatio,
		NumEmittersUsingCollision,
		NumEmittersUsingPhysics,
		NumBytesUsed,
		bUsesDistortionMaterial ? TEXT("TRUE") : TEXT("FALSE"),
		bUsesSceneTextureMaterial ? TEXT("TRUE") : TEXT("FALSE"),
		bMeshEmitterHasDoCollisions ? TEXT("TRUE") : TEXT("FALSE"),
		bMeshEmitterHasCastShadows ? TEXT("TRUE") : TEXT("FALSE"),
		WarmUpTime,
		bHasPhysXEmitters ? TEXT("TRUE") : TEXT("FALSE"),
		*MatNames,
		LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FParticleStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,ScriptReferenced,NumMapsUsedIn,NumEmitters,NumModules,NumPeakActiveParticles,MaxParticlesDrawnPerFrame,PeakToMaxDrawnRatio,NumEmittersUsingCollision,NumEmittersUsingPhysics,NumBytesUsed,bUsesDistortion,bUsesSceneColor,bMeshEmitterHasDoCollisions,bMeshEmitterHasCastShadows,WarmUpTime,bHasPhysXEmitters") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FParticleStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,ScriptReferenced(%),NumEmitters(Avg),NumModules(Avg),NumPeakActiveParticles(Avg),MaxParticlesDrawnPerFrame(Avg),PeakToMaxDrawnRatio(Avg),UseNumEmittersUsingCollision(%),UseNumEmittersUsingPhysics(%),ResourceSize(Avg),\
				UsesDistortion(%),UsesSceneColor(%),UseMeshEmitterHasDoCollisions(%),UseMeshEmitterHasCastShadows(%),UseWarmUpTime(%),UseHasPhysXEmitters(%)") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FParticleStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FParticleStats>& StatsData)
{
	FLOAT TotalIsReferencedByScript=0,
		TotalNumEmitters=0,
		TotalNumModules=0,
		TotalNumPeakActiveParticles=0,
		TotalMaxNumDrawnPerFrame=0,
		TotalPeakActiveToMaxDrawnRatio=0,
		TotalNumEmittersUsingCollision=0,
		TotalNumEmittersUsingPhysics=0,
		TotalNumBytesUsed=0,
		TotalUsesDistortionMaterial=0,
		TotalUsesSceneTextureMaterial=0,
		TotalUseMeshEmitterHasDoCollisions=0,
		TotalUseMeshEmitterHasCastShadows=0,
		TotalUseWarmUpTime=0,
		TotalUseHasPhysXEmitters=0;

	INT TotalCount=0;

	for( TMap<FString,FParticleStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FParticleStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
			TotalIsReferencedByScript+=(StatsEntry.bIsReferencedByScript);
			TotalNumEmitters+=StatsEntry.NumEmitters;
			TotalNumModules+=StatsEntry.NumModules;
			TotalNumPeakActiveParticles+=StatsEntry.NumPeakActiveParticles;
			TotalMaxNumDrawnPerFrame+=StatsEntry.MaxNumDrawnPerFrame;
			TotalPeakActiveToMaxDrawnRatio+=StatsEntry.PeakActiveToMaxDrawnRatio;
			TotalNumEmittersUsingCollision+=StatsEntry.NumEmittersUsingCollision;
			TotalNumEmittersUsingPhysics+=StatsEntry.NumEmittersUsingPhysics;
			TotalNumBytesUsed+=StatsEntry.NumBytesUsed;
			TotalUsesDistortionMaterial+=(StatsEntry.bUsesDistortionMaterial);
			TotalUsesSceneTextureMaterial+=(StatsEntry.bUsesSceneTextureMaterial);
			TotalUseMeshEmitterHasDoCollisions+=(StatsEntry.bMeshEmitterHasDoCollisions);
			TotalUseMeshEmitterHasCastShadows+=(StatsEntry.bMeshEmitterHasCastShadows);
			TotalUseWarmUpTime+=(StatsEntry.WarmUpTime>0);
			TotalUseHasPhysXEmitters+=StatsEntry.bHasPhysXEmitters;
			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		TotalIsReferencedByScript/=TotalCount;
		TotalNumEmitters/=TotalCount;
		TotalNumModules/=TotalCount;
		TotalNumPeakActiveParticles/=TotalCount;
		TotalMaxNumDrawnPerFrame/=TotalCount;
		TotalPeakActiveToMaxDrawnRatio/=TotalCount;
		TotalNumEmittersUsingCollision/=TotalCount;
		TotalNumEmittersUsingPhysics/=TotalCount;
		TotalNumBytesUsed/=TotalCount;
		TotalUsesDistortionMaterial/=TotalCount;
		TotalUsesSceneTextureMaterial/=TotalCount;
		TotalUseMeshEmitterHasDoCollisions/=TotalCount;
		TotalUseMeshEmitterHasCastShadows/=TotalCount;
		TotalUseWarmUpTime/=TotalCount;
		TotalUseHasPhysXEmitters/=TotalCount;
	}

	return FString::Printf(TEXT("%s,%i,%i,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f,%i,%i,%0.2f,%i,%i,%i,%i,%i,%i%s"), 
		*LevelName, (INT)TotalCount, 
		(INT)(TotalIsReferencedByScript*100),
		TotalNumEmitters,
		TotalNumModules,
		TotalNumPeakActiveParticles,
		TotalMaxNumDrawnPerFrame,
		TotalPeakActiveToMaxDrawnRatio,
		(INT)(TotalNumEmittersUsingCollision*100),
		(INT)(TotalNumEmittersUsingPhysics*100),
		TotalNumBytesUsed,
		(INT)(TotalUsesDistortionMaterial*100),
		(INT)(TotalUsesSceneTextureMaterial*100),
		(INT)(TotalUseMeshEmitterHasDoCollisions*100),
		(INT)(TotalUseMeshEmitterHasCastShadows*100),
		(INT)(TotalUseWarmUpTime*100),
		(INT)(TotalUseHasPhysXEmitters*100), LINE_TERMINATOR);
}

//
//	FTextureToParticleSystemStats
//
FAnalyzeReferencedContentStat::FTextureToParticleSystemStats::FTextureToParticleSystemStats(UTexture* InTexture) :
TextureName(InTexture->GetPathName())
{
	UTexture2D* Texture2D = Cast<UTexture2D>(InTexture);
	if (Texture2D)
	{
		TextureSize = FString::Printf(TEXT("%d x %d"), (INT)(Texture2D->GetSurfaceWidth()), (INT)(Texture2D->GetSurfaceHeight()));
		Format = GPixelFormats[Texture2D->Format].Name;
	}
	else
	{
		TextureSize = FString::Printf(TEXT("???"));
		Format = TextureSize;
	}
	appMemzero(&ParticleSystemsContainedIn, sizeof(TArray<UParticleSystem*>));
}

void FAnalyzeReferencedContentStat::FTextureToParticleSystemStats::AddParticleSystem(UParticleSystem* InParticleSystem)
{
	ParticleSystemsContainedIn.AddUniqueItem(InParticleSystem->GetPathName());
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FTextureToParticleSystemStats::ToCSV() const
{
	FString Row = FString::Printf(TEXT("%s,%s,%s,%i,%s"),
		*TextureName,
		*TextureSize, 
		*Format,
		ParticleSystemsContainedIn.Num(),
		LINE_TERMINATOR);

	// this will print out the specific particles systems that use this texture
	for (INT PSysIndex = 0; PSysIndex < GetParticleSystemsContainedInCount(); PSysIndex++)
	{
		const FString OutputText = FString::Printf(TEXT(",,,,%s,%s"), *(GetParticleSystemContainedIn(PSysIndex)), LINE_TERMINATOR);
		Row += OutputText;
	}

	return Row;
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FTextureToParticleSystemStats::GetCSVHeaderRow()
{
	return TEXT("ResourceName,Size,Format,NumParticleSystemsUsedIn") LINE_TERMINATOR;
}

extern FString GetAnimationTag( UAnimSequence * Sequence );

/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FAnimSequenceStats::FAnimSequenceStats( UAnimSequence* Sequence )
:	ResourceType(Sequence->GetClass()->GetName())
,	ResourceName(Sequence->GetPathName())
,	bIsReferencedByScript(FALSE)
,	CompressionScheme(FString(TEXT("")))
,	TranslationFormat(ACF_None)
,	RotationFormat(ACF_None)
,	AnimationResourceSize(0)
,	TotalTracks(0)
,	NumTransTracksWithOneKey(0)
,	NumRotTracksWithOneKey(0)
,	TrackTableSize(0)
,	TotalNumTransKeys(0)
,	TotalNumRotKeys(0)
,	TranslationKeySize(0.0f)
,	RotationKeySize(0.0f)
,	OverheadSize(0)
,	TotalFrames(0)
{
	// The sequence object name is not very useful - strip and add the friendly name.
	FString Left, Right;
	ResourceName.Split(TEXT("."), &Left, &Right, TRUE);
	ResourceName = Left + TEXT(".") + Sequence->SequenceName.ToString();

	UAnimSet * AnimSet = Sequence->GetAnimSet();
	if ( AnimSet )
	{
		AnimSetName = AnimSet->GetName();
	}
	else
	{
		AnimSetName = TEXT("NONE");
	}

	AnimTag = GetAnimationTag( Sequence );
	if(Sequence->CompressionScheme)
	{
		CompressionScheme = Sequence->CompressionScheme->GetClass()->GetName();
	}

	TranslationFormat = static_cast<AnimationCompressionFormat>(Sequence->TranslationCompressionFormat);
	RotationFormat = static_cast<AnimationCompressionFormat>(Sequence->RotationCompressionFormat);
	AnimationResourceSize = Sequence->GetResourceSize();
	INT ApproxRawSize = Sequence->GetApproxRawSize();
	if (ApproxRawSize > 0)
	{
		CompressionRatio = 100*Sequence->GetApproxCompressedSize()/ApproxRawSize;
	}

	INT NumRotTracks = 0;

	AnimationFormat_GetStats(	
		Sequence, 
		TotalTracks,
		NumRotTracks,
		TotalNumTransKeys,
		TotalNumRotKeys,
		TranslationKeySize,
		RotationKeySize,
		OverheadSize,
		NumTransTracksWithOneKey,
		NumRotTracksWithOneKey);

	bMarkedAsDoNotOverrideCompression = Sequence->bDoNotOverrideCompression;
	TrackTableSize = sizeof(INT)*Sequence->CompressedTrackOffsets.Num();
	TotalFrames = Sequence->NumFrames;
}

static FString GetReferenceTypeString(FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType ReferenceType)
{
	switch(ReferenceType)
	{
	case FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_SkeletalMeshComponent:
		return FString(TEXT("SkeletalMeshComponent"));
	case FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_Matinee:
		return FString(TEXT("Matinee"));
	case FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_Crowd:
		return FString(TEXT("Crowd"));
	}

	return FString(TEXT("Unknown"));
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FAnimSequenceStats::ToCSV() const
{
	FString CSV = FString::Printf(TEXT("%s,%s,%s,%s,%i,%i,%s,%s,%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%1.1f,"),
		*ResourceType,
		*ResourceName,	
		*AnimSetName, 
		*AnimTag,
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		*GetReferenceTypeString(ReferenceType),
		*FAnimationUtils::GetAnimationCompressionFormatString(TranslationFormat),
		*FAnimationUtils::GetAnimationCompressionFormatString(RotationFormat),
		*CompressionScheme,
		AnimationResourceSize,
		CompressionRatio,
		TrackTableSize,
		TotalTracks,
		NumTransTracksWithOneKey,
		NumRotTracksWithOneKey,
		TotalNumTransKeys,
		TotalNumRotKeys,
		TranslationKeySize);

	return FString::Printf(TEXT("%s%1.1f,%i,%i,%i%s"), *CSV, RotationKeySize, OverheadSize, TotalFrames, bMarkedAsDoNotOverrideCompression, LINE_TERMINATOR);
}
/** This takes a LevelName and then looks for the number of Instances of this AnimStat used within that level **/
FString FAnalyzeReferencedContentStat::FAnimSequenceStats::ToCSV( const FString& LevelName ) const
{
	UINT* NumInst = const_cast<UINT*>(LevelNameToInstanceCount.Find( LevelName ));
	if( NumInst == NULL )
	{
		*NumInst = 0;
	}

	FString CSV = FString::Printf(TEXT("%s,%s,%s,%s,%i,%i,%s,%s,%s,%s,%i,%i,%i,%i,%i,%i,%i,%i,%1.1f,"),
		*ResourceType,
		*ResourceName,	
		*AnimSetName, 
		*AnimTag,
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		*GetReferenceTypeString(ReferenceType),
		*FAnimationUtils::GetAnimationCompressionFormatString(TranslationFormat),
		*FAnimationUtils::GetAnimationCompressionFormatString(RotationFormat),
		*CompressionScheme,
		AnimationResourceSize,
		CompressionRatio,
		TrackTableSize,
		TotalTracks,
		NumTransTracksWithOneKey,
		NumRotTracksWithOneKey,
		TotalNumTransKeys,
		TotalNumRotKeys,
		TranslationKeySize);

	return FString::Printf(TEXT("%s%1.1f,%i,%i,%i%s"), *CSV, RotationKeySize, OverheadSize, TotalFrames, bMarkedAsDoNotOverrideCompression, LINE_TERMINATOR);
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FAnimSequenceStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,ScriptReferenced(%),ReferenceType-Matinee(%),ReferenceType-SkeletalMesh(%),ReferenceType-Crowd(%),\
				UseTransFormat(%), \
				RotFormat(ACF_None)(%),RotFormat(ACF_Float96NoW)(%), RotFormat(ACF_Fixed48NoW)(%), RotFormat(ACF_IntervalFixed32NoW)(%), RotFormat(ACF_Fixed32NoW)(%), RotFormat(ACF_Float32NoW)(%), \
				CompressionScheme(RemoveLinearKeys)(%),CompressionScheme(RemoveEverySecondKeys)(%),CompressionScheme(RemoveTrivialKeys)(%),CompressionScheme(BitwiseCompression)(%),CompressionScheme(Unknown)(%),\
				AnimResourceSize(Avg),CompressionRatio(Avg),TrackTableSize(Avg),Tracks(Avg),TracksWithoutTrans(%),TracksWithoutRot(%),TransKeys(Avg),RotKeys(Avg),TransKeySize(Avg),RotKeySize(Avg),TotalFrames(Avg)") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FAnimSequenceStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FAnimSequenceStats>& StatsData)
{
	FLOAT TotalIsReferencedByScript=0,
		TotalReferenceTypeByMatinee=0, TotalReferenceTypeBySkeletal=0, TotalReferenceTypeByCrowd=0,
		TotalUseTransFormat=0,
		TotalRotFormat_None=0, TotalRotFormat_Float96NoW=0, TotalRotFormat_Fixed48NoW=0, TotalRotFormat_IntervalFixed32NoW=0, TotalRotFormat_Fixed32NoW=0, TotalRotFormat_Float32NoW=0,
		TotalCompressionScheme_None=0,TotalCompressionScheme_LinearKeys=0, TotalCompressionScheme_EverySecondKeys=0, TotalCompressionScheme_TrivialKeys=0, TotalCompressionScheme_BitwiseCompression=0,
		TotalAnimationSize=0, TotalCompressionRatio=0, AverageTotalTrackTableSize=0,AverageTotalTracks=0,TotalNumTransTracksWithOneKey=0,TotalNumRotTracksWithOneKey=0,
		AverageTotalNumTransKeys=0,AverageTotalNumRotKeys=0,AverageTranslationKeySize=0,
		TotalRotationKeySize=0,AverageTotalFrames=0;

	INT TotalCount=0;

	for( TMap<FString,FAnimSequenceStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FAnimSequenceStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
			TotalIsReferencedByScript+=StatsEntry.bIsReferencedByScript;
			TotalReferenceTypeByMatinee+=(StatsEntry.ReferenceType==FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_Matinee);
			TotalReferenceTypeBySkeletal+=(StatsEntry.ReferenceType==FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_SkeletalMeshComponent);
			TotalReferenceTypeByCrowd+=(StatsEntry.ReferenceType==FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_Crowd);
			TotalUseTransFormat+=(StatsEntry.TranslationFormat!=ACF_None);
			TotalRotFormat_None+=(StatsEntry.RotationFormat==ACF_None);
			TotalRotFormat_Float96NoW+=(StatsEntry.RotationFormat==ACF_Float96NoW);
			TotalRotFormat_Fixed48NoW+=(StatsEntry.RotationFormat==ACF_Fixed48NoW);
			TotalRotFormat_IntervalFixed32NoW+=(StatsEntry.RotationFormat==ACF_IntervalFixed32NoW);
			TotalRotFormat_Fixed32NoW+=(StatsEntry.RotationFormat==ACF_Fixed32NoW);
			TotalRotFormat_Float32NoW+=(StatsEntry.RotationFormat==ACF_Float32NoW);
			if (StatsEntry.CompressionScheme==TEXT("AnimationCompressionAlgorithm_RemoveLinearKeys"))
			{
				TotalCompressionScheme_LinearKeys+=1;
			}
			else if (StatsEntry.CompressionScheme==TEXT("AnimationCompressionAlgorithm_RemoveEverySecondKey"))
			{
				TotalCompressionScheme_EverySecondKeys+=1;
			}
			else if (StatsEntry.CompressionScheme==TEXT("AnimationCompressionAlgorithm_RemoveTrivialKeys"))
			{
				TotalCompressionScheme_TrivialKeys+=1;
			}
			else if (StatsEntry.CompressionScheme==TEXT("AnimationCompressionAlgorithm_BitwiseCompressOnly"))
			{
				TotalCompressionScheme_BitwiseCompression+=1;
			}
			else
			{
				TotalCompressionScheme_None+=1;
			}

			TotalAnimationSize+=StatsEntry.AnimationResourceSize; 
			TotalCompressionRatio+=StatsEntry.CompressionRatio;
			AverageTotalTrackTableSize+=StatsEntry.TrackTableSize;
			AverageTotalTracks+=StatsEntry.TotalTracks;
			TotalNumTransTracksWithOneKey+=StatsEntry.NumTransTracksWithOneKey;
			TotalNumRotTracksWithOneKey+=StatsEntry.NumRotTracksWithOneKey;
			AverageTotalNumTransKeys+=StatsEntry.TotalNumTransKeys;
			AverageTotalNumRotKeys+=StatsEntry.TotalNumRotKeys;
			AverageTranslationKeySize+=StatsEntry.TranslationKeySize;
			TotalRotationKeySize+=StatsEntry.RotationKeySize;
			AverageTotalFrames+=StatsEntry.TotalFrames;

			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		TotalIsReferencedByScript/=TotalCount;
		TotalReferenceTypeByMatinee/=TotalCount; 
		TotalReferenceTypeBySkeletal/=TotalCount; 
		TotalReferenceTypeByCrowd/=TotalCount;
		TotalUseTransFormat/=TotalCount;
		TotalRotFormat_None/=TotalCount; 
		TotalRotFormat_Float96NoW/=TotalCount; 
		TotalRotFormat_Fixed48NoW/=TotalCount; 
		TotalRotFormat_IntervalFixed32NoW/=TotalCount; 
		TotalRotFormat_Fixed32NoW/=TotalCount; 
		TotalRotFormat_Float32NoW/=TotalCount;
		TotalCompressionScheme_None/=TotalCount; 
		TotalCompressionScheme_LinearKeys/=TotalCount; 
		TotalCompressionScheme_EverySecondKeys/=TotalCount; 
		TotalCompressionScheme_TrivialKeys/=TotalCount; 
		TotalCompressionScheme_BitwiseCompression/=TotalCount;
		TotalAnimationSize/=TotalCount; 
		TotalCompressionRatio/=TotalCount;
		AverageTotalTrackTableSize/=TotalCount;
		TotalNumTransTracksWithOneKey/=AverageTotalTracks;
		TotalNumRotTracksWithOneKey/=AverageTotalTracks;
		AverageTotalTracks/=TotalCount;
		AverageTotalNumTransKeys/=TotalCount;
		AverageTotalNumRotKeys/=TotalCount;
		AverageTranslationKeySize/=TotalCount;
		TotalRotationKeySize/=TotalCount;
		AverageTotalFrames/=TotalCount;
	}

	FString Result = FString::Printf(TEXT("%s,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,%i,"), 
		*LevelName, (INT)TotalCount, (INT)(TotalIsReferencedByScript*100),
		(INT)(TotalReferenceTypeByMatinee*100), (INT)(TotalReferenceTypeBySkeletal*100), (INT)(TotalReferenceTypeByCrowd*100),
		(INT)(TotalUseTransFormat*100),
		(INT)(TotalRotFormat_None*100), (INT)(TotalRotFormat_Float96NoW*100), (INT)(TotalRotFormat_Fixed48NoW*100), (INT)(TotalRotFormat_IntervalFixed32NoW*100), (INT)(TotalRotFormat_Fixed32NoW*100), (INT)(TotalRotFormat_Float32NoW*100),
		(INT)(TotalCompressionScheme_LinearKeys*100), (INT)(TotalCompressionScheme_EverySecondKeys*100), (INT)(TotalCompressionScheme_TrivialKeys*100), (INT)(TotalCompressionScheme_BitwiseCompression*100),(INT)(TotalCompressionScheme_None*100));

	return FString::Printf(TEXT("%s%0.2f,%0.2f,%0.2f,%0.2f,%i,%i,%0.2f,%0.2f,%0.2f,%0.2f,%0.2f%s"), 
		*Result,
		TotalAnimationSize, TotalCompressionRatio,AverageTotalTrackTableSize,AverageTotalTracks,(INT)(TotalNumTransTracksWithOneKey*100),(INT)(TotalNumRotTracksWithOneKey*100),
		AverageTotalNumTransKeys,AverageTotalNumRotKeys,AverageTranslationKeySize,
		TotalRotationKeySize,AverageTotalFrames, LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FAnimSequenceStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,AnimSetName,AnimTag,ScriptReferenced,NumMapsUsedIn,ReferenceType,TransFormat,RotFormat,CompressionScheme,AnimSize,CompressionRatio,TrackTableSize,Tracks,TracksWithoutTrans,TracksWithoutRot,TransKeys,RotKeys,TransKeySize,RotKeySize,OverheadSize,TotalFrames,bNoRecompress") LINE_TERMINATOR;
}

/** Constructor, initializing all members */
FAnalyzeReferencedContentStat::FFaceFXAnimSetStats::FFaceFXAnimSetStats( UFaceFXAnimSet* FaceFXAnimSet)
:	ResourceType(FaceFXAnimSet->GetClass()->GetName())
,	ResourceName(FaceFXAnimSet->GetPathName())
,	bIsReferencedByScript(FALSE)
{
#if WITH_FACEFX
	ResourceSize = FaceFXAnimSet->GetResourceSize();
	OC3Ent::Face::FxAnimSet* AnimSet = FaceFXAnimSet->GetFxAnimSet();
	if ( AnimSet )
	{
		const OC3Ent::Face::FxAnimGroup & Group = AnimSet->GetAnimGroup();
		GroupName = ANSI_TO_TCHAR(Group.GetName().GetAsCstr());
		NumberOfAnimations = Group.GetNumAnims();
	}
#endif //#if WITH_FACEFX
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FFaceFXAnimSetStats::ToCSV() const
{
#if WITH_FACEFX
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%s,%i%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		*GroupName, 
		NumberOfAnimations, 	
		LINE_TERMINATOR);
#else
	return FString(TEXT(""));
#endif //#if WITH_FACEFX
}
/** This takes a LevelName and then looks for the number of Instances of this AnimStat used within that level **/
FString FAnalyzeReferencedContentStat::FFaceFXAnimSetStats::ToCSV( const FString& LevelName ) const
{
#if WITH_FACEFX
	UINT* NumInst = const_cast<UINT*>(LevelNameToInstanceCount.Find( LevelName ));
	if( NumInst == NULL )
	{
		*NumInst = 0;
	}

	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%s,%i%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		*GroupName, 
		NumberOfAnimations, 	
		LINE_TERMINATOR);
#else
	return FString(TEXT(""));
#endif // #if WITH_FACEFX
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FFaceFXAnimSetStats::GetCSVHeaderRow()
{
#if WITH_FACEFX
	return TEXT("ResourceType,ResourceName,ScriptReferenced,NumMapsUsedIn,ResourceSize,GroupName,NumberOfAnimations") LINE_TERMINATOR;
#else
	return FString(TEXT(""));
#endif // #if WITH_FACEFX
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FFaceFXAnimSetStats::GetSummaryCSVHeaderRow()
{
#if WITH_FACEFX
	return TEXT("Level,TotalCount,ScriptReferenced(%),ResourceSize(Avg),UseGroupName(%),NumberOfAnimations(Avg)") LINE_TERMINATOR;
#else
	return FString(TEXT(""));
#endif // #if WITH_FACEFX
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FFaceFXAnimSetStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FFaceFXAnimSetStats>& StatsData)
{
	FLOAT TotalIsReferencedByScript=0,
		TotalResourceSize=0,
		TotalUseGroupName=0, 
		TotalNumberOfAnimations=0; 	

	INT TotalCount=0;

	for( TMap<FString,FFaceFXAnimSetStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FFaceFXAnimSetStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
			TotalIsReferencedByScript+=StatsEntry.bIsReferencedByScript;
			TotalResourceSize+=StatsEntry.ResourceSize;
			TotalUseGroupName+=(StatsEntry.GroupName!=TEXT(""))?1:0;
			TotalNumberOfAnimations+=StatsEntry.NumberOfAnimations; 	

			++TotalCount;
		}
	}

	if ( TotalCount > 0 )
	{
		TotalIsReferencedByScript/=TotalCount;
		TotalResourceSize/=TotalCount;
		TotalUseGroupName/=TotalCount;
		TotalNumberOfAnimations/=TotalCount;
	}

	return FString::Printf(TEXT("%s,%i,%i,%0.2f,%i,%0.2f%s"), 
		*LevelName, (INT)TotalCount, (INT)(TotalIsReferencedByScript*100), TotalResourceSize, (INT)(TotalUseGroupName*100), TotalNumberOfAnimations, LINE_TERMINATOR);
}

/** Constructor, initializing all members. */
FAnalyzeReferencedContentStat::FSoundCueStats::FSoundCueStats( USoundCue* SoundCue )
:	ResourceType(SoundCue->GetClass()->GetName())
,	ResourceName(SoundCue->GetPathName())
,	bIsReferencedByScript(FALSE)
,	FaceFXAnimName(SoundCue->FaceFXAnimName)
,	FaceFXGroupName(SoundCue->FaceFXGroupName)
,	ResourceSize(SoundCue->GetResourceSize(Platform))
{
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FSoundCueStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%s,%s%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		*FaceFXAnimName,
		*FaceFXGroupName,
		LINE_TERMINATOR);
}

/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
FString FAnalyzeReferencedContentStat::FSoundCueStats::ToCSV( const FString& LevelName ) const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i,%s,%s%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		*FaceFXAnimName,
		*FaceFXGroupName,
		LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FSoundCueStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,ScriptReferenced,NumMapsUsedIn,ResourceSize,FaceFXAnimName,FaceFXGroupName") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FSoundCueStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,ScriptReferenced(%),ResourceSize(Avg),UsedByFaceFX(%)") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FSoundCueStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FSoundCueStats>& StatsData)
{
	FLOAT TotalIsReferencedByScript=0;
	FLOAT TotalResourceSize=0;
	FLOAT TotalUsedByFaceFX=0;

	INT TotalCount=0;

	for( TMap<FString,FSoundCueStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FSoundCueStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
			TotalIsReferencedByScript+=(StatsEntry.bIsReferencedByScript==TRUE)?1:0;
			TotalResourceSize+=StatsEntry.ResourceSize;
			TotalUsedByFaceFX+=(StatsEntry.FaceFXAnimName!=TEXT(""))?1:0;

			++TotalCount;
		}
	}

	if ( TotalCount )
	{
		TotalResourceSize/=TotalCount;
		TotalIsReferencedByScript/=TotalCount;
		TotalUsedByFaceFX/=TotalCount;
	}

	return FString::Printf(TEXT("%s,%i,%i,%0.2f,%i%s"), 
		*LevelName, (INT)TotalCount, (INT)(TotalIsReferencedByScript*100), TotalResourceSize, (INT)(TotalUsedByFaceFX*100), LINE_TERMINATOR);
}



/** Constructor, initializing all members. */
FAnalyzeReferencedContentStat::FSoundNodeWaveStats::FSoundNodeWaveStats( USoundNodeWave* SoundNodeWave )
:	ResourceType(SoundNodeWave->GetClass()->GetName())
,	ResourceName(SoundNodeWave->GetPathName())
,	bIsReferencedByScript(FALSE)
,	ResourceSize(SoundNodeWave->GetResourceSize(Platform))
{
}

/**
* Stringifies gathered stats in CSV format.
*
* @return comma separated list of stats
*/
FString FAnalyzeReferencedContentStat::FSoundNodeWaveStats::ToCSV() const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		LINE_TERMINATOR);
}

/** This takes a LevelName and then looks for the number of Instances of this StatMesh used within that level **/
FString FAnalyzeReferencedContentStat::FSoundNodeWaveStats::ToCSV( const FString& LevelName ) const
{
	return FString::Printf(TEXT("%s,%s,%i,%i,%i%s"),
		*ResourceType,
		*ResourceName,	
		bIsReferencedByScript,
		MapsUsedIn.Num(),
		ResourceSize,
		LINE_TERMINATOR);
}

/**
* Returns a header row for CSV
*
* @return comma separated header row
*/
FString FAnalyzeReferencedContentStat::FSoundNodeWaveStats::GetCSVHeaderRow()
{
	return TEXT("ResourceType,ResourceName,ScriptReferenced,NumMapsUsedIn,ResourceSize") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary CSV header row
*/
FString FAnalyzeReferencedContentStat::FSoundNodeWaveStats::GetSummaryCSVHeaderRow()
{
	return TEXT("Level,TotalCount,ScriptReferenced(%),ResourceSize(Avg)") LINE_TERMINATOR;
}

/**
*	This is for summary
*	@return command separated summary data row
*/
FString FAnalyzeReferencedContentStat::FSoundNodeWaveStats::ToSummaryCSV(const FString& LevelName, const TMap<FString,FSoundNodeWaveStats>& StatsData)
{
	FLOAT TotalIsReferencedByScript=0;
	FLOAT TotalResourceSize=0;

	INT TotalCount=0;

	for( TMap<FString,FSoundNodeWaveStats>::TConstIterator It(StatsData); It; ++ It )
	{
		const FSoundNodeWaveStats& StatsEntry = It.Value();

		if( LevelName==TEXT("Total") || StatsEntry.LevelNameToInstanceCount.Find( LevelName ) != NULL )
		{
			TotalIsReferencedByScript+=(StatsEntry.bIsReferencedByScript==TRUE)?1:0;
			TotalResourceSize+=StatsEntry.ResourceSize;

			++TotalCount;
		}
	}

	if ( TotalCount )
	{
		TotalResourceSize/=TotalCount;
		TotalIsReferencedByScript/=TotalCount;
	}

	return FString::Printf(TEXT("%s,%i,%i,%0.2f%s"), 
		*LevelName, (INT)TotalCount, (INT)(TotalIsReferencedByScript*100), TotalResourceSize, LINE_TERMINATOR);
}

/**
* Retrieves/ creates material stats associated with passed in material.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	Material	Material to retrieve/ create material stats for
* @return	pointer to material stats associated with material
*/
FAnalyzeReferencedContentStat::FMaterialStats* FAnalyzeReferencedContentStat::GetMaterialStats( UMaterial* Material )
{
	FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ResourceNameToMaterialStats.Find( Material->GetFullName() );
	if( MaterialStats == NULL )
	{
		MaterialStats =	&ResourceNameToMaterialStats.Set( *Material->GetFullName(), FAnalyzeReferencedContentStat::FMaterialStats( Material ) );
	}
	return MaterialStats;
}

/**
* Retrieves/ creates texture stats associated with passed in texture.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	Texture		Texture to retrieve/ create texture stats for
* @return	pointer to texture stats associated with texture
*/
FAnalyzeReferencedContentStat::FTextureStats* FAnalyzeReferencedContentStat::GetTextureStats( UTexture* Texture )
{
	FAnalyzeReferencedContentStat::FTextureStats* TextureStats = ResourceNameToTextureStats.Find( Texture->GetFullName() );
	if( TextureStats == NULL )
	{
		TextureStats = &ResourceNameToTextureStats.Set( *Texture->GetFullName(), FAnalyzeReferencedContentStat::FTextureStats( Texture ) );
	}
	return TextureStats;
}

/**
* Retrieves/ creates static mesh stats associated with passed in static mesh.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	StaticMesh	Static mesh to retrieve/ create static mesh stats for
* @return	pointer to static mesh stats associated with static mesh
*/
FAnalyzeReferencedContentStat::FStaticMeshStats* FAnalyzeReferencedContentStat::GetStaticMeshStats( UStaticMesh* StaticMesh, UPackage* LevelPackage )
{
	FAnalyzeReferencedContentStat::FStaticMeshStats* StaticMeshStats = ResourceNameToStaticMeshStats.Find( StaticMesh->GetFullName() );

	if( StaticMeshStats == NULL )
	{
		StaticMeshStats = &ResourceNameToStaticMeshStats.Set( *StaticMesh->GetFullName(), FAnalyzeReferencedContentStat::FStaticMeshStats( StaticMesh ) );
	}

	return StaticMeshStats;
}

/**
* Retrieves/ creates static mesh stats associated with passed in primitive stats.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	Object	Object to retrieve/ create primitive stats for
* @return	pointer to primitive stats associated with object (ModelComponent, DecalComponent, SpeedTreeComponent, TerrainComponent)
*/
FAnalyzeReferencedContentStat::FPrimitiveStats* FAnalyzeReferencedContentStat::GetPrimitiveStats( UObject* Object, UPackage* LevelPackage )
{
	UModelComponent*		ModelComponent			= Cast<UModelComponent>(Object);
	UTerrainComponent*		TerrainComponent		= Cast<UTerrainComponent>(Object);
	USpeedTreeComponent*	SpeedTreeComponent		= Cast<USpeedTreeComponent>(Object);
	UDecalComponent*		DecalComponent			= Cast<UDecalComponent>(Object);
	UStaticMeshComponent*	StaticMeshComponent		= Cast<UStaticMeshComponent>(Object);
	USkeletalMeshComponent* SkeletalMeshComponent	= Cast<USkeletalMeshComponent>(Object);

	FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats = NULL;
	
	if ( DecalComponent )
	{
		PrimitiveStats = ResourceNameToPrimitiveStats.Find( Object->GetFullName() );
	}
	else if ( ModelComponent && ModelComponent->GetModel() )
	{
		PrimitiveStats = ResourceNameToPrimitiveStats.Find( ModelComponent->GetModel()->GetFullName() );
	}
	else if ( TerrainComponent && TerrainComponent->GetTerrain() )
	{
		PrimitiveStats = ResourceNameToPrimitiveStats.Find( TerrainComponent->GetTerrain()->GetFullName() );
	}
	else if ( SpeedTreeComponent && SpeedTreeComponent->SpeedTree )
	{
		PrimitiveStats = ResourceNameToPrimitiveStats.Find( SpeedTreeComponent->SpeedTree->GetFullName()  );
	}
	else if ( StaticMeshComponent && StaticMeshComponent->StaticMesh )
	{
		PrimitiveStats = ResourceNameToPrimitiveStats.Find( StaticMeshComponent->StaticMesh->GetFullName() );
	}
	else if ( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
	{
		PrimitiveStats = ResourceNameToPrimitiveStats.Find( SkeletalMeshComponent->SkeletalMesh->GetFullName() );
	}

	if( PrimitiveStats == NULL )
	{
		if ( ModelComponent && ModelComponent->GetModel() )
		{
			PrimitiveStats = &ResourceNameToPrimitiveStats.Set( *ModelComponent->GetModel()->GetFullName(), FAnalyzeReferencedContentStat::FPrimitiveStats( ModelComponent->GetModel() ) );
		}
		else if ( TerrainComponent && TerrainComponent->GetTerrain() )
		{
			PrimitiveStats = &ResourceNameToPrimitiveStats.Set( *TerrainComponent->GetTerrain()->GetFullName(), FAnalyzeReferencedContentStat::FPrimitiveStats( TerrainComponent->GetTerrain()) );
		}
		else if ( DecalComponent )
		{
			PrimitiveStats = &ResourceNameToPrimitiveStats.Set( *Object->GetFullName(), FAnalyzeReferencedContentStat::FPrimitiveStats( DecalComponent ) );
		}
		else if ( SpeedTreeComponent && SpeedTreeComponent->SpeedTree )
		{
			PrimitiveStats = &ResourceNameToPrimitiveStats.Set( *SpeedTreeComponent->SpeedTree->GetFullName(), FAnalyzeReferencedContentStat::FPrimitiveStats( SpeedTreeComponent ) );
		}
		else if ( StaticMeshComponent && StaticMeshComponent->StaticMesh )
		{
			PrimitiveStats = &ResourceNameToPrimitiveStats.Set( *StaticMeshComponent->StaticMesh->GetFullName(), FAnalyzeReferencedContentStat::FPrimitiveStats( StaticMeshComponent->StaticMesh , ResourceNameToStaticMeshStats.Find( StaticMeshComponent->StaticMesh ->GetFullName() )) );
		}
		else if ( SkeletalMeshComponent && SkeletalMeshComponent->SkeletalMesh )
		{
			PrimitiveStats = &ResourceNameToPrimitiveStats.Set( *SkeletalMeshComponent->SkeletalMesh->GetFullName(), FAnalyzeReferencedContentStat::FPrimitiveStats( SkeletalMeshComponent->SkeletalMesh, ResourceNameToSkeletalMeshStats.Find( SkeletalMeshComponent->SkeletalMesh->GetFullName() )) );
		}
	}

	return PrimitiveStats;
}

/**
* Retrieves/ creates skeletal mesh stats associated with passed in skeletal mesh.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	SkeletalMesh	Skeletal mesh to retrieve/ create skeletal mesh stats for
* @return	pointer to skeletal mesh stats associated with skeletal mesh
*/
FAnalyzeReferencedContentStat::FSkeletalMeshStats* FAnalyzeReferencedContentStat::GetSkeletalMeshStats( USkeletalMesh* SkeletalMesh, UPackage* LevelPackage )
{
	FAnalyzeReferencedContentStat::FSkeletalMeshStats* SkeletalMeshStats = ResourceNameToSkeletalMeshStats.Find( SkeletalMesh->GetFullName() );

	if( SkeletalMeshStats == NULL )
	{
		SkeletalMeshStats = &ResourceNameToSkeletalMeshStats.Set( *SkeletalMesh->GetFullName(), FAnalyzeReferencedContentStat::FSkeletalMeshStats( SkeletalMesh ) );
	}

	return SkeletalMeshStats;
}

/**
* Retrieves/ creates particle stats associated with passed in particle system.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	ParticleSystem	Particle system to retrieve/ create static mesh stats for
* @return	pointer to particle system stats associated with static mesh
*/
FAnalyzeReferencedContentStat::FParticleStats* FAnalyzeReferencedContentStat::GetParticleStats( UParticleSystem* ParticleSystem )
{
	FAnalyzeReferencedContentStat::FParticleStats* ParticleStats = ResourceNameToParticleStats.Find( ParticleSystem->GetFullName() );
	if( ParticleStats == NULL )
	{
		ParticleStats = &ResourceNameToParticleStats.Set( *ParticleSystem->GetFullName(), FAnalyzeReferencedContentStat::FParticleStats( ParticleSystem ) );
	}
	return ParticleStats;
}

/**
* Retrieves/creates texture in particle system stats associated with the passed in texture.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	InTexture	The texture to retrieve/create stats for
* @return	pointer to textureinparticlesystem stats
*/
FAnalyzeReferencedContentStat::FTextureToParticleSystemStats* FAnalyzeReferencedContentStat::GetTextureToParticleSystemStats(UTexture* InTexture)
{
	FAnalyzeReferencedContentStat::FTextureToParticleSystemStats* TxtrToPSysStats = ResourceNameToTextureToParticleSystemStats.Find(InTexture->GetPathName());
	if (TxtrToPSysStats == NULL)
	{
		TxtrToPSysStats = &ResourceNameToTextureToParticleSystemStats.Set(*InTexture->GetPathName(), 
			FAnalyzeReferencedContentStat::FTextureToParticleSystemStats(InTexture));
	}

	return TxtrToPSysStats;
}


/**
* Retrieves/ creates animation sequence stats associated with passed in animation sequence.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	AnimSequence	Anim sequence to retrieve/ create anim sequence stats for
* @return	pointer to particle system stats associated with anim sequence
*/
FAnalyzeReferencedContentStat::FAnimSequenceStats* FAnalyzeReferencedContentStat::GetAnimSequenceStats( UAnimSequence* AnimSequence )
{
	FAnalyzeReferencedContentStat::FAnimSequenceStats* AnimStats = ResourceNameToAnimStats.Find( AnimSequence->GetFullName() );
	if( AnimStats == NULL )
	{
		AnimStats = &ResourceNameToAnimStats.Set( *AnimSequence->GetFullName(), FAnalyzeReferencedContentStat::FAnimSequenceStats( AnimSequence ) );
	}
	return AnimStats;
}

/**
* Retrieves/ creates animation sequence stats associated with passed in animation sequence.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	FaceFXAnimSet	FaceFXAnimSet to retrieve/ create anim sequence stats for
* @return	pointer to particle system stats associated with anim sequence
*/
FAnalyzeReferencedContentStat::FFaceFXAnimSetStats* FAnalyzeReferencedContentStat::GetFaceFXAnimSetStats( UFaceFXAnimSet* FaceFXAnimSet )
{
	FAnalyzeReferencedContentStat::FFaceFXAnimSetStats* FaceFXAnimSetStats = ResourceNameToFaceFXAnimSetStats.Find( FaceFXAnimSet->GetFullName() );
	if( FaceFXAnimSetStats == NULL )
	{
		FaceFXAnimSetStats = &ResourceNameToFaceFXAnimSetStats.Set( *FaceFXAnimSet->GetFullName(), FAnalyzeReferencedContentStat::FFaceFXAnimSetStats( FaceFXAnimSet ) );
	}
	return FaceFXAnimSetStats;
}

/**
* Retrieves/ creates lighting optimization stats associated with passed in static mesh actor.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	ActorComponent	Actor component to calculate potential light map savings stats for
* @return	pointer to lighting optimization stats associated with this actor component
*/
FAnalyzeReferencedContentStat::FLightingOptimizationStats* FAnalyzeReferencedContentStat::GetLightingOptimizationStats( AStaticMeshActor* StaticMeshActor )
{
	FAnalyzeReferencedContentStat::FLightingOptimizationStats* LightingStats = ResourceNameToLightingStats.Find( StaticMeshActor->GetFullName() );
	if( LightingStats == NULL )
	{
		LightingStats = &ResourceNameToLightingStats.Set( *StaticMeshActor->GetFullName(), FAnalyzeReferencedContentStat::FLightingOptimizationStats( StaticMeshActor ) );
	}
	return LightingStats;
}

/**
* Retrieves/ creates sound cue stats associated with passed in sound cue.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
* @return				pointer to sound cue  stats associated with sound cue  
*/
FAnalyzeReferencedContentStat::FSoundCueStats* FAnalyzeReferencedContentStat::GetSoundCueStats( USoundCue* SoundCue, UPackage* LevelPackage )
{
	FAnalyzeReferencedContentStat::FSoundCueStats* SoundCueStats = ResourceNameToSoundCueStats.Find( SoundCue->GetFullName() );

	if( SoundCueStats == NULL )
	{
		SoundCueStats = &ResourceNameToSoundCueStats.Set( *SoundCue->GetFullName(), FAnalyzeReferencedContentStat::FSoundCueStats( SoundCue ) );
	}

	return SoundCueStats;
}



/**
* Retrieves/ creates sound cue stats associated with passed in sound cue.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
* @return				pointer to sound cue  stats associated with sound cue  
*/
FAnalyzeReferencedContentStat::FSoundNodeWaveStats* FAnalyzeReferencedContentStat::GetSoundNodeWaveStats( USoundNodeWave* SoundNodeWave, UPackage* LevelPackage )
{
	FAnalyzeReferencedContentStat::FSoundNodeWaveStats* SoundNodeWaveStats = ResourceNameToSoundNodeWaveStats.Find( SoundNodeWave->GetFullName() );

	if( SoundNodeWaveStats == NULL )
	{
		SoundNodeWaveStats = &ResourceNameToSoundNodeWaveStats.Set( *SoundNodeWave->GetFullName(), FAnalyzeReferencedContentStat::FSoundNodeWaveStats( SoundNodeWave ) );
	}

	return SoundNodeWaveStats;
}



/**
* Retrieves/ creates shadowmap 1D stats associated with passed in shadowmap 1D object.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
* @return				pointer to sound cue  stats associated with sound cue  
*/
FAnalyzeReferencedContentStat::FShadowMap1DStats* FAnalyzeReferencedContentStat::GetShadowMap1DStats( UShadowMap1D* ShadowMap1D, UPackage* LevelPackage )
{
	FAnalyzeReferencedContentStat::FShadowMap1DStats* ShadowMap1DStats = ResourceNameToShadowMap1DStats.Find( ShadowMap1D->GetFullName() );

	if( ShadowMap1DStats == NULL )
	{
		ShadowMap1DStats = &ResourceNameToShadowMap1DStats.Set( *ShadowMap1D->GetFullName(), FAnalyzeReferencedContentStat::FShadowMap1DStats( ShadowMap1D ) );
	}

	return ShadowMap1DStats;
}

/**
* Retrieves/ creates shadowmap 2D stats associated with passed in shadowmap 2D object.
*
* @warning: returns pointer into TMap, only valid till next time Set is called
*
* @param	SoundCue	Sound cue  to retrieve/ create sound cue  stats for
* @return				pointer to sound cue  stats associated with sound cue  
*/
FAnalyzeReferencedContentStat::FShadowMap2DStats* FAnalyzeReferencedContentStat::GetShadowMap2DStats( UShadowMap2D* ShadowMap2D, UPackage* LevelPackage )
{
	FAnalyzeReferencedContentStat::FShadowMap2DStats* ShadowMap2DStats = ResourceNameToShadowMap2DStats.Find( ShadowMap2D->GetFullName() );

	if( ShadowMap2DStats == NULL )
	{
		ShadowMap2DStats = &ResourceNameToShadowMap2DStats.Set( *ShadowMap2D->GetFullName(), FAnalyzeReferencedContentStat::FShadowMap2DStats( ShadowMap2D ) );
	}

	return ShadowMap2DStats;
}

void UAnalyzeReferencedContentCommandlet::StaticInitialize()
{
	ShowErrorCount = FALSE;
}

/**
* Returns whether the passed in object is part of a visible level.
*
* @param Object	object to check
* @return TRUE if object is inside (as defined by chain of outers) in a visible level, FALSE otherwise
*/
UBOOL UAnalyzeReferencedContentCommandlet::IsInVisibleLevel( UObject* Object )
{
	UObject* ObjectPackage = Object->GetOutermost();
	
	for( INT LevelIndex=0; LevelIndex<CurrentLevels.Num(); LevelIndex++ )
	{
		if( ObjectPackage == CurrentLevels(LevelIndex) )
		{
			return TRUE;
		}
	}
	return FALSE;
}

/**
* Handles encountered object, routing to various sub handlers.
*
* @param	Object			Object to handle
* @param	LevelPackage	Currently loaded level package, can be NULL if not a level
* @param	bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleObject( UObject* Object, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	// Disregard marked objects as they won't go away with GC.
	if( !Object->IsPendingKill() )
	{
		if (bEmulateCooking == TRUE)
		{
			Object->StripData(Platform);
		}

		// Whether the object is the passed in level package if it is != NULL.
		const UBOOL bIsInALevelPackage = !bIsScriptReferenced; //(LevelPackage!=NULL && IsInVisibleLevel( Object ));

		if( Object->IsA(UParticleSystemComponent::StaticClass()) && bIsInALevelPackage && ((ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Particle)) == 0))
		{
			HandleStaticMeshOnAParticleSystemComponent( (UParticleSystemComponent*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle static mesh.
		else if( Object->IsA(UStaticMesh::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_StaticMesh) == 0))
		{
			HandleStaticMesh( (UStaticMesh*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handles static mesh component if it's residing in the map package. LevelPackage == NULL for non map packages.
		else if( Object->IsA(UStaticMeshComponent::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_StaticMeshComponent) == 0))
		{
			HandleStaticMeshComponent( (UStaticMeshComponent*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handles static mesh component if it's residing in the map package. LevelPackage == NULL for non map packages.
		else if( Object->IsA(AStaticMeshActor::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_StaticMeshActor) == 0))
		{
			HandleStaticMeshActor( (AStaticMeshActor*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle static mesh.
		else if( Object->IsA(USkeletalMesh::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_SkeletalMesh) == 0))
		{
			HandleSkeletalMesh( (USkeletalMesh*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handles static mesh component if it's residing in the map package. LevelPackage == NULL for non map packages.
		else if( Object->IsA(USkeletalMeshComponent::StaticClass()) )
		{
			if ( bIsInALevelPackage && ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_SkeletalMeshComponent) == 0 )
		{
				HandleSkeletalMeshComponentForSMC( (USkeletalMeshComponent*) Object, LevelPackage, bIsScriptReferenced );
			}

			if ( bIsInALevelPackage && ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Anim) == 0 )
			{
				HandleSkeletalMeshComponentForAnim( (USkeletalMeshComponent*) Object, LevelPackage, bIsScriptReferenced );
			}
		}
		// Handle material.
		else if( Object->IsA(UMaterial::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Material) == 0))
		{
			HandleMaterial( (UMaterial*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle texture.
		else if( Object->IsA(UTexture::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Texture) == 0))
		{
			HandleTexture( (UTexture*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handles brush actor if it's residing in the map package. LevelPackage == NULL for non map packages.
		else if( Object->IsA(ABrush::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Brush) == 0))
		{
			HandleBrush( (ABrush*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle particle system.
		else if( Object->IsA(UParticleSystem::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Particle) == 0))
		{
			HandleParticleSystem( (UParticleSystem*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle anim sequence.
		else if( Object->IsA(UInterpTrackAnimControl::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Anim) == 0))
		{
			HandleInterpTrackAnimControl((UInterpTrackAnimControl*) Object, LevelPackage, bIsScriptReferenced );
		}
		else if( Object->IsA(UInterpGroup::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Anim) == 0))
		{
			HandleInterpGroup((UInterpGroup*) Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle level
		else if( Object->IsA(ULevel::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Level) == 0))
		{
			HandleLevel( (ULevel*)Object, LevelPackage, bIsScriptReferenced );
		}
		// Handle sound cue
		else if (Object->IsA(USoundCue::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_SoundCue) == 0))
		{
			HandleSoundCue((USoundCue*)Object, LevelPackage, bIsScriptReferenced);
		}
		// Handle sound node wave
		else if (Object->IsA(USoundNodeWave::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_SoundNodeWave) == 0))
		{
			HandleSoundNodeWave((USoundNodeWave*)Object, LevelPackage, bIsScriptReferenced);
		}
		// Handle 1D shadow maps
		else if (Object->IsA(UShadowMap1D::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_ShadowMap) == 0))
		{
			HandleShadowMap1D((UShadowMap1D*)Object,LevelPackage,bIsScriptReferenced);			
		}
		// Handle 2D shadow maps
		else if (Object->IsA(UShadowMap2D::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_ShadowMap) == 0))
		{
			HandleShadowMap2D((UShadowMap2D*)Object,LevelPackage,bIsScriptReferenced);
		}
		else if (Object->IsA(UFaceFXAnimSet::StaticClass()) && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_FaceFXAnimSet) == 0))
		{
			// if platform is windows or script referenced, handle them
			if ( !UsingPersistentFaceFXAnimSetGenerator() || bIsScriptReferenced )
			{
				HandleFaceFXAnimSet((UFaceFXAnimSet*)Object,LevelPackage,bIsScriptReferenced);
			}
		}
		else if( Object->IsA(UModelComponent::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Primitive) == 0))
		{
			HandleModelComponent( (UModelComponent*) Object, LevelPackage, bIsScriptReferenced );
		}
		else if( Object->IsA(UDecalComponent::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Primitive) == 0))
		{
			HandleDecalComponent( (UDecalComponent*) Object, LevelPackage, bIsScriptReferenced );
		}
		else if( Object->IsA(UTerrainComponent::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Primitive) == 0))
		{
			HandleTerrainComponent( (UTerrainComponent*) Object, LevelPackage, bIsScriptReferenced );
		}
		else if( Object->IsA(USpeedTreeComponent::StaticClass()) && bIsInALevelPackage && (ReferencedContentStat.InIgnoreObjectFlag(FAnalyzeReferencedContentStat::IGNORE_Primitive) == 0))
		{
			HandleSpeedTreeComponent( (USpeedTreeComponent*) Object, LevelPackage, bIsScriptReferenced );
		}
	}
}

/**
* Handles gathering stats for passed in static mesh.
*
* @param StaticMesh	StaticMesh to gather stats for.
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleStaticMesh( UStaticMesh* StaticMesh, UPackage* LevelPackage, UBOOL bIsScriptReferenced  )
{
	FAnalyzeReferencedContentStat::FStaticMeshStats* StaticMeshStats = ReferencedContentStat.GetStaticMeshStats( StaticMesh, LevelPackage );

	if (appStristr(*StaticMesh->GetName(), TEXT("destruct")) != NULL)
	{
		debugf(TEXT("HandleStaticMesh MeshName:%s"), *StaticMesh->GetFullName());
	}

	if( bIsScriptReferenced )
	{
		StaticMeshStats->bIsReferencedByScript = TRUE;
	}

	// Populate materials array, avoiding duplicate entries.
	TArray<UMaterial*> Materials;
	// @todo need to do foreach over all LODModels
	INT MaterialCount = StaticMesh->LODModels(0).Elements.Num();
	for( INT MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++ )
	{
		UMaterialInterface* MaterialInterface = StaticMesh->LODModels(0).Elements(MaterialIndex).Material;
		if( MaterialInterface && MaterialInterface->GetMaterial() )
		{
			Materials.AddUniqueItem( MaterialInterface->GetMaterial() );
		}
	}

	// Iterate over materials and create/ update associated stats.
	for( INT MaterialIndex=0; MaterialIndex<Materials.Num(); MaterialIndex++ )
	{
		UMaterial* Material	= Materials(MaterialIndex);	
		FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ReferencedContentStat.GetMaterialStats( Material );
		MaterialStats->StaticMeshesAppliedTo.Set( *StaticMesh->GetFullName(), TRUE );
	}
}

/**
* Handles gathering stats for passed in ModelComponent.
*
* @param ModelComponent	ModelComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleModelComponent( UModelComponent* ModelComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	// We count as one Model when multiple components belong to
	// Model is what LD would care, so that is what we would like to see.
	if ( ModelComponent->GetModel() )
	{
		// If multiple terrain component shares for one terrain, we should only count once
		// If not found, then mark as to add to level once stat is created
		UBOOL bAddToLevelInfo=FALSE;
		if ( !ReferencedContentStat.ResourceNameToPrimitiveStats.Find( ModelComponent->GetModel()->GetFullName() ) )
		{
			bAddToLevelInfo = TRUE;
		}

		FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats= ReferencedContentStat.GetPrimitiveStats( ModelComponent, LevelPackage );		
		if ( bAddToLevelInfo )
		{
			PrimitiveStats->AddLevelInfo( LevelPackage, TRUE );
		}

		UModel* TheModel = ModelComponent->GetModel();
		TIndirectArray<FModelElement> Elements = ModelComponent->GetElements();

		// Build the component's index buffer and compute each element's bounding box.
		for(INT ElementIndex = 0;ElementIndex < Elements.Num();ElementIndex++)
		{
			FModelElement& Element = Elements(ElementIndex);
			if ( TheModel )
			{
				for(INT NodeIndex = 0;NodeIndex < Element.Nodes.Num();NodeIndex++)
				{
					FBspNode& Node = TheModel->Nodes(Element.Nodes(NodeIndex));
					FBspSurf& Surf = TheModel->Surfs(Node.iSurf);

					// Don't put portal polygons in the static index buffer.
					if(Surf.PolyFlags & PF_Portal)
						continue;

					for(UINT BackFace = 0;BackFace < (UINT)((Surf.PolyFlags & PF_TwoSided) ? 2 : 1);BackFace++)
					{
						if(Node.iZone[1-BackFace] == ModelComponent->GetZoneIndex() || ModelComponent->GetZoneIndex() == INDEX_NONE)
						{
							for(INT VertexIndex = 2;VertexIndex < Node.NumVertices;VertexIndex++)
							{
								// does include # of triangles of all elements
								PrimitiveStats->NumTriangles++;
							}
						}
					}
				}
			}

			PrimitiveStats->NumSections++;
		}
	}
}

/**
* Handles gathering stats for passed in DecalComponent.
*
* @param DecalComponent	DecalComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleDecalComponent( UDecalComponent* DecalComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats= ReferencedContentStat.GetPrimitiveStats( DecalComponent, LevelPackage );
	PrimitiveStats->AddLevelInfo( LevelPackage, TRUE );
	PrimitiveStats->NumInstances++;
}
/**
* Handles gathering stats for passed in TerrainComponent.
*
* @param TerrainComponent	TerrainComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleTerrainComponent( UTerrainComponent* TerrainComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	if ( TerrainComponent->GetTerrain() )
	{
		// If multiple terrain component shares for one terrain, we should only count once
		// If not found, then mark as to add to level once stat is created
		UBOOL bAddToLevelInfo=FALSE;
		if ( !ReferencedContentStat.ResourceNameToPrimitiveStats.Find( TerrainComponent->GetTerrain()->GetFullName() ) )
		{
			bAddToLevelInfo = TRUE;
		}

		FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats= ReferencedContentStat.GetPrimitiveStats( TerrainComponent, LevelPackage );
		if ( bAddToLevelInfo )
		{
			PrimitiveStats->AddLevelInfo( LevelPackage, TRUE );
		}

		PrimitiveStats->NumTriangles += TerrainComponent->GetMaxTriangleCount();
		// Currently, the engine will make a draw call per component, regardless of the number of batch materials.
		PrimitiveStats->NumSections++;
	}
}
/**
* Handles gathering stats for passed in SpeedTreeComponent.
*
* @param SpeedTreeComponent	SpeedTreeComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleSpeedTreeComponent( USpeedTreeComponent* SpeedTreeComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
#if WITH_SPEEDTREE
	if ( SpeedTreeComponent->SpeedTree )
	{
		FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats= ReferencedContentStat.GetPrimitiveStats( SpeedTreeComponent, LevelPackage );
		PrimitiveStats->AddLevelInfo( LevelPackage, TRUE );
		PrimitiveStats->NumInstances++;
	}
#endif
}

/**
* Handles gathering stats for passed in static actor component.
*
* @param StaticMeshActor	StaticMeshActor to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleStaticMeshActor( AStaticMeshActor* StaticMeshActor, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	UStaticMeshComponent* StaticMeshComponent = StaticMeshActor->StaticMeshComponent;
	if( StaticMeshComponent && StaticMeshComponent->StaticMesh && StaticMeshComponent->StaticMesh->LODModels.Num() && StaticMeshComponent->HasStaticShadowing() )
	{
		// Track lighting optimization values for a given static mesh actor.
		FAnalyzeReferencedContentStat::FLightingOptimizationStats* LightingStats = ReferencedContentStat.GetLightingOptimizationStats( StaticMeshActor );

	}
}

/**
* Handles special case for stats for passed in static mesh component who is part of a ParticleSystemComponent
*
* @param ParticleSystemComponent	ParticleSystemComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleStaticMeshOnAParticleSystemComponent( UParticleSystemComponent* ParticleSystemComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	UParticleSystemComponent* PSC = ParticleSystemComponent;
	//warnf( TEXT("%d"), PSC->SMComponents.Num() );

	UStaticMeshComponent* StaticMeshComponent = NULL;
	TArray<UStaticMesh*> ReferencedStaticMeshes;
	for( INT i = 0; i < PSC->SMComponents.Num(); ++i )
	{
		StaticMeshComponent = PSC->SMComponents(i);
		if (StaticMeshComponent && StaticMeshComponent->StaticMesh)
		{
			ReferencedStaticMeshes.AddUniqueItem( StaticMeshComponent->StaticMesh );
		}
	}

	UStaticMesh* StaticMesh = NULL;
	for( INT i = 0; i < ReferencedStaticMeshes.Num(); ++i )
	{
		//warnf( TEXT("%s"), *ReferencedStaticMeshes(i)->GetFullName() );
		StaticMesh = ReferencedStaticMeshes(i);
		HandleStaticMesh( StaticMesh, LevelPackage, bIsScriptReferenced );

		FAnalyzeReferencedContentStat::FStaticMeshStats* StaticMeshStats = ReferencedContentStat.GetStaticMeshStats( StaticMesh, LevelPackage );
		StaticMeshStats->NumInstances++;

		// Mark object as being referenced by particles.
		StaticMeshStats->bIsReferencedByParticles = TRUE;
	}
}


/**
* Handles gathering stats for passed in material.
*
* @param Material	Material to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleMaterial( UMaterial* Material, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ReferencedContentStat.GetMaterialStats( Material );	
	MaterialStats->AddLevelInfo( LevelPackage, TRUE );

	if( bIsScriptReferenced )
	{
		MaterialStats->bIsReferencedByScript = TRUE;
	}

	// Array of textures used by this material. No duplicates.
	TArray<UTexture*> TexturesUsed;
	
	Material->GetUsedTextures(TexturesUsed);

	// Update textures used by this material.
	for( INT TextureIndex=0; TextureIndex<TexturesUsed.Num(); TextureIndex++ )
	{
		UTexture* Texture = TexturesUsed(TextureIndex);
		if (Texture != NULL)
		{
			FAnalyzeReferencedContentStat::FTextureStats* TextureStats = ReferencedContentStat.GetTextureStats(Texture);
			TextureStats->MaterialsUsedBy.Set( *Material->GetFullName(), TRUE );
		}
	}
}

/**
* Handles gathering stats for passed in texture.
*
* @paramTexture	Texture to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleTexture( UTexture* Texture, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FTextureStats* TextureStats = ReferencedContentStat.GetTextureStats( Texture );

	// Only handle further if we have a level package.
	TextureStats->AddLevelInfo( LevelPackage, TRUE );

	// Mark as being referenced by script.
	if( bIsScriptReferenced )
	{
		TextureStats->bIsReferencedByScript = TRUE;
	}
}

/**
* Handles gathering stats for passed in brush.
*
* @param BrushActor Brush actor to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleBrush( ABrush* BrushActor, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	if( BrushActor->Brush && BrushActor->Brush->Polys )
	{
		UPolys* Polys = BrushActor->Brush->Polys;

		// Populate materials array, avoiding duplicate entries.
		TArray<UMaterial*> Materials;
		for( INT ElementIndex=0; ElementIndex<Polys->Element.Num(); ElementIndex++ )
		{
			const FPoly& Poly = Polys->Element(ElementIndex);
			if( Poly.Material && Poly.Material->GetMaterial() )
			{
				Materials.AddUniqueItem( Poly.Material->GetMaterial() );
			}
		}

		// Iterate over materials and create/ update associated stats.
		for( INT MaterialIndex=0; MaterialIndex<Materials.Num(); MaterialIndex++ )
		{
			UMaterial* Material = Materials(MaterialIndex);
			FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ReferencedContentStat.GetMaterialStats( Material );
			MaterialStats->NumBrushesAppliedTo++;
		}
	}
}

/**
* Handles gathering stats for passed in particle system.
*
* @param ParticleSystem	Particle system to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleParticleSystem( UParticleSystem* ParticleSystem, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FParticleStats* ParticleStats = ReferencedContentStat.GetParticleStats( ParticleSystem );

	// Only handle further if we have a level package.
	ParticleStats->AddLevelInfo( LevelPackage, TRUE );

	// Mark object as being referenced by script.
	if( bIsScriptReferenced )
	{
		ParticleStats->bIsReferencedByScript = TRUE;
	}

	// Loop over the textures used in the particle system...
	for (INT EmitterIndex = 0; EmitterIndex < ParticleSystem->Emitters.Num(); EmitterIndex++)
	{
		UParticleSpriteEmitter* Emitter = Cast<UParticleSpriteEmitter>(ParticleSystem->Emitters(EmitterIndex));
		if (Emitter)
		{
			for (INT LODIndex = 0; LODIndex < Emitter->LODLevels.Num(); LODIndex++)
			{
				UParticleLODLevel* LODLevel = Emitter->LODLevels(LODIndex);
				check(LODLevel);
				check(LODLevel->RequiredModule);

				TArray<UTexture*> OutTextures;

				// First, check the sprite material
				UMaterialInterface* MatIntf = LODLevel->RequiredModule->Material;
				if (MatIntf)
				{
					MatIntf->GetUsedTextures(OutTextures);
					for (INT TextureIndex = 0; TextureIndex < OutTextures.Num(); TextureIndex++)
					{
						UTexture* Texture = OutTextures(TextureIndex);
						Texture->ConditionalPostLoad();
						FAnalyzeReferencedContentStat::FTextureToParticleSystemStats* TxtrToPSysStats = ReferencedContentStat.GetTextureToParticleSystemStats(Texture);
						if (TxtrToPSysStats)
						{
							TxtrToPSysStats->AddParticleSystem(ParticleSystem);
						}
					}
				}

				// Check if it is a mesh emitter...
				if (LODIndex == 0)
				{
					if (LODLevel->TypeDataModule)
					{
						UParticleModuleTypeDataMesh* MeshTD = Cast<UParticleModuleTypeDataMesh>(LODLevel->TypeDataModule);
						if (MeshTD)
						{
							if (MeshTD->bOverrideMaterial == FALSE)
							{
								// Grab the materials on the mesh...
								if (MeshTD->Mesh)
								{
									for (INT LODInfoIndex = 0; LODInfoIndex < MeshTD->Mesh->LODInfo.Num(); LODInfoIndex++)
									{
										FStaticMeshLODInfo& LODInfo = MeshTD->Mesh->LODInfo(LODInfoIndex);
										for (INT ElementIndex = 0; ElementIndex < LODInfo.Elements.Num(); ElementIndex++)
										{
											FStaticMeshLODElement& Element = LODInfo.Elements(ElementIndex);
											MatIntf = Element.Material;
											if (MatIntf)
											{
												MatIntf->GetUsedTextures(OutTextures);
												for (INT TextureIndex = 0; TextureIndex < OutTextures.Num(); TextureIndex++)
												{
													UTexture* Texture = OutTextures(TextureIndex);

													FAnalyzeReferencedContentStat::FTextureToParticleSystemStats* TxtrToPSysStats = 
														ReferencedContentStat.GetTextureToParticleSystemStats(Texture);
													if (TxtrToPSysStats)
													{
														TxtrToPSysStats->AddParticleSystem(ParticleSystem);
													}
												}

											}
										}
									}
								}
							}
						}
					}
				}

				// Check for a MeshMaterial override module...
			}
		}
	}
}


/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSequence		AnimSequence to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleFaceFXAnimSet( UFaceFXAnimSet* FaceFXAnimSet, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
#if WITH_FACEFX
	FAnalyzeReferencedContentStat::FFaceFXAnimSetStats* FaceFXAnimSetStat= ReferencedContentStat.GetFaceFXAnimSetStats( FaceFXAnimSet );

	// Only handle further if we have a level package.
	FaceFXAnimSetStat->AddLevelInfo( LevelPackage, TRUE );

	// Mark object as being referenced by script.
	if( bIsScriptReferenced )
	{
		FaceFXAnimSetStat->bIsReferencedByScript = TRUE;
	}
#endif
}

/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSequence		AnimSequence to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleAnimSetInternal( UAnimSet* AnimSet, UPackage* LevelPackage, UBOOL bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType ReferenceType )
{
	if (AnimSet)	
	{
		for ( INT I=0; I<AnimSet->Sequences.Num(); ++I )
		{
			HandleAnimSequenceInternal( AnimSet->Sequences(I), LevelPackage, bIsScriptReferenced, ReferenceType );
		}
	}
}

/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSequence		AnimSequence to gather stats for
* @param LevelPackage		Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleAnimSequenceInternal( UAnimSequence* AnimSequence, UPackage* LevelPackage, UBOOL bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType ReferenceType )
{
	FAnalyzeReferencedContentStat::FAnimSequenceStats* AnimStats = ReferencedContentStat.GetAnimSequenceStats( AnimSequence );

	AnimStats->ReferenceType = ReferenceType;

	// Only handle further if we have a level package.
	AnimStats->AddLevelInfo( LevelPackage, TRUE );

	// Mark object as being referenced by script.
	if( bIsScriptReferenced )
	{
		AnimStats->bIsReferencedByScript = TRUE;
	}
}

/**
* Handles gathering stats for passed in animation sequence.
*
* @param AnimSet	InterpTrackAnimControl to gather stats for
* @param LevelPackage				Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced		Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleInterpGroup( UInterpGroup* InterpGroup, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	for (INT I=0; I<InterpGroup->GroupAnimSets.Num(); ++I)
	{
		HandleAnimSetInternal(InterpGroup->GroupAnimSets(I), LevelPackage, bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_Matinee);
	}
}

/**
* Handles gathering stats for passed in animation sequence.
*
* @param InterpTrackAnimControl		InterpTrackAnimControl to gather stats for
* @param LevelPackage				Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced		Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleInterpTrackAnimControl( UInterpTrackAnimControl* InterpTrackAnimControl, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	for (INT I=0; I<InterpTrackAnimControl->AnimSets.Num(); ++I)
	{
		HandleAnimSetInternal(InterpTrackAnimControl->AnimSets(I), LevelPackage, bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_Matinee);
	}
}

/**
* Handles gathering stats for passed in animation sequence.
*
* @param SkeletalMeshComponent		SkeletalMeshComponent to gather stats for
* @param LevelPackage				Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced		Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleSkeletalMeshComponentForAnim( USkeletalMeshComponent* SkeletalMeshComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	for (INT I=0; I<SkeletalMeshComponent->AnimSets.Num(); ++I)
	{
		HandleAnimSetInternal(SkeletalMeshComponent->AnimSets(I), LevelPackage, bIsScriptReferenced, FAnalyzeReferencedContentStat::FAnimSequenceStats::EAnimReferenceType::ART_SkeletalMeshComponent);
	}
}

void UAnalyzeReferencedContentCommandlet::HandleLevel( ULevel* Level, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	for ( TMultiMap<UStaticMesh*, FCachedPhysSMData>::TIterator MeshIt(Level->CachedPhysSMDataMap); MeshIt; ++MeshIt )
	{
		UStaticMesh* Mesh = MeshIt.Key();
		FVector Scale3D = MeshIt.Value().Scale3D;

		if(Mesh)
		{
			FAnalyzeReferencedContentStat::FStaticMeshStats* StaticMeshStats = ReferencedContentStat.GetStaticMeshStats( Mesh, LevelPackage );

			if (appStristr(*Mesh->GetName(), TEXT("destruct")) != NULL)
			{
				debugf(TEXT("HandleLevel MeshName:%s Scale: [%f %f %f]"), *Mesh->GetFullName(), Scale3D.X, Scale3D.Y, Scale3D.Z);
			}
			UBOOL bHaveScale = FALSE;
			for (INT i=0; i < StaticMeshStats->UsedAtScales.Num(); i++)
			{
				// Found a shape with the right scale
				if ((StaticMeshStats->UsedAtScales(i) - Scale3D).IsNearlyZero())
				{
					bHaveScale = TRUE;
					break;
				}
			}

			if(!bHaveScale)
			{
				if (!Scale3D.IsUniform())
				{
					StaticMeshStats->bIsMeshNonUniformlyScaled = TRUE;
					//Any non uniform scaling of this mesh with box collision will result in no collision
					if (Mesh->BodySetup	&& Mesh->BodySetup->AggGeom.BoxElems.Num() > 0)
					{
						StaticMeshStats->bShouldConvertBoxColl = TRUE;
					}
				}

				StaticMeshStats->UsedAtScales.AddItem(Scale3D);
			}
		}
	}
}

/**
* Handles gathering stats for passed in static mesh component.
*
* @param StaticMeshComponent	StaticMeshComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleStaticMeshComponent( UStaticMeshComponent* StaticMeshComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	if( StaticMeshComponent->StaticMesh )
	{
		// Populate materials array, avoiding duplicate entries.
		TArray<UMaterial*> Materials;
		if ( StaticMeshComponent->StaticMesh->LODModels.Num() )
		{
			INT MaterialCount = StaticMeshComponent->StaticMesh->LODModels(0).Elements.Num();
			for( INT MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++ )
			{
				UMaterialInterface* MaterialInterface = StaticMeshComponent->GetMaterial( MaterialIndex );
				if( MaterialInterface && MaterialInterface->GetMaterial() )
				{
					Materials.AddUniqueItem( MaterialInterface->GetMaterial() );
				}
			}
		}

		// Iterate over materials and create/ update associated stats.
		for( INT MaterialIndex=0; MaterialIndex<Materials.Num(); MaterialIndex++ )
		{
			UMaterial* Material	= Materials(MaterialIndex);	
			FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ReferencedContentStat.GetMaterialStats( Material );
			MaterialStats->NumStaticMeshInstancesAppliedTo++;
		}
		// Track static meshes used by static mesh components.
		const UBOOL bBelongsToAParticleSystemComponent = StaticMeshComponent->GetOuter()->IsA(UParticleSystemComponent::StaticClass());

		if( bBelongsToAParticleSystemComponent == FALSE )
		{
			FAnalyzeReferencedContentStat::FStaticMeshStats* StaticMeshStats = ReferencedContentStat.GetStaticMeshStats( StaticMeshComponent->StaticMesh, LevelPackage );
			StaticMeshStats->AddLevelInfo( LevelPackage, TRUE );
			StaticMeshStats->NumInstances++;

			// this is double addition in a way, but it makes it way easier to see
			// this has to be added after staticmesh stat has been added as it utilize its result
			FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats = ReferencedContentStat.GetPrimitiveStats( StaticMeshComponent, LevelPackage );
			PrimitiveStats->AddLevelInfo( LevelPackage, TRUE );
			PrimitiveStats->NumInstances++;
		}
	}
}

/**
* Handles gathering stats for passed in skeletal mesh.
*
* @param SkeletalMesh	SkeletalMesh to gather stats for.
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleSkeletalMesh( USkeletalMesh* SkeletalMesh, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FSkeletalMeshStats* SkeletalMeshStats = ReferencedContentStat.GetSkeletalMeshStats( SkeletalMesh, LevelPackage );

	if (appStristr(*SkeletalMesh->GetName(), TEXT("destruct")) != NULL)
	{
		debugf(TEXT("HandleSkeletalMesh MeshName:%s"), *SkeletalMesh->GetFullName());
	}

	if( bIsScriptReferenced )
	{
		SkeletalMeshStats->bIsReferencedByScript = TRUE;
	}

	// Populate materials array, avoiding duplicate entries.
	TArray<UMaterial*> Materials;
	// @todo need to do foreach over all LODModels
	INT MaterialCount = SkeletalMesh->Materials.Num();
	for (INT MaterialIndex = 0; MaterialIndex < MaterialCount; MaterialIndex++)
	{
		UMaterialInterface* MaterialInterface = SkeletalMesh->Materials(MaterialIndex);
		if( MaterialInterface && MaterialInterface->GetMaterial() )
		{
			Materials.AddUniqueItem( MaterialInterface->GetMaterial() );
		}
	}

	// Iterate over materials and create/ update associated stats.
	for (INT MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
	{
		UMaterial* Material	= Materials(MaterialIndex);	
		FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ReferencedContentStat.GetMaterialStats( Material );
		MaterialStats->SkeletalMeshesAppliedTo.Set( *SkeletalMesh->GetFullName(), TRUE );
	}
}

/**
* Handles gathering stats for passed in skeletal mesh component.
*
* @param SkeletalMeshComponent	SkeletalMeshComponent to gather stats for
* @param LevelPackage	Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleSkeletalMeshComponentForSMC( USkeletalMeshComponent* SkeletalMeshComponent, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	if( SkeletalMeshComponent->SkeletalMesh )
	{
		// Populate materials array, avoiding duplicate entries.
		TArray<UMaterial*> Materials;
		INT MaterialCount = SkeletalMeshComponent->SkeletalMesh->Materials.Num();
		for( INT MaterialIndex=0; MaterialIndex<MaterialCount; MaterialIndex++ )
		{
			UMaterialInterface* MaterialInterface = SkeletalMeshComponent->GetMaterial( MaterialIndex );
			if( MaterialInterface && MaterialInterface->GetMaterial() )
			{
				Materials.AddUniqueItem( MaterialInterface->GetMaterial() );
			}
		}

		// Iterate over materials and create/ update associated stats.
		for (INT MaterialIndex = 0; MaterialIndex < Materials.Num(); MaterialIndex++)
		{
			UMaterial* Material	= Materials(MaterialIndex);	
			FAnalyzeReferencedContentStat::FMaterialStats* MaterialStats = ReferencedContentStat.GetMaterialStats( Material );
			MaterialStats->NumSkeletalMeshInstancesAppliedTo++;
		}

		FAnalyzeReferencedContentStat::FSkeletalMeshStats* SkeletalMeshStats = ReferencedContentStat.GetSkeletalMeshStats( SkeletalMeshComponent->SkeletalMesh, LevelPackage );
		SkeletalMeshStats->AddLevelInfo( LevelPackage, TRUE );
		SkeletalMeshStats->NumInstances++;

		// this is double addition in a way, but it makes it way easier to see
		// this has to be added after skeletalmesh stat has been added as it utilize its result
		FAnalyzeReferencedContentStat::FPrimitiveStats* PrimitiveStats = ReferencedContentStat.GetPrimitiveStats( SkeletalMeshComponent, LevelPackage );
		PrimitiveStats->AddLevelInfo( LevelPackage, TRUE );
		PrimitiveStats->NumInstances++;
	}
}

/**
* Handles gathering stats for passed in sound cue.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleSoundCue( USoundCue* SoundCue, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FSoundCueStats* SoundCueStats = ReferencedContentStat.GetSoundCueStats( SoundCue, LevelPackage );
	// Only handle further if we have a level package.
	SoundCueStats->AddLevelInfo( LevelPackage, TRUE );

	// Mark as being referenced by script.
	if( bIsScriptReferenced )
	{
		SoundCueStats->bIsReferencedByScript = TRUE;
	}
}


/**
* Handles gathering stats for passed in sound cue.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleSoundNodeWave( USoundNodeWave* SoundNodeWave, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FSoundNodeWaveStats* SoundNodeWaveStats = ReferencedContentStat.GetSoundNodeWaveStats( SoundNodeWave, LevelPackage );

	//warnf( TEXT("Sound %s"),*SoundNodeWave->GetFullName() );

	// Only handle further if we have a level package.
	SoundNodeWaveStats->AddLevelInfo( LevelPackage, TRUE );

	// Mark as being referenced by script.
	if( bIsScriptReferenced )
	{
		SoundNodeWaveStats->bIsReferencedByScript = TRUE;
	}
}

/**
* Handles gathering stats for passed in shadow map 1D.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleShadowMap1D( UShadowMap1D* ShadowMap1D, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FShadowMap1DStats* ShadowMap1DStats = ReferencedContentStat.GetShadowMap1DStats( ShadowMap1D, LevelPackage );
	// Only handle further if we have a level package.
	ShadowMap1DStats->AddLevelInfo( LevelPackage, TRUE );
}

/**
* Handles gathering stats for passed in shadow map 2D.
*
* @param SoundCue				SoundCue to gather stats for.
* @param LevelPackage			Currently loaded level package, can be NULL if not a level
* @param bIsScriptReferenced	Whether object is handled because there is a script reference
*/
void UAnalyzeReferencedContentCommandlet::HandleShadowMap2D( UShadowMap2D* ShadowMap2D, UPackage* LevelPackage, UBOOL bIsScriptReferenced )
{
	FAnalyzeReferencedContentStat::FShadowMap2DStats* ShadowMap2DStats = ReferencedContentStat.GetShadowMap2DStats( ShadowMap2D, LevelPackage );
	// Only handle further if we have a level package.
	ShadowMap2DStats->AddLevelInfo( LevelPackage, TRUE );
}

/** This will write out the specified Stats to the AnalyzeReferencedContentCSVs dir **/
template< typename STAT_TYPE >
void FAnalyzeReferencedContentStat::WriteOutCSVs( const TMap<FString,STAT_TYPE>& StatsData, const FString& CSVDirectory, const FString& StatsName )
{
	if (StatsData.Num() > 0)
	{
		// Re-used helper variables for writing to CSV file.
		FString		CSVFilename		= TEXT("");
		FArchive*	CSVFile			= NULL;


		// Create CSV folder in case it doesn't exist yet.
		GFileManager->MakeDirectory( *CSVDirectory );

		// CSV: Human-readable spreadsheet format.
		CSVFilename	= FString::Printf(TEXT("%s%s-%s-%i.csv"), *CSVDirectory, *StatsName, GGameName, GetChangeListNumberForPerfTesting() );
		CSVFile	= GFileManager->CreateFileWriter( *CSVFilename );
		if( CSVFile != NULL )
		{	
			// Write out header row.
			const FString& HeaderRow = STAT_TYPE::GetCSVHeaderRow();
			CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

			// Write out each individual stats row.
			for( TMap<FString,STAT_TYPE>::TConstIterator It(StatsData); It; ++ It )
			{
				const STAT_TYPE& StatsEntry = It.Value();
				const FString& Row = StatsEntry.ToCSV();
				if( Row.Len() > 0 &&
					StatsEntry.ShouldLogStat() )
				{
					CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
				}
			}

			// Close and delete archive.
			CSVFile->Close();
			delete CSVFile;
		}
		else
		{
			debugf(NAME_Warning,TEXT("Could not create CSV file %s for writing."), *CSVFilename);
		}
	}
}

IMPLEMENT_COMPARE_CONSTREF( FString, AnalyzeReferencedContent, { return appStricmp( *A, *B ); } )

/** This will write out the specified Stats to the AnalyzeReferencedContentCSVs dir **/
template< typename STAT_TYPE >
void FAnalyzeReferencedContentStat::WriteOutSummaryCSVs( const TMap<FString,STAT_TYPE>& StatsData, const FString& CSVDirectory, const FString& StatsName )
{
	if (StatsData.Num() > 0)
	{
		// this will now re organize the data into Level based statistics 
		// (we can template this to do it for any stat)
		TArray<FString> LevelList;

		for( TMap<FString,STAT_TYPE>::TConstIterator It(StatsData); It; ++ It )
		{
			const STAT_TYPE& StatsEntry = It.Value();

			for( FAnalyzeReferencedContentStat::PerLevelDataMap::TConstIterator Itr(StatsEntry.LevelNameToInstanceCount); Itr; ++Itr )
			{
				// find the map name in our LevelToDataMap
				// if the data exists for this level then we need to add it
				if( LevelList.ContainsItem( Itr.Key() ) == FALSE)
				{
					LevelList.AddItem( Itr.Key() );
				}
			}
		}

		Sort<USE_COMPARE_CONSTREF( FString, AnalyzeReferencedContent )>( &LevelList( 0 ), LevelList.Num() );

		// Re-used helper variables for writing to CSV file.
		FString		CSVFilename		= TEXT("");
		FArchive*	CSVFile			= NULL;

		CSVFilename	= FString::Printf(TEXT("%s%sSummary-%s-%i.csv"), *CSVDirectory, *StatsName, GGameName, GetChangeListNumberForPerfTesting() );
		CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
		if( CSVFile != NULL )
		{	
			// Write out header row.
			const FString& HeaderRow = STAT_TYPE::GetSummaryCSVHeaderRow();
			CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

			// so now we just need to print them all out per level as we have a list of STAT_TYPE and we can use our modified ToCSV which takes a levelname
			for( INT I=0; I<LevelList.Num(); ++I)
			{
				const FString& LevelData = STAT_TYPE::ToSummaryCSV(LevelList(I), StatsData);

				if( LevelData.Len() > 0 )
				{
					CSVFile->Serialize( TCHAR_TO_ANSI( *LevelData ), LevelData.Len() );
				}
			}

			const FString& LevelData = STAT_TYPE::ToSummaryCSV(TEXT("Total"), StatsData);
			if( LevelData.Len() > 0 )
			{
				CSVFile->Serialize( TCHAR_TO_ANSI( *LevelData ), LevelData.Len() );
			}

			// Close and delete archive.
			CSVFile->Close();
			delete CSVFile;
		}
		else
		{
			warnf(NAME_Warning,TEXT("Could not create CSV file %s for writing."), *CSVFilename);
		}
	}

}

/** This will write out the specified Stats to the CSVDirectory/Level Dir**/
template< typename STAT_TYPE >
void FAnalyzeReferencedContentStat::WriteOutCSVsPerLevel( const TMap<FString,STAT_TYPE>& StatsData, const FString& CSVDirectory, const FString& StatsName )
{
	if (StatsData.Num() > 0)
	{
		// this will now re organize the data into Level based statistics 
		// (we can template this to do it for any stat)
		typedef TMap<FString,TArray<STAT_TYPE>> LevelToDataMapType;
		LevelToDataMapType LevelToDataMap;

		for( TMap<FString,STAT_TYPE>::TConstIterator It(StatsData); It; ++ It )
		{
			const STAT_TYPE& StatsEntry = It.Value();

			for( FAnalyzeReferencedContentStat::PerLevelDataMap::TConstIterator Itr(StatsEntry.LevelNameToInstanceCount); Itr; ++Itr )
			{
				// find the map name in our LevelToDataMap
				TArray<STAT_TYPE>* LevelData = LevelToDataMap.Find( Itr.Key() );
				// if the data exists for this level then we need to add it
				if( LevelData == NULL )
				{
					TArray<STAT_TYPE> NewArray;
					NewArray.AddItem( StatsEntry );
					LevelToDataMap.Set( Itr.Key(), NewArray  );
				}
				else
				{
					LevelData->AddItem( StatsEntry );
				}
			}
		}

		// Re-used helper variables for writing to CSV file.
		FString		CSVFilename		= TEXT("");
		FArchive*	CSVFile			= NULL;

		// so now we just need to print them all out per level as we have a list of STAT_TYPE and we can use our modified ToCSV which takes a levelname
		for( LevelToDataMapType::TConstIterator Itr(LevelToDataMap); Itr; ++Itr )
		{
			const FString LevelSubDir = CSVDirectory + FString::Printf( TEXT("%s"), PATH_SEPARATOR TEXT("Levels") PATH_SEPARATOR );
			// CSV: Human-readable spreadsheet format.
			CSVFilename	= FString::Printf(TEXT("%s%s-%s-%i.csv")
				, *LevelSubDir
				, *FString::Printf( TEXT( "%s-%s" ), *Itr.Key(), *StatsName )
				, GGameName
				, GetChangeListNumberForPerfTesting()
				);

			CSVFile = GFileManager->CreateFileWriter( *CSVFilename );
			if( CSVFile != NULL )
			{	
				// Write out header row.
				const FString& HeaderRow = STAT_TYPE::GetCSVHeaderRow();
				CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

				// Write out each individual stats row.
				for( TArray<STAT_TYPE>::TConstIterator It(Itr.Value()); It; ++It )
				{
					const STAT_TYPE& StatsEntry = *It;
					const FString& Row = StatsEntry.ToCSV( Itr.Key() );
					if( Row.Len() > 0 )
					{
						CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
					}
				}

				// Close and delete archive.
				CSVFile->Close();
				delete CSVFile;
			}
			else
			{
				debugf(NAME_Warning,TEXT("Could not create CSV file %s for writing."), *CSVFilename);
			}
		}
	}
}
/**
* Setup the commandlet's platform setting based on commandlet params
* @param Params The commandline parameters to the commandlet - should include "platform=xxx"
*/
UBOOL SetPlatform(const FString& Params)
{
	// default to success
	UBOOL Ret = TRUE;

	FString PlatformStr;
	if (Parse(*Params, TEXT("PLATFORM="), PlatformStr))
	{
		if (PlatformStr == TEXT("PS3"))
		{
			Platform = UE3::PLATFORM_PS3;
		}
		else if (PlatformStr == TEXT("xenon") || PlatformStr == TEXT("xbox360"))
		{	
			Platform = UE3::PLATFORM_Xbox360;
		}
		else if (PlatformStr == TEXT("pc") || PlatformStr == TEXT("win32"))
		{
			Platform = UE3::PLATFORM_Windows;
		}
		else
		{
			// this is a failure
			Ret = FALSE;
		}
	}
	else
	{
		Ret = FALSE;
	}

	return Ret;
}

/**
* Fill up persistent map list among MapList
* @param MapList list of name of maps - only map names
*/
INT UAnalyzeReferencedContentCommandlet::FillPersistentMapList( const TArray<FString>& MapList )
{
	TMultiMap<FString, FString>		SubLevelMap;

	PersistentMapList.Empty();

	TArray<FString> LocalMapList = MapList;

	// Collect garbage, going back to a clean slate.
	UObject::CollectGarbage( RF_Native );

	// go through load sublevels and fill up SubLevelMap
	for ( INT I=0; I<LocalMapList.Num(); ++I )
	{
		FFilename Filename = LocalMapList(I);
		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
		if( Package )
		{
			// Find the world and load all referenced levels.
			UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
			if( World )
			{
				// Iterate over streaming level objects loading the levels.		
				AWorldInfo* WorldInfo	= World->GetWorldInfo();
				for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
				{
					ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
					if( StreamingLevel )
					{
						// Load package if found.
						FString SubFilename;
						if( GPackageFileCache->FindPackageFile( *StreamingLevel->PackageName.ToString(), NULL, SubFilename ) )
						{
							SubLevelMap.Add(Filename, SubFilename);
						}
					}
				}
			}
			else // world is not found, then please remove me from LocalMapList
			{
				LocalMapList.Remove(I);
				--I;
			}
		}
		else // package is unloadable, then please remove me from LocalMapList
		{
			LocalMapList.Remove(I);
			--I;
		}
	}

	// Collect garbage, going back to a clean slate.
	UObject::CollectGarbage( RF_Native );

	TArray<FString> SubLevelList;
	// get all sublevel list first
	SubLevelMap.GenerateValueArray( SubLevelList );

	if ( SubLevelList.Num() )
	{
		// Now see if this map can be found in sublevel list
		for ( INT I=0; I<LocalMapList.Num(); ++I )
		{
			// if I can't find current map in the sublevel list, 
			// this doesn't work if it goes down to 2-3 layer 
			// say the command for this commandlet was only Level1, Level2, Level3
			// this will go through find Level-1, Level1-2, Level1-3
			if ( SubLevelList.ContainsItem( LocalMapList(I) ) == FALSE )
			{
				PersistentMapList.AddUniqueItem( LocalMapList(I) );
			}
		}
	}
	else
	{
		PersistentMapList = LocalMapList;
	}

	return PersistentMapList.Num();
}
/**
* Handle Persistent FaceFXAnimset
* @param MapList list of name of maps - only map names
*/
UBOOL UAnalyzeReferencedContentCommandlet::HandlePersistentFaceFXAnimSet( const TArray<FString>& MapList, UBOOL bEnableLog  )
{
#if WITH_FACEFX
	if ( FillPersistentMapList(MapList) > 0 )
	{
		AnalyzeReferencedContent_PersistentMapInfoHelper.GeneratePersistentMapList(MapList, TRUE, FALSE);
		// this fills up PersistentMapList
		PersistentFaceFXAnimSetGenerator.SetLogPersistentFaceFXGeneration( bEnableLog );
		PersistentFaceFXAnimSetGenerator.SetPersistentMapInfo(&AnalyzeReferencedContent_PersistentMapInfoHelper);
		PersistentFaceFXAnimSetGenerator.SetupScriptReferencedFaceFXAnimSets();

		UFaceFXAnimSet * FaceFXAnimSet;
		for (INT I=0; I<PersistentMapList.Num(); ++I)
		{
			FFilename Filename = PersistentMapList(I);
			// generate persistent map facefx animset
			FaceFXAnimSet = PersistentFaceFXAnimSetGenerator.GeneratePersistentMapFaceFXAnimSet( Filename );
			if (FaceFXAnimSet)
			{
				// I need to load package later because GeneratePersistentMap function GCed if I do it earlier
				UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );
				if( Package  )
				{
					HandleFaceFXAnimSet( FaceFXAnimSet, Package, FALSE );
				}
			}
		}

		// Collect garbage, going back to a clean slate.
		UObject::CollectGarbage( RF_Native );

		return TRUE;
	}
#endif

	return FALSE;
}

/**
 * We use the CreateCustomEngine call to set some flags which will allow SerializeTaggedProperties to have the correct settings
 * such that editoronly and notforconsole data can correctly NOT be loaded from startup packages (e.g. engine.u)
 *
 **/
void UAnalyzeReferencedContentCommandlet::CreateCustomEngine()
{
	bEmulateCooking = FALSE;

	const TCHAR* Params = appCmdLine();
	TArray<FString> Tokens;
	TArray<FString> Switches;

	ParseCommandLine(Params, Tokens, Switches);
	for (INT SwitchIdx = 0; SwitchIdx < Switches.Num(); SwitchIdx++)
	{
		Switches(SwitchIdx) = Switches(SwitchIdx).ToUpper();
		if (Switches(SwitchIdx) == TEXT("COOKED"))
		{
			bEmulateCooking = TRUE;
			break;
		}
	}
    
	if (bEmulateCooking == TRUE)
	{
		warnf(NAME_Log, TEXT("Emulating cooked content!"));
		GIsCooking = TRUE;
		GCookingTarget = UE3::PLATFORM_Stripped;
	}
}

INT UAnalyzeReferencedContentCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	if ( SetPlatform(Params)==FALSE )
	{
		Platform = UE3::PLATFORM_Xbox360;
	}

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Whether to only deal with map files.
	const UBOOL bShouldOnlyLoadMaps	= Switches.FindItemIndex(TEXT("MAPSONLY")) != INDEX_NONE;
	// Whether to exclude script references.
	const UBOOL bExcludeScript		= Switches.FindItemIndex(TEXT("EXCLUDESCRIPT")) != INDEX_NONE;
	// Whether to load non native script packages (e.g. useful for seeing what will always be loaded)
	const UBOOL bExcludeNonNativeScript = Switches.FindItemIndex(TEXT("EXCLUDENONNATIVESCRIPT")) != INDEX_NONE;
	// Whether to automatically load all the sublevels from the world
	const UBOOL bAutoLoadSublevels = Switches.FindItemIndex(TEXT("LOADSUBLEVELS")) != INDEX_NONE;
	// Whether to examine shared textures of materisl between diffuse and specular color inputs
	const UBOOL bTestUniqueSpecularTexture = Switches.FindItemIndex(TEXT("TESTSPECULARTEXTURE")) != INDEX_NONE;


	// TODO: look on commandline for CLP to read map list from .ini file
	//	SP section
	//	MP section




	INT IgnoreObjects = 0;

	if (Switches.FindItemIndex(TEXT("ANIMSONLY")) != INDEX_NONE)
	{
		IgnoreObjects = -1;
		IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Anim;
	}

	if (Switches.FindItemIndex(TEXT("IGNOREALL")) != INDEX_NONE)				IgnoreObjects = -1;

	if (Switches.FindItemIndex(TEXT("IGNORESTATICMESH")) != INDEX_NONE)			IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_StaticMesh;
	if (Switches.FindItemIndex(TEXT("IGNORESMC")) != INDEX_NONE)				IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_StaticMeshComponent;
	if (Switches.FindItemIndex(TEXT("IGNORESTATICMESHACTOR")) != INDEX_NONE)	IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_StaticMeshActor;
	if (Switches.FindItemIndex(TEXT("IGNORETEXTURE")) != INDEX_NONE)			IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_Texture;
	if (Switches.FindItemIndex(TEXT("IGNOREMATERIAL")) != INDEX_NONE)			IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_Material;
	if (Switches.FindItemIndex(TEXT("IGNOREPARTICLE")) != INDEX_NONE)			IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_Particle;
	if (Switches.FindItemIndex(TEXT("IGNOREANIMS")) != INDEX_NONE)				IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_Anim;
	if (Switches.FindItemIndex(TEXT("IGNORESOUNDCUE")) != INDEX_NONE)			IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_SoundCue;
	if (Switches.FindItemIndex(TEXT("IGNORESOUNDWAVENODE")) != INDEX_NONE)		IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_SoundNodeWave;
	if (Switches.FindItemIndex(TEXT("IGNOREBRUSH")) != INDEX_NONE)				IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_Brush;
	if (Switches.FindItemIndex(TEXT("IGNORELEVEL")) != INDEX_NONE)				IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_Level;
	if (Switches.FindItemIndex(TEXT("IGNORESHADOWMAP")) != INDEX_NONE)			IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_ShadowMap;
	if (Switches.FindItemIndex(TEXT("IGNOREFACEFX")) != INDEX_NONE)				IgnoreObjects |= FAnalyzeReferencedContentStat::IGNORE_FaceFXAnimSet;

	if (Switches.FindItemIndex(TEXT("KEEPSTATICMESH")) != INDEX_NONE)			IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_StaticMesh;
	if (Switches.FindItemIndex(TEXT("KEEPSMC")) != INDEX_NONE)					IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_StaticMeshComponent;
	if (Switches.FindItemIndex(TEXT("KEEPSTATICMESHACTOR")) != INDEX_NONE)		IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_StaticMeshActor;
	if (Switches.FindItemIndex(TEXT("KEEPTEXTURE")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Texture;
	if (Switches.FindItemIndex(TEXT("KEEPMATERIAL")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Material;
	if (Switches.FindItemIndex(TEXT("KEEPPARTICLE")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Particle;
	if (Switches.FindItemIndex(TEXT("KEEPANIMS")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Anim;
	if (Switches.FindItemIndex(TEXT("KEEPSOUNDCUE")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_SoundCue;
	if (Switches.FindItemIndex(TEXT("KEEPSOUNDWAVENODE")) != INDEX_NONE)		IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_SoundNodeWave;
	if (Switches.FindItemIndex(TEXT("KEEPBRUSH")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Brush;
	if (Switches.FindItemIndex(TEXT("KEEPLEVEL")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_Level;
	if (Switches.FindItemIndex(TEXT("KEEPSHADOWMAP")) != INDEX_NONE)			IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_ShadowMap;
	if (Switches.FindItemIndex(TEXT("KEEPFACEFX")) != INDEX_NONE)				IgnoreObjects &= ~FAnalyzeReferencedContentStat::IGNORE_FaceFXAnimSet;

	ReferencedContentStat.SetIgnoreObjectFlag(IgnoreObjects);

	if( bExcludeNonNativeScript == FALSE )
	{
		// Load up all script files in EditPackages.
		const UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
		for( INT i=0; i<EditorEngine->EditPackages.Num(); i++ )
		{
			UPackage* Package = LoadPackage( NULL, *EditorEngine->EditPackages(i), LOAD_NoWarn );
			Package->AddToRoot();
		}
	}

	// Mark loaded objects as they are part of the always loaded set and are not taken into account for stats.
	for( TObjectIterator<UObject> It; It; ++It )
	{
		UObject* Object = *It;
		// Script referenced asset.
		if( !bExcludeScript )
		{
			HandleObject( Object, NULL, TRUE );
		}
		// Mark object as always loaded so it doesn't get counted multiple times.
		Object->SetFlags( RF_Marked );
	}

	// Get time as a string
	CurrentTime = appSystemTimeString();
	WriteOutAndEmptyCurrentlyReferencedContent();
	ReferencedContentStat.SetIgnoreObjectFlag(IgnoreObjects);

	// See if loading maps from the ini was passed in...
	GEditor->ParseMapSectionIni(*Params, Tokens);

	TArray<FString> FileList;

	// Build package file list from passed in command line if tokens are specified.
	if( Tokens.Num() )
	{
		for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
		{
			// Lookup token in file cache and add filename if found.
			FString OutFilename;
			if( GPackageFileCache->FindPackageFile( *Tokens(TokenIndex), NULL, OutFilename ) )
			{
				new(FileList)FString(OutFilename);
			}
		}
	}
	// Or use all files otherwise.
	else
	{
		FileList = GPackageFileCache->GetPackageFileList();
	}

	if( FileList.Num() == 0 )
	{
		warnf( NAME_Warning, TEXT("No packages found") );
		return 1;
	}

	// Find shader types.
	FAnalyzeReferencedContentStat::FMaterialStats::SetupShaders();
	FAnalyzeReferencedContentStat::FMaterialStats::TestUniqueSpecularTexture = bTestUniqueSpecularTexture;

	// Iterate over all files, loading up ones that have the map extension..
	for( INT FileIndex=0; FileIndex<FileList.Num(); FileIndex++ )
	{
		const FFilename& Filename = FileList(FileIndex);		

		// Disregard filenames that don't have the map extension if we're in MAPSONLY mode.
		if( bShouldOnlyLoadMaps && (Filename.GetExtension() != FURL::DefaultMapExt) )
		{
			continue;
		}

		// Skip filenames with the script extension. @todo: don't hardcode .u as the script file extension
		if( (Filename.GetExtension() == TEXT("u")) )
		{
			continue;
		}

		warnf( NAME_Log, TEXT("Loading %s"), *Filename );
		UPackage* Package = UObject::LoadPackage( NULL, *Filename, LOAD_None );

		if( Package == NULL )
		{
			warnf( NAME_Error, TEXT("Error loading %s!"), *Filename );
		}
		else
		{
			// Find the world and load all referenced levels.
			UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
			if( World )
			{
				World->AddToRoot();
				CurrentLevels.AddItem( Package );

				// Figure out whether package is a map or content package.
				UBOOL bIsAMapPackage = World ? TRUE : FALSE;

				if ( bIsAMapPackage )
				{
					// this is map - so add to MapFileList
					ReferencedContentStat.MapFileList.AddUniqueItem(Filename);
				}

				// Handle currently loaded objects.
				for( TObjectIterator<UObject> It; It; ++It )
				{
					UObject* Object = *It;
					if( ( Object->HasAnyFlags(RF_Marked) == FALSE ) && ( Object->IsIn(GetTransientPackage()) == FALSE ) )
					{
						//warnf( TEXT("HO: %s"), *Object->GetFullName() );
						HandleObject( Object, Package, FALSE );
					}
				}

				UObject::CollectGarbage( RF_Marked );



				if ( bAutoLoadSublevels )
				{
					AWorldInfo* WorldInfo	= World->GetWorldInfo();
					// Iterate over streaming level objects loading the levels.
					for( INT LevelIndex=0; LevelIndex<WorldInfo->StreamingLevels.Num(); LevelIndex++ )
					{
						ULevelStreaming* StreamingLevel = WorldInfo->StreamingLevels(LevelIndex);
						if( StreamingLevel )
						{
							// Load package if found.
							FString SubFilename;
							if( GPackageFileCache->FindPackageFile( *StreamingLevel->PackageName.ToString(), NULL, SubFilename ) )
							{
								warnf(NAME_Log, TEXT("Loading sub-level %s"), *SubFilename);
								UPackage * LevelPackage = UObject::LoadPackage( NULL, *SubFilename, LOAD_None );

								CurrentLevels.AddItem(LevelPackage);

								// this is map - so add to MapFileList - 
								// FIXME: Just for test
								ReferencedContentStat.MapFileList.AddUniqueItem(SubFilename);


								// Figure out whether package is a map or content package.
								UBOOL bIsAMapPackage = World ? TRUE : FALSE;

								if ( bIsAMapPackage )
								{
									// this is map - so add to MapFileList
									ReferencedContentStat.MapFileList.AddUniqueItem(Filename);
								}

								// Handle currently loaded objects.
								for( TObjectIterator<UObject> It; It; ++It )
								{
									UObject* Object = *It;
									if( ( Object->HasAnyFlags(RF_Marked) == FALSE ) && ( Object->IsIn(GetTransientPackage()) == FALSE ) )
									{
										HandleObject( Object, bIsAMapPackage ? Package : NULL, FALSE );
									}
								}

								UObject::CollectGarbage( RF_Marked );

							}
						}
					}
				}

				World->RemoveFromRoot();
				UObject::CollectGarbage( RF_Marked );
			}


// 			for( TObjectIterator<UStaticMesh> It; It; ++It )
// 			{
// 				UStaticMesh* Mesh = *It;
// 				if (appStristr(*Mesh->GetName(), TEXT("destruct")) != NULL)
// 				{
// 					debugf(TEXT("ITERATOR MeshName:%s"), *Mesh->GetFullName());
// 				}
// 			}

			CurrentLevels.Empty();
		}

		// Collect garbage, going back to a clean slate.
		UObject::CollectGarbage( RF_Native );

		// Verify that everything we cared about got cleaned up correctly.
		UBOOL bEncounteredUnmarkedObject = FALSE;
		for( TObjectIterator<UObject> It; It; ++It )
	{
		UObject* Object = *It;
			if( !Object->HasAllFlags( RF_Marked ) && !Object->IsIn(UObject::GetTransientPackage()) )
		{
				bEncounteredUnmarkedObject = TRUE;
				warnf(TEXT("----------------------------------------------------------------------------------------------------"));
				warnf(TEXT("%s didn't get cleaned up!"),*Object->GetFullName());
				UObject::StaticExec(*FString::Printf(TEXT("OBJ REFS CLASS=%s NAME=%s"),*Object->GetClass()->GetName(),*Object->GetPathName()));
				TMap<UObject*,UProperty*>	Route		= FArchiveTraceRoute::FindShortestRootPath( Object, TRUE, RF_Native  );
				FString						ErrorString	= FArchiveTraceRoute::PrintRootPath( Route, Object );
				warnf(TEXT("%s"),*ErrorString);
	}
		}
 		check(!bEncounteredUnmarkedObject);
	}

	// now MapFileList should be filled, go for persistentfacefx animset generator
	if ( UsingPersistentFaceFXAnimSetGenerator() )
	{
		// FIXME
		HandlePersistentFaceFXAnimSet( ReferencedContentStat.MapFileList, TRUE );
	}

	WriteOutAndEmptyCurrentlyReferencedContent();
	ReferencedContentStat.SetIgnoreObjectFlag(IgnoreObjects);

	return 0;
}


void UAnalyzeReferencedContentCommandlet::WriteOutAndEmptyCurrentlyReferencedContent()
{
	// Re-used helper variables for writing to CSV file.
	const FString CSVDirectory = appGameLogDir() + TEXT("AnalyzeReferencedContentCSVs") + PATH_SEPARATOR + FString::Printf( TEXT("%s-%d-%s"), GGameName, GetChangeListNumberForPerfTesting(), *CurrentTime ) + PATH_SEPARATOR;

	// Create CSV folder in case it doesn't exist yet.
	GFileManager->MakeDirectory( *CSVDirectory );

	ReferencedContentStat.WriteOutAllAvailableStatData( CSVDirectory );

	// "empty" our referenced contrent stats
	FAnalyzeReferencedContentStat NewReferencedContentStat;
	ReferencedContentStat = NewReferencedContentStat;
}

//////////////////////////////////////////////////////
//UAnalyzeReferencedObjectCommandlet
/////////////////////////////////////////////////////

INT UAnalyzeReferencedObjectCommandlet::Main( const FString& Params )
{
	// Parse command line.
	TArray<FString> Tokens;
	TArray<FString> Switches;

	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	// Whether to automatically load all the sublevels from the world
	const UBOOL bAutoLoadChildClasses = Switches.FindItemIndex(TEXT("LOADCHILDCLASSES")) != INDEX_NONE;

	// Load up all script files in EditPackages.
	const UEditorEngine* EditorEngine = CastChecked<UEditorEngine>(GEngine);
	for( INT i=0; i<EditorEngine->EditPackages.Num(); i++ )
	{
		LoadPackage( NULL, *EditorEngine->EditPackages(i), LOAD_NoWarn );
	}

	TArray<UClass*> ClassList;

	// Build package file list from passed in command line if tokens are specified.
	if( Tokens.Num() )
	{
		for( INT TokenIndex=0; TokenIndex<Tokens.Num(); TokenIndex++ )
		{
			// Lookup token in file cache and add filename if found.
			FString OutFilename;
			if( GPackageFileCache->FindPackageFile( *Tokens(TokenIndex), NULL, OutFilename ) )
			{
				LoadPackage( NULL, *OutFilename, LOAD_NoWarn );
			}
			else
			{
				UClass * Class=NULL;
				if (ParseObject<UClass>( *Tokens(TokenIndex), TEXT("CLASS="), Class, ANY_PACKAGE ))
				{
					// only actor class
					ClassList.AddUniqueItem(Class);
				}
			}
		}
	}
	if (ClassList.Num() <= 0)
	{
		warnf(TEXT("No class input entered"));
		return 0;
	}

	// class to look for...
	if ( ClassList.Num() > 0 )
	{
		for (INT ClassId=0; ClassId<ClassList.Num(); ++ClassId)
		{
			ExtractReferenceData(ClassList(ClassId));

			if ( bAutoLoadChildClasses )
			{
				for( TObjectIterator<UClass> It; It; ++It )
				{
					UClass * Class = *It;
					if (Class->IsChildOf(ClassList(ClassId)))
					{
						ExtractReferenceData(Class);
					}
				}
			}
		}
	}

	// output to log
	// Get time as a string
	const FString CurrentTime = appSystemTimeString();

	// Re-used helper variables for writing to CSV file.
	const FString CSVDirectory = appGameLogDir() + TEXT("ObjectStatsCSVs") + PATH_SEPARATOR + FString::Printf( TEXT("%s-%d-%s"), GGameName, GetChangeListNumberForPerfTesting(), *CurrentTime ) + PATH_SEPARATOR;

	// Create CSV folder in case it doesn't exist yet.
	GFileManager->MakeDirectory( *CSVDirectory );

	WriteOutAllAvailableData( CSVDirectory );

	return 0;
}

// this function does
// 1. Spawns actor of the class
// 2. Get object list
// 3. Get Diff from previous saved list
// 4. Update the Diff to the global list
AActor * UAnalyzeReferencedObjectCommandlet::ExtractActorReferencedObjectList(UClass * Class, INT SpawnIndex)
{
	FObjectList PreviousObjectList(UObject::StaticClass());
	AActor* Referencer = GWorld->SpawnActor(Class);
	if (Referencer)
	{
		GWorld->UpdateComponents( TRUE );

		FClassReferenceData ClassReferenceData;

		// get object list
		FObjectList CurrentObjectList(UObject::StaticClass());
		// get diff from previous saved list
		FObjectList DiffObjectList = CurrentObjectList - PreviousObjectList;
		// Remove the self object
		DiffObjectList.RemoveObject(Referencer);

		PreviousObjectList = CurrentObjectList;

		// Update the Diff to the global list

		for (INT I=0; I<DiffObjectList.ObjectList.Num(); ++I)
		{
			UObject * Current = DiffObjectList.ObjectList(I);

			// add to referencedclass
			ReferencedClassList.AddUniqueItem(Current->GetClass());

			//warnf(TEXT("\tReferenced class %s...."), *ReferencedClass->GetPathName());

			FObjectReferenceData *RefData = ClassReferenceData.ObjectRefData.Find(Current->GetClass());
			if (RefData)
			{
				++RefData->NumberOfReferenced;
				RefData->ObjectSize+= FObjectList::GetObjectSize(Current);
			}
			else
			{
				FObjectReferenceData NewData;
				NewData.ReferencedClass = Current->GetClass();
				NewData.NumberOfReferenced = 1;
				NewData.ObjectSize = FObjectList::GetObjectSize(Current);
				ClassReferenceData.ObjectRefData.Set(Current->GetClass(), NewData);
			}
		}

		ClassReferenceData.MemorySize = DiffObjectList.GetTotalSize();
		ReferenceData.Set(FClassID(Class, SpawnIndex), ClassReferenceData);
	}

	return Referencer;
}

UBOOL UAnalyzeReferencedObjectCommandlet::ExtractReferenceData(UClass* Class)
{
	if ( Class && !(Class->ClassFlags & CLASS_Abstract) )
	{
		warnf(TEXT("Creating %s class...."), *Class->GetPathName());

		AActor* Referencers[3];
		// spawn first object
		Referencers[0] = ExtractActorReferencedObjectList(Class, 0);
		if ( Referencers[0] )
		{
			Referencers[1] = ExtractActorReferencedObjectList(Class, 1);
			Referencers[2] = ExtractActorReferencedObjectList(Class, 2);

			GWorld->DestroyActor(Referencers[0]);

			if (Referencers[1])
			{
				GWorld->DestroyActor(Referencers[1]);
			}

			if (Referencers[2])
			{
				GWorld->DestroyActor(Referencers[2]);
			}

			warnf(TEXT("Clearing reference of %s class.... "), *Class->GetPathName());

			warnf(TEXT("Collecting Garbage.... "));
			UObject::CollectGarbage(GARBAGE_COLLECTION_KEEPFLAGS);
			warnf(TEXT("DONE.... "));
		}

		return TRUE;
	}

	return FALSE;
}

void UAnalyzeReferencedObjectCommandlet::CollectAllRowData(TMap<UClass *, TArray<FReferenceData>>& OutputData)
{
	INT TotalNumOfColumn = ReferenceData.Num();

	for ( INT ClassID=0; ClassID<ReferencedClassList.Num(); ++ClassID )
	{
		TArray<FReferenceData> Row;
		Row.Empty(TotalNumOfColumn);

		for (TMap<FClassID, FClassReferenceData>::TIterator Iter(ReferenceData); Iter; ++Iter)
		{
			FClassID & Class = Iter.Key();
			FClassReferenceData &ClassRefs = Iter.Value();

			// find and see if this class is found
			FObjectReferenceData *ObjData = ClassRefs.ObjectRefData.Find(ReferencedClassList(ClassID));
			if (ObjData)
			{
				Row.AddItem(FReferenceData(ObjData->ObjectSize, ObjData->NumberOfReferenced));
			}
			else
			{
				Row.AddItem(FReferenceData());
			}
		}

		OutputData.Set(ReferencedClassList(ClassID), Row);
	}
}

void UAnalyzeReferencedObjectCommandlet::WriteOutReferenceCount(const FString& FileName, const TMap<UClass *, TArray<FReferenceData>>& OutputData)
{
	FArchive*	CSVFile			= NULL;
	CSVFile	= GFileManager->CreateFileWriter( *FileName );
	if( CSVFile != NULL )
	{	
		// Write out header row.
		FString HeaderRow = TEXT("Referenced Class,");

		for (TMap<FClassID, FClassReferenceData>::TIterator Iter(ReferenceData); Iter; ++Iter)
		{
			FClassID Class = Iter.Key();
			HeaderRow += FString::Printf(TEXT("%s(%d),"), *Class.Class->GetName(), Class.SpawnIdx);
		}

		HeaderRow += LINE_TERMINATOR;
		CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

		for ( INT ClassID=0; ClassID<ReferencedClassList.Num(); ++ClassID )
		{
			FString Row = FString::Printf(TEXT("%s,"), *ReferencedClassList(ClassID)->GetName());
			const TArray<FReferenceData>* DataList = OutputData.Find(ReferencedClassList(ClassID));
			if (DataList)
			{
				for (INT ID=0; ID<DataList->Num(); ++ID)
				{
					const FReferenceData &RD = DataList->GetTypedData()[ID];
					Row+= FString::Printf(TEXT("%d,"), RD.ReferenceCount);
				}
			}

			Row += LINE_TERMINATOR;
			CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
		}
		// Close and delete archive.
		CSVFile->Close();
		delete CSVFile;
	}
}

void UAnalyzeReferencedObjectCommandlet::WriteOutResourceSize(const FString& FileName, const TMap<UClass *, TArray<FReferenceData>>& OutputData)
{
	FArchive*	CSVFile			= NULL;
	CSVFile	= GFileManager->CreateFileWriter( *FileName );
	if( CSVFile != NULL )
	{	
		// Write out header row.
		FString HeaderRow = TEXT("Referenced Class,");

		for (TMap<FClassID, FClassReferenceData>::TIterator Iter(ReferenceData); Iter; ++Iter)
		{
			FClassID Class = Iter.Key();
			HeaderRow += FString::Printf(TEXT("%s(%d),"), *Class.Class->GetName(), Class.SpawnIdx);
		}

		HeaderRow += LINE_TERMINATOR;
		CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

		for ( INT ClassID=0; ClassID<ReferencedClassList.Num(); ++ClassID )
		{
			FString Row = FString::Printf(TEXT("%s,"), *ReferencedClassList(ClassID)->GetName());
			const TArray<FReferenceData>* DataList = OutputData.Find(ReferencedClassList(ClassID));
			if (DataList)
			{
				for (INT ID=0; ID<DataList->Num(); ++ID)
				{
					const FReferenceData &RD = DataList->GetTypedData()[ID];
					Row+= FString::Printf(TEXT("%d,"), RD.ResourceSize);
				}
			}

			Row += LINE_TERMINATOR;
			CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
		}
		// Close and delete archive.
		CSVFile->Close();
		delete CSVFile;
	}
}

void UAnalyzeReferencedObjectCommandlet::WriteOutAllocationData(const FString& FileName)
{
	FArchive*	CSVFile			= NULL;
	CSVFile	= GFileManager->CreateFileWriter( *FileName );
	if( CSVFile != NULL )
	{	
		// Write out header row.
		FString HeaderRow = TEXT("Class,SpawnIndex,MemoryAllocation");
		HeaderRow += LINE_TERMINATOR;
		CSVFile->Serialize( TCHAR_TO_ANSI( *HeaderRow ), HeaderRow.Len() );

		for (TMap<FClassID, FClassReferenceData>::TIterator Iter(ReferenceData); Iter; ++Iter)
		{
			FClassID Class = Iter.Key();
			FClassReferenceData &Data = Iter.Value();
			FString Row = FString::Printf(TEXT("%s,%d,%d"), *Class.Class->GetName(), Class.SpawnIdx, Data.MemorySize);

			Row += LINE_TERMINATOR;
			CSVFile->Serialize( TCHAR_TO_ANSI( *Row ), Row.Len() );
		}
		// Close and delete archive.
		CSVFile->Close();
		delete CSVFile;
	}
}

void UAnalyzeReferencedObjectCommandlet::WriteOutAllAvailableData(const FString& LogDirectory)
{
	TMap<UClass *, TArray<FReferenceData>> OutputData;

	CollectAllRowData(OutputData);

	// now output to csv - top row first
	FString		CSVFilename		= TEXT("");
	// Create CSV folder in case it doesn't exist yet.
	GFileManager->MakeDirectory( *LogDirectory );

	// CSV: Human-readable spreadsheet format.
	CSVFilename	= FString::Printf(TEXT("%sMemSize-%s-%i.csv"), *LogDirectory, GGameName, GetChangeListNumberForPerfTesting() );
	WriteOutAllocationData(CSVFilename);

	CSVFilename	= FString::Printf(TEXT("%sRefCount-%s-%i.csv"), *LogDirectory, GGameName, GetChangeListNumberForPerfTesting() );
	WriteOutReferenceCount(CSVFilename, OutputData);

	CSVFilename	= FString::Printf(TEXT("%sResSize-%s-%i.csv"), *LogDirectory, GGameName, GetChangeListNumberForPerfTesting() );
	WriteOutResourceSize(CSVFilename, OutputData);
}

void UAnalyzeReferencedObjectCommandlet::StaticInitialize()
{
	ShowErrorCount = FALSE;
}

// construct the list
TArray<UObject *> ObjectList;
FObjectList::FObjectList()
{
}

FObjectList::FObjectList(UClass * ClassType)
{
	if (ClassType != NULL)
	{
		for( FObjectIterator It(ClassType); It; ++It )
		{
			if (It->IsPendingKill() == FALSE)
			{
				ObjectList.AddItem(*It);
			}
		}
	}
}

FObjectList FObjectList::operator-(const FObjectList& OtherObjectList)
{
	FObjectList Result;

	// Since it's too slow to go through everybody
	// Starting from OtherObjectList
	// This assumption only works when there is no GCed, and only new objects are added at the end
	for (INT Id=OtherObjectList.ObjectList.Num(); Id<ObjectList.Num(); ++Id)
	{
		if (OtherObjectList.ObjectList.ContainsItem(ObjectList(Id))==FALSE)
		{
			Result.ObjectList.AddItem(ObjectList(Id));
		}
	}

	return Result;
}

INT FObjectList::GetTotalSize()
{
	INT TotalSize=0;

	for (INT Id=0; Id<ObjectList.Num(); ++Id)
	{
		TotalSize += GetObjectSize(ObjectList(Id));
	}

	return TotalSize;
}

INT FObjectList::GetObjectSize(UObject * Object)
{
	INT ResourceSize =0;
	if (Object)
	{
		ResourceSize = Object->GetResourceSize();

		if (ResourceSize==0)
		{
			FArchiveCountMem CountBytesSize( Object );
			// if we want accurate results, count the max (actual amount allocated)
			ResourceSize = CountBytesSize.GetNum();
		}
	}

	return ResourceSize;
}

void FObjectList::RemoveObject(UObject * Object)
{
	ObjectList.RemoveItem(Object);
}