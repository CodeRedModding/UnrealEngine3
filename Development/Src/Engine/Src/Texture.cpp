/*=============================================================================
	Texture.cpp: Implementation of UTexture.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"

IMPLEMENT_CLASS(UTexture);

void UTexture::ReleaseResource()
{
	check(Resource);

	// Free the resource.
	ReleaseResourceAndFlush(Resource);
	delete Resource;
	Resource = NULL;
}

void UTexture::UpdateResource()
{
	if(Resource)
	{
		// Release the existing texture resource.
		ReleaseResource();
	}

	//Dedicated servers have no texture internals
	if( !GIsUCC && !HasAnyFlags(RF_ClassDefaultObject) && ((appGetPlatformType() & UE3::PLATFORM_WindowsServer) == 0) )
	{
		// Create a new texture resource.
		Resource = CreateResource();
		if( Resource )
		{
			BeginInitResource(Resource);
		}
	}
}

/**
 * Returns the cached combined LOD bias based on texture LOD group and LOD bias.
 *
 * @return	LOD bias
 */
INT UTexture::GetCachedLODBias() const
{
	return CachedCombinedLODBias;
}

void UTexture::PreEditChange(UProperty* PropertyAboutToChange)
{
	Super::PreEditChange(PropertyAboutToChange);
	if( Resource )
	{
		ReleaseResource();
	}

	// backup old "LODGroup" to detect changes from specific enum values to a new value
	CachedLODGroup = LODGroup;
}

