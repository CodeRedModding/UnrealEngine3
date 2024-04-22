/*=============================================================================
	UnEdExp.cpp: Editor exporters.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnTerrain.h"
#include "EngineSequenceClasses.h"
#include "EngineSoundClasses.h"
#include "EngineMaterialClasses.h"
#include "EnginePhysicsClasses.h"
#include "EngineMeshClasses.h"
#include "EngineFoliageClasses.h"
#include "UnFracturedStaticMesh.h"
#include "LandscapeDataAccess.h"
#include "SurfaceIterators.h"

#if WITH_FBX
#include "UnFbxExporter.h"
#endif

/*------------------------------------------------------------------------------
	UTextBufferExporterTXT implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTextBufferExporterTXT::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UTextBuffer::StaticClass();
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("TXT")) );
	new(FormatDescription)FString(TEXT("Text file"));
	bText = 1;
}
UBOOL UTextBufferExporterTXT::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UTextBuffer* TextBuffer = CastChecked<UTextBuffer>( Object );
	FString Str( TextBuffer->Text );

	TCHAR* Start = const_cast<TCHAR*>(*Str);
	TCHAR* End   = Start + Str.Len();
	while( Start<End && (Start[0]=='\r' || Start[0]=='\n' || Start[0]==' ') )
		Start++;
	while( End>Start && (End [-1]=='\r' || End [-1]=='\n' || End [-1]==' ') )
		End--;
	*End = 0;

	Ar.Log( Start );

	return 1;
}
IMPLEMENT_CLASS(UTextBufferExporterTXT);

/*------------------------------------------------------------------------------
	USoundExporterWAV implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
#if 1
void USoundExporterWAV::InitializeIntrinsicPropertyValues()
{
	SupportedClass = USoundNodeWave::StaticClass();
	bText = 0;
	new( FormatExtension ) FString( TEXT( "WAV" ) );
	new( FormatDescription ) FString( TEXT( "Sound" ) );
}
UBOOL USoundExporterWAV::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
	USoundNodeWave* Sound = CastChecked<USoundNodeWave>( Object );
	void* RawWaveData = Sound->RawData.Lock( LOCK_READ_ONLY );
	Ar.Serialize( RawWaveData, Sound->RawData.GetBulkDataSize() );
	Sound->RawData.Unlock();
	return TRUE;
}
IMPLEMENT_CLASS(USoundExporterWAV);

#else
/*------------------------------------------------------------------------------
	USoundExporterOGG implementation.
------------------------------------------------------------------------------*/

/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void USoundExporterOGG::InitializeIntrinsicPropertyValues()
{
	SupportedClass = USoundNodeWave::StaticClass();
	bText = 0;
	new( FormatExtension ) FString( TEXT( "OGG" ) );
	new( FormatDescription ) FString( TEXT( "Sound" ) );
}
UBOOL USoundExporterOGG::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
	USoundNodeWave* Sound = CastChecked<USoundNodeWave>( Object );
	void* RawOggData = Sound->CompressedPCData.Lock( LOCK_READ_ONLY );
	Ar.Serialize( RawOggData, Sound->CompressedPCData.GetBulkDataSize() );
	Sound->CompressedPCData.Unlock();
	return TRUE;
}
IMPLEMENT_CLASS(USoundExporterOGG);
#endif

