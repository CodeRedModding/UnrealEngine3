/*=============================================================================
	AnalyzeCookedContentCommandlet.cpp: Commandlet for analyzing and displaying
	statistics for cooked packages.

	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"

const FString GCookedFolder(TEXT("CookedPC"));

TLookupMap<FString> NameIndexMap;

IMPLEMENT_CLASS(UAnalyzeCookedContentCommandlet);

/** Standard Constructor */
FObjectResourceStat::FObjectResourceStat( FName InClassName, const FString& InResourceName, INT InResourceSize )
: ClassName(InClassName), ResourceSize(InResourceSize)
{
	ResourceNameIndex = NameIndexMap.AddItem(*InResourceName);
}

/**
 * Creates a new resource stat using the specified parameters.
 *
 * @param	ResourceClassName	the name of the class for the resource
 * @param	ResourcePathName	the complete path name for the resource
 * @param	ResourceSize		the size on disk for the resource
 *
 * @return	a pointer to the FObjectResourceStat that was added
 */
FObjectResourceStat* FPackageResourceStat::AddResourceStat( FName ResourceClassName, const FString& ResourcePathName, INT ResourceSize )
{
	FObjectResourceStat* Result = &PackageResources.Add(ResourceClassName, FObjectResourceStat(ResourceClassName, ResourcePathName, ResourceSize));
	return Result;
}

INT UAnalyzeCookedContentCommandlet::Main(const FString& Params)
{
	// make sure that we have a valid output type before doing all the work.
	FResourceStatReporter* Reporter = CreateReporter(Params);
	if ( Reporter == NULL )
	{
		warnf(NAME_Error, TEXT("Invalid report type specified!"));
		return 1;
	}

	// build the list of packages to work on
	Init();

	// search through the export map of each package and generate the arrays of resource stats
	AssembleResourceStats();

	Reporter->CreateReport(PackageResourceStats);
	delete Reporter;
	Reporter = NULL;

	// for now, we'll run the asset duplication report from here
	{
		FResourceStatReporter_AssetDuplication DuplicationReporter;
		DuplicationReporter.CreateReport(PackageResourceStats);
	}

	return 0;
}

/**
 * Builds the list of package names to load
 */
void UAnalyzeCookedContentCommandlet::Init()
{
	FFilename CookedPackageWildcard = (appGameDir() + GCookedFolder) * TEXT("*.*");

	// retrieve the list of file names for the packages in the cooked directory
	GFileManager->FindFiles((TArray<FString>&)CookedPackageNames, *CookedPackageWildcard, TRUE, FALSE);

	TArray<FString> NativeScriptPackageNames;
	appGetScriptPackageNames(NativeScriptPackageNames, SPT_Native, FALSE);

	for ( INT FileIndex = CookedPackageNames.Num() - 1; FileIndex >= 0; FileIndex-- )
	{
		FFilename& PackageName = CookedPackageNames(FileIndex);

		// don't attempt to load any native script packages
		if ( NativeScriptPackageNames.ContainsItem(*PackageName.GetBaseFilename()) )
		{
			CookedPackageNames.Remove(FileIndex);
		}
		else
		{
			// re-add the path information so that GetPackageLinker finds the correct version of the file.
			CookedPackageNames(FileIndex) = CookedPackageWildcard.GetPath() * PackageName;
		}
	}
}

/**
 * Loads each package and adds stats for its exports to the main list of stats
 */