void UTexture::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	SetLightingGuid();

	// Determine whether any property that requires recompression of the texture, or notification to Materials has changed.
	UBOOL RequiresRecompression = FALSE;
	UBOOL RequiresNotifyMaterials = FALSE;
	UProperty* PropertyThatChanged = PropertyChangedEvent.Property;
	if( PropertyThatChanged )
	{
		FString PropertyName = *PropertyThatChanged->GetName();

		if( appStricmp( *PropertyName, TEXT("CompressionSettings")			) == 0 )
		{
			RequiresNotifyMaterials = TRUE;
			RequiresRecompression = TRUE;
		}

		if( appStricmp( *PropertyName, TEXT("RGBE")							) == 0
		||	appStricmp( *PropertyName, TEXT("CompressionNoAlpha")			) == 0
		||	appStricmp( *PropertyName, TEXT("CompressionNone")				) == 0
		||	appStricmp( *PropertyName, TEXT("CompressionFullDynamicRange")	) == 0
		||	appStricmp( *PropertyName, TEXT("bDitherMipMapAlpha")			) == 0
		||	appStricmp( *PropertyName, TEXT("bPreserveBorderR")				) == 0
		||	appStricmp( *PropertyName, TEXT("bPreserveBorderG")				) == 0
		||	appStricmp( *PropertyName, TEXT("bPreserveBorderB")				) == 0
		||	appStricmp( *PropertyName, TEXT("bPreserveBorderA")				) == 0
		||	appStricmp( *PropertyName, TEXT("AdjustBrightness")				) == 0
		||	appStricmp( *PropertyName, TEXT("AdjustBrightnessCurve")		) == 0
		||	appStricmp( *PropertyName, TEXT("AdjustVibrance")				) == 0
		||	appStricmp( *PropertyName, TEXT("AdjustSaturation")				) == 0
		||	appStricmp( *PropertyName, TEXT("AdjustRGBCurve")				) == 0
		||	appStricmp( *PropertyName, TEXT("AdjustHue")					) == 0
		||	appStricmp( *PropertyName, TEXT("DeferCompression")				) == 0
		||	appStricmp( *PropertyName, TEXT("MipGenSettings")				) == 0
		||	appStricmp( *PropertyName, TEXT("MipsToRemoveOnCompress")		) == 0
		)
		{
			RequiresRecompression = TRUE;
		}

		if(	appStricmp( *PropertyName, TEXT("LODGroup")						) == 0)
		{	
			// from or to TEXTUREGROUP_ColorLookupTable/TEXTUREGROUP_Bokeh
			if(LODGroup == TEXTUREGROUP_ColorLookupTable || CachedLODGroup == TEXTUREGROUP_ColorLookupTable
			|| LODGroup == TEXTUREGROUP_Bokeh || CachedLODGroup == TEXTUREGROUP_Bokeh)
			{
				RequiresRecompression = TRUE;
			}

			CachedLODGroup = 0;
		}

	}		
	else if(PropertyChangedEvent.ChangeType == EPropertyChangeType::Undo ||
			PropertyChangedEvent.ChangeType == EPropertyChangeType::Redo)
	{
		RequiresRecompression = FALSE;
	}
	else
	{
		RequiresRecompression = TRUE;
	}

	// Update cached LOD bias.
	NumCinematicMipLevels = Max<INT>( NumCinematicMipLevels, 0 );
	CachedCombinedLODBias = GSystemSettings.TextureLODSettings.CalculateLODBias( this );

	// Only compress when we really need to to avoid lag when level designers/ artists manipulate properties like clamping in the editor.
	if (RequiresRecompression)
	{
		UBOOL CompressionNoneSave = CompressionNone;
		if (!(
			(CompressionSettings == TC_Default)	||
			(CompressionSettings == TC_Normalmap) || 
			(CompressionSettings == TC_NormalmapAlpha) ||
			(CompressionSettings == TC_NormalmapBC5)
			))
		{
			DeferCompression = FALSE;
		}

		if (DeferCompression)
		{
			CompressionNone = TRUE;
		}

		// Track if we started a slow task separately instead of testing against CompressionNone 
		// when ending the slow task because that variable can change during texture compression.
		UBOOL bBeganSlowTask = FALSE;
		
		// Pop-up the progress dialog if we are actually compressing. 
		if( !CompressionNone )
		{
			GWarn->BeginSlowTask( *FString::Printf( LocalizeSecure(LocalizeUnrealEd("SavingPackage_CompressingTexture"), *GetName()) ), TRUE );
			bBeganSlowTask = TRUE;
		}

		Compress();
		if (DeferCompression)
		{
			CompressionNone = CompressionNoneSave;
		}

		// Make sure to hide the progress dialog after compression is finished.
		if( bBeganSlowTask )
		{
			GWarn->EndSlowTask();
		}
	}

	// Recreate the texture's resource.
	UpdateResource();

	GCallbackEvent->Send(CALLBACK_TextureModified, this);

	// Notify any loaded material instances if changed our compression format
	if (RequiresNotifyMaterials)
	{
		// Notify any material that uses this texture
		for( TObjectIterator<UMaterial> It; It; ++It )
		{
			UMaterial* CurrentMaterial = *It;
			if (CurrentMaterial->UsesTexture(this))
			{
				CurrentMaterial->PostEditChange();
				if (GCallbackEvent != NULL)
				{
					GCallbackEvent->Send(CALLBACK_MaterialTextureSettingsChanged, CurrentMaterial);
				}
			}
		}

		// Go through all loaded material instances and recompile their static permutation resources if needed
		// This is necessary if we changed a format on a normal map.
		for( TObjectIterator<UMaterialInstance> It; It; ++It )
		{
			UMaterialInstance* CurrentMaterialInstance = *It;
			if (CurrentMaterialInstance->UsesTexture(this))
			{
				CurrentMaterialInstance->InitStaticPermutation();
				if (GCallbackEvent != NULL)
				{
					GCallbackEvent->Send(CALLBACK_MaterialTextureSettingsChanged, CurrentMaterialInstance);
				}
			}
		}
	}
}

void UTexture::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);

	SourceArt.Serialize( Ar, this );

	if (Ar.Ver() < VER_INTEGRATED_LIGHTMASS)
	{
		SetLightingGuid();
	}
}

void UTexture::PostLoad()
{
	Super::PostLoad();

	// to be backwards compatible, the flag was removed
	if(CompressionNoMipmaps_DEPRECATED)
	{
		MipGenSettings = TMGS_NoMipmaps;
	}

	// High dynamic range textures are currently always stored as RGBE (shared exponent) textures.
	// We explicitly set this here as older versions of the engine didn't correctly update the RGBE field.
	// @todo: Ensures that RGBE is correctly set to work around a bug in older versions of the engine.
	RGBE = (CompressionSettings == TC_HighDynamicRange);

	if( !IsTemplate() )
	{
		// Update cached LOD bias.
		CachedCombinedLODBias = GSystemSettings.TextureLODSettings.CalculateLODBias( this );

		// The texture will be cached by the cubemap it is contained within on consoles.
		UTextureCube* CubeMap = Cast<UTextureCube>(GetOuter());
		if (CubeMap == NULL)
		{
			// Recreate the texture's resource.
			UpdateResource();
		}
	}
}

