//! @file SubstanceAirEdCreateTestSceneCommandlet.cpp
//! @brief Substance Air commandlets implementation
//! @contact antoine.gonzalez@allegorithmic.com
//! @copyright Allegorithmic. All rights reserved.
//!
//! @note The commandlet needs an empty.umap to run
//! @note The result map is not complete, run the resave commandlet for lights and paths
//!

#include <UnrealEd.h>
#include <Factories.h>
#include <FileHelpers.h>
#include <LaunchEngineLoop.h>
#include <Engine.h>

#include "SubstanceAirEdCommandlet.h"

#include "SubstanceAirTypedefs.h"
#include "SubstanceAirHelpers.h"
#include "SubstanceAirPackage.h"

IMPLEMENT_CLASS(UCreateTestMapCommandlet);


UBOOL CommandletFindPackage(const FString& PackageName, FString& OutPath)
{
	UBOOL bResult = FALSE;

	if (GPackageFileCache->FindPackageFile(*PackageName, NULL, OutPath))
	{
		bResult = TRUE;
	}

	return bResult;
}


void CreateBlock(INT x, INT y, UBOOL bAddLight)
{
	const INT Density = 512;
	GUnrealEd->Exec(*FString::Printf(TEXT("BRUSH MOVETO %d, %d"), x * Density, y * Density ));
	GUnrealEd->Exec(TEXT("BRUSH ADD"));
	GUnrealEd->Exec(TEXT("BRUSH ADD"));
	GUnrealEd->Exec(TEXT("BRUSH RESET"));

	if (bAddLight)
	{
		// find the light actor factory
		static UActorFactory* LightFactory = GEditor->ActorFactories(0);
		TArray<UActorFactory*>::TIterator It(GEditor->ActorFactories);
		while (Cast<UActorFactoryLight>(LightFactory) == NULL && It)
		{
			LightFactory = *It;
			++It;
		}
		check(LightFactory);

		// use it
		const FVector Location(x * Density, y * Density, Density);
		const FRotator Rotation(0.0f, 0.0f, 0.0f);

		AActor* Light = LightFactory->CreateActor( &Location, &Rotation, NULL ); 
	}
}


void CreateDefaultContent() 
{
	// make the default brush bigger
	const INT CubeSize = 512;
	UCubeBuilder* CubeBuilder = ConstructObject<UCubeBuilder>( UCubeBuilder::StaticClass() );
	CubeBuilder->X = CubeSize;
	CubeBuilder->Y = CubeSize;
	CubeBuilder->Z = CubeSize;
	CubeBuilder->eventBuild();

	// create the default block
	CreateBlock(0, -1, FALSE);

	// add starting point
	UActorFactory* PlayerStartFactory = 0;
	TArray<UActorFactory*>::TIterator It(GEditor->ActorFactories);
	while (Cast<UActorFactoryPlayerStart>(PlayerStartFactory) == NULL && It)
	{
		PlayerStartFactory  = *It;
		++It;
	}
	check(PlayerStartFactory);

	const FLOAT zPlayerStart = 512.0f;
	const FVector Location(0, 0, zPlayerStart);
	const FRotator Rotation(0.0f, 0.0f, 0.0f);

	AActor* Light = PlayerStartFactory->CreateActor( &Location, &Rotation, NULL );
}


UWorld* LoadMap(const FString& LevelName)
{
	// See whether filename was found in cache.		
	FFilename Filename;	
	if( GPackageFileCache->FindPackageFile( *LevelName, NULL, Filename ) )
	{
		// Load the map file.
		UPackage* Package = UObject::LoadPackage( NULL, *Filename, 0 );
		if( Package )
		{
			// Find the world object inside the map file.
			UWorld* World = FindObject<UWorld>( Package, TEXT("TheWorld") );
			if( World )
			{
				return World;
			}
		}
	}
	appErrorf(TEXT("Missing or invalid default empty level."));
	return NULL;
}


void CreateTestScene(UPackage* LoadedPackage)
{
	UWorld* World = LoadMap(TEXT("empty.udk"));
	check(World);

	// Set loaded world as global world object and add to root set so the
	// lighting rebuild code doesn't garbage collect it.
	GWorld = World;
	GWorld->AddToRoot();
	GWorld->Init();

	CreateDefaultContent();

	// UMaterialInterface if parent of Material and MIC
	TArray< UMaterialInterface* > Materials;

	// search for materials contained in the package 
	for (FObjectIterator It; It; ++It)
	{
		if (It->IsIn(LoadedPackage))
		{
			UMaterialInterface* Material = Cast<UMaterialInterface>(*It);
			if (Material)
			{
				Materials.AddItem(Material);
			}	
		}
	}

	// apply each of those material on a bsp block
	// placed on a square grid
	const INT GridRes = INT(appSqrt(Materials.Num()) + 0.5f);
	INT GridIndex = 0;

	for (TArray< UMaterialInterface* >::TIterator It(Materials); It; ++It)
	{
		// select the material
		USelection* EditorSelection = GEditor->GetSelectedObjects();
		EditorSelection->BeginBatchSelectOperation();
		EditorSelection->Select(*It);

		// create the block
		CreateBlock(
			GridIndex%GridRes,
			GridIndex/GridRes,
			GridIndex%2 >= 1);

		++GridIndex;

		// deselect the material
		EditorSelection->Deselect(*It);
		EditorSelection->EndBatchSelectOperation();
	}
 
	GWorld->UpdateCullDistanceVolumes();

	// Iterate over all levels in the world and save them.
	for( INT LevelIndex=0; LevelIndex<GWorld->Levels.Num(); LevelIndex++ )
	{
		ULevel*			Level			= GWorld->Levels(LevelIndex);
		check(Level);					
		UWorld*			World			= CastChecked<UWorld>(Level->GetOuter());

		FString			LevelFilename	=
			FString::Printf(TEXT("%s\\Content\\Maps\\%s_test.udk"),
				*appGameDir(),
				*LoadedPackage->GetName());

		// Save sublevel.
		UObject::SavePackage(
			GWorld->GetOutermost(), GWorld, 0, *LevelFilename, GWarn );
	}

	// Remove from root again.
	GWorld->RemoveFromRoot();
}


void UCreateTestMapCommandlet::CreateCustomEngine()
{
	UClass* EngineClass = NULL;
	EngineClass = UObject::StaticLoadClass( 
		UUnrealEdEngine::StaticClass(),
		NULL, 
		TEXT("engine-ini:Engine.Engine.UnrealEdEngine"),
		NULL, 
		LOAD_None, 
		NULL );
	GEngine = GEditor = GUnrealEd = 
		ConstructObject<UUnrealEdEngine>(EngineClass);
	GEditor->Init();
}


INT UCreateTestMapCommandlet::Main(const FString& Params)
{
	// Command line parse
	TArray< FString > Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);
	GUnrealEd = (UUnrealEdEngine*)GEditor;
	TArray< FString > Packages = Tokens;
	UBOOL bReimportOnly = FALSE;

	// create a test scene for every package submitted
	for (TArray< FString >::TIterator ItPkg(Packages) ; ItPkg ; ++ItPkg)
	{
		FString FoundPackagePath;
		if (FALSE == CommandletFindPackage(*ItPkg, FoundPackagePath))
		{
			debugf(TEXT("Package not found %s"), *(*ItPkg));
			continue;
		}

		UPackage* LoadedPackage = LoadPackage(NULL, *FoundPackagePath, 0);
		CreateTestScene(LoadedPackage);
	}

    return 0; //success
}
