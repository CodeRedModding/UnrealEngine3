/*=============================================================================
	Exporter.cpp: Lightmass solver exporter class.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/
#include "stdafx.h"
#include "LightmassSwarm.h"
#include "Exporter.h"
#include "LightingSystem.h"


namespace Lightmass
{

	FLightmassSolverExporter::FLightmassSolverExporter( FLightmassSwarm* InSwarm, const FScene& InScene, UBOOL bInDumpTextures )
	:	Swarm( InSwarm )
	,	Scene( InScene )
	,	bDumpTextures( bInDumpTextures )
	{
	}

	FLightmassSolverExporter::~FLightmassSolverExporter()
	{
	}

	FLightmassSwarm* FLightmassSolverExporter::GetSwarm()
	{
		return Swarm;
	}

	INT FLightmassSolverExporter::BeginExportResults(struct FVertexMappingStaticLightingData& LightingData, UINT NumMappings) const
	{
		const FString ChannelName = CreateChannelName(LightingData.Mapping->Guid, LM_VERTEXMAPPING_VERSION, LM_VERTEXMAPPING_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_VERTEXMAPPING_CHANNEL_FLAGS, TRUE);
		if( ErrorCode < 0 )
		{
			debugf(TEXT("Failed to open vertex mapping channel %s!"), *ChannelName);
		}
		else
		{
			// Write out the number of mappings this channel will contain
			Swarm->Write(&NumMappings, sizeof(NumMappings));
		}
		return ErrorCode;
	}

	void FLightmassSolverExporter::EndExportResults() const
	{
		Swarm->CloseCurrentChannel();
	}

	/**
	 * Send complete lighting data to UE3
	 *
	 * @param LightingData - Object containing the computed data
	 */
	void FLightmassSolverExporter::ExportResults( FVertexMappingStaticLightingData& LightingData, UBOOL bUseUniqueChannel ) const
	{
		debugfSlow(TEXT("Exporting vertex lighting %s [%.3fs]"), *(LightingData.Mapping->Guid.String()), LightingData.ExecutionTime);

		// If requested, use a unique channel for this mapping, otherwise, just use the one that is already open
		if (bUseUniqueChannel)
		{
			if (BeginExportResults(LightingData, 1) < 0)
			{
				return;
			}
		}

		// Quantize the data if it hasn't been yet
		LightingData.LightMapData->Quantize(
			INDEX_NONE,
			Scene.SceneConstants.bUseFixedScaleForSimpleLightmaps,
			Scene.SceneConstants.UseFixedScaleValue
		);

#if LM_COMPRESS_OUTPUT_DATA && !LM_COMPRESS_ON_EACH_THREAD
		// If we need to compress the data before writing out, do it now
		LightingData.LightMapData->Compress(
			INDEX_NONE,
			Scene.SceneConstants.bUseFixedScaleForSimpleLightmaps,
			Scene.SceneConstants.UseFixedScaleValue
		);
#endif
		const INT ShadowMapCount = LightingData.ShadowMaps.Num();
		const INT NumLights = LightingData.LightMapData->Lights.Num();

#pragma pack (push,1)
		struct FTextureHeader
		{
			FTextureHeader(FGuid& InGuid, DOUBLE InExecutionTime, FLightMapData1DData& InData, INT InShadowMapCount, INT InLightCount)
				: Guid(InGuid), ExecutionTime(InExecutionTime), Data(InData), ShadowMapCount(InShadowMapCount), LightCount(InLightCount)
			{}
			FGuid Guid;
			DOUBLE ExecutionTime;
			FLightMapData1DData Data;
			INT ShadowMapCount;
			INT LightCount;
		};
#pragma pack (pop)
		FTextureHeader Header(LightingData.Mapping->Guid, LightingData.ExecutionTime, *(FLightMapData1DData*)LightingData.LightMapData, LightingData.ShadowMaps.Num(), NumLights);
		Swarm->Write(&Header, sizeof(Header));

		for (INT LightIndex = 0; LightIndex < NumLights; LightIndex++)
		{
			FGuid CurrentGuid = LightingData.LightMapData->Lights(LightIndex)->Guid;
			Swarm->Write(&CurrentGuid, sizeof(CurrentGuid));
		}

#if LM_COMPRESS_OUTPUT_DATA
		// write out compressed data if supported
		Swarm->Write(LightingData.LightMapData->GetCompressedData(), LightingData.LightMapData->CompressedDataSize ? LightingData.LightMapData->CompressedDataSize : LightingData.LightMapData->UncompressedDataSize);
#else
		// write out uncompressed raw data
		const FLightSampleData* SampleData = LightingData.LightMapData->GetQuantizedData();
		Swarm->Write((void*)SampleData, sizeof(FLightSampleData) * LightingData.LightMapData->GetSize());
#endif

		Swarm->Write(&LightingData.PreviewEnvironmentShadowing, sizeof(LightingData.PreviewEnvironmentShadowing));

		// The resulting light GUID --> shadow map data
		for (TMap<FLight*,FShadowMapData1D*>::TIterator It(LightingData.ShadowMaps); It; ++It)
		{
			FGuid OutGuid = It.Key()->Guid;
			FShadowMapData1D* OutData = It.Value();

			Swarm->Write(&OutGuid, sizeof(FGuid));
			Swarm->Write((FShadowMapData1DData*)OutData, sizeof(FShadowMapData1DData));

			// Write out unquantized, uncompressed raw data
			const FLOAT* ShadowSampleData = OutData->GetData();
			Swarm->Write((void*)ShadowSampleData, sizeof(FLOAT) * OutData->GetSize());
		}

		// free up the calculated data
		delete LightingData.LightMapData;
		LightingData.ShadowMaps.Empty();

		// Only close the channel if we opened it
		if (bUseUniqueChannel)
		{
			EndExportResults();
		}
	}

	/** Exports volume lighting samples to UE3. */
	void FLightmassSolverExporter::ExportVolumeLightingSamples(
		UBOOL bExportVolumeLightingDebugOutput,
		const FVolumeLightingDebugOutput& DebugOutput,
		const FVector4& VolumeCenter, 
		const FVector4& VolumeExtent, 
		const TMap<INT,TArray<FVolumeLightingSample> >& VolumeSamples) const
	{
		if (bExportVolumeLightingDebugOutput)
		{
			const FString ChannelName = CreateChannelName(VolumeLightingDebugOutputGuid, LM_VOLUMEDEBUGOUTPUT_VERSION, LM_VOLUMEDEBUGOUTPUT_EXTENSION);
			const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_VOLUMEDEBUGOUTPUT_CHANNEL_FLAGS, TRUE);
			if( ErrorCode >= 0 )
			{
				WriteArray(DebugOutput.VolumeLightingSamples);
				Swarm->CloseCurrentChannel();
			}
			else
			{
				debugf(TEXT("Failed to open volume sample debug output channel!"));
			}
		}

		const FString ChannelName = CreateChannelName(PrecomputedVolumeLightingGuid, LM_VOLUMESAMPLES_VERSION, LM_VOLUMESAMPLES_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_VOLUMESAMPLES_CHANNEL_FLAGS, TRUE);
		if( ErrorCode >= 0 )
		{
			Swarm->Write(&VolumeCenter, sizeof(VolumeCenter));
			Swarm->Write(&VolumeExtent, sizeof(VolumeExtent));
			const INT NumVolumeSampleArrays = VolumeSamples.Num();
			Swarm->Write(&NumVolumeSampleArrays, sizeof(NumVolumeSampleArrays));
			for (TMap<INT,TArray<FVolumeLightingSample> >::TConstIterator It(VolumeSamples); It; ++It)
			{
				Swarm->Write(&It.Key(), sizeof(It.Key()));
				checkAtCompileTime(sizeof(FVolumeLightingSample) == sizeof(FVolumeLightingSampleData), VolumeDerivedSizeMustMatch);
				WriteArray(It.Value());
			}
			Swarm->CloseCurrentChannel();
		}
		else
		{
			debugf(TEXT("Failed to open volume samples channel!"));
		}
	}

	/** Exports dominant shadow information to UE3. */
	void FLightmassSolverExporter::ExportDominantShadowInfo(const FGuid& LightGuid, const FDominantLightShadowInfo& DominantLightShadowInfo) const
	{
		const FString ChannelName = CreateChannelName(LightGuid, LM_DOMINANTSHADOW_VERSION, LM_DOMINANTSHADOW_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_DOMINANTSHADOW_CHANNEL_FLAGS, TRUE);
		if( ErrorCode >= 0 )
		{
			Swarm->Write(&DominantLightShadowInfo, sizeof(FDominantLightShadowInfoData));
			checkAtCompileTime(sizeof(FDominantLightShadowSample) == sizeof(FDominantLightShadowSampleData), ShadowDerivedSizeMustMatch);
			WriteArray(DominantLightShadowInfo.ShadowMap);
			Swarm->CloseCurrentChannel();
		}
		else
		{
			debugf(TEXT("Failed to open dominant shadow channel!"));
		}
	}

	/** 
	 * Exports information about mesh area lights back to UE3, 
	 * So that UE3 can create dynamic lights to approximate the mesh area light's influence on dynamic objects.
	 */
	void FLightmassSolverExporter::ExportMeshAreaLightData(const TIndirectArray<FMeshAreaLight>& MeshAreaLights, FLOAT MeshAreaLightGeneratedDynamicLightSurfaceOffset) const
	{
		const FString ChannelName = CreateChannelName(MeshAreaLightDataGuid, LM_MESHAREALIGHTDATA_VERSION, LM_MESHAREALIGHTDATA_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_MESHAREALIGHT_CHANNEL_FLAGS, TRUE);
		if( ErrorCode >= 0 )
		{
			INT NumMeshAreaLights = MeshAreaLights.Num();
			Swarm->Write(&NumMeshAreaLights, sizeof(NumMeshAreaLights));

			for (INT LightIndex = 0; LightIndex < MeshAreaLights.Num(); LightIndex++)
			{
				const FMeshAreaLight& CurrentLight = MeshAreaLights(LightIndex);
				FMeshAreaLightData LightData;
				LightData.LevelId = CurrentLight.LevelId;

				FVector4 AverageNormal(0,0,0);
				for (INT PrimitiveIndex = 0; PrimitiveIndex < CurrentLight.Primitives.Num(); PrimitiveIndex++)
				{
					AverageNormal += CurrentLight.Primitives(PrimitiveIndex).SurfaceNormal * CurrentLight.Primitives(PrimitiveIndex).SurfaceArea;
				}
				if (AverageNormal.SizeSquared() > KINDA_SMALL_NUMBER)
				{
					AverageNormal = AverageNormal.UnsafeNormal();
				}
				else
				{
					AverageNormal = FVector4(1,0,0);
				}
				// Offset the position somewhat to reduce the chance of the generated light being inside the mesh
				LightData.Position = CurrentLight.Position + AverageNormal *MeshAreaLightGeneratedDynamicLightSurfaceOffset;
				// Use the average normal for the generated light's direction
				LightData.Direction = AverageNormal;
				LightData.Radius = CurrentLight.InfluenceRadius;
				// Approximate the mesh area light's cosine lobe falloff using a UE3 spotlight's cone angle falloff
				LightData.ConeAngle = PI / 2.0f;
				FLinearColor LightIntensity = CurrentLight.TotalPower / CurrentLight.TotalSurfaceArea;
				// Extract an LDR light color and brightness scale
				FLOAT MaxComponent = Max(LightIntensity.R, Max(LightIntensity.G, LightIntensity.B));
				LightData.Color = LightIntensity / Max(MaxComponent, (FLOAT)KINDA_SMALL_NUMBER);
				LightData.Brightness = MaxComponent;
				LightData.FalloffExponent = CurrentLight.FalloffExponent;
				Swarm->Write(&LightData, sizeof(LightData));
			}
			
			Swarm->CloseCurrentChannel();
		}
		else
		{
			debugf(TEXT("Failed to open dominant shadow channel!"));
		}
	}

	/** Exports the volume distance field. */
	void FLightmassSolverExporter::ExportVolumeDistanceField(INT VolumeSizeX, INT VolumeSizeY, INT VolumeSizeZ, FLOAT VolumeMaxDistance, const FBox& DistanceFieldVolumeBounds, const TArray<FColor>& VolumeDistanceField) const
	{
		const FString ChannelName = CreateChannelName(VolumeDistanceFieldGuid, LM_MESHAREALIGHTDATA_VERSION, LM_MESHAREALIGHTDATA_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_MESHAREALIGHT_CHANNEL_FLAGS, TRUE);
		if( ErrorCode >= 0 )
		{
			Swarm->Write(&VolumeSizeX, sizeof(VolumeSizeX));
			Swarm->Write(&VolumeSizeY, sizeof(VolumeSizeY));
			Swarm->Write(&VolumeSizeZ, sizeof(VolumeSizeZ));
			Swarm->Write(&VolumeMaxDistance, sizeof(VolumeMaxDistance));
			Swarm->Write(&DistanceFieldVolumeBounds.Min, sizeof(DistanceFieldVolumeBounds.Min));
			Swarm->Write(&DistanceFieldVolumeBounds.Max, sizeof(DistanceFieldVolumeBounds.Max));

			WriteArray(VolumeDistanceField);

			Swarm->CloseCurrentChannel();
		}
		else
		{
			debugf(TEXT("Failed to open dominant shadow channel!"));
		}
	}

	/** Creates a new channel and exports everything in DebugOutput. */
	void FLightmassSolverExporter::ExportDebugInfo(const FDebugLightingOutput& DebugOutput) const
	{
		const FString ChannelName = CreateChannelName(DebugOutputGuid, LM_DEBUGOUTPUT_VERSION, LM_DEBUGOUTPUT_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_DEBUGOUTPUT_CHANNEL_FLAGS, TRUE);

		if( ErrorCode >= 0 )
		{
			Swarm->Write(&DebugOutput.bValid, sizeof(DebugOutput.bValid));
			WriteArray(DebugOutput.PathRays);
			WriteArray(DebugOutput.ShadowRays);
			WriteArray(DebugOutput.IndirectPhotonPaths);
			WriteArray(DebugOutput.SelectedVertexIndices);
			WriteArray(DebugOutput.Vertices);
			WriteArray(DebugOutput.CacheRecords);
			WriteArray(DebugOutput.DirectPhotons);
			WriteArray(DebugOutput.IndirectPhotons);
			WriteArray(DebugOutput.IrradiancePhotons);
			WriteArray(DebugOutput.GatheredCausticPhotons);
			WriteArray(DebugOutput.GatheredPhotons);
			WriteArray(DebugOutput.GatheredImportancePhotons);
			WriteArray(DebugOutput.GatheredPhotonNodes);
			Swarm->Write(&DebugOutput.bDirectPhotonValid, sizeof(DebugOutput.bDirectPhotonValid));
			Swarm->Write(&DebugOutput.GatheredDirectPhoton, sizeof(DebugOutput.GatheredDirectPhoton));
			Swarm->Write(&DebugOutput.TexelCorners, sizeof(DebugOutput.TexelCorners));
			Swarm->Write(&DebugOutput.bCornerValid, sizeof(DebugOutput.bCornerValid));
			Swarm->Write(&DebugOutput.SampleRadius, sizeof(DebugOutput.SampleRadius));

			Swarm->CloseCurrentChannel();
		}
		else
		{
			debugf(TEXT("Failed to open debug output channel!"));
		}
	}

	/** Writes a TArray to the channel on the top of the Swarm stack. */
	template<class T>
	void FLightmassSolverExporter::WriteArray(const TArray<T>& Array) const
	{
		const INT ArrayNum = Array.Num();
		Swarm->Write((void*)&ArrayNum, sizeof(ArrayNum));
		if (ArrayNum > 0)
		{
			Swarm->Write(Array.GetData(), Array.GetTypeSize() * ArrayNum);
		}
	}

	/**
	 * Write out bitmap files for a texture map
	 * @param BitmapBaseName Base file name for the bitmap (will have info appended for multiple components)
	 * @param Samples Texture map sample data
	 * @param Width Width of the texture map
	 * @param Height Height of the texture map
	 */
	template<INT NumComponents, typename SampleType>
	void WriteBitmap(const TCHAR* BitmapBaseName, SampleType* Samples, INT Width, INT Height)
	{
		// Without any data, just return
		if( Samples == NULL )
		{
			return;
		}

		// look to make sure that any texel has been mapped
		UBOOL bTextureIsMapped = FALSE;

		for(INT Y = 0; Y < Height; Y++)
		{
			for(INT X = 0; X < Width; X++)
			{
				const SampleType& Sample = Samples[Y * Width + X];
				if (Sample.bIsMapped)
				{
					bTextureIsMapped = TRUE;
					break;
				}
			}
		}

		// if it's all black, just punt out
		if (!bTextureIsMapped)
		{
			return;
		}

#pragma pack (push,1)
		struct BITMAPFILEHEADER
		{
			WORD	bfType GCC_PACK(1);
			DWORD	bfSize GCC_PACK(1);
			WORD	bfReserved1 GCC_PACK(1); 
			WORD	bfReserved2 GCC_PACK(1);
			DWORD	bfOffBits GCC_PACK(1);
		} FH; 
		struct BITMAPINFOHEADER
		{
			DWORD	biSize GCC_PACK(1); 
			INT		biWidth GCC_PACK(1);
			INT		biHeight GCC_PACK(1);
			WORD	biPlanes GCC_PACK(1);
			WORD	biBitCount GCC_PACK(1);
			DWORD	biCompression GCC_PACK(1);
			DWORD	biSizeImage GCC_PACK(1);
			INT		biXPelsPerMeter GCC_PACK(1); 
			INT		biYPelsPerMeter GCC_PACK(1);
			DWORD	biClrUsed GCC_PACK(1);
			DWORD	biClrImportant GCC_PACK(1); 
		} IH;
#pragma pack (pop)

		INT BytesPerLine = Align(Width * 3,4);

		HANDLE Files[NumComponents];
		for (INT CompIndex = 0; CompIndex < NumComponents; CompIndex++)
		{
			if (NumComponents == 1)
			{
				Files[CompIndex] = CreateFileW(*FString::Printf(TEXT("%s.bmp"), BitmapBaseName), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			}
			else
			{
				Files[CompIndex] = CreateFileW(*FString::Printf(TEXT("%s_Dir%d.bmp"), BitmapBaseName, CompIndex), GENERIC_WRITE, FILE_SHARE_READ, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			}
		}

		// File header.
		FH.bfType       		= (WORD) ('B' + 256*'M');
		FH.bfSize       		= (DWORD) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + BytesPerLine * Height);
		FH.bfReserved1  		= (WORD) 0;
		FH.bfReserved2  		= (WORD) 0;
		FH.bfOffBits    		= (DWORD) (sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER));
		::DWORD BytesWritten;
		for (INT CompIndex = 0; CompIndex < NumComponents; CompIndex++)
		{
			WriteFile(Files[CompIndex], &FH, sizeof(FH), &BytesWritten, NULL);
		}

		// Info header.
		IH.biSize               = (DWORD) sizeof(BITMAPINFOHEADER);
		IH.biWidth              = (DWORD) Width;
		IH.biHeight             = (DWORD) Height;
		IH.biPlanes             = (WORD) 1;
		IH.biBitCount           = (WORD) 24;
		IH.biCompression        = (DWORD) 0; //BI_RGB
		IH.biSizeImage          = (DWORD) BytesPerLine * Height;
		IH.biXPelsPerMeter      = (DWORD) 0;
		IH.biYPelsPerMeter      = (DWORD) 0;
		IH.biClrUsed            = (DWORD) 0;
		IH.biClrImportant       = (DWORD) 0;
		for (INT CompIndex = 0; CompIndex < NumComponents; CompIndex++)
		{
			WriteFile(Files[CompIndex], &IH, sizeof(IH), &BytesWritten, NULL);
		}

		// write out the image, bottom up
		for (INT Y = Height - 1; Y >= 0; Y--)
		{
			for (INT X = 0; X < Width; X++)
			{
				const SampleType& Sample = Samples[Y * Width + X];
				for (INT CompIndex = 0; CompIndex < NumComponents; CompIndex++)
				{
					FColor Color = Sample.GetColor(CompIndex);
					WriteFile(Files[CompIndex], &Color, 3, &BytesWritten, NULL);
				}
			}

			// pad if necessary
			static BYTE Zero[3] = {0, 0, 0};
			if (Width * 3 < BytesPerLine)
			{
				for (INT CompIndex = 0; CompIndex < NumComponents; CompIndex++)
				{
					WriteFile(Files[CompIndex], &Zero, BytesPerLine - Width * 3, &BytesWritten, NULL);
				}
			}
		}

		// close the open files
		for (INT CompIndex = 0; CompIndex < NumComponents; CompIndex++)
		{
			CloseHandle(Files[CompIndex]);
		}
	}

	INT FLightmassSolverExporter::BeginExportResults(struct FTextureMappingStaticLightingData& LightingData, UINT NumMappings) const
	{
		const FString ChannelName = CreateChannelName(LightingData.Mapping->Guid, LM_TEXTUREMAPPING_VERSION, LM_TEXTUREMAPPING_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_TEXTUREMAPPING_CHANNEL_FLAGS, TRUE);
		if( ErrorCode < 0 )
		{
			debugf(TEXT("Failed to open texture mapping channel %s!"), *ChannelName);
		}
		else
		{
			// Write out the number of mappings this channel will contain
			Swarm->Write(&NumMappings, sizeof(NumMappings));
		}
		return ErrorCode;
	}

	/**
	 * Send complete lighting data to UE3
	 *
	 * @param LightingData - Object containing the computed data
	 */
	void FLightmassSolverExporter::ExportResults( FTextureMappingStaticLightingData& LightingData, UBOOL bUseUniqueChannel ) const
	{
		debugfSlow(TEXT("Exporting texture lighting %s [%.3fs]"), *(LightingData.Mapping->Guid.String()), LightingData.ExecutionTime);

		// If requested, use a unique channel for this mapping, otherwise, just use the one that is already open
		if (bUseUniqueChannel)
		{
			if (BeginExportResults(LightingData, 1) < 0)
			{
				return;
			}
		}

		const INT PaddedOffset = LightingData.Mapping->bPadded ? 1 : 0;
		const INT DebugSampleIndex = LightingData.Mapping == Scene.DebugMapping
			? (Scene.DebugInput.LocalY + PaddedOffset) * LightingData.Mapping->SizeX + Scene.DebugInput.LocalX + PaddedOffset
			: INDEX_NONE;

		if (bDumpTextures)
		{
			WriteBitmap<4>(*(LightingData.Mapping->Guid.String() + TEXT("_LM")), LightingData.LightMapData->GetData(), LightingData.LightMapData->GetSizeX(), LightingData.LightMapData->GetSizeY());
		}

		// Quantize the data if it hasn't been yet
		LightingData.LightMapData->Quantize(
			DebugSampleIndex,
			Scene.SceneConstants.bUseFixedScaleForSimpleLightmaps,
			Scene.SceneConstants.UseFixedScaleValue
		);

#if LM_COMPRESS_OUTPUT_DATA && !LM_COMPRESS_ON_EACH_THREAD
		// If we need to compress the data before writing out, do it now
		LightingData.LightMapData->Compress(
			DebugSampleIndex,
			Scene.SceneConstants.bUseFixedScaleForSimpleLightmaps,
			Scene.SceneConstants.UseFixedScaleValue
		);
//		debugf(TEXT("LM data went from %d to %d bytes"), LightingData.LightMapData->UncompressedDataSize, LightingData.LightMapData->CompressedDataSize);
#endif
		const INT ShadowMapCount = LightingData.ShadowMaps.Num();
		const INT SignedDistanceFieldShadowMapCount = LightingData.SignedDistanceFieldShadowMaps.Num();
		const INT NumLights = LightingData.LightMapData->Lights.Num();
	
#pragma pack (push,1)
		struct FTextureHeader
		{
			FTextureHeader(FGuid& InGuid, DOUBLE InExecutionTime, FLightMapData2DData& InData, INT InShadowMapCount, INT InSignedDistanceFieldShadowMapCount, INT InLightCount)
				: Guid(InGuid), ExecutionTime(InExecutionTime), Data(InData), ShadowMapCount(InShadowMapCount), SignedDistanceFieldShadowMapCount(InSignedDistanceFieldShadowMapCount), LightCount(InLightCount)
			{}
			FGuid Guid;
			DOUBLE ExecutionTime;
			FLightMapData2DData Data;
			INT ShadowMapCount;
			INT SignedDistanceFieldShadowMapCount;
			INT LightCount;
		};
#pragma pack (pop)
		FTextureHeader Header(LightingData.Mapping->Guid, LightingData.ExecutionTime, *(FLightMapData2DData*)LightingData.LightMapData, LightingData.ShadowMaps.Num(), LightingData.SignedDistanceFieldShadowMaps.Num(), NumLights);
		Swarm->Write(&Header, sizeof(Header));

		for (INT LightIndex = 0; LightIndex < NumLights; LightIndex++)
		{
			FGuid CurrentGuid = LightingData.LightMapData->Lights(LightIndex)->Guid;
			Swarm->Write(&CurrentGuid, sizeof(CurrentGuid));
		}

#if LM_COMPRESS_OUTPUT_DATA
		// Write out compressed data if supported
		Swarm->Write(LightingData.LightMapData->GetCompressedData(), LightingData.LightMapData->CompressedDataSize ? LightingData.LightMapData->CompressedDataSize : LightingData.LightMapData->UncompressedDataSize);
#else
		// Write out uncompressed raw data
		const FQuantizedLightSampleData* SampleData = LightingData.LightMapData->GetQuantizedData();
		Swarm->Write((void*)SampleData, sizeof(FQuantizedLightSampleData) * LightingData.LightMapData->GetSizeX() * LightingData.LightMapData->GetSizeY());
#endif
		Swarm->Write(&LightingData.PreviewEnvironmentShadowing, sizeof(LightingData.PreviewEnvironmentShadowing));

		// The resulting light GUID --> shadow map data
		INT ShadowIndex = 0;
		for (TMap<const FLight*,FShadowMapData2D*>::TIterator It(LightingData.ShadowMaps); It; ++It, ++ShadowIndex)
		{
			FGuid OutGuid = It.Key()->Guid;
			FShadowMapData2D* OutData = It.Value();

			if (bDumpTextures)
			{
				WriteBitmap<1>(*FString::Printf(TEXT("%s_SM_%d"), *LightingData.Mapping->Guid.String(), ShadowIndex), OutData->GetData(), OutData->GetSizeX(), OutData->GetSizeY());
			}

			// Quantize the data if it hasn't been yet
			OutData->Quantize(INDEX_NONE);

#if LM_COMPRESS_OUTPUT_DATA && !LM_COMPRESS_ON_EACH_THREAD
			// If we need to compress the data before writing out, do it now
			OutData->Compress(INDEX_NONE);
#endif
			Swarm->Write(&OutGuid, sizeof(FGuid));
			Swarm->Write((FShadowMapData2DData*)OutData, sizeof(FShadowMapData2DData));

#if LM_COMPRESS_OUTPUT_DATA
			// Write out compressed data if supported
			Swarm->Write(OutData->GetCompressedData(), OutData->CompressedDataSize ? OutData->CompressedDataSize : OutData->UncompressedDataSize);
#else
			// Write out uncompressed raw data
			const FQuantizedShadowSampleData* ShadowSampleData = OutData->GetQuantizedData();
			Swarm->Write((void*)ShadowSampleData, sizeof(FQuantizedShadowSampleData) * OutData->GetSizeX() * OutData->GetSizeY());
#endif
		}

		ShadowIndex = 0;
		for (TMap<const FLight*,FSignedDistanceFieldShadowMapData2D*>::TIterator It(LightingData.SignedDistanceFieldShadowMaps); It; ++It, ++ShadowIndex)
		{
			FGuid OutGuid = It.Key()->Guid;
			FSignedDistanceFieldShadowMapData2D* OutData = It.Value();

			// Quantize the data if it hasn't been yet
			OutData->Quantize(INDEX_NONE);

#if LM_COMPRESS_OUTPUT_DATA && !LM_COMPRESS_ON_EACH_THREAD
			// If we need to compress the data before writing out, do it now
			OutData->Compress(INDEX_NONE);
#endif
			Swarm->Write(&OutGuid, sizeof(FGuid));
			Swarm->Write((FSignedDistanceFieldShadowMapData2DData*)OutData, sizeof(FSignedDistanceFieldShadowMapData2DData));

#if LM_COMPRESS_OUTPUT_DATA
			// Write out compressed data if supported
			Swarm->Write(OutData->GetCompressedData(), OutData->CompressedDataSize ? OutData->CompressedDataSize : OutData->UncompressedDataSize);
#else
			// Write out uncompressed raw data
			const FQuantizedSignedDistanceFieldShadowSampleData* ShadowSampleData = OutData->GetQuantizedData();
			Swarm->Write((void*)ShadowSampleData, sizeof(FQuantizedSignedDistanceFieldShadowSampleData) * OutData->GetSizeX() * OutData->GetSizeY());
#endif
		}

		// free up the calculated data
		delete LightingData.LightMapData;
		LightingData.ShadowMaps.Empty();
		LightingData.SignedDistanceFieldShadowMaps.Empty();

		// Only close the channel if we opened it
		if (bUseUniqueChannel)
		{
			EndExportResults();
		}
	}

	void FLightmassSolverExporter::ExportResults(const FPrecomputedVisibilityData& TaskData) const
	{
		const FString ChannelName = CreateChannelName(TaskData.Guid, LM_PRECOMPUTEDVISIBILITY_VERSION, LM_PRECOMPUTEDVISIBILITY_EXTENSION);
		const INT ErrorCode = Swarm->OpenChannel(*ChannelName, LM_PRECOMPUTEDVISIBILITY_CHANNEL_FLAGS, TRUE);
		if( ErrorCode >= 0 )
		{
			const INT NumCells = TaskData.PrecomputedVisibilityCells.Num();
			Swarm->Write(&NumCells, sizeof(NumCells));
			for (INT CellIndex = 0; CellIndex < NumCells; CellIndex++)
			{
				Swarm->Write(&TaskData.PrecomputedVisibilityCells(CellIndex).Bounds, sizeof(TaskData.PrecomputedVisibilityCells(CellIndex).Bounds));
				WriteArray(TaskData.PrecomputedVisibilityCells(CellIndex).VisibilityData);
			}
			WriteArray(TaskData.DebugVisibilityRays);
			Swarm->CloseCurrentChannel();
		}
		else
		{
			debugf(TEXT("Failed to open precomputed visibility channel!"));
		}
	}

}	//Lightmass