void UTexture::BeginDestroy()
{
	Super::BeginDestroy();
	if( !UpdateStreamingStatus() && Resource )
	{
		// Send the rendering thread a release message for the texture's resource.
		BeginReleaseResource(Resource);
		Resource->ReleaseFence.BeginFence();
		// Keep track that we alrady kicked off the async release.
		bAsyncResourceReleaseHasBeenStarted = TRUE;
	}
}

UBOOL UTexture::IsReadyForFinishDestroy()
{
	UBOOL bReadyForFinishDestroy = FALSE;
	// Check whether super class is ready and whether we have any pending streaming requests in flight.
	if( Super::IsReadyForFinishDestroy() && !UpdateStreamingStatus() )
	{
		// Kick off async resource release if we haven't already.
		if( !bAsyncResourceReleaseHasBeenStarted && Resource )
		{
			// Send the rendering thread a release message for the texture's resource.
			BeginReleaseResource(Resource);
			Resource->ReleaseFence.BeginFence();
			// Keep track that we alrady kicked off the async release.
			bAsyncResourceReleaseHasBeenStarted = TRUE;
		}
		// Only allow FinishDestroy to be called once the texture resource has finished its rendering thread cleanup.
		else if( !Resource || !Resource->ReleaseFence.GetNumPendingFences() )
		{
			bReadyForFinishDestroy = TRUE;
		}
	}
	return bReadyForFinishDestroy;
}

void UTexture::FinishDestroy()
{
	Super::FinishDestroy();

	if(Resource)
	{
		check(!Resource->ReleaseFence.GetNumPendingFences());

		// Free the resource.
		delete Resource;
		Resource = NULL;
	}
}

void UTexture::PreSave()
{
	GCallbackEvent->Send( CALLBACK_TexturePreSave, this );

	Super::PreSave();

#if WITH_EDITORONLY_DATA
	// If "no compress" is set, don't do it...
	if (CompressionNone)
	{
		return;
	}

	if( HasSourceArt() && bIsSourceArtUncompressed )
	{
		GWarn->StatusUpdatef( 0, 0, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("SavingPackage_CompressingSourceArt"), *GetName()) ) );

		// Since we defer compression of the texture's source art, we need to 
		// compress source art before we attempt to compress and save. 
		CompressSourceArt();
	}

	// Otherwise, if we are not already compressed, do it now.
	if (DeferCompression)
	{
		GWarn->StatusUpdatef( 0, 0, *FString::Printf( LocalizeSecure(LocalizeUnrealEd("SavingPackage_CompressingTexture"), *GetName()) ) );

		Compress();
		DeferCompression = FALSE;
	}
#endif // WITH_EDITORONLY_DATA
}

/**
 * Used by various commandlets to purge editor only and platform-specific data from various objects
 * 
 * @param PlatformsToKeep Platforms for which to keep platform-specific data
 * @param bStripLargeEditorData If TRUE, data used in the editor, but large enough to bloat download sizes, will be removed
 */
void UTexture::StripData(UE3::EPlatformType PlatformsToKeep, UBOOL bStripLargeEditorData)
{
	Super::StripData(PlatformsToKeep, bStripLargeEditorData); 

#if WITH_EDITORONLY_DATA
	// Remove source art if we are stripping large data, or if
	// we aren't keeping any non-stripped platforms
	if (bStripLargeEditorData || !(PlatformsToKeep & ~UE3::PLATFORM_Stripped))
	{
		SourceArt.RemoveBulkData();
	}
#endif // WITH_EDITORONLY_DATA
}


/**
 *	Gets the average brightness of the texture
 *
 *	@param	bIgnoreTrueBlack		If TRUE, then pixels w/ 0,0,0 rgb values do not contribute.
 *	@param	bUseGrayscale			If TRUE, use gray scale else use the max color component.
 *
 *	@return	FLOAT					The average brightness of the texture
 */
