 /*=============================================================================
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "EnginePrivate.h"
#include "EngineUtils.h"

#include "UnFile.h"
#include "DiagnosticTable.h"

#if !FINAL_RELEASE || FINAL_RELEASE_DEBUGCONSOLE
FContentComparisonHelper::FContentComparisonHelper()
{
	FConfigSection* RefTypes = GConfig->GetSectionPrivate(TEXT("ContentComparisonReferenceTypes"), FALSE, TRUE, GEngineIni);
	if (RefTypes != NULL)
	{
		for( FConfigSectionMap::TIterator It(*RefTypes); It; ++It )
		{
			const FString RefType = It.Value();
			ReferenceClassesOfInterest.Set(RefType, TRUE);
			debugf(TEXT("Adding class of interest: %s"), *RefType);
		}
	}
}

FContentComparisonHelper::~FContentComparisonHelper()
{
}

UBOOL FContentComparisonHelper::CompareClasses(const FString& InBaseClassName, INT InRecursionDepth)
{
	TArray<FString> EmptyIgnoreList;
	return CompareClasses(InBaseClassName, EmptyIgnoreList, InRecursionDepth);
}

UBOOL FContentComparisonHelper::CompareClasses(const FString& InBaseClassName, const TArray<FString>& InBaseClassesToIgnore, INT InRecursionDepth)
{
	TMap<FString,TArray<FContentComparisonAssetInfo> > ClassToAssetsMap;

	UClass* TheClass = (UClass*)UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *InBaseClassName, TRUE);
	if (TheClass != NULL)
	{
		TArray<UClass*> IgnoreBaseClasses;
		for (INT IgnoreIdx = 0; IgnoreIdx < InBaseClassesToIgnore.Num(); IgnoreIdx++)
		{
			UClass* IgnoreClass = (UClass*)UObject::StaticFindObject(UClass::StaticClass(), ANY_PACKAGE, *(InBaseClassesToIgnore(IgnoreIdx)), TRUE);
			if (IgnoreClass != NULL)
			{
				IgnoreBaseClasses.AddItem(IgnoreClass);
			}
		}

		for( TObjectIterator<UClass> It; It; ++It )
		{
			UClass* TheAssetClass = *It;
			if ((TheAssetClass->IsChildOf(TheClass) == TRUE) && 
				(TheAssetClass->HasAnyClassFlags(CLASS_Abstract) == FALSE))
			{
				UBOOL bSkipIt = FALSE;
				for (INT CheckIdx = 0; CheckIdx < IgnoreBaseClasses.Num(); CheckIdx++)
				{
					UClass* CheckClass = IgnoreBaseClasses(CheckIdx);
					if (TheAssetClass->IsChildOf(CheckClass) == TRUE)
					{
// 						warnf(NAME_Log, TEXT("Skipping class derived from other content comparison class..."));
// 						warnf(NAME_Log, TEXT("\t%s derived from %s"), *TheAssetClass->GetFullName(), *CheckClass->GetFullName());
						bSkipIt = TRUE;
					}
				}
				if (bSkipIt == FALSE)
				{
					TArray<FContentComparisonAssetInfo>* AssetList = ClassToAssetsMap.Find(TheAssetClass->GetFullName());
					if (AssetList == NULL)
					{
						TArray<FContentComparisonAssetInfo> TempAssetList;
						ClassToAssetsMap.Set(TheAssetClass->GetFullName(), TempAssetList);
						AssetList = ClassToAssetsMap.Find(TheAssetClass->GetFullName());
					}
					check(AssetList);

					// Serialize object with reference collector.
					const INT MaxRecursionDepth = 6;
					InRecursionDepth = Clamp<INT>(InRecursionDepth, 1, MaxRecursionDepth);
					TMap<UObject*,UBOOL> RecursivelyGatheredReferences;
					RecursiveObjectCollection(TheAssetClass, 0, InRecursionDepth, RecursivelyGatheredReferences);

					// Add them to the asset list
					for (TMap<UObject*,UBOOL>::TIterator GatheredIt(RecursivelyGatheredReferences); GatheredIt; ++GatheredIt)
					{
						UObject* Object = GatheredIt.Key();
						if (Object)
						{
							UBOOL bAddIt = TRUE;
							if (ReferenceClassesOfInterest.Num() > 0)
							{
								FString CheckClassName = Object->GetClass()->GetName();
								if (ReferenceClassesOfInterest.Find(CheckClassName) == NULL)
								{
									bAddIt = FALSE;
								}
							}
							if (bAddIt == TRUE)
							{
								INT NewIndex = AssetList->AddZeroed();
								FContentComparisonAssetInfo& Info = (*AssetList)(NewIndex);
								Info.AssetName = Object->GetFullName();
								Info.ResourceSize = Object->GetResourceSize();
							}
						}
					}
				}
			}
		}
	}
	else
	{
		warnf(NAME_Warning, TEXT("Failed to find class: %s"), *InBaseClassName);
		return FALSE;
	}

#if 0
	// Log them all out
	debugf(TEXT("CompareClasses on %s"), *InBaseClassName);
	for (TMap<FString,TArray<FContentComparisonAssetInfo>>::TIterator It(ClassToAssetsMap); It; ++It)
	{
		FString ClassName = It.Key();
		TArray<FContentComparisonAssetInfo>& AssetList = It.Value();

		debugf(TEXT("\t%s"), *ClassName);
		for (INT AssetIdx = 0; AssetIdx < AssetList.Num(); AssetIdx++)
		{
			FContentComparisonAssetInfo& Info = AssetList(AssetIdx);

			debugf(TEXT("\t\t%s,%f"), *(Info.AssetName), Info.ResourceSize/1024.0f);
		}
	}
#endif

	// Write out a CSV file
	FString CurrentTime = appSystemTimeString();
	FString Platform = appGetPlatformString();

	FString BaseCSVName = (
		FString(TEXT("ContentComparison\\")) + 
		FString::Printf(TEXT("ContentCompare-%d\\"), GBuiltFromChangeList) +
		FString::Printf(TEXT("%s"), *InBaseClassName)
		);
#if CONSOLE
	// Handle file name length on consoles... 
	FString EditedBaseClassName = InBaseClassName;
	FString TimeString = *appSystemTimeString();
	FString CheckLenName = FString::Printf(TEXT("%s-%s.csv"),*InBaseClassName,*TimeString);
	if (CheckLenName.Len() > 42)
	{
		while (CheckLenName.Len() > 42)
		{
			EditedBaseClassName = EditedBaseClassName.Right(EditedBaseClassName.Len() - 1);
			CheckLenName = FString::Printf(TEXT("%s-%s.csv"),*EditedBaseClassName,*TimeString);
		}
		BaseCSVName = (
			FString(TEXT("ContentComparison\\")) + 
			FString::Printf(TEXT("ContentCompare-%d\\"), GBuiltFromChangeList) +
			FString::Printf(TEXT("%s"), *EditedBaseClassName)
			);
	}
#endif

	FDiagnosticTableViewer* AssetTable = new FDiagnosticTableViewer(
			*FDiagnosticTableViewer::GetUniqueTemporaryFilePath(*BaseCSVName), TRUE);
	if ((AssetTable != NULL) && (AssetTable->OutputStreamIsValid() == TRUE))
	{
		// Fill in the header row
		AssetTable->AddColumn(TEXT("Class"));
		AssetTable->AddColumn(TEXT("Asset"));
		AssetTable->AddColumn(TEXT("ResourceSize(kB)"));
		AssetTable->CycleRow();

		// Fill it in
		for (TMap<FString,TArray<FContentComparisonAssetInfo> >::TIterator It(ClassToAssetsMap); It; ++It)
		{
			FString ClassName = It.Key();
			TArray<FContentComparisonAssetInfo>& AssetList = It.Value();

			AssetTable->AddColumn(*ClassName);
			AssetTable->CycleRow();
			for (INT AssetIdx = 0; AssetIdx < AssetList.Num(); AssetIdx++)
			{
				FContentComparisonAssetInfo& Info = AssetList(AssetIdx);

				AssetTable->AddColumn(TEXT(""));
				AssetTable->AddColumn(*(Info.AssetName));
				AssetTable->AddColumn(TEXT("%f"), Info.ResourceSize/1024.0f);
				AssetTable->CycleRow();
			}
		}
	}
	else if (AssetTable != NULL)
	{
		// Created the class, but it failed to open the output stream.
		warnf(NAME_Warning, TEXT("Failed to open output stream in asset table!"));
	}

	if (AssetTable != NULL)
	{
		// Close it and kill it
		AssetTable->Close();
		delete AssetTable;
	}

	return TRUE;
}

void FContentComparisonHelper::RecursiveObjectCollection(UObject* InStartObject, INT InCurrDepth, INT InMaxDepth, TMap<UObject*,UBOOL>& OutCollectedReferences)
{
	// Serialize object with reference collector.
	TArray<UObject*> LocalCollectedReferences;
	FArchiveObjectReferenceCollector ObjectReferenceCollector(&LocalCollectedReferences, NULL, FALSE, TRUE, TRUE, TRUE);
	InStartObject->Serialize(ObjectReferenceCollector);
	if (InCurrDepth < InMaxDepth)
	{
		InCurrDepth++;
		for (INT ObjRefIdx = 0; ObjRefIdx < LocalCollectedReferences.Num(); ObjRefIdx++)
		{
			UObject* InnerObject = LocalCollectedReferences(ObjRefIdx);
			if ((InnerObject != NULL) &&
				(InnerObject->IsA(UFunction::StaticClass()) == FALSE) &&
				(InnerObject->IsA(UPackage::StaticClass()) == FALSE)
				)
			{
				OutCollectedReferences.Set(InnerObject, TRUE);
				RecursiveObjectCollection(InnerObject, InCurrDepth, InMaxDepth, OutCollectedReferences);
			}
		}
		InCurrDepth--;
	}
}
#endif
