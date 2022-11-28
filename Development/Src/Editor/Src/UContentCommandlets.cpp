/*=============================================================================
	UCContentCommandlets.cpp: Various commmandlets.
	Copyright 2003-2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel

=============================================================================*/

#include "EditorPrivate.h"
#include "UnLinker.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"

/*-----------------------------------------------------------------------------
	ULoadPackage commandlet.
-----------------------------------------------------------------------------*/

void ULoadPackage::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT ULoadPackage::Main( const TCHAR* Parms )
{
	FString	PackageWildcard;

	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();

	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	while( ParseToken(Parms, PackageWildcard, 0) )
	{
		TArray<FString> FilesInPath;
		FString			PathPrefix;

		GFileManager->FindFiles( FilesInPath, *PackageWildcard, 1, 0 );

		if( !FilesInPath.Num() )
			continue;

		for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
		{
			const FString &Filename = FilesInPath(FileIndex);
			UObject* Package;

			warnf(NAME_Log, TEXT("Loading %s"), *Filename);

			try
			{
				Package = UObject::LoadPackage( NULL, *Filename, 0 );
			}
			catch( ... )
			{
				Package = NULL;
			}

			if( !Package )
			{
				warnf(NAME_Log, TEXT("Error loading %s!"), *Filename);
				continue;
			}

			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}
IMPLEMENT_CLASS(ULoadPackage)


/*-----------------------------------------------------------------------------
	UListExports commandlet.
-----------------------------------------------------------------------------*/

struct FExportInfo
{
	FName	Name;
	INT		Size;
};

IMPLEMENT_COMPARE_CONSTREF( FExportInfo, UContentCommandlets, { return B.Size - A.Size; } )

void UListExports::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT UListExports::Main( const TCHAR* Parms )
{
	FString	PackageWildcard;

	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();

	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	while( ParseToken(Parms, PackageWildcard, 0) )
	{
		TArray<FString> FilesInPath;
		FString			PathPrefix;

		GFileManager->FindFiles( FilesInPath, *PackageWildcard, 1, 0 );

		if( !FilesInPath.Num() )
			continue;

		for( INT FileIndex = 0; FileIndex < FilesInPath.Num(); FileIndex++ )
		{
			const FString &Filename = FilesInPath(FileIndex);

			ULinkerLoad* Linker = NULL;

			try
			{
				UObject::BeginLoad();
				Linker = UObject::GetPackageLinker( NULL, *Filename, LOAD_Throw, NULL, NULL );
				UObject::EndLoad();
			}
			catch( ... )
			{
				Linker = NULL;
			}

			if( Linker )
			{
				TArray<FExportInfo> Exports;

				for( INT i=0; i<Linker->ExportMap.Num(); i++ )
				{
					FObjectExport& Export = Linker->ExportMap(i);
					INT Index = Exports.Add(1);
					Exports(Index).Name = Export.ObjectName;
					Exports(Index).Size = Export.SerialSize;
				}
				
				if( Exports.Num() )
				{
					Sort<USE_COMPARE_CONSTREF(FExportInfo,UContentCommandlets)>( &Exports(0), Exports.Num() );
					for( INT i=0; i<Exports.Num(); i++ )
						warnf(TEXT("%i %s"),Exports(i).Size,*Exports(i).Name);
				}
			}

			UObject::CollectGarbage(RF_Native);
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UListExports)


/*-----------------------------------------------------------------------------
	UReimportTextures commandlet.
-----------------------------------------------------------------------------*/

void UReimportTextures::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;
}
INT UReimportTextures::Main( const TCHAR* Parms )
{
	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GLazyLoad					= 0;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	GEditor->InitEditor();

	// This is lame but it's so much easier to create a comprehensive list using "dir *.tga /B /S" than with GFileManager
	TArray<FString>	SourceTextures;
	FString			Dummy;
	appLoadFileToString( Dummy, TEXT("D:\\temp\\sourceart.txt") );
	Dummy.ParseIntoArray( LINE_TERMINATOR, &SourceTextures );
	if( (ARRAY_COUNT(LINE_TERMINATOR)-1) > 1 )
		for( INT i=1; i<SourceTextures.Num(); i++ )
			SourceTextures(i) = SourceTextures(i).Right( SourceTextures(i).Len() - (ARRAY_COUNT(LINE_TERMINATOR)-2) );

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FString &Filename = PackageList(PackageIndex);
		UObject* Package;

		if( appStricmp( *FFilename(Filename).GetExtension(), TEXT("upk")) == 0 )
		{
			Package = UObject::LoadPackage( NULL, *Filename, 0 );
		
			if( !Package )
			{
				warnf(NAME_Log, TEXT("Error loading %s!"), *Filename);
				continue;
			}
		}
		else
			continue;

		UTextureFactory* TextureFactory = ConstructObject<UTextureFactory>( UTextureFactory::StaticClass() );
		check( TextureFactory );

		for( TObjectIterator<UObject> It; It; ++It )
		{
			UTexture2D* Texture = Cast<UTexture2D>(*It);
			if( Texture && Texture->IsIn(Package) )
			{
				switch( Texture->Format )
				{
					case PF_A8R8G8B8:
					case PF_DXT1:
					case PF_DXT3:
					case PF_DXT5:
						Texture->SourceArt.Load();			
						if( !Texture->SourceArt.Num() )
						{
							warnf(TEXT("Texture [%s] is missing source art"), Texture->GetPathName() );

							// Find source art path.		
							INT TextureIndex;
							for( TextureIndex=0; TextureIndex<SourceTextures.Num(); TextureIndex++ )
							{
								FString MangledName;
								Texture->GetPathName().Split( TEXT("."), NULL, &MangledName, 1 );
								MangledName += TEXT(".tga");

								if( SourceTextures(TextureIndex).InStr( Package->GetName() ) == INDEX_NONE )
									continue;

								if( SourceTextures(TextureIndex).InStr( MangledName ) == INDEX_NONE )
									continue;

								break;
							}

							if( TextureIndex == SourceTextures.Num() )
								continue;

							warnf(TEXT("Reimporting"));

							// Mark texture as normal map if it has the characteristic normal map unpack range.
							if( (Texture->UnpackMin == FPlane( -1, -1, -1, 0 ) || Texture->UnpackMin == FPlane( -1, -1, -1, -1 )) && Texture->UnpackMax == FPlane( 1, 1, 1, 1 ) )
								Texture->CompressionSettings = TC_Normalmap;
		
							// Reimport temporary texture from source art.
							TArray<BYTE> Data;
							if( !appLoadFileToArray( Data, *SourceTextures(TextureIndex) ) )
							{
								warnf( TEXT("Couldn't open %s"), *SourceTextures(TextureIndex) );
								continue;
							}

							const BYTE* Buffer = &Data(0);
							UTexture2D* Temporary = Cast<UTexture2D>
														( 
															TextureFactory->FactoryCreateBinary( 
																	UTexture2D::StaticClass(), 
																	UObject::GetTransientPackage(), 
																	FName(TEXT("ReimportTemporary")), 
																	0, 
																	NULL, 
																	NULL, 
																	Buffer, 
																	Buffer + Data.Num() - 1, 
																	GWarn
															) 
														);

							if( !Temporary )
							{
								warnf( TEXT("Bad source art %s"), *SourceTextures(TextureIndex) );
								continue;
							}

							// Init.
							Texture->Init(Temporary->SizeX,Temporary->SizeY,PF_A8R8G8B8);
							Texture->PostLoad();

							// Copy over source art.
							Texture->SourceArt.Empty( Temporary->SourceArt.Num() );
							Texture->SourceArt.Add( Temporary->SourceArt.Num() );
							appMemcpy( &Texture->SourceArt(0), &Temporary->SourceArt(0), Temporary->SourceArt.Num() );
							Texture->SourceArt.Detach();				

							// Recompress.
							Texture->Compress();
						}
					case PF_Unknown:
					case PF_A32B32G32R32F:
					case PF_G8:
					case PF_G16:
					default:
						break;
				}
			}
		}

		UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );

		UObject::CollectGarbage(RF_Native);
	}

	return 0;
}
IMPLEMENT_CLASS(UReimportTextures)


/*-----------------------------------------------------------------------------
	UConvertTextures commandlet.
-----------------------------------------------------------------------------*/

void UConvertTextures::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;
}
INT UConvertTextures::Main( const TCHAR* Parms )
{
	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GLazyLoad					= 0;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	GEditor->InitEditor();

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FString &Filename = PackageList(PackageIndex);
		UObject* Package;

		if( appStricmp( *FFilename(Filename).GetExtension(), TEXT("upk")) == 0 )
		{
			Package = UObject::LoadPackage( NULL, *Filename, 0 );
			
			if( !Package )
			{
				warnf(NAME_Log, TEXT("Error loading %s!"), *Filename);
				continue;
			}
		}
		else
			continue;

		for( TObjectIterator<UObject> It; It; ++It )
		{
			UTexture2D* Texture = Cast<UTexture2D>(*It);
			if( Texture && Texture->IsIn(Package) && Texture->CompressionSettings == TC_Normalmap )
				Texture->Compress();
		}

		UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );

		UObject::CollectGarbage(RF_Native);
	}

	return 0;
}
IMPLEMENT_CLASS(UConvertTextures)

