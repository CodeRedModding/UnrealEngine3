/*=============================================================================
	UnEdFact.cpp: Editor class terrain factories.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "UnrealEd.h"
#include "Factories.h"
#include "UnTerrain.h"
#include "EditorImageClasses.h"

void UnEdFact_SkipWhitespace(const TCHAR*& Str)
{
	while ((*Str == ' ') || (*Str == 9))
	{
		Str++;
	}
}

void UnEdFact_SkipNumeric(const TCHAR*& Str)
{
	while ((*Str >= '0') && (*Str <= '9'))
	{
		Str++;
	}
}

UBOOL UnEdFact_GetPropertyText(	const TCHAR*& Buffer, const TCHAR* Keyword, FString& PropText)
{
	// Get property text.
	FString StrTemp;
	while ((GetEND(&Buffer, Keyword)==0) && ParseLine(&Buffer, StrTemp))
	{
		PropText += *StrTemp;
		PropText += TEXT("\r\n");
	}

	// Bump off the 'END' line...
	ParseLine(&Buffer, StrTemp);

	return TRUE;
}

/*------------------------------------------------------------------------------
	UTerrainFactory implementation.
------------------------------------------------------------------------------*/
/**
 * Initializes property values for intrinsic classes.  It is called immediately after the class default object
 * is initialized against its archetype, but before any objects of this class are created.
 */
void UTerrainFactory::InitializeIntrinsicPropertyValues()
{
	SupportedClass = ATerrain::StaticClass();
	new(Formats)FString(TEXT("t3d;Unreal Terrain"));
	bCreateNew	= 0;
	bText		= 1;
	Description	= TEXT("Terrain");
}
UTerrainFactory::UTerrainFactory()
{
	bEditorImport = 1;
}


