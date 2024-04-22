/*=============================================================================
	UnEdExpTerrain.cpp: Editor terrain exporters.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "UnTerrain.h"
#include "EngineSequenceClasses.h"

/*------------------------------------------------------------------------------
	UTerrainExporterT3D implementation.
------------------------------------------------------------------------------*/
UBOOL			UTerrainExporterT3D::s_bHeightMapExporterArrayFilled = FALSE;
TArray<UClass*>	UTerrainExporterT3D::s_HeightMapExporterArray;

/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainExporterT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = ATerrain::StaticClass();
	bText = 1;
	bExportingTerrainOnly = FALSE;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal terrain text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal terrain text"));
}
UBOOL UTerrainExporterT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	ATerrain* Terrain = CastChecked<ATerrain>(Object);
	if (!Terrain)
	{
		if (Warn)
		{
			Warn->Log(NAME_Error, TEXT("UTerrainExporterT3D passed an object that is not terrain!"));
		}
		return FALSE;
	}

	ObjectExporter = ConstructObject<UObjectExporterT3D>(UObjectExporterT3D::StaticClass());

//	Terrain->ClearComponents();

	// Iterate over all objects making sure they import/export flags are unset. 
	// These are used for ensuring we export each object only once etc.
	for(FObjectIterator It; It; ++It)
	{
		It->ClearFlags(RF_TagImp | RF_TagExp);
	}

	UBOOL bAsSingle = (appStricmp(Type,TEXT("T3D"))==0);

	if (appStricmp(Type,TEXT("COPY")) == 0)
	{
		PortFlags |= PPF_Copy;
	}

	// Lights??
	if (bExportingTerrainOnly)
	{
		Ar.Logf(TEXT("%sBegin Map\r\n"), appSpc(TextIndent));
		TextIndent += 3;
		Ar.Logf(TEXT("%sBegin Level\r\n"), appSpc(TextIndent));
		TextIndent += 3;
	}

	Ar.Logf(TEXT("%sBegin Terrain Class=%s Name=%s\r\n"), appSpc(TextIndent), *Terrain->GetClass()->GetName(), *Terrain->GetName());
	TextIndent += 3;

	// What needs to be written (assumptions!!!!)
	//		array<TerrainHeight>			Heights;
    //		array<TerrainInfoData>			InfoData;
	//		array<TerrainLayer>				Layers;
	//		array<TerrainDecoLayer>			DecoLayers;
	//		array<AlphaMap>					AlphaMaps;
	//		array<TerrainComponent>			TerrainComponents;
	//		int								NumSectionsX;
	//		int								NumSectionsY;
	//		int								SectionSize;
	//		array<TerrainWeightedMaterial>	WeightedMaterials;
	//		array<byte>						CachedDisplacements;
	//		float							MaxCollisionDisplacement;
	//		int								MaxTesselationLevel;
	//		float							TesselationDistanceScale;
	//		array<TerrainMaterialResource>	CachedMaterials;
	//		int								NumVerticesX;
	//		int								NumVerticesY;
	//		int								NumPatchesX;
	//		int								NumPatchesY;
	//		int								MaxComponentSize;
	//		int								StaticLightingResolution;
	//		bool							bIsOverridingLightResolution;
	//		bool							bCastShadow;
	//		bool							bForceDirectLightMap;
	//		const bool						bCastDynamicShadow;
	//		const bool						bBlockRigidBody;
	//		const bool						bAcceptsDynamicLights;
	//		const LightingChannelContainer	LightingChannels;
	//	remaining standard "Actor" information...

	// This will write out the following:
	//		TerrainLayer array
	//		TerrainComponent array (NOT including the static lights)
    //		NumSectionsX, NumSectionsY
    //		SectionSize
    //		MaxTesselationLevel
    //		TesselationDistanceScale
    //		NumVerticesX, NumVerticesY
    //		NumPatchesX, NumPatchesY
    //		MaxComponentSize
    //		Tag
    //		DrawScale3D
    //		Name
	//		
	Ar.Logf(TEXT("%sBegin TerrainActor Class=%s Name=%s\r\n"), appSpc(TextIndent), *Terrain->GetClass()->GetName(), *Terrain->GetName());
	TextIndent += 3;

	TArray<UComponent*> Components;
	Terrain->CollectComponents(Components,TRUE);
	ExportComponentDefinitions( Context, Components, Ar, PortFlags );