/*------------------------------------------------------------------------------
	USoundSurroundExporterWAV implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void USoundSurroundExporterWAV::InitializeIntrinsicPropertyValues()
{
	SupportedClass = USoundNodeWave::StaticClass();
	bText = 0;
	new( FormatExtension ) FString( TEXT( "WAV" ) );
	new( FormatDescription ) FString( TEXT( "Multichannel Sound" ) );
}

INT USoundSurroundExporterWAV::GetFileCount( void ) const
{
	return( SPEAKER_Count );
}

FString USoundSurroundExporterWAV::GetUniqueFilename( const TCHAR* Filename, INT FileIndex )
{
	static FString SpeakerLocations[SPEAKER_Count] =
	{
		TEXT( "_fl" ),			// SPEAKER_FrontLeft
		TEXT( "_fr" ),			// SPEAKER_FrontRight
		TEXT( "_fc" ),			// SPEAKER_FrontCenter
		TEXT( "_lf" ),			// SPEAKER_LowFrequency
		TEXT( "_sl" ),			// SPEAKER_SideLeft
		TEXT( "_sr" ),			// SPEAKER_SideRight
		TEXT( "_bl" ),			// SPEAKER_BackLeft
		TEXT( "_br" )			// SPEAKER_BackRight
	};

	FFilename WorkName = Filename;
	FString ReturnName = WorkName.GetBaseFilename( FALSE ) + SpeakerLocations[FileIndex] + FString( ".WAV" );

	return( ReturnName );
}

UBOOL USoundSurroundExporterWAV::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
	UBOOL bResult = FALSE;

	USoundNodeWave* Sound = CastChecked<USoundNodeWave>( Object );
	if ( Sound->ChannelSizes.Num() > 0 )
	{
		BYTE* RawWaveData = ( BYTE * )Sound->RawData.Lock( LOCK_READ_ONLY );

		if( Sound->ChannelSizes( FileIndex ) )
		{
			Ar.Serialize( RawWaveData + Sound->ChannelOffsets( FileIndex ), Sound->ChannelSizes( FileIndex ) );
		}

		Sound->RawData.Unlock();

		bResult = Sound->ChannelSizes( FileIndex ) != 0;
	}

	return bResult;
}
IMPLEMENT_CLASS(USoundSurroundExporterWAV);

/*------------------------------------------------------------------------------
	UClassExporterUC implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UClassExporterUC::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UClass::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("UC")) );
	new(FormatDescription)FString(TEXT("UnrealScript"));
}
UBOOL UClassExporterUC::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UClass* Class = CastChecked<UClass>( Object );

	// if the class is instrinsic, it won't have script text
	if ( (Class->ClassFlags&CLASS_Intrinsic) != 0 )
	{
		return FALSE;
	}

	// Nothing to do if the script text was e.g. stripped by the cooker.
	if ( Class->ScriptText == NULL )
	{
		return FALSE;
	}

	// Export script text.
	check(Class->GetDefaultsCount());
	UExporter::ExportToOutputDevice( Context, Class->ScriptText, NULL, Ar, TEXT("txt"), TextIndent, PortFlags );

	// Export cpptext.
	if( Class->CppText )
	{
		Ar.Log( TEXT("\r\n\r\ncpptext\r\n{\r\n") );
		Ar.Log( *Class->CppText->Text );
		Ar.Log( TEXT("\r\n}\r\n") );
	}

	// Export default properties that differ from parent's.
	Ar.Log( TEXT("\r\n\r\ndefaultproperties\r\n{\r\n") );
	ExportProperties
	(
		Context,
		Ar,
		Class,
		Class->GetDefaults(),
		TextIndent+3,
		Class->GetSuperClass(),
		Class->GetSuperClass() ? Class->GetSuperClass()->GetDefaults() : NULL,
		Class
	);
	Ar.Log( TEXT("}\r\n") );

	return TRUE;
}
IMPLEMENT_CLASS(UClassExporterUC);

/*------------------------------------------------------------------------------
	UObjectExporterT3D implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UObjectExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UObject::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal object text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal object text"));
}
UBOOL UObjectExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	EmitBeginObject(Ar, Object, PortFlags);
		ExportObjectInner( Context, Object, Ar, PortFlags);
	EmitEndObject(Ar);

	return TRUE;
}
IMPLEMENT_CLASS(UObjectExporterT3D);

/*------------------------------------------------------------------------------
	UPolysExporterT3D implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UPolysExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UPolys::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal poly text"));
}
UBOOL UPolysExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	UPolys* Polys = CastChecked<UPolys>( Object );

	FLightingChannelContainer		DefaultLightingChannels;
	DefaultLightingChannels.Bitfield		= 0;
	DefaultLightingChannels.BSP				= TRUE;
	DefaultLightingChannels.bInitialized	= TRUE;

	Ar.Logf( TEXT("%sBegin PolyList\r\n"), appSpc(TextIndent) );
	for( INT i=0; i<Polys->Element.Num(); i++ )
	{
		FPoly* Poly = &Polys->Element(i);
		TCHAR TempStr[MAX_SPRINTF]=TEXT("");

		// Start of polygon plus group/item name if applicable.
		// The default values need to jive FPoly::Init().
		Ar.Logf( TEXT("%s   Begin Polygon"), appSpc(TextIndent) );
		if( Poly->ItemName != NAME_None )
		{
			Ar.Logf( TEXT(" Item=%s"), *Poly->ItemName.ToString() );
		}
		if( Poly->Material )
		{
			Ar.Logf( TEXT(" Texture=%s"), *Poly->Material->GetPathName() );
		}
		if( Poly->PolyFlags != 0 )
		{
			Ar.Logf( TEXT(" Flags=%i"), Poly->PolyFlags );
		}
		if( Poly->iLink != INDEX_NONE )
		{
			Ar.Logf( TEXT(" Link=%i"), Poly->iLink );
		}
		if ( Poly->ShadowMapScale != 32.0f )
		{
			Ar.Logf( TEXT(" ShadowMapScale=%f"), Poly->ShadowMapScale );
		}
		if ( Poly->LightingChannels != DefaultLightingChannels.Bitfield )
		{
			Ar.Logf( TEXT(" LightingChannels=%i"), Poly->LightingChannels );
		}
		Ar.Logf( TEXT("\r\n") );

		// All coordinates.
		Ar.Logf( TEXT("%s      Origin   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Base) );
		Ar.Logf( TEXT("%s      Normal   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Normal) );
		Ar.Logf( TEXT("%s      TextureU %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureU) );
		Ar.Logf( TEXT("%s      TextureV %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->TextureV) );
		for( INT j=0; j<Poly->Vertices.Num(); j++ )
		{
			Ar.Logf( TEXT("%s      Vertex   %s\r\n"), appSpc(TextIndent), SetFVECTOR(TempStr,&Poly->Vertices(j)) );
		}
		Ar.Logf( TEXT("%s   End Polygon\r\n"), appSpc(TextIndent) );
	}
	Ar.Logf( TEXT("%sEnd PolyList\r\n"), appSpc(TextIndent) );

	return 1;
}
IMPLEMENT_CLASS(UPolysExporterT3D);

/*------------------------------------------------------------------------------
	UModelExporterT3D implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UModelExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UModel::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal model text"));
}
UBOOL UModelExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	UModel* Model = CastChecked<UModel>( Object );

	Ar.Logf( TEXT("%sBegin Brush Name=%s\r\n"), appSpc(TextIndent), *Model->GetName() );
		UExporter::ExportToOutputDevice( Context, Model->Polys, NULL, Ar, Type, TextIndent+3, PortFlags );
//		ExportObjectInner( Context, Model, Ar, PortFlags | PPF_ExportsNotFullyQualified );
	Ar.Logf( TEXT("%sEnd Brush\r\n"), appSpc(TextIndent) );

	return 1;
}
IMPLEMENT_CLASS(UModelExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterT3D implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void ULevelExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UWorld::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal world text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal world text"));
}

void ExporterHelper_DumpPackageInners(const FExportObjectInnerContext* Context, UPackage* InPackage, INT TabCount)
{
	const TArray<UObject*>* Inners = Context->GetObjectInners(InPackage);
	if (Inners)
	{
		for (INT InnerIndex = 0; InnerIndex < Inners->Num(); InnerIndex++)
		{
			UObject* InnerObj = (*Inners)(InnerIndex);

			FString TabString;
			for (INT TabOutIndex = 0; TabOutIndex < TabCount; TabOutIndex++)
			{
				TabString += TEXT("\t");
			}

			debugf(TEXT("%s%s : %s (%s)"), *TabString,
				InnerObj ? *InnerObj->GetClass()->GetName()	: TEXT("*NULL*"),
				InnerObj ? *InnerObj->GetName()				: TEXT("*NULL*"),
				InnerObj ? *InnerObj->GetPathName()			: TEXT("*NULL*"));

			UPackage* InnerPackage = Cast<UPackage>(InnerObj);
			if (InnerPackage)
			{
				TabCount++;
				ExporterHelper_DumpPackageInners(Context, InnerPackage, TabCount);
				TabCount--;
			}
		}
	}
}

UBOOL ULevelExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	UWorld* World = CastChecked<UWorld>( Object );
	APhysicsVolume* DefaultPhysicsVolume = World->GetDefaultPhysicsVolume();

	for( FObjectIterator It; It; ++It )
	{
		It->ClearFlags( RF_TagImp | RF_TagExp );
	}

	UPackage* MapPackage = NULL;
	if ((PortFlags & PPF_Copy) == 0)
	{
		// If we are not copying to clipboard, then export objects contained in the map package itself...
		MapPackage = Cast<UPackage>(Object->GetOutermost());
	}

	// this is the top level in the .t3d file
	if (MapPackage)
	{
		Ar.Logf(TEXT("%sBegin Map Name=%s\r\n"), appSpc(TextIndent),  *(MapPackage->GetName()));
	}
	else
	{
		Ar.Logf(TEXT("%sBegin Map\r\n"), appSpc(TextIndent));
	}

	// are we exporting all actors or just selected actors?
	UBOOL bAllActors = appStricmp(Type,TEXT("COPY"))!=0 && !bSelectedOnly;

	TextIndent += 3;

	if ((PortFlags & PPF_Copy) == 0)
	{
		// If we are not copying to clipboard, then export objects contained in the map package itself...
		UPackage* MapPackage = Cast<UPackage>(Object->GetOutermost());
		if (MapPackage && ((MapPackage->PackageFlags & PKG_ContainsMap) != 0))
		{
			MapPackage->FullyLoad();

			debugf(TEXT("Exporting objects found in map package: %s"), *(MapPackage->GetName()));
			ExporterHelper_DumpPackageInners(Context, MapPackage, 1);
/***
			// We really only care about the following:
			//	Packages (really groups under the main package)
			//	Materials
			//	MaterialInstanceConstants
			//	MaterialInstanceTimeVarying
			//	Textures
			FExportPackageParams ExpPackageParams;
			ExpPackageParams.RootMapPackageName = MapPackage->GetName();
			ExpPackageParams.Context = Context;
			ExpPackageParams.InPackage = MapPackage;
			ExpPackageParams.Type = Type;
			ExpPackageParams.Ar = &Ar;
			ExpPackageParams.Warn = Warn;
			ExpPackageParams.PortFlags = PortFlags;
			ExpPackageParams.InObject = NULL;
			ExportPackageInners(ExpPackageParams);
***/
			Ar.Logf(TEXT("%sBegin MapPackage\r\n"), appSpc(TextIndent));
				TextIndent += 3;
				UPackageExporterT3D* PackageExp = new UPackageExporterT3D();
				check(PackageExp);
				PackageExp->TextIndent = TextIndent;
				PackageExp->ExportText(Context, MapPackage, TEXT("T3DPKG"), Ar, Warn, PortFlags);
				TextIndent -=3;
			Ar.Logf(TEXT("%sEnd MapPackage\r\n"), appSpc(TextIndent));
		}
	}

	TextIndent -= 3;
	TextIndent += 3;

	ULevel* Level;

	// start a new level section
	if (appStricmp(Type, TEXT("COPY")) == 0)
	{
		// for copy and paste, we want to select actors in the current level
		Level = World->CurrentLevel;

		// if we are copy/pasting, then we don't name the level - we paste into the current level
		Ar.Logf(TEXT("%sBegin Level\r\n"), appSpc(TextIndent));

		// mark that we are doing a clipboard copy
		PortFlags |= PPF_Copy;
	}
	else
	{
		// for export, we only want the persistent level
		Level = World->PersistentLevel;

		//@todo seamless if we are exporting only selected, should we export from all levels? or maybe from the current level?

		// if we aren't copy/pasting, then we name the level so that when we import, we get the same level structure
		Ar.Logf(TEXT("%sBegin Level NAME=%s\r\n"), appSpc(TextIndent), *Level->GetName());
	}

	TextIndent += 3;

	// loop through all of the actors just in this level
	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		AActor* Actor = Level->Actors(iActor);
		// Don't export the default physics volume, as it doesn't have a UModel associated with it
		// and thus will not import properly.
		if ( Actor == DefaultPhysicsVolume )
		{
			continue;
		}
		ATerrain* pkTerrain = Cast<ATerrain>(Actor);
		if (pkTerrain && (bAllActors || pkTerrain->IsSelected()))
		{
			// Terrain exporter...
			// Find the UTerrainExporterT3D exporter?
			UTerrainExporterT3D* pkTerrainExp = ConstructObject<UTerrainExporterT3D>(UTerrainExporterT3D::StaticClass());
			if (pkTerrainExp)
			{
				pkTerrainExp->TextIndent = TextIndent;
				pkTerrainExp->ExportText( Context, pkTerrain, Type, Ar, Warn );
			}
		}
		else // Ensure actor is not a group if grouping is disabled and that the actor is currently selected
			if( Actor && 
			(GEditor->bGroupingActive || !Actor->IsA(AGroupActor::StaticClass())) && 
			( bAllActors || Actor->IsSelected() ) )
		{
			if (Actor->ShouldExport())
			{
				Ar.Logf( TEXT("%sBegin Actor Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
					appSpc(TextIndent), *Actor->GetClass()->GetName(), *Actor->GetName(),
					*Actor->GetArchetype()->GetClass()->GetName(), *Actor->GetArchetype()->GetPathName() );

				ExportObjectInner( Context, Actor, Ar, PortFlags | PPF_ExportsNotFullyQualified );

				Ar.Logf( TEXT("%sEnd Actor\r\n"), appSpc(TextIndent) );
			}
			else
			{
				GEditor->GetSelectedActors()->Deselect( Actor );
			}
		}
	}

	TextIndent -= 3;

	Ar.Logf(TEXT("%sEnd Level\r\n"), appSpc(TextIndent));

	TextIndent -= 3;

	// Export information about the first selected surface in the map.  Used for copying/pasting
	// information from poly to poly.
	Ar.Logf( TEXT("%sBegin Surface\r\n"), appSpc(TextIndent) );
	TCHAR TempStr[256];
	for( INT i=0; i<GWorld->GetModel()->Surfs.Num(); i++ )
	{
		FBspSurf *Poly = &GWorld->GetModel()->Surfs(i);
		if( Poly->PolyFlags&PF_Selected )
		{
			Ar.Logf( TEXT("%sTEXTURE=%s\r\n"), appSpc(TextIndent+3), *Poly->Material->GetPathName() );
			Ar.Logf( TEXT("%sBASE      %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(GWorld->GetModel()->Points(Poly->pBase))) );
			Ar.Logf( TEXT("%sTEXTUREU  %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(GWorld->GetModel()->Vectors(Poly->vTextureU))) );
			Ar.Logf( TEXT("%sTEXTUREV  %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(GWorld->GetModel()->Vectors(Poly->vTextureV))) );
			Ar.Logf( TEXT("%sNORMAL    %s\r\n"), appSpc(TextIndent+3), SetFVECTOR(TempStr,&(GWorld->GetModel()->Vectors(Poly->vNormal))) );
			Ar.Logf( TEXT("%sPOLYFLAGS=%d\r\n"), appSpc(TextIndent+3), Poly->PolyFlags );
			break;
		}
	}
	Ar.Logf( TEXT("%sEnd Surface\r\n"), appSpc(TextIndent) );

	Ar.Logf( TEXT("%sEnd Map\r\n"), appSpc(TextIndent) );


	return 1;
}

void ULevelExporterT3D::ExportComponentExtra( const FExportObjectInnerContext* Context, const TArray<UComponent*>& Components, FOutputDevice& Ar, DWORD PortFlags)
{
	for ( INT ComponentIndex = 0; ComponentIndex < Components.Num(); ComponentIndex++ )
	{
		UComponent* Component = Components(ComponentIndex);
		UActorComponent* ActorComponent = Cast<UActorComponent>(Component);
		if( ActorComponent )
		{
			AActor* ActorOwner = ActorComponent->GetOwner();
			if (ActorOwner != NULL) // might be NULL if referenced but not attached/in Components array
			{
				ULevel* ComponentLevel = Cast<ULevel>(ActorOwner->GetOuter());
				AInstancedFoliageActor* IFA = AInstancedFoliageActor::GetInstancedFoliageActorForLevel(ComponentLevel);
				if( IFA )
				{
					TMap<class UStaticMesh*,TArray<const FFoliageInstancePlacementInfo*> > FoliageInstanceMap = IFA->GetInstancesForComponent(ActorComponent);
					for( TMap<class UStaticMesh*,TArray<const FFoliageInstancePlacementInfo*> > ::TConstIterator It(FoliageInstanceMap); It; ++It )
					{
						Ar.Logf(TEXT("%sBegin Foliage StaticMesh=%s Component=%s%s"),appSpc(TextIndent), *It.Key()->GetPathName(), *ActorComponent->GetInstanceMapName().ToString(), LINE_TERMINATOR);
						const TArray<const FFoliageInstancePlacementInfo*>& FoliageInstances = It.Value();
						for( INT Idx=0;Idx<FoliageInstances.Num();Idx++ )
						{
							const FFoliageInstancePlacementInfo* Inst = FoliageInstances(Idx);
							Ar.Logf(TEXT("%sLocation=%f,%f,%f Rotation=%d,%d,%d PreAlignRotation=%d,%d,%d DrawScale3D=%f,%f,%f Flags=%u%s"),appSpc(TextIndent+3), 
								Inst->Location.X, Inst->Location.Y, Inst->Location.Z, 
								Inst->Rotation.Pitch, Inst->Rotation.Yaw, Inst->Rotation.Roll,
								Inst->PreAlignRotation.Pitch, Inst->PreAlignRotation.Yaw, Inst->PreAlignRotation.Roll,
								Inst->DrawScale3D.X, Inst->DrawScale3D.Y, Inst->DrawScale3D.Z, 
								Inst->Flags,
								LINE_TERMINATOR);									
						}

						Ar.Logf(TEXT("%sEnd Foliage%s"),appSpc(TextIndent), LINE_TERMINATOR);
					}
				}
			}
		}
	}
}

void ULevelExporterT3D::ExportPackageObject(FExportPackageParams& ExpPackageParams){}
void ULevelExporterT3D::ExportPackageInners(FExportPackageParams& ExpPackageParams){}

IMPLEMENT_CLASS(ULevelExporterT3D);

/*------------------------------------------------------------------------------
	ULevelExporterSTL implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void ULevelExporterSTL::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UWorld::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("STL")) );
	new(FormatDescription)FString(TEXT("Stereolithography"));
}
UBOOL ULevelExporterSTL::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	//@todo seamless - this needs to be world, like the t3d version above
	UWorld* World = CastChecked<UWorld>(Object);
	ULevel* Level = World->PersistentLevel;

	for( FObjectIterator It; It; ++It )
		It->ClearFlags( RF_TagImp | RF_TagExp );

	//
	// GATHER TRIANGLES
	//

	TArray<FVector> Triangles;

	for( INT iActor=0; iActor<Level->Actors.Num(); iActor++ )
	{
		// Terrain

		ATerrain* T = Cast<ATerrain>(Level->Actors(iActor));
		if( T && ( !bSelectedOnly || T->IsSelected() ) )
		{
			for( int y = 0 ; y < T->NumVerticesY-1 ; y++ )
			{
				for( int x = 0 ; x < T->NumVerticesX-1 ; x++ )
				{
					FVector P1	= T->GetWorldVertex(x,y);
					FVector P2	= T->GetWorldVertex(x,y+1);
					FVector P3	= T->GetWorldVertex(x+1,y+1);
					FVector P4	= T->GetWorldVertex(x+1,y);

					Triangles.AddItem( P4 );
					Triangles.AddItem( P1 );
					Triangles.AddItem( P2 );

					Triangles.AddItem( P3 );
					Triangles.AddItem( P2 );
					Triangles.AddItem( P4 );
				}
			}
		}

		// Landscape
		ALandscape* Landscape = Cast<ALandscape>(Level->Actors(iActor));
		if( Landscape && ( !bSelectedOnly || Landscape->IsSelected() ) )
		{
			if (Landscape->GetLandscapeInfo())
			{
				// Export data for each component
				for( TMap<QWORD,ULandscapeComponent*>::TIterator It(Landscape->GetLandscapeInfo()->XYtoComponentMap); It; ++It )
				{
					ULandscapeComponent* Component = It.Value();
					FLandscapeComponentDataInterface CDI(Component);

					for( INT y=0;y<Component->ComponentSizeQuads;y++ )
					{
						for( INT x=0;x<Component->ComponentSizeQuads;x++ )
						{
							FVector P00	= CDI.GetWorldVertex(x,y);
							FVector P01	= CDI.GetWorldVertex(x,y+1);
							FVector P11	= CDI.GetWorldVertex(x+1,y+1);
							FVector P10	= CDI.GetWorldVertex(x+1,y);

							// triangulation matches FLandscapeIndexBuffer constructor
							Triangles.AddItem(P00);
							Triangles.AddItem(P11);
							Triangles.AddItem(P10);

							Triangles.AddItem(P00);
							Triangles.AddItem(P01);
							Triangles.AddItem(P11);
						}	
					}
				}		
			}
		}

		// Static meshes

		AStaticMeshActor* Actor = Cast<AStaticMeshActor>(Level->Actors(iActor));
		if( Actor && ( !bSelectedOnly || Actor->IsSelected() ) && Actor->StaticMeshComponent->StaticMesh )
		{
			FStaticMeshRenderData* RenderData = Actor->StaticMeshComponent->StaticMesh->GetLODForExport(0);
			if (RenderData)
			{
				const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) RenderData->RawTriangles.Lock(LOCK_READ_ONLY);
				for( INT tri = 0 ; tri < RenderData->RawTriangles.GetElementCount() ; tri++ )
				{
					for( INT v = 2 ; v > -1 ; v-- )
					{
						FVector vtx = Actor->LocalToWorld().TransformFVector( RawTriangleData[tri].Vertices[v] );
						Triangles.AddItem( vtx );
					}
				}
				RenderData->RawTriangles.Unlock();
			}
		}
	}

	// BSP Surfaces

	for( INT i=0;i<GWorld->GetModel()->Nodes.Num();i++ )
	{
		FBspNode* Node = &GWorld->GetModel()->Nodes(i);
		if( !bSelectedOnly || GWorld->GetModel()->Surfs(Node->iSurf).PolyFlags&PF_Selected )
		{
			if( Node->NumVertices > 2 )
			{
				FVector vtx1(GWorld->GetModel()->Points(GWorld->GetModel()->Verts(Node->iVertPool+0).pVertex)),
					vtx2(GWorld->GetModel()->Points(GWorld->GetModel()->Verts(Node->iVertPool+1).pVertex)),
					vtx3;

				for( INT v = 2 ; v < Node->NumVertices ; v++ )
				{
					vtx3 = GWorld->GetModel()->Points(GWorld->GetModel()->Verts(Node->iVertPool+v).pVertex);

					Triangles.AddItem( vtx1 );
					Triangles.AddItem( vtx2 );
					Triangles.AddItem( vtx3 );

					vtx2 = vtx3;
				}
			}
		}
	}

	//
	// WRITE THE FILE
	//

	Ar.Logf( TEXT("%ssolid LevelBSP\r\n"), appSpc(TextIndent) );

	for( INT tri = 0 ; tri < Triangles.Num() ; tri += 3 )
	{
		FVector vtx[3];
		vtx[0] = Triangles(tri) * FVector(1,-1,1);
		vtx[1] = Triangles(tri+1) * FVector(1,-1,1);
		vtx[2] = Triangles(tri+2) * FVector(1,-1,1);

		FPlane Normal( vtx[0], vtx[1], vtx[2] );

		Ar.Logf( TEXT("%sfacet normal %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+2), Normal.X, Normal.Y, Normal.Z );
		Ar.Logf( TEXT("%souter loop\r\n"), appSpc(TextIndent+4) );

		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[0].X, vtx[0].Y, vtx[0].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[1].X, vtx[1].Y, vtx[1].Z );
		Ar.Logf( TEXT("%svertex %1.6f %1.6f %1.6f\r\n"), appSpc(TextIndent+6), vtx[2].X, vtx[2].Y, vtx[2].Z );

		Ar.Logf( TEXT("%sendloop\r\n"), appSpc(TextIndent+4) );
		Ar.Logf( TEXT("%sendfacet\r\n"), appSpc(TextIndent+2) );
	}

	Ar.Logf( TEXT("%sendsolid LevelBSP\r\n"), appSpc(TextIndent) );

	Triangles.Empty();

	return 1;
}
IMPLEMENT_CLASS(ULevelExporterSTL);






/*------------------------------------------------------------------------------
	Helper classes for render material to texture
	@lmtodo: Replace this with shared Lightmass version when we merge
------------------------------------------------------------------------------*/


/**
* Class for rendering previews of material expressions in the material editor's linked object viewport.
*/
class FExportMaterialProxy : public FMaterial, public FMaterialRenderProxy
{
public:
	FExportMaterialProxy()
	{}

	FExportMaterialProxy(UMaterialInterface* InMaterialInterface, EMaterialProperty InPropertyToCompile) :
	MaterialInterface(InMaterialInterface)
		, PropertyToCompile(InPropertyToCompile)
	{
		// always use high quality for the preview
		CacheShaders(GRHIShaderPlatform, MSQ_HIGH);
	}

	/**
	* Should the shader for this material with the given platform, shader type and vertex 
	* factory type combination be compiled
	*
	* @param Platform		The platform currently being compiled for
	* @param ShaderType	Which shader is being compiled
	* @param VertexFactory	Which vertex factory is being compiled (can be NULL)
	*
	* @return TRUE if the shader should be compiled
	*/
	virtual UBOOL ShouldCache(EShaderPlatform Platform, const FShaderType* ShaderType, const FVertexFactoryType* VertexFactoryType) const
	{
		if (VertexFactoryType == FindVertexFactoryType(FName(TEXT("FLocalVertexFactory"), FNAME_Find)))
		{
			// we only need the non-light-mapped, base pass, local vertex factory shaders for drawing an opaque Material Tile
			// @todo: Added a FindShaderType by fname or something"
			
			if(appStristr(ShaderType->GetName(), TEXT("BasePassVertexShaderFNoLightMapPolicyFNoDensityPolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("BasePassHullShaderFNoLightMapPolicyFNoDensityPolicy")) ||
				appStristr(ShaderType->GetName(), TEXT("BasePassDomainShaderFNoLightMapPolicyFNoDensityPolicy")))
			{
				return TRUE;
			}

			else if(appStristr(ShaderType->GetName(), TEXT("BasePassPixelShaderFNoLightMapPolicy")))
			{
				return TRUE;
			}
		}

		return FALSE;
	}

	////////////////
	// FMaterialRenderProxy interface.
	virtual const FMaterial* GetMaterial() const
	{
		if(GetShaderMap())
		{
			return this;
		}
		else
		{
			return GEngine->DefaultMaterial->GetRenderProxy(FALSE)->GetMaterial();
		}
	}

	virtual UBOOL GetVectorValue(const FName ParameterName, FLinearColor* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetVectorValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetScalarValue(const FName ParameterName, FLOAT* OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetScalarValue(ParameterName, OutValue, Context);
	}

	virtual UBOOL GetTextureValue(const FName ParameterName,const FTexture** OutValue, const FMaterialRenderContext& Context) const
	{
		return MaterialInterface->GetRenderProxy(0)->GetTextureValue(ParameterName,OutValue,Context);
	}

	// Material properties.
	/** Entry point for compiling a specific material property.  This must call SetMaterialProperty. */
	virtual INT CompileProperty(EMaterialProperty Property,FMaterialCompiler* Compiler) const
	{
		Compiler->SetMaterialProperty(Property);
		// MAKE SURE THIS MATCHES THE CHART IN WillFillData
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	SoftMasked  | Diffuse     | 0 (EMPTY)    | Emissive    | Normal    | (Emsv | Diffuse)*Opacity  |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		if( Property == MP_EmissiveColor )
		{
			UMaterial* Material = MaterialInterface->GetMaterial();
			check(Material);

			// If the property is not active, don't compile it
			if (!IsActiveMaterialProperty(Material, PropertyToCompile))
			{
				return INDEX_NONE;
			}

			switch (PropertyToCompile)
			{
			case MP_EmissiveColor:
				// Emissive is ALWAYS returned...
				return Compiler->ForceCast(Material->EmissiveColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
			case MP_DiffuseColor:
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
				}
				break;
			case MP_SpecularColor: 
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Compiler->AppendVector(
						Compiler->ForceCast(Material->SpecularColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE), 
						Compiler->ForceCast(Material->SpecularPower.Compile(Compiler,15.0f),MCT_Float1));
				}
				break;
			case MP_Normal:
				// Only return for Opaque and Masked...
				if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Compiler->ForceCast( Material->Normal.Compile(Compiler, FVector( 0, 0, 1 ) ), MCT_Float3, TRUE, TRUE );
				}
				break;
			case MP_Opacity:
				if (Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
				{
					return Material->OpacityMask.Compile(Compiler, 1.0f);
				}
				else
					if ((Material->BlendMode == BLEND_Modulate) || (Material->BlendMode == BLEND_ModulateAndAdd))
					{
						if (Material->LightingModel == MLM_Unlit)
						{
							return Compiler->ForceCast(Material->EmissiveColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
						else
						{
							return Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
						}
					}
					else
						if ((Material->BlendMode == BLEND_Translucent) || (Material->BlendMode == BLEND_AlphaComposite) || (Material->BlendMode == BLEND_Additive))
						{
							INT ColoredOpacity = -1;
							if (Material->LightingModel == MLM_Unlit)
							{
								ColoredOpacity = Compiler->ForceCast(Material->EmissiveColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
							}
							else
							{
								ColoredOpacity = Compiler->ForceCast(Material->DiffuseColor.Compile(Compiler,FColor(0,0,0)),MCT_Float3,TRUE,TRUE);
							}
							return Compiler->Lerp(Compiler->Constant3(1.0f, 1.0f, 1.0f), ColoredOpacity, Compiler->ForceCast(Material->Opacity.Compile(Compiler,1.0f),MCT_Float1));
						}
						break;
			default:
				return Compiler->Constant(1.0f);
			}

			return Compiler->Constant(0.0f);
		}
		else if (Property == MP_WorldPositionOffset)
		{
			//set to 0 to prevent off by 1 pixel errors
			return Compiler->Constant(0.0f);
		}
		else
		{
			return Compiler->Constant(1.0f);
		}
	}

	virtual FString GetMaterialUsageDescription() const { return FString::Printf(TEXT("FLightmassMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	virtual UBOOL IsTwoSided() const 
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->TwoSided == 1);
		}
		return FALSE;
	}
	virtual UBOOL RenderTwoSidedSeparatePass() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->TwoSidedSeparatePass == 1);
		}
		return FALSE;
	}
	virtual UBOOL RenderLitTranslucencyPrepass() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUseLitTranslucencyDepthPass == 1);
		}
		return FALSE;
	}
	virtual UBOOL RenderLitTranslucencyDepthPostpass() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUseLitTranslucencyPostRenderDepthPass == 1);
		}
		return FALSE;
	}
	virtual UBOOL CastLitTranslucencyShadowAsMasked() const
	{ 
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bCastLitTranslucencyShadowAsMasked == 1);
		}
		return FALSE;
	}
	virtual UBOOL NeedsDepthTestDisabled() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bDisableDepthTest == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsLightFunction() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedAsLightFunction == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsUsedWithFogVolumes() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedWithFogVolumes == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsSpecialEngineMaterial() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedAsSpecialEngineMaterial == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsUsedWithMobileLandscape() const
	{
		return FALSE;
	}
	virtual UBOOL IsTerrainMaterial() const
	{
		return FALSE;
	}
	virtual UBOOL IsDecalMaterial() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->bUsedWithDecals == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsWireframe() const
	{
		UMaterial* Material = MaterialInterface ? MaterialInterface->GetMaterial() : NULL;
		if (Material)
		{
			return (Material->Wireframe == 1);
		}
		return FALSE;
	}
	virtual UBOOL IsDistorted() const								{ return FALSE; }
	virtual UBOOL HasSubsurfaceScattering() const					{ return FALSE; }
	virtual UBOOL HasSeparateTranslucency() const					{ return FALSE; }
	virtual UBOOL IsMasked() const									{ return FALSE; }
	virtual enum EBlendMode GetBlendMode() const					{ return BLEND_Opaque; }
	virtual enum EMaterialLightingModel GetLightingModel() const	{ return MLM_Unlit; }
	virtual FLOAT GetOpacityMaskClipValue() const					{ return 0.5f; }
	virtual FString GetFriendlyName() const { return FString::Printf(TEXT("FLightmassMaterialRenderer %s"), MaterialInterface ? *MaterialInterface->GetName() : TEXT("NULL")); }
	/**
	* Should shaders compiled for this material be saved to disk?
	*/
	virtual UBOOL IsPersistent() const { return FALSE; }

	const UMaterialInterface* GetMaterialInterface() const
	{
		return MaterialInterface;
	}

	friend FArchive& operator<< ( FArchive& Ar, FExportMaterialProxy& V )
	{
		return Ar << V.MaterialInterface;
	}

	/**
	*	Checks if the configuration of the material proxy will generate a uniform
	*	value across the sampling... (Ie, nothing is hooked to the property)
	*
	*	@param	OutUniformValue		The value that will be returned.
	*
	*	@return	UBOOL				TRUE if a single value would be generated.
	*								FALSE if not.
	*/
	UBOOL WillGenerateUniformData(FColor& OutUniformValue)
	{
		// Pre-fill the value...
		OutUniformValue.R = 0;
		OutUniformValue.G = 0;
		OutUniformValue.B = 0;
		OutUniformValue.A = 0;

		UMaterial* Material = MaterialInterface->GetMaterial();
		check(Material);
		UBOOL bExpressionIsNULL = FALSE;
		switch (PropertyToCompile)
		{
		case MP_EmissiveColor:
			// Emissive is ALWAYS returned...
			bExpressionIsNULL = (Material->EmissiveColor.Expression == NULL);
			break;
		case MP_DiffuseColor:
			// Only return for Opaque and Masked...
			if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				bExpressionIsNULL = (Material->DiffuseColor.Expression == NULL);
			}
			break;
		case MP_SpecularColor: 
			// Only return for Opaque and Masked...
			if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				bExpressionIsNULL = (Material->SpecularColor.Expression == NULL);
				OutUniformValue.A = 255;
			}
			break;
		case MP_Normal:
			// Only return for Opaque and Masked...
			if (Material->BlendMode == BLEND_Opaque || Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				bExpressionIsNULL = (Material->Normal.Expression == NULL);
				OutUniformValue.B = 1.0f;	// Default normal is (0,0,1)
			}
			break;
		case MP_Opacity:
			if (Material->BlendMode == BLEND_Masked || Material->BlendMode == BLEND_SoftMasked)
			{
				bExpressionIsNULL = (Material->OpacityMask.Expression == NULL);
				OutUniformValue.R = 255;
				OutUniformValue.G = 255;
				OutUniformValue.B = 255;
				OutUniformValue.A = 255;
			}
			else
				if ((Material->BlendMode == BLEND_Modulate) ||
					(Material->BlendMode == BLEND_ModulateAndAdd) ||
					(Material->BlendMode == BLEND_Translucent) || 
					(Material->BlendMode == BLEND_AlphaComposite) || 
					(Material->BlendMode == BLEND_Additive))
				{
					UBOOL bColorInputIsNULL = FALSE;
					if (Material->LightingModel == MLM_Unlit)
					{
						bColorInputIsNULL = Material->EmissiveColor.Expression == NULL;
					}
					else
					{
						bColorInputIsNULL = Material->DiffuseColor.Expression == NULL;
					}
					if (Material->BlendMode == BLEND_Translucent
						|| Material->BlendMode == BLEND_AlphaComposite
						|| Material->BlendMode == BLEND_Additive)
					{
						bExpressionIsNULL = bColorInputIsNULL && Material->Opacity.Expression == NULL;
					}
					else
					{
						bExpressionIsNULL = bColorInputIsNULL;
					}
				}
				break;
		}

		return bExpressionIsNULL;
	}

	/**
	* Iterate through all textures used by the material and return the maximum texture resolution used
	* (ideally this could be made dependent of the material property)
	*
	* @param MaterialInterface The material to scan for texture size
	*
	* @return Size (width and height)
	*/
	FIntPoint FindMaxTextureSize(UMaterialInterface* MaterialInterface, FIntPoint MinimumSize = FIntPoint(1, 1)) const
	{
		// static lod settings so that we only initialize them once
		static FTextureLODSettings GameTextureLODSettings;
		static UBOOL bAreLODSettingsInitialized = FALSE;
		if (!bAreLODSettingsInitialized)
		{
			// initialize LOD settings with game texture resolutions, since we don't want to use 
			// potentially bloated editor settings
			GameTextureLODSettings.Initialize(GSystemSettingsIni, TEXT("SystemSettings"));
		}

		TArray<UTexture*> MaterialTextures;

		MaterialInterface->GetUsedTextures(MaterialTextures);

		// find the largest texture in the list (applying it's LOD bias)
		FIntPoint MaxSize = MinimumSize;
		for (INT TexIndex = 0; TexIndex < MaterialTextures.Num(); TexIndex++)
		{
			UTexture* Texture = MaterialTextures(TexIndex);

			if (Texture == NULL)
			{
				continue;
			}

			// get the max size of the texture
			FIntPoint LocalSize(0, 0);
			if (Texture->IsA(UTexture2D::StaticClass()))
			{
				UTexture2D* Tex2D = (UTexture2D*)Texture;
				LocalSize = FIntPoint(Tex2D->SizeX, Tex2D->SizeY);
			}
			else if (Texture->IsA(UTextureCube::StaticClass()))
			{
				UTextureCube* TexCube = (UTextureCube*)Texture;
				LocalSize = FIntPoint(TexCube->SizeX, TexCube->SizeY);
			}

			INT LocalBias = GameTextureLODSettings.CalculateLODBias(Texture);

			// bias the texture size based on LOD group
			FIntPoint BiasedLocalSize(LocalSize.X >> LocalBias, LocalSize.Y >> LocalBias);

			MaxSize.X = Max(BiasedLocalSize.X, MaxSize.X);
			MaxSize.Y = Max(BiasedLocalSize.Y, MaxSize.Y);
		}

		return MaxSize;
	}

	static UBOOL WillFillData(EBlendMode InBlendMode, EMaterialProperty InMaterialProperty)
	{
		// MAKE SURE THIS MATCHES THE CHART IN CompileProperty
		// 						  RETURNED VALUES (F16 'textures')
		// 	BLEND MODE  | DIFFUSE     | SPECULAR     | EMISSIVE    | NORMAL    | TRANSMISSIVE              |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|
		// 	Opaque      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | 0 (EMPTY)                 |
		// 	Masked      | Diffuse     | Spec,SpecPwr | Emissive    | Normal    | Opacity Mask              |
		// 	Translucent | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Additive    | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | (Emsv | Diffuse)*Opacity  |
		// 	Modulative  | 0 (EMPTY)   | 0 (EMPTY)    | Emissive    | 0 (EMPTY) | Emsv | Diffuse            |
		// 	SoftMasked  | Diffuse     | 0 (EMPTY)    | Emissive    | Normal    | (Emsv | Diffuse)*Opacity  |
		// 	------------+-------------+--------------+-------------+-----------+---------------------------|

		if (InMaterialProperty == MP_EmissiveColor)
		{
			return TRUE;
		}

		switch (InBlendMode)
		{
		case BLEND_Opaque:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return TRUE;
				case MP_SpecularColor:	return TRUE;
				case MP_Normal:			return TRUE;
				case MP_Opacity:		return FALSE;
				}
			}
			break;
		case BLEND_Masked:
		case BLEND_SoftMasked:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return TRUE;
				case MP_SpecularColor:	return TRUE;
				case MP_Normal:			return TRUE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		case BLEND_AlphaComposite:
		case BLEND_Translucent:
		case BLEND_DitheredTranslucent:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return FALSE;
				case MP_SpecularColor:	return FALSE;
				case MP_Normal:			return FALSE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		case BLEND_Additive:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return FALSE;
				case MP_SpecularColor:	return FALSE;
				case MP_Normal:			return FALSE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		case BLEND_Modulate:
		case BLEND_ModulateAndAdd:
			{
				switch (InMaterialProperty)
				{
				case MP_DiffuseColor:	return FALSE;
				case MP_SpecularColor:	return FALSE;
				case MP_Normal:			return FALSE;
				case MP_Opacity:		return TRUE;
				}
			}
			break;
		}
		return FALSE;
	}

