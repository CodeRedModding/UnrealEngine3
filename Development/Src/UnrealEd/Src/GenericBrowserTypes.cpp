
#include "UnrealEd.h"
#include "UnTerrain.h"
#include "EngineSoundClasses.h"
#include "EngineParticleClasses.h"
#include "EngineAnimClasses.h"
#include "EngineSequenceClasses.h"
#include "EngineMaterialClasses.h"

#include "AnimSetViewer.h"
#include "UnLinkedObjEditor.h"
#include "SoundCueEditor.h"
#include "AnimTreeEditor.h"

#include "..\..\Launch\Resources\resource.h"

/*------------------------------------------------------------------------------
	Implementations.
------------------------------------------------------------------------------*/

IMPLEMENT_CLASS(UGenericBrowserType)
IMPLEMENT_CLASS(UGenericBrowserType_All)
IMPLEMENT_CLASS(UGenericBrowserType_Animation)
IMPLEMENT_CLASS(UGenericBrowserType_AnimTree)
IMPLEMENT_CLASS(UGenericBrowserType_Material)
IMPLEMENT_CLASS(UGenericBrowserType_MaterialInstance)
IMPLEMENT_CLASS(UGenericBrowserType_ParticleSystem)
IMPLEMENT_CLASS(UGenericBrowserType_PhysicsAsset)
IMPLEMENT_CLASS(UGenericBrowserType_Prefab)
IMPLEMENT_CLASS(UGenericBrowserType_Sequence)
IMPLEMENT_CLASS(UGenericBrowserType_SHM)
IMPLEMENT_CLASS(UGenericBrowserType_SkeletalMesh)
IMPLEMENT_CLASS(UGenericBrowserType_SoundCue)
IMPLEMENT_CLASS(UGenericBrowserType_Sounds)
IMPLEMENT_CLASS(UGenericBrowserType_SoundWave)
IMPLEMENT_CLASS(UGenericBrowserType_StaticMesh)
IMPLEMENT_CLASS(UGenericBrowserType_TerrainLayer)
IMPLEMENT_CLASS(UGenericBrowserType_TerrainMaterial)
IMPLEMENT_CLASS(UGenericBrowserType_Texture)

/*------------------------------------------------------------------------------
	UGenericBrowserType
------------------------------------------------------------------------------*/

UBOOL UGenericBrowserType::Supports( UObject* InObject )
{
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		if( InObject->IsA( SupportInfo(x).Class ) )
		{
			return 1;
		}
	}

	return 0;
}

FColor UGenericBrowserType::GetBorderColor( UObject* InObject )
{
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		if( InObject->IsA( SupportInfo(x).Class ) )
		{
			return SupportInfo(x).BorderColor;
		}
	}

	return FColor(255,255,255);
}

wxMenu* UGenericBrowserType::GetContextMenu( UObject* InObject )
{
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		if( InObject->IsA( SupportInfo(x).Class ) && SupportInfo(x).ContextMenu != NULL )
		{
			return SupportInfo(x).ContextMenu;
		}
	}

	return (new WxMBGenericBrowserContext_Object);
}

UBOOL UGenericBrowserType::ShowObjectEditor()
{
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		for( TSelectedObjectIterator It( SupportInfo(x).Class ) ; It ; ++It )
		{
			UObject* obj = *It;
			if( ShowObjectEditor( obj ) )
			{
				return 1;
			}
		}
	}

	return 0;
}

void UGenericBrowserType::InvokeCustomCommand( INT InCommand )
{
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		for( TSelectedObjectIterator It( SupportInfo(x).Class ) ; It ; ++It )
		{
			UObject* obj = *It;
			InvokeCustomCommand( InCommand, obj );
		}
	}
}

void UGenericBrowserType::DoubleClick()
{
	for( INT x = 0 ; x < SupportInfo.Num() ; ++x )
	{
		for( TSelectedObjectIterator It( SupportInfo(x).Class ) ; It ; ++It )
		{
			UObject* obj = *It;
			DoubleClick( obj );
		}
	}
}