#if defined(_TERRAIN_T3D_EXPORT_OLD_SCHOOL_)
	ExportProperties( Context, Ar, Terrain->GetClass(), (BYTE*)Terrain, TextIndent, Terrain->GetArchetype()->GetClass(), (BYTE*)Terrain->GetArchetype(), Terrain, PPF_ExportsNotFullyQualified);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainActor\r\n"), appSpc(TextIndent));

#else	//#if defined(_TERRAIN_T3D_EXPORT_OLD_SCHOOL_)

	// hack to get only the info properties to export...
	AInfo* Info = Cast<AInfo>(Terrain);
	ExportProperties( Context, Ar, AInfo::StaticClass(), (BYTE*)Info, TextIndent, Info->GetArchetype()->GetClass(), (BYTE*)Info->GetArchetype(), Info, PortFlags | PPF_ExportsNotFullyQualified);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainActor\r\n"), appSpc(TextIndent));

	Ar.Logf(TEXT("%sBegin TerrainActorMembers\r\n"), appSpc(TextIndent));
	TextIndent += 3;
	// Now, export out each of the terrain variables... except for the special case ones.
	//		array<TerrainComponent>			TerrainComponents;
	// We don't export terrain components...
	//		int								NumSectionsX;
	//		int								NumSectionsY;
	//		int								SectionSize;
	//		array<TerrainWeightedMaterial>	WeightedMaterials;
	// We don't export weight materials
	//		array<byte>						CachedDisplacements;
	// We don't export cached displacements
	//		int								MaxTesselationLevel;
	Ar.Logf(TEXT("%sMaxTesselationLevel=%d\r\n"),		appSpc(TextIndent), Terrain->MaxTesselationLevel);
	//		float							TesselationDistanceScale;
	Ar.Logf(TEXT("%sTesselationDistanceScale=%f\r\n"),	appSpc(TextIndent), Terrain->TesselationDistanceScale);
	//		int								CollisionTesselationLevel
	Ar.Logf(TEXT("%sCollisionTesselationLevel=%d\r\n"),		appSpc(TextIndent), Terrain->CollisionTesselationLevel);
	//		array<TerrainMaterialResource>	CachedMaterials;
	// We don't export cached materials
	//		int								NumPatchesX;
	//		int								NumPatchesY;
	Ar.Logf(TEXT("%sNumPatchesX=%d\r\n"),				appSpc(TextIndent), Terrain->NumPatchesX); 
	Ar.Logf(TEXT("%sNumPatchesY=%d\r\n"),				appSpc(TextIndent), Terrain->NumPatchesY); 
	//		int								MaxComponentSize;
	Ar.Logf(TEXT("%sMaxComponentSize=%d\r\n"),			appSpc(TextIndent), Terrain->MaxComponentSize);
	//		int								StaticLightingResolution;
	Ar.Logf(TEXT("%sStaticLightingResolution=%d\r\n"),	appSpc(TextIndent), Terrain->StaticLightingResolution);
	//		bool							bIsOverridingLightResolution;
	Ar.Logf(TEXT("%sbIsOverridingLightResolution=%d\r\n"),	appSpc(TextIndent), Terrain->bIsOverridingLightResolution ? 1 : 0);
	//		bool							bCastShadow;
	Ar.Logf(TEXT("%sbCastShadow=%d\r\n"),	appSpc(TextIndent), Terrain->bCastShadow ? 1 : 0);
	//		bool							bForceDirectLightMap;
	Ar.Logf(TEXT("%sbForceDirectLightMap=%d\r\n"),	appSpc(TextIndent), Terrain->bForceDirectLightMap ? 1 : 0);
	//		const bool						bCastDynamicShadow;
	Ar.Logf(TEXT("%sbCastDynamicShadow=%d\r\n"),	appSpc(TextIndent), Terrain->bCastDynamicShadow ? 1 : 0);
	//		const bool						bBlockRigidBody;
	Ar.Logf(TEXT("%sbBlockRigidBody=%d\r\n"),	appSpc(TextIndent), Terrain->bBlockRigidBody ? 1 : 0);
	//		const bool						bAcceptsDynamicLights;
	Ar.Logf(TEXT("%sbAcceptsDynamicLights=%d\r\n"),	appSpc(TextIndent), Terrain->bAcceptsDynamicLights ? 1 : 0);
	//		const LightingChannelContainer	LightingChannels;
	Ar.Logf(TEXT("%sLightingChannels=%d\r\n"),	appSpc(TextIndent), Terrain->LightingChannels.Bitfield);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainActorMembers\r\n"), appSpc(TextIndent));

	// Now, export the special variables...

	//		array<TerrainHeight>			Heights;
	if (!ExportHeightMapData(Terrain, Ar, Warn, PortFlags))
	{
		if (Warn && Warn->YesNof(TEXT("Failed export of height map data.\nContinue?")) == FALSE)
		{
			return FALSE;
		}
	}
    //		array<TerrainInfoData>			InfoData;
	if (!ExportInfoData(Terrain, Ar, Warn, PortFlags))
	{
		if (Warn && Warn->YesNof(TEXT("Failed export of info data.\nContinue?")) == FALSE)
		{
			return FALSE;
		}
	}
	//		array<TerrainLayer>				Layers;
	if (!ExportLayerData(Terrain, Ar, Warn, PortFlags))
	{
		if (Warn && Warn->YesNof(TEXT("Failed export of layer data.\nContinue?")) == FALSE)
		{
			return FALSE;
		}
	}
	//		array<TerrainDecoLayer>			DecoLayers;
	if (!ExportDecoLayerData(Terrain, Ar, Warn, PortFlags))
	{
		if (Warn && Warn->YesNof(TEXT("Failed export of deco layer data.\nContinue?")) == FALSE)
		{
			return FALSE;
		}
	}
	//		array<AlphaMap>					AlphaMaps;
	if (!ExportAlphaMapData(Terrain, Ar, Warn, PortFlags))
	{
		if (Warn && Warn->YesNof(TEXT("Failed export of alpha map data.\nContinue?")) == FALSE)
		{
			return FALSE;
		}
	}