private:
	/** The material interface for this proxy */
	UMaterialInterface* MaterialInterface;
	/** The property to compile for rendering the sample */
	EMaterialProperty PropertyToCompile;
};

/**
 * Render a material to a render target
 *
 */
UBOOL GenerateExportMaterialPropertyData(
		UMaterialInterface* InMaterial, EMaterialProperty InMaterialProperty, 
		INT& InOutSizeX, INT& InOutSizeY, 
		TArray<FColor>& OutBMP,
		UTextureRenderTarget2D*& RenderTarget,
		FCanvas*& Canvas)
{
	FExportMaterialProxy* MaterialProxy = new FExportMaterialProxy(InMaterial, InMaterialProperty);
	if (MaterialProxy == NULL)
	{
		warnf(NAME_Warning, TEXT("Failed to create FExportMaterialProxy!"));
		return FALSE;
	}

	UBOOL bNormalmap = (InMaterialProperty == MP_Normal);

	FReadSurfaceDataFlags ReadPixelFlags(bNormalmap ? RCM_SNorm : RCM_UNorm);

	UBOOL bResult = TRUE;

	FColor UniformValue;
	if (MaterialProxy->WillGenerateUniformData(UniformValue) == FALSE)
	{
		//@lmtodo. The format may be determined by the material property...
		// For example, if Diffuse doesn't need to be F16 it can create a standard RGBA8 target.
		EPixelFormat Format = PF_FloatRGB;

		FIntPoint MaxTextureSize = MaterialProxy->FindMaxTextureSize(InMaterial);
		InOutSizeX = MaxTextureSize.X;
		InOutSizeY = MaxTextureSize.Y;

		UBOOL bForceLinear = bNormalmap;

		if(RenderTarget && 
			((RenderTarget->bForceLinearGamma != bForceLinear) || (RenderTarget->Format != Format) || (RenderTarget->SizeX != InOutSizeX) || (RenderTarget->SizeY != InOutSizeY))
			)
		{
			RenderTarget->RemoveFromRoot();
			RenderTarget = NULL;
			delete Canvas;
			Canvas = NULL;
		}

		if(!RenderTarget)
		{
			RenderTarget = new UTextureRenderTarget2D();
			check(RenderTarget);
			RenderTarget->AddToRoot();
			RenderTarget->ClearColor = FLinearColor(0.0f, 0.0f, 0.0f, 0.0f);
			RenderTarget->Init(InOutSizeX, InOutSizeY, Format, bForceLinear);

			Canvas = new FCanvas(RenderTarget->GetRenderTargetResource(), NULL);
			check(Canvas);
		}

		// start drawing to the render target
		Canvas->SetRenderTarget(RenderTarget->GetRenderTargetResource());
		// Freeze time while capturing the material's inputs
		::DrawTile(Canvas,0,0,InOutSizeX,InOutSizeY,0,0,1,1,MaterialProxy,TRUE);
		Canvas->Flush();
		FlushRenderingCommands();
		Canvas->SetRenderTarget(NULL);
		FlushRenderingCommands();

		// if PF_FloatRGB was used as render target format, the gamma conversion during rendering is deactivated
		// if we want it not in linear space we need to convert
		ReadPixelFlags.SetLinearToGamma(Format == PF_FloatRGB && !bForceLinear);

		if(!RenderTarget->GetRenderTargetResource()->ReadPixels(OutBMP, ReadPixelFlags))
		{
			bResult = FALSE;
		}
	}
	else
	{
		// Single value... fill it in.
		InOutSizeX = 1;
		InOutSizeY = 1; 
		OutBMP.Empty(1);
		OutBMP.AddZeroed(1);
		OutBMP(0) = UniformValue;
	}

	return bResult;
}




/*------------------------------------------------------------------------------
	Helper classes for the OBJ exporters.
------------------------------------------------------------------------------*/

// An individual face.

class FOBJFace
{
public:
	// index into FOBJGeom::VertexData (local within FOBJGeom)
	UINT VertexIndex[3];
	/** List of vertices that make up this face. */

	/** The material that was applied to this face. */
	UMaterialInterface* Material;
};

class FOBJVertex
{
public:
	// position
	FVector Vert;
	// texture coordiante
	FVector2D UV;
	// normal
	FVector Normal;
	//	FLinearColor Colors[3];
};

// A geometric object.  This will show up as a separate object when imported into a modeling program.
class FOBJGeom
{
public:
	/** List of faces that make up this object. */
	TArray<FOBJFace> Faces;

	/** Vertex positions that make up this object. */
	TArray<FOBJVertex> VertexData;

	/** Name used when writing this object to the OBJ file. */
	FString Name;

	// Constructors.
	FORCEINLINE FOBJGeom( const FString& InName )
		:	Name( InName )
	{}
};

inline FString FixupMaterialName(UMaterialInterface* Material)
{
	return Material->GetPathName().Replace(TEXT("."), TEXT("_")).Replace(TEXT(":"), TEXT("_"));
}


/**
 * Adds the given actor's mesh to the GOBJObjects array if possible
 * 
 * @param Actor The actor to export
 * @param Objects The array that contains cached exportable object data
 * @param Materials Optional set of materials to gather all used materials by the objects (currently only StaticMesh materials are supported)
 */
static void AddActorToOBJs(AActor* Actor, TArray<FOBJGeom*>& Objects, TSet<UMaterialInterface*>* Materials, UBOOL bSelectedOnly)
{
	FMatrix LocalToWorld = Actor->LocalToWorld();

	// Terrain

	ATerrain* T = Cast<ATerrain>(Actor);
	if( T )
	{
		FOBJGeom* OBJGeom = new FOBJGeom( Actor->GetName() );

//		OBJGeom->Faces.Add((T->NumVerticesX-1) * (T->NumVerticesY-1) * 2);

		// Take holes into account...
		INT PossibleFaceCount = (T->NumVerticesX-1) * (T->NumVerticesY-1) * 2;
		INT HoleCount = 0;
		for( int y = 0 ; y < T->NumVerticesY-1 ; y++ )
		{
			for( int x = 0 ; x < T->NumVerticesX-1 ; x++ )
			{
				if (T->IsTerrainQuadVisible(x, y) == FALSE)
				{
					HoleCount++;
				}
			}
		}

		INT ActualFaceCount = PossibleFaceCount - (2 * HoleCount);
		
		OBJGeom->Faces.Add(ActualFaceCount);
		// each quad has 2 trinagles so we need 6 vertices (can be optimized to 4 or even further)
		OBJGeom->VertexData.Add(ActualFaceCount * 3);

		INT CurrentFace = 0;
		UINT CurrentVertex = 0;
		for( int y = 0 ; y < T->NumVerticesY - 1 ; y++ )
		{
			for( int x = 0 ; x < T->NumVerticesX - 1 ; x++ )
			{
				if (T->IsTerrainQuadVisible(x,y) == TRUE)
				{
					FVector P1	= T->GetWorldVertex(x,y);
					FVector P2	= T->GetWorldVertex(x,y + 1);
					FVector P3	= T->GetWorldVertex(x + 1,y + 1);
					FVector P4	= T->GetWorldVertex(x + 1,y);

					// find the face
					FOBJFace& OBJFace = OBJGeom->Faces(CurrentFace);
					OBJFace.Material = NULL;
					FOBJFace& OBJFace2 = OBJGeom->Faces(CurrentFace + 1);
					OBJFace2.Material = NULL;

					UBOOL bIsFlipped = T->IsTerrainQuadFlipped(x,y);

					OBJFace.VertexIndex[0] = CurrentVertex;
					OBJFace.VertexIndex[1] = CurrentVertex + 1;
					OBJFace.VertexIndex[2] = CurrentVertex + 2;
					OBJFace2.VertexIndex[0] = CurrentVertex + 3;
					OBJFace2.VertexIndex[1] = CurrentVertex + 4;
					OBJFace2.VertexIndex[2] = CurrentVertex + 5;

					FOBJVertex* Vert = &OBJGeom->VertexData(CurrentVertex);

					// the diagonal can be flipped in one way or another
					if (bIsFlipped)
					{
						Vert[0].Vert = FVector( P2.X, P2.Y, P2.Z );
						Vert[1].Vert = FVector( P3.X, P3.Y, P3.Z );
						Vert[2].Vert = FVector( P1.X, P1.Y, P1.Z );

						Vert[3].Vert = FVector( P4.X, P4.Y, P4.Z );
						Vert[4].Vert = FVector( P1.X, P1.Y, P1.Z );
						Vert[5].Vert = FVector( P3.X, P3.Y, P3.Z );
					}
					else
					{
						Vert[0].Vert = FVector( P2.X, P2.Y, P2.Z );
						Vert[1].Vert = FVector( P4.X, P4.Y, P4.Z );
						Vert[2].Vert = FVector( P1.X, P1.Y, P1.Z );

						Vert[3].Vert = FVector( P4.X, P4.Y, P4.Z );
						Vert[4].Vert = FVector( P2.X, P2.Y, P2.Z );
						Vert[5].Vert = FVector( P3.X, P3.Y, P3.Z );
					}
					
					FVector NormalA = (P1 - P2) ^ (P3 - P2);
					FVector NormalB = (P3 - P4) ^ (P1 - P4);
					FVector Normal = (NormalA + NormalB).SafeNormal();

					for(UINT e = 0; e < 6; ++e)
					{
						// Texture UV is not yet supported
						Vert[e].UV = FVector2D( 0, 1 );
						// Normal in world space
						Vert[e].Normal = Normal;
					}

					CurrentFace += 2;
					CurrentVertex += 6;
				}
			}
		}

		Objects.AddItem( OBJGeom );
	}


	// Landscape
	ALandscape* Landscape = Cast<ALandscape>( Actor );
	ULandscapeInfo* LandscapeInfo = Landscape ? Landscape->GetLandscapeInfo() : NULL;
	if( Landscape && LandscapeInfo )
	{
		// Export data for each component
		for( TMap<QWORD,ULandscapeComponent*>::TIterator It(LandscapeInfo->XYtoComponentMap); It; ++It )
		{
			if (bSelectedOnly && LandscapeInfo->SelectedComponents.Num() && !LandscapeInfo->SelectedComponents.Contains(It.Value()))
			{
				continue;
			}
			ULandscapeComponent* Component = It.Value();
			FLandscapeComponentDataInterface CDI(Component);

			FOBJGeom* OBJGeom = new FOBJGeom( Component->GetName() );
			OBJGeom->VertexData.AddZeroed( Square(Component->ComponentSizeQuads+1) );
			OBJGeom->Faces.AddZeroed( Square(Component->ComponentSizeQuads) * 2 );

			// Check if there is any holes
			BYTE* VisDataMap = NULL;
			INT TexIndex = INDEX_NONE;
			INT WeightMapSize = (Component->SubsectionSizeQuads + 1) * Component->NumSubsections;
			INT ChannelOffsets[4] = {STRUCT_OFFSET(FColor,R),STRUCT_OFFSET(FColor,G),STRUCT_OFFSET(FColor,B),STRUCT_OFFSET(FColor,A)};

			for( INT AllocIdx=0;AllocIdx < Component->WeightmapLayerAllocations.Num(); AllocIdx++ )
			{
				FWeightmapLayerAllocationInfo& AllocInfo = Component->WeightmapLayerAllocations(AllocIdx);
				if( AllocInfo.LayerName == ALandscape::DataWeightmapName )
				{
					TexIndex = AllocInfo.WeightmapTextureIndex;
					VisDataMap = (BYTE*)Component->WeightmapTextures(TexIndex)->Mips(0).Data.Lock(LOCK_READ_ONLY) + ChannelOffsets[AllocInfo.WeightmapTextureChannel];
				}
			}

			// Export verts
			FOBJVertex* Vert = &OBJGeom->VertexData(0);
			for( INT y=0;y<Component->ComponentSizeQuads+1;y++ )
			{
				for( INT x=0;x<Component->ComponentSizeQuads+1;x++ )
				{					
					FVector WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ;

					CDI.GetWorldPositionTangents( x, y, WorldPos, WorldTangentX, WorldTangentY, WorldTangentZ );

					Vert->Vert = WorldPos;
					Vert->UV = FVector2D( Component->SectionBaseX + x, Component->SectionBaseY + y );
					Vert->Normal = WorldTangentZ;
					Vert++;
				}
			}

			INT VisThreshold = 170;
			INT SubNumX, SubNumY, SubX, SubY;

			FOBJFace* Face = &OBJGeom->Faces(0);
			for( INT y=0;y<Component->ComponentSizeQuads;y++ )
			{
				for( INT x=0;x<Component->ComponentSizeQuads;x++ )
				{
					CDI.ComponentXYToSubsectionXY(x, y, SubNumX, SubNumY, SubX, SubY );
					INT WeightIndex = SubX + SubNumX*(Component->SubsectionSizeQuads + 1) + (SubY+SubNumY*(Component->SubsectionSizeQuads + 1))*WeightMapSize;

					UBOOL bInvisible = VisDataMap && VisDataMap[WeightIndex * sizeof(FColor)] >= VisThreshold;
					// triangulation matches FLandscapeIndexBuffer constructor
					Face->VertexIndex[0] = (x+0) + (y+0) * (Component->ComponentSizeQuads+1);
					Face->VertexIndex[1] = bInvisible ? Face->VertexIndex[0] : (x+1) + (y+1) * (Component->ComponentSizeQuads+1);
					Face->VertexIndex[2] = bInvisible ? Face->VertexIndex[0] : (x+1) + (y+0) * (Component->ComponentSizeQuads+1);
					Face++;

					Face->VertexIndex[0] = (x+0) + (y+0) * (Component->ComponentSizeQuads+1);
					Face->VertexIndex[1] = bInvisible ? Face->VertexIndex[0] : (x+0) + (y+1) * (Component->ComponentSizeQuads+1);
					Face->VertexIndex[2] = bInvisible ? Face->VertexIndex[0] : (x+1) + (y+1) * (Component->ComponentSizeQuads+1);
					Face++;
				}	
			}

			if (VisDataMap && TexIndex >= 0)
			{
				Component->WeightmapTextures(TexIndex)->Mips(0).Data.Unlock();
			}

			Objects.AddItem( OBJGeom );
		}		
	}


	// Static mesh components

	UStaticMeshComponent* StaticMeshComponent = NULL;
	UStaticMesh* StaticMesh = NULL;

	for( INT j=0; j<Actor->AllComponents.Num(); j++ )
	{
		// If its a static mesh component, with a static mesh

		UActorComponent* Comp = Actor->AllComponents(j);
		StaticMeshComponent = Cast<UStaticMeshComponent>(Comp);
		if( StaticMeshComponent && StaticMeshComponent->StaticMesh )
		{
			LocalToWorld = StaticMeshComponent->LocalToWorld;
			StaticMesh = StaticMeshComponent->StaticMesh;
			break;
		}
	}

	// If this is actually a fracture static mesh, get a pointer to the source static mesh for exporting.  Exporting the fracturestaticmesh itself
	// results in a hugely bloated file with tons of unnecessary triangles.

	AFracturedStaticMeshActor* FracActor = Cast<AFracturedStaticMeshActor>( Actor );
	if( FracActor )
	{
		UFracturedStaticMesh* FracMesh = CastChecked<UFracturedStaticMesh>(StaticMeshComponent->StaticMesh);
		StaticMesh = FracMesh->SourceStaticMesh;
	}

	if( StaticMeshComponent && StaticMesh )
	{
		// is the lightmap a vertex lightmap?
		const FLightMap1D* Lightmap1D = NULL;//StaticMeshComponent->LODData(0).LightMap->GetLightMap1D();
		FQuantizedSimpleLightSample* Lightmap1DData = NULL;
		FVector4 LightmapScale;
		// if so, get the lightmap data
		if (Lightmap1D)
		{
			Lightmap1DData = Lightmap1D->BeginAccessToSimpleLightSamples();
			LightmapScale = Lightmap1D->GetSimpleLightmapQuantizationScale();
		}
		
		// is it a texture lightmap?
		const FLightMap2D* Lightmap2D = NULL;//StaticMeshComponent->LODData(0).LightMap->GetLightMap2D();
		

		FStaticMeshRenderData* RenderData = StaticMesh->GetLODForExport(0);
		if (RenderData)
		{
			const FStaticMeshTriangle* RawTriangleData = (const FStaticMeshTriangle*)RenderData->RawTriangles.Lock(LOCK_READ_ONLY);

			// make room for the faces
			FOBJGeom* OBJGeom = new FOBJGeom( Actor->GetName() );

			UINT NumIndices = RenderData->IndexBuffer.Indices.Num();

			// 3 indices for each triangle
			check(NumIndices % 3 == 0);
			UINT TriangleCount = NumIndices / 3;
			OBJGeom->Faces.Add(TriangleCount);
		
			UINT VertexCount = RenderData->PositionVertexBuffer.GetNumVertices();
			OBJGeom->VertexData.Add(VertexCount);
			FOBJVertex* VerticesOut = &OBJGeom->VertexData(0);

			check(VertexCount == RenderData->VertexBuffer.GetNumVertices());

			FMatrix LocalToWorldInverseTranspose = LocalToWorld.Inverse().Transpose();
			for(UINT i = 0; i < VertexCount; i++)
			{
				// Vertices
				VerticesOut[i].Vert = LocalToWorld.TransformFVector( RenderData->PositionVertexBuffer.VertexPosition(i) );
				// UVs from channel 0
				VerticesOut[i].UV = RenderData->VertexBuffer.GetVertexUV(i, 0);
				// Normal
				VerticesOut[i].Normal = LocalToWorldInverseTranspose.TransformNormal(RenderData->VertexBuffer.VertexTangentZ(i));
			}

			UBOOL bFlipCullMode = LocalToWorld.RotDeterminant() < 0.0f;

			UINT CurrentTriangleId = 0;
			for(INT ElementIndex = 0;ElementIndex < RenderData->Elements.Num(); ++ElementIndex)
			{
				FStaticMeshElement& Element = RenderData->Elements(ElementIndex);
				UMaterialInterface* Material = 0;

				// Get the material for this triangle by first looking at the material overrides array and if that is NULL by looking at the material array in the original static mesh
				if(StaticMeshComponent)
				{
					Material = StaticMeshComponent->GetMaterial(ElementIndex);
				}
			
				if(!Material)
				{
					Material = Element.Material;
				}

				// cache the set of needed materials if desired
				if(Materials && Material)
				{
					Materials->Add(Material);
				}

				WORD* Indices = (WORD*)RenderData->IndexBuffer.Indices.GetData();

				for(UINT i = 0; i < Element.NumTriangles; i++)
				{
					FOBJFace& OBJFace = OBJGeom->Faces(CurrentTriangleId++);

					UINT a = Indices[Element.FirstIndex + i * 3 + 0];
					UINT b = Indices[Element.FirstIndex + i * 3 + 1];
					UINT c = Indices[Element.FirstIndex + i * 3 + 2];

					if(bFlipCullMode)
					{
						Swap(a, c);
					}

					OBJFace.VertexIndex[0] = a;
					OBJFace.VertexIndex[1] = b; 
					OBJFace.VertexIndex[2] = c;

					// Material
					OBJFace.Material = Material;
	/*
					// add colors if it's vertex lit
					if (Lightmap1D)
					{
						for (INT VertexIndex = 0; VertexIndex < 3; VertexIndex++)
						{
							// get the quantized sample
							const FQuantizedSimpleLightSample& Sample = Lightmap1DData[i * 3 + (2 - VertexIndex)];

							// dequantize it
							FLinearColor Color;
							Color.R = appPow(Sample.Coefficients[0].R, 2.2f) * LightmapScale.X;
							Color.G = appPow(Sample.Coefficients[0].G, 2.2f) * LightmapScale.Y;
							Color.B = appPow(Sample.Coefficients[0].B, 2.2f) * LightmapScale.Z;

							// add it to the list of colors
							OBJFace.Colors[VertexIndex] = Color;
						}
					}
	*/
				}
			}

			// free up the lightmap data
			if (Lightmap1D)
			{
				Lightmap1D->EndAccessToSimpleLightSamples(Lightmap1DData);
			}

			RenderData->RawTriangles.Unlock();

			Objects.AddItem( OBJGeom );	
		}
	}
}