UObject* UTerrainFactory::FactoryCreateText(
	UClass* Class, 
	UObject* InParent, 
	FName Name, 
	EObjectFlags Flags, 
	UObject* Context, 
	const TCHAR* Type, 
	const TCHAR*& Buffer, 
	const TCHAR* BufferEnd, 
	FFeedbackContext* Warn)
{
	UBOOL bIsPasting = (appStricmp(Type,TEXT("paste"))==0);

/***	FORMAT OF TEXT....
      Begin Terrain Class=Terrain Name=Terrain_0
         Begin TerrainActor Class=Terrain Name=Terrain_0
             Begin Object Class=Terrain_SpriteComponent0_Class Name=SpriteComponent0
                 Name="Terrain_SpriteComponent0_Class_0"
             End Object
             Tag="Terrain"
             Location=(X=-544.000000,Y=592.000000)
             DrawScale3D=(X=256.000000,Y=256.000000,Z=256.000000)
             Name="Terrain_0"
         End TerrainActor
         Begin TerrainActorMembers
            NumSectionsX=1
            NumSectionsY=1
            SectionSize=0
            MaxCollisionDisplacement=0.250000
            MaxTesselationLevel=4
            TesselationDistanceScale=1.000000
            NumVerticesX=17
            NumVerticesY=17
            NumPatchesX=16
            NumPatchesY=16
            MaxComponentSize=16
            StaticLightingResolution=4
         End TerrainActorMembers
         Begin TerrainHeight	Exporter=TerrainHeightMapExporterTextT3D
			...
         End TerrainHeight
		 Begin TerrainInfoData
			Count=#
			   ...
		 End TerrainInfoData
         Begin TerrainLayerData
			...
         End TerrainLayerData
         Begin TerrainDecoLayerData
			...
         End TerrainDecoLayerData
         Begin TerrainAlphaMapData
			...
         End TerrainAlphaMapData
      End Terrain
***/
	ATerrain* OutTerrain = NULL;

	ParseNext(&Buffer);

	INT	iMemberIndex = -1;

	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str,TEXT("TERRAIN")))
		{
			// End of terrain.
			// The PostEditChange call will actually do the proper terrain setup... 
			// (ie, call Allocate, etc.)
			break;
		}
		else 
		if (GetBEGIN(&Str,TEXT("TERRAINACTOR")))
		{
			Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainActor"));

			UClass* TempClass;
			if (ParseObject<UClass>(Str, TEXT("CLASS="), TempClass, ANY_PACKAGE))
			{
				// Get actor name.
				FName ActorName(NAME_None);
				Parse(Str, TEXT("NAME="), ActorName);

				// Make sure this name is unique.
				AActor* Found = NULL;
				if (ActorName != NAME_None)
				{
					// look in the current level for the same named actor
					Found = FindObject<AActor>(GWorld->CurrentLevel, *ActorName.ToString());
				}
				if (Found)
				{
					Found->Rename();
				}

				// Import it.
				AActor* Actor = GWorld->SpawnActor(TempClass, ActorName, FVector(0,0,0), TempClass->GetDefaultActor()->Rotation, NULL, 1, 0);
				check(Actor);

				// Get property text.
				FString PropText;
				UnEdFact_GetPropertyText(Buffer, TEXT("TERRAINACTOR"), PropText);
				ActorMap->Set(Actor, *PropText);

				OutTerrain = (ATerrain*)Actor;
			}
		}
		else 
		if (GetEND(&Str,TEXT("TERRAINACTOR")))
		{
			// This should be handled in the BEGIN block for TerrainActor
			debugf(TEXT("Illegal parse of TerrainActor!"));
		}
		else 
		if (GetEND(&Str,TEXT("TERRAINACTORMEMBERS")))
		{
			iMemberIndex = -1;
			// We have to call Allocate here to ensure that we don't lose layer/height data...
			check(OutTerrain);
			OutTerrain->Allocate();
		}
		else
		if (GetBEGIN(&Str,TEXT("TERRAINACTORMEMBERS")) || (iMemberIndex >= 0))
		{
			INT	iValue;
			FLOAT fValue;

			if (iMemberIndex < 0)
			{
				Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainActorMembers"));
				// Skip to the next line
				iMemberIndex = 0;
			}
			else
			//MaxTesselationLevel=#
			if (Parse(Str, TEXT("MAXTESSELATIONLEVEL="),	iValue))
			{
				OutTerrain->MaxTesselationLevel = iValue;
			}
			else
			//TesselationDistanceScale=#.#
			if (Parse(Str, TEXT("TESSELATIONDISTANCESCALE="),	fValue))
			{
				OutTerrain->TesselationDistanceScale = fValue;
			}
			else
			//CollisionTesselationLevel=#
			if (Parse(Str, TEXT("COLLISIONTESSELATIONLEVEL="),	iValue))
			{
				OutTerrain->CollisionTesselationLevel = iValue;
			}
			else
			//NumPatchesX=#
			if (Parse(Str, TEXT("NUMPATCHESX="),	iValue))
			{
				OutTerrain->NumPatchesX = iValue;
			}
			else
			//NumPatchesY=#
			if (Parse(Str, TEXT("NUMPATCHESY="),	iValue))
			{
				OutTerrain->NumPatchesY = iValue;
			}
			else
			//MaxComponentSize=#
			if (Parse(Str, TEXT("MAXCOMPONENTSIZE="),	iValue))
			{
				OutTerrain->MaxComponentSize = iValue;
			}
			else
			//StaticLightingResolution=#
			if (Parse(Str, TEXT("STATICLIGHTINGRESOLUTION="),	iValue))
			{
				OutTerrain->StaticLightingResolution = iValue;
			}
			else
			//bIsOverridingLightResolution=#
			if (Parse(Str, TEXT("BISOVERRIDINGLIGHTRESOLUTION="),	iValue))
			{
				OutTerrain->bIsOverridingLightResolution = (iValue == 1) ? TRUE : FALSE;
			}
			else
			//bCastShadow=#
			if (Parse(Str, TEXT("BISOVERRIDINGLIGHTRESOLUTION="),	iValue))
			{
				OutTerrain->bCastShadow = (iValue == 1) ? TRUE : FALSE;
			}
			else
			//bForceDirectLightMap=#
			if (Parse(Str, TEXT("BFORCEDIRECTLIGHTMAP="),	iValue))
			{
				OutTerrain->bForceDirectLightMap = (iValue == 1) ? TRUE : FALSE;
			}
			else
			//bCastDynamicShadow=#
			if (Parse(Str, TEXT("BCASTDYNAMICSHADOW="),	iValue))
			{
				OutTerrain->bCastDynamicShadow = (iValue == 1) ? TRUE : FALSE;
			}
			else
			//bBlockRigidBody=#
			if (Parse(Str, TEXT("BBLOCKRIGIDBODY="),	iValue))
			{
				OutTerrain->bBlockRigidBody = (iValue == 1) ? TRUE : FALSE;
			}
			else
			//bAcceptsDynamicLights=#
			if (Parse(Str, TEXT("BACCEPTSDYNAMICLIGHTS="),	iValue))
			{
				OutTerrain->bAcceptsDynamicLights = (iValue == 1) ? TRUE : FALSE;
			}
			else
			//LightingChannels=#
			if (Parse(Str, TEXT("LIGHTINGCHANNELS="),	iValue))
			{
				OutTerrain->LightingChannels.Bitfield = iValue;
			}
			else
			{
				// PROBLEM!!!
				Warn->StatusUpdatef(0, 0, TEXT("Unknown terrain actor member: %s"), Str);
			}

			// Always index to the next value...
			iMemberIndex++;
		}
		else
		if (GetBEGIN(&Str,TEXT("TERRAINHEIGHT")))
		{
			Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainHeight"));

			TCHAR ExporterName[NAME_SIZE];
			if (Parse(Str,TEXT("EXPORTER="), ExporterName, NAME_SIZE))
			{
				// Use this exporter to import the terrain height
				Warn->StatusUpdatef( 0, 0, TEXT("Using %s"), ExporterName);

				// Parse off the "TerrainHeightExporter" portion...
				TCHAR* ExporterType = ExporterName + appStrlen(TEXT("TerrainHeightMapExporter"));
				TCHAR ImporterName[MAX_SPRINTF]=TEXT("");
				appSprintf(ImporterName, TEXT("TerrainHeightMapFactory%s"), ExporterType);

				UTerrainHeightMapFactory* Importer = GetHeightMapImporter(ImporterName);
				if (Importer)
				{
					if (!Importer->ImportHeightDataFromText(OutTerrain, Buffer, BufferEnd, Warn))
					{
						Warn->StatusUpdatef( 0, 0, TEXT("Failed to import terrain height map"));
					}
				}
				else
				{
					Warn->StatusUpdatef( 0, 0, TEXT("Unknown terrain height exporter %s"), ExporterName);
				}
			}
		}
		else 
		if (GetEND(&Str,TEXT("TERRAINHEIGHT")))
		{
		}
		else
		if (GetBEGIN(&Str,TEXT("TERRAININFODATA")))
		{
			if (ParseLine(&Buffer,StrLine))
			{
				Str = *StrLine;

				// Parse the count
				// Count=#
				INT Value;
				if (Parse(Str, TEXT("Count="),	Value))
				{
					if (Value > 0)
					{
						FString StrLine;

						Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainInfoData Starting..."));

						OutTerrain->InfoData.Empty(Value);
						for (INT ii = 0; ii < Value; ii += 8)
						{
							if (!ParseLine(&Buffer,StrLine))
							{
								// Error??
								break;
							}

							Str = *StrLine;
							// Skip white-space to the first number
							UnEdFact_SkipWhitespace(Str);

							INT CheckCount = 8;
							if (ii + 8 >= Value)
							{
								CheckCount = Value - ii;
							}
							// Parse values
							for (INT jj = 0; jj < CheckCount; jj++)
							{
								INT TempData;
								appSSCANF(Str, TEXT("%d\t"), &TempData);

								BYTE DataValue = (BYTE)TempData;
								OutTerrain->InfoData.AddItem(DataValue);

								// Advanced past the number...
								UnEdFact_SkipNumeric(Str);
								// And the whitespace...
								UnEdFact_SkipWhitespace(Str);
							}
						}
						Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainInfoData Completed"));
					}
					else
					{
						OutTerrain->InfoData.Empty();
					}
				}
			}
		}
		else 
		if (GetEND(&Str,TEXT("TERRAININFODATA")))
		{
		}
		else
		if (GetBEGIN(&Str,TEXT("TERRAINLAYERDATA")))
		{
			Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainLayerData"));
			if (!ParseLayerData(OutTerrain, Buffer, BufferEnd, Warn))
			{
				Warn->StatusUpdatef( 0, 0, TEXT("Failed to import TerrainLayerData"));
			}
		}
		else 
		if (GetEND(&Str,TEXT("TERRAINLAYERDATA")))
		{
		}
		else
		if (GetBEGIN(&Str,TEXT("TERRAINDECOLAYERDATA")))
		{
			Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainDecoLayerData"));
			if (!ParseDecoLayerData(OutTerrain, Buffer, BufferEnd, Warn))
			{
				Warn->StatusUpdatef( 0, 0, TEXT("Failed to import TerrainDecoLayerData"));
			}
		}
		else 
		if (GetEND(&Str,TEXT("TERRAINDECOLAYERDATA")))
		{
		}
		else
		if (GetBEGIN(&Str,TEXT("TERRAINALPHAMAPDATA")))
		{
			Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainAlphaMapData"));
			if (!ParseAlphaMapData(OutTerrain, Buffer, BufferEnd, Warn))
			{
				Warn->StatusUpdatef( 0, 0, TEXT("Failed to import TerrainAlphaMapData"));
			}
		}
		else 
		if (GetEND(&Str,TEXT("TERRAINALPHAMAPDATA")))
		{
		}
		else
		{
			Warn->StatusUpdatef( 0, 0, TEXT("UNKNOWN LINE %s"), Str);
		}
	}

	Warn->StatusUpdatef( 0, 0, TEXT("Terrain Import Complete"));
	if (OutTerrain)
	{
		OutTerrain->PostEditChange();
	}

	return OutTerrain;
}