#endif	//#if defined(_TERRAIN_T3D_EXPORT_OLD_SCHOOL_)

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd Terrain\r\n"), appSpc(TextIndent));
	if (bExportingTerrainOnly)
	{
		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd Level\r\n"), appSpc(TextIndent));
		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd Map\r\n"), appSpc(TextIndent));
	}

	return TRUE;
}

UBOOL UTerrainExporterT3D::ExportHeightMapData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD dwPortFlags)
{
	// For heightmaps, we want to provide a variety of export methods.

	// Ensure that all of the height map exporters have been found...
	if (!FindHeightMapExporters(Warn))
	{
		return FALSE;
	}

	// See if the selected one is present.
	UTerrainHeightMapExporter* pkExporter = NULL;

	FString Check;

	if ((dwPortFlags & PPF_Copy) == 0)
	{
		GConfig->GetString(TEXT("UnrealEd.EditorEngine"), TEXT("HeightMapExportClassName"), Check, GEditorIni);
	}
	if (Check.Len() == 0)
	{
		Check = TEXT("TerrainHeightMapExporterTextT3D");
		debugf(TEXT("DEFAULT: Looking for height map exporter: %s"), *Check);
		if ((dwPortFlags & PPF_Copy) == 0)
		{
			GConfig->SetString(TEXT("UnrealEd.EditorEngine"), TEXT("HeightMapExportClassName"), *Check, GEditorIni);
		}
	}
	else
	{
		debugf(TEXT("Looking for height map exporter: %s"), *Check);
	}

	for (INT ii = 0; ii < s_HeightMapExporterArray.Num(); ii++)
	{
		UClass* pkClass = s_HeightMapExporterArray(ii);
		debugf(TEXT("    Checking: %s"), *pkClass->GetName());
		if (appStrcmp(*pkClass->GetName(), *Check) == 0)
		{
			pkExporter = ConstructObject<UTerrainHeightMapExporter>(pkClass);
			break;
		}
	}

	if (pkExporter == NULL)
	{
		if (Warn)
		{
			FString strError;

			strError.Empty();
			strError += TEXT("Failed to find height map exporter ");
			strError += *GEditor->HeightMapExportClassName;
			Warn->Log(NAME_Error, strError);
		}
		return FALSE;
	}

	UBOOL bResult;

	Ar.Logf(TEXT("%sBegin TerrainHeight	Exporter=%s\r\n"), appSpc(TextIndent), *pkExporter->GetClass()->GetName());
	TextIndent += 3;

	pkExporter->TextIndent = TextIndent;
	bResult = pkExporter->ExportHeightData(Terrain, Ar, Warn);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainHeight\r\n"), appSpc(TextIndent));

	return bResult;
}