FLOAT UTexture::GetAverageBrightness(UBOOL bIgnoreTrueBlack, UBOOL bUseGrayscale)
{
	// Indicate the action was not performed...
	return -1.0f;
}

/** Helper functions for text output of texture properties... */
#ifndef CASE_ENUM_TO_TEXT
#define CASE_ENUM_TO_TEXT(txt) case txt: return TEXT(#txt);
#endif

#ifndef TEXT_TO_ENUM
#define TEXT_TO_ENUM(eVal, txt)		if (appStricmp(TEXT(#eVal), txt) == 0)	return eVal;
#endif

const TCHAR* UTexture::GetCompressionSettingsString(TextureCompressionSettings InCompressionSettings)
{
	switch (InCompressionSettings)
	{
	FOREACH_ENUM_TEXTURECOMPRESSIONSETTINGS(CASE_ENUM_TO_TEXT)
	}

	return TEXT("TC_Default");
}

TextureCompressionSettings UTexture::GetCompressionSettingsFromString(const TCHAR* InCompressionSettingsStr)
{
	#define TEXT_TO_COMPRESSIONSETTINGS(s) TEXT_TO_ENUM(s, InCompressionSettingsStr);
	FOREACH_ENUM_TEXTURECOMPRESSIONSETTINGS(TEXT_TO_COMPRESSIONSETTINGS)
	#undef TEXT_TO_COMPRESSIONSETTINGS
	return TC_Default;
}

const TCHAR* UTexture::GetPixelFormatString(EPixelFormat InPixelFormat)
{
	switch (InPixelFormat)
	{
	FOREACH_ENUM_EPIXELFORMAT(CASE_ENUM_TO_TEXT)
	}
	return TEXT("PF_Unknown");
}

EPixelFormat UTexture::GetPixelFormatFromString(const TCHAR* InPixelFormatStr)
{
#define TEXT_TO_PIXELFORMAT(f) TEXT_TO_ENUM(f, InPixelFormatStr);
	FOREACH_ENUM_EPIXELFORMAT(TEXT_TO_PIXELFORMAT)
#undef TEXT_TO_PIXELFORMAT
	return PF_Unknown;
}

const TCHAR* UTexture::GetTextureFilterString(TextureFilter InFilter)
{
	switch (InFilter)
	{
	FOREACH_ENUM_TEXTUREFILTER(CASE_ENUM_TO_TEXT)
	}
	return TEXT("TF_Nearest");
}

TextureFilter UTexture::GetTextureFilterFromString(const TCHAR* InFilterStr)
{
#define TEXT_TO_FILTER(f) TEXT_TO_ENUM(f, InFilterStr);
	FOREACH_ENUM_TEXTUREFILTER(TEXT_TO_FILTER)
#undef TEXT_TO_FILTER
	
	return TF_Nearest;
}

const TCHAR* UTexture::GetTextureAddressString(TextureAddress InAddress)
{
	switch (InAddress)
	{
	FOREACH_ENUM_TEXTUREADDRESS(CASE_ENUM_TO_TEXT)
	}

	return TEXT("TA_Wrap");
}
TextureAddress UTexture::GetTextureAddressFromString(const TCHAR* InAddressStr)
{
#define TEXT_TO_ADDRESS(a) TEXT_TO_ENUM(a, InAddressStr);
	FOREACH_ENUM_TEXTUREADDRESS(TEXT_TO_ADDRESS)
#undef TEXT_TO_ADDRESS
	return TA_Wrap;
}

const TCHAR* UTexture::GetTextureGroupString(TextureGroup InGroup)
{
	switch (InGroup)
	{
		FOREACH_ENUM_TEXTUREGROUP(CASE_ENUM_TO_TEXT)
	}

	return TEXT("TEXTUREGROUP_World");
}

TextureGroup UTexture::GetTextureGroupFromString(const TCHAR* InGroupStr)
{
#define TEXT_TO_GROUP(g) TEXT_TO_ENUM(g, InGroupStr);
	FOREACH_ENUM_TEXTUREGROUP(TEXT_TO_GROUP)
#undef TEXT_TO_GROUP
	return TEXTUREGROUP_World;
}