// @param Material must not be 0
// @param MatProp e.g. MP_DiffuseColor
static void ExportMaterialPropertyTexture(
	const FFilename &BMPFilename, 
	UMaterialInterface* Material, 
	const EMaterialProperty MatProp,
	UTextureRenderTarget2D*& RenderTarget,
	FCanvas*& Canvas
	)
{
	check(Material);

	// make the BMP for the diffuse channel
	TArray<FColor> OutputBMP;
	INT SizeX = 1024;
	INT SizeY = 1024;

	UBOOL bIsValidMaterial = FExportMaterialProxy::WillFillData((EBlendMode)(Material->GetMaterial()->BlendMode), MatProp);

	if (bIsValidMaterial)
	{
		// make space for the bmp
		OutputBMP.Add(SizeX * SizeY);

		// render the material to a texture to export as a bmp
		if (!GenerateExportMaterialPropertyData(Material, MatProp, SizeX, SizeY, OutputBMP, RenderTarget, Canvas))
		{
			bIsValidMaterial = FALSE;
		}
	}

	// make invalid textures a solid red
	if (!bIsValidMaterial)
	{
		SizeX = SizeY = 1;
		OutputBMP.Empty();
		OutputBMP.AddItem(FColor(255, 0, 0, 255));
	}

	// export the diffuse channel bmp
	appCreateBitmap(*BMPFilename, SizeX, SizeY, &OutputBMP(0), GFileManager);
}

/**
 * Exports the GOBJObjects array to the given Archive
 *
 * @param FileAr The main archive to output device. However, if MemAr exists, it will write to that until and flush it out for each object
 * @param MemAr Optional string output device for caching writes to
 * @param Warn Feedback context for updating status
 * @param OBJFilename Name of the main OBJ file to export to, used for tagalong files (.mtl, etc)
 * @param Objects The list of objects to export
 * @param Materials Optional list of materials to export
 */
void ExportOBJs(FOutputDevice& FileAr, FStringOutputDevice* MemAr, FFeedbackContext* Warn, const FString& OBJFilename, TArray<FOBJGeom*>& Objects, const TSet<UMaterialInterface*>* Materials, UINT &IndexOffset)
{
	// write to the memory archive if it exists, otherwise use the FileAr
	FOutputDevice& Ar = MemAr ? *MemAr : FileAr;

	// export extra material info if we added any
	if (Materials)
	{
		// stop the rendering thread so we can easily render to texture
		SCOPED_SUSPEND_RENDERING_THREAD(FSuspendRenderingThread::ST_RecreateThread);

		// make a .MTL file next to the .obj file that contains the materials
		FFilename MaterialLibFilename = FFilename(OBJFilename).GetBaseFilename(FALSE) + TEXT(".mtl");

		// use the output device file, just like the Exporter makes for the .obj, no backup
		FOutputDeviceFile* MaterialLib = new FOutputDeviceFile(*MaterialLibFilename, TRUE);
		MaterialLib->SetSuppressEventTag(TRUE);
		MaterialLib->SetAutoEmitLineTerminator(FALSE);

		UTextureRenderTarget2D* RenderTarget = NULL;
		FCanvas* Canvas = NULL;

		// export the material set to a mtllib
		INT MaterialIndex = 0;
		for (TSet<UMaterialInterface*>::TConstIterator It(*Materials); It; ++It, ++MaterialIndex)
		{
			UMaterialInterface* Material = *It;
			FString MaterialName = FixupMaterialName(Material);

			// export the material info
			MaterialLib->Logf(TEXT("newmtl %s\r\n"), *MaterialName);

			{
				FFilename BMPFilename = MaterialLibFilename.GetPath() * MaterialName + TEXT("_D.bmp");
				ExportMaterialPropertyTexture(BMPFilename, Material, MP_DiffuseColor, RenderTarget, Canvas);
				MaterialLib->Logf(TEXT("\tmap_Kd %s\r\n"), *BMPFilename.GetCleanFilename());
			}

			{
				FFilename BMPFilename = MaterialLibFilename.GetPath() * MaterialName + TEXT("_S.bmp");
				ExportMaterialPropertyTexture(BMPFilename, Material, MP_SpecularColor, RenderTarget, Canvas);
				MaterialLib->Logf(TEXT("\tmap_Ks %s\r\n"), *BMPFilename.GetCleanFilename());
			}

			{
				FFilename BMPFilename = MaterialLibFilename.GetPath() * MaterialName + TEXT("_N.bmp");
				ExportMaterialPropertyTexture(BMPFilename, Material, MP_Normal, RenderTarget, Canvas);
				MaterialLib->Logf(TEXT("\tbump %s\r\n"), *BMPFilename.GetCleanFilename());
			}

			MaterialLib->Logf(TEXT("\r\n"));
		}
		
		if(RenderTarget)
		{
			RenderTarget->RemoveFromRoot();
			RenderTarget = NULL;
			delete Canvas;
			Canvas = NULL;
		}

		MaterialLib->TearDown();
		delete MaterialLib;

		Ar.Logf(TEXT("mtllib %s\n"), *MaterialLibFilename.GetCleanFilename());
	}

	for( INT o = 0 ; o < Objects.Num() ; ++o )
	{
		FOBJGeom* object = Objects(o);
		UMaterialInterface* CurrentMaterial = NULL;

		// Object header

		Ar.Logf( TEXT("g %s\n"), *object->Name );
		Ar.Logf( TEXT("\n") );

		// Verts

		for( INT f = 0 ; f < object->VertexData.Num() ; ++f )
		{
			const FOBJVertex& vertex = object->VertexData(f);
			const FVector& vtx = vertex.Vert;

			Ar.Logf( TEXT("v %.4f %.4f %.4f\n"), vtx.X, vtx.Z, vtx.Y );
		}

		Ar.Logf( TEXT("\n") );

		// Texture coordinates

		for( INT f = 0 ; f < object->VertexData.Num() ; ++f )
		{
			const FOBJVertex& face = object->VertexData(f);
			const FVector2D& uv = face.UV;

			Ar.Logf( TEXT("vt %.4f %.4f\n"), uv.X, 1.0f - uv.Y );
		}

		Ar.Logf( TEXT("\n") );

		// Normals

		for( INT f = 0 ; f < object->VertexData.Num() ; ++f )
		{
			const FOBJVertex& face = object->VertexData(f);
			const FVector& Normal = face.Normal;

			Ar.Logf( TEXT("vn %.3f %.3f %.3f\n"), Normal.X, Normal.Z, Normal.Y );
		}

		Ar.Logf( TEXT("\n") );
		
		// Faces

		for( INT f = 0 ; f < object->Faces.Num() ; ++f )
		{
			const FOBJFace& face = object->Faces(f);

			if( face.Material != CurrentMaterial )
			{
				CurrentMaterial = face.Material;
				Ar.Logf( TEXT("usemtl %s\n"), *FixupMaterialName(face.Material));
			}

			Ar.Logf( TEXT("f ") );

			for( INT v = 0 ; v < 3 ; ++v )
			{
				// +1 as Wavefront files are 1 index based
				UINT VertexIndex = IndexOffset + face.VertexIndex[v] + 1; 
				Ar.Logf( TEXT("%d/%d/%d "), VertexIndex, VertexIndex, VertexIndex);
			}

			Ar.Logf( TEXT("\n") );
		}

		IndexOffset += object->VertexData.Num();

		Ar.Logf( TEXT("\n") );

		// dump to disk so we don't run out of memory ganging up all objects
		if (MemAr)
		{
			FileAr.Log(*MemAr);
			FileAr.Flush();
			MemAr->Empty();
		}

		// we are now done with the object, free it
		delete object;
		Objects(o) = NULL;
	}
}

/**
 * Compiles the selection order array by putting every geometry object
 * with a valid selection index into the array, and then sorting it.
 */

static int CDECL MaterialCompare(const void *InA, const void *InB)
{
	PTRINT A = (PTRINT)(((FOBJFace*)InA)->Material);
	PTRINT B = (PTRINT)(((FOBJFace*)InB)->Material);

	return (A == B) ? 0 : ((A < B) ? -1 : 1);
}


/*------------------------------------------------------------------------------
	ULevelExporterLOD implementation.
------------------------------------------------------------------------------*/
/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void ULevelExporterLOD::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UWorld::StaticClass();
	bText = 1;
	bForceFileOperations = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("LOD.OBJ")) );
	new(FormatDescription)FString(TEXT("Object File for LOD"));
}

UBOOL ULevelExporterLOD::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& FileAr, FFeedbackContext* Warn, DWORD PortFlags)
{
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ExportingLevelToLOD OBJ")), TRUE );

	// containers to hold exportable objects and their materials
	TArray<FOBJGeom*> Objects;
	TSet<UMaterialInterface*> Materials;

	UWorld* World = CastChecked<UWorld>(Object);

	// write to memory to buffer file writes
	FStringOutputDevice Ar;

	// OBJ file header
	Ar.Logf (TEXT("# LOD OBJ File Generated by UnrealEd\n"));
	Ar.Logf( TEXT("\n") );

	TArray<AActor*> ActorsToExport;
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		// only export selected actors if the flag is set
		if( !Actor || (bSelectedOnly && !Actor->IsSelected()))
		{
			continue;
		}

		ActorsToExport.AddItem(Actor);
	}

	// Export actors
	UINT IndexOffset = 0;
	for (INT Index = 0; Index < ActorsToExport.Num(); ++Index)
	{
		AActor* Actor = ActorsToExport(Index);
		Warn->StatusUpdatef( Index, ActorsToExport.Num(), *LocalizeUnrealEd(TEXT("ExportingLevelToOBJ")) );

		// for now, only export static mesh actors
		if (Cast<AStaticMeshActor>(Actor) == NULL)
		{
			continue;
		}

		// export any actor that passes the tests
		AddActorToOBJs(Actor, Objects, &Materials, bSelectedOnly);

		for( INT o = 0 ; o < Objects.Num() ; ++o )
		{
			FOBJGeom* object = Objects(o);
			appQsort( &object->Faces(0), object->Faces.Num(), sizeof(FOBJFace), (QSORT_COMPARE)MaterialCompare );
		}

		// Export to the OBJ file
		ExportOBJs(FileAr, &Ar, Warn, CurrentFilename, Objects, &Materials, IndexOffset);
		Objects.Reset();
	}

	// OBJ file footer
	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO DOL\n"));

	GWarn->EndSlowTask();

	// dump the rest to the file
	FileAr.Log(*Ar);

	return TRUE;
}

IMPLEMENT_CLASS(ULevelExporterLOD);