/*-----------------------------------------------------------------------------
	UCutdownPackages commandlet.
-----------------------------------------------------------------------------*/

void UCutdownPackages::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;
}
INT UCutdownPackages::Main( const TCHAR* Parms )
{
	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GLazyLoad					= 0;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	GEditor->InitEditor();

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	// Create directories for cutdown files.
	GFileManager->MakeDirectory( *FString::Printf(TEXT("..%sCutdownPackages%sPackages"	),PATH_SEPARATOR,PATH_SEPARATOR), 1 );
	GFileManager->MakeDirectory( *FString::Printf(TEXT("..%sCutdownPackages%sMaps"		),PATH_SEPARATOR,PATH_SEPARATOR), 1 );

	// Create a list of packages to base cutdown on.
	FString				PackageName;
	TArray<FString>		BasePackages;
	while( ParseToken( Parms, PackageName, 0 ) )
		new(BasePackages)FString(FFilename(PackageName).GetBaseFilename());

#if 0
	// Load all .u files
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FString &Filename = PackageList(PackageIndex);
		UObject* Package;

		if( appStricmp( *FFilename(Filename).GetExtension(), TEXT("u")) == 0 )
		{
			Package = UObject::LoadPackage( NULL, *Filename, 0 );
			
			if( !Package )
			{
				warnf(NAME_Log, TEXT("Error loading %s!"), *Filename);
				continue;
			}
		}
		else
			continue;
	}

	UObject::ResetLoaders( NULL, 0, 1 );