DWORD UTexture::GetTextureGroupBitfield( const FTextureGroupContainer& TextureGroups )
{
	DWORD TexGroupBitfield = 0;
#define GROUPBITFIELD(g) TexGroupBitfield |= TextureGroups.g << g;
	FOREACH_ENUM_TEXTUREGROUP(GROUPBITFIELD)
#undef GROUPBITFIELD

	return TexGroupBitfield;
}


const TCHAR* UTexture::GetMipGenSettingsString(TextureMipGenSettings InEnum)
{
	switch(InEnum)
	{
		default:
		FOREACH_ENUM_TEXTUREMIPGENSETTINGS(CASE_ENUM_TO_TEXT)
	}
}

TextureMipGenSettings UTexture::GetMipGenSettingsFromString(const TCHAR* InStr, UBOOL bTextureGroup)
{
#define TEXT_TO_MIPGENSETTINGS(m) TEXT_TO_ENUM(m, InStr);
	FOREACH_ENUM_TEXTUREMIPGENSETTINGS(TEXT_TO_MIPGENSETTINGS)
#undef TEXT_TO_MIPGENSETTINGS

	// default for TextureGroup and Texture is different
	return bTextureGroup ? TMGS_SimpleAverage : TMGS_FromTextureGroup;
}



/**
 * Initializes LOD settings by reading them from the passed in filename/ section.
 *
 * @param	IniFilename		Filename of ini to read from.
 * @param	IniSection		Section in ini to look for settings
 */
void FTextureLODSettings::Initialize( const TCHAR* IniFilename, const TCHAR* IniSection )
{
	// Read individual entries. This must be updated whenever new entries are added to the enumeration.
#define GROUPREADENTRY(g) ReadEntry( g, TEXT(#g), IniFilename, IniSection );
	FOREACH_ENUM_TEXTUREGROUP(GROUPREADENTRY)
#undef GROUPREADENTRY
}

/**
 * Returns the texture group names, sorted like enum.
 *
 * @return array of texture group names
 */
TArray<FString> FTextureLODSettings::GetTextureGroupNames()
{
	TArray<FString> TextureGroupNames;

#define GROUPNAMES(g) new(TextureGroupNames) FString(TEXT(#g));
	FOREACH_ENUM_TEXTUREGROUP(GROUPNAMES)
#undef GROUPNAMES

	return TextureGroupNames;
}

/**
 * Reads a single entry and parses it into the group array.
 *
 * @param	GroupId			Id/ enum of group to parse
 * @param	GroupName		Name of group to look for in ini
 * @param	IniFilename		Filename of ini to read from.
 * @param	IniSection		Section in ini to look for settings
 */
