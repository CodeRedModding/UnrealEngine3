/*=============================================================================
	UnFont.cpp: Unreal font code.
	Copyright 1998-2013 Epic Games, Inc. All Rights Reserved.
=============================================================================*/

#include "EnginePrivate.h"
#include "EngineUserInterfaceClasses.h"

IMPLEMENT_CLASS(UFont);
IMPLEMENT_CLASS(UFontImportOptions);
IMPLEMENT_CLASS(UMultiFont);


/**
* Serialize the object struct with the given archive
*
* @param Ar - archive to serialize with
*/
void UFont::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if ( Ar.Ver() < VER_FIXED_FONTS_SERIALIZATION && Ar.IsLoading() )
	{
		Ar << Characters << Textures << Kerning;
	}

	Ar << CharRemap;

	if ( Ar.Ver() < VER_FIXED_FONTS_SERIALIZATION && Ar.IsLoading() )
	{
		Ar << IsRemapped;
	}
}

/**
* Called after object and all its dependencies have been serialized.
*/
void UFont::PostLoad()
{
	Super::PostLoad();


	// Cache the character count and the maximum character height for this font
	CacheCharacterCountAndMaxCharHeight();


	for( INT TextureIndex = 0 ; TextureIndex < Textures.Num() ; ++TextureIndex )
	{
		UTexture2D* Texture = Textures(TextureIndex);
		if( Texture )
		{	
			Texture->SetFlags(RF_Public);
			Texture->LODGroup = TEXTUREGROUP_UI;	
		}
	}
}



/**
 * Caches the character count and maximum character height for this font (as well as sub-fonts, in the multi-font case)
 */
void UFont::CacheCharacterCountAndMaxCharHeight()
{
	// Cache the number of characters in the font.  Obviously this is pretty simple, but note that it will be
	// computed differently for MultiFonts.  We need to cache it so that we have it available in inline functions
	NumCharacters = Characters.Num();

	// Cache maximum character height
	MaxCharHeight.Reset();
	INT MaxCharHeightForThisFont = 1;
	for( INT CurCharNum = 0; CurCharNum < NumCharacters; ++CurCharNum )
	{
		MaxCharHeightForThisFont = Max( MaxCharHeightForThisFont, Characters( CurCharNum ).VSize );
	}

	// Add to the array
	MaxCharHeight.AddItem( MaxCharHeightForThisFont );
}


UBOOL UFont::IsLocalizedResource()
{
	//@todo: maybe this should be a flag?
	return TRUE;
}

/**
 * Calulate the index for the texture page containing the multi-font character set to use, based on the specified screen resolution.
 *
 * @param	HeightTest	the height (in pixels) of the viewport being rendered to.
 *
 * @return	the index of the multi-font "subfont" that most closely matches the specified resolution.  this value is used
 *			as the value for "ResolutionPageIndex" when calling other font-related methods.
 */
INT UFont::GetResolutionPageIndex(FLOAT HeightTest) const
{
	return 0;
}

/**
 * Calculate the amount of scaling necessary to match the multi-font subfont which most closely matches the specified resolution.
 *
 * @param	HeightTest	the height (in pixels) of the viewport being rendered to.
 *
 * @return	the percentage scale required to match the size of the multi-font's closest matching subfont.
 */
FLOAT UFont::GetScalingFactor(FLOAT HeightTest) const
{
	return ScalingFactor;
}

/**
 * Determine the height of the mutli-font resolution page which will be used for the specified resolution.
 *
 * @param	ViewportHeight	the height (in pixels) of the viewport being rendered to.
 */
FLOAT UFont::GetAuthoredViewportHeight( FLOAT ViewportHeight ) const
{
	return UCONST_DEFAULT_SIZE_Y;
}

/**
 * Returns the maximum height for any character in this font
 */
FLOAT UFont::GetMaxCharHeight() const
{
	// @todo: Provide a version of this function that supports multi-fonts properly.  It should take a
	//    HeightTest parameter and report the appropriate multi-font MaxCharHeight value back.
	INT MaxCharHeightForAllMultiFonts = 1;
	for( INT CurMultiFontIndex = 0; CurMultiFontIndex < MaxCharHeight.Num(); ++CurMultiFontIndex )
	{
		MaxCharHeightForAllMultiFonts = Max( MaxCharHeightForAllMultiFonts, MaxCharHeight( CurMultiFontIndex ) );
	}
	return MaxCharHeightForAllMultiFonts;
}


/**
 * Determines the height and width for the passed in string.
 */
void UFont::GetStringHeightAndWidth( const FString& InString, INT& Height, INT& Width ) const
{
	Height = GetStringHeightSize( *InString );
	Width = GetStringSize( *InString );
}