void UAnalyzeCookedContentCommandlet::AssembleResourceStats()
{
	// Save time by only collecting garbage after every 10th package
	INT GCIndex=0;

	// Iterate over all found files.
	for( INT FileIndex = 0; FileIndex < CookedPackageNames.Num(); FileIndex++ )
	{
		FFilename& PackageName = CookedPackageNames(FileIndex);

		// Skip any packages that don't exist or are 0 bytes
		INT FileSize = GFileManager->FileSize( *PackageName );
		if( FileSize <= 0 )
		{
			warnf(NAME_Console, TEXT("Skipping %s because it is %i bytes..."), *PackageName, FileSize);
			continue;
		}

		// reset the loaders for the packages we want to load so that we don't find the wrong version of the file
		// (otherwise, attempting to grab the linker for e.g. Engine.xxx will always return the linker for Engine.u instead)
		{
			const FString& ExistingPackageName = FPackageFileCache::PackageFromPath(*PackageName);
			UPackage* ExistingPackage = FindObject<UPackage>(NULL, *ExistingPackageName, TRUE);
			if ( ExistingPackage != NULL )
			{
				UObject::ResetLoaders(ExistingPackage);
			}
		}

		warnf(NAME_Console, TEXT("Loading %s..."), *PackageName);

		UObject::BeginLoad();
		ULinkerLoad* Linker = UObject::GetPackageLinker( NULL, *PackageName, LOAD_NoVerify, NULL, NULL );
		UObject::EndLoad();

		if( Linker )
		{
			check(Linker->LinkerRoot);

			// for now, skip over packages which aren't marked as cooked
			if ( (Linker->Summary.PackageFlags&PKG_Cooked) != 0 )
			{
				// create a package stat for this package
				FPackageResourceStat* CurrentPackageStats = new(PackageResourceStats) FPackageResourceStat(Linker->LinkerRoot->GetFName());

				CurrentPackageStats->PackageFilename = FName(*PackageName.GetBaseFilename());

				// for each export in the package,
				for ( INT ExportIndex = 0; ExportIndex < Linker->ExportMap.Num(); ExportIndex++ )
				{
					FObjectExport& Export = Linker->ExportMap(ExportIndex);

					// get the name of this object's class
					FName ClassName = Linker->GetExportClassName(ExportIndex);

					// get the full path name for this resource
					FString ResourceName = Linker->GetExportPathName(ExportIndex, NULL, TRUE);

					// add this resource to the list of resources stats for this package
					CurrentPackageStats->AddResourceStat(ClassName, ResourceName, Export.SerialSize);
				}
			}
			else
			{
				warnf(NAME_Warning, TEXT("Package '%s' not marked with PKG_Cooked flag!"), *Linker->LinkerRoot->GetName());
			}
		}

		if ( (++GCIndex % 50) == 0 )
		{
			// reset all non-asynch loaders
			ResetLoaders(NULL);

			// release all asynch loaders
			GIOManager->Flush();
		}

		if( (GCIndex % 10) == 0 )
		{
			UObject::CollectGarbage(RF_Native);
		}
	}

	PackageResourceStats.Shrink();
}

/**
 * Determines which report type is desired based on the command-line parameters specified and creates the appropriate reporter.
 *
 * @param	Params	the command-line parameters passed to Main
 *
 * @return	a pointer to a reporter which generates output in the desired format, or NULL if no valid report type was specified.
 */
FResourceStatReporter* UAnalyzeCookedContentCommandlet::CreateReporter( const FString& Params )
{
	TArray<FString> Tokens;
	TArray<FString> Switches;

	// Parse command line.
	const TCHAR* Parms = *Params;
	ParseCommandLine(Parms, Tokens, Switches);

	FResourceStatReporter* Result = NULL;

	// for now, we only have one report type
	//@todo - implement other report types
	Result = new FResourceStatReporter_TotalMemoryPerAsset;


	return Result;
}


IMPLEMENT_COMPARE_CONSTREF(FResourceDiskSize,TotalMemoryPerAsset,{ return (B.TotalSize > A.TotalSize) ? 1 : -1; })

/**
 * Creates a report using the specified stats.  The type of report created depends on the reporter type.
 *
 * @param	ResourceStats	the list of resource stats to create a report for.
 *
 * @return	TRUE if the report was created successfully; FALSE otherwise.
 */