/*------------------------------------------------------------------------------
	ULevelExporterOBJ implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void ULevelExporterOBJ::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UWorld::StaticClass();
	bText = 1;
	bForceFileOperations = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("OBJ")) );
	new(FormatDescription)FString(TEXT("Object File"));
}
static void ExportPolys( UPolys* Polys, INT &PolyNum, INT TotalPolys, FOutputDevice& Ar, FFeedbackContext* Warn, UBOOL bSelectedOnly, UModel* Model, TArray<FOBJGeom*>& Objects )
{
	FOBJGeom* OBJGeom = new FOBJGeom( TEXT("BSP") );

	for( INT i=0; i<Model->Nodes.Num(); i++ )
	{
		FBspNode* Node = &Model->Nodes(i);
		FBspSurf& Surf = Model->Surfs(Node->iSurf);

		if( (Surf.PolyFlags & PF_Selected) || !bSelectedOnly )
		{
			const FVector& TextureBase = Model->Points(Surf.pBase);
			const FVector& TextureX = Model->Vectors(Surf.vTextureU);
			const FVector& TextureY = Model->Vectors(Surf.vTextureV);
			const FVector& Normal = Model->Vectors(Surf.vNormal);

			FPoly Poly;
			GEditor->polyFindMaster( Model, Node->iSurf, Poly );

			// Triangulate this node and generate an OBJ face from the vertices.
			for(INT StartVertexIndex = 1;StartVertexIndex < Node->NumVertices-1;StartVertexIndex++)
			{
				INT TriangleIndex = OBJGeom->Faces.AddZeroed();
				FOBJFace& OBJFace = OBJGeom->Faces(TriangleIndex);
				INT VertexIndex = OBJGeom->VertexData.AddZeroed(3);
				FOBJVertex* Vertices = &OBJGeom->VertexData(VertexIndex);

				OBJFace.VertexIndex[0] = VertexIndex;
				OBJFace.VertexIndex[1] = VertexIndex + 1;
				OBJFace.VertexIndex[2] = VertexIndex + 2;

				// These map the node's vertices to the 3 triangle indices to triangulate the convex polygon.
				INT TriVertIndices[3] = { Node->iVertPool,
										  Node->iVertPool + StartVertexIndex,
										  Node->iVertPool + StartVertexIndex + 1 };

				for(UINT TriVertexIndex = 0; TriVertexIndex < 3; TriVertexIndex++)
				{
					const FVert& Vert = Model->Verts(TriVertIndices[TriVertexIndex]);
					const FVector& Vertex = Model->Points(Vert.pVertex);

					FLOAT U = ((Vertex - TextureBase) | TextureX) / 128.0f;
					FLOAT V = ((Vertex - TextureBase) | TextureY) / 128.0f;

					Vertices[TriVertexIndex].Vert = Vertex;
					Vertices[TriVertexIndex].UV = FVector2D( U, V );
					Vertices[TriVertexIndex].Normal = Normal;
				}
			}
		}
	}

	// Save the object representing the BSP into the OBJ pool
	if( OBJGeom->Faces.Num() > 0 )
	{
		Objects.AddItem( OBJGeom );
	}
	else
	{
		delete OBJGeom;
	}
}

UBOOL ULevelExporterOBJ::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& FileAr, FFeedbackContext* Warn, DWORD PortFlags)
{
	TSet<UMaterialInterface*> GlobalMaterials;
	TSet<UMaterialInterface*> *Materials = 0;

	INT YesNoCancelReply = appMsgf(AMT_YesNoCancel, *LocalizeUnrealEd("Prompt_OBJExportWithBMP"));

	switch (YesNoCancelReply)
	{
		case 0: // Yes
			Materials = &GlobalMaterials;
			break;

		case 1: // No
			break;

		case 2: // Cancel
			return 1;
	}


	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ExportingLevelToOBJ")), TRUE );

	// container to hold all exportable objects
	TArray<FOBJGeom*> Objects;

	UWorld* World = CastChecked<UWorld>(Object);

	GEditor->bspBuildFPolys(World->GetModel(), 0, 0 );
	UPolys* Polys = World->GetModel()->Polys;

	// write to memory to buffer file writes
	FStringOutputDevice Ar;

	// OBJ file header

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));
	Ar.Logf( TEXT("\n") );

	UINT IndexOffset = 0;
	// Export the BSP

	INT Dummy;
    ExportPolys( Polys, Dummy, 0, Ar, Warn, bSelectedOnly, World->GetModel(), Objects );
	// Export polys to the OBJ file
	ExportOBJs(FileAr, &Ar, Warn, CurrentFilename, Objects, NULL, IndexOffset);
	Objects.Reset();
	// Export actors
	
	TArray<AActor*> ActorsToExport;
	for( FActorIterator It; It; ++It )
	{
		AActor* Actor = *It;
		// only export selected actors if the flag is set
		if( !Actor || (bSelectedOnly && !Actor->IsSelected()))
		{
			continue;
		}

		ActorsToExport.AddItem(Actor);
	}

	for( INT Index = 0; Index < ActorsToExport.Num(); ++Index )
	{
		AActor* Actor = ActorsToExport(Index);
		Warn->StatusUpdatef( Index, ActorsToExport.Num(), *LocalizeUnrealEd(TEXT("ExportingLevelToOBJ")) );

		// try to export every object
		AddActorToOBJs(Actor, Objects, Materials, bSelectedOnly);

		for( INT o = 0 ; o < Objects.Num() ; ++o )
		{
			FOBJGeom* object = Objects(o);
			appQsort( &object->Faces(0), object->Faces.Num(), sizeof(FOBJFace), (QSORT_COMPARE)MaterialCompare );
		}
	}

	// Export to the OBJ file
	ExportOBJs(FileAr, &Ar, Warn, CurrentFilename, Objects, Materials, IndexOffset);
	Objects.Reset();

	// OBJ file footer
	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));

	GWarn->EndSlowTask();

	// write anything left in the memory Ar to disk
	FileAr.Log(*Ar);

	return 1;
}

IMPLEMENT_CLASS(ULevelExporterOBJ);

/*------------------------------------------------------------------------------
	UPackageExporterT3D implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UPackageExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UPackage::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3DPKG")) );
	new(FormatDescription)FString(TEXT("Unreal package text"));
}

UBOOL UPackageExporterT3D::ExportText( const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	UPackage* Package = Cast<UPackage>(Object);
	// this is the top level in the .t3d file
	if (Package == NULL)
	{
		return FALSE;
	}

	if (Package != Package->GetOutermost())
	{
		warnf(TEXT("PackageExporterT3D: Only supports exporting TopLevelPackages!"));
		return FALSE;
	}

	Ar.Logf( TEXT("%sBegin TopLevelPackage Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
		appSpc(TextIndent), 
		*(Object->GetClass()->GetName()), *(Object->GetName()),
		*(Object->GetArchetype()->GetClass()->GetName()), *(Object->GetArchetype()->GetPathName()));
		TextIndent += 3;

		ExportProperties(Context, Ar, Object->GetClass(), (BYTE*)Object, TextIndent, Object->GetArchetype()->GetClass(), (BYTE*)Object->GetArchetype(), Object, PortFlags);


		TextIndent -= 3;
	Ar.Logf( TEXT("%sEnd TopLevelPackage\r\n"), appSpc(TextIndent) );

	// We really only care about the following:
	//	Packages (really groups under the main package)
	//	Materials
	//	MaterialInstanceConstants
	//	MaterialInstanceTimeVarying
	//	Textures
	FExportPackageParams ExpPackageParams;
	ExpPackageParams.RootMapPackageName = Package->GetName();
	ExpPackageParams.Context = Context;
	ExpPackageParams.InPackage = Package;
	ExpPackageParams.Type = Type;
	ExpPackageParams.Ar = &Ar;
	ExpPackageParams.Warn = Warn;
	ExpPackageParams.PortFlags = PortFlags;
	ExpPackageParams.InObject = NULL;
	ExportPackageInners(ExpPackageParams);

	return 1;
}

/*------------------------------------------------------------------------------
	ULevelExporterFBX implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void ULevelExporterFBX::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UWorld::StaticClass();
	bText = 0;
	bForceFileOperations = 0;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("FBX")) );
	new(FormatDescription)FString(TEXT("FBX File"));
}

UBOOL ULevelExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
#if WITH_FBX
	GWarn->BeginSlowTask( *LocalizeUnrealEd(TEXT("ExportingLevelToFBX")), TRUE );

	UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();
	Exporter->CreateDocument();

	if (bSelectedOnly)
	{
		UWorld* World = CastChecked<UWorld>(Object);

		Exporter->ExportBSP( World->GetModel(), TRUE );

		for( FActorIterator It; It; ++It )
		{
			AActor* Actor = *It;
			// only export selected actors if the flag is set
			if( Actor && Actor->IsA(AActor::StaticClass()) && Actor->IsSelected() )
			{
				if (Actor->IsA(AStaticMeshActor::StaticClass()))
				{
					Exporter->ExportStaticMesh(Actor, ((AStaticMeshActor*)Actor)->StaticMeshComponent, NULL);
				}
				else if (Actor->IsA(ADynamicSMActor::StaticClass()))
				{
					Exporter->ExportStaticMesh(Actor, ((ADynamicSMActor*)Actor)->StaticMeshComponent, NULL );
				}
			}
		}
	}
	else
	{
		UWorld* World = CastChecked<UWorld>(Object);
		ULevel* Level = World->PersistentLevel;

		Exporter->ExportLevelMesh( Level, NULL );

		// Export streaming levels and actors
		for( INT CurLevelIndex = 0; CurLevelIndex < World->Levels.Num(); ++CurLevelIndex )
		{
			ULevel* CurLevel = World->Levels( CurLevelIndex );
			if( CurLevel != NULL && CurLevel != Level )
			{
				Exporter->ExportLevelMesh( CurLevel, NULL );
			}
		}
	}
	Exporter->WriteToFile(*UExporter::CurrentFilename);

	GWarn->EndSlowTask();
#endif

	return TRUE;
}

IMPLEMENT_CLASS(ULevelExporterFBX);

void UPackageExporterT3D::ExportPackageObject(FExportPackageParams& ExpPackageParams)
{
	UObject* ExpObject = ExpPackageParams.InObject;
	FOutputDevice& Ar = *(ExpPackageParams.Ar);

	FString PackageName = ExpPackageParams.InPackage ? *(ExpPackageParams.InPackage->GetName()) : TEXT("***NULL***");
	if (ExpObject->IsA(UPackage::StaticClass()))
	{
		Ar.Logf( TEXT("%sBegin Package ParentPackage=%s Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
			appSpc(TextIndent), *PackageName,
			*(ExpObject->GetClass()->GetName()), *(ExpObject->GetName()),
			*(ExpObject->GetArchetype()->GetClass()->GetName()), *(ExpObject->GetArchetype()->GetPathName()));

//			ExportObjectInner( ExpPackageParams.Context, ExpObject, Ar, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified, TRUE );
//			UExporter::ExportToOutputDevice(ExpPackageParams.Context, ExpObject, NULL, Ar, ExpPackageParams.Type, TextIndent+3, ExpPackageParams.PortFlags);
			// export the object's properties - don't want to use ExportObjectInner or ExportToOutputDevice as it will 
			// write out all of the contained items!
			TextIndent += 3;
			ExportProperties(ExpPackageParams.Context, Ar, ExpObject->GetClass(), (BYTE*)ExpObject, TextIndent, ExpObject->GetArchetype()->GetClass(), (BYTE*)ExpObject->GetArchetype(), ExpObject, ExpPackageParams.PortFlags);
			TextIndent -= 3;

		Ar.Logf( TEXT("%sEnd Package\r\n"), appSpc(TextIndent) );
	}
	else
	if (
		(ExpObject->IsA(UMaterialInstanceConstant::StaticClass())) || 
		(ExpObject->IsA(UMaterialInstanceTimeVarying::StaticClass()) ||
		(ExpObject->IsA(UPhysicalMaterial::StaticClass())))
		)
	{
		Ar.Logf( TEXT("%sBegin PackageObject ParentPackage=%s Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
			appSpc(TextIndent), *PackageName,
			*(ExpObject->GetClass()->GetName()), *(ExpObject->GetName()),
			*(ExpObject->GetArchetype()->GetClass()->GetName()), *(ExpObject->GetArchetype()->GetPathName()));

			ExportObjectInner( ExpPackageParams.Context, ExpObject, Ar, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified, TRUE );

		Ar.Logf( TEXT("%sEnd PackageObject\r\n"), appSpc(TextIndent) );
	}
	else
	if (ExpObject->IsA(UMaterial::StaticClass()))
	{
		UMaterialExporterT3D* MatExporter = NULL;
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if (*It == UMaterialExporterT3D::StaticClass())
			{
				MatExporter = ConstructObject<UMaterialExporterT3D>(*It);
				break;
			}
		}

		if (MatExporter)
		{
			Ar.Logf( TEXT("%sBegin PackageMaterial ParentPackage=%s") LINE_TERMINATOR, appSpc(TextIndent), *PackageName);
			TextIndent += 3;

				MatExporter->TextIndent = TextIndent;
				MatExporter->ExportText(ExpPackageParams.Context, ExpObject, TEXT("T3D"), Ar, 
					ExpPackageParams.Warn, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified);

			TextIndent -= 3;
			Ar.Logf( TEXT("%sEnd PackageMaterial\r\n"), appSpc(TextIndent) );
		}
		else
		{
			Ar.Logf( TEXT("%sBegin PackageObject ParentPackage=%s Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
				appSpc(TextIndent), *PackageName,
				*(ExpObject->GetClass()->GetName()), *(ExpObject->GetName()),
				*(ExpObject->GetArchetype()->GetClass()->GetName()), *(ExpObject->GetArchetype()->GetPathName()));

				ExportObjectInner( ExpPackageParams.Context, ExpObject, Ar, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified, TRUE );

			Ar.Logf( TEXT("%sEnd PackageObject\r\n"), appSpc(TextIndent) );
		}
	}
	else
	if (ExpObject->IsA(UStaticMesh::StaticClass()))
	{		
		UStaticMeshExporterT3D* SMExporter = NULL;
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if (*It == UStaticMeshExporterT3D::StaticClass())
			{
				SMExporter = ConstructObject<UStaticMeshExporterT3D>(*It);
				break;
			}
		}

		if (SMExporter)
		{
			Ar.Logf( TEXT("%sBegin PackageStaticMesh ParentPackage=%s") LINE_TERMINATOR, appSpc(TextIndent), *PackageName);
			TextIndent += 3;

				SMExporter->TextIndent = TextIndent;
				SMExporter->ExportText(ExpPackageParams.Context, ExpObject, TEXT("T3D"), Ar, 
					ExpPackageParams.Warn, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified);

			TextIndent -= 3;
			Ar.Logf( TEXT("%sEnd PackageStaticMesh\r\n"), appSpc(TextIndent) );
		}
		else
		{
			Ar.Logf( TEXT("%sBegin PackageObject ParentPackage=%s Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
				appSpc(TextIndent), *PackageName,
				*(ExpObject->GetClass()->GetName()), *(ExpObject->GetName()),
				*(ExpObject->GetArchetype()->GetClass()->GetName()), *(ExpObject->GetArchetype()->GetPathName()));

				ExportObjectInner( ExpPackageParams.Context, ExpObject, Ar, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified, TRUE );

			Ar.Logf( TEXT("%sEnd PackageObject\r\n"), appSpc(TextIndent) );
		}
	}
	else
	if (
		((ExpObject->IsA(UTexture2D::StaticClass())) ||
		(ExpObject->IsA(UTextureCube::StaticClass()))) &&
		(!ExpObject->IsA(ULightMapTexture2D::StaticClass()))
		)
	{
		UTextureExporterT3D* TextureExporter = NULL;
		for( TObjectIterator<UClass> It ; It ; ++It )
		{
			if (*It == UTextureExporterT3D::StaticClass())
			{
				TextureExporter = ConstructObject<UTextureExporterT3D>(*It);
				break;
			}
		}

		if (TextureExporter)
		{
			Ar.Logf( TEXT("%sBegin PackageTexture ParentPackage=%s") LINE_TERMINATOR, appSpc(TextIndent), *PackageName);
			TextIndent += 3;

				TextureExporter->TextIndent = TextIndent;
				TextureExporter->ExportText(ExpPackageParams.Context, ExpObject, TEXT("T3D"), Ar, 
					ExpPackageParams.Warn, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified);

			TextIndent -= 3;
			Ar.Logf( TEXT("%sEnd PackageTexture\r\n"), appSpc(TextIndent) );
		}
		else
		{
			Ar.Logf( TEXT("%sBegin PackageObject ParentPackage=%s Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
				appSpc(TextIndent), *PackageName,
				*(ExpObject->GetClass()->GetName()), *(ExpObject->GetName()),
				*(ExpObject->GetArchetype()->GetClass()->GetName()), *(ExpObject->GetArchetype()->GetPathName()));

				ExportObjectInner( ExpPackageParams.Context, ExpObject, Ar, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified, TRUE );

			Ar.Logf( TEXT("%sEnd PackageObject\r\n"), appSpc(TextIndent) );
		}
	}
	else
	{
#if 0
		// We don't want to just export any object at this particular time...
		Ar.Logf( TEXT("%sBegin PackageObject ParentPackage=%s Class=%s Name=%s Archetype=%s'%s'") LINE_TERMINATOR, 
			appSpc(TextIndent), *PackageName,
			*(ExpObject->GetClass()->GetName()), *(ExpObject->GetName()),
			*(ExpObject->GetArchetype()->GetClass()->GetName()), *(ExpObject->GetArchetype()->GetPathName()));

			ExportObjectInner( ExpPackageParams.Context, ExpObject, Ar, ExpPackageParams.PortFlags | PPF_ExportsNotFullyQualified, TRUE );

		Ar.Logf( TEXT("%sEnd PackageObject\r\n"), appSpc(TextIndent) );
#else
		// To see what objects are getting skipped, uncomment this line
//		warnf(TEXT("Skipping unsupported object: %s - %s"), 
//			*(ExpObject->GetClass()->GetName()), *(ExpObject->GetPathName()));
#endif
	}
}

void UPackageExporterT3D::ExportPackageInners(FExportPackageParams& ExpPackageParams)
{
	const TArray<UObject*>* Inners = ExpPackageParams.Context->GetObjectInners(ExpPackageParams.InPackage);
	if (Inners)
	{
		for (INT InnerIndex = 0; InnerIndex < Inners->Num(); InnerIndex++)
		{
			UObject* InnerObj = (*Inners)(InnerIndex);
			if (InnerObj)
			{
				FExportPackageParams NewParams(ExpPackageParams);
				NewParams.InObject = InnerObj;
				ExportPackageObject(NewParams);
				UPackage* InnerPackage = Cast<UPackage>(InnerObj);
				if (InnerPackage)
				{
//					TextIndent += 3;
					FExportPackageParams NewParams(ExpPackageParams);
					NewParams.InPackage = InnerPackage;
					ExportPackageInners(NewParams);
//					TextIndent -= 3;
				}
			}
		}
	}
}

IMPLEMENT_CLASS(UPackageExporterT3D);

/*------------------------------------------------------------------------------
	UPolysExporterOBJ implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UPolysExporterOBJ::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UPolys::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("OBJ")) );
	new(FormatDescription)FString(TEXT("Object File"));
}
UBOOL UPolysExporterOBJ::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	TArray<FOBJGeom*> Objects;

    UPolys* Polys = CastChecked<UPolys> (Object);

    INT PolyNum = 0;
    INT TotalPolys = Polys->Element.Num();

	Ar.Logf (TEXT("# OBJ File Generated by UnrealEd\n"));

    ExportPolys( Polys, PolyNum, TotalPolys, Ar, Warn, false, NULL, Objects );

	for( INT o = 0 ; o < Objects.Num() ; ++o )
	{
		FOBJGeom* object = Objects(o);
		appQsort( &object->Faces(0), object->Faces.Num(), sizeof(FOBJFace), (QSORT_COMPARE)MaterialCompare );
	}

	UINT IndexOffset = 0;
	// Export to the OBJ file
	ExportOBJs(Ar, NULL, Warn, CurrentFilename, Objects, NULL, IndexOffset);

	Ar.Logf (TEXT("# dElaernU yb detareneG eliF JBO\n"));

	return 1;
}

IMPLEMENT_CLASS(UPolysExporterOBJ);

/*------------------------------------------------------------------------------
	USequenceExporterT3D implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void USequenceExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = USequence::StaticClass();
	bText = TRUE;

	PreferredFormatIndex = FormatExtension.AddItem(TEXT("T3D"));
	FormatDescription.AddItem(TEXT("Unreal sequence text"));

	FormatExtension.AddItem(TEXT("COPY"));
	FormatDescription.AddItem(TEXT("Unreal sequence text"));
}

UBOOL USequenceExporterT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	USequence* Sequence = CastChecked<USequence>( Object );
	if ( TextIndent == 0 )
	{
		Sequence->ClearExternalVariableNameUsage(Sequence, REN_ForceNoResetLoaders);
	}

	const UBOOL bAsSingle = (appStricmp(Type,TEXT("T3D"))==0);

	// If exporting everything - just pass in the sequence.
	if ( bAsSingle )
	{
		EmitBeginObject(Ar, Sequence, PortFlags);
			ExportObjectInner( Context, Sequence, Ar, PortFlags );
		EmitEndObject(Ar);
	}
	// If we want only a selection, iterate over to find the SequenceObjects we want.
	else
	{
		for(INT i=0; i<Sequence->SequenceObjects.Num(); i++)
		{
			USequenceObject* SeqObj = Sequence->SequenceObjects(i);
			if( SeqObj && SeqObj->IsSelected() )
			{
				EmitBeginObject(Ar, SeqObj, PortFlags);

				// when we export sequences in this sequence, we don't want to count on selection, we want all objects to be exported
				// and PPF_Copy will only exported selected objects
				ExportObjectInner( Context, SeqObj, Ar, PortFlags & ~PPF_Copy );

				EmitEndObject(Ar);
			}
		}
	}

	return true;
}

IMPLEMENT_CLASS(USequenceExporterT3D);

/*------------------------------------------------------------------------------
UStaticMeshExporterOBJ implementation.
------------------------------------------------------------------------------*/
/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void UStaticMeshExporterOBJ::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UStaticMesh::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("OBJ")) );
	new(FormatDescription)FString(TEXT("Object File"));
}
UBOOL UStaticMeshExporterOBJ::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( Object );
	FStaticMeshRenderData* RenderData = StaticMesh->GetLODForExport(0);

	if (RenderData)
	{
		// Create a new filename for the lightmap coordinate OBJ file (by adding "_UV1" to the end of the filename)
		FString Filename = UExporter::CurrentFilename.Left( UExporter::CurrentFilename.Len() - 4 ) + "_UV1." + UExporter::CurrentFilename.Right( 3 );

		// Open a second archive here so we can export lightmap coordinates at the same time we export the regular mesh
		FArchive* UV1File = GFileManager->CreateFileWriter( *Filename, FILEWRITE_AllowRead );

		TArray<FVector> Verts;				// The verts in the mesh
		TArray<FVector2D> UVs;				// UV coords from channel 0
		TArray<FVector2D> UVLMs;			// Lightmap UVs from channel 1
		TArray<FVector> Normals;			// Normals
		TArray<DWORD> SmoothingMasks;		// Complete collection of the smoothing groups from all triangles
		TArray<DWORD> UniqueSmoothingMasks;	// Collection of the unique smoothing groups (used when writing out the face info into the OBJ file so we can group by smoothing group)

		UV1File->Logf( TEXT("# UnrealEd OBJ exporter\r\n") );
		Ar.Log( TEXT("# UnrealEd OBJ exporter\r\n") );

		UINT Count = RenderData->RawTriangles.GetElementCount();

		// Collect all the data about the mesh
		Verts.Reserve(3 * Count);
		UVs.Reserve(3 * Count);
		UVLMs.Reserve(3 * Count);
		Normals.Reserve(3 * Count);
		SmoothingMasks.Reserve(Count);
		UniqueSmoothingMasks.Reserve(Count);

		const FStaticMeshTriangle* RawTriangleData = (FStaticMeshTriangle*) RenderData->RawTriangles.Lock(LOCK_READ_ONLY);
		for( UINT tri = 0 ; tri < Count ; tri++ )
		{
			// Vertices
			Verts.AddItem( RawTriangleData[tri].Vertices[0] );
			Verts.AddItem( RawTriangleData[tri].Vertices[1] );
			Verts.AddItem( RawTriangleData[tri].Vertices[2] );

			// UVs from channel 0
			UVs.AddItem( RawTriangleData[tri].UVs[0][0] );
			UVs.AddItem( RawTriangleData[tri].UVs[1][0] );
			UVs.AddItem( RawTriangleData[tri].UVs[2][0] );

			// UVs from channel 1 (lightmap coords)
			UVLMs.AddItem( RawTriangleData[tri].UVs[0][1] );
			UVLMs.AddItem( RawTriangleData[tri].UVs[1][1] );
			UVLMs.AddItem( RawTriangleData[tri].UVs[2][1] );

			// Normals
			Normals.AddItem( RawTriangleData[tri].TangentZ[0] );
			Normals.AddItem( RawTriangleData[tri].TangentZ[1] );
			Normals.AddItem( RawTriangleData[tri].TangentZ[2] );

			// Smoothing groups
			SmoothingMasks.AddItem( RawTriangleData[tri].SmoothingMask );

			// Unique smoothing groups
			UniqueSmoothingMasks.AddUniqueItem( RawTriangleData[tri].SmoothingMask );
		}
		RenderData->RawTriangles.Unlock();

		// Write out the vertex data

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		for( INT v = 0 ; v < Verts.Num() ; ++v )
		{
			// Transform to Lightwave's coordinate system
			UV1File->Logf( TEXT("v %f %f %f\r\n"), Verts(v).X, Verts(v).Z, Verts(v).Y );
			Ar.Logf( TEXT("v %f %f %f\r\n"), Verts(v).X, Verts(v).Z, Verts(v).Y );
		}

		// Write out the UV data (the lightmap file differs here in that it writes from the UVLMs array instead of UVs)

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		for( INT uv = 0 ; uv < UVs.Num() ; ++uv )
		{
			// Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
			UV1File->Logf( TEXT("vt %f %f\r\n"), UVLMs(uv).X, 1.0f - UVLMs(uv).Y );
			Ar.Logf( TEXT("vt %f %f\r\n"), UVs(uv).X, 1.0f - UVs(uv).Y );
		}

		// Write object header

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		UV1File->Logf( TEXT("g UnrealEdObject\r\n") );
		Ar.Log( TEXT("g UnrealEdObject\r\n") );
		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );

		// Write out the face windings, sectioned by unique smoothing groups

		INT SmoothingGroup = 0;

		for( INT sm = 0 ; sm < UniqueSmoothingMasks.Num() ; ++sm )
		{
			UV1File->Logf( TEXT("s %i\r\n"), SmoothingGroup );
			Ar.Logf( TEXT("s %i\r\n"), SmoothingGroup );
			SmoothingGroup++;

			for( INT tri = 0 ; tri < RenderData->RawTriangles.GetElementCount() ; tri++ )
			{
				if( SmoothingMasks(tri) == UniqueSmoothingMasks(sm)  )
				{
					int idx = 1 + (tri * 3);

					UV1File->Logf( TEXT("f %d/%d %d/%d %d/%d\r\n"), idx, idx, idx+1, idx+1, idx+2, idx+2 );
					Ar.Logf( TEXT("f %d/%d %d/%d %d/%d\r\n"), idx, idx, idx+1, idx+1, idx+2, idx+2 );
				}
			}
		}

		// Write out footer

		UV1File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		UV1File->Logf( TEXT("g\r\n") );
		Ar.Log( TEXT("g\r\n") );

		// Clean up and finish
		delete UV1File;
	}

	// ------------------------------------------------------

	// 
	if (RenderData)
	{
		// Create a new filename for the internal OBJ file (by adding "_Internal" to the end of the filename)
		FString Filename = UExporter::CurrentFilename.Left( UExporter::CurrentFilename.Len() - 4 ) + "_Internal." + UExporter::CurrentFilename.Right( 3 );

		// Open another archive
		FArchive* File = GFileManager->CreateFileWriter( *Filename, FILEWRITE_AllowRead );

		File->Logf( TEXT("# UnrealEd OBJ exporter (_Internal)\r\n") );

		UINT VertexCount = RenderData->PositionVertexBuffer.GetNumVertices();

		check(VertexCount == RenderData->VertexBuffer.GetNumVertices());

		File->Logf( TEXT("\r\n") );
		for(UINT i = 0; i < VertexCount; i++)
		{
			const FVector& OSPos = RenderData->PositionVertexBuffer.VertexPosition( i );
//			const FVector WSPos = StaticMeshComponent->LocalToWorld.TransformFVector( OSPos );
			const FVector WSPos = OSPos;

			// Transform to Lightwave's coordinate system
			File->Logf( TEXT("v %f %f %f\r\n"), WSPos.X, WSPos.Z, WSPos.Y );
		}

		File->Logf( TEXT("\r\n") );
		for(UINT i = 0 ; i < VertexCount; ++i)
		{
			// takes the first UV
			const FVector2D UV = RenderData->VertexBuffer.GetVertexUV(i, 0);

			// Invert the y-coordinate (Lightwave has their bitmaps upside-down from us).
			File->Logf( TEXT("vt %f %f\r\n"), UV.X, 1.0f - UV.Y );
		}

		File->Logf( TEXT("\r\n") );
		Ar.Log( TEXT("\r\n") );
		for(UINT i = 0 ; i < VertexCount; ++i)
		{
			const FVector& OSNormal = RenderData->VertexBuffer.VertexTangentZ( i ); 
			const FVector WSNormal = OSNormal; 

			// Transform to Lightwave's coordinate system
			File->Logf( TEXT("vn %f %f %f\r\n"), WSNormal.X, WSNormal.Z, WSNormal.Y );
		}

		{
			WORD* Indices = (WORD*)RenderData->IndexBuffer.Indices.GetData();
			UINT NumIndices = RenderData->IndexBuffer.Indices.Num();

			check(NumIndices % 3 == 0);
			for(UINT i = 0; i < NumIndices / 3; i++)
			{
				// Wavefront indices are 1 based
				UINT a = Indices[3 * i + 0] + 1;
				UINT b = Indices[3 * i + 1] + 1;
				UINT c = Indices[3 * i + 2] + 1;

				File->Logf( TEXT("f %d/%d/%d %d/%d/%d %d/%d/%d\r\n"), 
					a,a,a,
					b,b,b,
					c,c,c);
			}
		}

		delete File;
	}


	return TRUE;
}