void UGenericBrowserType::DoubleClick( UObject* InObject )
{
	// This is what most types want to do with a double click so we
	// handle this in the base class.  Override this function to
	// implement a custom behavior.

	ShowObjectEditor( InObject );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_All
------------------------------------------------------------------------------*/

void UGenericBrowserType_All::Init()
{
	for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
	{
		UGenericBrowserType* gbt = *It;

		if( gbt != this )
		{
			for( INT x = 0 ; x < gbt->SupportInfo.Num() ; ++x )
			{
				SupportInfo.AddUniqueItem( gbt->SupportInfo(x) );
			}
		}
	}
}

UBOOL UGenericBrowserType_All::ShowObjectEditor()
{
	for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
	{
		UGenericBrowserType* gbt = *It;

		if( gbt != this )
		{
			if( gbt->ShowObjectEditor() )
			{
				return 1;
			}
		}
	}

	return 0;
}

void UGenericBrowserType_All::InvokeCustomCommand( INT InCommand )
{
	for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
	{
		UGenericBrowserType* gbt = *It;

		if( gbt != this )
		{
			gbt->InvokeCustomCommand( InCommand );
		}
	}
}

void UGenericBrowserType_All::DoubleClick()
{
	for( TObjectIterator<UGenericBrowserType> It ; It ; ++It )
	{
		UGenericBrowserType* gbt = *It;

		if( gbt != this )
		{
			gbt->DoubleClick();
		}
	}
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_Animation
------------------------------------------------------------------------------*/

void UGenericBrowserType_Animation::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UAnimSet::StaticClass(), FColor(192,128,255), new WxMBGenericBrowserContext_AnimSet ) );
}

UBOOL UGenericBrowserType_Animation::ShowObjectEditor( UObject* InObject )
{
	UAnimSet* AnimSet = Cast<UAnimSet>(InObject);

	if( !AnimSet )
	{
		return 0;
	}

	// When selecting an animation set, we have to find a skeletal mesh that we can play this animation on.

	for(TObjectIterator<USkeletalMesh> It; It; ++It)
	{
		USkeletalMesh* SkelMesh = *It;
		if( AnimSet->CanPlayOnSkeletalMesh(SkelMesh) )
		{
			WxAnimSetViewer* AnimSetViewer = new WxAnimSetViewer( (wxWindow*)GApp->EditorFrame, -1, SkelMesh, AnimSet );
			AnimSetViewer->SetSize(1200,800);
			AnimSetViewer->Show(1);

			return 1;
		}
	}

	// Could not find a skeletal mesh to play on.
	appMsgf( 0, TEXT("Could not find SkeletalMesh to play this AnimSet on.")  );
	
	return 0;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_AnimTree
------------------------------------------------------------------------------*/

void UGenericBrowserType_AnimTree::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UAnimTree::StaticClass(), FColor(255,128,192), new WxMBGenericBrowserContext_AnimTree ) );
}

UBOOL UGenericBrowserType_AnimTree::ShowObjectEditor( UObject* InObject )
{
	UAnimTree* AnimTree = Cast<UAnimTree>(InObject);
	if(AnimTree)
	{
		WxAnimTreeEditor* AnimTreeEditor = new WxAnimTreeEditor( (wxWindow*)GApp->EditorFrame, -1, AnimTree );
		AnimTreeEditor->SetSize(1024, 768);
		AnimTreeEditor->Show(1);
	}

	
	return 0;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_Material
------------------------------------------------------------------------------*/

void UGenericBrowserType_Material::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UMaterial::StaticClass(), FColor(0,255,0), new WxMBGenericBrowserContext_Material ) );
}

UBOOL UGenericBrowserType_Material::ShowObjectEditor( UObject* InObject )
{
	WxMaterialEditor* MaterialEditor = new WxMaterialEditor( (wxWindow*)GApp->EditorFrame,-1,CastChecked<UMaterial>(InObject) );
	MaterialEditor->SetSize(1024,768);
	MaterialEditor->Show();

	return 1;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_MaterialInstance
------------------------------------------------------------------------------*/

void UGenericBrowserType_MaterialInstance::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UMaterialInstanceConstant::StaticClass(), FColor(0,128,0), NULL ) );
}