UBOOL UTerrainExporterT3D::ExportInfoData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UBOOL bResult = TRUE;

	if (Terrain->InfoData.Num())
	{
		Ar.Logf(TEXT("%sBegin TerrainInfoData\r\n"), appSpc(TextIndent));
		TextIndent += 3;

		Ar.Logf(TEXT("%sCount=%d\r\n"), appSpc(TextIndent), Terrain->InfoData.Num());

		TextIndent += 3;
		for (INT jj = 0; jj < Terrain->InfoData.Num(); jj += 8)
		{
			Ar.Logf(TEXT("%s"), appSpc(TextIndent));
			if (jj + 8 < Terrain->InfoData.Num())
			{
				for (INT kk = 0; kk < 8; kk++)
				{
					Ar.Logf(TEXT("%3d"), Terrain->InfoData(jj+kk).Data);
					if (kk == 7)
						Ar.Logf(TEXT("\r\n"));
					else
						Ar.Logf(TEXT("\t"));
				}
			}
			else
			{
				for (INT kk = 0; kk < Terrain->InfoData.Num() - jj; kk++)
				{
					Ar.Logf(TEXT("%3d\t"), Terrain->InfoData(jj+kk).Data);
				}
				Ar.Logf(TEXT("\r\n"));
			}
		}
		TextIndent -= 3;

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd TerrainInfoData\r\n"), appSpc(TextIndent));
	}

	return bResult;
}

UBOOL UTerrainExporterT3D::ExportLayerData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UBOOL bResult = TRUE;

	if (Terrain->Layers.Num())
	{
		Ar.Logf(TEXT("%sBegin TerrainLayerData\r\n"), appSpc(TextIndent));
		TextIndent += 3;

		Ar.Logf(TEXT("%sCount=%d\r\n"), appSpc(TextIndent), Terrain->Layers.Num());

		for (INT ii = 0; ii < Terrain->Layers.Num(); ii++)
		{
			FTerrainLayer& Layer = Terrain->Layers(ii);

			Ar.Logf(TEXT("%sBegin TerrainLayer Index=%d Name=%s\r\n"), appSpc(TextIndent), ii, *Layer.Name);
			TextIndent += 3;

			//class UTerrainLayerSetup*	Setup;
			if (Layer.Setup)
			{
				if (!ExportLayerSetup(Layer.Setup, Ar, Warn, PortFlags))
				{
					bResult = FALSE;
				}
			}
			//INT						AlphaMapIndex;
			Ar.Logf(TEXT("%sAlphaMapIndex=%d\r\n"),	appSpc(TextIndent), Layer.AlphaMapIndex); 
			//BITFIELD					Highlighted : 1;
			Ar.Logf(TEXT("%sHighlighted=%d\r\n"),	appSpc(TextIndent), Layer.Highlighted); 
			//BITFIELD					Hidden : 1;
			Ar.Logf(TEXT("%sHidden=%d\r\n"),		appSpc(TextIndent), Layer.Hidden); 

			TextIndent -= 3;
			Ar.Logf(TEXT("%sEnd TerrainLayer\r\n"), appSpc(TextIndent));
		}

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd TerrainLayerData\r\n"), appSpc(TextIndent));
	}

	return bResult;
}

UBOOL UTerrainExporterT3D::ExportFilterLimit(FFilterLimit& FilterLimit, const TCHAR* Name, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	Ar.Logf(TEXT("%sBegin FilterLimit Name=%s\r\n"), appSpc(TextIndent), Name);
	TextIndent += 3;

	//BITFIELD		Enabled : 1;
	Ar.Logf(TEXT("%sEnabled=%s\r\n"),				appSpc(TextIndent), FilterLimit.Enabled ? TEXT("TRUE") : TEXT("FALSE"));
	//FNoiseParameter	Noise;
	Ar.Logf(TEXT("%sBegin NoiseParameter\r\n"),		appSpc(TextIndent));
	TextIndent += 3;

	//FLOAT	Base;
	Ar.Logf(TEXT("%sBase=%f\r\n"),					appSpc(TextIndent), FilterLimit.Noise.Base);
	//FLOAT	NoiseScale;
	Ar.Logf(TEXT("%sNoiseScale=%f\r\n"),			appSpc(TextIndent), FilterLimit.Noise.NoiseScale);
	//FLOAT	NoiseAmount;
	Ar.Logf(TEXT("%sNoiseAmount=%f\r\n"),			appSpc(TextIndent), FilterLimit.Noise.NoiseAmount);

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd NoiseParameter\r\n"),		appSpc(TextIndent));

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd FilterLimit\r\n"), appSpc(TextIndent));

	return TRUE;
}

UBOOL UTerrainExporterT3D::ExportLayerSetup(UTerrainLayerSetup* Setup, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	Ar.Logf(TEXT("%sBegin TerrainLayerSetup Name=%s\r\n"), appSpc(TextIndent), *Setup->GetPathName());
	Ar.Logf(TEXT("%sEnd TerrainLayerSetup\r\n"), appSpc(TextIndent));
	return TRUE;
}