UBOOL FResourceStatReporter_TotalMemoryPerAsset::CreateReport( const TArray<FPackageResourceStat>& ResourceStats )
{
	TMap<FName,FResourceDiskSize> ClassSizeTotals;

	FString ResourceNameHeader = TEXT("Resource Type");
	INT ClassNameIndent= ResourceNameHeader.Len();
	for ( INT ResourceIndex = 0; ResourceIndex < ResourceStats.Num(); ResourceIndex++ )
	{
		const FPackageResourceStat& PackageStats = ResourceStats(ResourceIndex);
		for ( FClassResourceMap::TConstIterator It(PackageStats.PackageResources); It; ++It )
		{
			const FName& ClassName = It.Key();
			const FObjectResourceStat& Stat = It.Value();

			FResourceDiskSize* pResourceSize = ClassSizeTotals.Find(ClassName);
			if ( pResourceSize == NULL )
			{
				pResourceSize = &ClassSizeTotals.Set(ClassName,FResourceDiskSize(ClassName));
			}

			pResourceSize->TotalSize += Stat.ResourceSize;
			ClassNameIndent = Max(ClassNameIndent, pResourceSize->ClassName.Len());
		}
	}

	TArray<FResourceDiskSize> SortedClassSizeTotals;

	// now copy the resource disk sizes to an array
	for ( TMap<FName,FResourceDiskSize>::TIterator It(ClassSizeTotals); It; ++It )
	{
		SortedClassSizeTotals.AddItem(It.Value());
	}

	// sort the list of resource sizes
	Sort<USE_COMPARE_CONSTREF(FResourceDiskSize,TotalMemoryPerAsset)>( &SortedClassSizeTotals(0), SortedClassSizeTotals.Num() );

	INT IndexIndent = appItoa(SortedClassSizeTotals.Num()).Len();

	// now print out the results
	warnf(NAME_Console, TEXT("%*s   Total Size"), IndexIndent + ClassNameIndent + 2, *ResourceNameHeader);
	const INT MaxDesiredResults = ParseParam(appCmdLine(), TEXT("FULL")) ? SortedClassSizeTotals.Num() : 100;
	for ( INT ClassIndex = 0; ClassIndex < MaxDesiredResults; ClassIndex++ )
	{
		FResourceDiskSize& ClassSize = SortedClassSizeTotals(ClassIndex);

		warnf(NAME_Console, TEXT("%*i) %*s:  %lli %s"), IndexIndent, ClassIndex, ClassNameIndent, *ClassSize.ClassName, 
			ClassSize.TotalSize > 1024 ? (QWORD)((DOUBLE)ClassSize.TotalSize / 1024.f) : ClassSize.TotalSize,
			ClassSize.TotalSize > 1024 ? TEXT("KB") : TEXT("bytes"));
	}

	return SortedClassSizeTotals.Num() > 0;
}

/**
 * Holds information about a resource that has been duplicated into multiple packages
 */
struct FDuplicatedResource
{
	/** the complete path name for this resource */
	INT ResourceNameIndex;

	/** the name of the class for this resource */
	FName ResourceClassName;

	/** minimum size used by this resource across all packages */
	INT ResourceMinSize;

	/** maximum size used by this resource across all packages */
	INT ResourceMaxSize;

	/**
	 * The cumulative size of all instances of this resource type
	 */
	QWORD ResourceCumulativeSize;

	/** the names of the packages where this resource is duplicated */
	TArray<FName> ResourceContainers;

	FDuplicatedResource( const FObjectResourceStat& InResource, const TArray<FName>& Containers )
	: ResourceNameIndex(InResource.ResourceNameIndex), ResourceClassName(InResource.ClassName)
	, ResourceMinSize(InResource.ResourceSize), ResourceMaxSize(InResource.ResourceSize)
	, ResourceCumulativeSize(InResource.ResourceSize), ResourceContainers(Containers)
	{
	}

	/**
	 * Returns the full name of the resource (class + path name)
	 */
	const FString GetResourceFullName()
	{
		return (ResourceClassName.ToString() + TEXT(" ") + NameIndexMap(ResourceNameIndex));
	}

	/**
	 * Returns the path name for this resource
	 */
	const FString& GetResourcePathName() const
	{
		return NameIndexMap(ResourceNameIndex);
	}
};