UBOOL UGenericBrowserType_MaterialInstance::ShowObjectEditor( UObject* InObject )
{
	WxPropertyWindowFrame* Properties = new WxPropertyWindowFrame( GApp->EditorFrame, -1, 0 );
	Properties->bAllowClose = 1;
	Properties->PropertyWindow->SetObject( InObject, 1,1,1 );
	Properties->SetTitle( *FString::Printf( TEXT("Material Instance %s"), InObject->GetPathName() ) );
	Properties->Show();

	return 1;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_ParticleSystem
------------------------------------------------------------------------------*/

void UGenericBrowserType_ParticleSystem::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UParticleSystem::StaticClass(), FColor(255,255,255), new WxMBGenericBrowserContext_ParticleSystem ) );
}

UBOOL UGenericBrowserType_ParticleSystem::ShowObjectEditor( UObject* InObject )
{
	WxCascade* ParticleEditor = new WxCascade(GApp->EditorFrame, -1, CastChecked<UParticleSystem>(InObject));
	ParticleEditor->SetSize(256,256,1024,768);
	ParticleEditor->Show(1);

	return 1;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_PhysicsAsset
------------------------------------------------------------------------------*/

void UGenericBrowserType_PhysicsAsset::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPhysicsAsset::StaticClass(), FColor(255,192,128), new WxMBGenericBrowserContext_PhysicsAsset ) );
}

UBOOL UGenericBrowserType_PhysicsAsset::ShowObjectEditor( UObject* InObject )
{
	WPhAT* pe = new WPhAT( CastChecked<UPhysicsAsset>(InObject) );
	pe->OpenWindow();	

	return 1;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_Prefab
------------------------------------------------------------------------------*/

void UGenericBrowserType_Prefab::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UPrefab::StaticClass(), FColor(255,255,0), NULL ) );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_Sequence
------------------------------------------------------------------------------*/

void UGenericBrowserType_Sequence::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USequence::StaticClass(), FColor(255,255,255), NULL ) );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_SHM
------------------------------------------------------------------------------*/

void UGenericBrowserType_SHM::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USphericalHarmonicMap::StaticClass(), FColor(255,255,255), NULL ) );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_SkeletalMesh
------------------------------------------------------------------------------*/

void UGenericBrowserType_SkeletalMesh::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USkeletalMesh::StaticClass(), FColor(255,0,255), new WxMBGenericBrowserContext_Skeletal ) );
}

UBOOL UGenericBrowserType_SkeletalMesh::ShowObjectEditor( UObject* InObject )
{
	WxAnimSetViewer* AnimSetViewer = new WxAnimSetViewer( (wxWindow*)GApp->EditorFrame, -1, CastChecked<USkeletalMesh>(InObject), NULL );
	AnimSetViewer->SetSize(1200,800);
	AnimSetViewer->Show();

	return 1;
}

