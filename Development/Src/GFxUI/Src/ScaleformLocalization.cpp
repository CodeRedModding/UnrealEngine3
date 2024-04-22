/**********************************************************************

Filename    :   ScaleformLocalization.cpp
Content     :   Localization for GFx

Copyright   :   (c) 2001-2007 Scaleform Corp. All Rights Reserved.

Portions of the integration code is from Epic Games as identified by Perforce annotations.
Copyright 2010 Epic Games, Inc. All rights reserved.

Notes       :

Licensees may use this file in accordance with the valid Scaleform
Commercial License Agreement provided with the software.

This file is provided AS IS with NO WARRANTY OF ANY KIND, INCLUDING
THE WARRANTY OF DESIGN, MERCHANTABILITY AND FITNESS FOR ANY PURPOSE.

**********************************************************************/

#include "GFxUI.h"

#if WITH_GFx

#include "ScaleformEngine.h"

#ifndef GFX_NO_LOCALIZATION

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "GFx/GFx_Loader.h"
#include "GFx/GFx_FontLib.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

using namespace GFx;

/* from UnMisc.cpp- update if Localize is changed */
UBOOL LocalizeArray ( TArray<FString>& Result, const TCHAR* Section, const TCHAR* Key, const TCHAR* Package = GPackage, const TCHAR* LangExt = NULL )
{
	Result.Empty();
	if ( !GIsStarted || !GConfig || !GSys )
	{
		return FALSE;
	}
	// The default behavior for Localize is to use the configured language which is indicated by passing in NULL.
	if ( LangExt == NULL )
	{
		LangExt = UObject::GetLanguage();
	}

	// We allow various .inis to contribute multiple paths for localization files.
	for ( INT PathIndex = 0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++ )
	{
		// Try specified language first
		FFilename FilenameLang	= FString::Printf ( TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s.%s" ),
		                          *GSys->LocalizationPaths ( PathIndex ), LangExt, Package, LangExt );
		if ( GConfig->GetArray ( Section, Key, Result, *FilenameLang ) )
		{
			return Result.Num() ? TRUE : FALSE;
		}
	}

	if ( appStricmp ( LangExt, TEXT ( "INT" ) ) )
	{
		// if we haven't found it yet, fall back to default (int) and see if it exists there.
		for ( INT PathIndex = 0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++ )
		{
			FFilename FilenameInt = FString::Printf ( TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s.%s" ),
			                        *GSys->LocalizationPaths ( PathIndex ), TEXT ( "INT" ), Package, TEXT ( "INT" ) );
			if ( GConfig->GetArray ( Section, Key, Result, *FilenameInt ) )
			{
				return Result.Num() ? TRUE : FALSE;
			}
		}
	}
	return FALSE;
}

/* from UnMisc.cpp- update if Localize is changed */
static UBOOL LocalizeSection ( TArray<FString>& Result, const TCHAR* Section, const TCHAR* Package = GPackage, const TCHAR* LangExt = NULL )
{
	Result.Empty();
	if ( !GIsStarted || !GConfig || !GSys )
	{
		return FALSE;
	}
	// The default behavior for Localize is to use the configured language which is indicated by passing in NULL.
	if ( LangExt == NULL )
	{
		LangExt = UObject::GetLanguage();
	}

	// We allow various .inis to contribute multiple paths for localization files.
	for ( INT PathIndex = 0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++ )
	{
		// Try specified language first
		FFilename FilenameLang = FString::Printf ( TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s.%s" ),
		                         *GSys->LocalizationPaths ( PathIndex ), LangExt, Package, LangExt );
		if ( GConfig->GetSection ( Section, Result, *FilenameLang ) )
		{
			return Result.Num() ? TRUE : FALSE;
		}
	}

	if ( appStricmp ( LangExt, TEXT ( "INT" ) ) )
	{
		// if we haven't found it yet, fall back to default (int) and see if it exists there.
		for ( INT PathIndex = 0; PathIndex < GSys->LocalizationPaths.Num(); PathIndex++ )
		{
			FFilename FilenameInt = FString::Printf ( TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s" ) PATH_SEPARATOR TEXT ( "%s.%s" ),
			                        *GSys->LocalizationPaths ( PathIndex ), TEXT ( "INT" ), Package, TEXT ( "INT" ) );
			if ( GConfig->GetSection ( Section, Result, *FilenameInt ) )
			{
				return Result.Num() ? TRUE : FALSE;
			}
		}
	}
	return FALSE;
}

class ScaleformTranslator : public Translator
{
	public:
		virtual unsigned GetCaps() const
		{
			return Cap_StripTrailingNewLines;
		}