#endif

	// Load all specified packages/ maps.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		FFilename		Filename			= FFilename(PackageList(PackageIndex));
		INT				BasePackageIndex	= 0;			
		UObject*		Package;

		// Find pacakge.
		for( BasePackageIndex=0; BasePackageIndex<BasePackages.Num(); BasePackageIndex++ )
			if( appStricmp( *Filename.GetBaseFilename(), *BasePackages(BasePackageIndex)) == 0 )
				break;
		if( BasePackageIndex == BasePackages.Num() )
			continue;

		UObject::CollectGarbage( RF_Native | RF_Standalone );

		Package = UObject::LoadPackage( NULL, *Filename, 0 );
		
		if( !Package )
		{
			warnf(NAME_Log, TEXT("Error loading %s!"), *Filename);
			continue;
		}

		// Saving levels.
		ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
		if( Level )
		{
			FString NewMapName = FString::Printf(TEXT("..%sCutdownPackages%sMaps%s%s.%s"),PATH_SEPARATOR,PATH_SEPARATOR,PATH_SEPARATOR,*Filename.GetBaseFilename(),*Filename.GetExtension());
			UObject::SavePackage( Package, Level, 0, *NewMapName, GWarn );
		}
	}

	// Save cutdown packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		FFilename		Filename	= FFilename(PackageList(PackageIndex));
		UObject*		Package		= FindObject<UPackage>( NULL, *Filename.GetBaseFilename() );
		
		// Don't save EnginePackages.
		if( Filename.Caps().InStr( TEXT("ENGINEPACKAGES") ) != INDEX_NONE )
			continue;

		// Only save .upk's that are currently loaded.
		if( !Package || appStricmp( *Filename.GetExtension(), TEXT("upk")) != 0 )
			continue;
		
		FString NewPackageName = FString::Printf(TEXT("..%sCutdownPackages%sPackages%s%s.%s"),PATH_SEPARATOR,PATH_SEPARATOR,PATH_SEPARATOR,*Filename.GetBaseFilename(),*Filename.GetExtension());
		SavePackage( Package, NULL, RF_Standalone, *NewPackageName, NULL );
	}

	return 0;
}
IMPLEMENT_CLASS(UCutdownPackages)

/*-----------------------------------------------------------------------------
	UCreateStreamingLevel commandlet.
-----------------------------------------------------------------------------*/