void FTextureLODSettings::ReadEntry( INT GroupId, const TCHAR* GroupName, const TCHAR* IniFilename, const TCHAR* IniSection )
{
	// Look for string in filename/ section.
	FString Entry;
	if( GConfig->GetString( IniSection, GroupName, Entry, IniFilename ) )
	{
		// Trim whitespace at the beginning.
		Entry = Entry.Trim();
		// Remove brackets.
		Entry = Entry.Replace( TEXT("("), TEXT("") );
		Entry = Entry.Replace( TEXT(")"), TEXT("") );
		
		// Parse minimum LOD mip count.
		INT	MinLODSize = 0;
		if( Parse( *Entry, TEXT("MinLODSize="), MinLODSize ) )
		{
			TextureLODGroups[GroupId].MinLODMipCount = appCeilLogTwo( MinLODSize );
		}

		// Parse maximum LOD mip count.
		INT MaxLODSize = 0;
		if( Parse( *Entry, TEXT("MaxLODSize="), MaxLODSize ) )
		{
			TextureLODGroups[GroupId].MaxLODMipCount = appCeilLogTwo( MaxLODSize );
		}

		// Parse LOD bias.
		INT LODBias = 0;
		if( Parse( *Entry, TEXT("LODBias="), LODBias ) )
		{
			TextureLODGroups[GroupId].LODBias = LODBias;
		}

		// Parse min/map/mip filter names.
		FName MinMagFilter = NAME_Aniso;
		Parse( *Entry, TEXT("MinMagFilter="), MinMagFilter );
		FName MipFilter = NAME_Point;
		Parse( *Entry, TEXT("MipFilter="), MipFilter );

		{
			FString MipGenSettings;
			Parse( *Entry, TEXT("MipGenSettings="), MipGenSettings );
			TextureLODGroups[GroupId].MipGenSettings = UTexture::GetMipGenSettingsFromString(*MipGenSettings, TRUE);
		}

		// Convert into single filter enum. The code is layed out such that invalid input will 
		// map to the default state of highest quality filtering.

		// Linear filtering
		if( MinMagFilter == NAME_Linear )
		{
			if( MipFilter == NAME_Point )
			{
				TextureLODGroups[GroupId].Filter = SF_Bilinear;
			}
			else
			{
				TextureLODGroups[GroupId].Filter = SF_Trilinear;
			}
		}
		// Point. Don't even care about mip filter.
		else if( MinMagFilter == NAME_Point )
		{
			TextureLODGroups[GroupId].Filter = SF_Point;
		}
		// Aniso or unknown.
		else
		{
			if( MipFilter == NAME_Point )
			{
				TextureLODGroups[GroupId].Filter = SF_AnisotropicPoint;
			}
			else
			{
				TextureLODGroups[GroupId].Filter = SF_AnisotropicLinear;
			}
		}

		// Parse NumStreamedMips
		INT NumStreamedMips = -1;
		if( Parse( *Entry, TEXT("NumStreamedMips="), NumStreamedMips ) )
		{
			TextureLODGroups[GroupId].NumStreamedMips = NumStreamedMips;
		}
	}
}

/**
 * Calculates and returns the LOD bias based on texture LOD group, LOD bias and maximum size.
 *
 * @param	Texture		Texture object to calculate LOD bias for.
 * @return	LOD bias
 */
INT FTextureLODSettings::CalculateLODBias( UTexture* Texture ) const
{	
	// Find LOD group.
	check( Texture );
	const FTextureLODGroup& LODGroup = TextureLODGroups[Texture->LODGroup];

	// Calculate maximum number of miplevels.
	INT TextureMaxLOD	= appCeilLogTwo( appTrunc( Max( Texture->GetSurfaceWidth(), Texture->GetSurfaceHeight() ) ) );

	// Calculate LOD bias.
	INT UsedLODBias		= LODGroup.LODBias + Texture->LODBias + Texture->NumCinematicMipLevels;
	INT MinLOD			= LODGroup.MinLODMipCount - Texture->InternalFormatLODBias;
	INT MaxLOD			= LODGroup.MaxLODMipCount - Texture->InternalFormatLODBias;
	INT WantedMaxLOD	= Clamp( TextureMaxLOD - UsedLODBias, MinLOD, MaxLOD );
	WantedMaxLOD		= Clamp( WantedMaxLOD, 0, TextureMaxLOD );
	UsedLODBias			= TextureMaxLOD - WantedMaxLOD;

	return UsedLODBias;
}

/** 
* Useful for stats in the editor.
*/
void FTextureLODSettings::ComputeInGameMaxResolution(INT LODBias, UTexture &Texture, UINT &OutSizeX, UINT &OutSizeY) const
{
	UINT ImportedSizeX = appTrunc(Texture.GetSurfaceWidth());
	UINT ImportedSizeY = appTrunc(Texture.GetSurfaceHeight());
	
	const FTextureLODGroup& LODGroup = GetTextureLODGroup((TextureGroup)Texture.LODGroup);

	UINT SourceLOD = Max(appCeilLogTwo(ImportedSizeX), appCeilLogTwo(ImportedSizeY));
	UINT MinLOD = Max(UINT(GMinTextureResidentMipCount - 1), (UINT)LODGroup.MinLODMipCount);
	UINT MaxLOD = Min(UINT(GMaxTextureMipCount - 1), (UINT)LODGroup.MaxLODMipCount);
	UINT GameLOD = Min(SourceLOD, Clamp(SourceLOD - LODBias, MinLOD, MaxLOD));

	UINT DeltaLOD = SourceLOD - GameLOD;

	OutSizeX = ImportedSizeX >> DeltaLOD;
	OutSizeY = ImportedSizeY >> DeltaLOD;
}