		virtual void Translate ( TranslateInfo* ptranslateInfo )
		{
			// Translated strings start with $$sign
#if TCHAR_IS_1_BYTE
			const wchar_t* pkeyW = ptranslateInfo->GetKey();
			int len = 0;
			while (pkeyW[len])
				len++;
			char* pkey = (char*) alloca ( len + 1 );
			for (int i = 0; i < len; i++ )
				pkey[i] = (char)pkeyW[i];
			pkey[len] = 0;
			StringBuffer buffer;
#else
			const wchar_t* pkey = ptranslateInfo->GetKey();
			WStringBuffer buffer;
#endif
			if ( pkey[0] == '$' )
			{
				FString label ( pkey + 1 );
				TArray<FString> splitlabel;
				switch ( label.ParseIntoArray ( &splitlabel, TEXT ( "." ), 1 ) )
				{
					case 0:
						return;
					case 1:
						buffer = *Localize ( TEXT ( "Global" ), pkey + 1, TEXT ( "GFxUI" ) );
						break;
					case 2:
						buffer = *Localize ( *splitlabel ( 0 ), *splitlabel ( 1 ), TEXT ( "GFxUI" ) );
						break;
					default:
						buffer = *Localize ( *splitlabel ( 1 ), *splitlabel ( 2 ), *splitlabel ( 0 ) );
						break;
				}
#if TCHAR_IS_1_BYTE
				ptranslateInfo->SetResult ( buffer.ToCStr() );
#else
				ptranslateInfo->SetResult ( buffer.ToWStr() );
#endif
			}
			else
			{
				buffer = *Localize ( TEXT ( "Global" ), pkey, TEXT ( "GFxUI" ), NULL, TRUE );
				if ( buffer.GetLength() )
				{
#if TCHAR_IS_1_BYTE
					ptranslateInfo->SetResult ( buffer.ToCStr() );
#else
					ptranslateInfo->SetResult ( buffer.ToWStr() );
#endif
				}
			}
		}
};

static FontMap::MapFontFlags UpdateFontFlags ( FontMap::MapFontFlags flags, const FString& symbol )
{
	FontMap::MapFontFlags newFlags = FontMap::MFF_Original;

	if ( !appStricmp ( *symbol, TEXT ( "bold" ) ) )
	{
		newFlags = FontMap::MFF_Bold;
	}
	else if ( !appStricmp ( *symbol, TEXT ( "normal" ) ) )
	{
		newFlags = FontMap::MFF_Normal;
	}
	else if ( !appStricmp ( *symbol, TEXT ( "italic" ) ) )
	{
		newFlags = FontMap::MFF_Italic;
	}
	else
	{
		debugf ( TEXT ( "GFx Warning: FontConfig - unknown map font style '%s'" ), *symbol );
		return flags;
	}

	if ( flags == FontMap::MFF_Original )
	{
		flags = newFlags;
	}
	else if ( ( ( flags == FontMap::MFF_Normal ) && ( ( newFlags & FontMap::MFF_BoldItalic ) != 0 ) ) ||
	          ( ( newFlags == FontMap::MFF_Normal ) && ( ( flags & FontMap::MFF_BoldItalic ) != 0 ) ) )
	{
		// Normal combined with incorrect flag.
		debugf ( TEXT ( "GFx Warning: FontMap - unexpected map font style '%s'" ), *symbol );
	}
	else
	{
		flags = ( FontMap::MapFontFlags ) ( newFlags | flags );
	}

	return flags;
}

void FGFxEngine::InitLocalization()
{
	FontlibInitialized = FALSE;
	mLoader.SetFontLib ( 0 );

	TArray<FString> FontMapText;
	if ( LocalizeSection ( FontMapText, TEXT ( "Fonts" ), TEXT ( "GFxUI" ) ) )
	{
		pFontMap = *SF_NEW FontMap;
		for ( INT line = 0; line < FontMapText.Num(); line++ )
		{
			FString label, ffont;
			for ( INT pos = 0; pos < FontMapText ( line ).Len(); pos++ )
			{
				if ( FontMapText ( line ) [pos] == TEXT ( '=' ) )
				{
					label = FString ( TEXT ( "$" ) ) + FontMapText ( line ).Mid ( 0, pos );
					TArray<FString> font;
					if ( FontMapText ( line ).Mid ( pos + 1 ).ParseIntoArray ( &font, TEXT ( "," ), 1 ) )
					{
						FontMap::MapFontFlags flags = FontMap::MFF_Original;
						for ( INT i = 1; i < font.Num(); i++ )
						{
							flags = UpdateFontFlags ( flags, font ( i ) );
						}
						const FString& FontResource = font ( 0 );
						pFontMap->MapFont ( *label, *FontResource, flags );
					}
					else
					{
						const FString& FontResource = FontMapText ( line ).Mid ( pos + 1 );
						pFontMap->MapFont ( *label, *FontResource, FontMap::MFF_Original );
					}
					break;
				}
			}
		}
		mLoader.SetFontMap ( pFontMap );
	}

	FString enabletr = Localize ( TEXT ( "Translation" ), TEXT ( "Enable" ), TEXT ( "GFxUI" ) );
	if ( enabletr.Len() > 0 && enabletr[0] == L'1' )
	{
		pTranslator = *SF_NEW ScaleformTranslator;
		mLoader.SetTranslator ( pTranslator );
	}
}

void FGFxEngine::InitFontlib()
{
	if ( FontlibInitialized )
	{
		return;
	}

	TArray<FString> FontLibFiles;
	if ( LocalizeArray ( FontLibFiles, TEXT ( "FontLib" ), TEXT ( "FontLib" ), TEXT ( "GFxUI" ) ) )
	{
		MovieInfo Info;
		pFontLib = *SF_NEW FontLib;
		mLoader.SetFontLib ( pFontLib );
		for ( INT i = 0; i < FontLibFiles.Num(); i++ )
		{
			Ptr<GFx::MovieDef> pdef = *LoadMovieDef ( *FontLibFiles ( i ), Info );
			if ( pdef )
			{
				pFontLib->AddFontsFrom ( pdef, 0 );
			}
		}
	}
	FontlibInitialized = TRUE;
}

#endif // GFX_NO_LOCALIZATION

#endif // WITH_GFx