UBOOL UTerrainExporterT3D::ExportDecoLayerData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UBOOL bResult = TRUE;

	if (Terrain->DecoLayers.Num())
	{
		Ar.Logf(TEXT("%sBegin TerrainDecoLayerData\r\n"), appSpc(TextIndent));
		TextIndent += 3;

		Ar.Logf(TEXT("%sCount=%d\r\n"), appSpc(TextIndent), Terrain->DecoLayers.Num());

		for (INT ii = 0; ii < Terrain->DecoLayers.Num(); ii++)
		{
			FTerrainDecoLayer& DecoLayer = Terrain->DecoLayers(ii);

			Ar.Logf(TEXT("%sBegin TerrainDecoLayer Index=%d Name=%s\r\n"), 
				appSpc(TextIndent), ii, *DecoLayer.Name);
			TextIndent += 3;

			//TArrayNoInit<FTerrainDecoration>	Decorations;
			Ar.Logf(TEXT("%sDecorationCount=%d\r\n"),	appSpc(TextIndent), DecoLayer.Decorations.Num());

			for (INT jj = 0; jj < DecoLayer.Decorations.Num(); jj++)
			{
				FTerrainDecoration& Decoration = DecoLayer.Decorations(jj);
				if (!ExportDecoration(Decoration, jj, Ar, Warn, PortFlags))
				{
					bResult = FALSE;
				}
			}

			//INT									AlphaMapIndex;
			Ar.Logf(TEXT("%sAlphaMapIndex=%d\r\n"),		appSpc(TextIndent), DecoLayer.AlphaMapIndex); 

			TextIndent -= 3;
			Ar.Logf(TEXT("%sEnd TerrainDecoLayer\r\n"),		appSpc(TextIndent));
		}

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd TerrainDecoLayerData\r\n"), appSpc(TextIndent));
	}

	return bResult;
}

UBOOL UTerrainExporterT3D::ExportDecoration(FTerrainDecoration& Decoration, INT Index, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	bool bResult = TRUE;

	Ar.Logf(TEXT("%sBegin TerrainDecoration Index=%d\r\n"),		appSpc(TextIndent), Index);
	TextIndent += 3;

	//class UPrimitiveComponentFactory*			Factory;
	ObjectExporter->TextIndent = TextIndent;
	ObjectExporter->ExportText(NULL, Decoration.Factory, TEXT("T3D"), Ar, Warn, PortFlags);
	//FLOAT										MinScale;
	Ar.Logf(TEXT("%sMinScale=%f\r\n"),							appSpc(TextIndent), Decoration.MinScale);
	//FLOAT										MaxScale;
	Ar.Logf(TEXT("%sMaxScale=%f\r\n"),							appSpc(TextIndent), Decoration.MaxScale);
	//FLOAT										Density;
	Ar.Logf(TEXT("%sDensity=%f\r\n"),							appSpc(TextIndent), Decoration.Density);
	//FLOAT										SlopeRotationBlend;
	Ar.Logf(TEXT("%sSlopeRotationBlend=%f\r\n"),				appSpc(TextIndent), Decoration.SlopeRotationBlend);
	//INT										RandSeed;
	Ar.Logf(TEXT("%sRandSeed=%d\r\n"),							appSpc(TextIndent), Decoration.RandSeed);
	//TArrayNoInit<FTerrainDecorationInstance>	Instances;
	Ar.Logf(TEXT("%sBegin TerrainDecorationInstanceArray Count=%d\r\n"),	appSpc(TextIndent), Decoration.Instances.Num());
	TextIndent += 3;

	for (INT ii = 0; ii < Decoration.Instances.Num(); ii++)
	{
		FTerrainDecorationInstance& Instance = Decoration.Instances(ii);
		if (!ExportDecorationInstance(Instance, ii, Ar, Warn, PortFlags))
		{
			bResult = FALSE;
		}
	}

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainDecorationInstanceArray\r\n"),	appSpc(TextIndent));

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainDecoration\r\n"),				appSpc(TextIndent));

	return bResult;
}

