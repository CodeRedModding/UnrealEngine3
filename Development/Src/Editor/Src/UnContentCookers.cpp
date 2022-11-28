/*=============================================================================
	UnContentCookers.cpp: Various platform specific content cookers.
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Daniel Vogel
=============================================================================*/

#include "EditorPrivate.h"
#include "UnLinker.h"
#include "UnConsoleTools.h"

/*-----------------------------------------------------------------------------
	UCookPackagesXenon commandlet.
-----------------------------------------------------------------------------*/

void UCookPackagesXenon::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT UCookPackagesXenon::Main( const TCHAR* Parms )
{
	// Initialize engine.
	UClass* EditorEngineClass	= UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor			= ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound			= 0;
	GEditor->InitEditor();
	GLazyLoad					= 1;
	GIsRequestingExit			= 1;	// so CTRL-C will exit immediately

	// Bind DLL's.
	FString ToolsDir			= FString(appBaseDir()) + TEXT("..\\Development\\Src\\Xenon\\XeTools\\DLL\\");
	FString	XbdmDllFileName		= ToolsDir + TEXT("xbdm.dll");
	FString XeToolsDllFileName	= ToolsDir + TEXT("XeTools.dll");
	void*	XbdmDllHandle		= appGetDllHandle( *XbdmDllFileName );
	if( !XbdmDllHandle )
	{
		warnf(TEXT("Couldn't bind to %s."), *XbdmDllFileName );
		return 0;
	}
	void*	XeToolsDllHandle	= appGetDllHandle( *XeToolsDllFileName );
	if( !XeToolsDllHandle )
	{
		appFreeDllHandle( XbdmDllHandle );
		warnf(TEXT("Couldn't bind to %s."), *XeToolsDllFileName );
		return 0;		
	}

	// Bind function entry points.
	FuncCreateTextureCooker			CreateTextureCooker			= (FuncCreateTextureCooker)			appGetDllExport( XeToolsDllHandle, TEXT("CreateTextureCooker"		) );
	FuncDestroyTextureCooker		DestroyTextureCooker		= (FuncDestroyTextureCooker)		appGetDllExport( XeToolsDllHandle, TEXT("DestroyTextureCooker"		) );
	FuncCreateVolumeTextureCooker	CreateVolumeTextureCooker	= (FuncCreateVolumeTextureCooker)	appGetDllExport( XeToolsDllHandle, TEXT("CreateVolumeTextureCooker"	) );
	FuncDestroyVolumeTextureCooker	DestroyVolumeTextureCooker	= (FuncDestroyVolumeTextureCooker)	appGetDllExport( XeToolsDllHandle, TEXT("DestroyVolumeTextureCooker") );
	FuncCopyFileToConsole			CopyFileToConsole			= (FuncCopyFileToConsole)			appGetDllExport( XeToolsDllHandle, TEXT("CopyFileToConsole"			) );
	if( !CreateTextureCooker || !DestroyTextureCooker || !CreateVolumeTextureCooker || !DestroyVolumeTextureCooker || !CopyFileToConsole )
	{
		appFreeDllHandle( XbdmDllHandle );
		appFreeDllHandle( XeToolsDllHandle );
		warnf(TEXT("Couldn't bind function entry points."));
		return 0;
	}

	// Create the platform specific texture cookers.
	FConsoleTextureCooker*			TextureCooker		= CreateTextureCooker();
	FConsoleVolumeTextureCooker*	VolumeTextureCooker	= CreateVolumeTextureCooker();
	if( !TextureCooker || !VolumeTextureCooker )
	{
		appFreeDllHandle( XbdmDllHandle );
		appFreeDllHandle( XeToolsDllHandle );
		warnf(TEXT("Couldn't create texture cookers."));
		return 0;
	}

	// Retrieve list of all packages in .ini paths.
	TArray<FString> PackageList = GPackageFileCache->GetPackageFileList();

	// Create folder for cooked data.
	FString CookedDir = appGameDir() + TEXT("CookedXenon\\");
	if( !GFileManager->MakeDirectory( *CookedDir ) )
	{
		warnf(NAME_Log, TEXT("Couldn't create %s"), *CookedDir );
		PackageList.Empty();
	}

	// Remote base folder.
	FString RemoteBaseFolder = TEXT("E:\\UnrealEngine3");
	Parse( appCmdLine(), TEXT("BASEDIR=" ), RemoteBaseFolder );

	// Iterate over all packages.
	for( INT PackageIndex = 0; PackageIndex < PackageList.Num(); PackageIndex++ )
	{
		const FFilename&	InFilename		= PackageList(PackageIndex);
		FFilename			OutFilename		= CookedDir + InFilename.GetBaseFilename() + TEXT(".xxx");
		FFilename			XenonFilename	= (appGameDir() + TEXT("CookedContent\\") + InFilename.GetBaseFilename() + TEXT(".xxx")).Replace( TEXT(".."), *RemoteBaseFolder );
		
		DOUBLE				OutFileAge		= GFileManager->GetFileAgeSeconds( *OutFilename );
		DOUBLE				InFileAge		= GFileManager->GetFileAgeSeconds( *InFilename );
		UBOOL				OutFileExists	= GFileManager->FileSize( *OutFilename ) > 0;
		UBOOL				OutFileNewer	= OutFileAge < InFileAge;

		// Skip over unchanged files.
		if( OutFileExists && OutFileNewer )
		{
			warnf(NAME_Log, TEXT("Skipping %s"), *InFilename);
			continue;
		}
		else
		{
			warnf(NAME_Log, TEXT("Loading %s"), *InFilename);
		}

		UPackage* Package = Cast<UPackage>(UObject::LoadPackage( NULL, *InFilename, 0 ));
		if( Package )
		{
			// Don't try cooking already cooked data!
			check( !(Package->PackageFlags & PKG_Cooked) );
			Package->PackageFlags |= PKG_Cooked;

			// Iterate over all objects...
			for( TObjectIterator<UObject> It; It; ++It )
			{
				// ... in the current package.
				if( !It->IsIn(Package) )
					continue;

				UTexture*		Texture			= Cast<UTexture>(*It);
				UTexture2D*		Texture2D		= Cast<UTexture2D>(*It);
				UTexture3D*		Texture3D		= Cast<UTexture3D>(*It);
				UStaticMesh*	StaticMesh		= Cast<UStaticMesh>(*It);
				USkeletalMesh*	SkeletalMesh	= Cast<USkeletalMesh>(*It);
				UClass*			Class			= Cast<UClass>(*It);

				if( Texture )
				{
					// Source art isn't needed on consoles.
					Texture->SourceArt.Detach();
					Texture->SourceArt.Empty();
				}

				if( Texture2D )
				{
					TextureCooker->Init( Texture2D->Format, Texture2D->SizeX, Texture2D->SizeY, Texture2D->Mips.Num() );

					for( INT MipLevel=0; MipLevel<Texture2D->Mips.Num(); MipLevel++ )
					{
						FStaticMipMap2D& Mip = Texture2D->Mips(MipLevel);

						// Load and detach so we can modify the array and have the updated version saved later.
						Mip.Data.Load();
						Mip.Data.Detach();

						UINT	MipSize				= TextureCooker->GetMipSize( MipLevel );
						void*	IntermediateData	= appMalloc( MipSize );
						UINT	SrcRowPitch			= Max<UINT>( 1, (Texture2D->SizeX >> MipLevel) / GPixelFormats[Texture2D->Format].BlockSizeX ) * GPixelFormats[Texture2D->Format].BlockBytes;

						TextureCooker->CookMip( MipLevel, Mip.Data.GetData(), IntermediateData, SrcRowPitch );

						Mip.Data.Empty( MipSize );
						Mip.Data.Add( MipSize );
					
						appMemcpy( Mip.Data.GetData(), IntermediateData, MipSize );
						appFree( IntermediateData );
					}
				}

				if( Texture3D )
				{
					VolumeTextureCooker->Init( Texture3D->Format, Texture3D->SizeX, Texture3D->SizeY, Texture3D->SizeZ, Texture3D->Mips.Num() );

					for( INT MipLevel=0; MipLevel<Texture3D->Mips.Num(); MipLevel++ )
					{
						FStaticMipMap3D& Mip = Texture3D->Mips(MipLevel);

						// Load and detach so we can modify the array and have the updated version saved later.
						Mip.Data.Load();
						Mip.Data.Detach();
						
						UINT	MipSize				= VolumeTextureCooker->GetMipSize( MipLevel );
						void*	IntermediateData	= appMalloc( MipSize );
						UINT	SrcRowPitch			= Max<UINT>( 1, (Texture3D->SizeX >> MipLevel) / GPixelFormats[Texture3D->Format].BlockSizeX ) * GPixelFormats[Texture3D->Format].BlockBytes;
						UINT	SrcSlicePitch		= Max<UINT>( 1, (Texture3D->SizeY >> MipLevel) / GPixelFormats[Texture3D->Format].BlockSizeY ) * SrcRowPitch;

						VolumeTextureCooker->CookMip( MipLevel, Mip.Data.GetData(), IntermediateData, SrcRowPitch, SrcSlicePitch );
		
						Mip.Data.Empty( MipSize );
						Mip.Data.Add( MipSize );
						
						appMemcpy( Mip.Data.GetData(), IntermediateData, MipSize );
						appFree( IntermediateData );
					}
				}

				if( StaticMesh )
				{
					// RawTriangles is only used in the Editor and for rebuilding static meshes.
					StaticMesh->RawTriangles.Detach();
					StaticMesh->RawTriangles.Empty();
				}

				if( SkeletalMesh )
				{
					// Influenses, Wedges, Faces and Points lazy arrays are only used for reprocessing data in the Editor.
					for( INT LODIndex=0; LODIndex<SkeletalMesh->LODModels.Num(); LODIndex++ )
					{
						SkeletalMesh->LODModels(LODIndex).Influences.Detach();
						SkeletalMesh->LODModels(LODIndex).Influences.Empty();

						SkeletalMesh->LODModels(LODIndex).Wedges.Detach();
						SkeletalMesh->LODModels(LODIndex).Wedges.Empty();

						SkeletalMesh->LODModels(LODIndex).Faces.Detach();
						SkeletalMesh->LODModels(LODIndex).Faces.Empty();

						SkeletalMesh->LODModels(LODIndex).Points.Detach();
						SkeletalMesh->LODModels(LODIndex).Points.Empty();
					}
				}

				if( Class )
				{
					// Get rid of cpp and script text.
					Class->ScriptText	= NULL;
					Class->CppText		= NULL;
				}
			}

			// Save the data in the platform endianness.
			
			// We need to reset loaders here to avoid SavePackage potentially loading data (lazy loaders) while 
			// GIsConsoleCooking is set to TRUE.
			ResetLoaders( Package, FALSE, TRUE );
			GIsConsoleCooking = TRUE;
			ULevel* Level = FindObject<ULevel>( Package, TEXT("MyLevel") );		
			if( Level )
			{	
				UObject::SavePackage( Package, Level, 0, *OutFilename, GWarn );
			}
			else
			{
				UObject::SavePackage( Package, NULL, RF_Standalone, *OutFilename, GWarn );
			}
			GIsConsoleCooking = FALSE;

			// Copy file over to Xenon.
			if( !CopyFileToConsole( TCHAR_TO_ANSI(*OutFilename), TCHAR_TO_ANSI(*XenonFilename) ) )
			{
				warnf( TEXT("Failed copying local file %s to Xenon at %s"), *OutFilename, *XenonFilename );
			}
		}
		else
		{
			warnf(NAME_Log, TEXT("Failed loading %s"), *InFilename);
		}

		UObject::CollectGarbage(RF_Native);
	}

	DestroyTextureCooker( TextureCooker );
	DestroyVolumeTextureCooker( VolumeTextureCooker );

	appFreeDllHandle( XbdmDllHandle );
	appFreeDllHandle( XeToolsDllHandle );

	return 0;
}
IMPLEMENT_CLASS(UCookPackagesXenon)


/*-----------------------------------------------------------------------------
	The End.
-----------------------------------------------------------------------------*/