/**
 * Returns the size of the object/ resource for display to artists/ LDs in the Editor.
 *
 * @return		Size of resource as to be displayed to artists/ LDs in the Editor.
 */
INT UFont::GetResourceSize()
{
	if (GExclusiveResourceSizeMode)
	{
		return 0;
	}
	else
	{
		FArchiveCountMem CountBytesSize( this );
		INT ResourceSize = CountBytesSize.GetNum();
		for( INT TextureIndex = 0 ; TextureIndex < Textures.Num() ; ++TextureIndex )
		{
			if ( Textures(TextureIndex) )
			{
				ResourceSize += Textures(TextureIndex)->GetResourceSize();
			}
		}
		return ResourceSize;
	}
}

/**
 * Serialize the ResolutionTestTable as well
 */
void UMultiFont::Serialize( FArchive& Ar )
{
	Super::Serialize( Ar );

	if ( Ar.Ver() < VER_FIXED_FONTS_SERIALIZATION && Ar.IsLoading() )
	{
		Ar << ResolutionTestTable;
	}
}


/**
* Called after object and all its dependencies have been serialized.
*/
void UMultiFont::PostLoad()
{
	// Call our parent implementation
	Super::PostLoad();

	// Also, we'll need to cache the character count and maximum character height for each multi-font
	CacheCharacterCountAndMaxCharHeight();
}



/**
 * Caches the character count and maximum character height for this font (as well as sub-fonts, in the multi-font case)
 */
void UMultiFont::CacheCharacterCountAndMaxCharHeight()
{
	// Cache character count for multi-font
	NumCharacters = Characters.Num() / ResolutionTestTable.Num();

	// Cache maximum character height for each multi-font
	MaxCharHeight.Reset();
	INT BaseCharIndex = 0;
	for( INT CurMultiFontIndex = 0; CurMultiFontIndex < ResolutionTestTable.Num(); ++CurMultiFontIndex )
	{
		INT MaxCharHeightForThisFont = 1;
		for( INT CurCharNum = 0; CurCharNum < NumCharacters; ++CurCharNum )
		{
			const INT CurCharIndex = BaseCharIndex + CurCharNum;
			MaxCharHeightForThisFont = Max( MaxCharHeightForThisFont, Characters( CurCharIndex ).VSize );
		}

		// Add to the array
		MaxCharHeight.AddItem( MaxCharHeightForThisFont );

		// On to the next multi-font!
		BaseCharIndex += NumCharacters;
	}
}


/**
 * Find the best fit in the resolution table.
 *
 * @returns the index that best fits the HeightTest
 */
INT UMultiFont::GetResolutionTestTableIndex(FLOAT HeightTest) const
{
	int RTTIndex = 0;
	for (INT i=1;i< ResolutionTestTable.Num(); i++)
	{
		if (Abs( ResolutionTestTable(i) - HeightTest) < Abs( ResolutionTestTable(RTTIndex) - HeightTest) )
		{
			RTTIndex = i;
		}
	}
	return RTTIndex;
}

/**
 * Calculate the starting index in to the character array for a given resolution
 *
 * @returns the index
 */
INT UMultiFont::GetResolutionPageIndex(FLOAT HeightTest) const
{
	INT RTTIndex = GetResolutionTestTableIndex(HeightTest);
	if ( RTTIndex < ResolutionTestTable.Num() )
	{
		return RTTIndex * NumCharacters;
	}

	return Super::GetResolutionPageIndex(HeightTest);
}

/**
 * Determine the height of the mutli-font resolution page which will be used for the specified resolution.
 *
 * @param	ViewportHeight	the height (in pixels) of the viewport being rendered to.
 */
FLOAT UMultiFont::GetAuthoredViewportHeight( FLOAT ViewportHeight ) const
{
	INT RTTIndex = GetResolutionTestTableIndex(ViewportHeight);
	if (RTTIndex < ResolutionTestTable.Num() )
	{
		return ResolutionTestTable(RTTIndex);
	}

	return Super::GetAuthoredViewportHeight(ViewportHeight);
}

/**
 * Calculate the scaling factor for between resolutions
 *
 * @returns the scaling factor
 */
FLOAT UMultiFont::GetScalingFactor(FLOAT HeightTest) const
{
	INT RTTIndex = GetResolutionTestTableIndex(HeightTest);
	if (RTTIndex < ResolutionTestTable.Num() )
	{
		return ((HeightTest / ResolutionTestTable(RTTIndex)) * ScalingFactor);
	}

	return Super::GetScalingFactor(HeightTest);
}