void UGenericBrowserType_SkeletalMesh::InvokeCustomCommand( INT InCommand, UObject* InObject )
{
	switch( InCommand )
	{
		case 0:	// Create New Physics Asset
		{
			USkeletalMesh* SkelMesh = Cast<USkeletalMesh>(InObject);

			if( SkelMesh )
			{
				// Create a new physics asset from the selected skeletal mesh. Defaults to being in the same package/group as the skeletal mesh.

				FString DefaultPackage;

				// If there are 2 levels above this mesh - top is package, then group.

				if( SkelMesh->GetOuter()->GetOuter() )
				{
					const TCHAR* PackageName = SkelMesh->GetOuter()->GetOuter()->GetName();
					const TCHAR* GroupName = SkelMesh->GetOuter()->GetName();

					DefaultPackage = FString::Printf( TEXT("%s.%s"), PackageName, GroupName );
				}
				// Otherwise, just a package.
				else 
				{
					const TCHAR* PackageName = SkelMesh->GetOuter()->GetName();

					DefaultPackage = FString( PackageName );
				}

				// Make default name by appending '_Physics' to the SkeletalMesh name.

				FString DefaultAssetName = FString( FString::Printf( TEXT("%s_Physics"), SkelMesh->GetName() ) );

				WxDlgNewPhysicsAsset dlg;

				if( dlg.ShowModal( DefaultPackage, DefaultAssetName, SkelMesh, false ) == wxID_OK )
				{			
					UPackage* Pkg = UObject::CreatePackage(NULL, *dlg.Package);
					UPhysicsAsset* NewAsset = ConstructObject<UPhysicsAsset>( UPhysicsAsset::StaticClass(), Pkg, *dlg.Name, RF_Public|RF_Standalone|RF_Transactional );

					if(NewAsset)
					{
						UBOOL success = NewAsset->CreateFromSkeletalMesh( SkelMesh, dlg.Params );

						if(success)
						{
							if(dlg.bOpenAssetNow)
							{
								WPhAT* pe = new WPhAT( NewAsset );
								pe->OpenWindow();
							}
						}
						else
						{
							appMsgf( 0, TEXT("Failed to create Physics Asset '%s' from Skeletal Mesh '%s'."), *dlg.Name, SkelMesh->GetName() );
							delete NewAsset;
						}
					}
					else
					{
						appMsgf( 0, TEXT("Failed to create new Physics Asset.") );
					}
				}
			}
		}
		break;
	}
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_SoundCue
------------------------------------------------------------------------------*/

void UGenericBrowserType_SoundCue::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundCue::StaticClass(), FColor(0,175,255), new WxMBGenericBrowserContext_SoundCue ) );
}

UBOOL UGenericBrowserType_SoundCue::ShowObjectEditor( UObject* InObject )
{
	WxSoundCueEditor* SoundCueEditor = new WxSoundCueEditor(GApp->EditorFrame,-1,CastChecked<USoundCue>(InObject));
	SoundCueEditor->Show(1);

	return 1;
}

void UGenericBrowserType_SoundCue::InvokeCustomCommand( INT InCommand, UObject* InObject )
{
	switch( InCommand )
	{
		case 0:	// Play
		{
			UGenericBrowserType_Sounds::Play( (USoundCue*)InObject );
		}
		break;

		case 1:	// Stop
		{
			UGenericBrowserType_Sounds::Stop();
		}
		break;
	}
}

void UGenericBrowserType_SoundCue::DoubleClick( UObject* InObject )
{
	UGenericBrowserType_Sounds::Play( (USoundCue*)InObject );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_Sounds
------------------------------------------------------------------------------*/

void UGenericBrowserType_Sounds::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundCue::StaticClass(), FColor(0,175,255), new WxMBGenericBrowserContext_SoundCue ) );
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundNodeWave::StaticClass(), FColor(0,0,255), new WxMBGenericBrowserContext_Sound ) );
}

UBOOL UGenericBrowserType_Sounds::ShowObjectEditor( UObject* InObject )
{
	if( !Cast<USoundCue>( InObject ) )
	{
		return 0;
	}

	WxSoundCueEditor* SoundCueEditor = new WxSoundCueEditor(GApp->EditorFrame,-1,CastChecked<USoundCue>(InObject));
	SoundCueEditor->Show(1);

	return 1;
}

void UGenericBrowserType_Sounds::InvokeCustomCommand( INT InCommand, UObject* InObject )
{
	switch( InCommand )
	{
		case 0:	// Play
		{
			if( Cast<USoundCue>( InObject ) )
			{
				UGenericBrowserType_Sounds::Play( (USoundCue*)InObject );
			}
			else
			{
				UGenericBrowserType_Sounds::Play( (USoundNode*)InObject );
			}
		}
		break;

		case 1:	// Stop
		{
			UGenericBrowserType_Sounds::Stop();
		}
		break;
	}
}

void UGenericBrowserType_Sounds::DoubleClick( UObject* InObject )
{
	if( Cast<USoundCue>( InObject ) )
	{
		UGenericBrowserType_Sounds::Play( (USoundCue*)InObject );
	}
	else
	{
		UGenericBrowserType_Sounds::Play( (USoundNode*)InObject );
	}
}

