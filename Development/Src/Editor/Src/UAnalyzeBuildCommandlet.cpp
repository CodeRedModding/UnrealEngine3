/*=============================================================================
	UAnalyzeContentCommandlet.cpp: Analyze game content
	Copyright 2004 Epic Games, Inc. All Rights Reserved.

Revision history:
	* Created by Andrew Scheidecker
=============================================================================*/

#include "EditorPrivate.h"

//
//	FResourceAnalysis
//

struct FResourceAnalysis
{
	UObject*	Resource;
	UINT		Size;
	FResourceAnalysis(UObject* InResource,UINT InSize): Resource(InResource), Size(InSize) {}
};

IMPLEMENT_COMPARE_CONSTREF( FResourceAnalysis, UAnalyzeBuildCommandlet,{ return B.Size - A.Size; } )

//
//	FResourceTypeAnalysis
//

struct FResourceTypeAnalysis
{
	FString						Description;
	TArray<FResourceAnalysis>	Resources;

	// Constructor.

	FResourceTypeAnalysis(const TCHAR* InDescription):
		Description(InDescription)
	{}

	// Print

	void Print(FOutputDevice& Ar)
	{
		Ar.Logf(TEXT("\tResource type: %s"),*Description);
		UINT	TotalSize = 0;
		Sort<USE_COMPARE_CONSTREF(FResourceAnalysis,UAnalyzeBuildCommandlet)>(&Resources(0),Resources.Num());
		for(UINT ResourceIndex = 0;ResourceIndex < (UINT)Resources.Num();ResourceIndex++)
		{
			//Ar.Logf(TEXT("\t\t%uKB\t%s"),Resources(ResourceIndex).Size / 1024,*Resources(ResourceIndex).Resource->GetFullName());
			TotalSize += Resources(ResourceIndex).Size;
		}
		Ar.Logf(TEXT("\t\t%uKB used by %u resources."),TotalSize / 1024,Resources.Num());
	}
};

//
//	UEditorEngine::AnalyzeLevel
//

void UEditorEngine::AnalyzeLevel(ULevel* Level,FOutputDevice& Ar)
{
	// Collect garbage, making sure that the level remains in memory.

	Level->AddToRoot();
	CollectGarbage(RF_Native);
	Level->RemoveFromRoot();

	// Sort the resources referenced by this level by type.

	FResourceTypeAnalysis	DXT1Textures(TEXT("DXT1 Textures")),
							DXT3Textures(TEXT("DXT3 Textures")),
							DXT5Textures(TEXT("DXT5 Textures")),
							RGBATextures(TEXT("RGBA Textures")),
							StaticMeshes(TEXT("Static Meshes")),
							SHTextures(TEXT("SH Textures"));

	for(FObjectIterator It;It;++It)
	{
		FResourceTypeAnalysis*	Type = NULL;
		UINT					Size = 0;
		if(It->IsA(UTexture2D::StaticClass()))
		{
			UTexture2D*	Texture = CastChecked<UTexture2D>(*It);
			
			switch(Texture->Format)
			{
			case PF_DXT1: Type = &DXT1Textures; break;
			case PF_DXT3: Type = &DXT3Textures; break;
			case PF_DXT5: Type = &DXT5Textures; break;
			case PF_A8R8G8B8: Type = &RGBATextures; break;
			default: continue;
			};

			for(UINT MipIndex = 0;MipIndex < (UINT)Texture->Mips.Num();MipIndex++)
				Size += Texture->Mips(MipIndex).SizeX / GPixelFormats[Texture->Format].BlockSizeX * (Texture->Mips(MipIndex).SizeY / GPixelFormats[Texture->Format].BlockSizeY) * GPixelFormats[Texture->Format].BlockBytes;
		}
		else if(It->IsA(UStaticMesh::StaticClass()))
		{
			UStaticMesh*	StaticMesh = CastChecked<UStaticMesh>(*It);
			Type = &StaticMeshes;
			Size += StaticMesh->PositionVertexBuffer.Size;
			Size += StaticMesh->TangentVertexBuffer.Size;
			for(UINT UVIndex = 0;UVIndex < (UINT)StaticMesh->UVBuffers.Num();UVIndex++)
				Size += StaticMesh->UVBuffers(UVIndex).Size;
			Size += StaticMesh->IndexBuffer.Size;
		}
		else
			continue;
		check(Type);
		new(Type->Resources) FResourceAnalysis(*It,Size);
	}
	DXT1Textures.Print(Ar);
	DXT3Textures.Print(Ar);
	DXT5Textures.Print(Ar);
	RGBATextures.Print(Ar);
	StaticMeshes.Print(Ar);
	SHTextures.Print(Ar);
}

//
//	UAnalyzeBuildCommandlet
//

void UAnalyzeBuildCommandlet::StaticConstructor()
{
	IsClient        = 1;
	IsEditor        = 1;
	IsServer        = 1;
	LazyLoad        = 1;
	ShowErrorCount  = 1;
}
INT UAnalyzeBuildCommandlet::Main( const TCHAR *args )
{
	UClass* EditorEngineClass = UObject::StaticLoadClass( UEditorEngine::StaticClass(), NULL, TEXT("engine-ini:Engine.Engine.EditorEngine"), NULL, LOAD_NoFail, NULL );
	GEngine = GEditor  = ConstructObject<UEditorEngine>( EditorEngineClass );
	GEditor->UseSound = 0;
	GEditor->InitEditor();

	GIsRequestingExit = 1; // Causes ctrl-c to immediately exit.

	TArray<FString>	Packages = GPackageFileCache->GetPackageFileList();
	for(UINT PackageIndex = 0;PackageIndex < (UINT)Packages.Num();PackageIndex++)
	{
		FString	Directory,
				Filename,
				Extension;
		GPackageFileCache->SplitPath(*Packages(PackageIndex),Directory,Filename,Extension);
		if(Extension == FURL::DefaultMapExt)
		{
			warnf(TEXT("Map \'%s\' analysis:"),*Packages(PackageIndex));
			GEditor->AnalyzeLevel(LoadObject<ULevel>(NULL,*FString::Printf(TEXT("%s.MyLevel"),*Filename),*Packages(PackageIndex),LOAD_NoFail,NULL),*GWarn);
		}
	}

	return 0;
}
IMPLEMENT_CLASS(UAnalyzeBuildCommandlet)