/**
* TextureLODGroups access with bounds check
*
* @param   GroupIndex      usually from Texture.LODGroup
* @return                  A handle to the indexed LOD group. 
*/
const FTextureLODSettings::FTextureLODGroup& FTextureLODSettings::GetTextureLODGroup(TextureGroup GroupIndex) const
{
	check((UINT)GroupIndex < TEXTUREGROUP_MAX);
	return TextureLODGroups[GroupIndex];
}

void FTextureLODSettings::GetMipGenSettings(UTexture& Texture, FLOAT& OutSharpen, UINT& OutKernelSize, UBOOL& bOutDownsampleWithAverage, UBOOL& bOutSharpenWithoutColorShift, UBOOL &bOutBorderColorBlack) const
{
	TextureMipGenSettings Setting = (TextureMipGenSettings)Texture.MipGenSettings;

	bOutBorderColorBlack = FALSE;

	// avoiding the color shift assumes we deal with colors which is not true for normalmaps
	// or we blur where it's good to blur the color as well
	bOutSharpenWithoutColorShift = !Texture.IsNormalMap();

	bOutDownsampleWithAverage = TRUE;

	// inherit from texture group
	if(Setting == TMGS_FromTextureGroup)
	{
		const FTextureLODGroup& LODGroup = TextureLODGroups[Texture.LODGroup];

		Setting = LODGroup.MipGenSettings;
	}

	// ------------

	// default:
	OutSharpen = 0;
	OutKernelSize = 2;

	if(Setting >= TMGS_Sharpen0 && Setting <= TMGS_Sharpen10)
	{
		// 0 .. 2.0f
		OutSharpen = ((INT)Setting - (INT)TMGS_Sharpen0) * 0.2f;
		OutKernelSize = 8;
		// @todo ib2merge: Chair added this line, but I can't say the side effects
		// bOutDownsampleWithAverage = FALSE;
	}
	else if(Setting >= TMGS_Blur1 && Setting <= TMGS_Blur5)
	{
		INT BlurFactor = ((INT)Setting + 1 - (INT)TMGS_Blur1);
		OutSharpen = -BlurFactor * 2;
		OutKernelSize = 2 + 2 * BlurFactor;
		bOutDownsampleWithAverage = FALSE;
		bOutSharpenWithoutColorShift = FALSE;
		bOutBorderColorBlack = TRUE;
	}
}


/**
 * Will return the LODBias for a passed in LODGroup
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	LODBias
 */
INT FTextureLODSettings::GetTextureLODGroupLODBias( INT InLODGroup ) const
{
	INT Retval = 0;

	const FTextureLODGroup& LODGroup = TextureLODGroups[InLODGroup]; 

	Retval = LODGroup.LODBias;

	return Retval;
}

/**
 * Returns the LODGroup setting for number of streaming mip-levels.
 * -1 means that all mip-levels are allowed to stream.
 *
 * @param	InLODGroup		The LOD Group ID 
 * @return	Number of streaming mip-levels for textures in the specified LODGroup
 */
INT FTextureLODSettings::GetNumStreamedMips( INT InLODGroup ) const
{
	INT Retval = 0;

	const FTextureLODGroup& LODGroup = TextureLODGroups[InLODGroup]; 

	Retval = LODGroup.NumStreamedMips;

	return Retval;
}

/**
 * Returns the filter state that should be used for the passed in texture, taking
 * into account other system settings.
 *
 * @param	Texture		Texture to retrieve filter state for
 * @return	Filter sampler state for passed in texture
 */
ESamplerFilter FTextureLODSettings::GetSamplerFilter( const UTexture* Texture ) const
{
	// Default to point filtering.
	ESamplerFilter Filter = SF_Point;

	// Only diverge from default for valid textures that don't use point filtering.
	if( !Texture || Texture->Filter != TF_Nearest )
	{
		// Use LOD group value to find proper filter setting.
		Filter = TextureLODGroups[Texture->LODGroup].Filter;
	}

	return Filter;
}