UBOOL UTerrainExporterT3D::ExportDecorationInstance(FTerrainDecorationInstance& DecorationInst, INT Index, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	bool bResult = TRUE;

	Ar.Logf(TEXT("%sBegin TerrainDecorationInstance Index=%d\r\n"),		appSpc(TextIndent), Index);
	TextIndent += 3;

	//class UPrimitiveComponent*	Component;
	ObjectExporter->TextIndent = TextIndent;
	ObjectExporter->ExportText(NULL, DecorationInst.Component, TEXT("T3D"), Ar, Warn, PortFlags);
	//FLOAT							X;
	Ar.Logf(TEXT("%sX=%f\r\n"),		appSpc(TextIndent), DecorationInst.X); 
	//FLOAT							Y;
	Ar.Logf(TEXT("%sY=%f\r\n"),		appSpc(TextIndent), DecorationInst.Y); 
	//FLOAT							Scale;
	Ar.Logf(TEXT("%sScale=%f\r\n"),	appSpc(TextIndent), DecorationInst.Scale); 
	//INT							Yaw;
	Ar.Logf(TEXT("%sYaw=%d\r\n"),	appSpc(TextIndent), DecorationInst.Yaw); 

	TextIndent -= 3;
	Ar.Logf(TEXT("%sEnd TerrainDecorationInstance\r\n"),			appSpc(TextIndent));

	return bResult;
}

UBOOL UTerrainExporterT3D::ExportAlphaMapData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UBOOL bResult = TRUE;

	if (Terrain->AlphaMaps.Num())
	{
		Ar.Logf(TEXT("%sBegin TerrainAlphaMapData\r\n"), appSpc(TextIndent));
		TextIndent += 3;

		Ar.Logf(TEXT("%sCount=%d\r\n"), appSpc(TextIndent), Terrain->AlphaMaps.Num());

		//@todo. Implement alpha map exporters...
		for (INT ii = 0; ii < Terrain->AlphaMaps.Num(); ii++)
		{
			FAlphaMap& AlphaMap = Terrain->AlphaMaps(ii);
			Ar.Logf(TEXT("%sBegin AlphaMap Index=%d Count=%d\r\n"), appSpc(TextIndent), ii, AlphaMap.Data.Num());
			TextIndent += 3;

			for (INT jj = 0; jj < AlphaMap.Data.Num(); jj += 8)
			{
				Ar.Logf(TEXT("%s"), appSpc(TextIndent));
				if (jj + 8 < AlphaMap.Data.Num())
				{
					for (INT kk = 0; kk < 8; kk++)
					{
						Ar.Logf(TEXT("%3d"), AlphaMap.Data(jj+kk));
						if (kk == 7)
							Ar.Logf(TEXT("\r\n"));
						else
							Ar.Logf(TEXT("\t"));
					}
				}
				else
				{
					for (INT kk = 0; kk < AlphaMap.Data.Num() - jj; kk++)
					{
						Ar.Logf(TEXT("%3d\t"), AlphaMap.Data(jj+kk));
					}
					Ar.Logf(TEXT("\r\n"));
				}
			}

			TextIndent -= 3;
			Ar.Logf(TEXT("%sEnd AlphaMap\r\n"), appSpc(TextIndent));
		}

		TextIndent -= 3;
		Ar.Logf(TEXT("%sEnd TerrainAlphaMapData\r\n"), appSpc(TextIndent));
	}

	return bResult;
}

UBOOL UTerrainExporterT3D::FindHeightMapExporters(FFeedbackContext* Warn)
{
	if (s_bHeightMapExporterArrayFilled)
		return TRUE;

	for (TObjectIterator<UClass> It; It; ++It)
	{
		if (It->IsChildOf(UTerrainHeightMapExporter::StaticClass()))
		{
			debugf(TEXT("FindHeightMapExporters: Checking class %32s"), *It->GetName());

			// Find all TerrainHeightExporter classes (ignoring abstract TerrainHeightExporter class)
			s_HeightMapExporterArray.AddItem(*It);
		}
	}

	if (s_HeightMapExporterArray.Num() == 0)
	{
		if (Warn)
		{
			Warn->Log(NAME_Warning, TEXT("TerrainExporterT3D did not find any height map exporters!"));
		}
		return FALSE;
	}

	s_bHeightMapExporterArrayFilled = TRUE;

	for (INT ii = 0; ii < s_HeightMapExporterArray.Num(); ii++)
	{
		debugf(TEXT("HM Exporter..............%s"), *s_HeightMapExporterArray(ii)->GetName());
	}

	return TRUE;
}

IMPLEMENT_CLASS(UTerrainExporterT3D);