void UCreateStreamingLevel::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT UCreateStreamingLevel::Main( const TCHAR* Parms )
{
	FString	Filename;

	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();

	GLazyLoad					= 1;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	if( ParseToken( Parms, Filename, 0 ) )
	{
		UObject*	Package = NULL;
		ULevel*		Level	= NULL;
			
		Package	= UObject::LoadPackage( NULL, *Filename, 0 );
		if( Package )
			Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
	
		if( !Level )
		{
			warnf( TEXT("Couldn't open level %s"), *Filename );
			return -1;
		}

		TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
		for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
		{
			const FString &Filename = PackageList(PackageIndex);
			if( appStricmp( *FFilename(Filename).GetExtension(), TEXT("upk")) == 0 )
				UObject::LoadPackage( NULL, *Filename, 0 );
		}
	
		TArray<UMaterial*> Materials;
		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			UMaterial* Material = *It;
			Materials.AddItem( Material );
		}
		
		TArray<UStaticMesh*> StaticMeshes;
		for( TObjectIterator<UStaticMesh> It; It; ++It )
		{
			UStaticMesh* StaticMesh = *It;
			StaticMeshes.AddItem( StaticMesh );
		}

		warnf( TEXT("Unique materials: %i"), Materials.Num() );
		warnf( TEXT("Unique static meshes: %i"), StaticMeshes.Num() );

		if( Level )
		{
			Level->UpdateComponents();

			UINT	MaterialIndex	= 0,
					StaticMeshIndex	= 0;

			for( TObjectIterator<UStaticMeshComponent> It; It; ++It )
			{
				UStaticMeshComponent* StaticMeshComponent = *It;
				if( StaticMeshComponent->Materials.Num() )
				{
					for( INT i=0; i<StaticMeshComponent->Materials.Num(); i++ )
						StaticMeshComponent->Materials(i) = Materials( MaterialIndex++ % Materials.Num() );
				}
				else
				{
					StaticMeshComponent->Materials.AddItem( Materials( MaterialIndex++ % Materials.Num() ) );
				}

				StaticMeshComponent->StaticMesh		= StaticMeshes( StaticMeshIndex++ % StaticMeshes.Num() );
				StaticMeshComponent->CollideActors	= 0;
				StaticMeshComponent->BlockActors	= 0;
			}
		
			for( TObjectIterator<AStaticMeshActor> It; It; ++It )
			{
				AStaticMeshActor* Actor = *It;
				Actor->DrawScale = 0.5f + 2.5f * appFrand();
				//Actor->Location	*= 1.25;
			}

			UObject::SavePackage( Package, Level, 0, *Filename, GWarn );
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UCreateStreamingLevel)


/*-----------------------------------------------------------------------------
	UResavePackages commandlet.
-----------------------------------------------------------------------------*/

void UResavePackages::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT UResavePackages::Main( const TCHAR* Parms )
{
	FString	PackageWildcard;

	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();
	GLazyLoad					= 1;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FFilename& Filename = PackageList(PackageIndex);
		
		if( GFileManager->IsReadOnly( *Filename) )
		{
			warnf(NAME_Log, TEXT("Skipping read-only file %s"), *Filename);
		}
		else if( Filename.GetExtension() == TEXT("U") )
		{
			warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
		}
		else
		{
			warnf(NAME_Log, TEXT("Loading %s"), *Filename);

			// Assert if package couldn't be opened so we have no chance of messing up saving later packages.
			UObject* Package = UObject::LoadPackage( NULL, *Filename, 0 );
			check(Package);
			
			ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
			if( Level )
			{	
				UObject::SavePackage( Package, Level, 0, *Filename, GWarn );
			}
			else
			{
				UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );
			}
		}
	
		UObject::CollectGarbage(RF_Native);
	}

	return 0;
}
IMPLEMENT_CLASS(UResavePackages)


/*-----------------------------------------------------------------------------
	UFixAnimNodeCommandlet commandlet.
-----------------------------------------------------------------------------*/

void UFixAnimNodeCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 0;
	ShowErrorCount  = 1;
}
INT UFixAnimNodeCommandlet::Main( const TCHAR* Parms )
{
	FString	PackageWildcard;

	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();
	GLazyLoad					= 1;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();
	if( !PackageList.Num() )
		return 0;

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FFilename& Filename = PackageList(PackageIndex);
		
		if( Filename.GetExtension() == TEXT("U") )
		{
			warnf(NAME_Log, TEXT("Skipping script file %s"), *Filename);
		}
		else
		{
			warnf(NAME_Log, TEXT("Loading %s"), *Filename);

			UObject* Package = UObject::LoadPackage( NULL, *Filename, 0 );
			if(Package)
			{
				UBOOL bSaveMap = false;

				ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
				if( Level )
				{	
					warnf(NAME_Log, TEXT("Looking For Incorrect AnimNodes.") );

					TArray<AActor*> ActorsToKill;
					TMap<UAnimNode*,USkeletalMeshComponent*> AnimToCompMap;

					// First, find all Components that refer to a shared AnimNode.
					for( TObjectIterator<USkeletalMeshComponent> It; It; ++It )
					{
						USkeletalMeshComponent* SkelComp = *It;
						UBOOL bInLevel = SkelComp->IsIn(Package);

						if( bInLevel && SkelComp->Animations )
						{
							USkeletalMeshComponent** FoundCompPtr = AnimToCompMap.Find(SkelComp->Animations);
							if(FoundCompPtr)
							{
								USkeletalMeshComponent* FoundComp = *FoundCompPtr;

								check( FoundComp->GetOuter()->IsA(AActor::StaticClass()) );
								ActorsToKill.AddUniqueItem( (AActor*)FoundComp->GetOuter() );

								check( SkelComp->GetOuter()->IsA(AActor::StaticClass()) );
								ActorsToKill.AddUniqueItem( (AActor*)SkelComp->GetOuter() );
							}
							else
							{
								AnimToCompMap.Set( SkelComp->Animations, SkelComp );
							}
						}
					}

					if(ActorsToKill.Num() > 0)
					{
						bSaveMap = true;
					}

					// Then iterate over them all, killing them off
					for(INT i=0; i<ActorsToKill.Num(); i++)
					{
						AActor* Actor = ActorsToKill(i);

						ASkeletalMeshActor* SMActor = Cast<ASkeletalMeshActor>(Actor);		

						UBOOL bMakeNewSkelMeshActor = false;
						USkeletalMesh* SkelMesh = NULL;
						FVector Location (0.f);
						FRotator Rotation(0,0,0);
						FName AttachTag = NAME_None;
						UAnimSet* AnimSet = NULL; 
						TArray<USequenceObject*> SeqVars;

						if(SMActor)
						{
							SkelMesh = SMActor->SkeletalMeshComponent->SkeletalMesh;
							Location = SMActor->Location;
							Rotation = SMActor->Rotation;
							AttachTag = SMActor->AttachTag;

							if(Level->GameSequence)
							{
								Level->GameSequence->FindSeqObjectsByObjectName(SMActor->GetFName(), SeqVars);
							}

							bMakeNewSkelMeshActor = true;
						}

						warnf( NAME_Log, TEXT("Destroying: %s"), Actor->GetName() );

						Level->DestroyActor(Actor);

						if(bMakeNewSkelMeshActor)
						{
							ASkeletalMeshActor* NewSMActor = CastChecked<ASkeletalMeshActor>( Level->SpawnActor( ASkeletalMeshActor::StaticClass(), NAME_None, Location, Rotation, NULL ) );

							// Set up the SkeletalMeshComponent based on the old one.
							NewSMActor->ClearComponents();

							NewSMActor->SkeletalMeshComponent->SkeletalMesh = SkelMesh;

							if(AnimSet)
							{
								NewSMActor->SkeletalMeshComponent->AnimSets.AddItem( AnimSet );
							}

							NewSMActor->UpdateComponents();

							// Remember the AttachTag
							NewSMActor->AttachTag = AttachTag;

							// Set Kismet Object Vars to new SMActor
							for(INT j=0; j<SeqVars.Num(); j++)
							{
								USeqVar_Object* ObjVar = Cast<USeqVar_Object>( SeqVars(j) );
								if(ObjVar)
								{
									ObjVar->ObjValue = NewSMActor;
								}
							}

							warnf( NAME_Log, TEXT("Created New: %s"), NewSMActor->GetName() );
						}
					}

					// Save the level if we changed it
					if(bSaveMap)
					{
						if( GFileManager->IsReadOnly(*Filename) )
						{
							warnf( NAME_Log, TEXT("******************************* Need To Save But Read-Only: %s ***************************************"), *Filename );
						}
						else
						{
							warnf( NAME_Log, TEXT("Saving Map") );
							UObject::SavePackage( Package, Level, 0, *Filename, GWarn );
						}
					}
					else
					{
						warnf( NAME_Log, TEXT("Nothing Changed - Not Saving") );						
					}

				} // if(Level)

				warnf( NAME_Log, TEXT("Collecting Garbage") );
				UObject::CollectGarbage(RF_Native);
			} // if(Package)
		}	
	}

	return 0;
}
IMPLEMENT_CLASS(UFixAnimNodeCommandlet)

/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/

/*
	// Saving levels.
	ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );
	if( Level )
		UObject::SavePackage( Package, Level, 0, *Filename, GWarn );
	else
		warnf(NAME_Log, TEXT("Error loading %s! (no ULevel)"), *Filename);

	// Saving packages.
	UObject::SavePackage( Package, NULL, RF_Standalone, *Filename, GWarn );
*/