IMPLEMENT_CLASS(UStaticMeshExporterOBJ);

/*------------------------------------------------------------------------------
UStaticMeshExporterFBX implementation.
------------------------------------------------------------------------------*/
/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void UStaticMeshExporterFBX::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UStaticMesh::StaticClass();
	bText = 0;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("FBX")) );
	new(FormatDescription)FString(TEXT("FBX File"));
}
UBOOL UStaticMeshExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
#if WITH_FBX
	UStaticMesh* StaticMesh = CastChecked<UStaticMesh>( Object );
	UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();
	Exporter->CreateDocument();
	Exporter->ExportStaticMesh(StaticMesh);
	Exporter->WriteToFile(*UExporter::CurrentFilename);
#endif

	return TRUE;
}

IMPLEMENT_CLASS(UStaticMeshExporterFBX);

/*------------------------------------------------------------------------------
USkeletalMeshExporterFBX implementation.
------------------------------------------------------------------------------*/
/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void USkeletalMeshExporterFBX::InitializeIntrinsicPropertyValues()
{
	SupportedClass = USkeletalMesh::StaticClass();
	bText = 0;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("FBX")) );
	new(FormatDescription)FString(TEXT("FBX File"));
}
UBOOL USkeletalMeshExporterFBX::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
#if WITH_FBX
	USkeletalMesh* SkeletalMesh = CastChecked<USkeletalMesh>( Object );
	UnFbx::CFbxExporter* Exporter = UnFbx::CFbxExporter::GetInstance();
	Exporter->CreateDocument();
	Exporter->ExportSkeletalMesh(SkeletalMesh);
	Exporter->WriteToFile(*UExporter::CurrentFilename);
#endif

	return TRUE;
}

IMPLEMENT_CLASS(USkeletalMeshExporterFBX);

/*-----------------------------------------------------------------------------
	'Extended' T3D exporters...
-----------------------------------------------------------------------------*/
IMPLEMENT_CLASS(UExporterT3DX)

void UExporterT3DX::ExportObjectStructProperty(const FString& PropertyName, const FString& ScriptStructName, BYTE* DataValue, UObject* ParentObject, FOutputDevice& Ar, DWORD PortFlags)
{
	UScriptStruct* TheStruct = FindField<UScriptStruct>(UObject::StaticClass(), *ScriptStructName);
	check(TheStruct);
	FString DataString;
	UStructProperty_ExportTextItem(TheStruct, DataString, DataValue, NULL, ParentObject, PortFlags);
	Ar.Logf(TEXT("%s%s=%s") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName, *DataString);
}

void UExporterT3DX::ExportBooleanProperty(const FString& PropertyName, UBOOL BooleanValue, FOutputDevice& Ar, DWORD PortFlags)
{
	TCHAR* Temp =	(TCHAR*) ((PortFlags & PPF_Localized)
		? (BooleanValue ? GTrue  : GFalse ) : (BooleanValue ? TEXT("True") : TEXT("False")));
	Ar.Logf(TEXT("%s%s=%s") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName, Temp);
}

void UExporterT3DX::ExportFloatProperty(const FString& PropertyName, FLOAT FloatValue, FOutputDevice& Ar, DWORD PortFlags)
{
	Ar.Logf(TEXT("%s%s=%f") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName, FloatValue);
}

void UExporterT3DX::ExportIntProperty(const FString& PropertyName, INT IntValue, FOutputDevice& Ar, DWORD PortFlags)
{
	Ar.Logf(TEXT("%s%s=%d") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName, IntValue);
}

void UExporterT3DX::ExportStringProperty(const FString& PropertyName, const FString& StringValue, FOutputDevice& Ar, DWORD PortFlags)
{
	Ar.Logf(TEXT("%s%s=%s") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName, *StringValue);
}

void UExporterT3DX::ExportObjectProperty(const FString& PropertyName, const UObject* InExportingObject, const UObject* InObject, FOutputDevice& Ar, DWORD PortFlags)
{
	if (InObject)
	{
		UBOOL bExportFullObjectName = TRUE;
		UObject* InObjectOuter = InObject->GetOuter();
		while (InObjectOuter)
		{
			if (InObjectOuter == InExportingObject)
			{
				bExportFullObjectName = FALSE;
				InObjectOuter = NULL;
			}
			else
			{
				InObjectOuter = InObjectOuter->GetOuter();
			}
		}

		Ar.Logf(TEXT("%s%s=%s'%s'") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName,
			*(InObject->GetClass()->GetName()), 
			bExportFullObjectName ? *(InObject->GetPathName()) : *(InObject->GetName()));
	}
	else
	{
		Ar.Logf(TEXT("%s%s=") LINE_TERMINATOR, appSpc(TextIndent), *PropertyName);
	}
}

void UExporterT3DX::ExportUntypedBulkData(FUntypedBulkData& BulkData, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	Ar.Logf(TEXT("%sBegin UntypedBulkData") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

	/** Number of elements in bulk data array																			*/
	ExportIntProperty(TEXT("ElementCount"), BulkData.GetElementCount(), Ar, PortFlags);
	/** Serialized flags for bulk data																					*/
	ExportIntProperty(TEXT("ElementSize"), BulkData.GetElementSize(), Ar, PortFlags);
	/** The bulk data... */
	INT Size = BulkData.GetBulkDataSize();
	BYTE* BulkDataPointer = (BYTE*)(BulkData.Lock(LOCK_READ_ONLY));
	ExportBinaryBlob(Size, BulkDataPointer, Ar, PortFlags);
	BulkData.Unlock();

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd UntypedBulkData") LINE_TERMINATOR, appSpc(TextIndent));
}

void UExporterT3DX::ExportBinaryBlob(INT BlobSize, BYTE* BlobData, FOutputDevice& Ar, DWORD PortFlags)
{
	Ar.Logf(TEXT("%sBegin BinaryBlob") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

	ExportIntProperty(TEXT("Size"), BlobSize, Ar, PortFlags);
	
	Ar.Logf(TEXT("%sBegin Binary") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

	FString OutputString;
	TArray<TCHAR>& ResultArray = OutputString.GetCharArray();
	INT Slack = 32 * 4 + TextIndent + 5;
	ResultArray.Empty(Slack);
	TCHAR TempDigits[12];
	UBOOL bOutputFinalLine = FALSE;
	for (INT ByteIndex = 0; ByteIndex < BlobSize; ByteIndex++)
	{
		bOutputFinalLine = TRUE;
		appSprintf(TempDigits, TEXT("%02x,"), BlobData[ByteIndex]);
		OutputString += TempDigits;

		if (((ByteIndex + 1) % 32) == 0)
		{
			Ar.Logf(TEXT("%s%s") LINE_TERMINATOR, appSpc(TextIndent), *OutputString);
			ResultArray.Empty(Slack);
			bOutputFinalLine = FALSE;
		}
	}
	if (bOutputFinalLine)
	{
		Ar.Logf(TEXT("%s%s") LINE_TERMINATOR, appSpc(TextIndent), *OutputString);
	}

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd Binary") LINE_TERMINATOR, appSpc(TextIndent));

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd BinaryBlob") LINE_TERMINATOR, appSpc(TextIndent));
}

/**
* Initializes property values for intrinsic classes.  It is called immediately after the class default object
* is initialized against its archetype, but before any objects of this class are created.
*/
void UMaterialExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UMaterial::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3DMAT")) );
	new(FormatDescription)FString(TEXT("Unreal material text"));
}

//*** NOTE: This code assumes that any expressions within the Material will have
//			the material as their outer!
//***
UBOOL UMaterialExporterT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UMaterial* MaterialObj = CastChecked<UMaterial>(Object);

	// Remove all the Expression for its inner list!
	const TArray<UObject*>* MaterialInners = Context->GetObjectInners(MaterialObj);

	Ar.Logf( TEXT("%sBegin Material Class=%s Name=%s ObjName=%s Archetype=%s'%s'") LINE_TERMINATOR,
		appSpc(TextIndent), *Object->GetClass()->GetName(), *Object->GetName(), *Object->GetName(),
		*Object->GetArchetype()->GetClass()->GetName(), *Object->GetArchetype()->GetPathName() );

//		ExportObjectInner( Context, Object, Ar, PortFlags);

		TextIndent += 3;
		Ar.Logf(TEXT("%sBegin MaterialData") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent += 3;

		/** Versioning system... */
		Ar.Logf(TEXT("%sVersion=%d.%d") LINE_TERMINATOR, appSpc(TextIndent), VersionMax, VersionMin);

		// Export out the expression lists first as they will have to be created before setting the material properties!
		Ar.Logf(TEXT("%sBegin ExpressionObjectList") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent += 3;

			if (MaterialInners)
			{
				for (INT InnerIndex = 0; InnerIndex < MaterialInners->Num(); InnerIndex++)
				{
					UObject* InnerObj = (*MaterialInners)(InnerIndex);
					UMaterialExpression* MatExp = Cast<UMaterialExpression>(InnerObj);
					if (MatExp)
					{
						// export the object
						UExporter::ExportToOutputDevice(Context, MatExp, NULL, Ar, (PortFlags & PPF_Copy) ? TEXT("Copy") : TEXT("T3D"), TextIndent, PortFlags);
						// don't reexport below in ExportProperties
						MatExp->SetFlags(RF_TagImp);
					}
					else
					{
						warnf(TEXT("Non-MaterialExpression object in Material inner object list: %s (Mat %s)"), 
							*(InnerObj->GetName()), *(MaterialObj->GetName()));
					}
				}
			}
		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd ExpressionObjectList") LINE_TERMINATOR, appSpc(TextIndent));

		// Export the properties of the static mesh...
		UScriptStruct* ColorMaterialInputStruct = FindField<UScriptStruct>(UMaterial::StaticClass(), TEXT("ColorMaterialInput"));
		check(ColorMaterialInputStruct);
		UScriptStruct* ScalarMaterialInputStruct = FindField<UScriptStruct>(UMaterial::StaticClass(), TEXT("ScalarMaterialInput"));
		check(ScalarMaterialInputStruct);
		UScriptStruct* VectorMaterialInputStruct = FindField<UScriptStruct>(UMaterial::StaticClass(), TEXT("VectorMaterialInput"));
		check(VectorMaterialInputStruct);
		UScriptStruct* Vector2MaterialInputStruct = FindField<UScriptStruct>(UMaterial::StaticClass(), TEXT("Vector2MaterialInput"));
		check(Vector2MaterialInputStruct);
		FString ValueString;

		ExportObjectProperty(TEXT("PhysMaterial"), MaterialObj, MaterialObj->PhysMaterial, Ar, PortFlags);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->DiffuseColor), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sDiffuseColor=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ScalarMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->DiffusePower), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sDiffusePower=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->SpecularColor), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sSpecularColor=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ScalarMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->SpecularPower), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sSpecularPower=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(VectorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->Normal), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sNormal=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->EmissiveColor), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sEmissiveColor=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ScalarMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->Opacity), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sOpacity=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ScalarMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->OpacityMask), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sOpacityMask=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ExportFloatProperty(TEXT("OpacityMaskClipValue"), MaterialObj->OpacityMaskClipValue, Ar, PortFlags);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(Vector2MaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->Distortion), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sDistortion=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ExportStringProperty(TEXT("BlendMode"), UMaterial::GetBlendModeString((EBlendMode)(MaterialObj->BlendMode)), Ar, PortFlags);
		ExportStringProperty(TEXT("LightingModel"), UMaterial::GetMaterialLightingModelString((EMaterialLightingModel)(MaterialObj->LightingModel)), Ar, PortFlags);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->CustomLighting), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sCustomLighting=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->CustomSkylightDiffuse), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sCustomSkylightDiffuse=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->AnisotropicDirection), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sAnisotropicDirection=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ScalarMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->TwoSidedLightingMask), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sTwoSidedLightingMask=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->TwoSidedLightingColor), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sTwoSidedLightingColor=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(VectorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->WorldPositionOffset), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sWorldPositionOffset=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(VectorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->WorldDisplacement), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%sWorldDisplacement=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(Vector2MaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->TessellationMultiplier), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%TessellationMultiplier=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->SubsurfaceInscatteringColor), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%SubsurfaceInscatteringColor=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ColorMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->SubsurfaceAbsorptionColor), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%SubsurfaceAbsorptionColor=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ValueString = TEXT("");
		UStructProperty_ExportTextItem(ScalarMaterialInputStruct, ValueString, (BYTE*)&(MaterialObj->SubsurfaceScatteringRadius), NULL, MaterialObj, PortFlags);
		Ar.Logf(TEXT("%SubsurfaceScatteringRadius=%s") LINE_TERMINATOR, appSpc(TextIndent), *ValueString);
		ExportBooleanProperty(TEXT("TwoSided"), MaterialObj->TwoSided, Ar, PortFlags);
		ExportBooleanProperty(TEXT("DisableDepthTest"), MaterialObj->bDisableDepthTest, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedAsLightFunction"), MaterialObj->bUsedAsLightFunction, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithFogVolumes"), MaterialObj->bUsedWithFogVolumes, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedAsSpecialEngineMaterial"), MaterialObj->bUsedAsSpecialEngineMaterial, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithSkeletalMesh"), MaterialObj->bUsedWithSkeletalMesh, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithFracturedMeshes"), MaterialObj->bUsedWithFracturedMeshes, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithParticleSprites"), MaterialObj->bUsedWithParticleSprites, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithBeamTrails"), MaterialObj->bUsedWithBeamTrails, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithParticleSubUV"), MaterialObj->bUsedWithParticleSubUV, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithSpeedTree"), MaterialObj->bUsedWithSpeedTree, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithStaticLighting"), MaterialObj->bUsedWithStaticLighting, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithLensFlare"), MaterialObj->bUsedWithLensFlare, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithGammaCorrection"), MaterialObj->bUsedWithGammaCorrection, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithInstancedMeshParticles"), MaterialObj->bUsedWithInstancedMeshParticles, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithFluidSurfaces"), MaterialObj->bUsedWithFluidSurfaces, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithDecals"), MaterialObj->bUsedWithDecals, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithMaterialEffect"), MaterialObj->bUsedWithMaterialEffect, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithRadialBlur"), MaterialObj->bUsedWithRadialBlur, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithMorphTargets"), MaterialObj->bUsedWithMorphTargets, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithInstancedMeshes"), MaterialObj->bUsedWithInstancedMeshes, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithSplineMeshes"), MaterialObj->bUsedWithSplineMeshes, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedWithScreenDoorFade"), MaterialObj->bUsedWithScreenDoorFade, Ar, PortFlags);
		ExportBooleanProperty(TEXT("Wireframe"), MaterialObj->Wireframe, Ar, PortFlags);
		ExportIntProperty(TEXT("EditorX"), MaterialObj->EditorX, Ar, PortFlags);
		ExportIntProperty(TEXT("EditorY"), MaterialObj->EditorY, Ar, PortFlags);
		ExportIntProperty(TEXT("EditorPitch"), MaterialObj->EditorPitch, Ar, PortFlags);
		ExportIntProperty(TEXT("EditorYaw"), MaterialObj->EditorYaw, Ar, PortFlags);

		Ar.Logf(TEXT("%sBegin ExpressionList") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent += 3;

			for (INT ExpIndex = 0; ExpIndex < MaterialObj->Expressions.Num(); ExpIndex++)
			{
				UMaterialExpression* MatExp = MaterialObj->Expressions(ExpIndex);
				ExportObjectProperty(TEXT("Expression"), MaterialObj, MatExp, Ar, PortFlags);
			}

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd ExpressionList") LINE_TERMINATOR, appSpc(TextIndent));

		// /** Array of comments associated with this material; viewed in the material editor. */
		// var editoronly array<MaterialExpressionComment>	EditorComments;
		Ar.Logf(TEXT("%sBegin ExpressionCommentList") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent += 3;

			for (INT CommentIndex = 0; CommentIndex < MaterialObj->EditorComments.Num(); CommentIndex++)
			{
				UMaterialExpression* MatExp = MaterialObj->EditorComments(CommentIndex);
				ExportObjectProperty(TEXT("Comment"), MaterialObj, MatExp, Ar, PortFlags);
			}

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd ExpressionCommentList") LINE_TERMINATOR, appSpc(TextIndent));

		ExportBooleanProperty(TEXT("bUsesDistortion"), MaterialObj->bUsesDistortion, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bIsMasked"), MaterialObj->bIsMasked, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bIsPreviewMaterial"), MaterialObj->bIsPreviewMaterial, Ar, PortFlags);

		TextIndent -= 3;

		Ar.Logf(TEXT("%sEnd MaterialData") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent -= 3;

	Ar.Logf( TEXT("%sEnd Material") LINE_TERMINATOR, appSpc(TextIndent));

	return TRUE;
}

IMPLEMENT_CLASS(UMaterialExporterT3D);

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UStaticMeshExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UStaticMesh::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3DSTM")) );
	new(FormatDescription)FString(TEXT("Unreal mesh text"));
}