/*------------------------------------------------------------------------------
	UTerrainHeightMapExporter implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainHeightMapExporter::InitializeIntrinsicPropertyValues()
{
	SupportedClass = ATerrain::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal heightmap exporter"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal heightmap exporter"));
}
UBOOL UTerrainHeightMapExporter::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	if (Warn)
	{
		Warn->Log(NAME_Error, TEXT("UTerrainHeightMapExporter::ExportText called!"));
	}
	return FALSE;
}

UBOOL UTerrainHeightMapExporter::ExportHeightData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	if (Warn)
	{
		Warn->Log(NAME_Error, TEXT("UTerrainHeightMapExporter::ExportHeightData called"));
	}
	return FALSE;
}

UBOOL UTerrainHeightMapExporter::ExportHeightDataToFile(ATerrain* Terrain, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags)
{
	if (Warn)
	{
		Warn->Log(NAME_Error, TEXT("UTerrainHeightMapExporter::ExportHeighDataToFile called"));
	}
	return FALSE;
}

UBOOL UTerrainHeightMapExporter::ExportLayerDataToFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags)
{
	if (Warn)
	{
		Warn->Log(NAME_Error, TEXT("UTerrainHeightMapExporter::ExportLayerDataToFile called"));
	}
	return FALSE;
}

IMPLEMENT_CLASS(UTerrainHeightMapExporter);

/*------------------------------------------------------------------------------
	UTerrainHeightMapExporterTextT3D implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainHeightMapExporterTextT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = ATerrain::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal heightmap text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal heightmap text"));
}
UBOOL UTerrainHeightMapExporterTextT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	ATerrain* Terrain = CastChecked<ATerrain>(Object);
	if (!Terrain)
	{
		if (Warn)
		{
			Warn->Log(NAME_Error, TEXT("UTerrainHeightMapExporterTextT3D passed an object that is not terrain!"));
		}
		return FALSE;
	}

	return ExportHeightData(Terrain, Ar, Warn);
}

UBOOL UTerrainHeightMapExporterTextT3D::ExportHeightData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	// For now, just blaze it out...
	Ar.Logf(TEXT("%sCount=%d\tWidth=%d\tHeight=%d\r\n"), appSpc(TextIndent), 
		Terrain->Heights.Num(), Terrain->NumVerticesX, Terrain->NumVerticesY);
	for (INT ii = 0; ii < Terrain->Heights.Num(); ii+=8)
	{
		Ar.Logf(TEXT("%s"), appSpc(TextIndent));
		if (ii + 8 < Terrain->Heights.Num())
		{
			for (INT jj = 0; jj < 8; jj++)
			{
				Ar.Logf(TEXT("%5d"), Terrain->Heights(ii+jj).Value);
				if (jj == 7)
					Ar.Logf(TEXT("\r\n"));
				else
					Ar.Logf(TEXT("\t"));
			}
		}
		else
		{
			for (INT jj = 0; jj < Terrain->Heights.Num() - ii; jj++)
			{
				Ar.Logf(TEXT("%5d"), Terrain->Heights(ii+jj).Value);
				Ar.Logf(TEXT("\t"));
			}
			Ar.Logf(TEXT("\r\n"));
		}
	}

	return TRUE;
}

UBOOL UTerrainHeightMapExporterTextT3D::ExportHeightDataToFile(ATerrain* Terrain, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags)
{
	return FALSE;
}

UBOOL UTerrainHeightMapExporterTextT3D::ExportLayerDataToFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags)
{
	return FALSE;
}

IMPLEMENT_CLASS(UTerrainHeightMapExporterTextT3D);

/*------------------------------------------------------------------------------
	UTerrainHeightMapExporterG16BMPT3D implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainHeightMapExporterG16BMPT3D::InitializeIntrinsicPropertyValues()
{
	SupportedClass = ATerrain::StaticClass();
	bText = 1;
	PreferredFormatIndex = FormatExtension.AddItem( FString(TEXT("T3D")) );
	new(FormatDescription)FString(TEXT("Unreal heightmap text"));
	new(FormatExtension)FString(TEXT("COPY"));
	new(FormatDescription)FString(TEXT("Unreal heightmap text"));
}
UBOOL UTerrainHeightMapExporterG16BMPT3D::ExportText(const FExportObjectInnerContext* Context, UObject* Object, const TCHAR* Type, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	ATerrain* Terrain = CastChecked<ATerrain>(Object);
	if (!Terrain)
	{
		if (Warn)
		{
			Warn->Log(NAME_Error, TEXT("UTerrainHeightMapExporterG16BMPT3D passed an object that is not terrain!"));
		}
		return FALSE;
	}

	return ExportHeightData(Terrain, Ar, Warn);
}

UBOOL UTerrainHeightMapExporterG16BMPT3D::ExportHeightData(ATerrain* Terrain, FOutputDevice& Ar, FFeedbackContext* Warn, DWORD PortFlags)
{
	UBOOL bResult = TRUE;

	check(Terrain->Heights.Num() == (Terrain->NumVerticesX * Terrain->NumVerticesY));
	// 
	Ar.Logf(TEXT("%sCount=%d\r\n"), appSpc(TextIndent), Terrain->Heights.Num());

	//@todo. HOW DO WE GET THE DIRECTORY???
	FString strFileName(*Terrain->GetName());
	strFileName += TEXT("_G16.BMP");
	Ar.Logf(TEXT("%sFileName=%s\r\n"), appSpc(TextIndent), *strFileName);

	FFilename	Filename(CurrentFilename);

	if (!ExportHeightDataToFile(Terrain, *(Filename.GetPath()), Warn, PortFlags))
	{
		bResult = FALSE;
	}

	return bResult;
}

UBOOL UTerrainHeightMapExporterG16BMPT3D::ExportHeightDataToFile(ATerrain* Terrain, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags)
{
	UBOOL bResult = TRUE;

	FBufferArchive Buffer;

	UTextureExporterBMP* pkBMPExporter = ConstructObject<UTextureExporterBMP>(UTextureExporterBMP::StaticClass());
	check(pkBMPExporter);

	// Allocate a temp data buffer
	WORD* wData = new WORD[Terrain->Heights.Num()];
	check(wData);

	// Copy the heights into it
	for (INT ii = 0; ii < Terrain->Heights.Num(); ii++)
	{
		wData[ii] = Terrain->Heights(ii).Value;
	}

	// Export it
	if (!pkBMPExporter->ExportBinary((BYTE*)wData, PF_G16, Terrain->NumVerticesX, Terrain->NumVerticesY, TEXT("BMP"), Buffer, Warn, PortFlags))
	{
		bResult = FALSE;
	}

	// Clean-up the data
	delete [] wData;

	if (bResult)
	{
		TCHAR FileName[MAX_SPRINTF]=TEXT("");
		appSprintf(FileName, TEXT("%s\\%s_G16.BMP"), Directory, *Terrain->GetName());
		if (!appSaveArrayToFile(Buffer, FileName))
		{
			warnf(*LocalizeError(TEXT("ExportHeightData"),TEXT("UTerrainHeightMapExporterG16BMPT3D")), *Terrain->GetFullName(), FileName);
			bResult = FALSE;
		}
	}

	return bResult;
}

UBOOL UTerrainHeightMapExporterG16BMPT3D::ExportLayerDataToFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* Directory, FFeedbackContext* Warn, DWORD PortFlags)
{
	if (Layer->AlphaMapIndex >= Terrain->AlphaMaps.Num())
	{
		return FALSE;
	}

	FAlphaMap& AlphaMap = Terrain->AlphaMaps(Layer->AlphaMapIndex);
	INT Count = AlphaMap.Data.Num();
	if (Count == 0)
	{
		return FALSE;
	}

	UBOOL bResult = TRUE;

	FBufferArchive Buffer;

	UTextureExporterBMP* pkBMPExporter = ConstructObject<UTextureExporterBMP>(UTextureExporterBMP::StaticClass());
	check(pkBMPExporter);

	// Allocate a temp data buffer
	WORD* wData = new WORD[Count];
	check(wData);

	// Copy the heights into it
	for (INT ii = 0; ii < Count; ii++)
	{
		WORD Value = (WORD)(AlphaMap.Data(ii));
		// Convert the value to 16-bit
		wData[ii] = Value * (65535 / 255);
	}

	// Export it
	if (!pkBMPExporter->ExportBinary((BYTE*)wData, PF_G16, Terrain->NumVerticesX, Terrain->NumVerticesY, TEXT("BMP"), Buffer, Warn, PortFlags))
	{
		bResult = FALSE;
	}

	// Clean-up the data
	delete [] wData;

	if (bResult)
	{
		TCHAR FileName[MAX_SPRINTF]=TEXT("");
		appSprintf(FileName, TEXT("%s\\%s_%s_G16.BMP"), Directory, *Terrain->GetName(), *(Layer->Name));
		if (!appSaveArrayToFile(Buffer, FileName))
		{
			warnf(*LocalizeError(TEXT("ExportLayerData"),TEXT("UTerrainHeightMapExporterG16BMPT3D")), *Terrain->GetFullName(), FileName);
			bResult = FALSE;
		}
	}

	return bResult;
}

IMPLEMENT_CLASS(UTerrainHeightMapExporterG16BMPT3D);