IMPLEMENT_COMPARE_CONSTREF(FDuplicatedResource,AssetDuplication_LargestSingleAsset,
{
	INT Result = B.ResourceMaxSize - A.ResourceMaxSize;
	if ( Result == 0 )
	{
		Result = B.ResourceContainers.Num() - A.ResourceContainers.Num();
		if ( Result == 0 )
		{
			const FString& ResourceNameA = NameIndexMap(A.ResourceNameIndex);
			const FString& ResourceNameB = NameIndexMap(B.ResourceNameIndex);
			Result = appStricmp(*ResourceNameA, *ResourceNameB);
		}
	}
	return Result;
})

IMPLEMENT_COMPARE_CONSTREF(FDuplicatedResource,AssetDuplication_LargestCumulativeAsset,
{
	QWORD CumulativeSizeA = A.ResourceCumulativeSize;
	QWORD CumulativeSizeB = B.ResourceCumulativeSize;
	INT Result = CumulativeSizeB - CumulativeSizeA;
	if ( Result == 0 )
	{
		Result = B.ResourceMaxSize - A.ResourceMaxSize;
		if ( Result == 0 )
		{
			Result = B.ResourceContainers.Num() - A.ResourceContainers.Num();
			if ( Result == 0 )
			{
				const FString& ResourceNameA = NameIndexMap(A.ResourceNameIndex);
				const FString& ResourceNameB = NameIndexMap(B.ResourceNameIndex);
				Result = appStricmp(*ResourceNameA, *ResourceNameB);
			}
		}
	}
	return Result;
})

/**
 * Creates a report using the specified stats.  The type of report created depends on the reporter type.
 *
 * @param	ResourceStats	the list of resource stats to create a report for.
 *
 * @return	TRUE if the report was created successfully; FALSE otherwise.
 */