UBOOL UStaticMeshExporterT3D::ExportText( const FExportObjectInnerContext* Context, 
	UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	UStaticMesh* StaticMeshObj = CastChecked<UStaticMesh>(Object);

	Ar.Logf( TEXT("%sBegin StaticMesh Class=%s Name=%s ObjName=%s Archetype=%s'%s'") LINE_TERMINATOR,
		appSpc(TextIndent), *Object->GetClass()->GetName(), *Object->GetName(), *Object->GetName(),
		*Object->GetArchetype()->GetClass()->GetName(), *Object->GetArchetype()->GetPathName() );

		// Export the properties of the static mesh...
		ExportObjectInner( Context, Object, Ar, PortFlags);

		TextIndent += 3;
		// Export the geometry data, etc.
		Ar.Logf(TEXT("%sBegin SMData") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent += 3;

		/** Versioning system... */
		Ar.Logf(TEXT("%sVersion=%d.%d") LINE_TERMINATOR, appSpc(TextIndent), VersionMax, VersionMin);
		/** LOD distance ratio for this mesh */
		ExportFloatProperty(TEXT("LODDistanceRatio"), StaticMeshObj->LODDistanceRatio, Ar, PortFlags);
		/** Range at which only the lowest detail LOD can be displayed */
		ExportFloatProperty(TEXT("LODMaxRange"), StaticMeshObj->LODMaxRange, Ar, PortFlags);
		/** Allows artists to adjust the distance where textures using UV 0 are streamed in/out. */
		ExportFloatProperty(TEXT("StreamingDistanceMultiplier"), StaticMeshObj->StreamingDistanceMultiplier, Ar, PortFlags);
		/** Thumbnail */
		ExportObjectStructProperty(TEXT("ThumbnailAngle"), TEXT("Rotator"), (BYTE*)&(StaticMeshObj->ThumbnailAngle), StaticMeshObj, Ar, PortFlags);
		//
		ExportFloatProperty(TEXT("ThumbnailDistance"), StaticMeshObj->ThumbnailDistance, Ar, PortFlags);
		ExportIntProperty(TEXT("LightMapResolution"), StaticMeshObj->LightMapResolution, Ar, PortFlags);
		ExportIntProperty(TEXT("LightMapCoordinateIndex"), StaticMeshObj->LightMapCoordinateIndex, Ar, PortFlags);
		//
		ExportObjectStructProperty(TEXT("Bounds"), TEXT("BoxSphereBounds"), (BYTE*)&(StaticMeshObj->Bounds), StaticMeshObj, Ar, PortFlags);
		//
		ExportBooleanProperty(TEXT("UseSimpleLineCollision"), StaticMeshObj->UseSimpleLineCollision, Ar, PortFlags);
		ExportBooleanProperty(TEXT("UseSimpleBoxCollision"), StaticMeshObj->UseSimpleBoxCollision, Ar, PortFlags);
		ExportBooleanProperty(TEXT("UseSimpleRigidBodyCollision"), StaticMeshObj->UseSimpleRigidBodyCollision, Ar, PortFlags);
		ExportBooleanProperty(TEXT("UseFullPrecisionUVs"), StaticMeshObj->UseFullPrecisionUVs, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUsedForInstancing"), StaticMeshObj->bUsedForInstancing, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bUseMaximumStreamingTexelRatio"), StaticMeshObj->bUseMaximumStreamingTexelRatio, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bPartitionForEdgeGeometry"), StaticMeshObj->bPartitionForEdgeGeometry, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bCanBecomeDynamic"), StaticMeshObj->bCanBecomeDynamic, Ar, PortFlags);
		ExportBooleanProperty(TEXT("bStripkDOPForConsole"), StaticMeshObj->bStripkDOPForConsole, Ar, PortFlags);

		ExportIntProperty(TEXT("InternalVersion"), StaticMeshObj->InternalVersion, Ar, PortFlags);
		ExportStringProperty(TEXT("HighResSourceMeshName"), StaticMeshObj->HighResSourceMeshName, Ar, PortFlags);
		ExportIntProperty(TEXT("HighResSourceMeshCRC"), StaticMeshObj->HighResSourceMeshCRC, Ar, PortFlags);

		// We don't care about these???
		/** Array of physics-engine shapes that can be used by multiple StaticMeshComponents. */
		//TArray<void*>							PhysMesh;
		/** Scale of each PhysMesh entry. Arrays should be same size. */
		//TArray<FVector>							PhysMeshScale3D;

		/** Array of LODs, holding their associated rendering and collision data */
		//TIndirectArray<FStaticMeshRenderData>	LODModels;
		for (INT ModelIndex = 0; ModelIndex < StaticMeshObj->LODModels.Num(); ModelIndex++)
		{
			FStaticMeshRenderData* Model = StaticMeshObj->GetLODForExport(ModelIndex);
			if (Model)
			{
				ExportRenderData(*Model, Ar, Warn, PortFlags);
			}
		}

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd SMData") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent -= 3;

	Ar.Logf( TEXT("%sEnd StaticMesh") LINE_TERMINATOR, appSpc(TextIndent) );

	return TRUE;
}

