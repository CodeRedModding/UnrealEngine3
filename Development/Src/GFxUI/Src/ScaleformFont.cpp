/**********************************************************************

Filename    :   ScaleformFont.cpp
Content     :

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
#include "ScaleformFont.h"
#include "Render/RHI_HAL.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(push, 8)
#endif

#include "GFx/GFx_Loader.h"
#include "GFx/GFx_TextureFont.h"

#if SUPPORTS_PRAGMA_PACK
#pragma pack(pop)
#endif

FGFxUFontProvider::FGFxUFontProvider()
{
}
FGFxUFontProvider::~FGFxUFontProvider()
{
}

Render::Font* FGFxUFontProvider::CreateFont ( const char* name, unsigned fontFlags )
{
	if ( !GGFxEngine )
	{
		return NULL;
	}

	UFont* Font = LoadObject<UFont> ( NULL, *FString ( name ), NULL, LOAD_None, NULL );
	if ( !Font )
	{
		return NULL;
	}

	fontFlags &= Render::Font::FF_CreateFont_Mask;
	fontFlags |= Render::Font::FF_DeviceFont | Render::Font::FF_GlyphShapesStripped;

	const INT PageIndex = Font->GetResolutionPageIndex ( 800 );
	const FLOAT FontScale = Font->GetScalingFactor ( 800 );

	if ( Font->EmScale == 0 )
	{
		warnf ( NAME_Warning,
				TEXT ( "The font %s has an EmScale of 0. It will render as blanks; please reimport font." ),
				*FString ( name ) );
	}

	float XScale = Font->EmScale;
	float TexScale = 1536.f / XScale;

	Render::RHI::HAL* RenderHal = GGFxEngine->GetRenderHAL();
	GFx::TextureFont* OutFont = SF_NEW GFx::TextureFont ( name, fontFlags, Font->NumCharacters );
	OutFont->SetFontMetrics ( Font->Leading, Font->Ascent, Font->Descent );

	TArray<Ptr<GFx::ImageResource> > Textures;
	for ( INT Tex = 0; Tex < Font->Textures.Num(); Tex++ )
	{
		Render::Texture*          Texture = RenderHal->GetTextureManager()->CreateTexture ( Font->Textures ( Tex ) );
		Ptr<Render::TextureImage> TextureImage =
			*SF_NEW Render::TextureImage ( Texture->GetFormat(), Render::ImageSize ( Font->Textures ( Tex )->SizeX, Font->Textures ( Tex )->SizeY ),
										   Render::ImageUse_NoDataLoss , Texture );
		Textures.AddItem ( *SF_NEW GFx::ImageResource ( TextureImage ) );

		if ( Tex == 0 )
		{
			GFx::FontPackParams::TextureConfig TexParams;
			TexParams.NominalSize = appTrunc ( TexScale );
			TexParams.TextureWidth  = Font->Textures ( Tex )->SizeX;
			TexParams.TextureHeight = Font->Textures ( Tex )->SizeY;
			OutFont->SetTextureParams ( TexParams,
										Font->ImportOptions.bUseDistanceFieldAlpha ? Render::Font::FF_DistanceFieldAlpha : 0,
										Min<INT> ( Font->ImportOptions.XPadding, Font->ImportOptions.YPadding ) );
		}

		Texture->Release();
	}

	for ( INT GlyphIndex = 0; GlyphIndex < Font->Characters.Num(); GlyphIndex++ )
	{
		const FFontCharacter& Glyph = Font->Characters ( PageIndex + GlyphIndex );
		UTexture2D* Texture = Font->Textures ( Glyph.TextureIndex );
		GFx::TextureFont::AdvanceEntry Advance;
		Render::RectF Bounds
		(
			float ( Glyph.StartU ) / float ( Texture->OriginalSizeX ),
			float ( Glyph.StartV ) / float ( Texture->OriginalSizeY ),
			float ( Glyph.StartU + Glyph.USize ) / float ( Texture->OriginalSizeX ),
			float ( Glyph.StartV + Glyph.VSize ) / float ( Texture->OriginalSizeY )
		);
		Render::PointF Origin
		(
			Bounds.x1,
			Bounds.y1 - ( Glyph.VerticalOffset - Font->Ascent / XScale ) / float ( Texture->OriginalSizeY )
		);

		Advance.Advance = ( Glyph.USize + Font->Kerning ) * XScale;
		Advance.Left    = 0;
		Advance.Top     = appTrunc ( -Glyph.VerticalOffset * XScale + Font->Ascent );
		Advance.Width   = appTrunc ( Glyph.USize * XScale );
		Advance.Height  = appTrunc ( Glyph.VSize * XScale );

		OutFont->AddTextureGlyph ( GlyphIndex, Textures ( Glyph.TextureIndex ), Bounds, Origin, Advance );
	}

	if ( Font->IsRemapped )
	{
		for ( TMap<WORD, WORD>::TIterator It ( Font->CharRemap ); It; ++It )
		{
			OutFont->SetCharMap ( It.Key(), It.Value() );
		}
	}
	else
	{
		for ( INT Ch = 0; Ch < Font->Characters.Num(); Ch++ )
		{
			OutFont->SetCharMap ( Ch, Ch );
		}
	}

	return OutFont;
}

void FGFxUFontProvider::LoadFontNames ( StringHash<String>& fontnames )
{
}


#endif