void UGenericBrowserType_Sounds::Play( USoundNode* InSound )
{
	if(GEditor->Client && GEditor->Client->GetAudioDevice())
	{
		USoundNode*			SoundNode			= Cast<USoundNode>( InSound );
		USoundCue*			SoundCue			= ConstructObject<USoundCue>(USoundCue::StaticClass());
		SoundCue->FirstNode						= SoundNode;
		
		Play( SoundCue );
	}
}

void UGenericBrowserType_Sounds::Play( USoundCue* InSound )
{
	if( GEditor->Client && GEditor->Client->GetAudioDevice() )
	{
		USoundCue*			SoundCue		= InSound;
		UAudioComponent*	AudioComponent	= UAudioDevice::CreateComponent(SoundCue,GUnrealEd->Level,NULL,0);

		Stop();
				
		AudioComponent->SoundCue				= SoundCue;
		AudioComponent->bUseOwnerLocation		= 0;
		AudioComponent->bAutoDestroy			= 1;
		AudioComponent->Location				= FVector(0,0,0);
		AudioComponent->bNonRealtime			= 1;
		AudioComponent->bAllowSpatialization	= 0;
		AudioComponent->Play();
	}
}

void UGenericBrowserType_Sounds::Stop()
{
	if(GEditor->Client && GEditor->Client->GetAudioDevice() )
	{
		for( TObjectIterator<UAudioComponent> It ; It ; ++It )
		{
			if( !It->Owner )
			{
				It->Stop();
			}
		}
	}
}


/*------------------------------------------------------------------------------
	UGenericBrowserType_SoundWave
------------------------------------------------------------------------------*/

void UGenericBrowserType_SoundWave::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( USoundNodeWave::StaticClass(), FColor(0,0,255), new WxMBGenericBrowserContext_Sound ) );
}

void UGenericBrowserType_SoundWave::InvokeCustomCommand( INT InCommand, UObject* InObject )
{
	switch( InCommand )
	{
		case 0:		// Play
		{
			UGenericBrowserType_Sounds::Play( (USoundNode*)InObject );
		}
		break;

		case 1:		// Stop
		{
			UGenericBrowserType_Sounds::Stop();
		}
		break;
	}
}

void UGenericBrowserType_SoundWave::DoubleClick( UObject* InObject )
{
	UGenericBrowserType_Sounds::Play( (USoundNode*)InObject );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_StaticMesh
------------------------------------------------------------------------------*/

void UGenericBrowserType_StaticMesh::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UStaticMesh::StaticClass(), FColor(0,255,255), new WxMBGenericBrowserContext_StaticMesh ) );
}

UBOOL UGenericBrowserType_StaticMesh::ShowObjectEditor( UObject* InObject )
{
	WxStaticMeshEditor* StaticMeshEditor = new WxStaticMeshEditor( GApp->EditorFrame, -1, CastChecked<UStaticMesh>(InObject) );
	StaticMeshEditor->Show(1);

	return 1;
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_TerrainLayer
------------------------------------------------------------------------------*/

void UGenericBrowserType_TerrainLayer::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTerrainLayerSetup::StaticClass(), FColor(128,192,255), NULL ) );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_TerrainMaterial
------------------------------------------------------------------------------*/

void UGenericBrowserType_TerrainMaterial::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTerrainMaterial::StaticClass(), FColor(192,255,192), NULL ) );
}

/*------------------------------------------------------------------------------
	UGenericBrowserType_Texture
------------------------------------------------------------------------------*/

void UGenericBrowserType_Texture::Init()
{
	SupportInfo.AddItem( FGenericBrowserTypeInfo( UTexture::StaticClass(), FColor(255,0,0), new WxMBGenericBrowserContext_Texture ) );
}

UBOOL UGenericBrowserType_Texture::ShowObjectEditor( UObject* InObject )
{
	WxDlgTextureProperties* dlg = new WxDlgTextureProperties();
	dlg->Show( 1, Cast<UTexture>(InObject) );

	return 1;
}