void UStaticMeshExporterT3D::ExportStaticMeshElement(const FString& Name, const FStaticMeshElement& SMElement, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	Ar.Logf(TEXT("%sBegin StaticMeshElement") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

	ExportStringProperty(TEXT("Material"), SMElement.Material ? SMElement.Material->GetPathName() : TEXT(""), Ar, PortFlags);
	ExportBooleanProperty(TEXT("EnableCollision"), SMElement.EnableCollision, Ar, PortFlags);
	ExportBooleanProperty(TEXT("OldEnableCollision"), SMElement.OldEnableCollision, Ar, PortFlags);
	ExportBooleanProperty(TEXT("bEnableShadowCasting"), SMElement.bEnableShadowCasting, Ar, PortFlags);
	ExportIntProperty(TEXT("FirstIndex"), SMElement.FirstIndex, Ar, PortFlags);
	ExportIntProperty(TEXT("NumTriangles"), SMElement.NumTriangles, Ar, PortFlags);
	ExportIntProperty(TEXT("MinVertexIndex"), SMElement.MinVertexIndex, Ar, PortFlags);
	ExportIntProperty(TEXT("MaxVertexIndex"), SMElement.MaxVertexIndex, Ar, PortFlags);
	ExportIntProperty(TEXT("MaterialIndex"), SMElement.MaterialIndex, Ar, PortFlags);

	Ar.Logf(TEXT("%sBegin Fragments") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

		for (INT FragmentIndex = 0; FragmentIndex < SMElement.Fragments.Num(); FragmentIndex++)
		{
			Ar.Logf(TEXT("%s%d,%d") LINE_TERMINATOR, appSpc(TextIndent), 
				SMElement.Fragments(FragmentIndex).BaseIndex,
				SMElement.Fragments(FragmentIndex).NumPrimitives);
		}

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd Fragments") LINE_TERMINATOR, appSpc(TextIndent));

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd StaticMeshElement") LINE_TERMINATOR, appSpc(TextIndent));
}

void UStaticMeshExporterT3D::ExportStaticMeshTriangleBulkData(FStaticMeshTriangleBulkData& SMTBulkData, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	Ar.Logf(TEXT("%sBegin StaticMeshTriangleBulkData") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

	ExportUntypedBulkData(SMTBulkData, Ar, Warn, PortFlags);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd StaticMeshTriangleBulkData") LINE_TERMINATOR, appSpc(TextIndent));
}

UBOOL UStaticMeshExporterT3D::ExportRenderData(FStaticMeshRenderData& Model, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags )
{
	Ar.Logf(TEXT("%sBegin SMRenderData") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

	/** The number of vertices in the LOD. */
	ExportIntProperty(TEXT("NumVertices"), Model.NumVertices, Ar, PortFlags);

	//TArray<FStaticMeshElement>				Elements;
	Ar.Logf(TEXT("%sBegin Elements") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;
	ExportIntProperty(TEXT("Count"), Model.Elements.Num(), Ar, PortFlags);
	for (INT ElementIndex = 0; ElementIndex < Model.Elements.Num(); ElementIndex++)
	{
		const FStaticMeshElement& Element = Model.Elements(ElementIndex);
		FString Name;
		ExportStaticMeshElement(Name, Element, Ar, Warn, PortFlags);
	}
	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd Elements") LINE_TERMINATOR, appSpc(TextIndent));

	ExportStaticMeshTriangleBulkData(Model.RawTriangles, Ar, Warn, PortFlags);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd SMRenderData") LINE_TERMINATOR, appSpc(TextIndent));

	return TRUE;
}

IMPLEMENT_CLASS(UStaticMeshExporterT3D)

/*------------------------------------------------------------------------------
	UTextureExporterT3D implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTextureExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UTexture::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3DT2D")) );
	new(FormatDescription)FString(TEXT("Unreal texture text"));
}

UBOOL UTextureExporterT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	FString PropText;
	UTexture* TextureObj = CastChecked<UTexture>(Object);
	UTexture2D* Texture2DObj = Cast<UTexture2D>(Object);
	UTextureCube* TextureCubeObj = Cast<UTextureCube>(Object);

	Ar.Logf( TEXT("%sBegin Texture Class=%s Name=%s ObjName=%s Archetype=%s'%s'") LINE_TERMINATOR,
		appSpc(TextIndent), *Object->GetClass()->GetName(), *Object->GetName(), *Object->GetName(),
		*Object->GetArchetype()->GetClass()->GetName(), *Object->GetArchetype()->GetPathName() );

		// Export the properties of the static mesh...
		ExportObjectInner( Context, Object, Ar, PortFlags);

		TextIndent += 3;
		// Export the geometry data, etc.
		Ar.Logf(TEXT("%sBegin TextureData") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent += 3;

			/** Versioning system... */
			Ar.Logf(TEXT("%sVersion=%d.%d") LINE_TERMINATOR, appSpc(TextIndent), VersionMax, VersionMin);

			ExportBooleanProperty(TEXT("SRGB"), TextureObj->SRGB, Ar, PortFlags);
			ExportBooleanProperty(TEXT("RGBE"), TextureObj->RGBE, Ar, PortFlags);
			Ar.Logf(TEXT("%sUnpackMin=(%f,%f,%f,%f)") LINE_TERMINATOR, appSpc(TextIndent),
				TextureObj->UnpackMin[0], TextureObj->UnpackMin[1], TextureObj->UnpackMin[2], TextureObj->UnpackMin[3]);
			Ar.Logf(TEXT("%sUnpackMax=(%f,%f,%f,%f)") LINE_TERMINATOR, appSpc(TextIndent),
				TextureObj->UnpackMax[0], TextureObj->UnpackMax[1], TextureObj->UnpackMax[2], TextureObj->UnpackMax[3]);

			ExportBooleanProperty(TEXT("CompressionNoAlpha"), TextureObj->CompressionNoAlpha, Ar, PortFlags);
			ExportBooleanProperty(TEXT("CompressionNone"), TextureObj->CompressionNone, Ar, PortFlags);
			ExportBooleanProperty(TEXT("CompressionFullDynamicRange"), TextureObj->CompressionFullDynamicRange, Ar, PortFlags);
			ExportBooleanProperty(TEXT("DeferCompression"), TextureObj->DeferCompression, Ar, PortFlags);

			/** Allows artists to specify that a texture should never have its miplevels dropped which is useful for e.g. HUD and menu textures */
			ExportBooleanProperty(TEXT("NeverStream"), TextureObj->NeverStream, Ar, PortFlags);
			/** When TRUE, mip-maps are dithered for smooth transitions. */
			ExportBooleanProperty(TEXT("bDitherMipMapAlpha"), TextureObj->bDitherMipMapAlpha, Ar, PortFlags);
			/** If TRUE, the color border pixels are preseved by mipmap generation.  One flag per color channel. */
			ExportBooleanProperty(TEXT("bPreserveBorderR"), TextureObj->bPreserveBorderR, Ar, PortFlags);
			ExportBooleanProperty(TEXT("bPreserveBorderG"), TextureObj->bPreserveBorderG, Ar, PortFlags);
			ExportBooleanProperty(TEXT("bPreserveBorderB"), TextureObj->bPreserveBorderB, Ar, PortFlags);
			ExportBooleanProperty(TEXT("bPreserveBorderA"), TextureObj->bPreserveBorderA, Ar, PortFlags);
			/** If TRUE, the RHI texture will be created using TexCreate_NoTiling */
			ExportBooleanProperty(TEXT("bNoTiling"), TextureObj->bNoTiling, Ar, PortFlags);

			ExportStringProperty(TEXT("CompressionSettings"), 
				UTexture::GetCompressionSettingsString((TextureCompressionSettings)(TextureObj->CompressionSettings)), 
				Ar, PortFlags);
			ExportStringProperty(TEXT("Filter"), 
				UTexture::GetTextureFilterString((TextureFilter)(TextureObj->Filter)), 
				Ar, PortFlags);
			ExportStringProperty(TEXT("LODGroup"), 
				UTexture::GetTextureGroupString((TextureGroup)(TextureObj->LODGroup)), 
				Ar, PortFlags);

			/** A bias to the index of the top mip level to use. */
			ExportIntProperty(TEXT("LODBias"), TextureObj->LODBias, Ar, PortFlags);
			ExportStringProperty(TEXT("SourceFilePath"), TextureObj->SourceFilePath, Ar, PortFlags);
			ExportStringProperty(TEXT("SourceFileTimestamp"), TextureObj->SourceFileTimestamp, Ar, PortFlags);

			Ar.Logf(TEXT("%sBegin SourceArt") LINE_TERMINATOR, appSpc(TextIndent));
			TextIndent += 3;

				ExportUntypedBulkData(TextureObj->SourceArt, Ar, Warn, PortFlags);

			TextIndent -= 3;
			Ar.Logf(TEXT("%sEnd SourceArt") LINE_TERMINATOR, appSpc(TextIndent));

			if (Texture2DObj)
			{
				ExportText_Texture2D(Context, Texture2DObj, Type, Ar, Warn, PortFlags);
			}
			if (TextureCubeObj)
			{
				ExportText_TextureCube(Context, TextureCubeObj, Type, Ar, Warn, PortFlags);
			}

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd TextureData") LINE_TERMINATOR, appSpc(TextIndent));
		TextIndent -= 3;

	Ar.Logf( TEXT("%sEnd Texture") LINE_TERMINATOR, appSpc(TextIndent) );

	return TRUE;
}

UBOOL UTextureExporterT3D::ExportText_Texture2D(const FExportObjectInnerContext* Context, UTexture2D* InTexture2D, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	//@NOTE: This must match up with the TextureFactory importing code!
	Ar.Logf(TEXT("%sBegin Texture2DData") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

		FString PropText;
		/** The width of the texture.												*/
		ExportIntProperty(TEXT("SizeX"), InTexture2D->SizeX, Ar, PortFlags);
		/** The height of the texture.												*/
		ExportIntProperty(TEXT("SizeY"), InTexture2D->SizeY, Ar, PortFlags);
		/** The format of the texture data.											*/
		ExportStringProperty(TEXT("Format"), GPixelFormats[InTexture2D->Format].Name, Ar, PortFlags);
		/** The addressing mode to use for the X axis.								*/
		ExportStringProperty(TEXT("AddressX"), 
			UTexture::GetTextureAddressString((TextureAddress)(InTexture2D->AddressX)), Ar, PortFlags);
		/** The addressing mode to use for the Y axis.								*/
		ExportStringProperty(TEXT("AddressY"), 
			UTexture::GetTextureAddressString((TextureAddress)(InTexture2D->AddressY)), Ar, PortFlags);
		/** Global/ serialized version of ForceMiplevelsToBeResident.				*/
		ExportBooleanProperty(TEXT("bGlobalForceMipLevelsToBeResident"), InTexture2D->bGlobalForceMipLevelsToBeResident, Ar, PortFlags);
		/** 
		* Keep track of the first mip level stored in the packed miptail.
		* it's set to highest mip level if no there's no packed miptail 
		*/
		ExportIntProperty(TEXT("MipTailBaseIdx"), InTexture2D->MipTailBaseIdx, Ar, PortFlags);

		// Write out the top-level mip only...
		if (InTexture2D->SourceArt.IsBulkDataLoaded() == FALSE)
		{
			if (InTexture2D->Mips.Num() > 0)
			{
				FTexture2DMipMap& TopLevelMip = InTexture2D->Mips(0);

				Ar.Logf(TEXT("%sBegin Mip0") LINE_TERMINATOR, appSpc(TextIndent));
				TextIndent += 3;
				ExportIntProperty(TEXT("SizeX"), TopLevelMip.SizeX, Ar, PortFlags);
				ExportIntProperty(TEXT("SizeY"), TopLevelMip.SizeY, Ar, PortFlags);

				//var native UntypedBulkData_Mirror Data{FTextureMipBulkData};	
				Ar.Logf(TEXT("%sBegin TextureMipBulkData") LINE_TERMINATOR, appSpc(TextIndent));
				TextIndent += 3;

					ExportUntypedBulkData(TopLevelMip.Data, Ar, Warn, PortFlags);

				TextIndent -= 3;
				Ar.Logf(TEXT("%sEnd TextureMipBulkData") LINE_TERMINATOR, appSpc(TextIndent));

				TextIndent -= 3;
				Ar.Logf(TEXT("%sEnd Mip0") LINE_TERMINATOR, appSpc(TextIndent));
			}
		}

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd Texture2DData") LINE_TERMINATOR, appSpc(TextIndent));

	return TRUE;
}

UBOOL UTextureExporterT3D::ExportText_TextureCube(const FExportObjectInnerContext* Context, UTextureCube* InTextureCube, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	//@NOTE: This must match up with the TextureFactory importing code!
	Ar.Logf(TEXT("%sBegin TextureCubeData") LINE_TERMINATOR, appSpc(TextIndent));
	TextIndent += 3;

		FString PropText;
		/** The width of the texture.												*/
		ExportIntProperty(TEXT("SizeX"), InTextureCube->SizeX, Ar, PortFlags);
		/** The height of the texture.												*/
		ExportIntProperty(TEXT("SizeY"), InTextureCube->SizeY, Ar, PortFlags);
		/** The format of the texture data.											*/
		ExportStringProperty(TEXT("Format"), GPixelFormats[InTextureCube->Format].Name, Ar, PortFlags);
		/** The addressing mode to use for the X axis.								*/

		ExportObjectProperty(TEXT("FacePosX"), InTextureCube, InTextureCube->FacePosX, Ar, PortFlags);
		ExportObjectProperty(TEXT("FaceNegX"), InTextureCube, InTextureCube->FaceNegX, Ar, PortFlags);
		ExportObjectProperty(TEXT("FacePosY"), InTextureCube, InTextureCube->FacePosY, Ar, PortFlags);
		ExportObjectProperty(TEXT("FaceNegY"), InTextureCube, InTextureCube->FaceNegY, Ar, PortFlags);
		ExportObjectProperty(TEXT("FacePosZ"), InTextureCube, InTextureCube->FacePosZ, Ar, PortFlags);
		ExportObjectProperty(TEXT("FaceNegZ"), InTextureCube, InTextureCube->FaceNegZ, Ar, PortFlags);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TextureCubeData") LINE_TERMINATOR, appSpc(TextIndent));

	return TRUE;
}

IMPLEMENT_CLASS(UTextureExporterT3D);

namespace RecastExporter
{
	struct FTriIndices
	{
		INT v0, v1, v2;
	};

	void ClipPolyWithConvexes(const TArray<TArray<FPlane>>& Convexes, const FPoly& PolyToClip, TArray<FPoly>& OutPolys)
	{
		for (INT iConvex = 0; iConvex < Convexes.Num(); iConvex++)
		{
			const TArray<FPlane>& ConvexPlanes = Convexes(iConvex);

			TArray<FPoly> PolysCutByConvex;
			PolysCutByConvex.AddItem(PolyToClip);

			for (INT iPlane = 0; iPlane < ConvexPlanes.Num(); iPlane++)
			{
				const FPlane Plane = ConvexPlanes(iPlane);
				const FVector PlaneBase = Plane.SafeNormal() * Plane.W;
				const FVector PlaneNormal = Plane.SafeNormal();

				TArray<FPoly> PolysLeftAfterPlaneCut;
				FPoly Front, Back;

				for (INT iPoly = 0; iPoly < PolysCutByConvex.Num(); iPoly++)
				{
					FPoly& Poly = PolysCutByConvex(iPoly);

					const INT SplitResult = Poly.SplitWithPlane(PlaneBase, PlaneNormal, &Front, &Back, 0);
					switch (SplitResult)
					{
						case SP_Split:
							PolysLeftAfterPlaneCut.AddItem(Back);
							break;

						case SP_Back:
							PolysLeftAfterPlaneCut.AddItem(Poly);
							break;
					}

					PolysCutByConvex = PolysLeftAfterPlaneCut;
				}

				OutPolys += PolysCutByConvex;
			}
		}
	}

	void ExportBrushComponent(UBrushComponent* BrushComp, const FMatrix& LocalToWorld, TArray<FVector>& Verts, TArray<FTriIndices>& Faces)
	{
		const UBOOL bFlipCullMode = LocalToWorld.RotDeterminant() < 0.0f;

		INT vertexCount = Verts.Num();
		INT faceCount = Faces.Num();
		TArray<INT> face;

		for (INT iPoly = 0 ; iPoly < BrushComp->Brush->Polys->Element.Num(); iPoly++)
		{
			FPoly* Poly = &(BrushComp->Brush->Polys->Element(iPoly));
			const INT NumVerts = Poly->Vertices.Num();

			Verts.Add(NumVerts);
			for (INT iVertex = NumVerts - 1; iVertex >= 0; iVertex--)
			{
				FVector PosWS = LocalToWorld.TransformFVector(Poly->Vertices(iVertex));
				FVector exportPos = FVector(-PosWS.X, PosWS.Z, -PosWS.Y);
				Verts(vertexCount++) = exportPos;
			}

			if (NumVerts > face.Num())
			{
				face.Add(NumVerts - face.Num());
			}

			for (INT iVertex = 0; iVertex < NumVerts; iVertex++)
			{
				face(iVertex) = vertexCount - iVertex - 1;
			}

			Faces.Add(NumVerts - 2);
			for (INT iVertex = 2; iVertex < NumVerts; iVertex++)
			{
				FTriIndices triangle = { face(iVertex), face(iVertex-1), face(0) };
				if (bFlipCullMode) Swap(triangle.v0, triangle.v2);
				Faces(faceCount++) = triangle;
			}
		}
	}

	void ExportPrimitiveComponent(UPrimitiveComponent* PrimComp, const FMatrix& LocalToWorld, TArray<FVector>& Verts, TArray<FTriIndices>& Faces, UBOOL bCollideComplex = FALSE)
	{
		const UBOOL bFlipCullMode = LocalToWorld.RotDeterminant() < 0.0f;

		INT vertexCount = Verts.Num();
		INT faceCount = Faces.Num();

		UStaticMeshComponent *SMC = Cast<UStaticMeshComponent>(PrimComp);
		if ((bCollideComplex && SMC) ||
			(SMC && SMC->StaticMesh && !SMC->StaticMesh->UseSimpleLineCollision))
		{
			if (SMC->StaticMesh)
			{
				FStaticMeshRenderData* StaticMeshRenderData = SMC->StaticMesh->GetLODForExport(0);
				for (INT iTriangle = 0; iTriangle < SMC->StaticMesh->kDOPTree.Triangles.Num(); iTriangle++)
				{
					Verts.Add(3);
					Faces.Add(1);

					const FkDOPCollisionTriangle<WORD>& Triangle = SMC->StaticMesh->kDOPTree.Triangles(iTriangle);

					FVector Pos = LocalToWorld.TransformFVector(StaticMeshRenderData->PositionVertexBuffer.VertexPosition(Triangle.v1));
					FVector exportPos = FVector(-Pos.X, Pos.Z, -Pos.Y);
					Verts(vertexCount++) = exportPos;
					Pos =  LocalToWorld.TransformFVector(StaticMeshRenderData->PositionVertexBuffer.VertexPosition(Triangle.v2));
					exportPos = FVector(-Pos.X, Pos.Z, -Pos.Y);
					Verts(vertexCount++) = exportPos;
					Pos =  LocalToWorld.TransformFVector(StaticMeshRenderData->PositionVertexBuffer.VertexPosition(Triangle.v3));
					exportPos = FVector(-Pos.X, Pos.Z, -Pos.Y);
					Verts(vertexCount++) = exportPos;

					FTriIndices triangle = {vertexCount-3, vertexCount-2, vertexCount-1};
					if (bFlipCullMode) Swap(triangle.v0, triangle.v2);
					Faces(faceCount++) = triangle;
				}
			}

			return;
		}

		URB_BodySetup* BodySetup = PrimComp->GetRBBodySetup();
		if (BodySetup != NULL)
		{
			FKAggregateGeom& AggGeom = BodySetup->AggGeom;
			for (INT iElem = 0; iElem < AggGeom.ConvexElems.Num(); iElem++)
			{
				FKConvexElem& ConEl = AggGeom.ConvexElems(iElem);
				INT NumTris = ConEl.FaceTriData.Num() / 3;
				Verts.Add(NumTris * 3);
				Faces.Add(NumTris);
				for (INT iTriangle = 0; iTriangle < NumTris; iTriangle++)
				{
					for (INT iVertex = 0; iVertex < 3; iVertex++)
					{
						FVector Pos = LocalToWorld.TransformFVector(ConEl.VertexData(ConEl.FaceTriData((iTriangle * 3) + iVertex)));
						FVector exportPos = FVector(-Pos.X, Pos.Z, -Pos.Y);
						Verts(vertexCount++) = exportPos;
					}

					FTriIndices triangle = {vertexCount-3, vertexCount-2, vertexCount-1};
					if (bFlipCullMode) Swap(triangle.v0, triangle.v2);
					Faces(faceCount++) = triangle;
				}
			}
		
			return;
		}

		UCylinderComponent* CylComp = Cast<UCylinderComponent>(PrimComp);
		if (CylComp != NULL)
		{
			Verts.Add(8);
			Faces.Add(12);

			FVector vects[] = {
				FVector(-CylComp->CollisionRadius,  CylComp->CollisionRadius, 0),
				FVector( CylComp->CollisionRadius,  CylComp->CollisionRadius, 0),
				FVector( CylComp->CollisionRadius, -CylComp->CollisionRadius, 0),
				FVector(-CylComp->CollisionRadius, -CylComp->CollisionRadius, 0),
				FVector(-CylComp->CollisionRadius,  CylComp->CollisionRadius, CylComp->CollisionHeight),
				FVector( CylComp->CollisionRadius,  CylComp->CollisionRadius, CylComp->CollisionHeight),
				FVector( CylComp->CollisionRadius, -CylComp->CollisionRadius, CylComp->CollisionHeight),
				FVector(-CylComp->CollisionRadius, -CylComp->CollisionRadius, CylComp->CollisionHeight),
			};
			for (INT i = 0; i < 8; i++)
			{
				FVector Pos = LocalToWorld.TransformFVector(vects[i]);
				FVector exportPos = FVector(-Pos.X, Pos.Z, -Pos.Y);
				Verts(vertexCount++) = exportPos;
			}

			FTriIndices faces[] = {
				{ 0, 2, 1 },
				{ 0, 3, 2 },
				{ 4, 5, 6 },
				{ 4, 6, 7 },
				{ 0, 1, 4 },
				{ 1, 5, 4 },
				{ 3, 7, 2 },
				{ 7, 6, 2 },
				{ 1, 2, 6 },
				{ 1, 6, 5 },
				{ 0, 7, 3 },
				{ 0, 4, 7 },
			};
			for (INT i = 0; i < 12; i++)
			{
				FTriIndices triangle = faces[i];
				triangle.v0 += vertexCount - 8;
				triangle.v1 += vertexCount - 8;
				triangle.v2 += vertexCount - 8;

				Faces(faceCount++) = triangle;
			}			
		}
	}

	void ExportActor(AActor* Actor, TArray<FVector>& Verts, TArray<FTriIndices>& Faces)
	{
		if (Actor == NULL)
			return;

		FMatrix LocalToWorld = Actor->LocalToWorld();

		AVolume* Volume = Cast<AVolume>(Actor);
		if (Volume != NULL && Volume->BrushComponent != NULL && Volume->BrushComponent->Brush != NULL)
		{
			ExportBrushComponent(Volume->BrushComponent, LocalToWorld, Verts, Faces);
			return;
		}

		for (INT ComponentIndex = 0; ComponentIndex < Actor->AllComponents.Num(); ComponentIndex++)
		{
			UPrimitiveComponent* PrimComponent = Cast<UPrimitiveComponent>(Actor->AllComponents(ComponentIndex));
			if (PrimComponent && PrimComponent->BlockNonZeroExtent && PrimComponent->BlockActors)
			{
				FMatrix& LocalToWorldCom = PrimComponent->LocalToWorld;
				ExportPrimitiveComponent(PrimComponent, LocalToWorldCom, Verts, Faces, Actor->bCollideComplex);
			}
		}
	}

	void FillFromPolyArray(const TArray<FPoly>& Polys, TArray<FVector>& Verts, TArray<FTriIndices>& Faces)
	{
		INT vertexCount = Verts.Num();
		INT faceCount = Faces.Num();
		TArray<INT> face;

		for (INT iPoly = 0; iPoly < Polys.Num(); iPoly++)
		{
			const FPoly& Poly = Polys(iPoly);
			const INT NumVerts = Poly.Vertices.Num();

			Verts.Add(NumVerts);

			for (INT iVertex = NumVerts - 1; iVertex >=0 ; iVertex--)
			{
				const FVector& Pos = Poly.Vertices(iVertex);
				FVector exportPos = FVector(-Pos.X, Pos.Z, -Pos.Y);
				Verts(vertexCount++) = exportPos;
			}

			if (NumVerts > face.Num())
			{
				face.Add(NumVerts - face.Num());
			}

			for (INT iVertex = 0; iVertex < NumVerts; iVertex++)
			{
				face(iVertex) = vertexCount - iVertex - 1;
			}

			Faces.Add(NumVerts - 2);
			for (INT iVertex = 2; iVertex < NumVerts; iVertex++)
			{
				FTriIndices triangle = { face(iVertex), face(iVertex-1), face(0) };
				Faces(faceCount++) = triangle;
			}
		}
	}

	void ExportLandscapeComponents(const TArray<ULandscapeHeightfieldCollisionComponent*>& LandscapeComponents, const TArray<TArray<FPlane>>& Convexes, TArray<FVector>& Verts, TArray<FTriIndices>& Faces)
	{
		for (INT ComponentIdx = 0; ComponentIdx < LandscapeComponents.Num(); ComponentIdx++ )
		{
			ULandscapeHeightfieldCollisionComponent* Component = LandscapeComponents(ComponentIdx);

			TArray<FPoly> OutPolys;
			TArray<FVector> TriangleVerts;
			Component->GetCollisionTriangles(TriangleVerts);

			INT i = 0;
			while (i < TriangleVerts.Num())
			{
				FPoly poly;

				poly.Vertices.AddItem(TriangleVerts(i++));
				poly.Vertices.AddItem(TriangleVerts(i++));
				poly.Vertices.AddItem(TriangleVerts(i++));

				ClipPolyWithConvexes(Convexes, poly, OutPolys);
			}

			FillFromPolyArray(OutPolys, Verts, Faces);
		}
	}

	void ExportTerrain(const TArray<ATerrain*>& Terrains, const TArray<TArray<FPlane>>& Convexes, TArray<FVector>& Verts, TArray<FTriIndices>& Faces)
	{
		for (INT iTerrain = 0; iTerrain < Terrains.Num(); iTerrain++)
		{
			ATerrain* TerrainActor = Terrains(iTerrain);
			if (TerrainActor == NULL) continue;

			TArray<FPoly> OutPolys;
			for (INT y = 0; y < TerrainActor->NumVerticesY - 1; y++)
			{
				for (INT x = 0 ; x < TerrainActor->NumVerticesX-1; x++)
				{
					if (TerrainActor->IsTerrainQuadVisible(x, y))
					{
						FPoly poly;

						poly.Vertices.AddItem(TerrainActor->GetWorldVertex(x + 1,y));
						poly.Vertices.AddItem(TerrainActor->GetWorldVertex(x + 1,y + 1));
						poly.Vertices.AddItem(TerrainActor->GetWorldVertex(x,    y + 1));
						poly.Vertices.AddItem(TerrainActor->GetWorldVertex(x,    y));

						ClipPolyWithConvexes(Convexes, poly, OutPolys);
					}
				}
			}

			FillFromPolyArray(OutPolys, Verts, Faces);
		}
	}

	void ExportBSP(const TArray<TArray<FPlane>>& Convexes, TArray<FVector>& Verts, TArray<FTriIndices>& Faces)
	{
		TArray<FOBJGeom*> Objects;
		FOutputDevice DummyOutput;
		INT TotalPolys = 0;

		FOR_EACH_UMODEL;
			GEditor->bspBuildFPolys(Model, 0, 0 );
			UPolys* Polys = Model->Polys;
			INT PolyNum = Polys->Element.Num();

			TotalPolys = TotalPolys + PolyNum;
			ExportPolys(Polys, PolyNum, TotalPolys, DummyOutput, NULL, FALSE, Model, Objects);
			Model->Polys->Element.Empty();
		END_FOR_EACH_UMODEL;

		for (INT iObject = 0; iObject < Objects.Num(); iObject++)
		{
			FOBJGeom* Object = Objects(iObject);
			TArray<FPoly> OutPolys;

			for (INT iFace = 0; iFace < Object->Faces.Num(); iFace++)
			{
				FOBJFace* Face = &Object->Faces(iFace);
				FPoly poly;
				const INT vertsCount = sizeof(Face->VertexIndex) / sizeof(UINT);

				for (INT i = 0; i < vertsCount; i++)
				{
					poly.Vertices.AddItem(Object->VertexData(Face->VertexIndex[i]).Vert);
				}

				ClipPolyWithConvexes(Convexes, poly, OutPolys);
			}

			FillFromPolyArray(OutPolys, Verts, Faces);
		}
	}

	UBOOL IsTerrainWithinBounds(ATerrain* TerrainActor, TArray<FBox>& Bounds)
	{
		FBox ActorBounds;
		for (INT ComponentIndex = 0; ComponentIndex < TerrainActor->TerrainComponents.Num(); ComponentIndex++)
		{
			UTerrainComponent* Component = TerrainActor->TerrainComponents(ComponentIndex);
			if (Component != NULL)
			{
				ActorBounds += Component->Bounds.GetBox();
			}
		}

		UBOOL bIntersects = FALSE;
		for (INT iBound = 0; iBound < Bounds.Num(); iBound++)
		{
			if (ActorBounds.Intersect(Bounds(iBound)))
			{
				bIntersects = TRUE;
				break;
			}
		}

		return bIntersects;
	}

	UBOOL IsActorWithinBounds(AActor* Actor, TArray<FBox>& Bounds)
	{
		if (Actor == NULL)
			return FALSE;

		FBox ActorBounds = Actor->GetComponentsBoundingBox();

		UBOOL bIntersects = FALSE;
		for (INT iBound = 0; iBound < Bounds.Num(); iBound++)
		{
			if (ActorBounds.Intersect(Bounds(iBound)))
			{
				bIntersects = TRUE;
				break;
			}
		}

		return bIntersects;
	}

	void GetVolumeConvexes(AVolume* Volume, TArray<TArray<FPlane>>& Convexes)
	{
		Volume->BrushComponent->BuildSimpleBrushCollision();

		FKAggregateGeom & BrushAggGeom = Volume->BrushComponent->BrushAggGeom;
		const INT NumElements = BrushAggGeom.ConvexElems.Num();

		for (INT iElement = 0; iElement < NumElements; iElement++)
		{
			TArray<FPlane> Conv;

			const FKConvexElem& ConvexElement = BrushAggGeom.ConvexElems(iElement);
			for (INT iPlane = 0; iPlane < ConvexElement.FacePlaneData.Num(); iPlane++)
			{
				// Transform the bounding plane into world-space.
				FPlane WorldPlane = ConvexElement.FacePlaneData(iPlane).TransformBy(Volume->BrushComponent->LocalToWorld);
				Conv.AddItem(WorldPlane);
			}

			if (Conv.Num() > 0)
			{
				Convexes.AddItem(Conv);
			}
		}
	}

	void PrepareExportBounds(APylon* Pylon, TArray<FBox>& Bounds, TArray<TArray<FPlane>>& ClipPlanes)
	{
		Bounds.Empty();
		ClipPlanes.Reset();

		for (INT i = 0; i < Pylon->ExpansionVolumes.Num(); i++)
		{
			AVolume* ExpansionVolume = Pylon->ExpansionVolumes(i);
			if (ExpansionVolume == NULL)
				continue;

			Bounds.AddItem(ExpansionVolume->GetComponentsBoundingBox());
			GetVolumeConvexes(ExpansionVolume, ClipPlanes);
		}

		// use expansion radius when volumes are not provided
		if (Bounds.Num() == 0)
		{
			FMatrix PylonWSNoRotation = FScaleMatrix(Pylon->DrawScale3D * Pylon->DrawScale) * FTranslationMatrix(Pylon->Location);

			FBoxSphereBounds SimpleBoundsLS(FVector(0), FVector(Pylon->ExpansionRadius), Pylon->ExpansionRadius);
			FBoxSphereBounds SimpleBoundsWS = SimpleBoundsLS.TransformBy(PylonWSNoRotation);
			
			Bounds.AddItem(SimpleBoundsWS.GetBox());

			FPlane SimplePlanes[] = {
				FPlane(FVector( 1, 0, 0), Pylon->ExpansionRadius),
				FPlane(FVector(-1, 0, 0), Pylon->ExpansionRadius),
				FPlane(FVector( 0, 1, 0), Pylon->ExpansionRadius),
				FPlane(FVector( 0,-1, 0), Pylon->ExpansionRadius),
				FPlane(FVector( 0, 0, 1), Pylon->ExpansionRadius),
				FPlane(FVector( 0, 0,-1), Pylon->ExpansionRadius)
			};

			TArray<FPlane> Conv;
			for (INT i = 0; i < 6; i++)
			{
				Conv.AddItem(SimplePlanes[i].TransformBy(PylonWSNoRotation));			
			}

			ClipPlanes.AddItem(Conv);
		}
	}
}


void UEditorEngine::GetPathCollidingGeometry(APylon* Pylon, TArray<FVector>& Verts, TArray<INT>& CompactFaces)
{
	Verts.Reset();
	CompactFaces.Reset();

	TArray<TArray<FPlane>> ClipPlanes;
	TArray<FBox> GenerationBounds;
	RecastExporter::PrepareExportBounds(Pylon, GenerationBounds, ClipPlanes);

	TArray<ULandscapeHeightfieldCollisionComponent*> LandscapeComponents;
	TArray<ATerrain *> Terrains;
	TArray<RecastExporter::FTriIndices> Faces;

	for (INT iLevel = 0; iLevel < GWorld->Levels.Num(); iLevel++) 
	{
		ULevel* Level = GWorld->Levels(iLevel);
		for (INT iActor = 0; iActor < Level->Actors.Num(); iActor++)
		{
			AActor* Actor = Level->Actors(iActor);
			UBOOL bCanExport = (Actor != NULL) &&
				Actor->bBlockActors && Actor->bCollideActors &&
				Actor->CollisionType != COLLIDE_NoCollision &&
				Actor->CollisionType != COLLIDE_BlockWeapons;

			// get all landscape components within bounds and process them later
			ALandscapeProxy* LandscapeActor = Cast<ALandscapeProxy>(Actor);
			if (LandscapeActor != NULL)
			{
				for (INT iComponent = 0; iComponent < LandscapeActor->CollisionComponents.Num(); iComponent++)
				{
					ULandscapeHeightfieldCollisionComponent* Component = LandscapeActor->CollisionComponents(iComponent);
					for (INT iBound = 0; iBound < GenerationBounds.Num(); iBound++)
					{
						if (Component->Bounds.GetBox().Intersect(GenerationBounds(iBound)))
						{
							LandscapeComponents.AddItem(Component);
							break;
						}
					}
				}

				continue;
			}

			// get all terrain actors within bounds and process them later
			ATerrain* TerrainActor = Cast<ATerrain>(Actor);
			if (TerrainActor != NULL)
			{
				if (bCanExport && RecastExporter::IsTerrainWithinBounds(TerrainActor, GenerationBounds))
				{
					Terrains.AddItem(TerrainActor);
				}

				continue;
			}

			// special case for fractures with cores
			AFracturedStaticMeshActor* FractureActor = Cast<AFracturedStaticMeshActor>(Actor);
			if (FractureActor != NULL &&
				FractureActor->FracturedStaticMeshComponent != NULL &&
				FractureActor->FracturedStaticMeshComponent->GetCoreFragmentIndex() != INDEX_NONE &&
				(FractureActor->bBlockActors || FractureActor->bPathColliding))
			{
				bCanExport = TRUE;
			}

			// regular export
			if (bCanExport && RecastExporter::IsActorWithinBounds(Actor, GenerationBounds))
			{
				RecastExporter::ExportActor(Actor, Verts, Faces);
			}
		}
	}

	// other special exports
	RecastExporter::ExportTerrain(Terrains, ClipPlanes, Verts, Faces);
	RecastExporter::ExportLandscapeComponents(LandscapeComponents, ClipPlanes, Verts, Faces);
	RecastExporter::ExportBSP(ClipPlanes, Verts, Faces);

	CompactFaces.Reserve(Faces.Num() * 3);
	for (INT iFace = 0; iFace < Faces.Num(); iFace++)
	{
		CompactFaces.AddItem(Faces(iFace).v0);
		CompactFaces.AddItem(Faces(iFace).v1);
		CompactFaces.AddItem(Faces(iFace).v2);
	}
}

/*------------------------------------------------------------------------------
	UPhysXExporterAsset implementation.
------------------------------------------------------------------------------*/

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UPhysXExporterAsset::InitializeIntrinsicPropertyValues()
{
	SupportedClass = UApexAsset::StaticClass();
	FormatExtension.AddItem( FString(TEXT("ue3.apx")) );
	new(FormatDescription)FString(TEXT("APEX XML Asset - UE3 Coords"));
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("apx")) );
	new(FormatDescription)FString(TEXT("APEX XML Asset"));
}

UBOOL UPhysXExporterAsset::ExportBinary( UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, INT FileIndex, DWORD PortFlags )
{
	UBOOL	ret	= TRUE;
#if WITH_APEX && !FINAL_RELEASE && !WITH_APEX_SHIPPING
	UApexAsset* apexAsset	= CastChecked<UApexAsset>( Object );
	UBOOL	isUE3Coords			= UExporter::CurrentFilename.Right(8) == FString(TEXT(".ue3.apx")) ? TRUE : FALSE;

	ret	= apexAsset->Export(*UExporter::CurrentFilename, isUE3Coords);
#endif

	return ret;
}

IMPLEMENT_CLASS(UPhysXExporterAsset);