UBOOL UTerrainFactory::ParseNoiseParameter(FNoiseParameter* Noise, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin NoiseParameter
		Base=0.000000
		NoiseScale=0.000000
		NoiseAmount=0.000000
	End NoiseParameter
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("NOISEPARAMETER")))
		{
			// End of noise parameter.
			break;
		}
		else
		{
			FLOAT fValue;

			if (Parse(Str, TEXT("BASE="), fValue))
			{
				Noise->Base = fValue;
			}
			else
			if (Parse(Str, TEXT("NOISESCALE="), fValue))
			{
				Noise->NoiseScale = fValue;
			}
			else
			if (Parse(Str, TEXT("NOISEAMOUNT="), fValue))
			{
				Noise->NoiseAmount = fValue;
			}
			else
			{
				debugf(TEXT("NoiseParameter unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseFilterLimit(FFilterLimit* Limit, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin FilterLimit Name=XXX
		Enabled=FALSE
		Begin NoiseParameter
			...
		End NoiseParameter
	End FilterLimit
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("FILTERLIMIT")))
		{
			// End of filter limit.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("NOISEPARAMETER")))
		{
			if (!ParseNoiseParameter(&Limit->Noise, Buffer, BufferEnd, Warn))
			{
				bResult = FALSE;
				break;
			}
		}
		else
		if (GetEND(&Str, TEXT("NOISEPARAMETER")))
		{
		}
		else
		{
			FString strBoolean;

			if (Parse(Str, TEXT("ENABLED="), strBoolean))
			{
				if (strBoolean == TEXT("TRUE"))
					Limit->Enabled = TRUE;
				else
					Limit->Enabled = FALSE;
			}
			else
			{
				debugf(TEXT("FilterLimit unhandle string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseLayerSetupMaterial(ATerrain* Terrain, UTerrainLayerSetup& Setup, FTerrainFilteredMaterial& Material, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin LayerSetupMaterial Index=0
		UseNoise=FALSE
		NoiseScale=0.000000
		NoisePercent=0.000000
		Begin FilterLimit Name=MinHeight
			...
		End FilterLimit
		Begin FilterLimit Name=MaxHeight
			...
		End FilterLimit
		Begin FilterLimit Name=MinSlope
			...
		End FilterLimit
		Begin FilterLimit Name=MaxSlope
			...
		End FilterLimit
		Alpha=1.000000
		Begin Object Class=TerrainMaterial Name=RockTerrainMaterial
			...
		End Object
    End LayerSetupMaterial
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("LAYERSETUPMATERIAL")))
		{
			// End of layer setup material.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("FILTERLIMIT")))
		{
			// Parse the name
			FString	strName;
			if (!Parse(Str, TEXT("NAME="), strName))
			{
				bResult = FALSE;
				break;
			}

			FFilterLimit* Limit = NULL;

			if (strName == TEXT("MinHeight"))
				Limit = &Material.MinHeight;
			if (strName == TEXT("MaxHeight"))
				Limit = &Material.MaxHeight;
			if (strName == TEXT("MinSlope"))
				Limit = &Material.MinSlope;
			if (strName == TEXT("MaxSlope"))
				Limit = &Material.MaxSlope;

			if (Limit == NULL)
			{
				bResult = FALSE;
				break;
			}

			if (!ParseFilterLimit(Limit, Buffer, BufferEnd, Warn))
			{
				bResult = FALSE;
				break;
			}
		}
		else
		if (GetEND(&Str, TEXT("FILTERLIMIT")))
		{
		}
		else
		if (GetBEGIN(&Str, TEXT("OBJECT")))
		{
/***
            Begin Object Class=TerrainMaterial Name=RockTerrainMaterial
                LocalToMapping=(XPlane=(X=0.250000),YPlane=(Y=0.250000),ZPlane=(Z=0.250000),WPlane=(W=1.000000))
                Material=Material'Terrain.Rock.Neutral_RockFace3_Mat'
                DisplacementMap=Texture2D'Terrain.Rock.Neutral_RockFace3_Dis'
                Name="RockTerrainMaterial"
            End Object
***/
			// Parse the class and name
			FString strClass, strName;

			if (!Parse(Str, TEXT("CLASS="), strClass))
			{
			}
			if (!Parse(Str, TEXT("NAME="), strName))
			{
			}

			// Get property text.
			FString PropText;
			UnEdFact_GetPropertyText(Buffer, TEXT("OBJECT"), PropText);

			Material.Material = FindObject<UTerrainMaterial>(ANY_PACKAGE, *strName);
			if (!Material.Material)
			{
				FName	Name(*strName);

				Material.Material = ConstructObject<UTerrainMaterial>(UTerrainMaterial::StaticClass(), Terrain, Name);
				check(Material.Material);

				Material.Material->PreEditChange(NULL);
				ImportObjectProperties((BYTE*)Material.Material, *PropText, Material.Material->GetClass(), Material.Material->GetOuter(), Material.Material, Warn, 0);
			}
			else
			{
				// Already loaded...
			}
		}
		else
		if (GetEND(&Str, TEXT("OBJECT")))
		{
		}
		else
		{
			FString	strBoolean;
			FLOAT	fValue;

			//BITFIELD				UseNoise : 1;
			if (Parse(Str, TEXT("USENOISE="), strBoolean))
			{
				if (strBoolean == FString("TRUE"))
				{
					Material.UseNoise = TRUE;
				}
				else
				{
					Material.UseNoise = FALSE;
				}
			}
			else
			//FLOAT					NoiseScale;
			if (Parse(Str, TEXT("NOISESCALE="), fValue))
			{
				Material.NoiseScale = fValue;
			}
			else
			//FLOAT					NoisePercent;
			if (Parse(Str, TEXT("NOISEPERCENT="), fValue))
			{
				Material.NoisePercent = fValue;
			}
			else
			//FLOAT					Alpha;
			if (Parse(Str, TEXT("ALPHA="), fValue))
			{
				Material.Alpha = fValue;
			}
			else
			{
				debugf(TEXT("LayerSetupMaterial unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseLayerSetup(ATerrain* Terrain, FTerrainLayer& Layer, UTerrainLayerSetup& Setup, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin TerrainLayerSetup
		Name=BaseTerrainLayerSetup
		Begin LayerSetupMaterials Count=3
			Begin LayerSetupMaterial Index=0
				...
            End LayerSetupMaterial
		End LayerSetupMaterials
	End TerrainLayerSetup
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINLAYERSETUP")))
		{
			// End of terrain layer setup.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("LAYERSETUPMATERIALS")))
		{
			// Parse the count
			INT	Count;
			if (!Parse(Str, TEXT("COUNT="), Count))
			{
				bResult = FALSE;
				break;
			}

			Setup.Materials.InsertZeroed(0, Count);
		}
		else
		if (GetEND(&Str, TEXT("LAYERSETUPMATERIALS")))
		{
		}
		else
		if (GetBEGIN(&Str, TEXT("LAYERSETUPMATERIAL")))
		{
			// Parse the index
			INT Index;
			if (!Parse(Str, TEXT("INDEX="), Index))
			{
				bResult = FALSE;
				break;
			}

			FTerrainFilteredMaterial& Material = Setup.Materials(Index);
			if (!ParseLayerSetupMaterial(Terrain, Setup, Material, Buffer, BufferEnd, Warn))
			{
				debugf(TEXT("Failed to ParseLayerSetupMaterial!"));
				bResult = FALSE;
				break;
			}
		}
		else
		{
			debugf(TEXT("TerrainLayerSetup unhandled string: %s"), Str);
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseLayer(ATerrain* Terrain, FTerrainLayer& Layer, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin TerrainLayer Index=0 Name=Test
		Begin TerrainLayerSetup Name=XXX
			...
		End TerrainLayerSetup
		AlphaMapIndex=-1
		Highlighted=0
		Hidden=0
	End TerrainLayer
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINLAYER")))
		{
			// End of terrain layer.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("TERRAINLAYERSETUP")))
		{
			// Parse the name
			FString	strName;
			Parse(Str, TEXT("NAME="), strName);

			UTerrainLayerSetup* Setup = FindObject<UTerrainLayerSetup>(ANY_PACKAGE, *strName);
			if (!Setup)
			{
				// Try loading the object...
				Setup = LoadObject<UTerrainLayerSetup>(NULL, *strName, NULL, LOAD_None, NULL);
			}

			if (!Setup)
			{
				//@todo. Do we want to do this??
				// Create the instance
				FName	Name(*strName);
				UTerrainLayerSetup* Setup = ConstructObject<UTerrainLayerSetup>(UTerrainLayerSetup::StaticClass(), Terrain, Name);
				check(Setup);

				// Read in the data...
				if (!ParseLayerSetup(Terrain, Layer, *Setup, Buffer, BufferEnd, Warn))
				{
					debugf(TEXT("Failed to ParseLayerSetup!"));
					bResult = FALSE;
				}
			}
			Layer.Setup = Setup;
		}
		else
		if (GetEND(&Str, TEXT("TERRAINLAYERSETUP")))
		{
		}
		else
		{
			INT		iValue;

			if (Parse(Str, TEXT("ALPHAMAPINDEX="), iValue))
			{
				//INT						AlphaMapIndex;
				Layer.AlphaMapIndex = iValue;
			}
			else
			if (Parse(Str, TEXT("HIGHLIGHTED="), iValue))
			{
				//BITFIELD					Highlighted : 1;
				Layer.Highlighted = iValue;
			}
			else
			if (Parse(Str, TEXT("HIDDEN="), iValue))
			{
				//BITFIELD					Hidden : 1;
				Layer.Hidden = iValue;
			}
			else
			{
				debugf(TEXT("Layer unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseLayerData(ATerrain* Terrain, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

	INT LayerIndex = -1;

/***	FORMAT OF TEXT....
    Begin TerrainLayerData
		Count=2
		Begin TerrainLayer Index=0 Name=Test
			...
		End TerrainLayer
		Begin TerrainLayer Index=1 Name=Paintable
			...
		End TerrainLayer
    End TerrainLayerData
***/
	INT CurrLayer = 0;

	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetBEGIN(&Str, TEXT("TERRAINLAYER")))
		{
            //Begin TerrainLayer Index=0 Name=(null)
			// Index
			if (!Parse(Str, TEXT("INDEX="), LayerIndex))
			{
				// ERROR!!!!
				bResult = FALSE;
				break;
			}

			FTerrainLayer& Layer = Terrain->Layers(LayerIndex);
			// Name
			if (!Parse(Str, TEXT("NAME="), Layer.Name))
			{
				// ERROR!!!!
				bResult = FALSE;
				break;
			}

			// Parse the layer
			if (!ParseLayer(Terrain, Layer, Buffer, BufferEnd, Warn))
			{
				debugf(TEXT("Failed to ParseLayer!"));
				bResult = FALSE;
			}
		}
		else
		if (GetEND(&Str, TEXT("TERRAINLAYERDATA")))
		{
			// End of terrain layer data.
			break;
		}
		else
		{
			INT		iValue;

			// Layer count
			if (Parse(Str, TEXT("COUNT="), iValue))
			{
				Terrain->Layers.InsertZeroed(0, iValue);
			}
			else
			{
				debugf(TEXT("LayerData unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseDecorationInstace(ATerrain* Terrain, FTerrainDecorationInstance& Instance, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin TerrainDecorationInstance Index=0
		Begin Object Class=StaticMeshComponent Name=StaticMeshComponent_3004
			StaticMesh=StaticMesh'Neutral_Rocks.Neutral_Rock1_SMesh'
			Name="StaticMeshComponent_3004"
		End Object
		X=10.590208
		Y=11.588464
		Scale=6.124574
		Yaw=15483
    End TerrainDecorationInstance
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINDECORATIONINSTANCE")))
		{
			// End of decoration instance.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("OBJECT")))
		{
			// Parse the class and name
			FString strClass, strName;

			if (!Parse(Str, TEXT("CLASS="), strClass))
			{
			}
			if (!Parse(Str, TEXT("NAME="), strName))
			{
			}

			FName	Name(*strName);

			Instance.Component = ConstructObject<UStaticMeshComponent>(UStaticMeshComponent::StaticClass(), Terrain, Name);
			check(Instance.Component);

			// Get property text.
			FString PropText;
			UnEdFact_GetPropertyText(Buffer, TEXT("OBJECT"), PropText);
			
			Instance.Component->PreEditChange(NULL);
			ImportObjectProperties((BYTE*)Instance.Component, *PropText, 
				Instance.Component->GetClass(),
				Instance.Component->GetOuter(), Instance.Component,
				Warn, 0);
		}
		else
		if (GetEND(&Str, TEXT("OBJECT")))
		{
		}
		else
		{
			FLOAT	fValue;
			INT		iValue;

			if (Parse(Str, TEXT("X="), fValue))
			{
				Instance.X = fValue;
			}
			else
			if (Parse(Str, TEXT("Y="), fValue))
			{
				Instance.Y = fValue;
			}
			else
			if (Parse(Str, TEXT("SCALE="), fValue))
			{
				Instance.Scale = fValue;
			}
			else
			if (Parse(Str, TEXT("YAW="), iValue))
			{
				Instance.Yaw = iValue;
			}
			else
			{
				debugf(TEXT("TerrainDecorationInstance unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseDecoration(ATerrain* Terrain, FTerrainDecoration& Decoration, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin TerrainDecoration Index=0
		Begin Object Class=StaticMeshComponentFactory Name=StaticMeshComponentFactory_0
			StaticMesh=StaticMesh'Neutral_Rocks.Neutral_Rock1_SMesh'
			Name="StaticMeshComponentFactory_0"
		End Object
		MinScale=5.000000
		MaxScale=10.000000
		Density=1.000000
		SlopeRotationBlend=0.000000
		RandSeed=0
		Begin TerrainDecorationInstanceArray Count=31
			Begin TerrainDecorationInstance Index=0
				...
			End TerrainDecorationInstance
			...
			Begin TerrainDecorationInstance Index=30
				...
			End TerrainDecorationInstance
		End TerrainDecorationInstanceArray
	End TerrainDecoration
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINDECORATION")))
		{
			// End of decoration.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("Object")))
		{
			//Begin Object Class=StaticMeshComponentFactory Name=StaticMeshComponentFactory_0
			//	StaticMesh=StaticMesh'Neutral_Rocks.Neutral_Rock1_SMesh'
			//	Name="StaticMeshComponentFactory_0"
			//End Object

			// Parse the class and name
			FString strClass, strName;

			if (!Parse(Str, TEXT("CLASS="), strClass))
			{
			}
			if (!Parse(Str, TEXT("NAME="), strName))
			{
			}

			FName	Name(*strName);

			Decoration.Factory = ConstructObject<UStaticMeshComponentFactory>(UStaticMeshComponentFactory::StaticClass(), Terrain, Name);
			check(Decoration.Factory);

			// Get property text.
			FString PropText;
			UnEdFact_GetPropertyText(Buffer, TEXT("OBJECT"), PropText);
			
			Decoration.Factory->PreEditChange(NULL);
			ImportObjectProperties((BYTE*)Decoration.Factory, *PropText, Decoration.Factory->GetClass(), Decoration.Factory->GetOuter(), Decoration.Factory, Warn, 0);
		}
		else
		if (GetEND(&Str, TEXT("OBJECT")))
		{
		}
		else
		if (GetBEGIN(&Str, TEXT("TERRAINDECORATIONINSTANCEARRAY")))
		{
			INT	Count;

			if (!Parse(Str, TEXT("COUNT="), Count))
			{
				bResult = FALSE;
				break;
			}

			Decoration.Instances.InsertZeroed(0, Count);
		}
		else
		if (GetEND(&Str, TEXT("TERRAINDECORATIONINSTANCEARRAY")))
		{
		}
		else
		if (GetBEGIN(&Str, TEXT("TERRAINDECORATIONINSTANCE")))
		{
			// Parse the index
			INT Index;
			if (!Parse(Str, TEXT("INDEX="), Index))
			{
				bResult = FALSE;
				break;
			}

			FTerrainDecorationInstance& Instance = Decoration.Instances(Index);
			if (!ParseDecorationInstace(Terrain, Instance, Buffer, BufferEnd, Warn))
			{
				bResult = FALSE;
				break;
			}
		}
		else
		{
/***	FORMAT OF TEXT....
			//MinScale=#.#
			//MaxScale=#.#
			//Density=#.#
			//SlopeRotationBlend=#.#
			//RandSeed=#
***/
			FLOAT	fValue;
			INT		iValue;

			if (Parse(Str, TEXT("MINSCALE="), fValue))
			{
				Decoration.MinScale = fValue;
			}
			else
			if (Parse(Str, TEXT("MAXSCALE="), fValue))
			{
				Decoration.MaxScale = fValue;
			}
			else
			if (Parse(Str, TEXT("DENSITY="), fValue))
			{
				Decoration.Density = fValue;
			}
			else
			if (Parse(Str, TEXT("SLOPEROTATIONBLEND="), fValue))
			{
				Decoration.SlopeRotationBlend = fValue;
			}
			else
			if (Parse(Str, TEXT("RANDSEED="), iValue))
			{
				Decoration.RandSeed = iValue;
			}
			else
			{
				debugf(TEXT("Decoration unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseDecoLayer(ATerrain* Terrain, FTerrainDecoLayer& DecoLayer, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
    Begin TerrainDecoLayer Index=0 Name=Rocks
        DecorationCount=1
        Begin TerrainDecoration Index=0
			...
        End TerrainDecoration
        AlphaMapIndex=1
    End TerrainDecoLayer
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINDECOLAYER")))
		{
			// End of terrain deco layer.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("TERRAINDECORATION")))
		{
			// Parse the index
			INT Index;
			if (!Parse(Str, TEXT("INDEX="), Index))
			{
				bResult = FALSE;
				break;
			}

			FTerrainDecoration& Decoration = DecoLayer.Decorations(Index);

			if (!ParseDecoration(Terrain, Decoration, Buffer, BufferEnd, Warn))
			{
				bResult = FALSE;
			}
		}
		else
		{
			INT	iValue;
			if (Parse(Str, TEXT("DECORATIONCOUNT="), iValue))
			{
				DecoLayer.Decorations.InsertZeroed(0, iValue);
			}
			else
			if (Parse(Str, TEXT("ALPHAMAPINDEX="), iValue))
			{
				DecoLayer.AlphaMapIndex = iValue;
			}
			else
			{
				debugf(TEXT("DecoLayer unhandled string: %s"), Str);
				bResult = FALSE;
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseDecoLayerData(ATerrain* Terrain, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
	Begin TerrainDecoLayerData
		Count=2
		Begin TerrainDecoLayer Index=0 Name=Rocks
			...
		End TerrainDecoLayer
		Begin TerrainDecoLayer Index=1 Name=Trees
			...
		End TerrainDecoLayer
    End TerrainDecoLayerData
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINDECOLAYERDATA")))
		{
			// End of terrain deco layer data.
			break;
		}
		else
		if (GetBEGIN(&Str, TEXT("TERRAINDECOLAYER")))
		{
			// Parse the index
			INT Index;
			if (!Parse(Str, TEXT("INDEX="), Index))
			{
				bResult = FALSE;
				break;
			}

			FTerrainDecoLayer& DecoLayer = Terrain->DecoLayers(Index);

			// Parse the name
			if (!Parse(Str, TEXT("NAME="), DecoLayer.Name))
			{
				bResult = FALSE;
				break;
			}

			if (!ParseDecoLayer(Terrain, DecoLayer, Buffer, BufferEnd, Warn))
			{
				bResult = FALSE;
			}
		}
		else
		{
			INT	iValue;
			if (Parse(Str, TEXT("COUNT="), iValue))
			{
				Terrain->DecoLayers.InsertZeroed(0, iValue);
			}
			else
			{
				debugf(TEXT("DecoLayerData unhandled string: %s"), Str);
				bResult = FALSE;
			}
		}
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseAlphaMap(FAlphaMap& AlphaMap, INT Count, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

	FString StrLine;

	for (INT ii = 0; ii < Count; ii += 8)
	{
		if (!ParseLine(&Buffer,StrLine))
		{
			// Error??
			break;
		}

		const TCHAR* Str = *StrLine;

		// Skip white-space to the first number
		UnEdFact_SkipWhitespace(Str);

		INT iCheckCount = 8;
		if (ii + 8 >= Count)
		{
			iCheckCount = Count - ii;
		}
		// Parse values
		for (INT jj = 0; jj < iCheckCount; jj++)
		{
			INT Value;
			appSSCANF(Str, TEXT("%d\t"), &Value);

			AlphaMap.Data(ii + jj) = (BYTE)Value;

			// Advanced past the number...
			UnEdFact_SkipNumeric(Str);
			// And the whitespace...
			UnEdFact_SkipWhitespace(Str);
		}

		Warn->StatusUpdatef(ii + iCheckCount, Count, TEXT("Importing AlphaMap (%d/%d)"), ii+iCheckCount, Count);
	}

	return bResult;
}

UBOOL UTerrainFactory::ParseAlphaMapData(ATerrain* Terrain, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

/***	FORMAT OF TEXT....
    Begin TerrainAlphaMapData
		Count=3
		Begin AlphaMap Index=0 Count=289
			...
	    End AlphaMap
		Begin AlphaMap Index=1 Count=289
			...
	    End AlphaMap
		Begin AlphaMap Index=2 Count=289
			...
	    End AlphaMap
    End TerrainAlphaMapData
***/
	FString StrLine;
	while (ParseLine(&Buffer,StrLine))
	{
		const TCHAR* Str = *StrLine;

		if (GetEND(&Str, TEXT("TERRAINALPHAMAPDATA")))
		{
			// End of terrain alpha map data.
			break;
		}
		else
		if (GetEND(&Str, TEXT("ALPHAMAP")))
		{
		}
		else
		if (GetBEGIN(&Str, TEXT("ALPHAMAP")))
		{
			// Parse the index and count
			INT Index;
			INT Count;

			if (!Parse(Str, TEXT("INDEX="), Index))
			{
				bResult = FALSE;
				break;
			}

			if (!Parse(Str, TEXT("COUNT="), Count))
			{
				bResult = FALSE;
				break;
			}

			FAlphaMap& AlphaMap = Terrain->AlphaMaps(Index);
			AlphaMap.Data.InsertZeroed(0, Count);

			if (!ParseAlphaMap(AlphaMap, Count, Buffer, BufferEnd, Warn))
			{
				bResult = FALSE;
				break;
			}
		}
		else
		{
			INT Count;
			if (Parse(Str, TEXT("COUNT="), Count))
			{
				Terrain->AlphaMaps.InsertZeroed(0, Count);
			}
			else
			{
				debugf(TEXT("AlphaMapData unhandled string: %s"), Str);
			}
		}
	}

	return bResult;
}

UTerrainHeightMapFactory* UTerrainFactory::GetHeightMapImporter(const TCHAR* FactoryName)
{
	UTerrainHeightMapFactory* Factory = NULL;

	TArray<UTerrainHeightMapFactory*> Factories;
	for (TObjectIterator<UClass> It; It ; ++It)
	{
		if (It->IsChildOf(UTerrainHeightMapFactory::StaticClass()) && !(It->ClassFlags & CLASS_Abstract))
		{
			if (appStrcmp(*It->GetName(), FactoryName) == 0)
			{
				Factory = ConstructObject<UTerrainHeightMapFactory>(*It);
				return Factory;
			}
		}
	}

	return NULL;
}

IMPLEMENT_CLASS(UTerrainFactory);

/*------------------------------------------------------------------------------
	UTerrainHeightMapFactory implementation.
------------------------------------------------------------------------------*/
UTerrainHeightMapFactory::UTerrainHeightMapFactory()
{
}

void UTerrainHeightMapFactory::StaticConstructor()
{
}

UBOOL UTerrainHeightMapFactory::ImportHeightDataFromText(ATerrain* Terrain, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	return FALSE;
}

UBOOL UTerrainHeightMapFactory::ImportHeightDataFromBinary(ATerrain* Terrain, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	return FALSE;
}

IMPLEMENT_CLASS(UTerrainHeightMapFactory);

/*------------------------------------------------------------------------------
	UTerrainHeightMapFactoryTextT3D implementation.
------------------------------------------------------------------------------*/
UTerrainHeightMapFactoryTextT3D::UTerrainHeightMapFactoryTextT3D()
{
}

void UTerrainHeightMapFactoryTextT3D::StaticConstructor()
{
}

UBOOL UTerrainHeightMapFactoryTextT3D::ImportHeightDataFromText(ATerrain* Terrain, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	INT iCount, iWidth, iHeight;

	FString StrLine;
	if (!ParseLine(&Buffer,StrLine))
	{
		// Error??
		return FALSE;
	}

	const TCHAR* Str = *StrLine;
	Parse(Str, TEXT("COUNT="),	iCount);
	Parse(Str, TEXT("WIDTH="),	iWidth);
	Parse(Str, TEXT("HEIGHT="),	iHeight);

	Terrain->Heights.Empty(iCount);
	for (INT ii = 0; ii < iCount; ii += 8)
	{
		if (!ParseLine(&Buffer,StrLine))
		{
			// Error??
			break;
		}

		Str = *StrLine;
		// Skip white-space to the first number
		UnEdFact_SkipWhitespace(Str);

		INT iCheckCount = 8;
		if (ii + 8 >= iCount)
		{
			iCheckCount = iCount - ii;
		}
		// Parse values
		for (INT jj = 0; jj < iCheckCount; jj++)
		{
			INT Height;
			appSSCANF(Str, TEXT("%d\t"), &Height);

			Terrain->Heights.AddItem(Height);

			// Advanced past the number...
			UnEdFact_SkipNumeric(Str);
			// And the whitespace...
			UnEdFact_SkipWhitespace(Str);
		}

		Warn->StatusUpdatef(ii + iCheckCount, iCount, TEXT("Importing TerrainHeight (%d/%d)"), ii+iCheckCount, iCount);
	}

	Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainHeight Completed"));

	return TRUE;
}

IMPLEMENT_CLASS(UTerrainHeightMapFactoryTextT3D);


/*------------------------------------------------------------------------------
	UTerrainHeightMapFactoryG16BMP implementation.
------------------------------------------------------------------------------*/
UTerrainHeightMapFactoryG16BMP::UTerrainHeightMapFactoryG16BMP()
{
}

void UTerrainHeightMapFactoryG16BMP::StaticConstructor()
{
}

UBOOL UTerrainHeightMapFactoryG16BMP::ImportHeightDataFromFile(ATerrain* Terrain, const TCHAR* FileName, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	TArray<BYTE> BMPFileData;
	if (appLoadFileToArray(BMPFileData, FileName) == FALSE)
	{
		GWarn->StatusUpdatef( 0, 0, TEXT("Failed opening height map %s"), FileName);
		return FALSE;
	}

	BMPFileData.AddItem(0);
	const BYTE* Buffer = &BMPFileData(0);

	INT Length = (Buffer + BMPFileData.Num() - 1) - Buffer;
	const BYTE* BufferEnd = Buffer + Length;
	if (ImportHeightDataFromBinary(Terrain, Buffer, BufferEnd, GWarn, bGenerateTerrainFromHeightMap) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

UBOOL UTerrainHeightMapFactoryG16BMP::ImportHeightDataFromBinary(ATerrain* Terrain, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	UBOOL bResult = TRUE;

	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(Buffer + 0);
	const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));

	// Validate it.
	INT Length = BufferEnd - Buffer;

	if ((Length >= (sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader))) && 
		(Buffer[0]=='B') && (Buffer[1]=='M'))
	{
		// We don't need to worry about powers of 2 for terrain height maps...
		if (bmhdr->biCompression != BCBI_RGB)
		{
			Warn->Logf(TEXT("G16BMP Height Map: RLE compression of BMP images not supported"));
			return FALSE;
		}

		if ((bmhdr->biPlanes == 1) && (bmhdr->biBitCount == 8))
		{
			//@todo. Add support for palettized height map?
			// Probably not...
		}
		else
		if ((bmhdr->biPlanes == 1) && (bmhdr->biBitCount == 24))
		{
			//@todo. Support converting 24-bit images to height map?
			// Probably not...
		}
		else
		if ((bmhdr->biPlanes == 1) && (bmhdr->biBitCount == 16))
		{
			if (bGenerateTerrainFromHeightMap)
			{
				// Setup the terrain
				Terrain->ClearComponents();

				Terrain->NumVerticesX = bmhdr->biWidth;
				Terrain->NumVerticesY = bmhdr->biHeight;

				Terrain->NumPatchesX = Terrain->NumVerticesX - 1;
				Terrain->NumPatchesY = Terrain->NumVerticesY - 1;

				// 
				INT TotalHeightValues = bmhdr->biWidth * bmhdr->biHeight;
				Terrain->Heights.Empty(TotalHeightValues);
				// Copy upside-down scanlines.
				const BYTE* Data = (BYTE*)Buffer + bmf->bfOffBits;

				INT TrueWidth = bmhdr->biWidth;
				if (bmhdr->biSizeImage != ((bmhdr->biHeight * bmhdr->biWidth) * 2))
				{
					if ((bmhdr->biWidth % 2) != 0)
					{
						TrueWidth += 1;
					}
				}
				for (INT y = bmhdr->biHeight - 1; y >= 0; y--)
				{
					WORD* SrcPtr = (WORD*)&Data[y * TrueWidth * 2];
					for (INT x = 0; x < (INT)bmhdr->biWidth; x++)
					{
						Terrain->Heights.AddItem(*SrcPtr++);
					}
				}

				// Set the InfoData array count as well...
				// NOTE: This will kill any previously set holes on the terrain...
				if (TotalHeightValues != Terrain->InfoData.Num())
				{
					Terrain->InfoData.Empty(TotalHeightValues);
					Terrain->InfoData.AddZeroed(TotalHeightValues);
				}
				Terrain->Allocate();
			}
			else
			{
				// We need to make sure that it matches the terrain
				if ((Terrain->NumVerticesX != bmhdr->biWidth) ||
					(Terrain->NumVerticesY != bmhdr->biHeight))
				{
					Warn->Logf(TEXT("G16BMP Height Map: Size mis-match (%dx%d vs. %dx%d)!"),
						bmhdr->biWidth, bmhdr->biHeight, 
						Terrain->NumVerticesX, Terrain->NumVerticesY);
					return FALSE;
				}
				
				INT TotalHeightValues = bmhdr->biWidth * bmhdr->biHeight;
				Terrain->Heights.Empty(TotalHeightValues);
				// Copy upside-down scanlines.
				const BYTE* Data = (BYTE*)Buffer + bmf->bfOffBits;

				INT TrueWidth = bmhdr->biWidth;
				if (bmhdr->biSizeImage != ((bmhdr->biHeight * bmhdr->biWidth) * 2))
				{
					if ((bmhdr->biWidth % 2) != 0)
					{
						TrueWidth += 1;
					}
				}
				for (INT y = bmhdr->biHeight - 1; y >= 0; y--)
				{
					WORD* SrcPtr = (WORD*)&Data[y * TrueWidth * 2];
					for (INT x = 0; x < (INT)bmhdr->biWidth; x++)
					{
						Terrain->Heights.AddItem(*SrcPtr++);
					}
				}

				// Set the InfoData array count as well...
				// NOTE: This will kill any previously set holes on the terrain...
				if (TotalHeightValues != Terrain->InfoData.Num())
				{
					Terrain->InfoData.Empty(TotalHeightValues);
					Terrain->InfoData.AddZeroed(TotalHeightValues);
				}
			}
		}
		else
		{
			Warn->Logf(TEXT("G16BMP Height Map: Uses an unsupported format (%i/%i)"), 
				bmhdr->biPlanes, bmhdr->biBitCount);
			return FALSE;
		}
	}

	Warn->StatusUpdatef( 0, 0, TEXT("Terrain Import HeightMap Complete"));
	if (bGenerateTerrainFromHeightMap)
	{
		Terrain->PostEditChange();
	}

	return bResult;
}

UBOOL UTerrainHeightMapFactoryG16BMP::ImportLayerDataFromFile(ATerrain* Terrain, FTerrainLayer* Layer, const TCHAR* FileName, FFeedbackContext* Warn)
{
	TArray<BYTE> BMPFileData;
	if (appLoadFileToArray(BMPFileData, FileName) == FALSE)
	{
		GWarn->StatusUpdatef( 0, 0, TEXT("Failed opening layer map %s"), FileName);
		return FALSE;
	}

	BMPFileData.AddItem(0);
	const BYTE* Buffer = &BMPFileData(0);

	INT Length = (Buffer + BMPFileData.Num() - 1) - Buffer;
	const BYTE* BufferEnd = Buffer + Length;
	if (ImportLayerDataFromBinary(Terrain, Layer, Buffer, BufferEnd, GWarn) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

UBOOL UTerrainHeightMapFactoryG16BMP::ImportLayerDataFromBinary(ATerrain* Terrain, FTerrainLayer* Layer, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn)
{
	UBOOL bResult = TRUE;

	const FBitmapFileHeader* bmf   = (FBitmapFileHeader *)(Buffer + 0);
	const FBitmapInfoHeader* bmhdr = (FBitmapInfoHeader *)(Buffer + sizeof(FBitmapFileHeader));

	// Validate it.
	INT Length = BufferEnd - Buffer;

	if ((Length >= (sizeof(FBitmapFileHeader) + sizeof(FBitmapInfoHeader))) && 
		(Buffer[0]=='B') && (Buffer[1]=='M'))
	{
		// We don't need to worry about powers of 2 for terrain height maps...
		if (bmhdr->biCompression != BCBI_RGB)
		{
			Warn->Logf(TEXT("G16BMP LayerAlphaMap: RLE compression of BMP images not supported"));
			return FALSE;
		}

		if ((bmhdr->biPlanes == 1) && (bmhdr->biBitCount == 8))
		{
			//@todo. Add support for palettized height map?
			// Probably not...
		}
		else
		if ((bmhdr->biPlanes == 1) && (bmhdr->biBitCount == 24))
		{
			//@todo. Support converting 24-bit images to height map?
			// Probably not...
		}
		else
		if ((bmhdr->biPlanes == 1) && (bmhdr->biBitCount == 16))
		{
			// We need to make sure that it matches the terrain
			if ((Terrain->NumVerticesX != bmhdr->biWidth) ||
				(Terrain->NumVerticesY != bmhdr->biHeight))
			{
				Warn->Logf(TEXT("G16BMP LayerAlphaMap: Size mis-match (%dx%d vs. %dx%d)!"),
					bmhdr->biWidth, bmhdr->biHeight, 
					Terrain->NumVerticesX, Terrain->NumVerticesY);
				return FALSE;
			}

			FAlphaMap& AlphaMap = Terrain->AlphaMaps(Layer->AlphaMapIndex);

			// 
			INT TotalAlphaValues = bmhdr->biWidth * bmhdr->biHeight;
			AlphaMap.Data.Empty(TotalAlphaValues);
			// Copy upside-down scanlines.
			const BYTE* Data = (BYTE*)Buffer + bmf->bfOffBits;
			for (INT y = bmhdr->biHeight - 1; y >= 0; y--)
			{
				WORD* SrcPtr = (WORD*)&Data[y * bmhdr->biWidth * 2];
				for (INT x = 0; x < (INT)bmhdr->biWidth; x++)
				{
					WORD InValue = *SrcPtr++;
					BYTE Value = (BYTE)((FLOAT)InValue * (255.0f / 65535.0f));
					AlphaMap.Data.AddItem(Value);
				}
			}
		}
		else
		{
			Warn->Logf(TEXT("G16BMP LayerAlphaMap: Uses an unsupported format (%i/%i)"), 
				bmhdr->biPlanes, bmhdr->biBitCount);
			return FALSE;
		}
	}

	Warn->StatusUpdatef( 0, 0, TEXT("Terrain Import LayerAlphaMap Complete"));
	Terrain->PostEditChange();
	Terrain->MarkPackageDirty();

	return bResult;
}

IMPLEMENT_CLASS(UTerrainHeightMapFactoryG16BMP);

/*------------------------------------------------------------------------------
	UTerrainHeightMapFactoryG16BMPT3D implementation.
------------------------------------------------------------------------------*/
UTerrainHeightMapFactoryG16BMPT3D::UTerrainHeightMapFactoryG16BMPT3D()
{
}

void UTerrainHeightMapFactoryG16BMPT3D::StaticConstructor()
{
}

UBOOL UTerrainHeightMapFactoryG16BMPT3D::ImportHeightDataFromText(ATerrain* Terrain, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	INT iCount;

	FString StrLine;
	if (!ParseLine(&Buffer,StrLine))
	{
		// Error??
		return FALSE;
	}

	const TCHAR* Str = *StrLine;
	Parse(Str, TEXT("COUNT="),	iCount);

	if (Terrain->Heights.Num() != iCount)
	{
		Terrain->Heights.Empty(iCount);
		Terrain->Heights.InsertZeroed(0, iCount);
	}

	// Next line
	if (!ParseLine(&Buffer, StrLine))
	{
		return FALSE;
	}

	Str = *StrLine;
	FString strFile;

	if (!Parse(Str, TEXT("FILENAME="), strFile))
	{
		return FALSE;
	}

	Warn->StatusUpdatef( 0, 0, TEXT("Importing TerrainHeight from file %s"), *strFile);

	// Grab the 'current' filename
	FFilename CurrFilename(UFactory::CurrentFilename);
	FFilename FullPath(CurrFilename.GetPath());

	FullPath += TEXT("\\");
	FullPath += strFile;

	if (ImportHeightDataFromFile(Terrain, *FullPath, Warn, bGenerateTerrainFromHeightMap) == FALSE)
	{
		return FALSE;
	}

	return TRUE;
}

UBOOL UTerrainHeightMapFactoryG16BMPT3D::ImportHeightDataFromBinary(ATerrain* Terrain, const BYTE*& Buffer, const BYTE* BufferEnd, FFeedbackContext* Warn, UBOOL bGenerateTerrainFromHeightMap)
{
	return UTerrainHeightMapFactoryG16BMP::ImportHeightDataFromBinary(Terrain, Buffer, BufferEnd, Warn, bGenerateTerrainFromHeightMap);
}

IMPLEMENT_CLASS(UTerrainHeightMapFactoryG16BMPT3D);

/*------------------------------------------------------------------------------
	UTerrainLayerSetupFactory implementation.
------------------------------------------------------------------------------*/
UTerrainLayerSetupFactory::UTerrainLayerSetupFactory()
{
}

void UTerrainLayerSetupFactory::StaticConstructor()
{
}

UObject* UTerrainLayerSetupFactory::FactoryCreateText(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const TCHAR*& Buffer, const TCHAR* BufferEnd, FFeedbackContext* Warn)
{
	return NULL;
}

IMPLEMENT_CLASS(UTerrainLayerSetupFactory);