UBOOL FResourceStatReporter_AssetDuplication::CreateReport( const TArray<FPackageResourceStat>& ResourceStats )
{
	warnf(NAME_Console, TEXT("\r\nPreparing duplicate asset report for %i resources"), NameIndexMap.Num());

	// this map tracks the number (and names) of packages containing each unique resource
	TMultiMap<INT,FName> ResourceContainerMap;

	// iterate through all package stats
	for ( INT ResourceIndex = 0; ResourceIndex < ResourceStats.Num(); ResourceIndex++ )
	{
		const FPackageResourceStat& PackageStats = ResourceStats(ResourceIndex);

		// for each type in the package
		for ( FClassResourceMap::TConstIterator It(PackageStats.PackageResources); It; ++It )
		{
			const FObjectResourceStat& Stat = It.Value();
			ResourceContainerMap.Add(Stat.ResourceNameIndex, PackageStats.PackageName);
		}
	}

	INT PathNameIndent=0;

	// this map tracks the duplicate resource data
	TMap<INT,FDuplicatedResource> DuplicatedResources;

	// get the list of unique names (well, name indexes)
	warnf(NAME_Console, TEXT("Collating results..."));
	for ( INT ObjectNameIndex = 0; ObjectNameIndex < NameIndexMap.Num(); ObjectNameIndex++ )
	{
		if ( (ObjectNameIndex % 100000) == 0 )
		{
			warnf(NAME_Console, TEXT("Processed %i of %i unique resources."), ObjectNameIndex, NameIndexMap.Num());
		}
		TArray<FName> ContainerPackages;
		ResourceContainerMap.MultiFind(ObjectNameIndex, ContainerPackages);

		if ( ContainerPackages.Num() > 1 )
		{
			// find a resource stat for this resource
			for ( INT ResourceIndex = 0; ResourceIndex < ResourceStats.Num(); ResourceIndex++ )
			{
				const FPackageResourceStat& PackageStats = ResourceStats(ResourceIndex);
				if ( ContainerPackages.ContainsItem(PackageStats.PackageName) )
				{
					// for each type in the package
					for ( FClassResourceMap::TConstIterator PackageIt(PackageStats.PackageResources); PackageIt; ++PackageIt )
					{
						const FObjectResourceStat& Stat = PackageIt.Value();
						if ( Stat.ResourceNameIndex == ObjectNameIndex )
						{
							FDuplicatedResource* DuplicateResource = DuplicatedResources.Find(Stat.ResourceNameIndex);
							if ( DuplicateResource != NULL )
							{
								DuplicateResource->ResourceMinSize = Min(DuplicateResource->ResourceMinSize, Stat.ResourceSize);
								DuplicateResource->ResourceMaxSize = Max(DuplicateResource->ResourceMaxSize, Stat.ResourceSize);
								DuplicateResource->ResourceCumulativeSize += Stat.ResourceSize;
							}
							else
							{
								DuplicateResource = &DuplicatedResources.Set(Stat.ResourceNameIndex, FDuplicatedResource(Stat, ContainerPackages));
							}
						}
					}
				}
			}
		}
	}

	// print out the report of largest single asset
	DuplicatedResources.ValueSort<COMPARE_CONSTREF_CLASS(FDuplicatedResource,AssetDuplication_LargestCumulativeAsset)>();

	warnf(NAME_Console, TEXT("Summary:"));
	INT ResultCount = 0;
	const INT MaxDesiredResults = ParseParam(appCmdLine(), TEXT("FULL")) ? DuplicatedResources.Num() : 100;
	for ( TMap<INT,FDuplicatedResource>::TIterator It(DuplicatedResources); It && ResultCount++ < MaxDesiredResults; ++It )
	{
		FDuplicatedResource& DuplicateResource = It.Value();
		INT MinResourceSize = DuplicateResource.ResourceMinSize > 1024 ? (INT)((DOUBLE)DuplicateResource.ResourceMinSize / 1024.f) : DuplicateResource.ResourceMinSize;
		INT MaxResourceSize = DuplicateResource.ResourceMaxSize > 1024 ? (INT)((DOUBLE)DuplicateResource.ResourceMaxSize / 1024.f) : DuplicateResource.ResourceMaxSize;
		QWORD CumulativeTotal = DuplicateResource.ResourceCumulativeSize > 1024 ? (QWORD)((DOUBLE)DuplicateResource.ResourceCumulativeSize / 1024.f) : DuplicateResource.ResourceCumulativeSize;

		warnf(NAME_Console, TEXT("Min/Max/Total: %i%s / %i%s / %lli%s across %i packages for %s"), 
			MinResourceSize, DuplicateResource.ResourceMinSize > 1024 ? TEXT("KB") : TEXT("bytes"),
			MaxResourceSize, DuplicateResource.ResourceMaxSize > 1024 ? TEXT("KB") : TEXT("bytes"),
			CumulativeTotal, DuplicateResource.ResourceCumulativeSize > 1024 ? TEXT("KB") : TEXT("bytes"),
			DuplicateResource.ResourceContainers.Num(),
			*DuplicateResource.GetResourceFullName());
	}

	// print out the report for large cumulative asset
	DuplicatedResources.ValueSort<COMPARE_CONSTREF_CLASS(FDuplicatedResource,AssetDuplication_LargestCumulativeAsset)>();

	warnf(NAME_Console, TEXT("\r\nDetails:"));
	ResultCount = 0;
	for ( TMap<INT,FDuplicatedResource>::TIterator It(DuplicatedResources); It && ResultCount++ < MaxDesiredResults; ++It )
	{
		FDuplicatedResource& DuplicateResource = It.Value();
		if ( ResultCount > 1 )
		{
			warnf(NAME_Console, TEXT(""));
		}

		QWORD CumulativeTotal = DuplicateResource.ResourceCumulativeSize;
		warnf(NAME_Console, TEXT("%s %lli%s (total in %i packages)"), *DuplicateResource.GetResourceFullName(),
			CumulativeTotal > 1024 ? (QWORD)((DOUBLE)CumulativeTotal / 1024.f) : CumulativeTotal,
			CumulativeTotal > 1024 ? TEXT("KB") : TEXT("bytes"),
			DuplicateResource.ResourceContainers.Num());

		// iterate backwards since this array is populated using MultiFind, which returns results in reverse order.
		for ( INT ContainerIndex = DuplicateResource.ResourceContainers.Num() - 1; ContainerIndex >= 0; ContainerIndex-- )
		{
			warnf(NAME_Console, TEXT("    %s"), *DuplicateResource.ResourceContainers(ContainerIndex).ToString());
		}
	}

	return TRUE;
}



// EOF



